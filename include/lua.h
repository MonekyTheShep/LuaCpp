#pragma once

#include "vm.h"

#include <string>

class Lua 
{
    public:
        VM vm;

        Lua()
        : vm(*this)
        {
        }

        void run(std::string code);

        void debugRun(std::string code);
};