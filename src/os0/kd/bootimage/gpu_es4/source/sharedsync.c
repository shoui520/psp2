/* SCE common/sharedsync.c: shared synchronization-handle registry. */
#include "sce_ddk_port.h"
#include <sce/debug.h>
#include <stddef.h>

typedef struct SharedSyncObject {
	IMG_UINT32 ui32SharedKey;
	IMG_UINT32 ui32RefCount;
	IMG_SID hHandle;
} SharedSyncObject;

static SharedSyncObject asSharedSyncObjects[8];

#if FW_DEBUG
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectDelete_L65 = {
	0x752211b6, 0x7a33cf6a, 0x2dd0dafa,
	"common/sharedsync.c", 65, "sceGpuSharedSyncObjectDelete"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectOpen_L89 = {
	0x752211b6, 0xe12ceaac, 0x49bb3d22,
	"common/sharedsync.c", 89, "sceGpuSharedSyncObjectOpen"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectOpen_L97 = {
	0x752211b6, 0x550c102d, 0x49bb3d22,
	"common/sharedsync.c", 97, "sceGpuSharedSyncObjectOpen"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectClose_L124 = {
	0x752211b6, 0x663c8eda, 0xacd0002e,
	"common/sharedsync.c", 124, "sceGpuSharedSyncObjectClose"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectGetHandle_L140 = {
	0x752211b6, 0xbe513a83, 0x14fbef50,
	"common/sharedsync.c", 140, "sceGpuSharedSyncObjectGetHandle"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectGetHandle_L148 = {
	0x752211b6, 0x4fe79ac5, 0x14fbef50,
	"common/sharedsync.c", 148, "sceGpuSharedSyncObjectGetHandle"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectClose_L118 = {
	0x752211b6, 0x208c891f, 0xacd0002e,
	"common/sharedsync.c", 118, "sceGpuSharedSyncObjectClose"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectCreate_L37 = {
	0x752211b6, 0x4b696fee, 0x5b513483,
	"common/sharedsync.c", 37, "sceGpuSharedSyncObjectCreate"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectCreate_L44 = {
	0x752211b6, 0x11459897, 0x5b513483,
	"common/sharedsync.c", 44, "sceGpuSharedSyncObjectCreate"
};
static const SceKernelDebugInfo dbg_sceGpuSharedSyncObjectDelete_L71 = {
	0x752211b6, 0xb9a30751, 0x2dd0dafa,
	"common/sharedsync.c", 71, "sceGpuSharedSyncObjectDelete"
};
#endif

SharedSyncObject *sceGpuSharedSyncObjectFind(IMG_UINT32 ui32SharedKey)
{
	IMG_UINT32 i;

	for (i = 0; i < 8; i++) {
		if (asSharedSyncObjects[i].ui32SharedKey == ui32SharedKey)
			return &asSharedSyncObjects[i];
	}
	return NULL;
}

IMG_INT sceGpuSharedSyncObjectCreate(IMG_UINT32 ui32SharedKey, IMG_SID hHandle)
{
	SharedSyncObject *psObject;

	if (sceGpuSharedSyncObjectFind(ui32SharedKey) != NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectCreate_L37,
				"%s: Sync object with key %08x was already registered!\n",
				"sceGpuSharedSyncObjectCreate", ui32SharedKey);
#endif
		return -1;
	}
	psObject = sceGpuSharedSyncObjectFind(0);
	if (psObject == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectCreate_L44,
				"%s: Too many shared sync objects!\n",
				"sceGpuSharedSyncObjectCreate");
#endif
		return -2;
	}
	psObject->ui32SharedKey = ui32SharedKey;
	psObject->ui32RefCount = 0;
	psObject->hHandle = hHandle;
	return 0;
}

IMG_INT sceGpuSharedSyncObjectDelete(IMG_UINT32 ui32SharedKey)
{
	SharedSyncObject *psObject = sceGpuSharedSyncObjectFind(ui32SharedKey);

	if (psObject == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectDelete_L65,
				"%s: Sync object with key %08x was not found!\n",
				"sceGpuSharedSyncObjectDelete", ui32SharedKey);
#endif
		return -3;
	}
	if (psObject->ui32RefCount != 0) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectDelete_L71,
				"%s: Sync object with key %08x is still referenced!\n",
				"sceGpuSharedSyncObjectDelete", ui32SharedKey);
#endif
		return -4;
	}
	psObject->ui32SharedKey = 0;
	psObject->ui32RefCount = 0;
	psObject->hHandle = 0;
	return 0;
}

IMG_INT sceGpuSharedSyncObjectOpen(IMG_UINT32 ui32SharedKey, IMG_SID *hHandle)
{
	SharedSyncObject *psObject;

	if (hHandle == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectOpen_L89,
				"%s: Invalid parameters\n",
				"sceGpuSharedSyncObjectOpen");
#endif
		return -5;
	}
	psObject = sceGpuSharedSyncObjectFind(ui32SharedKey);
	if (psObject == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectOpen_L97,
				"%s: Sync object with key %08x was not found!\n",
				"sceGpuSharedSyncObjectOpen", ui32SharedKey);
#endif
		return -3;
	}
	psObject->ui32RefCount++;
	*hHandle = psObject->hHandle;
	return 0;
}

IMG_INT sceGpuSharedSyncObjectClose(IMG_UINT32 ui32SharedKey)
{
	SharedSyncObject *psObject = sceGpuSharedSyncObjectFind(ui32SharedKey);

	if (psObject == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectClose_L118,
				"%s: Sync object with key %08x was not found!\n",
				"sceGpuSharedSyncObjectClose", ui32SharedKey);
#endif
		return -3;
	}
	if (psObject->ui32RefCount == 0) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectClose_L124,
				"%s: Sync object with key %08x was not opened!\n",
				"sceGpuSharedSyncObjectClose", ui32SharedKey);
#endif
		return -6;
	}
	psObject->ui32RefCount--;
	return 0;
}

IMG_INT sceGpuSharedSyncObjectGetHandle(IMG_UINT32 ui32SharedKey,
				      IMG_SID *hHandle)
{
	SharedSyncObject *psObject;

	if (hHandle == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectGetHandle_L140,
				"%s: Invalid parameters\n",
				"sceGpuSharedSyncObjectGetHandle");
#endif
		return -5;
	}
	psObject = sceGpuSharedSyncObjectFind(ui32SharedKey);
	if (psObject == NULL) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0,
				&dbg_sceGpuSharedSyncObjectGetHandle_L148,
				"%s: Sync object with key %08x was not found!\n",
				"sceGpuSharedSyncObjectGetHandle", ui32SharedKey);
#endif
		return -3;
	}
	*hHandle = psObject->hHandle;
	return 0;
}
