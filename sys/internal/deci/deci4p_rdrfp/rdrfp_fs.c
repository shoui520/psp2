/*
 * SceDeci4pRDrfp file system thread (Sony's rdrfp_fs.c): runs the queued
 * requests, and tells the host when a device is mounted or unmounted.
 */
#include <stddef.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/threadmgr.h>
#include "rdrfp.h"

#define RDRFP_DFMGR_ERROR_REQUESTED	0x80080007

#define RDRFP_MOUNT_EVENT_MOUNTED	1
#define RDRFP_MOUNT_EVENT_UNMOUNTED	2
#define RDRFP_MOUNT_POINT_GAME_CARD	0xa00

/* sceIoDevctl 0x3001 */
typedef struct RdrfpDevctlInfo {
	SceOff total;
	SceOff free;
	SceSize clusterSize;
	SceUInt32 reserved;
} RdrfpDevctlInfo;

const RdrfpDevice rdrfpDevices[RDRFP_DEVICES] = {
	{ "ux0:", 0, "ux0:", 0x800, 10 },
	{ "ux0:", 0, "Memory Card:", 0x800, 10 },
	{ "grw0:", 0, "Vita Card:", RDRFP_MOUNT_POINT_GAME_CARD, 10 },
};

static SceUID rdrfpFSThread;
static SceUID rdrfpMountEvents[RDRFP_DEVICES];
static SceUID rdrfpMountCallbacks[RDRFP_DEVICES];

/* inlined into the device notify in Sony's build */
SceSize rdrfpMakeDeviceInfo(int dev, SceOff total, SceOff free, RdrfpDeviceInfo *info)
{
	const char *name = rdrfpDevices[dev].name;
	SceSize len;

	info->total = total;
	info->free = free;
	info->type = rdrfpDevices[dev].type;
	info->attribute[0] = 0;
	info->attribute[1] = 4;
	info->attribute[2] = 0;
	info->attribute[3] = 0;
	len = strnlen(name, 0x20);
	info->nameSize = len + 1;
	memset(info->name, 0, (len + 4) & ~3);
	memcpy(info->name, name, info->nameSize);
	return info->size = (info->nameSize + offsetof(RdrfpDeviceInfo, name) + 3) & ~3;
}

int getDeviceInfo(int dev, SceOff *total, SceOff *free)
{
	RdrfpDevctlInfo info = { 0 };
	int ret;

	if (dev >= RDRFP_DEVICES)
		return SCE_ERROR_ERRNO_ENODEV;
	ret = sceIoDevctl(rdrfpDevices[dev].path, 0x3001, NULL, 0, &info, sizeof(info));
	if (ret < 0) {
		if (ret != (int)SCE_ERROR_ERRNO_ENODEV)
			SCE_DBG_PRINTF_WITH_INFO(0xf, 375, "sceIoDevctl ret = 0x%08x\n", ret);
		return ret;
	}
	*total = info.total;
	*free = info.free;
	return 0;
}

static inline __attribute__((always_inline)) void rdrfpFSMainLoop(void)
{
	RdrfpRequest *req;
	RdrfpHeader *packet;
	int ret;

	while ((req = rdrfpDequeueRequest()) != NULL) {
		packet = req->desc->packet;
		ret = rdrfpFSCallCommandHandler(req, packet->type, packet, packet + 1);
		if (ret < 0 && ret != (int)RDRFP_DFMGR_ERROR_REQUESTED)
			SCE_DBG_PRINTF_WITH_INFO(0xf, 189, "rdrfpFSCommandHandler failed (0x%08x)\n", ret);
	}
	SCE_DBG_PRINTF_WITH_INFO(0xf, 194, "fatal: elem = NULL\n");
}

static inline __attribute__((always_inline)) void rdrfpSendDeviceNotify(int dev, int mounted,
									SceOff total, SceOff free)
{
	RdrfpDeviceNotify *notify;
	RdrfpRequest *req;
	int index, ret;

	index = rdrfpReserveBuffer(RDRFP_BUF_RECV, 1);
	if (index < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 288, "couldn't reserve buffer\n");
		return;
	}
	notify = rdrfpBufferAddr(RDRFP_BUF_RECV, index);
	memset(notify, 0, 0x40);
	req = rdrfpPrepareRequest(0, RDRFP_BUF_RECV, index, notify);
	rdrfpMakeDeviceInfo(dev, total, free, &notify->info);
	notify->mounted = mounted;
	rdrfpSetPacketSize(notify, notify->info.size + offsetof(RdrfpDeviceNotify, info));
	rdrfpSetPacketType(&notify->head, RDRFP_TYPE_DEVICE_NOTIFY);
	ret = rdrfpSendPacket(req);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 314, "rdrfpSendPacket failed? (0x%08x)\n", ret);
}

