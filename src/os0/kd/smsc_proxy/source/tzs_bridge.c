/*
 * TrustZone shared-memory bridge.
 *
 * Message buffers live in the sysroot shared memory: 8 request buffers at +0,
 * 8 interrupt-message buffers at +0x400 and the index of the pending interrupt
 * message at +0x800. Free buffers of each set are kept on a list. A buffer is
 * allocated with interrupts suspended and the CPU's previous interrupt state
 * saved; releasing it restores that state.
 */

#include <psp2kern/kernel/cpu.h>
#include "sce/debug.h"
#include "sce/sysroot.h"
#include "tzs_bridge.h"

#define SCE_SBL_ERROR_NO_MEMORY		0x800F040C
#define SCE_SBL_ERROR_INVALID_ARGUMENT	0x800F0416

typedef struct BridgeNode {
	struct BridgeNode *next;
	struct BridgeNode *prev;
	void *buf;
	SceUInt32 reserved;
} BridgeNode;

int ksceKernelCpuId(void);

static SceKernelSpinlock s_lock;
static BridgeNode s_intrNode[TZS_BRIDGE_MSG_COUNT];
static void *s_intrBuf;
static BridgeNode s_reqNode[TZS_BRIDGE_MSG_COUNT];
static void *s_reqBuf;
static SceUInt32 *s_intrIndex;
static BridgeNode s_intrFree;
static int s_savedIntr[4];
static BridgeNode s_reqFree;
static void *s_shm;

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

static int SetupBuffers(void)
{
	char *shm;
	int i;

	shm = ksceKernelSysrootGetSharedMemory();
	s_shm = shm;
	if (shm == NULL)
		return SCE_SBL_ERROR_NO_MEMORY;

	s_reqBuf = shm;
	s_intrBuf = shm + 0x400;
	s_intrIndex = (SceUInt32 *)(shm + 0x800);
	ListInit(&s_reqFree);
	ListInit(&s_intrFree);
	for (i = 0; i < TZS_BRIDGE_MSG_COUNT; i++) {
		s_reqNode[i].buf = shm + i * TZS_BRIDGE_MSG_SIZE;
		s_intrNode[i].buf = shm + 0x400 + i * TZS_BRIDGE_MSG_SIZE;
		ListAddTail(&s_reqFree, &s_reqNode[i]);
		ListAddTail(&s_intrFree, &s_intrNode[i]);
	}
	s_lock = 0;
	return 0;
}

int sceSblTzsBridgeInit(void)
{
	int ret = SetupBuffers();

	if (ret >= 0) {
		ksceKernelDcacheCleanRange(s_reqBuf, 0x400);
		ksceKernelDcacheCleanRange(s_intrBuf, 0x400);
		ksceKernelDcacheCleanRange(s_intrIndex, 0x40);
	}
	return ret;
}

static int AllocBuffer(void *base, SceSize size, void **buf, SceSize *bufSize)
{
	BridgeNode *head, *node;

	if (base == NULL || size > TZS_BRIDGE_MSG_SIZE || buf == NULL || bufSize == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	head = (base == s_reqBuf) ? &s_reqFree : &s_intrFree;
	node = head->next;
	if (node == head)
		return SCE_SBL_ERROR_NO_MEMORY;

	node->next->prev = node->prev;
	node->prev->next = node->next;
	node->next = node;
	node->prev = node;

	*buf = node->buf;
	*bufSize = size;
	ksceKernelDcacheInvalidateRange(node->buf, TZS_BRIDGE_MSG_SIZE);
	return ((char *)node->buf - (char *)base) >> 7;
}

int sceSblTzsBridgeAllocArgArea(SceSize size, void **buf, SceSize *bufSize)
{
	int state, ret;

	if (s_shm == NULL && (ret = SetupBuffers()) < 0)
		return ret;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_lock);
	s_savedIntr[ksceKernelCpuId()] = state;
	ret = AllocBuffer(s_reqBuf, size, buf, bufSize);
	if (ret >= 0) {
		/* interrupts stay suspended until sceSblTzsBridgeFreeArgArea */
		ksceKernelSpinlockLowUnlock(&s_lock);
		return ret;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, state);
	return ret;
}

int sceSblTzsBridgePush(void *base, int index)
{
	if (s_shm == NULL) {
		SCE_DBG_ASSERT(0, 338);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	if (base == NULL || (unsigned int)index >= TZS_BRIDGE_MSG_COUNT)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	ksceKernelDcacheCleanRange((char *)base + index * TZS_BRIDGE_MSG_SIZE, TZS_BRIDGE_MSG_SIZE);
	return 0;
}

int sceSblTzsBridgePushArgArea(int index)
{
	return sceSblTzsBridgePush(s_reqBuf, index);
}

static int GetBuffer(void *base, int index, void **buf, SceSize *bufSize)
{
	if (base == NULL || (unsigned int)index >= TZS_BRIDGE_MSG_COUNT ||
	    buf == NULL || bufSize == NULL)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	*buf = (char *)base + index * TZS_BRIDGE_MSG_SIZE;
	*bufSize = TZS_BRIDGE_MSG_SIZE;
	ksceKernelDcacheInvalidateRange(*buf, TZS_BRIDGE_MSG_SIZE);
	return 0;
}

int sceSblTzsBridgePopArgArea(int index, void **buf, SceSize *bufSize)
{
	int ret;

	if (s_shm == NULL && (ret = SetupBuffers()) < 0)
		return ret;
	return GetBuffer(s_reqBuf, index, buf, bufSize);
}

int sceSblTzsBridgePopIntrArea(void **buf, SceSize *bufSize)
{
	int ret;

	if (s_shm == NULL && (ret = SetupBuffers()) < 0)
		return ret;
	ksceKernelDcacheInvalidateRange(s_intrIndex, TZS_BRIDGE_MSG_SIZE);
	return GetBuffer(s_intrBuf, *s_intrIndex, buf, bufSize);
}

int sceSblTzsBridgeFree(void *base, int index)
{
	BridgeNode *head, *nodes;

	if (s_shm == NULL) {
		SCE_DBG_ASSERT(0, 411);
		return SCE_SBL_ERROR_NO_MEMORY;
	}
	if (base == NULL || (unsigned int)index >= TZS_BRIDGE_MSG_COUNT)
		return SCE_SBL_ERROR_INVALID_ARGUMENT;

	head = (base == s_reqBuf) ? &s_reqFree : &s_intrFree;
	nodes = (base == s_reqBuf) ? s_reqNode : s_intrNode;
	ksceKernelDcacheCleanRange((char *)base + index * TZS_BRIDGE_MSG_SIZE, TZS_BRIDGE_MSG_SIZE);
	ListAddTail(head, &nodes[index]);
	return 0;
}

int sceSblTzsBridgeFreeArgArea(int index)
{
	int ret;

	ksceKernelSpinlockLowLock(&s_lock);
	ret = sceSblTzsBridgeFree(s_reqBuf, index);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_lock, s_savedIntr[ksceKernelCpuId()]);
	return ret;
}
