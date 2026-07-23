#include "vm.h"

#include <algorithm>
#include <cassert>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "bytecode.h"
#include "lua_table.h"
#include "metamethod.h"
#include "value.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

VMRuntimeError::VMRuntimeError(VM &vm, const Value &object, std::optional<ErrorPosInfo> posInfo) 
    : posInfo(std::move(posInfo))
    , value(object)
{
    error = std::visit(overloaded
    {
        [](const std::string &value) -> std::string
        {
            return value;
        },
        [&vm](const auto &value) -> std::string
        {
            return std::format("(error object is a {} value)", vm.type(value));
        }
    }, object);
}

const char* VMRuntimeError::what() const noexcept 
{
    return error.c_str();
}

Value VMRuntimeError::getObj() const noexcept
{
    return std::visit(overloaded
    {
        [this](const std::string &value) -> Value
        {
            return (posInfo) ? std::format("{}:{} {}", posInfo->funcName, posInfo->line, value) : value;
        },
        [](const auto &value) -> Value
        {
            return value;
        }
    }, value);
}

VM::VM(Lua &lua)
    : opCounts({})
    , globals(std::make_shared<LuaTable>())
    , lua(lua)
    , sp(0)
    ,
    runDepth(0) 
    {
        callFrames.reserve(MAX_FRAMES);
        stack.resize(STACK_SIZE);

        callees.reserve(20); // Vectors likely to grow
        tables.reserve(20);
        errorHandlers.reserve(5);

        StdLib::initLibraries(*this);
}

struct CallVisitor 
{
    VM& vm;
    const size_t calleeIndex;
    const int expectedReturn;
    int depth;
    const CallFrame::CallType callType;
    
    CallVisitor(size_t calleeIndex, int expectedReturn, CallFrame::CallType type, VM& vm)
        : vm(vm), calleeIndex(calleeIndex), expectedReturn(expectedReturn), depth(0), callType(type) {}

    void operator()(const NativeFunctionHandle &nativeFunction) const
    {
        vm.nativeCall(nativeFunction, calleeIndex, expectedReturn);
    }

    void operator()(const ClosureHandle &closure)
    {
        if (callType == CallFrame::CallType::CPP && ++vm.runDepth >= 100) vm.runtimeError("C++ stack overflow!");

        vm.call(closure, calleeIndex, expectedReturn, callType);

        if (callType == CallFrame::CallType::CPP) { vm.run(); vm.runDepth--; } 
    }

    void operator()(const LuaTableHandle &table)
    {
        std::optional<Value> function = vm.resolveMetaMethod(table, MetaMethod::CALL);

        if (!function) vm.runtimeError("Attempt to call a table value!");

        if (++depth >= 100) vm.runtimeError("Potential infinite loop within `__call`!");
    
        // Insert table in front of args
        {
            Value *start = &vm.stack[calleeIndex + 1];
            Value *end = &vm.stack[vm.sp];

            vm.checkStack(vm.sp, 1);
            vm.sp++;

            std::move_backward(start, end, end + 1);

            *start = table; 
        }

        vm.stack[calleeIndex] = *function;
        std::visit(*this, *function);
    }

    template <typename T>
    void operator()(const T &value) const 
    {
        vm.runtimeError(std::format("Attempt to call a {} value!", vm.type(value)));
    }
};

void VM::callValue(size_t calleeIndex, int expectedReturn, CallFrame::CallType type)
{
    std::visit(CallVisitor(calleeIndex, expectedReturn, type, *this), stack[calleeIndex]);
}

void VM::pushErrorHandler(size_t sp) 
{
    errorHandlers.emplace_back(sp, callFrames.size(), callees.size(), tables.size(), runDepth);
}

void VM::popErrorHandler() 
{
    assert(!errorHandlers.empty());
    errorHandlers.pop_back();
}

