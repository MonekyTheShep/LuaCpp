#include "lexer.h"

#include <cassert>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "token.h"

std::unordered_map<TokenType, std::string_view> Lexer::tokenNames 
{
    {TokenType::TEOF, "EOF"},
    {TokenType::COMMENT, "COMMENT"},
    {TokenType::MALFORMED_COMMENT, "MALFORMED_COMMENT"},
    {TokenType::LOCAL, "LOCAL"},
    {TokenType::GOTO, "GOTO"},
    {TokenType::FUNCTION, "FUNCTION"},
    {TokenType::THEN, "THEN"},
    {TokenType::END, "END"},
    {TokenType::IN, "IN"},
    {TokenType::NIL, "NIL"},
    {TokenType::FALSE, "FALSE"},
    {TokenType::TRUE, "TRUE"},
    {TokenType::IF, "IF"},
    {TokenType::ELSE, "ELSE"},
    {TokenType::ELSE_IF, "ELSE_IF"},
    {TokenType::FOR, "FOR"},
    {TokenType::WHILE, "WHILE"},
    {TokenType::REPEAT, "REPEAT"},
    {TokenType::UNTIL, "UNTIL"},
    {TokenType::DO, "DO"},
    {TokenType::AND, "AND"},
    {TokenType::OR, "OR"},
    {TokenType::NOT, "NOT"},
    {TokenType::RETURN, "RETURN"},
    {TokenType::BREAK, "BREAK"},

    {TokenType::STRING_LITERAL, "STRING_LITERAL"},
    {TokenType::MALFORMED_STRING, "MALFORMED_STRING"},
    {TokenType::NUMBER_LITERAL, "INTEGER_LITERAL"},

    {TokenType::OP_ADD, "OP_ADD"},
    {TokenType::OP_SUB, "OP_SUB"},
    {TokenType::OP_MUL, "OP_MUL"},
    {TokenType::OP_DIV, "OP_DIV"},
    {TokenType::OP_FLOOR_DIV, "OP_FLOOR_DIV"},
    {TokenType::OP_EXPO, "OP_EXPO"},

    {TokenType::OP_ASSIGN, "OP_ASSIGN"},

    {TokenType::OP_EQUAL, "OP_EQUAL"},
    {TokenType::OP_NOT_EQUAL, "OP_NOT_EQUAL"},
    {TokenType::OP_GREATER_EQUAL, "OP_GREAT_EQUAL"},
    {TokenType::OP_LESS_EQUAL, "OP_LESS_EQUAL"},

    {TokenType::OP_AND, "OP_AND"},
    {TokenType::OP_OR, "OP_OR"},
    {TokenType::OP_BITSHIFT_RIGHT, "OP_BITSHIFT_RIGHT"},
    {TokenType::OP_BITSHIFT_LEFT, "OP_BITSHIFT_LEFT"},
    {TokenType::OP_NOT, "OP_NOT"},

    {TokenType::OP_CONCAT, "CONCAT"},
    {TokenType::OP_LENGTH, "OP_LENGTH"},

    {TokenType::LEFT_PAREN, "LEFT_PAREN"},
    {TokenType::RIGHT_PAREN, "RIGHT_PAREN"},
    {TokenType::LEFT_BRACE, "LEFT_BRACE"},
    {TokenType::RIGHT_BRACE, "RIGHT_BRACE"},
    {TokenType::LEFT_BRACKET, "LEFT_BRACKET"},
    {TokenType::RIGHT_BRACKET, "RIGHT_BRACKET"},
    {TokenType::OP_GREATER, "OP_GREATER"},
    {TokenType::OP_LESS, "OP_LESS"},
    {TokenType::COMMA, "COMMA"},
    {TokenType::DOT, "DOT"},
    {TokenType::VARARG, "VARARG"},
    {TokenType::COLON, "COLON"},
    {TokenType::DOUBLECOLON, "DOUBLECOLON"},
    {TokenType::SEMICOLON, "SEMICOLON"},

    {TokenType::IDENTIFIER, "IDENTIFIER"},
};

std::string Lexer::getTokenTypeName(const TokenType &type)
{
    auto it = tokenNames.find(type);
    return it == tokenNames.end() ? "UNKNOWN" : std::string(it->second);
}

