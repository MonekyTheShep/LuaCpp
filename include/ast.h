#pragma once

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

struct StringLiteralExpr;
struct NumberLiteralExpr;
struct BoolLiteralExpr;
struct NilExpr;
struct VarArgExpr;
struct VariableExpr;
struct UnaryExpr;
struct BinaryExpr;
struct TableExprDef;
struct CallExpr;
struct IndexExpr;
struct MethodAccessExpr;
struct FieldAccessExpr;
struct FunctionExpr;

struct LocalAssignmentStmt;
struct LocalFunctionAssignmentStmt;
struct FunctionAssignmentStmt;
struct AssignmentStmt;
struct BlockStmt;
struct ForRangeStmt;
struct ForIteratorStmt;
struct RepeatStmt;
struct ExprStmt;
struct GoToStmt;
struct LabelStmt;
struct ReturnStmt;
struct BreakStmt;
struct WhileStmt;
struct IfStmt;

using Expr = std::variant<StringLiteralExpr, NumberLiteralExpr, BoolLiteralExpr, NilExpr, VarArgExpr, VariableExpr, UnaryExpr, BinaryExpr, TableExprDef, CallExpr, IndexExpr, MethodAccessExpr, FieldAccessExpr, FunctionExpr>;
using ExprHandle = std::unique_ptr<Expr>;

using Statement = std::variant<LocalAssignmentStmt, LocalFunctionAssignmentStmt, FunctionAssignmentStmt, AssignmentStmt, ForRangeStmt, ForIteratorStmt, RepeatStmt, BlockStmt, IfStmt, WhileStmt, ExprStmt, GoToStmt, LabelStmt, ReturnStmt, BreakStmt>;
using StatementHandle = std::unique_ptr<Statement>;

struct StmtWithPos 
{
    StatementHandle stmt;
    int line;
    int col;
};

using Ast = std::vector<StmtWithPos>;

// ----------------------------------------------
// Expression structs
// ----------------------------------------------
struct ErrorExpr {};

struct StringLiteralExpr 
{
    std::string value;
};

struct NumberLiteralExpr 
{
    double value;
};

struct BoolLiteralExpr 
{
    bool value;
};

struct NilExpr {};

struct VarArgExpr {};

struct VariableExpr 
{
    std::string ident;
};

struct FunctionExpr 
{
    bool isVarArg;
    std::vector<std::string> args;
    std::vector<StmtWithPos> body;
};

struct UnaryExpr 
{
    enum class UnaryOperator : uint8_t
    {
        NEGATE,
        LENGTH,
        BIT_NOT,
        NOT
    };

    UnaryOperator op;
    ExprHandle expr;
};

struct BinaryExpr 
{
    enum class BinaryOperator : uint8_t
    {
        ADD,
        SUB,
        DIV,
        FLOOR_DIV,
        MUL,
        MOD,
        EXPO,
        CONCAT,

        AND,
        OR,

        BIT_AND,
        BIT_OR,
        BIT_XOR,
        BITSHIFT_LEFT,
        BITSHIFT_RIGHT,

        EQ,
        NEQ,
        LSE,
        LS,
        GT,
        GTE
    };

    BinaryOperator op;
    ExprHandle lhs;
    ExprHandle rhs;
};

struct TableExpr 
{
    enum class Kind : uint8_t
    {
        List, // expr
        Record, // identifier = expr
        General // [expr] = expr
    };
        
    Kind kind;
    ExprHandle name; // nullptr if list
    ExprHandle value;
};

struct TableExprDef 
{
    std::vector<TableExpr> fields;
};

struct CallExpr 
{
    ExprHandle callee;
    std::vector<ExprHandle> args;
};

struct IndexExpr 
{
    ExprHandle expr;
    ExprHandle index;
};

struct FieldAccessExpr 
{
    ExprHandle expr;
    std::string field;
};

struct MethodAccessExpr 
{
    ExprHandle object;
    std::string field;
    std::vector<ExprHandle> args;
};

// ----------------------------------------------
// Statement structs
// ----------------------------------------------
struct ErrorStmt{};

struct LocalAssignmentStmt 
{
    std::vector<std::string> ident;
    std::vector<ExprHandle> value;
};

struct LocalFunctionAssignmentStmt
{
    std::string name;
    bool isVarArg;
    std::vector<std::string> args;
    std::vector<StmtWithPos> body;
};

