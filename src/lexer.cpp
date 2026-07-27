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

std::unordered_map<Token::Type, std::string_view> Lexer::tokenNames 
{
    {Token::Type::TEOF, "EOF"},
    {Token::Type::COMMENT, "COMMENT"},
    {Token::Type::MALFORMED_COMMENT, "MALFORMED_COMMENT"},
    {Token::Type::LOCAL, "LOCAL"},
    {Token::Type::GOTO, "GOTO"},
    {Token::Type::FUNCTION, "FUNCTION"},
    {Token::Type::THEN, "THEN"},
    {Token::Type::END, "END"},
    {Token::Type::IN, "IN"},
    {Token::Type::NIL, "NIL"},
    {Token::Type::FALSE, "FALSE"},
    {Token::Type::TRUE, "TRUE"},
    {Token::Type::IF, "IF"},
    {Token::Type::ELSE, "ELSE"},
    {Token::Type::ELSE_IF, "ELSE_IF"},
    {Token::Type::FOR, "FOR"},
    {Token::Type::WHILE, "WHILE"},
    {Token::Type::REPEAT, "REPEAT"},
    {Token::Type::UNTIL, "UNTIL"},
    {Token::Type::DO, "DO"},
    {Token::Type::AND, "AND"},
    {Token::Type::OR, "OR"},
    {Token::Type::NOT, "NOT"},
    {Token::Type::RETURN, "RETURN"},
    {Token::Type::BREAK, "BREAK"},

    {Token::Type::STRING_LITERAL, "STRING_LITERAL"},
    {Token::Type::MALFORMED_STRING, "MALFORMED_STRING"},
    {Token::Type::NUMBER_LITERAL, "INTEGER_LITERAL"},

    {Token::Type::OP_ADD, "OP_ADD"},
    {Token::Type::OP_SUB, "OP_SUB"},
    {Token::Type::OP_MUL, "OP_MUL"},
    {Token::Type::OP_DIV, "OP_DIV"},
    {Token::Type::OP_FLOOR_DIV, "OP_FLOOR_DIV"},
    {Token::Type::OP_EXPO, "OP_EXPO"},

    {Token::Type::OP_ASSIGN, "OP_ASSIGN"},

    {Token::Type::OP_EQUAL, "OP_EQUAL"},
    {Token::Type::OP_NOT_EQUAL, "OP_NOT_EQUAL"},
    {Token::Type::OP_GREATER_EQUAL, "OP_GREAT_EQUAL"},
    {Token::Type::OP_LESS_EQUAL, "OP_LESS_EQUAL"},

    {Token::Type::OP_AND, "OP_AND"},
    {Token::Type::OP_OR, "OP_OR"},
    {Token::Type::OP_BITSHIFT_RIGHT, "OP_BITSHIFT_RIGHT"},
    {Token::Type::OP_BITSHIFT_LEFT, "OP_BITSHIFT_LEFT"},
    {Token::Type::OP_NOT, "OP_NOT"},

    {Token::Type::OP_CONCAT, "CONCAT"},
    {Token::Type::OP_LENGTH, "OP_LENGTH"},

    {Token::Type::LEFT_PAREN, "LEFT_PAREN"},
    {Token::Type::RIGHT_PAREN, "RIGHT_PAREN"},
    {Token::Type::LEFT_BRACE, "LEFT_BRACE"},
    {Token::Type::RIGHT_BRACE, "RIGHT_BRACE"},
    {Token::Type::LEFT_BRACKET, "LEFT_BRACKET"},
    {Token::Type::RIGHT_BRACKET, "RIGHT_BRACKET"},
    {Token::Type::OP_GREATER, "OP_GREATER"},
    {Token::Type::OP_LESS, "OP_LESS"},
    {Token::Type::COMMA, "COMMA"},
    {Token::Type::DOT, "DOT"},
    {Token::Type::VARARG, "VARARG"},
    {Token::Type::COLON, "COLON"},
    {Token::Type::DOUBLECOLON, "DOUBLECOLON"},
    {Token::Type::SEMICOLON, "SEMICOLON"},

    {Token::Type::IDENTIFIER, "IDENTIFIER"},
};

std::string Lexer::getTokenTypeName(const Token::Type &type)
{
    auto it = tokenNames.find(type);
    return it == tokenNames.end() ? "UNKNOWN" : std::string(it->second);
}

Token Lexer::nextToken()
{
    if (lookAhead) 
        return std::exchange(lookAhead, std::nullopt).value();
    
    return lex();
}

Token Lexer::lookAheadToken()
{
    if (lookAhead) return *lookAhead;
    lookAhead = lex();
    return *lookAhead;
}

Token Lexer::lex()
{
    for(;;)
    {
        skipWhiteSpace();

        int startLine = line;
        int startCol = col;
        
        if (isEof()) return {{Token::Type::TEOF, std::string_view()}, startLine, startCol};

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
            return {{Token::Type::OP_SUB, "-"}, startLine, startCol};
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
                Token::Value value = readLongString(prefix, Token::Type::STRING_LITERAL, Token::Type::MALFORMED_STRING);
                return {value, startLine, startCol};
            }
            else if (prefix == 0)
            {
                return {{Token::Type::MALFORMED_STRING, std::string_view(query).substr(startPos, pos - startPos)}, startLine, startCol};
            }
            else 
            {
                return {{Token::Type::LEFT_BRACKET, "["}, startLine, startCol};
            }
        }

       return {readOperatorAndDelimiter(), startLine, startCol}; // Excluding `[` and `-` as handled above.
    }
}

