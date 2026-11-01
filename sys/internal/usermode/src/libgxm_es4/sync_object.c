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
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(20, sceGxmSyncObjectCreate);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (syncObject == NULL) {
		GXM_DBG_ERROR_NULL(26, sceGxmSyncObjectCreate, syncObject);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	err = SceGpuEs4User_94286660(&gxmGlobals.top, &created);
	if (err != 0) {
		GXM_DBG_ERROR(34, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	*syncObject = created;
	return 0;
}

int sceGxmSyncObjectDestroy(void *syncObject)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(47, sceGxmSyncObjectDestroy);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (syncObject == NULL) {
		GXM_DBG_ERROR_NULL(53, sceGxmSyncObjectDestroy, syncObject);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	err = SceGpuEs4User_B26DF3D2(&gxmGlobals.top, syncObject);
	if (err != 0) {
		GXM_DBG_ERROR(60, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

/* Sony's names for the shared sync object calls */
#define GXM_DBG_ERROR_NOT_SHARED(line, function)					\
	GXM_DBG_ERROR(line, "Function " #function " has been called, but libgxm was not "	\
		      "initialized to allow shared sync objects.")
#define GXM_DBG_ERROR_KEY(line, function)						\
	GXM_DBG_ERROR(line, "Parameter key to function " #function " is 0.")

int sceGxmVshSyncObjectCreate(SceUInt32 key, void *syncObject)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(72, sceGxmSyncObjectCreateShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	/* shared sync objects need SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC */
	if (!(gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC)) {
		GXM_DBG_ERROR_NOT_SHARED(76, sceGxmSyncObjectCreateShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (syncObject == NULL) {
		GXM_DBG_ERROR_NULL(82, sceGxmSyncObjectCreateShared, syncObject);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (key == 0) {
		GXM_DBG_ERROR_KEY(87, sceGxmSyncObjectCreateShared);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	err = SceGpuEs4UserForVsh_3C09BFCA(&gxmGlobals.top, key, syncObject);
	if (err != 0) {
		GXM_DBG_ERROR(91, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

int sceGxmVshSyncObjectOpen(SceUInt32 key, void **syncObject)
{
	void *opened;
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(103, sceGxmSyncObjectOpenShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (!(gxmGlobals.initFlags & SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC)) {
		GXM_DBG_ERROR_NOT_SHARED(107, sceGxmSyncObjectOpenShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (syncObject == NULL) {
		GXM_DBG_ERROR_NULL(113, sceGxmSyncObjectOpenShared, syncObject);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (key == 0) {
		GXM_DBG_ERROR_KEY(118, sceGxmSyncObjectOpenShared);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	opened = NULL;
	err = SceGpuEs4UserForVsh_8A27AB0A(&gxmGlobals.top, key, &opened);
	if (err != 0) {
		GXM_DBG_ERROR(122, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	if (opened == NULL) {
		GXM_DBG_ERROR(127, "Returned syncInfo from sceGxmSyncObjectOpenShared is NULL.");
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	*syncObject = opened;
	return 0;
}

int sceGxmVshSyncObjectClose(SceUInt32 key, void *syncObject)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(141, sceGxmSyncObjectCloseShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (syncObject == NULL) {
		GXM_DBG_ERROR_NULL(147, sceGxmSyncObjectCloseShared, syncObject);
		return SCE_GXM_ERROR_INVALID_POINTER;
	}
	if (key == 0) {
		GXM_DBG_ERROR_KEY(152, sceGxmSyncObjectCloseShared);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	err = SceGpuEs4UserForVsh_0A9556D9(&gxmGlobals.top, key, syncObject);
	if (err != 0) {
		GXM_DBG_ERROR(156, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}

int sceGxmVshSyncObjectDestroy(SceUInt32 key)
{
	int err;

	if (!gxmGlobals.initialized) {
		GXM_DBG_ERROR_UNINITIALIZED(168, sceGxmSyncObjectDeleteShared);
		return SCE_GXM_ERROR_UNINITIALIZED;
	}
	if (key == 0) {
		GXM_DBG_ERROR_KEY(173, sceGxmSyncObjectDeleteShared);
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	err = SceGpuEs4UserForVsh_010DA2CB(&gxmGlobals.top, key);
	if (err != 0) {
		GXM_DBG_ERROR(176, "Failed due to error in driver layer (%08X).", err);
		return SCE_GXM_ERROR_DRIVER;
	}
	return 0;
}
