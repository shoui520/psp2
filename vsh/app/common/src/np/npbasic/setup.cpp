/*
 * SceNpBasic: setup (Sony's npbasic/setup.cpp). The library state, the thread that sends presence
 * updates at a limited rate, initialization and termination, the event handlers, and the exports
 * that talk to the NP service about presence and the play session log.
 */
#include "npbasic_internal.h"

#define NP_BASIC_SETUP_SOURCE_FILE \
	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\npbasic\\setup.cpp"
#define NP_BASIC_SETUP_ASSERT_FAILED(line, func)					\
	do {										\
		sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n",		\
			      NP_BASIC_SETUP_SOURCE_FILE, line, func);			\
		*(volatile int *)0 = 0;							\
	} while (0)

#define NP_BASIC_ERROR_BUSY_SENDER		NP_BASIC_ERROR_BUSY
#define NP_BASIC_ERROR_NOT_READY_TO_COMMUNICATE	-2141905654	/* 0x80551d0a */
#define NP_BASIC_ERROR_NO_JOIN_GAME_ACK_HANDLER	-2141905646	/* 0x80551d12 */
/* the SDK version this firmware's library carries (what a handler struct with none is given) */
#if FW_SINCE(0x03650000)
#define NP_BASIC_SDK_VERSION_DEFAULT	0x3650011
#else
#define NP_BASIC_SDK_VERSION_DEFAULT	0x3630011
#endif
#define NP_BASIC_SDK_VERSION_1_50		0x1500000
#define NP_BASIC_SDK_VERSION_1_60		0x1600000
#define NP_BASIC_SDK_VERSION_2_00		0x2000000

NpBasicState g_state;			/* 0x81007018 */

/* the sender's interval and tokens: 6 updates in a row, then one per interval */
static NpBasicSender *npBasicSenderInit(NpBasicSender *sender, int interval) __attribute__((noinline));
static NpBasicSender *npBasicSenderInit(NpBasicSender *sender, int interval)
{
	sender->tokens = 6;
	sender->interval = interval;
	return sender;
}

/* two functions of the state's class that nothing calls; both give 9 */
int npBasicStateGetConstantA(void)
{
	return 9;
}

int npBasicStateGetConstantB(void)
{
	return 9;
}


NpBasicState::NpBasicState()
{
	initialized = 0;
	memBlock = 0;
	mspace = 0;
	ipcBuffer = 0;
	client = 0;
	npBasicSenderInit(&sender, 60000000);
	altHandlerRegistered = 0;
	reserved5[0] = 0;
	joinGameAckHandler = NULL;
	joinGameAckArg = NULL;
	joinGameAckMode = 0;
	sceClibMemset(&commId, 0, sizeof(commId));
	sceClibMemset(&callbackTable, 0, sizeof(callbackTable));
	sceClibMemset(&mutex, 0, sizeof(mutex));
	sceClibMemset(sendTimes, 0, sizeof(sendTimes));
	sceClibMemset(joinGameEntries, 0, sizeof(joinGameEntries));
}

static int npBasicStateCreateMutex(NpBasicState *state) __attribute__((noinline));
static int npBasicStateCreateMutex(NpBasicState *state)
{
	return sceKernelCreateLwMutex(&state->mutex, "SceNpBasicSystemLwMutex", 0x2000, 0, NULL);
}

static int npBasicStateDeleteMutex(NpBasicState *state) __attribute__((noinline));
static int npBasicStateDeleteMutex(NpBasicState *state)
{
	return sceKernelDeleteLwMutex(&state->mutex);
}

static int npBasicStateLock(NpBasicState *state) __attribute__((noinline));
static int npBasicStateLock(NpBasicState *state)
{
	return sceKernelLockLwMutex(&state->mutex, 1, NULL);
}

static int npBasicStateUnlock(NpBasicState *state) __attribute__((noinline));
static int npBasicStateUnlock(NpBasicState *state)
{
	return sceKernelUnlockLwMutex(&state->mutex, 1);
}

static int npBasicSenderThread(SceSize argSize, void *argp);

static int npBasicSenderCreate(NpBasicSender *sender, int interval, unsigned char tokens) __attribute__((noinline));
static int npBasicSenderCreate(NpBasicSender *sender, int interval, unsigned char tokens)
{
	SceUID thread;

	sender->tokens = tokens;
	sender->interval = interval;
	thread = sceKernelCreateThread("SceNpBasicThread", npBasicSenderThread, 0x10000100, 0x5000, 0, 0, NULL);
	sender->thread = thread;
	if (thread < 0)
		return thread;
	return 0;
}

static int npBasicSenderDestroy(NpBasicSender *sender) __attribute__((noinline));
static int npBasicSenderDestroy(NpBasicSender *sender)
{
	sceKernelDeleteThread(sender->thread);
	sender->interval = 0;
	sender->tokens = 0;
	return 0;
}

static int npBasicSenderSetup(NpBasicSender *sender, int interval, unsigned char tokens) __attribute__((noinline));
static int npBasicSenderSetup(NpBasicSender *sender, int interval, unsigned char tokens)
{
	sender->tokens = tokens;
	sender->interval = interval;
	return 0;
}

static int npBasicSenderStart(NpBasicSender *sender) __attribute__((noinline));
static int npBasicSenderStart(NpBasicSender *sender)
{
	NpBasicSender *argp;

	argp = sender;
	return sceKernelStartThread(sender->thread, sizeof(argp), &argp);
}

static int npBasicSenderStop(NpBasicSender *sender) __attribute__((noinline));
static int npBasicSenderStop(NpBasicSender *sender)
{
	SceUInt32 stat;
	SceUInt32 remaining;
	int result;

	stat = 0;
	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		sender->stopRequested = 1;
		npBasicStateUnlock(&g_state);
		npBasicStateLock(&g_state);
		if (sender->timer != 0) {
			result = sceKernelCancelTimer(sender->timer, &remaining);
			if (result == (int)0x80028241)
				result = 0;
		}
		npBasicStateUnlock(&g_state);
	}
	if (result >= 0)
		result = sceKernelWaitThreadEnd(sender->thread, (int *)&stat, NULL);
	return result;
}

