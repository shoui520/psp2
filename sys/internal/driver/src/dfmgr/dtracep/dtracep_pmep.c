/*
 * dtracep_pmep.c: the PMEP channel. A thread asks the host's communication
 * processor for its channels (names and scales) once, then polls their
 * status and power status and stores the scaled values where DTrace's
 * cpch0..cpch7 variables read them, once a second, until stopped.
 */
#include <dtrace/heap.h>
#include <dtrace/logging.h>
#include <deci4p_dfmgr/deci4p.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "dtracep_pmep.h"

#define PMEP_CHANNELS		8
#define PMEP_NAME_SIZE		32
#define PMEP_PROTOCOL		0x4000
#define PMEP_DESTINATION	0x6000
#define PMEP_POLL_INTERVAL	1000000		/* microseconds between polls */
#define PMEP_STOP_TIMEOUT	2000000

/* the thread's event flag bits */
#define PMEP_EVENT_REPLY	0x1		/* a reply came; also: start */
#define PMEP_EVENT_STOP		0x2

/* the host's commands; a reply's code is the command's plus one */
#define PMEP_CMD_GET_PROPERTIES		0x00
#define PMEP_CMD_GET_STATUS		0x02
#define PMEP_CMD_GET_POWER_STATUS	0x12

/* a poll step's result that ends the thread */
#define PMEP_POLL_STOP		PMEP_EVENT_STOP

typedef struct PmepRequest {
	SceDeci4pHeader hdr;
	SceUInt8 code;
	SceUInt8 reserved[3];
	SceUInt32 param[2];
} PmepRequest;

typedef struct PmepReply {
	SceDeci4pHeader hdr;
	SceUInt8 code;
	SceUInt8 reserved[3];
	SceUInt32 param[2];
	int result;
	SceUInt32 count;		/* the entries in the payload that follows */
} PmepReply;

typedef struct PmepPropertiesReply {
	PmepReply reply;
	SceUInt32 reserved;
} PmepPropertiesReply;

/* a channel in the properties' payload; the name is padded to 4 bytes */
typedef struct PmepChannelInfo {
	SceUInt32 size;			/* this entry and the ones after it */
	SceUInt32 nameLength;
	char name[];			/* then a word, then the channel's scale */
} PmepChannelInfo;

/* a channel in the power status' payload */
typedef struct PmepPowerStatus {
	SceUInt16 reserved;
	SceUInt16 flags;		/* bit 0: value is set */
	SceUInt32 value;
} PmepPowerStatus;

/* where the values go (sceDtracepPmepStartPolling's argument) */
typedef struct PmepValues {
	SceUInt32 value[PMEP_CHANNELS];
	char name[PMEP_CHANNELS][PMEP_NAME_SIZE];
} PmepValues;

typedef struct DtracepPmep {
	int polling;
	SceUID thread;
	SceUID eventFlag;
	int handle;			/* the protocol's, from sceDfMgrOpen */
	void *sendBuffer;		/* the request the link takes next */
	SceSize sendSize;
	void *recvBuffer;		/* where the reply goes */
	SceSize recvSize;
	void *payload;			/* what came after the reply */
	SceSize payloadSize;
	PmepValues *values;
} DtracepPmep;

static DtracepPmep s_pmep;
static float s_scale[PMEP_CHANNELS];
static uint32_t s_power[PMEP_CHANNELS];	/* a float's bits; 0: none given */
static SceUInt32 s_status[PMEP_CHANNELS];

static int _pmepThread(SceSize args, void *argp);

