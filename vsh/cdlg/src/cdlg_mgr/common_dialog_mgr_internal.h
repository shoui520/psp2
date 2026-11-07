/*
 * SceCommonDialog's own interface: what common_dialog_mgr.cpp gives the
 * dialogs' files (the slot of a dialog type, its work memory and status, its
 * calls through the worker thread), the dialog table, and the system calls
 * the module makes that no header of the tree declares for user code yet. The
 * names are ours where the 0.9x builds' messages give none (AllocateWorkMemory,
 * GetWorkMemory, NestInit, callByWorkerThread and DispatchApiCall are Sony's).
 */
#ifndef CDLG_MGR_COMMON_DIALOG_MGR_INTERNAL_H
#define CDLG_MGR_COMMON_DIALOG_MGR_INTERNAL_H

#include <scetypes.h>
#include <common_dialog.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <stdarg.h>
#include "common_dialog_mgr.h"

/* the module's errors that the SDK does not name */
#define COMMON_DIALOG_ERROR_FATAL			-2147351297	/* 0x800204FF */
#define COMMON_DIALOG_ERROR_ALREADY_EXISTS		-2147351359	/* 0x800204C1 */
#define COMMON_DIALOG_ERROR_NULL_FUNCTION		-2147351358	/* 0x800204C2 */
#define COMMON_DIALOG_ERROR_NOT_INITIALIZED		-2147351357	/* 0x800204C3 */
#define COMMON_DIALOG_ERROR_NO_BUDGET			-2147351356	/* 0x800204C4 */
#define COMMON_DIALOG_ERROR_UNKNOWN_TYPE		-2147351355	/* 0x800204C5 */
#define COMMON_DIALOG_ERROR_DISPATCH_BUSY		-2147351352	/* 0x800204C8 */
#define COMMON_DIALOG_ERROR_NEST_TOO_DEEP		-2147351341	/* 0x800204D3 */
#define COMMON_DIALOG_ERROR_NEST_NOT_RUNNING		-2147351340	/* 0x800204D4 */
#define COMMON_DIALOG_ERROR_NOT_PAF_MAIN_THREAD		-2147351339	/* 0x800204D5 */
#define COMMON_DIALOG_ERROR_TOO_MANY_ARGS		-2147351328	/* 0x800204E0 */
#define COMMON_DIALOG_ERROR_LOCK_FAILED			-2147351327	/* 0x800204E1 */
#define COMMON_DIALOG_ERROR_UNLOCK_FAILED		-2147351326	/* 0x800204E2 */

/*
 * The kernel's sceKernelCreateThread option (threadmgr/threadmgr.h): the
 * memory types of the user stack and the TLS, which the SDK's option lacks.
 */
typedef struct CommonDialogThreadOptParam {
	SceSize size;
	SceUInt32 attr;
	SceUInt32 kStackMemType;
	SceUInt32 uStackMemType;
	SceUInt32 uTLSMemType;
	SceUInt32 uStackMemid;
	SceUInt32 handedExceptions;
} CommonDialogThreadOptParam;

#define CDLG_THREAD_OPT_USTACK_MEMTYPE		0x2
#define CDLG_THREAD_OPT_UTLS_MEMTYPE		0x4

/* a dialog of the table: its type and its Init and Init2 (the two-argument form) */
typedef struct CommonDialogTableEntry {
	int dialogType;
	int (*onInit)(void *param);
	int (*onInit2)(void *param, void *arg);
} CommonDialogTableEntry;

extern const CommonDialogTableEntry g_commonDialogTable[];
extern CommonDialogWork g_work;

