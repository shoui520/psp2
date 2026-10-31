#include <libkernel/libkernel.h>
#include "shell_util.h"
#include "shellsvc.h"
#include "../ipmi/ipmi_error.h"

#define SHELL_UTIL_MAX_REQUESTS		4
#define SHELL_UTIL_THREAD_PRIORITY	0x10000100
#define SHELL_UTIL_THREAD_STACK_SIZE	0x1000
#define SHELL_UTIL_POLL_INTERVAL	17000

/* The ShellSvc event flag bit (index 4 in its client's flags) a blocking request waits on. */
#define SHELL_UTIL_BLOCKING_EVENT	4

/* The impose events the impose thread waits for, and the bits it answers with. */
#define IMPOSE_EVENT_NAME		"SceShellUtilImposeEvent"
#define IMPOSE_EVENT_WAIT_BITS		0xC9E
#define IMPOSE_EVENT_1			0x2
#define IMPOSE_EVENT_2_ON		0x4
#define IMPOSE_EVENT_2_OFF		0x8
#define IMPOSE_EVENT_3			0x10
#define IMPOSE_EVENT_3_NO		0x20
#define IMPOSE_EVENT_3_YES		0x40
#define IMPOSE_EVENT_PING		0x80
#define IMPOSE_EVENT_PONG		0x100
#define IMPOSE_EVENT_4_ON		0x400
#define IMPOSE_EVENT_4_OFF		0x800
#define IMPOSE_EVENT_NO_CALLBACK	0x200

/* The kind of a pending request, which the event handler receives as its event. */
enum RequestKind {
	REQUEST_NONE = 0,
	REQUEST_LOCK = 1,
	REQUEST_UNLOCK = 2,
	REQUEST_156FEACF = 3,
	REQUEST_040997D6 = 4,
	REQUEST_40DFAC6B = 5,
	REQUEST_03201F4B = 6,
	REQUEST_96BBF91B = 7,
	REQUEST_8B69AD27 = 8,
	REQUEST_B65B60CA = 9,
	REQUEST_3219A1C0 = 10,
	REQUEST_9B0EE918 = 11,
	REQUEST_6430C45E = 12,
	REQUEST_29DB95D1 = 13,
	REQUEST_0B066FAD = 14,
	REQUEST_01141995 = 16,
	REQUEST_E701BA71 = 17,
	REQUEST_0E7EAD89 = 18,
	REQUEST_F75C684D = 19,
	REQUEST_8CAAB23B = 20,
	REQUEST_71C21A8C = 22,
	REQUEST_FB612CA8 = 23,
	REQUEST_9A83AC60 = 24
};

/* An asynchronous request waiting for its result. */
struct Request {
	int requestId;
	int kind;
	int param;
	Request *pNext;
};

static bool s_initialized = 0;
static ShellUtilEventHandler s_handler = 0;
static void *s_pUserData = 0;
static SceUID s_threadId = -1;
static ShellUtilImposeEventCallback s_imposeCallback = 0;
static void *s_pImposeArg = 0;
static SceUID s_imposeThreadId = -1;
static bool s_imposeStop = 0;
static Request *s_pHead = 0;
static Request *s_pTail = 0;
static int s_psButtonLockCount = 0;

static SceKernelLwCondWork s_cond;
static Request s_requests[SHELL_UTIL_MAX_REQUESTS];
static SceKernelLwMutexWork s_mutex;

static int clientThread(SceSize argSize, void *pArgBlock);
static int psButtonCallback(int event, void *pArg);

static Request *findFreeRequest()
{
	SceUInt32 i;

	for (i = 0; i < SHELL_UTIL_MAX_REQUESTS; i++) {
		if (s_requests[i].kind == REQUEST_NONE)
			return &s_requests[i];
	}
	return NULL;
}

/* Queues a sent request under its kind; the caller holds the mutex. */
static void queueRequest(Request *pRequest, int kind)
{
	Request **ppLink;

	pRequest->kind = kind;
	ppLink = &s_pHead;
	if (s_pTail != NULL)
		ppLink = &s_pTail->pNext;
	*ppLink = pRequest;
	s_pTail = pRequest;
	if (s_threadId != -1)
		sceKernelSignalLwCond(&s_cond);
}

