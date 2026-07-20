#include "parser.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.h"
#include "lexer.h"
#include "token.h"

Ast Parser::parse()
{
    Ast statements = parseBlock();
    expect(TokenType::TEOF, "chunk");

    return statements;
}

void Parser::skipSeparators() { while (match(TokenType::SEMICOLON)); }

StatementHandle Parser::parseStatement(bool *isLast)
{
    switch (peek().type)
    {
        case TokenType::LOCAL:
            return parseLocalStatement();
        case TokenType::DO:
            return parseBlockStatement();
        case TokenType::WHILE:
            return parseWhileStatement();
        case TokenType::REPEAT:
            return parseRepeatStatement();
        case TokenType::FOR:
            return parseForStatement();
        case TokenType::FUNCTION:
            return parseFunctionAssignmentStatement();
        case TokenType::IF:
            return parseIfStatement();
        case TokenType::GOTO:
            return parseGoToStatement();
        case TokenType::RETURN:
            if (isLast) *isLast = true;
            return parseReturnStatement();
        case TokenType::BREAK:
            if (isLast) *isLast = true;
            return parseBreakStatement();
        case TokenType::DOUBLECOLON:
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

    while (match(TokenType::COMMA))
    {
        leftExprs.emplace_back(parsePostFix());
    }   

    expect(TokenType::OP_ASSIGN, context);

    std::vector<ExprHandle> rightExprs;

    do 
    {
        rightExprs.emplace_back(parseExpression());
    } while (match(TokenType::COMMA));


   return makeStmt(AssignmentStmt{std::move(leftExprs), std::move(rightExprs)});
}

StatementHandle Parser::parseLocalStatement()
{
    advance(); // Consume Local Token

    switch (peek().type)
    {
        case TokenType::FUNCTION:
            return parseLocalFunctionAssignmentStatement();
        case TokenType::IDENTIFIER:
            return parseLocalAssignmentStatement();
        default:
            multiTokenError({TokenType::FUNCTION, TokenType::IDENTIFIER}, "local");
    }
}

StatementHandle Parser::parseLocalAssignmentStatement()
{
    const char *context = "local assignment";

    std::vector<std::string> identifiers;

    do
    {
        identifiers.push_back(expectIdentifier(context));
    } while (match(TokenType::COMMA));

    std::vector<ExprHandle> exprs;
    
    if (match(TokenType::OP_ASSIGN))
    {
        do
        {
            exprs.emplace_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    return makeStmt(LocalAssignmentStmt{std::move(identifiers), std::move(exprs)});
}

StatementHandle Parser::parseLocalFunctionAssignmentStatement()
{
    advance(); // Consume Function Token;

    const char *context = "local function assignment";

    std::string identifier = expectIdentifier(context);

    expect(TokenType::LEFT_PAREN, context);
    auto [args, isVarArg] = parseParams();
    expect(TokenType::RIGHT_PAREN, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

    return makeStmt(LocalFunctionAssignmentStmt{std::move(identifier), isVarArg, std::move(args), std::move(stmts)});
}

StatementHandle Parser::parseFunctionAssignmentStatement()
{
    advance(); // Consume Function Token;

    const char *context = "function assignment";

    bool isMethod = false;
    ExprHandle expr = parseFunctionName(isMethod);

    expect(TokenType::LEFT_PAREN, context);
    auto [args, isVarArg] = parseParams();
    expect(TokenType::RIGHT_PAREN, context);

    if (isMethod)
    {
        args.insert(args.begin(), "self");
    }

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

    return makeStmt(FunctionAssignmentStmt{std::move(expr), isVarArg, std::move(args), std::move(stmts)});
}

ExprHandle Parser::parseFunctionName(bool &isMethod)
{
    std::string identifier = expectIdentifier("function");

    ExprHandle expr = makeExpr(VariableExpr{std::move(identifier)});

    while (match(TokenType::DOT))
    {
        std::string field = expectIdentifier("function field");
        expr = makeExpr(FieldAccessExpr{std::move(expr), std::move(field)});
    }

    if (match(TokenType::COLON))
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
    expect(TokenType::DO, context);
    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

    return makeStmt(WhileStmt{std::move(expr), std::move(stmts)});
}

StatementHandle Parser::parseRepeatStatement()
{
    advance(); // Consume Repeat Token
    
    const char *context = "repeat loop";
    
    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::UNTIL, context);
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
        case TokenType::OP_ASSIGN:
            return parseForCountStatement(std::move(identifier));
        case TokenType::COMMA: case TokenType::IN:
            return parseForIteratorStatement(std::move(identifier));
        default:
            multiTokenError({TokenType::IN, TokenType::OP_ASSIGN}, "for loop");
    }
}

StatementHandle Parser::parseForIteratorStatement(std::string firstIdentifier)
{
    const char *context = "for loop";

    std::vector<std::string> variables;
    variables.emplace_back(std::move(firstIdentifier));

    while (match(TokenType::COMMA))
    {
        variables.emplace_back(expectIdentifier(context));
    }

    expect(TokenType::IN, context);

    ExprHandle iterator = parseExpression();

    expect(TokenType::DO, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

    return makeStmt(ForIteratorStmt{std::move(variables), std::move(iterator), std::move(stmts)});
}

StatementHandle Parser::parseForCountStatement(std::string firstIdentifier)
{
    const char *context = "for loop";

    advance(); // Consume Assignment Token

    ExprHandle start = parseExpression();

    expect(TokenType::COMMA, context);
    ExprHandle end = parseExpression();

    ExprHandle step;
    if (match(TokenType::COMMA))
    {
        step = parseExpression();
    }

    expect(TokenType::DO, context);

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

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
    
    expect(TokenType::THEN, context);

    std::vector<StmtWithPos> thenStmts;
    std::vector<StmtWithPos> elseStmts;

    thenStmts = parseBlock();

    if (match(TokenType::ELSE_IF))
    {
        int startLine = peek().line;
        elseStmts.emplace_back(parseIfRestStatement(true), startLine);
    }
    else if (match(TokenType::ELSE))
    {
        elseStmts = parseBlock();

        expect(TokenType::END, "else");
    }
    else
    {
        expect(TokenType::END, context);
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
    if (!isEndBlock() && !check(TokenType::SEMICOLON))
    {
        do
        {
            values.emplace_back(parseExpression());
        } while (match(TokenType::COMMA));
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
    expect(TokenType::DOUBLECOLON, context);
    return makeStmt(LabelStmt{std::move(label), isEndBlock(false)});
}

StatementHandle Parser::parseBlockStatement()
{
    advance(); // Consume Do Token

    const char *context = "do block";

    std::vector<StmtWithPos> stmts = parseBlock();
    expect(TokenType::END, context);

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

std::pair<int, int> Parser::getPrecedence(TokenType op)
{
    switch (op)
    {
        case TokenType::OR:
            return {10, 11};
        case TokenType::AND:
            return {20, 21};
        case TokenType::OP_NOT_EQUAL: 
        case TokenType::OP_EQUAL:
            return {30, 31};
        case TokenType::OP_GREATER:
        case TokenType::OP_GREATER_EQUAL:
        case TokenType::OP_LESS:
        case TokenType::OP_LESS_EQUAL:
            return {40, 41};
        case TokenType::OP_OR:
            return {50, 51};
        case TokenType::OP_NOT:
            return {60, 61};
        case TokenType::OP_AND:
            return {70, 71};
        case TokenType::OP_BITSHIFT_LEFT:
        case TokenType::OP_BITSHIFT_RIGHT:
            return {80, 81};
        case TokenType::OP_CONCAT:  // Right associative
            return {91, 90};
        case TokenType::OP_ADD:
        case TokenType::OP_SUB:
            return {100, 101};
        case TokenType::OP_DIV:
        case TokenType::OP_FLOOR_DIV:
        case TokenType::OP_MOD:
        case TokenType::OP_MUL:
            return {110, 111};
        case TokenType::OP_EXPO:
            return {131, 130}; // Right associative
        default:
            return {-1,-1};
    }
}

std::unordered_map<TokenType, BinaryExpr::BinaryOperator> Parser::binOps = 
{
    {TokenType::OR, BinaryExpr::BinaryOperator::OR},
    {TokenType::AND, BinaryExpr::BinaryOperator::AND},
    {TokenType::OP_NOT_EQUAL, BinaryExpr::BinaryOperator::NEQ},
    {TokenType::OP_EQUAL, BinaryExpr::BinaryOperator::EQ},
    {TokenType::OP_NOT_EQUAL, BinaryExpr::BinaryOperator::NEQ},
    {TokenType::OP_GREATER, BinaryExpr::BinaryOperator::GT},
    {TokenType::OP_GREATER_EQUAL, BinaryExpr::BinaryOperator::GTE},
    {TokenType::OP_LESS, BinaryExpr::BinaryOperator::LS},
    {TokenType::OP_LESS_EQUAL, BinaryExpr::BinaryOperator::LSE},
    {TokenType::OP_OR, BinaryExpr::BinaryOperator::BIT_OR},
    {TokenType::OP_NOT, BinaryExpr::BinaryOperator::BIT_XOR},
    {TokenType::OP_AND, BinaryExpr::BinaryOperator::BIT_AND},
    {TokenType::OP_BITSHIFT_LEFT, BinaryExpr::BinaryOperator::BITSHIFT_LEFT},
    {TokenType::OP_BITSHIFT_RIGHT, BinaryExpr::BinaryOperator::BITSHIFT_RIGHT},
    {TokenType::OP_CONCAT, BinaryExpr::BinaryOperator::CONCAT},
    {TokenType::OP_ADD, BinaryExpr::BinaryOperator::ADD},
    {TokenType::OP_SUB, BinaryExpr::BinaryOperator::SUB},
    {TokenType::OP_DIV, BinaryExpr::BinaryOperator::DIV},
    {TokenType::OP_FLOOR_DIV, BinaryExpr::BinaryOperator::FLOOR_DIV},
    {TokenType::OP_MOD, BinaryExpr::BinaryOperator::MOD},
    {TokenType::OP_MUL, BinaryExpr::BinaryOperator::MUL},
    {TokenType::OP_EXPO, BinaryExpr::BinaryOperator::EXPO}
};

BinaryExpr::BinaryOperator Parser::getBinaryOperator(TokenType op)
{
    auto it = binOps.find(op);
    if (it != binOps.end()) return it->second;
    parserError("Unexpected binary operator!", peek());
}

std::unordered_map<TokenType, UnaryExpr::UnaryOperator> Parser::unaryOps = 
{
    {TokenType::OP_SUB, UnaryExpr::UnaryOperator::NEGATE},
    {TokenType::OP_LENGTH, UnaryExpr::UnaryOperator::LENGTH},
    {TokenType::OP_NOT, UnaryExpr::UnaryOperator::BIT_NOT},
    {TokenType::NOT, UnaryExpr::UnaryOperator::NOT}
};

UnaryExpr::UnaryOperator Parser::getUnaryOperator(TokenType op)
{
    auto it = unaryOps.find(op);
    if (it != unaryOps.end()) return it->second;
    parserError("Unexpected unary operator!", peek());
}

constexpr int UNARY_PRIORITY = 120;

ExprHandle Parser::parseExpression(int minBp)
{
    ExprHandle lhs;
    
    if(match(TokenType::OP_SUB, TokenType::OP_LENGTH, TokenType::OP_NOT, TokenType::NOT))
    {
        TokenWithPos op = previous();
        lhs = makeExpr(UnaryExpr{getUnaryOperator(op.type), parseExpression(UNARY_PRIORITY)});
    }
    else lhs = parseSimple();

    for(;;)
    {
        TokenWithPos op = peek();

        auto [lBp, rBp] = getPrecedence(op.type);
        if (lBp < minBp) break;
        advance(); // Consume operator

        ExprHandle rhs = parseExpression(rBp);
        
        lhs = makeExpr(BinaryExpr{getBinaryOperator(op.type), std::move(lhs), std::move(rhs)});
    }

    return lhs;
}

ExprHandle Parser::parseSimple()
{
    if (match(TokenType::FALSE)) return makeExpr(BoolLiteralExpr{false});
    if (match(TokenType::TRUE)) return makeExpr(BoolLiteralExpr{true});

    if (match(TokenType::NIL)) return makeExpr(NilExpr{});
    if (match(TokenType::VARARG)) return makeExpr(VarArgExpr{});

    if (match(TokenType::STRING_LITERAL)) return makeExpr(StringLiteralExpr{Lexer::makeFormattedString(previous().lexeme)});
    if (match(TokenType::MALFORMED_STRING)) parserError("Unclosed string", previous());

    if (match(TokenType::NUMBER_LITERAL)) 
    {
        std::string number = std::string(previous().lexeme);
        char *end;
        double value = std::strtod(number.c_str(), &end);
        if (end != number.c_str() + number.size()) parserError("Malformed number", previous());
        return makeExpr(NumberLiteralExpr{value});
    }

    if (match(TokenType::LEFT_BRACE))
    {
        const char *context = "table constructor";

        std::vector<TableExpr> tableDef = parseTable();
        expect(TokenType::RIGHT_BRACE, context);
        return makeExpr(TableExprDef{std::move(tableDef)});
    }

    if (match(TokenType::FUNCTION))
    {
        const char *context = "function";

        expect(TokenType::LEFT_PAREN, context);
        auto [args, isVarArg] = parseParams();
        expect(TokenType::RIGHT_PAREN, context);

        std::vector<StmtWithPos> stmts = parseBlock();
        expect(TokenType::END, context);

        return makeExpr(FunctionExpr{isVarArg, std::move(args), std::move(stmts)});
    }

    return parsePostFix();
}

ExprHandle Parser::parsePostFix()
{
    ExprHandle expr = parsePrimary();

    for(;;)
    {
        if (checkAny(TokenType::LEFT_PAREN, TokenType::STRING_LITERAL, TokenType::LEFT_BRACE))
        {
            std::vector<ExprHandle> args = parseArgs();
            expr = makeExpr(CallExpr{std::move(expr), std::move(args)});
        }
        else if (match(TokenType::COLON))
        {
            const char *context = "method call";

            std::string field = expectIdentifier(context);

            std::vector<ExprHandle> args = parseArgs();

            expr = makeExpr(MethodAccessExpr{std::move(expr), std::move(field), std::move(args)});
        }
        else if (match(TokenType::DOT))
        {
            const char *context = "field access";

            std::string field = expectIdentifier(context);
            expr = makeExpr(FieldAccessExpr{std::move(expr), std::move(field)});
        }
        else if (match(TokenType::LEFT_BRACKET))
        {
            const char *context = "array access";

            ExprHandle index = parseExpression();
            expect(TokenType::RIGHT_BRACKET, context);
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
    if (match(TokenType::LEFT_PAREN))
    {
        const char *context = "expression";

        ExprHandle expr = parseExpression();
        expect(TokenType::RIGHT_PAREN, context);
        return expr;
    }

    return makeExpr(VariableExpr{expectIdentifier("expression")});
}

std::vector<TableExpr> Parser::parseTable()
{
    const char *context = "table constructor";

    std::vector<TableExpr> tableExprs;

    if (check(TokenType::RIGHT_BRACE))
    {
        return tableExprs;
    }

    do
    {
        if (check(TokenType::RIGHT_BRACE)) // Allow trailing commas/semicolons
        {
            break;
        }

        if (match(TokenType::LEFT_BRACKET))
        {
            ExprHandle name = parseExpression();
            expect(TokenType::RIGHT_BRACKET, context);

            expect(TokenType::OP_ASSIGN, context);
            ExprHandle value = parseExpression();

            tableExprs.emplace_back(TableExpr{TableExpr::Kind::General, std::move(name), std::move(value)});
        }
        else if (check(TokenType::IDENTIFIER) && checkNext(TokenType::OP_ASSIGN))
        {
            std::string identifier = expectIdentifier(context);
            advance(); // Consume assignment

            ExprHandle value = parseExpression();
            tableExprs.emplace_back(TableExpr{TableExpr::Kind::Record, makeExpr(StringLiteralExpr{std::move(identifier)}), std::move(value)});
        }
        else
        {
            ExprHandle value = parseExpression();
            tableExprs.emplace_back(TableExpr{TableExpr::Kind::List, nullptr, std::move(value)});
        }

        if (check(TokenType::RIGHT_BRACE)) break;
    } while (match(TokenType::COMMA, TokenType::SEMICOLON));

    return tableExprs;
}

std::vector<ExprHandle> Parser::parseArgs()
{
    std::vector<ExprHandle> args;

    if (match(TokenType::LEFT_PAREN))
    {
        const char *context = "call";

        args = parseArgumentList();
        expect(TokenType::RIGHT_PAREN, context);
    }
    else if (match(TokenType::STRING_LITERAL))
    {
        args.emplace_back(makeExpr(StringLiteralExpr(Lexer::makeFormattedString(previous().lexeme)))); 
    }
    else if (match(TokenType::LEFT_BRACE))
    {
        const char *context = "table constructor";

        std::vector<TableExpr> tableDef = parseTable();
        expect(TokenType::RIGHT_BRACE, context);

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
    if (check(TokenType::RIGHT_PAREN))
    {
        return args;
    }

    do
    {
        args.emplace_back(parseExpression());

        if (check(TokenType::RIGHT_PAREN)) break;
    } while (match(TokenType::COMMA));

    return args;
}

ParsedParams Parser::parseParams()
{
    std::vector<std::string> args;
    bool isVarArg = false;
    
    if (check(TokenType::RIGHT_PAREN))
    {
        return {std::move(args), isVarArg};
    }

    do
    {
        if (match(TokenType::VARARG))
        {
            isVarArg = true;
        }
        else if (match(TokenType::IDENTIFIER))
        {
            args.emplace_back(previous().lexeme);
        }
        else 
        {
            multiTokenError({TokenType::VARARG, TokenType::IDENTIFIER}, "function params");
        }
        
        if (isVarArg) break;
    } while (match(TokenType::COMMA));

   return {std::move(args), isVarArg};
}
