/* Sensor command encoding and response decoding. */
#include "motion_command.h"
#include <string.h>

void motionInitReadBlockRequest(MotionSensorResponse *request,
	uint8_t block, uint8_t mode)
{
	request->payload.readBlock.block = block;
	request->payload.readBlock.mode = mode;
	request->type = 4;
	request->size = 4;
}

void motionInitSensorControlRequest(MotionSensorResponse *request, int control)
{
	request->payload.control = control;
	request->type = 6;
	request->size = 3;
}

void motionInitDeviceInfoRequest(MotionSensorResponse *request)
{
	request->type = 12;
	request->size = 2;
}

void motionInitSamplesRequest(MotionSensorResponse *request)
{
	request->type = 2;
	request->size = 2;
}

void motionInitIdentityRequest(MotionSensorResponse *request)
{
	request->type = 0x85;
	request->size = 2;
}

void motionInitResetRequest(MotionSensorResponse *request, int resetCommand)
{
	request->payload.resetCommand = resetCommand;
	request->type = 15;
	request->size = 3;
}

int motionEncodeRequest(MotionSensorPacket *packet,
	const MotionSensorResponse *responses, int count)
{
	uint8_t *output;
	uint8_t length;
	uint8_t checksum = 0;
	int size;
	int index;
	int i;

	if (packet == NULL)
		return (int)0x80360001U;
	output = &packet->data[1];
	if (count <= 0) {
		packet->data[0] = 3;
		packet->data[1] = 0;
		length = 3;
		goto sum;
	}
	output[0] = responses->type;
	output[1] = responses->size;
	size = responses->size;
	length = size + 3;
	if ((unsigned int)size > 56)
		return (int)0x80360103U;
	index = 0;
	for (;;) {
		if (responses->type == 0x81) {
			output[2] = responses->payload.startTransfer.headerSize;
			output[3] = responses->payload.startTransfer.hwInfo;
			output[4] = responses->payload.startTransfer.dataType;
			output[5] = responses->payload.startTransfer.dataVersion;
			output[6] = responses->payload.startTransfer.dataVersion >> 8;
			output[7] = responses->payload.startTransfer.dataSize;
			output[8] = responses->payload.startTransfer.dataSizeBytes[1];
			output[9] = responses->payload.startTransfer.dataSizeBytes[2];
			output[10] = responses->payload.startTransfer.dataSizeBytes[3];
			output[11] = responses->payload.startTransfer.address;
			output[12] = responses->payload.startTransfer.addressBytes[1];
			output[13] = responses->payload.startTransfer.addressBytes[2];
			output[14] = responses->payload.startTransfer.addressBytes[3];
			output[15] = responses->payload.startTransfer.check_sum;
			output[16] = responses->payload.startTransfer.check_sum >> 8;
			output[17] = responses->payload.startTransfer.terminalByte;
			size = responses->size;
		} else if (responses->type == 0x82) {
			if (size == 22 || size == 38) {
				uint8_t *data = output + 4;

				output[2] = responses->payload.programTransfer.offset;
				output[3] = responses->payload.programTransfer.offset >> 8;
				for (i = 0; i < responses->size - 6; i++)
					*data++ = responses->payload.programTransfer.programData[i];
				data[0] = responses->payload.programTransfer.trailer[0];
				data[1] = responses->payload.programTransfer.trailer[1];
				size = responses->size;
			}
		} else if (responses->type == 5) {
			output[2] = responses->payload.blockWrite.block;
			output[3] = responses->payload.blockWrite.size;
			output[4] = responses->payload.blockWrite.size >> 8;
			output[5] = responses->payload.blockWrite.offset;
			output[6] = responses->payload.blockWrite.offset >> 8;
			output[7] = responses->payload.blockWrite.count;
			for (i = 0; i < responses->payload.blockWrite.count; i++)
				output[i + 8] = responses->payload.blockWrite.data[i];
			size = responses->size;
		} else if (size > 2) {
			for (i = 0; i < size - 2; i++) {
				output[i + 2] = responses->payload.bytes[i];
				size = responses->size;
			}
		}
		index++;
		output += size;
		if (index == count)
			break;
		responses++;
		output[0] = responses->type;
		output[1] = responses->size;
		size = responses->size;
		length += size;
		if ((unsigned int)length > 59 || (unsigned int)size > 56)
			return (int)0x80360103U;
	}
	packet->data[0] = length;
	packet->data[length - 2] = 0;
sum:
	for (i = 0; i < length - 1; i++)
		checksum += packet->data[i];
	packet->data[length - 1] = checksum;
	packet->size = 59;
	return 0;
}

