#include "lua_table.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <variant>

#include "vm.h"
#include "value.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void LuaTable::set(VM &vm, const Value &key, const Value& value)
{
    if (std::holds_alternative<LUA_NIL_TYPE>(key)) vm.runtimeError("Attempt to index a nil key!");

    if (const auto *doubleKey = std::get_if<double>(&key))
    {
        if (floor(*doubleKey) == *doubleKey)
        {
            size_t offset = static_cast<size_t>(*doubleKey) - 1;

            if (offset == array.size())
            {
                array.emplace_back(value);
                return;
            } 

            if (offset < array.size())
            {
                array[offset] = value;
                return;
            }
        }
    }
    
    if (std::holds_alternative<LUA_NIL_TYPE>(value))
    {
        storage.erase(key);
        return;
    }

    storage[key] = value;
}

Value LuaTable::get(VM &vm, const Value &key)
{
    if (std::holds_alternative<LUA_NIL_TYPE>(key)) vm.runtimeError("Attempt to index a nil key!");

    if (auto *doubleKey = std::get_if<double>(&key)) 
    {
        if (floor(*doubleKey) == *doubleKey)
        {
            size_t offset = static_cast<size_t>(*doubleKey) - 1;

            if (offset < array.size()) return array[offset];
        }
    }

    auto it = storage.find(key);
    if (it == storage.end()) return LUA_NIL_VALUE;
    return it->second;
}
