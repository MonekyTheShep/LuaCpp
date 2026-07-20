#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class Op : uint8_t 
{
    // STACK OP
    LOAD_CONST,
    STORE_GLOBAL,
    LOAD_GLOBAL,
    STORE_LOCAL,
    LOAD_LOCAL,
    LOAD_NULL,
    LOAD_TRUE,
    LOAD_FALSE,
    POP,
    DUP,

    // ARITHMETIC OP
    ADD, // +
    SUB, // -
    MUL, // *
    FLOOR_DIV, // '//'
    DIV, // '/'
    MOD, // %
    EXPO, // ^
    CONCAT, // ..

    // COMPARISON OP
    EQ, // == and ~= (emitted NOT)
    LS, // < and >= (flipped operands)
    LSE, // <= and >= (flipped operands)

    // UNARY OP
    NEGATE, // -
    LENGTH, // #

    // LOGICAL OP
    NOT,

    // BIT OP
    BIT_AND,
    BIT_OR,
    BIT_XOR,
    BITSHIFT_LEFT,
    BITSHIFT_RIGHT,
    BIT_NOT, // Unary

    // TABLE OP
    MAKE_TABLE,
    SET_FIELD,
    GET_FIELD,
    GET_METHOD,
    SET_LIST,
    STORE_TABLE,

    // FUNC
    MAKE_CLOSURE,
    CALL,
    TAIL_CALL,
    STORE_CALLEE,

    // UPVALUE 
    CLOSE_UPVALUE,
    LOAD_UPVALUE,
    STORE_UPVALUE,
    
    // JUMPING 
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    JUMP,

    // LOOPING
    LOOP,
    FOR_PREP,
    FOR_LOOP,

    FOR_CALL,
    FOR_CALL_LOOP,

    // MISC OP
    VARARG,
    RETURN,
    COUNT
};

constexpr size_t numOfOps = static_cast<size_t>(Op::COUNT);

namespace ByteCode
{
    inline std::string toString(Op op)
    {
        switch (op)
        {
            case Op::LOAD_CONST: return "LOAD_CONST";
            case Op::STORE_GLOBAL: return "STORE_GLOBAL";
            case Op::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case Op::STORE_LOCAL: return "STORE_LOCAL";
            case Op::LOAD_LOCAL: return "LOAD_LOCAL";
            case Op::LOAD_NULL: return "LOAD_NULL";
            case Op::LOAD_TRUE: return "LOAD_TRUE";
            case Op::LOAD_FALSE: return "LOAD_FALSE";
            case Op::POP: return "POP";
            case Op::DUP: return "DUP";

            case Op::ADD: return "ADD";
            case Op::SUB: return "SUB";
            case Op::MUL: return "MUL";
            case Op::FLOOR_DIV: return "FLOOR_DIV";
            case Op::DIV: return "DIV";
            case Op::MOD: return "MOD";
            case Op::EXPO: return "EXPO";
            case Op::CONCAT: return "CONCAT";

            case Op::EQ: return "EQ";
            case Op::LS: return "LS";
            case Op::LSE: return "LSE";

            case Op::NEGATE: return "NEGATE";
            case Op::LENGTH: return "LENGTH";

            case Op::NOT: return "NOT";

            case Op::BIT_AND: return "BIT_AND";
            case Op::BIT_OR: return "BIT_OR";
            case Op::BIT_XOR: return "BIT_XOR";
            case Op::BITSHIFT_LEFT: return "BITSHIFT_LEFT";
            case Op::BITSHIFT_RIGHT: return "BITSHIFT_RIGHT";
            case Op::BIT_NOT: return "BIT_NOT";

            case Op::MAKE_TABLE: return "MAKE_TABLE";
            case Op::SET_FIELD: return "SET_FIELD";
            case Op::GET_FIELD: return "GET_FIELD";
            case Op::GET_METHOD: return "GET_METHOD";
            case Op::SET_LIST: return "SET_LIST";
            case Op::STORE_TABLE: return "STORE_TABLE";

            case Op::MAKE_CLOSURE: return "MAKE_CLOSURE";
            case Op::CALL: return "CALL";
            case Op::TAIL_CALL: return "TAIL_CALL";
            case Op::STORE_CALLEE: return "STORE_CALLEE";

            case Op::CLOSE_UPVALUE: return "CLOSE_UPVALUE";
            case Op::LOAD_UPVALUE: return "LOAD_UPVALUE";
            case Op::STORE_UPVALUE: return "STORE_UPVALUE";

            case Op::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
            case Op::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
            case Op::JUMP: return "JUMP";

            case Op::LOOP: return "LOOP";
            case Op::FOR_PREP: return "FOR_PREP";
            case Op::FOR_LOOP: return "FOR_LOOP";
            case Op::FOR_CALL: return "FOR_CALL";
            case Op::FOR_CALL_LOOP: return "FOR_CALL_LOOP";

            case Op::VARARG: return "VARARG";
            case Op::RETURN: return "RETURN";
            case Op::COUNT: return "COUNT";

            default: return "UNKNOWN";
        }
    }
}