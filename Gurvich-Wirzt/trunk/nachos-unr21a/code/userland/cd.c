#include "syscall.h"
#include "lib.c"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        putsNachos("Argumentos incorrectos\n");
        Exit(-1);
    }
    if (Cd(argv[1]))
    {
        putsNachos("Error cambiando directorio\n");
        Exit(-1);
    }

    Exit(0);
}