#pragma once

#include <stddef.h>
#include <stdint.h>

/* SDIO packet prefix, accessed as native halfwords by the host and as
 * little-endian wire bytes by the embedded ARMv5 firmware.
 */
typedef union WlanPacketHeader {
	struct {
		uint8_t bssType;
		uint8_t bssNumber;
		uint16_t packetLength;
		uint16_t packetOffset;
		uint16_t packetType;
	};
	struct {
		uint8_t bssType;
		uint8_t bssNumber;
		uint8_t packetLength[2];
		uint8_t packetOffset[2];
		uint8_t packetType[2];
	} wire;
} WlanPacketHeader;

typedef struct WlanRxDescriptor {
	WlanPacketHeader header;
	uint16_t sequence;
	uint8_t priority;
	uint8_t rate;
	int8_t snr;
	int8_t noiseFloor;
	uint8_t htInfo;
	uint8_t reserved;
} WlanRxDescriptor;

typedef struct WlanTxDescriptor {
	WlanPacketHeader header;
	uint32_t control;
	uint8_t priority;
	uint8_t flags;
	uint8_t delay;
	uint8_t reserved;
} WlanTxDescriptor;

typedef struct WlanLlcSnapPrefix {
	uint8_t destinationSap;
	uint8_t sourceSap;
	uint8_t control;
	uint8_t organization[3];
} WlanLlcSnapPrefix;

_Static_assert(sizeof(WlanPacketHeader) == 8, "SDIO packet header");
_Static_assert(__alignof__(WlanPacketHeader) == 2, "native packet header alignment");
_Static_assert(offsetof(WlanPacketHeader, packetLength) == 2, "packet length");
_Static_assert(offsetof(WlanPacketHeader, packetOffset) == 4, "packet offset");
_Static_assert(offsetof(WlanPacketHeader, packetType) == 6, "packet type");
_Static_assert(sizeof(WlanRxDescriptor) == 16, "firmware RX descriptor");
_Static_assert(__alignof__(WlanRxDescriptor) == 2, "native RX alignment");
_Static_assert(offsetof(WlanRxDescriptor, header.packetLength) == 2, "RX packet length");
_Static_assert(offsetof(WlanRxDescriptor, header.packetOffset) == 4, "RX packet offset");
_Static_assert(offsetof(WlanRxDescriptor, header.packetType) == 6, "RX packet type");
_Static_assert(sizeof(WlanTxDescriptor) == 16, "firmware TX descriptor");
_Static_assert(__alignof__(WlanTxDescriptor) == 4, "native TX alignment");
_Static_assert(offsetof(WlanTxDescriptor, control) == 8, "TX control");
_Static_assert(offsetof(WlanTxDescriptor, priority) == 12, "TX priority");
_Static_assert(sizeof(WlanLlcSnapPrefix) == 6, "RFC1042 SNAP prefix");
