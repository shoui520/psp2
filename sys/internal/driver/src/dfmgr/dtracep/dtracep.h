/*
 * DTrace's DECI4P protocols (Sony's dtracep.c): four channels, DTRACEP0 to
 * DTRACEP3, each with a thread that runs the host's commands.
 */
#pragma once

#include <deci4p_dfmgr/deci4p.h>
#include <threadmgr/threadmgr.h>
#include "dtracep_packet_queue.h"

#define DTRACEP_CHANNELS	4
#define DTRACEP_PROTOCOL	0x29000		/* DTRACEP0's; the others follow */

/* a channel's event flag bits */
#define DTRACEP_EVENT_RECV	0x1		/* a packet waits in recvQueue */
#define DTRACEP_EVENT_EXIT	0x2

typedef struct DtracepChannel {
	SceUID handle;			/* the protocol's, from sceDfMgrOpen */
	SceUID thread;
	SceUID eventFlag;
	int connected;			/* the host's link, from its status events */
	int suspended;			/* set while the system suspends */
	DtracepPacketQueue *recvQueue;	/* packets from the host, for the thread */
	DtracepPacketQueue *sendQueue;	/* replies waiting for the link */
	DtracepPacketQueue *sentQueue;	/* replies sent, freed once the link is idle */
} DtracepChannel;

_Static_assert(sizeof(DtracepChannel) == 0x20, "DtracepChannel size");

/* ask to send; while suspended, wait for the link to come back */
static inline int dtracepReqSend(DtracepChannel *ch)
{
	int res;

	while ((res = sceDfMgrReqSend(ch->handle)) == (int)DFMGR_ERROR_NOT_CONNECTED &&
	       ch->suspended && !ch->connected)
		sceKernelDelayThread(1000000);

	return res;
}

int dtracepInit(void);
int dtracepExit(void);
