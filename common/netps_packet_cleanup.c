/* Shared Sony packet cleanup adapters; unchanged original bodies. */

#include "../sys/internal/kernel/src/net_ps/net_ps_internal.h"
#include <string.h>

extern int gNetPsPacketCount;
extern int gNetPsQosPacketCount;
extern struct NetDriverPacket *gNetPsPacketCacheHead;
extern struct NetDriverPacket *gNetPsPacketCacheTail;
extern int gNetPsPacketCacheCount;

/* Name unknown; release chained buffers and retain reusable frame storage. */
void sceNetPsFreePacket(struct NetDriverPacket *packet)
{
	struct NetDriverBuffer *first, *buffer, *next;

	first = packet->buffer;
	if (!first)
		return;
	buffer = first->next;
	while (buffer) {
		next = buffer->next;
		sceNetPsFree(buffer->allocationStart);
		buffer = next;
	}
	sce_psnet_bnet_mutex_lock(&gNetPsPktMutex, NULL);
	--gNetPsPacketCount;
	if (gNetPsPacketCacheCount > 31 || packet->allocationSize < 1600) {
		sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
		sceNetPsFree(first->allocationStart);
	} else {
		packet->prev = gNetPsPacketCacheTail;
		if (gNetPsPacketCacheTail)
			gNetPsPacketCacheTail->next = packet;
		else
			gNetPsPacketCacheHead = packet;
		packet->next = NULL;
		gNetPsPacketCacheTail = packet;
		++gNetPsPacketCacheCount;
		sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
	}
}

/* Name unknown; return an externally backed receive mbuf and its packet. */
void sceNetPsReleaseRxMbuf(struct mbuf *mbuf, char *buffer, size_t size,
			 struct NetDriverPacket *packet)
{
	sceNetPsFreePacket(packet);
	pool_put(mb_cache.pc_pool, mbuf);
	--gNetPsQosPacketCount;
}

/* Name unknown; the driver returns transmitted packet ownership here. */
void sceNetPsReleaseTxPacket(struct NetDriver *driver,
			     struct NetDriverPacket *packet)
{
	sceNetPsFreePacket(packet);
}

/* Name unknown; extend a packet's buffer chain with separately owned storage. */
struct NetDriverBuffer *sceNetPsAppendBuffer(struct NetDriver *driver,
					   struct NetDriverPacket *packet,
					   unsigned int length,
					   unsigned int alignment)
{
	struct NetDriverBuffer *buffer, *tail;
	unsigned char *allocation;
	unsigned int dataSize;

	dataSize = (alignment - 1 + length) & -alignment;
	allocation = sceNetPsMalloc(dataSize + sizeof(*buffer), 1, alignment);
	if (!allocation)
		return NULL;
	buffer = (struct NetDriverBuffer *)(allocation + dataSize);
	memset(buffer, 0, sizeof(*buffer));
	buffer->allocationStart = allocation;
	buffer->allocationEnd = allocation + length;
	buffer->data = allocation;
	buffer->end = allocation;
	if (packet->buffer) {
		tail = packet->buffer;
		while (tail->next)
			tail = tail->next;
		tail->next = buffer;
	}
	return buffer;
}

