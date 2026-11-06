/*
 * The shell's party methods over the client's IPMI connection (party_game_ipc_client.cpp; the
 * file and function names are ours). Each is a synchronous call that returns the call's error
 * or the method's result.
 */
#ifndef PARTY_GAME_CLIENT_PARTY_GAME_IPC_CLIENT_H
#define PARTY_GAME_CLIENT_PARTY_GAME_IPC_CLIENT_H

#include <scetypes.h>
#include <np/np_party.h>
#include "../../../../../../../lowlayer/src/ipmi/client.h"

int partyGameInit(IPMI::Client *client, const SceNpPartyInitParam *param);
int partyGameRegisterHandler(IPMI::Client *client, const SceNpPartyEventHandlers *handlers);
int partyGameTerm(IPMI::Client *client);
int partyGameGetState(IPMI::Client *client, SceNpPartyState *state);
int partyGameGetMembers(IPMI::Client *client, SceNpPartyMemberList *memberList);
int partyGameGetMemberInfo(IPMI::Client *client, const SceNpId *npId,
			   SceNpPartyMemberInfo *memberInfo);
int partyGameGetMemberVoiceInfo(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
				SceNpPartyMemberVoiceState *memberVoiceState);
int partyGameGetMemberSessionInfo(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
				  SceNpPartyMemberGameSessionUpdateInfo *memberGameSessionInfo);
int partyGameGetId(IPMI::Client *client, SceNpPartyId *id);
int partyGameGetGameSessionReadyState(IPMI::Client *client,
				      SceNpPartyGameSessionUpdateReady *readyState);
int partyGameRequest110018(IPMI::Client *client);
int partyGameRequest110019(IPMI::Client *client);

#endif
