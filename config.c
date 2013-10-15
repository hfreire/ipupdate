#include "config.h"

// global variables that are used in getconfig() and cfg_zone()
struct key** keys = NULL;
int keyc = 0;

// local variable for the configuration file handle
FILE* hconfig;
extern int RUNMODE;

/*
** getconfig
**
**   Load the configuration file info into the 'config' structure for easy access
**   The first argument for the full config file path
**   The second argument is the pionter to the config structure to be filled
*/

void getconfig(char *cfgfile, struct config *cfg)
{
	struct mem cfgdata = {NULL,0,0};
	char *term = NULL;
	int type;
	char *msg = NULL;
	unsigned int ptr;
	struct key* key;
	struct server* server;
	int i;

	cfg->options.getipc = 0;
	cfg->options.logenable = 0;
	cfg->servers = NULL;
	cfg->serverc = 0;

	if (RUNMODE != 2)
		printf("getconfig: loading '%s'\n", cfgfile);

	if ((hconfig = fopen(cfgfile, "rb")) == NULL)
	{
		fprintf(stderr, "getconfig: fopen: %s\n", strerror(errno));
		exit(10);
	}

	cfg_fread(&cfgdata);

	ptr = 0;

	while ((type = cfg_read(&cfgdata, &term, &ptr)))
	{
		switch (type) {
		case CFG_KEYWORD:
			if (strcasecmp(term, "options") == 0)
			{
				free(term);

				switch (cfg_read(&cfgdata, &term, &ptr)) {
				case CFG_BLOCK:
					cfg_options(&cfg->options, term);
					if (cfg->options.getipc == 0)
					{
						PostMsg("getconfig: options: required 'getip' statement");
						exit(10);
					}
					free(term);

					if (RUNMODE == 4)
					{
						printf("check: options: using %u getip clients\n", cfg->options.getipc);
						printf("check: options: pidfile '%s'\n", cfg->options.pidfile);
						printf("check: options: logfile '%s'\n", cfg->options.logfile);
						printf("check: options: logenable '%u'\n", cfg->options.logenable);
						printf("check: options: checkip '%u'\n", cfg->options.checkip);
						printf("check: options: checkcname '%u'\n", cfg->options.checkcname);
					}
					break;
				case CFG_ERROR:
					PostMsg("getconfig: parse error after 'options'");
					asprintf(&msg, "getconfig: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: expected block after 'options'");
					exit(10);
				}

				// discard some data to help conserve memory
				mm_unshift(&cfgdata, ptr);
				ptr = 0;
			}
			else if (strcasecmp(term, "key") == 0)
			{
				free(term);

				switch (cfg_read(&cfgdata, &term, &ptr)) {
				case CFG_QUOTED:
					if ((keyc % 5) == 0)
					{
						keys = realloc(keys, sizeof(struct key*) * (keyc + 5));
					}
					if ((keys[keyc] = malloc(sizeof(struct key))) == NULL)
					{
						asprintf(&msg, "getconfig: malloc(): %s\n", strerror(errno));
						PostMsg(msg);
						exit(10);
					}
					key = keys[keyc];
					key->label = term;

					switch (cfg_read(&cfgdata, &term, &ptr)) {
					case CFG_BLOCK:
						cfg_key(key, term);
						if (key->name == NULL)
						{
							asprintf(&msg, "getconfig: key \"%s\": required 'keyname' statement", key->label);
							PostMsg(msg);
							exit(10);
						}
						keyc++;
						free(term);
						break;
					case CFG_ERROR:
						asprintf(&msg, "getconfig: parse error after key \"%s\"", key->label);
						PostMsg(msg);
						asprintf(&msg, "getconfig: %s", term);
						PostMsg(msg);
						exit(10);
					default:
						asprintf(&msg, "getconfig: expected block after key \"%s\"", key->label);
						PostMsg(msg);
						exit(10);
					}
					break;
				case CFG_ERROR:
					PostMsg("getconfig: parse error after 'key'");
					asprintf(&msg, "getconfig: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: expected quoted argument after 'key'");
					exit(10);
				}

				// release some data to help conserve memory
				mm_unshift(&cfgdata, ptr);
				ptr = 0;
			}
			else if (strcasecmp(term, "server") == 0)
			{
				free(term);

				switch (cfg_read(&cfgdata, &term, &ptr)) {
				case CFG_QUOTED:
					if ((cfg->serverc % 5) == 0)
					{
						cfg->servers = realloc(cfg->servers, sizeof(struct server*) * (cfg->serverc + 5));
					}
					if ((cfg->servers[cfg->serverc] = malloc(sizeof(struct server))) == NULL)
					{
						asprintf(&msg, "getconfig: malloc(): %s\n", strerror(errno));
						PostMsg(msg);
						exit(10);
					}
					server = cfg->servers[cfg->serverc];
					server->name = term;
					server->port = 53;
next:
					switch (cfg_read(&cfgdata, &term, &ptr)) {
					case CFG_KEYWORD:
					case CFG_QUOTED:
						for (i=0; term[i]; i++)
						{
							if (!isdigit(term[i]))
							{
								asprintf(&msg, "getconfig: server \"%s\": port must be a number", server->name);
								PostMsg(msg);
								exit(10);
							}
						}
						server->port = atoi(term);
						if (server->port == 0)
						{
							asprintf(&msg, "getconfig: server \"%s\": port may not be 0", server->name);
							PostMsg(msg);
							exit(10);
						}
						free(term);
						// minor bug, more than one 'port' may be specified
						goto next;
					case CFG_BLOCK:
						if (RUNMODE == 4)
						{
							printf("check: loading server '%s' block\n", server->name);
						}

						cfg_server(server, term);
						if (server->zonec == 0)
						{
							asprintf(&msg, "getconfig: server \"%s\": required 'zone' statement", server->name);
							PostMsg(msg);
							exit(10);
						}
						if (server->maxfudge < server->fudge)
						{
							asprintf(&msg, "getconfig: server \"%s\": setting fudge to maxfudge", server->name);
							server->fudge = server->maxfudge;
						}
						cfg->serverc++;
						free(term);

						if (RUNMODE == 4)
						{
							printf("check: server %s: port '%u'\n", server->name, server->port);
							printf("check: server %s: protocol '%s'\n", server->name, (server->protocol == IPPROTO_TCP ? "tcp" : "udp"));
							printf("check: server %s: timeout '%li'\n", server->name, server->timeout);
							printf("check: server %s: fudge '%u'\n", server->name, server->fudge);
							printf("check: server %s: maxfudge '%u'\n", server->name, server->maxfudge);
							printf("check: server %s: autofudge '%u'\n", server->name, server->autofudge);
							printf("check: server %s: ttl '%u'\n", server->name, server->ttl);
						}
						break;
					case CFG_ERROR:
						asprintf(&msg, "getconfig: parse error after server \"%s\"", server->name);
						PostMsg(msg);
						asprintf(&msg, "getconfig: %s", term);
						PostMsg(msg);
						exit(10);
					default:
						asprintf(&msg, "getconfig: expected port or block after server \"%s\"", server->name);
						PostMsg(msg);
						exit(10);
					}
					break;
				case CFG_ERROR:
					PostMsg("getconfig: parse error after 'server'");
					asprintf(&msg, "getconfig: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: expected quoted argument after 'server'");
					exit(10);
				}

				// discard some data to help conserve memory
				mm_unshift(&cfgdata, ptr);
				ptr = 0;
			}
			else
			{
				asprintf(&msg, "getconfig: unrecognized keyword '%s'", term);
				PostMsg(msg);
				exit(10);
			}
			break;
		case CFG_ERROR:
			PostMsg("getconfig: parse error");
			asprintf(&msg, "getconfig: %s", term);
			PostMsg(msg);
			exit(10);
		default:
			PostMsg("getconfig: expected keyword");
			exit(10);
		}
	}

	memmfree(&cfgdata);

	fclose(hconfig);
}

