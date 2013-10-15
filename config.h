#ifdef WIN32
	#include <windows.h>
	#include <winsock.h>     //IPPROTO_UDP/TCP
	extern int asprintf(char**, const char*, ...);
#else
	#define _GNU_SOURCE
	#include <stdlib.h>
	#include <string.h>
	#include <errno.h>
	#include <netinet/in.h>  //IPPROTO_UDP/TCP
#endif

#include <stdio.h>
#include <ctype.h>

#include "include/base64.h"
#include "include/array.h"
#include "include/memm.h"

enum {
	CFG_KEYWORD = 1,
	CFG_QUOTED  = 2,
	CFG_BLOCK   = 3,
	CFG_ERROR   = 4
};

struct options {
	char*           getip[5];
	int             getipc;
	int             logenable;
	char*           logfile;
	char*           pidfile;
	int             checkip;
	int             checkcname;
};

struct key {
	char*           label;
	char*           name;
	char            data[20];	/* 2 extra bytes padding for decode_base64 routine
	                               2 extra bytes for structure alignment */
};

struct zone {
	char*           name;
	char*           hostp;
	char**          hosts;
	int             hostc;
	int             ttl;
	char*           keyname;
	char            keydata[20];	/* 2 extra bytes padding for decode_base64 routine
	                                   2 extra bytes for structure alignment */
};

struct server {
	char*           name;
	unsigned short  port;
	unsigned short  protocol;
	long            timeout;
	struct zone**   zones;
	int             zonec;
	unsigned short  fudge;
	unsigned short  autofudge;
	int             maxfudge;
	int             ttl;
};

struct config {
	struct options  options;
	struct server**	servers;
	int             serverc;
};

extern void PostMsg(char*);
extern void PostMsgFree(char*);

void getconfig(char*, struct config*);

void cfg_options( struct options*, char*);
void cfg_key(struct key*, char*);
void cfg_server(struct server*, char*);
void cfg_zone(struct server*, struct zone*, char*);

int cfg_fread(struct mem*);
int cfg_read(struct mem*, char**, unsigned int*);
int cfg_readblock(char*, char**, unsigned int*);
