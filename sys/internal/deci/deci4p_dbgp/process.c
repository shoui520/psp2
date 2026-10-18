/* Debugger process control (Sony's process.c). */

#include <sysmem/debug.h>
#include <processmgr/process.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include "dbgp_internal.h"

int procSuspendProcess(DbgpProcCB *proc, SceUID pid, SceUID thid,
		       SceUInt32 reason)
{
	int status, newStatus, threadStatus, result;

	(void)proc;
	if (sceKernelGetProcessStatus(pid, &status) < 0)
		status = 0;
	else
		status = (SceUInt8)status;
	switch (reason & 0xf0) {
	case 0:
		switch (reason) {
		case 1:
			result = SceThreadmgrForDriver_04C6764B(thid, 0x800);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 63, "0x%08x\n", result);
				return result;
			}
			newStatus = 19;
			break;
		case 2:
			newStatus = 20;
			break;
		case 3:
			newStatus = 21;
			break;
		default:
			return reason;
		}
		if (status == PROCESS_STATE_RUNNING)
			goto debug_suspend;
		result = sceKernelSetProcessStatus(pid, newStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 87, "0x%08x\n", result);
			return result;
		}
		goto update_status;
	case 0x20:
		if (status == PROCESS_STATE_RUNNING) {
			if (reason - 0x28 <= 1) {
				threadStatus = 0x4000;
				newStatus = 28;
			} else {
				threadStatus = 0x1000;
				newStatus = 18;
			}
		} else {
			newStatus = 0;
			threadStatus = reason - 0x28 <= 1 ? 0x4002 : 0x1002;
		}
		result = SceThreadmgrForDriver_04C6764B(thid, threadStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 129, "0x%08x:0x%08x\n",
					     result, threadStatus);
			return result;
		}
		if (status == PROCESS_STATE_RUNNING)
			goto debug_suspend;
		if (newStatus == 0)
			return 0;
		goto set_status;
	case 0x30:
		switch (reason) {
		case 0x30: newStatus = 23; break;
		case 0x31: newStatus = 24; break;
		case 0x32: newStatus = 25; break;
		case 0x33: newStatus = 26; break;
		case 0x34: newStatus = 27; break;
		case 0x35: newStatus = 29; break;
		case 0x36: newStatus = 31; break;
		default: return reason;
		}
		goto update_status;
	case 0x40:
		result = SceThreadmgrForDriver_04C6764B(thid, 0x1002);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 145, "0x%08x\n", result);
			return result;
		}
		if (status != PROCESS_STATE_RUNNING)
			return 0;
		newStatus = 0;
		goto debug_suspend;
	case 0x50:
		threadStatus = status == PROCESS_STATE_RUNNING ? 0x2000 : 0x2002;
		result = SceThreadmgrForDriver_04C6764B(thid, threadStatus);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 168, "0x%08x:0x%08x\n",
					     result, threadStatus);
			return result;
		}
		newStatus = 22;
		goto update_status;
	default:
		return (int)0x80080001;
	}
update_status:
	if (status == PROCESS_STATE_RUNNING)
		goto debug_suspend;
set_status:
	result = sceKernelSetProcessStatus(pid, newStatus);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 229, "0x%08x:0x%08x\n", result, newStatus);
		return result;
	}
	return 0;
debug_suspend:
	result = sceKernelProcessDebugSuspend(pid, newStatus);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 222, "0x%08x:0x%08x\n", result, newStatus);
		return result;
	}
	return 0;
}