void cfg_options(struct options *options, char *data)
{
	char *term = NULL;
	unsigned int ptr = 0, type;
	char *msg;

	options->logenable = 1;
	options->checkip = 0;
	options->checkcname = 0;

	while ((type = cfg_readblock(data, &term, &ptr)))
	{
		switch (type) {
		case CFG_KEYWORD:
			if (strcasecmp(term, "getip") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (options->getipc < 5)
					{
						options->getip[options->getipc++] = term;
					}
					else
					{
						PostMsg("getconfig: options: 'getip' may not occur more than 5 times");
						exit(10);
					}
					break;
				case CFG_ERROR:
					PostMsg("getconfig: options: parse error after 'getip'");
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: options: expected quoted argument after 'getip'");
					exit(10);
				}
			}
			else if (strcasecmp(term, "logenable") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					if (((term[0] != '0') && (term[0] != '1')) || (term[1] != 0))
					{
						asprintf(&msg, "getconfig: options: logenable must be 0 or 1");
						PostMsg(msg);
						exit(10);
					}
					options->logenable = term[0] - 48;
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: options: parse error after 'logenable'");
					PostMsg(msg);
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: options: expected number after 'logenable'");
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "logfile") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (options->logfile) free(options->logfile);
					options->logfile = term;
					break;
				case CFG_ERROR:
					PostMsg("getconfig: options: parse error after 'logfile'");
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: options: expected quoted argument after 'logfile'");
					exit(10);
				}
			}
			else if (strcasecmp(term, "pidfile") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (options->pidfile) free(options->pidfile);
					options->pidfile = term;
					break;
				case CFG_ERROR:
					PostMsg("getconfig: options: parse error after 'pidfile'");
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					PostMsg("getconfig: options: expected quoted argument after 'pidfile'");
					exit(10);
				}
			}
			else if (strcasecmp(term, "checkip") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					if (((term[0] != '0') && (term[0] != '1')) || (term[1] != 0))
					{
						asprintf(&msg, "getconfig: options: checkip must be 0 or 1");
						PostMsg(msg);
						exit(10);
					}
					options->checkip = term[0] - 48;
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: options: parse error after 'checkip'");
					PostMsg(msg);
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: options: expected number after 'checkip'");
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "checkcname") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					if (((term[0] != '0') && (term[0] != '1')) || (term[1] != 0))
					{
						asprintf(&msg, "getconfig: options: checkcname must be 0 or 1");
						PostMsg(msg);
						exit(10);
					}
					options->checkcname = term[0] - 48;
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: options: parse error after 'checkcname'");
					PostMsg(msg);
					asprintf(&msg, "getconfig: options: %s", term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: options: expected number after 'checkcname'");
					PostMsg(msg);
					exit(10);
				}
			}
			else
			{
				asprintf(&msg, "getconfig: options: unrecognized keyword '%s'", term);
				PostMsg(msg);
				exit(10);
			}
			break;
		case CFG_ERROR:
			PostMsg("getconfig: options: parse error");
			asprintf(&msg, "getconfig: options: %s", term);
			PostMsg(msg);
			exit(10);
		default:
			PostMsg("getconfig: options: expected keyword");
			exit(10);
		}
	}
}

