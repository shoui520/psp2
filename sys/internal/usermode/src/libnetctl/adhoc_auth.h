/*
 * adhoc_auth.h: what the ad-hoc authentication files share (adhoc_auth.c, adhoc_auth_sys.c,
 * adhoc_auth_peer.c): the packet and peer constants, the session structures, the globals and
 * the functions.
 */
#ifndef NETCTL_ADHOC_AUTH_H
#define NETCTL_ADHOC_AUTH_H

#include <np/np_common.h>
#include "netctl_internal.h"

/* the packets: a version byte, the type, the payload's length (network order), payload */
#define NETCTL_ADHOC_AUTH_VERSION		1
#define NETCTL_ADHOC_AUTH_PACKET_SIZE		1024
#define NETCTL_ADHOC_AUTH_HEADER_SIZE		4
#define NETCTL_ADHOC_AUTH_PAYLOAD(packet)	(&(packet)[NETCTL_ADHOC_AUTH_HEADER_SIZE])

#define NETCTL_ADHOC_AUTH_PACKET_BEACON		1	/* nothing to announce: the peers are full */
#define NETCTL_ADHOC_AUTH_PACKET_HELLO		2	/* the peer record */
#define NETCTL_ADHOC_AUTH_PACKET_REQUEST	3	/* first exchange and the peer record */
#define NETCTL_ADHOC_AUTH_PACKET_RESPONSE	4	/* second exchange */
#define NETCTL_ADHOC_AUTH_PACKET_CONFIRM	5	/* third exchange */
#define NETCTL_ADHOC_AUTH_PACKET_FINISH		6	/* fourth exchange */
#define NETCTL_ADHOC_AUTH_PACKET_BYE		7
#define NETCTL_ADHOC_AUTH_PACKET_KEEPALIVE	8

/* the UDP port and the virtual ports of the receiving and sending sockets */
#define NETCTL_ADHOC_AUTH_PORT			0xe4a
#define NETCTL_ADHOC_AUTH_RECV_VPORT		0x8235
#define NETCTL_ADHOC_AUTH_SEND_VPORT		0x8236

/* the state of a peer */
#define NETCTL_ADHOC_AUTH_PEER_NEW		0
#define NETCTL_ADHOC_AUTH_PEER_REQUESTED	1	/* sent the request */
#define NETCTL_ADHOC_AUTH_PEER_CONFIRMED	2	/* sent the confirmation */
#define NETCTL_ADHOC_AUTH_PEER_RESPONDED	3	/* sent the response */
#define NETCTL_ADHOC_AUTH_PEER_AUTHENTICATED	4

/* the messages the timers send to the authentication thread */
#define NETCTL_ADHOC_AUTH_MESSAGE_RESEND	1
#define NETCTL_ADHOC_AUTH_MESSAGE_BEACON	3
#define NETCTL_ADHOC_AUTH_MESSAGE_KEEPALIVE	4
#define NETCTL_ADHOC_AUTH_MESSAGE_TIMEOUT	5
#define NETCTL_ADHOC_AUTH_MESSAGE_STOP		6

/* the context's flags */
#define NETCTL_ADHOC_AUTH_FLAG_BEACON_QUEUED	0x2

/* the network stack's requests (SceNetInternal_333F9CB3) and socket option */
#define NETCTL_ADHOC_AUTH_IOCTL_SET_COMM_ID	0x801050d8
#define NETCTL_ADHOC_AUTH_IOCTL_DELETE_COMM_ID	0x801050d9
#define NETCTL_ADHOC_AUTH_IOCTL_CLEAR_KEYS	0x802450c8
#define NETCTL_ADHOC_AUTH_IOCTL_SET_KEY		0x802450c9
#define NETCTL_ADHOC_AUTH_IOCTL_ADD_PEER_KEY	0x802450cb
#define NETCTL_ADHOC_AUTH_IOCTL_DELETE_PEER_KEY	0x802450cc
#define NETCTL_ADHOC_AUTH_IOCTL_SET_SIGN_KEY	0x802450cd
#define NETCTL_ADHOC_AUTH_SO_USEAUTH		0x00100000

/* the system software version from which the peers come from a static pool */
#define NETCTL_ADHOC_AUTH_POOL_VERSION		0x01800000

