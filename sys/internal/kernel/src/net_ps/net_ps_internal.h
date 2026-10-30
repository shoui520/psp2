#ifndef SCE_NET_PS_INTERNAL_H
#define SCE_NET_PS_INTERNAL_H

#include <scetypes.h>
#include <libhmac.h>
#include <net.h>
#include <net_ps/netdrv.h>
#include <net_ps/netps.h>
#include <net_ps/netps_gateway.h>
#include <net_ps/netps_thread.h>
#include "netps_process.h"
#include "netps_resource.h"
#include "netps_driver.h"
#include "netps_dump.h"
#include <sys/callout.h>

/* Sony's private names are unavailable; layouts follow complete owners. */
typedef struct SceNetPsGatewayState {
	struct SceNetKernelGateway *interface;
	int socket;
} SceNetPsGatewayState;

_Static_assert(sizeof(SceNetPsGatewayState) == 8, "gateway state size");
_Static_assert(offsetof(SceNetPsGatewayState, socket) == 4, "gateway socket");

#include <net_ps/net_ps_bsd.h>

typedef struct SceNetPsSysEventHooks {
	int (*suspend)(void);
	int (*resume)(void);
} SceNetPsSysEventHooks;

_Static_assert(sizeof(SceNetPsSysEventHooks) == 8,
	       "SceNetPsSysEventHooks size");

typedef struct SceNetPsThreadEventHooks {
	int (*exit)(SceUID threadId);
} SceNetPsThreadEventHooks;

_Static_assert(sizeof(SceNetPsThreadEventHooks) == 4,
	       "SceNetPsThreadEventHooks size");

/* The allocator's complete header precedes each returned allocation. */
typedef struct SceNetPsHeapBlock {
	unsigned int magic;
	const void *freeCaller;
	const void *allocationCaller;
	int requestedSize;
	struct SceNetPsHeapBlock *next;
	struct SceNetPsHeapBlock *prev;
	int capacity;
	unsigned int reserved;
} SceNetPsHeapBlock;

_Static_assert(sizeof(SceNetPsHeapBlock) == 32, "SceNetPsHeapBlock size");
_Static_assert(offsetof(SceNetPsHeapBlock, next) == 16, "heap block next");
_Static_assert(offsetof(SceNetPsHeapBlock, prev) == 20, "heap block prev");
_Static_assert(offsetof(SceNetPsHeapBlock, capacity) == 24, "heap block capacity");

/* Complete serialized record: 168c initializes all320 bytes; 1eb8 augments it.
 * 2f10 only changes byte order and clears private controls for serialization.
 * Reserved words have no operational producer; zero is retained on output.
 */
typedef struct SceNetPsInterfaceAddress {
	unsigned int address;
	unsigned int broadcastAddress;
	unsigned int broadcastAddressCopy;
	unsigned int subnetMask;
} SceNetPsInterfaceAddress;

