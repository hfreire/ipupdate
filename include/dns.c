#include "dns.h"

/*
** dns_strrcode
**
**   Return the static string mnemonic for the given response code
**
*/

char* dns_strerror(unsigned short rcode)
{
	static char* msgs[19] = {"NOERROR","FORMERR","SERVFAIL","NXDOMAIN","NOTIMP","REFUSED",
	                         "YXDOMAIN","YXRRSET","NXRRSET","NOTAUTH","NOTZONE",NULL,
	                         NULL,NULL,NULL,NULL,"BADSIG","BADKEY","BADTIME"};

	if ((rcode < 11) || ((rcode > 15) && (rcode < 19)))
		return msgs[rcode];
	else
		return NULL;
}

/*
** dns_t2wname
**
**   Convert dns name from text to wire format
**
*/

int dns_t2wname(char *text, char *wire)
{
	int r = 0, l;
	int sl = strlen(text);

	while ((l = strcspn(text, ".")))
	{
		if (l > 63)
			return 0;

		wire[0] = l;
		memcpy(&wire[1], text, l++);

		r    += l;
		wire += l;
		if (text[l-1] == 0) break;
		text += l;
	}

	// misplaced periods would cause this to fail
	if ((sl > 0) && (sl+2 != r+1)) return 0;

	wire[0] = 0;

	return r+1;
}

/*
** dns_t2wrecord1
**
**   Convert record type 1 from text to wire format
**
*/

int dns_t2wrecord1(char *name, unsigned short type, unsigned short class, char *wire)
{
	int r = dns_t2wname(name, wire);

	if (r == 0)
		return 0;

	wire[r++] = type >> 8;
	wire[r++] = type % 256;
	wire[r++] = class >> 8;
	wire[r++] = class % 256;

	return r;
}

/*
** dns_t2wrecord2
**
**   Convert record type 2 from text to wire format
**
*/

int dns_t2wrecord2(char *name, unsigned short type, unsigned short class, long ttl, unsigned short rdlen, char *wire)
{
	int r = dns_t2wname(name, wire);

	if (r == 0)
		return 0;

	wire[r++] = type >> 8;
	wire[r++] = type % 256;
	wire[r++] = class >> 8;
	wire[r++] = class % 256;
	wire[r++] = (ttl >> 24);
	wire[r++] = (ttl >> 16) % 256;
	wire[r++] = (ttl >>  8) % 256;
	wire[r++] = ttl % 256;
	wire[r++] = rdlen >> 8;
	wire[r++] = rdlen % 256;

	return r;
}

//For HMAC_MD5 TSIG only

int dns_t2wtsig(char *pktdata, unsigned short pktsize, char keyhash[16], int signtime, unsigned short fudge, char *wire)
{
	int r = 58;
	char *sigdata = (char*)malloc(pktsize + r);
	unsigned short sigsize;
	DNS_TSIG tsig;

	if (sigdata == NULL)
		return 0;

	//copy the raw packet to the sigdata up to the type field
	memcpy(sigdata, pktdata, pktsize-10);
	sigsize = pktsize-10;
	//grab the class and ttl fields
	memcpy(&sigdata[sigsize], &pktdata[pktsize-8], 6);
	sigsize += 6;

	//fill out the tsig record data fields
	dns_t2wname("hmac-md5.sig-alg.reg.int", tsig.algname);
	tsig.hitime = 0;
	tsig.lotime = htonl(signtime);
	tsig.fudge = htons(fudge);
	tsig.macsize = 0x1000;	//16
	memset(tsig.mac, 0, 16);
	memcpy(&tsig.id, pktdata, 2);
	tsig.error = 0;
	tsig.osize = 0;

	/*** Build signature message data ***/

	//append the tsig record data fields to the sigdata
	memcpy(&sigdata[sigsize], &tsig.algname, 26);
	sigsize += 26;
	memcpy(&sigdata[sigsize], &tsig.hitime, 8);
	sigsize += 8;
	memcpy(&sigdata[sigsize], &tsig.error, 4);
	sigsize += 4;

	//digest the sigdata against the key to get the mac
	hmac_md5(sigdata, sigsize, keyhash, 16, (char*)tsig.mac);

	//free allocated memory
	free(sigdata);

	//return the tsig record data fields
	memcpy(wire, &tsig.algname, 26);
	memcpy(&wire[26], &tsig.hitime, 32);

	return r;
}

