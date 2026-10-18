/*
 * The DECI4P link manager: protocol slots, packet routing between the links
 * and the protocol modules, and power events (Sony's dfmgr.c).
 */

#include <sysmem/sysroot.h>
#include <sysmem/sysroot_dfmgr.h>
#include <string.h>
#include <sysmem/dipsw.h>
#include <psp2kern/kernel/intrmgr.h>
#include <modulemgr/modulemgr.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/sysevent.h>
#include "dfmgr_internal.h"
#include <deci4p_dfmgr/deci4p_packet.h>

static void sendCallback(DfMgrLinkCB *link);
static int _suspendHandlerQuery(int resume, int eventid, void *args, void *opt);

#define DFMGR_PROGRESS_MSG_MAX	63

static int s_dfmgrPowerEvents;
static SceUID s_dfmgrNextHandle;
static SceUInt32 s_dfmgrFlags;
static SceKernelSpinlock s_dfmgrLock;
static int s_dfmgrResumeRetries;
DfMgrProtocolCB s_dfmgrPd[DFMGR_PROTOCOL_COUNT];
DfMgrLinkCB s_dfmgrLink[DFMGR_LINK_COUNT];

static const DfMgrLinkOps s_dfmgrLinkTable[DFMGR_LINK_COUNT] = {
	{ sdioInit, sdioTerm, 0 },
	{ usbInit, usbTerm, 1 },
};

/*
 * The progress callbacks hand the report to link 0's DCMP. Names and
 * parameters after the 0.990 _installEvent* and _updateEvent* records.
 */
static int _saveDataFormatEventFinished(SceUInt32 result, const char *description, int size)
{
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSetStatusSaveDataFormatFinished(0, result, description, size);
	return 0;
}

static int _saveDataFormatEventOngoing(SceInt32 phase, SceUInt32 progress)
{
	if (phase > 0xff || progress - 1 > 99)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusSaveDataFormatProgressOngoing(0, phase, progress);
	return 0;
}

static int _saveDataFormatEventStarted(SceInt32 phase, SceUInt32 total, const char *description, int size)
{
	SceUInt32 flag;

	if (phase < 0) {
		phase &= 0x7fffffff;
		flag = 1;
	} else {
		flag = 0;
	}
	if ((SceUInt32)phase > 0xff)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusSaveDataFormatProgressStarted(0, phase, total, description, size, flag);
	return 0;
}

static int _saveDataInstallEventFinished(SceUInt32 result, const char *description, int size)
{
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSetStatusSaveDataInstallFinished(0, result, description, size);
	return 0;
}

static int _saveDataInstallEventOngoing(SceInt32 phase, SceUInt32 progress)
{
	if (phase > 0xff || progress - 1 > 99)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusSaveDataInstallProgressOngoing(0, phase, progress);
	return 0;
}

static int _saveDataInstallEventStarted(SceInt32 phase, SceUInt32 total, const char *description, int size)
{
	SceUInt32 flag;

	if (phase < 0) {
		phase &= 0x7fffffff;
		flag = 1;
	} else {
		flag = 0;
	}
	if ((SceUInt32)phase > 0xff)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusSaveDataInstallProgressStarted(0, phase, total, description, size, flag);
	return 0;
}

static int _uninstallEventFinished(SceUInt32 result, const char *description, int size)
{
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSetStatusUninstallFinished(0, result, description, size);
	return 0;
}

static int _uninstallEventOngoing(SceInt32 phase, SceUInt32 progress)
{
	if (phase > 0xff || progress - 1 > 99)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusUninstallProgressOngoing(0, phase, progress);
	return 0;
}

static int _uninstallEventStarted(SceInt32 phase, SceUInt32 total, const char *description, int size)
{
	SceUInt32 flag;

	if (phase < 0) {
		phase &= 0x7fffffff;
		flag = 1;
	} else {
		flag = 0;
	}
	if ((SceUInt32)phase > 0xff)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusUninstallProgressStarted(0, phase, total, description, size, flag);
	return 0;
}

static int _installEventFinished(SceUInt32 result, const char *description, int size)
{
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSetStatusInstallFinished(0, result, description, size);
	return 0;
}

static int _installEventOngoing(SceInt32 phase, SceUInt32 progress)
{
	if (phase > 0xff || progress - 1 > 99)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusInstallProgressOngoing(0, phase, progress);
	return 0;
}

static int _installEventStarted(SceInt32 phase, SceUInt32 total, const char *description, int size)
{
	SceUInt32 flag;

	if (phase < 0) {
		phase &= 0x7fffffff;
		flag = 1;
	} else {
		flag = 0;
	}
	if ((SceUInt32)phase > 0xff)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusInstallProgressStarted(0, phase, total, description, size, flag);
	return 0;
}

static int _updateEventFinished(SceUInt32 result, const char *description, int size)
{
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusUpdateFinished(0, result, description, size);
	sceKernelDelayThread(3000000);
	return 0;
}

static int _updateEventOngoing(SceInt32 phase, SceUInt32 progress)
{
	if (phase > 0xff || progress - 1 > 99)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusUpdateProgressOngoing(0, phase, progress);
	return 0;
}

