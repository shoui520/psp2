/*
 * SceGxm: sync objects.
 *
 * A sync object orders the GPU's work on a buffer; the system software
 * can also share them between processes under a key.
 */

#include "gxm_internal.h"

int sceGxmSyncObjectCreate(void **syncObject)
{
	void *created;

	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (syncObject == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (SceGpuEs4User_94286660(&gxmGlobals.top, &created) != 0)
		return SCE_GXM_ERROR_DRIVER;
	*syncObject = created;
	return 0;
}

int sceGxmSyncObjectDestroy(void *syncObject)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (syncObject == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (SceGpuEs4User_B26DF3D2(&gxmGlobals.top, syncObject) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

/* shared sync objects need SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC */
static inline _Bool gxmSharedSyncReady(void)
{
	return gxmGlobals.initialized && (gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC);
}

int sceGxmVshSyncObjectCreate(SceUInt32 key, void *syncObject)
{
	if (!gxmSharedSyncReady())
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (syncObject == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (key == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (SceGpuEs4UserForVsh_3C09BFCA(&gxmGlobals.top, key, syncObject) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

int sceGxmVshSyncObjectOpen(SceUInt32 key, void **syncObject)
{
	void *opened;

	if (!gxmSharedSyncReady())
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (syncObject == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (key == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	opened = NULL;
	if (SceGpuEs4UserForVsh_8A27AB0A(&gxmGlobals.top, key, &opened) != 0)
		return SCE_GXM_ERROR_DRIVER;
	if (opened == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	*syncObject = opened;
	return 0;
}

int sceGxmVshSyncObjectClose(SceUInt32 key, void *syncObject)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (syncObject == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if (key == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (SceGpuEs4UserForVsh_0A9556D9(&gxmGlobals.top, key, syncObject) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}

int sceGxmVshSyncObjectDestroy(SceUInt32 key)
{
	if (!gxmGlobals.initialized)
		return SCE_GXM_ERROR_UNINITIALIZED;
	if (key == 0)
		return SCE_GXM_ERROR_INVALID_VALUE;
	if (SceGpuEs4UserForVsh_010DA2CB(&gxmGlobals.top, key) != 0)
		return SCE_GXM_ERROR_DRIVER;
	return 0;
}
