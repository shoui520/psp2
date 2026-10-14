/*
 * HCI command layer of the AMP controller (Bluetooth 3.0+HS, 802.11 PAL):
 * the event generator, the tables of physical and logical links, the
 * AMP_ASSOC builder and parser, and one handler per HCI command.
 *
 * Opcode and parameter names are the Bluetooth Core Specification's. The
 * filename is inferred; no source filename survives in the image.
 */
#include <stdarg.h>
#include <stddef.h>
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/wlan.h"

/* Link controller, WLAN and coexistence code the AMP layer calls (not converted). */
void *btLmpLcFormatContext(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

/* Offsets into the OSA buffers and messages used throughout. */
#define HCI_COMMAND_OPCODE(data)	((data)[0] | ((data)[1] << 8))

#define AMP_POOL_BUFFER_SIZE	200

/* State of the command layer. */
int8_t ampHciCommandCount __attribute__((section(".itcm.bss")));
static uint8_t ampHciIdleFlag __attribute__((section(".itcm.bss")));
uint8_t ampFlowControlMode __attribute__((section(".itcm.bss")));
uint8_t ampBufferCount __attribute__((section(".itcm.bss")));
static uint16_t ampBufferCountCopy __attribute__((section(".itcm.bss")));
static uint16_t ampAclPacketLength __attribute__((section(".itcm.bss")));
static uint16_t ampDataBlockLength __attribute__((section(".itcm.bss")));
static uint8_t ampDutMode __attribute__((section(".itcm.bss")));
static uint8_t ampDutReports __attribute__((section(".itcm.bss")));
static uint8_t ampEventMaskPage1[8] __attribute__((section(".itcm.bss")));
static uint8_t ampEventMaskPage2[8] __attribute__((section(".itcm.bss")));

/* Values reported by Read_Local_AMP_Info. */
static uint8_t ampStatus __attribute__((section(".itcm.bss")));
static uint8_t ampControllerType __attribute__((section(".itcm.bss")));
static uint16_t ampPalCapabilities __attribute__((section(".itcm.bss")));
static uint16_t ampMaxAssocLength __attribute__((section(".itcm.bss")));
static uint32_t ampTotalBandwidth __attribute__((section(".itcm.bss")));
static uint32_t ampMaxGuaranteedBandwidth __attribute__((section(".itcm.bss")));
static uint32_t ampMinLatency __attribute__((section(".itcm.bss")));
static uint32_t ampMaxPduSize __attribute__((section(".itcm.bss")));
static uint32_t ampMaxFlushTimeout __attribute__((section(".itcm.bss")));
static uint32_t ampBestEffortFlushTimeout __attribute__((section(".itcm.bss")));

/* Timeouts and the location data. */
static uint16_t ampConnectionAcceptTimeout __attribute__((section(".itcm.bss")));
static uint16_t ampLinkSupervisionTimeout __attribute__((section(".itcm.bss")));
static uint16_t ampLogicalLinkAcceptTimeout __attribute__((section(".itcm.bss")));
uint8_t ampDisconnectReason __attribute__((section(".itcm.bss")));
static uint16_t ampHciTimerValue __attribute__((section(".itcm.bss")));
static uint8_t ampLocationDomainAware __attribute__((section(".itcm.bss")));
static uint8_t ampLocationDomain[2] __attribute__((section(".itcm.bss")));
static uint8_t ampLocationDomainOptions __attribute__((section(".itcm.bss")));
static uint8_t ampLocationOptions __attribute__((section(".itcm.bss")));

/* AMP_ASSOC of the local controller, and the scratch buffer commands share. */
struct AmpLocalAssoc ampLocalAssoc __attribute__((section(".itcm.bss")));
/* Scratch buffer shared by Read_Local_Supported_Commands and Read_Local_AMP_ASSOC. */
static uint8_t ampSupportedCommands[64] __attribute__((section(".itcm.bss")));
struct AmpPhysicalLink ampPhysicalLinks[AMP_PHYSICAL_LINKS] __attribute__((section(".itcm.bss")));
struct AmpLinkMachine ampLinkMachines[AMP_PHYSICAL_LINKS] __attribute__((section(".itcm.bss")));

/* The remote AMP_ASSOC is received in fragments into this buffer. */
static OsaBuffer *ampRemoteAssocBuffer __attribute__((section(".itcm.rw")));

extern struct AmpPalState ampPalState;

/* 0x00020eec: tracks how many commands are being processed (0..2). */
int ampHciAdjustCommandCount(int8_t delta)
{
	int result = (int8_t)(ampHciCommandCount + delta);

	ampHciCommandCount = result;
	if (result < 0) {
		ampHciCommandCount = 0;
		return 0;
	}
	return result;
}

/* 0x00020f00 */
int ampHciIsIdle(void)
{
	return ampHciCommandCount == 0;
}

/* 0x00020f10: bit test in an event mask; the selector is (byte << 8) | bit mask. */
int ampHciEventEnabled(const uint8_t *mask, uint32_t selector)
{
	return (mask[selector >> 8] & selector) != 0;
}

/* 0x00020f1c */
OsaBuffer *ampHciAllocBuffer(struct OsaBufferPool *pool, uint32_t size, uint32_t flags)
{
	OsaBuffer *buffer = osa_BufferAlloc(pool, size, flags);

	if (buffer != NULL)
		buffer->dataOffset = 32;
	return buffer;
}

/*
 * 0x00020f2c: builds an HCI event from a format and sends it to the host. The
 * format letters: b byte, h 16-bit, w 32-bit, m memory (pointer, length; a
 * null pointer sends zeros), M skip, R the Num_HCI_Command_Packets field,
 * V mark the event as vendor specific. A selector other than zero names the
 * bit of event mask page 2 that has to be set for the event to go out.
 */
int ampHciSendEvent(uint8_t eventCode, uint32_t maskSelector, const char *format, ...)
{
	va_list args;
	OsaBuffer *buffer;
	uint8_t *data;
	uint8_t *start;
	uint8_t *out;
	int vendor = 0;
	uint32_t value;
	const void *source;
	int length;

	if (maskSelector != 0 &&
	    !ampHciEventEnabled(ampEventMaskPage2, (uint16_t)maskSelector))
		return 0;
	buffer = ampHciAllocBuffer(&ampHciPool, AMP_POOL_BUFFER_SIZE, 0);
	if (buffer == NULL)
		return 0;
	va_start(args, format);
	data = (uint8_t *)buffer + buffer->dataOffset;
	data[0] = eventCode;
	start = data + 2;
	out = start;
	for (; *format != '\0'; format++) {
		switch (*format) {
		case 'b':
			*out++ = va_arg(args, int);
			break;
		case 'h':
			value = va_arg(args, unsigned int);
			*out++ = value;
			*out++ = value >> 8;
			break;
		case 'w':
			value = va_arg(args, unsigned int);
			*out++ = value;
			*out++ = value >> 8;
			*out++ = value >> 16;
			*out++ = value >> 24;
			break;
		case 'm':
			source = va_arg(args, const void *);
			length = va_arg(args, int);
			if (out + length - start > AMP_POOL_BUFFER_SIZE)
				length = AMP_POOL_BUFFER_SIZE - (out - start);
			if (length != 0) {
				if (source != NULL)
					__aeabi_memcpy(out, source, (int16_t)length);
				else
					__aeabi_memclr(out, (int16_t)length);
			}
			out += length;
			break;
		case 'M':
			out += va_arg(args, int);
			break;
		case 'R':
			value = 2 - ampHciCommandCount;
			if ((int)value > 1)
				value = 1;
			*out++ = value;
			break;
		case 'V':
			vendor = 1;
			break;
		}
	}
	va_end(args);
	data[1] = (uint8_t)(out - start);
	buffer->length = data[1] + 2;
	btLmpLcAcquireContext(1);
	mdevForwardEvent(ampSdioDev, buffer, buffer->length, vendor ? 3 : 2);
	return 1;
}

/*
 * 0x0002105e: the same formats, for the messages to the host driver of the PAL
 * ("btamp_pal"): a 16-bit identifier, a 16-bit length, the parameters.
 */
int ampHciSendHostEvent(uint16_t id, int unused, const char *format, ...)
{
	va_list args;
	OsaBuffer *buffer;
	uint16_t *header;
	uint8_t *start;
	uint8_t *out;
	uint32_t value;
	const void *source;
	int length;

	if (unused != 0)
		return 0;
	buffer = ampHciAllocBuffer(&ampHciPool, AMP_POOL_BUFFER_SIZE, 0xffffffff);
	va_start(args, format);
	header = (uint16_t *)((uint8_t *)buffer + buffer->dataOffset);
	header[0] = id;
	start = (uint8_t *)buffer + 4 + buffer->dataOffset;
	out = start;
	for (; *format != '\0'; format++) {
		switch (*format) {
		case 'b':
			*out++ = va_arg(args, int);
			break;
		case 'h':
			value = va_arg(args, unsigned int);
			*out++ = value;
			*out++ = value >> 8;
			break;
		case 'w':
			value = va_arg(args, unsigned int);
			*out++ = value;
			*out++ = value >> 8;
			*out++ = value >> 16;
			*out++ = value >> 24;
			break;
		case 'm':
			source = va_arg(args, const void *);
			length = va_arg(args, int);
			if (out + length - start > AMP_POOL_BUFFER_SIZE)
				length = AMP_POOL_BUFFER_SIZE - (out - start);
			if (length != 0) {
				if (source != NULL)
					__aeabi_memcpy(out, source, (int16_t)length);
				else
					__aeabi_memclr(out, (int16_t)length);
			}
			out += length;
			break;
		case 'M':
			out += va_arg(args, int);
			break;
		case 'R':
			value = 2 - ampHciCommandCount;
			if ((int)value > 1)
				value = 1;
			*out++ = value;
			break;
		}
	}
	va_end(args);
	header[1] = (uint16_t)(out - start);
	buffer->length = (uint16_t)(header[1] + 4);
	mdevForwardEvent(ampPalDevice, buffer, buffer->length, 1);
	return 1;
}

/* 0x0002115c: Command Complete with status 1 (Unknown HCI Command). */
int ampHciRejectUnknownCommand(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhb", opcode, AMP_HCI_UNKNOWN_COMMAND);
	return 0;
}

/* 0x0002117a: Command Complete with a status; finished == 1 also ends the command. */
int ampHciDisallowCommand(uint16_t opcode, int status, int finished)
{
	if (finished == 1)
		ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhb", opcode, status);
	return 0;
}

/* 0x0002119a: Command Status event. */
int ampHciSendCommandStatus(uint16_t opcode, int status)
{
	ampHciSendEvent(AMP_EVENT_COMMAND_STATUS, 0, "bRh", status, opcode);
	return 0;
}

/* 0x000211ae: Command Status for opcode 0, which only refreshes Num_HCI_Command_Packets. */
int ampHciSendNopCommandStatus(void)
{
	ampHciSendEvent(AMP_EVENT_COMMAND_STATUS, 0, "bRh", 0, 0);
	return 0;
}

/* Command Complete with a status and an optional pair of values; used by many handlers. */
static void ampHciComplete(uint16_t opcode, int status)
{
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhb", opcode, status);
}

/* ---- Physical and logical link tables ---------------------------------------- */

/* 0x000211c4: claims a free physical link slot for a handle; 2 if none or if used. */
unsigned int ampPhysicalLinkAllocate(uint32_t handle)
{
	unsigned int i;

	for (i = 0; i < AMP_PHYSICAL_LINKS; i++) {
		if (ampPhysicalLinks[i].handle == handle)
			return AMP_PHYSICAL_LINKS;
	}
	for (i = 0; ampPhysicalLinks[i].handle != AMP_LINK_FREE; ) {
		i = (uint8_t)(i + 1);
		if (i >= AMP_PHYSICAL_LINKS)
			return AMP_PHYSICAL_LINKS;
	}
	ampPhysicalLinks[i].handle = handle;
	return i;
}

/* 0x0002120a */
unsigned int ampPhysicalLinkFind(uint32_t handle)
{
	unsigned int i;

	for (i = 0; ampPhysicalLinks[i].handle != handle; ) {
		i = (uint8_t)(i + 1);
		if (i >= AMP_PHYSICAL_LINKS)
			return AMP_PHYSICAL_LINKS;
	}
	return i;
}

/* 0x0002122c: the physical link that owns a logical link handle. */
unsigned int ampPhysicalLinkFindByLogical(uint32_t logicalHandle)
{
	unsigned int link;
	unsigned int slot;

	for (link = 0; link < AMP_PHYSICAL_LINKS; link++) {
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++) {
			if (ampPhysicalLinks[link].logicalLinks[slot].handle == logicalHandle)
				return link;
		}
	}
	return AMP_PHYSICAL_LINKS;
}

