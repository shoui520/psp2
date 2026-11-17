/*
 * SceNpCommonPs4, NpService: the client of the NP service (SceNpService) over IPMI, started with the services
 * the library's callers talk to (push notifications, game presence, the NpAsm client), and the exports that start
 * and stop those services and send their requests.
 */

#include "np_ipc2/client/np_ipc2_client.h"

extern "C" {

struct NpIpc2ServiceClient *g_npServiceClient;		/* 0x8102b124 */

/* the services: ID, name, number of event flags, event queue item size and attributes, number of requests */
struct NpIpc2ServiceConfig g_npServiceConfigs[] = {		/* 0x8102b094 */
	{ 0, "SceNpPush", 1, 0, 0, 0 },
	{ 1, "SceNpGamePrese", 1, 0, 0, 32 },
	{ 2, "SceNpAsmClient", 1, 0, 0, 0 },
	{ -1 },
};

struct NpCommonPs4Allocator *SceNpCommonPs4_53484B79(void);

int SceNpCommonPs4_CC43F27D(void)
{
	struct NpIpc2ServiceClientConfig config;
	struct NpIpc2ServiceClient *client;
	void *memory;
	int result;
	int error;

	result = npIpc2ClientInit();
	if (result < 0)
		return result;
	config.name[0] = '\0';
	config.syncCallCount = 0;
	config.maxSyncCallInSize = 0;
	config.maxSyncCallOutSize = 0;
	config.asyncCallCount = 0;
	config.maxAsyncCallInSize = 0;
	config.maxAsyncCallOutSize = 0;
	config.numIpcSlot = 0;
	sceClibStrlcpy(config.name, "SceNpService", 16);
	config.syncCallCount = 8;
	config.maxSyncCallInSize = 0x4000;
	config.maxSyncCallOutSize = 0x4000;
	config.asyncCallCount = 8;
	config.maxAsyncCallInSize = 0x4000;
	config.maxAsyncCallOutSize = 0x4000;
	config.numIpcSlot = 4;
	memory = SceNpCommonPs4_0C922423(188, SceNpCommonPs4_53484B79());
	client = NULL;
	if (memory != NULL)
		client = npIpc2ServiceClientConstruct((struct NpIpc2ServiceClient *)memory);
	g_npServiceClient = client;
	if (client == NULL)
		return NP_COMMON_PS4_ERROR_80550004;
	error = npIpc2ServiceClientInit(client, &config);
	if (error >= 0) {
		error = npIpc2ServiceClientAddServices(g_npServiceClient, g_npServiceConfigs);
		if (error >= 0) {
			error = npIpc2ServiceClientConnect(g_npServiceClient, NULL, 0);
			if (error >= 0)
				return 0;
		}
		npIpc2ServiceClientTerm(g_npServiceClient);
	}
	npIpc2ClientTerm();
	return error;
}

int SceNpCommonPs4_8CE077C7(void)
{
	npIpc2ServiceClientDisconnect(g_npServiceClient);
	npIpc2ServiceClientTerm(g_npServiceClient);
	if (g_npServiceClient != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(g_npServiceClient, 1))(g_npServiceClient);
	g_npServiceClient = NULL;
	npIpc2ClientTerm();
	return 0;
}

IPMI::Client *SceNpCommonPs4_A0599F24(void)
{
	return g_npServiceClient->ipmiClient.m_ipmiClient;
}

int SceNpCommonPs4_D84BF4B7(int serviceId)
{
	return npIpc2ServiceClientStartService(g_npServiceClient, serviceId);
}

int SceNpCommonPs4_1096EAD4(int serviceId)
{
	return npIpc2ServiceClientStopService(g_npServiceClient, serviceId);
}

int SceNpCommonPs4_11DAC1B9(int serviceId)
{
	return npIpc2ServiceClientIsStarted(g_npServiceClient, serviceId);
}

int SceNpCommonPs4_1409BA27(int serviceId, SceSize size, const void *data, int *pReqId)
{
	return npIpc2ServiceClientCreateRequest(g_npServiceClient, pReqId, serviceId, data, size);
}

int SceNpCommonPs4_1D2FB0F9(int serviceId, int reqId)
{
	return npIpc2ServiceClientDeleteRequest(g_npServiceClient, serviceId, reqId);
}

int SceNpCommonPs4_CE924EB3(int serviceId, int reqId)
{
	return npIpc2ServiceClientAbortRequest(g_npServiceClient, serviceId, reqId);
}

int SceNpCommonPs4_CB9970ED(int serviceId, int reqId)
{
	return npIpc2ServiceClientBeginRequest(g_npServiceClient, serviceId, reqId);
}

int SceNpCommonPs4_363B3916(int serviceId, int reqId, SceUInt32 timeout)
{
	return npIpc2ServiceClientWaitRequest(g_npServiceClient, serviceId, reqId, timeout);
}

int SceNpCommonPs4_E1418F84(int serviceId, int reqId)
{
	return npIpc2ServiceClientEndRequest(g_npServiceClient, serviceId, reqId);
}

int SceNpCommonPs4_DE3D4E8E(int serviceId, void *buffer, SceSize size)
{
	return npIpc2ServiceClientPollEventQueue(g_npServiceClient, serviceId, buffer, size);
}

int SceNpCommonPs4_6B1D3984(int serviceId, SceUInt32 evfIndex, SceUInt32 bits, int reserved, SceUInt32 waitMode,
	SceUInt64 *pResult)
{
	return npIpc2ServiceClientPollEventFlag(g_npServiceClient, serviceId, evfIndex, waitMode, bits, reserved,
		waitMode, pResult);
}

}
