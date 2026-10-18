/*
 * The FIOS2 API as the syscalls see it: each call goes to the kernel API,
 * except the directory handle calls, which user mode does not get.
 */

#include "fios2_internal.h"

int sceFiosKernelOverlayAdd(SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	return SceFios2KernelForDriver_2607EE4C(overlay, outId);
}

int sceFiosKernelOverlayRemove(SceFiosOverlayID id)
{
	return SceFios2KernelForDriver_2368FEB5(id);
}

int sceFiosKernelOverlayGetInfo(SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	return SceFios2KernelForDriver_725E6817(id, outOverlay);
}

int sceFiosKernelOverlayModify(SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	return SceFios2KernelForDriver_7F8B960C(id, overlay);
}

int sceFiosKernelOverlayAddForProcess(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	return SceFios2KernelForDriver_17E65A1C(pid, overlay, outId);
}

int sceFiosKernelOverlayRemoveForProcess(SceUID pid, SceFiosOverlayID id)
{
	return SceFios2KernelForDriver_23247EFB(pid, id);
}

int sceFiosKernelOverlayGetInfoForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	return SceFios2KernelForDriver_F1762BC2(pid, id, outOverlay);
}

int sceFiosKernelOverlayModifyForProcess(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *overlay)
{
	return SceFios2KernelForDriver_853EA82A(pid, id, overlay);
}

int sceFiosKernelOverlayGetList(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount)
{
	return SceFios2KernelForDriver_FF42AAF0(pid, minOrder, maxOrder, outIds, maxIds, outCount);
}

int sceFiosKernelOverlayResolveSync(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				    SceSize maxPath)
{
	return SceFios2KernelForDriver_0F456345(pid, resolveFlag, inPath, outPath, maxPath);
}

int sceFiosKernelOverlayResolveWithRangeSync(SceUID pid, int resolveFlag, const char *inPath,
					     char *outPath, SceSize maxPath, uint8_t minOrder,
					     uint8_t maxOrder)
{
	return SceFios2KernelForDriver_D3D968FC(pid, resolveFlag, inPath, outPath, maxPath,
							 minOrder, maxOrder);
}

int sceFiosKernelOverlayDHOpenSync(SceFiosKernelDH **outDH, const char *path, uint8_t minOrder,
				   uint8_t maxOrder)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHReadSync(SceFiosKernelDH *dh, SceIoDirent *dirent)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHCloseSync(SceFiosKernelDH *dh)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHStatSync(SceFiosKernelDH *dh, SceIoStat *stat)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHChstatSync(SceFiosKernelDH *dh, SceIoStat *stat, int bits)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayDHSyncSync(SceFiosKernelDH *dh, int flag)
{
	return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int sceFiosKernelOverlayThreadSetDisabled(int disabled)
{
	return SceFios2KernelForDriver_03727E5E(disabled);
}

int sceFiosKernelOverlayThreadIsDisabled(void)
{
	return SceFios2KernelForDriver_E71192C5();
}

int sceFiosKernelOverlayGetRecommendedScheduler(int avail, const char *path)
{
	return SceFios2KernelForDriver_241BF0D6(avail, path);
}
