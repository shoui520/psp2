/* Debugger software breakpoints (Sony's swbkpt.c). */

#include <modulemgr/modulemgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include "dbgp_internal.h"

#define SWBKPT_THUMB		0xbec1		/* bkpt 0xc1 */
#define SWBKPT_ARM		0xe1200c71	/* bkpt 0xc1 */
#define SWBKPT_ID_BASE		0x20000

#define INLINE static inline __attribute__((always_inline))

static int s_swbkptNext;

int swbkptHasAddress(const DbgpProcCB *proc, SceUID pid, const void *address)
{
	int i;

	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].address == address &&
		    proc->swbkpt[i].pid == pid)
			return 1;
	}
	return 0;
}

int swbkptGetAlignId(const DbgpProcCB *proc, const void *address,
		     DbgpThreadIdentity thread, SceUInt32 *stopCause)
{
	int i;

	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].active != 0 &&
		    proc->swbkpt[i].address == address &&
		    thread.pid == proc->swbkpt[i].pid)
			break;
	}
	if (i == 256)
		return -5;
	if (proc->swbkpt[i].thid != thread.thid && proc->swbkpt[i].thid != -1 &&
	    stopCause != NULL)
		*stopCause = 2;
	return i;
}

int swbkptGetSwbrkptCount(const DbgpProcCB *proc, SceUID pid)
{
	int i, n;

	n = 0;
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].active != 0 && proc->swbkpt[i].pid == pid)
			++n;
	}
	return n;
}

int swbkptGetSwbrkpt(const DbgpProcCB *proc, DbgpGetSwbrkptResponse *reply,
		     int id)
{
	SceUID pid;
	int i, n;

	pid = reply->pid;
	if (id != -1) {
		for (i = 0; i != 256; ++i) {
			if (proc->swbkpt[i].active != 0 &&
			    proc->swbkpt[i].pid == pid &&
			    proc->swbkpt[i].id == (SceUInt32)id)
				break;
		}
		if (i == 256)
			return 0;
		reply->info[0].size = sizeof(reply->info[0]);
		reply->info[0].id = proc->swbkpt[i].id;
		reply->info[0].thid = proc->swbkpt[i].userThid;
		reply->info[0].address = proc->swbkpt[i].address;
		reply->info[0].attr = proc->swbkpt[i].attr;
		reply->info[0].data = proc->swbkpt[i].requestData;
		reply->info[0].extSize = 8;
		reply->info[0].ext = 0;
		return 0;
	}
	n = 0;
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].active == 0 || proc->swbkpt[i].pid != pid)
			continue;
		reply->info[n].size = sizeof(reply->info[n]);
		reply->info[n].id = proc->swbkpt[i].id;
		reply->info[n].thid = proc->swbkpt[i].userThid;
		reply->info[n].address = proc->swbkpt[i].address;
		reply->info[n].attr = proc->swbkpt[i].attr;
		reply->info[n].data = proc->swbkpt[i].requestData;
		reply->info[n].extSize = 8;
		reply->info[n].ext = 0;
		++n;
	}
	return 0;
}

/* A breakpoint already at this address holds the instruction it replaced. */
INLINE void swbkptCopyOriginal(DbgpProcCB *proc)
{
	SwbkptInfo *bkpt, *other;
	int i;

	bkpt = &proc->swbkpt[s_swbkptNext];
	for (i = 0; i != 256; ++i) {
		other = &proc->swbkpt[i];
		if (other->active != 0 && other->pid == bkpt->pid &&
		    other->address == bkpt->address && i != s_swbkptNext)
			break;
	}
	if (i == 256)
		return;
	bkpt->original.bytes[0] = other->original.bytes[0];
	bkpt->original.bytes[1] = other->original.bytes[1];
	if ((other->attr & 1) == 0) {
		bkpt->original.bytes[2] = other->original.bytes[2];
		bkpt->original.bytes[3] = other->original.bytes[3];
	}
}

