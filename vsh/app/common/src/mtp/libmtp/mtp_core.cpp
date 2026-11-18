/*
 * The MTP core ("CORE:"): sceMtpInit and sceMtpEnd, the device thread that receives the
 * host's operations and hands them to the command handler, and the state the shell polls
 * (the public status and progress).
 */
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <udcd/udcd_user.h>
#include <std/string.h>
#include "mtp_internal.h"

/* the core's state: none, initialized, started */
static int s_coreState;
/* the status and progress the shell reads, under s_devMutex */
static int s_publicStatus;
static int s_publicProgress;
static SceUID s_devMutex;
/* exclusive control of the command handler */
static SceUID s_commandSema;
static SceUID s_devThread;
static int s_devThreadRun;
/* sleep and wake of the device thread */
static SceUID s_devEventFlag;
/* an operation is waiting for the command handler */
static int s_pendingOperation;
static MtpOperation s_operation;
static MtpOperation s_response;
static MtpObjectTransfer s_objectTransfer;
static MtpObjectImport s_objectImport;
static int s_cancelRequested;
static int s_importRunning;
static int s_importContextId;

static int waitingForRun(SceSize argSize, void *argp);

static void _mutexLock(void)
{
	int ret = sceKernelLockMutex(s_devMutex, 1, NULL);

	if (ret != 0 && ret != SCE_KERNEL_ERROR_MUTEX_RECURSIVE &&
	    ret != SCE_KERNEL_ERROR_MUTEX_LOCK_OVF)
		MTP_ERR("CORE:", 110, "unable to lock mutex for MTP dev %x\n", ret);
}

static void _mutexUnlock(void)
{
	int ret = sceKernelUnlockMutex(s_devMutex, 1);

	if (ret != 0 && ret != SCE_KERNEL_ERROR_MUTEX_NOT_OWNED)
		MTP_ERR("CORE:", 128, "unable to unlock mutex for MTP dev %x\n", ret);
}

int mtpCoreSetPublicStatus(int status, int progress)
{
	_mutexLock();
	s_publicStatus = status;
	s_publicProgress = progress;
	_mutexUnlock();
	return 0;
}

int mtpCoreWake(int wait)
{
	int ret;
	SceUInt timeout;

	if (mtpDevGetIfType() != MTP_IF_TYPE_NET)
		return 0;
	if (mtpStatusGetState() == 0)
		mtpStatusSetState(7, -1, 0);
	ret = sceKernelPollEventFlag(s_devEventFlag, MTP_CORE_EVF_CONNECTED,
		SCE_KERNEL_EVF_WAITMODE_OR, NULL);
	if (ret == 0)
		return 0;
	if (ret != SCE_KERNEL_ERROR_EVF_COND) {
		MTP_ERR("CORE:", 1072, "failed to poll event flag 0x%x\n", ret);
		return ret;
	}
	ret = sceKernelSetEventFlag(s_devEventFlag, MTP_CORE_EVF_RUN);
	if (ret < 0)
		MTP_ERR("CORE:", 1078, "failed to set event flag 0x%x\n", ret);
	if (wait) {
		timeout = 5000000;
		ret = sceKernelWaitEventFlag(s_devEventFlag, MTP_CORE_EVF_CONNECTED,
			SCE_KERNEL_EVF_WAITMODE_OR, NULL, &timeout);
		if (ret < 0)
			MTP_ERR("CORE:", 1088, "failed in wait event flag 0x%x\n", ret);
		if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL)
			ret = SCE_MTP_ERROR_CANCELED;
	}
	return ret;
}

int mtpCoreHasPendingOperation(void)
{
	int pending = 0;

	if (s_pendingOperation > 0)
		pending = 1;
	return pending;
}

