/*
 * SceDeci4pRDrfp write buffers (Sony's rdrfp_writebuf_list.c). Consecutive
 * writes to one file are collected in a write buffer and written as one
 * request; buffers left waiting are flushed by a 3 s timer, the second time
 * it finds them.
 */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include "rdrfp.h"

#define RDRFP_FLUSH_INTERVAL	3000000		/* us */

typedef struct RdrfpWritebufList {
	RdrfpLink head;
	SceUID mutex;
} RdrfpWritebufList;

static RdrfpWritebufList rdrfpWritebufList;
static SceUID rdrfpWritebufCallback;
static SceUID rdrfpWritebufTimer;
static RdrfpFileCacheEntry rdrfpFileCache[RDRFP_FILE_CACHE];

#define WRITEBUF_HEAD	(&rdrfpWritebufList.head)

static inline void writebufUnlink(RdrfpLink *link)
{
	link->next->prev = link->prev;
	link->prev->next = link->next;
	link->next = link;
	link->prev = link;
}

static int requestWritebufCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				   void *common)
{
	RdrfpLink *link, *next;
	RdrfpWritebuf *wb;
	int flushed = 0;

	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;

	sceKernelLockMutex(rdrfpWritebufList.mutex, 1, NULL);
	if (rdrfpWritebufList.head.next != WRITEBUF_HEAD) {
		for (link = rdrfpWritebufList.head.next; link != WRITEBUF_HEAD; link = next) {
			next = link->next;
			wb = (RdrfpWritebuf *)link;
			if (wb->flushed) {
				writebufUnlink(link);
				SCE_DBG_PRINTF_WITH_INFO(0xf, 62, "TimerCallbackFlush\n");
				rdrfpRequestWritebuf(wb, 2);
				flushed = 1;
			}
			wb->flushed = 1;
		}
		if (flushed && rdrfpWritebufList.head.next == WRITEBUF_HEAD)
			sceKernelStopTimer(rdrfpWritebufTimer);
	}
	sceKernelUnlockMutex(rdrfpWritebufList.mutex, 1);
	return 0;
}

int rdrfpInitWritebufList(void)
{
	int ret;

	rdrfpWritebufList.head.next = WRITEBUF_HEAD;
	rdrfpWritebufList.head.prev = WRITEBUF_HEAD;
	ret = sceKernelCreateMutex("SceDeci4pRDrfp", 0x2000, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 85, "CreateMutex failed (0x%08X)\n", ret);
		return ret;
	}
	rdrfpWritebufList.mutex = ret;
	return 0;
}

RdrfpWritebuf *rdrfpCreateWritebuf(SceUInt32 fd, SceOff offset, int bufType, int bufIndex)
{
	RdrfpWritebuf *wb = rdrfpAlloc(sizeof(*wb));

	if (wb == NULL) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 110, "AllocateHeapMemory failed\n");
		return NULL;
	}
	wb->link.next = &wb->link;
	wb->link.prev = &wb->link;
	wb->fd = fd;
	wb->bufType = bufType;
	wb->bufIndex = bufIndex;
	wb->count = 0;
	wb->size = 0;
	wb->buf = rdrfpBufferAddr(bufType, bufIndex);
	wb->offset = offset;
	wb->flushed = 0;
	return wb;
}

void rdrfpDeleteWritebuf(RdrfpWritebuf *wb)
{
	rdrfpFree(wb);
}

void rdrfpPutWritebufToList(RdrfpWritebuf *wb)
{
	RdrfpLink *link = &wb->link;
	RdrfpLink *head = WRITEBUF_HEAD;

	sceKernelLockMutex(rdrfpWritebufList.mutex, 1, NULL);
	SCE_DBG_ASSERT_LEVEL(0, link == link->next, 134);
	if (head->next == head)
		sceKernelStartTimer(rdrfpWritebufTimer);
	link->next = head;
	link->prev = head->prev;
	head->prev = link;
	wb->flushed = 0;
	link->prev->next = link;
	sceKernelUnlockMutex(rdrfpWritebufList.mutex, 1);
}

/* the oldest waiting write buffer, taken off the list */
RdrfpWritebuf *rdrfpGetWritebufFromList(void)
{
	RdrfpWritebuf *wb;

	sceKernelLockMutex(rdrfpWritebufList.mutex, 1, NULL);
	wb = (RdrfpWritebuf *)rdrfpWritebufList.head.next;
	if (&wb->link == WRITEBUF_HEAD) {
		wb = NULL;
	} else {
		writebufUnlink(&wb->link);
		if (rdrfpWritebufList.head.next == WRITEBUF_HEAD)
			sceKernelStopTimer(rdrfpWritebufTimer);
	}
	sceKernelUnlockMutex(rdrfpWritebufList.mutex, 1);
	return wb;
}

