/*
 * SceDeci4pCpup: DECI4P protocol 0x3000 ("CPUP"), requests from the target to
 * the development host (dipsw 0xC2 enables it).
 *
 * Every request is one packet in a shared 0x11000 byte buffer: header, code,
 * sequence number and arguments. The host's reply is read back into the same
 * buffer; its first argument word is an error code (0: success) followed by
 * the results. One request is in flight at a time.
 */

#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>

#define CPUP_PROTOCOL		0x3000
#define CPUP_BUF_SIZE		0x11000

/* request codes; the names of the requests are not known */
/* request codes, by the henkaku wiki's names of the exports that send them */
#define CPUP_REQ_INQUIRE_VERSION	0x00
#define CPUP_REQ_ENTER_UPDATE		0x02	/* 765F7234, DE07478F */
#define CPUP_REQ_TRANSFER_PACKAGE	0x04
#define CPUP_REQ_ABORT_UPDATE		0x06
#define CPUP_REQ_TRIGGER_UPDATE		0x08
#define CPUP_REQ_GET_PROGRESS		0x0a
#define CPUP_REQ_START_UPDATE		0x0c
#define CPUP_REQ_EXEC_COMMAND		0x0e
#define CPUP_REQ_GET_RTC		0x10
#define CPUP_REQ_SET_RTC		0x12	/* DAC4F630, 4939DA45 */
#define CPUP_REQ_GET_SERIAL_ID		0x14

typedef struct CpupPacket {
	SceDeci4pHeader hdr;
	SceUInt8 code;
	SceUInt8 reserved0;
	SceUInt16 reserved1;
	SceUInt32 reserved2;			/* sent as 0 */
	SceUInt32 seq;
	SceInt32 arg[];				/* reply: arg[0] error, then results */
} CpupPacket;

/* the reply's fixed part, read before the rest */
typedef struct CpupReplyHead {
	SceDeci4pHeader hdr;
	SceUInt32 word[3];
} CpupReplyHead;

typedef struct CpupRequest {
	SceUInt32 reserved[4];
	SceUInt32 seq;
	CpupPacket *pkt;
	SceSize size;
	SceUID evf;
} CpupRequest;

typedef struct CpupLink {
	int handle;				/* sceDfMgrOpen */
	SceUInt32 connected;
	SceUInt32 enabled;
	CpupRequest *req;
} CpupLink;

#if FW_VERSION >= 0x04000000
static SceUInt16 s_cpupSequence;
#endif
static SceUID s_cpupMemBlock;
static CpupRequest s_cpupRequest;
static CpupPacket *s_cpupBuffer;
static CpupLink s_cpupLink;
static SceUID s_cpupBufferSemaphore;
static SceUID s_cpupRequestSemaphore;
static SceUID s_cpupMutex;
#if FW_VERSION < 0x04000000
static SceUInt16 s_cpupSequence;
#endif

static const SceDfMgrProtocol s_protocol = {
	.size = sizeof(SceDfMgrProtocol),
	.protocol = CPUP_PROTOCOL,
	.version = 0x01000000,
	.subVersion = 0x00010000,
	.flags = DFMGR_PROTOCOL_ENABLE,
	.name = "CPUP",
};

/* name unknown: DfMgr event handler */
static int CpupHandler(int event, void *arg, void *ctx)
{
	CpupLink *link = ctx;
	CpupReplyHead head;
	CpupReplyHead *reply;
	SceUInt32 requestWord, reservedWord, sequence;
	SceDfMgrStatus *st;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		sceDfMgrRecv(link->handle, &head.hdr, sizeof(head.hdr));
		sceDfMgrRecv(link->handle, head.word, sizeof(head.word));
		reply = (CpupReplyHead *)s_cpupRequest.pkt;
		reply->hdr = head.hdr;
		requestWord = head.word[0];
		reservedWord = head.word[1];
		sequence = head.word[2];
		reply->word[2] = sequence;
		reply->word[0] = requestWord;
		reply->word[1] = reservedWord;
		sceDfMgrRecv(link->handle, (SceUInt8 *)s_cpupRequest.pkt + sizeof(head),
			     (SceSize)arg - sizeof(head));
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
		sceKernelSetEventFlag(link->req->evf, 1);
		sceKernelSignalSema(s_cpupRequestSemaphore, 1);
		break;
	case SCE_DFMGR_EVENT_SEND:
		sceDfMgrSend(link->handle, s_cpupRequest.pkt, s_cpupRequest.size);
		break;
	case SCE_DFMGR_EVENT_STATUS:
		st = arg;
		if (st->kind == 0) {
			if (st->connected == 0)
				link->connected = 0;
			else if (st->connected == 1)
				link->connected = 1;
		} else if (st->kind == 1) {
			if (st->enabled == 0)
				link->enabled = 0;
			else if (st->enabled == 1)
				link->enabled = 1;
		}
		break;
	}
	return 0;
}

