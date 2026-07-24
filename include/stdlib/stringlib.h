#pragma once

#include <array>
#include <span>

#include "library.h"
#include "value.h"

class VM;

class StringLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VM &vm) override;
    private:
        static int upper(VM &vm, std::span<Value> args);
    private:
        static std::array<Library::Method, 1> methods;
};