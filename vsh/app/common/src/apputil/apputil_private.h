/*
 * SceAppUtil's own declarations: the functions behind its exports, the checks they run on their
 * arguments, and the errors it translates. No record names them (names ours).
 */
#ifndef APPUTIL_PRIVATE_H
#define APPUTIL_PRIVATE_H

#include <scetypes.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <apputil/apputil_internal.h>
#include <appmgr/appmgr_savedata.h>
#include <shell_svc/shell_util.h>

/* SceAppUtil's errors the SDK does not name */
#define APPUTIL_ERROR_SAVEDATA_SLOT_FILE	(-2146433282)	/* 0x801006fe */
#define APPUTIL_ERROR_INVALID_STRING		(-2146433281)	/* 0x801006ff: not UTF-8 */
#define APPUTIL_ERROR_LAUNCH_APP_REJECTED	(-2146433391)	/* 0x80100691 */
#define APPUTIL_ERROR_NULL_STATE		(-2146433328)	/* 0x801006d0 */

/* SceShellSvc's launch request errors */
#define SHELL_UTIL_ERROR_LAUNCH_APP_PARAMETER	(-2146428924)	/* 0x80101804 */
#define SHELL_UTIL_ERROR_LAUNCH_APP_REJECTED	(-2146428911)	/* 0x80101811 */

/* the errors SceAppMgr's PSP save data read passes on: the PSP save data's own (0x8012xxxx)
 * and SceAppMgr's */
#define PSP_SAVEDATA_ERROR_FIRST		(-2146303999)	/* 0x80120001 */
#define PSP_SAVEDATA_ERROR_LAST			(-2146303992)	/* 0x80120008 */
#define APPMGR_ERROR_PSP_SAVEDATA_BROKEN	(-2139094944)	/* 0x80800060 */
#define APPMGR_ERROR_PSP_SAVEDATA_NOT_FOUND	(-2139094943)	/* 0x80800061 */
#define APPMGR_ERROR_PSP_SAVEDATA_FILETYPE	(-2139094942)	/* 0x80800062 */
#define APPMGR_ERROR_PSP_SAVEDATA_DATABUF_SIZE	(-2139094941)	/* 0x80800063 */
#define APPMGR_ERROR_PSP_SAVEDATA_LAST		(-2139094937)	/* 0x80800067 */

/* the stack a launch request needs */
#define APPUTIL_LAUNCH_STACK_SIZE		3072

/* SceAppUtil_C3CB128B's and SceAppUtil_C0418B1C's mount: the PSP save data root */
#define APPUTIL_MOUNT_PSP_SAVEDATA_ROOT		202

/* a save data path: at most three folders, each name at most 63 bytes */
#define APPUTIL_DATA_NAME_MAXLEN		63
#define APPUTIL_DATA_PATH_DEPTH_MAX		3

/* what one save may write for a PS Now client (SCE_APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE otherwise) */
#define APPUTIL_SAVEDATA_DATA_SAVE_MAXSIZE_PSNOW	(10*1024*1024)

/* the save data drives a mount point argument may name */
#define MOUNT_POINT_ANY				0	/* NULL, savedata0: or savedata1: */
#define MOUNT_POINT_OWN				1	/* NULL or savedata0: */
#define MOUNT_POINT_ADD				2	/* savedata1:, the added title's, never NULL */

extern SceUInt32 g_sdkVersion;
extern int g_initialized;

/* the start of each part of the library (module_start) */
int umassInit(void);
int bookInit(void);
int musicInit(void);
int pspSaveDataInit(void);
int systemParamInit(void);

void paramError(int code);
void dataParamError(int code, int dataIndex);

/* the checks */
int checkTitleId(const char *titleId);
int checkPassCode(const SceUChar8 *passCode);
int checkSlotId(SceAppUtilSaveDataSlotId slotId);
int checkMountPoint(const char *mountPoint, int kind);
int checkSlotParam(const SceAppUtilSaveDataSlotParam *param);
int checkDataSlot(const SceAppUtilSaveDataDataSlot *slot);
int checkSaveItem(SceUInt32 index, const SceChar8 *dataPath, const void *buf, SceSize bufSize,
		  SceOff offset, const SceChar8 *padding, SceSize paddingSize, SceUInt32 mode,
		  const SceChar8 *reserved, SceSize reservedSize, const char *mountPoint, SceSize *size);
