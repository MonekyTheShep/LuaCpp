#include "parser/parser.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "parser/ast.h"
#include "lexer/lexer.h"
#include "lexer/token.h"

Ast Parser::parse()
{
    Ast statements = parseBlock();
    expect(Token::Type::TEOF, "chunk");

    return statements;
}

void Parser::skipSeparators() { while (match(Token::Type::SEMICOLON)); }

StatementHandle Parser::parseStatement(bool *isLast)
{
    switch (peek().type)
    {
        case Token::Type::LOCAL:
            return parseLocalStatement();
        case Token::Type::DO:
            return parseBlockStatement();
        case Token::Type::WHILE:
            return parseWhileStatement();
        case Token::Type::REPEAT:
            return parseRepeatStatement();
        case Token::Type::FOR:
            return parseForStatement();
        case Token::Type::FUNCTION:
            return parseFunctionAssignmentStatement();
        case Token::Type::IF:
            return parseIfStatement();
        case Token::Type::GOTO:
            return parseGoToStatement();
        case Token::Type::RETURN:
            if (isLast) *isLast = true;
            return parseReturnStatement();
        case Token::Type::BREAK:
            if (isLast) *isLast = true;
            return parseBreakStatement();
        case Token::Type::DOUBLECOLON:
            return parseLabelStatement();
        default:
            ExprHandle expr = parsePostFix();

            if (isCallExpr(expr)) return makeStmt(ExprStmt{std::move(expr)});

            return parseAssignment(std::move(expr));
    }
}

StatementHandle Parser::parseAssignment(ExprHandle firstExpr)
{
    const char *context = "assignment list";

    std::vector<ExprHandle> leftExprs;
    leftExprs.push_back(std::move(firstExpr));

    while (match(Token::Type::COMMA))
    {
        leftExprs.emplace_back(parsePostFix());
    }   

    expect(Token::Type::OP_ASSIGN, context);

    std::vector<ExprHandle> rightExprs;

    do 
    {
        rightExprs.emplace_back(parseExpression());
    } while (match(Token::Type::COMMA));


   return makeStmt(AssignmentStmt{std::move(leftExprs), std::move(rightExprs)});
}

StatementHandle Parser::parseLocalStatement()
{
    advance(); // Consume Local Token

    switch (peek().type)
    {
        case Token::Type::FUNCTION:
            return parseLocalFunctionAssignmentStatement();
        case Token::Type::IDENTIFIER:
            return parseLocalAssignmentStatement();
        default:
            multiTokenError({Token::Type::FUNCTION, Token::Type::IDENTIFIER}, "local");
    }
}

using VariableAttribute = LocalAssignmentStmt::VariableAttribute;

VariableAttribute Parser::checkVariableAttribute()
{
    const char *context = "variable attribute";

    VariableAttribute attr = VariableAttribute::DEFAULT;

    if (match(Token::Type::OP_LESS))
    {
        std::string name = expectIdentifier(context);

        if (name == "const")
        {
            attr = VariableAttribute::CONST;
        }
        else
        {
            parserError("Unknown variable attribute", previous());
        }

        expect(Token::Type::OP_GREATER, context);
    }

    return attr;
}

StatementHandle Parser::parseLocalAssignmentStatement()
{
    const char *context = "local assignment";

    std::vector<LocalAssignmentStmt::Variable> variables;

    do
    {
        variables.emplace_back(expectIdentifier(context), checkVariableAttribute());
    } while (match(Token::Type::COMMA));

    std::vector<ExprHandle> exprs;
    
    if (match(Token::Type::OP_ASSIGN))
    {
        do
        {
            exprs.emplace_back(parseExpression());
        } while (match(Token::Type::COMMA));
    }

    return makeStmt(LocalAssignmentStmt{std::move(variables), std::move(exprs)});
}

