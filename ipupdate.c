#include "ipupdate.h"

void getoptions(int, char**);
void LoadConfig(void);
int DoUpdate(int);

int GetIP(void);
int checkcname(int*, struct server*, struct zone*);
int checkip(int*, struct server*, struct zone*);
int updateip(int*, struct server*, struct zone*);
int SendPacket(int*, struct server*);

void start(void);
int stop(void);

void PostMsg(char*);
void PostMsgFree(char*);

/*
** Global Variables
*/

char config[256];          //config path and filename
FILE* hpidFile;            //handle to the process id file

char aaddr[16] = {0};      // ascii form of the IP
int naddr = INADDR_NONE;   // numeric form of the IP

char *version = "1.1.1";

struct config cfg;

int RUNMODE = 0;
int STARTED = 0;

char memmap[512];          //memory buffer
char *MEM1 = memmap;       //MEM 1 & 2 used for general data,	256 length each
char *MEM2 = &memmap[256];
char *msg;                 //Pointer for asprintf returned strings
struct mem pkt;            //structure holding packet memory information

int main (int argc, char **argv)
{
	int err;

#ifdef WIN32
//Configure windows file paths to executeable directory
	GetModuleFileName(NULL, config, 256);

	config[strrchr(config, '\\') - config] = 0;
	strcpy(MEM1, config);
	strcpy(MEM2, config);
	SetCurrentDirectory(config);

	strcat(config, "\\ipupdate.conf");
	strcat(MEM1, "\\ipupdate.log");
	strcat(MEM2, "\\ipupdate.pid");

	asprintf(&cfg.options.logfile, MEM1);
	asprintf(&cfg.options.pidfile, MEM2);

	if ((err = InitWinsock()) != 0)
	{
		asprintf(&msg, "InitWinsock: Unable to initialize winsock 1.1 (%u)", err);
		PostMsgFree(msg);
		return 1;
	}
#else
//Configure linux/unix file paths to standard directories
	strcpy(config, "/etc/ipupdate.conf");
	asprintf(&cfg.options.logfile, "/var/log/ipupdate.log");
	asprintf(&cfg.options.pidfile, "/var/run/ipupdate.pid");
#endif

	// load the commandline options
	getoptions(argc, argv);

	if (RUNMODE == -1)
		return 1;

	//Load in the configuration file
	getconfig(config, &cfg);

	//If desired, stop the daemon
	if (RUNMODE == 2)
	{
		return stop();
	}
	else if (RUNMODE == 3)
	{
		stop();
		RUNMODE = 1;
	}
	else if (RUNMODE == 4)
	{
		printf("check: syntax ok\n");
		return 0;
	}

	if (cfg.serverc == 0)
	{
		PostMsg("getconfig: required 'server' statement");
		exit(10);
	}

	//pre allocate 512 bytes for the packet data
	if (!memmlen(&pkt, 512))
	{
		asprintf(&msg, "main: allocation error: %s", strerror(errno));
		PostMsgFree(msg);
		return 0;
	}

	if (naddr == INADDR_NONE)
	{
		if (cfg.options.getipc == 0)
		{
			PostMsg("getconfig: options: required 'getip' statement");
			exit(10);
		}

		// use configured getip tools to get the ip
		if (GetIP() == -1)
			return 2;
	}
	else
	{
		// it is counter intuitive to daemonize with a explicit ip
		RUNMODE = 0;
		asprintf(&msg, "Using IP %s", aaddr);
		PostMsgFree(msg);
	}

	if (cfg.options.checkcname)
	{
		// check for erroneous cname records then update if necessary
		if ((err = DoUpdate(2)) != 0)
			return err;
	}
	else
	{
		// update if necessary
		if ((err = DoUpdate(1)) != 0)
			return err;
	}

	// if desired, start the daemon
	if (RUNMODE == 1)
		start();

	return 0;
}

