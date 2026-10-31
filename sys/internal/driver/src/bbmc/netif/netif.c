/* Baseband network driver callbacks. */
#include <string.h>
#include <usbd/usbd.h>
#include <sysmem/cpu.h>
#include <net_ps/ethernet.h>
#include <sysmem/debug.h>
#include "../bbmc_netif.h"
#include "../bbmc_memory.h"
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include <sysmem/sysmem.h>
#include <udcd/udcd.h>
#include "../usb_serial.h"
#include "../usb_device.h"
#include "../qmi_usb.h"
#include <threadmgr/threadmgr.h>

extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
	const char *name, int attr, void *opt)
	__asm__("sceKernelInitializeFastMutex");
extern int bbmcFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelFinalizeFastMutex");
extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
int bbmcSendSimpleMessage(BbmcControl *control, unsigned int message);
int ConnectNetwork(struct NetDriver *driver, int argument);
int bbmcCloseNetworkLink(struct NetDriver *driver, int argument);
int bbmcKickNetworkTransmit(struct NetDriver *driver, int argument);
int GetCommRate(struct NetDriver *driver, unsigned int command,
	int *argument, unsigned int size);
/* The module's original import table uses this symbol for driver NID 4B675D05. */

BbmcPacketInterface g_bbmcPacketInterfaceStorage;
BbmcPacketInterface *g_bbmcPacketInterface;

#if FW_DEBUG
static const SceKernelDebugInfo dbg_ConnectNetwork_L910
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "netif/netif.c", .line = 910, .func = "ConnectNetwork",
};

static const SceKernelDebugInfoSource dbg_ConnectNetwork_L910Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_ConnectNetwork_L910, "netif/netif.c", 910, "ConnectNetwork",
};

static const SceKernelDebugInfo dbg_DisconnectNetwork_L1005
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "netif/netif.c", .line = 1005, .func = "DisconnectNetwork",
};

static const SceKernelDebugInfoSource dbg_DisconnectNetwork_L1005Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_DisconnectNetwork_L1005, "netif/netif.c", 1005, "DisconnectNetwork",
};

static const SceKernelDebugInfo dbg_GetCommRate_L1053
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "netif/netif.c", .line = 1053, .func = "GetCommRate",
};

static const SceKernelDebugInfoSource dbg_GetCommRate_L1053Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_GetCommRate_L1053, "netif/netif.c", 1053, "GetCommRate",
};

static const SceKernelDebugInfo dbg_PrepareTxPacket_L1552
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "netif/netif.c",
	.line = 1552,
	.func = "PrepareTxPacket",
};

static const SceKernelDebugInfoSource dbg_PrepareTxPacket_L1552Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_PrepareTxPacket_L1552, "netif/netif.c", 1552, "PrepareTxPacket",
};
#endif

/* name unknown: the network driver has no stop work. */
int bbmcStopNetworkInterface(struct NetDriver *driver, void *memory,
	int memorySize)
{
	return 0;
}

/* name unknown: the network driver has no start work. */
int bbmcStartNetworkInterface(struct NetDriver *driver, void *memory)
{
	return 0;
}

/* name unknown: the network driver accepts the multicast list unchanged. */
int bbmcSetNetworkMulticast(struct NetDriver *driver,
	const unsigned char *addresses, unsigned int size)
{
	return 0;
}

/* name unknown: construct and deliver an IPv4 ARP reply on the packet link. */
int bbmcReplyToArp(BbmcPacketInterface *control,
	const struct EthernetArpFrame *request, SceSize length)
{
	struct NetDriver *driver = &control->driver;
	struct NetDriverPacket *packet = driver->allocateRx(driver, length, 0);
	struct EthernetArpFrame *frame;
	unsigned char *operation;

	if (packet == NULL) {
		driver->rxDrops++;
		return 0;
	}
	frame = memcpy(packet->buffer->data, request, length);
	__builtin_memcpy(frame->ethernet.ether_dhost,
		frame->ethernet.ether_shost, 4);
	__builtin_memcpy(&frame->ethernet.ether_dhost[4],
		&frame->ethernet.ether_shost[4], 2);
	__builtin_memcpy(frame->ethernet.ether_shost, control->peerMac.data, 4);
	__builtin_memcpy(&frame->ethernet.ether_shost[4],
		&control->peerMac.data[4], 2);
	__builtin_memcpy(frame->arp.arp_tha, frame->arp.arp_sha, 4);
	__builtin_memcpy(frame->arp.arp_spa, frame->arp.arp_tpa, 4);
	__builtin_memcpy(&frame->arp.arp_tha[4], &frame->arp.arp_sha[4], 2);
	__builtin_memcpy(frame->arp.arp_sha, control->peerMac.data, 4);
	__builtin_memcpy(&frame->arp.arp_sha[4], &control->peerMac.data[4], 2);
	operation = (unsigned char *)&frame->arp.ea_hdr.ar_op;
	operation[1] = 2;
	operation[0] = 0;
	__builtin_memcpy(frame->arp.arp_tpa, &control->ipv4Address, 4);
	driver->rxBytes += length;
	driver->rxPackets++;
	packet->buffer->end += length;
	packet->length = length;
	driver->deliverRx(driver, packet);
	return driver->notify(driver,
		NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
				 NET_DRIVER_STATUS_RECEIVED), NULL, 0);
}

