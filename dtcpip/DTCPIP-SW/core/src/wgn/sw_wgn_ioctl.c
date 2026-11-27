/*
 * sw_wgn_ioctl.c — Wagner software device file-descriptor interface (sw_open,
 * sw_close, sw_ioctl), transfer lifecycle (start/stop/sync/mov_endp/set_last),
 * DMA completion callbacks, and memory accounting.
 */
#include "sw_wgn_internal.h"

#include <stdlib.h>
#include <string.h>
#include <rtc.h>

struct SwDeviceSlot {
	struct SwWgnInfo *info;
	int openCount;
};

static struct SwDeviceSlot g_swDevices[1];
static struct SwFilePrivate *g_swOpenFds[4];
uint32_t g_swWgnLogMask = 7;

static int swLockMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelLockLwMutex(mtx, 1, NULL);
}

static int swUnlockMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelUnlockLwMutex(mtx, 1);
}

static int swCreateMtx(SceKernelLwMutexWork *mtx)
{
	int ret = sceKernelCreateLwMutex(mtx, "sce_sws_mutex", 0x2000u, 0, NULL);

	return (ret >= 0) ? 0 : ret;
}

static int swDeleteMtx(SceKernelLwMutexWork *mtx)
{
	return sceKernelDeleteLwMutex(mtx);
}

static int swSignalEvent(struct SwWgnEvent *ev)
{
	sceKernelLockLwMutex(&ev->mutex, 1, NULL);
	ev->signaled = 1;
	sceKernelSignalLwCond(&ev->cond);
	sceKernelUnlockLwMutex(&ev->mutex, 1);
	return 0;
}

static int swWaitEventInfinite(struct SwWgnEvent *ev)
{
	if (sceKernelLockLwMutex(&ev->mutex, 1, NULL) < 0)
		return 22;
	while (ev->signaled == 0) {
		if (sceKernelWaitLwCond(&ev->cond, NULL) < 0) {
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return 22;
		}
	}
	ev->signaled = 0;
	if (sceKernelUnlockLwMutex(&ev->mutex, 1) != 0)
		return 22;
	return 0;
}

static uint32_t swGetTimeMs(void)
{
	SceRtcTick tick;

	sceRtcGetCurrentTick(&tick);
	return (uint32_t)(tick.tick / 1000u);
}

static uint32_t swElapsedMs(uint32_t now, uint32_t start)
{
	if (start > now)
		return now - start - 1u;
	return now - start;
}

static int swWaitEventTimeout(struct SwWgnEvent *ev, int timeoutMs)
{
	int remain = timeoutMs;
	int status = 0;

	if (timeoutMs < 0)
		return swWaitEventInfinite(ev);
	if (timeoutMs == 0) {
		sceKernelLockLwMutex(&ev->mutex, 1, NULL);
		if (ev->signaled != 0) {
			ev->signaled = 0;
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return 0;
		}
		sceKernelUnlockLwMutex(&ev->mutex, 1);
		return 116;
	}

	sceKernelLockLwMutex(&ev->mutex, 1, NULL);
	while (ev->signaled == 0) {
		uint32_t t0 = swGetTimeMs();
		SceUInt32 timeoutUs = (SceUInt32)(1000 * remain);
		uint32_t t1;

		status = sceKernelWaitLwCond(&ev->cond, &timeoutUs);
		t1 = swGetTimeMs();
		if (status <= (int)0x80020005u) {
			if (status > (int)0x80020004u)
				status = 116;
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return status;
		}
		if (status != 0) {
			sceKernelUnlockLwMutex(&ev->mutex, 1);
			return status;
		}
		if (ev->signaled == 0) {
			remain -= (int)swElapsedMs(t1, t0);
			if (remain <= 0) {
				sceKernelUnlockLwMutex(&ev->mutex, 1);
				return status;
			}
		}
	}
	ev->signaled = 0;
	sceKernelUnlockLwMutex(&ev->mutex, 1);
	return status;
}

static int swWaitEventCond(struct SwWgnEvent *ev, int alreadyReady, int timeoutMs)
{
	int ret;

	if (alreadyReady != 0)
		return timeoutMs;
	ret = swWaitEventTimeout(ev, timeoutMs);
	if (ret == 0)
		return timeoutMs;
	if (ret == 116)
		return 0;
	return -1;
}

int swCheckFd(int fd)
{
	int valid = 0;

	if ((fd & ~0xFFF) == 0xA000 && (fd & 0xFFF) < 4)
		valid = 1;
	return valid;
}

void swListRemoveNode(struct SwListNode *node)
{
	struct SwListNode *prev = node->prev;
	struct SwListNode *next = node->next;

	prev->next = next;
	next->prev = prev;
}

void swListAddTail(struct SwListNode *node, struct SwListNode *head)
{
	struct SwListNode *prev = head->prev;

	head->prev = node;
	node->next = head;
	node->prev = prev;
	prev->next = node;
}

int swIoctl(int fd, uint32_t cmd, void *arg, uint32_t argLen)
{
	struct SwFilePrivate *priv;

	if (!swCheckFd(fd)) {
		swMwTrace("sw: sw_ioctl invalid fd(%d)\n", fd);
		return 11;
	}
	priv = g_swOpenFds[fd & 0xFFF];
	if (priv == NULL) {
		swMwTrace("sw: sw_ioctl invalid pdata\n");
		return 11;
	}
	if (priv->wgnInfo == NULL) {
		swMwTrace("sw: sw_ioctl invalid fd(%d,%p)\n", fd, NULL);
		return 11;
	}
	if (priv->closing != 0) {
		swMwTrace("sw: sw_ioctl close in progress\n");
		return 11;
	}
	if (((cmd >> 8) & 0xFFu) != 'x') {
		swMwTrace("sw: sw_ioctl illegal ioctl magic: %02x -> %02x\n",
			  'x', (cmd >> 8) & 0xFFu);
		return 12;
	}
	return swDispatchIoctl(priv, cmd, (unsigned char *)arg, argLen);
}

static struct SwFilePrivate *swAllocPrivateData(struct SwWgnInfo *info,
						struct SwFilePrivate **outPriv)
{
	struct SwFilePrivate *priv = (struct SwFilePrivate *)swAllocMem(info, sizeof(*priv), 0);

	if (priv != NULL) {
		priv->wgnInfo = info;
		priv->cdevList.next = &priv->cdevList;
		priv->cdevList.prev = &priv->cdevList;
		*outPriv = priv;
		return priv;
	}
	swMwTrace("sw: alloc private_data fail\n");
	*outPriv = NULL;
	return NULL;
}

int swOpen(const char *devName)
{
	int idx;
	int fd = -1;
	struct SwWgnInfo *info = NULL;
	struct SwFilePrivate *priv = NULL;
	unsigned char depInitArg[4];
	struct SwInfo *swInfo = NULL;

	for (idx = 0; idx < 1; ++idx) {
		info = g_swDevices[idx].info;
		if (info != NULL)
			break;
	}
	if (idx == 1) {
		int ret;

		for (idx = 0; idx < 1 && g_swDevices[idx].info != NULL; ++idx)
			;
		if (idx > 0) {
			swMwTrace("sw: sw_open: no space to load wagner device\n");
			return -1;
		}
		ret = swDepInitLib(depInitArg, &swInfo);
		if (ret != 0) {
			swMwTrace("sw: sw_open: sw_dep_init_lib error %d\n", ret);
			return -1;
		}
		info = swInfo->wgnInfo;
		info->devIndex = (unsigned char)idx;
		g_swDevices[idx].info = info;
		g_swDevices[idx].openCount = 0;
		swAllocPrivateData(info, &priv);
	} else {
		if (g_swDevices[idx].openCount >= 4) {
			swMwTrace("sw: sw_open: max open count exceeded %d\n",
				  g_swDevices[idx].openCount);
			return -1;
		}
		swAllocPrivateData(info, &priv);
	}

	if (priv != NULL) {
		int slot = 4 * idx + g_swDevices[idx].openCount;

		g_swOpenFds[slot] = priv;
		fd = slot | 0xA000;
		++g_swDevices[idx].openCount;
	} else {
		fd = -1;
		if (g_swDevices[idx].openCount == 0) {
			g_swDevices[idx].info = NULL;
			swDepFiniLib(info->swInfo->depCore);
		}
	}
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: sw_open: devname=%s retval=%d\n", devName, fd);
	return fd;
}

