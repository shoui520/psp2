/*
 * np_party_ipc_server.cpp: "SceNpPartyUtil" IPMI server bridging the Party app,
 * game clients (SceNpPartyGameUtil), and VSH observer clients.
 * 3.65-CEX: 0x813C9BA4 - 0x813CB46F
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <ipmi_wrapper/ipmi_wrapper_server.h>

#include "np_party_vsh_internal.h"

#define NP_PARTY_IPC_SERVER_NAME		"SceNpPartyUtil"
#define NP_PARTY_IPC_THREAD_PRIORITY		80
#define NP_PARTY_IPC_THREAD_STACK_SIZE		4096
#define NP_PARTY_IPC_MAX_SESSIONS		16
#define NP_PARTY_IPC_SYNC_BUF_SIZE		3840
#define NP_PARTY_IPC_MSG_QUEUE_SIZE		640
#define NP_PARTY_IPC_MSG_QUEUE_DEPTH		8

extern int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
						  SceUInt32 methodId, const IPMI::DataInfo *inList,
						  SceUInt32 inCount);	/* 0x8115B554 */

struct PartyAppGameInitMsg {
	SceUInt32 eventType;
	SceNpPartyInitParam initParam;
	SceUID gameAppId;
};

struct PartyAppGameHandlersMsg {
	SceUInt32 eventType;
	NpPartyGameHandlerFlags handlers;
};

struct PartyGameJoinedMsg {
	SceUInt32 eventType;
	SceNpPartyMemberList memberList;
};

struct PartyGameMemberInfoMsg {
	SceUInt32 eventType;
	SceNpPartyMemberInfo memberInfo;
};

struct PartyGameMemberVoiceMsg {
	SceUInt32 eventType;
	SceNpPartyMemberVoiceInfo voiceInfo;
};

struct PartyGameMemberSessionMsg {
	SceUInt32 eventType;
	SceUInt32 alignmentWord;
	SceNpPartyMemberGameSessionUpdateInfo sessionInfo;
};

struct PartyGameSessionReadyMsg {
	SceUInt32 eventType;
	SceUInt32 result;
	SceNpPartyMemberGameSessionUpdateInfo members[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt32 memberCount;
};

struct PartyGameLeftMsg {
	SceUInt32 eventType;
	SceNpPartyRoomLeftReason reason;
};

static IPMI::Session *s_partyAppSession = NULL;					/* 0x81549370 */
static IPMI::Session *s_partyGameSession = NULL;				/* 0x81549374 */
static IpmiWrapper::Server *s_partyIpmiServer = NULL;				/* 0x81549378 */

static SceUInt8 s_partyServerInitialized = 0;					/* 0x815CC440 */
static SceUInt8 s_observerClientCount = 0;					/* 0x815CC441 */
static IPMI::Session *s_observerSessions[NP_PARTY_OBSERVER_CLIENT_MAX];		/* 0x815CC444 */

static NpPartyAppRoomSnapshot s_stagingRoom;					/* 0x815CC450 */
static char s_stagingRoomSessionId[NP_PARTY_ROOM_SESSION_ID_SIZE];		/* 0x815CD9E8 */

static void sendGameQueueMessage(const void *message, SceSize size)
{
	if (s_partyGameSession == NULL)
		return;
	int ret = s_partyGameSession->trySendMsg(0, message, size);
	if (ret == NP_PARTY_IPMI_ERROR_QUEUE_FULL) {
		s_partyGameSession->emptyMsgQueue(0);
		s_partyGameSession->trySendMsg(0, message, size);
	}
}

static bool isObserverRegistered(IPMI::Session *session)
{
	for (SceUInt8 i = 0; i < s_observerClientCount; ++i) {
		if (s_observerSessions[i] == session)
			return true;
	}
	return false;
}

/* 0x813C9D0A */
static int handleObserverRegister(IPMI::Session *session)
{
	if (isObserverRegistered(session))
		return NP_PARTY_OBSERVER_ERROR_ALREADY_REGISTERED;
	if (s_observerClientCount >= NP_PARTY_OBSERVER_CLIENT_MAX)
		return NP_PARTY_OBSERVER_ERROR_MAX_CLIENTS;

	s_observerSessions[s_observerClientCount] = session;
	++s_observerClientCount;
	return 0;
}

/* 0x813C9D6A */
static int handleObserverGetMembers(IPMI::Session *session, NpPartyObserverMemberList *outList)
{
	sce_paf_memset(outList, 0, sizeof(*outList));
	if (!isObserverRegistered(session))
		return NP_PARTY_OBSERVER_ERROR_NOT_REGISTERED;
	return npPartyVshGetObserverMembers(outList);
}

/* 0x813C9DBA */
static int handleObserverUnregister(IPMI::Session *session)
{
	for (SceUInt8 i = 0; i < s_observerClientCount; ++i) {
		if (s_observerSessions[i] != session)
			continue;

		if (i + 1 == s_observerClientCount) {
			s_observerSessions[i] = NULL;
		} else {
			sce_paf_memmove(&s_observerSessions[i], &s_observerSessions[i + 1],
					sizeof(IPMI::Session *) * (s_observerClientCount - 1 - i));
			s_observerSessions[NP_PARTY_OBSERVER_CLIENT_MAX - 1] = NULL;
		}
		--s_observerClientCount;
		return 0;
	}
	return NP_PARTY_OBSERVER_ERROR_NOT_FOUND;
}

/* 0x813C9E2A */
static int handleAppInit(IPMI::Session *session)
{
	s_partyAppSession = session;
	sce_paf_memset(&s_stagingRoom, 0, sizeof(s_stagingRoom));
	sce_paf_memset(s_stagingRoomSessionId, 0, sizeof(s_stagingRoomSessionId));
	return npPartyVshOnAppInit();
}

/* 0x813C9E5E */
static int handleAppRegisterHandler(IPMI::Session *session)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	return npPartyVshOnAppRegisterHandler();
}

