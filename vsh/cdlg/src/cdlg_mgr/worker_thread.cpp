/*
 * SceCommonDialog's worker thread: every dialog's API call runs on it, one at
 * a time under the API mutex, so that a dialog's calls never race with each
 * other. A caller hands the function and its arguments to the thread and
 * waits for the result; the worker thread itself and the thread that owns it
 * for a while (the PafMain thread in a game's common dialog mode, the caller
 * of a nested Init) call directly. Without the common dialog's budget there is
 * no worker thread, and the calls run on the caller under the mutex.
 *
 * Sony's names (the 0.9x builds' messages): initializeWorkerThread,
 * finalizeWorkerThread, dispatch. The other names are ours.
 */
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include <sysmem/memblock.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define CDLG_PRINT_FATAL(ret)						\
	sceClibPrintf("[SceCommonDialog] INTERNAL ASSERION FAILURE (0x%x)\n", (ret))

/* the most arguments an API call passes */
#define CDLG_DISPATCH_ARG_MAX		8

/* the worker thread */
#define CDLG_WORKER_PRIORITY		(SCE_KERNEL_DEFAULT_PRIORITY - 18)
#define CDLG_WORKER_PRIORITY_GAME	(SCE_KERNEL_DEFAULT_PRIORITY - 4)
#define CDLG_WORKER_STACK_SIZE		0x10000

/* the title whose worker keeps the default stack */
#define CDLG_TITLE_DEFAULT_STACK	"PCSG00118"

typedef struct CommonDialogWorker {
	SceUID thread;
	/* a thread that calls directly while it owns the worker */
	SceUID owner;
	bool exitRequested;
	/* no worker thread: the calls run on the caller */
	bool direct;
	SceUInt8 reserved10[6];
	SceKernelLwMutexWork apiMutex;
	SceUID apiMutexId;
	SceUID requestSema;
	SceUID resultSema;
	/* the call the worker thread runs, and its result */
	void *func;
	SceUInt32 numArgs;
	SceUInt32 args[CDLG_DISPATCH_ARG_MAX];
	int result;
} CommonDialogWorker;

static CommonDialogWorker s_worker = {
	-1,
	-1,
};

typedef int (*CommonDialogApi0)(void);
typedef int (*CommonDialogApi1)(SceUInt32);
typedef int (*CommonDialogApi2)(SceUInt32, SceUInt32);
typedef int (*CommonDialogApi3)(SceUInt32, SceUInt32, SceUInt32);
typedef int (*CommonDialogApi4)(SceUInt32, SceUInt32, SceUInt32, SceUInt32);
typedef int (*CommonDialogApi5)(SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32);
typedef int (*CommonDialogApi6)(SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32);
typedef int (*CommonDialogApi7)(SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32,
				SceUInt32);
typedef int (*CommonDialogApi8)(SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32, SceUInt32,
				SceUInt32, SceUInt32);

int callFunction(void *func, SceUInt32 numArgs, const SceUInt32 *args)
{
	switch (numArgs) {
	case 0:
		return ((CommonDialogApi0)func)();
	case 1:
		return ((CommonDialogApi1)func)(args[0]);
	case 2:
		return ((CommonDialogApi2)func)(args[0], args[1]);
	case 3:
		return ((CommonDialogApi3)func)(args[0], args[1], args[2]);
	case 4:
		return ((CommonDialogApi4)func)(args[0], args[1], args[2], args[3]);
	case 5:
		return ((CommonDialogApi5)func)(args[0], args[1], args[2], args[3], args[4]);
	case 6:
		return ((CommonDialogApi6)func)(args[0], args[1], args[2], args[3], args[4], args[5]);
	case 7:
		return ((CommonDialogApi7)func)(args[0], args[1], args[2], args[3], args[4], args[5],
						args[6]);
	case 8:
		return ((CommonDialogApi8)func)(args[0], args[1], args[2], args[3], args[4], args[5],
						args[6], args[7]);
	default:
		return COMMON_DIALOG_ERROR_FATAL;
	}
}

