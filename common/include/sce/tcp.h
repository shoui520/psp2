/* Complete fixed TCP wire header; options follow this record. */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct NetTcpHeader {
	uint8_t sourcePortBe[2];
	uint8_t destinationPortBe[2];
	uint8_t sequenceBe[4];
	uint8_t acknowledgementBe[4];
	uint8_t dataOffsetAndReserved;
	uint8_t flags;
	uint8_t windowBe[2];
	uint8_t checksumBe[2];
	uint8_t urgentPointerBe[2];
} NetTcpHeader;

_Static_assert(sizeof(NetTcpHeader) == 20, "TCP fixed header");
_Static_assert(offsetof(NetTcpHeader, sourcePortBe) == 0, "TCP source port");
_Static_assert(offsetof(NetTcpHeader, destinationPortBe) == 2, "TCP destination port");
_Static_assert(offsetof(NetTcpHeader, sequenceBe) == 4, "TCP sequence");
_Static_assert(offsetof(NetTcpHeader, acknowledgementBe) == 8, "TCP acknowledgement");
_Static_assert(offsetof(NetTcpHeader, dataOffsetAndReserved) == 12, "TCP data offset");
_Static_assert(offsetof(NetTcpHeader, flags) == 13, "TCP flags");
_Static_assert(offsetof(NetTcpHeader, windowBe) == 14, "TCP window");
_Static_assert(offsetof(NetTcpHeader, checksumBe) == 16, "TCP checksum");
_Static_assert(offsetof(NetTcpHeader, urgentPointerBe) == 18, "TCP urgent pointer");
