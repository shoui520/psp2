/* Device information and default WDS profile responses. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include "qmi_information.h"
#include <sysmem/sysclib.h>

const QmiEmptyMessageList g_qmiDeviceMessages = {
	4, { 33, 34, 35, 37, 48 }
};
const QmiEmptyMessageList g_qmiSubscriberMessages = {
	4, { 60, 48, 67, 36, 48 }
};
const QmiEmptyMessageList g_qmiHomeNetworkMessages = {
	1, { 37, 37, 37, 37, 37 }
};
const QmiEmptyMessageList g_qmiSmscMessages = {
	2, { 48, 52, 48, 48, 48 }
};

/* Both loads are observable when the parameter and output overlap. */
#define READ_PROFILE_WORD(destination, bytes) do { \
	(destination) = (bytes)[0] | ((bytes)[1] << 8) | \
		((bytes)[2] << 16) | ((SceUInt32)(bytes)[3] << 24); \
	(destination) = *(const SceUInt32 *)(bytes); \
} while (0)

#define READ_PROFILE_UMTS(member) do { \
	const QmiUmtsQosTlv *qos = (const QmiUmtsQosTlv *)tlv; \
	BbmcProfileSettings *profile = output[0].pointer; \
	profile->member.trafficClass = qos->trafficClass; \
	READ_PROFILE_WORD(profile->member.maxUplinkBitrate, \
		(const SceUInt8 *)&qos->maxUplinkBitrate); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.maxDownlinkBitrate, \
		(const SceUInt8 *)&qos->maxDownlinkBitrate); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.guaranteedUplinkBitrate, \
		(const SceUInt8 *)&qos->guaranteedUplinkBitrate); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.guaranteedDownlinkBitrate, \
		(const SceUInt8 *)&qos->guaranteedDownlinkBitrate); \
	profile = output[0].pointer; \
	profile->member.deliveryOrder = qos->deliveryOrder; \
	READ_PROFILE_WORD(profile->member.maximumSduSize, \
		(const SceUInt8 *)&qos->maximumSduSize); \
	profile = output[0].pointer; \
	profile->member.sduErrorRatio = qos->sduErrorRatio; \
	profile->member.residualBitErrorRatio = qos->residualBitErrorRatio; \
	profile->member.deliveryErroneousSdu = qos->deliveryErroneousSdu; \
	READ_PROFILE_WORD(profile->member.transferDelay, \
		(const SceUInt8 *)&qos->transferDelay); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.trafficHandlingPriority, \
		(const SceUInt8 *)&qos->trafficHandlingPriority); \
} while (0)

#define READ_PROFILE_GPRS(member) do { \
	const QmiGprsQosTlv *qos = (const QmiGprsQosTlv *)tlv; \
	BbmcProfileSettings *profile = output[0].pointer; \
	SceUInt32 reliability; \
	READ_PROFILE_WORD(profile->member.precedenceClass, qos->precedenceClass); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.delayClass, qos->delayClass); \
	reliability = *(const SceUInt32 *)qos->reliabilityClass; \
	profile = output[0].pointer; \
	if (reliability == 1) \
		reliability = 2; \
	profile->requestedGprsQos.reliabilityClass = reliability; \
	READ_PROFILE_WORD(profile->member.peakThroughputClass, qos->peakThroughputClass); \
	profile = output[0].pointer; \
	READ_PROFILE_WORD(profile->member.meanThroughputClass, qos->meanThroughputClass); \
} while (0)

