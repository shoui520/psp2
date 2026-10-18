#pragma once

#include <scetypes.h>
#include <motion/motion_command.h>
#include <motion/motion_dev.h>

typedef struct MotionUtilEventContext {
	SceUID eventFlagId;
	int result;
} MotionUtilEventContext;

_Static_assert(sizeof(MotionUtilEventContext) == 8,
	"motion update event context size");

extern MotionUtilEventContext updateMotionEvent;
extern MotionSensorResponse updateMotionRequest;
extern MotionSensorResponse updateMotionResponse;

int MotionUtilSendCommand(void);

/* Serialized firmware header; multi-byte fields are unaligned on the wire. */
typedef struct __attribute__((packed)) MotionUtilFirmwareHeader {
	uint8_t headerSize;
	uint8_t hwInfo;
	uint8_t dataType;
	uint16_t dataVersion;
	uint32_t dataSize;
	uint32_t address;
	uint16_t check_sum;
	uint8_t terminalByte;
} MotionUtilFirmwareHeader;

_Static_assert(sizeof(MotionUtilFirmwareHeader) == 16,
	"motion firmware header size");
_Static_assert(__builtin_offsetof(MotionUtilFirmwareHeader, dataSize) == 5,
	"motion firmware size position");

extern const unsigned int motionFlashChunkSizes[2];

void MotionUtilCommandComplete(int result,
	const struct MotionSensorResponse *request,
	struct MotionSensorResponse *response, void *arg);
