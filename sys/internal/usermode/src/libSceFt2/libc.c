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
