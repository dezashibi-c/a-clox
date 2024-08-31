#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "general.h"
#include "vm.h"

#define CLOX_REPL_EXIT ":q"

static void repl()
{
    puts("clox REPL");
    printf("Type '%s' to exit.\n", CLOX_REPL_EXIT);

    char line[1024];

    while (true)
    {
        printf("%s", "> ");

        if (!fgets(line, sizeof(line), stdin))
        {
            puts("");
            break;
        }

        if (strncmp(line, CLOX_REPL_EXIT, strlen(CLOX_REPL_EXIT)) == 0) break;

        vm_interpret(line);
    }
}

static char* file_read(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file '%s'.\n", path);
        exit(74);
    }

    fseek(file, 0l, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read '%s'.\n", path);
        exit(74);
    }

    size_t byte_read = fread(buffer, sizeof(char), file_size, file);
    if (byte_read < file_size)
    {
        fprintf(stderr, "Could not read file '%s'.\n", path);
        exit(74);
    }

    buffer[byte_read] = '\0';

    fclose(file);
    return buffer;
}

static void file_run(const char* path)
{
    char* source = file_read(path);
    InterpretResult result = vm_interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[])
{
    vm_init();

    if (argc == 1)
        repl();
    else if (argc == 2)
        file_run(argv[1]);
    else
    {
        fprintf(stderr, "Usage: clox [path]\n");
    }

    // Clean ups
    vm_free();

    return 0;
}