/*
 * libdbg: the SDK's assertion and logging handlers (SceDbg) and the macros a library's debug
 * build reports through. SCE_DBG_ASSERTS_ENABLED and SCE_DBG_LOGGING_ENABLED are the build's
 * switches; a library names itself with SCE_DBG_ASSERT_COMPONENT and SCE_DBG_LOG_COMPONENT.
 */

#ifndef _SCE_LIBDBG_H
#define _SCE_LIBDBG_H

#include <scetypes.h>
#include <stdbool.h>

#define _SCE_MACRO_BEGIN	do {
#define _SCE_MACRO_END		} while (0)

#ifndef SCE_BREAK
#define SCE_BREAK()		_SCE_MACRO_BEGIN { __asm__ volatile("bkpt 0x0000"); } _SCE_MACRO_END
#endif

#ifndef SCE_UNLIKELY
#define SCE_UNLIKELY(expr)	__builtin_expect(!!(expr), 0)
#endif

#ifndef SCE_DBG_ASSERTS_ENABLED
#define SCE_DBG_ASSERTS_ENABLED	0
#endif

#ifndef SCE_DBG_LOGGING_ENABLED
#define SCE_DBG_LOGGING_ENABLED	1
#endif

typedef enum SceDbgLogLevel {
	SCE_DBG_LOG_LEVEL_TRACE = 0,
	SCE_DBG_LOG_LEVEL_DEBUG,
	SCE_DBG_LOG_LEVEL_INFO,
	SCE_DBG_LOG_LEVEL_WARNING,
	SCE_DBG_LOG_LEVEL_ERROR,
	SCE_DBG_NUM_LOG_LEVELS
} SceDbgLogLevel;

typedef enum SceDbgBreakOnErrorState {
	SCE_DBG_DISABLE_BREAK_ON_ERROR = 0,
	SCE_DBG_ENABLE_BREAK_ON_ERROR
} SceDbgBreakOnErrorState;

typedef enum SceDbgBreakOnWarningState {
	SCE_DBG_DISABLE_BREAK_ON_WARNING = 0,
	SCE_DBG_ENABLE_BREAK_ON_WARNING
} SceDbgBreakOnWarningState;

#ifdef __cplusplus
extern "C" {
#endif

SceInt32 sceDbgSetMinimumLogLevel(SceInt32 minimumLogLevel);
SceInt32 sceDbgSetBreakOnErrorState(SceDbgBreakOnErrorState state);
SceInt32 sceDbgSetBreakOnWarningState(SceDbgBreakOnWarningState state);
SceInt32 sceDbgAssertionHandler(const char *pFile, int line, bool stop, const char *pComponent,
				const char *pMessage, ...) __attribute__((__format__(__printf__, 5, 6)));
SceInt32 sceDbgLoggingHandler(const char *pFile, int line, int severity, const char *pComponent,
			      const char *pMessage, ...) __attribute__((__format__(__printf__, 5, 6)));

#ifdef __cplusplus
}
#endif

#ifndef SCE_DBG_MINIMUM_LOG_LEVEL
#define SCE_DBG_MINIMUM_LOG_LEVEL	SCE_DBG_LOG_LEVEL_TRACE
#endif
#ifndef SCE_DBG_LOG_PREFIX
#define SCE_DBG_LOG_PREFIX		""
#endif
#ifndef SCE_DBG_LOG_COMPONENT
#define SCE_DBG_LOG_COMPONENT		""
#endif
#ifndef SCE_DBG_ASSERT_COMPONENT
#define SCE_DBG_ASSERT_COMPONENT	""
#endif

