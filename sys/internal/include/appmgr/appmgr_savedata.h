/*
 * SceAppMgr's save data records: the save data data calls (sceAppMgrSaveDataDataSave/Remove, the
 * system side of sceAppUtilSaveDataDataSave/Remove) and the save data slot calls. The items, mount
 * points and slot parameters are the SDK's (apputil.h); each AppMgr record wraps one call's.
 */

#ifndef _SCE_APPMGR_SAVEDATA_H
#define _SCE_APPMGR_SAVEDATA_H

#include <stddef.h>
#include <scetypes.h>
#include <apputil.h>
#include <pfsmgr/pfsmgr.h>

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

/* a save data's quota and use */
typedef struct SceAppMgrSaveDataQuota {
	SceSize size;				/* sizeof(SceAppMgrSaveDataQuota) */
	SceSize quotaSizeKiB;			/* out */
	SceSize usedSizeKiB;			/* out */
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataQuota;

_Static_assert(sizeof(SceAppMgrSaveDataQuota) == 0x1c, "SceAppMgrSaveDataQuota");

/* host0 save data to mount on a devkit ("host0:savedata") */
typedef struct SceAppMgrSaveDataMountParam {
	SceSize size;				/* sizeof(SceAppMgrSaveDataMountParam) */
	char titleId[16];
	SceAppUtilMountPoint mountPoint;
	SceUInt8 reserved[24];
} SceAppMgrSaveDataMountParam;

_Static_assert(sizeof(SceAppMgrSaveDataMountParam) == 0x3c, "SceAppMgrSaveDataMountParam");

/* host0 save data to unmount */
typedef struct SceAppMgrSaveDataUmountParam {
	SceSize size;				/* sizeof(SceAppMgrSaveDataUmountParam) */
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataUmountParam;

_Static_assert(sizeof(SceAppMgrSaveDataUmountParam) == 0x14, "SceAppMgrSaveDataUmountParam");

/* sceAppMgrSaveDataSlotInit's record: the slots a process's save data has */
typedef struct AppmgrSaveDataSlotInitParam {
	SceSize size;
	SceUInt32 slotCount;
	SceUInt32 flags;
	SceUInt8 unused[32];
	SceAppUtilMountPoint mountPoint;
} AppmgrSaveDataSlotInitParam;

/* sceAppMgrSaveDataSlotFileOpen's view of the slot file */
typedef struct AppmgrSaveDataSlotFileInfo {
	SceUInt32 version;
	SceUInt32 slotCount;
	SceUInt8 unused[32];
	SceUInt8 occupied[512];
} AppmgrSaveDataSlotFileInfo;

_Static_assert(sizeof(AppmgrSaveDataSlotFileInfo) == 552,
	       "save-data slot file information");
_Static_assert(offsetof(AppmgrSaveDataSlotFileInfo, slotCount) == 4,
	       "save-data file information slot count");
_Static_assert(offsetof(AppmgrSaveDataSlotFileInfo, occupied) == 40,
	       "save-data file information occupancy table");
_Static_assert(sizeof(AppmgrSaveDataSlotInitParam) == 60,
	       "save-data slot initialization parameters");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, slotCount) == 4,
	       "save-data initialization slot count");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, flags) == 8,
	       "save-data initialization flags");
_Static_assert(offsetof(AppmgrSaveDataSlotInitParam, mountPoint) == 44,
	       "save-data initialization mount point");

/* _sceAppMgrPspSaveDataGetParams' buffer: SceAppUtilPspSaveDataParamSfo with a longer directory */
typedef struct AppmgrPspSaveDataParamSfo {
	char saveDataDirectory[64];
	char title[128];
	char saveDataTitle[128];
	char detail[1024];
	SceUInt8 parentalLev;
	char reserved[3];
} AppmgrPspSaveDataParamSfo;

_Static_assert(sizeof(AppmgrPspSaveDataParamSfo) == 1348, "PSP save data PARAM.SFO parameters");

/* _sceAppMgrPspSaveDataRead's parameters: SceAppUtilPspSaveDataParams, the file ID copied in */
typedef struct AppmgrPspSaveDataParams {
	char dirName[128];
	char fileName[13];
	char reserved[3];
	SceUInt32 fileType;
	SceUInt32 dataVersion;
	SceUInt8 secureFileId[16];
	AppmgrPspSaveDataParamSfo *paramSfo;
	void *dataBuf;
	SceSize dataBufSize;
} AppmgrPspSaveDataParams;

