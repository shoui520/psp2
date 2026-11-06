/*
 * SceAppUtil's exports outside the SDK that SceAppUtilExt calls: the checks its save data data calls
 * run on their arguments, the library's state, and the notice it sends when a save data slot
 * changes. No record names them; the prototypes are SceAppUtil's (vs0:sys/external/apputil.suprx).
 */
#ifndef _SCE_APPUTIL_APPUTIL_INTERNAL_H
#define _SCE_APPUTIL_APPUTIL_INTERNAL_H

#include <scetypes.h>
#include <apputil.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a save data slot changed (SceAppUtil_F93C691C's event) */
#define SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT	1

/* the library's state: SCE_APPUTIL_ERROR_NOT_INITIALIZED before sceAppUtilInit */
typedef struct SceAppUtilInternalState {
	SceUInt32 sdkVersion;		/* the main module's */
	SceUInt8 reserved[60];
} SceAppUtilInternalState;

_Static_assert(sizeof(SceAppUtilInternalState) == 64, "SceAppUtilInternalState");

/* sceAppUtilSaveDataDataSave's and sceAppUtilSaveDataDataRemove's argument checks */
int SceAppUtil_E8CADCA9(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB);
int SceAppUtil_1CFFE892(const SceAppUtilSaveDataDataSlot *slot,
			const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
			const SceAppUtilMountPoint *mountPoint);
int SceAppUtil_A3E0694F(SceAppUtilInternalState *state);
/* passes the event to the callback the library was given, if any */
int SceAppUtil_F93C691C(int event, SceAppUtilSaveDataSlotId id);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_APPUTIL_APPUTIL_INTERNAL_H */