void VM::recoverVM() 
{
    assert(!callFrames.empty());
    assert(!errorHandlers.empty());

    auto &handler = errorHandlers.back();

    closeUpValues(&stack[handler.sp]);
    sp = handler.sp;

    assert(handler.frames <= callFrames.size());

    callFrames.erase(callFrames.begin() + 
        static_cast<ptrdiff_t>(handler.frames), callFrames.end());

    assert(handler.callees <= callees.size());

    callees.resize(handler.callees);

    assert(handler.tables <= tables.size());

    tables.resize(handler.tables);

    runDepth = handler.runDepth;
    
    errorHandlers.pop_back(); // Make sure to remove the error handler
}

void VM::runtimeError(const Value &error) 
{
    if (!errorHandlers.empty()) // Custom message for recoverable errors
    {
        auto &frame = callFrames.back();
        auto &function = frame.closure->function;

        int line = function->chunk.lines[frame.ip - 1];
        std::string name = function->name;

        throw VMRuntimeError(*this, error, VMRuntimeError::ErrorPosInfo{name, line});
    }

    for (auto i = callFrames.size(); i-- > 0; ) 
    {
        auto &frame = callFrames[i];

        int line = frame.closure->function->chunk.lines[frame.ip - 1];

        std::cerr << std::format("[line {}] in ", line);

        std::cerr << frame.closure->function->name << "\n";
    }

    throw VMRuntimeError(*this, error, std::nullopt);
}

void VM::nativeCall(const NativeFunctionHandle &nativeFunction, size_t calleeIndex, int expectedReturn)
{
    assert(calleeIndex < sp);

    size_t argStart = calleeIndex + 1;
    size_t argEnd = sp;

    auto args = std::span<Value>(stack).subspan(argStart, argEnd - argStart);

    int numOfReturn = nativeFunction->function(*this, args);
    moveReturns(sp - static_cast<size_t>(numOfReturn), calleeIndex, expectedReturn);
}

void VM::call(const ClosureHandle &closure, size_t calleeIndex, int expectedReturn, CallFrame::CallType type)
{
    if(callFrames.size() + 1 > MAX_FRAMES) runtimeError("Potential infinite recursion!");
                    
    std::vector<Value> vararg;

    assert(calleeIndex < sp);
    int argc = static_cast<int>(sp - calleeIndex - 1);
    auto &function = closure->function;

    int argDiff = function->arity - argc;

    if (argDiff < 0) 
    {
        size_t fixedArgEnd = calleeIndex + static_cast<size_t>(function->arity) + 1;

        if (function->isVarArg)
        {
            Value *start = &stack[fixedArgEnd];
            Value *top = &stack[sp];

            size_t numOfVarArg = sp - fixedArgEnd;
            vararg.resize(numOfVarArg);

            std::move(start, top, vararg.begin());
        }

        sp = fixedArgEnd;
    }
    else if (argDiff > 0)
    {
        for (; argDiff > 0; argDiff--) 
        {
            push(LUA_NIL_VALUE);
        }
    }

    callFrames.emplace_back (
        std::move(vararg),
        closure,
        0,
        calleeIndex,
        expectedReturn,
        type
    );
}

void VM::moveReturns(size_t firstResult, size_t frameBase, int expectedReturn)
{
    assert(firstResult <= sp);
    assert(frameBase <= firstResult);

    Value *start = &stack[firstResult];
    Value *top = &stack[sp];

    sp = frameBase;

    if (expectedReturn == RETURN_ALL)
    {
        while (start < top)
        {
            push(std::move(*start++));
        }
    }
    else
    {
        assert(expectedReturn >= 0);
        while (expectedReturn-- > 0)
        {
            push((start < top) ? std::move(*start++): LUA_NIL_VALUE);
        }
    }
}

LuaTableHandle VM::getMetaTable(const Value &value)
{
    return std::visit(overloaded
    {
        [this](const std::string &) -> LuaTableHandle 
        {
            return primitiveMt[static_cast<size_t>(Primitives::STRING)];
        },
        [](const LuaTableHandle &table) -> LuaTableHandle 
        {
            return table->metaTable;
        },
        [](const auto &) -> LuaTableHandle 
        {
            return nullptr;
        }
    }, value);
}

