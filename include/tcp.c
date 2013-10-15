#include "tcp.h"

int opentcp(char* host, unsigned short port, long timeout)
{
	int s, inum;
	unsigned long lnum;
	struct hostent* hostinfo;
	struct sockaddr_in sockaddr = { AF_INET };
	struct timeval tv;
	fd_set fds;

	sockaddr.sin_port = htons(port);

	//resolve the server's address
	if ((hostinfo = gethostbyname(host)) == NULL)
		return -1;

#ifndef WIN32
	// make sure h_errno is set to 0 to avoid confusion
	// for some reason I've seen this set to 1 on success
	h_errno = 0;
#endif

	memcpy(&sockaddr.sin_addr.s_addr, hostinfo->h_addr, 4);

	//create a tcp socket
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == -1)
		return -1;

	lnum = 1;
#ifdef WIN32
	ioctlsocket(s, FIONBIO, &lnum);
#else
	ioctl(s, FIONBIO, &lnum);
#endif

	connect(s, (struct sockaddr*)&sockaddr, sizeof(sockaddr));

	FD_ZERO(&fds);
	FD_SET(s, &fds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	if (select(FD_SETSIZE, NULL, &fds, NULL, &tv))
	{
		inum = sizeof(int);

#ifdef WIN32
		getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&lnum, &inum);
#else
		getsockopt(s, SOL_SOCKET, SO_ERROR, &lnum, (socklen_t*)&inum);
#endif
		setserr(lnum);

		if (lnum != 0)
		{
			closetcp(s);
			return -1;
		}
	}
	else
	{
		setserr(ETIMEDOUT);
		closetcp(s);
		return -1;
	}

	lnum = 0;
#ifdef WIN32
	ioctlsocket(s, FIONBIO, &lnum);
#else
	ioctl(s, FIONBIO, &lnum);
#endif

	return s;
}

int openudp(char* host, unsigned short port)
{
	int s;
	struct hostent* hostinfo;
	struct sockaddr_in sockaddr = { AF_INET };

	sockaddr.sin_port = htons(port);

	//resolve the server's address
	if ((hostinfo = gethostbyname(host)) == NULL)
		return -1;

#ifndef WIN32
	// make sure h_errno is set to 0 to avoid confusion
	// for some reason I've seen this set to 1 on success
	h_errno = 0;
#endif

	memcpy(&sockaddr.sin_addr.s_addr, hostinfo->h_addr, 4);

	//create a tcp socket
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1)
		return -1;

	//bind the socket to the remote address
	if (connect(s, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1)
	{
		closetcp(s);
		return -1;
	}

	return s;
}

void closetcp(int s)
{
#ifdef WIN32
	closesocket(s);
#else
    close(s);
#endif
}

#ifdef WIN32
int InitWinsock(void)
{
	WSADATA wsaData;
	return WSAStartup(0x0101,&wsaData);
}
#endif

void setserr(int err)
{
#ifdef WIN32
	WSASetLastError(err);
#else
	errno = err;
#endif
}

char* strserr(void)
{
#ifdef WIN32
	static char errmsg[64];
	int serr = WSAGetLastError();

	switch (serr) {
	case ETIMEDOUT:
		strcpy(errmsg, "Connection timed out");
		break;
	case ECONNREFUSED:
		strcpy(errmsg, "Connection refused");
		break;
	case ECONNRESET:
		strcpy(errmsg, "Connection reset by peer");
		break;
	case ENETUNREACH:
		strcpy(errmsg, "Network is unreachable");
		break;
	case EADDRNOTAVAIL:
		strcpy(errmsg, "Cannot assign requested address");
		break;
	case HOST_NOT_FOUND:
		strcpy(errmsg, "host not found");
		break;
	case TRY_AGAIN:
		strcpy(errmsg, "temporary dns error");
		break;
	case NO_RECOVERY:
		strcpy(errmsg, "permenant dns error");
		break;
	case NO_DATA:
		strcpy(errmsg, "record not found");
		break;
	default:
		sprintf(errmsg, "Socket Error (%u)", serr);
	}
	return errmsg;
#else
	if (h_errno)
		return (char*)hstrerror(h_errno);
	else
		return (char*)strerror(errno);
#endif
}
