/*
 * The NP service state for the NP infrastructure: an IpmiWrapper client of the shell's
 * "SceNpImpsUtil" service, whose state changes reach the registered callback through a
 * message queue polled by sceNpImpsUtilityForNpInfraCheckCallback.
 */

#include "../np_manager/np_manager_internal.h"

#define NP_IMPS_SERVICE_NAME		"SceNpImpsUtil"
#define NP_IMPS_MESSAGE_QUEUE		1
#define NP_IMPS_MESSAGE_SERVICE_STATE	0

/* the assertions of this file, as Sony's build printed them */
#define NP_IMPS_SOURCE_FILE \
	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\impsutility\\np_imps_utility_for_np_infra.cpp"
#define NP_IMPS_ASSERT(cond, line, func)						\
	do {										\
		if (!(cond)) {								\
			sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n",	\
				      NP_IMPS_SOURCE_FILE, line, func);			\
			*(volatile int *)0 = 0;						\
		}									\
	} while (0)

/* the IPC requests of the service */
#define NP_IMPS_IPC_REGISTER_CALLBACK	0x90000
#define NP_IMPS_IPC_UNREGISTER_CALLBACK	0x90001
#define NP_IMPS_IPC_GET_SERVICE_STATE	0x90002
#define NP_IMPS_IPC_REQUEST_SERVICE_STATE 0x90030
#define NP_IMPS_IPC_GET_SERVICE_STATE_EX 0x90038

/* a message of the service's queue */
typedef struct NpImpsMessage {
	int type;			/* NP_IMPS_MESSAGE_* */
	SceNpServiceState state;
	int retCode;
	SceUInt8 reserved[52];
} NpImpsMessage;

/* the utility's state */
typedef struct NpImpsUtility {
	bool initialized;
	void *(*alloc)(SceSize size);
	void (*free)(void *ptr);
	void *clientMemory;
	IpmiWrapper::Client *client;
	SceKernelLwMutexWork *mutex;
	SceNpServiceStateCallback callback;
	void *callbackArg;
} NpImpsUtility;

static NpImpsUtility s_npImps;

static int npImpsIpcRegisterCallback(IPMI::Client *client, const SceUInt8 *param);
static int npImpsIpcUnregisterCallback(IPMI::Client *client);
static int npImpsIpcGetServiceState(IPMI::Client *client, SceNpServiceState *state);
static int npImpsIpcRequestServiceState(IPMI::Client *client);
static int npImpsIpcGetServiceStateEx(IPMI::Client *client, int *state);

static inline int npImpsCreateClient(const IpmiWrapper::Client::Config *config)
{
	void *memory;
	int ret;

	memory = s_npImps.alloc(SceIpmi_006EFA9D(config));
	s_npImps.clientMemory = memory;
	if (memory == NULL)
		return NP_IMPS_UTILITY_ERROR_OUT_OF_MEMORY;
	ret = SceIpmi_22625598(&s_npImps.client, config, memory);
	if (ret < 0) {
		s_npImps.free(s_npImps.clientMemory);
		return ret;
	}
	return 0;
}

extern "C" {

int sceNpImpsUtilityForNpInfraInit(const NpAllocator *allocator)
{
	IpmiWrapper::Client::Config config;
	int ret;

	if (s_npImps.initialized)
		return NP_IMPS_UTILITY_ERROR_ALREADY_INITIALIZED;
	if (allocator->alloc != NULL && allocator->free != NULL) {
		s_npImps.alloc = allocator->alloc;
		s_npImps.free = allocator->free;
	} else {
		s_npImps.alloc = npManagerAlloc;
		s_npImps.free = npManagerFree;
	}
	s_npImps.mutex = (SceKernelLwMutexWork *)s_npImps.alloc(sizeof(SceKernelLwMutexWork));
	if (s_npImps.mutex == NULL) {
		ret = NP_IMPS_UTILITY_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	ret = sceKernelCreateLwMutex(s_npImps.mutex, "SceNpImpsUtilityForNpInfraSystemMutex",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0)
		goto error_free_mutex;
	sceClibStrncpy(config.name, NP_IMPS_SERVICE_NAME, sizeof(config.name));
	config.callbackCount = 0;
	config.msgQueueCount = 2;
	config.msgQueue[0].msgSize = 1;
	config.msgQueue[0].msgCount = 1;
	config.msgQueue[1].msgSize = sizeof(NpImpsMessage);
	config.msgQueue[1].msgCount = 16;
	config.budget = 2;
	ret = npImpsCreateClient(&config);
	if (ret < 0)
		goto error_delete_mutex;
	ret = SceIpmi_B56BAC7F(s_npImps.client);
	if (ret >= 0) {
		s_npImps.initialized = true;
		return 0;
	}
	SceIpmi_D73C03D2(s_npImps.client);
	s_npImps.client = NULL;
	s_npImps.free(s_npImps.clientMemory);
error_delete_mutex:
	sceKernelDeleteLwMutex(s_npImps.mutex);
error_free_mutex:
	s_npImps.free(s_npImps.mutex);
	s_npImps.mutex = NULL;
error:
	s_npImps.alloc = NULL;
	s_npImps.free = NULL;
	return ret;
}

int sceNpImpsUtilityForNpInfraTerm(void)
{
	if (!s_npImps.initialized)
		return NP_IMPS_UTILITY_ERROR_NOT_INITIALIZED;
	sceKernelLockLwMutex(s_npImps.mutex, 1, NULL);
	s_npImps.callback = NULL;
	s_npImps.callbackArg = NULL;
	sceKernelUnlockLwMutex(s_npImps.mutex, 1);
	SceIpmi_B943F5D9(s_npImps.client);
	SceIpmi_D73C03D2(s_npImps.client);
	s_npImps.client = NULL;
	s_npImps.free(s_npImps.clientMemory);
	sceKernelDeleteLwMutex(s_npImps.mutex);
	s_npImps.mutex = NULL;
	s_npImps.alloc = NULL;
	s_npImps.free = NULL;
	s_npImps.initialized = false;
	return 0;
}

SceBool sceNpImpsUtilityForNpInfraIsInitialized(void)
{
	return s_npImps.initialized != 0;
}

int sceNpImpsUtilityForNpInfraRegisterServiceStateCallback(SceNpServiceStateCallback callback,
							    void *userdata)
{
	SceUInt8 param[14];
	int ret;
	int i;

	for (i = 0; i < 14; i++)
		param[i] = 0;
	ret = sceKernelLockLwMutex(s_npImps.mutex, 1, NULL);
	if (ret >= 0) {
		s_npImps.callback = callback;
		s_npImps.callbackArg = userdata;
		ret = sceKernelUnlockLwMutex(s_npImps.mutex, 1);
		if (ret >= 0) {
			param[0] = 1;
			ret = npImpsIpcRegisterCallback(SceIpmi_9EE9496B(s_npImps.client), param);
		}
		if (ret < 0) {
			s_npImps.callback = NULL;
			s_npImps.callbackArg = NULL;
		} else {
			ret = 0;
		}
	}
	return ret;
}

int sceNpImpsUtilityForNpInfraUnregisterServiceStateCallback(void)
{
	int ret;

	npImpsIpcUnregisterCallback(SceIpmi_9EE9496B(s_npImps.client));
	ret = sceKernelLockLwMutex(s_npImps.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	s_npImps.callback = NULL;
	s_npImps.callbackArg = NULL;
	return sceKernelUnlockLwMutex(s_npImps.mutex, 1);
}

}

/* runs the callback for every service state message in the queue */
static inline void try_get_message(void)
{
	SceSize size;
	NpImpsMessage msg;
	IPMI::Client *client;

	size = 0;
	NP_IMPS_ASSERT(s_npImps.client != NULL, 320, "try_get_message");
	for (;;) {
		client = SceIpmi_9EE9496B(s_npImps.client);
		if (client->tryGetMsg(NP_IMPS_MESSAGE_QUEUE, &msg, &size, sizeof(msg)) != 0)
			break;
		if (msg.type != NP_IMPS_MESSAGE_SERVICE_STATE)
			continue;
		NP_IMPS_ASSERT(size == 12, 336, "try_get_message");
		if (s_npImps.callback != NULL)
			s_npImps.callback(msg.state, msg.retCode, s_npImps.callbackArg);
	}
}

extern "C" {

int sceNpImpsUtilityForNpInfraCheckCallback(void)
{
	int ret;

	ret = sceKernelLockLwMutex(s_npImps.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (s_npImps.initialized)
		try_get_message();
	else
		ret = NP_IMPS_UTILITY_ERROR_NOT_INITIALIZED;
	sceKernelUnlockLwMutex(s_npImps.mutex, 1);
	return ret;
}

int sceNpImpsUtilityForNpInfraGetServiceState(SceNpServiceState *state)
{
	return npImpsIpcGetServiceState(SceIpmi_9EE9496B(s_npImps.client), state);
}

int sceNpImpsUtilityForNpInfraRequestServiceState(void)
{
	return npImpsIpcRequestServiceState(SceIpmi_9EE9496B(s_npImps.client));
}

int sceNpImpsUtilityForNpInfraGetServiceStateEx(int *state)
{
	return npImpsIpcGetServiceStateEx(SceIpmi_9EE9496B(s_npImps.client), state);
}

}

__attribute__((noinline))
static int npImpsIpcRegisterCallback(IPMI::Client *client, const SceUInt8 *param)
{
	int result;
	IPMI::DataInfo in[1];
	int ret;

	result = 0;
	in[0].pData = param;
	in[0].dataSize = 14;
	ret = client->invokeSyncMethod(NP_IMPS_IPC_REGISTER_CALLBACK, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npImpsIpcUnregisterCallback(IPMI::Client *client)
{
	int result;
	int ret;

	result = 0;
	ret = client->invokeSyncMethod(NP_IMPS_IPC_UNREGISTER_CALLBACK, (const IPMI::DataInfo *)NULL,
				       0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npImpsIpcGetServiceState(IPMI::Client *client, SceNpServiceState *state)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = state;
	out[0].bufferSize = sizeof(*state);
	ret = client->invokeSyncMethod(NP_IMPS_IPC_GET_SERVICE_STATE, (const IPMI::DataInfo *)NULL,
				       0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npImpsIpcRequestServiceState(IPMI::Client *client)
{
	int result;
	int ret;

	result = 0;
	ret = client->invokeSyncMethod(NP_IMPS_IPC_REQUEST_SERVICE_STATE,
				       (const IPMI::DataInfo *)NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

__attribute__((noinline))
static int npImpsIpcGetServiceStateEx(IPMI::Client *client, int *state)
{
	int result;
	IPMI::BufferInfo out[1];
	int ret;

	result = 0;
	out[0].pBuffer = state;
	out[0].bufferSize = sizeof(*state);
	ret = client->invokeSyncMethod(NP_IMPS_IPC_GET_SERVICE_STATE_EX,
				       (const IPMI::DataInfo *)NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}