void cfg_key(struct key *key, char *data)
{
	char* term = NULL;
	unsigned int ptr = 0, type;
	char* msg;

	key->name = NULL;

	while ((type = cfg_readblock(data, &term, &ptr)))
	{
		switch (type) {
		case CFG_KEYWORD:
			if (strcasecmp(term, "keyname") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (key->name == NULL)
					{
						key->name = term;
					}
					else
					{
						asprintf(&msg, "getconfig: key \"%s\": duplicate references to 'keyname'", key->label);
						PostMsg(msg);
						exit(10);
					}
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: key \"%s\": parse error after 'keyname'", key->label);
					PostMsg(msg);
					asprintf(&msg, "getconfig: key \"%s\": %s", key->label, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: key \"%s\": expected quoted argument after 'keyname'", key->label);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "keydata") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if ((strlen(term) != 24) || (term[22] != '=') || (term[23] != '=') ||
					    (!base64_decode(term, key->data)))
					{
						asprintf(&msg, "getconfig: key \"%s\": 'keydata' is not a 128bit base64 encoded key", key->label);
						PostMsg(msg);
						exit(10);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: key \"%s\": parse error after 'keydata'", key->label);
					PostMsg(msg);
					asprintf(&msg, "getconfig: key \"%s\": %s", key->label, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: key \"%s\": expected quoted argument after 'keydata'", key->label);
					PostMsg(msg);
					exit(10);
				}
			}
			else
			{
				asprintf(&msg, "getconfig: key \"%s\": unrecognized keyword '%s'", key->label, term);
				PostMsg(msg);
				exit(10);
			}
			break;
		case CFG_ERROR:
			asprintf(&msg, "getconfig: key \"%s\": parse error", key->label);
			PostMsg(msg);
			asprintf(&msg, "getconfig: key \"%s\": %s", key->label, term);
			PostMsg(msg);
			exit(10);
		default:
			asprintf(&msg, "getconfig: key \"%s\": expected keyword", key->label);
			PostMsg(msg);
			exit(10);
		}
	}
}

