/* SceNetPs operating-system glue; original source filename is unavailable. */

#include "net_ps_internal.h"
#include <net_ps/netdrv.h>
#include <net_ps/netps.h>
#include <net_ps/netps_gateway.h>
#include <string.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/if_inarp.h>
#include <net/errno.h>

int sceNetPsCheckMemoryAvailability(int size, int count);
int sceNetPsAccept(int socket, SceNetSockaddr *address,
		   unsigned int *addressLength, int kernel);
int sceNetPsBind(int socket, const SceNetSockaddr *address,
		 unsigned int addressLength, int kernel);
int sceNetPsConnect(int socket, const SceNetSockaddr *address,
		    unsigned int addressLength, int kernel);
int sceNetPsGetpeername(int socket, SceNetSockaddr *address,
			unsigned int *addressLength, int kernel);
int sceNetPsGetsockname(int socket, SceNetSockaddr *address,
			unsigned int *addressLength, int kernel);
int sceNetPsGetsockopt(int socket, int level, int option, void *value,
		       unsigned int *length, int kernel);
int sceNetPsListen(int socket, int backlog, int kernel);
int sceNetPsRecvfrom(int socket, void *buffer, unsigned int length, int flags,
		     SceNetSockaddr *from, unsigned int *fromLength, int kernel);
int sceNetPsSendto(int socket, const void *buffer, unsigned int length, int flags,
		   const SceNetSockaddr *to, unsigned int toLength, int kernel);
int sceNetPsSetsockopt(int socket, int level, int option, const void *value,
		       unsigned int length, int kernel);
int sceNetPsShutdown(int socket, int how, int kernel);
int sceNetPsSocket(const char *name, int domain, int type, int protocol, int kernel);
int sceNetPsClose(int socket, int kernel);
int sceNetPsSocketAbort(int socket, int flags);
int sceNetPsIcmConnect(int socket, int flags);
int sceNetPsSendmsg(int socket, const SceNetMsghdr *message, int flags);
int sceNetPsRecvmsg(int socket, SceNetMsghdr *message, int flags);
int sceNetPsIoctl(int socket, unsigned int command, void *data);
int sceNetPsEpollCtl(int epoll, int operation, int socket, SceNetEpollEvent *event);
int sceNetPsDescriptorCtl(int descriptor, int operation, int socket, void *info);
struct mbuf *m_get(int how, int type);
extern struct malloc_type *mbtypes[8];
int sceNetPsRoutingSysctl(const int *name, unsigned int nameLength, void *oldData,
			  unsigned int *oldLength, const void *newData,
			  unsigned int newLength);

static const unsigned int gNetPsHeapTailMagic = 0x4d61416b;

struct in_ifaddrhead in_ifaddrhead;

struct NetDriver *gNetPsDriverHead;
struct NetDriver *gNetPsDriverTail;
unsigned int gNetPsDriverFlags;
unsigned int gNetPsSocketSuspendPolicy;
/* NetBSD's complete interface queue; both TOOL images initially clear it. */
struct ifnet_head ifnet;
int gNetPsPacketCount;
int gNetPsQosPacketCount;
SceNetPsPendingRequest *gNetPsPendingRequestHead;
SceNetPsPendingRequest *gNetPsPendingRequestTail;
SceNetPsSenderList gNetPsSenders;
struct NetDriverPacket *gNetPsPacketCacheHead;
struct NetDriverPacket *gNetPsPacketCacheTail;
int gNetPsPacketCacheCount;
SceNetPsDnsConfig gNetPsDnsConfig;
SceNetPsConfigSubscriber *gNetPsConfigSubscriberHead;
SceNetPsConfigSubscriber *gNetPsConfigSubscriberTail;
SceNetPsSysEventHooks gNetPsSystemHooks;
SceNetPsThreadEventHooks gNetPsThreadHooks;
SceNetPsProcessHooks gNetPsProcessHooks;
SceNetPsDebugClient **gNetPsDebugInterfaceSlot;
SceNetPsGatewayState *gNetPsGatewayInterfaceSlot;
struct NetDriver *gNetPsDebugDriver;
struct NetDriver *gNetPsGatewayDriver;
extern SceNetPsGatewayState gNetPsGatewayState;
struct SceNetPsDriverContext gNetPsDefaultDriverContext;
unsigned char *gNetPsHeapEnd;
SceNetPsHeapBlock *gNetPsAllocatedHighestBlock;
int gNetPsHeapSize;
int gNetPsHeapReady;
SceNetPsHeapBlock *gNetPsFreeLowestBlock;
SceNetPsHeapBlock *gNetPsAllocatedLowestBlock;
SceNetPsHeapBlock *gNetPsFreeHighestBlock;
unsigned char *gNetPsHeapLastByte;
unsigned int gNetPsLowestFreeMemory;
SceNetPsHeapBlock *gNetPsInitialHeapBlock;
unsigned int gNetPsFreeMemory;
unsigned char *gNetPsHeapStart;
int gNetPsSuspendResult;

unsigned int sceNetPsGetFreeMemory(int lowest);
int sceNetPsManageProcessSockets(SceUID processId, unsigned int flags);
int sceNetPsInitializeProcessProtocol(SceUID processId);
int sceNetPsDestroyProcessEpolls(SceUID processId);
int sceNetPsDestroyProcessDescriptors(SceUID processId);
int sceNetPsDestroyProcessDumps(SceUID processId);
int sceNetPsUnregisterProcessNetworkState(SceUID processId);
int sceNetPsReleaseProcessProtocol(SceUID processId);
int sceNetPsNotifyDriverCallbacks(struct SceNetPsDriverContext *context,
				  SceUInt64 events);
int sceNetPsDriverHasPendingEvents(struct SceNetPsDriverContext *context,
				  SceUInt64 events);
int sceNetPsCheckPacketReady(struct SceNetPsDriverContext *context,
			   struct NetDriverPacket *packet);
int sceNetPsRequeuePacket(struct SceNetPsDriverContext *context,
			 struct NetDriverPacket *packet);

extern int tick;
extern int ticks;
extern int netisr;
extern int gNetPsSoftTick;
extern volatile struct timeval gNetPsTime;
extern volatile struct timeval mono_time;
extern int gNetPsStartTime;
extern int gNetPsInitialized;

void callout_startup(void);
void mbinit(void);
void ifinit(void);
void domaininit(void);
void if_attachdomain(void);
void loopattach(int count);
void pppoeattach(int count);
int callout_hardclock(void);
void softclock(void *arg);
int sceNetPsPollDevices(void);
void sceNetPsReceivePackets(void);
void sceNetPsIsdnInterrupt(void);
void arpintr(void);
void ipintr(void);
void sce_psnet_panic(const char *message, ...) __attribute__((noreturn));

/* bnet_socket_sanity_check still requires the original text/data range owners. */

/* SetDump requires the complete Sony dump owner; allocation is unresolved. */

/* Name unknown; find a socket without checking its process or state. */
__attribute__((noinline))
struct socket *sceNetPsLookupSocketUnchecked(unsigned int socketId, int *error)
{
	if (socketId < 1024)
		return gNetPsResources.sockets[socketId];
	*error = 9;
	return NULL;
}

/* LookupDump requires the complete Sony dump owner. */

/* Name unknown; collect the complete serialized interface record. */
void sceNetPsSetDump(unsigned int dumpId, SceNetPsDump *dump)
{
	unsigned int index = dumpId - 6000;

	if (index < 16) {
		gNetPsResources.dumps[index] = dump;
		if (dump)
			dump->processId = sceNetPsGetProcessId();
	}
}

SceNetPsDump *sceNetPsLookupDump(unsigned int dumpId, int flags)
{
	unsigned int index = dumpId - 6000;
	SceNetPsDump *dump;

	if (index >= 16)
		return NULL;
	dump = gNetPsResources.dumps[index];
	if (dump && ((flags & 1) || dump->processId == sceNetPsGetProcessId()))
		return dump;
	return NULL;
}

int sceNetPsCollectInterfaceInfo(SceNetPsInterfaceInfo *info,
				 struct NetDriver *driver, struct ifnet *interface)
{
	struct in_ifaddr *address;
	struct SceNetPsDriverContext *context;
	int count = 0;

	memset(info, 0, sizeof(*info));
	info->interfaceIndex = interface->if_index;
	memcpy(info->name, interface->if_xname, sizeof(info->name));
	TAILQ_FOREACH(address, &in_ifaddrhead, ia_list) {
		SceNetPsInterfaceAddress *result;
		unsigned int value;

		if (address->ia_ifp != interface || count > 1)
			continue;
		value = address->ia_addr.sin_addr.s_addr;
		++count;
		result = &info->addresses[(unsigned short)value == 0xfea9];
		result->address = value;
		result->broadcastAddress = address->ia_broadaddr.sin_addr.s_addr;
		result->broadcastAddressCopy = address->ia_broadaddr.sin_addr.s_addr;
		result->subnetMask = address->ia_subnetmask;
	}
	info->flags = interface->if_flags;
	info->mtu = interface->if_mtu;
	if (!driver)
		return 0;
	context = driver->context;
	__builtin_memcpy(&info->txBytes, &driver->txBytes, 64);
	info->rxDrops += context->rxEmulationDrops;
	info->softwareTxDrops = context->txDrops;
	info->softwareRxDrops = context->rxDrops;
	info->connectionState = context->connectionState;
	info->idleSeconds = context->idle.seconds;
	info->idleTimeoutSeconds = context->idleTimeoutSeconds;
	info->autoConnectPolicy = context->autoConnectPolicy;
	memcpy(&info->promiscuousEnabled, &context->promiscuousEnabled, 8);
	info->uptimeSeconds = context->uptime.seconds;
	info->generation = context->generation;
	if (driver->type == 1)
		driver->control(driver, NET_DRIVER_CTL_GET_LINK,
				&info->linkStatus, sizeof(info->linkStatus));
	if (!context->emulation.seed) {
		info->emulationSeedPresent = 0;
		return 0;
	}
	info->emulationSeedPresent = 1;
	return 0;
}

/* SCE: the gateway may consume a received packet before driver queuing. */
int sceNetPsDeliverRxPacket(struct NetDriver *driver,
			   struct NetDriverPacket *packet)
{
	struct SceNetPsDriverContext *context = driver->context;

	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	if (gNetPsGatewayState.interface &&
	    gNetPsGatewayState.interface->receive(gNetPsGatewayState.interface,
					   driver, packet) != 1) {
		sceNetPsFreePacket(packet);
		return sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	}
	packet->prev = context->receiveQueue.pendingTail;
	if (packet->prev)
		packet->prev->next = packet;
	else
		context->receiveQueue.pendingHead = packet;
	packet->next = NULL;
	context->receiveQueue.pendingTail = packet;
	++context->receiveQueue.pendingCount;
	return sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
}

extern int gNetPsPacketCaptureActive;
void sceNetPsCaptureTxPacket(struct NetDriverPacket *packet);