static int _updateEventStarted(SceInt32 phase, SceUInt32 total, const char *description, int size)
{
	SceUInt32 flag;

	if (phase < 0) {
		phase &= 0x7fffffff;
		flag = 1;
	} else {
		flag = 0;
	}
	if ((SceUInt32)phase > 0xff)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (size > DFMGR_PROGRESS_MSG_MAX)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	if (description == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	dcmpSendStatusUpdateProgressStarted(0, phase, total, description, size, flag);
	return 0;
}

static const SysrootDfmgrOps s_dfmgrSysrootCallbacks = {
	.size = sizeof(SysrootDfmgrOps),
	.updateProgressStarted = _updateEventStarted,
	.updateProgressOngoing = _updateEventOngoing,
	.updateFinished = _updateEventFinished,
	.installProgressStarted = _installEventStarted,
	.installProgressOngoing = _installEventOngoing,
	.installFinished = _installEventFinished,
	.uninstallProgressStarted = _uninstallEventStarted,
	.uninstallProgressOngoing = _uninstallEventOngoing,
	.uninstallFinished = _uninstallEventFinished,
	.saveDataInstallProgressStarted = _saveDataInstallEventStarted,
	.saveDataInstallProgressOngoing = _saveDataInstallEventOngoing,
	.saveDataInstallFinished = _saveDataInstallEventFinished,
	.saveDataFormatProgressStarted = _saveDataFormatEventStarted,
	.saveDataFormatProgressOngoing = _saveDataFormatEventOngoing,
	.saveDataFormatFinished = _saveDataFormatEventFinished,
};

static inline void dfmgrSetFlags(SceUInt32 mask)
{
	SceKernelIntrStatus s;

	s = SceCpuForDriver_D32ACE9E(&s_dfmgrLock);
	s_dfmgrFlags |= mask;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
}

static inline void dfmgrClearFlags(SceUInt32 mask)
{
	SceKernelIntrStatus s;

	s = SceCpuForDriver_D32ACE9E(&s_dfmgrLock);
	s_dfmgrFlags &= ~mask;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
}

/* SceSysrootForDriver_C5EAF5F7: 0 power off, 1 power button, 2 low battery */
static void _sysstatemgrEvent(int event)
{
	int percent = s_dfmgrPowerEvents + 50;

	switch (event) {
	case 0:
		dcmpSendStatusPower(0, 0, 1, 0);
		dfmgrSetFlags(DFMGR_POWER_OFF);
		break;
	case 1:
		if (percent < 100) {
			dcmpSendStatusPower(0, 0, 2, percent);
			s_dfmgrPowerEvents++;
		}
		break;
	case 2:
		dcmpSendStatusPower(0, 0, 3, 0);
		break;
	default:
		SCE_PRINTF_WITH_INFO(15, 1943, "%d\n", event);
		break;
	}
}

static inline DfMgrProtocolCB *_getPdCB(SceUInt32 id)
{
	if (id < DFMGR_PROTOCOL_COUNT)
		return &s_dfmgrPd[id];
	SCE_PRINTF_WITH_INFO(15, 2468, "%d\n", id);
	SCE_DBG_ASSERT_FAIL(2469);
	return NULL;
}

static __attribute__((always_inline)) inline int
_callProtocolHandler(DfMgrProtocolCB *pd, int event, void *arg)
{
	int ret;

	if (pd->link == NULL) {
		SCE_PRINTF_WITH_INFO(15, 2330, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelWaitSema(pd->link->eventSema, 1, NULL);
	if (ret < 0)
		return ret;
	ret = pd->handler(event, arg, pd->arg);
	sceKernelSignalSema(pd->link->eventSema, 1);
	return ret;
}

int dfmgrSetProtocolStatus(DfMgrProtocolCB *pd, SceUInt32 mask, int on)
{
	SceKernelIntrStatus s;

	s = SceCpuForDriver_D32ACE9E(&pd->lock);
	if (on) {
		pd->flags |= mask;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pd->lock, s);
	} else {
		pd->flags &= ~mask;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&pd->lock, s);
	}
	return 0;
}

/* the link delivered a packet header: route the packet to its protocol */
static void recvCallback(DfMgrLinkCB *link)
{
	SceDeci4pHeader hdr;
	SceUInt8 discard[16];
	DfMgrProtocolCB *pd;
	SceUInt16 sizeLow;
	SceUInt32 packetSize, protocol;
	SceSize n;
	int ret;

	if (link->recv(&hdr, sizeof(hdr)) < 0)
		return;
	memcpy(&sizeLow, hdr.size, sizeof(sizeLow));
	packetSize = sizeLow | hdr.size[2] << 16;
	protocol = hdr.protocol;
	if (hdr.version != SCE_DECI4P_VERSION) {
		SCE_PRINTF_WITH_INFO(0, 1478, "[DECI] unknown packet version: 0x%02x\n",
				     hdr.version);
		sceKernelDelayThread(10000);
		return;
	}
	if (link->recvRemaining > DFMGR_PACKET_MAX) {
		SCE_PRINTF_WITH_INFO(0, 1490, "[DECI] illegal packet size: %d\n",
				     link->recvRemaining);
		sceKernelDelayThread(10000);
		return;
	}
	link->header = hdr;
	link->recvRemaining = packetSize;

	if (protocol == DFMGR_PROTOCOL_DCMP) {
		pd = _getPdCB(s_dfmgrLinkTable[link->info->id].pd);
		if (pd == NULL)
			goto unknown;
	} else {
		for (pd = s_dfmgrPd; pd != &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]; pd++)
			if ((pd->flags & DFMGR_PD_OPEN) && pd->info.protocol == protocol)
				break;
		if (pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT])
			goto unknown;
	}

	dfmgrSetProtocolStatus(pd, DFMGR_PD_RECEIVING, 1);
	for (;;) {
		n = link->recvRemaining;
		if (n == 0) {
			dfmgrSetProtocolStatus(pd, DFMGR_PD_RECEIVED, 1);
			_callProtocolHandler(pd, SCE_DFMGR_EVENT_REQ_DONE, NULL);
			goto done;
		}
		ret = _callProtocolHandler(pd, SCE_DFMGR_EVENT_RECV, (void *)n);
		if (ret < 0)
			break;
	}
	if (ret == DFMGR_ERROR_QUEUE_FULL) {
		dfmgrSetProtocolStatus(pd, DFMGR_PD_SPACE_ERROR | DFMGR_PD_SPACE_BUSY, 1);
		dcmpSendError(pd, link->info->id, 3, &hdr);
		dfmgrSetProtocolStatus(pd, DFMGR_PD_SPACE_BUSY, 0);
		dfmgrSetProtocolStatus(pd, DFMGR_PD_SPACE_DONE, 1);
		goto rejected;
	}
	if (link->flags & DFMGR_LINK_RECV_ERROR) {
		dfmgrSetLinkStatus(link, DFMGR_LINK_RECV_ERROR, 0);
		link->recvRemaining = 0;
		goto done;
	}
	goto drain;

