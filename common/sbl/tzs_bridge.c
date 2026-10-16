/*
 * tzs_bridge.c: the bridge areas (sce/tzs_bridge.h), Sony's one source for
 * both sides. A module's own tzs_bridge.c includes it after declaring the
 * imports under the Non-secure kernel's names, with TZS_BRIDGE_SECURE set
 * to 1 in the secure world and 0 in the Non-secure kernel.
 *
 * Free areas are kept on a list per set. The Non-secure kernel holds its
 * lock with interrupts suspended from sceSblTzsBridgeAllocArgArea until
 * sceSblTzsBridgeFreeArgArea, the call in between.
 */
#include <stddef.h>
#include "sce/debug.h"
#include "sce/tzs_bridge.h"

#ifndef SCE_SBL_ERROR_NO_MEMORY
#define SCE_SBL_ERROR_NO_MEMORY		((int)0x800f040c)
#define SCE_SBL_ERROR_INVALID_ARGUMENT	((int)0x800f0416)
#endif

/* name unknown: a free list entry for one area */
typedef struct BridgeNode {
	struct BridgeNode *next;
	struct BridgeNode *prev;
	void *area;
	SceUInt32 reserved;
} BridgeNode;

static SceKernelSpinlock s_listLock;
static BridgeNode s_intrNode[TZS_BRIDGE_AREAS];
static void *s_intrArea;
static BridgeNode s_argNode[TZS_BRIDGE_AREAS];
static void *s_argArea;
static SceUInt32 *s_intrIndex;
static BridgeNode s_intrFree;
#if !TZS_BRIDGE_SECURE
static int s_savedIntr[4];		/* by CPU, while an argument area is held */
#endif
static BridgeNode s_argFree;
static SceSblTzsBridgeMemory *s_memory;

static inline void ListInit(BridgeNode *head)
{
	head->next = head;
	head->prev = head;
}

static inline void ListAddTail(BridgeNode *head, BridgeNode *node)
{
	BridgeNode *tail = head->prev;

	node->next = head;
	head->prev = node;
	node->prev = tail;
	tail->next = node;
}

