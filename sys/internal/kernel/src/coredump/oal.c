/* SceCoredump operating-system abstraction. */

#include <string.h>
#include <stdlib.h>
#include <display/display.h>
#include <arm_neon.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/aes.h>
#include <appmgr/appmgr_debugger.h>
#include <msif/msif.h>
#include <coredump/coredump.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <threadmgr/mutex.h>
#include <sysmem/heap_info.h>
#include <rtc/rtc.h>
#include <iofilemgr/iofilemgr.h>
#include <processmgr/processmgr.h>
#include <processmgr/process_info.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_debugger.h>
#include <processmgr/process_budget.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/threadmgr_vfp.h>
#include <threadmgr/threadmgr_callback.h>
#include <threadmgr/threadmgr_timer.h>
#include <threadmgr/threadmgr_event.h>
#include <ulobjmgr/ulobjmgr.h>
#include <modulemgr/modulemgr_control.h>
#include <modulemgr/modulemgr_debugger.h>
#include <threadmgr/lw_mutex.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/dipsw.h>
#include "oal_internal.h"
#include "writer_internal.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/utils.h>
#include <sysmem/debugled.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/ctype.h>
#include <gpu/lockup.h>
#include <acmgr/acmgr.h>
#include <regmgr/regmgr.h>

static const CoredumpCafIv openPsIdIv = {
	.words = { 0xbf0b9e95, 0xc651a66b, 0x49e8533b, 0xa7056463 },
};

static const uint32_t buildVerWrappedKey[8] = {
	0x32b942ec, 0x8379f43f, 0xa927c396, 0xa2a400f5,
	0xeb959f78, 0x8cbfe768, 0xfc2bea73, 0xaf49026b,
};

static const uint8_t buildVerIv[16] = {
	0x37, 0xfa, 0x4e, 0xd2, 0xb6, 0x61, 0x8b, 0x59,
	0xb3, 0x4f, 0x77, 0x0f, 0xbb, 0x92, 0x94, 0x7b,
};

#if FW_DEBUG
static CoredumpBuildVerInfo buildVerInfo;
static int alwaysEncryptDump;
#else
static int alwaysEncryptDump;
static CoredumpBuildVerInfo buildVerInfo;
#endif
static uint32_t buildVerKey[8];

CoredumpRunContext coredumpRun;

#define IS_ABORT_REASON(reason) \
	((reason) == 0x20004 ? 1 : \
	 (reason) <= 0x20004 ? \
		((reason) >= 0x10004 && \
		 ((reason) <= 0x10009 || (reason) == 0x20001)) : \
	 (reason) == 0x40001 ? 1 : \
	 (reason) > 0x40001 ? (reason) == 0x50001 : \
	 (reason) - 0x30002 <= 2)

static inline void setReason(CoredumpReasonCode *reason,
	unsigned char code, unsigned char group)
{
	reason->codeHigh = 0;
	reason->groupHigh = 0;
	reason->codeLow = code;
	reason->groupLow = group;
}

/* Sony calls it out of line from all five callers */
__attribute__((noinline)) int _getThreadStopReason(SceUID threadId,
	const CoredumpThreadStopEvent *stopEvent, CoredumpStopReason *stopReason)
{
	int result;
	int debugSuspend;

#if FW_DEBUG
	SCE_DBG_ASSERT(stopReason != NULL, 2337);
	SCE_DBG_ASSERT(stopEvent != NULL, 2338);
#endif
	memset(stopReason, 0, sizeof(*stopReason));
	stopReason->reason.codeLow = 0;
	stopReason->reason.codeHigh = 0;
	stopReason->reason.groupLow = 0;
	stopReason->reason.groupHigh = 0;
	stopReason->detail.codeLow = 0;
	stopReason->detail.codeHigh = 0;
	stopReason->detail.groupLow = 0;
	stopReason->detail.groupHigh = 0;
	result = SceThreadmgrForDriver_A0B1AB21(threadId);
	if (result < 0)
		return result;
	if (result & 1) {
		setReason(&stopReason->reason, 2, 1);
		return result;
	}
	debugSuspend = result & ~3;
	switch (debugSuspend) {
	case 0:
		setReason(&stopReason->reason, 0, 0);
		break;
	case 0x10:
		setReason(&stopReason->reason, 1, 4);
		break;
	case 0x100:
	case 0x800:
		setReason(&stopReason->reason, 3, 3);
		break;
	case 0x200:
		setReason(&stopReason->reason, 4, 3);
		break;
	case 0x400:
		setReason(&stopReason->reason, 2, 3);
		break;
	case 0x1000:
		setReason(&stopReason->reason, 4, 2);
		break;
	case 0x2000:
		setReason(&stopReason->reason, 1, 2);
		break;
	case 0x20000:
		setReason(&stopReason->reason, 7, 1);
		break;
	case 0x40000:
		setReason(&stopReason->reason, 8, 1);
		break;
	case 0x80000:
		setReason(&stopReason->reason, 9, 1);
		break;
	}
	if (stopEvent->threadId == threadId) {
		if ((unsigned)(stopEvent->reason - 0x100) <= 12) {
			setReason(&stopReason->reason, 5, 1);
		} else {
			if (stopEvent->reason == 4) {
				setReason(&stopReason->reason, 6, 1);
			} else if (stopEvent->reason == 5) {
				setReason(&stopReason->reason, 7, 1);
			} else if (stopEvent->reason == 6) {
				setReason(&stopReason->reason, 8, 1);
			} else if (stopEvent->reason == 7) {
				setReason(&stopReason->reason, 9, 1);
			}
		}
	}
	return result;
}

int _getCdnPcAndLrInfo(SceUID pid, const CoredumpThreadStopEvent *stopEvent,
	CoredumpCdnPcAndLrInfo *location)
{
	ThreadmgrThreadInfoForDebugger threadInfo;
	ThreadmgrDebugRegisters registers[2];
	CoredumpStopReason stopReason;
	SceUID *threadIds;
	void *allocation;
	SceKernelModuleInfoForDebugger *modules;
	SceKernelModuleInfoForDebugger *module;
	SceSize allocationSize;
	unsigned int copiedCount;
	unsigned int i, j;
	int moduleCount;
	int threadBytes;
	int found = 0;
	int result;
	uintptr_t end;

	memset(location, 0, sizeof(*location));
	result = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (result <= 0)
		return result;
	threadBytes = result * sizeof(*threadIds);
	allocation = memMgrLocalAlloc(threadBytes);
	threadIds = allocation;
	if (threadIds == NULL)
		return 0;
	if (sceKernelGetThreadIdList(pid, threadIds, threadBytes,
		&copiedCount) < 0 || copiedCount == 0)
		goto freeAllocation;
	for (i = 0; i < copiedCount; ++i) {
		uint32_t reason;
		SceUID threadId = threadIds[i];

		threadInfo.size = sizeof(threadInfo);
		if (SceThreadmgrForDriver_DD8D9429(threadId, 0, &threadInfo) != 0)
			continue;
		_getThreadStopReason(threadId, stopEvent, &stopReason);
		__builtin_memcpy(&reason, &stopReason.reason, sizeof(reason));
		if (reason == 0)
			continue;
		if (SceThreadmgrForDriver_5022689D(threadId, registers) == 0) {
			if ((registers[0].cpsr & 0x1f) == 0x10) {
				location->pc = registers[0].pc;
				location->lr = registers[0].lr;
			} else {
				location->pc = registers[1].pc;
				location->lr = registers[1].lr;
			}
			found = 1;
			break;
		}
		found = 1;
	}
	result = memMgrLocalFree(threadIds);
	if (!found)
		return result;
	modules = memMgrLocalAllAlloc(&allocationSize);
	allocation = modules;
	if (allocationSize == 0 || modules == NULL)
		return (int)(uintptr_t)modules;
	memset(modules, 0, allocationSize);
	end = (uintptr_t)modules + allocationSize;
	moduleCount = allocationSize /
		(sizeof(*modules) + 4 * sizeof(modules->segment[0]) +
		 sizeof(SceKernelModuleExidxInfo));
	if (sceKernelGetModuleInfoForDebugger(pid, modules, &moduleCount) < 0) {
		moduleCount = 0;
	} else if (moduleCount != 0 && (uintptr_t)modules < end) {
		module = modules;
		for (i = 0; i != (unsigned int)moduleCount; ++i) {
			for (j = 0; j != module->nSegments; ++j) {
				uintptr_t start = (uintptr_t)module->segment[j].vaddr;
				uintptr_t limit = start + module->segment[j].memsz;

				if (start <= location->pc && location->pc < limit)
					location->pcModuleFingerprint = module->fingerprint;
				if (start <= location->lr && location->lr < limit)
					location->lrModuleFingerprint = module->fingerprint;
			}
			module = (SceKernelModuleInfoForDebugger *)
				((char *)module + module->size);
			if ((uintptr_t)module >= end ||
				(uintptr_t)module < (uintptr_t)modules)
				break;
		}
	}
freeAllocation:
	return memMgrLocalFree(allocation);
}

int decryptBuildVerInfo(void *data, unsigned int size)
{
	ScePortabilityData input;
	union {
		ScePortabilityData output;
		struct {
			SceAesFullContext context;
			uint8_t iv[32];
		} cbc;
	} work;
	uint8_t iv[16];
	uint8_t decrypted[32];
	uint8_t *block = data;
	unsigned int blocks = size >> 4;
	unsigned int i, j;
	int result;

	__builtin_memcpy(iv, buildVerIv, sizeof(iv));
	input.msg_size = sizeof(input.msg);
	__builtin_memcpy(input.msg, buildVerWrappedKey, sizeof(input.msg));
	work.output = (ScePortabilityData) {
		.msg_size = sizeof(work.output.msg),
		.msg = { 0 },
	};
	result = SceSblSsMgrForDriver_934DB6B5(1, iv, &input, &work.output);
	if (result < 0)
		return result;
	if (result == 0)
		__builtin_memcpy(buildVerKey, work.output.msg, sizeof(buildVerKey));
	memset(&work.cbc, 0, sizeof(work.cbc));
	result = SceKernelUtilsForDriver_F12B6451(&work.cbc.context, 128, 256, buildVerKey);
	if (result >= 0) {
		unsigned int blockBytes = work.cbc.context.encryption.blockWords * 4;

		if (blockBytes != 0)
			memcpy(work.cbc.iv, buildVerIv, blockBytes);
		for (i = 0; i < blocks; ++i) {
			SceKernelUtilsForDriver_D8678061(&work.cbc.context, block, decrypted);
			blockBytes = work.cbc.context.encryption.blockWords * 4;
			for (j = 0; j < blockBytes; ++j) {
				uint8_t ciphertext = block[j];

				block[j] = decrypted[j] ^ work.cbc.iv[j];
				work.cbc.iv[j] = ciphertext;
			}
			block += 16;
		}
		result = 0;
	}
	buildVerKey[0] = 0;
	buildVerKey[1] = 0;
	buildVerKey[2] = 0;
	buildVerKey[3] = 0;
	buildVerKey[4] = 0;
	buildVerKey[5] = 0;
	buildVerKey[6] = 0;
	buildVerKey[7] = 0;
	return result;
}

#if FW_DEBUG
int _getAbortedProcessStopReason(SceUID pid, CoredumpStopReason *stopReason)
#else
int _getAbortedProcessStopReason(CoredumpStopReason *stopReason)
#endif
{
	CoredumpThreadStopEvent stopEvent;
	CoredumpStopReason threadStopReason;
	SceUID *threadIds;
	unsigned int capacity, copiedCount, i;
	uint32_t reason, selectedReason;
	int triggerReason;
	SceUInt32 status;
	int count;
	int result;
	SceSize threadBytes;

#if FW_DEBUG
	SCE_DBG_ASSERT(pid != 0 && stopReason != NULL, 1848);
#endif
	result = (int)(uintptr_t)memset(stopReason, 0, sizeof(*stopReason));
	triggerReason = coredumpRun.trigger.triggerReason;
	stopReason->reason.codeLow = 0;
	stopReason->reason.codeHigh = 0;
	stopReason->reason.groupLow = 0;
	stopReason->reason.groupHigh = 0;
	stopReason->detail.codeLow = 0;
	stopReason->detail.codeHigh = 0;
	stopReason->detail.groupLow = 0;
	stopReason->detail.groupHigh = 0;
	if (triggerReason == 1) {
		reason = 0x50001;
		coredumpRun.stopReason = reason;
		goto foundReason;
	}
	if (triggerReason == 3) {
		reason = 0x10004;
		coredumpRun.stopReason = reason;
		goto foundReason;
	}
	if (triggerReason == -2) {
		reason = 0x10003;
		goto foundReason;
	}
	stopEvent.threadId = coredumpRun.trigger.crash_thid;
	stopEvent.reason = triggerReason;
	result = _getThreadStopReason(stopEvent.threadId, &stopEvent,
		&threadStopReason);
	__builtin_memcpy(&reason, &threadStopReason.reason, sizeof(reason));
	if (IS_ABORT_REASON(reason))
		goto foundReason;
	count = sceKernelGetThreadIdList(coredumpRun.pid, NULL, 0, NULL);
	if (count < 0) {
		capacity = 0;
		threadBytes = 0;
	} else {
		capacity = memMgrLocalGetFreeSize() >> 2;
		if (capacity >= (unsigned int)count)
			capacity = count;
		threadBytes = capacity * sizeof(*threadIds);
	}
	threadIds = memMgrLocalAlloc(threadBytes);
	if (threadIds == NULL)
		goto processStatus;
#if FW_DEBUG
	SCE_DBG_ASSERT_FUNC(1, 2178, "oalGetThreadIdList");
#endif
	if (capacity == 0) {
		memMgrLocalFree(threadIds);
		goto processStatus;
	}
	memset(threadIds, 0, threadBytes);
	if (sceKernelGetThreadIdList(coredumpRun.pid, threadIds, threadBytes,
		&copiedCount) >= 0) {
		for (i = 0; i < copiedCount; ++i) {
			if (threadIds[i] != 0) {
				SceUID uid = SceThreadmgrForDriver_F311808F(threadIds[i]);

				if (uid < 0)
					uid = 0;
				threadIds[i] = uid;
			}
		}
		if (capacity > copiedCount)
			memset(threadIds + copiedCount, 0,
				(capacity - copiedCount) * sizeof(*threadIds));
	}
	selectedReason = -1;
	for (i = 0; i < capacity; ++i) {
		SceUID threadId = SceSysmemForDriver_45D22597(coredumpRun.pid, threadIds[i]);

		_getThreadStopReason(threadId, &stopEvent, &threadStopReason);
		__builtin_memcpy(&reason, &threadStopReason.reason, sizeof(reason));
		if (IS_ABORT_REASON(reason))
			selectedReason = reason;
		if (reason != 0x20004 && reason != 0x20007 && reason != 0x10008) {
			if (reason == 0x20001)
				reason = 0x20001;
			else
				reason = selectedReason;
		}
		selectedReason = reason;
	}
	result = memMgrLocalFree(threadIds);
	if (selectedReason != (uint32_t)-1) {
		reason = selectedReason;
		goto foundReason;
	}
processStatus:
	result = sceKernelGetProcessStatus(coredumpRun.pid, &status);
	if (result >= 0) {
		switch ((uint8_t)status) {
		case 1:
		case 2:
			reason = 0;
			goto foundReason;
		case 3:
			reason = 0x10003;
			goto foundReason;
		case 0x11:
			reason = 0x10001;
			goto foundReason;
		case 0x16:
			reason = 0x20001;
			goto foundReason;
		}
	}
	return result;
foundReason:
	__builtin_memcpy(&stopReason->reason, &reason, sizeof(reason));
	return result;
}

