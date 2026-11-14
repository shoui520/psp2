/*
 * xwctytab.c: the C locale's wide character classes for iswctype: (low, high, 1) ranges of
 * each class, then the names wctype looks up, each with its ranges' offset
 */
#include <wchar.h>

#include "xwctype.h"

static const unsigned short wctype_tab[] = {
	L'A', L'Z', 1,  L'a', L'z', 1,  L'0', L'9', 1,	/* alnum */
	L'A', L'Z', 1,  L'a', L'z', 1,	/* alpha */
	0x00, 0x1f, 1,  0x7f, 0x7f, 1,	/* cntrl */
	L'0', L'9', 1,	/* digit */
	L'!', L'~', 1,	/* graph */
	L'a', L'z', 1,	/* lower */
	L' ', L'~', 1,	/* print */
	L'!', L'/', 1,  L':', L'@', 1,  L'[', L'`', 1,  L'{', L'~', 1,	/* punct */
	L' ', L' ', 1,  0x09, 0x0d, 1,	/* space */
	L'A', L'Z', 1,	/* upper */
	L'0', L'9', 1,  L'a', L'f', 1,  L'A', L'F', 1,	/* xdigit */
	L' ', L' ', 1,  0x09, 0x09, 1,	/* blank */
	0x09, 0x0d, 1,	/* unused */
};

static const _Wctab wctype_ents[] = {
	{(const char *)wctype_tab, 0},
	{"alnum", 0},
	{"alpha", 9},
	{"cntrl", 15},
	{"digit", 21},
	{"graph", 24},
	{"lower", 27},
	{"print", 30},
	{"punct", 33},
	{"space", 45},
	{"upper", 51},
	{"xdigit", 54},
	{"blank", 63},
	{(const char *)0, 69},
};

const _Wctab *_Wctype = wctype_ents;
