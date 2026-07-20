#pragma once

#include <array>
#include <span>

#include "library.h"
#include "value.h"

class BaseLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VM &vm) override;
    private:
        static int print(VM &vm, std::span<Value> args);
        static int tostring(VM &vm, std::span<Value> args);
        static int error(VM &vm, std::span<Value> args);
        static int luaAssert(VM &vm, std::span<Value> args);
        static int getmetatable(VM &vm, std::span<Value> args);
        static int setmetatable(VM &vm, std::span<Value> args);
        static int rawset(VM &vm, std::span<Value> args);
        static int rawget(VM &vm, std::span<Value> args);
        static int pcall(VM &vm, std::span<Value> args);
    private:
        static std::array<LibraryMethod, 9> methods;
};
