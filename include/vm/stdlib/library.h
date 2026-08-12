#pragma once

#include <memory>
#include <span>
#include <string>

#include "vm/types/value.h"
#include "vm/vm.h"

class VMGlobal;

class Library 
{
    public:
        virtual LuaTableHandle openLibrary(VMGlobal &vmGlobal) = 0;
        
        virtual ~Library() = default;
        
    protected:
        struct Method 
        {
            std::string name;
            NativeFunctionPointer func;
        };

        void setLibraryFunctions(VMGlobal &vmGlobal, std::span<Method> methods, LuaTableHandle &luaTable)
        {
            for (const Method &method : methods)
            {
                luaTable->set(vmGlobal.main, method.name, makeNative(method));
            }
        }
    private:
        static NativeFunctionHandle makeNative(const Method &method) 
        {
            return std::make_shared<NativeFunction>(
                method.func,
                method.name
            );
        }
};
