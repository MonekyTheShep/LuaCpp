#include "vm/stdlib/stringlib.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "vm/types/luatable.h"
#include "vm/types/meta.h"
#include "vm/stdlib/library.h"
#include "vm/types/value.h"
#include "vm/vm.h"

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

LuaTableHandle StringLib::openLibrary(VMGlobal &vmGlobal) 
{
    LuaTableHandle luaTable = std::make_shared<LuaTable>();
    setLibraryFunctions(vmGlobal, methods, luaTable);
    LuaTableHandle metaTable = std::make_shared<LuaTable>();
    metaTable->set(vmGlobal.main, Meta::getName(Meta::Method::INDEX), luaTable);
    vmGlobal.setPrimitiveMt(VMGlobal::Primitives::STRING, metaTable);
    return luaTable;
}