StatementHandle Parser::parseLocalFunctionAssignmentStatement()
{
    advance(); // Consume Function Token;

    const char *context = "local function assignment";

    std::string identifier = expectIdentifier(context);

    expect(Token::Type::LEFT_PAREN, context);
    auto [args, isVarArg] = parseParams();
    expect(Token::Type::RIGHT_PAREN, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(LocalFunctionAssignmentStmt{std::move(identifier), isVarArg, std::move(args), std::move(stmts)});
}

StatementHandle Parser::parseFunctionAssignmentStatement()
{
    advance(); // Consume Function Token;

    const char *context = "function assignment";

    bool isMethod = false;
    ExprHandle expr = parseFunctionName(isMethod);

    expect(Token::Type::LEFT_PAREN, context);
    auto [args, isVarArg] = parseParams();
    expect(Token::Type::RIGHT_PAREN, context);

    if (isMethod)
    {
        args.insert(args.begin(), "self");
    }

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(FunctionAssignmentStmt{std::move(expr), isVarArg, std::move(args), std::move(stmts)});
}

ExprHandle Parser::parseFunctionName(bool &isMethod)
{
    std::string identifier = expectIdentifier("function");

    ExprHandle expr = makeExpr(VariableExpr{std::move(identifier)});

    while (match(Token::Type::DOT))
    {
        std::string field = expectIdentifier("function field");
        expr = makeExpr(FieldAccessExpr{std::move(expr), std::move(field)});
    }

    if (match(Token::Type::COLON))
    {
        isMethod = true;
        std::string field = expectIdentifier("method name");
        expr = makeExpr(FieldAccessExpr{std::move(expr), std::move(field)});
    }

    return expr;
}

StatementHandle Parser::parseWhileStatement()
{
    advance(); // Consume While Token

    const char *context = "while loop";
   
    ExprHandle expr = parseExpression();
    expect(Token::Type::DO, context);
    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(WhileStmt{std::move(expr), std::move(stmts)});
}

StatementHandle Parser::parseRepeatStatement()
{
    advance(); // Consume Repeat Token
    
    const char *context = "repeat loop";
    
    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::UNTIL, context);
    ExprHandle expr = parseExpression();

    return makeStmt(RepeatStmt{std::move(expr), std::move(stmts)});
}

StatementHandle Parser::parseForStatement()
{
    advance(); // Consume For Token;

    const char *context = "for loop";

    std::string identifier = expectIdentifier(context);

    switch (peek().type)
    {
        case Token::Type::OP_ASSIGN:
            return parseForCountStatement(std::move(identifier));
        case Token::Type::COMMA: case Token::Type::IN:
            return parseForIteratorStatement(std::move(identifier));
        default:
            multiTokenError({Token::Type::IN, Token::Type::OP_ASSIGN}, "for loop");
    }
}

