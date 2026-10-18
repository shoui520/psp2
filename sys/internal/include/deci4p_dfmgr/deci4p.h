/*
 * DECI4P: the development-kit debug protocol carried by SceDeci4pDfMgr
 * (os0:kd/deci4p_dfmgr.skprx), shared by the deci4p_* protocol modules.
 */

#pragma once

#include <scetypes.h>

#define SCE_DECI4P_VERSION		0x40
#define SCE_DECI4P_PORT_TARGET		0x8000

/* packet header; the 24-bit size is set / read with SceDeci4pDfMgrForDebugger_6D26CC56 / _CACAB5F9 */
typedef struct SceDeci4pHeader {
	SceUInt8 size[3];
	SceUInt8 version;			/* SCE_DECI4P_VERSION */
	SceUInt16 src;
	SceUInt16 dst;
	SceUInt32 protocol;
	union {
		SceUInt32 reserved;		/* sent as 0 */
		struct {
			SceUInt8 reserved0;
			SceUInt8 reserved1;
			SceUInt16 reserved2;
		};
	};
} SceDeci4pHeader;

/* DCMP, the manager protocol (SDCMP on SceDeci4pSDfMgr): every packet starts with this */
typedef struct DcmpHeader {
	SceDeci4pHeader hdr;
	SceUInt8 type;				/* DCMP_TYPE_* */
	SceUInt8 code;
	SceUInt16 reserved;			/* sent as 0 */
} DcmpHeader;

/* protocol registration (sceDfMgrOpen) */
typedef struct SceDfMgrProtocol {
	SceSize size;				/* sizeof(SceDfMgrProtocol) */
	SceUInt32 protocol;
	SceUInt32 alias;			/* another protocol number answering for it, or 0 */
	SceUInt32 version;			/* 0x01020001: 1.2.0.1 */
	SceUInt32 subVersion;			/* version-shaped; sent to the host with version */
	SceUInt16 destination;
	SceUInt16 flags;
	char name[32];
} SceDfMgrProtocol;

/* SceDfMgrProtocol.flags */
#define DFMGR_PROTOCOL_ENABLE		0x0001	/* enabled as soon as its link connects */
#define DFMGR_PROTOCOL_ALIAS		0x0002	/* announce .alias as well */
#define DFMGR_PROTOCOL_SDIO		0x0100	/* on the SDIO link */
#define DFMGR_PROTOCOL_USB		0x0200	/* on the USB link */

/* handler events */
#define SCE_DFMGR_EVENT_RECV		1	/* arg: packet size, read it with sceDfMgrRecv */
#define SCE_DFMGR_EVENT_REQ_DONE	2
#define SCE_DFMGR_EVENT_SEND		3	/* ready: send with sceDfMgrSend */
#define SCE_DFMGR_EVENT_SENT		4
#define SCE_DFMGR_EVENT_STATUS		5	/* arg: SceDfMgrStatus */

/* SCE_DFMGR_EVENT_STATUS packet (dcmpMakeStatusConnection / dcmpMakeStatusProtocol) */
typedef struct SceDfMgrStatus {
	SceDeci4pHeader hdr;
	SceUInt8 code;				/* DCMP type: status */
	SceUInt8 kind;				/* 0: connection, 1: protocol */
	SceUInt16 reserved0;
	union {
		struct {
			SceUInt8 connected;	/* kind 0 */
			SceUInt8 padding0[3];
		};
		SceUInt32 protocol;		/* kind 1 */
	};
	SceUInt32 version;			/* kind 1: SceDfMgrProtocol.version */
	SceUInt32 subVersion;
	SceUInt16 port;				/* 0xa000 SDIO, 0xa100 USB */
	SceUInt16 reserved1;
	SceUInt8 enabled;			/* kind 1 */
	SceUInt8 padding1[3];
} SceDfMgrStatus;

_Static_assert(sizeof(SceDfMgrStatus) == 0x28, "SceDfMgrStatus size");

typedef int (*SceDfMgrHandler)(int event, void *arg, void *ctx);

int sceDfMgrOpen(SceDfMgrHandler handler, const SceDfMgrProtocol *protocol, void *ctx);
void sceDfMgrClose(int handle);
int sceDfMgrReqSend(int handle);
int sceDfMgrSend(int handle, const void *buf, SceSize size);
int sceDfMgrRecv(int handle, void *buf, SceSize size);
int sceDfMgrSendSpace(int handle);
/* the result of a tool app SceDeci4pLoadp launched for SceDeci4pDfMgr's DCMP */
void sceDfMgrDcmpReqSendForLoadp(SceUInt32 result);

