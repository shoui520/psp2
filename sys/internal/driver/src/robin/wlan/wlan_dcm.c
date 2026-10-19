/* WLAN DCM family; this opening hook has no source-name debug record. */
#undef __FILE_NAME__
#define __FILE_NAME__ "../robin/wlan/wlan_dcm.c"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sdif/sdif.h>
#include <threadmgr/threadmgr.h>
#include <wlanbt/wlanbt.h>
#include <wlanbt/wlanbt_protocol.h>
#include "../../wlanbt/wlanbt_internal.h"
#include <intrmgr/intrmgr.h>
#include <string.h>
#include <net_ps/netps_thread.h>
#include <net_ps/net_ps_bsd.h>

static const WlanLlcSnapPrefix gWlanRfc1042Prefix = {
	.destinationSap = 0xaa,
	.sourceSap = 0xaa,
	.control = 3,
	.organization = { 0, 0, 0 },
};

static unsigned int gCommandEventIndex;

void robinWlanReceiveMessages(SceWlanBtRobinWlanDcm *dcm);
int robinWlanTransmitPackets(SceWlanBtRobinWlanDcm *dcm);

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, gDcmInterruptAssert, 711, "robinWlanDcmInterruptRequest");
SCE_DBGINFO_DEFINE(static, gDcmInterruptError, 711, "robinWlanDcmInterruptRequest");
SCE_DBGINFO_DEFINE(static, gDcmClearPendingCommand, 931, "_commandClear");
SCE_DBGINFO_DEFINE(static, gDcmClearCurrentCommand, 919, "_commandClear");
#endif

int robinCompleteInitialization(void)
{
	return 0;
}

void robinWlanCompleteSdioTx(SdifIoResult *result)
{
	int error = result->errorCode;
	SceWlanBtRobinWlanDcm *dcm;
	unsigned int index, slot;

	if (!error)
		return;
	index = result->index;
	dcm = result->argument;
	if (dcm->txTransfers[0].index == index)
		slot = 0;
	else if (dcm->txTransfers[1].index == index)
		slot = 1;
	else if (dcm->txTransfers[2].index == index)
		slot = 2;
	else if (dcm->txTransfers[3].index == index)
		slot = 3;
	else if (dcm->txTransfers[4].index == index)
		slot = 4;
	else if (dcm->txTransfers[5].index == index)
		slot = 5;
	else if (dcm->txTransfers[6].index == index)
		slot = 6;
	else if (dcm->txTransfers[7].index == index)
		slot = 7;
	else if (dcm->txTransfers[8].index == index)
		slot = 8;
	else if (dcm->txTransfers[9].index == index)
		slot = 9;
	else if (dcm->txTransfers[10].index == index)
		slot = 10;
	else if (dcm->txTransfers[11].index == index)
		slot = 11;
	else if (dcm->txTransfers[12].index == index)
		slot = 12;
	else if (dcm->txTransfers[13].index == index)
		slot = 13;
	else if (dcm->txTransfers[14].index == index)
		slot = 14;
	else
		return;
	dcm->txTransfers[slot].errorCode = error;
}

int robinWlanSubInterrupt(void *subintrArgument, void *registerArgument,
			 unsigned char priority)
{
	(void)subintrArgument;
	(void)registerArgument;
	(void)priority;
	return robinWlanNotifyWorker(64);
}

