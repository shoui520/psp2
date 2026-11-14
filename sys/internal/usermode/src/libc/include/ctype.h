/*
 * ctype.h: the SDK's character classes, with Sony's psp2 definitions (SDK Release 03.570.011,
 * target/include/ctype.h): the classes are bits of the short at _Ctype[c], c from EOF to 255.
 */
#ifndef _CTYPE
#define _CTYPE

#include <yvals.h>

#define _XB		0x400	/* extra blank */
#define _XA		0x200	/* extra alphabetic */
#define _XS		0x100	/* extra space */
#define _BB		0x80	/* BEL, BS, etc. */
#define _CN		0x40	/* CR, FF, HT, NL, VT */
#define _DI		0x20	/* '0'-'9' */
#define _LO		0x10	/* 'a'-'z' */
#define _PU		0x08	/* punctuation */
#define _SP		0x04	/* space */
#define _UP		0x02	/* 'A'-'Z' */
#define _XD		0x01	/* '0'-'9', 'A'-'F', 'a'-'f' */

#ifndef EOF
#define EOF		(-1)
#endif

typedef const short *_Ctype_t;

extern const _Ctype_t _Ctype;
extern const _Ctype_t _Tolotab;
extern const _Ctype_t _Touptab;

#define _Getpctype()	_Ctype
#define _Getptolower()	_Tolotab
#define _Getptoupper()	_Touptab

#define _Getchrtype(x)	_Getpctype()[(int)(x)]
#define _CTolower(x)	_Getptolower()[(int)(x)]
#define _CToupper(x)	_Getptoupper()[(int)(x)]

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

#define isalnum(c)	(_Getchrtype(c) & (_DI|_LO|_UP|_XA))
#define isalpha(c)	(_Getchrtype(c) & (_LO|_UP|_XA))
#define isblank(c)	(_Getchrtype(c) & (_SP|_XB))
#define iscntrl(c)	(_Getchrtype(c) & _BB)
#define isdigit(c)	(_Getchrtype(c) & _DI)
#define isgraph(c)	(_Getchrtype(c) & (_DI|_LO|_PU|_UP|_XA))
#define islower(c)	(_Getchrtype(c) & _LO)
#define isprint(c)	(_Getchrtype(c) & (_DI|_LO|_PU|_SP|_UP|_XA))
#define ispunct(c)	(_Getchrtype(c) & _PU)
#define isspace(c)	(_Getchrtype(c) & (_CN|_SP|_XS))
#define isupper(c)	(_Getchrtype(c) & _UP)
#define isxdigit(c)	(_Getchrtype(c) & _XD)
#define tolower(c)	_CTolower(c)
#define toupper(c)	_CToupper(c)

#endif /* _CTYPE */