/* name unknown: combine modem registration, USB attachment and power events. */
int bbmcGetPacketInterfaceLinkState(BbmcPacketInterface *interface, int *state)
{
	BbmcControl *manager = bbmcGetControl();
	BbmcNetworkInfo information;
	int active, registeredActive, registered, link;
	int result = sceBbmcGetNetworkInfoForDriver(&information);

	if (result < 0) {
		if (state != NULL)
			*state = 1;
		return result;
	}
	if (manager->powerEventActive != 0) {
		active = 5;
		registeredActive = 7;
		registered = 3;
		link = 1;
	} else if (manager->recentPowerEvent != 0) {
		active = 13;
		registeredActive = 15;
		registered = 11;
		link = 9;
	} else {
		active = 5;
		registeredActive = 7;
		registered = 3;
		link = 1;
	}
	if (information.serving.registrationState == 1) {
		active = registeredActive;
		link = registered;
	}
	if (interface->usbAttached != 0)
		link = active;
	if (state != NULL)
		*state = link;
	return result;
}

/* name unknown: deliver one raw IP receive and submit the next USB transfer. */
int bbmcPacketReceiveCompleted(int result, int count, void *context)
{
	BbmcPacketReceiveBuffer *receive = context;
	BbmcPacketInterface *control = receive->owner;
	struct NetDriver *driver = &control->driver;

	if (count >= 0 && result >= 0) {
		if (control->linkOpen) {
			unsigned int length = count + sizeof(struct ether_header);
			struct NetDriverPacket *packet =
				driver->allocateRx(driver, length, 0);

			if (packet != NULL) {
				struct ether_header *header =
					(struct ether_header *)packet->buffer->data;
				unsigned char *type = (unsigned char *)&header->ether_type;

				__builtin_memcpy(header->ether_dhost, control->localMac.data, 4);
				__builtin_memcpy(&header->ether_dhost[4], &control->localMac.data[4], 2);
				__builtin_memcpy(header->ether_shost, control->peerMac.data, 4);
				type[0] = 8;
				type[1] = 0;
				__builtin_memcpy(&header->ether_shost[4], &control->peerMac.data[4], 2);
				memcpy(header + 1, receive->data, count);
				driver->rxBytes += length;
				driver->rxPackets++;
				packet->buffer->end += length;
				packet->length = length;
				driver->deliverRx(driver, packet);
				driver->notify(driver,
					NET_DRIVER_EVENT(NET_DRIVER_EVENT_STATUS,
							 NET_DRIVER_STATUS_RECEIVED), NULL, 0);
			} else {
				driver->rxDrops++;
			}
		}
		return SceUsbdForDriver_235C094A(control->receivePipe, receive->data,
			receive->length, (SceUsbdDoneCallback)bbmcPacketReceiveCompleted,
			receive);
	}
	return result;
}

int bbmcPacketTransmitCompleted(int result, int count, void *context);

/* Sony's helper is inlined into both transmit entry points. */
static inline __attribute__((always_inline)) int PrepareTxPacket(
	BbmcPacketInterface *control, struct NetDriverPacket *packet,
	const void *caller)
{
	struct NetDriver *driver = &control->driver;
	BbmcPacketTransmitBuffer *transmit;
	const struct ether_header *header;
	unsigned char *data;
	unsigned int packetLength, length, index, etherType;
	SceUInt32 ownerAddress;
	int prepend, result;

	if (!control->usbAttached || !(ownerAddress = control->ipv4Address)) {
		driver->txErrors++;
		driver->releaseTx(driver, packet);
		return 0;
	}
	prepend = control->prependPacketHeader;
	packetLength = packet->length;
	length = packetLength - sizeof(struct ether_header);
	if (prepend)
		length += sizeof(BbmcPacketPrefix);
	index = control->transmitBuffers[0].pending;
	if (index != 0) {
		if (control->transmitBuffers[1].pending != 0) {
			driver->txErrors++;
			driver->releaseTx(driver, packet);
			return 0x80418002;
		}
		index = 1;
	}
	transmit = &control->transmitBuffers[index];
	transmit->pending = 1;
	header = (const struct ether_header *)packet->buffer->allocationStart;
	data = transmit->data;
	etherType = (SceUInt16)(header->ether_type << 8) | (header->ether_type >> 8);
	if (etherType == 0x800) {
		if (control->hasIpv4Address == 1) {
			unsigned char *destination;
			unsigned int size;

			if (prepend) {
				BbmcPacketPrefix *prefix = (BbmcPacketPrefix *)data;

				prefix->marker = 1;
				prefix->zeroBytes[0] = 0;
				prefix->zeroBytes[1] = 0;
				prefix->zeroBytes[2] = 0;
				prefix->zeroBytes[3] = 0;
				prefix->zeroBytes[4] = 0;
				destination = data + sizeof(*prefix);
				size = length - sizeof(*prefix);
			} else {
				destination = data;
				size = length;
			}
			memcpy(destination, header + 1, size);
			packetLength = packet->length;
		}
	} else if (etherType == 0x806) {
		const struct EthernetArpFrame *frame = (const void *)header;
		unsigned char emptyAddress[ETHER_ADDR_LEN] = {0};
		SceUInt32 sourceAddress, targetAddress;
		int different;

		if (control->hasIpv4Address != 1)
			goto consumed;
		if (((SceUInt16)(frame->arp.ea_hdr.ar_hrd << 8) |
		     (frame->arp.ea_hdr.ar_hrd >> 8)) != 1)
			goto consumed;
		if (((SceUInt16)(frame->arp.ea_hdr.ar_op << 8) |
		     (frame->arp.ea_hdr.ar_op >> 8)) != 1)
			goto consumed;
		if (((SceUInt16)(frame->arp.ea_hdr.ar_pro << 8) |
		     (frame->arp.ea_hdr.ar_pro >> 8)) != 0x800)
			goto consumed;
		if (frame->arp.ea_hdr.ar_hln != ETHER_ADDR_LEN ||
		    frame->arp.ea_hdr.ar_pln != 4)
			goto consumed;
		__builtin_memcpy(&sourceAddress, frame->arp.arp_spa, 4);
		__builtin_memcpy(&targetAddress, frame->arp.arp_tpa, 4);
		if (sourceAddress == targetAddress)
			goto consumed;
		different = memcmp(frame->arp.arp_tha, emptyAddress, sizeof(emptyAddress));
		if (sourceAddress != 0) {
			if (different != 0)
				goto consumed;
		} else if (ownerAddress == targetAddress) {
			goto consumed;
		}
		bbmcReplyToArp(control, frame, packetLength);
		packetLength = packet->length;
		goto consumed;
#if FW_DEBUG
	} else {
		sceKernelAssert(0, &dbg_PrepareTxPacket_L1552, caller);
		packetLength = packet->length;
#endif
	}
	driver->releaseTx(driver, packet);
	transmit->owner = control;
	transmit->length = length;
	result = SceUsbdForDriver_235C094A(control->transmitPipe, data, length,
		(SceUsbdDoneCallback)bbmcPacketTransmitCompleted, transmit);
	if (result < 0) {
		driver->txDrops++;
		return result;
	}
	driver->txBytes += packetLength;
	driver->txPackets++;
	if (transmit->length % control->maxPacketSize == 0)
		SceUsbdForDriver_235C094A(control->transmitPipe, NULL, 0,
			(SceUsbdDoneCallback)bbmcPacketTransmitCompleted, NULL);
	return 0;

consumed:
	driver->txBytes += packetLength;
	driver->txPackets++;
	driver->releaseTx(driver, packet);
	transmit->pending = 0;
	return 0x80418002;
}