typedef struct SceNetPsInterfaceInfo {
	char name[16];
	char parentName[16];
	unsigned int interfaceIndex;
	unsigned int parentInterfaceIndex;
	SceNetPsInterfaceAddress addresses[2];
	unsigned int flags;
	unsigned int driverType;
	SceNetEtherAddr hardwareAddress;
	unsigned int mtu;
	unsigned int driverMtu;
	/* Each wire counter group is passed as one complete eight-word array. */
	union {
		struct {
			unsigned int txBytes, txPackets;
			unsigned int txBroadcastBytes, txBroadcastPackets;
			unsigned int txMulticastBytes, txMulticastPackets;
			unsigned int txErrors, txDrops;
		};
		unsigned int txCounters[8];
	};
	union {
		struct {
			unsigned int rxBytes, rxPackets;
			unsigned int rxBroadcastBytes, rxBroadcastPackets;
			unsigned int rxMulticastBytes, rxMulticastPackets;
			unsigned int rxDrops, rxErrors;
		};
		unsigned int rxCounters[8];
	};
	union {
		struct {
			unsigned int reservedTxCounters[6];
			unsigned int softwareTxDrops;
			unsigned int reservedTxCounter;
		};
		unsigned int softwareTxCounters[8];
	};
	union {
		struct {
			unsigned int reservedRxCounters[6];
			unsigned int softwareRxDrops;
			unsigned int reservedRxCounter;
		};
		unsigned int softwareRxCounters[8];
	};
	int connectionState;
	int idleSeconds;
	union {
		struct {
			unsigned int reservedControls[2];
			int idleTimeoutSeconds;
			int autoConnectPolicy;
			unsigned int reservedControl;
		};
		unsigned int privateControls[5];
	};
	int linkStatus;
	int emulationSeedPresent;
	union {
		struct {
			int promiscuousEnabled;
			int promiscuousReferences;
		};
		int promiscuousControls[2];
	};
	int uptimeSeconds;
	unsigned int generation;
	unsigned int reservedTail[11];
} SceNetPsInterfaceInfo;

_Static_assert(sizeof(SceNetPsInterfaceAddress) == 16, "interface address size");
_Static_assert(sizeof(SceNetPsInterfaceInfo) == 320, "interface info size");
_Static_assert(offsetof(SceNetPsInterfaceInfo, addresses) == 40,
	       "interface IPv4 records");
_Static_assert(offsetof(SceNetPsInterfaceInfo, txBytes) == 96,
	       "interface driver counters");
_Static_assert(offsetof(SceNetPsInterfaceInfo, connectionState) == 224,
	       "interface connection state");
_Static_assert(offsetof(SceNetPsInterfaceInfo, linkStatus) == 252,
	       "interface link status");
_Static_assert(offsetof(SceNetPsInterfaceInfo, generation) == 272,
	       "interface generation");
int sceNetPsGetInterfaceList(SceNetPsInterfaceInfo *records, int capacity);

/*
 * SCE: the sceNetControl record of the driver event commands: register a
 * callback (0x20000003), take its events (0x10000002), post events
 * (0x20000005)
 */
typedef struct SceNetPsDriverEventControl {
	SceUID callbackId;
	int clearPending;
	unsigned int reserved[2];
	SceUInt64 events;
	int error;
	unsigned int reserved2;
} SceNetPsDriverEventControl;

_Static_assert(sizeof(SceNetPsDriverEventControl) == 32,
	       "driver event control size");
_Static_assert(offsetof(SceNetPsDriverEventControl, events) == 16,
	       "driver event control events");

/* SCE: debug command 4's result, one interface's record */
typedef struct SceNetPsDebugInterfaceInfo {
	int interfaceType;
	unsigned int infoSize;
	SceNetPsInterfaceInfo info;
} SceNetPsDebugInterfaceInfo;

_Static_assert(sizeof(SceNetPsDebugInterfaceInfo) == 328,
	       "debug interface info size");

/* Sony's network-owned bookkeeping, separate from the kernel process object. */

struct socket;
struct NetDriverPacket;
struct SceNetPsDriverContext;

/* Complete Sony packet-service state; timers retain NetBSD's own owner. */
typedef struct SceNetPsPacketService {
	struct NetDriverPacket *(*prepareQueue)(SceNetPsPacketQueue *, int, int);
	int (*kickTx)(struct NetDriver *, int);
	unsigned int sequenceNumber;
	unsigned int releaseTimeMs;
	struct callout delayCallout;
	struct callout bandwidthCallout;
	unsigned int flags;
	struct NetDriver *activeDriver;
	unsigned int filterFlags;
} SceNetPsPacketService;

_Static_assert(sizeof(struct callout) == 24, "NetBSD callout size");
_Static_assert(sizeof(SceNetPsPacketService) == 76, "SceNetPsPacketService size");
_Static_assert(offsetof(SceNetPsPacketService, delayCallout) == 16,
	       "SceNetPsPacketService delayCallout");
