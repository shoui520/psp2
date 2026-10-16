/* Complete UDP wire header; descriptive field names follow protocol roles. */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct NetUdpHeader {
	uint8_t sourcePortBe[2];
	uint8_t destinationPortBe[2];
	uint8_t lengthBe[2];
	uint8_t checksumBe[2];
} NetUdpHeader;

_Static_assert(sizeof(NetUdpHeader) == 8, "UDP header");
_Static_assert(offsetof(NetUdpHeader, sourcePortBe) == 0, "UDP source port");
_Static_assert(offsetof(NetUdpHeader, destinationPortBe) == 2, "UDP destination port");
_Static_assert(offsetof(NetUdpHeader, lengthBe) == 4, "UDP length");
_Static_assert(offsetof(NetUdpHeader, checksumBe) == 6, "UDP checksum");
