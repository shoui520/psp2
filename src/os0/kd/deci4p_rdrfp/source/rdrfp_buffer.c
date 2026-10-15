/*
 * SceDeci4pRDrfp packet buffers and send descriptors (Sony's rdrfp_buffer.c).
 * Three pools of memory blocks: receive buffers for requests, read buffers
 * for read replies and write buffers collecting written data.
 */
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include "rdrfp.h"

static RdrfpBufferTable rdrfpBufferTable[RDRFP_BUF_TYPES];
static RdrfpBuffer rdrfpReadBuffers[2];
static RdrfpBuffer rdrfpRecvBuffers[9];
static RdrfpBuffer rdrfpWriteBuffers[8];

static const char *const rdrfpBufferNames[RDRFP_BUF_TYPES] = { "Recv", "Read", "Write" };

int rdrfpInitBufferTable(void)
{
	RdrfpBufferTable *table;
	char name[32];
	int type, i, ret;

	memset(rdrfpBufferTable, 0, sizeof(rdrfpBufferTable));
	rdrfpBufferTable[RDRFP_BUF_RECV].size = 0x11000;
	rdrfpBufferTable[RDRFP_BUF_RECV].count = 9;
	rdrfpBufferTable[RDRFP_BUF_RECV].buffers = rdrfpRecvBuffers;
	rdrfpBufferTable[RDRFP_BUF_READ].count = 2;
	rdrfpBufferTable[RDRFP_BUF_READ].buffers = rdrfpReadBuffers;
	rdrfpBufferTable[RDRFP_BUF_WRITE].count = 8;
	rdrfpBufferTable[RDRFP_BUF_WRITE].buffers = rdrfpWriteBuffers;
	rdrfpBufferTable[RDRFP_BUF_READ].size = 0x41000;
	rdrfpBufferTable[RDRFP_BUF_WRITE].size = 0x41000;

	for (type = 0; type < RDRFP_BUF_TYPES; type++) {
		table = &rdrfpBufferTable[type];
		ret = ksceKernelCreateMutex("SceDeci4pRDrfp", 0x2000, 0, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 79, "CreateMutex failed (0x%08X)\n", ret);
			goto failed;
		}
		table->mutex = ret;
		ret = ksceKernelCreateEventFlag("SceDeci4pRDrfp", 0, 0, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 89, "CreateCond failed (0x%08X)\n", ret);
			goto failed;
		}
		table->evf = ret;
		for (i = 0; i < table->count; i++) {
			snprintf(name, sizeof(name), "SceDeci4pRDrfpMemBlock%s%d", rdrfpBufferNames[type], i);
			ret = ksceKernelAllocMemBlock(name, 0x10f0d006, table->size, NULL);
			if (ret < 0) {
				SCE_DBG_PRINTF_WITH_INFO(0xf, 103, "AllocMemBlockForKernel failed (0x%08X)\n", ret);
				goto failed;
			}
			table->buffers[i].block = ret;
			ret = ksceKernelGetMemBlockBase(ret, &table->buffers[i].base);
			if (ret < 0) {
				SCE_DBG_PRINTF_WITH_INFO(0xf, 113, "GetMemBlockBase failed (0x%08X)\n", ret);
				goto failed;
			}
		}
	}
	return 0;

failed:
	for (table = rdrfpBufferTable; table != &rdrfpBufferTable[RDRFP_BUF_TYPES]; table++) {
		if (table->mutex)
			ksceKernelDeleteMutex(table->mutex);
		if (table->evf)
			ksceKernelDeleteEventFlag(table->evf);
		for (i = 0; i < table->count; i++) {
			if (table->buffers[i].block)
				ksceKernelFreeMemBlock(table->buffers[i].block);
		}
	}
	return ret;
}

/* a free buffer of the type, or -1; with wait, sleep until one is released */
int rdrfpReserveBuffer(int type, int wait)
{
	RdrfpBufferTable *table = &rdrfpBufferTable[type];
	int i, ret;

	ksceKernelLockMutex(table->mutex, 1, NULL);
	for (;;) {
		for (i = 0; i < table->count; i++) {
			if (!(table->buffers[i].flags & 1)) {
				table->buffers[i].flags |= 1;
				goto out;
			}
		}
		if (!wait) {
			i = -1;
			goto out;
		}
		ksceKernelUnlockMutex(table->mutex, 1);
		ret = ksceKernelWaitEventFlagCB(table->evf, 1, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT,
						NULL, NULL);
		ksceKernelLockMutex(table->mutex, 1, NULL);
		if (ret < 0)
			break;
	}
	i = -1;
	SCE_DBG_PRINTF_WITH_INFO(0xf, 216, "WaitCond failed (0x%08X)\n", ret);
out:
	ksceKernelUnlockMutex(table->mutex, 1);
	return i;
}

void rdrfpReleaseBuffer(int type, int index)
{
	SCE_DBG_ASSERT_LEVEL(0, index >= 0 && (unsigned int)type < RDRFP_BUF_TYPES, 235);
	ksceKernelLockMutex(rdrfpBufferTable[type].mutex, 1, NULL);
	SCE_DBG_ASSERT_LEVEL(0, index < rdrfpBufferTable[type].count, 239);
	rdrfpBufferTable[type].buffers[index].flags = 0;
	ksceKernelSetEventFlag(rdrfpBufferTable[type].evf, 1);
	ksceKernelUnlockMutex(rdrfpBufferTable[type].mutex, 1);
}

void *rdrfpBufferAddr(int type, int index)
{
	RdrfpBuffer *buffers;

	SCE_DBG_ASSERT_LEVEL(0, (unsigned int)type < RDRFP_BUF_TYPES, 251);
	SCE_DBG_ASSERT_LEVEL(0, index >= 0 && index < rdrfpBufferTable[type].count, 253);
	buffers = rdrfpBufferTable[type].buffers;
	SCE_DBG_ASSERT_LEVEL(0, buffers[index].flags & 1, 255);
	return buffers[index].base;
}

/* ===== descriptors ===== */

int rdrfpInitDesc(void)
{
	return 0;
}

RdrfpDesc *rdrfpCreateDesc(SceUInt32 id, int bufType, int bufIndex, void *packet)
{
	RdrfpDesc *desc = rdrfpAlloc(sizeof(*desc));

	if (desc) {
		memset(desc, 0, sizeof(*desc));
		desc->id = id;
		desc->bufType = bufType;
		desc->bufIndex = bufIndex;
		desc->packet = packet;
		desc->releaseBuffer = 1;
	}
	return desc;
}

void rdrfpDeleteDesc(RdrfpDesc *desc)
{
	if (desc->extra)
		rdrfpFree(desc->extra);
	rdrfpFree(desc);
}

void rdrfpSetDesc(RdrfpDesc *desc, int bufType, int bufIndex, void *packet, const void *data,
		  SceSize dataSize, int releaseBuffer)
{
	desc->bufType = bufType;
	desc->bufIndex = bufIndex;
	desc->packet = packet;
	desc->data = data;
	desc->dataSize = dataSize;
	desc->releaseBuffer = releaseBuffer;
}

void rdrfpSetDescExtra(RdrfpDesc *desc, void *extra)
{
	desc->extra = extra;
}
