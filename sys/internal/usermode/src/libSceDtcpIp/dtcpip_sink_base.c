/*
 * SceDtcpIp: the sink stream, shared by the sink and the move sink. The middleware
 * hands the stream over in chunks: each chunk starts with a DTCP-IP type byte (the
 * middleware already verified and decrypted it), and the reader copies the chunk out
 * to the caller's buffer as far as it goes.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

int sinkReleaseStream(void);
int sinkDeleteHttpRequest(void);
int sinkOpenStreamReader(void);

/* the http layer's range setters (dtcpip_http.c) */
int sinkSetByteRange(int startLow, int startHigh, int endLow, int endHigh);
int sinkSetTimeRange(int startLow, int startHigh, int endLow, int endHigh);

/* the caller starts playing: an idle stream has no pending seek */
int sinkStartStream(void)
{
	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	if (g_session.stream == 0) {
		g_session.stream = 1;
		g_session.buffer = 0;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	return 0;
}

/* the caller pauses: the middleware's stream is released (it replays from the range) */
int sinkPauseStream(void)
{
	int ret;

	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = sinkReleaseStream();
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	return ret;
}

/* seek by byte: the request's range, then the stream restarts at it */
int sinkSeekByte(SceUInt64 start, SceUInt64 end)
{
	int ret;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = sinkReleaseStream();
	if (ret >= 0) {
		ret = sinkSetByteRange((int)start, (int)(start >> 32), (int)end, (int)(end >> 32));
		if (ret >= 0) {
			ret = 0;
			if (g_session.stream == 0) {
				g_session.buffer = 0;
				g_session.stream = 1;
			}
		}
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	return ret;
}

/* seek by time: the request's NPT range, then the stream restarts at it */
int sinkSeekTime(SceUInt64 start, SceUInt64 end)
{
	int ret;

	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	ret = sinkReleaseStream();
	if (ret >= 0) {
		ret = sinkSetTimeRange((int)start, (int)(start >> 32), (int)end, (int)(end >> 32));
		if (ret >= 0) {
			ret = 0;
			if (g_session.stream == 0) {
				g_session.buffer = 0;
				g_session.stream = 1;
			}
		}
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	return ret;
}

/*
 * read the stream: pull chunk headers and chunk data from the middleware until the
 * caller's buffer is full, the stream ends, or the middleware reports an error. The
 * last argument tells the caller which kind of chunk ended the read (0 data, 1 stream
 * end, 2 move end, 3 ...).
 */
int sinkRead(void *buffer, SceSize size, int *outSize, int *outStatus)
{
	SceSize total = 0;
	SceSize chunkSize;
	unsigned char type[4];
	void *chunk;
	void *cursor = buffer;
	SceSize copySize;
	int ret = 0;
	int result;
	int receiveResult;
	int workerStatus;
	int workerStatus2;
	int typeStatus;

	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	*outSize = 0;

	if (g_session.stream == 3) {
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
		return 0;
	}

	chunk = NULL;
	if (g_session.stream == 1) {
		ret = sinkOpenStreamReader();
		if (ret < 0) {
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			return ret;
		}
		receiveResult = g_streamClient->getInfo();
		if (receiveResult != 0) {
			ret = dtcpIpMapError(receiveResult);
			sinkDeleteHttpRequest();
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			return ret;
		}
		chunk = NULL;
		dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		g_httpWorker.status = 0;
		g_httpWorker.stopRequest = 1;
		g_httpWorker.status2 = 0;
		result = dtcpIpSignalCond(SCE_DTCP_IP_COND_HTTP_WORKER);
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		ret = result;
		if (result < 0) {
			g_streamClient->setParameter(0);
			sinkDeleteHttpRequest();
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			return ret;
		}
		g_session.stream = 2;
	}

	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	chunkSize = 0;
	if (size == 0)
		goto done;

	while (1) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		workerStatus = g_httpWorker.status;
		workerStatus2 = g_httpWorker.status2;
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP_WORKER);
		if (workerStatus < 0)
			break;

		chunk = NULL;
		chunkSize = 0;
		receiveResult = g_streamClient->receive(&chunk, &chunkSize, type, 10);
		if (receiveResult == 29) {
			chunkSize = 0;
		} else if (receiveResult == 20) {
			dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
			if (g_session.stream != 0)
				g_session.stream = 3;
			else
				ret = SCE_DTCP_IP_ERROR_CANCEL;
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
			goto done;
		} else if (receiveResult != 0) {
			ret = dtcpIpMapError(receiveResult);
			goto done;
		}

		typeStatus = 0;
		switch (type[0]) {
		case 0:
		case 2:
			*outStatus = 0;
			break;
		case 1:
		case 3:
		case 5:
		case 7:
		case 9:
		case 0xB:
			typeStatus = SCE_DTCP_IP_ERROR_GENERIC;
			break;
		case 4:
		case 6:
			*outStatus = 1;
			break;
		case 8:
		case 0xA:
			*outStatus = 2;
			break;
		case 0xC:
			*outStatus = 3;
			break;
		default:
			if ((unsigned char)type[0] == 255)
				*outStatus = 0;
			else
				typeStatus = SCE_DTCP_IP_ERROR_GENERIC;
			break;
		}
		ret = typeStatus;
		copySize = size - total;
		if (chunkSize <= copySize)
			copySize = chunkSize;
		sceClibMemcpy_safe(cursor, chunk, copySize);
		total += copySize;
		cursor = (char *)cursor + copySize;
		receiveResult = g_streamClient->release(copySize);
		if (receiveResult != 0) {
			ret = dtcpIpMapError(receiveResult);
			goto done;
		}
		if (total >= size)
			goto done;
	}

	if (workerStatus == SCE_DTCP_IP_ERROR_HTTP_CONNECTION_CLOSED) {
		dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
		g_session.buffer = workerStatus2;
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	}
	return workerStatus;

done:
	*outSize = total;
	return ret;
}

/* the session buffer the middleware shares with the library */
int sinkGetSessionBuffer(int *outBuffer)
{
	dtcpIpLock(SCE_DTCP_IP_LOCK_MANAGER);
	*outBuffer = g_session.buffer;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_MANAGER);
	return 0;
}