/* the waiting write buffer a write at offset continues, if it has room */
RdrfpWritebuf *rdrfpFindWritebuf(SceUInt32 fd, SceOff offset, SceSize size)
{
	RdrfpLink *link;
	RdrfpWritebuf *wb;

	sceKernelLockMutex(rdrfpWritebufList.mutex, 1, NULL);
	for (link = rdrfpWritebufList.head.next; link != WRITEBUF_HEAD; link = link->next) {
		wb = (RdrfpWritebuf *)link;
		if (wb->fd == fd && wb->offset + wb->size == offset &&
		    wb->size + size <= RDRFP_WRITE_MAX) {
			writebufUnlink(link);
			if (rdrfpWritebufList.head.next == WRITEBUF_HEAD)
				sceKernelStopTimer(rdrfpWritebufTimer);
			goto out;
		}
	}
	wb = NULL;
out:
	sceKernelUnlockMutex(rdrfpWritebufList.mutex, 1);
	return wb;
}

void rdrfpWritebufAddSize(RdrfpWritebuf *wb, SceSize size)
{
	wb->size += size;
}

void rdrfpWritebufAddId(RdrfpWritebuf *wb, SceUInt32 id)
{
	wb->ids[wb->count++] = id;
}

int rdrfpRegisterWritebufTimer(void)
{
	SceKernelSysClock interval;
	int ret;

	ret = sceKernelCreateCallback("SceDeci4pRDrfp", 0, requestWritebufCallback, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 206, "CreateCallback failed (0x%08X)\n", ret);
		return ret;
	}
	rdrfpWritebufCallback = ret;
	ret = sceKernelCreateTimer("SceDeci4pRDrfp", 0x100, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 214, "CreateTimer failed (0x%08X)\n", ret);
		goto delete_callback;
	}
	rdrfpWritebufTimer = ret;
	interval.quad = RDRFP_FLUSH_INTERVAL;
	ret = sceKernelSetTimerEvent(rdrfpWritebufTimer, 0, &interval, 1);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 223, "SetTimerEvent failed (0x%08X)\n", ret);
		SceThreadmgrForDriver_91A8F38B(rdrfpWritebufTimer);
		goto delete_callback;
	}
	ret = sceKernelRegisterCallbackToEventForDriver(rdrfpWritebufTimer,
					      rdrfpWritebufCallback, rdrfpWritebufCallback);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 232, "RegisterCallbackToEvent failed (0x%08x)\n", ret);
		return ret;
	}
	return 0;

delete_callback:
	sceKernelDeleteCallback(rdrfpWritebufCallback);
	return ret;
}

void rdrfpUnregisterWritebufTimer(void)
{
	sceKernelStopTimer(rdrfpWritebufTimer);
	SceThreadmgrForDriver_91A8F38B(rdrfpWritebufTimer);
	/* Sony deletes the timer's UID as the callback */
	sceKernelDeleteCallback(rdrfpWritebufTimer);
}

/* ===== file cache ===== */

int rdrfpInitFileCache(void)
{
	memset(rdrfpFileCache, 0, sizeof(rdrfpFileCache));
	return 0;
}

/* remember a file, in a free entry or in place of the oldest */
RdrfpFileCacheEntry *rdrfpAddFileCache(SceUInt32 fd, SceOff offset, SceUInt32 result)
{
	RdrfpFileCacheEntry *entry, *oldest = NULL;
	int i, age = -1;

	for (i = 0; i < RDRFP_FILE_CACHE; i++) {
		if (!rdrfpFileCache[i].used)
			break;
		if (age < rdrfpFileCache[i].age) {
			oldest = &rdrfpFileCache[i];
			age = rdrfpFileCache[i].age;
		}
	}
	entry = i < RDRFP_FILE_CACHE ? &rdrfpFileCache[i] : NULL;
	for (i = 0; i < RDRFP_FILE_CACHE; i++)
		rdrfpFileCache[i].age++;
	if (entry == NULL)
		entry = oldest;
	if (entry) {
		entry->offset = offset;
		entry->used = 1;
		entry->fd = fd;
		entry->age = 0;
		entry->result = result;
	}
	return entry;
}

void rdrfpDeleteFileCache(RdrfpFileCacheEntry *entry)
{
	entry->used = 0;
	entry->age = 0;
}

RdrfpFileCacheEntry *rdrfpFindFileCache(SceUInt32 fd, SceOff offset)
{
	int i;

	for (i = 0; i < RDRFP_FILE_CACHE; i++) {
		if (rdrfpFileCache[i].used && rdrfpFileCache[i].fd == fd &&
		    rdrfpFileCache[i].offset == offset)
			return &rdrfpFileCache[i];
	}
	return NULL;
}

void rdrfpFileCacheAddOffset(RdrfpFileCacheEntry *entry, SceSize size)
{
	entry->offset += size;
}
