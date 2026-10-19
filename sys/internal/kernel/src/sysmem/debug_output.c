/* Debug output handler state and character ring. */
#include <sysmem/cp15.h>
#include <stdint.h>
#include <stdio.h>
#include <sysmem/debug.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include "sysmem_boot.h"
#include "debug_tty.h"
#include <stdarg.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysroot.h>

typedef int (*DebugPrintfHandler)(int argument, const char *format,
				  va_list arguments);
typedef void (*DebugPutcharHandler)(uintptr_t argument, int character);
typedef int (*DebugPrintfInvocation)(int argument, const char *format,
				     va_list arguments, uint32_t reserved);

extern int DebugDefaultPrintf(int argument, const char *format,
			      va_list arguments);
struct SysrootState;
extern volatile uintptr_t s_debugPutcharHandler;
extern volatile uintptr_t s_debugPutcharArgument;
extern volatile uint32_t s_debugPrintLevel;
extern int DebugCallPrintfHandler(int argument, const char *format,
				  va_list arguments);

DebugPrintfHandler volatile s_debugKprintfHandler = DebugDefaultPrintf;
static void *volatile s_debugKprintfArgument;
struct DebugTty s_debugTty;
static char s_debugPrefix[256];
volatile SceKernelSpinlock s_debugPrintLock;

int SceDebugForKernel_88AD6D0C(DebugPrintfHandler handler, void *argument)
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
		index = s_debugTty.index;
		next = index + 1;
		if (next == sizeof(s_debugTty.ring))
			next = 0;
	} while (SceCpuForDriver_CDA96E81(&s_debugTty.index, index, next) != index);
	s_debugTty.ring[index] = character;
}

int SceDebugForKernel_3A21194E(int character)
{
	DebugAppendCharacter(character);
	return 1;
}

__attribute__((noinline))
void DebugPutcharCallback(void *context, int character)
{
	DebugPutcharHandler handler;

	(void)context;
	if (character != 0x200 && character != 0x201)
		DebugAppendCharacter(character);

	handler = (DebugPutcharHandler)s_debugPutcharHandler;
	if (handler)
		handler(s_debugPutcharArgument, character);
}

int SceDebugForKernel_2AABAEDA(int character)
{
	DebugPutcharCallback(NULL, character);
	return 1;
}

int DebugDefaultPrintf(int argument, const char *format,
		       va_list arguments)
{
	(void)argument;
	return __prnt(DebugPutcharCallback, NULL, format, arguments);
}

__attribute__((noinline))
int DebugLevelEnabled(int level)
{
	if ((int32_t)s_debugPrintLevel < level)
		return 0;
	if (!SceSysrootForKernel_3E455842() || !SceSysrootForKernel_9DB56D1F())
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

int SceDebugForDriver_391B74B7(const char *format, ...)
{
	va_list arguments;
	uint32_t flags;
	int result;

	va_start(arguments, format);
	flags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	va_end(arguments);
	return result;
}

int SceDebugForDriver_411C0733(const char *format, va_list arguments)
{
	uint32_t flags;
	int result;

	flags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	return result;
}

int SceDebugForDriver_1A3F2AA4(int level, const char *format, ...)
{
	va_list arguments;
	uint32_t flags;
	int result = 0;

	va_start(arguments, format);
	if (!level || DebugLevelEnabled(level)) {
		flags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
		result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
					       format, arguments);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
	}
	va_end(arguments);
	return result;
}

int SceDebugForDriver_611A158B(int level, const char *format,
			  va_list arguments)
{
	uint32_t flags;
	int result;

	if (level && !DebugLevelEnabled(level))
		return 0;
	flags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, flags);
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

int SceDebugForDriver_02B04343(SceUInt32 flags, const SceKernelDebugInfo *info,
			    const char *format, ...)
{
	va_list arguments;
	uint32_t savedFlags;
	int result;

	va_start(arguments, format);
	savedFlags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
	DebugFormatPrefix(flags, info);
	result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
				       format, arguments);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, savedFlags);
	va_end(arguments);
	return result;
}

int SceDebugForDriver_D9703808(SceUInt32 level, SceUInt32 flags,
				 const SceKernelDebugInfo *info,
				 const char *format, ...)
{
	va_list arguments;
	uint32_t savedFlags;
	int result = 0;

	va_start(arguments, format);
	if (!level || DebugLevelEnabled(level)) {
		savedFlags = SceCpuForDriver_D32ACE9E(&s_debugPrintLock);
		DebugFormatPrefix(flags, info);
		result = DebugCallPrintfHandler((uintptr_t)s_debugKprintfArgument,
					       format, arguments);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_debugPrintLock, savedFlags);
	}
	va_end(arguments);
	return result;
}
