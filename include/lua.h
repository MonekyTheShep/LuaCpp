#pragma once

#include "vm/vm.h"

#include <string>

class Lua 
{
    public:
        VMGlobal vmGlobal;

        Lua()
        : vmGlobal(VMGlobal())
        {
        }

        void run(std::string code);

        void debugRun(std::string code);
};