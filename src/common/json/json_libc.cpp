/*
 * libSceJson: string helpers.  Small C-library replacements the library
 * carries itself (they also appear in safemode, libpaf and np_message).
 * Their original names are unknown (PLACEHOLDER names).
 */

#include "json_internal.h"

namespace sce {
namespace Json {

#define U	JSON_CTYPE_UPPER
#define L	JSON_CTYPE_LOWER
#define D	JSON_CTYPE_DIGIT
#define S	JSON_CTYPE_SPACE
#define P	JSON_CTYPE_PUNCT
#define C	JSON_CTYPE_CNTRL
#define X	JSON_CTYPE_XDIGIT

/* classes of EOF (first entry) and of the 128 ASCII characters */
static const char s_ctype[1 + 128] = {
	0,
	C, C, C, C, C, C, C, C, C, C | S, C | S, C | S, C | S, C | S, C, C,
	C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,
	S, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
	D, D, D, D, D, D, D, D, D, D, P, P, P, P, P, P,
	P, U | X, U | X, U | X, U | X, U | X, U | X, U, U, U, U, U, U, U, U, U,
	U, U, U, U, U, U, U, U, U, U, U, P, P, P, P, P,
	P, L | X, L | X, L | X, L | X, L | X, L | X, L, L, L, L, L, L, L, L, L,
	L, L, L, L, L, L, L, L, L, L, L, P, P, P, P, C,
};

#undef U
#undef L
#undef D
#undef S
#undef P
#undef C
#undef X

/* Index check failure of String::at() and friends: stops in the debugger. */
int jsonOutOfRange(void)
{
	jsonDebugHook();
	JSON_BREAKPOINT();
	return -1;
}

/* atof() without locale or errno; 0 when s holds no digits. */
double jsonStrtod(const char *s)
{
	bool digits = false;
	int exponent = 0;
	double sign, value, scale;
	bool negative;

	if (s == NULL)
		return 0;
	while (jsonCtype(*s) & JSON_CTYPE_SPACE)
		s++;
	sign = 1.0;
	while (*s == '-' || *s == '+') {
		if (*s == '-')
			sign = -sign;
		s++;
	}
	value = 0.0;
	while (jsonCtype(*s) & JSON_CTYPE_DIGIT) {
		value = value * 10.0 + (*s - '0');
		s++;
		digits = true;
	}
	if (*s == '.') {
		scale = 10.0;
		s++;
		while (jsonCtype(*s) & JSON_CTYPE_DIGIT) {
			value += (*s - '0') / scale;
			scale *= 10.0;
			s++;
			digits = true;
		}
	}
	if (!digits)
		return 0;
	if (*s == 'd' || *s == 'D' || *s == 'e' || *s == 'E') {
		negative = false;
		digits = false;
		s++;
		if (*s == '-') {
			negative = true;
			s++;
		} else if (*s == '+') {
			s++;
		}
		while (jsonCtype(*s) & JSON_CTYPE_DIGIT) {
			exponent = exponent * 10 + (*s - '0');
			digits = true;
			s++;
		}
		if (!digits)
			return 0;
		if (negative) {
			for (; exponent != 0; exponent--)
				value /= 10.0;
		} else {
			for (; exponent != 0; exponent--)
				value *= 10.0;
		}
	}
	return value * sign;
}

JSON_NOINLINE
int jsonCtype(int c)
{
	if ((unsigned int)c > 0x7f)
		return 0;
	return s_ctype[c + 1];
}

const char *jsonStrchr(const char *s, int c)
{
	do {
		if (*s == c)
			return s;
	} while (*s++ != '\0');
	return NULL;
}

/* strcmp() that orders NULL before every string */
int jsonStrcmp(const char *a, const char *b)
{
	if (a == NULL || b == NULL) {
		if (a == b)
			return 0;
		if (a == NULL)
			return -1;
		return 1;
	}
	while (*a == *b++) {
		if (*a++ == '\0')
			return 0;
	}
	return (unsigned char)*a - (unsigned char)b[-1];
}

size_t jsonStrlen(const char *s)
{
	if (s == NULL)
		jsonDebugHook();
	return sceClibStrnlen(s, (unsigned int)-1);
}

/* strncmp() that orders NULL before every string */
int jsonStrncmp(const char *a, const char *b, int n)
{
	if (a == NULL || b == NULL) {
		if (a == b)
			return 0;
		if (a == NULL)
			return -1;
		return 1;
	}
	while (--n >= 0 && *a == *b++) {
		if (*a++ == '\0')
			return 0;
	}
	return n < 0 ? 0 : (unsigned char)*a - (unsigned char)b[-1];
}

} /* namespace Json */
} /* namespace sce */
