/*
 * yvals.h: the SDK's C library configuration, with Sony's psp2 definitions (SDK Release
 * 03.570.011, target/include/yvals.h): the base types the C library's headers share, the
 * multibyte conversion state and the library's system locks.
 */
#ifndef _YVALS
#define _YVALS

#define _MBMAX		4	/* MB_LEN_MAX: UTF-8 is psp2's multibyte stream format */

#define _WCMIN		0x0
#define _WCMAX		0xffff
#define _WIMAX		0xffffffffU

#ifndef __cplusplus
typedef unsigned short _Wchart;
typedef unsigned int _Wintt;
#endif

typedef unsigned int _Sizet;
typedef int _Ptrdifft;
typedef unsigned long _Uint32t;
typedef long long _Longlong;
typedef unsigned long long _ULonglong;

#define _FD_TYPE		int
#define _FILE_OFFSET_TYPE	signed long long

/* the state of a multibyte translation */
#ifndef _MBSTATET
#define _MBSTATET
typedef struct _Mbstatet {
	unsigned long _Wchar;
	unsigned short _Byte, _State;
} _Mbstatet;
#endif

#ifndef _Mbstinit
#define _Mbstinit(x)	_Mbstatet x = {0}
#endif

/* the C library's mutex */
typedef struct _SceLibcMutexType {
	signed long long data[4];
} _SceLibcMutexType;

#ifdef __cplusplus
extern "C" {
#endif

/* the library's system locks (_Locksyslock) */
#define _LOCK_STREAM	0
#define _LOCK_DEBUG	1
#define _LOCK_MISC	2
#define _LOCK_ATEXIT	3
#define _LOCK_GUARD	4
#define _LOCK_ONCE	5
#define _MAX_LOCK	6	/* one more than the highest lock number */

#define _RSIZE_MAX	((unsigned int)(-1) >> 1)

int _Locksyslock(unsigned int);
int _Unlocksyslock(unsigned int);

#ifdef __cplusplus
}
#endif

#endif /* _YVALS */
