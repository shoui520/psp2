/* Debugger command validation (Sony's check.c). */

#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <sysmem/debug.h>
#include <sysmem/cp15.h>
#include <threadmgr/threadmgr_debugger.h>
#include "dbgp_internal.h"

int dbgpCheckSwbrkptSize(const SceSize *size)
{
	return *size != 32;
}

int dbgpCheckDuplicateSwbrkpt(DbgpProcCB *proc, SceUID pid,
			     SceUID userThid, const void *address)
{
	SwbkptInfo *bkpt;
	int count;

	for (bkpt = proc->swbkpt, count = 256; count != 0; ++bkpt, --count) {
		if (bkpt->active != 0 && bkpt->pid == pid &&
		    bkpt->address == address && bkpt->userThid == userThid)
			return 3;
	}
	return 0;
}

int checkInvalidRegisterId(SceUInt32 id)
{
	return id > 44 ? 7 : 0;
}

int checkInvalidMemorySize(SceSize size)
{
	return size - 1 < 0x10000 ? 0 : 7;
}

int checkInvalidWatchpointControl(SceUInt32 control)
{
	if (((control >> 6) & 0x1f) == 0)
		return 0;
	return (control & 0xf) == 0xf ? 0 : 7;
}

int checkInvalidNetInterface(SceUInt32 ifnum)
{
	return ifnum - 1 <= 5 ? 0 : 7;
}

int checkInvalidNetEmulationQuery(SceNetPsDebugEmulationSelector query)
{
	if ((SceUInt32)query.interfaceType - 2 > 1)
		return 7;
	return query.optionNumber <= 4 ? 0 : 7;
}

int checkInvalidNetEmulationRequest(SceNetPsDebugEmulationRequest request)
{
	if ((SceUInt32)request.interfaceType - 2 > 1 ||
	    request.optionNumber > 4)
		return 7;
	return request.parameterSize >= sizeof(request.parameters) ? 0 : 7;
}

int checkInvalidProcess(SceUID pid)
{
	if (pid == 0x10005 || sceKernelUIDtoProcess(pid) == NULL)
		return 0x20011;
	return 0;
}

int checkUnknownProcess(SceUID pid)
{
	return sceKernelUIDtoProcess(pid) != NULL ? 0 : 0x20011;
}

int checkInvalidThread(SceUID pid, SceUID thid)
{
	ThreadmgrThreadInfoForDebugger info;
	SceUID guid;

	info.size = sizeof(info);
	guid = SceSysmemForDriver_45D22597(pid, thid);
	if (SceThreadmgrForDriver_555CA6BB(guid, 0, &info) != 0)
		return 0x20012;
	if (info.processId == 0x10005)
		return 0x20012;
	return 0;
}

int checkInvalidModule(SceUID pid, SceUID modid)
{
	SceKernelModuleInfo info;
	SceUID guid;

	info.size = sizeof(info);
	guid = SceSysmemForDriver_45D22597(pid, modid);
	if (guid < 0 || ksceKernelGetModuleInfo(pid, guid, &info) < 0)
		return 0x20013;
	return 0;
}

int checkInvalidOptionalThread(SceUID pid, SceUID thid)
{
	ThreadmgrThreadInfoForDebugger info;
	SceUID guid;

	if (thid == -1)
		return 0;
	guid = SceSysmemForDriver_45D22597(pid, thid);
	info.size = sizeof(info);
	if (SceThreadmgrForDriver_555CA6BB(guid, 0, &info) < 0)
		return 0x20012;
	if (info.processId == 0x10005)
		return 0x20012;
	return 0;
}

static inline __attribute__((always_inline)) int
checkInvalidRegisterIds(const DbgpRegisterRequest *request, SceUInt32 count)
{
	SceUInt32 i, id, group;

	for (i = 0; i != count; ++i) {
		id = request->registers[i];
		group = id >> 16 << 16;
		if (group == 0x30000) {
			if (id - 0x30000 > 3)
				return 0x20020;
		} else if (group > 0x30000) {
			if (group == 0x40000) {
				if (id - 0x40000 > 4)
					return 0x20020;
			} else if (group != 0x60000 || id - 0x60000 > 15) {
				return 0x20020;
			}
		} else if (group == 0x10000) {
			if (id - 0x10000 > 15)
				return 0x20020;
		} else if (group != 0x20000 || id - 0x20000 > 2) {
			return 0x20020;
		}
	}
	return 0;
}

int checkInvalidGetRegisterIds(const DbgpRegisterRequest *request,
			       SceUInt32 count)
{
	return checkInvalidRegisterIds(request, count);
}

int checkInvalidSetRegisterIds(const DbgpRegisterRequest *request,
			       SceUInt32 count)
{
	return checkInvalidRegisterIds(request, count);
}

