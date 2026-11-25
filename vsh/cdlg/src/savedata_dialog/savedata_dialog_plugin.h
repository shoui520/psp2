/*
 * SceSaveDataDialogPlugin (vs0:sys/external/libcdlg_savedata.suprx):
 * plugin entry, work memory layout, and parameter validation declarations.
 */
#ifndef SAVEDATA_DIALOG_PLUGIN_H
#define SAVEDATA_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <common_dialog.h>
#include <savedata_dialog.h>
#include <apputil.h>
#include <apputil/apputil_psp.h>
#include <savedata_psp_dialog.h>
#include <plugin.h>
#include "../cdlg_mgr/common_dialog_mgr.h"
#include "../cdlg_main/common_dialog_main.h"

/* SceSaveDataDialog work-memory request flags (matching vsh/cdlg/src/cdlg_mgr/savedata_dialog.cpp) */
#define SAVEDATA_DIALOG_FLAG_PARAM		0x01u
#define SAVEDATA_DIALOG_FLAG_CONTINUE		0x02u
#define SAVEDATA_DIALOG_FLAG_FINISH		0x04u
#define SAVEDATA_DIALOG_FLAG_SUB_CLOSE		0x08u
#define SAVEDATA_DIALOG_FLAG_PROGRESS_INC	0x10u
#define SAVEDATA_DIALOG_FLAG_PROGRESS_SET	0x20u

#define SAVEDATA_DIALOG_SUB_STATUS_NONE		0
#define SAVEDATA_DIALOG_SUB_STATUS_RUNNING	1
#define SAVEDATA_DIALOG_SUB_STATUS_FINISHED	2

/* SceSaveDataDialog error codes */
#define SAVEDATA_DIALOG_ERROR_PARAM		(-2146432255)	/* 0x80100B01 */
#define SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY	(-2146432222)	/* 0x80100B22 */
#define SAVEDATA_DIALOG_ERROR_INTERNAL		(-2146432128)	/* 0x80100B80 */
#define SAVEDATA_DIALOG_ERROR_CANCELLED		(-2146432127)	/* 0x80100B81 */
#define SAVEDATA_DIALOG_ERROR_NOT_READY		(-2146432126)	/* 0x80100B82 */
#define SAVEDATA_DIALOG_ERROR_NO_MEMORY_CARD	(-2146432096)	/* 0x80100BA0 */
#define SAVEDATA_DIALOG_ERROR_COMMON_GENERAL	(-2147351425)	/* 0x8002047F */

/* User-data tag passed to SaveDataDialogCallback */
#define SAVEDATA_DIALOG_CALLBACK_TAG		((void *)0xDEADBEEF)

/* SceAppUtil internal slot-change notification registration */
extern "C" int SceAppUtil_CD0E5C59(
	int (*callback)(int event, SceAppUtilSaveDataSlotId id, void *arg),
	void *arg);

/*
 * Shared work memory layout between libcdlg (savedata_dialog.cpp) and SceSaveDataDialogPlugin.
 * Total size: 4184 bytes.
 */
struct CommonDialogSaveDataParam {
	SceInt32 reserved;
	SceInt32 request;
	SceUInt32 requestFlags;
	SceInt32 closeRequested;
	SceInt32 abortRequested;
	SceSaveDataDialogParam param;
	SceAppUtilSaveDataSlotEmptyParam emptyParam;
	SceSaveDataDialogListParam listParam;
	SceSaveDataDialogFixedParam fixedParam;
	SceSaveDataDialogUserMessageParam userMsgParam;
	SceSaveDataDialogSystemMessageParam sysMsgParam;
	SceSaveDataDialogErrorCodeParam errorCodeParam;
	SceSaveDataDialogProgressBarParam progBarParam;
	SceSaveDataDialogSlotConfigParam slotConfParam;
	SceSaveDataDialogSlotInfo slotInfo;
	SceAppUtilSaveDataSlotParam slotParam;
	SceSaveDataDialogProgressBarTarget progressBarTarget;
	SceUInt32 progressBarRate;
	SceInt32 progressBarMsgSet;
	SceChar8 msg[SCE_SAVEDATA_DIALOG_USER_MSG_SIZE];
	SceAppUtilMountPoint mountPoint;
	SceSaveDataDialogAppSubDirParam appSubDir;
	SceSaveDataDialogFinishParam finishParam;
	SceSaveDataDialogResult result;
	SceInt32 isPsp;
	ScePspSaveDataDialogParam pspParam;
	ScePspSaveDataDialogListParam pspListParam;
	ScePspSaveDataDialogFixedParam pspFixedParam;
	ScePspSaveDataDialogUserMessageParam pspUserMsgParam;
	ScePspSaveDataDialogSystemMessageParam pspSysMsgParam;
	ScePspSaveDataDialogErrorCodeParam pspErrorCodeParam;
	ScePspSaveDataDialogProgressBarParam pspProgBarParam;
	ScePspSaveDataDialogResult pspResult;
	SceAppUtilPspSaveDataParamSfo paramSfo;
	char apiName[64];
};

/* Displayable slot summary passed back to SaveDataDialogCallback */
struct SaveDataDialogSlotResultInfo {
	SceAppUtilSaveDataSlotId slotId;
	paf::wstring title;
	paf::wstring subTitle;
	paf::wstring detail;
	paf::wstring dirName;
	paf::string iconPath;
	SceInt32 status;
	SceInt32 userParam;
	SceDateTime modifiedTime;
	SceUInt32 sizeKiB;
	SceInt32 existState;
};

typedef void (*SaveDataDialogResultCallback)(SceInt32 result,
	SceSaveDataDialogMode mode, SceInt32 buttonAction,
	const SaveDataDialogSlotResultInfo *slotInfo, void *userData);

extern paf::Plugin *g_saveDataDialogPlugin;
extern SceInt32 g_saveDataParamErrorCode;

void PrintSaveDataParamError(void);
SceInt32 ValidateSaveDataParam(const SceSaveDataDialogParam *param, SceBool isContinue);
SceInt32 ValidatePspSaveDataParam(const ScePspSaveDataDialogParam *param, SceBool isContinue);
SceInt32 ValidateSaveDataFinishParam(const SceSaveDataDialogFinishParam *finishParam);

#endif /* SAVEDATA_DIALOG_PLUGIN_H */
