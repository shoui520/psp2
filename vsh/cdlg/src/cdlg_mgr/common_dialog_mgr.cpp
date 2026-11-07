/*
 * SceCommonDialog's manager: the work area every dialog shares (a slot per
 * dialog open at once), the PafMain thread that runs SceCommonDialogMain, and
 * the calls each dialog's file makes for its Init, NestInit, Term, status,
 * user data, work memory and lock. The dialogs' API calls run on the worker
 * thread (worker_thread.cpp) through callByWorkerThread and DispatchApiCall.
 *
 * Sony's names (the 0.9x builds' messages): AllocateWorkMemory,
 * GetWorkMemory, NestInit, callByWorkerThread, DispatchApiCall. The other
 * names are ours.
 */
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include <touch.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <sysmem/memblock.h>
#include <inputdevice/inputdevice.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define CDLG_ASSERT(cond)							\
	do {								\
		if (!(cond)) {						\
			sceClibPrintf("[SceCommonDialog] INTERNAL ASSERION FAILURE\n"); \
			__breakpoint(0);				\
		}							\
	} while (0)

#define CDLG_ASSERT_OK(ret)							\
	do {								\
		int ret_ = (ret);					\
		if (ret_ != SCE_OK) {					\
			sceClibPrintf("[SceCommonDialog] INTERNAL ASSERION FAILURE (0x%x)\n", ret_); \
			__breakpoint(0);				\
		}							\
	} while (0)

/* the fatal errors the module reports and returns */
#define CDLG_PRINT_FATAL(ret)						\
	sceClibPrintf("[SceCommonDialog] INTERNAL ASSERION FAILURE (0x%x)\n", (ret))

/* the main module's SDK versions that change what the dialogs check */
#define CDLG_SDK_VERSION_INFOBAR_CHECK		0x01800000
#define CDLG_SDK_VERSION_MAGIC_CHECK		0x02000000
#define CDLG_SDK_VERSION_CONFIG		0x02100000
#define CDLG_SDK_VERSION_GXM_CHECK		0x02500000

/* the application parameters and the system parameter the configuration is read from */
#define CDLG_APP_PARAM_ENTER_BUTTON		16
#define CDLG_REG_LANGUAGE			226562

/* sceKernelGetSubbudgetInfo's budget of the common dialog */
#define CDLG_SUBBUDGET_CDIALOG			1

/* what the dialogs may keep of the budget between Init and Term */
#define CDLG_BUDGET_MARGIN			(180 * 1024)

/* the largest work memory a dialog allocates */
#define CDLG_WORK_MEMORY_MAX			0x100000

/* sceKernelAllocMemBlock: the budget the common dialog reserves at module start */
#define CDLG_RESERVE_SIZE			0x20000

/* the touch region the dialogs intercept, on both panels */
#define CDLG_TOUCH_REGION			7
#define CDLG_TOUCH_PORT_NUM			2

/* the info bar states sceAppMgrSetInfobarStateForCommonDialog takes */
#define CDLG_INFOBAR_HIDDEN			0
#define CDLG_INFOBAR_VISIBLE			1
#define CDLG_INFOBAR_APPLICATION		2

/* SceCommonDialog reads the application's info bar state as signed bytes */
#define INFOBAR_VALUE(field)			((signed char)(field))

/* the input devices paf's inputdevice::Lock takes: all of them */
#define CDLG_INPUT_DEVICE_ALL			127

/* the PafMain thread */
#define CDLG_PAF_MAIN_PRIORITY			SCE_KERNEL_DEFAULT_PRIORITY
#define CDLG_PAF_MAIN_PRIORITY_CDIALOG		(SCE_KERNEL_DEFAULT_PRIORITY - 13)
#define CDLG_PAF_MAIN_STACK_SIZE		0x10000
#define CDLG_PAF_MAIN_ATTR			0x02000000
#define CDLG_PAF_MAIN_CPU_MASK_CDIALOG		0x00080000	/* the system's core */

/* the module load flag of a process that may show common dialogs */
#define CDLG_MODULE_LOAD_FLAG_CDIALOG		0x10

bool g_initialized;
CommonDialogWork g_work;
bool g_moduleStartSkipped;

/* the slot of a dialog type, or -1 */
static inline int findSlot(int dialogType)
{
	SceUInt32 i;

	for (i = 0; i < COMMON_DIALOG_SLOT_NUM; i++) {
		if (g_work.slot[i].dialogType == dialogType)
			return i;
	}
	return -1;
}

/* whether this thread may drive the slot: in a game's common dialog mode only the PafMain
   thread drives the nested dialogs */
static inline bool isSlotCaller(SceUID thread, int i)
{
	return !g_work.cdialogMode || thread == g_work.pafMainThread || i <= 0;
}

/* the dialog table's entry of a dialog type */
static inline const CommonDialogTableEntry *findDialog(int dialogType)
{
	SceUInt32 i;

	for (i = 0; g_commonDialogTable[i].dialogType != 0; i++) {
		if (g_commonDialogTable[i].dialogType == dialogType)
			return &g_commonDialogTable[i];
	}
	return NULL;
}

/* takes a free slot */
static inline bool reserveSlot(CommonDialogSlot *slot)
{
	int32_t state;

	__builtin_dmb();
	do {
		state = __builtin_ldrex(&slot->state);
		if (state != COMMON_DIALOG_SLOT_FREE)
			break;
	} while (__builtin_strex(COMMON_DIALOG_SLOT_RESERVED, &slot->state) != 0);
	__builtin_dmb();
	return state == COMMON_DIALOG_SLOT_FREE;
}

