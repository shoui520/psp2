/*
 * SceSblSsSmComm: ARM-side secure-module sessions and command exchange.
 * Private function names are descriptive; the originals contain no debug names.
 * PLACEHOLDER: fast-mutex internals and secure-world command meanings are unknown.
 */
#include <stdint.h>
#include <stddef.h>
#include <sm_comm/sm_comm.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/barrier.h>
#include <intrmgr/syscall.h>

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
int sceSblSmSchedProxyInvoke(uint32_t priority, uint32_t physicalVector,
	uint32_t count, const uint32_t *commandInput, const SelfAuthRequest *auth, SceUID *id);
int sceSblSmSchedProxyWait(SceUID id, void *result);
int SceSblSmSchedProxyForKernel_723B382F(SceUID id, int channel, uint32_t value);
int sceSblSmSchedProxyWriteCry2Arm(SceUID id, int channel, uint32_t value);
int sceSblSmSchedProxyReadCry2Arm(SceUID id, int channel, uint32_t *value);
int SceSblSmSchedProxyForKernel_8B84AC2A(SceUID id, int channel,
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

	if (lockFastMutex(&s_fastMutex) != 0)
		return -1;
	used = s_usedSlots;
	for (index = 0; index < 32; index++) {
		bit = UINT64_C(1) << index;
		if (used & bit)
			continue;
		s_usedSlots = used | bit;
		if (sceKernelUnlockFastMutex(&s_fastMutex) != 0)
			return -1;
		return index;
	}
	sceKernelUnlockFastMutex(&s_fastMutex);
	return -1;
}

static int NOINLINE ReleaseSessionSlot(int index)
{
	uint64_t used;

	if (index >= 32 || lockFastMutex(&s_fastMutex) != 0)
		return -1;
	used = s_usedSlots & ~(UINT64_C(1) << index);
	s_sessions[index].id = 0;
	s_usedSlots = used;
	return sceKernelUnlockFastMutex(&s_fastMutex) != 0 ? -1 : 0;
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
		if (sceSblSmSchedProxyReadCry2Arm(id, 1, &value) < 0 || !(value & 1))
			goto out;
		if (sceSblSmSchedProxyWriteCry2Arm(id, 1, value) < 0 || index < 0)
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
	sceKernelSetEventFlag(s_sessions[index].eventFlag, eventBits);
out:
	return;
}

static int NOINLINE StartSession(int index, uint32_t priority,
	const SceKernelVARange *range, uint32_t command,
	const SelfAuthRequest *auth, SceUID *id)
{
	SceKernelPAVector vector;
	void *base = NULL;
	SceKernelPARange *originalBase;
	SceUID eventFlag, memBlock = -1;
	int ret;

	*id = -1;
	eventFlag = sceKernelCreateEventFlag("SceSblSmCommB639DB03", 0, 0, NULL);
	ret = eventFlag;
	if (ret < 0)
		goto cleanup;
	memBlock = SceSysmemForDriver_C94850C9("SceSblSmComm7EB92BED", SM_MEMBLOCK_TYPE,
		SM_BUFFER_SIZE, NULL);
	ret = memBlock;
	if (ret < 0)
		goto cleanup;
	ret = SceSysmemForDriver_A841EDDA(memBlock, &base);
	if (ret < 0)
		goto cleanup;
	originalBase = base;
	vector.size = sizeof(vector);
	vector.capacity = SM_BUFFER_SIZE / sizeof(SceKernelPARange);
	vector.pRanges = base;
	/* nDataInVector and total are unset: sysmem writes them */
	ret = sceKernelVARangeToPAVector(range, &vector);
	if (ret < 0)
		goto cleanup;
	if (vector.total * 8 > SM_BUFFER_SIZE ||
	    (uintptr_t)originalBase + vector.total * 8 > (uintptr_t)base + SM_BUFFER_SIZE) {
		ret = SM_ERROR_VECTOR;
		goto cleanup;
	}
	*(volatile uint32_t *)base = *(volatile uint32_t *)base;
	PublishWord(base);
	vector.size = 0;
	ret = SceSysmemForDriver_8D160E65(base, &vector.size);
	if (ret < 0)
		goto cleanup;
	ret = sceSblSmSchedProxyInvoke(priority, vector.size, vector.total,
		(const uint32_t *)(uintptr_t)command, auth, id);
	if (ret < 0)
		goto cleanup;
	s_sessions[index].id = *id;
	s_sessions[index].result = 0;
	s_sessions[index].memBlock = memBlock;
	s_sessions[index].eventFlag = eventFlag;
	ret = SceSblSmSchedProxyForKernel_8B84AC2A(*id, 0, sceSblSmCommIntHandler, NULL);
	if (ret < 0)
		goto cleanup;
	ret = SceSblSmSchedProxyForKernel_8B84AC2A(*id, 1, sceSblSmCommIntHandler, NULL);
	if (ret < 0)
		goto cleanup;
	vector.size = 1;
	sceKernelSysrootDisableAutoClockDown();
	ret = sceKernelWaitEventFlag(eventFlag, 0x11,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &vector.size, NULL);
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
		sceSblSmSchedProxyWait(*id, &vector);
		*id = -1;
	}
	if (memBlock >= 0)
		SceSysmemForDriver_009E1C61(memBlock);
	if (eventFlag >= 0)
		sceKernelDeleteEventFlag(eventFlag);
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
	ret = sceKernelInitializeFastMutex(&s_fastMutex, "SceSblSmCommAB75E2BE", 0, 0);
	if (ret == 0) {
		ret = sceKernelCreateSema("SceSblSmComm62970c2d", 0, 32, 32, NULL);
		if (ret >= 0)
			s_semaphore = ret;
	}
	sceSyscallLeave(context);
	return ret;
}

