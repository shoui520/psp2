/*
 * apputil.h: the application utility's mount points and save data records, under Sony's names
 * and with Sony's definitions (SDK Release 03.570.011, target/include/apputil.h). SceAppMgr takes
 * these from the library's save data calls; the library's functions come with the user-mode SDK
 * headers.
 */
#ifndef _SCE_APPUTIL_APPUTIL_H
#define _SCE_APPUTIL_APPUTIL_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE		(16)

/* save data slots */
#define SCE_APPUTIL_SAVEDATA_SLOT_TITLE_MAXSIZE		(64)
#define SCE_APPUTIL_SAVEDATA_SLOT_SUBTITLE_MAXSIZE	(128)
#define SCE_APPUTIL_SAVEDATA_SLOT_DETAIL_MAXSIZE	(512)
#define SCE_APPUTIL_SAVEDATA_SLOT_ICON_PATH_MAXSIZE	(64)
#define SCE_APPUTIL_SAVEDATA_SLOT_MAX			(256)

#define SCE_APPUTIL_SAVEDATA_SLOT_STATUS_AVAILABLE	(0)
#define SCE_APPUTIL_SAVEDATA_SLOT_STATUS_BROKEN		(1)

#define SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID		(0xFFFFFFFF)

/* save data data (files and directories) */
#define SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE		(200)

#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE		(0)
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE	(1)	/* truncated at offset + bufSize */
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY		(2)

#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT		(0)
#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_KEEP_SLOT		(1)	/* the data only, not the slot */

#define SCE_APPUTIL_SAVEDATA_DATA_MAXNUM		(5)		/* items in one call */
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE		(1*1024*1024)	/* bytes written in one call */

typedef SceUInt32 SceAppUtilSaveDataSlotId;
typedef SceUInt32 SceAppUtilSaveDataSlotStatus;
typedef SceUInt32 SceAppUtilSaveDataDataSaveMode;
typedef SceUInt32 SceAppUtilSaveDataDataRemoveMode;

typedef struct SceAppUtilMountPoint {
	SceChar8 data[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
} SceAppUtilMountPoint;

typedef struct SceAppUtilSaveDataSlotParam {
	SceAppUtilSaveDataSlotStatus status;
	SceChar8 title[SCE_APPUTIL_SAVEDATA_SLOT_TITLE_MAXSIZE];		/* UTF-8 */
	SceChar8 subTitle[SCE_APPUTIL_SAVEDATA_SLOT_SUBTITLE_MAXSIZE];	/* UTF-8 */
	SceChar8 detail[SCE_APPUTIL_SAVEDATA_SLOT_DETAIL_MAXSIZE];		/* UTF-8 */
	SceChar8 iconPath[SCE_APPUTIL_SAVEDATA_SLOT_ICON_PATH_MAXSIZE];
	SceInt32 userParam;
	SceUInt32 sizeKiB;
	SceDateTime modifiedTime;
	SceChar8 reserved[48];
} SceAppUtilSaveDataSlotParam;

/* a file or directory to save */
typedef struct SceAppUtilSaveDataDataSaveItem {
	const SceChar8 *dataPath;
	const void *buf;
	SceSize bufSize;
	SceChar8 padding[4];
	SceOff offset;
	SceAppUtilSaveDataDataSaveMode mode;
	SceChar8 reserved[36];
} SceAppUtilSaveDataDataSaveItem;

/* a file or directory to remove */
typedef struct SceAppUtilSaveDataDataRemoveItem {
	const SceChar8 *dataPath;
	SceAppUtilSaveDataDataRemoveMode mode;
	SceChar8 reserved[36];
} SceAppUtilSaveDataDataRemoveItem;

#ifdef __cplusplus
}
#endif

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_APPUTIL_APPUTIL_H */