int _rxMessage(SceWlanBtRobinWlanDcm *dcm, WlanSdioMessageHeader *message,
	       unsigned int size, unsigned int *received)
{
	SceWlanBtRobinWlanContext *context = dcm->context;
	struct NetDriverPacket *packet = NULL;
	unsigned int messageType;
	unsigned int delivered = 0;
	WlanRxDescriptor *descriptor;
	struct NetDriver *driver;
	uint8_t *frame, *end;
	unsigned int length;

	if (context->status & 4) {
		unsigned int headroom = context->driver.prefix;

		packet = context->driver.allocateRx(&context->driver, size - headroom, 64);
		if (packet) {
			WlanSdioMessageHeader *copy =
				(void *)(packet->buffer->data - headroom);

			memcpy(copy, message, size);
			message = copy;
		}
	}
	messageType = message->messageType;
	if (messageType == 1) {
		WlanFirmwareCommandHeader *response = (void *)(message + 1);
		unsigned int responseId = response->commandId;
		unsigned int sequence = response->sequence.number;
		SceWlanBtRobinWlanCommand *command;

		lockFastMutex(&dcm->commandMutex);
		wlanBtNotifyMonitor(0x20002, messageType, response->commandSize, response);
		command = dcm->currentCommand;
		if (command) {
			WlanFirmwareCommandHeader *request =
				(void *)((WlanSdioMessageHeader *)command->requestBuffer + 1);

			if (responseId == (command->commandId | 0x8000) &&
			    request->sequence.number == sequence) {
				unsigned int result = response->result;
				unsigned int responseSize;

				if (result) {
					SceDebugForDriver_391B74B7("[SceWlanBt]:  Command failed. "
							 "(com=0x%04x, resp=0x%04x, result=%d)\n",
							 command->commandId, responseId, result);
					command->result = 0x80418004;
				} else {
					command->result = result;
				}
				responseSize = response->commandSize - 8;
				command->responseId = responseId;
				command->responsePayload = response + 1;
				command->responsePacket = packet;
				command->responseSize = responseSize;
				dcm->currentCommand = NULL;
				goto nextCommand;
			}
			SceDebugForDriver_391B74B7("[SceWlanBt]: Skip unexpected Response. (resp=0x%04x)\n",
					 responseId);
		}
		dcm->context->driver.releaseTx(&dcm->context->driver, packet);
		command = dcm->currentCommand;
		if (command) {
			sceKernelUnlockFastMutex(&dcm->commandMutex);
			return 0;
		}
nextCommand:
		if (dcm->pendingCommands) {
			SceWlanBtRobinWlanCommand *pending = dcm->pendingCommands;
			SceWlanBtRobinWlanCommand *next = pending->next;

			dcm->currentCommand = pending;
			dcm->pendingCommands = next;
			robinWlanNotifyWorker(256);
		}
		sceKernelUnlockFastMutex(&dcm->commandMutex);
		if (command) {
			if (command->callback)
				command->callback(dcm->context, command);
			else
				sceKernelSetEventFlag(dcm->commandEventFlag, command->eventMask);
		}
		return 0;
	}
	if (messageType) {
		if (messageType == 3) {
			void *event = message + 1;

			wlanBtNotifyMonitor(0x20003, 0, 16, event);
			robinWlanTcmEventReceive(dcm->context, event);
			if (packet)
				dcm->context->driver.releaseTx(&dcm->context->driver, packet);
		}
		return 0;
	}
	descriptor = (void *)(message + 1);
	context = dcm->context;
	driver = &context->driver;
	if (descriptor->header.packetType == 2) {
		struct NetDriverBuffer *buffer;
		unsigned int rxBytes, rxPackets;

		length = descriptor->header.packetLength;
		frame = (uint8_t *)descriptor + descriptor->header.packetOffset;
		if (!packet) {
			++driver->rxDrops;
			goto done;
		}
		buffer = packet->buffer;
		rxBytes = driver->rxBytes + length;
		rxPackets = driver->rxPackets + 1;
		buffer->data = frame;
		buffer->end = frame + length;
		packet->length = length;
		driver->rxBytes = rxBytes;
		driver->rxPackets = rxPackets;
		if (frame[0] == 255 && frame[1] == 255 && frame[2] == 255 &&
		    frame[3] == 255 && frame[4] == 255 && frame[5] == 255) {
			driver->rxBroadcastBytes += length;
			++driver->rxBroadcastPackets;
		} else if (frame[0] & 1) {
			driver->rxMulticastBytes += length;
			++driver->rxMulticastPackets;
		}
		delivered = 1;
		driver->deliverRx(driver, packet);
		goto done;
	}
	if (descriptor->header.packetType != 230)
		goto release;
	frame = (uint8_t *)descriptor + descriptor->header.packetOffset;
	end = frame + descriptor->header.packetLength;
	while (end >= frame + sizeof(struct ether_header)) {
		struct ether_header *header = (void *)frame;
		const uint8_t *llc = (const void *)(header + 1);
		struct NetDriverPacket *subframe;
		unsigned int frameSize;

		length = __builtin_bswap16(header->ether_type);
		if (end < llc + length || length <= 5)
			break;
		if (memcmp(llc, &gWlanRfc1042Prefix, sizeof(gWlanRfc1042Prefix)))
			break;
		if (end < llc + sizeof(gWlanRfc1042Prefix) + length - 6)
			break;
		frameSize = length + 6;
		subframe = driver->allocateRx(driver, frameSize, 64);
		if (subframe) {
			struct NetDriverBuffer *buffer;
			unsigned int rxBytes, rxPackets;

			memcpy(subframe->buffer->end, frame, 2 * ETHER_ADDR_LEN);
			subframe->buffer->end += 2 * ETHER_ADDR_LEN;
			memcpy(subframe->buffer->end, llc + sizeof(gWlanRfc1042Prefix), length - 6);
			buffer = subframe->buffer;
			rxBytes = driver->rxBytes + frameSize;
			rxPackets = driver->rxPackets + 1;
			frame = buffer->data;
			buffer->end += length - 6;
			subframe->length = frameSize;
			driver->rxBytes = rxBytes;
			driver->rxPackets = rxPackets;
			if (frame[0] == 255 && frame[1] == 255 && frame[2] == 255 &&
			    frame[3] == 255 && frame[4] == 255 && frame[5] == 255) {
				driver->rxBroadcastBytes += frameSize;
				++driver->rxBroadcastPackets;
			} else if (frame[0] & 1) {
				driver->rxMulticastBytes += frameSize;
				++driver->rxMulticastPackets;
			}
			++delivered;
			driver->deliverRx(driver, subframe);
		} else {
			++driver->rxDrops;
		}
		frame = (void *)(((uintptr_t)llc + length + 3) & ~(uintptr_t)3);
	}
release:
	if (packet)
		driver->releaseTx(driver, packet);
done:
	*received += delivered;
	return 0;
}