int sceShellUtilInitEvents(int flags)
{
	int ret;

	if (s_initialized)
		return SHELL_UTIL_ERROR_ALREADY_INITIALIZED;
	ret = sceKernelCreateLwMutex(&s_mutex, "SceShellUtilClientMutex", 0x2002, 0, NULL);
	if (ret != 0)
		return ret;
	s_initialized = 1;
	if ((flags & SHELL_UTIL_INIT_NO_THREAD) == 0) {
		ret = sceKernelCreateLwCond(&s_cond, "SceShellUtilClientCond", 0x2000, &s_mutex, NULL);
		if (ret == 0) {
			s_threadId = sceKernelCreateThread("SceShellUtilClientThread", clientThread,
							   SHELL_UTIL_THREAD_PRIORITY,
							   SHELL_UTIL_THREAD_STACK_SIZE, 0, 0, NULL);
			if (s_threadId > 0) {
				sceClibMemset(s_requests, 0, sizeof(s_requests));
				ret = sceKernelStartThread(s_threadId, 0, NULL);
				if (ret >= 0)
					return ret;
				sceKernelDeleteThread(s_threadId);
			} else {
				ret = s_threadId;
			}
			if (ret >= 0)
				return ret;
			sceKernelDeleteLwCond(&s_cond);
		}
		if (ret < 0) {
			sceKernelDeleteLwMutex(&s_mutex);
			s_threadId = -1;
			s_initialized = 0;
		}
	}
	return ret;
}

void SceShellUtilUketorne_FB551721(void)
{
	if (!s_initialized)
		return;
	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	s_initialized = 0;
	if (s_threadId != -1)
		sceKernelSignalLwCond(&s_cond);
	sceKernelUnlockLwMutex(&s_mutex, 1);
	if (s_threadId != -1) {
		sceKernelWaitThreadEnd(s_threadId, NULL, NULL);
		sceKernelDeleteThread(s_threadId);
		sceKernelDeleteLwCond(&s_cond);
		s_threadId = -1;
	}
	sceKernelDeleteLwMutex(&s_mutex);
	s_handler = NULL;
	s_pUserData = NULL;
	s_pHead = NULL;
	s_pTail = NULL;
	s_psButtonLockCount = 0;
}

/* Passes the first finished request to the handler and frees its slot. */
int SceShellUtilUketorne_FDC48BBE(void)
{
	Request *pPrev;
	Request *pRequest;
	Request **ppLink;
	Request *pNext;
	int ret;

	if (!s_initialized)
		return SHELL_UTIL_ERROR_NOT_INITIALIZED;
	sceKernelLockLwMutex(&s_mutex, 1, NULL);
	if (s_pHead != NULL) {
		pPrev = NULL;
		for (pRequest = s_pHead; pRequest != NULL; pRequest = pRequest->pNext) {
			ret = shellUtilIpcGetResult(SceShellSvc_B31E7F1C(), pRequest->requestId);
			if (ret != IPMI_ERROR_WOULD_BLOCK) {
				if (s_handler != NULL)
					s_handler(ret, pRequest->kind, pRequest->param, s_pUserData);
				ppLink = pPrev != NULL ? &pPrev->pNext : &s_pHead;
				pNext = (*ppLink)->pNext;
				sceClibMemset(*ppLink, 0, sizeof(Request));
				*ppLink = pNext;
				if (pNext == NULL)
					s_pTail = pPrev;
				break;
			}
			pPrev = pRequest;
		}
	}
	if (s_psButtonLockCount != 0) {
		SceShellSvc_FB03FBC9(53);
		SceShellSvc_FB03FBC9(54);
		SceShellSvc_FB03FBC9(55);
		SceShellSvc_FB03FBC9(56);
		SceShellSvc_FB03FBC9(57);
	}
	sceKernelUnlockLwMutex(&s_mutex, 1);
	return 0;
}

int sceShellUtilRegisterEventHandler(ShellUtilEventHandler handler, void *pUserData)
{
	int ret;

	ret = SHELL_UTIL_ERROR_NOT_INITIALIZED;
	if (s_initialized) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		s_handler = handler;
		s_pUserData = pUserData;
		sceKernelUnlockLwMutex(&s_mutex, 1);
		ret = 0;
	}
	return ret;
}

