#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "debug.h"
#include "general.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void vm_stack_reset()
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

static void raise_runtime_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; --i)
    {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    vm_stack_reset();
}

void vm_define_native_fn(const char* name, NativeFn function)
{
    vm_stack_push(value_make_obj(obj_string_cpy(name, (int)strlen(name))));
    vm_stack_push(value_make_obj(obj_native_fn_new(function)));
    table_set(&vm.globals, obj_as_string(vm.stack[0]), vm.stack[1]);
    vm_stack_pop();
    vm_stack_pop();
}

static Value native_fn_clock(int argc, Value* args)
{
    (void)argc;
    (void)args;

    return value_make_number((double)clock() / CLOCKS_PER_SEC);
}

static Value native_fn_list_append(int argc, Value* args)
{
    if (argc != 2)
    {
        raise_runtime_error("insufficient arguments, need 2 got=%d", argc);
        return value_make_nil();
    }

    if (!obj_is_list(args[0]))
    {
        raise_runtime_error("cannot append item to non-list variable.");
        return value_make_nil();
    }

    ObjList* list = obj_as_list(args[0]);
    Value item = args[1];
    obj_list_append(list, item);
    return value_make_nil();
}

static Value native_fn_list_delete(int argc, Value* args)
{
    if (argc != 2)
    {
        raise_runtime_error("insufficient arguments, need 2 got=%d", argc);
        return value_make_nil();
    }

    if (!obj_is_list(args[0]))
    {
        raise_runtime_error("cannot append item to non-list variable.");
        return value_make_nil();
    }

    if (!value_is_number(args[1]))
    {
        raise_runtime_error("index cannot be a non-number value.");
        return value_make_nil();
    }

    ObjList* list = obj_as_list(args[0]);
    int index = value_as_number(args[1]);

    if (!obj_list_is_valid_index(list, index))
    {
        raise_runtime_error("index out of range.");
        return value_make_nil();
    }

    obj_list_delete(list, index);
    return value_make_nil();
}

void vm_init()
{
    vm_stack_reset();
    vm.objects = NULL;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;
    vm.bytes_allocated = 0;
    vm.next_gc = 1024 * 1024;

    table_init(&vm.globals);
    table_init(&vm.strings);

    vm_define_native_fn("clock", native_fn_clock);
    vm_define_native_fn("append", native_fn_list_append);
    vm_define_native_fn("delete", native_fn_list_delete);
}

void vm_free()
{
    table_free(&vm.globals);
    table_free(&vm.strings);

    objects_free();
}

