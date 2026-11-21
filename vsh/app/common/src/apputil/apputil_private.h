/*
 * SceAppUtil's own declarations: the functions behind its exports, the checks they run on their
 * arguments, and the errors it translates. No record names them (names ours).
 */
#ifndef APPUTIL_PRIVATE_H
#define APPUTIL_PRIVATE_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <apputil/apputil_internal.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include <shell_svc/shell_util.h>

/* SceAppUtil's errors the SDK does not name */
#define APPUTIL_ERROR_SAVEDATA_SLOT_FILE	(-2146433282)	/* 0x801006fe */
#define APPUTIL_ERROR_INVALID_STRING		(-2146433281)	/* 0x801006ff: not UTF-8 */
#define APPUTIL_ERROR_LAUNCH_APP_REJECTED	(-2146433391)	/* 0x80100691 */
#define APPUTIL_ERROR_NULL_STATE		(-2146433328)	/* 0x801006d0 */
#define APPUTIL_ERROR_CACHE_DEVICE_NOT_FOUND	(-2146433407)	/* 0x80100681: no cache drive for the application */
#define APPUTIL_ERROR_UMASS_DEVICE_NOT_FOUND	(-2146433406)	/* 0x80100682: no memory card for the USB mass storage image */
#define APPUTIL_ERROR_UMASS_IMAGE_NOT_FOUND	(-2146433405)	/* 0x80100683: no image of that key */
#define APPUTIL_ERROR_UMASS_MOUNT_POINT	(-2146433327)	/* 0x801006d1: the image mounted elsewhere than lm */

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

/* SceAppMgr's errors SceAppUtil translates (names ours, from what the kernel returns them for) */
#define APPMGR_ERROR_LICENSE_NOT_FOUND	(-2139095036)	/* 0x80800004: no license file */
#define APPMGR_ERROR_ADDCONT_LOCATION	(-2139095026)	/* 0x8080000e: the additional content is not where the title may mount it */
#define APPMGR_ERROR_LICENSE_OPEN	(-2139095024)	/* 0x80800010: the license file cannot be read */
#define APPMGR_ERROR_LICENSE_EXPIRED	(-2139095023)	/* 0x80800011 */
#define APPMGR_ERROR_LICENSE_NOT_STARTED	(-2139095016)	/* 0x80800018: before the license's start time */
#define APPMGR_ERROR_CONTENT_INSTALL_BUSY	(-2139094975)	/* 0x80800041: an installation period of the title already open */
#define APPMGR_ERROR_LOOPBACK_BUSY	(-2139094970)	/* 0x80800046: another process formats the USB mass storage image */

/* SceNpDrm's license errors, which sceAppUtilDrmOpen reports as no entitlement (names ours) */
#define NPDRM_ERROR_DEBUG_LICENSE	(-2138636285)	/* 0x80870003 */
#define NPDRM_ERROR_NO_LICENSE	(-2138636284)	/* 0x80870004 */
#define NPDRM_ERROR_INVALID_LICENSE	(-2138636283)	/* 0x80870005 */
#define NPDRM_ERROR_NOT_ACTIVATED	(-2138636282)	/* 0x80870006 */
#define NPDRM_ERROR_ACTIVATION_TOO_OLD	(-2138636281)	/* 0x80870007 */
#define NPDRM_ERROR_ACCOUNT_MISMATCH	(-2138636280)	/* 0x80870008 */

/* the stack a launch request needs */
#define APPUTIL_LAUNCH_STACK_SIZE		3072

/* the calling thread's stack left below this frame, or the error getting its information */
static inline int getFreeStackSize(void)
{
	int here;
	SceKernelThreadInfo info;
	int ret;

	info.size = sizeof(info);
	ret = sceKernelGetThreadInfo(0, &info);
	if (ret >= 0)
		ret = (char *)&here - (char *)info.pStack;
	return ret;
}

/* SceAppUtil_C3CB128B's and SceAppUtil_C0418B1C's mount: the PSP save data root */
#define APPUTIL_MOUNT_PSP_SAVEDATA_ROOT		202

