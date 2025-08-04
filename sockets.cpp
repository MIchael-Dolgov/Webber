#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <exception>
#include <netdb.h>
#include <cstring>
#include <memory>

#define HTTP_MAX_SOCKET_REQUESTS_CAPACITY 10u
#define DEFAULT_HTTP_REQUEST_SIZE 512u
#define FAILED -1

namespace sockets
{
    // identify IP type
    bool is_IPv6(struct sockaddr *sa)
    {
        if (sa->sa_family == AF_INET6)
            return true;
        return false;
    }

    // get printable IP address (IPv4 or IPv6) as std::string
    std::string get_printable_ip(struct sockaddr *sa) noexcept(true)
    {
        char ipstr[INET6_ADDRSTRLEN];
        void *addr;
        if (!is_IPv6(sa))
        {
            addr = &(((struct sockaddr_in *)sa)->sin_addr);
        }
        else
        {
            addr = &(((struct sockaddr_in6 *)sa)->sin6_addr);
        }
        inet_ntop(sa->sa_family, addr, ipstr, sizeof(ipstr));
        return std::string(ipstr);
    }

    // get printable Port
    in_port_t get_in_port(struct sockaddr *sa) noexcept(true)
    {
        if (!is_IPv6(sa))
        {
            return (((struct sockaddr_in *)sa)->sin_port);
        }
        return (((struct sockaddr_in6 *)sa)->sin6_port);
    }

    /**
     * static class
     * Listen socket. Program handling only one opened listening socket for
     * incoming requests
     */
    class SocketListener
    {
    private:
        SocketListener() = default;
        SocketListener(const SocketListener &) = delete;
        SocketListener &operator=(const SocketListener &) = delete;
        ~SocketListener() = default;

        // config
        static struct addrinfo hints;
        static bool configured_;
        static bool opened_;
        static int listener_fd_;
        static uint conns_amount_;
        static std::string listener_host_, listener_port_;

        void initialize()
        {
            if (configured_ != true)
            {
                throw std::runtime_error("socket not configured");
            }
            // Listener init
            struct addrinfo *listener_res, *p;
            int sockStatus = getaddrinfo(listener_host_.c_str(),
                listener_port_.c_str(), &hints, &listener_res);
            int yes = 1;
            if (sockStatus != 0)
            {
                std::cerr << "getaddrinfo error: "
                          << gai_strerror(sockStatus) << "\n";
                throw std::runtime_error("initialization failed");
            }

            // Try to create listener server socket on TCP/IP
            for (p = listener_res; p != NULL; p = p->ai_next)
            {
                if ((listener_fd_ = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == FAILED)
                {
                    std::cerr << "server: failed to create new socket, "
                        << "trying next option\n";
                    continue;
                }

                if (setsockopt(listener_fd_, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(yes)) == FAILED)
                {
                    close(listener_fd_);
                    std::cerr << "server: failed to set socket options, "
                        << "trying next option\n";
                    continue;
                }

                if (bind(listener_fd_, p->ai_addr, p->ai_addrlen) == FAILED)
                {
                    close(listener_fd_);
                    std::cerr << "server: failed to bind new socket,"
                        << " trying next option\n";
                    continue;
                }

                break; // success
            }
            freeaddrinfo(listener_res); // all done with this structure

            if (p == NULL)
            {
                std::cerr << "server: failed to bind \n";
                throw std::runtime_error("listener socket has not been created");
            }
        }

    public:
        static SocketListener &instance() noexcept(true)
        {
            static SocketListener single_instance;
            return single_instance;
        }

        static int GetListenerDescriptor() noexcept(true)
        {
            if (configured_)
                return listener_fd_;
            return FAILED;
        }

        void configure(uint conns, const std::string &host, const std::string &port,
            int ai_family = AF_UNSPEC, int ai_socktype = SOCK_STREAM,
            int ai_protocol = 0, int ai_flags = AI_PASSIVE)
        {
            if (configured_)
            {
                throw std::runtime_error("SocketListener already configured");
            }

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = ai_family;
            hints.ai_socktype = ai_socktype;
            hints.ai_protocol = ai_protocol;
            hints.ai_flags = ai_flags;
            conns_amount_ = conns;
            listener_host_ = host;
            listener_port_ = port;
            // conf complete
            configured_ = true;
        }

        // request to open list sock from Linux Kernel
        void startListen()
        {
            initialize();
            if (listen(listener_fd_, conns_amount_) == FAILED)
            {
                std::cerr << "server: failed to open listening socket" << "\n";
                close(listener_fd_);
                throw std::runtime_error("failed to open");
            }

            // success
            std::cout << "\033[1;32m";
            std::cout << "=====   Success!   =====" << "\n";
            std::cout << "Web server is listening port: " 
                << listener_port_ << " " << "On ip: " << listener_host_ << "\n";
            std::cout << "========================" << "\n";
            std::cout << "\033[0m";
        }
    };
    // basic non initialized values for SocketListener
    struct addrinfo sockets::SocketListener::hints = {};
    bool sockets::SocketListener::configured_ = false;
    bool sockets::SocketListener::opened_ = false;
    int sockets::SocketListener::listener_fd_ = FAILED;
    uint sockets::SocketListener::conns_amount_ = 0;
    std::string sockets::SocketListener::listener_host_;
    std::string sockets::SocketListener::listener_port_;

