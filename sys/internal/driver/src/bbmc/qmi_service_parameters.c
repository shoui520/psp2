/* QMI client initialization and service event subscriptions. */
#include <string.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include "qmi_backend.h"

extern int bbcmdSetRequestedMask(unsigned int channel, unsigned int mask);
extern int bbcmdSelectState(SceUInt32 *state, SceUInt32 flags);
extern int bbcmdUpdateCompletedMaskForState(BbcmdRequest *request, int state);
extern int qmiReadCountedResponseBytes(const QmiResultResponse *response,
	QmiCountedBytes *output);
extern int qmiAcquireClientInstance(BbcmdProtocolClient *client);
extern int bbcmdConfiguredServiceClientCallback(unsigned int event,
	BbcmdProtocolClient *client);
extern int bbcmdDmsClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdNasClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdQosClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdWmsClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdPdsClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdAtClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdCat2ClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdUimClientCallback(unsigned int event, BbcmdProtocolClient *client);
extern int bbcmdWdsClientCallback(unsigned int event, BbcmdProtocolClient *client);

int bbcmdInitializeClients(BbcmdRequest *request)
{
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdClientBinding *binding;
	BbcmdRequestNode *node;
	unsigned int state;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 9)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	bbcmdLockMutex(&request->node->control->mutex);
	state = request->state;
	control = request->node->control;
	protocol = control->protocols[request->channel];
	binding = protocol->bindings;
	switch (state) {
	case 0: {
		BbcmdProtocolClient *clients;
		unsigned int flags;
		int count, index;

		bbcmdSetRequestedMask(request->channel, request->input[0].value);
		flags = request->input[0].value;
		count = (flags & 1) != 0 ? 2 : 0;
		if ((flags & 2) != 0)
			count++;
		if ((flags & 4) != 0)
			count += 2;
		if ((flags & 8) != 0)
			count++;
		if ((flags & 16) != 0)
			count++;
		if ((flags & 32) != 0)
			count++;
		clients = bbcmdAlloc(sizeof(*clients) * count);
		if (clients == NULL) {
			result = 0x80330005;
			goto failed;
		}
		protocol->clientStorage = clients;
		flags = request->input[0].value;
		if (protocol->bindings == NULL) {
			result = 0x80330002;
			goto failed;
		}
		index = 0;
		if ((flags & 1) != 0) {
			protocol->bindings[9].client = clients + 1;
			protocol->bindings[0].client = clients;
			index = 2;
		}
		if ((flags & 2) != 0)
			protocol->bindings[1].client = clients + index++;
		if ((flags & 4) != 0) {
			protocol->bindings[8].client = clients + index;
			protocol->bindings[7].client = clients + index + 1;
			index += 2;
		}
		if ((flags & 8) != 0)
			protocol->bindings[2].client = clients + index++;
		if ((flags & 16) != 0)
			protocol->bindings[3].client = clients + index++;
		if ((flags & 32) != 0)
			protocol->bindings[4].client = clients + index;
		result = bbcmdSelectState(&request->state, request->input[0].value);
		if (result < 0)
			goto failed;
		node = request->node;
		goto signal;
	}
	case 1:
	case 3:
	case 6:
	case 9:
	case 11:
	case 13:
	case 15:
	case 18: {
		BbcmdProtocolClient *client;
		int (*callback)(unsigned int, BbcmdProtocolClient *);
		unsigned int service, bits;

		switch (state) {
		case 1: service = 2; break;
		case 3:
			service = qmiGetConfiguredService();
			if (service > 0x7ffffffeU) {
				result = 0x80330002;
				goto failed;
			}
			break;
		case 6: service = 3; break;
		case 9: service = 10; break;
		case 11: service = 11; break;
		case 13: service = 1; break;
		case 15: service = 5; break;
		default: service = 6; break;
		}
		if (qmiGetConfiguredService() < service) {
			result = 0x80330002;
			goto failed;
		}
		switch (service) {
		case 1:
			client = protocol->bindings[2].client;
			binding = &protocol->bindings[2];
			goto wdsCallback;
		case 2:
			callback = bbcmdDmsClientCallback;
			client = protocol->bindings[0].client;
			break;
		case 3:
			client = protocol->bindings[1].client;
			binding = &protocol->bindings[1];
			goto nasCallback;
		case 4:
			client = protocol->bindings[5].client;
			binding = &protocol->bindings[5];
			goto qosCallback;
		case 5:
			client = protocol->bindings[3].client;
			binding = &protocol->bindings[3];
			goto wmsCallback;
		case 6:
			client = protocol->bindings[4].client;
			binding = &protocol->bindings[4];
			goto pdsCallback;
		case 8:
			client = protocol->bindings[6].client;
			binding = &protocol->bindings[6];
			goto atCallback;
		case 10:
			client = protocol->bindings[8].client;
			binding = &protocol->bindings[8];
			goto cat2Callback;
		case 11:
			client = protocol->bindings[7].client;
			binding = &protocol->bindings[7];
			goto uimCallback;
		default:
			if (service != qmiGetConfiguredService()) {
				result = 0x80330002;
				goto failed;
			}
			client = protocol->bindings[9].client;
			binding = &protocol->bindings[9];
			switch (service) {
			case 1:
			wdsCallback:
				callback = bbcmdWdsClientCallback;
				break;
			case 2:
				callback = bbcmdDmsClientCallback;
				break;
			case 3:
			nasCallback:
				callback = bbcmdNasClientCallback;
				break;
			case 4:
			qosCallback:
				callback = bbcmdQosClientCallback;
				break;
			case 5:
			wmsCallback:
				callback = bbcmdWmsClientCallback;
				break;
			case 6:
			pdsCallback:
				callback = bbcmdPdsClientCallback;
				break;
			case 8:
			atCallback:
				callback = bbcmdAtClientCallback;
				break;
			case 10:
			cat2Callback:
				callback = bbcmdCat2ClientCallback;
				break;
			case 11:
			uimCallback:
				callback = bbcmdUimClientCallback;
				break;
			default:
				if (qmiGetConfiguredService() != service) {
					result = 0x80330002;
					goto failed;
				}
				callback = bbcmdConfiguredServiceClientCallback;
				break;
			}
			break;
		}
		bits = binding->requestBits;
		if ((SceUInt16)bits != 0) {
			node = request->node;
			result = 0;
			binding->requestBits = bits | (1U << (node->index + 16));
		} else {
			binding->requestBits = bits | (1U << request->node->index);
			client->channel = request->channel != 0;
			client->service = service;
			client->callback = callback;
			client->callbackContext = request;
			result = qmiAcquireClientInstance(client);
			node = request->node;
			if (result < 0) {
				control = node->control;
				binding->requestBits &= ~(1U << node->index);
				goto failedLocked;
			}
		}
		control = node->control;
		request->state++;
		break;
	}
	case 2:
	case 10:
	case 12:
	case 14:
	case 16:
	case 19:
		result = request->result;
		if (result < 0)
			goto failedLocked;
		result = bbcmdUpdateCompletedMaskForState(request, state);
		if (result < 0)
			goto failed;
		goto selectState;
	case 4: {
		BbcmdProtocolClient *client, *destination;
		QmiByteValueTlv *parameter;
		QmiMessageHeader *message;
		unsigned int length;

		result = request->result;
		if (result < 0)
			goto failedLocked;
		result = bbcmdUpdateCompletedMaskForState(request, 4);
		if (result < 0)
			goto failed;
		if ((request->input[0].value & 2) == 0)
			goto selectState;
		node = request->node;
		protocol->bindings[9].requestBits |= 1U << node->index;
		if (protocol->bindings == NULL) {
			result = 0x80330002;
			goto failedNode;
		}
		memset(protocol->bindings[9].client->request.data, 0,
			sizeof(protocol->bindings[9].client->request.data));
		client = protocol->bindings[9].client;
		parameter = (QmiByteValueTlv *)&client->request.data[2070];
		parameter->header.lengthLow = 1;
		parameter->value = 1;
		parameter->header.type = 2;
		parameter->header.lengthHigh = 0;
		destination = protocol->bindings[9].client;
		client->request.length = 0;
		if (qmiGetConfiguredService() != destination->service) {
			result = 0x80330002;
			goto failedConfigured;
		}
		length = destination->request.length;
		if (length + 8 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto failedConfigured;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 3;
		message->lengthLow = 4;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, 4);
		client = protocol->bindings[9].client;
		destination->request.length += 4;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		node = request->node;
		if (result < 0)
			goto failedNode;
		control = node->control;
		request->state = 5;
		break;

	failedConfigured:
		node = request->node;
		protocol->bindings[9].client->callbackContext = request;
		goto failedNode;
	}
	case 5:
	case 8:
		result = request->result;
		if (result < 0)
			goto failedLocked;
		if (state == 5) {
			if (request->output[0].pointer == NULL) {
				result = 0x80330002;
				goto failedLocked;
			}
			result = qmiReadCountedResponseBytes(
				(const QmiResultResponse *)protocol->bindings[9].client->response.data,
				request->output[0].pointer);
			if (result < 0)
				goto failed;
		}
		goto selectState;
	case 7: {
		BbcmdProtocolClient *client, *destination;
		QmiByteValueTlv *parameter;
		QmiMessageHeader *message;
		unsigned int length, service;

		result = request->result;
		if (result < 0)
			goto failedLocked;
		result = bbcmdUpdateCompletedMaskForState(request, 7);
		node = request->node;
		if (result < 0)
			goto failedNode;
		protocol->bindings[1].requestBits |= 1U << node->index;
		if (protocol->bindings == NULL) {
			result = 0x80330002;
			goto failedNode;
		}
		memset(protocol->bindings[1].client->request.data, 0,
			sizeof(protocol->bindings[1].client->request.data));
		client = protocol->bindings[1].client;
		parameter = (QmiByteValueTlv *)&client->request.data[2070];
		parameter->header.lengthLow = 1;
		parameter->header.type = 19;
		parameter->header.lengthHigh = 0;
		parameter->value = 0;
		destination = protocol->bindings[1].client;
		client->request.length = 0;
		service = destination->service;
		if (service != 3) {
			result = 0x80330002;
			goto failedNas;
		}
		length = destination->request.length;
		if (length + 8 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto failedNas;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdLow = service;
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->lengthLow = 4;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, 4);
		client = protocol->bindings[1].client;
		destination->request.length += 4;
		client->callbackContext = request;
		parameter = (QmiByteValueTlv *)&client->request.data[2070];
		parameter->header.lengthLow = 1;
		parameter->header.lengthHigh = 0;
		parameter->value = 1;
		parameter->header.type = 18;
		destination = protocol->bindings[1].client;
		if (destination->service != 3) {
			result = 0x80330002;
			goto failedNas;
		}
		length = destination->request.length;
		if (length + 8 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto failedNas;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->lengthLow = 4;
		message->messageIdLow = 2;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, 4);
		client = protocol->bindings[1].client;
		destination->request.length += 4;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		node = request->node;
		if (result < 0)
			goto failedNode;
		request->state = 8;
		control = node->control;
		break;

	failedNas:
		node = request->node;
		destination->callbackContext = request;
		goto failedNode;
	}
	case 17:
		result = 0;
		break;
	default:
		result = 0x80330102;
		goto failedLocked;
	}
	bbcmdUnlockMutex(&control->mutex);
	goto complete;

