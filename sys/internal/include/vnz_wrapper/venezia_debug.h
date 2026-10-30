/* SceCodecEngineWrapper host capture interface, shared with SceAvcodec.
 * Layouts read in both TOOL originals: BB388700 dispatch 162f4/14fc8,
 * snapshot collection 860c/7364, video input e664/d338 and e7c4/d498.
 * Descriptive names retain the provider's reviewed naming provenance.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>

typedef struct VeneziaDebugTraceEvent {
	uint32_t marked;
	uint32_t value;
} VeneziaDebugTraceEvent;

typedef struct VeneziaDebugHalfwordPair {
	uint16_t high;
	uint16_t low;
} VeneziaDebugHalfwordPair;

/* PLACEHOLDER: individual DSP instruction/control field meanings are unknown. */
typedef struct VeneziaDebugContextInfo {
	uint32_t header;
	VeneziaDebugHalfwordPair instruction;
	uint16_t controlHigh, controlLow, controlZero, controlExtended;
	uint16_t controlZeros[2];
	VeneziaDebugHalfwordPair result;
	uint8_t executionMode, instructionWidth, flagA, flagB, flagC, captureMode;
	VeneziaDebugHalfwordPair accumulator;
	uint16_t accumulatorZero;
	VeneziaDebugHalfwordPair address, operand;
	uint16_t executionClass;
	VeneziaDebugHalfwordPair source, destination;
	uint16_t captureFlags;
	uint8_t headerMode, headerGroup;
} VeneziaDebugContextInfo;

typedef struct VeneziaDebugParameterInfo {
	uint32_t header;
	uint32_t values[4][3];
	uint16_t status;
	uint16_t reserved;
	uint8_t enabled;
	uint8_t mode;
} VeneziaDebugParameterInfo;

typedef struct VeneziaDebugParameterState {
	uint16_t status;
	uint8_t flags[2];
	uint8_t mode;
} VeneziaDebugParameterState;

typedef struct VeneziaDebugRegisterBank {
	uint32_t low[32];
	uint32_t high[32];
	uint8_t valid[32];
	uint8_t control[3];
	uint8_t selectors[4];
	uint8_t reserved;
} VeneziaDebugRegisterBank;

typedef struct VeneziaDebugRegisterInfo {
	VeneziaDebugRegisterBank banks[2];
	uint32_t programCounter;
	uint32_t stackPointer;
	VeneziaDebugHalfwordPair executionStatus;
	uint8_t control[16];
	uint8_t bankEnabled[2];
	uint8_t executionFlags[2];
	uint8_t indexControl[8];
} VeneziaDebugRegisterInfo;

typedef struct VeneziaDebugVectorRegisters {
	uint32_t flags;
	struct {
		uint32_t low[32];
		uint32_t high[32];
	} banks[2];
} VeneziaDebugVectorRegisters;

typedef struct VeneziaDebugChannelState {
	uint32_t header[2];
	uint8_t status[4];
	uint8_t control[13][3];
	uint8_t reserved;
	uint32_t parameters[3];
} VeneziaDebugChannelState;

/* name unknown: host snapshots assembled from the hardware capture decoders. */
typedef struct VeneziaDebugSnapshotData {
	VeneziaDebugRegisterInfo registers;
	VeneziaDebugVectorRegisters vectorRegisters;
	VeneziaDebugChannelState channels[2];
	VeneziaDebugParameterState parameterState;
	VeneziaDebugParameterInfo parameters[2];
	VeneziaDebugTraceEvent trace;
} VeneziaDebugSnapshotData;

typedef struct VeneziaDebugSnapshot {
	VeneziaDebugContextInfo context;
	VeneziaDebugSnapshotData data;
} VeneziaDebugSnapshot;

typedef struct VeneziaDebugSnapshotOutput {
	uint32_t virtualAddress;
	uint32_t physicalAddress;
	VeneziaDebugContextInfo context;
	uint32_t valid;
	uint8_t event;
	VeneziaDebugSnapshotData data;
} VeneziaDebugSnapshotOutput;

typedef struct VeneziaDebugSnapshotList {
	uint32_t count;
	VeneziaDebugSnapshotOutput **snapshots;
} VeneziaDebugSnapshotList;

typedef struct VeneziaVipInputBuffer {
	const void *data;
	SceSize size;
} VeneziaVipInputBuffer;

typedef struct VeneziaVipVideoInput {
	uint64_t presentationTimestamp;
	uint64_t decodingTimestamp;
	VeneziaVipInputBuffer additionalData[2];
	VeneziaVipInputBuffer payload;
	unsigned int nalLengthSize;
	uint32_t flags;
} VeneziaVipVideoInput;

