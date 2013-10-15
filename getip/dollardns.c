#ifndef WIN32
	#include <string.h>
#endif

#include <stdio.h>
#include "../include/tcp.h"

int main (int argc, char **argv)
{
#ifdef WIN32
	int err;

	if ((err = InitWinsock()) != 0)
	{
		printf("InitWinsock: Unable to initialize winsock 1.1 (%u)\n", err);
		return 1;
	}
#endif

	char buffer[1024];
	char* ptr = buffer;
	int l, s;

	// connect to the dollardns server port 443
	if ((s = opentcp("myip.dollardns.net", 443, 10)) == -1)
	{
		printf("Connection failed: %s\n", strserr());
		return 1;
	}

	// send the http request
	if (send(s, "GET / HTTP/1.1\r\nHost: myip.dollardns.net\r\nUser-Agent: getip\r\n\r\n", 68, 0) == -1)
	{
		printf("Send failed: %s\n", strserr());
		return 1;
	}

	// read up to 1023 bytes of the response into the buffer
	while ((l = recv(s, ptr, 1023 - (ptr - buffer), 0)) > 0)
	{
		ptr += l;
	}
	closetcp(s);

	// null terminate the data
	buffer[ptr - buffer] = 0;

	// skip the headers
	if ((ptr = strstr(buffer, "\r\n\r\n")) != NULL)
	{
		ptr += 4;

		// if the response is chunked, else it isn't
		if ((l = strcspn(ptr, "\r\n")) > 6)
		{
			ptr[l] = 0;
		}
		else if ((ptr = strstr(ptr, "\r\n")) != NULL)
		{
			ptr += 2;
			ptr[strcspn(ptr, "\r\n")] = 0;
		}

		// make sure this line is an IP address
		if (inet_addr(ptr) != INADDR_NONE)
		{
			printf("%s\n", ptr);
			return 0;
		}
	}

	printf("Unrecognized format from server\n");
	return 1;
}
