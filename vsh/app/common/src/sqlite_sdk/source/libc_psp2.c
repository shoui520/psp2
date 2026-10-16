/*
 * SceSqlite: the C library SQLite and the OS layer need, on SceLibc's clib
 * calls, and the allocator hooks set by sceSqliteConfigMallocMethods.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <sce/sqlite3_util.h>

#define STRLEN_MAX	0x800		/* longest string strlen measures */

/* SceLibKernel clib export missing from the vitasdk headers */
size_t sceClibStrlcpy(char *dst, const char *src, size_t size);

/* BSD ctype classes */
#define _U	0x01			/* upper case */
#define _L	0x02			/* lower case */
#define _N	0x04			/* digit */
#define _S	0x08			/* white space */
#define _P	0x10			/* punctuation */
#define _C	0x20			/* control */
#define _X	0x40			/* hex digit letter */
#define _B	0x80			/* blank */

/* ASCII classes, indexed by c + 1 (EOF is entry 0); this table classes
 * the white space controls as space only and the space as punctuation too */
static const char psp2Ctype[1 + 128] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_S,	_S,	_S,	_S,	_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_S|_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N,
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C,
};

#define CTYPE(c)	((psp2Ctype + 1)[(int)(char)(c)])

/* the allocator hooks (three separate variables in Sony's data; names unknown) */
static void *(*g_fMalloc)(size_t);
static void *(*g_fRealloc)(void *, size_t);
static void (*g_fFree)(void *);

void *memset(void *dst, int c, size_t n)
{
	return sceClibMemset(dst, c, n);
}

void *memcpy(void *dst, const void *src, size_t n)
{
	return sceClibMemcpy_safe(dst, src, n);
}

int memcmp(const void *a, const void *b, size_t n)
{
	return sceClibMemcmp(a, b, n);
}

void *memmove(void *dst, const void *src, size_t n)
{
	return sceClibMemmove(dst, src, n);
}

size_t strlen(const char *s)
{
	return sceClibStrnlen(s, STRLEN_MAX);
}

int strncmp(const char *a, const char *b, size_t n)
{
	return sceClibStrncmp(a, b, n);
}

int strcmp(const char *a, const char *b)
{
	return sceClibStrcmp(a, b);
}

size_t strlcpy(char *dst, const char *src, size_t size)
{
	return sceClibStrlcpy(dst, src, size);
}

int printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = sceClibVprintf(fmt, ap);
	va_end(ap);
	return ret;
}

void *malloc(size_t size)
{
	if (g_fMalloc != NULL)
		return g_fMalloc(size);
	printf("WARNING: malloc() is unavailable.\n");
	return NULL;
}

void *realloc(void *ptr, size_t size)
{
	if (g_fRealloc != NULL)
		return g_fRealloc(ptr, size);
	printf("WARNING: realloc() is unavailable.\n");
	return NULL;
}

void free(void *ptr)
{
	if (g_fFree != NULL)
		g_fFree(ptr);
	else
		printf("WARNING: free() is unavailable.\n");
}

/* set once, before SQLite allocates anything */
int sceSqliteConfigMallocMethods(SceSqliteMallocMethods *mf)
{
	if (mf == NULL) {
		printf("ERROR %s: argument is NULL.\n", __func__);
		return -1;
	}
	if (g_fMalloc != NULL) {
		printf("ERROR %s: functions are already set.\n", __func__);
		return -1;
	}
	g_fMalloc = mf->fMalloc;
	g_fRealloc = mf->fRealloc;
	g_fFree = mf->fFree;
	return 0;
}

/*
 * Decimal conversion.  A letter counts as a digit of value 10 and up, and
 * only values above 10 end the number, so "1a" reads as 20.
 */
#undef atoi		/* SNC's stdlib.h maps it to a builtin */
int atoi(const char *s)
{
	int n = 0;
	int sign = 1;
	int d, lower;
	char c;

	if (s == NULL)
		return n;
	while (CTYPE(*s) & _S)
		s++;
	while (*s == '-') {
		sign = -sign;
		s++;
	}
	for (;;) {
		c = *s++;
		if (CTYPE(c) & _N)
			d = c - '0';
		else if (CTYPE(c) & (_U | _L)) {
			lower = c;
			if (CTYPE(c) & _U)
				lower = (unsigned char)c + ('a' - 'A');
			d = lower - 'a' + 10;
		}
		else
			d = 9999999;
		if (d > 10)
			break;
		n = n * 10 + d;
	}
	return n * sign;
}

