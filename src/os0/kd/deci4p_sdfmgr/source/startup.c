/* Per-core DECI initialization and CPU0 exception/debugger registration. */
#include "sdfmgr_internal.h"
#include <psp2kern/kernel/excpmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sce/excpmgr_cpu.h>
#include <sce/intrmgr.h>
#include <sce/suspend.h>
#include <sce/sysmem.h>

extern void sdfMgrDataAbortEntry(void);
extern void sdfMgrPrefetchAbortEntry(void);
extern void sdfMgrUndefinedEntry(void);

/* Sysroot keeps these handlers for the bus error, system error and power paths. */
int SceSysrootForKernel_8747D415(void (*handler)(uint32_t, uint32_t, uint32_t));
int SceSysrootForKernel_2D6B2A79(void (*handler)(uint32_t, SceSize, const void *));
int SceSysrootForKernel_C5EAF5F7(void (*handler)(int));
int dcmpInit(SdfMgrProtocolState *protocol);
int tsmpInit(SdfMgrProtocolState *protocol);
int sdioInit(void);
void dcmpSendStatusSystemError(uint32_t type, SceSize size, const void *source);
int _threadKicker(void);
int sdfMgrSuspend(void);
int sdfMgrResume(void);

static const struct {
	uint32_t physical;
	const char *name;
} debugMappings[4] = {
	{ 0xe3310000, "SceDbg0RegForSdfmgr" },
	{ 0xe3312000, "SceDbg1RegForSdfmgr" },
	{ 0xe3314000, "SceDbg2RegForSdfmgr" },
	{ 0xe3316000, "SceDbg3RegForSdfmgr" },
};

#if FW_DEBUG
#define BOOT_CHECK(_result, _line, _hash, _assertLine, _assertHash, _format) do { \
	if ((_result) < 0) { \
		SceDeci4pSDfMgrForKernel_501CE61D(15, \
			SDFMGR_DEBUG_INFO(_line, _hash, 0xaaf0498e, \
					 "sceSDfMgrModuleBoot"), _format, _result); \
		if (ksceKernelGetAssertLevel() > 0) \
			ksceKernelAssertLevel(1, 0, \
				SDFMGR_DEBUG_INFO(_assertLine, _assertHash, 0xaaf0498e, \
						 "sceSDfMgrModuleBoot"), caller); \
		return _result; \
	} \
} while (0)
#else
#define BOOT_CHECK(_result, _line, _hash, _assertLine, _assertHash, _format) do { \
	if ((_result) < 0) \
		return _result; \
} while (0)
#endif

int module_bootstart(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt options;
	uint32_t debugControl;
	unsigned i;
	int result;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	(void)args;
	(void)argp;
	if (!ksceKernelCheckDipsw(0xc1)) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 11,
			SDFMGR_DEBUG_INFO(224, 0xdb4fcfbc, 0xaaf0498e,
					 "sceSDfMgrModuleBoot"), "disabled by dipsw\n");
