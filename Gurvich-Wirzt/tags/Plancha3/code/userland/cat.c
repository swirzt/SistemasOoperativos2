#include "syscall.h"
#include "lib.c"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		putsNachos("Argumentos incorrectos \n");
		Exit(-1);
	}

	OpenFileId file = Open(argv[1]);

	if (file < 2)
	{
		putsNachos("Error abriendo archivo");
		Exit(-1);
	}

	char buffer[1];
	while (Read(buffer, 1, file))
		Write(buffer, 1, CONSOLE_OUTPUT);
	Write("\n", 1, CONSOLE_OUTPUT);

	Close(file);
	Exit(0);
}