/* wait out the poll interval; PMEP_POLL_STOP once stopped */
static int _pmepSleep(void)
{
	SceUInt32 bits = 0;
	SceUInt32 timeout = PMEP_POLL_INTERVAL;
	int res;

	res = sceKernelWaitEventFlag(s_pmep.eventFlag, PMEP_EVENT_STOP,
				     SCE_KERNEL_EVF_WAITMODE_OR |
				     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, &timeout);
	if (res == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return 0;
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(596, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
		SCE_PANIC(597);
	}
	return bits & PMEP_EVENT_STOP;
}

static inline void _pmepSend(void)
{
	void *buffer = s_pmep.sendBuffer;
	SceSize size = s_pmep.sendSize;
	int res;

	SceDeci4pDfMgrForDebugger_6D26CC56(buffer, size);
	res = sceDfMgrSend(s_pmep.handle, buffer, size);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(365, "%s - sceDfMgrSend failed (0x%08x)", __func__, res);
}

static inline void _pmepReadDone(void)
{
	int res;

	res = sceKernelSetEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(423, "%s - SetEventFlag failed (%08x)", __func__, res);
}

/* the DECI4P manager's handler */
static int _pmepRead(int event, void *arg, void *ctx)
{
	SceDeci4pHeader *hdr;
	SceSize size, recvSize;
	int res;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		hdr = s_pmep.recvBuffer;
		recvSize = s_pmep.recvSize;
		res = sceDfMgrRecv(s_pmep.handle, hdr, recvSize);
		if (res < 0)
			DTRACE_LOG_ERROR_AT(384, "%s - DfMgrRecv failed (%08x)", __func__, res);
		size = *(SceUInt16 *)hdr->size + (hdr->size[2] << 16);
		if (size <= recvSize)
			SCE_PANIC(393);
		s_pmep.payloadSize = size - recvSize;
		s_pmep.payload = dtrace_heap_alloc(s_pmep.payloadSize);
		res = sceDfMgrRecv(s_pmep.handle, s_pmep.payload, s_pmep.payloadSize);
		if (res < 0)
			DTRACE_LOG_ERROR_AT(412, "%s - DfMgrRecv failed (%08x)", __func__, res);
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		_pmepReadDone();
		break;
	case SCE_DFMGR_EVENT_SEND:
		_pmepSend();
		break;
	}

	return 0;
}

static __attribute__((noinline)) int _pmepCreateResource(void)
{
	SceKernelThreadOptParam opt;
	char name[256];
	SceUID uid;
	int res;

	memset(name, 0, sizeof(name));
	strncpy(name, "SceDeci4pDtracepPmep", sizeof(name));
	opt.size = sizeof(opt);
	opt.attr = 1;
	opt.kStackMemType = 0x10f0d006;
	uid = sceKernelCreateThread("SceDeci4pDtracepPmep", _pmepThread, 30, 0x4000, 0,
				    0x80000, &opt);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(179, "%s - CreateThread failed (0x%08x)", __func__, uid);
		SCE_DBG_ASSERT(0, 180);
		return uid;
	}
	s_pmep.thread = uid;

	uid = sceKernelCreateEventFlag("SceDeci4pDtracepPmep", 0, 0, NULL);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(187, "%s - CreateEventFlag failed (0x%08x)", __func__, uid);
		SCE_DBG_ASSERT(0, 188);
		return uid;
	}
	s_pmep.eventFlag = uid;

	res = sceKernelStartThread(s_pmep.thread, 0, NULL);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(195, "%s - StartThread failed (0x%08x)", __func__, res);
		SCE_DBG_ASSERT(0, 196);
		return res;
	}
	return 0;
}

/* the values: each channel's status, scaled, and by its power status where it has one */
static int _pmepSetValue(void)
{
	PmepValues *values = s_pmep.values;
	float power;
	int i;

	if (values == NULL)
		SCE_PANIC(725);

	for (i = 0; i < PMEP_CHANNELS; i++) {
		power = 1.0f;
		if (s_power[i] != 0)
			power = *(float *)&s_power[i];
		values->value[i] = (SceUInt32)((float)s_status[i] * s_scale[i] * power);
	}
	return 0;
}

