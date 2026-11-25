/*
 * SceDtcpIp: the sink manager. The sink device pulls a DTCP-IP stream: it opens the
 * middleware's session and stream objects, sets the stream's uri, asks the middleware
 * to open the stream, and reads the stream's chunks (each chunk carries the DTCP-IP
 * header the middleware already stripped). The sink's stream client methods are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

/* the sink's stream client: set when the sink manager is created */
const struct SceDtcpIpStreamClient *g_streamClient;

/* the HTTP side of the sink (dtcpip_http.c) */
int sinkResetHttp(void);
int sinkAbortHttp(void);
int sinkDeleteHttpRequest(void);
int sinkCloseHttpRequest(void);
int sinkCloseHttpTemplate(void);
int sinkCloseHttpAll(void);
int sinkHttpCreateTemplate(const char *parameter);
int sinkHttpConnect(const char *uri, int mode);

/* create the sink: open the middleware session, then the sink's stream client */
int sinkCreate(void)
{
	g_session.connection = NULL;
	g_session.status = 0;
	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	g_session.stream = 0;
	g_session.buffer = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	g_streamClient = &g_streamStreamClient;
	return sinkResetHttp();
}

/*
 * release the sink's stream: abort the http request, let the HTTP worker finish, delete
 * the request, and stop the middleware's stream.
 */
int sinkReleaseStream(void)
{
	int ret = 0;
	int waitResult = 0;
	int result;
	SceUInt timeout;

	if (g_session.stream != 0) {
		if (g_session.stream == 2) {
			ret = sinkAbortHttp();
			result = g_streamClient->flush();
			if (result != 0)
				ret = dtcpIpMapError(result);
		}
		if (g_session.stream == 2 || g_session.stream == 3) {
			timeout = 10000000;
			dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
			g_httpWorker.stopRequest = 0;
			if (g_httpWorker.running != 0)
				waitResult = dtcpIpWaitCond(SCE_DTCP_IP_COND_HTTP_WORKER, &timeout);
			result = dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
			if (waitResult < 0)
				ret = waitResult;
			result = sinkDeleteHttpRequest();
			if (result < 0)
				ret = result;
			result = g_streamClient->setParameter(0);
			if (result != 0)
				ret = dtcpIpMapError(result);
		}
		g_session.stream = 0;
		if (g_session.status != 0)
			ret = dtcpIpMapError(g_session.status);
	}
	return ret;
}

/* close the stream: release it, close the http request and the middleware's stream */
int sinkCloseStream(void)
{
	int ret;
	int result;

	ret = dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = sinkReleaseStream();
	result = dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	result = sinkCloseHttpRequest();
	if (result != 0)
		ret = result;
	result = g_streamClient->closeStream();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.status != 0)
		return dtcpIpMapError(g_session.status);
	return ret;
}

/* clear the uri: release the stream, close the session, clear the http request */
int sinkClearUri(void)
{
	int ret = 0;
	int result;

	if (g_streamClient->isSessionOpen() != 0) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
		ret = sinkReleaseStream();
		result = dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
		result = sinkCloseHttpRequest();
		if (result != 0)
			ret = result;
		result = g_streamClient->closeStream();
		if (result != 0)
			ret = dtcpIpMapError(result);
		if (g_session.status != 0)
			ret = dtcpIpMapError(g_session.status);
	}
	result = g_streamClient->flush();
	if (result != 0)
		ret = dtcpIpMapError(result);
	result = g_streamClient->closeSession();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.status != 0)
		return dtcpIpMapError(g_session.status);
	return ret;
}

/* close the sink manager: clear it, then close the middleware's session */
int sinkClose(void)
{
	int ret = 0;
	int result;
	int closeResult;
	int sessionResult;

	if (g_streamClient->isSessionOpen() != 0) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
		ret = sinkReleaseStream();
		result = dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
		result = sinkCloseHttpRequest();
		if (result != 0)
			ret = result;
		result = g_streamClient->closeStream();
		if (result != 0)
			ret = dtcpIpMapError(result);
		if (g_session.status != 0)
			ret = dtcpIpMapError(g_session.status);
	}
	result = g_streamClient->flush();
	if (result != 0)
		ret = dtcpIpMapError(result);
	result = g_streamClient->closeSession();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.status != 0)
		ret = dtcpIpMapError(g_session.status);

	sessionResult = 1;
	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	if (g_session.connection != NULL) {
		closeResult = dtcpIpIpcClose(g_session.connection);
		g_session.connection = NULL;
		sessionResult = closeResult;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	sessionResult = dtcpIpMapError(sessionResult);
	result = sinkCloseHttpTemplate();
	if (result < 0)
		sessionResult = result;
	if (g_session.status != 0)
		sessionResult = dtcpIpMapError(g_session.status);
	if (sessionResult != 0)
		return sessionResult;
	return ret;
}

