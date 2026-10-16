/* Received packet and control-client completion notifications. */
#include <psp2kern/kernel/threadmgr.h>
#include "qmi_backend.h"

/* name unknown: turn the packet category and control reply into an event. */
int qmiNotifyClientPacket(QmiClientInstance *instance, unsigned int packetType,
	const QmiReceivedHeader *header)
{
	BbcmdProtocolClient *client;
	unsigned int event;
	unsigned int message;

	if (instance == NULL || instance->client == NULL)
		return (int)0x80330002;
	client = instance->client;
	if (packetType == 1) {
		event = 3;
		message = client->response.data[0] | (client->response.data[1] << 8);
	} else if (packetType == 2) {
		event = 4;
		message = client->indication.data[0] | (client->indication.data[1] << 8);
	} else if (packetType == 0) {
		event = 2;
		message = client->request.data[0] | (client->request.data[1] << 8);
	} else {
		return (int)0x80330002;
	}
	if (header->service != 0)
		goto notify;
	if (message == 34) {
		if (packetType == 0)
			return 0;
		event = 0;
		client->clientId = header->controlClientId;
		goto notify;
	}
	if (message == 35) {
		if (packetType == 0)
			return 0;
		event = 1;
		goto notify;
	}
	if (packetType != 1)
		return header->service;
notify:
	instance->event = event;
	return qmiCompleteClientInstance(instance, header->service, packetType, 0);
}

/* name unknown: retain reported control-version values and wake the worker. */
int qmiControlClientCompleted(unsigned int event, BbcmdProtocolClient *client)
{
	QmiBackendControl *control;
	unsigned int channel;
	unsigned int bit;

	if (client == NULL)
		return (int)0x80330002;
	channel = client->channel;
	if (event == 3) {
		if (client->service != 0)
			return 0;
		if ((client->response.data[0] | (client->response.data[1] << 8)) == 33) {
			control = g_qmiBackendControl;
			control->reportedVersionByte1 = client->response.data[14];
			control->reportedVersionByte2 = client->response.data[15];
			control->reportedVersionWord1 = client->response.data[16] |
				(client->response.data[17] << 8);
			control->reportedVersionWord2 = client->response.data[18] |
				(client->response.data[19] << 8);
			if (client->service != 0)
				return 0;
		}
		if (client->result != 0) {
			if (channel > 1)
				return 0;
			bit = channel + 16;
			goto signal;
		}
	} else if (event > 1) {
			return 0;
	}
	if (channel > 1)
		return 0;
	bit = channel + 4;
signal:
	ksceKernelSetEventFlag(g_qmiBackendControl->controlEvent, 1U << bit);
	return 0;
}
