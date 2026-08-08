#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace FileIo 
{
    inline std::optional<std::string> readFile(const std::filesystem::path& path)
    {
        std::string file;
        {
            std::ifstream inFile;
            inFile.open(path);
            if (!inFile.good()) 
            {
                return std::nullopt;
            }

            std::stringstream strStream;
            strStream << inFile.rdbuf();
            file = strStream.str();
            inFile.close();
        }

        return file;
    }
}