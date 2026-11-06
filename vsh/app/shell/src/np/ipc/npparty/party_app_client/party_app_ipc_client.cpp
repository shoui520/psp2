/*
 * The shell's party app methods over the client's IPMI connection. Sony's build keeps them after
 * the client's state, not inlined; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "party_app_ipc_client.h"

#define PARTY_APP_METHOD_INIT					0x110000
#define PARTY_APP_METHOD_REGISTER_HANDLER			0x110001
#define PARTY_APP_METHOD_TERM					0x110002
#define PARTY_APP_METHOD_JOINING_PARTY_EVENT			0x110003
#define PARTY_APP_METHOD_MEMBER_UPDATE_EVENT			0x110004
#define PARTY_APP_METHOD_JOINED_PARTY_EVENT			0x110005
#define PARTY_APP_METHOD_LEFT_PARTY_EVENT			0x110006
#define PARTY_APP_METHOD_MEMBER_JOINED_EVENT			0x110007
#define PARTY_APP_METHOD_MEMBER_LEFT_EVENT			0x110008
#define PARTY_APP_METHOD_MEMBER_GAME_PRESENCE_CHANGED_EVENT	0x110009
#define PARTY_APP_METHOD_MEMBER_VOICE_STATE_CHANGED_EVENT	0x11000A
#define PARTY_APP_METHOD_MEMBER_SESS_INV_COMM_ID_UPDATE_EVENT	0x11000B
#define PARTY_APP_METHOD_MEMBER_GAME_SESSION_UPDATE_EVENT	0x11000C
#define PARTY_APP_METHOD_GAME_SESSION_UPDATE_READY_EVENT	0x11000D

/* a method without arguments */
static inline int partyAppCall(IPMI::Client *client, SceUInt32 method)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(method, (const IPMI::DataInfo *)NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppInit(IPMI::Client *client)
{
	return partyAppCall(client, PARTY_APP_METHOD_INIT);
}

int partyAppRegisterHandler(IPMI::Client *client)
{
	return partyAppCall(client, PARTY_APP_METHOD_REGISTER_HANDLER);
}

int partyAppTerm(IPMI::Client *client)
{
	return partyAppCall(client, PARTY_APP_METHOD_TERM);
}

int partyAppJoiningPartyEvent(IPMI::Client *client, SceUInt8 memberNum,
			      const SceNpPartyAppMemberInfo *selfInfo,
			      const SceNpPartyAppMemberInfo *leaderInfo, const void *roomInfo,
			      const void *roomState, const void *roomSession)
{
	int result = 0;
	SceUInt8 num = memberNum;
	IPMI::DataInfo in[6];
	int ret;

	in[0].pData = &num;
	in[0].dataSize = sizeof(num);
	in[1].pData = selfInfo;
	in[1].dataSize = sizeof(*selfInfo);
	in[2].pData = leaderInfo;
	in[2].dataSize = sizeof(*leaderInfo);
	in[3].pData = roomInfo;
	in[3].dataSize = SCE_NP_PARTY_APP_ROOM_INFO_SIZE;
	in[4].pData = roomState;
	in[4].dataSize = SCE_NP_PARTY_APP_ROOM_STATE_SIZE;
	in[5].pData = roomSession;
	in[5].dataSize = SCE_NP_PARTY_APP_ROOM_SESSION_SIZE;
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_JOINING_PARTY_EVENT, in, 6, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberUpdateEvent(IPMI::Client *client, SceUInt32 index,
			      const SceNpPartyAppMemberInfo *memberInfo)
{
	int result = 0;
	SceUInt32 i = index;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &i;
	in[0].dataSize = sizeof(i);
	in[1].pData = memberInfo;
	in[1].dataSize = sizeof(*memberInfo);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_UPDATE_EVENT, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppJoinedPartyEvent(IPMI::Client *client)
{
	return partyAppCall(client, PARTY_APP_METHOD_JOINED_PARTY_EVENT);
}

int partyAppLeftPartyEvent(IPMI::Client *client, const SceNpPartyRoomLeftReason *reason,
			   SceUInt8 appStopped)
{
	int result = 0;
	SceUInt8 stopped = appStopped;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &stopped;
	in[0].dataSize = sizeof(stopped);
	in[1].pData = reason;
	in[1].dataSize = sizeof(*reason);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_LEFT_PARTY_EVENT, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberJoinedEvent(IPMI::Client *client, SceUInt32 index,
			      const SceNpPartyAppMemberInfo *memberInfo)
{
	int result = 0;
	SceUInt32 i = index;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &i;
	in[0].dataSize = sizeof(i);
	in[1].pData = memberInfo;
	in[1].dataSize = sizeof(*memberInfo);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_JOINED_EVENT, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberLeftEvent(IPMI::Client *client, SceUInt32 index,
			    const SceNpPartyAppMemberInfo *memberInfo)
{
	int result = 0;
	SceUInt32 i = index;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &i;
	in[0].dataSize = sizeof(i);
	in[1].pData = memberInfo;
	in[1].dataSize = sizeof(*memberInfo);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_LEFT_EVENT, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberGamePresenceChangedEvent(IPMI::Client *client,
					   const SceNpPartyAppMemberInfo *memberInfo)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = memberInfo;
	in[0].dataSize = sizeof(*memberInfo);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_GAME_PRESENCE_CHANGED_EVENT,
				       in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberVoiceStateChangedEvent(IPMI::Client *client,
					 const SceNpPartyMemberVoiceInfo *voiceInfo)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = voiceInfo;
	in[0].dataSize = sizeof(*voiceInfo);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_VOICE_STATE_CHANGED_EVENT,
				       in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberSessInvCommIdUpdateEvent(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
					   const SceNpCommunicationId *gameSessInvCommId)
{
	int result = 0;
	SceNpPartyRoomMemberId id = memberId;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = &id;
	in[0].dataSize = sizeof(id);
	in[1].pData = gameSessInvCommId;
	in[1].dataSize = sizeof(*gameSessInvCommId);
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_SESS_INV_COMM_ID_UPDATE_EVENT,
				       in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppMemberGameSessionUpdateEvent(IPMI::Client *client,
					 SceNpPartyAppMemberGameSession session)
{
	int result = 0;
	IPMI::DataInfo in[1];
	SceNpPartyAppMemberGameSession data;
	int ret;

	in[0].pData = &data;
	in[0].dataSize = sizeof(data);
	data = session;
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_MEMBER_GAME_SESSION_UPDATE_EVENT,
				       in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyAppGameSessionUpdateReadyEvent(IPMI::Client *client,
					const SceNpPartyGameSessionUpdateReady *ready,
					const void *readyInfo)
{
	int result = 0;
	IPMI::DataInfo in[2];
	int ret;

	in[0].pData = ready;
	in[0].dataSize = sizeof(*ready);
	in[1].pData = readyInfo;
	in[1].dataSize = SCE_NP_PARTY_APP_SESSION_READY_INFO_SIZE;
	ret = client->invokeSyncMethod(PARTY_APP_METHOD_GAME_SESSION_UPDATE_READY_EVENT,
				       in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
