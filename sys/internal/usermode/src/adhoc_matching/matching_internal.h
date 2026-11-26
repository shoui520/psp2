/*
 * SceNetAdhocMatching internal structures, protocol constants, and module-private interfaces.
 */
#ifndef ADHOC_MATCHING_INTERNAL_H
#define ADHOC_MATCHING_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <adhoc_matching.h>
#include <libnetctl.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <libnet/netinternal.h>

#define ADHOC_MATCHING_PROTOCOL_VERSION		1
#define ADHOC_MATCHING_UDP_PORT			0x0e4a
#define ADHOC_MATCHING_MAX_CONTEXTS		15
#define ADHOC_MATCHING_EVENT_PIPE_SIZE		0x1000
#define ADHOC_MATCHING_CALLOUT_STACK_SIZE	0x1000
#define ADHOC_MATCHING_INPUT_STACK_SIZE		0x1000
#define ADHOC_MATCHING_SDK_V150			0x01500000U
#define ADHOC_MATCHING_SDK_V310			0x03100000U
#define ADHOC_MATCHING_LEGACY_MAXOPTLEN		65495
#define ADHOC_MATCHING_LEGACY_MAXDATALEN	65495
#define ADHOC_MATCHING_LEGACY_MAXHELLOOPTLEN	65487

/* SceNetCallout error codes (shared with SceNpCommon callout) */
#define ADHOC_CALLOUT_ERROR_NOT_INITIALIZED	-2141880319	/* 0x80558001 */
#define ADHOC_CALLOUT_ERROR_ALREADY_INITIALIZED	-2141880318	/* 0x80558002 */
#define ADHOC_CALLOUT_ERROR_ALREADY_QUEUED	-2141880314	/* 0x80558006 */

/* Context lifecycle states */
enum AdhocMatchingContextState {
	ADHOC_MATCHING_CTX_STOPPED = 0,
	ADHOC_MATCHING_CTX_STOPPING = 1,
	ADHOC_MATCHING_CTX_RUNNING = 2
};

/* Peer protocol states */
enum AdhocMatchingPeerState {
	ADHOC_MATCHING_PEER_INIT = 1,
	ADHOC_MATCHING_PEER_REQUEST_RECV = 2,
	ADHOC_MATCHING_PEER_ACCEPT_SENT = 3,
	ADHOC_MATCHING_PEER_REQUEST_SENT = 4,
	ADHOC_MATCHING_PEER_ESTABLISHED = 5
};

/* Peer reliable-data transfer states */
enum AdhocMatchingDataState {
	ADHOC_MATCHING_DATA_IDLE = 1,
	ADHOC_MATCHING_DATA_SENDING = 2
};

/* On-wire matching packet types */
enum AdhocMatchingPacketType {
	ADHOC_MATCHING_PKT_HELLO = 1,
	ADHOC_MATCHING_PKT_REQUEST = 2,
	ADHOC_MATCHING_PKT_ACCEPT = 3,
	ADHOC_MATCHING_PKT_CONFIRM = 4,
	ADHOC_MATCHING_PKT_CANCEL = 5,
	ADHOC_MATCHING_PKT_KEEPALIVE = 6,
	ADHOC_MATCHING_PKT_KEEPALIVE_ACK = 7,
	ADHOC_MATCHING_PKT_BYE = 8,
	ADHOC_MATCHING_PKT_REQUEST_ACK = 9,
	ADHOC_MATCHING_PKT_DATA = 10,
	ADHOC_MATCHING_PKT_DATA_ACK = 11,
	ADHOC_MATCHING_PKT_MAX = 12
};

/* Internal event pipe message types */
enum AdhocMatchingEventType {
	ADHOC_MATCHING_EV_RECV_PACKET = 1,
	ADHOC_MATCHING_EV_REXMT_TIMEOUT = 2,
	ADHOC_MATCHING_EV_KEEPALIVE_TIMEOUT = 3,
	ADHOC_MATCHING_EV_HELLO_TIMEOUT = 4,
	ADHOC_MATCHING_EV_DATA_TIMEOUT = 5
};

