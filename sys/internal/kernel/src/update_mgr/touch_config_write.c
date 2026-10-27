#include <string.h>
#include "spkg_peripherals.h"
#include "touch_update.h"
#include "touch_dispatch.h"

int TouchUtilWriteConfig(uint64_t hardwareInfo, const void *buffer,
	uint64_t size, uint32_t version, unsigned int nodeType,
	UpdateUtilNodeStatus *status)
{
	TouchUtilConfigRecord record;
	const TouchUtilLegacyConfigRecord *legacyRecord;
	const TouchUtilPanelOperations *operations;
	const int8_t *text = buffer;
	uint64_t position, remaining;
	uint16_t sysconInfo[4];
	uint16_t first = 0, second = 0;
	union {
		uint32_t count;
		uint8_t value;
	} output;
	uint32_t commandState = 0;
	unsigned int format, port, panels, panelHardware, panelVersion;
	unsigned int offset, chunk, address, count, pending, failure, subtype;
	unsigned int expectedVersion, retry;
	uint16_t high;
	int result;

	format = (hardwareInfo >> 60) << 12;
	if (format != 0x4000 && format != 0x8000)
		return (int)0x800f0225;
	if (!updateUseTouchService)
		return (int)0x800f0202;
	if (format == 0x8000 && size % 3)
		return (int)0x800f0216;
	port = (uint32_t)hardwareInfo & 15;
	if (port > 1)
		return (int)0x800f0216;
	panels = port ? 2 : 1;
	panelHardware = updateSysconTouchDeviceInfo[port * 2];
	panelVersion = updateSysconTouchDeviceInfo[port * 2 + 1];
	if ((panelHardware & 0xf000) == 0x8000)
		panelVersion >>= 8;
	if (status) {
		status->progress = 3;
		status->written_rates = 0;
		status->seq_no++;
		UpdateUtilUpdateNodeStatus(nodeType, status);
	}
	result = sceTouchUpdateInit();
	if (result)
		goto enterFailure;
	operations = &touchPanelOperations[port];
	if (format == 0x4000) {
		result = operations->beginConfigUpdate();
		if (result)
			goto enterFailure;
		expectedVersion = 0;
		retry = 2;
		for (;;) {
			pending = 1042;
			position = 0;
			result = 0;
			while (position < size) {
				record.marker = text[(uint32_t)position++];
				if (record.marker != ':') {
					failure = 0x0a00201a;
					goto invalidRecord;
				}
				record.dataSize = TouchUtilReadHexByte(text, &position);
				if (record.dataSize > 64) {
					failure = 0x0a00201a;
					goto invalidRecord;
				}
				high = (uint16_t)(TouchUtilReadHexByte(text, &position) << 8);
				record.address = high;
				record.address = high | TouchUtilReadHexByte(text, &position);
				record.recordTag = TouchUtilReadHexByte(text, &position);
				for (offset = 0; offset < record.dataSize; offset++)
					record.payload[offset] = TouchUtilReadHexByte(text, &position);
				record.checksum = TouchUtilReadHexByte(text, &position);
				if (text[(uint32_t)position] != '\r' ||
				    text[(uint32_t)position + 1] != '\n') {
					failure = 0x0a00201a;
					goto invalidRecord;
				}
				position += 2;
				if ((uint16_t)(record.address + 0x0f00) > 0x411) {
					if (record.address <= 0xf511)
						continue;
					goto leaveUpdate;
				}
				for (chunk = 0; chunk < record.dataSize >> 5; chunk++) {
					count = pending <= 32 ? pending : 32;
					address = record.address + 32 * chunk;
					if (address <= 0xf510 && address + count > 0xf510) {
						offset = 0xf510 - address;
						if (offset > 62) {
							failure = 0x0a00201b;
							goto invalidRecord;
						}
						expectedVersion = record.payload[32 * chunk + offset] |
							(record.payload[32 * chunk + offset + 1] << 8);
					}
					result = operations->writeConfigBlock(0, address - 0xf100,
						count, record.payload + 32 * chunk, &output.count);
					if (result) {
						if (retry + 1 <= 1) {
							failure = 0x0a00201b;
							goto configFailure;
						}
						goto retryText;
					}
					if (status) {
						status->seq_no++;
						status->progress = 3;
						status->written_rates = 66 - 33 * retry +
							(100 - 100 * (pending / 1042)) / 3;
						UpdateUtilUpdateNodeStatus(nodeType, status);
					}
					pending -= output.count;
					if (!pending)
						goto checkText;
				}
				result = 0;
			}
			goto leaveUpdate;
checkText:
			result = operations->getConfigBlockInfo(0, &commandState,
				&first, &second);
			if (result) {
				if (retry + 1 <= 1) {
					failure = 0x0a00201c;
					goto configFailure;
				}
				goto retryText;
			}
			if (!commandState && second == expectedVersion)
				break;
			if (retry + 1 <= 1) {
				updateLastResult = (port << 16) | 0x0a00201c;
				result = (int)0x800f0229;
				goto leaveUpdate;
			}
retryText:
			retry--;
		}
	} else {
		retry = 3;
		remaining = size;
		offset = 0;
		for (;;) {
			while (remaining) {
				legacyRecord = (const TouchUtilLegacyConfigRecord *)
					((const uint8_t *)buffer + offset);
				result = operations->writeConfigData(legacyRecord->address,
					&legacyRecord->value, 1);
				if (result) {
					failure = 0x0a002010;
					goto commandFailure;
				}
				if (status) {
					status->progress = 3;
					status->seq_no++;
					status->written_rates = 99 - 33 * retry +
						(100 - 100 * (uint32_t)(remaining / size)) / 3u;
					UpdateUtilUpdateNodeStatus(nodeType, status);
				}
				offset += 3;
				remaining -= 3;
			}
			result = operations->readLegacyConfigStatus(&commandState);
			if (result) {
				failure = 0x0a002012;
				goto commandFailure;
			}
			if (commandState == 255) {
				failure = 0x0a002013;
				goto commandFailure;
			}
			result = sceTouchUpdateRestart(panels);
			if (result) {
				failure = 0x0a00200b;
				goto commandFailure;
			}
			result = sceSysconGetTouchpanelDeviceInfo(sysconInfo);
			if (result) {
				failure = 0x0a00200c;
				goto deviceFailure;
			}
			__builtin_memcpy(updateSysconTouchDeviceInfo, sysconInfo,
				sizeof(sysconInfo));
			position = 0;
			legacyRecord = buffer;
			while (position != size) {
				result = operations->readConfigData(legacyRecord->address,
					&output.count, 1);
				if (result) {
					failure = 0x0a002011;
					goto commandFailure;
				}
				if ((panelVersion != 36 ||
				     (unsigned int)legacyRecord->address - 405 > 7) &&
				    legacyRecord->value != output.value) {
					failure = 0x0a002014;
					goto configFailure;
				}
				legacyRecord++;
				position += 3;
			}
			result = TouchUtilGetConfigUpdateFlags(port, panelHardware,
				&output.value);
			if (result) {
				failure = 0x0a002011;
				goto commandFailure;
			}
			if (!(output.count & 1))
				break;
			if (retry <= 1) {
				failure = 0x0a002015;
				goto configFailure;
			}
			retry--;
		}
		result = 0;
	}
	if (status) {
		status->progress = 3;
		status->written_rates = 100;
		status->seq_no++;
		UpdateUtilUpdateNodeStatus(nodeType, status);
	}
	goto leaveUpdate;

enterFailure:
	updateLastResult = (port << 16) | 0x0a002003;
	return (int)0x800f0229;
invalidRecord:
	updateLastResult = failure | (port << 16);
	result = (int)0x800f0216;
	goto leaveUpdate;
commandFailure:
	subtype = (result & 0x80250200) == 0x80250200 ? (uint8_t)(result * 2) : 0;
	updateLastResult = failure | ((subtype | port) << 16);
	result = (int)0x800f0205;
	goto leaveUpdate;
configFailure:
	updateLastResult = failure | (port << 16);
	result = (int)0x800f0205;
	goto leaveUpdate;
deviceFailure:
	subtype = (result & 0x80250200) == 0x80250200 ? (uint8_t)(result * 2) : 0;
	updateLastResult = failure | ((subtype | port) << 16);
leaveUpdate:
	SceTouchForDriver_423F26D3();
	return result;
}