Token::Value Lexer::readComment()
{
    advance(); // Consume second '-' 
    size_t start = pos;

    if (peek() == '[')
    {
        int prefix = isLongStringSequence();

        if (prefix >= 2)
        {
            return readLongString(prefix, Token::Type::COMMENT, Token::Type::MALFORMED_COMMENT);
        }
    }

    while (!isEof() && peek() != '\n') // Read until new line
    {
        advance();
    }

    return {Token::Type::COMMENT, std::string_view(query).substr(start, pos - start)};
}

std::optional<Token::Type> Lexer::resolveKeyword(std::string_view lexeme)
{
    if (lexeme == "local") return Token::Type::LOCAL;
    if (lexeme == "goto") return Token::Type::GOTO;
    if (lexeme == "function") return Token::Type::FUNCTION;
    if (lexeme == "then") return Token::Type::THEN;
    if (lexeme == "end") return Token::Type::END;
    if (lexeme == "in") return Token::Type::IN;

    if (lexeme == "nil") return Token::Type::NIL;
    if (lexeme == "false") return Token::Type::FALSE;
    if (lexeme == "true") return Token::Type::TRUE;

    if (lexeme == "if") return Token::Type::IF;
    if (lexeme == "else") return Token::Type::ELSE;
    if (lexeme == "elseif") return Token::Type::ELSE_IF;

    if (lexeme == "for") return Token::Type::FOR;
    if (lexeme == "while") return Token::Type::WHILE;
    if (lexeme == "repeat") return Token::Type::REPEAT;
    if (lexeme == "until") return Token::Type::UNTIL;
    if (lexeme == "do") return Token::Type::DO;

    if (lexeme == "and") return Token::Type::AND;
    if (lexeme == "or") return Token::Type::OR;
    if (lexeme == "not") return Token::Type::NOT;

    if (lexeme == "return") return Token::Type::RETURN;
    if (lexeme == "break") return Token::Type::BREAK;

    return std::nullopt;
}

Token::Value Lexer::readIdentifier()
{
    size_t start = pos;

    while (!isEof() && (isAlphaNumeric(peek()) || peek() == '_'))
    {
        advance();
    }

    std::string_view lexeme = std::string_view(query).substr(start, pos - start);

    if(auto keyword = resolveKeyword(lexeme)) 
        return {*keyword, lexeme};

    return {Token::Type::IDENTIFIER, lexeme};
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

Token::Value Lexer::readLongString(int sep, Token::Type type, Token::Type broken)
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

Token::Value Lexer::readString()
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

    if (peek() != quote) return {Token::Type::MALFORMED_STRING, std::string_view(query).substr(start, endPos - start)};
    
    advance(); // Consume ending quote
    
    return {Token::Type::STRING_LITERAL, std::string_view(query).substr(start, endPos - start)};
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

Token::Value Lexer::readNumber() 
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

    return {Token::Type::NUMBER_LITERAL, std::string_view(query).substr(start, pos - start)};
}

Token::Value Lexer::readOperatorAndDelimiter()
{
    size_t start = pos;
    char c = advance();

    switch (c)
    {
        case '(':
            return {Token::Type::LEFT_PAREN, "("};
        case ')':
            return {Token::Type::RIGHT_PAREN, ")"};
        case '{':
            return {Token::Type::LEFT_BRACE, "{"};
        case '}':
            return {Token::Type::RIGHT_BRACE, "}"};     
        case ']':
            return {Token::Type::RIGHT_BRACKET, "]"};
        case ',':
            return {Token::Type::COMMA, ","};
        case '.':
        {
            if (peek() != '.') return {Token::Type::DOT, "."};
            advance();
            if (peek() != '.') return {Token::Type::OP_CONCAT, ".."};
            advance();
            return {Token::Type::VARARG, "..."};
        }
        case ':':
            if (peek() != ':') return {Token::Type::COLON, ":"};
            advance();
            return {Token::Type::DOUBLECOLON, "::"};
        case ';':
            return {Token::Type::SEMICOLON, ";"};
        case '+':
            return {Token::Type::OP_ADD, "+"};
        case '*':
            return {Token::Type::OP_MUL, "*"};
        case '/':
        {
            if (peek() != '/') return {Token::Type::OP_DIV, "/"};
            advance();
            return {Token::Type::OP_FLOOR_DIV, "//"};
        }
        case '%':
            return {Token::Type::OP_MOD, "%"};
        case '^':
            return {Token::Type::OP_EXPO, "^"};
        case '&':
            return {Token::Type::OP_AND, "&"};
        case '|':
            return {Token::Type::OP_OR, "|"};
        case '~':
        {
            if (peek() != '=') return {Token::Type::OP_NOT, "~"};
            advance();
            return {Token::Type::OP_NOT_EQUAL, "~="};
        }
        case '>':
        {
            if (peek() == '=')
            {
                advance();
                return {Token::Type::OP_GREATER_EQUAL, ">="};
            }
            if (peek() == '>')
            {
                advance();
                return {Token::Type::OP_BITSHIFT_RIGHT, ">>"};
            }
            return {Token::Type::OP_GREATER, ">"};
        }
        case '<':
        {
            if (peek() == '=')
            {
                advance();
                return {Token::Type::OP_LESS_EQUAL, "<="};
            }
            if (peek() == '<')
            {
                advance();
                return {Token::Type::OP_BITSHIFT_LEFT, "<<"};
            }
            return {Token::Type::OP_LESS, "<"};
        }
        case '#':
            return {Token::Type::OP_LENGTH, "#"};
        case '=':
        {
            if (peek() != '=') return {Token::Type::OP_ASSIGN, "="};

            advance();
            return {Token::Type::OP_EQUAL, "=="};
        }

        default:
            return {Token::Type::UNKNOWN_CHARACTER, std::string_view(query).substr(start, 1)};
    }
}