/* 0x813C9E8A */
static int handleAppTerm(IPMI::Session *session)
{
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	s_partyAppSession = NULL;
	return 0;
}

/* 0x813C9EB6 */
static int handleAppJoiningParty(IPMI::Session *session, SceUInt8 memberNum,
				 const NpPartyAppMemberRecord *selfInfo,
				 const NpPartyAppMemberRecord *leaderInfo,
				 const void *roomInfo, const SceUInt32 *roomState,
				 const char *roomSessionId)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	if (memberNum > SCE_NP_PARTY_MEMBER_NUM_MAX)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	sce_paf_memset(&s_stagingRoom, 0, sizeof(s_stagingRoom));
	sce_paf_memset(s_stagingRoomSessionId, 0, sizeof(s_stagingRoomSessionId));
	s_stagingRoom.memberNum = memberNum;
	s_stagingRoom.selfInfo = *selfInfo;
	s_stagingRoom.leaderInfo = *leaderInfo;
	sce_paf_memcpy(s_stagingRoom.roomInfo, roomInfo, NP_PARTY_ROOM_INFO_SIZE);
	s_stagingRoom.roomState = *roomState;
	sce_paf_memcpy(s_stagingRoomSessionId, roomSessionId, sizeof(s_stagingRoomSessionId));
	return 0;
}

/* 0x813C9F78 */
static int handleAppMemberUpdate(IPMI::Session *session, SceUInt32 memberIndex,
				 const NpPartyAppMemberRecord *memberInfo)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	if (memberIndex >= s_stagingRoom.memberNum)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	s_stagingRoom.members[memberIndex] = *memberInfo;
	return 0;
}

/* 0x813C9FD8 */
static int handleAppJoinedParty(IPMI::Session *session)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	return npPartyVshOnAppJoinedParty(&s_stagingRoom, s_stagingRoomSessionId);
}

/* 0x813CA02A */
static int handleAppLeftParty(IPMI::Session *session, const SceNpPartyRoomLeftReason *reason,
			      SceUInt8 appStopped)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	sce_paf_memset(&s_stagingRoom, 0, sizeof(s_stagingRoom));
	return npPartyVshOnAppLeftParty(reason, appStopped);
}

/* 0x813CA072 */
static int handleAppMemberJoined(IPMI::Session *session, SceUInt32 newMemberCount,
				 const NpPartyAppMemberRecord *memberInfo)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;
	if (newMemberCount != s_stagingRoom.memberNum + 1)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	s_stagingRoom.members[s_stagingRoom.memberNum] = *memberInfo;
	s_stagingRoom.memberNum = newMemberCount;
	return npPartyVshOnAppMemberJoined(&s_stagingRoom, memberInfo);
}

