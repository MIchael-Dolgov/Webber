#include "indexingtools.hpp"

namespace IndexingTools
{
    std::string getFileExtension(const std::string filePath) 
    {
        size_t dotPos = filePath.find_last_of('.');
    
        size_t slashPos = filePath.find_last_of("/\\");

        if (dotPos != std::string::npos && 
            (slashPos == std::string::npos || dotPos > slashPos)) 
        {
            std::string ext = filePath.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }
    
        return ""; //no extension
    }

    bool hasEnding(const std::string& full, const std::string& ending)
    {
        if (full.length() >= ending.length()) 
        {
            return full.compare(full.length() 
                - ending.length(), ending.length(), ending) == 0;
        }
        return false;
    }

    //TODO: refactoring required
    bool isTextFileFormat(const std::string& path)
    {
        std::string lowerPath = path;
        std::transform(
            lowerPath.begin(), 
            lowerPath.end(), 
            lowerPath.begin(), 
            ::tolower
        );

        //TODO: refactoring with enums or pattern matching required
        return hasEnding(lowerPath, ".css") ||
            hasEnding(lowerPath, ".html") ||
            hasEnding(lowerPath, ".js") ||
            hasEnding(lowerPath, ".txt");
    }

    // === FileExplorer ===
    FileExplorer::FileExplorer(const std::filesystem::path& path,
        OpenMode mode) : file_path(path)
    {
        if (!std::filesystem::exists(path)) 
        {
            throw std::runtime_error("File not found: " + path.string());
        }
        if (stat(path.string().c_str(), &file_info) == -1) 
        {
            throw std::runtime_error("Failed to extract file info for path: " 
                + path.string());
        }

        iterator = new FileExplorerIterator(file_path, mode);
    }

    FileExplorer::FileExplorerIterator* FileExplorer::getIterator()
    {
        return iterator;
    }

    size_t FileExplorer::getFileSizeInBytes()
    {
        return file_info.st_size;
    }

    // === FileExplorerIterator ===
    FileExplorer::FileExplorerIterator::FileExplorerIterator(
        const std::filesystem::path& path, OpenMode mode)
    {
        this->mode = mode;
        std::ios_base::openmode params = std::ios::in;
        if (mode == OpenMode::Binary) 
        {
            params |= std::ios::binary;
        }

        data_stream.open(path, params);

        if (!data_stream.is_open()) 
        {
            throw std::runtime_error("Failed to open file in iterator: " 
                + path.string());
        }
    }

    bool FileExplorer::FileExplorerIterator
        ::next(std::string& outline) noexcept(true)
    {
        if (this->isBinaryMode())
        {
            throw std::runtime_error("iterator has been opened in binary mode");
        }
        if (std::getline(data_stream, outline)) //this method remove \n in source
        {
            return true;
        }
        else
        {
            eof_reached = data_stream.eof();
            return false;
        }
    }

    bool FileExplorer::FileExplorerIterator::isBinaryMode()
    {
        if(this->mode == OpenMode::Binary) {return true;}
        return false;
    }

    bool FileExplorer::FileExplorerIterator::nextBytes(
        std::vector<char>& buffer, size_t count)
    {
        if (this->mode == OpenMode::Text)
        {
            return false;
        }
        if (eof_reached)
        {
            return false;
        }

        if(buffer.capacity() < count)
        {
            buffer.resize(count);
        }
    
        data_stream.read(buffer.data(), count);

        std::size_t bytesRead = data_stream.gcount();

        buffer.resize(bytesRead);

        eof_reached = data_stream.eof();

        return bytesRead > 0;
    }

    bool FileExplorer::FileExplorerIterator::isEnd() const
    {
        return eof_reached;
    }
}