void getoptions(int argc, char **argv)
{
	int i, c;
	extern char *optarg;
	extern int optind, optopt;

	while ((c = getopt(argc, argv, ":c:l:p:i:vh")) != -1)
	{
		switch (c) {
		case 'c':
			strcpy(config, optarg);
			break;
		case 'i':
			if (strcmp(optarg, "-") == 0)
			{
				fgets(aaddr, 16, stdin);

				i = strlen(aaddr)-1;
				if (aaddr[i] == '\n');
					aaddr[i] = 0;

				optarg = aaddr;
			}
			else
			{
				strcpy(aaddr, optarg);
			}

			if ((naddr = inet_addr(optarg)) == INADDR_NONE)
			{
				asprintf(&msg, "ipupdate -i: Can not use invalid IP '%s'\n", aaddr);
				PostMsgFree(msg);
			}
			break;
		case 'h':
			printf("Usage: ipupdate [ -c config-file ] [ -i ip|- ] [ start|stop|restart|check ]\n");
			exit(0);
		case 'v':
			printf("ipupdate version %s\n", version);
			exit(0);
		case ':':
			asprintf(&msg, "Option -%c requires an argument\n", optopt);
			PostMsgFree(msg);
			break;
		case '?':
			asprintf(&msg, "Option -%c is unrecognized\n", optopt);
			PostMsgFree(msg);
		}
	}
	if (argc > optind)
	{
		if      (strcasecmp(argv[optind], "start") == 0)
		{
			RUNMODE = 1;
		}
		else if (strcasecmp(argv[optind], "stop") == 0)
		{
			RUNMODE = 2;
		}
		else if (strcasecmp(argv[optind], "restart") == 0)
		{
			RUNMODE = 3;
		}
		else if (strcasecmp(argv[optind], "check") == 0)
		{
			RUNMODE = 4;
		}
		else
		{
			RUNMODE = -1;
			asprintf(&msg, "ipupdate: unknown command '%s'", argv[optind]);
			PostMsgFree(msg);
		}
	}
	return;
}

/*
** DoUpdate
**
** Connect to the update server and loop through all zones updating them if necessary
**
** Returns application error code on failure
** Returns 0 on success
*/

int DoUpdate(int check)
{
	int i=0,j=0,s;
	struct server *server = NULL;
	struct zone   *zone = NULL;

	//Loop through each server entry
	while (i < cfg.serverc)
	{
		server = cfg.servers[i];

		if (server->protocol == IPPROTO_TCP)
		{
			s = opentcp(server->name, server->port, server->timeout);
		}
		else
		{
			s = openudp(server->name, server->port);
		}

		//Connect to the dns server on port 53 via TCP
		if (s == -1)
		{
			asprintf(&msg, "server %s: failed connect(): %s", server->name, strserr());
			PostMsgFree(msg);
			return 4;
		}

		//Loop through each zone entry
		while (j < server->zonec)
		{
			zone = server->zones[j];

			switch (check) {
			case 2:
				checkcname(&s, server, zone);
			case 1:
				if (checkip(&s, server, zone)) break;
			case 0:
				updateip(&s, server, zone);
			}

			// if the zone was disabled, or all hosts disabled
			if (zone->hostc == 0)
			{
				if (RUNMODE != 0)
				{
					asprintf(&msg, "server %s: zone %s: disabled: no more hosts", server->name, zone->name);
					PostMsgFree(msg);
				}

				// free up some memory
				free(zone->name);
				free(zone->hostp);
				free(zone->hosts);
				free(zone->keyname);
				free(server->zones[j]);

				server->zonec -= _splice((void**)server->zones, server->zonec, j, 1);
				continue;
			}

			j++;
		}
		//don't process more zones if the socket is closed
		if (s) closetcp(s); else break;

		if (server->zonec == 0)
		{
			if (RUNMODE != 0)
			{
				asprintf(&msg, "server %s: disabled: no more zones", server->name);
				PostMsgFree(msg);
			}

			// free up some memory
			free(server->name);
			free(server->zones);
			free(cfg.servers[i]);

			cfg.serverc -= _splice((void**)cfg.servers, cfg.serverc, i, 1);
			continue;
		}
		i++;
	}

	if ((cfg.serverc == 0) && (RUNMODE != 0))
	{
		PostMsg("shutting down: no more servers");
		exit(1);
	}

	return 0;
}

/*
** GetIP
**
**   Execute and read the output from each getip utility in order.
**   Make sure the read info is an IP address.
**
**   On failure returns 0
**   On success returns 1
*/