std::optional<Value> VM::resolveMetaMethod(const LuaTableHandle &metatable, MetaMethod method)
{
    if (!metatable) return std::nullopt;

    const std::string &name = Method::names[static_cast<size_t>(method)];

    if (!metatable->exists(name)) return std::nullopt;

    return metatable->get(*this, name);
}


std::optional<Value> VM::resolveValueMetaMethod(const Value &value, MetaMethod method)
{
    LuaTableHandle metatable = getMetaTable(value);

    return resolveMetaMethod(metatable, method);
}

bool VM::tryMetaMethod(std::initializer_list<Value> values, MetaMethod method, CallFrame::CallType callType)
{
    std::optional<Value> function;

    for (const Value &value : values)
    {
        if ((function = resolveValueMetaMethod(value, method))) break; 
    }

    if (!function) return false;
        
    size_t calleeIndex = push(*function);
    for (const Value &value : values)
    {
        push(value);
    }
    
    callValue(calleeIndex, 1, callType);
    return true;
}

UpValueHandle VM::captureUpValue(Value *location)
{
    UpValueHandle prevUpvalue = nullptr;
    UpValueHandle upValue = openUpvalues;

    while (upValue != nullptr && upValue->location > location)
    {
        prevUpvalue = upValue;
        upValue = upValue->next;
    }

    if (upValue != nullptr && upValue->location == location)
    {
        return upValue;
    }

    UpValueHandle newUpValue = std::make_shared<UpValue>(UpValue{location});

    newUpValue->next = upValue;

    if (prevUpvalue == nullptr)
    {
        openUpvalues = newUpValue;
    } 
    else
    {
        prevUpvalue->next = newUpValue;
    }

    return newUpValue;
}

void VM::closeUpValues(Value *last)
{
    while (openUpvalues != nullptr && openUpvalues->location >= last)
    {
        auto& upvalue = openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        openUpvalues = upvalue->next;
    }
}

template <typename T>
void VM::handleBinaryError(std::string_view msg, const Value &a, const Value &b, const std::optional<T> &lhs, const std::optional<T> &rhs)
{
    std::string typeStr;
    
    assert(!lhs || !rhs);

    if (!lhs) typeStr = type(a);
    else if (!rhs) typeStr = type(b);

    runtimeError(std::format("Attempt to perform {} on a {} value", msg, typeStr));
}

template <typename T>
void VM::pushCompare(Op op, const T &a, const T &b) 
{
    if (op == Op::LS) push(a < b);
    else if (op == Op::LSE) push(a <= b);
    else assert(false); // Unreachable
}

void VM::handleCompare(Op op, MetaMethod method)
{
    Value b = pop();
    Value a = pop();

    std::visit(overloaded 
    {
        [this, op](const double a, const double b) 
        {
            pushCompare<double>(op, a, b);
        },
        [this, op](const std::string &a, const std::string &b) 
        {
            pushCompare<std::string>(op, a, b);
        },
        [this, method](const auto &a, const auto &b)  
        {
            if (!tryMetaMethod({a,b}, method, CallFrame::CallType::CPP)) 
                runtimeError(std::format("Attempt to compare {} with {}!", type(a), type(b)));

            push(isTruthy(pop()));
        }
    }, a, b);
}

void VM::handleEquality()
{
    Value b = pop();
    Value a = pop();

    if (std::holds_alternative<LuaTableHandle>(a) 
    && std::holds_alternative<LuaTableHandle>(b))
    {
        if (tryMetaMethod({a,b}, MetaMethod::EQ, CallFrame::CallType::CPP))
        {
            push(isTruthy(pop()));
            return;
        }
    }
    
    push(a == b);
}

#define BIT_OP(op, lhs, rhs) static_cast<int32_t>(static_cast<uint32_t>(lhs) op static_cast<uint32_t>(rhs))

namespace 
{
    constexpr int NBITS = sizeof(int32_t) * 8;