/* the mount IDs of the drives SceAppUtil mounts */
#define APPUTIL_MOUNT_ID_UMASS_FORMAT		100	/* sceAppMgrLoopBackFormat's */
#define APPUTIL_MOUNT_ID_MUSIC			105
#define APPUTIL_MOUNT_ID_BOOK			112
#define APPUTIL_MOUNT_ID_CACHE			205
#define APPUTIL_MOUNT_ID_UMASS			600

/* how the store and the sign-in dialog are launched */
#define APPUTIL_STORE_LAUNCH_MODE		0x60000
#define APPUTIL_NP_SIGNIN_LAUNCH_MODE		0x40000

/* SceAppUtilUmass_CC85F96F's kinds, sceAppMgrLoopBackFormat's commands one for one */
#define UMASS_FORMAT_CREATE			0
#define UMASS_FORMAT_QUERY			1
#define UMASS_FORMAT_DELETE			2
#define UMASS_FORMAT_OWNER			3
#define UMASS_FORMAT_SHRINK			4
/* the sizes SceAppUtilUmass_0BC38A89 lists */
#define UMASS_FORMAT_SIZES_MAX			16

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

/* the app event types the SDK header does not name */
#define SCE_APPUTIL_APPEVENT_TYPE_NP_BASIC_JOINABLE_PRESENCE	(3)
#define SCE_APPUTIL_APPEVENT_TYPE_NEAR_GIFT		(4)
#define SCE_APPUTIL_APPEVENT_TYPE_TRIGGER_UTIL		(6)	/* and USER_DEFINED before SDK 1.80 */
#define SCE_APPUTIL_APPEVENT_TYPE_INCOMING_DIALOG	(7)
#define SCE_APPUTIL_APPEVENT_TYPE_LAUNCH_APP		(9)
#define SCE_APPUTIL_APPEVENT_TYPE_USER_DEFINED		(10)
#define SCE_APPUTIL_APPEVENT_TYPE_WEBBROWSER		(15)

/* the parameters of the app events the SDK does not document (laid out from their parsers) */
typedef struct SceAppUtilNpBasicJoinablePresenceParam {
	SceNpCommunicationId commId;
	SceNpId npId;				/* from jid= */
} SceAppUtilNpBasicJoinablePresenceParam;

typedef struct SceAppUtilNearGiftParam {
	SceNpCommunicationId commId;
	SceUInt32 giftId;
	SceNpId npId;				/* the sender, from jid= */
	SceUInt32 version;
	SceUChar8 param[256];
} SceAppUtilNearGiftParam;

typedef struct SceAppUtilUserDefinedParam {
	SceChar8 arg[1000];
} SceAppUtilUserDefinedParam;

#define SCE_APPUTIL_WEBBROWSER_EVENT_TYPE_CALLBACK	1

typedef struct SceAppUtilWebBrowserEventParam {
	SceUInt32 type;				/* SCE_APPUTIL_WEBBROWSER_EVENT_TYPE_* */
	SceChar8 param[SCE_APPUTIL_LIVEAREA_PARAM_MAX_SIZE];
	SceChar8 reserved[32];
} SceAppUtilWebBrowserEventParam;

typedef struct SceAppUtilLaunchAppParam {
	SceChar8 uri[960];
	SceInt32 isTeleport;			/* from SDK 2.50: factor=TELEPORT */
} SceAppUtilLaunchAppParam;

typedef struct SceAppUtilIncomingDialogParam {
	SceInt32 isFromStartScreen;		/* from_start_screen=true */
	SceChar8 reserved[64];
	SceChar8 param[960];			/* from SDK 3.00 */
} SceAppUtilIncomingDialogParam;

#define SCE_APPUTIL_TRIGGER_UTIL_TYPE_PERIODIC_TIMER	1
#define SCE_APPUTIL_TRIGGER_UTIL_TYPE_TIME_OF_DAY_TIMER	2
#define SCE_APPUTIL_TRIGGER_UTIL_TYPE_DAILY_TIMER	3

typedef struct SceAppUtilTriggerUtilParam {
	SceUInt32 type;				/* SCE_APPUTIL_TRIGGER_UTIL_TYPE_* */
	SceUInt32 slot;
	SceChar8 reserved[64];
} SceAppUtilTriggerUtilParam;