int checkReadOnlyRegisters(const DbgpRegisterRequest *request)
{
	SceUInt32 i, id, count = request->count;

	for (i = 0; i != count; ++i) {
		id = request->registers[i];
		if (id <= 0x30003) {
			if (id >= 0x30000 || id == 0x20002)
				return 0x20022;
		} else if (id - 0x40000 <= 4) {
			return 0x20022;
		}
	}
	return 0;
}

int checkInvalidMemoryRange(SceUID pid, const void *address, SceSize size,
			    SceUInt32 mode)
{
	SceUInt32 start = (uintptr_t)address;
	int protected;

	if (SceSysmemForDriver_9F6E45E3(pid, mode, address, size) < 0)
		return 0x20030;
	protected = start <= 0x80000fff;
	if (start < 0x80000000)
		protected = 0;
	if (protected)
		return 0x20030;
	return start + size + 0x7fffffff >= 0x1000 ? 0 : 0x20030;
}

int checkInvalidAddressAlignment(const void *address, SceUInt32 type)
{
	if (type == 1)
		return ((uintptr_t)address & 3) != 0 ? 0x20031 : 0;
	return ((uintptr_t)address & 1) != 0 ? 0x20031 : 0;
}

int checkInvalidWatchpointId(const DbgpProcCB *proc, int id)
{
	if (id == -1 ||
	    (proc->hwwtpt[0].active != 0 && proc->hwwtpt[0].id == id) ||
	    (proc->hwwtpt[1].active != 0 && proc->hwwtpt[1].id == id) ||
	    (proc->hwwtpt[2].active != 0 && proc->hwwtpt[2].id == id))
		return 0;
	return 0x20040;
}

int checkNoFreeWatchpoint(const DbgpProcCB *proc)
{
	if (proc->hwwtpt[0].active != 0 && proc->hwwtpt[1].active != 0 &&
	    proc->hwwtpt[2].active != 0)
		return 0x20041;
	return 0;
}

int checkNotExecutable(SceUID pid, const void *address,
		       SceKernelModuleInfoForDebugger *modules)
{
	DbgpProcessDebugInfo *debugInfo;
	SceKernelSegmentInfoForDebugger *segment;
	SceUInt32 target = (uintptr_t)address;
	SceUInt32 base, size, j;
	int count = 0, i, result;

	debugInfo = dbgpGetDbgpCB()->processDebugInfo;
	if (debugInfo != NULL) {
		for (i = 0; i != 128; ++i) {
			if (debugInfo[i].pid != pid)
				continue;
			base = (uintptr_t)debugInfo[i].codeBase;
			if (base <= target &&
			    target < base + debugInfo[i].codeSize)
				return 0;
		}
	}
	result = sceKernelGetModuleInfoForDebugger(pid, NULL, NULL);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 619, "0x%08x\n", result);
		return result;
	}
	size = sizeof(*modules) * result;
	if (size <= 0x11000) {
		count = result;
	} else {
		size = 69600;
		count = 870;
	}
	memset(modules, 0, size);
	result = sceKernelGetModuleInfoForDebugger(pid, modules, &count);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 632, "0x%08x\n", result);
		return result;
	}
	for (i = 0; i < count; ++i) {
		for (j = 0; j != modules->nSegments; ++j) {
			segment = &modules->segment[j];
			if ((segment->perms & 1) == 0)
				continue;
			base = (uintptr_t)segment->vaddr;
			if (target >= (base >> 12 << 12) &&
			    target < ((base + segment->memsz + 0xfff) >> 12 << 12))
				return 0;
		}
		modules = (SceKernelModuleInfoForDebugger *)
			((char *)modules + modules->size);
	}
	return 0x20048;
}

int checkInvalidSwbrkptId(const DbgpProcCB *proc, int id)
{
	int i;

	if (id == -1)
		return 0;
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].active != 0 && proc->swbkpt[i].id == id)
			return 0;
	}
	return 0x20046;
}

int checkNoFreeSwbrkpt(const DbgpProcCB *proc)
{
	int i;

	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].active == 0)
			return 0;
	}
	return 0x20047;
}

int checkIllegalProcessStatus(SceUID pid)
{
	int status, fullStatus, result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 691, "0x%08x\n", result);
		return result;
	}
	fullStatus = status;
	status = (SceUInt8)status;
	if ((status & 0x10) == 0) {
		SCE_PRINTF_WITH_INFO(15, 703, "0x%08x:0x%08x\n", pid, fullStatus);
		return 0x20050;
	}
	return 0;
}

int checkIllegalProcessStatusResumeProcess(SceUID pid)
{
	int status, fullStatus, result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 715, "0x%08x\n", result);
		return result;
	}
	fullStatus = status;
	status = (SceUInt8)status;
	if (status == 29 || (SceUInt32)status - 24 <= 1)
		return 0x20050;
	if ((fullStatus & 0x10) == 0) {
		SCE_PRINTF_WITH_INFO(15, 736, "0x%08x:0x%08x\n", pid, fullStatus);
		return 0x20050;
	}
	return 0;
}

