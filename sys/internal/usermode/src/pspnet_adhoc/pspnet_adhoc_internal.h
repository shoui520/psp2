/*
 * pspnet_adhoc_internal.h: internal structures, constants, globals, and prototypes
 * for SceLibPspnetAdhoc (vs0:sys/external/pspnet_adhoc.suprx).
 */
#ifndef PSPNET_ADHOC_INTERNAL_H
#define PSPNET_ADHOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <net.h>
#include <libnetctl.h>
#include <pspnet_adhoc.h>

/* External library imports not yet in shared tree headers */
extern SceUInt32 SceLibKernel_9F793F84(void);
extern int SceLibKernel_EFD76235(void *context, void *output);
extern int SceLibKernel_1F4DF829(void *context, const void *input, void *output);
extern int SceLibKernel_C362ECD6(void *context, const void *input, void *output);
extern int SceLibKernel_6314CAA3(void *context, const void *input, void *output);
extern int SceLibKernel_3B595E1D(void *context, const void *input);
extern int SceLibKernel_A9002567(void *context, const void *data, int len, void *mac, int send);
extern int SceLibKernel_01E00CBF(void *context, void *data, int len, const void *mac,
				 int send, int verify);
extern int SceLibKernel_88E72157(const void *data, int len, void *mac, int send);
extern int SceLibKernel_E2984A54(void *context, void *data, int len, const void *mac,
				 int send, int verify);

extern int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
extern int SceNetCtl_326FEE96(int (*callback)(void));
extern int SceNetCtl_6D63C34B(SceNetEtherAddr *addr);
extern int SceNetCtl_CDEBA500(int code, void *info);
extern int SceNetInternal_0DFC23D6(SceUID pid, int policy, int flags);
extern void *SceNetInternal_A2C2BC5F(SceSize size, int flags);
extern void SceNetInternal_E62A2FAA(void *ptr);
extern int sceAppMgrSendNotificationRequest(void *request, int flags);
extern int sceRegMgrSystemParamGetStr(int id, char *buf, SceSize size);

/* Internal adhoc authentication error codes (0x804106xx) */
#define PSPNET_ADHOC_AUTH_ERROR_NOT_FOUND		-2143222027	/* 0x804106f5 */
#define PSPNET_ADHOC_AUTH_ERROR_ALREADY_EXISTS		-2143222026	/* 0x804106f6 */
#define PSPNET_ADHOC_AUTH_ERROR_FAILED			-2143222024	/* 0x804106f8 */
#define PSPNET_ADHOC_AUTH_ERROR_CALLOUT_RUNNING		-2143222022	/* 0x804106fa */
#define PSPNET_ADHOC_AUTH_ERROR_CALLOUT_QUEUED		-2143222020	/* 0x804106fc */

/* Raw Ethernet socket protocol and frame framing constants */
#define PSPNET_ADHOC_SOCK_RAW_ETHER			9
#define PSPNET_ADHOC_ETHER_PROTO			0x88c8
#define PSPNET_ADHOC_ETHER_HDR_LEN			16
#define PSPNET_ADHOC_AUTH_MAC_LEN			32
#define PSPNET_ADHOC_FRAME_HDR_LEN			48
#define PSPNET_ADHOC_PKT_HDR_LEN			60
#define PSPNET_ADHOC_AUTH_FRAME_SIZE			0x800
#define PSPNET_ADHOC_AUTH_VERSION			1
#define PSPNET_ADHOC_AUTH_HDR_SIZE			4
#define PSPNET_ADHOC_AUTH_ENC_HDR_SIZE			20
#define PSPNET_ADHOC_AUTH_KEY_BUCKETS			16
#define PSPNET_ADHOC_REG_SSID_PREFIX_ID			0x134c03

/* Auth packet types */
#define PSPNET_ADHOC_AUTH_PKT_BEACON			1
#define PSPNET_ADHOC_AUTH_PKT_HELLO			2
#define PSPNET_ADHOC_AUTH_PKT_REQUEST			3
#define PSPNET_ADHOC_AUTH_PKT_RESPONSE			4
#define PSPNET_ADHOC_AUTH_PKT_CONFIRM			5
#define PSPNET_ADHOC_AUTH_PKT_FINISH			6
#define PSPNET_ADHOC_AUTH_PKT_BYE			7
#define PSPNET_ADHOC_AUTH_PKT_KEEPALIVE			8

/* Auth peer states */
#define PSPNET_ADHOC_AUTH_PEER_NEW			0
#define PSPNET_ADHOC_AUTH_PEER_REQUESTED		1
#define PSPNET_ADHOC_AUTH_PEER_CONFIRMED		2
#define PSPNET_ADHOC_AUTH_PEER_RESPONDED		3
#define PSPNET_ADHOC_AUTH_PEER_AUTHENTICATED		4

