/* The boot configuration has finished; this module never stays resident. */
#include "sysstatemgr.h"

void afterBoot(void)
{
	sceKernelSysrootSetStatus(0x1000);
	SCE_DBG_PRINTF(0, 45, ".... done\n");
	sceKernelSetGPO(0);
	if (SceModulemgrForKernel_2F82EEBC() < 0)
		SCE_PANIC(53);
	for (;;)
		;
}
