/*
 * SceNpPartyGameUtil (vs0:sys/external/np_party.suprx): an application's view of the party it
 * is in, through the shell's "SceNpPartyUtil" IPMI service; the shell's party events reach the
 * application's handlers through the service's message queue, polled by
 * sceNpPartyCheckCallback. Sony's file: vsh/app/shell/src/np/ipc/npparty/party_game_client/
 * party_game_client.cpp (its asserts, which name create_mem_block_and_mspace and
 * create_ipmi_wrapper_client; 0.990's records name the other helpers). The message names and
 * the other names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <np/np_error.h>
#include <np/np_party.h>
#include "party_game_system.h"
#include "party_game_ipc_client.h"

/* the build the firmware's vsh was made in, as Sony's paths name it */
#if defined(FW_TYPE_CEX)
#define PARTY_GAME_CLIENT_BUILD "J01509657"
#elif defined(FW_TYPE_TOOL)
#define PARTY_GAME_CLIENT_BUILD "J01509658"
#else
#define PARTY_GAME_CLIENT_BUILD "J01508051"
#endif

/* the assertions of this file, as Sony's build printed them */
#define PARTY_GAME_CLIENT_SOURCE_FILE	\
	"W:\\Build\\" PARTY_GAME_CLIENT_BUILD	\
	"\\vsh\\app\\shell\\src\\np\\ipc\\npparty\\party_game_client\\party_game_client.cpp"
#define PARTY_GAME_ASSERT(cond, line, func)						\
	do {										\
		if (!(cond)) {								\
			sceClibPrintf("%s:%u: [PartyGameClient] ASSERION FAILURE in %s\n", \
				      PARTY_GAME_CLIENT_SOURCE_FILE, line, func);	\
			*(volatile int *)0 = 0;						\
		}									\
	} while (0)

#define PARTY_GAME_SERVICE_NAME		"SceNpPartyUtil"
#define PARTY_GAME_MEMBLOCK_NAME	"SceNpPartyGameMemBlock"
#define PARTY_GAME_MEMBLOCK_SIZE	0x1000
#define PARTY_GAME_MESSAGE_QUEUE	0
#define PARTY_GAME_MESSAGE_COUNT	8

/* the application SDK versions the library's behaviour changes at */
#define PARTY_GAME_SDK_VERSION_GAME_SESSION	0x03300000	/* game-session events */
#define PARTY_GAME_SDK_VERSION_NO_COMM_ID	0x03500000	/* commId and presence gone */

/* the messages of the service's queue */
#define PARTY_GAME_MESSAGE_JOINED			0
#define PARTY_GAME_MESSAGE_MEMBER_JOINED		1
#define PARTY_GAME_MESSAGE_MEMBER_LEFT			2
#define PARTY_GAME_MESSAGE_LEFT				3
#define PARTY_GAME_MESSAGE_MEMBER_VOICE			4
#define PARTY_GAME_MESSAGE_MEMBER_PRESENCE_UPDATE	5
#define PARTY_GAME_MESSAGE_GAME_SESSION_MEMBER_UPDATE	6
#define PARTY_GAME_MESSAGE_GAME_SESSION_READY		7