int GetIP(void)
{
	char buffer[256];
	int testaddr;
	int i;

	for (i=0; i<cfg.options.getipc; i++)
	{
		if (ipc_read(cfg.options.getip[i], buffer, 256) > 0)
		{
			buffer[strcspn(buffer, "\r\n")] = 0;

			if ((testaddr = inet_addr(buffer)) != INADDR_NONE)
			{
				if (testaddr != naddr)
				{
					naddr = testaddr;
					strcpy(aaddr, buffer);

					asprintf(&msg, "Detected IP: %s", aaddr);
					PostMsgFree(msg);
					return 1;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				asprintf(&msg, "GetIP: '%s' reports error", cfg.options.getip[i]);
				PostMsgFree(msg);
				asprintf(&msg, "GetIP: error: %s", buffer);
				PostMsgFree(msg);
			}
		}
		else
		{
			asprintf(&msg, "GetIP: failed ipc_read(%s)", cfg.options.getip[i]);
			PostMsgFree(msg);
			asprintf(&msg, "GetIP: error: %s", strerror(errno));
			PostMsgFree(msg);
		}
	}

	return -1;
}

int checkcname(int *s, struct server *server, struct zone *zone)
{
	int i=0, hlen, zlen, len;
	DNS_QUERY packet;
	char name[MAXNAMSIZ];

	zlen = strlen(zone->name);

	while (i < zone->hostc)
	{
		// set the header
		memcpy(pkt.data, "\0\0\0\1\0\0\0\1\0\0\0\0\0\0", 14);
		pkt.len = 14;

		hlen = strlen(zone->hosts[i]);

		// make sure the hostname.zonename isn't too long
		if (hlen + zlen > 253)
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: bad hostname", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: host disabled", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);

			zone->hostc -= _splice((void**)zone->hosts, zone->hostc, i, 1);
			continue;
		}

		if ((hlen > 0) && (zlen > 0))
		{
			// build the hostname.zonename
			memcpy(name, zone->hosts[i], hlen);
			name[hlen++] = '.';
			memcpy(&name[hlen], zone->name, zlen+1);
		}
		else if ((hlen == 0) && (zlen == 0))
		{
			name[0] = 0;
		}
		else if (hlen == 0)
		{
			memcpy(name, zone->name, zlen+1);
		}
		else
		{
			memcpy(name, zone->hosts[i], hlen+1);
		}

		//format the zone record in wire format
		if (!(len = dns_t2wrecord1(name, TYPE_CNAME, CLASS_IN, MEM1)))
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: bad hostname", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: host disabled", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
	
			zone->hostc -= _splice((void**)zone->hosts, zone->hostc, i, 1);
			continue;
		}
		//append the record to the packet
		memmcat(&pkt, MEM1, len);

		//write the packet length
		pkt.data[0] = (pkt.len-2) >> 8;
		pkt.data[1] = (pkt.len-2) % 256;

		//send the test packet to the server
		if (!SendPacket(s, server))
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: check failed", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
			return 0;
		}

		// parse the information from the response packet header
		if (dns_w2tquery(&pkt.data[2], &packet))
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: parse failed", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
		}
		else
		{
			// we won't actually need the allocated records
			dns_freequery(&packet);
		}

		if ((packet.header.flags & FLAGS_RCODE) != 0)
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: dns error: %s", server->name, zone->name, zone->hosts[i], dns_strerror(packet.header.flags & FLAGS_RCODE));
			PostMsgFree(msg);
		}
		else if (packet.header.ancount > 0)
		{
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: cname record", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
			asprintf(&msg, "checkcname: server %s: zone %s: host %s: host disabled", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);

			zone->hostc -= _splice((void**)zone->hosts, zone->hostc, i, 1);
			continue;
		}

		i++;
	}

	return 0;
}

/*
** checkip
**
**   Sends a dns update packet with no actual update records.
**   All of the records are prerequisites testing to see if the
**   hosts to be updated already point to the desired IP.
**
**   s       The opened socket descriptor for sending the packet
**   server  The server structure provides details required for
**           logging, and may be updated with a new fudge value
**   zone    The zone structure provides zone, host, and key info.
**
**   Returns boolean value indicating whether all hosts are pointed to the desired ip
**   Returns assumes true when there is an error
*/

