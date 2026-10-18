/* SceDeci4pDbgp debugger protocol. */

#include <fw.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>
#include <kernel/error.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysevent.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysroot.h>
#include <sysmem/sysclib.h>
#include <threadmgr/fast_mutex_object.h>
#include "dbgp_internal.h"

DbgpCB g_dbgp;

const SysrootDbgpOps dbgpHandler = {
	.size = sizeof(SysrootDbgpOps),
	.processStart = processHandlerStartHandler,
	.processExit = processHandlerExitHandler,
	.processKill = processHandlerKillHandler,
	.processSpontaneousExit = processHandlerSpontaneousExitHandler,
	.processExitToSpawn = processHandlerExitToSpawnHandler,
	.exitSpawn = processHandlerExitSpawnHandler,
	.processSuspend = processHandlerSuspendHandler,
	.processResume = processHandlerResumeHandler,
	.appAbort = processHandlerAppAbortHandler,
	.moduleStart = moduleHandlerStartHandler,
	.moduleStop = moduleHandlerStopHandler,
	.isDebugTarget = dbgpIsDebugTarget,
	.isPsButtonCaptured = dbgpIsPsButtonCaptured,
	.swBkptGetOriginalOpcode = swbkptGetOriginalOpcode,
	.codeModified = swbkptSetNewOpcode,
	.getPrxDebugFlag = procGetPrxDebugFlag,
	.suspendProcessAndWaitResume = _dbgpStopProcessForPrxDebug,
	.isProcessAttached = dbgpIsProcessAttached,
	.enableWatchpoint = hwwtptEnableHwwtpt,
	.disableWatchpoint = hwwtptDisableHwwtpt,
	.updateUlobjmgrFlagForDbgp = (int (*)(void *, void *, SceUInt32,
			SceUInt32, SceUInt32 *))_dbgpUpdateUlobjmgrFlagForDbgp,
	.updateUlobjmgrFlagForLibfiber = _dbgpUpdateUlobjmgrFlagForLibfiber,
};

void __attribute__((noreturn)) _dbgpGetBufPt(void)
{
	SCE_PANIC(3173);
}

void __attribute__((noreturn)) _dbgpGetSize(void)
{
	SCE_PANIC(3068);
}

int _dbgpUpdateUlobjmgrFlagForDbgp(DbgpCB *dbgp, DbgpProcCB *proc,
				 SceUInt32 set, SceUInt32 clear,
				 SceUInt32 *flag)
{
	SceKernelIntrStatus state;
	SceUInt32 value;

	if (dbgp == NULL)
		SCE_PANIC(3794);

	state = SceCpuForDriver_D32ACE9E(&dbgp->procLock);
	if (proc != NULL) {
		value = (proc->ulobjmgrFlag & ~clear) | set;
		proc->ulobjmgrFlag = value;
		if (flag != NULL)
			*flag = value;
	} else if (flag != NULL) {
		*flag = set;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->procLock, state);
	return 0;
}

void __attribute__((noreturn)) _dbgpSetSize(void)
{
	SCE_PANIC(3089);
}

int _dbgpUpdateUlobjmgrFlagForLibfiber(SceUID pid, SceUInt32 *flag)
{
	SceKernelIntrStatus state;
	DbgpProcCB *proc;

	if (flag == NULL)
		SCE_PANIC(3830);

	state = SceCpuForDriver_D32ACE9E(&g_dbgp.procLock);
	*flag = 0;

	/* Check each registry slot before loading its process pointer. */
#define CHECK_PROCESS(slot) do {					\
	if (g_dbgp.procActive[slot] != 0) {			\
		proc = g_dbgp.procCB[slot];			\
		if (proc->pid == pid)				\
			goto found;				\
	}							\
} while (0)
	CHECK_PROCESS(0);
	CHECK_PROCESS(1);
	CHECK_PROCESS(2);
	CHECK_PROCESS(3);
	CHECK_PROCESS(4);
	CHECK_PROCESS(5);
	CHECK_PROCESS(6);
	CHECK_PROCESS(7);
	CHECK_PROCESS(8);
	CHECK_PROCESS(9);
	CHECK_PROCESS(10);
	CHECK_PROCESS(11);
	CHECK_PROCESS(12);
	CHECK_PROCESS(13);
	CHECK_PROCESS(14);
	CHECK_PROCESS(15);
	CHECK_PROCESS(16);
#undef CHECK_PROCESS
	goto unlock;
found:
	*flag = proc->ulobjmgrFlag;
unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.procLock, state);
	return 0;
}

int dbgpSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	SceKernelIntrStatus state;

	if (resume == 0) {
		if (eventid == 0x102) {
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
			g_dbgp.status |= 4;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
		} else if (eventid == 0x4000) {
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
			g_dbgp.status &= ~4u;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
		}
	} else if (resume == 1) {
		if (eventid == 0x10000) {
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
			g_dbgp.status |= 8;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
		} else if (eventid == 0x400000) {
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
			g_dbgp.status &= ~8u;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
		}
	}
	return 0;
}

static inline __attribute__((always_inline)) void
_dbgpSetBufStatus(DbgpBuf *buf, SceUInt32 clear, SceUInt32 set)
{
	int result;

	result = lockFastMutex((SceKernelFastMutex *)&buf->mutex);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 3150, "_dbgpSetBufStatus",
					  "0x%08x\n", result);
		return;
	}
	buf->status = (buf->status & ~clear) | set;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&buf->mutex);
}

void _dbgpFreeWorkBuf(DbgpProcCB *proc)
{
	DbgpBuf *buf;
	int result;

	if (proc == NULL)
		SCE_PANIC_FUNC(3030, "_dbgpFreeBuf");
	buf = &proc->buf[2];
	if ((buf->status & 1) == 0)
		return;
	if ((buf->status & 0x10) != 0) {
		result = SceSysmemForDriver_009E1C61(buf->extendedMemBlock);
		if (result < 0)
			SCE_PRINTF_WITH_INFO_FUNC(15, 3051, "_dbgpFreeBuf",
						  "0x%08x\n", result);
		_dbgpSetBufStatus(buf, 0x10, 0);
	}
	_dbgpSetBufStatus(buf, ~0u, 0);
}

void _dbgpFreeBuf(DbgpProcCB *proc, unsigned int which)
{
	DbgpBuf *buf;
	int result;

	if (proc == NULL)
		SCE_PANIC(3030);
	if (which > 2)
		SCE_PANIC(3035);
	buf = &proc->buf[which];
	if ((buf->status & 1) == 0)
		return;
	if ((buf->status & 0x10) != 0) {
		result = SceSysmemForDriver_009E1C61(buf->extendedMemBlock);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 3051, "0x%08x\n", result);
		_dbgpSetBufStatus(buf, 0x10, 0);
	}
	_dbgpSetBufStatus(buf, ~0u, 0);
}

int _dbgpReqSend(DbgpProcCB *proc)
{
	SceKernelIntrStatus state;
	SceUID pid;
	int write, next;
	int result;

	if ((g_dbgp.status & 2) == 0)
		return DFMGR_ERROR_NOT_CONNECTED;
	state = SceCpuForDriver_D32ACE9E(&proc->lock);
	proc->status |= 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
	pid = proc->pid;
	state = SceCpuForDriver_D32ACE9E(&g_dbgp.sendLock);
	if (g_dbgp.sendCount > 31) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.sendLock, state);
		state = SceCpuForDriver_D32ACE9E(&proc->lock);
		proc->status &= ~1u;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
		return SCE_ERROR_ERRNO_ENOSPC;
	}
	write = g_dbgp.sendWrite;
	++g_dbgp.sendCount;
	next = write + 1;
	if (next > 31)
		next = 0;
	g_dbgp.sendQueue[write] = pid;
	g_dbgp.sendWrite = next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.sendLock, state);
	result = sceDfMgrReqSend(g_dbgp.protocol);
	if (result >= 0 || result == DFMGR_ERROR_BUSY)
		return 0;
	if (result == DFMGR_ERROR_NOT_CONNECTED) {
		state = SceCpuForDriver_D32ACE9E(&proc->lock);
		proc->status &= ~0x21u;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
		while (g_dbgp.sendCount > 0) {
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.sendLock);
			if (g_dbgp.sendCount > 0) {
				--g_dbgp.sendCount;
				next = g_dbgp.sendRead + 1;
				if (next > 31)
					next = 0;
				g_dbgp.sendRead = next;
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.sendLock, state);
		}
		_dbgpFreeBuf(proc, 1);
		sceKernelSetEventFlag(proc->eventFlag, 0x20000);
		return DFMGR_ERROR_NOT_CONNECTED;
	}
	SCE_PRINTF_WITH_INFO(15, 1126, "0x%08x\n", result);
	state = SceCpuForDriver_D32ACE9E(&proc->lock);
	proc->status &= ~1u;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
	return result;
}

