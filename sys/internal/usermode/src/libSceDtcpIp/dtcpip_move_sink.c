/*
 * SceDtcpIp: the move sink manager. The move sink pushes a downloaded file to the
 * middleware (the DTCP-IP move protocol): it keeps the uri it was given until the
 * manager closes, and the middleware's move commitment reports whether the peer
 * accepted the file. The move sink's methods are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

int sinkReleaseStream(void);
int sinkAbortHttp(void);
int sinkCloseHttpRequest(void);
int sinkCloseHttpTemplate(void);
int sinkCloseHttpAll(void);
int sinkHttpCreateTemplate(const char *parameter);
int sinkHttpConnect(const char *uri, int mode);

/* create the move sink: open the middleware session, then the system stream client */
int moveSinkCreate(void)
{
	g_session.connection = NULL;
	g_session.status = 0;
	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	g_session.stream = 0;
	g_session.buffer = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	g_streamClient = &g_systemStreamClient;
	return sinkResetHttp();
}

/* close the move sink's stream */
int moveSinkCloseStream(void)
{
	int ret;
	int result;

	result = dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = sinkReleaseStream();
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	result = sinkCloseHttpRequest();
	if (result != 0)
		ret = result;
	result = g_streamClient->closeStream();
	if (result != 0)
		ret = dtcpIpMapError(result);
	result = g_streamClient->closeSession();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.status != 0)
		return dtcpIpMapError(g_session.status);
	return ret;
}

/* clear the move sink's uri: release the stream, close the session, free the uri */
int moveSinkClearUri(void)
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
		result = g_streamClient->closeSession();
		if (result != 0)
			ret = dtcpIpMapError(result);
		if (g_session.status != 0)
			ret = dtcpIpMapError(g_session.status);
	}
	result = g_streamClient->flush();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.moveUri != NULL) {
		dtcpIpFreeWith(dtcpIpHeapFree, g_session.moveUri);
		g_session.moveUri = NULL;
	}
	g_session.movePort = 0;
	if (g_session.status != 0)
		return dtcpIpMapError(g_session.status);
	return ret;
}

/* close the move sink manager */
int moveSinkClose(void)
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
		result = g_streamClient->closeSession();
		if (result != 0)
			ret = dtcpIpMapError(result);
		if (g_session.status != 0)
			ret = dtcpIpMapError(g_session.status);
	}
	result = g_streamClient->flush();
	if (result != 0)
		ret = dtcpIpMapError(result);
	if (g_session.moveUri != NULL) {
		dtcpIpFreeWith(dtcpIpHeapFree, g_session.moveUri);
		g_session.moveUri = NULL;
	}
	g_session.movePort = 0;
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

/* tear the move sink down */
int moveSinkTeardown(void)
{
	int ret = 0;
	int result = 0;
	int closeResult;
	int templateResult;

	if (g_session.connection != NULL) {
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

/* set the move sink's parameter string */
int moveSinkSetParameter(const char *parameter)
{
	return sinkSetParameterInternal(1, parameter);
}

/* set the move sink's uri: the move sink keeps its own copy until it closes */
int moveSinkSetUri(const char *uri, unsigned short port)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(uri, 0x3FFF) + 1;
	copy = dtcpIpAllocWith(dtcpIpHeapMalloc, length);
	if (copy == NULL)
		return SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
	g_session.moveUri = copy;
	sceClibStrncpy(copy, uri, length);
	g_session.movePort = port;
	return 0;
}

/* open the move sink's stream: the move header tells the middleware the file's kind */
int moveSinkOpenStream(const char *uri, int mode, int *sizes, int reserved, SceInt32 affinity)
{
	int receiveSize = 0x20000;
	int sendSize = 0x20000;
	int blockSize = 0x8000;
	char header[4];
	int result;
	int first;
	int second;
	int third;

	result = g_streamClient->connect(g_session.connection);
	if (result != 0 || (result = g_streamClient->setUri(g_session.moveUri, g_session.movePort)) != 0) {
		return dtcpIpMapError(result);
	}
	dtcpIpSnprintf(header, "%02x", g_streamClient->getMoveHeader());
	result = sinkAddRequestHeader("BLKMove.dtcp.com", header, 0);
	if (result >= 0) {
		if (sizes != NULL) {
			first = sizes[0];
			second = sizes[1];
			third = sizes[2];
			if (first != 0)
				receiveSize = first << 10;
			if (second != 0)
				sendSize = second << 10;
			if (third != 0)
				blockSize = third << 10;
		}
		result = g_streamClient->open(receiveSize, sendSize, blockSize, affinity);
		if (result != 0)
			result = dtcpIpMapError(result);
		else
			result = sinkHttpConnect(uri, mode);
	}
	return result;
}

/* stop the move sink's stream: the middleware's move commitment says whether the peer
 * still needs the file */
int moveSinkStopStream(void)
{
	int result;

	result = g_streamClient->isStreamOpen();
	if (result != 11)
		return dtcpIpMapError(result);
	return SCE_DTCP_IP_ERROR_MOVE_COMMITMENT_NEED_RETRY;
}