#define SCE_DBG_LOG_BASE(level, component, format, ...)						\
	_SCE_MACRO_BEGIN									\
		if (level >= SCE_DBG_MINIMUM_LOG_LEVEL) {					\
			sceDbgLoggingHandler(__FILE__, __LINE__, level, component,	\
					     SCE_DBG_LOG_PREFIX format, ##__VA_ARGS__);		\
		}										\
	_SCE_MACRO_END

#if SCE_DBG_LOGGING_ENABLED
#define SCE_DBG_LOG_TRACE(format, ...)	\
	SCE_DBG_LOG_BASE(SCE_DBG_LOG_LEVEL_TRACE, SCE_DBG_LOG_COMPONENT, format, ##__VA_ARGS__)
#define SCE_DBG_LOG_DEBUG(format, ...)	\
	SCE_DBG_LOG_BASE(SCE_DBG_LOG_LEVEL_DEBUG, SCE_DBG_LOG_COMPONENT, format, ##__VA_ARGS__)
#define SCE_DBG_LOG_INFO(format, ...)	\
	SCE_DBG_LOG_BASE(SCE_DBG_LOG_LEVEL_INFO, SCE_DBG_LOG_COMPONENT, format, ##__VA_ARGS__)
#define SCE_DBG_LOG_WARNING(format, ...)	\
	SCE_DBG_LOG_BASE(SCE_DBG_LOG_LEVEL_WARNING, SCE_DBG_LOG_COMPONENT, format, ##__VA_ARGS__)
#define SCE_DBG_LOG_ERROR(format, ...)	\
	SCE_DBG_LOG_BASE(SCE_DBG_LOG_LEVEL_ERROR, SCE_DBG_LOG_COMPONENT, format, ##__VA_ARGS__)
#else
#define SCE_DBG_LOG_TRACE(format, ...)
#define SCE_DBG_LOG_DEBUG(format, ...)
#define SCE_DBG_LOG_INFO(format, ...)
#define SCE_DBG_LOG_WARNING(format, ...)
#define SCE_DBG_LOG_ERROR(format, ...)
#endif

#define SCE_DBG_ASSERT_PRIVATE(test, stop, stopaction, format, ...)				\
	_SCE_MACRO_BEGIN									\
		if (SCE_UNLIKELY(!(test))) {							\
			if (stop && sceDbgAssertionHandler(__FILE__, __LINE__, stop,	\
					SCE_DBG_ASSERT_COMPONENT, format, ##__VA_ARGS__)) {	\
				stopaction;							\
			}									\
		}										\
	_SCE_MACRO_END

#define SCE_DBG_ALWAYS_ASSERT(test)	\
	SCE_DBG_ASSERT_PRIVATE(test, true, SCE_BREAK(), "Assertion failed: %s\n", #test)

#if SCE_DBG_ASSERTS_ENABLED
#define SCE_DBG_SIMPLE_ASSERT(test)	\
	_SCE_MACRO_BEGIN if (SCE_UNLIKELY(!(test))) { SCE_BREAK(); } _SCE_MACRO_END
#define SCE_DBG_ASSERT(test)	\
	SCE_DBG_ASSERT_PRIVATE(test, true, SCE_BREAK(), "Assertion failed: %s\n", #test)
#define SCE_DBG_ASSERT_MSG(test, msg, ...)	\
	SCE_DBG_ASSERT_PRIVATE(test, true, SCE_BREAK(), msg, ##__VA_ARGS__)
#define SCE_DBG_VERIFY(test)	\
	SCE_DBG_ASSERT_PRIVATE(test, true, SCE_BREAK(), "Assertion failed: %s\n", #test)
#else
#define SCE_DBG_SIMPLE_ASSERT(test)		_SCE_MACRO_BEGIN { (void)sizeof((test)); } _SCE_MACRO_END
#define SCE_DBG_ASSERT(test)			_SCE_MACRO_BEGIN { (void)sizeof((test)); } _SCE_MACRO_END
#define SCE_DBG_ASSERT_MSG(test, msg, ...)	_SCE_MACRO_BEGIN { (void)sizeof((test)); } _SCE_MACRO_END
#define SCE_DBG_VERIFY(test)			((void)(test))
#endif

#endif /* _SCE_LIBDBG_H */
