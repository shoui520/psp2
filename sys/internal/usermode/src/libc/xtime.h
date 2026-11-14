/*
 * xtime.h: Dinkumware's internal time declarations, as SceLibc uses them.
 */
#ifndef _XTIME
#define _XTIME

#include <stddef.h>
#include <time.h>

size_t _Strftime(char *buf, size_t bufsize, const char *fmt, const struct tm *t);
const char *_Gentime(const struct tm *t, char qual, char code, int *pn, char *ac);
/* the seconds from 1900 to 1970 */
#define _TBIAS	((time_t)((70 * 365LU + 17) * 86400))

char *_Getasctimebuf(void);

#endif /* _XTIME */
