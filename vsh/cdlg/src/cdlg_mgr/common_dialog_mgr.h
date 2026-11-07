/*
 * The common dialog's work area: SceCommonDialog (libcdlg) keeps it and hands
 * it to SceCommonDialogMain (libcdlg_main) when the PafMain thread loads that
 * module; the main module fills in the callbacks the PafMain thread runs. One
 * slot per dialog that can be open at once: the first dialog, and the dialogs
 * nested on it. No Sony record names the structures or their fields: the
 * names are ours, after their readers and writers in both modules.
 */
#ifndef CDLG_MGR_COMMON_DIALOG_MGR_H
#define CDLG_MGR_COMMON_DIALOG_MGR_H

#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <cameraimport_dialog.h>
#include <photoimport_dialog.h>
#include <photoreview_dialog.h>
#include <videoimport_dialog.h>
#include <store_checkout_dialog.h>
#include <np/np_common.h>
#include <net.h>
#include <libkernel/libkernel.h>

#ifdef __cplusplus
namespace paf {
class Plugin;
}
typedef paf::Plugin CommonDialogPlugin;
#else
typedef struct CommonDialogPlugin CommonDialogPlugin;
#endif

#define COMMON_DIALOG_SLOT_NUM	3

/* CommonDialogSlot::dialogType: the dialogs by their plugins */
enum CommonDialogType {
	COMMON_DIALOG_TYPE_MSG = 1,
	COMMON_DIALOG_TYPE_SAVEDATA = 2,
	COMMON_DIALOG_TYPE_NETCHECK = 3,
	COMMON_DIALOG_TYPE_IME = 4,
	COMMON_DIALOG_TYPE_NP_MESSAGE = 5,
	COMMON_DIALOG_TYPE_INVITATION = 6,
	COMMON_DIALOG_TYPE_GAME_CUSTOM_DATA = 7,
	COMMON_DIALOG_TYPE_FRIENDLIST = 9,
	COMMON_DIALOG_TYPE_TROPHY_SETUP = 10,
	COMMON_DIALOG_TYPE_PHOTOIMPORT = 11,
	COMMON_DIALOG_TYPE_NPPROFILE2 = 12,
	COMMON_DIALOG_TYPE_PHOTOREVIEW = 13,
	COMMON_DIALOG_TYPE_CHECKOUT = 14,
	COMMON_DIALOG_TYPE_CAMERAIMPORT = 15,
	COMMON_DIALOG_TYPE_COMPAT = 16,
	COMMON_DIALOG_TYPE_NP_SNS_FB = 17,
	COMMON_DIALOG_TYPE_NPEULA = 18,
	COMMON_DIALOG_TYPE_CROSS_CONTROLLER = 19,
	COMMON_DIALOG_TYPE_TWITTER = 20,
	COMMON_DIALOG_TYPE_TW_LOGIN = 21,
	COMMON_DIALOG_TYPE_NEAR = 22,
	COMMON_DIALOG_TYPE_FRIENDLIST2 = 23,
	COMMON_DIALOG_TYPE_COMPANION = 24,
	COMMON_DIALOG_TYPE_REMOTE_OSK = 25,
	COMMON_DIALOG_TYPE_CALENDAR_REVIEW = 26,
	COMMON_DIALOG_TYPE_POCKETSTATION = 27,
	COMMON_DIALOG_TYPE_VIDEOIMPORT = 28
};

/* CommonDialogSlot::state: where the slot's dialog is (0: the slot is free) */
enum CommonDialogSlotState {
	COMMON_DIALOG_SLOT_FREE = 0,
	COMMON_DIALOG_SLOT_RESERVED = 1,
	COMMON_DIALOG_SLOT_IDLE = 2,
	COMMON_DIALOG_SLOT_LOADING = 3,
	COMMON_DIALOG_SLOT_RUNNING = 4,
	COMMON_DIALOG_SLOT_CLOSING = 5,
	COMMON_DIALOG_SLOT_UNLOADED = 6,
	COMMON_DIALOG_SLOT_FINISHED = 7
};