unknown:
	pd = _getPdCB(s_dfmgrLinkTable[link->info->id].pd);
	dcmpSendError(pd, link->info->id, 2, &hdr);
rejected:
	dfmgrSetProtocolStatus(pd, DFMGR_PD_RECEIVING, 0);
	_callProtocolHandler(pd, SCE_DFMGR_EVENT_REQ_DONE, NULL);
drain:
	while ((n = link->recvRemaining) != 0) {
		if (n > sizeof(discard))
			n = sizeof(discard);
		ret = link->recv(discard, n);
		if (ret < 0)
			break;
		link->recvRemaining -= ret;
	}
done:
	dfmgrSetProtocolStatus(pd, DFMGR_PD_RECEIVING | DFMGR_PD_RECEIVED, 0);
	dfmgrSetProtocolStatus(pd, DFMGR_PD_HEADER_PENDING, 1);
}

int sceDfMgrInit(void)
{
	DfMgrProtocolCB *pd;
	DfMgrLinkCB *link;
	int i, ret;

	s_dfmgrFlags = 0;
	s_dfmgrLock = 0;
	s_dfmgrResumeRetries = 0;
	memset(s_dfmgrLink, 0, sizeof(s_dfmgrLink));
	memset(s_dfmgrPd, 0, sizeof(s_dfmgrPd));
	s_dfmgrNextHandle = 0x1000;

	for (i = 0, link = s_dfmgrLink; i < DFMGR_LINK_COUNT; i++, link++) {
		pd = _getPdCB(s_dfmgrLinkTable[i].pd);
		ret = sceKernelCreateMutex("SceDeci4pDfMgrSend", SCE_KERNEL_ATTR_TH_PRIO, 0,
					    NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 228);
			return ret;
		}
		link->sendMutex = ret;
		ret = sceKernelCreateMutex("SceDeci4pDfMgrRecv", SCE_KERNEL_ATTR_TH_PRIO, 0,
					    NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 238);
			return ret;
		}
		link->recvMutex = ret;
		ret = sceKernelCreateSema("SceDeci4pDfMgrEvent", SCE_KERNEL_ATTR_TH_PRIO, 2, 2,
					   NULL);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 251);
			return ret;
		}
		link->eventSema = ret;
		link->reqLock = 0;
		ret = dcmpInit(&s_dfmgrPd[i], i, link);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 262, "%08X\n", ret);
			SCE_DBG_ASSERT_LEVEL(0, 0, 263);
			return ret;
		}
		dfmgrSetProtocolStatus(pd, DFMGR_PD_HEADER_PENDING, 1);
		ret = s_dfmgrLinkTable[i].init(link, sendCallback, recvCallback, &link->sendEvf);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(15, 278, "%08X\n", ret);
			SCE_DBG_ASSERT_LEVEL(0, 0, 279);
			return ret;
		}
		if (i == 0) {
			dfmgrSetLinkStatus(link, DFMGR_LINK_ATTACHED, 1);
			if (SceModulemgrForDriver_BBE1771C("SceDbgUsb") < 0)
				break;
		}
	}

	sceKernelRegisterSysEventHandler("SceDeci4pDfMgr", _suspendHandlerQuery, NULL);
	SceSysrootForDriver_C5EAF5F7(_sysstatemgrEvent);
	SceSysrootForDriver_579CC758(&s_dfmgrSysrootCallbacks);
	dfmgrSetFlags(DFMGR_INITIALIZED);
	dcmpSendStatusPower(0, 0xf, 1, 0);
	return 0;
}

