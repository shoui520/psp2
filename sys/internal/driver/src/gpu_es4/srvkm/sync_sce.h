/* IMG servicesint.h sync owner, with the SCE shared-resource extension.
 * Pristine upstream remains in contrib/img-sgx-ddk-1.7.862890.
 */
#ifndef SYNC_SCE_H
#define SYNC_SCE_H

#include <gpu/memory_sce.h>

typedef struct PVRSRV_KERNEL_SYNC_INFO {
	PVRSRV_SYNC_DATA *psSyncData;
	IMG_DEV_VIRTADDR sWriteOpsCompleteDevVAddr;
	IMG_DEV_VIRTADDR sReadOpsCompleteDevVAddr;
	PVRSRV_KERNEL_MEM_INFO *psSyncDataMemInfoKM;
	IMG_UINT32 ui32RefCount;
	struct Resource *hResItem;
	/* SCE: separate cleanup resource for the shared object. */
	struct Resource *hSharedResItem;
	IMG_UINT32 ui32UID;
	/* SCE: key passed to sceGpuSharedSyncObjectCreate/Open/Close. */
	IMG_UINT32 ui32SharedKey;
} PVRSRV_KERNEL_SYNC_INFO;

/* SCE: one modify object snapshots up to four sync objects. */
typedef struct _MODIFY_SYNC_OP_INFO {
	struct Resource *hResItem;
	IMG_UINT32 ui32NumSyncs;
	PVRSRV_KERNEL_SYNC_INFO *apsKernelSyncInfo[4];
	IMG_UINT32 ui32ModifyFlags;
	IMG_UINT32 aui32ReadOpsPendingSnapShot[4];
	IMG_UINT32 aui32WriteOpsPendingSnapShot[4];
	IMG_UINT32 ui32WaitState;
	IMG_UINT32 reserved;
} MODIFY_SYNC_OP_INFO;

#endif