int swbkptSetSwbrkpt(DbgpProcCB *proc, const DbgpSetSwbrkptRequest *request,
		     DbgpSetSwbrkptResponse *reply)
{
	SceUInt16 thumbBkpt = SWBKPT_THUMB;
	SceUInt32 armBkpt = SWBKPT_ARM;
	DbgpSwbrkptInfo info = request->info;
	SceUID pid = request->header.pid;
	union {
		SceUInt32 opcode;
		SceUInt8 bytes[4];
	} current;
	SwbkptInfo *bkpt;
	SceUID modid;
	int count, result;

	for (count = 256; proc->swbkpt[s_swbkptNext].active != 0;) {
		if (++s_swbkptNext == 256)
			s_swbkptNext = 0;
		if (--count == 0)
			return 0x20047;
	}

	modid = sceKernelGetModuleIdByAddrForDebugger(pid,
						      (SceUInt32)info.address);
	reply->pid = pid;
	reply->id = s_swbkptNext + SWBKPT_ID_BASE;
	bkpt = &proc->swbkpt[s_swbkptNext];
	bkpt->id = s_swbkptNext + SWBKPT_ID_BASE;
	bkpt->modid = modid < 0 ? 0 : modid;
	bkpt->pid = pid;
	bkpt->thid = info.thid;
	bkpt->userThid = info.thid;
	if (info.thid != -1)
		bkpt->thid = scePUIDtoGUID(pid, info.thid);
	bkpt = &proc->swbkpt[s_swbkptNext];
	bkpt->address = info.address;
	bkpt->attr = info.attr;
	bkpt->requestData = info.data;
	bkpt->active = 1;

	if (request->info.attr & 1) {
		result = sceKernelCopyFromUserProc(pid, &current, info.address, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 311, "0x%08x\n", result);
			return result;
		}
		if (current.bytes[0] + (current.bytes[1] << 8) == SWBKPT_THUMB) {
			swbkptCopyOriginal(proc);
		} else {
			bkpt = &proc->swbkpt[s_swbkptNext];
			bkpt->original.bytes[0] = current.bytes[0];
			bkpt->original.bytes[1] = current.bytes[1];
		}
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid, info.address,
								  &thumbBkpt, 2);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 338, "0x%08x\n", result);
			return result;
		}
		bkpt = &proc->swbkpt[s_swbkptNext];
		reply->opcodeBits = 16;
		reply->opcode = bkpt->original.bytes[0] +
				(bkpt->original.bytes[1] << 8);
	} else {
		result = sceKernelCopyFromUserProc(pid, &current, info.address, 4);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 265, "0x%08x\n", result);
			return result;
		}
		if (current.bytes[0] + (current.bytes[1] << 8) +
		    (current.bytes[2] << 16) + (current.bytes[3] << 24) ==
		    SWBKPT_ARM)
			swbkptCopyOriginal(proc);
		else
			proc->swbkpt[s_swbkptNext].original.opcode = current.opcode;
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid, info.address,
								  &armBkpt, 4);
		if (result < 0) {
			SCE_PRINTF_WITH_INFO(15, 292, "0x%08x\n", result);
			return result;
		}
		bkpt = &proc->swbkpt[s_swbkptNext];
		reply->opcodeBits = 32;
		reply->opcode = bkpt->original.bytes[0] +
				(bkpt->original.bytes[1] << 8) +
				(bkpt->original.bytes[2] << 16) +
				(bkpt->original.bytes[3] << 24);
	}
	dbgpFlushCache(&info.address);
	s_swbkptNext = s_swbkptNext == 255 ? 0 : s_swbkptNext + 1;
	return 0;
}

int swbkptSetSpecificSwbkpt(DbgpProcCB *proc, SwbkptSpecificInfo info)
{
	SceUInt16 thumbBkpt = SWBKPT_THUMB;
	SceUInt32 armBkpt = SWBKPT_ARM;
	int result;

	(void)proc;
	if (info.size < sizeof(info))
		return (int)0x80080001;
	if (info.thumb == 0)
		result = sceKernelCopyToUserProcTextDomainForDBGP(info.pid,
								  info.address,
								  &armBkpt, 4);
	else
		result = sceKernelCopyToUserProcTextDomainForDBGP(info.pid,
								  info.address,
								  &thumbBkpt, 2);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 450, "0x%08x\n", result);
		return result;
	}
	dbgpFlushCache(&info.address);
	return 0;
}

int swbkptClearSwbrkpt(DbgpProcCB *proc, const DbgpClearSwbrkptRequest *request,
		       int keep)
{
	SceUID pid = request->header.pid;
	SceUInt32 id = request->id;
	SceUInt8 current[4];
	SwbkptInfo *bkpt, *other;
	int i, j, result;

	for (i = 0; i != 256; ++i) {
		bkpt = &proc->swbkpt[i];
		if (bkpt->active == 0 || bkpt->pid != pid ||
		    (id != (SceUInt32)-1 && bkpt->id != id))
			continue;
		for (j = 0; j != 256; ++j) {
			other = &proc->swbkpt[j];
			if (other->active != 0 && pid == other->pid &&
			    other->address == bkpt->address && i != j)
				break;
		}
		if (j != 256)
			goto clear;
		if ((bkpt->attr & 1) == 0) {
			sceKernelCopyFromUserProc(pid, current, bkpt->address, 4);
			if (current[0] + (current[2] << 16) + (current[3] << 24) +
			    (current[1] << 8) != SWBKPT_ARM)
				goto clear;
		} else {
			sceKernelCopyFromUserProc(pid, current, bkpt->address, 2);
			if (current[0] + (current[1] << 8) != SWBKPT_THUMB)
				goto clear;
		}
		if (keep == 0) {
			result = sceKernelCopyToUserProcTextDomainForDBGP(
				request->header.pid, bkpt->address,
				&bkpt->original, (bkpt->attr & 1) ? 2 : 4);
			if (result < 0) {
				SCE_PRINTF_WITH_INFO(15, 514, "0x%08x\n", result);
				return result;
			}
			dbgpFlushCache(&bkpt->address);
		}
clear:
		excpInfoCancel(proc, bkpt->pid, bkpt->thid, bkpt->address);
		memset(bkpt, 0, sizeof(*bkpt));
		if (id != (SceUInt32)-1)
			break;
	}
	return 0;
}

