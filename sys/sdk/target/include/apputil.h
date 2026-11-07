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

/* error codes */
#define SCE_APPUTIL_ERROR_PARAMETER			(-2146433536)	/* 0x80100600 */
#define SCE_APPUTIL_ERROR_NOT_INITIALIZED		(-2146433535)	/* 0x80100601 */
#define SCE_APPUTIL_ERROR_NO_MEMORY			(-2146433534)	/* 0x80100602 */
#define SCE_APPUTIL_ERROR_BUSY				(-2146433533)	/* 0x80100603 */
#define SCE_APPUTIL_ERROR_NOT_MOUNTED			(-2146433532)	/* 0x80100604 */
#define SCE_APPUTIL_ERROR_NO_PERMISSION			(-2146433531)	/* 0x80100605 */
#define SCE_APPUTIL_ERROR_PASSCODE_MISMATCH		(-2146433530)	/* 0x80100606 */
#define SCE_APPUTIL_ERROR_STACKSIZE_TOO_SHORT		(-2146433376)	/* 0x801006a0 */
#define SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA	(-2146433504)	/* 0x80100620 */
#define SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS		(-2146433472)	/* 0x80100640 */
#define SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND	(-2146433471)	/* 0x80100641 */
#define SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_QUOTA	(-2146433470)	/* 0x80100642 */
#define SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS		(-2146433469)	/* 0x80100643 */
#define SCE_APPUTIL_ERROR_DRM_NO_ENTITLEMENT		(-2146433440)	/* 0x80100660 */
#define SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND	(-2146433408)	/* 0x80100680 */
#define SCE_APPUTIL_ERROR_MUSIC_DEVICE_NOT_FOUND	(-2146433403)	/* 0x80100685 */
#define SCE_APPUTIL_ERROR_MOUNT_LIMIT_OVER		(-2146433402)	/* 0x80100686 */

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

/* an empty slot as the save data dialog shows it */
typedef struct SceAppUtilSaveDataSlotEmptyParam {
	SceChar8 *title;
	SceChar8 *iconPath;
	void *iconBuf;
	SceSize iconBufSize;
	SceChar8 reserved[32];
} SceAppUtilSaveDataSlotEmptyParam;

/* a slot for the save data dialog */
typedef struct SceAppUtilSaveDataSlot {
	SceAppUtilSaveDataSlotId id;
	SceAppUtilSaveDataSlotStatus status;
	SceInt32 userParam;
	SceAppUtilSaveDataSlotEmptyParam *emptyParam;
} SceAppUtilSaveDataSlot;

/* the slot a save data data call updates */
typedef struct SceAppUtilSaveDataDataSlot {
	SceAppUtilSaveDataSlotId id;
	SceAppUtilSaveDataSlotParam *slotParam;		/* the slot's new parameters, or NULL */
	SceChar8 reserved[32];
} SceAppUtilSaveDataDataSlot;

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