int _dbgpParseThreadInfo(DbgpProcCB *proc)
{
	ThreadInfo info;
	DbgpBuf *buf;
	void *packet, *base, *extendedBase;
	SceUInt32 status;
	int result;

	result = threadInfoLoad(proc, &info);
	if (result < 0)
		return result;
	if (proc == NULL)
		SCE_PANIC_FUNC(2951, "_dbgpAllocBuf");
	buf = &proc->buf[1];
	if ((buf->status & 1) != 0)
		return SCE_ERROR_ERRNO_ENOMEM;
	_dbgpSetBufStatus(buf, 0, 1);
	status = buf->status;
	base = buf->base;
	extendedBase = buf->extendedBase;
	packet = (status & 0x10) != 0 ? extendedBase : base;
	if (packet == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	switch (info.type) {
	case 0:
		result = threadInfoCreateThreadNotification(proc, packet, &info);
		break;
	case 3:
		result = threadInfoCreateExitNotification(proc, packet, &info);
		break;
	case 1:
	case 2:
		result = SCE_KERNEL_ERROR_UNSUP;
		goto error;
	default:
		result = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto error;
	}
	if (result < 0)
		goto error;
	_dbgpFreeWorkBuf(proc);
	result = _dbgpReqSend(proc);
	if (result >= 0)
		return 0;
error:
	_dbgpFreeBuf(proc, 1);
	return result;
}

int _dbgpParseModuleInfo(DbgpProcCB *proc)
{
	ModuleInfo info;
	DbgpBuf *buf;
	void *packet, *base, *extendedBase;
	SceUInt32 status;
	int result;

	result = moduleInfoLoad(proc, &info);
	if (result < 0)
		return result;
	if (proc == NULL)
		SCE_PANIC_FUNC(2951, "_dbgpAllocBuf");
	buf = &proc->buf[1];
	if ((buf->status & 1) != 0)
		return SCE_ERROR_ERRNO_ENOMEM;
	_dbgpSetBufStatus(buf, 0, 1);
	status = buf->status;
	base = buf->base;
	extendedBase = buf->extendedBase;
	packet = (status & 0x10) != 0 ? extendedBase : base;
	if (packet == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	result = _createModuleLoadNotification(proc, packet, &info);
	if (result < 0)
		goto error;
	_dbgpFreeWorkBuf(proc);
	result = _dbgpReqSend(proc);
	if (result >= 0)
		return 0;
error:
	_dbgpFreeBuf(proc, 1);
	return result;
}

int _dbgpStopProcessForPrxDebug(SceUID pid, SceUID modid,
			      SceUInt32 reason, SceUInt64 time)
{
	DbgpProcCB *proc;
	SceUInt32 bits = 0;
	int i, result;

	for (i = 0; i < 17; ++i) {
		if (g_dbgp.procActive[i] == 0)
			continue;
		proc = g_dbgp.procCB[i];
		if (proc != NULL && proc->pid == pid &&
		    (proc->status & 0x10000000) != 0)
			goto found;
	}
	return DFMGR_ERROR_INVALID_ARGUMENT;
found:
	result = sceKernelWaitSema(proc->prxSema, 1, NULL);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 2723, "0x%08x\n", result);
	if (SceThreadmgrForDriver_20C228E4() == 0) {
		proc->pid = pid;
		proc->prxModid = modid;
		proc->prxStopReason = reason;
		proc->time.low = time;
		proc->time.high = time >> 32;
		result = sceKernelSetEventFlag(proc->eventFlag, 0x1000000);
		if (result < 0)
			goto cancelled;
	}
	for (;;) {
		result = sceKernelWaitEventFlag(proc->prxDebugEventFlag,
				0x80000, 5, &bits, NULL);
		if (result == SCE_KERNEL_ERROR_THREAD_SUSPENDED ||
		    result == SCE_KERNEL_ERROR_THREAD_STOPPED)
			goto cancelled;
		if (result >= 0)
			break;
		SCE_PRINTF_WITH_INFO(15, 2772, "0x%08x\n", result);
	}
	result = sceKernelSignalSema(proc->prxSema, 1);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 2782, "0x%08x\n", result);
	return 0;
cancelled:
	return sceKernelSignalSema(proc->prxSema, 1);
}

int _dbgpParseProcessInfo(DbgpProcCB *proc)
{
	ProcessInfo info;
	DbgpBuf *buf;
	void *packet, *base, *extendedBase;
	SceKernelIntrStatus state;
	SceUInt32 status;
	int result;

	result = processInfoLoad(proc, &info);
	if (result < 0)
		return result;
	if (info.type - 1 <= 1) {
		state = SceCpuForDriver_D32ACE9E(&proc->lock);
		proc->status |= 0x40;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
	}
	if (info.type == 5)
		procDebugSuspendProcess(proc->pid);
	if ((info.type & ~2u) == 5) {
		if (info.type == 5) {
			procCreateStopNotification(proc, 0x4000);
			goto freeWork;
		}
		procCreateStopNotification(proc, 0x8000);
		if (proc == NULL)
			SCE_PANIC_FUNC(3030, "_dbgpFreeBuf");
	} else {
		if (proc == NULL)
			SCE_PANIC_FUNC(2951, "_dbgpAllocBuf");
		buf = &proc->buf[1];
		if ((buf->status & 1) != 0)
			return SCE_ERROR_ERRNO_ENOMEM;
		_dbgpSetBufStatus(buf, 0, 1);
		status = buf->status;
		base = buf->base;
		extendedBase = buf->extendedBase;
		packet = (status & 0x10) != 0 ? extendedBase : base;
		if (packet == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		result = processInfoCreateProcessNotification(proc, packet, &info);
		if (result < 0)
			goto error;
	}

	/* Sony inlines _dbgpFreeBuf for the work buffer here. */
freeWork:
	buf = &proc->buf[2];
	if ((buf->status & 1) != 0) {
		if ((buf->status & 0x10) != 0) {
			result = SceSysmemForDriver_009E1C61(buf->extendedMemBlock);
			if (result < 0)
				SCE_PRINTF_WITH_INFO_FUNC(15, 3051, "_dbgpFreeBuf",
							  "0x%08x\n", result);
			_dbgpSetBufStatus(buf, 0x10, 0);
		}
		_dbgpSetBufStatus(buf, ~0u, 0);
	}
	if (info.type == 5) {
		result = sceKernelSignalSema(proc->spontaneousExitSema, 1);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 2604, "0x%08x\n", result);
			goto error;
		}
	}
	result = _dbgpReqSend(proc);
	if (result >= 0)
		return 0;
error:
	_dbgpFreeBuf(proc, 1);
	return result;
}

int dbgpCreateWorkerThread(SceUID pid)
{
	DbgpProcCB *proc = NULL, *allocated;
	SceKernelThreadOptParam opt;
	SceKernelIntrStatus state;
	SceUInt8 *base;
	int active[17];
	int count, slot, result;

	allocated = SceSysmemForDriver_7B4CB60A(g_dbgp.heap, 0x5000);
	if (allocated == NULL) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 3681, "_dbgpAllocDbgpProcCB", "\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	state = SceCpuForDriver_D32ACE9E(&g_dbgp.procLock);
	count = g_dbgp.procCount;
	if (count > 16)
		goto reject;

#define CHECK_PROCESS(slot) do {					\
	active[slot] = g_dbgp.procActive[slot];			\
	if (active[slot] != 0 && g_dbgp.procCB[slot]->pid == pid)	\
		goto reject;					\
} while (0)
	CHECK_PROCESS(0);
	CHECK_PROCESS(1);
	CHECK_PROCESS(2);
	CHECK_PROCESS(3);
	CHECK_PROCESS(4);
	CHECK_PROCESS(5);
	CHECK_PROCESS(6);
	CHECK_PROCESS(7);
	CHECK_PROCESS(8);
	CHECK_PROCESS(9);
	CHECK_PROCESS(10);
	CHECK_PROCESS(11);
	CHECK_PROCESS(12);
	CHECK_PROCESS(13);
	CHECK_PROCESS(14);
	CHECK_PROCESS(15);
	CHECK_PROCESS(16);
