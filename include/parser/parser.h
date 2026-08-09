#pragma once

#include <concepts>
#include <cstddef>
#include <exception>
#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.h"
#include "lexer/token.h"
#include "lexer/lexer.h"

class ParserError : public std::exception 
{
    public:
        ParserError(std::string msg)
            : message(std::move(msg)) 
            {}

        const char* what() const noexcept 
        {
            return message.c_str();
        }
    private:
        std::string message;
};

class Parser 
{
    public:
        Parser(std::string query)
        : lexer(std::move(query))
        {
            advance();
        }

        Ast parse();
    private:
        static std::unordered_map<Token::Type, BinaryExpr::BinaryOperator> binOps;
        static std::unordered_map<Token::Type, UnaryExpr::UnaryOperator> unaryOps;

        Lexer lexer;
        Token currentToken;
        Token previousToken;
    private:
        // -----------------------------
        // Error Functions
        // -----------------------------
        [[noreturn]] void parserError(std::string_view error, const Token &token) 
        { 
            throw ParserError(std::format("[Line {}, Col {}] {}!", token.line, token.col, error)); 
        }

        std::string makeContextError(std::initializer_list<Token::Type> tokens, Token::Type failedToken, const char *context)
        {
            std::string tokensExpected;

            for (auto it = tokens.begin(); it < tokens.end(); it++)
            {
                tokensExpected += std::format("`{}`", Token::toString(*it));
                if (it < tokens.end() - 1)
                {
                    tokensExpected += " or ";
                }
            }

            std::string failedTokenString = Token::toString(failedToken);

            if (context) return std::format("Expected {} got `{}` while parsing {}", tokensExpected, failedTokenString, context);
            else return std::format("Expected {} got `{}`", tokensExpected, failedTokenString);
        }

        [[noreturn]] void multiTokenError(std::initializer_list<Token::Type> tokens, const char *context)
        {
            parserError(makeContextError(tokens, peek().type, context), peek());
        }

        // -----------------------------
        // Helper Function
        // -----------------------------
        bool check(Token::Type type) { return peek().type == type; }

        bool checkNext(Token::Type type) { return peekNext().type == type; }

        template<typename... Args>
        requires (std::same_as<Args, Token::Type> && ...)
        bool checkAny(Args... types)
        {
            return ((check(types)) || ...);
        }

        bool isEof() { return (check(Token::Type::TEOF)); }

        bool isEndBlock(bool withUntil = true) // Until doesnt close the scope since it can contain an expression
        {
            switch (peek().type)
            {
                case Token::Type::END: case Token::Type::ELSE: 
                case Token::Type::ELSE_IF: case Token::Type::TEOF:
                    return true;
                case Token::Type::UNTIL: 
                    return withUntil;
                default:
                    return false;
            }
        }
        // -----------------------------
        // Traversal Functions
        // -----------------------------
        void expect(Token::Type type, const char* context)
        {
            if (!check(type)) 
                parserError(makeContextError({type}, peek().type, context), peek());
                
            advance();
        }

        std::string expectIdentifier(const char* context)
        {
            expect(Token::Type::IDENTIFIER, context);
            return std::string(previous().lexeme);
        }

        template<typename... Args>
        requires (std::same_as<Args, Token::Type> && ...)
        bool match(Args... types)
        {
            if ((check(types) || ...))
            {
                advance();
                return true;
            }

            return false;
        }

        Token peek() { return currentToken; }
  
        Token peekNext() 
        { 
            if (isEof()) return currentToken;
            return lexer.lookAheadToken();
        }

        void advance()
        {
            previousToken = currentToken;
            currentToken = lexer.nextToken();

            while (currentToken.type == Token::Type::COMMENT)
            {
                currentToken = lexer.nextToken();
            }
        }

        Token previous() { return previousToken; }

        void skipSeparators();

        // -----------------------------
        // Statement Functions
        // -----------------------------
        StatementHandle parseStatement(bool *isLast);
        bool isCallExpr(const ExprHandle &expr) { return std::holds_alternative<CallExpr>(*expr) || std::holds_alternative<MethodAccessExpr>(*expr); }
        StatementHandle parseAssignment(ExprHandle firstExpr);
        StatementHandle parseLocalStatement();

        LocalAssignmentStmt::VariableAttribute checkVariableAttribute();

        StatementHandle parseLocalAssignmentStatement();
        StatementHandle parseLocalFunctionAssignmentStatement();
        StatementHandle parseFunctionAssignmentStatement();
        ExprHandle parseFunctionName(bool &isMethod);
        StatementHandle parseWhileStatement();
        StatementHandle parseRepeatStatement();
        StatementHandle parseForStatement();
        StatementHandle parseForIteratorStatement(std::string firstIdentifier);
        StatementHandle parseForCountStatement(std::string firstIdentifier);
        StatementHandle parseIfStatement();
        StatementHandle parseIfRestStatement(bool withinElseIf);
        StatementHandle parseGoToStatement();
        StatementHandle parseReturnStatement();
        StatementHandle parseBreakStatement();
        StatementHandle parseLabelStatement();
        StatementHandle parseBlockStatement();
        std::vector<StmtWithPos> parseBlock();

        // -----------------------------
        // Expression Functions
        // -----------------------------
        std::pair<int, int> getPrecedence(Token::Type op);
        BinaryExpr::BinaryOperator getBinaryOperator(Token::Type op);
        UnaryExpr::UnaryOperator getUnaryOperator(Token::Type op);
        ExprHandle parseExpression(int minBp = 0);
        ExprHandle parseSimple();
        ExprHandle parsePostFix();
        ExprHandle parsePrimary();

        std::vector<TableExpr> parseTable();
        std::vector<ExprHandle> parseArgs();
        std::vector<ExprHandle> parseArgumentList();

        struct Params 
        {   
            std::vector<std::string> args;
            bool isVarArg;
        };

        Params parseParams();
};
