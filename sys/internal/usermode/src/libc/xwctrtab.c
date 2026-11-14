/*
 * xwctrtab.c: the C locale's wide character mappings for towctrans: (low, high, delta)
 * ranges of each property, then the names wctrans looks up, each with its ranges' offset
 */
#include <wchar.h>

#include "xwctype.h"

static const short wctrans_tab[] = {
	L'A', L'Z', 32,	/* tolower */
	L'a', L'z', -32,	/* toupper */
};

static const _Wctab wctrans_ents[] = {
	{(const char *)wctrans_tab, 0},
	{"tolower", 0},
	{"toupper", 3},
	{(const char *)0, 6},
};

const _Wctab *_Wctrans = wctrans_ents;