int qmiReadDefaultProfileSettings(BbcmdArgument *output,
	const QmiTlvHeader *parameters, unsigned int size)
{
	unsigned int offset = 0;

	if (size <= 2)
		return 0x80330002;
	((BbmcProfileSettings *)output[0].pointer)->validFields = 0;
	do {
		const QmiTlvHeader *tlv = (const QmiTlvHeader *)
			((const SceUInt8 *)parameters + offset);
		const SceUInt8 *value = (const SceUInt8 *)(tlv + 1);
		unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);
		unsigned int count;
		BbmcProfileSettings *profile;

		switch (tlv->type) {
		case 0x10:
			count = length < 15 ? length : 15;
			profile = output[0].pointer;
			profile->profileNameLength = count;
			memcpy(((BbmcProfileSettings *)output[0].pointer)->profileName,
				value, count);
			break;
		case 0x11:
			((BbmcProfileSettings *)output[0].pointer)->pdpType = 0;
			break;
		case 0x14:
			count = length < 100 ? length : 100;
			memcpy(((BbmcProfileSettings *)output[0].pointer)->apn,
				value, count);
			profile = output[0].pointer;
			profile->apnLength = count;
			profile->validFields |= 4;
			break;
		case 0x15:
			profile = output[0].pointer;
			READ_PROFILE_WORD(profile->primaryDnsAddress, value);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 8;
			break;
		case 0x16:
			profile = output[0].pointer;
			READ_PROFILE_WORD(profile->secondaryDnsAddress, value);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 16;
			break;
		case 0x17:
			READ_PROFILE_UMTS(requestedUmtsQos);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 32;
			break;
		case 0x18:
			READ_PROFILE_UMTS(minimumUmtsQos);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 64;
			break;
		case 0x19:
			READ_PROFILE_GPRS(requestedGprsQos);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 128;
			break;
		case 0x1a:
			READ_PROFILE_GPRS(minimumGprsQos);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 256;
			break;
		case 0x1b:
			count = length < 128 ? length : 128;
			memcpy(((BbmcProfileSettings *)output[0].pointer)->username,
				value, count);
			profile = output[0].pointer;
			profile->usernameLength = count;
			profile->validFields |= 512;
			break;
		case 0x1c:
			count = length < 128 ? length : 128;
			memcpy(((BbmcProfileSettings *)output[0].pointer)->password,
				value, count);
			profile = output[0].pointer;
			profile->passwordLength = count;
			profile->validFields |= 1024;
			break;
		case 0x1d:
			count = value[0];
			profile = output[0].pointer;
			profile->papAuthentication = (count & 1) != 0;
			profile->chapAuthentication = (count & 2) != 0;
			profile->validFields |= 2048;
			break;
		case 0x1e:
			profile = output[0].pointer;
			READ_PROFILE_WORD(profile->ipv4Address, value);
			((BbmcProfileSettings *)output[0].pointer)->validFields |= 4096;
			break;
		case 0x1f:
			profile = output[0].pointer;
			profile->pcscfAddressUsingPco = value[0];
			profile->validFields |= 8192;
			break;
		default:
			break;
		}
		offset += length + sizeof(*tlv);
	} while (offset < size);
	return 0;
}

#undef READ_PROFILE_GPRS
#undef READ_PROFILE_UMTS
#undef READ_PROFILE_WORD

#define COPY_INFORMATION_STRING(field) do { \
	unsigned int count = length; \
	if (count >= sizeof(((BbmcDeviceInformation *)0)->field)) \
		count = sizeof(((BbmcDeviceInformation *)0)->field); \
	memcpy(((BbmcDeviceInformation *)request->output[0].pointer)->field, \
		value, count); \
	((BbmcDeviceInformation *)request->output[0].pointer)->field[ \
		sizeof(((BbmcDeviceInformation *)0)->field) - 1] = 0; \
} while (0)

