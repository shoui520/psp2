/* Control-channel request progression and completion. */
#include "qmi_backend.h"
#include <threadmgr/threadmgr.h>

extern int qmiSignalBackend(unsigned int kind, unsigned int channel,
	unsigned int eventOffset);
extern int qmiWaitInterface(unsigned int kind, unsigned int channel,
	SceUInt32 timeout);
extern int qmiBuildTransmitHeader(unsigned int service,
	QmiClientInstance *instance, QmiTransmitHeader *header,
	unsigned int *length, unsigned int payloadLength);
extern int qmiStartClientRequestTimer(unsigned int service, QmiClientInstance *instance);
extern int qmiEnqueueClientRequest(QmiClientInstance *instance);

/* name unknown: advance one control exchange or complete its waiting client. */
int qmiAdvanceControlTransaction(QmiClientInstance *instance, unsigned int *stage)
{
	BbcmdProtocolClient *client;
	QmiControlScalarRequest *request;
	unsigned int current = *stage;
	unsigned int command;
	unsigned int hasParameter;
	unsigned int messageLength;
	unsigned int requestLength;
	unsigned char channelValue;
	int ret;

	if (current == 0) {
		ret = 0;
		goto completed;
	}
	if (current == 99) {
		if (instance->waitMode == 1) {
			ret = qmiSignalBackend(0, instance->client->channel, 0);
			goto completed;
		}
		ret = 0;
		if (instance->waitMode == 2 && instance->activationCallback != NULL &&
		    instance->activationRequest != NULL && *instance->activationCallback != NULL) {
			(*instance->activationCallback)(*instance->activationRequest,
				instance->client->result);
			*instance->activationCallback = NULL;
			*instance->activationRequest = NULL;
		}
		goto completed;
	}
	if (current == 1) {
		client = instance->client;
		client->request.length = 2075;
		request = (QmiControlScalarRequest *)client->request.data;
		command = 32;
		hasParameter = 1;
		messageLength = 4;
		requestLength = 8;
		request->message.messageIdHigh = 0;
		channelValue = client->channel + 1;
		request->message.messageIdLow = 32;
	} else {
		if (current == 2) {
			client = instance->client;
			command = client->channel == 1 ? 33 : 39;
		} else if (current == 3) {
			client = instance->client;
			command = 39;
		} else {
			ret = (int)0x8033000e;
			goto completed;
		}
		request = (QmiControlScalarRequest *)client->request.data;
		request->message.messageIdLow = command;
		current = 0;
		hasParameter = 0;
		messageLength = 3;
		requestLength = 7;
		channelValue = 0;
		request->message.messageIdHigh = 0;
		client->request.length = 2075;
	}
	request->message.lengthLow = messageLength;
	client->request.length = requestLength;
	request->message.lengthHigh = 0;
	if (hasParameter != 0) {
		request->parameter.lengthLow = current;
		request->parameter.lengthHigh = 0;
		request->parameter.type = 1;
	}
	switch (command) {
	case 32:
		__builtin_memcpy(&request->value, &channelValue, sizeof(channelValue));
		break;
	case 33:
	case 39:
		break;
	}
	client = instance->client;
	instance->transmit.length = 9;
	ret = qmiBuildTransmitHeader(0, instance, &instance->transmit.wire,
		&instance->transmit.length, client->request.length);
	if (ret < 0)
		goto completed;
	if (instance->retryTimer == 0)
		qmiStartClientRequestTimer(0, instance);
	ret = qmiEnqueueClientRequest(instance);
	if (ret < 0)
		goto completed;
	client = instance->client;
	if (ret == 0)
		client->transaction = instance->transaction;
	if (client->channel == 1) {
		if (*stage != 3) {
			(*stage)++;
			return ret;
		}
	} else if (*stage != 2) {
		(*stage)++;
		return ret;
	}
	*stage = 99;
	return ret;
completed:
	*stage = 0;
	return ret;
}

int qmiControlThread(SceSize args, void *argp)
{
	QmiBackendControl *control = *(QmiBackendControl **)argp;
	QmiClientInstance *instance;
	SceUInt32 flags = 0;
	unsigned int primaryStage = 0;
	unsigned int secondaryStage = 0;
	unsigned int advance;

	while (control->state != 0) {
		if (sceKernelWaitEventFlag(control->controlEvent, 0x80030333,
			5, &flags, NULL) < 0 || (SceInt32)flags < 0)
			break;
		if (flags & 0x10111) {
			instance = qmiGetClientInstance(10);
			advance = flags & 1;
			if (flags & 1) {
				advance = 1;
				primaryStage = 1;
			}
			if ((flags & 0x10) && primaryStage != 0)
				advance = 1;
			if ((flags & 0x10000) && primaryStage != 0) {
				primaryStage = 99;
			} else if (advance != 1) {
				goto secondary;
			}
			qmiAdvanceControlTransaction(instance, &primaryStage);
		}
secondary:
		if (flags & 0x20222) {
			instance = qmiGetClientInstance(11);
			advance = flags & 2;
			if (flags & 2) {
				advance = 1;
				secondaryStage = 1;
			}
			if ((flags & 0x20) && secondaryStage != 0)
				advance = 1;
			if ((flags & 0x20000) && secondaryStage != 0) {
				secondaryStage = 99;
			} else if (advance != 1) {
				continue;
			}
			qmiAdvanceControlTransaction(instance, &secondaryStage);
		}
	}
	return 0;
}

