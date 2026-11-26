/*
 * np_party_vsh_internal.h: internal types, constants, and cross-unit declarations
 * for SceShell's NP Party VSH service and "SceNpPartyUtil" IPMI server.
 * 3.65-CEX: 0x81384E12 - 0x8138870F, 0x813C9BA4 - 0x813CB46F
 */
#ifndef SHELL_NP_PARTY_VSH_INTERNAL_H
#define SHELL_NP_PARTY_VSH_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <np/np_common.h>
#include <np/np_basic.h>
#include <np/np_party.h>

#define NP_PARTY_VSH_ERROR_INTERNAL			(-2141875967)	/* 0x80559101 */
#define NP_PARTY_VSH_ERROR_ALREADY_INITIALIZED		(-2141875966)	/* 0x80559102 */
#define NP_PARTY_VSH_ERROR_NOT_FOUND			(-2141875964)	/* 0x80559104 */
#define NP_PARTY_VSH_ERROR_INVALID_ARGUMENT		(-2141875963)	/* 0x80559105 */
#define NP_PARTY_VSH_ERROR_GAME_NOT_INITIALIZED		(-2141875962)	/* 0x80559106 */

#define NP_PARTY_APP_ERROR_ACCESS_DENIED		(-2141875711)	/* 0x80559201 */
#define NP_PARTY_APP_ERROR_NOT_INITIALIZED		(-2141875709)	/* 0x80559203 */
#define NP_PARTY_APP_ERROR_INVALID_ARGUMENT		(-2141875708)	/* 0x80559204 */
#define NP_PARTY_APP_ERROR_OUT_OF_MEMORY		(-2141875707)	/* 0x80559205 */

#define NP_PARTY_GAME_ERROR_ACCESS_DENIED		(-2141903615)	/* 0x80552501 */
#define NP_PARTY_GAME_ERROR_ALREADY_INITIALIZED		(-2141903614)	/* 0x80552502 */
#define NP_PARTY_GAME_ERROR_NOT_INITIALIZED		(-2141903613)	/* 0x80552503 */
#define NP_PARTY_GAME_ERROR_NOT_IN_PARTY		(-2141903610)	/* 0x80552506 */
#define NP_PARTY_GAME_ERROR_MEMBER_NOT_FOUND		(-2141903608)	/* 0x80552508 */
#define NP_PARTY_GAME_ERROR_SESSION_NOT_READY		(-2141903607)	/* 0x80552509 */
#define NP_PARTY_GAME_ERROR_READY_STATE_UNAVAILABLE	(-2141903600)	/* 0x80552510 */

#define NP_PARTY_OBSERVER_ERROR_NOT_REGISTERED		(-2141874431)	/* 0x80559701 */
#define NP_PARTY_OBSERVER_ERROR_ALREADY_REGISTERED	(-2141874430)	/* 0x80559702 */
#define NP_PARTY_OBSERVER_ERROR_NOT_FOUND		(-2141874429)	/* 0x80559703 */
#define NP_PARTY_OBSERVER_ERROR_NOT_IN_PARTY		(-2141874427)	/* 0x80559705 */
#define NP_PARTY_OBSERVER_ERROR_MAX_CLIENTS		(-2141874426)	/* 0x80559706 */

#define NP_PARTY_IPMI_ERROR_UNSUPPORTED_METHOD		(-2147351103)	/* 0x800205C1 */
#define NP_PARTY_IPMI_ERROR_QUEUE_FULL			(-2147351295)	/* 0x80020501 */

#define NP_PARTY_ROOM_INFO_SIZE				184
#define NP_PARTY_ROOM_SESSION_ID_SIZE			48
#define NP_PARTY_ROOM_PRIVATE_BIT			0x80000000u
#define NP_PARTY_OBSERVER_CLIENT_MAX			2
#define NP_PARTY_OBSERVER_MEMBER_MAX			7