int checkip(int *s, struct server *server, struct zone *zone)
{
	unsigned short rcode;
	DNS_TSIG tsig;
	time_t tmclient, tmserver;
	unsigned int offset;
	DNS_UPDATE packet;

	int i=0, len;

	//set the packet length to \0\0 for now
	//set the packet ID to \0\1
	//set the operation code to \x28 (UPDATE)
	//z flags set to \0
	//set the ZORECORD to \0\1
	memcpy(pkt.data, "\0\0\0\1\x28\0\0\1", 8);
	//set the PRRECORD to the number of records we'll be checking
	*((unsigned short*)&pkt.data[8]) = htons(zone->hostc);
	//set the UPRECORD to the number of create and delete records
	//set the ADRECORD to 0 (1 added later for the TSIG record)
	memset(&pkt.data[10], 0, 4);
	//the packet is now 14 bytes long
	pkt.len = 14;

	//format the zone record in wire format
	if (!(len = dns_t2wrecord1(zone->name, TYPE_SOA, CLASS_IN, MEM1)))
	{
		asprintf(&msg, "checkip: server %s: zone %s: bad zonename", server->name, zone->name);
		PostMsgFree(msg);

		zone->hostc = 0;
		return 1;
	}
	//append the record to the packet
	memmcat(&pkt, MEM1, len);

	//write the prerequisite records to the packet
	while (i < zone->hostc)
	{
		//format the host name in wire format
		if (!(len = dns_t2wname(zone->hosts[i], MEM1)))
		{
			asprintf(&msg, "checkip: server %s: zone %s: host %s: bad hostname", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);
			asprintf(&msg, "checkip: server %s: zone %s: host %s: host disabled", server->name, zone->name, zone->hosts[i]);
			PostMsgFree(msg);

			if (zone->hostc -= _splice((void**)zone->hosts, zone->hostc, i, 1))
			{
				*((unsigned short*)&pkt.data[8]) = htons(zone->hostc);
				continue;
			}
			return 1;
		}

		//replace null terminator with label pointer and add type (A), class (IN), ttl, and rdlen
		memcpy(&MEM1[len-1], "\xC0\x0C\0\1\0\1\0\0\0\0\0\4", 12);

		//add desired address
		memcpy(&MEM1[len+11], &naddr, 4);

		//append the record to the packet
		memmcat(&pkt, MEM1, len+15);

		i++;
	}

	// if TSIG authentication desired
	if (zone->keyname != NULL)
	{
		//generate the TSIG record in wire format
		if (!(len = dns_t2wrecord2(zone->keyname, TYPE_TSIG, CLASS_ANY, 0, 58, MEM1)))
		{
			asprintf(&msg, "checkip: server %s: zone %s: bad keyname", server->name, zone->name);
			PostMsgFree(msg);

			zone->hostc = 0;
			return 1;
		}
		//append the TSIG record to the packet
		memmcat(&pkt, MEM1, len);

		//generate the TSIG data
		if (!(len = dns_t2wtsig(&pkt.data[2], pkt.len-2, zone->keydata, time(NULL), server->fudge, MEM1)))
		{
			asprintf(&msg, "checkip: server %s: zone %s: %s\n", server->name, zone->name, strerror(errno));
			PostMsgFree(msg);
			return 1;
		}
		//append the TSIG data to the packet
		memmcat(&pkt, MEM1, len);

		//now set the raw packet's ADCOUNT
		pkt.data[13] = 1;
	}

	// make sure the packet hasn't gotten too big
	if (pkt.len > 65537)
	{
		asprintf(&msg, "checkip: server %s: zone %s: packet too big", server->name, zone->name);
		PostMsgFree(msg);

		zone->hostc = 0;
		return 1;
	}

	//write the packet length
	pkt.data[0] = (pkt.len-2) >> 8;
	pkt.data[1] = (pkt.len-2) % 256;

	//send the test packet to the server
	if (!SendPacket(s, server))
	{
		asprintf(&msg, "checkip: server %s: zone %s: check failed", server->name, zone->name);
		PostMsgFree(msg);
		return 1;
	}

	// parse the information from the response packet header
	if (dns_w2tupdate(&pkt.data[2], &packet))
	{
		asprintf(&msg, "checkip: server %s: zone %s: unable to parse update reply", server->name, zone->name);
		PostMsgFree(msg);
		return 1;
	}

	rcode = (packet.header.flags & FLAGS_RCODE);
	switch (rcode) {
	//prerequisite success, desired IP exists on the server for all hosts
	case RCODE_NOERROR:
		break;
	//prerequisite failure, one or more hosts don't point to the desired IP
	case RCODE_NXRRSET:
		dns_freeupdate(&packet);
		return 0;
	//authentication failure, log the specific TSIG error code
	case RCODE_NOTAUTH:
		dns_w2ttsig(&pkt.data[2], packet.adrecord[0].pdata, &tsig);

		asprintf(&msg, "checkip: server %s: dns error: Not Authenticated (%s)", server->name, dns_strerror(tsig.error));
		PostMsgFree(msg);

		switch (tsig.error) {
		//The TSIG keydata was wrong
		case RCODE_BADSIG:
			asprintf(&msg, "checkip: zone %s: keydata is rejected by server", zone->name);
			PostMsgFree(msg);
			zone->hostc = 0;
			break;
		//The TSIG keyname was wrong
		case RCODE_BADKEY:
			asprintf(&msg, "checkip: zone %s: keyname is rejected by server", zone->name);
			PostMsgFree(msg);
			zone->hostc = 0;
			break;
		//The TSIG packet was recieved too long after the signed time
		case RCODE_BADTIME:
			tmclient = tsig.lotime;
			tmserver = ntohl(*((time_t*)(&tsig.odata[2])));
			offset = abs(tmclient - tmserver);

			if (offset % 60 > 0)
				offset += 60 - (offset % 60);

			if (offset > 86400)
				strcpy(MEM1, "over a day");
			else if (offset > 3600)
				strcpy(MEM1, "over an hour");
			else
				sprintf(MEM1, "%u minute(s)", offset / 60);

			if (tmclient > tmserver)
				strcpy(&MEM1[20], "ahead");
			else
				strcpy(&MEM1[20], "behind");

			asprintf(&msg, "checkip: Clock is %s by about %s", &MEM1[20], MEM1);
			PostMsgFree(msg);

			if ((server->autofudge) && (server->maxfudge > server->fudge))
			{
				if (offset > server->maxfudge)
				{
					asprintf(&msg, "checkip: autofudge: fudge set from %u to maxfudge %u", server->fudge, server->maxfudge);
					server->fudge = server->maxfudge;
				}
				else
				{
					asprintf(&msg, "checkip: autofudge: fudge set from %u to %u", server->fudge, offset);
					server->fudge = offset;
				}
				PostMsgFree(msg);
			}
		}
		break;
	default:
		asprintf(&msg, "checkip: server %s: zone %s: dns error: %s", server->name, zone->name, dns_strerror(rcode));
		PostMsgFree(msg);
	}

	dns_freeupdate(&packet);
	return 1;
}