struct FunctionAssignmentStmt 
{
    ExprHandle name;
    bool isVarArg;
    std::vector<std::string> args;
    std::vector<StmtWithPos> body;
};

struct AssignmentStmt 
{
    std::vector<ExprHandle> target;
    std::vector<ExprHandle> value;
};

struct BlockStmt 
{
    std::vector<StmtWithPos> stmt;
};

struct ExprStmt 
{
    ExprHandle expr;
};

struct GoToStmt 
{
    std::string label;
};

struct LabelStmt
{
    std::string label;
    bool isLastStmt;
};

struct ReturnStmt 
{
    std::vector<ExprHandle> values;
};

struct BreakStmt {};

struct ForRangeStmt 
{
    std::string variable;
    ExprHandle start;
    ExprHandle stop;
    ExprHandle step; // nullptr defaults to value of 1.0
    std::vector<StmtWithPos> forStmts;
};

struct ForIteratorStmt 
{
    std::vector<std::string> variables;
    ExprHandle iterator;
    std::vector<StmtWithPos> forStmts;
};

struct RepeatStmt 
{
    ExprHandle condExpr;
    std::vector<StmtWithPos> repeatStmts;
};

struct WhileStmt 
{
    ExprHandle condExpr;
    std::vector<StmtWithPos> whileStmts;
};

struct IfStmt 
{
    ExprHandle condExpr;
    std::vector<StmtWithPos> ifStmts;
    std::vector<StmtWithPos> elseStmts;
};

// ----------------------------------------------
// Creation Functions
// ----------------------------------------------
inline StatementHandle makeStmt(Statement &&node)
{
    return std::make_unique<Statement>(std::move(node));
}

inline ExprHandle makeExpr(Expr &&node)
{
    return std::make_unique<Expr>(std::move(node));
}

// ----------------------------------------------
// Debug Printing Functions
// ----------------------------------------------
class AstPrinter
{
    public:
        static void printStmts(const std::vector<StmtWithPos> &nodes);
    private:
        struct ExprVisitor 
        {
            public:
                explicit ExprVisitor(int indentLevel) : indentLevel(indentLevel) {}
            
                std::string operator()(const NumberLiteralExpr &node) const;
                std::string operator()(const StringLiteralExpr &node) const;
                std::string operator()(const BoolLiteralExpr &node) const;
                std::string operator()(const NilExpr &node) const;
                std::string operator()(const VarArgExpr &node) const;
                std::string operator()(const TableExprDef &node) const;
                std::string operator()(const CallExpr &node) const;
                std::string operator()(const MethodAccessExpr &node) const;
                std::string operator()(const FieldAccessExpr &node) const;
                std::string operator()(const IndexExpr &node) const;
                std::string operator()(const FunctionExpr &node) const;
                std::string operator()(const VariableExpr &node) const;
                std::string operator()(const UnaryExpr &node) const;
                std::string operator()(const BinaryExpr &node) const;

                std::string visitTableExpr(const TableExpr &tableExpr) const;
                static std::string unaryOpToString(UnaryExpr::UnaryOperator op);
                static std::string binaryOpToString(BinaryExpr::BinaryOperator op);

                [[nodiscard]] std::string visit(const ExprHandle &node) const;
            public:
                int indentLevel;
        };

        struct StmtVisitor
        {
            public:
                explicit StmtVisitor(int indentLevel) : indentLevel(indentLevel) {}

                [[nodiscard]] std::string visitStmt(const StatementHandle &node);
                [[nodiscard]] std::string addIndentation() const { return std::string(static_cast<size_t>(indentLevel), '\t'); }

                std::string operator()(const WhileStmt &node);
                std::string operator()(const ForRangeStmt &node);
                std::string operator()(const ForIteratorStmt &node);
                std::string operator()(const RepeatStmt &node);
                std::string operator()(const IfStmt &node);
                std::string operator()(const LocalAssignmentStmt &node);
                std::string operator()(const AssignmentStmt &node);
                std::string operator()(const LocalFunctionAssignmentStmt &node);
                std::string operator()(const FunctionAssignmentStmt &node);
                std::string operator()(const ExprStmt &node);
                std::string operator()(const ReturnStmt &node);
                std::string operator()(const BreakStmt &node);
                std::string operator()(const GoToStmt &node);
                std::string operator()(const LabelStmt &node);
                std::string operator()(const BlockStmt &node);

            public:
                int indentLevel;
        };
};
