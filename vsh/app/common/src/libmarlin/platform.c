/*
 * The library's platform mode (which PSN environment a debug console names in the
 * registry), the reference-counted setup of its two heaps, and the frees of two buffer
 * groups. The file's name is ours.
 */
#include <regmgr/regmgr.h>
#include "mln_internal.h"

#define MLN_ERROR_REGISTRY	5

/* The registry entry naming the PSN environment this console is set up for. */
#define MLN_REG_NP_ENVIRONMENT	210968

typedef struct MlnMemoryUse {
	int refCount;
	int ready;
} MlnMemoryUse;

static int s_platformMode;
static void *s_sharedBuffers[12];
MlnMemoryUse s_memoryUse[2];

/* The environments a debug console may name: each selects the developer platform. */
static const char *const s_npEnvironments[] = {
	"np", "mgmt", "sp-int", "prod-qa",
	"h1-np", "h1-pmgt", "h1-spint", "h1-pqa",
	"p1-np", "p1-pmgt", "p1-spint", "p1-pqa",
	"hf-np", "hf-pmgt", "hf-spint", "hf-pqa",
	NULL,
};


void MlnImpl_81006CD4(int heap, MlnBuffer **buffers)
{
	if (buffers == NULL)
		return;
	MlnBufferDestroy(heap, buffers[0]);
	MlnBufferDestroy(heap, buffers[1]);
	MlnBufferDestroy(heap, buffers[2]);
	MlnFree(heap, buffers);
}

void MlnImpl_81006D14(int heap, MlnBufferGroup *group)
{
	unsigned int i;
	MlnBuffer **entry;

	if (group == NULL)
		return;
	MlnBufferDestroy(heap, group->first);
	MlnBufferDestroy(heap, group->second);
	MlnBufferDestroy(heap, group->third);
	if (group->entries != NULL) {
		for (i = 0; i < group->entryCount; i++) {
			entry = group->entries[i];
			if (entry != NULL) {
				MlnBufferDestroy(heap, entry[0]);
				MlnBufferDestroy(heap, entry[1]);
				MlnBufferDestroy(heap, entry[2]);
				MlnFree(heap, entry);
			}
		}
		MlnFree(heap, group->entries);
	}
	MlnFree(heap, group);
}

int MlnPlatformMode(void)
{
	return s_platformMode;
}

static __attribute__((noinline)) int MlnPlatformModeInit(void)
{
	const char *const *name;
	const char *environmentName;
	char environment[17];
	unsigned int length;
	int ret;

	if (sceDrmBridgeIsAllowRemotePlayDebug() != 0 ||
	    (ret = SceDrmBridgeUser_6F5DC6CD()) != 0) {
		MlnMemset(environment, 0, sizeof(environment));
		if (sceRegMgrSystemParamGetStr(MLN_REG_NP_ENVIRONMENT, environment,
		    sizeof(environment)) != 0)
			return MLN_ERROR_REGISTRY;
		name = s_npEnvironments;
		environmentName = *name;
		for (;;) {
			length = MlnStrlen(environmentName);
			if (length == MlnStrlen(environment) &&
			    (ret = MlnStrcmp(environmentName, environment)) == 0)
				break;
			environmentName = *++name;
			if (environmentName == NULL) {
				s_platformMode = 2;
				return MLN_OK;
			}
		}
	}
	s_platformMode = 1;
	return ret;
}

void **MlnImpl_81006EAC(void)
{
	return s_sharedBuffers;
}

int MlnMemoryInit(int heap, void *memory, unsigned int size)
{
	MlnMemoryUse *use = heap ? &s_memoryUse[1] : &s_memoryUse[0];
	int ret;

	if (use->refCount++ != 0)
		return MLN_OK;
	if (memory == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	ret = MlnHeapInit(heap, memory, size);
	if (ret != MLN_OK)
		goto fail;
	ret = MlnPlatformModeInit();
	if (ret != MLN_OK)
		goto fail;
	return MLN_OK;

fail:
	use->refCount--;
	MlnHeapTerm(heap);
	use->ready = 0;
	return ret;
}

int MlnMemoryTerm(int heap)
{
	MlnMemoryUse *use = heap ? &s_memoryUse[1] : &s_memoryUse[0];

	if (use->refCount <= 0)
		return 1;
	if (--use->refCount != 0)
		return MLN_OK;
	if (heap == MLN_HEAP_GENERAL) {
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[0]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[1]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[2]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[3]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[4]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[5]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[6]);
		MlnFree(MLN_HEAP_GENERAL, s_sharedBuffers[7]);
		MlnMemset(s_sharedBuffers, 0, sizeof(s_sharedBuffers));
	}
	MlnHeapTerm(heap);
	use->ready = 0;
	s_platformMode = 0;
	return MLN_OK;
}