/*
** Build a DNS UPDATE packet based on zone and hosts
*/

int updateip(int *s, struct server *server, struct zone *zone)
{
	DNS_UPDATE packet;
	unsigned short rcode;
	DNS_TSIG tsig;
	time_t tmclient, tmserver;
	unsigned int offset;
	int i=0, len;

	//set the packet length to \0\0 for now
	//set the packet ID to \0\1
	//set the operation code to \x28 (UPDATE)
	//z flags set to \0
	//set the ZORECORD to \0\1
	//set the PRRECORD to \0\0
	memcpy(pkt.data, "\0\0\0\1\x28\0\0\1\0\0", 10);
	//set the UPRECORD to the number of records we'll be checking
	*((unsigned short*)&pkt.data[10]) = htons(zone->hostc*2);
	//set the ADRECORD to 0 (1 added later for the TSIG record)
	memset(&pkt.data[12], 0, 2);
	//the packet is now 14 bytes long
	pkt.len = 14;

	//format the zone record in wire format
	len = dns_t2wrecord1(zone->name, TYPE_SOA, CLASS_IN, MEM1);
	//append the record to the packet
	memmcat(&pkt, MEM1, len);

	//write the update records to the packet
	for (i=0; i<zone->hostc; i++)
	{
		//format the host name in wire format
		len = dns_t2wname(zone->hosts[i], MEM1);

		//replace null terminator with label pointer and add type (A), class (ANY), ttl, and rdlen
		memcpy(&MEM1[len-1], "\xC0\x0C\0\1\0\xFF\0\0\0\0\0\0", 12);

		//append the delete record to the packet
		memmcat(&pkt, MEM1, len+11);

		// change the class from ANY to IN
		MEM1[len+4] = CLASS_IN;
		// change the ttl from 0 to the new record's ttl
		*((int*)&MEM1[len+5]) = htonl(zone->ttl);
		// change the rdlen from 0 to 4
		MEM1[len+10] = 4;

		// add desired address
		memcpy(&MEM1[len+11], &naddr, 4);

		// check to see if we can do further dns label compression here
		if (pkt.len - (len+13) < 0x3FFF)
		{
			// point this hostname to the previous record's hostname
			*((unsigned short*)&MEM1[len-1]) = htons(0xC000 + (pkt.len - (len+13)));
			// append the create record to the packet
			memmcat(&pkt, &MEM1[len-1], 16);
		}
		else
		{
			// append the create record to the packet
			memmcat(&pkt, MEM1, len+15);
		}
	}

	// if TSIG authentication desired
	if (zone->keyname != NULL)
	{
		//generate the TSIG record in wire format
		len = dns_t2wrecord2(zone->keyname, TYPE_TSIG, CLASS_ANY, 0, 58, MEM1);
		//append the TSIG record to the packet
		memmcat(&pkt, MEM1, len);

		//generate the TSIG record data
		if (!(len = dns_t2wtsig(&pkt.data[2], pkt.len-2, zone->keydata, time(NULL), server->fudge, MEM1)))
		{
			asprintf(&msg, "server %s: zone %s: %s\n", server->name, zone->name, strerror(errno));
			PostMsgFree(msg);
			return 0;
		}
		//append the TSIG record to the packet
		memmcat(&pkt, MEM1, len);

		//now increment the raw packet's ADCOUNT
		pkt.data[13] += 1;
	}

	// make sure the packet hasn't gotten too big
	if (pkt.len > 65537)
	{
		asprintf(&msg, "server %s: zone %s: packet too big", server->name, zone->name);
		PostMsgFree(msg);

		zone->hostc = 0;
		return 0;
	}

	//write the packet length
	pkt.data[0] = (pkt.len-2) >> 8;
	pkt.data[1] = (pkt.len-2) % 256;

	//Send the dns update packet
	if (!SendPacket(s, server))
	{
		asprintf(&msg, "server %s: zone %s: update failed", server->name, zone->name);
		PostMsgFree(msg);
		return 0;
	}

	// parse the information from the response packet
	if (dns_w2tupdate(&pkt.data[2], &packet))
	{
		asprintf(&msg, "server %s: zone %s: unable to parse update reply", server->name, zone->name);
		PostMsgFree(msg);
		return 0;
	}

	rcode = (packet.header.flags & FLAGS_RCODE);

	// test the response code for an error
	switch (rcode) {
	case RCODE_NOERROR:
		asprintf(&msg, "server %s: zone %s: update successful", server->name, zone->name);
		PostMsgFree(msg);
		dns_freeupdate(&packet);
		return 1;
	// authentication failure, log the specific TSIG error code
	case RCODE_NOTAUTH:
		dns_w2ttsig(&pkt.data[2], packet.adrecord[0].pdata, &tsig);

		asprintf(&msg, "server %s: dns error: Not Authenticated (%s)", server->name, dns_strerror(tsig.error));
		PostMsgFree(msg);

		switch (tsig.error) {
		//The TSIG keydata was wrong
		case RCODE_BADSIG:
			asprintf(&msg, "zone %s: keydata is rejected by server", server->name);
			PostMsgFree(msg);
			zone->hostc = 0;
			break;
		//The TSIG keyname was wrong
		case RCODE_BADKEY:
			asprintf(&msg, "zone %s: keyname is rejected by server", server->name);
			PostMsgFree(msg);
			zone->hostc = 0;
			break;
		//The TSIG packet was recieved too long after the signed time
		case RCODE_BADTIME:
			tmclient = tsig.lotime;
			tmserver = ntohl(*((time_t*)(&tsig.odata[2])));
			offset = abs(tmclient - tmserver);

			if (offset % 60 > 0)
				offset += 60 - (offset % 60);

			if (offset > 86400)
				strcpy(MEM1, "over a day");
			else if (offset > 3600)
				strcpy(MEM1, "over an hour");
			else
				sprintf(MEM1, "%u minute(s)", offset / 60);

			if (tmclient > tmserver)
				strcpy(&MEM1[20], "ahead");
			else
				strcpy(&MEM1[20], "behind");

			asprintf(&msg, "badtime: Clock is %s by about %s", &MEM1[20], MEM1);
			PostMsgFree(msg);

			if ((server->autofudge) && (server->maxfudge > server->fudge))
			{
				if (offset > server->maxfudge)
				{
					asprintf(&msg, "autofudge: fudge set from %u to maxfudge %u", server->fudge, server->maxfudge);
					server->fudge = server->maxfudge;
				}
				else
				{
					asprintf(&msg, "autofudge: fudge set from %u to %u", server->fudge, offset);
					server->fudge = offset;
				}
				PostMsgFree(msg);
			}
		}
		break;
	default:
		asprintf(&msg, "server %s: zone %s: update failed (%s)", server->name, zone->name, dns_strerror(rcode));
		PostMsgFree(msg);
	}

	dns_freeupdate(&packet);
	return 0;
}

