/*
 * SceDtcpIp: the sink's HTTP request. The middleware streams the file to the library
 * through SceHttp: the library creates a template and a connection for the uri, opens
 * a request with the seek range the caller set (DTCP-IP's Range.Dtcp.com for bytes,
 * TimeSeekRange.dlna.org for NPT), sends it and reads the body in chunks.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libhttp.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "dtcpip_internal.h"

/* the DTCP-IP request headers */
#define SCE_DTCP_IP_RANGE_HEADER		"Range.Dtcp.com"
#define SCE_DTCP_IP_TIME_SEEK_HEADER		"TimeSeekRange.dlna.org"

/* the request's states (the context's state word) */
#define SCE_DTCP_IP_HTTP_IDLE			0
#define SCE_DTCP_IP_HTTP_CREATED		1
#define SCE_DTCP_IP_HTTP_SENT			2
#define SCE_DTCP_IP_HTTP_NONE			3
#define SCE_DTCP_IP_HTTP_READING		4
#define SCE_DTCP_IP_HTTP_EOF			5
#define SCE_DTCP_IP_HTTP_ERROR			6

/* the sink's http request, its connection and its template (ours) */
struct SceDtcpIpHttp {
	SceUID template;	/* the request template, -1 while none */
	SceUID connection;	/* the connection, -1 while none */
	SceUID request;		/* the request, -1 while none; created when the stream opens */
	int statusCode;		/* the response's status code */
	int mode;		/* the stream's range kind: 0 none, 1 NPT, 2 bytes */
	int state;		/* SCE_DTCP_IP_HTTP_* */
	int eof;		/* the body was read to its end */
	int reserved0;
	SceUInt64 start;	/* the range's start: NPT milliseconds or bytes */
	SceUInt64 end;		/* the range's end: NPT milliseconds or bytes, 0 for open */
	char *url;		/* the url's copy the connection was made with */
	int aborted;		/* the request was aborted */
	int reserved1;
};

static struct SceDtcpIpHttp g_http;

/* reset the request's state, before a sink manager uses it */
int sinkResetHttp(void)
{
	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	g_http.template = -1;
	g_http.connection = -1;
	g_http.request = -1;
	g_http.state = SCE_DTCP_IP_HTTP_NONE;
	g_http.mode = 0;
	g_http.eof = 0;
	g_http.url = NULL;
	g_http.start = 0;
	g_http.end = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return 0;
}

