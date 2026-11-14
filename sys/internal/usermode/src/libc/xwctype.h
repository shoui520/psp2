/*
 * xwctype.h: the C library's wide character class and mapping tables. Entry 0 of each table
 * describes the table; entries 1 on are a property's name and its offset in the class or
 * mapping data, ended by a null name.
 */
#ifndef _XWCTYPE
#define _XWCTYPE

typedef struct _Wctab {
	const char *_Name;	/* the property's name */
	int _Off;		/* its offset in the table's data */
} _Wctab;

extern const _Wctab *_Wctrans;
extern const _Wctab *_Wctype;

#endif /* _XWCTYPE */