/* app events the SDK header does not declare (its stubs name them) */
extern "C" SceInt32 sceAppUtilAppEventParseIncomingDialog(const SceAppUtilAppEventParam *eventParam,
							 SceAppUtilIncomingDialogParam *param);
extern "C" SceInt32 sceAppUtilAppEventParseTriggerUtil(const SceAppUtilAppEventParam *eventParam,
						      SceAppUtilTriggerUtilParam *param);

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
int umassFormat(int type, const char *key, AppmgrLoopBackImageInfo *info);
int umassMount(const char *key, SceAppUtilMountPoint *mountPoint);
int umassUmount(void);
int umassForceUmount(void);
int umassGetFormatSizes(SceUInt64 *sizes, SceUInt32 num);
int deviceGetDevInfo(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);
int cacheMount(void);
int cacheUmount(void);
int cacheGetDevInfo(SceUInt64 *maxSize, SceUInt64 *freeSize);
int bgdlGetStatus(SceAppUtilBgdlStatus *stat);
int bgdlGetAppStatus(SceInt32 *notReady, SceInt32 *ready);
int npSigninState(void);
int bookMount(SceAppUtilMountPoint *mountPoint);
int bookUmount(void);
int musicMount(void);
int musicUmount(void);
int _sceAppUtilLaunchWebBrowserCBLimited(const SceAppUtilWebBrowserCBLimitedParam *param);
int _sceAppUtilResetCookieWebBrowser(const SceAppUtilWebBrowserResetCookieParam *param);
int _sceAppUtilAddCookieWebBrowser(const SceAppUtilWebBrowserAddCookieParam *param);
int _sceAppUtilLaunchWebBrowser(const SceAppUtilWebBrowserParam *param);

/*
 * The web browser's files in the application's work directory (useragent.dat, cookie.dat): a
 * header, then the records one after the other
 */
#define APPUTIL_MOUNT_ID_WEBBROWSER		206
#define WEBBROWSER_FILE_MOUNT_WORKDIR		0
#define WEBBROWSER_URI_SIZE			1024
#define WEBBROWSER_URI_PART_SIZE		64

typedef struct WebBrowserFileHeader {
	SceUInt32 headerSize;		/* sizeof(WebBrowserFileHeader) */
	SceUInt32 recordsSize;		/* the bytes of records after the header */
	SceUInt32 count;		/* the records */
	SceUInt32 reserved;
} WebBrowserFileHeader;

typedef struct WebBrowserFileRecord {
	char uri[WEBBROWSER_URI_SIZE];	/* the page it is for */
	char value[4096];		/* the user agent, or the cookie's Set-Cookie string */
} WebBrowserFileRecord;

/* a file of records: Append adds one, refusing past maxCount (0: no limit) */
class WebBrowserFile {
public:
	WebBrowserFile();
	int Append(const char *path, WebBrowserFileHeader *header, const WebBrowserFileRecord *record,
		   SceUInt32 maxCount);
};

/* the URI a launch request builds: str copied at *cursor, both moved on by its length */
SceSize webBrowserUriAppend(char **cursor, const char *str, SceSize maxLength);
void webBrowserUriAdvance(char **cursor, SceSize *length, SceSize n);
/* the work directory a web browser file is kept in, mounted and unmounted; a file removed */
int webBrowserFileMount(int type, char *mountPoint);
int webBrowserFileUmount(const char *mountPoint);
int webBrowserFileRemove(const char *name);

/* the argument checks the add-on content and background download calls share */
int checkAddcontMountPoint(const char *mountPoint, int added);
int checkDrmAddcontId(const char *dirName);
int checkBgdlStatus(const SceAppUtilBgdlStatus *stat);

/* save data */
void getModifiedTime(SceDateTime *time);
void copySlotParamIn(const SceAppUtilSaveDataSlotParam *param, SceAppMgrSaveDataSlotParam *slotParam);
void copySlotParamOut(const SceAppMgrSaveDataSlotParam *slotParam, SceAppUtilSaveDataSlotParam *param);
int pspSaveDataRootMount(char *mountPoint);
int pspSaveDataRootUmount(void);

#endif /* APPUTIL_PRIVATE_H */
