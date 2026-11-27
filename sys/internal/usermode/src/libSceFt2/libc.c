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

#ifdef PAF_STATIC_BUILD
extern int SceLibKernel_A7819967(SceKernelLwMutexWork *work, int count, unsigned int *timeout);
extern int SceLibKernel_499EA781(SceKernelLwMutexWork *work, int count);
#define FT2_LOCK_LWMUTEX(m)	SceLibKernel_A7819967((m), 1, NULL)
#define FT2_UNLOCK_LWMUTEX(m)	SceLibKernel_499EA781((m), 1)
#else
#define FT2_LOCK_LWMUTEX(m)	sceKernelLockLwMutex((m), 1, NULL)
#define FT2_UNLOCK_LWMUTEX(m)	sceKernelUnlockLwMutex((m), 1)
#endif

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

void __sceLibFt2SetLibCHook(void)
{
	if (g_ioMutexCreated == 0) {
		g_ioMutexCreated = 1;
		sceKernelCreateLwMutex(&g_ioMutex, "SceLibFt2IoMutex", 0x2000, 0, NULL);
	}
}

int __sceLibFt2DoneLibCHook(void)
{
	return sceKernelDeleteLwMutex(&g_ioMutex);
}

#ifdef PAF_STATIC_BUILD
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *ft2_memset(void *dst, int ch, size_t len)
{
	unsigned char *out = dst;
	size_t i;

	for (i = 0; i < len; i++)
		out[i] = (unsigned char)ch;
	return dst;
}
#else
void *memset(void *dst, int ch, size_t len)
{
	sceClibMemset(dst, ch, len);
	return dst;
}
#endif

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

#ifdef PAF_STATIC_BUILD
char *strcpy(char *dst, const char *src)
{
	char *out;

	if (dst == NULL || src == NULL)
		return NULL;
	out = dst;
	while ((*out++ = *src++) != '\0')
		;
	return dst;
}

__attribute__((optimize("no-tree-loop-distribute-patterns")))
char *strncpy(char *dst, const char *src, size_t n)
{
	size_t i;

	if (dst == NULL || src == NULL)
		return NULL;
	for (i = 0; i < n && src[i] != '\0'; i++)
		dst[i] = src[i];
	for (; i < n; i++)
		dst[i] = '\0';
	return dst;
}
#endif

/* the C locale's character classes, indexed by the character plus one (index 0 is EOF) */
#define CT_U	0x01	/* upper case */
#define CT_L	0x02	/* lower case */
#define CT_N	0x04	/* digit */
#define CT_S	0x08	/* white space */
#define CT_P	0x10	/* punctuation */
#define CT_C	0x20	/* control */
#define CT_X	0x40	/* hexadecimal digit */

static const char g_ctype[1 + 128] = {
	[0] = 0,
	[1 ... 9] = CT_C,		/* NUL..BS */
	[10 ... 14] = CT_S,		/* HT, LF, VT, FF, CR */
	[15 ... 32] = CT_C,		/* the rest of the controls up to US */
	[33] = CT_S,			/* space */
	[34 ... 48] = CT_P,		/* ! .. / */
	[49 ... 58] = CT_N,		/* 0 .. 9 */
	[59 ... 65] = CT_P,		/* : .. @ */
	[66 ... 71] = CT_U | CT_X,	/* A .. F */
	[72 ... 91] = CT_U,		/* G .. Z */
	[92 ... 97] = CT_P,		/* [ .. ` */
	[98 ... 103] = CT_L | CT_X,	/* a .. f */
	[104 ... 123] = CT_L,		/* g .. z */
	[124 ... 127] = CT_P,		/* { .. ~ */
	[128] = CT_C,			/* DEL */
};

/* the lowercase of a mode character */
static int toLower(int c)
{
	if ((unsigned int)c <= 0x7F && (g_ctype[c + 1] & CT_U))
		return (unsigned char)c + ('a' - 'A');
	return c;
}

/* fseek without the stream check (fopen's own seek has a stream) */
static int seekFile(Ft2File *f, long offset, int whence)
{
	int pos;

	if (f->mode == 0 || f->fd < 0)
		return -1;
	f->pushback = -1;
	pos = sceIoLseek(f->fd, (SceOff)offset, whence);
	if (pos < 0)
		return -1;
	f->position = pos;
	f->state &= ~2;
	return 0;
}

