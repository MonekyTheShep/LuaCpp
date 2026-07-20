#pragma once

#include <memory>
#include <span>
#include <string>

#include "value.h"
#include "lua_table.h"

struct LibraryMethod 
{
    std::string name;
    NativeFunctionPointer func;
};

class Library 
{
    public:
        virtual LuaTableHandle openLibrary(VM &vm) = 0;
        
        virtual ~Library() = default;
    protected:
        static NativeFunctionHandle makeNative(const LibraryMethod &method) 
        {
            return std::make_shared<NativeFunction>(
                method.func,
                method.name
            );
        }

        void setLibraryFunctions(VM &vm, std::span<LibraryMethod> methods, LuaTableHandle &luaTable)
        {
            for (const LibraryMethod &method : methods)
            {
                luaTable->set(vm, method.name, makeNative(method));
            }
        }
};
