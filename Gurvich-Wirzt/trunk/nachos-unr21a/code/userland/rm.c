#include "syscall.h"
#include "lib.c"

int main(int argc, char **argv)
{
    Write(&argc, sizeof(int), 1);
    if (argc == 3)
        putsNachos("Convención normal segun santi");
    if (argc == 1)
        putsNachos("Solo recibe el argumento rmrmrmr");
    if (argc != 2)
    {
        putsNachos("Argumentos incorrectos");
        Exit(-1);
    }

    if (Remove(argv[1]) == -1)
    {
        putsNachos("Couldn't delete file");
        Exit(-1);
    }
    else
    {
        putsNachos("Sucesfully deleted file");
        Exit(0);
    }
}