#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <format>
#include <unordered_map>

#include "value.h"
#include "bytecode.h"
#include "ast.h"

struct Local 
{
    std::string name;
    int depth;
    bool isCaptured;
};

struct Upvalue 
{
    uint8_t index;
    bool isLocal;
};

struct LoopContext 
{
    LoopContext(std::vector<size_t> breaks, int loopDepth)
      : breaks(std::move(breaks)), loopDepth(loopDepth) {}

    std::vector<size_t> breaks;
    int loopDepth;
};

struct LabelContext 
{
    std::optional<std::vector<Local>> locals;
    std::string name;
    size_t pos;
    size_t currentLocals;
    int currentScope;
};

class Compiler;

class ExprCompiler 
{
    public:
        ExprCompiler(int expectedReturn, bool isTailCall, Compiler &context) 
        : context(context)
        , expectedReturn(expectedReturn)
        , isTailCall(isTailCall) 
        {
            assert(expectedReturn < INT8_MAX);
        }

        void operator()(const NumberLiteralExpr &node);
        void operator()(const StringLiteralExpr &node);
        void operator()(const BoolLiteralExpr &node);
        void operator()(const NilExpr &node);
        void operator()(const VarArgExpr &node);

        void compileTableExpr(const TableExpr &node, double &arrayIndex);
        void operator()(const TableExprDef &node);

        void compileArgs(const std::vector<ExprHandle> &args);
        void operator()(const CallExpr &node);
        void operator()(const MethodAccessExpr &node);

        void operator()(const FieldAccessExpr &node);
        void operator()(const IndexExpr &node);
        void operator()(const FunctionExpr &node);
        void operator()(const VariableExpr &node);
        void operator()(const UnaryExpr &node);

        void compileLogicalOp(Op op, const ExprHandle &lhs, const ExprHandle &rhs);
        void operator()(const BinaryExpr &node);

        void compileExpression(const ExprHandle &expression, int expectedReturn);
    private:
        Compiler &context;
        const int expectedReturn;
        const bool isTailCall;
};

class Compiler 
{
    public:
        Compiler(Compiler *enclosing, std::string_view name, int arity, bool isVarArg)
        : function(std::make_shared<FunctionChunk>(FunctionChunk{{}, std::string(name), 0, arity, isVarArg}))
        , chunk(function->chunk)
        , enclosing(enclosing)
        , scopeDepth(0)
        , currentLine(1)
        {
            addLocal(""); // Reserved for the callee
        }

        // --------------------
        // Compiling Stmts
        // --------------------
        void operator()(const WhileStmt &node);
        void operator()(const ForRangeStmt &node);
        void operator()(const ForIteratorStmt &node);
        void operator()(const RepeatStmt &node);
        void operator()(const IfStmt &node);
        void operator()(const LocalAssignmentStmt &node);
        void operator()(const AssignmentStmt &node);
        void operator()(const LocalFunctionAssignmentStmt &node);
        void operator()(const FunctionAssignmentStmt &node);
        void operator()(const ExprStmt &node);
        void operator()(const ReturnStmt &node);
        void operator()(const BreakStmt &node);
        void operator()(const GoToStmt &node);
        void operator()(const LabelStmt &node);
        void operator()(const BlockStmt &node);

        FunctionHandle compile(const Ast &stmts);
        static Compiler makeTopLevel() { return Compiler(nullptr, "<main>", 0, true); };
    public:
        friend ExprCompiler;
    private:
        static const std::unordered_map<UnaryExpr::UnaryOperator, Op> unaryOp;
        static const std::unordered_map<BinaryExpr::BinaryOperator, Op> binaryOp;

        std::vector<Local> locals;
        std::vector<Upvalue> upvalues;

        std::vector<LoopContext> loopStack;

        std::vector<LabelContext> unresolvedGoto;
        std::vector<LabelContext> labels;
        
        FunctionHandle function;
        Chunk &chunk;
        Compiler *enclosing;

        int scopeDepth;
        int currentLine;
    private:
        // ---------------------
        // Scope Functions
        // ---------------------
        void beginScope();
        void endScope();

        // ---------------------
        // Local Variable Functions
        // ---------------------
        int addLocal(const std::string &name);
        int resolveLocal(const std::string &name);

        // --------------------
        // Upvalue Functions
        // --------------------
        int resolveUpValue(const std::string &name);
        int addUpvalue(uint8_t index, bool isLocal);

        // --------------------
        // Variable Functions
        // --------------------
        void namedVariable(const std::string &name, bool assignment);

        // --------------------
        // Jumping Helper Functions
        // --------------------
        size_t emitJump(Op op);
        void emitLoop(Op op, size_t loopStart);
        void patchJump(size_t offset);
        void patchJumpAt(size_t jumpPos, size_t target);
        void emitReturn() { emitWithArg(Op::RETURN, static_cast<uint8_t>(locals.size())); }

        // --------------------
        // Chunk Functions
        // --------------------
        void emitConstant(const Value &value);
        int makeConstant(Value value);
        void emitWithArg(Op op, uint8_t arg);
        void emitWithArg2(Op op, uint8_t arg, uint8_t arg2);
        void emit(Op op);
        void emit(uint8_t arg);

        // --------------------
        // Compiling Functions
        // --------------------
        FunctionHandle makeFunction();
        void compileFunction(const std::string &name, bool isVarArg, const std::vector<std::string> &args, const std::vector<StmtWithPos> &stmts);

        // --------------------
        // Constant Folding
        // --------------------
        template <typename T>
        static std::optional<bool> tryFoldCompare(BinaryExpr::BinaryOperator op, const T &a, const T &b);

        static std::optional<double> tryFoldArithmetic(BinaryExpr::BinaryOperator op, double a, double b);

        static bool constTruthy(const Value &value);

        static std::optional<Value> tryFoldConstant(const ExprHandle &expression);

        // --------------------
        // Helper Functions
        // --------------------
        void compilerError(std::string_view error) { throw std::runtime_error(std::format("[Line {}] {}!", currentLine, error)); }
                
        void compileExpression(const ExprHandle &expression, int expectedReturn, bool isTailCall);

        void compileStmt(const StatementHandle &stmt)
        {
            std::visit(*this, *stmt);
        }

        void compileStmts(const std::vector<StmtWithPos> &stmts)
        {
            for (const auto &[stmt, line, col] : stmts)
            {
                currentLine = line;
                compileStmt(stmt);
            }
        }

        static bool isCallable(const ExprHandle &expr) { return std::holds_alternative<CallExpr>(*expr) || std::holds_alternative<MethodAccessExpr>(*expr); };

        static bool isMultiReturn(const ExprHandle &expr) { return isCallable(expr) || std::holds_alternative<VarArgExpr>(*expr); }

        void compileAssignment(size_t numOfTargets, const std::vector<ExprHandle> &values);

        void compileBlock(const Ast &stmts);
};

