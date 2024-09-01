#include <stdio.h>
#include <string.h>

#include "general.h"
#include "scanner.h"

typedef struct
{
    const char* start;
    const char* current;
    int line;
} Scanner;

Scanner scanner;

void scanner_init(const char* source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_at_end()
{
    return *scanner.current == '\0';
}

static char move_to_next_char()
{
    scanner.current++;
    return scanner.current[-1];
}

static char get_current_char()
{
    return *scanner.current;
}

static char get_next_char()
{
    if (is_at_end()) return '\0';

    return scanner.current[1];
}

static bool match_char(char expected)
{
    if (is_at_end()) return false;

    if (*scanner.current != expected) return false;

    scanner.current++;
    return true;
}

static Token token_make(TokenType type)
{
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token token_make_error(const char* message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static void skip_whitespaces()
{
    while (true)
    {
        char c = get_current_char();

        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                move_to_next_char();
                break;

            case '\n':
                scanner.line++;
                move_to_next_char();
                break;

            case '/':
                if (get_next_char() == '/')
                {
                    // A comment goes until the end of line
                    while (get_current_char() != '\n' && !is_at_end())
                        move_to_next_char();
                }
                else
                    return;

                break;

            default:
                return;
        }
    }
}

static TokenType if_can_get_keyword(int start, int length, const char* rest,
                                    TokenType type)
{
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0)
        return type;

    return TOKEN_IDENTIFIER;
}

static TokenType get_identifier_type()
{
    switch (scanner.start[0])
    {
        case 'a':
            return if_can_get_keyword(1, 2, "nd", TOKEN_AND);

        case 'c':
            return if_can_get_keyword(1, 4, "lass", TOKEN_CLASS);

        case 'e':
            return if_can_get_keyword(1, 3, "lse", TOKEN_ELSE);

        case 'f':
            if (scanner.current - scanner.start > 1)
            {
                switch (scanner.start[1])
                {
                    case 'a':
                        return if_can_get_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return if_can_get_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return if_can_get_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;

        case 'i':
            return if_can_get_keyword(1, 1, "f", TOKEN_IF);

        case 'n':
            return if_can_get_keyword(1, 2, "il", TOKEN_NIL);

        case 'o':
            return if_can_get_keyword(1, 1, "r", TOKEN_OR);

        case 'p':
        {
            TokenType t = if_can_get_keyword(1, 6, "rintln", TOKEN_PRINTLN);

            return t != TOKEN_IDENTIFIER
                       ? t
                       : if_can_get_keyword(1, 4, "rint", TOKEN_PRINT);
        }

        case 'r':
            return if_can_get_keyword(1, 5, "eturn", TOKEN_RETURN);

        case 's':
            return if_can_get_keyword(1, 4, "uper", TOKEN_SUPER);

        case 't':
            if (scanner.current - scanner.start > 1)
            {
                switch (scanner.start[1])
                {
                    case 'h':
                        return if_can_get_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return if_can_get_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;

        case 'v':
            return if_can_get_keyword(1, 2, "ar", TOKEN_VAR);

        case 'w':
            return if_can_get_keyword(1, 4, "hile", TOKEN_WHILE);
    }

    return TOKEN_IDENTIFIER;
}

static Token token_make_identifier()
{
    while (is_alpha(get_current_char()) || is_digit(get_current_char()))
        move_to_next_char();

    return token_make(get_identifier_type());
}

static Token token_make_number()
{
    while (is_digit(get_current_char())) move_to_next_char();

    // Look for a fractional part.
    if (get_current_char() == '.' && is_digit(get_next_char()))
    {
        // consume the ".".
        move_to_next_char();

        while (is_digit(get_current_char())) move_to_next_char();
    }

    return token_make(TOKEN_NUMBER);
}

static Token token_make_string()
{
    while (get_current_char() != '"' && !is_at_end())
    {
        if (get_current_char() == '\n') scanner.line++;
        move_to_next_char();
    }

    if (is_at_end()) return token_make_error("Unterminated string.");

    // The closing quote.
    move_to_next_char();
    return token_make(TOKEN_STRING);
}

Token scanner_scan_token()
{
    skip_whitespaces();
    scanner.start = scanner.current;

    if (is_at_end()) return token_make(TOKEN_EOF);

    char c = move_to_next_char();

    if (is_alpha(c)) return token_make_identifier();
    if (is_digit(c)) return token_make_number();

    switch (c)
    {
        case '(':
            return token_make(TOKEN_LEFT_PAREN);

        case ')':
            return token_make(TOKEN_RIGHT_PAREN);

        case '{':
            return token_make(TOKEN_LEFT_BRACE);

        case '}':
            return token_make(TOKEN_RIGHT_BRACE);

        case '[':
            return token_make(TOKEN_LEFT_BRACKET);

        case ']':
            return token_make(TOKEN_RIGHT_BRACKET);

        case ';':
            return token_make(TOKEN_SEMICOLON);

        case ',':
            return token_make(TOKEN_COMMA);

        case '.':
            return token_make(TOKEN_DOT);

        case '-':
            return token_make(TOKEN_MINUS);

        case '+':
            return token_make(TOKEN_PLUS);

        case '/':
            return token_make(TOKEN_SLASH);

        case '*':
            return token_make(TOKEN_STAR);

        case '!':
            return token_make(match_char('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);

        case '=':
            return token_make(match_char('=') ? TOKEN_EQUAL_EQUAL
                                              : TOKEN_EQUAL);

        case '<':
            return token_make(match_char('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

        case '>':
            return token_make(match_char('=') ? TOKEN_GREATER_EQUAL
                                              : TOKEN_GREATER);

        case '"':
            return token_make_string();
    }

    return token_make_error("Unexpected character.");
}