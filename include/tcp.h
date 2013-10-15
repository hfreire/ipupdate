#ifdef WIN32
	#include <winsock2.h>
	#define EADDRNOTAVAIL  WSAEADDRNOTAVAIL
	#define ENETUNREACH    WSAENETUNREACH
	#define ECONNRESET     WSAECONNRESET
	#define ETIMEDOUT      WSAETIMEDOUT
	#define ECONNREFUSED   WSAECONNREFUSED
	#define HOST_NOT_FOUND WSAHOST_NOT_FOUND
	#define TRY_AGAIN      WSATRY_AGAIN
	#define NO_RECOVERY    WSANO_RECOVERY
	#define NO_DATA        WSANO_DATA
	#include <stdio.h>
	#include <io.h>

	int InitWinsock(void);
#else
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <string.h>
	#include <unistd.h>
	#include <errno.h>
#endif

int opentcp(char*, unsigned short, long);
int openudp(char*, unsigned short);
void closetcp(int);
void setserr(int);
char* strserr(void);
