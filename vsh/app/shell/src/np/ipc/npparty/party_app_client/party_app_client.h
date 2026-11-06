/*
 * SceNpPartyAppUtil (vs0:app/NPXS10001/np_party_app.suprx): the party app's side of the shell's
 * "SceNpPartyUtil" IPMI service. The party app reports the party's events to the shell, which
 * passes them on to the game's party library (np_party), and gets the game's requests back as
 * the service's messages, polled by sceNpPartyAppUtilityCheckCallback.
 *
 * Export names are Sony's where the party app's messages name the call they report
 * (vsh/app/party/PartySdkManager/src/PartySdkManager.cpp, in the 3.6x debug builds), the
 * SDK event names are the party app's strings, and SceNpPartyAppGameRegisteredHandlers and its
 * fields are the party app's format string; the other names are ours.
 */
#ifndef PARTY_APP_CLIENT_H
#define PARTY_APP_CLIENT_H

#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_party.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the library's errors (facility 0x0055, 0x805592xx; names ours, after SCE_NP_PARTY_ERROR_*) */
#define SCE_NP_PARTY_APP_UTILITY_ERROR_UNKNOWN			-2141875711	/* 0x80559201 */
#define SCE_NP_PARTY_APP_UTILITY_ERROR_ALREADY_INITIALIZED	-2141875710	/* 0x80559202 */
#define SCE_NP_PARTY_APP_UTILITY_ERROR_NOT_INITIALIZED		-2141875709	/* 0x80559203 */
#define SCE_NP_PARTY_APP_UTILITY_ERROR_OUT_OF_MEMORY		-2141875707	/* 0x80559205 */

/* the game's requests, as the party app's event handler gets them */
#define SCE_NP_PARTY_SDK_EVENT_GAME_INIT_PARAMS		1
#define SCE_NP_PARTY_SDK_EVENT_GAME_REGISTERED_HANDLERS	2
#define SCE_NP_PARTY_SDK_EVENT_GAME_SESSION_TERM	4
#define SCE_NP_PARTY_SDK_EVENT_GAME_DISABLE_VOICE	5
#define SCE_NP_PARTY_SDK_EVENT_GAME_RE_ENABLE_VOICE	6

/* SCE_NP_PARTY_SDK_EVENT_GAME_INIT_PARAMS: the game's sceNpPartyInit */
typedef struct SceNpPartyAppGameInitParam {
	SceUInt32 sdkVersion;
	SceNpCommunicationId commId;
	SceUInt32 gameAppId;
} SceNpPartyAppGameInitParam;

/* SCE_NP_PARTY_SDK_EVENT_GAME_REGISTERED_HANDLERS: the game's sceNpPartyRegisterHandler */
typedef struct SceNpPartyAppGameRegisteredHandlers {
	SceUInt8 roomEvents;
	SceUInt8 voiceEvents;
	SceUInt8 gameSessionEvents;
	SceUInt8 gameSessionSystemReady;
	SceUInt8 reserved[8];
	SceNpCommunicationId gameCommId;
} SceNpPartyAppGameRegisteredHandlers;

/*
 * What the party app's handler gets: one of the events above, a word the library always
 * passes as 0, the event's data (or NULL) and the handler's argument.
 */
typedef void (*SceNpPartyAppEventHandler)(int event, int reserved, const void *data,
					  void *userdata);

/* a party member as the party app reports it: the game's member info and the app's own */
typedef struct SceNpPartyAppMemberInfo {
	SceNpPartyMemberInfo memberInfo;
	SceUInt8 reserved[16];
} SceNpPartyAppMemberInfo;

_Static_assert(sizeof(SceNpPartyAppMemberInfo) == 532, "SceNpPartyAppMemberInfo size");

/* a member's game session as sceNpPartyAppUtil_A540F067 passes it, by value */
typedef struct SceNpPartyAppMemberGameSession {
	SceNpPartyRoomMemberId memberId;
	SceUInt8 session[52];
	SceUInt8 sessionData[128];
} SceNpPartyAppMemberGameSession;

_Static_assert(sizeof(SceNpPartyAppMemberGameSession) == 182,
	       "SceNpPartyAppMemberGameSession size");

/* the party the app is joining, as the shell gets it with the members */
#define SCE_NP_PARTY_APP_ROOM_INFO_SIZE		184
#define SCE_NP_PARTY_APP_ROOM_STATE_SIZE	4
#define SCE_NP_PARTY_APP_ROOM_SESSION_SIZE	48
#define SCE_NP_PARTY_APP_SESSION_READY_INFO_SIZE 12

int SceNpPartyAppUtil_E64BB92C(SceNpPartyAppEventHandler handler, void *userdata);
int sceNpPartyAppUtilityCheckCallback(void);
int sceNpPartyAppUtilityInit(void);
int sceNpPartyAppUtilityTerm(void);
int sceNpPartyAppUtilityJoiningPartyEvent(SceUInt8 memberNum,
					  const SceNpPartyAppMemberInfo *selfInfo,
					  const SceNpPartyAppMemberInfo *leaderInfo,
					  const void *roomInfo, const void *roomState,
					  const void *roomSession);
int sceNpPartyAppUtilityMemberUpdateEvent(SceUInt32 index,
					  const SceNpPartyAppMemberInfo *memberInfo);
int sceNpPartyAppUtilityJoinedPartyEvent(void);
int SceNpPartyAppUtil_74FA54BF(const SceNpPartyRoomLeftReason *reason, SceUInt8 appStopped);
int sceNpPartyAppUtilityMemberJoinedEvent(SceUInt32 index,
					  const SceNpPartyAppMemberInfo *memberInfo);
int sceNpPartyAppUtilityMemberLeftEvent(SceUInt32 index,
					const SceNpPartyAppMemberInfo *memberInfo);
int sceNpPartyAppUtilityMemberGamePresenceChangedEvent(const SceNpPartyAppMemberInfo *memberInfo);
int sceNpPartyAppUtilityMemberVoiceStateChangedEvent(const SceNpPartyMemberVoiceInfo *voiceInfo);
int sceNpPartyAppUtilityMemberSessInvCommIdUpdateEvent(SceNpPartyRoomMemberId memberId,
						       const SceNpCommunicationId *gameSessInvCommId);
int SceNpPartyAppUtil_A540F067(SceNpPartyAppMemberGameSession session);
int sceNpPartyAppUtilityGameSessionUpdateReadyEvent(const SceNpPartyGameSessionUpdateReady *ready,
						    const void *readyInfo);

#ifdef __cplusplus
}
#endif

#endif
