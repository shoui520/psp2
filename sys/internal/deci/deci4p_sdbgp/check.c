/*
 * The checks of the host's requests: each returns 0 when the request may go
 * on, else the result code the reply carries.
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/cp15.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "sdbgp_internal.h"

int isIllegalBreakpointAttribute(void)
{
	return 0;
}

/* A software break point of the process at the address is already set. */
int isSwbkptAlreadySet(SceUID pid, SceUID tid, uintptr_t address)
{
	SdbgpState *state = sdbgpGetState();
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (state->swbkpt[i].used && state->swbkpt[i].processId == pid &&
		    state->swbkpt[i].address == address)
			return SDBGP_RESULT_ALREADY_SET;
	}
	return 0;
}

int isIllegalSize(SceSize size)
{
	if (size - 1 < 0x10000)
		return 0;
	return SDBGP_RESULT_ILLEGAL_VALUE;
}

/* A user thread's priority must be in the user range. */
int isIllegalThreadPriority(const SdbgpThreadPriorityRequest *request)
{
	ThreadmgrThreadInfoForDebugger info;
	int priority;

	if (!request)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	priority = request->priority;
	info.size = sizeof(info);
	if (SceThreadmgrForKernel_C976719E(request->threadId, 0, &info) < 0)
		return SCE_KERNEL_ERROR_THREADMGR_ERROR;
	if (info.processId == SDBGP_KERNEL_PROCESS_ID)
		return 0;
	if ((unsigned int)(priority - 64) <= 127)
		return 0;
	return SDBGP_RESULT_ILLEGAL_VALUE;
}

int isIllegalProcessControl(const SdbgpProcessControlRequest *request)
{
	uint32_t flags = request->flags;

	if (flags & 0xfff0fff0)
		return SDBGP_RESULT_ILLEGAL_VALUE;
	if (!(flags & 0xf0000))
		return 0;
	if (flags & 0xf)
		return SDBGP_RESULT_ILLEGAL_VALUE;
	return 0;
}

int isIllegalThreadListMode(const SdbgpThreadListRequest *request)
{
	if (request->mode != 1)
		return SDBGP_RESULT_ILLEGAL_VALUE;
	if (request->detail <= 1)
		return 0;
	return SDBGP_RESULT_ILLEGAL_VALUE;
}

int isIllegalMountNum(unsigned int num)
{
	if (num <= 48)
		return 0;
	return SDBGP_RESULT_ILLEGAL_VALUE;
}

int isIllegalKernelApiId(int id)
{
	if ((unsigned int)(id - 1) <= 5)
		return 0;
	return SDBGP_RESULT_ILLEGAL_VALUE;
}

/* Every mount but the last must be mounted. */
int isIllegalMountList(SceVfsMount *const *mounts, int num, VfsMountListEntry *list)
{
	int listNum;
	int result;
	int i, j;

	result = vfsGetMntList(list, 0x11000, &listNum);
	if (result < 0)
		return result;
	for (i = 0; i < num; i++) {
		for (j = 0; j < listNum; j++) {
			if (list[j].mnt == mounts[i])
				break;
		}
		if (j == listNum) {
			if (i + 1 >= num)
				return 0;
			return SDBGP_RESULT_ILLEGAL_VALUE;
		}
	}
	return 0;
}

int isIllegalMountAccess(SceVfsMount *const *mounts, int num, void *info)
{
	int result;
	int i;

	for (i = 0; i < num; i++) {
		result = SceSysrootForKernel_4F57A13F(mounts[i], info);
		if (result < 0)
			return result;
	}
	return 0;
}

int isIllegalProcessId(SceUID pid)
{
	SceKernelProcessInfo info;

	info.size = sizeof(info);
	if (sceKernelGetProcessInfo(pid, &info) < 0)
		return SDBGP_RESULT_ILLEGAL_PROCESS_ID;
	return 0;
}

int isIllegalThreadId(SceUID tid)
{
	ThreadmgrThreadInfoForDebugger info;

	info.size = sizeof(info);
	if (SceThreadmgrForKernel_C976719E(tid, 0, &info))
		return SDBGP_RESULT_ILLEGAL_THREAD_ID;
	return 0;
}

int isIllegalThreadIdOrAll(SceUID tid)
{
	ThreadmgrThreadInfoForDebugger info;

	if (tid == -1)
		return 0;
	info.size = sizeof(info);
	if (SceThreadmgrForKernel_C976719E(tid, 0, &info) < 0)
		return SDBGP_RESULT_ILLEGAL_THREAD_ID;
	return 0;
}

int isIllegalGetRegisterState(void)
{
	return 0;
}

