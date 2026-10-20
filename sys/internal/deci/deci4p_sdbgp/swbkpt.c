/*
 * Software break points: a BKPT written over the code, the code kept in the
 * table to put back.
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "sdbgp_internal.h"

/* What a BKPT's immediate says about why the core stopped. */
int swbkptTypeOpcode(uint32_t opcode, int thumb, uint32_t cpsr)
{
	uint32_t mode = cpsr & PSR_MODE_MASK;
	uint32_t immediate;

	if (thumb == 1) {
		if ((opcode & 0xff00) != SWBKPT_THUMB_BKPT)
			return SDBGP_STOP_PABT;
		if ((uint8_t)opcode > 0xdf)
			return SDBGP_STOP_BKPT_E0;
		if ((uint8_t)opcode > 0xbf)
			return SDBGP_STOP_BKPT_C0;
		if ((uint8_t)opcode > 0x7f)
			return SDBGP_STOP_SWBKPT;
	} else {
		if ((opcode & 0xfff000f0) != SWBKPT_ARM_BKPT)
			return SDBGP_STOP_PABT;
		immediate = opcode & 0xfff0f;
		if (immediate >= 0xe00)
			return SDBGP_STOP_BKPT_E0;
		if (immediate >= 0xc00)
			return SDBGP_STOP_BKPT_C0;
		if (immediate >= 0x800)
			return SDBGP_STOP_SWBKPT;
	}
	if (mode == PSR_MODE_USR)
		return SDBGP_STOP_SWBKPT_USER;
	return SDBGP_STOP_SWBKPT;
}

int swbkptFindByAddress(SceUID pid, uintptr_t address)
{
	SdbgpState *state = sdbgpGetState();
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (state->swbkpt[i].address == address)
			return i;
	}
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

/* Clear one entry, or the whole table for SDBGP_SWBKPT_MAX. */
int swbkptClear(int index)
{
	SdbgpState *state = sdbgpGetState();

	if (index == SDBGP_SWBKPT_MAX) {
		SceDeci4pSDfMgrForKernel_8A3B06F2(state->swbkpt, 0, sizeof(state->swbkpt));
		return 0;
	}
	SceDeci4pSDfMgrForKernel_8A3B06F2(&state->swbkpt[index], 0, sizeof(SdbgpSwbkpt));
	return 0;
}

/* Put the kernel's code back over its break point. */
int _swbkptRemoveKernel(SdbgpState *state, int index)
{
	SdbgpSwbkpt *bkpt = &state->swbkpt[index];
	void *address = (void *)bkpt->address;
	SceSize size = (bkpt->thumb & 1) ? 2 : 4;
	uint32_t dacr;
	uintptr_t start;

	dacr = readDacr();
	writeDacr(0xffff0000);
	start = (uintptr_t)memcpy(address, bkpt->opcode, size);
	sceKernelDcacheCleanRangeForL1WBWA((void *)(start & ~0x1f),
		((start + size + 31) & ~0x1f) - (start & ~0x1f));
	writeDacr(dacr);
	SceCpuForKernel_803C84BF();
	swbkptClear(index);
	return 0;
}

/* Put a process's code back over its break point. */
int _swbkptRemoveUser(SdbgpState *state, int index)
{
	SdbgpSwbkpt *bkpt = &state->swbkpt[index];
	int result;

	result = sceKernelCopyToUserProcTextDomainForDBGP(bkpt->processId,
		(void *)bkpt->address, bkpt->opcode, (bkpt->thumb & 1) ? 2 : 4);
	if (result < 0)
		return result;
	SceCpuForKernel_803C84BF();
	swbkptClear(index);
	return 0;
}

int swbkptCount(void)
{
	SdbgpState *state = sdbgpGetState();
	int count = 0;
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (state->swbkpt[i].used)
			count++;
	}
	return count;
}

/* List the break points, or the one with the id. */
int swbkptGetList(SdbgpSwbkptListReply *reply, uint32_t id)
{
	SdbgpState *state = sdbgpGetState();
	SdbgpSwbkptInfo *info;
	int num = 0;
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (!state->swbkpt[i].used)
			continue;
		info = &reply->info[num];
		if (id != (uint32_t)-1 && state->swbkpt[i].id != id)
			continue;
		info->size = sizeof(*info);
		num++;
		info->id = state->swbkpt[i].id;
		info->processId = state->swbkpt[i].processId;
		info->threadId = state->swbkpt[i].threadId;
		info->address = state->swbkpt[i].address;
		info->thumb = state->swbkpt[i].thumb;
		info->reserved = 0;
		info->attr = state->swbkpt[i].attr;
		info->kind = 8;
		if (id != (uint32_t)-1) {
			reply->num = 1;
			return 0;
		}
	}
	reply->num = num;
	return 0;
}

