/*
 * np_update_clock.cpp: SceNpUpdateClock background clock-synchronization service
 * (0x81328E68..0x813297EF). Fetches an auth ticket for Sony's dedicated clock
 * account ("SCEI-0", version 2.1) and applies its server timestamp to the secure,
 * network, and optionally user RTC clocks.
 */
#include "np_manager_core.h"

#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <rtc/rtc.h>
#include <np/np_auth.h>
#include <np_common/np_common.h>
#include "../auth/np_auth_core.h"

extern "C" {
extern int vshRtcSetCurrentTick(const SceRtcTick *tick);
extern int vshRtcSetCurrentSecureTick(const SceRtcTick *tick);
extern int vshRtcSetCurrentNetworkTick(const SceRtcTick *tick);
extern void *sce_paf_memset(void *dst, int ch, SceSize len);
}

#define NP_CLOCK_MAX_REQUEST_ID		15
#define NP_CLOCK_DEFAULT_PRIORITY	0x10000100
#define NP_CLOCK_THREAD_STACK_SIZE	0x4000U
#define NP_CLOCK_UNIX_EPOCH_TICKS	62135596800000000ULL

enum NpClockUpdateMode {
	NP_CLOCK_MODE_SECURE_ONLY	= 0,
	NP_CLOCK_MODE_SECURE_AND_USER	= 1,
	NP_CLOCK_MODE_NETWORK_ONLY	= 2
};

enum NpClockRequestStatus {
	NP_CLOCK_STATUS_IDLE		= 0,
	NP_CLOCK_STATUS_RUNNING		= 1,
	NP_CLOCK_STATUS_FINISHED	= 2
};

/* 48-byte active clock update request (0x8132906E) */
struct NpClockUpdateRequest {
	NpClockUpdateRequest *next;	/* +0x00 */
	int reqId;			/* +0x04 */
	void *ticketBuf;		/* +0x08 */
	SceSize ticketSize;		/* +0x0C */
	int resultCode;			/* +0x10 */
	SceUID semaId;			/* +0x14 */
	SceUID threadId;		/* +0x18 */
	int aborted;			/* +0x1C */
	int status;			/* +0x20 */
	int mode;			/* +0x24 */
	SceUID callerPid;		/* +0x28 */
	int authReqId;			/* +0x2C */
};

/* Global SceNpUpdateClock state (0x815467E8..0x81546800, 0x815C6088..0x815C60F0) */
static SceKernelLwMutexWork *s_clockListMutex = NULL;
static SceUID s_clockExecMutex = -1;
static SceUID s_clockExecCond = -1;
static SceUID s_clockActiveThreadId = -1;
static int s_clockLastReqId = 0;
static NpClockUpdateRequest *s_clockReqListHead = NULL;
static int s_clockInitialized = 0;
static char s_clockPassword[36];
static char s_clockLoginId[68];

static int npUpdateClockAuthCallback(int authReqId, int resultOrSize, void *userArg);
static int npUpdateClockThreadEntry(SceSize args, void *argp);

static NpClockUpdateRequest *npUpdateClockFindLocked(int reqId)
{
	NpClockUpdateRequest *cur = s_clockReqListHead;
	while (cur != NULL) {
		if (cur->reqId == reqId)
			break;
		cur = cur->next;
	}
	return cur;
}

static NpClockUpdateRequest *npUpdateClockFind(int reqId)
{
	sceKernelLockLwMutex(s_clockListMutex, 1, NULL);
	NpClockUpdateRequest *found = npUpdateClockFindLocked(reqId);
	sceKernelUnlockLwMutex(s_clockListMutex, 1);
	return found;
}