/* Auth mailbox message types */
#define PSPNET_ADHOC_AUTH_MSG_RESEND			1
#define PSPNET_ADHOC_AUTH_MSG_BEACON			3
#define PSPNET_ADHOC_AUTH_MSG_KEEPALIVE			4
#define PSPNET_ADHOC_AUTH_MSG_TIMEOUT			5
#define PSPNET_ADHOC_AUTH_MSG_STOP			6

/* Auth context flags */
#define PSPNET_ADHOC_AUTH_FLAG_BEACON_QUEUED		0x0002

/* Protocol numbers inside adhoc data frames (offset 48) */
#define PSPNET_ADHOC_PROTO_PDP				1
#define PSPNET_ADHOC_PROTO_PTP				2

/* Socket types */
#define PSPNET_ADHOC_SOCK_PDP				1
#define PSPNET_ADHOC_SOCK_PTP				2

/* Socket state flags */
#define PSPNET_ADHOC_SF_DELETED				0x0001
#define PSPNET_ADHOC_SF_LISTEN				0x1000
#define PSPNET_ADHOC_SF_CONNECTING			0x2000
#define PSPNET_ADHOC_SF_CONNECTED			0x4000
#define PSPNET_ADHOC_SF_DISCONNECTED			0x8000

/* Socket buffer wait flags */
#define PSPNET_ADHOC_SB_WAIT				0x0001
#define PSPNET_ADHOC_SB_SEL				0x0002

/* PTP packet opcodes */
#define PSPNET_ADHOC_PTP_OP_SYN				1
#define PSPNET_ADHOC_PTP_OP_SYN_ACK			2
#define PSPNET_ADHOC_PTP_OP_ACK_SYN			3
#define PSPNET_ADHOC_PTP_OP_DATA			4
#define PSPNET_ADHOC_PTP_OP_ACK				5
#define PSPNET_ADHOC_PTP_OP_NACK			6
#define PSPNET_ADHOC_PTP_OP_RST				7

/* Mbuf flags and sizes */
#define PSPNET_ADHOC_MSIZE				256
#define PSPNET_ADHOC_MCLBYTES				2048
#define PSPNET_ADHOC_MLEN				236
#define PSPNET_ADHOC_MHLEN				208
#define PSPNET_ADHOC_MT_DATA				1
#define PSPNET_ADHOC_M_EXT				0x0001
#define PSPNET_ADHOC_M_PKTHDR				0x0002
#define PSPNET_ADHOC_M_CLUSTER				0x0008
#define PSPNET_ADHOC_M_BCAST				0x0100
#define PSPNET_ADHOC_M_MCAST				0x0200
#define PSPNET_ADHOC_M_SENT				0x8000

/* Callout flags */
#define PSPNET_ADHOC_CALLOUT_ACTIVE			0x0001
#define PSPNET_ADHOC_CALLOUT_PENDING			0x0002

/* Sleep queue flags and buckets */
#define PSPNET_ADHOC_SLPQUE_BUCKETS			128
#define PSPNET_ADHOC_SLP_TIMEDOUT			0x0001
#define PSPNET_ADHOC_SLP_ABORTED			0x0002

/* Kernel-style errno return codes used internally */
#define PSPNET_ADHOC_EIO				5
#define PSPNET_ADHOC_EPIPE				32

/* Forward declarations */
struct AdhocAuth;
struct AdhocSocket;
struct AdhocPdpPcb;
struct AdhocPtpPcb;

/* Callout timer node for the adhoc auth callout thread (0x18 bytes) */
typedef struct AdhocAuthCallout {
	struct AdhocAuthCallout *next;			/* 0x00 */
	void (*func)(void *arg);			/* 0x04 */
	SceUInt64 time;					/* 0x08 */
	void *arg;					/* 0x10 */
	SceUInt32 pad;					/* 0x14 */
} AdhocAuthCallout;

/* Message sent to the adhoc auth mailbox (0x18 bytes) */
typedef struct AdhocAuthMessage {
	SceUInt32 header[2];				/* 0x00 */
	int type;					/* 0x08 */
	SceNetEtherAddr addr;				/* 0x0C */
	SceUChar8 pad[6];				/* 0x12 */
} AdhocAuthMessage;

/* Parameters passed to start adhoc authentication (0x14 bytes) */
typedef struct AdhocAuthParam {
	SceUInt32 beaconInterval;			/* 0x00 */
	SceUInt32 resendInterval;			/* 0x04 */
	SceUInt32 maxRetries;				/* 0x08 */
	SceUInt32 keepaliveInterval;			/* 0x0C */
	SceUInt32 peerTimeout;				/* 0x10 */
} AdhocAuthParam;

