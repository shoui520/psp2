/*
 * SceDtcpIp: the middleware's two SW IPC clients. The system client carries the session
 * and key requests (the sink's connect, uri, stream setup); the stream client carries
 * the streamed chunks. They are separate compilations of the same SW IPC protocol, so
 * each has its own object, its own state and its own status mapping.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <string.h>
#include <libnetctl.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

/* the system client's object: the connection, the stream and the request state */
struct SceDtcpIpSystemClient {
	void *connection;
	void *stream;
	int state;
	unsigned char flag;
};

static struct SceDtcpIpSystemClient g_systemClient;

/* the stream client's object */
struct SceDtcpIpStreamSession {
	void *connection;
	void *stream;
	int state;
};

static struct SceDtcpIpStreamSession g_streamSession;

/* the SW IPC operations, per client (dtcpip_swipc.c) */
int dtcpIpSysInit(void **outClient, void *connection);
int dtcpIpSysSetUri(void *client, const char *uri, unsigned short port, unsigned char *flag);
int dtcpIpSysCloseSession(void *client);
int dtcpIpSysFlushStart(void *client);
int dtcpIpSysFlushDone(void *client);
int dtcpIpSysOpenStream(void **outStream, void *connection, const int *config);
int dtcpIpSysCloseStream(void *stream);
int dtcpIpSysGetInfo(void *stream, void *info);
int dtcpIpSysSetParameter(void *stream, int parameter);
int dtcpIpSysReceive(void *stream, void **buffer, SceSize *size, void *type, int timeout);
int dtcpIpSysRelease(void *stream, SceSize size);
int dtcpIpSysSend(void *stream, void **buffer, SceSize *size, int timeout);
int dtcpIpSysSendMessage(void *stream, int message);
int dtcpIpSysAbort(void *stream);
int dtcpIpSysIsStreamOpen(void *stream);
int dtcpIpSysClose(void *stream);

int dtcpIpStrmInit(void **outClient, void *connection);
int dtcpIpStrmSetUri(void *client, const char *uri, unsigned short port);
int dtcpIpStrmCloseSession(void *client);
int dtcpIpStrmFlushStart(void *client);
int dtcpIpStrmFlushDone(void *client);
int dtcpIpStrmOpenStream(void **outStream, void *connection, const int *config);
int dtcpIpStrmCloseStream(void *stream);
int dtcpIpStrmGetInfo(void *stream, void *info);
int dtcpIpStrmSetParameter(void *stream, int parameter);
int dtcpIpStrmReceive(void *stream, void **buffer, SceSize *size, void *type, int timeout);
int dtcpIpStrmRelease(void *stream, SceSize size);
int dtcpIpStrmSend(void *stream, void **buffer, SceSize *size, int timeout);
int dtcpIpStrmSendMessage(void *stream, int message);
int dtcpIpStrmAbort(void *stream);
int dtcpIpStrmIsStreamOpen(void *stream);

/* the stream engine's client table (the sink's session points at it) */
const struct SceDtcpIpStreamClient *dtcpIpGetStreamStreamClient(void)
{
	return &g_streamStreamClient;
}

/* the system client: connect, set the uri, open and close the session and stream */
static int systemConnect(void *connection)
{
	g_systemClient.connection = NULL;
	g_systemClient.stream = NULL;
	g_systemClient.state = 0;
	g_systemClient.flag = 0;
	return dtcpIpSysInit(&g_systemClient, connection);
}

