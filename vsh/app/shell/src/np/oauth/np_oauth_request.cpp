/*
 * np_oauth_request.cpp: NpOAuthRequest lifecycle, reference counting, abort tracking,
 * and scoped LW mutex lock guard for NpOAuthVsh.
 */
#include "np_oauth_internal.h"
#include <libkernel/libkernel.h>

NpOAuthScopedLwMutexLockBase::NpOAuthScopedLwMutexLockBase(SceKernelLwMutexWork *mutex)
	: m_mutex(mutex)
{
	sceKernelLockLwMutex(m_mutex, 1, NULL);
}

NpOAuthScopedLwMutexLockBase::~NpOAuthScopedLwMutexLockBase()
{
	sceKernelUnlockLwMutex(m_mutex, 1);
}

NpOAuthScopedLwMutexLock::NpOAuthScopedLwMutexLock(NpOAuthRequest *request)
	: NpOAuthScopedLwMutexLockBase(request->mutex)
{
}

NpOAuthScopedLwMutexLock::~NpOAuthScopedLwMutexLock()
{
}

int npOAuthRequestCreate(NpOAuthRequest **outRequest)
{
	if (outRequest == NULL)
		return NP_OAUTH_ERR_COMMON_INVALID_ARG;

	*outRequest = NULL;
	NpOAuthContext *ctx = npOAuthGetContext();
	NpOAuthAllocator *allocator = ctx->allocator;

	NpOAuthRequest *req = (NpOAuthRequest *)allocator->allocate(sizeof(NpOAuthRequest),
								    allocator->context);
	if (req == NULL)
		return NP_OAUTH_ERR_OUT_OF_MEMORY;

	sceClibMemset(req, 0, sizeof(NpOAuthRequest));
	req->httpTemplateId = ctx->httpTemplateId;

	SceKernelLwMutexWork *mutex = (SceKernelLwMutexWork *)allocator->allocate(
		sizeof(SceKernelLwMutexWork), allocator->context);
	int ret;
	if (mutex != NULL) {
		ret = sceKernelCreateLwMutex(mutex, "SceNpOAuthVshReqMutex",
					     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
		if (ret >= 0) {
			req->mutex = mutex;
			*outRequest = req;
			return ret;
		}
		allocator->deallocate(mutex, allocator->context);
	} else {
		ret = NP_OAUTH_ERR_OUT_OF_MEMORY;
	}

	allocator->deallocate(req, allocator->context);
	return ret;
}

void npOAuthRequestRelease(NpOAuthRequest *request)
{
	if (request == NULL)
		return;

	bool shouldFree = false;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	if (request->refCount != 0)
		--request->refCount;
	if (request->destroyPending == 1 && request->refCount == 0)
		shouldFree = true;
	sceKernelUnlockLwMutex(request->mutex, 1);

	if (shouldFree && request->mutex != NULL) {
		sceKernelDeleteLwMutex(request->mutex);
		NpOAuthAllocator *allocator = npOAuthGetContext()->allocator;
		allocator->deallocate(request->mutex, allocator->context);
		allocator->deallocate(request, allocator->context);
	}
}

int npOAuthRequestAcquire(NpOAuthRequest *request, NpOAuthRequest **outRequest)
{
	if (request == NULL || outRequest == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	*outRequest = NULL;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	*outRequest = request;
	sceKernelUnlockLwMutex(request->mutex, 1);
	return 0;
}

int npOAuthRequestAbort(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	int ret = 0;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	if (request->httpReqId != 0)
		ret = sceHttpAbortRequest(request->httpReqId);
	if (request->ticketReqId != 0)
		ret = SceNpManagerTicket_A30BF29A(request->ticketReqId);
	if (request->cslaReqId != 0)
		ret = npCslaAbortRequest(request->cslaReqId);
	request->aborted = 1;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return ret;
}

int npOAuthRequestDestroy(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestAbort(request);

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	request->destroyPending = 1;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return 0;
}

bool npOAuthRequestIsInUse(const NpOAuthRequest *request)
{
	if (request == NULL)
		return false;
	return request->inUse != 0;
}

void npOAuthRequestMarkInUse(NpOAuthRequest *request)
{
	if (request != NULL)
		request->inUse = 1;
}

int npOAuthRequestSetHttpReqId(NpOAuthRequest *request, int httpReqId)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	int ret = 0;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	if (request->httpReqId != 0) {
		ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
	} else if (request->aborted != 0) {
		ret = NP_OAUTH_ERR_ABORTED;
	} else {
		request->httpReqId = httpReqId;
	}
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return ret;
}

int npOAuthRequestClearHttpReqId(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	request->httpReqId = 0;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return 0;
}

int npOAuthRequestSetTicketReqId(NpOAuthRequest *request, int ticketReqId)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	int ret = 0;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	if (request->ticketReqId != 0) {
		ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
	} else if (request->aborted != 0) {
		ret = NP_OAUTH_ERR_ABORTED;
	} else {
		request->ticketReqId = ticketReqId;
	}
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return ret;
}

int npOAuthRequestClearTicketReqId(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	request->ticketReqId = 0;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return 0;
}

int npOAuthRequestSetCslaReqId(NpOAuthRequest *request, int cslaReqId)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	int ret = 0;
	sceKernelLockLwMutex(request->mutex, 1, NULL);
	if (request->cslaReqId != 0) {
		ret = NP_OAUTH_ERR_INVALID_ARGUMENT;
	} else if (request->aborted != 0) {
		ret = NP_OAUTH_ERR_ABORTED;
	} else {
		request->cslaReqId = cslaReqId;
	}
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return ret;
}

int npOAuthRequestClearCslaReqId(NpOAuthRequest *request)
{
	if (request == NULL)
		return NP_OAUTH_ERR_INVALID_ARGUMENT;

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	++request->refCount;
	sceKernelUnlockLwMutex(request->mutex, 1);

	sceKernelLockLwMutex(request->mutex, 1, NULL);
	request->cslaReqId = 0;
	sceKernelUnlockLwMutex(request->mutex, 1);

	npOAuthRequestRelease(request);
	return 0;
}