/*
** Send the update and recieve the response
*/

int SendPacket(int *s, struct server *server)
{
	int recved, attempts = 0, length;
	struct timeval tv;
	fd_set fds;
redo:
	attempts++;
	if (server->protocol == IPPROTO_TCP)
	{
		recved = send(*s, pkt.data, pkt.len, 0);
	}
	else
	{
		recved = send(*s, &pkt.data[2], pkt.len-2, 0);
	}

	// send the packet data
	if (recved == -1)
	{
		asprintf(&msg, "SendPacket: failed send(): %s", strserr());
		PostMsgFree(msg);
		return 0;
	}

	FD_ZERO(&fds);
	FD_SET(*s, &fds);

	tv.tv_sec = server->timeout;
	tv.tv_usec = 0;

	if (!select(FD_SETSIZE, &fds, NULL, NULL, &tv))
	{
		asprintf(&msg, "server %s: reply timed out", server->name);
		PostMsgFree(msg);

		closetcp(*s);

		if (server->protocol == IPPROTO_TCP)
		{
			if (attempts == 3)
				return *s = 0;

			if ((*s = opentcp(server->name, server->port, server->timeout)) == -1)
			{
				asprintf(&msg, "server %s: failed reconnect: %s", server->name, strserr());
				PostMsgFree(msg);
				return *s = 0;
			}
			goto redo;
		}
		else
		{
			return *s = 0;
		}
	}

	if (server->protocol != IPPROTO_TCP)
	{
		// recieve the response packet
		if ((recved = recv(*s, &pkt.data[2], 510, 0)) < 1)
		{
			if (recved == 0)
			{
				asprintf(&msg, "server %s: failed recv(): socket shutdown", server->name);
				PostMsgFree(msg);
			}
			else
			{
				asprintf(&msg, "server %s: failed recv(): %s", server->name, strserr());
				PostMsgFree(msg);
			}
			closetcp(*s);
			return *s = 0;
		}
		pkt.len = recved;
		return 1;
	}
	else
	{
		// recieve the response packet
		if ((recved = recv(*s, pkt.data, 2, 0)) < 1)
		{
			if (recved == 0)
			{
				asprintf(&msg, "server %s: failed recv(): Disconnected by peer", server->name);
				PostMsgFree(msg);
			}
			else
			{
				asprintf(&msg, "server %s: failed recv(): %s", server->name, strserr());
				PostMsgFree(msg);
			}
			closetcp(*s);
			if (attempts == 3)
				return *s = 0;

			if ((*s = opentcp(server->name, server->port, server->timeout)) == -1)
			{
				asprintf(&msg, "server %s: failed reconnect: %s", server->name, strserr());
				PostMsgFree(msg);
				return *s = 0;
			}
			goto redo;
		}
	}

	length = 2+ntohs(*((unsigned short*)pkt.data));
	memmlen(&pkt, length);
	pkt.len = 2;

	// keep reading until we've got the whole reply
	while (pkt.len < length)
	{
		// test for an error while recieving
		if ((recved = recv(*s, &pkt.data[pkt.len], length - pkt.len, 0)) < 1)
		{
			if (recved == 0)
			{
				asprintf(&msg, "server %s: failed recv(): Disconnected by peer", server->name);
				PostMsgFree(msg);
			}
			else
			{
				asprintf(&msg, "server %s: failed recv(): %s", server->name, strserr());
				PostMsgFree(msg);
			}
			closetcp(*s);
			if (attempts == 3)
				return *s = 0;

			if ((*s = opentcp(server->name, server->port, server->timeout)) == -1)
			{
				asprintf(&msg, "server %s: failed reconnect: %s", server->name, strserr());
				PostMsgFree(msg);
				return *s = 0;
			}
			goto redo;
		}
		pkt.len += recved;
	}

	return 1;
}