/* Notification request passed to SceAppMgrUser (0x40 bytes) */
typedef struct AdhocAuthNotification {
	SceUInt32 header;				/* 0x00 */
	char text[60];					/* 0x04 */
} AdhocAuthNotification;

/* Authenticated peer key hash-table entry (0xD8 bytes) */
typedef struct AdhocAuthKeyEntry {
	struct AdhocAuthKeyEntry *next;			/* 0x00 */
	struct AdhocAuthKeyEntry *prev;			/* 0x04 */
	SceUChar8 context[0xC0];			/* 0x08 */
	SceNetEtherAddr addr;				/* 0xC8 */
	SceUChar8 pad[2];				/* 0xCE */
	SceUInt64 lastReceive;				/* 0xD0 */
} AdhocAuthKeyEntry;

/* Adhoc authentication peer (0x508 bytes) */
typedef struct AdhocAuthPeer {
	struct AdhocAuthPeer *next;			/* 0x000 */
	struct AdhocAuthPeer *prev;			/* 0x004 */
	struct AdhocAuth *auth;				/* 0x008 */
	int state;					/* 0x00C */
	AdhocAuthKeyEntry key;				/* 0x010 */
	SceNetEtherAddr addr;				/* 0x0E8 */
	SceNetAdhocctlNickname nickname;		/* 0x0EE */
	SceUChar8 request[0x120];			/* 0x16E */
	SceUChar8 response[0x110];			/* 0x28E */
	SceUChar8 confirm[0x100];			/* 0x39E */
	SceUChar8 padConfirm[2];			/* 0x49E */
	AdhocAuthCallout resendTimer;			/* 0x4A0 */
	AdhocAuthCallout keepaliveTimer;		/* 0x4B8 */
	AdhocAuthCallout timeoutTimer;			/* 0x4D0 */
	SceUInt32 retries;				/* 0x4E8 */
	SceUInt32 keepaliveSent;			/* 0x4EC */
	SceUInt32 keepaliveReceived;			/* 0x4F0 */
	SceUChar8 applicationId[4];			/* 0x4F4 */
	SceUChar8 swVersion[4];				/* 0x4F8 */
	SceUChar8 option[4];				/* 0x4FC */
	int used;					/* 0x500 */
	SceUInt32 padTail;				/* 0x504 */
} AdhocAuthPeer;

/* Adhoc authentication session context (0x670 bytes) */
typedef struct AdhocAuth {
	int mode;					/* 0x000 */
	int recvSocket;					/* 0x004 */
	int sendSocket;					/* 0x008 */
	SceUID thread;					/* 0x00C */
	SceUInt32 flags;				/* 0x010 */
	SceUInt32 padHeader;				/* 0x014 */
	AdhocAuthPeer peers;				/* 0x018 */
	int peerCount;					/* 0x520 */
	SceUInt32 padPeerCount;				/* 0x524 */
	SceNetAdhocctlNickname nickname;		/* 0x528 */
	int maxPeers;					/* 0x5A8 */
	SceUInt32 padBeforeBeaconCount[3];		/* 0x5AC */
	int beaconCount;				/* 0x5B8 */
	SceUInt32 padBeforeInterval[3];			/* 0x5BC */
	SceUInt32 beaconInterval;			/* 0x5C8 */
	SceUInt32 padInterval;				/* 0x5CC */
	AdhocAuthCallout beaconTimer;			/* 0x5D0 */
	AdhocAuthMessage beaconMessage;			/* 0x5E8 */
	SceUInt32 resendInterval;			/* 0x600 */
	SceUInt32 maxRetries;				/* 0x604 */
	SceUInt32 keepaliveInterval;			/* 0x608 */
	SceUInt32 peerTimeout;				/* 0x60C */
	SceUInt32 applicationId;			/* 0x610 */
	SceUInt32 swVersion;				/* 0x614 */
	SceUInt32 option;				/* 0x618 */
	SceUChar8 tlv[64];				/* 0x61C */
	int tlvSize;					/* 0x65C */
	SceNetEtherAddr localAddr;			/* 0x660 */
	SceUChar8 padAddr[2];				/* 0x666 */
	int stop;					/* 0x668 */
	SceUInt32 padTail;				/* 0x66C */
} AdhocAuth;

