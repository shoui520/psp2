/*
 * SceNpImpsUtilityIpc: the message client of the NP service's IMPS utility. The service queues
 * event messages; TryGetIpmiMessages reads them and calls the callback of the message's kind.
 */
#ifndef NP_IMPS_UTILITY_IPC_MESSAGE_CLIENT_H
#define NP_IMPS_UTILITY_IPC_MESSAGE_CLIENT_H

#include <scetypes.h>
#include <np/np_common.h>
#include <np/np_basic.h>

/* the event callbacks of a client, each with the argument it was registered with */
typedef struct NpImpsUtilityIpcCallbacks {
	void (*serviceState)(int state, int reserved, void *arg);
	void *serviceStateArg;
	void (*myOnlineStatus)(int status, void *arg);
	void *myOnlineStatusArg;
	void (*friendList)(int count, const void *entries, void *arg);
	void *friendListArg;
	void (*friendOnlineStatus)(const void *message, int status, void *arg);
	void *friendOnlineStatusArg;
	void (*friendGamePresence)(const void *message, const SceNpBasicGamePresence *presence, void *arg);
	void *friendGamePresenceArg;
	void (*friendPartyPresence)(const void *message, const void *presence, void *arg);
	void *friendPartyPresenceArg;
	void (*inGameDataMessage)(const void *message, const void *data, void *arg);
	void *inGameDataMessageArg;
	void (*friendGamePresenceDetail)(const void *message, const void *detail, void *arg);
	void *friendGamePresenceDetailArg;
	void (*blockList)(int count, const void *entries, void *arg);
	void *blockListArg;
	void (*myGamePresenceDetail)(const void *message, void *arg);
	void *myGamePresenceDetailArg;
	void (*friendContext)(const void *message, int state, void *arg);
	void *friendContextArg;
	void (*platformFriendGamePresenceDetail)(const void *message, int platform, const void *detail, void *arg);
	void *platformFriendGamePresenceDetailArg;
	void (*myPartyPresence)(const void *message, void *arg);
	void *myPartyPresenceArg;
	void (*friendRequest)(int count, const void *entries, void *arg);
	void *friendRequestArg;
} NpImpsUtilityIpcCallbacks;

/* a message of the service's queue (1024 bytes at most) */
typedef struct NpImpsUtilityIpcMessage {
	int kind;
	int value;
	unsigned char entries[32];
	SceNpCommunicationId commId;
	unsigned char detail[16];
	unsigned char body[524];
	int platform;
	unsigned char reserved[428];
} NpImpsUtilityIpcMessage;

extern void npImpsUtilityIpcTryGetMessages(int client, const NpImpsUtilityIpcCallbacks *callbacks,
					  const SceNpCommunicationId *commId);

#endif