int bbcmdReadInformationResponse(BbcmdRequest *request,
	BbcmdProtocolClient *client, int finalResponse)
{
	unsigned int offset, total;
	int result = request->result;

	if (result < 0 && result == (int)0x80330306) {
		request->state = 11;
		return result;
	}
	if (client == NULL || client->response.length <= 10) {
		result = 0x80330002;
		goto completed;
	}
	total = client->response.length;
	offset = 0;
	result = 0;
	do {
		const QmiResultResponse *response;
		const QmiTlvHeader *parameters;
		unsigned int messageId, messageLength, parameterSize;
		unsigned int position;

		if (offset + 11 > client->response.length)
			break;
		response = (const QmiResultResponse *)(client->response.data + offset);
		messageId = response->message.messageIdLow |
			(response->message.messageIdHigh << 8);
		messageLength = response->message.lengthLow |
			(response->message.lengthHigh << 8);
		if ((response->result.resultLow | (response->result.resultHigh << 8)) != 0) {
			unsigned int error = response->result.errorLow |
				(response->result.errorHigh << 8);
			unsigned int service = client->service;

			switch (error) {
			case 16:
				result = messageId == 36 && service == 2 ? 0 : 0x80335010;
				break;
			case 25:
				if ((service == 2 && (messageId == 48 || messageId == 67)) ||
					(service == 5 && messageId == 48))
					result = 0;
				else
					result = 0x80335019;
				break;
			case 26:
				result = messageId == 33 && service == 6 ? 0 : 0x8033501a;
				break;
			case 37:
				result = messageId == 37 && service == 3 ? 0 : 0x80335025;
				break;
			case 47:
				result = messageId == 52 && service == 5 ? 0 : 0x8033502f;
				break;
			default:
				result = error == 0 ? 0 : error | 0x80335000;
				break;
			}
			goto next_message;
		}
		if (messageLength <= 7)
			goto next_message;
		parameters = (const QmiTlvHeader *)response->parameters;
		parameterSize = (SceUInt16)(messageLength - 7);
		switch (client->service) {
		case 1:
			result = messageId == 44 ? qmiReadDefaultProfileSettings(
				&request->output[2], parameters, parameterSize) : 0;
			break;
		case 2:
			switch (messageId) {
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
			case 48:
			case 60:
			case 67:
				break;
			default:
				result = 0;
				goto next_message;
			}
			if (parameterSize <= 2) {
				result = 0x80330002;
				break;
			}
			result = 0;
			for (position = 0; position < parameterSize;) {
				const QmiTlvHeader *tlv = (const QmiTlvHeader *)
					((const SceUInt8 *)parameters + position);
				const SceUInt8 *value = (const SceUInt8 *)(tlv + 1);
				unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);

				if (messageId == 37) {
					switch (tlv->type) {
					case 16:
						COPY_INFORMATION_STRING(esn);
						break;
					case 17:
						COPY_INFORMATION_STRING(imei.digits);
						break;
					case 18:
						COPY_INFORMATION_STRING(meid);
						break;
					case 19: {
						unsigned int count = length < 3 ? length : 3;
						unsigned int index, number = 0;
						char shift = 4 * (count - 1);
						char digits[4] = { 0 };

#if FW_VERSION >= 0x04000000
						__memcpy_chk(digits, value, count, sizeof(digits));
#else
						memcpy(digits, value, count);
#endif
						if (count == 0) {
							result = 0x80330002;
							break;
						}
						for (index = 0; index != count; index++, shift -= 4) {
							unsigned int digit = (unsigned char)digits[index];
							unsigned int decimal = (unsigned char)(digit - '0');

							if (digit == 0)
								break;
							if (decimal <= 9) {
								if (digit != '0' && count > index)
									number += (char)decimal << shift;
							} else {
								if (digit - 'A' > 5)
									goto invalid_software_version;
								if (count > index)
									number += (unsigned char)(digit - 55) << shift;
							}
						}
						result = 0;
						if (number <= 255)
							((BbmcDeviceInformation *)request->output[0].pointer)->
								imeiSoftwareVersion = number;
						break;
					invalid_software_version:
						result = 0x80330002;
						break;
					}
					default:
						break;
					}
				} else if (tlv->type == 1) {
					switch (messageId) {
					case 33:
						COPY_INFORMATION_STRING(manufacturer);
						break;
					case 34:
						COPY_INFORMATION_STRING(model);
						break;
					case 35:
						COPY_INFORMATION_STRING(revision);
						break;
					case 36:
						COPY_INFORMATION_STRING(msisdn);
						break;
					case 48:
						((BbmcDeviceInformation *)request->output[0].pointer)->
							prlVersion = *(const SceUInt16 *)value;
						break;
					case 60:
						COPY_INFORMATION_STRING(iccid);
						break;
					case 67:
						COPY_INFORMATION_STRING(imsi);
						break;
					}
				}
				position += length + sizeof(*tlv);
			}
			break;
		case 3:
			if (messageId != 37) {
				result = 0;
				break;
			}
			if (parameterSize <= 2) {
				result = 0x80330002;
				break;
			}
			for (position = 0; position < parameterSize;) {
				const QmiTlvHeader *tlv = (const QmiTlvHeader *)
					((const SceUInt8 *)parameters + position);
				unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);

				if (tlv->type == 1) {
					const QmiHomeNetworkValue *value = (const QmiHomeNetworkValue *)(tlv + 1);
					unsigned int count;

					memset(&((BbmcDeviceInformation *)request->output[0].pointer)->
						homeNetwork, 0, sizeof(BbmcHomeNetworkInfo));
					((BbmcDeviceInformation *)request->output[0].pointer)->
						homeNetwork.mcc = *(const SceUInt16 *)&value->mccLow;
					((BbmcDeviceInformation *)request->output[0].pointer)->
						homeNetwork.mnc = *(const SceUInt16 *)&value->mncLow;
					count = value->descriptionLength;
					if (count >= 68)
						count = 68;
					memcpy(((BbmcDeviceInformation *)request->output[0].pointer)->
						homeNetwork.description, value->description, count);
					((BbmcDeviceInformation *)request->output[0].pointer)->
						homeNetwork.description[67] = 0;
				}
				position += length + sizeof(*tlv);
			}
			result = 0;
			break;
		case 5:
			if (messageId != 48 && messageId != 52) {
				result = 0;
				break;
			}
			if (parameterSize <= 2) {
				result = 0x80330002;
				break;
			}
			for (position = 0; position < parameterSize;) {
				const QmiTlvHeader *tlv = (const QmiTlvHeader *)
					((const SceUInt8 *)parameters + position);
				unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);

				if (tlv->type == 1) {
					if (messageId == 48) {
						((BbmcDeviceInformation *)request->output[0].pointer)->
							messageProtocol = ((const QmiByteValueTlv *)tlv)->value;
					} else {
						const QmiSmscAddressValue *value = (const QmiSmscAddressValue *)(tlv + 1);
						BbmcSmscAddress *address = request->output[1].pointer;
						unsigned int count;

						address->typeLength = 3;
						*(SceUInt16 *)address->type = *(const SceUInt16 *)value->type;
						address->type[2] = value->type[2];
						((BbmcSmscAddress *)request->output[1].pointer)->type[3] = 0;
						address = request->output[1].pointer;
						count = value->digitsLength;
						address->digitsLength = count;
						memcpy(address->digits, value->digits, count);
						((BbmcSmscAddress *)request->output[1].pointer)->digits[19] = 0;
					}
				}
				position += length + sizeof(*tlv);
			}
			result = 0;
			break;
		default:
			result = 0x80330002;
			break;
		}
	 next_message:
		offset += messageLength + 4;
	} while (offset < total);
