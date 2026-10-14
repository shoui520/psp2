/*
 * SceSblSsSmComm: ARM-side secure-module sessions and command exchange.
 * Private function names are descriptive; the originals contain no debug names.
 * PLACEHOLDER: fast-mutex internals and secure-world command meanings are unknown.
 */
#include <stdint.h>
#include <stddef.h>
#include <sce/sm_comm.h>
#include <sce/suspend.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include "sce/iofilemgr.h"
#include "sce/sysmem.h"
#include "sce/sysroot.h"
#include "sce/barrier.h"
#include "sce/syscall.h"

#define SM_ERROR_START		((int)0x800f0002)
#define SM_ERROR_CHANNEL	((int)0x800f0001)
#define SM_ERROR_TRANSFER	((int)0x800f0005)
#define SM_ERROR_VECTOR		((int)0x800f0007)
#define SM_ERROR_BUFFER		((int)0x800f000c)
#define SM_ERROR_ALIGNMENT	((int)0x800f000e)
#define SM_ERROR_REQUEST	((int)0x800f0016)
#define SM_ERROR_SLOTS		((int)0x800f0018)
#define SM_ERROR_SESSION	((int)0x800f0026)
#define SM_ERROR_STOPPED		((int)0x800f0429)
#define SM_BUFFER_SIZE		0x1000U
#define SM_PAYLOAD_OFFSET	0x40U
#define SM_MAX_IMAGE_SIZE	0x20000U
#define SM_MEMBLOCK_TYPE	0x10208006U
#define NOINLINE		__attribute__((noinline))

/* Module-local declarations while the shared dependency headers are frozen.
 * These are the same contracts used by the verified SMSC Proxy and Sysmem.
 */
typedef void (*SmCry2ArmHandler)(SceUID id, int channel, void *argument,
	uint32_t status, uint32_t result);
int ksceSblSmSchedProxyInvoke(uint32_t priority, uint32_t physicalVector,
	uint32_t count, const uint32_t *commandInput, const SceAuthInfo *auth, SceUID *id);
int ksceSblSmSchedProxyWait(SceUID id, void *result);
int ksceSblSmSchedCallFunc(SceUID id, int channel, uint32_t value);
int ksceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, uint32_t value);
int ksceSblSmSchedProxyReadCry2Arm(SceUID id, int channel, uint32_t *value);
int ksceSblSmSchedProxyEnableCry2ArmInterrupt(SceUID id, int channel,
	SmCry2ArmHandler handler, void *argument);

struct SmSession {
	volatile SceUID id;
	volatile int result;
	volatile SceUID memBlock;
	volatile SceUID eventFlag;
};

static SceUID s_semaphore;
static volatile uint64_t s_usedSlots;
static struct SmSession s_sessions[32];
static SceKernelFastMutex s_fastMutex;

static inline uint32_t EnterApi(void)
{
	uint32_t context = sceSyscallEnter();

	SceKernelSuspendForDriver_4DF40893(0);
	return context;
}

static inline void LeaveApi(uint32_t context)
{
	SceKernelSuspendForDriver_2BB92967(0);
	sceSyscallLeave(context);
}

/* Retain the discarded read between the original DMB and DSB. */
static inline void PublishWord(volatile uint32_t *word)
{
	dmb();
	(void)*word;
	dsb();
}

static int NOINLINE AllocateSessionSlot(void)
{
	uint64_t used, bit;
	int index;

	if (ksceKernelLockFastMutex(&s_fastMutex) != 0)
		return -1;
	used = s_usedSlots;
	for (index = 0; index < 32; index++) {
		bit = UINT64_C(1) << index;
		if (used & bit)
			continue;
		s_usedSlots = used | bit;
		if (ksceKernelUnlockFastMutex(&s_fastMutex) != 0)
			return -1;
		return index;
	}
	ksceKernelUnlockFastMutex(&s_fastMutex);
	return -1;
}

static int NOINLINE ReleaseSessionSlot(int index)
{
	uint64_t used;

	if (index >= 32 || ksceKernelLockFastMutex(&s_fastMutex) != 0)
		return -1;
	used = s_usedSlots & ~(UINT64_C(1) << index);
	s_sessions[index].id = 0;
	s_usedSlots = used;
	return ksceKernelUnlockFastMutex(&s_fastMutex) != 0 ? -1 : 0;
}