static int _pmepGetPowerStatus(void)
{
	PmepRequest request = { .code = PMEP_CMD_GET_POWER_STATUS };
	PmepReply reply = { };
	PmepPowerStatus *status;
	SceUInt32 bits;
	SceUInt32 i;
	int res;

	s_pmep.sendBuffer = &request;
	s_pmep.sendSize = sizeof(request);
	s_pmep.recvBuffer = &reply;
	s_pmep.recvSize = sizeof(reply);
	sceDfMgrReqSend(s_pmep.handle);
	res = sceKernelWaitEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY,
				     SCE_KERNEL_EVF_WAITMODE_OR |
				     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (res < 0) {
		if (res == SCE_KERNEL_ERROR_WAIT_DELETE)
			return res;
		DTRACE_LOG_ERROR_AT(529, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
		return res;
	}
	if (reply.code != PMEP_CMD_GET_POWER_STATUS + 1)
		DTRACE_LOG_ERROR_AT(535, "%s - res code is invalid code : 0x%08x", __func__,
				    reply.code);
	if (reply.result != 0)
		DTRACE_LOG_ERROR_AT(540, "%s - res result is error : 0x%08x", __func__,
				    reply.result);

	status = s_pmep.payload;
	SCE_DBG_ASSERT(reply.count == s_pmep.payloadSize / sizeof(*status), 557);
	for (i = 0; i < reply.count; i++) {
		if (status[i].flags & 1)
			s_power[i] = status[i].value;
	}

	if (s_pmep.payload != NULL) {
		dtrace_heap_free(s_pmep.payload);
		s_pmep.payload = NULL;
	}
	return 0;
}

static int _pmepGetStatus(void)
{
	PmepRequest request = { .code = PMEP_CMD_GET_STATUS };
	PmepReply reply = { };
	SceUInt16 *status;
	SceUInt32 bits;
	SceUInt32 i;
	int res;

	s_pmep.sendBuffer = &request;
	s_pmep.sendSize = sizeof(request);
	s_pmep.recvBuffer = &reply;
	s_pmep.recvSize = sizeof(reply);
	sceDfMgrReqSend(s_pmep.handle);
	res = sceKernelWaitEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY,
				     SCE_KERNEL_EVF_WAITMODE_OR |
				     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (res < 0) {
		if (res == SCE_KERNEL_ERROR_WAIT_DELETE)
			return res;
		DTRACE_LOG_ERROR_AT(457, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
		return res;
	}
	if (reply.code != PMEP_CMD_GET_STATUS + 1)
		DTRACE_LOG_ERROR_AT(463, "%s - res code is invalid code : 0x%08x", __func__,
				    reply.code);
	if (reply.result != 0)
		DTRACE_LOG_ERROR_AT(468, "%s - res result is error : 0x%08x", __func__,
				    reply.result);

	status = s_pmep.payload;
	SCE_DBG_ASSERT(reply.count == s_pmep.payloadSize / sizeof(*status), 484);
	for (i = 0; i < reply.count; i++)
		s_status[i] = status[i];

	if (s_pmep.payload != NULL) {
		dtrace_heap_free(s_pmep.payload);
		s_pmep.payload = NULL;
	}
	return 0;
}

/* each channel's name and scale, from the properties' payload */
static inline void _parseChannelInfo(void)
{
	char *data = s_pmep.payload;
	SceSize size = s_pmep.payloadSize;
	PmepValues *values = s_pmep.values;
	PmepChannelInfo *info = (PmepChannelInfo *)data;
	float *scale = s_scale;
	SceUInt32 length;
	SceUInt32 *tail;
	int i;

	for (i = 0; ; i++) {
		if (info->size > (SceSize)(data + size - (char *)info)) {
			DTRACE_LOG_ERROR_AT(680, "%s - payload data error", __func__);
			SCE_PANIC(681);
		}
		length = info->nameLength;
		if (values != NULL)
			strncpy(values->name[i], info->name, PMEP_NAME_SIZE - 1);
		if (length & 3)
			length = (length & ~3) + 4;
		tail = (SceUInt32 *)(info->name + length);
		*scale++ = *(float *)&tail[1];
		info = (PmepChannelInfo *)&tail[2];
		if ((SceSize)((char *)info - data) >= size)
			break;
	}

	dtrace_heap_free(data);
	s_pmep.payload = NULL;
}

static inline void _pmepGetProperties(void)
{
	PmepRequest request = { .code = PMEP_CMD_GET_PROPERTIES };
	PmepPropertiesReply reply = { };
	SceUInt32 bits;
	int res;

	s_pmep.sendBuffer = &request;
	s_pmep.sendSize = sizeof(request);
	s_pmep.recvBuffer = &reply;
	s_pmep.recvSize = sizeof(reply);
	sceDfMgrReqSend(s_pmep.handle);
	res = sceKernelWaitEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY,
				     SCE_KERNEL_EVF_WAITMODE_OR |
				     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (res < 0) {
		if (res != SCE_KERNEL_ERROR_WAIT_DELETE)
			DTRACE_LOG_ERROR_AT(635, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
		return;
	}
	if (reply.reply.code != PMEP_CMD_GET_PROPERTIES + 1)
		DTRACE_LOG_ERROR_AT(641, "%s - res code is invalid code : 0x%08x", __func__,
				    reply.reply.code);
	if (reply.reply.result != 0)
		DTRACE_LOG_ERROR_AT(646, "%s - res result is error : 0x%08x", __func__,
				    reply.reply.result);
	_parseChannelInfo();
}

static int _pmepThread(SceSize args, void *argp)
{
	int (*const poll[])(void) = {
		_pmepGetStatus, _pmepGetPowerStatus, _pmepSetValue, _pmepSleep, NULL
	};
	int (*step)(void);
	SceUInt32 bits = 0;
	int res, i;

	res = sceKernelWaitEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY | PMEP_EVENT_STOP,
				     SCE_KERNEL_EVF_WAITMODE_OR |
				     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
	if (res < 0) {
		if (res == SCE_KERNEL_ERROR_WAIT_DELETE)
			return res;
		DTRACE_LOG_ERROR_AT(280, "%s - WaitEventFlag failed (0x%08x)", __func__, res);
		return res;
	}
	if (bits & PMEP_EVENT_STOP)
		return 0;

	_pmepGetProperties();

	/* Sony's: the first step runs twice each time round */
	i = 0;
	for (;;) {
		step = poll[i++];
		if (step == NULL) {
			i = 0;
			step = poll[0];
		}
		if (step() == PMEP_POLL_STOP)
			break;
	}
	return 0;
}

static inline int _pmepOpenDfmgr(void)
{
	SceDfMgrProtocol protocol;
	int res;

	protocol.size = sizeof(protocol);
	protocol.protocol = PMEP_PROTOCOL;
	protocol.alias = 0;
	protocol.version = 0;
	protocol.subVersion = 0;
	protocol.destination = PMEP_DESTINATION;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(protocol.name, "DTRACEP_PMEP", sizeof(protocol.name));
	res = sceDfMgrOpen(_pmepRead, &protocol, NULL);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(237, "%s - DfMgrOpen failed (0x%08x)", __func__, res);
		return res;
	}
	s_pmep.handle = res;
	return 0;
}

int sceDtracepPmepStartPolling(void *values)
{
	int res, i;

	s_pmep.values = values;
	if (s_pmep.polling)
		return 0;
	if (values == NULL) {
		DTRACE_LOG_ERROR_AT(85, "%s - Invalid Addr (%p)", __func__, values);
		SCE_DBG_ASSERT(values != NULL, 86);
		return -1;
	}

	for (i = 0; i < PMEP_CHANNELS; i++) {
		s_scale[i] = 0;
		s_status[i] = 0;
	}
	res = _pmepCreateResource();
	if (res < 0)
		return res;
	res = _pmepOpenDfmgr();
	if (res < 0)
		return res;
	s_pmep.polling = 1;

	res = sceKernelSetEventFlag(s_pmep.eventFlag, PMEP_EVENT_REPLY);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(110, "%s - SetEventFlag failed (%08x)", __func__, res);
		return res;
	}
	return 0;
}

static inline void _pmepDeleteResource(void)
{
	int res;

	res = sceKernelDeleteThread(s_pmep.thread);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(208, "%s - DeleteThread failed (0x%08x)", __func__, res);
	res = sceKernelDeleteEventFlag(s_pmep.eventFlag);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(214, "%s - DeleteEventFlag failed (0x%08x)", __func__, res);
}

int sceDtracepPmepStopPolling(void)
{
	int status;
	SceUInt32 timeout;
	int res;

	if (!s_pmep.polling)
		return 0;

	res = sceKernelSetEventFlag(s_pmep.eventFlag, PMEP_EVENT_STOP);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(131, "%s - SetEventFlag failed (%08x)", __func__, res);
	timeout = PMEP_STOP_TIMEOUT;
	res = sceKernelWaitThreadEnd(s_pmep.thread, &status, &timeout);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(139, "%s - WaitThreadEnd failed (%08x)", __func__, res);
	sceDfMgrClose(s_pmep.handle);
	_pmepDeleteResource();
	s_pmep.polling = 0;
	return 0;
}