/* BSD-style mbuf packet header (0x1C bytes, at offset 0x14 of AdhocMbuf) */
typedef struct AdhocMbufPktHdr {
	void *rcvif;					/* 0x14 */
	int len;					/* 0x18 */
	struct AdhocMbuf *aux;				/* 0x1C */
	SceInt16 rssi;					/* 0x20 */
	SceInt16 channel;				/* 0x22 */
	SceUInt32 padTime;				/* 0x24 */
	SceUInt32 sentTimeLo;				/* 0x28 */
	SceUInt32 sentTimeHi;				/* 0x2C */
} AdhocMbufPktHdr;

/* BSD-style external cluster descriptor (0x1C bytes, at offset 0x30 of AdhocMbuf) */
typedef struct AdhocMbufExt {
	char *extBuf;					/* 0x30 */
	void (*extFree)(void);				/* 0x34 */
	void *extArg;					/* 0x38 */
	unsigned int extSize;				/* 0x3C */
	void *extType;					/* 0x40 */
	struct AdhocMbuf *extNextRef;			/* 0x44 */
	struct AdhocMbuf *extPrevRef;			/* 0x48 */
} AdhocMbufExt;

/* BSD-style mbuf (0x100 = 256 bytes) */
typedef struct AdhocMbuf {
	struct AdhocMbuf *next;				/* 0x00 */
	struct AdhocMbuf *nextPkt;			/* 0x04 */
	char *data;					/* 0x08 */
	int len;					/* 0x0C */
	SceShort16 type;				/* 0x10 */
	SceShort16 flags;				/* 0x12 */
	union {
		struct {
			AdhocMbufPktHdr pktHdr;		/* 0x14 */
			union {
				AdhocMbufExt ext;	/* 0x30 */
				char pktDat[PSPNET_ADHOC_MHLEN]; /* 0x30 */
			} body;
		} mh;
		char dat[PSPNET_ADHOC_MLEN];		/* 0x14 */
	} storage;
} AdhocMbuf;

/* BSD-style callout descriptor (0x20 bytes) */
typedef struct AdhocCallout {
	struct AdhocCallout *next;			/* 0x00 */
	struct AdhocCallout **prev;			/* 0x04 */
	SceUInt32 padLink[2];				/* 0x08 */
	void (*func)(void *arg);			/* 0x10 */
	void *arg;					/* 0x14 */
	int time;					/* 0x18 */
	int flags;					/* 0x1C */
} AdhocCallout;

/* Per-thread sleep and reference state (0x58 bytes) */
typedef struct AdhocThreadInfo {
	struct AdhocThreadInfo *next;			/* 0x00 */
	struct AdhocThreadInfo **prev;			/* 0x04 */
	SceUID threadId;				/* 0x08 */
	SceShort16 refCount;				/* 0x0C */
	SceShort16 padRef;				/* 0x0E */
	int lastError;					/* 0x10 */
	SceUInt32 padBeforeSlp;				/* 0x14 */
	struct AdhocThreadInfo *slpNext;		/* 0x18 */
	struct AdhocThreadInfo **slpPrev;		/* 0x1C */
	void *wchan;					/* 0x20 */
	AdhocCallout timeout;				/* 0x24 */
	int slpFlags;					/* 0x44 */
	SceUID sema;					/* 0x48 */
	SceUInt32 padAfterSema[2];			/* 0x4C */
	struct AdhocThreadInfo *sockNext;		/* 0x54 */
} AdhocThreadInfo;

/* Sleep queue hash bucket (0x08 bytes) */
typedef struct AdhocSlpqueHead {
	AdhocThreadInfo *first;				/* 0x00 */
	AdhocThreadInfo **last;				/* 0x04 */
} AdhocSlpqueHead;

/* Adhoc socket descriptor (0x28 bytes) */
typedef struct AdhocSocket {
	struct AdhocSocket *next;			/* 0x00 */
	struct AdhocSocket **prev;			/* 0x04 */
	int id;						/* 0x08 */
	int flags;					/* 0x0C */
	SceUChar8 type;					/* 0x10 */
	SceUChar8 padType[3];				/* 0x11 */
	void *pcb;					/* 0x14 */
	void *sendWchan;				/* 0x18 */
	void *recvWchan;				/* 0x1C */
	void *pollWchan;				/* 0x20 */
	AdhocThreadInfo *waiters;			/* 0x24 */
} AdhocSocket;

/* Socket buffer queue (0x18 bytes) */
typedef struct AdhocSockBuf {
	AdhocMbuf *head;				/* 0x00 */
	AdhocMbuf *tail;				/* 0x04 */
	unsigned int cc;				/* 0x08 */
	unsigned int hiwat;				/* 0x0C */
	unsigned int lowat;				/* 0x10 */
	int flags;					/* 0x14 */
} AdhocSockBuf;

