#include <modulemgr/modulemgr.h>
#include "bbmc_update.h"

int BbmcUtilIsAvailable(void)
{
	int (*getVersion)(int, uint32_t *) = SceBbmcForDriver_7CB72009;

	if (SceModulemgrForDriver_BBE1771C("SceBbmc") < 0)
		return 0;
	return getVersion != 0;
}