static void swDoRelease(struct SwFilePrivate *priv)
{
	struct SwWgnInfo *info = priv->wgnInfo;
	struct SwListNode *node;
	struct SwListNode *next;
	unsigned char s;
	int p;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: sw_do_release:\n");
	++priv->closing;
	for (s = 0; s < SW_MAX_STREAMS; ++s) {
		if (priv->started[s] != 0)
			swForceStopStream(info, s, priv);
		if (priv->active[s] != 0) {
			for (p = 0; p < 2; ++p)
				swMarkPortDone(info, s, p, priv);
		}
	}
	node = priv->cdevList.next;
	while (node != &priv->cdevList) {
		struct SwCdevRefNode *entry = (struct SwCdevRefNode *)node;

		next = node->next;
		info->cdevRefCnt[entry->cdevId] -= entry->refCount;
		if (info->cdevRefCnt[entry->cdevId] <= 0)
			swExecCommand(info, 0x80047833u, (void *)(uintptr_t)entry->cdevId, 0, NULL);
		swFreeMem(info, entry, sizeof(*entry));
		node = next;
	}
	swFreeMem(info, priv, sizeof(*priv));
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: sw_do_release: done\n");
}

int swClose(int fd)
{
	struct SwFilePrivate *priv;
	struct SwWgnInfo *info;

	if (!swCheckFd(fd)) {
		swMwTrace("sw: sw_close invalid fd(%d)\n", fd);
		return -1;
	}
	priv = g_swOpenFds[fd & 0xFFF];
	g_swOpenFds[fd & 0xFFF] = NULL;
	if (priv == NULL) {
		swMwTrace("sw: sw_close invalid fd (pdata==NULL)\n");
		return -1;
	}
	info = priv->wgnInfo;
	if (info == NULL) {
		swMwTrace("sw: sw_close invalid fd(%d,%p)\n", fd, NULL);
		return -1;
	}
	if (info->devIndex != 0 || g_swDevices[0].info != info) {
		swMwTrace("sw: sw_close ioctl invalid fd(%d)\n", fd);
		return -1;
	}
	swDoRelease(priv);
	if (--g_swDevices[0].openCount == 0) {
		g_swDevices[0].info = NULL;
		swDepFiniLib(info->swInfo->depCore);
	}
	return 0;
}

static void swCleanupResetState(struct SwWgnInfo *info, struct SwFilePrivate *priv)
{
	int s, p;
	struct SwListNode *node;
	struct SwListNode *next;

	for (s = 0; s < SW_MAX_STREAMS; ++s) {
		for (p = 0; p < 2; ++p) {
			struct SwDmaPort *port = &info->stream[s].port[p];

			if ((port->stateFlags & 2u) != 0)
				swUnmapDmaPort(info, port);
			swResetDmaPort(info, port);
		}
	}
	node = priv->cdevList.next;
	while (node != &priv->cdevList) {
		struct SwCdevRefNode *entry = (struct SwCdevRefNode *)node;

		next = node->next;
		info->cdevRefCnt[entry->cdevId] -= entry->refCount;
		if (info->cdevRefCnt[entry->cdevId] <= 0) {
			info->cdevRefCnt[entry->cdevId] = 0;
			swExecCommand(info, 0x80047833u, (void *)(uintptr_t)entry->cdevId, 0, NULL);
		}
		swListRemoveNode(node);
		swFreeMem(info, entry, sizeof(*entry));
		node = next;
	}
}

int swResetWagner(struct SwWgnInfo *info, struct SwFilePrivate *priv)
{
	int idle = 0;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: reset_wagner:\n");
	if (info->stream[0].route == 0 && swStreamDoneCheck(info, 0) == 0)
		idle = 1;
	if (!idle) {
		swMwTrace("sw: WAGNER_IOC_RST_WAGNER, device busy\n");
		return 9;
	}
	swCleanupResetState(info, priv);
	swWgnReset(info->wgnCtx);
	swDepCoreResetWrap(info->swInfo);
	swFiniWgnInfo(info);
	swInitWgnInfo(info);
	return 0;
}

int swExecCommand(struct SwWgnInfo *info, uint32_t cmd, void *arg, uint32_t argLen, int *outStatus)
{
	int ret;

	swLockMtx(&info->cmdMutex);
	ret = swWgnCommand(info->wgnCtx, cmd, arg, argLen);
	if (outStatus != NULL)
		*outStatus = ret;
	swUnlockMtx(&info->cmdMutex);
	return 0;
}

static int swIoctlGenCdev(struct SwWgnInfo *info, uint32_t cmd, unsigned char *arg,
			  uint32_t argLen, struct SwFilePrivate *priv)
{
	int cmdStatus = 0;
	int ret;
	uint32_t cdevId;
	struct SwListNode *node;
	struct SwCdevRefNode *found = NULL;

	ret = swExecCommand(info, cmd, arg, argLen, &cmdStatus);
	if (ret != 0)
		return ret;
	if (cmdStatus != 0) {
		swMwTrace("sw: WAGNER_IOC_GEN_[SRC/SINK]CDEV(%08x) return error(%08x)\n",
			  cmd, cmdStatus);
		return 4;
	}
	if (arg[2] != 0)
		return 0;
	cdevId = arg[0];
	if (cdevId >= SW_MAX_CDEVS) {
		swMwTrace("sw: WAGNER_IOC_GEN_[SRC/SINK]CDEV content device ID(%d) >= %d\n",
			  cdevId, SW_MAX_CDEVS);
		swExecCommand(info, 0x80047833u, (void *)(uintptr_t)cdevId, 0, NULL);
		return 2;
	}
	for (node = priv->cdevList.next; node != &priv->cdevList; node = node->next) {
		struct SwCdevRefNode *entry = (struct SwCdevRefNode *)node;

		if (entry->cdevId == cdevId) {
			found = entry;
			break;
		}
	}
	if (found == NULL) {
		struct SwCdevRefNode *entry = (struct SwCdevRefNode *)swAllocMem(info, sizeof(*entry), 0);

		if (entry == NULL) {
			swMwTrace("sw: WAGNER_IOC_GEN_[SRC/SINK]CDEV alloc list fail\n");
			swExecCommand(info, 0x80047833u, (void *)(uintptr_t)cdevId, 0, NULL);
			return 7;
		}
		entry->cdevId = cdevId;
		entry->refCount = 1;
		swListAddTail(&entry->link, &priv->cdevList);
	} else {
		++found->refCount;
	}
	++info->cdevRefCnt[cdevId];
	return 0;
}