static int FindSessionSlot(SceUID id)
{
	uint64_t used = s_usedSlots;
	int index;

	for (index = 0; index < 32; index++) {
		if ((used & (UINT64_C(1) << index)) && s_sessions[index].id == id)
			return index;
	}
	return -1;
}

/* 0.990 names it in its cry2arm messages. SMSC Proxy supplies five words. */
static void NOINLINE sceSblSmCommIntHandler(SceUID id, int channel, void *argument,
	uint32_t status, uint32_t result)
{
	uint32_t value = 0, eventBits;
	int index = FindSessionSlot(id);

	(void)argument;
	if (channel == 0) {
		if (index < 0)
			goto out;
		if ((status & ~8U) == 2) {
			result = 0;
			eventBits = 1;
		} else {
			eventBits = 0x10;
		}
	} else if (channel == 1) {
		if (ksceSblSmSchedProxyReadCry2Arm(id, 1, &value) < 0 || !(value & 1))
			goto out;
		if (ksceSblSmSchedProxyWriteCry2Arm(id, 1, value) < 0 || index < 0)
			goto out;
		eventBits = 0x10;
		switch (value & ~1U) {
		case 0:
			result = 0;
			eventBits = 1;
			break;
		case 2:
			result = SM_ERROR_START;
			break;
		case 4:
			result = SM_ERROR_CHANNEL;
			break;
		case 8:
			result = SM_ERROR_TRANSFER;
			break;
		default:
			result = SM_ERROR_REQUEST;
			break;
		}
	} else {
		goto out;
	}
	s_sessions[index].result = result;
	ksceKernelSetEventFlag(s_sessions[index].eventFlag, eventBits);
out:
	return;
}

