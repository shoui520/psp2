/*
 * SceNpCommonPs4, NpAsm: the exports that forward to the client, behind the flag that says the
 * layer is initialised.
 */

#include <appmgr.h>
#include "np_asm.h"

extern "C" {

struct NpAsmRegistryEntry g_npAsmRegistryTable[128];	/* 0x8102b170 */
struct NpAsmRegistryEntry *g_npAsmRegistryHead;		/* 0x8102b074 */
SceKernelLwMutexWork g_npAsmRegistryMutex;		/* 0x8102b970 */

/* the secret of the system title NPXS10097_00 that the secondary client signs its requests with */
const unsigned char g_npAsmSystemTitleSecret[NP_ASM_TITLE_SECRET_SIZE] = {	/* 0x81028e5c */
	0x13, 0x30, 0xa5, 0x14, 0xc6, 0xfa, 0x34, 0xf6,
	0x93, 0x11, 0x30, 0xf6, 0x6b, 0xb2, 0x23, 0x58,
	0x9e, 0x96, 0x85, 0xe1, 0xf4, 0x7d, 0x9d, 0x17,
	0xec, 0x9f, 0x47, 0xdb, 0x37, 0x7c, 0x50, 0x8a,
	0x45, 0xf3, 0xb1, 0xc5, 0xfb, 0x30, 0xc4, 0x2d,
	0x34, 0x02, 0xe8, 0x64, 0x2c, 0x84, 0x18, 0xab,
	0x9f, 0x9c, 0x72, 0x18, 0x70, 0x58, 0xf4, 0x91,
	0xe7, 0x0a, 0x0d, 0xf5, 0xc5, 0x5b, 0x3c, 0xcd,
	0x8e, 0x1d, 0x77, 0x9a, 0xff, 0x82, 0xa0, 0x26,
	0xca, 0xbe, 0xb6, 0xba, 0x6f, 0xc6, 0x70, 0xe4,
	0x4a, 0x2e, 0x06, 0xb6, 0xc6, 0x5f, 0xd5, 0x2b,
	0x4e, 0x18, 0x6c, 0x55, 0x1e, 0xa4, 0x64, 0xdf,
	0x19, 0xd0, 0x89, 0x20, 0xd2, 0x6b, 0x36, 0x83,
	0xff, 0xe0, 0xba, 0x0a, 0x5e, 0x12, 0xb0, 0x82,
	0x5b, 0x76, 0xb7, 0x79, 0x34, 0x4e, 0xc9, 0x6f,
	0xb1, 0xcb, 0xc8, 0x8a, 0xa4, 0xab, 0x09, 0x4e,
};

SceUInt8 g_npAsmWrapInitialized;				/* 0x8102b078 */

/*SORT-START*/
int npAsmRegistryAdd(void *object, void *callback, const int *pExtra)
{
	struct NpAsmRegistryEntry *scan;
	struct NpAsmRegistryEntry *entry;
	int i;

	sceKernelLockLwMutex(&g_npAsmRegistryMutex, 1, NULL);
	scan = g_npAsmRegistryTable;
	i = 0;
	while (scan->callback != NULL) {
		++i;
		++scan;
		if (i >= 128)
			goto allocate;
	}
	entry = scan;
	if (entry == NULL) {
allocate:
		entry = (struct NpAsmRegistryEntry *)operator new(sizeof(struct NpAsmRegistryEntry));
		if (entry == NULL) {
			sceKernelLockLwMutex(&g_npAsmRegistryMutex, 1, NULL);
			return 1;
		}
	}
	entry->callback = callback;
	entry->next = g_npAsmRegistryHead;
	entry->object = object;
	entry->extra = *pExtra;
	g_npAsmRegistryHead = entry;
	sceKernelUnlockLwMutex(&g_npAsmRegistryMutex, 1);
	return 0;
}

void npAsmWrapSetInitialized(SceUInt8 value)
{
	g_npAsmWrapInitialized = value;
}

SceUInt8 npAsmWrapIsInitialized(void)
{
	return g_npAsmWrapInitialized;
}

/* the errors the NpManager reports for an expired or rejected login make it drop its own state */

int SceNpCommonPs4_5492CC76(int param1, const int *pParam)
{
	int result;

	if (g_npAsmWrapInitialized != 0)
		return NP_ASM_ERROR_ALREADY_INITIALIZED;
	if (pParam == NULL)
		return NP_ASM_ERROR_INVALID_ARGUMENT;
	result = npAsmWrapInit(param1, pParam);
	if (result >= 0)
		g_npAsmWrapInitialized = 1;
	return result;
}

int SceNpCommonPs4_F2D6BA27(void)
{
	int result;

	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	result = npAsmWrapTerm();
	g_npAsmWrapInitialized = 0;
	return result;
}

int SceNpCommonPs4_4E897A97(const void *pTitleId, const void *pTitleSecret)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pTitleId != NULL && pTitleSecret != NULL)
		return npAsmSetTitleId(pTitleId, pTitleSecret, 0);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_9AFE6E37(const void *pTitleId, const void *pTitleSecret)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pTitleId != NULL && pTitleSecret != NULL)
		return npAsmSetTitleId(pTitleId, pTitleSecret, 1);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_17EB36C2(void *pTitleId, void *pTitleSecret)
{
	if (g_npAsmWrapInitialized != 0)
		return npAsmGetTitleId(pTitleId, pTitleSecret);
	return NP_ASM_ERROR_NOT_INITIALIZED;
}

int SceNpCommonPs4_6AC5EF70(void *pTitleId, void *pTitleSecret)
{
	if (g_npAsmWrapInitialized != 0)
		return npAsmGetTitleIdUnchecked(pTitleId, pTitleSecret);
	return NP_ASM_ERROR_NOT_INITIALIZED;
}

