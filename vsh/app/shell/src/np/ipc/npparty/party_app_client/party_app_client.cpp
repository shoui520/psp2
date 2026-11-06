/*
 * SceNpPartyAppUtil (vs0:app/NPXS10001/np_party_app.suprx): the party app's client of the
 * shell's "SceNpPartyUtil" IPMI service. Sony's file: vsh/app/shell/src/np/ipc/npparty/
 * party_app_client/party_app_client.cpp (its asserts, which name create_mem_block_and_mspace,
 * create_ipmi_wrapper_client and sceNpPartyAppUtilityCheckCallback). The export names are
 * the party app's (party_app_client.h); the message names and the other names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include "party_app_client.h"
#include "party_system.h"
#include "party_app_ipc_client.h"

/* the build the firmware's vsh was made in, as Sony's paths name it */
#if defined(FW_TYPE_CEX)
#define PARTY_APP_CLIENT_BUILD "J01509657"
#elif defined(FW_TYPE_TOOL)
#define PARTY_APP_CLIENT_BUILD "J01509658"
#else
#define PARTY_APP_CLIENT_BUILD "J01508051"
#endif

/* the assertions of this file, as Sony's build printed them */
#define PARTY_APP_CLIENT_SOURCE_FILE	\
	"W:\\Build\\" PARTY_APP_CLIENT_BUILD	\
	"\\vsh\\app\\shell\\src\\np\\ipc\\npparty\\party_app_client\\party_app_client.cpp"
#define PARTY_APP_ASSERT(cond, line, func)						\
	do {										\
		if (!(cond)) {								\
			sceClibPrintf("%s:%u: [PartyAppClient] ASSERION FAILURE in %s\n", \
				      PARTY_APP_CLIENT_SOURCE_FILE, line, func);	\
			*(volatile int *)0 = 0;						\
		}									\
	} while (0)

#define PARTY_APP_SERVICE_NAME		"SceNpPartyUtil"
#define PARTY_APP_MEMBLOCK_NAME		"SceNpPartyAppMemBlock"
#define PARTY_APP_MEMBLOCK_SIZE		0x1000
#define PARTY_APP_MESSAGE_QUEUE		0
#define PARTY_APP_MESSAGE_COUNT		10

/* the messages of the service's queue: the game's requests */
#define PARTY_APP_MESSAGE_GAME_INIT_PARAMS		0
#define PARTY_APP_MESSAGE_GAME_REGISTERED_HANDLERS	1
#define PARTY_APP_MESSAGE_GAME_SESSION_TERM		2
#define PARTY_APP_MESSAGE_GAME_DISABLE_VOICE		3
#define PARTY_APP_MESSAGE_GAME_RE_ENABLE_VOICE		4

typedef struct PartyAppMessage {
	int type;			/* PARTY_APP_MESSAGE_* */
	union {
		SceNpPartyAppGameInitParam initParam;
		SceNpPartyAppGameRegisteredHandlers registeredHandlers;
		SceUInt8 data[508];
	};
} PartyAppMessage;

extern "C" {

int SceNpPartyAppUtil_E64BB92C(SceNpPartyAppEventHandler handler, void *userdata)
{
	g_partySystem.m_eventHandler = handler;
	g_partySystem.m_userdata = userdata;
	return partyAppRegisterHandler(g_partySystem.m_client->getClient());
}

int sceNpPartyAppUtilityCheckCallback(void)
{
	SceSize size;
	PartyAppMessage msg;
	IPMI::Client *client;

	if (!g_partySystem.m_initialized)
		return SCE_NP_PARTY_APP_UTILITY_ERROR_NOT_INITIALIZED;
	for (;;) {
		client = g_partySystem.m_client->getClient();
		if (client == NULL)
			return SCE_NP_PARTY_APP_UTILITY_ERROR_UNKNOWN;
		if (client->tryGetMsg(PARTY_APP_MESSAGE_QUEUE, &msg, &size, sizeof(msg)) != 0)
			break;
		switch (msg.type) {
		case PARTY_APP_MESSAGE_GAME_INIT_PARAMS:
			PARTY_APP_ASSERT(size == 24, 202, "sceNpPartyAppUtilityCheckCallback");
			if (g_partySystem.m_eventHandler != NULL)
				g_partySystem.m_eventHandler(SCE_NP_PARTY_SDK_EVENT_GAME_INIT_PARAMS,
							     0, &msg.initParam,
							     g_partySystem.m_userdata);
			break;
		case PARTY_APP_MESSAGE_GAME_REGISTERED_HANDLERS:
			PARTY_APP_ASSERT(size == 28, 217, "sceNpPartyAppUtilityCheckCallback");
			if (g_partySystem.m_eventHandler != NULL)
				g_partySystem.m_eventHandler(
					SCE_NP_PARTY_SDK_EVENT_GAME_REGISTERED_HANDLERS, 0,
					&msg.registeredHandlers, g_partySystem.m_userdata);
			break;
		case PARTY_APP_MESSAGE_GAME_SESSION_TERM:
			PARTY_APP_ASSERT(size == 4, 248, "sceNpPartyAppUtilityCheckCallback");
			if (g_partySystem.m_eventHandler != NULL)
				g_partySystem.m_eventHandler(SCE_NP_PARTY_SDK_EVENT_GAME_SESSION_TERM,
							     0, NULL, g_partySystem.m_userdata);
			break;
		case PARTY_APP_MESSAGE_GAME_DISABLE_VOICE:
			PARTY_APP_ASSERT(size == 4, 263, "sceNpPartyAppUtilityCheckCallback");
			if (g_partySystem.m_eventHandler != NULL)
				g_partySystem.m_eventHandler(SCE_NP_PARTY_SDK_EVENT_GAME_DISABLE_VOICE,
							     0, NULL, g_partySystem.m_userdata);
			break;
		case PARTY_APP_MESSAGE_GAME_RE_ENABLE_VOICE:
			PARTY_APP_ASSERT(size == 4, 277, "sceNpPartyAppUtilityCheckCallback");
			if (g_partySystem.m_eventHandler != NULL)
				g_partySystem.m_eventHandler(
					SCE_NP_PARTY_SDK_EVENT_GAME_RE_ENABLE_VOICE, 0, NULL,
					g_partySystem.m_userdata);
			break;
		default:
			break;
		}
	}
	return 0;
}

}

