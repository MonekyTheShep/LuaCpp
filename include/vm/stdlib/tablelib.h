#pragma once

#include <array>
#include <span>

#include "library.h"
#include "vm/types/value.h"

class VMGlobal;

class TableLib : public Library 
{
    public:
        LuaTableHandle openLibrary(VMGlobal &vmGlobal) override;
    private:
        static int pack(VM &vm, std::span<Value> args);
        static int unpack(VM &vm, std::span<Value> args);
        static int insert(VM &vm, std::span<Value> args);
        static int remove(VM &vm, std::span<Value> args);
    private:
        static std::array<Library::Method, 2> methods;
};