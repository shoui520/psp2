/* QMI protocol-driver availability and channel validation. */
#include <stddef.h>
#include "bbcmd.h"
#include "qmi_backend.h"

/* name unknown: initialize only when the protocol driver is absent. */
int qmiInitializeBackend(void)
{
	if (qmiGetBackendControl() != NULL)
		return (int)0x80330004;
	return qmiCreateBackend();
}

/* name unknown: finalize the active protocol driver. */
int qmiFinalizeBackend(void)
{
	if (qmiGetBackendControl() != NULL)
		return qmiDestroyBackend();
	return (int)0x80330003;
}

/* name unknown: validate the selected transport before opening it. */
int qmiOpenTransport(unsigned int channel)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiConnectTransport(channel);
}

/* name unknown: validate the selected transport before closing it. */
int qmiCloseTransport(unsigned int channel)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiDisconnectTransport(channel);
}

/* name unknown: forward activation and its request completion callback. */
int qmiActivateChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiStartProtocolChannel(channel, completed, request);
}

/* name unknown: forward deactivation and its request completion callback. */
int qmiDeactivateChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiStopProtocolChannel(channel, completed, request);
}

/* name unknown: preserve both words forwarded to the driver callback. */
int qmiSuspendChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiPauseProtocolChannel(channel, forwardedValue1, forwardedValue2);
}

/* name unknown: preserve both words forwarded to the driver callback. */
int qmiResumeChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (channel > 1)
		return (int)0x80330002;
	return qmiResumeProtocolChannel(channel, forwardedValue1, forwardedValue2);
}

/* name unknown: acquire an instance for a non-null client record. */
int qmiAcquireClientInstance(BbcmdProtocolClient *client)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (client != NULL)
		return qmiGetClientInstance_internal(client);
	return (int)0x80330002;
}

/* name unknown: release the selected non-null client instance. */
int qmiReleaseClientInstance(BbcmdProtocolClient *client)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (client != NULL)
		return qmiReleaseClientInstanceInternal(client);
	return (int)0x80330002;
}

/* name unknown: reset the clients belonging to an active protocol driver. */
int qmiResetClientInstances(void)
{
	if (qmiGetBackendControl() != NULL)
		return qmiResetClientInstancesInternal();
	return (int)0x80330003;
}

/* name unknown: forward a non-null client request to its active instance. */
int qmiSendClientRequest(BbcmdProtocolClient *client)
{
	if (qmiGetBackendControl() == NULL)
		return (int)0x80330003;
	if (client != NULL)
		return qmiSendClientRequestInternal(client);
	return (int)0x80330002;
}