/* the common dialog's internal errors (not in the SDK's common_dialog/error.h) */
#define COMMON_DIALOG_ERROR_LOCK_BUSY		-2147351423	/* 0x80020481 */
#define COMMON_DIALOG_ERROR_NO_MEMORY		-2147351420	/* 0x80020484 */
#define COMMON_DIALOG_ERROR_SLOT_NOT_FOUND	-2147351417	/* 0x80020487 */
#define COMMON_DIALOG_ERROR_SLOT_NOT_RUNNING	-2147351407	/* 0x80020491 */
#define COMMON_DIALOG_ERROR_LOCK_RECURSIVE	-2147351391	/* 0x800204A1 */
#define COMMON_DIALOG_ERROR_LOCK_NOT_OWNED	-2147351390	/* 0x800204A2 */
#define COMMON_DIALOG_ERROR_INTERNAL		-2147351361	/* 0x800204BF */

/* the PafMain thread's callbacks, set by SceCommonDialogMain (kept as void * in the work
   area, CommonDialogWork::initFunc ...) */
typedef int (*CommonDialogInitFunc)(int dialogType);
typedef int (*CommonDialogTermFunc)(int dialogType);
typedef int (*CommonDialogLoadFunc)(int dialogType);
typedef int (*CommonDialogStopFunc)(int dialogType);	/* SceCommonDialogMain's ignores it */
typedef int (*CommonDialogRunFunc)(void);
typedef int (*CommonDialogUpdateFunc)(const void *updateParam);

typedef struct CommonDialogSlot {
	int dialogType;
	/* SceCommonDialogStatus: what the dialog's GetStatus returns */
	int dialogStatus;
	int userData;
	/* CommonDialogSlotState */
	int32_t state;
	SceUInt8 reserved16[4];
	/* the work memory the dialog's Init allocates (AllocateWorkMemory), and its base */
	SceUID workMemoryBlock;
	void *workMemory;
	SceUInt8 reserved28[4];
	bool paramReady;
	SceUInt8 reserved33[3];
	CommonDialogPlugin *plugin;
	SceUInt8 reserved40[8];
	SceKernelLwMutexWork mutex;
	SceUID doneSema;
	char pluginName[32];
	SceUInt8 reserved116[4];
} CommonDialogSlot;

typedef struct CommonDialogColor {
	SceUInt8 r;
	SceUInt8 g;
	SceUInt8 b;
	SceUInt8 a;
} CommonDialogColor;

/* SceCommonDialogConfigParam, as sceCommonDialogSetConfigParam copies it */
typedef struct CommonDialogConfig {
	SceUInt32 sdkVersion;
	int language;
	int enterButtonAssign;
	SceUInt8 reserved[32];
} CommonDialogConfig;

typedef struct CommonDialogWork {
	bool hasConfig;
	SceUInt8 reserved1[3];
	CommonDialogConfig config;
	bool hasDimmerColor;
	CommonDialogColor dimmerColor;
	SceUInt8 reserved53[3];
	int drawPriority;
	/* the dialog runs in a game's common dialog mode (sceKernelIsCDialogAvailable, paf loaded):
	   only the PafMain thread drives it, and the input stays intercepted */
	bool cdialogMode;
	/* the 1280x720 and the 1440 or 1920x1080 render targets are accepted */
	bool allow720p;
	bool allow1080p;
	SceUInt8 reserved63[1];
	int32_t openCount;
	SceUInt32 lastSlot;
	CommonDialogSlot slot[COMMON_DIALOG_SLOT_NUM];
	bool frameworkShared;
	bool pafLoaded;
	SceUInt8 reserved434[2];
	SceUID pafMainThread;
	SceUID frameworkSema;
	void *initFunc;
	void *termFunc;
	void *loadFunc;
	void *stopFunc;
	void *runFunc;
	void *updateFunc;
	SceUInt8 reserved468[4];
	/* "SceCommonDialogPafUpdateMutex": the update callback against the PafMain thread's term */
	SceKernelLwMutexWork updateMutex;
	/* the main module's flags, cleared when its work ends (clearMainWorkFlags) */
	bool mainWorkFlags[3];
	SceUInt8 reserved507[1];
	/* the free budget at module start, and the main module's SDK version */
	SceSize initialBudgetFree;
	SceUInt32 sdkVersion;
	/* SceCommonDialogFocusStatus: whether the system shows a focus */
	int focusState;
} CommonDialogWork;