/* Name unknown; convert queued BSD packets to driver-owned transmit storage. */
void sceNetPsStartTransmit(struct ifnet *interface)
{
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct NetDriver *driver = context->driver;
	struct NetDriverPacket *packet, *old;
	struct NetDriverBuffer *buffer;
	struct mbuf *mbuf;
	unsigned int checksum;
	int queued = 0, resetIdle = 0, duplicates = 0;

	while ((mbuf = interface->if_snd.ifq_head) != NULL) {
		interface->if_snd.ifq_head = mbuf->m_nextpkt;
		if (!interface->if_snd.ifq_head)
			interface->if_snd.ifq_tail = NULL;
		mbuf->m_nextpkt = NULL;
		--interface->if_snd.ifq_len;
		if (sceNetPsFilterTxMbuf(context, mbuf, &duplicates))
			continue;
		do {
			packet = sceNetPsAllocatePacket(driver, mbuf->m_pkthdr.len, 128);
			if (!packet) {
				m_freem(mbuf);
				++context->txDrops;
				goto done;
			}
			checksum = interface->if_csum_flags_tx & mbuf->m_pkthdr.csum_flags;
			if (checksum & 0x11)
				packet->checksumFlags |= 4;
			if (checksum & 0x22)
				packet->checksumFlags |= 2;
			if (checksum & 0x40)
				packet->checksumFlags |= 1;
			sceNetPsSetTxEmulation(context, mbuf, packet);
			buffer = packet->buffer;
			m_copydata(mbuf, 0, mbuf->m_pkthdr.len, buffer->end);
			buffer->end += mbuf->m_pkthdr.len;
			packet->length += mbuf->m_pkthdr.len;
			++context->txPackets;
			context->txBytes += mbuf->m_pkthdr.len;
			if (gNetPsPacketCaptureActive)
				sceNetPsCaptureTxPacket(packet);
			sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
			if ((int)context->sendQueue.pendingCount > 49) {
				old = context->sendQueue.pendingHead;
				if (old->next)
					old->next->prev = old->prev;
				else
					context->sendQueue.pendingTail = old->prev;
				if (old->prev)
					old->prev->next = old->next;
				else
					context->sendQueue.pendingHead = old->next;
				--context->sendQueue.pendingCount;
				sceNetPsFreePacket(old);
				++context->txDrops;
			}
			packet->prev = context->sendQueue.pendingTail;
			if (context->sendQueue.pendingTail)
				context->sendQueue.pendingTail->next = packet;
			else
				context->sendQueue.pendingHead = packet;
			packet->next = NULL;
			++queued;
			context->sendQueue.pendingTail = packet;
			++context->sendQueue.pendingCount;
			sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
			if (duplicates <= 0)
				break;
			--duplicates;
		} while (1);
		if (!(mbuf->m_flags & M_SCE_NOAUTOCONNECT) && context->connectionState == 4) {
			struct SceNetPsDriverContext *owner = driver->context;

			if (owner->autoConnectPolicy != -1) {
				owner->autoConnectEnabled = owner->autoConnectPolicy > 0;
				owner->connectionState = 3;
			}
			if (!(gNetPsDriverFlags & 1))
				sceNetPsNotifyDriver(driver, 0x1000000, NULL, 0);
		}
		if (!(mbuf->m_flags & M_SCE_NOIDLERESET))
			resetIdle = 1;
		m_freem(mbuf);
	}
done:
	if (queued && sceNetPsCanTransmit(context, interface))
		sceNetPsKickTransmit(driver, resetIdle);
}

struct pppoe_softc;
struct ifnet *sceNetPsGetPppoeParentInterface(struct pppoe_softc *context);

/* Name unknown; augment the serialized record for physical or virtual links. */
void sceNetPsGetInterfaceInfo(struct ifnet *interface, SceNetPsInterfaceInfo *info)
{
	if (interface->if_type == 23) {
		struct ifnet *parent;

		sceNetPsCollectInterfaceInfo(info, NULL, interface);
		info->driverType = 0;
		memset(&info->hardwareAddress, 0, 8);
		info->driverMtu = interface->if_mtu;
		parent = sceNetPsGetPppoeParentInterface(interface->if_softc);
		if (parent) {
			memcpy(info->parentName, parent->if_xname, 16);
			info->parentInterfaceIndex = parent->if_index;
		}
	} else if (interface->if_type == 24) {
		sceNetPsCollectInterfaceInfo(info, NULL, interface);
		info->driverType = 0;
		memset(&info->hardwareAddress, 0, 8);
		info->driverMtu = interface->if_mtu;
	} else {
		struct SceNetPsDriverContext *context = interface->if_softc;
		struct NetDriver *driver = context->driver;

		sceNetPsCollectInterfaceInfo(info, driver, interface);
		info->driverType = driver->type;
		memcpy(&info->hardwareAddress, &driver->mac, 8);
		info->driverMtu = driver->mtu;
	}
}

/*
 * Name unknown; take an interface down: close its driver unless the system
 * is suspending, drop the configuration subscribers and every socket's cached
 * route.
 */
int sceNetPsCloseInterface(struct SceNetPsDriverContext *context,
			   unsigned int *flags, int suspended)
{
	struct NetDriver *driver = context->driver;
	SceNetPsConfigSubscriber *subscriber;
	struct socket *socket;
	struct inpcb *pcb;
	int result = 0;
	int first, last, error, i;

	*flags &= ~IFF_SCEINET;
	if (!suspended) {
		struct SceNetPsDriverContext *owner = driver->context;
		struct ifnet *interface = owner->interface;

		if (owner->flags & 1) {
			result = driver->close(driver, 0);
			if (result < 0)
				return result;
			owner->flags &= ~1;
			interface->if_flags &= ~IFF_RUNNING;
			interface->if_flags |= IFF_OACTIVE;
		} else {
			/* the driver was not open */
			owner->lastError = 0x80418110;
			result = 0;
			sceNetPsNotifyDriver(driver, 0x4000000, NULL, 0);
		}
	}
	if (context->autoConnectPolicy > 0) {
		if (context->connectionState != 1) {
			context->connectionState = 4;
			context->autoConnectEnabled = 0;
		}
	} else {
		*flags |= IFF_SCERESET;
		sceNetPsNotifyDriver(NULL, 0x10000000, NULL, 0);
		context->connectionState = 0;
		context->autoConnectEnabled = 0;
	}
	while ((subscriber = gNetPsConfigSubscriberHead) != NULL) {
		if (subscriber->next)
			subscriber->next->prev = subscriber->prev;
		else
			gNetPsConfigSubscriberTail = subscriber->prev;
		if (subscriber->prev)
			subscriber->prev->next = subscriber->next;
		else
			gNetPsConfigSubscriberHead = subscriber->next;
		sceNetPsFree(subscriber);
	}
	error = 0;
	sceNetPsGetSocketIdRange(&first, &last);
	for (i = first; i <= last; ++i) {
		socket = sceNetPsLookupSocket(i, &error, 1);
		if (!socket)
			continue;
		if (socket->so_proto && socket->so_proto->pr_domain &&
		    socket->so_proto->pr_domain->dom_family == AF_ROUTE)
			continue;
		pcb = sotoinpcb(socket);
		if (pcb && pcb->inp_route.ro_rt) {
			RTFREE(pcb->inp_route.ro_rt);
			pcb->inp_route.ro_rt = NULL;
		}
	}
	rt_timer_remove_all_queues();
	*flags &= ~(IFF_UP | IFF_RUNNING);
	return result;
}

/* Name unknown; suspend active interfaces while holding the driver lock. */
int sceNetPsSuspendDrivers(void)
{
	struct NetDriver *driver;
	struct ifnet *interface;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsNotifyDriver(NULL, 4, NULL, 0);
	if (gNetPsSocketSuspendPolicy & 1)
		sceNetPsManageProcessSockets(0, 0x1000009);
	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		interface = driver->context->interface;
		if ((interface->if_flags & 0x41) == 0x41)
			sceNetPsCloseInterface(interface->if_softc,
					       &interface->if_flags, 1);
	}
	gNetPsDriverFlags |= 1;
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/* The if_ioctl of the driver interfaces. */
int sceNetPsDriverIoctl(struct ifnet *interface, unsigned long command,
			caddr_t argument)
{
	struct SceNetPsDriverContext *context = interface->if_softc;
	struct NetDriver *driver = context->driver;
	struct ifaddr *address = (struct ifaddr *)argument;
	struct ifreq *request = (struct ifreq *)argument;
	int result;

	switch (command) {
	case SIOCSIFADDR:
		if (address->ifa_addr->sa_family == AF_INET) {
			if (((struct in_ifaddr *)address)->ia_addr.sin_addr.s_addr)
				interface->if_flags |= IFF_SCEINET;
			else
				interface->if_flags &= ~IFF_SCEINET;
			arp_ifinit(interface, address);
		}
		return 0;
	case SIOCSIFFLAGS:
		if (!(interface->if_flags & IFF_UP)) {
			if ((interface->if_flags & IFF_SCEINET) &&
			    (gNetPsSocketSuspendPolicy & 1))
				sceNetPsManageProcessSockets(0, 0x1000009);
			return sceNetPsCloseInterface(interface->if_softc,
						      &interface->if_flags, 0);
		}
		if (interface->if_flags & IFF_SCERESET) {
			interface->if_flags &= ~IFF_SCERESET;
			++context->generation;
			memset(&driver->txBytes, 0, 64);
			memset(&context->uptime, 0, sizeof(context->uptime));
			memset(&context->rxPackets, 0, 32);
		}
		context = driver->context;
		if (!(context->flags & 1)) {
			result = driver->open(driver, 0);
			if (result < 0)
				return result;
			context->flags |= 1;
		}
		return 0;
	case SIOCADDMULTI:
		ether_addmulti(request, (struct ethercom *)context->interface);
		return sceNetPsUpdateDriverMulticast(driver);
	case SIOCDELMULTI:
		ether_delmulti(request, (struct ethercom *)context->interface);
		return sceNetPsUpdateDriverMulticast(driver);
	case SIOCSIFMTU:
		if ((unsigned int)request->ifr_mtu > driver->mtu)
			return EINVAL;
		interface->if_mtu = request->ifr_mtu;
		if (driver->capabilities & 0x10000000) {
			sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
			driver->control(driver, NET_DRIVER_CTL_SET_MTU,
					(int *)&interface->if_mtu,
					sizeof(interface->if_mtu));
			sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
		}
		return 0;
	case SIOCGIFSCEDRV:
		request->ifr_addr.sa_len = ETHER_ADDR_LEN;
		request->ifr_addr.sa_family = AF_LINK;
		memcpy(request->ifr_addr.sa_data, &driver->mac, ETHER_ADDR_LEN);
		return 0;
	}
	return EINVAL;
}

/* SCE: publish periodic events and expire idle interfaces. */
int sceNetPsPollDevices(void)
{
	struct NetDriver *driver;

	if (ticks % 60000 == 0)
		sceNetPsNotifyDriver(NULL, 0x10000000, NULL, 0);
	if (ticks % 1000 != 0)
		return 0;
	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		struct SceNetPsDriverContext *context = driver->context;

		if ((context->interface->if_flags & 0x41) != 0x41)
			continue;
		++context->uptime.seconds;
		if (context->autoConnectPolicy <= 0)
			continue;
		if (++context->idle.seconds < context->idleTimeoutSeconds)
			continue;
		context = driver->context;
		if ((unsigned int)(context->connectionState - 3) > 1)
			continue;
		if (context->autoConnectPolicy != -1) {
			context->connectionState = 4;
			context->autoConnectEnabled = 0;
		}
		sceNetPsNotifyDriver(driver, 0x2000000, NULL, 0);
		memset(&context->idle, 0, sizeof(context->idle));
	}
	return 0;
}

/*
 * SCE: ask the preferred driver to connect for a socket's traffic. A driver
 * qualifies when it has callbacks and is connected or idle-disconnected, or is
 * disconnected under the manual policy; the lowest policy wins. With
 * checkStates nothing is asked unless every driver is disconnected (0),
 * connected (1) or idle-disconnected (4).
 */
