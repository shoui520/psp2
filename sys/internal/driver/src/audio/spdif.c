#include "audio_work.h"
/* S/PDIF lifecycle. */
#include <stdint.h>
#include <string.h>
#include <sysmem/sysmem.h>
#include <sysmem/qafmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/dipsw.h>
#include "audio_regs.h"
#include <lowio/pervasive.h>

int audioInitializeSpdif(void)
{
	SceSpdifRegisters *registers = audioSpdifRegisters;
	SceKernelAllocMemBlockOptKernel option;
	int result;

	if (registers == NULL) {
		memset(&option, 0, sizeof(option));
		option.size = sizeof(option);
		option.paddr = SPDIF_REG_BASE;
		option.attr = 2;
		result = ksceKernelAllocMemBlock("SceSpdifReg", 0x20100806,
			SPDIF_REG_SIZE, &option);
		if (result < 0)
			return result;
		ksceKernelGetMemBlockBase(result, (void **)&audioSpdifRegisters);
		registers = audioSpdifRegisters;
	}
	ScePervasiveForDriver_6B5DEDA2();
	ScePervasiveForDriver_6A2FB697();
	registers->configurationControl = 1;
	registers->transmitControl = 0;
	registers->clockControl = 0;
	registers->configuration[0] = 0;
	registers->configuration[0] = 0;
	registers->configuration[1] = 0;
	registers->configuration[1] = 0;
	registers->configuration[2] = 0;
	registers->configuration[2] = 0;
	registers->configuration[3] = 0;
	registers->configuration[3] = 0;
	registers->configuration[4] = 0;
	registers->configuration[4] = 0;
	registers->configuration[5] = 0;
	registers->configuration[5] = 0;
	registers->configuration[6] = 0;
	registers->configuration[6] = 0;
	registers->configuration[7] = 0;
	registers->configuration[7] = 0;
	registers->configuration[8] = 0;
	registers->configuration[8] = 0;
	registers->configuration[9] = 0;
	registers->configuration[9] = 0;
	registers->configuration[10] = 0;
	registers->configuration[10] = 0;
	registers->configuration[11] = 0;
	registers->configuration[11] = 0;
	if (ksceSblAimgrIsTool() &&
	    (!SceQafMgrForDriver_382C71E8() || ksceKernelCheckDipsw(229))) {
		registers->configuration[12] = 0x1e0040;
		registers->configuration[12] = 0x1e0040;
	} else {
		registers->configuration[12] = 0x1f0040;
		registers->configuration[12] = 0x1f0040;
	}
	registers->configuration[13] = 0;
	registers->configuration[13] = 0;
	registers->configuration[14] = 0;
	registers->configuration[14] = 0;
	registers->configuration[15] = 0;
	registers->configuration[15] = 0;
	registers->configuration[16] = 0;
	registers->configuration[16] = 0;
	registers->configuration[17] = 0;
	registers->configuration[17] = 0;
	registers->configurationControl = 0x400000;
	registers->control = 0;
	registers->configurationControl = 0x80400000;
	registers->outputEnable = 1;
	(void)registers->control;
	__asm__ volatile("dsb sy" ::: "memory");
	return 0;
}

int audioPrepareSuspendHook(void)
{
	return 0;
}