/* frees the last slot, the dialog's */
static inline void releaseSlot(int dialogType)
{
	CommonDialogSlot *slot = &g_work.slot[g_work.lastSlot];

	CDLG_ASSERT(slot->dialogType == dialogType);
	slot->dialogType = 0;
	slot->dialogStatus = SCE_COMMON_DIALOG_STATUS_NONE;
	slot->userData = 0;
	slot->state = COMMON_DIALOG_SLOT_FREE;
	if (slot->workMemoryBlock != -1)
		CDLG_ASSERT_OK(sceKernelFreeMemBlock(slot->workMemoryBlock));
	slot->workMemoryBlock = -1;
	slot->workMemory = NULL;
	slot->paramReady = false;
	slot->pluginName[0] = '\0';
}

/* the budget left, and what the dialogs before kept of it */
static inline void printBudget(const char *format, int dialogType)
{
	SceKernelSubbudgetInfo info;
	int leak;

	info.size = sizeof(info);
	if (sceKernelGetSubbudgetInfo(CDLG_SUBBUDGET_CDIALOG, &info) != SCE_OK || info.free == 0) {
		sceClibPrintf(format, dialogType);
		sceClibPrintf("\n");
		return;
	}
	leak = g_work.initialBudgetFree - info.free - CDLG_BUDGET_MARGIN;
	if (leak > 0)
		sceClibPrintf("[SceCommonDialog] *WARNING* Possible memory leak detected (%d KiB)\n",
			      leak / 1024);
	sceClibPrintf(format, dialogType);
	sceClibPrintf(", budget %u KiB free\n", info.free >> 10);
}

