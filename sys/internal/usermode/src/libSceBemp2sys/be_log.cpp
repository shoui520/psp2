#include "be_mp2sys_internal.h"

BeLogFunc g_logFunc = 0;
int32_t g_logLevel = 0;
static char g_logBuf[1024];

int beLogPrintf(int level, const char *fmt, ...)
{
	va_list ap;

	if (g_logFunc == 0 || g_logLevel <= 0 || g_logLevel > 3 || g_logLevel > level)
		return 0;

	va_start(ap, fmt);
	beVsnprintf(g_logBuf, sizeof(g_logBuf), fmt, ap);
	va_end(ap);

	return g_logFunc(g_logBuf);
}