int sceShellUtilLock(int type)
{
	Request *pRequest;
	int ret;

	ret = SHELL_UTIL_ERROR_NOT_INITIALIZED;
	if (s_initialized) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		pRequest = findFreeRequest();
		if (pRequest != NULL) {
			ret = shellUtilIpcLock(SceShellSvc_B31E7F1C(), &pRequest->requestId, NULL, type);
			if (ret == 0) {
				pRequest->param = type;
				queueRequest(pRequest, REQUEST_LOCK);
				if ((type & SHELL_UTIL_LOCK_TYPE_PS_BTN) != 0) {
					if (s_psButtonLockCount == 0) {
						SceShellSvc_D943CE15(53, psButtonCallback, NULL);
						SceShellSvc_D943CE15(54, psButtonCallback, NULL);
						SceShellSvc_D943CE15(55, psButtonCallback, NULL);
						SceShellSvc_D943CE15(56, psButtonCallback, NULL);
						SceShellSvc_D943CE15(57, psButtonCallback, NULL);
					}
					s_psButtonLockCount++;
				}
			}
		} else {
			ret = SHELL_UTIL_ERROR_BUSY;
		}
		sceKernelUnlockLwMutex(&s_mutex, 1);
	}
	return ret;
}

int sceShellUtilUnlock(int type)
{
	Request *pRequest;
	int ret;

	ret = SHELL_UTIL_ERROR_NOT_INITIALIZED;
	if (s_initialized) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		pRequest = findFreeRequest();
		if (pRequest != NULL) {
			ret = shellUtilIpcUnlock(SceShellSvc_B31E7F1C(), &pRequest->requestId, NULL, type);
			if (ret == 0) {
				pRequest->param = type;
				queueRequest(pRequest, REQUEST_UNLOCK);
				if ((type & SHELL_UTIL_LOCK_TYPE_PS_BTN) != 0 && s_psButtonLockCount != 0) {
					if (--s_psButtonLockCount == 0) {
						SceShellSvc_7CBF442B(53);
						SceShellSvc_7CBF442B(54);
						SceShellSvc_7CBF442B(55);
						SceShellSvc_7CBF442B(56);
						SceShellSvc_7CBF442B(57);
					}
				}
			}
		} else {
			ret = SHELL_UTIL_ERROR_BUSY;
		}
		sceKernelUnlockLwMutex(&s_mutex, 1);
	}
	return ret;
}

int SceShellUtilUketorne_156FEACF(int type)
{
	Request *pRequest;
	int ret;

	ret = SHELL_UTIL_ERROR_NOT_INITIALIZED;
	if (s_initialized) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		pRequest = findFreeRequest();
		if (pRequest != NULL) {
			ret = shellUtilIpc_156FEACF(SceShellSvc_B31E7F1C(), &pRequest->requestId, NULL, type);
			if (ret == 0) {
				pRequest->param = type;
				queueRequest(pRequest, REQUEST_156FEACF);
				if ((type & SHELL_UTIL_LOCK_TYPE_PS_BTN) != 0)
					s_psButtonLockCount = 0;
			}
		} else {
			ret = SHELL_UTIL_ERROR_BUSY;
		}
		sceKernelUnlockLwMutex(&s_mutex, 1);
	}
	return ret;
}

/*
 * The asynchronous requests without a lock type: prepare the input, send, then queue under
 * the kind. Each export is one copy of this.
 */
#define SHELL_UTIL_ASYNC_REQUEST(requestKind, prepare, call)			\
	do {									\
		Request *pRequest;						\
										\
		ret = SHELL_UTIL_ERROR_NOT_INITIALIZED;				\
		if (s_initialized) {						\
			prepare;						\
			sceKernelLockLwMutex(&s_mutex, 1, NULL);		\
			pRequest = findFreeRequest();				\
			if (pRequest != NULL) {					\
				ret = call;					\
				if (ret == 0)					\
					queueRequest(pRequest, requestKind);	\
			} else {						\
				ret = SHELL_UTIL_ERROR_BUSY;			\
			}							\
			sceKernelUnlockLwMutex(&s_mutex, 1);			\
		}								\
	} while (0)

#define REQUEST_ID	&pRequest->requestId

int SceShellUtil_040997D6(int value)
{
	int ret;

	SHELL_UTIL_ASYNC_REQUEST(REQUEST_040997D6, ,
		shellUtilIpc_040997D6(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, value));
	return ret;
}

