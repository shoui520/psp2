/*
 * SceDeci4pDfMgr internals: the DECI4P link manager of the development kit.
 *
 * Two links carry DECI4P packets to the host: SDIO (link 0) and the DevKit USB
 * port (link 1). Every protocol module opens a protocol slot with
 * sceDfMgrOpen; slots 0 and 1 belong to DCMP, the manager protocol of each
 * link (Sony's dfmgr.c, dcmp.c, sdio.c and usb.c).
 */

#pragma once

#include <fw.h>
#include <string.h>
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <sysmem/debug.h>
/* deci4p_dfmgr/deci4p.h declares sceDfMgrClose void; it returns an error code */
#define sceDfMgrClose sceDfMgrClose_deci4p_h
#include <deci4p_dfmgr/deci4p.h>
#include <threadmgr/threadmgr.h>
#undef sceDfMgrClose
int sceDfMgrClose(SceUID handle);

/*
 * Sony built dfmgr with the compiler builtins enabled: constant clears and
 * copies are expanded inline where the compiler can. The build forces
 * -fno-builtin, so ask for them.
 */
#define memset(dst, c, n)	__builtin_memset(dst, c, n)
#define memcpy(dst, src, n)	__builtin_memcpy(dst, src, n)
#define strncpy(dst, src, n)	__builtin_strncpy(dst, src, n)

#define DFMGR_PROTOCOL_COUNT	32
#define DFMGR_LINK_COUNT	2

#define DFMGR_PROTOCOL_DCMP	0x100
#define DFMGR_PACKET_MAX	0x11000

/* DfMgrProtocolCB.flags */
#define DFMGR_PD_OPEN		0x0001
#define DFMGR_PD_STARTED	0x0002
#define DFMGR_PD_SEND_REQUESTED	0x0010
#define DFMGR_PD_SENDING	0x0020
#define DFMGR_PD_HEADER_SENT	0x0040
#define DFMGR_PD_SENT		0x0080
#define DFMGR_PD_HEADER_PENDING	0x0100
#define DFMGR_PD_RECEIVING	0x0200
#define DFMGR_PD_RECEIVED	0x0400
#define DFMGR_PD_SPACE_ERROR	0x1000
#define DFMGR_PD_SPACE_BUSY	0x2000
#define DFMGR_PD_SPACE_DONE	0x4000

/* DfMgrLinkCB.flags */
#define DFMGR_LINK_READY	0x01
#define DFMGR_LINK_ATTACHED	0x02
#define DFMGR_LINK_CONNECTED	0x04
#define DFMGR_LINK_SUSPENDED	0x08
#define DFMGR_LINK_RECV_ERROR	0x40

/* s_dfmgrFlags */
#define DFMGR_INITIALIZED	0x01
#define DFMGR_SUSPENDING	0x02
#define DFMGR_SUSPENDED		0x04
#define DFMGR_POWER_OFF		0x08

#define DFMGR_REQ_QUEUE		64

typedef struct DfMgrLinkInfo {
	const char *name;
	SceUInt32 id;			/* index in s_dfmgrLinkTable */
} DfMgrLinkInfo;

struct DfMgrLinkCB;

typedef int (*DfMgrLinkIo)(void *buf, SceSize size);
typedef void (*DfMgrLinkCallback)(struct DfMgrLinkCB *link);

/* one link to the host */
typedef struct DfMgrLinkCB {
	DfMgrLinkIo send;
	DfMgrLinkIo recv;
	const DfMgrLinkInfo *info;
	volatile SceUInt32 flags;		/* DFMGR_LINK_* */
	SceKernelSpinlock lock;
	SceUID sendMutex;
	SceUID recvMutex;
	SceUID sendEvf;			/* bit 0: a protocol wants to send */
	SceUID eventSema;		/* held around protocol handlers */
	SceSize sendRemaining;		/* of the packet being sent */
	SceSize recvRemaining;		/* of the packet being received */
	SceUID req[DFMGR_REQ_QUEUE];	/* handles waiting to send */
	SceKernelSpinlock reqLock;
	int reqWrite;
	int reqRead;
	int reqCount;
	SceDeci4pHeader header;		/* of the packet being received */
} DfMgrLinkCB;

