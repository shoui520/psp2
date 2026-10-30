/*
 * dtracep_packet_queue.c: DTrace's DECI4P packets, each in a memory block of
 * its own, and the mutex-guarded queues they wait in to be sent.
 */
#include <dtrace/heap.h>
#include <dtrace/logging.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>
#include "dtracep_packet_queue.h"

DtracepPacketQueue *dtracepCreatePacketQueue(void)
{
	DtracepPacketQueue *queue;

	queue = dtrace_heap_alloc(sizeof(*queue));
	if (queue == NULL) {
		SCE_DBG_ASSERT_FAIL(26);
		return NULL;
	}
	memset(queue, 0, sizeof(*queue));

	queue->mutex = sceKernelCreateMutex("SceDeci4pDtracepPacketQ", 0x2000, 0, NULL);
	if (queue->mutex < 0) {
		DTRACE_LOG_ERROR_AT(35, "%s - CreateMutex failed (0x%08x)", __func__, queue->mutex);
		SCE_DBG_ASSERT_LEVEL(0, 0, 36);
	}
	list_create(&queue->packets, sizeof(DtracepPacket), offsetof(DtracepPacket, node));
	return queue;
}

void dtracepPushPacketQueue(DtracepPacketQueue *queue, DtracepPacket *packet)
{
	SCE_DBG_ASSERT_CHECK(queue != NULL, 46);

	if (sceKernelLockMutex(queue->mutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(51);

	list_insert_tail(&queue->packets, packet);

	if (sceKernelUnlockMutex(queue->mutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(58);
}

int dtracepIsEmptyPacketQueue(DtracepPacketQueue *queue)
{
	int empty;

	SCE_DBG_ASSERT_CHECK(queue != NULL, 64);

	if (sceKernelLockMutex(queue->mutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(71);

	empty = list_is_empty(&queue->packets);

	if (sceKernelUnlockMutex(queue->mutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(78);

	return empty;
}

DtracepPacket *dtracepPopPacketQueue(DtracepPacketQueue *queue)
{
	DtracepPacket *packet;

	SCE_DBG_ASSERT_CHECK(queue != NULL, 86);

	if (sceKernelLockMutex(queue->mutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(93);

	packet = NULL;
	if (!list_is_empty(&queue->packets)) {
		packet = list_head(&queue->packets);
		list_remove(&queue->packets, packet);
	}

	if (sceKernelUnlockMutex(queue->mutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(105);

	return packet;
}

void dtracepDrainPacketQueue(DtracepPacketQueue *queue)
{
	DtracepPacket *packet;

	SCE_DBG_ASSERT_CHECK(queue != NULL, 112);

	if (sceKernelLockMutex(queue->mutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(117);

	while ((packet = list_head(&queue->packets)) != NULL) {
		list_remove(&queue->packets, packet);
		dtracepFreePacket(packet);
	}

	if (sceKernelUnlockMutex(queue->mutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(128);
}

void dtracepDestroyPacketQueue(DtracepPacketQueue *queue)
{
	DtracepPacket *packet;

	SCE_DBG_ASSERT_CHECK(queue != NULL, 134);

	if (sceKernelLockMutex(queue->mutex, 1, NULL) < 0)
		SCE_DBG_ASSERT_FAIL(139);

	while ((packet = list_head(&queue->packets)) != NULL) {
		list_remove(&queue->packets, packet);
		dtracepFreePacket(packet);
	}
	list_destroy(&queue->packets);

	if (sceKernelUnlockMutex(queue->mutex, 1) < 0)
		SCE_DBG_ASSERT_FAIL(151);

	sceKernelDeleteMutex(queue->mutex);
	dtrace_heap_free(queue);
}

DtracepPacket *dtracepAllocPacket(void)
{
	DtracepPacket *packet;
	void *base;
	SceUID uid;
	int res;

	packet = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, sizeof(*packet));
	if (packet == NULL) {
		SCE_DBG_ASSERT_FAIL(167);
		return NULL;
	}
	memset(packet, 0, sizeof(*packet));

	uid = sceKernelAllocMemBlockForKernel("SceDeci4pDtracepPacket", 0x10f0d006, DTRACEP_PACKET_MAX, NULL);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(178, "%s - failed to alloc mem block (0x%08x)", __func__, uid);
		SCE_DBG_ASSERT_FAIL(179);
		return NULL;
	}
	packet->memBlock = uid;

	base = NULL;
	res = sceKernelGetMemBlockBase(uid, &base);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(187, "%s - failed to get memory block base (0x%08x)",
				    __func__, res);
		SCE_DBG_ASSERT_FAIL(188);
		return NULL;
	}
	packet->data = base;
	packet->header = base;
	return packet;
}

void dtracepInitPacket(DtracepPacket *packet, SceSize size, SceUInt8 command, int token)
{
	SceSize packetSize;

	SCE_DBG_ASSERT_CHECK(packet != NULL, 199);
	SCE_DBG_ASSERT_CHECK(size <= DTRACEP_PACKET_MAX, 200);

	packetSize = (size + 7) & ~7;
	packet->size = packetSize;
	packet->payloadSize = size - sizeof(DtracepHeader);
	memset(packet->data, 0, packetSize);
	SceDeci4pDfMgrForDebugger_6D26CC56(&packet->header->hdr, packetSize);
	packet->header->command = command;
	packet->header->token = token;
}

void dtracepFreePacket(DtracepPacket *packet)
{
	int res;

	SCE_DBG_ASSERT_CHECK(packet != NULL, 217);

	res = sceKernelFreeMemBlock(packet->memBlock);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(221, "%s - failed to free memory block (0x%08x)", __func__, res);

	sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, packet);
}
