/*
 * SceNpCommonPs4: a queue of fixed size items.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_eventQueueVtable[];	/* 0x8102a9f4 */

struct NpCommonPs4EventQueue *SceNpCommonPs4_2B902206(struct NpCommonPs4EventQueue *queue, struct NpCommonPs4Allocator *pool)
{
	queue->vtable = NP_COMMON_PS4_VTABLE(g_eventQueueVtable);
	SceNpCommonPs4_3245630F(&queue->ringBuf, pool);
	SceNpCommonPs4_4266D870(&queue->mutex);
	npCommonPs4CondConstruct(&queue->itemCond);
	npCommonPs4CondConstruct(&queue->spaceCond);
	queue->initialized = 0;
	return queue;
}

struct NpCommonPs4EventQueue *SceNpCommonPs4_7E37A6BF(struct NpCommonPs4EventQueue *queue, struct NpCommonPs4Allocator *pool)
{
	queue->vtable = NP_COMMON_PS4_VTABLE(g_eventQueueVtable);
	SceNpCommonPs4_3245630F(&queue->ringBuf, pool);
	SceNpCommonPs4_4266D870(&queue->mutex);
	npCommonPs4CondConstruct(&queue->itemCond);
	npCommonPs4CondConstruct(&queue->spaceCond);
	queue->initialized = 0;
	return queue;
}

struct NpCommonPs4EventQueue *SceNpCommonPs4_3673DD2D(struct NpCommonPs4EventQueue *queue)
{
	queue->vtable = NP_COMMON_PS4_VTABLE(g_eventQueueVtable);
	npCommonPs4CondDestruct(&queue->spaceCond);
	npCommonPs4CondDestruct(&queue->itemCond);
	SceNpCommonPs4_294F0B1A(&queue->mutex);
	SceNpCommonPs4_A264C490(&queue->ringBuf);
	queue->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return queue;
}

int SceNpCommonPs4_35CDC7BE(struct NpCommonPs4EventQueue *queue)
{
	queue->vtable = NP_COMMON_PS4_VTABLE(g_eventQueueVtable);
	npCommonPs4CondDestruct(&queue->spaceCond);
	npCommonPs4CondDestruct(&queue->itemCond);
	SceNpCommonPs4_294F0B1A(&queue->mutex);
	SceNpCommonPs4_A264C490(&queue->ringBuf);
	queue->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(queue);
}

struct NpCommonPs4EventQueue *SceNpCommonPs4_3311DD67(struct NpCommonPs4EventQueue *queue)
{
	queue->vtable = NP_COMMON_PS4_VTABLE(g_eventQueueVtable);
	npCommonPs4CondDestruct(&queue->spaceCond);
	npCommonPs4CondDestruct(&queue->itemCond);
	SceNpCommonPs4_294F0B1A(&queue->mutex);
	SceNpCommonPs4_A264C490(&queue->ringBuf);
	queue->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return queue;
}

