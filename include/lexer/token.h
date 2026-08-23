#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct Token
{
    public:
        Token() = default;

        struct Value;

        Token(Value value, int line, int col)
            : lexeme(value.lexeme)
            , type(value.type)
            , line(line)
            , col(col) {}

        enum class Type : uint8_t;

        static std::string toString(Type type)
        {
            switch (type)
            {
                // KEYWORDS
                case Type::LOCAL: return "local";
                case Type::GOTO: return "goto";
                case Type::FUNCTION: return "function";
                case Type::THEN: return "then";
                case Type::END: return "end";
                case Type::IN: return "in";
                case Type::NIL: return "nil";
                case Type::IF: return "if";
                case Type::ELSE: return "else";
                case Type::ELSE_IF: return "elseif";
                case Type::FOR: return "for";
                case Type::WHILE: return "while";
                case Type::REPEAT: return "repeat";
                case Type::UNTIL: return "until";
                case Type::DO: return "do";
                case Type::AND: return "and";
                case Type::OR: return "or";
                case Type::NOT: return "not";
                case Type::RETURN: return "return";
                case Type::BREAK: return "break";
                case Type::IDENTIFIER: return "<identifier>";

                // TYPES
                case Type::STRING_LITERAL: return "<string>";
                case Type::MALFORMED_STRING: return "<malformed string>";
                case Type::NUMBER_LITERAL: return "<number>";
                case Type::FALSE: return "false";
                case Type::TRUE: return "true";

                // ARITHMETIC OPERATORS
                case Type::OP_ADD: return "+";
                case Type::OP_SUB: return "-";
                case Type::OP_MUL: return "*";
                case Type::OP_DIV: return "/";
                case Type::OP_FLOOR_DIV: return "//";
                case Type::OP_MOD: return "%";
                case Type::OP_EXPO: return "^";

                // ASSIGNMENT OPERATORS
                case Type::OP_ASSIGN: return "=";

                // COMPARISON OPERATORS
                case Type::OP_EQUAL: return "==";
                case Type::OP_NOT_EQUAL: return "~=";
                case Type::OP_GREATER: return ">";
                case Type::OP_LESS: return "<";
                case Type::OP_GREATER_EQUAL: return ">=";
                case Type::OP_LESS_EQUAL: return "<=";

                // BITWISE OPERATORS
                case Type::OP_AND: return "&";
                case Type::OP_OR: return "|";
                case Type::OP_BITSHIFT_RIGHT: return ">>";
                case Type::OP_BITSHIFT_LEFT: return "<<";
                case Type::OP_NOT: return "~";

                // MISC OPERATOR
                case Type::OP_LENGTH: return "#";
                case Type::OP_CONCAT: return "..";

                // DELIMITERS
                case Type::LEFT_PAREN: return "(";
                case Type::RIGHT_PAREN: return ")";
                case Type::LEFT_BRACE: return "{";
                case Type::RIGHT_BRACE: return "}";
                case Type::LEFT_BRACKET: return "[";
                case Type::RIGHT_BRACKET: return "]";
                case Type::COMMA: return ",";
                case Type::DOT: return ".";
                case Type::VARARG: return "...";
                case Type::COLON: return ":";
                case Type::SEMICOLON: return ";";
                case Type::DOUBLECOLON: return "::";
                
                case Type::MALFORMED_COMMENT: return "<malformed comment>";
                case Type::COMMENT: return "<comment>";
                case Type::TEOF: return "<eof>";
                default:
                    return "<unknown>";
            }
        }

        enum class Type : uint8_t
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

        struct Value
        {
            Type type;
            std::string_view lexeme;
        };


    public:
        std::string_view lexeme;
        Type type;
        int line;
        int col;
};