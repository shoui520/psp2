/* QMI packet delivery to client storage. */
#include <string.h>
#include "qmi_backend.h"
#include "qmi_fifo.h"

const int g_qmiControlPacketTypes[3] = { 0, 1, 2 };
const int g_qmiServicePacketTypes[5] = { 0, 0, 1, 1, 2 };

/* name unknown: retain a received response or indication and notify its owner. */
int qmiStoreReceivedClientPacket(unsigned int index,
	const QmiReceivedHeader *header, unsigned int packetType,
	const void *packet, unsigned int length)
{
	QmiClientInstance *instance;
	BbcmdProtocolClient *client;
	const void *payload;
	unsigned int remaining;
	unsigned int payloadLength;

	if (packet == NULL)
		return (int)0x80330002;
	if (index > 11)
		return (int)0x80330002;
	instance = qmiGetClientInstance(index);
	client = instance->client;
	if (client == NULL)
		return (int)0x80330002;
	if (header->service != 0) {
		const QmiServiceWirePacket *wire = packet;

		payload = wire->payload;
		remaining = length - sizeof(wire->header);
	} else {
		const QmiControlWirePacket *wire = packet;

		payload = wire->payload;
		remaining = length - sizeof(wire->header);
	}
	if (remaining >= sizeof(client->response.data))
		payloadLength = sizeof(client->response.data);
	else
		payloadLength = remaining;
	if (packetType != 1) {
		if (packetType == 2) {
			memcpy(client->indication.data, payload, payloadLength);
			client->indication.length = payloadLength;
			return qmiNotifyClientPacket(instance, 2, header);
		}
		return (int)0x80330002;
	}
	memcpy(client->response.data, payload, payloadLength);
	client->response.length = payloadLength;
	return qmiNotifyClientPacket(instance, 1, header);
}

/* name unknown: match a parsed packet to an active or releasing client. */
int qmiMatchReceivedClient(unsigned int channel,
	const QmiReceivedHeader *header, int *packetType)
{
	unsigned int flags;
	unsigned int index = 0;
	int kind;

	if (packetType == NULL)
		return (int)0x80330002;
	flags = header->messageFlags;
	if (header->service != 0) {
		if (flags > 4)
			return (int)0x80330201;
		kind = g_qmiServicePacketTypes[flags];
	} else {
		if (flags > 2)
			return (int)0x80330201;
		kind = g_qmiControlPacketTypes[flags];
	}
	if (kind < 0)
		return kind;
	if (kind == 2) {
		for (; index != 12; ++index) {
			QmiClientInstance *instance = qmiGetClientInstance(index);
			BbcmdProtocolClient *client;

			if (instance->state - 2 > 1)
				continue;
			client = instance->client;
			if (client->channel != channel)
				continue;
			if (header->service == client->service) {
				if (header->clientId == client->clientId ||
				    (header->clientId == 255 && header->transaction == 0))
					goto found;
			} else if (header->service == 0 &&
				   client->service == header->controlService &&
				   header->transaction == instance->transaction) {
				goto found;
			}
		}
	} else {
		for (; index != 12; ++index) {
			QmiClientInstance *instance = qmiGetClientInstance(index);
			BbcmdProtocolClient *client;

			if (instance->state - 2 > 1)
				continue;
			client = instance->client;
			if (client->channel != channel)
				continue;
			if (header->service == client->service) {
				if (header->clientId == client->clientId)
					goto found;
			} else if (header->service == 0 &&
				   client->service == header->controlService &&
				   header->transaction == instance->transaction) {
				goto found;
			}
		}
	}
	index = 0x80330304;
found:
	*packetType = kind;
	return index;
}

/* name unknown: initialize shared packet queues before the channel client. */
int qmiInitializeChannelClient(unsigned int channel)
{
	int ret;

	if (channel > 1)
		return (int)0x80330002;
	if (qmiGetBackendControl()->fifoInitialized == 0) {
		ret = qmiInitializePacketFifos();
		if (ret < 0)
			return ret;
		qmiGetBackendControl()->fifoInitialized = 1;
	}
	return qmiInitializeControlClient(channel);
}

/* name unknown: allow the next channel setup to rebuild packet queues. */
int qmiResetChannelFifo(unsigned int channel)
{
	if (channel > 1)
		return (int)0x80330002;
	qmiGetBackendControl()->fifoInitialized = 0;
	return 0;
}
