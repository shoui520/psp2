/* SceKernelBlueScreenOfDeath startup and public compatibility entry. */
#include "bsod_internal.h"
#include <sce/compat_regs.h>

BsodDisplay s_bsodDisplay;
int s_bsodQafEnabled;

#if FW_DEBUG
static const SceKernelDebugInfo s_initAssert = {
	0x42b85a92, 0xdfcfee9f, 0x22ce179a,
	"bsod.c", 200, "_sceKernelBsodInit",
};
#endif

int module_start(unsigned arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	unsigned state;
	int uid;

	(void)arglen;
	(void)argp;
	s_bsodQafEnabled = SceQafMgrForDriver_382C71E8();
	if ((ksceSblAimgrIsCEX() && !s_bsodQafEnabled) ||
	    ksceKernelCheckDipsw(0xc1) || ksceKernelCheckDipsw(0xc2))
		return 1;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 2;
	options.paddr = COMPAT_SRAM_BASE;
	uid = ksceKernelAllocMemBlock("SceKernelBsodSram", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW,
				    COMPAT_SRAM_SIZE, &options);
	if (uid < 0) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0, &s_initAssert,
					     __builtin_return_address(0));
#endif
		return 1;
	}
	ksceKernelGetMemBlockBase(uid, &base);
	s_bsodDisplay.pixels = base;
	s_bsodDisplay.current.physical = COMPAT_SRAM_BASE;
	s_bsodDisplay.current.pitch = 960;
	s_bsodDisplay.current.x = 0;
	s_bsodDisplay.current.width = 960;
	s_bsodDisplay.current.height = 544;
	s_bsodDisplay.current.scaleX = 0x10000;
	s_bsodDisplay.current.scaleY = 0x10000;
	_sceKernelBsodSysconInit();
	_sceKernelBsodIftuInit(ksceSblAimgrIsDolce() != 0);
	state = ksceKernelCpuSuspendIntr();
	ksceExcpmgrRegisterHandler(3, 2, bsodPrefetchEntry);
	ksceExcpmgrRegisterHandler(4, 2, bsodDataEntry);
	ksceExcpmgrRegisterHandler(1, 2, bsodUndefinedEntry);
	SceDebugForKernel_67E9D1EC(bsodDebugCrashCallback);
	bsodInitializePutchar();
	ksceKernelEnableCrashDump(0);
	ksceKernelCpuResumeIntr(state);
	_sceKernelBsodDummyFsInit();
	return 0;
}

int SceDeci4pUserp_C53CFD12(void)
{
	return 0;
}
