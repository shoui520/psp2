/*
 * SceNpImpsUtil: the raw message client, shared by the NP libraries that talk to the NP service
 * over IPMI. Sony's build path names it vsh/app/common/src/np/impsutility/raw_message_client.cpp.
 */
#include "raw_message_client.h"

#define NP_IMPS_RAW_MSG_ERROR_INVALID_ARGUMENT	-2141873663	/* 0x80552d01 */
#define NP_IMPS_RAW_MSG_ERROR_INVALID_MESSAGE	-2141873657	/* 0x80552d07 */
#define NP_IMPS_RAW_MSG_ERROR_NO_RECEIVER	-2141873656	/* 0x80552d08 */
#define NP_IMPS_RAW_MSG_ERROR_OUT_OF_MEMORY	-2141906175	/* 0x80551b01 */
#define NP_IMPS_RAW_MSG_ERROR_ALREADY_CREATED	-2141906173	/* 0x80551b03 */
#define NP_IMPS_RAW_MSG_ERROR_NOT_CREATED	-2141906172	/* 0x80551b04 */
#define NP_IMPS_RAW_MSG_ERROR_MSPACE		-2141906167	/* 0x80551b09 */


NpImpsRawMsgClient *s_client;		/* 0x81007008 */
int s_memBlock;			/* 0x8100700c */
void *s_mspace;			/* 0x81007010 */

static NpImpsRawMsgClient *npImpsPlacement(SceSize size, void *where) __attribute__((noinline));
static NpImpsRawMsgClient *npImpsPlacement(SceSize size, void *where)
{
	return (NpImpsRawMsgClient *)where;
}

static inline int createMemBlockAndMspace(void) __attribute__((always_inline));
static inline int createMemBlockAndMspace(void)
{
	void *base;
	int result;

	base = NULL;
	if (s_memBlock != 0) {
		sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n",
		    "W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\impsutility\\raw_message_client.cpp", 78,
		    "create_mem_block_and_mspace");
		*(volatile int *)0 = 0;
	}
	result = sceKernelAllocMemBlock("SceNpImpsUtilRawMsgClient", 0xC20D060, 0x1000, NULL);
	if (result >= 0) {
		s_memBlock = result;
		result = sceKernelGetMemBlockBase(result, &base);
		if (result >= 0) {
			s_mspace = sceClibMspaceCreate(base, 0x1000);
			if (s_mspace != NULL)
				return 0;
			result = NP_IMPS_RAW_MSG_ERROR_MSPACE;
		}
		sceKernelFreeMemBlock(s_memBlock);
	}
	s_memBlock = 0;
	return result;
}

int npImpsRawMsgClientCreate(int type, SceSize maxSize, int queueCount)
{
	NpImpsRawMsgClient *client;
	void *p;
	int result;

	if (s_client != NULL) {
		result = NP_IMPS_RAW_MSG_ERROR_ALREADY_CREATED;
	} else {
		IpmiWrapper::Client::Config config;

		result = createMemBlockAndMspace();
		if (result >= 0) {
			p = sceClibMspaceMalloc(s_mspace, sizeof(NpImpsRawMsgClient));
			if (p == NULL) {
				result = NP_IMPS_RAW_MSG_ERROR_OUT_OF_MEMORY;
			} else {
				client = npImpsPlacement(sizeof(NpImpsRawMsgClient), p);
				if (client != NULL)
					s_client = client;
				else
					s_client = NULL;
				sceClibStrncpy(config.name, "SceNpImpsUtil", sizeof(config.name));
				config.callbackCount = 0;
				config.msgQueueCount = 3;
				config.msgQueue[0].msgSize = 1;
				config.msgQueue[0].msgCount = 1;
				config.msgQueue[1].msgSize = 1;
				config.msgQueue[1].msgCount = 1;
				config.msgQueue[2].msgSize = maxSize;
				config.msgQueue[2].msgCount = queueCount;
				s_client->clientMemory = sceClibMspaceMalloc(s_mspace, SceIpmi_006EFA9D(&config));
				if (s_client->clientMemory == NULL) {
					result = NP_IMPS_RAW_MSG_ERROR_MSPACE;
				} else {
					result = SceIpmi_22625598(&s_client->client, &config, s_client->clientMemory);
					if (result >= 0) {
						result = SceIpmi_B56BAC7F(s_client->client);
						if (result >= 0) {
							result = npBasicRequest_90035(SceIpmi_9EE9496B((int)s_client->client), type);
							if (result >= 0) {
								s_client->type = type;
								s_client->maxSize = maxSize;
								return result;
							}
							SceIpmi_B943F5D9(s_client->client);
						}
						SceIpmi_D73C03D2(s_client->client);
					}
					sceClibMspaceFree(s_mspace, s_client->clientMemory);
					s_client->clientMemory = NULL;
				}
				sceClibMspaceFree(s_mspace, s_client);
				s_client = NULL;
			}
			sceClibMspaceDestroy(s_mspace);
			s_mspace = NULL;
			sceKernelFreeMemBlock(s_memBlock);
			s_memBlock = 0;
		}
	}
	return result;
}

