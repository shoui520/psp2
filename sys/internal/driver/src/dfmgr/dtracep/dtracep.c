/*
 * dtracep.c: DTrace's DECI4P protocols. Each of the four channels has its
 * own protocol, DTRACEP0 to DTRACEP3: the DECI4P manager's handler queues
 * what the host sends and wakes the channel's thread, which runs the
 * commands one at a time under the DTrace mutex. Replies wait in the send
 * queue until the link takes them.
 */
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_cyclic.h>
#include <sysmem/debug.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include "dtracep.h"
#include "dtracep_command.h"
#include "global_timer.h"

static SceUID s_mutex = -1;

static DtracepChannel s_channels[DTRACEP_CHANNELS];
static int s_timersSuspended;

static int _dtracepThread(SceSize args, void *argp)
{
	DtracepChannel *ch = *(DtracepChannel **)argp;
	DtracepPacket *packet;
	uint32_t bits;
	int res;

	if ((uintptr_t)ch - (uintptr_t)s_channels >= sizeof(s_channels)) {
		DTRACE_LOG_ERROR_AT(454, "invalid argument to DTRACEP thread: %p", ch);
		return -1;
	}

	for (;;) {
		bits = 0;
		res = sceKernelWaitEventFlag(ch->eventFlag, DTRACEP_EVENT_RECV | DTRACEP_EVENT_EXIT,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (res < 0) {
			if (res == SCE_KERNEL_ERROR_WAIT_DELETE)
				return res;
			DTRACE_LOG_ERROR_AT(466, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
			return res;
		}
		if (bits & DTRACEP_EVENT_EXIT)
			return SCE_OK;

		packet = dtracepPopPacketQueue(ch->recvQueue);
		if (packet == NULL)
			continue;

		res = sceKernelLockMutex(s_mutex, 1, NULL);
		if (res < 0) {
			DTRACE_LOG_ERROR_AT(480, "%s - LockMutex failed (0x%08x)", __func__, res);
			return res;
		}

		switch (packet->header->command) {
		case DTRACEP_CMD_TEST:
			res = dtracepTestCmd(ch, packet);
			break;
		case DTRACEP_CMD_OPEN:
			SCE_DBG_PRINTF(0, 545, "XXX [dtracep] OPEN_CMD\n");
			res = dtracepOpenCmd(ch, packet);
			break;
		case DTRACEP_CMD_CLOSE:
			SCE_DBG_PRINTF(0, 549, "XXX [dtracep] CLOSE_CMD\n");
			res = dtracepCloseCmd(ch, packet);
			break;
		case DTRACEP_CMD_GET_BIG_DATA:
			dtracepGetBigDataCmd(ch, packet);
			goto done;
		case DTRACEP_CMD_PUT_BIG_DATA:
			dtracepPutBigDataCmd(ch, packet);
			goto done;
		case DTRACEP_CMD_PROVIDER:
			res = dtracepProviderCmd(ch, packet);
			break;
		case DTRACEP_CMD_PROBES:
			res = dtracepProbesCmd(ch, packet);
			break;
		case DTRACEP_CMD_BUF_SNAP:
			res = dtracepBufSnapCmd(ch, packet);
			break;
		case DTRACEP_CMD_PROBE_MATCH:
			res = dtracepProbeMatchCmd(ch, packet);
			break;
		case DTRACEP_CMD_ENABLE:
			res = dtracepEnableCmd(ch, packet);
			break;
		case DTRACEP_CMD_AGG_SNAP:
			res = dtracepAggSnapCmd(ch, packet);
			break;
		case DTRACEP_CMD_EPROBE:
			dtracepEProbeCmd(ch, packet);
			goto done;
		case DTRACEP_CMD_PROBE_ARG:
			res = dtracepProbeArgCmd(ch, packet);
			break;
		case DTRACEP_CMD_CONF:
			res = dtracepConfCmd(ch, packet);
			break;
		case DTRACEP_CMD_STATUS:
			res = dtracepStatusCmd(ch, packet);
			break;
		case DTRACEP_CMD_GO:
			res = dtracepGoCmd(ch, packet);
			break;
		case DTRACEP_CMD_STOP:
			res = dtracepStopCmd(ch, packet);
			break;
		case DTRACEP_CMD_AGG_DESC:
			res = dtracepAggDescCmd(ch, packet);
			break;
		case DTRACEP_CMD_FORMAT:
			res = dtracepFormatCmd(ch, packet);
			break;
		case DTRACEP_CMD_DOF_GET:
			res = dtracepDofGetCmd(ch, packet);
			break;
		case DTRACEP_CMD_REPLICATE:
			res = dtracepReplicateCmd(ch, packet);
			break;
		case DTRACEP_CMD_ALL_PROBES:
			res = dtracepAllProbesCmd(ch, packet);
			break;
		case DTRACEP_CMD_ALL_PROBE_MATCH:
			res = dtracepAllProbeMatchCmd(ch, packet);
			break;
		case DTRACEP_CMD_ALL_EPROBES:
			res = dtracepAllEProbesCmd(ch, packet);
			break;
		case DTRACEP_CMD_GET_MODULE_INFO:
			dtracepGetModuleInfoCmd(ch, packet);
			goto done;
		case DTRACEP_CMD_GET_PROCESS_INFO:
			dtracepGetProcessInfoCmd(ch, packet);
			goto done;
		default:
			DTRACE_LOG_ERROR_AT(568, "%s - invalid command (0x%08x)", __func__,
					    packet->header->command);
			dtracepSendErrorReply(ch, packet->header->command,
					      DTRACEP_ERROR_INVALID_COMMAND);
			goto done;
		}
		if (res < 0)
			DTRACE_LOG_ERROR_AT(575, "%s - failed (0x%08x)", __func__, res);
done:
		sceKernelUnlockMutex(s_mutex, 1);
		dtracepFreePacket(packet);
	}
}

static int dtracepSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	int i;

	if (resume == 0) {
		if (eventid == SYSEVENT_SUSPEND_PREPARE(0)) {
			for (i = 0; i < DTRACEP_CHANNELS; i++)
				s_channels[i].suspended = 1;
		} else if (eventid == SYSEVENT_SUSPEND_COMMIT) {
			GlobalTimerSuspend();
			sceDtracepCyclicHighSuspend();
			s_timersSuspended = 1;
		}
	} else if (resume == 1) {
		if (eventid == SYSEVENT_RESUME_INTR) {
			if (s_timersSuspended) {
				s_timersSuspended = 0;
				sceDtracepCyclicHighResume();
				GlobalTimerResume();
			}
		} else if (eventid == SYSEVENT_RESUME_IDLE_QUERY) {
			for (i = 0; i < DTRACEP_CHANNELS; i++)
				s_channels[i].suspended = 0;
		}
	}

	return SCE_OK;
}

/* read the host's packet into the channel's receive queue */
static void dtracepRecv(DtracepChannel *ch)
{
	DtracepHeader *header;
	DtracepPacket *packet;
	SceSize size;
	int res;

	packet = dtracepAllocPacket();
	if (packet == NULL) {
		DTRACE_LOG_ERROR_AT(346, "%s - failed to allocate receive packet", __func__);
		return;
	}

	res = sceDfMgrRecv(ch->handle, packet->data, sizeof(DtracepHeader));
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(352, "%s - sceDfMgrRecv failed (0x%08x)", __func__, res);
		dtracepFreePacket(packet);
		return;
	}

	header = packet->header;
	size = SceDeci4pDfMgrForDebugger_CACAB5F9(&header->hdr);
	packet->size = size;
	packet->payloadSize = size - sizeof(DtracepHeader);
	res = sceDfMgrRecv(ch->handle, header + 1, size - sizeof(DtracepHeader));
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(362, "%s - sceDfMgrRecv failed (0x%08x)", __func__, res);
		dtracepFreePacket(packet);
		return;
	}

	dtracepPushPacketQueue(ch->recvQueue, packet);
	res = sceKernelSetEventFlag(ch->eventFlag, DTRACEP_EVENT_RECV);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(371, "%s - SetEventFlag failed (0x%08x)", __func__, res);
}

