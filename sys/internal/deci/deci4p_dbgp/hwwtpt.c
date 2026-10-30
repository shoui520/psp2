/* Debugger hardware watchpoints (Sony's hwwtpt.c). */

#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include "dbgp_internal.h"

#define HWWTPT_ID_BASE	0x1001

/* The watchpoint whose range holds the faulting address, else the nearest. */
int hwwtptGetHitId(const DbgpProcCB *proc, DbgpThreadIdentity thread)
{
	SceUInt32 fault, control, length, mask, access, distance, nearest, j;
	int i, id;

	fault = proc->faultAddress;
	mask = 0;
	access = 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active == 0 ||
		    proc->hwwtpt[i].pid != thread.pid)
			continue;
		control = proc->hwwtpt[i].control;
		length = (control & 0x7c0) >> 6;
		for (j = 0; j < length; ++j)
			mask |= 1 << j;
		if (control & 8)
			access = 3;
		if (control & 4)
			access |= 2;
		if (control & 2)
			access |= 1;
		if ((SceUInt32)proc->hwwtpt[i].address == (fault & ~mask & ~access))
			return i;
	}

	id = -1;
	nearest = (SceUInt32)-1;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].pid != thread.pid)
			continue;
		if (fault > (SceUInt32)proc->hwwtpt[i].address)
			distance = fault - (SceUInt32)proc->hwwtpt[i].address;
		else
			distance = (SceUInt32)proc->hwwtpt[i].address - fault;
		if (distance < nearest) {
			nearest = distance;
			id = i;
		}
	}
	if (id == -1)
		return -5;
	if (nearest >= 0x100)
		return -5;
	return id;
}

int hwwtptGetHwwtptCount(const DbgpProcCB *proc, SceUID pid)
{
	int i, n;

	n = 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active != 0 && proc->hwwtpt[i].pid == pid)
			++n;
	}
	return n;
}

int hwwtptGetHwwtpt(const DbgpProcCB *proc, DbgpGetHwwatchptResponse *reply,
		    int id)
{
	SceUID pid;
	int i, n;

	pid = reply->pid;
	if (id != -1) {
		for (i = 0; i != 3; ++i) {
			if (proc->hwwtpt[i].active != 0 &&
			    proc->hwwtpt[i].pid == pid &&
			    proc->hwwtpt[i].id == (SceUInt32)id)
				break;
		}
		if (i == 3)
			return 0;
		reply->info[0].size = sizeof(reply->info[0]);
		reply->info[0].id = proc->hwwtpt[i].id;
		reply->info[0].address = proc->hwwtpt[i].address;
		reply->info[0].control = proc->hwwtpt[i].control;
		return 0;
	}
	n = 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active == 0 || proc->hwwtpt[i].pid != pid)
			continue;
		reply->info[n].size = sizeof(reply->info[n]);
		reply->info[n].id = proc->hwwtpt[i].id;
		reply->info[n].address = proc->hwwtpt[i].address;
		reply->info[n].control = proc->hwwtpt[i].control;
		++n;
	}
	return 0;
}

int hwwtptSetHwwtpt(DbgpProcCB *proc, const DbgpHwwatchptInfo *info,
		    SceUID pid, SceUInt32 *id)
{
	SceUInt32 control, bas;
	int i, result;

	*id = 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active == 0)
			break;
	}
	if (i == 3)
		return 0x20041;

	proc->hwwtpt[i].id = i + HWWTPT_ID_BASE;
	proc->hwwtpt[i].address = info->address;
	proc->hwwtpt[i].control = info->control;
	proc->hwwtpt[i].active = 1;
	proc->hwwtpt[i].pid = pid;

	/* DBGWCR: address mask, load/store, byte address select, enable. */
	control = ((info->control >> 6) & 0x1f) << 24;
	if (info->control & 0x20)
		control |= 0x10;
	if (info->control & 0x10)
		control |= 8;
	bas = info->control & 0xf;
	if (bas != 0)
		control |= bas << 5 | 7;
	else
		control = ~1u;
	result = sceKernelSetPHWP(pid, (i + HWWTPT_ID_BASE) & 0xf,
				  (SceUInt32)info->address, control);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 270, "0x%08x\n", result);
		return result;
	}
	*id = proc->hwwtpt[i].id;
	return 0;
}

int hwwtptClearHwwtpt(DbgpProcCB *proc, SceUID pid, int id, SceUInt32 flags)
{
	int i, result;

	if (id == -1) {
		if (flags != 0) {
			for (i = 0; i != 3; ++i) {
				if (proc->hwwtpt[i].active != 0)
					memset(&proc->hwwtpt[i], 0,
					       sizeof(proc->hwwtpt[i]));
			}
			return 0;
		}
		for (i = 0; i != 3; ++i) {
			if (proc->hwwtpt[i].active == 0)
				continue;
			result = sceKernelSetPHWP(pid, proc->hwwtpt[i].id & 0xf,
					(SceUInt32)proc->hwwtpt[i].address, 0);
			if (result < 0)
				goto error;
			memset(&proc->hwwtpt[i], 0, sizeof(proc->hwwtpt[i]));
		}
		return 0;
	}

	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active != 0 && proc->hwwtpt[i].id == (SceUInt32)id)
			break;
	}
	if (i == 3)
		return 0;
	if (flags == 0) {
		result = sceKernelSetPHWP(pid, proc->hwwtpt[i].id & 0xf,
					  (SceUInt32)proc->hwwtpt[i].address, 0);
		if (result < 0)
			goto error;
	}
	memset(&proc->hwwtpt[i], 0, sizeof(proc->hwwtpt[i]));
	return 0;

error:
	SCE_PRINTF_WITH_INFO(15, 372, "0x%08x\n", result);
	return result;
}

int hwwtptEnableHwwtpt(const SysrootDbgpOps *ops)
{
	DbgpProcCB *proc;
	SceUID pid;
	int i, result;

	(void)ops;
	pid = sceKernelGetProcessId();
	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active == 0)
			continue;
		result = sceKernelEnablePHWP(pid, proc->hwwtpt[i].id & 0xf);
		if (result < 0)
			goto error;
	}
	return 0;

error:
	SCE_PRINTF_WITH_INFO(15, 427, "0x%08x\n", result);
	return result;
}

int hwwtptDisableHwwtpt(const SysrootDbgpOps *ops)
{
	DbgpProcCB *proc;
	SceUID pid;
	int i, result;

	(void)ops;
	pid = sceKernelGetProcessId();
	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	for (i = 0; i != 3; ++i) {
		if (proc->hwwtpt[i].active == 0)
			continue;
		result = sceKernelDisablePHWP(pid, proc->hwwtpt[i].id & 0xf);
		if (result < 0)
			goto error;
	}
	return 0;

error:
	SCE_PRINTF_WITH_INFO(15, 471, "0x%08x\n", result);
	return result;
}