/* the memory block the client is made in, and the mspace over it */
static inline int create_mem_block_and_mspace(void)
{
	void *base = NULL;
	int ret;

	PARTY_APP_ASSERT(g_partySystem.m_memBlockUid == 0, 27, "create_mem_block_and_mspace");
	ret = sceKernelAllocMemBlock(PARTY_APP_MEMBLOCK_NAME, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
				     PARTY_APP_MEMBLOCK_SIZE, NULL);
	if (ret < 0)
		goto error;
	g_partySystem.m_memBlockUid = ret;
	ret = sceKernelGetMemBlockBase(ret, &base);
	if (ret < 0)
		goto error_free;
	g_partySystem.m_mspace = sceClibMspaceCreate(base, PARTY_APP_MEMBLOCK_SIZE);
	if (g_partySystem.m_mspace == NULL) {
		ret = SCE_NP_PARTY_APP_UTILITY_ERROR_OUT_OF_MEMORY;
		goto error_free;
	}
	return 0;

error_free:
	sceKernelFreeMemBlock(g_partySystem.m_memBlockUid);
error:
	g_partySystem.m_memBlockUid = 0;
	return ret;
}

static inline void destroy_mem_block_and_mspace(void)
{
	sceClibMspaceDestroy(g_partySystem.m_mspace);
	g_partySystem.m_mspace = NULL;
	sceKernelFreeMemBlock(g_partySystem.m_memBlockUid);
	g_partySystem.m_memBlockUid = 0;
}

/* Sony's build frees the client's buffer as soon as the client is made in it */
static inline int create_ipmi_wrapper_client(const IpmiWrapper::Client::Config *config)
{
	int ret;

	PARTY_APP_ASSERT(g_partySystem.m_clientMemory == NULL, 79, "create_ipmi_wrapper_client");
	g_partySystem.m_clientMemory =
		sceClibMspaceMalloc(g_partySystem.m_mspace,
				    IpmiWrapper::Client::getBufferSize(config));
	ret = SCE_NP_PARTY_APP_UTILITY_ERROR_OUT_OF_MEMORY;
	if (g_partySystem.m_clientMemory != NULL) {
		ret = IpmiWrapper::Client::create(&g_partySystem.m_client, config,
						  g_partySystem.m_clientMemory);
		sceClibMspaceFree(g_partySystem.m_mspace, g_partySystem.m_clientMemory);
		g_partySystem.m_clientMemory = NULL;
	}
	return ret;
}

static inline int destroy_ipmi_wrapper_client(void)
{
	int ret;

	ret = g_partySystem.m_client->destroy();
	if (ret < 0)
		return ret;
	g_partySystem.m_client = NULL;
	sceClibMspaceFree(g_partySystem.m_mspace, g_partySystem.m_clientMemory);
	return 0;
}