int isIllegalGetRegisterIndex(void)
{
	return 0;
}

int isIllegalSetRegisterState(void)
{
	return 0;
}

int isIllegalSetRegisterIndex(void)
{
	return 0;
}

int isIllegalAddress(SceUID pid, uintptr_t address, SceSize size, uint32_t mode)
{
	if (SceSysmemForDriver_9F6E45E3(pid, mode, (const void *)address, size) < 0)
		return SDBGP_RESULT_ILLEGAL_ADDRESS;
	return 0;
}

/* Only the memory types the hardware reads may be read. */
int isIllegalMemoryType(const void *address)
{
	int type = SceSysmemForKernel_0B5A3E93(address);

	if (type < 0)
		return SCE_KERNEL_ERROR_SYSMEM_ERROR;
	if (type == 0x80 || type == 0xd0)
		return 0;
	return SDBGP_RESULT_ILLEGAL_ADDRESS;
}

int isIllegalAlignment(void)
{
	return 0;
}

int isIllegalAccessSize(SceSize size)
{
	if (size <= 4)
		return 0;
	return SDBGP_RESULT_ILLEGAL_ACCESS_SIZE;
}

/* The watch point belongs to another thread. */
int isHwwatchNotOwner(SceUID owner)
{
	SdbgpState *state = sdbgpGetState();

	if (owner == -1)
		return 0;
	if (!state->hwwatch[0].used)
		return SDBGP_RESULT_HWWATCH_NOT_OWNER;
	if (state->hwwatch[0].owner == owner)
		return 0;
	return SDBGP_RESULT_HWWATCH_NOT_OWNER;
}

int isHwwatchInUse(void)
{
	if (sdbgpGetState()->hwwatch[0].used)
		return SDBGP_RESULT_HWWATCH_IN_USE;
	return 0;
}

/* No break point slot is free, or the first used one is another's. */
int isHwbkptInUse(SceUID pid, SceUID tid)
{
	SdbgpState *state = sdbgpGetState();
	int free = 0;
	int i;

	for (i = 0; i < SDBGP_HWBKPT_MAX; i++) {
		if (!state->hwbkpt[i].used)
			free = 1;
	}
	if (!free)
		return SDBGP_RESULT_HWBKPT_IN_USE;
	for (i = 0; i < SDBGP_HWBKPT_MAX; i++) {
		if (state->hwbkpt[i].used)
			break;
	}
	if (i == SDBGP_HWBKPT_MAX)
		return 0;
	if (pid != -1 && state->hwbkpt[i].processId == pid)
		return SDBGP_RESULT_HWBKPT_IN_USE;
	if (tid == -1)
		return 0;
	if (state->hwbkpt[i].threadId == -1)
		return 0;
	return SDBGP_RESULT_HWBKPT_IN_USE;
}

int isSwbkptNotFound(uint32_t id)
{
	SdbgpState *state = sdbgpGetState();
	int i;

	if (id == (uint32_t)-1)
		return 0;
	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (state->swbkpt[i].used && state->swbkpt[i].id == id)
			return 0;
	}
	return SDBGP_RESULT_SWBKPT_NOT_FOUND;
}

int isProcessNotSuspended(SceUID pid)
{
	SceUInt32 status;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0)
		return result;
	if (status & SDBGP_PROCESS_STATUS_SUSPENDED)
		return 0;
	return SDBGP_RESULT_PROCESS_NOT_SUSPENDED;
}

int isAlreadyAttached(SdbgpCpuState *cpu, SceUID pid)
{
	if (cpu->flags == (uint32_t)pid)
		return SDBGP_RESULT_ALREADY_ATTACHED;
	return 0;
}

int isIllegalAttachState(void)
{
	return 0;
}

int isIllegalProcessStatus(SceUID pid)
{
	SceUInt32 status;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0)
		return result;
	if (status & SDBGP_PROCESS_STATUS_SUSPENDED)
		return SDBGP_RESULT_PROCESS_SUSPENDED;
	return 0;
}

int isThreadNotSuspended(SceUID tid)
{
	int status = SceThreadmgrForDriver_A0B1AB21(tid);

	if (status & 3)
		return 0;
	if (status)
		return 0;
	return SDBGP_RESULT_THREAD_NOT_SUSPENDED;
}

int isThreadIsSuspendedAlready(SceUID tid)
{
	int status = SceThreadmgrForDriver_A0B1AB21(tid);

	if (status < 0)
		return status;
	if (status & 1)
		return SDBGP_RESULT_THREAD_SUSPENDED;
	return 0;
}

