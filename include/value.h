#pragma once

#include "bytecode.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using LUA_NIL_TYPE = std::monostate;
inline constexpr LUA_NIL_TYPE LUA_NIL_VALUE = std::monostate{};

class LuaTable;
struct NativeFunction;
struct FunctionChunk;
struct Closure;

using LuaTableHandle = std::shared_ptr<LuaTable>;
using NativeFunctionHandle = std::shared_ptr<NativeFunction>;
using FunctionHandle = std::shared_ptr<FunctionChunk>;
using ClosureHandle = std::shared_ptr<Closure>;

using Value = std::variant<LUA_NIL_TYPE, double, bool, std::string, LuaTableHandle, NativeFunctionHandle, FunctionHandle, ClosureHandle>;

namespace ValueHelper 
{
    std::optional<double> toNumber(const Value &value);
    std::optional<std::string> toString(const Value &value);
}

struct Chunk 
{
    Chunk() 
    {
        constants.reserve(UINT8_MAX);
    }

    std::vector<uint8_t> code;
    std::unordered_map<std::string_view, int> strings;

    std::vector<int> lines;
    std::vector<Value> constants;
    
    std::optional<int> makeConstant(Value value);
    void write(uint8_t arg, int line);
    void write(Op op, int line);
};

struct FunctionChunk 
{
    FunctionChunk(Chunk chunk, std::string name, size_t upValueCount, int arity,
                bool isVarArg)
      : chunk(std::move(chunk)), name(std::move(name)),
        upValueCount(upValueCount), arity(arity), isVarArg(isVarArg) {}

    Chunk chunk;
    std::string name;
    size_t upValueCount;
    int arity;
    bool isVarArg;
};

struct UpValue;
using UpValueHandle = std::shared_ptr<UpValue>;

struct UpValue 
{
    UpValue(Value* location): next(nullptr), location(location), closed(Value(LUA_NIL_VALUE)) {}

    UpValueHandle next;
    Value *location;
    Value closed;
};

struct Closure 
{
    Closure(const FunctionHandle& function): function(function) { upvalues.resize(function->upValueCount); };

    std::vector<UpValueHandle> upvalues;
    
    FunctionHandle function;
};

class VM;

using NativeFunctionPointer = int (*)(VM &vm, std::span<Value> args);
struct NativeFunction 
{
    NativeFunctionPointer function;
    std::string name;
};
