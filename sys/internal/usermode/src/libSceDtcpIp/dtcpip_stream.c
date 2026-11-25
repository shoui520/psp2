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
int dtcpIpSysInit(struct SceDtcpIpSystemClient *client);
int dtcpIpSysSetUri(struct SceDtcpIpSystemClient *client, const char *uri, unsigned short port,
		    unsigned char *flag);
int dtcpIpSysCloseSession(struct SceDtcpIpSystemClient *client);
int dtcpIpSysFlushStart(struct SceDtcpIpSystemClient *client);
int dtcpIpSysFlushDone(struct SceDtcpIpSystemClient *client);
int dtcpIpSysOpenStream(void **outStream, void *connection, const int *config);
int dtcpIpSysCloseStream(void *stream);
int dtcpIpSysGetInfo(void *stream, void *info);
int dtcpIpSysSetParameter(void *stream, int parameter);
int dtcpIpSysReceive(void *stream, void **buffer, SceSize *size, void *type, int timeout);
int dtcpIpSysRelease(void *stream, SceSize size);
int dtcpIpSysSend(void *stream, int a1, int a2, int a3);
int dtcpIpSysSendMessage(void *stream, int message);
int dtcpIpSysAbort(void *stream);
int dtcpIpSysIsStreamOpen(void *stream);
int dtcpIpSysClose(void *stream);

int dtcpIpStrmInit(struct SceDtcpIpStreamSession *client);
int dtcpIpStrmSetUri(struct SceDtcpIpStreamSession *client, const char *uri, unsigned short port);
int dtcpIpStrmCloseSession(struct SceDtcpIpStreamSession *client);
int dtcpIpStrmFlushStart(struct SceDtcpIpStreamSession *client);
int dtcpIpStrmFlushDone(struct SceDtcpIpStreamSession *client);
int dtcpIpStrmOpenStream(void **outStream, void *connection, const int *config);
int dtcpIpStrmCloseStream(void *stream);
int dtcpIpStrmGetInfo(void *stream, void *info);
int dtcpIpStrmSetParameter(void *stream, int parameter);
int dtcpIpStrmReceive(void *stream, void **buffer, SceSize *size, void *type, int timeout);
int dtcpIpStrmRelease(void *stream, SceSize size);
int dtcpIpStrmSend(void *stream, int a1, int a2, int a3);
int dtcpIpStrmSendMessage(void *stream, int message);
int dtcpIpStrmAbort(void *stream);
int dtcpIpStrmIsStreamOpen(void *stream);
int dtcpIpStrmClose(void *stream);

/* the system client: connect, set the uri, open and close the session and stream */
static int systemConnect(void *connection)
{
	g_systemClient.connection = NULL;
	g_systemClient.stream = NULL;
	g_systemClient.state = 0;
	g_systemClient.flag = 0;
	return dtcpIpSysInit(&g_systemClient);
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
	result = dtcpIpSysSetUri(&g_systemClient, uri, port, &g_systemClient.flag);
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

static int systemCloseStream(void)
{
	if (g_systemClient.connection != NULL) {
		int result = dtcpIpSysCloseSession(&g_systemClient);

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
		ret = dtcpIpSysFlushStart(&g_systemClient);
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

static int systemCloseSession(void)
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

	sceClibMemset(streamInfo.info, 0, 16);
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
	return dtcpIpSysReceive(g_systemClient.stream, buffer, size, type, timeout);
}

static int systemRelease(SceSize size)
{
	return dtcpIpSysRelease(g_systemClient.stream, size);
}

static int systemSend(int a1, int a2, int a3)
{
	return dtcpIpSysSend(g_systemClient.stream, a1, a2, a3);
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

/* the stream client */
static int streamConnect(void *connection)
{
	g_streamSession.connection = NULL;
	g_streamSession.stream = NULL;
	g_streamSession.state = 0;
	return dtcpIpStrmInit(&g_streamSession);
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
	result = dtcpIpStrmSetUri(&g_streamSession, uri, port);
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
		int result = dtcpIpStrmCloseSession(&g_streamSession);

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
		ret = dtcpIpStrmFlushStart(&g_streamSession);
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

	sceClibMemset(streamInfo, 0, 16);
	streamInfo[13] = 0;
	return dtcpIpStrmGetInfo(g_streamSession.stream, streamInfo);
}

static int streamSetParameter(int parameter)
{
	return dtcpIpStrmSetParameter(g_streamSession.stream, parameter);
}

static int streamReceive(void **buffer, SceSize *size, unsigned char *type, int timeout)
{
	return dtcpIpStrmReceive(g_streamSession.stream, buffer, size, type, timeout);
}

static int streamRelease(SceSize size)
{
	return dtcpIpStrmRelease(g_streamSession.stream, size);
}

static int streamSend(int a1, int a2, int a3)
{
	return dtcpIpStrmSend(g_streamSession.stream, a1, a2, a3);
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

static int streamClose(void)
{
	return dtcpIpStrmClose(g_streamSession.stream);
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
	systemClose,
	systemIsStreamOpen,
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
	streamClose,
	NULL,
	streamIsSessionOpen,
};