/* 0x00021262: clears a physical link entry. */
int ampPhysicalLinkReset(unsigned int index)
{
	struct AmpPhysicalLink *link;
	unsigned int slot;

	if (index < AMP_PHYSICAL_LINKS) {
		link = &ampPhysicalLinks[index];
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++)
			link->logicalLinks[slot].handle = AMP_LOGICAL_FREE;
		link->handle = AMP_LINK_FREE;
		__aeabi_memclr4(link->peerAddress, 6);
	}
	return 0;
}

/* 0x00021298: claims a logical link slot on the physical link with this handle. */
unsigned int ampLogicalLinkAllocate(uint32_t physicalHandle)
{
	struct AmpPhysicalLink *link = &ampPhysicalLinks[ampPhysicalLinkFind(physicalHandle)];
	unsigned int slot = 0;

	do {
		if (link->logicalLinks[slot].handle == AMP_LOGICAL_FREE) {
			link->logicalLinks[slot].handle = 16 * (int16_t)physicalHandle + slot + 1;
			return slot;
		}
		slot = (uint8_t)(slot + 1);
	} while (slot < AMP_LOGICAL_LINKS);
	return AMP_LOGICAL_LINKS;
}

/* 0x000212fe: the slot of a logical link handle inside its physical link. */
unsigned int ampLogicalLinkFind(uint32_t logicalHandle)
{
	unsigned int link;
	unsigned int slot;

	if (logicalHandle == AMP_LOGICAL_FREE)
		return AMP_LOGICAL_LINKS;
	for (link = 0; link < AMP_PHYSICAL_LINKS; link++) {
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++) {
			if (ampPhysicalLinks[link].logicalLinks[slot].handle == logicalHandle)
				return slot;
		}
	}
	return AMP_LOGICAL_LINKS;
}

/* 0x0002133a: the slot whose Tx flow spec identifier matches. */
unsigned int ampLogicalLinkFindByFlowSpec(uint32_t flowSpecId)
{
	unsigned int link;
	unsigned int slot;

	for (link = 0; link < AMP_PHYSICAL_LINKS; link++) {
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++) {
			if (ampPhysicalLinks[link].logicalLinks[slot].txFlowSpec.id == flowSpecId)
				return slot;
		}
	}
	return AMP_LOGICAL_LINKS;
}

/* 0x00021372: Tx flow spec service type of a logical link (3 when unknown). */
int ampLogicalLinkServiceType(uint32_t logicalHandle)
{
	int result = 3;
	unsigned int link;
	unsigned int slot;

	if (logicalHandle < AMP_LOGICAL_FREE) {
		for (link = 0; link < AMP_PHYSICAL_LINKS; link++) {
			for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++) {
				struct AmpLogicalLink *logical =
					&ampPhysicalLinks[link].logicalLinks[slot];

				if (logical->handle == logicalHandle)
					result = logical->txFlowSpec.serviceType;
			}
		}
	}
	return result;
}

/* ---- AMP_ASSOC ---------------------------------------------------------------- */

/* AMP_ASSOC type identifiers. */
enum {
	AMP_ASSOC_MAC_ADDRESS = 1,
	AMP_ASSOC_PREFERRED_CHANNEL_LIST = 2,
	AMP_ASSOC_CONNECTED_CHANNEL = 3,
	AMP_ASSOC_PAL_CAPABILITIES = 4,
	AMP_ASSOC_PAL_VERSION = 5,
	AMP_ASSOC_VENDOR = 0xfe,
};