/* one protocol slot */
typedef struct DfMgrProtocolCB {
	SceDfMgrHandler handler;
	SceDfMgrProtocol info;
	void *arg;
	SceUID handle;
	volatile SceUInt32 flags;	/* DFMGR_PD_* */
	SceKernelSpinlock lock;
	SceUInt32 linkIndex;
	DfMgrLinkCB *link;
} DfMgrProtocolCB;

typedef int (*DfMgrLinkInit)(DfMgrLinkCB *link, DfMgrLinkCallback sendCallback,
			     DfMgrLinkCallback recvCallback, SceUID *pSendEvf);
typedef int (*DfMgrLinkTerm)(DfMgrLinkCB *link);

typedef struct DfMgrLinkOps {
	DfMgrLinkInit init;
	DfMgrLinkTerm term;
	SceUInt32 pd;			/* the link's DCMP protocol slot */
} DfMgrLinkOps;

#define DCMP_TYPE_CONTROL	0	/* from the host: DCMP_CONTROL_* */
#define DCMP_TYPE_ECHO		1
#define DCMP_TYPE_STATUS	2	/* DCMP_STATUS_* */
#define DCMP_TYPE_ERROR		3	/* the rejected packet follows */

#define DCMP_CONTROL_CONNECT		0
#define DCMP_CONTROL_DISCONNECT		1
#define DCMP_CONTROL_CONNECT2		2
#define DCMP_CONTROL_DISCONNECT2	3
#define DCMP_CONTROL_START		4
#define DCMP_CONTROL_STOP		5
#define DCMP_CONTROL_DISCARD		6

#define DCMP_STATUS_CONNECTION		0
#define DCMP_STATUS_PROTOCOL		1
#define DCMP_STATUS_ALIAS		2
#define DCMP_STATUS_SPACE		3
#define DCMP_STATUS_POWER		4
#define DCMP_STATUS_UPDATE_PROGRESS	8
#define DCMP_STATUS_UPDATE_FINISHED	9
#define DCMP_STATUS_INSTALL_PROGRESS	0xa
#define DCMP_STATUS_INSTALL_FINISHED	0xb
#define DCMP_STATUS_UNINSTALL_PROGRESS	0xc
#define DCMP_STATUS_UNINSTALL_FINISHED	0xd
#define DCMP_STATUS_LOADP		0xf
#define DCMP_STATUS_SD_INSTALL_PROGRESS	0x10
#define DCMP_STATUS_SD_INSTALL_FINISHED	0x11
#define DCMP_STATUS_SD_FORMAT_PROGRESS	0x12
#define DCMP_STATUS_SD_FORMAT_FINISHED	0x13

#define DCMP_PORT_HOST		0x4000
#define DCMP_PORT_POWER		0x6000
#define DCMP_PORT_TARGET	0x8000
#define DCMP_PORT_SDIO		0xa000
#define DCMP_PORT_USB		0xa100

typedef struct DcmpEcho {
	DcmpHeader dcmp;
	SceUInt16 id;
	SceUInt16 seq;
	SceUInt8 data[32];
} DcmpEcho;

typedef struct DcmpStatusConnection {
	DcmpHeader dcmp;
	SceUInt8 connected;
	SceUInt8 pad[3];
} DcmpStatusConnection;

typedef struct DcmpStatusProtocol {
	DcmpHeader dcmp;
	SceUInt32 protocol;
	SceUInt32 version;		/* SceDfMgrProtocol.version */
	SceUInt32 subVersion;		/* SceDfMgrProtocol.subVersion */
	SceUInt16 port;			/* 0xa000 SDIO, 0xa100 USB */
	SceUInt16 reserved;
	SceUInt8 status;
	SceUInt8 pad[3];
} DcmpStatusProtocol;

/* handed to the protocol handlers with SCE_DFMGR_EVENT_STATUS */
typedef union DcmpStatus {
	DcmpHeader dcmp;
	DcmpStatusConnection connection;
	DcmpStatusProtocol protocol;
} DcmpStatus;