#define NETCTL_ERROR_ADHOC_AUTH_ALREADY_INITIALIZED	-2143215193	/* 0x804121a7 */
#define NETCTL_ERROR_ADHOC_AUTH_INVALID_POINTER		-2143215186	/* 0x804121ae */
#define NETCTL_ERROR_ADHOC_AUTH_FAILED			-2143215185	/* 0x804121af */
#define NETCTL_ERROR_ADHOC_AUTH_CALLOUT_RUNNING		-2143215183	/* 0x804121b1 */
#define NETCTL_ERROR_ADHOC_AUTH_CALLOUT_QUEUED		-2143215181	/* 0x804121b3 */

/* a timer of the callout thread, in its list sorted by time */
typedef struct NetCtlAdhocAuthCallout {
	struct NetCtlAdhocAuthCallout *next;	/* 0x00 */
	void (*func)(void *arg);		/* 0x04 */
	SceUInt64 time;				/* 0x08 */
	void *arg;				/* 0x10 */
	SceUInt32 reserved;			/* 0x14 */
} NetCtlAdhocAuthCallout;

/* what a timer sends to the authentication thread */
typedef struct NetCtlAdhocAuthMessage {
	SceUInt32 reserved[2];			/* 0x00 */
	int type;				/* 0x08 */
	SceNetSockaddrIn from;			/* 0x0C */
	SceUInt32 reserved2;			/* 0x1C */
} NetCtlAdhocAuthMessage;

/* the request that installs a key for a peer in the network stack */
typedef struct NetCtlAdhocAuthKeyParam {
	SceUShort16 reserved;			/* 0x00 */
	SceNetInPort_t port;			/* 0x02 */
	SceNetInAddr addr;			/* 0x04 */
	SceUChar8 reserved2[8];			/* 0x08 */
	SceUChar8 key[16];			/* 0x10 */
	SceUShort16 reserved3;			/* 0x20 */
	SceUChar8 direction;			/* 0x22: 0 receive, 1 send */
	SceUChar8 type;				/* 0x23 */
} NetCtlAdhocAuthKeyParam;

/* the request a notification is shown with */
typedef struct NetCtlAdhocAuthNotification {
	SceUInt32 reserved;			/* 0x00 */
	char text[60];				/* 0x04 */
} NetCtlAdhocAuthNotification;

/* the timer intervals of a session (SceNetCtl_1172C5FC) */
typedef struct NetCtlAdhocAuthParam {
	SceUInt32 beaconInterval;		/* 0x00 */
	SceUInt32 resendInterval;		/* 0x04 */
	SceUInt32 maxRetries;			/* 0x08 */
	SceUInt32 keepaliveInterval;		/* 0x0C */
	SceUInt32 peerTimeout;			/* 0x10 */
} NetCtlAdhocAuthParam;

struct NetCtlAdhocAuth;

/* a peer in the context's list */
typedef struct NetCtlAdhocAuthPeer {
	struct NetCtlAdhocAuthPeer *next;		/* 0x000 */
	struct NetCtlAdhocAuthPeer *prev;		/* 0x004 */
	struct NetCtlAdhocAuth *auth;			/* 0x008 */
	int state;					/* 0x00C */
	SceUChar8 context[0x100];			/* 0x010: the exchanges' state */
	SceUInt64 lastReceive;				/* 0x110 */
	SceNetSockaddrIn addr;				/* 0x118 */
	NetCtlAdhocPeerRecord record;			/* 0x128 */
	SceUChar8 request[0x1d0];			/* 0x228 */
	SceUChar8 response[0x170];			/* 0x3F8 */
	SceUChar8 confirm[0x130];			/* 0x568 */
	NetCtlAdhocAuthCallout resendTimer;		/* 0x698 */
	NetCtlAdhocAuthCallout keepaliveTimer;		/* 0x6B0 */
	NetCtlAdhocAuthCallout timeoutTimer;		/* 0x6C8 */
	SceUInt32 retries;				/* 0x6E0 */
	SceUInt32 keepaliveSent;			/* 0x6E4 */
	SceUInt32 keepaliveReceived;			/* 0x6E8 */
	SceUChar8 applicationId[4];			/* 0x6EC: TLV 1 */
	SceUChar8 swVersion[4];				/* 0x6F0: TLV 2 */
	SceUChar8 option[4];				/* 0x6F4: TLV 3 */
	int used;					/* 0x6F8 */
	SceUInt32 reserved;				/* 0x6FC */
} NetCtlAdhocAuthPeer;