void robinWlanReceiveMessages(SceWlanBtRobinWlanDcm *dcm)
{
	unsigned int bitmap = dcm->readBitmap;

	while (bitmap) {
		SceWlanBtRobinWlanContext *context;
		unsigned int port, size, received;
		int readSingle, flush, result;

		if (bitmap & 1) {
			bitmap &= 0xfffe;
			dcm->readBitmap = bitmap;
			port = 0;
		} else {
			unsigned int next = (uint16_t)(dcm->nextReadPort + 1);

			port = dcm->nextReadPort;
			if (!((bitmap >> port) & 1))
				break;
			bitmap &= ~(1U << port);
			dcm->readBitmap = bitmap;
			port = (uint8_t)port;
			dcm->nextReadPort = next == 16 ? 1 : next;
		}
		size = dcm->status.readLengths[port][0] |
		       dcm->status.readLengths[port][1] << 8;
		if (!size)
			continue;
		context = dcm->context;
		received = 0;
		size = (size + 255) & ~255U;
		readSingle = 0;
		flush = 0;
		result = 0;
		if (!port) {
			readSingle = 1;
		} else {
			unsigned int count = dcm->aggregateCount;
			unsigned int total = dcm->aggregateSize + size;
			unsigned int start = dcm->aggregateStartPort;

			if ((bitmap & ~1U) && !count) {
				start = port;
				dcm->aggregateStartPort = port;
			} else if (!count || total > dcm->aggregateCapacity) {
				readSingle = 1;
				flush = count != 0;
				goto readBatch;
			}
			dcm->aggregateSize = total;
			if (port < start) {
				++count;
				dcm->aggregateCount = count;
				dcm->aggregatePortBitmap |= 1U << count;
			} else {
				dcm->aggregateCount = count + 1;
				dcm->aggregatePortBitmap |= 1U << count;
				++count;
			}
			flush = !(bitmap & ~1U);
			if (count == 8 ||
			    (dcm->nextReadPort < start ? dcm->nextReadPort + 16 - start :
			     dcm->nextReadPort - start) > 7)
				flush = 1;
		}
readBatch:
		if (flush) {
			if (dcm->aggregateSize <= 0x4000) {
				result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction,
					dcm->aggregateStartPort + (dcm->ioPort | 0x1000 |
					(uint16_t)(dcm->aggregatePortBitmap << 4)),
					dcm->aggregateSize, dcm->aggregateBuffer, 2);
				if (result >= 0 && dcm->aggregateCount) {
					WlanSdioMessageHeader *message = (void *)dcm->aggregateBuffer;
					uint8_t *limit = dcm->aggregateBuffer + 0x4000;
					uint8_t *next = (uint8_t *)message +
						((message->messageSize + 255U) & ~255U);
					unsigned int index = 0;

					while (limit >= next) {
						_rxMessage(dcm, message, message->messageSize, &received);
						if (++index >= dcm->aggregateCount)
							break;
						message = (void *)next;
						next += (message->messageSize + 255U) & ~255U;
					}
				}
			} else {
				SceDebugForDriver_391B74B7("[SceWlanBt]: too large message size=%d\n", size);
			}
			dcm->aggregateStartPort = 0;
			dcm->aggregateCount = 0;
			dcm->aggregateSize = 0;
			dcm->aggregatePortBitmap = 0;
		}
		if (readSingle) {
			if (size <= 0x4000) {
				WlanSdioMessageHeader *message;

				result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction,
					dcm->ioPort + port, size, dcm->aggregateBuffer, 2);
				if (result >= 0) {
					message = (void *)dcm->aggregateBuffer;
					if (size == ((message->messageSize + 255U) & ~255U))
						_rxMessage(dcm, message, message->messageSize, &received);
				}
			} else {
				SceDebugForDriver_391B74B7("[SceWlanBt]: too large message size=%d\n", size);
			}
		}
		if (received)
			context->driver.notify(&context->driver, NULL,
				NET_DRIVER_EVENT_STATUS, NET_DRIVER_STATUS_RECEIVED, NULL, 0);
		if (result < 0)
			break;
		bitmap = dcm->readBitmap;
	}
}