int dns_w2theader(char* pktdata, DNS_HEADER *header)
{
	header->id = ntohs(*((unsigned short*)&pktdata[0]));
	header->flags = ntohs(*((unsigned short*)&pktdata[2]));
	header->qdcount = ntohs(*((unsigned short*)&pktdata[4]));
	header->ancount = ntohs(*((unsigned short*)&pktdata[6]));
	header->nscount = ntohs(*((unsigned short*)&pktdata[8]));
	header->arcount = ntohs(*((unsigned short*)&pktdata[10]));
	return 12;
}

int dns_w2theader5(char* pktdata, DNS_HEADER_5 *header)
{
	header->id = ntohs(*((unsigned short*)&pktdata[0]));
	header->flags = ntohs(*((unsigned short*)&pktdata[2]));
	header->zocount = ntohs(*((unsigned short*)&pktdata[4]));
	header->prcount = ntohs(*((unsigned short*)&pktdata[6]));
	header->upcount = ntohs(*((unsigned short*)&pktdata[8]));
	header->adcount = ntohs(*((unsigned short*)&pktdata[10]));
	return 12;
}

int dns_w2tname(char *pktdata, int p, char *name)
{
	int np = p;
	int nl = 0;

	while (pktdata[np])
	{
		if ((unsigned char)pktdata[np] < 64)
		{
			memcpy(&name[nl], &pktdata[np+1], (unsigned char)pktdata[np]);
			nl += (unsigned char)pktdata[np];
			name[nl++] = '.';

			if (np == p)
			{
				np += 1 + (unsigned char)pktdata[np];
				p = np;
			}
			else
			{
				np += 1 + (unsigned char)pktdata[np];
			}
		}
		else
		{
			if (np == p) p++;
			np = ntohs(*((unsigned short*)&pktdata[np])) - 0xC000;
		}
	}
	if (nl) name[nl-1] = 0; else name[0] = 0;

	return p+1;
}

int dns_w2trecord1(char *pktdata, int p, DNS_RECORD_1 *record)
{
	p = dns_w2tname(pktdata, p, record->name);
	record->type  = ntohs(*((unsigned short*)&pktdata[p]));
	record->class = ntohs(*((unsigned short*)&pktdata[p+2]));

	return p+4;
}

int dns_w2trecord2(char *pktdata, int p, DNS_RECORD_2 *record)
{
	p = dns_w2tname(pktdata, p, record->name);
	record->type  = ntohs(*((unsigned short*)&pktdata[p]));
	record->class = ntohs(*((unsigned short*)&pktdata[p+2]));
	record->ttl   = ntohl(*((long*)&pktdata[p+4]));
	record->rdlen = ntohs(*((unsigned short*)&pktdata[p+8]));
	record->pdata = p+10;

	return p+10+record->rdlen;
}

int dns_w2tquery(char *pktdata, DNS_QUERY *packet)
{
	int p = 0, i = 0;

	if (packet == NULL)
		return 1;

	p = dns_w2theader(pktdata, &packet->header);

	if (packet->header.qdcount == 1)
	{
		if ((packet->qdrecord = malloc(sizeof(DNS_RECORD_1))) == NULL)
			return 1;

		p = dns_w2trecord1(pktdata, p, packet->qdrecord);
	}
	else
	{
		packet->qdrecord = NULL;
	}

	if (packet->header.ancount > 0)
	{
		if ((packet->anrecord = malloc(packet->header.ancount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->qdrecord);
			return 1;
		}

		for (i=0; i<packet->header.ancount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->anrecord[i]);
		}
	}
	else
	{
		packet->anrecord = NULL;
	}

	if (packet->header.nscount > 0)
	{
		if ((packet->nsrecord = malloc(packet->header.nscount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->qdrecord);
			free(packet->anrecord);
			return 1;
		}

		for (i=0; i<packet->header.nscount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->nsrecord[i]);
		}
	}
	else
	{
		packet->nsrecord = NULL;
	}

	if (packet->header.arcount > 0)
	{
		if ((packet->arrecord = malloc(packet->header.arcount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->qdrecord);
			free(packet->anrecord);
			free(packet->nsrecord);
			return 1;
		}

		for (i=0; i<packet->header.arcount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->arrecord[i]);
		}
	}
	else
	{
		packet->arrecord = NULL;
	}

	return 0;
}