    int32_t bitShift(int32_t x, int32_t y) 
    // https://www.lua.org/source/5.3/lvm.c.html#luaV_shiftl
    {
        if (y < 0) 
        {  /* shift right? */
            if (y <= -NBITS) return 0;
            else return BIT_OP(>>, x, -y);
        }
        else 
        {  /* shift left */
            if (y >= NBITS) return 0;
            else return BIT_OP(<<, x, y);
        }
    }  
}

int32_t VM::doubleToInt(double num)
{
    const bool valid = 
        std::isfinite(num) 
        && (floor(num) == num) 
        && ((num <= INT32_MAX) && (num >= INT32_MIN));

    if (!valid)
        runtimeError("Number has no integer representation!"); 

    return static_cast<int32_t>(num);
}

void VM::handleBitWise(Op op, MetaMethod method)
{
    Value b = pop();
    Value a = pop();

    std::optional<double> lhs, rhs;

    if ((lhs = ValueHelper::toNumber(a)) && 
    (rhs = ValueHelper::toNumber(b))) 
    {

        int32_t iLhs = doubleToInt(*lhs);
        int32_t iRhs = doubleToInt(*rhs);

        int32_t result = 0;

        switch (op)
        {
            case Op::BIT_AND: result = BIT_OP(&, iLhs, iRhs); break;
            case Op::BIT_OR: result =  BIT_OP(|, iLhs, iRhs); break;
            case Op::BIT_XOR: result = BIT_OP(^, iLhs, iRhs); break;
            case Op::BITSHIFT_LEFT:  result = bitShift(iLhs, iRhs); break;
            case Op::BITSHIFT_RIGHT: result = bitShift(iLhs, -iRhs); break;
            default:
                assert(false); // Unreachable
        }

        push(static_cast<double>(result));
    } 
    else if (!tryMetaMethod({a,b}, method, CallFrame::CallType::LUA))
    {
        handleBinaryError("bitwise operation", a, b, lhs, rhs);
    }
}

#undef BIT_OP

void VM::handleConcat()
{
    Value b = pop();
    Value a = pop();

    std::optional<std::string> lhs, rhs;

    if ((lhs = ValueHelper::toString(a)) && 
    (rhs = ValueHelper::toString(b))) 
    {
        push(*lhs + *rhs);
    } 
    else if (!tryMetaMethod({a,b}, MetaMethod::CONCAT, CallFrame::CallType::LUA))
    {
        handleBinaryError("concat operation", a, b, lhs, rhs);
    }
}

void VM::handleArithmetic(Op op, MetaMethod method)
{
    Value b = pop();
    Value a = pop();

    std::optional<double> lhs, rhs;

    if ((lhs = ValueHelper::toNumber(a)) && 
    (rhs = ValueHelper::toNumber(b))) 
    {
        switch (op)
        {
            case Op::ADD: push(*lhs + *rhs); break;
            case Op::SUB: push(*lhs - *rhs); break;
            case Op::MUL: push(*lhs * *rhs); break;  
            case Op::FLOOR_DIV: push(floor(*lhs / *rhs)); break;
            case Op::DIV: push(*lhs / *rhs); break;
            case Op::MOD: push(fmod(*lhs, *rhs)); break;
            case Op::EXPO: push(pow(*lhs, *rhs)); break;
            default:
                assert(false); // Unreachable
        }
    }
    else if (!tryMetaMethod({a,b}, method, CallFrame::CallType::LUA))
    {
        handleBinaryError("arithmetic operation", a, b, lhs, rhs);
    }
}

void VM::luaTableSet(const Value &table, const Value &key, const Value &value, int depth)
{
    if (depth >= 100) runtimeError("Potential infinite loop within `__newindex`");

    std::optional<Value> meta = resolveValueMetaMethod(table, MetaMethod::NEWINDEX);

    if (auto* luaTable = std::get_if<LuaTableHandle>(&table))
    {
        if (!meta || (*luaTable)->exists(key))
        {
            return (*luaTable)->set(*this, key, value);
        }
    }

    if (!meta) runtimeError("Attempt to index a non table value!");

    if (isCallable(*meta))
    {
        size_t calleeIndex = push(*meta);
        push(table);
        push(key);
        push(value);
        callValue(calleeIndex, 0, CallFrame::CallType::LUA); 
        return;
    }
    
    return luaTableSet(*meta, key, value, depth + 1);
}