StatementHandle Parser::parseForIteratorStatement(std::string firstIdentifier)
{
    const char *context = "for loop";

    std::vector<std::string> variables;
    variables.emplace_back(std::move(firstIdentifier));

    while (match(Token::Type::COMMA))
    {
        variables.emplace_back(expectIdentifier(context));
    }

    expect(Token::Type::IN, context);

    ExprHandle iterator = parseExpression();

    expect(Token::Type::DO, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(ForIteratorStmt{std::move(variables), std::move(iterator), std::move(stmts)});
}

StatementHandle Parser::parseForCountStatement(std::string firstIdentifier)
{
    const char *context = "for loop";

    advance(); // Consume Assignment Token

    ExprHandle start = parseExpression();

    expect(Token::Type::COMMA, context);
    ExprHandle end = parseExpression();

    ExprHandle step;
    if (match(Token::Type::COMMA))
    {
        step = parseExpression();
    }

    expect(Token::Type::DO, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(ForRangeStmt{std::move(firstIdentifier), std::move(start), std::move(end), std::move(step), std::move(stmts)});
}

StatementHandle Parser::parseIfStatement()
{
    advance(); // Consume If
    return parseIfRestStatement(false);
}

StatementHandle Parser::parseIfRestStatement(bool withinElseIf)
{
    ExprHandle condition = parseExpression();

    const char *context = withinElseIf ? "elseif" : "if";
    
    expect(Token::Type::THEN, context);

    std::vector<StmtWithPos> thenStmts;
    std::vector<StmtWithPos> elseStmts;

    thenStmts = parseBlock();

    if (match(Token::Type::ELSE_IF))
    {
        int startLine = peek().line;
        elseStmts.emplace_back(parseIfRestStatement(true), startLine);
    }
    else if (match(Token::Type::ELSE))
    {
        elseStmts = parseBlock();

        expect(Token::Type::END, "else");
    }
    else
    {
        expect(Token::Type::END, context);
    }

    return makeStmt(IfStmt{std::move(condition),std::move(thenStmts), std::move(elseStmts)});
}

StatementHandle Parser::parseGoToStatement()
{
    advance(); // Consume Goto token

    const char *context = "goto";

   
    std::string label = expectIdentifier(context);

    return makeStmt(GoToStmt{std::move(label)});
}

StatementHandle Parser::parseReturnStatement()
{
    advance(); // Consume Return Token;

    std::vector<ExprHandle> values;
    if (!isEndBlock() && !check(Token::Type::SEMICOLON))
    {
        do
        {
            values.emplace_back(parseExpression());
        } while (match(Token::Type::COMMA));
    }

    return makeStmt(ReturnStmt{std::move(values)});
}

StatementHandle Parser::parseBreakStatement()
{
    advance(); // Consume Break Token;
    return makeStmt(BreakStmt{});
}

StatementHandle Parser::parseLabelStatement()
{
    advance(); // Consume Double Colon Token

    const char *context = "label";

    std::string label = expectIdentifier(context);
    expect(Token::Type::DOUBLECOLON, context);
    return makeStmt(LabelStmt{std::move(label), isEndBlock(false)});
}

StatementHandle Parser::parseBlockStatement()
{
    advance(); // Consume Do Token

    const char *context = "do block";

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(Token::Type::END, context);

    return makeStmt(BlockStmt{std::move(stmts)});
}

std::vector<StmtWithPos> Parser::parseBlock()
{
    std::vector<StmtWithPos> stmts;

    skipSeparators();
    bool isLast = false;
    while (!isEndBlock() && !isLast)
    {
        int startLine = peek().line;
        stmts.emplace_back(parseStatement(&isLast), startLine);
        skipSeparators();
    }

    return stmts;
}

std::pair<int, int> Parser::getPrecedence(Token::Type op)
{
    switch (op)
    {
        case Token::Type::OR:
            return {10, 11};
        case Token::Type::AND:
            return {20, 21};
        case Token::Type::OP_NOT_EQUAL: 
        case Token::Type::OP_EQUAL:
            return {30, 31};
        case Token::Type::OP_GREATER:
        case Token::Type::OP_GREATER_EQUAL:
        case Token::Type::OP_LESS:
        case Token::Type::OP_LESS_EQUAL:
            return {40, 41};
        case Token::Type::OP_OR:
            return {50, 51};
        case Token::Type::OP_NOT:
            return {60, 61};
        case Token::Type::OP_AND:
            return {70, 71};
        case Token::Type::OP_BITSHIFT_LEFT:
        case Token::Type::OP_BITSHIFT_RIGHT:
            return {80, 81};
        case Token::Type::OP_CONCAT:  // Right associative
            return {91, 90};
        case Token::Type::OP_ADD:
        case Token::Type::OP_SUB:
            return {100, 101};
        case Token::Type::OP_DIV:
        case Token::Type::OP_FLOOR_DIV:
        case Token::Type::OP_MOD:
        case Token::Type::OP_MUL:
            return {110, 111};
        case Token::Type::OP_EXPO:  // Right associative
            return {131, 130};
        default:
            return {-1,-1};
    }
}

std::unordered_map<Token::Type, BinaryExpr::BinaryOperator> Parser::binOps = 
{
    {Token::Type::OR, BinaryExpr::BinaryOperator::OR},
    {Token::Type::AND, BinaryExpr::BinaryOperator::AND},
    {Token::Type::OP_NOT_EQUAL, BinaryExpr::BinaryOperator::NEQ},
    {Token::Type::OP_EQUAL, BinaryExpr::BinaryOperator::EQ},
    {Token::Type::OP_NOT_EQUAL, BinaryExpr::BinaryOperator::NEQ},
    {Token::Type::OP_GREATER, BinaryExpr::BinaryOperator::GT},
    {Token::Type::OP_GREATER_EQUAL, BinaryExpr::BinaryOperator::GTE},
    {Token::Type::OP_LESS, BinaryExpr::BinaryOperator::LS},
    {Token::Type::OP_LESS_EQUAL, BinaryExpr::BinaryOperator::LSE},
    {Token::Type::OP_OR, BinaryExpr::BinaryOperator::BIT_OR},
    {Token::Type::OP_NOT, BinaryExpr::BinaryOperator::BIT_XOR},
    {Token::Type::OP_AND, BinaryExpr::BinaryOperator::BIT_AND},
    {Token::Type::OP_BITSHIFT_LEFT, BinaryExpr::BinaryOperator::BITSHIFT_LEFT},
    {Token::Type::OP_BITSHIFT_RIGHT, BinaryExpr::BinaryOperator::BITSHIFT_RIGHT},
    {Token::Type::OP_CONCAT, BinaryExpr::BinaryOperator::CONCAT},
    {Token::Type::OP_ADD, BinaryExpr::BinaryOperator::ADD},
    {Token::Type::OP_SUB, BinaryExpr::BinaryOperator::SUB},
    {Token::Type::OP_DIV, BinaryExpr::BinaryOperator::DIV},
    {Token::Type::OP_FLOOR_DIV, BinaryExpr::BinaryOperator::FLOOR_DIV},
    {Token::Type::OP_MOD, BinaryExpr::BinaryOperator::MOD},
    {Token::Type::OP_MUL, BinaryExpr::BinaryOperator::MUL},
    {Token::Type::OP_EXPO, BinaryExpr::BinaryOperator::EXPO}
};

BinaryExpr::BinaryOperator Parser::getBinaryOperator(Token::Type op)
{
    auto it = binOps.find(op);
    if (it != binOps.end()) return it->second;
    parserError("Unexpected binary operator!", peek());
}

std::unordered_map<Token::Type, UnaryExpr::UnaryOperator> Parser::unaryOps = 
{
    {Token::Type::OP_SUB, UnaryExpr::UnaryOperator::NEGATE},
    {Token::Type::OP_LENGTH, UnaryExpr::UnaryOperator::LENGTH},
    {Token::Type::OP_NOT, UnaryExpr::UnaryOperator::BIT_NOT},
    {Token::Type::NOT, UnaryExpr::UnaryOperator::NOT}
};

UnaryExpr::UnaryOperator Parser::getUnaryOperator(Token::Type op)
{
    auto it = unaryOps.find(op);
    if (it != unaryOps.end()) return it->second;
    parserError("Unexpected unary operator!", peek());
}

constexpr int UNARY_PRIORITY = 120;

ExprHandle Parser::parseExpression(int minBp)
{
    ExprHandle lhs;
    
    if(match(Token::Type::OP_SUB, Token::Type::OP_LENGTH, Token::Type::OP_NOT, Token::Type::NOT))
    {
        Token::Type op = previous().type;
        lhs = makeExpr(UnaryExpr{getUnaryOperator(op), parseExpression(UNARY_PRIORITY)});
    }
    else lhs = parseSimple();

    for(;;)
    {
        Token::Type op = peek().type;

        auto [lBp, rBp] = getPrecedence(op);
        if (lBp < minBp) break;
        advance(); // Consume operator

        ExprHandle rhs = parseExpression(rBp);
        
        lhs = makeExpr(BinaryExpr{getBinaryOperator(op), std::move(lhs), std::move(rhs)});
    }

    return lhs;
}

ExprHandle Parser::parseSimple()
{
    if (match(Token::Type::FALSE)) return makeExpr(BoolLiteralExpr{false});
    if (match(Token::Type::TRUE)) return makeExpr(BoolLiteralExpr{true});

    if (match(Token::Type::NIL)) return makeExpr(NilExpr{});
    if (match(Token::Type::VARARG)) return makeExpr(VarArgExpr{});

    if (match(Token::Type::STRING_LITERAL)) return makeExpr(StringLiteralExpr{Lexer::makeFormattedString(previous().lexeme)});
    if (match(Token::Type::MALFORMED_STRING)) parserError("Unclosed string", previous());

    if (match(Token::Type::NUMBER_LITERAL)) 
    {
        std::string number = std::string(previous().lexeme);
        char *end;
        double value = std::strtod(number.c_str(), &end);
        if (end != number.c_str() + number.size()) parserError("Malformed number", previous());
        return makeExpr(NumberLiteralExpr{value});
    }

    if (match(Token::Type::LEFT_BRACE))
    {
        const char *context = "table constructor";

        std::vector<TableExpr> tableDef = parseTable();
        expect(Token::Type::RIGHT_BRACE, context);
        return makeExpr(TableExprDef{std::move(tableDef)});
    }

    if (match(Token::Type::FUNCTION))
    {
        const char *context = "function";

        expect(Token::Type::LEFT_PAREN, context);
        auto [args, isVarArg] = parseParams();
        expect(Token::Type::RIGHT_PAREN, context);

        std::vector<StmtWithPos> stmts = parseBlock();
        expect(Token::Type::END, context);

        return makeExpr(FunctionExpr{isVarArg, std::move(args), std::move(stmts)});
    }

    return parsePostFix();
}

ExprHandle Parser::parsePostFix()
{
    ExprHandle expr = parsePrimary();

    for(;;)
    {
        if (checkAny(Token::Type::LEFT_PAREN, Token::Type::STRING_LITERAL, Token::Type::LEFT_BRACE))
        {
            std::vector<ExprHandle> args = parseArgs();
            expr = makeExpr(CallExpr{std::move(expr), std::move(args)});
        }
        else if (match(Token::Type::COLON))
        {
            const char *context = "method call";

            std::string field = expectIdentifier(context);

            std::vector<ExprHandle> args = parseArgs();

            expr = makeExpr(MethodAccessExpr{std::move(expr), std::move(field), std::move(args)});
        }
        else if (match(Token::Type::DOT))
        {
            const char *context = "field access";

            std::string field = expectIdentifier(context);
            expr = makeExpr(FieldAccessExpr{std::move(expr), std::move(field)});
        }
        else if (match(Token::Type::LEFT_BRACKET))
        {
            const char *context = "array access";

            ExprHandle index = parseExpression();
            expect(Token::Type::RIGHT_BRACKET, context);
            expr = makeExpr(IndexExpr{std::move(expr), std::move(index)});
        }
        else
        {
            return expr;
        }
    }
}

ExprHandle Parser::parsePrimary()
{
    if (match(Token::Type::LEFT_PAREN))
    {
        const char *context = "expression";

        ExprHandle expr = parseExpression();
        expect(Token::Type::RIGHT_PAREN, context);
        return expr;
    }

    return makeExpr(VariableExpr{expectIdentifier("expression")});
}

std::vector<TableExpr> Parser::parseTable()
{
    const char *context = "table constructor";

    std::vector<TableExpr> tableExprs;

    if (check(Token::Type::RIGHT_BRACE))
    {
        return tableExprs;
    }

    do
    {
        if (check(Token::Type::RIGHT_BRACE)) // Allow trailing commas/semicolons
        {
            break;
        }

        if (match(Token::Type::LEFT_BRACKET))
        {
            ExprHandle name = parseExpression();
            expect(Token::Type::RIGHT_BRACKET, context);

            expect(Token::Type::OP_ASSIGN, context);
            ExprHandle value = parseExpression();

            tableExprs.emplace_back(std::move(name), std::move(value), TableExpr::Kind::General);
        }
        else if (check(Token::Type::IDENTIFIER) && checkNext(Token::Type::OP_ASSIGN))
        {
            std::string identifier = expectIdentifier(context);
            advance(); // Consume assignment

            ExprHandle value = parseExpression();
            tableExprs.emplace_back(makeExpr(StringLiteralExpr{std::move(identifier)}), std::move(value), TableExpr::Kind::Record);
        }
        else
        {
            ExprHandle value = parseExpression();
            tableExprs.emplace_back(nullptr, std::move(value), TableExpr::Kind::List);
        }

        if (check(Token::Type::RIGHT_BRACE)) break;
    } while (match(Token::Type::COMMA, Token::Type::SEMICOLON));

    return tableExprs;
}

std::vector<ExprHandle> Parser::parseArgs()
{
    std::vector<ExprHandle> args;

    if (match(Token::Type::LEFT_PAREN))
    {
        const char *context = "call";

        args = parseArgumentList();
        expect(Token::Type::RIGHT_PAREN, context);
    }
    else if (match(Token::Type::STRING_LITERAL))
    {
        args.emplace_back(makeExpr(StringLiteralExpr(Lexer::makeFormattedString(previous().lexeme)))); 
    }
    else if (match(Token::Type::LEFT_BRACE))
    {
        const char *context = "table constructor";

        std::vector<TableExpr> tableDef = parseTable();
        expect(Token::Type::RIGHT_BRACE, context);

        args.emplace_back(makeExpr(TableExprDef{std::move(tableDef)}));
    }
    else 
    {
        parserError("Expected function arguments", peek());
    }

    return args;
}

std::vector<ExprHandle> Parser::parseArgumentList()
{
    std::vector<ExprHandle> args;
    if (check(Token::Type::RIGHT_PAREN))
    {
        return args;
    }

    do
    {
        args.emplace_back(parseExpression());

        if (check(Token::Type::RIGHT_PAREN)) break;
    } while (match(Token::Type::COMMA));

    return args;
}

Parser::Params Parser::parseParams()
{
    std::vector<std::string> args;
    bool isVarArg = false;
    
    if (check(Token::Type::RIGHT_PAREN))
    {
        return {std::move(args), isVarArg};
    }

    do
    {
        if (match(Token::Type::VARARG))
        {
            isVarArg = true;
        }
        else if (match(Token::Type::IDENTIFIER))
        {
            args.emplace_back(previous().lexeme);
        }
        else 
        {
            multiTokenError({Token::Type::VARARG, Token::Type::IDENTIFIER}, "function params");
        }
        
        if (isVarArg) break;
    } while (match(Token::Type::COMMA));

   return {std::move(args), isVarArg};
}
