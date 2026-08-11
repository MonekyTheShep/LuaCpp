#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "value.h"

class VM;
using LuaMap = std::unordered_map<Value, Value>;

class LuaTable
{
    public:
        LuaTable() = default;
        ~LuaTable() = default;

        void set(VM &vm, const Value &key, const Value& value);

        Value get(VM &vm, const Value &key);

        bool exists(const Value &field);

        double length(VM &) { return static_cast<double>(array.size()); }

        void reserve(size_t amount) { array.reserve(array.capacity() + amount); }
    public:
        LuaTableHandle metaTable;
    private:
        LuaMap storage;
        std::vector<Value> array; 
};