/* PAL capabilities advertised in the local AMP_ASSOC (type 4). */
static uint16_t ampLocalPalCapabilitiesLength __attribute__((section(".itcm.bss")));
static uint8_t ampLocalPalCapabilities[4] __attribute__((section(".itcm.bss")));

/* 0x0002308e: resets the connected-channel part of the local AMP_ASSOC. */
int ampAssocResetConnectedChannels(struct AmpLocalAssoc *assoc)
{
	assoc->connectedChannelsLength = 0;
	assoc->connectedChannels[0] = 'U';
	assoc->connectedChannels[1] = 'S';
	assoc->connectedChannels[2] = ' ';
	assoc->connectedChannels[3] = 201;
	assoc->connectedChannels[4] = 12;
	assoc->connectedChannels[5] = 0;
	assoc->connectedChannels[6] = 11;
	assoc->connectedChannels[7] = 1;
	assoc->connectedChannels[8] = 20;
	return 1;
}

/*
 * 0x000213bc: writes the local AMP_ASSOC as type/length/value elements (1 byte
 * type, 2 bytes length) and returns its size.
 */
unsigned int ampAssocBuild(uint8_t *out, unsigned int physicalIndex)
{
	uint8_t channel;
	unsigned int position;
	unsigned int next;

	ampAssocResetConnectedChannels(&ampLocalAssoc);
	out[0] = AMP_ASSOC_MAC_ADDRESS;
	out[2] = 0;
	out[1] = 6;
	__aeabi_memcpy(out + 3, ampLocalAssoc.macAddress, 6);
	out[9] = AMP_ASSOC_PREFERRED_CHANNEL_LIST;
	out[10] = ampLocalAssoc.preferredChannelsLength;
	out[11] = ampLocalAssoc.preferredChannelsLength >> 8;
	if (physicalIndex < AMP_PHYSICAL_LINKS) {
		channel = ampPhysicalLinks[physicalIndex].channel;
		ampLocalAssoc.preferredChannels[6] = channel;
		ampLocalAssoc.preferredChannels[7] = 1;
	} else if (ampLocalAssoc.connectedChannelsLength != 0) {
		channel = ampLocalAssoc.connectedChannels[6];
		ampLocalAssoc.preferredChannels[6] = channel;
		ampLocalAssoc.preferredChannels[7] = 1;
	} else {
		ampLocalAssoc.preferredChannels[6] = 1;
		ampLocalAssoc.preferredChannels[7] = 11;
	}
	__aeabi_memcpy(out + 12, ampLocalAssoc.preferredChannels,
		       ampLocalAssoc.preferredChannelsLength);
	position = (uint16_t)(ampLocalAssoc.preferredChannelsLength + 12);
	if (ampLocalAssoc.connectedChannelsLength != 0) {
		out[position] = AMP_ASSOC_CONNECTED_CHANNEL;
		next = (uint16_t)(position + 1);
		out[next] = ampLocalAssoc.connectedChannelsLength;
		out[next + 1] = ampLocalAssoc.connectedChannelsLength >> 8;
		next = (uint16_t)(next + 2);
		__aeabi_memcpy(out + next, ampLocalAssoc.connectedChannels,
			       ampLocalAssoc.connectedChannelsLength);
		position = (uint16_t)(ampLocalAssoc.connectedChannelsLength + next);
	}
	out[position] = AMP_ASSOC_PAL_CAPABILITIES;
	next = (uint16_t)(position + 1);
	out[next] = ampLocalPalCapabilitiesLength;
	out[next + 1] = ampLocalPalCapabilitiesLength >> 8;
	next = (uint16_t)(next + 2);
	__aeabi_memcpy(out + next, ampLocalPalCapabilities, ampLocalPalCapabilitiesLength);
	position = (uint16_t)(ampLocalPalCapabilitiesLength + next);
	out[position] = AMP_ASSOC_PAL_VERSION;
	next = (uint16_t)(position + 1);
	out[next + 1] = 0;
	out[next] = 5;
	position = (uint16_t)(next + 2);
	out[position] = 1;			/* PAL version */
	next = (uint16_t)(position + 1);
	out[next + 1] = 0;
	out[next] = 72;				/* company identifier */
	position = (uint16_t)(next + 2);
	out[position] = 1;			/* PAL sub-version */
	out[position + 1] = 0;
	return (uint16_t)(position + 2);
}

/*
 * 0x000214de: stores the elements of a received AMP_ASSOC in a physical link.
 * Returns an HCI status: 0, or 0x12 when an element is longer than its slot.
 */
int ampAssocParse(const uint8_t *data, unsigned int total, int physicalIndex)
{
	struct AmpPhysicalLink *link = &ampPhysicalLinks[physicalIndex];
	unsigned int used = 0;
	int status = 0;
	unsigned int type;
	unsigned int length;
	const uint8_t *value;

	while (used < total && status == 0) {
		type = data[0];
		value = data + 3;
		length = data[1] | (data[2] << 8);
		switch (type) {
		case AMP_ASSOC_PAL_CAPABILITIES:
			if (length > 4) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->palCapabilities, value, length);
			link->palCapabilitiesLength = length;
			break;
		case AMP_ASSOC_PAL_VERSION:
			if (length > 5) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->palVersion, value, length);
			link->palVersionLength = length;
			break;
		case AMP_ASSOC_VENDOR:
			if (length > 0x20) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->vendor, value, length);
			link->vendorLength = length;
			break;
		case AMP_ASSOC_MAC_ADDRESS:
			if (length > 6) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->peerAddress, value, length);
			break;
		case AMP_ASSOC_PREFERRED_CHANNEL_LIST:
			if (length > 0x40) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->preferredChannels, value, length);
			link->preferredChannelsLength = length;
			if (length == 6) {
				link->preferredChannels[6] = 11;
				link->preferredChannels[7] = 1;
				link->preferredChannels[8] = 20;
			}
			break;
		case AMP_ASSOC_CONNECTED_CHANNEL:
			if (length > 9) {
				status = AMP_HCI_INVALID_PARAMETERS;
				break;
			}
			__aeabi_memcpy(link->connectedChannels, value, length);
			link->connectedChannelsLength = length;
			break;
		}
		data = value + length;
		used = (uint16_t)(used + 3 + length);
	}
	return status;
}

/*
 * 0x000215d8: the channel the local controller takes for a physical link: 5 or 6
 * when the peer's preferred list covers one of them, else its first channel.
 */
unsigned int ampAssocSelectChannel(unsigned int physicalIndex)
{
	struct AmpPhysicalLink *link;
	unsigned int length;
	unsigned int channel;
	unsigned int i;
	unsigned int first;
	unsigned int count;

	ampAssocResetConnectedChannels(&ampLocalAssoc);
	if (ampLocalAssoc.connectedChannelsLength != 0) {
		channel = ampLocalAssoc.connectedChannels[6];
	} else {
		link = &ampPhysicalLinks[physicalIndex];
		length = link->preferredChannelsLength;
		if (length == 6)
			return 5;
		for (i = 6; length > i; i += 3) {
			first = link->preferredChannels[i];
			count = link->preferredChannels[i + 1];
			if (first < 0xc9) {
				if (first <= 5 && first + count > 5)
					return 5;
				if (first <= 6 && first + count > 6)
					return 6;
			}
		}
		channel = link->preferredChannels[6];
	}
	if (channel > 11)
		return 0;
	return channel;
}

/* Frames pending for the link state machines are posted through the ROM. */
int ampLinkPostEvent(unsigned int index, uint32_t event, const void *record)
{
	if (index < AMP_PHYSICAL_LINKS)
		return mdevRomQueryState(&ampLinkMachines[index], event, record);
	return index;
}