int checkAttachedProcess(const DbgpProcCB *proc, SceUID pid)
{
	return proc->pid == pid ? 0 : 0x20051;
}

int checkAlreadyAttachedProcess(SceUID pid)
{
	return dbgpGetProcCB(pid) != NULL ? 0x20052 : 0;
}

int checkProcessIsSuspendedAlready(SceUID pid)
{
	int status, result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 777, "0x%08x\n", result);
		return result;
	}
	status = (SceUInt8)status;
	if ((status & 0x10) != 0) {
		SCE_PRINTF_WITH_INFO(15, 784, "0x%08x:0x%08x\n", pid, status);
		return 0x20056;
	}
	return 0;
}

int checkIllegalThreadStatusResumeThread(SceUID pid, SceUID thid)
{
	SceUID guid;
	int status;

	guid = SceSysmemForDriver_45D22597(pid, thid);
	status = SceThreadmgrForDriver_A0B1AB21(guid);
	if ((status & 1) != 0)
		return 0;
	SCE_PRINTF_WITH_INFO(15, 804, "0x%08x:0x%08x\n", thid, status);
	return 0x20060;
}

int checkIllegalThreadStatusResumeProcess(SceUID pid, SceUID thid,
					 int *isBreakpoint)
{
	SceThreadCpuRegisters registers;
	SceUInt8 bytes[4];
	SceUInt32 opcode;
	SceUID guid;
	int status, result;

	guid = SceSysmemForDriver_45D22597(pid, thid);
	status = SceThreadmgrForDriver_A0B1AB21(guid);
	if (status < 0) {
		SCE_PRINTF_WITH_INFO(15, 823, "0x%08x\n", status);
		return status;
	}
	if (status == 0 || (status & 1) != 0)
		return 0x20060;
	result = SceThreadmgrForDriver_5022689D(guid, &registers);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 845, "0x%08x\n", result);
		return result;
	}
	if ((registers.entry[0].cpsr & 0x1f) != 0x10)
		return 0;
	if ((registers.entry[0].cpsr & 0x20) != 0) {
		result = sceKernelCopyFromUserProc(pid, bytes,
				(const void *)registers.entry[0].pc, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 865, "0x%08x\n", result);
			return result;
		}
		opcode = bytes[0] + ((SceUInt32)bytes[1] << 8);
		if ((opcode & 0xff00) == 0xbe00)
			*isBreakpoint = 1;
	} else {
		result = sceKernelCopyFromUserProc(pid, bytes,
				(const void *)registers.entry[0].pc, 4);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 885, "0x%08x\n", result);
			return result;
		}
		opcode = bytes[0] + ((SceUInt32)bytes[1] << 8) +
			 ((SceUInt32)bytes[2] << 16) + ((SceUInt32)bytes[3] << 24);
		if ((opcode & 0xfff000f0) == 0xe1200070)
			*isBreakpoint = 1;
	}
	return 0;
}

int checkThreadIsSuspendedAlready(SceUID pid, SceUID thid)
{
	SceUID guid;
	int status;

	guid = SceSysmemForDriver_45D22597(pid, thid);
	status = SceThreadmgrForDriver_A0B1AB21(guid);
	if (status < 0) {
		SCE_PRINTF_WITH_INFO(15, 913, "0x%08x\n", status);
		return status;
	}
	if ((status & 1) != 0) {
		SCE_PRINTF_WITH_INFO(15, 918, "0x%08x:0x%08x\n", thid, status);
		return 0x20061;
	}
	return 0;
}

int checkInvalidAddressOverflow(const void *address, SceSize size)
{
	SceUInt32 start = (uintptr_t)address;

	return start + size < start ? 0x20070 : 0;
}

int checkIllegalApplicationStatus(SceUID pid)
{
	int status = 0;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 965, "0x%08x\n", result);
		return result;
	}
	return status == 3 ? 0 : 0x20081;
}

int checkApplicationIsSuspendedAlready(SceUID pid)
{
	int status = 0;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 984, "0x%08x\n", result);
		return result;
	}
	return status == 3 ? 0x20080 : 0;
}

int checkApplicationIsSuspended(SceUID pid)
{
	int status = 0;
	int result;

	result = sceKernelGetProcessStatus(pid, &status);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1002, "0x%08x\n", result);
		return result;
	}
	return status == 3 ? 0x20082 : 0;
}

int checkInvalidAllocPageSize(SceSize size)
{
	return (size & 0xfffff) != 0 ? 0x20055 : 0;
}

