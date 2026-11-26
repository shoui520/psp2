/*
 * shell_svc_patchcheck.cpp: SceShellSvc IPMI client for updating the shell's
 * patch-check status cache (0x810034C0-0x8100354F).
 */
#include "libgameupdate_internal.h"
#include <ipmi/client.h>

#define SHELL_SVC_METHOD_PATCHCHECK_SET_CACHE	0x190002u

extern "C" IPMI::Client *SceShellSvc_B31E7F1C(void);

__attribute__((noinline))
static int shellSvcPatchCheckSetCacheImpl(IPMI::Client *client, const char *titleId,
					  const PatchCheckCacheEntry *entry, int updateMask)
{
	int methodResult = 0;
	int mask = updateMask;
	IPMI::DataInfo inList[3];
	SceSize len = 0;
	const char *p = titleId;

	while (*p != '\0') {
		++p;
		++len;
	}

	inList[0].pData = &mask;
	inList[0].dataSize = sizeof(mask);
	inList[1].pData = titleId;
	inList[1].dataSize = len + 1;
	inList[2].pData = entry;
	inList[2].dataSize = sizeof(*entry);

	int ret = client->invokeSyncMethod(SHELL_SVC_METHOD_PATCHCHECK_SET_CACHE,
					   inList, 3, &methodResult, NULL, 0);
	if (ret == 0)
		ret = methodResult;
	return ret;
}

__attribute__((noinline))
int shellSvcPatchCheckSetCache(const char *titleId, const PatchCheckCacheEntry *entry,
			       int updateMask)
{
	IPMI::Client *client = SceShellSvc_B31E7F1C();
	return shellSvcPatchCheckSetCacheImpl(client, titleId, entry, updateMask);
}