static int swIoctlDelCdev(struct SwWgnInfo *info, struct SwFilePrivate *priv, uint32_t cdevId)
{
	struct SwListNode *node;
	struct SwCdevRefNode *found = NULL;
	int cmdStatus = 0;

	if (cdevId >= SW_MAX_CDEVS) {
		swMwTrace("sw: WAGNER_IOC_DEL_CDEV content device ID(%d) < 0(or >= %d)\n",
			  cdevId, SW_MAX_CDEVS);
		return 13;
	}
	for (node = priv->cdevList.next; node != &priv->cdevList; node = node->next) {
		struct SwCdevRefNode *entry = (struct SwCdevRefNode *)node;

		if (entry->cdevId == cdevId) {
			found = entry;
			break;
		}
	}
	if (found == NULL) {
		swMwTrace("sw: WAGNER_IOC_DEL_CDEV No such content device ID(%d)\n", cdevId);
		return 11;
	}
	if (info->cdevRefCnt[cdevId] - 1 > 0) {
		--info->cdevRefCnt[cdevId];
	} else {
		int ret = swExecCommand(info, 0x80047833u, (void *)(uintptr_t)cdevId, 0, &cmdStatus);

		if (ret != 0)
			return ret;
		if (cmdStatus != 0) {
			swMwTrace("sw: WAGNER_IOC_DEL_CDEV return  error(%08x)\n", cmdStatus);
			return 4;
		}
		info->cdevRefCnt[cdevId] = 0;
	}
	if (found->refCount - 1 > 0) {
		--found->refCount;
	} else {
		swListRemoveNode(&found->link);
		swFreeMem(info, found, sizeof(*found));
	}
	return 0;
}

int swStreamDoneCheck(struct SwWgnInfo *info, unsigned char streamId)
{
	int p;

	for (p = 0; p < 2; ++p) {
		struct SwDmaPort *port = &info->stream[streamId].port[p];

		if (port->doneHead != NULL && (port->stateFlags & 0x400u) == 0)
			return -1;
	}
	return 0;
}

void swFreeStreamResource(struct SwWgnInfo *info, int streamId)
{
	unsigned char param[4];

	swFreeStreamWrap(info->swInfo, (unsigned char)streamId);
	memset(param, 0, sizeof(param));
	param[0] = (unsigned char)streamId;
	swExecCommand(info, 0xC0017816u, param, sizeof(param), NULL);
}

void swMarkPortDone(struct SwWgnInfo *info, unsigned char streamId, int portIdx,
		    struct SwFilePrivate *priv)
{
	struct SwStreamState *st = &info->stream[streamId];
	struct SwDmaPort *port = &st->port[portIdx];

	port->stateFlags |= 0x400u;
	if (port->direction == 1)
		info->stats.txStat[streamId].state = 0;
	else
		info->stats.rxStat[streamId].state = 0;

	while (port->doneHead != NULL) {
		struct SwDmaDoneInfo *node = port->doneHead;

		port->doneHead = node->next;
		swRecycleDoneInfo(port, node);
	}
	swFreeDonePool(info, port);
	if (swStreamDoneCheck(info, streamId) == 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: reports done #%d\n", streamId);
		priv->active[streamId] = 0;
		info->stats.active[streamId] = 0;
		if (st->route == 0)
			swFreeStreamResource(info, streamId);
	}
}

static int swValidateRoute(uint32_t route)
{
	switch (route) {
	case 0x10010002u:
	case 0x10040002u:
	case 0x10080002u:
	case 0x10100002u:
	case 0x10200002u:
	case 0x10400002u:
	case 0x10800002u:
	case 0x11000002u:
	case 0x12000002u:
	case 0x50010001u:
	case 0x50010004u:
	case 0x50010008u:
	case 0x50010010u:
	case 0x50010020u:
	case 0x50010040u:
	case 0x50010080u:
	case 0x50010100u:
	case 0x50010200u:
	case 0x50040001u:
	case 0x50080001u:
	case 0x50100001u:
	case 0x50200001u:
	case 0x50400001u:
	case 0x50800001u:
	case 0x51000001u:
	case 0x52000001u:
	case 0x90020001u:
	case 0x90020004u:
	case 0x90020008u:
	case 0x90020010u:
	case 0x90020020u:
	case 0x90020040u:
	case 0x90020080u:
	case 0x90020100u:
	case 0x90020200u:
		return 1;
	default:
		return 0;
	}
}

static int swCheckStartTransParam(struct SwWgnInfo *info, unsigned char *param)
{
	uint32_t route = *(uint32_t *)(param + 64);
	int tsif = 0;

	if ((route & 0x10000000u) == 0) {
		switch (route) {
		case 0x00020400u:
			route = 0x10010002u;
			break;
		case 0x00080400u:
			route = 0x10040002u;
			break;
		case 0x00110400u:
			route = 0x10080002u;
			break;
		case 0x40020200u:
			route = 0x50010001u;
			break;
		case 0x40020800u:
		case 0x40021100u:
			route = 0x50010004u;
			break;
		case 0x40080200u:
			route = 0x50040001u;
			break;
		case 0x40110200u:
			route = 0x50080001u;
			break;
		case 0x80040200u:
			route = 0x90020001u;
			break;
		case 0x80040800u:
			route = 0x90020004u;
			break;
		case 0x80041100u:
			route = 0x90020008u;
			break;
		default:
			swMwTrace("sw: WAGNER_IOC_STT_TRANS illegal route(0x%08x)\n", route);
			return 11;
		}
		*(uint32_t *)(param + 64) = route;
	} else if (!swValidateRoute(route)) {
		swMwTrace("sw: WAGNER_IOC_STT_TRANS illegal route(0x%08x)\n", route);
		return 11;
	}

	if ((route & 0x03FC0000u) != 0) {
		uint32_t inBits = route & 0x03FC0000u;

		switch (inBits) {
		case 0x02000000u:
			tsif = 7;
			break;
		case 0x01000000u:
			tsif = 6;
			break;
		case 0x00800000u:
			tsif = 5;
			break;
		case 0x00400000u:
			tsif = 4;
			break;
		case 0x00200000u:
			tsif = 3;
			break;
		case 0x00100000u:
			tsif = 2;
			break;
		default:
			tsif = (inBits == 0x00080000u) ? 1 : 0;
			break;
		}
	} else if ((route & 0x03FCu) != 0) {
		uint32_t outBits = route & 0x03FCu;
		int s;

		switch (outBits) {
		case 0x200u:
			tsif = 7;
			break;
		case 0x100u:
			tsif = 6;
			break;
		case 0x80u:
			tsif = 5;
			break;
		case 0x40u:
			tsif = 4;
			break;
		case 0x20u:
			tsif = 3;
			break;
		case 0x10u:
			tsif = 2;
			break;
		default:
			tsif = (outBits == 8u) ? 1 : 0;
			break;
		}
		for (s = 0; s < SW_MAX_STREAMS; ++s) {
			if ((info->stream[s].route & 0x03FCu) == outBits) {
				swMwTrace("sw: WAGNER_IOC_STT_TRANS, out TS/IF(%d) conflicts\n",
					  tsif);
				return 11;
			}
		}
	}

	if (tsif > 0) {
		swMwTrace("sw: WAGNER_IOC_STT_TRANS, max tsif exceeded\n");
		return 11;
	}
	if ((route & 2u) != 0) {
		unsigned char encMode = param[92];

		switch (encMode) {
		case 0:
		case 2:
		case 4:
		case 6:
		case 8:
		case 0x0A:
		case 0x0C:
		case 0xFF:
			if (*(int *)(param + 24) > 0x08000000) {
				swMwTrace("sw: WAGNER_IOC_STT_TRANS, illegal sync_len(0x%08x)\n",
					  *(uint32_t *)(param + 24));
				return 11;
			}
			break;
		default:
			swMwTrace("sw: WAGNER_IOC_STT_TRANS, illegal enc_mode(0x%02x)\n", encMode);
			return 11;
		}
	}
	if (param[89] == 1) {
		swMwTrace("sw: WAGNER_IOC_STT_TRANS, PS not supported\n");
		return 11;
	}
	return tsif;
}

