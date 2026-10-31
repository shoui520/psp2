/*
 * Kernel diagnostics and leak-check command group (0xf0): interrupt handlers,
 * kernel utilities, resource snapshots, application manager queries, UID
 * classes and entries, Venezia status, HID devices and callbacks, Bluetooth
 * registered devices, and the secure monitor crash trigger.
 */

#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <appmgr/appmgr_debugger.h>
#include <bt/bt_registered.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>

#include "sdbgp_internal.h"

SceSize strnlen(const char *s, SceSize maxlen);

#define SDBGP_ERROR_NOT_SUPPORTED	((int)0x8009000e)

typedef struct SdbgpIntrHandlerEntry {
	SceSize size;				/* 136 */
	void *arg;
	void *handler;
	uint8_t intrCode;
	uint8_t subCode;
	uint16_t type;
	uint16_t subCount;
	uint16_t priority;
	uint16_t targetCpu;
	uint16_t enabled;
	uint32_t count[4];
	uint32_t reserved0[4];
	uint32_t reserved1[4];
	uint32_t reserved2[8];
	char name[32];
} SdbgpIntrHandlerEntry;

_Static_assert(sizeof(SdbgpIntrHandlerEntry) == 136, "SdbgpIntrHandlerEntry size");

typedef struct SdbgpClassEntry {
	SceSize size;				/* 56 */
	uint32_t attributes;
	char name[32];
	uint32_t itemsizeAligned;
	uint32_t defaultObjectType;
	SceClass *cls;
	SceClass *parent;
} SdbgpClassEntry;

_Static_assert(sizeof(SdbgpClassEntry) == 56, "SdbgpClassEntry size");

typedef struct SdbgpGUIDEntry {
	SceSize size;				/* 56 */
	SceUID uid;
	char name[32];
	SceObjectBase *object;
	SceClass *cls;
	uint32_t attr;
	uint32_t useCount;
} SdbgpGUIDEntry;

_Static_assert(sizeof(SdbgpGUIDEntry) == 56, "SdbgpGUIDEntry size");

typedef struct SdbgpPUIDEntry {
	SceSize size;				/* 60 */
	SceUID uid;
	char name[32];
	SceObjectBase *object;
	SceClass *cls;
	uint32_t attr;
	uint32_t useCount;
	SceUID guid;
} SdbgpPUIDEntry;

_Static_assert(sizeof(SdbgpPUIDEntry) == 60, "SdbgpPUIDEntry size");

typedef struct SdbgpBtDeviceEntry {
	SceSize size;				/* 160 */
	uint8_t mac[8];
	char name[124];
	uint32_t btClass;
	uint32_t profiles;
	uint32_t flags;
	uint16_t vid;
	uint16_t pid;
	uint16_t version;
	uint8_t primary;
	uint8_t reserved;
	int connectingState;
} SdbgpBtDeviceEntry;

_Static_assert(sizeof(SdbgpBtDeviceEntry) == 160, "SdbgpBtDeviceEntry size");