/* Timer-driven reset of the PAL controller parameters (0x00021652). */
int ampHciResetState(void)
{
	ampHciCommandCount = 0;
	ampHciIdleFlag = 0;
	ampBufferCount = 4;
	ampBufferCountCopy = 4;
	ampStatus = 1;
	ampTotalBandwidth = 24000;
	ampMaxGuaranteedBandwidth = 24000;
	ampMinLatency = 100;
	ampMaxPduSize = 1492;
	ampControllerType = 1;
	ampPalCapabilities = 1;
	ampMaxAssocLength = 672;
	ampMaxFlushTimeout = 20000;
	ampBestEffortFlushTimeout = 20000;
	ampDutMode = 0;
	ampDutReports = 0;
	/* Marvell OUI 00:50:43, then the default device part. */
	ampLocalAssoc.macAddress[0] = 0x00;
	ampLocalAssoc.macAddress[1] = 0x50;
	ampLocalAssoc.macAddress[2] = 0x43;
	ampLocalAssoc.macAddress[3] = 0x88;
	ampLocalAssoc.macAddress[4] = 0x88;
	ampLocalAssoc.macAddress[5] = 0x88;
	ampLocalAssoc.preferredChannelsLength = 9;
	ampLocalAssoc.preferredChannels[0] = 'U';
	ampLocalAssoc.preferredChannels[1] = 'S';
	ampLocalAssoc.preferredChannels[2] = ' ';
	ampLocalAssoc.preferredChannels[3] = 201;
	ampLocalAssoc.preferredChannels[4] = 12;
	ampLocalAssoc.preferredChannels[5] = 0;
	ampLocalAssoc.preferredChannels[6] = 11;
	ampLocalAssoc.preferredChannels[7] = 1;
	ampLocalAssoc.preferredChannels[8] = 20;
	ampLocalAssoc.connectedChannelsLength = 9;
	ampLocalAssoc.connectedChannels[0] = 'U';
	ampLocalAssoc.connectedChannels[1] = 'S';
	ampLocalAssoc.connectedChannels[2] = ' ';
	ampLocalAssoc.connectedChannels[3] = 201;
	ampLocalAssoc.connectedChannels[4] = 12;
	ampLocalAssoc.connectedChannels[5] = 0;
	ampLocalAssoc.connectedChannels[6] = 11;
	ampLocalAssoc.connectedChannels[7] = 1;
	ampLocalAssoc.connectedChannels[8] = 20;
	ampLocalPalCapabilitiesLength = 4;
	ampLocalPalCapabilities[0] = 3;
	ampLocalPalCapabilities[1] = 0;
	ampLocalPalCapabilities[2] = 0;
	ampLocalPalCapabilities[3] = 0;
	ampFlowControlMode = 1;
	ampConnectionAcceptTimeout = 0xb540;
	ampLinkSupervisionTimeout = 32000;
	ampLogicalLinkAcceptTimeout = 0xb540;
	ampHciTimerValue = 0;
	__aeabi_memset4(ampEventMaskPage1, 8, 0xff);
	__aeabi_memset4(ampEventMaskPage2, 8, 0xff);
	ampPhysicalLinksInit();
	return 1;
}

/* ---- HCI commands ---------------------------------------------------------- */

/* Parameters of a received HCI command packet start three bytes into its data. */
static uint8_t *ampHciParameters(const OsaBuffer *buffer)
{
	return (uint8_t *)buffer + buffer->dataOffset;
}

/* 0x0002172a: Reset. */
int ampHciReset(uint16_t opcode)
{
	unsigned int i;

	for (i = 0; i < AMP_PHYSICAL_LINKS; i++) {
		if (ampPhysicalLinks[i].handle != AMP_LINK_FREE)
			ampHciSendHostEvent(3, 0, "w", (uint32_t)&ampPhysicalLinks[i]);
	}
	mdevRomProcessState(ampPalDevice);
	mdevRomProcessState(ampHciDev);
	ampHciSendHostEvent(0, 0, "bw", 0, (uint32_t)&ampHciCommandCount);
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021790: Read_Local_AMP_Info. */
int ampHciReadLocalAmpInfo(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendHostEvent(0, 0, "bw", 0, (uint32_t)&ampHciCommandCount);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbbwwwwbhhww", opcode, 0,
			ampStatus, ampTotalBandwidth, ampMaxGuaranteedBandwidth,
			ampMinLatency, ampMaxPduSize, ampControllerType,
			ampPalCapabilities, ampMaxAssocLength, ampMaxFlushTimeout,
			ampBestEffortFlushTimeout);
	return 0;
}

/* 0x0002181e: Read_Local_AMP_ASSOC. */
void ampHciReadLocalAmpAssoc(uint16_t opcode, OsaBuffer *command)
{
	uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int link;
	unsigned int length;

	ampHciSendHostEvent(0, 0, "bw", 0, (uint32_t)&ampHciCommandCount);
	ampHciAdjustCommandCount(-1);
	link = ampPhysicalLinkFind(data[3]);
	if (link >= AMP_PHYSICAL_LINKS && data[3] != 0)
		status = 2;
	length = ampAssocBuild(ampSupportedCommands, link);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbbhm", opcode, status, data[3],
			length, ampSupportedCommands, length);
}

/* 0x0002187e: Set_Event_Mask and Set_Event_Mask_Page_2. */
int ampHciSetEventMask(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *mask = ampHciParameters(command) + 3;
	void *page = (opcode & 0x3ff) == 1 ? ampEventMaskPage1 : ampEventMaskPage2;

	__aeabi_memcpy(page, mask, 8);
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x000218c0: Read_Connection_Accept_Timeout, Read_Link_Supervision_Timeout and
 * Read_Logical_Link_Accept_Timeout. */
int ampHciReadTimeout(uint16_t opcode, OsaBuffer *command)
{
	int status = 0;
	uint32_t timeout = 0;
	uint8_t *data;

	ampHciAdjustCommandCount(-1);
	switch (opcode & 0x3ff) {
	case 21:
		ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbh", opcode, 0,
				ampConnectionAcceptTimeout);
		break;
	case 54:
		data = ampHciParameters(command);
		if (ampPhysicalLinkFind(data[3]) < AMP_PHYSICAL_LINKS)
			timeout = ampLinkSupervisionTimeout;
		else
			status = AMP_HCI_INVALID_PARAMETERS;
		ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhh", opcode, status,
				(data[4] << 8) | data[3], timeout);
		break;
	case 97:
		ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbh", opcode, 0,
				ampLogicalLinkAcceptTimeout);
		break;
	}
	return status;
}

/* 0x00021938: Write_Connection_Accept_Timeout, Write_Link_Supervision_Timeout and
 * Write_Logical_Link_Accept_Timeout. */
int ampHciWriteTimeout(uint16_t opcode, OsaBuffer *command)
{
	int status = 0;
	uint8_t *data = ampHciParameters(command);
	uint32_t timeout;
	unsigned int link;

	ampHciAdjustCommandCount(-1);
	switch (opcode & 0x3ff) {
	case 22:
		timeout = (data[4] << 8) | data[3];
		if (timeout - 1 < 0xb540)
			ampConnectionAcceptTimeout = timeout;
		else
			status = AMP_HCI_INVALID_PARAMETERS;
		ampHciComplete(opcode, status);
		return status;
	case 55:
		link = ampPhysicalLinkFind(data[3]);
		timeout = (data[6] << 8) | data[5];
		if (timeout - 1 < 0xb540 && link < AMP_PHYSICAL_LINKS) {
			ampLinkSupervisionTimeout = timeout;
			ampHciSendHostEvent(13, 0, "w", 625 * timeout);
		} else {
			status = AMP_HCI_INVALID_PARAMETERS;
		}
		ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbh", opcode, status,
				(data[4] << 8) | data[3]);
		break;
	case 98:
		timeout = (data[4] << 8) | data[3];
		if (timeout - 1 < 0xb540)
			ampLogicalLinkAcceptTimeout = timeout;
		else
			status = AMP_HCI_INVALID_PARAMETERS;
		ampHciComplete(opcode, status);
		return status;
	}
	return status;
}

/* 0x000219f0: Read_Local_Version_Information. */
int ampHciReadLocalVersion(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	/* HCI version 5, HCI revision 1, PAL version 1, Marvell, PAL sub-version 1. */
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbbhbhh", opcode, 0, 5, 1, 1, 72, 1);
	return 0;
}

/* 0x00021a22: Read_BD_ADDR returns the MAC address of the controller. */
int ampHciReadBdAddr(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbm", opcode, 0,
			ampLocalAssoc.macAddress, 6);
	return 0;
}

