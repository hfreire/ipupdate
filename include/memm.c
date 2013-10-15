#include "memm.h"

unsigned int memmlen(struct mem *mem, unsigned int len)
{
	char *ptr;

	if (mem->size < len)
	{
		if ((ptr = realloc(mem->data, len)) == NULL)
			return mem->size;
		mem->data = ptr;
		mem->size = len;
	}
	return mem->len = len;
}

unsigned int memmcpy(struct mem *mem, char *data, unsigned int datalen)
{
	char *ptr;

	if (mem->size < datalen)
	{
		if ((ptr = realloc(mem->data, datalen)) == NULL)
			return mem->len;
		mem->data = ptr;
		mem->size = datalen;
	}
	if (memcpy(mem->data, data, datalen) == NULL)
		return 0;
	return mem->len = datalen;
}

unsigned int memmcat(struct mem *mem, char *data, unsigned int datalen)
{
	unsigned int newlen = mem->len + datalen;
	char *ptr;

	if (mem->size < newlen)
	{
		if ((ptr = realloc(mem->data, newlen)) == NULL)
			return mem->len;
		mem->data = ptr;
		mem->size = newlen;
	}
	if (memcpy(&mem->data[mem->len], data, datalen) == NULL)
		return mem->len;
	return mem->len = newlen;
}

void memmfree(struct mem *mem)
{
	if (mem->data)
		free(mem->data);
	memset(mem, 0, sizeof(struct mem));
}

// This function will discard len bytes from the beginning of the memory block

void mm_unshift(struct mem *mem, unsigned int len)
{
	if (mem->len > len)
	{
		mem->len -= len;
		memmove(mem->data, &mem->data[len], mem->len);
	}
	else
	{
		mem->len = 0;
	}
}