int createSyncObjects(void)
{
	char name[32];
	SceUInt32 i;
	SceUInt32 j;
	SceUID sema;
	int ret;

	ret = sceKernelCreateSema("SceCommonDialogComp", SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0, 1, NULL);
	if (ret < 0)
		return ret;
	g_work.frameworkSema = ret;

	for (i = 0; i < COMMON_DIALOG_SLOT_NUM; i++) {
		g_work.slot[i].dialogType = 0;
		g_work.slot[i].dialogStatus = SCE_COMMON_DIALOG_STATUS_NONE;
		g_work.slot[i].userData = 0;
		g_work.slot[i].state = COMMON_DIALOG_SLOT_FREE;
		g_work.slot[i].workMemoryBlock = -1;
		g_work.slot[i].workMemory = NULL;
		g_work.slot[i].paramReady = false;
		g_work.slot[i].pluginName[0] = '\0';

		sceClibSnprintf(name, sizeof(name), "SceCommonDialogSharedMutex%u", i);
		ret = sceKernelCreateLwMutex(&g_work.slot[i].mutex, name,
					     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
		if (ret != SCE_OK) {
			for (j = 0; j < i; j++)
				CDLG_ASSERT_OK(sceKernelDeleteLwMutex(&g_work.slot[j].mutex));
			return ret;
		}

		sceClibSnprintf(name, sizeof(name), "SceCommonDialogPluginComp%u", i);
		sema = sceKernelCreateSema(name, SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0, 1, NULL);
		if (sema < 0) {
			for (j = 0; j <= i; j++)
				CDLG_ASSERT_OK(sceKernelDeleteLwMutex(&g_work.slot[j].mutex));
			for (j = 0; j < i; j++)
				CDLG_ASSERT_OK(sceKernelDeleteSema(g_work.slot[j].doneSema));
			CDLG_ASSERT_OK(sceKernelDeleteSema(g_work.frameworkSema));
			return sema;
		}
		g_work.slot[i].doneSema = sema;
	}

	g_work.openCount = 0;
	g_work.lastSlot = 0;
	g_work.pafMainThread = -1;
	g_work.frameworkShared = false;
	g_work.pafLoaded = false;
	g_work.cdialogMode = false;
	g_work.hasConfig = false;
	g_work.hasDimmerColor = false;
	g_work.drawPriority = 229;
	g_work.dimmerColor.r = 0;
	g_work.dimmerColor.g = 0;
	g_work.dimmerColor.b = 0;
	g_work.dimmerColor.a = 0;
	g_work.allow720p = false;
	g_work.allow1080p = false;
	g_work.focusState = SCE_COMMON_DIALOG_FOCUS_INVALID;
	sceClibMemset(&g_work.initFunc, 0, 6 * sizeof(void *));
	return SCE_OK;
}

int deleteSyncObjects(void)
{
	SceUInt32 i;
	int ret;

	for (i = 0; i < COMMON_DIALOG_SLOT_NUM; i++) {
		ret = sceKernelDeleteLwMutex(&g_work.slot[i].mutex);
		if (ret != SCE_OK)
			goto fatal;
		ret = sceKernelDeleteSema(g_work.slot[i].doneSema);
		if (ret != SCE_OK)
			goto fatal;
	}
	ret = sceKernelDeleteSema(g_work.frameworkSema);
	if (ret != SCE_OK)
		goto fatal;
	return SCE_OK;

fatal:
	CDLG_PRINT_FATAL(ret);
	return COMMON_DIALOG_ERROR_FATAL;
}

/* the render targets of 1280x720 and of 1440 or 1920x1080: for what is no game, the titles
   that draw at them, and what the display gives */
void checkRenderTargetSizes(void)
{
	char titleId[10];
	int width;
	int height;

	if (sceAppMgrIsGameProgram() == 0) {
		g_work.allow720p = true;
		g_work.allow1080p = true;
	}
	sceClibMemset(titleId, 0, sizeof(titleId));
	if (sceAppMgrAppParamGetString(0, CDLG_APP_PARAM_TITLE_ID, titleId, sizeof(titleId)) == SCE_OK
	    && (sceClibStrncmp(titleId, "PCSG80001", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSG80007", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSG00318", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSG00319", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSG00320", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSG00321", sizeof(titleId)) == 0
		|| sceClibStrncmp(titleId, "PCSH00059", sizeof(titleId)) == 0)) {
		g_work.allow720p = true;
		g_work.allow1080p = true;
	}
	if (sceDisplayGetMaximumFrameBufResolution(&width, &height) == SCE_OK) {
		if ((SceUInt32)height >= 720)
			g_work.allow720p = true;
		if ((SceUInt32)height >= 1080)
			g_work.allow1080p = true;
	}
}

/* in a game's common dialog mode the buttons and the whole of both panels go to the dialog */
void enableInputIntercept(void)
{
	SceTouchPanelInfo info;
	SceUInt32 attr;
	SceInt16 rect[4];
	SceUInt32 port;

	if (!g_work.cdialogMode)
		return;
	CDLG_ASSERT_OK(sceCtrlSetButtonIntercept(1));
	for (port = 0; port < CDLG_TOUCH_PORT_NUM; port++) {
		CDLG_ASSERT_OK(sceTouchGetPanelInfo(port, &info));
		sceClibMemset(&attr, 0, sizeof(attr));
		rect[0] = info.minAaX;
		rect[1] = info.minAaY;
		rect[2] = info.maxAaX - info.minAaX + 1;
		rect[3] = info.maxAaY - info.minAaY + 1;
		CDLG_ASSERT_OK(sceTouchSetRegion(port, CDLG_TOUCH_REGION, rect, attr));
		CDLG_ASSERT_OK(sceTouchActivateRegion(port, CDLG_TOUCH_REGION, 1));
	}
}

/* undoes initialize's work once its worker thread and sync objects are made */
static inline int undoInitialize(int ret)
{
	int err;

	err = finalizeWorkerThread();
	if (err == SCE_OK) {
		err = deleteSyncObjects();
		if (err == SCE_OK) {
			err = sceKernelDeleteLwMutex(&g_work.updateMutex);
			if (err == SCE_OK)
				return ret;
		}
	}
	CDLG_PRINT_FATAL(err);
	return COMMON_DIALOG_ERROR_FATAL;
}

int initialize(void)
{
	SceKernelSubbudgetInfo info;
	SceUInt32 sdkVersion;
	int enterButtonAssign;
	int value;
	int ret;
	int err;

	if (g_initialized)
		return COMMON_DIALOG_ERROR_ALREADY_EXISTS;
	ret = sceKernelCreateLwMutex(&g_work.updateMutex, "SceCommonDialogPafUpdateMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret != SCE_OK)
		return ret;

	ret = initializeWorkerThread();
	if (ret != SCE_OK) {
		err = sceKernelDeleteLwMutex(&g_work.updateMutex);
		if (err == SCE_OK)
			return ret;
		goto fatal;
	}

	ret = createSyncObjects();
	if (ret != SCE_OK) {
		err = finalizeWorkerThread();
		if (err != SCE_OK || (err = sceKernelDeleteLwMutex(&g_work.updateMutex)) != SCE_OK)
			goto fatal;
		return ret;
	}

	sdkVersion = SceLibKernel_9F793F84();
	g_work.sdkVersion = sdkVersion;
	if (sdkVersion >= CDLG_SDK_VERSION_CONFIG) {
		value = 0;
		g_work.config.sdkVersion = sdkVersion;
		ret = sceAppMgrAppParamGetInt(0, CDLG_APP_PARAM_ENTER_BUTTON, &value);
		if (ret != SCE_OK)
			goto undo;
		switch (value) {
		case 0:
			enterButtonAssign = 0;
			break;
		case 1:
			enterButtonAssign = 1;
			break;
		default:
			ret = SCE_COMMON_DIALOG_ERROR_INVALID_ENTER_BUTTON_ASSIGN;
			goto undo;
		}
		g_work.config.enterButtonAssign = enterButtonAssign;
		ret = sceRegMgrUtilityGetInt(CDLG_REG_LANGUAGE, &value);
		if (ret != SCE_OK)
			goto undo;
		g_work.config.language = value;
		g_work.hasConfig = true;
	}

	checkRenderTargetSizes();
	if (sceKernelIsCDialogAvailable())
		sceKernelAllocMemBlock("SceCommonDialogReserve", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW,
				       CDLG_RESERVE_SIZE, NULL);
	if (sceKernelIsCDialogAvailable() && sceKernelCDialogSetLeaseLimit(1) != SCE_OK)
		return undoInitialize(COMMON_DIALOG_ERROR_NO_BUDGET);
	info.size = sizeof(info);
	g_initialized = true;
	g_work.initialBudgetFree = sceKernelGetSubbudgetInfo(CDLG_SUBBUDGET_CDIALOG, &info) == SCE_OK
				   ? info.free : 0;
	return SCE_OK;

undo:
	return undoInitialize(ret);

fatal:
	CDLG_PRINT_FATAL(err);
	return COMMON_DIALOG_ERROR_FATAL;
}

int finalize(void)
{
	int ret;

	if (!g_initialized)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	if (g_work.slot[0].state != COMMON_DIALOG_SLOT_FREE)
		return COMMON_DIALOG_ERROR_ALREADY_EXISTS;
	ret = finalizeWorkerThread();
	if (ret != SCE_OK)
		return ret;
	ret = deleteSyncObjects();
	if (ret != SCE_OK || (ret = sceKernelDeleteLwMutex(&g_work.updateMutex)) != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	g_initialized = false;
	return SCE_OK;
}

/* the PafMain thread: SceCommonDialogMain makes the framework, loads the dialog's plugin and
   runs the framework until the dialog ends */
int pafMainThread(SceSize args, void *argp)
{
	int status;
	CommonDialogWork *work = &g_work;
	SceUInt32 flags = 0;
	SceUInt32 port;
	int ret;

	if (sceKernelIsCDialogAvailable())
		flags = CDLG_MODULE_LOAD_FLAG_CDIALOG;
	ret = loadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN, sizeof(work), &work,
				   flags, &status);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	if (status != 0)
		SceThreadmgrCoredumpTime_sceKernelExitThread(status);

	CDLG_ASSERT_OK(((CommonDialogInitFunc)g_work.initFunc)(g_work.slot[0].dialogType));
	CDLG_ASSERT_OK(((CommonDialogLoadFunc)g_work.loadFunc)(g_work.slot[0].dialogType));
	status = ((CommonDialogRunFunc)g_work.runFunc)();
	CDLG_ASSERT_OK(((CommonDialogStopFunc)g_work.stopFunc)(g_work.slot[0].dialogType));

	ret = sceKernelLockLwMutex(&g_work.updateMutex, 1, NULL);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	CDLG_ASSERT_OK(((CommonDialogTermFunc)g_work.termFunc)(g_work.slot[0].dialogType));
	ret = sceKernelUnlockLwMutex(&g_work.updateMutex, 1);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}

	if (g_work.cdialogMode) {
		CDLG_ASSERT_OK(sceCtrlSetButtonIntercept(0));
		for (port = 0; port < CDLG_TOUCH_PORT_NUM; port++) {
			CDLG_ASSERT_OK(sceTouchActivateRegion(port, CDLG_TOUCH_REGION, 0));
			CDLG_ASSERT_OK(sceTouchClearRegion(port, CDLG_TOUCH_REGION));
		}
	}

	ret = sceAppMgrSetInfobarStateForCommonDialog(CDLG_INFOBAR_APPLICATION, 0, 0);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	ret = unloadSysmoduleWithArg(SCE_SYSMODULE_INTERNAL_COMMON_DIALOG_MAIN, 0, NULL, 0, &status);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	if (sceKernelIsCDialogAvailable()) {
		ret = sceKernelCDialogSessionClose();
		if (ret != SCE_OK)
			sceClibPrintf("[SceCommonDialog] Failed to shrink heap (%x)\n", ret);
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(status);
	return SCE_OK;
}

int startPafMainThread(void)
{
	CommonDialogThreadOptParam opt;
	const SceKernelThreadOptParam *option;
	SceInt32 priority;
	SceInt32 cpuMask;
	SceUID thread;
	int ret;

	g_work.slot[0].dialogStatus = SCE_COMMON_DIALOG_STATUS_RUNNING;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = CDLG_THREAD_OPT_USTACK_MEMTYPE | CDLG_THREAD_OPT_UTLS_MEMTYPE;
	opt.uStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	opt.uTLSMemType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	priority = g_work.cdialogMode ? CDLG_PAF_MAIN_PRIORITY_CDIALOG : CDLG_PAF_MAIN_PRIORITY;
	cpuMask = g_work.cdialogMode ? CDLG_PAF_MAIN_CPU_MASK_CDIALOG : 0;
	option = g_work.cdialogMode ? (const SceKernelThreadOptParam *)&opt : NULL;
	thread = sceKernelCreateThread("SceCommonDialogPafMain", pafMainThread, priority,
				       CDLG_PAF_MAIN_STACK_SIZE, CDLG_PAF_MAIN_ATTR, cpuMask, option);
	if (thread < 0) {
		g_work.slot[0].dialogStatus = SCE_COMMON_DIALOG_STATUS_NONE;
		return thread;
	}
	g_work.pafMainThread = thread;
	if (g_work.cdialogMode)
		setWorkerThreadOwner(thread);
	ret = sceKernelStartThread(thread, 0, NULL);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return SCE_OK;
}

/* a dialog's Init, on the worker thread: the first slot, the dialog's own Init, and the
   PafMain thread that shows it */
int InitImpl(int dialogType, bool withArg, void *param, void *arg)
{
	const CommonDialogTableEntry *entry;
	bool cdialogMode;
	int ret;
	int err;

	if (sceKernelIsCDialogAvailable()) {
		if (g_work.slot[0].state != COMMON_DIALOG_SLOT_FREE)
			cdialogMode = g_work.cdialogMode;
		else
			cdialogMode = sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_PAF) != SCE_OK;
	} else {
		cdialogMode = false;
	}
	g_work.cdialogMode = cdialogMode;
	if (cdialogMode && !g_work.hasConfig)
		return SCE_COMMON_DIALOG_ERROR_NOT_CONFIGURED;
	if (g_work.slot[0].state != COMMON_DIALOG_SLOT_FREE)
		return SCE_COMMON_DIALOG_ERROR_BUSY;
	if (sceKernelIsCDialogAvailable() && sceKernelCDialogSetLeaseLimit(0) != SCE_OK)
		return COMMON_DIALOG_ERROR_NO_BUDGET;
	entry = findDialog(dialogType);
	if (entry == NULL)
		return COMMON_DIALOG_ERROR_UNKNOWN_TYPE;
	if (g_work.cdialogMode && SceSvcCtrl_8BEB8B7C() != 0)
		return SCE_COMMON_DIALOG_ERROR_IME_IN_USE;
	if (!reserveSlot(&g_work.slot[0])) {
		SceSvcCtrl_077F8AF1();
		return SCE_COMMON_DIALOG_ERROR_BUSY;
	}

	printBudget("[SceCommonDialog] Starting Init(), type %u", dialogType);
	g_work.slot[0].dialogType = dialogType;
	if (withArg)
		ret = entry->onInit2(param, arg);
	else
		ret = entry->onInit(param);
	if (ret != SCE_OK) {
		releaseSlot(dialogType);
		SceSvcCtrl_077F8AF1();
		err = sceAppMgrSetInfobarStateForCommonDialog(CDLG_INFOBAR_APPLICATION, 0, 0);
		if (err != SCE_OK) {
			CDLG_PRINT_FATAL(err);
			return COMMON_DIALOG_ERROR_FATAL;
		}
		return ret;
	}
	if (startPafMainThread() != SCE_OK) {
		releaseSlot(dialogType);
		SceSvcCtrl_077F8AF1();
		return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	}
	enableInputIntercept();
	if (!g_work.cdialogMode)
		paf::inputdevice::Lock(CDLG_INPUT_DEVICE_ALL);
	return ret;
}

/* a dialog opened on a running one, on the worker thread: the next slot */
int NestInitImpl(int dialogType, void *param, void *arg)
{
	const CommonDialogTableEntry *entry;
	int ret;

	if (g_work.slot[0].state != COMMON_DIALOG_SLOT_RUNNING)
		return COMMON_DIALOG_ERROR_NEST_NOT_RUNNING;
	/* Sony's test compares the slot with the error code, not with the slots */
	if (g_work.lastSlot >= (SceUInt32)COMMON_DIALOG_ERROR_NEST_TOO_DEEP)
		return COMMON_DIALOG_ERROR_NEST_TOO_DEEP;
	entry = findDialog(dialogType);
	if (entry == NULL)
		return COMMON_DIALOG_ERROR_UNKNOWN_TYPE;
	if (!reserveSlot(&g_work.slot[g_work.lastSlot + 1]))
		return SCE_COMMON_DIALOG_ERROR_BUSY;

	g_work.lastSlot++;
	g_work.slot[g_work.lastSlot].dialogType = dialogType;
	if (arg != NULL && entry->onInit2 != NULL)
		ret = entry->onInit2(param, arg);
	else
		ret = entry->onInit(param);
	if (ret != SCE_OK) {
		releaseSlot(dialogType);
		g_work.lastSlot--;
		return ret;
	}
	if (((CommonDialogLoadFunc)g_work.loadFunc)(dialogType) != SCE_OK) {
		releaseSlot(dialogType);
		g_work.lastSlot--;
		return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	}
	return ret;
}

/* waits until the dialog's plugin is done */
static inline int waitPluginDone(int dialogType)
{
	int i;
	int ret;

	i = findSlot(dialogType);
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	ret = sceKernelWaitSema(g_work.slot[i].doneSema, 1, NULL);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return SCE_OK;
}

/* a dialog's Term, on the worker thread: the first dialog's ends the PafMain thread */
int TermImpl(int dialogType)
{
	SceInt32 status;
	int i;
	int ret;

	i = findSlot(dialogType);
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if ((SceUInt32)i != g_work.lastSlot)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (g_work.slot[i].dialogStatus != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	ret = waitPluginDone(dialogType);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}

	if (i != 0) {
		releaseSlot(dialogType);
		g_work.lastSlot--;
		return SCE_OK;
	}

	ret = sceKernelWaitThreadEnd(g_work.pafMainThread, &status, NULL);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	ret = sceKernelDeleteThread(g_work.pafMainThread);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	if (g_work.cdialogMode)
		clearWorkerThreadOwner();
	releaseSlot(dialogType);
	if (g_work.cdialogMode)
		SceSvcCtrl_077F8AF1();
	printBudget("[SceCommonDialog] Term() completed, type %u", dialogType);
	if (sceKernelIsCDialogAvailable() && sceKernelCDialogSetLeaseLimit(1) != SCE_OK)
		return COMMON_DIALOG_ERROR_NO_BUDGET;
	return SCE_OK;
}

/* sceCommonDialogUpdate, on the worker thread: the render target is checked, and the running
   dialog draws into it */
int UpdateImpl(const SceCommonDialogUpdateParam *updateParam)
{
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 stride;
	int ret;
	int err;

	if (updateParam == NULL || updateParam->renderTarget.colorSurfaceData == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (updateParam->renderTarget.colorFormat != SCE_GXM_COLOR_FORMAT_A8B8G8R8)
		return SCE_COMMON_DIALOG_ERROR_INVALID_COLOR_FORMAT;
	if (updateParam->renderTarget.surfaceType != SCE_GXM_COLOR_SURFACE_LINEAR)
		return SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_TYPE;
	width = updateParam->renderTarget.width;
	height = updateParam->renderTarget.height;
	stride = updateParam->renderTarget.strideInPixels;
	if (!(width == 480 && height == 272) && !(width == 640 && height == 368)
	    && !(width == 720 && height == 408) && !(width == 960 && height == 544)
	    && !(g_work.allow720p && width == 1280 && (height == 720 || height == 725))
	    && !(g_work.allow1080p && (width == 1440 || width == 1920)
		 && (height == 1080 || height == 1088)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_RESOLUTION;
	if (stride < width || (stride & 63) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_SURFACE_STRIDE;
	if (sceGxmRenderingContextIsWithinSceneInternal())
		return SCE_COMMON_DIALOG_ERROR_WITHIN_SCENE;

	ret = SCE_OK;
	err = sceKernelLockLwMutex(&g_work.updateMutex, 1, NULL);
	if (err == SCE_OK) {
		if (!g_work.frameworkShared && g_work.slot[0].state > COMMON_DIALOG_SLOT_IDLE
		    && g_work.slot[0].state < COMMON_DIALOG_SLOT_FINISHED)
			ret = ((CommonDialogUpdateFunc)g_work.updateFunc)(updateParam);
		err = sceKernelUnlockLwMutex(&g_work.updateMutex, 1);
		if (err == SCE_OK)
			return ret;
	}
	CDLG_PRINT_FATAL(err);
	return COMMON_DIALOG_ERROR_FATAL;
}

int callByWorkerThread(void *func, SceUInt32 numArgs, ...)
{
	va_list args;

	if (!g_initialized)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	va_start(args, numArgs);
	return dispatch(func, numArgs, args, false);
}

/* sceCommonDialogSetConfigParam, on the worker thread */
int SetConfigParamImpl(const CommonDialogConfig *configParam)
{
	AppmgrNotificationRequest request;
	const char *language;
	const char *enterButton;

	if (configParam == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (!sceKernelIsCDialogAvailable())
		return SCE_COMMON_DIALOG_ERROR_NOT_AVAILABLE;
	if (g_work.slot[0].state != COMMON_DIALOG_SLOT_FREE)
		return SCE_COMMON_DIALOG_ERROR_BUSY;
	switch (configParam->language) {
	case 0:
		language = "Japanese";
		break;
	case 1:
		language = "English(US)";
		break;
	case 2:
		language = "French";
		break;
	case 3:
		language = "Spanish";
		break;
	case 4:
		language = "German";
		break;
	case 5:
		language = "Italian";
		break;
	case 6:
		language = "Dutch";
		break;
	case 7:
		language = "Portuguese(PT)";
		break;
	case 8:
		language = "Russian";
		break;
	case 9:
		language = "Korean";
		break;
	case 10:
		language = "Chinese(T)";
		break;
	case 11:
		language = "Chinese(S)";
		break;
	case 12:
		language = "Finnish";
		break;
	case 13:
		language = "Swedish";
		break;
	case 14:
		language = "Danish";
		break;
	case 15:
		language = "Norwegian";
		break;
	case 16:
		language = "Polish";
		break;
	case 17:
		language = "Portuguese(BR)";
		break;
	case 18:
		language = "English(GB)";
		break;
	case 19:
		language = "Turkish";
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_LANGUAGE;
	}
	switch (configParam->enterButtonAssign) {
	case 0:
		enterButton = "Circle";
		break;
	case 1:
		enterButton = "Cross";
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ENTER_BUTTON_ASSIGN;
	}

	sceClibMemset(&request, 0, sizeof(request));
	sceClibSnprintf((char *)request.payload, sizeof(request.payload) - 1,
			"CommonDialog:\nReq TRC R3088\nSetConfigParam\n%s, %s", language, enterButton);
	sceAppMgrSendNotificationRequest(&request, 0);
	g_work.config = *configParam;
	g_work.hasConfig = true;
	return SCE_OK;
}

int SetConfigParam(const CommonDialogConfig *configParam)
{
	if (!g_initialized)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	return callByWorkerThread((void *)SetConfigParamImpl, 1, configParam);
}

bool IsRunning(void)
{
	return g_work.slot[0].dialogStatus == SCE_COMMON_DIALOG_STATUS_RUNNING;
}

int Update(const SceCommonDialogUpdateParam *updateParam)
{
	if (sceKernelGetThreadId() == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (g_work.slot[0].dialogStatus != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_OK;
	return callByWorkerThread((void *)UpdateImpl, 1, updateParam);
}

SceUID GetWorkerThreadId(void)
{
	return getWorkerThreadId();
}

int OnInit(int dialogType, void *param)
{
	return callByWorkerThread((void *)InitImpl, 4, dialogType, false, param, NULL);
}

int OnInit2(int dialogType, void *param, void *arg)
{
	return callByWorkerThread((void *)InitImpl, 4, dialogType, true, param, arg);
}

int NestInit(int dialogType, void *param, void *arg)
{
	SceUID thread;

	thread = sceKernelGetThreadId();
	if (g_work.cdialogMode) {
		if (g_work.pafMainThread != thread)
			return COMMON_DIALOG_ERROR_NOT_PAF_MAIN_THREAD;
	} else if (g_work.pafMainThread == -1) {
		setWorkerThreadOwner(thread);
	}
	return callByWorkerThread((void *)NestInitImpl, 3, dialogType, param, arg);
}

int Term(int dialogType)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	return callByWorkerThread((void *)TermImpl, 1, dialogType);
}

int AllocateWorkMemory(int dialogType, SceSize size)
{
	CommonDialogSlot *slot;
	char name[32];
	SceUInt32 type;
	SceUID thread;
	int i;
	int ret;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	slot = &g_work.slot[i];
	if (slot->workMemory != NULL)
		return COMMON_DIALOG_ERROR_ALREADY_EXISTS;
	if (size > CDLG_WORK_MEMORY_MAX)
		return COMMON_DIALOG_ERROR_NO_BUDGET;
	type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
	if (g_work.cdialogMode)
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	sceClibSnprintf(name, sizeof(name), "SceCommonDialogWorkMemory%u", i);
	ret = sceKernelAllocMemBlock(name, type, (size + 4095) & ~4095, NULL);
	if (ret < 0)
		return ret;
	slot->workMemoryBlock = ret;
	ret = sceKernelGetMemBlockBase(ret, &g_work.slot[i].workMemory);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return SCE_OK;
}

void *GetWorkMemory(int dialogType)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal() || i < 0 || !isSlotCaller(thread, i))
		return NULL;
	return g_work.slot[i].workMemory;
}

int GetStatus(int dialogType)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal() || i < 0 || !isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_STATUS_NONE;
	return g_work.slot[i].dialogStatus;
}

int GetUserData(int dialogType)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal() || i < 0 || !isSlotCaller(thread, i))
		return 0;
	return g_work.slot[i].userData;
}

int SetUserData(int dialogType, int userData)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	g_work.slot[i].userData = userData;
	return SCE_OK;
}

int SetParamReady(int dialogType)
{
	SceUID thread;
	int i;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	g_work.slot[i].paramReady = true;
	return SCE_OK;
}

int DispatchApiCall(int dialogType, void *func, SceUInt32 numArgs, ...)
{
	va_list args;
	SceUID thread;
	int i;

	va_start(args, numArgs);
	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	return dispatch(func, numArgs, args, false);
}

int LockDialog(int dialogType)
{
	SceUID thread;
	int i;
	int ret;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	ret = sceKernelLockLwMutex(&g_work.slot[i].mutex, 1, NULL);
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE)
		return COMMON_DIALOG_ERROR_LOCK_FAILED;
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return ret;
}

int UnlockDialog(int dialogType)
{
	SceUID thread;
	int i;
	int ret;

	i = findSlot(dialogType);
	thread = sceKernelGetThreadId();
	if (thread == sceGxmGetDisplayQueueThreadIdInternal())
		return SCE_COMMON_DIALOG_ERROR_ILLEGAL_CALLER_THREAD;
	if (i < 0)
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	if (!isSlotCaller(thread, i))
		return SCE_COMMON_DIALOG_ERROR_NOT_IN_USE;
	ret = sceKernelUnlockLwMutex(&g_work.slot[i].mutex, 1);
	if (ret == SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED)
		return COMMON_DIALOG_ERROR_UNLOCK_FAILED;
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return ret;
}

bool isApplicationMode(void)
{
	return !g_work.cdialogMode;
}

/* the info bar the dialog's parameter asks for (main modules of SDK 1.80 and later may not
   ask for the application's), and whether it shows */
int setInfobar(const SceCommonDialogInfobarParam *infobarParam, bool *infobarShown)
{
	SceAppMgrAppStatus status;
	bool shown;
	int result;
	int ret;

	result = SCE_OK;
	shown = false;
	if (infobarParam != NULL) {
		if (g_work.sdkVersion >= CDLG_SDK_VERSION_INFOBAR_CHECK
		    && infobarParam->visibility != CDLG_INFOBAR_HIDDEN
		    && infobarParam->visibility != CDLG_INFOBAR_VISIBLE)
			result = SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM;
		if (sceAppMgrSetInfobarStateForCommonDialog(infobarParam->visibility,
							    infobarParam->color,
							    infobarParam->transparency) != SCE_OK) {
			result = SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM;
		} else if (infobarShown != NULL) {
			if (infobarParam->visibility == CDLG_INFOBAR_VISIBLE)
				shown = true;
			*infobarShown = shown;
		}
		return result;
	}

	ret = sceAppMgrGetStatusByName(NULL, &status);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	ret = sceAppMgrSetInfobarStateForCommonDialog(INFOBAR_VALUE(status.infobar.visibility),
						      INFOBAR_VALUE(status.infobar.color),
						      INFOBAR_VALUE(status.infobar.transparency));
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	if (infobarShown != NULL)
		*infobarShown = INFOBAR_VALUE(status.infobar.visibility) == CDLG_INFOBAR_VISIBLE;
	return result;
}

/* shows the info bar as the application has it, or the plain one */
static inline void showApplicationInfobar(void)
{
	SceAppMgrAppStatus status;

	CDLG_ASSERT_OK(sceAppMgrGetStatusByName(NULL, &status));
	if (INFOBAR_VALUE(status.infobar.visibility) == CDLG_INFOBAR_VISIBLE)
		CDLG_ASSERT_OK(sceAppMgrSetInfobarStateForCommonDialog(CDLG_INFOBAR_VISIBLE,
						INFOBAR_VALUE(status.infobar.color),
						INFOBAR_VALUE(status.infobar.transparency)));
	else
		CDLG_ASSERT_OK(sceAppMgrSetInfobarStateForCommonDialog(CDLG_INFOBAR_VISIBLE, 0, 0));
}

void restoreInfobar(void)
{
	showApplicationInfobar();
}

/* the parameter every dialog's has: its magic, the info bar, the background and the dimmer */
int checkCommonParam(const CommonDialogParamOption *option, const SceCommonDialogParam *commonParam,
		     bool *infobarShown)
{
	CommonDialogParamOption defaultOption;
	const SceCommonDialogInfobarParam *infobarParam;
	const SceCommonDialogColor *dimmerColor;
	SceUInt32 magic;
	bool keepInfobar;
	int initialized;

	if (commonParam == NULL)
		goto invalid;
	magic = commonParam->magic;
	if (g_work.sdkVersion < CDLG_SDK_VERSION_MAGIC_CHECK) {
		if (magic != 0)
			sceClibPrintf("[SceCommonDialog] *WARNING* Possible uninitialized parameter "
				      "detected. (%x)\n", magic);
	} else if (magic != SCE_COMMON_DIALOG_MAGIC_NUMBER + (SceUInt32)commonParam) {
invalid:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	initialized = sceGxmIsInitializationInternal();
	if (g_work.sdkVersion >= CDLG_SDK_VERSION_GXM_CHECK && initialized == 0)
		return SCE_COMMON_DIALOG_ERROR_GXM_IS_UNINITIALIZED;

	defaultOption.hideInfobar = false;
	defaultOption.keepInfobar = false;
	defaultOption.drawPriority = 229;
	if (option == NULL)
		option = &defaultOption;
	keepInfobar = option->keepInfobar;
	if (option->keepInfobar && option->hideInfobar)
		return SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM;
	infobarParam = commonParam->infobarParam;
	if (infobarParam != NULL) {
		if (option->keepInfobar)
			return SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM;
		if (option->hideInfobar)
			return SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM;
	}
	if (g_work.cdialogMode && commonParam->bgColor != NULL && commonParam->bgColor->a != 0
	    && commonParam->bgColor->a != 255)
		return SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR;

	dimmerColor = commonParam->dimmerColor;
	if (g_work.cdialogMode) {
		if (dimmerColor == NULL)
			goto noDimmer;
		/* a game's dimmer is transparent or opaque black */
		if (!(dimmerColor->r == 0 && dimmerColor->g == 0 && dimmerColor->b == 0
		      && dimmerColor->a == 0)
		    && !(dimmerColor->r == 0 && dimmerColor->g == 0 && dimmerColor->b == 0
			 && dimmerColor->a == 255))
			return SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR;
	}
	if (dimmerColor != NULL && (dimmerColor->r != 0 || dimmerColor->g != 0 || dimmerColor->b != 0
				    || dimmerColor->a != 0)) {
		g_work.dimmerColor.r = dimmerColor->r;
		g_work.dimmerColor.g = dimmerColor->g;
		g_work.dimmerColor.b = dimmerColor->b;
		g_work.dimmerColor.a = dimmerColor->a;
		g_work.drawPriority = option->drawPriority;
		g_work.hasDimmerColor = true;
	} else {
noDimmer:
		g_work.hasDimmerColor = false;
	}

	if (keepInfobar) {
		showApplicationInfobar();
		if (infobarShown != NULL)
			*infobarShown = true;
		return SCE_OK;
	}
	if (option->hideInfobar) {
		CDLG_ASSERT_OK(sceAppMgrSetInfobarStateForCommonDialog(CDLG_INFOBAR_HIDDEN, 0, 0));
		if (infobarShown != NULL)
			*infobarShown = false;
		return SCE_OK;
	}
	return setInfobar(infobarParam, infobarShown);
}

int checkCommonParamWithInfobar(const SceCommonDialogParam *commonParam, bool *infobarShown,
				bool keepInfobar, bool hideInfobar)
{
	CommonDialogParamOption option;

	option.keepInfobar = keepInfobar;
	option.hideInfobar = hideInfobar;
	option.drawPriority = 229;
	return checkCommonParam(&option, commonParam, infobarShown);
}

bool isCommonDialogError(int error)
{
	return error >= SCE_COMMON_DIALOG_ERROR_BUSY && error <= SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
}

void clearWorkFlags(void)
{
	clearMainWorkFlags(&g_work);
}

extern "C" int module_start(SceSize args, const void *argp)
{
	if (args == 1)
		g_moduleStartSkipped = *(const bool *)argp;
	if (g_moduleStartSkipped)
		return SCE_KERNEL_START_SUCCESS;
	return initialize();
}

extern "C" int module_stop(SceSize args, const void *argp)
{
	if (g_moduleStartSkipped)
		return SCE_KERNEL_STOP_SUCCESS;
	return finalize();
}

extern "C" int module_exit(SceSize args, const void *argp)
{
	if (g_moduleStartSkipped)
		return SCE_KERNEL_STOP_SUCCESS;
	return finalize();
}