/*
** start
**
**   Daemonize, save pid to file, and loop every 5 minutes to check zones
**
*/

void start(void)
{
	int pid;
	PostMsg("start: starting service");

	if (daemonize() > 0)
	{
		PostMsg("start: failed daemonize()");
		exit(1);
	}

	STARTED = 1;

	//store the process ID
	pid = getpid();

	//Write the process id to ipupdate.pid
	if ((hpidFile = fopen(cfg.options.pidfile, "wb")) != NULL)
	{
		if (fwrite(&pid, 1, sizeof(pid), hpidFile) != sizeof(pid))
		{
			PostMsg("start: Unable to write to ipupdate.pid");
		}
		fclose(hpidFile);
	}
	else
	{
		PostMsg("start: Unable to open ipupdate.pid for writing");
	}

	//service loop
	while (STARTED)
	{
		//sleep for 5 minutes before rechecking the update
#ifdef WIN32
		Sleep(300000);
#else
		sleep(300);
#endif

		//get the IP to be used for updates
		switch (GetIP()) {
		case 0:
			// IP unchanged, check to see if an update is required if configured
			if (cfg.options.checkip) DoUpdate(1);
			break;
		case 1:
			// IP changed.  update each zone and maybe check the ip first
			DoUpdate(cfg.options.checkip);
			break;
		case -1:
			//no getip utilities managed to find the ip, temporary failure
			PostMsg("start: getip failed to retrieve an IP address");
		}
	}
}

