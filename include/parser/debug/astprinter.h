#include <string>
#include <vector>

#include "parser/ast.h"

class AstPrinter
{
    public:
        static void printStmts(const std::vector<StmtWithPos> &nodes);
    private:
        struct ExprVisitor 
        {
            public:
                ExprVisitor(int indentLevel) : indentLevel(indentLevel) {}
            
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
                StmtVisitor(int indentLevel) : indentLevel(indentLevel) {}

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

                [[nodiscard]] std::string visitStmt(const StatementHandle &node);
                [[nodiscard]] std::string addIndentation() const { return std::string(static_cast<size_t>(indentLevel), '\t'); }
            public:
                int indentLevel;
        };
};
