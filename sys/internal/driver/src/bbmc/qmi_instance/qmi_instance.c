/* QMI client-instance synchronization and selection. */
#include <string.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/debug.h>
#include "../qmi_backend.h"

extern int qmiInitializeDeviceDriver(void);
extern int qmiFinalizeDeviceDriver(void);
extern int qmiControlThread(SceSize args, void *argp);
extern int qmiClassifyService(unsigned int service);

/* name unknown: allocate the protocol-driver synchronization resources. */
int qmiCreateBackend(void)
{
	int ret;

	memset(&g_qmiBackendStorage, 0, sizeof(g_qmiBackendStorage));
	ret = qmiInitializeDeviceDriver();
	if (ret < 0)
		goto failed;
	ret = ksceKernelCreateMutex("SceBbmcProtDrvInst", 2, 0, NULL);
	if (ret < 0)
		goto driver;
	g_qmiBackendStorage.instanceMutex = ret;
	ret = ksceKernelCreateMutex("SceBbmcProtDrvFifo", 2, 0, NULL);
	if (ret < 0)
		goto instance;
	g_qmiBackendStorage.fifoMutex = ret;
	ret = ksceKernelCreateEventFlag("SceBbmcProtDrvIf", 0x1000, 0, NULL);
	if (ret < 0)
		goto fifo;
	g_qmiBackendStorage.interfaceEvent = ret;
	ret = ksceKernelCreateEventFlag("SceBbmcProtDrvCtrl", 0x1000, 0, NULL);
	if (ret < 0)
		goto interface;
	g_qmiBackendStorage.controlEvent = ret;
	ret = ksceKernelCreateThread("SceBbmcProtDrv", qmiControlThread, 96,
		0x2000, 0x10000000, 0x80000, NULL);
	if (ret < 0)
		goto control;
	g_qmiBackendStorage.thread = ret;
	g_qmiBackendControl = &g_qmiBackendStorage;
	g_qmiBackendStorage.state = 1;
	ksceKernelStartThread(ret, sizeof(g_qmiBackendControl), &g_qmiBackendControl);
	return ret;
control:
	if (g_qmiBackendControl->controlEvent > 0)
		ksceKernelDeleteEventFlag(g_qmiBackendControl->controlEvent);
interface:
	if (g_qmiBackendControl->interfaceEvent > 0)
		ksceKernelDeleteEventFlag(g_qmiBackendControl->interfaceEvent);
fifo:
	if (g_qmiBackendStorage.fifoMutex > 0)
		ksceKernelDeleteMutex(g_qmiBackendStorage.fifoMutex);
instance:
	if (g_qmiBackendStorage.instanceMutex > 0)
		ksceKernelDeleteMutex(g_qmiBackendStorage.instanceMutex);
driver:
	qmiFinalizeDeviceDriver();
failed:
	memset(&g_qmiBackendStorage, 0, sizeof(g_qmiBackendStorage));
	g_qmiBackendControl = NULL;
	return ret;
}

/* name unknown: stop the protocol worker and destroy its owned resources. */
int qmiDestroyBackend(void)
{
	SceUInt32 timeout = 1000000;
	int ret;

	ret = ksceKernelSetEventFlag(g_qmiBackendControl->controlEvent, 0x80000000);
	g_qmiBackendControl->state = 0;
	if (ret >= 0)
		ret = ksceKernelWaitThreadEnd(g_qmiBackendControl->thread, NULL, &timeout);
	if (g_qmiBackendControl->fifoMutex > 0)
		ksceKernelDeleteMutex(g_qmiBackendControl->fifoMutex);
	if (g_qmiBackendControl->instanceMutex > 0)
		ksceKernelDeleteMutex(g_qmiBackendControl->instanceMutex);
	if (g_qmiBackendControl->controlEvent > 0)
		ksceKernelDeleteEventFlag(g_qmiBackendControl->controlEvent);
	if (g_qmiBackendControl->interfaceEvent > 0)
		ksceKernelDeleteEventFlag(g_qmiBackendControl->interfaceEvent);
	if (g_qmiBackendControl->thread > 0)
		ksceKernelDeleteThread(g_qmiBackendControl->thread);
	qmiFinalizeDeviceDriver();
	memset(&g_qmiBackendStorage, 0, sizeof(g_qmiBackendStorage));
	g_qmiBackendControl = NULL;
	return ret;
}