/* PDP fragment reassembly entry (0x34 bytes) */
typedef struct AdhocPdpReasm {
	struct AdhocPdpReasm *next;			/* 0x00 */
	struct AdhocPdpReasm **prev;			/* 0x04 */
	SceUShort16 totalLen;				/* 0x08 */
	SceUShort16 padLen;				/* 0x0A */
	AdhocMbuf *mbuf;				/* 0x0C */
	AdhocCallout timer;				/* 0x10 */
	struct AdhocPdpPcb *pcb;			/* 0x30 */
} AdhocPdpReasm;

/* PDP protocol control block (0x34 bytes) */
typedef struct AdhocPdpPcb {
	struct AdhocPdpPcb *next;			/* 0x00 */
	struct AdhocPdpPcb **prev;			/* 0x04 */
	SceNetEtherAddr laddr;				/* 0x08 */
	SceUShort16 lport;				/* 0x0E */
	AdhocSockBuf rcv;				/* 0x10 */
	AdhocSocket *sock;				/* 0x28 */
	AdhocPdpReasm *reasmHead;			/* 0x2C */
	AdhocPdpReasm **reasmTail;			/* 0x30 */
} AdhocPdpPcb;

/* PTP protocol control block (0xDC bytes) */
typedef struct AdhocPtpPcb {
	struct AdhocPtpPcb *next;			/* 0x00 */
	struct AdhocPtpPcb **prev;			/* 0x04 */
	struct AdhocPtpPcb *qNext;			/* 0x08 */
	struct AdhocPtpPcb **qPrev;			/* 0x0C */
	struct AdhocPtpPcb *qHead;			/* 0x10 */
	struct AdhocPtpPcb **qTail;			/* 0x14 */
	SceNetEtherAddr laddr;				/* 0x18 */
	SceNetEtherAddr paddr;				/* 0x1E */
	SceUShort16 lport;				/* 0x24 */
	SceUShort16 pport;				/* 0x26 */
	AdhocSockBuf snd;				/* 0x28 */
	AdhocSockBuf rcv;				/* 0x40 */
	AdhocSocket *sock;				/* 0x58 */
	int state;					/* 0x5C */
	int flags;					/* 0x60 */
	unsigned int mss;				/* 0x64 */
	SceUInt32 iss;					/* 0x68 */
	SceUInt32 sndQueued;				/* 0x6C */
	SceUInt32 sndUna;				/* 0x70 */
	SceUInt32 sndNxt;				/* 0x74 */
	SceUInt32 rcvNxt;				/* 0x78 */
	unsigned int sndWnd;				/* 0x7C */
	SceUInt32 padSeq;				/* 0x80 */
	AdhocCallout rexmtTimer;			/* 0x84 */
	AdhocCallout persistTimer;			/* 0xA4 */
	unsigned int rexmtInt;				/* 0xC4 */
	int rexmtCnt;					/* 0xC8 */
	int rexmtRemain;				/* 0xCC */
	int qLen;					/* 0xD0 */
	int qLimit;					/* 0xD4 */
	struct AdhocPtpPcb *parent;			/* 0xD8 */
} AdhocPtpPcb;

/* Input packet queue (0x14 bytes) */
typedef struct AdhocInputQueue {
	AdhocMbuf *head;				/* 0x00 */
	AdhocMbuf *tail;				/* 0x04 */
	int len;					/* 0x08 */
	int maxLen;					/* 0x0C */
	int drops;					/* 0x10 */
} AdhocInputQueue;

/* Poll entry pairing a socket and caller's descriptor */
typedef struct AdhocPollEntry {
	AdhocSocket *sock;
	struct SceNetAdhocPollSd *sd;
} AdhocPollEntry;

/* Optional RSSI/channel metadata returned by internal PDP recv */
typedef struct AdhocPdpRxInfo {
	int rssi;
	int channel;
} AdhocPdpRxInfo;

/* adhocctl.c globals and functions */
extern int g_adhocctlInitialized;
extern SceNetAdhocctlGroupName g_adhocctlGroupName;
extern SceNetAdhocctlAdhocId g_adhocctlAdhocId;

void adhocctlGetAdhocIdCopy(SceNetAdhocctlAdhocId *adhocId);
int ScePspnetAdhoc_5C83C5C8(const SceNetAdhocctlGroupName *groupName);
void adhocctlGetGroupNameCopy(SceNetAdhocctlGroupName *groupName);
int ScePspnetAdhoc_F87F9518(const char *groupName, char *ssid, SceSize size);

