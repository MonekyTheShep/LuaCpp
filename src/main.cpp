#include <cstdlib>
#include <iostream>
#include <utility>

#include "lua.h"
#include "fileio.h"

#define DEBUG

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: luacpp <script>" << '\n';
        return EXIT_FAILURE;
    }

    auto [result, status] = FileIo::readFile(argv[1]);

    if (status == FileIo::FileStatus::FILE_ERROR)
    {
        std::cerr << "Error loading <script>!" << '\n';
        return EXIT_FAILURE;
    }

#ifdef DEBUG
    Lua().debugRun(std::move(result));
#else
    Lua().run(std::move(result));
#endif
    

    return EXIT_SUCCESS;
}
