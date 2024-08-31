#ifndef CLOX_VALUE_H_
#define CLOX_VALUE_H_

#include <string.h>

#include "general.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

typedef uint64_t Value;

static inline Value num_to_value(double num)
{
    Value value;
    memcpy(&value, &num, sizeof(double));

    return value;
}

static inline double value_to_num(Value value)
{
    double num;
    memcpy(&num, &value, sizeof(Value));

    return num;
}

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL 1   // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE 3  // 11.

#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))

#define value_make_bool(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define value_make_nil() ((Value)(uint64_t)(QNAN | TAG_NIL))
#define value_make_number(number) num_to_value(number)
#define value_make_obj(object)                                                 \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(object))

#define value_as_bool(value) ((value) == TRUE_VAL)
#define value_as_number(value) value_to_num(value)
#define value_as_obj(value) ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define value_is_bool(value) (((value) | 1) == TRUE_VAL)
#define value_is_nil(value) ((value) == value_make_nil())
#define value_is_number(value) (((value) & QNAN) != QNAN)
#define value_is_obj(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

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

#define value_make_bool(value) ((Value){VAL_BOOL, {.boolean = value}})
#define value_make_nil() ((Value){VAL_NIL, {.number = 0}})
#define value_make_number(value) ((Value){VAL_NUMBER, {.number = value}})
#define value_make_obj(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#define value_as_bool(value) ((value).as.boolean)
#define value_as_number(value) ((value).as.number)
#define value_as_obj(value) ((value).as.obj)

#define value_is_bool(value) ((value).type == VAL_BOOL)
#define value_is_nil(value) ((value).type == VAL_NIL)
#define value_is_number(value) ((value).type == VAL_NUMBER)
#define value_is_obj(value) ((value).type == VAL_OBJ)

#endif

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
