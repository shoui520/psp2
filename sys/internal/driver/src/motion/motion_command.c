/* Completion callbacks for the sensor command state machine. */
#include "motion_command.h"
#include <string.h>
#include <lowio/gpio.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>

MotionWorkerContext motionWorker;
MotionCommandContext motionCommand;

void motionCommandComplete(int result, void *arg)
{
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandResetComplete(int result, void *arg)
{
	if ((uint8_t)motionCommand.destination.command == 3)
		motionCommand.resetRequired = 1;
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandBufferComplete(int result, void *arg)
{
	if (result == 0 && motionCommand.auxiliaryOutput == NULL)
		result = (int)0x80360001U;
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandResponseComplete(int result, void *arg)
{
	if (result == 0) {
		MotionSensorResponse *response = motionCommand.auxiliaryOutput;

		if (response != NULL) {
			response->type = 0;
			response->size = 0;
			motionParseResponse(&motionCommand.response, response, 1);
		}
	}
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandSamplesComplete(int result, void *arg)
{
	SceMotionDevData *data = motionCommand.destination.output;
	MotionSensorResponse responses[2];
	uint64_t time;
	int count;
	int index;

	if (data == NULL)
		goto invalid_output;
	memset(data, 0, sizeof(*data));
	time = sceKernelGetSystemTimeWide();
	count = motionParseResponse(&motionCommand.response, responses, 2);
	if (count <= 0)
		goto no_samples;
	if (responses[0].type == 2) {
		index = 0;
	} else if (count > 1 && responses[1].type == 2) {
		index = 1;
	} else {
		goto no_samples;
	}
	data->time = time;
	data->flags = 0;
	data->sampleFormat = 0;
	data->timestampWraps = 0;
	data->count = 0;
	result = motionDecodeSamples(&responses[index], data);
	if (result < 0) {
		data->count = 0;
		data->flags |= 0x80;
	}
	goto complete;

no_samples:
	result = 0;
complete:
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
	return;

invalid_output:
	result = (int)0x803600ffU;
	goto complete;
}

int motionCommandWorker(SceSize args, void *argp)
{
	union {
		MotionSensorPacket request;
		SceMotionDevCalibrationHeader header;
	} scratch;

	for (;;) {
		void (*completion)(int result, void *arg);
		void *arg;
		int result;

		while (sceKernelWaitEventFlag(motionWorker.event, 1, 5,
			NULL, NULL) < 0)
			;
		completion = motionWorker.completion;
		arg = motionWorker.arg;
		switch (motionWorker.command) {
		case 1: {
			const MotionSensorPacket *request = motionWorker.input.packet;
			MotionSensorPacket *response = motionWorker.output.packet;
			uint32_t delay = motionWorker.delay;

			if (motionCommand.resetRequired != 0) {
				if (SceGpioForDriver_129DF5AC(0, 28) != 0 &&
					SceMotionDevForDriver_77BA3A04() == 0)
					sceKernelDelayThread(16000);
				if (SceMotionDevForDriver_77BA3A04() != 0) {
					motionEncodeRequest(&scratch.request, NULL, 0);
					motionSpiTransfer(&scratch.request, response);
					sceKernelDelayThread(2000);
				} else {
					if (SceSysconForDriver_40FF3898(2, 0) >= 0) {
						sceKernelDelayThread(2000);
						SceSysconForDriver_40FF3898(2, 1);
						motionCommand.resetRequired = 0;
					}
					sceKernelDelayThread(80000);
				}
				motionCommand.resetRequired = 0;
				SceGpioForDriver_129DF5AC(0, 28);
			}
			result = motionSpiTransfer(request, response);
			motionWorker.transferMode = 0;
			motionWorker.delay = 0;
			motionWorker.output.packet = NULL;
			motionWorker.command = 0;
			motionWorker.input.packet = NULL;
			motionWorker.completion = NULL;
			motionWorker.arg = NULL;
			if (completion != NULL)
				completion(result, arg);
			sceKernelDelayThread(delay != 0 ? delay : 2000);
			continue;
		}
		case 2:
			result = SceSysconForDriver_40FF3898(2, 0);
			if (result >= 0) {
				motionSpiDeactivate();
				result = motionSpiSuspend();
				if (result >= 0) {
					SceGpioForDriver_F6310435(0, 27);
					result = SceSysconForDriver_063425AE(0);
					if (result >= 0) {
						result = 0;
						motionCommand.resetRequired = 0;
					}
				}
			}
			break;
		case 3: {
			int activeLow = motionWorker.input.activeLow;

			SceSysconForDriver_063425AE(1);
			SceGpioForDriver_372022A4(0, 27, 0);
			SceGpioForDriver_372022A4(0, 28, 1);
			if (activeLow != 0)
				SceGpioForDriver_F6310435(0, 27);
			else
				SceGpioForDriver_D454A584(0, 27);
			result = motionSpiResume();
			if (result >= 0) {
				motionSpiActivate();
				if (SceSysconForDriver_40FF3898(2, 0) >= 0) {
					sceKernelDelayThread(2000);
					SceSysconForDriver_40FF3898(2, 1);
					motionCommand.resetRequired = 0;
				}
				sceKernelDelayThread(80000);
				result = 0;
			}
			break;
		}
		case 4: {
			uint32_t calibration = motionWorker.input.calibration;
			MotionCalibrationBuffer *buffer = motionWorker.output.calibration;
			int remaining;
			uint32_t offset;

			if (calibration > 1 || buffer == NULL) {
				result = (int)0x80360001U;
				break;
			}
			buffer->size = 0;
			buffer->received = 0;
			__builtin_memset(&scratch.header, 0, sizeof(scratch.header));
			result = SceSysconForDriver_76506A57(calibration, 0,
				buffer->data, 12);
			if (result < 0)
				break;
			result = motionDecodeCalibrationHeader(buffer->data, 12,
				&scratch.header);
			if (result < 0)
				break;
			remaining = 20 * scratch.header.recordCount;
			if ((uint32_t)(remaining + 12) > 512) {
				result = (int)0x80360106U;
				break;
			}
			offset = 12;
			buffer->size = remaining + 12;
			buffer->received = 12;
			if (remaining != 0) {
				for (;;) {
					int count = remaining >= 64 ? 64 : remaining;

					remaining -= count;
					result = SceSysconForDriver_76506A57(calibration,
						offset, buffer->data + offset, count);
					if (result < 0) {
						buffer->size = 0;
						break;
					}
					offset = buffer->received + count;
					buffer->received = offset;
					if (remaining <= 0)
						break;
				}
			}
			break;
		}
		default:
			motionWorker.transferMode = 0;
			motionWorker.delay = 0;
			motionWorker.output.packet = NULL;
			motionWorker.command = 0;
			motionWorker.input.packet = NULL;
			motionWorker.completion = NULL;
			motionWorker.arg = NULL;
			continue;
		}
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.output.packet = NULL;
		motionWorker.command = 0;
		motionWorker.input.packet = NULL;
		motionWorker.completion = NULL;
		motionWorker.arg = NULL;
		if (completion != NULL)
			completion(result, arg);
	}
}

int motionQueueRequest(const MotionSensorResponse *request,
	void (*completion)(int result, void *arg), void *arg, uint32_t delay)
{
	int result;
	uint32_t transferMode;

	if (completion == NULL)
		return (int)0x80360003U;
	if (motionEncodeRequest(&motionCommand.request, request,
		request != NULL ? 1 : 0) < 0)
		return (int)0x803600ffU;
	if (request == NULL) {
		motionCommand.busy = 1;
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		transferMode = delay != 0 ? 2 : 0;
		if (motionWorker.command != 0)
			goto busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = transferMode;
		motionWorker.delay = delay;
		goto queue_completion;
	}
	if (request->type == 130)
		goto immediate;
	if (request->type != 131) {
		if (request->type == 129)
			goto immediate;
		goto ordinary;
	}
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	delay = SceMotionDevForDriver_77BA3A04() != 0 ? 100000 : 10000;
	if (motionWorker.command != 0)
		goto busy;
	motionWorker.command = 1;
	motionWorker.input.packet = &motionCommand.request;
	motionWorker.output.packet = &motionCommand.response;
	motionWorker.delay = delay;
	motionWorker.completion = completion;
	motionWorker.transferMode = 2;
	motionWorker.arg = arg;
	result = sceKernelSetEventFlag(motionWorker.event, 1);
	if (result < 0)
		goto failed;
	return result;

immediate:
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	if (motionWorker.command != 0)
		goto busy;
	motionWorker.command = 1;
	motionWorker.input.packet = &motionCommand.request;
	motionWorker.output.packet = &motionCommand.response;
	motionWorker.transferMode = 0;
	motionWorker.delay = 0;
queue_completion:
	motionWorker.completion = completion;
	motionWorker.arg = arg;
	result = sceKernelSetEventFlag(motionWorker.event, 1);
	if (result < 0)
		goto failed;
	return result;

ordinary:
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	transferMode = delay != 0 ? 2 : 0;
	if (motionWorker.command != 0)
		goto busy;
	motionWorker.command = 1;
	motionWorker.input.packet = &motionCommand.request;
	motionWorker.output.packet = &motionCommand.response;
	motionWorker.transferMode = transferMode;
	motionWorker.delay = delay;
	motionWorker.completion = completion;
	motionWorker.arg = arg;
	result = sceKernelSetEventFlag(motionWorker.event, 1);
	if (result >= 0)
		return result;
failed:
	motionCommand.busy = 0;
	return result;
busy:
	result = (int)0x80360003U;
	goto failed;
}

void motionCommandIdentityComplete(int result, void *arg)
{
	MotionSensorResponse responses[2];
	int count;
	int index;

	if (result != 0)
		goto complete;
	count = motionParseResponse(&motionCommand.response, responses, 2);
	if (count < 0) {
		result = count;
		goto complete;
	}
	if (count != 0) {
		if (responses[0].type == 133) {
			index = 0;
			goto identity;
		}
		if (count > 1 && responses[1].type == 133) {
			index = 1;
			goto identity;
		}
	}
	if (motionCommand.retries != 0) {
		if (motionEncodeRequest(&motionCommand.request, NULL, 0) < 0) {
			result = (int)0x803600ffU;
		} else {
			motionCommand.busy = 1;
			memset(&motionCommand.response, 0, sizeof(motionCommand.response));
			if (motionWorker.command != 0) {
				result = (int)0x80360003U;
			} else {
				motionWorker.command = 1;
				motionWorker.input.packet = &motionCommand.request;
				motionWorker.output.packet = &motionCommand.response;
				motionWorker.transferMode = 0;
				motionWorker.delay = 0;
				motionWorker.completion = motionCommandIdentityComplete;
				motionWorker.arg = arg;
				count = sceKernelSetEventFlag(motionWorker.event, 1);
				if (count >= 0) {
					--motionCommand.retries;
					return;
				}
				result = count;
			}
		}
	} else {
		result = (int)0x80360109U;
	}
	goto complete;

identity: {
	SceMotionDevDeviceInfo *info = motionCommand.destination.output;

	if (responses[index].payload.identity.kind == 6) {
		info->firmwareVersion = responses[index].payload.identity.identityCheck[0] ==
			responses[index].payload.identity.identityCheck[1] ?
			responses[index].payload.identity.firmwareVersion : 0xffff;
		info->hardwareInfo = responses[index].payload.identity.hardwareInfoLow +
			(responses[index].payload.identity.hardwareInfoHigh << 8);
		info->protocolVersion = responses[index].payload.identity.protocolVersion;
	} else {
		result = (int)0x8036010aU;
		info->firmwareVersion = 0;
		info->hardwareInfo = 0;
		info->protocolVersion = 0;
	}
}
complete:
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandDeviceInfoComplete(int result, void *arg)
{
	MotionSensorResponse responses[2];
	int count;
	int index;

	if (result != 0)
		goto complete;
	count = motionParseResponse(&motionCommand.response, responses, 2);
	if (count < 0) {
		result = count;
		goto complete;
	}
	if (count != 0) {
		if (responses[0].type == 7) {
			index = 0;
			goto device_info;
		}
		if (responses[0].type == 254)
			goto rejected;
		if (count > 1) {
			if (responses[1].type == 7) {
				index = 1;
				goto device_info;
			}
			if (responses[1].type == 254)
				goto rejected;
		}
	}
	if (motionCommand.retries == 0) {
		result = (int)0x80360109U;
		goto complete;
	}
	if (motionEncodeRequest(&motionCommand.request, NULL, 0) < 0) {
		result = (int)0x803600ffU;
		goto complete;
	}
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	if (motionWorker.command != 0) {
		result = (int)0x80360003U;
		goto complete;
	}
	motionWorker.command = 1;
	motionWorker.input.packet = &motionCommand.request;
	motionWorker.output.packet = &motionCommand.response;
	motionWorker.transferMode = 0;
	motionWorker.delay = 0;
	motionWorker.completion = motionCommandDeviceInfoComplete;
	motionWorker.arg = arg;
	count = sceKernelSetEventFlag(motionWorker.event, 1);
	if (count >= 0) {
		--motionCommand.retries;
		return;
	}
	result = count;
	goto complete;

device_info: {
	SceMotionDevDeviceInfo *info = motionCommand.destination.output;
	unsigned int version = responses[index].payload.deviceInfo.firmwareVersion;

	info->firmwareVersion = version;
	info->hardwareInfo = responses[index].payload.deviceInfo.hardwareInfo;
	info->deviceMetadata = responses[index].payload.deviceInfo.deviceMetadata;
	info->protocolVersion = responses[index].payload.deviceInfo.protocolVersion;
	info->attributes[0] = responses[index].payload.deviceInfo.attributes[0];
	info->attributes[1] = responses[index].payload.deviceInfo.attributes[1];
	info->attributes[2] = responses[index].payload.deviceInfo.attributes[2];
	info->attributes[3] = responses[index].payload.deviceInfo.attributes[3];
	info->descriptor = responses[index].payload.deviceInfo.descriptor;
	if (version > 0x1f) {
		info->configuration.current.attributes[0] =
			responses[index].payload.deviceInfo.configuration.current.attributes[0];
		info->configuration.current.attributes[1] =
			responses[index].payload.deviceInfo.configuration.current.attributes[1];
		info->configuration.current.attributes[2] =
			responses[index].payload.deviceInfo.configuration.current.attributes[2];
		if (version < 0x650) {
			info->configuration.current.attributes[3] = 0;
			info->configuration.current.values[0] = 0;
			info->configuration.current.values[1] = 0;
			info->configuration.current.values[2] = 0;
			info->extension = 0;
			info->configuration.current.values[3] = 0;
			info->configuration.current.values[4] = 0;
			info->configuration.current.values[5] = 0;
		} else {
			info->configuration.current.attributes[3] =
				responses[index].payload.deviceInfo.configuration.current.attributes[3];
			info->configuration.current.values[0] =
				responses[index].payload.deviceInfo.configuration.current.values[0];
			info->configuration.current.values[1] =
				responses[index].payload.deviceInfo.configuration.current.values[1];
			info->configuration.current.values[2] =
				responses[index].payload.deviceInfo.configuration.current.values[2];
			info->extension =
				responses[index].payload.deviceInfo.configuration.current.extension;
			info->configuration.current.values[3] =
				responses[index].payload.deviceInfo.configuration.current.values[3];
			info->configuration.current.values[4] =
				responses[index].payload.deviceInfo.configuration.current.values[4];
			info->configuration.current.values[5] =
				responses[index].payload.deviceInfo.configuration.current.values[5];
		}
	} else {
		info->configuration.legacy[0] =
			responses[index].payload.deviceInfo.configuration.legacy[0];
		info->configuration.legacy[1] =
			responses[index].payload.deviceInfo.configuration.legacy[1];
		info->configuration.legacy[2] =
			responses[index].payload.deviceInfo.configuration.legacy[2];
		info->configuration.legacy[3] =
			responses[index].payload.deviceInfo.configuration.legacy[3];
	}
	}
	goto complete;

rejected:
	result = (int)0x8036010aU;
complete:
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

void motionCommandReadBlockComplete(int result, void *arg)
{
	MotionSensorResponse request;
	MotionSensorResponse responses[2];
	MotionCalibrationBuffer *buffer;
	uint32_t block;
	uint32_t size;
	int count;
	int transfer;

	if (result != 0)
		goto complete;
	count = motionParseResponse(&motionCommand.response, responses, 2);
	block = motionCommand.destination.command;
	buffer = motionCommand.auxiliaryOutput;
	if (count > 0) {
		size = buffer->size;
		for (int i = 0; i != count; ++i) {
			const MotionSensorBlock *response = &responses[i].payload.block;
			uint32_t total;
			uint32_t end;

			__builtin_prefetch((const void *)((uintptr_t)&responses[i] +
				5 * sizeof(responses[i])));
			if (responses[i].type != 4 || block != response->block)
				continue;
			if (size != 0) {
				total = size;
				size = buffer->received;
			} else {
				total = response->size;
				if (total == 0)
					goto complete;
				buffer->size = total;
				buffer->received = 0;
				if (total > 512)
					goto invalid_response;
			}
			end = size + response->count;
			if (response->offset != size || end > 512)
				goto invalid_response;
			if (response->count != 0) {
				int copied = 0;
				uint8_t *destination = buffer->data + size;

				do {
					destination[copied] = response->data[copied];
					++copied;
				} while (copied < response->count);
				size = buffer->size;
				end = response->count + buffer->received;
			} else {
				size = total;
			}
			buffer->received = end;
		}
		result = 0;
		if (size != 0 && buffer->received >= size)
			goto complete;
	}
	++motionCommand.pollCount;
	if ((motionCommand.pollCount & 1) == 0) {
		motionInitReadBlockRequest(&request,
			(uint8_t)motionCommand.destination.command, 1);
		result = motionQueueRequest(&request, motionCommandReadBlockComplete,
			arg, 0);
		if (result >= 0)
			return;
		goto complete;
	}
	transfer = motionEncodeRequest(&motionCommand.request, NULL, 0);
	if (transfer < 0)
		return;
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	if (motionWorker.command != 0)
		goto queue_failed;
	motionWorker.command = 1;
	motionWorker.input.packet = &motionCommand.request;
	motionWorker.output.packet = &motionCommand.response;
	motionWorker.transferMode = 0;
	motionWorker.delay = 0;
	motionWorker.completion = motionCommandReadBlockComplete;
	motionWorker.arg = arg;
	if (sceKernelSetEventFlag(motionWorker.event, 1) >= 0)
		return;
queue_failed:
	motionCommand.busy = 0;
	return;

invalid_response:
	result = (int)0x80360103U;
complete:
	motionCommand.busy = 0;
	motionCommand.retries = 0;
	if (motionCommand.completion != NULL)
		motionCommand.completion(result, arg);
}

int motionPowerOff(void)
{
	int result;

	result = SceSysconForDriver_40FF3898(2, 0);
	if (result >= 0) {
		motionSpiDeactivate();
		result = motionSpiSuspend();
		if (result >= 0) {
			SceGpioForDriver_F6310435(0, 27);
			result = SceSysconForDriver_063425AE(0);
			if (result >= 0) {
				motionCommand.resetRequired = 0;
				return 0;
			}
		}
	}
	return result;
}

int motionPowerOn(int activeLow)
{
	int result;

	SceSysconForDriver_063425AE(1);
	SceGpioForDriver_372022A4(0, 27, 0);
	SceGpioForDriver_372022A4(0, 28, 1);
	if (activeLow != 0)
		SceGpioForDriver_F6310435(0, 27);
	else
		SceGpioForDriver_D454A584(0, 27);
	result = motionSpiResume();
	if (result >= 0) {
		motionSpiActivate();
		if (SceSysconForDriver_40FF3898(2, 0) >= 0) {
			sceKernelDelayThread(2000);
			SceSysconForDriver_40FF3898(2, 1);
			motionCommand.resetRequired = 0;
		}
		sceKernelDelayThread(80000);
		return 0;
	}
	return result;
}

int motionCommandInitialize(void)
{
	SceKernelThreadOptParamV1 option;
	int result;

	memset(&motionCommand, 0, sizeof(motionCommand));
	result = SceSysconForDriver_063425AE(1);
	if (result < 0)
		return result;
	SceGpioForDriver_372022A4(0, 27, 0);
	SceGpioForDriver_372022A4(0, 28, 1);
	result = motionSpiInitialize(6, 500, 59, 0);
	if (result < 0)
		return result;
	SceGpioForDriver_D454A584(0, 27);
	motionSpiActivate();
	if (SceSysconForDriver_40FF3898(2, 0) >= 0) {
		sceKernelDelayThread(2000);
		SceSysconForDriver_40FF3898(2, 1);
		motionCommand.resetRequired = 0;
	}
	motionWorker.thread = 0;
	motionWorker.event = 0;
	motionWorker.command = 0;
	motionWorker.input.calibration = 0;
	motionWorker.output.packet = NULL;
	motionWorker.transferMode = 0;
	motionWorker.delay = 0;
	motionWorker.completion = NULL;
	motionWorker.arg = NULL;
	option.size = 0;
	option.attr = 0;
	result = sceKernelCreateEventFlag("SceMotionDev", 0x1000, 0, NULL);
	motionWorker.event = result;
	if (result < 0)
		return result;
	option.size = 8;
	result = sceKernelCreateThread("SceMotionDev", motionCommandWorker,
		42, 0x2000, 0x10000000, 0x80000,
		(const SceKernelThreadOptParam *)&option);
	motionWorker.thread = result;
	if (result < 0)
		return result;
	return sceKernelStartThread(result, 0, NULL);
}

int motionQueuePowerOff(void (*completion)(int result, void *arg), void *arg)
{
	if (motionWorker.command != 0)
		return (int)0x80360003U;
	motionWorker.completion = completion;
	motionWorker.arg = arg;
	motionWorker.command = 2;
	return sceKernelSetEventFlag(motionWorker.event, 1);
}

int motionQueuePowerOn(int activeLow,
	void (*completion)(int result, void *arg), void *arg)
{
	if (motionWorker.command != 0)
		return (int)0x80360003U;
	motionWorker.input.activeLow = activeLow;
	motionWorker.completion = completion;
	motionWorker.arg = arg;
	motionWorker.command = 3;
	return sceKernelSetEventFlag(motionWorker.event, 1);
}

int motionSetActivePolarity(int activeLow)
{
	if (activeLow != 0)
		return SceGpioForDriver_F6310435(0, 27);
	return SceGpioForDriver_D454A584(0, 27);
}

int motionReleaseReset(int pulse)
{
	int result;

	if (pulse != 0) {
		result = SceSysconForDriver_40FF3898(2, 0);
		if (result < 0)
			return result;
		sceKernelDelayThread(2000);
	}
	result = SceSysconForDriver_40FF3898(2, 1);
	motionCommand.resetRequired = 0;
	return result;
}

int motionReadCalibrationBlock(uint32_t block, MotionCalibrationBuffer *buffer,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (block > 2)
		return (int)0x80360001U;
	if (completion == NULL || buffer == NULL)
		return (int)0x80360001U;
	buffer->size = 0;
	buffer->received = 0;
	if (SceMotionDevForDriver_77BA3A04() == 0) {
		if (motionWorker.command != 0)
			return (int)0x80360003U;
		motionWorker.input.calibration = block;
		motionWorker.output.calibration = buffer;
		motionWorker.completion = completion;
		motionWorker.arg = arg;
		motionWorker.command = 4;
		return sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	motionCommand.retries = 0;
	motionCommand.completion = completion;
	motionCommand.pollCount = 0;
	motionCommand.destination.command = block;
	motionCommand.auxiliaryOutput = buffer;
	motionInitReadBlockRequest(&request, block, 0);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0)
		return (int)0x803600ffU;
	motionCommand.busy = 1;
	if (request.type == 0x82 || request.type != 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandReadBlockComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandReadBlockComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionRequestDeviceInfo(SceMotionDevDeviceInfo *info,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL || info == NULL)
		return (int)0x80360001U;
	memset(info, 0, sizeof(*info));
	motionCommand.retries = 10;
	motionCommand.completion = completion;
	motionCommand.destination.output = info;
	motionInitDeviceInfoRequest(&request);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0)
		return (int)0x803600ffU;
	motionCommand.busy = 1;
	if (request.type == 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandDeviceInfoComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandDeviceInfoComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionRequestIdentity(SceMotionDevDeviceInfo *info,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL || info == NULL)
		return (int)0x80360001U;
	memset(info, 0, sizeof(*info));
	motionCommand.retries = 10;
	motionCommand.completion = completion;
	motionCommand.destination.output = info;
	motionInitIdentityRequest(&request);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0)
		return (int)0x803600ffU;
	motionCommand.busy = 1;
	if (request.type == 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandIdentityComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandIdentityComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionSendSensorControl(int control,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL)
		return (int)0x80360001U;
	motionCommand.completion = completion;
	motionCommand.retries = 0;
	motionInitSensorControlRequest(&request, control);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0)
		return (int)0x803600ffU;
	motionCommand.busy = 1;
	if (request.type == 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionRequestReset(int resetCommand,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL)
		return (int)0x80360001U;
	motionCommand.completion = completion;
	motionCommand.destination.resetCommand = resetCommand;
	motionCommand.busy = 1;
	motionCommand.retries = 0;
	motionCommand.pollCount = 0;
	motionInitResetRequest(&request, resetCommand);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0) {
		motionCommand.busy = 0;
		return (int)0x803600ffU;
	}
	motionCommand.busy = 1;
	if (request.type == 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandResetComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandResetComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionRequestSensorReset(void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse request;
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL)
		return (int)0x80360001U;
	motionCommand.completion = completion;
	motionCommand.destination.resetCommand = 3;
	motionCommand.busy = 1;
	motionCommand.retries = 0;
	motionCommand.pollCount = 0;
	motionInitResetRequest(&request, 3);
	if (motionEncodeRequest(&motionCommand.request, &request, 1) < 0) {
		motionCommand.busy = 0;
		return (int)0x803600ffU;
	}
	motionCommand.busy = 1;
	if (request.type == 0x83) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandResetComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else if (request.type == 0x82 || request.type == 0x81) {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandResetComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 2;
		motionWorker.delay = 10000;
		motionWorker.completion = motionCommandResetComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result < 0)
		motionCommand.busy = 0;
	return result;

worker_busy:
	motionCommand.busy = 0;
	return (int)0x80360003U;
}

int motionRequestSamples(SceMotionDevData *data, int resetCommand,
	void (*completion)(int result, void *arg), void *arg)
{
	MotionSensorResponse requests[2];
	int count = 1;
	int index;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL)
		return (int)0x80360001U;
	motionCommand.completion = completion;
	motionCommand.destination.output = data;
	motionCommand.retries = 0;
	motionCommand.busy = 1;
	memset(&motionCommand.response, 0, sizeof(motionCommand.response));
	index = SceMotionDevForDriver_77BA3A04();
	if (index != 0) {
		motionInitSamplesRequest(&requests[0]);
		if (resetCommand == 255)
			goto encode;
		index = 1;
		count = 2;
	} else if (resetCommand == 255) {
		return (int)0x803600ffU;
	}
	motionInitResetRequest(&requests[index], resetCommand);
encode:
	result = motionEncodeRequest(&motionCommand.request, requests, count);
	if (result == 0) {
		if (motionWorker.command != 0) {
			result = (int)0x80360003U;
			goto failed;
		}
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.completion = motionCommandSamplesComplete;
		motionWorker.arg = arg;
		motionWorker.command = 1;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result >= 0)
		return result;
failed:
	motionCommand.busy = 0;
	return result;
}

int motionRequestResponse(const MotionSensorResponse *request,
	MotionSensorResponse *response,
	void (*completion)(int result, void *arg), void *arg)
{
	uint32_t delay;
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (completion == NULL || response == NULL)
		return (int)0x80360001U;
	motionCommand.destination.request = request;
	motionCommand.auxiliaryOutput = response;
	motionCommand.completion = completion;
	if (request == NULL)
		return motionQueueRequest(NULL, motionCommandResponseComplete, arg, 0);
	if (request->type == 0x80) {
		motionCommand.busy = 1;
		goto packet_transfer;
	}
	if (request->type == 0x84) {
		motionCommand.busy = 1;
packet_transfer:
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		result = motionEncodeRequest(&motionCommand.request, request, 1);
		if (result == 0) {
			if (motionWorker.command != 0)
				goto worker_busy;
			motionWorker.output.packet = &motionCommand.response;
			motionWorker.transferMode = 0;
			motionWorker.delay = 0;
			motionWorker.command = 1;
			motionWorker.input.packet = &motionCommand.request;
			motionWorker.completion = motionCommandResponseComplete;
			motionWorker.arg = arg;
			result = sceKernelSetEventFlag(motionWorker.event, 1);
		}
		if (result >= 0)
			return result;
		goto failed;
	}
	if (motionEncodeRequest(&motionCommand.request, request, 1) < 0)
		return (int)0x803600ffU;
	if (request->type == 0x82) {
		motionCommand.busy = 1;
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.completion = motionCommandResponseComplete;
		motionWorker.arg = arg;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else if (request->type != 0x83) {
		motionCommand.busy = 1;
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.completion = motionCommandResponseComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 0;
		motionWorker.delay = 0;
		motionWorker.command = 1;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	} else {
		motionCommand.busy = 1;
		memset(&motionCommand.response, 0, sizeof(motionCommand.response));
		delay = 10000;
		if (SceMotionDevForDriver_77BA3A04() != 0)
			delay = 100000;
		if (motionWorker.command != 0)
			goto worker_busy;
		motionWorker.command = 1;
		motionWorker.input.packet = &motionCommand.request;
		motionWorker.output.packet = &motionCommand.response;
		motionWorker.delay = delay;
		motionWorker.completion = motionCommandResponseComplete;
		motionWorker.arg = arg;
		motionWorker.transferMode = 2;
		result = sceKernelSetEventFlag(motionWorker.event, 1);
	}
	if (result >= 0)
		return result;
	goto failed;

worker_busy:
	result = (int)0x80360003U;
failed:
	motionCommand.busy = 0;
	return result;
}

int motionRequestPacket(const MotionSensorPacket *request,
	MotionSensorPacket *response,
	void (*completion)(int result, void *arg), void *arg)
{
	int result;

	if (motionCommand.busy == 1)
		return (int)0x80360003U;
	if (response == NULL || request == NULL || completion == NULL)
		return (int)0x80360001U;
	motionCommand.destination.packet = request;
	motionCommand.auxiliaryOutput = response;
	motionCommand.completion = completion;
	motionCommand.busy = 1;
	memset(response, 0, sizeof(*response));
	if (motionWorker.command != 0) {
		result = (int)0x80360003U;
		goto failed;
	}
	motionWorker.command = 1;
	motionWorker.input.packet = request;
	motionWorker.output.packet = response;
	motionWorker.transferMode = 0;
	motionWorker.delay = 0;
	motionWorker.completion = motionCommandBufferComplete;
	motionWorker.arg = arg;
	result = sceKernelSetEventFlag(motionWorker.event, 1);
	if (result >= 0)
		return result;
failed:
	motionCommand.busy = 0;
	return result;
}
