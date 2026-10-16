#include <psp2kern/kernel/modulemgr.h>
#include "bbmc_update.h"

int BbmcUtilIsAvailable(void)
{
	int (*getVersion)(int, uint32_t *) = SceBbmcForDriver_7CB72009;

	if (ksceKernelSearchModuleByName("SceBbmc") < 0)
		return 0;
	return getVersion != 0;
}
