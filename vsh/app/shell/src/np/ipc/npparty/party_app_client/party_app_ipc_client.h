/*
 * The shell's party app methods over the client's IPMI connection (party_app_ipc_client.cpp;
 * the file and function names are ours). Each is a synchronous call that returns the call's
 * error or the method's result.
 */
#ifndef PARTY_APP_CLIENT_PARTY_APP_IPC_CLIENT_H
#define PARTY_APP_CLIENT_PARTY_APP_IPC_CLIENT_H

#include <scetypes.h>
#include "party_app_client.h"
#include "../../../../../../../lowlayer/src/ipmi/client.h"

int partyAppInit(IPMI::Client *client);
int partyAppRegisterHandler(IPMI::Client *client);
int partyAppTerm(IPMI::Client *client);
int partyAppJoiningPartyEvent(IPMI::Client *client, SceUInt8 memberNum,
			      const SceNpPartyAppMemberInfo *selfInfo,
			      const SceNpPartyAppMemberInfo *leaderInfo, const void *roomInfo,
			      const void *roomState, const void *roomSession);
int partyAppMemberUpdateEvent(IPMI::Client *client, SceUInt32 index,
			      const SceNpPartyAppMemberInfo *memberInfo);
int partyAppJoinedPartyEvent(IPMI::Client *client);
int partyAppLeftPartyEvent(IPMI::Client *client, const SceNpPartyRoomLeftReason *reason,
			   SceUInt8 appStopped);
int partyAppMemberJoinedEvent(IPMI::Client *client, SceUInt32 index,
			      const SceNpPartyAppMemberInfo *memberInfo);
int partyAppMemberLeftEvent(IPMI::Client *client, SceUInt32 index,
			    const SceNpPartyAppMemberInfo *memberInfo);
int partyAppMemberGamePresenceChangedEvent(IPMI::Client *client,
					   const SceNpPartyAppMemberInfo *memberInfo);
int partyAppMemberVoiceStateChangedEvent(IPMI::Client *client,
					 const SceNpPartyMemberVoiceInfo *voiceInfo);
int partyAppMemberSessInvCommIdUpdateEvent(IPMI::Client *client, SceNpPartyRoomMemberId memberId,
					   const SceNpCommunicationId *gameSessInvCommId);
int partyAppMemberGameSessionUpdateEvent(IPMI::Client *client,
					 SceNpPartyAppMemberGameSession session);
int partyAppGameSessionUpdateReadyEvent(IPMI::Client *client,
					const SceNpPartyGameSessionUpdateReady *ready,
					const void *readyInfo);

#endif