/* 0x81012e70: cmd 0xf0 / 0x02 — interrupt handler list */
int sdbgpCmdF0_02(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceKernelIntrHandlerInfo info;
	char nameBuf[32];
	uint8_t *reply;
	SdbgpIntrHandlerEntry *entries;
	SceSize sendSize;
	int total = 0;
	int intr, sub, subCount, outIdx, j, ret;

	if (sdbgpGetRecvSize(cpu) < sizeof(SdbgpRequest)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	for (intr = 0; intr < 256; intr++) {
		info.size = sizeof(info);
		info.name = NULL;
		if (sceKernelQueryIntrHandlerInfo(intr, (unsigned int)-1, &info) < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, 0x20000);
			return 0x20000;
		}
		if (info.handler != NULL) {
			subCount = info.subCount;
			total++;
			for (sub = 0; sub < subCount; sub++) {
				if (sceKernelQueryIntrHandlerInfo(intr, sub, &info) < 0) {
					sdbgpSendError(cpu, packet->group,
						       packet->command + 1,
						       packet->tag, 0x20000);
					return 0x20000;
				}
				if (info.handler != NULL)
					total++;
			}
		}
	}
	sendSize = sizeof(SdbgpIntrHandlerEntry) * total + 32;
	if (sendSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	*(uint32_t *)(reply + 28) = total;
	entries = (SdbgpIntrHandlerEntry *)(reply + 32);
	outIdx = 0;
	for (intr = 0; intr < 256; intr++) {
		info.size = sizeof(info);
		info.name = nameBuf;
		if (sceKernelQueryIntrHandlerInfo(intr, (unsigned int)-1, &info) < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, SCE_KERNEL_ERROR_INTRMGR_ERROR);
			break;
		}
		if (info.handler != NULL) {
			SdbgpIntrHandlerEntry *ent = &entries[outIdx++];

			ent->size = sizeof(*ent);
			ent->arg = info.arg;
			ent->handler = info.handler;
			ent->intrCode = (uint8_t)info.intrCode;
			ent->subCode = 0xff;
			ent->type = info.type;
			ent->subCount = info.subCount;
			ent->priority = info.priority;
			ent->targetCpu = info.targetCpu;
			ent->enabled = info.enabled;
			for (j = 0; j < 4; j++) {
				ent->count[j] = info.count[j];
				ent->reserved0[j] = info.reserved0[j];
				ent->reserved1[j] = info.reserved1[j];
			}
			SceDeci4pSDfMgrForKernel_5996E2DB(ent->name, info.name,
							  sizeof(ent->name));
			subCount = info.subCount;
			for (sub = 0; sub < subCount; sub++) {
				ret = sceKernelQueryIntrHandlerInfo(intr, sub, &info);
				if (ret < 0) {
					sdbgpSendError(cpu, packet->group,
						       packet->command + 1,
						       packet->tag, ret);
					goto done;
				}
				if (info.handler != NULL) {
					ent = &entries[outIdx++];
					ent->size = sizeof(*ent);
					ent->handler = info.handler;
					ent->intrCode = (uint8_t)info.intrCode;
					ent->subCode = (uint8_t)sub;
					ent->type = info.type;
					ent->subCount = info.subCount;
					ent->priority = info.priority;
					ent->targetCpu = info.targetCpu;
					ent->enabled = info.enabled;
					for (j = 0; j < 4; j++) {
						ent->count[j] = info.count[j];
						ent->reserved0[j] = info.reserved0[j];
						ent->reserved1[j] = info.reserved1[j];
					}
					SceDeci4pSDfMgrForKernel_5996E2DB(
						ent->name, info.name,
						sizeof(ent->name));
				}
			}
		}
	}
done:
	sdbgpSendErrorHeader(cpu, packet->group, 3, 0, 0);
	return 0;
}

/* 0x81013108: cmd 0xf0 / 0x08 — kernel utility dispatch */
int sdbgpCmdF0_08(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	void *reply;

	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (kernelUtility(*(const int *)((const SdbgpRequest *)packet + 1)) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_VALUE);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, 0x2c);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 0x2c);
	sdbgpSetSendSize(cpu, 0x2c);
	sdbgpSendErrorHeader(cpu, packet->group, 9, 0, 0);
	return 0;
}

/* 0x81013194: cmd 0xf0 / 0x0a — resource snapshot */
int sdbgpCmdF0_0a(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	uint32_t mode;
	void *reply;

	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	mode = *(const uint32_t *)((const SdbgpRequest *)packet + 1);
	if (mode == 2)
		LeakCheckUIDSnapshot();
	else if (mode == 4)
		LeakCheckHeapSnapshot();
	else
		sceKernelSnapshot();
	reply = sdbgpAllocSendBuf(cpu, 0x2c);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 0x2c);
	sdbgpSetSendSize(cpu, 0x2c);
	sdbgpSendErrorHeader(cpu, packet->group, 11, 0, 0);
	return 0;
}