int sceDfMgrModuleStart(SceSize args, void *argp)
{
	if (SceDipswForDriver_A98FC2FD(0xc2) == 0) {
		SCE_DBG_PRINTF(0xb, 164, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	if (sceDfMgrInit() < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}

int sceDfMgrOpen(SceDfMgrHandler handler, const SceDfMgrProtocol *protocol, void *arg)
{
	DcmpStatus status;
	DfMgrProtocolCB *pd;
	SceKernelIntrStatus s;
	SceUInt32 linkIndex, st, aliasSt;
	SceUInt16 flags;
	SceUID handle;
	int ret;

	if (protocol == NULL) {
		SCE_PRINTF_WITH_INFO(15, 742, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	if (protocol->protocol == 0) {
		SCE_PRINTF_WITH_INFO(15, 746, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	if (protocol->size != sizeof(SceDfMgrProtocol)) {
		SCE_PRINTF_WITH_INFO(15, 750, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	if (strnlen(protocol->name, sizeof(protocol->name)) >= sizeof(protocol->name)) {
		SCE_PRINTF_WITH_INFO(15, 754, "\n");
		return DFMGR_ERROR_INVALID_ARGUMENT;
	}

	flags = protocol->flags;
	s = SceCpuForDriver_D32ACE9E(&s_dfmgrLock);
	for (pd = s_dfmgrPd; pd != &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]; pd++) {
		if ((pd->flags & DFMGR_PD_OPEN) && pd->info.protocol == protocol->protocol) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
			return DFMGR_ERROR_ALREADY_OPENED;
		}
	}
	for (pd = &s_dfmgrPd[DFMGR_LINK_COUNT]; pd != &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]; pd++)
		if (!(pd->flags & DFMGR_PD_OPEN))
			break;
	if (pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
		SCE_PRINTF_WITH_INFO(15, 826, "\n");
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
		return DFMGR_ERROR_NO_SLOT;
	}

	memset(pd, 0, sizeof(*pd));
	pd->handler = handler;
	pd->info = *protocol;
	handle = s_dfmgrNextHandle++;
	pd->arg = arg;
	linkIndex = 0;
	if (s_dfmgrLink[1].flags & DFMGR_LINK_READY)
		linkIndex = (pd->info.flags & DFMGR_PROTOCOL_USB) ? 1 : 0;
	pd->handle = handle;
	pd->linkIndex = linkIndex;
	pd->link = &s_dfmgrLink[linkIndex];
	if ((s_dfmgrLink[linkIndex].flags & DFMGR_LINK_CONNECTED) && (flags & DFMGR_PROTOCOL_ENABLE)) {
		pd->flags = DFMGR_PD_HEADER_PENDING | DFMGR_PD_STARTED | DFMGR_PD_OPEN;
		aliasSt = 7;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
		st = 3;
	} else {
		pd->flags = DFMGR_PD_HEADER_PENDING | DFMGR_PD_OPEN;
		aliasSt = 5;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
		st = 1;
	}
	if (protocol->flags & DFMGR_PROTOCOL_ALIAS)
		st = aliasSt;
	dcmpSendStatusProtocol(pd->linkIndex, st, protocol);
	if (protocol->flags & DFMGR_PROTOCOL_ALIAS)
		dcmpSendStatusProtocolAlias(pd->linkIndex, protocol);

	if (pd->link->flags & DFMGR_LINK_CONNECTED) {
		if (protocol->flags & DFMGR_PROTOCOL_ENABLE)
			return handle;
		dcmpMakeStatusConnection(1, &status.connection);
		ret = _callProtocolHandler(pd, SCE_DFMGR_EVENT_STATUS, &status);
		if (ret < 0)
			SCE_PRINTF_WITH_INFO(15, 894, "%08X\n", ret);
		return handle;
	}
	dcmpMakeStatusConnection(0, &status.connection);
	ret = _callProtocolHandler(pd, SCE_DFMGR_EVENT_STATUS, &status);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 870, "%08X\n", ret);
	if (!(protocol->flags & 1))
		return handle;
	dcmpMakeStatusProtocol(0, &pd->info, &status);
	ret = _callProtocolHandler(pd, SCE_DFMGR_EVENT_STATUS, &status);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(15, 882, "%08X\n", ret);
	return handle;
}

int sceDfMgrClose(SceUID handle)
{
	SceDfMgrProtocol info;
	DfMgrProtocolCB *pd;
	DfMgrLinkCB *link;
	SceKernelIntrStatus s;
	SceUInt32 linkIndex;
	int ret;

	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 921, "%08X\n", handle);
			return DFMGR_ERROR_INVALID_ARGUMENT;
		}
	}
	ret = sceKernelWaitSema(pd->link->eventSema, 2, NULL);
	if (ret < 0)
		return ret;
	s = SceCpuForDriver_D32ACE9E(&s_dfmgrLock);
	info = pd->info;
	linkIndex = pd->linkIndex;
	link = pd->link;
	memset(pd, 0, sizeof(*pd));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_dfmgrLock, s);
	sceKernelSignalSema(link->eventSema, 2);
	dcmpSendStatusProtocol(linkIndex, 0, &info);
	return 0;
}

/* DCMP and the TTY / file protocols may send before the protocol is started */
static inline int dfmgrIsAlwaysOpen(SceUInt32 protocol)
{
	return (protocol & ~0x1000) == 0x2000 || protocol == DFMGR_PROTOCOL_DCMP ||
	       protocol == 0x4000;
}

int sceDfMgrReqSend(SceUID handle)
{
	DfMgrProtocolCB *pd;
	DfMgrLinkCB *link;
	SceKernelIntrStatus s, rs;
	int ret;

	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 966, "%08X\n", handle);
			return DFMGR_ERROR_INVALID_ARGUMENT;
		}
	}
	link = pd->link;
	s = SceCpuForDriver_D32ACE9E(&link->lock);
	if (pd->flags & DFMGR_PD_SEND_REQUESTED) {
		ret = DFMGR_ERROR_BUSY;
	} else {
		if (!dfmgrIsAlwaysOpen(pd->info.protocol) &&
		    (!(link->flags & DFMGR_LINK_CONNECTED) || !(pd->flags & DFMGR_PD_STARTED))) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&link->lock, s);
			return DFMGR_ERROR_NOT_CONNECTED;
		}
		dfmgrSetProtocolStatus(pd, DFMGR_PD_SEND_REQUESTED, 1);
		handle = pd->handle;
		rs = SceCpuForDriver_D32ACE9E(&link->reqLock);
		if (link->reqCount > DFMGR_REQ_QUEUE - 1) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&link->reqLock, rs);
			ret = DFMGR_ERROR_QUEUE_FULL;
		} else {
			int write = link->reqWrite;
			int next = write + 1;

			link->reqCount++;
			if (next > DFMGR_REQ_QUEUE - 1)
				next = 0;
			link->req[write] = handle;
			link->reqWrite = next;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&link->reqLock, rs);
			ret = sceKernelSetEventFlag(link->sendEvf, 1);
			if (ret < 0)
				SCE_DBG_ASSERT(0, 1032);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&link->lock, s);
	return ret;
}