static int swIoctlStartTransfer(struct SwWgnInfo *info, struct SwFilePrivate *priv,
				unsigned char *param)
{
	int tsif;
	unsigned char preReq[24];
	int cmdStatus = 0;
	unsigned char streamId;
	struct SwStreamState *st;
	int p, ret;

	tsif = swCheckStartTransParam(info, param);
	if (tsif < 0)
		return tsif;

	memset(preReq, 0, sizeof(preReq));
	preReq[0] = param[0];
	preReq[2] = param[4];
	if (preReq[2] != 0)
		memcpy(preReq + 4, param + 5, 8);
	*(uint32_t *)(preReq + 12) = *(uint32_t *)(param + 64);
	*(uint32_t *)(preReq + 16) = *(uint32_t *)(param + 24);
	preReq[20] = param[93];

	swExecCommand(info, 0xC4B07814u, preReq, sizeof(preReq), &cmdStatus);
	if (cmdStatus != 0) {
		if (cmdStatus == 4)
			return 7;
		if (cmdStatus == 3)
			return 6;
		return 11;
	}

	param[72] = preReq[3];
	streamId = preReq[1];
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: start_transfer %d\n", streamId);

	st = &info->stream[streamId];
	st->route = *(uint32_t *)(param + 64);
	st->lastCount = 0;
	st->flags = 0;
	for (p = 0; p < 2; ++p)
		swResetDmaPort(info, &st->port[p]);
	swClearDepStream(info->swInfo, streamId);

	ret = swSetupAllDmacs(info, priv, streamId, st->route, (const uint32_t *)param);
	if (ret != 0) {
		st->route = 0;
		return ret;
	}

	ret = swDepPrepareTrans(info->swInfo, param, info->tsifMode[tsif], streamId);
	if (ret != 0) {
		if ((g_swWgnLogMask & 1u) != 0)
			swMwTrace("sw: dep_prepare/start_trans: error %d\n", ret);
		for (p = 0; p < 2; ++p) {
			if ((st->port[p].stateFlags & 2u) != 0) {
				swMarkDmaDone(info, streamId, p);
				swPostDoneDma(info, streamId, p, priv);
				swMarkPortDone(info, streamId, p, priv);
			}
		}
		st->route = 0;
		return ret;
	}

	info->stats.route[streamId] = st->route;
	info->stats.active[streamId] = 1;
	memset(&info->stats.rxStat[streamId], 0, sizeof(info->stats.rxStat[streamId]));
	memset(&info->stats.txStat[streamId], 0, sizeof(info->stats.txStat[streamId]));
	if ((st->txDmapNo & 0x80) != 0) {
		info->stats.txStat[streamId].state = 2;
		++info->stats.txStat[streamId].startCount;
	}
	if ((st->rxDmapNo & 0x80) != 0) {
		info->stats.rxStat[streamId].state = 2;
		++info->stats.rxStat[streamId].startCount;
	}
	param[1] = streamId;
	priv->active[streamId] = 1;
	priv->started[streamId] = 1;
	st->flags |= 1u;
	return 0;
}

static int swIoctlStopTransfer(struct SwWgnInfo *info, const unsigned char *arg, int force,
			       struct SwFilePrivate *priv)
{
	unsigned char streamId = arg[0];
	struct SwStreamState *st;
	struct SwDmaPort *locked0;
	struct SwDmaPort *locked1;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: stop_transfer: %d force=%d\n", streamId, force);
	if (streamId != 0) {
		swMwTrace("sw: WAGNER_IOC_STP_TRANS, illegal stream#%d\n", streamId);
		return 11;
	}
	if (priv->started[0] == 0)
		swMwTrace("sw: WAGNER_IOC_STP_TRANS, stream#%d isn't started with this file descriptor: pdata = %p\n",
			  0, priv);

	st = &info->stream[0];
	if ((st->flags & 2u) != 0) {
		swMwTrace("sw: WAGNER_IOC_STP_TRANS, stream#%d: already call WAGNER_IOC_STP_TRANS ioctl\n",
			  0);
		return 12;
	}

	locked0 = ((st->port[0].stateFlags & 1u) == 0) ? &st->port[0] : NULL;
	if (locked0 != NULL)
		swLockMtx(&locked0->opMutex);
	locked1 = ((st->port[1].stateFlags & 1u) == 0) ? &st->port[1] : NULL;
	if (locked1 != NULL)
		swLockMtx(&locked1->opMutex);

	swStopStreamWrap(info->swInfo, 0);
	if ((st->route & 0x30000u) != 0) {
		int txPortIdx = ((st->route & 0x20000u) == 0) ? 1 : 0;
		struct SwDmaPort *txPort = &st->port[txPortIdx];

		if ((txPort->stateFlags & 0x10u) != 0) {
			if ((txPort->stateFlags & 0x200u) == 0)
				swPostDoneDma(info, 0, txPortIdx, priv);
		} else if (st->lastCount != 0 && (g_swWgnLogMask & 1u) != 0) {
			swMwTrace("sw: WAGNER_IOC_STP_TRANS #%d not DMA_DONE yet after set last\n", 0);
		}
	}

	st->flags |= 2u;
	if ((st->flags & 4u) != 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: Free Stream #%d\n", 0);
		st->route = 0;
		priv->started[0] = 0;
		info->stats.route[0] = 0;
		if (swStreamDoneCheck(info, 0) == 0)
			swFreeStreamResource(info, 0);
	}

	if (locked0 != NULL)
		swUnlockMtx(&locked0->opMutex);
	if (locked1 != NULL)
		swUnlockMtx(&locked1->opMutex);
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: stop_transfer: done\n");
	return 0;
}

static int swQueryStreamNc(struct SwWgnInfo *info, unsigned char streamId, void *outNc)
{
	unsigned char req[12];
	int cmdStatus = 0;
	int ret;

	memset(req, 0, sizeof(req));
	req[0] = streamId;
	ret = swExecCommand(info, 0xC0507817u, req, sizeof(req), &cmdStatus);
	if (ret != 0)
		return ret;
	if (cmdStatus != 0) {
		swMwTrace("sw: SYC_TRANS command(%08x) return error(%08x)\n",
			  0xC0507817u, cmdStatus);
		return 4;
	}
	memcpy(outNc, req + 4, 8);
	return 0;
}

struct SwDmaDoneInfo *swAllocDoneInfo(struct SwWgnInfo *info, struct SwDmaPort *port, int unused)
{
	struct SwDmaDoneInfo *node;

	if (port->doneFree != NULL) {
		node = port->doneFree;
		port->doneFree = node->next;
	} else {
		node = (struct SwDmaDoneInfo *)swAllocMem(info, sizeof(*node), unused);
	}
	memset(node, 0, sizeof(*node));
	return node;
}

void swRecycleDoneInfo(struct SwDmaPort *port, struct SwDmaDoneInfo *node)
{
	node->next = port->doneFree;
	port->doneFree = node;
}

void swFreeDonePool(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	while (port->doneFree != NULL) {
		struct SwDmaDoneInfo *node = port->doneFree;

		port->doneFree = node->next;
		swFreeMem(info, node, sizeof(*node));
	}
}