/* delete the request's template */
int sinkCloseHttpTemplate(void)
{
	int ret = 0;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.template != -1) {
		ret = sceHttpDeleteTemplate(g_http.template);
		if (ret >= 0)
			g_http.template = -1;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* delete the request, its connection and the url's copy */
int sinkCloseHttpRequest(void)
{
	int ret = 0;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.request != -1) {
		if (g_http.state == SCE_DTCP_IP_HTTP_SENT || g_http.state == SCE_DTCP_IP_HTTP_READING) {
			result = sceHttpAbortRequest(g_http.request);
			if (result < 0)
				ret = result;
		}
		result = sceHttpDeleteRequest(g_http.request);
		if (result < 0)
			ret = result;
		else
			g_http.request = -1;
	}
	if (g_http.connection > 0) {
		result = sceHttpDeleteConnection(g_http.connection);
		if (result >= 0)
			g_http.connection = -1;
		else
			ret = result;
	}
	if (g_http.url != NULL) {
		dtcpIpFreeWith(dtcpIpHeapFree, g_http.url);
		g_http.url = NULL;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* delete the request, its connection and the template */
int sinkCloseHttpAll(void)
{
	int ret = 0;
	int result;
	int requestRet = 0;
	int templateRet;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.request != -1) {
		if (g_http.state == SCE_DTCP_IP_HTTP_SENT || g_http.state == SCE_DTCP_IP_HTTP_READING) {
			result = sceHttpAbortRequest(g_http.request);
			if (result < 0)
				requestRet = result;
		}
		result = sceHttpDeleteRequest(g_http.request);
		if (result < 0)
			requestRet = result;
		else
			g_http.request = -1;
	}
	if (g_http.connection > 0) {
		result = sceHttpDeleteConnection(g_http.connection);
		if (result >= 0)
			g_http.connection = -1;
		else
			requestRet = result;
	}
	if (g_http.url != NULL) {
		dtcpIpFreeWith(dtcpIpHeapFree, g_http.url);
		g_http.url = NULL;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	if (requestRet < 0)
		ret = requestRet;
	templateRet = 0;
	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.template != -1) {
		templateRet = sceHttpDeleteTemplate(g_http.template);
		if (templateRet >= 0)
			g_http.template = -1;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	if (templateRet < 0)
		return templateRet;
	return ret;
}

/* create the request's template with the parameter string as its user agent */
int sinkHttpCreateTemplate(const char *parameter)
{
	SceUID template;
	int ret;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	g_http.mode = 0;
	template = sceHttpCreateTemplate(parameter, 2, 0);
	ret = template;
	if (template >= 0) {
		g_http.template = template;
		ret = 0;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* connect the template to the uri, and remember the range kind the stream uses */
int sinkHttpConnect(const char *uri, int mode)
{
	SceSize length;
	char *copy;
	SceUID connection;
	int ret;

	length = sceClibStrnlen(uri, 0x3FFF) + 1;
	copy = dtcpIpAllocWith(dtcpIpHeapMalloc, length);
	if (copy == NULL)
		return SCE_DTCP_IP_ERROR_OUT_OF_MEMORY;
	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	g_http.url = copy;
	sceClibStrncpy(copy, uri, length);
	connection = sceHttpCreateConnectionWithURL(g_http.template, g_http.url, 1);
	ret = connection;
	if (connection < 0) {
		g_http.url = NULL;
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
		dtcpIpFreeWith(dtcpIpHeapFree, copy);
		return ret;
	}
	g_http.connection = connection;
	g_http.mode = mode;
	g_http.start = 0;
	g_http.end = 0;
	g_http.aborted = 0;
	g_http.eof = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return 0;
}

/*
 * read the next chunk of the stream's body: the response's status is remembered; the
 * first read starts the request (the middleware starts sending the file), and the read
 * reports the end of the body once.
 */
int sinkHttpReceive(void *buffer, unsigned int size, int *outSize)
{
	int ret = 0;
	int result;
	int state;
	SceUID request;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.statusCode > 199) {
		if (g_http.statusCode < 208)
			goto read;
		goto failed;
	}
	if (g_http.statusCode > 101) {
		if (g_http.statusCode <= 102)
			goto read;
		goto failed;
	}
	if (g_http.statusCode <= 99)
failed:
		ret = SCE_DTCP_IP_ERROR_HTTP_CONNECTION_CLOSED;
read:
	if (ret >= 0) {
		if (g_http.eof != 0) {
			*outSize = 0;
		} else {
			if (g_http.aborted == 0)
				g_http.state = SCE_DTCP_IP_HTTP_READING;
			state = g_http.state;
			request = g_http.request;
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
			if (state != SCE_DTCP_IP_HTTP_READING)
				return SCE_DTCP_IP_ERROR_CANCEL;
			ret = sceHttpReadData(request, buffer, size);
			dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
			if (g_http.aborted != 0) {
				ret = SCE_DTCP_IP_ERROR_CANCEL;
			} else if (ret <= 0) {
				if (ret != 0) {
					g_http.state = SCE_DTCP_IP_HTTP_ERROR;
				} else {
					*outSize = 0;
					g_http.eof = 1;
					g_http.state = SCE_DTCP_IP_HTTP_EOF;
				}
			} else {
				*outSize = ret;
				ret = 0;
			}
		}
	} else {
		g_http.state = SCE_DTCP_IP_HTTP_ERROR;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* the caller's byte range: it becomes the request's Range.Dtcp.com header */
int sinkSetByteRange(int startLow, int startHigh, int endLow, int endHigh)
{
	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	g_http.start = ((SceUInt64)(unsigned int)startHigh << 32) | (unsigned int)startLow;
	g_http.end = ((SceUInt64)(unsigned int)endHigh << 32) | (unsigned int)endLow;
	g_http.aborted = 0;
	g_http.eof = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return 0;
}

/* the caller's NPT range: it becomes the request's TimeSeekRange.dlna.org header */
int sinkSetTimeRange(int startLow, int startHigh, int endLow, int endHigh)
{
	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	g_http.start = ((SceUInt64)(unsigned int)startHigh << 32) | (unsigned int)startLow;
	g_http.end = ((SceUInt64)(unsigned int)endHigh << 32) | (unsigned int)endLow;
	g_http.aborted = 0;
	g_http.eof = 0;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return 0;
}

/* add a header to the request */
int sinkAddRequestHeader(const char *name, const char *value, unsigned int mode)
{
	int ret;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	ret = sceHttpAddRequestHeader(g_http.template, name, value, mode);
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* remove a header from the request */
int sinkRemoveRequestHeader(const char *name)
{
	int ret;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	ret = sceHttpRemoveRequestHeader(g_http.template, name);
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* the request's status code, while it is being read */
int sinkGetStreamInfo(int *outInfo)
{
	int ret = 0;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.state > 3) {
		if (g_http.state > 6)
			goto badState;
		*outInfo = g_http.statusCode;
	} else {
badState:
		ret = SCE_DTCP_IP_ERROR_SEQUENCE;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* delete the request, keeping the connection (the stream restarts on it) */
int sinkDeleteHttpRequest(void)
{
	int ret = 0;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.request != -1) {
		result = sceHttpDeleteRequest(g_http.request);
		if (result >= 0)
			g_http.request = -1;
		ret = result;
	}
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/* abort the request (the caller paused or stopped) */
int sinkAbortHttp(void)
{
	int ret = 0;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.request == -1)
		goto done;
	result = 0;
	if (g_http.state == SCE_DTCP_IP_HTTP_SENT || g_http.state == SCE_DTCP_IP_HTTP_READING)
		result = sceHttpAbortRequest(g_http.request);
	ret = result;
	if (result == 0)
done:
		g_http.aborted = 1;
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	return ret;
}

/*
 * open the stream's request: create it on the connection, add the range header the
 * caller set, send it and take the response's status code.
 */
int sinkOpenStreamReader(void)
{
	char value[64];
	char range[64];
	int statusCode[4];
	SceUID request;
	int ret;
	int length;
	int result;

	dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
	if (g_http.request != -1) {
		ret = SCE_DTCP_IP_ERROR_SEQUENCE;
		goto unlock;
	}
	g_http.statusCode = 0;
	request = sceHttpCreateRequestWithURL(g_http.connection, 0, g_http.url, 0);
	ret = request;
	if (request < 0)
		goto created;
	g_http.request = request;
	g_http.state = SCE_DTCP_IP_HTTP_CREATED;
	if (g_http.mode > 1) {
		if (g_http.mode <= 2) {
			if (g_http.end != 0)
				length = dtcpIpSnprintf(range, "bytes=%llu-%llu", g_http.start, g_http.end);
			else
				length = dtcpIpSnprintf(range, "bytes=%llu-", g_http.start);
			if (length < 0x40)
				result = sceHttpAddRequestHeader(request, SCE_DTCP_IP_RANGE_HEADER, range, 0);
			else
				result = SCE_DTCP_IP_ERROR_GENERIC;
			ret = result;
			goto created;
		}
	} else if (g_http.mode != 0) {
		SceUInt64 startSeconds = (g_http.start / 1000);
		int startMillis = (int)(g_http.start - startSeconds * 1000);

		if (g_http.end == 0) {
			length = dtcpIpSnprintf(value, "npt=%llu.%03d-", startSeconds, startMillis);
		} else {
			SceUInt64 endSeconds = (g_http.end / 1000);
			int endMillis = (int)(g_http.end - endSeconds * 1000);

			length = dtcpIpSnprintf(value, "npt=%llu.%03d-%llu.%03d", startSeconds,
						startMillis, endSeconds, endMillis);
		}
		if (length < 0x40)
			result = sceHttpAddRequestHeader(request, SCE_DTCP_IP_TIME_SEEK_HEADER, value, 0);
		else
			result = SCE_DTCP_IP_ERROR_GENERIC;
		ret = result;
		goto created;
	}
	ret = 0;
created:
	if (ret != 0)
		goto unlock;
	g_http.aborted = 0;
unlock:
	dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
	if (ret >= 0) {
		g_http.eof = 0;
		dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
		request = g_http.request;
		result = g_http.state;
		if (g_http.aborted == 0) {
			result = SCE_DTCP_IP_HTTP_SENT;
			g_http.state = SCE_DTCP_IP_HTTP_SENT;
		}
		dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
		if (result != SCE_DTCP_IP_HTTP_SENT) {
			ret = SCE_DTCP_IP_ERROR_CANCEL;
		} else {
			ret = sceHttpSendRequest(request, NULL, 0);
			dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
			if (g_http.aborted != 0) {
				ret = SCE_DTCP_IP_ERROR_CANCEL;
			} else {
				result = SCE_DTCP_IP_HTTP_NONE;
				if (ret < 0)
					result = SCE_DTCP_IP_HTTP_ERROR;
				g_http.state = result;
				if (ret >= 0) {
					ret = sceHttpGetStatusCode(g_http.request, statusCode);
					if (ret >= 0)
						g_http.statusCode = statusCode[0];
				}
			}
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
		}
		if (ret < 0) {
			dtcpIpLock(SCE_DTCP_IP_LOCK_HTTP);
			if (g_http.request != -1 && sceHttpDeleteRequest(g_http.request) >= 0)
				g_http.request = -1;
			dtcpIpUnlock(SCE_DTCP_IP_LOCK_HTTP);
		}
	}
	return ret;
}
