#include <string>
#include <string.h>
#include <stdexcept>

#define FAIL false
#define SUCCESS true
#define HTML_TAG_CONTAINER_SIZE 8 // </html>\n + \0 end string
// TODO: implement body parser
namespace HTTP
{
    class request
    {
    private:
        size_t request_size = 0;
        bool header_extracted = false;
        bool isHaveBody = false;
        bool body_extracted = false;

        //TODO: implement easy keywords recognition by structure fields
        struct DeserializedHTMLStructure
        {

        };
    public:
        std::string header = "";
        std::string body = "";
        request() = default;

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
                return FAIL;
            }
            // body extraction
            // if sequence not startidng with <html> skip body parsing
            // sequence ends up to </html>\n then complete parsing
            // Does it have <html> body? Alg
            int j = 0;
            char tagContainer[HTML_TAG_CONTAINER_SIZE] = {'\000'}; //empty
            for(; j < HTML_TAG_CONTAINER_SIZE-1 && i + j < len; j++)
            {
                tagContainer[j] = raw_data[i+j];
            }
            tagContainer[j] = '\0';
            if(strcmp(tagContainer, "<html>\n") != 0) {return SUCCESS;}
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
            if(this->isHaveBody && !this->body_extracted) {return FAIL;}
            return SUCCESS;
        }
    };
    
    class response
    {
    };
}