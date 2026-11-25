/*
 * SceClib wrapper helpers for SceNgsUser.
 */
#include "ngs_internal.h"

void *ngsMemcpy(void *dst, const void *src, SceSize len)
{
	return sceClibMemcpy_safe(dst, src, len);
}

void *ngsMemcpyChecked(void *dst, SceSize dstSize, const void *src, SceSize len)
{
	if (dstSize < len)
		return NULL;
	return sceClibMemcpy_safe(dst, src, len);
}

void *ngsMemset(void *dst, int ch, SceSize len)
{
	return sceClibMemset(dst, ch, len);
}

int ngsVsnprintf(char *buf, SceSize size, const char *fmt, va_list ap)
{
	return sceClibVsnprintf(buf, size, fmt, ap);
}

char *ngsStrncpy(char *dst, const char *src, SceSize len)
{
	return sceClibStrncpy(dst, src, len);
}

SceSize ngsStrnlen(const char *s, SceSize maxLen)
{
	return sceClibStrnlen(s, maxLen);
}

int ngsMemcmp(const void *s1, const void *s2, SceSize len)
{
	return sceClibMemcmp(s1, s2, len);
}