int _getBuildVerInfo(void)
{
	uint32_t digest[8];
	char *end;
	char *buffer;
	char *cursor, *limit;
	int result;
	int fd;
	int bytes;
	unsigned int size;

	result = memMgrInit();
	if (result != 0)
		return result;
	size = result;
#if FW_DEBUG
	SCE_DBG_ASSERT(1, 1437);
#endif
	memset(&buildVerInfo, 0, sizeof(buildVerInfo));
	fd = sceIoOpen("vs0:vsh/etc/index.dat", 1, 0);
	if (fd < 0)
		goto finish;
	buffer = memMgrLocalAlloc(0x2000);
	if (buffer == NULL)
		goto closeFile;
	for (;;) {
		bytes = sceIoRead(fd, buffer + size, 0x2000 - size);
		if (bytes <= 0)
			break;
		size += bytes;
	}
	if ((bytes != 0 && bytes != (int)0x80010009) || (int)size < 0 ||
		decryptBuildVerInfo(buffer, size) != 0 || size <= 31)
		goto freeBuffer;
	cursor = buffer + 32;
	__builtin_memset(digest, 0, sizeof(digest));
	if (SceKernelUtilsForDriver_A773A6A8(cursor, size - 32, digest) < 0 ||
		memcmp(buffer, digest, sizeof(digest)) != 0)
		goto freeBuffer;
	limit = buffer + 0x2000;
	for (;;) {
		char *name, *nameEnd, *value, *valueEnd;
		size_t nameSize, valueSize;

		while (cursor < limit && (look_ctype_table((uint8_t)*cursor) & 8))
			++cursor;
		if (cursor == limit)
			break;
		name = cursor;
		while (cursor < limit) {
			int character = (uint8_t)*cursor;

			if ((look_ctype_table(character) & 8) || character == ':')
				break;
			++cursor;
		}
		nameEnd = cursor;
		if (cursor == limit)
			break;
		while (cursor < limit && (look_ctype_table((uint8_t)*cursor) & 8))
			++cursor;
		if (cursor == limit || *cursor != ':')
			break;
		++cursor;
		if (cursor == limit)
			break;
		while (cursor < limit && (look_ctype_table((uint8_t)*cursor) & 8))
			++cursor;
		if (cursor == limit)
			break;
		value = cursor;
		while (cursor < limit) {
			int character = (uint8_t)*cursor;

			if ((look_ctype_table(character) & 8) || character == ':')
				break;
			++cursor;
		}
		valueEnd = cursor;
		valueSize = valueEnd - value;
		if (valueSize >= 64)
			valueSize = 64;
		nameSize = nameEnd - name;
		if (strncmp("sys-revision", name, nameSize) == 0)
			buildVerInfo.sysRevision = strtol(value, &end, 10);
		else if (strncmp("vsh-revision", name, nameSize) == 0)
			buildVerInfo.vshRevision = strtol(value, &end, 10);
		else if (strncmp("security-revision", name, nameSize) == 0)
			buildVerInfo.securityRevision = strtol(value, &end, 10);
		else if (strncmp("sys-repository-path", name, nameSize) == 0) {
#if FW_DEBUG
			__strncpy_chk(buildVerInfo.sysRepositoryPath, value, valueSize,
				sizeof(buildVerInfo) - 16);
#else
			strncpy(buildVerInfo.sysRepositoryPath, value, valueSize);
#endif
		} else if (strncmp("vsh-repository-path", name, nameSize) == 0) {
#if FW_DEBUG
			__strncpy_chk(buildVerInfo.vshRepositoryPath, value, valueSize,
				sizeof(buildVerInfo) - 80);
#else
			strncpy(buildVerInfo.vshRepositoryPath, value, valueSize);
#endif
		} else if (strncmp("security-repository-path", name, nameSize) == 0) {
#if FW_DEBUG
			__strncpy_chk(buildVerInfo.securityRepositoryPath, value, valueSize,
				sizeof(buildVerInfo) - 144);
#else
			strncpy(buildVerInfo.securityRepositoryPath, value, valueSize);
#endif
		} else if (strncmp("sdk-internal-build-number", name, nameSize) == 0)
			buildVerInfo.sdkInternalBuildNumber = strtol(value, &end, 10);
		else if (strncmp("vsh-build-number", name, nameSize) == 0)
			buildVerInfo.vshBuildNumber = strtol(value, &end, 10);
	}
freeBuffer:
	memMgrLocalFree(buffer);
closeFile:
	sceIoClose(fd);
finish:
	memMgrFinal();
	result = sceRegMgrGetKeyInt("/CONFIG/COREDUMP", "always_encrypt_dump",
		(int *)&end);
	alwaysEncryptDump = result == 0 && end == (char *)1;
	return result;
}

static inline int _isPlaintextDumpAllowed(SceUID pid)
{
	return alwaysEncryptDump == 0 && SceSblACMgrForDriver_4CBD6156(pid) != 0;
}

static inline int _isSystemProgram(SceUID pid)
{
	SceSize titleSize = coredumpRun.trigger.titleid_len;
	const char *titleid = coredumpRun.trigger.titleid;
	int isCurrentProcess;

	titleSize = titleSize < 9 ? titleSize : 9;
	isCurrentProcess = coredumpRun.pid == pid;
	if (titleSize == 0)
		isCurrentProcess = 0;
	return (isCurrentProcess &&
		(strncmp(titleid, "NPXS10083", titleSize) == 0 ||
		 strncmp(titleid, "NPXS10084", titleSize) == 0)) ||
		sceSblACMgrIsSystemProgram(pid) != 0;
}

int oalIsPlaintextDumpAllowed(SceUID pid)
{
	return _isPlaintextDumpAllowed(pid);
}

int oalIsSystemProgram(SceUID pid)
{
	return _isSystemProgram(pid);
}

int oalIsRootProgram(SceUID pid)
{
	return sceSblACMgrIsKernelProgram(pid) != 0;
}

int oalIsCEX(void)
{
	return SceSblAIMgrForDriver_D78B04A2() != 0;
}

int oalIsToolDipswEnabled(void)
{
	int result = SceSblAIMgrForDriver_274663A0();

	if (result != 0)
		return SceDipswForDriver_A98FC2FD(210) != 0;
	return result;
}

int oalMemBlockInit(SceUID *blockId, SceSize size)
{
	SceUID result;

	SCE_DBG_ASSERT(blockId != NULL, 317);
	result = sceKernelAllocMemBlockForKernel("SceCoredumpMemBlk",
		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, size, NULL);
	if (result >= 0) {
		*blockId = result;
		return 0;
	}
	return result;
}

int oalMemBlockInitForDevTool(SceUID *blockId, SceSize size)
{
	SceUID result;

	SCE_DBG_ASSERT(blockId != NULL, 335);
	result = sceKernelAllocMemBlockForKernel("SceCoredumpMemBlk",
		SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW, size, NULL);
	if (result >= 0) {
		*blockId = result;
		return 0;
	}
	return result;
}

int oalMemBlockFinal(SceUID blockId)
{
	return SceSysmemForDriver_009E1C61(blockId);
}

int oalHeapInit(SceUID blockId, SceUID *heapId, SceSize maxSize)
{
	SceKernelHeapCreateOpt option;
	SceUID result;

	SCE_DBG_ASSERT(heapId != NULL, 365);
	option.size = sizeof(option);
	option.flags = 0x300;
	option.memBlock = blockId;
	option.maxSize = maxSize;
	option.memtype = 0;
	option.pid = 0;
	option.heap = NULL;
	result = sceKernelCreateHeap("SceCoredumpHeap", 0x1000, &option);
	if (result >= 0) {
		*heapId = result;
		return 0;
	}
	return result;
}

int oalHeapFinal(SceUID heapId)
{
	return sceKernelDeleteHeap(heapId);
}

SceSize oalHeapGetFreeSize(SceUID heapId)
{
	HeapInfo info;
	struct malloc_segment_range blocks[16];
	int result;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	info.maxBlocks = 16;
	info.blockList = blocks;
	result = sceKernelGetHeapInfo(heapId, 5, &info);
	if (result != 0)
		return 0;
	if (info.fordblks > 64)
		return info.fordblks - 64;
	return 0;
}

void *oalHeapAlloc(SceUID heapId, SceSize size)
{
	return sceKernelAllocHeapMemory(heapId, size);
}

int oalHeapFree(SceUID heapId, void *allocation)
{
	SCE_DBG_ASSERT(allocation != NULL, 463);
	return sceKernelFreeHeapMemory(heapId, allocation);
}

int oalGetPartitionOfMemBlock(const CoredumpMemBlockInfo *info)
{
	uintptr_t vbase;

	SCE_DBG_ASSERT(info != NULL, 475);
	vbase = info->vbase;
	if (vbase - 0x60000000u < 0x08000000u)
		return 32;
	if (vbase - 0x68000000u < 0x08000000u)
		return 64;
	if (vbase - 0x70000000u < 0x10000000u)
		return 128;
	if (vbase - 0x80000000u < 0x01000000u)
		return 256;
	if (vbase - 0x81000000u < 0x4f000000u)
		return 512;
	if (vbase - 0xd0000000u < 0x10000000u)
		return 1024;
	if (vbase - 0xe0000000u < 0x10000000u)
		return 2048;
	if (vbase >= 0xf0000000u)
		return 4096;
	SCE_DBG_ASSERT(0, 519);
	return 0;
}

SceUID oalGetThreadId(void)
{
	return sceKernelGetThreadId();
}

int oalCreateThread(SceUID *threadId, const char *name, int priority,
	SceSize stackSize, SceKernelThreadEntry entry)
{
	SceUID result;

	SCE_DBG_ASSERT(threadId != NULL, 543);
	result = sceKernelCreateThread(name, entry, priority, stackSize,
		0, 0x80000, NULL);
	if (result >= 0) {
		*threadId = result;
		return 0;
	}
	return result;
}

int oalDeleteThread(SceUID threadId)
{
	return sceKernelDeleteThread(threadId);
}

int oalStartThread(SceUID threadId)
{
	return sceKernelStartThread(threadId, 0, NULL);
}

int oalExitDeleteThread(void)
{
	return sceKernelExitDeleteThread(0);
}

int oalWaitThreadEnd(SceUID threadId, SceUInt timeout)
{
	SceUInt remaining = timeout;
	int exitStatus;

	return sceKernelWaitThreadEnd(threadId, &exitStatus, &remaining);
}

/* the core file's name part: the process name, else its program class */
void oalGetProcessDumpName(SceUID pid, char *name)
{
	SceKernelProcessInfo process;

	memset(name, 0, 32);
	if (_isSystemProgram(pid) != 0 || sceSblACMgrIsKernelProgram(pid) != 0)
		strncpy(name, "system", 32);
	else
		strncpy(name, "nonsystem", 32);
	memset(&process, 0, sizeof(process));
	process.size = sizeof(process);
	if (sceKernelGetProcessInfo(pid, &process) == 0 &&
	    strnlen(process.name, sizeof(process.name)) != 0)
		__builtin_memcpy(name, process.name, sizeof(process.name));
}

int oalCondInit(SceUID *condId, SceUID mutexId)
{
	SceUID result;

	SCE_DBG_ASSERT(condId != NULL, 648);
	result = sceKernelCreateCond("SceCoredumpCnd", 0, mutexId, NULL);
	if (result >= 0) {
		*condId = result;
		return 0;
	}
	return result;
}

int oalCondFinal(SceUID condId)
{
	return sceKernelDeleteCond(condId);
}

int oalCondWait(SceUID condId)
{
	return sceKernelWaitCond(condId, NULL);
}

int oalCondSignalTo(SceUID condId, SceUID threadId)
{
	return sceKernelSignalCondTo(condId, threadId);
}

int oalMutexInit(SceUID *mutexId)
{
	SceUID result;

	SCE_DBG_ASSERT(mutexId != NULL, 713);
	result = sceKernelCreateMutex("SceCoredumpMtx", 0, 0, NULL);
	if (result >= 0) {
		*mutexId = result;
		return 0;
	}
	return result;
}

int oalMutexFinal(SceUID mutexId)
{
	return sceKernelDeleteMutex(mutexId);
}

int oalMutexLock(SceUID mutexId)
{
	return sceKernelLockMutex(mutexId, 1, NULL);
}

int oalMutexUnlock(SceUID mutexId)
{
	return sceKernelUnlockMutex(mutexId, 1);
}

int oalEventQueueInit(CoredumpEventQueue *queue)
{
	SceUID mutexId, condId;

	SCE_DBG_ASSERT(queue != NULL, 760);
	memset(queue, 0, sizeof(*queue));
	mutexId = sceKernelCreateMutex("SceCoredumpEvtMtx", 0, 0, NULL);
	if (mutexId < 0)
		return mutexId;
	queue->mutexId = mutexId;
	condId = sceKernelCreateCond("SceCoredumpEvtCnd", 0, mutexId, NULL);
	if (condId < 0) {
		sceKernelDeleteMutex(queue->mutexId);
		return condId;
	}
	queue->condId = condId;
	return 0;
}

int oalEventQueueFinal(CoredumpEventQueue *queue)
{
	int condResult, mutexResult;

	SCE_DBG_ASSERT(queue != NULL, 803);
	condResult = sceKernelDeleteCond(queue->condId);
	mutexResult = sceKernelDeleteMutex(queue->mutexId);
	return mutexResult < 0 ? mutexResult : condResult;
}

int oalEventQueueSend(CoredumpEventQueue *queue, const CoredumpEvent *event)
{
	unsigned int count;
	int result, status;

	SCE_DBG_ASSERT(queue != NULL && event != NULL, 825);
	result = sceKernelLockMutex(queue->mutexId, 1, NULL);
	if (result < 0)
		return result;
	count = queue->count;
	if (count < 10) {
		memcpy(&queue->events[count], event, sizeof(*event));
		queue->count = count + 1;
		status = sceKernelSignalCond(queue->condId);
	} else {
		status = (int)0x80010069;
	}
	result = sceKernelUnlockMutex(queue->mutexId, 1);
	return result == 0 ? status : result;
}

int oalEventQueueReceive(CoredumpEventQueue *queue, CoredumpEvent *event)
{
	unsigned int count;
	int result, status;

	SCE_DBG_ASSERT(queue != NULL && event != NULL, 865);
	status = sceKernelLockMutex(queue->mutexId, 1, NULL);
	if (status < 0)
		return status;
	while (queue->count == 0) {
		status = sceKernelWaitCond(queue->condId, NULL);
		if (status < 0)
			goto unlock;
	}
	memcpy(event, queue->events, sizeof(*event));
	count = queue->count;
	if (count > 1)
		memmove(queue->events, &queue->events[1],
			sizeof(*event) * count - sizeof(*event));
	queue->count = count - 1;
unlock:
	result = sceKernelUnlockMutex(queue->mutexId, 1);
	return result == 0 ? status : result;
}

int oalEventQueueEachElem(CoredumpEventQueue *queue,
	void (*callback)(CoredumpEvent *event, void *context), void *context)
{
	CoredumpEvent *event;
	unsigned int index;
	int result, status;

	SCE_DBG_ASSERT(queue != NULL && callback != NULL, 915);
	status = sceKernelLockMutex(queue->mutexId, 1, NULL);
	if (status < 0)
		return status;
	if (queue->count != 0) {
		event = queue->events;
		index = 0;
		do {
			callback(event, context);
			++index;
			++event;
		} while (index < queue->count);
	}
	result = sceKernelUnlockMutex(queue->mutexId, 1);
	return result == 0 ? status : result;
}

unsigned int oalGetUnixTime(void)
{
	SceRtcTick tick;
	SceDateTime dateTime;
	SceUInt32 unixTime[2];

	SceRtcForDriver_DEC408D4(&tick);
	SceRtcForDriver_42A0DFCB(&dateTime, &tick);
	SceRtcForDriver_497B5DC9(&dateTime, (SceUInt64 *)unixTime);
	if (unixTime[1] != 0)
		return (unsigned int)-1;
	return unixTime[0];
}

int oalFileOpen(const char *path, SceUID *fd)
{
	SceUID result;

	SCE_DBG_ASSERT(path != NULL, 972);
	SCE_DBG_ASSERT(fd != NULL, 973);
	result = sceIoOpen(path, 0x4603, 0606);
	if (result >= 0) {
		*fd = result;
		return 0;
	}
	return result;
}

int oalFileReopen(const char *path, SceUID *fd)
{
	SceUID result;

	SCE_DBG_ASSERT(path != NULL, 991);
	SCE_DBG_ASSERT(fd != NULL, 992);
	result = sceIoOpen(path, 0x4001, 0606);
	if (result >= 0) {
		*fd = result;
		return 0;
	}
	return result;
}

int oalFileSetIoCacheSize(SceUID fd, SceSize cacheSize)
{
	VfsBuffCtlIoctlArg argument = {
		.ioCacheSize = cacheSize,
		.mask = 8,
	};

	return sceIoIoctl(fd, 0x1001, &argument, sizeof(argument), NULL, 0);
}

int oalFileClose(SceUID fd)
{
	return sceIoClose(fd);
}

int oalFileRemove(const char *path)
{
	SCE_DBG_ASSERT(path != NULL, 1043);
	return sceIoRemove(path);
}

int oalFileRename(const char *oldPath, const char *newPath)
{
	SCE_DBG_ASSERT(oldPath != NULL, 1054);
	SCE_DBG_ASSERT(newPath != NULL, 1055);
	return sceIoRename(oldPath, newPath);
}

int oalFileRead(SceUID fd, void *buffer, SceSize size)
{
	SCE_DBG_ASSERT(buffer != NULL, 1066);
	return sceIoRead(fd, buffer, size);
}

int oalFileWrite(SceUID fd, const void *buffer, SceSize size)
{
	SCE_DBG_ASSERT(buffer != NULL, 1077);
	return sceIoWrite(fd, buffer, size);
}

SceOff oalFileSeek(SceUID fd, SceOff offset, int whence)
{
	return sceIoLseek(fd, offset, whence);
}

int oalGetThreadStack(SceUID pid, SceUID threadId, uintptr_t *stackBase,
	SceSize *stackSize)
{
	ThreadmgrThreadInfoForDebugger info;
	int result;

	SCE_DBG_ASSERT(stackBase != NULL && stackSize != NULL, 1100);
	*stackBase = 0;
	*stackSize = 0;
	info.size = sizeof(info);
	result = SceThreadmgrForDriver_DD8D9429(
		SceSysmemForDriver_45D22597(pid, threadId), 0, &info);
	if (result == 0) {
		*stackBase = info.userStackBase;
		*stackSize = info.userStackSize;
	}
	return result;
}

