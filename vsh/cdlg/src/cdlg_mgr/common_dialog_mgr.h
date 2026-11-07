/*
 * The common dialog's work area: SceCommonDialog (libcdlg) keeps it and hands
 * it to SceCommonDialogMain (libcdlg_main) when its worker thread loads that
 * module; the main module fills in the callbacks the worker thread runs. One
 * slot per dialog that can be open at once. No Sony record names the
 * structures or their fields: the names are ours, after their readers and
 * writers in both modules.
 */
#ifndef CDLG_MGR_COMMON_DIALOG_MGR_H
#define CDLG_MGR_COMMON_DIALOG_MGR_H

#include <stdint.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>

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

/* CommonDialogSlot::status */
enum CommonDialogSlotStatus {
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

/* CommonDialogSlot::closeState */
enum {
	COMMON_DIALOG_CLOSE_DONE = 2
};

/* the worker thread's callbacks, set by SceCommonDialogMain (kept as void * in the work
   area, CommonDialogWork::initFunc ...) */
typedef int (*CommonDialogInitFunc)(int dialogType);
typedef int (*CommonDialogTermFunc)(int dialogType);
typedef int (*CommonDialogLoadFunc)(int dialogType);
typedef int (*CommonDialogStopFunc)(void);
typedef int (*CommonDialogRunFunc)(void);
typedef int (*CommonDialogUpdateFunc)(const void *updateParam);

typedef struct CommonDialogSlot {
	int dialogType;
	int closeState;
	int userData;
	int32_t status;
	SceUInt8 reserved16[8];
	void *param;
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

typedef struct CommonDialogWork {
	bool hasConfig;
	SceUInt8 reserved1[7];
	int language;
	int enterButtonAssign;
	SceUInt8 reserved16[32];
	bool hasDimmerColor;
	CommonDialogColor dimmerColor;
	SceUInt8 reserved53[3];
	int drawPriority;
	bool keepInputLocked;
	SceUInt8 reserved61[3];
	int32_t openCount;
	SceUInt32 lastSlot;
	CommonDialogSlot slot[COMMON_DIALOG_SLOT_NUM];
	bool frameworkShared;
	bool pafLoaded;
	SceUInt8 reserved434[6];
	SceUID frameworkSema;
	void *initFunc;
	void *termFunc;
	void *loadFunc;
	void *stopFunc;
	void *runFunc;
	void *updateFunc;
	SceUInt8 reserved468[48];
	/* SceCommonDialogFocusStatus: whether the system shows a focus */
	int focusState;
} CommonDialogWork;

/*
 * The parameters libcdlg keeps for the dialogs whose plugins draw at the
 * caller's priority (CommonDialogSlot::param): the SDK's parameter copied in,
 * and what libcdlg adds. Only the draw priority SceCommonDialogMain reads is
 * laid out yet; the rest comes with libcdlg's dialogs.
 */
typedef struct CommonDialogPhotoImportParam {
	SceUInt8 reserved[176];
	int drawPriority;
} CommonDialogPhotoImportParam;

typedef struct CommonDialogPhotoReviewParam {
	SceUInt8 reserved[2260];
	int drawPriority;
} CommonDialogPhotoReviewParam;

typedef struct CommonDialogCameraImportParam {
	SceUInt8 reserved[424];
	int drawPriority;
} CommonDialogCameraImportParam;

typedef struct CommonDialogNpEulaParam {
	SceUInt8 reserved[28];
	int drawPriority;
} CommonDialogNpEulaParam;

typedef struct CommonDialogCrossControllerParam {
	SceUInt8 reserved[172];
	int drawPriority;
} CommonDialogCrossControllerParam;

typedef struct CommonDialogVideoImportParam {
	SceUInt8 reserved[284];
	int drawPriority;
} CommonDialogVideoImportParam;

typedef struct CommonDialogCalendarReviewParam {
	SceUInt8 reserved[1188];
	int drawPriority;
} CommonDialogCalendarReviewParam;

typedef struct CommonDialogCompanionParam {
	SceUInt8 reserved[204];
	int drawPriority;
} CommonDialogCompanionParam;

typedef struct CommonDialogRemoteOskParam {
	SceUInt8 reserved[432];
	int drawPriority;
} CommonDialogRemoteOskParam;

#endif
