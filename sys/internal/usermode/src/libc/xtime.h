/*
 * xtime.h: Dinkumware's internal time declarations, as SceLibc uses them.
 */
#ifndef _XTIME
#define _XTIME

#include <stddef.h>
#include <time.h>

size_t _Strftime(char *buf, size_t bufsize, const char *fmt, const struct tm *t);
char *_Getasctimebuf(void);

#endif /* _XTIME */
