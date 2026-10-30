/*
 * DTrace's DECI4P packets and the queues they wait in (Sony's
 * dtracep_packet_queue.c): each packet is a memory block of its own, from the
 * protocol header to the payload.
 */
#pragma once

#include <deci4p_dfmgr/deci4p.h>
#include <sys/list.h>

/* the largest packet: its memory block */
#define DTRACEP_PACKET_MAX	0x11000

/* every DTrace packet starts with this */
typedef struct DtracepHeader {
	SceDeci4pHeader hdr;
	SceUInt8 command;		/* 0x10: the command, or the reply's */
	SceUInt8 reserved0[3];
	int token;			/* 0x14: the client's */
	SceUInt32 reserved1[2];		/* not set by the packet code */
} DtracepHeader;

typedef struct DtracepPacket {
	list_node_t node;		/* in a DtracepPacketQueue */
	SceSize size;			/* 0x08: the packet's, rounded up to 8 */
	SceSize payloadSize;		/* 0x0c: after the header */
	SceUID memBlock;		/* 0x10 */
	void *data;			/* 0x14: the memory block's base */
	DtracepHeader *header;		/* 0x18: the same */
} DtracepPacket;

typedef struct DtracepPacketQueue {
	SceUID mutex;
	list_t packets;
} DtracepPacketQueue;

_Static_assert(sizeof(DtracepHeader) == 0x20, "DtracepHeader size");
_Static_assert(sizeof(DtracepPacket) == 0x1c, "DtracepPacket size");
_Static_assert(sizeof(DtracepPacketQueue) == 0x14, "DtracepPacketQueue size");

DtracepPacketQueue *dtracepCreatePacketQueue(void);
void dtracepPushPacketQueue(DtracepPacketQueue *queue, DtracepPacket *packet);
int dtracepIsEmptyPacketQueue(DtracepPacketQueue *queue);
DtracepPacket *dtracepPopPacketQueue(DtracepPacketQueue *queue);
void dtracepDrainPacketQueue(DtracepPacketQueue *queue);
void dtracepDestroyPacketQueue(DtracepPacketQueue *queue);
DtracepPacket *dtracepAllocPacket(void);
void dtracepInitPacket(DtracepPacket *packet, SceSize size, SceUInt8 command, int token);
void dtracepFreePacket(DtracepPacket *packet);
