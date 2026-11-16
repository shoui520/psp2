/*
 * SceNpBasic: the join game ack messages of the raw message queue. A message of the NP service's raw message queue (type
 * 2, 256 bytes at most) is given to the callback of the library's user as an event.
 */
#include "../impsutility/raw_message_client.h"

NpBasicJgaCallback g_jgaCallback;	/* 0x81007000 */

int npBasicJgaStart(NpBasicJgaCallback callback)
{
	int result;

	if (callback == NULL)
		return NP_BASIC_JGA_ERROR_INVALID_ARGUMENT;
	result = npImpsRawMsgStart(2, 256, 32, (NpImpsRawMsgCallback)npBasicJgaIpcCallback);
	if (result < 0)
		return result;
	g_jgaCallback = callback;
	return 0;
}

int npBasicJgaStop(void)
{
	g_jgaCallback = NULL;
	return npImpsRawMsgStop();
}

int npBasicJgaSendMessage(int requestId, const SceNpId *self, const SceNpId *peer, const void *data,
			  SceSize dataSize)
{
	NpBasicJgaSendMessage message;
	int result;

	if (dataSize + 96 > 0x100)
		return NP_BASIC_JGA_ERROR_MESSAGE_TOO_LARGE;
	sceClibMemset(&message, 0, 0x100);
	message.requestId = requestId;
	message.type = 3;
	sceClibMemcpy_safe(&message.self, self, sizeof(SceNpId));
	sceClibMemcpy_safe(&message.peer, peer, sizeof(SceNpId));
	message.dataSize = dataSize;
	if (data != NULL)
		sceClibMemcpy_safe(message.data, data, dataSize);
	result = npImpsRawMsgSend(&message, dataSize + 96);
	if (result >= 0)
		result = 0;
	return result;
}

int npBasicJgaPoll(void)
{
	NpBasicJgaMessage message;
	NpBasicJgaEvent event;
	int received;
	int result;

	sceClibMemset(&message, 0, 0x100);
	received = 0;
	result = npImpsRawMsgReceive(&message, 256, &received);
	if (result >= 0) {
		if (received != 0) {
			sceClibMemset(&event, 0, sizeof(event));
			if (message.type > 2) {
				if (message.type <= 3) {
					event.type = 2;
					event.requestId = message.requestId;
					event.peer = &message.npIdB;
					event.self = &message.npIdA;
					if (message.dataSize != 0)
						event.data = message.data;
					event.dataSize = message.dataSize;
					g_jgaCallback(&event);
				} else if (message.type <= 4) {
					event.type = 3;
					event.requestId = message.requestId;
					event.peer = *(const void **)&message.npIdA;
					g_jgaCallback(&event);
				}
			} else if (message.type > 0) {
				event.type = 1;
				event.requestId = message.requestId;
				event.peer = &message.npIdB;
				event.self = &message.npIdA;
				event.data = message.data;
				event.dataSize = message.dataSize;
				g_jgaCallback(&event);
			}
		}
		result = 0;
	}
	return result;
}

void npBasicJgaIpcCallback(const NpBasicJgaMessage *message)
{
	NpBasicJgaEvent event;

	sceClibMemset(&event, 0, sizeof(event));
	if (message->type > 2) {
		if (message->type <= 3) {
			event.type = 2;
			event.requestId = message->requestId;
			event.peer = &message->npIdB;
			event.self = &message->npIdA;
			if (message->dataSize != 0)
				event.data = message->data;
			event.dataSize = message->dataSize;
			g_jgaCallback(&event);
		} else if (message->type <= 4) {
			event.type = 3;
			event.requestId = message->requestId;
			event.peer = *(const void **)&message->npIdA;
			g_jgaCallback(&event);
		}
	} else if (message->type > 0) {
		event.type = 1;
		event.requestId = message->requestId;
		event.peer = &message->npIdB;
		event.self = &message->npIdA;
		event.data = message->data;
		event.dataSize = message->dataSize;
		g_jgaCallback(&event);
	}
}
