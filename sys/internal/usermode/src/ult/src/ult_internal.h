/*
 * ult_internal.h: what Cinterface.cpp and the library's other files share: the objects
 * behind each of the SDK's types and the library's own entry points.
 */
#ifndef ULT_INTERNAL_H
#define ULT_INTERNAL_H

#include "CondWaiterQ.h"

extern "C" {
#include <ulobjmgr/ulobjmgr.h>
#include <fiber/fiber.h>
}

/* the start of every object, as the kernel's ulobjmgr reads it (its UlobjSyncObjInfoHeader) */
#define ULT_OBJECT_VERSION	0x100

enum {
	ULT_OBJECT_TYPE_MUTEX = 1,
	ULT_OBJECT_TYPE_CONDITION_VARIABLE = 2,
	ULT_OBJECT_TYPE_SEMAPHORE = 3,
	ULT_OBJECT_TYPE_READER_WRITER_LOCK = 4,
	ULT_OBJECT_TYPE_QUEUE = 5,
	ULT_OBJECT_TYPE_WAITING_QUEUE_RESOURCE_POOL = 6,
	ULT_OBJECT_TYPE_QUEUE_DATA_RESOURCE_POOL = 7
};

struct UltObjectHeader {
	char name[SCE_ULT_MAX_NAME_LENGTH + 1];
	uint16_t version;		/* ULT_OBJECT_VERSION */
	uint8_t type;			/* ULT_OBJECT_TYPE_* */
	uint8_t reserved;
};

/* the debugger's registration of an object (SceUlobjDbg) */
#define ULOBJ_DBG_KIND_SYNC_OBJECT	2

/* SceUltWaitingQueueResourcePool */
struct WaitingQueueResourcePoolObject {
	UltObjectHeader header;
	uint32_t numThreads;
	uint32_t numSyncObjects;
	WaitingQueueResourcePoolInternal internal;
	uint32_t ulobjId;
};

/* the pool an object's waiters take their entries from, if any */
static inline WaitingQueueResourcePoolInternal *
internalOf(SceUltWaitingQueueResourcePool *pool)
{
	if (pool == NULL)
		return NULL;
	return &((WaitingQueueResourcePoolObject *)pool)->internal;
}

/* every option parameter starts zeroed, with its header */
template <class T> static inline int initializeOptParam(T *optParam, uint32_t buildVersion)
{
	if (optParam == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(optParam))
		return SCE_ULT_ERROR_ALIGNMENT;
	sceClibMemset(optParam, 0, sizeof(*optParam));
	return initializeOptParamHeader((OptParamHeader *)optParam, buildVersion);
}

/* an option parameter given: aligned, initialized, and nothing in its reserved words */
template <class T> static inline int checkOptParam(const T *optParam)
{
	if (!isAligned8(optParam))
		return SCE_ULT_ERROR_ALIGNMENT;
	int ret = checkOptParamHeader((const OptParamHeader *)optParam);
	if (ret != SCE_OK)
		return ret;
	for (uint32_t i = 0; i < sizeof(optParam->__reserved__) / sizeof(uint64_t); i++) {
		if (optParam->__reserved__[i] != 0)
			return SCE_ULT_ERROR_INVALID;
	}
	return SCE_OK;
}

/* CinterfaceInternal.cpp: what the module's start and stop do beyond the kernel's revision */
void initializeInternal();
void finalizeInternal();

/* an ulthread runtime's defaults (_sceUltUlthreadRuntimeOptParamInitialize) */
#define ULT_DEFAULT_ONE_SHOT_THREAD_STACK_SIZE		4096
#define ULT_DEFAULT_WORKER_THREAD_PRIORITY		0x10000100	/* SCE_KERNEL_DEFAULT_PRIORITY_USER */
#define ULT_DEFAULT_WORKER_THREAD_CPU_AFFINITY_MASK	0x00070000	/* any user CPU */

#endif
