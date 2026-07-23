#include "ast.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

std::string PrintExprVisitor::operator()(const NumberLiteralExpr &node) const { return std::to_string(node.value); }

std::string PrintExprVisitor::operator()(const StringLiteralExpr &node) const { return '"' + node.value + '"'; }

std::string PrintExprVisitor::operator()(const BoolLiteralExpr &node) const { return (node.value) ? "true" : "false"; }

std::string PrintExprVisitor::operator()(const NilExpr &) const { return "nil"; }

std::string PrintExprVisitor::operator()(const VarArgExpr &) const { return "..."; }

std::string PrintExprVisitor::operator()(const CallExpr &node) const
{
    std::string result;

    result += visit(node.callee) + "(";

    for (size_t i = 0; i < node.args.size(); i++)
    {
        result += visit(node.args[i]);
        if (i < node.args.size() - 1)
        {
            result += ", ";
        }
    }
    result += ')';

    return result;
}

std::string PrintExprVisitor::operator()(const IndexExpr &node) const
{
    std::string result;
    result += visit(node.expr);
    result += "[" + visit(node.index) + ']';
    return result;
}

std::string PrintExprVisitor::operator()(const FieldAccessExpr &node) const
{
    std::string result;
    result += visit(node.expr);
    result += "." + node.field;
    return result;
}

std::string PrintExprVisitor::operator()(const MethodAccessExpr &node) const
{
    std::string result;

    result += visit(node.object) + "." + node.field + "(";

    result += visit(node.object);

    if (!node.args.empty()) result += ", ";

    for (size_t i = 0; i < node.args.size(); i++)
    {
        result += visit(node.args[i]);
        if (i < node.args.size() - 1)
        {
            result += ", ";
        }
    }
    result += ')';

    return result;
}

std::string PrintExprVisitor::operator()(const FunctionExpr &node) const
{
    PrintStmtVisitor visitor{indentLevel};

    std::string result = "function(";
    for (size_t i = 0; i < node.args.size(); i++)
    {
        result += node.args[i];
        if (i < node.args.size() - 1)
        {
            result += ", ";
        }
    }
    result += ")\n";

    visitor.indentLevel++;
    for (size_t i = 0; i < node.body.size(); i++)
    {
        result += visitor.visitStmt(node.body[i].stmt) + "\n";
    }
    visitor.indentLevel--;

    result += visitor.addIndentation();
    result += "end";

    return result;
}

std::string PrintExprVisitor::operator()(const VariableExpr &node) const { return node.ident; }

std::string PrintExprVisitor::operator()(const UnaryExpr &node) const { return unaryOpToString(node.op) + "(" + visit(node.expr) + ")"; }

std::string PrintExprVisitor::unaryOpToString(UnaryExpr::UnaryOperator op)
{
    switch (op)
    {
        case UnaryExpr::UnaryOperator::NEGATE: return "-";
        case UnaryExpr::UnaryOperator::LENGTH: return "#";
        case UnaryExpr::UnaryOperator::BIT_NOT: return "~";
        case UnaryExpr::UnaryOperator::NOT: return "not";
        default: 
            throw std::runtime_error("Unexpected unary operator!");
    }
}

std::string PrintExprVisitor::binaryOpToString(BinaryExpr::BinaryOperator op)
{
    switch (op)
    {
        case BinaryExpr::BinaryOperator::ADD: return "+";
        case BinaryExpr::BinaryOperator::SUB: return "-";
        case BinaryExpr::BinaryOperator::DIV: return "/";
        case BinaryExpr::BinaryOperator::FLOOR_DIV: return "//";
        case BinaryExpr::BinaryOperator::MUL: return "*";
        case BinaryExpr::BinaryOperator::MOD: return "%";
        case BinaryExpr::BinaryOperator::EXPO: return "^";
        case BinaryExpr::BinaryOperator::CONCAT: return "..";
        
        case BinaryExpr::BinaryOperator::AND: return "and";
        case BinaryExpr::BinaryOperator::OR: return "or";

        case BinaryExpr::BinaryOperator::BIT_AND: return "&";
        case BinaryExpr::BinaryOperator::BIT_OR: return "|";
        case BinaryExpr::BinaryOperator::BIT_XOR: return "~";
        case BinaryExpr::BinaryOperator::BITSHIFT_LEFT: return "<<";
        case BinaryExpr::BinaryOperator::BITSHIFT_RIGHT: return ">>";

        case BinaryExpr::BinaryOperator::EQ: return "==";
        case BinaryExpr::BinaryOperator::NEQ: return "~=";
        case BinaryExpr::BinaryOperator::LS: return "<";
        case BinaryExpr::BinaryOperator::LSE: return "<=";
        case BinaryExpr::BinaryOperator::GT: return ">";
        case BinaryExpr::BinaryOperator::GTE: return ">=";

        default: 
            throw std::runtime_error("Unexpected binary operator!");
    }
}

