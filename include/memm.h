#include <stdlib.h>
#include <string.h>

#ifndef _MEMM_H
#define _MEMM_H
struct mem
{
	char *data;
	unsigned int size;
	unsigned int len;
};
#endif

// replacements for standard functions

unsigned int memmlen(struct mem*, unsigned int);
unsigned int memmcpy(struct mem*, char*, unsigned int);
unsigned int memmcat(struct mem*, char*, unsigned int);
void memmfree(struct mem*);

// non-standard functions

void mm_unshift(struct mem*, unsigned int);