/* IPMI methods on "SceNpPartyUtil" (0x110000..0x11001C) */
enum NpPartyIpcMethod {
	NP_PARTY_IPC_APP_INIT				= 0x110000,
	NP_PARTY_IPC_APP_REGISTER_HANDLER		= 0x110001,
	NP_PARTY_IPC_APP_TERM				= 0x110002,
	NP_PARTY_IPC_APP_JOINING_PARTY			= 0x110003,
	NP_PARTY_IPC_APP_MEMBER_UPDATE			= 0x110004,
	NP_PARTY_IPC_APP_JOINED_PARTY			= 0x110005,
	NP_PARTY_IPC_APP_LEFT_PARTY			= 0x110006,
	NP_PARTY_IPC_APP_MEMBER_JOINED			= 0x110007,
	NP_PARTY_IPC_APP_MEMBER_LEFT			= 0x110008,
	NP_PARTY_IPC_APP_MEMBER_PRESENCE_CHANGED	= 0x110009,
	NP_PARTY_IPC_APP_MEMBER_VOICE_CHANGED		= 0x11000A,
	NP_PARTY_IPC_APP_MEMBER_SESS_COMM_ID_UPDATE	= 0x11000B,
	NP_PARTY_IPC_APP_MEMBER_GAME_SESSION_UPDATE	= 0x11000C,
	NP_PARTY_IPC_APP_GAME_SESSION_UPDATE_READY	= 0x11000D,

	NP_PARTY_IPC_GAME_INIT				= 0x11000E,
	NP_PARTY_IPC_GAME_REGISTER_HANDLER		= 0x11000F,
	NP_PARTY_IPC_GAME_TERM				= 0x110010,
	NP_PARTY_IPC_GAME_GET_STATE			= 0x110011,
	NP_PARTY_IPC_GAME_GET_MEMBERS			= 0x110012,
	NP_PARTY_IPC_GAME_GET_MEMBER_INFO		= 0x110013,
	NP_PARTY_IPC_GAME_GET_MEMBER_VOICE_INFO		= 0x110014,
	NP_PARTY_IPC_GAME_GET_MEMBER_SESSION_INFO	= 0x110015,
	NP_PARTY_IPC_GAME_GET_ID			= 0x110016,
	NP_PARTY_IPC_GAME_GET_SESSION_READY_STATE	= 0x110017,
	NP_PARTY_IPC_GAME_DISABLE_VOICE			= 0x110018,
	NP_PARTY_IPC_GAME_RE_ENABLE_VOICE		= 0x110019,

	NP_PARTY_IPC_OBSERVER_REGISTER			= 0x11001A,
	NP_PARTY_IPC_OBSERVER_GET_MEMBERS		= 0x11001B,
	NP_PARTY_IPC_OBSERVER_UNREGISTER		= 0x11001C
};

/* Messages pushed to the party app's IPMI message queue */
enum NpPartyAppQueueEvent {
	NP_PARTY_APP_QUEUE_GAME_INIT			= 0,
	NP_PARTY_APP_QUEUE_GAME_HANDLERS		= 1,
	NP_PARTY_APP_QUEUE_GAME_TERM			= 2,
	NP_PARTY_APP_QUEUE_DISABLE_VOICE		= 3,
	NP_PARTY_APP_QUEUE_RE_ENABLE_VOICE		= 4
};

/* Messages pushed to the game client's IPMI message queue */
enum NpPartyGameQueueEvent {
	NP_PARTY_GAME_QUEUE_JOINED			= 0,
	NP_PARTY_GAME_QUEUE_MEMBER_JOINED		= 1,
	NP_PARTY_GAME_QUEUE_MEMBER_LEFT			= 2,
	NP_PARTY_GAME_QUEUE_LEFT			= 3,
	NP_PARTY_GAME_QUEUE_MEMBER_VOICE		= 4,
	NP_PARTY_GAME_QUEUE_MEMBER_PRESENCE		= 5,
	NP_PARTY_GAME_QUEUE_MEMBER_SESSION		= 6,
	NP_PARTY_GAME_QUEUE_SESSION_READY		= 7
};

/* Party member entry sent by SceNpPartyApp (532 bytes) */
struct NpPartyAppMemberRecord {
	SceNpPartyMemberInfo memberInfo;
	SceNpCommunicationId legacyCommId;
	SceUInt8 tailAlignment[4];
};

/* Game-session update payload sent by SceNpPartyApp (182 bytes) */
struct NpPartyAppGameSessionPayload {
	SceNpPartyMemberGameSessionUpdateInfo sessionInfo;
	SceUInt8 gamePresenceData[128];
};