/* fills in the header of the caller's packet, though deci4p_dfmgr/deci4p.h says const */
int sceDfMgrSend(SceUID handle, const void *data, SceSize size)
{
	SceDeci4pHeader *buf = (SceDeci4pHeader *)data;
	DfMgrProtocolCB *pd;
	DfMgrLinkCB *link;
	SceSize remaining;
	int ret;

	if (buf == NULL) {
		SCE_PRINTF_WITH_INFO(15, 1053, "\n");
		return DFMGR_ERROR_NULL_BUFFER;
	}
	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 1060, "%08X\n", handle);
			return DFMGR_ERROR_INVALID_ARGUMENT;
		}
	}
	link = pd->link;
	if (!(pd->flags & DFMGR_PD_SEND_REQUESTED) && !(pd->flags & DFMGR_PD_SENDING))
		return 0;
	if (!dfmgrIsAlwaysOpen(pd->info.protocol) &&
	    (!(link->flags & DFMGR_LINK_READY) || !(link->flags & DFMGR_LINK_ATTACHED) ||
	     !(link->flags & DFMGR_LINK_CONNECTED) || !(pd->flags & DFMGR_PD_STARTED)))
		return DFMGR_ERROR_NOT_CONNECTED;
	ret = sceKernelLockMutex(link->sendMutex, 1, NULL);
	if (ret < 0)
		return ret;

	if (!(pd->flags & DFMGR_PD_HEADER_SENT)) {
		SceUInt16 sizeLow;
		SceUInt32 protocol, packetProtocol, id;

		memcpy(&sizeLow, buf->size, sizeof(sizeLow));
		link->sendRemaining = sizeLow | buf->size[2] << 16;
		buf->version = SCE_DECI4P_VERSION;
		protocol = pd->info.protocol;
		id = link->info->id;
		buf->reserved0 = 0;
		buf->reserved1 = 0;
		packetProtocol = pd->info.protocol;
		buf->reserved2 = 0;
		buf->src = id == 1 ? 0xa100 : 0xa000;
		buf->protocol = protocol;
		if (packetProtocol != DFMGR_PROTOCOL_DCMP)
			buf->dst = pd->info.destination;
		if (size <= link->sendRemaining) {
			dfmgrSetProtocolStatus(pd, DFMGR_PD_HEADER_SENT, 1);
			goto send;
		}
		ret = DFMGR_ERROR_INVALID_ARGUMENT;
		SCE_PRINTF_WITH_INFO(15, 1141, "%d:%d\n", link->sendRemaining, size);
		link->sendRemaining = 0;
	} else {
		remaining = link->sendRemaining;
		ret = DFMGR_ERROR_NOTHING_TO_SEND;
		if (remaining != 0) {
			if (size <= remaining)
				goto send;
			SCE_PRINTF_WITH_INFO(15, 1165, "%d:%d\n", remaining, size);
			ret = DFMGR_ERROR_INVALID_ARGUMENT;
		}
	}
	sceKernelUnlockMutex(link->sendMutex, 1);
	return ret;

