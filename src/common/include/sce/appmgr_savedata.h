/*
 * SceAppMgr savedata0: data calls (sceAppMgrSaveDataDataSave/Remove), the
 * system-side backend of sceAppUtilSaveDataDataSave/Remove.  The item
 * layouts are the SDK's (apputil.h); the AppMgr parameter blocks wrap one
 * item list each.
 */

#ifndef _SCE_APPMGR_SAVEDATA_H
#define _SCE_APPMGR_SAVEDATA_H

#include <stdint.h>
#include <psp2/types.h>

#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE		0
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE	1
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY		2

#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT		0
#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_KEEP_SLOT		1

#define SCE_APPMGR_SAVEDATA_SLOT_NONE		(-1)	/* data outside any slot */

typedef struct SceAppUtilMountPoint {
	char data[16];
} SceAppUtilMountPoint;

typedef struct SceAppUtilSaveDataDataSaveItem {
	const char *dataPath;		/* path below the mount point */
	const void *buf;
	SceSize bufSize;
	char padding[4];
	SceOff offset;
	int mode;			/* SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_* */
	char reserved[36];
} SceAppUtilSaveDataDataSaveItem;

typedef struct SceAppUtilSaveDataDataRemoveItem {
	const char *dataPath;
	int mode;			/* SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_* */
	char reserved[36];
} SceAppUtilSaveDataDataRemoveItem;

typedef struct SceAppMgrSaveDataData {
	int size;			/* sizeof(SceAppMgrSaveDataData) */
	int slotId;
	void *slotParam;
	char reserved[32];
	const SceAppUtilSaveDataDataSaveItem *items;
	int itemNum;
	SceAppUtilMountPoint mountPoint;
	int unk44;			/* 1 for writes, 0 for a plain sync */
	int unk48;
} SceAppMgrSaveDataData;

typedef struct SceAppMgrSaveDataDataDelete {
	int size;			/* sizeof(SceAppMgrSaveDataDataDelete) */
	int slotId;
	void *slotParam;
	char reserved[32];
	const SceAppUtilSaveDataDataRemoveItem *items;
	int itemNum;
	SceAppUtilMountPoint mountPoint;
} SceAppMgrSaveDataDataDelete;

int sceAppMgrSaveDataDataSave(SceAppMgrSaveDataData *data);
int sceAppMgrSaveDataDataRemove(SceAppMgrSaveDataDataDelete *data);

#endif /* _SCE_APPMGR_SAVEDATA_H */