int oalGetThreadTlsStack(SceUID pid, SceUID threadId, uintptr_t *stackBase,
	SceSize *stackSize)
{
	ThreadmgrDebugRegisters registers[2];
	uintptr_t stackTop, stackBottom, sp;
	const void *topAddress, *bottomAddress;
	SceUID guid;
	int accessible, result;

	*stackBase = 0;
	*stackSize = 0;
	guid = SceSysmemForDriver_45D22597(pid, threadId);
	topAddress = SceThreadmgrForDriver_66EEA46A(guid, 2);
	bottomAddress = SceThreadmgrForDriver_66EEA46A(guid, 3);
	stackTop = 0;
	stackBottom = 0;
	SCE_DBG_ASSERT_FUNC(1, 1254, "oalCopyFromUserSpace");
	if (sceKernelCopyFromUserProc(pid, &stackTop, topAddress,
		sizeof(stackTop)) != 0)
		stackTop = 0;
	SCE_DBG_ASSERT_FUNC(1, 1254, "oalCopyFromUserSpace");
	if (sceKernelCopyFromUserProc(pid, &stackBottom, bottomAddress,
		sizeof(stackBottom)) != 0)
		stackBottom = 0;
	*stackBase = stackBottom >> 12 << 12;
	*stackSize = ((stackTop + 4095) >> 12 << 12) - *stackBase;
	accessible = SceSysmemForDriver_9F6E45E3(pid, 16,
		(const void *)*stackBase, *stackSize);
	result = SceThreadmgrForDriver_5022689D(guid, registers);
	if (result == 0) {
		sp = (registers[0].cpsr & 0x1f) == 0x10 ?
			registers[0].sp : registers[1].sp;
		if (stackBottom <= sp && stackTop >= sp && accessible == 0 &&
			stackTop > stackBottom)
			return result;
	}
	return oalGetThreadStack(pid, threadId, stackBase, stackSize);
}

int oalProbeThreadInfo(SceUID pid, SceUID threadId)
{
	ThreadmgrThreadInfoForDebugger info;

	info.size = sizeof(info);
	SceThreadmgrForDriver_DD8D9429(SceSysmemForDriver_45D22597(pid, threadId), 0, &info);
	return 0;
}

int oalIsAccessibleRange(SceUID pid, const void *address, SceSize size)
{
	return SceSysmemForDriver_9F6E45E3(pid, 16, address, size) == 0;
}

void oalCopyFromUserSpace(SceUID pid, void *destination, const void *source,
	SceSize size)
{
	SCE_DBG_ASSERT(destination != NULL, 1254);
	if (size != 0 &&
		sceKernelCopyFromUserProc(pid, destination, source, size) != 0)
		memset(destination, 0, size);
}

void oalGetCorefileInfo(SceUID pid, CoredumpCorefileInfo *info)
{
	SceSize titleSize;
	SceUID currentPid;
	const char *titleid;
	int isCurrentProcess;

	SCE_DBG_ASSERT(info != NULL, 1280);
	memset(info, 0, sizeof(*info));
	titleSize = coredumpRun.trigger.titleid_len;
	currentPid = coredumpRun.pid;
	titleid = coredumpRun.trigger.titleid;
	titleSize = titleSize < 9 ? titleSize : 9;
	info->formatVersion[0] = 19;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	isCurrentProcess = pid == currentPid;
	if (titleSize == 0)
		isCurrentProcess = 0;
	info->formatVersion[3] = 0;
	if ((isCurrentProcess &&
		(strncmp(titleid, "NPXS10083", titleSize) == 0 ||
		 strncmp(titleid, "NPXS10084", titleSize) == 0)) ||
		sceSblACMgrIsSystemProgram(pid) != 0 ||
		sceSblACMgrIsKernelProgram(pid) != 0)
		info->programClass = 2;
	else
		info->programClass = 1;
}

void oalGetSystemInfo(SceUID pid, CoredumpSystemInfo *info)
{
	SceKernelOpenPsId openPsId;
	CoredumpCafSectionHash sectionHash;
	uint8_t qaFlags[16];
	CoredumpCafIv iv;
	uint8x16_t flags, zero;
	uint64_t timestamp;
	SceSize titleSize;
	SceUID currentPid, context;
	const char *titleid;
	int isCurrentProcess, qaEnabled;

	SCE_DBG_ASSERT(info != NULL, 1304);
	memset(info, 0, sizeof(*info));
	info->systemVersion[0] = FW_VERSION & 255;
	info->systemVersion[1] = FW_VERSION >> 8 & 255;
	info->systemVersion[2] = FW_VERSION >> 16 & 255;
	info->kernelVersion[0] = FW_VERSION & 255;
	info->kernelVersion[1] = FW_VERSION >> 8 & 255;
	info->kernelVersion[2] = FW_VERSION >> 16 & 255;
	info->hardwareType[0] = 0;
	info->hardwareType[1] = 0;
	info->hardwareType[2] = 0;
	info->hardwareType[3] = 0;
	info->systemVersion[3] = FW_VERSION >> 24;
	info->kernelVersion[3] = FW_VERSION >> 24;
	if (SceSblAIMgrForDriver_274663A0() != 0) {
		info->hardwareType[0] = 1;
	} else if (SceSblAIMgrForDriver_F4B98F66() != 0) {
		info->hardwareType[1] = 0;
		info->hardwareType[2] = 0;
		info->hardwareType[3] = 0;
		info->hardwareType[0] = 2;
	} else if (SceSblAIMgrForDriver_D78B04A2() != 0) {
		info->hardwareType[1] = 0;
		info->hardwareType[2] = 0;
		info->hardwareType[3] = 0;
		info->hardwareType[0] = 3;
	}
	info->dipswInfo = SceDipswForDriver_B2AD48BE(3);
	info->flags[0] = 0;
	info->flags[1] = 0;
	info->flags[2] = 0;
	info->flags[3] = 0;
	info->reservedHardware[0] = 0;
	info->reservedHardware[1] = 0;
	info->isDolceBytes[0] = 0xff;
	info->isDolceBytes[1] = 0xff;
	info->isDolce = SceSblAIMgrForDriver_71608CA3() != 0;
	if (SceSblSsMgrForDriver_A5B5D269(&openPsId) != 0)
		return;
	titleSize = coredumpRun.trigger.titleid_len;
	currentPid = coredumpRun.pid;
	titleid = coredumpRun.trigger.titleid;
	titleSize = titleSize < 9 ? titleSize : 9;
	isCurrentProcess = pid == currentPid;
	if (titleSize == 0)
		isCurrentProcess = 0;
	if ((isCurrentProcess &&
		(strncmp(titleid, "NPXS10083", titleSize) == 0 ||
		 strncmp(titleid, "NPXS10084", titleSize) == 0)) ||
		sceSblACMgrIsSystemProgram(pid) != 0 ||
		sceSblACMgrIsKernelProgram(pid) != 0)
		info->openPsId = openPsId;
	else
		memset(&info->openPsId, 0, sizeof(info->openPsId));
	context = sceCoredumpCafContextCreate();
	if (context < 0)
		return;
	iv = openPsIdIv;
	SCE_DBG_ASSERT_FUNC(1, 5831, "oalCafSegmentInit");
	if (SceCoredumpForDriver_07185515(context, 1, 1, 2, &iv, sizeof(iv)) != 0)
		return;
	SCE_DBG_ASSERT_FUNC(1, 5890, "oalCafSegmentTransform");
	SCE_DBG_ASSERT_FUNC(1, 5891, "oalCafSegmentTransform");
	if (SceCoredumpForDriver_FB7AEBFE(context, &openPsId,
		&info->encryptedOpenPsId, sizeof(openPsId)) != 0) {
		memset(&info->encryptedOpenPsId, 0, sizeof(info->encryptedOpenPsId));
		return;
	}
	SCE_DBG_ASSERT_FUNC(1, 5902, "oalCafSegmentFinal");
	sectionHash = (CoredumpCafSectionHash){ 0 };
	if (SceCoredumpForDriver_DF17420A(context, sectionHash.digest,
		sizeof(sectionHash.digest)) != 0)
		return;
	timestamp = SceSysrootForKernel_E20F6FC8();
	info->timestampLow = timestamp;
	info->timestampHigh = timestamp >> 32;
	memset(qaFlags, 0, sizeof(qaFlags));
	qaEnabled = 0;
	if (SceSblSsMgrForKernel_83D254FF(qaFlags) == 0) {
		zero = vdupq_n_u8(0);
		flags = vld1q_u8(qaFlags);
		flags = vorrq_u8(vextq_u8(flags, zero, 8), flags);
		flags = vorrq_u8(vextq_u8(flags, zero, 4), flags);
		flags = vorrq_u8(vextq_u8(flags, zero, 2), flags);
		flags = vorrq_u8(vextq_u8(flags, zero, 1), flags);
		qaEnabled = vgetq_lane_u8(flags, 0) != 0;
	}
	info->qaEnabled = qaEnabled;
	info->vshBuildNumber = buildVerInfo.vshBuildNumber;
	info->gpi = sceKernelGetGPI();
	info->grownPhyMemPartSize = SceSysmemForKernel_BC36755F();
}

int oalGetAppBuildSDKVersion(SceUID pid, SceUInt32 *sdkVersion)
{
	SCE_DBG_ASSERT(sdkVersion != NULL, 1414);
	*sdkVersion = 0;
	return SceProcessmgrForDriver_D141C076(pid, sdkVersion);
}

void oalGetBuildVerInfo(SceUID pid, CoredumpBuildVerInfo *info)
{
	SCE_DBG_ASSERT(info != NULL, 1428);
	memcpy(info, &buildVerInfo, sizeof(*info));
}

void oalGetHwInfo(SceUID pid, CoredumpHwInfo *info)
{
	uint32_t qaFlags[4];
	char qafName[16];

	SCE_DBG_ASSERT(info != NULL, 1692);
	memset(info, 0, sizeof(*info));
	info->productCode = SceSblAIMgrForDriver_14345161();
	info->productSubCode = SceSblAIMgrForDriver_B33CEC8F();
	__builtin_memset(qaFlags, 0, sizeof(qaFlags));
	if (SceSblSsMgrForKernel_83D254FF(qaFlags) == 0)
		__builtin_memcpy(info->qaFlags, qaFlags, sizeof(qaFlags));
	__builtin_memset(qafName, 0, sizeof(qafName));
	SceSblSsMgrForKernel_E2DD0378(qafName, sizeof(qafName));
	__builtin_memcpy(info->qafName, qafName, sizeof(qafName));
}

static inline void _getExternalProcessStopReason(
	const SceKernelProcessInfo *process, CoredumpStopReason *stopReason)
{
	SCE_DBG_ASSERT_FUNC(stopReason != NULL, 2009, "_getExternalProcessStopReason");
	memset(stopReason, 0, sizeof(*stopReason));
	stopReason->reason.codeLow = 0;
	stopReason->reason.codeHigh = 0;
	stopReason->reason.groupLow = 0;
	stopReason->reason.groupHigh = 0;
	stopReason->detail.codeLow = 0;
	stopReason->detail.codeHigh = 0;
	stopReason->detail.groupLow = 0;
	stopReason->detail.groupHigh = 0;
	switch ((uint8_t)process->status) {
	case 1:
	case 2:
	case 3:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 3;
		stopReason->reason.groupLow = 1;
		break;
	case 17:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 1;
		stopReason->reason.groupLow = 1;
		break;
	case 18:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 4;
		stopReason->reason.groupLow = 2;
		break;
	case 19:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 3;
		stopReason->reason.groupLow = 3;
		break;
	case 20:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 4;
		stopReason->reason.groupLow = 3;
		break;
	case 21:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 2;
		stopReason->reason.groupLow = 3;
		break;
	case 22:
		stopReason->reason.codeHigh = 0;
		stopReason->reason.groupHigh = 0;
		stopReason->reason.codeLow = 1;
		stopReason->reason.groupLow = 2;
		break;
	}
}

void oalGetProcessInfo(SceUID pid, CoredumpProcessInfo *info)
{
	SceKernelProcessInfo process;
	uint32_t flags;
	SceUInt32 status;

	SCE_DBG_ASSERT(info != NULL, 1722);
	memset(info, 0, sizeof(*info));
	memset(&process, 0, sizeof(process));
	process.size = sizeof(process);
	if (sceKernelGetProcessInfo(pid, &process) != 0)
		return;
	status = 0;
	info->pid = process.pid;
	if (sceKernelGetProcessStatus(pid, &status) < 0) {
		flags = 0;
	} else {
		status = (uint8_t)status;
		flags = _isPlaintextDumpAllowed(pid) ? 0x10000 : 0;
		if (status == 3)
			flags |= 8;
		if (_isSystemProgram(pid) != 0)
			flags |= 4;
		if (status == 2)
			flags |= 2;
		flags |= 1;
	}
	info->flags = flags;
	__builtin_memcpy(info->name, process.name, sizeof(info->name));
	info->cpuAffinityMask = process.cpuAffinity;
	info->entryPoint = (uintptr_t)process.entryPoint;
	SceModulemgrForKernel_337A3908(process.mainModule, &info->fingerprint);
	if (process.parentPid == 0x10005) {
		info->parentPidBytes[0] = -1;
		info->parentPidBytes[1] = -1;
		info->parentPidBytes[2] = -1;
		info->parentPidBytes[3] = -1;
	} else {
		info->parentPid = process.parentPid;
	}
	if (pid == coredumpRun.pid) {
#if FW_DEBUG
		_getAbortedProcessStopReason(pid, &info->stopReason);
#else
		_getAbortedProcessStopReason(&info->stopReason);
#endif
		return;
	}
	_getExternalProcessStopReason(&process, &info->stopReason);
}

SceUInt64 oalGetProcessTime(SceUID pid)
{
	SceUInt64 time = 0;
	SceUInt64 timestamp = SceSysrootForKernel_E20F6FC8();

	if (SceProcessmgrForDriver_8CB01675(pid, &time, timestamp) < 0)
		return (SceUInt64)-1;
	return time;
}

int oalGetProcessPath(SceUID pid, char *path, SceSize *pathSize)
{
	SceKernelProcessInfo process;
	int result;

	SCE_DBG_ASSERT(path != NULL && pathSize != NULL, 2074);
	memset(path, 0, 256);
	*pathSize = 0;
	memset(&process, 0, sizeof(process));
	process.size = sizeof(process);
	result = sceKernelGetProcessInfo(pid, &process);
	if (result != 0)
		return result;
	result = SceModulemgrForKernel_79E761B5(process.mainModule, path, 255);
	if (result < 0)
		return result;
	*pathSize = strnlen(path, 255) + 1;
	return *pathSize;
}

void oalGetProcessArmExInfo(SceUID pid, CoredumpArmExInfo *info)
{
	SceKernelProcessInfo process;
	SceKernelModuleInfo module;

	SCE_DBG_ASSERT(info != NULL, 2111);
	memset(info, 0, sizeof(*info));
	memset(&process, 0, sizeof(process));
	process.size = sizeof(process);
	if (sceKernelGetProcessInfo(pid, &process) != 0)
		return;
	if (SceModulemgrForKernel_DAA90093(pid, process.mainModule, &module) < 0)
		return;
	/* exidx_top, exidx_btm, extab_top, extab_btm */
	__builtin_memcpy(info, &module.exidx_top, sizeof(*info));
}

unsigned int oalGetNumOfThreads(SceUID pid)
{
	int count;
	unsigned int capacity;

	count = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (count < 0)
		return 0;
	capacity = memMgrLocalGetFreeSize() / sizeof(SceUID);
	if (capacity >= (unsigned int)count)
		return count;
	return capacity;
}

void oalGetThreadIdList(SceUID pid, SceUID *ids, unsigned int capacity)
{
	SceUID *cursor, threadId;
	unsigned int index;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 2178);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (sceKernelGetThreadIdList(pid, ids, capacity * sizeof(*ids),
		&count) < 0)
		return;
	if (count != 0) {
		cursor = ids;
		index = 0;
		do {
			threadId = *cursor++;
			if (threadId != 0) {
				threadId = SceThreadmgrForDriver_F311808F(threadId);
				if (threadId < 0)
					threadId = 0;
			}
			cursor[-1] = threadId;
			index++;
		} while (index < count);
	}
	if (capacity > count)
		memset(ids + count, 0, (capacity - count) * sizeof(*ids));
}

void oalFormatThreadInfo(CoredumpThreadInfo *info,
	const CoredumpThreadSnapshot *snapshot)
{
	const ThreadmgrThreadInfoForDebugger *thread = &snapshot->info;
	unsigned int index;

	info->formatSize[0] = sizeof(*info) & 0xff;
	info->initAttr = thread->initAttr;
	info->attr = thread->attr;
	info->status = thread->status;
	info->entry = thread->entry;
	info->userStackBase = thread->userStackBase;
	info->userStackSize = thread->userStackSize;
	info->argSize = thread->argSize;
	info->pArgBlock = thread->pArgBlock;
	info->initPriority = thread->initPriority;
	info->currentPriority = thread->currentPriority;
	info->initCpuAffinityMask = thread->initCpuAffinityMask;
	info->cpuAffinityMask = thread->cpuAffinityMask;
	info->lastCpuId = thread->lastCpuId;
	info->waitType = thread->waitType;
	info->waitId = thread->waitId;
	info->runClocksLow = thread->runClocksLow;
	info->runClocksHigh = thread->runClocksHigh;
	info->formatSize[1] = 0;
	info->formatSize[2] = 0;
	info->formatSize[3] = 0;
	info->exitStatus = thread->exitStatus;
	info->intrPreemptCount = thread->intrPreemptCount;
	info->threadPreemptCount = thread->threadPreemptCount;
	info->threadReleaseCount = thread->threadReleaseCount;
	info->changeCpuCount = thread->changeCpuCount;
	info->timeout = thread->timeout;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
	info->formatVersion[0] = 1;
	if (thread->status == 8) {
		for (index = 0; index < 4; index++)
			info->waitDetail[index] = thread->waitDetail[index];
		for (index = 0; index < 4; index++)
			info->waitDetail[index + 4] = thread->reserved2[index];
		info->waitDetail[8] = snapshot->waitDetailCaptureTail;
		if ((info->waitType & 0x7fffffff) == 0x20000) {
			info->waitDetail[0] = info->waitDetail[1];
			info->waitDetail[1] = info->waitDetail[2];
			info->waitDetail[2] = info->waitDetail[3];
			info->waitDetail[3] = 0;
		}
	}
	strncpy(info->name, thread->name, sizeof(info->name));
}