int workerThread(SceSize args, void *argp)
{
	int ret;

	for (;;) {
		ret = sceKernelWaitSema(s_worker.requestSema, 1, NULL);
		if (ret != SCE_OK)
			goto fatal;
		if (s_worker.exitRequested)
			break;
		s_worker.result = callFunction(s_worker.func, s_worker.numArgs, s_worker.args);
		ret = sceKernelSignalSema(s_worker.resultSema, 1);
		if (ret != SCE_OK)
			goto fatal;
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return SCE_OK;

fatal:
	CDLG_PRINT_FATAL(ret);
	return COMMON_DIALOG_ERROR_FATAL;
}

int initializeWorkerThread(void)
{
	CommonDialogThreadOptParam opt;
	const SceKernelThreadOptParam *option;
	char titleId[10];
	SceInt32 priority;
	SceUID sema;
	SceUID thread;
	int ret;

	if (s_worker.thread != -1 || s_worker.direct)
		return COMMON_DIALOG_ERROR_ALREADY_EXISTS;
	ret = sceKernelCreateLwMutex(&s_worker.apiMutex, "SceCommonDialogApiMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0)
		return ret;
	s_worker.apiMutexId = ret;

	sema = sceKernelCreateSema("SceCommonDialogDispatchRequest", SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0,
				   1, NULL);
	if (sema < 0) {
		ret = sceKernelDeleteLwMutex(&s_worker.apiMutex);
		if (ret == SCE_OK)
			return sema;
		goto fatal;
	}
	s_worker.requestSema = sema;

	sema = sceKernelCreateSema("SceCommonDialogDispatchResult", SCE_KERNEL_SEMA_ATTR_TH_PRIO, 0, 1,
				   NULL);
	if (sema < 0) {
		ret = sceKernelDeleteLwMutex(&s_worker.apiMutex);
		if (ret != SCE_OK || (ret = sceKernelDeleteSema(s_worker.requestSema)) != SCE_OK)
			goto fatal;
		return sema;
	}
	s_worker.resultSema = sema;
	s_worker.exitRequested = false;

	if (!sceKernelIsCDialogAvailable()) {
		s_worker.direct = true;
		return SCE_OK;
	}

	option = (const SceKernelThreadOptParam *)&opt;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = CDLG_THREAD_OPT_USTACK_MEMTYPE | CDLG_THREAD_OPT_UTLS_MEMTYPE;
	opt.uStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	opt.uTLSMemType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	sceClibMemset(titleId, 0, sizeof(titleId));
	if (sceAppMgrAppParamGetString(0, CDLG_APP_PARAM_TITLE_ID, titleId, sizeof(titleId)) == SCE_OK
	    && sceClibStrncmp(titleId, CDLG_TITLE_DEFAULT_STACK, 9) == 0)
		option = NULL;
	priority = CDLG_WORKER_PRIORITY;
	if (sceKernelIsGameBudget())
		priority = CDLG_WORKER_PRIORITY_GAME;
	thread = sceKernelCreateThread("SceCommonDialogWorker", workerThread, priority,
				       CDLG_WORKER_STACK_SIZE, 0, 0, option);
	if (thread < 0) {
		ret = sceKernelDeleteLwMutex(&s_worker.apiMutex);
		if (ret != SCE_OK || (ret = sceKernelDeleteSema(s_worker.requestSema)) != SCE_OK
		    || (ret = sceKernelDeleteSema(s_worker.resultSema)) != SCE_OK)
			goto fatal;
		return thread;
	}
	ret = sceKernelStartThread(thread, 0, NULL);
	if (ret != SCE_OK)
		goto fatal;
	s_worker.thread = thread;
	return SCE_OK;

fatal:
	CDLG_PRINT_FATAL(ret);
	return COMMON_DIALOG_ERROR_FATAL;
}

int finalizeWorkerThread(void)
{
	int ret;

	if (s_worker.thread == -1 && !s_worker.direct)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockLwMutex(&s_worker.apiMutex, 1, NULL);
	if (ret != SCE_OK)
		goto fatal;
	s_worker.exitRequested = true;
	ret = sceKernelSignalSema(s_worker.requestSema, 1);
	if (ret != SCE_OK)
		goto fatal;
	if (!s_worker.direct) {
		ret = sceKernelWaitThreadEnd(s_worker.thread, NULL, NULL);
		if (ret != SCE_OK)
			goto fatal;
		ret = sceKernelDeleteThread(s_worker.thread);
		if (ret != SCE_OK)
			goto fatal;
	}
	ret = sceKernelDeleteSema(s_worker.requestSema);
	if (ret != SCE_OK)
		goto fatal;
	ret = sceKernelDeleteSema(s_worker.resultSema);
	if (ret != SCE_OK)
		goto fatal;
	ret = sceKernelUnlockLwMutex(&s_worker.apiMutex, 1);
	if (ret != SCE_OK)
		goto fatal;
	ret = sceKernelDeleteLwMutex(&s_worker.apiMutex);
	if (ret != SCE_OK)
		goto fatal;
	s_worker.thread = -1;
	s_worker.direct = false;
	return SCE_OK;

fatal:
	CDLG_PRINT_FATAL(ret);
	return COMMON_DIALOG_ERROR_FATAL;
}

/* runs an API call on the worker thread and returns its result */
int dispatch(void *func, SceUInt32 numArgs, va_list args, bool tryLock)
{
	SceUID thread;
	SceUInt32 i;
	int result;
	int ret;

	if (s_worker.thread == -1 && !s_worker.direct)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	if (func == NULL)
		return COMMON_DIALOG_ERROR_NULL_FUNCTION;
	if (numArgs > CDLG_DISPATCH_ARG_MAX)
		return COMMON_DIALOG_ERROR_TOO_MANY_ARGS;
	thread = sceKernelGetThreadId();

	if (s_worker.direct) {
		SceUInt32 argv[CDLG_DISPATCH_ARG_MAX];

		for (i = 0; i < numArgs; i++)
			argv[i] = va_arg(args, SceUInt32);
		ret = sceKernelLockLwMutex(&s_worker.apiMutex, 1, NULL);
		if (ret != SCE_OK) {
			CDLG_PRINT_FATAL(ret);
			return COMMON_DIALOG_ERROR_FATAL;
		}
		result = callFunction(func, numArgs, argv);
		ret = sceKernelUnlockLwMutex(&s_worker.apiMutex, 1);
		if (ret == SCE_OK)
			return result;
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}

	if (s_worker.thread == thread || s_worker.owner == thread) {
		SceUInt32 argv[CDLG_DISPATCH_ARG_MAX];

		for (i = 0; i < numArgs; i++)
			argv[i] = va_arg(args, SceUInt32);
		return callFunction(func, numArgs, argv);
	}

	if (tryLock) {
		ret = sceKernelTryLockLwMutex(&s_worker.apiMutex, 1);
		if (ret == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
			return COMMON_DIALOG_ERROR_DISPATCH_BUSY;
		if (ret != SCE_OK) {
			CDLG_PRINT_FATAL(ret);
			return COMMON_DIALOG_ERROR_FATAL;
		}
	} else {
		ret = sceKernelLockLwMutex(&s_worker.apiMutex, 1, NULL);
		if (ret != SCE_OK) {
			CDLG_PRINT_FATAL(ret);
			return COMMON_DIALOG_ERROR_FATAL;
		}
	}
	/* Sony's return leaves the mutex locked */
	if (s_worker.exitRequested)
		return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
	s_worker.func = func;
	s_worker.numArgs = numArgs;
	for (i = 0; i < numArgs; i++)
		s_worker.args[i] = va_arg(args, SceUInt32);
	ret = sceKernelSignalSema(s_worker.requestSema, 1);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	ret = sceKernelWaitSema(s_worker.resultSema, 1, NULL);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	result = s_worker.result;
	ret = sceKernelUnlockLwMutex(&s_worker.apiMutex, 1);
	if (ret != SCE_OK) {
		CDLG_PRINT_FATAL(ret);
		return COMMON_DIALOG_ERROR_FATAL;
	}
	return result;
}

void setWorkerThreadOwner(SceUID thread)
{
	s_worker.owner = thread;
}

void clearWorkerThreadOwner(void)
{
	s_worker.owner = -1;
}

SceUID getWorkerThreadId(void)
{
	return s_worker.thread;
}