/* 0x813CA0E4 */
static int handleAppMemberLeft(IPMI::Session *session, SceUInt32 newMemberCount,
			       const NpPartyAppMemberRecord *memberInfo)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	SceUInt32 matchIndex = 0;
	while (matchIndex < s_stagingRoom.memberNum) {
		if (s_stagingRoom.members[matchIndex].memberInfo.memberId ==
		    memberInfo->memberInfo.memberId)
			break;
		++matchIndex;
	}

	if (matchIndex == s_stagingRoom.memberNum) {
		sce_paf_memset(&s_stagingRoom.members[matchIndex], 0,
			       sizeof(NpPartyAppMemberRecord));
	} else if (matchIndex + 1 < s_stagingRoom.memberNum) {
		sce_paf_memmove(&s_stagingRoom.members[matchIndex],
				&s_stagingRoom.members[matchIndex + 1],
				sizeof(NpPartyAppMemberRecord) *
				(s_stagingRoom.memberNum - 1 - matchIndex));
	}
	s_stagingRoom.memberNum = newMemberCount;
	return npPartyVshOnAppMemberLeft(&s_stagingRoom, memberInfo);
}

/* 0x813CA194 */
static int handleAppMemberPresenceChanged(IPMI::Session *session,
					  const NpPartyAppMemberRecord *memberInfo)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	SceUInt32 matchIndex = 0;
	while (matchIndex < s_stagingRoom.memberNum) {
		if (s_stagingRoom.members[matchIndex].memberInfo.memberId ==
		    memberInfo->memberInfo.memberId)
			break;
		++matchIndex;
	}
	if (matchIndex == s_stagingRoom.memberNum)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	s_stagingRoom.members[matchIndex] = *memberInfo;
	return npPartyVshOnAppMemberPresenceChanged(&s_stagingRoom, memberInfo);
}

/* 0x813CA21C */
static int handleAppMemberVoiceChanged(IPMI::Session *session,
				       const SceNpPartyMemberVoiceInfo *voiceInfo)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	SceUInt32 matchIndex = 0;
	while (matchIndex < s_stagingRoom.memberNum) {
		if (s_stagingRoom.members[matchIndex].memberInfo.memberId == voiceInfo->memberId)
			break;
		++matchIndex;
	}
	if (matchIndex == s_stagingRoom.memberNum)
		return NP_PARTY_APP_ERROR_INVALID_ARGUMENT;

	return npPartyVshOnAppMemberVoiceChanged(voiceInfo);
}

/* 0x813CA290 */
static int handleAppMemberSessCommIdUpdate(IPMI::Session *session,
					   SceNpPartyRoomMemberId memberId,
					   const SceNpCommunicationId *commId)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	return npPartyVshOnAppMemberSessCommIdUpdate(memberId, commId);
}

/* 0x813CA2C6 */
static int handleAppMemberGameSessionUpdate(IPMI::Session *session,
					    const NpPartyAppGameSessionPayload *payload)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	for (SceUInt32 i = 0; i < s_stagingRoom.memberNum; ++i) {
		if (s_stagingRoom.members[i].memberInfo.memberId ==
		    payload->sessionInfo.memberId) {
			sce_paf_memcpy(s_stagingRoom.members[i].memberInfo.gamePresence.title,
				       payload->gamePresenceData,
				       sizeof(payload->gamePresenceData));
			break;
		}
	}
	return npPartyVshOnAppMemberGameSessionUpdate(payload);
}

/* 0x813CA484 */
static int handleAppGameSessionUpdateReady(IPMI::Session *session,
					   const SceNpPartyGameSessionUpdateReady *ready,
					   const SceNpCommunicationId *gameCommId)
{
	if (s_partyAppSession == NULL)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;
	if (session != s_partyAppSession)
		return NP_PARTY_APP_ERROR_ACCESS_DENIED;

	return npPartyVshOnAppGameSessionUpdateReady(ready, gameCommId);
}

/* 0x813CA4BA */
static int handleGameInit(IPMI::Session *session, const SceNpPartyInitParam *initParam)
{
	if (s_partyGameSession != NULL)
		return NP_PARTY_GAME_ERROR_ALREADY_INITIALIZED;

	s_partyGameSession = session;
	SceUID gameAppId = session->getAppId();
	return npPartyVshOnGameInit(initParam, gameAppId);
}

/* 0x813CA4F0 */
static int handleGameTerm(IPMI::Session *session)
{
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	s_partyGameSession = NULL;
	return npPartyVshOnGameTerm();
}

