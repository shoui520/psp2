/*
 * SceDtcpIp: the middleware session. The library talks to the middleware application
 * (NPXS10080) over the shared-memory IPC it owns: the connect opens the IPC, asks the
 * application to open its DTCP-IP session and closes the IPC again; the activation
 * check asks for the session's status and maps it onto the library's errors.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

/* the sink and the move sink classes, and the wrappers they share (ours) */
extern int sinkCreate(void);
extern int sinkTeardown(void);
extern int sinkSetParameter(const char *parameter);
extern int sinkClose(void);
extern int sinkSetUri(const char *uri, unsigned short port);
extern int sinkClearUri(void);
extern int sinkOpenStream(const char *uri, int mode, void *streamInfo, int reserved,
			  SceInt32 affinity);
extern int sinkStopStream(void);
extern int sinkCloseStream(void);

extern int moveSinkCreate(void);
extern int moveSinkTeardown(void);
extern int moveSinkSetParameter(const char *parameter);
extern int moveSinkClose(void);
extern int moveSinkSetUri(const char *uri, unsigned short port);
extern int moveSinkClearUri(void);
extern int moveSinkOpenStream(const char *uri, int mode, void *streamInfo, int reserved,
			      SceInt32 affinity);
extern int moveSinkStopStream(void);
extern int moveSinkCloseStream(void);

extern int sinkStartStream(void);
extern int sinkPauseStream(void);
extern int sinkHttpReceive(void *buffer, unsigned int size, int *outSize);
extern int sinkRead(void *buffer, SceSize size, int argument, int *outSize);
extern int sinkSeekByte(SceUInt64 start, SceUInt64 end);
extern int sinkSeekTime(SceUInt64 start, SceUInt64 end);
extern int sinkGetSessionBuffer(int *outBuffer);
extern int sinkAddRequestHeader(const char *name, const char *value, unsigned int mode);
extern int sinkRemoveRequestHeader(const char *name);
extern int sinkGetStreamInfo(int *outInfo);

/* the sink managers' function tables */
const struct SceDtcpIpSinkManager g_sinkManager = {
	sinkCreate,
	sinkTeardown,
	sinkSetParameter,
	sinkClose,
	sinkSetUri,
	sinkClearUri,
	sinkStartStream,
	sinkPauseStream,
	sinkOpenStream,
	sinkRead,
	sinkSeekByte,
	sinkSeekTime,
	sinkStopStream,
	sinkCloseStream,
	sinkAddRequestHeader,
	sinkRemoveRequestHeader,
	sinkGetStreamInfo,
	sinkGetSessionBuffer,
};

const struct SceDtcpIpSinkManager g_moveSinkManager = {
	moveSinkCreate,
	moveSinkTeardown,
	moveSinkSetParameter,
	moveSinkClose,
	moveSinkSetUri,
	moveSinkClearUri,
	sinkStartStream,
	sinkPauseStream,
	moveSinkOpenStream,
	sinkRead,
	sinkSeekByte,
	sinkSeekTime,
	moveSinkStopStream,
	moveSinkCloseStream,
	sinkAddRequestHeader,
	sinkRemoveRequestHeader,
	sinkGetStreamInfo,
	sinkGetSessionBuffer,
};

/* the worker picks the manager's class through these */
const struct SceDtcpIpSinkManager *dtcpIpGetSinkManager(void)
{
	return &g_sinkManager;
}

const struct SceDtcpIpSinkManager *dtcpIpGetMoveSinkManager(void)
{
	return &g_moveSinkManager;
}

/* the sink session, through the middleware's shared memory */
struct SceDtcpIpSession g_session;

/* the library state and the command the worker is running (the original keeps both in
 * cleared data words) */
struct SceDtcpIpContext g_dtcpIp;
union SceDtcpIpCommand g_command;
int g_dtcpIpIpcFlag;

/* the HTTP worker thread, and its status words */
struct SceDtcpIpHttpWorker g_httpWorker = {
	.thread = -1,
};