std::optional<TokenType> Lexer::resolveKeyword(std::string_view lexeme)
{
    if (lexeme == "local") return TokenType::LOCAL;
    if (lexeme == "goto") return TokenType::GOTO;
    if (lexeme == "function") return TokenType::FUNCTION;
    if (lexeme == "then") return TokenType::THEN;
    if (lexeme == "end") return TokenType::END;
    if (lexeme == "in") return TokenType::IN;

    if (lexeme == "nil") return TokenType::NIL;
    if (lexeme == "false") return TokenType::FALSE;
    if (lexeme == "true") return TokenType::TRUE;

    if (lexeme == "if") return TokenType::IF;
    if (lexeme == "else") return TokenType::ELSE;
    if (lexeme == "elseif") return TokenType::ELSE_IF;

    if (lexeme == "for") return TokenType::FOR;
    if (lexeme == "while") return TokenType::WHILE;
    if (lexeme == "repeat") return TokenType::REPEAT;
    if (lexeme == "until") return TokenType::UNTIL;
    if (lexeme == "do") return TokenType::DO;

    if (lexeme == "and") return TokenType::AND;
    if (lexeme == "or") return TokenType::OR;
    if (lexeme == "not") return TokenType::NOT;

    if (lexeme == "return") return TokenType::RETURN;
    if (lexeme == "break") return TokenType::BREAK;

    return std::nullopt;
}

TokenWithPos Lexer::nextToken()
{
    if (lookAhead) 
        return std::exchange(lookAhead, std::nullopt).value();
    
    return lex();
}

TokenWithPos Lexer::lookAheadToken()
{
    if (lookAhead) return *lookAhead;
    lookAhead = lex();
    return *lookAhead;
}

TokenWithPos Lexer::lex()
{
    for(;;)
    {
        skipWhiteSpace();

        int startLine = line;
        int startCol = col;
        
        if (isEof()) return {{TokenType::TEOF, std::string_view()}, startLine, startCol};

        size_t startPos = pos;
        char currentChar = peek();

        if (currentChar == '\n')
        {
            incLine(); 
            continue;
        }
        else if (currentChar == '-')
        {
            advance();
            if (peek() == '-')
            {
                return {readComment(), startLine, startCol};
            }
            return {{TokenType::OP_SUB, "-"}, startLine, startCol};
        }
        else if (isAlpha(currentChar) || currentChar == '_')
        {
            return {readIdentifier(), startLine, startCol};
        }
        else if (isDigit(currentChar))
        {
            return {readNumber(), startLine, startCol};
        }
        else if (currentChar == '\'' || currentChar == '"')
        {
            return {readString(), startLine, startCol};
        }
        else if (currentChar == '[')
        {
            int prefix = isLongStringSequence();
            if (prefix >= 2)
            {
                return {{readLongString(prefix, TokenType::STRING_LITERAL, TokenType::MALFORMED_STRING)}, startLine, startCol};
            }
            else if (prefix == 0)
            {
                return {{TokenType::MALFORMED_STRING, std::string_view(query).substr(startPos, pos - startPos)}, startLine, startCol};
            }
            else 
            {
                return {{TokenType::LEFT_BRACKET, "]"}, startLine, startCol};
            }
        }

        return {readOperatorAndDelimiter(), startLine, startCol};
    }
}

TokenValue Lexer::readComment()
{
    advance(); // Consume '-' 
    size_t start = pos;

    if (peek() == '[')
    {
        int prefix = isLongStringSequence();

        if (prefix >= 2)
        {
            return readLongString(prefix, TokenType::COMMENT, TokenType::MALFORMED_COMMENT);
        }
    }

    while (!isEof() && peek() != '\n') // Read until new line
    {
        advance();
    }

    return {TokenType::COMMENT, std::string_view(query).substr(start, pos - start)};
}

TokenValue Lexer::readIdentifier()
{
    size_t start = pos;

    while (!isEof() && (isAlphaNumeric(peek()) || peek() == '_'))
    {
        advance();
    }

    std::string_view lexeme = std::string_view(query).substr(start, pos - start);

    if(auto keyword = resolveKeyword(lexeme)) 
        return {*keyword, lexeme};

    return {TokenType::IDENTIFIER, lexeme};
}

/*
    If there is a sequence itll return 2 or more. 
    If its an invalid itll return 0
    else return 1 for just a single '['
*/
int Lexer::isLongStringSequence()
{
    char c = advance();
    int numOfEqual = countEquals();

    if (c == peek()) return numOfEqual + 2; 

    return (numOfEqual == 0) ? 1 : 0; 
}

int Lexer::countEquals() 
{
    int equals = 0;
    while (peek() == '=')
    {
        equals++;
        advance(); // Consume '='
    }
    return equals;
}

TokenValue Lexer::readLongString(int sep, TokenType type, TokenType broken)
{
    advance(); // Consume long string 2nd starting '['
    size_t start = pos;
   
    if (peek() == '\n') incLine(); // Skip first new line

    while (!isEof())
    {
        if (peek() == ']')
        {
            size_t endPos = pos;

            if (isLongStringSequence() != sep) continue;

            advance(); // Consume long string 2nd ending ']'
            
            return {type, std::string_view(query).substr(start, endPos - start)};
        }
        else if (peek() == '\n')
        {
            incLine();
        } else advance();
    }

    return {broken, std::string_view(query).substr(start, pos - start)};
}

