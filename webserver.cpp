#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>
#include <string>
#include <fstream>
#include <sstream>

#include "sockets.cpp"

// config
#define LOCALHOST "127.0.0.1"
#define PORT "10980"
#define CONNS_AMOUNT 128
#define PACKET_SIZE 1024
#define DEFAULT_HTTP_SIZE 1024

// get printable IP address (IPv4 or IPv6) as std::string
std::string get_printable_ip(struct sockaddr *sa)
{
    char ipstr[INET6_ADDRSTRLEN];
    void *addr;
    if (sa->sa_family == AF_INET) 
    {
        addr = &(((struct sockaddr_in*)sa)->sin_addr);
    }
    else
    {
        addr = &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
    inet_ntop(sa->sa_family, addr, ipstr, sizeof(ipstr));
    return std::string(ipstr);
}

in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
    {
        return (((struct sockaddr_in*)sa)->sin_port);
    }
    return (((struct sockaddr_in6*)sa)->sin6_port);
}

std::map<std::string, std::string> LoadRoutingMap(const std::string& filepath)
{
    std::map<std::string, std::string> routeMap;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Can't open file: " << filepath << "\n";
        return routeMap;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string route, filePath;

        if (!(iss >> route >> filePath)) {
            std::cerr << "Incorrect stroke: " << line << "\n";
            continue;
        }

        routeMap[route] = filePath;
    }

    return routeMap;
}


// rewrite and optimize
char* LoadFileToBuffer(const std::string& filepath, size_t& outSize)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << "\n";
        outSize = 0;
        return nullptr;
    }

    std::streamsize size = file.tellg(); 
    file.seekg(0, std::ios::beg);          

    char* buffer = new char[size + 1]; 
    if (!file.read(buffer, size)) {
        std::cerr << "Ошибка при чтении файла: " << filepath << "\n";
        delete[] buffer;
        outSize = 0;
        return nullptr;
    }

    buffer[size] = '\0';
    outSize = static_cast<size_t>(size);
    return buffer;
}

// args: port, connectionsAtMomentLimit
int main (int argc, char *argv[]) 
{
    //index all routes
    std::map<std::string, std::string> routes = LoadRoutingMap("routes.txt");

    // Listener socket config
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE; // use my local IP (OS localloop) 

    // Create Listener socket
    SingleListener *listener = SingleListener::CreateInstance(LOCALHOST, PORT, hints);
    listener->StartListen(CONNS_AMOUNT);

    // Configurate event loop
    int listener_fd = listener->GetListenerDescriptor();
    int fdmax = listener_fd;
    fd_set master, read_fds, write_fds, except_fds; //set for all descriptors
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    // add the listener to the master set
    FD_SET(listener_fd, &master);

    // clients
    int newfd; //newly accepted socket descriptor
    struct sockaddr_storage remoteaddr; //client address
    socklen_t addrlen;
    int nbytes;
    std::map<int, ClientSocket*> client_sockets = {};

    //char request_buf[DEFAULT_HTTP_SIZE]; //buffer for client data;

    // I/O event loop
    int read_write_event_status;
    while(true) 
    {
        read_fds = master; 
        read_write_event_status = select(fdmax+1, &read_fds, &write_fds, &except_fds, NULL);
        if(read_write_event_status == -1) 
        {
            perror("select");
            return EXIT_FAILURE;
        }

        for(int i = 0; i <= fdmax; i++) 
        {
            // New TCP connection!
            if (i == listener_fd && FD_ISSET(i, &read_fds)) 
            {
                addrlen = sizeof(remoteaddr);
                newfd = accept(listener_fd,
                    (struct sockaddr *)&remoteaddr,
                    &addrlen);
                if (newfd == -1) 
                {
                    perror("accept error");
                }
                FD_SET(newfd, &master);
                if (newfd > fdmax)
                {
                    fdmax = newfd;
                }
                client_sockets[newfd] = new ClientSocket(newfd, get_printable_ip((struct sockaddr*)&remoteaddr));
                std::cout << "new connection from " << get_printable_ip((struct sockaddr*)&remoteaddr);
                std::cout << " from port: " << std::dec << 
                ntohs(get_in_port((struct sockaddr*)&remoteaddr)) << " ";
                std::cout << "with fd: " << newfd << "\n";
            }

            // Connected client send data
            else if (i != listener_fd && FD_ISSET(i, &read_fds))
            {
                nbytes = (*client_sockets[i]).receiveall(); 

                if (nbytes <= 0)
                {
                    // client closed connection
                    if (nbytes == 0) 
                    {
                        std::cout << "close connection: " << i << "\n";
                    }
                    else 
                    {
                        perror("recv");
                    }
                    close(i);
                    FD_CLR(i, &master);
                    FD_CLR(i, &write_fds);
                }    
                if((*client_sockets[i]).isHaveRequests())
                {
                    FD_SET(i, &write_fds);
                }
            }
            // Connected client ready to recieve data
            else if (i != listener_fd && FD_ISSET(i, &write_fds) && (*client_sockets[i]).isHaveRequests())
            {
                std::cout << "\e[0;34m";
                std::cout << "socket fd: " << (*client_sockets[i]).getSocketDescriptorNum() << " "
                << "with ip: " << (*client_sockets[i]).getSocketIP() << " "
                << "is ready to recieve html" << "\n";
                // clear stream from decorators
                std::cout << "\033[0m" << "\n";

                
                std::cout << "\e[0;34m";
                std::cout << "Request processing for ip: " << 
                (*client_sockets[i]).getSocketIP() << 
                " is started:" << "\n";
                std::cout << "\033[0m" << "\n";
                char* req = new char[DEFAUL_HTTP_REQUEST_LENGHT];
                if((*client_sockets[i]).ExtractHeadRequest(req))
                {
                    std::cout << req << "\n";

                    //request satisfaction
                    int j = 0, k = 0;
                    char httpMethod[6+1] = {};
                    char route[256] = {};
                    while(req[j] != 0x20)
                    {
                        httpMethod[j] = req[j];
                        j += 1;
                    }
                    httpMethod[++j] = '\0';
                    while(req[j] != 0x20)
                    {
                        route[k] = req[j];
                        j += 1;
                        k += 1;
                    }
                    route[++k] = '\0';
                    if(strcmp(httpMethod, "GET") == 0)
                    {
                        size_t len;
                        char* filecontent = LoadFileToBuffer("pages/" + routes[route], len);
                        int tmp = len;
                        //implement multithreading
                        //(*client_sockets[i]).sendall_threaded(filecontent, (int)len);
                        (*client_sockets[i]).sendallHTML(i, filecontent, &tmp);
                    }
                }
                
                if(!(*client_sockets[i]).isHaveRequests())
                {
                    FD_CLR(i, &write_fds);
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