/* 0x81013214: cmd 0xf0 / 0x20 — leak check query / reset */
int sdbgpCmdF0_20(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	uint32_t mode;
	uint32_t *reply;
	SceSize count, dataSize, sendSize;
	uint32_t generation;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	mode = *(const uint32_t *)((const SdbgpRequest *)packet + 1);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	switch (mode) {
	case 0x80000000u:
		ret = _leakcheckReset();
		if (ret < 0)
			goto err;
		sendSize = 40;
		reply[7] = mode;
		reply[8] = 0;
		reply[9] = 0;
		break;
	case 2:
		ret = _leakcheckUID((LeakCheckUIDRecord *)&reply[10], &generation,
				    &count, &dataSize);
		if (ret < 0)
			goto err;
		sendSize = dataSize + 40;
		reply[7] = mode;
		reply[8] = count;
		reply[9] = generation;
		break;
	case 4:
		ret = _leakcheckHeap((LeakCheckHeapRecord *)&reply[10], &generation,
				     &count, &dataSize);
		if (ret < 0)
			goto err;
		sendSize = dataSize + 40;
		reply[7] = mode;
		reply[8] = count;
		reply[9] = generation;
		break;
	default:
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_NOT_SUPPORTED);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 33, 0, 0);
	return 0;

err:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

/* 0x81013344: cmd 0xf0 / 0x0c — application list and device owners */
int sdbgpCmdF0_0c(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	AppMgrAppList apps;
	uint8_t *reply;
	uint32_t *dev;
	SceSize sendSize;
	int appCount = 0, renderCount = 0, touchCount = 0;
	int i;

	if (sdbgpGetRecvSize(cpu) < sizeof(SdbgpRequest)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (SceSysrootForKernel_2A03DFA1(&apps) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_NOT_SUPPORTED);
		return SDBGP_ERROR_NOT_SUPPORTED;
	}
	for (i = 0; i < 16; i++) {
		if (apps.appIds[i] != -1)
			appCount++;
	}
	for (i = 0; i < 16; i++) {
		if (apps.device.renderingPids[i] != -1)
			renderCount++;
	}
	for (i = 0; i < 16; i++) {
		if (apps.device.touchPids[i] != -1)
			touchCount++;
	}
	sendSize = 4 * (appCount + 24 + renderCount + touchCount);
	if (sendSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	*(uint32_t *)(reply + 28) = appCount;
	SceDeci4pSDfMgrForKernel_5996E2DB(reply + 32, apps.appIds, 4 * appCount);
	dev = (uint32_t *)(reply + 32 + 4 * appCount);
	dev[0] = apps.device.soundExclusivePid;
	dev[1] = apps.device.mainOutPortOwner;
	dev[2] = apps.device.bgmOutPortOwner;
	dev[3] = apps.device.voiceOutPortOwner;
	dev[4] = apps.device.inputPortOwner;
	dev[5] = apps.device.backRenderPid;
	dev[6] = apps.device.inputOwnerPid;
	dev[7] = apps.device.bgmProxyAppId;
	dev[8] = apps.device.shellPid;
	dev[9] = apps.device.activeAppId;
	dev[10] = apps.device.displayOwner0;
	dev[11] = apps.device.displayOwner1;
	dev[12] = apps.device.systemChatPid;
	dev[13] = apps.device.backRenderAppId;
	dev[14] = renderCount;
	SceDeci4pSDfMgrForKernel_5996E2DB(&dev[15], apps.device.renderingPids,
					  4 * renderCount);
	dev[15 + renderCount] = touchCount;
	SceDeci4pSDfMgrForKernel_5996E2DB(&dev[16 + renderCount], apps.device.touchPids,
					  4 * touchCount);
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 13, 0, 0);
	return 0;
}