void cfg_server(struct server* server, char *data)
{
	char* term = NULL;
	unsigned int ptr = 0, type;
	char* msg;
	struct zone* zone;
	int i;

	server->zones = NULL;
	server->zonec = 0;
	server->fudge = 900;
	server->autofudge = 1;
	server->maxfudge = 3600;
	server->protocol = IPPROTO_TCP;
	server->timeout = 30;
	server->ttl = 300;

	while ((type = cfg_readblock(data, &term, &ptr)))
	{
		switch (type) {
		case CFG_KEYWORD:
			if (strcasecmp(term, "zone") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (strlen(term) > 254)
					{
						asprintf(&msg, "getconfig: zone \"%s\" must not be longer than 254 characters", term);
						PostMsg(msg);
						exit(10);
					}
					if ((server->zonec % 5) == 0)
					{
						server->zones = realloc(server->zones, sizeof(struct zone*) * (server->zonec + 5));
					}
					if ((server->zones[server->zonec] = malloc(sizeof(struct zone))) == NULL)
					{
						asprintf(&msg, "getconfig: malloc(): %s\n", strerror(errno));
						PostMsg(msg);
						exit(10);
					}
					zone = server->zones[server->zonec];
					zone->name = term;

					switch (cfg_readblock(data, &term, &ptr)) {
					case CFG_BLOCK:
						if (RUNMODE == 4)
						{
							printf("check: loading zone '%s' block\n", zone->name);
						}

						cfg_zone(server, zone, term);
						if (zone->hostc == 0)
						{
							asprintf(&msg, "getconfig: zone \"%s\": required 'hosts' statement", zone->name);
							PostMsg(msg);
							exit(10);
						}
						server->zonec++;
						free(term);

						if (RUNMODE == 4)
						{
							printf("check: zone %s: loaded %u hosts\n", zone->name, zone->hostc);
							printf("check: zone %s: ttl '%u'\n", zone->name, zone->ttl);
							if (zone->keyname == NULL)
							{
								printf("check: zone %s: unsecured update\n", zone->name);
							}
							else
							{
								printf("check: zone %s: TSIG secured update with key '%s'\n", zone->name, zone->keyname);
							}
						}
						break;
					case CFG_ERROR:
						asprintf(&msg, "getconfig: server \"%s\": parse error after zone \"%s\"", server->name, zone->name);
						PostMsg(msg);
						asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
						PostMsg(msg);
						exit(10);
					default:
						asprintf(&msg, "getconfig: server \"%s\": expected block after zone \"%s\"", server->name, zone->name);
						PostMsg(msg);
						exit(10);
					}
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'zone'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected quoted argument after 'zone'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "fudge") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					for (i=0; term[i]; i++)
					{
						if (!(isdigit(term[i])))
						{
							asprintf(&msg, "getconfig: server \"%s\": fudge must be a number", server->name);
							PostMsg(msg);
							exit(10);
						}
					}
					server->fudge = (unsigned short)atoi(term);
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'fudge'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected number after 'fudge'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "autofudge") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					if (((term[0] != '0') && (term[0] != '1')) || (term[1] != 0))
					{
						asprintf(&msg, "getconfig: server \"%s\": autofudge must be 0 or 1", server->name);
						PostMsg(msg);
						exit(10);
					}
					server->autofudge = term[0] - 48;
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'autofudge'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected number after 'autofudge'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "maxfudge") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					for (i=1; term[i]; i++)
					{
						if (!isdigit(term[i]))
						{
							asprintf(&msg, "getconfig: server \"%s\": maxfudge must be a number", server->name);
							PostMsg(msg);
							exit(10);
						}
					}
					server->maxfudge = atoi(term);
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'maxfudge'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected number after 'maxfudge'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "protocol") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if      (strcasecmp(term, "tcp") == 0)
					{
						server->protocol = IPPROTO_TCP;
					}
					else if (strcasecmp(term, "udp") == 0)
					{
						server->protocol = IPPROTO_UDP;
					}
					else
					{
						asprintf(&msg, "getconfig: server \"%s\": ignored unknown protocol", server->name);
						PostMsgFree(msg);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'protocol'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected quoted argument after 'protocol'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "timeout") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					for (i=0; term[i]; i++)
					{
						if (!isdigit(term[i]))
						{
							asprintf(&msg, "getconfig: server \"%s\": timeout must be a number", server->name);
							PostMsg(msg);
							exit(10);
						}
					}
					server->timeout = atoi(term);
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'ttl'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected number after 'ttl'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "ttl") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					for (i=0; term[i]; i++)
					{
						if (!isdigit(term[i]))
						{
							asprintf(&msg, "getconfig: server \"%s\": ttl must be a number", server->name);
							PostMsg(msg);
							exit(10);
						}
					}
					server->ttl = atoi(term);
					if (server->ttl < 60)
					{
						asprintf(&msg, "getconfig: server \"%s\": ttl must not be less than 60 seconds", server->name);
						PostMsg(msg);
						exit(10);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: server \"%s\": parse error after 'ttl'", server->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: server \"%s\": expected number after 'ttl'", server->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else
			{
				asprintf(&msg, "getconfig: server \"%s\": unrecognized keyword '%s'", server->name, term);
				PostMsg(msg);
				exit(10);
			}
			break;
		case CFG_ERROR:
			asprintf(&msg, "getconfig: server \"%s\": parse error", server->name);
			PostMsg(msg);
			asprintf(&msg, "getconfig: server \"%s\": %s", server->name, term);
			PostMsg(msg);
			exit(10);
		default:
			asprintf(&msg, "getconfig: server \"%s\": expected keyword", server->name);
			PostMsg(msg);
			exit(10);
		}
	}
}