/* adhoc_auth.c globals and functions */
extern int g_adhocAuthPeerPoolCount;
extern AdhocAuthPeer *g_adhocAuthPeerPool;
extern AdhocAuth *g_adhocAuth;
extern SceKernelLwMutexWork *g_adhocAuthStopLock;
extern AdhocAuth g_adhocAuthContext;

int adhocAuthEncryptFrame(AdhocMbuf *m);
int adhocAuthDecryptFrame(AdhocMbuf *m);
AdhocAuthPeer *adhocAuthPeerAlloc(void);
void adhocAuthPeerFree(AdhocAuthPeer *peer);
void adhocAuthPeerPoolTerm(void);
int adhocAuthStopLockInit(void);
int ScePspnetAdhoc_8938A4D0(void);
void adhocAuthStopLockTerm(void);
int ScePspnetAdhoc_A3EC45B9(int priority, SceSize stackSize, const AdhocAuthParam *param,
			    int maxPeers, const SceNetAdhocctlNickname *nickname);
int adhocAuthGetPeerList(int *buflen, void *buf);
int adhocAuthGetPeerInfo(const SceNetEtherAddr *addr, SceNetAdhocctlPeerInfo *peerInfo);
int adhocAuthGetAddrByName(const SceNetAdhocctlNickname *nickname, int *buflen, void *buf);
int adhocAuthGetNameByAddr(const SceNetEtherAddr *addr, SceNetAdhocctlNickname *nickname);
int adhocAuthGetParameter(SceNetAdhocctlParameter *parameter);
int adhocAuthGetLocalAddr(SceNetEtherAddr *addr);

/* adhoc_auth_sys.c globals and functions */
extern SceUID g_adhocAuthKeySema;
extern SceUID g_adhocAuthCalloutThread;
extern SceUID g_adhocAuthCalloutMutex;
extern SceUID g_adhocAuthCalloutCond;
extern int g_adhocAuthCalloutInitialized;
extern int g_adhocAuthCalloutStop;
extern SceUID g_adhocAuthMbx;
extern AdhocAuthKeyEntry g_adhocAuthKeyTable[PSPNET_ADHOC_AUTH_KEY_BUCKETS];
extern AdhocAuthCallout *g_adhocAuthCalloutList;

int adhocAuthKeyInit(void);
int adhocAuthKeyTerm(void);
AdhocAuthKeyEntry *adhocAuthKeyFind(const SceUChar8 *addr);
int adhocAuthKeyAdd(AdhocAuthKeyEntry *entry);
int adhocAuthKeyRemove(AdhocAuthKeyEntry *entry);
int adhocAuthCalloutInit(SceSize stackSize, int priority);
void adhocAuthCalloutTerm(void);
int adhocAuthCalloutAdd(AdhocAuthCallout *callout, SceUInt64 delay,
			void (*func)(void *arg), void *arg);
int adhocAuthCalloutRemove(AdhocAuthCallout *callout, int *removed);
int adhocAuthCalloutThread(SceSize argSize, void *argBlock);
int adhocAuthThread(SceSize argSize, void *argBlock);
int adhocAuthMbxCreate(const char *name);
int adhocAuthMbxDelete(void);
int adhocAuthMbxSend(const AdhocAuthMessage *message, SceUInt32 waitMode);
int adhocAuthMbxReceive(AdhocAuthMessage *message);
int adhocAuthMbxCancel(void);

/* adhoc_auth_peer.c globals and functions */
extern SceUID g_adhocAuthLockSema;
extern SceUID g_adhocAuthLockOwner;
extern SceUChar8 g_adhocAuthSendBuffer[PSPNET_ADHOC_AUTH_FRAME_SIZE];
extern SceUChar8 g_adhocAuthRecvBuffer[PSPNET_ADHOC_AUTH_FRAME_SIZE];

void adhocAuthPeerAdd(AdhocAuth *auth, AdhocAuthPeer *peer, const SceUChar8 *addr,
		      const SceNetAdhocctlNickname *nickname);
AdhocAuthPeer *adhocAuthPeerFind(AdhocAuth *auth, const SceUChar8 *addr);
void adhocAuthPeerRemove(AdhocAuth *auth, AdhocAuthPeer *peer);
void adhocAuthPeerSetState(AdhocAuth *auth, AdhocAuthPeer *peer, int state);
void adhocAuthSetMode(AdhocAuth *auth, int mode);
int adhocAuthLockInit(void);
void adhocAuthLockTerm(void);
int adhocAuthLock(void);
void adhocAuthUnlock(int locked);
int adhocAuthBuildTlv(AdhocAuth *auth, SceUChar8 *tlv, int *size);
void adhocAuthParseTlv(AdhocAuthPeer *peer, const char *tlv, int size);
int adhocAuthSend(AdhocAuth *auth, const SceUChar8 *to, SceUChar8 *frame,
		  SceUChar8 type, const void *payload, const void *extra, SceSize extraSize);