int motionParseResponse(const MotionSensorPacket *packet,
	MotionSensorResponse *responses, int capacity)
{
	const uint8_t *input;
	unsigned int length;
	unsigned int type;
	unsigned int size;
	uint8_t remaining;
	uint8_t checksum = 0;
	int count = 0;
	unsigned int i;

	if (responses == NULL || packet == NULL || capacity <= 0)
		return (int)0x80360001U;
	if (packet->size <= 58)
		return (int)0x80360101U;
	length = packet->data[0];
	if (packet->size < length)
		return (int)0x80360101U;
	if (length == 0)
		return 0;
	if (length <= 2)
		return (int)0x80360101U;
	length--;
	for (i = 0; i < length; i++)
		checksum += packet->data[i];
	if (packet->data[length] != checksum)
		return (int)0x80360102U;
	input = &packet->data[1];
	type = *input;
	remaining = length;
	if (type == 0)
		return 0;
	responses->type = type;
	size = input[1];
	responses->size = size;
	for (;;) {
		if (size > remaining)
			return (int)0x80360101U;
		if (size <= 1)
			return (int)0x80360103U;
		if (type <= 0x84) {
			if (type >= 0x80)
				goto status;
			if (type == 4) {
				if (size <= 7)
					return (int)0x80360103U;
				responses->payload.block.block = input[2];
				responses->payload.block.size = input[3] | (input[4] << 8);
				responses->payload.block.offset = input[5] | (input[6] << 8);
				responses->payload.block.count = input[7];
				if (input[7] > 48)
					return (int)0x80360103U;
				memcpy(responses->payload.block.data, input + 8, input[7]);
				size = responses->size;
			} else if (type > 4) {
				if (type == 5)
					goto status;
				if (type == 7) {
					unsigned int version;

					if (size <= 23)
						return (int)0x80360103U;
					version = input[2] | (input[3] << 8);
					responses->payload.deviceInfo.firmwareVersion = version;
					responses->payload.deviceInfo.hardwareInfo = input[4] | (input[5] << 8);
					responses->payload.deviceInfo.deviceMetadata =
						(input[8] << 16) | ((uint32_t)input[9] << 24) |
						input[6] | (input[7] << 8);
					responses->payload.deviceInfo.protocolVersion = input[10] | (input[11] << 8);
					responses->payload.deviceInfo.attributes[0] = input[12];
					responses->payload.deviceInfo.attributes[1] = input[13];
					responses->payload.deviceInfo.attributes[2] = input[14];
					responses->payload.deviceInfo.attributes[3] = input[15];
					if (version <= 31) {
						responses->payload.deviceInfo.descriptor = input[16] | (input[17] << 8);
						responses->payload.deviceInfo.configuration.legacy[0] = input[18] | (input[19] << 8);
						responses->payload.deviceInfo.configuration.legacy[1] = input[20] | (input[21] << 8);
						responses->payload.deviceInfo.configuration.legacy[2] = input[22] | (input[23] << 8);
						responses->payload.deviceInfo.configuration.legacy[3] = input[24] | (input[25] << 8);
					} else {
						responses->payload.deviceInfo.descriptor = input[16] | (input[17] << 8);
						responses->payload.deviceInfo.configuration.current.attributes[0] = input[18];
						responses->payload.deviceInfo.configuration.current.attributes[1] = input[19];
						responses->payload.deviceInfo.configuration.current.attributes[2] = input[20];
						if (version >= 0x650) {
							responses->payload.deviceInfo.configuration.current.attributes[3] = input[21];
							responses->payload.deviceInfo.configuration.current.values[0] = input[22] | (input[23] << 8);
							responses->payload.deviceInfo.configuration.current.values[1] = input[24] | (input[25] << 8);
							responses->payload.deviceInfo.configuration.current.values[2] = input[26] | (input[27] << 8);
							responses->payload.deviceInfo.configuration.current.extension = input[28];
							responses->payload.deviceInfo.configuration.current.values[3] = input[29] | (input[30] << 8);
							responses->payload.deviceInfo.configuration.current.values[4] = input[31] | (input[32] << 8);
							responses->payload.deviceInfo.configuration.current.values[5] = input[33] | (input[34] << 8);
						} else {
							responses->payload.deviceInfo.configuration.current.attributes[3] = 0;
							responses->payload.deviceInfo.configuration.current.values[0] = 0;
							responses->payload.deviceInfo.configuration.current.values[1] = 0;
							responses->payload.deviceInfo.configuration.current.values[2] = 0;
							responses->payload.deviceInfo.configuration.current.extension = 0;
							responses->payload.deviceInfo.configuration.current.values[3] = 0;
							responses->payload.deviceInfo.configuration.current.values[4] = 0;
							responses->payload.deviceInfo.configuration.current.values[5] = 0;
						}
					}
					size = responses->size;
				}
			} else if (type == 1) {
				if (size != 6)
					return (int)0x80360103U;
				responses->payload.deviceInfo.firmwareVersion = input[2] | (input[3] << 8);
				responses->payload.deviceInfo.hardwareInfo = input[4] | (input[5] << 8);
			} else if (type == 2) {
				if (size <= 9 || size - 10 > 46)
					return (int)0x80360103U;
				responses->payload.sample.format = input[2];
				responses->payload.sample.timestampWraps = input[3];
				responses->payload.sample.sequence = input[4] | (input[5] << 8);
				responses->payload.sample.sampleFlags = (input[8] << 16) |
					((uint32_t)input[9] << 24) | input[6] | (input[7] << 8);
				memcpy(responses->payload.sample.samples, input + 10, size - 10);
				size = responses->size;
			}
		} else if (type == 0x87) {
			if (size <= 4)
				return (int)0x80360103U;
			responses->payload.identity.kind = input[2];
			responses->payload.identity.hardwareInfoLow = input[3];
			responses->payload.identity.hardwareInfoHigh = input[4];
		} else if (type > 0x87) {
			if (type >= 0xfe)
				goto status;
		} else if (type == 0x85) {
			if (size <= 29)
				return (int)0x80360103U;
			responses->payload.identity.kind = input[2];
			responses->payload.identity.hardwareInfoLow = input[3];
			responses->payload.identity.hardwareInfoHigh = input[4];
			responses->payload.identity.protocolVersion = input[5] | (input[6] << 8);
			responses->payload.identity.firmwareVersion = input[7] | (input[8] << 8);
			responses->payload.identity.descriptorWords[0] = (input[11] << 16) |
				((uint32_t)input[12] << 24) | input[9] | (input[10] << 8);
			responses->payload.identity.descriptorWords[1] = (input[15] << 16) |
				((uint32_t)input[16] << 24) | input[13] | (input[14] << 8);
			responses->payload.identity.descriptorWords[2] = (input[19] << 16) |
				((uint32_t)input[20] << 24) | input[17] | (input[17] << 8);
			responses->payload.identity.flags = input[21];
			responses->payload.identity.descriptorBytes[0] = input[22];
			responses->payload.identity.descriptorBytes[1] = input[23];
			responses->payload.identity.identityCheck[0] = input[24] | (input[25] << 8);
			responses->payload.identity.identityCheck[1] = input[26] | (input[27] << 8);
			responses->payload.identity.extension[0] = input[28];
			responses->payload.identity.extension[1] = input[29];
			size = responses->size;
		} else if (type == 0x86) {
			if (size <= 7)
				return (int)0x80360103U;
			responses->payload.descriptor.kind = input[2];
			responses->payload.descriptor.attributes = input[3];
			responses->payload.descriptor.value = input[4] | (input[5] << 8);
			responses->payload.descriptor.extension[0] = input[6];
			responses->payload.descriptor.extension[1] = input[7];
		}
		goto next;
status:
		if (size == 2)
			return (int)0x80360103U;
		responses->payload.control = input[2];
next:
		input += size;
		type = *input;
		remaining -= size;
		count++;
		if (type == 0 || remaining <= 1)
			return count;
		if (capacity <= count)
			return (int)0x803600ffU;
		responses++;
		responses->type = type;
		size = input[1];
		responses->size = size;
	}
}

