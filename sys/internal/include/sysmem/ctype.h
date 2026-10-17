/*
 * Kernel character classes: the bits of SceSysclib's class table, look_ctype_table (NID
 * 0xCDF7F155, the PSP sysclib name; declared in sysmem/sysclib.h). Sony's callers pass the
 * character sign-extended (char is signed SceChar8) and use the class bits as an int.
 */
#pragma once

#include <sysmem/sysclib.h>

/* the class bits (BSD ctype) */
#define _U	0x01	/* upper case */
#define _L	0x02	/* lower case */
#define _N	0x04	/* digit */
#define _S	0x08	/* space */
#define _P	0x10	/* punctuation */
#define _C	0x20	/* control */
#define _X	0x40	/* hex digit */
#define _B	0x80	/* blank */

#define isdigit(c)	(look_ctype_table(c) & _N)