int npImpsRawMsgClientDestroy(void)
{
	if (s_client == NULL)
		return NP_IMPS_RAW_MSG_ERROR_NOT_CREATED;
	npBasicRequest_90036(SceIpmi_9EE9496B((int)s_client->client), s_client->type);
	SceIpmi_B943F5D9(s_client->client);
	SceIpmi_D73C03D2(s_client->client);
	sceClibMspaceFree(s_mspace, s_client->clientMemory);
	s_client->clientMemory = NULL;
	sceClibMspaceFree(s_mspace, s_client);
	s_client = NULL;
	sceClibMspaceDestroy(s_mspace);
	s_mspace = NULL;
	sceKernelFreeMemBlock(s_memBlock);
	s_memBlock = 0;
	return 0;
}

NpImpsRawMsgClient *npImpsRawMsgClientGet(void)
{
	return s_client;
}

int npImpsRawMsgClientReceive(NpImpsRawMsgClient *client_, void *buffer, SceSize size, void *received)
{
	char dummy;
	char *out;
	NpBasicClient *client;

	if (s_client == NULL)
		return NP_IMPS_RAW_MSG_ERROR_NOT_CREATED;
	out = &dummy;
	if (received != NULL)
		out = (char *)received;
	client = SceIpmi_9EE9496B((int)s_client->client);
	return client->vtbl->receive(client, 2, buffer, out, size);
}

int npImpsRawMsgClientSend(NpImpsRawMsgClient *client_, void *message, SceSize size)
{
	if (s_client == NULL)
		return NP_IMPS_RAW_MSG_ERROR_NOT_CREATED;
	if (s_client->maxSize < size)
		return NP_IMPS_RAW_MSG_ERROR_OUT_OF_MEMORY;
	return npBasicRequest_90037(SceIpmi_9EE9496B((int)s_client->client), s_client->type, message, size);
}

static NpImpsRawMsgCallback s_callback;		/* 0x81007004 */

int npImpsRawMsgStart(int type, SceSize maxSize, int queueCount, NpImpsRawMsgCallback callback)
{
	int result;

	if (callback == NULL)
		return NP_IMPS_RAW_MSG_ERROR_INVALID_ARGUMENT;
	result = npImpsRawMsgClientCreate(type, maxSize, queueCount);
	if (result < 0)
		return result;
	s_callback = callback;
	return 0;
}

int npImpsRawMsgStop(void)
{
	s_callback = NULL;
	return npImpsRawMsgClientDestroy();
}

int npImpsRawMsgSend(void *message, SceSize size)
{
	if (message == NULL)
		return NP_IMPS_RAW_MSG_ERROR_INVALID_ARGUMENT;
	if (*(int *)message != 3)
		return NP_IMPS_RAW_MSG_ERROR_INVALID_MESSAGE;
	if (((int *)message)[2] == 0)
		return NP_IMPS_RAW_MSG_ERROR_NO_RECEIVER;
	return npImpsRawMsgClientSend(npImpsRawMsgClientGet(), message, size);
}

int npImpsRawMsgReceive(void *buffer, SceSize size, int *received)
{
	int result;

	result = npImpsRawMsgClientReceive(npImpsRawMsgClientGet(), buffer, size, received);
	if (result >= 0)
		return 0;
	if (result == -2147351295)
		return 0;
	return result;
}
