#ifndef CHUNK_MEMORY_H_
#define CHUNK_MEMORY_H_

#include "general.h"
#include "object.h"

#define mem_alloc(type, count) (type*)reallocate(NULL, 0, sizeof(type) * count)

#define mem_free(type, pointer) reallocate(pointer, sizeof(type), 0)

#define capacity_grow(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define array_grow(type, pointer, old_count, new_count)                        \
    (type*)reallocate(pointer, sizeof(type) * (old_count),                     \
                      sizeof(type) * (new_count))

#define array_free(type, pointer, old_count)                                   \
    reallocate(pointer, sizeof(type) * (old_count), 0)

void* reallocate(void* pointer, size_t old_size, size_t new_size);
void gc_mark_obj(Obj* object);
void gc_mark_value(Value value);
void gc_perform();
void objects_free();

#endif // CHUNK_MEMORY_H_
