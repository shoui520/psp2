/*
 * SceFios2Kernel: the kernel's FIOS2 overlays, as other modules use them (appmgr mounts
 * application paths with them, iofilemgr resolves paths through them).
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <kernel/iofilemgr_dirent.h>
#include <kernel/iofilemgr_stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the SDK's fios2_types.h */
typedef int32_t SceFiosOverlayID;

typedef enum SceFiosOverlayLimits {
	SCE_FIOS_OVERLAY_POINT_MAX = 292	/* an overlay's src and dst, with the NUL */
} SceFiosOverlayLimits;

typedef enum SceFiosOverlayType {
	SCE_FIOS_OVERLAY_TYPE_OPAQUE      = 0,	/* src replaces dst */
	SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT = 1,	/* reads src, then dst; writes dst */
	SCE_FIOS_OVERLAY_TYPE_NEWER       = 2,	/* reads the newer of src and dst; writes dst */
	SCE_FIOS_OVERLAY_TYPE_WRITABLE    = 3	/* reads src, then dst; writes src */
} SceFiosOverlayType;

/*
 * The SDK's SceFiosOverlay. Its reserved bytes are the kernel's: the lengths of the
 * normalized paths and the process the overlay belongs to (-1: every process).
 */
typedef struct SceFiosOverlay {
	uint8_t type;			/* SceFiosOverlayType */
	uint8_t order;
	uint16_t dst_len;
	uint16_t src_len;
	uint16_t reserved;
	SceUID pid;
	SceFiosOverlayID id;
	char dst[SCE_FIOS_OVERLAY_POINT_MAX];
	char src[SCE_FIOS_OVERLAY_POINT_MAX];
} SceFiosOverlay;

_Static_assert(sizeof(SceFiosOverlay) == 0x258, "SceFiosOverlay");

/* a directory handle listing a directory with the overlays merged in */
typedef struct SceFiosKernelDH SceFiosKernelDH;

/* vitasdk: ksceFiosKernelOverlayAdd */
int SceFios2KernelForDriver_2607EE4C(SceFiosOverlay *overlay, SceFiosOverlayID *outId);
/* vitasdk: ksceFiosKernelOverlayAddForProcess */
int SceFios2KernelForDriver_17E65A1C(SceUID pid, SceFiosOverlay *overlay, SceFiosOverlayID *outId);
/* vitasdk: ksceFiosKernelOverlayRemove */
int SceFios2KernelForDriver_2368FEB5(SceFiosOverlayID id);
/* vitasdk: ksceFiosKernelOverlayRemoveForProcess */
int SceFios2KernelForDriver_23247EFB(SceUID pid, SceFiosOverlayID id);
/* vitasdk: ksceFiosKernelOverlayGetInfo */
int SceFios2KernelForDriver_725E6817(SceFiosOverlayID id, SceFiosOverlay *outOverlay);
/* vitasdk: ksceFiosKernelOverlayGetInfoForProcess */
int SceFios2KernelForDriver_F1762BC2(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay);
/* vitasdk: ksceFiosKernelOverlayModify */
int SceFios2KernelForDriver_7F8B960C(SceFiosOverlayID id, SceFiosOverlay *overlay);
/* vitasdk: ksceFiosKernelOverlayModifyForProcess */
int SceFios2KernelForDriver_853EA82A(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *overlay);
/* vitasdk: ksceFiosKernelOverlayGetList */
int SceFios2KernelForDriver_FF42AAF0(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
				     SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount);
/* vitasdk: ksceFiosKernelOverlayResolveSync */
int SceFios2KernelForDriver_0F456345(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				     SceSize maxPath);
/* vitasdk: ksceFiosKernelOverlayResolveWithRangeSync */
int SceFios2KernelForDriver_D3D968FC(SceUID pid, int resolveFlag, const char *inPath,
				     char *outPath, SceSize maxPath, uint8_t minOrder,
				     uint8_t maxOrder);
/* vitasdk: ksceFiosKernelOverlayThreadSetDisabled */
int SceFios2KernelForDriver_03727E5E(int disabled);
int SceFios2KernelForDriver_E71192C5(void);	/* vitasdk: ksceFiosKernelOverlayThreadIsDisabled */
/* vitasdk: ksceFiosKernelOverlayGetRecommendedScheduler */
int SceFios2KernelForDriver_241BF0D6(int avail, const char *path);
int SceFios2KernelForDriver_3B329E86(SceFiosKernelDH **outDH, const char *path, unsigned int minOrder,
				     unsigned int maxOrder);
int SceFios2KernelForDriver_28E28A58(SceFiosKernelDH *dh, SceIoDirent *dirent);
int SceFios2KernelForDriver_DD7627EC(SceFiosKernelDH *dh);
int SceFios2KernelForDriver_2649408B(SceFiosKernelDH *dh, SceIoStat *stat);
int SceFios2KernelForDriver_35E7E75C(SceFiosKernelDH *dh, SceIoStat *stat, int bits);
int SceFios2KernelForDriver_990F46A5(SceFiosKernelDH *dh, int flag);

#ifdef __cplusplus
}
#endif
