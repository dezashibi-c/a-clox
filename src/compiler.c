#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "general.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_SUBSCRIPT,  // []
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct
{
    Token name;
    int depth;
    bool is_captured;
} Local;

typedef struct
{
    uint8_t index;
    bool is_local;
} UpValue;

typedef enum
{
    CP_FUNCTION_BODY,
    CP_MAIN_BODY,
} CodePlacement;

typedef struct Compiler
{
    struct Compiler* enclosing;
    ObjFunction* function;
    CodePlacement code_placement;

    Local locals[UINT8_COUNT];
    int local_count;
    UpValue upvalues[UINT8_COUNT];
    int scope_depth;
} Compiler;

Parser parser;
Compiler* current_compiler = NULL;
Chunk* compiling_chunk;

///////////////////////////////////////////////////////////////////////////////////////
// STATIC FUNCTIONS DECLARATIONS, MACROS AND PARSER RULES
///////////////////////////////////////////////////////////////////////////////////////

#define current_token_is(TokenType) (parser.current.type == TokenType)

static Chunk* current_chunk();
static void compiler_init(Compiler* compiler, CodePlacement code_placement);
static void compiler_scope_begin();
static void compiler_scope_end();
static void compiler_local_add(Token name);
static int compiler_upvalue_add(Compiler* compiler, uint8_t index,
                                bool is_local);
static int compiler_local_resolve(Compiler* compiler, Token* name);
static int compiler_upvalue_resolve(Compiler* compiler, Token* name);
static void compiler_local_mark_initialized();
static void compiler_declare_variable();

static void raise_error_at(Token* token, const char* message);
static void raise_error(const char* message);
static void raise_error_at_current(const char* message);
static void move_to_next_token();
static void sync_errors();
static void expect_token_or_fail(TokenType type, const char* message);
static bool expect_token(TokenType type);
static ParseRule* get_rule(TokenType type);

static uint8_t constant_make(Value value);
static uint8_t constant_identifier(Token* name);
static bool token_identifiers_equal(Token* a, Token* b);
static void byte_emit(uint8_t byte);
static void byte_emit_duo(uint8_t byte1, uint8_t byte2);
static void byte_emit_var_def(uint8_t global);
static void byte_emit_named_variable(Token name, bool can_assign);
static void byte_emit_variable(bool can_assign);
static int byte_emit_jump(uint8_t instruction);
static void byte_emit_patch_jump(int offset);
static void byte_emit_loop(int loop_start);
static void byte_emit_return();
static void byte_emit_constant(Value value);

static void parse_precedence(Precedence precedence);
static void parse_grouping(bool can_assign);
static void parse_binary(bool can_assign);
static void parse_unary(bool can_assign);
static void parse_number(bool can_assign);
static void parse_string(bool can_assign);
static void parse_literal(bool can_assign);
static void parse_call(bool can_assign);
static void parse_dot(bool can_assign);
static void parse_list(bool can_assign);
static void parse_subscript(bool can_assign);

static void parse_expression();

static uint8_t parse_variable(const char* error_message);
static uint8_t parse_argument_list();
static void parse_fun_declaration();
static void parse_class_method();
static void parse_class_declaration();
static void parse_var_declaration();
static void parse_declaration();

static void parse_and(bool can_assign);
static void parse_or(bool can_assign);

static void parse_print_statement();
static void parse_if_statement();
static void parse_return_statement();
static void parse_while_statement();
static void parse_for_statement();
static void parse_expression_statement();
static void parse_block();
static void parse_function(CodePlacement code_placement);
static void parse_statement();

