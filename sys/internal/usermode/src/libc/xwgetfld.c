/*
 * _WGetfld function: convert one wscanf field. A %n argument the caller passes
 * as a null pointer is a runtime-constraint violation.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include "xwstdio.h"

#define BAD_N_ARGUMENT()	(EOF - _EXT1_FAIL("scanf_s: bad %n argument"))

/* store the count of characters read through a %n argument of type T */
#define STORE_COUNT(px, T)						\
	do {								\
		T *p = va_arg((px)->ap, T *);				\
									\
		if (p == NULL)						\
			return BAD_N_ARGUMENT();			\
		*p = (px)->nchar;					\
	} while (0)

int _WGetfld(_WSft *px)
{
	px->stored = 0;
	switch (*px->s) {	/* switch on the conversion */
	case 'c':	/* an array of char */
		return _WGetstr(px, 0);
	case 'd':	/* an integer */
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	case 'p':
		return _WGetint(px, NULL);
	case 'e':	/* a floating-point value */
	case 'E':
	case 'g':
	case 'G':
	case 'f':
	case 'F':
	case 'a':
	case 'A':
		return _WGetfloat(px, NULL);
	case 'n':	/* the count of characters read */
		if (!px->noconv) {
			switch (px->qual) {	/* store in the integer type */
			case 'b':
				STORE_COUNT(px, signed char);
				break;
			case 'q':
				STORE_COUNT(px, long long);
				break;
			case 'j':
				STORE_COUNT(px, intmax_t);
				break;
			case 't':
				STORE_COUNT(px, ptrdiff_t);
				break;
			case 'z':
				STORE_COUNT(px, size_t);
				break;
			case 'h':
				STORE_COUNT(px, short);
				break;
			case 'l':
				STORE_COUNT(px, long);
				break;
			default:
				STORE_COUNT(px, int);
				break;
			}
		}
		return 1;
	case 's':	/* a string */
		return _WGetstr(px, 1);
	case '%': {	/* a '%' */
		wint_t ch;

		if ((ch = GET(px)) == L'%')
			return 1;
		UNGETN(px, ch);
		return ch == WEOF ? EOF : 0;
	}
	case '[':	/* a scan set */
		return _WGetstr(px, -1);
	default:	/* an undefined conversion: stop */
		return 0;
	}
}
