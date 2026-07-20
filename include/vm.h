#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <initializer_list>
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

#include "stdlib/stdlib.h"

inline constexpr size_t MAX_FRAMES = 128;
inline constexpr size_t STACK_SIZE = UINT8_MAX * MAX_FRAMES;
inline constexpr int RETURN_ALL = -1;

inline constexpr size_t NUM_OF_PRIMITIVE = 1;
inline constexpr size_t STRING = 0;

struct CallVisitor;

class BaseLib;
class TableLib;
class IoLib;
class StringLib;
class StdLib;
class Lua;

enum class CallType : uint8_t
{
    LUA,
    CPP 
};

struct CallFrame
{
    CallFrame() = default;

    CallFrame(std::vector<Value> varArg, ClosureHandle closure, size_t ip,
            size_t frameBase, int expectedReturn, CallType callType)
      : varArg(std::move(varArg)), closure(std::move(closure)), ip(ip),
        frameBase(frameBase), expectedReturn(expectedReturn),
        callType(callType) {}
        
    std::vector<Value> varArg;
    ClosureHandle closure;
    size_t ip;
    size_t frameBase;
    int expectedReturn;
    CallType callType;
};

struct ErrorHandler 
{ 
    size_t sp;
    size_t frame;
    int runDepth;
};

struct ErrHandlerInfo
{
    int line;
    std::string funcName;
};

class VMRuntimeError : public std::exception 
{
    public:
        VMRuntimeError(VM &vm, const Value &object, std::optional<ErrHandlerInfo> posInfo);

        const char* what() const noexcept;
        Value getObj() const noexcept;

    private:
        std::optional<ErrHandlerInfo> posInfo;
        Value value;
        std::string error;
};

class VM
{
    public:
        VM(Lua &lua)
        : opcount(0)
        , globals(std::make_shared<LuaTable>())
        , lua(lua)
        , sp(0)
        , runDepth(0)
        {
            callFrames.reserve(MAX_FRAMES);
            StdLib::initLibraries(*this);
        }

        void execute(const FunctionHandle &code);
    public:
        int opcount;
    public: 
        friend CallVisitor;
        friend VMRuntimeError;
        friend LuaTable;

        friend TableLib; 
        friend IoLib;
        friend StringLib;
        friend BaseLib;
        friend StdLib;     
    private:
        std::array<Value, STACK_SIZE> stack;

        std::vector<CallFrame> callFrames;
        std::vector<ErrorHandler> errorHandlers;

        std::vector<size_t> callees; // Temporary solution to a bigger problem
        std::vector<size_t> tables; // Temporary solution to a bigger problem
  
        std::array<LuaTableHandle, NUM_OF_PRIMITIVE> primitiveMt; // Stores references to meta tables for primites

        LuaTableHandle globals;

        LuaTableHandle loaded; // Stores a reference to the loaded modules table

        UpValueHandle openUpvalues;
         
        Lua &lua;

        size_t sp;

        int runDepth;
    private:
        // -------------------------
        // Arg Helper Functions
        // -----------------------=
        void argError(size_t argIndex, const char *msg)
        {
            runtimeError(std::format("Bad argument #{} expected {}!", argIndex + 1, msg));
        }

        bool validArgIndex(std::span<Value> args, size_t index) { return index < args.size(); }

        Value argCheckAny(std::span<Value> args, size_t argIndex)
        {
            if (!validArgIndex(args, argIndex)) argError(argIndex, "value");

            return args[argIndex];
        }

        Value argOptAny(std::span<Value> args, size_t argIndex, const Value &fallback)
        {
            if (!validArgIndex(args, argIndex)) return fallback;
            return args[argIndex];
        }

        template<typename... Ts>
        void argCheck(std::span<Value> args, size_t argIndex, const char *msg)
        {
            if (!validArgIndex(args, argIndex)) argError(argIndex, msg);
            if (!(std::holds_alternative<Ts>(args[argIndex]) || ...)) 
            {
                argError(argIndex, msg);
            }
        }