int SceNpCommonPs4_639086A3(const void *pParam)
{
	if (g_npAsmWrapInitialized != 0)
		return npAsmRequestCreate(pParam);
	return NP_ASM_ERROR_NOT_INITIALIZED;
}

int SceNpCommonPs4_530CAFDA(int requestId)
{
	if (g_npAsmWrapInitialized != 0)
		return npAsmRequestDestroy(requestId);
	return NP_ASM_ERROR_NOT_INITIALIZED;
}

int SceNpCommonPs4_13798894(SceInt32 requestId, const char *onlineId, void *buffer, SceUInt8 flag)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (onlineId != NULL)
		return npAsmRequestGetTitleToken(requestId, onlineId, buffer, flag);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int npAsmWrapGetTokenLimitation(void *pExpired, void *pTick)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pExpired != NULL && pTick != NULL)
		return npAsmGetTokenLimitation(pExpired, pTick);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_DA99BBD7(int requestId, const void *pOnlineId, void *serviceName, void *buffer, int size)
{
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];

	if (g_npAsmWrapInitialized != 0) {
		if (pOnlineId != NULL && serviceName != NULL) {
			sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
			result = npAsmGetTitleId(titleId, NULL);
			if (result >= 0)
				result = npAsmRequestGetBaseUrlChecked(requestId, pOnlineId, titleId, serviceName, buffer, size);
		} else {
			result = NP_ASM_ERROR_INVALID_ARGUMENT;
		}
	} else {
		result = NP_ASM_ERROR_NOT_INITIALIZED;
	}
	return result;
}

int SceNpCommonPs4_6E443E9B(int requestId, const void *pOnlineId, const char *npComId, const char *titleId,
	const unsigned char *titleSecret, int serviceLabel, void *pOut)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pOnlineId != NULL && npComId != NULL && titleId != NULL)
		return npAsmRequestGetNpComInfo(requestId, pOnlineId, npComId, titleId, titleSecret, serviceLabel, pOut);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_B6CD5BEB(int requestId, const void *pOnlineId, const char *serviceName, const char *id, unsigned int *pCount)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pOnlineId != NULL && id != NULL && serviceName != NULL)
		return npAsmRequestGetRelatedTitleIds(requestId, pOnlineId, serviceName, id, pCount);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_E2FC8250(int requestId, void *buffer, unsigned int count)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (buffer != NULL && count != 0)
		return npAsmRequestCopyRelatedTitleIds(requestId, buffer, count);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int npAsmWrapRequestGetMaxAge(int requestId, int *pMaxAge)
{
	if (g_npAsmWrapInitialized == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (pMaxAge != NULL)
		return npAsmRequestGetMaxAge(requestId, pMaxAge);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_68D08C92(SceInt32 requestId)
{
	return npAsmRequestAbort(requestId);
}

__attribute__((noinline)) void npAsmNotifyServiceError(int result, const void *pOnlineId)
{
	int code;

	if ((result & 0xFF000000) == 0x82000000 && pOnlineId != NULL) {
		if ((result & 0xFFF00000) == 0x82200000) {
			code = result & 0xFFF;
			if (code == 513 || code == 514 || code == 525) {
				SceNpManager_F66A7AAE();
				return;
			}
		}
		if ((result & 0xFFF00000) == 0x82300000) {
			if ((result & 0xFFF) == 3858)
				SceNpManager_F66A7AAE();
		}
	}
}

int npAsmIsSystemApp(void)
{
	unsigned char status[128];

	return sceAppMgrGetStatusByName(NULL, status) >= 0 && *(int *)&status[48] == NP_ASM_SYSTEM_APP_ID;
}

static int npAsmIsSystemAppInline(void)
{
	unsigned char status[128];

	return sceAppMgrGetStatusByName(NULL, status) >= 0 && *(int *)&status[48] == NP_ASM_SYSTEM_APP_ID;
}

__attribute__((noinline)) int npAsmWrapInit(int param1, const int *pParam)
{
	int result;
	int next;
	int notDevelopmentMode;

	result = npAsmConnectionInit();
	if (result >= 0) {
		if (npAsmIsSystemAppInline() || (next = SceNpCommonPs4_D84BF4B7(2), result = next, next >= 0)) {
			notDevelopmentMode = sceAppMgrIsDevelopmentMode() == 0;
			result = npAsmClientCreate(param1, pParam, notDevelopmentMode);
			if (result >= 0)
				return 0;
			if (!npAsmIsSystemAppInline())
				next = SceNpCommonPs4_1096EAD4(2);
			else
				next = NP_ASM_SYSTEM_APP_ID;
		}
		npAsmConnectionTerm();
	}
	return result;
}

__attribute__((noinline)) int npAsmWrapTerm(void)
{
	int next;

	if (!npAsmIsSystemAppInline())
		next = SceNpCommonPs4_1096EAD4(2);
	else
		next = NP_ASM_SYSTEM_APP_ID;
	next = npAsmClientDestroy(next);
	npAsmConnectionTerm();
	return 0;
}

int npAsmWrapConnectionSetTitleId(const void *pTitleId)
{
	int result;

	result = 0;
	if (!npAsmIsSystemAppInline())
		result = npAsmConnectionSetTitleId(SceNpCommonPs4_A0599F24(), pTitleId);
	return result;
}

int npAsmWrapConnectionClearTitleId(void)
{
	int result;

	result = 0;
	if (!npAsmIsSystemAppInline())
		result = npAsmConnectionClearTitleId(SceNpCommonPs4_A0599F24());
	return result;
}

int npAsmWrapConnectionQuery(int a1, int a2)
{
	return npAsmConnectionQuery(SceNpCommonPs4_A0599F24(), a1, a2);
}

__attribute__((noinline)) int npAsmSetTitleId(const void *pTitleId, const void *pTitleSecret, int a3)
{
	struct NpAsmClient *client;
	int result;

	result = 0;
	client = npAsmClientGet();
	if (SceLibKernel_9F793F84() < 0x3500000 || npAsmClientIsNotDevelopmentMode(client) != 0 || SceNpCommonPs4_64C85FD5() != 0
		|| ((!npAsmIsSystemAppInline() ? (result = npAsmConnectionSetTitleId(SceNpCommonPs4_A0599F24(), pTitleId)) : 0), result >= 0)) {
		npAsmClientSetSystemTitle(client, a3);
		npAsmClientSetTitleId(client, pTitleId, pTitleSecret, 0);
	}
	return result;
}
/* the NpManager is told when the login of the title's user was refused (what npAsmNotifyServiceError does, inline) */
#define NP_ASM_NOTIFY_SERVICE_ERROR(result, pOnlineId) \
	do { \
		if (((result) & 0xFF000000) == 0x82000000 && (pOnlineId) != NULL \
			&& ((((result) & 0xFFF00000) == 0x82200000 \
				&& (((result) & 0xFFF) == 513 || ((result) & 0xFFF) == 514 || ((result) & 0xFFF) == 525)) \
				|| (((result) & 0xFFF00000) == 0x82300000 && ((result) & 0xFFF) == 0xF12))) \
			SceNpManager_F66A7AAE(); \
	} while (0)

/* the NpManager and connection steps every call of a request shares: take the login the request waits on and fetch its
   access token, then give the login back and open the HTTP connection the call runs on */
#define NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, abortedLabel) \
	do { \
		(result) = SceNpManager_4110B787(&(npmId)); \
		if ((result) >= 0) { \
			npAsmClientLock(client); \
			npAsmRequestSetNpManagerId(request, npmId); \
			if (npAsmRequestIsAborted(request) != 0) { \
				(result) = NP_ASM_ERROR_ABORTED; \
				npAsmClientUnlock(client); \
				goto abortedLabel; \
			} \
			npAsmClientUnlock(client); \
			(result) = SceNpManager_FDE78C74(npmId, accessToken); \
		} \
	} while (0)

#define NP_ASM_FINISH_REQUEST(client, request, result) \
	do { \
		if ((request) != NULL) { \
			npAsmClientLock(client); \
			if (npAsmRequestIsAborted(request) != 0) \
				(result) = NP_ASM_ERROR_ABORTED; \
			if (npAsmRequestGetConnection(request) != NULL) \
				npAsmConnectionDestroy(npAsmRequestGetConnection(request)); \
			if (npAsmRequestGetNpManagerId(request) != 0) \
				SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request)); \
			npAsmClientUnlock(client); \
		} \
	} while (0)