#undef CHECK_PROCESS
#define SELECT_PROCESS(index) do {				\
	if (active[index] == 0) {					\
		slot = index;					\
		goto available;					\
	}							\
} while (0)
	SELECT_PROCESS(0);
	SELECT_PROCESS(1);
	SELECT_PROCESS(2);
	SELECT_PROCESS(3);
	SELECT_PROCESS(4);
	SELECT_PROCESS(5);
	SELECT_PROCESS(6);
	SELECT_PROCESS(7);
	SELECT_PROCESS(8);
	SELECT_PROCESS(9);
	SELECT_PROCESS(10);
	SELECT_PROCESS(11);
	SELECT_PROCESS(12);
	SELECT_PROCESS(13);
	SELECT_PROCESS(14);
	SELECT_PROCESS(15);
	SELECT_PROCESS(16);
#undef SELECT_PROCESS
reject:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.procLock, state);
	result = sceKernelFreeHeapMemory(g_dbgp.heap, allocated);
	if (result < 0)
		SCE_PRINTF_WITH_INFO_FUNC(15, 3718, "_dbgpAllocDbgpProcCB",
					  "0x%08x\n", result);
	return SCE_ERROR_ERRNO_ENOMEM;
available:
	g_dbgp.procCB[slot] = allocated;
	g_dbgp.procActive[slot] = 1;
	g_dbgp.procCount = count + 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.procLock, state);
	memset(allocated, 0, sizeof(*allocated));
	allocated->pid = pid;
	proc = allocated;
	if (count < 0)
		return SCE_ERROR_ERRNO_ENOMEM;
	proc->buf[0].name = "recv";
	proc->buf[2].name = "work";
	proc->buf[1].name = "send";
	result = SceSysmemForDriver_C94850C9("SceDeci4pDbgp",
			SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW, 0x33000, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1171, "0x%08x\n", result);
		return result;
	}
	proc->bufferMemBlock = result;
	result = SceSysmemForDriver_A841EDDA(result, (void **)&base);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1179, "0x%08x\n", result);
		return result;
	}
	proc->buf[1].base = base + 0x11000;
	proc->buf[0].base = base;
	proc->buf[2].base = base + 0x22000;
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&proc->buf[0].mutex,
			"SceDeci4pDbgpRecvBuf", 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1191, "0x%08x\n", result);
		return result;
	}
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&proc->buf[1].mutex,
			"SceDeci4pDbgpSendBuf", 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1199, "0x%08x\n", result);
		return result;
	}
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&proc->buf[2].mutex,
			"SceDeci4pDbgpWorkBuf", 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1207, "0x%08x\n", result);
		return result;
	}
	result = sceKernelCreateEventFlag("SceDeci4pDbgp", 0x1000, 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1216, "0x%08x\n", result);
		return result;
	}
	proc->eventFlag = result;
	result = sceKernelCreateEventFlag("SceDeci4pDbgpPrxDebug", 0x9000, 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1226, "0x%08x\n", result);
		return result;
	}
	proc->prxDebugEventFlag = result;
	result = sceKernelCreateSema("SceDeci4pDbgpPrx", 0, 1, 1, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1236, "0x%08x\n", result);
		return result;
	}
	proc->prxSema = result;
	result = sceKernelCreateSema("SceDeci4pDbgpSponExit", 0x8000, 0, 1, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1246, "0x%08x\n", result);
		return result;
	}
	proc->spontaneousExitSema = result;
	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.kStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	result = sceKernelCreateThread("SceDeci4pDbgp", _dbgpWorkerThread,
				      30, 0x4000, 0, 15, &opt);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1263, "0x%08x\n", result);
		return result;
	}
	proc->workerThread = result;
	state = SceCpuForDriver_D32ACE9E(&proc->lock);
	proc->status |= 0x10000000;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
	result = sceKernelStartThread(proc->workerThread, sizeof(proc), &proc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1279, "0x%08x\n", result);
		return result;
	}
	return 0;
}

int module_start(SceSize args, const void *argp)
{
	SceKernelHeapCreateOpt heapOpt;
	SceDfMgrProtocol protocol;
	SceKernelIntrStatus state;
	int result;

	if (SceDipswForDriver_A98FC2FD(194) == 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_PRINTF_FUNC(11, 188, "DbgpModuleStart", "disabled by dipsw\n");
#endif
		return 1;
	}
	memset(&g_dbgp, 0, sizeof(g_dbgp));
	heapOpt.size = sizeof(heapOpt);
	heapOpt.flags = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE;
	heapOpt.memtype = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	result = SceSysmemForDriver_9328E0E8("SceDeci4pDbgp", 0x10000, &heapOpt);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 3872, "_dbgpMemInit", "0x%08x\n", result);
		return 2;
	}
	g_dbgp.lock = 0;
	g_dbgp.heap = result;
	g_dbgp.processDebugInfo = NULL;
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&g_dbgp.mutex,
			"SceDeci4pDbgpFlowCtl", 0, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 231, "_dbgpInit", "0x%08x\n", result);
		return 2;
	}
	protocol.size = sizeof(protocol);
	protocol.protocol = 0x20000;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	protocol.version = 0x01001601;
	protocol.subVersion = 0x01000300;
	protocol.alias = 0;
	protocol.destination = 0;
	strncpy(protocol.name, "DBGP", sizeof(protocol.name));
	state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
	g_dbgp.status |= 2;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
	if (SceDipswForDriver_A98FC2FD(195) != 0)
		protocol.flags |= DFMGR_PROTOCOL_SDIO;
	else
		protocol.flags |= DFMGR_PROTOCOL_USB;
	result = sceDfMgrOpen(_dbgpEventRecv, &protocol, &g_dbgp);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 264, "_dbgpInit", "0x%08x\n", result);
		return 2;
	}
	g_dbgp.protocol = result;
	if (excpHandlerInit(&g_dbgp) < 0 || intrHandlerInit(&g_dbgp) < 0 ||
	    threadHandlerInit(&g_dbgp) < 0 || processHandlerInit(&g_dbgp) < 0 ||
	    moduleHandlerInit(&g_dbgp) < 0 || procInit(&g_dbgp) < 0 ||
	    swbkptInit(&g_dbgp) < 0 || threadAppObjectInit(&g_dbgp) < 0)
		return 2;
	sceKernelRegisterSysEventHandler("SceDeci4pDbgp", dbgpSysEventHandler, NULL);
	SceSysrootForDriver_3999F917(&dbgpHandler);
	if (dbgpCreateWorkerThread(0) < 0)
		return 2;
	return 0;
}

int dbgpDeleteWorkerThread(SceUID pid)
{
	DbgpProcCB *proc;
	SceKernelIntrStatus state;
	int i, slot, result, exitStatus;

	for (i = 0; i < 17; ++i) {
		if (g_dbgp.procActive[i] == 0)
			continue;
		proc = g_dbgp.procCB[i];
		if (proc != NULL && proc->pid == pid &&
		    (proc->status & 0x10000000) != 0)
			goto found;
	}
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
found:
	result = sceKernelSetEventFlag(proc->eventFlag, 0x10000000);
	if (result < 0)
		return result;

#define DELETE_RESOURCE(call, line) do {				\
	result = (call);						\
	if (result < 0)						\
		SCE_PRINTF_WITH_INFO(15, line, "0x%08x\n", result);	\
} while (0)
	DELETE_RESOURCE(sceKernelWaitThreadEnd(proc->workerThread,
					     &exitStatus, NULL), 1313);
	DELETE_RESOURCE(sceKernelDeleteThread(proc->workerThread), 1320);
	DELETE_RESOURCE(sceKernelDeleteEventFlag(proc->eventFlag), 1332);
	DELETE_RESOURCE(sceKernelDeleteEventFlag(proc->prxDebugEventFlag), 1339);
	DELETE_RESOURCE(sceKernelDeleteSema(proc->spontaneousExitSema), 1346);
	DELETE_RESOURCE(sceKernelDeleteSema(proc->prxSema), 1353);
	DELETE_RESOURCE(sceKernelFinalizeFastMutex(
			(SceKernelFastMutex *)&proc->buf[2].mutex), 1360);
	DELETE_RESOURCE(sceKernelFinalizeFastMutex(
			(SceKernelFastMutex *)&proc->buf[1].mutex), 1367);
	DELETE_RESOURCE(sceKernelFinalizeFastMutex(
			(SceKernelFastMutex *)&proc->buf[0].mutex), 1374);
	DELETE_RESOURCE(SceSysmemForDriver_009E1C61(proc->bufferMemBlock), 1381);
