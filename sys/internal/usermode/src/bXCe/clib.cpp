/*
 * ScebXCe's C library: SceLibKernel's sceClib functions behind the library's own names.
 */
#include "bxce_internal.h"
#include <libkernel/libkernel.h>

void *bxceMemset(void *dst, int c, SceSize len)
{
	return sceClibMemset(dst, c, len);
}

void *bxceMemcpy(void *dst, const void *src, SceSize len)
{
	return sceClibMemcpy_safe(dst, src, len);
}

int bxceMemcmp(const void *s1, const void *s2, SceSize len)
{
	return sceClibMemcmp(s1, s2, len);
}

void *bxceMemmove(void *dst, const void *src, SceSize len)
{
	return sceClibMemmove(dst, src, len);
}

SceSize bxceStrnlen(const char *s, SceSize maxlen)
{
	return sceClibStrnlen(s, maxlen);
}

int bxceStrncmp(const char *s1, const char *s2, SceSize len)
{
	return sceClibStrncmp(s1, s2, len);
}

int bxceStrncmpTag(const char *s1, const char *s2, SceSize len)
{
	return sceClibStrncmp(s1, s2, len);
}

SceSize bxceStrlcpy(char *dst, const char *src, SceSize size)
{
	return sceClibStrlcpy(dst, src, size);
}

int bxceSprintf(char *dst, const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = sceClibVsnprintf(dst, 0x100000, format, args);
	va_end(args);
	return ret;
}
