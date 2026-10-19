#ifndef BBMC_QMI_FIFO_H
#define BBMC_QMI_FIFO_H

#include "qmi_backend.h"

typedef struct QmiPacketFifoEntry {
	SceUInt8 data[2076];
	SceUInt32 length;
	SceUInt32 channel;
	SceUInt32 state;
	struct QmiPacketFifoEntry *prev;
	struct QmiPacketFifoEntry *next;
} QmiPacketFifoEntry;

typedef struct QmiPacketFifo {
	QmiPacketFifoEntry entries[8];
	QmiPacketFifoEntry *producer;
	QmiPacketFifoEntry *consumer;
	SceUInt32 freeCount;
} QmiPacketFifo;

_Static_assert(sizeof(QmiPacketFifoEntry) == 2096, "QMI FIFO entry size");
_Static_assert(offsetof(QmiPacketFifoEntry, length) == 2076,
	"QMI FIFO entry length");
_Static_assert(offsetof(QmiPacketFifo, producer) == 16768,
	"QMI FIFO producer cursor");
_Static_assert(sizeof(QmiPacketFifo) == 16780, "QMI FIFO size");

extern QmiPacketFifo g_qmiPacketFifo;

/* qmi_fifo.c */
int qmiInitializePacketFifos(void);
int qmiEnqueueClientRequest(QmiClientInstance *instance);
int qmiClaimQueuedPacket(unsigned int *channel, void **packet, unsigned int *length);
int qmiRetryQueuedPacket(void);
int qmiReleaseQueuedPacket(void);
int qmiIsPacketFifoEmpty(void);

#endif
