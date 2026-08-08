#pragma once

#include <array>
#include <span>

#include "library.h"
#include "vm/types/value.h"

class VM;

class IoLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VM &vm) override;
    private:
        static int read(VM &vm, std::span<Value> args);
    private:
        static std::array<Library::Method, 1> methods;
};