_Static_assert(offsetof(SceNetPsPacketService, bandwidthCallout) == 40,
	       "SceNetPsPacketService bandwidthCallout");
_Static_assert(offsetof(SceNetPsPacketService, flags) == 64,
	       "SceNetPsPacketService flags");
_Static_assert(offsetof(SceNetPsPacketService, activeDriver) == 68,
	       "SceNetPsPacketService activeDriver");
_Static_assert(offsetof(SceNetPsPacketService, filterFlags) == 72,
	       "SceNetPsPacketService filterFlags");

struct ifnet;

extern SceNetPsMutex gNetPsThreadMutex;
extern SceNetPsMutex gNetPsJumboMutex;
extern SceNetPsRuntime gNetPsRuntime;

void *sceNetPsMalloc(SceSize size, int flags, SceSize align);
int sceNetPsFree(void *ptr);
int sceNetPsCopyIn(const void *source, void *destination, size_t length,
		   int kernel);
int sceNetPsCopyOut(const void *source, void *destination, size_t length,
		    int kernel);
int copyinstr(const void *source, void *destination, size_t length, size_t *copied);
int copyin(const void *source, void *destination, size_t length);
int copyout(const void *source, void *destination, size_t length);
int sceNetPsAllocateDescriptorId(void);
int sceNetPsAllocateDumpId(void);
void sceNetPsSetDump(unsigned int dumpId, SceNetPsDump *dump);
SceNetPsDump *sceNetPsLookupDump(unsigned int dumpId, int flags);
void sceNetPsWriteDump(SceNetPsDump *dump, const void *buffer, int length);
int sceNetPsAllocateEpollId(void);

int sce_psnet_bnet_mutex_lock(SceNetPsMutex *mutex, unsigned int *timeout);
int sce_psnet_bnet_mutex_unlock(SceNetPsMutex *mutex);
int sce_psnet_bnet_sync_signal(SceNetPsSync *sync, unsigned int events);
int sceNetPsInitializePlatform(int (*initialize)(SceUID, void *, int,
		SceNetPsThreadParam *, SceUID, SceUID, SceUID),
		SceNetPsSysEventHooks *systemHooks,
		SceNetPsThreadEventHooks *threadHooks,
		SceNetPsProcessHooks *processHooks);

/* net_ps_emulation.c */
void sceNetPsSwapEmulationParam(SceNetEmulationParam *parameters);
int sceNetPsSetEmulationPreset(SceNetEmulationParam *parameters);
void sceNetPsBandwidthTimer(void *argument);
int sceNetPsProcessDelayedPackets(struct ifnet *interface);
int sceNetPsSetDriverEmulation(struct SceNetPsDriverContext *context,
			       SceNetEmulationParam *parameters,
			       int networkOrder, int privileged);
int sceNetPsFilterTxMbuf(struct SceNetPsDriverContext *context,
			struct mbuf *mbuf, int *duplicates);
int sceNetPsSetTxEmulation(struct SceNetPsDriverContext *context,
			  struct mbuf *mbuf, struct NetDriverPacket *packet);
int sceNetPsFilterRxMbuf(struct SceNetPsDriverContext *context,
			struct mbuf *mbuf, unsigned int length);

/* SCE: a socket request waiting for an interface's automatic connection */
typedef struct SceNetPsPendingRequest {
	struct SceNetPsPendingRequest *next;
	struct SceNetPsPendingRequest *prev;
	int (*complete)(void *argument, int cancel);
	void *argument;
} SceNetPsPendingRequest;

_Static_assert(sizeof(SceNetPsPendingRequest) == 16, "SceNetPsPendingRequest size");

/* SCE: a non-blocking connect retried once the automatic connection is up */
typedef struct SceNetPsConnectParam {
	int socket;
	struct sockaddr address;
	unsigned int addressLength;
} SceNetPsConnectParam;

