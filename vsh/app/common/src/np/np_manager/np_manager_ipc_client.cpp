/*
 * SceNpManager's requests to the shell's NP service: the blocking requests go through the
 * "SceNpManager" BlockingIpc client, the others through ShellSvc. The NP service's event
 * reaches the callbacks registered with SceNpManager_351A74C2.
 */

#include "np_manager_internal.h"

#define NP_MANAGER_SERVICE_NAME		"SceNpManager"
#define NP_MANAGER_CALLBACK_COUNT	4

/* the kinds of request SceNpManager creates */
#define NP_MANAGER_REQUEST_TICKET	1
#define NP_MANAGER_REQUEST_21F1D18E	2
#define NP_MANAGER_REQUEST_OAUTH	4

/* a request the NP service did not answer in time */
#define NP_MANAGER_ERROR_REQUEST_TIMEOUT	-2141911675	/* 0x80550585 */

/* a callback of the NP service's event, and the id the service gave it */
typedef struct NpManagerCallbackEntry {
	int id;
	NpManagerCallback func;
	void *arg;
} NpManagerCallbackEntry;

static BlockingIpc::Client *s_npManagerIpcClient = NULL;
static SceKernelLwMutexWork *s_npManagerIpcMutex = NULL;
static const NpManagerOverride *s_npManagerOverride = NULL;
static NpManagerCallbackEntry s_npManagerCallbacks[NP_MANAGER_CALLBACK_COUNT];

static int npManagerIpcEventHandler(int event, void *arg);

/* beginBlockingIpc, its errors as SceNpManager's */
__attribute__((always_inline))
static inline int npManagerBeginBlockingIpc(int reqId, BlockingIpc::Client::Handle *handle)
{
	int ret;

	ret = SceIpmi_7C002FEC(s_npManagerIpcClient, reqId, handle);
	if (ret == 0)
		return 0;
	switch (ret) {
	case BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND:
		return NP_MANAGER_ERROR_REQUEST_NOT_FOUND;
	case BLOCKING_IPC_ERROR_REQUEST_BUSY:
		return NP_MANAGER_ERROR_REQUEST_BUSY;
	case BLOCKING_IPC_ERROR_REQUEST_ABORTED:
		return SCE_NP_MANAGER_ERROR_ABORTED;
	default:
		return NP_MANAGER_ERROR_REQUEST_FAILED;
	}
}

/* waitBlockingIpc; a request that times out is aborted */
__attribute__((always_inline))
static inline int npManagerWaitBlockingIpc(int reqId, const BlockingIpc::Client::Handle *handle)
{
	int ret;

	ret = SceIpmi_7E079E19(s_npManagerIpcClient, handle, NULL);
	if (ret == BLOCKING_IPC_ERROR_TIMEOUT) {
		SceIpmi_48E072C8(s_npManagerIpcClient, reqId);
		return NP_MANAGER_ERROR_REQUEST_TIMEOUT;
	}
	if (ret < 0)
		return ret;
	return 0;
}

/* endBlockingIpc; its error replaces the request's */
__attribute__((always_inline))
static inline int npManagerEndBlockingIpc(int reqId, int ret)
{
	int ret2;

	ret2 = SceIpmi_8A0BAC4E(s_npManagerIpcClient, reqId);
	if (ret2 < 0)
		return ret2;
	return ret;
}

static inline int npManagerCreateRequest(int type)
{
	int param;
	int reqId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	param = type;
	ret = SceIpmi_367CEF82(s_npManagerIpcClient, &reqId, &param, sizeof(param));
	if (ret < 0)
		return ret;
	return reqId;
}

static inline int npManagerDestroyRequest(int reqId)
{
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = SceIpmi_5ECD40E9(s_npManagerIpcClient, reqId);
	if (ret >= 0)
		return ret;
	switch (ret) {
	case BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND:
		return NP_MANAGER_ERROR_REQUEST_NOT_FOUND;
	case BLOCKING_IPC_ERROR_REQUEST_BUSY:
		return NP_MANAGER_ERROR_REQUEST_BUSY;
	default:
		return NP_MANAGER_ERROR_REQUEST_FAILED;
	}
}