int isAddressOverflow(uintptr_t address, SceSize size)
{
	if (address + size < address)
		return SDBGP_RESULT_ADDRESS_OVERFLOW;
	return 0;
}

int isIllegalFd(SceUID pid, SceUID fd, SceIofileInfo *info)
{
	SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(*info));
	if (SceIofilemgrForDriver_A1EF2648(fd, pid, info) < 0)
		return SDBGP_RESULT_ILLEGAL_FD;
	return 0;
}

int isIllegalSysFd(SceUID fd, SceIofileInfo *info)
{
	SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(*info));
	if (SceIofilemgrForDriver_A1EF2648(fd, SDBGP_KERNEL_PROCESS_ID, info) < 0)
		return SDBGP_RESULT_ILLEGAL_FD;
	return 0;
}

/* Why a prefetch abort stopped the core: a break point, a BKPT, or no debug event. */
int checkSdbgpTargetAddrStopCausePabt(uint32_t ifar, SceUID pid, SceUID tid,
				      uintptr_t pc, uint32_t cpsr, int addressSpace)
{
	SdbgpCpuState *cpu = sdbgpGetCpuState();
	uint32_t ifsr = readIfsr();
	uint32_t opcode = 0;
	uint8_t code[4];
	int thumb;
	int result;

	if (FSR_STATUS(ifsr) != FSR_DEBUG_EVENT)
		return SDBGP_STOP_PABT;
	if (sceKernelCheckDipsw(SDBGP_DIPSW_HW_DEBUG)) {
		uint32_t moe = readDbgdscrExt() & DBGDSCR_MOE_MASK;

		if (moe == DBGDSCR_MOE_BREAKPOINT) {
			if (sdbgpGetStatus(cpu, 0x400) || sdbgpGetStatus(cpu, 0x100))
				return SDBGP_STOP_STEPPING;
			result = hwbkptCheckStop(pid, tid, pc, cpsr);
			if (result > 0)
				return result;
		} else if ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR) {
			if (moe == DBGDSCR_MOE_BKPT)
				return SDBGP_STOP_BKPT_C0;
			return SDBGP_STOP_USER_DEBUG_EVENT;
		}
	}
	if ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR)
		return SDBGP_STOP_USER_DEBUG_EVENT;
	thumb = cpsr & PSR_THUMB;
	if (addressSpace == 1 || addressSpace == 16) {
		if (SceSysmemForDriver_9F6E45E3(pid, 1, (const void *)pc, 4) < 0)
			return SDBGP_STOP_PABT;
		if (!thumb) {
			SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)pc, 4);
			goto arm;
		}
		SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)pc, 2);
	} else {
		if (!thumb) {
			if (sceKernelCopyFromUserProc(pid, code, (const void *)pc, 4) < 0)
				return SCE_KERNEL_ERROR_SYSMEM_ERROR;
			goto arm;
		}
		if (sceKernelCopyFromUserProc(pid, code, (const void *)pc, 2) < 0)
			return SCE_KERNEL_ERROR_SYSMEM_ERROR;
	}
	result = sdbgpDecodeOpcode(1, code, &opcode);
	if (result < 0)
		return result;
	result = swbkptTypeOpcode(opcode, 1, cpsr);
	if (result > 0)
		return result;
	return SDBGP_STOP_PABT;
arm:
	result = sdbgpDecodeOpcode(thumb, code, &opcode);
	if (result < 0)
		return result;
	result = swbkptTypeOpcode(opcode, thumb, cpsr);
	if (result > 0)
		return result;
	return SDBGP_STOP_PABT;
}

/* Why a data abort stopped the core: a watch point, or no debug event. */
int checkSdbgpTargetAddrStopCauseDabt(uint32_t dfar, SceUID pid, SceUID tid,
				      uintptr_t pc, uint32_t cpsr, int mode)
{
	uint32_t dfsr = readDfsr();
	int result;

	if (FSR_STATUS(dfsr) != FSR_DEBUG_EVENT ||
	    !sceKernelCheckDipsw(SDBGP_DIPSW_HW_DEBUG) ||
	    (readDbgdscrExt() & 0x1c) != DBGDSCR_MOE_WATCHPOINT)
		return SDBGP_STOP_DABT;
	result = hwwatchCheckStop(tid, dfar, mode);
	if (result <= 0)
		return SDBGP_STOP_DABT;
	return result;
}

int getCpuStopReason(SdbgpCpuState *cpu, uint32_t *reason)
{
	int index = sceKernelCpuId();
	SdbgpState *state = sdbgpGetState();

	if (!state)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	*reason = state->stopReason[index];
	return 0;
}