static void swWritePcpContentLength(struct SwDmaPort *port, int pos, uint32_t contentLen)
{
	unsigned char *hdr;

	if (port->direction != 0 && (g_swWgnLogMask & 1u) != 0)
		swMwTrace("sw: write cl at not DMA read mode\n");
	hdr = (unsigned char *)port->wsgl[pos].vaddr + 10;
	hdr[0] = (unsigned char)(contentLen >> 24);
	hdr[1] = (unsigned char)(contentLen >> 16);
	hdr[2] = (unsigned char)(contentLen >> 8);
	hdr[3] = (unsigned char)contentLen;
}

static void swAppendDoneInfo(struct SwWgnInfo *info, struct SwDmaPort *port)
{
	struct SwDmaDoneInfo *node = swAllocDoneInfo(info, port, 0);
	int nextPos;

	if (node == NULL) {
		swMwTrace("sw: alloc last done info fail\n");
		port->stateFlags |= 0x100000u;
		return;
	}
	port->doneTail->next = node;
	port->doneTail = node;
	nextPos = (port->curPos + 1 < port->nWsgl) ? (port->curPos + 1) : 0;
	if (port->unitLen != 0 && port->unitHdrLen != 0)
		node->addr = (uintptr_t)port->userBase + port->wsgl[nextPos].offset - port->unitHdrLen;
	else
		node->addr = (uintptr_t)port->userBase + port->wsgl[nextPos].offset;
	node->length = 0;
}

static void swRxCompPcpSmall(struct SwWgnInfo *info, int streamId, uint32_t contentLen)
{
	struct SwStreamState *st = &info->stream[streamId];
	int portIdx = st->rxDmapNo & 1;
	struct SwDmaPort *port = &st->port[portIdx];
	struct SwDmaSgl *wsgl = port->wsgl;
	uint32_t flags;

	if ((port->stateFlags & 2u) == 0 || (port->stateFlags & 0x200u) != 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: ERROR DMA not in use %d\n", streamId);
		return;
	}

	swLockMtx(&port->dmaMutex);
	flags = wsgl[port->curPos].flags;
	while ((int32_t)flags >= 0) {
		uint32_t xferLen;

		if ((flags & 0x100000u) != 0) {
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: ERROR SGL_DONE %d cur_pos=%d\n", streamId, port->curPos);
			break;
		}
		xferLen = wsgl[port->curPos].xferLen;
		port->doneTail->length += xferLen;
		port->doneTail->pos = port->curPos;
		info->stats.rxStat[streamId].totalBytes += xferLen;
		if ((flags & 0x08000000u) != 0) {
			++info->stats.rxStat[streamId].clChgCount;
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: CL_CHG#%d len=%04x cl=%04x\n",
					  streamId, xferLen, contentLen);
			swWritePcpContentLength(port, port->curPos, contentLen);
			swAppendDoneInfo(info, port);
		} else if (port->curPos + 1 >= port->nWsgl ||
			   (wsgl[port->curPos + 1].offset - wsgl[port->curPos].offset) > port->syncLen) {
			swAppendDoneInfo(info, port);
		}
		if (port->waiting != 0) {
			port->waiting = 0;
			swSignalEvent(&port->event);
		}
		wsgl[port->curPos].flags = flags | 0x100000u;
		if (++port->curPos >= port->nWsgl)
			port->curPos = 0;
		if ((flags & 0x20000000u) != 0) {
			++info->stats.rxStat[streamId].lastDoneCount;
			swMarkDmaDone(info, streamId, portIdx);
			break;
		}
		flags = wsgl[port->curPos].flags;
	}
	swUnlockMtx(&port->dmaMutex);
}

static void swFlushPcpLargeDone(struct SwWgnInfo *info, struct SwDmaPort *port, int streamId)
{
	port->doneTail->length = port->recvLenLo;
	port->doneTail->pos = (port->recvLenHi != 0) ? (port->nWsgl - 1) : port->curPos;
	info->stats.rxStat[streamId].totalBytes += port->recvLenLo;
	swAppendDoneInfo(info, port);
	if (port->recvLenHi != 0) {
		port->doneTail->addr = (uintptr_t)port->userBase + port->wsgl[0].offset;
		port->doneTail->length = port->recvLenHi;
		port->doneTail->pos = port->curPos;
		info->stats.rxStat[streamId].totalBytes += port->recvLenHi;
		swAppendDoneInfo(info, port);
	}
	port->pcpStartPos = (port->curPos + 1 < port->nWsgl) ? (port->curPos + 1) : 0;
	port->recvLenHi = 0;
	port->recvLenLo = 0;
	port->recvLenPtr = &port->recvLenLo;
}

static void swRxCompPcpLarge(struct SwWgnInfo *info, int streamId, uint32_t contentLen)
{
	struct SwStreamState *st = &info->stream[streamId];
	int portIdx = st->rxDmapNo & 1;
	struct SwDmaPort *port = &st->port[portIdx];
	struct SwDmaSgl *wsgl = port->wsgl;
	uint32_t flags;

	if ((port->stateFlags & 2u) == 0 || (port->stateFlags & 0x200u) != 0) {
		if ((g_swWgnLogMask & 4u) != 0)
			swMwTrace("sw: ERROR DMA not in use %d\n", streamId);
		return;
	}

	swLockMtx(&port->dmaMutex);
	flags = wsgl[port->curPos].flags;
	while ((int32_t)flags >= 0) {
		if ((flags & 0x100000u) != 0) {
			if ((g_swWgnLogMask & 1u) != 0)
				swMwTrace("sw: ERROR SGL_DONE %d cur_pos=%d\n", streamId, port->curPos);
			break;
		}
		*port->recvLenPtr += wsgl[port->curPos].xferLen;
		if (port->curPos + 1 >= port->nWsgl)
			port->recvLenPtr = &port->recvLenHi;
		if ((port->recvLenLo + port->recvLenHi) >= port->syncLen) {
			swFlushPcpLargeDone(info, port, streamId);
			if (port->waiting != 0) {
				port->waiting = 0;
				swSignalEvent(&port->event);
			}
		} else if ((flags & 0x08000000u) != 0) {
			++info->stats.rxStat[streamId].clChgCount;
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: CL_CHG %d, recvlen=%08x, cl=%08x\n",
					  streamId, port->recvLenLo + port->recvLenHi, contentLen);
			swWritePcpContentLength(port, port->pcpStartPos, contentLen);
			swFlushPcpLargeDone(info, port, streamId);
			if (port->waiting != 0) {
				port->waiting = 0;
				swSignalEvent(&port->event);
			}
		}
		wsgl[port->curPos].flags = flags | 0x100000u;
		if (++port->curPos >= port->nWsgl)
			port->curPos = 0;
		if ((flags & 0x20000000u) != 0) {
			++info->stats.rxStat[streamId].lastDoneCount;
			swMarkDmaDone(info, streamId, portIdx);
			break;
		}
		flags = wsgl[port->curPos].flags;
	}
	swUnlockMtx(&port->dmaMutex);
}