/* send the first waiting reply */
static void dtracepSend(DtracepChannel *ch)
{
	DtracepPacket *packet;
	int res;

	packet = dtracepPopPacketQueue(ch->sendQueue);
	res = sceDfMgrSend(ch->handle, packet->data, packet->size);
	if (res < 0) {
		/* Sony's message reports the packet's size */
		DTRACE_LOG_ERROR_AT(270, "%s - sceDfMgrSend failed (0x%08x)", __func__, packet->size);
		SCE_DBG_ASSERT_FAIL(271);
		return;
	}

	if (res != packet->size)
		DTRACE_LOG_ERROR_AT(275, "%s - sceDfMgrSend only sent %d bytes of the total %d packet bytes",
				    __func__, res, packet->size);
	dtracepPushPacketQueue(ch->sentQueue, packet);
}

static int dtracepHandler(int event, void *arg, void *ctx)
{
	DtracepChannel *ch = ctx;
	SceDfMgrStatus *status;
	int connected;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		dtracepRecv(ch);
		break;
	case SCE_DFMGR_EVENT_SEND:
		dtracepSend(ch);
		break;
	case SCE_DFMGR_EVENT_SENT:
		if (dtracepIsEmptyPacketQueue(ch->sendQueue))
			dtracepDrainPacketQueue(ch->sentQueue);
		else
			dtracepReqSend(ch);
		break;
	case SCE_DFMGR_EVENT_STATUS:
		status = arg;
		if (status->kind == 0)
			connected = status->connected;
		else if (status->kind == 1)
			connected = status->enabled;
		else
			break;
		if (connected == 0)
			ch->connected = 0;
		else if (connected == 1)
			ch->connected = 1;
		break;
	}

	return SCE_OK;
}