/* Set a break point in the kernel. */
int swbkptSetKernel(const SdbgpSwbkptSetRequest *request, SdbgpSwbkptSetReply *reply)
{
	uint32_t armBkpt = SWBKPT_ARM_BKPT_DEBUGGER;
	SdbgpState *state = sdbgpGetState();
	uintptr_t address = request->address;
	uint32_t thumb = request->thumb;
	uint32_t attr = request->attr;
	uint32_t opcode = 0;
	uint8_t code[4];
	SdbgpSwbkpt *bkpt;
	uint32_t dacr;
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (!state->swbkpt[i].used)
			break;
	}
	if (i == SDBGP_SWBKPT_MAX)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	bkpt = &state->swbkpt[i];
	reply->id = 0x20000 + i;
	bkpt->thumb = thumb;
	bkpt->attr = attr;
	bkpt->processId = SDBGP_KERNEL_PROCESS_ID;
	bkpt->id = 0x20000 + i;
	bkpt->address = address;
	bkpt->threadId = -1;
	bkpt->used = 1;
	if (thumb & 1) {
		SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)address, 2);
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, code, 2);
		dacr = readDacr();
		writeDacr(0xffff0000);
		*(volatile uint16_t *)address = SWBKPT_THUMB_BKPT_DEBUGGER;
		sceKernelDcacheCleanRangeForL1WBWA((void *)(address & ~0x1f),
			((address + 33) & ~0x1f) - (address & ~0x1f));
		writeDacr(dacr);
		reply->size = 2;
		sdbgpDecodeOpcode(1, code, &opcode);
	} else {
		SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)address, 4);
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, code, 4);
		dacr = readDacr();
		writeDacr(0xffff0000);
		*(volatile uint32_t *)address = armBkpt;
		sceKernelDcacheCleanRangeForL1WBWA((void *)(address & ~0x1f),
			((address + 35) & ~0x1f) - (address & ~0x1f));
		writeDacr(dacr);
		reply->size = 4;
		sdbgpDecodeOpcode(0, code, &opcode);
	}
	reply->opcode = opcode;
	SceCpuForKernel_803C84BF();
	return 0;
}

/* Set a break point in a process. */
int swbkptSetUser(const SdbgpSwbkptSetRequest *request, SdbgpSwbkptSetReply *reply)
{
	uint32_t opcode = 0;
	uint32_t armBkpt = SWBKPT_ARM_BKPT_DEBUGGER;
	uint16_t thumbBkpt = SWBKPT_THUMB_BKPT_DEBUGGER;
	SdbgpState *state = sdbgpGetState();
	SceUID pid = request->processId;
	uintptr_t address = request->address;
	uint32_t thumb = request->thumb;
	uint32_t attr = request->attr;
	uint8_t code[4];
	SdbgpSwbkpt *bkpt;
	int result;
	int i;

	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (!state->swbkpt[i].used)
			break;
	}
	if (i == SDBGP_SWBKPT_MAX)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	bkpt = &state->swbkpt[i];
	reply->id = 0x20000 + i;
	bkpt->address = address;
	bkpt->thumb = thumb;
	bkpt->attr = attr;
	bkpt->id = 0x20000 + i;
	bkpt->processId = pid;
	bkpt->threadId = -1;
	bkpt->used = 1;
	if (thumb & 1) {
		result = sceKernelCopyFromUserProc(pid, code, (const void *)address, 2);
		if (result < 0)
			return result;
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, code, 2);
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)address,
								  &thumbBkpt, 2);
		if (result < 0)
			return result;
		reply->size = 2;
		sdbgpDecodeOpcode(1, code, &opcode);
	} else {
		result = sceKernelCopyFromUserProc(pid, code, (const void *)address, 4);
		if (result < 0)
			return result;
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, code, 4);
		result = sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)address,
								  &armBkpt, 4);
		if (result < 0)
			return result;
		reply->size = 4;
		sdbgpDecodeOpcode(0, code, &opcode);
	}
	reply->opcode = opcode;
	SceCpuForKernel_803C84BF();
	return 0;
}

