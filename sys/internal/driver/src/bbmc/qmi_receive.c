/* QMI receive framing and client delivery. */
#include "qmi_backend.h"
#include "bbmc_control.h"

extern const int g_qmiControlPacketTypes[3];
extern const int g_qmiServicePacketTypes[5];

/* name unknown: parse transport framing and deliver matched client packets. */
int qmiReceiveClientPacket(unsigned int channel, const void *packet,
	unsigned int length)
{
	const QmiControlWirePacket *control = packet;
	const QmiServiceWirePacket *service = packet;
	const QmiMessageHeader *message;
	QmiReceivedHeader header;
	QmiClientInstance *instance;
	unsigned int start;
	int kind;
	int packetType = 0;
	int index;
	int locked;
	int ret;
	int lastResult = 0;

	if (packet == NULL)
		return (int)0x80330002;
	if (length <= 5)
		return (int)0x80330201;
	header.service = control->header.service;
	if (header.service != 0) {
		if (length <= 8)
			return (int)0x80330201;
		header.transaction = service->header.transactionLow |
			(service->header.transactionHigh << 8);
		message = (const QmiMessageHeader *)service->payload;
		header.messageId = message->messageIdLow | (message->messageIdHigh << 8);
		header.controlService = 0;
		header.controlClientId = 0;
	} else {
		if (length <= 11)
			return (int)0x80330201;
		header.transaction = control->header.transaction;
		message = (const QmiMessageHeader *)control->payload;
		header.messageId = message->messageIdLow | (message->messageIdHigh << 8);
		if ((unsigned int)(header.messageId - 34) <= 1) {
			const QmiControlClientResponse *response = packet;

			if (length <= 23)
				return (int)0x80330201;
			header.controlService = response->client.service;
			header.controlClientId = response->client.clientId;
		} else {
			header.controlService = 0;
			header.controlClientId = 0;
		}
	}
	header.length = control->header.lengthLow | (control->header.lengthHigh << 8);
	header.senderFlags = control->header.senderFlags;
	header.clientId = control->header.clientId;
	header.messageFlags = control->header.messageFlags;
	kind = 0;
	index = qmiMatchReceivedClient(channel, &header, &kind);
	if (index < 0)
		return index;
	instance = qmiGetClientInstance(index);
	if (kind == 1) {
		locked = qmiLockBackend(1);
		if (instance->retryTimer != 0) {
			ret = bbmcTimerCancelHandler(instance->retryTimer);
			instance->retryTimer = 0;
			qmiUnlockBackend(1, locked);
			if (ret < 0)
				return ret;
		} else {
			qmiUnlockBackend(1, locked);
		}
	}
	if (header.service != 0) {
		if (header.messageFlags > 4)
			return (int)0x80330201;
		ret = g_qmiServicePacketTypes[header.messageFlags];
	} else {
		if (header.messageFlags > 2)
			return (int)0x80330201;
		ret = g_qmiControlPacketTypes[header.messageFlags];
	}
	if (ret < 0)
		return ret;
	if (ret == 1) {
		if (header.service != 0) {
			if (length <= 19)
				return (int)0x80330201;
		} else if (length <= 18) {
			return (int)0x80330201;
		}
	}
	if (header.clientId != 255) {
		index = qmiMatchReceivedClient(channel, &header, &packetType);
		if (index < 0)
			return index;
		return qmiStoreReceivedClientPacket(index, &header, packetType,
			packet, length);
	}
	start = 0;
	while (start != 12) {
		if (header.service != 0) {
			if (header.messageFlags > 4)
				return (int)0x80330201;
			packetType = g_qmiServicePacketTypes[header.messageFlags];
		} else {
			if (header.messageFlags > 2)
				return (int)0x80330201;
			packetType = g_qmiControlPacketTypes[header.messageFlags];
		}
		if (packetType < 0)
			return packetType == (int)0x80330304 ? lastResult : packetType;
		if (packetType == 2) {
			for (index = start; index != 12; ++index) {
				BbcmdProtocolClient *client;

				instance = qmiGetClientInstance(index);
				if (instance->state - 2 > 1)
					continue;
				client = instance->client;
				if (client->channel != channel)
					continue;
				if (header.service == client->service) {
					if (header.clientId == client->clientId ||
					    (header.clientId == 255 && header.transaction == 0))
						break;
				} else if (header.service == 0 &&
					   client->service == header.controlService &&
					   header.transaction == instance->transaction) {
					break;
				}
			}
		} else {
			for (index = start; index != 12; ++index) {
				BbcmdProtocolClient *client;

				instance = qmiGetClientInstance(index);
				if (instance->state - 2 > 1)
					continue;
				client = instance->client;
				if (client->channel != channel)
					continue;
				if (header.service == client->service) {
					if (header.clientId == client->clientId)
						break;
				} else if (header.service == 0 &&
					   client->service == header.controlService &&
					   header.transaction == instance->transaction) {
					break;
				}
			}
		}
		if (index == 12)
			return lastResult;
		/* The original retries the same start when slot zero matches. */
		if (index == 0) {
			lastResult = 0;
			continue;
		}
		lastResult = qmiStoreReceivedClientPacket(index, &header, packetType,
			packet, length);
		if (lastResult < 0 || index > 11)
			return lastResult;
		start = index + 1;
	}
	return (int)0x80330002;
}