FILE *fopen(const char *path, const char *mode)
{
	Ft2File *f;
	const char *p;
	char c;
	int i, access, flags, openFlags;
	SceUInt32 param[8];

	if (path == NULL || mode == NULL)
		return NULL;
	FT2_LOCK_LWMUTEX(&g_ioMutex);
	/* a free slot: Sony tests the low byte of its mode */
	for (i = 0; (SceUInt8)g_files[i].mode != 0; ) {
		if (++i == FT2_NUM_FILES)
			goto fail;
	}
	if (*mode == 0)
		goto fail;
	p = mode;
	do {
		c = *p++;
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
		c = *++p;
	} while (toLower(c) == 'b');
	if (c == '+') {
		flags |= 0xC000;
		openFlags |= 3;
	} else if (access == 2) {
		openFlags |= 2;
	} else {
		openFlags |= 1;
	}
	g_files[i].mode = flags | 1;
	f = &g_files[i];
	FT2_UNLOCK_LWMUTEX(&g_ioMutex);
	f->state = 0;
	f->fd = sceIoOpen(path, openFlags, 0606);
	f->pushback = -1;
	/* append opens at the end */
	seekFile(f, 0, (f->mode & 0x2000) ? SCE_SEEK_END : SCE_SEEK_SET);
	if (g_files[i].fd < 0) {
		g_files[i].mode = 0;
		return NULL;
	}
	f = &g_files[i];
	if (path[0] == 's' && path[1] == 'a' && path[2] == '0' && path[3] == ':')
		f->deviceType = 0x800;
	else
		f->deviceType = 0x8000;
	sceClibMemset(param, 0, sizeof(param));
	param[3] = f->deviceType;
	param[5] = 8;
	sceIoIoctl(f->fd, 0x1001, param, sizeof(param), NULL, 0);
	return (FILE *)f;
fail:
	FT2_UNLOCK_LWMUTEX(&g_ioMutex);
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
	remaining = (int)size * (int)count;
	if (remaining == 0)
		return 0;
	if (f->pushback == -1) {
		done = f->state;
	} else {
		*dst = f->pushback;
		remaining--;
		f->pushback = -1;
		if (remaining == 0)
			return 1 / (int)size;
		if (f->state != 0)
			return 1 / (int)size;
		dst++;
		done = 1;
	}
	while (remaining > 0) {
		int pos;

		n = sceIoRead(f->fd, dst, remaining);
		remaining -= n;
		pos = sceIoLseek(f->fd, 0, SCE_SEEK_CUR);
		dst += n;
		if (pos >= 0)
			f->position = pos;
		if (n < 0) {
			f->state |= 1;
			return 0;
		}
		if (n == 0) {
			f->state |= 2;
			return done / (int)size;
		}
		done += n;
		if (f->state != 0)
			break;
	}
	return done / (int)size;
}

int fseek(FILE *stream, long offset, int whence)
{
	if (stream == NULL)
		return -1;
	return seekFile((Ft2File *)stream, offset, whence);
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

char *strstr(const char *s1, const char *s2)
{
	const char *p1, *p2;

	if (*s2 == '\0')
		return (char *)s1;
	for (; *s1 != '\0'; s1++) {
		if (*s1 != *s2)
			continue;
		p1 = s1;
		p2 = s2;
		for (;;) {
			if (*++p2 == '\0')
				return (char *)s1;
			if (*++p1 == '\0')
				return NULL;
			if (*p1 != *p2)
				break;
		}
	}
	return NULL;
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

#ifdef PAF_STATIC_BUILD
void __stack_chk_fail(void)
{
	__asm__ volatile("bkpt 0");
}

void *ft2_memcpy(void *dst, const void *src, size_t n)
#else
void *memcpy(void *dst, const void *src, size_t n)
#endif
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
		size_t j;

		for (j = 0; j < n; j++)
			d[j] = s[j];
	}
	return dst;
}

/* the memory object FreeType's callers pass (FT_MemoryRec: user, alloc, free) */
typedef struct Ft2Memory {
	void *user;
	void *(*alloc)(struct Ft2Memory *pMemory, long size);
	void (*free)(struct Ft2Memory *pMemory, void *pBlock);
} Ft2Memory;

typedef int (*Ft2CompareFunc)(const void *pA, const void *pB);

static void copyBytes(char *pDst, const char *pSrc, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		pDst[i] = pSrc[i];
}

/* Hoare's partition of elements lo..hi of the array; the swap and pivot buffers come from the caller's allocator */
static void qsortRange(char *pBase, size_t size, Ft2CompareFunc compare, int lo, int hi, Ft2Memory *pMemory)
{
	char *pTemp = pMemory->alloc(pMemory, size);
	char *pPivot = pMemory->alloc(pMemory, size);
	char *pLow, *pHigh;
	int i = lo, j = hi;

	copyBytes(pPivot, pBase + size * ((lo + hi) / 2), size);
	pLow = pBase + size * i;
	for (;;) {
		if (compare(pLow, pPivot) < 0) {
			pLow += size;
			i++;
			continue;
		}
		pHigh = pBase + size * j;
		while (compare(pPivot, pHigh) < 0) {
			j--;
			pHigh -= size;
		}
		if (j < i)
			break;
		copyBytes(pTemp, pLow, size);
		copyBytes(pLow, pHigh, size);
		copyBytes(pHigh, pTemp, size);
		i++;
		j--;
		if (i > j)
			break;
		pLow += size;
	}
	if (lo < j)
		qsortRange(pBase, size, compare, lo, j, pMemory);
	if (i < hi)
		qsortRange(pBase, size, compare, i, hi, pMemory);
	pMemory->free(pMemory, pTemp);
	pMemory->free(pMemory, pPivot);
}

void ftQsort(void *pBase, int count, size_t size, Ft2CompareFunc compare, Ft2Memory *pMemory)
{
	if (compare != NULL && pBase != NULL && size != 0 && count != 0 && pMemory != NULL)
		qsortRange(pBase, size, compare, 0, count - 1, pMemory);
}
