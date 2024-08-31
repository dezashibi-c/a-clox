#ifndef CLOX_OBJECT_H_
#define CLOX_OBJECT_H_

#include "chunk.h"
#include "general.h"
#include "table.h"
#include "value.h"

#define obj_get_type(value) (value_as_obj(value)->type)

#define obj_is_list(value) (is_object_of_type(value, OBJ_LIST))
#define obj_is_class(value) (is_object_of_type(value, OBJ_CLASS))
#define obj_is_instance(value) (is_object_of_type(value, OBJ_INSTANCE))
#define obj_is_closure(value) (is_object_of_type(value, OBJ_CLOSURE))
#define obj_is_function(value) (is_object_of_type(value, OBJ_FUNCTION))
#define obj_is_native_fn(value) (is_object_of_type(value, OBJ_NATIVE_FN))
#define obj_is_string(value) (is_object_of_type(value, OBJ_STRING))

#define obj_as_list(value) ((ObjList*)value_as_obj(value))
#define obj_as_class(value) ((ObjClass*)value_as_obj(value))
#define obj_as_instance(value) ((ObjInstance*)value_as_obj(value))
#define obj_as_closure(value) ((ObjClosure*)value_as_obj(value))
#define obj_as_function(value) ((ObjFunction*)value_as_obj(value))
#define obj_as_native_fn(value) (((ObjNativeFn*)value_as_obj(value))->function)
#define obj_as_string(value) ((ObjString*)value_as_obj(value))
#define obj_as_cstring(value) (((ObjString*)value_as_obj(value))->chars)

typedef enum
{
    OBJ_LIST,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE_FN,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

struct Obj
{
    ObjType type;
    bool is_marked;
    struct Obj* next;
};

typedef struct
{
    Obj obj;
    int count;
    int capacity;
    Value* items;
} ObjList;

typedef struct
{
    Obj obj;
    int upvalue_count;
    int arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argc, Value* args);

typedef struct
{
    Obj obj;
    NativeFn function;
} ObjNativeFn;

struct ObjString
{
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpValue
{
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpValue* next;
} ObjUpValue;

typedef struct
{
    Obj obj;
    ObjFunction* function;
    ObjUpValue** upvalues;
    int upvalue_count;
} ObjClosure;

typedef struct
{
    Obj obj;
    ObjString* name;
} ObjClass;

typedef struct
{
    Obj obj;
    ObjClass* cls;
    Table fields;
} ObjInstance;

ObjList* obj_list_new();
void obj_list_append(ObjList* list, Value value);
void obj_list_insert(ObjList* list, int index, Value value);
Value obj_list_get(ObjList* list, int index);
void obj_list_delete(ObjList* list, int index);
bool obj_list_is_valid_index(ObjList* list, int index);

ObjClass* obj_class_new(ObjString* name);
ObjInstance* obj_instance_new(ObjClass* cls);

ObjFunction* obj_function_new();
ObjNativeFn* obj_native_fn_new(NativeFn function);
ObjClosure* obj_closure_new(ObjFunction* function);

ObjString* obj_string_take(char* chars, int length);
ObjString* obj_string_cpy(const char* chars, int length);

ObjUpValue* obj_upvalue_new(Value* slot);

void obj_print(Value value);

static inline bool is_object_of_type(Value value, ObjType type)
{
    return value_is_obj(value) && value_as_obj(value)->type == type;
}

#endif // CLOX_OBJECT_H_
