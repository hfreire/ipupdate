#ifdef WIN32
	#include <winsock2.h>
#else
	#include <netinet/in.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#pragma pack(1)		//Turn off structure alignment

#define FLAGS_QR		0x8000	//Query or Response
#define FLAGS_OPCODE	0x7800	//Operation Code
#define FLAGS_AA		0x0400	//Authoritative Answer
#define FLAGS_TC		0x0200	//Truncated
#define FLAGS_RD		0x0100	//Recursion Desired
#define FLAGS_RA		0x0080	//Recursion Available
#define FLAGS_Z			0x0070	//Reserved
#define FLAGS_RCODE		0x000F	//Response Code

#define TYPE_A			1
#define TYPE_NS			2
#define TYPE_MD			3
#define TYPE_MF			4
#define TYPE_CNAME		5
#define TYPE_SOA		6
#define TYPE_MB			7
#define TYPE_MG			8
#define TYPE_MR			9
#define TYPE_NULL		10
#define TYPE_WKS		11
#define TYPE_PTR		12
#define TYPE_HINFO		13
#define TYPE_MINFO		14
#define TYPE_MX			15
#define TYPE_TXT		16
#define TYPE_RP			17
#define TYPE_AFSDB		18
#define TYPE_X25		19
#define TYPE_ISDN		20
#define TYPE_RT			21
#define TYPE_NSAP		22
#define TYPE_NSAP_PTR	23
#define TYPE_SIG		24
#define TYPE_KEY		25
#define TYPE_PX			26
#define TYPE_GPOS		27
#define TYPE_AAAA		28
#define TYPE_LOC		29
//#define TYPE_
#define TYPE_SRV		33
//#define TYPE_
#define TYPE_TKEY		249	//RFC 2930
#define TYPE_TSIG		250	//RFC 2845
#define TYPE_AXFR		252
#define TYPE_ANY		255

#define CLASS_IN		1	//RFC 1035
#define CLASS_CS		2	//RFC 1035
#define CLASS_CH		3	//RFC 1035
#define CLASS_HS		4	//RFC 1035
#define CLASS_NONE		254	//RFC 2136
#define CLASS_ANY		255	//RFC 1035

#define RCODE_NOERROR	0	//RFC 1035
#define RCODE_FORMERR	1	//RFC 1035
#define RCODE_SERVFAIL	2	//RFC 1035
#define RCODE_NXDOMAIN	3	//RFC 1035
#define RCODE_NOTIMP	4	//RFC 1035
#define RCODE_REFUSED	5	//RFC 1035
#define RCODE_YXDOMAIN	6	//RFC 2136
#define RCODE_RXRRSET	7	//RFC 2136
#define RCODE_NXRRSET	8	//RFC 2136
#define RCODE_NOTAUTH	9	//RFC 2136
#define RCODE_NOTZONE	10	//RFC 2136

//Extended RCODE in TSIG and TKEY data
#define RCODE_BADSIG	16	//RFC 2845
#define RCODE_BADKEY	17	//RFC 2845
#define RCODE_BADTIME	18	//RFC 2845
#define RCODE_BADMODE	19	//RFC 2930
#define RCODE_BADNAME	20	//RFC 2930
#define RCODE_BADALG	21	//RFC 2930

#define OPCODE_QUERY	0	//RFC 1035
#define OPCODE_INVERSE	1	//RFC 1035
#define OPCODE_STATUS	2	//RFC 1035
#define OPCODE_UPDATE	5	//RFC 2136

#define MAXNAMSIZ       256 //RFC 1035

typedef struct {
	unsigned short id;
	unsigned short flags;
	/*
		0....... ........ = Query/Response
		.0000... ........ = Operation Code
		.....0.. ........ = Authoritative Answer
		......0. ........ = Truncated
		.......0 ........ = Recursion Desired
		........ 0....... = Recursion Available
		........ .000.... = Z
		........ ....0000 = Response Code
	*/
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
} DNS_HEADER_0;

typedef struct {
	unsigned short id;
	unsigned short flags;
	/*
		0....... ........ = Query/Response
		.0000... ........ = Operation Code
		.....000 0000.... = Z
		........ ....0000 = Response Code
	*/
	unsigned short zocount;
	unsigned short prcount;
	unsigned short upcount;
	unsigned short adcount;
} DNS_HEADER_5;

