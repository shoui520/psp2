/*
 * SceLibDbg: the assertion and logging handlers behind the SDK's
 * SCE_DBG_ASSERT / SCE_DBG_LOG_* macros, and their break settings.
 *
 * A handler builds one line of at most 511 characters in a stack buffer
 * ("SCE libdbg LOG, LEVEL: warning, COMPONENT: x, FILE:f, LINE:n message")
 * and prints it; a line that does not fit is printed cut short.
 */
#include <stdarg.h>
#include <psp2/types.h>

#define DBG_LINE_SIZE		0x200

#define SCE_DBG_LOG_LEVEL_WARNING	3
#define SCE_DBG_LOG_LEVEL_ERROR		4

extern void *sceClibMemset(void *dst, int c, SceSize len);
extern int sceClibSnprintf(char *dst, SceSize len, const char *fmt, ...);
extern int sceClibVsnprintf(char *dst, SceSize len, const char *fmt, va_list ap);
extern int sceClibPrintf(const char *fmt, ...);

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

static int s_minimumLogLevel;
static int s_breakOnWarning;
static int s_breakOnError;

/* print a break message without its newline, then stop in the debugger */
static inline void breakWithMessage(char *message, SceSize size)
{
	message[size - 2] = '\0';
	sceClibPrintf("%s\n", message);
	__asm__ volatile("bkpt #0");
}

int sceDbgAssertionHandler(const char *file, int line, int stop, const char *component,
			   const char *fmt, ...)
{
	char buf[DBG_LINE_SIZE];
	va_list ap;
	int len, n;

	sceClibMemset(buf, 0, sizeof(buf));
	len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg ASSERT");
	if (len >= (int)sizeof(buf))
		len = 0;
	if (component != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", COMPONENT: %s", component);
		if (n >= (int)sizeof(buf) - len)
			goto print;
		len += n;
	}
	if (file != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", FILE:%s, LINE:%d", file, line);
		if (n >= (int)sizeof(buf) - len)
			goto print;
		len += n;
	}
	if (fmt != NULL) {
		if (len < (int)sizeof(buf))
			buf[len++] = '\n';
		va_start(ap, fmt);
		sceClibVsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
		va_end(ap);
	}
print:
	buf[sizeof(buf) - 1] = '\0';
	sceClibPrintf("%s", buf);
	return stop;
}

int sceDbgSetBreakOnErrorState(int state)
{
	s_breakOnError = state;
	return 0;
}

int sceDbgSetBreakOnWarningState(int state)
{
	s_breakOnWarning = state;
	return 0;
}

int sceDbgSetMinimumLogLevel(int level)
{
	s_minimumLogLevel = level;
	return 0;
}

/* returns -1 when the line had to be cut short */
int sceDbgLoggingHandler(const char *file, int line, int level, const char *component,
			 const char *fmt, ...)
{
	char buf[DBG_LINE_SIZE];
	va_list ap;
	int len, n, ret;

	if (level < s_minimumLogLevel) {
		ret = 0;
		goto check_break;
	}
	sceClibMemset(buf, 0, sizeof(buf));
	if ((unsigned int)level <= SCE_DBG_LOG_LEVEL_ERROR)
		len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg LOG, LEVEL: %s",
				      s_levelNames[level]);
	else
		len = sceClibSnprintf(buf, sizeof(buf), "SCE libdbg LOG, LEVEL: %d", level);
	if (len >= (int)sizeof(buf))
		len = 0;
	if (component != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", COMPONENT: %s", component);
		if (n >= (int)sizeof(buf) - len)
			goto cut_short;
		len += n;
	}
	if (file != NULL) {
		n = sceClibSnprintf(buf + len, sizeof(buf) - len, ", FILE:%s, LINE:%d", file, line);
		if (n >= (int)sizeof(buf) - len)
			goto cut_short;
		len += n;
	}
	if (fmt != NULL) {
		if (len < (int)sizeof(buf))
			buf[len++] = ' ';
		va_start(ap, fmt);
		n = sceClibVsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
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
	if (s_breakOnWarning == 1 && level == SCE_DBG_LOG_LEVEL_WARNING)
		breakWithMessage(s_breakOnWarningMessage, sizeof(s_breakOnWarningMessage));
	else if (s_breakOnError == 1 && level > SCE_DBG_LOG_LEVEL_WARNING)
		breakWithMessage(s_breakOnErrorMessage, sizeof(s_breakOnErrorMessage));
	return ret;
}
