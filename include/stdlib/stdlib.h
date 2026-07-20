#pragma once

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
        static Lib libraries[];
};