#define ADHOC_MATCHING_EV_FLAG_QUEUED		1U
#define ADHOC_MATCHING_CAP_REXMT_TIMEOUT	1U

typedef struct AdhocMatchingCallout {
	struct AdhocMatchingCallout *next;
	void (*func)(void *arg);
	SceUInt64 time;
	void *arg;
	SceUInt32 reserved;
} AdhocMatchingCallout;

typedef struct AdhocMatchingCalloutContext {
	SceUID thread;
	SceUID mutex;
	SceUID cond;
	int initialized;
	int stop;
	AdhocMatchingCallout *list;
} AdhocMatchingCalloutContext;

struct AdhocMatchingPeer;

typedef struct AdhocMatchingEventMessage {
	int type;
	struct AdhocMatchingPeer *peer;
	unsigned int flags;
} AdhocMatchingEventMessage;

typedef struct AdhocMatchingPeer {
	struct AdhocMatchingPeer *next;
	int state;
	SceNetInAddr addr;
	unsigned int helloInterval;
	unsigned int keepaliveInterval;
	int recvPacketSize;
	unsigned char *recvPacket;
	int recvPayloadEnd;
	int optLen;
	void *opt;
	AdhocMatchingEventMessage recvEvent;
	AdhocMatchingEventMessage timerEvent;
	int timerActive;
	int keepaliveCount;
	int rexmtCount;
	SceUID eventPipe;
	unsigned char remoteCaps[4];
	int byeReceived;
	unsigned int localSessionId;
	unsigned int remoteSessionId;
	unsigned int sendDataSeq;
	unsigned int recvDataSeq;
	int dataState;
	int sendDataLen;
	void *sendData;
	AdhocMatchingEventMessage dataTimerEvent;
	int dataTimerActive;
	int dataRexmtCount;
	AdhocMatchingCallout protocolCallout;
	AdhocMatchingCallout dataCallout;
} AdhocMatchingPeer;

typedef struct AdhocMatchingContext {
	struct AdhocMatchingContext *next;
	int id;
	int state;
	int mode;
	int maxNum;
	SceShort16 port;
	int rxBufLen;
	void *rxBuf;
	unsigned int helloInterval;
	unsigned int keepaliveInterval;
	int initCount;
	unsigned int rexmtInterval;
	SceNetAdhocMatchingHandler handler;
	void *reserved;
	SceNetId sendSock;
	SceNetId recvSock;
	SceUID eventThread;
	SceUID eventPipe;
	SceUID inputThread;
	int helloPacketLen;
	unsigned char *helloPacket;
	AdhocMatchingEventMessage helloEvent;
	int keepalivePacketLen;
	unsigned char *keepalivePacket;
	int helloTimerActive;
	unsigned char localCaps[4];
	AdhocMatchingPeer *peers;
	int pad;
	AdhocMatchingCalloutContext calloutContext;
	AdhocMatchingCallout helloCallout;
} AdhocMatchingContext;