#undef DELETE_RESOURCE

	state = SceCpuForDriver_D32ACE9E(&g_dbgp.procLock);
#define FIND_PROCESS(index) do {					\
	if (g_dbgp.procActive[index] != 0) {			\
		proc = g_dbgp.procCB[index];			\
		if (proc->pid == pid) {				\
			slot = index;				\
			goto release;				\
		}						\
	}							\
} while (0)
	FIND_PROCESS(0);
	FIND_PROCESS(1);
	FIND_PROCESS(2);
	FIND_PROCESS(3);
	FIND_PROCESS(4);
	FIND_PROCESS(5);
	FIND_PROCESS(6);
	FIND_PROCESS(7);
	FIND_PROCESS(8);
	FIND_PROCESS(9);
	FIND_PROCESS(10);
	FIND_PROCESS(11);
	FIND_PROCESS(12);
	FIND_PROCESS(13);
	FIND_PROCESS(14);
	FIND_PROCESS(15);
	FIND_PROCESS(16);
#undef FIND_PROCESS
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.procLock, state);
	result = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	goto error;
release:
	result = sceKernelFreeHeapMemory(g_dbgp.heap, proc);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(15, 3765, "_dbgpFreeDbgpProcCB",
					  "0x%08x\n", result);
		goto error;
	}
	--g_dbgp.procCount;
	g_dbgp.procCB[slot] = NULL;
	g_dbgp.procActive[slot] = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.procLock, state);
	return 0;
error:
	SCE_PRINTF_WITH_INFO(15, 1388, "0x%08x\n", result);
	return 0;
}

int dbgpDetachAllProcess(DbgpCB *dbgp)
{
	DbgpProcCB *proc;
	SceUID *threads;
	SceUID pid;
	int i, result, alreadySuspended = 0;

	threads = SceSysmemForDriver_7B4CB60A(dbgp->heap, 0x1000);
	if (threads == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	for (i = 0; i < 17; ++i) {
		if (i == 0)
			continue;
		if (dbgp->procActive[i] == 0)
			continue;
		proc = dbgp->procCB[i];
		if (proc == NULL)
			continue;
		pid = proc->pid;
		if (checkInvalidProcess(pid) != 0) {
			result = dbgpDeleteWorkerThread(pid);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 1438, "0x%08x\n", result);
			continue;
		}
		result = processSuspendProcessByDBGP(pid, &alreadySuspended);
		if (result == SCE_KERNEL_ERROR_WAIT_DELETE) {
			result = dbgpDeleteWorkerThread(pid);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 1451, "0x%08x\n", result);
			continue;
		}
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1457, "0x%08x:0x%08x\n", pid, result);
			result = dbgpDeleteWorkerThread(pid);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 1460, "0x%08x\n", result);
			continue;
		}
		if (processWaitThreadSuspendForProcess(pid, threads, 2) < 0) {
			result = dbgpDeleteWorkerThread(pid);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 1472, "0x%08x\n", result);
			continue;
		}
		result = swbkptClearSwbrkptDetach(proc, pid);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 1482, "0x%08x\n", result);
		result = hwwtptClearHwwtpt(proc, pid, -1, 0);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 1490, "0x%08x\n", result);
		processClearDebugInfo(pid);
		result = dbgpDeleteWorkerThread(pid);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 1500, "0x%08x\n", result);
		result = processThreadStatusIntegration(pid, 2, threads);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1508, "0x%08x\n", result);
			continue;
		}
		result = processResumeProcessByDBGP(pid, alreadySuspended);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 1516, "0x%08x\n", result);
	}
	result = sceKernelFreeHeapMemory(dbgp->heap, threads);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1524, "0x%08x\n", result);
		return result;
	}
	return 0;
}

static inline __attribute__((always_inline)) DbgpProcCB *
dbgpFindProcess(SceUID pid)
{
	DbgpProcCB *proc;
	int i;

	for (i = 0; i < 17; ++i) {
		if (g_dbgp.procActive[i] == 0)
			continue;
		proc = g_dbgp.procCB[i];
		if (proc != NULL && proc->pid == pid &&
		    (proc->status & 0x10000000) != 0)
			return proc;
	}
	return NULL;
}

static inline __attribute__((always_inline)) int
_dbgpRecvData(int protocol, void *buffer, int size)
{
	SceUInt8 *next = buffer;
	int result;

	if (size <= 0)
		return size;
	do {
		result = sceDfMgrRecv(protocol, next, size);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 1043, "_dbgpRecvData",
						  "0x%08x\n", result);
			return result;
		}
		size -= result;
		next += result;
	} while (size > 0);
	return 0;
}

static inline __attribute__((always_inline)) int
_dbgpSendData(int protocol, const void *buffer, int size)
{
	const SceUInt8 *next = buffer;
	int result;

	if (size <= 0)
		return size;
	do {
		result = sceDfMgrSend(protocol, next, size);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 1065, "_dbgpSendData",
						  "0x%08x\n", result);
			return result;
		}
		size -= result;
		next += result;
	} while (size > 0);
	return 0;
}