/* 0x810134ec: cmd 0xf0 / 0x0e — application info by app ID */
int sdbgpCmdF0_0e(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	AppMgrAppInfo app;
	const uint32_t *req = (const uint32_t *)((const SdbgpRequest *)packet + 1);
	uint8_t *reply;
	uint32_t *ent;
	SceSize recvSize, allocSize, sendSize;
	uint32_t count, i;
	int ret;

	recvSize = sdbgpGetRecvSize(cpu);
	if (recvSize <= 0x17 || (count = req[0], recvSize < 4 * (count + 6))) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	allocSize = 332 * count + 32;
	if (allocSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	reply = sdbgpAllocSendBuf(cpu, allocSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ent = (uint32_t *)(reply + 32);
	sendSize = 32;
	for (i = 0; i < req[0]; i++) {
		SceSize len, pad, entrySize;

		ret = SceSysrootForKernel_6D111FA7(req[1 + i], &app);
		if (ret != 0) {
			uint32_t errCode = 0x200e0;
			int errRet = SDBGP_ERROR_INVALID_ARGUMENT;

			if (ret == (int)0x80010086) {
				errCode = SDBGP_RESULT_NOT_SUPPORTED;
				errRet = SDBGP_ERROR_NOT_SUPPORTED;
			}
			sdbgpSetSendSize(cpu, 28);
			sdbgpSendErrorHeader(cpu, packet->group, 15, 0, errCode);
			return errRet;
		}
		ent[1] = app.appId;
		ent[2] = app.launchSourceAppId;
		ent[3] = app.processId;
		ent[4] = app.launchSourceProcessId;
		SceDeci4pSDfMgrForKernel_8A3B06F2(&ent[5], 0, 32);
		SceDeci4pSDfMgrForKernel_5996E2DB(&ent[5], app.appName, 32);
		ent[13] = app.budgetId;
		ent[14] = app.launchMode;
		ent[15] = app.processAttr;
		ent[16] = app.fdLimit;
		ent[17] = app.dirLimit;
		len = strnlen(app.appPath, 255);
		SceDeci4pSDfMgrForKernel_5996E2DB(&ent[19], app.appPath, len);
		*((uint8_t *)&ent[19] + len) = 0;
		pad = -(len + 1) & 3;
		SceDeci4pSDfMgrForKernel_8A3B06F2((uint8_t *)&ent[19] + len + 1, 0, pad);
		entrySize = len + 77 + pad;
		ent[18] = len + 1;
		ent[0] = entrySize;
		ent = (uint32_t *)((uint8_t *)ent + entrySize);
		sendSize += entrySize;
	}
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 15, 0, 0);
	return 0;
}

/* 0x810136a8: cmd 0xf0 / 0x10 — UID class list */
int sdbgpCmdF0_10(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceKernelClassInfo *work;
	uint8_t *reply;
	SdbgpClassEntry *entries;
	int count = 0;
	SceSize sendSize;
	int i;

	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (SceSysmemForKernel_100B4029(work, SDBGP_WORK_BUF_SIZE,
					(SceUInt32 *)&count) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	sendSize = sizeof(SdbgpClassEntry) * count + 32;
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	entries = (SdbgpClassEntry *)(reply + 32);
	for (i = 0; i < count; i++) {
		SceClass *cls = work[i].cls;
		SceClass *parent = work[i].parent;

		entries[i].size = sizeof(SdbgpClassEntry);
		entries[i].attributes = cls->attributes;
		entries[i].itemsizeAligned = cls->itemsize_aligned;
		entries[i].defaultObjectType = cls->defaultObjectType;
		entries[i].cls = cls;
		entries[i].parent = parent;
		if (cls->name != NULL)
			SceDeci4pSDfMgrForKernel_5996E2DB(entries[i].name, cls->name,
							  sizeof(entries[i].name));
	}
	*(uint32_t *)(reply + 28) = count;
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 17, 0, 0);
	return 0;
}

/* 0x810137b8: cmd 0xf0 / 0x12 — GUID entry list */
int sdbgpCmdF0_12(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceKernelUIDEntryInfo *work;
	uint8_t *reply;
	SdbgpGUIDEntry *entries;
	SceSize count = 0;
	SceSize sendSize, i;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = sceGUIDGetEntryInfoAll(5, 3, work, 334080, &count);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	sendSize = sizeof(SdbgpGUIDEntry) * count + 32;
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	entries = (SdbgpGUIDEntry *)(reply + 32);
	for (i = 0; i < count; i++) {
		entries[i].size = sizeof(SdbgpGUIDEntry);
		entries[i].uid = work[i].uid;
		entries[i].object = work[i].object;
		entries[i].cls = work[i].cls;
		entries[i].attr = work[i].attr;
		entries[i].useCount = work[i].useCount;
		if (work[i].name != NULL)
			SceDeci4pSDfMgrForKernel_5996E2DB(entries[i].name, work[i].name,
							  sizeof(entries[i].name));
	}
	*(uint32_t *)(reply + 28) = count;
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 19, 0, 0);
	return 0;
}

