#ifndef SCE_NET_PS_INTERNAL_H
#define SCE_NET_PS_INTERNAL_H

#include <scetypes.h>
#include <net.h>
#include <net_ps/netdrv.h>
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

/* Kernel syscall result, distinct from libnet's six-word public statistics. */
typedef struct SceNetPsKernelStatistics {
	unsigned int kernel_mem_free_min;
	unsigned int kernel_mem_free_size;
	int packet_count;
	int packet_qos_count;
} SceNetPsKernelStatistics;

_Static_assert(sizeof(SceNetPsKernelStatistics) == 16,
	       "SceNetPsKernelStatistics size");

/* Debug enumeration writes the whole header followed by SDK socket records. */
typedef struct SceNetPsDebugSockInfo {
	SceUID processId;
	int count;
	unsigned int recordSize;
	SceNetSockInfo sockets[];
} SceNetPsDebugSockInfo;

_Static_assert(sizeof(SceNetPsDebugSockInfo) == 12, "debug sockinfo header size");
_Static_assert(offsetof(SceNetPsDebugSockInfo, processId) == 0,
	       "debug sockinfo process");
_Static_assert(offsetof(SceNetPsDebugSockInfo, count) == 4,
	       "debug sockinfo count");
_Static_assert(offsetof(SceNetPsDebugSockInfo, recordSize) == 8,
	       "debug sockinfo record size");
_Static_assert(offsetof(SceNetPsDebugSockInfo, sockets) == 12,
	       "debug sockinfo records");

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

void *sceNetPsMalloc(SceSize size, char flags, SceSize align);
int sceNetPsFree(void *ptr);
int sceNetPsCopyIn(const void *source, void *destination, size_t length,
		   int kernel);
int sceNetPsCopyInString(const char *source, char *destination, size_t length,
			 size_t *copied);
int sceNetPsCheckedCopyUserIn(const void *source, void *destination, size_t length);
int sceNetPsCheckedCopyUserOut(const void *source, void *destination, size_t length);
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

/* net_ps_glue.c */
int sceNetPsPollDevices(void);
int sceNetPsGetDnsConfig(SceNetPsDnsConfig *config, int networkOrder, int addressesOnly);
int sceNetPsInitializeDebugInterface(void *slot, struct NetDriver *driver);
int sceNetPsInitializeGateway(SceNetPsGatewayState *slot, struct NetDriver *driver);
int sceNetPsNotifyDriverCallbacks(struct SceNetPsDriverContext *context, SceUInt64 events);
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
int sceNetPsIsApplicationProgram(int *error);
int sceNetPsIsGameBudget(void);
int sceNetPsIsDevelopmentMode(void);
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
int sceNetPsExitThread(void);
int sce_psnet_bnet_mutex_create(SceNetPsMutex *mutex, const char *name);
int sce_psnet_bnet_sync_create(SceNetPsSync *sync, unsigned int events, const char *name);
int sce_psnet_bnet_sync_delete(SceNetPsSync *sync);
int sce_psnet_bnet_sync_wait(SceNetPsSync *sync, unsigned int *timeout, unsigned int flags);
unsigned int sce_psnet_bnet_sync_clear(SceNetPsSync *sync);
int sceNetPsBindCallback(SceNetPsDriverCallback *callback, SceUID callbackId, const char *name);
void sceNetPsUnbindCallback(SceNetPsDriverCallback *callback);
int sceNetPsNotifyCallback(SceNetPsDriverCallback *callback, unsigned int argument);
unsigned int sceNetPsSyscallEnter(void);
void sceNetPsSyscallLeave(unsigned int state);

#endif
