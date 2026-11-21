/*
 * mtp_client_lib.cpp: the MTP client (0x81137A18-0x81138994; the file's name is ours, no
 * record names it): two IPMI connections to SceMtpService, one for the calls and one for
 * the service's callbacks, a thread that hands the service's messages on, and the calls
 * MTPR2 downloads with. Sony's library has many more calls (its messages are still in the
 * shell's strings); the shell keeps only these.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <net/errno.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>

#include "../../../../lowlayer/src/ipmi/ipmi_error.h"
#include "mtp_client_lib.h"

/* Sony's message: the component, the reporting function and Sony's source line */
#define MTP_CLIENT_PRINT_ERROR(line, fmt, ...)					\
	sceClibPrintf("[MTP %s] Error %s(%u): " fmt, "CLIENT", __FUNCTION__, line,	\
		      ##__VA_ARGS__)

/* SceMtpService's methods (PLACEHOLDER names, after the calls that use them) */
#define MTP_SERVICE_METHOD_OPEN_SESSION			0x12340000
#define MTP_SERVICE_METHOD_CLOSE_SESSION		0x12340001
#define MTP_SERVICE_METHOD_BEGIN_HTTP_GET_DATA_WITH_URL	0x12340018
#define MTP_SERVICE_METHOD_END_HTTP_GET_DATA_WITH_URL	0x12340019
#define MTP_SERVICE_METHOD_BEGIN_RECV_DATA		0x12340036
#define MTP_SERVICE_METHOD_END_RECV_DATA		0x12340037
#define MTP_SERVICE_METHOD_ABORT			0x1234003C
#define MTP_SERVICE_METHOD_GET_STATUS			0x1234003E
#define MTP_SERVICE_METHOD_CANCEL			0x12340040
#define MTP_SERVICE_METHOD_5F				0x1234005F
#define MTP_SERVICE_METHOD_60				0x12340060
#define MTP_SERVICE_METHOD_61				0x12340061

#define MTP_CLIENT_OBSERVER_PRIORITY	89
#define MTP_CLIENT_OBSERVER_STACK_SIZE	0x8000
#define MTP_CLIENT_WORKER_WAIT_US	5000000
#define MTP_CLIENT_WORKER_RUNNING	0x1	/* SceMtpServiceWorkerRunning's bits */
#define MTP_CLIENT_WORKER_BUSY		0x2

/* what the service sends the observer */
struct MtpClientMsg {
	int type;		/* the listener it goes to */
	char data[108];
};

/* what BeginHttpGetDataWithUrl sends ahead of the URL */
struct HttpGetDataParam {
	SceSize size;
	int reserved;		/* PLACEHOLDER: not set */
	SceInt64 offset;
};

/* what BeginRecvData sends */
struct RecvDataParam {
	int requestId;
	SceSize size;
};

/* the two values a call sends (PLACEHOLDER) */
struct MtpClientPairParam {
	int first;
	int second;
};

static __attribute__((noinline)) int Bgdl_81138598(IPMI::Client *client, int *session);
static __attribute__((noinline)) int Bgdl_811385E8(IPMI::Client *client);
static __attribute__((noinline)) int Bgdl_81138630(IPMI::Client *client, int *requestId,
			 const IPMI::EventFlagInfo *eventFlag, const char *url, SceSize size,
			 SceInt64 offset);
static __attribute__((noinline)) int Bgdl_811386B8(IPMI::Client *client, int requestId);
static __attribute__((noinline)) int Bgdl_811386FA(IPMI::Client *client, int *requestId,
			 const IPMI::EventFlagInfo *eventFlag, int id, SceSize size);
static __attribute__((noinline)) int Bgdl_8113874A(IPMI::Client *client, int requestId, void *buf, SceSize *received,
			 SceUInt64 *remaining);
static __attribute__((noinline)) int Bgdl_811387A8(IPMI::Client *client, int requestId, int mode);
static __attribute__((noinline)) int Bgdl_811387FC(IPMI::Client *client, MtpClientStatus *status);
static __attribute__((noinline)) int Bgdl_8113884C(IPMI::Client *client, int requestId, int session);
static __attribute__((noinline)) int Bgdl_811388A0(IPMI::Client *client, int *mode);
static __attribute__((noinline)) int Bgdl_811388F0(IPMI::Client *client, int arg);
static __attribute__((noinline)) int Bgdl_81138940(IPMI::Client *client);
static SceInt32 observerThreadEntry(SceSize argSize, void *argp);

int MtpClient::finalizeClient(void)
{
	int ret;

	m_observerRunning = 0;
	if (m_client->disconnect() != 0)
		m_client->terminateConnection();
	m_client->destroy();
	m_callbackClient->disconnect();
	m_callbackClient->destroy();
	if (m_observerThread != -1) {
		ret = sceKernelWaitThreadEnd(m_observerThread, NULL, NULL);
		if (ret < 0)
			MTP_CLIENT_PRINT_ERROR(326, "failed sceKernelWaitThreadEnd 0x%x", ret);
		ret = sceKernelDeleteThread(m_observerThread);
		if (ret < 0)
			MTP_CLIENT_PRINT_ERROR(330, "failed sceKernelDeleteThread 0x%x", ret);
		m_observerThread = -1;
		m_listeners[0] = NULL;
	}
	sce_paf_free(m_client);
	m_client = NULL;
	sce_paf_free(m_callbackClient);
	m_callbackClient = NULL;
	m_session = -1;
	return 0;
}

int MtpClient::Bgdl_81137ADE(void)
{
	if (m_client == NULL || m_callbackClient == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	Bgdl_811385E8(m_client);
	Bgdl_811385E8(m_callbackClient);
	m_contexts.Finalize();
	return finalizeClient();
}

MtpClient::~MtpClient()
{
	if (m_client != NULL && m_callbackClient != NULL) {
		Bgdl_811385E8(m_client);
		Bgdl_811385E8(m_callbackClient);
		m_contexts.Finalize();
		finalizeClient();
	}
}

int MtpClient::initializeClient(IPMI::Client **client, IPMI::Client::Config config)
{
	IPMI::Client *ipmi;
	int result;
	void *mem;
	int ret;

	if (client == NULL)
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	if (*client != NULL)
		return MTP_CLIENT_ERROR_ALREADY_INITIALIZED;
	ipmi = NULL;
	mem = sce_paf_malloc(IPMI::Client::getBufferSize(&config));
	if (mem == NULL) {
		MTP_CLIENT_PRINT_ERROR(211, "no memory\n");
		return MTP_CLIENT_ERROR_OUT_OF_MEMORY;
	}
	ret = IPMI::Client::create(&ipmi, &config, this, mem);
	if (ret != 0) {
		sce_paf_free(mem);
		return ret;
	}
	ret = ipmi->connect(NULL, 0, &result);
	if (ret != 0) {
		MTP_CLIENT_PRINT_ERROR(224, "Connect failed: 0x%x\n", ret);
		ipmi->destroy();
		sce_paf_free(mem);
		return ret;
	}
	*client = ipmi;
	if (result != 0) {
		MTP_CLIENT_PRINT_ERROR(233, "ServerResult: 0x%x\n", result);
		finalizeClient();
		return result;
	}
	return 0;
}

int MtpClient::Bgdl_81137C7C(IPMI::Client::Config *config, int callback)
{
	if (config == NULL)
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	if (callback != 0) {
		sce_paf_strncpy(config->serviceName, "SceMtpServiceCa", sizeof(config->serviceName));
	} else {
		sce_paf_strncpy(config->serviceName, "SceMtpService", sizeof(config->serviceName));
		config->maxSyncCallInSize += 0x10000;
		config->maxSyncCallOutSize += 0x10000;
		config->asyncCallCount = 3;
		config->maxAsyncCallInSize += 0x10000;
		config->maxAsyncCallOutSize += 0x10000;
	}
	config->eventFlagCount = 3;
	config->msgQueueCount = 2;
	config->budget = IPMI::USER_BUDGET;
	config->msgQueueBufferSize[0] = 256;
	config->msgQueueBufferSize[1] = 40960;
	return 0;
}

/* the status observer started */
inline int MtpClient::startObserver(SceKernelThreadOptParam *opt, MtpClient **arg, SceUID *thread)
{
	int ret;

	sce_paf_memset(opt, 0, sizeof(*opt));
	*thread = sceKernelCreateThread("SceMtpStatusObserver", observerThreadEntry,
					MTP_CLIENT_OBSERVER_PRIORITY, MTP_CLIENT_OBSERVER_STACK_SIZE, 0, 0,
					opt);
	if (*thread < 0) {
		finalizeClient();
		return MTP_CLIENT_ERROR_THREAD;
	}
	m_observerThread = *thread;
	m_observerRunning = 1;
	*arg = this;
	ret = 0;
	if (sceKernelStartThread(*thread, sizeof(*arg), arg) < 0) {
		sceKernelDeleteThread(m_observerThread);
		m_observerThread = -1;
		finalizeClient();
		ret = MTP_CLIENT_ERROR_THREAD;
	}
	return ret;
}

int MtpClient::Bgdl_81137CF0(void)
{
	IPMI::Client::Config config;
	SceKernelThreadOptParam opt;
	int session;
	MtpClient *arg;
	SceUID thread;
	int ret;

	ret = Bgdl_81137C7C(&config, 0);
	if (ret >= 0) {
		ret = initializeClient(&m_client, config);
		if (ret >= 0) {
			IPMI::Client::Config callbackConfig;

			ret = Bgdl_81137C7C(&callbackConfig, 1);
			if (ret >= 0) {
				ret = initializeClient(&m_callbackClient, callbackConfig);
				if (ret >= 0) {
					ret = startObserver(&opt, &arg, &thread);
					if (ret >= 0) {
						ret = m_contexts.Initialize(1, 2);
						if (ret < 0) {
							finalizeClient();
							return ret;
						}
						ret = Bgdl_81138598(m_client, &m_session);
						if (ret < 0) {
							m_contexts.Finalize();
							finalizeClient();
							return ret;
						}
						session = 0;
						ret = Bgdl_81138598(m_callbackClient, &session);
						if (ret < 0) {
							m_contexts.Finalize();
							finalizeClient();
						}
					}
				}
			}
		}
	}
	return ret;
}

int MtpClient::Bgdl_81138068(int handle, int mode)
{
	int requestId;
	int ret;

	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	ret = m_contexts.Get(handle, &requestId);
	if (ret < 0)
		return ret;
	m_ended = 1;
	return Bgdl_811387A8(m_client, requestId, mode);
}

int MtpClient::BeginHttpGetDataWithUrl(const char *url, SceSize size, SceInt64 offset,
				       int *handle)
{
	int id;
	int requestId;
	IPMI::EventFlagInfo eventFlag;
	int ret;

	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	if (handle == NULL)
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	ret = m_contexts.Create(&id, &eventFlag);
	if (ret < 0)
		return ret;
	ret = Bgdl_81138630(m_client, &requestId, &eventFlag, url, size, offset);
	if (ret >= 0) {
		m_contexts.Set(id, requestId);
		*handle = id;
		m_ended = 0;
	} else {
		MTP_CLIENT_PRINT_ERROR(1071, "BeginHttpGetDataWithUrl failed: 0x%x\n", ret);
		m_contexts.Delete(id);
	}
	return ret;
}

int MtpClient::EndHttpGetDataWithUrl(int handle)
{
	int resultId;
	int requestId;
	int ret;

	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	if (m_ended == 0 && m_contexts.Get(handle, &requestId) >= 0) {
		m_ended = 1;
		Bgdl_811387A8(m_client, requestId, 0x2001);
	}
	resultId = -1;
	ret = m_contexts.WaitResult(m_client, handle, &resultId);
	if (ret < 0)
		return ret;
	ret = Bgdl_811386B8(m_client, resultId);
	if (ret < 0 && ret != MTP_SERVICE_ERROR_8054001A) {
		MTP_CLIENT_PRINT_ERROR(1102, "EndHttpGetDataWithUrl failed: 0x%x\n", ret);
		if (ret == MTP_SERVICE_ERROR_80540040 || ret == MTP_SERVICE_ERROR_80540016)
			ret = SCE_NET_ERROR_EIPADDRCHANGED;
	}
	m_contexts.Delete(handle);
	return ret;
}

int MtpClient::RecvData(int handle, void *buf, SceSize size, SceSize *received,
			SceUInt64 *remaining)
{
	int requestId;
	int recvRequestId;
	SceUInt32 bits;
	SceUInt32 timeout;
	IPMI::EventFlagInfo eventFlag;
	SceUID evf;
	int ret;
	int wait;

	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	ret = m_contexts.Get(handle, &requestId);
	if (ret < 0)
		return ret;
	ret = m_contexts.CreateSendRecvEvent(handle, &eventFlag);
	if (ret < 0)
		return ret;
	evf = sceKernelOpenEventFlag("SceMtpServiceWorkerRunning");
	if (evf < 0) {
		MTP_CLIENT_PRINT_ERROR(1987, "sceKernelOpenEventFlag failed: 0x%x\n", evf);
		return evf;
	}
	for (;;) {
		bits = 0;
		timeout = MTP_CLIENT_WORKER_WAIT_US;
		ret = sceKernelWaitEventFlag(evf, MTP_CLIENT_WORKER_RUNNING | MTP_CLIENT_WORKER_BUSY,
					     SCE_KERNEL_EVF_WAITMODE_OR, &bits, &timeout);
		if (ret < 0) {
			MTP_CLIENT_PRINT_ERROR(1995, "sceKernelWaitEventFlag failed: 0x%x\n", ret);
			break;
		}
		if ((bits & MTP_CLIENT_WORKER_BUSY) == 0 || (bits & MTP_CLIENT_WORKER_RUNNING) != 0)
			break;
		sceKernelClearEventFlag(evf, ~MTP_CLIENT_WORKER_BUSY);
	}
	sceKernelCloseEventFlag(evf);
	if (ret < 0)
		return ret;
	ret = Bgdl_811386FA(m_client, &recvRequestId, &eventFlag, requestId, size);
	if (ret < 0) {
		MTP_CLIENT_PRINT_ERROR(2011, "BeginRecvData failed: 0x%x\n", ret);
		return ret;
	}
	*received = size;
	wait = m_contexts.WaitSendRecvResult(m_client, handle);
	if (wait < 0) {
		Bgdl_8113874A(m_client, recvRequestId, buf, received, remaining);
		return wait;
	}
	ret = Bgdl_8113874A(m_client, recvRequestId, buf, received, remaining);
	if (ret < 0 && ret != MTP_SERVICE_ERROR_80540014)
		MTP_CLIENT_PRINT_ERROR(2027, "EndRecvData failed: 0x%x\n", ret);
	return ret;
}

int MtpClient::Bgdl_81138432(MtpClientStatus *status)
{
	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	return Bgdl_811387FC(m_client, status);
}

int MtpClient::Bgdl_8113844A(int handle)
{
	int requestId;
	int ret;

	if (m_callbackClient == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	ret = m_contexts.Get(handle, &requestId);
	if (ret < 0)
		return ret;
	return Bgdl_8113884C(m_callbackClient, requestId, m_session);
}

int MtpClient::Bgdl_81138498(int *mode)
{
	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	return Bgdl_811388A0(m_client, mode);
}

int MtpClient::Bgdl_811384AE(int arg)
{
	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	return Bgdl_811388F0(m_client, arg);
}

int MtpClient::Bgdl_811384C4(void)
{
	if (m_client == NULL)
		return MTP_CLIENT_ERROR_NOT_INITIALIZED;
	return Bgdl_81138940(m_client);
}

/* the status observer: hands each of the service's messages to the listener of its type */
static SceInt32 observerThreadEntry(SceSize argSize, void *argp)
{
	MtpClient *client = *(MtpClient **)argp;
	SceSize size;
	MtpClientMsg msg;
	MtpClientEventListener *listener;
	int ret;

	if (client->m_observerRunning == 0)
		return 0;
	do {
		size = 4;
		ret = client->m_client->getMsg(0, &msg, &size, sizeof(msg), NULL);
		if (ret != 0) {
			if (ret != IPMI_ERROR_TERMINATED)
				MTP_CLIENT_PRINT_ERROR(100, "failed to get message fregisterBgCopyMtpTaskrom server 0x%x\n",
						       ret);
			return 0;
		}
		switch (msg.type) {
		case 0:
			listener = client->m_listeners[0];
			if (listener != NULL)
				listener->onEvent(msg.data);
			break;
		case 1:
			listener = client->m_listeners[1];
			if (listener != NULL)
				listener->onEvent(msg.data);
			break;
		case 2:
			listener = client->m_listeners[2];
			if (listener != NULL)
				listener->onEvent(msg.data);
			break;
		}
	} while (client->m_observerRunning != 0);
	return 0;
}

/* the bytes before the terminator, counted inline */
static inline SceSize stringLength(const char *str)
{
	SceSize length = 0;

	while (*str != '\0') {
		str++;
		length++;
	}
	return length;
}

static __attribute__((noinline)) int Bgdl_81138598(IPMI::Client *client, int *session)
{
	int result = 0;
	IPMI::BufferInfo out;
	int ret;

	out.pBuffer = session;
	out.bufferSize = sizeof(*session);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_OPEN_SESSION, NULL, 0, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811385E8(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_CLOSE_SESSION, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_81138630(IPMI::Client *client, int *requestId,
			 const IPMI::EventFlagInfo *eventFlag, const char *url, SceSize size,
			 SceInt64 offset)
{
	HttpGetDataParam param;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &param;
	in[0].dataSize = sizeof(param);
	in[1].pData = url;
	param.size = size;
	in[1].dataSize = stringLength(url) + 1;
	param.offset = offset;
	ret = client->invokeAsyncMethod(MTP_SERVICE_METHOD_BEGIN_HTTP_GET_DATA_WITH_URL, in, 2,
					requestId, eventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int Bgdl_811386B8(IPMI::Client *client, int requestId)
{
	int result = 0;
	int ret;

	ret = client->tryGetResult(MTP_SERVICE_METHOD_END_HTTP_GET_DATA_WITH_URL, requestId,
				   &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811386FA(IPMI::Client *client, int *requestId,
			 const IPMI::EventFlagInfo *eventFlag, int id, SceSize size)
{
	RecvDataParam param;
	IPMI::DataInfo in;
	int ret;

	param.requestId = id;
	param.size = size;
	in.pData = &param;
	in.dataSize = sizeof(param);
	ret = client->invokeAsyncMethod(MTP_SERVICE_METHOD_BEGIN_RECV_DATA, &in, 1, requestId,
					eventFlag);
	if (ret != 0)
		return ret;
	return 0;
}

static __attribute__((noinline)) int Bgdl_8113874A(IPMI::Client *client, int requestId, void *buf, SceSize *received,
			 SceUInt64 *remaining)
{
	int result = 0;
	IPMI::BufferInfo out[3];
	int ret;

	out[0].pBuffer = buf;
	out[0].bufferSize = *received;
	out[1].pBuffer = received;
	out[1].bufferSize = sizeof(*received);
	out[2].pBuffer = remaining;
	out[2].bufferSize = sizeof(*remaining);
	ret = client->tryGetResult(MTP_SERVICE_METHOD_END_RECV_DATA, requestId, &result, out, 3);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811387A8(IPMI::Client *client, int requestId, int mode)
{
	int result = 0;
	MtpClientPairParam param;
	IPMI::DataInfo in;
	int ret;

	param.first = requestId;
	param.second = mode;
	in.pData = &param;
	in.dataSize = sizeof(param);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_ABORT, &in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811387FC(IPMI::Client *client, MtpClientStatus *status)
{
	int result = 0;
	IPMI::BufferInfo out;
	int ret;

	out.pBuffer = status;
	out.bufferSize = sizeof(*status);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_GET_STATUS, NULL, 0, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_8113884C(IPMI::Client *client, int requestId, int session)
{
	int result = 0;
	MtpClientPairParam param;
	IPMI::DataInfo in;
	int ret;

	param.first = requestId;
	param.second = session;
	in.pData = &param;
	in.dataSize = sizeof(param);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_CANCEL, &in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811388A0(IPMI::Client *client, int *mode)
{
	int result = 0;
	IPMI::BufferInfo out;
	int ret;

	out.pBuffer = mode;
	out.bufferSize = sizeof(*mode);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_5F, NULL, 0, &result, &out, 1);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_811388F0(IPMI::Client *client, int arg)
{
	int result = 0;
	int value = arg;
	IPMI::DataInfo in;
	int ret;

	in.pData = &value;
	in.dataSize = sizeof(value);
	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_60, &in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

static __attribute__((noinline)) int Bgdl_81138940(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(MTP_SERVICE_METHOD_61, NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