/* 0x810138e4: cmd 0xf0 / 0x14 — PUID entry list */
int sdbgpCmdF0_14(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceUID pid;
	SceKernelUIDEntryInfo *work;
	uint8_t *reply;
	SdbgpPUIDEntry *entries;
	int count = 0;
	SceSize sendSize;
	int i, ret;

	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const SdbgpRequest *)packet + 1);
	if (pid == SDBGP_KERNEL_PROCESS_ID || isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	ret = PUIDGetEntryInfoAll(pid, 5, 0, work, 9280, (SceSize *)&count);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	sendSize = sizeof(SdbgpPUIDEntry) * count + 32;
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	entries = (SdbgpPUIDEntry *)(reply + 32);
	for (i = 0; i < count; i++) {
		SceUID guid;

		entries[i].size = sizeof(SdbgpPUIDEntry);
		entries[i].uid = work[i].uid;
		entries[i].object = work[i].object;
		entries[i].cls = work[i].cls;
		entries[i].attr = work[i].attr;
		entries[i].useCount = work[i].useCount;
		if (work[i].name != NULL)
			SceDeci4pSDfMgrForKernel_5996E2DB(entries[i].name, work[i].name,
							  sizeof(entries[i].name));
		guid = scePUIDtoGUID(pid, work[i].uid);
		if (guid < 0)
			guid = -1;
		entries[i].guid = guid;
	}
	*(uint32_t *)(reply + 28) = count;
	sdbgpSetSendSize(cpu, sendSize);
	sdbgpSendErrorHeader(cpu, packet->group, 21, 0, 0);
	return 0;
}

/* 0x81013a90: cmd 0xf0 / 0x16 — Venezia status */
int sdbgpCmdF0_16(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	uint8_t buf[288];
	uint8_t *reply;

	if (sdbgpGetRecvSize(cpu) < sizeof(SdbgpRequest)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	reply = sdbgpAllocSendBuf(cpu, 0x13c);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 0x13c);
	sdbgpSetSendSize(cpu, 0x13c);
	SceCodecEngineWrapperForDebugger_771190E7(0x1000, buf);
	SceDeci4pSDfMgrForKernel_5996E2DB(reply + 28, buf, sizeof(buf));
	sdbgpSendErrorHeader(cpu, packet->group, 23, 0, 0);
	return 0;
}