int checkSaveItems(const SceAppUtilSaveDataDataSaveItem *data, SceUInt32 dataNum,
		   const SceAppUtilMountPoint *mountPoint);
int checkRemoveItem(SceUInt32 index, const SceChar8 *dataPath, SceUInt32 mode,
		    const SceChar8 *reserved, SceSize reservedSize);
int checkRemoveItems(const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum);
int checkRequiredSize(const SceSize *requiredSizeKiB);
int checkQuota(const SceSize *quotaSizeKiB, const SceSize *usedSizeKiB);
int checkSearch(const SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
		const SceAppUtilSlotSearchResult *result);
int checkPspSaveDataParams(const SceAppUtilPspSaveDataParams *params);
int checkPspSaveDataDirName(const char *dirName);

/* the parameters of the app events the SDK does not document (laid out in their parsers) */
typedef struct SceAppUtilNpBasicJoinablePresenceParam SceAppUtilNpBasicJoinablePresenceParam;
typedef struct SceAppUtilNearGiftParam SceAppUtilNearGiftParam;
typedef struct SceAppUtilUserDefinedParam SceAppUtilUserDefinedParam;
typedef struct SceAppUtilWebBrowserEventParam SceAppUtilWebBrowserEventParam;
typedef struct SceAppUtilLaunchAppParam SceAppUtilLaunchAppParam;

/* the functions behind the exports */
int launchAppRequest(const SceShellUtilLaunchAppParam *param);
int saveDataMount(const SceAppUtilTitleId *titleId, int reserved, const SceAppUtilPassCode *passCode);
int saveDataUmount(void);
int saveDataSlotCreate(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param,
		       const SceAppUtilMountPoint *mountPoint);
int saveDataSlotDelete(SceAppUtilSaveDataSlotId slotId, const SceAppUtilMountPoint *mountPoint);
int saveDataSlotSearch(SceAppUtilWorkBuffer *workBuf, const SceAppUtilSaveDataSlotSearchCond *cond,
		       SceAppUtilSlotSearchResult *result, const SceAppUtilMountPoint *mountPoint);
int saveDataSlotSetParam(SceAppUtilSaveDataSlotId slotId, const SceAppUtilSaveDataSlotParam *param,
			 const SceAppUtilMountPoint *mountPoint);
int saveDataSlotGetParam(SceAppUtilSaveDataSlotId slotId, SceAppUtilSaveDataSlotParam *param,
			 const SceAppUtilMountPoint *mountPoint);
int saveDataDataSave(const SceAppUtilSaveDataDataSlot *slot, const SceAppUtilSaveDataDataSaveItem *data,
		     SceUInt32 dataNum, const SceAppUtilMountPoint *mountPoint, SceSize *requiredSizeKiB);
int saveDataDataRemove(const SceAppUtilSaveDataDataSlot *slot,
		       const SceAppUtilSaveDataDataRemoveItem *data, SceUInt32 dataNum,
		       const SceAppUtilMountPoint *mountPoint);
int saveDataGetQuota(SceSize *quotaSizeKiB, SceSize *usedSizeKiB, const SceAppUtilMountPoint *mountPoint);
int pspSaveDataLoad(SceAppUtilPspSaveDataParams *loadParam);
int pspSaveDataGetDirNameList(const SceAppUtilPspSaveDataDirName *prefix,
			      SceAppUtilPspSaveDataDirName *dirNameList, SceUInt32 dirNameListNum,
			      SceUInt32 *hitNum);
int photoMount(void);
int photoUmount(void);
int initSafeMemory(const char *id);
int saveSafeMemory(const void *buf, SceSize bufSize, SceOff offset);
int loadSafeMemory(void *buf, SceSize bufSize, SceOff offset);
int systemParamGetInt(SceSystemParamId paramId, SceInt32 *value);
int systemParamGetString(SceSystemParamId paramId, SceChar8 *buf, SceSize bufSize);
int appParamGetInt(SceAppUtilAppParamId paramId, SceInt32 *value);
int receiveAppEvent(SceAppUtilAppEventParam *eventParam);
int appEventParseNpInviteMessage(const SceAppUtilAppEventParam *eventParam,
				 SceAppUtilNpInviteMessageParam *param);
