#pragma once

#include <stdint.h>

/* Type2 responses carry a format and one flag nibble per packed sample. */
typedef struct MotionSensorSamples {
	uint8_t format;
	uint8_t timestampWraps;
	uint16_t sequence;
	uint32_t sampleFlags;
	uint8_t samples[46];
} MotionSensorSamples;

/* Type133 reports firmware/hardware identity and two validation words. */
typedef struct MotionSensorIdentity {
	uint8_t kind;
	uint8_t hardwareInfoLow;
	uint8_t hardwareInfoHigh;
	uint8_t flags;
	uint16_t protocolVersion;
	uint16_t firmwareVersion;
	uint32_t descriptorWords[3];
	uint8_t descriptorBytes[2];
	uint16_t identityCheck[2];
	uint8_t extension[2];
} MotionSensorIdentity;

typedef struct MotionSensorDeviceInfo {
	uint16_t firmwareVersion;
	uint16_t hardwareInfo;
	uint32_t deviceMetadata;
	uint16_t protocolVersion;
	uint8_t attributes[4];
	uint16_t reserved;
	uint16_t descriptor;
	union {
		uint16_t legacy[4];
		struct {
			uint8_t attributes[4];
			uint16_t values[6];
			uint8_t extension;
			uint8_t reserved;
		} current;
	} configuration;
} MotionSensorDeviceInfo;

typedef struct MotionSensorBlock {
	uint8_t block;
	uint8_t count;
	uint16_t size;
	uint16_t offset;
	uint8_t data[48];
} MotionSensorBlock;

typedef struct SceMotionDevUpdateStartTransferRequest {
	uint8_t headerSize;
	uint8_t hwInfo;
	uint16_t dataVersion;
	union {
		uint32_t dataSize;
		uint8_t dataSizeBytes[4];
	};
	union {
		uint32_t address;
		uint8_t addressBytes[4];
	};
	uint16_t check_sum;
	uint8_t dataType;
	uint8_t terminalByte;
} SceMotionDevUpdateStartTransferRequest;

typedef struct MotionSensorProgramTransfer {
	uint16_t offset;
	uint8_t programData[32];
	uint8_t trailer[2];
} MotionSensorProgramTransfer;

typedef struct MotionSensorBlockWrite {
	uint16_t size;
	uint16_t offset;
	uint8_t block;
	uint8_t count;
	uint8_t data[48];
} MotionSensorBlockWrite;

/* Type134 descriptor: two byte tags, one word and two extension bytes. */
typedef struct MotionSensorDescriptor {
	uint8_t kind;
	uint8_t attributes;
	uint16_t value;
	uint8_t extension[2];
} MotionSensorDescriptor;

typedef struct MotionSensorResponse {
	uint8_t type;
	uint8_t size;
	uint16_t reserved;
	union {
		MotionSensorSamples sample;
		struct {
			uint8_t block;
			uint8_t mode;
		} readBlock;
		uint8_t control;
		uint8_t resetCommand;
		MotionSensorIdentity identity;
		MotionSensorDeviceInfo deviceInfo;
		MotionSensorBlock block;
		MotionSensorBlockWrite blockWrite;
		MotionSensorDescriptor descriptor;
		SceMotionDevUpdateStartTransferRequest startTransfer;
		MotionSensorProgramTransfer programTransfer;
		uint8_t bytes[56];
	} payload;
} MotionSensorResponse;

_Static_assert(sizeof(MotionSensorResponse) == 60, "decoded sensor record size");
_Static_assert(sizeof(SceMotionDevUpdateStartTransferRequest) == 16,
	"sensor update start payload size");
_Static_assert(sizeof(MotionSensorProgramTransfer) == 36,
	"sensor update data payload size");
_Static_assert(sizeof(MotionSensorBlockWrite) == 54,
	"sensor block write payload size");
_Static_assert(sizeof(MotionSensorDescriptor) == 6,
	"sensor descriptor payload size");
_Static_assert(sizeof(MotionSensorIdentity) == 28, "sensor identity payload size");
_Static_assert(sizeof(MotionSensorDeviceInfo) == 36, "sensor device-info payload size");

int SceMotionDevForDriver_09918DAE(
	void (*completion)(int, const MotionSensorResponse *,
		MotionSensorResponse *, void *),
	const MotionSensorResponse *request, MotionSensorResponse *response,
	void *arg);
