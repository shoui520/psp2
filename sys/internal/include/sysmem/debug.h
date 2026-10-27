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
 * only for calls that remain, e.g. SceDebugForDriver_391B5B74. (vitasdk's SceKernelDebugInfo puts
 * func before file; the records in Sony's modules are file, line, func.)
 *
 * Release firmwares (FW_DEBUG == 0) compile all of it out.
 */

#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <stdarg.h>

typedef struct SceKernelDebugInfo {
	SceUInt32 fileHash;
	SceUInt32 lineHash;
	SceUInt32 funcHash;
	const char *file;
	int line;
	const char *func;
} SceKernelDebugInfo;

typedef struct ThreadSyscallFrame ThreadSyscallFrame;
typedef struct SceExcpmgrContext SceExcpmgrContext;
typedef struct SceKernelBacktraceFrameForKernel DebugFrame;

/* the services that report a fault (all optional) */
typedef int (*DebugBacktraceHandler)(SceUID thid, DebugFrame *frames, SceSize size,
				     SceSize *count, SceUInt32 flags);
typedef int (*DebugPrintBacktraceHandler)(SceUID pid, const DebugFrame *frames,
					  SceSize count, SceUInt32 start);
typedef int (*DebugCrashDumpHandler)(int type, SceSize size, void *dump);
typedef void (*DebugFaultHandler)(SceUInt64 time, int cpu, SceUInt32 fileHash,
				  SceUInt32 lineHash, SceUInt32 funcHash, int reserved0, int reserved1,
				  const void *caller, const void *lr);
typedef void (*EventLogHandler)(const void *entry, SceSize size);

/* the exception dump's header has no return addresses */
typedef struct DebugExceptionDumpHeader {
	SceSize size;
	SceUID pid;
	SceUID tid;
	int cpu;
	SceUInt64 time;
	SceUInt32 fileHash;
	SceUInt32 lineHash;
	SceUInt32 funcHash;
	SceUInt32 kernelFrames;
	SceUInt32 userFrames;
	SceSize frameSize;
	SceUInt32 registerWords;
	SceUInt32 syscallWords;
} DebugExceptionDumpHeader;

/* vitasdk: ksceKernelPrintfLevelWithInfo */
int SceDebugForDriver_D9703808(SceUInt32 level, SceUInt32 flags,
				  const SceKernelDebugInfo *info, const char *fmt, ...);
int sceKernelGetAssertLevel(void) __attribute__((pure));	/* Sony's: consecutive checks share one call */
/* vitasdk: ksceKernelAssertLevel */
void SceDebugForDriver_FD753E7A(SceUInt32 level, SceBool cond,
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
#define SCE_DBGSRC_SECTION	".debug_sce_dbgsrc,\"\",%progbits @"

#ifndef SCE_DBGINFO_SECTION
#ifdef SCE_DBGINFO_GC
/* a section per record, so a link with --gc-sections keeps only the records of kept code */
#define SCE_DBGINFO_STR(_n)		#_n
#define SCE_DBGINFO_GC_SECTION(_n)	".sce_dbginfo." SCE_DBGINFO_STR(_n)
#define SCE_DBGINFO_SECTION		SCE_DBGINFO_GC_SECTION(__COUNTER__)
#else
#define SCE_DBGINFO_SECTION ".sce_dbginfo"
#endif
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

/* vitasdk: ksceKernelPanic */
void SceDebugForDriver_391B5B74(const SceKernelDebugInfo *info, const void *lr) __attribute__((noreturn));
void sceKernelAssert(SceBool cond, const SceKernelDebugInfo *info, const void *lr);
void SceDebugForDriver_00CCE39C(const SceKernelDebugInfo *info, const void *lr, const char *fmt, ...)
	__attribute__((noreturn));	/* vitasdk: ksceKernelPrintfPanic */

int SceDebugForDriver_391B74B7(const char *fmt, ...);	/* vitasdk: ksceKernelPrintf */
/* vitasdk: ksceKernelRegisterKprintfHandler */
int SceDebugForKernel_88AD6D0C(int (*handler)(int unk, const char *fmt, va_list ap),
				     void *argp);
/* vitasdk: ksceKernelPrintfWithInfo */
int SceDebugForDriver_02B04343(SceUInt32 level, const SceKernelDebugInfo *info, const char *fmt, ...);
/* int: appmgr returns its result, though sysmem's function sets none */
/* vitasdk: ksceKernelPrintfAssertLevel */
int SceDebugForDriver_821A2D59(SceUInt32 level, SceBool cond, const SceKernelDebugInfo *info,
			       const void *lr, const char *fmt, ...);

/*
 * A line of Sony's source that moved between the 3.6x sources (3.63, 3.65)
 * and the 4.00 ones; it matters for the records release builds keep too.
 */
#define SCE_LINE(_line36x, _line400)	(FW_VERSION >= 0x04000000 ? (_line400) : (_line36x))

/* SceDebugForDriver_02B04343(level, ...): compiled in on every firmware */
#define SCE_PRINTF_WITH_INFO(_level, _line, _fmt, ...)			\
	SceDebugForDriver_02B04343((_level), SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)

/* the same in a function Sony named differently */
#define SCE_PRINTF_WITH_INFO_FUNC(_level, _line, _func, _fmt, ...)	\
	SceDebugForDriver_02B04343((_level), SCE_DBGINFO_FUNC(_line, _func), _fmt, ##__VA_ARGS__)

/* kernel panic: compiled in on every firmware */
#define SCE_PANIC(_line)						\
	SceDebugForDriver_391B5B74(SCE_DBGINFO(_line), __builtin_return_address(0))

/* the same in a helper inlined into another function, under the helper's name */
#define SCE_PANIC_FUNC(_line, _func)					\
	SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0))

/* kernel panic with a message */
#define SCE_PANIC_PRINTF(_line, _fmt, ...)				\
	SceDebugForDriver_00CCE39C(SCE_DBGINFO(_line), __builtin_return_address(0),	\
			      _fmt, ##__VA_ARGS__)

/* the same in a helper inlined into another function, under the helper's name */
#define SCE_PANIC_PRINTF_FUNC(_line, _func, _fmt, ...)			\
	SceDebugForDriver_00CCE39C(SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0), \
			      _fmt, ##__VA_ARGS__)

#if FW_DEBUG

/* SceDebugForDriver_02B04343(level, ...), compiled in on debug firmwares only */
#define SCE_DBG_PRINTF_WITH_INFO(_level, _line, _fmt, ...)		\
	SceDebugForDriver_02B04343((_level), SCE_DBGINFO(_line), _fmt, ##__VA_ARGS__)

/* SceDebugForDriver_D9703808(1, flags, ...) */
#define SCE_DBG_PRINTF(_flags, _line, _fmt, ...)			\
	SceDebugForDriver_D9703808(1, (_flags), SCE_DBGINFO(_line),	\
				      _fmt, ##__VA_ARGS__)

/* SceDebugForDriver_D9703808(level, flags, ...) */
#define SCE_DBG_PRINTF_LEVEL(_level, _flags, _line, _fmt, ...)		\
	SceDebugForDriver_D9703808((_level), (_flags), SCE_DBGINFO(_line),	\
				      _fmt, ##__VA_ARGS__)

/* SceDebugForDriver_FD753E7A(level, cond, info, lr) */
#define SCE_DBG_ASSERT_LEVEL(_level, _cond, _line)			\
	SceDebugForDriver_FD753E7A((_level), (_cond), SCE_DBGINFO(_line),	\
			      __builtin_return_address(0))

/* unconditional assertion failure (the condition is already known false) */
/* sceKernelAssert(cond, info, lr) */
#define SCE_DBG_ASSERT(_cond, _line)					\
	sceKernelAssert((_cond), SCE_DBGINFO(_line), __builtin_return_address(0))

/* level 1 assertion, checked only while assertions are enabled */
#define SCE_DBG_ASSERT_CHECK(_cond, _line) do {				\
	if (sceKernelGetAssertLevel() > 0)				\
		SceDebugForDriver_FD753E7A(1, (_cond), SCE_DBGINFO(_line),	\
				      __builtin_return_address(0));	\
} while (0)

#define SCE_DBG_ASSERT_FAIL(_line)	SCE_DBG_ASSERT_CHECK(0, _line)

/* the same, reporting a return address the caller passed down */
#define SCE_DBG_ASSERT_CHECK_LR(_cond, _line, _lr) do {			\
	if (sceKernelGetAssertLevel() > 0)				\
		SceDebugForDriver_FD753E7A(1, (_cond), SCE_DBGINFO(_line), (_lr));	\
} while (0)

/* level _level assertion, checked only while assertions of that level are enabled */
#define SCE_DBG_ASSERT_LEVEL_CHECK(_level, _cond, _line) do {		\
	if (sceKernelGetAssertLevel() >= (_level))			\
		SceDebugForDriver_FD753E7A((_level), (_cond), SCE_DBGINFO(_line),	\
				      __builtin_return_address(0));	\
} while (0)

/* level 1 assertion with a message, checked only while assertions are enabled */
#define SCE_DBG_ASSERT_PRINTF(_cond, _line, _fmt, ...) do {		\
	if (sceKernelGetAssertLevel() > 0)				\
		SceDebugForDriver_821A2D59(1, (_cond), SCE_DBGINFO(_line),	\
					    __builtin_return_address(0), _fmt,	\
					    ##__VA_ARGS__);			\
} while (0)

/* SceDebugForDriver_821A2D59(level, cond, ...), checked whatever the assert level */
#define SCE_DBG_ASSERT_PRINTF_LEVEL(_level, _cond, _line, _fmt, ...)		\
	SceDebugForDriver_821A2D59((_level), (_cond), SCE_DBGINFO(_line),	\
				    __builtin_return_address(0), _fmt, ##__VA_ARGS__)

/* the same in a helper inlined into _func, sharing one record over all its copies */
#define SCE_DBG_PRINTF_FUNC(_flags, _line, _func, _fmt, ...)		\
	SceDebugForDriver_D9703808(1, (_flags), SCE_DBGINFO_FUNC(_line, _func),	\
				      _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_FUNC(_cond, _line, _func)			\
	sceKernelAssert((_cond), SCE_DBGINFO_FUNC(_line, _func), __builtin_return_address(0))
#define SCE_DBG_PRINTF_LEVEL_FUNC(_level, _flags, _line, _func, _fmt, ...)	\
	SceDebugForDriver_D9703808((_level), (_flags), SCE_DBGINFO_FUNC(_line, _func),	\
				      _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_PRINTF_FUNC(_cond, _line, _func, _fmt, ...) do {	\
	if (sceKernelGetAssertLevel() > 0)				\
		SceDebugForDriver_821A2D59(1, (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
					    __builtin_return_address(0), _fmt,	\
					    ##__VA_ARGS__);			\
} while (0)
#define SCE_DBG_PRINTF_WITH_INFO_FUNC(_level, _line, _func, _fmt, ...)	\
	SceDebugForDriver_02B04343((_level), SCE_DBGINFO_FUNC(_line, _func),	\
				 _fmt, ##__VA_ARGS__)
#define SCE_DBG_ASSERT_LEVEL_FUNC(_level, _cond, _line, _func)		\
	SceDebugForDriver_FD753E7A((_level), (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
			      __builtin_return_address(0))
#define SCE_DBG_ASSERT_CHECK_FUNC(_cond, _line, _func) do {		\
	if (sceKernelGetAssertLevel() > 0)				\
		SceDebugForDriver_FD753E7A(1, (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
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

int SceDebugForKernel_A465A31A(SceBool enable);	/* vitasdk: ksceKernelEnableCrashDump */
int SceDebugForDriver_62466B0A(char *buf, SceSize size);	/* vitasdk: ksceKernelGetTtyInfo */

/* vitasdk: ksceEventLogGetInfo */
SceSSize SceDebugForDriver_CC5365D3(void *buffer, SceSize size, SceUInt32 *count);
int SceDebugForKernel_2AABAEDA(int character);	/* vitasdk: ksceKernelDebugPutchar */
int sceKernelPrintfLevel(int level, const char *format, ...);
/* vitasdk: ksceKernelVprintf */
int SceDebugForDriver_411C0733(const char *format, va_list arguments);

/* vitasdk: ksceKernelVprintfLevel */
int SceDebugForDriver_611A158B(int level, const char *format, va_list arguments);
/* vitasdk: ksceEventLogPut */
int SceDebugForDriver_912CF2BA(SceUID pid, SceUID tid, SceUInt16 type, SceUInt16 subtype, SceUInt32 arg, const void *data, SceSize size);
int SceDebugForKernel_0EDA5174(uint32_t value);
/* vitasdk: ksceKernelRegisterDebugPutcharHandler */
uintptr_t SceDebugForKernel_22546577(uintptr_t handler, uintptr_t argument);
void SceDebugForKernel_38F1A352(DebugPrintBacktraceHandler handler);
void SceDebugForKernel_4C62F8CC(DebugCrashDumpHandler handler);
SceSize SceDebugForKernel_5D6F0CFC(DebugExceptionDumpHeader *dump, int cpu, SceUInt64 time, const SceKernelDebugInfo *info, const SceExcpmgrContext *context, SceUInt32 arg);
void SceDebugForKernel_67E9D1EC(DebugFaultHandler handler);
void SceDebugForKernel_78027203(EventLogHandler handler);
uintptr_t SceDebugForKernel_8D474850(void);	/* vitasdk: ksceKernelGetDebugPutcharHandler */
void SceDebugForKernel_91E7D24F(DebugBacktraceHandler handler);
uint32_t SceDebugForKernel_A0CE3287(uint32_t level);	/* vitasdk: ksceKernelSetAssertLevel */
void SceDebugForKernel_BCC8D0B2(SceUID pid, const ThreadSyscallFrame *frame, const SceUInt32 *result);
uint32_t SceDebugForKernel_C5889385(uint32_t level);
void SceDebugForKernel_CCABDD98(const SceKernelDebugInfo *info, const SceExcpmgrContext *context, SceUInt32 arg, const char *type);
void SceDebugForTZS_563B623D(const SceKernelDebugInfo *info, const void *lr);
void SceDebugForTZS_618F89E6(SceUInt32 level, SceBool condition, const SceKernelDebugInfo *info, const void *caller);
int SceDebugForTZS_D26EB4BB(SceUInt32 level, SceUInt32 flags, const SceKernelDebugInfo *info, const char *format, ...);
int sceKernelGetAssertLevel(void);
int k_sceEventLogPut(SceUInt16 type, SceUInt16 subtype, SceUInt32 arg, const void *data, SceSize size);