/* 0x00021a4a: Read_Local_Supported_Commands. */
int ampHciReadLocalSupportedCommands(uint16_t opcode)
{
	__aeabi_memclr(ampSupportedCommands, 64);
	ampSupportedCommands[5] = 0xc0;
	ampSupportedCommands[6] = 2;
	ampSupportedCommands[7] = 12;
	ampSupportedCommands[9] = 0;
	ampSupportedCommands[11] = 3;
	ampSupportedCommands[14] = 8;
	ampSupportedCommands[15] = 60;
	ampSupportedCommands[16] = 7;
	ampSupportedCommands[21] = 0xff;
	ampSupportedCommands[22] = 0xff;
	ampSupportedCommands[23] = 0xf7;
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbm", opcode, 0,
			ampSupportedCommands, 64);
	return 0;
}

/* 0x00021aa4: Read_Location_Data. */
int ampHciReadLocationData(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbbhbb", opcode, 0,
			ampLocationDomainAware,
			(ampLocationDomain[1] << 8) | ampLocationDomain[0],
			ampLocationDomainOptions, ampLocationOptions);
	return 0;
}

/* 0x00021ae2: Write_Location_Data. */
int ampHciWriteLocationData(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampLocationDomainAware = data[3];
	ampLocationDomain[0] = data[4];
	ampLocationDomain[1] = data[5];
	ampLocationDomainOptions = data[6];
	ampLocationOptions = data[7];
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021c70: Read_Flow_Control_Mode. */
int ampHciReadFlowControlMode(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbb", opcode, 0, ampFlowControlMode);
	return 0;
}

/* 0x00021c94: Write_Flow_Control_Mode. */
int ampHciWriteFlowControlMode(uint16_t opcode, OsaBuffer *command)
{
	ampFlowControlMode = ampHciParameters(command)[3];
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021cbe: Read_Data_Block_Size. */
int ampHciReadDataBlockSize(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampAclPacketLength = 1492;
	ampDataBlockLength = 1492;
	ampBufferCountCopy = ampBufferCount;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhhh", opcode, 0, 1492, 1492,
			ampBufferCount & 0xffff);
	return 0;
}

/* 0x00021cf8: Read_Buffer_Size. */
int ampHciReadBufferSize(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampAclPacketLength = 1492;
	ampDataBlockLength = 1492;
	ampBufferCountCopy = ampBufferCount;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhbhh", opcode, 0, 1492, 0,
			ampBufferCount & 0xffff, 0);
	return 0;
}

/* 0x00021d36: vendor command 0xfc22 sets the Bluetooth address of the controller (type 0xfe). */
int ampHciVendorSetAddress(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;

	if (data[3] == 0xfe && data[4] == 6)
		ampHciSendHostEvent(14, 0, "bm", data[3], data + 5, data[4]);
	else
		status = AMP_HCI_INVALID_PARAMETERS;
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, status);
	return status;
}

