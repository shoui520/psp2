/*
 * SceNpCommon: the NP authentication client. Ticket requests run in the shell's NP service,
 * reached through ShellSvc requests; the service's event reports a finished request, whose
 * callback then gets its result (the ticket size, or an error).
 */

#include "np_common_internal.h"
#include <vsh/lowlayer/src/shell_svc/shellsvc.h>

#define NP_AUTH_REQUEST_MAX		16
#define NP_AUTH_TICKET_CHUNK_SIZE	4096

/* the ShellSvc event of a finished request */
#define NP_AUTH_EVENT			48

/* the NP service's requests */
#define NP_AUTH_IPC_CREATE_START_REQUEST	0xf0000
#define NP_AUTH_IPC_DESTROY_REQUEST		0xf0002
#define NP_AUTH_IPC_ABORT_REQUEST		0xf0003
#define NP_AUTH_IPC_GET_TICKET			0xf0004
#define NP_AUTH_IPC_GET_RESULT			0xf0005

typedef struct NpAuthRequest {
	SceNpAuthRequestId id;
	SceNpAuthCallback callback;	/* NULL: the slot is free */
	void *arg;
	int result;			/* the ticket size, or an error */
} NpAuthRequest;

static SceUID s_npAuthMutex = -1;
static int s_npAuthInitialized = 0;
static NpAuthRequest s_npAuthRequests[NP_AUTH_REQUEST_MAX];

static int npAuthEventHandler(int event, void *arg);
__attribute__((noinline))
static int npAuthIpcCreateStartRequest(struct ShellSvcInterface *svc,
				       const SceNpAuthRequestParameter *param,
				       SceSize serviceIdSize, const char *serviceId,
				       SceSize cookieSize, const void *cookie,
				       SceSize entitlementIdSize, const char *entitlementId);
__attribute__((noinline))
static int npAuthIpcDestroyRequest(struct ShellSvcInterface *svc, SceNpAuthRequestId id);
__attribute__((noinline))
static int npAuthIpcAbortRequest(struct ShellSvcInterface *svc, SceNpAuthRequestId id);
__attribute__((noinline))
static int npAuthIpcGetTicket(struct ShellSvcInterface *svc, SceNpAuthRequestId id, void *buf,
			      SceSize offset, SceSize size);
__attribute__((noinline))
static int npAuthIpcGetResult(struct ShellSvcInterface *svc, SceNpAuthRequestId id,
			      int *result);

