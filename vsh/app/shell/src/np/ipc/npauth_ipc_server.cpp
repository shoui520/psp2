/*
 * npauth_ipc_server.cpp: SceNpAuthServer ShellSvc IPC handler (0x0F0000..0x0F0005,
 * 0x8121DF68 - 0x8121E728).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "np_ipc_internal.h"

#define NP_AUTH_SERVER_MAX_REQUESTS		16
#define NP_AUTH_SHELL_EVENT_TICKET_READY	48

#define NP_AUTH_SLOT_FREE			0
#define NP_AUTH_SLOT_PENDING			1
#define NP_AUTH_SLOT_READY			2
#define NP_AUTH_SLOT_RETRIEVED			3

#define NP_AUTH_IPC_CREATE_ASYNC_REQUEST	0x000F0000
#define NP_AUTH_IPC_CREATE_ASYNC_REQUEST_EX	0x000F0001
#define NP_AUTH_IPC_DESTROY_REQUEST		0x000F0002
#define NP_AUTH_IPC_ABORT_REQUEST		0x000F0003
#define NP_AUTH_IPC_READ_TICKET			0x000F0004
#define NP_AUTH_IPC_GET_RESULT			0x000F0005

/* 80-byte request tracking slot (0x815A2DF0, 16 entries = 0x500 bytes) */
struct NpAuthServerSlot {
	IPMI::Session *session;		/* +0x00 */
	int requestId;			/* +0x04 */
	int result;			/* +0x08 */
	SceUInt8 *ticketBuf;		/* +0x0C */
	SceSize ticketSize;		/* +0x10 */
	int state;			/* +0x14 */
	SceUInt32 reserved[10];		/* +0x18..+0x3F */
	SceInt64 completedTime;		/* +0x40 */
	SceUID clientPid;		/* +0x48 */
	SceUInt32 pad;			/* +0x4C */
};

/* Packed parameter headers for create/read IPC calls */
struct NpAuthCreateIpcHeader {
	int hasServiceId;
	int hasCookie;
	int hasEntitlementId;
};

struct NpAuthReadIpcHeader {
	int requestId;
	SceSize offset;
	SceSize readSize;
};

static int s_npAuthServerInitialized = 0;				/* 0x815451A4 */
static SceUID s_npAuthServerMutex = -1;					/* 0x815A2DE8 */
static NpAuthServerSlot s_npAuthSlots[NP_AUTH_SERVER_MAX_REQUESTS];	/* 0x815A2DF0 */

static int npAuthServerOnTicketComplete(int requestId, int resultOrSize);
static int npAuthServerDispatchSync(IPMI::Session *session, SceUInt32 methodId,
				    const IPMI::DataInfo *inList, SceUInt32 inCount,
				    IPMI::BufferInfo *outList, SceUInt32 outCount);

/* 0x8121DF68 */
int npAuthServerInit(void)
{
	SceUID mutexId;
	int i;

	if (s_npAuthServerInitialized != 0)
		return NP_AUTH_ERR_ALREADY_INIT;

	sceClibMemset(s_npAuthSlots, 0, sizeof(s_npAuthSlots));
	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++)
		s_npAuthSlots[i].state = NP_AUTH_SLOT_FREE;

	mutexId = sceKernelCreateMutex("SceNpAuthServer", 2u, 0, NULL);
	if (mutexId < 0) {
		if (s_npAuthServerMutex > 0) {
			sceKernelDeleteMutex(s_npAuthServerMutex);
			s_npAuthServerMutex = -1;
		}
		return 0;
	}

	s_npAuthServerMutex = mutexId;
	s_npAuthServerInitialized = 1;
	return 0;
}

/* 0x8121E016 */
int shellSvcDisp_0F_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return npAuthServerDispatchSync(session, methodId, inList, inCount, outList, outCount);
}

/* 0x8121E02A */
int shellSvcDisp_0F_Async(IPMI::Session *session, SceUInt32 reqId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)reqId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return ipmiAsyncNotSupported();
}

/* 0x8121E03A */
void shellSvcDisp_0F_Disconn(IPMI::Session *session)
{
	int i;

	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].clientPid == session->getProcessId()) {
			npAuthCoreAbortRequest(s_npAuthSlots[i].requestId);
			npAuthCoreDestroyRequest(s_npAuthSlots[i].requestId);
		}
	}

	sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].clientPid == session->getProcessId()) {
			s_npAuthSlots[i].session = NULL;
			if (s_npAuthSlots[i].ticketBuf != NULL) {
				sceNpCommonFreeNpServerName(s_npAuthSlots[i].ticketBuf);
				s_npAuthSlots[i].ticketBuf = NULL;
			}
			sceClibMemset(&s_npAuthSlots[i], 0, sizeof(NpAuthServerSlot));
			s_npAuthSlots[i].state = NP_AUTH_SLOT_FREE;
			s_npAuthSlots[i].requestId = 0;
			s_npAuthSlots[i].clientPid = 0;
		}
	}
	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
}

