/*
 * WLAN device callbacks and frame adaptation. The filename is inferred from
 * the original driver's callback family; no source filename survives.
 */
#include "robin/wlan.h"
#include "robin/rom.h"
#include "robin/sys.h"

void *ampPalDevice __attribute__((section(".itcm.rw"))) = NULL;
void *wlanSdioDev __attribute__((section(".itcm.rw"))) = NULL;
void *wlanDevice __attribute__((section(".itcm.rw"))) = NULL;
void *wlanSdioCallbackDevice __attribute__((section(".itcm.rw"))) = NULL;
void (*wlanReceiveErrorHandler)(void *context) __attribute__((section(".itcm.rw"))) = NULL;
struct OsaBufferPool *wlanPacketPool __attribute__((section(".itcm.rw"))) = NULL;

/* The pool is supplied by the allocator interface; no private fields read here. */
extern struct OsaBufferPool ampTransmitPool;
extern struct OsaBufferPool wlanPacketBufferPool;

int wlanHandleSecurityFrame(OsaBuffer *buffer)
{
	WlanPacketHeader *header;
	struct ether_header *ethernet;
	WlanEthernetSnapFrame *snapFrame;
	const uint8_t *protocolBytes;
	unsigned int protocol;
	int packetOffset;
	uint16_t dataOffset;

	if (!wlanBufferNeedsSecurity(buffer) && !wlanSecurityOverrideEnabled())
		return -1;
	header = (WlanPacketHeader *)((uint8_t *)buffer + buffer->dataOffset);
	packetOffset = (int16_t)(header->wire.packetOffset[0] |
		((unsigned int)header->wire.packetOffset[1] << 8));
	ethernet = (struct ether_header *)((uint8_t *)header + packetOffset);
	dataOffset = (uint8_t)(buffer->dataOffset + packetOffset + sizeof(*ethernet));
	if (wlanBufferUsesEthernetFrames(buffer)) {
		protocolBytes = (const uint8_t *)&ethernet->ether_type;
	} else {
		snapFrame = (WlanEthernetSnapFrame *)ethernet;
		protocolBytes = snapFrame->snap.protocol.wire;
		dataOffset = (uint8_t)(dataOffset + sizeof(snapFrame->snap));
	}
	protocol = protocolBytes[0] | ((unsigned int)protocolBytes[1] << 8);
	if (!wlanIsSecurityProtocol(protocol))
		return -1;
	buffer->dataOffset = dataOffset;
	if (!wlanDispatchSecurityFrame(buffer, ethernet->ether_shost,
				      ethernet->ether_dhost))
		return -1;
	wlanReleasePacket(buffer);
	return 0;
}

/* Optional initialization hook called after the SDIO callbacks are installed. */
int wlanInitializationHook(void)
{
	return 0;
}

int wlanForwardSdioEvent(void *device, OsaBuffer *buffer,
			 uintptr_t argument, unsigned int event)
{
	if (event == 4 || event == 1) {
		buffer->context = wlanSdioDev;
		mdevForwardEvent(wlanDevice, buffer, argument, event);
	}
	return 0;
}

int wlanReceiveDeviceEvent(void *device, OsaBuffer *buffer,
			   uintptr_t argument, unsigned int event)
{
	WlanPacketHeader *header;
	unsigned int length;

	if (event == 5) {
		header = (WlanPacketHeader *)((uint8_t *)buffer + buffer->dataOffset);
		length = header->wire.packetLength[0] |
			 ((unsigned int)header->wire.packetLength[1] << 8);
		if (length == 0) {
			wlanReleasePacket(buffer);
			return 0;
		}
		if (wlanHandleSecurityFrame(buffer) != 0 && !wlanHandleAmpFrame(buffer))
			mdevForwardEvent(wlanSdioDev, buffer, argument, 5);
	} else if (event == 3) {
		mdevForwardEvent(wlanGetBufferDevice(buffer), buffer, argument, 3);
	} else if (event == 2) {
		mdevForwardEvent(wlanSdioCallbackDevice, buffer, argument, event);
	}
	return 0;
}

int wlanConnectSdioDevice(void)
{
	if (wlanSdioDev != NULL)
		return (intptr_t)wlanSdioDev;
	wlanSdioDev = wlanOpenSdioDevice(0);
	wlanSdioCallbackDevice = wlanSdioDev;
	mdevIoctl(wlanSdioDev, 0x20, (uintptr_t)wlanForwardSdioEvent);
	return mdevIoctl(wlanDevice, 0x20, (uintptr_t)wlanReceiveDeviceEvent);
}