int appEventParseNpAppDataMessage(const SceAppUtilAppEventParam *eventParam,
				  SceAppUtilNpAppDataMessageParam *param);
int appEventParseNpBasicJoinablePresence(const SceAppUtilAppEventParam *eventParam,
					 SceAppUtilNpBasicJoinablePresenceParam *param);
int appEventParseNearGift(const SceAppUtilAppEventParam *eventParam, SceAppUtilNearGiftParam *param);
int appEventParseLiveArea(const SceAppUtilAppEventParam *eventParam, SceAppUtilLiveAreaParam *param);
int appEventParseNpActivity(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpActivityParam *param);
int appEventParseTeleport(const SceAppUtilAppEventParam *eventParam, SceAppUtilTeleportParam *param);
int appEventParseUserDefined(const SceAppUtilAppEventParam *eventParam,
			     SceAppUtilUserDefinedParam *param);
int appEventParseWebBrowser(const SceAppUtilAppEventParam *eventParam,
			    SceAppUtilWebBrowserEventParam *param);
int appEventParseScreenShotNotification(const SceAppUtilAppEventParam *eventParam,
					SceAppUtilScreenShotNotification *param);
int appEventParseLaunchApp(const SceAppUtilAppEventParam *eventParam, SceAppUtilLaunchAppParam *param);
int appEventParseSessionInvitation(const SceAppUtilAppEventParam *eventParam,
				   SceAppUtilSessionInvitationParam *param);
int appEventParseGameCustomData(const SceAppUtilAppEventParam *eventParam,
				SceAppUtilGameCustomDataParam *param);
int storeBrowse(SceAppUtilStoreBrowseParam *param);
int addcontMount(const SceAppUtilTitleId *titleId, int reserved, const SceAppUtilPassCode *passCode,
		 int force);
int addcontUmount(int reserved);
int drmOpen(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint);
int drmClose(const SceAppUtilDrmAddcontId *dirName, const SceAppUtilMountPoint *mountPoint);
int umassMount(void);
int umassMountByPath(const char *path, char *mountPoint);
int umassUmount(void);
int umassGetDevInfo(void);
int umassGetFreeSpace(SceUInt64 *freeSize, SceUInt32 reserved);
int deviceGetDevInfo(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);
int cacheMount(void);
int cacheUmount(void);
int cacheGetDevInfo(SceUInt64 *maxSize, SceUInt64 *freeSize);
int bgdlGetStatus(SceAppUtilBgdlStatus *stat);
int appVersionGet(SceInt32 *value, SceInt32 *value2);
int npSigninState(void);
int bookMount(void *param);
int bookUmount(void);
int musicMount(void);
int musicUmount(void);
int webBrowserCallback(int arg);
int resetCookieWebBrowser(SceAppUtilWebBrowserResetCookieParam *param);
int addCookieWebBrowser(SceAppUtilWebBrowserAddCookieParam *param);
int launchWebBrowser(SceAppUtilWebBrowserParam *param);

/* UTF-8 */
int utf8Scan(const SceChar8 *str, SceSize size, SceUInt32 *length, SceUInt32 *bom);
int utf8DecoderInit(void *context);
int utf8Decode(void *context, const SceChar8 *str, SceSize size, SceUInt32 *length, SceUInt8 *last);

/* save data */
void getModifiedTime(SceDateTime *time);
void copySlotParamIn(const SceAppUtilSaveDataSlotParam *param, SceAppMgrSaveDataSlotParam *slotParam);
void copySlotParamOut(const SceAppMgrSaveDataSlotParam *slotParam, SceAppUtilSaveDataSlotParam *param);
int pspSaveDataRootMount(char *mountPoint);
int pspSaveDataRootUmount(void);

#endif /* APPUTIL_PRIVATE_H */