/* name unknown: release a USB transmit slot and drain the next queued packet. */
int bbmcPacketTransmitCompleted(int result, int count, void *context)
{
	BbmcPacketTransmitBuffer *transmit = context;
	BbmcPacketInterface *control;
	struct NetDriverPacket *packet;

	if (transmit == NULL)
		return result;
	control = transmit->owner;
	bbmcLockFastMutex(&control->transmitMutex);
	transmit->pending = 0;
	bbmcUnlockFastMutex(&control->transmitMutex);
	bbmcLockFastMutex(&control->transmitMutex);
	if (control->transmitBuffers[0].pending == 0 ||
	    control->transmitBuffers[1].pending == 0) {
		packet = control->driver.dequeueTx(&control->driver);
		if (packet != NULL)
			PrepareTxPacket(control, packet, __builtin_return_address(0));
	}
	return bbmcUnlockFastMutex(&control->transmitMutex);
}

/* name unknown: expire a packet interface after twelve timer intervals. */
SceUInt32 bbmcPacketInterfaceExpire(void *context)
{
	BbmcPacketInterface *control = context;
	SceKernelIntrStatus intr =
		sceKernelSpinlockLowLockCpuSuspendIntr(&control->expiryLock);

	if (++control->expiryCount > 11) {
		BbmcControl *manager = bbmcGetControl();

		control->expiryCount = 0;
		control->expiryTimer = 0;
		bbmcSendSimpleMessage(manager, 114);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&control->expiryLock, intr);
		return 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&control->expiryLock, intr);
	return 3600000000U;
}

int GetCommRate(struct NetDriver *driver, unsigned int command,
	int *argument, unsigned int size)
{
	BbmcPacketInterface *interface = driver->private;
	BbmcControl *manager = bbmcGetControl();
	BbmcMessage *message;
	SceUInt32 startBusy, packetRequest;
	int result;

	sceKernelAtomicAddAndGet32((SceInt32 *)&interface->activeCalls, 1);
	if (command == 0x40800002) {
		result = (int)0x8041810c;
	} else if (command <= 0x40800002) {
		if (command != 0x40800001) {
			result = (int)0x80418006;
		} else if (size != 16) {
			result = (int)0x8041810c;
		} else {
			manager = bbmcGetControl();
			message = msgAllocMsgPacket(manager, 111);
			if (message != NULL) {
				message->body.channelRatesMessage.maxTxRate =
					(SceUInt32 *)&argument[0];
				message->body.channelRatesMessage.txRate =
					(SceUInt32 *)&argument[2];
				message->body.channelRatesMessage.rxRate =
					(SceUInt32 *)&argument[3];
				message->body.channelRatesMessage.maxRxRate =
					(SceUInt32 *)&argument[1];
				result = msgSendMsgPacket(manager, message);
			} else {
				result = (int)0x80330005;
#if FW_DEBUG
				sceKernelAssert(0, &dbg_GetCommRate_L1053,
					__builtin_return_address(0));
#endif
			}
		}
	} else if (command == 0x40800003) {
		result = size == 4 ? bbmcGetPacketInterfaceLinkState(interface, argument) :
			(int)0x8041810c;
	} else if (command != 0x50800001) {
		result = (int)0x80418006;
	} else if (manager->connectPending != 0) {
		result = (int)0x80330506;
	} else {
		manager = bbmcGetControl();
		bbmcEnterCS(&manager->mutex);
		startBusy = manager->networkStartBusy;
		packetRequest = manager->pendingPacketRequest;
		bbmcLeaveCS(&manager->mutex);
		if (startBusy != 0) {
			bbmcEnterCS(&manager->mutex);
			if (manager->networkRetryPending != 0) {
				result = (int)0x80330507;
			} else if (manager->deferredNetworkStart != 0) {
				result = (int)0x80330508;
				bbmcSendSimpleMessage(manager, 107);
			} else {
				manager->networkRetryPending = 1;
				result = bbmcSendSimpleMessage(manager, 107);
			}
			bbmcLeaveCS(&manager->mutex);
		} else if (packetRequest == 0) {
			result = (int)0x80330508;
		} else {
			bbmcEnterCS(&manager->mutex);
			bbmcLockFastMutex(&interface->flagMutex);
			if (interface->disconnectPending != 0) {
				result = (int)0x80330507;
			} else if (manager->networkStarted == 0 &&
				   manager->clientOnline == 0 && manager->clientInitialized == 0) {
				result = (int)0x80330508;
			} else {
				interface->disconnectPending = 1;
				message = msgAllocMsgPacket(manager, 104);
				if (message != NULL) {
					message->body.networkStop.networkHandle = &interface->networkHandle;
					manager->reservedOperation = 1;
					result = msgSendMsgPacket(manager, message);
					if (result < 0)
						manager->reservedOperation = 0;
				} else {
					result = (int)0x80330005;
#if FW_DEBUG
					sceKernelAssert(0, &dbg_DisconnectNetwork_L1005,
						__builtin_return_address(0));
#endif
				}
			}
			bbmcUnlockFastMutex(&interface->flagMutex);
			bbmcLeaveCS(&manager->mutex);
		}
	}
	sceKernelAtomicSubAndGet32((SceInt32 *)&interface->activeCalls, 1);
	return result;
}

