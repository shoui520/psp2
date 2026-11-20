/*
 * apputil.h: the application utility (SceAppUtil), under Sony's names and with Sony's
 * definitions, in Sony's order (SDK Release 03.570.011, target/include/apputil.h).
 */
#ifndef _SCE_APPUTIL_APPUTIL_H
#define _SCE_APPUTIL_APPUTIL_H

#include <scetypes.h>
#include <kernel/types.h>
#include <system_param.h>
#include <np/np_common.h>
#include <np/np_message.h>

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

/* drives */
#define SCE_APPUTIL_APP_DRIVE_NAME			"app0:"
#define SCE_APPUTIL_SAVEDATA_DRIVE_NAME			"savedata0:"
#define SCE_APPUTIL_SAVEDATA_DRIVE_NAME_ADD		"savedata1:"
#define SCE_APPUTIL_ADDCONT_DRIVE_NAME			"addcont0:"
#define SCE_APPUTIL_ADDCONT_DRIVE_NAME_ADD		"addcont1:"
#define SCE_APPUTIL_PHOTO_DRIVE_NAME			"photo0:"
#define SCE_APPUTIL_MUSIC_DRIVE_NAME			"music0:"

/* application events */
#define SCE_APPUTIL_APPEVENT_TYPE_INVALID		(0)
#define SCE_APPUTIL_APPEVENT_TYPE_NP_INVITE_MESSAGE	(1)
#define SCE_APPUTIL_APPEVENT_TYPE_NP_APP_DATA_MESSAGE	(2)
#define SCE_APPUTIL_APPEVENT_TYPE_LIVEAREA		(5)
#define SCE_APPUTIL_APPEVENT_TYPE_SCREENSHOT_NOTIFICATION (8)
#define SCE_APPUTIL_APPEVENT_TYPE_NP_ACTIVITY		(11)
#define SCE_APPUTIL_APPEVENT_TYPE_TELEPORT		(12)
#define SCE_APPUTIL_APPEVENT_TYPE_SESSION_INVITATION	(13)
#define SCE_APPUTIL_APPEVENT_TYPE_GAME_CUSTOM_DATA	(14)

#define SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE		(1024)
#define SCE_APPUTIL_LIVEAREA_PARAM_MAX_SIZE		(1019)
#define SCE_APPUTIL_NP_ACTIVITY_PARAM_MAXSIZE		(897)
#define SCE_APPUTIL_TELEPORT_PARAM_MAXSIZE		(952)

#define SCE_APPUTIL_TITLE_ID_DATA_SIZE			(10)
#define SCE_APPUTIL_PASSCODE_DATA_SIZE			(32)
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

#define SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EXIST_SLOT	(0)
#define SCE_APPUTIL_SAVEDATA_SLOT_SEARCH_TYPE_EMPTY_SLOT	(1)

#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SLOT_ID		(0)
#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_USER_PARAM		(1)
#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_SIZE_KIB		(2)
#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_KEY_MODIFIED_TIME	(3)

#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_ASCENT		(0)
#define SCE_APPUTIL_SAVEDATA_SLOT_SORT_TYPE_DESCENT		(1)

#define SCE_APPUTIL_WORKBUF_SEARCH_SLOT_DEFAULT_ELEMENT_SIZE		(16)
#define SCE_APPUTIL_WORKBUF_SEARCH_SLOT_BY_SIZE_KIB_ELEMENT_SIZE	(20)
#define SCE_APPUTIL_WORKBUF_SEARCH_SLOT_BY_MODIFIEDTIME_ELEMENT_SIZE	(32)

/* save data data (files and directories) */
#define SCE_APPUTIL_SAVEDATA_DATA_PATH_MAXSIZE		(200)

#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE		(0)
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_FILE_TRUNCATE	(1)	/* truncated at offset + bufSize */
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MODE_DIRECTORY		(2)

#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_DEFAULT		(0)
#define SCE_APPUTIL_SAVEDATA_DATA_REMOVE_MODE_KEEP_SLOT		(1)	/* the data only, not the slot */

#define SCE_APPUTIL_SAVEDATA_DATA_MAXNUM		(5)		/* items in one call */
#define SCE_APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE		(1*1024*1024)	/* bytes written in one call */

#define SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE		(17)

/* application parameters */
#define SCE_APPUTIL_APPPARAM_ID_SKU_FLAG		(0)

#define SCE_APPUTIL_APPPARAM_SKU_FLAG_NONE		(0)
#define SCE_APPUTIL_APPPARAM_SKU_FLAG_TRIAL		(1)
#define SCE_APPUTIL_APPPARAM_SKU_FLAG_FULL		(3)

