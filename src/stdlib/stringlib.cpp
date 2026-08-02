#include "stdlib/stringlib.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "meta.h"
#include "stdlib/library.h"
#include "value.h"
#include "vm.h"

int StringLib::upper(VM &vm, std::span<Value> args)
{
    std::string string = vm.argEnsure<std::string>(args, 0, "string value");

    std::transform (
        string.begin(), 
        string.end(), 
        string.begin(), 
        ::toupper
    );
    
    vm.push(std::move(string));

    return 1;
}

std::array<Library::Method, 1> StringLib::methods
{{
    {"upper",  &upper},
}};

LuaTableHandle StringLib::openLibrary(VM &vm) 
{
    LuaTableHandle luaTable = std::make_shared<LuaTable>();
    setLibraryFunctions(vm, methods, luaTable);
    LuaTableHandle metaTable = std::make_shared<LuaTable>();
    metaTable->set(vm, Meta::getName(Meta::Method::INDEX), luaTable);
    vm.setPrimitiveMt(VM::Primitives::STRING, metaTable);
    return luaTable;
}
