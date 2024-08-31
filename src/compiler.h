#ifndef CLOX_COMPILER_H_
#define CLOX_COMPILER_H_

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void gc_mark_compiler_roots();

#endif // CLOX_COMPILER_H_