void oalGetThreadInfo(SceUID pid, SceUID threadId,
	const CoredumpThreadStopEvent *stopEvent, CoredumpThreadInfo *info)
{
	ThreadmgrDebugRegisters registers[2];
	CoredumpThreadSnapshot snapshot;
	SceUID guid;
	uint32_t status;
	uintptr_t pc;

	SCE_DBG_ASSERT(info != NULL, 2275);
	SCE_DBG_ASSERT(stopEvent != NULL, 2276);
	memset(info, 0, sizeof(*info));
	info->threadId = threadId;
	snapshot.info.size = sizeof(snapshot.info);
	guid = SceSysmemForDriver_45D22597(pid, threadId);
	if (coredumpGetCachedThreadInfo(pid, threadId, info) != 0 &&
		SceThreadmgrForDriver_DD8D9429(guid, 0, &snapshot.info) == 0)
		oalFormatThreadInfo(info, &snapshot);
	if (guid < 0)
		return;
	_getThreadStopReason(guid, stopEvent, &info->stopReason);
	if (SceThreadmgrForDriver_5022689D(guid, registers) != 0)
		return;
	if ((registers[0].cpsr & 0x1f) != 0x10) {
		status = info->status;
		pc = registers[1].pc;
		status |= 0x80000000;
		info->pc = pc;
		info->status = status;
	} else {
		info->pc = registers[0].pc;
	}
}

int oalGetStackInfo(SceUID pid, SceUID threadId, CoredumpStackInfo *info)
{
	CoredumpThreadSnapshot snapshot;
	int result;

	SCE_DBG_ASSERT(info != NULL, 2431);
	memset(info, 0, sizeof(*info));
	info->threadId = threadId;
	snapshot.info.size = sizeof(snapshot.info);
	result = SceThreadmgrForDriver_DD8D9429(SceSysmemForDriver_45D22597(pid, threadId),
		1, &snapshot.info);
	if (result == 0) {
		info->userStackMaxUsed = snapshot.info.userStackMaxUsed;
		info->userStackUsed = snapshot.info.userStackUsed;
	}
	return result;
}

int oalGetThreadRegInfo(SceUID pid, SceUID threadId,
	CoredumpThreadRegInfo *info)
{
	ThreadmgrDebugRegisters registers[2];
	uint64_t vfp[32];
	CoredumpThreadSnapshot snapshot;
	ThreadRegisterContext *context;
	struct ThreadmgrVfpContext *vfpContext;
	uint64_t *alignedVfp;
	unsigned int index;
	unsigned char *vfpBytes;
	SceUID guid;
	int result;

	SCE_DBG_ASSERT(info != NULL, 2461);
	memset(info, 0, sizeof(*info));
	info->threadId = threadId;
	info->formatSize[0] = sizeof(*info) & 0xff;
	info->formatSize[1] = sizeof(*info) >> 8;
	info->formatSize[2] = 0;
	info->formatSize[3] = 0;
	guid = SceSysmemForDriver_45D22597(pid, threadId);
	if (SceThreadmgrForDriver_5022689D(guid, registers) == 0) {
		if ((registers[0].cpsr & 0x1f) == 0x10) {
			__builtin_memcpy(&info->registers, &registers[0],
				offsetof(ThreadmgrDebugRegisters, cpsr));
			info->registers.cpsr = registers[0].cpsr;
		} else {
			__builtin_memcpy(&info->registers, &registers[1],
				offsetof(ThreadmgrDebugRegisters, cpsr));
			info->registers.cpsr = registers[1].cpsr;
		}
		info->registers.fpscr = registers[0].fpscr;
	}
	result = SceThreadmgrForDriver_5CDE387A(guid, vfp);
	if (result == 0) {
		if (((uintptr_t)info->vfp & 15) == 0) {
			alignedVfp = (uint64_t *)info->vfp;
			for (index = 0; index < 32; index++)
				alignedVfp[index] = vfp[index];
		} else {
			__builtin_memcpy(info->vfp, vfp, sizeof(vfp));
		}
	} else if (result == (int)0x80020008) {
		if (((uintptr_t)info->vfp & 15) == 0) {
			alignedVfp = (uint64_t *)info->vfp;
			for (index = 0; index < 32; index++)
				alignedVfp[index] = UINT64_C(0x7ff8dead7f80dead);
		} else {
			vfpBytes = (unsigned char *)info->vfp;
			for (index = 0; index < 32; index++) {
				vfpBytes[0] = 0xad;
				vfpBytes[1] = 0xde;
				vfpBytes[2] = 0x80;
				vfpBytes[3] = 0x7f;
				vfpBytes[4] = 0xad;
				vfpBytes[5] = 0xde;
				vfpBytes[6] = 0xf8;
				vfpBytes[7] = 0x7f;
				vfpBytes += 8;
			}
		}
	}
	snapshot.info.size = sizeof(snapshot.info);
	result = SceThreadmgrForDriver_DD8D9429(guid, 0, &snapshot.info);
	if (result == 0) {
		result = snapshot.info.tls;
		context = (ThreadRegisterContext *)snapshot.info.registers;
		vfpContext = (struct ThreadmgrVfpContext *)snapshot.info.vfpContext;
		info->tls = snapshot.info.tls;
		info->fpexc = vfpContext->fpexc;
		info->tpidrurw = context->tpidrurw;
		info->cpacr = context->cpacr;
		info->userDacr = context->userDacr;
		info->savedControl = (context->reserved0 & 0x1fc) | 0x01371002;
		info->ifsr = context->ifsr;
		info->ifar = context->ifar;
		info->dfsr = context->dfsr;
		info->dfar = context->dfar;
	}
	return result;
}

int oalGetNumOfModulesAndSegments(SceUID pid, int *numModules,
	unsigned int *numSegments, SceKernelModuleInfoForDebugger *modules,
	SceSize capacity)
{
	SceKernelModuleInfoForDebugger *module;
	uintptr_t end;
	int count, moduleCount, result, outOfRange;
	unsigned int segmentCount;

	SCE_DBG_ASSERT(numModules != NULL && numSegments != NULL && modules != NULL,
		2558);
	count = capacity / (sizeof(*modules) +
		4 * sizeof(SceKernelSegmentInfoForDebugger) +
		sizeof(SceKernelModuleExidxInfo));
	result = sceKernelGetModuleInfoForDebugger(pid, modules, &count);
	if (result < 0)
		return result;
	end = (uintptr_t)modules + capacity;
	moduleCount = 0;
	segmentCount = 0;
	module = modules;
	if (count > 0 && (uintptr_t)module < end) {
		do {
			moduleCount++;
			segmentCount += module->nSegments;
			module = (SceKernelModuleInfoForDebugger *)
				((char *)module + module->size);
			if (moduleCount == count)
				break;
			outOfRange = (uintptr_t)module < (uintptr_t)modules;
			if (end <= (uintptr_t)module)
				outOfRange = 1;
		} while (!outOfRange);
	}
	*numModules = moduleCount;
	*numSegments = segmentCount;
	return 0;
}

int oalEachModuleInfo(SceUID pid,
	int (*callback)(CoredumpModuleInfo *info,
		SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
		SceKernelModuleExidxInfo *exidx, void *context),
	SceKernelModuleInfoForDebugger *modules, SceSize capacity, void *context)
{
	CoredumpModuleInfo info;
	SceKernelSegmentInfoForDebugger segments[4];
	SceKernelModuleExidxInfo exidx;
	SceKernelModuleInfoForDebugger *module;
	uintptr_t end;
	unsigned int segment;
	int count, index, outOfRange;

	SCE_DBG_ASSERT(callback != NULL && modules != NULL, 2613);
	count = capacity / (sizeof(*modules) + sizeof(segments) + sizeof(exidx));
	if (count == 0)
		return 0;
	memset(modules, 0, capacity);
	if (sceKernelGetModuleInfoForDebugger(pid, modules, &count) < 0)
		return 0;
	end = (uintptr_t)modules + capacity;
	if (count <= 0 || (uintptr_t)modules >= end)
		return 0;
	index = 0;
	module = modules;
	do {
		memset(&info, 0, sizeof(info));
		memset(segments, 0, sizeof(segments));
		info.version[0] = module->version;
		info.version[1] = module->version >> 8;
		exidx = (SceKernelModuleExidxInfo){ 0 };
		info.flags = module->flags;
		info.type = module->type;
		info.moduleId = module->modid;
		info.sdkVersion = module->sdkVersion;
		info.moduleStart = (uintptr_t)module->moduleStart;
		info.reservedStart = module->reserved1;
		info.moduleStop = (uintptr_t)module->moduleStop;
		info.moduleExit = (uintptr_t)module->moduleExit;
		strncpy(info.name, module->name, 26);
		info.status = module->status;
		info.fingerprint = module->fingerprint;
		info.nSegments = module->nSegments;
		for (segment = 0; segment != module->nSegments; segment++) {
			segments[segment].perms = module->segment[segment].perms;
			segments[segment].vaddr = module->segment[segment].vaddr;
			segments[segment].memsz = module->segment[segment].memsz;
			segments[segment].align = module->segment[segment].align;
		}
		exidx = *(SceKernelModuleExidxInfo *)(module->segment + module->nSegments);
		index++;
		callback(&info, segments, info.nSegments, &exidx, context);
		module = (SceKernelModuleInfoForDebugger *)
			((char *)module + module->size);
		if (count <= index)
			break;
		outOfRange = (uintptr_t)module < (uintptr_t)modules;
		if (end <= (uintptr_t)module)
			outOfRange = 1;
	} while (!outOfRange);
	return 0;
}

int oalGetNumOfLibs(SceUID pid)
{
	int count = sceKernelGetLibraryList(pid, NULL, NULL);

	if (count < 0)
		count = 0;
	return count;
}

void oalGetLibIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 2726);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	count = capacity;
	sceKernelGetLibraryList(pid, ids, &count);
}

int oalGetLibInfo(SceUID pid, SceUID libraryId, CoredumpLibraryInfo *info)
{
	SceKernelLibraryInfoForDebugger library;
	int result;

	SCE_DBG_ASSERT(info != NULL, 2749);
	memset(info, 0, sizeof(*info));
	library.size = sizeof(library);
	result = sceKernelGetLibraryInfoForDebugger(pid, libraryId, &library);
	if (result >= 0) {
		info->libraryId = (library.libid & 0x40000000) ?
			library.libid : -1;
		info->flags = library.attr;
		info->numFunctions = library.nfunc;
		info->numVariables = library.nvar;
		result = library.modid;
		info->moduleId = result;
		info->numClients = library.clientCount;
		info->reservedVersion[0] = 0;
		info->reservedVersion[1] = 0;
		info->reservedVersion[2] = 0;
		info->reservedVersion[3] = 0;
	}
	return result;
}

unsigned int oalGetLibNumFuncAndVar(SceUID pid, const CoredumpLibraryInfo *info)
{
	SCE_DBG_ASSERT(info != NULL, 2780);
	return info->numVariables + info->numFunctions;
}

void oalGetLibExportInfo(SceUID pid, SceUID libraryId,
	SceKernelLibraryExportEntry *entries, SceSize capacity, SceSize skip)
{
	SceSize count;

	SCE_DBG_ASSERT(entries != NULL, 2796);
	if (capacity == 0)
		return;
	memset(entries, 0, capacity * sizeof(*entries));
	count = capacity;
	sceKernelGetLibraryExportInfoForDebugger(pid, libraryId, entries, &count, skip);
}

unsigned int oalGetLibNumClient(SceUID pid, const CoredumpLibraryInfo *info)
{
	SCE_DBG_ASSERT(info != NULL, 2821);
	return info->numClients;
}

void oalGetLibClientInfo(SceUID pid, SceUID libraryId, SceUID *moduleIds,
	SceSize capacity)
{
	SceSize count;

	SCE_DBG_ASSERT(moduleIds != NULL, 2829);
	if (capacity == 0)
		return;
	memset(moduleIds, 0, capacity * sizeof(*moduleIds));
	count = capacity;
	sceKernelGetLibraryClientList(pid, libraryId, moduleIds, &count, 0);
}

void oalGetLibName(SceUID pid, SceUID libraryId, char *name, SceSize *nameSize)
{
	SceKernelLibraryInfoForDebugger library;
	SceSize count;

	SCE_DBG_ASSERT(name != NULL, 2849);
	SCE_DBG_ASSERT(nameSize != NULL, 2850);
	memset(name, 0, 256);
	*nameSize = 0;
	library.size = sizeof(library);
	if (sceKernelGetLibraryInfoForDebugger(pid, libraryId, &library) < 0)
		return;
	count = strnlen(library.libname, 255) + 1;
	memcpy(name, library.libname, count);
	*nameSize = count;
}

int oalGetNumOfMemBlocks(SceUID pid)
{
	int count;

	count = scePUIDGetUIDVectorByClass(pid, SceSysmemForKernel_86681B64(),
		5, NULL, 0, NULL);
	if (count < 0)
		count = 0;
	return count;
}

void oalGetMemBlockIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 2889);
	if (capacity == 0)
		return;
	cls = SceSysmemForKernel_86681B64();
	memset(ids, 0, capacity * sizeof(*ids));
	scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

void oalGetMemBlockInfo(SceUID pid, SceUID uid, CoredumpMemBlockInfo *info)
{
	MemBlockPhyInfo block;
	int result;

	SCE_DBG_ASSERT(info != NULL, 2916);
	memset(info, 0, sizeof(*info));
	result = SceSysmemForDriver_45D22597(pid, uid);
	if (result >= 0) {
		memset(&block, 0, sizeof(block));
		block.ex.size = sizeof(block);
		result = SceSysmemForDriver_A73CFFEF(result, 5, &block);
		if (result >= 0) {
			info->uid = uid;
			strncpy(info->name, block.ex.details.name, 32);
			info->vbase = block.ex.details.vbase;
			result = block.ex.details.size;
			info->size = result;
			info->type = block.ex.details.type;
			info->mapSize = block.ex.details.mapSize;
			info->headFree = block.ex.details.headFree;
			info->tailFree = block.ex.details.tailFree;
		}
	}
}

/* the partitions oalEachOfAllMemBlockInfo has walked: shared ones are walked once */
static SceUInt32 numVisitedPartitions;
static void *visitedPartitions[64];

/*
 * Calls callback for every memory block of the process's user partitions (every
 * partition for the kernel), blocks being the work buffer for the block lists.
 */
int oalEachOfAllMemBlockInfo(SceUID pid,
	int (*callback)(CoredumpMemBlockInfo *info, void *context), void *context,
	MemBlockInfoEx *blocks, SceSize size)
{
	SceKernelProcessInfo process;
	AddressSpaceInfo space;
	CoredumpMemBlockInfo info;
	SceUInt32 count, i, k;
	uintptr_t address;
	void *partition;
	int j, result;

	SCE_DBG_ASSERT(callback != NULL && blocks != NULL, 2959);
	process.size = sizeof(process);
	if (sceKernelGetProcessInfo(pid, &process) < 0)
		return 0;
	space.size = sizeof(space);
	result = SceSysmemForKernel_C3EF4055(process.addressSpace, &space);
	if (result < 0)
		return result;
	numVisitedPartitions = 0;
	for (i = 0; i < space.partitionCount; i++) {
		if (pid != 0x10005 && space.partition[i].vbase < 0x40000000)
			continue;
		partition = space.partition[i].partition;
		for (k = 0; k < numVisitedPartitions; k++) {
			if (visitedPartitions[k] == partition)
				break;
		}
		if (k < numVisitedPartitions || numVisitedPartitions == 64)
			continue;
		visitedPartitions[numVisitedPartitions++] = partition;
		address = 0;
		for (;;) {
			result = SceSysmemForKernel_AD19285A(partition, address,
				0xffffffff, 5, blocks, size, &count);
			if (result < 0)
				return result;
			if (count == 0)
				break;
			for (j = 0; j < (int)count; j++) {
				memset(&info, 0, sizeof(info));
				strncpy(info.name, blocks[j].details.name, sizeof(info.name));
				info.type = blocks[j].details.type;
				info.vbase = blocks[j].details.vbase;
				info.size = blocks[j].details.size;
				info.mapSize = blocks[j].details.mapSize;
				info.headFree = blocks[j].details.headFree;
				info.tailFree = blocks[j].details.tailFree;
				result = callback(&info, context);
				if (result != 0)
					return result;
			}
			address = blocks[count - 1].details.vbase +
				blocks[count - 1].details.size;
		}
	}
	return 0;
}

