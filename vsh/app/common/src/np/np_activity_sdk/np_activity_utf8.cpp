/*
 * The status message's UTF-8 counts: characters, and linefeeds, up to its NUL or a size in
 * bytes. Sony's build keeps them out of the client's file (called, not inlined); the file's
 * name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "np_activity_client.h"

/* the size of the UTF-8 sequence at p (a stray continuation byte counts as one), 0 if invalid */
static int utf8SequenceSize(const char *p)
{
	char c;

	if (p == NULL)
		return 0;
	c = *p;
	if ((c & 0xc0) == 0x80)
		return 1;
	if ((c & 0x80) == 0)
		return 1;
	if ((c & 0xe0) == 0xc0 && (unsigned char)c >= 0xc2)
		return 2;
	if ((c & 0xf0) == 0xe0)
		return 3;
	if ((c & 0xf8) == 0xf0)
		return 4;
	if ((c & 0xfc) == 0xf8)
		return 5;
	if ((c & 0xfe) == 0xfc)
		return 6;
	return 0;
}

/* the UTF-8 characters of string, up to its NUL or size bytes */
int countUtf8Characters(const char *string, SceSize size)
{
	const char *end;
	const char *p;
	int count = 0;
	int sequenceSize;

	if (string == NULL)
		return 0;
	end = string + size;
	for (p = string; (SceSize)(p - string) <= size && *p != '\0'; p += sequenceSize) {
		sequenceSize = utf8SequenceSize(p);
		if (sequenceSize == 0)
			break;
		count++;
		if (end - p < sequenceSize)
			break;
	}
	return count;
}

/* its linefeeds, counted the same way */
int countLinefeeds(const char *string, SceSize size)
{
	const char *end;
	const char *p;
	int count = 0;
	int sequenceSize;

	if (string == NULL)
		return 0;
	end = string + size;
	for (p = string; (SceSize)(p - string) <= size && *p != '\0'; p += sequenceSize) {
		sequenceSize = utf8SequenceSize(p);
		if (sequenceSize == 0)
			break;
		if (sequenceSize == 1 && *p == '\n')
			count++;
		if (end - p < sequenceSize)
			break;
	}
	return count;
}