int robinWlanTransmitPackets(SceWlanBtRobinWlanDcm *dcm)
{
	SceWlanBtRobinWlanContext *context = dcm->context;
	struct NetDriver *driver;
	unsigned int count = 0, index;
	unsigned int bytes = 0, packets = 0, broadcastBytes = 0, broadcastPackets = 0;
	unsigned int multicastBytes = 0, multicastPackets = 0, drops = 0;
	int result;

	if (context->status & 0x80010) {
		robinWlanNotifyWorker(0x10000);
		dcm->context->transmitDeferred = 1;
		return 0;
	}
	if (context->transmitDeferred)
		context->transmitDeferred = 0;
	__atomic_exchange_n(&context->transmitRequests, 1, __ATOMIC_RELEASE);
	context = dcm->context;
	driver = &context->driver;
	while ((dcm->writeBitmap & 0xfffe) &&
	       ((dcm->writeBitmap >> dcm->nextWritePort) & 1)) {
		struct NetDriverPacket *packet = driver->dequeueTx(driver);
		WlanSdioMessageHeader *message;
		WlanTxDescriptor *descriptor;
		SceWlanBtRobinWlanTxTransfer *slot;
		unsigned int size, port, next;

		if (!packet)
			break;
		size = packet->length + sizeof(*message) + sizeof(*descriptor);
		message = (void *)(packet->buffer->data - driver->prefix);
		message->messageSize = size;
		message->messageType = 0;
		descriptor = (void *)(message + 1);
		memset(descriptor, 0, sizeof(*descriptor));
		descriptor->header.packetOffset = sizeof(*descriptor);
		descriptor->priority = 0;
		descriptor->header.packetLength = packet->length;
		if (!(dcm->writeBitmap & 0xfffe) ||
		    !((dcm->writeBitmap >> dcm->nextWritePort) & 1)) {
			driver->releaseTx(driver, packet);
			break;
		}
		port = dcm->nextWritePort;
		next = (uint16_t)(port + 1);
		dcm->writeBitmap &= ~(1U << port);
		dcm->nextWritePort = next == 16 ? 1 : next;
		slot = &dcm->txTransfers[count++];
		memset(slot, 0, sizeof(*slot));
		slot->size = size;
		slot->buffer = message;
		slot->packet = packet;
		slot->address = dcm->ioPort + (uint8_t)port;
		if (count == 15)
			break;
	}
	if (!(dcm->writeBitmap & 0xfffe) ||
	    !((dcm->writeBitmap >> dcm->nextWritePort) & 1))
		dcm->waitingForWritePort = 1;
	if (!count) {
		result = 0x80418007;
		goto done;
	}
	for (index = 0; index < count; ++index) {
		SceWlanBtRobinWlanTxTransfer *slot = &dcm->txTransfers[index];

		if (index == count - 1) {
			result = SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, slot->address,
				(slot->size + 255) & ~255U, slot->buffer, 2);
		} else {
			SdifIoTransfer transfer;
			uint32_t transferIndex;

			transfer.reserved = 0;
			transfer.address = slot->address;
			transfer.length = (slot->size + 255) & ~255U;
			transfer.buffer = slot->buffer;
			transfer.flags = 2;
			transfer.argument = NULL;
			transfer.completion = robinWlanCompleteSdioTx;
			transfer.index = 0;
			do {
				result = SceSdifForDriver_855C95E1(dcm->sdioFunction,
								&transfer, &transferIndex);
			} while (result == (int)0x80320011);
		}
		if (result < 0)
			goto done;
	}
	for (index = 0; index < count; ++index) {
		SceWlanBtRobinWlanTxTransfer *slot = &dcm->txTransfers[index];
		struct NetDriverPacket *packet = slot->packet;
		unsigned int length = packet->length;

		if (slot->errorCode) {
			++drops;
		} else {
			const uint8_t *address = packet->buffer->data;

			bytes += length;
			++packets;
			if (address[0] == 255 && address[1] == 255 && address[2] == 255 &&
			    address[3] == 255 && address[4] == 255 && address[5] == 255) {
				broadcastBytes += length;
				++broadcastPackets;
			} else if (address[0] & 1) {
				multicastBytes += length;
				++multicastPackets;
			}
		}
		driver->releaseTx(driver, packet);
	}
	bytes += driver->txBytes;
	packets += driver->txPackets;
	multicastBytes += driver->txMulticastBytes;
	multicastPackets += driver->txMulticastPackets;
	broadcastBytes += driver->txBroadcastBytes;
	broadcastPackets += driver->txBroadcastPackets;
	drops += driver->txDrops;
	dcm->context->transmittedPackets += count;
	driver->txBytes = bytes;
	driver->txPackets = packets;
	driver->txMulticastBytes = multicastBytes;
	driver->txMulticastPackets = multicastPackets;
	driver->txBroadcastBytes = broadcastBytes;
	driver->txBroadcastPackets = broadcastPackets;
	driver->txDrops = drops;
done:
	if (!dcm->waitingForWritePort &&
	    (int)__atomic_fetch_sub(&dcm->context->transmitRequests, 1, __ATOMIC_RELEASE) > 1)
		robinWlanNotifyWorker(128);
	return result;
}