/* Registered game handler flags forwarded to SceNpPartyApp (24 bytes) */
struct NpPartyGameHandlerFlags {
	SceUInt8 roomEvents;
	SceUInt8 voiceEvents;
	SceUInt8 gameSessionEvents;
	SceUInt8 gameSessionSystemReady;
	SceUInt8 gameSessionReadyValid;
	SceUInt8 reservedHeader[3];
	SceUInt32 gameSessionReadyResult;
	SceNpCommunicationId gameCommId;
};

/* Complete room snapshot buffered between joiningPartyEvent and joinedPartyEvent (5528 bytes) */
struct NpPartyAppRoomSnapshot {
	NpPartyAppMemberRecord members[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt8 memberAlignment[8];
	NpPartyAppMemberRecord selfInfo;
	NpPartyAppMemberRecord leaderInfo;
	SceUInt32 memberNum;
	SceUInt32 countAlignment;
	SceUInt8 roomInfo[NP_PARTY_ROOM_INFO_SIZE];
	SceUInt32 roomState;
	SceUInt32 stateAlignment;
};

/* Per-member game session tracking slot in NpPartyVshContext (64 bytes) */
struct NpPartyMemberSessionSlot {
	SceNpPartyRoomMemberId memberId;
	SceNpCommunicationId commId;
	SceNpPartyGameSessionMemberUpdateType updateType;
	SceNpSessionId sessionId;
	SceUInt8 tailAlignment;
};

/* Observer-facing compact member entry (168 bytes) */
struct NpPartyObserverMember {
	SceNpId npId;
	SceUInt8 gamePresenceData[128];
	SceUInt8 presenceKind;
	SceUInt8 tailAlignment[3];
};

/* Observer-facing member list returned by NP_PARTY_IPC_OBSERVER_GET_MEMBERS (1180 bytes) */
struct NpPartyObserverMemberList {
	SceUInt8 memberCount;
	SceUInt8 headerAlignment[3];
	NpPartyObserverMember members[NP_PARTY_OBSERVER_MEMBER_MAX];
};

/* Game client lifecycle state */
enum NpPartyGameClientState {
	NP_PARTY_GAME_STATE_UNINITIALIZED		= 1,
	NP_PARTY_GAME_STATE_INITIALIZED			= 2,
	NP_PARTY_GAME_STATE_HANDLERS_REGISTERED		= 3
};

/* Voice-disable state machine driven by game app requests */
enum NpPartyVoiceSuspendState {
	NP_PARTY_VOICE_ACTIVE				= 0,
	NP_PARTY_VOICE_SUSPEND_PENDING			= 1,
	NP_PARTY_VOICE_SUSPENDED			= 2,
	NP_PARTY_VOICE_RESUME_PENDING			= 3,
	NP_PARTY_VOICE_RESUMED				= 4
};

/* VSH service API (np_party_vsh_service.cpp) */
int npPartyVshInit(void);								/* 0x81385C1E */
int npPartyVshGetObserverMembers(NpPartyObserverMemberList *outList);			/* 0x81385D2A */
int npPartyVshOnGameInit(const SceNpPartyInitParam *initParam, SceUID gameAppId);	/* 0x81385E00 */
int npPartyVshOnGameRegisterHandlers(const NpPartyGameHandlerFlags *handlers);		/* 0x81385E54 */
int npPartyVshGetMembers(SceNpPartyMemberList *outMembers);				/* 0x81385FD2 */
int npPartyVshGetMemberInfo(const SceNpId *npId, SceNpPartyMemberInfo *outInfo);	/* 0x81386154 */
int npPartyVshGetMemberSessionInfo(SceNpPartyRoomMemberId memberId,
				   SceNpPartyMemberGameSessionUpdateInfo *outInfo);	/* 0x813861E0 */
int npPartyVshGetPartyId(SceNpPartyId *outId);						/* 0x813862E6 */
int npPartyVshGetGameSessionReadyState(SceNpPartyGameSessionUpdateReady *outReady);	/* 0x81386334 */
int npPartyVshGetMemberVoiceInfo(SceNpPartyRoomMemberId memberId,
				 SceNpPartyMemberVoiceState *outVoiceState);		/* 0x8138639C */
int npPartyVshReEnableVoice(void);							/* 0x8138668A */
int npPartyVshOnGameTerm(void);								/* 0x81386708 */
int npPartyVshGetState(SceNpPartyState *outState);					/* 0x81386752 */
int npPartyVshDisableVoice(void);							/* 0x8138677A */
int npPartyVshOnAppInit(void);								/* 0x813867F0 */
int npPartyVshOnAppRegisterHandler(void);						/* 0x81386814 */
int npPartyVshOnAppJoinedParty(const NpPartyAppRoomSnapshot *snapshot,
			       const char *roomSessionId);				/* 0x81386870 */
int npPartyVshOnAppMemberLeft(const NpPartyAppRoomSnapshot *snapshot,
			      const NpPartyAppMemberRecord *member);			/* 0x81386A2A */
int npPartyVshOnAppMemberJoined(const NpPartyAppRoomSnapshot *snapshot,
				const NpPartyAppMemberRecord *member);			/* 0x81386B5C */
int npPartyVshOnAppMemberPresenceChanged(const NpPartyAppRoomSnapshot *snapshot,
					 const NpPartyAppMemberRecord *member);		/* 0x81386C04 */
int npPartyVshOnAppMemberVoiceChanged(const SceNpPartyMemberVoiceInfo *voiceInfo);	/* 0x81386CDA */
int npPartyVshOnAppMemberSessCommIdUpdate(SceNpPartyRoomMemberId memberId,
					  const SceNpCommunicationId *commId);		/* 0x81386DA4 */
int npPartyVshOnAppMemberGameSessionUpdate(const NpPartyAppGameSessionPayload *payload); /* 0x81386EF8 */
int npPartyVshOnAppGameSessionUpdateReady(const SceNpPartyGameSessionUpdateReady *ready,
					  const SceNpCommunicationId *gameCommId);	/* 0x81387090 */
int npPartyVshOnAppLeftParty(const SceNpPartyRoomLeftReason *reason,
			     SceUInt8 appStopped);					/* 0x81387222 */

/* IPC server API (np_party_ipc_server.cpp) */
int npPartyIpcServerInit(void);								/* 0x813C9BA4 */
int npPartyIpcNotifyObserversChanged(void);						/* 0x813CA890 */
int npPartyIpcSendAppGameInit(const SceNpPartyInitParam *initParam, SceUID gameAppId);	/* 0x813CA908 */
int npPartyIpcSendAppGameHandlers(const NpPartyGameHandlerFlags *handlers);		/* 0x813CA96C */
int npPartyIpcSendAppGameTerm(void);							/* 0x813CA9CC */
int npPartyIpcSendAppDisableVoice(void);						/* 0x813CAA22 */
int npPartyIpcSendAppReEnableVoice(void);						/* 0x813CAA78 */
int npPartyIpcSendGameJoined(const SceNpPartyMemberList *memberList);			/* 0x813CAACE */
int npPartyIpcSendGameMemberJoined(const SceNpPartyMemberInfo *memberInfo);		/* 0x813CAB60 */
int npPartyIpcSendGameMemberLeft(const SceNpPartyMemberInfo *memberInfo);		/* 0x813CABF6 */
int npPartyIpcSendGameMemberVoice(const SceNpPartyMemberVoiceInfo *voiceInfo);		/* 0x813CAC8C */
int npPartyIpcSendGameMemberPresence(const SceNpPartyMemberInfo *memberInfo);		/* 0x813CAD18 */
int npPartyIpcSendGameMemberSession(const SceNpPartyMemberGameSessionUpdateInfo *info);	/* 0x813CADAE */
int npPartyIpcSendGameSessionReady(const SceNpPartyGameSessionUpdateReady *ready,
				   const SceNpPartyMemberGameSessionUpdateInfo *members,
				   SceUInt8 memberCount);				/* 0x813CAE5C */
int npPartyIpcSendGameLeft(const SceNpPartyRoomLeftReason *reason);			/* 0x813CAEF6 */

#endif /* SHELL_NP_PARTY_VSH_INTERNAL_H */
