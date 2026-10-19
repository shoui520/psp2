/* IMG sgx_mkif_km.h shared 2D command, with SCE status extension. */
#ifndef SGXTRANSFER_SCE_H
#define SGXTRANSFER_SCE_H

#include "sgxkick_sce.h"

typedef struct _SGXMKIF_2DCMD_SHARED_ {
	IMG_UINT32 ui32NumSrcSync;
	PVRSRV_DEVICE_SYNC_OBJECT sSrcSyncData[3];
	PVRSRV_DEVICE_SYNC_OBJECT sDstSyncData;
	PVRSRV_DEVICE_SYNC_OBJECT sTASyncData;
	PVRSRV_DEVICE_SYNC_OBJECT s3DSyncData;
	/* SCE: one status update accompanies the hardware 2D command. */
	IMG_UINT32 ui32NumStatusVals;
	CTL_STATUS sCtlStatusInfo;
} SGXMKIF_2DCMD_SHARED;

/* Host-facing firmware command. The command stream is supplied at runtime;
 * the final slot accommodates the terminator appended by the host.
 */
typedef struct SGXMKIF_2DCMD {
	IMG_UINT32 ui32CommandSize;
	IMG_UINT32 firmwarePrivateState[2];
	IMG_UINT32 ui32NumCommands;
	IMG_UINT32 aui32Commands[26];
	SGXMKIF_2DCMD_SHARED sShared;
} SGXMKIF_2DCMD;

_Static_assert(sizeof(SGXMKIF_2DCMD_SHARED) == 112, "SCE shared 2D command");
_Static_assert(sizeof(SGXMKIF_2DCMD) == 232, "hardware 2D command host interface");

/* sgxtransfer_sce.c */
PVRSRV_ERROR SGXSubmit2DKM(PVRSRV_DEVICE_NODE *psDeviceNode, PVRSRV_2D_SGX_KICK *psKick);

#endif