int npAsmGetSystemTitleId(void *pTitleId, void *pTitleSecret)
{
	struct NpAsmSystemData data;
	char magic[4] = { 'N', 'P', 'T', 'D' };
	int result;
	SceSize length;

	sceClibMemset(&data, 0, sizeof(data));
	result = sceAppMgrGetSystemDataFile(4, &data, sizeof(data));
	if (result >= 0) {
		result = 0;
		if (sceClibMemcmp(&data, magic, 4) != 0) {
			result = NP_ASM_ERROR_80559D4D;
		} else {
			data.magic = sceNetNtohl(data.magic);
			if (sceNetNtohl(data.reserved0) != 0 || sceNetNtohl(data.reserved1) != 0) {
				result = NP_ASM_ERROR_80559D4D;
			} else if (sceNetNtohl(data.dataSize) == NP_ASM_TITLE_SECRET_SIZE) {
				data.dataSize = NP_ASM_TITLE_SECRET_SIZE;
				sceClibMemset(pTitleId, 0, NP_ASM_TITLE_ID_SIZE);
				sceClibMemcpy_safe(pTitleId, data.titleId, 12);
				sceClibMemset(pTitleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
				length = NP_ASM_TITLE_SECRET_SIZE;
				if (data.dataSize <= NP_ASM_TITLE_SECRET_SIZE)
					length = data.dataSize;
				sceClibMemcpy_safe(pTitleSecret, data.titleSecret, length);
			} else {
				result = NP_ASM_ERROR_80559D4D;
			}
		}
	} else if (result == -2147418110) {
		result = NP_ASM_ERROR_80552A0A;
	}
	return result;
}

__attribute__((noinline)) int npAsmGetTitleId(void *pTitleId, void *pTitleSecret)
{
	return npAsmClientGetTitleId(npAsmClientGet(), pTitleId, pTitleSecret, 1);
}

__attribute__((noinline)) int npAsmGetTitleIdUnchecked(void *pTitleId, void *pTitleSecret)
{
	return npAsmClientGetTitleId(npAsmClientGet(), pTitleId, pTitleSecret, 0);
}

int npAsmRequestCreate(const void *pParam)
{
	struct NpAsmClient *client;
	struct NpCommonPs4Allocator *allocator;
	struct NpAsmRequest *request;

	client = npAsmClientGet();
	if (pParam == NULL || (allocator = ((const struct NpAsmRequestParam *)pParam)->allocator) == NULL)
		allocator = npAsmClientGetAllocator(client);
	request = (struct NpAsmRequest *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmRequest), allocator);
	if (request == NULL || (request = npAsmRequestInit(request, (const struct NpAsmRequestParam *)pParam)) == NULL)
		return NP_ASM_ERROR_OUT_OF_MEMORY;
	npAsmClientAddRequest(client, request);
	return npAsmRequestGetId(request);
}

int npAsmRequestDestroy(int requestId)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	struct NpCommonPs4Allocator *allocator;
	int result;

	result = 0;
	client = npAsmClientGet();
	npAsmClientLock(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request != NULL) {
		if (npAsmRequestGetParam(request)->allocator != NULL)
			allocator = npAsmRequestGetParam(request)->allocator;
		else
			allocator = npAsmClientGetAllocator(client);
		npAsmClientRemoveRequest(client, request);
		npAsmRequestCancel(request);
		npAsmRequestFreeResults(request, allocator);
		((void (*)(struct NpAsmRequest *))request->vtable[1])(request);
	} else {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
	}
	npAsmClientUnlock(client);
	return result;
}