#endif
		return 1;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&s_sdfMgrState.cpu[sdfMgrCoreIndex()],
					 0, sizeof(SdfMgrCpuState));
	sdfMgrUpdateCpuFlags(2, 1);
	result = dcmpInit(_getPdCB(0));
	BOOT_CHECK(result, 251, 0xd78ad3ac, 252, 0x72ec2305,
		   "dcmpInit failed (0x%08x)\n");
	result = tsmpInit(_getPdCB(1));
	BOOT_CHECK(result, 259, 0x11bd48c8, 260, 0xae9f4ebd,
		   "tsmpInit failed (0x%08x)\n");
	result = sdioInit();
	BOOT_CHECK(result, 267, 0x8bc042c7, 268, 0x5805c178,
		   "sdioInit failed (0x%08x)\n");
	if (ksceKernelCheckDipsw(0xe4)) {
		CP_WRITE(p14, 0, c0, c0, 4, 0);
		CP_WRITE(p14, 0, c0, c1, 4, 0);
		CP_WRITE(p14, 0, c0, c2, 4, 0);
		CP_WRITE(p14, 0, c0, c3, 4, 0);
		CP_WRITE(p14, 0, c0, c4, 4, 0);
		CP_WRITE(p14, 0, c0, c5, 4, 0);
		CP_WRITE(p14, 0, c0, c0, 5, 0);
		CP_WRITE(p14, 0, c0, c1, 5, 0);
		CP_WRITE(p14, 0, c0, c2, 5, 0);
		CP_WRITE(p14, 0, c0, c3, 5, 0);
		CP_WRITE(p14, 0, c0, c4, 5, 0);
		CP_WRITE(p14, 0, c0, c5, 5, 0);
		CP_WRITE(p14, 0, c0, c0, 6, 0);
		CP_WRITE(p14, 0, c0, c1, 6, 0);
		CP_WRITE(p14, 0, c0, c2, 6, 0);
		CP_WRITE(p14, 0, c0, c3, 6, 0);
		CP_WRITE(p14, 0, c0, c0, 7, 0);
		CP_WRITE(p14, 0, c0, c1, 7, 0);
		CP_WRITE(p14, 0, c0, c2, 7, 0);
		CP_WRITE(p14, 0, c0, c3, 7, 0);
		debugControl = CP_READ(p14, 0, c0, c2, 2);
		if (!(debugControl & 0x8000))
			CP_WRITE(p14, 0, c0, c2, 2, debugControl | 0x8000);
		if (!ksceKernelCpuId()) {
			memset(&options, 0, sizeof(options));
			options.size = sizeof(options);
			options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
			for (i = 0; i < 4; i++) {
				options.paddr = debugMappings[i].physical;
				result = ksceKernelAllocMemBlock(debugMappings[i].name,
								 SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW, 0x1000, &options);
				if (result < 0)
					return result;
				s_sdfMgrState.global.debugMemblocks[i] = result;
				ksceKernelGetMemBlockBase(result,
					(void **)&s_sdfMgrState.global.debugRegisters[i]);
			}
		}
	}
	if (!ksceKernelCpuId()) {
		result = ksceExcpmgrRegisterHandler(3, 2, sdfMgrPrefetchAbortEntry);
		BOOT_CHECK(result, 291, 0x3ca87a6f, 292, 0x40a8c17c,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		result = ksceExcpmgrRegisterHandler(4, 2, sdfMgrDataAbortEntry);
		BOOT_CHECK(result, 303, 0x6a2d38af, 304, 0x292cf543,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		result = ksceExcpmgrRegisterHandler(1, 2, sdfMgrUndefinedEntry);
		BOOT_CHECK(result, 315, 0xda54a773, 316, 0xfa6cbe59,
			   "sceKernelRegisterExceptionHandler() failed (0x%08x)\n");
		/* The registered routines ignore the interrupt code and context. */
		result = ksceKernelRegisterIntrHandler(0, "SceSDfMgrBreakSgiHandler", 0,
			(SceKernelIntrHandler)sdfMgrWaitForControl, NULL, 0x10, 15, NULL);
		BOOT_CHECK(result, 331, 0xb54914e4, 332, 0xe368e1fb,
			   "sceKernelRegisterIntrHandler() failed (0x%08x)\n");
		result = ksceKernelRegisterIntrHandler(1, "SceSDfMgrThreadSgiHandler", 0,
			(SceKernelIntrHandler)_threadKicker, NULL, 0x20, 15, NULL);
		BOOT_CHECK(result, 347, 0x9d408fa5, 348, 0xdc18cd4a,
			   "sceKernelRegisterIntrHandler() failed (0x%08x)\n");
		SceSysrootForKernel_8747D415(busErrorHandler);
		SceSysrootForKernel_2D6B2A79(dcmpSendStatusSystemError);
		SceSysrootForKernel_C5EAF5F7(sdfMgrReportPowerState);
	}
	result = SceDeci4pSDfMgrForKernel_B22B42FB(sdfMgrHandleControlInterrupt);
	BOOT_CHECK(result, 366, 0xda2c4ab9, 367, 0x7cebaae2,
		   "RegisterBreakHook failed (0x%08x)\n");
	SceKernelSuspendForDriver_CE7A2207(25, (int (*)(void *))sdfMgrSuspend, NULL);
	SceKernelSuspendForDriver_105C5752(24, (int (*)(void *))sdfMgrResume, NULL);
	sdfMgrUpdateCpuFlags(1, 1);
	dcmpSendStatusPower(15, 1, 0);
	if (!ksceKernelCpuId())
		dcmpSendStatusPower(0, 2, 10);
	return 0;
}

#undef BOOT_CHECK
