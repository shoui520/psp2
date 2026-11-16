/*
 * Queue.cpp: the queue of Sony's first design (ImplQueue), with its own work area.
 */
#include "Queue.h"

#define QUEUE_CPP_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Queue.cpp", line, text, cond)
/* where Sony's build calls the check out of line */
#define QUEUE_CPP_ASSERT_OUT_OF_LINE(line, text, cond) \
	ULT_ASSERT_OUT_OF_LINE_AT("Queue.cpp", line, text, cond)
#define QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_OUT_OF_LINE_AT("Queue.cpp", line, text, actual, expected)

int ImplQueue::initialize(const char *name, uint32_t queueDepth, uint32_t dataSize,
			  uint32_t waitingQueueLength, void *workArea,
			  const SceUltQueueOptParam *optParam, uint32_t buildVersion)
{
	SceUltQueueOptParam defaultOptParam;
	int ret;

	if (this == NULL || workArea == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this) || !isAligned8(workArea))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (queueDepth == 0 || dataSize == 0)
		return SCE_ULT_ERROR_INVALID;
	QUEUE_CPP_ASSERT(37, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
	if (optParam == NULL) {
		initializeOptParam(&defaultOptParam, ULT_VERSION);
		optParam = &defaultOptParam;
	} else {
		ret = checkOptParam(optParam);
		if (ret != SCE_OK)
			return ret;
	}
	ret = setName(this->name, name);
	if (ret != SCE_OK)
		return ret;
	this->queueDepth = queueDepth;
	this->waitingQueueLength = waitingQueueLength;
	this->dataSize = dataSize;
	if (queueDepth == 0 || dataSize == 0)
		return SCE_ULT_ERROR_INVALID;

	NBQueueEntry2 *dummies = (NBQueueEntry2 *)workArea;
	NBQueueEntry2 *waiterEntries = dummies + 3;
	NBQueueEntry2 *dataEntries = waiterEntries + waitingQueueLength;
	intptr_t ibuffer = (intptr_t)(dataEntries + queueDepth);

	this->workArea = workArea;
	dataBuffer = (void *)ibuffer;
	ibuffer += dataSize * queueDepth;
	QUEUE_CPP_ASSERT(84, "(ibuffer - (intptr_t)workArea) == (intptr_t)getWorkAreaSize(queueDepth,dataSize,waitingQueueLength)",
			 (ibuffer - (intptr_t)workArea) ==
			 (intptr_t)getWorkAreaSize(queueDepth, dataSize, waitingQueueLength));
	sceClibMemset(workArea, 0,
		      sizeof(NBQueueEntry2) * (queueDepth + waitingQueueLength) +
		      3 * sizeof(NBQueueEntry2));

	m_waiterEntryQueue.initialize(&dummies[0]);
	for (uint32_t i = 0; i < waitingQueueLength; i++) {
		waiterEntries[i].data = 0;
		m_waiterEntryQueue.push(&waiterEntries[i]);
	}

	/* every buffer is free */
	char *buffer = (char *)dataBuffer;
	m_freeQueue.initialize(&dummies[1]);
	for (uint32_t i = 0; i < queueDepth; i++) {
		dataEntries[i].data = (uint32_t)buffer;
		__builtin_dmb();
		m_freeQueue.pushOrPopWaiter(&dataEntries[i]);
		buffer += dataSize;
	}

	m_dataQueue.initialize(&dummies[2]);
	__builtin_dmb();
	return SCE_OK;
}

