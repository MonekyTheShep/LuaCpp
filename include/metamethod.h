#pragma once

#include <cstdint>
#include <string>

enum class MetaMethod : uint8_t
{
    ADD = 0,
    SUB,
    MUL,
    IDIV,
    DIV,
    MOD,
    POW,
    CONCAT,
    UNM,
    EQ,
    LT,
    LE,
    BAND,
    BOR,
    BXOR,
    SHL,
    SHR,
    BNOT,
    TOSTRING,
    LEN,
    CALL,
    INDEX,
    NEWINDEX,
    METATABLE
};

namespace Method 
{
    inline std::string names[] = 
    {
        "__add",
        "__sub",
        "__mul",
        "__idiv",
        "__div",
        "__mod",
        "__pow",
        "__concat",
        "__unm",
        "__eq",
        "__lt",
        "__le",
        "__band",
        "__bor",
        "__bxor",
        "__shl",
        "__shr",
        "__bnot",
        "__tostring",
        "__len",
        "__call",
        "__index",
        "__newindex",
        "__metatable"
    };
};
