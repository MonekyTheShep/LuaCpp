#pragma once

#include <string>
#include <string_view>

enum class TokenType 
{
    // KEYWORDS
    LOCAL,
    GOTO,
    FUNCTION,
    THEN,
    END,
    IN,
    NIL,
    IF,
    ELSE,
    ELSE_IF,
    FOR,
    WHILE,
    REPEAT,
    UNTIL,
    DO,
    AND,
    OR,
    NOT,
    RETURN,
    BREAK,
    IDENTIFIER,

    // TYPES
    STRING_LITERAL,
    MALFORMED_STRING,
    NUMBER_LITERAL,
    FALSE,
    TRUE,

    // ARITHMETIC OPERATORS
    OP_ADD, // +
    OP_SUB, // -
    OP_MUL, // *
    OP_DIV, // /
    OP_FLOOR_DIV, // //
    OP_MOD, // %
    OP_EXPO, // ^

    // ASSIGNMENT OPERATORS
    OP_ASSIGN, // =

    // COMPARISON OPERATORS
    OP_EQUAL, // ==
    OP_NOT_EQUAL, // ~=
    OP_GREATER, // >
    OP_LESS, // <
    OP_GREATER_EQUAL, // >=
    OP_LESS_EQUAL, // <=

    // BITWISE OPERATORS
    OP_AND, // &
    OP_OR, // |
    OP_BITSHIFT_RIGHT, // >>
    OP_BITSHIFT_LEFT, // <<
    OP_NOT, // ~

    // MISC OPERATOR
    OP_LENGTH, // #
    OP_CONCAT, // ..

    // DELIMITERS
    LEFT_PAREN, // (
    RIGHT_PAREN, // )
    LEFT_BRACE, // {
    RIGHT_BRACE, // }
    LEFT_BRACKET, // [
    RIGHT_BRACKET, // ]
    COMMA, // ,
    DOT, // .
    VARARG, // ...
    COLON, // :
    DOUBLECOLON, // ::
    SEMICOLON, // ;

    // MISC
    MALFORMED_COMMENT,
    COMMENT,
    UNKNOWN_CHARACTER,
    TEOF
};

struct TokenValue
{
    TokenType type;
    std::string_view lexeme;
};

struct TokenWithPos
{
    TokenWithPos() = default;
    
    TokenWithPos(TokenValue value, int l, int c)
            : type(value.type)
            , lexeme(value.lexeme)
            , line(l)
            , col(c)
    {}

    TokenType type;
    std::string_view lexeme;

    int line;
    int col;
};

namespace TokenHelper 
{
    inline std::string toString(TokenType type)
    {
        switch (type)
        {
            // KEYWORDS
            case TokenType::LOCAL: return "local";
            case TokenType::GOTO: return "goto";
            case TokenType::FUNCTION: return "function";
            case TokenType::THEN: return "then";
            case TokenType::END: return "end";
            case TokenType::IN: return "in";
            case TokenType::NIL: return "nil";
            case TokenType::IF: return "if";
            case TokenType::ELSE: return "else";
            case TokenType::ELSE_IF: return "elseif";
            case TokenType::FOR: return "for";
            case TokenType::WHILE: return "while";
            case TokenType::REPEAT: return "repeat";
            case TokenType::UNTIL: return "until";
            case TokenType::DO: return "do";
            case TokenType::AND: return "and";
            case TokenType::OR: return "or";
            case TokenType::NOT: return "not";
            case TokenType::RETURN: return "return";
            case TokenType::BREAK: return "break";
            case TokenType::IDENTIFIER: return "<identifier>";

            // TYPES
            case TokenType::STRING_LITERAL: return "<string>";
            case TokenType::MALFORMED_STRING: return "<malformed string>";
            case TokenType::NUMBER_LITERAL: return "<number>";
            case TokenType::FALSE: return "false";
            case TokenType::TRUE: return "true";

            // ARITHMETIC OPERATORS
            case TokenType::OP_ADD: return "+";
            case TokenType::OP_SUB: return "-";
            case TokenType::OP_MUL: return "*";
            case TokenType::OP_DIV: return "/";
            case TokenType::OP_FLOOR_DIV: return "//";
            case TokenType::OP_MOD: return "%";
            case TokenType::OP_EXPO: return "^";

            // ASSIGNMENT OPERATORS
            case TokenType::OP_ASSIGN: return "=";

            // COMPARISON OPERATORS
            case TokenType::OP_EQUAL: return "==";
            case TokenType::OP_NOT_EQUAL: return "~=";
            case TokenType::OP_GREATER: return ">";
            case TokenType::OP_LESS: return "<";
            case TokenType::OP_GREATER_EQUAL: return ">=";
            case TokenType::OP_LESS_EQUAL: return "<=";

            // BITWISE OPERATORS
            case TokenType::OP_AND: return "&";
            case TokenType::OP_OR: return "|";
            case TokenType::OP_BITSHIFT_RIGHT: return ">>";
            case TokenType::OP_BITSHIFT_LEFT: return "<<";
            case TokenType::OP_NOT: return "~";

            // MISC OPERATOR
            case TokenType::OP_LENGTH: return "#";
            case TokenType::OP_CONCAT: return "..";

            // DELIMITERS
            case TokenType::LEFT_PAREN: return "(";
            case TokenType::RIGHT_PAREN: return ")";
            case TokenType::LEFT_BRACE: return "{";
            case TokenType::RIGHT_BRACE: return "}";
            case TokenType::LEFT_BRACKET: return "[";
            case TokenType::RIGHT_BRACKET: return "]";
            case TokenType::COMMA: return ",";
            case TokenType::DOT: return ".";
            case TokenType::VARARG: return "...";
            case TokenType::COLON: return ":";
            case TokenType::SEMICOLON: return ";";
            case TokenType::DOUBLECOLON: return "::";
            
            case TokenType::MALFORMED_COMMENT: return "<malformed comment>";
            case TokenType::COMMENT: return "<comment>";
            case TokenType::TEOF: return "<eof>";
            default:
                return "<unknown>";
        }
    }
}