int oalEachFileInfo(SceUID pid,
	int (*callback)(CoredumpFileInfo *info, SceSize resolvedPathSize,
		const char *resolvedPath, SceSize resolvedPathPadding,
		SceSize openPathSize, const char *openPath, SceSize openPathPadding,
		void *context),
	void *context, SceIoFdInfo *fds, SceSize capacity)
{
	CoredumpFileInfo info;
	SceIofileInfo file;
	SceSize count = 0;
	SceSize resolvedSize, openSize;
	SceUID fd;
	int index, result;

	SCE_DBG_ASSERT(callback != NULL && fds != NULL, 3102);
	if (sceIoGetPUIDFdListForDebugger(5, fds, capacity, &count) < 0 ||
		(int)count <= 0)
		return 0;
	index = 0;
	do {
		if (fds->pid == pid) {
			memset(&file, 0, sizeof(file));
			if (SceIofilemgrForDriver_A1EF2648(fds->fd, pid, &file) >= 0) {
				fd = fds->fd;
				memset(&info, 0, sizeof(info));
				info.fd = fd;
				info.isDir = file.isDir;
				info.flags = file.flags;
				info.pid = file.pid;
				info.mode = file.mode;
				info.position = file.position;
				info.size = file.size;
				info.ioSchedData = file.ioSchedData;
				resolvedSize = strnlen(file.resolvedOpenPath, 1024) + 1;
				openSize = strnlen(file.openPath, 1024) + 1;
				result = callback(&info, resolvedSize,
					file.resolvedOpenPath,
					((resolvedSize + 3) & ~3) - resolvedSize,
					openSize, file.openPath,
					((openSize + 3) & ~3) - openSize, context);
				if (result != 0)
					return result;
			}
		}
		index++;
		fds++;
	} while (index < (int)count);
	return 0;
}

unsigned int oalGetNumOfSyncObj(int type, SceUID pid)
{
	SceSize capacity;
	int count;

	switch (type) {
	case 1:
		count = SceThreadmgrForDriver_477F7C8A(pid, 5, NULL, 0, NULL);
		break;
	case 2:
		count = SceThreadmgrForDriver_0EB0ABF0(pid, 5, NULL, 0, NULL);
		break;
	case 3:
		count = sceKernelGetMutexInfoAllProc(pid, 5, NULL, 0, NULL);
		break;
	case 4:
		count = sceKernelGetLwMutexInfoAllProc(pid, 5, NULL, 0, NULL);
		break;
	case 5:
		count = SceThreadmgrForDriver_CA704239(pid, 5, NULL, 0, NULL);
		break;
	case 6:
		count = SceThreadmgrForDriver_09346D1A(pid, 5, NULL, 0, NULL);
		break;
	case 7:
		count = SceThreadmgrForDriver_E9E50096(pid, 5, NULL, 0, NULL);
		break;
	case 8:
		count = SceThreadmgrForDriver_FF8DA217(pid, 5, NULL, 0, NULL);
		break;
	case 9:
		count = SceThreadmgrForDriver_C327CA7C(pid, 5, NULL, 0, NULL);
		break;
	case 10:
		count = sceKernelGetRWLockInfoAllProc(pid, 5, NULL, 0, NULL);
		break;
	case 11:
		count = SceThreadmgrForDriver_E0C7BCBB(pid, 5, NULL, 0, NULL);
		break;
	default:
		SCE_DBG_ASSERT(0, 3211);
		count = 0;
	}
	if (count < 0)
		return 0;
	capacity = memMgrLocalGetFreeSize();
	if (type != 8) {
		if (capacity <= 7680)
			return 0;
		capacity -= 7680;
	}
	capacity /= sizeof(SceUID);
	return capacity < (unsigned int)count ? capacity : count;
}

static inline void _getSemaphoreIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3322);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDSemaphoreClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getEventFlagIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3371);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDEventFlagClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getMutexIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3419);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDMutexClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getLwMutexIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3468);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDLwMutexClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getCondIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3516);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDCondClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getLwCondIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3564);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDLwCondClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getMsgPipeIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3612);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDMsgPipeClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getCallbackIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3660);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDCallbackClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getTimerIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3708);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDTimerClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getRWLockIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3756);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDRWLockClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

static inline void _getSimpleEventIdList(SceUID pid, SceUID *ids, SceSize capacity)
{
	SceClass *cls;
	SceSize count;

	SCE_DBG_ASSERT(ids != NULL, 3804);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysmemForKernel_7D87F706("SceUIDSimpleEventClass", &cls) >= 0)
		scePUIDGetUIDVectorByClass(pid, cls, 5, ids, capacity, &count);
}

void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids, SceSize capacity)
{
	SCE_DBG_ASSERT(ids != NULL, 3271);
	switch (type) {
	case 1:
		_getSemaphoreIdList(pid, ids, capacity);
		break;
	case 2:
		_getEventFlagIdList(pid, ids, capacity);
		break;
	case 3:
		_getMutexIdList(pid, ids, capacity);
		break;
	case 4:
		_getLwMutexIdList(pid, ids, capacity);
		break;
	case 5:
		_getCondIdList(pid, ids, capacity);
		break;
	case 6:
		_getLwCondIdList(pid, ids, capacity);
		break;
	case 7:
		_getMsgPipeIdList(pid, ids, capacity);
		break;
	case 8:
		_getCallbackIdList(pid, ids, capacity);
		break;
	case 9:
		_getTimerIdList(pid, ids, capacity);
		break;
	case 10:
		_getRWLockIdList(pid, ids, capacity);
		break;
	case 11:
		_getSimpleEventIdList(pid, ids, capacity);
		break;
	default:
		SCE_DBG_ASSERT(0, 3309);
	}
}

int oalGetNumOfWaitingThreads(SceUID pid, SceUID waitId)
{
	ThreadmgrWaitingThread thread;
	SceUID guid;
	int count;

	guid = SceSysmemForDriver_45D22597(pid, waitId);
	if (guid < 0)
		return 0;
	count = coredumpGetCachedWaitingThreadCount(pid, waitId);
	if (count != 0)
		return count;
	count = sceKernelGetWaitingThreadList(guid, &thread, 0, NULL);
	return count & ~(count >> 31);
}

void oalGetWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *list, SceSize capacity)
{
	ThreadmgrWaitingThread *threads;
	SceSize count, i;
	SceUID guid;

	SCE_DBG_ASSERT(list != NULL, 3883);
	if (capacity == 0)
		return;
	memset(list, 0, capacity * sizeof(*list));
	if (coredumpGetCachedWaitingThreadList(pid, waitId, list, capacity) == 0)
		return;
	threads = memMgrLocalAlloc(capacity * sizeof(*threads));
	if (threads == NULL)
		return;
	memset(threads, 0, capacity * sizeof(*threads));
	guid = SceSysmemForDriver_45D22597(pid, waitId);
	if (guid >= 0 && sceKernelGetWaitingThreadList(guid, threads,
		capacity * sizeof(*threads), &count) >= 0) {
		for (i = 0; i < count; i++) {
			list[i].processId = threads[i].processId;
			list[i].userThreadId = threads[i].userThreadId;
		}
	}
	memMgrLocalFree(threads);
}

void oalGetSemaphoreInfo(SceUID pid, SceUID semaId, CoredumpSemaphoreInfo *info)
{
	SceKernelSemaInfo sema;
	SceUID guid;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 3945);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedSemaphoreInfo(pid, semaId, info) == 0)
		return;
	memset(&sema, 0, sizeof(sema));
	sema.size = sizeof(sema);
	guid = SceSysmemForDriver_45D22597(pid, semaId);
	if (guid < 0 || SceThreadmgrForDriver_C99DCF0D(guid, &sema) < 0)
		return;
	if (scePUIDGetAttr(pid, semaId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		attr = sema.attr | 0x80000;
	else
		attr = sema.attr;
	info->pid = pid;
	info->semaId = semaId;
	strncpy(info->name, sema.name, 32);
	info->attr = attr;
	info->initCount = sema.initCount;
	info->currentCount = sema.currentCount;
	info->maxCount = sema.maxCount;
	info->numWaitThreads = sema.numWaitThreads;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
}

void oalGetEventFlagInfo(SceUID pid, SceUID eventFlagId,
	CoredumpEventFlagInfo *info)
{
	SceUID guid;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 3996);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedEventFlagInfo(pid, eventFlagId, info) == 0)
		return;
	SceKernelEventFlagInfo eventFlag = {0};
	eventFlag.size = sizeof(eventFlag);
	guid = SceSysmemForDriver_45D22597(pid, eventFlagId);
	if (guid < 0 || SceThreadmgrForDriver_612277C9(guid, &eventFlag) < 0)
		return;
	if (scePUIDGetAttr(pid, eventFlagId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		attr = eventFlag.attr | 0x80000;
	else
		attr = eventFlag.attr;
	info->eventFlagId = eventFlagId;
	info->pid = pid;
	strncpy(info->name, eventFlag.name, 32);
	info->attr = attr;
	info->currentPattern = eventFlag.currentPattern;
	info->initPattern = eventFlag.initPattern;
	info->numWaitThreads = eventFlag.numWaitThreads;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
}

void oalGetMutexInfo(SceUID pid, SceUID mutexId, CoredumpMutexInfo *info)
{
	SceKernelMutexInfo mutex;
	SceUID guid, owner;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 4047);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedMutexInfo(pid, mutexId, info) == 0)
		return;
	memset(&mutex, 0, sizeof(mutex));
	mutex.size = sizeof(mutex);
	guid = SceSysmemForDriver_45D22597(pid, mutexId);
	if (guid < 0 || sceKernelGetMutexInfo(guid, &mutex) < 0)
		return;
	if (scePUIDGetAttr(pid, mutexId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		mutex.attr |= 0x80000;
	attr = mutex.attr;
	info->mutexId = mutexId;
	info->pid = pid;
	strncpy(info->name, mutex.name, 32);
	info->attr = attr;
	info->currentCount = mutex.currentCount;
	info->initCount = mutex.initCount;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
	owner = mutex.currentOwnerId;
	if (owner != 0) {
		owner = SceThreadmgrForDriver_F311808F(owner);
		if (owner < 0)
			owner = 0;
	}
	info->currentOwnerId = owner;
	info->numWaitThreads = mutex.numWaitThreads;
	info->ceilingPriority = mutex.ceilingPriority;
}

void oalGetLwMutexInfo(SceUID pid, SceUID lwMutexId, CoredumpLwMutexInfo *info)
{
	SceKernelLwMutexInfo mutex;
	SceUID guid, owner;
	int result;

	SCE_DBG_ASSERT(info != NULL, 4100);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedLwMutexInfo(pid, lwMutexId, info) == 0)
		return;
	memset(&mutex, 0, sizeof(mutex));
	mutex.size = sizeof(mutex);
	guid = SceSysmemForDriver_45D22597(pid, lwMutexId);
	if (guid < 0)
		return;
	result = sceKernelGetLwMutexInfoById(guid, &mutex);
	if (result >= 0) {
		info->lwMutexId = lwMutexId;
		strncpy(info->name, mutex.name, 32);
		info->attr = mutex.attr;
		info->work = mutex.pWork;
		info->initCount = mutex.initCount;
		info->currentCount = mutex.currentCount;
		owner = mutex.currentOwnerId;
		if (owner != 0) {
			owner = SceThreadmgrForDriver_F311808F(owner);
			if (owner < 0)
				owner = 0;
		}
		info->currentOwnerId = owner;
		info->numWaitThreads = mutex.numWaitThreads;
	} else if (result == (int)0x80028187) {
		info->lwMutexId = lwMutexId;
		strncpy(info->name, mutex.name, 32);
		info->attr = mutex.attr;
		info->work = mutex.pWork;
		info->initCount = mutex.initCount;
		info->currentCount = mutex.currentCount;
		info->numWaitThreads = mutex.numWaitThreads;
		info->ownerBytes[0] = 0xff;
		info->ownerBytes[1] = 0xff;
		info->ownerBytes[2] = 0xff;
		info->ownerBytes[3] = 0xff;
	}
}

