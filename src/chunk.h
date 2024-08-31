#ifndef CLOX_CHUNK_H_
#define CLOX_CHUNK_H_

#include "general.h"
#include "value.h"

typedef enum
{
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_LIST_INIT,
    OP_LIST_GETIDX,
    OP_LIST_SETIDX,
    OP_RETURN,
} OpCode;

typedef struct
{
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk* chunk);

void chunk_free(Chunk* chunk);

void chunk_write(Chunk* chunk, uint8_t byte, int line);

int chunk_constant_add(Chunk* chunk, Value value);

#endif // CLOX_CHUNK_H_