static void swDmaCompGeneric(struct SwWgnInfo *info, int streamId, int isTx)
{
	struct SwStreamState *st = &info->stream[streamId];
	int dmapNo = (isTx == 1) ? st->txDmapNo : st->rxDmapNo;
	int portIdx = dmapNo & 1;
	struct SwDmaPort *port = &st->port[portIdx];
	struct SwDmaSgl *wsgl = port->wsgl;
	struct SwDmaStat *stat = (isTx == 1) ? &info->stats.txStat[streamId] :
					       &info->stats.rxStat[streamId];
	uint32_t flags;

	if ((port->stateFlags & 2u) == 0 || (port->stateFlags & 0x200u) != 0)
		return;

	swLockMtx(&port->dmaMutex);
	flags = wsgl[port->curPos].flags;
	while ((int32_t)flags >= 0 && (flags & 0x100000u) == 0) {
		uint32_t xferLen = wsgl[port->curPos].xferLen;

		if (port->unitLen != 0 && port->unitHdrLen != 0 && xferLen != 0) {
			if (port->unitAccum == 0)
				xferLen += port->unitHdrLen;
			port->unitAccum += wsgl[port->curPos].xferLen;
			if (port->unitAccum >= port->unitLen - port->unitHdrLen)
				port->unitAccum = 0;
		}
		port->doneTail->length += xferLen;
		port->doneTail->pos = port->curPos;
		stat->totalBytes += xferLen;
		if (port->curPos + 1 >= port->nWsgl)
			swAppendDoneInfo(info, port);
		if (port->waiting != 0) {
			port->waiting = 0;
			swSignalEvent(&port->event);
		}
		wsgl[port->curPos].flags = flags | 0x100000u;
		if (++port->curPos >= port->nWsgl)
			port->curPos = 0;
		if ((flags & 0x20000000u) != 0) {
			++stat->lastDoneCount;
			swMarkDmaDone(info, streamId, portIdx);
			break;
		}
		flags = wsgl[port->curPos].flags;
	}
	swUnlockMtx(&port->dmaMutex);
}

int swTxCompCallback(struct SwWgnInfo *info, unsigned char streamId)
{
	swDmaCompGeneric(info, streamId, 1);
	return 1;
}

int swTxErrCallback(struct SwWgnInfo *info, unsigned char streamId)
{
	struct SwStreamState *st = &info->stream[streamId];

	if ((st->txDmapNo & 0x80) != 0) {
		int portIdx = st->txDmapNo & 1;

		swMarkDmaDone(info, streamId, portIdx);
		st->port[portIdx].stateFlags |= 0x10000u;
	}
	return 1;
}

int swRxCompCallback(struct SwWgnInfo *info, unsigned char streamId, uint32_t contentLen)
{
	struct SwStreamState *st = &info->stream[streamId];

	if ((st->route & 2u) != 0) {
		int portIdx = st->rxDmapNo & 1;

		if (st->port[portIdx].syncLen <= SW_PAGE_CHUNK_SIZE)
			swRxCompPcpSmall(info, streamId, contentLen);
		else
			swRxCompPcpLarge(info, streamId, contentLen);
	} else {
		swDmaCompGeneric(info, streamId, 0);
	}
	return 1;
}

int swRxErrCallback(struct SwWgnInfo *info, unsigned char streamId)
{
	struct SwStreamState *st = &info->stream[streamId];

	if ((st->rxDmapNo & 0x80) != 0) {
		int portIdx = st->rxDmapNo & 1;

		swMarkDmaDone(info, streamId, portIdx);
		st->port[portIdx].stateFlags |= 0x10000u;
	}
	return 1;
}

void *swAllocMem(struct SwWgnInfo *info, size_t size, int unused)
{
	void *ptr;

	(void)unused;
	ptr = malloc(size);
	if (ptr != NULL) {
		memset(ptr, 0, size);
		if (info != NULL) {
			swLockMtx(&info->stats.memMutex);
			info->stats.allocBytes += (int)size;
			swUnlockMtx(&info->stats.memMutex);
		} else {
			struct SwWgnInfo *self = (struct SwWgnInfo *)ptr;

			self->stats.allocBytes = (int)size;
			swCreateMtx(&self->stats.memMutex);
		}
	}
	return ptr;
}

void swFreeMem(struct SwWgnInfo *info, void *ptr, int size)
{
	swLockMtx(&info->stats.memMutex);
	info->stats.allocBytes -= size;
	swUnlockMtx(&info->stats.memMutex);
	if ((void *)info == ptr) {
		if (info->stats.allocBytes != 0)
			swMwTrace("sw: possible memory leak(%d byte)\n", info->stats.allocBytes);
		swDeleteMtx(&info->stats.memMutex);
	}
	free(ptr);
}

int swInitWgnInfo(struct SwWgnInfo *info)
{
	int i;

	swCreateMtx(&info->cmdMutex);
	for (i = 0; i < SW_MAX_STREAMS; ++i)
		memset(&info->stream[i], 0, sizeof(info->stream[i]));
	for (i = 0; i < SW_MAX_STREAMS; ++i)
		info->tsifMode[i] = 0;
	for (i = 0; i < SW_MAX_CDEVS; ++i)
		info->cdevRefCnt[i] = 0;
	return 0;
}

void swFiniWgnInfo(struct SwWgnInfo *info)
{
	swDeleteMtx(&info->cmdMutex);
}

static void swBuildSyncStatus(struct SwDmaPort *port, uint32_t streamBits,
			      unsigned char *outCode, uint32_t *outDetail)
{
	*outCode = 0;
	*outDetail = 0;
	if ((port->stateFlags & 0x10u) != 0) {
		if ((port->stateFlags & 0x10000u) != 0) {
			*outCode = 3;
			*outDetail = 0x80000000u;
		} else if ((streamBits & 0x0FFFFFF0u) != 0) {
			*outCode = 2;
			*outDetail |= streamBits & 0x0FFFFFF0u;
		} else {
			*outCode = 1;
		}
	} else if ((port->stateFlags & 0x103000u) != 0 || (streamBits & 0x0FFFFFF0u) != 0) {
		*outCode = 2;
		if ((port->stateFlags & 0x1000u) != 0)
			*outDetail |= 0x10000000u;
		if ((port->stateFlags & 0x2000u) != 0)
			*outDetail |= 0x20000000u;
		if ((port->stateFlags & 0x100000u) != 0)
			*outDetail |= 0x40000000u;
		if ((streamBits & 0x0FFFFFF0u) != 0)
			*outDetail |= streamBits & 0x0FFFFFF0u;
	}
}

static void swFinishSyncResponse(struct SwWgnInfo *info, struct SwStreamState *st,
				 struct SwDmaPort *port, int portIdx,
				 unsigned char *arg, struct SwFilePrivate *priv,
				 int hasData)
{
	uint32_t savedFlags;

	if (hasData) {
		*(uint32_t *)(arg + 8) = (uint32_t)port->doneHead->addr;
		*(uint32_t *)(arg + 12) = port->doneHead->length;
		port->lastSyncPos = port->doneHead->pos;
		if (port->doneHead == port->doneTail) {
			port->doneHead->addr += port->doneHead->length;
			port->doneHead->length = 0;
		} else {
			struct SwDmaDoneInfo *old = port->doneHead;

			port->doneHead = old->next;
			swRecycleDoneInfo(port, old);
		}
	} else {
		uint32_t addr = (port->doneHead != NULL) ? (uint32_t)port->doneHead->addr : 0;

		*(uint32_t *)(arg + 8) = addr;
		*(uint32_t *)(arg + 12) = 0;
	}

	savedFlags = port->stateFlags;
	if ((savedFlags & 0x113000u) != 0)
		port->stateFlags &= ~0x113000u;
	swUnlockMtx(&port->dmaMutex);

	if ((savedFlags & 0x10u) != 0 && (savedFlags & 0x200u) == 0)
		swPostDoneDma(info, 0, portIdx, priv);
	if (arg[73] == 1) {
		swMarkPortDone(info, 0, portIdx, priv);
		if (portIdx == 0 && (port->stateFlags & 8u) == 0)
			swQueryStreamNc(info, 0, arg + 65);
	}
	st->nextSyncPort = (portIdx == 0) ? 1 : 0;
	port->syncCalled = 1;
	swUnlockMtx(&port->opMutex);
}

