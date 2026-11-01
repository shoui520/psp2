/*
 * SceNpManager: NP initialisation, the library heap and the requests the shell's NP service
 * answers through ShellSvc. The communication configuration of sceNpInit is kept for the
 * other NP libraries.
 */

#include "np_manager_internal.h"

#define NP_MANAGER_HEAP_SIZE		0x1000
#define NP_PLUS_ENTITLEMENT_COUNT	5
#define NP_TICKET_BUFFER_SIZE		1536

/* the registry setting that grants PlayStation Plus without an entitlement */
#define NP_REG_PLUS_FORCED		8165220

/* the ShellSvc requests of this file */
#define NP_IPC_GET_A0000		0xa0000
#define NP_IPC_SET_A0001		0xa0001
#define NP_IPC_GET_COMMON_SETTINGS	0xa0002
#define NP_IPC_GET_A0003		0xa0003
#define NP_IPC_SET_A0004		0xa0004
#define NP_IPC_GET_A0005		0xa0005
#define NP_IPC_GET_A0006		0xa0006
#define NP_IPC_SET_A0007		0xa0007
#define NP_IPC_SET_A0008		0xa0008

/* a callback sceNpCheckCallback runs */
typedef struct NpCheckCallback {
	const char *name;
	int (*func)(void);
} NpCheckCallback;

static int s_npCallbackEvents[7][2] = {
	{ 48, 51 }, { 24, 26 }, { 28, 30 }, { 32, 34 }, { 36, 38 }, { 40, 42 }, { 44, 46 },
};
static int s_npShellCallbackEvents[6][2] = {
	{ 25, 27 }, { 29, 31 }, { 33, 35 }, { 37, 39 }, { 41, 43 }, { 45, 47 },
};
static NpCheckCallback s_npCheckCallbacks[1] = {
	{ "sceNpImpsUtilityForNpInfraCheckCallback", sceNpImpsUtilityForNpInfraCheckCallback },
};
static int s_npInitialized = 0;
static SceUID s_npMemBlock = 0;
static int s_npShellInitialized = 0;
static SceUID s_npShellMemBlock = 0;
static int s_npHeapInitialized = 0;
static const char *s_npPlusEntitlementIds[NP_PLUS_ENTITLEMENT_COUNT] = {
	"IP9101-NPIA90005_00-PLUS00",
	"IP9102-NPIA90006_00-PLUS00",
	"IP9103-NPIA90007_00-PLUS00",
	"IP9104-NPIA90008_00-PLUS00",
	"IP9105-NPIA90009_00-PLUS00",
};

static SceNpCommunicationId s_npCommunicationId;
static SceNpCommunicationPassphrase s_npCommunicationPassphrase;
static SceNpCommunicationSignature s_npCommunicationSignature;
static SceNpCommunicationConfig s_npCommunicationConfig;
static void *s_npHeap;
static SceUInt8 s_npTicket[NP_TICKET_BUFFER_SIZE];

static int npManagerHeapInit(SceSize size, void *base);
static void npManagerHeapTerm(void);
static int npManagerGetCommonSettings(void *settings, char *environment, SceSize environmentSize);
static int npIpcGetA0000(IPMI::Client *client, int *value);
static int npIpcSetA0001(IPMI::Client *client, SceUInt64 value);
static int npIpcGetCommonSettings(IPMI::Client *client, void *settings, char *environment,
				  SceSize environmentSize, int *result);
static int npIpcGetA0003(IPMI::Client *client, int *value);
static int npIpcSetA0004(IPMI::Client *client, int value);
static int npIpcGetA0005(IPMI::Client *client, void *buf);
static int npIpcGetA0006(IPMI::Client *client, void *buf);
static int npIpcSetA0007(IPMI::Client *client, const void *data1, const void *data2,
			 const void *data3);
static int npIpcSetA0008(IPMI::Client *client, const void *data1, const void *data2);

/* the NP service's request errors as the OAuth functions return them */
static inline int npAuthOAuthConvertError(int ret)
{
	switch (ret) {
	case NP_SERVER_ERROR_ABORTED:
		return SCE_NP_AUTH_ERROR_ABORTED;
	case NP_SERVER_ERROR_SIGNED_OUT:
		return SCE_NP_AUTH_ERROR_SIGNED_OUT;
	case NP_MANAGER_ERROR_REQUEST_NOT_FOUND:
		return SCE_NP_AUTH_ERROR_EINVAL;
	default:
		return ret;
	}
}