int SceSblSmCommForKernel_7863A0CC(SceUInt32 priority, const char *path,
	SceUInt32 command, SelfAuthRequest *auth, SceSblSmCommId *id)
{
	uint32_t context = EnterApi();
	SceKernelVARange range;
	void *base = NULL;
	SceUID fd, memBlock;
	SceOff fileSize, position;
	int index, ret;

	ret = sceKernelWaitSema(s_semaphore, 1, NULL);
	if (ret != 0)
		goto out;
	index = AllocateSessionSlot();
	if (index < 0) {
		ret = SM_ERROR_SLOTS;
		goto out;
	}
	fd = sceIoOpen(path, 1, 0);
	ret = fd;
	if (fd < 0)
		goto release;
	fileSize = sceIoLseek(fd, 0, 2);
	if (fileSize < 0) {
		ret = (uint32_t)fileSize;
		goto close;
	}
	if ((uint64_t)fileSize > SM_MAX_IMAGE_SIZE) {
		ret = SM_ERROR_REQUEST;
		goto close;
	}
	position = sceIoLseek(fd, 0, 0);
	if (position < 0) {
		ret = (uint32_t)position;
		goto close;
	}
	memBlock = SceSysmemForDriver_C94850C9("SceSblSmComm8D26C678", SM_MEMBLOCK_TYPE,
		((uint32_t)fileSize + 0xfff) & ~0xfffU, NULL);
	if (memBlock < 0) {
		ret = SM_ERROR_BUFFER;
		goto close;
	}
	ret = SceSysmemForDriver_A841EDDA(memBlock, &base);
	if (ret == 0) {
		ret = sceIoRead(fd, base, fileSize);
		if ((uint32_t)ret == (uint32_t)fileSize) {
			range.addr = (uintptr_t)base;
			range.size = fileSize;
			ret = StartSession(index, priority, &range, command, auth, id);
		} else {
			ret = SM_ERROR_TRANSFER;
		}
	}
	SceSysmemForDriver_009E1C61(memBlock);
close:
	sceIoClose(fd);
	if (ret == 0)
		goto out;
release:
	ReleaseSessionSlot(index);
out:
	LeaveApi(context);
	return ret;
}

int SceSblSmCommForKernel_039C73B1(SceUInt32 priority, const void *data,
	SceUInt32 size, SceUInt32 command, SelfAuthRequest *auth, SceSblSmCommId *id)
{
	uint32_t context = EnterApi();
	SceKernelVARange range;
	void *base = NULL;
	SceUID memBlock;
	int index, ret;

	ret = sceKernelWaitSema(s_semaphore, 1, NULL);
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
	memBlock = SceSysmemForDriver_C94850C9("SceSblSmComm8D26C678", SM_MEMBLOCK_TYPE,
		(size + 0xfff) & ~0xfffU, NULL);
	if (memBlock < 0) {
		ret = SM_ERROR_BUFFER;
		goto release;
	}
	ret = SceSysmemForDriver_A841EDDA(memBlock, &base);
	if (ret == 0) {
		memcpy(base, data, size);
		range.addr = (uintptr_t)base;
		range.size = size;
		ret = StartSession(index, priority, &range, command, auth, id);
	}
	SceSysmemForDriver_009E1C61(memBlock);
	if (ret == 0)
		goto out;
release:
	ReleaseSessionSlot(index);
out:
	LeaveApi(context);
	return ret;
}

int SceSblSmCommForKernel_DB9FC204(SceSblSmCommId id, SceUInt32 service,
	SceUInt32 *serviceResult, void *data, SceSize size)
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
	ret = SceSysmemForDriver_A841EDDA(s_sessions[index].memBlock, (void **)&buffer);
	if (ret < 0)
		goto out;
	if ((uintptr_t)buffer & 3) {
		ret = SM_ERROR_ALIGNMENT;
		goto out;
	}
	ret = SceSysmemForDriver_8D160E65(buffer, &physicalAddress);
	if (ret < 0)
		goto out;
	if (size != 0)
		memcpy((uint8_t *)buffer + SM_PAYLOAD_OFFSET, data, size);
	buffer[0] = size + SM_PAYLOAD_OFFSET;
	buffer[1] = service;
	buffer[2] = 0;
	buffer[3] = 0;
	PublishWord(buffer + 3);
	ret = SceSblSmSchedProxyForKernel_723B382F(id, 1, physicalAddress | 1);
	if (ret < 0)
		goto out;
	if (service == UINT32_MAX) {
		ret = 0;
		goto out;
	}
	sceKernelSysrootDisableAutoClockDown();
	ret = sceKernelWaitEventFlag(eventFlag, 0x11,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &eventBits, NULL);
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

int SceSblSmCommForKernel_0631F8ED(SceSblSmCommId id, SceSblSmCommPair *result)
{
	uint32_t context = EnterApi();
	struct SmSession *session;
	int index, ret, waitRet;

	ret = SceSblSmCommForKernel_DB9FC204(id, UINT32_MAX, NULL, NULL, 0);
	if (ret == 0 || ret == SM_ERROR_STOPPED) {
		index = FindSessionSlot(id);
		/* Native cleanup does not reject an unmatched (-1) slot here. */
		session = &s_sessions[index];
		SceSysmemForDriver_009E1C61(session->memBlock);
		sceKernelDeleteEventFlag(session->eventFlag);
		ReleaseSessionSlot(index);
		ret = sceKernelSignalSema(s_semaphore, 1);
		waitRet = sceSblSmSchedProxyWait(id, result);
		if (waitRet != 0 && ret == 0)
			ret = waitRet;
	}
	LeaveApi(context);
	return ret;
}