/* 0x813CA51C */
static int handleGameGetState(IPMI::Session *session, SceNpPartyState *outState)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	return npPartyVshGetState(outState);
}

/* 0x813CA54A */
static int handleGameRegisterHandler(IPMI::Session *session,
				     const SceNpPartyEventHandlers *handlers)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	NpPartyGameHandlerFlags flags;
	sce_paf_memset(&flags, 0, sizeof(flags));
	flags.roomEvents = (handlers->roomEventHandler != NULL) ? 1 : 0;
	flags.voiceEvents = (handlers->voiceEventHandler != NULL) ? 1 : 0;
	flags.gameSessionEvents = (handlers->gameSessionEventHandler != NULL) ? 1 : 0;
	flags.gameSessionSystemReady = 0;
	flags.gameSessionReadyValid = 0;
	return npPartyVshOnGameRegisterHandlers(&flags);
}

/* 0x813CA5D2 */
static int handleGameGetMembers(IPMI::Session *session, SceNpPartyMemberList *outMembers)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_GAME_ERROR_NOT_IN_PARTY;

	return npPartyVshGetMembers(outMembers);
}

/* 0x813CA616 */
static int handleGameGetMemberInfo(IPMI::Session *session, const SceNpId *npId,
				   SceNpPartyMemberInfo *outInfo)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_GAME_ERROR_NOT_IN_PARTY;

	return npPartyVshGetMemberInfo(npId, outInfo);
}

/* 0x813CA660 */
static int handleGameGetMemberVoiceInfo(IPMI::Session *session,
					SceNpPartyRoomMemberId memberId,
					SceNpPartyMemberVoiceState *outVoiceState)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_GAME_ERROR_NOT_IN_PARTY;

	return npPartyVshGetMemberVoiceInfo(memberId, outVoiceState);
}

/* 0x813CA6AC */
static int handleGameGetMemberSessionInfo(IPMI::Session *session,
					  SceNpPartyRoomMemberId memberId,
					  SceNpPartyMemberGameSessionUpdateInfo *outInfo)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_GAME_ERROR_NOT_IN_PARTY;

	return npPartyVshGetMemberSessionInfo(memberId, outInfo);
}

/* 0x813CA6F8 */
static int handleGameGetId(IPMI::Session *session, SceNpPartyId *outId)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;
	if (s_stagingRoom.memberNum == 0)
		return NP_PARTY_GAME_ERROR_NOT_IN_PARTY;

	return npPartyVshGetPartyId(outId);
}

/* 0x813CA73C */
static int handleGameGetSessionReadyState(IPMI::Session *session,
					  SceNpPartyGameSessionUpdateReady *outReady)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	return npPartyVshGetGameSessionReadyState(outReady);
}

/* 0x813CA76A */
static int handleGameDisableVoice(IPMI::Session *session)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	return npPartyVshDisableVoice();
}

/* 0x813CA796 */
static int handleGameReEnableVoice(IPMI::Session *session)
{
	if (s_partyGameSession == NULL)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;
	if (session != s_partyGameSession)
		return NP_PARTY_GAME_ERROR_ACCESS_DENIED;

	return npPartyVshReEnableVoice();
}

/* 0x813CA7C4 */
static void onPartySessionDisconnected(IPMI::Session *session)
{
	if (session == s_partyAppSession) {
		SceNpPartyRoomLeftReason reason = SCE_NP_PARTY_ROOM_LEFT_REASON_SERVER_ERROR;
		s_partyAppSession = NULL;
		npPartyVshOnAppLeftParty(&reason, 1);
	} else if (session == s_partyGameSession) {
		s_partyGameSession = NULL;
		npPartyVshOnGameTerm();
	} else {
		handleObserverUnregister(session);
	}
}

