#ifdef WIN32
	#include <windows.h>
	#define getpid GetCurrentProcessId
	#include <winsock2.h>
	#include <getopt.h>
	extern int asprintf(char**, const char*, ...);

	#include "include/win32/include.h"
#else
	#define _GNU_SOURCE
	#include <stdio.h>
	#include <string.h>

	#include "include/linux/include.h"
#endif

#include <sys/types.h>
#include <signal.h>
#include <time.h>

#include "include/array.h"
#include "include/dns.h"
#include "include/md5.h"
#include "include/memm.h"
#include "include/tcp.h"
#include "config.h"