void cfg_zone(struct server *server, struct zone *zone, char *data)
{
	char* term = NULL;
	unsigned int ptr = 0, type;
	char* msg;
	int i, k;

	zone->hosts = NULL;
	zone->hostc = 0;
	zone->keyname = NULL;
	zone->ttl = server->ttl;

	while ((type = cfg_readblock(data, &term, &ptr)))
	{
		switch (type) {
		case CFG_KEYWORD:
			if (strcasecmp(term, "hosts") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					zone->hostp = term;
					zone->hosts = split(zone->hostp, ", \t\r\n", &zone->hostc);
					if (zone->hostc > 32767)
					{
							asprintf(&msg, "getconfig: zone \"%s\": there may not be more than 32767 hosts", zone->name);
							PostMsg(msg);
							exit(10);
					}
					for (i=0; i<zone->hostc; i++)
					{
						if (strlen(zone->hosts[i]) > 254)
						{
							asprintf(&msg, "getconfig: zone \"%s\": hosts may not be more than 254 characters", zone->name);
							PostMsg(msg);
							exit(10);
						}
						else if ((zone->hosts[i][0] == '@') && (zone->hosts[i][1] == 0))
						{
							zone->hosts[i][0] = 0;
						}
					}
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: zone \"%s\": parse error after 'hosts'", zone->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: zone \"%s\": expected quoted argument after 'hosts'", zone->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "key") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (zone->keyname)
					{
						asprintf(&msg, "getconfig: zone \"%s\": key already set by previous statement", zone->name);
						PostMsg(msg);
						exit(10);
					}
					for (k=0; k<keyc; k++)
					{
						if (strcasecmp(term, keys[k]->label) == 0)
						{
							zone->keyname = keys[k]->name;
							memcpy(zone->keydata, keys[k]->data, 16);
						}
					}
					if (zone->keyname == NULL)
					{
						asprintf(&msg, "getconfig: zone \"%s\": key \"%s\" not previously defined", zone->name, term);
						PostMsg(msg);
						exit(10);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: zone \"%s\": parse error after 'key'", zone->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: zone \"%s\": expected quoted argument after 'key'", zone->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "keyname") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if (zone->keyname)
					{
						asprintf(&msg, "getconfig: zone \"%s\": key already set by previous statement", zone->name);
						PostMsg(msg);
						exit(10);
					}
					zone->keyname = term;
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: zone \"%s\": parse error after 'keyname'", zone->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: zone \"%s\": expected quoted argument after 'keyname'", zone->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "keydata") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_QUOTED:
					if ((strlen(term) != 24) || (term[22] != '=') || (term[23] != '=') ||
					    (!base64_decode(term, zone->keydata)))
					{
						asprintf(&msg, "getconfig: zone \"%s\": 'keydata' is not a 128bit base64 encoded key", zone->name);
						PostMsg(msg);
						exit(10);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: zone \"%s\": parse error after 'keyname'", zone->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: zone \"%s\": expected quoted argument after 'keyname'", zone->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else if (strcasecmp(term, "ttl") == 0)
			{
				free(term);

				switch (cfg_readblock(data, &term, &ptr)) {
				case CFG_KEYWORD:
				case CFG_QUOTED:
					for (i=0; term[i]; i++)
					{
						if (!isdigit(term[i]))
						{
							asprintf(&msg, "getconfig: zone \"%s\": ttl must be a number", zone->name);
							PostMsg(msg);
							exit(10);
						}
					}
					zone->ttl = atoi(term);
					if (zone->ttl < 60)
					{
						asprintf(&msg, "getconfig: zone \"%s\": ttl must not be less than 60 seconds", zone->name);
						PostMsg(msg);
						exit(10);
					}
					free(term);
					break;
				case CFG_ERROR:
					asprintf(&msg, "getconfig: zone \"%s\": parse error after 'ttl'", zone->name);
					PostMsg(msg);
					asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
					PostMsg(msg);
					exit(10);
				default:
					asprintf(&msg, "getconfig: zone \"%s\": expected number after 'ttl'", zone->name);
					PostMsg(msg);
					exit(10);
				}
			}
			else
			{
				asprintf(&msg, "getconfig: zone \"%s\": unrecognized keyword '%s'", zone->name, term);
				PostMsg(msg);
				exit(10);
			}
			break;
		case CFG_ERROR:
			asprintf(&msg, "getconfig: zone \"%s\": parse error", zone->name);
			PostMsg(msg);
			asprintf(&msg, "getconfig: zone \"%s\": %s", zone->name, term);
			PostMsg(msg);
			exit(10);
		default:
			asprintf(&msg, "getconfig: zone \"%s\": expected keyword", zone->name);
			PostMsg(msg);
			exit(10);
		}
	}
}

