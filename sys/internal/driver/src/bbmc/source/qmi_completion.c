/* QMI client completion and instance lifetime. */
#include <psp2kern/kernel/threadmgr.h>
#include "qmi_backend.h"

/* name unknown: publish the result, notify the client, and finish release. */
int qmiCompleteClientInstance(QmiClientInstance *instance,
	unsigned int forwardedService, unsigned int packetType, int result)
{
	BbcmdProtocolClient *client;
	QmiBackendControl *control;
	int (*callback)(SceUInt32 event, BbcmdProtocolClient *client);
	SceUInt32 service;

	(void)forwardedService;
	if (instance == NULL || instance->client == NULL)
		return (int)0x80330002;
	client = instance->client;
	service = client->service;
	client->result = result;
	if (service == 0) {
		callback = instance->callback;
		if (callback == NULL) {
			if (instance->event != 1)
				return 0;
			goto finishRelease;
		}
	} else {
		callback = client->callback;
		if (callback == NULL)
			goto checkEvent;
	}
	callback(instance->event, client);
checkEvent:
	if (instance->event != 1)
		return 0;
finishRelease:
	if (packetType != 1)
		return 0;
	if (g_qmiBackendControl == NULL ||
	    ksceKernelLockMutex(g_qmiBackendControl->instanceMutex, 1, NULL) != 0) {
		instance->state = 0;
		instance->client = NULL;
		return 0;
	}
	control = g_qmiBackendControl;
	instance->state = 0;
	instance->client = NULL;
	if (control == NULL)
		return 0;
	ksceKernelUnlockMutex(control->instanceMutex, 1);
	return 0;
}
