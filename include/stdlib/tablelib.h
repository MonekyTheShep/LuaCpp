#pragma once

#include <array>
#include <span>

#include "library.h"
#include "value.h"

class TableLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VM &vm) override;
    private:
        static int pack(VM &vm, std::span<Value> args);
        static int unpack(VM &vm, std::span<Value> args);
        static int insert(VM &vm, std::span<Value> args);
        static int remove(VM &vm, std::span<Value> args);
    private:
        static std::array<LibraryMethod, 2> methods;
};