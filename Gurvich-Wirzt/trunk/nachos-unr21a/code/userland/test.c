#include "syscall.h"

int main()
{
	Create("test.al");
	OpenFileId arch = Open("pepe");
	Write("algo\n", 5, arch);
	Close(arch);
	Exit(0);
	return 0;
}
