#pragma once

#include <cstddef>

struct Chunk;

class Disassembler
{
    public:
        static void disassemble(const Chunk &chunk); 
    private:
        static size_t disassembleInstruction(const Chunk &chunk, size_t offset);
};