int _dbgpEventRecv(int event, void *arg, void *ctx)
{
	DbgpCB *dbgp = ctx;
	DbgpProcCB *proc, *kernelProc;
	DbgpBuf *buf;
	DbgpRequestHeader request;
	DbgpResponseHeader *response;
	SceDeci4pHeader discarded;
	const SceDfMgrStatus *linkStatus;
	SceKernelIntrStatus state;
	SceUInt8 *packet;
	void *base, *extendedBase;
	SceUID pid, savedPid;
	SceSize size = (SceSize)arg, headerSize, allocationSize;
	SceUInt32 status, remaining, fragmentSize;
	int i, read, next, result;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		result = lockFastMutex((SceKernelFastMutex *)&g_dbgp.mutex);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 561, "0x%08x\n", result);
			goto error;
		}
		for (i = 0; i < 17; ++i) {
			if (g_dbgp.procActive[i] == 0)
				continue;
			proc = g_dbgp.procCB[i];
			if (proc == NULL || (proc->status & 0x10000000) == 0 ||
			    (proc->buf[0].status & 1) == 0)
				continue;
			_dbgpRecvData(g_dbgp.protocol, &discarded, sizeof(discarded));
			state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
			g_dbgp.status |= 1;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
			sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_dbgp.mutex);
			return DFMGR_ERROR_QUEUE_FULL;
		}
		result = sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_dbgp.mutex);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 588, "0x%08x\n", result);
		result = _dbgpRecvData(g_dbgp.protocol, &request.prefix, sizeof(request.prefix));
		if (result < 0)
			goto error;
		if (size > 23) {
			result = _dbgpRecvData(g_dbgp.protocol, &request.pid, sizeof(request.pid));
			if (result < 0)
				goto error;
			pid = request.pid;
			headerSize = sizeof(request);
		} else {
			pid = 0;
			headerSize = sizeof(request.prefix);
		}
		if (request.prefix.group == 0x20) {
			if (request.prefix.type == 0x12 || (request.prefix.type & 0xef) == 0)
				pid = 0;
		} else if (request.prefix.group == 0x30 && request.prefix.type == 0x14) {
			pid = 0;
		}
		proc = dbgpFindProcess(pid);
		if (proc == NULL) {
			proc = dbgpFindProcess(0);
			if (proc == NULL)
				SCE_PANIC(663);
			pid = proc->pid;
		}
		buf = &proc->buf[0];
		if ((buf->status & 1) != 0)
			return 0;
		_dbgpSetBufStatus(buf, 0, 1);
		if (size > 0x11000) {
			_dbgpSetBufStatus(buf, 0, 0x10);
			allocationSize = (size + 4095) & ~4095u;
			result = SceSysmemForDriver_C94850C9("SceDeci4pDbgp",
					SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW,
					allocationSize, NULL);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO_FUNC(15, 2995, "_dbgpAllocBuf",
							  "0x%08x\n", result);
				return 0;
			}
			buf->extendedMemBlock = result;
			result = SceSysmemForDriver_A841EDDA(result, &extendedBase);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO_FUNC(15, 3003, "_dbgpAllocBuf",
							  "0x%08x\n", result);
				return 0;
			}
			buf->extendedBase = extendedBase;
			buf->extendedSize = allocationSize;
		} else {
			extendedBase = buf->extendedBase;
		}
		status = buf->status;
		base = buf->base;
		packet = (status & 0x10) != 0 ? extendedBase : base;
		if (packet == NULL)
			return 0;
		memcpy(packet, &request, headerSize);
		result = _dbgpRecvData(g_dbgp.protocol, packet + headerSize, size - headerSize);
		if (result < 0)
			goto error;
		buf->size = size;
		_dbgpSetBufStatus(buf, 0, 2);
		g_dbgp.recvPid = pid;
		return 0;

	case SCE_DFMGR_EVENT_REQ_DONE:
		proc = dbgpFindProcess(dbgp->recvPid);
		if (proc == NULL)
			goto invalid;
		result = sceKernelSetEventFlag(proc->eventFlag, 0x10000);
		if (result >= 0)
			return 0;
		goto error;

	case SCE_DFMGR_EVENT_SEND:
		state = SceCpuForDriver_D32ACE9E(&dbgp->sendLock);
		if (dbgp->sendCount <= 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->sendLock, state);
			goto empty;
		}
		read = dbgp->sendRead;
		--dbgp->sendCount;
		next = read + 1;
		if (next > 31)
			next = 0;
		pid = dbgp->sendQueue[read];
		dbgp->sendRead = next;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->sendLock, state);
		if (pid < 0)
			goto empty;
		dbgp->sendPid = pid;
		proc = dbgpFindProcess(pid);
		if (proc == NULL)
			return 0;
		buf = &proc->buf[1];
		status = buf->status;
		base = buf->base;
		extendedBase = buf->extendedBase;
		packet = (status & 0x10) != 0 ? extendedBase : base;
		if ((status & 1) == 0 || (buf->status & 6) != 6 || buf->size == 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 774, "_dbgpEventSend",
						  "0x%08x:%d\n", status, buf->size);
			result = SCE_KERNEL_ERROR_INVALID_FLAGS;
			goto error;
		}
		response = (DbgpResponseHeader *)packet;
		if ((status & 0x20) == 0) {
			fragmentSize = buf->size;
			if (fragmentSize > 0x11000) {
				_dbgpSetBufStatus(buf, 0, 0x20);
				savedPid = response->pid;
				response->flags |= 1;
				proc->fragmentPid = savedPid;
				response->pid = isProcessNotification(response->prefix.group,
					response->prefix.type) != 0 ? proc->pid : 0;
				fragmentSize = 0x11000;
				proc->fragmentOffset = 0x11000;
				proc->fragmentSequence = 0;
			}
			SceDeci4pDfMgrForDebugger_6D26CC56(&response->prefix.hdr, fragmentSize);
			result = _dbgpSendData(dbgp->protocol, packet, fragmentSize);
		} else {
			remaining = buf->size - proc->fragmentOffset;
			++proc->fragmentSequence;
			if (remaining <= 0x10fe4) {
				response->flags &= ~1u;
				response->pid = isProcessNotification(response->prefix.group,
					response->prefix.type) != 0 ? proc->pid : proc->fragmentPid;
				_dbgpSetBufStatus(buf, 0x20, 0);
			}
			fragmentSize = remaining + sizeof(*response);
			if (fragmentSize > 0x11000) {
				fragmentSize = 0x11000;
				remaining = 0x10fe4;
			}
			SceDeci4pDfMgrForDebugger_6D26CC56(&response->prefix.hdr, fragmentSize);
			response->sequence = proc->fragmentSequence;
			result = _dbgpSendData(dbgp->protocol, packet, sizeof(*response));
			if (result < 0) {
				SCE_PRINTF_WITH_INFO_FUNC(15, 821, "_dbgpEventSend",
							  "0x%08x\n", result);
				goto error;
			}
			packet += proc->fragmentOffset;
			proc->fragmentOffset += remaining;
			result = _dbgpSendData(dbgp->protocol, packet, remaining);
		}
		if (result < 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 886, "_dbgpEventSend",
						  "0x%08x\n", result);
			goto error;
		}
		return 0;

	case SCE_DFMGR_EVENT_SENT:
		proc = dbgpFindProcess(dbgp->sendPid);
		if (proc == NULL)
			return 0;
		if ((proc->buf[1].status & 0x20) != 0) {
			result = _dbgpReqSend(proc);
			if (result >= 0)
				return 0;
			_dbgpFreeBuf(proc, 1);
			goto error;
		}
		_dbgpFreeBuf(proc, 1);
		if ((proc->status & 0x40) != 0) {
			kernelProc = dbgpFindProcess(0);
			if (kernelProc == NULL)
				SCE_PANIC_FUNC(928, "_dbgpEventSendDone");
			dbgp->deferredPid = proc->pid;
			result = sceKernelSetEventFlag(kernelProc->eventFlag, 0x20000000);
			if (result < 0)
				SCE_PRINTF_WITH_INFO_FUNC(15, 937, "_dbgpEventSendDone",
							  "0x%08x\n", result);
		}
		result = sceKernelSetEventFlag(proc->eventFlag, 0x20000);
		if (result < 0)
			goto error;
		if (dbgp->sendCount <= 0)
			return 0;
		result = sceDfMgrReqSend(dbgp->protocol);
		if (result >= 0 || result == DFMGR_ERROR_BUSY)
			return 0;
		SCE_PRINTF_WITH_INFO_FUNC(15, 957, "_dbgpEventSendDone", "0x%08x\n", result);
		goto error;

	case SCE_DFMGR_EVENT_STATUS:
		linkStatus = arg;
		if (linkStatus->kind == 0) {
			if (linkStatus->connected == 0)
				goto disconnected;
			if (linkStatus->connected != 1)
				goto invalid;
			state = SceCpuForDriver_D32ACE9E(&dbgp->lock);
			dbgp->status |= 2;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->lock, state);
			if ((dbgp->status & 8) != 0)
				return 0;
			result = dbgpDetachAllProcess(dbgp);
			SCE_PRINTF_WITH_INFO_FUNC(0, 982, "_dbgpEventChangeStatus",
						  "[DECI] cleaned up all debugger sessions\n");
		} else if (linkStatus->kind == 1) {
			if (linkStatus->enabled == 0)
				goto disconnected;
			if (linkStatus->enabled != 1)
				goto invalid;
			state = SceCpuForDriver_D32ACE9E(&dbgp->lock);
			dbgp->status |= 2;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->lock, state);
			result = dbgpDetachAllProcess(dbgp);
		} else {
			goto invalid;
		}
		if (result >= 0)
			return result;
		goto error;
disconnected:
		state = SceCpuForDriver_D32ACE9E(&dbgp->lock);
		dbgp->status &= ~2u;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&dbgp->lock, state);
		return 0;
	case 6:
		return 0;
	default:
		result = SCE_KERNEL_ERROR_UNSUP;
		goto error;
	}
empty:
	dbgp->sendPid = 0;
	result = DFMGR_ERROR_NOTHING_TO_SEND;
	goto error;
invalid:
	result = DFMGR_ERROR_INVALID_ARGUMENT;
error:
	if (result != (int)DFMGR_ERROR_QUEUE_FULL)
		SCE_PRINTF_WITH_INFO_FUNC(15, 517, "dbgpEventHandler",
					  "0x%08x:0x%08x\n", event, result);
	return result;
}

int dbgpClearPrxDebug(DbgpProcCB *proc)
{
	SceKernelIntrStatus state;
	int result;

	state = SceCpuForDriver_D32ACE9E(&proc->lock);
	proc->status &= ~0x100000u;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state);
	result = sceKernelSetEventFlag(proc->prxDebugEventFlag, 0x80000);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 2798, "0x%08x\n", result);
		return result;
	}
	return 0;
}