static void npUpdateClockUnlinkAndFree(NpClockUpdateRequest *req)
{
	sceKernelLockLwMutex(s_clockListMutex, 1, NULL);
	NpClockUpdateRequest *cur = s_clockReqListHead;
	NpClockUpdateRequest *prev = NULL;
	bool unlinked = false;
	while (cur != NULL) {
		if (cur == req) {
			if (prev != NULL)
				prev->next = cur->next;
			else
				s_clockReqListHead = cur->next;
			cur->next = NULL;
			unlinked = true;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	sceKernelUnlockLwMutex(s_clockListMutex, 1);

	if (unlinked) {
		if (req->semaId != -1) {
			sceKernelDeleteSema(req->semaId);
			req->semaId = -1;
		}
		sceNpCommonFreeNpServerName(req);
	}
}

/* 0x81328E68 */
int npUpdateClockInit(void)
{
	if (s_clockInitialized > 0)
		return NP_MGR_ERR_ALREADY_INIT;

	int ret = sceKernelCreateMutex("SceNpUpdateClock", 0, 0, NULL);
	if (ret >= 0) {
		s_clockExecMutex = ret;
		ret = sceKernelCreateCond("SceNpUpdateClock", 0, s_clockExecMutex, NULL);
		if (ret >= 0) {
			s_clockExecCond = ret;
			s_clockListMutex = static_cast<SceKernelLwMutexWork *>(
				sceNpCommonMallocNpServerName(sizeof(SceKernelLwMutexWork)));
			if (s_clockListMutex != NULL) {
				ret = sceKernelCreateLwMutex(s_clockListMutex, "SceNpUpdateClock",
							     0x2002U, 0, NULL);
				if (ret >= 0) {
					sceClibStrncpy(s_clockLoginId, "VrEa2uUV3s2hTgLj",
						       sizeof(s_clockLoginId));
					sceClibStrncpy(s_clockPassword, "BDFvcv2vQ2mGvxsJ",
						       sizeof(s_clockPassword));
					s_clockInitialized = 1;
					return 0;
				}
			} else {
				ret = NP_MGR_ERR_NO_MEMORY;
			}
		}
	}

	if (s_clockListMutex != NULL) {
		sceKernelDeleteLwMutex(s_clockListMutex);
		sceNpCommonFreeNpServerName(s_clockListMutex);
		s_clockListMutex = NULL;
	}
	if (s_clockExecCond != -1) {
		sceKernelDeleteCond(s_clockExecCond);
		s_clockExecCond = -1;
	}
	if (s_clockExecMutex != -1) {
		sceKernelDeleteMutex(s_clockExecMutex);
		s_clockExecMutex = -1;
	}
	return ret;
}

/* 0x81328FAA */
int npSignInDialogCleanup(void)
{
	if (s_clockInitialized <= 0)
		return 0;

	sceKernelLockLwMutex(s_clockListMutex, 1, NULL);
	NpClockUpdateRequest *cur = s_clockReqListHead;
	while (cur != NULL) {
		NpClockUpdateRequest *next = cur->next;
		sceNpCommonFreeNpServerName(cur);
		cur = next;
	}
	s_clockReqListHead = NULL;
	sceKernelUnlockLwMutex(s_clockListMutex, 1);

	if (s_clockListMutex != NULL) {
		sceKernelDeleteLwMutex(s_clockListMutex);
		sceNpCommonFreeNpServerName(s_clockListMutex);
		s_clockListMutex = NULL;
	}
	if (s_clockExecCond != -1) {
		sceKernelDeleteCond(s_clockExecCond);
		s_clockExecCond = -1;
	}
	if (s_clockExecMutex != -1) {
		sceKernelDeleteMutex(s_clockExecMutex);
		s_clockExecMutex = -1;
	}
	s_clockInitialized = 0;
	return 0;
}

/* 0x8132906E */
static int npUpdateClockStartInternal(int *outReqId, int threadPriority, int mode, SceUID callerPid)
{
	if (s_clockInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	int priority = (threadPriority != 0) ? threadPriority : NP_CLOCK_DEFAULT_PRIORITY;

	sceKernelLockLwMutex(s_clockListMutex, 1, NULL);
	int candidateId = (s_clockLastReqId != NP_CLOCK_MAX_REQUEST_ID)
			  ? (s_clockLastReqId + 1) : 1;
	int reqId = NP_MGR_ERR_NOT_FOUND;

	if (candidateId != s_clockLastReqId) {
		while (npUpdateClockFind(candidateId) != NULL) {
			candidateId = (candidateId != NP_CLOCK_MAX_REQUEST_ID)
				      ? (candidateId + 1) : 1;
			if (candidateId == s_clockLastReqId) {
				candidateId = NP_MGR_ERR_NOT_FOUND;
				break;
			}
		}
		if (candidateId > 0) {
			s_clockLastReqId = candidateId;
			NpClockUpdateRequest *node = static_cast<NpClockUpdateRequest *>(
				sceNpCommonMallocNpServerName(sizeof(NpClockUpdateRequest)));
			if (node != NULL) {
				sce_paf_memset(node, 0, 4U);
				node->reqId = candidateId;
				node->next = s_clockReqListHead;
				s_clockReqListHead = node;
				reqId = candidateId;
			} else {
				reqId = NP_MGR_ERR_NO_MEMORY;
			}
		}
	}
	sceKernelUnlockLwMutex(s_clockListMutex, 1);

	if (reqId < 0)
		return reqId;

	NpClockUpdateRequest *req = npUpdateClockFind(reqId);
	int ret = NP_MGR_ERR_INTERNAL;
	if (req != NULL) {
		req->semaId = -1;
		req->threadId = -1;
		ret = sceKernelCreateSema("SceNpUpdateClock", 0, 0, 1, NULL);
		if (ret >= 0) {
			req->semaId = ret;
			req->ticketBuf = NULL;
			req->ticketSize = 0;
			req->resultCode = 0;
			req->aborted = 0;
			req->status = NP_CLOCK_STATUS_IDLE;
			req->mode = mode;
			req->callerPid = callerPid;

			ret = sceKernelCreateThread("SceNpUpdateClock", npUpdateClockThreadEntry,
						    priority, NP_CLOCK_THREAD_STACK_SIZE,
						    0, 0, NULL);
			if (ret >= 0) {
				req->threadId = ret;
				NpClockUpdateRequest *threadArg = req;
				ret = sceKernelStartThread(req->threadId, sizeof(threadArg),
							   &threadArg);
				if (ret >= 0) {
					*outReqId = reqId;
					return 0;
				}
			}
		}
	}

	if (req != NULL) {
		if (req->threadId != -1) {
			sceKernelDeleteThread(req->threadId);
			req->threadId = -1;
		}
		if (req->semaId != -1) {
			sceKernelDeleteSema(req->semaId);
			req->semaId = -1;
		}
		npUpdateClockUnlinkAndFree(req);
	}
	return ret;
}

/* 0x8132932C */
int npSignInDialogStartModeA(int *outReqId, int dialogParam, SceUID pid)
{
	return npUpdateClockStartInternal(outReqId, dialogParam, NP_CLOCK_MODE_SECURE_AND_USER, pid);
}

/* 0x81329338 */
int npSignInDialogStartModeB(int *outReqId, int dialogParam, SceUID pid)
{
	return npUpdateClockStartInternal(outReqId, dialogParam, NP_CLOCK_MODE_SECURE_ONLY, pid);
}

/* 0x81329344 */
int npSignInDialogStartModeC(int *outReqId, int dialogParam, SceUID pid)
{
	return npUpdateClockStartInternal(outReqId, dialogParam, NP_CLOCK_MODE_NETWORK_ONLY, pid);
}

/* 0x81329350 */
int npSignInDialogGetStatus(int dialogReqId)
{
	NpClockUpdateRequest *req = npUpdateClockFind(dialogReqId);
	if (s_clockInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;
	return (req != NULL) ? req->status : NP_CLOCK_STATUS_IDLE;
}

/* 0x813293AE */
int npSignInDialogAbort(int dialogReqId)
{
	NpClockUpdateRequest *req = npUpdateClockFind(dialogReqId);
	if (req == NULL)
		return NP_MGR_ERR_INVALID_ARG;
	if (s_clockInitialized == 0)
		return NP_MGR_ERR_NOT_INIT;

	int ret = 0;
	sceKernelLockMutex(s_clockExecMutex, 1, NULL);
	req->aborted = 1;
	if (req->threadId != -1)
		ret = sceKernelSignalCondTo(s_clockExecCond, req->threadId);
	sceKernelUnlockMutex(s_clockExecMutex, 1);

	if (req->authReqId != 0)
		return npAuthCoreAbortRequest(req->authReqId);
	return ret;
}

/* 0x8132945E */
int npSignInDialogGetResult(int dialogReqId, int *outResult)
{
	NpClockUpdateRequest *req = npUpdateClockFind(dialogReqId);
	if (req == NULL)
		return NP_MGR_ERR_INVALID_ARG;

	if (outResult != NULL)
		*outResult = req->resultCode;

	npUpdateClockUnlinkAndFree(req);
	return 0;
}

int npUpdateClockStart(int *outReqId, int threadPriority, int callerPid)
{
	return npSignInDialogStartModeA(outReqId, threadPriority, callerPid);
}

int npUpdateClockGetState(int reqId)
{
	return npSignInDialogGetStatus(reqId);
}

int npUpdateClockGetResult(int reqId, int *outResult)
{
	return npSignInDialogGetResult(reqId, outResult);
}

/* 0x8132951C */
static int npUpdateClockAuthCallback(int authReqId, int resultOrSize, void *userArg)
{
	NpClockUpdateRequest *req = static_cast<NpClockUpdateRequest *>(userArg);
	if (req == NULL)
		return 0;

	int result = resultOrSize;
	if (resultOrSize > 0) {
		if (req->ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(req->ticketBuf);
			req->ticketBuf = NULL;
		}
		req->ticketSize = static_cast<SceSize>(resultOrSize);
		req->ticketBuf = sceNpCommonMallocNpServerName(req->ticketSize);
		if (req->ticketBuf != NULL) {
			result = npAuthCoreGetTicket(authReqId, req->ticketBuf, req->ticketSize);
		} else {
			result = NP_MGR_ERR_NO_MEMORY;
		}
	}

	req->resultCode = result;
	sceKernelSignalSema(req->semaId, 1);
	return 0;
}

/* 0x8132956C */
static int npUpdateClockThreadEntry(SceSize args, void *argp)
{
	(void)args;
	NpClockUpdateRequest *req = *static_cast<NpClockUpdateRequest **>(argp);
	req->status = NP_CLOCK_STATUS_RUNNING;

	sceKernelLockMutex(s_clockExecMutex, 1, NULL);
	while (s_clockActiveThreadId != -1) {
		sceKernelWaitCond(s_clockExecCond, NULL);
		if (req->aborted > 0) {
			req->resultCode = NP_MGR_ERR_ABORTED;
			sceKernelUnlockMutex(s_clockExecMutex, 1);

			sceKernelLockMutex(s_clockExecMutex, 1, NULL);
			req->threadId = -1;
			if (s_clockActiveThreadId == sceKernelGetThreadId())
				s_clockActiveThreadId = -1;
			sceKernelSignalCond(s_clockExecCond);
			sceKernelUnlockMutex(s_clockExecMutex, 1);

			req->status = NP_CLOCK_STATUS_FINISHED;
			sceKernelExitDeleteThread(NP_MGR_ERR_ABORTED);
			return NP_MGR_ERR_ABORTED;
		}
	}

	s_clockActiveThreadId = sceKernelGetThreadId();
	sceKernelUnlockMutex(s_clockExecMutex, 1);

	SceUID selfPid = sceKernelGetProcessId();
	unsigned int netToken = 0;
	if (selfPid != req->callerPid)
		netToken = static_cast<unsigned int>(SceShell_6B8772A2(req->callerPid));

	NpAuthTicketVersion ticketVer;
	ticketVer.major = 2;
	ticketVer.minor = 1;
	req->authReqId = 0;

	int ret = npAuthCoreCreateRequest(&req->authReqId, &ticketVer, "SCEI-0", 0);
	if (ret >= 0)
		ret = npAuthCoreSetCredentials(req->authReqId, s_clockLoginId, s_clockPassword);
	if (ret >= 0) {
		req->resultCode = 0;
		if (req->aborted > 0) {
			ret = NP_MGR_ERR_ABORTED;
		} else {
			ret = npAuthCoreStartAsyncDefault(req->authReqId, NULL,
							  npUpdateClockAuthCallback, req);
			if (ret >= 0) {
				sceKernelWaitSema(req->semaId, 0, NULL);
				while (npAuthCoreGetRequestStatus(req->authReqId) < 4)
					sceKernelDelayThread(100000U);

				ret = req->resultCode;
				if (ret >= 0) {
					SceNpTicketParam paramBuf;
					ret = sceNpAuthGetTicketParam(
						static_cast<const unsigned char *>(req->ticketBuf),
						req->ticketSize,
						SCE_NP_TICKET_PARAM_ISSUED_DATE,
						&paramBuf);
					if (ret >= 0) {
						SceUInt64 issueMs = 0;
						sceClibMemcpy_safe(&issueMs, paramBuf.data,
								   sizeof(issueMs));
						SceRtcTick serverTick;
						serverTick.tick = issueMs * 1000ULL +
								  NP_CLOCK_UNIX_EPOCH_TICKS;
						if ((req->mode & NP_CLOCK_MODE_NETWORK_ONLY) != 0) {
							ret = vshRtcSetCurrentNetworkTick(&serverTick);
						} else {
							ret = vshRtcSetCurrentSecureTick(&serverTick);
							if (ret >= 0 &&
							    (req->mode & NP_CLOCK_MODE_SECURE_AND_USER) != 0) {
								ret = vshRtcSetCurrentTick(&serverTick);
							}
						}
					}
				}
			}
		}
	}

	if (req->ticketBuf != NULL) {
		sceNpCommonFreeNpServerName(req->ticketBuf);
		req->ticketBuf = NULL;
	}
	if (req->semaId != -1) {
		sceKernelDeleteSema(req->semaId);
		req->semaId = -1;
	}
	if (req->authReqId > 0) {
		npAuthCoreDestroyRequest(req->authReqId);
		req->authReqId = 0;
	}

	req->aborted = 0;
	if (ret < 0 && req->resultCode == 0)
		req->resultCode = ret;

	if (selfPid != req->callerPid)
		SceShell_794BF5BE(netToken);

	sceKernelLockMutex(s_clockExecMutex, 1, NULL);
	req->threadId = -1;
	if (s_clockActiveThreadId == sceKernelGetThreadId())
		s_clockActiveThreadId = -1;
	sceKernelSignalCond(s_clockExecCond);
	sceKernelUnlockMutex(s_clockExecMutex, 1);

	req->status = NP_CLOCK_STATUS_FINISHED;
	sceKernelExitDeleteThread(ret);
	return ret;
}