/* 0x8121E116 / 0x8121E25A shared helper */
static int npAuthServerCreateRequestCommon(IPMI::Session *session,
					   const NpAuthInternalParam *inParam,
					   int hasServiceId, const char *serviceId,
					   int hasCookie, const void *cookie,
					   int hasEntitlementId, const char *entitlementId,
					   bool isExtended)
{
	NpAuthInternalParam param;
	int reqId;
	int slotIdx;
	int ret;

	ret = sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	sceClibMemcpy_safe(&param, inParam, sizeof(param));
	param.callback = npAuthServerOnTicketComplete;
	param.serviceId = (hasServiceId > 0) ? serviceId : NULL;
	param.cookie = (hasCookie > 0) ? cookie : NULL;
	param.entitlementId = (hasEntitlementId > 0) ? entitlementId : NULL;

	reqId = isExtended ? npAuthCoreCreateAsyncRequestEx(&param)
			   : npAuthCoreCreateAsyncRequest(&param);
	ret = reqId;
	if (reqId >= 0) {
		for (slotIdx = 0; slotIdx < NP_AUTH_SERVER_MAX_REQUESTS; slotIdx++) {
			if (s_npAuthSlots[slotIdx].state == NP_AUTH_SLOT_FREE)
				break;
		}
		if (slotIdx == NP_AUTH_SERVER_MAX_REQUESTS) {
			ret = NP_AUTH_ERR_TABLE_FULL;
		} else {
			char noticeName[32];
			s_npAuthSlots[slotIdx].session = session;
			s_npAuthSlots[slotIdx].requestId = reqId;
			s_npAuthSlots[slotIdx].state = NP_AUTH_SLOT_PENDING;
			s_npAuthSlots[slotIdx].clientPid = session->getProcessId();
			sceClibMemset(noticeName, 0, sizeof(noticeName));
			sceClibStrlcpy(noticeName, "SceNpAuth", sizeof(noticeName));
			npDebugShowNotice("NpAuth", serviceId, noticeName);
		}
	}

	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
	return ret;
}

/* 0x8121E116 */
static int npAuthServerCreateRequest(IPMI::Session *session, const NpAuthInternalParam *inParam,
				     int hasServiceId, const char *serviceId,
				     int hasCookie, const void *cookie,
				     int hasEntitlementId, const char *entitlementId)
{
	return npAuthServerCreateRequestCommon(session, inParam, hasServiceId, serviceId,
					       hasCookie, cookie, hasEntitlementId, entitlementId,
					       false);
}

/* 0x8121E25A */
static int npAuthServerCreateRequestEx(IPMI::Session *session, const NpAuthInternalParam *inParam,
				       int hasServiceId, const char *serviceId,
				       int hasCookie, const void *cookie,
				       int hasEntitlementId, const char *entitlementId)
{
	return npAuthServerCreateRequestCommon(session, inParam, hasServiceId, serviceId,
					       hasCookie, cookie, hasEntitlementId, entitlementId,
					       true);
}

/* 0x8121E39E */
static int npAuthServerDestroyRequest(IPMI::Session *session, int requestId)
{
	int ret;
	int i;

	(void)session;
	ret = sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].requestId == requestId)
			break;
	}
	if (i != NP_AUTH_SERVER_MAX_REQUESTS) {
		if (s_npAuthSlots[i].ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(s_npAuthSlots[i].ticketBuf);
			s_npAuthSlots[i].ticketBuf = NULL;
		}
		sceClibMemset(&s_npAuthSlots[i], 0, sizeof(NpAuthServerSlot));
		s_npAuthSlots[i].state = NP_AUTH_SLOT_FREE;
	}

	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
	return npAuthCoreDestroyRequest(requestId);
}

/* 0x8121E412 */
static int npAuthServerAbortRequest(IPMI::Session *session, int requestId)
{
	(void)session;
	return npAuthCoreAbortRequest(requestId);
}

/* 0x8121E41C */
int npAuthServerReadTicket(IPMI::Session *session, int requestId, void *outBuf,
			   SceSize offset, SceSize readSize)
{
	int ret;
	int i;

	(void)session;
	ret = sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].requestId == requestId)
			break;
	}
	if (i == NP_AUTH_SERVER_MAX_REQUESTS)
		ret = NP_AUTH_ERR_NOT_FOUND;
	else
		sceClibMemcpy_safe(outBuf, s_npAuthSlots[i].ticketBuf + offset, readSize);

	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
	return ret;
}