static int rdrfpMountCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg, void *common)
{
	int dev = (int)common;
	SceOff total, free;
	SceUInt32 events;
	int mounted, ret;

	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;

	ret = SceThreadmgrForDriver_C529EA32(rdrfpMountEvents[dev], 3, &events, NULL, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 341, "WaitEvent failed (0x%08x)\n", ret);
		return ret;
	}
	if ((events & RDRFP_MOUNT_EVENT_UNMOUNTED) &&
	    rdrfpDevices[dev].mountPoint == RDRFP_MOUNT_POINT_GAME_CARD)
		rdrfpDeleteVDTreeOfGameCard();
	mounted = events & RDRFP_MOUNT_EVENT_MOUNTED;
	if (!rdrfpIsEnabled())
		return 0;
	if (mounted) {
		if (getDeviceInfo(dev, &total, &free) < 0)
			return 0;
	} else {
		total = 0;
		free = 0;
	}
	rdrfpSendDeviceNotify(dev, mounted, total, free);
	return 0;
}

static int rdrfpFSThreadEntryPoint(SceSize args, void *argp)
{
	int i, ret;

	(void)args;
	(void)argp;

	for (i = 0; i < RDRFP_DEVICES; i++) {
		rdrfpMountCallbacks[i] = sceKernelCreateCallback("SceDeci4pRDrfp", 0, rdrfpMountCallback,
								  (void *)i);
		if (rdrfpMountCallbacks[i] < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 135, "CreateCallback failed (0x%08x)\n",
					     rdrfpMountCallbacks[i]);
			return rdrfpMountCallbacks[i];
		}
		rdrfpMountEvents[i] = sceIoCreateMountEvent(rdrfpDevices[i].mountPoint,
							     RDRFP_MOUNT_EVENT_MOUNTED | RDRFP_MOUNT_EVENT_UNMOUNTED);
		if (rdrfpMountEvents[i] < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 146, "CreateMountEvent(%d) failed (0x%08x)\n", i,
					     rdrfpMountEvents[i]);
			return rdrfpMountEvents[i];
		}
		ret = sceKernelRegisterCallbackToEventForDriver(rdrfpMountEvents[i],
						      rdrfpMountCallbacks[i], 0);
		if (ret < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 156, "RegisterCallbackToEvent failed (0x%08x)\n", ret);
			return ret;
		}
	}
	ret = rdrfpRegisterWritebufTimer();
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 165, "RegisterTimerCallback failed (0x%08x)\n", ret);
		return ret;
	}

	rdrfpFSMainLoop();

	rdrfpUnregisterWritebufTimer();
	for (i = 0; i < RDRFP_DEVICES; i++) {
		sceIoDeleteMountEvent(rdrfpMountEvents[i]);
		sceKernelDeleteCallback(rdrfpMountCallbacks[i]);
	}
	return 0;
}

int rdrfpInitFS(void)
{
	SceKernelThreadOptParam opt;
	int ret;

	opt.size = 0x1c;
	opt.attr = 1;
	opt.kStackMemType = 0x10f0d006;
	ret = sceKernelCreateThread("SceDeci4pRDrfpFS", rdrfpFSThreadEntryPoint, 0x1f, 0x4000, 0,
				     0x80000, &opt);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 87, "CreateThread failed (0x%08x)\n", ret);
		return ret;
	}
	rdrfpFSThread = ret;
	ret = sceKernelStartThread(rdrfpFSThread, 0, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 96, "StartThread failed (0x%08x)\n", ret);
		return ret;
	}
	rdrfpVDTreeInit();
	rdrfpInitFdList();
	return 0;
}

/* whether a path names one of the devices, as the host shows them */
int rdrfpCheckDeviceName(const char *path, SceSize len)
{
	const char *cursor = path;
	SceSize i;
	int ret;

	if (len >= 0x20)
		len = 0x20;
	for (i = 0; i != len; i++, cursor++) {
		if (*cursor == ':')
			break;
		if (*cursor == '\0')
			return SCE_ERROR_ERRNO_EACCES;
	}
	if (i == 0x20)
		return SCE_ERROR_ERRNO_EACCES;
	ret = strncmp(rdrfpDevices[0].name, path, i + 1);
	if (ret == 0)
		return ret;
	ret = strncmp(rdrfpDevices[1].name, path, i + 1);
	if (ret == 0)
		return ret;
	return strncmp(rdrfpDevices[2].name, path, i + 1) ? SCE_ERROR_ERRNO_EACCES : 0;
}