void oalGetCondInfo(SceUID pid, SceUID condId, CoredumpCondInfo *info)
{
	SceUInt32 uidAttr, attr;
	SceUID guid, mutexId;
	SceSize count;

	SCE_DBG_ASSERT(info != NULL, 4152);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedCondInfo(pid, condId, info) == 0)
		return;
	SceKernelCondInfo cond = {0};
	cond.size = sizeof(cond);
	guid = SceSysmemForDriver_45D22597(pid, condId);
	if (guid < 0 || SceThreadmgrForDriver_D1FBDFA6(guid, &cond) < 0)
		return;
	if (scePUIDGetAttr(pid, condId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		cond.attr |= 0x80000;
	attr = cond.attr;
	info->condId = condId;
	info->pid = pid;
	strncpy(info->name, cond.name, 32);
	info->attr = attr;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
	if (PUIDGetPUIDVectorByGUID(pid, cond.mutexId, 5, &mutexId,
		sizeof(mutexId), &count) < 0 || count == 0)
		mutexId = -1;
	info->mutexId = mutexId;
	info->numWaitThreads = cond.numWaitThreads;
}

void oalGetLwCondInfo(SceUID pid, SceUID lwCondId, CoredumpLwCondInfo *info,
	void **work, ThreadmgrLwMutexWork **lwMutexWork)
{
	SceUID guid;
	ThreadmgrLwMutexWork *mutexWork;
	SceUInt32 lwMutexId;
	uint8_t uidBytes[4];

	SCE_DBG_ASSERT(info != NULL && work != NULL && lwMutexWork != NULL, 4222);
	memset(info, 0, sizeof(*info));
	*work = NULL;
	*lwMutexWork = NULL;
	if (coredumpGetCachedLwCondInfo(pid, lwCondId, info, work, lwMutexWork) == 0)
		return;
	SceKernelLwCondInfo cond = {0};
	cond.size = sizeof(cond);
	guid = SceSysmemForDriver_45D22597(pid, lwCondId);
	if (guid < 0 || SceThreadmgrForDriver_C2AFFBD0(guid, &cond) < 0)
		return;
	__builtin_memset(uidBytes, 0, sizeof(uidBytes));
	mutexWork = cond.pLwMutex;
	if (SceSysmemForDriver_9F6E45E3(pid, 16, &mutexWork->uid, 4) == 0)
		sceKernelCopyFromUserProc(pid, uidBytes, &mutexWork->uid, 4);
	lwMutexId = uidBytes[0] + ((SceUInt32)uidBytes[1] << 8) +
		((SceUInt32)uidBytes[2] << 16) + ((SceUInt32)uidBytes[3] << 24);
	info->lwCondId = lwCondId;
	strncpy(info->name, cond.name, 32);
	info->lwMutexId = lwMutexId;
	info->attr = cond.attr;
	info->numWaitThreads = cond.numWaitThreads;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
	*work = cond.pWork;
	*lwMutexWork = cond.pLwMutex;
}

void oalGetMsgPipeInfo(SceUID pid, SceUID msgPipeId, CoredumpMsgPipeInfo *info)
{
	SceKernelMsgPipeInfo msgPipe;
	SceUID guid;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 4290);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedMsgPipeInfo(pid, msgPipeId, info) == 0)
		return;
	memset(&msgPipe, 0, sizeof(msgPipe));
	msgPipe.size = sizeof(msgPipe);
	guid = SceSysmemForDriver_45D22597(pid, msgPipeId);
	if (guid < 0 || SceThreadmgrForDriver_207E0E18(guid, &msgPipe) < 0)
		return;
	if (scePUIDGetAttr(pid, msgPipeId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		attr = msgPipe.attr | 0x80000;
	else
		attr = msgPipe.attr;
	info->pid = pid;
	info->msgPipeId = msgPipeId;
	strncpy(info->name, msgPipe.name, 32);
	info->attr = attr;
	info->bufferSize = msgPipe.bufferSize;
	info->freeSize = msgPipe.freeSize;
	info->numSendWaitThreads = msgPipe.numSendWaitThreads;
	info->numReceiveWaitThreads = msgPipe.numReceiveWaitThreads;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
}

void oalGetCallbackInfo(SceUID pid, SceUID callbackId, CoredumpCallbackInfo *info)
{
	SceKernelCallbackInfo callback;
	SceUID guid, threadId;

	SCE_DBG_ASSERT(info != NULL, 4341);
	memset(info, 0, sizeof(*info));
	memset(&callback, 0, sizeof(callback));
	callback.size = sizeof(callback);
	guid = SceSysmemForDriver_45D22597(pid, callbackId);
	if (guid < 0 || SceThreadmgrForDriver_FDC044AC(guid, &callback) < 0)
		return;
	info->callbackId = callbackId;
	info->pid = pid;
	threadId = callback.threadId;
	if (threadId != 0) {
		threadId = SceThreadmgrForDriver_F311808F(threadId);
		if (threadId < 0)
			threadId = 0;
	}
	info->threadId = threadId;
	strncpy(info->name, callback.name, 32);
	info->attr = callback.attr;
	info->entry = callback.callbackFunc;
	info->common = callback.pCommon;
	info->notifyCount = callback.notifyCount;
	info->notifyArg = callback.notifyArg;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
}

void oalGetTimerInfo(SceUID pid, SceUID timerId, CoredumpTimerInfo *info)
{
	SceKernelTimerInfo timer;
	SceUID guid;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 4381);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedTimerInfo(pid, timerId, info) == 0)
		return;
	memset(&timer, 0, sizeof(timer));
	timer.size = sizeof(timer);
	guid = SceSysmemForDriver_45D22597(pid, timerId);
	if (guid < 0 || SceThreadmgrForDriver_19F3E92A(guid, &timer) < 0)
		return;
	if (scePUIDGetAttr(pid, timerId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		attr = timer.attr | 0x80000;
	else
		attr = timer.attr;
	info->timerId = timerId;
	info->pid = pid;
	strncpy(info->name, timer.name, 32);
	__builtin_memcpy(&info->baseTime, &timer.baseTime, sizeof(info->baseTime));
	info->attr = attr;
	__builtin_memcpy(&info->currentTime, &timer.currentTime, sizeof(info->currentTime));
	info->fActive = timer.fActive;
	info->type = timer.type;
	__builtin_memcpy(&info->schedule, &timer.schedule, sizeof(info->schedule));
	info->fRepeat = timer.fRepeat;
	info->numWaitThreads = timer.numWaitThreads;
	__builtin_memcpy(&info->interval, &timer.interval, sizeof(info->interval));
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
}

void oalGetRWLockInfo(SceUID pid, SceUID rwLockId, CoredumpRWLockInfo *info)
{
	SceKernelRWLockInfo rwLock;
	ThreadmgrWaitingThread thread;
	SceUID guid, owner;
	SceUInt32 uidAttr;
	int count;

	SCE_DBG_ASSERT(info != NULL, 4438);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedRWLockInfo(pid, rwLockId, info) == 0)
		return;
	memset(&rwLock, 0, sizeof(rwLock));
	rwLock.size = sizeof(rwLock);
	guid = SceSysmemForDriver_45D22597(pid, rwLockId);
	if (guid < 0 || sceKernelGetRWLockInfo(guid, &rwLock) < 0)
		return;
	if (scePUIDGetAttr(pid, rwLockId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		rwLock.attr |= 0x80000;
	guid = SceSysmemForDriver_45D22597(pid, rwLockId);
	if (guid >= 0 &&
	    ((count = coredumpGetCachedWaitingThreadCount(pid, rwLockId)) != 0 ||
	     (count = sceKernelGetWaitingThreadList(guid, &thread, 0, NULL)) >= 0))
		info->size = sizeof(*info) + sizeof(CoredumpWaitingThread) *
			(SceSize)count;
	else
		info->size = sizeof(*info);
	info->pid = pid;
	info->rwLockId = rwLockId;
	strncpy(info->name, rwLock.name, 32);
	info->attr = rwLock.attr;
	info->numReadWaitThreads = rwLock.numReadWaitThreads;
	info->numWriteWaitThreads = rwLock.numWriteWaitThreads;
	info->formatVersion[0] = 0;
	info->formatVersion[1] = 0;
	info->formatVersion[2] = 0;
	info->formatVersion[3] = 0;
	owner = rwLock.writeOwnerId;
	if (owner != 0) {
		owner = SceThreadmgrForDriver_F311808F(owner);
		if (owner < 0)
			owner = 0;
	}
	info->currentOwnerId = owner;
	info->lockCount = rwLock.lockCount;
}

void oalGetCommonEventInfo(SceUID pid, SceUID eventId,
	CoredumpCommonEventInfo *info)
{
	SceKernelEventInfo event;
	SceUID guid;

	SCE_DBG_ASSERT(info != NULL, 4493);
	memset(&event, 0, sizeof(event));
	event.size = sizeof(event);
	guid = SceSysmemForDriver_45D22597(pid, eventId);
	if (guid < 0 || SceThreadmgrForDriver_CBA7FAAA(guid, &event) < 0)
		return;
	info->pattern = event.eventPattern;
	info->userData[0] = event.userData;
	info->userData[1] = event.userData >> 32;
}

void oalGetSimpleEventInfo(SceUID pid, SceUID eventId,
	CoredumpSimpleEventInfo *info)
{
	SceKernelEventInfo event;
	SceUID guid;
	SceUInt32 uidAttr, attr;

	SCE_DBG_ASSERT(info != NULL, 4522);
	memset(info, 0, sizeof(*info));
	if (coredumpGetCachedSimpleEventInfo(pid, eventId, info) == 0)
		return;
	memset(&event, 0, sizeof(event));
	event.size = sizeof(event);
	guid = SceSysmemForDriver_45D22597(pid, eventId);
	if (guid < 0 || SceThreadmgrForDriver_CBA7FAAA(guid, &event) < 0)
		return;
	if (scePUIDGetAttr(pid, eventId, &uidAttr) >= 0 && (uidAttr & 0x80000))
		attr = event.attr | 0x80000;
	else
		attr = event.attr;
	info->eventId = eventId;
	__builtin_memcpy(info->name, event.name, sizeof(info->name));
	info->numWaitThreads = event.numWaitThreads;
	info->pid = pid;
	info->attr = attr;
	info->pattern = event.eventPattern;
	__builtin_memcpy(info->userData, &event.userData, sizeof(info->userData));
}

int oalGetNumAndTotalSizeOfMetaData(SceUID pid, int *numMetaData,
	SceSize *totalSize, SceKernelModuleInfoForDebugger *modules,
	SceSize capacity)
{
	SceKernelModuleInfoForDebugger *module;
	void *metadata;
	SceSize metadataSize, size;
	uintptr_t end;
	int count, index, num, result, outOfRange;

	SCE_DBG_ASSERT(numMetaData != NULL && totalSize != NULL && modules != NULL,
		4584);
	count = capacity / (sizeof(*modules) +
		4 * sizeof(SceKernelSegmentInfoForDebugger) +
		sizeof(SceKernelModuleExidxInfo));
	if (count == 0)
		return 0;
	memset(modules, 0, capacity);
	result = sceKernelGetModuleInfoForDebugger(pid, modules, &count);
	if (result < 0)
		return result;
	end = (uintptr_t)modules + capacity;
	num = 0;
	size = 0;
	module = modules;
	index = 0;
	if (count > 0 && (uintptr_t)module < end) {
		do {
			result = sceKernelGetMetaDataForDebugger(pid, module->modid,
				&metadata, &metadataSize);
			if (result == 0 || result == (int)0x8002d01c) {
				num++;
				size += (metadataSize + 3) & ~3U;
			}
			index++;
			module = (SceKernelModuleInfoForDebugger *)
				((char *)module + module->size);
			if (index >= count)
				break;
			outOfRange = (uintptr_t)module < (uintptr_t)modules;
			if (end <= (uintptr_t)module)
				outOfRange = 1;
		} while (!outOfRange);
	}
	*numMetaData = num;
	*totalSize = size;
	return 0;
}

int oalEachMetaDataInfo(SceUID pid,
	int (*callback)(CoredumpMetaDataInfo *info, void *metadata,
		SceSize paddingSize, void *context),
	SceKernelModuleInfoForDebugger *modules, SceSize capacity, void *context)
{
	SceKernelModuleInfoForDebugger *module;
	CoredumpMetaDataInfo info;
	void *metadata;
	SceSize size;
	uintptr_t end;
	int count, index, result, outOfRange;

	SCE_DBG_ASSERT(callback != NULL && modules != NULL, 4661);
	count = capacity / (sizeof(*modules) +
		4 * sizeof(SceKernelSegmentInfoForDebugger) +
		sizeof(SceKernelModuleExidxInfo));
	if (count == 0)
		return 0;
	memset(modules, 0, capacity);
	result = sceKernelGetModuleInfoForDebugger(pid, modules, &count);
	if (result < 0)
		return result;
	end = (uintptr_t)modules + capacity;
	module = modules;
	index = 0;
	if (count > 0 && (uintptr_t)module < end) {
		do {
			result = sceKernelGetMetaDataForDebugger(pid, module->modid,
				&metadata, &size);
			if (result == 0 || result == (int)0x8002d01c) {
				info.size = size;
				info.moduleId = module->modid;
				callback(&info, metadata, ((size + 3) & ~3U) - size,
					context);
			}
			index++;
			module = (SceKernelModuleInfoForDebugger *)
				((char *)module + module->size);
			if (index >= count)
				break;
			outOfRange = (uintptr_t)module < (uintptr_t)modules;
			if (end <= (uintptr_t)module)
				outOfRange = 1;
		} while (!outOfRange);
	}
	return 0;
}

int oalGetNumOfFibers(SceUID pid)
{
	int count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 1);

	if (count < 0)
		count = 0;
	return count;
}

int oalEachFiberInfo(SceUID pid,
	int (*callback)(CoredumpFiberInfo *info, void *context),
	SceUInt32 *ids, SceSize capacity, void *context)
{
	UlobjFiberInfo fiber;
	CoredumpFiberInfo info;
	SceUInt32 maxIds = capacity >> 2;
	SceUInt32 skip = 0;
	int count, index, ret;

	SCE_DBG_ASSERT(callback != NULL && ids != NULL, 4757);
	for (;;) {
		count = sceUlobjMgrGetIdForFiber(pid, ids, maxIds, skip, 1);
		if (count <= 0)
			return 0;
		for (index = 0; index != count; index++) {
			memset(&info, 0, sizeof(info));
			if (SceUlobjMgrForDriver_8E200387(pid, ids[index], &fiber) == 0) {
				info.address = fiber.address;
				strncpy(info.name, fiber.name, 31);
				info.state = fiber.state;
				info.addrContext = fiber.addrContext;
				info.sizeContext = fiber.sizeContext;
				info.entry = fiber.entry;
				info.threadId = fiber.threadId;
				info.fpscr = fiber.fpscr;
				info.reg[0] = fiber.reg[0];
				info.reg[1] = fiber.reg[1];
				info.reg[2] = fiber.reg[2];
				info.reg[3] = fiber.reg[3];
				info.reg[4] = fiber.reg[4];
				info.reg[5] = fiber.reg[5];
				info.reg[6] = fiber.reg[6];
				info.reg[7] = fiber.reg[7];
				info.sp = fiber.sp;
				info.lr = fiber.lr;
				info.vfp[0] = fiber.vfp[0];
				info.vfp[1] = fiber.vfp[1];
				info.vfp[2] = fiber.vfp[2];
				info.vfp[3] = fiber.vfp[3];
				info.vfp[4] = fiber.vfp[4];
				info.vfp[5] = fiber.vfp[5];
				info.vfp[6] = fiber.vfp[6];
				info.vfp[7] = fiber.vfp[7];
			}
			ret = callback(&info, context);
			if (ret != 0)
				return ret;
		}
		skip += index;
	}
}

int oalGetNumOfUltRuntimes(SceUID pid)
{
	int count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 3);

	if (count < 0)
		count = 0;
	return count;
}

int oalEachUltRuntimeInfo(SceUID pid,
	int (*callback)(SceUInt32 runtimeId, CoredumpUltRuntimeInfo *info,
		void *context), SceUInt32 *ids, SceSize capacity, void *context)
{
	UlobjUltRuntimeInfo runtime;
	SceUInt32 maxIds = capacity >> 2;
	SceUInt32 skip = 0;
	int count, index, ret;

	SCE_DBG_ASSERT(callback != NULL && ids != NULL, 4844);
	for (;;) {
		count = sceUlobjMgrGetIdForFiber(pid, ids, maxIds, skip, 3);
		if (count <= 0)
			return 0;
		for (index = 0; index != count; index++) {
			CoredumpUltRuntimeInfo info = { 0 };

			if (SceUlobjMgrForDriver_525A9BDB(pid, ids[index],
						 &runtime) == 0) {
				int numUlts;

				info.address = runtime.address;
				strncpy(info.name, runtime.name, 31);
				info.opaque = runtime.opaque;
				info.numUlts = runtime.numUlts;
				numUlts = SceUlobjMgrForDriver_3D6E2EF3(pid, ids[index],
							     NULL, 0, 0);
				if (numUlts < 0)
					numUlts = 0;
				info.numUsedUlts = numUlts;
			}
			ret = callback(ids[index], &info, context);
			if (ret != 0)
				return ret;
		}
		skip += index;
	}
}

int oalEachUltInfo(SceUID pid, SceUIntPtr runtimeId,
	int (*callback)(CoredumpUltInfo *info, void *context),
	SceUInt32 *ids, SceSize capacity, void *context)
{
	UlobjUltInfo ult;
	CoredumpUltInfo info;
	SceUInt32 maxIds = capacity >> 2;
	SceUInt32 skip = 0;
	int count, index, ret;

	SCE_DBG_ASSERT(callback != NULL && ids != NULL, 4928);
	for (;;) {
		count = SceUlobjMgrForDriver_3D6E2EF3(pid, runtimeId, ids,
						 maxIds, skip);
		if (count <= 0)
			return 0;
		for (index = 0; index != count; index++) {
			memset(&info, 0, sizeof(info));
			if (SceUlobjMgrForDriver_FFC8C811(pid, ids[index],
						 runtimeId, &ult) == 0) {
				info.address = ult.address;
				strncpy(info.name, ult.name, 31);
				info.state = ult.state;
				info.opaque0 = ult.opaque0;
				info.opaque1 = ult.opaque1;
				info.threadId = ult.threadId;
				info.opaque2 = ult.opaque2;
				info.fiber = ult.fiber;
			}
			ret = callback(&info, context);
			if (ret != 0)
				return ret;
		}
		skip += index;
	}
}

int oalGetNumOfUltObjects(SceUID pid, int type)
{
	int count;

	switch (type) {
	case 0:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 17);
		break;
	case 1:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 18);
		break;
	case 2:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 19);
		break;
	case 3:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 20);
		break;
	case 4:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 21);
		break;
	case 5:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 22);
		break;
	case 6:
		count = sceUlobjMgrGetIdForFiber(pid, NULL, 0, 0, 23);
		break;
	default:
		return 0;
	}
	if (count < 0)
		count = 0;
	return count;
}

void oalGetUltObjIds(SceUID pid, SceUInt32 *ids, SceUInt32 capacity,
	SceUInt32 skip, int type)
{
	SCE_DBG_ASSERT(ids != NULL, 5021);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	switch (type) {
	case 0:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 17);
		break;
	case 1:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 18);
		break;
	case 2:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 19);
		break;
	case 3:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 20);
		break;
	case 4:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 21);
		break;
	case 5:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 22);
		break;
	case 6:
		sceUlobjMgrGetIdForFiber(pid, ids, capacity, skip, 23);
		break;
	}
}

void oalGetUltMutexInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltMutexInfo *info, SceUInt32 *ids, uint32_t *numIds)
{
	UlobjMutexInfo mutex;

	SCE_DBG_ASSERT(info != NULL, 5064);
	SCE_DBG_ASSERT(ids != NULL, 5065);
	SCE_DBG_ASSERT(numIds != NULL, 5066);
	memset(info, 0, sizeof(*info));
	if (*numIds != 0)
		memset(ids, 0, *numIds * sizeof(*ids));
	if (SceUlobjMgrForDriver_6D2C8928(pid, address, &mutex, ids, numIds) == 0) {
		info->address = mutex.header.address;
		__builtin_memcpy(info->name, mutex.header.name, sizeof(info->name));
		info->attr = mutex.attr;
		info->waitingQueueResourcePool = mutex.waitingQueueResourcePool;
		info->locked = mutex.locked;
		info->ownerThreadType = mutex.ownerThreadType;
		info->ownerThreadId = mutex.ownerThreadId;
		info->numWaitThreads = mutex.numWaitThreads;
	}
}

void oalGetUltCondInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltCondInfo *info, SceUInt32 *ids, uint32_t *numIds)
{
	UlobjCondInfo cond;

	SCE_DBG_ASSERT(info != NULL, 5095);
	SCE_DBG_ASSERT(ids != NULL, 5096);
	SCE_DBG_ASSERT(numIds != NULL, 5097);
	memset(info, 0, sizeof(*info));
	if (*numIds != 0)
		memset(ids, 0, *numIds * sizeof(*ids));
	if (SceUlobjMgrForDriver_362C9488(pid, address, &cond, ids, numIds) == 0) {
		info->address = cond.header.address;
		__builtin_memcpy(info->name, cond.header.name, sizeof(info->name));
		info->reserved = cond.reserved0;
		info->mutex = cond.mutex;
		info->numWaitThreads = cond.numWaitThreads;
	}
}

void oalGetUltSemaInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltSemaInfo *info, SceUInt32 *ids, uint32_t *numIds)
{
	UlobjSemaInfo sema;

	SCE_DBG_ASSERT(info != NULL, 5122);
	SCE_DBG_ASSERT(ids != NULL, 5123);
	SCE_DBG_ASSERT(numIds != NULL, 5124);
	memset(info, 0, sizeof(*info));
	if (*numIds != 0)
		memset(ids, 0, *numIds * sizeof(*ids));
	if (SceUlobjMgrForDriver_74B9199C(pid, address, &sema, ids, numIds) == 0) {
		info->address = sema.header.address;
		__builtin_memcpy(info->name, sema.header.name, sizeof(info->name));
		info->reserved = sema.reserved0;
		info->waitingQueueResourcePool = sema.waitingQueueResourcePool;
		info->count = sema.count;
		info->numWaitThreads = sema.numWaitThreads;
	}
}

void oalGetUltRwlockInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltRwlockInfo *info, SceUInt32 *writerThreadId,
	SceUInt32 maxIds, SceUInt32 *ids, SceUInt32 *numReaders,
	SceUInt32 *numWriters)
{
	UlobjRwlockInfo rwlock;
	SceUInt32 writer;

	SCE_DBG_ASSERT(info != NULL, 5154);
	SCE_DBG_ASSERT(writerThreadId != NULL, 5155);
	SCE_DBG_ASSERT(ids != NULL, 5156);
	SCE_DBG_ASSERT(numReaders != NULL, 5157);
	SCE_DBG_ASSERT(numWriters != NULL, 5158);
	memset(info, 0, sizeof(*info));
	if (maxIds != 0)
		memset(ids, 0, maxIds * sizeof(*ids));
	*numReaders = 0;
	*numWriters = 0;
	if (SceUlobjMgrForDriver_0C29AFEC(pid, address, &rwlock, &writer,
			maxIds, ids, numReaders, numWriters) == 0) {
		info->address = rwlock.header.address;
		__builtin_memcpy(info->name, rwlock.header.name, sizeof(info->name));
		info->reserved = rwlock.reserved0;
		info->waitingQueueResourcePool = rwlock.waitingQueueResourcePool;
		info->state = rwlock.state;
		info->numReaders = rwlock.numReaders;
		info->numReadWaitThreads = rwlock.numReadWaitThreads;
		info->numWriteWaitThreads = rwlock.numWriteWaitThreads;
		*writerThreadId = writer;
	}
}

void oalGetUltQueueInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltQueueInfo *info, SceUInt32 maxIds, SceUInt32 *ids,
	SceUInt32 *numSenders, SceUInt32 *numReceivers, SceUInt32 *numData)
{
	UlobjQueueInfo queue;

	SCE_DBG_ASSERT(info != NULL, 5199);
	SCE_DBG_ASSERT(ids != NULL, 5200);
	SCE_DBG_ASSERT(numSenders != NULL, 5201);
	SCE_DBG_ASSERT(numReceivers != NULL, 5202);
	SCE_DBG_ASSERT(numData != NULL, 5203);
	memset(info, 0, sizeof(*info));
	if (maxIds != 0)
		memset(ids, 0, maxIds * sizeof(*ids));
	*numSenders = 0;
	*numReceivers = 0;
	*numData = 0;
	if (SceUlobjMgrForDriver_5D7A7925(pid, address, &queue, maxIds,
			ids, numSenders, numReceivers, numData) == 0) {
		info->address = queue.header.address;
		__builtin_memcpy(info->name, queue.header.name, sizeof(info->name));
		info->reserved = queue.reserved0;
		info->waitingQueueResourcePool = queue.waitingQueueResourcePool;
		info->queueDataResourcePool = queue.queueDataResourcePool;
		info->dataSize = queue.dataSize;
		info->numSendWaitThreads = queue.numSendWaitThreads;
		info->numReceiveWaitThreads = queue.numReceiveWaitThreads;
		info->numData = queue.numData;
	}
}

void oalGetUltWaitingQueueResourcePoolInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltWaitingQueueResourcePoolInfo *info)
{
	UlobjWaitingQueueResourcePoolInfo pool;

	SCE_DBG_ASSERT(info != NULL, 5240);
	memset(info, 0, sizeof(*info));
	if (SceUlobjMgrForDriver_6BCB4568(pid, address, &pool) == 0) {
		info->address = pool.header.address;
		__builtin_memcpy(info->name, pool.header.name, sizeof(info->name));
		info->numThreads = pool.numThreads;
		info->numFreeThreads = pool.numFreeThreads;
		info->numSyncObjects = pool.numSyncObjects;
		info->numUsedSyncObjects = pool.numUsedSyncObjects;
	}
}

void oalGetUltQueueDataResourcePoolInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltQueueDataResourcePoolInfo *info)
{
	UlobjQueueDataResourcePoolInfo pool;

	SCE_DBG_ASSERT(info != NULL, 5263);
	memset(info, 0, sizeof(*info));
	if (SceUlobjMgrForDriver_ADAAA961(pid, address, &pool) == 0) {
		info->address = pool.header.address;
		__builtin_memcpy(info->name, pool.header.name, sizeof(info->name));
		info->reserved = pool.reserved0;
		info->numData = pool.numData;
		info->numUsedData = pool.numUsedData;
		info->dataSize = pool.dataSize;
		info->numQueues = pool.numQueues;
		info->numUsedQueues = pool.numUsedQueues;
	}
}

/* processes other than the caller's own; all of them for a system program */
int oalGetNumOfExtnlProcs(SceUID pid)
{
	SceKernelProcessInfo *processes;
	SceUInt32 count;
	SceSize size;
	SceUID currentPid;
	int i, num;

	processes = memMgrLocalAllAlloc(&size);
	if (processes == NULL)
		return 0;
	if (SceProcessmgrForDriver_BA0A06A4(5, processes, size, &count) < 0) {
		memMgrLocalFree(processes);
		return 0;
	}
	if (_isSystemProgram(pid) != 0) {
		memMgrLocalFree(processes);
		return count;
	}
	currentPid = sceKernelGetProcessId();
	num = 0;
	for (i = 0; i < (int)count; i++) {
		if (processes[i].pid != currentPid)
			num++;
	}
	memMgrLocalFree(processes);
	return num;
}

void oalGetExtnlProcIdList(SceUID pid, SceUID *ids, int capacity)
{
	SceKernelProcessInfo *processes;
	SceUInt32 count;
	SceSize size;
	SceUID currentPid;
	int systemProgram, i, num;

	SCE_DBG_ASSERT(ids != NULL, 5329);
	memset(ids, 0, capacity * sizeof(*ids));
	processes = memMgrLocalAllAlloc(&size);
	if (processes == NULL)
		return;
	if (SceProcessmgrForDriver_BA0A06A4(5, processes, size, &count) >= 0) {
		systemProgram = _isSystemProgram(pid);
		currentPid = sceKernelGetProcessId();
		if (systemProgram != 0) {
			for (i = 0; i < (int)count && i < capacity; i++)
				ids[i] = processes[i].pid;
		} else {
			for (i = 0, num = 0; i < (int)count && num < capacity; i++) {
				if (processes[i].pid != currentPid)
					ids[num++] = processes[i].pid;
			}
		}
	}
	memMgrLocalFree(processes);
}

void oalGetExtnlProcInfo(SceUID pid, SceUID extnlPid, CoredumpExtnlProcInfo *info)
{
	SceKernelProcessInfo process;
	uint32_t flags;
	SceUInt32 status;

	SCE_DBG_ASSERT(info != NULL, 5366);
	memset(info, 0, sizeof(*info));
	memset(&process, 0, sizeof(process));
	process.size = sizeof(process);
	if (sceKernelGetProcessInfo(extnlPid, &process) != 0)
		return;
	info->pid = process.pid;
	if (_isSystemProgram(pid) != 0)
		info->budgetId = process.budgetId;
	status = 0;
	if (sceKernelGetProcessStatus(extnlPid, &status) < 0) {
		flags = 0;
	} else {
		status = (uint8_t)status;
		flags = _isPlaintextDumpAllowed(extnlPid) ? 0x10000 : 0;
		if (status == 3)
			flags |= 8;
		if (_isSystemProgram(extnlPid) != 0)
			flags |= 4;
		if (status == 2)
			flags |= 2;
		flags |= 1;
	}
	info->flags = flags;
	__builtin_memcpy(info->name, process.name, sizeof(info->name));
	info->cpuAffinityMask = process.cpuAffinity;
	info->entryPoint = (uintptr_t)process.entryPoint;
	SceModulemgrForKernel_337A3908(process.mainModule, &info->fingerprint);
	if (process.parentPid == 0x10005) {
		info->parentPidBytes[0] = -1;
		info->parentPidBytes[1] = -1;
		info->parentPidBytes[2] = -1;
		info->parentPidBytes[3] = -1;
	} else {
		info->parentPid = process.parentPid;
	}
	if (extnlPid == coredumpRun.pid) {
#if FW_DEBUG
		_getAbortedProcessStopReason(extnlPid, &info->stopReason);
#else
		_getAbortedProcessStopReason(&info->stopReason);
#endif
		return;
	}
	_getExternalProcessStopReason(&process, &info->stopReason);
}

int oalGetNumOfBudgets(void)
{
	SceSize size, count;
	void *allocation = memMgrLocalAllAlloc(&size);

	if (allocation == NULL)
		return 0;
	if (SceProcessmgrForKernel_23EF9D8E(5, allocation, size, &count) < 0) {
		memMgrLocalFree(allocation);
		return 0;
	}
	memMgrLocalFree(allocation);
	return count;
}

void oalGetBudgetIdList(SceUID *ids, int capacity)
{
	SceSize size, count;
	SceKernelBudgetInfoForDebugger *budgets;
	int i;

	SCE_DBG_ASSERT(ids != NULL, 5433);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	budgets = memMgrLocalAllAlloc(&size);
	if (budgets == NULL)
		return;
	if (SceProcessmgrForKernel_23EF9D8E(5, budgets, size, &count) >= 0) {
		for (i = 0; i < (int)count && i < capacity; i++)
			ids[i] = budgets[i].budgetId;
	}
	memMgrLocalFree(budgets);
}

void oalGetBudgetInfo(SceUID budgetId, CoredumpBudgetInfo *info,
	CoredumpPhyMemPartInfo *parts, uint32_t *numParts)
{
	SceKernelBudgetInfoForDebugger budget;
	const SceKernelPhyMemPartInfoForDebugger *part;
	SceUInt32 count, capacity, i, j;

	SCE_DBG_ASSERT(info != NULL, 5472);
	SCE_DBG_ASSERT(parts != NULL, 5473);
	SCE_DBG_ASSERT(numParts != NULL, 5474);
	memset(info, 0, sizeof(*info));
	budget.size = sizeof(budget);
	if (sceKernelGetBudgetInfoForDebugger(budgetId, 5, &budget) != 0)
		return;
	count = budget.count;
	info->budgetId = budget.budgetId;
	info->numPhyMemParts = count;
	strncpy(info->name, budget.name, sizeof(info->name));
	if (*numParts == 0)
		return;
	memset(parts, 0, *numParts * sizeof(*parts));
	capacity = *numParts;
	for (i = 0; i < count && i < capacity; i++) {
		part = &budget.part[i].info;
		parts[i].uid = part->uid;
		if (part->name != NULL)
			strncpy(parts[i].name, part->name, sizeof(parts[i].name));
		parts[i].type = part->type;
		parts[i].numSegments = part->segmentCount >= 8 ? 8 : part->segmentCount;
		/* Sony walks every segment, past the eight it has room for */
		for (j = 0; j < part->segmentCount; j++) {
			parts[i].segments[j].pbase = part->segments[j].pbase;
			parts[i].segments[j].psize = part->segments[j].psize;
			parts[i].segments[j].freeSize = part->freeSize;
			parts[i].segments[j].freeSizeMin = part->freeSizeMin;
			parts[i].segments[j].freeUnitCount = part->freeUnitCount;
		}
	}
	*numParts = i;
}

int oalGetNumOfApp(void)
{
	AppMgrAppList apps;
	int count;

	if (SceSysrootForKernel_2A03DFA1(&apps) != 0)
		return 0;
	for (count = 0; count < 16; count++) {
		if (apps.appIds[count] == -1)
			break;
	}
	return count;
}

void oalGetAppIdList(int *ids, SceSize capacity)
{
	AppMgrAppList apps;
	SceSize i;

	SCE_DBG_ASSERT(ids != NULL, 5549);
	if (capacity == 0)
		return;
	memset(ids, 0, capacity * sizeof(*ids));
	if (SceSysrootForKernel_2A03DFA1(&apps) != 0)
		return;
	for (i = 0; i < 16 && i < capacity; i++) {
		if (apps.appIds[i] == -1)
			break;
		ids[i] = apps.appIds[i];
	}
}

void oalGetAppInfo(int appId, CoredumpAppInfo *info, char *path)
{
	AppMgrAppInfo app;

	SCE_DBG_ASSERT(info != NULL, 5579);
	SCE_DBG_ASSERT(path != NULL, 5580);
	memset(info, 0, sizeof(*info));
	memset(path, 0, 256);
	if (SceSysrootForKernel_6D111FA7(appId, &app) != 0)
		return;
	info->appId = app.appId;
	info->launchSourceAppId = app.launchSourceAppId;
	info->processId = app.processId;
	info->launchSourceProcessId = app.launchSourceProcessId;
	strncpy(info->appName, app.appName, sizeof(info->appName));
	info->budgetId = app.budgetId;
	info->launchMode = app.launchMode;
	info->processAttr = app.processAttr;
	info->fdLimit = app.fdLimit;
	info->dirLimit = app.dirLimit;
	memcpy(path, app.appPath, 256);
	info->appPathLength = strnlen(path, 256);
}

void oalGetDeviceInfo(CoredumpDeviceInfo *info, SceUID *renderingPids,
	SceSize *renderingCount, SceUID *touchPids, SceSize *touchCount)
{
	AppMgrAppList apps;
	SceSize i, capacity;

	SCE_DBG_ASSERT(info != NULL, 5615);
	SCE_DBG_ASSERT(renderingPids != NULL, 5616);
	SCE_DBG_ASSERT(touchPids != NULL, 5617);
	memset(info, 0, sizeof(*info));
	SceSysrootForKernel_2A03DFA1(&apps);
	info->soundExclusivePid = apps.device.soundExclusivePid;
	info->mainOutPortOwner = apps.device.mainOutPortOwner;
	info->bgmOutPortOwner = apps.device.bgmOutPortOwner;
	info->voiceOutPortOwner = apps.device.voiceOutPortOwner;
	info->inputPortOwner = apps.device.inputPortOwner;
	info->backRenderPid = apps.device.backRenderPid;
	info->inputOwnerPid = apps.device.inputOwnerPid;
	info->bgmProxyAppId = apps.device.bgmProxyAppId;
	info->shellPid = apps.device.shellPid;
	info->activeAppId = apps.device.activeAppId;
	info->displayOwner0 = apps.device.displayOwner0;
	info->displayOwner1 = apps.device.displayOwner1;
	info->systemChatPid = apps.device.systemChatPid;
	info->backRenderAppId = apps.device.backRenderAppId;
	if (*renderingCount != 0) {
		memset(renderingPids, 0, *renderingCount * sizeof(*renderingPids));
		capacity = *renderingCount;
		for (i = 0; i < 16 && i < capacity; i++) {
			if (apps.device.renderingPids[i] == -1)
				break;
			renderingPids[i] = apps.device.renderingPids[i];
		}
		*renderingCount = i;
	}
	if (*touchCount != 0) {
		memset(touchPids, 0, *touchCount * sizeof(*touchPids));
		capacity = *touchCount;
		for (i = 0; i < 16 && i < capacity; i++) {
			if (apps.device.touchPids[i] == -1)
				break;
			touchPids[i] = apps.device.touchPids[i];
		}
		*touchCount = i;
	}
}

SceSize oalGetGpuInfoSize(void)
{
	return (SceGpuEs4ForDriver_F001AE90() + 3) & ~3U;
}

void oalGetGpuInfo(void *buffer, SceSize size)
{
	SCE_DBG_ASSERT(buffer != NULL, 5686);
	if (size != 0) {
		memset(buffer, 0, size);
		SceGpuEs4ForDriver_14ECCFCD(buffer, size);
	}
}

SceSize oalGetGpuActivitySize(void)
{
	return SceGpuEs4ForDriver_395F7689();
}

void oalGetGpuActivity(void *buffer, SceSize size)
{
	SCE_DBG_ASSERT(buffer != NULL, 5712);
	SceGpuEs4ForDriver_8A3929D2(buffer, size);
}

SceUInt32 oalGetGpuRecoveryCount(void)
{
	return SceGpuEs4ForDriver_18C90B42();
}

int oalCafInit(void)
{
	return SceCoredumpForDriver_9336009B();
}

int oalCafFinal(void)
{
	return SceCoredumpForDriver_C90F61AF();
}

SceUID oalCafContextCreate(void)
{
	return sceCoredumpCafContextCreate();
}

int oalCafContextDestroy(SceUID context)
{
	return sceCoredumpCafContextDestroy(context);
}

int oalCafCreateIV(void *iv)
{
	SCE_DBG_ASSERT(iv != NULL, 5767);
	return SceCoredumpForDriver_E1BCBE8F(iv, 16);
}

