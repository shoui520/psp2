#include <string.h>
#include "spkg_peripherals.h"
#include "touch_update.h"
#include "touch_dispatch.h"

int UpdateUtilUpdateNodeStatus(unsigned int nodeType, UpdateUtilNodeStatus *status);

int TouchUtilWriteFirmware(uint64_t hardwareInfo, const void *buffer,
	uint64_t size, uint32_t version, unsigned int nodeType,
	UpdateUtilNodeStatus *status)
{
	union {
		TouchUtilFirmwareRecord record;
		struct touchDeviceInfo deviceInfo[2];
	} work;
	struct touchDeviceInfo deviceInfo[2];
	uint16_t sysconInfo[4];
	uint8_t header[6];
	uint64_t position = 0;
	uint64_t remaining;
	uint32_t legacyState = 0, commandState = 0;
	uint32_t commandResult;
	const TouchUtilPanelOperations *operations;
	const TouchUtilLegacyFirmwareRecord *binaryRecord;
	const uint8_t *payload;
	unsigned int format, port, panels, block, offset, recordSize;
	unsigned int chunkSize, chunkCount, chunk, failure, subtype;
	int result;

	format = (hardwareInfo >> 60) << 12;
	if (format != 0x4000 && format != 0x8000)
		return (int)0x800f0225;
	if (!updateUseTouchService)
		return (int)0x800f0202;
	port = (uint32_t)hardwareInfo & 15;
	if (port == 0)
		panels = 1;
	else if (port == 1)
		panels = 2;
	else
		return (int)0x800f0216;
	if (status) {
		status->progress = 3;
		status->written_rates = 0;
		status->seq_no++;
		UpdateUtilUpdateNodeStatus(nodeType, status);
	}
	if (SceTouchForDriver_DDCF5145()) {
		updateLastResult = (port << 16) | 0x0a001003;
		return (int)0x800f0229;
	}
	operations = &touchPanelOperations[port];
	if (format == 0x4000) {
		result = operations->getFirmwareUpdateState(&commandState);
		if (!result && commandState) {
			result = operations->beginFirmwareUpdate();
			if (result) {
				failure = 0x0a00101d;
				goto commandFailure;
			}
		}
		position = 0;
		result = TouchUtilReadFirmwareHeader(buffer, &position, header);
		if (result)
			goto leaveUpdate;
		while (position < size) {
			result = TouchUtilReadFirmwareRecord(buffer, &position, &work.record);
			if (result)
				goto leaveUpdate;
			if (work.record.blockIndex != 511)
				continue;
			if (work.record.dataSize & 31) {
				result = (int)0x800f0216;
				goto leaveUpdate;
			}
			chunkCount = work.record.dataSize >> 5;
			payload = work.record.payload;
			for (chunk = 0; chunk < chunkCount; chunk++) {
				result = operations->writeFirmwareChunk(work.record.blockIndex,
					chunk * 32, payload, 32, &commandState);
				if (result) {
					failure = 0x0a00101f;
					goto commandFailure;
				}
				payload += 32;
			}
			result = operations->checkFirmwareBlock(work.record.blockIndex,
				&commandState);
			if (result || commandState) {
				failure = 0x0a001020;
				goto commandFailure;
			}
			break;
		}
		position = 0;
		result = TouchUtilReadFirmwareHeader(buffer, &position, header);
		if (result)
			goto leaveUpdate;
		while (position < size) {
			result = TouchUtilReadFirmwareRecord(buffer, &position, &work.record);
			if (result)
				goto leaveUpdate;
			if (work.record.blockIndex > 510)
				break;
			if (work.record.dataSize & 31) {
				result = (int)0x800f0216;
				goto leaveUpdate;
			}
			chunkCount = work.record.dataSize >> 5;
			payload = work.record.payload;
			for (chunk = 0; chunk < chunkCount; chunk++) {
				result = operations->writeFirmwareChunk(work.record.blockIndex,
					chunk * 32, payload, 32, &commandState);
				if (result || commandState) {
					failure = 0x0a00101f;
					goto commandFailure;
				}
				payload += 32;
			}
			result = operations->commitFirmwareBlock(work.record.blockIndex,
				&commandState);
			if (result || commandState) {
				failure = 0x0a001021;
				goto commandFailure;
			}
			if (status) {
				status->progress = 3;
				status->seq_no++;
				status->written_rates = 100 - 100 * (uint32_t)(position / size);
				UpdateUtilUpdateNodeStatus(nodeType, status);
			}
		}
		result = operations->finishFirmwareUpdate(&commandState);
		if (result || commandState) {
			failure = 0x0a001022;
			goto commandFailure;
		}
		if (status) {
			status->progress = 3;
			status->written_rates = 100;
			status->seq_no++;
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
		result = SceTouchForDriver_4E6C4558(panels);
		if (result) {
			failure = 0x0a00100b;
			goto commandFailure;
		}
		result = SceSysconForDriver_F492E69E(sysconInfo);
		if (result) {
			failure = 0x0a00100c;
			goto deviceFailure;
		}
		__builtin_memcpy(updateSysconTouchDeviceInfo, sysconInfo, sizeof(sysconInfo));
		memset(deviceInfo, 0, sizeof(deviceInfo));
		result = ksceTouchGetDeviceInfo(port, &deviceInfo[port]);
		if (!result)
			updateTouchDeviceInfo[port] = deviceInfo[port];
		goto leaveUpdate;
	}

	if ((updateSysconTouchDeviceInfo[port * 2] & 0xc0c0) != 0xc0c0) {
		result = operations->readLegacyFirmwareStatus(&legacyState);
		if (result) {
			failure = 0x0a001004;
			goto commandFailure;
		}
		if (!(legacyState & 0xc0)) {
			failure = 0x0a001005;
			goto commandFailure;
		}
	}
	result = operations->beginLegacyFirmwareUpdate(&legacyState);
	if (result) {
		failure = 0x0a001006;
		goto commandFailure;
	}
	if (!(legacyState & 0x80)) {
		failure = 0x0a001007;
		goto commandFailure;
	}
	remaining = size;
	block = 0;
	offset = 0;
	while (remaining) {
		binaryRecord = (const TouchUtilLegacyFirmwareRecord *)
			((const uint8_t *)buffer + offset);
		recordSize = ((unsigned int)binaryRecord->payloadSizeBigEndian[0] << 8) +
			binaryRecord->payloadSizeBigEndian[1] + 2;
		if (recordSize > 278) {
			result = (int)0x800f0224;
			goto leaveUpdate;
		}
		payload = (const uint8_t *)binaryRecord;
		chunkCount = recordSize;
		chunk = 0;
		commandResult = 0;
		do {
			chunkSize = chunkCount >= 20 ? 20 : chunkCount;
			result = operations->writeLegacyFirmwareChunk(block, chunk,
				payload + chunk, chunkSize, &commandResult);
			if (result) {
				failure = 0x0a001008;
				goto commandFailure;
			}
			if (!(commandResult & 0x80)) {
				failure = 0x0a001007;
				goto commandFailure;
			}
			chunkCount -= chunkSize;
			chunk += chunkSize;
		} while (chunkCount);
		result = operations->commitLegacyFirmwareBlock(block, &commandResult);
		if (result) {
			failure = 0x0a001009;
			goto commandFailure;
		}
		if (!(commandResult & 4)) {
			failure = 0x0a00100a;
			goto commandFailure;
		}
		if (status) {
			status->progress = 3;
			status->seq_no++;
			status->written_rates = 100 - 100 * (uint32_t)(remaining / size);
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
		block = (uint16_t)(block + 1);
		remaining -= recordSize;
		offset += recordSize;
	}
	result = SceTouchForDriver_4E6C4558(panels);
	if (result) {
		failure = 0x0a00100b;
		goto commandFailure;
	}
	result = SceSysconForDriver_F492E69E(sysconInfo);
	if (result) {
		failure = 0x0a00100c;
		goto deviceFailure;
	}
	__builtin_memcpy(updateSysconTouchDeviceInfo, sysconInfo, sizeof(sysconInfo));
	memset(work.deviceInfo, 0, sizeof(work.deviceInfo));
	result = ksceTouchGetDeviceInfo(port, &work.deviceInfo[port]);
	if (!result)
		updateTouchDeviceInfo[port] = work.deviceInfo[port];
	goto leaveUpdate;

deviceFailure:
	subtype = (result & 0x80250200) == 0x80250200 ? (uint8_t)(result * 2) : 0;
	updateLastResult = failure | ((subtype | port) << 16);
	goto leaveUpdate;

commandFailure:
	subtype = (result & 0x80250200) == 0x80250200 ? (uint8_t)(result * 2) : 0;
	updateLastResult = failure | ((subtype | port) << 16);
	result = (int)0x800f0205;
leaveUpdate:
	SceTouchForDriver_423F26D3();
	return result;
}