static int NOINLINE StartSession(int index, uint32_t priority,
	const SceKernelVARange *range, uint32_t command,
	const SceAuthInfo *auth, SceUID *id)
{
	SceKernelPAVector vector;
	void *base = NULL;
	SceKernelPARange *originalBase;
	SceUID eventFlag, memBlock = -1;
	int ret;

	*id = -1;
	eventFlag = ksceKernelCreateEventFlag("SceSblSmCommB639DB03", 0, 0, NULL);
	ret = eventFlag;
	if (ret < 0)
		goto cleanup;
	memBlock = ksceKernelAllocMemBlock("SceSblSmComm7EB92BED", SM_MEMBLOCK_TYPE,
		SM_BUFFER_SIZE, NULL);
	ret = memBlock;
	if (ret < 0)
		goto cleanup;
	ret = ksceKernelGetMemBlockBase(memBlock, &base);
	if (ret < 0)
		goto cleanup;
	originalBase = base;
	vector.size = sizeof(vector);
	vector.ranges_size = SM_BUFFER_SIZE / sizeof(SceKernelPARange);
	vector.ranges = base;
	/* Sysmem defines data_in_vector/count on return; incoming words are untouched. */
	ret = ksceKernelVARangeToPAVector(range, &vector);
	if (ret < 0)
		goto cleanup;
	if (vector.count * 8 > SM_BUFFER_SIZE ||
	    (uintptr_t)originalBase + vector.count * 8 > (uintptr_t)base + SM_BUFFER_SIZE) {
		ret = SM_ERROR_VECTOR;
		goto cleanup;
	}
	*(volatile uint32_t *)base = *(volatile uint32_t *)base;
	PublishWord(base);
	vector.size = 0;
	ret = ksceKernelVAtoPA(base, &vector.size);
	if (ret < 0)
		goto cleanup;
	ret = ksceSblSmSchedProxyInvoke(priority, vector.size, vector.count,
		(const uint32_t *)(uintptr_t)command, auth, id);
	if (ret < 0)
		goto cleanup;
	s_sessions[index].id = *id;
	s_sessions[index].result = 0;
	s_sessions[index].memBlock = memBlock;
	s_sessions[index].eventFlag = eventFlag;
	ret = ksceSblSmSchedProxyEnableCry2ArmInterrupt(*id, 0, sceSblSmCommIntHandler, NULL);
	if (ret < 0)
		goto cleanup;
	ret = ksceSblSmSchedProxyEnableCry2ArmInterrupt(*id, 1, sceSblSmCommIntHandler, NULL);
	if (ret < 0)
		goto cleanup;
	vector.size = 1;
	sceKernelSysrootDisableAutoClockDown();
	ret = ksceKernelWaitEventFlag(eventFlag, 0x11, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &vector.size, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (ret < 0)
		goto cleanup;
	if (vector.size == 1) {
		ret = 0;
		goto out;
	}
	ret = s_sessions[index].result;
cleanup:
	if (*id != -1) {
		ksceSblSmSchedProxyWait(*id, &vector);
		*id = -1;
	}
	if (memBlock >= 0)
		ksceKernelFreeMemBlock(memBlock);
	if (eventFlag >= 0)
		ksceKernelDeleteEventFlag(eventFlag);
out:
	return ret;
}

int module_start(SceSize arglen, void *argp)
{
	uint32_t context;
	int ret;

	(void)arglen;
	(void)argp;
	context = sceSyscallEnter();
	ret = ksceKernelInitializeFastMutex(&s_fastMutex, "SceSblSmCommAB75E2BE", 0, 0);
	if (ret == 0) {
		ret = ksceKernelCreateSema("SceSblSmComm62970c2d", 0, 32, 32, NULL);
		if (ret >= 0)
			s_semaphore = ret;
	}
	sceSyscallLeave(context);
	return ret;
}

int ksceSblSmCommStartSmFromFile(uint32_t priority, const char *path,
	uint32_t command, SceAuthInfo *auth, SceSblSmCommId *id)
{
	uint32_t context = EnterApi();
	SceKernelVARange range;
	void *base = NULL;
	SceUID fd, memBlock;
	SceOff fileSize, position;
	int index, ret;

	ret = ksceKernelWaitSema(s_semaphore, 1, NULL);
	if (ret != 0)
		goto out;
	index = AllocateSessionSlot();
	if (index < 0) {
		ret = SM_ERROR_SLOTS;
		goto out;
	}
	fd = ksceIoOpen(path, 1, 0);
	ret = fd;
	if (fd < 0)
		goto release;
	fileSize = ksceIoLseek(fd, 0, 2);
	if (fileSize < 0) {
		ret = (uint32_t)fileSize;
		goto close;
	}
	if ((uint64_t)fileSize > SM_MAX_IMAGE_SIZE) {
		ret = SM_ERROR_REQUEST;
		goto close;
	}
	position = ksceIoLseek(fd, 0, 0);
	if (position < 0) {
		ret = (uint32_t)position;
		goto close;
	}
	memBlock = ksceKernelAllocMemBlock("SceSblSmComm8D26C678", SM_MEMBLOCK_TYPE,
		((uint32_t)fileSize + 0xfff) & ~0xfffU, NULL);
	if (memBlock < 0) {
		ret = SM_ERROR_BUFFER;
		goto close;
	}
	ret = ksceKernelGetMemBlockBase(memBlock, &base);
	if (ret == 0) {
		ret = ksceIoRead(fd, base, fileSize);
		if ((uint32_t)ret == (uint32_t)fileSize) {
			range.addr = (uintptr_t)base;
			range.size = fileSize;
			ret = StartSession(index, priority, &range, command, auth, id);
		} else {
			ret = SM_ERROR_TRANSFER;
		}
	}
	ksceKernelFreeMemBlock(memBlock);
close:
	ksceIoClose(fd);
	if (ret == 0)
		goto out;
release:
	ReleaseSessionSlot(index);
out:
	LeaveApi(context);
	return ret;
}

int ksceSblSmCommStartSmFromData(uint32_t priority, const void *data,
	uint32_t size, uint32_t command, SceAuthInfo *auth, SceSblSmCommId *id)
{
	uint32_t context = EnterApi();
	SceKernelVARange range;
	void *base = NULL;
	SceUID memBlock;
	int index, ret;

	ret = ksceKernelWaitSema(s_semaphore, 1, NULL);
	if (ret != 0)
		goto out;
	index = AllocateSessionSlot();
	if (index < 0) {
		ret = SM_ERROR_SLOTS;
		goto out;
	}
	if (size > SM_MAX_IMAGE_SIZE) {
		ret = SM_ERROR_REQUEST;
		goto release;
	}
	memBlock = ksceKernelAllocMemBlock("SceSblSmComm8D26C678", SM_MEMBLOCK_TYPE,
		(size + 0xfff) & ~0xfffU, NULL);
	if (memBlock < 0) {
		ret = SM_ERROR_BUFFER;
		goto release;
	}
	ret = ksceKernelGetMemBlockBase(memBlock, &base);
	if (ret == 0) {
		memcpy(base, data, size);
		range.addr = (uintptr_t)base;
		range.size = size;
		ret = StartSession(index, priority, &range, command, auth, id);
	}
	ksceKernelFreeMemBlock(memBlock);
	if (ret == 0)
		goto out;
release:
	ReleaseSessionSlot(index);
out:
	LeaveApi(context);
	return ret;
}

int ksceSblSmCommCallFunc(SceSblSmCommId id, uint32_t service,
	uint32_t *serviceResult, void *data, SceSize size)
{
	uint32_t context = EnterApi();
	uint32_t eventBits = 1, physicalAddress = 0;
	uint32_t *buffer = NULL;
	int index = FindSessionSlot(id), ret;
	SceUID eventFlag;

	if (index < 0 || s_sessions[index].memBlock < 0 ||
	    s_sessions[index].eventFlag < 0) {
		ret = SM_ERROR_SESSION;
		goto out;
	}
	eventFlag = s_sessions[index].eventFlag;
	if (size + SM_PAYLOAD_OFFSET > SM_BUFFER_SIZE) {
		ret = SM_ERROR_BUFFER;
		goto out;
	}
	ret = ksceKernelGetMemBlockBase(s_sessions[index].memBlock, (void **)&buffer);
	if (ret < 0)
		goto out;
	if ((uintptr_t)buffer & 3) {
		ret = SM_ERROR_ALIGNMENT;
		goto out;
	}
	ret = ksceKernelVAtoPA(buffer, &physicalAddress);
	if (ret < 0)
		goto out;
	if (size != 0)
		memcpy((uint8_t *)buffer + SM_PAYLOAD_OFFSET, data, size);
	buffer[0] = size + SM_PAYLOAD_OFFSET;
	buffer[1] = service;
	buffer[2] = 0;
	buffer[3] = 0;
	PublishWord(buffer + 3);
	ret = ksceSblSmSchedCallFunc(id, 1, physicalAddress | 1);
	if (ret < 0)
		goto out;
	if (service == UINT32_MAX) {
		ret = 0;
		goto out;
	}
	sceKernelSysrootDisableAutoClockDown();
	ret = ksceKernelWaitEventFlag(eventFlag, 0x11, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &eventBits, NULL);
	sceKernelSysrootEnableAutoClockDown();
	if (ret < 0)
		goto out;
	if (size != 0)
		memcpy(data, (uint8_t *)buffer + SM_PAYLOAD_OFFSET, size);
	if (serviceResult != NULL)
		*serviceResult = buffer[2];
	ret = eventBits == 1 ? 0 : s_sessions[index].result;
out:
	LeaveApi(context);
	return ret;
}

int ksceSblSmCommStopSm(SceSblSmCommId id, SceSblSmCommPair *result)
{
	uint32_t context = EnterApi();
	struct SmSession *session;
	int index, ret, waitRet;

	ret = ksceSblSmCommCallFunc(id, UINT32_MAX, NULL, NULL, 0);
	if (ret == 0 || ret == SM_ERROR_STOPPED) {
		index = FindSessionSlot(id);
		/* Native cleanup does not reject an unmatched (-1) slot here. */
		session = &s_sessions[index];
		ksceKernelFreeMemBlock(session->memBlock);
		ksceKernelDeleteEventFlag(session->eventFlag);
		ReleaseSessionSlot(index);
		ret = ksceKernelSignalSema(s_semaphore, 1);
		waitRet = ksceSblSmSchedProxyWait(id, result);
		if (waitRet != 0 && ret == 0)
			ret = waitRet;
	}
	LeaveApi(context);
	return ret;
}
