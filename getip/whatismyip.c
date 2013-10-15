#ifndef WIN32
        #include <string.h>
#endif

#include <ctype.h>
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
	int i, l, s;

	// connect to the whatismyip server port 80
	if ((s = opentcp("www.whatismyip.com", 80, 10)) == -1)
	{
		printf("Connection failed: %s\n", strserr());
		return 1;
	}

	// send the http request
	if (send(s, "GET / HTTP/1.1\r\n" \
	            "Host: www.whatismyip.com\r\n" \
	            "User-Agent: getip\r\n" \
	            "Connection: close\r\n\r\n", 82, 0) == -1)
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
		// skip the next three lines
		if (((ptr = strstr(ptr+4, "\r\n")) != NULL) &&
		    ((ptr = strstr(ptr+2, "\r\n")) != NULL) &&
		    ((ptr = strstr(ptr+2, "\r\n")) != NULL))
		{
			ptr += 2;
			// skip to the beginning of the ip
			for (; !isdigit(ptr[0]); ptr++);

			// null terminate the ip
			for (i=0; ptr[i]; i++)
			{
				if (ptr[i] == ' ')
				{
					ptr[i] = 0;
					break;
				}
			}

			// make sure this line is an IP address
			if (inet_addr(ptr) != INADDR_NONE)
			{
				printf("%s\n", ptr);
				return 0;
			}
		}
	}

	printf("Unrecognized format from server\n");
	return 1;
}