send:
	ret = link->send(buf, size);
	if (ret >= 0)
		link->sendRemaining -= size;
	sceKernelUnlockMutex(link->sendMutex, 1);
	return ret;
}

/* the link may send: let the next requesting protocol send its packet */
static void sendCallback(DfMgrLinkCB *link)
{
	DfMgrProtocolCB *pd;
	SceKernelIntrStatus s;
	SceUInt32 pad;
	SceSize n;
	SceUID handle;
	int ret, read, next;

	link->sendRemaining = 0;
	s = SceCpuForDriver_D32ACE9E(&link->reqLock);
	if (link->reqCount <= 0) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&link->reqLock, s);
		return;
	}
	read = link->reqRead;
	link->reqCount--;
	next = read + 1;
	if (next > DFMGR_REQ_QUEUE - 1)
		next = 0;
	handle = link->req[read];
	link->reqRead = next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&link->reqLock, s);
	if (handle < 0)
		return;
	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 1356, "%08X\n", handle);
			return;
		}
	}
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SENDING, 1);
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SEND_REQUESTED, 0);
	do {
		ret = _callProtocolHandler(pd, SCE_DFMGR_EVENT_SEND, NULL);
		if (ret < 0)
			goto error;
	} while (link->sendRemaining != 0);

	dfmgrSetProtocolStatus(pd, DFMGR_PD_SENDING, 0);
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SENT, 1);
	_callProtocolHandler(pd, SCE_DFMGR_EVENT_SENT, NULL);
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SENT | DFMGR_PD_HEADER_SENT, 0);
	if (link->reqCount > 0)
		sceKernelSetEventFlag(link->sendEvf, 1);
	return;

error:
	if (ret == DFMGR_ERROR_NOTHING_TO_SEND) {
		_callProtocolHandler(pd, SCE_DFMGR_EVENT_SENT, NULL);
	} else {
		/* pad the packet the protocol failed to complete */
		pad = 0xdeadbeef;
		while ((n = link->sendRemaining) != 0) {
			if (n > sizeof(pad))
				n = sizeof(pad);
			sceDfMgrSend(pd->handle, &pad, n);
		}
	}
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SENT | DFMGR_PD_HEADER_SENT | DFMGR_PD_SENDING, 0);
}

int sceDfMgrRecv(SceUID handle, void *buf, SceSize size)
{
	DfMgrProtocolCB *pd;
	DfMgrLinkCB *link;
	SceSize n, rest;
	int ret;

	if (buf == NULL) {
		SCE_PRINTF_WITH_INFO(15, 1201, "\n");
		return DFMGR_ERROR_NULL_BUFFER;
	}
	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 1208, "%08X\n", handle);
			return DFMGR_ERROR_INVALID_ARGUMENT;
		}
	}
	link = pd->link;
	ret = sceKernelLockMutex(link->recvMutex, 1, NULL);
	if (ret < 0)
		return ret;
	n = size < link->recvRemaining ? size : link->recvRemaining;
	rest = n;
	if (pd->flags & DFMGR_PD_HEADER_PENDING) {
		if (n < sizeof(SceDeci4pHeader)) {
			ret = DFMGR_ERROR_BUFFER_TOO_SMALL;
			goto out;
		}
		*(SceDeci4pHeader *)buf = link->header;
		dfmgrSetProtocolStatus(pd, DFMGR_PD_HEADER_PENDING, 0);
		buf = (SceDeci4pHeader *)buf + 1;
		rest = n - sizeof(SceDeci4pHeader);
	}
	ret = link->recv(buf, rest);
	if (ret >= 0) {
		link->recvRemaining -= n;
		sceKernelUnlockMutex(link->recvMutex, 1);
		return n;
	}
	dfmgrSetLinkStatus(link, DFMGR_LINK_RECV_ERROR, 1);
out:
	sceKernelUnlockMutex(link->recvMutex, 1);
	return ret;
}

SceSize SceDeci4pDfMgrForDebugger_CACAB5F9(const SceDeci4pHeader *hdr)
{
	return deci4pGetSize(hdr);
}

void SceDeci4pDfMgrForDebugger_6D26CC56(SceDeci4pHeader *hdr, SceSize size)
{
	deci4pSetSize(hdr, size);
}

int sceDfMgrSendSpace(SceUID handle)
{
	DfMgrProtocolCB *pd;
	int i;

	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->handle != handle) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]) {
			SCE_PRINTF_WITH_INFO(15, 1303, "%08X\n", handle);
			return DFMGR_ERROR_NOT_FOUND;
		}
	}
	for (i = 100; i != 0; i--) {
		if (pd->flags & DFMGR_PD_SPACE_DONE)
			break;
		sceKernelDelayThread(100);
	}
	dcmpSendStatusSpace(pd->linkIndex, pd->info.protocol);
	dfmgrSetProtocolStatus(pd, DFMGR_PD_SPACE_ERROR | DFMGR_PD_SPACE_BUSY | DFMGR_PD_SPACE_DONE,
			       0);
	return 0;
}

/* name unknown */
SceUInt32 dfmgrTestLinkStatus(DfMgrLinkCB *link, SceUInt32 mask)
{
	return link->flags & mask;
}

