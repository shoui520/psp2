/* System break/continue and the mapped ARM debug register interface. */
#include "sdfmgr_internal.h"
#include <sce/excpmgr_cpu.h>
#include <sce/arm_psr_text.h>
#include <sce/modulemgr_control.h>
#include <psp2kern/kernel/sysclib.h>

int ksceKernelGetModuleCBByAddr(SceUID pid, uint32_t address, SceModuleCB **module);

#if FW_DEBUG
#define MODULE_INFO(_line, _hash) \
	SDFMGR_DEBUG_INFO(_line, _hash, 0xd2bbeef4, "dumpModuleInfo")
#else
#define MODULE_INFO(_line, _hash) ({ \
	static const SceKernelDebugInfo info = { \
		.fileHash = 0x6364a003, .lineHash = (_hash), .funcHash = 0xd2bbeef4, \
	}; \
	&info; \
})
#endif

void dumpModuleInfo(uint32_t address)
{
	SceModuleCB *module, *snapshot;
	char name[27];

	if (ksceKernelGetModuleCBByAddr(SCE_KERNEL_PROCESS_ID, address, &module) < 0)
		return;
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2808, 0x3099e3a6),
		"\n**** Module Info ****\n  ModuleID    0x%08x\n", module->modid);
	snapshot = module;
	strncpy(name, snapshot->name, 26);
	name[26] = 0;
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2812, 0x25789f2e),
		"  Name        %s (%d.%d)\n", name,
		snapshot->version[1], snapshot->version[0]);
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2813, 0x3d30d411),
		"  Filename    %s\n", module->path);
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2814, 0xed38cbf5),
		"  FingerPrint 0x%08x\n", module->fingerprint);
	snapshot = module;
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2818, 0xeae37fc4),
		"  text        base=0x%08x, size=0x%08x\n",
		snapshot->segment[0].vaddr, snapshot->segment[0].memsz);
	snapshot = module;
	if (snapshot->nSegments > 1)
		ksceKernelPrintfWithInfo(0, MODULE_INFO(2822, 0x6eab9e96),
			"  data        base=0x%08x, size=0x%08x\n",
			snapshot->segment[1].vaddr, snapshot->segment[1].memsz);
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2824, 0xf85f551d),
		"  Pc          0x%08x\n", address);
	ksceKernelPrintfWithInfo(0, MODULE_INFO(2825, 0x0b786902), "\n\n");
}

void sdfMgrFormatCpsr(char *buffer, uint32_t cpsr)
{
	armFormatCpsr(cpsr, buffer);
}

int SceDeci4pSDfMgrForKernel_501CE61D(SceUInt32 level,
				   const SceKernelDebugInfo *info,
				   const char *format, ...)
{
	va_list args;
	SdfMgrCpuState *cpu;
	SdfMgrPrintHandler handler;
	int result = 0;

	va_start(args, format);
	cpu = &s_sdfMgrState.cpu[sdfMgrCoreIndex()];
	handler = cpu->printHandler;
	if (handler)
		result = handler(cpu->printContext, cpu->reserved, level, info, format, args);
	va_end(args);
	return result;
}

struct SdfMgrDebugRegisters {
	uint8_t reserved0[0x100];
	volatile uint32_t breakpointValue[6];
	uint8_t reserved1[0x28];
	volatile uint32_t breakpointControl[6];
	uint8_t reserved2[0xe48];
	volatile uint32_t claimSet;
	volatile uint32_t claimClear;
	uint8_t reserved3[8];
	volatile uint32_t lockAccess;
	volatile uint32_t lockStatus;
	uint8_t reserved4[0x48];
};

_Static_assert(sizeof(struct SdfMgrDebugRegisters) == 0x1000, "debug register page");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, breakpointValue) == 0x100,
	       "BVR0 register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, breakpointControl) == 0x140,
	       "BCR0 register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, claimSet) == 0xfa0,
	       "debug claim set register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, lockAccess) == 0xfb0,
	       "debug lock access register");
