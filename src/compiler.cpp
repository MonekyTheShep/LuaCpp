#include "compiler.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "bytecode.h"
#include "value.h"
#include "ast.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void Compiler::beginScope() { scopeDepth++; }

void Compiler::endScope()
{
    scopeDepth--;

    while (!locals.empty() && locals.back().depth > scopeDepth)
    {
        emit(locals.back().isCaptured ? ByteCode::Op::CLOSE_UPVALUE : ByteCode::Op::POP);
        locals.pop_back();
    }

    while (!labels.empty() && labels.back().currentScope > scopeDepth)
    {
        labels.pop_back();
    }

    for (size_t i = unresolvedGoto.size(); i-- > 0 && unresolvedGoto[i].currentScope > scopeDepth;) 
    {
        unresolvedGoto[i].currentLocals = locals.size();
        unresolvedGoto[i].currentScope = scopeDepth;
    }
}

int Compiler::addLocal(const std::string &name)
{ 
    locals.emplace_back(name, scopeDepth, false); 
    if (locals.size() > UINT8_MAX) compilerError("Can only have 255 locals in function");
    return static_cast<int>(locals.size() - 1);
}
 
int Compiler::resolveLocal(const std::string& name)
{
    for (auto i = locals.size(); i-- > 0;)
    {
        if (name == locals[i].name)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int Compiler::resolveUpValue(const std::string &name)
{
    if (enclosing == nullptr) return -1;

    int local = enclosing->resolveLocal(name);
    if (local != -1)
    {
        enclosing->locals[static_cast<size_t>(local)].isCaptured = true;
        return addUpvalue(static_cast<uint8_t>(local), true);
    }

    int upvalue = enclosing->resolveUpValue(name);
    if (upvalue != -1) 
    {
        return addUpvalue(static_cast<uint8_t>(upvalue), false);
    }

    return -1;
}

int Compiler::addUpvalue(uint8_t index, bool isLocal)
{
    for (size_t i = 0; i < upvalues.size(); i++) 
    {
        if (upvalues[i].index == index && upvalues[i].isLocal == isLocal)
        {
            return static_cast<int>(i);
        }
    }

    if (upvalues.size() == UINT8_MAX) 
    {
        compilerError("Too many closure variables in function.");
        return -1;
    }

    upvalues.emplace_back(index, isLocal);
    return static_cast<int>(upvalues.size()) - 1;
}

void Compiler::namedVariable(const std::string &name, bool assignment)
{
    ByteCode::Op storeOp, loadOp;

    int arg = resolveLocal(name);

    if (arg != -1)
    {
        storeOp = ByteCode::Op::STORE_LOCAL;
        loadOp = ByteCode::Op::LOAD_LOCAL;
    }
    else if ((arg = resolveUpValue(name)) != -1)
    {
        storeOp = ByteCode::Op::STORE_UPVALUE;
        loadOp = ByteCode::Op::LOAD_UPVALUE;
    }
    else
    {
        storeOp = ByteCode::Op::STORE_GLOBAL;
        loadOp = ByteCode::Op::LOAD_GLOBAL;
        arg = makeConstant(name);
    }

    emitWithArg((assignment) ? storeOp : loadOp, static_cast<uint8_t>(arg));
}

size_t Compiler::emitJump(ByteCode::Op op) 
{
    emit(op);
    emit(0XFF);
    emit(0XFF);
    return chunk.code.size() - 2;
}

void Compiler::emitLoop(ByteCode::Op op, size_t loopStart) 
{
    emit(op);

    size_t offset = chunk.code.size() - loopStart + 2;
    if (offset > INT16_MAX) compilerError("Loop body is too large");

    emit(static_cast<uint8_t>(offset >> 8));
    emit(static_cast<uint8_t>(offset));
}

void Compiler::patchJump(size_t offset) 
{
    size_t jump = chunk.code.size() - offset - 2;

    if (jump > INT16_MAX) compilerError("Too much code to jump over");

    chunk.code[offset] = static_cast<uint8_t>(jump >> 8);
    chunk.code[offset + 1] = static_cast<uint8_t>(jump);
}

void Compiler::patchJumpAt(size_t jumpPos, size_t target) 
{
	int jump = static_cast<int>(target) - static_cast<int>(jumpPos) - 2;

    if (jump > INT16_MAX || jump < INT16_MIN) 
        compilerError("Too much code to jump over");

    chunk.code[jumpPos] = static_cast<uint8_t>(jump >> 8);
	chunk.code[jumpPos + 1] = static_cast<uint8_t>(jump);
}

void Compiler::emitConstant(const Value &value)
{
    emitWithArg(ByteCode::Op::LOAD_CONST, static_cast<uint8_t>(makeConstant(value)));
}

int Compiler::makeConstant(Value value) 
{
    std::optional<int> constant = chunk.makeConstant(std::move(value));
    if (!constant) compilerError("Too many constants in function");
    return *constant;
}

void Compiler::emitWithArg(ByteCode::Op op, uint8_t arg)
{
    emit(op);
    emit(arg);
}

void Compiler::emitWithArg2(ByteCode::Op op, uint8_t arg, uint8_t arg2)
{
    emit(op);
    emit(arg);
    emit(arg2);
}

void Compiler::emit(ByteCode::Op op)
{
    chunk.write(op, currentLine);
}

void Compiler::emit(uint8_t arg)
{
    chunk.write(arg, currentLine);
}

void Compiler::ExprVisitor::operator()(const NumberLiteralExpr &node)
{
    compiler.emitConstant(node.value);
}

void Compiler::ExprVisitor::operator()(const StringLiteralExpr &node)
{
    compiler.emitConstant(node.value);
}

void Compiler::ExprVisitor::operator()(const BoolLiteralExpr &node)
{
    compiler.emit(node.value ? ByteCode::Op::LOAD_TRUE : ByteCode::Op::LOAD_FALSE);
}

void Compiler::ExprVisitor::operator()(const NilExpr &)
{
    compiler.emit(ByteCode::Op::LOAD_NULL);
}

void Compiler::ExprVisitor::operator()(const VarArgExpr &)
{
    if (!compiler.function->isVarArg) compiler.compilerError("Can't use `...` outside a vararg function");
    compiler.emitWithArg(ByteCode::Op::VARARG, static_cast<uint8_t>(expectedReturn));
}

void Compiler::ExprVisitor::compileTableExpr(const TableExpr &node, double &arrayIndex)
{
    switch (node.kind) 
    {
        case TableExpr::Kind::List:
        {
            compiler.emitConstant(arrayIndex++);
            compileExpression(node.value, 1);
            break;
        }
        case TableExpr::Kind::Record: case TableExpr::Kind::General:
        {
            compileExpression(node.name, 1);
            compileExpression(node.value, 1);
            break;
        }    
    }
}

void Compiler::ExprVisitor::operator()(const TableExprDef &node)
{
    if (node.fields.size() > UINT8_MAX) compiler.compilerError("Can't have more than 255 fields in table constructor");

    if (node.fields.empty()) return compiler.emitWithArg(ByteCode::Op::MAKE_TABLE, 0); // No point in compiling fields

    uint8_t fieldsMinusOne = static_cast<uint8_t>(node.fields.size()) - 1;
    double arrayIndex = 1.0;
    for (size_t i = 0; i < fieldsMinusOne; i++)
    {
        compileTableExpr(node.fields[i], arrayIndex);
    }
    
    const auto &lastField = node.fields.back();

    const bool expandable = Compiler::isMultiReturn(lastField.value) 
    && (lastField.kind == TableExpr::Kind::List);

    if (expandable)
    {
        compiler.emitWithArg(ByteCode::Op::MAKE_TABLE,fieldsMinusOne); 
        compiler.emit(ByteCode::Op::STORE_TABLE);
        compileExpression(lastField.value, -1);
        if (arrayIndex > UINT8_MAX) compiler.compilerError("Constructor too long!");
        compiler.emitWithArg(ByteCode::Op::SET_LIST, static_cast<uint8_t>(arrayIndex));
    }
    else 
    {
        compileTableExpr(lastField, arrayIndex);
        compiler.emitWithArg(ByteCode::Op::MAKE_TABLE,++fieldsMinusOne); 
    }
}

void Compiler::ExprVisitor::compileArgs(const std::vector<ExprHandle> &args) 
{
    for (size_t i = 0; i + 1 < args.size(); i++) 
    {
        compileExpression(args[i], 1);
    }

    if (!args.empty())
    {
        compileExpression(args.back(), -1);
    }
}

void Compiler::ExprVisitor::operator()(const CallExpr &node)
{
    compileExpression(node.callee, 1);
    compiler.emit(ByteCode::Op::STORE_CALLEE);
            
    if (node.args.size() > UINT8_MAX) compiler.compilerError("Can't have more than 255 arguments inside of function call");
    compileArgs(node.args);
   
    compiler.emitWithArg2 (
        isTailCall ? ByteCode::Op::TAIL_CALL : ByteCode::Op::CALL, 
        static_cast<uint8_t>(node.args.size()), 
        static_cast<uint8_t>(expectedReturn)
    );
}

void Compiler::ExprVisitor::operator()(const MethodAccessExpr &node) 
{
    compiler.emitConstant(node.field);
    compileExpression(node.object, 1);
    compiler.emit(ByteCode::Op::GET_METHOD);

    if (node.args.size() > UINT8_MAX) compiler.compilerError("Can't have more than 255 arguments inside of method call");
    compileArgs(node.args);
   
    compiler.emitWithArg2 (
        isTailCall ? ByteCode::Op::TAIL_CALL : ByteCode::Op::CALL, 
        static_cast<uint8_t>(node.args.size()), 
        static_cast<uint8_t>(expectedReturn)
    );
}

void Compiler::ExprVisitor::operator()(const FieldAccessExpr &node)
{
    compiler.emitConstant(node.field);
    compileExpression(node.expr, 1);

    compiler.emit(ByteCode::Op::GET_FIELD);
}

void Compiler::ExprVisitor::operator()(const IndexExpr &node)
{
    compileExpression(node.index, 1);
    compileExpression(node.expr, 1);
    
    compiler.emit(ByteCode::Op::GET_FIELD);
}

void Compiler::ExprVisitor::operator()(const FunctionExpr &node)
{
    compiler.compileFunction("<anonymous>", node.isVarArg, node.args, node.body);
}

FunctionHandle Compiler::makeFunction()
{
    if (!unresolvedGoto.empty()) compilerError(std::format("No visible label for goto `{}`", unresolvedGoto.back().name));

    function->upValueCount = static_cast<uint8_t>(upvalues.size());
    return function;
}

void Compiler::compileFunction(const std::string &name, bool isVarArg, const std::vector<std::string> &args, const std::vector<StmtWithPos> &stmts) 
{
    Compiler inner(this, name, static_cast<int>(args.size()), isVarArg);
    
    if (args.size() > UINT8_MAX) compilerError("Can't have more than 255 arguments in function");

    for (const std::string& arg : args)
    {
        inner.addLocal(arg); // pre allocate first slots for arg
    }

    inner.compileStmts(stmts);
    
    inner.emitReturn();

    FunctionHandle function = inner.makeFunction();
    emitWithArg(ByteCode::Op::MAKE_CLOSURE, static_cast<uint8_t>(makeConstant(function)));

    for (size_t i = 0; i < function->upValueCount; i++) 
    {
        emit(inner.upvalues[i].isLocal ? 1 : 0);
        emit(inner.upvalues[i].index);
    }
}

template <typename T>
std::optional<bool> Compiler::tryFoldCompare(BinaryExpr::BinaryOperator op, const T &a, const T &b)
{
    switch (op)
    {
        case BinaryExpr::BinaryOperator::EQ: return a == b;
        case BinaryExpr::BinaryOperator::NEQ: return a != b;
        case BinaryExpr::BinaryOperator::GT: return a > b;
        case BinaryExpr::BinaryOperator::GTE: return a >= b;
        case BinaryExpr::BinaryOperator::LS: return a < b;
        case BinaryExpr::BinaryOperator::LSE: return a <= b;
        default:
            return std::nullopt;
    }
}

std::optional<double> Compiler::tryFoldArithmetic(BinaryExpr::BinaryOperator op, double a, double b)
{
    switch (op)
    {
       case BinaryExpr::BinaryOperator::ADD: return a + b;
       case BinaryExpr::BinaryOperator::SUB: return a - b;
       case BinaryExpr::BinaryOperator::MUL: return a * b;
       case BinaryExpr::BinaryOperator::FLOOR_DIV: return floor(a / b);
       case BinaryExpr::BinaryOperator::DIV: return a / b;
       case BinaryExpr::BinaryOperator::MOD: return fmod(a, b);
       case BinaryExpr::BinaryOperator::EXPO: return pow(a, b);
       default:
            return std::nullopt;
    }
}

bool Compiler::constTruthy(const Value &value)
{
    return std::visit(overloaded 
    {
        [](const bool value) -> bool { return value; },
        [](const LUA_NIL_TYPE) -> bool { return false; },
        [](const auto&) -> bool { return true; }
    }, value);
}

std::optional<Value> Compiler::tryFoldConstant(const ExprHandle &expression) 
{
    return std::visit(overloaded 
    {
        [](const StringLiteralExpr &node) -> std::optional<Value> 
        { 
            return node.value;
        },
        [](const NumberLiteralExpr &node) -> std::optional<Value> 
        { 
            return node.value;
        },
        [](const BoolLiteralExpr &node) -> std::optional<Value>
        { 
            return node.value;
        },
        [](const NilExpr &) -> std::optional<Value> 
        {
            return LUA_NIL_VALUE;
        },
        [](const UnaryExpr &node) -> std::optional<Value> 
        {
            auto constant = tryFoldConstant(node.expr);

            if (!constant) return std::nullopt;

            switch (node.op)
            {
                case UnaryExpr::UnaryOperator::NOT: return !constTruthy(*constant);
                case UnaryExpr::UnaryOperator::NEGATE:
                {
                    if (auto* num = std::get_if<double>(&constant.value())) 
                        return -(*num);
                    break;
                }
                case UnaryExpr::UnaryOperator::LENGTH:
                {
                    if (auto* string = std::get_if<std::string>(&constant.value())) 
                        return static_cast<double>((*string).size());
                    break;
                }
                default:
                    break;
            }

            return std::nullopt;
        },
        [](const BinaryExpr &node) -> std::optional<Value> 
        {
            std::optional<Value> lhs;
            if (!(lhs = tryFoldConstant(node.lhs))) 
                return std::nullopt;

            std::optional<Value> rhs;
            if (!(rhs = tryFoldConstant(node.rhs))) 
                return std::nullopt;

            return std::visit(overloaded 
            {
                [&node](const std::string &a, const std::string &b) -> std::optional<Value> 
                {
                    // Concat
                    if (node.op == BinaryExpr::BinaryOperator::CONCAT) return a + b; 
                
                    // Compare
                    if (auto folded = tryFoldCompare<std::string>(node.op, a, b)) 
                        return *folded;
                    return std::nullopt;
                },
                [&node](const double a, const double b) -> std::optional<Value> 
                {
                    // Arithmetic
                     if (auto folded = tryFoldArithmetic(node.op, a, b)) 
                        return *folded;

                    // Compare
                    if (auto folded = tryFoldCompare<double>(node.op, a, b)) 
                        return *folded;

                    return std::nullopt;
                },
                [](const auto &, const auto &) -> std::optional<Value> 
                {
                    return std::nullopt;
                },
            }, *lhs, *rhs);
        },
        [](const auto &) -> std::optional<Value> 
        {
           return std::nullopt;
        },
    },
    *expression);
}

void Compiler::ExprVisitor::operator()(const VariableExpr &node)
{  
    compiler.namedVariable(node.ident, false);
}

using UnaryOp = UnaryExpr::UnaryOperator;

const std::unordered_map<UnaryOp, ByteCode::Op> Compiler::unaryOp 
{
    {UnaryOp::NEGATE, ByteCode::Op::NEGATE},
    {UnaryOp::NOT, ByteCode::Op::NOT},
    {UnaryOp::LENGTH, ByteCode::Op::LENGTH},
    {UnaryOp::BIT_NOT, ByteCode::Op::BIT_NOT}
};

void Compiler::ExprVisitor::operator()(const UnaryExpr &node)
{
    compileExpression(node.expr, 1);

    auto it = Compiler::unaryOp.find(node.op);
    if (it != Compiler::unaryOp.end()) compiler.emit(it->second);
    else compiler.compilerError("Unexpected unary operator!");
}

void Compiler::ExprVisitor::compileLogicalOp(ByteCode::Op op, const ExprHandle &lhs, const ExprHandle &rhs)
{
    compileExpression(lhs, 1);
    compiler.emit(ByteCode::Op::DUP);
	size_t skip = compiler.emitJump(op);
	compiler.emit(ByteCode::Op::POP);
	compileExpression(rhs, 1);
	compiler.patchJump(skip);
}

void Compiler::ExprVisitor::operator()(BinaryExpr &node)
{
    using Binop = BinaryExpr::BinaryOperator;

    auto emitTwo = [this](ByteCode::Op a, ByteCode::Op b) 
    {
        compiler.emit(a);
        compiler.emit(b);
    };

    if (node.op == Binop::OR)
        return compileLogicalOp(ByteCode::Op::JUMP_IF_TRUE, node.lhs, node.rhs);
    else if (node.op == Binop::AND)
        return compileLogicalOp(ByteCode::Op::JUMP_IF_FALSE, node.lhs, node.rhs);

    bool flipOperand = (node.op == Binop::GT || node.op == Binop::GTE);

    compileExpression((!flipOperand) ? node.lhs : node.rhs, 1);
    compileExpression((!flipOperand) ? node.rhs : node.lhs, 1);

    switch (node.op)
    {
        case Binop::ADD: compiler.emit(ByteCode::Op::ADD); break;
        case Binop::SUB: compiler.emit(ByteCode::Op::SUB); break;
        case Binop::MUL: compiler.emit(ByteCode::Op::MUL); break;
        case Binop::FLOOR_DIV: compiler.emit(ByteCode::Op::FLOOR_DIV); break;
        case Binop::DIV: compiler.emit(ByteCode::Op::DIV); break;
        case Binop::EXPO: compiler.emit(ByteCode::Op::EXPO); break;
        case Binop::CONCAT: compiler.emit(ByteCode::Op::CONCAT); break; 

        case Binop::BIT_AND: compiler.emit(ByteCode::Op::BIT_AND); break;
        case Binop::BIT_OR: compiler.emit(ByteCode::Op::BIT_OR); break;
        case Binop::BIT_XOR: compiler.emit(ByteCode::Op::BIT_XOR); break;
        case Binop::BITSHIFT_LEFT: compiler.emit(ByteCode::Op::BITSHIFT_LEFT); break;
        case Binop::BITSHIFT_RIGHT: compiler.emit(ByteCode::Op::BITSHIFT_RIGHT); break;

        case Binop::EQ: compiler.emit(ByteCode::Op::BIT_OR); break;
        case Binop::NEQ: emitTwo(ByteCode::Op::EQ, ByteCode::Op::NOT); break;
        case Binop::LS: case Binop::GT: compiler.emit(ByteCode::Op::LS); break;
        case Binop::LSE: case Binop::GTE: compiler.emit(ByteCode::Op::LSE); break;
        default: 
            compiler.compilerError("Unknown binary operator!");
    }
}

void Compiler::ExprVisitor::compileExpression(const ExprHandle &expression, int expectedReturn)
{
    compiler.compileExpression(expression, expectedReturn, false);
}

void Compiler::StmtVisitor::operator()(const WhileStmt &node)
{
    size_t loopStart = compiler.chunk.code.size();

    compiler.compileExpression(node.condExpr, 1, false);
    size_t jumpToEnd = compiler.emitJump(ByteCode::Op::JUMP_IF_FALSE);

    compiler.loopStack.emplace_back(std::vector<size_t>{}, compiler.scopeDepth);
    compiler.compileBlock(node.whileStmts);

    compiler.emitLoop(ByteCode::Op::LOOP, loopStart);
    compiler.patchJump(jumpToEnd);

    size_t endLoop = compiler.chunk.code.size();

    for (size_t breakStmt : compiler.loopStack.back().breaks)
    {
        compiler.patchJumpAt(breakStmt, endLoop);
    }

    compiler.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()(const ForRangeStmt &node)
{
    compiler.loopStack.emplace_back(std::vector<size_t>{}, compiler.scopeDepth);

    compiler.beginScope();

    compiler.compileExpression(node.start, 1, false);
    compiler.addLocal(node.variable);

    compiler.compileExpression(node.start, 1, false);
    compiler.addLocal("(start)");

    compiler.compileExpression(node.stop, 1, false);
    compiler.addLocal("(stop)");

    if (node.step != nullptr) compiler.compileExpression(node.step, 1, false);
    else compiler.emitConstant(1.0);
    compiler.addLocal("(step)");

    size_t forPrepJump = compiler.emitJump(ByteCode::Op::FOR_PREP);
    size_t loopStart = compiler.chunk.code.size();

    compiler.compileBlock(node.forStmts);

    compiler.emitLoop(ByteCode::Op::FOR_LOOP, loopStart);
    compiler.patchJump(forPrepJump);

    compiler.endScope();

    size_t endLoop = compiler.chunk.code.size();

    for (size_t breakStmt : compiler.loopStack.back().breaks)
    {
        compiler.patchJumpAt(breakStmt, endLoop);
    }
    
    compiler.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()([[maybe_unused]] const ForIteratorStmt &node)
{

}

void Compiler::StmtVisitor::operator()(const RepeatStmt &node)
{
    size_t loopStart = compiler.chunk.code.size();
    compiler.loopStack.emplace_back(std::vector<size_t>{}, compiler.scopeDepth);

    compiler.compileBlock(node.repeatStmts);

    compiler.compileExpression(node.condExpr, 1, false);
    
    size_t jumpToEnd = compiler.emitJump(ByteCode::Op::JUMP_IF_TRUE);
    compiler.emitLoop(ByteCode::Op::LOOP, loopStart);
    compiler.patchJump(jumpToEnd);

    size_t endLoop = compiler.chunk.code.size();

    for (size_t breakStmt : compiler.loopStack.back().breaks)
    {
        compiler.patchJumpAt(breakStmt, endLoop);
    }

    compiler.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()(const IfStmt &node)
{
    compiler.compileExpression(node.condExpr, 1, false);
    size_t jumpToElse = compiler.emitJump(ByteCode::Op::JUMP_IF_FALSE);

    compiler.compileBlock(node.ifStmts);

    if (!node.elseStmts.empty())
    {
        size_t jumpToEnd = compiler.emitJump(ByteCode::Op::JUMP);
        compiler.patchJump(jumpToElse);

        compiler.compileBlock(node.elseStmts);

        compiler.patchJump(jumpToEnd);
    }
    else
    {
        compiler.patchJump(jumpToElse);
    }
}

void Compiler::compileAssignment(size_t numOfTargets, const std::vector<ExprHandle> &values)
{
    if (numOfTargets > INT8_MAX) compilerError("Can't have more than 128 assignment targets!");

    int remaining = static_cast<int>(numOfTargets);

    size_t available = values.size();

    for (size_t i = 0; i + 1 < available && remaining > 0; i++) 
    {
        compileExpression(values[i], 1, false);

        remaining--;
    }

    if (!values.empty() && remaining > 0)
    {
        const ExprHandle &expr = values.back();

        compileExpression(expr, remaining, false);

        remaining--;

        if (isMultiReturn(expr)) remaining = 0; // Multi return expressions should return all the values to pad nils
    }
     
    while (remaining-- > 0) emit(ByteCode::Op::LOAD_NULL); // Pad nils
}

void Compiler::StmtVisitor::operator()(const LocalAssignmentStmt &node)
{
    compiler.compileAssignment(node.ident.size(), node.value);
    
    for (const auto &i : node.ident)
    {
        compiler.addLocal(i);
    }
}

void Compiler::StmtVisitor::operator()(const AssignmentStmt &node)
{
    compiler.compileAssignment(node.target.size(), node.value);

    for (auto i = node.target.size(); i-- > 0; ) 
    {
        std::visit(overloaded 
        {
            [this](const FieldAccessExpr &node)
            { 
                compiler.emitConstant(node.field);
                compiler.compileExpression(node.expr, 1, false);
                compiler.emit(ByteCode::Op::SET_FIELD);
            },
            [this](const IndexExpr &node)
            { 
                compiler.compileExpression(node.index, 1, false);
                compiler.compileExpression(node.expr, 1, false);

                compiler.emit(ByteCode::Op::SET_FIELD);
            },
            [this](const VariableExpr &node)
            {
                compiler.namedVariable(node.ident, true);
            },
            [this](const auto &)
            {
                compiler.compilerError("Unexpected assignment target");
            }
        }, *node.target[i]);
    }
}

void Compiler::StmtVisitor::operator()(const LocalFunctionAssignmentStmt &node)
{
    compiler.addLocal(node.name);
    compiler.compileFunction(node.name, node.isVarArg, node.args, node.body);
}

void Compiler::StmtVisitor::operator()(const FunctionAssignmentStmt &node)
{
    std::visit(overloaded 
    {
        [this, &node](const FieldAccessExpr &fieldAccessExpr) 
        { 
            compiler.compileFunction(fieldAccessExpr.field, node.isVarArg, node.args, node.body);

            compiler.emitConstant(fieldAccessExpr.field);
            compiler.compileExpression(fieldAccessExpr.expr, 1, false);
            compiler.emit(ByteCode::Op::SET_FIELD);
        },
        [this, &node](const VariableExpr &variableExpr)
        {
            compiler.compileFunction(variableExpr.ident, node.isVarArg, node.args, node.body);

            compiler.namedVariable(variableExpr.ident, true);
        },
        [](const auto&)
        {
            assert(false); // Unreachable
        }
    }, *node.name);
}

void Compiler::StmtVisitor::operator()(const ExprStmt &node)
{
    if (isCallable(node.expr))
    {
        compiler.compileExpression(node.expr, 0, false);
    }
    else compiler.compilerError("Unexpected expression statement expected callable expression");
}

void Compiler::StmtVisitor::operator()(const ReturnStmt &node)
{
    if (node.values.size() > UINT8_MAX) compiler.compilerError("Can't have more than 255 return values");

    if (node.values.size() == 1 && isCallable(node.values.back()))
    {
        return compiler.compileExpression(node.values.back(), -1, true);
    }
    
    for (size_t i = 0; i + 1 < node.values.size(); i++)
    {
        compiler.compileExpression(node.values[i], 1, false);
    }

    if (!node.values.empty())
    {
        compiler.compileExpression(node.values.back(), -1, false);
    }
    
    compiler.emitWithArg(ByteCode::Op::RETURN, static_cast<uint8_t>(compiler.locals.size()));
}

void Compiler::StmtVisitor::operator()(const BreakStmt &)
{
    if (compiler.loopStack.empty()) compiler.compilerError("Break statement outside of loop");

    auto &locals = compiler.locals;

    for (auto i = locals.size(); i-- > 0 
    && locals[i].depth > compiler.loopStack.back().loopDepth;) // Remove all locals added before the break statement
    {
        compiler.emit(locals.back().isCaptured ? ByteCode::Op::CLOSE_UPVALUE : ByteCode::Op::POP);
    }

    compiler.loopStack.back().breaks.emplace_back(compiler.emitJump(ByteCode::Op::JUMP));
}

void Compiler::StmtVisitor::operator()(const GoToStmt &node) 
{
    for (size_t i = compiler.labels.size(); i-- > 0;) // Resolve backward jumps
    {
        const auto &lb = compiler.labels[i];
        if (node.label == lb.name)
        {
            for (size_t i = compiler.locals.size(); i-- > lb.currentLocals;) 
            {
                compiler.emit(compiler.locals[i].isCaptured ? ByteCode::Op::CLOSE_UPVALUE : ByteCode::Op::POP);
            }

            compiler.patchJumpAt(compiler.emitJump(ByteCode::Op::JUMP), lb.pos);
            return;
        }
    }
   
    compiler.unresolvedGoto.emplace_back(compiler.locals, node.label, compiler.emitJump(ByteCode::Op::JUMP), compiler.locals.size(), compiler.scopeDepth);
}

void Compiler::StmtVisitor::operator()(const LabelStmt &node)
{
    for (const auto &label : compiler.labels)
    {
        if (node.label == label.name)
        {
            compiler.compilerError(std::format("Label `{}` already defined in scope", node.label));
        }
    }

    compiler.labels.emplace_back(std::nullopt, node.label, compiler.chunk.code.size(), compiler.locals.size(), compiler.scopeDepth);

    const auto &lb = compiler.labels.back();

    for (size_t i = compiler.unresolvedGoto.size(); i-- > 0 && compiler.unresolvedGoto[i].currentScope >= lb.currentScope;) // Resolve forward jumps
    {
        const auto &gt = compiler.unresolvedGoto[i];
        if (gt.name == node.label)
        {
            if (node.isLastStmt) compiler.locals.resize(gt.currentLocals); // Those locals after the goto dont exist

            if (lb.currentLocals > gt.currentLocals)
            {
                compiler.compilerError(std::format("Goto `{}` jumps over scope of local `{}`", gt.name, compiler.locals.back().name));
            }

            const auto &gtLocals = gt.locals.value();
            if (!gtLocals.empty() && gtLocals.back().depth > lb.currentScope) // Locals to close
            {
                size_t fallthrough = compiler.emitJump(ByteCode::Op::JUMP);
                compiler.patchJumpAt(gt.pos, lb.pos);

                for (size_t i = gtLocals.size(); i-- > 0 && gtLocals[i].depth > lb.currentScope;)
                {
                    compiler.emit(gtLocals[i].isCaptured ? ByteCode::Op::CLOSE_UPVALUE : ByteCode::Op::POP);
                }
           
                compiler.patchJump(fallthrough);
            }
            else 
            {
                compiler.patchJumpAt(gt.pos, lb.pos);
            }

            compiler.unresolvedGoto.erase(compiler.unresolvedGoto.begin() + 
            static_cast<std::ptrdiff_t>(i));
        }
    }
}

void Compiler::StmtVisitor::operator()(const BlockStmt &node)
{
    compiler.compileBlock(node.stmt);
}

void Compiler::compileExpression(const ExprHandle &expression, int expectedReturn, bool isTailCall)
{
    if (auto folded = tryFoldConstant(expression))
    {
        return std::visit(overloaded 
        {
            [this](const bool a)
            {
                emit(a ? ByteCode::Op::LOAD_TRUE : ByteCode::Op::LOAD_FALSE);
            },
            [this](const LUA_NIL_TYPE)
            {
                emit(ByteCode::Op::LOAD_NULL);
            },
            [this](const auto &a) 
            {
                emitConstant(a);
            },
        } , *folded);
    }

    std::visit(ExprVisitor(expectedReturn, isTailCall, *this), *expression);
}

void Compiler::compileBlock(const Ast &stmts)
{
    beginScope();
    compileStmts(stmts);
    endScope();
}

FunctionHandle Compiler::compile(const Ast &stmts)
{
    compileStmts(stmts);

    emitReturn();

    return makeFunction();
}