/* 0x00021d7a: vendor command 0xfc61 passes its parameters on to the PAL driver. */
int ampHciVendorPassThroughA(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampHciSendHostEvent(12, 0, "bm", data[3], data + 4, data[2] - 1);
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021db4: vendor command 0xfc62, the same with another event. */
int ampHciVendorPassThroughB(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampHciSendHostEvent(9, 0, "bm", data[3], data + 4, data[2] - 1);
	ampHciAdjustCommandCount(-1);
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021dee: Enable_Device_Under_Test_Mode. */
int ampHciEnableDeviceUnderTestMode(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	ampDutMode = 1;
	ampHciComplete(opcode, 0);
	return 0;
}

/* 0x00021e14: AMP_Test. */
int ampHciAmpTest(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status;

	ampHciAdjustCommandCount(-1);
	if (ampDutMode != 0) {
		if (ampDutReports != 1) {
			ampHciSendCommandStatus(opcode, 0);
			ampHciSendEvent(73, 0, "bb", 0, data[3]);
			ampHciSendEvent(74, 0, "bb", 0, data[3]);
			return 0;
		}
		status = AMP_HCI_LIMITED_RESOURCES;
	} else {
		status = AMP_HCI_COMMAND_DISALLOWED;
	}
	ampHciComplete(opcode, status);
	return 0;
}

/* 0x00021e70: AMP_Test_End. */
int ampHciAmpTestEnd(uint16_t opcode)
{
	ampHciAdjustCommandCount(-1);
	if (ampDutMode != 0) {
		ampHciSendCommandStatus(opcode, 0);
		ampDutReports = 0;
		ampHciSendEvent(74, 0, "bb", 0, 2);
	} else {
		ampHciComplete(opcode, AMP_HCI_COMMAND_DISALLOWED);
	}
	return 0;
}

/* Vendor command 0xfc5b: sub-commands 0xf1 and 0xf2 answer with a vendor event. */
int ampHciVendorFc5b(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampHciAdjustCommandCount(-1);
	ampHciSendCommandStatus(opcode, 0);
	if (data[3] == 0xf1) {
		ampHciSendEvent(0xff, 0, "Vbbbb", 0x5b, 0xf1, 0, 1);
	} else if (data[3] == 0xf2) {
		btLmpLcFormatContext(1, 0, 2, 0);
		ampHciSendEvent(0xff, 0, "Vbbbb", 0x5b, 0xf2, 0, 1);
	}
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbb", opcode, 0, 0);
	return 0;
}

/* Vendor command 0xfc23: sub-commands 2 and 3 with their parameters. */
int ampHciVendorFc23(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;

	if (data[3] == 2) {
		if (data[2] == 1)
			btLmpLcFormatContext(1, 0, 1, 0);
		else if (data[2] == 3)
			btLmpLcFormatContext(1, 0, 1,
					     (uint16_t)((data[5] << 8) | data[4] |
							(((uint16_t)((data[7] << 8) | data[6])) << 8)));
		else
			status = AMP_HCI_INVALID_PARAMETERS;
	} else if (data[3] == 3 && data[2] == 1) {
		btLmpLcFormatContext(1, 0, 2, 0);
	} else {
		status = AMP_HCI_INVALID_PARAMETERS;
	}
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(0xff, 0, "Vbbb", 0x23, data[3], status);
	return 0;
}

/* ---- Physical and logical link commands --------------------------------------- */

extern struct AmpQueue ampLinkQueues[];
extern int fmCoexAcquireState(struct AmpLinkMachine *machine);
extern void osa_SemaphoreCreate(void *semaphore, void *config);

/* 0x00021ee4: empties the link tables and rearms the link state machines. */
void ampPhysicalLinksInit(void)
{
	unsigned int i;
	unsigned int slot;
	struct AmpPhysicalLink *link;

	for (i = 0; i < AMP_PHYSICAL_LINKS; i++) {
		link = &ampPhysicalLinks[i];
		link->handle = AMP_LINK_FREE;
		link->logicalLinkCount = 0;
		for (slot = 0; slot < AMP_LOGICAL_LINKS; slot++) {
			link->logicalLinks[slot].handle = AMP_LOGICAL_FREE;
			link->logicalLinks[slot].pending = 0;
			link->logicalLinks[slot].failedContactCounter = 0;
		}
		fmCoexAcquireState(&ampLinkMachines[i]);
		osa_SemaphoreCreate(&ampLinkMachines[i], &ampLinkMachines[i].words[1]);
	}
	if (ampRemoteAssocBuffer != NULL) {
		osa_BufferFree(ampRemoteAssocBuffer);
		ampRemoteAssocBuffer = NULL;
	}
}

/* 0x00021f52: Create_Physical_Link and Accept_Physical_Link. */
int ampHciCreatePhysicalLink(uint16_t opcode, OsaBuffer *command, uint32_t argument0,
			     uint32_t argument1)
{
	const uint8_t *data = ampHciParameters(command);
	struct AmpLinkEvent event;
	struct AmpPhysicalLink *link;
	unsigned int index = ampPhysicalLinkAllocate(data[3]);
	unsigned int kind;

	if (index < AMP_PHYSICAL_LINKS) {
		link = &ampPhysicalLinks[index];
		link->keyLength = data[4];
		link->keyType = data[5];
		__aeabi_memcpy(link->key, data + 6, data[4]);
		kind = (uint8_t)(data[0] - 53);
		link->state = kind;
		event.opcode = (data[1] << 8) | data[0];
		event.link = index;
		event.state = kind;
		event.word1 = argument0;
		event.word2 = argument1;
		ampLinkPostEvent(index, kind == 0 ? 1 : 2, &event);
	} else {
		ampHciAdjustCommandCount(-1);
		ampHciSendCommandStatus((data[1] << 8) | data[0], AMP_HCI_CONNECTION_LIMIT);
	}
	if (ampRemoteAssocBuffer != NULL) {
		osa_BufferFree(ampRemoteAssocBuffer);
		ampRemoteAssocBuffer = NULL;
	}
	return 0;
}

/* 0x00021fde: Accept_Physical_Link has no handler of its own. */
void ampHciAcceptPhysicalLinkStub(void)
{
}

/* 0x00021fe0: Disconnect_Physical_Link. */
int ampHciDisconnectPhysicalLink(uint16_t opcode, OsaBuffer *command, uint32_t argument0,
				 uint32_t argument1)
{
	const uint8_t *data = ampHciParameters(command);
	struct AmpLinkEvent event;
	unsigned int index = ampPhysicalLinkFind(data[3]);

	if (index < AMP_PHYSICAL_LINKS) {
		ampDisconnectReason = data[4];
		event.opcode = (data[1] << 8) | data[0];
		event.link = index;
		event.state = 0;
		event.word1 = argument0;
		event.word2 = argument1;
		ampLinkPostEvent(index, 4, &event);
		ampPalNotifyDisconnect(index);
	} else {
		ampHciAdjustCommandCount(-1);
		ampHciSendCommandStatus(opcode, AMP_HCI_UNKNOWN_CONNECTION);
	}
	return 0;
}

/* 0x0002202e: Short_Range_Mode. */
int ampHciShortRangeMode(uint16_t opcode, OsaBuffer *command, uint32_t argument0,
			 uint32_t argument1)
{
	const uint8_t *data = ampHciParameters(command);
	struct AmpLinkEvent event;
	int status = 0;
	unsigned int index = ampPhysicalLinkFind(data[3]);

	if (index < AMP_PHYSICAL_LINKS) {
		ampPhysicalLinks[index].shortRangeMode = data[4];
		event.opcode = (data[1] << 8) | data[0];
		event.link = index;
		event.state = 0;
		event.word1 = argument0;
		event.word2 = argument1;
		ampLinkPostEvent(index, 5, &event);
	} else {
		status = AMP_HCI_UNKNOWN_CONNECTION;
		ampHciAdjustCommandCount(-1);
		ampHciSendCommandStatus(opcode, AMP_HCI_UNKNOWN_CONNECTION);
	}
	return status;
}

/* 0x0002207c: Write_Remote_AMP_ASSOC; the ASSOC arrives in fragments. */
int ampHciWriteRemoteAmpAssoc(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	struct AmpLinkEvent event;
	int status = 0;
	unsigned int link = ampPhysicalLinkFind(data[3]);
	unsigned int fragment = (data[7] << 8) | data[6];
	unsigned int total = (uint16_t)(fragment + ((data[5] << 8) | data[4]));
	uint32_t length;
	unsigned int channel;

	if (link < AMP_PHYSICAL_LINKS) {
		if (total > 0x100)
			status = AMP_HCI_LIMITED_RESOURCES;
	} else {
		status = AMP_HCI_UNKNOWN_CONNECTION;
	}
	if ((ampRemoteAssocBuffer != NULL || status != 0) == 0) {
		ampRemoteAssocBuffer = ampHciAllocBuffer(&ampHciPool, AMP_POOL_BUFFER_SIZE, 0);
		if (ampRemoteAssocBuffer == NULL) {
			status = AMP_HCI_MEMORY_CAPACITY;
			goto check;
		}
		ampRemoteAssocBuffer->length = 0;
	}
	if (status == 0 && ampRemoteAssocBuffer != NULL) {
		__aeabi_memcpy((uint8_t *)ampRemoteAssocBuffer + ampRemoteAssocBuffer->dataOffset +
			       ampRemoteAssocBuffer->length, data + 8, fragment);
		ampRemoteAssocBuffer->length += fragment;
	}
check:
	/* The original reads the length even without a buffer; treat that as no match. */
	length = ampRemoteAssocBuffer != NULL ? ampRemoteAssocBuffer->length : ~total;
	if (length != total) {
		if (status != 0)
			goto fail;
		goto progress;
	}
	status = ampAssocParse((uint8_t *)ampRemoteAssocBuffer + ampRemoteAssocBuffer->dataOffset,
			       length, link);
	if (status == 0) {
		channel = ampAssocSelectChannel(link);
		ampPhysicalLinks[link].channel = channel;
		if (channel == 0) {
			status = AMP_HCI_NO_USABLE_CHANNEL;
			goto fail;
		}
progress:
		if (ampRemoteAssocBuffer->length == total) {
			event.opcode = (data[1] << 8) | data[0];
			event.link = link;
			event.state = ampPhysicalLinks[link].state;
			ampLinkPostEvent(link, 3, &event);
			osa_BufferFree(ampRemoteAssocBuffer);
			ampRemoteAssocBuffer = NULL;
		} else {
			ampHciAdjustCommandCount(-1);
			ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbb", opcode, status, data[3]);
		}
		return status;
	}
fail:
	if (ampRemoteAssocBuffer != NULL) {
		osa_BufferFree(ampRemoteAssocBuffer);
		ampRemoteAssocBuffer = NULL;
	}
	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbb", (data[1] << 8) | data[0], status,
			data[3]);
	event.opcode = (data[1] << 8) | data[0];
	event.link = link;
	event.state = ampPhysicalLinks[link].state;
	event.word1 = status;
	ampLinkPostEvent(link, 16, &event);
	return status;
}

/* ---- Logical links ------------------------------------------------------------ */

/* 0x0002268e: Logical Link Complete event. */
int ampHciLogicalLinkComplete(int status, int logicalHandle, int physicalHandle, int flowSpecId)
{
	ampHciSendEvent(0x45, 0x20, "bhbb", status, logicalHandle, physicalHandle, flowSpecId);
	return 0;
}

/* 0x0002278c: Disconnection Logical Link Complete event. */
int ampHciDisconnectionLogicalLinkComplete(int status, int logicalHandle, int reason)
{
	ampHciSendEvent(0x46, 0x40, "bhb", status, logicalHandle, reason);
	return 0;
}

/* 0x00022902: Flow Spec Modify Complete event. */
int ampHciFlowSpecModifyComplete(int status, int logicalHandle)
{
	ampHciSendEvent(0x47, 0x80, "bh", status, logicalHandle);
	return 0;
}

/* 0x000229ba: Flush Occurred event. */
int ampHciFlushOccurred(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampHciSendEvent(0x11, 0, "h", (data[6] << 8) | data[5]);
	return 0;
}

/* 0x000229d6: Enhanced Flush Complete event. */
int ampHciEnhancedFlushComplete(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);

	ampHciAdjustCommandCount(-1);
	ampHciSendEvent(0x39, 0, "h", (data[6] << 8) | data[5]);
	ampHciSendNopCommandStatus();
	return 0;
}

/* 0x00022a2e: Number Of Completed Data Blocks event for one logical link. */
int ampHciNumberOfCompletedDataBlocks(int logicalHandle, int completedPackets, int completedBlocks)
{
	ampHciSendEvent(0x48, 0x101, "hbhhh", (int8_t)ampBufferCount, 1, logicalHandle,
			completedPackets, completedBlocks);
	return 0;
}

/* 0x00022a50: Number Of Completed Packets event for one logical link. */
int ampHciNumberOfCompletedPackets(int logicalHandle, int completedPackets)
{
	ampHciSendEvent(0x13, 0, "bhh", 1, logicalHandle, completedPackets);
	return 0;
}

