/*
 * The shell's party methods over the client's IPMI connection. Sony's build keeps them after
 * party_game_system.cpp, not inlined; the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "party_game_ipc_client.h"

#define PARTY_GAME_METHOD_INIT				0x11000E
#define PARTY_GAME_METHOD_REGISTER_HANDLER		0x11000F
#define PARTY_GAME_METHOD_TERM				0x110010
#define PARTY_GAME_METHOD_GET_STATE			0x110011
#define PARTY_GAME_METHOD_GET_MEMBERS			0x110012
#define PARTY_GAME_METHOD_GET_MEMBER_INFO		0x110013
#define PARTY_GAME_METHOD_GET_MEMBER_VOICE_INFO		0x110014
#define PARTY_GAME_METHOD_GET_MEMBER_SESSION_INFO	0x110015
#define PARTY_GAME_METHOD_GET_ID			0x110016
#define PARTY_GAME_METHOD_GET_GAME_SESSION_READY_STATE	0x110017
#define PARTY_GAME_METHOD_110018			0x110018
#define PARTY_GAME_METHOD_110019			0x110019

int partyGameInit(IPMI::Client *client, const SceNpPartyInitParam *param)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = param;
	in[0].dataSize = sizeof(*param);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_INIT, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameRegisterHandler(IPMI::Client *client, const SceNpPartyEventHandlers *handlers)
{
	int result = 0;
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = handlers;
	in[0].dataSize = sizeof(*handlers);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_REGISTER_HANDLER, in, 1, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameTerm(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_TERM,
				       (const IPMI::DataInfo *)NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetState(IPMI::Client *client, SceNpPartyState *state)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = state;
	out[0].bufferSize = sizeof(*state);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_STATE,
				       (const IPMI::DataInfo *)NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetMembers(IPMI::Client *client, SceNpPartyMemberList *memberList)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = memberList;
	out[0].bufferSize = sizeof(*memberList);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_MEMBERS,
				       (const IPMI::DataInfo *)NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetMemberInfo(IPMI::Client *client, const SceNpId *npId,
			   SceNpPartyMemberInfo *memberInfo)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = npId;
	in[0].dataSize = sizeof(*npId);
	out[0].pBuffer = memberInfo;
	out[0].bufferSize = sizeof(*memberInfo);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_MEMBER_INFO, in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetMemberVoiceInfo(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
				SceNpPartyMemberVoiceState *memberVoiceState)
{
	int result = 0;
	SceNpPartyRoomMemberId id = memberId;
	IPMI::BufferInfo out[1];
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &id;
	in[0].dataSize = sizeof(id);
	out[0].pBuffer = memberVoiceState;
	out[0].bufferSize = sizeof(*memberVoiceState);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_MEMBER_VOICE_INFO,
				       in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetMemberSessionInfo(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
				  SceNpPartyMemberGameSessionUpdateInfo *memberGameSessionInfo)
{
	int result = 0;
	SceNpPartyRoomMemberId id = memberId;
	IPMI::BufferInfo out[1];
	IPMI::DataInfo in[1];
	int ret;

	in[0].pData = &id;
	in[0].dataSize = sizeof(id);
	out[0].pBuffer = memberGameSessionInfo;
	out[0].bufferSize = sizeof(*memberGameSessionInfo);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_MEMBER_SESSION_INFO,
				       in, 1, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetId(IPMI::Client *client, SceNpPartyId *id)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = id;
	out[0].bufferSize = sizeof(*id);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_ID,
				       (const IPMI::DataInfo *)NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameGetGameSessionReadyState(IPMI::Client *client,
				      SceNpPartyGameSessionUpdateReady *readyState)
{
	int result = 0;
	IPMI::BufferInfo out[1];
	int ret;

	out[0].pBuffer = readyState;
	out[0].bufferSize = sizeof(*readyState);
	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_GET_GAME_SESSION_READY_STATE,
				       (const IPMI::DataInfo *)NULL, 0, &result, out, 1);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameRequest110018(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_110018,
				       (const IPMI::DataInfo *)NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int partyGameRequest110019(IPMI::Client *client)
{
	int result = 0;
	int ret;

	ret = client->invokeSyncMethod(PARTY_GAME_METHOD_110019,
				       (const IPMI::DataInfo *)NULL, 0, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