int SceNpCommonPs4_EDE54448(struct NpCommonPs4EventQueue *queue, const char *name, SceSize itemSize, SceSize itemCount)
{
	int result;

	if (queue->initialized != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_8AA7E995(&queue->mutex, name, 0);
	if (result < 0
		|| (result = npCommonPs4CondInit(&queue->spaceCond, name, &queue->mutex)) < 0
		|| (result = npCommonPs4CondInit(&queue->itemCond, name, &queue->mutex)) < 0
		|| (result = SceNpCommonPs4_530564F5(&queue->ringBuf, itemSize * itemCount)) < 0) {
		if (SceNpCommonPs4_E1F6AE0F(&queue->ringBuf) != 0)
			SceNpCommonPs4_31D856EA(&queue->ringBuf);
		if (npCommonPs4CondIsInit(&queue->spaceCond) != 0)
			npCommonPs4CondDestroy(&queue->spaceCond);
		if (npCommonPs4CondIsInit(&queue->itemCond) != 0)
			npCommonPs4CondDestroy(&queue->itemCond);
		if (SceNpCommonPs4_C78F5DF5(&queue->mutex) != 0)
			SceNpCommonPs4_63EED478(&queue->mutex);
		return result;
	}
	queue->itemSize = itemSize;
	queue->itemCount = itemCount;
	queue->closedFlags = 0;
	queue->initialized = 1;
	return 0;
}

int SceNpCommonPs4_DC7BB7F1(struct NpCommonPs4EventQueue *queue)
{
	return SceNpCommonPs4_B599A560(&queue->ringBuf.ringBuf);
}

int SceNpCommonPs4_38E19C68(struct NpCommonPs4EventQueue *queue, const char *data, SceSize size, SceUInt32 timeout)
{
	static const char functionName[] = "Enqueue";
	int result;

	if (size != queue->itemSize)
		return NP_COMMON_PS4_ERROR_80559E11;
	SceNpCommonPs4_A2A8493B(&queue->mutex);
	while ((queue->closedFlags & 1) == 0 && SceNpCommonPs4_2A607C9A(&queue->ringBuf.ringBuf) < size) {
		result = npCommonPs4CondWait(&queue->spaceCond, timeout);
		if (result < 0) {
			SceNpCommonPs4_6BA5A164(&queue->mutex);
			return result;
		}
	}
	if ((queue->closedFlags & 1) != 0) {
		SceNpCommonPs4_6BA5A164(&queue->mutex);
		return NP_COMMON_PS4_ERROR_80559E14;
	}
	NP_COMMON_PS4_ASSERT(SceNpCommonPs4_2A607C9A(&queue->ringBuf.ringBuf) >= size, functionName,
		"npext\\thread\\event_queue.cpp", 148, "m_ringbuf.GetFreeSize() >= size");
	result = SceNpCommonPs4_548EF805(&queue->ringBuf.ringBuf, data, size);
	SceNpCommonPs4_6BA5A164(&queue->mutex);
	if (result >= 0)
		return npCommonPs4CondSignal(&queue->itemCond);
	return result;
}

int SceNpCommonPs4_F634B111(struct NpCommonPs4EventQueue *queue, char *data, SceSize size, SceUInt32 timeout)
{
	static const char functionName[] = "Dequeue";
	int result;

	if (size != queue->itemSize)
		return NP_COMMON_PS4_ERROR_80559E11;
	SceNpCommonPs4_A2A8493B(&queue->mutex);
	while ((queue->closedFlags & 2) == 0 && SceNpCommonPs4_B599A560(&queue->ringBuf.ringBuf) != 0) {
		result = npCommonPs4CondWait(&queue->itemCond, timeout);
		if (result < 0) {
			SceNpCommonPs4_6BA5A164(&queue->mutex);
			return result;
		}
	}
	if ((queue->closedFlags & 2) != 0) {
		SceNpCommonPs4_6BA5A164(&queue->mutex);
		return NP_COMMON_PS4_ERROR_80559E14;
	}
	NP_COMMON_PS4_ASSERT(SceNpCommonPs4_B599A560(&queue->ringBuf.ringBuf) == 0, functionName,
		"npext\\thread\\event_queue.cpp", 196, "!m_ringbuf.IsEmpty()");
	result = SceNpCommonPs4_DC5607D0(&queue->ringBuf.ringBuf, data, size);
	SceNpCommonPs4_6BA5A164(&queue->mutex);
	if (result >= 0)
		return npCommonPs4CondSignal(&queue->spaceCond);
	return result;
}

int SceNpCommonPs4_6C3B5D0F(struct NpCommonPs4EventQueue *queue, char *data, SceSize size)
{
	struct NpCommonPs4Mutex *mutex;
	int result;
	int signaled;

	if (size != queue->itemSize)
		return NP_COMMON_PS4_ERROR_80559E11;
	mutex = &queue->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (SceNpCommonPs4_B599A560(&queue->ringBuf.ringBuf) != 0) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_COMMON_PS4_ERROR_80559E16;
	}
	result = SceNpCommonPs4_DC5607D0(&queue->ringBuf.ringBuf, data, size);
	if (result >= 0) {
		signaled = npCommonPs4CondSignal(&queue->spaceCond);
		SceNpCommonPs4_6BA5A164(mutex);
		if (signaled >= 0)
			return 0;
		return signaled;
	}
	SceNpCommonPs4_6BA5A164(&queue->mutex);
	return result;
}

int SceNpCommonPs4_3A4163FD(struct NpCommonPs4EventQueue *queue, char flags)
{
	struct NpCommonPs4Mutex *mutex;

	mutex = &queue->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if ((flags & 1) != 0) {
		queue->closedFlags |= 1;
		npCommonPs4CondBroadcast(&queue->itemCond);
	}
	if ((flags & 2) != 0) {
		queue->closedFlags |= 2;
		npCommonPs4CondBroadcast(&queue->spaceCond);
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return 0;
}

const void *g_eventQueueVtable[] = {		/* 0x8102a9f4 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_3673DD2D,
	(const void *)SceNpCommonPs4_35CDC7BE,
};

}