int sceNetPsRequestAutoConnect(struct socket *so, struct mbuf *m,
			       int checkStates)
{
	struct NetDriver *driver = gNetPsDriverHead;
	struct NetDriver *best = NULL;
	struct SceNetPsDriverContext *context;
	int requested = 0, lowest = 99, policy;
	unsigned int flags;

	if (so && (so->so_options & (SO_SCE_OPTION_10000 | SO_SCE_OPTION_20000)))
		return 0;
	if (checkStates) {
		struct NetDriver *cursor;

		for (cursor = driver; cursor; cursor = cursor->next) {
			int state = cursor->context->connectionState;

			if (state != 0 && state != 1 && state != 4)
				return 0;
		}
	}
	if (!driver)
		return 0;
	for (; driver; driver = driver->next) {
		context = driver->context;
		if (!sceNetPsHasDriverCallbacks(context))
			continue;
		if (context->connectionState == 1 || context->connectionState == 4) {
			policy = context->autoConnectPolicy;
		} else if (context->connectionState == 0 && !best &&
			   context->autoConnectPolicy == -1) {
			policy = -1;
		} else {
			continue;
		}
		requested = 1;
		if (policy < lowest) {
			lowest = policy;
			best = driver;
		}
		if (so) {
			so->so_snd.sb_flags &= ~SB_SCE_CONNECTED;
			so->so_snd.sb_flags |= SB_SCE_CONNECTING;
			sceNetPsRecordSleep(so->processId, 0);
			so->so_error = 0;
		}
	}
	if (!best)
		return requested;
	flags = gNetPsDriverFlags;
	context = best->context;
	if (flags & 2)
		return 0;
	context->loopbackRequested = 0;
	if (m && ntohl(mtod(m, struct sockaddr_in *)->sin_addr.s_addr) >> 24 ==
	    IN_LOOPBACKNET)
		context->loopbackRequested = 1;
	if (!context->connectionState) {
		context = best->context;
		context->flags |= 2;
		if (flags & 1)
			context->flags |= 8;
		else
			sceNetPsNotifyDriver(best, 0x8000000, NULL, 0);
		return requested;
	}
	context = best->context;
	if (context->autoConnectPolicy != -1) {
		context->autoConnectEnabled = context->autoConnectPolicy > 0;
		context->connectionState = 1;
	}
	if (flags & 1)
		context->flags |= 4;
	else
		sceNetPsNotifyDriver(best, 0x1000000, NULL, 0);
	return requested;
}

/* SCE: queue a request until the automatic connection completes or fails. */
int sceNetPsQueuePendingRequest(SceNetPsPendingRequest *request)
{
	request->prev = gNetPsPendingRequestTail;
	if (gNetPsPendingRequestTail)
		gNetPsPendingRequestTail->next = request;
	else
		gNetPsPendingRequestHead = request;
	request->next = NULL;
	gNetPsPendingRequestTail = request;
	return 0;
}

/*
 * SCE: sleep until a connecting driver's automatic connection ends or its
 * timeout passes; a failed connection returns the driver's connection error.
 */
int sceNetPsWaitAutoConnect(struct socket *so)
{
	const void *waitChannel = &so->emulationWaitChannel;
	struct NetDriver *driver;
	struct SceNetPsDriverContext *context;
	SceUInt64 elapsed, now;
	int error = 0, delay;

again:
	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		context = driver->context;
		if (context->connectionState == 1 || (context->flags & 2))
			break;
	}
	if (!driver)
		return error;
	if (context->connectTimeoutSeconds < 0)
		delay = -1;
	else
		delay = 1000 * context->connectTimeoutSeconds * hz;
	for (elapsed = 0;; elapsed = now - elapsed) {
		if (delay > 0) {
			if (elapsed >= delay)
				break;
			delay -= elapsed;
		}
		if (!delay)
			break;
		/* the start of the sleep */
		sceNetPsGetSystemTime(&elapsed);
		error = sceNetPsSleep(waitChannel, 0, NULL, delay);
		if (error != EWOULDBLOCK) {
			if (error == SCE_NET_EIPADDRCHANGED || error == EINTR ||
			    error == SCE_NET_ECANCELED)
				return error;
			error = context->connectionError;
			if (!context->connectionState &&
			    context->autoConnectPolicy != -1)
				goto again;
			return error;
		}
		sceNetPsGetSystemTime(&now);
	}
	return error;
}

/* SCE: wake this socket's emulation waiters once for each registered driver. */
int sceNetPsInterruptEmulationWait(struct socket *socket)
{
	struct NetDriver *driver = gNetPsDriverHead;
	int result = 15;

	if (driver) {
		const void *waitChannel = &socket->emulationWaitChannel;

		do {
			int error = sceNetPsWakeupInterrupted(waitChannel);

			driver = driver->next;
			if (!error)
				result = 0;
		} while (driver);
	}
	return result;
}

/* SCE: cancel this socket's emulation waits once for each registered driver. */
int sceNetPsCancelEmulationWait(struct socket *socket)
{
	struct NetDriver *driver = gNetPsDriverHead;
	int result = 15;

	if (driver) {
		const void *waitChannel = &socket->emulationWaitChannel;

		do {
			int error = sceNetPsWakeupCanceled(waitChannel);

			driver = driver->next;
			if (!error)
				result = 0;
		} while (driver);
	}
	return result;
}

/* SCE: hand a driver's received packets to its interface as mbufs. */
static void sceNetPsInputPackets(struct NetDriver *driver)
{
	struct SceNetPsDriverContext *context = driver->context;
	struct ifnet *interface = context->interface;
	struct NetDriverPacket *packet;

	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	while ((packet = sceNetPsDequeuePacket(context, &context->receiveQueue,
					       0, 1)) != NULL) {
		struct SceNetPsDriverContext *owner;
		struct ifnet *receiver;
		struct mbuf *m;
		unsigned int length;
		int freeMemory;

		if ((interface->if_flags & (IFF_UP | IFF_RUNNING)) !=
		    (IFF_UP | IFF_RUNNING)) {
			++context->rxDrops;
			sceNetPsFreePacket(packet);
			continue;
		}
		owner = driver->context;
		length = packet->length;
		receiver = owner->interface;
		if (gNetPsPacketCaptureActive)
			sceNetPsCaptureTxPacket(packet);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (!m)
			goto drop;
		sceNetPsFilterRxMbuf(owner, m, length);
		freeMemory = sceNetPsGetFreeMemory(0);
		if (freeMemory < 0x8000) {
			m_freem(m);
			goto drop;
		}
		/* short of memory: sockets that ask for it drop the packet */
		if (freeMemory < 0x10000)
			m->m_flags |= M_SCE_OPTION_8000;
		else if (freeMemory < 102400 &&
			 (int)(arc4random() % 101) >
			 100 * (freeMemory - 0x10000) / 36864)
			m->m_flags |= M_SCE_OPTION_8000;
		MEXTADD(m, packet->buffer->data, length, M_MBUF,
			sceNetPsReleaseRxMbuf, packet);
		m->m_pkthdr.len = m->m_len = length;
		m->m_pkthdr.rcvif = receiver;
		m->m_pkthdr.csum_flags = 0;
		if ((receiver->if_csum_flags_rx & M_CSUM_IPv4) &&
		    (packet->checksumFlags & 1))
			m->m_pkthdr.csum_flags = M_CSUM_IPv4;
		if ((receiver->if_csum_flags_rx & (M_CSUM_UDPv4 | M_CSUM_UDPv6)) &&
		    (packet->checksumFlags & 2))
			m->m_pkthdr.csum_flags |= M_CSUM_UDPv4 | M_CSUM_UDPv6;
		if ((receiver->if_csum_flags_rx & (M_CSUM_TCPv4 | M_CSUM_TCPv6)) &&
		    (packet->checksumFlags & 4))
			m->m_pkthdr.csum_flags |= M_CSUM_TCPv4 | M_CSUM_TCPv6;
		++owner->rxPackets;
		owner->rxBytes += length;
		++gNetPsQosPacketCount;
		(*receiver->if_input)(receiver, m);
		continue;
drop:
		++context->rxDrops;
		sceNetPsFreePacket(packet);
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
}

/* SCE: complete (or cancel) a pending request and drop it from the queue. */
static inline void sceNetPsCompletePendingRequest(SceNetPsPendingRequest *request,
						  int cancel)
{
	request->complete(request->argument, cancel);
	if (request->next)
		request->next->prev = request->prev;
	else
		gNetPsPendingRequestTail = request->prev;
	if (request->prev)
		request->prev->next = request->next;
	else
		gNetPsPendingRequestHead = request->next;
	sceNetPsFree(request);
}

/*
 * SCE: apply a driver's new connection state: run the requests that waited
 * for it (cancel them when the interface has no address), and wake the
 * sockets that waited, with the connection error if there is one.
 */
static void sceNetPsApplyConnection(struct NetDriver *driver)
{
	struct SceNetPsDriverContext *context = driver->context;
	struct ifnet *interface = context->interface;
	SceNetPsPendingRequest *request, *last;
	struct ifaddr *address;
	struct socket *so;
	int first, last_, error, connectionError, i;

	if ((unsigned int)(context->connectionState - 2) > 1 && !(context->flags & 2))
		return;
	TAILQ_FOREACH(address, &interface->if_addrlist, ifa_list) {
		if (address->ifa_addr->sa_family == AF_INET)
			break;
	}
	if (context->loopbackRequested) {
		gNetPsDriverFlags |= 2;
		while ((request = gNetPsPendingRequestHead) != NULL)
			sceNetPsCompletePendingRequest(request, 0);
		gNetPsDriverFlags &= ~2;
		if (context->autoConnectPolicy != -1) {
			context->connectionState = 4;
			context->autoConnectEnabled = 0;
		}
	} else if ((interface->if_flags & (IFF_UP | IFF_RUNNING)) !=
		   (IFF_UP | IFF_RUNNING) || !address ||
		   !((struct in_ifaddr *)address)->ia_addr.sin_addr.s_addr) {
		while ((request = gNetPsPendingRequestHead) != NULL)
			sceNetPsCompletePendingRequest(request, 1);
		if (context->autoConnectPolicy != -1) {
			context->connectionState = 4;
			context->autoConnectEnabled = 0;
		}
	} else {
		/* run the requests queued so far; cancel those they queue */
		last = gNetPsPendingRequestTail;
		gNetPsDriverFlags |= 2;
		while ((request = gNetPsPendingRequestHead) != NULL) {
			sceNetPsCompletePendingRequest(request, 0);
			if (request == last)
				break;
		}
		while ((request = gNetPsPendingRequestHead) != NULL)
			sceNetPsCompletePendingRequest(request, 1);
		gNetPsDriverFlags &= ~2;
		if (context->autoConnectPolicy != -1) {
			context->autoConnectEnabled = context->autoConnectPolicy > 0;
			context->connectionState = 3;
		}
	}
	memset(&context->idle, 0, sizeof(context->idle));
	error = 0;
	connectionError = context->connectionError;
	sceNetPsGetSocketIdRange(&first, &last_);
	for (i = first; i <= last_; ++i) {
		so = sceNetPsLookupSocketUnchecked(i, &error);
		if (!so || !(so->so_snd.sb_flags & SB_SCE_CONNECTING))
			continue;
		so->so_snd.sb_flags &= ~SB_SCE_CONNECTING;
		so->so_snd.sb_flags |= SB_SCE_CONNECTED;
		if (connectionError) {
			so->so_error = SCE_NET_ERETURN;
			so->returnedError = connectionError;
		}
		sceNetPsWakeupEpolls(so);
		sceNetPsWakeupNormal(&so->emulationWaitChannel);
	}
	sceNetPsRecordSleep(0, 1);
	context->flags &= ~2;
}

/* SCE: the soft thread's handling of every driver's pending events. */
void sceNetPsReceivePackets(void)
{
	struct NetDriver *driver;

	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		struct SceNetPsDriverContext *context = driver->context;

		if (context->pendingEvents & NET_PS_EVENT_STARTED) {
			struct ifnet *interface = context->interface;

			interface->if_flags &= ~IFF_OACTIVE;
			interface->if_flags |= IFF_RUNNING;
			/* start with an empty multicast list */
			if (driver->multicast)
				driver->multicast(driver, NULL, 0);
			sceNetPsNotifyDriver(driver, 0x20000000, NULL, 0);
			context->pendingEvents &= ~NET_PS_EVENT_STARTED;
		}
		if (context->pendingEvents & NET_PS_EVENT_STOPPED) {
			sceNetPsNotifyDriver(driver, 0x40000000, NULL, 0);
			context->pendingEvents &= ~NET_PS_EVENT_STOPPED;
		}
		if (context->pendingEvents & NET_PS_EVENT_RECEIVED) {
			sceNetPsInputPackets(driver);
			context->pendingEvents &= ~NET_PS_EVENT_RECEIVED;
		}
		if (context->pendingEvents & NET_PS_EVENT_ERROR) {
			sceNetPsNotifyDriver(driver, 0x4000000, NULL, 0);
			context->pendingEvents &= ~NET_PS_EVENT_ERROR;
		}
		if (context->pendingEvents &
		    NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0x10)) {
			sceNetPsNotifyDriver(driver, 0x80000000, NULL, 0);
			context->pendingEvents &=
				~NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0x10);
		}
		if (context->pendingEvents &
		    NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0xc0)) {
			unsigned int events = 0;

			if (context->pendingEvents &
			    NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0x40))
				events = 0x20;
			if (context->pendingEvents &
			    NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0x80))
				events |= 0x40;
			sceNetPsNotifyDriver(NULL, events, NULL, 0);
			context->pendingEvents &=
				~NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS, 0xc0);
		}
		if (context->pendingEvents & NET_PS_EVENT_CONNECTION) {
			sceNetPsApplyConnection(driver);
			context->pendingEvents &= ~NET_PS_EVENT_CONNECTION;
			sceNetPsScheduleReceive();
		}
	}
}