int dtracepInit(void)
{
	SceKernelThreadOptParam opt;
	SceDfMgrProtocol protocol;
	DtracepChannel *ch;
	DtracepChannel *arg;
	char name[256];
	SceUID uid;
	int res;
	int i;

	uid = sceKernelCreateMutex("SceDeci4pDtracep", 0x2000, 0, NULL);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(69, "%s - CreateMutex failed (0x%08X)", __func__, uid);
		return uid;
	}
	s_mutex = uid;

	protocol.size = sizeof(protocol);
	protocol.version = 0x01000002;
	protocol.subVersion = 0x20000;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	protocol.alias = 0;
	protocol.destination = 0;
	if (sceKernelCheckDipsw(195))
		protocol.flags |= DFMGR_PROTOCOL_SDIO;
	else
		protocol.flags |= DFMGR_PROTOCOL_USB;

	memset(s_channels, 0, sizeof(s_channels));
	for (i = 0; i < DTRACEP_CHANNELS; i++) {
		ch = &s_channels[i];

		uid = sceKernelCreateEventFlag("SceDeci4pDtracep", 0, 0, NULL);
		if (uid < 0) {
			DTRACE_LOG_ERROR_AT(96, "%s - CreateEventFlag failed (0x%08x)", __func__, uid);
			SCE_DBG_ASSERT_FAIL(97);
			return uid;
		}
		ch->eventFlag = uid;

		memset(name, 0, sizeof(name));
		snprintf(name, sizeof(name), "%s%d", "SceDeci4pDtracep", i);
		opt.size = sizeof(opt);
		opt.attr = 1;
		opt.kStackMemType = 0x10f0d006;
		uid = sceKernelCreateThread(name, _dtracepThread, 30, 0x4000, 0, 0x80000, &opt);
		if (uid < 0) {
			DTRACE_LOG_ERROR_AT(120, "%s - CreateThread failed (0x%08x)", __func__, uid);
			SCE_DBG_ASSERT_FAIL(121);
			return uid;
		}
		ch->thread = uid;

		arg = ch;
		res = sceKernelStartThread(uid, sizeof(arg), &arg);
		if (res < 0) {
			DTRACE_LOG_ERROR_AT(130, "%s - StartThread failed (0x%08x)", __func__, res);
			SCE_DBG_ASSERT_FAIL(131);
			return res;
		}

		ch->connected = 1;
		ch->suspended = 0;
		ch->recvQueue = dtracepCreatePacketQueue();
		ch->sendQueue = dtracepCreatePacketQueue();
		ch->sentQueue = dtracepCreatePacketQueue();

		protocol.protocol = DTRACEP_PROTOCOL + i;
		snprintf(protocol.name, sizeof(protocol.name), "DTRACEP%d", i);
		res = sceDfMgrOpen(dtracepHandler, &protocol, ch);
		if (res < 0) {
			DTRACE_LOG_ERROR_AT(150, "%s - initializing %s (0x%08x) failed (0x%08x)",
					    __func__, protocol.name, protocol.protocol, res);
			return res;
		}
		ch->handle = res;
	}

	sceKernelRegisterSysEventHandler("SceDeci4pDtracep", dtracepSysEventHandler, NULL);
	return SCE_OK;
}

int dtracepExit(void)
{
	DtracepChannel *ch;
	int status;
	int res;

	for (ch = s_channels; ch != &s_channels[DTRACEP_CHANNELS]; ch++) {
		sceDfMgrClose(ch->handle);
		dtracepDestroyPacketQueue(ch->recvQueue);
		ch->recvQueue = NULL;
		dtracepDestroyPacketQueue(ch->sendQueue);
		ch->sendQueue = NULL;
		dtracepDestroyPacketQueue(ch->sentQueue);
		ch->sentQueue = NULL;

		res = sceKernelSetEventFlag(ch->eventFlag, DTRACEP_EVENT_EXIT);
		if (res < 0) {
			DTRACE_LOG_ERROR_AT(189, "%s - SetEventFlag failed (0x%08x)", __func__, res);
			continue;
		}
		res = sceKernelWaitThreadEnd(ch->thread, &status, NULL);
		if (res < 0)
			DTRACE_LOG_ERROR_AT(196, "%s - WaitThreadEnd failed (0x%08X)\n", __func__, res);
		res = sceKernelDeleteThread(ch->thread);
		if (res < 0)
			DTRACE_LOG_ERROR_AT(203, "%s - DeleteThread failed (0x%08x)", __func__, res);
		res = sceKernelDeleteEventFlag(ch->eventFlag);
		if (res < 0)
			DTRACE_LOG_ERROR_AT(210, "%s - DeleteEventFlag failed (0x%08x)", __func__, res);
	}

	sceKernelDeleteMutex(s_mutex);
	s_mutex = -1;
	return SCE_OK;
}
