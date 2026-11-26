#include "be_mp2sys_internal.h"

void *beMemcpy(void *dst, const void *src, SceSize size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

void *beMemset(void *dst, int value, SceSize size)
{
	return sceClibMemset(dst, value, size);
}

int beMemcmp(const void *lhs, const void *rhs, SceSize size)
{
	return sceClibMemcmp(lhs, rhs, size);
}

int beVsnprintf(char *buf, SceSize size, const char *fmt, va_list ap)
{
	return sceClibVsnprintf(buf, size, fmt, ap);
}
