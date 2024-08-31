#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define obj_mem_alloc(type, object_type)                                       \
    (type*)obj_alloc(sizeof(type), object_type)

static Obj* obj_alloc(size_t size, ObjType type)
{
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

ObjList* obj_list_new()
{
    ObjList* list = obj_mem_alloc(ObjList, OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;

    return list;
}

void obj_list_append(ObjList* list, Value value)
{
    if (list->capacity < list->count + 1)
    {
        int old_capacity = list->capacity;
        list->capacity = capacity_grow(old_capacity);
        list->items =
            array_grow(Value, list->items, old_capacity, list->capacity);
    }

    list->items[list->count] = value;
    list->count++;
}

void obj_list_insert(ObjList* list, int index, Value value)
{
    list->items[index] = value;
}

Value obj_list_get(ObjList* list, int index)
{
    return list->items[index];
}

void obj_list_delete(ObjList* list, int index)
{
    for (int i = index; i < list->count - 1; ++i)
        list->items[i] = list->items[i + 1];

    list->items[list->count - 1] = value_make_nil();
    list->count--;
}

bool obj_list_is_valid_index(ObjList* list, int index)
{
    return (index >= 0 && index < list->count);
}

ObjClass* obj_class_new(ObjString* name)
{
    ObjClass* cls = obj_mem_alloc(ObjClass, OBJ_CLASS);
    cls->name = name;

    return cls;
}

ObjFunction* obj_function_new()
{
    ObjFunction* function = obj_mem_alloc(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    chunk_init(&function->chunk);

    return function;
}

ObjNativeFn* obj_native_fn_new(NativeFn function)
{
    ObjNativeFn* native = obj_mem_alloc(ObjNativeFn, OBJ_NATIVE_FN);
    native->function = function;
    return native;
}

ObjClosure* obj_closure_new(ObjFunction* function)
{
    ObjUpValue** upvalues = mem_alloc(ObjUpValue*, function->upvalue_count);

    for (int i = 0; i < function->upvalue_count; ++i)
    {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = obj_mem_alloc(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

static ObjString* obj_string_allocate(char* chars, int length, uint32_t hash)
{
    ObjString* string = obj_mem_alloc(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    vm_stack_push(value_make_obj(string));
    table_set(&vm.strings, string, value_make_nil());
    vm_stack_pop();

    return string;
}

static uint32_t string_hash(const char* key, int length)
{
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }

    return hash;
}

ObjString* obj_string_take(char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);

    if (interned != NULL)
    {
        array_free(char, chars, length + 1);
        return interned;
    }

    return obj_string_allocate(chars, length, hash);
}

ObjString* obj_string_cpy(const char* chars, int length)
{
    uint32_t hash = string_hash(chars, length);

    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);

    if (interned != NULL) return interned;

    char* head_chars = mem_alloc(char, length + 1);
    memcpy(head_chars, chars, length);
    head_chars[length] = '\0';
    return obj_string_allocate(head_chars, length, hash);
}

static void function_print(ObjFunction* function)
{
    if (function->name == NULL)
    {
        printf("<Main Body>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

static void list_print(ObjList* list)
{
    printf("[");

    for (int i = 0; i < list->count; ++i)
    {
        value_print(list->items[i]);
        if (i < list->count - 1) printf(", ");
    }

    printf("]");
}

ObjUpValue* obj_upvalue_new(Value* slot)
{
    ObjUpValue* upvalue = obj_mem_alloc(ObjUpValue, OBJ_UPVALUE);
    upvalue->closed = value_make_nil();
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

void obj_print(Value value)
{
    switch (obj_get_type(value))
    {
        case OBJ_CLASS:
            printf("%s", obj_as_class(value)->name->chars);
            break;

        case OBJ_CLOSURE:
            function_print(obj_as_closure(value)->function);
            break;

        case OBJ_FUNCTION:
            function_print(obj_as_function(value));
            break;

        case OBJ_NATIVE_FN:
            printf("<native fn>");
            break;

        case OBJ_STRING:
            printf("%s", obj_as_cstring(value));
            break;

        case OBJ_UPVALUE:
            printf("upvalue");
            break;

        case OBJ_LIST:
            list_print(obj_as_list(value));
            break;
    }
}