#define SCE_APPUTIL_SAFEMEMORY_MEMORY_SIZE		(64*1024)

/* the store */
#define SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT2		(3)
#define SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY2		(4)
#define SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE2	(5)

/* the web browser */
#define SCE_APPUTIL_WEBBROWSER_LAUNCH_URL_MAXSIZE	(960)

#define SCE_APPUTIL_WEBBROWSER_LAUNCH_APP_NORMAL	(0)
#define SCE_APPUTIL_WEBBROWSER_LAUNCH_APP_MODAL		(1)

#define SCE_APPUTIL_WEBBROWSER_LAUNCH_CMD_OPENURL	(0 << 2)
#define SCE_APPUTIL_WEBBROWSER_LAUNCH_CMD_SEARCH	(1 << 2)

/* background downloads */
#define SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT		(0)
#define SCE_APPUTIL_BGDL_STATUS_TYPE_ADDCONT_AND_LICENSE (1)

/* screenshots */
#define SCE_APPUTIL_SCREENSHOT_TRIGGER_TYPE_USER	(0)
#define SCE_APPUTIL_SCREENSHOT_TRIGGER_TYPE_IN_GAME	(1)

#define SCE_APPUTIL_SCREENSHOT_CONTENT_PATH_MAXSIZE	(1024)

typedef SceUInt32 SceAppUtilBootAttribute;
typedef SceUInt32 SceAppUtilAppEventType;
typedef SceUInt32 SceAppUtilSaveDataSlotId;
typedef SceUInt32 SceAppUtilSaveDataSlotStatus;
typedef SceUInt32 SceAppUtilSaveDataDataSaveMode;
typedef SceUInt32 SceAppUtilSaveDataDataRemoveMode;
typedef SceUInt32 SceAppUtilSaveDataSlotSearchType;
typedef SceUInt32 SceAppUtilSaveDataSlotSortKey;
typedef SceUInt32 SceAppUtilSaveDataSlotSortType;
typedef SceUInt32 SceAppUtilAppParamId;
typedef SceUInt32 SceAppUtilBgdlStatusType;
typedef SceUInt32 SceAppUtilScreenShotTriggerType;

typedef struct SceAppUtilInitParam {
	SceSize workBufSize;
	SceChar8 reserved[60];
} SceAppUtilInitParam;

typedef struct SceAppUtilWorkBuffer {
	void *buf;
	SceUInt32 bufSize;
	SceChar8 reserved[32];
} SceAppUtilWorkBuffer;

typedef struct SceAppUtilBootParam {
	SceAppUtilBootAttribute attr;
	SceUInt32 appVersion;
	SceChar8 reserved[32];
} SceAppUtilBootParam;