typedef struct SceNetPsConnectRequest {
	SceNetPsPendingRequest request;
	SceNetPsConnectParam param;
} SceNetPsConnectRequest;

_Static_assert(sizeof(SceNetPsConnectRequest) == 40, "SceNetPsConnectRequest size");

/* SCE: a non-blocking send queued until the automatic connection is up */
typedef struct SceNetPsSendParam {
	int socket;
	struct msghdr message;
	struct iovec iov;
	int flags;
} SceNetPsSendParam;

typedef struct SceNetPsSendRequest {
	SceNetPsPendingRequest request;
	SceNetPsSendParam param;
} SceNetPsSendRequest;

_Static_assert(sizeof(SceNetPsSendRequest) == 60, "SceNetPsSendRequest size");

/* SCE: the processes that sent data since the list was last taken */
typedef struct SceNetPsSenderList {
	SceUID processId[16];
	int count;
} SceNetPsSenderList;

_Static_assert(sizeof(SceNetPsSenderList) == 68, "SceNetPsSenderList size");

/* net_ps_glue.c */
int sceNetPsQueuePendingRequest(SceNetPsPendingRequest *request);
int sceNetPsWaitAutoConnect(struct socket *so);
int sceNetPsRecordSender(SceUID processId);
int sceNetPsTakeSenders(SceNetPsSenderList *list);
int sceNetPsPollDevices(void);
int sceNetPsGetDnsConfig(SceNetPsDnsConfig *config, int networkOrder, int addressesOnly);
int sceNetPsInitializeDebugInterface(SceNetPsDebugClient **slot,
				     struct NetDriver *driver);
int sceNetPsInitializeGateway(SceNetPsGatewayState *slot, struct NetDriver *driver);
int sceNetPsNotifyDriverCallbacks(struct SceNetPsDriverContext *context, SceUInt64 events);
int sceNetPsTakeDriverEvents(struct SceNetPsDriverContext *context,
			     SceUID callbackId,
			     SceNetPsDriverEventControl *control);
int sceNetPsPostDriverEvents(struct SceNetPsDriverContext *context,
			     SceUID callbackId,
			     const SceNetPsDriverEventControl *control);
int sceNetPsHasDriverCallbacks(struct SceNetPsDriverContext *context);
int sceNetPsHasPendingDriverEvents(SceUInt64 events);
int sceNetPsSetSocketSuspendPolicy(unsigned int policy);
int sceNetPsSetDnsConfig(const SceNetPsDnsConfig *config);
int sceNetPsGetConnectionParam(struct SceNetPsDriverContext *context,
			       SceNetPsConnectionParam *parameters);
int sceNetPsSetConnectionParam(struct SceNetPsDriverContext *context,
			       const SceNetPsConnectionParam *parameters);
int sceNetPsGetPromiscuous(struct SceNetPsDriverContext *context,
			   SceNetPsPromiscuousParam *parameters);
int sceNetPsSetPromiscuous(struct SceNetPsDriverContext *context,
			   const SceNetPsPromiscuousParam *parameters);
int sceNetPsRegisterDriverCallback(struct SceNetPsDriverContext *context,
				   SceUID callbackId, int clearPending);
int sceNetPsUnregisterDriverCallback(struct SceNetPsDriverContext *context,
				     SceUID callbackId);
unsigned int sceNetPsGetFreeMemory(int lowest);
int sceNetPsInitializeHeap(void *base, int size);

/* net_ps_packet.c */
int sceNetPsStartBandwidthTimer(struct ifnet *interface);
int sceNetPsGetDriverEmulation(struct SceNetPsDriverContext *context,
			       SceNetEmulationParam *parameters, int networkOrder);
