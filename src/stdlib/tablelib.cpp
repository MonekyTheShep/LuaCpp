#include "stdlib/tablelib.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>

#include "stdlib/library.h"
#include "value.h"
#include "vm.h"
#include "lua_table.h"

int TableLib::pack(VM &vm, std::span<Value> args)
{
    LuaTableHandle table = std::make_shared<LuaTable>();
    double index = 1.0;
    for (const auto &arg : args)
    {
        table->set(vm, index++, arg);
    }

    table->set(vm,  "n", static_cast<double>(args.size()));
                 
    vm.push(table);         
    return 1;
}

int TableLib::unpack(VM &vm, std::span<Value> args)
{
    auto table = vm.argEnsure<LuaTableHandle>(args, 0, "table value");

    auto start = vm.argOpt<double>(args, 1, 1.0, "number value");
    auto stop = vm.argOpt<double>(args, 2, vm.length(table), "number value");

    if (start > stop) return 0;

    int n = static_cast<int>(stop - start) + 1;
    while (start <= stop)
    {
        vm.push(table->get(vm, start++));
    }        

    return n;
}

std::array<LibraryMethod, 2> TableLib::methods
{{
    {"pack",  &pack},
    {"unpack",  &unpack},
}};
        
LuaTableHandle TableLib::openLibrary(VM &vm) 
{
    LuaTableHandle luaTable = std::make_shared<LuaTable>();
    setLibraryFunctions(vm, methods, luaTable);
    return luaTable;
}