/* the session */
typedef struct NetCtlAdhocAuth {
	int mode;					/* 0x000 */
	int recvSocket;					/* 0x004 */
	int sendSocket;					/* 0x008 */
	SceUID thread;					/* 0x00C */
	SceUInt32 flags;				/* 0x010 */
	SceUInt32 reserved;				/* 0x014 */
	NetCtlAdhocAuthPeer peers;			/* 0x018: the list's head */
	int peerCount;					/* 0x718 */
	SceNetInAddr ignoredAddress;			/* 0x71C */
	NetCtlAdhocPeerRecord record;			/* 0x720 */
	int maxPeers;					/* 0x820 */
	SceUInt32 reserved2[3];				/* 0x824 */
	int beaconCount;				/* 0x830 */
	SceUInt32 reserved3[5];				/* 0x834 */
	SceUInt32 beaconInterval;			/* 0x848 */
	SceUInt32 reserved4;				/* 0x84C */
	NetCtlAdhocAuthCallout beaconTimer;		/* 0x850 */
	NetCtlAdhocAuthMessage beaconMessage;		/* 0x868 */
	SceUInt32 resendInterval;			/* 0x888 */
	SceUInt32 maxRetries;				/* 0x88C */
	SceUInt32 keepaliveInterval;			/* 0x890 */
	SceUInt32 peerTimeout;				/* 0x894 */
	SceUChar8 applicationId[4];			/* 0x898 */
	SceUInt32 swVersion;				/* 0x89C */
	SceUChar8 option[4];				/* 0x8A0 */
	SceUChar8 tlv[64];				/* 0x8A4 */
	int tlvSize;					/* 0x8E4 */
	SceNetInAddr localAddress;			/* 0x8E8 */
	int stop;					/* 0x8EC */
} NetCtlAdhocAuth;

/* adhoc_auth.c */
extern int g_adhocAuthPeerPoolCount;
extern NetCtlAdhocAuthPeer *g_adhocAuthPeerPool;
extern NetCtlAdhocAuth *g_adhocAuth;
extern SceKernelLwMutexWork *g_adhocAuthStopLock;
extern int g_adhocAuthSwVersion;
extern int g_adhocAuthInitialized;
extern NetCtlAdhocAuth g_adhocAuthContext;

/* adhoc_auth_sys.c */
extern SceUID g_adhocAuthKeySema;
extern SceUID g_adhocAuthCalloutThread;
extern SceUID g_adhocAuthCalloutMutex;
extern SceUID g_adhocAuthCalloutCond;
extern int g_adhocAuthCalloutInitialized;
extern int g_adhocAuthCalloutStop;
extern SceUID g_adhocAuthMbx;
extern NetCtlAdhocAuthCallout *g_adhocAuthCalloutList;

/* adhoc_auth_peer.c */
extern SceUID g_adhocAuthLockSema;
extern SceUID g_adhocAuthLockOwner;
extern SceUChar8 g_adhocAuthSendBuffer[NETCTL_ADHOC_AUTH_PACKET_SIZE];
extern SceUChar8 g_adhocAuthRecvBuffer[NETCTL_ADHOC_AUTH_PACKET_SIZE];

int netCtlAdhocAuthInit(void);
int netCtlAdhocAuthTerm(void);
NetCtlAdhocAuthPeer *netCtlAdhocAuthPeerAlloc(void);
void netCtlAdhocAuthPeerFree(NetCtlAdhocAuthPeer *peer);
void netCtlAdhocAuthPeerPoolTerm(void);
void netCtlAdhocAuthDeleteCommId(void);
int netCtlAdhocAuthDisconnect(void);
int netCtlAdhocAuthGetPeerList(SceSize *peerInfoNum, SceNetCtlAdhocPeerInfo *peerInfo);
int netCtlAdhocAuthStopLockInit(void);
void netCtlAdhocAuthStopLockTerm(void);
int netCtlAdhocAuthKeyInit(void);
int netCtlAdhocAuthKeyTerm(void);
int netCtlAdhocAuthCalloutInit(SceSize stackSize, int priority);
void netCtlAdhocAuthCalloutTerm(void);
int netCtlAdhocAuthCalloutAdd(NetCtlAdhocAuthCallout *callout, SceUInt64 delay,
			      void (*func)(void *arg), void *arg);