/* 0x00022864: removes a logical link and reports why. */
int ampLogicalLinkRemove(int logicalHandle, int reason)
{
	unsigned int slot = ampLogicalLinkFind(logicalHandle);
	unsigned int physical = ampPhysicalLinkFindByLogical(logicalHandle);
	struct AmpPhysicalLink *link;
	struct AmpLogicalLink *logical;

	if (slot == AMP_LOGICAL_LINKS)
		return 2;
	ampHciDisconnectionLogicalLinkComplete(0, logicalHandle, reason);
	link = &ampPhysicalLinks[physical];
	if (link->logicalLinkCount != 0)
		link->logicalLinkCount--;
	logical = &link->logicalLinks[slot];
	logical->handle = AMP_LOGICAL_FREE;
	logical->rxFlowSpec.id = 0;
	logical->txFlowSpec.id = 0;
	return 0;
}

/* 0x000226aa: Create_Logical_Link. */
int ampHciCreateLogicalLink(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int physical = ampPhysicalLinkFind(data[3]);
	unsigned int slot = 0;
	uint32_t interArrival;
	struct AmpPhysicalLink *link;
	struct AmpLogicalLink *logical;

	ampHciAdjustCommandCount(-1);
	if (physical >= AMP_PHYSICAL_LINKS) {
		status = 2;
	} else if (ampLinkMachines[physical].state == 4) {
		if (data[5] != 2 || (interArrival = __aeabi_uread4(data + 8)) == 0xffffffff ||
		    (interArrival != 0 &&
		     ampMaxGuaranteedBandwidth >=
		     8000 * ((data[7] << 8) | data[6]) / interArrival)) {
			slot = ampLogicalLinkAllocate(data[3]);
			if (slot >= AMP_LOGICAL_LINKS)
				status = AMP_HCI_CONNECTION_LIMIT;
		} else {
			status = AMP_HCI_QOS_REJECTED;
		}
	} else if (ampLinkMachines[physical].state == 2) {
		status = AMP_HCI_COMMAND_DISALLOWED;
	} else {
		status = 2;
	}
	ampHciSendCommandStatus(opcode, status);
	if (status == 0) {
		link = &ampPhysicalLinks[physical];
		logical = &link->logicalLinks[slot];
		__aeabi_memcpy(&logical->rxFlowSpec, data + 20, 16);
		__aeabi_memcpy(&logical->txFlowSpec, data + 4, 16);
		logical->origin = (uint8_t)((data[1] << 8 | data[0]) - 0x38);
		link->logicalLinkCount++;
		ampHciLogicalLinkComplete(0, logical->handle, link->handle, logical->txFlowSpec.id);
		ampHciSendNopCommandStatus();
	}
	return status;
}

/* 0x0002278a: Accept_Logical_Link has no handler of its own. */
void ampHciAcceptLogicalLinkStub(void)
{
}

/* 0x000227a2: Disconnect_Logical_Link. */
int ampHciDisconnectLogicalLink(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	unsigned int physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	struct AmpPhysicalLink *link;
	struct AmpLogicalLink *logical;

	if (slot == AMP_LOGICAL_LINKS) {
		status = 2;
		ampHciAdjustCommandCount(-1);
	}
	ampHciSendCommandStatus(opcode, status);
	if (status == 0) {
		link = &ampPhysicalLinks[physical];
		logical = &link->logicalLinks[slot];
		ampHciDisconnectionLogicalLinkComplete(0, logical->handle, 22);
		logical->handle = AMP_LOGICAL_FREE;
		if (link->logicalLinkCount != 0)
			link->logicalLinkCount--;
		ampHciAdjustCommandCount(-1);
		ampHciSendNopCommandStatus();
	}
	return status;
}

/* 0x000228b8: Logical_Link_Cancel. */
int ampHciLogicalLinkCancel(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;

	if (ampPhysicalLinkFind(data[3]) == AMP_PHYSICAL_LINKS)
		status = 2;
	if (ampLogicalLinkFindByFlowSpec(data[4]) == AMP_LOGICAL_LINKS)
		status = 2;
	ampHciAdjustCommandCount(-1);
	if (status == 0)
		status = 11;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbbb", opcode, status, data[3], data[4]);
	return status;
}

/* 0x00022916: Flow_Spec_Modify. */
int ampHciFlowSpecModify(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	unsigned int slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	struct AmpLogicalLink *logical;

	if (physical == AMP_PHYSICAL_LINKS || slot == AMP_LOGICAL_LINKS) {
		status = 2;
		ampHciAdjustCommandCount(-1);
	} else {
		logical = &ampPhysicalLinks[physical].logicalLinks[slot];
		if (logical->txFlowSpec.id != data[5] || logical->txFlowSpec.serviceType != data[6] ||
		    logical->rxFlowSpec.id != data[21] || logical->rxFlowSpec.serviceType != data[22]) {
			status = AMP_HCI_QOS_UNACCEPTABLE;
			ampHciAdjustCommandCount(-1);
		}
	}
	ampHciSendCommandStatus(opcode, status);
	if (status == 0) {
		logical = &ampPhysicalLinks[physical].logicalLinks[slot];
		ampHciFlowSpecModifyComplete(0, logical->handle);
		ampHciAdjustCommandCount(-1);
		ampHciSendNopCommandStatus();
	}
	return status;
}

/* 0x00022452: Write_Best_Effort_Flush_Timeout. */
int ampHciWriteBestEffortFlushTimeout(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int physical;
	unsigned int slot;

	ampHciAdjustCommandCount(-1);
	physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	if (physical == AMP_PHYSICAL_LINKS || slot == AMP_LOGICAL_LINKS)
		status = 2;
	else
		ampPhysicalLinks[physical].logicalLinks[slot].txFlowSpec.flushTimeout =
			__aeabi_uread4(data + 5);
	ampHciComplete(opcode, status);
	return status;
}

/* 0x000224b6: Read_Best_Effort_Flush_Timeout. */
int ampHciReadBestEffortFlushTimeout(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	uint32_t timeout;
	unsigned int physical;
	unsigned int slot;

	ampHciAdjustCommandCount(-1);
	physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	if (physical == AMP_PHYSICAL_LINKS || slot == AMP_LOGICAL_LINKS) {
		status = 2;
		timeout = 0xffffffff;
	} else {
		timeout = ampPhysicalLinks[physical].logicalLinks[slot].txFlowSpec.flushTimeout;
	}
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbw", opcode, status, timeout);
	return status;
}

/* 0x000223d4: Enhanced_Flush. */
int ampHciEnhancedFlush(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int physical;
	uint32_t handle = (data[4] << 8) | data[3];

	if (ampLogicalLinkFind(handle) == AMP_LOGICAL_LINKS)
		status = 2;
	if (data[5] != 0)
		status = AMP_HCI_INVALID_PARAMETERS;
	ampHciSendCommandStatus(opcode, status);
	if (status != 0) {
		ampHciAdjustCommandCount(-1);
		ampHciSendEvent(0x39, 0, "h", (data[4] << 8) | data[3]);
		ampHciSendNopCommandStatus();
	} else {
		physical = ampPhysicalLinkFindByLogical(handle);
		ampHciSendHostEvent(11, 0, "hw", handle, (uint32_t)&ampPhysicalLinks[physical]);
	}
	return 0;
}

/* 0x00022516: Read_Failed_Contact_Counter (reads and clears). */
int ampHciReadFailedContactCounter(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	uint32_t counter = 0;
	unsigned int slot;
	unsigned int physical;
	struct AmpLogicalLink *logical;

	ampHciAdjustCommandCount(-1);
	slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	if (physical == AMP_PHYSICAL_LINKS || slot == AMP_LOGICAL_LINKS) {
		status = 2;
	} else {
		logical = &ampPhysicalLinks[physical].logicalLinks[slot];
		counter = logical->failedContactCounter;
		logical->failedContactCounter = 0;
	}
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhh", opcode, status,
			(data[4] << 8) | data[3], counter);
	return status;
}