#define DNS_HEADER	DNS_HEADER_0

typedef struct {
	char name[MAXNAMSIZ];
	unsigned short type;
	unsigned short class;
} DNS_RECORD_1;

#define DNS_QDRECORD	DNS_RECORD_1
#define DNS_ZORECORD	DNS_RECORD_1

typedef struct {
	char name[MAXNAMSIZ];
	unsigned short type;
	unsigned short class;
	signed long ttl;
	unsigned short rdlen;
	unsigned short pdata;
} DNS_RECORD_2;

#define DNS_ANRECORD	DNS_RECORD_2
#define DNS_NSRECORD	DNS_RECORD_2
#define DNS_ARRECORD	DNS_RECORD_2
#define DNS_PRRECORD	DNS_RECORD_2
#define DNS_UPRECORD	DNS_RECORD_2
#define DNS_ADRECORD	DNS_RECORD_2

typedef struct {
	DNS_HEADER header;
	DNS_QDRECORD* qdrecord;
	DNS_ANRECORD* anrecord;
	DNS_NSRECORD* nsrecord;
	DNS_ARRECORD* arrecord;
} DNS_QUERY;

typedef struct {
	DNS_HEADER_5 header;
	DNS_ZORECORD* zorecord;
	DNS_PRRECORD* prrecord;
	DNS_UPRECORD* uprecord;
	DNS_ADRECORD* adrecord;
} DNS_UPDATE;

typedef struct {
	char* MName;
	char* RName;
	unsigned long Serial;
	unsigned long Refresh;
	unsigned long Retry;
	unsigned long Expire;
	unsigned long Minimum;
} DNS_SOA;

typedef struct {
	unsigned long Address;
	unsigned char Protocol;
	void* BitMap;
} DNS_WKS;

typedef struct {
	char* CPU;
	char* OS;
} DNS_HINFO;

typedef struct {
	char* RMailBX;
	char* EMailBX;
} DNS_MINFO;

typedef struct {
	unsigned short Preference;
	char* Exchange;
} DNS_MX;

typedef struct {
	char* mbox_dname;
	char* txt_dname;
} DNS_RP;

typedef struct {
	unsigned short SubType;
	char* HostName;
} DNS_AFSDB;

typedef struct {
	char* address;
	char* sa;
} DNS_ISDN;

typedef struct {
	unsigned short Preference;
	char* Intermediate_Host;
} DNS_RT;

typedef struct {
	unsigned short ip6[8];
} DNS_AAAA;

typedef struct {
	unsigned char Version;
	unsigned char Size;
	unsigned char Horiz_Pre;
	unsigned char Vert_Pre;
	unsigned long Latitude;
	unsigned long Longitude;
	unsigned long Altitude;
} DNS_LOC;

//HMAC_MD5 TSIG

typedef struct {
	char           algname[MAXNAMSIZ];
	unsigned short hitime;
	unsigned long  lotime;
	unsigned short fudge;
	unsigned short macsize;
	unsigned char  mac[16];
	unsigned short id;
	unsigned short error;
	unsigned short osize;
	char odata[6];
} DNS_TSIG;

// DNS Routines from dns.c

char* dns_strerror(unsigned short);

int dns_t2wname(char*, char*);
int dns_t2wrecord1(char*, unsigned short, unsigned short, char*);
int dns_t2wrecord2(char*, unsigned short, unsigned short, long, unsigned short, char*);
int dns_t2wtsig(char*, unsigned short, char[16], int, unsigned short, char*);

int dns_w2trecord1(char*, int, DNS_RECORD_1*);
int dns_w2trecord2(char*, int, DNS_RECORD_2*);
int dns_w2ttsig(char*, int, DNS_TSIG*);
int dns_w2tname(char*, int, char*);

int dns_w2tquery(char*, DNS_QUERY*);
int dns_w2theader(char*, DNS_HEADER*);
void dns_freequery(DNS_QUERY*);

int dns_w2tupdate(char*, DNS_UPDATE*);
int dns_w2theader5(char*, DNS_HEADER_5*);
void dns_freeupdate(DNS_UPDATE*);

extern void hmac_md5(char*, int, char*, int, char[16]);
