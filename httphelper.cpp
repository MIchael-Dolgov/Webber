#include <string>
#include <string.h>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

#include "routing.cpp"

#define FAIL false
#define SUCCESS true
#define HTML_TAG_CONTAINER_SIZE 8 // </html>\n + \0 end string
namespace HTTP
{
    enum class StatusCode : int 
    {
        // 1xx Informational
        Continue = 100,
        SwitchingProtocols = 101,
        Processing = 102,

        // 2xx Success
        OK = 200,
        Created = 201,
        Accepted = 202,
        NoContent = 204,

        // 3xx Redirection
        MovedPermanently = 301,
        Found = 302,
        NotModified = 304,

        // 4xx Client Error
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        RequestTimeout = 408,

        // 5xx Server Error
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503,
    };

    inline std::string statusMessage(StatusCode code) 
    {
        switch (code) 
        {
            // 1xx
            case StatusCode::Continue: return "Continue";
            case StatusCode::SwitchingProtocols: return "Switching Protocols";
            case StatusCode::Processing: return "Processing";

            // 2xx
            case StatusCode::OK: return "OK";
            case StatusCode::Created: return "Created";
            case StatusCode::Accepted: return "Accepted";
            case StatusCode::NoContent: return "No Content";

            // 3xx
            case StatusCode::MovedPermanently: return "Moved Permanently";
            case StatusCode::Found: return "Found";
            case StatusCode::NotModified: return "Not Modified";

            // 4xx
            case StatusCode::BadRequest: return "Bad Request";
            case StatusCode::Unauthorized: return "Unauthorized";
            case StatusCode::Forbidden: return "Forbidden";
            case StatusCode::NotFound: return "Not Found";
            case StatusCode::MethodNotAllowed: return "Method Not Allowed";
            case StatusCode::RequestTimeout: return "Request Timeout";

            // 5xx
            case StatusCode::InternalServerError: return "Internal Server Error";
            case StatusCode::NotImplemented: return "Not Implemented";
            case StatusCode::BadGateway: return "Bad Gateway";
            case StatusCode::ServiceUnavailable: return "Service Unavailable";

            default: return "Unknown Status";
        }
    }

    class Request
    {
    private:
        bool header_extracted = false;
        bool isHaveBody = false;
        bool body_extracted = false;

    public:
        size_t request_char_len = 0;
        std::string header = "";
        std::string body = "";
        Request() = default;

        // FAIL(false) return means what HTML not full or broken
        bool tryExtractHTML(std::string& raw_data) noexcept(true)
        {
            this->header_extracted = false;
            this->isHaveBody = false;
            this->body_extracted = false;
            this->header = "";
            this->body = "";
            // Shift all elements left by one, add newChar at the end
            auto shiftLeftAndAppend = [](char* arr, size_t size, char newChar) 
            {
                for (size_t idx = 0; idx < size - 1; ++idx) 
                {
                    arr[idx] = arr[idx + 1];
                }
                arr[size - 1] = newChar;
            };
            //header parser
            size_t len = raw_data.length();
            size_t i = 0;
            int rcnt = 0, ncnt = 0;

            char tmp;
            while(!this->header_extracted && i < len)
            {
                tmp = raw_data[i];
                header += tmp;
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
                    header_extracted = true;
                }
                i += 1;
            }
            if(!this->header_extracted)
            {
                this->request_char_len = this->header.length();
                return FAIL;
            }
            // body extraction
            // if sequence not starting with <html> skip body parsing
            // sequence ends up to </html>\n then complete parsing
            // Does it have <html> body? Alg
            int j = 0;
            char tagContainer[HTML_TAG_CONTAINER_SIZE] = {'\000'}; //empty
            for(; j < HTML_TAG_CONTAINER_SIZE-1 && i + j < len; j++)
            {
                tagContainer[j] = raw_data[i+j];
            }
            tagContainer[j] = '\0';
            if(strcmp(tagContainer, "<html>\n") != 0) 
            {
                this->request_char_len = this->header.length();
                return SUCCESS;
            }
            this->isHaveBody = true;
            body += "<html>";
            i += j;
            //prepare container for </html>\n tag search
            memset(tagContainer, '\000', HTML_TAG_CONTAINER_SIZE);
            while(this->isHaveBody && !this->body_extracted && i < len)
            {
                tmp = raw_data[i];
                shiftLeftAndAppend(tagContainer, 
                    sizeof(tagContainer)/sizeof(char), tmp);
                body += tmp;
                if(strncmp(tagContainer, "</html>\n", 8) == 0) 
                {
                    this->body_extracted = true;
                }
                i += 1;
            }
            this->request_char_len = this->header.length() 
                + this->body.length();
            if(this->isHaveBody && !this->body_extracted) {return FAIL;}
            return SUCCESS;
        }
    };
    
    //TODO: сделать метод, который сможет откусывать по 1024 знака и возвращать
    // ссылку на такой объект, возращать данный кусок сокету клиента для 
    // дальнейшей POSIX отправки
    class Response {
    private:
        IndexingTools::FileExplorer explorer;
        bool isFound = false;

    public:
        Response(const std::string& directory, const std::string& filename)
            : explorer(directory, filename, isFound) {}

        bool readLine(std::string& outLine) noexcept(true) 
        {
            return explorer.readLine(outLine);
        }

        bool eof() const noexcept 
        {
            return explorer.isEOF();
        }
    };

    //TODO: implement deserializers
    class DeserializedHeader 
    {
    public:
        std::string method;
        std::string path;
        std::string version;
        std::unordered_map<std::string, std::string> headers;

        DeserializedHeader(const std::string& raw_header) 
        {
            std::istringstream stream(raw_header);
            std::string line;

            bool isFirstLine = true;

            while (std::getline(stream, line)) 
            {
                // Delete \r
                if (!line.empty() && line.back() == '\r') 
                {
                    line.pop_back();
                }

                if (line.empty()) continue;

                if (isFirstLine) 
                {
                    showLogFetchedRequestLine(line);
                    isFirstLine = false;
                    continue;
                }

                size_t colon_pos = line.find(':');
                if (colon_pos == std::string::npos) continue; //invalid stroke

                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                value.erase(0, value.find_first_not_of(" \t")); //something like trim

                headers[key] = value;
            }
        }

        void showLogFetchedRequestLine(const std::string& line) 
        {
            std::istringstream iss(line);
            iss >> method >> path >> version;
        }
    };

    class DeserializedBody
    {
    };
}