/* SCE: remember a process that sent data, sixteen at most. */
int sceNetPsRecordSender(SceUID processId)
{
	int i;

	for (i = 0; i < gNetPsSenders.count; ++i) {
		if (gNetPsSenders.processId[i] == processId)
			return 0;
	}
	if (gNetPsSenders.count < 16)
		gNetPsSenders.processId[gNetPsSenders.count++] = processId;
	return 0;
}

/* SCE: take the processes that sent data and start a new list. */
int sceNetPsTakeSenders(SceNetPsSenderList *list)
{
	memcpy(list, &gNetPsSenders, sizeof(*list));
	memset(&gNetPsSenders, 0, sizeof(gNetPsSenders));
	return 0;
}

/* Name unknown; serialize the interface record for host or network consumers. */
int sceNetPsSerializeInterfaceInfo(struct ifnet *interface,
				 SceNetPsInterfaceInfo *info, int networkOrder)
{
	sceNetPsGetInterfaceInfo(interface, info);
	if (networkOrder) {
		info->interfaceIndex = __builtin_bswap32(info->interfaceIndex);
		info->parentInterfaceIndex = __builtin_bswap32(info->parentInterfaceIndex);
		info->flags = __builtin_bswap32(info->flags);
		info->driverType = __builtin_bswap32(info->driverType);
		info->mtu = __builtin_bswap32(info->mtu);
		info->driverMtu = __builtin_bswap32(info->driverMtu);
		sceNetPsSwapCounterByteOrder(info->txCounters);
		sceNetPsSwapCounterByteOrder(info->rxCounters);
		sceNetPsSwapCounterByteOrder(info->softwareTxCounters);
		sceNetPsSwapCounterByteOrder(info->softwareRxCounters);
		info->connectionState = __builtin_bswap32(info->connectionState);
		info->idleSeconds = __builtin_bswap32(info->idleSeconds);
		memset(info->privateControls, 0, sizeof(info->privateControls));
		memset(info->promiscuousControls, 0, sizeof(info->promiscuousControls));
		info->linkStatus = __builtin_bswap32(info->linkStatus);
		info->emulationSeedPresent = __builtin_bswap32(info->emulationSeedPresent);
	} else {
		SceNetPsInterfaceAddress *address = info->addresses;
		int count = 2;

		do {
			address->address = __builtin_bswap32(address->address);
			address->broadcastAddress = __builtin_bswap32(address->broadcastAddress);
			address->broadcastAddressCopy = __builtin_bswap32(address->broadcastAddressCopy);
			address->subnetMask = __builtin_bswap32(address->subnetMask);
			++address;
		} while (--count);
	}
	return 0;
}

/* Name unknown; enumerate complete interface records or count the interfaces. */
int sceNetPsGetInterfaceList(SceNetPsInterfaceInfo *records, int capacity)
{
	struct ifnet *interface = TAILQ_FIRST(&ifnet);
	int count = 0;

	if (!interface)
		return 0;
	do {
		if (records) {
			if (capacity > count) {
				SceNetPsInterfaceInfo *record = records;

				++count;
				++records;
				sceNetPsGetInterfaceInfo(interface, record);
			}
		} else {
			++count;
		}
		interface = TAILQ_NEXT(interface, if_list);
	} while (interface);
	return count;
}

int sceNetPsHasPendingDriverEvents(SceUInt64 events)
{
	struct NetDriver *driver;

	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		if (sceNetPsDriverHasPendingEvents(driver->context, events))
			return 1;
	}
	return 0;
}

/*
 * SCE: sceNetControl on one interface: the driver's own commands (bit 30)
 * go to its control entry point without the stack lock, the rest to the
 * stack's handling for that interface.
 */
int sceNetPsInterfaceControl(int interfaceIndex, int command, void *data,
			     int length)
{
	struct NetDriver *driver;
	struct SceNetPsDriverContext *context;
	int error = -EIO, result;

	for (driver = gNetPsDriverHead; driver; driver = driver->next) {
		context = driver->context;
		if (context->interface->if_index == interfaceIndex)
			break;
	}
	if (!driver)
		return -EIO;
	if (!(command & 0x40000000))
		return sceNetPsStackControl(context, command, data, length);
	if (sceNetPsIsApplicationProgram(&error))
		return error;
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	result = driver->control(driver, command, data, length);
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	return result;
}

/* SCE: bounded debug lookup, separate from upstream ifunit's index lookup. */
struct ifnet *sceNetPsFindInterface(const char *name)
{
	struct ifnet *interface;

	TAILQ_FOREACH(interface, &ifnet, if_list) {
		if (!strncmp(interface->if_xname, name, IFNAMSIZ))
			return interface;
	}
	return NULL;
}

/* SCE: mark a packet of a socket that must not start a connection. */
int sceNetPsSetNoAutoConnect(struct mbuf *m, struct ifnet *ifp,
			     struct socket *so)
{
	if (so && (ifp->if_flags & IFF_RUNNING) &&
	    (so->so_options & SO_SCE_OPTION_10000))
		m->m_flags |= M_SCE_NOAUTOCONNECT;
	return 0;
}

/* SCE: whether a socket, its process or its thread disables an interface type */
static inline int sceNetPsIsInterfaceDisabled(struct SceNetPsDriverContext *context,
					      struct socket *so, int processFlags,
					      int threadFlags, unsigned int bit)
{
	return ((context->instanceMask & 1) && (so->filterFlags & bit)) ||
	       (processFlags & bit) || (threadFlags & bit);
}

/*
 * SCE: refuse a packet on an interface its socket, process or thread has
 * disabled; the bit for each driver type is the same in all three masks.
 */
int sceNetPsCheckDisabledInterface(struct mbuf *m, struct ifnet *ifp,
				   struct socket *so)
{
	struct SceNetPsDriverContext *context = ifp->if_softc;
	struct NetDriver *driver;
	int processFlags, threadFlags;
	SceUID threadId;

	if (!context)
		return 0;
	driver = context->driver;
	if (!so)
		return 0;
	sceNetPsGetProcessError(so->processId, &processFlags);
	if (sceNetPsIsSoftThread()) {
		threadId = so->initiatingThreadId;
	} else {
		threadId = sceNetPsGetThreadId();
		so->initiatingThreadId = threadId;
	}
	sceNetPsGetThreadError(threadId, &threadFlags);
	switch (driver->type) {
	case NET_DRIVER_TYPE_ETHER:
		/* the fake 3G interface is an Ethernet or wireless LAN one */
		if (sceNetPsGetFake3gInterface())
			goto modem;
		if (sceNetPsIsInterfaceDisabled(context, so, processFlags,
						threadFlags, 1))
			return -SCE_NET_EDISABLEDIF;
		return 0;
	case 2:
		if (sceNetPsGetFake3gInterface())
			goto modem;
		if (sceNetPsIsInterfaceDisabled(context, so, processFlags,
						threadFlags, 2))
			return -SCE_NET_EDISABLEDIF;
		return 0;
	case 3:
		if (sceNetPsIsInterfaceDisabled(context, so, processFlags,
						threadFlags, 4))
			return -SCE_NET_EDISABLEDIF;
		return 0;
	case 4:
modem:
		if (sceNetPsIsInterfaceDisabled(context, so, processFlags,
						threadFlags, 8))
			return -SCE_NET_EDISABLEDIF;
		return 0;
	default:
		return 0;
	}
}

/* Name unknown; select socket handling when an interface is suspended. */
int sceNetPsSetSocketSuspendPolicy(unsigned int policy)
{
	gNetPsSocketSuspendPolicy = policy;
	return 0;
}

/* SCE: querying DNS records subscribes the process to configuration changes. */
int sceNetPsGetDnsConfig(SceNetPsDnsConfig *config, int networkOrder,
			int addressesOnly)
{
	SceNetPsConfigSubscriber *subscriber;
	SceUID processId;

	memcpy(config, &gNetPsDnsConfig, sizeof(*config));
	if (networkOrder) {
		config->flags = __builtin_bswap32(config->flags);
		if (addressesOnly)
			return 0;
	} else if (addressesOnly) {
		unsigned int primary = config->dns.dns_addr[0].s_addr;
		unsigned int secondary = config->dns.dns_addr[1].s_addr;

		config->dns.dns_addr[0].s_addr = __builtin_bswap32(primary);
		config->dns.dns_addr[1].s_addr = __builtin_bswap32(secondary);
		return 0;
	}
	processId = sceNetPsGetProcessId();
	for (subscriber = gNetPsConfigSubscriberHead; subscriber;
	     subscriber = subscriber->next) {
		if (subscriber->processId == processId)
			return 0;
	}
	subscriber = sceNetPsMalloc(sizeof(*subscriber), 3, 8);
	if (!subscriber)
		return 0;
	subscriber->processId = processId;
	subscriber->prev = gNetPsConfigSubscriberTail;
	if (subscriber->prev)
		subscriber->prev->next = subscriber;
	else
		gNetPsConfigSubscriberHead = subscriber;
	subscriber->next = NULL;
	gNetPsConfigSubscriberTail = subscriber;
	config->flags |= 1;
	return 0;
}

/* SCE: save the complete configuration and notify the registered gateway. */
int sceNetPsSetDnsConfig(const SceNetPsDnsConfig *config)
{
	memcpy(&gNetPsDnsConfig, config, sizeof(*config));
	sceNetPsNotifyGatewayDns(config);
	return 0;
}

/* SCE: report the complete connection policy and its current state. */
int sceNetPsGetConnectionParam(struct SceNetPsDriverContext *context,
			       SceNetPsConnectionParam *parameters)
{
	parameters->connectTimeoutSeconds = context->connectTimeoutSeconds;
	parameters->autoConnectEnabled = context->autoConnectEnabled;
	parameters->idleTimeoutSeconds = context->idleTimeoutSeconds;
	parameters->autoConnectPolicy = context->autoConnectPolicy;
	parameters->loopbackRequested = context->loopbackRequested;
	return 0;
}

/* SCE: select automatic connection or hand the active connection to a peer. */
int sceNetPsSetConnectionParam(struct SceNetPsDriverContext *context,
			       const SceNetPsConnectionParam *parameters)
{
	struct ifnet *interface;
	struct ifaddr *address;
	struct NetDriver *driver;
	struct SceNetPsDriverContext *peer;
	int timeout = parameters->idleTimeoutSeconds;
	int policy;

	if (timeout < 0 || (unsigned int)parameters->autoConnectPolicy + 1 > 100)
		return -22;
	if (parameters->connectTimeoutSeconds) {
		context->connectTimeoutSeconds = parameters->connectTimeoutSeconds;
		timeout = parameters->idleTimeoutSeconds;
	}
	if (timeout)
		context->idleTimeoutSeconds = timeout;
	policy = parameters->autoConnectPolicy;
	interface = context->interface;
	context->autoConnectPolicy = policy;
	if (policy > 0) {
		TAILQ_FOREACH(address, &interface->if_addrlist, ifa_list) {
			if (address->ifa_addr->sa_family == AF_INET)
				break;
		}
		if (address &&
		    (interface->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING) &&
		    ((struct in_ifaddr *)address)->ia_addr.sin_addr.s_addr) {
			memset(&context->idle, 0, sizeof(context->idle));
			policy = context->autoConnectPolicy;
			if (policy != -1) {
				context->autoConnectEnabled = policy > 0;
				context->connectionState = 3;
			}
		} else if (!context->connectionState) {
			context->autoConnectEnabled = 0;
			context->connectionState = 4;
		}
		return 0;
	}
	interface->if_flags |= IFF_SCERESET;
	sceNetPsNotifyDriver(NULL, 0x10000000, NULL, 0);
	if (context->connectionState == 1) {
		context->connectionState = 0;
		context->autoConnectEnabled = 0;
		for (driver = gNetPsDriverHead; driver; driver = driver->next) {
			peer = driver->context;
			if (peer->connectionState != 1 && peer->connectionState != 4)
				continue;
			if (context->loopbackRequested)
				peer->loopbackRequested = 1;
			policy = peer->autoConnectPolicy;
			if (policy != -1) {
				peer->autoConnectEnabled = policy > 0;
				peer->connectionState = 1;
			}
			return 0;
		}
		return 0;
	}
	context->connectionState = 0;
	context->autoConnectEnabled = 0;
	return 0;
}

