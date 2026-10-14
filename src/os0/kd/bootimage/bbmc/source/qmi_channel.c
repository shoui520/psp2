/* Protocol-driver channel transitions. */
#include "qmi_backend.h"

extern int qmiOpenDeviceChannel(unsigned int channel);
extern int qmiCloseDeviceChannel(unsigned int channel);
extern int qmiInitializeChannelClient(unsigned int channel);
extern int qmiResetChannelFifo(unsigned int channel);
extern int qmiWaitControlTransaction(QmiClientInstance *instance);
extern int qmiActivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request);
extern int qmiDeactivateDeviceChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request);
extern int qmiSuspendDeviceChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2);
extern int qmiResumeDeviceChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2);

/* name unknown: open the device and initialize its protocol client. */
int qmiConnectTransport(unsigned int channel)
{
	int ret = qmiOpenDeviceChannel(channel);

	if (ret < 0)
		return ret;
	ret = qmiInitializeChannelClient(channel);
	if (ret < 0) {
		qmiCloseDeviceChannel(channel);
		return ret;
	}
	return 0;
}

/* name unknown: reset channel FIFO state before closing the device. */
int qmiDisconnectTransport(unsigned int channel)
{
	qmiResetChannelFifo(channel);
	qmiCloseDeviceChannel(channel);
	return 0;
}

/* name unknown: activate an idle channel and start the primary exchange. */
int qmiStartProtocolChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request, int result), BbcmdRequest *request)
{
	int ret;

	if ((SceUInt32)g_qmiBackendControl->channelState[channel] + 1 > 1)
		return (int)0x8033000c;
	ret = qmiActivateDeviceChannel(channel, completed, request);
	g_qmiBackendControl->channelState[channel] = ret >= 0 ? 1 : -1;
	if (channel == 0)
		return qmiWaitControlTransaction(qmiGetClientInstance(10));
	return ret;
}

/* name unknown: deactivate an active channel and record its result. */
int qmiStopProtocolChannel(unsigned int channel,
	int (*completed)(BbcmdRequest *request), BbcmdRequest *request)
{
	int ret;

	if (g_qmiBackendControl->channelState[channel] != 1)
		return (int)0x80330006;
	ret = qmiDeactivateDeviceChannel(channel, completed, request);
	if (ret < 0) {
		g_qmiBackendControl->channelState[channel] = -1;
		return ret;
	}
	g_qmiBackendControl->channelState[channel] = 0;
	return ret;
}

/* name unknown: suspend an active channel while preserving callback words. */
int qmiPauseProtocolChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2)
{
	int ret;

	if (g_qmiBackendControl->channelState[channel] != 1)
		return (int)0x80330006;
	ret = qmiSuspendDeviceChannel(channel, forwardedValue1, forwardedValue2);
	g_qmiBackendControl->channelState[channel] = ret >= 0 ? 2 : -1;
	return ret;
}

/* name unknown: resume a suspended channel while preserving callback words. */
int qmiResumeProtocolChannel(unsigned int channel,
	void *forwardedValue1, void *forwardedValue2)
{
	int ret;

	if (g_qmiBackendControl->channelState[channel] != 2)
		return (int)0x80330006;
	ret = qmiResumeDeviceChannel(channel, forwardedValue1, forwardedValue2);
	g_qmiBackendControl->channelState[channel] = ret >= 0 ? 1 : -1;
	return ret;
}