/* the layout both modules share */
#define COMMON_DIALOG_LAYOUT_CHECK(name, cond)	typedef char name[(cond) ? 1 : -1]
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSlotSize, sizeof(CommonDialogSlot) == 120);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogWorkSlot, __builtin_offsetof(CommonDialogWork, slot) == 72);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogWorkUpdateMutex,
			   __builtin_offsetof(CommonDialogWork, updateMutex) == 0x1d8);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogWorkSize, sizeof(CommonDialogWork) == 0x208);

/*
 * The parameters libcdlg keeps for the dialogs whose plugins draw at the
 * caller's priority (CommonDialogSlot::workMemory): the SDK's parameter copied in,
 * and what libcdlg adds. Only the draw priority SceCommonDialogMain reads is
 * laid out yet; the rest comes with libcdlg's dialogs.
 */
/* photoimport_dialog.cpp: the caller's parameter, the result for GetResult and what the
   plugin draws with */
typedef struct CommonDialogPhotoImportParam {
	ScePhotoImportDialogParam param;
	ScePhotoImportDialogResult result;
	SceCommonDialogColor bgColor;
	int infobarShown;
	int drawPriority;
	/* the API call that holds the dialog (CommonDialogApiScope) */
	char apiName[64];
} CommonDialogPhotoImportParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogPhotoImportDrawPriority,
			   __builtin_offsetof(CommonDialogPhotoImportParam, drawPriority) == 176);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogPhotoImportSize, sizeof(CommonDialogPhotoImportParam) == 244);

/* photoreview_dialog.cpp: the caller's parameter, the result for GetResult, and a copy
   of the export metadata with its strings, which the copied parameter points to */
typedef struct CommonDialogPhotoReviewParam {
	ScePhotoReviewDialogParam param;
	ScePhotoReviewDialogResult result;
	SceCommonDialogColor bgColor;
	int infobarShown;
	ScePhotoReviewDialogExportParam exportParam;
	SceChar8 photoTitle[SCE_PHOTOREVIEW_DIALOG_MAX_PHOTO_TITLE_SIZE];
	SceChar8 gameTitle[SCE_PHOTOREVIEW_DIALOG_MAX_GAME_TITLE_SIZE];
	SceChar8 gameComment[SCE_PHOTOREVIEW_DIALOG_MAX_GAME_COMMENT_SIZE];
	int drawPriority;
	/* the API call that holds the dialog (CommonDialogApiScope) */
	char apiName[64];
} CommonDialogPhotoReviewParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogPhotoReviewDrawPriority,
			   __builtin_offsetof(CommonDialogPhotoReviewParam, drawPriority) == 2260);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogPhotoReviewSize, sizeof(CommonDialogPhotoReviewParam) == 2328);

/* cameraimport_dialog.cpp: the caller's parameter, copies of what it points to (which
   the copied parameter points to instead), the result for GetResult and the draw priority */
typedef struct CommonDialogCameraImportParam {
	SceCameraImportDialogParam param;
	SceCameraImportDialogMemoryRawParam memRawParam;
	SceCameraImportDialogMemoryJpegParam memJpegParam;
	SceCameraImportDialogOverlayImage overlayImage;
	SceCommonDialogColor bgColor;
	SceCameraImportDialogResult result;
	int drawPriority;
	/* the API call that holds the dialog (CommonDialogApiScope) */
	char apiName[64];
} CommonDialogCameraImportParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCameraImportDrawPriority,
			   __builtin_offsetof(CommonDialogCameraImportParam, drawPriority) == 424);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCameraImportSize, sizeof(CommonDialogCameraImportParam) == 492);

/* npeula_dialog.cpp: the plugin's result and what it needs of the
   caller's parameter */
typedef struct CommonDialogNpEulaParam {
	SceInt32 result;
	SceInt32 errorCode;
	SceNpCommunicationId npCommId;
	SceUInt32 version;
	SceUInt32 resultVersion;
	int drawPriority;
	SceUInt8 option;
} CommonDialogNpEulaParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpEulaDrawPriority,
			   __builtin_offsetof(CommonDialogNpEulaParam, drawPriority) == 28);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpEulaSize, sizeof(CommonDialogNpEulaParam) == 36);

