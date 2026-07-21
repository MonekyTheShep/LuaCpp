#pragma once

#include <array>
#include <memory>
#include <string>

class Library;
class VM;

struct Lib 
{
    std::string name;
    std::unique_ptr<Library> handle;
};

class StdLib 
{    
    public:
        static void initLibraries(VM &vm);
    private:
        static std::array<Lib, 4> libraries;
};