/*
 * libSceFt2's C library: the part of the C library FreeType's ftsystem.c and the autofitter use.
 * The module links these, built by Sony with GCC, in place of the system's: a stdio over sceIo
 * (20 FILE slots under one LwMutex), a few string and memory routines, and the qsort kernel.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/iofilemgr.h>

#define FT2_NUM_FILES		20

/* the FILE record the library keeps (36 bytes; fopen hands out a slot of the table) */
typedef struct Ft2File {
	SceUInt32 mode;			/* 0 for a free slot, else the mode flags fopen derived */
	SceUID fd;
	SceUInt32 deviceType;		/* 0x8000, or 0x800 for sa0: */
	SceUInt32 state;		/* 1: error, 2: end of file */
	SceInt32 pushback;		/* -1 when empty */
	SceInt32 position;
	SceUInt32 reserved[3];
} Ft2File;

static SceKernelLwMutexWork g_ioMutex;
static SceInt32 g_ioMutexCreated;
static Ft2File g_files[FT2_NUM_FILES];

int __sceLibFt2SetLibCHook(void)
{
	if (g_ioMutexCreated == 0) {
		g_ioMutexCreated = 1;
		return sceKernelCreateLwMutex(&g_ioMutex, "SceLibFt2IoMutex", 0x2000, 0, NULL);
	}
	return 0;
}

int __sceLibFt2DoneLibCHook(void)
{
	return sceKernelDeleteLwMutex(&g_ioMutex);
}

void *memset(void *dst, int ch, size_t len)
{
	sceClibMemset(dst, ch, len);
	return dst;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1, *p2 = s2;
	unsigned int c1, c2;

	if ((int)n <= 0)
		return 0;
	c1 = *p1;
	c2 = *p2;
	if (c1 == c2) {
		const unsigned char *last = p1 + n - 1;

		while (p1 != last) {
			c1 = *++p1;
			c2 = *++p2;
			if (c1 != c2)
				goto differ;
		}
		return 0;
	}
differ:
	if (c1 < c2)
		return -1;
	return 1;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	const char *p;

	if (s == NULL)
		return 0;
	if (*s != 0) {
		p = s + 1;
		do {
			len = p - s;
		} while (*p++ != 0);
	}
	return len;
}

/* the lowercase of a mode character (the library's ctype table is the C locale's) */
static int toLower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

FILE *fopen(const char *path, const char *mode)
{
	Ft2File *f;
	const char *p;
	unsigned int c;
	int i, access, flags, openFlags;
	SceInt32 whence;
	SceUInt8 param[0x20];

	if (mode == NULL || path == NULL)
		return NULL;
	sceKernelLockLwMutex(&g_ioMutex, 1, NULL);
	for (i = 0; g_files[i].mode != 0; i++) {
		if (i + 1 == FT2_NUM_FILES)
			goto fail;
	}
	if (*mode == 0)
		goto fail;
	p = mode;
	do {
		c = (unsigned char)*p++;
	} while (toLower(c) == 'b');
	p--;
	switch (toLower(c)) {
	case 'a':
		access = 2;
		flags = 0x6000;
		openFlags = 768;
		break;
	case 'r':
		access = 1;
		flags = 0x8000;
		openFlags = 0;
		break;
	case 'w':
		access = 2;
		flags = 0x4000;
		openFlags = 1536;
		break;
	default:
		goto fail;
	}
	do {
		c = (unsigned char)*++p;
	} while (toLower(c) == 'b');
	if (c == '+') {
		flags |= 0xC000;
		openFlags |= 3;
	} else if (access == 2) {
		openFlags |= 2;
	} else {
		openFlags |= 1;
	}
	f = &g_files[i];
	f->mode = flags | 1;
	sceKernelUnlockLwMutex(&g_ioMutex, 1);
	f->state = 0;
	f->fd = sceIoOpen(path, openFlags, 0606);
	f->pushback = -1;
	whence = (f->mode & 0x2000) != 0 ? SCE_SEEK_END : SCE_SEEK_SET;
	if (f->mode != 0 && f->fd >= 0) {
		int pos;

		f->pushback = -1;
		pos = sceIoLseek(f->fd, 0, whence);
		if (pos >= 0) {
			f->position = pos;
			f->state &= ~2;
		}
	}
	if (f->fd < 0) {
		f->mode = 0;
		return NULL;
	}
	if (path[0] == 's' && path[1] == 'a' && path[2] == '0' && path[3] == ':')
		f->deviceType = 0x800;
	else
		f->deviceType = 0x8000;
	memset(param, 0, sizeof(param));
	*(SceUInt32 *)&param[0xc] = f->deviceType;
	*(SceUInt32 *)&param[0x14] = 8;
	sceIoIoctl(f->fd, 0x1001, param, sizeof(param), NULL, 0);
	return (FILE *)f;
fail:
	sceKernelUnlockLwMutex(&g_ioMutex, 1);
	return NULL;
}