/* 0x81013b30: cmd 0xf0 / 0x18 — HID devices */
int sdbgpCmdF0_18(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	HidDeviceInfo *work;
	uint8_t *reply;
	HidDeviceInfo *entries;
	SceUInt32 count = 0;
	int colIdx = 0;
	SceUInt32 i;
	int j, ret;

	if (sdbgpGetRecvSize(cpu) < sizeof(SdbgpRequest)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	ret = SceSysrootForDriver_ED22CF8F(work, SDBGP_SEND_BUF_SIZE, &count);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	entries = (HidDeviceInfo *)(reply + 32);
	for (i = 0; i < count; i++) {
		HidDeviceInfo *src = &work[i];
		HidDeviceInfo *dst = &entries[i];

		dst->size = sizeof(*dst);
		dst->transportType = src->transportType;
		dst->vendorId = src->vendorId;
		dst->productId = src->productId;
		dst->version = src->version;
		dst->primary = src->primary;
		for (j = 0; j < 24; j++) {
			if (src->collections[j].size != 0) {
				dst->collections[colIdx].size = sizeof(HidCollectionInfo);
				dst->collections[colIdx].id = src->collections[j].id;
				dst->collections[colIdx].type = src->collections[j].type;
				colIdx++;
			}
		}
		SceDeci4pSDfMgrForKernel_5996E2DB(&dst->mac0, &src->mac0, 8);
		SceDeci4pSDfMgrForKernel_5996E2DB(dst->name, src->name, 257);
	}
	*(uint32_t *)(reply + 28) = count;
	sdbgpSetSendSize(cpu, sizeof(HidDeviceInfo) * count + 32);
	sdbgpSendErrorHeader(cpu, packet->group, 25, 0, 0);
	return 0;
}

/* 0x81013cc4: cmd 0xf0 / 0x1a — HID hint callback lists */
int sdbgpCmdF0_1a(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	HidHintCallbackInfo *work;
	uint8_t *reply;
	HidHintCallbackInfo *entries;
	SceUInt32 count = 0;
	SceUInt32 i;
	int ret;

	if (sdbgpGetRecvSize(cpu) < sizeof(SdbgpRequest)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	ret = SceSysrootForDriver_DC651D22(work, SDBGP_SEND_BUF_SIZE, &count);
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	entries = (HidHintCallbackInfo *)(reply + 32);
	for (i = 0; i < count; i++) {
		entries[i].size = sizeof(HidHintCallbackInfo);
		entries[i].deviceType = work[i].deviceType;
		entries[i].hintType = work[i].hintType;
		entries[i].callbackId = work[i].callbackId;
		entries[i].pid = work[i].pid;
	}
	*(uint32_t *)(reply + 28) = count;
	sdbgpSetSendSize(cpu, sizeof(HidHintCallbackInfo) * count + 32);
	sdbgpSendErrorHeader(cpu, packet->group, 27, 0, 0);
	return 0;
}

/* 0x81013de0: cmd 0xf0 / 0x1c — Bluetooth registered devices */
int sdbgpCmdF0_1c(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SceBtRegisteredInfo *work;
	uint8_t *reply;
	SdbgpBtDeviceEntry *entries;
	int count, i, conn;

	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	count = SceSysrootForDriver_951C7943(0, work, 32);
	if (count < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, count);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	*(uint32_t *)(reply + 28) = count;
	entries = (SdbgpBtDeviceEntry *)(reply + 32);
	for (i = 0; i < count; i++) {
		SceBtRegisteredInfo *src = &work[i];
		SdbgpBtDeviceEntry *dst = &entries[i];
		uint64_t addr;

		dst->size = sizeof(*dst);
		dst->btClass = src->bt_class;
		dst->profiles = src->profiles;
		dst->flags = src->flags;
		dst->vid = src->vid;
		dst->pid = src->pid;
		dst->version = src->version;
		dst->primary = src->primary;
		SceDeci4pSDfMgrForKernel_5996E2DB(dst->mac, src->mac, 8);
		SceDeci4pSDfMgrForKernel_5996E2DB(dst->name, src->name, 121);
		__builtin_memcpy(&addr, src->mac, sizeof(addr));
		conn = SceSysrootForDriver_D1E54BF4(addr);
		if (conn < 0)
			conn = 0;
		dst->connectingState = conn;
	}
	sdbgpSetSendSize(cpu, sizeof(SdbgpBtDeviceEntry) * count + 32);
	sdbgpSendErrorHeader(cpu, packet->group, 29, 0, 0);
	return 0;
}

/* 0x81013f1c: cmd 0xf0 / 0x30 — secure monitor crash trigger */
int sdbgpCmdF0_30(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	uint32_t flags = 0;

	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	if (SceExcpmgrForKernel_B615A7DA() == 0)
		goto dump_tty;
	for (;;) {
		void *sysroot = sceKernelSysrootGetSysroot();
		uint32_t ttbr0, vbar;

		__asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
		__asm__ volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));
		sdbgpCallSecureMonitor(flags, ttbr0, vbar, sysroot);
dump_tty:
		SceDebugForDriver_62466B0A(sysrootSharedMemory()->tty, 0x1000);
		flags = 0x1000;
	}
}