/* name unknown: return whether the selected driver mutex was acquired. */
int qmiLockBackend(unsigned int kind)
{
	if (g_qmiBackendControl != NULL) {
		if (kind == 1)
			return ksceKernelLockMutex(g_qmiBackendControl->instanceMutex,
				1, NULL) == 0;
		return kind == 2 &&
			ksceKernelLockMutex(g_qmiBackendControl->fifoMutex, 1, NULL) == 0;
	}
	return 0;
}

/* name unknown: release a mutex only when its acquisition token is one. */
int qmiUnlockBackend(unsigned int kind, int locked)
{
	if (locked == 1 && g_qmiBackendControl != NULL) {
		if (kind == 1)
			return ksceKernelUnlockMutex(g_qmiBackendControl->instanceMutex, 1);
		if (kind == 2)
			return ksceKernelUnlockMutex(g_qmiBackendControl->fifoMutex, 1);
	}
	return kind;
}

/* Sony: qmi_instance/qmi_instance.c. */
int qmiGetClientInstance_internal(BbcmdProtocolClient *client)
{
	QmiControlCommandOptions options;
	QmiClientInstance *instance;
	unsigned int index = 0;
	int locked;
	int ret;

	locked = qmiLockBackend(1);
	for (;;) {
		instance = &qmiGetBackendControl()->instances[index];
		if (instance->state == 0)
			break;
		if (++index == 10) {
			qmiUnlockBackend(1, locked);
			SCE_DBG_ASSERT(0, 51);
			return (int)0x80330303;
		}
	}
	qmiUnlockBackend(1, locked);
	locked = qmiLockBackend(1);
	instance = &qmiGetBackendControl()->instances[index];
	instance->client = client;
	instance->event = 5;
	instance->callback = qmiControlClientCompleted;
	instance->state = 1;
	instance->waitMode = 0;
	memset(&client->request, 0, sizeof(client->request));
	qmiUnlockBackend(1, locked);
	ret = qmiSendControlCommand(instance, 34, NULL);
	if (ret < 0) {
		locked = qmiLockBackend(1);
		instance->state = 0;
		qmiUnlockBackend(1, locked);
		return ret;
	}
	if (instance->client->channel == 1 && instance->client->service == 1) {
		QmiClientInstance *controlInstance =
			&qmiGetBackendControl()->instances[11];

		options.selector = 0;
		options.forwardedValue = 2;
		ret = qmiSendControlCommand(controlInstance, 38, &options);
		if (ret < 0)
			return ret;
	}
	locked = qmiLockBackend(1);
	instance->state = 2;
	qmiUnlockBackend(1, locked);
	return ret;
}

/* name unknown: locate the active client key and begin its release command. */
int qmiReleaseClientInstanceInternal(BbcmdProtocolClient *client)
{
	SceUInt32 clientId = client->clientId;
	SceUInt32 channel = client->channel;
	SceUInt32 service = client->service;
	unsigned int index = 0;
	int locked = qmiLockBackend(1);

	do {
		unsigned int current = index++;
		QmiClientInstance *instance =
			&qmiGetBackendControl()->instances[current];

		if (instance->state == 2 && instance->client->clientId == clientId &&
		    instance->client->service == service &&
		    instance->client->channel == channel) {
			int releaseLocked;

			qmiUnlockBackend(1, locked);
			releaseLocked = qmiLockBackend(1);
			instance = &qmiGetBackendControl()->instances[current];
			instance->state = 3;
			instance->callback = NULL;
			qmiUnlockBackend(1, releaseLocked);
			return qmiSendControlCommand(instance, 35, NULL);
		}
	} while (index != 12);
	qmiUnlockBackend(1, locked);
	return (int)0x80330303;
}