int npAsmRequestGetTitleToken(SceInt32 requestId, const char *onlineId, void *buffer, SceUInt8 flag)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	int result;

	client = npAsmClientGet();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL)
		return NP_ASM_ERROR_REQUEST_NOT_FOUND;
	result = npAsmTransportGetTitleToken(transport, request, onlineId, buffer, flag);
	NP_ASM_NOTIFY_SERVICE_ERROR(result, onlineId);
	return result;
}

__attribute__((noinline)) int npAsmGetTokenLimitation(void *pExpired, void *pTick)
{
	struct NpAsmClient *client;

	client = npAsmClientGet();
	return npAsmTransportGetTokenLimitation(npAsmClientGetTransport(client), pExpired, pTick);
}

int npAsmCallbackUpdate(void *pTitleId, void *serviceName, void *buffer)
{
	return npAsmCallbackTableUpdate(npAsmGetCallbackTable(), pTitleId, serviceName, buffer);
}

int npAsmRequestGetBaseUrl(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	int npmId;
	void *connection;
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	client = npAsmClientGet();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL) {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		goto finish;
	}
	result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
	if (result >= 0) {
		NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
		if (result >= 0) {
			npAsmClientLock(client);
			SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
			npAsmRequestSetNpManagerId(request, 0);
			npAsmClientUnlock(client);
			result = npAsmConnectionCreate(
				npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
					: npAsmClientGetHttpContextId(client),
				npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
					: npAsmClientGetAllocator(client),
				&connection);
			if (result >= 0) {
				if (pTitleId == NULL) {
					sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
					sceClibMemset(titleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
					result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
					if (result < 0)
						goto finish;
					pTitleId = titleId;
				}
				npAsmClientLock(client);
				npAsmRequestSetConnection(request, connection);
				if (npAsmRequestIsAborted(request) != 0) {
					result = NP_ASM_ERROR_ABORTED;
					npAsmClientUnlock(client);
					goto finish;
				}
				npAsmClientUnlock(client);
				result = npAsmConnectionGetBaseUrl(npAsmRequestGetConnection(request), pTitleId, serviceName, accessToken,
					tokenEntry, buffer, size);
				if (result >= 0) {
					npAsmClientLock(client);
					npAsmConnectionDestroy(npAsmRequestGetConnection(request));
					npAsmRequestSetConnection(request, NULL);
					npAsmClientUnlock(client);
				}
			}
		}
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	NP_ASM_NOTIFY_SERVICE_ERROR(result, pOnlineId);
	return result;
}

int npAsmCallbackCheck(void *pTitleId, void *serviceName, void *pValid, void *buffer, int size)
{
	return npAsmCallbackTableCheck(npAsmGetCallbackTable(), pTitleId, serviceName, pValid, buffer, size);
}

int npAsmRequestGetBaseUrlChecked(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size)
{
	int result;
	SceUInt8 valid;

	valid = 0;
	result = npAsmCallbackTableCheck(npAsmGetCallbackTable(), pTitleId, serviceName, &valid, buffer, size);
	if (result >= 0 && valid == 0) {
		result = npAsmRequestGetBaseUrl(requestId, pOnlineId, pTitleId, serviceName, buffer, size);
		if (result >= 0)
			result = npAsmCallbackTableUpdate(npAsmGetCallbackTable(), pTitleId, serviceName, buffer);
	}
	return result;
}

/* decrypts the pass phrase of an npComInfo entry (AES-256-CBC with the title's secret as the key and the entry's IV) */
int npAsmDecryptNpComInfo(const struct NpAsmNpComInfo *entry, const void *key, struct NpAsmNpComInfoPlain *plain)
{
	unsigned char decrypted[512];
	struct NpCommonPs4AesContext aes;
	unsigned char block[36];
	unsigned int i;
	unsigned int j;
	int result;

	result = npCommonPs4AesInit(&aes, 128, 256, key);
	if (result >= 0) {
		for (j = 0; j < aes.blockWords * 4; j++)
			aes.iv[j] = entry->iv[j];
		result = 0;
	}
	if (result >= 0) {
		if ((entry->passPhraseSize & 0xF) != 0) {
			result = NP_ASM_ERROR_INVALID_ARGUMENT;
		} else {
			for (i = 0; i < entry->passPhraseSize; i += 16) {
				npCommonPs4AesDecryptBlock(&aes, &entry->passPhrase[i], block);
				for (j = 0; j < aes.blockWords * 4; j++) {
					decrypted[i + j] = block[j] ^ aes.iv[j];
					aes.iv[j] = entry->passPhrase[i + j];
				}
			}
			sceClibMemcpy_safe(plain->passPhrase, decrypted, 0x80);
		}
	}
	return result;
}

int npAsmRequestGetNpComInfo(int requestId, const void *pOnlineId, const char *npComId, const char *titleId,
	const unsigned char *titleSecret, int serviceLabel, void *pOut)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	struct NpAsmNpComInfo *list;
	unsigned int count;
	struct NpCommonPs4Allocator *allocator;
	const unsigned char *key;
	unsigned int i;
	int npmId;
	void *connection;
	int result;
	unsigned char localSecret[NP_ASM_TITLE_SECRET_SIZE];
	struct NpAsmNpComInfoPlain plain;
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	list = NULL;
	count = 0;
	client = npAsmClientGet();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request != NULL) {
		sceClibMemset(localSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
		result = npAsmClientGetTitleId(client, NULL, localSecret, 1);
		allocator = NULL;
		if (result >= 0) {
			result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
			allocator = NULL;
			if (result >= 0) {
				NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
				allocator = NULL;
				if (result >= 0) {
					npAsmClientLock(client);
					SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
					npAsmRequestSetNpManagerId(request, 0);
					npAsmClientUnlock(client);
					allocator = npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
						: npAsmClientGetRequestAllocator(client);
					result = npAsmConnectionCreate(
						npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
							: npAsmClientGetHttpContextId(client),
						allocator, &connection);
					if (result >= 0) {
						npAsmClientLock(client);
						npAsmRequestSetConnection(request, connection);
						if (npAsmRequestIsAborted(request) != 0) {
							result = NP_ASM_ERROR_ABORTED;
							npAsmClientUnlock(client);
							goto finish;
						}
						npAsmClientUnlock(client);
						result = npAsmConnectionGetNpComInfo(npAsmRequestGetConnection(request), (const char *)titleId, npComId, accessToken,
							tokenEntry, &list, &count);
						if (result >= 0) {
							if (list != NULL && count != 0) {
								for (i = 0; i < count; i++) {
									if (list[i].serviceLabel == (SceUInt32)serviceLabel)
										break;
								}
								if (i == count) {
									result = NP_ASM_ERROR_80559D49;
								} else {
									sceClibMemset(&plain, 0, sizeof(plain));
									key = titleSecret;
									if (key == NULL)
										key = localSecret;
									result = npAsmDecryptNpComInfo(&list[i], key, &plain);
									if (result >= 0) {
										sceClibMemcpy_safe(plain.npComId, list[i].npComId, 12);
										plain.serviceLabel = list[i].serviceLabel;
										if (pOut != NULL)
											sceClibMemcpy_safe(pOut, &plain, sizeof(plain));
										npAsmClientLock(client);
										npAsmConnectionDestroy(npAsmRequestGetConnection(request));
										npAsmRequestSetConnection(request, NULL);
										npAsmClientUnlock(client);
									}
								}
							} else {
								result = NP_ASM_ERROR_80559D4F;
							}
						}
					}
				}
			}
		}
	} else {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		allocator = NULL;
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	if (list != NULL && allocator != NULL)
		SceNpCommonPs4_6B428829(allocator, list);
	NP_ASM_NOTIFY_SERVICE_ERROR(result, pOnlineId);
	return result;
}

int npAsmRequestParseCacheControl(struct NpAsmRequest *request)
{
	struct NpAsmClient *client;
	char *header;
	char *scan;
	unsigned int size;
	unsigned int i;
	int result;

	header = NULL;
	size = 0;
	client = npAsmClientGet();
	result = npAsmConnectionGetHeaderSize(npAsmRequestGetConnection(request), "Cache-Control", &size);
	if (result >= 0) {
		header = (char *)SceNpCommonPs4_AA980113(npAsmClientGetAllocator(client), size + 1);
		if (header == NULL) {
			result = NP_ASM_ERROR_OUT_OF_MEMORY;
		} else {
			sceClibMemset(header, 0, size + 1);
			result = npAsmConnectionGetHeader(npAsmRequestGetConnection(request), "Cache-Control", header, size);
			if (result >= 0) {
				i = 0;
				if (size != 0) {
					i = 0;
					scan = header;
					while (*scan != '=') {
						++i;
						++scan;
						if (size <= i)
							goto parse;
					}
					++i;
				}
parse:
				request->maxAge = (int)sceClibStrtoll(&header[i], NULL, 10);
			}
		}
	}
	if (result < 0)
		request->cacheStatus = result;
	if (header != NULL)
		SceNpCommonPs4_6B428829(npAsmClientGetAllocator(client), header);
	return result;
}

int npAsmRequestUpdateCacheControl(struct NpAsmRequest *request)
{
	return npAsmRequestParseCacheControl(request);
}

int npAsmRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *serviceName, const char *id, unsigned int *pCount)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	void *list;
	unsigned int count;
	int npmId;
	void *connection;
	int result;
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	list = NULL;
	count = 0;
	client = npAsmClientGet();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL) {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		goto finish;
	}
	result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
	if (result >= 0) {
		NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
		if (result >= 0) {
			npAsmClientLock(client);
			SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
			npAsmRequestSetNpManagerId(request, 0);
			npAsmClientUnlock(client);
			result = npAsmConnectionCreate(
				npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
					: npAsmClientGetHttpContextId(client),
				npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
					: npAsmClientGetRequestAllocator(client),
				&connection);
			if (result >= 0) {
				npAsmClientLock(client);
				npAsmRequestSetConnection(request, connection);
				if (npAsmRequestIsAborted(request) != 0) {
					result = NP_ASM_ERROR_ABORTED;
					npAsmClientUnlock(client);
					goto finish;
				}
				npAsmClientUnlock(client);
				result = npAsmConnectionGetRelatedTitleIds(npAsmRequestGetConnection(request), id, serviceName, accessToken,
					tokenEntry, &list, &count);
				if (result >= 0) {
					result = npAsmRequestUpdateCacheControl(request);
					if (result < 0)
						result = 0;
					if (list != NULL && count != 0) {
						request->results = list;
						request->resultCount = count;
						if (pCount != NULL)
							*pCount = count;
						npAsmClientLock(client);
						npAsmConnectionDestroy(npAsmRequestGetConnection(request));
						npAsmRequestSetConnection(request, NULL);
						npAsmClientUnlock(client);
					} else {
						result = NP_ASM_ERROR_80559D4F;
					}
				}
			}
		}
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	NP_ASM_NOTIFY_SERVICE_ERROR(result, pOnlineId);
	return result;
}

