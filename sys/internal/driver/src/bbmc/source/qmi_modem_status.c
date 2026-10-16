/* Cell location and network scan requests. */
#include <string.h>
#include <psp2kern/kernel/threadmgr.h>
#include "qmi_modem_status.h"

#define QMI_HALFWORD(bytes) ((bytes)[0] | ((bytes)[1] << 8))
#define QMI_WORD(bytes) ((bytes)[0] | ((bytes)[1] << 8) | \
	((bytes)[2] << 16) | ((SceUInt32)(bytes)[3] << 24))

int bbcmdGetCellLocationInfo(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdClientBinding *binding;
	BbcmdCompletion completion;
	BbcmdProtocolClient *client;
	unsigned int bit, cancelMask;
	int result = 0;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 34)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	bit = 1U << node->index;
	cancelMask = control->cancelMask;
	if ((bit & cancelMask) != 0) {
		unsigned int bits;

		control->cancelMask = cancelMask & ~bit;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		node = request->node;
		control = node->control;
		protocol = control->protocols[request->channel];
		binding = &protocol->bindings[9];
		bits = binding->requestBits & ~(1U << node->index);
		if ((bits >> 16) != 0)
			bits &= ~(1U << (node->index + 16));
		cancelMask = control->cancelMask;
		binding->requestBits = bits;
		if (cancelMask == 0)
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	protocol = control->protocols[request->channel];
	binding = &protocol->bindings[9];
	if (request->state == 1) {
		unsigned int offset, total;

		if (request->result < 0) {
			request->state = 0;
			goto unlock;
		}
		client = binding->client;
		if ((unsigned int)client->response.length <= 10) {
			request->state = 0;
			result = 0x80330002;
			goto failed;
		}
		total = client->response.length;
		offset = 0;
		while (offset + 11 <= (unsigned int)client->response.length) {
			const QmiResultResponse *response = (const QmiResultResponse *)
				(client->response.data + offset);
			unsigned int messageId = response->message.messageIdLow |
				(response->message.messageIdHigh << 8);
			unsigned int messageLength = response->message.lengthLow |
				(response->message.lengthHigh << 8);

			if ((response->result.resultLow | (response->result.resultHigh << 8)) == 0 &&
				messageLength > 7) {
				BbmcCellLocationInfo *info = request->output[0].pointer;
				unsigned int service = client->service;

				if (messageLength - 8 > 384 || service != qmiGetConfiguredService()) {
					result = 0x80330002;
					goto next_message;
				}
				if (messageId == 1) {
					unsigned int position = 0;
					unsigned int size = messageLength - 7;

					info->geran.cellId = -1;
					info->umts.cellId = -1;
					for (;;) {
						const QmiTlvHeader *tlv = (const QmiTlvHeader *)
							(response->parameters + position);
						unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);
						unsigned int end = position + length + sizeof(*tlv);
						unsigned int count, index;

						if (size < end)
							goto invalid_parameter;
						switch (tlv->type) {
						case 16: {
							const QmiGeranCellInfoValue *value =
								(const QmiGeranCellInfoValue *)(tlv + 1);

							if (length > 103)
								goto invalid_parameter;
							info->geran.cellId = QMI_WORD(value->cellId);
							info->geran.plmn[0] = value->plmn[0];
							info->geran.plmn[1] = value->plmn[1];
							info->geran.plmn[2] = value->plmn[2];
							info->geran.lac = QMI_HALFWORD(value->lac);
							info->geran.arfcn = QMI_HALFWORD(value->arfcn);
							info->geran.bsic = value->bsic;
							info->geran.timingAdvance = QMI_WORD(value->timingAdvance);
							info->geran.rxLevel = QMI_HALFWORD(value->rxLevel);
							count = value->neighborCount;
							if (count > 16)
								goto invalid_parameter;
							info->geran.neighborCount = count;
							for (index = 0; index < count; index++) {
								const QmiGeranNeighborValue *cell = &value->neighbors[index];
								BbmcGeranNeighbor *neighbor = &info->geran.neighbors[index];

								neighbor->cellId = QMI_WORD(cell->cellId);
								neighbor->plmn[0] = cell->plmn[0];
								neighbor->plmn[1] = cell->plmn[1];
								neighbor->plmn[2] = cell->plmn[2];
								neighbor->lac = QMI_HALFWORD(cell->lac);
								neighbor->arfcn = QMI_HALFWORD(cell->arfcn);
								neighbor->bsic = cell->bsic;
								neighbor->rxLevel = cell->rxLevel;
							}
							break;
						}
						case 17: {
							const QmiUmtsCellInfoValue *value =
								(const QmiUmtsCellInfoValue *)(tlv + 1);
							const QmiUmtsNeighborValue *cells =
								(const QmiUmtsNeighborValue *)&value->neighborCount;
							const QmiUmtsGeranNeighborsValue *geran;

							if (length >= 258)
								goto invalid_parameter;
							info->umts.cellId = QMI_HALFWORD(value->cellId);
							info->umts.plmn[0] = value->plmn[0];
							info->umts.plmn[1] = value->plmn[1];
							info->umts.plmn[2] = value->plmn[2];
							info->umts.lac = QMI_HALFWORD(value->lac);
							info->umts.uarfcn = QMI_HALFWORD(value->uarfcn);
							info->umts.primaryScramblingCode = QMI_HALFWORD(value->primaryScramblingCode);
							info->umts.rscp = QMI_HALFWORD(value->rscp);
							info->umts.ecio = QMI_HALFWORD(value->ecio);
							count = value->neighborCount;
							if (count > 16)
								goto invalid_parameter;
							info->umts.neighborCount = count;
							for (index = 0; index < count; index++) {
								const QmiUmtsNeighborValue *cell = &cells[index];
								BbmcUmtsNeighbor *neighbor = &info->umts.neighbors[index];

								neighbor->uarfcn = QMI_HALFWORD(cell->uarfcn);
								neighbor->primaryScramblingCode = QMI_HALFWORD(cell->primaryScramblingCode);
								neighbor->rscp = QMI_HALFWORD(cell->rscp);
								neighbor->ecio = QMI_HALFWORD(cell->ecio);
							}
							/* Sony includes the count byte in each neighbor list. */
							geran = (const QmiUmtsGeranNeighborsValue *)&cells[count];
							count = geran->count;
							if (count > 16)
								goto invalid_parameter;
							info->umts.geranNeighborCount = count;
							for (index = 0; index < count; index++) {
								const QmiUmtsGeranNeighborValue *cell =
									&((const QmiUmtsGeranNeighborValue *)&geran->count)[index];
								BbmcUmtsGeranNeighbor *neighbor = &info->umts.geranNeighbors[index];

								neighbor->arfcn = QMI_HALFWORD(cell->arfcn);
								neighbor->networkColorCode = cell->networkColorCode;
								neighbor->baseStationColorCode = cell->baseStationColorCode;
								neighbor->rssi = QMI_HALFWORD(cell->rssi);
							}
							break;
						}
						case 18: {
							const QmiCdmaCellInfoValue *value =
								(const QmiCdmaCellInfoValue *)(tlv + 1);

							if (length > 16)
								goto invalid_parameter;
							info->cdma.systemId = QMI_HALFWORD(value->systemId);
							info->cdma.networkId = QMI_HALFWORD(value->networkId);
							info->cdma.networkId = QMI_HALFWORD(value->baseStationId);
							info->cdma.referencePn = QMI_HALFWORD(value->referencePn);
							info->cdma.latitude = QMI_WORD(value->latitude);
							info->cdma.longitude = QMI_WORD(value->longitude);
							break;
						}
						default:
							break;
						}
						if (size <= end)
							break;
						position = end;
					}
				}
				result = 0;
			}
			goto next_message;
		 invalid_parameter:
			result = 0x80330302;
		 next_message:
			offset += messageLength + 4;
			if (total <= offset)
				break;
		}
		control = request->node->control;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		if (result != 0)
			goto error;
		goto completed;
	}
	if (request->state == 0 || request->state == 2) {
		unsigned int bits, zero, length;
		QmiMessageHeader *message;

		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto failed;
		}
		bits = binding->requestBits;
		zero = (SceUInt16)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 2;
			goto unlock;
		}
		binding->requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = zero;
		if (qmiGetConfiguredService() != client->service) {
			result = 0x80330002;
			request->result = result;
			goto failed_node;
		}
		length = client->request.length;
		if (length + 4 > sizeof(client->request.data)) {
			result = 0x80330009;
			request->result = result;
			goto failed_node;
		}
		message = (QmiMessageHeader *)(client->request.data + length);
		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 1;
		client->request.length = length + 4;
		client = binding->client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0)
			goto failed_node;
		request->state = 1;
		control = request->node->control;
		goto unlock;
	}
	result = 0x80330102;
	goto failed;