std::string PrintExprVisitor::operator()(const BinaryExpr &node) const { return "(" + visit(node.lhs) + " " + binaryOpToString(node.op) + " " + visit(node.rhs) + ")"; }

std::string PrintExprVisitor::visitTableExpr(const TableExpr &tableExpr) const
{
    std::string result;

    switch (tableExpr.kind) 
    {
        case TableExpr::Kind::List:
            result += visit(tableExpr.value); break;
        case TableExpr::Kind::Record:
            result += visit(tableExpr.name);
            result += " = ";
            result += visit(tableExpr.value); break;
        case TableExpr::Kind::General:
            result += "[" + visit(tableExpr.name) + "]";
            result += " = ";
            result += visit(tableExpr.value); break;
    }

    return result;
}

std::string PrintExprVisitor::operator()(const TableExprDef &node) const 
{

    std::string result;
    result += "{";

    for (size_t i = 0; i < node.fields.size(); i++)
    {
        result += visitTableExpr(node.fields[i]);
        if (i < node.fields.size() - 1) 
        {
            result += ", ";
        }
    }

    result += "}";

    return result;
}

[[nodiscard]] std::string PrintExprVisitor::visit(const ExprHandle &node) const 
{
    return std::visit(*this, *node);
}

std::string PrintStmtVisitor::operator()(const WhileStmt &node)
{
    PrintExprVisitor visitor{indentLevel};
    std::string result;

    result += addIndentation();
    result += "while ";
    result += visitor.visit(node.condExpr);
    result += " do \n";

    indentLevel++;
    for (size_t i = 0; i < node.whileStmts.size(); i++)
    {
        result += visitStmt(node.whileStmts[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const ForRangeStmt &node)
{
    PrintExprVisitor ExprVisitor{indentLevel};
    std::string result;

    result += addIndentation();
    result += "for " + node.variable + " = ";
    result += ExprVisitor.visit(node.start) + ", ";
    result += ExprVisitor.visit(node.stop);
    if (node.step != nullptr) result += ", " + ExprVisitor.visit(node.step);

    result += " do\n";

    indentLevel++;
    for (size_t i = 0; i < node.forStmts.size(); i++)
    {
        result += visitStmt(node.forStmts[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const ForIteratorStmt &node)
{
    PrintExprVisitor ExprVisitor{indentLevel};
    std::string result;

    result += addIndentation();
    result += "for ";

    for (size_t i = 0; i < node.variables.size(); i++)
    {
        result += node.variables[i];
        if (i < node.variables.size() - 1) 
        {
            result += ", ";
        }
    }

    result += " in ";
    result += ExprVisitor.visit(node.iterator);

    result += " do\n";

    indentLevel++;
    for (size_t i = 0; i < node.forStmts.size(); i++)
    {
        result += visitStmt(node.forStmts[i].stmt) + "\n";
    }

    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const RepeatStmt &node)
{
    PrintExprVisitor visitor{indentLevel};
    std::string result;
    result += addIndentation();
    result += "repeat\n";

    indentLevel++;
    for (size_t i = 0; i < node.repeatStmts.size(); i++) 
    {
        result += "\t" + visitStmt(node.repeatStmts[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "until ";
    result += visitor.visit(node.condExpr);

    return result;
}

std::string PrintStmtVisitor::operator()(const IfStmt &node)
{
    PrintExprVisitor visitor{indentLevel};
    std::string result;

    result += addIndentation();
    result += "if " + visitor.visit(node.condExpr);
    result += " then\n";

    indentLevel++;
    for (size_t i = 0; i < node.ifStmts.size(); i++)
    {
        result += visitStmt(node.ifStmts[i].stmt) + '\n';
    }
    indentLevel--;

    if (!node.elseStmts.empty())
    {
        result += addIndentation();
        result += "else\n";

        indentLevel++;
        for (size_t i = 0; i < node.elseStmts.size(); i++)
        {
            result += visitStmt(node.elseStmts[i].stmt) += "\n";
        }
        indentLevel--;
    }

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const LocalAssignmentStmt &node)
{
    PrintExprVisitor visitor{indentLevel};

    std::string result;
    result += addIndentation();
    result += "local ";

    for (size_t i = 0; i < node.ident.size(); i++)
    {
        result += node.ident[i];
        if (i < node.ident.size() - 1)
        {
            result += ", ";
        }
    }

    if (!node.value.empty()) result += " = ";

    for (size_t i = 0; i < node.value.size(); i++)
    {
        result += visitor.visit(node.value[i]);
        if (i < node.value.size() - 1)
        {
            result += ", ";
        }
    }

    return result;
}

std::string PrintStmtVisitor::operator()(const AssignmentStmt &node)
{
    PrintExprVisitor visitor{indentLevel};

    std::string result;
    result += addIndentation();
    for (size_t i = 0; i < node.target.size(); i++)
    {
        result += visitor.visit(node.target[i]);
        if (i < node.target.size() - 1)
        {
            result += ", ";
        }
    }

    if (!node.value.empty()) result += " = ";

    for (size_t i = 0; i < node.value.size(); i++)
    {
        result += visitor.visit(node.value[i]);
        if (i < node.value.size() - 1)
        {
            result += ", ";
        }
    }

    return result;
}

std::string PrintStmtVisitor::operator()(const LocalFunctionAssignmentStmt &node)
{
    std::string result;
    result += addIndentation();
    result += "local function " + node.name + "(";

    for (size_t i = 0; i < node.args.size(); i++)
    {
        result += node.args[i];
        if (i < node.args.size() - 1)
        {
            result += ", ";
        }
    }
    result += ")\n";

    indentLevel++;
    for (size_t i = 0; i < node.body.size(); i++)
    {
        result += visitStmt(node.body[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const FunctionAssignmentStmt &node)
{
    PrintExprVisitor ExprVisitor{indentLevel};

    std::string result;
    result += addIndentation();
    result += "function " + ExprVisitor.visit(node.name) + "(";
    for (size_t i = 0; i < node.args.size(); i++)
    {
        result += node.args[i];
        if (i < node.args.size() - 1)
        {
            result += ", ";
        }
    }
    result += ")\n";

    indentLevel++;
    for (size_t i = 0; i < node.body.size(); i++)
    {
        result += visitStmt(node.body[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

std::string PrintStmtVisitor::operator()(const ExprStmt &node)
{
    PrintExprVisitor visitor{indentLevel};
    return addIndentation() + visitor.visit(node.expr);
}

std::string PrintStmtVisitor::operator()(const ReturnStmt &node)
{
    PrintExprVisitor visitor{indentLevel};
    
    std::string result;
    result += addIndentation();
    result += "return ";

    for (size_t i = 0; i < node.values.size(); i++)
    {
        result += visitor.visit(node.values[i]);
        if (i < node.values.size() - 1)
        {
            result += ", ";
        }
    }

    return result;
}

std::string PrintStmtVisitor::operator()(const BreakStmt &) {return addIndentation() + "break"; }

std::string PrintStmtVisitor::operator()(const GoToStmt &node)
{
    return addIndentation() + "goto " + node.label;
}

std::string PrintStmtVisitor::operator()(const LabelStmt &node)
{
    return addIndentation() + "::" + node.label + "::";
}

std::string PrintStmtVisitor::operator()(const BlockStmt &node)
{
    std::string result;
    result += addIndentation();
    result += "do \n";

    indentLevel++;
    for (size_t i = 0; i < node.stmt.size(); i++)
    {
        result += visitStmt(node.stmt[i].stmt) + "\n";
    }
    indentLevel--;

    result += addIndentation();
    result += "end";

    return result;
}

[[nodiscard]] std::string PrintStmtVisitor::visitStmt(const StatementHandle &node)
{
    return std::visit(*this, *node);
}

[[nodiscard]] std::string PrintStmtVisitor::visitStmts(const std::vector<StmtWithPos> &nodes)
{
    std::string ss;

    for (const StmtWithPos &stmtWithPos : nodes)
    {
        ss += std::format("[line {}] ", stmtWithPos.line);
        ss += std::visit(*this, *stmtWithPos.stmt) + "\n";
    }

    return ss;
}