#pragma once

#include <array>
#include <span>

#include "library.h"
#include "vm/types/value.h"

class VMGlobal;

class StringLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VMGlobal &vmGlobal) override;
    private:
        static int upper(VM &vm, std::span<Value> args);
    private:
        static std::array<Library::Method, 1> methods;
};