/* name unknown */
static __attribute__((noinline)) int CpupInit(void)
{
	s_cpupMutex = sceKernelCreateMutex("SceDeci4pCpup", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (s_cpupMutex < 0)
		return s_cpupMutex;

	s_cpupMemBlock = SceSysmemForDriver_C94850C9("SceDeci4pCpup", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW,
						  CPUP_BUF_SIZE, NULL);
	SceSysmemForDriver_A841EDDA(s_cpupMemBlock, (void **)&s_cpupBuffer);

	s_cpupRequestSemaphore = sceKernelCreateSema("SceDeci4pCpup", SCE_KERNEL_ATTR_TH_PRIO, 1,
						      1, NULL);
	if (s_cpupRequestSemaphore < 0) {
		sceKernelDeleteMutex(s_cpupMutex);
		return s_cpupRequestSemaphore;
	}
	s_cpupBufferSemaphore = sceKernelCreateSema("SceDeci4pCpupBuf", SCE_KERNEL_ATTR_TH_PRIO, 1,
						     1, NULL);
	if (s_cpupBufferSemaphore < 0) {
		sceKernelDeleteMutex(s_cpupMutex);
		sceKernelDeleteSema(s_cpupRequestSemaphore);
		return s_cpupBufferSemaphore;
	}
	s_cpupRequest.evf = sceKernelCreateEventFlag("SceDeci4pCpup", 0, 0, NULL);
	if (s_cpupRequest.evf < 0) {
		sceKernelDeleteMutex(s_cpupMutex);
		sceKernelDeleteSema(s_cpupRequestSemaphore);
		sceKernelDeleteSema(s_cpupBufferSemaphore);
		return s_cpupRequest.evf;
	}

	s_cpupLink.handle = 0;
	s_cpupLink.connected = 0;
	s_cpupLink.enabled = 0;
	s_cpupLink.req = NULL;
	s_cpupLink.handle = sceDfMgrOpen(CpupHandler, &s_protocol, &s_cpupLink);
	s_cpupLink.connected = 0;
	s_cpupLink.enabled = 0;
	s_cpupLink.req = &s_cpupRequest;
	return 0;
}

int CpupModuleStart(SceSize args, void *argp)
{
	if (!SceDipswForDriver_A98FC2FD(0xc2)) {
		SCE_DBG_PRINTF(0xb, 102, "disabled by dipsw\n");
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	return CpupInit() < 0 ? SCE_KERNEL_START_FAILED : SCE_KERNEL_START_SUCCESS;
}

/* name unknown: take the packet buffer */
static __attribute__((noinline)) CpupPacket *CpupGetBuffer(void)
{
	if (sceKernelWaitSema(s_cpupBufferSemaphore, 1, NULL) == 0)
		return s_cpupBuffer;
	return NULL;
}

/* name unknown */
static __attribute__((noinline)) void CpupReleaseBuffer(void)
{
	sceKernelSignalSema(s_cpupBufferSemaphore, 1);
}

/* name unknown: send pkt and wait for the reply */
static int CpupSendRequest(CpupPacket *pkt, SceSize size)
{
	CpupRequest *req;
	SceUInt32 sequence;
	int handle;

	if (sceKernelWaitSema(s_cpupRequestSemaphore, 1, NULL) == 0) {
		req = &s_cpupRequest;
		req->seq = s_cpupSequence++;
	} else {
		/* Sony's code carries on with no request */
		req = NULL;
	}
	req->pkt = pkt;
	req->size = size;

	pkt->hdr.version = SCE_DECI4P_VERSION;
	SceDeci4pDfMgrForDebugger_6D26CC56(&pkt->hdr, size);
	sequence = req->seq;
	pkt->hdr.protocol = CPUP_PROTOCOL;
	pkt->seq = sequence;
	handle = s_cpupLink.handle;
	pkt->hdr.src = SCE_DECI4P_PORT_TARGET;
	pkt->hdr.dst = 0;
	sceDfMgrReqSend(handle);

	return sceKernelWaitEventFlag(req->evf, 1,
				       SCE_KERNEL_EVF_WAITMODE_OR |
				       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
}

static inline __attribute__((always_inline)) CpupPacket *CpupNewPacket(SceSize size,
								     SceUInt8 code)
{
	CpupPacket *pkt = CpupGetBuffer();

	if (pkt == NULL)
		return NULL;
	memset(pkt, 0, size);
	pkt->code = code;
	pkt->reserved2 = 0;
	pkt->seq = 0;
	return pkt;
}

/* the host's error code, else the request's own result */
static inline __attribute__((always_inline)) int CpupFinish(CpupPacket *pkt, int ret)
{
	if (pkt->arg[0] != 0)
		ret = pkt->arg[0];
	CpupReleaseBuffer();
	return ret;
}

/* sceDeci4pCpupInquireVersionForDriver */
int sceDeci4pCpupInquireVersionForDriver(SceUInt32 *protocolVersion, SceUInt32 *packageVersion,
				      SceUInt32 *boardVersion)
{
	CpupPacket *pkt;
	int ret, err;

	if (protocolVersion == NULL || packageVersion == NULL || boardVersion == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x1c, CPUP_REQ_INQUIRE_VERSION)) == NULL)
		return -1;

	ret = CpupSendRequest(pkt, 0x1c);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	*protocolVersion = pkt->arg[1];
	*packageVersion = pkt->arg[2];
	*boardVersion = pkt->arg[3];
	CpupReleaseBuffer();
	return ret;
}

/* sceDeci4pCpupStartUpdateForDriver with its third argument */
int SceDeci4pCpupForDriver_765F7234(SceUInt32 arg0, SceUInt32 arg1, SceUInt32 arg2, SceUInt32 *ctx)
{
	CpupPacket *pkt;
	int ret, err;

	if (ctx == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x28, CPUP_REQ_ENTER_UPDATE)) == NULL)
		return -1;
	pkt->arg[0] = arg0;
	pkt->arg[1] = arg1;
	pkt->arg[2] = arg2;

	ret = CpupSendRequest(pkt, 0x28);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	*ctx = pkt->arg[1];
	CpupReleaseBuffer();
	return ret;
}