/* names unknown (NID) */
void SceDeci4pDfMgrForDebugger_6D26CC56(SceDeci4pHeader *hdr, SceSize size);	/* set size */
SceSize SceDeci4pDfMgrForDebugger_CACAB5F9(const SceDeci4pHeader *hdr);		/* get size */

/* sceDfMgr* errors (names ours: SceDeci4pDfMgr has no records for them) */
#define DFMGR_ERROR_INVALID_ARGUMENT	0x80080001
#define DFMGR_ERROR_NOT_FOUND		0x80080002
#define DFMGR_ERROR_ALREADY_OPENED	0x80080003
#define DFMGR_ERROR_NO_SLOT		0x80080004
#define DFMGR_ERROR_NULL_BUFFER		0x80080005
#define DFMGR_ERROR_BUFFER_TOO_SMALL	0x80080006
#define DFMGR_ERROR_BUSY		0x80080007
#define DFMGR_ERROR_NOTHING_TO_SEND	0x80080008
#define DFMGR_ERROR_NOT_CONNECTED	0x8008000a
#define DFMGR_ERROR_QUEUE_FULL		0x8008000b
#define DFMGR_ERROR_UNKNOWN_PROTOCOL	0x8008000e

/* sceKernelDeci4p* errors (SDK libdeci4p reference) */
#define SCE_KERNEL_ERROR_DECI4P_UNKNOWN			0x80080800
#define SCE_KERNEL_ERROR_DECI4P_ALREADYUSE_PROTOCOL	0x80080801
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SID		0x80080802
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_CBID		0x80080803
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_ADDR		0x80080804
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_SIZE		0x80080805
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PROTOCOL	0x80080806
#define SCE_KERNEL_ERROR_DECI4P_NO_MEMORY		0x80080807
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_RESERVED	0x80080808
#define SCE_KERNEL_ERROR_DECI4P_TOOMANY_PROTOCOL	0x80080809
#define SCE_KERNEL_ERROR_DECI4P_NO_CONNECT		0x8008080a
#define SCE_KERNEL_ERROR_DECI4P_NO_PROTO		0x8008080b
#define SCE_KERNEL_ERROR_DECI4P_TOOSMALL_BUFFER		0x8008080c
#define SCE_KERNEL_ERROR_DECI4P_ILLEGAL_PARAM		0x8008080d
#define SCE_KERNEL_ERROR_DECI4P_INTERRUPTED		0x8008080e
#define SCE_KERNEL_ERROR_DECI4P_HOST_TOOMANY_PROCESS	0x80080811

/* sceKernelDeci4pCreateHostProcess(AndWait) parameters (SDK libdeci4p reference) */
typedef struct SceKernelDeci4pCreateHostProcessParam {
	SceUInt32 flags;
	SceUInt32 reserved0;
	const char *pathName;
	const char *cmdLine;
	const char *workDir;
	SceUInt32 reserved1[8];
} SceKernelDeci4pCreateHostProcessParam;

/*
 * A console packet: the TTYP protocols of SceDeci4pTtyp (tty0: .. tty7:) and
 * SceDeci4pSTtyp (the secure kernel's kprintf) carry this after the DECI4P
 * header, then len data bytes.
 */
typedef struct TtypHeader {
	SceDeci4pHeader hdr;
	SceUInt16 port;				/* TTYP_PORT_* */
	SceUInt8 type;				/* TTYP_TYPE_* */
	SceUInt8 reserved;
	SceUID pid;
	SceUID tid;
	SceUInt32 tag;				/* echoed in the acknowledgement */
	SceUInt32 time[2];			/* when it was sent (word aligned) */
	SceSize len;				/* data bytes */
} TtypHeader;

_Static_assert(sizeof(TtypHeader) == 0x2c, "TtypHeader size");

#define TTYP_PORT_PROCESS	0x1000
#define TTYP_PORT_KERNEL	SCE_DECI4P_PORT_TARGET

#define TTYP_TYPE_DATA		0
#define TTYP_TYPE_ACK		1
#define TTYP_TYPE_CLOSE		2		/* a process ended */