int wlanDisconnectSdioDevice(void)
{
	if (wlanSdioDev != NULL) {
		mdevIoctl(wlanSdioDev, 0x13, 0);
		mdevReleaseDevice(wlanSdioDev);
		wlanSdioDev = NULL;
	}
	return 0;
}

int wlanInitializeDevice(void)
{
	wlanDevice = mdevOpenDevice("wlan", 0);
	wlanConnectSdioDevice();
	wlanInitializationHook();
	if (wlanSdioDev != NULL)
		mdevIoctl(wlanSdioDev, 0x0d, 0);
	return 0;
}

uint32_t wlanCompletePacket(OsaBuffer *buffer, uint32_t completion)
{
	uint32_t posture = sysIrqDisable();

	buffer->packetClass = 1;
	if (buffer->pool == &ampTransmitPool && (uint16_t)completion != 0) {
		buffer->ringEnd = completion >> 16;
		mdevIoctl(ampPalDevice, 0x53, (uintptr_t)buffer);
	}
	osa_BufferFree(buffer);
	return sysIrqRestore(posture);
}

int wlanHandleAmpFrame(OsaBuffer *buffer)
{
	WlanPacketHeader *header;
	WlanEthernetSnapFrame *frame;
	int packetOffset;
	unsigned int event;

	if (!wlanBufferUsesAmp(buffer))
		return 0;
	header = (WlanPacketHeader *)((uint8_t *)buffer + buffer->dataOffset);
	packetOffset = (int16_t)(header->wire.packetOffset[0] |
		((unsigned int)header->wire.packetOffset[1] << 8));
	frame = (WlanEthernetSnapFrame *)((uint8_t *)header + packetOffset);
	switch (frame->snap.protocol.native) {
	case 0x0100:
		event = 5;
		break;
	case 0x0400:
		mdevIoctl(ampPalDevice, 0x50, (uintptr_t)buffer);
		wlanReleasePacket(buffer);
		return 1;
	case 0x0500:
		mdevIoctl(ampPalDevice, 0x51, (uintptr_t)buffer);
		wlanReleasePacket(buffer);
		return 1;
	case 0x0200:
		event = 0x15;
		break;
	default:
		return 1;
	}
	buffer->dataOffset = (uint8_t *)(frame + 1) - (uint8_t *)buffer;
	buffer->length -= sizeof(*frame);
	/* The original subtracts the frame size again for the event argument. */
	mdevForwardEvent(ampPalDevice, buffer, buffer->length - sizeof(*frame), event);
	return 1;
}

int wlanConvertDataFrame(OsaBuffer *buffer, WlanPacketHeader *header,
			uintptr_t argument, const void *wirelessHeader)
{
	return wlanDecodeDataFrame(buffer, header, wirelessHeader);
}

int wlanPrepareAmsduFrame(OsaBuffer *buffer, WlanPacketHeader *header)
{
	int packetOffset;

	if (buffer->length <= 96)
		return 0;
	buffer->length -= 96;
	packetOffset = (int16_t)(header->wire.packetOffset[0] |
		((unsigned int)header->wire.packetOffset[1] << 8));
	packetOffset += 36;
	header->wire.packetOffset[0] = packetOffset;
	header->wire.packetOffset[1] = (uint16_t)packetOffset >> 8;
	header->wire.packetType[0] = 0xe6;
	header->wire.packetType[1] = 0;
	return 1;
}

/* The original default callback is exactly one BX LR. */
void wlanDefaultReceiveError(void *context)
{
}

struct OsaBufferPool *wlanInitializeReceiveErrorHandler(void)
{
	wlanReceiveErrorHandler = wlanDefaultReceiveError;
	wlanPacketPool = &wlanPacketBufferPool;
	return &wlanPacketBufferPool;
}

void wlanForwardEventPacket(OsaBuffer *buffer)
{
	buffer->packetClass = 2;
	mdevForwardEvent(wlanDevice, buffer, sizeof(*buffer), 4);
}

int wlanBufferUsesEthernetFrames(OsaBuffer *buffer)
{
	return wlanContextUsesEthernetFrames(buffer->context);
}

void *wlanOpenSdioDevice(unsigned int flags)
{
	return mdevOpenDevice("sdio", flags);
}

int wlanBufferNeedsSecurity(OsaBuffer *buffer)
{
	return wlanInterfaceNeedsSecurity(buffer->context);
}

void wlanSetEventPacketLength(OsaBuffer *buffer, uint32_t length)
{
	WlanPacketHeader *header;

	header = (WlanPacketHeader *)((uint8_t *)buffer + buffer->dataOffset);
	length += 18;
	header->wire.packetLength[0] = length;
	header->wire.packetLength[1] = length >> 8;
	wlanForwardEventPacket(buffer);
}