typedef struct SceAppUtilAppEventParam {
	SceAppUtilAppEventType type;
	SceUChar8 data[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
} SceAppUtilAppEventParam;

typedef struct SceAppUtilNpInviteMessageParam {
	SceNpCommunicationId commId;
	SceUChar8 uid[SCE_NP_MESSAGE_MAX_MESSAGE_ID_SIZE + 1];
} SceAppUtilNpInviteMessageParam;

typedef struct SceAppUtilNpAppDataMessageParam {
	SceNpCommunicationId commId;
	SceUChar8 uid[SCE_NP_MESSAGE_MAX_MESSAGE_ID_SIZE + 1];
} SceAppUtilNpAppDataMessageParam;

typedef struct SceAppUtilLiveAreaParam {
	SceUChar8 param[SCE_APPUTIL_LIVEAREA_PARAM_MAX_SIZE];
	SceChar8 reserved[32];
} SceAppUtilLiveAreaParam;

typedef struct SceAppUtilScreenShotNotification {
	SceAppUtilScreenShotTriggerType triggerType;
	SceUChar8 contentPath[SCE_APPUTIL_SCREENSHOT_CONTENT_PATH_MAXSIZE];
	SceChar8 reserved[32];
} SceAppUtilScreenShotNotification;

typedef struct SceAppUtilNpActivityParam {
	SceChar8 param[SCE_APPUTIL_NP_ACTIVITY_PARAM_MAXSIZE];
	SceChar8 reserved[32];
} SceAppUtilNpActivityParam;

typedef struct SceAppUtilTeleportParam {
	SceChar8 param[SCE_APPUTIL_TELEPORT_PARAM_MAXSIZE];
	SceChar8 reserved[32];
} SceAppUtilTeleportParam;

typedef struct SceAppUtilMountPoint {
	SceChar8 data[SCE_APPUTIL_MOUNTPOINT_DATA_MAXSIZE];
} SceAppUtilMountPoint;

typedef struct SceAppUtilTitleId {
	SceChar8 data[SCE_APPUTIL_TITLE_ID_DATA_SIZE];
	SceChar8 padding[6];
} SceAppUtilTitleId;

typedef struct SceAppUtilPassCode {
	SceUChar8 data[SCE_APPUTIL_PASSCODE_DATA_SIZE];
} SceAppUtilPassCode;

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

typedef struct SceAppUtilSaveDataSlot {
	SceAppUtilSaveDataSlotId id;
	SceAppUtilSaveDataSlotStatus status;
	SceInt32 userParam;
	SceAppUtilSaveDataSlotEmptyParam *emptyParam;
} SceAppUtilSaveDataSlot;

typedef struct SceAppUtilSaveDataSlotSearchCond {
	SceAppUtilSaveDataSlotSearchType type;
	SceAppUtilSaveDataSlotId from;
	SceUInt32 range;
	SceAppUtilSaveDataSlotSortKey key;
	SceAppUtilSaveDataSlotSortType order;
	SceChar8 reserved[32];
} SceAppUtilSaveDataSlotSearchCond;

typedef struct SceAppUtilSlotSearchResult {
	SceUInt32 hitNum;
	SceAppUtilSaveDataSlot *slotList;
	SceChar8 reserved[32];
} SceAppUtilSlotSearchResult;

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

/* the slot a save data data call updates */
typedef struct SceAppUtilSaveDataDataSlot {
	SceAppUtilSaveDataSlotId id;
	SceAppUtilSaveDataSlotParam *slotParam;		/* the slot's new parameters, or NULL */
	SceChar8 reserved[32];
} SceAppUtilSaveDataDataSlot;

typedef struct SceAppUtilDrmAddcontId {
	SceChar8 data[SCE_APPUTIL_NP_DRM_ADDCONT_ID_SIZE];
	SceChar8 padding[3];
} SceAppUtilDrmAddcontId;

typedef struct SceAppUtilStoreBrowseParam {
	SceUInt32 type;
	const SceChar8 *id;
} SceAppUtilStoreBrowseParam;

typedef struct SceAppUtilWebBrowserParam {
	const SceChar8 *wbstr;
	SceUInt32 wbstrLength;
	SceUInt32 launchMode;
	SceUInt32 reserved0;
} SceAppUtilWebBrowserParam;

typedef struct SceAppUtilWebBrowserResetCookieParam {
	SceChar8 reserved[32];
} SceAppUtilWebBrowserResetCookieParam;

typedef struct SceAppUtilWebBrowserAddCookieParam {
	const SceChar8 *wbstr;
	const SceChar8 *wbstrName;
	const SceChar8 *wbstrValue;
	const SceChar8 *wbstrExpires;
	const SceChar8 *wbstrPath;
	const SceChar8 *wbstrDomain;
	SceBool isSecure;
	SceChar8 reserved[32];
} SceAppUtilWebBrowserAddCookieParam;

typedef struct SceAppUtilBgdlStatus {
	SceAppUtilBgdlStatusType type;
	SceUInt32 addcontNumReady;
	SceUInt32 addcontNumNotReady;
	SceUInt32 licenseReady;
	SceChar8 reserved[28];
} SceAppUtilBgdlStatus;

typedef struct SceAppUtilSessionInvitationParam {
	SceNpSessionId sessionId;
	SceNpInvitationId invitationId;
} SceAppUtilSessionInvitationParam;

typedef struct SceAppUtilGameCustomDataParam {
	SceNpGameCustomDataId gameCustomDataId;
} SceAppUtilGameCustomDataParam;

SceInt32 sceAppUtilInit(const SceAppUtilInitParam *initParam, SceAppUtilBootParam *bootParam);
SceInt32 sceAppUtilShutdown(void);
SceInt32 sceAppUtilReceiveAppEvent(SceAppUtilAppEventParam *eventParam);
SceInt32 sceAppUtilAppEventParseNpInviteMessage(const SceAppUtilAppEventParam *eventParam,
						SceAppUtilNpInviteMessageParam *param);
SceInt32 sceAppUtilAppEventParseNpAppDataMessage(const SceAppUtilAppEventParam *eventParam,
						 SceAppUtilNpAppDataMessageParam *param);
SceInt32 sceAppUtilAppEventParseLiveArea(const SceAppUtilAppEventParam *eventParam,
					 SceAppUtilLiveAreaParam *param);
SceInt32 sceAppUtilAppEventParseScreenShotNotification(const SceAppUtilAppEventParam *eventParam,
						       SceAppUtilScreenShotNotification *param);
SceInt32 sceAppUtilAppEventParseNpActivity(const SceAppUtilAppEventParam *eventParam,
					   SceAppUtilNpActivityParam *param);
SceInt32 sceAppUtilAppEventParseTeleport(const SceAppUtilAppEventParam *eventParam,
					 SceAppUtilTeleportParam *param);
SceInt32 sceAppUtilSaveDataMount(const SceAppUtilTitleId *titleId,
				 const SceAppUtilPassCode *passCode);
SceInt32 sceAppUtilSaveDataUmount();
SceInt32 sceAppUtilSaveDataSlotCreate(SceAppUtilSaveDataSlotId slotId,
				      const SceAppUtilSaveDataSlotParam *param,
				      const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataSlotDelete(SceAppUtilSaveDataSlotId slotId,
				      const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataSlotSearch(SceAppUtilWorkBuffer *workBuf,
				      const SceAppUtilSaveDataSlotSearchCond *cond,
				      SceAppUtilSlotSearchResult *result,
				      const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataSlotSetParam(SceAppUtilSaveDataSlotId slotId,
					const SceAppUtilSaveDataSlotParam *param,
					const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataSlotGetParam(SceAppUtilSaveDataSlotId slotId,
					SceAppUtilSaveDataSlotParam *param,
					const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataDataSave(const SceAppUtilSaveDataDataSlot *slot,
				    const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
				    const SceAppUtilMountPoint *mountPoint,
				    SceSize *requiredSizeKiB);
SceInt32 sceAppUtilSaveDataDataRemove(const SceAppUtilSaveDataDataSlot *slot,
				      const SceAppUtilSaveDataDataRemoveItem *data,
				      SceUInt32 dataNum, const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilSaveDataGetQuota(SceSize *quotaSizeKiB, SceSize *usedSizeKiB,
				    const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilAddcontMount(const SceAppUtilTitleId *titleId,
				const SceAppUtilPassCode *passCode);
SceInt32 sceAppUtilAddcontUmount();
SceInt32 sceAppUtilDrmOpen(const SceAppUtilDrmAddcontId *dirName,
			   const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilDrmClose(const SceAppUtilDrmAddcontId *dirName,
			    const SceAppUtilMountPoint *mountPoint);
SceInt32 sceAppUtilPhotoMount(void);
SceInt32 sceAppUtilPhotoUmount(void);
SceInt32 sceAppUtilMusicMount(void);
SceInt32 sceAppUtilMusicUmount(void);
SceInt32 sceAppUtilSystemParamGetInt(SceSystemParamId paramId, SceInt32 *value);
SceInt32 sceAppUtilSystemParamGetString(SceSystemParamId paramId, SceChar8 *buf,
					SceSize bufSize);
SceInt32 sceAppUtilAppParamGetInt(SceAppUtilAppParamId paramId, SceInt32 *value);
SceInt32 sceAppUtilSaveSafeMemory(const void *buf, SceSize bufSize, SceOff offset);
SceInt32 sceAppUtilLoadSafeMemory(void *buf, SceSize bufSize, SceOff offset);
SceInt32 sceAppUtilStoreBrowse(SceAppUtilStoreBrowseParam *param);
SceInt32 sceAppUtilLaunchWebBrowser(SceAppUtilWebBrowserParam *param);
SceInt32 sceAppUtilResetCookieWebBrowser(SceAppUtilWebBrowserResetCookieParam *param);
SceInt32 sceAppUtilAddCookieWebBrowser(SceAppUtilWebBrowserAddCookieParam *param);
SceInt32 sceAppUtilBgdlGetStatus(SceAppUtilBgdlStatus *stat);
SceInt32 sceAppUtilAppEventParseSessionInvitation(const SceAppUtilAppEventParam *eventParam,
						  SceAppUtilSessionInvitationParam *param);
SceInt32 sceAppUtilAppEventParseGameCustomData(const SceAppUtilAppEventParam *eventParam,
					       SceAppUtilGameCustomDataParam *param);

/* deprecated store browse types */
#define SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT		(0)
#define SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY		(1)
#define SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE	(2)

#ifdef __cplusplus
}
#endif

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_APPUTIL_APPUTIL_H */