void adhocAuthResend(AdhocAuth *auth, AdhocAuthPeer *peer);
void adhocAuthBeacon(AdhocAuth *auth);
void adhocAuthKeepalive(AdhocAuth *auth, AdhocAuthPeer *peer);
void adhocAuthTimeout(AdhocAuth *auth, AdhocAuthPeer *peer);
void adhocAuthShutdown(AdhocAuth *auth);
void adhocAuthHandleRequest(AdhocAuth *auth, AdhocAuthPeer *peer, const SceUChar8 *packet,
			    const SceUChar8 *from, const char *tlv, int tlvSize);
void adhocAuthInput(AdhocAuth *auth, const SceUChar8 *packet, int size, const SceUChar8 *from);
int adhocAuthReceive(AdhocAuth *auth, AdhocAuthMessage *message, int *received,
		     const SceUInt32 *timeout);
void adhocAuthBeaconTimer(void *arg);
void adhocAuthResendTimer(void *arg);
void adhocAuthTimeoutTimer(void *arg);
void adhocAuthKeepaliveTimer(void *arg);
int adhocAuthGcInit(void);
int adhocAuthGcTerm(void);
int adhocAuthGcRequest(void *context, void *output);
int adhocAuthGcResponse(void *context, const void *input, void *output);
int adhocAuthGcConfirm(void *context, const void *input, void *output);
int adhocAuthGcFinish(void *context, const void *input, void *output);
int adhocAuthGcVerify(void *context, const void *input);
int adhocAuthGcSign(void *context, const void *data, int len, void *mac, int send);
int adhocAuthGcDecrypt(void *context, void *data, int len, const void *mac, int send, int verify);
int adhocAuthGcSignBroadcast(const void *data, int len, void *mac, int send);
int adhocAuthGcDecryptBroadcast(void *context, void *data, int len, const void *mac,
				int send, int verify);

/* adhoc.c globals and functions */
extern const SceUChar8 g_adhocEtherType[2];
extern const SceUChar8 g_adhocDataSubType[2];
extern const SceUChar8 g_adhocBroadcastAddr[6];
extern int g_adhocRawSocket;
extern int g_adhocInitialized;
extern SceNetEtherAddr g_adhocZeroAddr;
extern int g_adhocSendPollFlags;
extern SceNetEtherAddr g_adhocLocalEtherAddr;
extern AdhocInputQueue g_adhocInputQueue;
extern int g_adhocLastSocketId;
extern AdhocSocket *g_adhocSocketHead;
extern AdhocSocket **g_adhocSocketTail;
extern int g_adhocPollChannel;

int adhocEnqueueInput(void *ifp, AdhocMbuf *m);
void adhocProcessInput(void);
void adhocOutput(void *ifp, AdhocMbuf *m);
int adhocSocketTableInit(void);
void adhocSocketFree(AdhocSocket *sock);
void adhocSocketTableTerm(void);
AdhocSocket *adhocSocketFind(SceUChar8 type, int id);
int adhocSocketAllocId(void);
int adhocSocketCreate(SceUChar8 type, AdhocSocket **outSock);
int adhocSocketAnyBusy(void);
void adhocSocketAlertWakeup(AdhocSocket *sock, int alertFlags);

/* adhoc_pdp.c functions */
int adhocPdpSendInternal(AdhocSocket *sock, const SceNetEtherAddr *daddr, SceUShort16 dport,
			 const void *data, int len);
int adhocPdpRecvInternal(int id, SceNetEtherAddr *saddr, SceUShort16 *sport,
			 void *buf, int *len, unsigned int timeout, int flag,
			 AdhocPdpRxInfo *rxInfo);
int adhocPdpInit(void);
void adhocPdpDetach(AdhocSocket *sock);
void adhocPdpFlush(AdhocPdpPcb *pcb);
void adhocPdpTerm(void);
int adhocPdpAttach(AdhocSocket *sock);
int adhocPdpBind(AdhocPdpPcb *pcb, const SceNetEtherAddr *saddr, SceUShort16 sport);
int adhocPdpSetBufSize(AdhocPdpPcb *pcb, unsigned int bufsize);
void adhocPdpInput(AdhocMbuf *m);
int adhocPdpPoll(AdhocSocket *sock, int events);
void adhocPdpReasmTimeout(void *arg);

/* adhoc_ptp.c functions */
int adhocPtpSendInternal(AdhocSocket *sock, const void *data, int *len,
			 unsigned int timeout, int flag);