int bbmcKickNetworkTransmit(struct NetDriver *driver, int argument)
{
	BbmcPacketInterface *control = driver->private;
	struct NetDriverPacket *packet;
	int result;

	bbmcLockFastMutex(&control->transmitMutex);
	if (control->transmitBuffers[0].pending != 0 &&
	    control->transmitBuffers[1].pending != 0) {
		result = 0x80418002;
	} else {
		packet = control->driver.dequeueTx(&control->driver);
		result = 0;
		if (packet != NULL)
			result = PrepareTxPacket(control, packet, __builtin_return_address(0));
	}
	bbmcUnlockFastMutex(&control->transmitMutex);
	return result;
}

int bbmcInitializeUsbPacketInterface(void)
{
	BbmcPacketInterface *control = &g_bbmcPacketInterfaceStorage;
	int result, i;

	memset(control, 0, sizeof(*control));
	control->localMac.data[4] = 1;
	control->localMac.data[5] = 2;
	control->peerMac.data[3] = 4;
	control->peerMac.data[4] = 5;
	control->peerMac.data[5] = 6;
	control->localMac.data[1] = 36;
	control->peerMac.data[1] = 36;
	control->localMac.data[2] = 141;
	control->peerMac.data[2] = 141;
	for (i = 0; i < 4; ++i) {
		unsigned char *data;

		control->receiveBuffers[i].length = 1536;
		data = bbmcAlloc(1536, 64);
		control->receiveBuffers[i].owner = control;
		control->receiveBuffers[i].data = data;
	}
	control->transmitBuffers[0].length = 1536;
	control->transmitBuffers[0].data = bbmcAlloc(1536, 64);
	control->transmitBuffers[0].owner = control;
	control->transmitBuffers[1].length = 1536;
	control->transmitBuffers[1].data = bbmcAlloc(1536, 64);
	control->transmitBuffers[1].owner = control;
	if (control->receiveBuffers[0].data == NULL ||
	    control->receiveBuffers[1].data == NULL ||
	    control->receiveBuffers[2].data == NULL ||
	    control->receiveBuffers[3].data == NULL ||
	    control->transmitBuffers[0].data == NULL ||
	    control->transmitBuffers[1].data == NULL) {
		if (control->transmitBuffers[0].data != NULL)
			bbmcFree(control->transmitBuffers[0].data);
		if (control->transmitBuffers[1].data != NULL)
			bbmcFree(control->transmitBuffers[1].data);
		for (i = 0; i < 4; ++i) {
			if (control->receiveBuffers[i].data != NULL)
				bbmcFree(control->receiveBuffers[i].data);
		}
		return 0x80330001;
	}
	result = bbmcInitializeFastMutex(&control->transmitMutex,
		"SceBbmcPktIfTxDescLock", 0, NULL);
	if (result >= 0) {
		result = bbmcInitializeFastMutex(&control->flagMutex,
			"SceBbmcPktIfFlagLock", 2, NULL);
		if (result >= 0) {
			g_bbmcPacketInterface = control;
			control->expiryLock = 0;
			control->activeCalls = 0;
			return 0;
		}
		bbmcFinalizeFastMutex(&control->transmitMutex);
	}
	if (control->transmitBuffers[0].data != NULL)
		bbmcFree(control->transmitBuffers[0].data);
	if (control->transmitBuffers[1].data != NULL)
		bbmcFree(control->transmitBuffers[1].data);
	for (i = 0; i < 4; ++i) {
		if (control->receiveBuffers[i].data != NULL)
			bbmcFree(control->receiveBuffers[i].data);
	}
	memset(control, 0, sizeof(*control));
	return result;
}