int motionDecodeCalibrationHeader(const uint8_t *data, int size,
	SceMotionDevCalibrationHeader *header)
{
	if (data == NULL || header == NULL)
		return (int)0x80360001U;
	if (size <= 11)
		return (int)0x80360103U;
	header->format[0] = data[0];
	header->format[1] = data[1];
	header->metadata = (data[7] << 24) | (data[6] << 16) |
		data[4] | (data[5] << 8);
	header->recordCount = data[10] | (data[11] << 8);
	return 0;
}

int motionDecodeCalibrationRecords(const uint8_t *data, int size,
	const SceMotionDevCalibrationHeader *header, SceMotionDevCalibrationData *records,
	int capacity)
{
	const MotionCalibrationWireRecord *input;
	int count;
	int index;

	if (data == NULL || header == NULL || records == NULL)
		return (int)0x80360001U;
	count = header->recordCount;
	if (count == 0)
		return count;
	if (capacity < count)
		count = capacity;
	if (size != 20 * count)
		return (int)0x80360107U;
	if (count <= 0)
		return count;
	input = (const MotionCalibrationWireRecord *)data;
	index = 0;
	do {
		unsigned int kindChannel = input->kindChannel;

		index++;
		__builtin_prefetch((const void *)((uintptr_t)input + 59));
		__builtin_prefetch((const void *)((uintptr_t)records + 52));
		records->kind = kindChannel >> 4;
		records->channel = input->kindChannel & 15;
		records->attributes[0] = input->attributes[0];
		records->attributes[1] = input->attributes[1];
		records->attributes[2] = input->attributes[2];
		records->parameters[0] = (input->parameterBytes[0][3] << 24) |
			(input->parameterBytes[0][2] << 16) | input->parameterBytes[0][0] |
			(input->parameterBytes[0][1] << 8);
		records->parameters[1] = (input->parameterBytes[1][3] << 24) |
			(input->parameterBytes[1][2] << 16) | input->parameterBytes[1][0] |
			(input->parameterBytes[1][1] << 8);
		records->parameters[2] = (input->parameterBytes[2][3] << 24) |
			(input->parameterBytes[2][2] << 16) | input->parameterBytes[2][0] |
			(input->parameterBytes[2][1] << 8);
		records->parameters[3] = (input->parameterBytes[3][3] << 24) |
			(input->parameterBytes[3][2] << 16) | input->parameterBytes[3][0] |
			(input->parameterBytes[3][1] << 8);
		input++;
		records++;
	} while (index != count);
	return index;
}

