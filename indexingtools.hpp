#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>

#include "interfaces.hpp"

namespace IndexingTools
{
    std::string getFileExtension(const std::string filePath);

    bool hasEnding(const std::string& full, const std::string& ending);

    bool isTextFileFormat(const std::string& path);

    bool isHtmlFile(const std::string& path);

    //Open Mode
    enum class OpenMode { Text, Binary };

    class FileExplorer
    {
    public:
        class FileExplorerIterator : public IIterator
        {
        private:
            OpenMode mode;
            bool eof_reached = false;
            std::ifstream data_stream;

        public:
            explicit FileExplorerIterator(const std::filesystem::path& path,
                OpenMode mode = OpenMode::Text);

            bool isEnd() const override;
            bool isBinaryMode();
            bool next(std::string& outLine) noexcept(true) override;
            bool nextBytes(std::vector<char>& buffer, std::size_t count);
        };

        FileExplorerIterator* getIterator();
        explicit FileExplorer(const std::filesystem::path& path,
            OpenMode mode = OpenMode::Text);
        size_t getFileSizeInBytes();

    private:
        std::filesystem::path file_path;
        FileExplorerIterator* iterator;
        struct stat file_info;
    };
}