_Static_assert(sizeof(AppmgrPspSaveDataParams) == 180, "PSP save data read parameters");
_Static_assert(offsetof(AppmgrPspSaveDataParams, fileType) == 144, "PSP save data file type");
_Static_assert(offsetof(AppmgrPspSaveDataParams, paramSfo) == 168, "PSP save data PARAM.SFO buffer");

/* the parameters of the add mounts (add-on content, save data): the title, its passcode and the drive to use */
typedef struct AppmgrAddMountParam {
	char titleId[16];
	char passcode[32];
	ScePfsRndDriveId mountPoint;
} AppmgrAddMountParam;

_Static_assert(sizeof(AppmgrAddMountParam) == 64 &&
	       offsetof(AppmgrAddMountParam, passcode) == 16 &&
	       offsetof(AppmgrAddMountParam, mountPoint) == 48,
	       "add mount parameters");

#ifdef __cplusplus
extern "C" {
#endif

/* SceAppMgrUser's save data calls (driver_us), which SceAppUtil makes */
#if !defined(PSP2_KERNEL)
int sceAppMgrSaveDataSlotInit(AppmgrSaveDataSlotInitParam *param);
int sceAppMgrSaveDataSlotCreate(SceAppMgrSaveDataSlot *slot);
int sceAppMgrSaveDataSlotDelete(SceAppMgrSaveDataSlotDelete *slot);
int sceAppMgrSaveDataSlotSetParam(SceAppMgrSaveDataSlot *slot);
int sceAppMgrSaveDataSlotGetParam(SceAppMgrSaveDataSlot *slot);
int sceAppMgrSaveDataGetQuota(SceAppMgrSaveDataQuota *quota);
int sceAppMgrSaveDataSlotFileOpen(const char *mountPoint, AppmgrSaveDataSlotFileInfo *info);
int sceAppMgrSaveDataSlotFileGetParam(SceUInt32 slotId, SceAppMgrSaveDataSlotParam *param);
int sceAppMgrSaveDataSlotFileClose(const char *mountPoint);
int sceAppMgrSaveDataAddMount(const AppmgrAddMountParam *param);
int sceAppMgrAddContAddMount(const AppmgrAddMountParam *param, int add);
int sceAppMgrPspSaveDataRootMount(char *mountPoint);
int sceAppMgrPspSaveDataRead(AppmgrPspSaveDataParams *params);
#endif

int sceAppMgrSaveDataDataSave(SceAppMgrSaveDataData *data);
int sceAppMgrSaveDataDataRemove(SceAppMgrSaveDataDataDelete *data);
/* their second forms (SceDriverUser), which SceAppUtilExt calls */
int sceAppMgrSaveDataDataSave2(SceAppMgrSaveDataData *data);
int sceAppMgrSaveDataDataRemove2(SceAppMgrSaveDataDataDelete *data);
/* sceAppMgrSaveDataDataSave and sceAppMgrSaveDataDataRemove for system applications */
int SceAppMgrForDriver_30947DF4(SceAppMgrSaveDataData *operation);
int SceAppMgrForDriver_EBFE1BE2(SceAppMgrSaveDataDataDelete *operation);

/* vitasdk: ksceAppMgrSaveDataSlotDelete */
int SceAppMgrForDriver_B544F2A0(SceAppMgrSaveDataSlotDelete *parameters);
/* vitasdk: ksceAppMgrSaveDataGetQuota */
int SceAppMgrForDriver_E104C37E(SceAppMgrSaveDataQuota *quota);
/* vitasdk: ksceAppMgrSaveDataMount */
int SceAppMgrForDriver_E73C9516(const SceAppMgrSaveDataMountParam *param);
/* vitasdk: ksceAppMgrSaveDataUmount */
int SceAppMgrForDriver_2B080268(const SceAppMgrSaveDataUmountParam *param);
/* not written in C yet: the prototype is its caller's (appmgr's system call) */
int SceAppMgrForDriver_D137D486(SceAppMgrSaveDataSlot *slot);	/* vitasdk: ksceAppMgrSaveDataSlotGetParam */

#ifdef __cplusplus
}
#endif

#endif /* _SCE_APPMGR_SAVEDATA_H */
