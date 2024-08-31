#ifndef CLOX_VM_H_
#define CLOX_VM_H_

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    Value stack[STACK_MAX];
    Value* stack_top;
    Table globals;
    Table strings;
    ObjUpValue* open_upvalues;

    size_t bytes_allocated;
    size_t next_gc;
    Obj* objects;
    int gray_count;
    int gray_capacity;
    Obj** gray_stack;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void vm_init();
void vm_free();
InterpretResult vm_interpret(const char* source);
void vm_stack_push(Value value);
Value vm_stack_pop();

#endif // CLOX_VM_H_
