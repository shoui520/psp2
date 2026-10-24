/* Shared SceNetPs packet adapters; unchanged Sony glue bodies. */

#include "net_ps_internal.h"
#include <net_ps/netps.h>
#include <string.h>

extern struct NetDriver *gNetPsDriverHead;
extern unsigned int gNetPsDriverFlags;
extern int gNetPsPacketCount;
extern struct NetDriverPacket *gNetPsPacketCacheHead;
extern struct NetDriverPacket *gNetPsPacketCacheTail;
extern int gNetPsPacketCacheCount;
int sceNetPsCheckPacketReady(struct SceNetPsDriverContext *, struct NetDriverPacket *);
int sceNetPsRequeuePacket(struct SceNetPsDriverContext *, struct NetDriverPacket *);

/* Name unknown; move pending packets to the consumer's ready list. */
struct NetDriverPacket *sceNetPsPreparePacketQueue(SceNetPsPacketQueue *queue,
						 int sending, int locked)
{
	struct NetDriverPacket *head, *tail;
	SceNetPsMutex *mutex;

	if (queue->readyHead)
		return queue->readyHead;
	if (locked) {
		head = queue->pendingHead;
		tail = queue->pendingTail;
		queue->pendingHead = NULL;
		queue->pendingTail = NULL;
		queue->readyHead = head;
		queue->readyTail = tail;
		queue->pendingCount = 0;
		return head;
	}
	if (sending) {
		sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
		head = queue->pendingHead;
		mutex = &gNetPsDevSendMutex;
	} else {
		sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
		mutex = &gNetPsDevRecvMutex;
		head = queue->pendingHead;
	}
	tail = queue->pendingTail;
	queue->pendingCount = 0;
	queue->pendingHead = NULL;
	queue->pendingTail = NULL;
	queue->readyHead = head;
	queue->readyTail = tail;
	sce_psnet_bnet_mutex_unlock(mutex);
	return queue->readyHead;
}

/* Name unknown; publish queued per-driver status changes. */
int sceNetPsDispatchDriverEvents(void)
{
	struct NetDriver *driver;
	struct SceNetPsDriverContext *context;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsNotifyDriver(NULL, 8, NULL, 0);
	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		context = driver->context;
		if (context->flags & 4) {
			sceNetPsNotifyDriver(driver, 0x1000000, NULL, 0);
			context->flags &= ~4u;
		}
		if (context->flags & 8) {
			sceNetPsNotifyDriver(driver, 0x8000000, NULL, 0);
			context->flags &= ~8u;
		}
	}
	gNetPsDriverFlags &= ~1u;
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/* Name unknown; detach the next ready packet, honoring emulation delays. */
struct NetDriverPacket *sceNetPsDequeuePacket(struct SceNetPsDriverContext *context,
					    SceNetPsPacketQueue *queue,
					    int sending, int locked)
{
	struct NetDriverPacket *packet;

	sce_psnet_bnet_mutex_lock(&gNetPsEmuMutex, NULL);
	for (;;) {
		packet = sceNetPsPreparePacketQueue(queue, sending, locked);
		if (!packet || sceNetPsCheckPacketReady(context, packet) < 0) {
			packet = NULL;
			break;
		}
		if (packet->next)
			packet->next->prev = packet->prev;
		else
			queue->readyTail = packet->prev;
		if (packet->prev)
			packet->prev->next = packet->next;
		else
			queue->readyHead = packet->next;
		if (!sceNetPsRequeuePacket(context, packet))
			break;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsEmuMutex);
	return packet;
}

/* Name unknown; fetch the next transmit packet for a registered driver. */
struct NetDriverPacket *sceNetPsDequeueTxPacket(struct NetDriver *driver)
{
	struct SceNetPsDriverContext *context = driver->context;

	return sceNetPsDequeuePacket(context, &context->sendQueue, 1, 0);
}

/* Name unknown; the driver's release service uses its second argument. */
int sceNetPsFreeForDriver(struct NetDriver *driver, void *ptr)
{
	return sceNetPsFree(ptr);
}