selectState:
	result = bbcmdSelectState(&request->state, request->input[0].value);
	if (result < 0)
		goto failed;
	node = request->node;
	if (request->state == 22) {
		bbcmdUnlockMutex(&node->control->mutex);
		goto complete;
	}
signal:
	result = sceKernelSetEventFlag(node->control->event, 1U << node->index);
	bbcmdUnlockMutex(&request->node->control->mutex);
	if (result >= 0)
		goto complete;
	goto failedUnlocked;
failedNode:
	control = node->control;
	goto failedLocked;
failed:
	control = request->node->control;
failedLocked:
	bbcmdUnlockMutex(&control->mutex);
failedUnlocked:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	request->result = result;
	request->state = 22;
	bbcmdUnlockMutex(&node->control->mutex);
complete:
	if (request->state == 22) {
		BbcmdCompletion completion;

		bbcmdLockMutex(&request->node->control->mutex);
		if (request->result == 0)
			bbcmdSetChannelState(request->channel, 3);
		node = request->node;
		completion = request->completion;
		node->state = 2;
		request->state = 0;
		if (completion != NULL) {
			completion(request->channel, 9, request->result, request->input,
				request->output, request->completionArg);
			node = request->node;
		}
		bbcmdUnlockMutex(&node->control->mutex);
	}
	return result;
}
