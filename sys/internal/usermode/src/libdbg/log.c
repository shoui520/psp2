/*
 * SceLibDbg's logging handler, behind the SDK's SCE_DBG_LOG_* macros, and the settings it reads:
 * the minimum level and whether a warning or an error stops in the debugger. A line ("SCE libdbg
 * LOG, LEVEL: warning, COMPONENT: x, FILE:f, LINE:n message") is built like an assertion's (the
 * file name is ours).
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <scetypes.h>
#include <libdbg.h>
#include <libkernel/libkernel.h>

#define DBG_LINE_SIZE		0x200

static const char *const s_levelNames[] = {
	"trace",
	"debug",
	"info",
	"warning",
	"error",
	NULL,
};

static char s_breakOnErrorMessage[] =
	"SCE libdbg: Client requested break on error, breaking...\n";
static char s_breakOnWarningMessage[] =
	"SCE libdbg: Client requested break on warning, breaking...\n";

static SceInt32 s_minimumLogLevel;
static SceDbgBreakOnWarningState s_breakOnWarning;
static SceDbgBreakOnErrorState s_breakOnError;

/*
 * print a break message, terminated at its buffer's end like the line buffers (it keeps its
 * own newline, so a blank line follows), then stop in the debugger
 */
static inline void breakWithMessage(char *message, SceSize size)
{
	message[size - 1] = '\0';
	sceClibPrintf("%s\n", message);
	SCE_BREAK();
}

SceInt32 sceDbgSetBreakOnErrorState(SceDbgBreakOnErrorState state)
{
	s_breakOnError = state;
	return 0;
}

SceInt32 sceDbgSetBreakOnWarningState(SceDbgBreakOnWarningState state)
{
	s_breakOnWarning = state;
	return 0;
}

SceInt32 sceDbgSetMinimumLogLevel(SceInt32 minimumLogLevel)
{
	s_minimumLogLevel = minimumLogLevel;
	return 0;
}

/* returns -1 when the line had to be cut short */
SceInt32 sceDbgLoggingHandler(const char *pFile, int line, int severity, const char *pComponent,
			      const char *pMessage, ...)
{
	char buf[DBG_LINE_SIZE];
	va_list ap;
	int len, n, ret;

	if (severity < s_minimumLogLevel) {
		ret = 0;
		goto check_break;
	}
	sceClibMemset(buf, 0, sizeof(buf));
	if ((unsigned int)severity <= SCE_DBG_LOG_LEVEL_ERROR)
		len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg LOG, LEVEL: %s",
				      s_levelNames[severity]);
	else
		len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg LOG, LEVEL: %d", severity);
	if (len >= (int)sizeof(buf))
		len = 0;
	if (pComponent != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", COMPONENT: %s", pComponent);
		if (n >= (int)sizeof(buf) - len)
			goto cut_short;
		len += n;
	}
	if (pFile != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", FILE:%s, LINE:%d", pFile, line);
		if (n >= (int)sizeof(buf) - len)
			goto cut_short;
		len += n;
	}
	if (pMessage != NULL) {
		if (len < (int)sizeof(buf))
			buf[len++] = ' ';
		va_start(ap, pMessage);
		n = sceClibVsnprintf(buf + len, sizeof(buf) - len, pMessage, ap);
		va_end(ap);
		if (n > (int)sizeof(buf) - len)
			goto cut_short;
	}
	buf[sizeof(buf) - 1] = '\0';
	sceClibPrintf("%s\n", buf);
	ret = 0;
	goto check_break;
cut_short:
	buf[sizeof(buf) - 1] = '\0';
	ret = -1;
	sceClibPrintf("%s\n", buf);
check_break:
	if (s_breakOnWarning == SCE_DBG_ENABLE_BREAK_ON_WARNING &&
	    severity == SCE_DBG_LOG_LEVEL_WARNING)
		breakWithMessage(s_breakOnWarningMessage, sizeof(s_breakOnWarningMessage));
	else if (s_breakOnError == SCE_DBG_ENABLE_BREAK_ON_ERROR &&
		 severity > SCE_DBG_LOG_LEVEL_WARNING)
		breakWithMessage(s_breakOnErrorMessage, sizeof(s_breakOnErrorMessage));
	return ret;
}
