#include <cstdlib>
#include <iostream>
#include <utility>

#include "lua.h"
#include "utils/fileio.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: luacpp <script>" << '\n';
        return EXIT_FAILURE;
    }

    auto file = FileIo::readFile(argv[1]);

    if (!file)
    {
        std::cerr << "Error loading <script>!" << '\n';
        return EXIT_FAILURE;
    }

#ifdef NDEBUG
    Lua().run(std::move(result));
#else
    Lua().debugRun(std::move(*file));
#endif
    

    return EXIT_SUCCESS;
}
