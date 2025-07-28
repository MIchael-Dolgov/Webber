#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <set>
#include <thread>
#include <mutex>

#define DEFAUL_HTTP_REQUEST_LENGHT 1024

class SingleListener 
{
    public:
        // Singleton
        static SingleListener *CreateInstance(const char *host, const char *port, addrinfo hints)
        {
            if (pinstance_ == nullptr)
            {
                pinstance_ = new SingleListener(host, port, hints);
            }
            return pinstance_;
        }
        static SingleListener *GetInstance()
        {
            if (pinstance_ == nullptr)
            {
                throw std::runtime_error("Something went wrong");
            }
            return pinstance_;
        }
        static int GetListenerDescriptor() 
        {
            return listener_fd_;
        }
                void StartListen(uint conns_amount) {
            if (listen(listener_fd_, conns_amount) == -1) 
            {
                perror("server: listen");
                close(listener_fd_);
                exit(EXIT_FAILURE);
            }

            // success
            std::cout << "\033[1;32m";
            std::cout << "===Success!===" << "\n";
            std::cout << "Web server is listening port: " << listener_port << " "
                << "On ip: " << listener_host << "\n";
            std::cout << "\033[0m";
        }

    private:
        static SingleListener *pinstance_;
        static int listener_fd_; //listen on listener_fd_
        static char *listener_host, *listener_port;

        SingleListener(const char *host, const char *port, addrinfo hints) 
        {
            listener_host = strdup(host);
            listener_port = strdup(port);

            // Listener init
            struct addrinfo *listener_res, *p;


            int sockStatus = getaddrinfo(host, port, &hints, &listener_res);
            int yes = 1;
            if (sockStatus != 0) 
            {
                std::cerr << "getaddrinfo error: " << gai_strerror(sockStatus) << "\n";
                exit(EXIT_FAILURE);
            }

            // Try to create listener server socket on TCP/IP
	        for(p = listener_res; p != NULL; p = p->ai_next) 
            {
		        if ((listener_fd_ = socket(p->ai_family, p->ai_socktype,
				        p->ai_protocol)) == -1) 
                {
                    perror("server: socket");
			        continue;
		        }

		        if (setsockopt(listener_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) 
                {
                    perror("server: setsockopt");
                    exit(EXIT_FAILURE);
		        }

		        if (bind(listener_fd_, p->ai_addr, p->ai_addrlen) == -1) 
                {
			        close(listener_fd_);
                    perror("server: bind");
			        continue;
		        }

		        break; //success
	        }
            freeaddrinfo(listener_res); // all done with this structure

            if (p == NULL) 
            {
                std::cerr << "server: failed to bind \n";
                exit(EXIT_FAILURE);
            }           
        }
        ~SingleListener()
        {
            close(listener_fd_);
            free(listener_host);
            free(listener_port);
        }
        SingleListener(const SingleListener&) = delete;
        SingleListener& operator=(const SingleListener&) = delete;
};

SingleListener* SingleListener::pinstance_ = nullptr;
int SingleListener::listener_fd_ = -1;
char* SingleListener::listener_host = nullptr;
char* SingleListener::listener_port = nullptr;

// Only working with HTTP header requests
class ClientSocket 
{
    public:
        ClientSocket(int sockfd, std::string ipstr): descriptor_num(sockfd), ipstr(ipstr)
        {
        }
        ~ClientSocket()
        {
            delete buffer;
        }
        ssize_t receiveall()
        {
            //return recv(descriptor_num, (*buffer).buffer, sizeof((*buffer).buffer), 0);
            char tmpchar[DEFAUL_HTTP_REQUEST_LENGHT];
            ssize_t res = recv(descriptor_num, tmpchar, sizeof(tmpchar), 0);
            if (res > 0) 
            {
                res = (*buffer).tryAddRequest(tmpchar, res);
            }
            return res;
        }

        //TODO: продумать логику с мультеплесированием и асинхронным уведовлением результата отправки, пока вслепую
        void sendall_threaded(char* buf, int len) 
        {
            socks_thread_pool.insert(descriptor_num);
            std::lock_guard<std::mutex> lock(pool_mutex);
            if (socks_thread_pool.count(descriptor_num)) 
            {
                std::cerr << "socket is already in use\n";
                return;
            }
            

            std::thread([this, buf, len]() 
            {
                int sent = len;
                int res = sendall(descriptor_num, buf, &sent);
                if (res == -1) 
                {
                    std::cerr << "sendall failed in thread: " << strerror(errno) << "\n";
                }
                {
                    std::lock_guard<std::mutex> lock(pool_mutex);
                    socks_thread_pool.erase(descriptor_num);
                }
            }).detach();
        }

