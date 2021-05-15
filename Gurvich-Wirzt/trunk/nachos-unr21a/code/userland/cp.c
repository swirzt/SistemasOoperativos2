#include "syscall.h"
#include "lib.c"

int main(int argc, char **argv)
{
    Write(&argc, sizeof(int), 1);
    if (argc == 4)
        putsNachos("Convención normal segun santi");
    if (argc == 2)
        putsNachos("Solo recibe el argumento cat.c");
    if (argc != 3)
    {
        putsNachos("Argumentos incorrectos");
        Exit(-1);
    }

    OpenFileId filesrc = Open(argv[1]);

    if (filesrc < 2)
    {
        putsNachos("Error abriendo archivo");
        Exit(-1);
    }

    if (Create(argv[2]) == -1)
    {
        putsNachos("Error creando archivo");
        Exit(-1);
    }

    OpenFileId filedst = Open(argv[2]);

    if (filedst < 2)
    {
        putsNachos("Error abriendo archivo");
        Exit(-1);
    }

    char buffer[1];
    while (Read(buffer, 1, filesrc))
        Write(buffer, 1, filedst);

    Close(filesrc);
    Close(filedst);
    Exit(0);
}