/* sceDeci4pCpupStartUpdateForDriver: enter CP update */
int sceDeci4pCpupStartUpdateForDriver(SceUInt32 arg0, SceUInt32 arg1, SceUInt32 *ctx)
{
	CpupPacket *pkt;
	int ret, err;

	if (ctx == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x28, CPUP_REQ_ENTER_UPDATE)) == NULL)
		return -1;
	pkt->arg[0] = arg0;
	pkt->arg[1] = arg1;
	pkt->arg[2] = 0;

	ret = CpupSendRequest(pkt, 0x28);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	*ctx = pkt->arg[1];
	CpupReleaseBuffer();
	return ret;
}

/* sceDeci4pCpupTransferUpdatePackageForDriver */
int sceDeci4pCpupTransferUpdatePackageForDriver(SceUInt32 ctx, SceUInt32 offset,
					     SceSize size, const void *data)
{
	SceSize total = size + 0x28;
	CpupPacket *pkt;
	int ret;

	if (total > CPUP_BUF_SIZE)
		return -1;
	if ((pkt = CpupNewPacket(total, CPUP_REQ_TRANSFER_PACKAGE)) == NULL)
		return -1;
	pkt->arg[0] = ctx;
	pkt->arg[1] = offset;
	pkt->arg[2] = size;
	memcpy(&pkt->arg[3], data, size);

	ret = CpupSendRequest(pkt, total);
	return CpupFinish(pkt, ret);
}

static inline __attribute__((always_inline)) int CpupSimpleRequest(SceUInt8 code,
								  SceUInt32 value)
{
	CpupPacket *pkt;
	int ret;

	if ((pkt = CpupNewPacket(0x20, code)) == NULL)
		return -1;
	pkt->arg[0] = value;

	ret = CpupSendRequest(pkt, 0x20);
	return CpupFinish(pkt, ret);
}

/* abort CP update */
int SceDeci4pCpupForDriver_02CDA82C(SceUInt32 ctx)
{
	return CpupSimpleRequest(CPUP_REQ_ABORT_UPDATE, ctx);
}

