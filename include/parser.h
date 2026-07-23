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

#include "token.h"
#include "ast.h"
#include "lexer.h"

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
        static std::unordered_map<TokenType, BinaryExpr::BinaryOperator> binOps;
        static std::unordered_map<TokenType, UnaryExpr::UnaryOperator> unaryOps;

        Lexer lexer;
        TokenWithPos currentToken;
        TokenWithPos previousToken;
    private:
        // -----------------------------
        // Helper Functions
        // -----------------------------
        [[noreturn]] void parserError(std::string_view error, const TokenWithPos &token) 
        { 
            throw ParserError(std::format("[Line {}, Col {}] {}!", token.line, token.col, error)); 
        }

        bool check(TokenType type) { return peek().type == type; }

        bool checkNext(TokenType type) { return peekNext().type == type; }

        template<typename... Args>
        requires (std::same_as<Args, TokenType> && ...)
        bool checkAny(Args... types)
        {
            return ((check(types)) || ...);
        }

        bool isEof() { return (check(TokenType::TEOF)); }

        bool isEndBlock(bool withUntil = true) // Until doesnt close the scope since it can contain an expression
        {
            switch (peek().type)
            {
                case TokenType::END: case TokenType::ELSE: 
                case TokenType::ELSE_IF: case TokenType::TEOF:
                    return true;
                case TokenType::UNTIL: 
                    return withUntil;
                default:
                    return false;
            }
        }

        std::string makeContextError(std::initializer_list<TokenType> tokens, TokenType failedToken, const char *context)
        {
            std::string tokensExpected;

            auto it = tokens.begin();

            for (size_t i = 0; i < tokens.size(); i++)
            {
                tokensExpected += std::format("`{}`", TokenHelper::toString(it[i]));
                if (i < tokens.size() - 1)
                {
                    tokensExpected += " or ";
                }
            }

            std::string failedTokenString = TokenHelper::toString(failedToken);

            if (context) return std::format("Expected {} got `{}` while parsing {}", tokensExpected, failedTokenString, context);
            else return std::format("Expected {} got `{}`", tokensExpected, failedTokenString);
        }

        [[noreturn]] void multiTokenError(std::initializer_list<TokenType> tokens, const char *context)
        {
            parserError(makeContextError(tokens, peek().type, context), peek());
        }

        // -----------------------------
        // Traversal Functions
        // -----------------------------
        void expect(TokenType type, const char* context)
        {
            if (!check(type)) 
                parserError(makeContextError({type}, peek().type, context), peek());
                
            advance();
        }

        std::string expectIdentifier(const char* context)
        {
            expect(TokenType::IDENTIFIER, context);
            return std::string(previous().lexeme);
        }

        template<typename... Args>
        requires (std::same_as<Args, TokenType> && ...)
        bool match(Args... types)
        {
            if ((check(types) || ...))
            {
                advance();
                return true;
            }

            return false;
        }

        TokenWithPos peek() { return currentToken; }
  
        TokenWithPos peekNext() 
        { 
            if (isEof()) return currentToken;
            return lexer.lookAheadToken();
        }

        void advance()
        {
            previousToken = currentToken;
            currentToken = lexer.nextToken();

            while (currentToken.type == TokenType::COMMENT)
            {
                currentToken = lexer.nextToken();
            }
        }

        TokenWithPos previous() { return previousToken; }

        void skipSeparators();

        // -----------------------------
        // Statement Functions
        // -----------------------------
        StatementHandle parseStatement(bool *isLast);
        bool isCallExpr(const ExprHandle &expr) { return std::holds_alternative<CallExpr>(*expr) || std::holds_alternative<MethodAccessExpr>(*expr); }
        StatementHandle parseAssignment(ExprHandle firstExpr);
        StatementHandle parseLocalStatement();
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
        std::pair<int, int> getPrecedence(TokenType op);
        BinaryExpr::BinaryOperator getBinaryOperator(TokenType op);
        UnaryExpr::UnaryOperator getUnaryOperator(TokenType op);
        ExprHandle parseExpression(int minBp = 0);
        ExprHandle parseSimple();
        ExprHandle parsePostFix();
        ExprHandle parsePrimary();

        std::vector<TableExpr> parseTable();
        std::vector<ExprHandle> parseArgs();
        std::vector<ExprHandle> parseArgumentList();

        struct ParsedParams 
        {   
            std::vector<std::string> args;
            bool isVarArg;
        };

        ParsedParams parseParams();
};