extern "C" {

int sceNpPartyAppUtilityInit(void)
{
	IpmiWrapper::Client::Config config;
	int ret;

	if (g_partySystem.m_initialized)
		return SCE_NP_PARTY_APP_UTILITY_ERROR_ALREADY_INITIALIZED;
	g_partySystem.initialize();
	sceClibStrncpy(config.name, PARTY_APP_SERVICE_NAME, sizeof(config.name));
	config.callbackCount = 0;
	config.msgQueueCount = 1;
	config.msgQueue[0].msgSize = sizeof(PartyAppMessage);
	config.msgQueue[0].msgCount = PARTY_APP_MESSAGE_COUNT;
	ret = create_mem_block_and_mspace();
	if (ret < 0)
		return ret;
	ret = create_ipmi_wrapper_client(&config);
	if (ret >= 0) {
		ret = g_partySystem.m_client->connect();
		if (ret >= 0) {
			if (g_partySystem.m_client->getClient() == NULL) {
				ret = SCE_NP_PARTY_APP_UTILITY_ERROR_UNKNOWN;
			} else {
				ret = g_partySystem.InitMutex();
				if (ret >= 0) {
					ret = partyAppInit(g_partySystem.m_client->getClient());
					if (ret >= 0) {
						g_partySystem.m_initialized = true;
						return 0;
					}
				}
			}
			g_partySystem.m_client->disconnect();
		}
		destroy_ipmi_wrapper_client();
	}
	destroy_mem_block_and_mspace();
	return ret;
}

int sceNpPartyAppUtilityTerm(void)
{
	IPMI::Client *client;
	int ret;

	if (!g_partySystem.m_initialized)
		return SCE_NP_PARTY_APP_UTILITY_ERROR_NOT_INITIALIZED;
	client = g_partySystem.m_client->getClient();
	if (client == NULL)
		return SCE_NP_PARTY_APP_UTILITY_ERROR_UNKNOWN;
	ret = partyAppTerm(client);
	if (ret < 0)
		return ret;
	ret = g_partySystem.TermMutex();
	if (ret < 0)
		return ret;
	ret = g_partySystem.m_client->disconnect();
	if (ret < 0)
		return ret;
	ret = destroy_ipmi_wrapper_client();
	if (ret < 0)
		return ret;
	destroy_mem_block_and_mspace();
	g_partySystem.reset();
	return 0;
}

int sceNpPartyAppUtilityJoiningPartyEvent(SceUInt8 memberNum,
					  const SceNpPartyAppMemberInfo *selfInfo,
					  const SceNpPartyAppMemberInfo *leaderInfo,
					  const void *roomInfo, const void *roomState,
					  const void *roomSession)
{
	return partyAppJoiningPartyEvent(g_partySystem.m_client->getClient(), memberNum,
					 selfInfo, leaderInfo, roomInfo, roomState, roomSession);
}

int sceNpPartyAppUtilityMemberUpdateEvent(SceUInt32 index,
					  const SceNpPartyAppMemberInfo *memberInfo)
{
	return partyAppMemberUpdateEvent(g_partySystem.m_client->getClient(), index, memberInfo);
}

int sceNpPartyAppUtilityJoinedPartyEvent(void)
{
	return partyAppJoinedPartyEvent(g_partySystem.m_client->getClient());
}

int SceNpPartyAppUtil_74FA54BF(const SceNpPartyRoomLeftReason *reason, SceUInt8 appStopped)
{
	return partyAppLeftPartyEvent(g_partySystem.m_client->getClient(), reason, appStopped);
}

int sceNpPartyAppUtilityMemberJoinedEvent(SceUInt32 index,
					  const SceNpPartyAppMemberInfo *memberInfo)
{
	return partyAppMemberJoinedEvent(g_partySystem.m_client->getClient(), index, memberInfo);
}

int sceNpPartyAppUtilityMemberLeftEvent(SceUInt32 index,
					const SceNpPartyAppMemberInfo *memberInfo)
{
	return partyAppMemberLeftEvent(g_partySystem.m_client->getClient(), index, memberInfo);
}

int sceNpPartyAppUtilityMemberGamePresenceChangedEvent(const SceNpPartyAppMemberInfo *memberInfo)
{
	return partyAppMemberGamePresenceChangedEvent(g_partySystem.m_client->getClient(),
						      memberInfo);
}

int sceNpPartyAppUtilityMemberVoiceStateChangedEvent(const SceNpPartyMemberVoiceInfo *voiceInfo)
{
	return partyAppMemberVoiceStateChangedEvent(g_partySystem.m_client->getClient(),
						    voiceInfo);
}

int sceNpPartyAppUtilityMemberSessInvCommIdUpdateEvent(SceNpPartyRoomMemberId memberId,
						       const SceNpCommunicationId *gameSessInvCommId)
{
	return partyAppMemberSessInvCommIdUpdateEvent(g_partySystem.m_client->getClient(),
						      memberId, gameSessInvCommId);
}

int SceNpPartyAppUtil_A540F067(SceNpPartyAppMemberGameSession session)
{
	return partyAppMemberGameSessionUpdateEvent(g_partySystem.m_client->getClient(), session);
}

int sceNpPartyAppUtilityGameSessionUpdateReadyEvent(const SceNpPartyGameSessionUpdateReady *ready,
						    const void *readyInfo)
{
	return partyAppGameSessionUpdateReadyEvent(g_partySystem.m_client->getClient(), ready,
						   readyInfo);
}

}