int sceNpAuthInit(void)
{
	SceUID mutex;
	int ret;

	if (s_npAuthInitialized)
		return SCE_NP_AUTH_ERROR_ALREADY_INITIALIZED;
	sceClibMemset(s_npAuthRequests, 0, sizeof(s_npAuthRequests));
	mutex = sceKernelCreateMutex("SceNpAuthClient", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	ret = mutex;
	if (mutex < 0)
		goto error;
	s_npAuthMutex = mutex;
	ret = SceShellSvc_D943CE15(NP_AUTH_EVENT, npAuthEventHandler, NULL);
	if (ret < 0)
		goto error;
	s_npAuthInitialized = 1;
	return 0;

error:
	SceShellSvc_7CBF442B(NP_AUTH_EVENT);
	if (s_npAuthMutex > 0) {
		sceKernelDeleteMutex(s_npAuthMutex);
		s_npAuthMutex = -1;
	}
	return ret;
}

int sceNpAuthDestroyRequest(SceNpAuthRequestId id)
{
	int ret;
	int i;

	if (!s_npAuthInitialized)
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockMutex(s_npAuthMutex, 1, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < NP_AUTH_REQUEST_MAX; i++) {
		if (s_npAuthRequests[i].id == id)
			break;
	}
	if (i == NP_AUTH_REQUEST_MAX) {
		ret = SCE_NP_AUTH_ERROR_ESRCH;
	} else {
		ret = npAuthIpcDestroyRequest(SceShellSvc_B31E7F1C(), id);
		if (ret >= 0) {
			s_npAuthRequests[i].callback = NULL;
			sceClibMemset(&s_npAuthRequests[i], 0, sizeof(s_npAuthRequests[i]));
		}
	}
	sceKernelUnlockMutex(s_npAuthMutex, 1);
	return ret;
}

int sceNpAuthAbortRequest(SceNpAuthRequestId id)
{
	if (!s_npAuthInitialized)
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	return npAuthIpcAbortRequest(SceShellSvc_B31E7F1C(), id);
}

int sceNpAuthTerm(void)
{
	int i;

	if (!s_npAuthInitialized)
		return 0;
	SceShellSvc_7CBF442B(NP_AUTH_EVENT);
	for (i = 0; i < NP_AUTH_REQUEST_MAX; i++) {
		if (s_npAuthRequests[i].id > 0) {
			sceNpAuthAbortRequest(s_npAuthRequests[i].id);
			sceNpAuthDestroyRequest(s_npAuthRequests[i].id);
		}
	}
	if (s_npAuthMutex > 0) {
		sceKernelDeleteMutex(s_npAuthMutex);
		s_npAuthMutex = -1;
	}
	s_npAuthInitialized = 0;
	return 0;
}

int sceNpAuthCreateStartRequest(const SceNpAuthRequestParameter *param)
{
	SceSize serviceIdSize;
	SceSize cookieSize;
	SceSize entitlementIdSize = 0;
	int ret;
	int i;

	if (!s_npAuthInitialized)
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	if (param == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (param->ticketCb == NULL || param->cookieSize > SCE_NP_COOKIE_MAX_SIZE)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = sceKernelLockMutex(s_npAuthMutex, 1, NULL);
	if (ret >= 0) {
		for (i = 0; i < NP_AUTH_REQUEST_MAX; i++) {
			if (s_npAuthRequests[i].callback == NULL)
				break;
		}
		if (i == NP_AUTH_REQUEST_MAX) {
			sceKernelUnlockMutex(s_npAuthMutex, 1);
			return SCE_NP_AUTH_ERROR_EBUSY;
		}
		serviceIdSize = 0;
		if (param->serviceId != NULL)
			serviceIdSize = sceClibStrnlen(param->serviceId, 0x7fffffff);
		cookieSize = 0;
		if (param->cookie != NULL)
			cookieSize = param->cookieSize;
		if (param->entitlementId != NULL)
			entitlementIdSize = sceClibStrnlen(param->entitlementId, 0x7fffffff);
		s_npAuthRequests[i].callback = param->ticketCb;
		s_npAuthRequests[i].arg = param->cbArg;
		ret = npAuthIpcCreateStartRequest(SceShellSvc_B31E7F1C(), param, serviceIdSize,
						  param->serviceId, cookieSize, param->cookie,
						  entitlementIdSize, param->entitlementId);
		if (ret >= 0)
			s_npAuthRequests[i].id = ret;
	}
	sceKernelUnlockMutex(s_npAuthMutex, 1);
	return ret;
}

int sceNpAuthGetTicket(SceNpAuthRequestId id, void *buf, SceSize len)
{
	SceSize offset;
	SceSize chunk;
	int size;
	int ret;
	int i;

	if (!s_npAuthInitialized)
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	if (buf == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (len == 0)
		return SCE_NP_AUTH_ERROR_EINVAL;
	ret = sceKernelLockMutex(s_npAuthMutex, 1, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < NP_AUTH_REQUEST_MAX; i++) {
		if (s_npAuthRequests[i].id == id)
			break;
	}
	if (i == NP_AUTH_REQUEST_MAX) {
		ret = SCE_NP_AUTH_ERROR_ESRCH;
	} else {
		size = s_npAuthRequests[i].result;
		if (size < 0) {
			ret = size;
		} else {
			if (len < size)
				size = len;
			for (offset = 0; size != 0; offset += chunk) {
				chunk = size;
				if (chunk >= NP_AUTH_TICKET_CHUNK_SIZE)
					chunk = NP_AUTH_TICKET_CHUNK_SIZE;
				ret = npAuthIpcGetTicket(SceShellSvc_B31E7F1C(), s_npAuthRequests[i].id,
							 (char *)buf + offset, offset, chunk);
				if (ret < 0)
					break;
				size -= chunk;
			}
		}
	}
	sceKernelUnlockMutex(s_npAuthMutex, 1);
	return ret;
}

/* the event of a finished request: each pending request's callback gets its result */
static int npAuthEventHandler(int event, void *arg)
{
	int result;
	int ret;
	int i;

	if (event != NP_AUTH_EVENT)
		return 0;
	ret = sceKernelLockMutex(s_npAuthMutex, 1, NULL);
	if (ret >= 0) {
		for (i = 0; i < NP_AUTH_REQUEST_MAX; i++) {
			if (s_npAuthRequests[i].callback == NULL)
				continue;
			ret = npAuthIpcGetResult(SceShellSvc_B31E7F1C(), s_npAuthRequests[i].id,
						 &result);
			if (ret < 0)
				continue;
			s_npAuthRequests[i].result = result;
			s_npAuthRequests[i].callback(s_npAuthRequests[i].id, result,
						     s_npAuthRequests[i].arg);
		}
	}
	sceKernelUnlockMutex(s_npAuthMutex, 1);
	return ret;
}

__attribute__((noinline))
static int npAuthIpcCreateStartRequest(struct ShellSvcInterface *svc,
				       const SceNpAuthRequestParameter *param,
				       SceSize serviceIdSize, const char *serviceId,
				       SceSize cookieSize, const void *cookie,
				       SceSize entitlementIdSize, const char *entitlementId)
{
	int result;
	int ret;
	SceSize sizes[3];
	struct ShellSvcBuffer in[5];

	result = 0;
	sizes[0] = serviceIdSize;
	in[0].address = sizes;
	in[0].size = sizeof(sizes);
	in[1].address = (void *)param;
	in[1].size = sizeof(*param);
	in[2].address = (void *)serviceId;
	in[2].size = serviceIdSize;
	sizes[1] = cookieSize;
	in[3].address = (void *)cookie;
	in[3].size = cookieSize;
	sizes[2] = entitlementIdSize;
	in[4].address = (void *)entitlementId;
	in[4].size = entitlementIdSize;
	ret = ((ShellSvcRequest)svc->methods[5])(svc, NP_AUTH_IPC_CREATE_START_REQUEST, in, 5,
						 &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npAuthIpcDestroyRequest(struct ShellSvcInterface *svc, SceNpAuthRequestId id)
{
	int result;
	int ret;
	SceNpAuthRequestId param;
	struct ShellSvcBuffer in[1];

	result = 0;
	param = id;
	in[0].address = &param;
	in[0].size = sizeof(param);
	ret = ((ShellSvcRequest)svc->methods[5])(svc, NP_AUTH_IPC_DESTROY_REQUEST, in, 1,
						 &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npAuthIpcAbortRequest(struct ShellSvcInterface *svc, SceNpAuthRequestId id)
{
	int result;
	int ret;
	SceNpAuthRequestId param;
	struct ShellSvcBuffer in[1];

	result = 0;
	param = id;
	in[0].address = &param;
	in[0].size = sizeof(param);
	ret = ((ShellSvcRequest)svc->methods[5])(svc, NP_AUTH_IPC_ABORT_REQUEST, in, 1,
						 &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npAuthIpcGetTicket(struct ShellSvcInterface *svc, SceNpAuthRequestId id, void *buf,
			      SceSize offset, SceSize size)
{
	int result;
	SceUInt32 param[3];
	struct ShellSvcBuffer in[1];
	struct ShellSvcBuffer out[1];
	int ret;

	result = 0;
	param[0] = id;
	in[0].address = param;
	in[0].size = sizeof(param);
	out[0].address = buf;
	out[0].size = NP_AUTH_TICKET_CHUNK_SIZE;
	param[1] = offset;
	param[2] = size;
	ret = ((ShellSvcRequest)svc->methods[5])(svc, NP_AUTH_IPC_GET_TICKET, in, 1, &result,
						 out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npAuthIpcGetResult(struct ShellSvcInterface *svc, SceNpAuthRequestId id,
			      int *result)
{
	int ipcResult;
	int ret;
	SceNpAuthRequestId param;
	struct ShellSvcBuffer in[1];
	struct ShellSvcBuffer out[1];

	ipcResult = 0;
	param = id;
	in[0].address = &param;
	in[0].size = sizeof(param);
	out[0].address = result;
	out[0].size = sizeof(*result);
	ret = ((ShellSvcRequest)svc->methods[5])(svc, NP_AUTH_IPC_GET_RESULT, in, 1, &ipcResult,
						 out, 1);
	if (ret != 0)
		return ret;
	return ipcResult;
}
