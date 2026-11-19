/*
 * SceNpSnsFacebook (vs0:sys/external/np_sns_facebook.suprx): the access token library of the
 * Facebook link. The library is an IPMI client of the service "SceNpSnsFb"; every export checks
 * that the client exists and sends one command (synchronously, or asynchronously and then
 * fetches its result).
 *
 * Built by Sony with SNC. No debug record names its files; the names are ours, taken from what
 * the code does.
 */

#include <stddef.h>
#include <string.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include "np_sns_facebook_internal.h"

#define NOINLINE __attribute__((noinline))

extern int sceAppMgrAppParamGetString(SceUID pid, int param, char *string, int length);

static NOINLINE int ConvertError(int error);

SceInt32 g_numRequests = 0;
static IpmiHandle *g_pClients[SNSFB_MAX_CLIENTS] = { NULL, NULL };
static SceInt32 g_clientState[SNSFB_MAX_CLIENTS] = { 0, 0 };
static SnsFbAllocFunc g_pAlloc = NULL;
static SnsFbFreeFunc g_pFree = NULL;
static SceClibMspace g_mspace = NULL;
static SceUID g_memBlock = -1;
static SnsFbAllocator g_allocators[SNSFB_MAX_CLIENTS];
static SceInt32 g_clientTag[SNSFB_MAX_CLIENTS];
static SceInt32 g_useExternalHeap;
static SceInt32 g_ownsMspace;
static SceInt32 g_numAllocations;

