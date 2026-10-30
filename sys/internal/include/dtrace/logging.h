/*
 * DTrace's kernel log (Sony's dtrace/logging.h). Errors, and debug messages
 * up to DTRACE_DEBUG_LEVEL, go to the kernel console with the file and line
 * they come from; only the debug build (4.00) keeps them.
 */
#pragma once

#include <fw.h>
#include <sysmem/debug.h>

#define DTRACE_LOG_PREFIX	"[dtrace] "

#if FW_DEBUG

#ifndef DTRACE_DEBUG_LEVEL
#define DTRACE_DEBUG_LEVEL	0
#endif

#define DTRACE_LOG_AT(_line, _kind, _fmt, ...) {				\
	SceDebugForDriver_391B74B7(DTRACE_LOG_PREFIX _kind _fmt " [%s:%d]\n",	\
				   ##__VA_ARGS__, __FILE_NAME__, (_line));	\
}

#define DTRACE_LOG_ERROR_AT(_line, _fmt, ...)					\
	DTRACE_LOG_AT(_line, "ERROR:   ", _fmt, ##__VA_ARGS__)

#define DTRACE_LOG_DEBUG_AT(_line, _level, _fmt, ...) {			\
	if ((_level) <= DTRACE_DEBUG_LEVEL)					\
		DTRACE_LOG_AT(_line, "DEBUG:   ", _fmt, ##__VA_ARGS__)	\
}

#else

#define DTRACE_LOG_ERROR_AT(_line, _fmt, ...)		{ }
#define DTRACE_LOG_DEBUG_AT(_line, _level, _fmt, ...)	{ }

#endif

#define DTRACE_LOG_ERROR(_fmt, ...)	DTRACE_LOG_ERROR_AT(__LINE__, _fmt, ##__VA_ARGS__)
#define DTRACE_LOG_DEBUG(_level, _fmt, ...)					\
	DTRACE_LOG_DEBUG_AT(__LINE__, _level, _fmt, ##__VA_ARGS__)

/*
 * Each is a block, as Sony's are: the sources call some without a semicolon.
 * Warnings, call traces and the stub and measurement reports are never built in */
#define DTRACE_LOG_WARNING(_fmt, ...)		{ }
#define DTRACE_LOG_STUB_WARNING(_fmt, ...)	{ }
#define DTRACE_LOG_CALL(_level)			{ }
#define DTRACE_LOG_HEADER(_level, _fmt, ...)	{ }
#define DTRACE_LOG_SEPARATOR(_level)		{ }
#define DTRACE_PRINT(_level, _fmt, ...)		{ }
