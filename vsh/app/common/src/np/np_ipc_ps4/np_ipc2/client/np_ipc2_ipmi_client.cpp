/*
 * The NP IPC2 client's IPMI client: one IPMI::Client of the library's own, made in memory of the
 * client's heap, connected and disconnected as a whole.
 */

#include "np_ipc2_client.h"

extern "C" {

extern const void *g_npIpc2IpmiClientVtable[];		/* 0x8102adc8 */

struct NpIpc2IpmiClient *npIpc2IpmiClientConstruct(struct NpIpc2IpmiClient *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2IpmiClientVtable);
	self->m_ipmiClient = NULL;
	self->disconnected = 1;
	self->ipmiClientBuffer = NULL;
	return self;
}

int npIpc2IpmiClientIsInit(struct NpIpc2IpmiClient *self)
{
	return self->m_ipmiClient != NULL;
}

struct NpIpc2IpmiClient *npIpc2IpmiClientDestruct(struct NpIpc2IpmiClient *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2IpmiClientVtable);
	if (self->m_ipmiClient != NULL) {
		if (self->disconnected == 0)
			self->m_ipmiClient->disconnect();
		self->m_ipmiClient->destroy();
	}
	if (self->ipmiClientBuffer != NULL)
		npIpc2ClientFree(self->ipmiClientBuffer);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2IpmiClientDelete(struct NpIpc2IpmiClient *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2IpmiClientVtable);
	if (self->m_ipmiClient != NULL) {
		if (self->disconnected == 0)
			self->m_ipmiClient->disconnect();
		self->m_ipmiClient->destroy();
	}
	if (self->ipmiClientBuffer != NULL)
		npIpc2ClientFree(self->ipmiClientBuffer);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpIpc2IpmiClient *npIpc2IpmiClientDestructBase(struct NpIpc2IpmiClient *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2IpmiClientVtable);
	if (self->m_ipmiClient != NULL) {
		if (self->disconnected == 0)
			self->m_ipmiClient->disconnect();
		self->m_ipmiClient->destroy();
	}
	if (self->ipmiClientBuffer != NULL)
		npIpc2ClientFree(self->ipmiClientBuffer);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2IpmiClientInit(struct NpIpc2IpmiClient *self, const struct NpIpc2IpmiClientConfig *config)
{
	static const char functionName[] = "Init";
	SceSize size;
	int result;

	NP_IPC2_ASSERT(self->m_ipmiClient == NULL, functionName, "np_ipc2_ipmi_client.cpp", 56, "!IsInit()");
	IPMI::Client::Config ipmiConfig;
	sceClibStrlcpy(ipmiConfig.serviceName, config->name, 17);
	ipmiConfig.syncCallCount = config->syncCallCount;
	ipmiConfig.maxSyncCallInSize = config->maxSyncCallInSize;
	ipmiConfig.maxSyncCallOutSize = config->maxSyncCallOutSize;
	ipmiConfig.asyncCallCount = config->asyncCallCount;
	ipmiConfig.maxAsyncCallInSize = config->maxAsyncCallInSize;
	ipmiConfig.maxAsyncCallOutSize = config->maxAsyncCallOutSize;
	ipmiConfig.eventFlagCount = config->eventFlagCount;
	size = IPMI::Client::getBufferSize(&ipmiConfig);
	self->ipmiClientBuffer = npIpc2ClientAlloc(size);
	if (self->ipmiClientBuffer == NULL) {
		result = NP_IPC2_ERROR_OUT_OF_MEMORY;
	} else {
		result = IPMI::Client::create(&self->m_ipmiClient, &ipmiConfig, self, self->ipmiClientBuffer);
		if (result >= 0) {
			self->disconnected = 1;
			sceClibStrlcpy(self->name, config->name, 16);
			NP_IPC2_ASSERT(self->m_ipmiClient != NULL, functionName, "np_ipc2_ipmi_client.cpp", 90, "IsInit()");
			return 0;
		}
	}
	if (self->ipmiClientBuffer != NULL) {
		npIpc2ClientFree(self->ipmiClientBuffer);
		self->ipmiClientBuffer = NULL;
	}
	return result;
}

int npIpc2IpmiClientTerm(struct NpIpc2IpmiClient *self)
{
	if (self->m_ipmiClient != NULL) {
		if (self->disconnected == 0)
			self->m_ipmiClient->disconnect();
		self->m_ipmiClient->destroy();
	}
	if (self->ipmiClientBuffer != NULL)
		npIpc2ClientFree(self->ipmiClientBuffer);
	self->m_ipmiClient = NULL;
	self->ipmiClientBuffer = NULL;
	self->disconnected = 1;
	return 0;
}

int npIpc2IpmiClientConnect(struct NpIpc2IpmiClient *self, const void *param, SceSize paramSize)
{
	static const char functionName[] = "Connect";
	int status;
	int result;

	NP_IPC2_ASSERT(self->m_ipmiClient != NULL, functionName, "np_ipc2_ipmi_client.cpp", 117, "m_ipmiClient");
	if (self->disconnected != 1)
		return NP_IPC2_ERROR_8055A307;
	result = self->m_ipmiClient->connect(param, paramSize, &status);
	if (result >= 0) {
		self->disconnected = 0;
		result = 0;
	}
	return result;
}

int npIpc2IpmiClientDisconnect(struct NpIpc2IpmiClient *self)
{
	static const char functionName[] = "Disconnect";
	int result;

	NP_IPC2_ASSERT(self->m_ipmiClient != NULL, functionName, "np_ipc2_ipmi_client.cpp", 141, "m_ipmiClient");
	if (self->disconnected != 0)
		return NP_IPC2_ERROR_8055A308;
	result = self->m_ipmiClient->disconnect();
	if (result >= 0) {
		self->disconnected = 1;
		result = 0;
	}
	return result;
}

int npIpc2IpmiClientIsConnected(struct NpIpc2IpmiClient *self)
{
	return self->disconnected == 0;
}

int npIpc2IpmiClientCall(struct NpIpc2IpmiClient *self, SceUInt32 methodId, const void *in, SceSize inSize,
	void *out, SceSize *pOutSize, SceSize outSize)
{
	SceSize received;
	int status;
	int result;

	result = self->m_ipmiClient->invokeSyncMethod(methodId, in, inSize, &status, out, &received, outSize);
	if (result >= 0) {
		result = status;
		if (status >= 0 && out != NULL)
			*pOutSize = received;
	}
	return result;
}

int npIpc2IpmiClientWaitEventFlag(struct NpIpc2IpmiClient *self, SceUInt32 index, SceUInt32 bitPattern,
	SceUInt32 waitMode, SceUInt32 *pResult, SceUInt32 *pTimeout)
{
	int result;

	result = self->m_ipmiClient->waitEventFlag(index, bitPattern, waitMode, pResult, pTimeout);
	if (result >= 0)
		return 0;
	if (result == (int)0x8002051a)
		return NP_IPC2_ERROR_TIMEOUT;
	return result;
}


const void *g_npIpc2IpmiClientVtable[] = {		/* 0x8102adc8 */
	NULL,
	NULL,
	(const void *)npIpc2IpmiClientDestruct,
	(const void *)npIpc2IpmiClientDelete,
};

}
