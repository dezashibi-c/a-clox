#ifndef CLOX_VALUE_H_
#define CLOX_VALUE_H_

#include "general.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum
{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct
{
    ValueType type;
    union
    {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define value_as_bool(value) ((value).as.boolean)
#define value_as_number(value) ((value).as.number)
#define value_as_obj(value) ((value).as.obj)

#define value_is_bool(value) ((value).type == VAL_BOOL)
#define value_is_nil(value) ((value).type == VAL_NIL)
#define value_is_number(value) ((value).type == VAL_NUMBER)
#define value_is_obj(value) ((value).type == VAL_OBJ)

#define value_make_bool(value) ((Value){VAL_BOOL, {.boolean = value}})
#define value_make_nil() ((Value){VAL_NIL, {.number = 0}})
#define value_make_number(value) ((Value){VAL_NUMBER, {.number = value}})
#define value_make_obj(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct
{
    int capacity;
    int count;
    Value* values;
} ValueArray;

bool value_check_equality(Value a, Value b);

void value_array_init(ValueArray* array);
void value_array_write(ValueArray* array, Value value);
void value_array_free(ValueArray* array);
void value_print(Value value);

static inline bool value_is_falsy(Value value)
{
    return value_is_nil(value) ||
           (value_is_bool(value) && !value_as_bool(value));
}


#endif // CLOX_VALUE_H_