int motionDecodeSamples(const MotionSensorResponse *response, SceMotionDevData *data)
{
	unsigned int format;
	unsigned int timestampFactor;
	unsigned int sequence;
	unsigned int available;
	int consumed = 0;
	int count = 0;

	if (response == NULL || data == NULL)
		return (int)0x80360001U;
	data->sampleFormat = 0;
	data->timestampWraps = 0;
	data->count = 0;
	if (response->type != 2 || response->payload.sample.sampleFlags == 0)
		return 0;
	format = response->payload.sample.format;
	data->sampleFormat = format;
	data->timestampWraps = response->payload.sample.timestampWraps;
	timestampFactor = response->payload.sample.timestampWraps & 15;
	sequence = response->payload.sample.sequence;
	available = response->size - 10;
	if (response->size <= 10)
		return (int)0x80360103U;
	for (;;) {
		unsigned int packedFlags = response->payload.sample.sampleFlags >> (4 * count);
		uint16_t gyro = packedFlags & 2;
		uint16_t gyroSecondary = 0;
		uint16_t accel = packedFlags & 4;
		uint16_t magnetometer = packedFlags & 8;
		uint16_t status = packedFlags & 1;
		uint16_t flags = 0;
		int size = 0;
		const uint8_t *input;
		SceMotionDevDataEntry *entry;

		if (gyro) {
			flags = MOTION_ENTRY_GYRO;
			size = 6;
			if (format == 0) {
				gyroSecondary = 2;
				flags |= 2;
				size += 6;
			}
		}
		if (accel) {
			flags |= MOTION_ENTRY_ACCEL;
			size += 6;
		}
		if (magnetometer) {
			flags |= 8;
			size += 6;
		}
		if (status) {
			flags |= MOTION_ENTRY_STATUS;
			size += 4;
		}
		if (size == 0 || (int)(available - consumed) < size) {
			data->count = count;
			return 0;
		}
		if (count == MOTION_DATA_ENTRIES)
			return (int)0x80360005U;
		entry = &data->entry[count];
		entry->flags = flags;
		entry->sequence = sequence;
		entry->timestamp = timestampFactor * sequence;
		if (status) {
			input = response->payload.sample.samples + consumed;
			consumed += 4;
			entry->statusData = input[0] | (input[1] << 8);
			entry->status = input[2] | (input[3] << 8);
		}
		if (gyro) {
			input = response->payload.sample.samples + consumed;
			consumed += 6;
			entry->gyro[0] = input[0] | (input[1] << 8);
			entry->gyro[1] = input[2] | (input[3] << 8);
			entry->gyro[2] = input[4] | (input[5] << 8);
		}
		if (gyroSecondary) {
			input = response->payload.sample.samples + consumed;
			consumed += 6;
			entry->gyroSecondary[0] = input[0] | (input[1] << 8);
			entry->gyroSecondary[1] = input[2] | (input[3] << 8);
			entry->gyroSecondary[2] = input[4] | (input[5] << 8);
		}
		if (accel) {
			input = response->payload.sample.samples + consumed;
			consumed += 6;
			entry->accel[0] = input[0] | (input[1] << 8);
			entry->accel[1] = input[2] | (input[3] << 8);
			entry->accel[2] = input[4] | (input[5] << 8);
		}
		if (magnetometer) {
			input = response->payload.sample.samples + consumed;
			consumed += 6;
			entry->magnetometer[0] = input[0] | (input[1] << 8);
			entry->magnetometer[1] = input[2] | (input[3] << 8);
			entry->magnetometer[2] = input[4] | (input[5] << 8);
		}
		count++;
		if ((flags & 15) != 0 || count == 1)
			sequence = (uint16_t)(sequence + 1);
	}
}
