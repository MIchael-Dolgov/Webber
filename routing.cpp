#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace IndexingTools
{
    namespace fs = std::filesystem;

    class FileExplorer 
    {
    private:
        std::ifstream file_stream;
        bool eof_reached = false;

    public:
        FileExplorer(const fs::path& directory, const std::string& filename, 
            bool& outFlag) 
        {
            bool found = false;
            for (const auto& entry : fs::directory_iterator(directory)) 
            {
                if (entry.is_regular_file() 
                    && entry.path().filename() == filename) 
                {
                    file_stream.open(entry.path());
                    if (!file_stream.is_open()) 
                    {
                        throw std::runtime_error("Failed to open file");
                    }
                    found = true;
                    break;
                }
            }
            outFlag = found;
            if (!found) 
            {
                throw std::runtime_error("File: " + filename 
                    + " not found in directory: " + (std::string)directory);
            }
        }

        bool isEOF() const 
        {
            return eof_reached;
        }

        // if EOF reached then methods returns false
        bool readLine(std::string& outLine) noexcept(true)
        {
            if (eof_reached)
            {
                return false;
            }

            if (std::getline(file_stream, outLine))
            {
                return true;
            }
            else
            {
                eof_reached = true;
                return false;
            }
        }
    };
}
