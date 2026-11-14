/*
 * psp2time.c: the time and file status functions of the C library's system interface on
 * psp2, over SceProcessmgr's C library time and SceIofilemgr's status calls.
 */
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <kernel/processmgr.h>
#include <processmgr/processmgr.h>

#include "psp2io.h"

#define TM_YEAR_BASE	1900

static inline int sceErrorToErrno(int err)
{
	int e = EIO;

	if (SCE_ERROR_FACILITY(err) == SCE_ERROR_FACILITY_ERRNO)
		e = (unsigned short)err;
	return e;
}

time_t mktime(struct tm *tptr)
{
	ProcessTm tm;
	SceKernelTime t;
	ProcessTimezone tz;

	tm.sec = tptr->tm_sec;
	tm.min = tptr->tm_min;
	tm.hour = tptr->tm_hour;
	tm.mday = tptr->tm_mday;
	tm.mon = tptr->tm_mon;
	tm.year = tptr->tm_year + TM_YEAR_BASE;
	tm.wday = 0;
	tm.yday = 0;
	tm.isdst = 0;
	if (sceKernelLibcMktime(&tm, &t, (SceUInt64 *)&tz) < 0)
		return (time_t)-1;
	tptr->tm_sec = tm.sec;
	tptr->tm_min = tm.min;
	tptr->tm_hour = tm.hour;
	tptr->tm_mday = tm.mday;
	tptr->tm_mon = tm.mon;
	tptr->tm_year = tm.year >= TM_YEAR_BASE ? tm.year - TM_YEAR_BASE : tm.year;
	tptr->tm_wday = tm.wday;
	tptr->tm_yday = tm.yday;
	tptr->tm_isdst = tm.isdst;
	return t;
}

/* the time of an SceDateTime, or -1 */
static inline unsigned int dateTimeToTime(const SceDateTime *dt)
{
	ProcessTm tm;
	SceKernelTime t;
	ProcessTimezone tz;

	tm.sec = dt->second;
	tm.min = dt->minute;
	tm.hour = dt->hour;
	tm.mday = dt->day;
	tm.mon = dt->month;
	tm.year = dt->year + TM_YEAR_BASE;
	tm.wday = 0;
	tm.yday = 0;
	tm.isdst = 0;
	if (sceKernelLibcMktime(&tm, &t, (SceUInt64 *)&tz) < 0)
		return (unsigned int)-1;
	return t;
}

int _Stat(const char *path, struct _Stat *st)
{
	SceIoStat stat;
	int res;

	res = sceIoGetstat(path, &stat);
	if (res < 0) {
		errno = sceErrorToErrno(res);
		return -1;
	}
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_mode = stat.st_mode;
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = stat.st_size;
	st->st_atime = dateTimeToTime(&stat.st_atime);
	st->st_mtime = dateTimeToTime(&stat.st_mtime);
	st->st_ctime = dateTimeToTime(&stat.st_ctime);
	st->st_blksize = 0;
	st->st_blocks = 0;
	return res;
}

int _Access(const char *path, unsigned int mode)
{
	SceIoStat stat;
	int res;

	res = sceIoGetstat(path, &stat);
	if (res < 0) {
		errno = sceErrorToErrno(res);
		return -1;
	}
	return (stat.st_mode & mode) != mode;
}

int _Chmod(const char *path, unsigned int mode)
{
	SceIoStat stat;
	int res;

	memset(&stat, 0, sizeof(stat));
	stat.st_mode = mode;
	res = sceIoChstat(path, &stat, SCE_CST_MODE);
	if (res < 0) {
		errno = sceErrorToErrno(res);
		return -1;
	}
	return res;
}

clock_t clock(void)
{
	return sceKernelLibcClock();
}

time_t time(time_t *tod)
{
	SceKernelTime t;
	time_t res;

	res = sceKernelLibcTime(&t);
	if (tod != NULL)
		*tod = res;
	return res;
}

int _Gettimeofday(struct _Timeval *tv, struct _Timezone *tz)
{
	ProcessTimeval ptv;
	ProcessTimezone ptz;
	int res;

	res = sceKernelLibcGettimeofday(&ptv, &ptz);
	if (res < 0) {
		errno = sceErrorToErrno(res);
		return -1;
	}
	if (tv != NULL) {
		tv->tv_sec = ptv.sec;
		tv->tv_usec = ptv.usec;
	}
	if (tz != NULL) {
		tz->tz_minuteswest = ptz.minuteswest;
		tz->tz_dsttime = ptz.dsttime;
	}
	return res;
}

static inline void processTmToTm(const ProcessTm *ptm, struct tm *tm)
{
	tm->tm_sec = ptm->sec;
	tm->tm_min = ptm->min;
	tm->tm_hour = ptm->hour;
	tm->tm_mday = ptm->mday;
	tm->tm_mon = ptm->mon;
	tm->tm_year = ptm->year >= TM_YEAR_BASE ? ptm->year - TM_YEAR_BASE : ptm->year;
	tm->tm_wday = ptm->wday;
	tm->tm_yday = ptm->yday;
	tm->tm_isdst = ptm->isdst;
}

struct tm *_Gmtime_r(const unsigned int *tod, struct tm *tm)
{
	SceKernelTime t = *tod;
	ProcessTm ptm;

	sceKernelLibcGmtime_r(&t, &ptm);
	if (tm != NULL)
		processTmToTm(&ptm, tm);
	return tm;
}

struct tm *_Localtime_r(const unsigned int *tod, struct tm *tm)
{
	SceKernelTime t = *tod;
	ProcessTm ptm;

	sceKernelLibcLocaltime_r(&t, &ptm);
	if (tm != NULL)
		processTmToTm(&ptm, tm);
	return tm;
}

int _Getrusage(int *who)
{
	*who = -1;
	return 0;
}

int _Getpid(void)
{
	return -1;
}

void _ExitProcess(int status)
{
	sceKernelExitProcess(status);
}

void _Nop(void)
{
}
