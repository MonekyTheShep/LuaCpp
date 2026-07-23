#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

enum class FileStatus : uint8_t
{
    FILE_ERROR,
    FILE_SUCCESS
};

namespace FileIo 
{
    inline std::pair<std::string, FileStatus> readFile(const std::filesystem::path& path)
    {
        std::string file;
        {
            std::ifstream inFile;
            inFile.open(path);
            if (!inFile.good()) 
            {
                return {"", FileStatus::FILE_ERROR};
            }

            std::stringstream strStream;
            strStream << inFile.rdbuf();
            file = strStream.str();
            inFile.close();
        }

        return {file, FileStatus::FILE_SUCCESS};
    }
}