int robinWlanInitializeDcm(SceWlanBtRobinWlanDcm *dcm)
{
	SceKernelAllocMemBlockKernelOpt options;
	uint8_t *base;
	int result;

	result = sceKernelCreateEventFlag("SceWlanBtRobinWlanCommand", 0, 0, NULL);
	if (result < 0)
		return result;
	dcm->commandEventFlag = result;
	result = sceKernelInitializeFastMutex(&dcm->commandMutex,
					     "SceWlanBtRobinWlanCommand", 0x2000, NULL);
	if (result < 0)
		return result;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 0x200000;
	result = sceKernelAllocMemBlockForKernel("SceWlanBtRobinWlan", 0x1020d006, 0x1000, &options);
	if (result < 0)
		return result;
	dcm->commandMemory = result;
	result = sceKernelGetMemBlockBase(result, (void **)&base);
	if (result < 0)
		return result;
	dcm->requestBuffer = base;
	dcm->responseBuffer = base + 0x800;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = 0x200004;
	options.alignment = 0x1000;
	result = sceKernelAllocMemBlockForKernel("SceWlanBtRobinWlanSdioAggrRx",
				       0x10208006, 0x4000, &options);
	if (result < 0)
		return result;
	dcm->aggregateMemory = result;
	result = sceKernelGetMemBlockBase(result, (void **)&base);
	if (result >= 0) {
		dcm->aggregateCapacity = 0x4000;
		dcm->packetHeadroom = 20;
		dcm->aggregateBuffer = base;
	}
	return result;
}

int robinWlanPowerOn(void)
{
	int result = sceWlanBtSetConfiguration(3);
	unsigned int retries;

	if (result >= 0) {
		retries = 100;
		while (!(sceWlanBtGetConfiguration() & 2)) {
			sceKernelDelayThread(100000);
			if (!--retries)
				return result;
		}
		return 0;
	}
	return result;
}

int robinWlanPowerOff(void)
{
	int result = sceWlanBtSetConfiguration(4);
	unsigned int retries;

	if (result >= 0) {
		retries = 100;
		while (sceWlanBtGetConfiguration() & 2) {
			sceKernelDelayThread(100000);
			if (!--retries)
				return result;
		}
		return 0;
	}
	return result;
}

int robinWlanRestart(void)
{
	int result = sceWlanBtSetConfiguration(7);
	unsigned int retries;

	if (result >= 0) {
		for (retries = 1000; retries; --retries) {
			if (sceWlanBtGetConfiguration() & 8)
				return 0;
			sceKernelDelayThread(10000);
		}
		return 0x80418004;
	}
	return result;
}

int robinWlanRequestStandby(void)
{
	int result = sceWlanBtSetConfiguration(8);
	unsigned int retries;

	if (result >= 0) {
		for (retries = 1000; retries; --retries) {
			if (sceWlanBtGetConfiguration() & 0x20)
				return 0;
			sceKernelDelayThread(10000);
		}
		return 0x80418004;
	}
	return result;
}

/* The 4.00 debug records establish this helper's actual inlining. */
#if FW_DEBUG
static inline
#endif
int _commandClear(SceWlanBtRobinWlanDcm *dcm)
{
	SceWlanBtRobinWlanCommand *command, *next, *slot;
	SceWlanBtRobinWlanContext *context;

	lockFastMutex(&dcm->commandMutex);
	command = dcm->currentCommand;
	if (command) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &gDcmClearCurrentCommand,
			"[SceWlanBt]: Free CurCmd cc=%04x, cb=%p\n",
			command->commandId, command->callback);
		command = dcm->currentCommand;
#endif
		slot = &dcm->commands[0];
		if (command == slot || (slot = &dcm->commands[1], command == slot) ||
		    (slot = &dcm->commands[2], command == slot) ||
		    (slot = &dcm->commands[3], command == slot) ||
		    (slot = &dcm->commands[4], command == slot) ||
		    (slot = &dcm->commands[5], command == slot) ||
		    (slot = &dcm->commands[6], command == slot) ||
		    (slot = &dcm->commands[7], command == slot)) {
			context = dcm->context;
			if ((context->status & 4) && command->requestBuffer) {
				context->driver.free(&context->driver, command->requestBuffer);
				if (command->responsePacket)
					context->driver.releaseTx(&context->driver,
							  command->responsePacket);
			}
			memset(slot, 0, sizeof(*slot));
		}
		dcm->currentCommand = NULL;
	}
	command = dcm->pendingCommands;
	dcm->pendingCommands = NULL;
	while (command) {
		next = command->next;
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &gDcmClearPendingCommand,
			"[SceWlanBt]: Free PendingCmd cc=%04x, cb=%p\n",
			command->commandId, command->callback);
#endif
		slot = &dcm->commands[0];
		if (command == slot || (slot = &dcm->commands[1], command == slot) ||
		    (slot = &dcm->commands[2], command == slot) ||
		    (slot = &dcm->commands[3], command == slot) ||
		    (slot = &dcm->commands[4], command == slot) ||
		    (slot = &dcm->commands[5], command == slot) ||
		    (slot = &dcm->commands[6], command == slot) ||
		    (slot = &dcm->commands[7], command == slot)) {
			context = dcm->context;
			if ((context->status & 4) && command->requestBuffer) {
				context->driver.free(&context->driver, command->requestBuffer);
				if (command->responsePacket)
					context->driver.releaseTx(&context->driver,
							  command->responsePacket);
			}
			memset(slot, 0, sizeof(*slot));
		}
		command = next;
	}
	return sceKernelUnlockFastMutex(&dcm->commandMutex);
}

