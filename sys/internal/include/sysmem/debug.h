/*
 * Kernel debug output as Sony's debug builds emit it.
 *
 * Every Assert/PrintfWithInfo call site passes a SceKernelDebugInfo record:
 *
 *	fileHash  first 4 bytes (big endian) of SHA-1("file.c")
 *	lineHash  SHA-1("file.c:line")
 *	funcHash  SHA-1("function")
 *	file, line, func
 *
 * The line is Sony's original source line, given explicitly at each site. The
 * hashes are filled in after linking by tools/dbginfo.py from the .sce_dbgsrc
 * side table. Release firmwares keep only the hashes (file/line/func are 0) and
 * only for calls that remain, e.g. ksceKernelPanic. (vitasdk's SceKernelDebugInfo puts
 * func before file; the records in Sony's modules are file, line, func.)
 *
 * Release firmwares (FW_DEBUG == 0) compile all of it out.
 */

#pragma once

#include <psp2kern/types.h>
#include <stdarg.h>

typedef struct SceKernelDebugInfo {
	SceUInt32 fileHash;
	SceUInt32 lineHash;
	SceUInt32 funcHash;
	const char *file;
	int line;
	const char *func;
} SceKernelDebugInfo;

int ksceKernelPrintfLevelWithInfo(SceUInt32 level, SceUInt32 flags,
				  const SceKernelDebugInfo *info, const char *fmt, ...);
int ksceKernelGetAssertLevel(void) __attribute__((pure));	/* Sony's: consecutive checks share one call */
void ksceKernelAssertLevel(SceUInt32 level, SceBool cond,
			   const SceKernelDebugInfo *info, const void *lr);

/* where a record's hashes come from; non-allocated, read by tools/dbginfo.py */
typedef struct SceKernelDebugInfoSource {
	const SceKernelDebugInfo *info;
	const char *file;
	int line;
	const char *func;
} SceKernelDebugInfoSource;

#if FW_DEBUG && defined(__cplusplus)
/* g++ 4.8 takes no designators that skip fields */
#define SCE_DBGINFO_FIELDS(_line, _func)	0, 0, 0, __FILE_NAME__, (_line), (_func),
#elif FW_DEBUG
#define SCE_DBGINFO_FIELDS(_line, _func)	.file = __FILE_NAME__, .line = (_line), .func = (_func),
#else
#define SCE_DBGINFO_FIELDS(_line, _func)	/* release: hashes only */
#endif

/* section flags "" (non-alloc): the rest of the directive is an ARM comment */
#define SCE_DBGSRC_SECTION	".sce_dbgsrc,\"\",%progbits @"

#ifndef SCE_DBGINFO_SECTION
#define SCE_DBGINFO_SECTION ".sce_dbginfo"
#endif

/* Named records allow a module to retain its original readonly-data order. */
#define SCE_DBGINFO_DEFINE(_storage, _name, _line, _func)		\
	_storage const SceKernelDebugInfo _name			\
	__attribute__((section(SCE_DBGINFO_SECTION), used)) = {	\
		SCE_DBGINFO_FIELDS(_line, _func)			\
	};								\
	static const SceKernelDebugInfoSource _name##Source		\
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {		\
		&_name, __FILE_NAME__, (_line), (_func),			\
	}

/* _func: Sony's source function, including calls from an inlined helper. */
#define SCE_DBGINFO_LOCAL(_line, _func) ({				\
	SCE_DBGINFO_DEFINE(static, __dbginfo, _line, _func);		\
	(void)__dbginfoSource;						\
	&__dbginfo;							\
})

#define SCE_DBGINFO_FUNC(_line, _func) SCE_DBGINFO_LOCAL(_line, _func)

#define SCE_DBGINFO(_line)	SCE_DBGINFO_FUNC(_line, __func__)

void ksceKernelPanic(const SceKernelDebugInfo *info, const void *lr) __attribute__((noreturn));
void ksceKernelAssert(SceBool cond, const SceKernelDebugInfo *info, const void *lr);
void ksceKernelPrintfPanic(const SceKernelDebugInfo *info, const void *lr, const char *fmt, ...)
	__attribute__((noreturn));

int ksceKernelPrintf(const char *fmt, ...);
int ksceKernelRegisterKprintfHandler(int (*handler)(int unk, const char *fmt, va_list ap),
				     void *argp);
int ksceKernelPrintfWithInfo(SceUInt32 level, const SceKernelDebugInfo *info, const char *fmt, ...);
/*
 * ksceKernelPrintfAssertLevel is not declared here: modules declare it
 * themselves (with differing return types); users of SCE_DBG_ASSERT_PRINTF
 * need a declaration in scope.
 */

