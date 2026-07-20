#pragma once

#include <cstddef>

struct Chunk;

class Disassembler
{
    public:
        void disassemble(const Chunk &chunk); 
    private:
        size_t disassembleInstruction(const Chunk &chunk, size_t offset);
};