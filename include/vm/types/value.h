#pragma once

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

#include "compiler/bytecode.h"

using LUA_NIL_TYPE = std::monostate;
inline constexpr LUA_NIL_TYPE LUA_NIL_VALUE = std::monostate{};

class LuaTable;
struct NativeFunction;
struct FunctionChunk;
struct Closure;
struct Coroutine;

using LuaTableHandle = std::shared_ptr<LuaTable>;
using NativeFunctionHandle = std::shared_ptr<NativeFunction>;
using FunctionHandle = std::shared_ptr<FunctionChunk>;
using ClosureHandle = std::shared_ptr<Closure>;
using CoroutineHandle = std::shared_ptr<Coroutine>;

using Value = std::variant<LUA_NIL_TYPE, double, bool, std::string, LuaTableHandle, NativeFunctionHandle, FunctionHandle, ClosureHandle, CoroutineHandle>;

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
    void write(ByteCode::Op op, int line);
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
    UpValue(Value* location): location(location), closed(Value(LUA_NIL_VALUE)) {}

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

using VMHandle = std::unique_ptr<VM>;

struct Coroutine
{
    public:
        enum class Status : uint8_t
        {
            Pending,
            Running,
            Normal,
            Dead
        };

        Status status;
        VMHandle vm;
    public:
        std::string statusToString()
        {
            switch (status)
            {   
                case Status::Pending: return "Pending"; break;
                case Status::Running: return "Running"; break;
                case Status::Normal: return "Normal"; break;
                case Status::Dead: return "Dead"; break;
            }          
        }
};


using NativeFunctionPointer = int (*)(VM &vm, std::span<Value> args);
struct NativeFunction 
{
    NativeFunctionPointer function;
    std::string name;
};