/*
 * cross_controller_dialog.cpp: the PS3 title the dialog connects to and the
 * connection's state and result, which the plugin fills. OnInit constructs
 * it in the work memory; the plugin (vsh/cdlg/src/cross_controller_dialog)
 * opens the socket, reads the caller's own version and title ID from its
 * application parameters and keeps its dialog here. The names are ours:
 * nothing names the fields the plugin alone reads and writes.
 */
#ifdef __cplusplus
/* placement new, the copy paf's <new> keeps out of line */
#include <std/new>

class CommonDialogCrossControllerParam {
public:
	CommonDialogCrossControllerParam(const char *ps3Title, const char *appMinVer, bool isCancelable)
	{
		socketId = -1;
		sceClibMemset(&inaddr, 0, sizeof(inaddr));
		appVersion = 0;
		sceClibMemset(titleId, 0, sizeof(titleId));
		sceClibMemset(ps3AppMinVer, 0, sizeof(ps3AppMinVer));
		sceClibMemset(ps3TitleId, 0, sizeof(ps3TitleId));
		sceClibMemset(peerName, 0, sizeof(peerName));
		sceClibStrncpy(ps3AppMinVer, appMinVer, sizeof(ps3AppMinVer) - 1);
		sceClibStrncpy(ps3TitleId, ps3Title, sizeof(ps3TitleId) - 1);
		result = SCE_OK;
		dialogId = -1;
		cancelable = isCancelable;
	}

	/* the UDP socket the plugin broadcasts on */
	SceInt32 socketId;
	/* the PS3 that answered */
	SceNetInAddr inaddr;
	/* the caller's own version (its application parameter 3, as 100 * major + minor) and title ID */
	SceInt32 appVersion;
	char titleId[9 + 1];
	char ps3AppMinVer[5 + 1];
	char ps3TitleId[9 + 1];
	/* the PS3's name, from its answer */
	char peerName[128];
	/* the dialog's plugin, once it has started */
	paf::Plugin *plugin;
	int drawPriority;
	SceInt32 result;
	/* SceCommonGuiDialog's waiting dialog, -1 without one */
	SceInt32 dialogId;
	bool cancelable;
};

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCrossControllerDrawPriority,
			   __builtin_offsetof(CommonDialogCrossControllerParam, drawPriority) == 172);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCrossControllerSize, sizeof(CommonDialogCrossControllerParam) == 188);
#endif

/* videoimport_dialog.cpp: the caller's parameter and the result for GetResult */
typedef struct CommonDialogVideoImportParam {
	SceVideoImportDialogParam param;
	SceVideoImportDialogResult result;
	SceCommonDialogColor bgColor;
	int infobarShown;
	int drawPriority;
	/* the API call that holds the dialog (CommonDialogApiScope) */
	char apiName[64];
} CommonDialogVideoImportParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogVideoImportDrawPriority,
			   __builtin_offsetof(CommonDialogVideoImportParam, drawPriority) == 284);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogVideoImportSize, sizeof(CommonDialogVideoImportParam) == 0x160);

/* the calendar review dialog's parameter: no SDK header has it; the dialog's
   own part only its plugin reads */
typedef struct CalendarReviewDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt8 dialogData[1064];
} CalendarReviewDialogParam;

typedef struct CalendarReviewDialogResult {
	SceInt32 result;
	SceUInt8 dialogData[32];
} CalendarReviewDialogResult;

/* calendar_review_dialog.cpp: the caller's parameter and the result for GetResult */
typedef struct CommonDialogCalendarReviewParam {
	CalendarReviewDialogParam param;
	CalendarReviewDialogResult result;
	SceCommonDialogColor bgColor;
	int infobarShown;
	int drawPriority;
	char apiName[64];
} CommonDialogCalendarReviewParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCalendarReviewDrawPriority,
			   __builtin_offsetof(CommonDialogCalendarReviewParam, drawPriority) == 1188);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCalendarReviewSize, sizeof(CommonDialogCalendarReviewParam) == 1256);

/* the companion util dialog's parameter, its Init2 argument and its result: no SDK
   header has them; the dialog's own parts only its plugin reads */
