/*
 * np/np_basic.h: the NP Basic library's game presence, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_basic.h), as user code needs them.
 */
#ifndef _SCE_NP_BASIC_H_
#define _SCE_NP_BASIC_H_

#include <scetypes.h>
#include <np/np_common.h>

#define SCE_NP_BASIC_TITLE_SIZE_MAX			128

#define SCE_NP_BASIC_IN_GAME_PRESENCE_STATUS_SIZE_MAX	192
#define SCE_NP_BASIC_IN_GAME_PRESENCE_DATA_SIZE_MAX	128

typedef enum {
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_UNKNOWN = -1,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_NONE = 0,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_DEFAULT = 1,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_GAME_JOINING = 2,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_GAME_JOINING_ONLY_FOR_PARTY = 3,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_JOIN_GAME_ACK = 4,
	SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_MAX = 5
} SceNpBasicInGamePresenceType;

typedef struct SceNpBasicInGamePresence {
	SceUInt32 sdkVersion;
	SceNpBasicInGamePresenceType type;
	char status[SCE_NP_BASIC_IN_GAME_PRESENCE_STATUS_SIZE_MAX];
	SceUInt8 data[SCE_NP_BASIC_IN_GAME_PRESENCE_DATA_SIZE_MAX];
	SceSize dataSize;
} SceNpBasicInGamePresence;

typedef struct SceNpBasicGamePresence {
	SceSize size;
	char title[SCE_NP_BASIC_TITLE_SIZE_MAX];
	SceNpBasicInGamePresence inGamePresence;
} SceNpBasicGamePresence;

#define SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_DENY_REASON_SIZE_MAX	128

typedef enum {
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_UNKNOWN = 0,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_ALLOW = 1,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_DENY = 2,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_DENY_FULL = 3,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_DENY_WITH_REASON = 4,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_SYSTEM_ERROR_TIMEOUT = 5,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_SYSTEM_ERROR_GENERAL = 6,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_SYSTEM_ERROR_GAME_DOES_NOT_EXIST = 7,
	SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_TYPE_MAX = 8
} SceNpBasicJoinGameAckResponseType;

typedef void (*SceNpBasicJoinGameAckRequestEventHandler)(const SceNpId fromNpId, void *userdata);

typedef struct SceNpBasicJoinGameAckResponse {
	SceNpId npId;
	SceNpBasicJoinGameAckResponseType type;
	char denyReason[SCE_NP_BASIC_JOIN_GAME_ACK_RESPONSE_DENY_REASON_SIZE_MAX];
} SceNpBasicJoinGameAckResponse;

#endif /* _SCE_NP_BASIC_H_ */