    /*
     * Client sockets handler
     */
    // TODO: Сделать RawDataSocketBuffer именно закльцованный буфер из char *
    // логика должна быть независима от HTTP, остаток обратно в начало буфера занесём
    // TODO: атомарность операций с буфером
    class ClientSocketHandler
    {
    private:
        class RawDataCycledBuffer
        {
        public:
            // clear data frame
            char raw_data_sequence[DEFAULT_HTTP_REQUEST_SIZE *
                                   HTTP_MAX_SOCKET_REQUESTS_CAPACITY] = {0};

            static RawDataCycledBuffer *InitBuffer()
            {
                return new RawDataCycledBuffer();
            }

            RawDataCycledBuffer(const RawDataCycledBuffer &) = delete;
            RawDataCycledBuffer &operator=(const RawDataCycledBuffer &) = delete;
            ~RawDataCycledBuffer() = default;

            bool addData(const char *data) noexcept(true)
            {
                // Implementation placeholder
                return false;
            }

            bool extractData(char &extractHandler) noexcept(true)
            {
                // Implementation placeholder
                return false;
            }

            ssize_t getFreeSize() { return size; }
            const size_t available_space()
            {
                return size - placed;
            }
            // -1 means failed to fetch
            ssize_t tryAddData(const char *str, size_t len) noexcept(true)
            {
                if (len == 0)
                {
                    return 0;
                }
                else if (len > getFreeSize())
                {
                    std::cerr << "Not enough space for request. Skip";
                    return FAILED;
                }
                else if (write_pos == read_pos && placed != 0)
                {
                    std::cerr << "Something went wrong. Buffer dumped";
                    write_pos = 0;
                    read_pos = 0;
                    placed = 0;
                    return FAILED;
                }
                for (int i = 0; i < len; i++)
                {
                    raw_data_sequence[write_pos] = str[i];
                    write_pos = (write_pos + 1) %
                                (sizeof(raw_data_sequence) / sizeof(char));
                }
                placed += len;
                return len;
            }

            uint write_pos = 0, read_pos = 0, placed = 0;
            size_t size;
        private:
            RawDataCycledBuffer()
            {
                size = (size_t)sizeof(raw_data_sequence) / sizeof(char);
            }
        };

        // vars
        int socket_fd;
        uint client_port;
        bool isIPv6;
        std::string client_ip;
        sockaddr socket_info;
        RawDataCycledBuffer *buffer = nullptr;

        // TCP/IP data manipulations
        ssize_t proceedIncomeSocketData() noexcept(true)
        {
            int flags = 0;
            char tmpchar[DEFAULT_HTTP_REQUEST_SIZE];
            ssize_t res = recv(socket_fd, tmpchar, sizeof(tmpchar), flags);
            if (res == 0)
            {
                return 0;
            }
            uint write_pos_snapshot = buffer->write_pos;
            uint read_pos_snapshot = buffer->read_pos;
            uint placedCnt_snapshot = buffer->placed;
            ssize_t buffer_res;
            if (res > 0)
            {
                buffer_res = buffer->tryAddData(tmpchar, res);
            }
            if (res != buffer_res)
            {
                std::cerr << "Failed to add data. Skip socket data" << "\n";
                buffer->write_pos = write_pos_snapshot;
                buffer->read_pos = read_pos_snapshot;
                buffer->placed = placedCnt_snapshot;
                return FAILED;
            }
            return res;
        }

    public:
        ClientSocketHandler(int sockfd, struct sockaddr sa)
        {
            socket_fd = sockfd;
            client_port = get_in_port(&sa);
            isIPv6 = is_IPv6(&sa);
            client_ip = get_printable_ip(&sa);
            socket_info = sa;
            buffer = RawDataCycledBuffer::InitBuffer();
        }
        // ClientSocketHandler(const ClientSocketHandler&) = default;
        // ClientSocketHandler& operator=(const ClientSocketHandler&) = default;
        ~ClientSocketHandler()
        {
        }

        std::string getIP() { return client_ip; }
        uint getPort() { return client_port; }
        int getFd() { return socket_fd; }

        bool isSocketEmpty() {return (buffer->placed == 0) && buffer->read_pos
            == buffer->write_pos;}

        // TODO: implement multithreading
        size_t proceedIncomeSocketDataThreaded() noexcept(true)
        {
            return proceedIncomeSocketData();
        }
    };
}