/* 0x8121E4AE */
static int npAuthServerGetResult(IPMI::Session *session, int requestId, int *outResult)
{
	int ret;
	int i;

	(void)session;
	ret = sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].state == NP_AUTH_SLOT_READY &&
		    s_npAuthSlots[i].requestId == requestId) {
			break;
		}
	}
	if (i == NP_AUTH_SERVER_MAX_REQUESTS) {
		ret = NP_AUTH_ERR_NOT_FOUND;
	} else {
		*outResult = s_npAuthSlots[i].result;
		s_npAuthSlots[i].state = NP_AUTH_SLOT_RETRIEVED;
	}

	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
	return ret;
}

/* 0x8121E532 */
static int npAuthServerOnTicketComplete(int requestId, int resultOrSize)
{
	int i;

	sceKernelLockMutex(s_npAuthServerMutex, 1, NULL);
	for (i = 0; i < NP_AUTH_SERVER_MAX_REQUESTS; i++) {
		if (s_npAuthSlots[i].requestId == requestId)
			break;
	}
	if (i != NP_AUTH_SERVER_MAX_REQUESTS) {
		NpAuthServerSlot *slot = &s_npAuthSlots[i];
		int finalResult;

		if (resultOrSize > 0) {
			if (slot->ticketBuf != NULL) {
				sceNpCommonFreeNpServerName(slot->ticketBuf);
				slot->ticketBuf = NULL;
			}
			slot->ticketSize = (SceSize)resultOrSize;
			slot->ticketBuf = (SceUInt8 *)sceNpCommonMallocNpServerName((SceSize)resultOrSize);
			if (slot->ticketBuf != NULL) {
				slot->completedTime = sceKernelGetSystemTimeWide();
				finalResult = npAuthCoreGetTicket(requestId, slot->ticketBuf,
								  (SceSize)resultOrSize);
			} else {
				finalResult = NP_AUTH_ERR_NO_MEMORY;
			}
		} else {
			finalResult = resultOrSize;
		}

		slot->result = finalResult;
		slot->state = NP_AUTH_SLOT_READY;
		if (slot->session != NULL)
			shellSvcNotifyEvent(slot->session, NP_AUTH_SHELL_EVENT_TICKET_READY);
	}
	sceKernelUnlockMutex(s_npAuthServerMutex, 1);
	return 0;
}

/* 0x8121E5F8 */
static int npAuthServerDispatchSync(IPMI::Session *session, SceUInt32 methodId,
				    const IPMI::DataInfo *inList, SceUInt32 inCount,
				    IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;

	switch (methodId) {
	case NP_AUTH_IPC_CREATE_ASYNC_REQUEST: {
		const NpAuthCreateIpcHeader *hdr = (const NpAuthCreateIpcHeader *)inList[0].pData;
		int res = npAuthServerCreateRequest(session,
						    (const NpAuthInternalParam *)inList[1].pData,
						    hdr->hasServiceId,
						    (const char *)inList[2].pData,
						    hdr->hasCookie,
						    inList[3].pData,
						    hdr->hasEntitlementId,
						    (const char *)inList[4].pData);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_AUTH_IPC_CREATE_ASYNC_REQUEST_EX: {
		const NpAuthCreateIpcHeader *hdr = (const NpAuthCreateIpcHeader *)inList[0].pData;
		int res = npAuthServerCreateRequestEx(session,
						      (const NpAuthInternalParam *)inList[1].pData,
						      hdr->hasServiceId,
						      (const char *)inList[2].pData,
						      hdr->hasCookie,
						      inList[3].pData,
						      hdr->hasEntitlementId,
						      (const char *)inList[4].pData);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_AUTH_IPC_DESTROY_REQUEST: {
		int reqId = *(const int *)inList[0].pData;
		int res = npAuthServerDestroyRequest(session, reqId);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_AUTH_IPC_ABORT_REQUEST: {
		int reqId = *(const int *)inList[0].pData;
		int res = npAuthServerAbortRequest(session, reqId);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_AUTH_IPC_READ_TICKET: {
		const NpAuthReadIpcHeader *hdr = (const NpAuthReadIpcHeader *)inList[0].pData;
		int res = npAuthServerReadTicket(session, hdr->requestId, outList[0].pBuffer,
						 hdr->offset, hdr->readSize);
		outList[0].outSize = hdr->readSize;
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	case NP_AUTH_IPC_GET_RESULT: {
		int reqId = *(const int *)inList[0].pData;
		int res = npAuthServerGetResult(session, reqId, (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(res, outList, outCount);
	}
	default:
		return IPMI_ERR_UNSUPPORTED_METHOD;
	}
}