static inline int npManagerAbortRequest(int reqId)
{
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = SceIpmi_48E072C8(s_npManagerIpcClient, reqId);
	if (ret >= 0)
		return ret;
	if (ret == BLOCKING_IPC_ERROR_REQUEST_NOT_FOUND)
		return NP_MANAGER_ERROR_REQUEST_NOT_FOUND;
	return NP_MANAGER_ERROR_REQUEST_FAILED;
}

extern "C" {

int npManagerIpcInit(void)
{
	void *memory;
	int ret;

	if (s_npManagerIpcClient != NULL)
		return SCE_NP_MANAGER_ERROR_ALREADY_INITIALIZED;

	BlockingIpc::Client::Config config;

	sceClibStrncpy(config.name, NP_MANAGER_SERVICE_NAME, 16);
	config.budget = IPMI::USER_BUDGET;
	config.blockingIpcCount = 4;
	config.maxBlockingInSize = 1024;
	config.maxBlockingOutSize = 12288;
	config.asyncCallCount = 1;
	config.maxAsyncCallInSize = 1024;
	config.maxAsyncCallOutSize = 1024;
	s_npManagerIpcMutex = (SceKernelLwMutexWork *)npManagerAlloc(sizeof(SceKernelLwMutexWork));
	if (s_npManagerIpcMutex == NULL) {
		ret = SCE_NP_MANAGER_ERROR_OUT_OF_MEMORY;
		goto error_client;
	}
	ret = sceKernelCreateLwMutex(s_npManagerIpcMutex, "SceNpManagerIpc",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0) {
		npManagerFree(s_npManagerIpcMutex);
		s_npManagerIpcMutex = NULL;
		goto error_client;
	}
	memory = npManagerAlloc(SceIpmi_296D44D4(&config));
	if (memory == NULL) {
		ret = SCE_NP_MANAGER_ERROR_OUT_OF_MEMORY;
		goto error_client;
	}
	ret = SceIpmi_BC3A3031(&s_npManagerIpcClient, &config, memory);
	if (ret != 0) {
		npManagerFree(memory);
		s_npManagerIpcClient = NULL;
		goto error;
	}
	ret = SceShellSvc_D943CE15(NP_MANAGER_EVENT_REQUEST, npManagerIpcEventHandler, NULL);
	if (ret < 0)
		goto error_client;
	ret = SceIpmi_2C6DB642(s_npManagerIpcClient);
	if (ret == 0)
		return 0;
error_client:
	if (s_npManagerIpcClient != NULL) {
		SceIpmi_A20BB95B(s_npManagerIpcClient);
		npManagerFree(memory);
		s_npManagerIpcClient = NULL;
	}
error:
	SceShellSvc_7CBF442B(NP_MANAGER_EVENT_REQUEST);
	if (s_npManagerIpcMutex != NULL) {
		sceKernelDeleteLwMutex(s_npManagerIpcMutex);
		npManagerFree(s_npManagerIpcMutex);
		s_npManagerIpcMutex = NULL;
	}
	return ret;
}

int npManagerIpcTerm(void)
{
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	SceShellSvc_7CBF442B(NP_MANAGER_EVENT_REQUEST);
	if (s_npManagerIpcMutex != NULL) {
		sceKernelDeleteLwMutex(s_npManagerIpcMutex);
		npManagerFree(s_npManagerIpcMutex);
		s_npManagerIpcMutex = NULL;
	}
	SceIpmi_3554D4B0(s_npManagerIpcClient);
	SceIpmi_A20BB95B(s_npManagerIpcClient);
	npManagerFree(s_npManagerIpcClient);
	s_npManagerIpcClient = NULL;
	return 0;
}

int SceNpManagerTicket_37F7F258(void)
{
	return npManagerCreateRequest(NP_MANAGER_REQUEST_TICKET);
}

int SceNpManagerTicket_2B813EC7(int reqId)
{
	return npManagerDestroyRequest(reqId);
}

int SceNpManagerTicket_A30BF29A(int reqId)
{
	return npManagerAbortRequest(reqId);
}

int SceNpManagerTicket_877251F3(int reqId, void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_877251F3(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_877251F3(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_2841C20F(int reqId, void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_2841C20F(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_2841C20F(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManagerTicket_80895A1E(int reqId, void *ticket, SceSize *pSize)
{
	BlockingIpc::Client::Handle handle;
	SceSize ticketSize;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ticketSize = 0;
	if (ticket != NULL)
		ticketSize = *pSize;
	ret = npManagerIpcStart_80895A1E(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId,
					 ticketSize);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_80895A1E(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, pSize, &ticketSize, ticket);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_E2FAC2CA(int reqId, void *ticket, SceSize *pSize)
{
	BlockingIpc::Client::Handle handle;
	SceSize ticketSize;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ticketSize = 0;
	if (ticket != NULL)
		ticketSize = *pSize;
	ret = npManagerIpcStart_E2FAC2CA(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId,
					 ticketSize);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_E2FAC2CA(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, pSize, &ticketSize, ticket);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_5EB63590(int reqId, void *ticket, SceSize *pSize)
{
	BlockingIpc::Client::Handle handle;
	SceSize ticketSize;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ticketSize = 0;
	if (ticket != NULL)
		ticketSize = *pSize;
	ret = npManagerIpcStart_5EB63590(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId,
					 ticketSize);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_5EB63590(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, pSize, &ticketSize, ticket);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_2FE3625C(int reqId, void *data1, void *data2)
{
	BlockingIpc::Client::Handle handle;
	SceUInt8 buf2[16];
	SceUInt8 buf1[464];
	int requestId;
	int ret;
	int ret2;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_2FE3625C(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId,
					 data1 != NULL, data2 != NULL);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_2FE3625C(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf1, buf2);
	}
	ret2 = SceIpmi_8A0BAC4E(s_npManagerIpcClient, reqId);
	if (ret2 < 0)
		return ret2;
	if (data1 != NULL)
		sceClibMemcpy_safe(data1, buf1, sizeof(buf1));
	if (data2 != NULL)
		sceClibMemcpy_safe(data2, buf2, sizeof(buf2));
	return ret;
}

int SceNpManager_63EF4853(int reqId, const void *data1, const void *data2)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_63EF4853(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data1, data2);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_63EF4853(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int sceNpManagerGetNpId(SceNpId *npId)
{
	if (npId == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_sceNpManagerGetNpId(SceShellSvc_B31E7F1C(), npId);
}

int SceNpManager_1796CD2E(void)
{
	return npManagerIpc_1796CD2E(SceShellSvc_B31E7F1C(), 0);
}

int SceNpManager_A775A452(void)
{
	return npManagerIpc_A775A452(SceShellSvc_B31E7F1C(), 0);
}

int SceNpManager_455E05C1(SceUInt64 value, const void *data1, const void *data2)
{
	SceUInt8 buf2[36];
	SceUInt8 buf1[68];

	if (data1 != NULL)
		sceClibMemcpy_safe(buf1, data1, sizeof(buf1));
	if (data2 != NULL)
		sceClibMemcpy_safe(buf2, data2, sizeof(buf2));
	return npManagerIpc_455E05C1(SceShellSvc_B31E7F1C(), value, buf1, data1 == NULL, buf2,
				     data2 == NULL);
}

int SceNpManager_D83C8CCB(void)
{
	return npManagerIpc_D83C8CCB(SceShellSvc_B31E7F1C(), 0);
}

int SceNpManager_E45E4003(NpManagerAccountInfo *info)
{
	return npManagerIpc_E45E4003(SceShellSvc_B31E7F1C(), info);
}

int SceNpManager_D4180D28(void)
{
	return npManagerIpc_D4180D28(SceShellSvc_B31E7F1C());
}

int SceNpManager_2EA038D8(void *ticket, SceSize *pSize)
{
	if (ticket == NULL || pSize == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_2EA038D8(SceShellSvc_B31E7F1C(), ticket, *pSize, pSize);
}

int SceNpManager_4341692A(int *value)
{
	if (value == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_4341692A(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_C80D8C3E(void *ticket, SceSize *pSize)
{
	if (pSize == NULL || ticket == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_C80D8C3E(SceShellSvc_B31E7F1C(), 0, ticket, pSize);
}

int SceNpManager_6CDB2AEC(const void *data1, const void *data2, const void *data3)
{
	SceUInt8 empty3[8];
	SceUInt8 empty2[36];
	SceUInt8 empty1[68];
	IPMI::Client *client;
	int hasData2;
	int hasData3;

	client = SceShellSvc_B31E7F1C();
	hasData2 = 0;
	hasData3 = 0;
	if (data2 != NULL)
		hasData2 = 1;
	else
		data2 = empty2;
	if (data3 != NULL)
		hasData3 = 1;
	else
		data3 = empty3;
	return npManagerIpc_6CDB2AEC(client, data1 != NULL, data1 != NULL ? data1 : empty1,
				     hasData2, data2, hasData3, data3);
}

int SceNpManager_ED12AF16(const void *data1, const void *data2, const void *data3,
			  const void *data4)
{
	SceUInt8 empty3[8];
	SceUInt8 empty4[20];
	SceUInt8 empty2[36];
	SceUInt8 empty1[68];
	IPMI::Client *client;
	int hasData2;
	int hasData3;
	int hasData4;

	client = SceShellSvc_B31E7F1C();
	hasData2 = 0;
	hasData3 = 0;
	hasData4 = 0;
	if (data2 != NULL)
		hasData2 = 1;
	else
		data2 = empty2;
	if (data3 != NULL)
		hasData3 = 1;
	else
		data3 = empty3;
	if (data4 != NULL)
		hasData4 = 1;
	else
		data4 = empty4;
	return npManagerIpc_ED12AF16(client, data1 != NULL, data1 != NULL ? data1 : empty1,
				     hasData2, data2, hasData3, data3, hasData4, data4);
}

int SceNpManager_DDF30B0E(void *data1, void *data2, void *data3)
{
	SceUInt8 empty3[8];
	SceUInt8 empty2[36];
	SceUInt8 empty1[68];
	IPMI::Client *client;
	int hasData2;
	int hasData3;

	client = SceShellSvc_B31E7F1C();
	hasData2 = 0;
	hasData3 = 0;
	if (data2 != NULL)
		hasData2 = 1;
	else
		data2 = empty2;
	if (data3 != NULL)
		hasData3 = 1;
	else
		data3 = empty3;
	return npManagerIpc_DDF30B0E(client, data1 != NULL, data1 != NULL ? data1 : empty1,
				     hasData2, data2, hasData3, data3);
}

int SceNpManager_4E94274C(void)
{
	return npManagerIpc_4E94274C(SceShellSvc_B31E7F1C());
}

int sceNpManagerGetAccountRegion(SceNpCountryCode *countryCode, int *languageCode)
{
	if (countryCode == NULL || languageCode == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_sceNpManagerGetAccountRegion(SceShellSvc_B31E7F1C(), countryCode,
							 languageCode);
}

int SceNpManager_97291707(void *data1, void *data2)
{
	if (data1 == NULL || data2 == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_97291707(SceShellSvc_B31E7F1C(), data1, data2);
}

int sceNpManagerGetContentRatingFlag(int *isRestricted, int *age)
{
	if (isRestricted == NULL || age == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_sceNpManagerGetContentRatingFlag(SceShellSvc_B31E7F1C(), isRestricted,
							     age);
}

int sceNpManagerGetChatRestrictionFlag(int *isRestricted)
{
	if (isRestricted == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_sceNpManagerGetChatRestrictionFlag(SceShellSvc_B31E7F1C(),
							       isRestricted);
}

int SceNpManager_BB384A4D(void)
{
	return npManagerIpc_BB384A4D(SceShellSvc_B31E7F1C(), 0);
}

int SceNpManager_351A74C2(NpManagerCallback func, void *arg)
{
	NpManagerCallbackEntry *entry;
	int ret;
	int id;
	int i;

	sceKernelLockLwMutex(s_npManagerIpcMutex, 1, NULL);
	for (i = 0; i < NP_MANAGER_CALLBACK_COUNT; i++) {
		if (s_npManagerCallbacks[i].func == NULL)
			break;
	}
	if (i == NP_MANAGER_CALLBACK_COUNT) {
		ret = SCE_NP_MANAGER_ERROR_OUT_OF_MEMORY;
	} else {
		entry = &s_npManagerCallbacks[i];
		entry->func = func;
		entry->arg = arg;
		ret = npManagerIpc_351A74C2(SceShellSvc_B31E7F1C(), 0);
		id = ret;
		if (ret < 0) {
			id = 0;
			entry->func = NULL;
			entry->arg = NULL;
		}
		entry->id = id;
	}
	sceKernelUnlockLwMutex(s_npManagerIpcMutex, 1);
	return ret;
}

int SceNpManager_8C6EACC4(int id)
{
	NpManagerCallbackEntry *entry;
	int ret;
	int i;

	sceKernelLockLwMutex(s_npManagerIpcMutex, 1, NULL);
	for (i = 0; i < NP_MANAGER_CALLBACK_COUNT; i++) {
		if (s_npManagerCallbacks[i].id == id)
			break;
	}
	ret = SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL;
	if (i != NP_MANAGER_CALLBACK_COUNT) {
		entry = &s_npManagerCallbacks[i];
		entry->func = NULL;
		entry->arg = NULL;
		entry->id = 0;
		ret = npManagerIpc_8C6EACC4(SceShellSvc_B31E7F1C(), id);
	}
	sceKernelUnlockLwMutex(s_npManagerIpcMutex, 1);
	return ret;
}

int SceNpManager_FFBFC540(void *buf)
{
	if (buf == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	return npManagerIpc_FFBFC540(SceShellSvc_B31E7F1C(), buf);
}

int SceNpManager_77DE17D4(const NpManagerOverride *override)
{
	if (override == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	s_npManagerOverride = override;
	return 0;
}

int SceNpManager_27112D01(void)
{
	return 0;
}

int SceNpManager_FAEA7A73(void)
{
	return 0;
}

int SceNpManager_375A8BC0(int *pValue, int value)
{
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_375A8BC0 != NULL)
		return s_npManagerOverride->func_375A8BC0(pValue, value, sceKernelGetProcessId());
	return npManagerIpc_375A8BC0(SceShellSvc_B31E7F1C(), pValue, value);
}

int SceNpManager_F35CC917(int *pValue, int value)
{
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_F35CC917 != NULL)
		return s_npManagerOverride->func_F35CC917(pValue, value, sceKernelGetProcessId());
	return npManagerIpc_F35CC917(SceShellSvc_B31E7F1C(), pValue, value);
}

int SceNpManager_E41932D4(int *pValue, int value)
{
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_E41932D4 != NULL)
		return s_npManagerOverride->func_E41932D4(pValue, value, sceKernelGetProcessId());
	return npManagerIpc_E41932D4(SceShellSvc_B31E7F1C(), pValue, value);
}

int SceNpManager_D1D191FB(int value)
{
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_D1D191FB != NULL)
		return s_npManagerOverride->func_D1D191FB(value);
	return npManagerIpc_D1D191FB(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_EB16514C(int value)
{
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_EB16514C != NULL)
		return s_npManagerOverride->func_EB16514C(value);
	return npManagerIpc_EB16514C(SceShellSvc_B31E7F1C(), value);
}

int SceNpManager_96534006(int value, int *pValue)
{
	if (pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerOverride != NULL && s_npManagerOverride->func_96534006 != NULL)
		return s_npManagerOverride->func_96534006(value, pValue);
	return npManagerIpc_96534006(SceShellSvc_B31E7F1C(), value, pValue);
}

int SceNpManager_21F1D18E(void)
{
	return npManagerCreateRequest(NP_MANAGER_REQUEST_21F1D18E);
}

int SceNpManager_A3E472B8(int reqId)
{
	return npManagerDestroyRequest(reqId);
}

int SceNpManager_6EA27D2A(int reqId)
{
	return npManagerAbortRequest(reqId);
}

int SceNpManager_FF0D6501(int reqId, const void *data, int value, void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (buf == NULL || data == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_FF0D6501(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data, value);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_FF0D6501(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_2BB7D844(int reqId, const void *data1, const void *data2, const void *data3,
			  const int *pValue1, const void *data4, int value2, const int *pValue3,
			  const void *data5, int value4, void *buf1, int *pResult1, int *pResult2,
			  void *buf2, const void *data6, SceSize data6Size)
{
	BlockingIpc::Client::Handle handle;
	SceUInt8 data2Copy[16];
	SceUInt8 data1Copy[52];
	SceUInt8 data3Copy[64];
	SceUInt8 empty4;
	SceUInt8 empty5;
	int value1;
	int value3;
	int requestId;
	int ret;

	sceClibMemset(data1Copy, 0, sizeof(data1Copy));
	sceClibMemset(data2Copy, 0, sizeof(data2Copy));
	sceClibMemset(data3Copy, 0, sizeof(data3Copy));
	sceClibMemset(&empty4, 0, sizeof(empty4));
	value1 = 0;
	sceClibMemset(&empty5, 0, sizeof(empty5));
	if (buf1 == NULL || pResult1 == NULL || pResult2 == NULL || buf2 == NULL || data6 == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	value3 = 0;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	if (data1 != NULL)
		sceClibMemcpy_safe(data1Copy, data1, sizeof(data1Copy));
	if (data2 != NULL)
		sceClibMemcpy_safe(data2Copy, data2, sizeof(data2Copy));
	if (data3 != NULL)
		sceClibMemcpy_safe(data3Copy, data3, sizeof(data3Copy));
	if (pValue1 != NULL)
		value1 = *pValue1;
	if (pValue3 != NULL)
		value3 = *pValue3;
	ret = npManagerIpcStart_2BB7D844(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data1Copy, data1 == NULL, data2Copy,
					 data2 == NULL, data3Copy, data3 == NULL, &value1,
					 pValue1 == NULL,
					 data4 != NULL ? data4 : &empty4, data4 == NULL, value2,
					 &value3, pValue3 == NULL,
					 data5 != NULL ? data5 : &empty5, data5 == NULL, value4,
					 data6, data6Size);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_2BB7D844(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf1, pResult1, pResult2, buf2);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_A5F0BD9C(int reqId, int value1, int value2, void *entries, SceUInt32 count,
			  int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (entries == NULL || pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_A5F0BD9C(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, count);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_A5F0BD9C(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, entries, pValue);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_D6C54CC2(int reqId, const int *data, int value1, int value2, int *values,
			  void *entries, SceUInt32 count, int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (data == NULL || values == NULL || entries == NULL || pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_D6C54CC2(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data, value1, value2, count);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_D6C54CC2(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, values, entries, pValue);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_89E693FB(int reqId, int value1, int value2, void *entries, SceUInt32 count,
			  int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (entries == NULL || pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_89E693FB(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, count);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_89E693FB(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, entries, pValue);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_28DED5D1(int reqId, const int *data, int value1, int value2, int *values1,
			  int *values2, void *entries, SceUInt32 count, int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (data == NULL || values1 == NULL || values2 == NULL || entries == NULL || pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_28DED5D1(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data, value1, value2, count);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_28DED5D1(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, values1, values2, entries, pValue);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_6D13F55F(int reqId, int value1, int value2, void *entries, SceUInt32 count,
			  int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (entries == NULL || pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_6D13F55F(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, count);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_6D13F55F(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, entries, pValue);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_C7F6ADC4(int reqId, const void *data, SceSize size, const int *pValue,
			  int value1, int value2, int *values, void *entries, int *values2,
			  SceUInt32 count, int *pResult1, int *pResult2)
{
	BlockingIpc::Client::Handle handle;
	SceUInt32 count2;
	int requestId;
	int ret;

	if (data == NULL || pValue == NULL || values == NULL || entries == NULL || pResult1 == NULL ||
	    pResult2 == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	count2 = 0;
	ret = npManagerIpcStart_C7F6ADC4(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data, size, pValue, value1, value2, count,
					 values2 == NULL);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0) {
			if (values2 != NULL)
				count2 = count;
			ret = npManagerIpcFinish_C7F6ADC4(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, values, entries, &count2, values2,
							  pResult1, pResult2);
		}
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_BBCD1B6D(int reqId, const void *data, SceSize size, int value1, int value2,
			  void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (data == NULL || buf == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_BBCD1B6D(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, data, size, value1, value2);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_BBCD1B6D(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_0F6AC47E(int reqId, int value1, int value2, int value3, int *pValue)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (pValue == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_0F6AC47E(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, value3);
	if (ret < 0)
		return ret;
	ret = npManagerWaitBlockingIpc(reqId, &handle);
	if (ret >= 0)
		ret = npManagerIpcFinish_0F6AC47E(SceIpmi_1372C7B7(s_npManagerIpcClient), requestId,
						  pValue);
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_D157DC12(int type)
{
	return npManagerCreateRequest(type);
}

int SceNpManager_557D5CC1(int reqId, int value1, int value2, const int *pValue, int value3,
			  int value4, int *values, void *entries, int *values2, SceUInt32 count,
			  int *pResult1, int *pResult2)
{
	BlockingIpc::Client::Handle handle;
	SceUInt32 count2;
	int requestId;
	int ret;

	if (pValue == NULL || values == NULL || entries == NULL || pResult1 == NULL || pResult2 == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	count2 = 0;
	ret = npManagerIpcStart_557D5CC1(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, pValue, value3, value4, count,
					 values2 == NULL);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0) {
			if (values2 != NULL)
				count2 = count;
			ret = npManagerIpcFinish_557D5CC1(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, &count, values, entries, &count2, values2,
							  pResult1, pResult2);
		}
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_D891AB9A(int reqId, int value1, int value2, int value3, int value4, void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (buf == NULL)
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret < 0)
		return ret;
	ret = npManagerIpcStart_D891AB9A(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle,
					 reqId, value1, value2, value3, value4);
	if (ret >= 0) {
		ret = npManagerWaitBlockingIpc(reqId, &handle);
		if (ret >= 0)
			ret = npManagerIpcFinish_D891AB9A(SceIpmi_1372C7B7(s_npManagerIpcClient),
							  requestId, buf);
	}
	return npManagerEndBlockingIpc(reqId, ret);
}

int SceNpManager_4110B787(SceNpAuthOAuthRequestId *reqId)
{
	int param;
	int id;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	param = NP_MANAGER_REQUEST_OAUTH;
	ret = SceIpmi_367CEF82(s_npManagerIpcClient, &id, &param, sizeof(param));
	if (ret >= 0)
		*reqId = id;
	return ret;
}

int SceNpManager_3907F96A(SceNpAuthOAuthRequestId reqId)
{
	return npManagerDestroyRequest(reqId);
}

int SceNpManager_737B2820(SceNpAuthOAuthRequestId reqId)
{
	return npManagerAbortRequest(reqId);
}

int SceNpManager_FDE78C74(int reqId, void *buf)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret >= 0) {
		ret = npManagerIpcStart_FDE78C74(SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId,
						 &handle, reqId);
		if (ret >= 0) {
			ret = npManagerWaitBlockingIpc(reqId, &handle);
			if (ret >= 0)
				ret = npManagerIpcFinish_FDE78C74(SceIpmi_1372C7B7(s_npManagerIpcClient),
								  requestId, buf);
		}
		SceIpmi_8A0BAC4E(s_npManagerIpcClient, reqId);
	}
	return ret;
}

int npAuthGetAuthorizationCode(SceNpAuthOAuthRequestId reqId,
			       const SceNpAuthGetAuthorizationCodeParameter *param,
			       SceNpAuthorizationCode *authCode, int *issuerId)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret >= 0) {
		ret = npManagerIpcStart_npAuthGetAuthorizationCode(SceIpmi_1372C7B7(s_npManagerIpcClient),
								   &requestId, &handle, reqId,
								   param->pClientId, param->pScope);
		if (ret >= 0) {
			ret = npManagerWaitBlockingIpc(reqId, &handle);
			if (ret >= 0)
				ret = npManagerIpcFinish_npAuthGetAuthorizationCode(
					SceIpmi_1372C7B7(s_npManagerIpcClient), requestId, authCode,
					issuerId);
		}
		SceIpmi_8A0BAC4E(s_npManagerIpcClient, reqId);
	}
	return ret;
}

int npAuthGetAuthorizationCodeEx(SceNpAuthOAuthRequestId reqId, const SceNpClientId *clientId,
				 const SceChar8 *clientSecret, const SceChar8 *scope,
				 SceNpAuthorizationCode *authCode, int *issuerId)
{
	BlockingIpc::Client::Handle handle;
	int requestId;
	int ret;

	if (s_npManagerIpcClient == NULL)
		return SCE_NP_MANAGER_ERROR_NOT_INITIALIZED;
	ret = npManagerBeginBlockingIpc(reqId, &handle);
	if (ret >= 0) {
		ret = npManagerIpcStart_npAuthGetAuthorizationCodeEx(
			SceIpmi_1372C7B7(s_npManagerIpcClient), &requestId, &handle, reqId, clientId,
			clientSecret, scope);
		if (ret >= 0) {
			ret = npManagerWaitBlockingIpc(reqId, &handle);
			if (ret >= 0)
				ret = npManagerIpcFinish_npAuthGetAuthorizationCodeEx(
					SceIpmi_1372C7B7(s_npManagerIpcClient), requestId, authCode,
					issuerId);
		}
		SceIpmi_8A0BAC4E(s_npManagerIpcClient, reqId);
	}
	return ret;
}

int SceNpManager_F66A7AAE(void)
{
	return npManagerIpc_F66A7AAE(SceIpmi_1372C7B7(s_npManagerIpcClient));
}

int SceNpManager_1F0C6314(void *buf, SceUInt8 *value)
{
	return npManagerIpc_1F0C6314(SceIpmi_1372C7B7(s_npManagerIpcClient), buf, value);
}

}

/* the NP service's event: every registered callback runs */
static int npManagerIpcEventHandler(int event, void *arg)
{
	int i;

	if (event != NP_MANAGER_EVENT_REQUEST)
		return 0;
	sceKernelLockLwMutex(s_npManagerIpcMutex, 1, NULL);
	for (i = 0; i < NP_MANAGER_CALLBACK_COUNT; i++) {
		if (s_npManagerCallbacks[i].func != NULL)
			s_npManagerCallbacks[i].func(0, s_npManagerCallbacks[i].arg);
	}
	sceKernelUnlockLwMutex(s_npManagerIpcMutex, 1);
	return 0;
}
