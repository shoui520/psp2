/* UIM card status and PIN state request. */
#include <string.h>
#include "qmi_uim_card_status.h"

int bbcmdGetUimCardStatus(BbcmdRequest *request)
{
	BbcmdRequestNode *node;
	BbcmdControl *control;
	BbcmdClientBinding *binding;
	BbcmdProtocolClient *client;
	BbcmdCompletion completion;
	unsigned int bit, bits;
	int result = 0;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 11)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	bit = 1U << node->index;
	if ((bit & control->cancelMask) != 0) {
		control->cancelMask &= ~bit;
		request->state = 3;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		request->state = 0;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		node = request->node;
		control = node->control;
		binding = &control->protocols[request->channel]->bindings[7];
		bits = binding->requestBits & ~(1U << node->index);
		if ((bits >> 16) != 0)
			bits &= ~(1U << (node->index + 16));
		result = control->cancelMask;
		binding->requestBits = bits;
		if (result == 0)
			completion(request->channel, 0, request->result,
				request->input, request->output, request->completionArg);
		return 0;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	control = node->control;
	binding = &control->protocols[request->channel]->bindings[7];
	if (request->state == 1) {
		const QmiMessageHeader *message;
		const QmiTlvHeader *tlv;
		unsigned int remaining;

		if (request->result < 0) {
			request->state = 3;
			goto unlock;
		}
		client = binding->client;
		message = (const QmiMessageHeader *)client->response.data;
		tlv = (const QmiTlvHeader *)(message + 1);
		remaining = (SceUInt16)(message->lengthLow | (message->lengthHigh << 8));
		if (remaining <= 2) {
			request->state = 3;
			result = 0x80330002;
			goto failed;
		}
		do {
			unsigned int length = tlv->lengthLow | (tlv->lengthHigh << 8);

			if (tlv->type == 16) {
				const QmiUimCardStatusPrefix *status =
					(const QmiUimCardStatusPrefix *)(tlv + 1);
				const QmiUimCardPrefix *card =
					(const QmiUimCardPrefix *)status->cards;
				BbmcPinFlags *flags = (BbmcPinFlags *)&request->output[0];
				unsigned int error = card->error;

				if (card->state == 0 ||
					(card->state == 2 && (error & 0xfd) == 1)) {
					flags->blocked = -1;
					flags->verified = -1;
					flags->enabled = 1;
					request->output[4].value = 1;
				} else if (card->state == 2) {
					request->output[3].value = 1;
					if (error == 0)
						request->output[5].value = 1;
				} else if (card->applicationCount != 0) {
					const QmiUimApplicationPrefix *application =
						(const QmiUimApplicationPrefix *)card->applications;
					const SceUInt8 *aidLengthAddress = &application->aidLength;
					unsigned int count = card->applicationCount;
					unsigned int applicationType = application->type;
					unsigned int applicationState = application->state;
					unsigned int aidLength = *aidLengthAddress;

					while (applicationType == 0) {
						application = (const QmiUimApplicationPrefix *)
							(aidLengthAddress + aidLength +
							 sizeof(QmiUimApplicationTail) + 1);
						aidLengthAddress = application->aid;
						if (--count == 0)
							goto next_parameter;
						/* Sony shifts the subsequent application view by one byte. */
						applicationType = application->state;
						applicationState = application->personalizationState;
						aidLength = *aidLengthAddress;
					}
					if (applicationState == 1 || applicationState == 4)
						request->output[5].value = 1;
					{
						const QmiUimApplicationTail *tail =
							(const QmiUimApplicationTail *)(aidLengthAddress + aidLength + 1);
						const QmiUimPinStatus *pin = request->input[0].value == 1 ?
							&tail->pin1 : &tail->pin2;
						unsigned int pinState = pin->state;

						*(SceUInt8 *)request->output[1].pointer = pin->verifyRetries;
						*(SceUInt8 *)request->output[2].pointer = pin->unblockRetries;
						switch (pinState) {
						case 0:
							request->output[3].value = 1;
							flags->verified = -1;
							break;
						case 1:
							flags->blocked = 0;
							flags->verified = 0;
							flags->enabled = 1;
							break;
						case 2:
							flags->blocked = 0;
							flags->verified = 1;
							flags->enabled = 1;
							break;
						case 3:
							flags->blocked = 0;
							flags->verified = 0;
							flags->enabled = 0;
							break;
						case 4:
							flags->blocked = 1;
							flags->verified = 0;
							flags->enabled = 1;
							break;
						case 5:
							flags->blocked = -1;
							flags->verified = -1;
							flags->enabled = 1;
							break;
						case 6:
							flags->blocked = 0;
							flags->verified = 0;
							break;
						default:
							request->output[3].value = 1;
							flags->enabled = -1;
							break;
						}
					}
				}
			}
next_parameter:
			tlv = (const QmiTlvHeader *)((const SceUInt8 *)(tlv + 1) + length);
			remaining -= length + sizeof(*tlv);
		} while (remaining != 0);
		request->state = 3;
		control = request->node->control;
		goto unlock;
	}
	if (request->state == 0 || request->state == 2) {
		QmiMessageHeader *message;
		unsigned int zero;

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
		client->request.length = 0;
		if (client->service != 11) {
			result = 0x80330002;
			goto failed_node;
		}
		message = (QmiMessageHeader *)client->request.data;
		message->messageIdHigh = zero;
		message->lengthLow = zero;
		message->lengthHigh = zero;
		message->messageIdLow = 47;
		client->request.length = sizeof(*message);
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
	bbcmdUnlockMutex(&control->mutex);
	goto failed_unlocked;
failed_node:
	control = request->node->control;
failed:
	bbcmdUnlockMutex(&control->mutex);
failed_unlocked:
	request->result = result;
	request->state = 3;
	goto completed;
unlock:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 3)
		return result;
completed:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	request->state = 0;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion != NULL)
		completion(request->channel, 11, request->result,
			request->input, request->output, request->completionArg);
	return result;
}
