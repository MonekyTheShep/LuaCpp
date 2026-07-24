#include "stdlib/iolib.h"

#include <array>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "stdlib/library.h"
#include "value.h"
#include "vm.h"

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

LuaTableHandle IoLib::openLibrary(VM &vm) 
{
    LuaTableHandle luaTable = std::make_shared<LuaTable>();
    setLibraryFunctions(vm, methods, luaTable);
    return luaTable;
}