completed:
	if (!finalResponse) {
		BbcmdRequestNode *node = request->node;

		result = sceKernelSetEventFlag(node->control->event, 1U << node->index);
		if (result < 0)
			request->state = 11;
	}
	return result;
}

#undef COPY_INFORMATION_STRING

#define SEND_INFORMATION_MESSAGES(bindingIndex, serviceId, list, nextState) do { \
	BbcmdClientBinding *binding = &protocol->bindings[bindingIndex]; \
	unsigned int bits = binding->requestBits; \
	unsigned int index = request->node->index; \
	unsigned int zero = (SceUInt16)bits; \
	BbcmdProtocolClient *client = binding->client; \
	QmiEmptyMessageList values = list; \
	QmiMessageHeader *messages; \
	if (zero != 0) { \
		result = 0; \
		binding->requestBits = bits | (1U << (index + 16)); \
	} else { \
		binding->requestBits = bits | (1U << index); \
		memset(client->request.data, 0, sizeof(client->request.data)); \
		client->request.length = zero; \
		if (client->service != serviceId) { \
			request->result = 0x80330002; \
			result = 0x80330002; \
			goto failed; \
		} \
		messages = (QmiMessageHeader *)client->request.data; \
		messages[0].messageIdLow = values.messageIds[0]; \
		messages[0].messageIdHigh = values.messageIds[0] >> 8; \
		messages[0].lengthLow = zero; \
		messages[0].lengthHigh = zero; \
		client->request.length = 4; \
		if (values.count > 1) { \
			messages[1].lengthLow = zero; \
			messages[1].lengthHigh = zero; \
			messages[1].messageIdLow = values.messageIds[1]; \
			messages[1].messageIdHigh = values.messageIds[1] >> 8; \
			client->request.length = 8; \
		} \
		client->callbackContext = request; \
		result = qmiSendClientRequest(client); \
		request->result = result; \
		if (result < 0) \
			goto failed; \
	} \
	control = request->node->control; \
	request->state = nextState; \
} while (0)

