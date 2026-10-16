#include "update_internal.h"
#include "spkg_peripherals.h"
#include "motion_update.h"
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <string.h>

int UpdateUtilUpdateNodeStatus(unsigned int nodeType,
	UpdateUtilNodeStatus *status);

int MotionUtilWriteFirmware(uint64_t hardwareInfo, const void *buffer,
	uint64_t size, uint32_t version, unsigned int nodeType,
	UpdateUtilNodeStatus *status)
{
	const MotionUtilFirmwareHeader *header = buffer;
	const uint8_t *program;
	unsigned int hardwareType = (unsigned int)(hardwareInfo >> 32) & 0xf0;
	unsigned int kind, chunkSize, block = 1;
	uint64_t remaining = size - sizeof(*header);
	SceUID eventFlag;
	int result;
	uint8_t dataType, hwInfo;
	uint16_t dataVersion, check_sum;
	uint32_t dataSize, address;

	if (!updateMotionConnected)
		return (int)0x800f0202;
	if (hardwareType == 0x10 || hardwareType == 0x20)
		kind = 1;
	else if (hardwareType == 0)
		kind = 0;
	else
		return (int)0x800f0202;
	if (remaining % motionFlashChunkSizes[kind])
		return (int)0x800f0224;
	if (!size || !buffer || !status)
		return (int)0x800f0216;
	if (hardwareType == 0x10 || hardwareType == 0x20)
		kind = 1;
	else if (hardwareType != 0)
		return (int)0x800f0202;
	status->progress = 3;
	status->written_rates = 0;
	status->seq_no++;
	chunkSize = motionFlashChunkSizes[kind];
	UpdateUtilUpdateNodeStatus(nodeType, status);
	eventFlag = ksceKernelCreateEventFlag("SceSblUsUpdateMgr", 0, 0, NULL);
	if (eventFlag < 0)
		return (int)0x800f020c;
	updateMotionEvent.eventFlagId = eventFlag;
	updateMotionEvent.result = 0;
	result = SceMotionDevForDriver_FD1C528D();
	if (result) {
		result = (int)0x800f0205;
		goto out;
	}
	memset(&updateMotionRequest, 0, sizeof(updateMotionRequest));
	memset(&updateMotionResponse, 0, sizeof(updateMotionResponse));
	updateMotionRequest.type = 0x80;
	updateMotionRequest.size = 2;
	result = MotionUtilSendCommand();
	if (result)
		goto out;
	dataType = header->dataType;
	dataVersion = header->dataVersion;
	check_sum = header->check_sum;
	dataSize = header->dataSize;
	address = header->address;
	hwInfo = header->hwInfo;
	memset(&updateMotionRequest, 0, sizeof(updateMotionRequest));
	memset(&updateMotionResponse, 0, sizeof(updateMotionResponse));
	updateMotionRequest.payload.startTransfer.dataType = dataType;
	updateMotionRequest.payload.startTransfer.hwInfo = hwInfo;
	updateMotionRequest.payload.startTransfer.dataVersion = dataVersion;
	updateMotionRequest.payload.startTransfer.dataSize = dataSize;
	updateMotionRequest.type = 0x81;
	updateMotionRequest.size = 18;
	updateMotionRequest.payload.startTransfer.address = address;
	updateMotionRequest.payload.startTransfer.headerSize = 16;
	updateMotionRequest.payload.startTransfer.check_sum = check_sum;
	result = MotionUtilSendCommand();
	if (result)
		goto out;
	program = (const uint8_t *)(header + 1);
	while (remaining) {
		memset(&updateMotionRequest, 0, sizeof(updateMotionRequest));
		memset(&updateMotionResponse, 0, sizeof(updateMotionResponse));
		updateMotionRequest.type = 0x82;
		updateMotionRequest.size = 38;
		__builtin_memcpy(updateMotionRequest.payload.programTransfer.programData,
			program, 32);
		program += 32;
		updateMotionRequest.payload.programTransfer.offset = block;
		result = MotionUtilSendCommand();
		if (result)
			goto out;
		if (block % (chunkSize >> 5) == 0) {
			memset(&updateMotionRequest, 0, sizeof(updateMotionRequest));
			memset(&updateMotionResponse, 0, sizeof(updateMotionResponse));
			updateMotionRequest.type = 0x83;
			updateMotionRequest.size = 2;
			result = MotionUtilSendCommand();
			if (result)
				goto out;
			status->progress = 3;
			status->seq_no++;
			status->written_rates = 100 -
				100 * (remaining / (size - sizeof(*header)));
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
		remaining -= 32;
		block++;
	}
	memset(&updateMotionRequest, 0, sizeof(updateMotionRequest));
	memset(&updateMotionResponse, 0, sizeof(updateMotionResponse));
	updateMotionRequest.type = 0x84;
	updateMotionRequest.size = 2;
	result = MotionUtilSendCommand();
out:
	if (eventFlag)
		ksceKernelDeleteEventFlag(eventFlag);
	if (!result) {
		if (SceMotionDevForDriver_A75976EE())
			return (int)0x800f0205;
		return 0;
	}
	return result;
}
