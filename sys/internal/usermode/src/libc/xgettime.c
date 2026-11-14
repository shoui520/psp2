/*
 * _Gettime function: field n of a time string, the fields separated by the
 * string's first character; the field's length goes to *len (0 when there are
 * fewer fields, the string's end returned).
 */
#include <string.h>

#include "xtime.h"

const char *_Gettime(const char *s, int n, int *len)
{
	const char delim = *s != '\0' ? *s++ : '\0';
	const char *s1;

	for (; ; --n, s = s1 + 1) {	/* find the end of the current field */
		if ((s1 = strchr(s, delim)) == NULL)
			s1 = s + strlen(s);
		if (n <= 0) {	/* the field */
			*len = s1 - s;
			return s;
		} else if (*s1 == '\0') {	/* not enough fields */
			*len = 0;
			return s1;
		}
	}
}
