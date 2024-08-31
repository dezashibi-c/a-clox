#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t old_size, size_t new_size)
{
    vm.bytes_allocated += new_size - old_size;

    if (new_size > old_size)
    {
#ifdef DEBUG_STRESS_GC
        gc_perform();
#endif

        if (vm.bytes_allocated > vm.next_gc) gc_perform();
    }

    if (new_size == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL) exit(1);
    return result;
}

void gc_mark_obj(Obj* object)
{
    if (object == NULL) return;
    if (object->is_marked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    value_print(value_make_obj(object));
    puts("");
#endif

    object->is_marked = true;

    if (vm.gray_capacity < vm.gray_count + 1)
    {
        vm.gray_capacity = capacity_grow(vm.gray_capacity);
        vm.gray_stack =
            (Obj**)realloc(vm.gray_stack, sizeof(Obj*) * vm.gray_capacity);

        if (vm.gray_stack == NULL) exit(1);
    }

    vm.gray_stack[vm.gray_count++] = object;
}

void gc_mark_value(Value value)
{
    if (value_is_obj(value)) gc_mark_obj(value_as_obj(value));
}

static void gc_mark_array(ValueArray* array)
{
    for (int i = 0; i < array->count; ++i) gc_mark_value(array->values[i]);
}

static void gc_blacken_obj(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    value_print(value_make_obj(object));
    puts("");
#endif

    switch (object->type)
    {
        case OBJ_BOUND_METHOD:
        {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            gc_mark_value(bound->receiver);
            gc_mark_obj((Obj*)bound->method);
            break;
        }

        case OBJ_CLASS:
        {
            ObjClass* cls = (ObjClass*)object;
            gc_mark_obj((Obj*)cls->name);
            gc_mark_table(&cls->methods);
            break;
        }

        case OBJ_INSTANCE:
        {
            ObjInstance* instance = (ObjInstance*)object;
            gc_mark_obj((Obj*)instance->cls);
            gc_mark_table(&instance->fields);
            break;
        }

        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            gc_mark_obj((Obj*)closure->function);
            for (int i = 0; i < closure->upvalue_count; ++i)
                gc_mark_obj((Obj*)closure->upvalues[i]);

            break;
        }

        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            gc_mark_obj((Obj*)function->name);
            gc_mark_array(&function->chunk.constants);
            break;
        }

        case OBJ_UPVALUE:
            gc_mark_value(((ObjUpValue*)object)->closed);
            break;

        case OBJ_NATIVE_FN:
        case OBJ_STRING:
            break;

        case OBJ_LIST:
        {
            ObjList* list = (ObjList*)object;
            for (int i = 0; i < list->count; ++i) gc_mark_value(list->items[i]);

            break;
        }
    }
}

static void object_free(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch (object->type)
    {
        case OBJ_BOUND_METHOD:
            mem_free(ObjBoundMethod, object);
            break;

        case OBJ_CLASS:
        {
            ObjClass* cls = (ObjClass*)object;
            table_free(&cls->methods);
            mem_free(ObjClass, object);
            break;
        }

        case OBJ_INSTANCE:
        {
            ObjInstance* instance = (ObjInstance*)object;
            table_free(&instance->fields);
            mem_free(ObjInstance, object);
            break;
        }

        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            array_free(ObjUpValue*, closure->upvalues, closure->upvalue_count);
            mem_free(ObjClosure, object);
            break;
        }

        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            chunk_free(&function->chunk);
            mem_free(ObjFunction, object);
            break;
        }

        case OBJ_NATIVE_FN:
            mem_free(ObjNativeFn, object);
            break;

        case OBJ_STRING:
        {
            ObjString* string = (ObjString*)object;
            array_free(char, string->chars, string->length + 1);
            mem_free(ObjString, object);
            break;
        }

        case OBJ_UPVALUE:
            mem_free(ObjUpValue, object);
            break;

        case OBJ_LIST:
        {
            ObjList* list = (ObjList*)object;
            array_free(Value*, list->items, list->count);
            mem_free(ObjList, object);
            break;
        }
    }
}

static void gc_mark_roots()
{
    for (Value* slot = vm.stack; slot < vm.stack_top; ++slot)
        gc_mark_value(*slot);

    for (int i = 0; i < vm.frame_count; ++i)
        gc_mark_obj((Obj*)vm.frames[i].closure);

    for (ObjUpValue* upvalue = vm.open_upvalues; upvalue != NULL;
         upvalue = upvalue->next)
        gc_mark_obj((Obj*)upvalue);

    gc_mark_table(&vm.globals);

    gc_mark_compiler_roots();

    gc_mark_obj((Obj*)vm.init_str);
}

static void gc_trace_refs()
{
    while (vm.gray_count > 0)
    {
        Obj* object = vm.gray_stack[--vm.gray_count];
        gc_blacken_obj(object);
    }
}

static void gc_sweep()
{
    Obj* previous = NULL;
    Obj* object = vm.objects;

    while (object != NULL)
    {
        if (object->is_marked)
        {
            object->is_marked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            Obj* unreached = object;
            object = object->next;

            if (previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm.objects = object;
            }

            object_free(unreached);
        }
    }
}

void gc_perform()
{
#ifdef DEBUG_LOG_GC
    puts("-- gc begin");
    size_t before = vm.bytes_allocated;
#endif

    gc_mark_roots();
    gc_trace_refs();
    gc_table_remove_white(&vm.strings);
    gc_sweep();

    vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    puts("-- gc end");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_gc);
#endif
}

void objects_free()
{
    Obj* object = vm.objects;
    while (object != NULL)
    {
        Obj* next = object->next;
        object_free(object);
        object = next;
    }

    free(vm.gray_stack);
}
