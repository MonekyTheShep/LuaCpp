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
        emit(locals.back().isCaptured ? Op::CLOSE_UPVALUE : Op::POP);
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
    Op storeOp, loadOp;

    int arg = resolveLocal(name);

    if (arg != -1)
    {
        storeOp = Op::STORE_LOCAL;
        loadOp = Op::LOAD_LOCAL;
    }
    else if ((arg = resolveUpValue(name)) != -1)
    {
        storeOp = Op::STORE_UPVALUE;
        loadOp = Op::LOAD_UPVALUE;
    }
    else
    {
        storeOp = Op::STORE_GLOBAL;
        loadOp = Op::LOAD_GLOBAL;
        arg = makeConstant(name);
    }

    if (assignment)
    {
        emitWithArg(storeOp, static_cast<uint8_t>(arg));
    }
    else 
    {
        emitWithArg(loadOp, static_cast<uint8_t>(arg));
    }
}

size_t Compiler::emitJump(Op op) 
{
    emit(op);
    emit(0XFF);
    emit(0XFF);
    return chunk.code.size() - 2;
}

void Compiler::emitLoop(Op op, size_t loopStart) 
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
    emitWithArg(Op::LOAD_CONST, static_cast<uint8_t>(makeConstant(value)));
}

int Compiler::makeConstant(Value value) 
{
    std::optional<int> constant = chunk.makeConstant(std::move(value));
    if (!constant) compilerError("Too many constants in function");
    return *constant;
}

void Compiler::emitWithArg(Op op, uint8_t arg)
{
    emit(op);
    emit(arg);
}

void Compiler::emitWithArg2(Op op, uint8_t arg, uint8_t arg2)
{
    emit(op);
    emit(arg);
    emit(arg2);
}

void Compiler::emit(Op op)
{
    chunk.write(op, currentLine);
}

void Compiler::emit(uint8_t arg)
{
    chunk.write(arg, currentLine);
}

void Compiler::ExprVisitor::operator()(const NumberLiteralExpr &node)
{
    context.emitConstant(node.value);
}

void Compiler::ExprVisitor::operator()(const StringLiteralExpr &node)
{
    context.emitConstant(node.value);
}

void Compiler::ExprVisitor::operator()(const BoolLiteralExpr &node)
{
    context.emit(node.value ? Op::LOAD_TRUE : Op::LOAD_FALSE);
}

void Compiler::ExprVisitor::operator()(const NilExpr &)
{
    context.emit(Op::LOAD_NULL);
}

void Compiler::ExprVisitor::operator()(const VarArgExpr &)
{
    if (!context.function->isVarArg) context.compilerError("Can't use `...` outside a vararg function");
    context.emitWithArg(Op::VARARG, static_cast<uint8_t>(expectedReturn));
}

