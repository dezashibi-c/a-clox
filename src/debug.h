#ifndef CLOX_DEBUG_H_
#define CLOX_DEBUG_H_

#include "chunk.h"

void chunk_disassemble(Chunk* chunk, const char* name);

int instruction_disassemble(Chunk* chunk, int offset);

#endif // CLOX_DEBUG_H_