        static int sendall(int sockfd, char *buf, int *len)
        {
            int total = 0;
            int bytesleft = *len;
            int n;
            while(total < *len)
            {
                n = send(sockfd, buf+total, bytesleft, 0);
                if (n==-1) {break;}
                total += n;
                bytesleft -= n;
            }
            *len = total;
            return n == -1 ? -1 : 0;
        }

        static int sendallHTML(int sockfd, char *buf, int *len)
        {
            std::string body(buf, *len);  // тело
            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "\r\n" +
                body;

            const char* full_response = response.c_str();
            int total = 0;
            int bytesleft = response.length();
            int n;

            while (total < bytesleft)
            {
                n = send(sockfd, full_response + total, bytesleft - total, 0);
                if (n == -1) break;
                total += n;
            }

            *len = total;
            return n == -1 ? -1 : 0;
        }

        int getSocketDescriptorNum(void)
        {
            return descriptor_num;
        }
        
        std::string getSocketIP(void)
        {
            return ipstr;
        }

        bool ExtractHeadRequest(char req[])
        {
            if(buffer->size() == 0)
            {
                return false;
            }

            bool res = buffer->tryExtractFirstRequest(req);
            if(res)
            {
                std::cout << "Request has been readed" << "\n";
            }
            else 
            {
                std::cerr << "Failed to read request. Request dumped." 
                << "\n";
                return false;
            }

            return true;
        }

        bool isHaveRequests(void)
        {
            return buffer->size() != 0;
        }
        
    private:
        const std::string ipstr;
        const int descriptor_num; 
        static std::set<int> socks_thread_pool;
        static std::mutex pool_mutex;

        class RingedRequestsBuffer
        {
            public: 
                std::string printableIP = "";
                const static constexpr size_t request_buffer_chars = 1024;
                const static constexpr size_t total_size = request_buffer_chars * 10;
                char buffer[request_buffer_chars];
                RingedRequestsBuffer() 
                {
                    write_pos = 0;
                    read_pos = 0;
                    used = 0;
                }

                ~RingedRequestsBuffer() 
                {
                }

                const size_t available_space()
                {
                    return total_size - used;
                }

                const size_t size()
                {
                    return used;
                }

                ssize_t tryAddRequest(const char *str, size_t len)
                {
                    if (len == 0)
                    {
                        return 0;
                    }
                    else if (len > available_space()) 
                    {
                        throw std::runtime_error("RingedBuffer overflow: not enough space to write");
                        return -1;
                    }
                    else if(write_pos == read_pos && used != 0) 
                    {
                        std::cerr << "Something went wrong. Buffer dumped";
                        write_pos = 0;
                        read_pos = 0;
                        used = 0;
                        return -1;
                    }
                    for (int i = 0; i < len; i++)
                    {
                        buffer[write_pos] = str[i];
                        write_pos = (write_pos+1)%request_buffer_chars;
                    }
                    used += len;
                    return len;
                }

                bool tryExtractFirstRequest(char toCopy[])
                {
                    bool extracted = false;
                    int i = 0;
                    int rcnt = 0, ncnt = 0;
                    char tmp;
                    while(!extracted && used > 0)
                    {
                        used -= 1;
                        tmp = buffer[read_pos];
                        toCopy[i] = tmp;
                        buffer[read_pos++] = '\000';
                        if(tmp == '\r' || tmp == '\n')    
                        {
                            if(tmp == '\r' && (ncnt==0 || ncnt == 1))
                            {
                                rcnt += 1;
                            }
                            else if(tmp == '\n' && (rcnt==1 || rcnt == 2))
                            {
                                ncnt += 1;
                            }
                            
                        }
                        else
                        {
                            rcnt = 0;
                            ncnt = 0;
                        }
                        if(rcnt==2 && ncnt ==2)
                        {
                            extracted = true;
                        }
                        i += 1;
                    }
                    return extracted;
                }

            private:
                int write_pos, read_pos, used;
        };


        RingedRequestsBuffer *buffer = new RingedRequestsBuffer();
};
std::mutex ClientSocket::pool_mutex;
std::set<int> ClientSocket::socks_thread_pool;
