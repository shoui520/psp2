#ifndef SCE_NETPS_DRIVER_H
#define SCE_NETPS_DRIVER_H

#include <psp2/types.h>
#include <psp2/net/net.h>
#include <sce/netdrv.h>
#include <stddef.h>

struct ifnet;

/* Two consecutive 20-byte queues occupy context bytes 8 through 47. */
typedef struct SceNetPsPacketQueue {
	struct NetDriverPacket *pendingHead;
	struct NetDriverPacket *pendingTail;
	struct NetDriverPacket *readyHead;
	struct NetDriverPacket *readyTail;
	unsigned int pendingCount;
} SceNetPsPacketQueue;

typedef struct SceNetPsDriverCallback {
	SceUID globalCallbackId;
	const char *name;
} SceNetPsDriverCallback;

typedef struct SceNetPsDriverEvent {
	char name[32];
	SceUInt64 events;
	SceUID callbackId;
	SceNetPsDriverCallback callback;
	unsigned int flags;
} SceNetPsDriverEvent;

/* The second word is only cleared or copied, never consumed by a reader. */
typedef struct SceNetPsDriverCounter {
	int seconds;
	unsigned int reserved;
} SceNetPsDriverCounter;

/* Allocated as 864 bytes; the entire owner is retained across driver removal. */
struct SceNetPsDriverContext {
	struct NetDriver *driver;
	unsigned int instanceMask;
	SceNetPsPacketQueue sendQueue;
	SceNetPsPacketQueue receiveQueue;
	struct ifnet *interface;
	unsigned char *hardwareAddress;
	unsigned int rxPackets;
	unsigned int txPackets;
	unsigned int rxBytes;
	unsigned int txBytes;
	unsigned int rxDrops;
	unsigned int txDrops;
	unsigned int rxEmulationDrops;
	SceUInt64 pendingEvents;
	int lastError;
	SceNetPsDriverEvent eventHandlers[3];
	unsigned int flags;
	int pendingReceiveArgument;
	int lastLinkStatus;
	int autoConnectEnabled;
	SceNetPsDriverCounter idle;
	SceNetPsDriverCounter uptime;
	unsigned int generation;
	int idleTimeoutSeconds;
	int autoConnectPolicy;
	int loopbackRequested;
	int connectionState;
	unsigned int sendBpsBytes;
	unsigned int receiveBpsBytes;
	unsigned int sendBpsPackets;
	unsigned int receiveBpsPackets;
	SceNetEmulationParam emulation;
	unsigned int emulationOwnerFlags;
	int promiscuousEnabled;
	int promiscuousReferences;
};

_Static_assert(sizeof(SceNetPsPacketQueue) == 20, "SceNetPsPacketQueue size");
_Static_assert(sizeof(SceNetPsDriverCallback) == 8, "SceNetPsDriverCallback size");
_Static_assert(sizeof(SceNetPsDriverEvent) == 56, "SceNetPsDriverEvent size");
_Static_assert(sizeof(SceNetPsDriverCounter) == 8, "SceNetPsDriverCounter size");
_Static_assert(offsetof(struct SceNetPsDriverContext, pendingEvents) == 88,
	       "SceNetPsDriverContext pendingEvents");
_Static_assert(offsetof(struct SceNetPsDriverContext, eventHandlers) == 104,
	       "SceNetPsDriverContext eventHandlers");
_Static_assert(offsetof(struct SceNetPsDriverContext, idle) == 288,
	       "SceNetPsDriverContext idle");
_Static_assert(offsetof(struct SceNetPsDriverContext, emulation) == 340,
	       "SceNetPsDriverContext emulation");
_Static_assert(sizeof(struct SceNetPsDriverContext) == 864,
	       "SceNetPsDriverContext size");

int sceNetPsSetDriverMtu(struct NetDriver *driver, unsigned int mtu);
int sceNetPsUpdateConnection(struct SceNetPsDriverContext *context,
			    int receiveArgument);
struct NetDriverPacket *sceNetPsPreparePacketQueue(SceNetPsPacketQueue *queue,
						 int sending, int locked);
int sceNetPsDispatchDriverEvents(void);
struct NetDriverPacket *sceNetPsDequeuePacket(struct SceNetPsDriverContext *context,
					    SceNetPsPacketQueue *queue,
					    int sending, int locked);
struct NetDriverPacket *sceNetPsDequeueTxPacket(struct NetDriver *driver);
int sceNetPsFreeForDriver(struct NetDriver *driver, void *ptr);
struct NetDriverPacket *sceNetPsAllocatePacket(struct NetDriver *driver,
					     unsigned int length,
					     unsigned int alignment);
struct NetDriverPacket *sceNetPsAllocateRxPacket(struct NetDriver *driver,
					       unsigned int length,
					       unsigned int alignment);
void *sceNetPsMallocForDriver(struct NetDriver *driver, SceSize size,
			    SceSize alignment);
int sceNetPsUpdateDriverMulticast(struct NetDriver *driver);
int sceNetPsKickTransmit(struct NetDriver *driver, int resetIdle);
int sceNetPsNotifyDriver(struct NetDriver *driver, const void *reserved,
			int event, int argument, const void *data, int size);
void sceNetPsSwapCounterByteOrder(unsigned int counters[8]);

struct mbuf;
void sceNetPsFreePacket(struct NetDriverPacket *packet);
void sceNetPsReleaseRxMbuf(struct mbuf *mbuf, char *buffer, size_t size,
			 struct NetDriverPacket *packet);
void sceNetPsReleaseTxPacket(struct NetDriver *driver,
			     struct NetDriverPacket *packet);
struct NetDriverBuffer *sceNetPsAppendBuffer(struct NetDriver *driver,
					   struct NetDriverPacket *packet,
					   unsigned int length,
					   unsigned int alignment);

int sceNetPsUpdateConnections(struct SceNetPsDriverContext *context,
			     int receiveArgument);
int sceNetPsResetIdleTime(struct SceNetPsDriverContext *context);
int sceNetPsNotifyLinkState(struct SceNetPsDriverContext *context);
struct sockaddr_in;
int sceNetPsResetAddressIdleTime(const struct sockaddr_in *address);

#endif
