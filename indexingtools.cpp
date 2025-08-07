#include "indexingtools.hpp"

#include <stdexcept>
#include <iostream>

namespace IndexingTools
{
    bool hasEnding(const std::string& full, const std::string& ending)
    {
        if (full.length() >= ending.length()) {
            return full.compare(full.length() - ending.length(), ending.length(), ending) == 0;
        }
        return false;
    }

    bool isTextFile(const std::string& path)
    {
        std::string lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        return hasEnding(lowerPath, ".css") ||
            hasEnding(lowerPath, ".html") ||
            hasEnding(lowerPath, ".txt");
    }

    bool isHtmlFile(const std::string& path)
    {
        std::string lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        return hasEnding(lowerPath, ".html") ||
            hasEnding(lowerPath, ".htm");
    }

    // === FileExplorer ===
    FileExplorer::FileExplorer(const std::filesystem::path& path,
        OpenMode mode) : file_path(path)
    {
        if (stat(path.c_str(), &file_info) == -1)
        {
            throw std::runtime_error("Failed to extract file info");
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

    bool FileExplorer::FileExplorerIterator::next(std::string& outline) noexcept(true)
    {
        if (std::getline(data_stream, outline))
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
        if(this->mode == OpenMode::Text)
        {
            return 0;
        }

        buffer.resize(count);
        data_stream.read(buffer.data(), count);
        std::size_t bytesRead = data_stream.gcount();
        buffer.resize(bytesRead); // cut if less

        eof_reached = data_stream.eof();

        return bytesRead > 0;
    }

    bool FileExplorer::FileExplorerIterator::isEnd() const
    {
        return eof_reached;
    }
}