static int swIoctlSyncTrans(struct SwWgnInfo *info, unsigned char *arg, struct SwFilePrivate *priv)
{
	unsigned char streamId = (unsigned char)*(uint32_t *)arg;
	struct SwStreamState *st;
	int portIdx;
	struct SwDmaPort *port;
	int bothActive = 0;
	int firstTry = 1;
	int elapsed = 0;
	int timeoutMs = *(int *)(arg + 4);

	if (streamId != 0) {
		swMwTrace("sw: sync_trans illegal stream#%d\n", streamId);
		return 11;
	}
	arg[73] = 0;
	st = &info->stream[0];

	if (arg[16] == 0) {
		portIdx = 0;
		port = &st->port[0];
		if ((port->stateFlags & 1u) != 0)
			return 2;
	} else if (arg[16] == 1) {
		portIdx = 1;
		port = &st->port[1];
		if ((port->stateFlags & 1u) != 0)
			return 2;
	} else if (arg[16] == 0xFFu) {
		portIdx = st->nextSyncPort;
		port = &st->port[portIdx];
		if ((port->stateFlags & 1u) != 0) {
			portIdx = (portIdx == 0) ? 1 : 0;
			port = &st->port[portIdx];
			if ((port->stateFlags & 1u) != 0) {
				swMwTrace("sw: sync_trans#%d ERR NOENT\n", 0);
				return 2;
			}
		} else {
			bothActive = ((st->port[(portIdx == 0) ? 1 : 0].stateFlags & 1u) == 0);
		}
		arg[16] = (unsigned char)portIdx;
	} else {
		swMwTrace("sw: sync_trans#%d ERR buff type(%d)\n", 0, arg[16]);
		return 11;
	}

	if ((st->route & 0x20000u) != 0)
		swGetCciInfo(info->swInfo, 0, arg + 20);

	for (;;) {
		int waited;

		for (;;) {
			uint32_t streamBits;

			swLockMtx(&port->opMutex);
			streamBits = swPollStreamStatus(info->swInfo, streamId, arg[16] == 0);
			swLockMtx(&port->dmaMutex);
			swBuildSyncStatus(port, streamBits, &arg[73], (uint32_t *)(arg + 76));

			if (port->doneHead != NULL && port->doneHead->length != 0) {
				if (arg[73] == 1)
					arg[73] = 0;
				swFinishSyncResponse(info, st, port, portIdx, arg, priv, 1);
				return 0;
			}
			if (arg[73] != 0) {
				if ((g_swWgnLogMask & 0x80u) != 0)
					swMwTrace("sw: stat_found#%d:%d %08x\n", 0, portIdx, arg[73]);
				swFinishSyncResponse(info, st, port, portIdx, arg, priv, 0);
				return 0;
			}
			if (firstTry != 0)
				port->waiting = 1;
			swUnlockMtx(&port->dmaMutex);
			if (port->doneHead == NULL) {
				if ((g_swWgnLogMask & 1u) != 0)
					swMwTrace("sw: sync_trans#%d:%d ERR no sync and status\n",
						  0, portIdx);
				swUnlockMtx(&port->opMutex);
				return 2;
			}
			if (!bothActive)
				break;
			swUnlockMtx(&port->opMutex);
			if (firstTry == 0) {
				firstTry = 1;
				portIdx = (portIdx == 0) ? 1 : 0;
				arg[16] = (unsigned char)portIdx;
				port = &st->port[portIdx];
				swLockMtx(&port->opMutex);
				break;
			}
			firstTry = 0;
			portIdx = (portIdx == 0) ? 1 : 0;
			arg[16] = (unsigned char)portIdx;
			port = &st->port[portIdx];
		}

		if (timeoutMs == 0 || (timeoutMs > 0 && elapsed >= timeoutMs)) {
			if (elapsed < timeoutMs) {
				swLockMtx(&port->dmaMutex);
				swFinishSyncResponse(info, st, port, portIdx, arg, priv, 1);
				return 0;
			}
			if ((g_swWgnLogMask & 0x80u) != 0)
				swMwTrace("sw: sync_trans#%d:%d timeout\n", 0, portIdx);
			swLockMtx(&port->dmaMutex);
			swFinishSyncResponse(info, st, port, portIdx, arg, priv, 0);
			return 0;
		}

		swUnlockMtx(&port->opMutex);
		waited = swWaitEventCond(&port->event, port->waiting == 0, 20);
		if (waited < 0) {
			if ((g_swWgnLogMask & 4u) != 0)
				swMwTrace("sw: event_wait error\n");
			return waited;
		}
		elapsed = elapsed - waited + 20;
	}
}

static int swIoctlMovEndp(struct SwWgnInfo *info, const unsigned char *arg)
{
	unsigned char streamId = (unsigned char)*(const uint32_t *)arg;
	uint32_t portIdx = arg[4];
	uint32_t offset = *(const uint32_t *)(arg + 8);
	struct SwStreamState *st;
	struct SwDmaPort *port;
	struct SwDmaSgl *wsgl;

	if (streamId != 0) {
		swMwTrace("sw: mov_endp illegal stream#%d\n", streamId);
		return 11;
	}
	if (portIdx >= 2u) {
		swMwTrace("sw: mov_endp#%d illegal dmap#%d\n", 0, portIdx);
		return 11;
	}
	if ((g_swWgnLogMask & 0x80u) != 0)
		swMwTrace("sw: mov_endp#%d:%d offset=%d\n", 0, portIdx, offset);

	st = &info->stream[0];
	port = &st->port[portIdx];
	wsgl = port->wsgl;
	swLockMtx(&port->opMutex);

	if ((st->flags & 2u) != 0) {
		if (port->direction == 1) {
			swMwTrace("sw: mov_endp#%d WARN already stp trans\n", 0);
			swUnlockMtx(&port->opMutex);
			return 0;
		}
		if ((port->stateFlags & 0x10u) != 0) {
			if ((port->stateFlags & 0x400u) != 0)
				swMwTrace("sw: mov_endp#%d:%d WARN already TRANS_DONE\n", 0, portIdx);
			swUnlockMtx(&port->opMutex);
			return 0;
		}
	}
	if ((port->stateFlags & 2u) == 0) {
		if ((g_swWgnLogMask & 0x80u) != 0)
			swMwTrace("sw: mov_endp#%d:%d dmap not used\n", 0, portIdx);
		swUnlockMtx(&port->opMutex);
		return 0;
	}
	if (offset > port->map.bufSize || offset == 0) {
		swMwTrace("sw: mov_endp#%d:%d ERR offset=%d > %d\n",
			  0, portIdx, offset, port->map.bufSize);
		swUnlockMtx(&port->opMutex);
		return 0;
	}
	if (port->syncCalled == 0) {
		swMwTrace("sw: mov_endp#%d:%d ERR not call sync trans\n", 0, portIdx);
		swUnlockMtx(&port->opMutex);
		return 0;
	}
	if (port->direction == 1 && (wsgl[port->endPos].flags & 0x20000000u) != 0) {
		swMwTrace("sw: mov_endp#%d:%d WARN already set last\n", 0, portIdx);
		swUnlockMtx(&port->opMutex);
		return 0;
	}

	{
		uint32_t prevLastLen = port->lastLen;
		int pos = 0;
		uint32_t rem = 0;

		port->lastLen = 0;
		while (pos < port->nWsgl) {
			if (offset <= wsgl[pos].offset + wsgl[pos].length) {
				if (offset >= wsgl[pos].offset) {
					rem = offset - wsgl[pos].offset;
				} else {
					pos = (pos != 0) ? (pos - 1) : (port->nWsgl - 1);
					rem = wsgl[pos].length;
				}
				port->lastLen = rem;
				break;
			}
			++pos;
		}
		if (pos >= port->nWsgl) {
			if (offset > port->map.bufSize) {
				swMwTrace("sw: mov_endp#%d:%d ERR move pos=%d >= %d\n",
					  0, portIdx, pos, port->nWsgl);
				swUnlockMtx(&port->opMutex);
				return 0;
			}
			pos = port->nWsgl - 1;
			rem = wsgl[pos].length;
			port->lastLen = rem;
		}

		if ((pos <= port->lastSyncPos &&
		     (port->lastSyncPos <= port->endPos || port->endPos < pos)) ||
		    (port->lastSyncPos <= port->endPos && port->endPos < pos)) {
			int targetPos;
			uint32_t flags;

			if (rem < wsgl[pos].length)
				pos = (pos != 0) ? (pos - 1) : (port->nWsgl - 1);
			else if (rem == wsgl[pos].length)
				port->lastLen = 0;

			port->syncCalled = 0;
			targetPos = pos;
			flags = wsgl[pos].flags;
			if (pos != port->endPos ||
			    ((flags & 0x80000000u) == 0 && port->lastLen <= prevLastLen)) {
				wsgl[pos].xferLen = wsgl[pos].length;
				wsgl[pos].flags = (flags & 0x7FEFFFFFu) | 0x80000000u;
				for (;;) {
					pos = (pos != 0) ? (pos - 1) : (port->nWsgl - 1);
					if (pos == port->endPos)
						break;
					wsgl[pos].xferLen = wsgl[pos].length;
					wsgl[pos].flags = (wsgl[pos].flags & 0x7FEFFFFFu) | 0x80000000u;
				}
				port->endPos = targetPos;
				if ((port->stateFlags & 0x10u) != 0)
					swMwTrace("sw: mov_endp#%d:#%d WARN already done\n",
						  0, portIdx);
				else
					swKickDmaPort(info, port, 0);
			}
		} else {
			swMwTrace("sw: mov_endp#%d:%d ERR move pos=%d is not between (%d-%d)\n",
				  0, portIdx, pos, port->endPos, port->lastSyncPos);
		}
	}
	swUnlockMtx(&port->opMutex);
	return 0;
}

