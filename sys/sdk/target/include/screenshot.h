/*
 * screenshot.h: taking screenshots, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/screenshot.h). SceScreenShot exports the functions.
 */
#ifndef _SCE_SCREENSHOT_H
#define _SCE_SCREENSHOT_H

#include <scetypes.h>
#include <screenshot/error.h>

#ifdef __cplusplus
extern "C" {
#endif

/* maximum size of path strings (device name and NUL terminator included) */
#define SCE_SCREENSHOT_MAX_FS_PATH			(1024)

/* maximum length of the photo title */
#define SCE_SCREENSHOT_MAX_PHOTO_TITLE_LENGTH		(64)
/* maximum size of the photo title (NUL terminator included) */
#define SCE_SCREENSHOT_MAX_PHOTO_TITLE_SIZE		(SCE_SCREENSHOT_MAX_PHOTO_TITLE_LENGTH * 4)

/* maximum length of the game title */
#define SCE_SCREENSHOT_MAX_GAME_TITLE_LENGTH		(64)
/* maximum size of the game title (NUL terminator included) */
#define SCE_SCREENSHOT_MAX_GAME_TITLE_SIZE		(SCE_SCREENSHOT_MAX_GAME_TITLE_LENGTH * 4)

/* maximum length of the comment (description) */
#define SCE_SCREENSHOT_MAX_GAME_COMMENT_LENGTH		(128)
/* maximum size of the comment (NUL terminator included) */
#define SCE_SCREENSHOT_MAX_GAME_COMMENT_SIZE		(SCE_SCREENSHOT_MAX_GAME_COMMENT_LENGTH * 4)

/* mode of a capture */
typedef SceInt32 SceScreenShotCaptureMode;
#define SCE_SCREENSHOT_CAPTURE_MODE_NORMAL		(0)
#define SCE_SCREENSHOT_CAPTURE_MODE_FORCE_CAPTURE	(1)

/* screenshot parameter */
typedef struct SceScreenShotParam {
	const SceChar8 *photoTitle;	/* photo title */
	const SceChar8 *gameTitle;	/* game title */
	const SceChar8 *gameComment;	/* comment (description) */
	void *reserved;			/* reserved range (must be NULL) */
} SceScreenShotParam;

/* what a screenshot file says about the game */
typedef struct SceScreenShotGameInfo {
	SceChar8 photoTitle[SCE_SCREENSHOT_MAX_PHOTO_TITLE_SIZE];
	SceChar8 gameTitle[SCE_SCREENSHOT_MAX_GAME_TITLE_SIZE];
	SceChar8 gameComment[SCE_SCREENSHOT_MAX_GAME_COMMENT_SIZE];
	SceDateTime createdDate;	/* image created date */
} SceScreenShotGameInfo;

/* the capture file's information */
typedef struct SceScreenShotCaptureFileInfo {
	SceChar8 path[SCE_SCREENSHOT_MAX_FS_PATH];
} SceScreenShotCaptureFileInfo;

typedef SceBool (*SceScreenShotCaptureCancelFunc)(void *);

SceInt32 sceScreenShotSetParam(const SceScreenShotParam *param);

SceInt32 sceScreenShotSetOverlayImage(const SceChar8 *filePath, SceInt32 offsetX, SceInt32 offsetY);

SceInt32 sceScreenShotDisable(void);

SceInt32 sceScreenShotEnable(void);

SceInt32 sceScreenShotDisableNotification(void);

SceInt32 sceScreenShotEnableNotification(void);

SceInt32 sceScreenShotCapture(SceScreenShotCaptureMode mode, SceScreenShotCaptureFileInfo *captureFileInfo,
			      SceScreenShotCaptureCancelFunc cancelFunc, void *userdata);

SceInt32 sceScreenShotGetParam(const SceChar8 *filePath, SceScreenShotGameInfo *param);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_SCREENSHOT_H */