int npBasicSenderTakeToken(NpBasicSender *sender)
{
	unsigned char tokens;
	int result;
	int ret;

	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		ret = NP_BASIC_ERROR_BUSY;
		tokens = sender->tokens;
		if (tokens != 0) {
			sender->tokens = tokens - 1;
			ret = (unsigned char)(tokens - 1);
		}
		npBasicStateUnlock(&g_state);
		return ret;
	}
	return result;
}

int npBasicSenderSetPresence(NpBasicSender *sender, const SceNpBasicInGamePresence *presence)
{
	int result;

	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		sender->presence = *presence;
		sender->presenceState = 1;
		return npBasicStateUnlock(&g_state);
	}
	return result;
}

int npBasicSenderClearPresence(NpBasicSender *sender)
{
	int result;

	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		sender->presenceState = 2;
		return npBasicStateUnlock(&g_state);
	}
	return result;
}

int npBasicSenderSetPartyPresence(NpBasicSender *sender, const NpBasicPartyPresence *presence)
{
	int result;

	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		sender->partyPresence = *presence;
		sender->partyPresenceState = 1;
		return npBasicStateUnlock(&g_state);
	}
	return result;
}

int npBasicSenderClearPartyPresence(NpBasicSender *sender)
{
	int result;

	result = npBasicStateLock(&g_state);
	if (result >= 0) {
		sender->partyPresenceState = 2;
		return npBasicStateUnlock(&g_state);
	}
	return result;
}

/* the state at module start: the sender's thread is made */
static int npBasicStateStart(NpBasicState *state) __attribute__((noinline));
static int npBasicStateStart(NpBasicState *state)
{
	int result;

	state->handlerRegistered = 0;
	state->initialized = 0;
	state->memBlock = 0;
	state->mspace = 0;
	state->ipcBuffer = 0;
	state->client = 0;
	sceClibMemset(&state->commId, 0, sizeof(state->commId));
	sceClibMemset(&state->callbackTable, 0, sizeof(state->callbackTable));
	sceClibMemset(&state->mutex, 0, sizeof(state->mutex));
	sceClibMemset(state->joinGameEntries, 0, sizeof(state->joinGameEntries));
	state->joinGameAckHandler = NULL;
	state->joinGameAckArg = NULL;
	result = npBasicSenderCreate(&state->sender, 60000000, 6);
	state->joinGameAckMode = 0;
	return result;
}

static int npBasicStateStop(NpBasicState *state) __attribute__((noinline));
static int npBasicStateStop(NpBasicState *state)
{
	npBasicSenderDestroy(&state->sender);
	sceClibMemset(state->joinGameEntries, 0, sizeof(state->joinGameEntries));
	sceClibMemset(&state->mutex, 0, sizeof(state->mutex));
	sceClibMemset(&state->callbackTable, 0, sizeof(state->callbackTable));
	sceClibMemset(&state->commId, 0, sizeof(state->commId));
	state->initialized = 0;
	state->joinGameAckHandler = NULL;
	state->joinGameAckArg = NULL;
	state->ipcBuffer = 0;
	state->client = 0;
	state->mspace = 0;
	state->memBlock = 0;
	state->joinGameAckMode = 0;
	return 0;
}

static int npBasicStateReset(NpBasicState *state) __attribute__((noinline));
static int npBasicStateReset(NpBasicState *state)
{
	int result;

	state->handlerRegistered = 0;
	state->initialized = 0;
	state->memBlock = 0;
	state->mspace = 0;
	state->ipcBuffer = 0;
	state->client = 0;
	sceClibMemset(&state->commId, 0, sizeof(state->commId));
	sceClibMemset(&state->callbackTable, 0, sizeof(state->callbackTable));
	sceClibMemset(&state->mutex, 0, sizeof(state->mutex));
	sceClibMemset(state->joinGameEntries, 0, sizeof(state->joinGameEntries));
	state->joinGameAckHandler = NULL;
	state->joinGameAckArg = NULL;
	result = npBasicSenderSetup(&state->sender, 60000000, 6);
	state->joinGameAckMode = 0;
	return result;
}

extern "C" {

int module_start(SceSize argSize, const void *argp)
{
	npBasicStateStart(&g_state);
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	npBasicStateStop(&g_state);
	return 0;
}

int SceNpBasic_7F13243D(const NpBasicPartyPresence *a1)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (a1->version <= 0 || a1->version >= 3)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicSenderTakeToken(&g_state.sender);
	if (result >= 0) {
		result = npBasicRequest_9000D(client, a1);
		if (result < 0)
			return npBasicConvertError(result);
	} else if (result == NP_BASIC_ERROR_BUSY) {
		npBasicSenderSetPartyPresence(&g_state.sender, a1);
	}
	return result;
}

