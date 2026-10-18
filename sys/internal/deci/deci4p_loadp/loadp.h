/*
 * SceDeci4pLoadp: the "LOADP" DECI4P protocol, through which the development
 * host starts and kills processes and titles, loads, starts, stops and
 * unloads modules, and runs the system's tool apps (package installer and
 * uninstaller, DS4 pairing, save data installer and formatter). It also tells
 * the host about every process and module event, through the sysroot process
 * handler.
 *
 * Commands arrive in the SceDeci4pDfMgr handler and run one at a time on the
 * module's thread; replies and notifications are queued and sent from the
 * handler.
 */

#pragma once

#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>

#define LOADP_RECV_BUF_SIZE		0x11000
#define LOADP_SEND_BUF_SIZE		(LOADP_RECV_BUF_SIZE + 0x3f)	/* aligned to 64 inside */
#define LOADP_SEND_QUEUE_MAX		0x40

/* every LOADP packet: the DECI4P header, then the command */
typedef struct LoadpHeader {
	SceDeci4pHeader hdr;
	SceUInt8 arg;				/* request: flags; reply: the request's command */
	SceUInt8 cmd;				/* LOADP_CMD_*, replies cmd + 1, notifications 0x80+ */
	SceUInt16 reserved;
} LoadpHeader;

/* a reply: the header, then the result */
typedef struct LoadpReply {
	LoadpHeader head;
	SceUInt32 result;			/* LOADP_RESULT_* or a converted error */
} LoadpReply;

/* a packet queued for sending; the data is 64-aligned inside mem */
typedef struct LoadpSendBuf {
	SceSize size;
	void *mem;
	void *packet;
	struct LoadpSendBuf *next;
} LoadpSendBuf;

/* the protocol's state; sceDfMgrOpen's context */
typedef struct LoadpContext {
	int handle;				/* sceDfMgrOpen */
	SceKernelSpinlock lock;
	SceUInt32 flags;			/* LOADP_FLAG_* */
	SceUID thread;
	SceUID evf;				/* 1: a command is pending */
	SceSize packetSize;			/* of the pending command */
	SceSize argSize;			/* packetSize - sizeof(LoadpHeader) */
	LoadpHeader *recvBuf;
	LoadpSendBuf *sendHead;			/* queued for sending */
	SceUInt32 sendCount;
	LoadpSendBuf *doneList;			/* sent, freed when the queue drains */
	LoadpHeader *pending;			/* the command the thread runs */
	SceUID heap;
	SceUID sema;				/* signalled with a launched app's result */
	SceUID mutex;				/* the send queue */
	SceUID appPid;				/* the tool app running, -1: none */
	int appCmd;				/* the command that launched it */
} LoadpContext;

#define LOADP_FLAG_ENABLED		2	/* the host enabled the protocol */
