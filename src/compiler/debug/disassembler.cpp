#include "compiler/debug/disassembler.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>

#include "vm/types/value.h"
#include "compiler/bytecode.h"

size_t disassembleInstruction(const Chunk &chunk, size_t offset);

void Disassembler::disassemble(const Chunk &chunk)
{
    for (size_t i = 0; i < chunk.code.size();) 
    {
        i = disassembleInstruction(chunk, i);
    }
}

static size_t simpleInstruction(const char *name, size_t offset) 
{
    std::cout << name << "\n";
    return offset + 1;
}

static size_t constantInstruction(const char *name, const Chunk& chunk, size_t offset) 
{
    std::cout << std::format("{:16} {:4}", name, chunk.code[offset + 1]) << "\n";
    return offset + 2;
}

static size_t unsignedInstruction(const char *name, const Chunk& chunk, size_t offset) 
{
    std::cout << std::format("{:16} {:4}", name, chunk.code[offset + 1]) << "\n";
    return offset + 2;
}

static size_t signedInstruction(const char *name, const Chunk& chunk, size_t offset) 
{
    std::cout << std::format("{:16} {:4}", name, static_cast<int8_t>(chunk.code[offset + 1])) << "\n"; 
    return offset + 2;
}

static size_t unsignedSignedInstruction(const char *name, const Chunk& chunk, size_t offset) 
{
    std::cout << std::format("{:16} {:4} {:4}", name, chunk.code[offset + 1], static_cast<int8_t>(chunk.code[offset + 2])) << "\n";
    return offset + 3;
}

static size_t jumpInstruction(const char *name, int sign, const Chunk& chunk, size_t offset) 
{
    int16_t jump = static_cast<int16_t>(chunk.code[offset + 1] << 8 | chunk.code[offset + 2]);

    int target = static_cast<int>(offset) + 3 + sign * jump;

    std::cout << std::format("{:16} {:4} -> {}", name, offset, target) << "\n";
    return offset + 3;
}

