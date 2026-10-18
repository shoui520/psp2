#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/sysclib.h>
#include "update_internal.h"

extern int spkgMbrLoaded;
extern unsigned int spkgBlSwitchCount, spkgOsSwitchCount;
extern uint32_t spkgModifiedBanks;
extern uint64_t spkgOsChunkMask, spkgVshChunkMask;
extern SceMbr spkgMbr;
int UpdateUtilLockMbr(void);
int UpdateUtilUnlockMbr(void);
int StorageUtilReadMbr(SceMbr *mbr);
int StorageUtilWriteMbr(const SceMbr *mbr);

int SpkgUtilApplyMBR(void)
{
	SceMbr mbr;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	UpdateUtilLockMbr();
	result = spkgMbrLoaded;
	if (!result)
		goto unlock;
	result = spkgModifiedBanks;
	if (!result)
		goto unlock;
	result = StorageUtilWriteMbr(&spkgMbr);
	if (result)
		goto unlock;
	result = StorageUtilReadMbr(&mbr);
	if (result)
		goto unlock;
	if (timingsafe_memcmp(&spkgMbr, &mbr, sizeof(mbr))) {
		result = (int)0x800f0205;
		goto unlock;
	}
	spkgMbrLoaded = 0;
	spkgModifiedBanks = 0;
	spkgBlSwitchCount = 0;
	spkgOsSwitchCount = 0;
	spkgOsChunkMask = 0;
	spkgVshChunkMask = 0;
unlock:
	UpdateUtilUnlockMbr();
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
