#pragma once

#include <stdint.h>
#include <touch/touch.h>

int TouchUtilReadHexByte(const int8_t *buffer, uint64_t *position);
int UpdateUtilStringToInt(const int8_t *text, int unused, int base);
int TouchUtilGetConfigUpdateFlags(int port, unsigned int hardwareInfo,
	uint8_t *flags);
extern int updateTouchRecoveryMode;

typedef struct TouchUtilLegacyFirmwareRecord {
	uint8_t payloadSizeBigEndian[2];
	uint8_t payload[];
} TouchUtilLegacyFirmwareRecord;

_Static_assert(sizeof(TouchUtilLegacyFirmwareRecord) == 2,
	"legacy touch firmware record header size");


typedef struct TouchUtilLegacyConfigRecord {
	uint16_t address;
	uint8_t value;
} __attribute__((packed)) TouchUtilLegacyConfigRecord;

typedef struct TouchUtilConfigRecord {
	uint8_t marker;
	uint8_t dataSize;
	uint16_t address;
	uint8_t recordTag;
	uint8_t payload[64];
	uint8_t checksum;
} TouchUtilConfigRecord;

_Static_assert(sizeof(TouchUtilLegacyConfigRecord) == 3,
	"legacy touch configuration record size");
_Static_assert(sizeof(TouchUtilConfigRecord) == 70,
	"touch configuration decoded record size");
_Static_assert(__builtin_offsetof(TouchUtilConfigRecord, checksum) == 69,
	"touch configuration checksum position");

typedef struct TouchUtilFirmwareTextHeader {
	uint8_t encodedBytes[12];
	uint8_t carriageReturn;
	uint8_t lineFeed;
} TouchUtilFirmwareTextHeader;

_Static_assert(sizeof(TouchUtilFirmwareTextHeader) == 14,
	"touch firmware text header size");

int TouchUtilReadFirmwareHeader(const int8_t *buffer, uint64_t *position,
	uint8_t header[6]);

typedef struct TouchUtilFirmwareTextRecord {
	int8_t marker;
	int8_t recordTag[2];
	int8_t blockIndex[4];
	int8_t dataSize[4];
	int8_t payload[256];
	int8_t checksum[2];
	int8_t carriageReturn;
	int8_t lineFeed;
} TouchUtilFirmwareTextRecord;

typedef struct TouchUtilFirmwareRecord {
	uint8_t marker;
	uint8_t recordTag;
	uint16_t blockIndex;
	uint16_t dataSize;
	uint8_t payload[128];
	uint8_t checksum;
} TouchUtilFirmwareRecord;

_Static_assert(sizeof(TouchUtilFirmwareTextRecord) == 271,
	"touch firmware text record size");
_Static_assert(sizeof(TouchUtilFirmwareRecord) == 136,
	"touch firmware decoded record size");
_Static_assert(__builtin_offsetof(TouchUtilFirmwareRecord, payload) == 6,
	"touch firmware payload position");

int TouchUtilReadFirmwareRecord(const int8_t *buffer, uint64_t *position,
	TouchUtilFirmwareRecord *record);