static int systemSetUri(const char *uri, unsigned short port)
{
	int oldState;
	int result;
	int signalResult;

	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	oldState = g_systemClient.state;
	g_systemClient.state = 1;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	result = dtcpIpSysSetUri(g_systemClient.connection, uri, port, &g_systemClient.flag);
	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	if (g_systemClient.state == 3) {
		signalResult = dtcpIpSignalCond(SCE_DTCP_IP_COND_AUTH);
		if (signalResult < 0)
			result = signalResult;
	} else {
		if (result == 0)
			oldState = 2;
		g_systemClient.state = oldState;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	return result;
}

static int systemCloseSession(void)
{
	if (g_systemClient.connection != NULL) {
		int result = dtcpIpSysCloseSession(g_systemClient.connection);

		g_systemClient.connection = NULL;
		return result;
	}
	return 0;
}

static int systemFlush(void)
{
	int ret = 0;
	int result;
	SceUInt timeout = 10000000;

	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	if (g_systemClient.state == 1) {
		ret = dtcpIpSysFlushStart(g_systemClient.connection);
		g_systemClient.state = 3;
		result = dtcpIpWaitCond(SCE_DTCP_IP_COND_AUTH, &timeout);
		if (result < 0)
			ret = result;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	return ret;
}

static int systemOpen(int receiveSize, int sendSize, int blockSize, SceInt32 affinity)
{
	int config[5];

	config[0] = 0;
	config[1] = 0x20000;
	config[2] = 0x20000;
	config[3] = 0;
	config[4] = 0x8000;
	if (receiveSize != 0)
		config[1] = receiveSize;
	if (sendSize != 0)
		config[2] = sendSize;
	if (blockSize != 0)
		config[4] = blockSize;
	return dtcpIpSysOpenStream(&g_systemClient.stream, g_systemClient.connection, config);
}

static int systemCloseStream(void)
{
	if (g_systemClient.stream != NULL) {
		int result = dtcpIpSysCloseStream(g_systemClient.stream);

		g_systemClient.stream = NULL;
		return result;
	}
	return 0;
}

static int systemGetInfo(void)
{
	struct {
		int info[14];
		unsigned char flag;
		int zero;
		char buffer[512];
	} streamInfo;

	memset(streamInfo.info, 0, 16);
	streamInfo.info[13] = 0;
	streamInfo.flag = g_systemClient.flag;
	streamInfo.zero = 0;
	sceClibMemset(streamInfo.buffer, 0, 0x200);
	return dtcpIpSysGetInfo(g_systemClient.stream, &streamInfo);
}

static int systemSetParameter(int parameter)
{
	return dtcpIpSysSetParameter(g_systemClient.stream, parameter);
}

static int systemReceive(void **buffer, SceSize *size, unsigned char *type, int timeout)
{
	unsigned char receivedType[4];
	int result = dtcpIpSysReceive(g_systemClient.stream, buffer, size, receivedType, timeout);

	*type = receivedType[0];
	return result;
}

static int systemRelease(SceSize size)
{
	return dtcpIpSysRelease(g_systemClient.stream, size);
}

static int systemSend(void **buffer, SceSize *size, int timeout)
{
	return dtcpIpSysSend(g_systemClient.stream, buffer, size, timeout);
}

static int systemSendMessage(int message)
{
	return dtcpIpSysSendMessage(g_systemClient.stream, message);
}

static int systemAbort(void)
{
	return dtcpIpSysAbort(g_systemClient.stream);
}

static int systemIsStreamOpen(void)
{
	return dtcpIpSysIsStreamOpen(g_systemClient.stream);
}

static int systemClose(void)
{
	return dtcpIpSysClose(g_systemClient.stream);
}

static int systemIsSessionOpen(void)
{
	return g_systemClient.stream != NULL;
}

static int systemGetMoveHeader(void)
{
	return g_systemClient.flag;
}

/* whether the middleware's connection is a WLAN that is not secured */
int dtcpIpIsWlanConnection(int *outIsWlan, int *outIsSecured)
{
	SceNetCtlInfo info;
	int state = 0;
	int result;

	if (outIsWlan == NULL || outIsSecured == NULL)
		return SCE_DTCP_IP_ERROR_GENERIC;
	*outIsWlan = 0;
	*outIsSecured = 0;
	result = sceNetCtlInetGetState(&state);
	if (result >= 0 && state == SCE_NET_CTL_STATE_IPOBTAINED) {
		result = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info);
		if (info.device == SCE_NET_CTL_DEVICE_WIRELESS) {
			*outIsWlan = 1;
			result = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_WIFI_SECURITY, &info);
			if (result >= 0) {
				switch (info.wifi_security) {
				case SCE_NET_CTL_WIFI_SECURITY_WEP:
				case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_WPA2PSK:
				case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_TKIP:
				case SCE_NET_CTL_WIFI_SECURITY_WPAPSK_AES:
				case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_TKIP:
				case SCE_NET_CTL_WIFI_SECURITY_WPA2PSK_AES:
					*outIsSecured = 1;
					break;
				default:
					break;
				}
			}
		}
	}
	return result;
}

/* the system engine's client table (the move sink's session points at it) */
const struct SceDtcpIpStreamClient *dtcpIpGetSystemStreamClient(void)
{
	return &g_systemStreamClient;
}

/* the stream client */
static int streamConnect(void *connection)
{
	g_streamSession.connection = NULL;
	g_streamSession.stream = NULL;
	g_streamSession.state = 0;
	return dtcpIpStrmInit(&g_streamSession, connection);
}

