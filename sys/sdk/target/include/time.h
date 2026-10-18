/*
 * time.h: the SDK's date and time functions, with Sony's psp2 types (SDK Release 03.570.011,
 * target/include/time.h and yvals.h): clock_t and time_t are unsigned int, clock() counts
 * microseconds.
 */
#ifndef _TIME
#define _TIME

#ifndef NULL
#define NULL		0
#endif

#define CLOCKS_PER_SEC	1000000

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#if !defined(_CLOCK_T) && !defined(__clock_t_defined) && !defined(_BSD_CLOCK_T_DEFINED_)
#define _CLOCK_T
#define __clock_t_defined
#define _BSD_CLOCK_T_DEFINED_
typedef unsigned int clock_t;
#endif

#if !defined(_TIME_T) && !defined(__time_t_defined) && !defined(_BSD_TIME_T_DEFINED_) && \
    !defined(_TIME_T_DEFINED)
#define _TIME_T
#define __time_t_defined
#define _BSD_TIME_T_DEFINED_
#define _TIME_T_DEFINED
typedef unsigned int time_t;
#endif

#ifndef _TM_DEFINED
#define _TM_DEFINED
struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

time_t time(time_t *tod);
char *asctime(const struct tm *tptr);
clock_t clock(void);
char *ctime(const time_t *tod);
double difftime(time_t t1, time_t t0);
struct tm *gmtime(const time_t *tod);
struct tm *localtime(const time_t *tod);
time_t mktime(struct tm *tptr);
size_t strftime(char *__restrict s, size_t n, const char *__restrict format,
		const struct tm *__restrict tptr);

#ifdef __cplusplus
}
#endif

#endif /* _TIME */
