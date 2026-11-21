/*
 * ScePhotoExport's session with the media service (SceAvMediaSrv's IPMI service) and what the
 * library sends it. Unlike SceMusicExport and SceVideoExport the library is C and calls the
 * IPMI client through its function table. No debug record names the types; the names are ours.
 */
#ifndef LIBSCEPHOTOEXPORT_PHOTO_EXPORT_INTERNAL_H
#define LIBSCEPHOTOEXPORT_PHOTO_EXPORT_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <photo_export.h>
#include <libScePhotoExport/photo_export.h>
#include "../../../../vsh/lowlayer/src/ipmi/client.h"

/* the media service's methods */
#define PHOTO_EXPORT_METHOD_SEND_DATA		0x41560000
#define PHOTO_EXPORT_METHOD_START		0x41560001
#define PHOTO_EXPORT_METHOD_GET_RESULT		0x41560002
#define PHOTO_EXPORT_METHOD_CANCEL		0x41560003
#define PHOTO_EXPORT_METHOD_ABORT		0x41560004
#define PHOTO_EXPORT_METHOD_GET_PROGRESS	0x41560005
#define PHOTO_EXPORT_METHOD_SET_THUMBNAIL	0x41560006
#define PHOTO_EXPORT_METHOD_START_VIDEO		0x4156000F
#define PHOTO_EXPORT_METHOD_GET_VIDEO_RESULT	0x41560010

#define PHOTO_EXPORT_MAX_ITEMS			10

/* what the service answers for an item */
typedef struct PhotoExportResult {
	char path[SCE_PHOTO_EXPORT_MAX_FS_PATH];
	SceInt32 contentId[2];
} PhotoExportResult;

/* an item of the batch export (ScePhotoExport_2EFB7250's input) */
typedef struct PhotoExportBatchItem {
	char path[SCE_PHOTO_EXPORT_MAX_FS_PATH];
	SceUInt32 version;
	const char *photoTitle;
	const char *gameTitle;
	const char *gameComment;
	SceInt32 attribute;		/* what the service gets as the item's attribute */
	SceUInt8 reserved[28];
	const void *pThumbnail;		/* the thumbnail's data, or NULL to use its path */
	SceInt32 thumbnailSize;
	const char *pThumbnailPath;
	SceInt32 reserved3C;
	SceInt32 duration[2];
} PhotoExportBatchItem;

/* what the service takes for an item of the batch */
typedef struct PhotoExportStartItem {
	char path[SCE_PHOTO_EXPORT_MAX_FS_PATH];
	char photoTitle[0x100];
	char gameTitle[0x100];
	char gameComment[0x200];
	SceInt32 attribute;
	SceInt32 reserved;
	SceInt32 duration[2];
} PhotoExportStartItem;

/* one connection to the media service */
typedef struct PhotoExportSession {
	IPMI::Client *pClient;
	SceInt32 requestId;		/* -1: no request yet */
	void *mspace;
	SceUID memblockId;		/* the memory block of the heap, 0 where the caller gave the memory */
	void *pClientBuffer;
	void *pWorkMemory;		/* the caller's heap */
	char *pPhotoTitle;		/* 0x100 bytes */
	char *pGameTitle;		/* 0x100 bytes */
	char *pGameComment;		/* 0x200 bytes */
	SceInt32 progress;
	SceInt32 weights[10];		/* the share of each item, 1..10 */
	const char *pFilePath;		/* the file being exported */
	SceInt32 mode;			/* 0: photo, 2: video, 3: cancel */
} PhotoExportSession;

#endif /* LIBSCEPHOTOEXPORT_PHOTO_EXPORT_INTERNAL_H */