/* SCE: expose whether promiscuous reception is held and its reference count. */
int sceNetPsGetPromiscuous(struct SceNetPsDriverContext *context,
			 SceNetPsPromiscuousParam *parameters)
{
	parameters->enabled = context->promiscuousEnabled;
	parameters->references = context->promiscuousReferences;
	return 0;
}

/* SCE: each enable acquires a reference, and the final release clears IFF_PPROMISC. */
int sceNetPsSetPromiscuous(struct SceNetPsDriverContext *context,
			 const SceNetPsPromiscuousParam *parameters)
{
	struct ifnet *interface = context->interface;
	int enabled;

	if (parameters->references)
		return -22;
	enabled = parameters->enabled;
	if (!enabled) {
		if (!context->promiscuousEnabled)
			return -77;
		--context->promiscuousReferences;
		if (context->promiscuousReferences <= 0) {
			context->promiscuousEnabled = parameters->enabled;
			interface->if_flags &= ~IFF_PPROMISC;
		}
		return enabled;
	}
	if (enabled == 1) {
		++context->promiscuousReferences;
		context->promiscuousEnabled = parameters->enabled;
		interface->if_flags |= IFF_PPROMISC;
		return 0;
	}
	return -22;
}

extern void *gNetPsMemoryBase;
extern int gNetPsMemorySize;
extern struct SceNetPsDriverContext *gNetPsAllocatedDriverContext;
void if_attach(struct ifnet *interface);
void ether_ifattach(struct ifnet *interface, const unsigned char *address);

