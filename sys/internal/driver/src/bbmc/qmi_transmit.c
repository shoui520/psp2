/* QMI transmit framing and transaction allocation. */
#include "qmi_backend.h"
#include "bbmc_control.h"
#include "qmi_fifo.h"

typedef struct QmiRequestTimeout {
	unsigned int service;
	unsigned int messageId;
	unsigned int timeoutMs;
} QmiRequestTimeout;

_Static_assert(sizeof(QmiRequestTimeout) == 12, "QMI request timeout record");

const QmiRequestTimeout g_qmiRequestTimeouts[3] = {
	{ .service = 3, .messageId = 33, .timeoutMs = 0 },
	{ .service = 1, .messageId = 2, .timeoutMs = 0 },
	{ .service = 3, .messageId = 1, .timeoutMs = 0 },
};

int qmiBuildTransmitHeader(unsigned int service, QmiClientInstance *instance,
	QmiTransmitHeader *header, unsigned int *length, unsigned int payloadLength);

/* name unknown: frame and queue an active client's request with its timeout. */
int qmiSendClientRequestInternal(BbcmdProtocolClient *client)
{
	QmiClientInstance *instance;
	BbcmdProtocolClient *storedClient;
	unsigned int service;
	unsigned int messageId;
	unsigned int delay;
	int timer;
	int locked;
	int ret;

	ret = qmiFindActiveClientInstance(client->clientId, client->service,
		client->channel);
	if (ret < 0)
		return ret;
	instance = qmiGetClientInstance(ret);
	service = client->service;
	instance->transmit.length = 9;
	{
		QmiTransmitHeader *header = &instance->transmit.wire;
		unsigned int payloadLength = instance->client->request.length;
		unsigned int headerLength;
		unsigned int packetLength;
		SceUInt16 transaction = 0;

		ret = qmiClassifyService(service);
		if (ret < 0)
			return ret;
		if (service != 0) {
			headerLength = 9;
			packetLength = 8;
		} else {
			headerLength = 8;
			packetLength = 7;
		}
		if ((SceUInt16)instance->transmit.length < headerLength)
			return (int)0x80330002;
		packetLength += payloadLength;
		header->lengthLow = packetLength;
		header->lengthHigh = packetLength >> 8;
		instance->transmit.length = headerLength;
		header->senderFlags = 0;
		header->service = service;
		header->marker = 1;
		locked = qmiLockBackend(1);
		if (qmiGetBackendControl() != NULL) {
			if (qmiGetBackendControl()->transaction == 0xffff)
				qmiGetBackendControl()->transaction = 1;
			else
				++qmiGetBackendControl()->transaction;
			if (service != 0)
				transaction = qmiGetBackendControl()->transaction;
			else
				transaction = (SceUInt8)qmiGetBackendControl()->transaction;
		}
		qmiUnlockBackend(1, locked);
		if (service != 0) {
			unsigned int clientId = instance->client->clientId;

			header->transactionLow = transaction;
			header->transactionHigh = transaction >> 8;
			header->clientId = clientId;
			header->messageFlags = 0;
		} else {
			header->transactionLow = transaction;
			header->clientId = 0;
			header->messageFlags = 0;
		}
		instance->transaction = transaction;
	}
	service = client->service;
	storedClient = instance->client;
	if (storedClient == NULL || (unsigned int)storedClient->request.length <= 1)
		goto enqueue;
	messageId = storedClient->request.data[0] |
		(storedClient->request.data[1] << 8);
	if (service == 3) {
		if (messageId == 33) {
			service = 0;
			goto selected;
		}
		if (messageId == 1) {
			service = 2;
			goto selected;
		}
	} else if (service == 1 && messageId == 2) {
		goto selected;
	}
	delay = 60000000;
	goto start;
selected:
	delay = 1000 * g_qmiRequestTimeouts[service].timeoutMs;
	if (delay == 0)
		goto enqueue;
start:
	timer = bbmcTimerSetHandler(delay, qmiClientRequestTimedOut, instance);
	locked = qmiLockBackend(1);
	instance->retryTimer = timer & ~(timer >> 31);
	qmiUnlockBackend(1, locked);
enqueue:
	ret = qmiEnqueueClientRequest(instance);
	if (ret == 0)
		client->transaction = instance->transaction;
	return ret;
}

