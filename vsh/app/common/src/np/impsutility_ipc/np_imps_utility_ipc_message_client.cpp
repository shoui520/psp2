/*
 * SceNpImpsUtilityIpc: reads the event messages the NP service queued for a client and gives each
 * to the callback of its kind. Sony's path: vsh/app/common/src/np/impsutility_ipc/
 * np_imps_utility_ipc_message_client.cpp; the process...EventAsyncMsg functions it names are
 * inlined into TryGetIpmiMessages.
 */
#include "../npbasic/npbasic_internal.h"
#include "np_imps_utility_ipc_message_client.h"

#define NP_IMPS_IPC_SOURCE_FILE \
	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\impsutility_ipc\\np_imps_utility_ipc_message_client.cpp"
#define NP_IMPS_IPC_ASSERT_SIZE(size, expected, line, func)				\
	do {										\
		if ((size) != (expected)) {						\
			sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n",	\
				      NP_IMPS_IPC_SOURCE_FILE, line, func);		\
			*(volatile int *)0 = 0;						\
		}									\
	} while (0)

/* is the communication ID missing or all zero */
static inline int isEmptyCommId(const SceNpCommunicationId *commId, unsigned char *none)
	__attribute__((always_inline));
static inline int isEmptyCommId(const SceNpCommunicationId *commId, unsigned char *none)
{
	sceClibMemset(none, 0, 12);
	if (commId == NULL || sceClibMemcmp(none, commId, 12) == 0)
		return 1;
	return 0;
}

void npImpsUtilityIpcTryGetMessages(int client, const NpImpsUtilityIpcCallbacks *callbacks,
				    const SceNpCommunicationId *commId)
{
	NpImpsUtilityIpcMessage message;
	SceNpBasicGamePresence presence;
	NpBasicClient *ipmi;
	unsigned char noneInGameData[12];
	unsigned char noneFriendContext[12];
	unsigned char noneFriendContextOwn[12];
	unsigned char noneGamePresence2[12];
	unsigned char noneGamePresence3[12];
	unsigned char noneGamePresence1[12];
	unsigned char noneGamePresence[12];
	int received;
	volatile int result;

	if (client == 0) {
		sceClibPrintf("## ASSERTION FAILURE in %s(%d): %s() \n", NP_IMPS_IPC_SOURCE_FILE, 441,
			      "TryGetIpmiMessages");
		*(volatile int *)0 = 0;
	}
	for (;;) {
		ipmi = SceIpmi_9EE9496B(client);
		result = ipmi->vtbl->receive(ipmi, 0, &message, &received, 1024);
		if (result != 0)
			break;
		switch (message.kind) {
		case 0:
			NP_IMPS_IPC_ASSERT_SIZE(received, 12, 81, "processServiceStateEventAsyncMsg");
			if (callbacks->serviceState != NULL)
				callbacks->serviceState(message.value, *(int *)message.entries, callbacks->serviceStateArg);
			break;
		case 1: {
			void (*callback)(int, void *);
			int status;

			NP_IMPS_IPC_ASSERT_SIZE(received, 8, 101, "processMyOnlineStatusEventAsyncMsg");
			callback = callbacks->myOnlineStatus;
			if (message.value > 1) {
				if (message.value <= 2) {
					status = 2;
					goto call1;
				}
				if (message.value <= 3) {
					status = 3;
					goto call1;
				}
			} else if (message.value > 0) {
				status = 1;
				goto call1;
			}
			status = 0;
call1:
			if (callback != NULL)
				callback(status, callbacks->myOnlineStatusArg);
			break;
		}
		case 2:
			NP_IMPS_IPC_ASSERT_SIZE(received, 44, 123, "processFriendListEventAsyncMsg");
			if (callbacks->friendList != NULL)
				callbacks->friendList(message.value, message.entries, callbacks->friendListArg);
			break;
		case 3:
			NP_IMPS_IPC_ASSERT_SIZE(received, 44, 148, "processFriendOnlineStatusEventAsyncMsg");
			if (callbacks->friendOnlineStatus != NULL)
				callbacks->friendOnlineStatus(&message.value, *(int *)&message.commId,
							      callbacks->friendOnlineStatusArg);
			break;
		case 4: {
			void (*callback)(const void *, const SceNpBasicGamePresence *, void *);
			int notGiven;
			bool same;

			NP_IMPS_IPC_ASSERT_SIZE(received, 532, 213, "processFriendGamePresenceEventAsyncMsg");
			sceClibMemset(&presence, 0, sizeof(presence));
			if (message.detail[12] != 0) {
				sceClibMemset(noneGamePresence, 0, sizeof(noneGamePresence));
				if (commId == NULL || sceClibMemcmp(noneGamePresence, commId, sizeof(noneGamePresence)) == 0)
					notGiven = 1;
				else
					notGiven = 0;
				same = false;
				if (notGiven == 0)
					same = sceClibMemcmp(commId, &message.commId, sizeof(noneGamePresence)) == 0;
				callback = callbacks->friendGamePresence;
				if (same)
					presence = *(const SceNpBasicGamePresence *)message.body;
				else
					presence.inGamePresence.type = SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_UNKNOWN;
			} else {
				sceClibMemset(noneGamePresence3, 0, sizeof(noneGamePresence3));
				if (commId != NULL && sceClibMemcmp(noneGamePresence3, commId, sizeof(noneGamePresence3)) != 0
				    && sceClibMemcmp(commId, message.detail, sizeof(noneGamePresence3)) == 0) {
					sceClibMemset(noneGamePresence2, 0, sizeof(noneGamePresence2));
					same = false;
					if (sceClibMemcmp(noneGamePresence2, commId, sizeof(noneGamePresence2)) != 0)
						same = sceClibMemcmp(commId, &message.commId, sizeof(noneGamePresence2)) == 0;
					callback = callbacks->friendGamePresence;
					if (same)
						presence = *(const SceNpBasicGamePresence *)message.body;
					else
						presence.inGamePresence.type = SCE_NP_BASIC_IN_GAME_PRESENCE_TYPE_UNKNOWN;
				} else {
					sceClibMemset(noneGamePresence1, 0, sizeof(noneGamePresence1));
					if (commId == NULL || sceClibMemcmp(noneGamePresence1, commId, sizeof(noneGamePresence1)) == 0)
						notGiven = 1;
					else
						notGiven = 0;
					same = false;
					if (notGiven == 0)
						same = sceClibMemcmp(commId, &message.commId, sizeof(noneGamePresence1)) == 0;
					callback = callbacks->friendGamePresence;
					if (same)
						presence = *(const SceNpBasicGamePresence *)message.body;
				}
			}
			if (callback != NULL)
				callback(&message.value, &presence, callbacks->friendGamePresenceArg);
			break;
		}
		case 5:
			NP_IMPS_IPC_ASSERT_SIZE(received, 500, 270, "processFriendPartyPresenceEventAsyncMsg");
			if (callbacks->friendPartyPresence != NULL)
				callbacks->friendPartyPresence(&message.value, &message.commId,
							       callbacks->friendPartyPresenceArg);
			break;
		case 6:
			NP_IMPS_IPC_ASSERT_SIZE(received, 568, 314, "processInGameDataMessageEventAsyncMsg");
			sceClibMemset(noneInGameData, 0, sizeof(noneInGameData));
			if (commId != NULL && sceClibMemcmp(noneInGameData, commId, sizeof(noneInGameData)) != 0
			    && sceClibMemcmp(commId, &message.commId, sizeof(noneInGameData)) == 0) {
				if (callbacks->inGameDataMessage != NULL)
					callbacks->inGameDataMessage(&message.value, message.detail,
								     callbacks->inGameDataMessageArg);
			}
			break;
		case 7:
			NP_IMPS_IPC_ASSERT_SIZE(received, 592, 291, "processFriendGamePresenceDetailEventAsyncMsg");
			if (callbacks->friendGamePresenceDetail != NULL)
				callbacks->friendGamePresenceDetail(&message.value, &message.commId,
								    callbacks->friendGamePresenceDetailArg);
			break;
		case 8:
			NP_IMPS_IPC_ASSERT_SIZE(received, 44, 173, "processBlockListEventAsyncMsg");
			if (callbacks->blockList != NULL)
				callbacks->blockList(message.value, message.entries, callbacks->blockListArg);
			break;
		case 9:
			NP_IMPS_IPC_ASSERT_SIZE(received, 556, 192, "processMyGamePresenceDetailEventAsyncMsg");
			if (callbacks->myGamePresenceDetail != NULL)
				callbacks->myGamePresenceDetail(&message.value, callbacks->myGamePresenceDetailArg);
			break;
		case 10: {
			int state;
			int zero;
			int level;

			NP_IMPS_IPC_ASSERT_SIZE(received, 52, 343, "processFriendContextEventAsyncMsg");
			zero = isEmptyCommId(&message.commId, noneFriendContext);
			state = 0;
			if (zero == 0) {
				level = 1;
				sceClibMemset(noneFriendContextOwn, 0, sizeof(noneFriendContextOwn));
				if (commId != NULL && sceClibMemcmp(noneFriendContextOwn, commId, sizeof(noneFriendContextOwn)) != 0
				    && sceClibMemcmp(commId, &message.commId, sizeof(noneFriendContextOwn)) == 0)
					level = 2;
				state = level;
			}
			if (callbacks->friendContext != NULL)
				callbacks->friendContext(&message.value, state, callbacks->friendContextArg);
			break;
		}
		case 11:
			NP_IMPS_IPC_ASSERT_SIZE(received, 596, 377,
						"processPlatformIdentifiableFriendGamePresenceDetailEventAsyncMsg");
			if (callbacks->platformFriendGamePresenceDetail != NULL)
				callbacks->platformFriendGamePresenceDetail(&message.value, message.platform,
									    &message.commId,
									    callbacks->platformFriendGamePresenceDetailArg);
			break;
		case 12:
			NP_IMPS_IPC_ASSERT_SIZE(received, 464, 400, "processMyPartyPresenceEventAsyncMsg");
			if (callbacks->myPartyPresence != NULL)
				callbacks->myPartyPresence(&message.value, callbacks->myPartyPresenceArg);
			break;
		case 13:
			NP_IMPS_IPC_ASSERT_SIZE(received, 44, 419, "processFriendRequestEventAsyncMsg");
			if (callbacks->friendRequest != NULL)
				callbacks->friendRequest(message.value, message.entries, callbacks->friendRequestArg);
			break;
		default:
			break;
		}
		if (result != 0)
			break;
	}
}