/*
 * A line of Sony's source that moved between the 3.6x sources (3.63, 3.65)
 * and the 4.00 ones; it matters for the records release builds keep too.
 */
#define SCE_LINE(_line36x, _line400)	(FW_VERSION >= 0x04000000 ? (_line400) : (_line36x))

/* ksceKernelPrintfWithInfo(level, ...): compiled in on every firmware */
#define SCE_PRINTF_WITH_INFO(_level, _line, _fmt, ...)			\
	ksceKernelPrintfWithInfo((_level), SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)

/* the same in a function Sony named differently */
#define SCE_PRINTF_WITH_INFO_FUNC(_level, _line, _func, _fmt, ...)	\
	ksceKernelPrintfWithInfo((_level), SCE_DBGINFO_FUNC(_line, _func), _fmt, ##__VA_ARGS__)

/* kernel panic: compiled in on every firmware */
#define SCE_PANIC(_line)						\
	ksceKernelPanic(SCE_DBGINFO(_line), __builtin_return_address(0))

/* the same in a helper inlined into another function, under the helper's name */
#define SCE_PANIC_FUNC(_line, _func)					\
	ksceKernelPanic(SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0))

/* kernel panic with a message */
#define SCE_PANIC_PRINTF(_line, _fmt, ...)				\
	ksceKernelPrintfPanic(SCE_DBGINFO(_line), __builtin_return_address(0),	\
			      _fmt, ##__VA_ARGS__)

/* the same in a helper inlined into another function, under the helper's name */
#define SCE_PANIC_PRINTF_FUNC(_line, _func, _fmt, ...)			\
	ksceKernelPrintfPanic(SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0), \
			      _fmt, ##__VA_ARGS__)

#if FW_DEBUG

/* ksceKernelPrintfWithInfo(level, ...), compiled in on debug firmwares only */
#define SCE_DBG_PRINTF_WITH_INFO(_level, _line, _fmt, ...)		\
	ksceKernelPrintfWithInfo((_level), SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)

/* ksceKernelPrintfLevelWithInfo(1, flags, ...) */
#define SCE_DBG_PRINTF(_flags, _line, _fmt, ...)			\
	ksceKernelPrintfLevelWithInfo(1, (_flags), SCE_DBGINFO(_line),	\
				      _fmt, ##__VA_ARGS__)

/* ksceKernelPrintfLevelWithInfo(level, flags, ...) */
#define SCE_DBG_PRINTF_LEVEL(_level, _flags, _line, _fmt, ...)		\
	ksceKernelPrintfLevelWithInfo((_level), (_flags), SCE_DBGINFO(_line),	\
				      _fmt, ##__VA_ARGS__)

/* ksceKernelAssertLevel(level, cond, info, lr) */
#define SCE_DBG_ASSERT_LEVEL(_level, _cond, _line)			\
	ksceKernelAssertLevel((_level), (_cond), SCE_DBGINFO(_line),	\
			      __builtin_return_address(0))

/* unconditional assertion failure (the condition is already known false) */
/* ksceKernelAssert(cond, info, lr) */
#define SCE_DBG_ASSERT(_cond, _line)					\
	ksceKernelAssert((_cond), SCE_DBGINFO(_line), __builtin_return_address(0))

/* level 1 assertion, checked only while assertions are enabled */
#define SCE_DBG_ASSERT_CHECK(_cond, _line) do {				\
	if (ksceKernelGetAssertLevel() > 0)				\
		ksceKernelAssertLevel(1, (_cond), SCE_DBGINFO(_line),	\
				      __builtin_return_address(0));	\
} while (0)

#define SCE_DBG_ASSERT_FAIL(_line)	SCE_DBG_ASSERT_CHECK(0, _line)

/* the same, reporting a return address the caller passed down */
#define SCE_DBG_ASSERT_CHECK_LR(_cond, _line, _lr) do {			\
	if (ksceKernelGetAssertLevel() > 0)				\
		ksceKernelAssertLevel(1, (_cond), SCE_DBGINFO(_line), (_lr));	\
} while (0)

/* level _level assertion, checked only while assertions of that level are enabled */
#define SCE_DBG_ASSERT_LEVEL_CHECK(_level, _cond, _line) do {		\
	if (ksceKernelGetAssertLevel() >= (_level))			\
		ksceKernelAssertLevel((_level), (_cond), SCE_DBGINFO(_line),	\
				      __builtin_return_address(0));	\
} while (0)

