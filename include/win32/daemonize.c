#include "daemonize.h"

int daemonize(void)
{
	FreeConsole();

	//cleanup unused handles
	close(STDIN);
	close(STDOUT);
	close(STDERR);

	return 0;
}
