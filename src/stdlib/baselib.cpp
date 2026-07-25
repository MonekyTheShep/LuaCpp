#include "stdlib/baselib.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <utility>
#include <variant>

#include "metamethod.h"
#include "stdlib/library.h"
#include "value.h"
#include "vm.h"

int BaseLib::print(VM &vm, std::span<Value> args)
{
    for (size_t i = 0; i < args.size(); i++)
    {
        std::cout << vm.toString(args[i]);
        if (i < args.size() - 1)
        {
            std::cout << "\t";
        }
    }

    std::cout << "\n";
    return 0;
}

int BaseLib::tostring(VM &vm, std::span<Value> args)
{
    vm.push(vm.toString(vm.argCheckAny(args, 0)));
    return 1;
}

int BaseLib::error(VM &vm, std::span<Value> args)
{
    vm.runtimeError(vm.argCheckAny(args, 0));
}

int BaseLib::luaAssert(VM &vm, std::span<Value> args)
{
    Value is = vm.argCheckAny(args, 0);
    Value message = vm.argOptAny(args, 1, "assertion failed!");

    if (!vm.isTruthy(is)) vm.runtimeError(message);

    for (const auto &arg : args)
    {
        vm.push(arg);
    }

    return static_cast<int>(args.size());
}

int BaseLib::getmetatable(VM &vm, std::span<Value> args)
{
    auto value = vm.argCheckAny(args, 0);
   
    auto metatable = vm.getMetaTable(value);

    if (auto metamethod = vm.resolveMetaMethod(metatable, Meta::Method::METATABLE))
    {
        vm.push(*metamethod);
    }
    else 
    {
        vm.push((metatable == nullptr) ? LUA_NIL_VALUE : Value(metatable));
    }

    return 1;
}

int BaseLib::setmetatable(VM &vm, std::span<Value> args)
{
    auto table = vm.argEnsure<LuaTableHandle>(args, 0, "table value");

    if (vm.resolveValueMetaMethod(table, Meta::Method::METATABLE))
    {
        vm.runtimeError("cannot change a protected metatable");
    }

    vm.argCheck<LUA_NIL_TYPE, LuaTableHandle>(args, 1, "table or nil value");

    bool isNull = std::holds_alternative<LUA_NIL_TYPE>(args[1]);
    table->metaTable = (isNull) ? nullptr :  std::get<LuaTableHandle>(args[1]);

    vm.push(table);

    return 1;
}

int BaseLib::rawset(VM &vm, std::span<Value> args)
{
    auto table = vm.argEnsure<LuaTableHandle>(args, 0, "table value");
    Value field = vm.argCheckAny(args, 1);
    Value value = vm.argCheckAny(args, 2);

    table->set(vm, field, value);
    vm.push(table);
    return 1;
}

int BaseLib::rawget(VM &vm, std::span<Value> args)
{
    auto table = vm.argEnsure<LuaTableHandle>(args, 0, "table value");
    Value field = vm.argCheckAny(args, 1);

    vm.push(table->get(vm, field));
    return 1;
}

int BaseLib::pcall(VM &vm, std::span<Value> args)
{
    vm.argCheckAny(args, 0);

    size_t calleeIndex = vm.sp - args.size();
    auto pcall = vm.protectedCall(calleeIndex);

    if (pcall.has_value())
    {
        return pcall.value();
    }
    else
    {
        vm.push(false);
        vm.push(pcall.error());
        return 2;
    }
}

int BaseLib::xpcall(VM &vm, std::span<Value> args)
{
    vm.argCheckAny(args, 0);

    vm.argCheck<ClosureHandle, NativeFunctionHandle>(args, 1, "function value");
    Value handler = args[1];

    size_t calleeIndex = vm.sp - args.size();
    vm.shiftLeft(calleeIndex + 2, 1); // Move args over handler

    auto pcall = vm.protectedCall(calleeIndex);

    if (pcall.has_value())
    {
        return pcall.value();
    }
    else
    {
        vm.push(false);
        size_t calleeIndex = vm.push(std::move(handler));
        vm.push(pcall.error());
        vm.callValue(calleeIndex, 1, VM::CallType::CPP);
        return 2;
    }
}

std::array<Library::Method, 10> BaseLib::methods
{{
    {"print",  &print},
    {"tostring", &tostring},
    {"error", &error},
    {"assert", &luaAssert},
    {"getmetatable", &getmetatable},
    {"setmetatable", &setmetatable},
    {"rawset", &rawset},
    {"rawget", &rawget},
    {"pcall", &pcall},
    {"xpcall", &xpcall},
}};

LuaTableHandle BaseLib::openLibrary(VM &vm) 
{
    setLibraryFunctions(vm, methods, vm.globals);
    return vm.globals;
}
