/*
 * SceNpCommonPs4, NpAsm: the calls to the NpAsm service, and the client of the layer (its title id, the list of
 * requests and the token cache), of which there are two: the one the exports work on and a secondary one.
 */

#include "np_asm.h"

extern "C" {

extern const void *g_npAsmClientVtable[];	/* 0x8102aba8 */

struct NpAsmClient *g_npAsmClient;		/* 0x8102b08c */
struct NpAsmClient *g_npAsmSecondaryClient;	/* 0x8102b090 */
struct NpCommonPs4Allocator g_npAsmAllocator;	/* 0x8102ba00 */

/* the client's constructor, written out where Sony's compiler inlined it */
#define NP_ASM_CLIENT_CONSTRUCT(client, httpContext, notDevelopment) \
	do { \
		(client)->httpContextId = (httpContext); \
		(client)->vtable = NP_COMMON_PS4_VTABLE(g_npAsmClientVtable); \
		sceClibMemset((client)->titleId, 0, sizeof((client)->titleId)); \
		sceClibMemset((client)->titleSecret, 0, sizeof((client)->titleSecret)); \
		(client)->titleFromApplication = 0; \
		(client)->titleFromSystem = 0; \
		(client)->transport = NULL; \
		(client)->requests = NULL; \
		(client)->systemTitle = 0; \
		(client)->notDevelopmentMode = (notDevelopment); \
	} while (0)

static inline SceSize npAsmStringLength(const char *string)
{
	SceSize length;

	length = 0;
	if (*string != 0) {
		length = 0;
		do {
			string++;
			length++;
		} while (*string != 0);
	}
	return length;
}

/*SORT-START*/
int npAsmIpcTerminate(void *ipcClient)
{
	int result;
	int status;

	status = 0;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30000, NULL, 0, &status, NULL, 0);
	if (result == 0)
		result = status;
	return result;
}

int npAsmIpcCheck(void *ipcClient, void *pTitleId, void *serviceName, void *pValid, void *buffer, int size)
{
	int result;
	int status;
	int bufferSize;
	SceSize length;
	struct NpAsmIpcInBuffer in[3];
	struct NpAsmIpcOutBuffer out[2];

	status = 0;
	in[0].data = &bufferSize;
	in[0].size = sizeof(bufferSize);
	in[1].data = pTitleId;
	in[1].size = 16;
	in[2].data = serviceName;
	length = npAsmStringLength((const char *)serviceName);
	out[0].data = pValid;
	out[0].size = 1;
	in[2].size = length + 1;
	bufferSize = size;
	out[1].data = buffer;
	out[1].size = size;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30001, in, 3, &status, out, 2);
	if (result == 0)
		result = status;
	return result;
}

int npAsmIpcUpdate(void *ipcClient, void *pTitleId, void *serviceName, void *buffer)
{
	int result;
	int status;
	SceSize length;
	SceSize bufferLength;
	struct NpAsmIpcInBuffer in[3];

	status = 0;
	in[0].data = pTitleId;
	in[0].size = 16;
	in[1].data = serviceName;
	length = npAsmStringLength((const char *)serviceName);
	in[1].size = length + 1;
	in[2].data = buffer;
	bufferLength = npAsmStringLength((const char *)buffer);
	in[2].size = bufferLength + 1;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30002, in, 3, &status, NULL, 0);
	if (result == 0)
		result = status;
	return result;
}

int npAsmConnectionSetTitleId(void *ipcClient, const void *pTitleId)
{
	int result;
	int status;
	struct NpAsmIpcInBuffer in[1];

	status = 0;
	in[0].data = pTitleId;
	in[0].size = 16;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30003, in, 1, &status, NULL, 0);
	if (result == 0)
		result = status;
	return result;
}

int npAsmConnectionClearTitleId(void *ipcClient)
{
	int result;
	int status;

	status = 0;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30004, NULL, 0, &status, NULL, 0);
	if (result == 0)
		result = status;
	return result;
}