Value VM::luaTableGet(const Value &table, const Value &key, int depth)
{
    if (depth >= 100) runtimeError("Potential infinite loop within `__index`");

    std::optional<Value> meta = resolveValueMetaMethod(table, MetaMethod::INDEX);

    if (auto* luaTable = std::get_if<LuaTableHandle>(&table))
    {
        if (!meta || (*luaTable)->exists(key))
        {
            return (*luaTable)->get(*this, key);
        }
    } 

    if (!meta) runtimeError("Attempt to index a non table value!");

    if (isCallable(*meta))
    {
        size_t calleeIndex = push(*meta);
        push(table);
        push(key);
        callValue(calleeIndex, 1, CallFrame::CallType::CPP); 
        return pop();
    }

    return luaTableGet(*meta, key, depth + 1);
}

void VM::execute(const FunctionHandle &code)
{
    auto closure = std::make_shared<Closure>(Closure{code});
    size_t calleeIndex = push(closure);
    callValue(calleeIndex, 0, CallFrame::CallType::LUA);
    run();
}

uint8_t VM::readByte()
{
    auto &frame = callFrames.back();
    return frame.closure->function->chunk.code[frame.ip++];
}

int8_t VM::readSignedByte()
{
    auto &frame = callFrames.back();
    return static_cast<int8_t>(frame.closure->function->chunk.code[frame.ip++]);
}

Value VM::readConstant()
{
    auto &frame = callFrames.back();
    return frame.closure->function->chunk.constants[readByte()];
}

int16_t VM::readShort()
{
    auto &frame = callFrames.back();
    auto &chunk = frame.closure->function->chunk;
    const size_t ip = frame.ip += 2;

    auto jump = static_cast<int16_t>(chunk.code[ip - 2] << 8 | chunk.code[ip - 1]);

    return jump;
}