int SceNpBasic_FB1B7762(void)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = npBasicSenderTakeToken(&g_state.sender);
	if (result >= 0) {
		result = npBasicRequest_9000E(client);
		if (result < 0)
			return npBasicConvertError(result);
	} else if (result == NP_BASIC_ERROR_BUSY) {
		npBasicSenderClearPartyPresence(&g_state.sender);
	}
	return result;
}

int sceNpBasicRecordPlaySessionLog(const SceNpId *withWhom, const SceNpBasicPlaySessionLogDescription *description)
{
	SceRtcTick tick;
	const SceNpCommunicationId *src;
	SceNpCommunicationId commId;
	SceNpBasicPlaySessionLogDescription logDescription;
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	sceClibMemset(&commId, 0, sizeof(commId));
	sceClibMemset(&logDescription, 0, sizeof(logDescription));
	if (withWhom == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (description != NULL) {
		sceClibMemcpy_safe(&logDescription, description, sizeof(logDescription));
		logDescription.text[SCE_NP_BASIC_PLAY_SESSION_DESCRIPTION_SIZE_MAX - 1] = 0;
	}
	result = sceRtcGetCurrentNetworkTick(&tick);
	if (result >= 0) {
		if (g_state.handlerRegistered != 0) {
			commId = g_state.commId;
		} else {
			result = SceNpManager_96DF1B76(&src);
			if (result < 0)
				return result;
			if (src == NULL)
				return NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED;
			sceClibMemcpy_safe(&commId, src, sizeof(commId));
		}
		result = npBasicRequest_90014(client, &tick, withWhom, &commId, &logDescription);
		if (result < 0)
			result = npBasicConvertError(result);
	}
	return result;
}

int sceNpBasicGetPlaySessionLogSize(SceNpBasicPlaySessionLogType type, SceSize *size)
{
	const SceNpCommunicationId *src;
	SceNpCommunicationId commId;
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (size == NULL || type <= SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_INVALID || type >= SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_MAX)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (type == SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_BY_NP_COMM_ID) {
		if (g_state.handlerRegistered != 0) {
			commId = g_state.commId;
		} else {
			result = SceNpManager_96DF1B76(&src);
			if (result < 0)
				return result;
			if (src == NULL)
				return NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED;
			sceClibMemcpy_safe(&commId, src, sizeof(commId));
		}
	}
	result = npBasicRequest_90015(client, type, &commId, (int *)size);
	if (result < 0)
		result = npBasicConvertError(result);
	return result;
}

int sceNpBasicGetPlaySessionLog(SceNpBasicPlaySessionLogType type, unsigned int index, SceNpBasicPlaySessionLog *log)
{
	const SceNpCommunicationId *src;
	SceNpCommunicationId commId;
	NpBasicClient *client;
	int count;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	count = 0;
	sceClibMemset(&commId, 0, sizeof(commId));
	if (type <= SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_INVALID || type >= SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_MAX || log == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	if (type == SCE_NP_BASIC_PLAY_SESSION_LOG_TYPE_BY_NP_COMM_ID) {
		if (g_state.handlerRegistered != 0) {
			commId = g_state.commId;
		} else {
			result = SceNpManager_96DF1B76(&src);
			if (result < 0)
				return result;
			if (src == NULL)
				return NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED;
			sceClibMemcpy_safe(&commId, src, sizeof(commId));
		}
	}
	result = npBasicRequest_90016(client, type, &commId, index, log, 1, &count);
	if (result < 0)
		result = npBasicConvertError(result);
	return result;
}

int SceNpBasic_1CAAF066(const void *a1, int *a2)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a2 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicRequest_9003D(client, a1, a2);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

int SceNpBasic_748925A4(const void *a1, int a2, void *a3)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (a1 == NULL || a3 == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	result = npBasicRequest_9003E(client, a1, a2, a3);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

int SceNpBasic_A6BFBBFF(void)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = npBasicRequest_9002B(client);
	if (result < 0)
		return npBasicConvertError(result);
	return result;
}

} /* extern "C" */

/* the handler flags the service is told about (a flag per handler that is set) */
typedef struct NpBasicHandlerFlags {
	unsigned char flags[14];

	NpBasicHandlerFlags()
	{
		unsigned char *first;
		unsigned char *second;
		int i;

		first = flags;
		second = flags;
		for (i = 0; i < 7; i++) {
			first[0] = 0;
			second[1] = 0;
			second += 2;
			first += 2;
		}
	}
} NpBasicHandlerFlags;

static void npBasicSetHandlerFlags(unsigned char *flags, const NpBasicEventHandlers *handlers)
	__attribute__((noinline));
static void npBasicSetHandlerFlags(unsigned char *flags, const NpBasicEventHandlers *handlers)
{
	SceUInt32 version;
	const void *friendList;
	const void *friendOnlineStatus;
	const void *blockList;
	const void *friendGamePresence;
	const void *inGameDataMessage;
	const void *friendContext;

	version = handlers->sdkVersion;
	friendOnlineStatus = (const void *)handlers->friendOnlineStatus;
	blockList = (const void *)handlers->blockList;
	friendGamePresence = (const void *)handlers->friendGamePresence;
	inGameDataMessage = (const void *)handlers->inGameDataMessage;
	friendContext = (const void *)handlers->friendContext;
	if (handlers->friendList != NULL)
		flags[2] = 1;
	if (friendOnlineStatus != NULL)
		flags[3] = 1;
	if (blockList != NULL)
		flags[8] = 1;
	if (friendGamePresence != NULL)
		flags[4] = 1;
	if (inGameDataMessage != NULL)
		flags[6] = 1;
	if (friendContext != NULL)
		flags[10] = 1;
	if (version >= NP_BASIC_SDK_VERSION_1_60 && handlers->friendRequest != NULL)
		flags[13] = 1;
	(void)friendList;
}

/* create_mem_block_and_mspace of setup.cpp (the library's heap) */
static inline int npBasicCreateMemBlockAndMspace(void) __attribute__((always_inline));
static inline int npBasicCreateMemBlockAndMspace(void)
{
	void *base;
	int result;

	base = NULL;
	if (g_state.memBlock != 0)
		NP_BASIC_SETUP_ASSERT_FAILED(43, "create_mem_block_and_mspace");
	result = sceKernelAllocMemBlock("SceNpBasicMemBlock", 0xC20D060, 0x1000, NULL);
	if (result >= 0) {
		g_state.memBlock = result;
		result = sceKernelGetMemBlockBase(result, &base);
		if (result >= 0) {
			g_state.mspace = (int)sceClibMspaceCreate(base, 0x1000);
			if (g_state.mspace != 0)
				return 0;
			result = NP_BASIC_ERROR_OUT_OF_MEMORY;
		}
		sceKernelFreeMemBlock(g_state.memBlock);
	}
	g_state.memBlock = 0;
	return result;
}

/* the same with the memory type the caller wants */
static inline int npBasicCreateMemBlockAndMspaceInternal(int mode) __attribute__((always_inline));
static inline int npBasicCreateMemBlockAndMspaceInternal(int mode)
{
	void *base;
	int result;

	base = NULL;
	if (g_state.memBlock != 0)
		NP_BASIC_SETUP_ASSERT_FAILED(652, "create_mem_block_and_mspace_internal");
	if (mode == 1)
		result = sceKernelAllocMemBlock("SceNpBasicMemBlock", (SceKernelMemBlockType)0xE20D060, 0x1000, NULL);
	else
		result = sceKernelAllocMemBlock("SceNpBasicMemBlock", (SceKernelMemBlockType)0xC20D060, 0x1000, NULL);
	if (result >= 0) {
		g_state.memBlock = result;
		result = sceKernelGetMemBlockBase(result, &base);
		if (result >= 0) {
			g_state.mspace = (int)sceClibMspaceCreate(base, 0x1000);
			if (g_state.mspace != 0)
				return 0;
			result = NP_BASIC_ERROR_OUT_OF_MEMORY;
		}
		sceKernelFreeMemBlock(g_state.memBlock);
	}
	g_state.memBlock = 0;
	return result;
}

/* create_ipmi_wrapper_client of setup.cpp */
static inline int npBasicCreateIpmiWrapperClient(IpmiWrapper::Client::Config *config)
	__attribute__((always_inline));
static inline int npBasicCreateIpmiWrapperClient(IpmiWrapper::Client::Config *config)
{
	int result;

	if (g_state.ipcBuffer != 0)
		NP_BASIC_SETUP_ASSERT_FAILED(93, "create_ipmi_wrapper_client");
	g_state.ipcBuffer = (int)sceClibMspaceMalloc((void *)g_state.mspace, SceIpmi_006EFA9D(config));
	result = NP_BASIC_ERROR_OUT_OF_MEMORY;
	if (g_state.ipcBuffer != 0) {
		result = SceIpmi_22625598((IpmiWrapper::Client **)&g_state.client, config, (void *)g_state.ipcBuffer);
		sceClibMspaceFree((void *)g_state.mspace, (void *)g_state.ipcBuffer);
		g_state.ipcBuffer = 0;
	}
	return result;
}

extern "C" {

int sceNpBasicInit(void)
{
	IpmiWrapper::Client::Config config;
	int result;
	int client;

	if (g_state.initialized != 0) {
		result = NP_BASIC_ERROR_ALREADY_INITIALIZED;
	} else {
		sceClibStrncpy(config.name, "SceNpImpsUtil", sizeof(config.name));
		config.msgQueue[0].msgSize = 1024;
		config.msgQueue[0].msgCount = 256;
		config.callbackCount = 0;
		config.msgQueueCount = 1;
		config.budget = 2;
		result = npBasicCreateMemBlockAndMspace();
		if (result >= 0) {
			result = npBasicCreateIpmiWrapperClient(&config);
			if (result >= 0) {
				result = SceIpmi_B56BAC7F((IpmiWrapper::Client *)g_state.client);
				client = g_state.client;
				if (result >= 0) {
					if (SceIpmi_9EE9496B(g_state.client) != NULL) {
						result = npBasicStateCreateMutex(&g_state);
						if (result >= 0) {
							result = npBasicSenderStart(&g_state.sender);
							if (result >= 0) {
								sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
								g_state.initialized = 1;
								return 0;
							}
							npBasicStateDeleteMutex(&g_state);
						}
					} else {
						result = NP_BASIC_ERROR_UNKNOWN;
					}
					SceIpmi_B943F5D9((IpmiWrapper::Client *)g_state.client);
					client = g_state.client;
				}
				if (SceIpmi_D73C03D2((IpmiWrapper::Client *)client) >= 0) {
					g_state.client = 0;
					sceClibMspaceFree((void *)g_state.mspace, (void *)g_state.ipcBuffer);
				}
			}
			sceClibMspaceDestroy((void *)g_state.mspace);
			g_state.mspace = 0;
			sceKernelFreeMemBlock(g_state.memBlock);
			g_state.memBlock = 0;
		}
	}
	return result;
}

int sceNpBasicTerm(void)
{
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (g_state.handlerRegistered != 0) {
		npBasicRequest_90010(SceIpmi_9EE9496B(g_state.client));
		npBasicUnregisterHandler(SceIpmi_9EE9496B(g_state.client));
		sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
		g_state.handlerRegistered = 0;
		sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
	}
	result = npBasicSenderStop(&g_state.sender);
	if (result >= 0) {
		result = npBasicStateDeleteMutex(&g_state);
		if (result >= 0) {
			if (SceIpmi_9EE9496B(g_state.client) != NULL) {
				result = SceIpmi_B943F5D9((IpmiWrapper::Client *)g_state.client);
				if (result >= 0) {
					result = SceIpmi_D73C03D2((IpmiWrapper::Client *)g_state.client);
					if (result >= 0) {
						g_state.client = 0;
						sceClibMspaceFree((void *)g_state.mspace, (void *)g_state.ipcBuffer);
						result = 0;
					}
					if (result >= 0) {
						sceClibMspaceDestroy((void *)g_state.mspace);
						g_state.mspace = 0;
						sceKernelFreeMemBlock(g_state.memBlock);
						g_state.memBlock = 0;
						sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
						sceNpBasicUnregisterJoinGameAckHandler();
						g_state.initialized = 0;
						return 0;
					}
				}
			} else {
				return NP_BASIC_ERROR_UNKNOWN;
			}
		}
	}
	return result;
}

int sceNpBasicRegisterHandler(const NpBasicEventHandlers *handlers, const SceNpCommunicationId *commId, void *arg)
{
	const SceNpCommunicationId *src[3];
	int result;

	sceClibMemset(src, 0, sizeof(src));
	NpBasicHandlerFlags flags;
	sceClibMemset(flags.flags, 0, sizeof(flags.flags));
	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (handlers == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	g_state.callbackTable.friendList = handlers->friendList;
	g_state.callbackTable.friendListArg = arg;
	g_state.callbackTable.friendOnlineStatus = handlers->friendOnlineStatus;
	g_state.callbackTable.friendOnlineStatusArg = arg;
	g_state.callbackTable.blockList = handlers->blockList;
	g_state.callbackTable.blockListArg = arg;
	g_state.callbackTable.friendGamePresence = handlers->friendGamePresence;
	g_state.callbackTable.friendGamePresenceArg = arg;
	g_state.callbackTable.inGameDataMessage = handlers->inGameDataMessage;
	g_state.callbackTable.inGameDataMessageArg = arg;
	g_state.callbackTable.friendContext = handlers->friendContext;
	g_state.callbackTable.friendContextArg = arg;
	if (handlers->sdkVersion >= NP_BASIC_SDK_VERSION_1_60) {
		g_state.callbackTable.friendRequest = handlers->friendRequest;
		g_state.callbackTable.friendRequestArg = arg;
	}
	if (commId != NULL) {
		sceClibMemcpy_safe(&g_state.commId, commId, sizeof(g_state.commId));
	} else {
		result = SceNpManager_96DF1B76(&src[0]);
		if (result < 0)
			goto fail;
		if (src[0] == NULL) {
			result = NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED;
			goto fail;
		}
		sceClibMemcpy_safe(&g_state.commId, src[0], sizeof(g_state.commId));
	}
	npBasicSetHandlerFlags(flags.flags, handlers);
	result = npBasicRegisterHandler(SceIpmi_9EE9496B(g_state.client), flags.flags);
	if (result >= 0) {
		result = npBasicRequest_9000F(SceIpmi_9EE9496B(g_state.client), &g_state.commId);
		if (result >= 0) {
			g_state.handlerRegistered = 1;
			return 0;
		}
		npBasicUnregisterHandler(SceIpmi_9EE9496B(g_state.client));
		sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
	}
fail:
	sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
	return result;
}

int sceNpBasicUnregisterHandler(void)
{
	int result;
	int error;

	result = 0;
	error = 0;
	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (g_state.handlerRegistered != 0) {
		int ret;

		ret = npBasicRequest_90010(SceIpmi_9EE9496B(g_state.client));
		if (ret < 0)
			error = ret;
		result = npBasicUnregisterHandler(SceIpmi_9EE9496B(g_state.client));
		if (result < 0 && error == 0)
			error = result;
		sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
		g_state.handlerRegistered = 0;
		sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
		if (error != 0)
			return error;
	}
	return result;
}

int SceNpBasic_6F1B58F9(const NpBasicEventHandlers2 *handlers, void *arg)
{
	NpBasicHandlerFlags flags;
	NpBasicEventHandlers staging;
	NpBasicEventHandlers common;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (SceIpmi_9EE9496B(g_state.client) == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	sceClibMemset(flags.flags, 0, sizeof(flags.flags));
	if (handlers != NULL) {
		sceClibMemset(&staging, 0, sizeof(staging));
		g_state.callbackTable.myOnlineStatusArg = arg;
		g_state.callbackTable.friendList = handlers->friendList;
		g_state.callbackTable.friendListArg = arg;
		g_state.callbackTable.friendOnlineStatus = handlers->friendOnlineStatus;
		g_state.callbackTable.friendOnlineStatusArg = arg;
		g_state.callbackTable.friendGamePresence = NULL;
		g_state.callbackTable.friendGamePresenceDetailArg = arg;
		g_state.callbackTable.blockList = handlers->blockList;
		g_state.callbackTable.friendGamePresenceArg = arg;
		staging.friendList = handlers->friendList;
		g_state.callbackTable.inGameDataMessage = NULL;
		g_state.callbackTable.inGameDataMessageArg = arg;
		staging.friendOnlineStatus = handlers->friendOnlineStatus;
		g_state.callbackTable.friendContextArg = arg;
		staging.blockList = handlers->blockList;
		staging.friendGamePresence = NULL;
		staging.inGameDataMessage = NULL;
		g_state.callbackTable.friendPartyPresence = handlers->friendPartyPresence;
		g_state.callbackTable.friendPartyPresenceArg = arg;
		g_state.callbackTable.friendGamePresenceDetail = handlers->friendGamePresenceDetail;
		g_state.callbackTable.blockListArg = arg;
		g_state.callbackTable.myGamePresenceDetail = handlers->myGamePresenceDetail;
		g_state.callbackTable.myGamePresenceDetailArg = arg;
		g_state.callbackTable.friendContext = staging.friendContext;
		g_state.callbackTable.myOnlineStatus = handlers->myOnlineStatus;
		g_state.callbackTable.friendRequest = handlers->friendRequest;
		g_state.callbackTable.friendRequestArg = arg;
		sceClibMemset(&common, 0, sizeof(common));
		common.friendList = handlers->friendList;
		common.friendOnlineStatus = handlers->friendOnlineStatus;
		common.blockList = handlers->blockList;
		npBasicSetHandlerFlags(flags.flags, &common);
		if (handlers->friendPartyPresence != NULL)
			flags.flags[5] = 1;
		if (handlers->friendGamePresenceDetail != NULL)
			flags.flags[7] = 1;
		if (handlers->myGamePresenceDetail != NULL)
			flags.flags[9] = 1;
		if (handlers->myOnlineStatus != NULL)
			flags.flags[1] = 1;
		if (handlers->friendRequest != NULL)
			flags.flags[13] = 1;
		result = npBasicRegisterHandler(SceIpmi_9EE9496B(g_state.client), flags.flags);
		if (result >= 0) {
			g_state.altHandlerRegistered = 1;
			return 0;
		}
		sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
	} else {
		result = NP_BASIC_ERROR_INVALID_ARGUMENT;
	}
	return result;
}

int SceNpBasic_B414E768(void)
{
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	g_state.altHandlerRegistered = 0;
	result = npBasicUnregisterHandler(SceIpmi_9EE9496B(g_state.client));
	sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
	return result;
}

int SceNpBasic_999CC1E9(const int *mode)
{
	IpmiWrapper::Client::Config config;
	int result;
	int first;
	int client;

	if (g_state.initialized != 0) {
		result = NP_BASIC_ERROR_ALREADY_INITIALIZED;
	} else {
		npBasicStateReset(&g_state);
		first = *mode;
		sceClibStrncpy(config.name, "SceNpImpsUtil", sizeof(config.name));
		config.msgQueue[0].msgCount = 256;
		config.callbackCount = 0;
		config.msgQueueCount = 1;
		config.msgQueue[0].msgSize = 1024;
		config.budget = 2;
		if (first == 1)
			config.budget = 3;
		result = npBasicCreateMemBlockAndMspaceInternal(*mode);
		if (result >= 0) {
			result = npBasicCreateIpmiWrapperClient(&config);
			if (result >= 0) {
				result = SceIpmi_B56BAC7F((IpmiWrapper::Client *)g_state.client);
				client = g_state.client;
				if (result >= 0) {
					if (SceIpmi_9EE9496B(g_state.client) != NULL) {
						result = npBasicStateCreateMutex(&g_state);
						if (result >= 0) {
							sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
							g_state.initialized = 1;
							return 0;
						}
					} else {
						result = NP_BASIC_ERROR_UNKNOWN;
					}
					SceIpmi_B943F5D9((IpmiWrapper::Client *)g_state.client);
					client = g_state.client;
				}
				if (SceIpmi_D73C03D2((IpmiWrapper::Client *)client) >= 0) {
					g_state.client = 0;
					sceClibMspaceFree((void *)g_state.mspace, (void *)g_state.ipcBuffer);
				}
			}
			sceClibMspaceDestroy((void *)g_state.mspace);
			g_state.mspace = 0;
			sceKernelFreeMemBlock(g_state.memBlock);
			g_state.memBlock = 0;
		}
	}
	return result;
}

int sceNpBasicRegisterInGameDataMessageHandler(
	void (*handler)(const void *message, const void *data, void *arg), const SceNpCommunicationId *commId, void *arg)
{
	const SceNpCommunicationId *src[3];
	NpBasicEventHandlers handlers;
	int result;

	sceClibMemset(src, 0, sizeof(src));
	NpBasicHandlerFlags flags;
	sceClibMemset(flags.flags, 0, sizeof(flags.flags));
	sceClibMemset(&handlers, 0, sizeof(handlers));
	handlers.sdkVersion = NP_BASIC_SDK_VERSION_DEFAULT;
	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (handler == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	handlers.inGameDataMessage = handler;
	g_state.callbackTable.friendList = handlers.friendList;
	g_state.callbackTable.friendListArg = arg;
	g_state.callbackTable.friendOnlineStatus = handlers.friendOnlineStatus;
	g_state.callbackTable.friendOnlineStatusArg = arg;
	g_state.callbackTable.blockList = handlers.blockList;
	g_state.callbackTable.blockListArg = arg;
	g_state.callbackTable.friendGamePresence = handlers.friendGamePresence;
	g_state.callbackTable.friendGamePresenceArg = arg;
	g_state.callbackTable.inGameDataMessage = handler;
	g_state.callbackTable.inGameDataMessageArg = arg;
	g_state.callbackTable.friendContext = handlers.friendContext;
	g_state.callbackTable.friendContextArg = arg;
	g_state.callbackTable.friendRequest = handlers.friendRequest;
	g_state.callbackTable.friendRequestArg = arg;
	if (commId != NULL) {
		sceClibMemcpy_safe(&g_state.commId, commId, sizeof(g_state.commId));
	} else {
		result = SceNpManager_96DF1B76(&src[0]);
		if (result < 0)
			goto fail;
		if (src[0] == NULL) {
			result = NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED;
			goto fail;
		}
		sceClibMemcpy_safe(&g_state.commId, src[0], sizeof(g_state.commId));
	}
	npBasicSetHandlerFlags(flags.flags, &handlers);
	result = npBasicRegisterHandler(SceIpmi_9EE9496B(g_state.client), flags.flags);
	if (result >= 0) {
		g_state.handlerRegistered = 1;
		return 0;
	}
fail:
	sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
	return result;
}

int sceNpBasicUnregisterInGameDataMessageHandler(void)
{
	int result;
	int error;

	result = 0;
	error = 0;
	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	if (g_state.handlerRegistered != 0) {
		result = npBasicUnregisterHandler(SceIpmi_9EE9496B(g_state.client));
		if (result < 0)
			error = result;
		sceClibMemset(&g_state.commId, 0, sizeof(g_state.commId));
		g_state.handlerRegistered = 0;
		sceClibMemset(&g_state.callbackTable, 0, sizeof(g_state.callbackTable));
		if (error != 0)
			return error;
	}
	return result;
}

} /* extern "C" */

static int npBasicSenderThread(SceSize argSize, void *argp)
{
	NpBasicSender *sender;
	SceNpBasicInGamePresence presenceCopy;
	NpBasicPartyPresence partyCopy;
	SceInt64 interval;
	SceUInt32 resultPattern;
	SceUInt64 userData;
	NpBasicClient *client;
	int result;

	sender = *(NpBasicSender **)argp;
	interval = sender->interval;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client != NULL) {
		sender->timer = sceKernelCreateTimer("SceNpBasicTimer", 0x2100, NULL);
		if (sender->timer >= 0) {
			result = sceKernelSetTimerEvent(sender->timer, 0, (SceKernelSysClock *)&interval, 1);
			if (result >= 0) {
				result = sceKernelStartTimer(sender->timer);
				if (result >= 0) {
					if (sender->stopRequested == 0) {
						for (;;) {
							result = npBasicStateLock(&g_state);
							if (result < 0)
								break;
							if (sender->presenceState == 1 || sender->presenceState == 2
							    || sender->partyPresenceState == 1 || sender->partyPresenceState == 2) {
								if (sender->presenceState > 1) {
									if (sender->presenceState <= 2 && npBasicRequest_90012(client) >= 0) {
										if (g_state.joinGameAckMode == 4)
											npBasicJgaCancelAll();
										g_state.joinGameAckMode = 0;
									}
								} else if (sender->presenceState > 0) {
									presenceCopy = sender->presence;
									if (npBasicRequest_90011(client, &presenceCopy) >= 0) {
										int type;

										type = presenceCopy.type;
										if (g_state.joinGameAckMode == 4 && presenceCopy.type != 4) {
											npBasicJgaCancelAll();
											type = presenceCopy.type;
										}
										g_state.joinGameAckMode = type;
									}
								}
								if (sender->partyPresenceState > 1) {
									if (sender->partyPresenceState <= 2)
										npBasicRequest_9000E(client);
								} else if (sender->partyPresenceState > 0) {
									partyCopy = sender->partyPresence;
									npBasicRequest_9000D(client, &partyCopy);
								}
								sceClibMemset(&sender->presence, 0, sizeof(sender->presence));
								sender->presence.sdkVersion = NP_BASIC_SDK_VERSION_DEFAULT;
								sender->presence.type = SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_DEFAULT;
								sceClibMemset(&sender->partyPresence, 0, sizeof(sender->partyPresence));
								sender->partyPresence.version = 1;
								sender->partyPresence.size = 460;
								sender->presenceState = 0;
								sender->partyPresenceState = 0;
								sender->tokens = sender->tokens - 1;
							}
							npBasicStateUnlock(&g_state);
							sceKernelWaitEvent(sender->timer, 0x8000, &resultPattern, &userData, NULL);
							result = npBasicStateLock(&g_state);
							if (result < 0)
								break;
							if (sender->tokens < 6)
								sender->tokens = sender->tokens + 1;
							npBasicStateUnlock(&g_state);
							if (sender->stopRequested != 0)
								goto stopped;
						}
					} else {
stopped:
						result = npBasicStateLock(&g_state);
						if (result >= 0) {
							sender->stopRequested = 0;
							npBasicStateUnlock(&g_state);
						}
					}
				}
			}
			sceKernelDeleteTimer(sender->timer);
			npBasicStateLock(&g_state);
			sender->timer = 0;
			npBasicStateUnlock(&g_state);
		} else {
			result = sender->timer;
		}
	} else {
		result = NP_BASIC_ERROR_UNKNOWN;
	}
	sceKernelExitThread(result);
	return result;
}

extern "C" {

int sceNpBasicSetInGamePresence(const SceNpBasicInGamePresence *presence)
{
	NpBasicClient *client;
	SceNpBasicInGamePresence copy;
	SceUInt32 version;
	int result;
	int type;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	sceClibMemset(&copy, 0, sizeof(copy));
	version = SceLibKernel_9F793F84();
	if (g_state.handlerRegistered != 0) {
		if (version >= NP_BASIC_SDK_VERSION_1_50) {
			if (presence == NULL || (int)presence->type <= 0 || (int)presence->type >= 5
			    || presence->dataSize > SCE_NP_BASIC_IN_GAME_PRESENCE_DATA_SIZE_MAX)
				return NP_BASIC_ERROR_INVALID_ARGUMENT;
			if (presence->type == SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_JOIN_GAME_ACK
			    && g_state.joinGameAckHandler == NULL)
				return NP_BASIC_ERROR_NO_JOIN_GAME_ACK_HANDLER;
		} else if (presence == NULL || (int)presence->type <= 0 || (int)presence->type >= 5) {
			return NP_BASIC_ERROR_INVALID_ARGUMENT;
		}
		sceClibMemcpy_safe(&copy, presence, sizeof(copy));
		copy.status[SCE_NP_BASIC_IN_GAME_PRESENCE_STATUS_SIZE_MAX - 1] = 0;
		if (version < NP_BASIC_SDK_VERSION_1_50 && presence->dataSize > SCE_NP_BASIC_IN_GAME_PRESENCE_DATA_SIZE_MAX)
			copy.dataSize = SCE_NP_BASIC_IN_GAME_PRESENCE_DATA_SIZE_MAX;
		result = npBasicSenderTakeToken(&g_state.sender);
		if (result >= 0) {
			result = npBasicRequest_90011(client, &copy);
			if (result >= 0) {
				type = presence->type;
				if (g_state.joinGameAckMode == 4 && type != 4) {
					npBasicJgaCancelAll();
					type = presence->type;
				}
				g_state.joinGameAckMode = type;
			} else {
				result = npBasicConvertError(result);
			}
		} else if (result == NP_BASIC_ERROR_BUSY) {
			npBasicSenderSetPresence(&g_state.sender, &copy);
		}
		return result;
	}
	return NP_BASIC_ERROR_NOT_READY_TO_COMMUNICATE;
}

int sceNpBasicUnsetInGamePresence(void)
{
	NpBasicClient *client;
	int result;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	result = npBasicSenderTakeToken(&g_state.sender);
	if (result >= 0) {
		result = npBasicRequest_90012(client);
		if (result >= 0) {
			if (g_state.joinGameAckMode == 4)
				npBasicJgaCancelAll();
			g_state.joinGameAckMode = 0;
		} else {
			return npBasicConvertError(result);
		}
	} else if (result == NP_BASIC_ERROR_BUSY) {
		npBasicSenderClearPresence(&g_state.sender);
	}
	return result;
}

int sceNpBasicSendInGameDataMessage(const SceNpId *to, const SceNpBasicInGameDataMessage *message)
{
	NpBasicClient *client;
	SceUInt64 now;
	int result;
	int ret;
	unsigned int i;
	unsigned char *base;
	SceUInt64 *slot;
	int offset;

	if (g_state.initialized == 0)
		return NP_BASIC_ERROR_NOT_INITIALIZED;
	client = SceIpmi_9EE9496B(g_state.client);
	if (client == NULL)
		return NP_BASIC_ERROR_UNKNOWN;
	if (g_state.handlerRegistered == 0)
		return NP_BASIC_ERROR_NOT_READY_TO_COMMUNICATE;
	if (to == NULL || message == NULL)
		return NP_BASIC_ERROR_INVALID_ARGUMENT;
	sceKernelGetSystemTime((SceKernelSysClock *)&now);
	result = npBasicStateLock(&g_state);
	ret = result;
	if (result >= 0) {
		base = (unsigned char *)g_state.sendTimes;
		offset = 0;
		slot = g_state.sendTimes;
		i = 0;
		do {
			if (*slot == 0) {
				*(SceUInt64 *)(base + offset) = now;
				goto found;
			}
			if (now > *slot + 60000000LL) {
				*(SceUInt64 *)(base + offset) = now;
				goto found;
			}
			i++;
			slot++;
			offset += 8;
		} while (i < 20);
		ret = NP_BASIC_ERROR_BUSY;
found:
		result = ret;
		npBasicStateUnlock(&g_state);
	}
	if (result >= 0) {
		if (SceLibKernel_9F793F84() >= NP_BASIC_SDK_VERSION_2_00)
			result = npBasicRequest_90013(client, to, message, &g_state.commId, 516);
		else
			result = npBasicRequest_90013(client, to, message, &g_state.commId, 132);
		if (result < 0)
			result = npBasicConvertError(result);
	}
	return result;
}

} /* extern "C" */