int bbmcFinalizeUsbPacketInterface(void)
{
	BbmcPacketInterface *control = g_bbmcPacketInterface;
	int i;

	if (control->receivePipe != 0) {
		SceUsbdForDriver_F304DC5C(control->receivePipe);
		control->receivePipe = 0;
	}
	if (control->transmitPipe != 0) {
		SceUsbdForDriver_F304DC5C(control->transmitPipe);
		control->transmitPipe = 0;
	}
	bbmcFinalizeFastMutex(&control->transmitMutex);
	bbmcFinalizeFastMutex(&control->flagMutex);
	if (control->transmitBuffers[0].data != NULL)
		bbmcFree(control->transmitBuffers[0].data);
	if (control->transmitBuffers[1].data != NULL)
		bbmcFree(control->transmitBuffers[1].data);
	for (i = 0; i < 4; ++i) {
		if (control->receiveBuffers[i].data != NULL)
			bbmcFree(control->receiveBuffers[i].data);
	}
	memset(control, 0, sizeof(*control));
	g_bbmcPacketInterface = NULL;
	return 0;
}

/* name unknown: publish the baseband driver's complete network interface. */
int bbmcRegisterNetworkInterface(void)
{
	BbmcPacketInterface *control = g_bbmcPacketInterface;

	if (control == NULL)
		return 0x80330006;
	control->driver.multicast = bbmcSetNetworkMulticast;
	control->driver.control = GetCommRate;
	control->driver.stop = bbmcStopNetworkInterface;
	control->driver.start = bbmcStartNetworkInterface;
	control->driver.open = ConnectNetwork;
	control->driver.close = bbmcCloseNetworkLink;
	control->driver.kickTx = bbmcKickNetworkTransmit;
	memset(&control->driver.txBytes, 0, 64);
	control->driver.capabilities = 0;
	control->driver.type = 4;
	__builtin_memcpy(control->driver.mac.data, control->localMac.data, 4);
	__builtin_memcpy(&control->driver.mac.data[4], &control->localMac.data[4], 2);
	control->driver.prefix = 0;
	control->driver.private = control;
	control->driver.mtu = 1500;
	return SceNetPsForDriver_1ABF937D(&control->driver);
}

/* name unknown: drain active driver calls before unregistering the interface. */
int bbmcUnregisterNetworkInterface(void)
{
	BbmcPacketInterface *control = g_bbmcPacketInterface;
	int i;

	if (control == NULL)
		return 0x80330006;
	for (i = 3000; i != 0; --i) {
		if (control->activeCalls == 0)
			break;
		sceKernelDelayThread(1000);
	}
	return SceNetPsForDriver_F5788FF8(&control->driver);
}

/* name unknown: report a packet service status change from the modem as interface events */
int bbmcSetPacketServiceStatus(int connectionStatus, int reconfigurationRequired)
{
	unsigned int events = 0;

	if (connectionStatus == 2)
		events = 4;
	else if (connectionStatus == 1)
		events = 8;
	if (reconfigurationRequired == 1)
		events |= 32;
	return bbmcNotifyPacketInterfaceEvent(events, 0);
}

/*
 * name unknown: apply interface events (1 link up, 2 link down, 4 USB attached, 8 USB
 * detached, 16 failed, 32 address lost, 64 call ended, 0x10000 stopped) and report them
 * to SceNetPs, with status when one is given.
 */