failed_node:
	control = request->node->control;
failed:
	bbcmdUnlockMutex(&control->mutex);
error:
	request->result = result;
	request->state = 0;
	goto completed;
unlock:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 34, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

/* The final cancellation notification has a three-argument call site. */
typedef void (*BbcmdScanCanceledCompletion)(SceUInt32 channel,
	SceUInt32 command, int result);

int bbcmdNetworkScan(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbmcNetworkScanContext *context;
	BbcmdCompletion completion;
	unsigned int bit, cancelMask;
	int result = 0;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 28)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	bit = 1U << node->index;
	if ((bit & control->cancelMask) != 0) {
		unsigned int state = request->state;

		if (state == 1) {
			request->state = 3;
		} else if ((state & ~2U) == 0 || state == 5) {
			unsigned int bits;

			request->state = 7;
			protocol = control->protocols[request->channel];
			binding = &protocol->bindings[1];
			bits = binding->requestBits;
			if ((bits >> 16) != 0)
				binding->requestBits = bits & ~(1U << (node->index + 16));
			goto completed;
		}
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	protocol = control->protocols[request->channel];
	binding = &protocol->bindings[1];
	switch (request->state) {
	case 0:
	case 2:
	case 5: {
		unsigned int bits, zero;
		QmiMessageHeader *message;

		client = binding->client;
		if (client == NULL) {
			result = 0x80330305;
			goto failed;
		}
		bits = binding->requestBits;
		zero = (SceUInt16)bits;
		if (zero != 0) {
			binding->requestBits = bits | (1U << (node->index + 16));
			request->state = 2;
			goto unlock;
		}
		context = request->commandContext;
		binding->requestBits = bits | (1U << node->index);
		if (context == NULL) {
			context = bbcmdAlloc(sizeof(*context));
			request->commandContext = context;
			if (context == NULL) {
				result = 0x80330005;
				goto failed_node;
			}
			context->attempts = zero;
			client = binding->client;
		}
		memset(client->request.data, 0, sizeof(client->request.data));
		client = binding->client;
		client->request.length = 0;
		if (client->service != 3) {
			result = 0x80330002;
			goto failed_node;
		}
		message = (QmiMessageHeader *)client->request.data;
		message->messageIdHigh = 0;
		message->lengthLow = 0;
		message->lengthHigh = 0;
		message->messageIdLow = 33;
		client->request.length = 4;
		client = binding->client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0)
			goto failed_node;
		node = request->node;
		context->attempts++;
		control = node->control;
		request->state = 1;
		goto unlock;
	}
	case 1:
	case 6: {
		unsigned int offset = 0, total;

		result = request->result;
		if (result < 0) {
			if (result != (int)0x80335004)
				goto failed;
			request->state = 4;
			request->result = 0;
			binding->requestBits |= 1U << node->index;
			result = 0;
			goto unlock;
		}
		client = binding->client;
		total = client->response.length;
		if (total <= 10) {
			result = 0x80330002;
			goto failed;
		}
		while (offset + 11 <= (unsigned int)client->response.length) {
			const QmiResultResponse *response = (const QmiResultResponse *)
				(client->response.data + offset);
			unsigned int messageLength = response->message.lengthLow |
				(response->message.lengthHigh << 8);
			unsigned int messageId = response->message.messageIdLow |
				(response->message.messageIdHigh << 8);

			if ((response->result.resultLow | (response->result.resultHigh << 8)) == 0 &&
				messageLength > 7) {
				unsigned int position = 0;
				unsigned int size = messageLength - 7;

				if (client->service != 3) {
					result = 0x80330002;
					goto failed_node;
				}
				if (messageId == 33) {
					do {
						const QmiTlvHeader *tlv = (const QmiTlvHeader *)
							(response->parameters + position);
						const QmiNetworkListValue *value =
							(const QmiNetworkListValue *)(tlv + 1);
						unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);
						BbmcNetworkScanInfo *output;
						unsigned int count, index;

						if (tlv->type == 16) {
							unsigned int entryOffset = 0;

							count = QMI_HALFWORD(value->count);
							if (count >= 10)
								count = 10;
							output = request->output[0].pointer;
							output->count = count;
							for (index = 0; index < output->count; index++) {
								const QmiScannedNetworkValue *network =
									(const QmiScannedNetworkValue *)(value->networks + entryOffset);
								BbmcScannedNetwork *entry = &output->networks[index];
								unsigned int status;

								entry->mcc = QMI_HALFWORD(network->mcc);
								entry->mnc = QMI_HALFWORD(network->mnc);
								status = network->status;
								entry->descriptionLength = network->descriptionLength;
								output = request->output[0].pointer;
								entry = &output->networks[index];
								memcpy(entry->description, network->description,
									entry->descriptionLength);
								output = request->output[0].pointer;
								entry = &output->networks[index];
								entry->forbiddenStatus = 0;
								entry->preferredStatus = 0;
								entry->availabilityStatus = 0;
								entry->currentStatus = 0;
								if (status & 0x40)
									entry->forbiddenStatus = 1;
								else if (status & 0x80)
									entry->forbiddenStatus = 2;
								if (status & 0x10)
									entry->preferredStatus = 1;
								else if (status & 0x20)
									entry->preferredStatus = 2;
								if (status & 4)
									entry->availabilityStatus = 1;
								else if (status & 8)
									entry->availabilityStatus = 2;
								if (status & 1)
									entry->currentStatus = 1;
								else if (status & 2)
									entry->currentStatus = 2;
								entryOffset += entry->descriptionLength + sizeof(*network);
							}
						} else if (tlv->type == 17) {
							const QmiNetworkRadioInterfaceValue *networks =
								(const QmiNetworkRadioInterfaceValue *)value->networks;

							output = request->output[0].pointer;
							if (output->count == 0) {
								result = 0x80330002;
								goto failed_node;
							}
							count = QMI_HALFWORD(value->count);
							if (count >= 10)
								count = 10;
							for (index = 0; index < count; index++) {
								BbmcScannedNetwork *entry = &output->networks[index];
								const QmiNetworkRadioInterfaceValue *network = &networks[index];

								if (entry->mcc == QMI_HALFWORD(network->mcc) &&
									entry->mnc == QMI_HALFWORD(network->mnc))
									entry->radioInterface = network->radioInterface;
							}
						}
						position += length + sizeof(*tlv);
					} while (position < size);
				}
			}
			offset += messageLength + 4;
			if (total <= offset)
				break;
		}
		context = request->commandContext;
		if (context != NULL && ((BbmcNetworkScanInfo *)request->output[0].pointer)->count == 0 &&
			context->attempts <= 1) {
			unsigned int mask;

			node = request->node;
			mask = 1U << node->index;
			binding->requestBits &= ~mask;
			result = ksceKernelSetEventFlag(node->control->event, mask);
			control = request->node->control;
			request->state = 5;
			bbcmdUnlockMutex(&control->mutex);
			if (result < 0)
				goto error;
			if (request->state == 7)
				goto completed;
			return result;
		}
		control = request->node->control;
		request->state = 7;
		result = 0;
		goto unlock;
	}
	case 3: {
		unsigned int bits = binding->requestBits;
		unsigned int mask = 1U << node->index;
		QmiHalfwordValueTlv *parameter;
		BbcmdProtocolClient *destination;
		QmiMessageHeader *message;
		unsigned int length;

		if ((bits & mask) == 0)
			binding->requestBits = bits | mask;
		memset(binding->client->request.data, 0, sizeof(binding->client->request.data));
		client = binding->client;
		parameter = (QmiHalfwordValueTlv *)&client->request.data[2069];
		parameter->header.lengthLow = 2;
		parameter->header.type = 1;
		parameter->header.lengthHigh = 0;
		client->request.length = 0;
		parameter->value[0] = binding->client->transaction;
		parameter->value[1] = binding->client->transaction >> 8;
		destination = binding->client;
		if (destination->service != 3) {
			result = 0x80330002;
			destination->callbackContext = request;
			goto failed_node;
		}
		length = destination->request.length;
		if (length + 9 > sizeof(destination->request.data)) {
			result = 0x80330009;
			destination->callbackContext = request;
			goto failed_node;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->messageIdHigh = 0;
		message->lengthHigh = 0;
		message->lengthLow = 5;
		message->messageIdLow = 1;
		destination->request.length = length + 4;
		memmove(message + 1, parameter, sizeof(*parameter));
		client = binding->client;
		destination->request.length += sizeof(*parameter);
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0)
			goto failed_node;
		control = request->node->control;
		request->state = 6;
		goto unlock;
	}
	case 4:
		result = 0;
		if (request->result >= 0)
			request->state = 7;
		goto unlock;
	default:
		result = 0x80330102;
		goto failed;
	}
failed_node:
	control = request->node->control;
failed:
	bbcmdUnlockMutex(&control->mutex);
error:
	request->result = result;
	request->state = 7;
	goto completed;
unlock:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 7)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	request->state = 0;
	control = node->control;
	node->state = 2;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if ((bit & cancelMask) == 0) {
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL)
			completion(request->channel, 28, request->result,
				request->input, request->output, request->completionArg);
	} else {
		cancelMask &= ~bit;
		protocol = control->protocols[request->channel];
		binding = &protocol->bindings[1];
		control->cancelMask = cancelMask;
		binding->requestBits &= ~bit;
		request->result = 0x80335004;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion != NULL) {
			if (cancelMask != 0) {
				completion(request->channel, 28, request->result,
					request->input, request->output, request->completionArg);
			} else {
				((BbcmdScanCanceledCompletion)completion)(request->channel, 28, 0x80335004);
				request->completion(request->channel, 0, request->result,
					request->input, request->output, request->completionArg);
			}
		}
	}
	if (request->commandContext != NULL)
		bbcmdFree(request->commandContext);
	return result;
}
