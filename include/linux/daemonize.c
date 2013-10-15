#include "daemonize.h"

/*
** Daemonize
**
**   Detach from shell/console and close standard handles
**
*/

int daemonize(void)
{
	// start a child process from this point on
	switch (fork()) {
	case 0:
		break;		//child breaks free
	case -1:
		return 1;	//parent fails on error
	default:
		exit(0);	//parent exits
	}

	// child process becomes a session group leader (detach console)
	if (setsid() < 0)
		return 2;

	// create a non-session group leader (prevent future consoles)
	switch (fork()) {
	case 0:
		break;		//new child breaks free
	case -1:
		return 3;	//old child fails on error
	default:
		exit(0);	//old child exits
	}

	// now a non session group leading process backgrounding tasks */
	chdir("/");
	umask(0777);

	//cleanup unused handles
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	return 0;
}
