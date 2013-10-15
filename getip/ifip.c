#ifdef WIN32
#include <winsock2.h>
#else
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>   //inet_ntoa
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#include <stdio.h>

int main (int argc, char **argv)
{
	int s;
	struct in_addr addr;
#ifdef WIN32
	WSADATA wsaData;
	SOCKET_ADDRESS_LIST *slist;
	DWORD i;
	char buff[4096];
#else
	struct ifreq ifr;
	int i;
#endif

#ifdef WIN32
	if (WSAStartup(0x0202,&wsaData) != 0)
	{
		printf("ifip: WSAStartup: error (%u)\n", WSAGetLastError());
		return 1;
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("ifip: socket: error (%u)\n", WSAGetLastError());
		return 1;
	}
	if (WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, NULL, 0, &buff, sizeof(buff), (unsigned long*)&i, NULL, NULL) == -1)
	{
		printf("ifip: WSAIoctl: error (%u)\n", WSAGetLastError());
		return 1;
	}
	slist = (SOCKET_ADDRESS_LIST*)buff;

	switch (argc) {
	case 2:
		if ((i = atoi(argv[1])) >= slist->iAddressCount)
		{
			printf("ifip: %u: interface index too high\n", (unsigned int)i);
			return 1;
		}
		break;
	case 1:
		i = 0;
		break;
	default:
		printf("Usage: ifip [interface]\n");
		return 1;
	}

	addr = ((struct sockaddr_in*)slist->Address[i].lpSockaddr)->sin_addr;
#else
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("ifip: socket: %s\n", strerror(errno));
		return 1;
	}

	switch (argc) {
	case 2:
		if (strlen(argv[1]) > IFNAMSIZ)
		{
			printf("ifip: interface too long\n");
			return 1;
		}
		strcpy(ifr.ifr_name, argv[1]);
		break;
	case 1:
		for (i=1;; i++)
		{
			ifr.ifr_ifindex = i;
			if (ioctl(s, SIOCGIFNAME, &ifr) < 0)
			{
				if (errno == ENODEV)
				{
					printf("ifip: No acceptable interfaces found\n");
					return 1;
				}
				continue;
			}

			// make sure this interface index is not loopback or offline
			if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
			{
				printf("%u: %s: ioctl(SIOCGIFFLAGS): %s\n", i, ifr.ifr_name, strerror(errno));
				continue;
			}
			if ((ifr.ifr_flags & IFF_UP) == 0)
				continue;
			if ((ifr.ifr_flags & IFF_LOOPBACK) == IFF_LOOPBACK)
				continue;

			break;
		}
		break;
	default:
		printf("Usage: ifip [interface]\n");
		return 1;
	}

	// get the interface address
	if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
	{
		printf("ifip: %s: %s\n", ifr.ifr_name, strerror(errno));
		return 1;
	}

	addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
#endif

	printf("%s\n", inet_ntoa(addr));
	return 0;
}