__attribute__((noinline)) int npAsmRequestCopyRelatedTitleIds(int requestId, void *buffer, unsigned int count)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	int result;

	result = 0;
	client = npAsmClientGet();
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL)
		return NP_ASM_ERROR_REQUEST_NOT_FOUND;
	if (buffer != NULL && count != 0) {
		if (request->results != NULL) {
			if (request->resultCount != 0) {
				if (count >= (unsigned int)request->resultCount)
					count = request->resultCount;
				sceClibMemcpy_safe(buffer, request->results, 16 * count);
			}
		}
	}
	return result;
}

__attribute__((noinline)) int npAsmRequestGetMaxAge(int requestId, int *pMaxAge)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	int result;

	result = 0;
	client = npAsmClientGet();
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL)
		return NP_ASM_ERROR_REQUEST_NOT_FOUND;
	if (request->cacheStatus < 0)
		return request->cacheStatus;
	if (pMaxAge != NULL)
		*pMaxAge = request->maxAge;
	return result;
}

int npAsmRequestAbort(SceInt32 requestId)
{
	struct NpAsmRequest *request;

	request = npAsmClientFindRequest(npAsmClientGet(), requestId);
	if (request != NULL)
		return npAsmRequestCancel(request);
	return NP_ASM_ERROR_REQUEST_NOT_FOUND;
}

