/* Debug output handler state and character ring. */
#include "sce/cp15.h"
#include <stdint.h>
#include <stdio.h>
#include <sce/debug.h>
#include "sysmem_boot.h"
#include <stdarg.h>

typedef int (*DebugPrintfHandler)(int argument, const char *format,
				  va_list arguments);
typedef void (*DebugPutcharHandler)(uintptr_t argument, int character);
typedef int (*DebugPrintfInvocation)(int argument, const char *format,
				     va_list arguments, uint32_t reserved);

extern int DebugDefaultPrintf(int argument, const char *format,
			      va_list arguments);
extern uint32_t ksceKernelAtomicCompareAndSet32(volatile uint32_t *address,
					      uint32_t expected, uint32_t replacement);
extern SceKblParam *ksceKernelSysrootGetKblParam(void);
extern int SceQafMgrForDriver_382C71E8(void);	/* qafmgr.c */
struct SysrootState;
extern struct SysrootState *ksceSysrootGetSysroot(void);
extern int __prnt(DebugPutcharHandler output, uintptr_t argument,
		  const char *format, va_list arguments);
extern volatile uintptr_t s_debugPutcharHandler;
extern volatile uintptr_t s_debugPutcharArgument;
extern volatile uint32_t s_debugPrintLevel;
extern int ksceKernelSpinlockLowLockCpuSuspendIntr(volatile uint32_t *lock);
extern void ksceKernelSpinlockLowUnlockCpuResumeIntr(volatile uint32_t *lock,
						   uint32_t flags);
extern int DebugCallPrintfHandler(int argument, const char *format,
				  va_list arguments);

DebugPrintfHandler volatile s_debugKprintfHandler = DebugDefaultPrintf;
static void *volatile s_debugKprintfArgument;
volatile uint32_t s_debugRingIndex;
char s_debugRing[4096];
static char s_debugPrefix[256];
volatile uint32_t s_debugPrintLock;

int ksceKernelRegisterKprintfHandler(DebugPrintfHandler handler, void *argument)
{
	s_debugKprintfHandler = handler;
	s_debugKprintfArgument = argument;
	return (uintptr_t)handler;
}

__attribute__((noinline))
void DebugAppendCharacter(int character)
{
	uint32_t index, next;

	do {
		index = s_debugRingIndex;
		next = index + 1;
		if (next == sizeof(s_debugRing))
			next = 0;
	} while (ksceKernelAtomicCompareAndSet32(&s_debugRingIndex, index, next) != index);
	s_debugRing[index] = character;
}

int SceDebugForKernel_3A21194E(int character)
{
	DebugAppendCharacter(character);
	return 1;
}

__attribute__((noinline))
void DebugPutcharCallback(uintptr_t argument, int character)
{
	DebugPutcharHandler handler;

	(void)argument;
	if (character != 0x200 && character != 0x201)
		DebugAppendCharacter(character);

	handler = (DebugPutcharHandler)s_debugPutcharHandler;
	if (handler)
		handler(s_debugPutcharArgument, character);
}

int ksceKernelDebugPutchar(int character)
{
	DebugPutcharCallback(0, character);
	return 1;
}

int DebugDefaultPrintf(int argument, const char *format,
		       va_list arguments)
{
	(void)argument;
	return __prnt(DebugPutcharCallback, 0, format, arguments);
}

__attribute__((noinline))
int DebugLevelEnabled(int level)
{
	if ((int32_t)s_debugPrintLevel < level)
		return 0;
	if (!ksceSysrootGetSysroot() || !ksceKernelSysrootGetKblParam())
		return 1;
	return SceQafMgrForDriver_382C71E8() != 0;
}

__attribute__((noinline))
int DebugCallPrintfHandler(int argument, const char *format,
			   va_list arguments)
{
	DebugPrintfInvocation handler = (DebugPrintfInvocation)s_debugKprintfHandler;

	/* Native dispatch clears r3 before BLX; the public callback ABI stays unchanged. */
	return handler ? handler(argument, format, arguments, 0) : 0;
}

int ksceKernelPrintf(const char *format, ...)
{
	va_list arguments;
	uint32_t flags;
	int result;

	va_start(arguments, format);
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	va_end(arguments);
	return result;
}

int ksceKernelVprintf(const char *format, va_list arguments)
{
	uint32_t flags;
	int result;

	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	return result;
}

int ksceKernelPrintfLevel(int level, const char *format, ...)
{
	va_list arguments;
	uint32_t flags;
	int result = 0;

	va_start(arguments, format);
	if (!level || DebugLevelEnabled(level)) {
		flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
		result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
					       format, arguments);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	}
	va_end(arguments);
	return result;
}

int ksceKernelVprintfLevel(int level, const char *format,
			  va_list arguments)
{
	uint32_t flags;
	int result;

	if (level && !DebugLevelEnabled(level))
		return 0;
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	return result;
}

__attribute__((noinline))
int DebugPrintfPrefix(const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       "%s", arguments);
	va_end(arguments);
	return result;
}

__attribute__((noinline))
int DebugFormatPrefix(uint32_t flags, const SceKernelDebugInfo *info)
{
	char *cursor = s_debugPrefix;
	uint32_t cpuId;

	if (flags & 1) {
		cpuId = readMpidr();
		*cursor++ = '0' + (cpuId & 3);
		*cursor++ = ':';
	}
	if (info) {
		if ((flags & 6) == 2) {
			if (info->file)
				cursor += snprintf(cursor, 64, "0x%08x(%s):",
						   info->fileHash, info->file);
			else
				cursor += snprintf(cursor, 64, "0x%08x:", info->fileHash);
		} else if ((flags & 6) == 6) {
			if (info->file)
				cursor += snprintf(cursor, 64, "0x%08x%08x(%s:%d):",
						   info->fileHash, info->lineHash,
						   info->file, info->line);
			else
				cursor += snprintf(cursor, 64, "0x%08x%08x:",
						   info->fileHash, info->lineHash);
		}
		if (flags & 8) {
			if (info->func)
				cursor += snprintf(cursor, 64, "0x%08x(%s):",
						   info->funcHash, info->func);
			else
				cursor += snprintf(cursor, 64, "0x%08x:", info->funcHash);
		}
	}
	*cursor = '\0';
	return DebugPrintfPrefix("%s", s_debugPrefix);
}

int ksceKernelPrintfWithInfo(SceUInt32 flags, const SceKernelDebugInfo *info,
			    const char *format, ...)
{
	va_list arguments;
	uint32_t savedFlags;
	int result;

	va_start(arguments, format);
	savedFlags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
	DebugFormatPrefix(flags, info);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, savedFlags);
	va_end(arguments);
	return result;
}

int ksceKernelPrintfLevelWithInfo(SceUInt32 level, SceUInt32 flags,
				 const SceKernelDebugInfo *info,
				 const char *format, ...)
{
	va_list arguments;
	uint32_t savedFlags;
	int result = 0;

	va_start(arguments, format);
	if (!level || DebugLevelEnabled(level)) {
		savedFlags = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_debugPrintLock);
		DebugFormatPrefix(flags, info);
		result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
					       format, arguments);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, savedFlags);
	}
	va_end(arguments);
	return result;
}