static int swIoctlSetLast(struct SwWgnInfo *info, unsigned char streamId)
{
	struct SwStreamState *st;
	struct SwDmaPort *port;
	int nextPos;
	uint32_t flags;

	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: set_lastflag: %d\n", streamId);
	if (streamId != 0) {
		swMwTrace("sw: set_last illegal stream#%d\n", streamId);
		return 11;
	}
	st = &info->stream[0];
	if (st->lastCount != 0) {
		swMwTrace("sw: set_last%d already set last\n", 0);
		return 12;
	}
	if ((st->route & 0x20000u) != 0) {
		port = &st->port[0];
	} else if ((st->route & 0x10000u) != 0) {
		port = &st->port[1];
	} else if ((st->route & 0x03FC0000u) != 0) {
		swSetLastWrap(info->swInfo, 0);
		++st->lastCount;
		return 0;
	} else {
		swMwTrace("sw: set_last%d invalid route(%08x)\n", 0, st->route);
		return 11;
	}

	swLockMtx(&port->opMutex);
	if (port->wsgl == NULL) {
		swMwTrace("sw: set_last%d already buf freed\n", 0);
		swUnlockMtx(&port->opMutex);
		return 0;
	}
	nextPos = port->endPos + 1;
	if (nextPos >= port->nWsgl)
		nextPos = 0;
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: set_last%d winfop=%x headp=%x end_pos=%d nwsgl=%d\n",
			  0, (uint32_t)(uintptr_t)port, (uint32_t)(uintptr_t)port->wsgl,
			  port->endPos, port->nWsgl);
	flags = port->wsgl[nextPos].flags;
	if ((int32_t)flags < 0) {
		swUnlockMtx(&port->opMutex);
		return 6;
	}
	if ((g_swWgnLogMask & 4u) != 0)
		swMwTrace("sw: set_last%d last length=%d\n", 0, port->lastLen);
	port->wsgl[nextPos].xferLen = port->lastLen;
	port->wsgl[nextPos].flags = (flags & 0x5FEFFFFFu) | 0xA0000000u;
	port->endPos = nextPos;
	++st->lastCount;
	if (port->direction == 1)
		++info->stats.txStat[0].setLastCount;
	else
		++info->stats.rxStat[0].setLastCount;
	swKickDmaPort(info, port, 0);
	swUnlockMtx(&port->opMutex);
	return 0;
}

void swForceStopStream(struct SwWgnInfo *info, int streamId, struct SwFilePrivate *priv)
{
	uint32_t route = info->stream[streamId].route;
	unsigned char arg[4];

	arg[0] = (unsigned char)streamId;
	swIoctlStopTransfer(info, arg, 1, priv);
	if ((route & 3u) != 0) {
		int portIdx = ((route & 2u) != 0 || (route & 0x10000u) != 0) ? 0 : 1;

		swMarkDmaDone(info, streamId, portIdx);
		swPostDoneDma(info, streamId, portIdx, priv);
	}
	if ((route & 0x30000u) != 0) {
		int portIdx = ((route & 0x20000u) != 0) ? 0 : 1;

		swMarkDmaDone(info, streamId, portIdx);
		swPostDoneDma(info, streamId, portIdx, priv);
	}
}

int swDispatchIoctl(struct SwFilePrivate *priv, uint32_t cmd, unsigned char *arg, uint32_t argLen)
{
	struct SwWgnInfo *info = priv->wgnInfo;
	int cmdStatus = 0;

	switch (cmd) {
	case 0xC0017816u:
		return swIoctlStopTransfer(info, arg, 0, priv);
	case 0xC00878BCu:
		return swIoctlGenCdev(info, 0xC00878BCu, arg, argLen, priv);
	case 0xC00878D0u:
		return swIoctlGenCdev(info, 0xC00878D0u, arg, argLen, priv);
	case 0xC0507817u:
		return swIoctlSyncTrans(info, arg, priv);
	case 0xC4B07814u:
		return swIoctlStartTransfer(info, priv, arg);
	case 0x800C7818u:
		return swIoctlMovEndp(info, arg);
	case 0x80087888u:
		if (argLen < 0x14u || arg[0] != 0)
			return 11;
		info->tsifMode[arg[0]] = arg[15];
		return 0;
	case 0x80047833u:
		return swIoctlDelCdev(info, priv, *(uint32_t *)arg);
	case 0x8004781Au:
		return 0;
	case 0x80047819u:
		return swIoctlSetLast(info, (unsigned char)*(uint32_t *)arg);
	case 0x20007800u:
		return swResetWagner(info, priv);
	default:
		swExecCommand(info, cmd, arg, argLen, &cmdStatus);
		if (cmdStatus == 27) {
			swMwTrace("sw: command(%08x) return error(%08x)\n", cmd, 27);
			return 15;
		}
		if (cmdStatus != 0) {
			swMwTrace("sw: command(%08x) return error(%08x)\n", cmd, cmdStatus);
			return 4;
		}
		return 0;
	}
}