#ifdef __cplusplus
extern "C" {
#endif

extern SceNetInAddr g_adhocMatchingLocalAddr;

/* SceThreadmgrCoredumpTime */
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* matching_event.c */
int matchingEventStart(AdhocMatchingContext *ctx, int priority, SceSize stackSize,
		       int cpuAffinityMask);
int matchingEventStop(AdhocMatchingContext *ctx, int *lockStatus);
void matchingInvokeHandler(AdhocMatchingContext *ctx, unsigned int stackPad, int id, int event,
			   SceNetInAddr *peer, int optlen, void *opt);
void matchingProcessPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingEventThread(SceSize argSize, void *argBlock);

/* matching_context.c */
void matchingValidateAddr(const SceNetInAddr *addr);
int matchingUpdateLocalAddr(void);
int matchingLockInit(void);
void matchingLockTerm(void);
int matchingLock(void);
int matchingUnlock(int lockStatus);
int matchingContextListInit(void);
void matchingFree(void *ptr);
int matchingContextListTerm(void);
int matchingCheckNotRunning(void);
int matchingPoolCreate(SceSize size, void *memblock);
void matchingPoolDestroy(void);
void *matchingAlloc(SceSize size);
AdhocMatchingContext *matchingFindContext(int id);
int matchingAllocContext(SceUShort16 port);
void matchingFreeContext(AdhocMatchingContext *ctx);
void matchingSetPeerDataState(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int state);
void matchingSetPeerState(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int state);
AdhocMatchingPeer *matchingAllocPeer(AdhocMatchingContext *ctx, const SceNetInAddr *addr);
AdhocMatchingPeer *matchingFindPeer(AdhocMatchingContext *ctx, const SceNetInAddr *addr);
void matchingFreePeer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingFreeAllPeers(AdhocMatchingContext *ctx);
void matchingCollectPeers(AdhocMatchingContext *ctx, int minState, SceNetInAddr *outAddrs,
			  int *count);
int matchingCountPeers(AdhocMatchingContext *ctx, int minState);
void matchingCopyMembers(AdhocMatchingContext *ctx, SceSize *memberNum,
			 SceNetAdhocMatchingMember *members);
int matchingIsHigherAddr(const AdhocMatchingPeer *peer);

/* matching_send.c */
int matchingSendSocketOpen(AdhocMatchingContext *ctx);
int matchingSendSocketClose(AdhocMatchingContext *ctx);
int matchingSendControlPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int pktType,
			      int optlen, const void *opt);
int matchingSendDataPacket(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer, int pktType,
			   int datalen, const void *data);
int matchingBuildHelloPacket(AdhocMatchingContext *ctx, int optlen, const void *opt);
int matchingFreeHelloPacket(AdhocMatchingContext *ctx);
int matchingSendHello(AdhocMatchingContext *ctx);
int matchingBuildKeepalivePacket(AdhocMatchingContext *ctx);
int matchingSendKeepalive(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingSaveKeepalivePacket(AdhocMatchingContext *ctx, const void *packet, SceSize len);
int matchingFreeKeepalivePacket(AdhocMatchingContext *ctx);
int matchingSendBye(AdhocMatchingContext *ctx);

/* matching_recv.c */
int matchingRecvStart(AdhocMatchingContext *ctx, int priority, SceSize stackSize,
		      int cpuAffinityMask);
void matchingRecvStop(AdhocMatchingContext *ctx, int *lockStatus);
int matchingInputThread(SceSize argSize, void *argBlock);
int matchingStartRexmtTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingStartDataTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingStartKeepaliveTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingStartHelloTimer(AdhocMatchingContext *ctx, unsigned int interval);
int matchingStopPeerTimers(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingStopDataTimer(AdhocMatchingContext *ctx, AdhocMatchingPeer *peer);
int matchingStopHelloTimer(AdhocMatchingContext *ctx);
void matchingOnRexmtTimeout(void *arg);
void matchingOnDataTimeout(void *arg);
void matchingOnKeepaliveTimeout(void *arg);
void matchingOnHelloTimeout(void *arg);

/* callout.c */
int matchingCalloutInit(AdhocMatchingCalloutContext *ctx, const char *name, SceSize stackSize,
			int priority, int cpuAffinityMask);
int matchingCalloutTerm(AdhocMatchingCalloutContext *ctx);
int matchingCalloutAdd(AdhocMatchingCalloutContext *ctx, AdhocMatchingCallout *callout,
		       SceUInt64 delay, void (*func)(void *arg), void *arg);
int matchingCalloutRemove(AdhocMatchingCalloutContext *ctx, AdhocMatchingCallout *callout,
			  int *removed);

#ifdef __cplusplus
}
#endif

#endif /* ADHOC_MATCHING_INTERNAL_H */