size_t fread(void *buf, size_t size, size_t count, FILE *stream)
{
	Ft2File *f = (Ft2File *)stream;
	unsigned char *dst = buf;
	int remaining, done, n;

	if (f == NULL || f->mode == 0)
		return 0;
	if ((f->mode & 0x8000) == 0) {
		f->state |= 1;
		return 0;
	}
	if (f->fd < 0 || f->state != 0)
		return 0;
	remaining = size * count;
	if (remaining == 0)
		return 0;
	if (f->pushback == -1) {
		done = f->state;
	} else {
		*dst = f->pushback;
		remaining--;
		f->pushback = -1;
		if (remaining == 0)
			return 1 / size;
		if (f->state != 0)
			return 1 / size;
		dst++;
		done = 1;
	}
	while (remaining > 0) {
		int pos;

		n = sceIoRead(f->fd, dst, remaining);
		remaining -= n;
		pos = sceIoLseek(f->fd, 0, done);
		dst += n;
		if (pos >= 0)
			f->position = pos;
		if (n < 0) {
			f->state |= 1;
			return 0;
		}
		if (n == 0) {
			f->state |= 2;
			return done / size;
		}
		done += n;
		if (f->state != 0)
			break;
	}
	return done / size;
}

int fseek(FILE *stream, long offset, int whence)
{
	Ft2File *f = (Ft2File *)stream;
	int pos;

	if (f == NULL || f->mode == 0 || f->fd < 0)
		return -1;
	f->pushback = -1;
	pos = sceIoLseek(f->fd, (SceOff)offset, SCE_SEEK_SET);
	if (pos < 0)
		return -1;
	f->position = pos;
	f->state &= ~2;
	return 0;
}

int fclose(FILE *stream)
{
	Ft2File *f = (Ft2File *)stream;
	int ret;

	if (f == NULL || f->mode == 0)
		return -1;
	if ((unsigned char)f->mode == 2)
		return 0;
	if (f->fd < 0) {
		ret = -1;
	} else {
		sceIoClose(f->fd);
		f->fd = -1;
		ret = 0;
	}
	f->mode = 0;
	return ret;
}

long ftell(FILE *stream)
{
	Ft2File *f = (Ft2File *)stream;

	if (f == NULL || f->mode == 0 || f->fd < 0)
		return -1;
	if (f->pushback != -1) {
		if (f->position <= 0)
			return 0;
		return f->position - 1;
	}
	return f->position;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char *p1 = (const unsigned char *)s1, *p2 = (const unsigned char *)s2;
	int c1, c2;
	int isNull = s1 == NULL;

	if (s1 != NULL && s2 != NULL) {
		p1--;
		while ((int)--n >= 0) {
			c1 = *++p1;
			c2 = *p2++;
			if (c2 != c1)
				return c1 - c2;
			if (c1 == 0)
				return c1;
		}
		return 0;
	}
	if (s1 == s2)
		return 0;
	if (isNull)
		return -1;
	return 1;
}

int strcmp(const char *s1, const char *s2)
{
	const unsigned char *p1 = (const unsigned char *)s1;
	int c1, c2;
	int isNull = s1 == NULL;

	if (s1 == NULL || s2 == NULL) {
		if (s1 == s2)
			return 0;
		if (isNull)
			return -1;
		return 1;
	}
	p1--;
	for (;;) {
		c1 = *++p1;
		c2 = *(const unsigned char *)s2++;
		if (c2 != c1)
			break;
		if (c1 == 0)
			return c1;
	}
	return c1 - c2;
}

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	const unsigned char *end = p + n;

	if ((int)n <= 0)
		return NULL;
	for (; p != end; p++) {
		if (*p == (unsigned char)c)
			return (void *)p;
	}
	return NULL;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	size_t i;

	for (i = 0; i < n; i++)
		d[i] = s[i];
	return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	int distance;
	int i;

	if (d == s)
		return dst;
	distance = s - d;
	if (distance < 0)
		distance = -distance;
	if ((int)n > distance) {
		if (d >= s) {
			for (i = n - 1; i >= 0; i--)
				d[i] = s[i];
		} else {
			for (i = 0; i < (int)n; i++)
				d[i] = s[i];
		}
	} else {
		for (i = 0; i < (int)n; i++)
			d[i] = s[i];
	}
	return dst;
}
