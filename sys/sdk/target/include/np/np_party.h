/*
 * np/np_party.h: the NP Party library (SceNpPartyGameUtil), under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_party.h). Its error codes are in
 * np/np_error.h.
 */
#ifndef _SCE_NP_PARTY_H_
#define _SCE_NP_PARTY_H_

#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_basic.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef SceUInt16 SceNpPartyRoomEventType;
#define SCE_NP_PARTY_ROOM_EVENT_JOINED			(0x0001)
#define SCE_NP_PARTY_ROOM_EVENT_MEMBER_JOINED		(0x0002)
#define SCE_NP_PARTY_ROOM_EVENT_MEMBER_LEFT		(0x0003)
#define SCE_NP_PARTY_ROOM_EVENT_LEFT			(0x0004)
#define SCE_NP_PARTY_ROOM_EVENT_MEMBER_PRESENCE_UPDATE	(0x0005)

typedef SceUChar8 SceNpPartyMemberVoiceState;
#define SCE_NP_PARTY_MEMBER_VOICE_STATE_UNKNOWN		(0x0000)
#define SCE_NP_PARTY_MEMBER_VOICE_STATE_DISCONNECTED	(0x0001)
#define SCE_NP_PARTY_MEMBER_VOICE_STATE_CONNECTED	(0x0002)
#define SCE_NP_PARTY_MEMBER_VOICE_STATE_TALKING		(0x0003)
#define SCE_NP_PARTY_MEMBER_VOICE_STATE_MUTED		(0x0004)

#define SCE_NP_PARTY_ROOM_TALKING_LIMIT			1000000

typedef SceUInt16 SceNpPartyState;
#define SCE_NP_PARTY_STATE_IN_PARTY			(0x0001)
#define SCE_NP_PARTY_STATE_NOT_IN_PARTY			(0x0002)
#define SCE_NP_PARTY_STATE_IN_PRIVATE_PARTY		(0x0003)

typedef SceUInt16 SceNpPartyRoomMemberId;

#define SCE_NP_PARTY_MEMBER_NUM_MAX			8

typedef SceUInt8 SceNpPartyMemberFlags;
#define SCE_NP_PARTY_MEMBER_FLAG_IS_ME			(0x40)
#define SCE_NP_PARTY_MEMBER_FLAG_IS_PARTY_LEADER	(0x80)

typedef SceUInt32 SceNpPartyRoomLeftReason;
#define SCE_NP_PARTY_ROOM_LEFT_REASON_USER_ACTION	0x00000001
#define SCE_NP_PARTY_ROOM_LEFT_REASON_SERVER_ERROR	0x00000002
#define SCE_NP_PARTY_ROOM_LEFT_REASON_KICKED_OUT	0x00000003

typedef struct SceNpPartyMemberInfo {
	SceNpId npId;
	SceNpPartyRoomMemberId memberId;
	SceNpPartyMemberFlags memberFlags;
	SceUChar8 reserved;
	SceNpBasicGamePresence gamePresence;	/* deprecated */
	SceNpCommunicationId commId;		/* deprecated */
} SceNpPartyMemberInfo;

typedef struct SceNpPartyMemberList {
	SceNpId members[SCE_NP_PARTY_MEMBER_NUM_MAX];
	SceUInt8 memberNum;
	bool privateParty;
	SceUChar8 padding[2];
} SceNpPartyMemberList;

typedef struct SceNpPartyMemberVoiceInfo {
	SceNpPartyRoomMemberId memberId;
	SceNpPartyMemberVoiceState memberVoiceState;
	SceUChar8 padding;
} SceNpPartyMemberVoiceInfo;

typedef void (*SceNpPartyRoomEventHandler)(SceNpPartyRoomEventType eventType, const void *data,
					   void *userdata);
typedef void (*SceNpPartyVoiceEventHandler)(const SceNpPartyMemberVoiceInfo *memberVoiceInfo,
					    void *userdata);

typedef struct SceNpPartyInitParam {
	SceUInt32 sdkVersion;
	SceNpCommunicationId commId;		/* deprecated */
} SceNpPartyInitParam;

typedef SceUInt8 SceNpPartyGameSessionEvent;
#define SCE_NP_PARTY_GAME_SESSION_UPDATE_EVENT_READY	(0x01)
#define SCE_NP_PARTY_GAME_SESSION_MEMBER_UPDATE_EVENT	(0x02)

typedef struct SceNpPartyGameSessionUpdateReady {
	SceUInt32 result;
} SceNpPartyGameSessionUpdateReady;

typedef SceUInt8 SceNpPartyGameSessionMemberUpdateType;
#define SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_NOT_JOINABLE	(0x01)
#define SCE_NP_PARTY_GAME_SESSION_UPDATE_TYPE_JOINABLE		(0x02)

typedef struct SceNpPartyMemberGameSessionUpdateInfo {
	SceNpPartyGameSessionMemberUpdateType type;
	SceUInt8 reserved;
	SceNpPartyRoomMemberId memberId;
	SceUInt8 reserved2[2];
	SceNpSessionId sessionId;
} SceNpPartyMemberGameSessionUpdateInfo;

typedef void (*SceNpPartyGameSessionEventHandler)(SceNpPartyGameSessionEvent event,
						  const void *data, void *userdata);

typedef struct SceNpPartyEventHandlers {
	SceUInt32 sdkVersion;
	SceNpPartyRoomEventHandler roomEventHandler;
	SceNpPartyVoiceEventHandler voiceEventHandler;
	SceNpPartyGameSessionEventHandler gameSessionEventHandler;
} SceNpPartyEventHandlers;

#define SCE_NP_PARTY_ID_LENGTH				16

typedef struct SceNpPartyId {
	SceUChar8 id[SCE_NP_PARTY_ID_LENGTH];
} SceNpPartyId;

int sceNpPartyInit(const SceNpPartyInitParam *param);
int sceNpPartyTerm(void);
int sceNpPartyGetMembers(SceNpPartyMemberList *memberList);
int sceNpPartyGetMemberInfo(const SceNpId *npId, SceNpPartyMemberInfo *memberInfo);
int sceNpPartyGetMemberVoiceInfo(const SceNpPartyRoomMemberId memberId,
				 SceNpPartyMemberVoiceState *memberVoiceState);
int sceNpPartyGetMemberSessionInfo(const SceNpPartyRoomMemberId memberId,
				   SceNpPartyMemberGameSessionUpdateInfo *memberGameSessionInfo);
int sceNpPartyGetState(SceNpPartyState *state);
int sceNpPartyRegisterHandler(const SceNpPartyEventHandlers *handlers, void *userdata);
int sceNpPartyCheckCallback(void);
int sceNpPartyGetId(SceNpPartyId *const id);
int sceNpPartyGetGameSessionReadyState(SceNpPartyGameSessionUpdateReady *readyState);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_PARTY_H_ */
