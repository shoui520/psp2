/*
 * SceLibDbg's assertion handler, behind the SDK's SCE_DBG_ASSERT macros: one line of at most 511
 * characters built in a stack buffer ("SCE libdbg ASSERT, COMPONENT: x, FILE:f, LINE:n" and the
 * message on the next line) and printed; a line that does not fit is printed cut short. Its own
 * translation unit, as Sony's string pool shows (the file name is ours).
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <scetypes.h>
#include <libdbg.h>
#include <libkernel/libkernel.h>

#define DBG_LINE_SIZE		0x200

SceInt32 sceDbgAssertionHandler(const char *pFile, int line, bool stop, const char *pComponent,
				const char *pMessage, ...)
{
	char buf[DBG_LINE_SIZE];
	va_list ap;
	int len, n;

	sceClibMemset(buf, 0, sizeof(buf));
	len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg ASSERT");
	if (len >= (int)sizeof(buf))
		len = 0;
	if (pComponent != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", COMPONENT: %s", pComponent);
		if (n >= (int)sizeof(buf) - len)
			goto print;
		len += n;
	}
	if (pFile != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", FILE:%s, LINE:%d", pFile, line);
		if (n >= (int)sizeof(buf) - len)
			goto print;
		len += n;
	}
	if (pMessage != NULL) {
		if (len < (int)sizeof(buf))
			buf[len++] = '\n';
		va_start(ap, pMessage);
		sceClibVsnprintf(buf + len, sizeof(buf) - len, pMessage, ap);
		va_end(ap);
	}
print:
	buf[sizeof(buf) - 1] = '\0';
	sceClibPrintf("%s", buf);
	return stop;
}