int npAsmConnectionQuery(void *ipcClient, int a1, int a2)
{
	int result;
	int status;
	int value;
	struct NpAsmIpcInBuffer in[1];
	struct NpAsmIpcOutBuffer out[1];

	status = 0;
	value = a1;
	in[0].data = &value;
	in[0].size = sizeof(value);
	out[0].data = (void *)a2;
	out[0].size = 16;
	result = ((int (*)(void *, int, const struct NpAsmIpcInBuffer *, int, int *, struct NpAsmIpcOutBuffer *, int))
		(*(const void *const *const *)ipcClient)[5])(ipcClient, 0x30005, in, 1, &status, out, 1);
	if (result == 0)
		result = status;
	return result;
}

struct NpAsmClient *npAsmClientTerm(struct NpAsmClient *client)
{
	client->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return client;
}

int npAsmClientDelete(struct NpAsmClient *client)
{
	client->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(client);
}

void npAsmClientNullsub(void)
{
}

int npAsmClientUnlock(struct NpAsmClient *client)
{
	int result;

	result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
	if (result < 0)
		return sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
	return result;
}

int npAsmClientLock(struct NpAsmClient *client)
{
	int result;

	result = SceNpCommonPs4_F5840D6E(&client->mutex);
	if (result < 0)
		return sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
	return result;
}

int npAsmClientDestroy(int unused)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	struct NpAsmRequest *next;
	int result;

	client = g_npAsmClient;
	if (client != NULL) {
		if (client->transport != NULL) {
			((void (*)(struct NpAsmTransport *))((const void *const *)client->transport->vtable)[1])(client->transport);
			g_npAsmClient->transport = NULL;
			client = g_npAsmClient;
		}
		result = SceNpCommonPs4_F5840D6E(&client->mutex);
		if (result < 0)
			sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
		request = client->requests;
		if (request != NULL) {
			do {
				next = npAsmRequestGetNext(request);
				npAsmRequestCancel(request);
				((void (*)(struct NpAsmRequest *))request->vtable[1])(request);
				request = next;
			} while (next != NULL);
		}
		result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
		if (result < 0)
			sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
		SceNpCommonPs4_29895EBB(&g_npAsmClient->mutex);
		if (g_npAsmClient != NULL)
			((void (*)(struct NpAsmClient *))((const void *const *)g_npAsmClient->vtable)[1])(g_npAsmClient);
		g_npAsmClient = NULL;
	}
	g_npAsmAllocator.allocate = NULL;
	g_npAsmAllocator.reallocate = NULL;
	g_npAsmAllocator.deallocate = NULL;
	g_npAsmAllocator.context = NULL;
	return 0;
}

int npAsmClientCreate(int param1, const int *pParam, int notDevelopmentMode)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmTransport *object;
	int result;

	if (g_npAsmClient != NULL) {
		result = NP_ASM_ERROR_ALREADY_CREATED;
	} else {
		g_npAsmAllocator.allocate = (void *(*)(SceSize, void *))pParam[0];
		client = NULL;
		transport = NULL;
		g_npAsmAllocator.deallocate = (void (*)(void *, void *))pParam[2];
		g_npAsmAllocator.reallocate = (void *(*)(void *, SceSize, void *))pParam[1];
		g_npAsmAllocator.context = (void *)pParam[3];
		object = (struct NpAsmTransport *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmClient), &g_npAsmAllocator);
		if (object != NULL) {
			NP_ASM_CLIENT_CONSTRUCT((struct NpAsmClient *)object, param1, notDevelopmentMode);
			client = (struct NpAsmClient *)object;
		}
		g_npAsmClient = client;
		if (client != NULL) {
			object = (struct NpAsmTransport *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmTransport), &g_npAsmAllocator);
			client = g_npAsmClient;
			if (object != NULL) {
				transport = npAsmTransportInit(object, g_npAsmClient);
				client = g_npAsmClient;
			}
			client->transport = transport;
			if (g_npAsmClient->transport != NULL) {
				result = SceNpCommonPs4_4858EFB6(&g_npAsmClient->mutex, "SceNpAsmClientLock", 1);
				if (result >= 0)
					return result;
			} else {
				result = NP_ASM_ERROR_OUT_OF_MEMORY;
			}
		} else {
			result = NP_ASM_ERROR_OUT_OF_MEMORY;
		}
	}
	npAsmClientDestroy(result);
	return result;
}