int swbkptClearSwbrkptDetach(DbgpProcCB *proc, SceUID pid)
{
	SwbkptInfo *bkpt;
	int count, result;

	for (bkpt = proc->swbkpt, count = 256; count != 0; ++bkpt, --count) {
		if (bkpt->active == 0 || bkpt->pid != pid)
			continue;
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid,
				bkpt->address, &bkpt->original,
				(bkpt->attr & 1) ? 2 : 4);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 571, "0x%08x\n", result);
		dbgpFlushCache(&bkpt->address);
		excpInfoCancel(proc, bkpt->pid, bkpt->thid, bkpt->address);
		memset(bkpt, 0, sizeof(*bkpt));
	}
	return 0;
}

int swbkptClearSwbrkptProcess(DbgpProcCB *proc, SceUID pid)
{
	SwbkptInfo *bkpt;
	int count;

	for (bkpt = proc->swbkpt, count = 256; count != 0; ++bkpt, --count) {
		if (bkpt->active == 0 || bkpt->pid != pid)
			continue;
		excpInfoCancel(proc, pid, bkpt->thid, bkpt->address);
		memset(bkpt, 0, sizeof(*bkpt));
	}
	return 0;
}

int swbkptClearSwbrkptModule(DbgpProcCB *proc, SceUID pid, SceUID modid)
{
	SwbkptInfo *bkpt;
	int count;

	for (bkpt = proc->swbkpt, count = 256; count != 0; ++bkpt, --count) {
		if (bkpt->active == 0 || bkpt->pid != pid ||
		    bkpt->modid != modid)
			continue;
		excpInfoCancel(proc, pid, bkpt->thid, bkpt->address);
		memset(bkpt, 0, sizeof(*bkpt));
	}
	return 0;
}

int swbkptGetSpecificSwbkpt(const DbgpProcCB *proc, SceUID pid, SceUID thid,
			    void *address, SwbkptSpecificInfo *info)
{
	const SwbkptInfo *bkpt;
	int i;

	if (info->size != sizeof(*info) || info->index > 2)
		return (int)0x80080001;
	info->pid = pid;
	info->thid = thid;
	info->address = address;
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].address == address &&
		    proc->swbkpt[i].pid == pid)
			break;
	}
	if (i == 256)
		return -5;
	bkpt = &proc->swbkpt[i];
	info->thumb = bkpt->attr & 1;
	info->original.bytes[0] = bkpt->original.bytes[0];
	info->original.bytes[1] = bkpt->original.bytes[1];
	if (info->thumb == 0) {
		info->original.bytes[2] = bkpt->original.bytes[2];
		info->original.bytes[3] = bkpt->original.bytes[3];
	}
	return 0;
}

int swbkptGetOriginalOpcode(SceUID pid, void *address, SceUInt32 *opcode)
{
	DbgpProcCB *proc;
	const SwbkptInfo *bkpt;
	int i;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL) {
		SCE_PRINTF_WITH_INFO(15, 863, "0x%08x\n", pid);
		return (int)0x80080001;
	}
	*opcode = 0;
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].pid == pid && proc->swbkpt[i].address == address)
			break;
	}
	if (i == 256)
		return (int)0x80080001;
	bkpt = &proc->swbkpt[i];
	if (bkpt->attr & 1) {
		*opcode += bkpt->original.bytes[1] << 8;
		*opcode += bkpt->original.bytes[0];
	} else {
		*opcode += bkpt->original.bytes[3] << 24;
		*opcode += bkpt->original.bytes[2] << 16;
		*opcode += bkpt->original.bytes[1] << 8;
		*opcode += bkpt->original.bytes[0];
	}
	return 0;
}

int swbkptSetNewOpcode(SceUID pid, SceUInt32 address, SceUInt32 instruction)
{
	DbgpProcCB *proc;
	SwbkptInfo *bkpt;
	int i;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL) {
		SCE_PRINTF_WITH_INFO(15, 912, "0x%08x\n", pid);
		return (int)0x80080001;
	}
	for (i = 0; i != 256; ++i) {
		if (proc->swbkpt[i].pid == pid &&
		    proc->swbkpt[i].address == (void *)address)
			break;
	}
	if (i == 256)
		return (int)0x80080001;
	bkpt = &proc->swbkpt[i];
	if (bkpt->attr & 1) {
		bkpt->original.bytes[0] = instruction;
		bkpt->original.bytes[1] = instruction >> 8;
	} else {
		bkpt->original.bytes[0] = instruction;
		bkpt->original.bytes[1] = instruction >> 8;
		bkpt->original.bytes[2] = instruction >> 16;
		bkpt->original.bytes[3] = instruction >> 24;
	}
	return 0;
}

int swbkptInit(DbgpCB *dbgp)
{
	(void)dbgp;
	return 0;
}