int bbmcNotifyPacketInterfaceEvent(unsigned int events, int status)
{
	BbmcControl *control = bbmcGetControl();
	BbmcPacketInterface *interface = &g_bbmcPacketInterfaceStorage;
	int (*notify)(struct NetDriver *driver, SceUInt64 events, const void *data,
		      int size);
	SceKernelIntrStatus intr;
	SceUInt32 linkState;
	SceUInt32 low;
	SceUInt32 high;
	int statusValid;
	int started;
	int result;
	int i;

	if (control->controlFlags & 4)
		return 0;
	bbmcEnterCS(&control->mutex);
	if (control->connectPending != 0 && (events & 1)) {
		control->connectPending = 0;
		started = 1;
		bbmcLockFastMutex(&interface->flagMutex);
		interface->linkOpen = 1;
		bbmcUnlockFastMutex(&interface->flagMutex);
		linkState = 3;
	} else {
		started = 0;
		linkState = 2;
	}
	bbmcLeaveCS(&control->mutex);
	low = 0;
	high = (events & 2) ? linkState : started;

	result = events & 4;
	if (events & 4) {
		result = qmiGetUsbDeviceSpeed(&interface->usbSpeed);
		if (result < 0)
			return result;
		interface->maxPacketSize = interface->usbSpeed == 2 ? 512 : 64;
		result = qmiOpenUsbBulkOut();
		if (result < 0)
			return result;
		interface->transmitPipe = result;
		result = qmiOpenUsbBulkIn();
		if (result < 0) {
			qmiCloseUsbPipe(interface->transmitPipe);
			return result;
		}
		interface->receivePipe = result;
		for (i = 0; i < 4; i++)
			result = SceUsbdForDriver_235C094A(interface->receivePipe,
				interface->receiveBuffers[i].data,
				interface->receiveBuffers[i].length,
				(SceUsbdDoneCallback)bbmcPacketReceiveCompleted,
				&interface->receiveBuffers[i]);
		if (result < 0)
			return result;
		low = 1;
		bbmcLockFastMutex(&interface->flagMutex);
		interface->usbAttached = 1;
		bbmcUnlockFastMutex(&interface->flagMutex);
		interface->expiryTimer = SceThreadmgrForDriver_C58DF384(
			"SceBbmcPacketIfExpire", 3600000000U, bbmcPacketInterfaceExpire,
			interface);
	}

	statusValid = 0;
	if (events & 8) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&interface->expiryLock);
		if (interface->expiryTimer > 0)
			sceKernelCancelAlarm(interface->expiryTimer);
		interface->expiryCount = 0;
		interface->expiryTimer = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&interface->expiryLock, intr);
		interface->ipv4Address = 0;
		qmiCloseUsbPipe(interface->receivePipe);
		interface->receivePipe = 0;
		qmiCloseUsbPipe(interface->transmitPipe);
		interface->transmitPipe = 0;
		bbmcEnterCS(&control->mutex);
		if (control->suspendState != 0) {
			if (control->operationBusy != 0)
				control->operationBusy = 0;
		} else if (control->operationBusy != 0) {
			high |= 16;
			control->operationBusy = 0;
			status = (int)0x80330509;
			statusValid = 1;
		} else {
			low |= 2;
		}
		control->pendingPacketRequest = 0;
		bbmcLockFastMutex(&interface->flagMutex);
		interface->usbAttached = 0;
		bbmcUnlockFastMutex(&interface->flagMutex);
		bbmcLeaveCS(&control->mutex);
	}
	if (events & 32) {
		low |= 4;
		interface->ipv4Address = 0;
	}
	if (events & 0x10000)
		low |= 0x100000;

	notify = interface->driver.notify;
	bbmcEnterCS(&control->mutex);
	if ((control->connectPending != 0 || control->closeNetworkLinkInProgress != 0) &&
	    (events & 16)) {
		high |= 8;
		if (control->connectPending != 0) {
			control->connectPending = 0;
			interface->linkOpen = 1;
			if (status == (int)0x80330505 && control->reservedOperation == 0)
				bbmcNotifyFeatureEvent(control, 5, 0x20);
		}
		bbmcLeaveCS(&control->mutex);
	} else {
		bbmcLeaveCS(&control->mutex);
		if ((events & 64) == 0) {
			if (low == 0 && high == 0)
				return result;
			if (!statusValid) {
				notify(&interface->driver, ((SceUInt64)high << 32) | low, NULL, 0);
				return result;
			}
			goto report;
		}
	}
	if (events & 64)
		high |= 16;
report:
	notify(&interface->driver, ((SceUInt64)high << 32) | low, &status, 4);
	return result;
}

/* the driver's close: take the link down and report it */
int bbmcCloseNetworkLink(struct NetDriver *driver, int argument)
{
	BbmcPacketInterface *interface = driver->private;
	BbmcControl *control = bbmcGetControl();
	int result;

	if (control == NULL)
		return 0x80330003;
	sceKernelAtomicAddAndGet32((SceInt32 *)&interface->activeCalls, 1);
	bbmcEnterCS(&control->mutex);
	control->closeNetworkLinkInProgress = 1;
	bbmcLockFastMutex(&interface->flagMutex);
	if (interface->linkOpen != 0) {
		interface->linkOpen = 0;
		result = 0;
		bbmcNotifyPacketInterfaceEvent(2, 0);
	} else {
		result = 0x80330508;
		bbmcNotifyPacketInterfaceEvent(16, 0x80330508);
	}
	bbmcUnlockFastMutex(&interface->flagMutex);
	control->closeNetworkLinkInProgress = 0;
	bbmcLeaveCS(&control->mutex);
	sceKernelAtomicSubAndGet32((SceInt32 *)&interface->activeCalls, 1);
	return result;
}

int bbmcStartUsbSerialDevice(int argumentSize, void *arguments, void *context);
int bbmcStopUsbSerialDevice(int argumentSize, void *arguments, void *context);

int bbmcStopUsbSerialDevice(int argumentSize, void *arguments, void *context)
{
	BbmcUsbSerialDevice *device;

	sceKernelSetEventFlag(g_usbSerialDevice->event, 0x80000000u);
	sceKernelWaitThreadEnd(g_usbSerialDevice->thread, NULL, NULL);
	device = g_usbSerialDevice;
	if (device->memoryBlock > 0) {
		sceKernelFreeMemBlock(device->memoryBlock);
		device = g_usbSerialDevice;
		device->memoryBlock = 0;
	}
	device->controlOutBuffer = NULL;
	device->controlInBuffer = NULL;
	device->qcomOutBuffer = NULL;
	device->qcomInBuffer = NULL;
	device->atInterruptBuffer = NULL;
	device->atOutBuffer = NULL;
	device->atInBuffer = NULL;
	return 0;
}