typedef struct CompanionUtilDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt8 dialogData[8];
	SceUInt8 reserved[32];
} CompanionUtilDialogParam;

typedef struct CompanionUtilDialogInitArg {
	SceUInt8 dialogData[40];
} CompanionUtilDialogInitArg;

typedef struct CompanionUtilDialogResult {
	SceInt32 result;
	SceUInt8 dialogData[6];
	SceUInt8 reserved[32];
	SceUInt8 padding[2];
} CompanionUtilDialogResult;

/* companion_util_dialog.cpp: the caller's parameter, the Init2 argument and the
   result for GetResult */
typedef struct CommonDialogCompanionParam {
	CompanionUtilDialogParam param;
	CompanionUtilDialogInitArg initArg;
	CompanionUtilDialogResult result;
	int drawPriority;
	char apiName[64];
} CommonDialogCompanionParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCompanionDrawPriority,
			   __builtin_offsetof(CommonDialogCompanionParam, drawPriority) == 204);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCompanionSize, sizeof(CommonDialogCompanionParam) == 272);

/* the remote OSK dialog's parameter and result: no SDK header has them; the
   dialog's own parts only its plugin reads */
typedef struct RemoteOskDialogParam {
	SceUInt32 sdkVersion;
	const SceWChar16 *title;
	SceCommonDialogParam commonParam;
	char reserved[35];
	SceUInt8 padding;
} RemoteOskDialogParam;

typedef struct RemoteOskDialogResult {
	SceInt32 result;
	SceUInt8 dialogData[8];
	char reserved[28];
} RemoteOskDialogResult;

#define REMOTE_OSK_DIALOG_TITLE_LENGTH		128

/* remote_osk_dialog.cpp: the caller's parameter with its title, the plugin's
   state and the result for GetResult */
typedef struct CommonDialogRemoteOskParam {
	/* Abort asked the plugin to close, and the plugin's result for it */
	int abortRequested;
	int abortResult;
	bool infobarShown;
	SceUInt8 reservedByte;
	SceWChar16 title[REMOTE_OSK_DIALOG_TITLE_LENGTH];
	SceUInt8 reservedTitle[2];
	int paramSet;
	RemoteOskDialogParam param;
	RemoteOskDialogResult result;
	int drawPriority;
	char apiName[64];
} CommonDialogRemoteOskParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogRemoteOskDrawPriority,
			   __builtin_offsetof(CommonDialogRemoteOskParam, drawPriority) == 432);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogRemoteOskSize, sizeof(CommonDialogRemoteOskParam) == 500);

/* the SKU IDs the work memory keeps, and their length */
#define STORE_CHECKOUT_DIALOG_SKU_NUM_MAX		16
#define STORE_CHECKOUT_DIALOG_SKU_ID_LEN		56
#define STORE_CHECKOUT_DIALOG_SERVICE_ID_LEN		32

/*
 * store_checkout_dialog.cpp: the work memory (CommonDialogSlot::workMemory) the
 * checkout plugin (libcdlg_checkout) reads: the caller's parameter,
 * the result for GetResult, and copies of the dimmer colour, the SKU IDs
 * and the service ID. The copied parameter points to the dimmer colour's
 * copy; the dialog takes no background colour.
 */
typedef struct CommonDialogStoreCheckoutParam {
	SceStoreCheckoutDialogParam param;
	SceStoreCheckoutDialogResult result;
	SceCommonDialogColor bgColor;
	SceCommonDialogColor dimmerColor;
	SceChar8 skuIds[STORE_CHECKOUT_DIALOG_SKU_NUM_MAX][STORE_CHECKOUT_DIALOG_SKU_ID_LEN + 1];
	SceChar8 serviceId[STORE_CHECKOUT_DIALOG_SERVICE_ID_LEN + 1];
	SceUInt8 padding[3];
	char apiName[64];
} CommonDialogStoreCheckoutParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogStoreCheckoutApiName,
			   __builtin_offsetof(CommonDialogStoreCheckoutParam, apiName) == 1128);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogStoreCheckoutSize, sizeof(CommonDialogStoreCheckoutParam) == 1192);

#endif
