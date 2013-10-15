#include "array.h"

/*
** split
**
** Accepts a string argument converting all delimeters to nulls
** Returns an allocated array pointer or NULL
**
** str      string to be mangled
** del      delimiting characters converted to nulls
** cnt      [in]  reallocate array in increments of cnt
**          [out] array element count
**
** str is not to be deallocated while using this array
*/

char** split(char* str, const char* del, int* cnt)
{
	char** args = NULL;
	int inc;

	if ((str == NULL) || (del == NULL) || (cnt == NULL))
		return NULL;

	inc = *cnt ? *cnt : 8;
	*cnt = 0;

	//keep looping while the next char isn't null
	while (*str)
	{
		//skip past delimiters
		str += strspn(str,del);
		if (*str == '\0')
			return args;

		//allocate more space if we've filled up what was already allocated
		if ((*cnt % inc) == 0)
		{
			args = realloc(args, sizeof(char*) * (*cnt + inc));
		}
		//store the pointer to this element in the next available index
		args[(*cnt)++] = str;

		//skip to the next delimiter or bailout if there isn't one
		str = strpbrk(str, del);
		if (str == NULL)
			return args;
		//replace delimiter with a nullchar and skip it
		*str++ = '\0';
	}

	return args;
}

/*
** splice
**
** remove length elements from array starting at offset
** All trailing elements will be left aligned
**
** array    array to remove elements from
** cnt      array element count
** offset   [<    0] first element to be removed from the end of the array
**          [>=   0] first element to be removed from the beginning of the array
**          [>= cnt] don't remove anything
** length   [ =   0] remove all elements starting at offset
**          [<    0] remove all elements starts at offset except abs(length) elements
**          [>    0] remove length elements starting at offset, or less if array is too short
**
** Returns the number of elements removed
*/

int _splice(void** array, int cnt, int offset, int length)
{
	int t;

	if (array == NULL) return 0;
	if (cnt < 1) return 0;

	if (offset < 0)
	{
		// convert negative offsets to a positive value
		offset = cnt-offset-1;

		// if offset is still negative, set to 0
		if (offset < 0) offset = 0;
	}
	else if (offset >= cnt)
	{
		// don't remove anything if offset is beyond the end of the array
		return 0;
	}

	if ((length == 0) || (length > cnt-offset))
	{
		// set length to the number of elements after offset
		length = cnt-offset;
		t = 0;
	}
	else if (length < 0)
	{
		t = length * -1;
		// set length to the number of elements after offset minus length
		length = cnt-offset-length;

		// if length is still less than 1, remove nothing
		if (length < 1) return 0;
	}
	else
	{
		t = (cnt-offset)-length;
	}

	// left align trailing elements
	if (t) memmove(&array[offset], &array[cnt-t], t*sizeof(void*));

	return length;
}
