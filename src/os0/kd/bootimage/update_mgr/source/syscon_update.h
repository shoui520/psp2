#ifndef UPDATE_MGR_SYSCON_UPDATE_H
#define UPDATE_MGR_SYSCON_UPDATE_H

#include <stdint.h>

typedef struct SysconUpdatePacketHeader {
	uint8_t type;
	uint8_t dataOffset;
	uint16_t dataSize;
} SysconUpdatePacketHeader;

typedef struct SysconUpdateVersionPacket {
	SysconUpdatePacketHeader header;
	uint32_t firmwareVersion;
	uint32_t hardwareInfo;
	uint32_t reserved;
} SysconUpdateVersionPacket;

typedef struct SysconUpdateKeyRevisionPacket {
	SysconUpdatePacketHeader header;
	uint32_t imageSize;
	uint32_t firmwareType;
	uint32_t reserved;
} SysconUpdateKeyRevisionPacket;

typedef struct SysconUpdateSignaturePacket {
	SysconUpdatePacketHeader header;
	uint8_t signature[24];
	uint32_t reserved;
} SysconUpdateSignaturePacket;

typedef struct SysconUpdateSegmentHeader {
	SysconUpdatePacketHeader header;
	uint32_t segmentNumber;
	int32_t size;
	uint32_t reserved;
} SysconUpdateSegmentHeader;

typedef struct SysconUpdateChecksumPacket {
	SysconUpdatePacketHeader header;
	uint8_t digest[20];
} SysconUpdateChecksumPacket;

typedef union SysconUpdatePacket {
	SysconUpdatePacketHeader header;
	SysconUpdateVersionPacket version;
	SysconUpdateKeyRevisionPacket keyRevision;
	SysconUpdateSignaturePacket signature;
	SysconUpdateSegmentHeader segment;
	SysconUpdateChecksumPacket checksum;
} SysconUpdatePacket;

_Static_assert(sizeof(SysconUpdatePacketHeader) == 4, "Syscon packet header");
_Static_assert(sizeof(SysconUpdateVersionPacket) == 16, "Syscon version packet");
_Static_assert(sizeof(SysconUpdateKeyRevisionPacket) == 16, "Syscon revision packet");
_Static_assert(sizeof(SysconUpdateSignaturePacket) == 32, "Syscon signature packet");
_Static_assert(sizeof(SysconUpdateSegmentHeader) == 16, "Syscon segment header");
_Static_assert(sizeof(SysconUpdateChecksumPacket) == 24, "Syscon checksum packet");

#endif
