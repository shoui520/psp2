/*
 * SceDeci4pSDfCtl internals: the SDfCtl manager (sdfctl.c), its two protocol
 * descriptors SDCMP (sdcmp.c, connection / power control) and STSMP (stsmp.c,
 * target status / boot parameters), and the host interface it drives: the debug
 * UART 1 (uart.c).
 *
 * Packets are DECI4P-style: a 16-byte header (24-bit size including the header,
 * route, protocol), then the payload.
 */

#ifndef SDFCTL_H
#define SDFCTL_H

#include <psp2kern/types.h>
#include "sce/deci4p.h"

#if FW_DEBUG
#define SDFCTL_PRINTF(_line, _fmt, ...)	SCE_PRINTF_WITH_INFO(0xf, _line, _fmt, ##__VA_ARGS__)
#else
#define SDFCTL_PRINTF(_line, _fmt, ...)	do { } while (0)
#endif

/* error codes (names unknown) */
#define SDFCTL_ERROR_INVALID_ARG	0x80090101
#define SDFCTL_ERROR_INVALID_SOCK	0x80090102
#define SDFCTL_ERROR_INVALID_BUFFER	0x80090105
#define SDFCTL_ERROR_INVALID_SIZE	0x80090106
#define SDFCTL_ERROR_BUSY		0x80090107
#define SDFCTL_ERROR_QUEUE_EMPTY	0x80090108
#define SDFCTL_ERROR_NO_ROUTE		0x8009010a
#define SDFCTL_ERROR_QUEUE_FULL		0x8009010b
#define SDFCTL_ERROR_NO_HOST_IF		0x8009010d
#define SDFCTL_ERROR_INVALID_PROTOCOL	0x8009010e

/* packet header */
#define SDFCTL_HDR_SIZE			0x10
#define SDFCTL_ROUTE_HOST		0x8000	/* SDfCtl's own status packets */
#define SDFCTL_ROUTE_LOCAL		0x6000	/* always routed, even when not connected */
#define SDFCTL_ROUTE_TARGET		0x4000
#define SDFCTL_PROTOCOL_MGR		0x10000100	/* SDCMP */

#define SDFCTL_PORT			0xa300	/* the header's source port */

/* protocol event handler: SDFCTL_EVENT_*, then an event argument and the context */
typedef void (*SDfCtlHandler)(int event, SceUIntPtr arg, void *ctx);

#define SDFCTL_EVENT_RECV		1	/* arg: packet size */
#define SDFCTL_EVENT_RECV_DONE		2
#define SDFCTL_EVENT_SEND		3	/* the host interface can take a packet */
#define SDFCTL_EVENT_SEND_DONE		4
#define SDFCTL_EVENT_STATUS		5	/* arg: SDCMP status packet */
#define SDFCTL_EVENT_6			6

/* protocol descriptor control block */
typedef struct SDfCtlPd {
	SDfCtlHandler handler;
	SceDfMgrProtocol desc;			/* destination: route; flags bit 1: alias */
	void *ctx;
	SceUID sock;
	SceUInt32 flags;			/* SDFCTL_PD_* */
} SDfCtlPd;

#define SDFCTL_PD_REGISTERED		0x1
#define SDFCTL_PD_STARTED		0x2
#define SDFCTL_PD_SEND_REQ		0x4

/* host interface */
struct SDfCtlIf;
typedef int (*SDfCtlIfOp)(struct SDfCtlIf *ifp);
typedef int (*SDfCtlIfXfer)(struct SDfCtlIf *ifp, void *buf, SceSize size);

typedef struct SDfCtlIfOps {
	SDfCtlIfOp recvStart;			/* 0x4c: no PD takes the packet: drop it */
	SDfCtlIfXfer recv;
	SDfCtlIfOp recvDone;
	SDfCtlIfOp sendStart;
	SDfCtlIfXfer send;
	SDfCtlIfOp sendDone;
	SDfCtlIfOp poll;
	SDfCtlIfOp recvBlock;
	SDfCtlIfOp recvUnblock;
	SDfCtlIfOp sendBlock;
	SDfCtlIfOp sendUnblock;
	SDfCtlIfOp reserved0;			/* never called */
	void (*reserved1)(struct SDfCtlIf *ifp);	/* never called */
	SDfCtlIfOp stop;			/* 0x80: suspend */
	SDfCtlIfOp start;			/* 0x84: resume */
} SDfCtlIfOps;

typedef struct SDfCtlIf {
	SceUInt32 state;			/* interface driver's own (uart.c) */
	SceUInt32 intr;
	SceUInt32 flags;			/* SDFCTL_IF_* */
	SDfCtlPd *sendPd;
	SceSize sendSize;
	SceSize sendDone;
	SceDeci4pHeader sendHdr;
	SceUInt32 recvFlags;			/* SDFCTL_IF_IN_HANDLER */
	SDfCtlPd *recvPd;
	SceSize recvSize;
	SceSize recvDone;
	SceDeci4pHeader recvHdr;
	const char *name;
	SDfCtlIfOps ops;
} SDfCtlIf;

#define SDFCTL_IF_UP			0x1
#define SDFCTL_IF_IN_HANDLER		0x4
#define SDFCTL_IF_SENDING		0x8
#define SDFCTL_IF_SEND_STARTED		0x10

/* SDfCtl status */
#define SDFCTL_STATUS_POWER		0x1
#define SDFCTL_STATUS_CONNECTED		0x2

/* sdfctl.c */
#if FW_DEBUG
SDfCtlPd *_getPdCB(SceUInt32 id);
#else
static inline SDfCtlPd *_getPdCB(SceUInt32 id);
#endif
void sdfctlMemset(void *p, int c, SceSize n);
int sceSDfCtlExRecv(SceUID sock, void *buf, SceSize size);
int sceSDfCtlExReqSend(SceUID sock);
int sdfctlReqSend(SceUID sock);
int sceSDfCtlExSend(SceUID sock, void *buf, SceSize size);
void sdfctlPoll(void);
SceUInt32 sdfctlTestStatus(SceUInt32 mask);
int sdfctlSetStatus(SceUInt32 mask, int on);
int sdfctlSetProtocolStatus(SDfCtlPd *pd, SceUInt32 mask, int on);
int sdfctlRegisterIf(int id, SDfCtlIf *ifp);
int sceSDfCtlIf2MgrEvent(SDfCtlIf *ifp, int event, SceUIntPtr arg, SceUInt32 protocol, SceUInt16 route);
void sdfctlConnect(void);
void sdfctlDisconnect(void);
int sdfctlStartProtocol(SceUInt32 protocol);
int sdfctlStopProtocol(SceUInt32 protocol);

/* sdcmp.c */
void sdfctlMemcpy(void *dst, const void *src, SceSize n);
SceSize sdfctlGetSize(const void *hdr);
void sdfctlSetSize(void *hdr, SceSize size);
int sdcmpInit(SDfCtlPd *pd);
void sdcmpSendStatusProtocol(SceUInt32 status, const SceDfMgrProtocol *desc);
void sdcmpSendStatusProtocolAlias(const SceDfMgrProtocol *desc);
void sdcmpSendStatusPower(int kind, int state, int arg2);
int sdcmpMakeStatusConnection(SceUInt32 status, void *pkt);
int sdcmpMakeStatusProtocol(SceUInt32 status, const SceDfMgrProtocol *desc, void *pkt);

/* stsmp.c */
int stsmpInit(SDfCtlPd *pd);

/* uart.c */
int uartInit(void);

#endif /* SDFCTL_H */