/* Take out the break point with the id, or all of them for -1. */
int swbkptClearById(uint32_t id)
{
	SdbgpState *state = sdbgpGetState();
	int result;
	int i;

	if (!state)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (id == (uint32_t)-1) {
		for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
			if (!state->swbkpt[i].used)
				continue;
			if (state->swbkpt[i].processId == SDBGP_KERNEL_PROCESS_ID)
				result = _swbkptRemoveKernel(state, i);
			else
				result = _swbkptRemoveUser(state, i);
			if (result < 0)
				return result;
		}
		return 0;
	}
	for (i = 0; ; i++) {
		if (state->swbkpt[i].used && state->swbkpt[i].id == id)
			break;
		if (i + 1 == SDBGP_SWBKPT_MAX)
			return 0;
	}
	if (state->swbkpt[i].processId == SDBGP_KERNEL_PROCESS_ID)
		result = _swbkptRemoveKernel(state, i);
	else
		result = _swbkptRemoveUser(state, i);
	return result & (result >> 31);
}

/* The immediate of the BKPT at a kernel address. */
uint32_t swbkptGetImmediate(uintptr_t address, uint32_t cpsr)
{
	uint32_t opcode = 0;
	uint8_t code[4];

	if (cpsr & PSR_THUMB) {
		SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)address, 2);
		sdbgpDecodeOpcode(1, code, &opcode);
		return (uint8_t)opcode;
	}
	SceDeci4pSDfMgrForKernel_5996E2DB(code, (const void *)address, 4);
	sdbgpDecodeOpcode(0, code, &opcode);
	return (opcode & 0xf) + ((opcode & 0xfff00) >> 4);
}

/* Take a break point out to step over it, keeping it to put back. */
int swbkptDisableSWBP(SdbgpSwbkpt bkpt)
{
	SdbgpState *state = sdbgpGetState();
	int result;

	if (!state)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	SceDeci4pSDfMgrForKernel_8A3B06F2(&state->sysSwbkpt, 0, sizeof(state->sysSwbkpt));
	SceDeci4pSDfMgrForKernel_5996E2DB(&state->sysSwbkpt, &bkpt, sizeof(bkpt));
	result = sdbgpWriteMemory(bkpt.processId, (void *)bkpt.address,
				  (bkpt.thumb & 1) ? 2 : 4, bkpt.opcode);
	if (result >= 0)
		return 0;
	SCE_DBG_PRINTF_WITH_INFO(0, 568, "sdbgpCopyToUserTextDomain failed 0x%08x\n", result);
	return result;
}

/* Put the debugger's BKPT back over the break point taken out. */
int swbkptEnableSWBP(void)
{
	uint16_t thumbBkpt = SWBKPT_THUMB_BKPT_DEBUGGER;
	uint32_t armBkpt = SWBKPT_ARM_BKPT_DEBUGGER;
	SdbgpState *state = sdbgpGetState();
	SceSize size;
	const void *code;
	int result;

	if (!state)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (state->sysSwbkpt.thumb & 1) {
		size = 2;
		code = &thumbBkpt;
	} else {
		size = 4;
		code = &armBkpt;
	}
	result = sdbgpWriteMemory(state->sysSwbkpt.processId,
				  (void *)state->sysSwbkpt.address, size, code);
	return result & (result >> 31);
}

/* Put the system's own BKPT back over the break point taken out. */
int swbkptEnableSysSWBP(void)
{
	uint32_t armBkpt = SWBKPT_ARM_BKPT;
	uint16_t thumbBkpt = SWBKPT_THUMB_BKPT;
	SdbgpState *state = sdbgpGetState();
	SceSize size;
	const void *code;
	int result;

	if (!state)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	armBkpt = state->sysSwbkpt.immediate + SWBKPT_ARM_BKPT;
	thumbBkpt = state->sysSwbkpt.immediate + SWBKPT_THUMB_BKPT;
	if (state->sysSwbkpt.thumb & 1) {
		size = 2;
		code = &thumbBkpt;
	} else {
		code = &armBkpt;
		size = 4;
	}
	result = sdbgpWriteMemory(state->sysSwbkpt.processId,
				  (void *)state->sysSwbkpt.address, size, code);
	if (result < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0, 632, "sdbgpCopyToUserTextDomain failed 0x%08x\n",
					 result);
		return result;
	}
	return 0;
}