int sceSblTzsBridgeInit(void)
{
	SceSblTzsBridgeMemory *memory;
	int i;

	memory = ksceKernelSysrootGetSharedMemory();
	s_memory = memory;
	if (memory == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;
	s_argArea = memory->argArea;
	s_intrArea = memory->intrArea;
	s_intrIndex = &memory->intrIndex;
	ListInit(&s_argFree);
	ListInit(&s_intrFree);
	for (i = 0; i < TZS_BRIDGE_AREAS; i++) {
		s_argNode[i].area = (char *)memory + i * TZS_BRIDGE_AREA_SIZE;
		s_intrNode[i].area = (char *)memory + offsetof(SceSblTzsBridgeMemory, intrArea) +
				     i * TZS_BRIDGE_AREA_SIZE;
		ListAddTail(&s_argFree, &s_argNode[i]);
		ListAddTail(&s_intrFree, &s_intrNode[i]);
	}
	s_listLock = 0;
	return 0;
}

#if !TZS_BRIDGE_SECURE
int sceSblTzsBridgeStart(void)
{
	int ret = sceSblTzsBridgeInit();

	if (ret >= 0) {
		ksceKernelDcacheCleanRange(s_argArea, sizeof(s_memory->argArea));
		ksceKernelDcacheCleanRange(s_intrArea, sizeof(s_memory->intrArea));
		ksceKernelDcacheCleanRange(s_intrIndex, 0x40);
	}
	return ret;
}
#endif

/* a free area of a set taken; its index returned */
int sceSblTzsBridgeAlloc(void *areas, SceSize size, void **area, SceSize *areaSize)
{
	BridgeNode *head, *node;
	void *nodeArea;

	if (areas == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (size > TZS_BRIDGE_AREA_SIZE)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (area == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (areaSize == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	head = (areas == s_argArea) ? &s_argFree : &s_intrFree;
	node = head->next;
	if (node == head)
		return SCE_SBL_ERROR_NO_MEMORY;
	nodeArea = node->area;
	node->next->prev = node->prev;
	node->prev->next = node->next;
	node->next = node;
	node->prev = node;
	*area = nodeArea;
	*areaSize = size;
	ksceKernelDcacheInvalidateRange(nodeArea, TZS_BRIDGE_AREA_SIZE);
	return ((char *)node->area - (char *)areas) >> 7;
}

#if TZS_BRIDGE_SECURE
int sceSblTzsBridgeAllocIntrArea(SceSize size, void **area, SceSize *areaSize)
{
	int ret;

	if (s_memory == NULL && (ret = sceSblTzsBridgeInit()) < 0)
		return ret;
	ksceKernelSpinlockLowLock(&s_listLock);
	ret = sceSblTzsBridgeAlloc(s_intrArea, size, area, areaSize);
	ksceKernelSpinlockLowUnlock(&s_listLock);
	return ret;
}
#else
int sceSblTzsBridgeAllocArgArea(SceSize size, void **area, SceSize *areaSize)
{
	int intr, ret;

	if (s_memory == NULL && (ret = sceSblTzsBridgeInit()) < 0)
		return ret;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_listLock);
	s_savedIntr[ksceKernelCpuId()] = intr;
	ret = sceSblTzsBridgeAlloc(s_argArea, size, area, areaSize);
	if (ret >= 0) {
		/* interrupts stay suspended until sceSblTzsBridgeFreeArgArea */
		ksceKernelSpinlockLowUnlock(&s_listLock);
		return ret;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_listLock, intr);
	return ret;
}
#endif

/* an area written for the other side: made visible to it */
int sceSblTzsBridgePush(void *areas, int index)
{
	if (s_memory == NULL) {
		SCE_DBG_ASSERT(0, 338);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	if (areas == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if ((unsigned int)index >= TZS_BRIDGE_AREAS)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	ksceKernelDcacheCleanRange((char *)areas + index * TZS_BRIDGE_AREA_SIZE,
				   TZS_BRIDGE_AREA_SIZE);
	return 0;
}

int sceSblTzsBridgePushArgArea(int index)
{
	return sceSblTzsBridgePush(s_argArea, index);
}

#if TZS_BRIDGE_SECURE
int sceSblTzsBridgePushIntrArea(int index)
{
	*s_intrIndex = index;
	ksceKernelDcacheCleanRange(s_intrIndex, TZS_BRIDGE_AREA_SIZE);
	return sceSblTzsBridgePush(s_intrArea, index);
}
#endif

/* an area the other side wrote: read from memory */
int sceSblTzsBridgePop(void *areas, int index, void **area, SceSize *areaSize)
{
	if (areas == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if ((unsigned int)index >= TZS_BRIDGE_AREAS)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (area == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if (areaSize == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	*area = (char *)areas + index * TZS_BRIDGE_AREA_SIZE;
	*areaSize = TZS_BRIDGE_AREA_SIZE;
	ksceKernelDcacheInvalidateRange(*area, TZS_BRIDGE_AREA_SIZE);
	return 0;
}

int sceSblTzsBridgePopArgArea(int index, void **area, SceSize *areaSize)
{
	int ret;

	if (s_memory == NULL && (ret = sceSblTzsBridgeInit()) < 0)
		return ret;
	return sceSblTzsBridgePop(s_argArea, index, area, areaSize);
}

#if !TZS_BRIDGE_SECURE
int sceSblTzsBridgePopIntrArea(void **area, SceSize *areaSize)
{
	int ret;

	if (s_memory == NULL && (ret = sceSblTzsBridgeInit()) < 0)
		return ret;
	ksceKernelDcacheInvalidateRange(s_intrIndex, TZS_BRIDGE_AREA_SIZE);
	return sceSblTzsBridgePop(s_intrArea, *s_intrIndex, area, areaSize);
}
#endif

/* an area back on its set's free list */
int sceSblTzsBridgeFree(void *areas, int index)
{
	BridgeNode *head, *nodes;

	if (s_memory == NULL) {
		SCE_DBG_ASSERT(0, 411);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	if (areas == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	if ((unsigned int)index >= TZS_BRIDGE_AREAS)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	head = (areas == s_argArea) ? &s_argFree : &s_intrFree;
	nodes = (areas == s_argArea) ? s_argNode : s_intrNode;
	ksceKernelDcacheCleanRange((char *)areas + index * TZS_BRIDGE_AREA_SIZE,
				   TZS_BRIDGE_AREA_SIZE);
	ListAddTail(head, &nodes[index]);
	return 0;
}

#if TZS_BRIDGE_SECURE
int sceSblTzsBridgeFreeIntrArea(int index)
{
	int ret;

	ksceKernelSpinlockLowLock(&s_listLock);
	ret = sceSblTzsBridgeFree(s_intrArea, index);
	ksceKernelSpinlockLowUnlock(&s_listLock);
	return ret;
}
#else
int sceSblTzsBridgeFreeArgArea(int index)
{
	int ret;

	ksceKernelSpinlockLowLock(&s_listLock);
	ret = sceSblTzsBridgeFree(s_argArea, index);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_listLock, s_savedIntr[ksceKernelCpuId()]);
	return ret;
}
#endif