static ObjFunction* compiler_finalize();

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {parse_grouping, parse_call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACKET] = {parse_list, parse_subscript, PREC_SUBSCRIPT},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, parse_dot, PREC_CALL},
    [TOKEN_MINUS] = {parse_unary, parse_binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, parse_binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, parse_binary, PREC_FACTOR},
    [TOKEN_BANG] = {parse_unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, parse_binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, parse_binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {byte_emit_variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {parse_string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {parse_number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, parse_and, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {parse_literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {parse_literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, parse_or, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {parse_literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

///////////////////////////////////////////////////////////////////////////////////////
// HELPERS
///////////////////////////////////////////////////////////////////////////////////////

static Chunk* current_chunk()
{
    return &current_compiler->function->chunk;
}

static void compiler_init(Compiler* compiler, CodePlacement code_placement)
{
    compiler->enclosing = current_compiler;
    compiler->function = NULL;
    compiler->code_placement = code_placement;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = obj_function_new();
    current_compiler = compiler;

    if (code_placement != CP_MAIN_BODY)
    {
        current_compiler->function->name =
            obj_string_cpy(parser.previous.start, parser.previous.length);
    }

    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->is_captured = false;
    local->name.length = 0;
}

static void compiler_scope_begin()
{
    current_compiler->scope_depth++;
}

static void compiler_scope_end()
{
    current_compiler->scope_depth--;

    while (current_compiler->local_count > 0 &&
           current_compiler->locals[current_compiler->local_count - 1].depth >
               current_compiler->scope_depth)
    {
        if (current_compiler->locals[current_compiler->local_count - 1]
                .is_captured)
        {
            byte_emit(OP_CLOSE_UPVALUE);
        }
        else
        {
            byte_emit(OP_POP);
        }

        current_compiler->local_count--;
    }
}

static void compiler_local_add(Token name)
{
    if (current_compiler->local_count == UINT8_COUNT)
    {
        raise_error("Too many local variables in function.");
        return;
    }

    Local* local = &current_compiler->locals[current_compiler->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static int compiler_upvalue_add(Compiler* compiler, uint8_t index,
                                bool is_local)
{
    int upvalue_count = compiler->function->upvalue_count;

    for (int i = 0; i < upvalue_count; ++i)
    {
        UpValue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) return i;
    }

    if (upvalue_count == UINT8_COUNT)
    {
        raise_error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;

    return compiler->function->upvalue_count++;
}

static int compiler_local_resolve(Compiler* compiler, Token* name)
{
    for (int i = compiler->local_count - 1; i >= 0; --i)
    {
        Local* local = &compiler->locals[i];
        if (token_identifiers_equal(name, &local->name))
        {
            if (local->depth == -1)
            {
                raise_error(
                    "Can't read local variable in its own initializer.");
            }

            return i;
        }
    }

    return -1;
}

static int compiler_upvalue_resolve(Compiler* compiler, Token* name)
{
    if (compiler->enclosing == NULL) return -1;

    int upvalue = compiler_upvalue_resolve(compiler->enclosing, name);
    if (upvalue != -1)
        return compiler_upvalue_add(compiler, (uint8_t)upvalue, false);

    int local = compiler_local_resolve(compiler->enclosing, name);
    if (local != -1)

    {
        compiler->enclosing->locals[local].is_captured = true;
        return compiler_upvalue_add(compiler, (uint8_t)local, true);
    }

    return -1;
}

static void compiler_local_mark_initialized()
{
    if (current_compiler->scope_depth == 0) return;

    current_compiler->locals[current_compiler->local_count - 1].depth =
        current_compiler->scope_depth;
}

static void compiler_declare_variable()
{
    if (current_compiler->scope_depth == 0) return;

    Token* name = &parser.previous;

    for (int i = current_compiler->local_count - 1; i >= 0; --i)
    {
        Local* local = &current_compiler->locals[i];

        if (local->depth != -1 && local->depth < current_compiler->scope_depth)
            break;

        if (token_identifiers_equal(name, &local->name))
            raise_error("Already a variable with this name in this scope.");
    }

    compiler_local_add(*name);
}

static void raise_error_at(Token* token, const char* message)
{
    if (parser.panic_mode) return;

    parser.panic_mode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing.
    }
    else
    {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void raise_error(const char* message)
{
    raise_error_at(&parser.previous, message);
}

static void raise_error_at_current(const char* message)
{
    raise_error_at(&parser.current, message);
}

static void move_to_next_token()
{
    parser.previous = parser.current;

    while (true)
    {
        parser.current = scanner_scan_token();
        if (parser.current.type != TOKEN_ERROR) break;

        raise_error_at_current(parser.current.start);
    }
}

static void sync_errors()
{
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF)
    {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:; // Do nothing.
        }

        move_to_next_token();
    }
}

static void expect_token_or_fail(TokenType type, const char* message)
{
    if (parser.current.type == type)
    {
        move_to_next_token();
        return;
    }

    raise_error_at_current(message);
}

static bool expect_token(TokenType type)
{
    if (!current_token_is(type)) return false;

    move_to_next_token();
    return true;
}

static ParseRule* get_rule(TokenType type)
{
    return &rules[type];
}

///////////////////////////////////////////////////////////////////////////////////////
// EMITTERS
///////////////////////////////////////////////////////////////////////////////////////

static uint8_t constant_make(Value value)
{
    int constant = chunk_constant_add(current_chunk(), value);
    if (constant > UINT8_MAX)
    {
        raise_error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static uint8_t constant_identifier(Token* name)
{
    return constant_make(
        value_make_obj(obj_string_cpy(name->start, name->length)));
}

static bool token_identifiers_equal(Token* a, Token* b)
{
    if (a->length != b->length) return false;

    return memcmp(a->start, b->start, a->length) == 0;
}

static void byte_emit(uint8_t byte)
{
    chunk_write(current_chunk(), byte, parser.previous.line);
}

static void byte_emit_duo(uint8_t byte1, uint8_t byte2)
{
    byte_emit(byte1);
    byte_emit(byte2);
}

static void byte_emit_var_def(uint8_t global)
{
    if (current_compiler->scope_depth > 0)
    {
        compiler_local_mark_initialized();
        return;
    }

    byte_emit_duo(OP_DEFINE_GLOBAL, global);
}

static void byte_emit_named_variable(Token name, bool can_assign)
{
    uint8_t get_op, set_op;
    int arg = compiler_local_resolve(current_compiler, &name);

    if (arg != -1)
    {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    }
    else if ((arg = compiler_upvalue_resolve(current_compiler, &name)) != -1)
    {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    }
    else
    {
        arg = constant_identifier(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && expect_token(TOKEN_EQUAL))
    {
        parse_expression();

        byte_emit_duo(set_op, (uint8_t)arg);

        return;
    }

    byte_emit_duo(get_op, (uint8_t)arg);
}

static void byte_emit_variable(bool can_assign)
{
    byte_emit_named_variable(parser.previous, can_assign);
}

static int byte_emit_jump(uint8_t instruction)
{
    byte_emit(instruction);
    byte_emit(0xFF);
    byte_emit(0xFF);

    return current_chunk()->count - 2;
}

static void byte_emit_patch_jump(int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) raise_error("Too much code to jump over.");

    current_chunk()->code[offset] = (jump >> 8) & 0xFF;
    current_chunk()->code[offset + 1] = jump & 0xFF;
}

static void byte_emit_loop(int loop_start)
{
    byte_emit(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) raise_error("Loop body too large.");

    byte_emit((offset >> 8) & 0xFF);
    byte_emit(offset & 0xFF);
}

static void byte_emit_return()
{
    byte_emit(OP_NIL);
    byte_emit(OP_RETURN);
}

static void byte_emit_constant(Value value)
{
    byte_emit_duo(OP_CONSTANT, constant_make(value));
}

///////////////////////////////////////////////////////////////////////////////////////
// PARSING FUNCTIONS
///////////////////////////////////////////////////////////////////////////////////////

static void parse_precedence(Precedence precedence)
{
    move_to_next_token();
    ParseFn parse_prefix = get_rule(parser.previous.type)->prefix;
    if (parse_prefix == NULL)
    {
        raise_error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    parse_prefix(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence)
    {
        move_to_next_token();
        ParseFn parse_infix = get_rule(parser.previous.type)->infix;
        parse_infix(can_assign);
    }

    if (can_assign && expect_token(TOKEN_EQUAL))
    {
        raise_error("Invalid assignment target.");
    }
}

static void parse_grouping(bool can_assign)
{
    (void)can_assign;

    parse_expression();
    expect_token_or_fail(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void parse_binary(bool can_assign)
{
    (void)can_assign;

    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (operator_type)
    {
        case TOKEN_BANG_EQUAL:
            byte_emit_duo(OP_EQUAL, OP_NOT);
            break;

        case TOKEN_EQUAL_EQUAL:
            byte_emit(OP_EQUAL);
            break;

        case TOKEN_GREATER:
            byte_emit(OP_GREATER);
            break;

        case TOKEN_GREATER_EQUAL:
            byte_emit_duo(OP_LESS, OP_NOT);
            break;

        case TOKEN_LESS:
            byte_emit(OP_LESS);
            break;

        case TOKEN_LESS_EQUAL:
            byte_emit_duo(OP_GREATER, OP_NOT);
            break;

        case TOKEN_PLUS:
            byte_emit(OP_ADD);
            break;

        case TOKEN_MINUS:
            byte_emit(OP_SUBTRACT);
            break;

        case TOKEN_STAR:
            byte_emit(OP_MULTIPLY);
            break;

        case TOKEN_SLASH:
            byte_emit(OP_DIVIDE);
            break;

        default:
            return; // Unreachable.
    }
}

static void parse_unary(bool can_assign)
{
    (void)can_assign;

    TokenType operator_type = parser.previous.type;

    // Compile the operand.
    parse_precedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operator_type)
    {
        case TOKEN_BANG:
            byte_emit(OP_NOT);
            break;

        case TOKEN_MINUS:
            byte_emit(OP_NEGATE);
            break;

        default:
            return; // Unreachable.
    }
}

static void parse_number(bool can_assign)
{
    (void)can_assign;

    double value = strtod(parser.previous.start, NULL);
    byte_emit_constant(value_make_number(value));
}

static void parse_literal(bool can_assign)
{
    (void)can_assign;

    switch (parser.previous.type)
    {
        case TOKEN_FALSE:
            byte_emit(OP_FALSE);
            break;

        case TOKEN_NIL:
            byte_emit(OP_NIL);
            break;

        case TOKEN_TRUE:
            byte_emit(OP_TRUE);
            break;

        default:
            return; // Unreachable.
    }
}

static void parse_string(bool can_assign)
{
    (void)can_assign;

    byte_emit_constant(value_make_obj(
        obj_string_cpy(parser.previous.start + 1, parser.previous.length - 2)));
}

static void parse_call(bool can_assign)
{
    (void)can_assign;

    uint8_t argc = parse_argument_list();
    byte_emit_duo(OP_CALL, argc);
}

static void parse_dot(bool can_assign)
{
    expect_token_or_fail(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = constant_identifier(&parser.previous);

    if (can_assign && expect_token(TOKEN_EQUAL))
    {
        parse_expression();
        byte_emit_duo(OP_SET_PROPERTY, name);
    }
    else
    {
        byte_emit_duo(OP_GET_PROPERTY, name);
    }
}

static void parse_list(bool can_assign)
{
    (void)can_assign;

    int item_count = 0;
    if (!current_token_is(TOKEN_RIGHT_BRACKET))
    {
        do
        {
            // Trailing comma case
            if (current_token_is(TOKEN_RIGHT_BRACKET)) break;

            parse_precedence(PREC_OR);

            if (item_count == UINT8_COUNT)
                raise_error(
                    "Cannot have more than 256 items in a list literal.");

            item_count++;
        } while (expect_token(TOKEN_COMMA));
    }

    expect_token_or_fail(TOKEN_RIGHT_BRACKET, "Expect ']' after list literal.");

    byte_emit_duo(OP_LIST_INIT, item_count);
}

static void parse_subscript(bool can_assign)
{
    parse_precedence(PREC_OR);
    expect_token_or_fail(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (can_assign && expect_token(TOKEN_EQUAL))
    {
        parse_expression();
        byte_emit(OP_LIST_SETIDX);
        return;
    }

    byte_emit(OP_LIST_GETIDX);
}

static void parse_expression()
{
    parse_precedence(PREC_ASSIGNMENT);
}

static uint8_t parse_variable(const char* error_message)
{
    expect_token_or_fail(TOKEN_IDENTIFIER, error_message);

    compiler_declare_variable();
    if (current_compiler->scope_depth > 0) return 0;

    return constant_identifier(&parser.previous);
}

static uint8_t parse_argument_list()
{
    uint8_t argc = 0;

    if (!current_token_is(TOKEN_RIGHT_PAREN))
    {
        do
        {
            parse_expression();

            if (argc == 255)
            {
                raise_error("Can't have more than 255 arguments.");
            }

            argc++;
        } while (expect_token(TOKEN_COMMA));
    }

    expect_token_or_fail(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

    return argc;
}

static void parse_fun_declaration()
{
    uint8_t global = parse_variable("Expect function name.");
    compiler_local_mark_initialized();
    parse_function(CP_FUNCTION_BODY);
    byte_emit_var_def(global);
}

static void parse_class_method()
{
    expect_token_or_fail(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = constant_identifier(&parser.previous);

    parse_function(CP_FUNCTION_BODY);
    byte_emit_duo(OP_METHOD, constant);
}

static void parse_class_declaration()
{
    expect_token_or_fail(TOKEN_IDENTIFIER, "Expect class name.");
    Token class_name = parser.previous;
    uint8_t name_constant = constant_identifier(&parser.previous);
    compiler_declare_variable();

    byte_emit_duo(OP_CLASS, name_constant);
    byte_emit_var_def(name_constant);

    byte_emit_named_variable(class_name, false);
    expect_token_or_fail(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    while (!current_token_is(TOKEN_RIGHT_BRACE) && !current_token_is(TOKEN_EOF))
        parse_class_method();

    expect_token_or_fail(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    byte_emit(OP_POP);
}

static void parse_var_declaration()
{
    uint8_t global = parse_variable("Expect variable name.");

    if (expect_token(TOKEN_EQUAL))
    {
        parse_expression();
    }
    else
    {
        byte_emit(OP_NIL);
    }

    expect_token_or_fail(TOKEN_SEMICOLON,
                         "Expect ';' after variable declaration.");

    byte_emit_var_def(global);
}

static void parse_declaration()
{
    if (expect_token(TOKEN_CLASS))
    {
        parse_class_declaration();
    }
    else if (expect_token(TOKEN_FUN))
    {
        parse_fun_declaration();
    }
    else if (expect_token(TOKEN_VAR))
    {
        parse_var_declaration();
    }
    else
    {
        parse_statement();
    }

    if (parser.panic_mode) sync_errors();
}

static void parse_and(bool can_assign)
{
    (void)can_assign;

    int end_jump = byte_emit_jump(OP_JUMP_IF_FALSE);

    byte_emit(OP_POP);

    parse_precedence(PREC_AND);

    byte_emit((uint8_t)end_jump);
}

static void parse_or(bool can_assign)
{
    (void)can_assign;

    int else_jump = byte_emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = byte_emit_jump(OP_JUMP);

    byte_emit_patch_jump(else_jump);
    byte_emit(OP_POP);

    parse_precedence(PREC_OR);

    byte_emit_patch_jump(end_jump);
}

static void parse_print_statement()
{
    parse_expression();
    expect_token_or_fail(TOKEN_SEMICOLON, "Expect ';' after value.");

    byte_emit(OP_PRINT);
}

static void parse_if_statement()
{
    expect_token_or_fail(TOKEN_LEFT_PAREN, " Expect '(' after 'if'.");

    parse_expression();

    expect_token_or_fail(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = byte_emit_jump(OP_JUMP_IF_FALSE);
    byte_emit(OP_POP);

    parse_statement();

    int else_jump = byte_emit_jump(OP_JUMP);

    byte_emit_patch_jump(then_jump);
    byte_emit(OP_POP);

    if (expect_token(TOKEN_ELSE)) parse_statement();

    byte_emit_patch_jump(else_jump);
}

static void parse_return_statement()
{
    if (current_compiler->code_placement == CP_MAIN_BODY)
    {
        raise_error("Can't return from top-level code.");
    }

    if (expect_token(TOKEN_SEMICOLON))
    {
        byte_emit_return();
        return;
    }

    parse_expression();
    expect_token_or_fail(TOKEN_SEMICOLON, "Expect ';' after return value.");
    byte_emit(OP_RETURN);
}

static void parse_while_statement()
{
    int loop_start = current_chunk()->count;

    expect_token_or_fail(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");

    parse_expression();

    expect_token_or_fail(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = byte_emit_jump(OP_JUMP_IF_FALSE);
    byte_emit(OP_POP);

    parse_statement();

    byte_emit_loop(loop_start);

    byte_emit_patch_jump(exit_jump);
    byte_emit(OP_POP);
}

static void parse_for_statement()
{
    compiler_scope_begin();

    expect_token_or_fail(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if (expect_token(TOKEN_SEMICOLON))
    {
        // No initializer
    }
    else if (expect_token(TOKEN_VAR))
    {
        parse_var_declaration();
    }
    else
    {
        parse_expression_statement();
    }

    int loop_start = current_chunk()->count;

    int exit_jump = -1;
    if (!expect_token(TOKEN_SEMICOLON))
    {
        parse_expression();
        expect_token_or_fail(TOKEN_SEMICOLON,
                             "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exit_jump = byte_emit_jump(OP_JUMP_IF_FALSE);
        byte_emit(OP_POP); // Condition.
    }

    if (!expect_token(TOKEN_RIGHT_PAREN))
    {
        int body_jump = byte_emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;

        parse_expression();
        byte_emit(OP_POP);

        expect_token_or_fail(TOKEN_RIGHT_PAREN,
                             "Expect ')' after for clauses.");

        byte_emit_loop(loop_start);
        loop_start = increment_start;
        byte_emit_patch_jump(body_jump);
    }

    parse_statement();
    byte_emit_loop(loop_start);

    if (exit_jump != -1)
    {
        byte_emit_patch_jump(exit_jump);
        byte_emit(OP_POP); // Condition.
    }

    compiler_scope_end();
}

static void parse_expression_statement()
{
    parse_expression();
    expect_token_or_fail(TOKEN_SEMICOLON, "Expect ';' after expression.");

    byte_emit(OP_POP);
}

static void parse_block()
{
    while (!current_token_is(TOKEN_RIGHT_BRACE) && !current_token_is(TOKEN_EOF))
        parse_declaration();

    expect_token_or_fail(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void parse_function(CodePlacement code_placement)
{
    Compiler compiler;
    compiler_init(&compiler, code_placement);

    compiler_scope_begin();

    expect_token_or_fail(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    if (!current_token_is(TOKEN_RIGHT_PAREN))
    {
        do
        {
            current_compiler->function->arity++;
            if (current_compiler->function->arity > 255)
                raise_error_at_current("Can't have more than 255 parameters.");

            uint8_t constant = parse_variable("Expect parameter name.");
            byte_emit_var_def(constant);

        } while (expect_token(TOKEN_COMMA));
    }

    expect_token_or_fail(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    expect_token_or_fail(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    parse_block();

    ObjFunction* function = compiler_finalize();
    byte_emit_duo(OP_CLOSURE, constant_make(value_make_obj(function)));

    for (int i = 0; i < function->upvalue_count; ++i)
    {
        byte_emit(compiler.upvalues[i].is_local ? 1 : 0);
        byte_emit(compiler.upvalues[i].index);
    }
}

static void parse_statement()
{
    if (expect_token(TOKEN_PRINT))
    {
        parse_print_statement();
    }
    else if (expect_token(TOKEN_FOR))
    {
        parse_for_statement();
    }
    else if (expect_token(TOKEN_IF))
    {
        parse_if_statement();
    }
    else if (expect_token(TOKEN_RETURN))
    {
        parse_return_statement();
    }
    else if (expect_token(TOKEN_WHILE))
    {
        parse_while_statement();
    }
    else if (expect_token(TOKEN_LEFT_BRACE))
    {
        compiler_scope_begin();

        parse_block();

        compiler_scope_end();
    }
    else
    {
        parse_expression_statement();
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// COMPILATION
///////////////////////////////////////////////////////////////////////////////////////

static ObjFunction* compiler_finalize()
{
    byte_emit_return();

    ObjFunction* function = current_compiler->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error)
    {
        chunk_disassemble(current_chunk(), function->name != NULL
                                               ? function->name->chars
                                               : "<Main Body>");
    }
#endif

    current_compiler = current_compiler->enclosing;

    return function;
}

ObjFunction* compile(const char* source)
{
    scanner_init(source);

    Compiler compiler;
    compiler_init(&compiler, CP_MAIN_BODY);

    parser.had_error = false;
    parser.panic_mode = false;

    move_to_next_token();

    while (!expect_token(TOKEN_EOF))
    {
        parse_declaration();
    }

    ObjFunction* function = compiler_finalize();

    return parser.had_error ? NULL : function;
}

void gc_mark_compiler_roots()
{
    Compiler* compiler = current_compiler;
    while (compiler != NULL)
    {
        gc_mark_obj((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