int isProcessNotification(SceUInt8 group, SceUInt8 type)
{
	int notification;

	if (group != 0x10)
		return 0;
	notification = type & 0xf9;
	if (notification == 0xc0)
		return 1;
	return notification == 0xc8;
}

int checkGetTargetAddrOpcode(const void *address, SceUInt32 cpsr, SceUID pid,
			     SceUInt32 *armOpcode, SceUInt16 *thumbOpcode,
			     int *isThumb)
{
	SceUInt32 ifsr, status;
	int result;

	ifsr = readIfsr();
	status = (ifsr & 15) | ((ifsr >> 10) & 1) << 4;
	if (status != 2)
		return (int)0x80080001;
	if ((cpsr & 0x20) != 0) {
		*isThumb = 1;
		result = sceKernelCopyFromUserProc(pid, thumbOpcode, address, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1061, "0x%08x\n", result);
			return result;
		}
	} else {
		*isThumb = 0;
		result = sceKernelCopyFromUserProc(pid, armOpcode, address, 4);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1070, "0x%08x\n", result);
			return result;
		}
	}
	return 0;
}

int checkGetTargetAddrStopCause(DbgpProcCB *proc, const void *address,
				SceUInt32 cpsr, SceUID pid, SceUInt32 *flags)
{
	SceUInt8 bytes[4];
	SceUInt32 ifsr, dbgdscr, opcode, immediate;
	int result;

	ifsr = readIfsr();
	if (((ifsr & 15) | ((ifsr >> 10) & 1) << 4) != 2)
		return 1;
	dbgdscr = CP_READ(p14, 0, c0, c2, 2) & 0x3c;
	if (dbgdscr == 4) {
		*flags |= 1;
		return (cpsr & 0x20) != 0 ? 65 : 64;
	}
	if (dbgdscr != 12)
		return 1;
	if ((cpsr & 0x20) != 0) {
		result = sceKernelCopyFromUserProc(pid, bytes, address, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1138, "0x%08x\n", result);
			return result;
		}
		opcode = bytes[0] + ((SceUInt32)bytes[1] << 8);
		if ((opcode & 0xff00) == 0xbe00 && bytes[0] > 0x7f) {
			if (bytes[0] <= 0xbf)
				return opcode == 0xbe91 ? 41 : 39;
			if (bytes[0] <= 0xdf && swbkptHasAddress(proc, pid, address)) {
				if (opcode == 0xbec1)
					return 35;
				if (opcode == 0xbec2) {
					*flags |= 1;
					return 37;
				}
				return 47;
			}
		}
		return 33;
	}
	result = sceKernelCopyFromUserProc(pid, bytes, address, 4);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1213, "0x%08x\n", result);
		return result;
	}
	opcode = bytes[0] + ((SceUInt32)bytes[1] << 8) +
		 ((SceUInt32)bytes[2] << 16) + ((SceUInt32)bytes[3] << 24);
	if ((opcode & 0xfff000f0) == 0xe1200070) {
		immediate = (opcode & 15) + ((opcode & 0xfff00) >> 4);
		if (immediate > 0x7f) {
			if (immediate <= 0xbf)
				return opcode == 0xe1200971 ? 40 : 38;
			if (immediate <= 0xdf) {
				if (swbkptHasAddress(proc, pid, address)) {
					if (opcode == 0xe1200c71)
						return 34;
					if (opcode == 0xe1200c72) {
						*flags |= 1;
						return 36;
					}
					return 47;
				}
			} else if (immediate > 0xff) {
				return 47;
			}
		}
	}
	return 32;
}

int checkGetTargetAddrStopCauseDabt(void)
{
	SceUInt32 dfsr;

	dfsr = readDfsr();
	if (((dfsr & 15) | ((dfsr >> 10) & 1) << 4) != 2)
		return 2;
	if ((CP_READ(p14, 0, c0, c2, 2) & 0x1c) == 8)
		return 80;
	return 2;
}

int isDbgpThreadPcBkpt(SceUID pid, const void *address, SceUInt32 cpsr)
{
	SceUInt8 bytes[4];
	SceUInt32 opcode;
	int result;

	if ((cpsr & 0x20) != 0) {
		result = sceKernelCopyFromUserProc(pid, bytes, address, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 1338, "0x%08x\n", result);
			return result;
		}
		opcode = bytes[0] + ((SceUInt32)bytes[1] << 8);
		return (opcode & 0xff00) == 0xbe00;
	}
	result = sceKernelCopyFromUserProc(pid, bytes, address, 4);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 1358, "0x%08x\n", result);
		return result;
	}
	opcode = bytes[0] + ((SceUInt32)bytes[1] << 8) +
		 ((SceUInt32)bytes[2] << 16) + ((SceUInt32)bytes[3] << 24);
	return (opcode & 0xfff000f0) == 0xe1200070;
}