        template <typename T>
        T argEnsure(std::span<Value> args, size_t argIndex, const char *msg)
        {
            if (!validArgIndex(args, argIndex)) argError(argIndex, msg);
            if (!std::holds_alternative<T>(args[argIndex])) argError(argIndex, msg);
            return std::get<T>(args[argIndex]);
        }
        
        template <typename T>
        T argOpt(std::span<Value> args, size_t argIndex, T fallback, const char *msg)
        {
            if (!validArgIndex(args, argIndex)) return fallback;
            if (!std::holds_alternative<T>(args[argIndex])) argError(argIndex, msg);
            return std::get<T>(args[argIndex]);
        }

        // -------------------------
        // Call Functions
        // -------------------------
        void nativeCall(const NativeFunctionHandle &nativeFunction, size_t calleeIndex, int expectedReturn);
        void call(const ClosureHandle &closure, size_t calleeIndex, int expectedReturn, CallType type);
        void callValue(size_t calleeIndex, int expectedReturn, CallType type);
        void moveReturns(size_t firstResult, size_t frameBase, int expectedReturn);

        // -------------------------
        // Error Handling Functions
        // -------------------------
        void pushErrorHandler(size_t returnSp);
        void popErrorHandler();
        void recoverVM();
        [[noreturn]] void runtimeError(const Value &error);

        // -------------------------
        // Meta Method Functions
        // -------------------------
        LuaTableHandle getMetaTable(const Value &value);
        std::optional<Value> resolveMetaMethod(const LuaTableHandle &metatable, MetaMethod method);
        std::optional<Value> resolveValueMetaMethod(const Value &value, MetaMethod method);
        bool tryMetaMethod(std::initializer_list<Value> values, MetaMethod method, CallType callType);

        // -------------------------
        // Upvalue Functions
        // -----------------------=
        UpValueHandle captureUpValue(Value *location);
        void closeUpValues(Value *last);

        // -------------------------
        // Binary Operator Functions
        // -------------------------
        template <typename T>
        void handleBinaryError(std::string_view message, const Value &a, const Value &b, const std::optional<T> &lhs, const std::optional<T> &rhs);
        template <typename T>
        void pushCompare(Op op, const T &a, const T &b);
        void handleCompare(Op op, MetaMethod method);
        void handleEquality();
        void handleBitWise(Op op, MetaMethod method);
        void handleConcat();
        void handleArithmetic(Op op, MetaMethod method);

        // ------------------------- 
        // Table Functions
        // -------------------------
        void luaTableSet(const Value &table, const Value &key, const Value &value, int depth = 0);
        Value luaTableGet(const Value &table, const Value &key, int depth = 0);
        bool isCallable(const Value &value) {return std::holds_alternative<NativeFunctionHandle>(value) || std::holds_alternative<ClosureHandle>(value);};

        // -------------------------
        // Byte Traversal Functions
        // -----------------------=
        uint8_t readByte();
        int8_t readSignedByte();
        Value readConstant();
        int16_t readShort();

        // -------------------------
        // Entry Point
        // -------------------------
        void run();

        // -------------------------
        // Value Functions
        // -------------------------
        static bool isTruthy(const Value &value);
        double length(const Value &value);
        std::string type(const Value &value);
        std::string toString(const Value &value);

        // -------------------------
        // Stack Functions
        // -------------------------
        void checkStack(size_t pos, size_t diff)
        {
            if (pos + diff >= STACK_SIZE) runtimeError("Stack overflow!");
        }

        size_t push(Value value)
        {
            if (sp >= STACK_SIZE) runtimeError("Stack overflow!");
            stack[sp] = std::move(value);
            return sp++;
        }

        Value pop()
        {
            assert(sp > 0);
            return std::move(stack[--sp]);
        }

        // -------------------------
        // Jump Function
        // -------------------------
        void doJump(int offset)
        {
            if (offset < 0)
            {
                callFrames.back().ip -= static_cast<size_t>(-offset);
            }
            else 
            {
                callFrames.back().ip += static_cast<size_t>(offset);
            }
        }
};
 