int cfg_fread(struct mem *cfgdata)
{
	char buffer[4096];
	int buflen;
	char *msg = NULL;

	if ((buflen = fread(buffer, 1, 4095, hconfig)) == 0)
	{
		if (feof(hconfig))
		{
			return 0;
		}
		else
		{
			asprintf(&msg, "getconfig: fread: %s", strerror(errno));
			PostMsg(msg);
			exit(10);
		}
	}
	buffer[buflen] = 0;

	if (cfgdata->len == 0)
	{
		memmcpy(cfgdata, buffer, buflen+1);
	}
	else
	{
		cfgdata->len--;
		memmcat(cfgdata, buffer, buflen+1);
	}

	return 1;
}

int cfg_read(struct mem *input, char **output, unsigned int *ptr)
{
	int i,j,k=0,l;

	if (input == NULL)
		return 0;

	if (input->len < 1)
		return 0;

	*output = NULL;

	for (i=*ptr;; i++)
	{
		switch (input->data[i]) {
		case 0:
			return 0;
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case ',':
		case ';':
			break;
		case '#':
			for (j=i+1; j<input->len-1; j++)
			{
				if (input->data[j] == '\n')
					break;
			}
			i = j;
			break;
		case '"':
			for (j=i+1;; j++)
			{
				if (input->data[j] == '"')
					break;

				if (j == input->len-1)
				{
					// attempt to read in more of the file
					if (!cfg_fread(input))
					{
						asprintf(output, "Missing terminating double quote");
						return CFG_ERROR;
					}
				}
			}

			l = j-i-1;

			if ((*output = malloc(l+1)))
			{
				memcpy(*output, &input->data[i+1], l);
				(*output)[l] = 0;
			}
			else
			{
				asprintf(output, "malloc: %s", strerror(errno));
				return CFG_ERROR;
			}

			*ptr = j+1;
			return CFG_QUOTED;
		case '{':
			for (j=i+1;; j++)
			{
				if (input->data[j] == '{')
				{
					k++;
				}
				else if ((input->data[j] == '}') && (--k < 0))
				{
					break;
				}

				if (j == input->len-1)
				{
					// attempt to read in more of the file
					if (!cfg_fread(input))
					{
						asprintf(output, "Missing close brace '}'");
						return CFG_ERROR;
					}
				}
			}

			l = j-i-1;

			if ((*output = malloc(l+1)))
			{
				memcpy(*output, &input->data[i+1], l);
				(*output)[l] = 0;
			}
			else
			{
				asprintf(output, "malloc: %s", strerror(errno));
				return CFG_ERROR;
			}

			*ptr = j+1;
			return CFG_BLOCK;
		default:
			for (j=i;; j++)
			{
				switch (input->data[j]) {
				case ' ':
				case '\t':
				case '\n':
				case '\r':
				case ',':
				case ';':
				case '{':
				case '}':
				case '#':
				case '"':
					k = 1;
					break;
				}
				if (k == 1) break;

				if (j == input->len-1)
				{
					// attempt to read in more of the file
					if (!cfg_fread(input))
						break;
				}
			}

			l = j-i;

			if ((*output = malloc(l+1)))
			{
				memcpy(*output, &input->data[i], l);
				(*output)[l] = 0;
			}
			else
			{
				asprintf(output, "malloc: %s", strerror(errno));
				return CFG_ERROR;
			}

			*ptr = j;
			return CFG_KEYWORD;
		}

		if (i == input->len-1)
		{
			// attempt to read in more of the file
			if (!cfg_fread(input))
				break;
		}
	}

	return 0;
}