void VM::run()
{
    for(;;)
    {
        auto op = Op(readByte()); 
        opCounts[static_cast<size_t>(op)]++;

        switch (op)
        {
            case Op::LOAD_CONST:
            {
                push(readConstant());
                break;
            }
            case Op::STORE_LOCAL:
            {
                auto arg = readByte();
                stack[callFrames.back().frameBase + arg] = pop();
                break;
            } 
            case Op::STORE_GLOBAL:
            {
                luaTableSet(globals, readConstant(), pop());
                break;
            }
            case Op::STORE_UPVALUE:
            {
                auto arg = readByte();

                *callFrames.back().closure->upvalues[arg]->location = pop();
                break;
            }
            case Op::LOAD_LOCAL:
            {
                auto arg = readByte();

                push(stack[callFrames.back().frameBase + arg]);
                break;
            }
            case Op::LOAD_GLOBAL:
            {
                push(luaTableGet(globals, readConstant()));
                break;
            }
            case Op::LOAD_UPVALUE:
            {
                auto arg = readByte();

                push(*callFrames.back().closure->upvalues[arg]->location);
                break;
            }
            case Op::LOAD_NULL:
            {
                push(LUA_NIL_VALUE);
                break;
            }
            case Op::LOAD_TRUE:
            {
                push(true);
                break;
            }
            case Op::LOAD_FALSE:
            {
                push(false);
                break;
            }
            case Op::POP:
            {
                pop();
                break;
            }
            case Op::DUP:
            {
                assert(sp - 1 > 0);
                push(stack[sp - 1]);
                break;
            }
            case Op::ADD:
            {
                handleArithmetic(Op::ADD, MetaMethod::ADD);
                break;
            }
            case Op::SUB:
            {
                handleArithmetic(Op::SUB, MetaMethod::SUB);
                break;
            }
            case Op::MUL:
            {
                handleArithmetic(Op::MUL, MetaMethod::MUL);
                break;
            }
            case Op::FLOOR_DIV:
            {
                handleArithmetic(Op::FLOOR_DIV, MetaMethod::IDIV);
                break;
            }
            case Op::DIV:
            {
                handleArithmetic(Op::DIV, MetaMethod::DIV);
                break;
            }
            case Op::MOD:
            {
                handleArithmetic(Op::MOD, MetaMethod::MOD);
                break;
            }
            case Op::EXPO:
            {
                handleArithmetic(Op::EXPO,MetaMethod::POW);
                break;
            }
            case Op::CONCAT:
            {
                handleConcat();
                break;
            }
            case Op::EQ:
            {
                handleEquality();
                break;
            }
            case Op::LS:
            {
                handleCompare(Op::LS, MetaMethod::LT);
                break;
            }
            case Op::LSE:
            {
                handleCompare(Op::LSE, MetaMethod::LE);
                break;
            }
            case Op::BIT_AND:
            {
                handleBitWise(Op::BIT_AND, MetaMethod::BAND);
                break;
            }
            case Op::BIT_OR:
            {
                handleBitWise(Op::BIT_OR, MetaMethod::BOR);
                break;
            }
            case Op::BIT_XOR:
            {
                handleBitWise(Op::BIT_XOR, MetaMethod::BXOR);
                break;
            }
            case Op::BITSHIFT_LEFT:
            {
                handleBitWise(Op::BITSHIFT_LEFT, MetaMethod::SHL);
                break;
            }
            case Op::BITSHIFT_RIGHT:
            {
                handleBitWise(Op::BITSHIFT_RIGHT, MetaMethod::SHR);
                break;
            }
            case Op::BIT_NOT:
            {
                Value value = pop();

                std::optional<double> num;
                if ((num = ValueHelper::toNumber(value))) 
                {
                    auto numInt = doubleToInt(*num);
                    push(static_cast<double>(static_cast<int32_t>(~static_cast<uint32_t>(numInt))));
                }
                else if (!tryMetaMethod({value}, MetaMethod::BNOT, CallFrame::CallType::LUA)) 
                    runtimeError(std::format("Attempt to perform binary operation a {} value", type(value)));
                break;
            }
            case Op::NEGATE:
            {
                Value value = pop();

                std::optional<double> num;
                if ((num = ValueHelper::toNumber(value))) push(-num.value());
                else if (!tryMetaMethod({value}, MetaMethod::UNM, CallFrame::CallType::LUA)) 
                    runtimeError(std::format("Attempt to negate a {} value", type(value)));
                break;
            }
            case Op::LENGTH:
            {
                push(length(pop()));
                break;
            }
            case Op::NOT:
            {
                push(!isTruthy(pop()));
                break;
            }
            case Op::MAKE_TABLE:
            {
                auto fieldc = readByte();

                const size_t values = fieldc * 2;

                Value *start = &stack[sp - values];
                Value *top = &stack[sp];
                sp -= values;

                auto luaTable = std::make_shared<LuaTable>();

                for (; start < top; start += 2)
                {
                    luaTable->set(*this, *start, *(start + 1));
                }

                push(luaTable);
                break;
            }
            case Op::SET_FIELD:
            {
                Value table = pop();
                Value field = pop();
                Value value = pop();

                luaTableSet(table, field, value);
                break;
            }
            case Op::GET_FIELD: case Op::GET_METHOD:
            {
                Value table = pop();
                Value field = pop();

                size_t calleeIndex = push(luaTableGet(table, field));
                if (op == Op::GET_METHOD) {
                    callees.emplace_back(calleeIndex);
                    push(table); // Push self
                }
                break;
            }
            case Op::SET_LIST:
            {
                auto index = static_cast<double>(readByte());
                assert(!tables.empty());
                size_t tableIndex = tables.back(); tables.pop_back();
                
                size_t listStart = tableIndex + 1;
                Value &value = stack[tableIndex];
                Value *start = &stack[listStart];
                Value *top = &stack[sp];

                auto &table = std::get<LuaTableHandle>(value);

                size_t count = sp - listStart;
                sp = listStart;
               
                table->reserve(count);

                while (start < top)
                {
                    table->set(*this, index++, *start++);
                }
                
                break;
            }
            case Op::STORE_TABLE:
            {
                tables.emplace_back(sp - 1);
                break;
            }
            case Op::MAKE_CLOSURE:
            {
                auto function = std::get<FunctionHandle>(readConstant());
                auto closure = std::make_shared<Closure>(Closure{function});
                push(closure);

                auto frame = callFrames.back();
                for (size_t i = 0; i < closure->upvalues.size(); i++)
                {
                    auto isLocal = readByte();
                    auto index = readByte();

                    if (isLocal) 
                    {
                        closure->upvalues[i] = captureUpValue(&stack[frame.frameBase + index]);
                    } 
                    else 
                    {
                        closure->upvalues[i] = frame.closure->upvalues[index];
                    }
                }
                break;
            }
            case Op::STORE_CALLEE:
            {
                callees.emplace_back(sp - 1);
                break;
            }
            case Op::CALL:
            {
                [[maybe_unused]] auto argc = readByte();
                auto expectedReturn = readSignedByte();

                assert(!callees.empty());
                size_t calleeIndex = callees.back(); callees.pop_back();

                callValue(calleeIndex, expectedReturn, CallFrame::CallType::LUA);
                break;
            }
            case Op::TAIL_CALL:
            {
                [[maybe_unused]] auto argc = readByte();
                [[maybe_unused]] auto expectedReturn = readSignedByte();

                size_t calleeIndex = callees.back(); callees.pop_back();
                size_t frameBase = callFrames.back().frameBase;
                int expected = callFrames.back().expectedReturn;

                closeUpValues(&stack[frameBase]); 
                callFrames.pop_back();

                assert(calleeIndex < sp);
                assert(frameBase < calleeIndex);
                Value *start = &stack[calleeIndex];
                Value *top = &stack[sp];

                sp = frameBase;

                while (start < top)
                {
                    push(std::move(*start++));
                } 
                
                callValue(frameBase, expected, CallFrame::CallType::LUA);

                if (callFrames.empty()) return;
                break;
            }
            case Op::JUMP_IF_FALSE:
            {
                auto arg = readShort();
                if (!isTruthy(pop()))
                {
                    doJump(arg);
                }
                break;
            }
            case Op::JUMP_IF_TRUE:
            {
                auto arg = readShort();
                if (isTruthy(pop()))
                {
                    doJump(arg);
                }
                break;
            }
            case Op::JUMP:
            {
                auto arg = readShort();
                doJump(arg);
                break;
            }
            case Op::FOR_PREP:
            {
                auto arg = readShort();

                Value &variable = stack[sp - 4];
                Value &index = stack[sp - 3];
                Value &stop = stack[sp - 2];
                Value &step = stack[sp - 1];

                std::optional<double> nIndex, nStop, nStep;
                if (!(nIndex = ValueHelper::toNumber(index))) runtimeError("Initial value required to be a number!");
                if (!(nStop = ValueHelper::toNumber(stop))) runtimeError("Stop required to be a number!");
                if (!(nStep = ValueHelper::toNumber(step))) runtimeError("Step required to be a number!");

                variable = index = *nIndex;
                stop = *nStop;
                step = *nStep;

                if (*nStep > 0 ? 
                *nIndex > *nStop : 
                *nIndex < *nStop)
                {
                    doJump(arg);
                }
                break;
            }
            case Op::FOR_LOOP:
            {
                auto arg = readShort();

                Value &variable = stack[sp - 4];
                auto &nIndex = std::get<double>(stack[sp - 3]);
                auto &nStop = std::get<double>(stack[sp - 2]);
                auto &nStep = std::get<double>(stack[sp - 1]);

                nIndex += nStep;

                if (nStep > 0 ? 
                nIndex <= nStop : 
                nIndex >= nStop)
                {
                    doJump(-arg);
                    variable = nIndex;
                }
                break;
            }
            case Op::LOOP:
            {
                auto arg = readShort();
                doJump(-arg);
                break;
            }
            case Op::CLOSE_UPVALUE:
            {
                closeUpValues(&stack[sp - 1]);
                pop();
                break;
            }
            case Op::RETURN:
            {
                auto numOfLocals = readByte();

                auto &frame = callFrames.back();
                size_t frameBase = frame.frameBase;
                int expectedReturn = frame.expectedReturn;
                CallFrame::CallType callType = frame.callType;

                closeUpValues(&stack[frameBase]);
                callFrames.pop_back();

                if (callFrames.empty()) return;  

                moveReturns(frameBase + numOfLocals, frameBase, expectedReturn);
  
                if (callType == CallFrame::CallType::CPP) return; // Allows for recursive entry of the run() function  
                break;
            }
            case Op::VARARG:
            {       
                auto expected = readSignedByte();
               
                if (expected == RETURN_ALL)
                {
                    for (const Value &value : callFrames.back().varArg)
                    {
                        push(value);
                    }
                }
                else 
                {
                    size_t available = callFrames.back().varArg.size();

                    for (size_t i = 0; i < static_cast<size_t>(expected); i++)
                    {
                        push((i < available) ? callFrames.back().varArg[i] : LUA_NIL_VALUE);
                    }
                }
                
                break;
            }
            default:
                return;
        }
    }
}