typedef struct DcmpStatusAlias {
	DcmpHeader dcmp;
	SceUInt32 protocol;
	SceUInt32 alias;		/* SceDfMgrProtocol.alias */
} DcmpStatusAlias;

typedef struct DcmpStatusSpace {
	DcmpHeader dcmp;
	SceUInt32 protocol;
} DcmpStatusSpace;

typedef struct DcmpStatusPower {
	DcmpHeader dcmp;
	SceUInt8 kind;
	SceUInt8 state;
	SceUInt8 arg;			/* kind 0x80 / 0x81, state 3 */
	SceUInt8 arg2;			/* kind 0, state 2 */
} DcmpStatusPower;

#define DCMP_PROGRESS_MSG_MAX	0xe0

/* progress of the system software tools (update, install, ...) */
typedef struct DcmpProgress {
	DcmpHeader dcmp;
	union {
		struct {
			SceUInt8 kind;
			SceUInt8 percent;
			SceUInt16 flag;
		};
		SceUInt32 status;	/* finished: DCMP result code */
	};
	SceUInt32 value;		/* started: total, finished: the result */
	SceSize msgLen;			/* with the terminating NUL */
	char msg[DCMP_PROGRESS_MSG_MAX];
} DcmpProgress;

/* an ongoing report: DcmpProgress with an empty message */
typedef struct DcmpProgressOngoing {
	DcmpHeader dcmp;
	SceUInt8 kind;
	SceUInt8 percent;
	SceUInt16 flag;
	SceUInt32 value;
	SceSize msgLen;
	char msg[4];
} DcmpProgressOngoing;

/* a finished report waiting for sceDfMgrDcmpReqSendForLoadp */
typedef struct DcmpPending {
	SceInt32 result;
	char msg[64];
	SceSize len;			/* with the terminating NUL */
	SceUInt8 type;
	SceUInt8 code;
	SceUInt16 pad;
} DcmpPending;

#define DCMP_SEND_QUEUE		48
#define DCMP_SEND_BUF_SIZE	0x100

/* the DCMP state of one link */
typedef struct DcmpCB {
	SceUID handle;
	SceUID memBlock;
	DcmpHeader *recvBuf;
	SceKernelSpinlock lock;		/* the send queue */
	int write;
	int read;
	int count;
	int errPending;			/* the queued error carries errBuf */
	void *buf[DCMP_SEND_QUEUE];
	SceSize size[DCMP_SEND_QUEUE];
	void *errBuf;			/* the rejected packet */
	SceSize errSize;
	DcmpPending pending;
	DfMgrLinkCB *link;
} DcmpCB;

_Static_assert(sizeof(DfMgrLinkCB) == 0x14c, "DfMgrLinkCB");
_Static_assert(sizeof(DfMgrProtocolCB) == 0x54, "DfMgrProtocolCB");
_Static_assert(sizeof(DcmpEcho) == 0x38, "DcmpEcho");
_Static_assert(sizeof(DcmpStatus) == 0x28, "DcmpStatus");
_Static_assert(sizeof(DcmpProgress) == 0x100, "DcmpProgress");
_Static_assert(sizeof(DcmpPending) == 0x4c, "DcmpPending");
_Static_assert(sizeof(DcmpCB) == 0x1f8, "DcmpCB");

/* dfmgr.c */
extern DfMgrProtocolCB s_dfmgrPd[DFMGR_PROTOCOL_COUNT];
extern DfMgrLinkCB s_dfmgrLink[DFMGR_LINK_COUNT];

int dfmgrSetProtocolStatus(DfMgrProtocolCB *pd, SceUInt32 mask, int on);
SceUInt32 dfmgrTestLinkStatus(DfMgrLinkCB *link, SceUInt32 mask);
int dfmgrSetLinkStatus(DfMgrLinkCB *link, SceUInt32 mask, int on);
void dfmgrConnectLink(DfMgrLinkCB *link, int notify);
void dfmgrDisconnectLink(DfMgrLinkCB *link, int notify);
int dfmgrStartProtocol(SceUInt32 protocol);
int dfmgrStopProtocol(SceUInt32 protocol);