int netCtlAdhocAuthCalloutRemove(NetCtlAdhocAuthCallout *callout, int *removed);
int netCtlAdhocAuthCalloutThread(SceSize argSize, void *argBlock);
int netCtlAdhocAuthThread(SceSize argSize, void *argBlock);
int netCtlAdhocAuthMbxCreate(const char *name);
int netCtlAdhocAuthMbxDelete(void);
int netCtlAdhocAuthMbxSend(const NetCtlAdhocAuthMessage *message, SceUInt32 waitMode);
int netCtlAdhocAuthMbxReceive(NetCtlAdhocAuthMessage *message);
int netCtlAdhocAuthMbxCancel(void);
void netCtlAdhocAuthPeerAdd(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer, SceNetSockaddrIn addr,
			    const NetCtlAdhocPeerRecord *record);
NetCtlAdhocAuthPeer *netCtlAdhocAuthPeerFind(NetCtlAdhocAuth *auth, SceNetInAddr addr);
void netCtlAdhocAuthPeerRemove(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer);
void netCtlAdhocAuthPeerSetState(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer, int state);
void netCtlAdhocAuthSetMode(NetCtlAdhocAuth *auth, int mode);
int netCtlAdhocAuthLockInit(void);
void netCtlAdhocAuthLockTerm(void);
int netCtlAdhocAuthLock(void);
void netCtlAdhocAuthUnlock(int locked);
int netCtlAdhocAuthBuildTlv(NetCtlAdhocAuth *auth, SceUChar8 *tlv, int *size);
void netCtlAdhocAuthParseTlv(NetCtlAdhocAuthPeer *peer, const char *tlv, int size);
int netCtlAdhocAuthSend(NetCtlAdhocAuth *auth, SceNetSockaddrIn to, SceUChar8 *packet,
			SceUChar8 type, const void *payload, const void *extra, int extraSize);
void netCtlAdhocAuthResend(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer);
void netCtlAdhocAuthBeacon(NetCtlAdhocAuth *auth);
void netCtlAdhocAuthKeepalive(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer);
void netCtlAdhocAuthTimeout(NetCtlAdhocAuth *auth, NetCtlAdhocAuthPeer *peer);
void netCtlAdhocAuthShutdown(NetCtlAdhocAuth *auth);
void netCtlAdhocAuthInput(NetCtlAdhocAuth *auth, const SceUChar8 *packet, int size,
			  SceNetSockaddrIn from);
int netCtlAdhocAuthReceive(NetCtlAdhocAuth *auth, NetCtlAdhocAuthMessage *message, int *received,
			   const SceUInt32 *timeout);
void netCtlAdhocAuthBeaconTimer(void *arg);
void netCtlAdhocAuthResendTimer(void *arg);
void netCtlAdhocAuthTimeoutTimer(void *arg);
void netCtlAdhocAuthKeepaliveTimer(void *arg);
int netCtlAdhocAuthGcInit(void);
int netCtlAdhocAuthGcTerm(void);
int netCtlAdhocAuthGcRequest(void *context, void *output);
int netCtlAdhocAuthGcResponse(void *context, const void *input, void *output);
int netCtlAdhocAuthGcConfirm(void *context, const void *input, void *output);
int netCtlAdhocAuthGcFinish(void *context, const void *input, void *output);
int netCtlAdhocAuthGcVerify(void *context, const void *input);
int netCtlAdhocAuthGcSign(void *context, void *mac, SceSize macSize, const void *data,
			  int send, int flags);
int netCtlAdhocAuthGcSignBroadcast(void *context, void *mac, SceSize macSize, const void *data,
				   int send, int flags);

#endif /* NETCTL_ADHOC_AUTH_H */