int robinWlanStartDcm(SceWlanBtRobinWlanDcm *dcm)
{
	uint8_t interruptMask[8], buffer[8];
	SdifSdioFunctionIdentifier identifier;
	unsigned int lowPort, middlePort;
	int result;

	dcm->sdioContext = SceSdifForDriver_6A8235FC(2);
	if (!dcm->sdioContext)
		return 0x80418007;
	identifier.manufacturerId = 0x2df;
	identifier.productId = 0x9119;
	identifier.interfaceCode = -1;
	dcm->sdioFunction = SceSdifForDriver_0C66E36F(dcm->sdioContext, &identifier);
	if (!dcm->sdioFunction)
		return 0x80418007;
	result = SceSdifForDriver_3428884D(dcm->sdioFunction);
	if (result < 0)
		return result;
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0x78, 1, buffer, 1);
	if (result < 0)
		return result;
	lowPort = buffer[0];
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0x79, 1, buffer, 1);
	if (result < 0)
		return result;
	middlePort = buffer[0];
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0x7a, 1, buffer, 1);
	if (result < 0)
		return result;
	dcm->ioPort = buffer[0] << 16 | middlePort << 8 | lowPort;
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 1, 1, buffer, 1);
	if (result < 0)
		return result;
	buffer[0] |= 0x3f;
	result = SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, 1, 1, buffer, 1);
	if (result < 0)
		return result;
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0x6c, 1, buffer, 1);
	if (result < 0)
		return result;
	buffer[0] |= 0x10;
	result = SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, 0x6c, 1, buffer, 1);
	if (result < 0)
		return result;
	interruptMask[0] = 0xff;
	result = SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, 2, 1, interruptMask, 1);
	if (result < 0)
		return result;
	dcm->subInterrupt = dcm->sdioFunction->number + 1;
	result = sceKernelRegisterSubIntrHandler(222, dcm->subInterrupt,
			"SceWlanBtRobinWlan", robinWlanSubInterrupt, dcm->context);
	if (result < 0)
		return result;
	result = sceKernelEnableSubIntr(222, dcm->subInterrupt);
	if (result < 0)
		return result;
	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0, sizeof(dcm->status),
					 (uint8_t *)&dcm->status, 0);
	if (result < 0)
		return result;
	dcm->nextReadPort = 1;
	dcm->nextWritePort = 1;
	dcm->readBitmap = dcm->status.readBitmap[0] | dcm->status.readBitmap[1] << 8;
	dcm->writeBitmap = dcm->status.writeBitmap[0] | dcm->status.writeBitmap[1] << 8;
	robinWlanReceiveMessages(dcm);
	_commandClear(dcm);
	return result;
}

int robinWlanShutdownDcm(SceWlanBtRobinWlanDcm *dcm, int keepFunction)
{
	uint8_t interruptMask = 0;
	int result;

	result = SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, 2, 1, &interruptMask, 1);
	if (result >= 0) {
		result = sceKernelDisableSubIntr(222, dcm->subInterrupt);
		if (result >= 0) {
			result = sceKernelReleaseSubIntrHandler(222, dcm->subInterrupt);
			if (result >= 0 && !keepFunction)
				result = SceSdifForDriver_E80293EF(dcm->sdioFunction);
		}
	}
	return result;
}

SceWlanBtRobinWlanCommand *robinWlanAllocateCommand(SceWlanBtRobinWlanContext *context,
						 unsigned int size)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	SceWlanBtRobinWlanContext *owner;
	SceWlanBtRobinWlanCommand *command;
	WlanFirmwareCommandHeader *header;
	unsigned int index;

	lockFastMutex(&dcm->commandMutex);
	if (!dcm->commands[0].requestBuffer)
		index = 0;
	else if (!dcm->commands[1].requestBuffer)
		index = 1;
	else if (!dcm->commands[2].requestBuffer)
		index = 2;
	else if (!dcm->commands[3].requestBuffer)
		index = 3;
	else if (!dcm->commands[4].requestBuffer)
		index = 4;
	else if (!dcm->commands[5].requestBuffer)
		index = 5;
	else if (!dcm->commands[6].requestBuffer)
		index = 6;
	else if (!dcm->commands[7].requestBuffer)
		index = 7;
	else {
		command = NULL;
		goto unlock;
	}
	owner = dcm->context;
	command = &dcm->commands[index];
	if (owner->status & 4) {
		command->requestBuffer = owner->driver.allocate(&owner->driver,
			(size + sizeof(WlanSdioMessageHeader) +
			 sizeof(WlanFirmwareCommandHeader) + 255) & ~255U, 64);
		if (!command->requestBuffer) {
			command = NULL;
			goto unlock;
		}
	} else {
		command->requestBuffer = owner->dcm.requestBuffer;
	}
	header = (void *)((uint8_t *)command->requestBuffer +
			 sizeof(WlanSdioMessageHeader));
	command->requestPayload = header + 1;
	if (size)
		memset(command->requestPayload, 0, size);