void vm_stack_push(Value value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

Value vm_stack_pop()
{
    vm.stack_top--;
    return *vm.stack_top;
}

static Value vm_stack_peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool obj_func_call(ObjClosure* closure, int argc)
{
    if (argc != closure->function->arity)
    {
        raise_runtime_error("Expected %d argument but got %d.",
                            closure->function->arity, argc);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX)
    {
        raise_runtime_error("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - argc - 1;
    return true;
}

static bool value_call(Value callee, int argc)
{
    if (value_is_obj(callee))
    {
        switch (obj_get_type(callee))
        {
            case OBJ_CLOSURE:
                return obj_func_call(obj_as_closure(callee), argc);

            case OBJ_NATIVE_FN:
            {
                NativeFn native = obj_as_native_fn(callee);
                Value result = native(argc, vm.stack_top - argc);
                vm.stack_top -= argc + 1;
                vm_stack_push(result);
                return true;
            }

            default:
                break; // Non-callable object type;
        }
    }

    raise_runtime_error("Can only call functions and classes.");
    return false;
}

static ObjUpValue* upvalue_capture(Value* local)
{
    ObjUpValue* prev_upvalue = NULL;
    ObjUpValue* upvalue = vm.open_upvalues;
    while (upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) return upvalue;

    ObjUpValue* created_upvalue = obj_upvalue_new(local);
    created_upvalue->next = upvalue;

    if (prev_upvalue == NULL)
    {
        vm.open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void upvalue_close_until(Value* last)
{
    while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last)
    {
        ObjUpValue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static void string_concat()
{
    ObjString* b = obj_as_string(vm_stack_peek(0));
    ObjString* a = obj_as_string(vm_stack_peek(1));

    int length = a->length + b->length;
    char* chars = mem_alloc(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = obj_string_take(chars, length);
    vm_stack_pop();
    vm_stack_pop();

    vm_stack_push(value_make_obj(result));
}

static InterpretResult run()
{
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

#define byte_read_raw() (*frame->ip++)
#define byte_read_constant()                                                   \
    (frame->closure->function->chunk.constants.values[byte_read_raw()])

#define byte_read_short()                                                      \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

#define byte_read_string() (obj_as_string(byte_read_constant()))
#define binary_op(value_type, op)                                              \
    do                                                                         \
    {                                                                          \
        if (!value_is_number(vm_stack_peek(0)) ||                              \
            !value_is_number(vm_stack_peek(1)))                                \
        {                                                                      \
            raise_runtime_error("Operand must be numbers.");                   \
            return INTERPRET_RUNTIME_ERROR;                                    \
        }                                                                      \
        double b = value_as_number(vm_stack_pop());                            \
        double a = value_as_number(vm_stack_pop());                            \
        vm_stack_push(value_make_##value_type(a op b));                        \
    } while (false)

    while (true)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("%s", "          ");
        for (Value* slot = vm.stack; slot < vm.stack_top; ++slot)
        {
            printf("%s", "[ ");
            value_print(*slot);
            printf("%s", " ]");
        }

        puts("");

        instruction_disassemble(
            &frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = byte_read_raw())
        {
            case OP_CONSTANT:
            {
                Value constant = byte_read_constant();
                vm_stack_push(constant);
                break;
            }

            case OP_NIL:
                vm_stack_push(value_make_nil());
                break;

            case OP_TRUE:
                vm_stack_push(value_make_bool(true));
                break;

            case OP_FALSE:
                vm_stack_push(value_make_bool(false));
                break;

            case OP_POP:
                vm_stack_pop();
                break;

            case OP_GET_LOCAL:
            {
                uint8_t slot = byte_read_raw();
                vm_stack_push(frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL:
            {
                uint8_t slot = byte_read_raw();
                frame->slots[slot] = vm_stack_peek(0);
                break;
            }

            case OP_GET_GLOBAL:
            {
                ObjString* name = byte_read_string();
                Value value;

                if (!table_get(&vm.globals, name, &value))
                {
                    raise_runtime_error("Undefined symbol '%s'.", name->chars);

                    return INTERPRET_RUNTIME_ERROR;
                }

                vm_stack_push(value);
                break;
            }

            case OP_DEFINE_GLOBAL:
            {
                ObjString* name = byte_read_string();
                table_set(&vm.globals, name, vm_stack_peek(0));
                vm_stack_pop();
                break;
            }

            case OP_SET_GLOBAL:
            {
                ObjString* name = byte_read_string();

                if (table_set(&vm.globals, name, vm_stack_peek(0)))
                {
                    table_delete(&vm.globals, name);
                    raise_runtime_error("Undefined variable '%s'.",
                                        name->chars);

                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }

            case OP_GET_UPVALUE:
            {
                uint8_t slot = byte_read_raw();
                vm_stack_push(*frame->closure->upvalues[slot]->location);
                break;
            }

            case OP_SET_UPVALUE:
            {
                uint8_t slot = byte_read_raw();
                *frame->closure->upvalues[slot]->location = vm_stack_peek(0);
                break;
            }

            case OP_EQUAL:
            {
                Value b = vm_stack_pop();
                Value a = vm_stack_pop();

                vm_stack_push(value_make_bool(value_check_equality(a, b)));
                break;
            }

            case OP_GREATER:
                binary_op(bool, >);
                break;

            case OP_LESS:
                binary_op(bool, <);
                break;

            case OP_ADD:
            {
                if (obj_is_string(vm_stack_peek(0)) &&
                    obj_is_string(vm_stack_peek(1)))
                {
                    string_concat();
                }
                else if (value_is_number(vm_stack_peek(0)) &&
                         value_is_number(vm_stack_peek(1)))
                {
                    double b = value_as_number(vm_stack_pop());
                    double a = value_as_number(vm_stack_pop());

                    vm_stack_push(value_make_number(a + b));
                }
                else
                {
                    raise_runtime_error(
                        "Operands must be two numbers or two strings.");

                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }

            case OP_SUBTRACT:
                binary_op(number, -);
                break;

            case OP_MULTIPLY:
                binary_op(number, *);
                break;

            case OP_DIVIDE:
                binary_op(number, /);
                break;

            case OP_NOT:
                vm_stack_push(value_make_bool(value_is_falsy(vm_stack_pop())));
                break;

            case OP_NEGATE:
                if (value_is_number(vm_stack_peek(0)))
                {
                    raise_runtime_error("Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                vm_stack_push(
                    value_make_number(-value_as_number(vm_stack_pop())));
                break;

            case OP_PRINT:
                value_print(vm_stack_pop());
                puts("");
                break;

            case OP_JUMP:
            {
                uint16_t offset = byte_read_short();
                frame->ip += offset;

                break;
            }

            case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = byte_read_short();
                if (value_is_falsy(vm_stack_peek(0))) frame->ip += offset;

                break;
            }

            case OP_LOOP:
            {
                uint16_t offset = byte_read_short();
                frame->ip -= offset;
                break;
            }

            case OP_CALL:
            {
                int argc = byte_read_raw();
                if (!value_call(vm_stack_peek(argc), argc))
                    return INTERPRET_RUNTIME_ERROR;

                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OP_CLOSURE:
            {
                ObjFunction* function = obj_as_function(byte_read_constant());
                ObjClosure* closure = obj_closure_new(function);
                vm_stack_push(value_make_obj(closure));

                for (int i = 0; i < closure->upvalue_count; ++i)
                {
                    uint8_t is_local = byte_read_raw();
                    uint8_t index = byte_read_raw();

                    if (is_local)
                    {
                        closure->upvalues[i] =
                            upvalue_capture(frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }

            case OP_CLOSE_UPVALUE:
                upvalue_close_until(vm.stack_top - 1);
                vm_stack_pop();
                break;

            case OP_LIST_INIT:
            {
                // Stack before: [item1, item2, ..., itemN] and after: [list]
                ObjList* list = obj_list_new();
                uint8_t item_count = byte_read_raw();

                // So list isn't sweeped by GC in obj_list_append
                vm_stack_push(value_make_obj(list));
                // Add items to list
                for (int i = item_count; i > 0; --i)
                    obj_list_append(list, vm_stack_peek(i));

                vm_stack_pop();

                // Pop items from stack
                while (item_count-- > 0) vm_stack_pop();

                vm_stack_push(value_make_obj(list));
                break;
            }

            case OP_LIST_GETIDX:
            {
                // Stack before: [list, index] and after: [index(list, index)]
                Value index = vm_stack_pop();
                Value list = vm_stack_pop();

                if (!obj_as_list(list))
                {
                    raise_runtime_error("Invalid type to index into.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!value_is_number(index))
                {
                    raise_runtime_error("List index is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!obj_list_is_valid_index(obj_as_list(list),
                                             value_as_number(index)))
                {
                    raise_runtime_error("List index out of range");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value result =
                    obj_list_get(obj_as_list(list), value_as_number(index));
                vm_stack_push(result);
                break;
            }

            case OP_LIST_SETIDX:
            {
                // Stack before: [list, index, item] and after: [item]
                Value item = vm_stack_pop();
                Value index = vm_stack_pop();
                Value list = vm_stack_pop();

                if (!obj_as_list(list))
                {
                    raise_runtime_error("Invalid type to index into.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!value_is_number(index))
                {
                    raise_runtime_error("List index is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!obj_list_is_valid_index(obj_as_list(list),
                                             value_as_number(index)))
                {
                    raise_runtime_error("List index out of range");
                    return INTERPRET_RUNTIME_ERROR;
                }

                obj_list_insert(obj_as_list(list), value_as_number(index),
                                item);
                vm_stack_push(item);
                break;
            }

            case OP_RETURN:
            {
                Value result = vm_stack_pop();
                upvalue_close_until(frame->slots);
                vm.frame_count--;
                if (vm.frame_count == 0)
                {
                    vm_stack_pop();
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                vm_stack_push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }

            case OP_CLASS:
                vm_stack_push(
                    value_make_obj(obj_class_new(byte_read_string())));
                break;
        }
    }

#undef byte_read_raw
#undef byte_read_short
#undef byte_read_constant
#undef byte_read_string
#undef binary_op
}

InterpretResult vm_interpret(const char* source)
{
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    vm_stack_push(value_make_obj(function));

    ObjClosure* closure = obj_closure_new(function);
    vm_stack_pop();
    vm_stack_push(value_make_obj(closure));
    obj_func_call(closure, 0);

    return run();
}