int dns_w2tupdate(char *pktdata, DNS_UPDATE *packet)
{
	int p = 0, i = 0;

	if (packet == NULL)
		return 1;

	p = dns_w2theader5(pktdata, &packet->header);

	if (packet->header.zocount == 1)
	{
		if ((packet->zorecord = malloc(sizeof(DNS_RECORD_1))) == NULL)
			return 1;

		p = dns_w2trecord1(pktdata, p, packet->zorecord);
	}
	else
	{
		packet->zorecord = NULL;
	}

	if (packet->header.prcount > 0)
	{
		if ((packet->prrecord = malloc(packet->header.prcount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->zorecord);
			return 1;
		}

		for (i=0; i<packet->header.prcount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->prrecord[i]);
		}
	}
	else
	{
		packet->prrecord = NULL;
	}

	if (packet->header.upcount > 0)
	{
		if ((packet->uprecord = malloc(packet->header.upcount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->zorecord);
			free(packet->prrecord);
			return 1;
		}

		for (i=0; i<packet->header.upcount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->uprecord[i]);
		}
	}
	else
	{
		packet->uprecord = NULL;
	}

	if (packet->header.adcount > 0)
	{
		if ((packet->adrecord = malloc(packet->header.adcount * sizeof(DNS_RECORD_2))) == NULL)
		{
			free(packet->zorecord);
			free(packet->prrecord);
			free(packet->uprecord);
			return 1;
		}

		for (i=0; i<packet->header.adcount; i++)
		{
			p = dns_w2trecord2(pktdata, p, &packet->adrecord[i]);
		}
	}
	else
	{
		packet->adrecord = NULL;
	}

	return 0;
}

int dns_w2ttsig(char *pktdata, int p, DNS_TSIG *tsig)
{
	p = dns_w2tname(pktdata, p, tsig->algname);
	tsig->hitime  = ntohs(*((unsigned short*)&pktdata[p]));
	tsig->lotime  = ntohl(*((unsigned long *)&pktdata[p+2]));
	tsig->fudge   = ntohs(*((unsigned short*)&pktdata[p+6]));
	tsig->macsize = ntohs(*((unsigned short*)&pktdata[p+8]));
	if (tsig->macsize > 0)
	{
		memcpy(tsig->mac, &pktdata[p+10], tsig->macsize);
		p += tsig->macsize;
	}
	else
	{
		memset(tsig->mac, 0, sizeof(tsig->mac));
	}
	tsig->id    = ntohs(*((unsigned short*)&pktdata[p+10]));
	tsig->error = ntohs(*((unsigned short*)&pktdata[p+12]));
	tsig->osize = ntohs(*((unsigned short*)&pktdata[p+14]));
	if (tsig->osize > 0)
	{
		memcpy(tsig->odata, &pktdata[p+16], tsig->osize);
	}
	else
	{
		memset(tsig->odata, 0, sizeof(tsig->odata));
	}

	return 1;
}

void dns_freequery(DNS_QUERY *packet)
{
	if (packet == NULL) return;
	free(packet->qdrecord);
	free(packet->anrecord);
	free(packet->nsrecord);
	free(packet->arrecord);
}

void dns_freeupdate(DNS_UPDATE *packet)
{
	if (packet == NULL) return;
	free(packet->zorecord);
	free(packet->prrecord);
	free(packet->uprecord);
	free(packet->adrecord);
}