extern "C" {

SceNpAuthOAuthRequestId sceNpAuthCreateOAuthRequest(void)
{
	int reqId = 0;
	int ret;

	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	ret = SceNpManager_4110B787(&reqId);
	if (ret < 0)
		return npAuthOAuthConvertError(ret);
	return reqId;
}

int sceNpAuthDeleteOAuthRequest(SceNpAuthOAuthRequestId reqId)
{
	int ret;

	if (reqId == 0)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	ret = SceNpManager_3907F96A(reqId);
	if (ret < 0)
		return npAuthOAuthConvertError(ret);
	return ret;
}

int sceNpAuthAbortOAuthRequest(SceNpAuthOAuthRequestId reqId)
{
	int ret;

	if (reqId == 0)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	ret = SceNpManager_737B2820(reqId);
	if (ret < 0)
		return npAuthOAuthConvertError(ret);
	return ret;
}

int sceNpAuthGetAuthorizationCode(SceNpAuthOAuthRequestId reqId,
				  const SceNpAuthGetAuthorizationCodeParameter *param,
				  SceNpAuthorizationCode *authCode, int *issuerId)
{
	int ret;

	if (reqId == 0 || param == NULL || authCode == NULL || issuerId == NULL ||
	    param->size != sizeof(*param) || param->pClientId == NULL || param->pScope == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	ret = npAuthGetAuthorizationCode(reqId, param, authCode, issuerId);
	if (ret < 0)
		return npAuthOAuthConvertError(ret);
	return ret;
}

int SceNpManagerOAuth_C62F86A6(SceNpAuthOAuthRequestId reqId,
			       const NpAuthGetAuthorizationCodeExParameter *param,
			       SceNpAuthorizationCode *authCode, int *issuerId)
{
	int ret;

	if (reqId == 0 || param == NULL || authCode == NULL || issuerId == NULL ||
	    param->size != sizeof(*param))
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (param->size != sizeof(*param) || param->pClientId == NULL ||
	    param->pClientSecret == NULL || param->pScope == NULL)
		return SCE_NP_AUTH_ERROR_EINVAL;
	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	ret = npAuthGetAuthorizationCodeEx(reqId, param->pClientId, param->pClientSecret,
					   param->pScope, authCode, issuerId);
	if (ret < 0)
		return npAuthOAuthConvertError(ret);
	return ret;
}

int sceNpManagerGetCachedParam(SceNpManagerCacheParam *param)
{
	NpManagerAccountInfo info;
	int ret;

	if (param == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	ret = SceNpManager_E45E4003(&info);
	if (ret < 0)
		return ret;
	sceClibMemcpy_safe(&param->npId, &info.npId, sizeof(param->npId));
	sceClibMemcpy_safe(&param->avatarUrl, &info.avatarUrl, sizeof(param->avatarUrl));
	return 0;
}

int sceNpCheckCallback(void)
{
	int firstError = 0;
	int failed = 0;
	int ret;
	int i;

	if (!SceNpManager_A6964218())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	for (i = 0; i < 7; i++) {
		SceShellSvc_FB03FBC9(s_npCallbackEvents[i][0]);
		SceShellSvc_FB03FBC9(s_npCallbackEvents[i][1]);
	}
	for (i = 0; i < 1; i++) {
		ret = s_npCheckCallbacks[i].func();
		if (ret < 0 && !failed) {
			failed = 1;
			firstError = ret;
		}
	}
	if (failed)
		return firstError;
	return ret;
}

int SceNpManager_997CD5A7(void)
{
	int ret = 0;
	int i;

	if (SceNpManager_A6964218()) {
		for (i = 0; i < 6; i++) {
			SceShellSvc_FB03FBC9(s_npShellCallbackEvents[i][0]);
			SceShellSvc_FB03FBC9(s_npShellCallbackEvents[i][1]);
		}
	} else {
		ret = SCE_NP_ERROR_NOT_INITIALIZED;
	}
	if (SceLibKernel_9F793F84() < 0x1500000)
		sceNpCheckCallback();
	return ret;
}

int sceNpInit(const SceNpCommunicationConfig *commConf, SceNpOptParam *opt)
{
	void *base;
	NpAllocator allocator;
	char environment[17];
	NpCommonSettings settings;
	const SceNpCommunicationId *commId;
	const SceNpCommunicationPassphrase *commPassphrase;
	int ret;

	if (s_npInitialized > 0)
		return SCE_NP_ERROR_ALREADY_INITIALIZED;
	ret = sceKernelAllocMemBlock("SceNp", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
				     NP_MANAGER_HEAP_SIZE, NULL);
	if (ret < 0)
		goto error;
	s_npMemBlock = ret;
	sceKernelGetMemBlockBase(ret, &base);
	ret = npManagerHeapInit(NP_MANAGER_HEAP_SIZE, base);
	if (ret < 0)
		goto error;
	if (npManagerGetCommonSettings(&settings, environment, sizeof(environment)) >= 0) {
		SceNpCommon_652FC5E0(environment);
		SceNpCommon_DA04BA55(&settings);
	}
	ret = npManagerIpcInit();
	if (ret < 0)
		goto error;
	allocator.alloc = npManagerAlloc;
	allocator.free = npManagerFree;
	ret = sceNpImpsUtilityForNpInfraInit(&allocator);
	if (ret < 0)
		goto error;
	ret = SceNpCommon_A385C8CD(4096, 0x10000100, 0, 0);
	if (ret < 0)
		goto error;
	sceClibMemset(&s_npCommunicationConfig, 0, sizeof(s_npCommunicationConfig));
	if (commConf != NULL) {
		commId = NULL;
		if (commConf->commId != NULL) {
			commId = &s_npCommunicationId;
			sceClibMemcpy_safe(&s_npCommunicationId, commConf->commId,
					   sizeof(s_npCommunicationId));
		}
		s_npCommunicationConfig.commId = commId;
		commPassphrase = NULL;
		if (commConf->commPassphrase != NULL) {
			commPassphrase = &s_npCommunicationPassphrase;
			sceClibMemcpy_safe(&s_npCommunicationPassphrase, commConf->commPassphrase,
					   sizeof(s_npCommunicationPassphrase));
		}
		s_npCommunicationConfig.commPassphrase = commPassphrase;
		if (commConf->commSignature != NULL) {
			sceClibMemcpy_safe(&s_npCommunicationSignature, commConf->commSignature,
					   sizeof(s_npCommunicationSignature));
			s_npCommunicationConfig.commSignature = &s_npCommunicationSignature;
		} else {
			s_npCommunicationConfig.commSignature = NULL;
		}
	}
	s_npInitialized = 1;
	return ret;

error:
	sceNpImpsUtilityForNpInfraTerm();
	npManagerIpcTerm();
	npManagerHeapTerm();
	if (s_npMemBlock != 0) {
		sceKernelFreeMemBlock(s_npMemBlock);
		s_npMemBlock = 0;
	}
	sceClibMemset(&s_npCommunicationConfig, 0, sizeof(s_npCommunicationConfig));
	return ret;
}

int sceNpTerm(void)
{
	if (!s_npInitialized)
		return SCE_NP_ERROR_NOT_INITIALIZED;
	SceNpCommon_BA6310A1();
	sceNpImpsUtilityForNpInfraTerm();
	npManagerIpcTerm();
	npManagerHeapTerm();
	if (s_npMemBlock != 0) {
		sceKernelFreeMemBlock(s_npMemBlock);
		s_npMemBlock = 0;
	}
	s_npInitialized = 0;
	return 0;
}

__attribute__((noinline))
int SceNpManager_A6964218(void)
{
	return s_npInitialized != 0;
}

int SceNpManager_96DF1B76(SceNpCommunicationConfig *commConf)
{
	if (!s_npInitialized)
		return SCE_NP_ERROR_NOT_INITIALIZED;
	sceClibMemcpy_safe(commConf, &s_npCommunicationConfig, sizeof(s_npCommunicationConfig));
	return 0;
}

/* sceNpInit for the shell: the heap, the NP service connections, no communication config */
int SceNpManager_C89E0B94(SceSize size)
{
	void *base;
	NpAllocator allocator;
	int ret;

	if (s_npShellInitialized > 0)
		return SCE_NP_ERROR_ALREADY_INITIALIZED;
	if (size == 0)
		size = NP_MANAGER_HEAP_SIZE;
	ret = sceKernelAllocMemBlock("SceNpShell", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, NULL);
	if (ret < 0)
		goto error;
	s_npShellMemBlock = ret;
	sceKernelGetMemBlockBase(ret, &base);
	ret = npManagerHeapInit(size, base);
	if (ret < 0)
		goto error;
	ret = npManagerIpcInit();
	if (ret < 0)
		goto error;
	allocator.alloc = npManagerAlloc;
	allocator.free = npManagerFree;
	ret = sceNpImpsUtilityForNpInfraInit(&allocator);
	if (ret < 0)
		goto error;
	s_npShellInitialized = 1;
	return ret;

error:
	sceNpImpsUtilityForNpInfraTerm();
	npManagerIpcTerm();
	npManagerHeapTerm();
	SceNpCommon_E2743C07();
	return ret;
}

int SceNpManager_C253AE5B(void)
{
	if (!s_npShellInitialized)
		return SCE_NP_ERROR_NOT_INITIALIZED;
	sceNpImpsUtilityForNpInfraTerm();
	npManagerIpcTerm();
	npManagerHeapTerm();
	SceNpCommon_E2743C07();
	s_npShellInitialized = 0;
	return 0;
}

}

__attribute__((noinline))
static int npManagerHeapInit(SceSize size, void *base)
{
	if (s_npHeapInitialized > 0)
		return SCE_NP_ERROR_ALREADY_INITIALIZED;
	s_npHeap = sceClibMspaceCreate(base, size);
	s_npHeapInitialized = 1;
	return 0;
}

__attribute__((noinline))
static void npManagerHeapTerm(void)
{
	if (s_npHeapInitialized) {
		sceClibMspaceDestroy(s_npHeap);
		s_npHeapInitialized = 0;
	}
}

void *npManagerAlloc(SceSize size)
{
	return sceClibMspaceMalloc(s_npHeap, size);
}

void npManagerFree(void *ptr)
{
	sceClibMspaceFree(s_npHeap, ptr);
}

extern "C" {

int SceNpManager_BA49FD24(void **heap)
{
	if (s_npHeap == NULL)
		return SCE_NP_ERROR_NOT_INITIALIZED;
	*heap = s_npHeap;
	return 0;
}

/* whether the signed-in account has PlayStation Plus */
int SceNpManagerSP_7C78A608(int *isPlus)
{
	SceNpEntitlement ent;
	SceSize ticketSize;
	SceSize size;
	int value;
	int plus;
	int ret;
	SceUInt32 i;

	*isPlus = 0;
	ticketSize = sizeof(s_npTicket);
	ret = SceNpManager_2EA038D8(s_npTicket, &ticketSize);
	if (ret < 0)
		return ret;
	size = ticketSize;
	value = 0;
	if (sceRegMgrSystemParamGetInt(NP_REG_PLUS_FORCED, &value) >= 0 && value == 1) {
		plus = 1;
	} else {
		for (i = 0; i < NP_PLUS_ENTITLEMENT_COUNT; i++) {
			if (sceNpAuthGetEntitlementById(s_npTicket, size,
							s_npPlusEntitlementIds[i], &ent) >= 0) {
				plus = 1;
				goto out;
			}
		}
		plus = 0;
	}
out:
	*isPlus = plus;
	return 0;
}

int sceNpGetServiceState(SceNpServiceState *state)
{
	if (state == NULL)
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	if (!sceNpImpsUtilityForNpInfraIsInitialized())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	return sceNpImpsUtilityForNpInfraGetServiceState(state);
}

int sceNpRegisterServiceStateCallback(SceNpServiceStateCallback callback, void *userdata)
{
	if (callback == NULL)
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	if (!sceNpImpsUtilityForNpInfraIsInitialized())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	return sceNpImpsUtilityForNpInfraRegisterServiceStateCallback(callback, userdata);
}

int sceNpUnregisterServiceStateCallback(void)
{
	if (!sceNpImpsUtilityForNpInfraIsInitialized())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	return sceNpImpsUtilityForNpInfraUnregisterServiceStateCallback();
}

int SceNpManager_06EBD8B1(void)
{
	if (!sceNpImpsUtilityForNpInfraIsInitialized())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	return sceNpImpsUtilityForNpInfraRequestServiceState();
}

int SceNpManager_44DF26F5(int *state)
{
	if (state == NULL)
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	if (!sceNpImpsUtilityForNpInfraIsInitialized())
		return SCE_NP_ERROR_NOT_INITIALIZED;
	return sceNpImpsUtilityForNpInfraGetServiceStateEx(state);
}

int SceNpManager_B24FC028(int *value)
{
	return npIpcGetA0000(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_BD57A0B8(SceUInt64 value)
{
	return npIpcSetA0001(SceShellSvc_B31E7F1C(), value);
}

}

__attribute__((noinline))
static int npManagerGetCommonSettings(void *settings, char *environment, SceSize environmentSize)
{
	int result;

	return npIpcGetCommonSettings(SceShellSvc_B31E7F1C(), settings, environment,
				      environmentSize, &result);
}

extern "C" {

int SceNpManager_4C5D2A19(int *value)
{
	return npIpcGetA0003(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_0E26EA86(int value)
{
	return npIpcSetA0004(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_CFAEB050(void *buf)
{
	return npIpcGetA0005(SceShellSvc_B31E7F1C(), buf);
}

int SceNpManager_38626F73(void *buf)
{
	return npIpcGetA0006(SceShellSvc_B31E7F1C(), buf);
}

void SceNpManager_9554802B(const void *data1, const void *data2, const void *data3)
{
	SceUInt8 empty[32];

	if (data3 == NULL) {
		data3 = empty;
		sceClibMemset(empty, 0, sizeof(empty));
	}
	npIpcSetA0007(SceShellSvc_B31E7F1C(), data1, data2, data3);
}

void SceNpManager_72936716(const void *data1, const void *data2)
{
	SceUInt8 empty[32];

	if (data1 == NULL)
		return;
	if (data2 == NULL) {
		data2 = empty;
		sceClibMemset(empty, 0, sizeof(empty));
	}
	npIpcSetA0008(SceShellSvc_B31E7F1C(), data1, data2);
}

}

__attribute__((noinline))
static int npIpcGetA0000(IPMI::Client *client, int *value)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = value;
	out[0].bufferSize = sizeof(*value);
	ret = client->invokeSyncMethod(NP_IPC_GET_A0000, (const IPMI::DataInfo *)NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcSetA0001(IPMI::Client *client, SceUInt64 value)
{
	int result;
	SceUInt64 param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	param = value;
	ret = client->invokeSyncMethod(NP_IPC_SET_A0001, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcGetCommonSettings(IPMI::Client *client, void *settings, char *environment,
				  SceSize environmentSize, int *value)
{
	int result;
	SceSize param;
	IPMI::DataInfo in[1];
	IPMI::BufferInfo out[3];
	int ret;

	result = 0;
	param = environmentSize;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	out[0].pBuffer = settings;
	out[0].bufferSize = sizeof(NpCommonSettings);
	out[1].pBuffer = environment;
	out[1].bufferSize = 17;
	out[2].pBuffer = value;
	out[2].bufferSize = sizeof(*value);
	ret = client->invokeSyncMethod(NP_IPC_GET_COMMON_SETTINGS, in, 1, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcGetA0003(IPMI::Client *client, int *value)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = value;
	out[0].bufferSize = sizeof(*value);
	ret = client->invokeSyncMethod(NP_IPC_GET_A0003, (const IPMI::DataInfo *)NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcSetA0004(IPMI::Client *client, int value)
{
	int result;
	int param;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	param = value;
	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	ret = client->invokeSyncMethod(NP_IPC_SET_A0004, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcGetA0005(IPMI::Client *client, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 32;
	ret = client->invokeSyncMethod(NP_IPC_GET_A0005, (const IPMI::DataInfo *)NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcGetA0006(IPMI::Client *client, void *buf)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = buf;
	out[0].bufferSize = 32;
	ret = client->invokeSyncMethod(NP_IPC_GET_A0006, (const IPMI::DataInfo *)NULL, 0, &result,
				       out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcSetA0007(IPMI::Client *client, const void *data1, const void *data2,
			 const void *data3)
{
	int result;
	IPMI::DataInfo in[3];
	int ret;

	result = 0;
	in[0].pData = data1;
	in[0].dataSize = 128;
	in[1].pData = data2;
	in[1].dataSize = 128;
	in[2].pData = data3;
	in[2].dataSize = 32;
	ret = client->invokeSyncMethod(NP_IPC_SET_A0007, in, 3, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npIpcSetA0008(IPMI::Client *client, const void *data1, const void *data2)
{
	int result;
	IPMI::DataInfo in[2];
	int ret;

	result = 0;
	in[0].pData = data1;
	in[0].dataSize = 1024;
	in[1].pData = data2;
	in[1].dataSize = 32;
	ret = client->invokeSyncMethod(NP_IPC_SET_A0008, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
