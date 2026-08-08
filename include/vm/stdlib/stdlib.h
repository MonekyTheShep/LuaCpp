#pragma once

#include <array>
#include <memory>
#include <string>

class Library;
class VM;

class StdLib 
{    
    public:
        static void initLibraries(VM &vm);
    private:
        struct Lib 
        {
            std::string name;
            std::unique_ptr<Library> handle;
        };

        static std::array<Lib, 4> libraries;
};