__attribute__((noinline)) struct NpAsmClient *npAsmClientGet(void)
{
	return g_npAsmClient;
}

int npAsmClientGetHttpContextId(struct NpAsmClient *client)
{
	return client->httpContextId;
}

struct NpCommonPs4Allocator *npAsmClientGetAllocator(struct NpAsmClient *client)
{
	return &g_npAsmAllocator;
}

__attribute__((noinline)) struct NpCommonPs4Allocator *npAsmClientGetRequestAllocator(struct NpAsmClient *client)
{
	return &g_npAsmAllocator;
}

struct NpAsmTransport *npAsmClientGetTransport(struct NpAsmClient *client)
{
	return client->transport;
}

void npAsmClientAddRequest(struct NpAsmClient *client, struct NpAsmRequest *request)
{
	int result;

	result = SceNpCommonPs4_F5840D6E(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
	if (client->requests != NULL)
		npAsmRequestSetNext(request, client->requests);
	client->requests = request;
	result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
}

void npAsmClientRemoveRequest(struct NpAsmClient *client, struct NpAsmRequest *request)
{
	struct NpAsmRequest *current;
	struct NpAsmRequest *previous;
	int result;

	result = SceNpCommonPs4_F5840D6E(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
	current = client->requests;
	previous = NULL;
	while (current != NULL) {
		if (current == request)
			break;
		previous = current;
		current = npAsmRequestGetNext(current);
	}
	if (current == NULL) {
		result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
		if (result < 0)
			sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
		return;
	}
	if (previous != NULL)
		npAsmRequestSetNext(previous, npAsmRequestGetNext(current));
	else
		client->requests = npAsmRequestGetNext(current);
	result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
}

struct NpAsmRequest *npAsmClientFindRequest(struct NpAsmClient *client, int requestId)
{
	struct NpAsmRequest *request;
	int result;

	result = SceNpCommonPs4_F5840D6E(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
	for (request = client->requests; request != NULL; request = npAsmRequestGetNext(request)) {
		if (npAsmRequestGetId(request) == requestId)
			break;
	}
	result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
	if (result < 0)
		sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
	return request;
}

int npAsmClientSetTitleId(struct NpAsmClient *client, const void *pTitleId, const void *pTitleSecret, SceUInt8 fromSystem)
{
	int system;

	system = fromSystem;
	if (pTitleId != NULL)
		sceClibMemcpy_safe(client->titleId, pTitleId, sizeof(client->titleId));
	if (pTitleSecret != NULL)
		sceClibMemcpy_safe(client->titleSecret, pTitleSecret, sizeof(client->titleSecret));
	if (system != 0) {
		client->titleFromSystem = 1;
		client->titleFromApplication = 0;
		return 0;
	} else {
		client->titleFromSystem = 0;
		client->titleFromApplication = 1;
		return 1;
	}
}

int npAsmClientIsNotDevelopmentMode(struct NpAsmClient *client)
{
	return client->notDevelopmentMode;
}

int npAsmClientIsSystemTitle(struct NpAsmClient *client)
{
	return client->systemTitle;
}

int npAsmClientCheckTitleId(struct NpAsmClient *client, const char *pTitleId)
{
	int result;
	int pid;
	SceSize length;
	SceSize systemLength;
	char appTitleId[12];

	result = 0;
	sceClibMemset(appTitleId, 0, 10);
	if (SceLibKernel_9F793F84() >= 0x3500000 && client->systemTitle == 0) {
		pid = sceKernelGetProcessId();
		result = sceAppMgrAppParamGetString(pid, 12, appTitleId, 10);
		if (result >= 0) {
			length = sceClibStrnlen(appTitleId, 0xFFFFFFFF);
			if (length != sceClibStrnlen("NPXS10999", 0xFFFFFFFF)
				|| (systemLength = sceClibStrnlen("NPXS10999", 0xFFFFFFFF),
					sceClibStrncmp(appTitleId, "NPXS10999", systemLength) != 0)) {
				if (sceClibStrncmp(appTitleId, pTitleId, 9) != 0)
					result = NP_ASM_ERROR_80552A0B;
			}
		}
	}
	return result;
}

int npAsmClientGetTitleId(struct NpAsmClient *client, void *pTitleId, void *pTitleSecret, SceUInt8 check)
{
	int result;
	int verify;
	char titleId[16];
	SceUInt8 titleSecret[128];

	result = 0;
	verify = check;
	sceClibMemset(titleId, 0, sizeof(titleId));
	sceClibMemset(titleSecret, 0, sizeof(titleSecret));
	if (client->systemTitle != 0) {
		if (client->titleFromApplication == 0) {
			result = NP_ASM_ERROR_80559D4A;
			goto done;
		}
		sceClibMemcpy_safe(titleId, client->titleId, sizeof(titleId));
		sceClibMemcpy_safe(titleSecret, client->titleSecret, sizeof(titleSecret));
	} else if (client->notDevelopmentMode != 0 || SceNpCommonPs4_64C85FD5() != 0 || client->titleFromApplication == 0) {
		if (client->titleFromSystem != 0) {
			sceClibMemcpy_safe(titleId, client->titleId, sizeof(titleId));
			sceClibMemcpy_safe(titleSecret, client->titleSecret, sizeof(titleSecret));
			if (verify != 0) {
				result = npAsmClientCheckTitleId(client, titleId);
				if (result < 0)
					goto done;
			}
		} else {
			result = npAsmGetSystemTitleId(titleId, titleSecret);
			if (result < 0)
				goto done;
			if (verify != 0) {
				result = npAsmClientCheckTitleId(client, titleId);
				if (result < 0)
					goto done;
			}
			sceClibMemcpy_safe(client->titleId, titleId, sizeof(titleId));
			sceClibMemcpy_safe(client->titleSecret, titleSecret, sizeof(titleSecret));
			client->titleFromApplication = 0;
			client->titleFromSystem = 1;
			if (SceLibKernel_9F793F84() >= 0x3500000) {
				result = npAsmWrapConnectionSetTitleId(titleId);
				if (result < 0) {
					sceClibMemset(client->titleId, 0, sizeof(client->titleId));
					sceClibMemset(client->titleSecret, 0, sizeof(client->titleSecret));
					client->titleFromApplication = 0;
					client->titleFromSystem = 0;
					goto done;
				}
			}
		}
	} else {
		sceClibMemcpy_safe(titleId, client->titleId, sizeof(titleId));
		sceClibMemcpy_safe(titleSecret, client->titleSecret, sizeof(titleSecret));
		if (verify != 0) {
			result = npAsmClientCheckTitleId(client, titleId);
			if (result < 0)
				goto done;
		}
	}
	if (pTitleId != NULL)
		sceClibMemcpy_safe(pTitleId, titleId, sizeof(titleId));
	if (pTitleSecret != NULL)
		sceClibMemcpy_safe(pTitleSecret, titleSecret, sizeof(titleSecret));
done:
	return result;
}

int npAsmSecondaryClientCreate(void)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmClient *object;
	struct NpAsmTransport *transportObject;
	int result;

	result = 0;
	if (g_npAsmAllocator.allocate != NULL) {
		if (g_npAsmSecondaryClient != NULL)
			return result;
		client = NULL;
		object = (struct NpAsmClient *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmClient), &g_npAsmAllocator);
		if (object != NULL) {
			NP_ASM_CLIENT_CONSTRUCT(object, 0, 0);
			client = object;
		}
		g_npAsmSecondaryClient = client;
		if (client != NULL) {
			transportObject = (struct NpAsmTransport *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmTransport), &g_npAsmAllocator);
			client = g_npAsmSecondaryClient;
			transport = NULL;
			if (transportObject != NULL) {
				transport = npAsmTransportInit(transportObject, g_npAsmSecondaryClient);
				client = g_npAsmSecondaryClient;
			}
			client->transport = transport;
			if (g_npAsmSecondaryClient->transport != NULL) {
				result = SceNpCommonPs4_4858EFB6(&g_npAsmSecondaryClient->mutex, "SceNpAsmClientLock", 1);
				if (result >= 0)
					return result;
			} else {
				result = NP_ASM_ERROR_OUT_OF_MEMORY;
			}
		} else {
			result = NP_ASM_ERROR_OUT_OF_MEMORY;
		}
	} else {
		result = NP_ASM_ERROR_NOT_INITIALIZED;
	}
	if (g_npAsmAllocator.allocate != NULL) {
		client = g_npAsmSecondaryClient;
		if (client != NULL) {
			transport = client->transport;
			if (transport != NULL) {
				((void (*)(struct NpAsmTransport *))((const void *const *)transport->vtable)[1])(transport);
				client = g_npAsmSecondaryClient;
			}
			if (client != NULL)
				((void (*)(struct NpAsmClient *))((const void *const *)client->vtable)[1])(client);
			g_npAsmSecondaryClient = NULL;
		}
	}
	return result;
}

