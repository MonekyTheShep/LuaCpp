#pragma once

#include <memory>
#include <span>
#include <string>

#include "value.h"
#include "lua_table.h"

class Library 
{
    public:
        virtual LuaTableHandle openLibrary(VM &vm) = 0;
        
        virtual ~Library() = default;
        
    protected:
        struct Method 
        {
            std::string name;
            NativeFunctionPointer func;
        };

        static NativeFunctionHandle makeNative(const Method &method) 
        {
            return std::make_shared<NativeFunction>(
                method.func,
                method.name
            );
        }

        void setLibraryFunctions(VM &vm, std::span<Method> methods, LuaTableHandle &luaTable)
        {
            for (const Method &method : methods)
            {
                luaTable->set(vm, method.name, makeNative(method));
            }
        }
};