static int streamSetUri(const char *uri, unsigned short port)
{
	int oldState;
	int result;
	int signalResult;

	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	oldState = g_streamSession.state;
	g_streamSession.state = 1;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	result = dtcpIpStrmSetUri(g_streamSession.connection, uri, port);
	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	if (g_streamSession.state == 3) {
		signalResult = dtcpIpSignalCond(SCE_DTCP_IP_COND_AUTH);
		if (signalResult < 0)
			result = signalResult;
	} else {
		if (result == 0)
			oldState = 2;
		g_streamSession.state = oldState;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	return result;
}

static int streamCloseSession(void)
{
	if (g_streamSession.connection != NULL) {
		int result = dtcpIpStrmCloseSession(g_streamSession.connection);

		g_streamSession.connection = NULL;
		return result;
	}
	return 0;
}

static int streamFlush(void)
{
	int ret = 0;
	int result;
	SceUInt timeout = 10000000;

	dtcpIpLock(SCE_DTCP_IP_LOCK_AUTH);
	if (g_streamSession.state == 1) {
		ret = dtcpIpStrmFlushStart(g_streamSession.connection);
		g_streamSession.state = 3;
		result = dtcpIpWaitCond(SCE_DTCP_IP_COND_AUTH, &timeout);
		if (result < 0)
			ret = result;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_AUTH);
	return ret;
}

static int streamOpen(int receiveSize, int sendSize, int blockSize, SceInt32 affinity)
{
	int config[6];

	config[0] = 0;
	config[1] = 0x20000;
	config[2] = 0x20000;
	config[3] = 0;
	config[4] = 0x8000;
	config[5] = affinity;
	if (receiveSize != 0)
		config[1] = receiveSize;
	if (sendSize != 0)
		config[2] = sendSize;
	if (blockSize != 0)
		config[4] = blockSize;
	return dtcpIpStrmOpenStream(&g_streamSession.stream, g_streamSession.connection, config);
}

static int streamCloseStream(void)
{
	if (g_streamSession.stream != NULL) {
		int result = dtcpIpStrmCloseStream(g_streamSession.stream);

		g_streamSession.stream = NULL;
		return result;
	}
	return 0;
}

static int streamGetInfo(void)
{
	int streamInfo[14];

	memset(streamInfo, 0, 16);
	streamInfo[13] = 0;
	return dtcpIpStrmGetInfo(g_streamSession.stream, streamInfo);
}

static int streamSetParameter(int parameter)
{
	return dtcpIpStrmSetParameter(g_streamSession.stream, parameter);
}

static int streamReceive(void **buffer, SceSize *size, unsigned char *type, int timeout)
{
	unsigned char receivedType[4];
	int result = dtcpIpStrmReceive(g_streamSession.stream, buffer, size, receivedType, timeout);

	*type = receivedType[0];
	return result;
}

static int streamRelease(SceSize size)
{
	return dtcpIpStrmRelease(g_streamSession.stream, size);
}

static int streamSend(void **buffer, SceSize *size, int timeout)
{
	return dtcpIpStrmSend(g_streamSession.stream, buffer, size, timeout);
}

static int streamSendMessage(int message)
{
	return dtcpIpStrmSendMessage(g_streamSession.stream, message);
}

static int streamAbort(void)
{
	return dtcpIpStrmAbort(g_streamSession.stream);
}

static int streamIsSessionOpen(void)
{
	return g_streamSession.stream != NULL;
}

static int streamIsStreamOpen(void)
{
	return dtcpIpStrmIsStreamOpen(g_streamSession.stream);
}

/* the clients' tables, in the library's order */
const struct SceDtcpIpStreamClient g_systemStreamClient = {
	systemConnect,
	systemSetUri,
	systemCloseStream,
	systemFlush,
	systemOpen,
	systemCloseSession,
	systemGetInfo,
	systemSetParameter,
	systemReceive,
	systemRelease,
	systemSend,
	systemSendMessage,
	systemAbort,
	systemIsStreamOpen,
	systemClose,
	systemIsSessionOpen,
	systemGetMoveHeader,
};

const struct SceDtcpIpStreamClient g_streamStreamClient = {
	streamConnect,
	streamSetUri,
	streamCloseSession,
	streamFlush,
	streamOpen,
	streamCloseStream,
	streamGetInfo,
	streamSetParameter,
	streamReceive,
	streamRelease,
	streamSend,
	streamSendMessage,
	streamAbort,
	streamIsStreamOpen,
	NULL,
	streamIsSessionOpen,
};