unlock:
	sceKernelUnlockFastMutex(&dcm->commandMutex);
	return command;
}

int robinWlanReleaseCommand(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	SceWlanBtRobinWlanCommand *slot = &dcm->commands[0];
	SceWlanBtRobinWlanContext *owner;

	lockFastMutex(&dcm->commandMutex);
	if (command == slot || (slot = &dcm->commands[1], command == slot) ||
	    (slot = &dcm->commands[2], command == slot) ||
	    (slot = &dcm->commands[3], command == slot) ||
	    (slot = &dcm->commands[4], command == slot) ||
	    (slot = &dcm->commands[5], command == slot) ||
	    (slot = &dcm->commands[6], command == slot) ||
	    (slot = &dcm->commands[7], command == slot)) {
		owner = dcm->context;
		if ((owner->status & 4) && command->requestBuffer) {
			owner->driver.free(&owner->driver, command->requestBuffer);
			if (command->responsePacket)
				owner->driver.releaseTx(&owner->driver, command->responsePacket);
		}
		memset(slot, 0, sizeof(*slot));
	}
	return sceKernelUnlockFastMutex(&dcm->commandMutex);
}

int robinWlanSubmitCommand(SceWlanBtRobinWlanContext *context,
			  SceWlanBtRobinWlanCommand *command, uint16_t commandId,
			  unsigned int size)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	WlanCommandMessage *message;
	SceWlanBtRobinWlanCommand *pending;
	uint16_t sequence;
	unsigned int eventIndex, nextEventIndex, eventMask;
	int result;

	lockFastMutex(&dcm->commandMutex);
	sequence = dcm->sequence;
	message = command->requestBuffer;
	command->commandId = commandId;
	message->command.commandSize = size + sizeof(message->command);
	message->command.commandId = commandId;
	dcm->sequence = sequence + 1;
	message->command.sequence.number = sequence;
	message->command.result = 0;
	message->command.sequence.bssNumber = 0;
	message->command.sequence.bssType = 0;
	wlanBtNotifyMonitor(0x20001, 1, size + sizeof(message->command),
			    &message->command);
	message->header.messageSize = (size + sizeof(*message) + 255) & ~255U;
	message->header.messageType = 1;
	if (!command->callback) {
		eventIndex = gCommandEventIndex;
		nextEventIndex = eventIndex + 1;
		eventMask = 1U << eventIndex;
		if (nextEventIndex == 16)
			nextEventIndex = 0;
		gCommandEventIndex = nextEventIndex;
		command->eventMask = eventMask;
	}
	if (dcm->currentCommand) {
		pending = dcm->pendingCommands;
		if (pending) {
			while (pending->next)
				pending = pending->next;
			pending->next = command;
		} else {
			dcm->pendingCommands = command;
		}
		result = 0;
	} else {
		dcm->currentCommand = command;
		result = robinWlanNotifyWorker(256);
	}
	sceKernelUnlockFastMutex(&dcm->commandMutex);
	return result;
}

int robinWlanWaitCommand(SceWlanBtRobinWlanContext *context,
			SceWlanBtRobinWlanCommand *command)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	SceWlanBtRobinWlanCommand *pending, *next;
	unsigned int events;
	unsigned int timeout = 10000000;
	int result;

	result = sceKernelWaitEventFlag(dcm->commandEventFlag,
			command->eventMask | 0x80000000U, 5, &events, &timeout);
	if (result >= 0) {
		if (!(events & 0x80000000U)) {
			if (command->result < 0)
				return command->result;
			return result;
		}
		result = 0x80418004;
	}
	lockFastMutex(&dcm->commandMutex);
	if (command == dcm->currentCommand) {
		dcm->currentCommand = NULL;
	} else {
		pending = dcm->pendingCommands;
		if (command == pending) {
			dcm->pendingCommands = command->next;
		} else if (pending) {
			next = pending->next;
			while (next) {
				if (command == next) {
					pending->next = next->next;
					break;
				}
				pending = next;
				next = next->next;
			}
		}
	}
	sceKernelUnlockFastMutex(&dcm->commandMutex);
	return result;
}