TokenValue Lexer::readString()
{
    char quote = advance(); // Consume starting quote
    size_t start = pos;

    while (!isEof() && peek() != '\n' && peek() != quote)
    {
        if (peek() == '\\')
        {
            advance(); // Consume backslash
            if (isEof() || peek() == '\n') continue;
            char escaped = advance(); // Consume escape character

            switch (escaped)
            {
                case 'z':
                {
                    while (isspace(peek()))
                    {
                        if (peek() == '\n') incLine();
                        else advance();
                    }
                    break;
                }
                default:
                    continue;
            }
        }
        else advance();
    }

    size_t endPos = pos;

    if (peek() != quote) return {TokenType::MALFORMED_STRING, std::string_view(query).substr(start, endPos - start)};
    
    advance(); // Consume ending quote
    
    return {TokenType::STRING_LITERAL, std::string_view(query).substr(start, endPos - start)};
}

std::string Lexer::makeFormattedString(std::string_view lexeme)
{
    std::string content;

    for (size_t i = 0; i < lexeme.size();)
    {
        if (lexeme[i] != '\\')
        {
            content += lexeme[i++];
            continue;
        }


        assert(i + 2 <= lexeme.size());
        i += 2;
        char escaped = lexeme[i - 1];
        switch (escaped) // Should I add hex, decimal and utf escape sequence?
        {
            case 'a': content += '\a'; break;
            case 'b': content += '\b'; break;
            case 'f': content += '\f'; break;
            case 'n': content += '\n'; break;
            case 't': content += '\t'; break;
            case 'r': content += '\r'; break;
            case 'v': content += '\v'; break;
            case 'z':
            {
                while (isspace(lexeme[i])) i++;
                break;
            }
            default:
                content += escaped;
        }
    }

    return content;
}

TokenValue Lexer::readNumber() 
{
    size_t start = pos;

    const char *expo = "eE";

    char first = advance();

    if (first == '0' && nextEqual2("xX"))
    {
        expo = "pP";
    }

    while (!isEof())
    {
        if (nextEqual2(expo))
        {
            nextEqual2("-+");
        }
        else if (peek() == '.' || (isHex(peek())))
        {
            advance();
        }
        else break;
    }

    return {TokenType::NUMBER_LITERAL, std::string_view(query).substr(start, pos - start)};
}

TokenValue Lexer::readOperatorAndDelimiter()
{
    size_t start = pos;
    char c = advance();

    switch (c)
    {
        case '(':
            return {TokenType::LEFT_PAREN, "("};
        case ')':
            return {TokenType::RIGHT_PAREN, ")"};
        case '{':
            return {TokenType::LEFT_BRACE, "{"};
        case '}':
            return {TokenType::RIGHT_BRACE, "}"};     
        case ']':
            return {TokenType::RIGHT_BRACKET, "]"};
        case ',':
            return {TokenType::COMMA, ","};
        case '.':
        {
            if (peek() != '.') return {TokenType::DOT, "."};
            advance();
            if (peek() != '.') return {TokenType::OP_CONCAT, ".."};
            advance();
            return {TokenType::VARARG, "..."};
        }
        case ':':
            if (peek() != ':') return {TokenType::COLON, ":"};
            advance();
            return {TokenType::DOUBLECOLON, "::"};
        case ';':
            return {TokenType::SEMICOLON, ";"};
        case '+':
            return {TokenType::OP_ADD, "+"};
        case '*':
            return {TokenType::OP_MUL, "*"};
        case '/':
        {
            if (peek() != '/') return {TokenType::OP_DIV, "/"};
            advance();
            return {TokenType::OP_FLOOR_DIV, "//"};
        }
        case '%':
            return {TokenType::OP_MOD, "%"};
        case '^':
            return {TokenType::OP_EXPO, "^"};
        case '&':
            return {TokenType::OP_AND, "&"};
        case '|':
            return {TokenType::OP_OR, "|"};
        case '~':
        {
            if (peek() != '=') return {TokenType::OP_NOT, "~"};
            advance();
            return {TokenType::OP_NOT_EQUAL, "~="};
        }
        case '>':
        {
            if (peek() == '=')
            {
                advance();
                return {TokenType::OP_GREATER_EQUAL, ">="};
            }
            if (peek() == '>')
            {
                advance();
                return {TokenType::OP_BITSHIFT_RIGHT, ">>"};
            }
            return {TokenType::OP_GREATER, ">"};
        }
        case '<':
        {
            if (peek() == '=')
            {
                advance();
                return {TokenType::OP_LESS_EQUAL, "<="};
            }
            if (peek() == '<')
            {
                advance();
                return {TokenType::OP_BITSHIFT_LEFT, "<<"};
            }
            return {TokenType::OP_LESS, "<"};
        }
        case '#':
            return {TokenType::OP_LENGTH, "#"};
        case '=':
        {
            if (peek() != '=') return {TokenType::OP_ASSIGN, "="};

            advance();
            return {TokenType::OP_EQUAL, "=="};
        }

        default:
            return {TokenType::UNKNOWN_CHARACTER, std::string_view(query).substr(start, 1)};
    }
}