int bbcmdGetInformation(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdCompletion completion;
	unsigned int cancelMask, bit;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 14)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	cancelMask = control->cancelMask;
	bit = 1U << node->index;
	if ((bit & cancelMask) != 0) {
		BbcmdClientBinding *binding = NULL;
		unsigned int state = request->state;

		protocol = control->protocols[request->channel];
		control->cancelMask = cancelMask & ~bit;
		if (state - 4 <= 1) {
			if (qmiGetConfiguredService() > 4)
				binding = &protocol->bindings[3];
		} else if (state - 6 <= 1) {
			if (qmiGetConfiguredService() != 0)
				binding = &protocol->bindings[2];
		} else if (state - 8 <= 1) {
			if (qmiGetConfiguredService() > 2)
				binding = &protocol->bindings[1];
		} else if (qmiGetConfiguredService() > 1) {
			binding = &protocol->bindings[0];
		}
		request->state = 0;
		bbcmdLockMutex(&request->node->control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (binding == NULL) {
			result = 0x80330002;
			if (completion == NULL)
				return result;
		} else {
			unsigned int bits;

			if (completion == NULL)
				return 0;
			result = 0;
			node = request->node;
			bits = binding->requestBits & ~(1U << node->index);
			binding->requestBits = bits;
			if ((bits >> 16) != 0 && request->state != 10)
				binding->requestBits = bits & ~(1U << (node->index + 16));
		}
		if (request->node->control->cancelMask == 0)
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		return result;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	protocol = control->protocols[request->channel];
	if (protocol->bindings[0].client == NULL ||
		protocol->bindings[3].client == NULL ||
		protocol->bindings[2].client == NULL ||
		protocol->bindings[1].client == NULL) {
		result = 0x80330305;
		goto unlock_failed;
	}
	switch (request->state) {
	case 0:
		if (request->input[0].value == 1) {
			request->state = 2;
			goto subscriber_messages;
		}
		goto device_messages;
	case 1:
		result = bbcmdReadInformationResponse(request,
			protocol->bindings[0].client, request->input[0].value == 2);
		if (result < 0)
			goto failed;
		request->state = request->input[0].value == 2 ? 11 : 2;
		control = request->node->control;
		break;
	case 2:
	 subscriber_messages:
		result = bbcmdSendEmptyDmsMessages(request, g_qmiSubscriberMessages);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 3;
		break;
	case 3:
		result = bbcmdReadInformationResponse(request, protocol->bindings[0].client, 0);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 4;
		break;
	case 4:
		SEND_INFORMATION_MESSAGES(3, 5, g_qmiSmscMessages, 5);
		break;
	case 5:
		result = bbcmdReadInformationResponse(request, protocol->bindings[3].client, 0);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 6;
		break;
	case 6: {
		BbcmdClientBinding *binding = &protocol->bindings[2];
		BbcmdProtocolClient *client;
		QmiByteValueTlv *parameter;
		unsigned int bits, zero, index;

		memset(binding->client->request.data, 0, sizeof(binding->client->request.data));
		client = binding->client;
		parameter = (QmiByteValueTlv *)&client->request.data[2070];
		parameter->header.lengthHigh = 0;
		parameter->value = 0;
		parameter->header.type = 1;
		parameter->header.lengthLow = 1;
		client->request.length = 0;
		bits = binding->requestBits;
		node = request->node;
		index = node->index;
		zero = (SceUInt16)bits;
		if (zero != 0) {
			result = 0;
			binding->requestBits = bits | (1U << (index + 16));
		} else {
			BbcmdProtocolClient *destination = binding->client;
			QmiMessageHeader *message;
			unsigned int length;

			binding->requestBits = bits | (1U << index);
			if (destination->service != 1) {
				result = 0x80330002;
				goto failed;
			}
			length = destination->request.length;
			if (length + 8 > sizeof(destination->request.data)) {
				result = 0x80330009;
				goto failed;
			}
			message = (QmiMessageHeader *)(destination->request.data + length);
			message->messageIdHigh = zero;
			message->lengthHigh = zero;
			message->messageIdLow = 44;
			message->lengthLow = 4;
			destination->request.length = length + 4;
			memmove(message + 1, parameter, 4);
			client = binding->client;
			destination->request.length += 4;
			client->callbackContext = request;
			result = qmiSendClientRequest(client);
			if (result < 0)
				goto failed;
			node = request->node;
		}
		control = node->control;
		request->state = 7;
		break;
	}
	case 7:
		result = bbcmdReadInformationResponse(request, protocol->bindings[2].client, 0);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 8;
		break;
	case 8:
		SEND_INFORMATION_MESSAGES(1, 3, g_qmiHomeNetworkMessages, 9);
		break;
	case 9:
		result = bbcmdReadInformationResponse(request, protocol->bindings[1].client, 1);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 11;
		break;
	case 10:
	 device_messages:
		result = bbcmdSendEmptyDmsMessages(request, g_qmiDeviceMessages);
		if (result < 0)
			goto failed;
		control = request->node->control;
		request->state = 1;
		break;
	default:
		result = 0x80330102;
		goto unlock_failed;
	}
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 11)
		return result;
	goto completed;
failed:
	control = request->node->control;
unlock_failed:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 11;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 14, request->result,
			request->input, request->output, request->completionArg);
	return result;
}

#undef SEND_INFORMATION_MESSAGES