int ImplQueue::push(const void *data)
{
	Waiter waiter;

	if (!AbstractThread::canWaitOutOfLine())
		return SCE_ULT_ERROR_PERMISSION;
	QUEUE_CPP_ASSERT_OUT_OF_LINE(113, "dataBuffer != NULL", dataBuffer != NULL);
	int ret = tryPush(data);
	if (ret != SCE_ULT_ERROR_AGAIN)
		return ret;

	/* no free buffer: wait for one */
	if (waitingQueueLength == 0)
		return SCE_ULT_ERROR_BUSY;
	NBQueueEntry2 *waiterEntry = m_waiterEntryQueue.pop();
	if (waiterEntry == NULL)
		return SCE_ULT_ERROR_BUSY;
	ret = waiter.initializeOutOfLine(this);
	QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(131, "ret==0", ret, 0);
	waiterEntry->data = (uint32_t)&waiter;
	__builtin_dmb();
	NBQueueEntry2 *entry = m_freeQueue.popOrPushWaiter(waiterEntry);
	if (entry != NULL) {
		/* one was freed meanwhile */
		waiterEntry->data = 0;
		__builtin_dmb();
		ret = m_waiterEntryQueue.push(waiterEntry);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(145, "ret==0", ret, 0);
	} else {
		entry = (NBQueueEntry2 *)waiter.waitOutOfLine(this);
	}
	waiter.finalizeOutOfLine();
	__builtin_dmb();
	QUEUE_CPP_ASSERT_OUT_OF_LINE(152, "dataP->getData() != NULL", entry->data != 0);
	sceClibMemcpy_safe((void *)entry->data, data, dataSize);
	__builtin_dmb();

	NBQueueEntry2 *popperEntry = m_dataQueue.pushOrPopWaiter(entry);
	if (popperEntry != NULL) {
		/* a pop waits: the datum is handed to it */
		Waiter *popper = (Waiter *)popperEntry->data;
		popperEntry->data = 0;
		__builtin_dmb();
		ret = m_waiterEntryQueue.push(popperEntry);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(170, "ret==0", ret, 0);
		__builtin_dmb();
		ret = popper->signalOutOfLine(entry, this);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(173, "ret==0", ret, 0);
	}
	__builtin_dmb();
	return SCE_OK;
}

int ImplQueue::pop(void *data)
{
	Waiter waiter;

	if (!AbstractThread::canWaitOutOfLine())
		return SCE_ULT_ERROR_PERMISSION;
	int ret = tryPop(data);
	if (ret != SCE_ULT_ERROR_AGAIN)
		return ret;

	/* no data: wait for some */
	NBQueueEntry2 *waiterEntry = m_waiterEntryQueue.pop();
	if (waiterEntry == NULL)
		return SCE_ULT_ERROR_BUSY;
	__builtin_dmb();
	ret = waiter.initializeOutOfLine(this);
	QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(204, "ret==0", ret, 0);
	waiterEntry->data = (uint32_t)&waiter;
	__builtin_dmb();
	NBQueueEntry2 *entry = m_dataQueue.popOrPushWaiter(waiterEntry);
	if (entry != NULL) {
		/* one was pushed meanwhile */
		waiterEntry->data = 0;
		__builtin_dmb();
		ret = m_waiterEntryQueue.push(waiterEntry);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(219, "ret==0", ret, 0);
	} else {
		entry = (NBQueueEntry2 *)waiter.waitOutOfLine(this);
	}
	waiter.finalizeOutOfLine();
	__builtin_dmb();
	QUEUE_CPP_ASSERT_OUT_OF_LINE(226, "dataP->getData() != NULL", entry->data != 0);
	sceClibMemcpy_safe(data, (const void *)entry->data, dataSize);
	__builtin_dmb();

	NBQueueEntry2 *pusherEntry = m_freeQueue.pushOrPopWaiter(entry);
	if (pusherEntry != NULL) {
		/* a push waits: the buffer is handed to it */
		Waiter *pusher = (Waiter *)pusherEntry->data;
		pusherEntry->data = 0;
		__builtin_dmb();
		ret = m_waiterEntryQueue.push(pusherEntry);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(241, "ret==0", ret, 0);
		__builtin_dmb();
		ret = pusher->signalOutOfLine(entry, this);
		QUEUE_CPP_ASSERT_EQUAL_OUT_OF_LINE(244, "ret==0", ret, 0);
	}
	__builtin_dmb();
	return SCE_OK;
}