/* sceDeci4pCpupTriggerUpdateForDriver */
int sceDeci4pCpupTriggerUpdateForDriver(SceUInt32 ctx)
{
	return CpupSimpleRequest(CPUP_REQ_TRIGGER_UPDATE, ctx);
}

/* sceDeci4pCpupGetProgressForDriver */
int sceDeci4pCpupGetProgressForDriver(SceUInt32 ctx, SceUInt32 *out0, SceUInt32 *out1,
				     SceUInt32 *out2)
{
	CpupPacket *pkt;
	int ret, err;

	if (out0 == NULL || out1 == NULL || out2 == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x20, CPUP_REQ_GET_PROGRESS)) == NULL)
		return -1;
	pkt->arg[0] = ctx;

	ret = CpupSendRequest(pkt, 0x20);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	*out0 = pkt->arg[1];
	*out1 = pkt->arg[2];
	*out2 = pkt->arg[3];
	CpupReleaseBuffer();
	return ret;
}

/* start CP update */
int SceDeci4pCpupForDriver_2BAD7B39(SceUInt32 ctx)
{
	return CpupSimpleRequest(CPUP_REQ_START_UPDATE, ctx);
}

/* sceDeci4pCpupExecCommand */
int sceDeci4pCpupExecCommand(SceUInt32 command)
{
	return CpupSimpleRequest(CPUP_REQ_EXEC_COMMAND, command);
}

/* sceDeci4pCpupGetRtcForDriver */
int sceDeci4pCpupGetRtcForDriver(SceUInt32 *rtc, int physical)
{
	CpupPacket *pkt;
	int ret, err;

	if (rtc == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x20, CPUP_REQ_GET_RTC)) == NULL)
		return -1;
	pkt->arg[0] = physical != 0;

	ret = CpupSendRequest(pkt, 0x20);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	*rtc = pkt->arg[1];
	CpupReleaseBuffer();
	return ret;
}

/* sceDeci4pCpupSetRtcForDriver */
int sceDeci4pCpupSetRtcForDriver(SceUInt32 rtc, int physical)
{
	CpupPacket *pkt;
	int ret;

	if ((pkt = CpupNewPacket(0x3c, CPUP_REQ_SET_RTC)) == NULL)
		return -1;
	pkt->arg[0] = physical != 0;
	pkt->arg[1] = rtc;
	pkt->arg[2] = 0;

	ret = CpupSendRequest(pkt, 0x3c);
	return CpupFinish(pkt, ret);
}

/* sceDeci4pCpupSetRtcWithActivationKeyForDriver */
int sceDeci4pCpupSetRtcWithActivationKeyForDriver(SceUInt32 rtc, int physical,
					      const SceUInt32 *key)
{
	CpupPacket *pkt;
	SceUInt32 secondWord, thirdWord, fourthWord;
	int ret;

	if ((pkt = CpupNewPacket(0x3c, CPUP_REQ_SET_RTC)) == NULL)
		return -1;
	if (physical != 0)
		physical = 1;
	pkt->arg[0] = physical;
	pkt->arg[1] = rtc;
	pkt->arg[2] = 0;
	if (key != NULL) {
		pkt->arg[0] = physical | 2;
		secondWord = key[1];
		thirdWord = key[2];
		fourthWord = key[3];
		pkt->arg[3] = key[0];
		pkt->arg[4] = secondWord;
		pkt->arg[5] = thirdWord;
		pkt->arg[6] = fourthWord;
		pkt->arg[7] = key[4];
	}

	ret = CpupSendRequest(pkt, 0x3c);
	return CpupFinish(pkt, ret);
}

/* sceDeci4pCpupGetCpSerialIdForDriver */
int sceDeci4pCpupGetCpSerialIdForDriver(void *serialId, SceSize *size)
{
	CpupPacket *pkt;
	SceSize n;
	int ret, err;

	if (serialId == NULL || size == NULL)
		return -1;
	if ((pkt = CpupNewPacket(0x1c, CPUP_REQ_GET_SERIAL_ID)) == NULL)
		return -1;

	ret = CpupSendRequest(pkt, 0x1c);
	if ((err = pkt->arg[0]) != 0) {
		CpupReleaseBuffer();
		return err;
	}
	n = *size;
	if ((SceSize)pkt->arg[1] < n)
		*size = n = pkt->arg[1];
	memcpy(serialId, &pkt->arg[2], n);
	CpupReleaseBuffer();
	return ret;
}
