/* Debug handler registration and assertion-level state. */
#include <stdint.h>
#include <sysmem/debug.h>
#include <sysmem/sysroot.h>
#include "sysmem_boot.h"
#include "sysmem_internal.h"

volatile uint32_t s_assertLevel;
static volatile uint32_t s_debugPutcharGate = 1;
volatile uintptr_t s_debugPutcharHandler;
volatile uintptr_t s_debugPutcharArgument;
volatile uint32_t s_debugPrintLevel;

#if !SYSMEM_NSKBL
__attribute__((noinline))
uint32_t SceDebugForKernel_C5889385(uint32_t level)
{
	uint32_t previous = s_debugPrintLevel;

	s_debugPrintLevel = level;
	return previous;
}

/* Name unknown; initializes debug output before the first startup query. */
int InitializeDebugPrintLevel(const struct SysmemBootContext *context)
{
	SceDebugForKernel_C5889385(context->debugPrintLevel);
	return 0;
}
#endif

/* boot type indicator 2: no crash dumps */
#define BOOT_NO_CRASH_DUMP		0x40

extern DebugCrashDumpHandler s_crashDumpHandler;
extern int s_crashDumpEnabled;

#if !SYSMEM_NSKBL
/* name unknown: the assertion level from the DIP switches, crash dumps on */
int InitializeAssertLevel(const struct SysmemBootContext *context)
{
	if (sceKernelCheckDipsw(DIPSW_ASSERT_LEVEL_2))
		s_assertLevel = 2;
	else if (sceKernelCheckDipsw(DIPSW_ASSERT_LEVEL_1))
		s_assertLevel = 1;
	s_crashDumpHandler = NULL;
	if (!(SysrootGetBootTypeIndicator2() & BOOT_NO_CRASH_DUMP))
		s_crashDumpEnabled = 1;
	return 0;
}
#endif

/* name unknown: core 0's lock taken, the lock released (no call reaches it) */
int InitializeDebugCorelock(void)
{
	sceKernelSysrootCorelockLock(0);
	sceKernelCpuId();
	sceKernelSysrootCorelockUnlock();
	return 0;
}

#if !SYSMEM_NSKBL
/* name unknown: a boot step with nothing left to do */
int InitializeDebugNop(void)
{
	return 0;
}

int SceDebugForKernel_0EDA5174(uint32_t value)
{
	(void)value;
	return 0;
}
#endif

#if FW_DEBUG || !SYSMEM_NSKBL	/* the release boot loader has no assertions */
int sceKernelGetAssertLevel(void)
{
	return s_assertLevel;
}
#endif

#if !SYSMEM_NSKBL
uint32_t SceDebugForKernel_A0CE3287(uint32_t level)
{
	uint32_t previous = s_assertLevel;

	s_assertLevel = level;
	return previous;
}
#endif

#if FW_DEBUG || !SYSMEM_NSKBL	/* the release boot loader has no assertions */
void SceDebugForDriver_FD753E7A(SceUInt32 level, SceBool condition,
			   const SceKernelDebugInfo *info, const void *caller)
{
	if ((int32_t)level <= (int32_t)s_assertLevel)
		sceKernelAssert(condition, info, caller);
}
#endif

uintptr_t SceDebugForKernel_8D474850(void)
{
	if (s_debugPutcharGate)
		return 0;
	return s_debugPutcharHandler;
}

uintptr_t SceDebugForKernel_22546577(uintptr_t handler,
					      uintptr_t argument)
{
	s_debugPutcharHandler = handler;
	s_debugPutcharGate = 0;
	s_debugPutcharArgument = argument;
	return handler;
}