size_t disassembleInstruction(const Chunk &chunk, size_t offset)
{
    auto instruction = ByteCode::Op(chunk.code[offset]);

    switch (instruction)
    {
        case ByteCode::Op::LOAD_CONST:
            return constantInstruction("LOAD_CONST", chunk, offset);
        case ByteCode::Op::STORE_LOCAL:
            return unsignedInstruction("STORE_LOCAL", chunk, offset);
        case ByteCode::Op::STORE_GLOBAL:
            return constantInstruction("STORE_GLOBAL", chunk, offset);
        case ByteCode::Op::LOAD_LOCAL:
            return unsignedInstruction("LOAD_LOCAL", chunk, offset);
        case ByteCode::Op::LOAD_GLOBAL:
            return constantInstruction("LOAD_GLOBAL", chunk, offset);
        case ByteCode::Op::LOAD_NULL:
            return simpleInstruction("LOAD_NULL", offset);
        case ByteCode::Op::LOAD_TRUE:
            return simpleInstruction("LOAD_TRUE", offset);
        case ByteCode::Op::LOAD_FALSE:
            return simpleInstruction("LOAD_FALSE", offset);
        case ByteCode::Op::POP:
            return simpleInstruction("POP", offset);
        case ByteCode::Op::DUP:
            return simpleInstruction("DUP", offset);

        case ByteCode::Op::ADD:
            return simpleInstruction("ADD", offset);
        case ByteCode::Op::SUB:
            return simpleInstruction("SUB", offset);
        case ByteCode::Op::MUL:
            return simpleInstruction("MUL", offset);
        case ByteCode::Op::FLOOR_DIV:
            return simpleInstruction("FLOOR_DIV", offset);
        case ByteCode::Op::DIV:
            return simpleInstruction("DIV", offset);
        case ByteCode::Op::MOD:
            return simpleInstruction("MOD", offset);
        case ByteCode::Op::EXPO:
            return simpleInstruction("EXPO", offset);
        case ByteCode::Op::CONCAT:
            return simpleInstruction("CONCAT", offset);

        case ByteCode::Op::BIT_AND:
            return simpleInstruction("BIT_AND", offset);
        case ByteCode::Op::BIT_OR:
            return simpleInstruction("BIT_OR", offset);
        case ByteCode::Op::BIT_XOR:
            return simpleInstruction("BIT_XOR", offset);
        case ByteCode::Op::BITSHIFT_LEFT:
            return simpleInstruction("BITSHIFT_LEFT", offset);
        case ByteCode::Op::BITSHIFT_RIGHT:
            return simpleInstruction("BITSHIFT_RIGHT", offset);
        case ByteCode::Op::BIT_NOT:
            return simpleInstruction("BIT_NOT", offset);


        case ByteCode::Op::EQ:
            return simpleInstruction("EQ", offset);
        case ByteCode::Op::LS:
            return simpleInstruction("LS", offset);
        case ByteCode::Op::LSE:
            return simpleInstruction("LSE", offset);

        case ByteCode::Op::NEGATE:
            return simpleInstruction("NEGATE", offset);
        case ByteCode::Op::LENGTH:
            return simpleInstruction("LENGTH", offset);

        case ByteCode::Op::NOT:
            return simpleInstruction("NOT", offset);

        case ByteCode::Op::MAKE_TABLE:
            return unsignedInstruction("MAKE_TABLE", chunk, offset);
        case ByteCode::Op::SET_FIELD:
            return simpleInstruction("SET_FIELD", offset);
        case ByteCode::Op::GET_FIELD:
            return simpleInstruction("GET_FIELD", offset);
        case ByteCode::Op::GET_METHOD:
            return simpleInstruction("GET_METHOD", offset);
        case ByteCode::Op::STORE_TABLE:
            return simpleInstruction("STORE_TABLE", offset);
        case ByteCode::Op::SET_LIST:
            return unsignedInstruction("SET_LIST", chunk, offset);

        case ByteCode::Op::JUMP_IF_FALSE:
            return jumpInstruction("JUMP_IF_FALSE", 1, chunk, offset);
        case ByteCode::Op::JUMP_IF_TRUE:
            return jumpInstruction("JUMP_IF_TRUE", 1, chunk, offset);
        case ByteCode::Op::JUMP:
            return jumpInstruction("JUMP", 1, chunk, offset);

        case ByteCode::Op::FOR_PREP:
            return jumpInstruction("FOR_PREP", 1, chunk, offset);
        case ByteCode::Op::FOR_LOOP:
            return jumpInstruction("FOR_LOOP", -1, chunk, offset);
        case ByteCode::Op::LOOP:
            return jumpInstruction("LOOP", -1, chunk, offset);
            
        case ByteCode::Op::MAKE_CLOSURE:
        {
            offset++;

            auto constant = chunk.code[offset++];

            std::cout << std::format("{:16} {:4}", "MAKE_CLOSURE", constant) << "\n";

            auto function = std::get<FunctionHandle>(chunk.constants[constant]);

            for (size_t i = 0; i < function->upValueCount; i++)
            {
                int isLocal = chunk.code[offset++];
                int index = chunk.code[offset++];
                std::cout << std::format("{} index: {}", isLocal ? "local" : "upvalue", index) << "\n";
            }

            return offset;
        }
        case ByteCode::Op::CALL:
            return unsignedSignedInstruction("CALL", chunk, offset);
        case ByteCode::Op::TAIL_CALL:
            return unsignedSignedInstruction("TAIL_CALL", chunk, offset);
        case ByteCode::Op::STORE_CALLEE:
            return simpleInstruction("STORE_CALLEE", offset);

        case ByteCode::Op::CLOSE_UPVALUE:
            return simpleInstruction("CLOSE_UPVALUE", offset);
        case ByteCode::Op::LOAD_UPVALUE:
            return simpleInstruction("LOAD_UPVALUE", offset);
        case ByteCode::Op::STORE_UPVALUE:
            return simpleInstruction("STORE_UPVALUE", offset);

        case ByteCode::Op::VARARG:
            return signedInstruction("VARARG", chunk, offset);
        case ByteCode::Op::RETURN:
            return unsignedInstruction("RETURN", chunk, offset);
        
        default:
            std::cout << "UNKNOWN";
            return offset + 1;
    }
}