void Compiler::ExprVisitor::compileTableExpr(const TableExpr &node, double &arrayIndex)
{
    switch (node.kind) 
    {
        case TableExpr::Kind::List:
        {
            context.emitConstant(arrayIndex++);
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
    if (node.fields.size() > UINT8_MAX) context.compilerError("Can't have more than 255 fields in table constructor");

    if (node.fields.empty()) return context.emitWithArg(Op::MAKE_TABLE, 0); // No point in compiling fields

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
        context.emitWithArg(Op::MAKE_TABLE,fieldsMinusOne); 
        context.emit(Op::STORE_TABLE);
        compileExpression(lastField.value, -1);
        if (arrayIndex > UINT8_MAX) context.compilerError("Constructor too long!");
        context.emitWithArg(Op::SET_LIST, static_cast<uint8_t>(arrayIndex));
    }
    else 
    {
        compileTableExpr(lastField, arrayIndex);
        context.emitWithArg(Op::MAKE_TABLE,++fieldsMinusOne); 
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
    context.emit(Op::STORE_CALLEE);
            
    if (node.args.size() > UINT8_MAX) context.compilerError("Can't have more than 255 arguments inside of function call");
    compileArgs(node.args);
   
    context.emitWithArg2 (
        isTailCall ? Op::TAIL_CALL : Op::CALL, 
        static_cast<uint8_t>(node.args.size()), 
        static_cast<uint8_t>(expectedReturn)
    );
}

void Compiler::ExprVisitor::operator()(const MethodAccessExpr &node) 
{
    context.emitConstant(node.field);
    compileExpression(node.object, 1);
    context.emit(Op::GET_METHOD);

    if (node.args.size() > UINT8_MAX) context.compilerError("Can't have more than 255 arguments inside of method call");
    compileArgs(node.args);
   
    context.emitWithArg2 (
        isTailCall ? Op::TAIL_CALL : Op::CALL, 
        static_cast<uint8_t>(node.args.size()), 
        static_cast<uint8_t>(expectedReturn)
    );
}

void Compiler::ExprVisitor::operator()(const FieldAccessExpr &node)
{
    context.emitConstant(node.field);
    compileExpression(node.expr, 1);

    context.emit(Op::GET_FIELD);
}

void Compiler::ExprVisitor::operator()(const IndexExpr &node)
{
    compileExpression(node.index, 1);
    compileExpression(node.expr, 1);
    
    context.emit(Op::GET_FIELD);
}

void Compiler::ExprVisitor::operator()(const FunctionExpr &node)
{
    context.compileFunction("<anonymous>", node.isVarArg, node.args, node.body);
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
    emitWithArg(Op::MAKE_CLOSURE, static_cast<uint8_t>(makeConstant(function)));

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
    context.namedVariable(node.ident, false);
}

const std::unordered_map<UnaryExpr::UnaryOperator, Op> Compiler::unaryOp 
{
    {UnaryExpr::UnaryOperator::NEGATE, Op::NEGATE},
    {UnaryExpr::UnaryOperator::NOT, Op::NOT},
    {UnaryExpr::UnaryOperator::LENGTH, Op::LENGTH},
    {UnaryExpr::UnaryOperator::BIT_NOT, Op::BIT_NOT}
};

void Compiler::ExprVisitor::operator()(const UnaryExpr &node)
{
    compileExpression(node.expr, 1);

    auto it = Compiler::unaryOp.find(node.op);
    if (it != Compiler::unaryOp.end()) context.emit(it->second);
    else context.compilerError("Unexpected unary operator!");
}

const std::unordered_map<BinaryExpr::BinaryOperator, Op> Compiler::binaryOp 
{
    {BinaryExpr::BinaryOperator::ADD, Op::ADD},
    {BinaryExpr::BinaryOperator::SUB, Op::SUB},
    {BinaryExpr::BinaryOperator::MUL, Op::MUL},
    {BinaryExpr::BinaryOperator::FLOOR_DIV, Op::FLOOR_DIV},
    {BinaryExpr::BinaryOperator::DIV, Op::DIV},
    {BinaryExpr::BinaryOperator::MOD, Op::MOD},
    {BinaryExpr::BinaryOperator::EXPO, Op::EXPO},
    {BinaryExpr::BinaryOperator::CONCAT, Op::CONCAT},

    {BinaryExpr::BinaryOperator::BIT_AND, Op::BIT_AND},
    {BinaryExpr::BinaryOperator::BIT_OR, Op::BIT_OR},
    {BinaryExpr::BinaryOperator::BIT_XOR, Op::BIT_XOR},
    {BinaryExpr::BinaryOperator::BITSHIFT_LEFT, Op::BITSHIFT_LEFT},
    {BinaryExpr::BinaryOperator::BITSHIFT_RIGHT, Op::BITSHIFT_RIGHT},

    {BinaryExpr::BinaryOperator::EQ, Op::EQ},
    {BinaryExpr::BinaryOperator::NEQ, Op::EQ}, // NEQ pushes not after eq
    {BinaryExpr::BinaryOperator::GT, Op::LS}, // Flips operands around
    {BinaryExpr::BinaryOperator::GTE, Op::LSE}, // Flips operands around
    {BinaryExpr::BinaryOperator::LS, Op::LS},
    {BinaryExpr::BinaryOperator::LSE, Op::LSE},
};

void Compiler::ExprVisitor::compileLogicalOp(Op op, const ExprHandle &lhs, const ExprHandle &rhs)
{
    compileExpression(lhs, 1);
    context.emit(Op::DUP);
	size_t skip = context.emitJump(op);
	context.emit(Op::POP);
	compileExpression(rhs, 1);
	context.patchJump(skip);
}

void Compiler::ExprVisitor::operator()(const BinaryExpr &node)
{
    bool flipOperand = false;

    switch (node.op)
    {
        case BinaryExpr::BinaryOperator::OR:
            return compileLogicalOp(Op::JUMP_IF_TRUE, node.lhs, node.rhs);
        case BinaryExpr::BinaryOperator::AND:
            return compileLogicalOp(Op::JUMP_IF_FALSE, node.lhs, node.rhs);
        case BinaryExpr::BinaryOperator::GTE: case BinaryExpr::BinaryOperator::GT:
            flipOperand = true;
            [[fallthrough]];
        default:
        {
            auto it = Compiler::binaryOp.find(node.op);
            if (it != Compiler::binaryOp.end())
            {
                compileExpression(flipOperand ? node.rhs : node.lhs, 1);
                compileExpression(flipOperand ? node.lhs : node.rhs, 1);

                context.emit(it->second);
                if (node.op == BinaryExpr::BinaryOperator::NEQ) context.emit(Op::NOT);
            }
            else context.compilerError("Unexpected binary operator");
        }
    }  
}

void Compiler::ExprVisitor::compileExpression(const ExprHandle &expression, int expectedReturn)
{
    context.compileExpression(expression, expectedReturn, false);
}

void Compiler::StmtVisitor::operator()(const WhileStmt &node)
{
    size_t loopStart = context.chunk.code.size();

    context.compileExpression(node.condExpr, 1, false);
    size_t jumpToEnd = context.emitJump(Op::JUMP_IF_FALSE);

    context.loopStack.emplace_back(std::vector<size_t>{}, context.scopeDepth);
    context.compileBlock(node.whileStmts);

    context.emitLoop(Op::LOOP, loopStart);
    context.patchJump(jumpToEnd);

    size_t endLoop = context.chunk.code.size();

    for (size_t breakStmt : context.loopStack.back().breaks)
    {
        context.patchJumpAt(breakStmt, endLoop);
    }

    context.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()(const ForRangeStmt &node)
{
    context.loopStack.emplace_back(std::vector<size_t>{}, context.scopeDepth);

    context.beginScope();

    context.compileExpression(node.start, 1, false);
    context.addLocal(node.variable);

    context.compileExpression(node.start, 1, false);
    context.addLocal("(start)");

    context.compileExpression(node.stop, 1, false);
    context.addLocal("(stop)");

    if (node.step != nullptr) context.compileExpression(node.step, 1, false);
    else context.emitConstant(1.0);
    context.addLocal("(step)");

    size_t forPrepJump = context.emitJump(Op::FOR_PREP);
    size_t loopStart = context.chunk.code.size();

    context.compileBlock(node.forStmts);

    context.emitLoop(Op::FOR_LOOP, loopStart);
    context.patchJump(forPrepJump);

    context.endScope();

    size_t endLoop = context.chunk.code.size();

    for (size_t breakStmt : context.loopStack.back().breaks)
    {
        context.patchJumpAt(breakStmt, endLoop);
    }
    
    context.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()([[maybe_unused]] const ForIteratorStmt &node)
{

}

void Compiler::StmtVisitor::operator()(const RepeatStmt &node)
{
    size_t loopStart = context.chunk.code.size();
    context.loopStack.emplace_back(std::vector<size_t>{}, context.scopeDepth);

    context.compileBlock(node.repeatStmts);

    context.compileExpression(node.condExpr, 1, false);
    
    size_t jumpToEnd = context.emitJump(Op::JUMP_IF_TRUE);
    context.emitLoop(Op::LOOP, loopStart);
    context.patchJump(jumpToEnd);

    size_t endLoop = context.chunk.code.size();

    for (size_t breakStmt : context.loopStack.back().breaks)
    {
        context.patchJumpAt(breakStmt, endLoop);
    }

    context.loopStack.pop_back();
}

void Compiler::StmtVisitor::operator()(const IfStmt &node)
{
    context.compileExpression(node.condExpr, 1, false);
    size_t jumpToElse = context.emitJump(Op::JUMP_IF_FALSE);

    context.compileBlock(node.ifStmts);

    if (!node.elseStmts.empty())
    {
        size_t jumpToEnd = context.emitJump(Op::JUMP);
        context.patchJump(jumpToElse);

        context.compileBlock(node.elseStmts);

        context.patchJump(jumpToEnd);
    }
    else
    {
        context.patchJump(jumpToElse);
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
     
    while (remaining-- > 0) emit(Op::LOAD_NULL); // Pad nils
}

void Compiler::StmtVisitor::operator()(const LocalAssignmentStmt &node)
{
    context.compileAssignment(node.ident.size(), node.value);
    
    for (const auto &i : node.ident)
    {
        context.addLocal(i);
    }
}

void Compiler::StmtVisitor::operator()(const AssignmentStmt &node)
{
    context.compileAssignment(node.target.size(), node.value);

    for (auto i = node.target.size(); i-- > 0; ) 
    {
        std::visit(overloaded 
        {
            [this](const FieldAccessExpr &node)
            { 
                context.emitConstant(node.field);
                context.compileExpression(node.expr, 1, false);
                context.emit(Op::SET_FIELD);
            },
            [this](const IndexExpr &node)
            { 
                context.compileExpression(node.index, 1, false);
                context.compileExpression(node.expr, 1, false);

                context.emit(Op::SET_FIELD);
            },
            [this](const VariableExpr &node)
            {
                context.namedVariable(node.ident, true);
            },
            [this](const auto &)
            {
                context.compilerError("Unexpected assignment target");
            }
        }, *node.target[i]);
    }
}

void Compiler::StmtVisitor::operator()(const LocalFunctionAssignmentStmt &node)
{
    context.addLocal(node.name);
    context.compileFunction(node.name, node.isVarArg, node.args, node.body);
}

void Compiler::StmtVisitor::operator()(const FunctionAssignmentStmt &node)
{
    std::visit(overloaded 
    {
        [this, &node](const FieldAccessExpr &fieldAccessExpr) 
        { 
            context.compileFunction(fieldAccessExpr.field, node.isVarArg, node.args, node.body);

            context.emitConstant(fieldAccessExpr.field);
            context.compileExpression(fieldAccessExpr.expr, 1, false);
            context.emit(Op::SET_FIELD);
        },
        [this, &node](const VariableExpr &variableExpr)
        {
            context.compileFunction(variableExpr.ident, node.isVarArg, node.args, node.body);

            context.namedVariable(variableExpr.ident, true);
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
        context.compileExpression(node.expr, 0, false);
    }
    else context.compilerError("Unexpected expression statement expected callable expression");
}

void Compiler::StmtVisitor::operator()(const ReturnStmt &node)
{
    if (node.values.size() > UINT8_MAX) context.compilerError("Can't have more than 255 return values");

    if (node.values.size() == 1 && isCallable(node.values.back()))
    {
        return context.compileExpression(node.values.back(), -1, true);
    }
    
    for (size_t i = 0; i + 1 < node.values.size(); i++)
    {
        context.compileExpression(node.values[i], 1, false);
    }

    if (!node.values.empty())
    {
        context.compileExpression(node.values.back(), -1, false);
    }
    
    context.emitWithArg(Op::RETURN, static_cast<uint8_t>(context.locals.size()));
}

void Compiler::StmtVisitor::operator()(const BreakStmt &)
{
    if (context.loopStack.empty()) context.compilerError("Break statement outside of loop");

    auto &locals = context.locals;

    for (auto i = locals.size(); i-- > 0 
    && locals[i].depth > context.loopStack.back().loopDepth;) // Remove all locals added before the break statement
    {
        context.emit(locals.back().isCaptured ? Op::CLOSE_UPVALUE : Op::POP);
    }

    context.loopStack.back().breaks.emplace_back(context.emitJump(Op::JUMP));
}

void Compiler::StmtVisitor::operator()(const GoToStmt &node) 
{
    for (size_t i = context.labels.size(); i-- > 0;) // Resolve backward jumps
    {
        const auto &lb = context.labels[i];
        if (node.label == lb.name)
        {
            for (size_t i = context.locals.size(); i-- > lb.currentLocals;) 
            {
                context.emit(context.locals[i].isCaptured ? Op::CLOSE_UPVALUE : Op::POP);
            }

            context.patchJumpAt(context.emitJump(Op::JUMP), lb.pos);
            return;
        }
    }
   
    context.unresolvedGoto.emplace_back(context.locals, node.label, context.emitJump(Op::JUMP), context.locals.size(), context.scopeDepth);
}

void Compiler::StmtVisitor::operator()(const LabelStmt &node)
{
    for (const auto &label : context.labels)
    {
        if (node.label == label.name)
        {
            context.compilerError(std::format("Label `{}` already defined in scope", node.label));
        }
    }

    context.labels.emplace_back(std::nullopt, node.label, context.chunk.code.size(), context.locals.size(), context.scopeDepth);

    for (size_t i = context.unresolvedGoto.size(); i-- > 0 && context.unresolvedGoto[i].currentScope >= context.scopeDepth;) // Resolve forward jumps
    {
        const auto &gt = context.unresolvedGoto[i];
        if (gt.name == node.label)
        {
            if (node.isLastStmt) context.locals.resize(gt.currentLocals); // Those locals after the goto dont exist

            if (context.locals.size() > gt.currentLocals)
            {
                context.compilerError(std::format("Goto `{}` jumps over scope of local `{}`", gt.name, context.locals.back().name));
            }

            size_t fallthrough = context.emitJump(Op::JUMP);
            context.patchJumpAt(gt.pos, context.chunk.code.size());

            auto &gtLocals = gt.locals.value();
            for (size_t i = gtLocals.size(); i-- > 0 && gtLocals[i].depth > context.scopeDepth;)
            {
                context.emit(gtLocals[i].isCaptured ? Op::CLOSE_UPVALUE : Op::POP);
            }
           
            context.patchJump(fallthrough);

            context.unresolvedGoto.erase(context.unresolvedGoto.begin() + 
            static_cast<std::ptrdiff_t>(i));
        }
    }
}

void Compiler::StmtVisitor::operator()(const BlockStmt &node)
{
    context.compileBlock(node.stmt);
}

void Compiler::compileExpression(const ExprHandle &expression, int expectedReturn, bool isTailCall)
{
    if (auto folded = tryFoldConstant(expression))
    {
        return std::visit(overloaded 
        {
            [this](const bool a)
            {
                emit(a ? Op::LOAD_TRUE : Op::LOAD_FALSE);
            },
            [this](const LUA_NIL_TYPE)
            {
                emit(Op::LOAD_NULL);
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