int npAsmSecondaryRequestGetBaseUrl(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	int npmId;
	void *connection;
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	client = npAsmClientGetSecondary();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL) {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		goto finish;
	}
	result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
	if (result >= 0) {
		NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
		if (result >= 0) {
			npAsmClientLock(client);
			SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
			npAsmRequestSetNpManagerId(request, 0);
			npAsmClientUnlock(client);
			result = npAsmConnectionCreate(
				npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
					: npAsmClientGetHttpContextId(client),
				npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
					: npAsmClientGetRequestAllocator(client),
				&connection);
			if (result >= 0) {
				sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
				sceClibMemset(titleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
				result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
				if (result >= 0) {
					npAsmClientLock(client);
					npAsmRequestSetConnection(request, connection);
					if (npAsmRequestIsAborted(request) != 0) {
						result = NP_ASM_ERROR_ABORTED;
						npAsmClientUnlock(client);
						goto finish;
					}
					npAsmClientUnlock(client);
					result = npAsmConnectionGetBaseUrl(npAsmRequestGetConnection(request), pTitleId != NULL ? pTitleId : titleId,
						serviceName, accessToken, tokenEntry, buffer, size);
					if (result >= 0) {
						npAsmClientLock(client);
						npAsmConnectionDestroy(npAsmRequestGetConnection(request));
						npAsmRequestSetConnection(request, NULL);
						npAsmClientUnlock(client);
					}
				}
			}
		}
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	npAsmNotifyServiceError(result, pOnlineId);
	return result;
}

int npAsmSecondaryRequestGetNpComInfo(int requestId, const void *pOnlineId, const char *npComId, int serviceLabel, void *pOut)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	struct NpAsmNpComInfo *list;
	unsigned int count;
	struct NpCommonPs4Allocator *allocator;
	unsigned int i;
	int npmId;
	void *connection;
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];
	struct NpAsmNpComInfoPlain plain;
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	list = NULL;
	count = 0;
	client = npAsmClientGetSecondary();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request != NULL) {
		sceClibMemset(titleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
		result = npAsmClientGetTitleId(client, titleId, titleSecret, 1);
		allocator = NULL;
		if (result >= 0) {
			result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
			allocator = NULL;
			if (result >= 0) {
				NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
				allocator = NULL;
				if (result >= 0) {
					npAsmClientLock(client);
					SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
					npAsmRequestSetNpManagerId(request, 0);
					npAsmClientUnlock(client);
					allocator = npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
						: npAsmClientGetRequestAllocator(client);
					result = npAsmConnectionCreate(
						npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
							: npAsmClientGetHttpContextId(client),
						allocator, &connection);
					if (result >= 0) {
						npAsmClientLock(client);
						npAsmRequestSetConnection(request, connection);
						if (npAsmRequestIsAborted(request) != 0) {
							result = NP_ASM_ERROR_ABORTED;
							npAsmClientUnlock(client);
							goto finish;
						}
						npAsmClientUnlock(client);
						result = npAsmConnectionGetNpComInfo(npAsmRequestGetConnection(request), (const char *)titleId, npComId, accessToken,
							tokenEntry, &list, &count);
						if (result >= 0) {
							if (list == NULL || count == 0)
								goto release;
							for (i = 0; i < count; i++) {
								if (list[i].serviceLabel == (SceUInt32)serviceLabel)
									break;
							}
							if (i == count) {
								result = NP_ASM_ERROR_80559D49;
								goto finish;
							}
							sceClibMemset(&plain, 0, sizeof(plain));
							result = npAsmDecryptNpComInfo(&list[i], titleSecret, &plain);
							if (result >= 0) {
								sceClibMemcpy_safe(plain.npComId, list[i].npComId, 12);
								plain.serviceLabel = list[i].serviceLabel;
								if (pOut != NULL)
									sceClibMemcpy_safe(pOut, &plain, sizeof(plain));
release:
								npAsmClientLock(client);
								npAsmConnectionDestroy(npAsmRequestGetConnection(request));
								npAsmRequestSetConnection(request, NULL);
								npAsmClientUnlock(client);
							}
						}
					}
				}
			}
		}
	} else {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		allocator = NULL;
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	if (list != NULL && allocator != NULL)
		SceNpCommonPs4_6B428829(allocator, list);
	npAsmNotifyServiceError(result, pOnlineId);
	return result;
}
int SceNpCommonPs4_E7831B1A(void)
{
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];
	struct NpAsmClient *client;
	int result;

	sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
	sceClibMemset(titleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
	sceClibMemcpy_safe(titleId, "NPXS10097_00", 13);
	sceClibMemcpy_safe(titleSecret, g_npAsmSystemTitleSecret, NP_ASM_TITLE_SECRET_SIZE);
	result = npAsmSecondaryClientCreate();
	if (result >= 0) {
		client = npAsmClientGetSecondary();
		npAsmClientSetTitleId(client, titleId, titleSecret, 0);
		npAsmClientSetSystemTitle(client, 1);
	}
	return result;
}

int SceNpCommonPs4_A2AEF12D(void)
{
	return npAsmSecondaryClientDestroy();
}

int SceNpCommonPs4_FB0B3FBD(const void *pParam)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;

	client = npAsmClientGetSecondary();
	request = (struct NpAsmRequest *)SceNpCommonPs4_0C922423(sizeof(struct NpAsmRequest), npAsmClientGetAllocator(client));
	if (request == NULL || (request = npAsmRequestInit(request, (const struct NpAsmRequestParam *)pParam)) == NULL)
		return NP_ASM_ERROR_OUT_OF_MEMORY;
	npAsmClientAddRequest(client, request);
	return npAsmRequestGetId(request);
}

