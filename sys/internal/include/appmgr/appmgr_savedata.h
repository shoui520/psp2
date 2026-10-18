/*
 * SceAppMgr's save data records: the save data data calls (sceAppMgrSaveDataDataSave/Remove, the
 * system side of sceAppUtilSaveDataDataSave/Remove) and the save data slot calls. The items, mount
 * points and slot parameters are the SDK's (apputil.h); each AppMgr record wraps one call's.
 */

#ifndef _SCE_APPMGR_SAVEDATA_H
#define _SCE_APPMGR_SAVEDATA_H

#include <scetypes.h>
#include <apputil.h>

/*
 * A slot's parameters as SceAppMgr keeps them: the SDK's SceAppUtilSaveDataSlotParam, its
 * reserved bytes running to 1 KiB.
 */
typedef struct SceAppMgrSaveDataSlotParam {
	SceInt32 status;
	char title[SCE_APPUTIL_SAVEDATA_SLOT_TITLE_MAXSIZE];
	char subTitle[SCE_APPUTIL_SAVEDATA_SLOT_SUBTITLE_MAXSIZE];
	char detail[SCE_APPUTIL_SAVEDATA_SLOT_DETAIL_MAXSIZE];
	char iconPath[SCE_APPUTIL_SAVEDATA_SLOT_ICON_PATH_MAXSIZE];
	SceInt32 userParam;
	SceUInt32 sizeKiB;
	SceDateTime modifiedTime;
	SceUInt8 reserved[228];
} SceAppMgrSaveDataSlotParam;

_Static_assert(sizeof(SceAppMgrSaveDataSlotParam) == 1024, "SceAppMgrSaveDataSlotParam");

/* sceAppMgrSaveDataDataSave's record */
typedef struct SceAppMgrSaveDataData {
	SceSize size;				/* sizeof(SceAppMgrSaveDataData) */
	SceUInt32 slotId;			/* SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID: no slot */
	SceAppMgrSaveDataSlotParam *slotParam;	/* the slot's new parameters, or NULL */
	SceUInt8 reserved[32];
	const SceAppUtilSaveDataDataSaveItem *items;
	SceUInt32 itemNum;			/* at most SCE_APPUTIL_SAVEDATA_DATA_MAXNUM */
	SceAppUtilMountPoint mountPoint;
	SceUInt32 flags;			/* bit 0: data to write (0: a plain sync) */
	SceUInt32 requiredSizeKiB;		/* out: the space missing */
} SceAppMgrSaveDataData;

_Static_assert(sizeof(SceAppMgrSaveDataData) == 0x4c, "SceAppMgrSaveDataData");

/* sceAppMgrSaveDataDataRemove's record */
typedef struct SceAppMgrSaveDataDataDelete {
	SceSize size;				/* sizeof(SceAppMgrSaveDataDataDelete) */
	SceUInt32 slotId;
	SceAppMgrSaveDataSlotParam *slotParam;
	SceUInt8 reserved[32];
	const SceAppUtilSaveDataDataRemoveItem *items;
	SceUInt32 itemNum;
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataDataDelete;

_Static_assert(sizeof(SceAppMgrSaveDataDataDelete) == 0x44, "SceAppMgrSaveDataDataDelete");

/* a slot's parameters to create, set or get */
typedef struct SceAppMgrSaveDataSlot {
	SceSize size;				/* sizeof(SceAppMgrSaveDataSlot) */
	SceUInt32 slotId;
	SceAppMgrSaveDataSlotParam slotParam;
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataSlot;

_Static_assert(sizeof(SceAppMgrSaveDataSlot) == 0x418, "SceAppMgrSaveDataSlot");

/* a slot to delete */
typedef struct SceAppMgrSaveDataSlotDelete {
	SceSize size;				/* sizeof(SceAppMgrSaveDataSlotDelete) */
	SceUInt32 slotId;
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataSlotDelete;

_Static_assert(sizeof(SceAppMgrSaveDataSlotDelete) == 0x18, "SceAppMgrSaveDataSlotDelete");

int sceAppMgrSaveDataDataSave(SceAppMgrSaveDataData *data);
int sceAppMgrSaveDataDataRemove(SceAppMgrSaveDataDataDelete *data);

/* vitasdk: ksceAppMgrSaveDataSlotDelete */
int SceAppMgrForDriver_B544F2A0(SceAppMgrSaveDataSlotDelete *parameters);
/* not written in C yet: the prototype is its caller's (appmgr's system call) */
int SceAppMgrForDriver_D137D486(SceAppMgrSaveDataSlot *slot);	/* vitasdk: ksceAppMgrSaveDataSlotGetParam */

#endif /* _SCE_APPMGR_SAVEDATA_H */
