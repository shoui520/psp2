/*
 * ult_util.cpp: the helpers every object uses (ult_util.h): alignment, option parameters'
 * headers, names and the descriptions of waiting threads. Sony's file between Queue.cpp and
 * Ulthread.cpp has no name in the binaries; it is named here after its header.
 */
#include "Waiter.h"
#include "FunctorNBQueue.h"

bool isAligned8(const void *p)
{
	return ((uintptr_t)p & 7) == 0;
}

int initializeOptParamHeader(OptParamHeader *header, uint32_t buildVersion)
{
	header->buildVersion = buildVersion;
	header->magic[0] = ULT_OPT_PARAM_MAGIC0;
	header->magic[1] = ULT_OPT_PARAM_MAGIC1;
	header->magic[2] = ULT_OPT_PARAM_MAGIC2;
	return SCE_OK;
}

int checkOptParamHeader(const OptParamHeader *header)
{
	if (header->magic[0] == ULT_OPT_PARAM_MAGIC0 && header->magic[1] == ULT_OPT_PARAM_MAGIC1 &&
	    header->magic[2] == ULT_OPT_PARAM_MAGIC2)
		return SCE_OK;
	return SCE_ULT_ERROR_INVALID;
}

int setName(char *dst, const char *name)
{
	if (name == NULL)
		return SCE_ULT_ERROR_NULL;
	sceClibStrncpy(dst, name, SCE_ULT_MAX_NAME_LENGTH);
	dst[SCE_ULT_MAX_NAME_LENGTH] = '\0';
	return SCE_OK;
}

int getName(const char *name, char *dst)
{
	if (dst == NULL)
		return SCE_ULT_ERROR_NULL;
	sceClibStrncpy(dst, name, SCE_ULT_MAX_NAME_LENGTH);
	dst[SCE_ULT_MAX_NAME_LENGTH] = '\0';
	return SCE_OK;
}

/* the waiting entries are 8-byte aligned: the low 3 bits of an address word are flags */
static inline NBQueueEntry2 *waitEntry(uint32_t address)
{
	return (NBQueueEntry2 *)(address & ~7u);
}

static inline bool isWaitEntry(const NBQueueEntry2 *e)
{
	return e != NULL && e != (NBQueueEntry2 *)~7u;
}

static inline void describeWaiter(SceUltWaitThreadInfo *info, int32_t operation,
				  const NBQueueEntry2 *e)
{
	sceClibMemset(info, 0, sizeof(*info));
	info->operation = operation;
	info->size = sizeof(*info);
	uint32_t id = ((const Waiter *)e->data)->m_id;
	if (!isKernelThread(id)) {
		info->threadType = SCE_ULT_THREAD_TYPE_ULTHREAD;
		info->threadId = ((const AbstractThread *)id)->id;
	} else {
		info->threadType = SCE_ULT_THREAD_TYPE_KERNEL_THREAD;
		info->threadId = id >> 1;
	}
}

uint32_t getWaitThreadInfo(uint32_t head, SceUltWaitThreadInfo *list, uint32_t max)
{
	if (head == 0)
		return 0;
	NBQueueEntry2 *e = waitEntry(head);
	if (!isWaitEntry(e))
		return 0;

	uint32_t n = 0;
	NBQueueID next = e->mNext;
	int32_t operation = idCount(next);
	NBQueueEntry2 *p = waitEntry(idAddress(next));
	for (SceUltWaitThreadInfo *info = list; p != NULL && p != (NBQueueEntry2 *)~7u; n++) {
		if (list != NULL && n < max)
			describeWaiter(info, operation, p);
		info++;
		next = p->mNext;
		operation = idCount(next);
		p = waitEntry(idAddress(next));
	}
	return n;
}

int32_t getWaitThreadInfoByCause(uint32_t head, SceUltWaitThreadInfo *list, uint32_t max,
				 const void *cause)
{
	if (head == 0)
		return 0;
	NBQueueEntry2 *e = waitEntry(head);
	if (!isWaitEntry(e))
		return 0;

	uint32_t n = 0;
	NBQueueID next = e->mNext;
	int32_t operation = idCount(next);
	NBQueueEntry2 *p = waitEntry(idAddress(next));
	for (SceUltWaitThreadInfo *info = list; p != NULL && p != (NBQueueEntry2 *)~7u;
	     p = waitEntry(idAddress(p->mNext))) {
		if (((const Waiter *)p->data)->m_cause == cause) {
			if (list != NULL && n < max)
				describeWaiter(info, operation, p);
			info++;
			n++;
		}
		operation = idCount(p->mNext);
	}
	return n;
}

int checkWaitThreadInfoList(const SceUltWaitThreadInfo *list, uint32_t max)
{
	if (list == NULL)
		return SCE_OK;
	for (uint32_t i = 0; i < max; i++)
		if (list[i].size != sizeof(list[i]))
			return SCE_ULT_ERROR_INVALID;
	return SCE_OK;
}