int SceNpCommonPs4_9C4F1236(int requestId)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	struct NpCommonPs4Allocator *allocator;
	int result;

	result = 0;
	client = npAsmClientGetSecondary();
	npAsmClientLock(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request != NULL) {
		if (npAsmRequestGetParam(request)->allocator != NULL)
			allocator = npAsmRequestGetParam(request)->allocator;
		else
			allocator = npAsmClientGetRequestAllocator(client);
		npAsmClientRemoveRequest(client, request);
		npAsmRequestCancel(request);
		npAsmRequestFreeResults(request, allocator);
		((void (*)(struct NpAsmRequest *))request->vtable[1])(request);
	} else {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
	}
	npAsmClientUnlock(client);
	return result;
}

int SceNpCommonPs4_9AC24306(int requestId)
{
	struct NpAsmRequest *request;

	request = npAsmClientFindRequest(npAsmClientGetSecondary(), requestId);
	if (request != NULL)
		return npAsmRequestCancel(request);
	return NP_ASM_ERROR_REQUEST_NOT_FOUND;
}

int SceNpCommonPs4_25D1C02B(int requestId, const char *onlineId, void *buffer, SceUInt8 flag)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	int result;

	if (onlineId == NULL)
		return NP_ASM_ERROR_INVALID_ARGUMENT;
	client = npAsmClientGetSecondary();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
	if (request != NULL)
		result = npAsmTransportGetTitleToken(transport, request, onlineId, buffer, flag);
	npAsmNotifyServiceError(result, onlineId);
	return result;
}

int SceNpCommonPs4_C3B57417(int requestId, const void *pOnlineId, void *serviceName, void *buffer, int size)
{
	int result;
	SceUInt8 valid;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];

	if (pOnlineId != NULL && serviceName != NULL) {
		valid = 0;
		result = npAsmClientGetTitleId(npAsmClientGetSecondary(), titleId, titleSecret, 1);
		if (result >= 0) {
			result = npAsmCallbackCheck(titleId, serviceName, &valid, buffer, size);
			if (result >= 0 && valid == 0) {
				result = npAsmSecondaryRequestGetBaseUrl(requestId, pOnlineId, titleId, serviceName, buffer, size);
				if (result >= 0)
					result = npAsmCallbackUpdate(titleId, serviceName, buffer);
			}
		}
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int SceNpCommonPs4_AB41A67D(int requestId, const void *pOnlineId, const char *npComId, int serviceLabel, void *pOut)
{
	if (pOnlineId != NULL && npComId != NULL)
		return npAsmSecondaryRequestGetNpComInfo(requestId, pOnlineId, npComId, serviceLabel, pOut);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int npAsmSecondaryRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *serviceName, const char *id,
	unsigned int *pCount)
{
	struct NpAsmClient *client;
	struct NpAsmTransport *transport;
	struct NpAsmRequest *request;
	void *list;
	unsigned int count;
	int npmId;
	void *connection;
	int result;
	unsigned char accessToken[272];
	unsigned char tokenEntry[1040];

	npmId = 0;
	connection = NULL;
	list = NULL;
	count = 0;
	client = npAsmClientGetSecondary();
	transport = npAsmClientGetTransport(client);
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL) {
		result = NP_ASM_ERROR_REQUEST_NOT_FOUND;
		goto finish;
	}
	result = npAsmTransportGetTitleToken(transport, request, pOnlineId, tokenEntry, 0);
	if (result >= 0) {
		NP_ASM_ACQUIRE_LOGIN(client, request, npmId, accessToken, result, finish);
		if (result >= 0) {
			npAsmClientLock(client);
			SceNpManager_3907F96A(npAsmRequestGetNpManagerId(request));
			npAsmRequestSetNpManagerId(request, 0);
			npAsmClientUnlock(client);
			result = npAsmConnectionCreate(
				npAsmRequestGetParam(request)->httpContextId > 0 ? npAsmRequestGetParam(request)->httpContextId
					: npAsmClientGetHttpContextId(client),
				npAsmRequestGetParam(request)->allocator != NULL ? npAsmRequestGetParam(request)->allocator
					: npAsmClientGetRequestAllocator(client),
				&connection);
			if (result >= 0) {
				npAsmClientLock(client);
				npAsmRequestSetConnection(request, connection);
				if (npAsmRequestIsAborted(request) != 0) {
					result = NP_ASM_ERROR_ABORTED;
					npAsmClientUnlock(client);
					goto finish;
				}
				npAsmClientUnlock(client);
				result = npAsmConnectionGetRelatedTitleIds(npAsmRequestGetConnection(request), id, serviceName, accessToken,
					tokenEntry, &list, &count);
				if (result >= 0) {
					result = npAsmRequestUpdateCacheControl(request);
					if (result < 0)
						result = 0;
					if (list != NULL && count != 0) {
						request->results = list;
						request->resultCount = count;
						if (pCount != NULL)
							*pCount = count;
						npAsmClientLock(client);
						npAsmConnectionDestroy(npAsmRequestGetConnection(request));
						npAsmRequestSetConnection(request, NULL);
						npAsmClientUnlock(client);
					} else {
						result = NP_ASM_ERROR_80559D4F;
					}
				}
			}
		}
	}
finish:
	NP_ASM_FINISH_REQUEST(client, request, result);
	npAsmNotifyServiceError(result, pOnlineId);
	return result;
}

int npAsmSecondaryWrapRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *serviceName, const char *id,
	unsigned int *pCount)
{
	if (pOnlineId != NULL && id != NULL && serviceName != NULL)
		return npAsmSecondaryRequestGetRelatedTitleIds(requestId, pOnlineId, serviceName, id, pCount);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int npAsmSecondaryRequestCopyRelatedTitleIds(int requestId, void *buffer, unsigned int count)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	int result;

	if (buffer == NULL || count == 0)
		return NP_ASM_ERROR_INVALID_ARGUMENT;
	result = 0;
	client = npAsmClientGetSecondary();
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL)
		return NP_ASM_ERROR_REQUEST_NOT_FOUND;
	if (count != 0) {
		if (request->results != NULL) {
			if (request->resultCount != 0) {
				if (count >= (unsigned int)request->resultCount)
					count = request->resultCount;
				sceClibMemcpy_safe(buffer, request->results, 16 * count);
			}
		}
	}
	return result;
}

int npAsmSecondaryRequestGetMaxAge(int requestId, int *pMaxAge)
{
	struct NpAsmClient *client;
	struct NpAsmRequest *request;
	int result;

	if (pMaxAge == NULL)
		return NP_ASM_ERROR_INVALID_ARGUMENT;
	result = 0;
	client = npAsmClientGetSecondary();
	request = npAsmClientFindRequest(client, requestId);
	if (request == NULL)
		return NP_ASM_ERROR_REQUEST_NOT_FOUND;
	if (request->cacheStatus < 0)
		return request->cacheStatus;
	*pMaxAge = request->maxAge;
	return result;
}

int npAsmSecondaryReloadTitleId(void)
{
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];
	unsigned char titleSecret[NP_ASM_TITLE_SECRET_SIZE];

	sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
	sceClibMemset(titleSecret, 0, NP_ASM_TITLE_SECRET_SIZE);
	if (npAsmGetSystemTitleId(titleId, titleSecret) >= 0 && npAsmWrapConnectionSetTitleId(titleId) >= 0
		&& npAsmClientGet() != NULL)
		npAsmClientSetTitleId(npAsmClientGet(), titleId, titleSecret, 0);
	return 0;
}

int SceNpCommonPs4_E2805B06(void)
{
	return npAsmSecondaryReloadTitleId();
}

int SceNpCommonPs4_019C2522(void)
{
	return npAsmWrapConnectionClearTitleId();
}

int SceNpCommonPs4_D0F55105(int a1, int a2)
{
	if (a2 != 0)
		return npAsmWrapConnectionQuery(a1, a2);
	return NP_ASM_ERROR_INVALID_ARGUMENT;
}

int SceNpCommonPs4_1BF9EFFF(const void *pOnlineId, SceUInt8 *pValid, SceUInt64 *pExpire)
{
	return npAsmTransportIsTokenValid(npAsmClientGetTransport(npAsmClientGet()), pOnlineId, pValid, pExpire);
}

int SceNpCommonPs4_19B2A80D(const void *pOnlineId, SceUInt8 *pValid, SceUInt64 *pExpire)
{
	return npAsmTransportIsTokenValid(npAsmClientGetTransport(npAsmClientGetSecondary()), pOnlineId, pValid, pExpire);
}

int SceNpCommonPs4_28372F53(void *serviceName, void *pValid, void *buffer, int size)
{
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];

	if (serviceName != NULL) {
		sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
		result = npAsmClientGetTitleId(npAsmClientGet(), titleId, NULL, 1);
		if (result >= 0)
			result = npAsmCallbackCheck(titleId, serviceName, pValid, buffer, size);
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int SceNpCommonPs4_2A1D2A4B(void *serviceName, void *pValid, void *buffer, int size)
{
	int result;
	unsigned char titleId[NP_ASM_TITLE_ID_SIZE];

	if (serviceName != NULL) {
		sceClibMemset(titleId, 0, NP_ASM_TITLE_ID_SIZE);
		result = npAsmClientGetTitleId(npAsmClientGetSecondary(), titleId, NULL, 1);
		if (result >= 0)
			result = npAsmCallbackCheck(titleId, serviceName, pValid, buffer, size);
	} else {
		result = NP_ASM_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int SceNpCommonPs4_6602CA8B(const int *pParam, const int *pCallbacks)
{
	int result;

	if (npAsmWrapIsInitialized() != 0)
		return NP_ASM_ERROR_ALREADY_INITIALIZED;
	if (pParam == NULL)
		return NP_ASM_ERROR_INVALID_ARGUMENT;
	if (!npAsmIsSystemApp())
		return NP_ASM_ERROR_80559D4B;
	result = npAsmWrapInit(0, pParam);
	if (result >= 0) {
		result = npAsmCallbackTableSet(npAsmGetCallbackTable(), (const struct NpAsmCallbacks *)pCallbacks);
		if (result >= 0) {
			npAsmWrapSetInitialized(1);
			return result;
		}
		npAsmWrapTerm();
	}
	return result;
}

int SceNpCommonPs4_7F68D169(void)
{
	int result;

	if (npAsmWrapIsInitialized() == 0)
		return NP_ASM_ERROR_NOT_INITIALIZED;
	if (!npAsmIsSystemApp())
		return NP_ASM_ERROR_80559D4B;
	npAsmCallbackTableClear(npAsmGetCallbackTable());
	result = npAsmWrapTerm();
	npAsmWrapSetInitialized(0);
	return result;
}
/*SORT-END*/

}