/* Sony inlines submission and the synchronous wait in this entry point. */
int robinWlanSubmitAndWaitCommand(SceWlanBtRobinWlanContext *context,
				 SceWlanBtRobinWlanCommand *command, uint16_t commandId,
				 unsigned int size)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	WlanCommandMessage *message;
	SceWlanBtRobinWlanCommand *pending, *next;
	uint16_t sequence;
	unsigned int eventIndex, nextEventIndex, eventMask;
	unsigned int events, timeout;
	int result;

	lockFastMutex(&dcm->commandMutex);
	sequence = dcm->sequence;
	message = command->requestBuffer;
	command->commandId = commandId;
	message->command.commandSize = size + sizeof(message->command);
	message->command.commandId = commandId;
	dcm->sequence = sequence + 1;
	message->command.sequence.number = sequence;
	message->command.result = 0;
	message->command.sequence.bssNumber = 0;
	message->command.sequence.bssType = 0;
	wlanBtNotifyMonitor(0x20001, 1, size + sizeof(message->command),
			    &message->command);
	message->header.messageSize = (size + sizeof(*message) + 255) & ~255U;
	message->header.messageType = 1;
	if (!command->callback) {
		eventIndex = gCommandEventIndex;
		nextEventIndex = eventIndex + 1;
		eventMask = 1U << eventIndex;
		if (nextEventIndex == 16)
			nextEventIndex = 0;
		gCommandEventIndex = nextEventIndex;
		command->eventMask = eventMask;
	}
	if (dcm->currentCommand) {
		pending = dcm->pendingCommands;
		if (pending) {
			while (pending->next)
				pending = pending->next;
			pending->next = command;
		} else {
			dcm->pendingCommands = command;
		}
		sceKernelUnlockFastMutex(&dcm->commandMutex);
	} else {
		dcm->currentCommand = command;
		result = robinWlanNotifyWorker(256);
		sceKernelUnlockFastMutex(&dcm->commandMutex);
		if (result < 0)
			return result;
	}
	timeout = 10000000;
	result = sceKernelWaitEventFlag(dcm->commandEventFlag,
			command->eventMask | 0x80000000U, 5, &events, &timeout);
	if (result >= 0) {
		if (!(events & 0x80000000U)) {
			if (command->result < 0)
				return command->result;
			return result;
		}
		result = 0x80418004;
	}
	lockFastMutex(&dcm->commandMutex);
	if (command == dcm->currentCommand) {
		dcm->currentCommand = NULL;
	} else {
		pending = dcm->pendingCommands;
		if (command == pending) {
			dcm->pendingCommands = command->next;
		} else if (pending) {
			next = pending->next;
			while (next) {
				if (command == next) {
					pending->next = next->next;
					break;
				}
				pending = next;
				next = next->next;
			}
		}
	}
	sceKernelUnlockFastMutex(&dcm->commandMutex);
	return result;
}

int robinWlanTransmitCommand(SceWlanBtRobinWlanContext *context)
{
	SceWlanBtRobinWlanDcm *dcm = &context->dcm;
	SceWlanBtRobinWlanCommand *command = dcm->currentCommand;
	WlanCommandMessage *message;
	SdifSdioFunction *function;
	unsigned int port, size;
	uint64_t timestamp;

	if (!command)
		return 0;
	message = command->requestBuffer;
	if (command->flags & 1) {
		__atomic_fetch_or(&context->status, 0x80000, __ATOMIC_RELEASE);
		command = dcm->currentCommand;
	} else if (context->status & 0x80010) {
		return 0x80418004;
	}
	timestamp = sceKernelGetSystemTimeWide();
	function = dcm->sdioFunction;
	port = dcm->ioPort;
	size = message->header.messageSize;
	command->timestamp = timestamp;
	return SceSdifForDriver_3C4CDC8B(function, port, size, (uint8_t *)message, 2);
}

int robinWlanRequestTransmit(SceWlanBtRobinWlanDcm *dcm)
{
	return robinWlanTransmitPackets(dcm);
}

int robinWlanDcmInterruptRequest(SceWlanBtRobinWlanDcm *dcm)
{
	unsigned int interrupts, readPending, writePending;
	unsigned int bitmap;
	int result;

	result = SceSdifForDriver_D0F78D9B(dcm->sdioFunction, 0, 40,
					 dcm->aggregateBuffer, 2);
	if (result < 0)
		return result;
	dcm->status = *(const SceWlanBtRobinSdioStatus *)dcm->aggregateBuffer;
	interrupts = dcm->status.interruptStatus;
	readPending = interrupts & 1;
	writePending = interrupts & 2;
	bitmap = dcm->writeBitmap;
	if (writePending)
		bitmap = dcm->status.writeBitmap[0] | dcm->status.writeBitmap[1] << 8;
	dcm->writeBitmap = bitmap | (dcm->status.writeBitmap[0] & 1);
	if (readPending)
		dcm->readBitmap = dcm->status.readBitmap[0] | dcm->status.readBitmap[1] << 8;
	result = sceKernelEnableSubIntr(222, dcm->subInterrupt);
#if FW_DEBUG
	if (result < 0) {
		SceDebugForDriver_D9703808(1, 0, &gDcmInterruptError,
			"%s : sceKernelEnableSubIntr err:0x%08x\n",
			"robinWlanDcmInterruptRequest", result);
		SceDebugForDriver_35A35322(0, &gDcmInterruptAssert, __builtin_return_address(0));
	}
#endif
	if (readPending)
		robinWlanReceiveMessages(dcm);
	if (writePending && dcm->waitingForWritePort) {
		dcm->waitingForWritePort = 0;
		return robinWlanTransmitPackets(dcm);
	}
	return result;
}

int robinWlanRequestCardWake(SceWlanBtRobinWlanDcm *dcm)
{
	uint8_t configuration = 2;

	return SceSdifForDriver_3C4CDC8B(dcm->sdioFunction, 0, 1, &configuration, 1);
}