_Static_assert(sizeof(VeneziaDebugHalfwordPair) == 4, "Venezia halfword pair");
_Static_assert(sizeof(VeneziaDebugContextInfo) == 60, "Venezia context info");
_Static_assert(sizeof(VeneziaDebugRegisterBank) == 296, "Venezia register bank");
_Static_assert(sizeof(VeneziaDebugRegisterInfo) == 632, "Venezia register info");
_Static_assert(sizeof(VeneziaDebugVectorRegisters) == 516, "Venezia vector registers");
_Static_assert(sizeof(VeneziaDebugChannelState) == 64, "Venezia channel state");
_Static_assert(sizeof(VeneziaDebugParameterState) == 6, "Venezia parameter state");
_Static_assert(sizeof(VeneziaDebugParameterInfo) == 60, "Venezia parameter info");
_Static_assert(sizeof(VeneziaDebugTraceEvent) == 8, "Venezia trace event");
_Static_assert(sizeof(VeneziaDebugSnapshotData) == 1412, "Venezia snapshot data");
_Static_assert(sizeof(VeneziaDebugSnapshot) == 1472, "Venezia snapshot");
_Static_assert(sizeof(VeneziaDebugSnapshotOutput) == 1488, "Venezia output snapshot");
_Static_assert(sizeof(VeneziaDebugSnapshotList) == 8, "Venezia snapshot list");
_Static_assert(sizeof(VeneziaVipInputBuffer) == 8, "Venezia input buffer");
_Static_assert(sizeof(VeneziaVipVideoInput) == 48, "Venezia video input");
_Static_assert(offsetof(VeneziaDebugSnapshotOutput, context) == 8, "Venezia output context");
_Static_assert(offsetof(VeneziaDebugSnapshotOutput, valid) == 68, "Venezia output validity");
_Static_assert(offsetof(VeneziaDebugSnapshotOutput, event) == 72, "Venezia output event");
_Static_assert(offsetof(VeneziaDebugSnapshotOutput, data) == 76, "Venezia output data");
_Static_assert(offsetof(VeneziaDebugSnapshotData, vectorRegisters) == 632, "Venezia vectors");
_Static_assert(offsetof(VeneziaDebugSnapshotData, channels) == 1148, "Venezia channels");
_Static_assert(offsetof(VeneziaDebugSnapshotData, parameterState) == 1276, "Venezia parameter state offset");
_Static_assert(offsetof(VeneziaDebugSnapshotData, parameters) == 1284, "Venezia parameters");
_Static_assert(offsetof(VeneziaDebugSnapshotData, trace) == 1404, "Venezia trace");
_Static_assert(offsetof(VeneziaVipVideoInput, decodingTimestamp) == 8, "Venezia decoding timestamp");
_Static_assert(offsetof(VeneziaVipVideoInput, additionalData) == 16, "Venezia additional input");
_Static_assert(offsetof(VeneziaVipVideoInput, payload) == 32, "Venezia payload");
_Static_assert(offsetof(VeneziaVipVideoInput, nalLengthSize) == 40, "Venezia NAL length size");
_Static_assert(offsetof(VeneziaVipVideoInput, flags) == 44, "Venezia video flags");

typedef struct VeneziaDebugImageGeometry {
	uint32_t width;
	uint32_t height;
	uint32_t imageCount;
	uint32_t processorCount;
} VeneziaDebugImageGeometry;

typedef struct VeneziaDebugImageFormat {
	uint32_t width;
	uint32_t height;
	uint32_t imageCount;
} VeneziaDebugImageFormat;

typedef struct VeneziaVipCaptureParam {
	uint32_t imageVirtualAddress;
	SceSize imageSize;
	uint32_t storageVirtualAddress;
	SceSize storageSize;
	const VeneziaDebugImageFormat *imageFormat;
	const VeneziaDebugImageGeometry *geometry;
} VeneziaVipCaptureParam;

int SceCodecEngineWrapperForDriver_2B74706E(const VeneziaDebugImageFormat *geometry,
	uint32_t *size);
int SceCodecEngineWrapperForDriver_9D3B1425(unsigned int *processor,
	const VeneziaVipCaptureParam *param);
int SceCodecEngineWrapperForDriver_8CD35820(const VeneziaDebugImageGeometry *geometry, SceSize *size);
int SceCodecEngineWrapperForDriver_86861877(unsigned int flags, int wait, SceUID pid);

int SceCodecEngineWrapperForDriver_BB388700(unsigned int processor,
	const VeneziaVipVideoInput *input, VeneziaDebugSnapshotList *list,
	unsigned int *completed);