/* dcmp.c */
int dcmpInit(DfMgrProtocolCB *pd, SceUInt32 linkIndex, DfMgrLinkCB *link);
void dcmpSendError(DfMgrProtocolCB *pd, SceUInt32 linkIndex, SceUInt8 code,
		   const SceDeci4pHeader *hdr);
void dcmpSendStatusProtocol(SceUInt32 linkIndex, SceUInt32 status,
			    const SceDfMgrProtocol *protocol);
void dcmpSendStatusProtocolAlias(SceUInt32 linkIndex, const SceDfMgrProtocol *protocol);
void dcmpSendStatusPower(SceUInt32 linkIndex, SceUInt32 kind, SceUInt32 state, SceUInt32 arg);
void dcmpSendStatusSpace(SceUInt32 linkIndex, SceUInt32 protocol);
int dcmpIsSendQueueEmpty(SceUInt32 linkIndex);
int dcmpMakeStatusConnection(SceUInt32 connected, DcmpStatusConnection *status);
int dcmpMakeStatusProtocol(SceUInt32 enabled, const SceDfMgrProtocol *protocol,
			   DcmpStatus *status);

void dcmpSendStatusUpdateProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					 const char *msg, SceSize len, SceUInt32 flag);
void dcmpSendStatusUpdateProgressOngoing(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 percent);
void dcmpSendStatusUpdateFinished(SceUInt32 linkIndex, SceUInt32 result, const char *msg,
				  SceSize len);
void dcmpSendStatusInstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					  const char *msg, SceSize len, SceUInt32 flag);
void dcmpSendStatusInstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 percent);
void dcmpSetStatusInstallFinished(SceUInt32 linkIndex, SceUInt32 result, const char *msg,
				  SceSize len);
void dcmpSendStatusUninstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind, SceUInt32 total,
					    const char *msg, SceSize len, SceUInt32 flag);
void dcmpSendStatusUninstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
					    SceUInt32 percent);
void dcmpSetStatusUninstallFinished(SceUInt32 linkIndex, SceUInt32 result, const char *msg,
				    SceSize len);
void dcmpSendStatusSaveDataInstallProgressStarted(SceUInt32 linkIndex, SceInt32 kind,
						  SceUInt32 total, const char *msg, SceSize len,
						  SceUInt32 flag);
void dcmpSendStatusSaveDataInstallProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
						  SceUInt32 percent);
void dcmpSetStatusSaveDataInstallFinished(SceUInt32 linkIndex, SceUInt32 result,
					  const char *msg, SceSize len);
void dcmpSendStatusSaveDataFormatProgressStarted(SceUInt32 linkIndex, SceInt32 kind,
						 SceUInt32 total, const char *msg, SceSize len,
						 SceUInt32 flag);
void dcmpSendStatusSaveDataFormatProgressOngoing(SceUInt32 linkIndex, SceInt32 kind,
						 SceUInt32 percent);
void dcmpSetStatusSaveDataFormatFinished(SceUInt32 linkIndex, SceUInt32 result,
					 const char *msg, SceSize len);

/* sdio.c, usb.c */
int sdioInit(DfMgrLinkCB *link, DfMgrLinkCallback sendCallback,
	     DfMgrLinkCallback recvCallback, SceUID *pSendEvf);
int sdioTerm(DfMgrLinkCB *link);
int usbInit(DfMgrLinkCB *link, DfMgrLinkCallback sendCallback,
	    DfMgrLinkCallback recvCallback, SceUID *pSendEvf);
int usbTerm(DfMgrLinkCB *link);

/* exports */
void sceDfMgrDcmpReqSendForLoadp(SceUInt32 result);
int sceDfMgrInit(void);

/* imports without a declaration elsewhere */
int SceSysrootForDriver_C5EAF5F7(void (*handler)(int event));	/* power state notifications */
int SceSysrootForDriver_579CC758(const void *callbacks);	/* update / install progress */
int SceProcessmgrForDriver_C715591F(void);			/* a process refuses suspend */
int SceDbgUsbForDriver_A6BFCC0A(SceUID evf);
int SceDbgUsbForDriver_08CF16D1(void);
int SceDbgUsbForDriver_40C65DB2(const void *buf, SceSize size);
int _sceDbgUsbPullData(void *buf, SceSize size);