int swbkptWriteBkpt(const SdbgpSwbkpt *bkpt)
{
	uint16_t thumbBkpt;
	uint32_t armBkpt = SWBKPT_ARM_BKPT_DEBUGGER;
	SceSize size;
	const void *code;
	int result;

	if (bkpt->thumb & 1) {
		size = 2;
		code = &thumbBkpt;
	} else {
		size = 4;
		code = &armBkpt;
	}
	thumbBkpt = SWBKPT_THUMB_BKPT_DEBUGGER;
	result = sdbgpWriteMemory(bkpt->processId, (void *)bkpt->address, size, code);
	return result & (result >> 31);
}

int swbkptRemove(SdbgpSwbkpt *bkpt)
{
	int result;

	result = sdbgpWriteMemory(bkpt->processId, (void *)bkpt->address,
				  (bkpt->thumb & 1) ? 2 : 4, bkpt->opcode);
	if (result < 0)
		return result;
	SceDeci4pSDfMgrForKernel_8A3B06F2(bkpt, 0, sizeof(*bkpt));
	return 0;
}

/* Fill an entry for a break point outside the table, a no-op as its code. */
int swbkptMakeEntry(SceUID pid, SceUID tid, uintptr_t address, uint32_t cpsr,
		    int readImmediate, SdbgpSwbkpt *bkpt)
{
	uint32_t armNop = SWBKPT_ARM_NOP;
	uint16_t thumbNop = SWBKPT_THUMB_NOP;
	uint32_t opcode;
	uint8_t code[4];
	int result;

	if (!bkpt)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	bkpt->processId = pid;
	bkpt->threadId = tid;
	bkpt->address = address;
	bkpt->thumb = (cpsr & PSR_THUMB) != 0;
	bkpt->id = -1;
	bkpt->used = 1;
	if (!readImmediate) {
		if (cpsr & PSR_THUMB)
			SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, (const void *)address, 2);
		else
			SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, (const void *)address, 4);
		return 0;
	}
	if (cpsr & PSR_THUMB) {
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, &thumbNop, 2);
		opcode = 0;
		result = sdbgpReadMemory(pid, (const void *)address, 2, code);
		if (result < 0)
			return result;
		sdbgpDecodeOpcode(1, code, &opcode);
		bkpt->immediate = (uint8_t)opcode;
	} else {
		SceDeci4pSDfMgrForKernel_5996E2DB(bkpt->opcode, &armNop, 4);
		opcode = 0;
		result = sdbgpReadMemory(pid, (const void *)address, 4, code);
		if (result < 0)
			return result;
		sdbgpDecodeOpcode(0, code, &opcode);
		bkpt->immediate = opcode & 0xfff0f;
	}
	return 0;
}

/* Is the code at the address a BKPT? */
int swbkptIsBkpt(SceUID pid, uintptr_t address, uint32_t cpsr, int *isBkpt)
{
	uint32_t thumb = cpsr & PSR_THUMB;
	uint32_t opcode = 0;
	uint8_t code[4];
	int result;

	*isBkpt = 0;
	if (thumb) {
		result = sdbgpReadMemory(pid, (const void *)address, 2, code);
		if (result < 0)
			return result;
		sdbgpDecodeOpcode(1, code, &opcode);
		if ((opcode & 0xff00) != SWBKPT_THUMB_BKPT)
			return 0;
		*isBkpt = 1;
		return 0;
	}
	result = sdbgpReadMemory(pid, (const void *)address, 4, code);
	if (result < 0)
		return result;
	sdbgpDecodeOpcode(thumb, code, &opcode);
	if ((opcode & 0xfff000f0) != SWBKPT_ARM_BKPT)
		return 0;
	*isBkpt = 1;
	return thumb;
}

/* Show the code under the process's break points in memory read into buf. */
int swbkptHideInBuffer(SceUID pid, uintptr_t start, SceSize size, uint8_t *buf)
{
	SdbgpState *state;
	uintptr_t end;
	int i;

	if (!buf)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	state = sdbgpGetState();
	end = start + size;
	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		SdbgpSwbkpt *bkpt = &state->swbkpt[i];
		uint8_t *p;

		if (!bkpt->used || bkpt->processId != pid)
			continue;
		if (bkpt->address < start || bkpt->address >= end)
			continue;
		p = &buf[bkpt->address - start];
		p[0] = bkpt->opcode[0];
		p[1] = bkpt->opcode[1];
		if (!(bkpt->thumb & 1)) {
			p[2] = bkpt->opcode[2];
			p[3] = bkpt->opcode[3];
		}
	}
	return 0;
}