/* name unknown: build control or service framing and allocate a transaction. */
int qmiBuildTransmitHeader(unsigned int service, QmiClientInstance *instance,
	QmiTransmitHeader *header, unsigned int *length, unsigned int payloadLength)
{
	unsigned int headerLength;
	unsigned int packetLength;
	SceUInt16 transaction = 0;
	int locked;
	int ret;

	if (instance == NULL || header == NULL)
		return (int)0x80330002;
	ret = qmiClassifyService(service);
	if (ret < 0)
		return ret;
	if (service != 0) {
		headerLength = 9;
		packetLength = 8;
	} else {
		headerLength = 8;
		packetLength = 7;
	}
	if ((SceUInt16)*length < headerLength)
		return (int)0x80330002;
	*length = headerLength;
	header->senderFlags = 0;
	packetLength += payloadLength;
	header->service = service;
	header->lengthLow = packetLength;
	header->marker = 1;
	header->lengthHigh = packetLength >> 8;
	locked = qmiLockBackend(1);
	if (qmiGetBackendControl() != NULL) {
		if (qmiGetBackendControl()->transaction == 0xffff)
			qmiGetBackendControl()->transaction = 1;
		else
			++qmiGetBackendControl()->transaction;
		if (service != 0)
			transaction = qmiGetBackendControl()->transaction;
		else
			transaction = (SceUInt8)qmiGetBackendControl()->transaction;
	}
	qmiUnlockBackend(1, locked);
	if (service != 0) {
		header->transactionLow = transaction;
		header->transactionHigh = transaction >> 8;
		header->clientId = instance->client->clientId;
		header->messageFlags = 0;
	} else {
		header->transactionLow = transaction;
		header->clientId = 0;
		header->messageFlags = 0;
	}
	instance->transaction = transaction;
	return 0;
}

/* name unknown: schedule the client's default or command-specific timeout. */
/* name unknown: notify the selected client after its queued packet is sent. */
int qmiNotifyTransmittedPacket(unsigned int channel, const void *packet)
{
	const QmiControlWirePacket *wire = packet;
	const QmiMessageHeader *message = (const void *)wire->payload;
	QmiReceivedHeader header = { 0 };
	unsigned int service = wire->header.service;
	int ret = service;

	header.service = service;
	if ((service != 0 || (SceUInt16)(message->messageIdLow - 34 +
		(message->messageIdHigh << 8)) > 1) &&
		(ret = qmiLookupClientInstance(channel, service,
			wire->header.clientId)) >= 0)
		return qmiNotifyClientPacket(qmiGetClientInstance(ret), 0, &header);
	return ret;
}

int qmiStartClientRequestTimer(unsigned int service, QmiClientInstance *instance)
{
	BbcmdProtocolClient *client;
	unsigned int messageId;
	unsigned int delay;
	unsigned int timer;
	int locked;

	if (instance == NULL)
		return (int)0x80330002;
	client = instance->client;
	if (client == NULL)
		return (int)0x80330002;
	if ((unsigned int)client->request.length <= 1)
		return (int)0x80330002;
	messageId = client->request.data[0] | (client->request.data[1] << 8);
	if (service == 3) {
		if (messageId == 33) {
			service = 0;
			goto selected;
		}
		if (messageId == 1) {
			service = 2;
			goto selected;
		}
	} else if (service == 1 && messageId == 2) {
		goto selected;
	}
	delay = 60000000;
	goto start;
selected:
	delay = 1000 * g_qmiRequestTimeouts[service].timeoutMs;
	if (delay == 0)
		return 0;
start:
	timer = bbmcTimerSetHandler(delay, qmiClientRequestTimedOut, instance);
	locked = qmiLockBackend(1);
	instance->retryTimer = timer & ((int)~timer >> 31);
	qmiUnlockBackend(1, locked);
	return timer;
}