/* name unknown */
int dfmgrSetLinkStatus(DfMgrLinkCB *link, SceUInt32 mask, int on)
{
	SceKernelIntrStatus s;

	s = SceCpuForDriver_D32ACE9E(&link->lock);
	if (on) {
		link->flags |= mask;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&link->lock, s);
	} else {
		link->flags &= ~mask;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&link->lock, s);
	}
	return 0;
}

/* tell the protocols on the link that it is up / down */
static inline void dfmgrNotifyConnection(DfMgrLinkCB *link, SceUInt32 connected)
{
	DcmpStatusConnection status;
	DfMgrProtocolCB *pd;

	dcmpMakeStatusConnection(connected, &status);
	for (pd = &s_dfmgrPd[DFMGR_LINK_COUNT]; pd != &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]; pd++) {
		if ((pd->flags & DFMGR_PD_OPEN) && pd->link == link &&
		    sceKernelWaitSema(link->eventSema, 1, NULL) >= 0) {
			pd->handler(SCE_DFMGR_EVENT_STATUS, &status, pd->arg);
			sceKernelSignalSema(pd->link->eventSema, 1);
		}
	}
}

/* name unknown */
void dfmgrDisconnectLink(DfMgrLinkCB *link, int notify)
{
	if (!(link->flags & DFMGR_LINK_CONNECTED))
		return;
	dfmgrSetLinkStatus(link, DFMGR_LINK_CONNECTED, 0);
	if (notify)
		dfmgrNotifyConnection(link, 0);
}

int dfmgrStartProtocol(SceUInt32 protocol)
{
	DcmpStatus status;
	DfMgrProtocolCB *pd;
	int ret;

	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->info.protocol != protocol) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT])
			return DFMGR_ERROR_UNKNOWN_PROTOCOL;
	}
	dfmgrSetProtocolStatus(pd, DFMGR_PD_STARTED, 1);
	ret = dcmpMakeStatusProtocol(1, &pd->info, &status);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 1825, "%08X\n", ret);
		return ret;
	}
	_callProtocolHandler(pd, SCE_DFMGR_EVENT_STATUS, &status);
	return 0;
}

int dfmgrStopProtocol(SceUInt32 protocol)
{
	DcmpStatus status;
	DfMgrProtocolCB *pd;
	int ret;

	pd = s_dfmgrPd;
	while (!(pd->flags & DFMGR_PD_OPEN) || pd->info.protocol != protocol) {
		if (++pd == &s_dfmgrPd[DFMGR_PROTOCOL_COUNT])
			return DFMGR_ERROR_INVALID_ARGUMENT;
	}
	dfmgrSetProtocolStatus(pd, DFMGR_PD_STARTED, 0);
	ret = dcmpMakeStatusProtocol(0, &pd->info, &status);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(15, 1861, "%08X\n", ret);
		return ret;
	}
	_callProtocolHandler(pd, SCE_DFMGR_EVENT_STATUS, &status);
	return 0;
}

/* name unknown: report the protocols of a link that came up */
#if FW_SINCE(0x04000000)
static void dfmgrSendStatusProtocols(SceUInt32 linkIndex) __attribute__((noinline));
#endif
static void dfmgrSendStatusProtocols(SceUInt32 linkIndex)
{
	DfMgrProtocolCB *pd;
	SceUInt32 st, aliasSt;

	for (pd = &s_dfmgrPd[DFMGR_LINK_COUNT]; pd != &s_dfmgrPd[DFMGR_PROTOCOL_COUNT]; pd++) {
		if (pd->linkIndex != linkIndex || !(pd->flags & DFMGR_PD_OPEN))
			continue;
		if (pd->flags & DFMGR_PD_STARTED) {
			aliasSt = 7;
			st = 3;
		} else {
			aliasSt = 5;
			st = 1;
		}
		if (pd->info.flags & 2)
			st = aliasSt;
		dcmpSendStatusProtocol(linkIndex, st, &pd->info);
		if (pd->info.flags & 2)
			dcmpSendStatusProtocolAlias(pd->linkIndex, &pd->info);
	}
}

void dfmgrConnectLink(DfMgrLinkCB *link, int notify)
{
	if (link->flags & DFMGR_LINK_CONNECTED)
		return;
	if (link->info->id == 1)
		SCE_PRINTF_WITH_INFO(0, 1739, "[DECI] PS Vita DevKit (target) USB: connected\n");
	dfmgrSetLinkStatus(link, DFMGR_LINK_CONNECTED, 1);
	dfmgrSendStatusProtocols(link->info->id);
	if (notify)
		dfmgrNotifyConnection(link, 1);
}