/* Export 1ABF937D: register the driver's complete interface and context. */
int SceNetPsForDriver_1ABF937D(void *argument)
{
	struct NetDriver *driver = argument;
	struct NetDriver *cursor;
	struct SceNetPsDriverContext *context, *saved;
	struct ethercom *ethernet;
	struct ifnet *interface;
	unsigned int instances = 0, capabilities, mtu;
	int instance, index, policy, state, result = 0;

	if (!driver || !driver->open || !driver->close || !driver->kickTx ||
	    !driver->control || (driver->prefix & 3))
		return (int)0x80418101;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	for (cursor = gNetPsDriverHead; cursor; cursor = cursor->next) {
		if (cursor == driver)
			goto unlock;
	}
	memset(&driver->allocate, 0,
	       offsetof(struct NetDriver, stop) - offsetof(struct NetDriver, allocate));
	for (cursor = gNetPsDriverHead; cursor; cursor = cursor->next) {
		if (cursor->type == driver->type)
			instances |= cursor->context->instanceMask;
	}
	instance = 0;
	while (instances & (1U << instance)) {
		if (++instance == 8) {
			result = (int)0x80418101;
			goto unlock;
		}
	}
	if (driver->stop) {
		result = driver->stop(driver, gNetPsMemoryBase, gNetPsMemorySize);
		if (result < 0)
			goto unlock;
	}
	driver->allocate = sceNetPsMallocForDriver;
	driver->free = sceNetPsFreeForDriver;
	driver->allocateRx = sceNetPsAllocateRxPacket;
	driver->appendBuffer = sceNetPsAppendBuffer;
	driver->releaseTx = sceNetPsReleaseTxPacket;
	driver->deliverRx = sceNetPsDeliverRxPacket;
	driver->dequeueTx = sceNetPsDequeueTxPacket;
	driver->notify = sceNetPsNotifyDriver;
	driver->setMtu = sceNetPsSetDriverMtu;
	context = sceNetPsMalloc(sizeof(*context), 3, 8);
	if (!context)
		goto failed;
	ethernet = sceNetPsMalloc(sizeof(*ethernet), 3, 8);
	if (!ethernet) {
		sceNetPsFree(context);
		goto failed;
	}
	interface = &ethernet->ec_if;
	driver->context = context;
	context->interface = interface;
	context->driver = driver;
	context->instanceMask = 1U << instance;
	interface->if_softc = context;
	mtu = driver->mtu;
	interface->if_start = sceNetPsStartTransmit;
	interface->if_ioctl = sceNetPsDriverIoctl;
	interface->if_watchdog = NULL;
	interface->if_flags = 34818;
	interface->if_mtu = mtu < 1500 ? mtu : 1500;
	capabilities = driver->capabilities;
	if (capabilities & 0x100000)
		interface->if_csum_flags_tx |= 0x11;
	if (capabilities & 0x40000)
		interface->if_csum_flags_tx |= 0x22;
	if (capabilities & 0x10000)
		interface->if_csum_flags_tx |= 0x40;
	if (capabilities & 0x200000)
		interface->if_csum_flags_rx |= 0x11;
	if (capabilities & 0x80000)
		interface->if_csum_flags_rx |= 0x22;
	if (capabilities & 0x20000)
		interface->if_csum_flags_rx |= 0x40;
	switch (driver->type) {
	case 1:
		strncpy(interface->if_xname, "eth*", sizeof(interface->if_xname));
		context->autoConnectPolicy = 1;
		break;
	case 2:
		strncpy(interface->if_xname, "wlan*", sizeof(interface->if_xname));
		context->autoConnectPolicy = 1;
		break;
	case 3:
		strncpy(interface->if_xname, "bt*", sizeof(interface->if_xname));
		context->autoConnectPolicy = 0;
		break;
	case 4:
		strncpy(interface->if_xname, "phone*", sizeof(interface->if_xname));
		context->autoConnectPolicy = 0;
		break;
	default:
		strncpy(interface->if_xname, "other*", sizeof(interface->if_xname));
		context->autoConnectPolicy = 0;
		break;
	}
	interface->if_xname[sizeof(interface->if_xname) - 1] = 0;
	for (index = 0; index < sizeof(interface->if_xname); ++index) {
		if (((signed char *)interface->if_xname)[index] == '*') {
			interface->if_xname[index] = instance + '0';
			break;
		}
	}
	context->idleTimeoutSeconds = 60;
	context->connectTimeoutSeconds = -1;
	if (context->autoConnectPolicy > 0) {
		context->connectionState = 4;
		context->autoConnectEnabled = 0;
	}
	if (driver->type == 1 && gNetPsAllocatedDriverContext) {
		context->generation = gNetPsAllocatedDriverContext->generation + 1;
		sceNetPsNotifyDriver(NULL, 0x10000000, NULL, 0);
		saved = gNetPsAllocatedDriverContext;
		if (saved->flags & 4) {
			policy = saved->autoConnectPolicy;
			context->autoConnectPolicy = policy;
			state = saved->connectionState;
			if (policy != -1) {
				context->connectionState = state;
				context->autoConnectEnabled = state != 0 && state != 4 && policy > 0;
			} else if (!state) {
				context->connectionState = 0;
				context->autoConnectEnabled = 0;
			}
			sceNetPsNotifyDriver(driver, 0x1000000, NULL, 0);
			saved = gNetPsAllocatedDriverContext;
		}
		memset(saved, 0, sizeof(*saved));
	} else if (driver->type == 4 && gNetPsAllocatedDriverContext) {
		context->generation = gNetPsAllocatedDriverContext->generation + 1;
		sceNetPsNotifyDriver(NULL, 0x10000000, NULL, 0);
	}
	if_attach(interface);
	ether_ifattach(interface, driver->mac.data);
	context->hardwareAddress = (unsigned char *)LLADDR(interface->if_sadl);
	driver->prev = gNetPsDriverTail;
	if (gNetPsDriverTail)
		gNetPsDriverTail->next = driver;
	else
		gNetPsDriverHead = driver;
	driver->next = NULL;
	gNetPsDriverTail = driver;
	sceNetPsNotifyDriver(NULL, 1, NULL, 0);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
failed:
	result = (int)0x80418102;
	if (driver->start)
		driver->start(driver, gNetPsMemoryBase);
unlock:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

void ether_ifdetach(struct ifnet *interface);
void if_detach(struct ifnet *interface);

/* Export F5788FF8: remove the driver's complete interface and queued packets. */
int SceNetPsForDriver_F5788FF8(void *argument)
{
	struct NetDriver *driver = argument, *cursor;
	struct SceNetPsDriverContext *context, *receiveContext;
	struct ifnet *interface;
	struct NetDriverPacket *packet;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	for (cursor = gNetPsDriverHead; cursor; cursor = cursor->next) {
		if (cursor == driver)
			break;
	}
	if (!cursor) {
		result = (int)0x80418101;
		goto unlock;
	}
	sceNetPsStopDriverEmulation(driver);
	sceNetPsNotifyDriver(NULL, 2, NULL, 0);
	if (driver->next)
		driver->next->prev = driver->prev;
	else
		gNetPsDriverTail = driver->prev;
	if (driver->prev)
		driver->prev->next = driver->next;
	else
		gNetPsDriverHead = driver->next;
	context = driver->context;
	interface = context->interface;
	while ((packet = sceNetPsDequeuePacket(context, &context->sendQueue, 1, 0)))
		sceNetPsFreePacket(packet);
	receiveContext = driver->context;
	while ((packet = sceNetPsDequeuePacket(receiveContext,
					     &context->receiveQueue, 0, 0)))
		sceNetPsFreePacket(packet);
	if ((driver->type == 1 || driver->type == 4) && gNetPsAllocatedDriverContext)
		memcpy(gNetPsAllocatedDriverContext, context, sizeof(*context));
	ether_ifdetach(interface);
	if_detach(interface);
	interface->if_softc = NULL;
	sceNetPsFree(context->interface);
	sceNetPsFree(context);
	driver->context = NULL;
	if (driver->start)
		driver->start(driver, gNetPsMemoryBase);
	memset(&driver->allocate, 0,
	       offsetof(struct NetDriver, stop) - offsetof(struct NetDriver, allocate));
	result = 0;
unlock:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsSuspendDrivers(void);

/* Name unknown; connect the complete network-owned platform hook records. */
int sceNetPsInitializeModule(void)
{
	sceNetPsInitializeSync();
	sceNetPsInitializePacketCallbacks(sceNetPsPreparePacketQueue,
					 sceNetPsKickTransmit);
	memset(&gNetPsSystemHooks, 0, sizeof(gNetPsSystemHooks));
	gNetPsSystemHooks.suspend = sceNetPsSuspendDrivers;
	gNetPsSystemHooks.resume = sceNetPsDispatchDriverEvents;
	memset(&gNetPsThreadHooks, 0, sizeof(gNetPsThreadHooks));
	gNetPsThreadHooks.exit = sceNetPsOnThreadExit;
	memset(&gNetPsProcessHooks, 0, sizeof(gNetPsProcessHooks));
	gNetPsProcessHooks.create = sceNetPsOnProcessCreate;
	gNetPsProcessHooks.exit = sceNetPsOnProcessExit;
	gNetPsProcessHooks.kill = sceNetPsOnProcessKill;
	gNetPsProcessHooks.suspend = sceNetPsSuspendProcessSockets;
	gNetPsProcessHooks.resume = sceNetPsResumeProcessSockets;
	return sceNetPsInitializePlatform(sceNetPsStartModule,
			&gNetPsSystemHooks, &gNetPsThreadHooks, &gNetPsProcessHooks);
}

unsigned int sceNetPsQueryDebugEmulation(
		const SceNetPsDebugEmulationSelector *selector,
		unsigned int selectorSize, SceNetPsDebugEmulationInfo *info,
		unsigned int infoSize)
{
	struct ifnet *interface;
	unsigned int result;

	if (!selector)
		return 0x80418101;
	if (selectorSize < sizeof(*selector))
		return 0x8041810c;
	if (!info)
		return 0x80418101;
	if (infoSize < sizeof(*info))
		return 0x8041810c;
	memset(info, 0, sizeof(*info));
	info->parameters.option_number = selector->optionNumber;
	if (selector->interfaceType == 2)
		interface = sceNetPsFindInterface("eth0");
	else if (selector->interfaceType == 3)
		interface = sceNetPsFindInterface("wlan0");
	else
		return 0x8041810d;
	if (!interface || !interface->if_softc)
		return 0x8041810e;
	result = sceNetPsGetDriverEmulation(interface->if_softc,
					  &info->parameters, 0);
	if ((int)result < 0) {
		if ((result & 0xffff0000) == 0xffff0000)
			return -result | 0x80410100;
		return result;
	}
	info->parameterSize = sizeof(info->parameters);
	info->interfaceType = selector->interfaceType;
	return sizeof(*info);
}

int sceNetPsSetDebugEmulation(SceNetPsDebugEmulationRequest *request,
			    unsigned int requestSize, int flags)
{
	struct ifnet *interface;
	int result;

	if (!request)
		return (int)0x80418101;
	if (requestSize < sizeof(*request) || flags ||
	    request->parameterSize != sizeof(request->parameters))
		return (int)0x8041810c;
	request->parameters.option_number = request->optionNumber;
	if (request->interfaceType == 2)
		interface = sceNetPsFindInterface("eth0");
	else if (request->interfaceType == 3)
		interface = sceNetPsFindInterface("wlan0");
	else
		return (int)0x8041810d;
	if (!interface || !interface->if_softc)
		return (int)0x8041810e;
	result = sceNetPsSetDriverEmulation(interface->if_softc,
					  &request->parameters, 0, 1);
	if (result >= 0)
		return 0;
	if (((unsigned int)result & 0xffff0000) == 0xffff0000)
		return -result | 0x80410100;
	return result;
}

/* Name unknown; validate and translate the kernel statistics query result. */
int sceNetPsQueryKernelStatistics(int flags, SceNetPsKernelStatistics *statistics,
			       unsigned int size)
{
	int result;

	if (flags)
		return 0x8041810c;
	if (!statistics)
		return 0x80418101;
	if (size < sizeof(*statistics))
		return 0x8041810c;
	result = sceNetPsGetKernelStatistics(statistics, 0);
	if (result >= 0)
		return sizeof(*statistics);
	if (((unsigned int)result & 0xffff0000u) == 0xffff0000u)
		return -result | 0x80410100;
	return result;
}

/* SCE: debug command 4, one interface's record by type (as in SceNetIfType) */
int sceNetPsQueryDebugInterface(const int *interfaceType, unsigned int selectorSize,
				SceNetPsDebugInterfaceInfo *info,
				unsigned int infoSize)
{
	struct ifnet *interface;
	int result;

	if (!interfaceType)
		return 0x80418101;
	if (selectorSize < sizeof(*interfaceType))
		return 0x8041810c;
	if (!info)
		return 0x80418101;
	if (infoSize < sizeof(*info))
		return 0x8041810c;
	switch (*interfaceType) {
	case 1:
		interface = sceNetPsFindInterface("lo0");
		break;
	case 2:
		interface = sceNetPsFindInterface("eth0");
		break;
	case 3:
		interface = sceNetPsFindInterface("wlan0");
		break;
	case 4:
		interface = sceNetPsFindInterface("bt0");
		break;
	case 5:
		interface = sceNetPsFindInterface("phone0");
		break;
	case 6:
		interface = sceNetPsFindInterface("pppoe");
		break;
	default:
		return 0x8041810d;
	}
	if (!interface)
		return 0x8041810e;
	result = sceNetPsSerializeInterfaceInfo(interface, &info->info, 0);
	if (result < 0) {
		if (((unsigned int)result & 0xffff0000) == 0xffff0000)
			return -result | 0x80410100;
		return result;
	}
	info->interfaceType = *interfaceType;
	info->infoSize = sizeof(info->info);
	return sizeof(*info);
}

int sceNetPsCollectSockInfo(int socket, SceNetSockInfo *records, int capacity,
			 int flags, int *count, SceUID processId,
			 int kernel);

int sceNetPsQueryDebugSockInfo(const SceUID *processId, unsigned int processSize,
			     SceNetPsDebugSockInfo *info, unsigned int infoSize)
{
	int count = 0;
	int result;

	if (!processId)
		return (int)0x80418101;
	if (processSize < sizeof(*processId))
		return (int)0x8041810c;
	if (!info)
		return (int)0x80418101;
	if (infoSize < sizeof(*info))
		return (int)0x8041810c;
	result = sceNetPsCollectSockInfo(-1, info->sockets,
			(infoSize - sizeof(*info)) / sizeof(*info->sockets),
			54, &count, *processId, 1);
	if (result >= 0) {
		SceUID selectedProcess = *processId;

		info->count = count;
		info->recordSize = sizeof(*info->sockets);
		info->processId = selectedProcess;
		return (int)sizeof(*info->sockets) * count + sizeof(*info);
	}
	if (((unsigned int)result & 0xffff0000) == 0xffff0000)
		return -result | 0x80410100;
	return result;
}

/* SCE: the public query exposes the DNS addresses and configuration flags. */
int sceNetPsQueryDnsConfig(int flags, void *buffer, unsigned int length)
{
	SceNetPsDnsConfig config;
	int result;

	if (flags)
		return 0x8041810c;
	if (!buffer)
		return 0x80418101;
	if (length <= 11)
		return 0x8041810c;
	result = sceNetPsGetDnsConfig(&config, 0, 1);
	if (result >= 0) {
		memcpy(buffer, &config, 12);
		return 12;
	}
	if ((result & 0xffff0000) == 0xffff0000)
		result = -result | 0x80410100;
	return result;
}

/* SCE: a debug command; each returns its result size or an error */
static inline int sceNetPsRunDebugCommand(int command, void *input,
					  unsigned int inputSize, void *output,
					  unsigned int outputSize)
{
	switch (command) {
	case 1:
		return sceNetPsQueryDebugEmulation(input, inputSize, output,
						  outputSize);
	case 2:
		return sceNetPsSetDebugEmulation(input, inputSize, outputSize);
	case 3:
		return sceNetPsQueryKernelStatistics(inputSize, output, outputSize);
	case 4:
		return sceNetPsQueryDebugInterface(input, inputSize, output,
						   outputSize);
	case 5:
		return sceNetPsQueryDebugSockInfo(input, inputSize, output,
						  outputSize);
	case 6:
		return sceNetPsQueryDnsConfig(inputSize, output, outputSize);
	default:
		return 0x8041810b;
	}
}

/* SCE: a registered debugger's command, from a caller holding the stack lock */
int sceNetPsDebugControlUnlocked(SceNetPsDebugClient *client, int command,
				 void *input, unsigned int inputSize,
				 void *output, unsigned int outputSize)
{
	SceNetPsDebugClient *registered = *gNetPsDebugInterfaceSlot;

	if (registered != client)
		return 0x8041810a;
	if (!registered)
		return 0x80418109;
	return sceNetPsRunDebugCommand(command, input, inputSize, output,
				       outputSize);
}

/*
 * SCE: a registered debugger's command under the stack lock; with flag 2
 * the lock is waited for at most three seconds.
 */
int sceNetPsDebugControl(SceNetPsDebugClient *client, int command, void *input,
			 unsigned int inputSize, void *output,
			 unsigned int outputSize)
{
	SceNetPsDebugClient *registered = *gNetPsDebugInterfaceSlot;
	int timeout = 3000000;
	int result;

	if (registered != client)
		return 0x8041810a;
	if (!registered)
		return 0x80418109;
	if (registered->flags & 2) {
		sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex,
					  (unsigned int *)&timeout);
	} else {
		timeout = 0;
		sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	}
	if (timeout < 0)
		return 0x8041810f;
	result = sceNetPsRunDebugCommand(command, input, inputSize, output,
					 outputSize);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/* Name unknown; retain the debug interface slot and its initial driver. */
int sceNetPsInitializeDebugInterface(SceNetPsDebugClient **slot,
				     struct NetDriver *driver)
{
	gNetPsDebugInterfaceSlot = slot;
	gNetPsDebugDriver = driver;
	return 0;
}

/*
 * SCE: take the locks a debugger registration needs, unless the debugger
 * holds them (flag 1); with flag 2 the stack lock is waited for at most
 * three seconds.
 */
static inline int sceNetPsLockForDebugClient(unsigned int flags)
{
	int timeout = 3000000;

	if (flags & 2) {
		sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex,
					  (unsigned int *)&timeout);
	} else {
		timeout = 0;
		sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	}
	if (timeout < 0)
		return 0x8041810f;
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	return 0;
}

static inline void sceNetPsUnlockForDebugClient(void)
{
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
}

/* SCE: register a debugger and give it its two command entry points. */
int SceNetPsForDriver_83A12CCC(SceNetPsDebugClient *client)
{
	int result;

	if (!client)
		return 0x80418101;
	if (!(client->flags & 1) && (result = sceNetPsLockForDebugClient(client->flags)))
		return result;
	if (!gNetPsDebugInterfaceSlot) {
		result = 0x80418106;
	} else if (*gNetPsDebugInterfaceSlot) {
		result = 0x80418101;
	} else {
		*gNetPsDebugInterfaceSlot = client;
		client->control = sceNetPsDebugControl;
		(*gNetPsDebugInterfaceSlot)->controlUnlocked =
			sceNetPsDebugControlUnlocked;
		result = 0;
	}
	if (!(client->flags & 1))
		sceNetPsUnlockForDebugClient();
	return result;
}

/* SCE: unregister the debugger registered by SceNetPsForDriver 83A12CCC. */
int SceNetPsForDriver_C000CA8F(SceNetPsDebugClient *client)
{
	int result;

	if (!client)
		return 0x80418101;
	if (!(client->flags & 1) && (result = sceNetPsLockForDebugClient(client->flags)))
		return result;
	if (!gNetPsDebugInterfaceSlot) {
		result = 0x80418106;
	} else if (*gNetPsDebugInterfaceSlot == client) {
		*gNetPsDebugInterfaceSlot = NULL;
		result = 0;
	} else {
		result = 0x80418101;
	}
	if (!(client->flags & 1))
		sceNetPsUnlockForDebugClient();
	return result;
}

/* Name unknown; retain the kernel gateway interface slot and initial driver. */
int sceNetPsInitializeGateway(SceNetPsGatewayState *slot, struct NetDriver *driver)
{
	gNetPsGatewayInterfaceSlot = slot;
	gNetPsGatewayDriver = driver;
	return 0;
}

/* Name unknown; determine whether any driver event callback is registered. */
__attribute__((noinline))
int sceNetPsHasDriverCallbacks(struct SceNetPsDriverContext *context)
{
	int index;

	for (index = 0; index < 3; index++) {
		if (context->eventHandlers[index].flags & 1)
			return 1;
	}
	return 0;
}

/* Name unknown; retain a kernel callback in one of the driver's three slots. */
int sceNetPsRegisterDriverCallback(struct SceNetPsDriverContext *context,
				   SceUID callbackId, int clearPending)
{
	SceNetPsDriverEvent *handler;
	int index, result;

	if (!context)
		context = &gNetPsDefaultDriverContext;
	for (index = 0; index < 3; index++) {
		handler = &context->eventHandlers[index];
		if ((handler->flags & 1) && handler->callbackId == callbackId)
			return 0;
	}
	for (index = 0; index < 3; index++) {
		if (!(context->eventHandlers[index].flags & 1))
			break;
	}
	if (index == 3)
		return -17;
	handler = &context->eventHandlers[index];
	strncpy(handler->name, "SceNetKernelEvent********", 31);
	sceNetPsFormatUidName(callbackId, handler->name, 31);
	result = sceNetPsBindCallback(&handler->callback, callbackId, handler->name);
	if (result >= 0) {
		handler->callbackId = callbackId;
		if (clearPending)
			handler->events = 0;
		handler->flags |= 1;
	}
	return result;
}

/* Name unknown; remove a callback without disturbing the other event slots. */
int sceNetPsUnregisterDriverCallback(struct SceNetPsDriverContext *context,
				     SceUID callbackId)
{
	SceNetPsDriverEvent *handler;
	int index;

	if (!context)
		context = &gNetPsDefaultDriverContext;
	for (index = 0; index < 3; index++) {
		handler = &context->eventHandlers[index];
		if ((handler->flags & 1) && handler->callbackId == callbackId)
			break;
	}
	if (index == 3)
		return -5;
	handler = &context->eventHandlers[index];
	sceNetPsUnbindCallback(&handler->callback);
	handler->callbackId = 0;
	handler->flags &= ~1u;
	return 0;
}

/* Name unknown; coalesce events for each registered driver callback. */
int sceNetPsNotifyDriverCallbacks(struct SceNetPsDriverContext *context,
				  SceUInt64 events)
{
	SceNetPsDriverEvent *handler;
	int remaining;

	if (events) {
		if (!context)
			context = &gNetPsDefaultDriverContext;
		handler = context->eventHandlers;
		remaining = 3;
		do {
			handler->events |= events;
			if (handler->flags & 1)
				sceNetPsNotifyCallback(&handler->callback, events);
			handler++;
		} while (--remaining);
	}
	return 0;
}

/* Name unknown; test pending events in each registered driver callback. */
int sceNetPsDriverHasPendingEvents(struct SceNetPsDriverContext *context,
				  SceUInt64 events)
{
	SceNetPsDriverEvent *handler;
	int index;

	if (!context)
		context = &gNetPsDefaultDriverContext;
	for (index = 0; index < 3; index++) {
		handler = &context->eventHandlers[index];
		if ((handler->flags & 1) && (handler->events & events) == events)
			return 1;
	}
	return 0;
}

/* Name unknown; report current free bytes or the allocator's low-water mark. */
unsigned int sceNetPsGetFreeMemory(int lowest)
{
	if (lowest)
		return gNetPsLowestFreeMemory;
	return gNetPsFreeMemory;
}

/* Name unknown; count allocations available across the free-block list. */
int sceNetPsCheckMemoryAvailability(int size, int count)
{
	SceNetPsHeapBlock *block;
	unsigned int allocations = count;
	int remaining, available;

	sce_psnet_bnet_mutex_lock(&gNetPsMemMutex, NULL);
	for (block = gNetPsFreeLowestBlock; block; block = block->next) {
		remaining = block->capacity - 32 - size;
		for (;;) {
			available = remaining;
			remaining -= 32 + size;
			if (available < 0)
				break;
			if ((int)--allocations <= 0) {
				sce_psnet_bnet_mutex_unlock(&gNetPsMemMutex);
				return 1;
			}
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsMemMutex);
	return 0;
}

/* Name unknown; initialize the private allocator's arena and free lists. */
int sceNetPsInitializeHeap(void *base, int size)
{
	SceNetPsHeapBlock *block;

	if (size > 31) {
		gNetPsHeapStart = base;
		memset(base, 0, size);
		block = (SceNetPsHeapBlock *)gNetPsHeapStart;
		gNetPsFreeLowestBlock = block;
		gNetPsFreeHighestBlock = block;
		gNetPsHeapSize = size;
		gNetPsAllocatedLowestBlock = NULL;
		gNetPsAllocatedHighestBlock = NULL;
		block->magic = 0x46724565;
		block->next = NULL;
		block->prev = NULL;
		gNetPsInitialHeapBlock = gNetPsFreeLowestBlock;
		gNetPsFreeLowestBlock->capacity = size - sizeof(*block);
		gNetPsHeapEnd = (unsigned char *)gNetPsFreeLowestBlock + size;
		gNetPsHeapLastByte = gNetPsHeapStart + size - 1;
		gNetPsFreeMemory = size;
		gNetPsLowestFreeMemory = size;
		gNetPsHeapReady = 1;
	}
	return 0;
}

/* Name unknown; take the best-fitting free block, splitting when possible. */
void *sceNetPsMalloc(SceSize size, int flags, SceSize align)
{
	SceNetPsHeapBlock *block, *split, *whole, *allocated, *following;
	unsigned int alignment = (align + 7) & ~7u;
	unsigned int mask, padding, candidatePadding;
	unsigned int freeBytes, lowest;
	int requested = ((size + 7) & ~7u) + 4;
	int remainder, candidateRemainder, capacity;
	const void *caller;
	void *result;

	if ((int)align <= 0)
		alignment = 8;
	mask = alignment - 1;
	for (;;) {
		caller = __builtin_return_address(0);
		if (size > 0x4000000 || !gNetPsHeapReady)
			goto wait;
		sce_psnet_bnet_mutex_lock(&gNetPsMemMutex, NULL);
		block = alignment == 8 ? gNetPsFreeHighestBlock :
			gNetPsFreeLowestBlock;
		split = NULL;
		whole = NULL;
		padding = 0;
		remainder = 0;
		while (block) {
			if (block->magic != 0x46724565) {
				*(volatile unsigned int *)0 = 0;
				return (void *)10;
			}
			capacity = block->capacity;
			candidatePadding = ((uintptr_t)(block + 1) + capacity -
					    requested) & mask;
			candidateRemainder = capacity - 32 - requested -
				candidatePadding;
			if (candidateRemainder >= 0) {
				if (!split || capacity < split->capacity) {
					padding = candidatePadding;
					remainder = candidateRemainder;
					split = block;
				}
			} else if (!((uintptr_t)(block + 1) & mask) &&
				   requested <= capacity) {
				if (!whole || capacity < whole->capacity)
					whole = block;
			}
			block = alignment == 8 ? block->prev : block->next;
		}
		if (whole) {
			allocated = whole;
			if (whole->next)
				whole->next->prev = whole->prev;
			else
				gNetPsFreeHighestBlock = whole->prev;
			if (whole->prev)
				whole->prev->next = whole->next;
			else
				gNetPsFreeLowestBlock = whole->next;
			capacity = whole->capacity;
		} else if (split) {
			allocated = (SceNetPsHeapBlock *)
				((unsigned char *)(split + 1) + remainder);
			split->capacity = remainder;
			capacity = requested + padding;
			allocated->capacity = capacity;
		} else {
			sce_psnet_bnet_mutex_unlock(&gNetPsMemMutex);
			goto wait;
		}
		following = (SceNetPsHeapBlock *)
			((unsigned char *)(allocated + 1) + capacity);
		if (gNetPsAllocatedLowestBlock) {
			if ((uintptr_t)following < (uintptr_t)gNetPsHeapEnd) {
				if (following->magic != 0x42755379) {
					*(volatile unsigned int *)0 = 0;
					return (void *)20;
				}
				allocated->prev = following->prev;
				if (following->prev)
					following->prev->next = allocated;
				else
					gNetPsAllocatedLowestBlock = allocated;
				allocated->next = following;
				following->prev = allocated;
				capacity = allocated->capacity;
			} else {
				allocated->next = NULL;
				block = gNetPsAllocatedHighestBlock;
				gNetPsAllocatedHighestBlock = allocated;
				allocated->prev = block;
				block->next = allocated;
				capacity = allocated->capacity;
			}
		} else {
			allocated->prev = NULL;
			allocated->next = NULL;
			gNetPsAllocatedLowestBlock = allocated;
			gNetPsAllocatedHighestBlock = allocated;
		}
		freeBytes = gNetPsFreeMemory - 32 - capacity;
		allocated->requestedSize = size;
		lowest = gNetPsLowestFreeMemory;
		allocated->allocationCaller = caller;
		allocated->magic = 0x42755379;
		if (freeBytes < lowest)
			lowest = freeBytes;
		gNetPsLowestFreeMemory = lowest;
		result = allocated + 1;
		gNetPsFreeMemory = freeBytes;
		memcpy((unsigned char *)result + size, &gNetPsHeapTailMagic, 4);
		sce_psnet_bnet_mutex_unlock(&gNetPsMemMutex);
		if (flags & 2)
			memset(result, 0, size);
		return result;

wait:
		if ((int)size <= 0 || (flags & 1))
			return NULL;
		sceNetPsSleep(&gNetPsHeapSize, 0, 0, -1);
	}
}

/* Name unknown; release an allocation and coalesce adjacent free blocks. */
int sceNetPsFree(void *ptr)
{
	SceNetPsHeapBlock *block, *merged, *cursor, *following;
	unsigned int tail;
	const void *caller = __builtin_return_address(0);

	if (!ptr)
		return sceNetPsWakeupNormal(&gNetPsHeapSize);
	block = (SceNetPsHeapBlock *)ptr - 1;
	merged = block;
	if (block->magic != 0x42755379) {
		*(volatile unsigned int *)0 = 0;
		return 30;
	}
	memcpy(&tail, (unsigned char *)ptr + block->requestedSize, 4);
	if (tail != 0x4d61416b) {
		*(volatile unsigned int *)0 = 0;
		return 40;
	}
	sce_psnet_bnet_mutex_lock(&gNetPsMemMutex, NULL);
	if (block->next)
		block->next->prev = block->prev;
	else
		gNetPsAllocatedHighestBlock = block->prev;
	if (block->prev)
		block->prev->next = block->next;
	else
		gNetPsAllocatedLowestBlock = block->next;
	block->magic = 0x46724565;
	block->freeCaller = caller;
	gNetPsFreeMemory += block->capacity + 32;
	cursor = block;
	for (;;) {
		following = (SceNetPsHeapBlock *)
			((unsigned char *)(cursor + 1) + cursor->capacity);
		cursor = cursor->next;
		if (cursor != following)
			break;
	}
	if ((uintptr_t)gNetPsHeapEnd > (uintptr_t)following) {
		block->prev = following->prev;
		if (following->prev)
			following->prev->next = block;
		else
			gNetPsFreeLowestBlock = block;
		block->next = following;
		following->prev = block;
	} else {
		block->next = NULL;
		block->prev = gNetPsFreeHighestBlock;
		if (gNetPsFreeHighestBlock)
			gNetPsFreeHighestBlock->next = block;
		else
			gNetPsFreeLowestBlock = block;
		gNetPsFreeHighestBlock = block;
	}
	if (block->prev && (unsigned char *)(block->prev + 1) +
		block->prev->capacity == (unsigned char *)block) {
		block->prev->capacity += block->capacity + 32;
		merged = block->prev;
		block->magic = 0;
		merged->next = merged->next->next;
		if (merged->next)
			merged->next->prev = merged;
		else
			gNetPsFreeHighestBlock = merged;
	}
	following = merged->next;
	if (following && (unsigned char *)(merged + 1) + merged->capacity ==
		(unsigned char *)following) {
		merged->capacity += following->capacity + 32;
		merged->next = following->next;
		if (merged->next)
			merged->next->prev = merged;
		else
			gNetPsFreeHighestBlock = merged;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsMemMutex);
	return sceNetPsWakeupNormal(&gNetPsHeapSize);
}

/* Name unknown; accept addresses within the network allocator's arena. */
int sceNetPsIsHeapAddress(const void *address)
{
	return (uintptr_t)gNetPsHeapStart <= (uintptr_t)address &&
		(uintptr_t)address <= (uintptr_t)gNetPsHeapLastByte;
}

/* Name unknown; copy a terminated string from a kernel or user address. */
int sceNetPsCopyString(const char *source, char *destination, size_t length,
		       size_t *copied, int kernel)
{
	signed char *cursor;
	const unsigned char *input = (const unsigned char *)source;
	signed char *end;
	int result;
	unsigned char value;

	if (!destination || !source || !copied)
		return 14;
	if (kernel) {
		end = (signed char *)destination + length;
		cursor = (signed char *)destination;
		while (cursor != end) {
			value = *input++;
			*cursor++ = value;
			if (!value)
				goto complete;
		}
		return 63;
	}
	result = sceNetPsCopyUserString(source, destination, length);
	if (result < 0)
		return result;
	cursor = (signed char *)destination;
	while (*cursor++)
		;
complete:
	*copied = cursor - (signed char *)destination;
	return 0;
}

/* Name unknown; copy kernel bytes to a user or kernel destination. */
int sceNetPsCopyOut(const void *source, void *destination, size_t length,
		    int kernel)
{
	int invalid = !source || !destination;

	if (invalid)
		return 14;
	if (!kernel)
		return sceNetPsCopyUserOut(source, destination, length);
	memcpy(destination, source, length);
	return invalid;
}

/* Name unknown; copy user or kernel bytes to a kernel destination. */
int sceNetPsCopyIn(const void *source, void *destination, size_t length,
		   int kernel)
{
	if (!source || !destination)
		return 14;
	if (!kernel)
		return sceNetPsCopyUserIn(source, destination, length);
	memcpy(destination, source, length);
	return 0;
}

/* Name unknown; user-only string copy returning the terminator-inclusive count. */
int sceNetPsCopyInString(const char *source, char *destination, size_t length,
			size_t *copied)
{
	signed char *cursor;
	int result;

	if (!source || !destination || !copied)
		return 14;
	result = sceNetPsCopyUserString(source, destination, length);
	if (result >= 0) {
		cursor = (signed char *)destination;
		do {
			result = *cursor++;
		} while (result);
		*copied = cursor - (signed char *)destination;
	}
	return result;
}

/* NetBSD's copyout: kernel bytes to a user address, EFAULT for a NULL pointer */
int copyout(const void *source, void *destination, size_t length)
{
	if (!source || !destination)
		return 14;
	return sceNetPsCopyUserOut(source, destination, length);
}

/* NetBSD's copyin: user bytes into the kernel, EFAULT for a NULL pointer */
int copyin(const void *source, void *destination, size_t length)
{
	if (!source || !destination)
		return 14;
	return sceNetPsCopyUserIn(source, destination, length);
}

/* SCE: initialize a fixed-object pool backed by the network heap. */
void pool_init(struct pool *pool, size_t size, unsigned int alignment,
	       unsigned int offset, int flags, const char *name,
	       struct pool_allocator *allocator)
{
	if (!alignment)
		alignment = 4;
	size = (size - 1 + alignment) / alignment * alignment;
	pool->freeList = NULL;
	pool->freeCount = 0;
	pool->pr_size = size;
	pool->pr_align = alignment;
	pool->pr_itemoffset = offset;
	pool->pr_flags = flags;
	pool->pr_wchan = name;
	pool->pr_alloc = allocator;
}

/* SCE: reuse one cached object or allocate it from the network heap. */
void *pool_get(struct pool *pool, int flags)
{
	struct SceNetPsPoolFreeItem *object = pool->freeList;

	if (!object)
		return sceNetPsMalloc(pool->pr_size, ((flags ^ 2) >> 1) & 1,
				      pool->pr_align);
	pool->freeList = object->next;
	--pool->freeCount;
	return object;
}

/* SCE: cache at most 32 reusable objects with room for their free-list link. */
void pool_put(struct pool *pool, void *allocation)
{
	struct SceNetPsPoolFreeItem *object = allocation;

	if (pool->pr_size > 3 && pool->freeCount <= 31) {
		object->next = pool->freeList;
		pool->freeList = object;
		++pool->freeCount;
	} else {
		sceNetPsFree(object);
	}
}

/* SCE: replacement for NetBSD's VM-backed pool cache initialization. */
void pool_cache_init(struct pool_cache *cache, struct pool *pool,
		     int (*constructor)(void *, void *, int),
		     void (*destructor)(void *, void *), void *argument)
{
	cache->pc_pool = pool;
	cache->pc_ctor = constructor;
	cache->pc_dtor = destructor;
	cache->pc_arg = argument;
}

/* SCE: this heap-backed cache does not supply a physical address. */
void *pool_cache_get_paddr(struct pool_cache *cache, int flags,
			   paddr_t *physicalAddress)
{
	void *object = pool_get(cache->pc_pool, flags);

	if (object && physicalAddress)
		*physicalAddress = (paddr_t)-1;
	return object;
}

/* SCE: retain the upstream drain-hook interface in the reduced pool. */
void pool_set_drain_hook(struct pool *pool, void (*hook)(void *, int),
			 void *argument)
{
	pool->pr_drain_hook = hook;
	pool->pr_drain_hook_arg = argument;
}

/* SCE: preserve the configured limit and its warning rate in seconds. */
void pool_sethardlimit(struct pool *pool, int limit, const char *warning,
		       int ratecap)
{
	pool->pr_hardlimit = limit;
	pool->pr_hardlimit_warning = warning;
	pool->pr_hardlimit_ratecap = ratecap;
}

/* SCE: retain the requested low-water count without VM page accounting. */
void pool_setlowat(struct pool *pool, int count)
{
	pool->pr_minitems = count;
}

int SceNetPsForDriver_880A5423(int socket, SceNetSockaddr *address,
		  unsigned int *addressLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsAccept(socket, address, addressLength, 1);
}

int SceNetPsForDriver_84AB650F(int socket, const SceNetSockaddr *address,
		unsigned int addressLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsBind(socket, address, addressLength, 1);
}

int SceNetPsForDriver_13491DA1(int socket, const SceNetSockaddr *address,
		   unsigned int addressLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsConnect(socket, address, addressLength, 1);
}

int SceNetPsForDriver_B949AFD5(int socket, SceNetSockaddr *address,
		       unsigned int *addressLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsGetpeername(socket, address, addressLength, 1);
}

int SceNetPsForDriver_8F1BB0E7(int socket, SceNetSockaddr *address,
		       unsigned int *addressLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsGetsockname(socket, address, addressLength, 1);
}

int SceNetPsForDriver_92EE24A6(int socket, int level, int option, void *value,
		     unsigned int *length)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsGetsockopt(socket, level, option, value, length, 1);
}

int SceNetPsForDriver_080C7992(int socket, int backlog)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsListen(socket, backlog, 1);
}

