/*
 * photoimport_dialog.h: the photo import dialog's data, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/photoimport_dialog.h): what the shell gives
 * and reads back. The dialog's calls are added as code needs them.
 */
#ifndef _SCE_PHOTOIMPORT_DIALOG_H
#define _SCE_PHOTOIMPORT_DIALOG_H

#include <scetypes.h>
#include <common_dialog.h>

#ifdef __cplusplus
extern "C" {
#endif

/* type of category */
#define SCE_PHOTOIMPORT_DIALOG_CATEGORY_DEFAULT			(0x00000007U)
#define SCE_PHOTOIMPORT_DIALOG_CATEGORY_ALBUM_ALL		(0x00000001U)
#define SCE_PHOTOIMPORT_DIALOG_CATEGORY_ALBUM_CAMERA		(0x00000002U)
#define SCE_PHOTOIMPORT_DIALOG_CATEGORY_ALBUM_SCREENSHOT	(0x00000004U)

/* maximum size of path strings (device name and NUL terminator included) */
#define SCE_PHOTOIMPORT_DIALOG_MAX_FS_PATH			(1024)

/* maximum length of the photo title */
#define SCE_PHOTOIMPORT_DIALOG_MAX_PHOTO_TITLE_LENGTH		(64)
/* maximum size of the photo title (NUL terminator included) */
#define SCE_PHOTOIMPORT_DIALOG_MAX_PHOTO_TITLE_SIZE		(SCE_PHOTOIMPORT_DIALOG_MAX_PHOTO_TITLE_LENGTH * 4)

/* maximum number of items */
#define SCE_PHOTOIMPORT_DIALOG_MAX_ITEM_NUM			(1)

/* file format type */
typedef enum ScePhotoImportDialogFormatType {
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_UNKNOWN = 0,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_JPEG,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_PNG,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_GIF,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_BMP,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_TIFF,
	SCE_PHOTOIMPORT_DIALOG_FORMAT_TYPE_MPO
} ScePhotoImportDialogFormatType;

typedef SceInt32 ScePhotoImportDialogMode;

/* type of photo orientation */
typedef enum ScePhotoImportDialogOrientation {
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_UNKNOWN = 0,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_TOP_LEFT,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_TOP_RIGHT,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_BOTTOM_RIGHT,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_BOTTOM_LEFT,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_LEFT_TOP,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_RIGHT_TOP,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_RIGHT_BOTTOM,
	SCE_PHOTOIMPORT_DIALOG_ORIENTATION_LEFT_BOTTOM
} ScePhotoImportDialogOrientation;

/* additional information for a file */
typedef struct ScePhotoImportDialogFileDataSub {
	SceUInt32 width;				/* width of the image */
	SceUInt32 height;				/* height of the image */
	ScePhotoImportDialogFormatType format;		/* file format type */
	ScePhotoImportDialogOrientation orientation;	/* type of photo orientation */
	SceChar8 reserved[32];
} ScePhotoImportDialogFileDataSub;

/* a file imported */
typedef struct ScePhotoImportDialogFileData {
	SceChar8 fileName[SCE_PHOTOIMPORT_DIALOG_MAX_FS_PATH];			/* the file name */
	SceChar8 photoTitle[SCE_PHOTOIMPORT_DIALOG_MAX_PHOTO_TITLE_SIZE];	/* the title shown */
	SceChar8 reserved[32];
} ScePhotoImportDialogFileData;

/* an item imported */
typedef struct ScePhotoImportDialogItemData {
	ScePhotoImportDialogFileData fileData;
	ScePhotoImportDialogFileDataSub dataSub;
	SceChar8 reserved[32];
} ScePhotoImportDialogItemData;

/* structure for receiving the result of calling the function */
typedef struct ScePhotoImportDialogResult {
	SceInt32 result;
	SceUInt32 importedItemNum;
	SceChar8 reserved[32];
} ScePhotoImportDialogResult;

/* structure for calling scePhotoImportDialogInit() */
typedef struct ScePhotoImportDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	ScePhotoImportDialogMode mode;
	SceUInt32 visibleCategory;
	SceUInt32 itemCount;
	ScePhotoImportDialogItemData *itemData;
	SceChar8 reserved[32];
} ScePhotoImportDialogParam;

#ifdef __cplusplus
}
#endif

#endif /* _SCE_PHOTOIMPORT_DIALOG_H */
