/* WDS network activation, configuration reports and cancellation. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include "qmi_network_operations.h"
#include "bbmc_memory.h"

/* Each optional parameter is appended directly to the client's wire packet. */
#define APPEND_BYTE(parameterType, parameterValue) do { \
	QmiByteValueTlv *parameter = (QmiByteValueTlv *)cursor; \
	parameter->header.type = (parameterType); \
	parameter->header.lengthLow = 1; \
	parameter->header.lengthHigh = 0; \
	parameter->value = (parameterValue); \
	cursor += sizeof(*parameter); \
} while (0)

#define APPEND_WORD(parameterType, parameterValue) do { \
	QmiWordValueTlv *parameter = (QmiWordValueTlv *)cursor; \
	parameter->header.type = (parameterType); \
	parameter->header.lengthLow = 4; \
	parameter->header.lengthHigh = 0; \
	parameter->value[0] = (parameterValue); \
	parameter->value[1] = (parameterValue) >> 8; \
	parameter->value[2] = (parameterValue) >> 16; \
	parameter->value[3] = (parameterValue) >> 24; \
	cursor += sizeof(*parameter); \
} while (0)

int bbcmdStartNetwork(BbcmdRequest *request)
{
	BbcmdProtocolContext *protocol;
	BbcmdClientBinding *binding;
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolClient *client, *destination;
	BbcmdCompletion completion;
	unsigned int channel, bit, state, bits;
	unsigned int completedCommand;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 25)
		return 0x80330101;
	channel = request->channel;
	if (channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	bit = 1U << node->index;
	protocol = control->protocols[channel];
	binding = &protocol->bindings[2];
	if ((control->cancelMask & bit) != 0) {
		state = request->state;
		result = state & ~4U;
		if (result == 0) {
			binding->requestBits &= ~(1U << (node->index + 16));
			request->state = 0;
			bbcmdLockMutex(&control->mutex);
			completion = request->completion;
			if (completion != NULL)
				goto cancelCompleted;
			node = request->node;
			control = node->control;
			node->state = 2;
			bbcmdUnlockMutex(&control->mutex);
			return 0;
		}
		if (state <= 4)
			request->state = 5;
	}
	bbcmdLockMutex(&control->mutex);
	client = binding->client;
	if (client == NULL) {
		result = 0x80330305;
		goto failed;
	}
	switch (request->state) {
	case 0:
	case 4: {
		const BbmcNetworkStartOptions *options;
		QmiMessageHeader *message;
		SceUInt8 *parameters, *cursor;
		unsigned int size, length;

		bits = binding->requestBits;
		node = request->node;
		if ((SceUInt16)bits != 0) {
			result = 0;
			binding->requestBits = bits | (1U << (node->index + 16));
			goto unlock;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = (SceUInt16)bits;
		if (request->input[4].value != 0 &&
		    (options = request->input[5].pointer) != NULL) {
			size = options->usernameLength + options->passwordLength +
				options->apnLength + 47;
			parameters = client->request.data + sizeof(client->request.data) - 1 - size;
			cursor = parameters;
			if ((request->input[4].value & 1) != 0)
				APPEND_BYTE(48, options->technologyPreference);
			if ((request->input[4].value & 2) != 0)
				APPEND_BYTE(49, options->profileIndex);
			if ((request->input[4].value & 4) != 0)
				APPEND_WORD(16, options->primaryDnsAddress);
			if ((request->input[4].value & 8) != 0)
				APPEND_WORD(17, options->secondaryDnsAddress);
			if ((request->input[4].value & 16) != 0)
				APPEND_WORD(18, options->primaryNbnsAddress);
			if ((request->input[4].value & 32) != 0)
				APPEND_WORD(19, options->secondaryNbnsAddress);
			if ((request->input[4].value & 64) != 0) {
				QmiTlvHeader *parameter = (QmiTlvHeader *)cursor;

				parameter->type = 20;
				parameter->lengthLow = options->apnLength;
				parameter->lengthHigh = options->apnLength >> 8;
				memcpy(parameter + 1, options->apn, options->apnLength);
				cursor += options->apnLength + sizeof(*parameter);
			}
			if ((request->input[4].value & 128) != 0)
				APPEND_WORD(21, options->ipv4Address);
			if ((request->input[4].value & 256) != 0)
				APPEND_BYTE(22, options->authenticationPreference);
			if ((request->input[4].value & 512) != 0) {
				QmiTlvHeader *parameter = (QmiTlvHeader *)cursor;

				parameter->type = 23;
				parameter->lengthLow = options->usernameLength;
				parameter->lengthHigh = options->usernameLength >> 8;
				memcpy(parameter + 1, options->apn, options->apnLength);
				cursor += options->apnLength + sizeof(*parameter);
			}
			if ((request->input[4].value & 1024) != 0) {
				QmiTlvHeader *parameter = (QmiTlvHeader *)cursor;

				parameter->type = 24;
				parameter->lengthLow = options->passwordLength;
				parameter->lengthHigh = options->passwordLength >> 8;
				memcpy(parameter + 1, options->apn, options->apnLength);
			}
			destination = binding->client;
			if (destination->service != 1) {
				result = 0x80330002;
				goto failed;
			}
			length = destination->request.length;
			if (size + length + 4 > sizeof(destination->request.data)) {
				result = 0x80330009;
				goto failed;
			}
			message = (QmiMessageHeader *)(destination->request.data + length);
			message->lengthLow = size;
			message->lengthHigh = size >> 8;
			message->messageIdLow = 32;
			message->messageIdHigh = 0;
			destination->request.length = length + 4;
			if (size != 0) {
				memmove(message + 1, parameters, size);
				destination->request.length += size;
			}
		} else {
			if (client->service != 1) {
				result = 0x80330002;
				goto failed;
			}
			message = (QmiMessageHeader *)client->request.data;
			message->messageIdLow = 32;
			message->messageIdHigh = 0;
			message->lengthLow = 0;
			message->lengthHigh = 0;
			client->request.length = 4;
		}
		bbmcNotifyFeatureEvent(bbmcGetControl(), 5, 1);
		client = binding->client;
		size = request->input[0].value;
		parameters = client->request.data + sizeof(client->request.data) - 1 - 24;
		cursor = parameters;
		if (size != 0) {
			if ((request->input[0].value & 1) != 0)
				APPEND_BYTE(16, request->input[2].bytes[0]);
			if ((request->input[0].value & 2) != 0) {
				QmiWdsStatisticsTlv *parameter = (QmiWdsStatisticsTlv *)cursor;

				parameter->header.type = 17;
				parameter->header.lengthLow = 5;
				parameter->header.lengthHigh = 0;
				parameter->period = request->input[2].bytes[1];
				parameter->mask[0] = request->input[1].value;
				parameter->mask[1] = request->input[1].value >> 8;
				parameter->mask[2] = request->input[1].value >> 16;
				parameter->mask[3] = request->input[1].value >> 24;
				cursor += sizeof(*parameter);
			}
			if ((request->input[0].value & 4) != 0)
				APPEND_BYTE(18, request->input[2].bytes[2]);
			if ((request->input[0].value & 8) != 0)
				APPEND_BYTE(19, request->input[2].bytes[3]);
			if ((request->input[0].value & 16) != 0)
				APPEND_BYTE(21, request->input[3].bytes[0]);
			client = binding->client;
			size = cursor - parameters;
		}
		if (client->service != 1) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (size + length + 4 > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdLow = 1;
		message->lengthLow = size;
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		client->request.length = length + 4;
		if (size != 0) {
			memmove(message + 1, parameters, size);
			client->request.length += size;
		}
		client = binding->client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0)
			goto failed;
		request->state = 1;
		goto unlock;
	}
	case 1:
		if (request->result == (int)0x80330306) {
			request->state = 5;
		} else {
			result = bbcmdReadNetworkStartResponse(request, client);
			if (result < 0)
				goto failed;
			request->state = 2;
		}
		completedCommand = 25;
		node = request->node;
		result = sceKernelSetEventFlag(node->control->event, 1U << node->index);
		control = request->node->control;
		bbcmdUnlockMutex(&control->mutex);
		if (result < 0)
			goto failedUnlocked;
		goto checkComplete;
	case 2: {
		QmiWordValueTlv *parameter;
		QmiMessageHeader *message;
		unsigned int length;

		bits = binding->requestBits;
		node = request->node;
		if ((SceUInt16)bits != 0) {
			result = 0;
			binding->requestBits = bits | (1U << (node->index + 16));
			goto unlock;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		parameter = (QmiWordValueTlv *)&client->request.data[2067];
		parameter->header.lengthHigh = (SceUInt16)bits;
		parameter->value[0] = (SceUInt16)bits;
		parameter->value[2] = (SceUInt16)bits;
		parameter->value[3] = (SceUInt16)bits;
		parameter->header.type = 16;
		parameter->header.lengthLow = 4;
		parameter->value[1] = 33;
		destination = binding->client;
		client->request.length = (SceUInt16)bits;
		if (destination->service != 1) {
			result = 0x80330002;
			goto failed;
		}
		length = destination->request.length;
		if (length + 11 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdHigh = (SceUInt16)bits;
		message->lengthHigh = (SceUInt16)bits;
		message->messageIdLow = 45;
		message->lengthLow = 7;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, 7);
		client = binding->client;
		destination->request.length += 7;
		if (client->service != 1) {
			result = 0x80330002;
			goto failed;
		}
		length = client->request.length;
		if (length + 4 > sizeof(client->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = (SceUInt16)bits;
		message->lengthLow = (SceUInt16)bits;
		message->lengthHigh = (SceUInt16)bits;
		message->messageIdLow = 68;
		destination = binding->client;
		client->request.length = length + 4;
		destination->callbackContext = request;
		result = qmiSendClientRequest(destination);
		if (result < 0)
			goto failedResult;
		request->state = 3;
		goto unlock;
	}
	case 3:
		if (request->result != 0) {
			result = request->input[6].value;
			if (result == 0)
				goto unlock;
			request->input[6].value = result - 1;
			node = request->node;
			control = node->control;
			request->state = 2;
			result = sceKernelSetEventFlag(control->event, 1U << node->index);
			if (result < 0)
				goto failedResult;
		} else {
			result = bbcmdReadNetworkStartResponse(request, client);
			if (result < 0)
				goto failedResult;
			request->state = 0;
		}
		goto unlock;
	case 5: {
		QmiHalfwordValueTlv *parameter;
		QmiMessageHeader *message;
		unsigned int length;

		binding->requestBits |= 1U << request->node->index;
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		parameter = (QmiHalfwordValueTlv *)&client->request.data[2069];
		parameter->header.type = 1;
		parameter->header.lengthLow = 2;
		parameter->header.lengthHigh = 0;
		destination = binding->client;
		client->request.length = 0;
		parameter->value[0] = destination->transaction;
		parameter->value[1] = binding->client->transaction >> 8;
		destination = binding->client;
		if (destination->service != 1) {
			result = 0x80330002;
			goto failed;
		}
		length = destination->request.length;
		if (length + 9 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto failed;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 2;
		message->lengthLow = 5;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, 5);
		client = binding->client;
		destination->request.length += 5;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0)
			goto failed;
		request->state = 6;
		goto unlock;
	}
	case 6: {
		unsigned int cancelMask;

		result = request->result;
		request->state = 0;
		if (result < 0) {
			if (result == (int)0x80335004) {
				result = 0x80330306;
				request->result = result;
			}
			node = request->node;
			control = node->control;
			bit = 1U << node->index;
			cancelMask = control->cancelMask;
			if ((bit & cancelMask) == 0) {
				request->result = result;
				completedCommand = 25;
				goto unlockCompletion;
			}
		} else {
			node = request->node;
			control = node->control;
			bit = 1U << node->index;
			cancelMask = control->cancelMask;
			if ((bit & cancelMask) == 0) {
				request->result = 0x80335004;
				result = 0;
				completedCommand = 25;
				goto unlockCompletion;
			}
			result = 0;
		}
		completedCommand = 0;
		control->cancelMask = cancelMask & ~bit;
		goto unlockCompletion;
	}
	default:
		result = 0x80330102;
		goto failed;
	}

failedResult:
	request->result = result;
failed:
	completedCommand = 25;
	bbcmdUnlockMutex(&request->node->control->mutex);
failedUnlocked:
	request->result = result;
	request->state = 0;
	goto complete;
unlock:
	completedCommand = 25;
	control = request->node->control;
	bbcmdUnlockMutex(&control->mutex);
	goto checkComplete;
unlockCompletion:
	bbcmdUnlockMutex(&control->mutex);
	if (result < 0)
		goto failedUnlocked;
checkComplete:
	if (request->state != 0)
		return result;
complete:
	bbcmdLockMutex(&request->node->control->mutex);
	completion = request->completion;
	if (completion != NULL) {
		if (completedCommand == 0) {
		cancelCompleted:
			node = request->node;
			control = node->control;
			if (control->cancelMask == 0) {
				BbcmdArgument input[7] = { { .value = 0 }, { .value = 25 } };

				completion(request->channel, 0, request->result,
					input, NULL, request->completionArg);
			}
		} else {
			completion(request->channel, completedCommand, request->result,
				request->input, request->output, request->completionArg);
		}
	}
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	return result;
}