void dbgpSendError(DbgpProcCB *proc, SceUInt8 group, SceUInt8 type,
		   SceUInt8 code, SceUInt32 result)
{
	DbgpResponseHeader response;
	DbgpBuf *buf;
	void *packet, *base, *extendedBase;
	SceUInt32 status;

	if (proc == NULL)
		SCE_PANIC_FUNC(2951, "_dbgpAllocBuf");
	buf = &proc->buf[1];
#define ALLOC_SEND_BUFFER() do {					\
	packet = NULL;						\
	if ((buf->status & 1) == 0) {				\
		_dbgpSetBufStatus(buf, 0, 1);			\
		status = buf->status;				\
		base = buf->base;				\
		extendedBase = buf->extendedBase;			\
		packet = (status & 0x10) != 0 ? extendedBase : base; \
	}							\
} while (0)
	ALLOC_SEND_BUFFER();
	if (packet == NULL) {
		_dbgpFreeBuf(proc, 1);
		ALLOC_SEND_BUFFER();
		if (packet == NULL) {
			SCE_PRINTF_WITH_INFO(15, 2833, "\n");
			return;
		}
	}
#undef ALLOC_SEND_BUFFER
	memset(packet, 0, sizeof(response));
	buf->size = sizeof(response);
	_dbgpSetBufStatus(buf, 0, 2);
	response.prefix.code = code;
	response.prefix.group = group;
	response.prefix.type = type;
	response.prefix.version = 1;
	response.sequence = 0;
	response.flags = 0;
	response.result = result;
	status = buf->status;
	base = buf->base;
	extendedBase = buf->extendedBase;
	packet = (status & 0x10) != 0 ? extendedBase : base;
	__builtin_memcpy(packet, &response, sizeof(response));
	_dbgpSetBufStatus(buf, 0, 4);
	if (_dbgpReqSend(proc) < 0)
		_dbgpFreeBuf(proc, 1);
}

void dbgpFillResponseHeader(DbgpProcCB *proc, SceUInt8 group, SceUInt8 type,
			    SceUInt8 code, SceUInt32 result)
{
	DbgpResponseHeader response;
	DbgpBuf *buf;
	void *packet, *base, *extendedBase;
	SceUInt32 status;

	response.prefix.group = group;
	response.prefix.type = type;
	response.prefix.code = code;
	response.result = result;
	response.prefix.version = 1;
	response.sequence = 0;
	response.flags = 0;
	if (proc == NULL)
		_dbgpGetBufPt();
	buf = &proc->buf[1];
	status = buf->status;
	base = buf->base;
	extendedBase = buf->extendedBase;
	packet = (status & 0x10) != 0 ? extendedBase : base;
	__builtin_memcpy(packet, &response, sizeof(response));
	_dbgpSetBufStatus(buf, 0, 4);
}

void dbgpFreeRecvBuf(DbgpProcCB *proc)
{
	DbgpBuf *buf;
	DbgpProcCB *other;
	SceKernelIntrStatus state;
	int i, result;

	if (proc == NULL)
		SCE_PANIC_FUNC(3030, "_dbgpFreeBuf");
	buf = &proc->buf[0];
	if ((buf->status & 1) != 0) {
		if ((buf->status & 0x10) != 0) {
			result = SceSysmemForDriver_009E1C61(buf->extendedMemBlock);
			if (result < 0)
				SCE_PRINTF_WITH_INFO_FUNC(15, 3051, "_dbgpFreeBuf",
							  "0x%08x\n", result);
			_dbgpSetBufStatus(buf, 0x10, 0);
		}
		_dbgpSetBufStatus(buf, ~0u, 0);
	}
	if ((g_dbgp.status & 1) == 0)
		return;
	result = lockFastMutex((SceKernelFastMutex *)&g_dbgp.mutex);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 3234, "0x%08x\n", result);
		return;
	}
	for (i = 0; i < 17; ++i) {
		if (g_dbgp.procActive[i] == 0)
			continue;
		other = g_dbgp.procCB[i];
		if (other != NULL && (other->status & 0x10000000) != 0 &&
		    (other->buf[0].status & 1) != 0)
			goto unlock;
	}
	state = SceCpuForDriver_D32ACE9E(&g_dbgp.lock);
	g_dbgp.status &= ~1u;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_dbgp.lock, state);
	sceDfMgrSendSpace(g_dbgp.protocol);
unlock:
	result = sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_dbgp.mutex);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 3258, "0x%08x\n", result);
}