/* what a dialog's file calls (common_dialog_mgr.cpp) */
int InitImpl(int dialogType, bool withArg, void *param, void *arg);
int NestInitImpl(int dialogType, void *param, void *arg);
int TermImpl(int dialogType);
int UpdateImpl(const SceCommonDialogUpdateParam *updateParam);
int callByWorkerThread(void *func, SceUInt32 numArgs, ...);
int OnInit(int dialogType, void *param);
int OnInit2(int dialogType, void *param, void *arg);
int NestInit(int dialogType, void *param, void *arg);
int Term(int dialogType);
int AllocateWorkMemory(int dialogType, SceSize size);
void *GetWorkMemory(int dialogType);
int GetStatus(int dialogType);
int GetUserData(int dialogType);
int SetUserData(int dialogType, int userData);
int SetParamReady(int dialogType);
int DispatchApiCall(int dialogType, void *func, SceUInt32 numArgs, ...);
int LockDialog(int dialogType);
int UnlockDialog(int dialogType);
bool isApplicationMode(void);
/* what a dialog asks checkCommonParam to do with the info bar, and its draw priority */
typedef struct CommonDialogParamOption {
	bool keepInfobar;	/* show the application's info bar */
	bool hideInfobar;
	int drawPriority;
} CommonDialogParamOption;

int setInfobar(const SceCommonDialogInfobarParam *infobarParam, bool *infobarShown);
void restoreInfobar(void);
int checkCommonParam(const CommonDialogParamOption *option, const SceCommonDialogParam *commonParam,
		     bool *infobarShown);
int checkCommonParamWithInfobar(const SceCommonDialogParam *commonParam, bool *infobarShown,
				bool keepInfobar, bool hideInfobar);
void clearWorkFlags(void);

/* the sysmodule calls with the module's status (the module's tail) */
int loadSysmoduleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags, int *status);
void clearMainWorkFlags(CommonDialogWork *work);
int unloadSysmoduleWithArg(SceUInt32 id, SceSize args, void *argp, SceUInt32 flags, int *status);
bool isCommonDialogError(int error);

/* the worker thread (worker_thread.cpp) */
int initializeWorkerThread(void);
int finalizeWorkerThread(void);
int dispatch(void *func, SceUInt32 numArgs, va_list args, bool tryLock);
void setWorkerThreadOwner(SceUID thread);
void clearWorkerThreadOwner(void);
SceUID getWorkerThreadId(void);

#ifdef __cplusplus
extern "C" {
#endif

/* imports no header of the tree declares for user code yet */
typedef struct SceKernelSubbudgetInfo {
	SceSize size;
	SceSize total;
	SceSize free;
} SceKernelSubbudgetInfo;

int sceKernelGetSubbudgetInfo(int budget, SceKernelSubbudgetInfo *info);
int sceKernelIsCDialogAvailable(void);
int sceKernelIsGameBudget(void);
int sceKernelCDialogSessionClose(void);
int sceKernelCDialogSetLeaseLimit(SceUInt32 limit);
int sceAppMgrIsGameProgram(void);
int sceAppMgrIsDevelopmentMode(void);
int sceAppMgrAppParamGetInt(int pid, int param, int *value);
int sceAppMgrAppParamGetString(int pid, int param, char *string, SceSize length);
int sceAppMgrSetInfobarStateForCommonDialog(int visibility, int color, int transparency);
int sceDisplayGetMaximumFrameBufResolution(int *width, int *height);
int sceCtrlSetButtonIntercept(int intercept);
int sceTouchSetRegion(SceUInt32 port, SceUInt32 id, const void *region, int attr);
int sceTouchActivateRegion(SceUInt32 port, SceUInt32 id, int activate);
int sceTouchClearRegion(SceUInt32 port, SceUInt32 id);
int sceRegMgrUtilityGetInt(int id, int *value);
SceUInt32 SceLibKernel_9F793F84(void);
int SceSvcCtrl_8BEB8B7C(void);
int SceSvcCtrl_077F8AF1(void);
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
SceUID sceGxmGetDisplayQueueThreadIdInternal(void);
int sceGxmIsInitializationInternal(void);
int sceGxmRenderingContextIsWithinSceneInternal(void);

#ifdef __cplusplus
}
#endif

#endif