int cfg_readblock(char* input, char** output, unsigned int *ptr)
{
	int i,k=0,l;
	char* ignore = "\n\r\t ,;";

	if (input == NULL)
		return 0;

	*output = NULL;

	// remove initial whitespace and other garbage
	*ptr += strspn(&input[*ptr], ignore);

	// remove all the comments before the next element
	while (input[*ptr] == '#')
	{
		// remove the comment text up to the newline
		*ptr += strcspn(&input[*ptr], "\n");

		// remove whitespace and other garbage after it
		*ptr += strspn(&input[*ptr], ignore);
	}

	// bailout if we've hit the end of the block
	if (input[*ptr] == 0)
		return 0;

	// now to see what kind of element we'll be reading
	switch (input[*ptr]) {
	case '"':
		for (i=*ptr+1;; i++)
		{
			if (input[i] == 0)
			{
				asprintf(output, "Missing terminating double quote");
				return CFG_ERROR;
			}
			else if (input[i] == '"')
			{
				break;
			}
		}

		l = i-*ptr-1;

		if ((*output = malloc(l+1)))
		{
			memcpy(*output, &input[*ptr+1], l);
			(*output)[l] = 0;
		}
		else
		{
			asprintf(output, "malloc: %s", strerror(errno));
			return CFG_ERROR;
		}

		*ptr = i+1;
		return CFG_QUOTED;
	case '{':
		for (i=*ptr+1;; i++)
		{
			if (input[i] == 0)
			{
				asprintf(output, "Missing close brace '}'");
				return CFG_ERROR;
			}
			else if (input[i] == '{')
			{
				k++;
			}
			else if ((input[i] == '}') && (--k < 0))
			{
				break;
			}
		}

		l = i-*ptr-1;

		if ((*output = malloc(l+1)))
		{
			memcpy(*output, &input[*ptr+1], l);
			(*output)[l] = 0;
		}
		else
		{
			asprintf(output, "malloc: %s", strerror(errno));
			return CFG_ERROR;
		}

		*ptr = i+1;
		return CFG_BLOCK;
	default:
		l = strcspn(&input[*ptr], " \t\n\r,;{}#\"");

		if ((*output = malloc(l+1)))
		{
			memcpy(*output, &input[*ptr], l);
			(*output)[l] = 0;
		}
		else
		{
			asprintf(output, "malloc: %s", strerror(errno));
			return CFG_ERROR;
		}

		*ptr += l;
		return CFG_KEYWORD;
	}
}