/*
/* the middleware's status, in the library's words (the MW's error.c table) */
int dtcpIpMapError(int status)
{
	int error;

	switch (status) {
	case 0:
		error = 0;
		break;
	case 6:
		error = SCE_DTCP_IP_ERROR_CANCEL;
		break;
	case 7:
		error = SCE_DTCP_IP_ERROR_DEVCERT_EXPIRED;
		break;
	case 8:
		error = SCE_DTCP_IP_ERROR_AUTH_INVALID_HOSTNAME;
		break;
	case 9:
		error = SCE_DTCP_IP_ERROR_AUTH_SERVER_NOT_FOUND;
		break;
	case 10:
		error = SCE_DTCP_IP_ERROR_AUTH_CANNOT_ACCESS;
		break;
	case 11:
	case 12: {
		int isWlan = 0;
		int isSecured = 0;

		error = SCE_DTCP_IP_ERROR_AUTH_FAILED;
		if (dtcpIpIsWlanConnection(&isWlan, &isSecured) == 0 && isWlan != 0 && isSecured == 0)
			error = SCE_DTCP_IP_ERROR_AUTH_FAILED_WLAN;
		break;
	}
	case 21:
		error = SCE_DTCP_IP_ERROR_STREAM_GENERIC;
		break;
	case 22:
		error = SCE_DTCP_IP_ERROR_KEY_NEED_ACTIVATE;
		break;
	case 23:
		error = SCE_DTCP_IP_ERROR_KEY_NEED_UPDATE;
		break;
	case 24:
		error = SCE_DTCP_IP_ERROR_KEY_NEED_CLOCK_SETUP;
		break;
	case 25:
	case 26:
	case 27:
	case 28:
		error = SCE_DTCP_IP_ERROR_KEY_FATAL;
		break;
	case 33:
		error = SCE_DTCP_IP_ERROR_MOVE_COMMITMENT_ABORTED;
		break;
	case 35:
		g_session.status = status;
		error = SCE_DTCP_IP_ERROR_IPC;
		break;
	case 36:
		g_session.status = status;
		error = SCE_DTCP_IP_ERROR_IPC_TIMEOUT;
		break;
#if FW_VERSION != 0x03630011
	case 37:
		error = SCE_DTCP_IP_ERROR_SESSION_CHECK;	/* -2146404583, 0x80107719 */
		break;
	case 38:
		error = SCE_DTCP_IP_ERROR_SESSION_CHECK_2;	/* -2146404584, 0x80107718 */
		break;
#endif
	default:
		error = SCE_DTCP_IP_ERROR_GENERIC;
		break;
	}
	return error;
}

/*

/*
 * the ACTIVATE command: ask the middleware's session for its status, and map it onto
 * the library's errors. The IPC connection the check opens is the context's, and it
 * is closed again before the status is mapped.
 */
int dtcpIpCheckActivation(struct SceDtcpIpMwConnection *connection)
{
	void *ipc = NULL;
	int subStatus = 0;
	int status;
	int error;

	int connectError = dtcpIpIpcConnect(&ipc);

	if (connectError != 0)
		return dtcpIpMapError(connectError);

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	connection->handle = ipc;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);

	status = 6;
	if (connection->closed == 0)
		status = dtcpIpIpcGetSessionStatus(ipc, &subStatus);

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	connection->handle = NULL;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	dtcpIpIpcDisconnect(ipc);

#if FW_VERSION == 0x03630011
	g_session.closed = 0;
	if (status == 0)
		return dtcpIpMapError(0);
	if (status == 1 && subStatus != 1)
		return subStatus;
	return dtcpIpMapError(status);
#else
	if (status == 0) {
		return dtcpIpMapError(0);
	} else {
		error = subStatus;
		if (status == 1 && subStatus != 1)
			return error;
		if (status != 37 || subStatus == 1) {
			error = dtcpIpMapError(status);
		} else {
			if (subStatus > -7) {
				if (subStatus <= -6)
					return SCE_DTCP_IP_ERROR_GENERIC;
				if (subStatus <= -5)
					return SCE_DTCP_IP_ERROR_CANCEL;
			} else if (subStatus > -8) {
				return SCE_DTCP_IP_ERROR_GENERIC;
			}
			error = SCE_DTCP_IP_ERROR_SESSION_CHECK;
		}
	}
	g_session.closed = 0;
	return error;
#endif
}

/*
 * the CONNECT command: open the middleware's DTCP-IP session (a one-shot IPC session:
 * the IPC opens, the application opens its session, and the IPC closes again).
 */
int dtcpIpOpenMwSession(void)
{
	int ret = 0;
	int ipcError;

	if (g_session.closed != 0)
		return SCE_DTCP_IP_ERROR_KEY_NEED_ACTIVATE;

	ipcError = dtcpIpIpcConnect(&g_session.connection);
	if (ipcError == 0) {
		ret = dtcpIpIpcOpenSession(g_session.connection);
		ipcError = dtcpIpIpcDisconnect(g_session.connection);
	}
	g_session.connection = NULL;
	if (ret == 0 && ipcError != 0)
		ret = ipcError;
	return dtcpIpMapError(ret);
}

/* the DISCONNECT command: ask the middleware to close its DTCP-IP session */
int dtcpIpCloseMwSession(struct SceDtcpIpMwConnection *connection)
{
	int ret = 0;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (connection->handle != NULL)
		ret = dtcpIpIpcCloseSession(connection->handle);
	connection->closed = 1;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return dtcpIpMapError(ret);
}

