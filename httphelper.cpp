#include <string>
#include <stdexcept>
// TODO: implement body parser
namespace HTTP
{
    class request
    {
    private:
        std::string header = "";
        std::string body = "";
        size_t request_size = 0;
        bool header_extracted = false;
        bool body_extracted = false;
    public:
        request() = default;

        request(const char raw_data[], uint read_pos)
        {
            //header parser
            bool header_extracted = false;
            int i = 0;
            int rcnt = 0, ncnt = 0;
            char tmp;
            while(!header_extracted && i > sizeof(raw_data))
            {
                tmp = raw_data[read_pos];
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
            if(!header_extracted)
            {
                throw std::runtime_error("failed to parse html header");
            }
            //body extraction
            //TODO: if sequence not starting with <html> skip body parsing
            //TODO: if sequence ends up to </html>\n then complete parsing

        }
    };
    
    class response
    {
    };
}