NOINLINE int SceNpSnsFacebook_EF957AD0(void);
NOINLINE int SceNpSnsFacebook_836AA54A(void);
NOINLINE int SceNpSnsFacebook_C9ACC1C1();
NOINLINE int SceNpSnsFacebook_0CDD6B52(int requestId);
NOINLINE int SceNpSnsFacebook_78636540(int requestId, const char *pUserId, const char *pAppParam, const char *pPermissions, void *pOut);
NOINLINE int SceNpSnsFacebook_B45419A5(int requestId, const char *pUserId, const char *pAppParam, const char *pPermissions, void *pOut);
NOINLINE int SceNpSnsFacebook_D08701B6(void);
static NOINLINE int CreateClient(int index, SceInt32 *pMode, int useLibraryHeap, SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree);
static NOINLINE int DestroyClient(int index);
static NOINLINE int HasClient(int index);
static NOINLINE int ReserveRequest(int index, int *pRequestId);
static NOINLINE int ResetRequests(int index, int unused);
static NOINLINE int AbortRequest(int index, int requestId);
static NOINLINE int InvokeMethod02(int index, int requestId, int *pValue0, int *pValue1);
static NOINLINE int InvokeMethod04(int index, int requestId, const char *pString0, const char *pString1);
static NOINLINE int InvokeMethod06(int index, int requestId, const char *pString0, const char *pString1, const char *pString2, SceUInt64 value, int *pResult);
static NOINLINE int InvokeMethod08(int index, int requestId, int value0, const char *pString0, const char *pString1, int value1, const char *pString2, int value2, int value3, int *pResult);
static NOINLINE int InvokeMethod0A(int index, int requestId, int value0, const char *pString0, const char *pString1, int value1, int *pResult);
static NOINLINE int InvokeMethod0C(int index, int requestId, int value, const void *pData, const char *pString, void *pOut);
static NOINLINE int InvokeMethod0E(int index, int requestId, const void *pData, const char *pString, void *pOut);
static NOINLINE int InvokeMethod10(int index, int requestId, const char *pString0, const char *pString1, const char *pString2, void *pOut);
static NOINLINE int InvokeMethod12(int index, int requestId, const char *pString0, const char *pString1, const char *pString2, void *pOut);
static NOINLINE int InvokeMethod1E(int index, int requestId, const char *pString0, const char *pString1, const char *pString2, void *pOut);
static NOINLINE int InvokeMethod20(int index, int requestId, const char *pString0, const char *pString1, const char *pString2, void *pOut);
static NOINLINE int GetString(int index, char *pBuffer, SceSize size);
static NOINLINE int Method15(int index, void *pOut);
static NOINLINE int Method16(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3);
static NOINLINE int Method17(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3);
static NOINLINE int Method18(int index);
static NOINLINE int Method19(int index);
static NOINLINE int Method1A(int index);
static NOINLINE int Method1B(int index, int *pValue);
static NOINLINE int Method1C(int index, int *pValue);
static NOINLINE int Method1D(int index, int *pValue);
static NOINLINE int StartSession(IpmiClient *pClient, SceInt32 *pMode);
static NOINLINE int StopSession(IpmiClient *pClient, SceInt32 *pMode);
static NOINLINE int PackMethod02(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId);
static NOINLINE int ReceiveResult03(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod04(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1);
static NOINLINE int ReceiveResult05(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod06(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1, const char *pString2, SceUInt64 value);
static NOINLINE int ReceiveResult07(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod08(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value0, const char *pString0, const char *pString1, int value1, const char *pString2, int value2, int value3);
static NOINLINE int ReceiveResult09(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod0A(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value0, const char *pString0, const char *pString1, int value1);
static NOINLINE int ReceiveResult0B(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod0C(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value, const void *pData, const char *pString);
static NOINLINE int ReceiveResult0D(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod0E(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const void *pData, const char *pString);
static NOINLINE int ReceiveResult0F(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod1E(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1, const char *pString2);
static NOINLINE int ReceiveResult1F(IpmiClient *pClient, int callId, int *pStatus, void *pOut);
static NOINLINE int PackMethod20(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1, const char *pString2);
static NOINLINE int ReceiveResult21(IpmiClient *pClient, int callId, int *pStatus, void *pOut);
static NOINLINE int CallMethod14(IpmiClient *pClient, void *pOut);
static NOINLINE int CallMethod15(IpmiClient *pClient, void *pOut);
static NOINLINE int CallMethod16(IpmiClient *pClient, int present0, int *pValue0, int present1, int *pValue1, int present2, int *pValue2, int present3, int *pValue3);
static NOINLINE int CallMethod17(IpmiClient *pClient, int present0, int value0, int present1, int value1, int present2, int value2, int present3, int value3);
static NOINLINE int CallMethod18(IpmiClient *pClient);
static NOINLINE int CallMethod19(IpmiClient *pClient);
static NOINLINE int CallMethod1A(IpmiClient *pClient);
static NOINLINE int CallMethod1B(IpmiClient *pClient, int *pValue);
static NOINLINE int CallMethod1C(IpmiClient *pClient, int *pValue);
static NOINLINE int CallMethod1D(IpmiClient *pClient, int *pValue);
static NOINLINE int PackMethod10(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1, const char *pString2);
static NOINLINE int ReceiveResult11(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int PackMethod12(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0, const char *pString1, const char *pString2);
static NOINLINE int ReceiveResult13(IpmiClient *pClient, int callId, void *pOut);
static NOINLINE int InitExternalHeap(SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree);
static NOINLINE int InitHeapFromMemory(SceSize size, void *pMemory);
static NOINLINE int InitHeap(SceSize size);
static NOINLINE void TermHeap(void);
static NOINLINE void *AllocMemory(SceSize size);
static NOINLINE void FreeMemory(void *ptr);

/* maps an internal error to the SDK's */
static NOINLINE int ConvertError(int error)
{
	if (error < 0 && (error & 0xFFFFFF00) != 0x80552600) {
		switch (error) {
		case SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED:
			error = SCE_NP_SNS_FACEBOOK_ERROR_ALREADY_INITIALIZED;
			break;
		case SNSFB_INTERNAL_ERROR_NOT_INITIALIZED:
			error = SCE_NP_SNS_FACEBOOK_ERROR_NOT_INITIALIZED;
			break;
		case SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT:
			error = SCE_NP_SNS_FACEBOOK_ERROR_INVALID_ARGUMENT;
			break;
		case SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY:
			error = SCE_NP_SNS_FACEBOOK_ERROR_OUT_OF_MEMORY;
			break;
		case SNSFB_INTERNAL_ERROR_EXCEEDS_MAX:
			error = SCE_NP_SNS_FACEBOOK_ERROR_EXCEEDS_MAX;
			break;
		case SNSFB_INTERNAL_ERROR_ABORTED:
			error = SCE_NP_SNS_FACEBOOK_ERROR_ABORTED;
			break;
		case SNSFB_INTERNAL_ERROR_REQUEST_FAILED:
		case SNSFB_INTERNAL_ERROR_SERVER_ERROR_08:
		case SNSFB_INTERNAL_ERROR_SERVER_ERROR_0B:
		case 0x80559413:
		case 0x80559414:
		case 0x80559415:
		case 0x80559416:
		case 0x80559417:
		case 0x80559418:
			error = SCE_NP_SNS_FACEBOOK_ERROR_FACEBOOK_SERVER_ERROR;
			break;
		case SNSFB_INTERNAL_ERROR_AUTH_ERROR_09:
		case SNSFB_INTERNAL_ERROR_AUTH_ERROR_0A:
			error = SCE_NP_SNS_FACEBOOK_ERROR_AUTH_ERROR;
			break;
		case 0x8055940C:
			error = SCE_NP_SNS_FACEBOOK_ERROR_NOT_SIGNIN;
			break;
		case 0x8055940E:
			error = SCE_NP_SNS_FACEBOOK_ERROR_ACCESS_NOT_ALLOWED;
			break;
		case 0x8055940F:
			error = SCE_NP_SNS_FACEBOOK_ERROR_ACCOUNT_NOT_BOUND;
			break;
		case 0x80559410:
			error = SCE_NP_SNS_FACEBOOK_ERROR_CONFIG_DISABLED;
			break;
		case 0x80559411:
			error = SCE_NP_SNS_FACEBOOK_ERROR_CANCELED_BY_SYSTEM;
			break;
		case 0x80559412:
			error = SCE_NP_SNS_FACEBOOK_ERROR_CHAT_RESTRICTION;
			break;
		default:
			error = SCE_NP_SNS_FACEBOOK_ERROR_UNKNOWN;
			break;
		}
	}
	return error;
}

static inline int ConvertBeginError(int error)
{
	if (error > IPMI_ERROR_INVALID_ID) {
		if (error <= IPMI_ERROR_INVALID_REQUEST) {
			return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
		}
		if (error <= IPMI_ERROR_REQUEST_BUSY) {
			return SNSFB_INTERNAL_ERROR_ABORTED;
		}
	} else if (error > IPMI_ERROR_INVALID_ARGUMENT) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	return SNSFB_INTERNAL_ERROR_BASE;
}

static inline int SendRequest(int index, void *pEvent, int requestId)
{
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = SceIpmi_7E079E19(g_pClients[index], pEvent, 0);
	if (ret == IPMI_ERROR_ABORTED) {
		SceIpmi_48E072C8(g_pClients[index], requestId);
		return SNSFB_INTERNAL_ERROR_REQUEST_FAILED;
	}
	if (ret >= 0) {
		ret = 0;
	}
	return ret;
}

static inline SceSize StringSize(const char *pString)
{
	SceSize length;

	length = 0;
	if (*pString != 0) {
		do {
			pString++;
			length++;
		} while (*pString != 0);
	}
	return length + 1;
}

/* the first client's exports, the library's own client */
int sceNpSnsFacebookInit(void)
{
	int ret;

	ret = SceNpSnsFacebook_EF957AD0();
	if (ret >= 0) {
		g_numRequests = 0;
	}
	return ConvertError(ret);
}

int sceNpSnsFacebookTerm(void)
{
	return ConvertError(SceNpSnsFacebook_D08701B6());
}

int sceNpSnsFacebookCreateRequest(void)
{
	int ret;

	if (g_numRequests >= SNSFB_MAX_REQUESTS) {
		return SCE_NP_SNS_FACEBOOK_ERROR_EXCEEDS_MAX;
	}
	ret = SceNpSnsFacebook_836AA54A();
	if (ret > 0) {
		g_numRequests++;
	}
	return ConvertError(ret);
}

int sceNpSnsFacebookDeleteRequest(void)
{
	int ret;

	ret = SceNpSnsFacebook_C9ACC1C1();
	if (ret >= 0 && --g_numRequests < 0) {
		g_numRequests = 0;
	}
	return ConvertError(ret);
}

int sceNpSnsFacebookAbortRequest(int requestId)
{
	return ConvertError(SceNpSnsFacebook_0CDD6B52(requestId));
}

int sceNpSnsFacebookGetAccessToken(int requestId, const SceNpSnsFacebookAccessTokenParam *pParam,
	SceNpSnsFacebookAccessTokenResult *pResult)
{
	SnsFbAccessToken token;
	char appParam[10];
	char userId[32];
	int ret;
	SceUInt32 i;

	sceClibMemset(&token, 0, sizeof(token));
	if (pParam == NULL || pResult == NULL) {
		return SCE_NP_SNS_FACEBOOK_ERROR_INVALID_ARGUMENT;
	}
	for (i = 0; i < 32; i++) {
		if (pParam->reserved[i] != 0) {
			return SCE_NP_SNS_FACEBOOK_ERROR_INVALID_ARGUMENT;
		}
	}
	sceClibMemset(appParam, 0, sizeof(appParam));
	ret = sceAppMgrAppParamGetString(sceKernelGetProcessId(), 12, appParam, sizeof(appParam));
	if (ret < 0) {
		return ret;
	}
	sceClibMemset(userId, 0, sizeof(userId));
	sceClibSnprintf(userId, sizeof(userId), "%llu", pParam->fbAppId);
	ret = SceNpSnsFacebook_78636540(requestId, userId, appParam, pParam->permissions, &token);
	if (ret < 0) {
		return ConvertError(ret);
	}
	sceClibMemset(pResult, 0, 0x128);
	sceClibMemcpy_safe(pResult->accessToken, token.accessToken, 0x100);
	pResult->expiration = token.expiration;
	return ConvertError(ret);
}

int sceNpSnsFacebookGetLongAccessToken(int requestId, const SceNpSnsFacebookAccessTokenParam *pParam,
	SceNpSnsFacebookLongAccessTokenResult *pResult)
{
	char appParam[10];
	char userId[32];
	int ret;
	SceUInt32 i;

	if (pParam == NULL || pResult == NULL) {
		return SCE_NP_SNS_FACEBOOK_ERROR_INVALID_ARGUMENT;
	}
	for (i = 0; i < 32; i++) {
		if (pParam->reserved[i] != 0) {
			return SCE_NP_SNS_FACEBOOK_ERROR_INVALID_ARGUMENT;
		}
	}
	sceClibMemset(appParam, 0, sizeof(appParam));
	ret = sceAppMgrAppParamGetString(sceKernelGetProcessId(), 12, appParam, sizeof(appParam));
	if (ret >= 0) {
		sceClibMemset(userId, 0, sizeof(userId));
		sceClibSnprintf(userId, sizeof(userId), "%llu", pParam->fbAppId);
		ret = SceNpSnsFacebook_B45419A5(requestId, userId, appParam, pParam->permissions, pResult);
		ret = ConvertError(ret);
	}
	return ret;
}

int SceNpSnsFacebook_9663A2EC(SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree)
{
	SceInt32 mode;

	if (HasClient(0) != 0) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	if (pAlloc == NULL || pFree == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	InitExternalHeap(pAlloc, pFree);
	mode = 1;
	return CreateClient(0, &mode, 1, NULL, NULL);
}

int SceNpSnsFacebook_34A88B2D(SceSize size, void *pMemory)
{
	SceInt32 mode;

	if (HasClient(0) != 0) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	InitHeapFromMemory(size, pMemory);
	mode = 1;
	return CreateClient(0, &mode, 1, NULL, NULL);
}

int SceNpSnsFacebook_9C6AAB52(void)
{
	SceInt32 mode;

	if (HasClient(0) != 0) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	InitHeap(SNSFB_MEMORY_SIZE);
	mode = 1;
	return CreateClient(0, &mode, 1, NULL, NULL);
}

int SceNpSnsFacebook_06A167FC(void)
{
	int ret;

	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = DestroyClient(0);
	if (ret >= 0) {
		TermHeap();
	}
	return ret;
}

NOINLINE int SceNpSnsFacebook_836AA54A(void)
{
	int requestId;
	int ret;

	requestId = 0;
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = ReserveRequest(0, &requestId);
	if (ret >= 0) {
		ret = requestId;
		if (requestId <= 0) {
			ret = SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
		}
	}
	return ret;
}

NOINLINE int SceNpSnsFacebook_C9ACC1C1(int unused)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return ResetRequests(0, unused);
}

NOINLINE int SceNpSnsFacebook_0CDD6B52(int requestId)
{
	if (HasClient(0) != 0) {
		return AbortRequest(0, requestId);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_6A170F5D(int requestId, int *pValue0, int *pValue1)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pValue0 != NULL && pValue1 != NULL) {
		return InvokeMethod02(0, requestId, pValue0, pValue1);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_5EAB459F(int requestId, const char *pString0, const char *pString1)
{
	if (HasClient(0) != 0) {
		return InvokeMethod04(0, requestId, pString0, pString1);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_76B234BD(int requestId, const char *pString0, const char *pString1, const char *pString2,
	SceUInt64 value, int *pResult)
{
	if (HasClient(0) != 0) {
		return InvokeMethod06(0, requestId, pString0, pString1, pString2, value, pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_DC06BD48(int requestId, int value0, const char *pString0, const char *pString1, int value1,
	const char *pString2, int value2, int value3, int *pResult)
{
	if (HasClient(0) != 0) {
		return InvokeMethod08(0, requestId, value0, pString0, pString1, value1, pString2, value2, value3, pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_CB51675B(int requestId, int value0, const char *pString0, const char *pString1, int value1,
	int *pResult)
{
	if (HasClient(0) != 0) {
		return InvokeMethod0A(0, requestId, value0, pString0, pString1, value1, pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_4A453D97(int requestId, int value, const void *pData, const char *pString, void *pOut)
{
	if (HasClient(0) != 0) {
		return InvokeMethod0C(0, requestId, value, pData, pString, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_CB2ED352(int requestId, const void *pData, const char *pString, void *pOut)
{
	if (HasClient(0) != 0) {
		return InvokeMethod0E(0, requestId, pData, pString, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_FD842B21(int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod1E(0, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

NOINLINE int SceNpSnsFacebook_B45419A5(int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod20(0, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_B22D89FD(int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod10(0, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

NOINLINE int SceNpSnsFacebook_78636540(int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod12(0, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_64E5B424(char *pBuffer, SceSize size)
{
	if (HasClient(0) != 0) {
		return GetString(0, pBuffer, size);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_7C1286C0(void *pOut)
{
	if (HasClient(0) != 0) {
		return Method15(0, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_DCCE1BB3(int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	if (HasClient(0) != 0) {
		return Method16(0, pValue0, pValue1, pValue2, pValue3);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_2814F3DB(int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	if (HasClient(0) != 0) {
		return Method17(0, pValue0, pValue1, pValue2, pValue3);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_501CCC39(void)
{
	if (HasClient(0) != 0) {
		return Method18(0);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_249B3C57(void)
{
	if (HasClient(0) != 0) {
		return Method19(0);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_72A0685C(void)
{
	if (HasClient(0) != 0) {
		return Method1A(0);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_61DDCBB7(int *pValue)
{
	if (HasClient(0) != 0) {
		return Method1B(0, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_42CD6A87(int *pValue)
{
	if (HasClient(0) != 0) {
		return Method1C(0, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_C9798074(int *pValue)
{
	if (HasClient(0) != 0) {
		return Method1D(0, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

NOINLINE int SceNpSnsFacebook_EF957AD0(void)
{
	SceInt32 mode;

	if (HasClient(0) != 0) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	InitHeap(SNSFB_MEMORY_SIZE);
	mode = 2;
	return CreateClient(0, &mode, 1, NULL, NULL);
}

NOINLINE int SceNpSnsFacebook_D08701B6(void)
{
	int ret;

	if (HasClient(0) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = DestroyClient(0);
	if (ret >= 0) {
		TermHeap();
	}
	return ret;
}

/* the exports that take the client's index */
int SceNpSnsFacebook_8CB58B88(int index, SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree)
{
	SceInt32 mode;

	if (HasClient(index) != 0) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	if (pAlloc == NULL || pFree == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	mode = 1;
	return CreateClient(index, &mode, 0, pAlloc, pFree);
}

int SceNpSnsFacebook_C15158EE(int index)
{
	if (HasClient(index) != 0) {
		return DestroyClient(index);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_A9C661E9(int index)
{
	int requestId;
	int ret;

	requestId = 0;
	if (HasClient(index) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = ReserveRequest(index, &requestId);
	if (ret >= 0) {
		ret = requestId;
		if (requestId <= 0) {
			ret = SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
		}
	}
	return ret;
}

int SceNpSnsFacebook_8770E114(int index, int unused)
{
	if (HasClient(index) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return ResetRequests(index, unused);
}

int SceNpSnsFacebook_46C23078(int index, int requestId)
{
	if (HasClient(index) != 0) {
		return AbortRequest(index, requestId);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_0FEBCA93(int index, int requestId, int *pValue0, int *pValue1)
{
	if (HasClient(index) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pValue0 != NULL && pValue1 != NULL) {
		return InvokeMethod02(index, requestId, pValue0, pValue1);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_ABF56333(int index, int requestId, const char *pString0, const char *pString1)
{
	if (HasClient(index) != 0) {
		return InvokeMethod04(index, requestId, pString0, pString1);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_58CAC784(int index, int requestId, const char *pString0, const char *pString1,
	const char *pString2, SceUInt64 value, int *pResult)
{
	if (HasClient(index) != 0) {
		return InvokeMethod06(index, requestId, pString0, pString1, pString2, value, pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_9DE0B1BC(int index, int requestId, int value0, const char *pString0, const char *pString1,
	int value1, const char *pString2, int value2, int value3, int *pResult)
{
	if (HasClient(index) != 0) {
		return InvokeMethod08(index, requestId, value0, pString0, pString1, value1, pString2, value2, value3,
			pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_C10D390C(int index, int requestId, int value0, const char *pString0, const char *pString1,
	int value1, int *pResult)
{
	if (HasClient(index) != 0) {
		return InvokeMethod0A(index, requestId, value0, pString0, pString1, value1, pResult);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_A3785735(int index, int requestId, int value, const void *pData, const char *pString,
	void *pOut)
{
	if (HasClient(index) != 0) {
		return InvokeMethod0C(index, requestId, value, pData, pString, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_20B68EBC(int index, int requestId, const void *pData, const char *pString, void *pOut)
{
	if (HasClient(index) != 0) {
		return InvokeMethod0E(index, requestId, pData, pString, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_F3956995(int index, int requestId, const char *pString0, const char *pString1,
	const char *pString2, void *pOut)
{
	if (HasClient(index) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod10(index, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_1375638F(int index, int requestId, const char *pString0, const char *pString1,
	const char *pString2, void *pOut)
{
	if (HasClient(index) == 0) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 != NULL && pString1 != NULL) {
		return InvokeMethod12(index, requestId, pString0, pString1, pString2, pOut);
	}
	return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
}

int SceNpSnsFacebook_5069C04E(int index, char *pBuffer, SceSize size)
{
	if (HasClient(index) != 0) {
		return GetString(index, pBuffer, size);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_9A0A4832(int index, void *pOut)
{
	if (HasClient(index) != 0) {
		return Method15(index, pOut);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_FA6FBAAE(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	if (HasClient(index) != 0) {
		return Method16(index, pValue0, pValue1, pValue2, pValue3);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_18041141(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	if (HasClient(index) != 0) {
		return Method17(index, pValue0, pValue1, pValue2, pValue3);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_FB99FF8E(int index)
{
	if (HasClient(index) != 0) {
		return Method18(index);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_6CAB3583(int index)
{
	if (HasClient(index) != 0) {
		return Method19(index);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_3574800C(int index)
{
	if (HasClient(index) != 0) {
		return Method1A(index);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_7CE37858(int index, int *pValue)
{
	if (HasClient(index) != 0) {
		return Method1B(index, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_C87C016D(int index, int *pValue)
{
	if (HasClient(index) != 0) {
		return Method1C(index, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

int SceNpSnsFacebook_DEDD3ECA(int index, int *pValue)
{
	if (HasClient(index) != 0) {
		return Method1D(index, pValue);
	}
	return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
}

/* the library's client */
static NOINLINE int CreateClient(int index, SceInt32 *pMode, int useLibraryHeap, SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree)
{
	IpmiClientConfig config;
	SceSize memorySize;
	void *pMemory;
	IpmiClient *pClient;
	int ret;
	int i;

	if (g_pClients[index] != NULL) {
		return SNSFB_INTERNAL_ERROR_ALREADY_INITIALIZED;
	}
	g_allocators[index].useLibraryHeap = useLibraryHeap;
	g_allocators[index].pAlloc = pAlloc;
	g_allocators[index].pFree = pFree;
	if (useLibraryHeap == 0 && (pAlloc == NULL || pFree == NULL)) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	config.numThreads = 16;
	config.serviceName[0] = '\0';
	config.reserved1 = 0;
	config.numEventFlags = 1;
	config.syncCall.count = 1;
	config.syncCall.maxInSize = 3840;
	config.syncCall.maxOutSize = 3840;
	config.asyncCall.count = 0;
	config.asyncCall.maxInSize = 3840;
	config.asyncCall.maxOutSize = 3840;
	config.resultCall.count = 1;
	config.resultCall.maxInSize = 3840;
	config.resultCall.maxOutSize = 3840;
	config.reserved2 = 0;
	for (i = 0; i < 32; i++) {
		config.queues[i].bufferSize = 256;
		config.queues[i].depth = 8;
	}
	config.maxClients = 0;
	sceClibStrlcpy(config.serviceName, SNSFB_SERVICE_NAME, sizeof(config.serviceName));
	config.maxClients = 4;
	config.resultCall.count = 4;
	config.resultCall.maxInSize = 0x2000;
	config.syncCall.count = 4;
	config.syncCall.maxInSize = 0x2000;
	config.resultCall.maxOutSize = 0x2000;
	config.syncCall.maxOutSize = 0x2000;
	config.asyncCall.count = 4;
	config.asyncCall.maxInSize = 0x2000;
	config.asyncCall.maxOutSize = 0x2000;
	g_clientTag[index] = -1;
	g_clientState[index] = 0;
	config.numEventFlags = 1;
	config.queues[0].bufferSize = 256;
	config.queues[0].depth = 2;
	if (g_allocators[index].useLibraryHeap != 0) {
		memorySize = SceIpmi_296D44D4(&config);
		pMemory = AllocMemory(memorySize);
		if (pMemory == NULL) {
			return SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
		}
	} else {
		memorySize = SceIpmi_296D44D4(&config);
		pMemory = g_allocators[index].pAlloc(memorySize);
		if (pMemory == NULL) {
			return SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
		}
	}
	ret = SceIpmi_BC3A3031(&g_pClients[index], &config, pMemory);
	if (ret != 0) {
		if (g_allocators[index].useLibraryHeap != 0) {
			FreeMemory(pMemory);
		} else {
			g_allocators[index].pFree(pMemory);
		}
		return ret;
	}
	ret = SceIpmi_2C6DB642(g_pClients[index]);
	if (ret != 0) {
		if (ret > IPMI_ERROR_BUSY) {
			if (ret == IPMI_ERROR_UNAVAILABLE) {
				ret = SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
			}
		} else if (ret > IPMI_ERROR_REFUSED) {
			ret = SNSFB_INTERNAL_ERROR_EXCEEDS_MAX;
		}
		SceIpmi_A20BB95B(g_pClients[index]);
		if (g_allocators[index].useLibraryHeap != 0) {
			FreeMemory(pMemory);
		} else {
			g_allocators[index].pFree(pMemory);
		}
		g_pClients[index] = NULL;
		return ret;
	}
	pClient = SceIpmi_1372C7B7(g_pClients[index]);
	ret = StartSession(pClient, pMode);
	if (ret < 0) {
		SceIpmi_A20BB95B(g_pClients[index]);
		if (g_allocators[index].useLibraryHeap != 0) {
			FreeMemory(pMemory);
		} else {
			g_allocators[index].pFree(pMemory);
		}
		g_pClients[index] = NULL;
	}
	return ret;
}

static NOINLINE int DestroyClient(int index)
{
	int status;
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	status = 0;
	StopSession(SceIpmi_1372C7B7(g_pClients[index]), &status);
	ret = SceIpmi_3554D4B0(g_pClients[index]);
	if (ret != IPMI_ERROR_NO_RESULT) {
		SceIpmi_A20BB95B(g_pClients[index]);
		if (g_allocators[index].useLibraryHeap != 0) {
			FreeMemory(g_pClients[index]);
		} else {
			g_allocators[index].pFree(g_pClients[index]);
		}
		ret = 0;
		g_pClients[index] = NULL;
	}
	return ret;
}

static NOINLINE int HasClient(int index)
{
	return g_pClients[index] != NULL;
}

static NOINLINE int ReserveRequest(int index, int *pRequestId)
{
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pRequestId == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	ret = SceIpmi_367CEF82(g_pClients[index], pRequestId, 0, NULL);
	if (ret == IPMI_ERROR_NO_FREE_REQUEST) {
		return SNSFB_INTERNAL_ERROR_EXCEEDS_MAX;
	}
	return ret;
}

static NOINLINE int ResetRequests(int index, int unused)
{
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = SceIpmi_5ECD40E9(g_pClients[index], unused);
	if (ret > IPMI_ERROR_INVALID_ARGUMENT && ret <= IPMI_ERROR_INVALID_REQUEST) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	return ret;
}

static NOINLINE int AbortRequest(int index, int requestId)
{
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	ret = SceIpmi_48E072C8(g_pClients[index], requestId);
	if (ret == IPMI_ERROR_INVALID_ID) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	return ret;
}

static NOINLINE int InvokeMethod02(int index, int requestId, int *pValue0, int *pValue1)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header; SceInt32 value0; SceInt32 value1; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pValue0 == NULL || pValue1 == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod02(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult03(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (ret == 0) {
			*pValue0 = response.value0;
			*pValue1 = response.value1;
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod04(int index, int requestId, const char *pString0, const char *pString1)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	SceUInt8 response[4];

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pString0 == NULL || pString1 == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod04(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0, pString1);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult05(SceIpmi_1372C7B7(g_pClients[index]), callId, response);
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod06(int index, int requestId, const char *pString0, const char *pString1, const char *pString2,
	SceUInt64 value, int *pResult)
{
	struct { SceInt32 header; SceInt32 value; } response;
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod06(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0 != NULL ? pString0 : "",
		pString1 != NULL ? pString1 : "", pString2 != NULL ? pString2 : "", value);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult07(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (pResult != NULL) {
			*pResult = response.value;
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod08(int index, int requestId, int value0, const char *pString0, const char *pString1, int value1,
	const char *pString2, int value2, int value3, int *pResult)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header; SceInt32 value; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod08(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, value0, pString0 != NULL ? pString0 : "",
		pString1 != NULL ? pString1 : "", value1, pString2 != NULL ? pString2 : "", value2, value3);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult09(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (pResult != NULL) {
			*pResult = response.value;
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod0A(int index, int requestId, int value0, const char *pString0, const char *pString1, int value1,
	int *pResult)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header; SceInt32 value; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod0A(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, value0, pString0 != NULL ? pString0 : "",
		pString1 != NULL ? pString1 : "", value1);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult0B(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (pResult != NULL) {
			*pResult = response.value;
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod0C(int index, int requestId, int value, const void *pData, const char *pString, void *pOut)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header; SceUInt8 data[6160]; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pData == NULL || pString == NULL || pOut == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod0C(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, value, pData, pString);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult0D(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (ret == 0) {
			sceClibMemcpy_safe(pOut, response.data, 0x1810);
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod0E(int index, int requestId, const void *pData, const char *pString, void *pOut)
{
	SceUInt8 event[8];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header; SceUInt8 data[516]; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod0E(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pData, pString);
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult0F(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (ret == 0) {
			sceClibMemcpy_safe(pOut, response.data, 0x204);
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod1E(int index, int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	int status;
	int callId;
	SceUInt8 event[8];
	int error;
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	status = 0;
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod1E(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0, pString1,
		pString2 != NULL ? pString2 : "");
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult1F(SceIpmi_1372C7B7(g_pClients[index]), callId, &status, pOut);
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod20(int index, int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	int status;
	int callId;
	SceUInt8 event[8];
	int error;
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	status = 0;
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod20(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0, pString1,
		pString2 != NULL ? pString2 : "");
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult21(SceIpmi_1372C7B7(g_pClients[index]), callId, &status, pOut);
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod10(int index, int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	SceUInt8 event[12];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header[2]; SceUInt8 data[264]; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod10(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0, pString1,
		pString2 != NULL ? pString2 : "");
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult11(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (ret == 0) {
			sceClibMemcpy_safe(pOut, response.data, 0x108);
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int InvokeMethod12(int index, int requestId, const char *pString0, const char *pString1, const char *pString2,
	void *pOut)
{
	SceUInt8 event[12];
	int callId;
	int ret;
	int error;
	struct { SceInt32 header[2]; SceUInt8 data[264]; } response;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	error = SceIpmi_7C002FEC(g_pClients[index], requestId, event);
	if (error != 0) {
		return ConvertBeginError(error);
	}
	PackMethod12(SceIpmi_1372C7B7(g_pClients[index]), &callId, event, requestId, pString0, pString1,
		pString2 != NULL ? pString2 : "");
	ret = SendRequest(index, event, requestId);
	if (ret >= 0) {
		ret = ReceiveResult13(SceIpmi_1372C7B7(g_pClients[index]), callId, &response);
		if (ret == 0) {
			sceClibMemcpy_safe(pOut, response.data, 0x108);
		}
	}
	SceIpmi_8A0BAC4E(g_pClients[index], requestId);
	return ret;
}

static NOINLINE int GetString(int index, char *pBuffer, SceSize size)
{
	char string[320];
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	if (pBuffer == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	sceClibMemset(string, 0, sizeof(string));
	ret = CallMethod14(SceIpmi_1372C7B7(g_pClients[index]), string);
	if (ret >= 0) {
		sceClibStrncpy(pBuffer, string, size);
		pBuffer[size - 1] = '\0';
	}
	return ret;
}

static NOINLINE int Method15(int index, void *pOut)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod15(SceIpmi_1372C7B7(g_pClients[index]), pOut);
}

static NOINLINE int Method16(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	int value0;
	int value1;
	int value2;
	int value3;
	int ret;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	value0 = 0;
	value1 = 0;
	value2 = 0;
	value3 = 0;
	ret = CallMethod16(SceIpmi_1372C7B7(g_pClients[index]), pValue0 != NULL, &value0, pValue1 != NULL, &value1,
		pValue2 != NULL, &value2, pValue3 != NULL, &value3);
	if (pValue0 != NULL) {
		*pValue0 = value0;
	}
	if (pValue1 != NULL) {
		*pValue1 = value1;
	}
	if (pValue2 != NULL) {
		*pValue2 = value2;
	}
	if (pValue3 != NULL) {
		*pValue3 = value3;
	}
	return ret;
}

static NOINLINE int Method17(int index, int *pValue0, int *pValue1, int *pValue2, int *pValue3)
{
	int value0;
	int value1;
	int value2;
	int value3;

	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	value3 = 0;
	value0 = 0;
	value1 = 0;
	value2 = 0;
	if (pValue0 != NULL) {
		value0 = *pValue0;
	}
	if (pValue1 != NULL) {
		value1 = *pValue1;
	}
	if (pValue2 != NULL) {
		value2 = *pValue2;
	}
	if (pValue3 != NULL) {
		value3 = *pValue3;
	}
	return CallMethod17(SceIpmi_1372C7B7(g_pClients[index]), pValue0 != NULL, value0, pValue1 != NULL, value1,
		pValue2 != NULL, value2, pValue3 != NULL, value3);
}

static NOINLINE int Method18(int index)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod18(SceIpmi_1372C7B7(g_pClients[index]));
}

static NOINLINE int Method19(int index)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod19(SceIpmi_1372C7B7(g_pClients[index]));
}

static NOINLINE int Method1A(int index)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod1A(SceIpmi_1372C7B7(g_pClients[index]));
}

static NOINLINE int Method1B(int index, int *pValue)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod1B(SceIpmi_1372C7B7(g_pClients[index]), pValue);
}

static NOINLINE int Method1C(int index, int *pValue)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod1C(SceIpmi_1372C7B7(g_pClients[index]), pValue);
}

static NOINLINE int Method1D(int index, int *pValue)
{
	if (g_pClients[index] == NULL) {
		return SNSFB_INTERNAL_ERROR_NOT_INITIALIZED;
	}
	return CallMethod1D(SceIpmi_1372C7B7(g_pClients[index]), pValue);
}

/* the commands: a synchronous call, or an asynchronous call (Pack) and the fetch of its result (ReceiveResult) */
static NOINLINE int StartSession(IpmiClient *pClient, SceInt32 *pMode)
{
	IpmiInBuffer in;
	int result;
	int ret;

	result = 0;
	in.pBuf = pMode;
	in.size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x00, &in, 1, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int StopSession(IpmiClient *pClient, SceInt32 *pMode)
{
	IpmiInBuffer in;
	int result;
	int ret;

	result = 0;
	in.pBuf = pMode;
	in.size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x01, &in, 1, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod02(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId)
{
	IpmiInBuffer in;
	int value;
	int ret;

	value = requestId;
	in.pBuf = &value;
	in.size = 4;
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x02, &in, 1, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult03(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 12;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x03, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod04(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0,
	const char *pString1)
{
	IpmiInBuffer in[3];
	int value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x04, in, 3, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult05(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 4;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x05, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod06(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const char *pString0,
	const char *pString1, const char *pString2, SceUInt64 value)
{
	IpmiInBuffer in[4];
	struct {
		SceInt32 requestId;
		SceInt32 padding;
		SceUInt64 value;
	} request;
	int ret;

	in[0].pBuf = &request;
	in[0].size = 16;
	request.requestId = requestId;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	request.value = value;
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x06, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult07(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 8;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x07, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod08(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value0,
	const char *pString0, const char *pString1, int value1, const char *pString2, int value2, int value3)
{
	IpmiInBuffer in[4];
	SceInt32 request[5];
	int ret;

	request[0] = requestId;
	request[1] = value0;
	request[2] = value1;
	request[3] = value2;
	request[4] = value3;
	in[0].pBuf = request;
	in[0].size = 20;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x08, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult09(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 8;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x09, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod0A(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value0,
	const char *pString0, const char *pString1, int value1)
{
	IpmiInBuffer in[3];
	SceInt32 request[3];
	int ret;

	request[0] = requestId;
	request[1] = value0;
	request[2] = value1;
	in[0].pBuf = request;
	in[0].size = 12;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x0A, in, 3, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult0B(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 8;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x0B, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod0C(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, int value, const void *pData,
	const char *pString)
{
	IpmiInBuffer in[3];
	SceInt32 request[2];
	int ret;

	request[0] = requestId;
	request[1] = value;
	in[0].pBuf = request;
	in[0].size = 8;
	in[1].pBuf = pData;
	in[1].size = 4144;
	in[2].pBuf = pString;
	in[2].size = StringSize(pString);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x0C, in, 3, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult0D(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 6164;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x0D, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod0E(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId, const void *pData,
	const char *pString)
{
	IpmiInBuffer in[3];
	SceInt32 value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pData;
	in[1].size = 4144;
	in[2].pBuf = pString;
	in[2].size = StringSize(pString);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x0E, in, 3, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult0F(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 520;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x0F, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod10(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId,
	const char *pString0, const char *pString1, const char *pString2)
{
	IpmiInBuffer in[4];
	SceInt32 value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x10, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult11(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 272;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x11, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod12(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId,
	const char *pString0, const char *pString1, const char *pString2)
{
	IpmiInBuffer in[4];
	SceInt32 value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x12, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult13(IpmiClient *pClient, int callId, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 272;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x13, callId, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod14(IpmiClient *pClient, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 320;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x14, NULL, 0, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod15(IpmiClient *pClient, void *pOut)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pOut;
	out.size = 205;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x15, NULL, 0, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod16(IpmiClient *pClient, int present0, int *pValue0, int present1, int *pValue1, int present2,
	int *pValue2, int present3, int *pValue3)
{
	IpmiInBuffer in;
	IpmiOutBuffer out[4];
	SceInt32 present[4];
	int result;
	int ret;

	present[0] = present0;
	present[1] = present1;
	result = 0;
	in.pBuf = present;
	in.size = 16;
	out[0].pBuf = pValue0;
	out[0].size = 4;
	out[1].pBuf = pValue1;
	out[1].size = 4;
	present[2] = present2;
	out[2].pBuf = pValue2;
	out[2].size = 4;
	present[3] = present3;
	out[3].pBuf = pValue3;
	out[3].size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x16, &in, 1, &result, out, 4);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod17(IpmiClient *pClient, int present0, int value0, int present1, int value1, int present2,
	int value2, int present3, int value3)
{
	IpmiInBuffer in;
	SceInt32 request[8];
	int result;
	int ret;

	request[0] = present0;
	result = 0;
	in.pBuf = request;
	in.size = 32;
	request[1] = value0;
	request[2] = present1;
	request[3] = value1;
	request[4] = present2;
	request[5] = value2;
	request[6] = present3;
	request[7] = value3;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x17, &in, 1, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod18(IpmiClient *pClient)
{
	int result;
	int ret;

	result = 0;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x18, NULL, 0, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod19(IpmiClient *pClient)
{
	int result;
	int ret;

	result = 0;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x19, NULL, 0, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod1A(IpmiClient *pClient)
{
	int result;
	int ret;

	result = 0;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x1A, NULL, 0, &result, NULL, 0);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod1B(IpmiClient *pClient, int *pValue)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pValue;
	out.size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x1B, NULL, 0, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod1C(IpmiClient *pClient, int *pValue)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pValue;
	out.size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x1C, NULL, 0, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int CallMethod1D(IpmiClient *pClient, int *pValue)
{
	IpmiOutBuffer out;
	int result;
	int ret;

	result = 0;
	out.pBuf = pValue;
	out.size = 4;
	ret = pClient->pVtbl->invokeSyncMethodList(pClient, SNSFB_CMD_BASE + 0x1D, NULL, 0, &result, &out, 1);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod1E(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId,
	const char *pString0, const char *pString1, const char *pString2)
{
	IpmiInBuffer in[4];
	SceInt32 value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x1E, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult1F(IpmiClient *pClient, int callId, int *pStatus, void *pOut)
{
	IpmiOutBuffer out[2];
	int result;
	int ret;

	result = 0;
	out[0].pBuf = pStatus;
	out[0].size = 4;
	out[1].pBuf = pOut;
	out[1].size = 4144;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x1F, callId, &result, out, 2);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

static NOINLINE int PackMethod20(IpmiClient *pClient, int *pCallId, void *pEvent, int requestId,
	const char *pString0, const char *pString1, const char *pString2)
{
	IpmiInBuffer in[4];
	SceInt32 value;
	int ret;

	value = requestId;
	in[0].pBuf = &value;
	in[0].size = 4;
	in[1].pBuf = pString0;
	in[1].size = StringSize(pString0);
	in[2].pBuf = pString1;
	in[2].size = StringSize(pString1);
	in[3].pBuf = pString2;
	in[3].size = StringSize(pString2);
	ret = pClient->pVtbl->invokeAsyncMethodList(pClient, SNSFB_CMD_BASE + 0x20, in, 4, pCallId, pEvent);
	if (ret == 0) {
		ret = 0;
	}
	return ret;
}

static NOINLINE int ReceiveResult21(IpmiClient *pClient, int callId, int *pStatus, void *pOut)
{
	IpmiOutBuffer out[2];
	int result;
	int ret;

	result = 0;
	out[0].pBuf = pStatus;
	out[0].size = 4;
	out[1].pBuf = pOut;
	out[1].size = 4144;
	ret = pClient->pVtbl->tryGetResultList(pClient, SNSFB_CMD_BASE + 0x21, callId, &result, out, 2);
	if (ret == 0) {
		ret = result;
	}
	return ret;
}

/* the heap the library's client memory comes from: the application's allocator, or an mspace */
static NOINLINE int InitExternalHeap(SnsFbAllocFunc pAlloc, SnsFbFreeFunc pFree)
{
	g_useExternalHeap = 1;
	g_pAlloc = pAlloc;
	g_pFree = pFree;
	g_ownsMspace = 0;
	g_mspace = NULL;
	g_numAllocations = 0;
	g_memBlock = -1;
	if (pAlloc == NULL || pFree == NULL) {
		return SNSFB_INTERNAL_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

static NOINLINE int InitHeapFromMemory(SceSize size, void *pMemory)
{
	g_useExternalHeap = 0;
	g_pAlloc = NULL;
	g_pFree = NULL;
	g_mspace = NULL;
	g_numAllocations = 0;
	g_ownsMspace = 1;
	g_memBlock = -1;
	g_mspace = sceClibMspaceCreate(pMemory, size);
	if (g_mspace != NULL) {
		return 0;
	}
	return SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
}

static NOINLINE int InitHeap(SceSize size)
{
	void *pBase;
	SceUID uid;

	g_useExternalHeap = 0;
	g_ownsMspace = 0;
	g_pAlloc = NULL;
	g_pFree = NULL;
	pBase = NULL;
	g_mspace = NULL;
	g_numAllocations = 0;
	uid = sceKernelAllocMemBlock(SNSFB_MEMORY_NAME, SNSFB_MEMORY_TYPE, size, NULL);
	if (uid < 0) {
		return uid;
	}
	g_memBlock = uid;
	sceKernelGetMemBlockBase(uid, &pBase);
	g_mspace = sceClibMspaceCreate(pBase, size);
	if (g_mspace != NULL) {
		return uid;
	}
	if (g_memBlock != 0) {
		sceKernelFreeMemBlock(g_memBlock);
		g_memBlock = 0;
	}
	return SNSFB_INTERNAL_ERROR_OUT_OF_MEMORY;
}

static NOINLINE void TermHeap(void)
{
	SnsFbMspaceStats stats;
	SnsFbMspaceStats stats2;

	if (g_useExternalHeap != 0) {
		g_pAlloc = NULL;
		g_pFree = NULL;
		g_numAllocations = 0;
	} else if (g_ownsMspace != 0) {
		if (g_mspace != NULL) {
			sceClibMspaceMallocStats(g_mspace, &stats.stats);
			sceClibMspaceDestroy(g_mspace);
			g_numAllocations = 0;
		}
		g_mspace = NULL;
	} else {
		if (g_mspace != NULL) {
			sceClibMspaceMallocStats(g_mspace, &stats2.stats);
			sceClibMspaceDestroy(g_mspace);
			g_numAllocations = 0;
			if (g_memBlock != 0) {
				g_mspace = NULL;
				sceKernelFreeMemBlock(g_memBlock);
				g_memBlock = 0;
			}
		}
		g_mspace = NULL;
	}
}

static NOINLINE void *AllocMemory(SceSize size)
{
	struct MspaceStats stats;
	void *ptr;

	if (g_useExternalHeap != 0) {
		if (g_pAlloc != NULL) {
			ptr = g_pAlloc(size);
			if (ptr != NULL) {
				g_numAllocations++;
				return ptr;
			}
		}
		return NULL;
	}
	if (g_mspace == NULL) {
		return NULL;
	}
	ptr = sceClibMspaceMalloc(g_mspace, size);
	if (ptr == NULL) {
		sceClibMspaceMallocStats(g_mspace, &stats);
		return NULL;
	}
	g_numAllocations++;
	return ptr;
}

static NOINLINE void FreeMemory(void *ptr)
{
	if (g_useExternalHeap != 0) {
		g_numAllocations--;
		if (g_pFree != NULL) {
			g_pFree(ptr);
		}
	} else {
		g_numAllocations--;
		if (g_mspace != NULL) {
			sceClibMspaceFree(g_mspace, ptr);
		}
	}
}

int module_start(void)
{
	return 9;
}

int module_stop(void)
{
	return 9;
}