void adhocPtpInput(AdhocMbuf *m);
void adhocPtpSendPacket(const SceNetEtherAddr *saddr, SceUShort16 sport,
			const SceNetEtherAddr *daddr, SceUShort16 dport,
			int opcode, SceUInt32 seq, AdhocMbuf *payload);
void adhocPtpSendControl(AdhocPtpPcb *pcb, SceUChar8 opcode);
void adhocPtpOutput(AdhocPtpPcb *pcb, int isRexmt);
void adhocPtpRexmtTimeout(void *arg);
void adhocPtpKeepaliveTimeout(void *arg);
int adhocPtpInit(void);
void adhocPtpDetach(AdhocSocket *sock);
void adhocPtpFlushBuffers(AdhocPtpPcb *pcb);
void adhocPtpTerm(void);
AdhocPtpPcb *adhocPtpLookup(const SceNetEtherAddr *laddr, SceUShort16 lport,
			    const SceNetEtherAddr *paddr, SceUShort16 pport);
int adhocPtpAttach(AdhocSocket *sock);
int adhocPtpBind(AdhocPtpPcb *pcb, const SceNetEtherAddr *saddr, SceUShort16 sport,
		 const SceNetEtherAddr *daddr, SceUShort16 dport);
int adhocPtpSetBufSize(AdhocPtpPcb *pcb, unsigned int sndSize, unsigned int rcvSize);
void adhocPtpSetRexmt(AdhocPtpPcb *pcb, unsigned int rexmtInt, int rexmtCnt);
int adhocPtpConnectStart(AdhocPtpPcb *pcb, const SceNetEtherAddr *daddr, SceUShort16 dport);
int adhocPtpListenStart(AdhocPtpPcb *pcb, int backlog);
AdhocPtpPcb *adhocPtpAcceptChild(AdhocPtpPcb *listenPcb, const SceNetEtherAddr *paddr,
				 SceUShort16 pport, SceUInt32 seq);
void adhocPtpConnected(AdhocPtpPcb *pcb);
void adhocPtpDisconnect(AdhocPtpPcb *pcb);
void adhocPtpCloseChildren(AdhocPtpPcb *pcb);
int adhocPtpPoll(AdhocSocket *sock, int events);

/* callout.c functions */
void adhocCalloutInit(AdhocCallout *c);
void adhocCalloutNoop(void);
void adhocCalloutStop(AdhocCallout *c);
void adhocCalloutReset(AdhocCallout *c, int ticks, void (*func)(void *arg), void *arg);

/* mbuf.c functions */
void adhocMbufFreeChain(AdhocMbuf *m);
AdhocMbuf *adhocMbufFree(AdhocMbuf *m);
void adhocMbufCopyData(const AdhocMbuf *m, int off, int len, void *cp);
void adhocMbufCat(AdhocMbuf *m, AdhocMbuf *n);
void adhocMbufAdj(AdhocMbuf *mp, int reqLen);
AdhocMbuf *adhocMbufPullup(AdhocMbuf *n, int len);
void adhocMbufCopyBack(AdhocMbuf *head, int off, int len, const void *cp);

/* slpque.c functions */
int adhocSlpqueInit(void);
void adhocSlpqueTerm(void);
int adhocSleep(void *wchan, int timo);
void adhocSleepTimeout(void *arg);
void adhocWakeup(void *wchan);
void adhocNotifyAlert(AdhocSocket *sock, int alertFlags);
int adhocSetAlertCallback(void (*callback)(AdhocSocket *sock, int alertFlags));
int adhocClearAlertCallback(void);
void adhocSlpqueNoop(void);

/* spl.c functions */
int adhocSplInit(void);
void adhocSplTerm(void);
int adhocSplNet(void);
int adhocSplX(int s);
void *adhocMalloc(SceSize size);
void adhocFree(void *ptr);

/* M521 GFSR PRNG functions */
void adhocRandSeed(SceUInt32 seed);
SceUInt32 adhocRandNext(void);

/* thread.c functions */
int adhocThreadCleanup(SceUID threadId);
int adhocThreadModuleStart(void);
int adhocThreadModuleStop(void);
int adhocThreadInit(void);
AdhocThreadInfo *adhocThreadFind(SceUID threadId);
AdhocThreadInfo *adhocThreadEnter(int *errOut);
int adhocThreadLeave(AdhocThreadInfo *ti, int err);
void adhocThreadTerm(void);
SceInt32 adhocThreadEventHandler(SceInt32 evId, SceUID thId, SceInt32 evOpt, void *common);
AdhocMbuf *adhocMbufDevGet(const SceUChar8 *buf, SceSize totlen);

#endif /* PSPNET_ADHOC_INTERNAL_H */
