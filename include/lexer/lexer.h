#pragma once

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "token.h"

class Lexer 
{
    public:
        Lexer(std::string query)
        : query(std::move(query))
        , pos(0)
        , col(1)
        , line(1)
        {}

        Token nextToken();
        Token lookAheadToken();

        static std::string makeFormattedString(std::string_view lexeme);

        static std::string getTokenTypeName(const Token::Type &type);
    private:
        std::optional<Token> lookAhead;

        std::string query;

        size_t pos;
        int col;
        int line;
    private:
        // -----------------------------
        // Token Functions
        // -----------------------------
        Token lex();

        Token::Value readComment();
        
        std::optional<Token::Type> resolveKeyword(std::string_view lexeme);
        Token::Value readIdentifier();

        int isLongStringSequence();
        int countEquals();
        Token::Value readLongString(int sep, Token::Type type, Token::Type broken);

        Token::Value readString();

        Token::Value readNumber();

        Token::Value readOperatorAndDelimiter();

        // -----------------------------
        // Helper Functions
        // -----------------------------
        static bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

        static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

        static bool isDigit(char c) { return c >= '0' && c <= '9'; }
        
        static bool isHex(char c) { return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');}

        static bool isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c);}

        bool isEof() { return pos >= query.length(); }

        bool nextEqual2(const char *set)
        {
            assert(set[2] == '\0');
            if (set[0] == peek() || set[1] == peek())
            {
                advance(); // Consume set
                return true;
            }

            return false;
        }

        // -----------------------------
        // Traversal Functions
        // -----------------------------
        char peek()
        {
            if (isEof()) return EOF;
            return query[pos];
        }

        char peekBy(size_t by)
        {
            if (pos + by >= query.length()) return EOF;
            return query[pos + by];
        }

        char advance()
        {
            if (isEof()) return EOF;
            col++;
            return query[pos++];
        }

        void incLine() 
        {
            advance();
            line++;
            col = 1;
        }

        void skipWhiteSpace() { while (!isEof() && isWhitespace(peek())) advance(); }
};