bool VM::isTruthy(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const bool value) -> bool { return value; },
        [](const LUA_NIL_TYPE) -> bool { return false; },
        [](const auto&) -> bool { return true; }
    }, value);
}

double VM::length(const Value &value)
{
    return std::visit(overloaded 
    {
        [this](const LuaTableHandle &table) -> double 
        {
            if (tryMetaMethod({table}, MetaMethod::LEN, CallFrame::CallType::CPP))
            {
                std::optional<double> value = ValueHelper::toNumber(pop());
                if (!value) runtimeError("__len must return a number value!");
                return *value;
            }

            return table->length(*this);
        },
        [](const std::string &string) -> double { return static_cast<double>(string.length()); },
        [this](const auto &value) -> double 
        {
            runtimeError(std::format("Attempt to get length of {} value!", type(value)));
        }
    }, value);
}

std::string VM::type(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const double) -> std::string { return "number"; },
        [](const std::string &) -> std::string { return "string"; },
        [](const bool) -> std::string { return "boolean"; },
        [](const NativeFunctionHandle &) -> std::string { return "function"; },
        [](const ClosureHandle &) -> std::string { return "function"; },
        [](const LUA_NIL_TYPE) -> std::string { return "nil"; },
        [](const LuaTableHandle &) -> std::string { return "table"; },
        [this](const auto &) -> std::string 
        {
            runtimeError("Attempt to get unknown type");
        }
    }, value);
}

namespace 
{
    template <typename T>
    std::string pointerToString(const std::shared_ptr<T> &pointer)
    {
        return std::format("{:p}", static_cast<const void*>(pointer.get()));
    }
}

std::string VM::toString(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const double a) -> std::string { return std::format("{:.14g}", a); },
        [](const std::string &a) -> std::string  { return a; },
        [](const bool a) -> std::string { return (a) ? "true" : "false"; },
        [](const NativeFunctionHandle &native) -> std::string { return "function: " + pointerToString(native);},
        [](const ClosureHandle &closure) -> std::string { return "function: " + pointerToString(closure);},
        [](const LUA_NIL_TYPE) -> std::string { return "nil"; },
        [this](const LuaTableHandle &table) -> std::string 
        {
            if (tryMetaMethod({table}, MetaMethod::TOSTRING, CallFrame::CallType::CPP))
            {
                std::optional<std::string> value = ValueHelper::toString(pop());
                if (!value) runtimeError("__tostring must return a string value!");
                return *value;
            }

            return "table: " + pointerToString(table);
        },
        [this](const auto&) -> std::string {
            runtimeError("Attempt to convert unexpected value to string!");
        }
    }, value);
}