#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

bool value_check_equality(Value a, Value b)
{
#ifdef NAN_BOXING
    if (value_is_number(a) && value_is_number(b))
        return value_as_number(a) == value_as_number(b);

    return a == b;
#else
    if (a.type != b.type) return false;

    switch (a.type)
    {
        case VAL_BOOL:
            return value_as_bool(a) == value_as_bool(b);

        case VAL_NIL:
            return true;

        case VAL_NUMBER:
            return value_as_number(a) == value_as_number(b);

        case VAL_OBJ:
            return value_as_obj(a) == value_as_obj(b);

        default:
            return false; // Unreachable.
    }
#endif
}

void value_array_init(ValueArray* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void value_array_write(ValueArray* array, Value value)
{
    if (array->capacity < array->count + 1)
    {
        int old_capacity = array->capacity;
        array->capacity = capacity_grow(old_capacity);
        array->values =
            array_grow(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void value_array_free(ValueArray* array)
{
    array_free(Value, array->values, array->capacity);
    value_array_init(array);
}

void value_print(Value value)
{
#ifdef NAN_BOXING
    if (value_is_bool(value))
    {
        printf(value_as_bool(value) ? "true" : "false");
    }
    else if (value_is_nil(value))
    {
        printf("nil");
    }
    else if (value_is_number(value))
    {
        printf("%g", value_as_number(value));
    }
    else if (value_is_obj(value))
    {
        obj_print(value);
    }
#else

    switch (value.type)
    {
        case VAL_BOOL:
            printf("%s", value_as_bool(value) ? "true" : "false");
            break;

        case VAL_NIL:
            printf("%s", "nil");
            break;

        case VAL_NUMBER:
            printf("%g", value_as_number(value));
            break;

        case VAL_OBJ:
            obj_print(value);
            break;
    }
#endif
}