int _dbgpWorkerThread(SceSize args, void *argp)
{
	union {
		DbgpThreadIdentity thread;
		CoredumpInfo coredump;
	} info;
	DbgpProcCB *proc;
	DbgpBuf *buf;
	const DbgpPrefix *request;
	void *packet, *base, *extendedBase;
	SceKernelIntrStatus state;
	SceUInt32 bits, mask, status, reason, pc;
	int result;

	if (args != sizeof(proc))
		SCE_PANIC(1542);
	proc = *(DbgpProcCB **)argp;

#define UPDATE_STATUS(clear, set) do { \
	state = SceCpuForDriver_D32ACE9E(&proc->lock); \
	proc->status = (proc->status & ~(SceUInt32)(clear)) | (set); \
	sceKernelSpinlockLowUnlockCpuResumeIntr(&proc->lock, state); \
} while (0)
#define SET_EVENT(event, line) do { \
	result = sceKernelSetEventFlag(proc->eventFlag, (event)); \
	if (result < 0) \
		SCE_PRINTF_WITH_INFO(15, line, "0x%08x\n", result); \
} while (0)
	for (;;) {
		status = proc->status;
		if ((status & 1) != 0)
			mask = 0x10020000;
		else if ((status & 0x1000) != 0)
			mask = 0x300001;
		else if ((status & 0x8000) != 0)
			mask = 0x200000;
		else if ((status & 0x4000) != 0)
			mask = 1;
		else if ((status & 0x300) != 0)
			mask = 1;
		else if ((status & 0x10000) != 0)
			mask = 0x2000000;
		else if ((status & 2) != 0)
			mask = (status & 0x10) != 0 ? 1985 : 2033;
		else
			mask = (status & 0x10) != 0 ? 1023477697 : 1023477745;
		bits = 0;
		result = sceKernelWaitEventFlag(proc->eventFlag, mask, 5,
					       &bits, NULL);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1602, "0x%08x\n", result);
			continue;
		}
		bits &= mask;
		if ((bits & 0x20000) != 0) {
			UPDATE_STATUS(1, 0);
			if ((proc->status & 0x10000) != 0)
				SET_EVENT(0x2000000, 1622);
			bits &= ~0x20000u;
		} else if ((bits & 0x40) != 0) {
			_dbgpParseProcessInfo(proc);
			if (processInfoGetCount(proc) > 0)
				SET_EVENT(0x40, 1650);
			bits &= ~0x40u;
		} else if ((bits & 0x100) != 0) {
			_dbgpParseModuleInfo(proc);
			if (moduleInfoGetCount(proc) > 0)
				SET_EVENT(0x100, 1677);
			bits &= ~0x100u;
		} else if ((bits & 0x10) != 0) {
			_dbgpParseThreadInfo(proc);
			if (threadInfoGetCount(proc) > 0)
				SET_EVENT(0x10, 1704);
			bits &= ~0x10u;
		} else if ((bits & 0x20) != 0) {
			_dbgpParseThreadInfo(proc);
			if (threadInfoGetCount(proc) > 0)
				SET_EVENT(0x20, 1731);
			bits &= ~0x20u;
		} else if ((bits & 0x200) != 0) {
			_dbgpParseModuleInfo(proc);
			if (moduleInfoGetCount(proc) > 0)
				SET_EVENT(0x200, 1757);
			bits &= ~0x200u;
		} else if ((bits & 0x80) != 0) {
			_dbgpParseProcessInfo(proc);
			if (processInfoGetCount(proc) > 0)
				SET_EVENT(0x80, 1785);
			bits &= ~0x80u;
		} else if ((bits & 0x100000) != 0) {
			if (stepAutoSkipBpPost(proc) < 0) {
				UPDATE_STATUS(0, 0x8000);
				sceKernelSetEventFlag(proc->eventFlag, 0x200000);
			}
			dbgpFreeRecvBuf(proc);
			_dbgpFreeWorkBuf(proc);
			UPDATE_STATUS(0x3000, 0);
			result = _dbgpReqSend(proc);
			if (result < 0)
				goto send_failed;
			bits &= ~0x100000u;
		} else if ((bits & 0x200000) != 0) {
			dbgpFreeRecvBuf(proc);
			_dbgpFreeWorkBuf(proc);
			UPDATE_STATUS(0xb000, 0);
			bits &= ~0x200000u;
		} else if ((bits & 1) != 0) {
			status = proc->status;
			if ((status & 0x200) != 0) {
				UPDATE_STATUS(0x200, 0);
				if (stepThreadSpecificBpPre(proc) >= 0)
					_dbgpFreeWorkBuf(proc);
			} else if ((status & 0x100) != 0) {
				UPDATE_STATUS(0x30, 0);
				UPDATE_STATUS(0x100, 0);
				if (stepThreadSpecificBpPost(proc) >= 0)
					_dbgpFreeWorkBuf(proc);
				else
					UPDATE_STATUS(0, 0x100);
			} else if ((status & 0x1000) != 0) {
				if (stepAutoSkipBpMid(proc) < 0)
					UPDATE_STATUS(0, 0x8000);
				status = proc->status;
				if ((status & 0x6000) == 0 ||
				    sceKernelSetEventFlag(proc->eventFlag,
							   0x100000) >= 0) {
					if ((proc->status & 0x8000) != 0)
						sceKernelSetEventFlag(proc->eventFlag,
								       0x200000);
				}
			} else if (excpInfoLoad(proc, &info.thread, &reason, &pc) < 0) {
				UPDATE_STATUS(2, 0);
			} else {
				result = excpInfoCreateStopNotification(proc,
							&info.thread, reason, pc);
				if (result >= 0) {
					_dbgpFreeWorkBuf(proc);
					UPDATE_STATUS(2, 0);
					if (_dbgpReqSend(proc) >= 0)
						goto exception_done;
				}
				_dbgpFreeBuf(proc, 1);
			}
exception_done:
			if (excpInfoGetCount(proc) > 0)
				SET_EVENT(1, 1874);
			UPDATE_STATUS(0x4000, 0);
			bits &= ~1u;
		} else if ((bits & 0x1000000) != 0) {
			UPDATE_STATUS(0, 0x10000);
			procDebugSuspendProcess(proc->pid);
			result = moduleInfoSendPrxLoadNotificationFake(proc);
			_dbgpFreeWorkBuf(proc);
			if (result < 0) {
				UPDATE_STATUS(0x10000, 0);
			} else {
				result = _dbgpReqSend(proc);
				if (result < 0)
					goto send_failed;
			}
			bits &= ~0x1000000u;
		} else if ((bits & 0x2000000) != 0) {
			UPDATE_STATUS(0, 0x100000);
			UPDATE_STATUS(0x10000, 0);
			procCreateStopNotification(proc, 0x2000);
			_dbgpFreeWorkBuf(proc);
			result = _dbgpReqSend(proc);
			if (result < 0)
				goto send_failed;
			bits &= ~0x2000000u;
		} else if ((bits & 0x4000000) != 0) {
			_dbgpParseProcessInfo(proc);
			bits &= ~0x4000000u;
		} else if ((bits & 0x8000000) != 0) {
			_dbgpParseProcessInfo(proc);
			bits &= ~0x8000000u;
		} else if ((bits & 0x400) != 0) {
			if (coredumpInfoLoad(proc, &info.coredump) < 0)
				goto coredump_done;
			buf = &proc->buf[1];
			if ((buf->status & 1) != 0)
				goto coredump_done;
			_dbgpSetBufStatus(buf, 0, 1);
			packet = buf->base;
			if ((buf->status & 0x10) != 0)
				packet = buf->extendedBase;
			if (packet != NULL) {
				result = coredumpInfoCreateCoredumpNotification(proc,
							packet, &info.coredump);
				if (result >= 0) {
					_dbgpFreeWorkBuf(proc);
					if (_dbgpReqSend(proc) >= 0)
						goto coredump_done;
				}
				_dbgpFreeBuf(proc, 1);
			}
coredump_done:
			if (coredumpInfoGetCount(proc) > 0)
				SET_EVENT(0x400, 2004);
			bits &= ~0x400u;
		} else if ((bits & 0x20000000) != 0) {
			dbgpDeleteWorkerThread(g_dbgp.deferredPid);
			bits &= ~0x20000000u;
		} else if ((bits & 0x10000) != 0) {
			buf = &proc->buf[0];
			extendedBase = buf->extendedBase;
			base = buf->base;
			status = buf->status;
			request = (status & 0x10) != 0 ? extendedBase : base;
#define COMMAND(type, handler) case type: \
	result = handler(proc, request); break
#define UNKNOWN_COMMAND(group) do { \
	dbgpSendError(proc, group, request->type + 1, request->code, 8); \
	result = SCE_KERNEL_ERROR_UNSUP; \
} while (0)
			switch (request->group) {
			case 0x10:
				switch (request->type) {
				COMMAND(0, systemGetConfCmd);
				COMMAND(4, systemGetRegCmd);
				COMMAND(6, _setRegister);
				COMMAND(0x10, systemRdmemCmd);
				COMMAND(0x12, systemWrmemCmd);
				COMMAND(0x30, systemGetHwwatchptCmd);
				COMMAND(0x32, systemSetHwwatchptCmd);
				COMMAND(0x34, systemClearHwwatchptCmd);
				COMMAND(0x48, systemGetSwbrkptCmd);
				COMMAND(0x4a, systemSetSwbrkptCmd);
				COMMAND(0x4c, systemClearSwbrkptCmd);
				default: UNKNOWN_COMMAND(0x10); break;
				}
				break;
			case 0x20:
				switch (request->type) {
				COMMAND(0, processGetProcessInfoCmd);
				COMMAND(0x10, processAttachProcessCmd);
				COMMAND(0x12, processDetachProcessCmd);
				COMMAND(0x20, _getSuspendThreadInfo);
				COMMAND(0x22, processResumeProcessCmd);
				COMMAND(0x24, processSuspendApplicationCmd);
				COMMAND(0x26, processResumeApplicationCmd);
				COMMAND(0x30, processAllocCodePageCmd);
				COMMAND(0x32, processAllocDataPageCmd);
				COMMAND(0x40, processGetMemBlockInfoCmd);
				default: UNKNOWN_COMMAND(0x20); break;
				}
				break;
			case 0x30:
				switch (request->type) {
				COMMAND(0, _getThreadInfoNormal);
				COMMAND(2, _getThreadInfoEx);
				COMMAND(0x10, threadSuspendThreadCmd);
				COMMAND(0x12, threadResumeThreadCmd);
				COMMAND(0x14, threadGetThreadNamesCmd);
				COMMAND(0x16, _getStackInfo);
				COMMAND(0x20, _getSyncPrimitiveInfo);
				COMMAND(0x30, threadGetAppObjectCmd);
				COMMAND(0x32, threadGetFiberStackInfoCmd);
				default: UNKNOWN_COMMAND(0x30); break;
				}
				break;
			case 0x40:
				switch (request->type) {
				COMMAND(0, moduleGetModInfoCmd);
				COMMAND(2, moduleGetMetaDataCmd);
				COMMAND(0x10, moduleGetModuleListCmd);
				default: UNKNOWN_COMMAND(0x40); break;
				}
				break;
			case 0x60:
				if (request->type != 2)
					goto unknown_command;
				result = fileGetFdInfoCmd(proc, request);
				break;
			case 0x70:
				switch (request->type) {
				COMMAND(0, GetNetSystemInfoCmd);
				COMMAND(2, GetNetSockInfoCmd);
				COMMAND(4, GetNetInterfaceInfoCmd);
				COMMAND(6, GetNetResolverInfoCmd);
				COMMAND(8, GetNetEmulationSettingCmd);
				COMMAND(0xa, SetNetEmulationSettingCmd);
				default: UNKNOWN_COMMAND(0x70); break;
				}
				break;
			case 0x80:
				switch (request->type) {
				COMMAND(0, coredumpTriggerCoredumpCmd);
				COMMAND(2, coredumpCancelCoredumpCmd);
				default: UNKNOWN_COMMAND(0x80); break;
				}
				break;
			default:
unknown_command:
				UNKNOWN_COMMAND(request->group);
				break;
			}