/* 0x813CAFAA */
static int onPartySyncMethod(IPMI::Session *session, SceUInt32 methodId,
			     const IPMI::DataInfo *inList, SceUInt32 inCount,
			     IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int result = 0;

	switch (methodId) {
	case NP_PARTY_IPC_APP_INIT:
		result = handleAppInit(session);
		break;
	case NP_PARTY_IPC_APP_REGISTER_HANDLER:
		result = handleAppRegisterHandler(session);
		break;
	case NP_PARTY_IPC_APP_TERM:
		result = handleAppTerm(session);
		break;
	case NP_PARTY_IPC_APP_JOINING_PARTY:
		result = handleAppJoiningParty(
			session,
			*static_cast<const SceUInt8 *>(inList[0].pData),
			static_cast<const NpPartyAppMemberRecord *>(inList[1].pData),
			static_cast<const NpPartyAppMemberRecord *>(inList[2].pData),
			inList[3].pData,
			static_cast<const SceUInt32 *>(inList[4].pData),
			static_cast<const char *>(inList[5].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_UPDATE:
		result = handleAppMemberUpdate(
			session,
			*static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const NpPartyAppMemberRecord *>(inList[1].pData));
		break;
	case NP_PARTY_IPC_APP_JOINED_PARTY:
		result = handleAppJoinedParty(session);
		break;
	case NP_PARTY_IPC_APP_LEFT_PARTY:
		result = handleAppLeftParty(
			session,
			static_cast<const SceNpPartyRoomLeftReason *>(inList[1].pData),
			*static_cast<const SceUInt8 *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_JOINED:
		result = handleAppMemberJoined(
			session,
			*static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const NpPartyAppMemberRecord *>(inList[1].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_LEFT:
		result = handleAppMemberLeft(
			session,
			*static_cast<const SceUInt32 *>(inList[0].pData),
			static_cast<const NpPartyAppMemberRecord *>(inList[1].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_PRESENCE_CHANGED:
		result = handleAppMemberPresenceChanged(
			session,
			static_cast<const NpPartyAppMemberRecord *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_VOICE_CHANGED:
		result = handleAppMemberVoiceChanged(
			session,
			static_cast<const SceNpPartyMemberVoiceInfo *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_SESS_COMM_ID_UPDATE:
		result = handleAppMemberSessCommIdUpdate(
			session,
			*static_cast<const SceNpPartyRoomMemberId *>(inList[0].pData),
			static_cast<const SceNpCommunicationId *>(inList[1].pData));
		break;
	case NP_PARTY_IPC_APP_MEMBER_GAME_SESSION_UPDATE:
		result = handleAppMemberGameSessionUpdate(
			session,
			static_cast<const NpPartyAppGameSessionPayload *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_APP_GAME_SESSION_UPDATE_READY:
		result = handleAppGameSessionUpdateReady(
			session,
			static_cast<const SceNpPartyGameSessionUpdateReady *>(inList[0].pData),
			static_cast<const SceNpCommunicationId *>(inList[1].pData));
		break;
	case NP_PARTY_IPC_GAME_INIT:
		result = handleGameInit(
			session,
			static_cast<const SceNpPartyInitParam *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_GAME_REGISTER_HANDLER:
		result = handleGameRegisterHandler(
			session,
			static_cast<const SceNpPartyEventHandlers *>(inList[0].pData));
		break;
	case NP_PARTY_IPC_GAME_TERM:
		result = handleGameTerm(session);
		break;
	case NP_PARTY_IPC_GAME_GET_STATE:
		result = handleGameGetState(
			session,
			static_cast<SceNpPartyState *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyState);
		break;
	case NP_PARTY_IPC_GAME_GET_MEMBERS:
		result = handleGameGetMembers(
			session,
			static_cast<SceNpPartyMemberList *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyMemberList);
		break;
	case NP_PARTY_IPC_GAME_GET_MEMBER_INFO:
		result = handleGameGetMemberInfo(
			session,
			static_cast<const SceNpId *>(inList[0].pData),
			static_cast<SceNpPartyMemberInfo *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyMemberInfo);
		break;
	case NP_PARTY_IPC_GAME_GET_MEMBER_VOICE_INFO:
		result = handleGameGetMemberVoiceInfo(
			session,
			*static_cast<const SceNpPartyRoomMemberId *>(inList[0].pData),
			static_cast<SceNpPartyMemberVoiceState *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyMemberVoiceState);
		break;
	case NP_PARTY_IPC_GAME_GET_MEMBER_SESSION_INFO:
		result = handleGameGetMemberSessionInfo(
			session,
			*static_cast<const SceNpPartyRoomMemberId *>(inList[0].pData),
			static_cast<SceNpPartyMemberGameSessionUpdateInfo *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyMemberGameSessionUpdateInfo);
		break;
	case NP_PARTY_IPC_GAME_GET_ID:
		result = handleGameGetId(
			session,
			static_cast<SceNpPartyId *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyId);
		break;
	case NP_PARTY_IPC_GAME_GET_SESSION_READY_STATE:
		result = handleGameGetSessionReadyState(
			session,
			static_cast<SceNpPartyGameSessionUpdateReady *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(SceNpPartyGameSessionUpdateReady);
		break;
	case NP_PARTY_IPC_GAME_DISABLE_VOICE:
		result = handleGameDisableVoice(session);
		break;
	case NP_PARTY_IPC_GAME_RE_ENABLE_VOICE:
		result = handleGameReEnableVoice(session);
		break;
	case NP_PARTY_IPC_OBSERVER_REGISTER:
		result = handleObserverRegister(session);
		break;
	case NP_PARTY_IPC_OBSERVER_GET_MEMBERS:
		result = handleObserverGetMembers(
			session,
			static_cast<NpPartyObserverMemberList *>(outList[0].pBuffer));
		outList[0].outSize = sizeof(NpPartyObserverMemberList);
		break;
	case NP_PARTY_IPC_OBSERVER_UNREGISTER:
		result = handleObserverUnregister(session);
		break;
	default:
		return NP_PARTY_IPMI_ERROR_UNSUPPORTED_METHOD;
	}

	return session->respondToSyncMethodRequest(result, outList, outCount);
}

/* 0x813C9BA4 */
int npPartyIpcServerInit(void)
{
	IpmiWrapper::Server::Config config;
	sce_paf_memset(&config, 0, sizeof(config));
	sce_paf_strncpy(config.name, NP_PARTY_IPC_SERVER_NAME, sizeof(config.name));
	config.threadPriority = NP_PARTY_IPC_THREAD_PRIORITY;
	config.threadStackSize = NP_PARTY_IPC_THREAD_STACK_SIZE;
	config.maxSessions = NP_PARTY_IPC_MAX_SESSIONS;
	config.syncCallCount = 1;
	config.maxSyncCallInSize = NP_PARTY_IPC_SYNC_BUF_SIZE;
	config.maxSyncCallOutSize = NP_PARTY_IPC_SYNC_BUF_SIZE;
	config.asyncCallCount = 1;
	config.maxAsyncCallInSize = NP_PARTY_IPC_SYNC_BUF_SIZE;
	config.maxAsyncCallOutSize = NP_PARTY_IPC_SYNC_BUF_SIZE;
	config.callbackCount = 0;
	config.msgQueueCount = 1;
	config.msgQueue[0].msgSize = NP_PARTY_IPC_MSG_QUEUE_SIZE;
	config.msgQueue[0].msgCount = NP_PARTY_IPC_MSG_QUEUE_DEPTH;
	config.callbacks.onSyncMethod = onPartySyncMethod;
	config.callbacks.onAsyncMethod = shellIncomingDialogServerAsyncDispatch;
	config.callbacks.onSessionKilled = onPartySessionDisconnected;

	SceSize bufferSize = IpmiWrapper::Server::getBufferSize(&config);
	void *workMemory = sce_paf_malloc(bufferSize);
	int ret;
	if (workMemory != NULL) {
		ret = IpmiWrapper::Server::create(&s_partyIpmiServer, &config, workMemory);
		if (ret == 0) {
			ret = s_partyIpmiServer->start();
			if (ret == 0) {
				s_partyAppSession = NULL;
				s_partyGameSession = NULL;
				s_observerClientCount = 0;
				s_observerSessions[0] = NULL;
				s_observerSessions[1] = NULL;
				s_partyServerInitialized = 1;
				return 0;
			}
		}
	} else {
		ret = NP_PARTY_APP_ERROR_OUT_OF_MEMORY;
	}

	if (s_partyIpmiServer != NULL) {
		s_partyIpmiServer->destroy();
		sce_paf_free(workMemory);
	}
	return ret;
}

/* 0x813CA890 */
int npPartyIpcNotifyObserversChanged(void)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	SceUInt32 eventCode = 0;
	for (SceUInt8 i = 0; i < s_observerClientCount; ++i) {
		if (s_observerSessions[i] != NULL)
			s_observerSessions[i]->trySendMsg(0, &eventCode, sizeof(eventCode));
	}
	return 0;
}

/* 0x813CA908 */
int npPartyIpcSendAppGameInit(const SceNpPartyInitParam *initParam, SceUID gameAppId)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;

	PartyAppGameInitMsg message;
	message.eventType = NP_PARTY_APP_QUEUE_GAME_INIT;
	message.initParam = *initParam;
	message.gameAppId = gameAppId;
	if (s_partyAppSession != NULL)
		s_partyAppSession->trySendMsg(0, &message, sizeof(message));
	return 0;
}

/* 0x813CA96C */
int npPartyIpcSendAppGameHandlers(const NpPartyGameHandlerFlags *handlers)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;

	PartyAppGameHandlersMsg message;
	message.eventType = NP_PARTY_APP_QUEUE_GAME_HANDLERS;
	message.handlers = *handlers;
	if (s_partyAppSession != NULL)
		s_partyAppSession->trySendMsg(0, &message, sizeof(message));
	return 0;
}

/* 0x813CA9CC */
int npPartyIpcSendAppGameTerm(void)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;

	if (s_partyAppSession != NULL) {
		SceUInt32 eventType = NP_PARTY_APP_QUEUE_GAME_TERM;
		s_partyAppSession->trySendMsg(0, &eventType, sizeof(eventType));
	}
	return 0;
}

/* 0x813CAA22 */
int npPartyIpcSendAppDisableVoice(void)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;

	if (s_partyAppSession != NULL) {
		SceUInt32 eventType = NP_PARTY_APP_QUEUE_DISABLE_VOICE;
		s_partyAppSession->trySendMsg(0, &eventType, sizeof(eventType));
	}
	return 0;
}

/* 0x813CAA78 */
int npPartyIpcSendAppReEnableVoice(void)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_APP_ERROR_NOT_INITIALIZED;

	if (s_partyAppSession != NULL) {
		SceUInt32 eventType = NP_PARTY_APP_QUEUE_RE_ENABLE_VOICE;
		s_partyAppSession->trySendMsg(0, &eventType, sizeof(eventType));
	}
	return 0;
}

/* 0x813CAACE */
int npPartyIpcSendGameJoined(const SceNpPartyMemberList *memberList)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameJoinedMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_JOINED;
	message.memberList = *memberList;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CAB60 */
int npPartyIpcSendGameMemberJoined(const SceNpPartyMemberInfo *memberInfo)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameMemberInfoMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_MEMBER_JOINED;
	message.memberInfo = *memberInfo;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CABF6 */
int npPartyIpcSendGameMemberLeft(const SceNpPartyMemberInfo *memberInfo)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameMemberInfoMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_MEMBER_LEFT;
	message.memberInfo = *memberInfo;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CAC8C */
int npPartyIpcSendGameMemberVoice(const SceNpPartyMemberVoiceInfo *voiceInfo)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameMemberVoiceMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_MEMBER_VOICE;
	message.voiceInfo = *voiceInfo;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CAD18 */
int npPartyIpcSendGameMemberPresence(const SceNpPartyMemberInfo *memberInfo)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameMemberInfoMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_MEMBER_PRESENCE;
	message.memberInfo = *memberInfo;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CADAE */
int npPartyIpcSendGameMemberSession(const SceNpPartyMemberGameSessionUpdateInfo *info)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameMemberSessionMsg message;
	sce_paf_memset(&message, 0, sizeof(message));
	message.eventType = NP_PARTY_GAME_QUEUE_MEMBER_SESSION;
	message.sessionInfo = *info;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CAE5C */
int npPartyIpcSendGameSessionReady(const SceNpPartyGameSessionUpdateReady *ready,
				   const SceNpPartyMemberGameSessionUpdateInfo *members,
				   SceUInt8 memberCount)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	PartyGameSessionReadyMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_SESSION_READY;
	message.result = ready->result;
	sce_paf_memcpy(message.members, members, sizeof(message.members));
	message.memberCount = memberCount;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}

/* 0x813CAEF6 */
int npPartyIpcSendGameLeft(const SceNpPartyRoomLeftReason *reason)
{
	if (s_partyServerInitialized == 0)
		return NP_PARTY_GAME_ERROR_NOT_INITIALIZED;

	sce_paf_memset(&s_stagingRoom, 0, sizeof(s_stagingRoom));
	sce_paf_memset(s_stagingRoomSessionId, 0, sizeof(s_stagingRoomSessionId));

	PartyGameLeftMsg message;
	message.eventType = NP_PARTY_GAME_QUEUE_LEFT;
	message.reason = *reason;
	sendGameQueueMessage(&message, sizeof(message));
	return 0;
}
