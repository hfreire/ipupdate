#include <stdio.h>
#include <stdlib.h>          // atoi
#include "../include/tcp.h"

int main (int argc, char **argv)
{
	int s;
#ifdef WIN32
	int err;

	if ((err = InitWinsock()) != 0)
	{
		printf("InitWinsock: Unable to initialize winsock 1.1 (%u)\n", err);
		return 1;
	}
#endif
	if ((argc < 3) || (argc > 4))
	{
		printf("Usage: failover <port> <mainip> [failoverip]\n");
		return 1;
	}

	// connect to the mainip's port
	if ((s = opentcp(argv[2], atoi(argv[1]), 10)) == -1)
	{
		// connection failed, return the error or failoverip
		if (argc == 3)
			printf("%s\n", strserr());
		else
			printf("%s\n", argv[3]);
	}
	else
	{
		closetcp(s);
		// connection succeeded, return the mainip
		printf("%s\n", argv[2]);
	}

	return 0;
}