int SceNetPsForDriver_49B1669C(int socket, void *buffer, unsigned int length, int flags,
		    SceNetSockaddr *from, unsigned int *fromLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 7))
		return -55;
	return sceNetPsRecvfrom(socket, buffer, length, flags, from, fromLength, 1);
}

int SceNetPsForDriver_AB746734(int socket, const void *buffer, unsigned int length, int flags,
		  const SceNetSockaddr *to, unsigned int toLength)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 7))
		return -55;
	return sceNetPsSendto(socket, buffer, length, flags, to, toLength, 1);
}

int SceNetPsForDriver_4BF5FAB4(int socket, int level, int option, const void *value,
		     unsigned int length)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsSetsockopt(socket, level, option, value, length, 1);
}

int SceNetPsForDriver_EEB19FB6(int socket, int how)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsShutdown(socket, how, 1);
}

int SceNetPsForDriver_EB95B024(const char *name, int domain, int type, int protocol)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsSocket(name, domain, type, protocol, 1);
}

int SceNetPsForDriver_21F4428D(int socket)
{
	if (!sceNetPsCheckMemoryAvailability(2048, 5))
		return -55;
	return sceNetPsClose(socket, 1);
}

int sceNetSyscallGetSockinfo(int socket, void *buffer, int count, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsGetSockinfo(socket, buffer, count, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallSocketAbort(int socket, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsSocketAbort(socket, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallSocket(const char *name, int domain, int type, int protocol)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsSocket(name, domain, type, protocol, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallBind(int socket, const SceNetSockaddr *address,
		      unsigned int addressLength)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsBind(socket, address, addressLength, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallListen(int socket, int backlog)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsListen(socket, backlog, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallAccept(int socket, SceNetSockaddr *address,
			unsigned int *addressLength)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsAccept(socket, address, addressLength, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallConnect(int socket, const SceNetSockaddr *address,
			 unsigned int addressLength)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsConnect(socket, address, addressLength, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallIcmConnect(int socket, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsIcmConnect(socket, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallSendmsg(int socket, const SceNetMsghdr *message, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 7))
		result = sceNetPsSendmsg(socket, message, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallSendto(const SceNetSyscallSendtoParam *parameter)
{
	unsigned int state = sceNetPsSyscallEnter();
	SceNetSyscallSendtoParam arguments;
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 7)) {
		if (sceNetPsCopyUserIn(parameter, &arguments, sizeof(arguments)))
			result = -8;
		else
			result = sceNetPsSendto(arguments.socket, arguments.buffer,
					       arguments.length, arguments.flags,
					       arguments.address,
					       arguments.addressLength, 0);
	} else {
		result = -55;
	}
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallRecvmsg(int socket, SceNetMsghdr *message, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 7))
		result = sceNetPsRecvmsg(socket, message, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallRecvfrom(const SceNetSyscallRecvfromParam *parameter)
{
	unsigned int state = sceNetPsSyscallEnter();
	SceNetSyscallRecvfromParam arguments;
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 7)) {
		if (sceNetPsCopyUserIn(parameter, &arguments, sizeof(arguments)))
			result = -8;
		else
			result = sceNetPsRecvfrom(arguments.socket, arguments.buffer,
						 arguments.length, arguments.flags,
						 arguments.address,
						 arguments.addressLength, 0);
	} else {
		result = -55;
	}
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallShutdown(int socket, int how)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsShutdown(socket, how, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallGetsockname(int socket, SceNetSockaddr *address,
			    unsigned int *addressLength)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsGetsockname(socket, address, addressLength, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallGetpeername(int socket, SceNetSockaddr *address,
			    unsigned int *addressLength)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsGetpeername(socket, address, addressLength, 0);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallIoctl(int socket, unsigned int command, void *data)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsIoctl(socket, command, data);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallClose(int socket)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result = sceNetPsClose(socket, 0);

	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallEpollCreate(const char *name, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsEpollCreate(name, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallEpollCtl(int epoll, int operation, int socket,
			 SceNetEpollEvent *event)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsEpollCtl(epoll, operation, socket, event);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallEpollClose(int epoll)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsEpollClose(epoll);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallEpollAbort(int epoll, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsEpollAbort(epoll, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDumpCreate(const char *name, int length, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDumpCreate(name, length, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDumpRead(int dump, void *buffer, int length, int *flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDumpRead(dump, buffer, length, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDumpClose(int dump)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDumpClose(dump);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDumpAbort(int dump, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDumpAbort(dump, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDescriptorCreate(const char *name, int flags)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDescriptorCreate(name, flags);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDescriptorCtl(int descriptor, int operation, int socket, void *info)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDescriptorCtl(descriptor, operation, socket, info);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallDescriptorClose(int descriptor)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsDescriptorClose(descriptor);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallGetIfList(void *list, int count)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsGetIfList(list, count);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallControl(int interfaceIndex, int command, void *data, int length)
{
	unsigned int state = sceNetPsSyscallEnter();
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5))
		result = sceNetPsControl(interfaceIndex, command, data, length);
	else
		result = -55;
	sceNetPsSyscallLeave(state);
	return result;
}

int sceNetSyscallSysctl(const SceNetSyscallSysctlParam *parameter)
{
	unsigned int state = sceNetPsSyscallEnter();
	SceNetSyscallSysctlParam arguments;
	int result;

	if (sceNetPsCheckMemoryAvailability(2048, 5)) {
		if (sceNetPsCopyUserIn(parameter, &arguments, sizeof(arguments)))
			result = -8;
		else
			result = sceNetPsSysctl(arguments.name, arguments.nameLength,
					       arguments.oldData, arguments.oldLength,
					       arguments.newData, arguments.newLength);
	} else {
		result = -55;
	}
	sceNetPsSyscallLeave(state);
	return result;
}