#undef COMMAND
#undef UNKNOWN_COMMAND
			_dbgpFreeWorkBuf(proc);
			status = proc->status;
			if ((status & 0x1000) != 0) {
				if ((status & 0x8000) != 0)
					sceKernelSetEventFlag(proc->eventFlag, 0x200000);
			} else {
				dbgpFreeRecvBuf(proc);
				if (result >= 0 && _dbgpReqSend(proc) < 0)
					_dbgpFreeBuf(proc, 1);
			}
			bits &= ~0x10000u;
		} else if ((bits & 0x10000000) != 0) {
			bits &= ~0x10000000u;
			UPDATE_STATUS(0x10000000, 0);
			sceKernelExitThread(0);
			return 0;
		}
		if (bits != 0)
			SET_EVENT(bits, 2068);
	}
send_failed:
	_dbgpFreeBuf(proc, 1);
	return result;
#undef SET_EVENT
#undef UPDATE_STATUS
}

SceSize _dbgpGetRecvSize(DbgpProcCB *proc)
{
	if (proc == NULL)
		_dbgpGetSize();
	return proc->buf[0].size;
}

static inline __attribute__((always_inline)) void *
_dbgpAllocBuf(DbgpProcCB *proc, unsigned int which, SceSize size)
{
	DbgpBuf *buf;
	void *base;
	SceUID memid;
	int result;

	if (proc == NULL)
		SCE_PANIC_FUNC(2951, "_dbgpAllocBuf");
	buf = &proc->buf[which];
	if ((buf->status & 1) != 0)
		return NULL;
	_dbgpSetBufStatus(buf, 0, 1);
	if (size > 0x11000) {
		_dbgpSetBufStatus(buf, 0, 0x10);
		size = (size + 0xfff) >> 12 << 12;
		memid = SceSysmemForDriver_C94850C9("SceDeci4pDbgp", 0x10f0d006,
					       size, NULL);
		if (memid < 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 2995, "_dbgpAllocBuf",
						  "0x%08x\n", memid);
			return NULL;
		}
		buf->extendedMemBlock = memid;
		result = SceSysmemForDriver_A841EDDA(memid, &base);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO_FUNC(15, 3003, "_dbgpAllocBuf",
						  "0x%08x\n", result);
			return NULL;
		}
		buf->extendedBase = base;
		buf->extendedSize = size;
	}
	return (buf->status & 0x10) != 0 ? buf->extendedBase : buf->base;
}

void *_dbgpAllocSendBuf(DbgpProcCB *proc, SceSize size)
{
	return _dbgpAllocBuf(proc, 1, size);
}

void _dbgpSetSendSize(DbgpProcCB *proc, SceSize size)
{
	if (proc == NULL)
		_dbgpSetSize();
	proc->buf[1].size = size;
	_dbgpSetBufStatus(&proc->buf[1], 0, 2);
}

void *_dbgpAllocWorkBuf(DbgpProcCB *proc, SceSize size)
{
	return _dbgpAllocBuf(proc, 2, size);
}

void dbgpFreeWorkBuf(DbgpProcCB *proc)
{
	DbgpBuf *buf;
	int result;

	if (proc == NULL)
		SCE_PANIC_FUNC(3030, "_dbgpFreeBuf");
	buf = &proc->buf[2];
	if ((buf->status & 1) == 0)
		return;
	if ((buf->status & 0x10) != 0) {
		result = SceSysmemForDriver_009E1C61(buf->extendedMemBlock);
		if (result < 0)
			SCE_PRINTF_WITH_INFO_FUNC(15, 3051, "_dbgpFreeBuf",
						  "0x%08x\n", result);
		_dbgpSetBufStatus(buf, 0x10, 0);
	}
	_dbgpSetBufStatus(buf, ~0u, 0);
}

void dbgpFlushCache(void *address)
{
	SceCpuForKernel_74B7F445(address);
	SceCpuForKernel_803C84BF();
}

/* The loader protocol uses the same host error values. */
SceUInt32 dbgpKernelErrToDbgpErr(int error)
{
	switch (error) {
	case 0:
		return 0;
	case 0x80802021:
		return 4;
	case SCE_ERROR_ERRNO_ENOMEM:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_LIB:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_STUB:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_SELF:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM:
		return 0x30008;
	case SCE_ERROR_ERRNO_EAGAIN:
		return 0x30009;
	case SCE_ERROR_ERRNO_ENOENT:
	case SCE_ERROR_ERRNO_EINVAL:
	case SCE_KERNEL_ERROR_MODULEMGR_NOENT:
	case 0x8010390c:
	case 0x80800007:
		return 0x3000a;
	case SCE_KERNEL_ERROR_AUTHFAIL:
		return 0x3000b;
	case SCE_ERROR_ERRNO_ENOEXEC:
	case SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER:
	case SCE_KERNEL_ERROR_MODULEMGR_NOEXEC:
		return 0x3000c;
	case SCE_ERROR_ERRNO_EMFILE:
		return 0x3000e;
	case SCE_ERROR_ERRNO_EISDIR:
		return 0x3000f;
	case SCE_ERROR_ERRNO_ENAMETOOLONG:
	case SCE_KERNEL_ERROR_MODULEMGR_NAMETOOLONG:
		return 0x30010;
	case SCE_KERNEL_ERROR_INVALID_PID:
	case SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS:
	case 0x80802002:
		return 0x30012;
	case SCE_KERNEL_ERROR_MODULEMGR_NO_MOD:
		return 0x30014;
	case SCE_KERNEL_ERROR_MODULEMGR_STARTED:
		return 0x30015;
	case SCE_KERNEL_ERROR_MODULEMGR_STOPPED:
		return 0x30016;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_STOPPED:
		return 0x30017;
	case SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID:
	case SCE_KERNEL_ERROR_MODULEMGR_NO_VAR_NID:
		return 0x30018;
	case 0x800f0b3a:
		return 0x30019;
	case 0x80800020:
		return 0x30022;
	case 0x80800021:
		return 0x30023;
	case 0x80800022:
		return 0x30024;
	case 0x80800023:
		return 0x30025;
	case SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER:
	case 0x8080001a:
		return 0x30028;
	case 0x800f0627:
		return 0x30029;
	case 0x800f0b30:
		return 0x3002a;
	case 0x800f0b32:
		return 0x3002b;
	case 0x800f0537:
	case 0x800f0b37:
		return 0x30030;
	case SCE_KERNEL_ERROR_PRELOAD_LIBC_FAILED:
		return 0x30031;
	case SCE_KERNEL_ERROR_PRELOAD_FIOS2_FAILED:
		return 0x30032;
	case 0x80800005:
		return 0x30035;
	case 0x80800015:
		return 0x30036;
	case 0x80800016:
		return 0x30037;
	case 0x80800017:
		return 0x30038;
	case 0x80800019:
		return 0x30039;
	case 0x80802025:
		return 0x3003b;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_MAIN_MODULE:
		return 0x3003c;
	case SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM:
		return 0x3003d;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED:
		return 0x30080;
	case SCE_KERNEL_ERROR_MODULEMGR_START_FAILED:
		return 0x30081;
	case SCE_KERNEL_ERROR_MODULEMGR_STOP_FAIL:
		return 0x30082;
	case SCE_ERROR_ERRNO_ENOSYS:
		return 0x30083;
	case SCE_ERROR_ERRNO_EEXIST:
		return 0x30084;
	default:
		return 0x3003a;
	}
}

DbgpProcCB *dbgpGetProcCB(SceUID pid)
{
	return dbgpFindProcess(pid);
}

DbgpCB *dbgpGetDbgpCB(void)
{
	return &g_dbgp;
}

int dbgpIsDebugTarget(SceUID pid)
{
	return dbgpGetProcCB(pid) != NULL;
}

int dbgpIsProcessAttached(SceUID pid)
{
	return dbgpGetProcCB(pid) != NULL;
}

int dbgpIsApplicationActive(void)
{
	SceUID pids[16];
	SceSize count = 0;
	int status = 0;
	int i;

	memset(pids, 0, sizeof(pids));
	if (SceProcessmgrForKernel_EFFF2975(3, pids, sizeof(pids), &count) < 0)
		return 0;
	for (i = 0; i < (int)count; ++i) {
		if (pids[i] == 0x10005)
			continue;
		if (sceKernelGetProcessStatus(pids[i], &status) >= 0) {
			status = (SceUInt8)status;
			if ((status & 0x10) != 0)
				return 1;
		}
	}
	return 0;
}