int oalCafHeaderInit(SceUID context, int type)
{
	switch (type) {
	case 1:
	case 2:
	case 5:
		return SceCoredumpForDriver_7C8120C5(context, 1);
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

int oalCafHeaderTransform(SceUID context, const void *data, SceSize size)
{
	SCE_DBG_ASSERT(data != NULL, 5805);
	return SceCoredumpForDriver_AE2C2793(context, data, size);
}

int oalCafHeaderFinal(SceUID context, void *digest)
{
	SCE_DBG_ASSERT(digest != NULL, 5817);
	memset(digest, 0, 32);
	return SceCoredumpForDriver_65AA4991(context, digest, 32);
}

int oalCafSegmentInit(SceUID context, int type, const void *iv)
{
	SceUInt32 keySelector;

	SCE_DBG_ASSERT(iv != NULL, 5831);
	switch (type) {
	case 1:
	case 3:
		keySelector = 1;
		break;
	case 2:
	case 4:
		keySelector = 2;
		break;
	case 5:
		keySelector = 0x10000001;
		break;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
	return SceCoredumpForDriver_07185515(context, 1, 1, keySelector, iv, 16);
}

int oalCafSegmentTransform(SceUID context, const void *source,
	void *destination, SceSize size)
{
	SCE_DBG_ASSERT(source != NULL, 5890);
	SCE_DBG_ASSERT(destination != NULL, 5891);
	return SceCoredumpForDriver_FB7AEBFE(context, source, destination, size);
}

int oalCafSegmentFinal(SceUID context, CoredumpCafSectionHash *hash)
{
	SCE_DBG_ASSERT(hash != NULL, 5902);
	memset(hash, 0, sizeof(*hash));
	return SceCoredumpForDriver_DF17420A(context, hash->digest,
		sizeof(hash->digest));
}

int oalCafMakeHeader(CoredumpCafHeader *header, SceUInt64 segmentCount,
	SceUInt64 headerSize, SceUInt64 dataSize, int type)
{
	SCE_DBG_ASSERT(header != NULL, 5922);
	memset(header, 0, sizeof(*header));
	header->segmentCountLow = segmentCount;
	header->segmentCountHigh = segmentCount >> 32;
	header->headerSizeLow = headerSize;
	header->headerSizeHigh = headerSize >> 32;
	header->dataSizeLow = dataSize;
	header->dataSizeHigh = dataSize >> 32;
	header->magic[0] = 0;
	header->magic[1] = 0;
	header->magic[2] = 'F';
	header->magic[3] = 'A';
	header->magic[4] = 'C';
	header->magic[5] = 'E';
	header->magic[6] = 'C';
	header->magic[7] = 'S';
	header->formatVersion[0] = 2;
	header->formatVersion[1] = 0;
	header->formatVersion[2] = 0;
	header->formatVersion[3] = 0;
	header->formatVersion[4] = 0;
	header->formatVersion[5] = 0;
	header->formatVersion[6] = 0;
	header->formatVersion[7] = 0;
	switch (type) {
	case 1:
	case 2:
		header->primaryTypeFlag[1] = 0;
		header->primaryTypeFlag[2] = 0;
		header->primaryTypeFlag[3] = 0;
		header->primaryTypeFlag[4] = 0;
		header->primaryTypeFlag[5] = 0;
		header->primaryTypeFlag[6] = 0;
		header->primaryTypeFlag[7] = 0;
		header->primaryTypeFlag[0] = 1;
		return 0;
	case 3:
	case 4:
	case 5:
		header->primaryTypeFlag[0] = 0;
		header->primaryTypeFlag[1] = 0;
		header->primaryTypeFlag[2] = 0;
		header->primaryTypeFlag[3] = 0;
		header->primaryTypeFlag[4] = 0;
		header->primaryTypeFlag[5] = 0;
		header->primaryTypeFlag[6] = 0;
		header->primaryTypeFlag[7] = 0;
		return 0;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

int oalCafMakeSegmentTable(CoredumpCafSegmentTable *table,
	SceUInt64 segmentId, SceUInt64 fileOffset, SceUInt32 segmentSize,
	SceUInt32 segmentIndex, int type, const CoredumpCafIv *iv)
{
	uint8_t keySelector;
	uint32_t ivFirst, ivSecond, ivThird, ivFourth;

	SCE_DBG_ASSERT(table != NULL && iv != NULL, 5966);
	memset(table, 0, sizeof(*table));
	table->segmentIdLow = segmentId;
	table->segmentIdHigh = segmentId >> 32;
	table->fileOffsetLow = fileOffset;
	table->fileOffsetHigh = fileOffset >> 32;
	table->segmentSize = segmentSize;
	table->segmentIndex = segmentIndex;
	table->hashKeySelector[0] = 1;
	table->hashKeySelector[1] = 0;
	table->hashKeySelector[2] = 0;
	table->hashKeySelector[3] = 0;
	table->hashKeySelector[4] = 0;
	table->hashKeySelector[5] = 0;
	table->hashKeySelector[6] = 0;
	table->hashKeySelector[7] = 0;
	switch (type) {
	case 1:
	case 3:
		keySelector = 1;
		goto setKeySelector;
	case 2:
	case 4:
		keySelector = 2;
setKeySelector:
		table->cipherKeySelector[1] = 0;
		table->cipherKeySelector[2] = 0;
		table->cipherKeySelector[3] = 0;
		table->cipherKeySelector[4] = 0;
		table->cipherKeySelector[5] = 0;
		table->cipherKeySelector[6] = 0;
		table->cipherKeySelector[7] = 0;
		table->cipherKeySelector[0] = keySelector;
		break;
	case 5:
		table->cipherKeySelector[1] = 0;
		table->cipherKeySelector[2] = 0;
		table->cipherKeySelector[4] = 0;
		table->cipherKeySelector[5] = 0;
		table->cipherKeySelector[6] = 0;
		table->cipherKeySelector[7] = 0;
		table->cipherKeySelector[0] = 1;
		table->cipherKeySelector[3] = 0x10;
		break;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ivFirst = iv->words[0];
	ivSecond = iv->words[1];
	ivThird = iv->words[2];
	ivFourth = iv->words[3];
	table->iv.words[0] = ivFirst;
	table->iv.words[1] = ivSecond;
	table->iv.words[2] = ivThird;
	table->iv.words[3] = ivFourth;
	return 0;
}

void oalGetCdnAccessParam(uint32_t appVersion, uint32_t stopReason,
	CoredumpCdnAccessParam *param, const CoredumpCdnPcAndLrInfo *location)
{
	uintptr_t pc, lr;

	SCE_DBG_ASSERT(param != NULL, 6005);
	SCE_DBG_ASSERT(location != NULL, 6006);
	memset(param, 0, sizeof(*param));
	param->magic[0] = 'C';
	param->magic[1] = 'D';
	param->magic[2] = 'A';
	param->magic[3] = 'P';
	pc = location->pc;
	lr = location->lr;
	param->appVersion = appVersion;
	param->stopReason = stopReason;
	param->formatVersion[0] = 1;
	param->systemVersion[0] = FW_VERSION & 255;
	param->systemVersion[1] = FW_VERSION >> 8 & 255;
	param->systemVersion[2] = FW_VERSION >> 16 & 255;
	param->systemVersion[3] = FW_VERSION >> 24;
	param->pc = pc;
	param->lr = lr;
	param->formatVersion[1] = 0;
	param->formatVersion[2] = 0;
	param->formatVersion[3] = 0;
	param->reservedPcHigh = 0;
	param->reservedLrHigh = 0;
	param->pcModuleFingerprint = location->pcModuleFingerprint;
	param->lrModuleFingerprint = location->lrModuleFingerprint;
}

int oalGetCdnPcAndLrInfo(SceUID pid, const CoredumpThreadStopEvent *stopEvent,
	CoredumpCdnPcAndLrInfo *location)
{
	SCE_DBG_ASSERT(stopEvent != NULL, 6033);
	SCE_DBG_ASSERT(location != NULL, 6034);
	return _getCdnPcAndLrInfo(pid, stopEvent, location);
}

SceUID oalGetAbortedThreadID(SceUID pid,
	const CoredumpThreadStopEvent *stopEvent)
{
	CoredumpThreadSnapshot threadInfo;
	CoredumpStopReason stopReason;
	SceUID *threadIds;
	SceUID threadId = -1;
	SceUID userThreadId;
	SceSize copiedCount;
	SceSize allocationSize;
	unsigned int i;
	uint32_t reason;
	int count;

	SCE_DBG_ASSERT(stopEvent != NULL, 6047);
	count = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (count <= 0)
		return -1;
	allocationSize = count * sizeof(*threadIds);
	threadIds = memMgrLocalAlloc(allocationSize);
	if (threadIds == NULL)
		return -1;
	if (sceKernelGetThreadIdList(pid, threadIds, allocationSize,
		&copiedCount) < 0) {
		memMgrLocalFree(threadIds);
		return -1;
	}
	for (i = 0; i < copiedCount; i++) {
		threadId = threadIds[i];
		threadInfo.info.size = sizeof(threadInfo.info);
		if (SceThreadmgrForDriver_DD8D9429(threadId, 0,
			&threadInfo.info) == 0) {
			_getThreadStopReason(threadId, stopEvent, &stopReason);
			__builtin_memcpy(&reason, &stopReason.reason, sizeof(reason));
			if (reason != 0)
				goto found;
		}
	}
	threadId = -1;
found:
	memMgrLocalFree(threadIds);
	if (threadId == 0)
		return 0;
	userThreadId = SceThreadmgrForDriver_F311808F(threadId);
	if (userThreadId < 0)
		userThreadId = 0;
	return userThreadId;
}

int oalPrintf(const char *fmt, ...)
{
	va_list args;
	int result;

	va_start(args, fmt);
	result = SceDebugForDriver_411C0733(fmt, args);
	va_end(args);
	return result;
}

uint32_t oalGetDumpLevelFromSettings(void)
{
	int dumpLevel;

	if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP", "dump_level", &dumpLevel) != 0)
		return 15;
	return dumpLevel == 1 ? 7920 : 15;
}

void oalGetDestHostPathFromSettings(char *path)
{
	SCE_DBG_ASSERT(path != NULL, 6302);
	memset(path, 0, 1024);
	if (sceRegMgrGetKeyStr("/CONFIG/COREDUMP", "dest_hostpath", path, 1024) != 0)
		memset(path, 0, 1024);
}

int oalUnlinkNormalSyscall(SceUID pid)
{
	return sceKernelUnlinkNormalSyscall(pid);
}

int oalRegisterCoredumpHandler(SceCoredumpHandler handler,
	SceSize stackSize, void *pCommon)
{
	int result;

	if (handler == NULL)
		return SCE_COREDUMP_ERROR_PARAM;
	result = sceKernelRegisterCoredumpHandler((void *)handler, stackSize, pCommon);
	if (result == 0)
		return 0;
	return result == SCE_COREDUMP_ERROR_ALREADY_REGISTERED ?
		SCE_COREDUMP_ERROR_ALREADY_REGISTERED : SCE_COREDUMP_ERROR_PARAM;
}

int oalUnregisterCoredumpHandler(void)
{
	return SceProcessmgrForDriver_33B3D026();
}

SceUID oalCallCoredumpHandler(SceUID pid)
{
	return sceKernelCallCoredumpHandler(pid);
}

int oalHasCoredumpHandler(SceUID pid)
{
	return SceProcessmgrForDriver_A1CF74FA(pid);
}

int oalIsCoredumpHandlerAtSyscall(SceUID pid, SceUID threadId)
{
	CoredumpThreadSnapshot snapshot;
	ThreadmgrDebugRegisters registers[2];
	uint32_t instruction;

	if (SceThreadmgrForDriver_A0B1AB21(threadId) <= 0)
		return 0;
	snapshot.info.size = sizeof(snapshot.info);
	if (SceThreadmgrForDriver_DD8D9429(threadId, 0, &snapshot.info) != 0)
		return 0;
	if (SceThreadmgrForDriver_5022689D(threadId, registers) != 0)
		return 0;
	if ((registers[0].cpsr & 0x3f) != 0x10)
		return 0;
	instruction = 0;
	if (sceKernelCopyFromUserProc(pid, &instruction,
		(const void *)registers[0].pc, sizeof(instruction)) < 0)
		return 0;
	return instruction == 0xe12cd070;
}

int oalGetDebugTailLog(char *buf, SceSize size)
{
	int result;

	SCE_DBG_ASSERT(buf != NULL, 6477);
	SCE_DBG_ASSERT(size != 0, 6478);
	result = SceDebugForDriver_62466B0A(buf, size);
	if (result < 0)
		result = 0;
	return result;
}

int oalGetDebugTailLogPid(SceUID pid, char *buf, SceSize size)
{
	int result;

	SCE_DBG_ASSERT(buf != NULL, 6489);
	SCE_DBG_ASSERT(size != 0, 6490);
	result = SceSysrootForDriver_591BB490(pid, buf, size);
	if (result < 0)
		result = 0;
	return result;
}

int oalGetFrameBufferInfo(SceUID pid, unsigned int index,
	unsigned int *vblankcount, unsigned int *width, unsigned int *height,
	unsigned int *pixelformat)
{
	SceDisplayFrameBufInfo info;
	int result;

	SCE_DBG_ASSERT(vblankcount != NULL, 6524);
	SCE_DBG_ASSERT(width != NULL, 6525);
	SCE_DBG_ASSERT(height != NULL, 6526);
	SCE_DBG_ASSERT(pixelformat != NULL, 6527);
	SCE_DBG_ASSERT(index <= 1, 6528);
	info.size = sizeof(info);
	result = SceDisplayForDriver_3BC165EF(pid,
		SceDisplayForDriver_C8E554C5(), index, &info);
	if (result == 0) {
		*vblankcount = info.vblankcount;
		*width = info.framebuf.width;
		*height = info.framebuf.height;
		*pixelformat = info.framebuf.pixelformat;
	}
	return result;
}

int oalGetFrameBuffer(SceUID pid, int index, void **base, unsigned int *pitch)
{
	SceDisplayFrameBufInfo info;
	int result;

	SCE_DBG_ASSERT(pitch != NULL, 6556);
	SCE_DBG_ASSERT(base != NULL, 6557);
	info.size = sizeof(info);
	result = SceDisplayForDriver_3BC165EF(pid,
		SceDisplayForDriver_C8E554C5(), index, &info);
	if (result == 0) {
		*base = info.framebuf.base;
		*pitch = info.framebuf.pitch;
	}
	return result;
}

int oalGetFrameBufferDetails(SceUID pid, int index,
	unsigned int *vblankcount, unsigned int *width, unsigned int *height,
	unsigned int *pixelformat, void **base, unsigned int *pitch)
{
	SceDisplayFrameBufInfo info;
	int result;

	info.size = sizeof(info);
	result = SceDisplayForDriver_3BC165EF(pid,
		SceDisplayForDriver_C8E554C5(), index, &info);
	if (result == 0) {
		*vblankcount = info.vblankcount;
		*width = info.framebuf.width;
		*height = info.framebuf.height;
		*pixelformat = info.framebuf.pixelformat;
	}
	if (result < 0)
		return result;
	info.size = sizeof(info);
	result = SceDisplayForDriver_3BC165EF(pid,
		SceDisplayForDriver_C8E554C5(), index, &info);
	if (result == 0) {
		*base = info.framebuf.base;
		*pitch = info.framebuf.pitch;
	}
	return result;
}

int oalReferObject(SceUID guid)
{
	return sceGUIDReferObject(guid, NULL);
}

int oalReleaseObject(SceUID guid)
{
	return sceGUIDReleaseObject(guid);
}

int oalChangeThreadPriority(SceUID threadId, int priority)
{
	return sceKernelChangeThreadPriority2(threadId, priority);
}

int oalGetEventlog(void *buf, SceSize size, SceSize *readBlocks)
{
	SCE_DBG_ASSERT(buf != NULL, 6606);
	SCE_DBG_ASSERT(readBlocks != NULL, 6607);
	return SceDebugForDriver_CC5365D3(buf, size, readBlocks);
}

/* 0x6894499C in 4.00 takes a second argument, always 0 here */

int oalResumeProcess(SceUID pid)
{
	return sceKernelProcessDebugResume(pid, 0);
}

int oalSuspendProcess(SceUID pid, SceUInt32 *timeout)
{
	return sceKernelSuspendProcess(pid, 3, timeout);
}

int oalGetVitaCardInfo(CoredumpVitaCardInfo *out)
{
	SdifMmcContext *context;

	SCE_DBG_ASSERT_FUNC(out != NULL, 6637, "oalGetVitaCardInfo");
	memset(out, 0, sizeof(*out));
	out->size[0] = sizeof(*out);
	out->size[1] = 0;
	out->size[2] = 0;
	out->size[3] = 0;
	out->headerSize[0] = 36;
	out->headerSize[1] = 0;
	out->headerSize[2] = 0;
	out->headerSize[3] = 0;
	__builtin_memcpy(out->name, "System Device VITA Card Info.",
		sizeof("System Device VITA Card Info."));
	out->recordSize[0] = 20;
	out->recordSize[1] = 0;
	out->recordType[0] = 1;
	out->recordType[1] = 0;
	context = SceSdifForDriver_6A71987F(1);
	if (context != NULL) {
		SceUInt32 cid0, cid1, cid2, cid3;

		out->present[0] = 1;
		out->present[1] = 0;
		out->present[2] = 0;
		out->present[3] = 0;
		cid0 = context->cid.word[0];
		cid1 = context->cid.word[1];
		cid2 = context->cid.word[2];
		cid3 = context->cid.word[3];
		out->cid.word[0] = cid0;
		out->cid.word[1] = cid1;
		out->cid.word[2] = cid2;
		out->cid.word[3] = cid3;
	}
	return 0;
}

int oalGetMemoryCardInfo(CoredumpMemoryCardInfo *out)
{
	SceMsInfo info;

	SCE_DBG_ASSERT_FUNC(out != NULL, 6664, "oalGetMemoryCardInfo");
	memset(out, 0, sizeof(*out));
	out->size[0] = sizeof(*out);
	out->size[1] = 0;
	out->size[2] = 0;
	out->size[3] = 0;
	out->headerSize[0] = 36;
	out->headerSize[1] = 0;
	out->headerSize[2] = 0;
	out->headerSize[3] = 0;
	__builtin_memcpy(out->name, "System Device Memory Card Info.", 31);
	out->recordSize[0] = 68;
	out->recordSize[1] = 0;
	out->recordType[0] = 1;
	out->recordType[1] = 0;
	if (SceMsifForDriver_D0307849(&info) == 0) {
		out->present[0] = 1;
		out->present[1] = 0;
		out->present[2] = 0;
		out->present[3] = 0;
		out->mediaType = info.mediaType;
		out->writeProtected = info.writeProtected;
		out->nbytes[0] = info.nbytes;
		out->nbytes[1] = info.nbytes >> 32;
		out->nbytes2[0] = info.nbytes2;
		out->nbytes2[1] = info.nbytes2 >> 32;
		out->sector_size[0] = info.sector_size;
		out->sector_size[1] = info.sector_size >> 32;
		out->blockSize = info.blockSize;
		out->serialNumber = info.serialNumber;
		out->assemblyModel = info.assemblyModel;
		out->assemblyMaker = info.assemblyMaker;
		__builtin_memcpy(out->assemblyDate, info.assemblyDate,
			sizeof(out->assemblyDate));
	}
	return 0;
}
