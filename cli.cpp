#include <string>
#include <stdexcept>

#define DEFAULT_WEB_SERVER_PORT "10980"
#define DEFAULT_THREADS_LIMIT 8u
#define CONNS_AMOUNT 128u
#define DEFAULT_HOST_IP "127.0.0.1"
//#define DEFAULT_PACKET_TRANSFER_SIZE 1024u
//#define DEFAULT_REQUEST_LENGTH 1024u
#define DEFAULT_PAGES_ROUTES_INDEXING_FILE_LOCATION "routes.txt"
#define DEFAULT_SENDING_PACKET_SIZE_IN_CHARS 1024u

// ===Pretty instruments===
namespace CliTools
{
    class ConsoleColor {
    public:
        enum class Color {
            Default = 37,  // стандартный цвет
            Red = 31,
            Green = 32,
            Yellow = 33,
            Blue = 34,
            Magenta = 35,
            Cyan = 36
        };

        static std::string getAnsiCode(Color color) {
            return std::string("\e[0;") + 
                std::to_string(static_cast<int>(color)) + "m";
        }
    };

    void printColoredMessage(std::ostream& out, const std::string& message, ConsoleColor::Color color)
    {
        out << ConsoleColor::getAnsiCode(color) << message << "\n"
            << ConsoleColor::getAnsiCode(ConsoleColor::Color::Default);
    }
}

// ===Global webserver condition===
class WebCliConfig
{
private:
    static constexpr const char *help_text =
        "======================================\n"
        "||              Webber              ||\n"
        "||       HTTP 1.1 Web Server        ||\n"
        "======================================\n"
        "flags:\n"
        "--help help docstring\n"
        "-p     webserver listening port\n"
        "-ip    set webserver ip\n"
        "-t     max working threads limit\n"
        "-c     set max connections limit\n"
        "-r     set specific file routes location\n"
        "s      set max sending packet size\n"
        "";

    //class methods
    WebCliConfig() = default;
    WebCliConfig(const WebCliConfig&) = delete;
    WebCliConfig& operator=(const WebCliConfig&) = delete;

    //config
    std::string port = DEFAULT_WEB_SERVER_PORT;
    std::string host_ip = DEFAULT_HOST_IP;
    uint threads = DEFAULT_THREADS_LIMIT;
    uint max_connections = CONNS_AMOUNT;
    std::string routes = DEFAULT_PAGES_ROUTES_INDEXING_FILE_LOCATION;
    size_t sending_packet_size = DEFAULT_SENDING_PACKET_SIZE_IN_CHARS;

    bool created = false;

    //input checkouts
    void isValidIP(std::string str)
    {
        for(const char &ch: str)
        {
            if(ch != '.' && !std::isdigit(ch))
            {
                throw std::runtime_error("invalid ip");
            }
        }
    }

public:

    std::string getPort() noexcept(true) {return port;}
    std::string getIP() noexcept(true) {return host_ip;}
    uint getThreadsCnt() noexcept(true) {return threads;}
    uint getMaxConns() noexcept(true) {return max_connections;}
    std::string getRoutesFile() noexcept(true) {return routes;}
    size_t getSendingPacketSize() noexcept(true) {return sending_packet_size;}

    void setParam(const std::string& key, const std::string& value)
    {
        //correct arguments checkouts not comprehensive yet
        if(key == "-p") {/*check*/std::stoul(value); port = value; return;}
        if(key == "-ip") {/*check*/isValidIP(value); host_ip = value; return;}
        if(key == "-t") {threads = (uint)std::stoul(value); return;}
        if(key == "-c") {max_connections = (uint)std::stoul(value); return;}
        if(key == "-r") {routes = value;}
        if(key == "-s") {sending_packet_size = (uint)std::stoul(value); return;}
        throw std::runtime_error("invalid argument");
    }

    static WebCliConfig& instance()
    { 
        static WebCliConfig single_instance;
        return single_instance;
    }

    static const char* help(void)
    {
        return help_text;
    }
};