/* 0x00022586: Reset_Failed_Contact_Counter. */
int ampHciResetFailedContactCounter(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	unsigned int slot;
	unsigned int physical;

	ampHciAdjustCommandCount(-1);
	slot = ampLogicalLinkFind((data[4] << 8) | data[3]);
	physical = ampPhysicalLinkFindByLogical((data[4] << 8) | data[3]);
	if (physical == AMP_PHYSICAL_LINKS || slot == AMP_LOGICAL_LINKS)
		status = 2;
	else
		ampPhysicalLinks[physical].logicalLinks[slot].failedContactCounter = 0;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbh", opcode, status,
			(data[4] << 8) | data[3]);
	return 0;
}

/* 0x00022b4e ... */

/* 0x000230b8: link quality reported by Read_Link_Quality (fixed in the firmware). */
int ampLinkQuality(void)
{
	return 102;
}

/* 0x000230bc: RSSI reported by Read_RSSI, -120 dBm. */
int ampRssi(void)
{
	return 136;
}

/* 0x000225ee: Read_Link_Quality. */
int ampHciReadLinkQuality(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	int quality = 0;

	ampHciAdjustCommandCount(-1);
	if (ampPhysicalLinkFind(data[3]) < AMP_PHYSICAL_LINKS)
		quality = ampLinkQuality();
	else
		status = 2;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhb", opcode, status,
			(data[4] << 8) | data[3], quality);
	return 0;
}

/* 0x0002263e: Read_RSSI. */
int ampHciReadRssi(uint16_t opcode, OsaBuffer *command)
{
	const uint8_t *data = ampHciParameters(command);
	int status = 0;
	int rssi = 0;

	ampHciAdjustCommandCount(-1);
	if (ampPhysicalLinkFind(data[3]) < AMP_PHYSICAL_LINKS)
		rssi = ampRssi();
	else
		status = 2;
	ampHciSendEvent(AMP_EVENT_COMMAND_COMPLETE, 0, "Rhbhb", opcode, status,
			(data[4] << 8) | data[3], rssi);
	return 0;
}

/* ---- Command dispatcher ------------------------------------------------------- */

/* Opcode groups (OGF). */
enum {
	AMP_OGF_LINK_CONTROL = 1,
	AMP_OGF_CONTROLLER_BASEBAND = 3,
	AMP_OGF_INFORMATIONAL = 4,
	AMP_OGF_STATUS = 5,
	AMP_OGF_TESTING = 6,
	AMP_OGF_VENDOR = 63,
};

/* The pending-command queue of the HCI task (messages carry HCI command packets). */
extern struct AmpQueue ampCommandQueue;

/* Handles one queued HCI command; unknown opcodes are rejected. */
static int ampHciDispatch(OsaBuffer *command, uint32_t opcode)
{
	uint32_t ogf = ((ampHciParameters(command)[1] << 8) >> 10);
	uint32_t ocf = opcode & 0x3ff;

	switch (ogf) {
	case AMP_OGF_STATUS:
		switch ((uint8_t)opcode) {
		case 1:
			ampHciReadFailedContactCounter(opcode, command);
			return 1;
		case 2:
			ampHciResetFailedContactCounter(opcode, command);
			return 1;
		case 3:
			ampHciReadLinkQuality(opcode, command);
			return 1;
		case 5:
			ampHciReadRssi(opcode, command);
			return 1;
		case 9:
			ampHciReadLocalAmpInfo(opcode);
			return 1;
		case 10:
			ampHciReadLocalAmpAssoc(opcode, command);
			return 1;
		case 11:
			ampHciWriteRemoteAmpAssoc(opcode, command);
			return 1;
		}
		return 0;
	case AMP_OGF_TESTING:
		switch ((uint8_t)opcode) {
		case 3:
			ampHciEnableDeviceUnderTestMode(opcode);
			return 1;
		case 8:
			ampHciAmpTestEnd(opcode);
			return 1;
		case 9:
			ampHciAmpTest(opcode, command);
			return 1;
		}
		return 0;
	case AMP_OGF_VENDOR:
		switch (ocf) {
		case 91:
			ampHciVendorFc5b(opcode, command);
			return 1;
		case 97:
			ampHciVendorPassThroughA(opcode, command);
			return 1;
		case 98:
			ampHciVendorPassThroughB(opcode, command);
			return 1;
		case 34:
			ampHciVendorSetAddress(opcode, command);
			return 1;
		case 35:
			ampHciVendorFc23(opcode, command);
			return 1;
		}
		return 0;
	case AMP_OGF_LINK_CONTROL:
		switch ((uint8_t)opcode) {
		case 0x35:
		case 0x36:
			ampHciCreatePhysicalLink(opcode, command, 0, ocf - 0x35);
			return 1;
		case 0x37:
			ampHciDisconnectPhysicalLink(opcode, command, 0, ocf - 0x35);
			return 1;
		case 0x38:
			ampHciCreateLogicalLink(opcode, command);
			return 1;
		case 0x39:
			ampHciAcceptLogicalLinkStub();
			return 1;
		case 0x3a:
			ampHciDisconnectLogicalLink(opcode, command);
			return 1;
		case 0x3b:
			ampHciLogicalLinkCancel(opcode, command);
			return 1;
		case 0x3c:
			ampHciFlowSpecModify(opcode, command);
			return 1;
		}
		return 0;
	case AMP_OGF_CONTROLLER_BASEBAND:
		switch (ocf) {
		case 0x01:
		case 0x63:
			ampHciSetEventMask(opcode, command);
			return 1;
		case 0x03:
			ampHciReset(opcode);
			return 1;
		case 0x15:
		case 0x36:
		case 0x61:
			ampHciReadTimeout(opcode, command);
			return 1;
		case 0x16:
		case 0x37:
		case 0x62:
			ampHciWriteTimeout(opcode, command);
			return 1;
		case 0x5f:
			ampHciEnhancedFlush(opcode, command);
			return 1;
		case 0x64:
			ampHciReadLocationData(opcode);
			return 1;
		case 0x65:
			ampHciWriteLocationData(opcode, command);
			return 1;
		case 0x66:
			ampHciReadFlowControlMode(opcode);
			return 1;
		case 0x67:
			ampHciWriteFlowControlMode(opcode, command);
			return 1;
		case 0x69:
			ampHciReadBestEffortFlushTimeout(opcode, command);
			return 1;
		case 0x6a:
			ampHciWriteBestEffortFlushTimeout(opcode, command);
			return 1;
		case 0x6b:
			ampHciShortRangeMode(opcode, command, 0, ocf - 99);
			return 1;
		}
		return 0;
	case AMP_OGF_INFORMATIONAL:
		switch (ocf) {
		case 1:
			ampHciReadLocalVersion(opcode);
			return 1;
		case 2:
			ampHciReadLocalSupportedCommands(opcode);
			return 1;
		case 5:
			ampHciReadBufferSize(opcode);
			return 1;
		case 9:
			ampHciReadBdAddr(opcode);
			return 1;
		case 10:
			ampHciReadDataBlockSize(opcode);
			return 1;
		}
		return 0;
	}
	return 0;
}

/*
 * 0x0002220c: drains the command queue of the HCI task. While two commands are
 * in progress only Reset is accepted; unknown opcodes get Command Complete 1.
 */
int ampHciProcessCommands(void)
{
	OsaBuffer *command;
	uint8_t *data;
	uint32_t opcode;
	uint32_t ogf;
	uint32_t ocf;
	int known;

	do {
		command = osa_MsgQueueRecv(&ampCommandQueue);
		if (command == NULL)
			break;
		data = ampHciParameters(command);
		opcode = data[0] | (data[1] << 8);
		ogf = (data[1] << 8) >> 10;
		ocf = data[0] | ((data[1] << 8) & 0x3ff);
		if (ampHciCommandCount == 2 && (ogf != AMP_OGF_CONTROLLER_BASEBAND || ocf != 3)) {
			ampHciDisallowCommand(opcode, AMP_HCI_COMMAND_DISALLOWED, 0);
		} else {
			ampHciAdjustCommandCount(1);
			known = ampHciDispatch(command, opcode);
			if (!known)
				ampHciRejectUnknownCommand(opcode);
		}
		osa_BufferFree(command);
		mdevIoctl(ampSdioDev, 0x14, 0);
	} while (osa_MsgQueueCount(&ampCommandQueue) != 0);
	return 1;
}