static inline int _resumeHandlerEnd(void)
{
	DfMgrLinkCB *link;

	if (!(s_dfmgrFlags & DFMGR_SUSPENDING))
		return 0;
	if (SceSysrootForDriver_55392965() != 0)
		dfmgrConnectLink(&s_dfmgrLink[0], 1);
	if (s_dfmgrResumeRetries++ < 100) {
		link = &s_dfmgrLink[0];
		if (link->flags & DFMGR_LINK_SUSPENDED) {
			if (!(link->flags & DFMGR_LINK_CONNECTED))
				goto wait;
			if (!dcmpIsSendQueueEmpty(0))
				return SCE_ERROR_ERRNO_EBUSY;
			dfmgrSetLinkStatus(link, DFMGR_LINK_SUSPENDED, 0);
		}
		link = &s_dfmgrLink[1];
		if (link->flags & DFMGR_LINK_SUSPENDED) {
			if (!(link->flags & DFMGR_LINK_CONNECTED))
				goto wait;
			if (!dcmpIsSendQueueEmpty(1))
				return SCE_ERROR_ERRNO_EBUSY;
			dfmgrSetLinkStatus(link, DFMGR_LINK_SUSPENDED, 0);
		}
	} else {
		SCE_PRINTF_WITH_INFO(0, 661, "[DECI] PS Vita DevKit (target) USB: "
				     "continue to resume without reconnection\n");
	}
	dfmgrClearFlags(DFMGR_SUSPENDING);
	return 0;

wait:
	sceKernelDelayThread(10000);
	return SCE_ERROR_ERRNO_EBUSY;
}

static int _suspendHandlerQuery(int resume, int eventid, void *args, void *opt)
{
	const struct PowerSuspendEvent *suspend = args;
	SceUInt8 code;
	int sleepMode;

	if (resume == 0) {
		switch (eventid) {
		case SYSEVENT_SUSPEND_PREPARE(0):
			sleepMode = suspend->sleepMode;
			if (s_dfmgrLink[0].flags & DFMGR_LINK_CONNECTED)
				dfmgrSetLinkStatus(&s_dfmgrLink[0], DFMGR_LINK_SUSPENDED, 1);
			else
				dfmgrSetLinkStatus(&s_dfmgrLink[0], DFMGR_LINK_SUSPENDED, 0);
			if (s_dfmgrLink[1].flags & DFMGR_LINK_CONNECTED)
				dfmgrSetLinkStatus(&s_dfmgrLink[1], DFMGR_LINK_SUSPENDED, 1);
			else
				dfmgrSetLinkStatus(&s_dfmgrLink[1], DFMGR_LINK_SUSPENDED, 0);
			dcmpSendStatusPower(0, sleepMode == POWER_SLEEP_SUSPEND ? 0x81 : 0x80, 0, 0);
			return 0;
		case SYSEVENT_SUSPEND_QUERY:
			sleepMode = suspend->sleepMode;
			if (SceSysrootForDriver_55392965() != 0)
				return 0;
			if (SceProcessmgrForDriver_C715591F() == 0)
				return 0;
			SCE_PRINTF_WITH_INFO(0, 466, "[DECI] reject suspending system due to a "
					     "busy process\n");
			dcmpSendStatusPower(0, sleepMode == POWER_SLEEP_SUSPEND ? 0x81 : 0x80, 3, 1);
			return SCE_ERROR_ERRNO_EBUSY;
		case SYSEVENT_SUSPEND_IDLE:
			sleepMode = suspend->sleepMode;
			if (s_dfmgrFlags & DFMGR_SUSPENDING)
				return 0;
			code = sleepMode == POWER_SLEEP_SUSPEND ? 0x8e : 0x8f;
			dfmgrSetFlags(DFMGR_SUSPENDING);
			if (SceDipswForDriver_A98FC2FD(0xc3) == 0)
				dcmpSendStatusPower(1, code, 0, 0);
			dcmpSendStatusPower(0, code, 0, 0);
			return 0;
		case SYSEVENT_SUSPEND_IDLE_QUERY:
			if (s_dfmgrFlags & DFMGR_SUSPENDED)
				return 0;
			if (SceSysrootForDriver_55392965() == 0) {
				if (s_dfmgrLink[0].flags & DFMGR_LINK_CONNECTED)
					return SCE_ERROR_ERRNO_EBUSY;
			} else if (s_dfmgrLink[0].flags & DFMGR_LINK_CONNECTED) {
				dfmgrSetLinkStatus(&s_dfmgrLink[0], DFMGR_LINK_CONNECTED, 0);
			}
			if (s_dfmgrLink[1].flags & DFMGR_LINK_CONNECTED)
				dfmgrSetLinkStatus(&s_dfmgrLink[1], DFMGR_LINK_CONNECTED, 0);
			sceKernelDisableIntr(0xb8);
			dfmgrSetFlags(DFMGR_SUSPENDED);
			return 0;
		default:
			return 0;
		}
	}
	if (resume != 1)
		return 0;
	switch (eventid) {
	case SYSEVENT_RESUME_IDLE_QUERY:
		return _resumeHandlerEnd();
	case SYSEVENT_RESUME_COMPLETE:
		dcmpSendStatusPower(0, 1, 1, 0);
		return 0;
	case SYSEVENT_RESUME_START:
		if (!(s_dfmgrFlags & DFMGR_SUSPENDED))
			return 0;
		dfmgrClearFlags(DFMGR_SUSPENDED);
		sceKernelEnableIntr(0xb8);
		dcmpSendStatusPower(0, 0xe, 1, 0);
		s_dfmgrResumeRetries = 0;
		return 0;
	default:
		return 0;
	}
}
