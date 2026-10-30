/*
 * Solaris console messages (Sony's dtrace/sce/solaris_debug.c), on the
 * kernel's console.
 */
#include <dtrace/sce/solaris_debug.h>

int vuprintf(const char *fmt, va_list ap)
{
	return SceDebugForDriver_411C0733(fmt, ap);
}

int uprintf(const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = SceDebugForDriver_411C0733(fmt, ap);
	va_end(ap);
	return res;
}
