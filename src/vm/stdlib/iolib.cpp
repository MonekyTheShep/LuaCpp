#include "vm/stdlib/iolib.h"

#include <array>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "vm/types/luatable.h"
#include "vm/stdlib/library.h"
#include "vm/types/value.h"
#include "vm/vm.h"

int IoLib::read(VM &vm, std::span<Value>)
{
    std::string input;
    std::getline(std::cin, input);
    vm.push(std::move(input));
    return 1;
}

std::array<Library::Method, 1> IoLib::methods
{{
    {"read", &read},
}};

LuaTableHandle IoLib::openLibrary(VMGlobal &vmGlobal) 
{
    LuaTableHandle luaTable = std::make_shared<LuaTable>();
    setLibraryFunctions(vmGlobal, methods, luaTable);
    return luaTable;
}
