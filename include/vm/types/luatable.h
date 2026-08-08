#pragma once

#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <variant>
#include <vector>

#include "value.h"

class VM;
using LuaMap = std::unordered_map<Value, Value>;

class LuaTable
{
    public:
        LuaTable() = default;
        ~LuaTable() = default;

        LuaTableHandle metaTable;

        void set(VM &vm, const Value &key, const Value& value);

        Value get(VM &vm, const Value &key);

        bool exists(const Value &field)
        { 
            if (auto *doubleKey = std::get_if<double>(&field)) 
            {
                if (floor(*doubleKey) == *doubleKey && static_cast<size_t>(*doubleKey) - 1 < array.size()) return true;
            }

            return storage.contains(field); 
        }

        double length(VM &) { return static_cast<double>(array.size()); }

        void reserve(size_t amount)
        {
            array.reserve(array.capacity() + amount);
        }

    private:
        LuaMap storage;
        std::vector<Value> array; 
};