/* tear the whole sink down (the STOP export's first half) */
int sinkTeardown(void)
{
	int ret = 0;
	int result;
	int closeResult;
	int templateResult;

	if (g_session.connection != NULL) {
		result = 0;
		if (g_streamClient->isSessionOpen() != 0) {
			dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
			result = sinkReleaseStream();
			closeResult = dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			closeResult = sinkCloseHttpRequest();
			if (closeResult != 0)
				result = closeResult;
			closeResult = g_streamClient->closeStream();
			if (closeResult != 0)
				result = dtcpIpMapError(closeResult);
			closeResult = g_streamClient->closeSession();
			if (closeResult != 0)
				result = dtcpIpMapError(closeResult);
			if (g_session.status != 0)
				result = dtcpIpMapError(g_session.status);
		}
		closeResult = g_streamClient->flush();
		if (closeResult != 0)
			result = dtcpIpMapError(closeResult);
		if (g_session.moveUri != NULL) {
			dtcpIpFreeWith(dtcpIpHeapFree, g_session.moveUri);
			g_session.moveUri = NULL;
		}
		g_session.movePort = 0;
		if (g_session.status != 0)
			result = dtcpIpMapError(g_session.status);
		templateResult = 1;
		dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
		if (g_session.connection != NULL) {
			closeResult = dtcpIpIpcClose(g_session.connection);
			g_session.connection = NULL;
			templateResult = closeResult;
		}
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
		templateResult = dtcpIpMapError(templateResult);
		closeResult = sinkCloseHttpTemplate();
		if (closeResult < 0)
			templateResult = closeResult;
		if (g_session.status != 0)
			templateResult = dtcpIpMapError(g_session.status);
		if (templateResult != 0)
			result = templateResult;
		ret = result;
	}
	result = sinkCloseHttpAll();
	if (result != 0)
		ret = result;
	if (g_session.status != 0)
		return dtcpIpMapError(g_session.status);
	return ret;
}

/*
 * set the sink's parameter string: connect the middleware session (it may take a
 * minute: the middleware activates itself first), then create the http template with
 * the parameter. isMove picks the move sink's session entry.
 */
int sinkSetParameterInternal(int isMove, const char *parameter)
{
	int config[4];
	int connection;
	int ret = 0;
	int tries;
	int status;

	if (g_session.closed != 0)
		return -2146404597;	/* 0x8010771B: the session was closed */

	config[2] = 0;
	config[0] = 1;
	config[1] = isMove != 0;
	config[3] = 0;

	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	if (g_session.cancelled != 0) {
		ret = 6;
	} else {
		status = dtcpIpIpcCreate(&connection, config, &g_dtcpIpIpcConfig, 140);
		if (status != 0) {
			ret = status;
		} else {
			tries = 0;
			status = 0;
			do {
				ret = dtcpIpIpcWaitReady(connection, &status);
				if (ret != 30)
					break;
				if (g_session.cancelled != 0) {
					ret = 6;
					break;
				}
				sceKernelDelayThread(100000);
				++tries;
			} while (tries < 600);
			if (ret != 0) {
				dtcpIpIpcDestroy(connection);
				dtcpIpIpcClose(connection);
			} else {
				g_session.connection = (void *)connection;
			}
		}
	}
	if (ret == 22) {
		if (g_session.cancelled != 0) {
			ret = 6;
		} else {
			status = dtcpIpIpcCreate(&connection, config, &g_dtcpIpActivateConfig, 140);
			if (status != 0) {
				ret = status;
			} else {
				tries = 0;
				status = 0;
				do {
					ret = dtcpIpIpcWaitReady(connection, &status);
					if (ret != 30)
						break;
					if (g_session.cancelled != 0) {
						ret = 6;
						break;
					}
					sceKernelDelayThread(100000);
					++tries;
				} while (tries < 600);
				if (ret != 0) {
					dtcpIpIpcDestroy(connection);
					dtcpIpIpcClose(connection);
				} else {
					g_session.connection = (void *)connection;
				}
			}
		}
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = dtcpIpMapError(ret);
	if (ret >= 0)
		ret = sinkHttpCreateTemplate(parameter);
	return ret;
}

/* set the sink's parameter string */
int sinkSetParameter(const char *parameter)
{
	return sinkSetParameterInternal(0, parameter);
}

/* set the sink's uri and port: the middleware session gets them */
int sinkSetUri(const char *uri, unsigned short port)
{
	int ret = 0;
	int result;

	result = g_streamClient->connect(g_session.connection);
	if (result != 0)
		return dtcpIpMapError(result);
	result = g_streamClient->setUri(uri, port);
	if (result != 0)
		return dtcpIpMapError(result);
	return ret;
}

/*
 * open the sink's stream: ask the middleware for the buffers (the caller's sizes are
 * in KiB), then open the http connection the stream is read from.
 */
int sinkOpenStream(const char *uri, int mode, int *sizes, int reserved, SceInt32 affinity)
{
	int receiveSize = 0x20000;
	int sendSize = 0x20000;
	int blockSize = 0x8000;
	int result;

	if (sizes != NULL) {
		int first = sizes[0];
		int second = sizes[1];
		int third = sizes[2];

		if (first != 0)
			receiveSize = first << 10;
		if (second != 0)
			sendSize = second << 10;
		if (third != 0)
			blockSize = third << 10;
	}
	result = g_streamClient->open(receiveSize, sendSize, blockSize, affinity);
	if (result != 0)
		return dtcpIpMapError(result);
	return sinkHttpConnect(uri, mode);
}

/* the sink's own stop has nothing to stop */
int sinkStopStream(void)
{
	return SCE_DTCP_IP_ERROR_SEQUENCE;
}