int stop(void)
{
	int pid = 0;

	if ((hpidFile = fopen(cfg.options.pidfile, "rb")) != NULL)
	{
		if (fread(&pid, 1, sizeof(pid), hpidFile) < sizeof(pid))
		{
#ifdef WIN32
			asprintf(&msg, "stop: failed fread(<pidfile>): %s", strerror(GetLastError()));
#else
			asprintf(&msg, "stop: failed fread(<pidfile>): %s", strerror(errno));
#endif
			PostMsgFree(msg);
			exit(1);
		}
		fclose(hpidFile);
	}
	else
	{
		PostMsg("stop: ipupdate.pid not found");
		return 1;
	}

#ifdef WIN32
	HWND hProcess;

	if ((hProcess = OpenProcess(PROCESS_TERMINATE, 0, pid)) == NULL)
	{
		asprintf(&msg, "stop: failed OpenProcess(pid): %s", strerror(GetLastError()));
		PostMsgFree(msg);
		exit(1);
	}

	if (TerminateProcess(hProcess, 0))
	{
		PostMsg("stop: service stopped");
		remove(cfg.options.pidfile);
		return 0;
	}
	else
	{
		asprintf(&msg, "stop: failed TerminateProcess(pid): %s", strerror(GetLastError()));
		PostMsgFree(msg);
		exit(1);
	}
#else
	if ((kill(pid, 9) == 0) || (errno == ESRCH))
	{
		asprintf(&msg, "stop: service stopped (%u)", pid);
		PostMsgFree(msg);
		remove(cfg.options.pidfile);
		return 0;
	}
	else
	{
		asprintf(&msg, "stop: failed kill(%u): %s", pid, strerror(errno));
		PostMsgFree(msg);
		exit(1);
	}
#endif
}

void PostMsgFree(char* lpMessage)
{
	PostMsg(lpMessage);
	free(lpMessage);
}

void PostMsg(char* lpMessage)
{
	FILE* hlogFile;
 	time_t tm;

	if (!STARTED)
		printf("%s\n", lpMessage);

	if ((RUNMODE != 1) && (RUNMODE != 3)) return;
	if (!cfg.options.logenable) return;

	//format the local time (mm/dd/yyyy hh:mm:ss)
	tm = time(NULL);
	strftime(MEM1, 20, "%m/%d/%Y %H:%M:%S", localtime(&tm));

	if ((hlogFile = fopen(cfg.options.logfile, "ab")) != 0)
	{
#if WIN32
		sprintf(MEM2, "%s - %s\r\n", MEM1, lpMessage);
#else
		sprintf(MEM2, "%s - %s\n", MEM1, lpMessage);
#endif
		fwrite(MEM2, strlen(MEM2), 1, hlogFile);
		fclose(hlogFile);
	}
	else if (STARTED)
	{
		printf("PostMsg: Unable to open log file\n");
	}

	return;
}
