/*
 * SceNpBasic: the NP Basic client library. Sony built it with SNC 3.57 in C++; its asserts name
 * npbasic/setup.cpp and npbasic/jga_response.cpp. The exports' names are the SDK's (np/np_basic.h)
 * where it has one; the internal names are ours, taken from what the code does.
 */
#ifndef NPBASIC_INTERNAL_H
#define NPBASIC_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <np/np_basic.h>
#include <np/np_manager.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include "../../../../../lowlayer/src/ipmi_wrapper/ipmi_wrapper_client.h"
#include "../impsutility_ipc/np_imps_utility_ipc_message_client.h"

/* SCE_NP_BASIC_ERROR_* (SDK np/np_basic.h), as signed values */
#define NP_BASIC_ERROR_UNKNOWN			-2141905663	/* 0x80551d01 */
#define NP_BASIC_ERROR_INVALID_ARGUMENT		-2141905662	/* 0x80551d02 */
#define NP_BASIC_ERROR_OUT_OF_MEMORY		-2141905661	/* 0x80551d03 */
#define NP_BASIC_ERROR_NOT_INITIALIZED		-2141905660	/* 0x80551d04 */
#define NP_BASIC_ERROR_ALREADY_INITIALIZED	-2141905659	/* 0x80551d05 */
#define NP_BASIC_ERROR_SIGNED_OUT		-2141905658	/* 0x80551d06 */
#define NP_BASIC_ERROR_NOT_ONLINE		-2141905657	/* 0x80551d07 */
#define NP_BASIC_ERROR_DATA_NOT_FOUND		-2141905656	/* 0x80551d08 */
#define NP_BASIC_ERROR_BUSY			-2141905655	/* 0x80551d09 */
#define NP_BASIC_ERROR_NO_COMM_ID_SUPPLIED	-2141905653	/* 0x80551d0b */
#define NP_BASIC_ERROR_NO_RESULT		-2141905651	/* 0x80551d0d, not in the SDK */
#define NP_BASIC_ERROR_NO_NP_COMM_ID_IN_PARAM_SFO	-2141905650	/* 0x80551d0e */

/* the IPMI client the library talks to the NP service through, the library's state */

/* the NP service's client object (0 when there is none) */
typedef struct NpBasicClient NpBasicClient;

/* the SceIpmi imports (client calls), nameless: the NID is all there is */
#ifdef __cplusplus
extern "C" {
#endif
extern NpBasicClient *SceIpmi_9EE9496B(int client);
extern SceUInt32 SceLibKernel_9F793F84(void);
extern SceInt32 sceKernelGetSystemTime(SceKernelSysClock *pClock);
extern int sceNpBasicUnregisterJoinGameAckHandler(void);
extern int SceNpManager_96DF1B76(const SceNpCommunicationId **ppCommId);
extern SceSize SceIpmi_006EFA9D(const IpmiWrapper::Client::Config *pConfig);
extern int SceIpmi_22625598(IpmiWrapper::Client **ppClient, const IpmiWrapper::Client::Config *pConfig, void *pMemory);
extern int SceIpmi_B56BAC7F(IpmiWrapper::Client *pClient);
extern int SceIpmi_B943F5D9(IpmiWrapper::Client *pClient);
extern int SceIpmi_D73C03D2(IpmiWrapper::Client *pClient);
#ifdef __cplusplus
}
#endif

/* the record the service answers a presence or own-information request with (1016 bytes) */
typedef struct NpBasicPresenceTail {
	int data[115];
} NpBasicPresenceTail;

typedef struct NpBasicPresenceRecord {
	unsigned char reserved0[12];
	char title[SCE_NP_BASIC_TITLE_SIZE_MAX];
	SceNpCommunicationId friendCommId;
	SceNpCommunicationId commId;
	unsigned char block[48];
	SceNpCommunicationId extraCommId;
	SceNpBasicInGamePresence inGamePresence;
	NpBasicPresenceTail tail;
} NpBasicPresenceRecord;

/* what the friend information exports give back (552 bytes) */
typedef struct NpBasicFriendInfo {
	SceSize size;
	SceNpCommunicationId friendCommId;
	unsigned char block[48];
	SceNpCommunicationId commId;
	SceNpCommunicationId extraCommId;
	SceNpBasicGamePresence gamePresence;
} NpBasicFriendInfo;

/* the communication ID of the title, set when the handler is registered */

/* an IPMI error of the service as the error of the library */
extern int npBasicConvertError(int error);

/* the buffers of an IPMI call: the input ones, and the output ones that the service fills in */
typedef struct NpBasicIpmiIn {
	const void *data;
	SceSize size;
} NpBasicIpmiIn;

typedef struct NpBasicIpmiOut {
	void *data;
	SceSize size;
	SceSize actualSize;
} NpBasicIpmiOut;