typedef struct PartyGameSessionReadyMessage {
	SceNpPartyGameSessionUpdateReady ready;
	SceNpPartyMemberGameSessionUpdateInfo memberInfo[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt32 memberNum;
} PartyGameSessionReadyMessage;

typedef struct PartyGameMessage {
	int type;			/* PARTY_GAME_MESSAGE_* */
	union {
		SceNpPartyMemberList memberList;
		SceNpPartyMemberInfo memberInfo;
		SceNpPartyRoomLeftReason leftReason;
		SceNpPartyMemberVoiceInfo voiceInfo;
		SceNpPartyMemberGameSessionUpdateInfo sessionInfo;
		PartyGameSessionReadyMessage sessionReady;
		SceUInt8 data[636];
	};
} PartyGameMessage;

extern "C" {

int sceNpPartyCheckCallback(void)
{
	SceSize size;
	PartyGameMessage msg;
	IPMI::Client *client;
	SceUInt32 i;
	int ret;

	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	ret = 0;
	while (ret == 0) {
		client = g_partyGameSystem.m_client->getClient();
		if (client == NULL)
			return SCE_NP_PARTY_ERROR_UNKNOWN;
		ret = client->tryGetMsg(PARTY_GAME_MESSAGE_QUEUE, &msg, &size, sizeof(msg));
		if (ret != 0)
			continue;
		switch (msg.type) {
		case PARTY_GAME_MESSAGE_JOINED:
			PARTY_GAME_ASSERT(size == 296, 200, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_roomEventHandler != NULL)
				g_partyGameSystem.m_roomEventHandler(
					SCE_NP_PARTY_ROOM_EVENT_JOINED, &msg.memberList,
					g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_MEMBER_JOINED:
			PARTY_GAME_ASSERT(size == 520, 213, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_roomEventHandler != NULL)
				g_partyGameSystem.m_roomEventHandler(
					SCE_NP_PARTY_ROOM_EVENT_MEMBER_JOINED, &msg.memberInfo,
					g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_MEMBER_LEFT:
			PARTY_GAME_ASSERT(size == 520, 223, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_roomEventHandler != NULL)
				g_partyGameSystem.m_roomEventHandler(
					SCE_NP_PARTY_ROOM_EVENT_MEMBER_LEFT, &msg.memberInfo,
					g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_MEMBER_VOICE:
			PARTY_GAME_ASSERT(size == 8, 233, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_voiceEventHandler != NULL)
				g_partyGameSystem.m_voiceEventHandler(&msg.voiceInfo,
								      g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_MEMBER_PRESENCE_UPDATE:
			PARTY_GAME_ASSERT(size == 520, 243, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_sdkVersion < PARTY_GAME_SDK_VERSION_NO_COMM_ID &&
			    g_partyGameSystem.m_roomEventHandler != NULL)
				g_partyGameSystem.m_roomEventHandler(
					SCE_NP_PARTY_ROOM_EVENT_MEMBER_PRESENCE_UPDATE,
					&msg.memberInfo, g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_LEFT:
			PARTY_GAME_ASSERT(size == 8, 259, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_roomEventHandler != NULL)
				g_partyGameSystem.m_roomEventHandler(
					SCE_NP_PARTY_ROOM_EVENT_LEFT, &msg.leftReason,
					g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_GAME_SESSION_MEMBER_UPDATE:
			PARTY_GAME_ASSERT(size == 60, 270, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_gameSessionEventHandler != NULL)
				g_partyGameSystem.m_gameSessionEventHandler(
					SCE_NP_PARTY_GAME_SESSION_MEMBER_UPDATE_EVENT,
					&msg.sessionInfo, g_partyGameSystem.m_userdata);
			break;
		case PARTY_GAME_MESSAGE_GAME_SESSION_READY:
			PARTY_GAME_ASSERT(size == 444, 281, "sceNpPartyCheckCallback");
			if (g_partyGameSystem.m_gameSessionEventHandler == NULL)
				break;
			g_partyGameSystem.m_gameSessionEventHandler(
				SCE_NP_PARTY_GAME_SESSION_UPDATE_EVENT_READY,
				&msg.sessionReady.ready, g_partyGameSystem.m_userdata);
			if (msg.sessionReady.ready.result != 0)
				break;
			for (i = 0; i < msg.sessionReady.memberNum; i++)
				g_partyGameSystem.m_gameSessionEventHandler(
					SCE_NP_PARTY_GAME_SESSION_MEMBER_UPDATE_EVENT,
					&msg.sessionReady.memberInfo[i],
					g_partyGameSystem.m_userdata);
			break;
		default:
			break;
		}
	}
	return 0;
}

int sceNpPartyGetMemberSessionInfo(const SceNpPartyRoomMemberId memberId,
				   SceNpPartyMemberGameSessionUpdateInfo *memberGameSessionInfo)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (g_partyGameSystem.m_gameSessionEventHandler == NULL)
		return SCE_NP_PARTY_ERROR_GAME_SESSION_NOT_ENABLED;
	if (memberGameSessionInfo == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetMemberSessionInfo(g_partyGameSystem.m_client->getClient(), memberId,
					     memberGameSessionInfo);
}

int sceNpPartyGetState(SceNpPartyState *state)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (state == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetState(g_partyGameSystem.m_client->getClient(), state);
}

int sceNpPartyRegisterHandler(const SceNpPartyEventHandlers *handlers, void *userdata)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	g_partyGameSystem.m_roomEventHandler = handlers->roomEventHandler;
	g_partyGameSystem.m_voiceEventHandler = handlers->voiceEventHandler;
	if (g_partyGameSystem.m_sdkVersion >= PARTY_GAME_SDK_VERSION_GAME_SESSION)
		g_partyGameSystem.m_gameSessionEventHandler = handlers->gameSessionEventHandler;
	else
		g_partyGameSystem.m_gameSessionEventHandler = NULL;
	g_partyGameSystem.m_userdata = userdata;
	return partyGameRegisterHandler(g_partyGameSystem.m_client->getClient(), handlers);
}

}

/* the memory block the client is made in, and the mspace over it */
static inline int create_mem_block_and_mspace(void)
{
	void *base = NULL;
	int ret;

	PARTY_GAME_ASSERT(g_partyGameSystem.m_memBlockUid == 0, 33, "create_mem_block_and_mspace");
	ret = sceKernelAllocMemBlock(PARTY_GAME_MEMBLOCK_NAME, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
				     PARTY_GAME_MEMBLOCK_SIZE, NULL);
	if (ret < 0)
		goto error;
	g_partyGameSystem.m_memBlockUid = ret;
	ret = sceKernelGetMemBlockBase(ret, &base);
	if (ret < 0)
		goto error_free;
	g_partyGameSystem.m_mspace = sceClibMspaceCreate(base, PARTY_GAME_MEMBLOCK_SIZE);
	if (g_partyGameSystem.m_mspace == NULL) {
		ret = SCE_NP_PARTY_ERROR_OUT_OF_MEMORY;
		goto error_free;
	}
	return 0;

error_free:
	sceKernelFreeMemBlock(g_partyGameSystem.m_memBlockUid);
error:
	g_partyGameSystem.m_memBlockUid = 0;
	return ret;
}

static inline void destroy_mem_block_and_mspace(void)
{
	sceClibMspaceDestroy(g_partyGameSystem.m_mspace);
	g_partyGameSystem.m_mspace = NULL;
	sceKernelFreeMemBlock(g_partyGameSystem.m_memBlockUid);
	g_partyGameSystem.m_memBlockUid = 0;
}

/* Sony's build frees the client's buffer as soon as the client is made in it */
static inline int create_ipmi_wrapper_client(const IpmiWrapper::Client::Config *config)
{
	int ret;

	PARTY_GAME_ASSERT(g_partyGameSystem.m_clientMemory == NULL, 85,
			  "create_ipmi_wrapper_client");
	g_partyGameSystem.m_clientMemory =
		sceClibMspaceMalloc(g_partyGameSystem.m_mspace,
				    IpmiWrapper::Client::getBufferSize(config));
	ret = SCE_NP_PARTY_ERROR_OUT_OF_MEMORY;
	if (g_partyGameSystem.m_clientMemory != NULL) {
		ret = IpmiWrapper::Client::create(&g_partyGameSystem.m_client, config,
						  g_partyGameSystem.m_clientMemory);
		sceClibMspaceFree(g_partyGameSystem.m_mspace, g_partyGameSystem.m_clientMemory);
		g_partyGameSystem.m_clientMemory = NULL;
	}
	return ret;
}

static inline int destroy_ipmi_wrapper_client(void)
{
	int ret;

	ret = g_partyGameSystem.m_client->destroy();
	if (ret < 0)
		return ret;
	g_partyGameSystem.m_client = NULL;
	sceClibMspaceFree(g_partyGameSystem.m_mspace, g_partyGameSystem.m_clientMemory);
	return 0;
}

static inline int disconnect_ipmi_wrapper_client(void)
{
	if (g_partyGameSystem.m_client->getClient() == NULL)
		return SCE_NP_PARTY_ERROR_UNKNOWN;
	return g_partyGameSystem.m_client->disconnect();
}

extern "C" {

int sceNpPartyInit(const SceNpPartyInitParam *param)
{
	IpmiWrapper::Client::Config config;
	SceNpCommunicationId commId;
	int ret;

	if (param->sdkVersion >= PARTY_GAME_SDK_VERSION_NO_COMM_ID) {
		sceClibMemset(&commId, 0, sizeof(commId));
		if (sceClibMemcmp(&param->commId, &commId, sizeof(commId)) != 0)
			return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	}
	if (g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_ALREADY_INITIALIZED;
	g_partyGameSystem.initialize(param->sdkVersion);
	sceClibStrncpy(config.name, PARTY_GAME_SERVICE_NAME, sizeof(config.name));
	config.callbackCount = 0;
	config.msgQueueCount = 1;
	config.msgQueue[0].msgSize = sizeof(PartyGameMessage);
	config.msgQueue[0].msgCount = PARTY_GAME_MESSAGE_COUNT;
	ret = create_mem_block_and_mspace();
	if (ret < 0)
		return ret;
	ret = create_ipmi_wrapper_client(&config);
	if (ret >= 0) {
		ret = g_partyGameSystem.m_client->connect();
		if (ret >= 0) {
			if (g_partyGameSystem.m_client->getClient() == NULL) {
				ret = SCE_NP_PARTY_ERROR_UNKNOWN;
			} else {
				ret = g_partyGameSystem.InitMutex();
				if (ret >= 0) {
					ret = partyGameInit(g_partyGameSystem.m_client->getClient(),
							    param);
					if (ret == 0) {
						g_partyGameSystem.m_initialized = true;
						return 0;
					}
					g_partyGameSystem.TermMutex();
				}
			}
			g_partyGameSystem.m_client->disconnect();
		}
		destroy_ipmi_wrapper_client();
	}
	destroy_mem_block_and_mspace();
	return ret;
}

int sceNpPartyTerm(void)
{
	IPMI::Client *client;
	int ret;

	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	client = g_partyGameSystem.m_client->getClient();
	if (client == NULL)
		return SCE_NP_PARTY_ERROR_UNKNOWN;
	ret = partyGameTerm(client);
	if (ret < 0)
		return ret;
	ret = g_partyGameSystem.TermMutex();
	if (ret < 0)
		return ret;
	ret = disconnect_ipmi_wrapper_client();
	if (ret < 0)
		return ret;
	ret = destroy_ipmi_wrapper_client();
	if (ret < 0)
		return ret;
	destroy_mem_block_and_mspace();
	g_partyGameSystem.reset();
	return 0;
}

int sceNpPartyGetMemberInfo(const SceNpId *npId, SceNpPartyMemberInfo *memberInfo)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (npId == NULL || memberInfo == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetMemberInfo(g_partyGameSystem.m_client->getClient(), npId, memberInfo);
}

int sceNpPartyGetMembers(SceNpPartyMemberList *memberList)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (memberList == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetMembers(g_partyGameSystem.m_client->getClient(), memberList);
}

int sceNpPartyGetMemberVoiceInfo(const SceNpPartyRoomMemberId memberId,
				 SceNpPartyMemberVoiceState *memberVoiceState)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (g_partyGameSystem.m_voiceEventHandler == NULL)
		return SCE_NP_PARTY_ERROR_VOICE_NOT_ENABLED;
	if (memberVoiceState == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetMemberVoiceInfo(g_partyGameSystem.m_client->getClient(), memberId,
					   memberVoiceState);
}

int SceNpPartyGameUtil_6370CEB6(void)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	return partyGameRequest110018(g_partyGameSystem.m_client->getClient());
}

int SceNpPartyGameUtil_089134D1(void)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	return partyGameRequest110019(g_partyGameSystem.m_client->getClient());
}

int sceNpPartyGetId(SceNpPartyId *const id)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (id == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetId(g_partyGameSystem.m_client->getClient(), id);
}

int sceNpPartyGetGameSessionReadyState(SceNpPartyGameSessionUpdateReady *readyState)
{
	if (!g_partyGameSystem.m_initialized)
		return SCE_NP_PARTY_ERROR_NOT_INITIALIZED;
	if (g_partyGameSystem.m_gameSessionEventHandler == NULL)
		return SCE_NP_PARTY_ERROR_GAME_SESSION_NOT_ENABLED;
	if (readyState == NULL)
		return SCE_NP_PARTY_ERROR_INVALID_ARGUMENT;
	return partyGameGetGameSessionReadyState(g_partyGameSystem.m_client->getClient(),
						 readyState);
}

}