/* level 1 assertion with a message, checked only while assertions are enabled */
#define SCE_DBG_ASSERT_PRINTF(_cond, _line, _fmt, ...) do {		\
	if (ksceKernelGetAssertLevel() > 0)				\
		ksceKernelPrintfAssertLevel(1, (_cond), SCE_DBGINFO(_line),	\
					    __builtin_return_address(0), _fmt,	\
					    ##__VA_ARGS__);			\
} while (0)

/* ksceKernelPrintfAssertLevel(level, cond, ...), checked whatever the assert level */
#define SCE_DBG_ASSERT_PRINTF_LEVEL(_level, _cond, _line, _fmt, ...)		\
	ksceKernelPrintfAssertLevel((_level), (_cond), SCE_DBGINFO(_line),	\
				    __builtin_return_address(0), _fmt, ##__VA_ARGS__)

/* the same in a helper inlined into _func, sharing one record over all its copies */
#define SCE_DBG_PRINTF_FUNC(_flags, _line, _func, _fmt, ...)		\
	ksceKernelPrintfLevelWithInfo(1, (_flags), SCE_DBGINFO_FUNC(_line, _func),	\
				      _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_FUNC(_cond, _line, _func)			\
	ksceKernelAssert((_cond), SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0))
#define SCE_DBG_PRINTF_LEVEL_FUNC(_level, _flags, _line, _func, _fmt, ...)	\
	ksceKernelPrintfLevelWithInfo((_level), (_flags), SCE_DBGINFO_FUNC(_line, _func),	\
				      _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_PRINTF_FUNC(_cond, _line, _func, _fmt, ...) do {	\
	if (ksceKernelGetAssertLevel() > 0)				\
		ksceKernelPrintfAssertLevel(1, (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
					    __builtin_return_address(0), _fmt,	\
					    ##__VA_ARGS__);			\
} while (0)
#define SCE_DBG_PRINTF_WITH_INFO_FUNC(_level, _line, _func, _fmt, ...)	\
	ksceKernelPrintfWithInfo((_level), SCE_DBGINFO_FUNC(_line, _func),	\
				 _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_LEVEL_FUNC(_level, _cond, _line, _func)		\
	ksceKernelAssertLevel((_level), (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
			      __builtin_return_address(0))
#define SCE_DBG_ASSERT_CHECK_FUNC(_cond, _line, _func) do {		\
	if (ksceKernelGetAssertLevel() > 0)				\
		ksceKernelAssertLevel(1, (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
				      __builtin_return_address(0));	\
} while (0)

#else

#define SCE_DBG_PRINTF_WITH_INFO(_level, _line, _fmt, ...)	do { } while (0)
#define SCE_DBG_PRINTF(_flags, _line, _fmt, ...)	do { } while (0)
#define SCE_DBG_PRINTF_LEVEL(_level, _flags, _line, _fmt, ...)	do { } while (0)
#define SCE_DBG_ASSERT_LEVEL(_level, _cond, _line)		do { } while (0)
#define SCE_DBG_ASSERT(_cond, _line)			do { } while (0)
#define SCE_DBG_ASSERT_CHECK(_cond, _line)		do { } while (0)
#define SCE_DBG_ASSERT_CHECK_LR(_cond, _line, _lr)	do { } while (0)
#define SCE_DBG_ASSERT_FAIL(_line)			do { } while (0)
#define SCE_DBG_ASSERT_LEVEL_CHECK(_level, _cond, _line)	do { } while (0)
#define SCE_DBG_PRINTF_FUNC(_flags, _line, _func, _fmt, ...)	do { } while (0)
#define SCE_DBG_ASSERT_PRINTF(_cond, _line, _fmt, ...)		do { } while (0)
#define SCE_DBG_ASSERT_PRINTF_LEVEL(_level, _cond, _line, _fmt, ...)	do { } while (0)
#define SCE_DBG_ASSERT_CHECK_FUNC(_cond, _line, _func)	do { } while (0)
#define SCE_DBG_ASSERT_LEVEL_FUNC(_level, _cond, _line, _func)	do { } while (0)
#define SCE_DBG_PRINTF_WITH_INFO_FUNC(_level, _line, _func, _fmt, ...)	do { } while (0)
#define SCE_DBG_ASSERT_FUNC(_cond, _line, _func)	do { } while (0)
#define SCE_DBG_ASSERT_PRINTF_FUNC(_cond, _line, _func, _fmt, ...)	do { } while (0)
#define SCE_DBG_PRINTF_LEVEL_FUNC(_level, _flags, _line, _func, _fmt, ...)	do { } while (0)

#endif

/* SceDebugForKernel_3A21194E: one character to the kernel console */
int SceDebugForKernel_3A21194E(int c);

int ksceKernelEnableCrashDump(SceBool enable);
int ksceKernelGetTtyInfo(char *buf, SceSize size);