/* the driver's open: ask the manager to start the packet network (message 103) */
int ConnectNetwork(struct NetDriver *driver, int argument)
{
	BbmcPacketInterface *interface = driver->private;
	BbmcControl *control = bbmcGetControl();
	BbmcControl *current;
	BbmcMessage *message;
	SceUInt32 suspendState;
	SceUInt32 scanState;
	SceUInt32 powerTransitionBusy;
	int currentState;
	int status;
	int result;

	if (control == NULL)
		return 0x80330003;
	sceKernelAtomicAddAndGet32((SceInt32 *)&interface->activeCalls, 1);
	bbmcEnterCS(&control->mutex);
	bbmcLockFastMutex(&interface->flagMutex);
	if (control->connectPending != 0 || interface->linkOpen != 0) {
		result = 0x80330506;
		bbmcNotifyPacketInterfaceEvent(16, 0x80330506);
		goto out;
	}
	control->connectPending = 1;
	if (interface->usbAttached != 0 && interface->disconnectPending == 0) {
		bbmcNotifyPacketInterfaceEvent(1, 0);
		result = 0;
		goto out;
	}

	current = bbmcGetControl();
	bbmcEnterCS(&current->mutex);
	currentState = current->currentState;
	scanState = current->networkScanState;
	suspendState = current->suspendState;
	powerTransitionBusy = current->powerTransitionBusy;
	bbmcLeaveCS(&current->mutex);
	if ((suspendState & ~4U) != 0) {
		status = 0x8033000A;
	} else if ((current->powerEventActive != 0 || current->recentPowerEvent == 0) &&
		   (powerTransitionBusy != 0 || currentState != 7)) {
		status = 0x80330501;
	} else if (current->powerPolicy == 0 && currentState == 6) {
		status = 0x80330501;
	} else if (current->normalizedControlState != 0) {
		status = 0x80330502;
	} else if (scanState != 0) {
		status = 0x8033000D;
	} else {
		bbmcEnterCS(&current->mutex);
		if (current->pendingPacketRequest != 0 && interface->disconnectPending == 0) {
			bbmcLeaveCS(&current->mutex);
			status = 0x80330504;
		} else {
			current->pendingPacketRequest = 1;
			bbmcLeaveCS(&current->mutex);
			message = msgAllocMsgPacket(current, 103);
			if (message == NULL) {
#if FW_DEBUG
				sceKernelAssert(0, &dbg_ConnectNetwork_L910,
						__builtin_return_address(0));
#endif
				status = 0x80330005;
			} else {
				message->body.networkStart.networkHandle =
					&interface->networkHandle;
				message->body.networkStart.ipv4Address = &interface->ipv4Address;
				message->body.networkStart.hasIpv4Address =
					&interface->hasIpv4Address;
				message->body.networkStart.runtimeValue = &interface->runtimeValue;
				result = msgSendMsgPacket(current, message);
				if (result >= 0)
					goto out;
				status = result;
			}
		}
	}
	result = 0;
	bbmcNotifyPacketInterfaceEvent(16, status);
out:
	bbmcUnlockFastMutex(&interface->flagMutex);
	bbmcLeaveCS(&control->mutex);
	sceKernelAtomicSubAndGet32((SceInt32 *)&interface->activeCalls, 1);
	return result;
}

int bbmcStartUsbSerialDevice(int argumentSize, void *arguments, void *context)
{
	SceKernelAllocMemBlockKernelOpt option;
	BbmcUsbSerialDevice *device = g_usbSerialDevice;
	void *base = NULL;
	unsigned char *buffer;
	int result;

	if (!device) {
		result = 0x80330003;
		goto failed;
	}
	__builtin_memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	result = sceKernelAllocMemBlockForKernel("SceUsbQcomSer", 0x1020d006,
				       8192, &option);
	device = g_usbSerialDevice;
	if (result < 0)
		goto failed;
	device->memoryBlock = result;
	result = sceKernelGetMemBlockBase(result, &base);
	if (result < 0)
		goto reload;
	buffer = (unsigned char *)(((SceUIntPtr)base + 63) & ~63u);
	device = g_usbSerialDevice;
	device->controlOutBuffer = buffer + 64;
	device->controlIn.request.data = buffer + 64;
	device->qcomIn.request.data = buffer + 128;
	device->controlInBuffer = buffer;
	device->controlOut.request.next = NULL;
	device->controlOut.request.unused = device;
	device->controlOut.request.data = buffer;
	device->controlIn.request.onComplete = bbmcUsbDeviceControlSent;
	device->controlOut.request.onComplete = bbmcUsbDeviceControlOutCompleted;
	device->qcomIn.request.endpoint = &g_usbSerialEndpoints[1];
	device->qcomInBuffer = device->qcomIn.request.data;
	device->controlIn.request.size = 64;
	device->controlOut.request.size = 64;
	device->controlIn.request.endpoint = &g_usbSerialEndpoints[0];
	device->controlOut.request.endpoint = &g_usbSerialEndpoints[0];
	device->qcomOutBuffer = buffer + 1152;
	device->atInterruptBuffer = buffer + 2176;
	device->atOutBuffer = buffer + 4736;
	device->atInBuffer = buffer + 3200;
	device->controlIn.request.isControlRequest = 1;
	device->controlIn.request.returnCode = 1;
	device->controlIn.request.next = NULL;
	device->controlIn.request.unused = device;
	device->controlIn.request.attributes = 0;
	device->controlIn.request.transmitted = 0;
	device->controlOut.request.attributes = 0;
	device->controlOut.request.transmitted = 0;
	device->controlOut.request.isControlRequest = 1;
	device->qcomIn.request.size = 1024;
	device->qcomIn.request.isControlRequest = 1;
	device->qcomOut.request.size = 1024;
	device->qcomOut.request.isControlRequest = 1;
	device->qcomOut.request.data = buffer + 1152;
	device->atInterruptIn.request.data = buffer + 2176;
	device->atIn.request.data = buffer + 3200;
	device->atInterruptIn.request.size = 1024;
	device->qcomIn.request.onComplete = bbmcUsbDeviceQcomInCompleted;
	device->controlOut.request.returnCode = 1;
	device->qcomIn.request.transmitted = 0;
	device->qcomIn.request.returnCode = 1;
	device->qcomOut.request.transmitted = 0;
	device->qcomOut.request.returnCode = 1;
	device->atInterruptIn.request.isControlRequest = 1;
	device->atInterruptIn.request.transmitted = 0;
	device->atInterruptIn.request.returnCode = 1;
	device->atIn.request.isControlRequest = 1;
	device->qcomOut.request.endpoint = &g_usbSerialEndpoints[2];
	device->atInterruptIn.request.onComplete = bbmcUsbDeviceAtInterruptCompleted;
	device->qcomIn.request.next = NULL;
	device->qcomIn.request.unused = device;
	device->qcomIn.request.attributes = 0;
	device->qcomOut.request.next = NULL;
	device->qcomOut.request.unused = device;
	device->qcomOut.request.attributes = 0;
	device->atInterruptIn.request.next = NULL;
	device->atInterruptIn.request.unused = device;
	device->atInterruptIn.request.attributes = 0;
	device->atIn.request.next = NULL;
	device->atIn.request.unused = device;
	device->qcomOut.request.onComplete = bbmcUsbDeviceQcomOutCompleted;
	device->atInterruptIn.request.endpoint = &g_usbSerialEndpoints[3];
	device->atIn.request.endpoint = &g_usbSerialEndpoints[5];
	device->atIn.request.attributes = 0;
	device->atIn.request.size = 1536;
	device->atOut.request.endpoint = &g_usbSerialEndpoints[4];
	device->atOut.request.data = buffer + 4736;
	device->atIn.request.transmitted = 0;
	device->atIn.request.returnCode = 1;
	device->atOut.request.size = 1536;
	device->atOut.request.isControlRequest = 1;
	device->atOut.request.next = NULL;
	device->atOut.request.unused = device;
	device->atOut.request.transmitted = 0;
	device->atOut.request.returnCode = 1;
	device->atIn.request.onComplete = bbmcUsbDeviceAtInCompleted;
	device->atOut.request.attributes = 0;
	device->atOut.request.onComplete = bbmcUsbDeviceAtOutCompleted;
	result = sceKernelStartThread(device->thread, 0, NULL);
	if (result >= 0)
		return result;
reload:
	device = g_usbSerialDevice;
failed:
	if (device->memoryBlock > 0) {
		sceKernelFreeMemBlock(device->memoryBlock);
		device = g_usbSerialDevice;
		device->memoryBlock = 0;
	}
	device->controlInBuffer = NULL;
	device->controlOutBuffer = NULL;
	device->qcomOutBuffer = NULL;
	device->qcomInBuffer = NULL;
	device->atInterruptBuffer = NULL;
	device->atOutBuffer = NULL;
	device->atInBuffer = NULL;
	return result;
}

