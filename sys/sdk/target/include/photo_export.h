/*
 * photo_export.h: exporting a photo to the system's photo content, under Sony's names and with
 * Sony's definitions (SDK Release 03.570.011, target/include/photo_export.h).
 * ScePhotoExport exports the functions.
 */
#ifndef _SCE_PHOTO_EXPORT_PHOTO_EXPORT_H
#define _SCE_PHOTO_EXPORT_PHOTO_EXPORT_H

#include <scetypes.h>
#include <photo_export/error.h>

#ifdef __cplusplus
extern "C" {
#endif

/* maximum size of path strings (device name and NUL terminator included) */
#define SCE_PHOTO_EXPORT_MAX_FS_PATH			(1024)

/* maximum length of the photo title */
#define SCE_PHOTO_EXPORT_MAX_PHOTO_TITLE_LENGTH		(64)
/* maximum size of the photo title (NUL terminator included) */
#define SCE_PHOTO_EXPORT_MAX_PHOTO_TITLE_SIZE		(SCE_PHOTO_EXPORT_MAX_PHOTO_TITLE_LENGTH * 4)

/* maximum length of the game title */
#define SCE_PHOTO_EXPORT_MAX_GAME_TITLE_LENGTH		(64)
/* maximum size of the game title (NUL terminator included) */
#define SCE_PHOTO_EXPORT_MAX_GAME_TITLE_SIZE		(SCE_PHOTO_EXPORT_MAX_GAME_TITLE_LENGTH * 4)

/* maximum length of the comment (description) */
#define SCE_PHOTO_EXPORT_MAX_GAME_COMMENT_LENGTH	(128)
/* maximum size of the comment (NUL terminator included) */
#define SCE_PHOTO_EXPORT_MAX_GAME_COMMENT_SIZE		(SCE_PHOTO_EXPORT_MAX_GAME_COMMENT_LENGTH * 4)

/* maximum size of the memory block */
#define SCE_PHOTO_EXPORT_MAX_MEMBLOCK_SIZE		(64 * 1024)

/* the metadata of an export */
typedef struct ScePhotoExportParam {
	SceUInt32 version;		/* SDK version */
	const SceChar8 *photoTitle;	/* photo title the system software shows */
	const SceChar8 *gameTitle;	/* game title (album name) */
	const SceChar8 *gameComment;	/* comment (description) */
	SceChar8 reserved[32];		/* reserved range (must be filled by zero) */
} ScePhotoExportParam;

typedef SceBool (*ScePhotoExportCancelFunc)(void *);

/* photo export by data pointer */
SceInt32 scePhotoExportFromData(const void *photodata, SceInt32 photodataSize,
				const ScePhotoExportParam *param, void *workMemory,
				ScePhotoExportCancelFunc cancelFunc, void *userdata,
				SceChar8 *exportedPath, SceInt32 exportedPathLength);

/* photo export by file path */
SceInt32 scePhotoExportFromFile(const SceChar8 *photodataPath, const ScePhotoExportParam *param,
				void *workMemory, ScePhotoExportCancelFunc cancelFunc,
				void *userdata, SceChar8 *exportedPath,
				SceInt32 exportedPathLength);

/* whether the photo application can show the data */
SceInt32 scePhotoExportIsAvailableFromData(const void *photodata, SceInt32 photodataSize,
					   SceBool *isAvailable);

/* whether the photo application can show the file */
SceInt32 scePhotoExportIsAvailableFromFile(const SceChar8 *filePath, SceBool *isAvailable);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_PHOTO_EXPORT_PHOTO_EXPORT_H */