/* Name unknown; allocate a frame with its packet and first buffer owners. */
struct NetDriverPacket *sceNetPsAllocatePacket(struct NetDriver *driver,
					     unsigned int length,
					     unsigned int alignment)
{
	struct NetDriverPacket *packet;
	struct NetDriverBuffer *buffer;
	unsigned char *allocation;
	unsigned int prefix, dataSize, allocationSize;
	int mask;

	if (!alignment || (alignment & 3)) {
		mask = -4;
		alignment = 4;
	} else {
		mask = -alignment;
	}
	prefix = driver ? driver->prefix : 0;
	dataSize = (length - 1 + alignment) & mask;
	sce_psnet_bnet_mutex_lock(&gNetPsPktMutex, NULL);
	dataSize += prefix;
	allocationSize = dataSize + sizeof(*packet) + sizeof(*buffer);
	++gNetPsPacketCount;
	if (allocationSize <= 1600 && gNetPsPacketCacheHead) {
		packet = gNetPsPacketCacheHead;
		if (packet->next)
			packet->next->prev = packet->prev;
		else
			gNetPsPacketCacheTail = packet->prev;
		if (packet->prev)
			packet->prev->next = packet->next;
		else
			gNetPsPacketCacheHead = packet->next;
		--gNetPsPacketCacheCount;
		allocationSize = packet->allocationSize;
		allocation = packet->buffer->allocationStart;
		sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
		if (allocation)
			goto initialize;
	} else {
		sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
	}
	allocation = sceNetPsMalloc(allocationSize, 1, alignment);
	if (allocation) {
initialize:
		packet = (struct NetDriverPacket *)(allocation + dataSize);
		memset(packet, 0, sizeof(*packet));
		packet->allocationSize = allocationSize;
		buffer = (struct NetDriverBuffer *)(packet + 1);
		packet->buffer = buffer;
		memset(buffer, 0, sizeof(*buffer));
		buffer->allocationStart = allocation;
		buffer->allocationEnd = allocation + length + prefix;
		buffer->data = allocation + prefix;
		buffer->end = allocation + prefix;
		return packet;
	}
	sce_psnet_bnet_mutex_lock(&gNetPsPktMutex, NULL);
	--gNetPsPacketCount;
	sce_psnet_bnet_mutex_unlock(&gNetPsPktMutex);
	return NULL;
}

/* Name unknown; driver packet allocation requires a registered driver. */
struct NetDriverPacket *sceNetPsAllocateRxPacket(struct NetDriver *driver,
					       unsigned int length,
					       unsigned int alignment)
{
	if (driver)
		return sceNetPsAllocatePacket(driver, length, alignment);
	return NULL;
}

/* Name unknown; the driver's memory service uses its second argument. */
void *sceNetPsMallocForDriver(struct NetDriver *driver, SceSize size,
			    SceSize alignment)
{
	return sceNetPsMalloc(size, 1, alignment);
}

/* Name unknown; give the driver the current Ethernet multicast addresses. */
int sceNetPsUpdateDriverMulticast(struct NetDriver *driver)
{
	struct SceNetPsDriverContext *context = driver->context;
	struct ethercom *ethernet = (struct ethercom *)context->interface;
	struct ether_multi *entry = LIST_FIRST(&ethernet->ec_multiaddrs);
	unsigned char *addresses, *destination;
	unsigned int size = 0;

	if (!entry)
		return 0;
	do {
		entry = LIST_NEXT(entry, enm_list);
		size += ETHER_ADDR_LEN;
	} while (entry);
	addresses = sceNetPsMalloc(size, 1, 8);
	if (!addresses)
		return 12;
	destination = addresses;
	ethernet = (struct ethercom *)context->interface;
	LIST_FOREACH(entry, &ethernet->ec_multiaddrs, enm_list) {
		memcpy(destination, entry->enm_addrlo, ETHER_ADDR_LEN);
		destination += ETHER_ADDR_LEN;
	}
	if (driver->multicast)
		driver->multicast(driver, addresses, size);
	sceNetPsFree(addresses);
	return 0;
}