_Static_assert(offsetof(struct SdfMgrDebugRegisters, lockStatus) == 0xfb4,
	       "debug lock status register");

static inline int sdfMgrUnlockDebugRegisters(struct SdfMgrDebugRegisters *regs)
{
	if (!(regs->lockStatus & 2))
		return 0;
	regs->lockAccess = 0xc5acce55;
	(void)regs->lockAccess;
	dsb();
	return 1;
}

static inline void sdfMgrLockDebugRegisters(struct SdfMgrDebugRegisters *regs)
{
	regs->lockAccess = 0x3a5331aa;
	(void)regs->lockAccess;
	dsb();
}

void sceSDfMgrExcpAssert(void)
{
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
	unsigned cpu = ksceKernelCpuId();

	SceDeci4pSDfMgrForKernel_501CE61D(0,
		SDFMGR_DEBUG_INFO(2843, 0xf14ac88a, 0xce8e563c,
				 "sceSDfMgrExcpAssert"),
		"nestCount%d\n", s_sdfMgrState.exceptionDepth[cpu]);
	ksceKernelAssert(0,
		SDFMGR_DEBUG_INFO(2846, 0xd31e11b0, 0xce8e563c,
				 "sceSDfMgrExcpAssert"), caller);
#endif
}

int sceSDfMgrBreakSystem(void)
{
	unsigned cpu = sdfMgrCoreIndex();
	unsigned i;
	int result;

	if (!(s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4)) {
		sdfMgrUpdateCpuFlags(4, 1);
		_execControlComponent(&s_sdfMgrState.cpu[cpu], 1);
	}
	sdfMgrUpdateGlobalFlags(1, 1);
	result = ksceKernelTriggerSGI(0, 1, 0);
#if FW_DEBUG
	if (result < 0)
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(2583, 0x63163c46, 0x2ef59665,
					 "sceSDfMgrBreakSystem"),
			"generate break SGI failed. (0x%08x)\n", result);
#else
	(void)result;
#endif
	if (ksceKernelCheckDipsw(0xe4)) {
		for (i = 0; i < 4; i++) {
			struct SdfMgrDebugRegisters *regs =
				s_sdfMgrState.global.debugRegisters[i];
			int locked = sdfMgrUnlockDebugRegisters(regs);

			regs->claimSet = 1;
			dsb();
			if (locked)
				sdfMgrLockDebugRegisters(regs);
		}
		sev();
	}
	return 0;
}

int sceSDfMgrContinueSystem(void)
{
	unsigned cpu = sdfMgrCoreIndex();
	int result;

	if (s_sdfMgrState.cpu[sdfMgrCoreIndex()].flags & 4) {
		sdfMgrUpdateCpuFlags(4, 0);
		_execControlComponent(&s_sdfMgrState.cpu[cpu], 0);
	}
	sdfMgrUpdateGlobalFlags(1, 0);
	result = ksceKernelTriggerSGI(0, 1, 0);
#if FW_DEBUG
	if (result < 0)
		SceDeci4pSDfMgrForKernel_501CE61D(15,
			SDFMGR_DEBUG_INFO(2663, 0x483507b9, 0xb949ecd3,
					 "sceSDfMgrContinueSystem"),
			"generate break SGI failed. (0x%08x)\n", result);
#else
	(void)result;
#endif
	return 0;
}

int SceDeci4pSDfMgrForKernel_FCF211FB(uint32_t value, uint32_t control)
{
	unsigned i;

	if (!ksceKernelCheckDipsw(0xe4))
		return 0;
	for (i = 0; i < 4; i++) {
		struct SdfMgrDebugRegisters *regs =
			s_sdfMgrState.global.debugRegisters[i];
		int locked = sdfMgrUnlockDebugRegisters(regs);

		regs->breakpointValue[0] = value;
		regs->breakpointControl[0] = control;
		dsb();
		if (locked)
			sdfMgrLockDebugRegisters(regs);
	}
	return 0;
}
