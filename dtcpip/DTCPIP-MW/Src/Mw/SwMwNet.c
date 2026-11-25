/*
 * swagner's network setup (SwMwMain.cpp's helpers, from the middleware's own code): the
 * SceNet library loaded, its 64 KiB work memory, sceNetInit and sceNetCtlInit; the term
 * undoes them in reverse.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <sysmodule/sysmodule.h>
#include <stdlib.h>
#include <net.h>
#include <libnetctl.h>

#define SWMW_NET_MEMORY_SIZE	0x10000

/* the error the middleware reports when its network memory cannot be had */
#define SWMW_NET_ERROR_MEMORY	22

int swMwNetInit(void)
{
	SceNetInitParam param;
	int result;

	if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0)
		return SWMW_NET_ERROR_MEMORY;
	param.memory = malloc(SWMW_NET_MEMORY_SIZE);
	param.size = SWMW_NET_MEMORY_SIZE;
	param.flags = 0;
	if (param.memory == NULL) {
		result = SWMW_NET_ERROR_MEMORY;
		goto fail;
	}
	result = sceNetInit(&param);
	if (result >= 0) {
		result = sceNetCtlInit();
		if (result >= 0)
			return 0;
	}
fail:
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	return result;
}

int swMwNetTerm(void)
{
	sceNetCtlTerm();
	sceNetTerm();
	return sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}