int qmiWaitControlTransaction(QmiClientInstance *instance)
{
	unsigned int channel;
	int ret;

	if (instance == NULL || instance->client == NULL)
		return (int)0x80330002;
	channel = instance->client->channel;
	ret = qmiSignalBackend(1, channel, 0);
	if (ret == 0 && instance->waitMode == 1) {
		ret = qmiWaitInterface(ret, channel, ret);
		if (ret == 0)
			return instance->client->result;
	}
	return ret;
}

int qmiSendControlCommand(QmiClientInstance *instance, unsigned int command,
	const void *arguments)
{
	QmiControlArgumentValues values = { 0 };
	BbcmdProtocolClient *client;
	QmiControlScalarRequest *request;
	QmiControlClientRequest *clientRequest;
	QmiControlExtendedRequest *extended;
	const QmiControlCommandOptions *options = arguments;
	unsigned int parameterLength;
	unsigned int messageLength;
	unsigned int requestLength;
	unsigned int extraLength = 0;
	int ret;

	switch (command) {
	case 32:
		client = instance->client;
		if (arguments != NULL)
			values.forwardedByte = *(const unsigned char *)arguments;
		parameterLength = 1;
		messageLength = 4;
		requestLength = 8;
		break;
	case 33:
	case 39:
		client = instance->client;
		parameterLength = 0;
		messageLength = 3;
		requestLength = 7;
		break;
	case 34:
		client = instance->client;
		values.service = client->service;
		client->clientId = 0;
		parameterLength = 1;
		messageLength = 4;
		requestLength = 8;
		break;
	case 35:
		client = instance->client;
		values.service = client->service;
		values.clientId = client->clientId;
		parameterLength = 2;
		messageLength = 5;
		requestLength = 9;
		break;
	case 38:
		if (options != NULL) {
			values.selector = options->selector;
			values.forwardedValue = options->forwardedValue;
		} else {
			values.selector = 1;
		}
		client = instance->client;
		parameterLength = 1;
		if (values.selector != 0) {
			messageLength = 4;
			requestLength = 8;
		} else {
			messageLength = 9;
			requestLength = 13;
			extraLength = 2;
		}
		break;
	default:
		return (int)0x8033000e;
	}
	client->request.length = 2075;
	request = (QmiControlScalarRequest *)client->request.data;
	request->message.messageIdLow = command;
	request->message.messageIdHigh = command >> 8;
	request->message.lengthLow = messageLength;
	client->request.length = requestLength;
	request->message.lengthHigh = 0;
	if (parameterLength != 0) {
		request->parameter.lengthLow = parameterLength;
		request->parameter.type = 1;
		request->parameter.lengthHigh = 0;
	}
	switch (command) {
	case 32:
		request->value = values.forwardedByte;
		break;
	case 33:
	case 39:
		break;
	case 34:
		request->value = values.service;
		break;
	case 35:
		clientRequest = (QmiControlClientRequest *)client->request.data;
		__builtin_memcpy(&clientRequest->client.service, &values.service, 2);
		break;
	case 38:
		request->value = values.selector;
		if (extraLength != 0) {
			extended = (QmiControlExtendedRequest *)client->request.data;
			extended->forwarded.type = 16;
			extended->forwarded.lengthLow = extraLength;
			extended->forwarded.lengthHigh = 0;
			extended->forwardedLow = values.forwardedValue;
			extended->forwardedHigh = values.forwardedValue >> 8;
		}
		break;
	default:
		return (int)0x8033000e;
	}
	client = instance->client;
	instance->transmit.length = 9;
	ret = qmiBuildTransmitHeader(0, instance, &instance->transmit.wire,
		&instance->transmit.length, client->request.length);
	if (ret >= 0) {
		if (instance->retryTimer == 0)
			qmiStartClientRequestTimer(0, instance);
		ret = qmiEnqueueClientRequest(instance);
		if (ret == 0)
			instance->client->transaction = instance->transaction;
	}
	return ret;
}