/* Copies up to max characters and terminates the copy. */
template <typename Char>
static void copyString(Char *pDst, const Char *pSrc, SceUInt32 max)
{
	SceUInt32 n = 0;

	if (pSrc != NULL) {
		while (*pSrc != 0 && n < max) {
			pDst[n] = *pSrc++;
			n++;
		}
	}
	pDst[n] = 0;
}

int SceShellUtil_40DFAC6B(const SceWChar16 *pString1, const SceWChar16 *pString2)
{
	SceWChar16 string1[9];
	SceWChar16 string2[9];
	int ret;

	SHELL_UTIL_ASYNC_REQUEST(REQUEST_40DFAC6B,
		copyString(string1, pString1, 8);
		copyString(string2, pString2, 8),
		shellUtilIpc_40DFAC6B(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, string1, string2));
	return ret;
}

int SceShellUtil_03201F4B(const char *pString)
{
	char string[32];
	int ret;

	SHELL_UTIL_ASYNC_REQUEST(REQUEST_03201F4B,
		copyString(string, pString, sizeof(string) - 1),
		shellUtilIpc_03201F4B(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, string));
	return ret;
}

#define SHELL_UTIL_ASYNC_INT_EXPORT(nid)					\
int SceShellUtil_##nid(int value)						\
{										\
	int ret;								\
										\
	SHELL_UTIL_ASYNC_REQUEST(REQUEST_##nid, ,				\
		shellUtilIpc_##nid(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, value)); \
	return ret;								\
}

#define SHELL_UTIL_ASYNC_VOID_EXPORT(nid)					\
int SceShellUtil_##nid(void)							\
{										\
	int ret;								\
										\
	SHELL_UTIL_ASYNC_REQUEST(REQUEST_##nid, ,				\
		shellUtilIpc_##nid(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL));	\
	return ret;								\
}

SHELL_UTIL_ASYNC_INT_EXPORT(96BBF91B)
SHELL_UTIL_ASYNC_INT_EXPORT(8B69AD27)
SHELL_UTIL_ASYNC_INT_EXPORT(B65B60CA)
SHELL_UTIL_ASYNC_INT_EXPORT(3219A1C0)
SHELL_UTIL_ASYNC_VOID_EXPORT(9B0EE918)

int SceShellUtil_8F2F143D(int value)
{
	return shellUtilIpc_8F2F143D(SceShellSvc_B31E7F1C(), value);
}

int SceShellUtil_636544FB(int value)
{
	return shellUtilIpc_636544FB(SceShellSvc_B31E7F1C(), value);
}

int SceShellUtil_BB54D049(int value1, int value2, int value3)
{
	return shellUtilIpc_BB54D049(SceShellSvc_B31E7F1C(), value1, value2, value3);
}

SHELL_UTIL_ASYNC_VOID_EXPORT(6430C45E)
SHELL_UTIL_ASYNC_INT_EXPORT(29DB95D1)
SHELL_UTIL_ASYNC_VOID_EXPORT(0B066FAD)
SHELL_UTIL_ASYNC_INT_EXPORT(01141995)

int SceShellUtil_E701BA71(const char *pString)
{
	char string[32];
	int ret;

	SHELL_UTIL_ASYNC_REQUEST(REQUEST_E701BA71,
		copyString(string, pString, sizeof(string) - 1),
		shellUtilIpc_E701BA71(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, string));
	return ret;
}

SHELL_UTIL_ASYNC_INT_EXPORT(0E7EAD89)
SHELL_UTIL_ASYNC_INT_EXPORT(F75C684D)

/*
 * The blocking requests: sent with ShellSvc's event flag bit, waited for, and their result
 * taken at once.
 */
int SceShellUtil_4A234ABB(void)
{
	IPMI::EventFlagInfo eventFlag;
	int requestId;
	int ret;

	ret = SceShellSvc_A0B067AC(&eventFlag, SHELL_UTIL_BLOCKING_EVENT);
	if (ret == 0) {
		ret = shellUtilIpc_4A234ABB(SceShellSvc_B31E7F1C(), &requestId, &eventFlag);
		if (ret == 0) {
			ret = SceShellSvc_8D4F9F2E(SHELL_UTIL_BLOCKING_EVENT, NULL);
			if (ret == 0)
				ret = shellUtilIpcGetResult(SceShellSvc_B31E7F1C(), requestId);
		}
	}
	return ret;
}

int sceShellUtilLaunchAppRequestLaunchApp(const SceShellUtilLaunchAppParam *param)
{
	IPMI::EventFlagInfo eventFlag;
	char cmd[960];
	int requestId;
	int ret;

	ret = SHELL_UTIL_ERROR_INVALID_ARGUMENT;
	if (param != NULL) {
		copyString(cmd, param->cmd, sizeof(cmd) - 1);
		ret = SceShellSvc_A0B067AC(&eventFlag, SHELL_UTIL_BLOCKING_EVENT);
		if (ret == 0) {
			ret = shellUtilIpcLaunchApp(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, cmd);
			if (ret == 0) {
				ret = SceShellSvc_8D4F9F2E(SHELL_UTIL_BLOCKING_EVENT, NULL);
				if (ret == 0)
					ret = shellUtilIpcGetResult(SceShellSvc_B31E7F1C(), requestId);
			}
		}
	}
	return ret;
}

int SceShellUtil_6F730976(void)
{
	return shellUtilIpc_6F730976(SceShellSvc_B31E7F1C());
}

SHELL_UTIL_ASYNC_VOID_EXPORT(8CAAB23B)

#define SHELL_UTIL_BLOCKING_STRING_EXPORT(nid)					\
int SceShellUtil_##nid(const char *pString)					\
{										\
	IPMI::EventFlagInfo eventFlag;						\
	char string[256];							\
	int requestId;								\
	int ret;								\
										\
	copyString(string, pString, sizeof(string) - 1);			\
	ret = SceShellSvc_A0B067AC(&eventFlag, SHELL_UTIL_BLOCKING_EVENT);	\
	if (ret == 0) {	\
		ret = shellUtilIpc_##nid(SceShellSvc_B31E7F1C(), &requestId, &eventFlag, string);	\
		if (ret == 0) {	\
			ret = SceShellSvc_8D4F9F2E(SHELL_UTIL_BLOCKING_EVENT, NULL);	\
			if (ret == 0)	\
				ret = shellUtilIpcGetResult(SceShellSvc_B31E7F1C(), requestId);	\
		}	\
	}	\
	return ret;	\
}

SHELL_UTIL_BLOCKING_STRING_EXPORT(48C7E8F8)
SHELL_UTIL_BLOCKING_STRING_EXPORT(B67A0C68)
SHELL_UTIL_ASYNC_VOID_EXPORT(71C21A8C)
SHELL_UTIL_ASYNC_VOID_EXPORT(FB612CA8)

int SceShellUtil_9A83AC60(int value1, int value2)
{
	int ret;

	SHELL_UTIL_ASYNC_REQUEST(REQUEST_9A83AC60, ,
		shellUtilIpc_9A83AC60(SceShellSvc_B31E7F1C(), REQUEST_ID, NULL, value1, value2));
	return ret;
}

/* Waits for impose events and passes each to the impose callback. */
int imposeEventThread(SceSize argSize, void *pArgBlock)
{
	SceUInt32 bits;
	SceUID evfId;
	int event;
	int param;
	int ret;

	evfId = sceKernelOpenEventFlag(IMPOSE_EVENT_NAME);
	if (evfId < 0) {
		sceClibPrintf("sceKernelOpenEventFlag %x", evfId);
		return -1;
	}
	while (1) {
		ret = sceKernelWaitEventFlag(evfId, IMPOSE_EVENT_WAIT_BITS,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0) {
			sceClibPrintf("sceKernelWaitEventFlag %x\n", ret);
			break;
		}
		if (s_imposeCallback == NULL) {
			sceKernelSetEventFlag(evfId, IMPOSE_EVENT_NO_CALLBACK);
		} else {
			if ((bits & IMPOSE_EVENT_1) != 0) {
				event = 1;
				param = 0;
			} else if ((bits & IMPOSE_EVENT_2_ON) != 0) {
				event = 2;
				param = 1;
			} else if ((bits & IMPOSE_EVENT_2_OFF) != 0) {
				event = 2;
				param = 0;
			} else if ((bits & IMPOSE_EVENT_4_ON) != 0) {
				event = 4;
				param = 1;
			} else if ((bits & IMPOSE_EVENT_4_OFF) != 0) {
				event = 4;
				param = 0;
			} else if ((bits & IMPOSE_EVENT_3) != 0) {
				event = 3;
				param = 0;
			} else {
				param = 0;
				event = 0;
				if ((bits & IMPOSE_EVENT_PING) != 0) {
					sceKernelSetEventFlag(evfId, IMPOSE_EVENT_PONG);
					continue;
				}
			}
			ret = s_imposeCallback(0, event, param, s_pImposeArg);
			if ((bits & IMPOSE_EVENT_3) != 0)
				sceKernelSetEventFlag(evfId, ret != 0 ? IMPOSE_EVENT_3_YES :
								      IMPOSE_EVENT_3_NO);
		}
		if (s_imposeStop)
			break;
		sceKernelDelayThread(SHELL_UTIL_POLL_INTERVAL);
	}
	if (sceKernelCloseEventFlag(evfId) < 0)
		sceClibPrintf("sceKernelCloseEventFlag %x", evfId);
	s_imposeThreadId = -1;
	ret = sceKernelExitDeleteThread(0);
	if (ret != 0)
		sceClibPrintf("sceKernelExitThread %x \n", ret);
	return 0;
}

int SceShellUtil_CE35B2B8(ShellUtilImposeEventCallback callback, void *pArg)
{
	s_imposeCallback = callback;
	s_pImposeArg = pArg;
	return 0;
}

int SceShellUtil_EC5881A5(void)
{
	int ret;

	if (s_imposeThreadId != -1)
		return SHELL_UTIL_ERROR_ALREADY_INITIALIZED;
	s_imposeCallback = NULL;
	s_pImposeArg = NULL;
	s_imposeStop = 0;
	s_imposeThreadId = sceKernelCreateThread(IMPOSE_EVENT_NAME, imposeEventThread,
						 SHELL_UTIL_THREAD_PRIORITY,
						 SHELL_UTIL_THREAD_STACK_SIZE, 0, 0, NULL);
	if (s_imposeThreadId > 0) {
		ret = sceKernelStartThread(s_imposeThreadId, 0, NULL);
		if (ret < 0)
			sceKernelDeleteThread(s_imposeThreadId);
	} else {
		ret = s_imposeThreadId;
	}
	if (ret != 0)
		sceClibPrintf("initWaitImposeEventFlagThread %x \n", ret);
	return ret;
}

void SceShellUtil_F979148A(void)
{
	SceUID evfId;
	int ret;

	s_imposeStop = 1;
	evfId = sceKernelOpenEventFlag(IMPOSE_EVENT_NAME);
	if (evfId < 0) {
		sceClibPrintf("sceKernelOpenEventFlag %x", evfId);
		return;
	}
	ret = sceKernelCancelEventFlag(evfId, 0, NULL);
	if (ret != 0)
		sceClibPrintf("sceKernelCancelEventFlag %x", ret);
	sceKernelCloseEventFlag(evfId);
	if (evfId < 0)
		sceClibPrintf("sceKernelCloseEventFlag %x", evfId);
}

/* Polls the requests while any is pending, and sleeps on the condition otherwise. */
static int clientThread(SceSize argSize, void *pArgBlock)
{
	while (1) {
		sceKernelLockLwMutex(&s_mutex, 1, NULL);
		SceShellUtilUketorne_FDC48BBE();
		if (((SceUInt32)s_pHead | s_psButtonLockCount) == 0) {
			if (!s_initialized)
				break;
			sceKernelWaitLwCond(&s_cond, NULL);
			if (((SceUInt32)s_pHead | s_psButtonLockCount) == 0)
				break;
		}
		sceKernelUnlockLwMutex(&s_mutex, 1);
		sceKernelDelayThread(SHELL_UTIL_POLL_INTERVAL);
	}
	return 0;
}

/* ShellSvc's callback for a PS-button event while a PS-button lock is held. */
static int psButtonCallback(int event, void *pArg)
{
	int param;

	if (s_handler != NULL) {
		switch (event) {
		case 54:
			param = 1;
			break;
		case 55:
			param = 2;
			break;
		case 56:
			param = 3;
			break;
		case 57:
			param = 4;
			break;
		default:
			param = 0;
			break;
		}
		s_handler(0, SHELL_UTIL_EVENT_PS_BTN, param, s_pUserData);
	}
	return 0;
}
