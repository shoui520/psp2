/* Register SIM refresh files with the configured service and UIM. */
#include <string.h>
#include "qmi_custom_messages.h"

extern const QmiUimFileSelection g_qmiUimFileSelections[21];

int bbcmdRegisterRefreshFiles(BbcmdRequest *request)
{
	BbcmdControl *control;
	BbcmdProtocolContext *protocol;
	BbcmdRequestNode *node;
	BbcmdCompletion completion;
	unsigned int bit, bits, state, requestIndex;
	int result;

	if (request == NULL)
		return 0x80330002;
	if (request->command != 32)
		return 0x80330101;
	if (request->channel > 1)
		return 0x80330002;
	node = request->node;
	control = node->control;
	requestIndex = node->index;
	bit = 1U << requestIndex;
	bits = control->cancelMask;
	if ((bit & bits) != 0) {
		unsigned int configuredBits, uimBits;

		protocol = control->protocols[request->channel];
		control->cancelMask = bits & ~bit;
		configuredBits = protocol->bindings[9].requestBits;
		if ((configuredBits & bit) != 0) {
			protocol->bindings[9].requestBits = configuredBits & ~bit;
		} else {
			uimBits = protocol->bindings[7].requestBits;
			if ((uimBits & bit) != 0) {
				protocol->bindings[7].requestBits = uimBits & ~bit;
			} else if (request->state == 2) {
				if ((configuredBits >> 16) != 0)
					protocol->bindings[9].requestBits = configuredBits &
						~(1U << (requestIndex + 16));
			} else if (request->state == 4 && (uimBits >> 16) != 0) {
				protocol->bindings[7].requestBits = uimBits &
					~(1U << (requestIndex + 16));
			}
		}
		result = 0;
		request->state = 0;
		bbcmdLockMutex(&control->mutex);
		node = request->node;
		control = node->control;
		node->state = 2;
		bbcmdUnlockMutex(&control->mutex);
		completion = request->completion;
		if (completion == NULL)
			return 0;
		if (request->node->control->cancelMask == 0) {
			completion(request->channel, 0, request->result, request->input,
				request->output, request->completionArg);
			request->completion(request->channel, 32, request->result,
				request->input, request->output, request->completionArg);
			return 0;
		}
		goto callback;
	}
	bbcmdLockMutex(&control->mutex);
	node = request->node;
	state = request->state;
	control = node->control;
	protocol = control->protocols[request->channel];
	switch (state) {
	case 0:
	case 2: {
		BbcmdProtocolClient *client = protocol->bindings[9].client;
		BbcmdProtocolClient *destination;
		QmiRefreshFileList *list;
		QmiMessageHeader *message;
		unsigned int count, size, length;
		unsigned int index;

		if (client == NULL) {
			result = 0x80330101;
			request->result = result;
			goto failed;
		}
		bits = protocol->bindings[9].requestBits;
		if ((SceUInt16)bits != 0) {
			result = 0;
			protocol->bindings[9].requestBits = bits | (1U << (node->index + 16));
			request->state = 2;
			goto unlock;
		}
		protocol->bindings[9].requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = protocol->bindings[9].client;
		client->request.length = (SceUInt16)bits;
		if (request->input[0].value == 1) {
			const BbmcRefreshFileList *selection = request->input[1].pointer;

			count = selection->fileCount;
			size = sizeof(*list) + count * sizeof(list->files[0]);
			list = (QmiRefreshFileList *)(client->request.data +
				sizeof(client->request.data) - 1 - size);
			list->header.type = 2;
			list->fileCountLow = count;
			list->header.lengthLow = 6 * count + 2;
			list->header.lengthHigh = (6 * count + 2) >> 8;
			list->fileCountHigh = count >> 8;
		} else if (request->input[0].value == 0) {
			count = 21;
			size = 131;
			list = (QmiRefreshFileList *)(client->request.data +
				sizeof(client->request.data) - 1 - size);
			list->header.lengthHigh = 0;
			list->fileCountHigh = 0;
			list->header.type = 2;
			list->header.lengthLow = 128;
			list->fileCountLow = 21;
		} else {
			result = 0x80330002;
			request->result = result;
			control = request->node->control;
			goto failed;
		}
		for (index = 0; index < count; index++) {
			QmiRefreshFileIdentity *file = &list->files[index];

			if (request->input[0].value != 0) {
				const BbmcRefreshFileList *selection = request->input[1].pointer;

				file->path[0][0] = selection->files[index].path[0] >> 8;
				selection = request->input[1].pointer;
				file->path[0][1] = selection->files[index].path[0];
				selection = request->input[1].pointer;
				file->path[1][0] = selection->files[index].path[1] >> 8;
				selection = request->input[1].pointer;
				file->path[1][1] = selection->files[index].path[1];
				selection = request->input[1].pointer;
				file->fileId[0] = selection->files[index].fileId >> 8;
				selection = request->input[1].pointer;
				file->fileId[1] = selection->files[index].fileId;
			} else {
				const QmiUimFileSelection *input = &g_qmiUimFileSelections[index];
				unsigned int path;

				file->path[0][0] = input->path[0] >> 8;
				file->path[0][1] = input->path[0];
				path = input->path[1];
				file->path[1][1] = path;
				file->path[1][0] = path >> 8;
				path = input->fileId;
				file->fileId[1] = path;
				file->fileId[0] = path >> 8;
			}
		}
		destination = protocol->bindings[9].client;
		if (qmiGetConfiguredService() != destination->service) {
			result = 0x80330002;
			goto configuredFailed;
		}
		length = destination->request.length;
		if (size + length + 4 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto configuredFailed;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->lengthLow = size;
		message->lengthHigh = size >> 8;
		message->messageIdLow = 2;
		message->messageIdHigh = 0;
		destination->request.length = length + 4;
		memmove(message + 1, list, size);
		client = protocol->bindings[9].client;
		destination->request.length += size;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		node = request->node;
		if (result < 0) {
			control = node->control;
			goto failed;
		}
		control = node->control;
		request->state = 1;
		goto unlock;

	configuredFailed:
		control = request->node->control;
		goto failed;
	}
	case 1:
	case 4: {
		BbcmdProtocolClient *client, *destination;
		QmiUimRefreshParameters *parameters;
		QmiMessageHeader *message;
		const QmiUimFileSelection *selection;
		unsigned int size, length, index;
		SceUInt8 *cursor;

		if (request->result < 0 && state == 1)
			goto reset;
		bits = protocol->bindings[7].requestBits;
		if ((SceUInt16)bits != 0) {
			result = 0;
			protocol->bindings[7].requestBits = bits | (1U << (node->index + 16));
			request->state = 4;
			goto unlock;
		}
		client = protocol->bindings[7].client;
		protocol->bindings[7].requestBits = bits | (1U << node->index);
		memset(client->request.data, 0, sizeof(client->request.data));
		client = protocol->bindings[7].client;
		client->request.length = (SceUInt16)bits;
		size = (SceUInt16)bits;
		for (index = 0; index < 21; index++)
			size += 2 * g_qmiUimFileSelections[index].pathCount + 3;
		parameters = (QmiUimRefreshParameters *)(client->request.data +
			sizeof(client->request.data) - 1 - (size + sizeof(*parameters)));
		parameters->session.header.type = 1;
		parameters->info.registerFlag = 1;
		parameters->info.header.lengthLow = size + 4;
		parameters->info.header.lengthHigh = (size + 4) >> 8;
		parameters->info.fileCountLow = 21;
		parameters->session.header.lengthLow = 2;
		parameters->info.header.type = 2;
		parameters->session.header.lengthHigh = 0;
		parameters->session.sessionType = 0;
		parameters->session.aidLength = 0;
		parameters->info.voteForInit = 0;
		parameters->info.fileCountHigh = 0;
		cursor = parameters->info.files;
		selection = g_qmiUimFileSelections;
		do {
			QmiUimRefreshFile *file = (QmiUimRefreshFile *)cursor;
			unsigned int count;

			file->fileId = selection->fileId;
			count = selection->pathCount;
			file->pathLength = 2 * count;
			if (count != 0) {
				file->path[0] = selection->path[0];
				if (count > 1)
					file->path[1] = selection->path[1];
			}
			selection++;
			cursor += 2 * count + 3;
		} while (selection != g_qmiUimFileSelections + 21);
		destination = protocol->bindings[7].client;
		size += sizeof(*parameters);
		if (destination->service != 11) {
			result = 0x80330002;
			goto uimFailed;
		}
		length = destination->request.length;
		if (size + length + 4 > sizeof(destination->request.data)) {
			result = 0x80330009;
			goto uimFailed;
		}
		message = (QmiMessageHeader *)(destination->request.data + length);
		message->lengthLow = size;
		message->lengthHigh = size >> 8;
		message->messageIdLow = 42;
		message->messageIdHigh = 0;
		destination->request.length = length + 4;
		if (size != 0) {
			memmove(message + 1, parameters, size);
			destination->request.length += size;
		}
		client = protocol->bindings[7].client;
		client->callbackContext = request;
		result = qmiSendClientRequest(client);
		if (result < 0) {
			control = request->node->control;
			goto failed;
		}
		node = request->node;
		request->state = 3;
		control = node->control;
		goto unlock;

	uimFailed:
		node = request->node;
		destination->callbackContext = request;
		control = node->control;
		goto failed;
	}
	case 3:
	reset:
		result = 0;
		request->state = 0;
		goto unlock;
	default:
		result = 0x80330102;
		goto failed;
	}

failed:
	bbcmdUnlockMutex(&control->mutex);
	request->result = result;
	request->state = 0;
	goto complete;
unlock:
	bbcmdUnlockMutex(&control->mutex);
	if (request->state != 0)
		return result;
complete:
	bbcmdLockMutex(&request->node->control->mutex);
	node = request->node;
	control = node->control;
	node->state = 2;
	bbcmdUnlockMutex(&control->mutex);
	completion = request->completion;
	if (completion == NULL)
		return result;
callback:
	completion(request->channel, 32, request->result, request->input,
		request->output, request->completionArg);
	return result;
}