/* name unknown: discard non-control clients and reset each transmit header. */
int qmiResetClientInstancesInternal(void)
{
	unsigned int index = 0;
	int locked = qmiLockBackend(1);

	do {
		QmiClientInstance *instance =
			&qmiGetBackendControl()->instances[index++];
		BbcmdProtocolClient *client = instance->client;

		if (client != NULL && client->service != 0) {
			client->clientId = 0;
			memset(&client->request, 0, sizeof(client->request));
			memset(&instance->client->response, 0,
				sizeof(instance->client->response));
			memset(&instance->client->indication, 0,
				sizeof(instance->client->indication));
			instance->client = NULL;
			instance->state = 0;
			instance->callback = NULL;
			instance->waitMode = 0;
		}
		instance->reserved = 0;
		memset(&instance->transmit, 0, sizeof(instance->transmit));
	} while (index != 12);
	qmiUnlockBackend(1, locked);
	return 0;
}

/* name unknown: match an active client, retaining the service fallback value. */
int qmiLookupClientInstance(unsigned int channel, unsigned int service,
	unsigned int clientId)
{
	int selected;
	int locked;

	if (channel > 1)
		return (int)0x80330002;
	selected = qmiClassifyService(service);
	if (selected < 0)
		return selected;
	locked = qmiLockBackend(1);
	if (clientId != 0) {
		unsigned int index;

		for (index = 0; index != 12; ++index) {
			QmiClientInstance *instance =
				&qmiGetBackendControl()->instances[index];

			if (instance->state == 2 && instance->client->channel == channel &&
			    instance->client->service == service &&
			    instance->client->clientId == clientId) {
				selected = index;
				goto out;
			}
		}
	} else {
		unsigned int index;

		for (index = 0; index != 12; ++index) {
			QmiClientInstance *instance =
				&qmiGetBackendControl()->instances[index];

			if (instance->state == 2 && instance->client->channel == channel &&
			    instance->client->service == service) {
				selected = index;
				goto out;
			}
		}
	}
out:
	qmiUnlockBackend(1, locked);
	return selected;
}

/* name unknown: require a complete active client key instead of a fallback. */
int qmiFindActiveClientInstance(unsigned int clientId, unsigned int service,
	unsigned int channel)
{
	int index = 0;
	int locked = qmiLockBackend(1);

	for (; index != 12; ++index) {
		QmiClientInstance *instance =
			&qmiGetBackendControl()->instances[index];

		if (instance->state == 2 && instance->client->clientId == clientId &&
		    instance->client->service == service &&
		    instance->client->channel == channel)
			goto out;
	}
	index = (int)0x80330303;
out:
	qmiUnlockBackend(1, locked);
	return index;
}

/* name unknown: initialize the channel's permanent control-service client. */
int qmiInitializeControlClient(unsigned int channel)
{
	BbcmdProtocolClient *client;
	QmiBackendControl *control;
	QmiClientInstance *instance;
	int locked;

	if (channel == 0) {
		client = &g_qmiPrimaryControlClient;
		control = qmiGetBackendControl();
		memset(client, 0, sizeof(*client));
		client->channel = 0;
		locked = qmiLockBackend(1);
		instance = &control->instances[10];
		instance->event = 5;
		instance->client = client;
		instance->state = 2;
		instance->callback = qmiControlClientCompleted;
		instance->waitMode = 1;
	} else if (channel == 1) {
		client = &g_qmiSecondaryControlClient;
		control = qmiGetBackendControl();
		memset(client, 0, sizeof(*client));
		client->channel = 1;
		locked = qmiLockBackend(1);
		instance = &control->instances[11];
		instance->event = 5;
		instance->client = client;
		instance->callback = qmiControlClientCompleted;
		instance->state = 2;
		instance->waitMode = 2;
	} else {
		return (int)0x80330002;
	}
	client->service = 0;
	client->callback = NULL;
	client->callbackContext = instance;
	qmiUnlockBackend(1, locked);
	return 0;
}

/* name unknown: select one of the twelve complete instance records. */
QmiClientInstance *qmiGetClientInstance(unsigned int index)
{
	if (index > 11)
		return NULL;
	return &qmiGetBackendControl()->instances[index];
}

/* name unknown: expire a submitted client request from the timer worker. */
void qmiClientRequestTimedOut(void *arg)
{
	QmiClientInstance *instance = arg;
	BbcmdProtocolClient *client;

	if (instance == NULL)
		return;
	client = instance->client;
	if (client != NULL && (unsigned int)client->request.length > 1) {
		unsigned int service = (SceUInt8)client->service;

		instance->retryTimer = 0;
		instance->event = 3;
		qmiCompleteClientInstance(instance, service, 1, (int)0x80330306);
	}
}