int sceNetPsCanTransmit(struct SceNetPsDriverContext *context, struct ifnet *interface);
int sceNetPsStopDriverEmulation(struct NetDriver *driver);
int sceNetPsInitializePacketCallbacks(struct NetDriverPacket *(*prepare)(
		SceNetPsPacketQueue *, int, int),
		int (*kick)(struct NetDriver *, int));

/* net_ps_startup.c */
int sceNetPsStartModule(SceUID memoryId, void *base, int size,
		SceNetPsThreadParam *thread, SceUID systemId,
		SceUID threadId, SceUID processId);

/* net_ps_statistics.c */
int sceNetPsGetKernelStatistics(SceNetPsKernelStatistics *statistics, int networkOrder);

/* src/ps/osdependence/ngp/system_os.c */
int init_crypto(void *(*allocate)(SceSize), void (*release)(void *));
void sce_psnet_panic(const char *message, ...);
int sceNetPsIsHeapAddress(const void *address);
int sceNetPsIsGameBudget(void);
int sceNetPsIsCEX(void);
int sceNetPsIsSdkVersion2OrLater(void);
int sceNetPsCopyUserOut(const void *source, void *destination, size_t length);
int sceNetPsCopyUserIn(const void *source, void *destination, size_t length);
int sceNetPsCopyUserString(const char *source, char *destination, size_t length);
SceInt64 sceNetPsGetSystemTime(SceUInt64 *value);
int sceNetPsFormatUidName(SceUID uid, char *name, int length);
int sceNetPsCreateThread(SceUID *threadId, SceNetPsThreadParam *param, const char *name);
SceUID sceNetPsGetThreadId(void);
SceUID sceNetPsGetProcessId(void);
unsigned int sceNetPsGetRandom(void);
SceUID bnet_puid_to_guid(SceUID puid);

/* net_ps_random.c */
void sceNetPsSeedRandom(void);
int sceNetPsSeedEmulationRandom(unsigned int seed);
unsigned int sceNetPsEmulationRandom(void);

/* net_ps_hmac.c, net_ps_aes.c, system_os.c: what udpp2p signs, encrypts and compares with */
int sceNetPsHmacSha1Init(SceHmacSha1Context *pContext,
		       const void *secretkey, unsigned int keylen);
int sceNetPsHmacSha1Update(SceHmacSha1Context *pContext,
			 const void *plain, unsigned int len);
int sceNetPsHmacSha1Result(unsigned char *digest,
			 SceHmacSha1Context *pContext);
int sceNetPsAesEncrypt(unsigned char *destination, const unsigned char *source,
		       int length, const void *key, unsigned int keyBits,
		       const unsigned char *initialVector);
int sceNetPsAesDecrypt(unsigned char *destination, const unsigned char *source,
		       int length, const void *key, unsigned int keyBits,
		       const unsigned char *initialVector);
int sceNetPsTimingSafeCompare(const void *first, const void *second,
			    SceSize length);
int sceNetPsExitThread(void);
int sce_psnet_bnet_mutex_create(SceNetPsMutex *mutex, const char *name);
int sce_psnet_bnet_sync_create(SceNetPsSync *sync, unsigned int events, const char *name);
int sce_psnet_bnet_sync_delete(SceNetPsSync *sync);
int sce_psnet_bnet_sync_wait(SceNetPsSync *sync, unsigned int *timeout, unsigned int flags);
unsigned int sce_psnet_bnet_sync_clear(SceNetPsSync *sync);
int sceNetPsBindCallback(SceNetPsDriverCallback *callback, SceUID callbackId, const char *name);
void sceNetPsUnbindCallback(SceNetPsDriverCallback *callback);
int sceNetPsNotifyCallback(SceNetPsDriverCallback *callback, unsigned int argument);
int sceNetPsInitializeModule(void);
int sceNetPsGetCurrentTick(SceUInt64 *tick);
unsigned int sceNetPsSyscallEnter(void);
void sceNetPsSyscallLeave(unsigned int state);

#endif