/* the client object of the service: slot 5 of its table makes a synchronous call */
typedef struct NpBasicClientVtbl {
	void *slots[5];
	int (*invoke)(NpBasicClient *client, int command, const NpBasicIpmiIn *in, int inNum, int *result,
		NpBasicIpmiOut *out, int outNum);
	void *slots2[8];
	int (*receive)(NpBasicClient *client, int type, void *buffer, void *received, int size);
} NpBasicClientVtbl;

struct NpBasicClient {
	const NpBasicClientVtbl *vtbl;
};

/* the requests the exports make over IPMI (npbasic/ipmi_client.cpp, ours) */
extern int npBasicRegisterHandler(NpBasicClient *client, const void *a2);
extern int npBasicUnregisterHandler(NpBasicClient *client);
extern int npBasicGetFriendListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetFriendListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicGetFriendPresence(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicGetBlockListEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetBlockListEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicCheckIfPlayerIsBlocked(NpBasicClient *client, const void *a2, unsigned char *a3, unsigned char *a4);
extern int npBasicRequest_9000D(NpBasicClient *client, const void *a2);
extern int npBasicRequest_9000E(NpBasicClient *client);
extern int npBasicRequest_9000F(NpBasicClient *client, const void *a2);
extern int npBasicRequest_90010(NpBasicClient *client);
extern int npBasicRequest_90011(NpBasicClient *client, const void *a2);
extern int npBasicRequest_90012(NpBasicClient *client);
extern int npBasicRequest_90013(NpBasicClient *client, const void *a2, const void *a3, const void *a4, int a5);
extern int npBasicRequest_90014(NpBasicClient *client, const void *a2, const void *a3, const void *a4, const void *a5);
extern int npBasicRequest_90015(NpBasicClient *client, int a2, void *a3, int *a4);
extern int npBasicRequest_90016(NpBasicClient *client, int a2, void *a3, int a4, void *a5, int a6, int *a7);
extern int npBasicRequest_9002B(NpBasicClient *client);
extern int npBasicGetFriendOnlineStatus(NpBasicClient *client, const void *a2, int *a3);
extern int npBasicGetGameJoiningPresence(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicGetFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicGetRequestedFriendRequestEntryCount(NpBasicClient *client, int *a2, unsigned char *a3);
extern int npBasicGetRequestedFriendRequestEntries(NpBasicClient *client, int a2, void *a3, int a4, int *a5, unsigned char *a6);
extern int npBasicRequest_90035(NpBasicClient *client, int a2);
extern int npBasicRequest_90036(NpBasicClient *client, int a2);
extern int npBasicRequest_90037(NpBasicClient *client, int a2, const void *a3, int a4);
extern int npBasicGetOwnInfo(NpBasicClient *client, const void *a2, void *a3);
extern int npBasicRequest_9003B(NpBasicClient *client, int a2);
extern int npBasicRequest_9003C(NpBasicClient *client, int *a2, void *a3);
extern int npBasicRequest_9003D(NpBasicClient *client, const void *a2, int *a3);
extern int npBasicRequest_9003E(NpBasicClient *client, const void *a2, int a3, void *a4);

/* npbasic/jga_ipc.cpp: the join game ack messages of the raw message queue */
#define NP_BASIC_JGA_ERROR_MESSAGE_TOO_LARGE	-2141874686	/* 0x80559602 */
#define NP_BASIC_JGA_ERROR_INVALID_ARGUMENT	-2141873663	/* 0x80559a01 */

/* a message of the service (received) */
typedef struct NpBasicJgaMessage {
	int type;
	int requestId;
	SceNpId npIdA;
	SceNpId npIdB;
	int dataSize;
	unsigned char data[172];
} NpBasicJgaMessage;

/* a message to the service (sent) */
typedef struct NpBasicJgaSendMessage {
	int type;
	int reserved;
	int requestId;
	SceNpId peer;
	SceNpId self;
	int dataSize;
	unsigned char data[168];
} NpBasicJgaSendMessage;

typedef struct NpBasicJgaEvent {
	int type;
	int requestId;
	const void *peer;
	const void *self;
	const void *data;
	int dataSize;
} NpBasicJgaEvent;

typedef void (*NpBasicJgaCallback)(const NpBasicJgaEvent *event);

extern NpBasicJgaCallback g_jgaCallback;	/* 0x81007000 */

extern int npBasicJgaStart(NpBasicJgaCallback callback);
extern int npBasicJgaStop(void);
extern int npBasicJgaSendMessage(int requestId, const SceNpId *self, const SceNpId *peer, const void *data,
				 SceSize dataSize);
extern int npBasicJgaPoll(void);
extern void npBasicJgaIpcCallback(const NpBasicJgaMessage *message);

/* the join game requests being answered: 8 entries (the library state) */
#define NP_BASIC_JOIN_GAME_ENTRY_MAX		8

#define NP_BASIC_JGA_ERROR_INVALID_HANDLER	-2141874172	/* 0x80559804 */
#define NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_TIMEDOUT	-2141905649	/* 0x80551d0f */
#define NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_CANCELED	-2141905648	/* 0x80551d10 */
#define NP_BASIC_ERROR_JOIN_GAME_ACK_REQUEST_INVALID	-2141905647	/* 0x80551d11 */

typedef struct NpBasicJoinGameEntry {
	SceNpId npId;
	int requestId;
	SceUInt64 deadline;
	unsigned char used;
	unsigned char canceled;
	unsigned char timedOut;
	unsigned char reserved[5];
} NpBasicJoinGameEntry;

typedef struct NpBasicJgaRequestHeader {
	int kind;
	int reserved;
	SceUInt64 deadline;
} NpBasicJgaRequestHeader;


/* what the title is told it may not yet send: pending presence for the rate limited sender */
typedef struct NpBasicPartyPresence {
	int size;
	int version;
	unsigned char data[452];
} NpBasicPartyPresence;

/* the sender thread's object: presence updates go out at most 6 in a row, one more every 60 s */
typedef struct NpBasicSender {
	SceInt64 interval;
	unsigned char tokens;
	SceUID thread;
	unsigned char stopRequested;
	SceUID timer;
	SceNpBasicInGamePresence presence;
	NpBasicPartyPresence partyPresence;
	int presenceState;
	int partyPresenceState;
} NpBasicSender;

/* the library's state (0x81007018): one object Sony's code reaches every part of through its base */
typedef struct NpBasicState {
	unsigned char initialized;
	int memBlock;
	int mspace;
	int ipcBuffer;
	int client;
	unsigned char handlerRegistered;
	SceNpCommunicationId commId;
	unsigned char reserved0[3];
	NpImpsUtilityIpcCallbacks callbackTable;
	NpBasicSender sender;
	SceKernelLwMutexWork mutex;
	unsigned char altHandlerRegistered;
	unsigned char reserved3[7];
	SceUInt64 sendTimes[20];
	unsigned char reserved5[8];
	NpBasicJoinGameEntry joinGameEntries[NP_BASIC_JOIN_GAME_ENTRY_MAX];
	SceNpBasicJoinGameAckRequestEventHandler joinGameAckHandler;
	void *joinGameAckArg;
	int joinGameAckMode;

	NpBasicState();
} NpBasicState;

extern NpBasicState g_state;		/* 0x81007018 */

extern int npBasicJgaRequestReceived(int requestId, SceNpId npId, SceUInt64 deadline) __attribute__((noinline));
extern int npBasicJgaCancelAll(void) __attribute__((noinline));
extern int npBasicJgaCheckTimeouts(void) __attribute__((noinline));
extern void npBasicJgaEventCallback(const NpBasicJgaEvent *event);

/* the event handlers a title registers (sceNpBasicRegisterHandler) */
typedef struct NpBasicEventHandlers {
	SceUInt32 sdkVersion;
	void (*friendList)(int count, const void *entries, void *arg);
	void (*friendOnlineStatus)(const void *message, int status, void *arg);
	void (*blockList)(int count, const void *entries, void *arg);
	void (*friendGamePresence)(const void *message, const SceNpBasicGamePresence *presence, void *arg);
	void (*inGameDataMessage)(const void *message, const void *data, void *arg);
	void (*friendContext)(const void *message, int state, void *arg);
	void (*friendRequest)(int count, const void *entries, void *arg);
} NpBasicEventHandlers;

/* the event handlers of SceNpBasic_6F1B58F9 */
typedef struct NpBasicEventHandlers2 {
	void (*myOnlineStatus)(int status, void *arg);
	void (*friendList)(int count, const void *entries, void *arg);
	void (*friendOnlineStatus)(const void *message, int status, void *arg);
	void (*blockList)(int count, const void *entries, void *arg);
	void (*friendPartyPresence)(const void *message, const void *presence, void *arg);
	void (*friendGamePresenceDetail)(const void *message, const void *detail, void *arg);
	void (*myGamePresenceDetail)(const void *message, void *arg);
	void (*friendRequest)(int count, const void *entries, void *arg);
} NpBasicEventHandlers2;

extern int npBasicSenderTakeToken(NpBasicSender *sender) __attribute__((noinline));
extern int npBasicSenderSetPresence(NpBasicSender *sender, const SceNpBasicInGamePresence *presence) __attribute__((noinline));
extern int npBasicSenderClearPresence(NpBasicSender *sender) __attribute__((noinline));
extern int npBasicSenderSetPartyPresence(NpBasicSender *sender, const NpBasicPartyPresence *presence) __attribute__((noinline));
extern int npBasicSenderClearPartyPresence(NpBasicSender *sender) __attribute__((noinline));

#endif