/* start the HTTP worker thread, at the API's priority */
int dtcpIpStartHttpWorker(SceInt32 priority, SceInt32 affinity)
{
	SceUID thread;
	int ret;

	g_httpWorker.status2 = 0;
	g_httpWorker.thread = -1;
	g_httpWorker.stopRequest = 0;
	g_httpWorker.running = 1;
	g_httpWorker.status = 0;

	thread = sceKernelCreateThread("SceDtcpIpHttpWorkerThread", dtcpIpHttpWorkerThread, priority,
				       0x1000, 0, affinity, NULL);
	ret = thread;
	if (thread >= 0) {
		g_httpWorker.thread = thread;
		ret = sceKernelStartThread(thread, 0, NULL);
		if (ret < 0) {
			sceKernelDeleteThread(g_httpWorker.thread);
			g_httpWorker.thread = -1;
		}
	}
	return ret;
}

/* stop the HTTP worker thread, and wait for it (10 seconds) */
int dtcpIpStopHttpWorker(void)
{
	int ret = 0;
	int result;
	SceInt32 stat = 0;
	SceUInt timeout = 10000000;

	if (g_httpWorker.thread != -1) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		g_httpWorker.stopRequest = 2;
		result = dtcpIpSignalCond(SCE_DTCP_IP_COND_HTTP_WORKER);
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		if (result < 0)
			ret = result;
		result = sceKernelWaitThreadEnd(g_httpWorker.thread, &stat, &timeout);
		if (result < 0)
			ret = result;
		result = sceKernelDeleteThread(g_httpWorker.thread);
		if (result >= 0)
			g_httpWorker.thread = -1;
		else
			ret = result;
	}
	return ret;
}

/*
 * the HTTP worker: take a chunk buffer from the middleware, fill it from the request,
 * report how much was read, and signal the end of the body.
 */
int dtcpIpHttpWorkerThread(SceSize args, void *argp)
{
	void *buffer;
	SceSize size;
	int outSize;
	int status = 0;
	int status2 = 0;
	int finished = 0;
	int result;
	int stopRequest;
	int (*sendMessage)(int);
	int abortResult;

	(void)args;
	(void)argp;
	for (;;) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		g_httpWorker.status = status;
		g_httpWorker.status2 = status2;
		stopRequest = g_httpWorker.stopRequest;
		if (g_httpWorker.stopRequest != 2) {
			if (status2 < 0 || finished != 0 || g_httpWorker.stopRequest == 0) {
				g_httpWorker.stopRequest = 0;
				do {
					g_httpWorker.running = 0;
					result = dtcpIpSignalCond(SCE_DTCP_IP_COND_HTTP_WORKER);
					if (result < 0)
						break;
					result = dtcpIpWaitCond(SCE_DTCP_IP_COND_HTTP_WORKER, NULL);
					if (result < 0)
						break;
					stopRequest = g_httpWorker.stopRequest;
				} while (g_httpWorker.stopRequest == 0);
				if (result < 0) {
					g_httpWorker.status2 = result;
					dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
					return g_httpWorker.status2;
				}
				status = g_httpWorker.status;
				status2 = g_httpWorker.status2;
			}
		}
		if (stopRequest == 2)
			break;
		g_httpWorker.running = 1;
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		result = g_streamClient->send(&buffer, &size, 0);
		if (result != 0) {
			finished = 0;
			if (result != 29)
				status2 = dtcpIpMapError(result);
		} else {
			result = sinkHttpReceive(buffer, size, &outSize);
			sendMessage = g_streamClient->sendMessage;
			if (result >= 0) {
				result = sendMessage(outSize);
				finished = 0;
				if (result != 0) {
					status2 = dtcpIpMapError(result);
				} else if (outSize == 0) {
					abortResult = g_streamClient->abort();
					if (abortResult != 0)
						status2 = dtcpIpMapError(abortResult);
					finished = 1;
				}
			} else {
				if (result != SCE_DTCP_IP_ERROR_CANCEL) {
					status2 = SCE_DTCP_IP_ERROR_HTTP_CONNECTION_CLOSED;
					status = result;
				}
				result = sendMessage(0);
				finished = 0;
				if (result != 0)
					status2 = dtcpIpMapError(result);
			}
		}
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
	return g_httpWorker.status2;
}

/* mark the session closed (the RESET export) */
void dtcpIpSessionClosed(void)
{
	g_session.closed = 1;
}

/* clear the sink session, before a sink manager is created or after it is closed */
void dtcpIpSessionClear(void)
{
	g_session.connection = NULL;
	g_session.stream = 0;
	g_session.buffer = 0;
	g_session.status = 0;
#if FW_VERSION != 0x03630011
	g_session.cancelled = 0;
#endif
}

/* the STOP export asks the worker to cancel what it is holding out on (the 3.630 build
 * keeps its closed word where the later builds keep this flag, so it has no cancel) */
void dtcpIpSessionCancelled(void)
{
#if FW_VERSION != 0x03630011
	g_session.cancelled = 1;
#endif
}

