/*
 * psp2io.h: the C library's system interface on psp2 (psp2io.c, psp2time.c): file numbers
 * over FIOS2 and the process's standard I/O descriptors. The names are ours.
 */
#ifndef _LIBC_PSP2IO_H
#define _LIBC_PSP2IO_H

#include <stddef.h>
#include <scetypes.h>

/* what _Read, _Write and _Lseek return for a file number that is not open */
#define _NOT_OPEN	(-100)

/* the process's standard input, output and error descriptors (module_start) */
extern SceUID _StdinFd;
extern SceUID _StdoutFd;
extern SceUID _StderrFd;

/* a file's status (_Stat) */
struct _Stat {
	unsigned int st_dev;
	unsigned int st_ino;
	unsigned int st_mode;
	unsigned int st_nlink;
	unsigned int st_uid;
	unsigned int st_gid;
	unsigned int st_rdev;
	long long st_size;
	unsigned int st_atime;
	unsigned int st_mtime;
	unsigned int st_ctime;
	unsigned int st_blksize;
	unsigned int st_blocks;
};

/* the time of day (_Gettimeofday) */
struct _Timeval {
	int tv_sec;
	long long tv_usec;
};

struct _Timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

int _FiosOpenFlags(int flags);
int _FiosErrorToErrno(int err);
int _Read(int fd, void *buf, size_t nbyte);
int _Write(int fd, const void *buf, size_t nbyte);
int _Open(const char *path, int flags, ...);
int _Fopen(const char *path, unsigned int smode, int share);
int _Close(int fd);
long long _Lseek(int fd, long long offset, int whence);
int _Isatty(int fd);
int _Mkdir(const char *path);
int _Rmdir(const char *path);
int _Truncate(const char *path, long long length);

int _Stat(const char *path, struct _Stat *st);
int _Access(const char *path, unsigned int mode);
int _Chmod(const char *path, unsigned int mode);
int _Gettimeofday(struct _Timeval *tv, struct _Timezone *tz);
struct tm *_Gmtime_r(const unsigned int *tod, struct tm *tm);
struct tm *_Localtime_r(const unsigned int *tod, struct tm *tm);
int _Getrusage(int *who);
int _Getpid(void);
void _ExitProcess(int status);
void _Nop(void);

#endif /* _LIBC_PSP2IO_H */