int npAsmSecondaryClientDestroy(void)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	struct NpAsmRequest *next;
	int result;

	if (g_npAsmAllocator.deallocate == NULL)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	client = g_npAsmSecondaryClient;
	if (client != NULL) {
		if (client->transport != NULL) {
			((void (*)(struct NpAsmTransport *))((const void *const *)client->transport->vtable)[1])(client->transport);
			g_npAsmSecondaryClient->transport = NULL;
			client = g_npAsmSecondaryClient;
		}
		result = SceNpCommonPs4_F5840D6E(&client->mutex);
		if (result < 0)
			sceClibPrintf("sceNpLwMutexLock() failed. ret = 0x%x\n", result);
		request = client->requests;
		if (request != NULL) {
			do {
				next = npAsmRequestGetNext(request);
				npAsmRequestCancel(request);
				((void (*)(struct NpAsmRequest *))request->vtable[1])(request);
				request = next;
			} while (next != NULL);
		}
		result = SceNpCommonPs4_4A1C1C3C(&client->mutex);
		if (result < 0)
			sceClibPrintf("sceNpLwMutexUnlock() failed. ret = 0x%x\n", result);
		SceNpCommonPs4_29895EBB(&g_npAsmSecondaryClient->mutex);
		if (g_npAsmSecondaryClient != NULL)
			((void (*)(struct NpAsmClient *))((const void *const *)g_npAsmSecondaryClient->vtable)[1])(g_npAsmSecondaryClient);
		g_npAsmSecondaryClient = NULL;
	}
	return 0;
}

__attribute__((noinline)) struct NpAsmClient *npAsmClientGetSecondary(void)
{
	return g_npAsmSecondaryClient;
}

int npAsmClientSetSystemTitle(struct NpAsmClient *client, int systemTitle)
{
	client->systemTitle = systemTitle;
	return (int)client;
}
/*SORT-END*/

const void *g_npAsmClientVtable[] = {		/* 0x8102aba8 */
	NULL,
	NULL,
	(const void *)npAsmClientTerm,
	(const void *)npAsmClientDelete,
};

}
