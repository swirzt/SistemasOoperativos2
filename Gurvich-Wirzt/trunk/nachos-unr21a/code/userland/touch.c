/// Creates files specified on the command line.

#include "syscall.h"
#include "lib.c"

#define ARGC_ERROR "Error: missing argument."
#define CREATE_ERROR "Error: could not create file."

int main(int argc, char *argv[])
{
    char buffer[1000];
    itoa(argc, buffer);
    Write(buffer, strlen(buffer), CONSOLE_OUTPUT);
    if (argc < 1)
    {
        Write(ARGC_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }

    int success = 1;
    for (unsigned i = 1; i < argc; i++)
    {
        if (Create(argv[i]) < 0)
        {
            Write(CREATE_ERROR, sizeof(CREATE_ERROR) - 1, CONSOLE_OUTPUT);
            success = 0;
        }
    }
    return !success;
}
