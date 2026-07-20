#pragma once

#include <cstdint>

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
    RETURN
};