int mtpCoreStart(int ifType, const MtpNetHost *host, MtpCommandHandler handler, int run)
{
	int ret;

	if (s_coreState == MTP_CORE_STATE_NONE) {
		MTP_ERR("CORE:", 391, "mtp core is not initialized\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (s_coreState == MTP_CORE_STATE_STARTED) {
		MTP_ERR("CORE:", 397, "mtp core is already started\n");
		return SCE_MTP_ERROR_ALREADY;
	}
	ret = mtpDevStart(ifType, host);
	if (ret < 0) {
		MTP_ERR("CORE:", 406, "unable to start mtp device 0x%x\n", ret);
		return SCE_MTP_ERROR_DEVICE;
	}
	mtpSpecLoadIniData(0);
	mtpCoreSetPublicStatus(1, -1);
	mtpStatusReset();
	if (ifType == MTP_IF_TYPE_NET) {
		if (sceKernelClearEventFlag(s_devEventFlag, 0) < 0) {
			MTP_ERR("CORE:", 422, "unable to clear event flag\n");
			ret = SCE_MTP_ERROR_NOT_INITIALIZED;
			goto error;
		}
		if (run && sceKernelSetEventFlag(s_devEventFlag, MTP_CORE_EVF_RUN) < 0) {
			MTP_ERR("CORE:", 429, "unable to set event flag\n");
			ret = SCE_MTP_ERROR_NOT_INITIALIZED;
			goto error;
		}
	}
	if (ifType == MTP_IF_TYPE_USB)
		mtpUsbMonitorCreate();
	s_devThreadRun = 1;
	if (sceKernelStartThread(s_devThread, 0, NULL) < 0) {
		MTP_ERR("CORE:", 444, "unable to start MTP device thread\n");
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto error;
	}
	ret = mtpQueueInit(handler);
	if (ret < 0) {
		MTP_ERR("CORE:", 451, "unable to initialize mtpQueueInit 0x%x\n", ret);
		goto error;
	}
	ret = mtpBgCopyIdleTimerInitialize();
	if (ret < 0) {
		MTP_ERR("CORE:", 458,
			"unable to initialize mtpBgCopyIdleTimerInitialize 0x%x\n", ret);
		mtpQueueTerminate();
		mtpBgCopyIdleTimerTerminate();
		goto error;
	}
	s_coreState = MTP_CORE_STATE_STARTED;
	return 0;

error:
	mtpSpecTerminate();
	mtpDevStop();
	return ret;
}

int mtpCoreStop(void)
{
	int ret;
	int result;
	MtpStatus status;

	if (s_coreState == MTP_CORE_STATE_NONE) {
		MTP_ERR("CORE:", 491, "mtp core is not initialized\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (s_coreState != MTP_CORE_STATE_STARTED) {
		MTP_ERR("CORE:", 497, "mtp core is not started\n");
		return SCE_MTP_ERROR_ALREADY;
	}
	if (mtpDevGetIfType() == MTP_IF_TYPE_USB) {
		if (mtpUsbMonitorIsStarted())
			mtpUsbMonitorStop();
		mtpUsbMonitorDelete();
	}
	s_devThreadRun = 0;
	mtpQueueCancelAll(0);
	mtpCoreSetPublicStatus(0, -1);
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET) {
		ret = sceMtpTerminateExecution();
		if (ret < 0 && ret != MTP_ERROR_DISCONNECTED)
			MTP_ERR("CORE:", 524, "sceMtpTerminateExecution failed: 0x%x\n", ret);
		ret = sceKernelSetEventFlag(s_devEventFlag, MTP_CORE_EVF_STOP);
		if (ret < 0)
			MTP_ERR("CORE:", 528, "sceKernelCancelEventFlag failed: 0x%x\n", ret);
	} else {
		ret = mtpDevIfCancelTransfer();
		if (ret < 0 && ret != MTP_ERROR_UNAVAILABLE)
			MTP_ERR("CORE:", 534, "MtpIfCancelTransfer failed: 0x%x\n", ret);
	}
	ret = mtpDevStop();
	if (ret < 0)
		MTP_ERR("CORE:", 543, "unable to stop mtp device 0x%x\n", ret);
	ret = sceKernelWaitThreadEnd(s_devThread, NULL, NULL);
	if (ret < 0)
		MTP_ERR("CORE:", 549, "unable to terminate MTP devicethread: 0x%x\n", ret);
	mtpStatusGet(&status);
	if (status.state != 0)
		mtpStatusSetState(0, 0, 0);
	mtpSpecTerminate();
	result = mtpQueueTerminate();
	if (result < 0)
		MTP_ERR("CORE:", 569, "unable to mtpQueueTerminate 0x%x\n", result);
	mtpBgCopyIdleTimerTerminate();
	s_coreState = MTP_CORE_STATE_INITIALIZED;
	return result;
}

int mtpCoreIsStarted(int *started)
{
	if (started == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	*started = s_coreState == MTP_CORE_STATE_STARTED;
	return 0;
}

int mtpCoreGetIfType(void)
{
	return mtpDevGetIfType();
}

int mtpCoreGetHostInfo(MtpNetHost *host)
{
	return mtpDevGetHostInfo(host);
}

static inline void mtpCoreReset(void)
{
	s_coreState = MTP_CORE_STATE_NONE;
	s_publicStatus = 0;
	s_publicProgress = -1;
	s_devMutex = -1;
	s_commandSema = -1;
	s_devThread = -1;
	s_devThreadRun = 0;
	s_devEventFlag = -1;
	s_pendingOperation = 0;
	mtpUtilMemset(&s_operation, 0, sizeof(s_operation));
	mtpUtilMemset(&s_response, 0, sizeof(s_response));
	mtpUtilMemset(&s_objectTransfer, 0, sizeof(s_objectTransfer));
	mtpUtilMemset(&s_objectImport, 0, sizeof(s_objectImport));
}

int sceMtpInit(const SceMtpInitParam *param)
{
	int ret;
	int priority = 89;
	int xferPriority = 90;
	int affinity;

	if (s_coreState != MTP_CORE_STATE_NONE) {
		MTP_ERR("CORE:", 612, "mtp core is already initialized\n");
		return SCE_MTP_ERROR_ALREADY;
	}
	mtpCoreReset();
	ret = sceKernelCreateMutex("SceMtpDevice", 0, 0, NULL);
	if (ret < 0) {
		MTP_ERR("CORE:", 630, "unable to create create mutex for exclusive control of "
			"publicStatus and publicProgress 0x%x\n", ret);
		goto error;
	}
	s_devMutex = ret;
	ret = sceKernelCreateSema("SceMtpDevice", 0, 1, 1, NULL);
	if (ret < 0) {
		MTP_ERR("CORE:", 638, "unable to create create semaphore for exclusive control of "
			"gMtpCommandHandler 0x%x\n", ret);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto deleteMutex;
	}
	s_commandSema = ret;
	ret = sceKernelCreateEventFlag("SceMtpNet", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (ret < 0) {
		MTP_ERR("CORE:", 650, "unable to create event flag create event flag for sleep/wake "
			"of _devThread 0x%x\n", ret);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto deleteSema;
	}
	s_devEventFlag = ret;
	affinity = 0;
	if (param != NULL) {
		priority = param->devThreadPriority;
		affinity = param->devThreadCpuAffinityMask;
	}
	ret = sceKernelCreateThread("SceMtpDevice", waitingForRun, priority, 0x8000, 0, affinity,
		NULL);
	if (ret < 0) {
		MTP_ERR("CORE:", 668, "unable to create mtp responder thread 0x%x\n", ret);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto deleteEventFlag;
	}
	s_devThread = ret;
	ret = mtpUtilInitialize(0x60000, param->largeBufferFuncs);
	if (ret < 0) {
		MTP_ERR("CORE:", 677, "unable to initialize utilities=0x%x\n", ret);
		ret = SCE_MTP_ERROR_NOT_INITIALIZED;
		goto deleteThread;
	}
	affinity = 0;
	if (param != NULL) {
		xferPriority = param->xferThreadPriority;
		affinity = param->xferThreadCpuAffinityMask;
	}
	ret = mtpXferInitialize(xferPriority, affinity);
	if (ret < 0) {
		MTP_ERR("CORE:", 690, "unable to initialize Xfering functions=0x%x\n", ret);
		goto uninitializeUtil;
	}
	ret = mtpStrgInitialize();
	if (ret < 0) {
		MTP_ERR("CORE:", 697, "unable to initialize Storage functions=0x%x\n", ret);
		goto uninitializeXfer;
	}
	ret = mtpContextInitialize();
	if (ret < 0) {
		MTP_ERR("CORE:", 704, "unable to initialize Context Manager 0x%x\n", ret);
		goto uninitializeXfer;
	}
	ret = mtpXmlAllocatorInitialize();
	if (ret < 0) {
		MTP_ERR("CORE:", 711, "unable to initialize Xml Allocator 0x%x\n", ret);
		goto finalizeContext;
	}
	mtpStatusReset();
	mtpDevInitialize();
	mtpMarlinInitialize();
	ret = mtpCapabilityInfoInitialize();
	if (ret >= 0) {
		s_coreState = MTP_CORE_STATE_INITIALIZED;
		return 0;
	}
	MTP_ERR("CORE:", 725, "unable to initialize CapabilityInfo 0x%x\n", ret);
	mtpCapabilityInfoFinalize();
finalizeContext:
	mtpContextFinalize();
uninitializeXfer:
	mtpXferUninitialize();
uninitializeUtil:
	mtpUtilUninitialize();
deleteThread:
	sceKernelDeleteThread(s_devThread);
deleteEventFlag:
	sceKernelDeleteEventFlag(s_devEventFlag);
deleteSema:
	sceKernelDeleteSema(s_commandSema);
deleteMutex:
	sceKernelDeleteMutex(s_devMutex);
error:
	mtpCoreReset();
	return ret;
}

int sceMtpEnd(void)
{
	int ret;

	if (s_coreState == MTP_CORE_STATE_NONE) {
		MTP_ERR("CORE:", 767, "mtp core is not initialized\n");
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	if (s_coreState == MTP_CORE_STATE_STARTED) {
		MTP_ERR("CORE:", 773, "mtp core is not stopped\n");
		return SCE_MTP_ERROR_NOT_STOPPED;
	}
	mtpCapabilityInfoFinalize();
	mtpDevTerminate();
	ret = mtpXmlAllocatorUninitialize();
	if (ret < 0)
		MTP_ERR("CORE:", 786, "unable to uninitialize Xml Allocator 0x%x\n", ret);
	ret = mtpContextFinalize();
	if (ret < 0)
		MTP_ERR("CORE:", 792, "unable to uninitialize Context Manager 0x%x\n", ret);
	ret = mtpStrgUninitialize();
	if (ret < 0)
		MTP_ERR("CORE:", 798, "unable to uninitialize storage functions 0x%x\n", ret);
	ret = mtpXferUninitialize();
	if (ret < 0)
		MTP_ERR("CORE:", 804, "unable to uninitialize xfer functions 0x%x\n", ret);
	ret = mtpUtilUninitialize();
	if (ret < 0)
		MTP_ERR("CORE:", 810, "unable to uninitialize utilities 0x%x\n", ret);
	ret = sceKernelDeleteThread(s_devThread);
	if (ret < 0)
		MTP_ERR("CORE:", 816, "unable to delete device thread 0x%x\n", ret);
	ret = sceKernelDeleteEventFlag(s_devEventFlag);
	if (ret < 0)
		MTP_ERR("CORE:", 822, "unable to delete device event flag 0x%x\n", ret);
	ret = sceKernelDeleteSema(s_commandSema);
	if (ret < 0)
		MTP_ERR("CORE:", 829, "unable to delete device semaphore 0x%x\n", ret);
	ret = sceKernelDeleteMutex(s_devMutex);
	if (ret < 0)
		MTP_ERR("CORE:", 835, "unable to delete device mutex 0x%x\n", ret);
	mtpCoreReset();
	return 0;
}

int mtpCoreWaitSema(void)
{
	int ret = sceKernelWaitSema(s_commandSema, 1, NULL);

	if (ret < 0)
		MTP_ERR("CORE:", 854, "failed in waiting sema=0x%x\n", ret);
	return ret;
}

int mtpCoreSignalSema(void)
{
	int ret = sceKernelSignalSema(s_commandSema, 1);

	if (ret < 0)
		MTP_ERR("CORE:", 869, "failed in signaling sema=0x%x\n", ret);
	return ret;
}

void mtpCoreSetPendingOperation(int pending)
{
	s_pendingOperation = pending;
}

int mtpCoreGetPendingOperation(void)
{
	return s_pendingOperation;
}

void mtpCoreClearPendingOperation(void)
{
	s_pendingOperation = 0;
}

int mtpCoreGetPublicStatus(int *progress)
{
	int status;

	_mutexLock();
	status = s_publicStatus;
	*progress = s_publicProgress > 0 ? s_publicProgress : -1;
	_mutexUnlock();
	return status;
}

void mtpCoreClearObjectTransfer(void)
{
	mtpUtilMemset(&s_objectTransfer, 0, sizeof(s_objectTransfer));
}

int mtpCoreGetObjectTransferIsFile(void)
{
	return s_objectTransfer.isFile;
}

void mtpCoreSetObjectTransferIsFile(int isFile)
{
	s_objectTransfer.isFile = isFile;
}

SceUInt32 mtpCoreGetObjectTransferSize(void)
{
	return s_objectTransfer.size;
}

void mtpCoreSetObjectTransferSize(SceUInt32 size)
{
	s_objectTransfer.size = size;
}

SceUInt32 mtpCoreGetObjectTransferHandle(void)
{
	return s_objectTransfer.handle;
}

SceUInt32 mtpCoreGetObjectTransferHandle2(void)
{
	return s_objectTransfer.handle;
}

void mtpCoreSetObjectTransferHandle(SceUInt32 handle)
{
	s_objectTransfer.handle = handle;
}

void mtpCoreClearObjectImport(void)
{
	mtpUtilMemset(&s_objectImport, 0, sizeof(s_objectImport));
}

void mtpCoreSetObjectImportHandle(SceUInt32 handle)
{
	s_objectImport.handle = handle;
}

SceUInt32 mtpCoreGetObjectImportHandle(void)
{
	return s_objectImport.handle;
}

void mtpCoreSetObjectTransferPath(const char *path)
{
	sce_paf_strncpy(s_objectTransfer.path, path, sizeof(s_objectTransfer.path) - 1);
}

void mtpCoreObjectTransferNop(void)
{
}

void mtpCoreSetObjectTransferImporting(int importing)
{
	s_objectTransfer.importing = importing;
}

void mtpCoreSetImportRunning(int running)
{
	s_importRunning = running;
}

int mtpCoreGetImportRunning(void)
{
	return s_importRunning;
}

void mtpCoreSetImportContextId(int contextId)
{
	s_importContextId = contextId;
}

void mtpCoreImportNop(void)
{
}

int mtpCoreSleep(void)
{
	int ret;

	if (mtpDevGetIfType() != MTP_IF_TYPE_NET)
		return 0;
	ret = sceKernelClearEventFlag(s_devEventFlag, ~MTP_CORE_EVF_RUN);
	if (ret < 0)
		MTP_ERR("CORE:", 1049, "failed in cancel event flag 0x%x\n", ret);
	return ret;
}

int mtpCoreWakeCancel(void)
{
	int ret;
	SceUInt32 waiting = 0;

	if (mtpDevGetIfType() != MTP_IF_TYPE_NET)
		return 0;
	ret = sceKernelCancelEventFlag(s_devEventFlag, ~(MTP_CORE_EVF_RUN | MTP_CORE_EVF_CONNECTED |
		MTP_CORE_EVF_STOP), &waiting);
	if (ret < 0)
		MTP_ERR("CORE:", 1111, "failed in cancel event flag 0x%x\n", ret);
	return ret;
}

int setConnected(void)
{
	int ret;

	if (mtpDevGetIfType() != MTP_IF_TYPE_NET)
		return 1;
	if (mtpStatusGetState() != 4)
		return 1;
	ret = sceKernelSetEventFlag(s_devEventFlag, MTP_CORE_EVF_CONNECTED);
	if (ret >= 0)
		return 1;
	MTP_ERR("CORE:", 204, "failed in set event flag 0x%x\n", ret);
	return 0;
}

void mtpCoreSetCancelRequested(int requested)
{
	_mutexLock();
	s_cancelRequested = requested;
	_mutexUnlock();
}

int mtpCoreSendData(const void *data, SceSize size, SceUInt64 totalSize)
{
	return mtpDevIfSendData(s_operation.transactionId, data, size, totalSize);
}

int mtpCoreRecvData(void *data, SceSize size, SceUInt64 *totalSize)
{
	return mtpDevIfRecvData(s_operation.transactionId, data, size, totalSize);
}

int mtpCoreDriverSendData(const void *data, SceSize size, SceUInt64 totalSize)
{
	return mtpDevDriverSendData(s_operation.transactionId, data, size, totalSize);
}

int mtpCoreDriverRecvData(void *data, SceSize size, SceUInt64 *totalSize)
{
	return mtpDevDriverRecvData(s_operation.transactionId, data, size, totalSize);
}

int mtpCoreSendResponse(const MtpEvent *event, SceUInt32 contextId)
{
	int ret;
	int cancel;
	MtpContext *context = NULL;
	int type = 1;

	if (contextId != 0 && mtpContextGet(contextId, 0, &context) == 0 && context != NULL)
		type = context->checkVersion;
	ret = mtpStatusCheck(type);
	if (ret < 0)
		return ret;
	_mutexLock();
	cancel = s_cancelRequested;
	_mutexUnlock();
	if (cancel)
		return SCE_MTP_ERROR_CANCELED;
	ret = mtpDevIfSendEvent(&s_operation, event);
	if (ret == MTP_ERROR_TIMEOUT && mtpDevGetIfType() == MTP_IF_TYPE_USB)
		mtpDevIfReset();
	return ret;
}

int mtpCoreSendEvent(const MtpEvent *event)
{
	int ret;
	int cancel;

	_mutexLock();
	cancel = s_cancelRequested;
	_mutexUnlock();
	if (cancel)
		return SCE_MTP_ERROR_CANCELED;
	if (mtpContextIsCanceling())
		return SCE_MTP_ERROR_CANCELED;
	ret = mtpDevIfSendEvent(&s_operation, event);
	if (ret == MTP_ERROR_TIMEOUT && mtpDevGetIfType() == MTP_IF_TYPE_USB)
		mtpDevIfReset();
	return ret;
}

void mtpCoreSetConnectTimeout(int seconds)
{
	mtpDevSetConnectTimeout(seconds);
}

SceUInt16 mtpCoreGetOperationCode(void)
{
	return s_operation.code;
}

/* the device thread: waits for the host, then receives its operations for the command queue */
static int waitingForRun(SceSize argSize, void *argp)
{
	int ret = 0;
	int retry = 0;
	int notify = 0;
	int wait;
	int found;
	int reason;
	SceUInt32 outBits;
	SceUInt timeout;
	UdcdUserDeviceState udcdState;
	MtpStatus status;

	if (!s_devThreadRun)
		goto end;
	do {
		if (ret == MTP_ERROR_DISCONNECTED && mtpDevGetIfType() == MTP_IF_TYPE_USB)
			sceKernelDelayThread(500000);
		if (mtpDevIfIsConnected() == 0 || (mtpDevGetIfType() == MTP_IF_TYPE_USB &&
		    mtpUsbMonitorHasBeenDisconnected() != 0) || retry != 0) {
			mtpCoreSetPublicStatus(2, -1);
			if (s_pendingOperation > 0) {
				mtpCmdAbortOperation(&s_operation, &s_response);
				mtpContextCancelAll();
			}
			mtpQueueCancelAll(0);
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET) {
				wait = sceKernelClearEventFlag(s_devEventFlag, ~MTP_CORE_EVF_CONNECTED);
				if (wait < 0) {
					MTP_ERR("CORE:", 164, "sceKernelClearEventFlag 0x%x\n", wait);
					goto end;
				}
				outBits = 0;
				do {
					if (netMtpDeviceIsWaking() == 0)
						mtpStatusSetState(0, 3, 0);
					timeout = 500000;
					wait = sceKernelWaitEventFlag(s_devEventFlag,
						MTP_CORE_EVF_RUN | MTP_CORE_EVF_STOP,
						SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						&outBits, &timeout);
				} while (wait == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
				if (wait < 0 && wait != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
					MTP_ERR("CORE:", 185, "sceKernelWaitEventFlag 0x%x\n", wait);
					goto end;
				}
				if (outBits & MTP_CORE_EVF_STOP)
					goto end;
			}
			if (mtpDevGetIfType() == MTP_IF_TYPE_USB && mtpUsbMonitorIsStarted())
				mtpUsbMonitorStop();
			found = notify;
			reason = 1;
			if (mtpDevWaitConnect(&found, &reason) == 0) {
				if (!s_devThreadRun)
					goto end;
				mtpStatusSetState(0, reason, 0);
				sceKernelDelayThread(1000000);
				retry = 1;
			} else {
				if (found)
					mtpCallbackNotify(0x200);
				mtpStatusSetState(1, -1, 0);
				retry = 0;
				wait = mtpDevIfWaitConnect(1000000);
				if (wait == 0)
					ret = 0;
				else if (mtpDevGetIfType() == MTP_IF_TYPE_NET &&
					 wait == MTP_ERROR_PROTOCOL)
					mtpCoreWake(0);
			}
		} else {
			if (mtpStatusGetState() == 1) {
				if (mtpDevGetIfType() == MTP_IF_TYPE_USB) {
					mtpUtilMemset(&udcdState, 0, sizeof(udcdState));
					sceUdcdGetDeviceState(&udcdState);
					if (udcdState.connection & SCE_UDCD_STATUS_CONNECTION_ESTABLISHED)
						mtpCallbackNotify(0x10);
				} else if (mtpDevGetIfType() == MTP_IF_TYPE_NET) {
					mtpCallbackNotify(0x10);
				}
			}
			if (ret < 0) {
				if (ret == -1 || ret == MTP_ERROR_UNSUPPORTED ||
				    ret == MTP_ERROR_PROTOCOL) {
					mtpDevIfReset();
					mtpDevStopStartPort();
				}
				ret = 0;
				continue;
			}
			if (mtpDevGetIfType() == MTP_IF_TYPE_USB && !mtpUsbMonitorIsStarted())
				mtpUsbMonitorStart();
			mtpDevIfChangePhase(0);
			if (!setConnected())
				goto end;
			if (mtpBgCopyGetNumTasks() <= 0)
				mtpCoreSetPublicStatus(3, -1);
			else
				mtpCoreSetPublicStatus(4, -1);
			mtpUtilMemset(&s_operation, 0, sizeof(s_operation));
			ret = mtpDevIfRecvCommand(&s_operation);
			if (ret >= 0) {
				ret = mtpQueuePushCommand(&s_operation, 0x10000);
			} else {
				if (!s_devThreadRun)
					goto end;
				if (ret == MTP_ERROR_CANCELLED) {
					if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
						mtpStatusSetState(0, 8, 0);
					sceKernelDelayThread(100000);
				} else if (mtpDevGetIfType() == MTP_IF_TYPE_NET &&
					   ret == MTP_ERROR_DISCONNECTED) {
					mtpStatusGet(&status);
					if (status.reason != 9)
						mtpStatusSetState(0, 4, 0);
				}
			}
		}
	} while (s_devThreadRun);
end:
	mtpQueuePushCommand(&s_operation, 0x20000);
	return notify;
}