/* ask the manager to stop the packet network (message 104), or retry a start (107) */
int DisconnectNetwork(void)
{
	BbmcPacketInterface *interface = g_bbmcPacketInterface;
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	SceUInt32 startBusy;
	SceUInt32 packetRequest;
	int result;

	bbmcEnterCS(&control->mutex);
	startBusy = control->networkStartBusy;
	packetRequest = control->pendingPacketRequest;
	bbmcLeaveCS(&control->mutex);
	if (startBusy != 0) {
		bbmcEnterCS(&control->mutex);
		if (control->networkRetryPending != 0) {
			bbmcLeaveCS(&control->mutex);
			return 0x80330507;
		}
		if (control->deferredNetworkStart != 0) {
			result = 0x80330508;
			bbmcSendSimpleMessage(control, 107);
		} else {
			control->networkRetryPending = 1;
			result = bbmcSendSimpleMessage(control, 107);
		}
		bbmcLeaveCS(&control->mutex);
		return result;
	}
	if (packetRequest == 0)
		return 0x80330508;
	bbmcEnterCS(&control->mutex);
	bbmcLockFastMutex(&interface->flagMutex);
	if (interface->disconnectPending != 0) {
		result = 0x80330507;
	} else if (control->networkStarted != 0 || control->clientOnline != 0 ||
		   control->clientInitialized != 0) {
		interface->disconnectPending = 1;
		message = msgAllocMsgPacket(control, 104);
		if (message == NULL) {
			result = 0x80330005;
#if FW_DEBUG
			sceKernelAssert(0, &dbg_DisconnectNetwork_L1005,
					__builtin_return_address(0));
#endif
		} else {
			message->body.networkStop.networkHandle = &interface->networkHandle;
			control->reservedOperation = 0;
			result = msgSendMsgPacket(control, message);
			if (result < 0)
				control->reservedOperation = 0;
		}
	} else {
		result = 0x80330508;
	}
	bbmcUnlockFastMutex(&interface->flagMutex);
	bbmcLeaveCS(&control->mutex);
	return result;
}

/* name unknown: clear the detached USB state and cancel its expiry alarm. */
int bbmcClearPacketInterfaceUsbState(void)
{
	BbmcPacketInterface *control;
	SceUID timer;
	int result;

	result = bbmcLockFastMutex(&g_bbmcPacketInterface->flagMutex);
	if (result < 0)
		return result;
	control = g_bbmcPacketInterface;
	if (control->usbAttached)
		control->usbAttached = 0;
	timer = control->expiryTimer;
	control->disconnectPending = 0;
	if (timer > 0) {
		sceKernelCancelAlarm(timer);
		control = g_bbmcPacketInterface;
	}
	control->expiryTimer = 0;
	return bbmcUnlockFastMutex(&control->flagMutex);
}
