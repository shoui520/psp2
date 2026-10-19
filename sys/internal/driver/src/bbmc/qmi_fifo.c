/* QMI transport packet queue. */
#include <string.h>
#include "qmi_fifo.h"

QmiPacketFifo g_qmiPacketFifo;

/* name unknown: reset the eight-entry circular transport queue. */
int qmiInitializePacketFifos(void)
{
	QmiPacketFifoEntry *entries = g_qmiPacketFifo.entries;

	entries[0].prev = &entries[7];
	entries[6].next = &entries[7];
	entries[6].prev = &entries[5];
	entries[2].next = &entries[3];
	entries[4].prev = &entries[3];
	entries[4].next = &entries[5];
	entries[6].state = 0;
	entries[0].state = 0;
	entries[0].next = &entries[1];
	entries[4].state = 0;
	entries[2].state = 0;
	entries[2].prev = &entries[1];
	entries[5].state = 0;
	entries[5].next = &entries[6];
	entries[7].next = &entries[0];
	entries[7].state = 0;
	entries[7].prev = &entries[6];
	entries[5].prev = &entries[4];
	entries[3].next = &entries[4];
	entries[3].state = 0;
	entries[3].prev = &entries[2];
	entries[1].state = 0;
	entries[1].prev = &entries[0];
	entries[1].next = &entries[2];
	g_qmiPacketFifo.producer = &entries[0];
	g_qmiPacketFifo.consumer = &entries[0];
	g_qmiPacketFifo.freeCount = 8;
	return 0;
}

/* name unknown: append client framing and payload to the producer's entry. */
int qmiEnqueueClientRequest(QmiClientInstance *instance)
{
	BbcmdProtocolClient *client = instance->client;
	unsigned int channel = client->channel;
	QmiPacketFifoEntry *entry;
	unsigned int headerLength;
	unsigned int length;
	unsigned int freeCount;
	int locked;

	if (g_qmiPacketFifo.freeCount == 0)
		return (int)0x80330203;
	locked = qmiLockBackend(2);
	entry = g_qmiPacketFifo.producer;
	entry->channel = channel;
	memcpy(entry->data, &instance->transmit.wire, instance->transmit.length);
	headerLength = instance->transmit.length;
	entry->length = headerLength;
	memcpy(entry->data + headerLength, client->request.data, client->request.length);
	length = client->request.length + entry->length;
	freeCount = g_qmiPacketFifo.freeCount;
	entry->state = 1;
	entry->length = length;
	g_qmiPacketFifo.producer = entry->next;
	g_qmiPacketFifo.freeCount = freeCount - 1;
	qmiUnlockBackend(2, locked);
	return qmiWakePacketTransmitter();
}

/* name unknown: claim the consumer's queued packet without advancing it. */
int qmiClaimQueuedPacket(unsigned int *channel, void **packet,
	unsigned int *length)
{
	QmiPacketFifoEntry *entry;
	unsigned int packetLength;
	int locked;

	if (channel == NULL || packet == NULL || length == NULL)
		return (int)0x80330002;
	if (g_qmiPacketFifo.freeCount == 8)
		return (int)0x80330202;
	locked = qmiLockBackend(2);
	entry = g_qmiPacketFifo.consumer;
	if (g_qmiPacketFifo.producer != entry && entry->state == 1) {
		packetLength = entry->length;
		*channel = entry->channel;
		*packet = entry->data;
		*length = packetLength;
		entry->state = 2;
		qmiUnlockBackend(2, locked);
		return 0;
	}
	qmiUnlockBackend(2, locked);
	return (int)0x80330002;
}

/* name unknown: return a claimed consumer packet to the queued state. */
int qmiRetryQueuedPacket(void)
{
	int locked;
	int ret;

	if (g_qmiPacketFifo.freeCount == 8)
		return (int)0x80330202;
	locked = qmiLockBackend(2);
	if (g_qmiPacketFifo.producer == g_qmiPacketFifo.consumer ||
	    g_qmiPacketFifo.consumer->state != 2) {
		ret = (int)0x80330002;
	} else {
		ret = 0;
		g_qmiPacketFifo.consumer->state = 1;
	}
	qmiUnlockBackend(2, locked);
	return ret;
}

/* name unknown: advance the consumer and release its claimed packet. */
int qmiReleaseQueuedPacket(void)
{
	QmiPacketFifoEntry *entry;
	unsigned int state;
	unsigned int freeCount;
	int locked = qmiLockBackend(2);
	int nested;
	int ret;

	entry = g_qmiPacketFifo.consumer;
	state = entry->state;
	g_qmiPacketFifo.consumer = entry->next;
	if (state == 2) {
		if (g_qmiPacketFifo.freeCount == 8) {
			ret = (int)0x80330202;
		} else {
			nested = qmiLockBackend(2);
			entry->data[0] = 0;
			freeCount = g_qmiPacketFifo.freeCount;
			entry->channel = 2;
			entry->state = 0;
			entry->length = 0;
			g_qmiPacketFifo.freeCount = freeCount + 1;
			qmiUnlockBackend(2, nested);
			ret = 0;
		}
		qmiUnlockBackend(2, locked);
		return ret;
	}
	qmiUnlockBackend(2, locked);
	return (int)0x80330002;
}

/* name unknown: test the queue's free count under its synchronization lock. */
int qmiIsPacketFifoEmpty(void)
{
	int locked = qmiLockBackend(2);
	int empty = g_qmiPacketFifo.freeCount == 8;

	qmiUnlockBackend(2, locked);
	return empty;
}
