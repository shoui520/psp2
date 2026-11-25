#include "mlndl_internal.h"

static int32_t s_httpAbortRequested;
static int32_t s_httpAbortEnabled = 1;
static int32_t s_httpActiveRequestId = -1;
static const char s_emptyPostBody[] = "";

static uint32_t mlndlBoundedStrLen(const char *str, uint32_t limit)
{
	uint32_t len = 0;

	if (str == NULL)
		return limit;
	while (str[len] != '\0') {
		len++;
		if (len > limit)
			return limit;
	}
	return len;
}

int mlndlHttpCancel(void)
{
	mlndlHttpAbort();
	return MLNDL_OK;
}

int mlndlHttpOpen(const char *url, MlndlHttpContext **outContext)
{
	uint32_t scanLen = 0;
	uint32_t urlLen;
	char *urlCopy;
	int32_t tmplId = -1;
	int32_t connId;
	MlndlHttpContext *context;

	if (url == NULL || outContext == NULL)
		return MLNDL_RES_INVALID_PARAM;

	s_httpActiveRequestId = -1;
	while (url[scanLen] != '\0' && scanLen < 513)
		scanLen++;
	if (scanLen == 513)
		return MLNDL_RES_HTTP_ERROR;

	urlLen = mlndlBoundedStrLen(url, 512);
	urlCopy = (char *)mlndlAlloc(0, urlLen + 1);
	if (urlCopy == NULL)
		return MLNDL_RES_NO_MEMORY;

	mlndlMemCpy(urlCopy, url, urlLen);
	urlCopy[urlLen] = '\0';

	tmplId = sceHttpCreateTemplate("Mozilla/4.0 (PSP (PlayStation Portable); 2.00)", 2, 0);
	if (tmplId < 0)
		return tmplId;

	connId = sceHttpCreateConnectionWithURL(tmplId, url, 0);
	if (connId < 0) {
		sceHttpDeleteTemplate(tmplId);
		return connId;
	}

	context = (MlndlHttpContext *)mlndlAlloc(0, sizeof(MlndlHttpContext));
	if (context == NULL)
		return MLNDL_RES_NO_MEMORY;

	mlndlMemSet(context, 0, sizeof(MlndlHttpContext));
	context->templateId = tmplId;
	context->connectionId = connId;
	context->url = urlCopy;
	*outContext = context;
	return MLNDL_OK;
}

int mlndlHttpClose(MlndlHttpContext *context)
{
	if (context != NULL) {
		if (context->requestId >= 0)
			sceHttpDeleteRequest(context->requestId);
		if (context->connectionId >= 0)
			sceHttpDeleteConnection(context->connectionId);
		if (context->templateId >= 0)
			sceHttpDeleteTemplate(context->templateId);
		mlndlFree(0, context->url);
		mlndlFree(0, context);
		s_httpActiveRequestId = -1;
	}
	s_httpAbortEnabled = 1;
	s_httpAbortRequested = 0;
	return MLNDL_OK;
}

int mlndlHttpAddHeader(MlndlHttpContext *context, const char *name, const char *value)
{
	int err;

	if (context == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = sceHttpAddRequestHeader(context->connectionId, name, value, 0);
	return (err >= 0) ? MLNDL_OK : err;
}

int mlndlHttpRequest(MlndlHttpContext *context, int isGet, uint32_t reqLen,
	const void *reqBody, uint32_t *outLen, void **outBody)
{
	int method;
	int err;
	int statusCode;
	uint32_t capacity;
	uint32_t received;
	uint8_t *recvBuf = NULL;
	uint8_t *grownBuf;
	int readBytes;

	if (context == NULL || outBody == NULL || outLen == NULL)
		return MLNDL_RES_INVALID_PARAM;

	if (context->requestId >= 0) {
		sceHttpDeleteRequest(context->requestId);
		context->requestId = -1;
		s_httpActiveRequestId = -1;
	}

	if (isGet == 1) {
		reqLen = 0;
		reqBody = NULL;
		method = 0;
	} else {
		method = 1;
		if (reqLen == 0 && reqBody == NULL) {
			reqBody = s_emptyPostBody;
			reqLen = 1;
		}
	}

	if (s_httpAbortEnabled == 1 && s_httpAbortRequested == 1)
		return MLNDL_RES_ABORTED;

	err = sceHttpCreateRequestWithURL(context->connectionId, method, context->url,
		(uint64_t)reqLen);
	if (err < 0)
		return err;

	context->requestId = err;
	s_httpActiveRequestId = err;

	err = sceHttpSendRequest(context->requestId, reqBody, reqLen);
	if (err < 0)
		return err;

	err = sceHttpGetStatusCode(context->requestId, &statusCode);
	if (err < 0)
		return err;

	if (statusCode == 503 || statusCode == 500)
		return MLNDL_RES_SERVICE_UNAVAILABLE;
	if (statusCode != 200)
		return MLNDL_RES_HTTP_ERROR;

	if (s_httpAbortEnabled == 1 && s_httpAbortRequested == 1)
		return MLNDL_RES_ABORTED;

	capacity = 0x2000;
	recvBuf = (uint8_t *)mlndlAlloc(0, capacity);
	if (recvBuf == NULL)
		return MLNDL_RES_NO_MEMORY;

	received = 0;
	for (;;) {
		readBytes = sceHttpReadData(context->requestId, recvBuf + received,
			capacity - received);
		if (readBytes < 0) {
			mlndlFree(0, recvBuf);
			return readBytes;
		}
		if (readBytes == 0)
			break;
		received += (uint32_t)readBytes;
		if (received == capacity) {
			capacity *= 2;
			grownBuf = (uint8_t *)mlndlRealloc(0, recvBuf, capacity);
			if (grownBuf == NULL) {
				mlndlFree(0, recvBuf);
				return MLNDL_RES_NO_MEMORY;
			}
			recvBuf = grownBuf;
		}
	}

	if (received < capacity) {
		capacity = received;
		if (received != 0) {
			grownBuf = (uint8_t *)mlndlRealloc(0, recvBuf, received);
			if (grownBuf == NULL) {
				mlndlFree(0, recvBuf);
				return MLNDL_RES_NO_MEMORY;
			}
			recvBuf = grownBuf;
		}
	}

	*outLen = capacity;
	*outBody = recvBuf;
	return MLNDL_OK;
}

__attribute__((noinline)) int mlndlHttpGetResponseHeader(MlndlHttpContext *context, const char *name, char *outValue)
{
	char *rawHeader;
	uint32_t rawHeaderSize;
	const char *fieldValue = NULL;
	uint32_t fieldLen;
	int err;

	if (context == NULL || name == NULL || outValue == NULL)
		return MLNDL_RES_INVALID_PARAM;

	err = sceHttpGetAllResponseHeaders(context->requestId, &rawHeader, &rawHeaderSize);
	if (err < 0)
		return err;

	err = sceHttpParseResponseHeader(rawHeader, rawHeaderSize, name, &fieldValue, &fieldLen);
	if (err < 0)
		return err;

	if (fieldValue != NULL) {
		mlndlMemCpy(outValue, fieldValue, fieldLen);
		outValue[fieldLen] = '\0';
	}
	return MLNDL_OK;
}

__attribute__((noinline)) int mlndlHttpAbort(void)
{
	int err;

	s_httpAbortRequested = 1;
	if (s_httpAbortEnabled != 1 || s_httpActiveRequestId < 0)
		return MLNDL_OK;

	err = sceHttpAbortRequest(s_httpActiveRequestId);
	return (err == 0) ? MLNDL_OK : err;
}

void mlndlHttpFreeBody(void *body)
{
	mlndlFree(0, body);
}

int mlndlFindSubstring(const char *haystack, const char *needle, const char **outMatch)
{
	uint32_t haystackLen;
	uint32_t needleLen;
	int maxOffset;
	int offset;

	if (haystack == NULL || needle == NULL || outMatch == NULL)
		return MLNDL_RES_INVALID_PARAM;

	haystackLen = mlndlBoundedStrLen(haystack, 512);
	needleLen = mlndlBoundedStrLen(needle, 512);
	maxOffset = (int)(haystackLen - needleLen);
	if (maxOffset > 0) {
		for (offset = 0; offset < maxOffset; offset++) {
			if (mlndlStrNCmp(haystack + offset, needle, needleLen) == 0) {
				*outMatch = haystack + offset;
				return MLNDL_OK;
			}
		}
	}
	return MLNDL_OK;
}

int mlndlParseHeaderInt(const char *header, const char *key, int *outValue)
{
	const char *match = NULL;
	const char *delim = NULL;
	char digits[16];
	uint32_t keyLen;
	uint32_t digitLen;
	int value = 0;
	int pos;
	int scale;
	int remaining;
	int ch;

	mlndlMemSet(digits, 0, sizeof(digits));
	keyLen = mlndlBoundedStrLen(key, 512);
	mlndlFindSubstring(header, key, &match);
	if (match == NULL)
		return MLNDL_RES_INVALID_PARAM;

	mlndlFindSubstring(match + keyLen, ";", &delim);
	if (delim == NULL)
		delim = header + mlndlBoundedStrLen(header, 512);

	mlndlStrNCpy(digits, match + keyLen, (uint32_t)(delim - (match + keyLen)));
	digitLen = mlndlBoundedStrLen(digits, 512);
	if (digitLen != 0) {
		remaining = (int)digitLen - 1;
		for (pos = 0; pos < (int)digitLen; pos++) {
			ch = digits[pos];
			if (ch < '0' || ch > '9')
				return MLNDL_OK;
			scale = ch - '0';
			for (int pow = remaining; pow > 0; pow--)
				scale *= 10;
			value += scale;
			remaining--;
		}
		*outValue = value;
	}
	return MLNDL_OK;
}

int mlndlParseStatusHeader(MlndlHttpContext *context, const char *headerName,
	MlndlStatusInfo *statusInfo)
{
	char headerBuf[256];
	int reason = -1;
	int err;

	if (context == NULL || headerName == NULL || statusInfo == NULL)
		return MLNDL_RES_INVALID_PARAM;

	mlndlMemSet(headerBuf, 0, sizeof(headerBuf));
	statusInfo->maxConsole = -1;
	statusInfo->currentConsole = -1;
	statusInfo->maxTotalConsole = -1;
	statusInfo->currentTotalConsole = -1;
	statusInfo->maxUser = -1;
	statusInfo->currentUser = -1;

	err = mlndlHttpGetResponseHeader(context, headerName, headerBuf);
	if (err != MLNDL_OK)
		return err;

	mlndlParseHeaderInt(headerBuf, "max_console=", &statusInfo->maxConsole);
	mlndlParseHeaderInt(headerBuf, "current_console=", &statusInfo->currentConsole);
	mlndlParseHeaderInt(headerBuf, "max_total_console=", &statusInfo->maxTotalConsole);
	mlndlParseHeaderInt(headerBuf, "current_total_console=", &statusInfo->currentTotalConsole);
	mlndlParseHeaderInt(headerBuf, "max_user=", &statusInfo->maxUser);
	mlndlParseHeaderInt(headerBuf, "current_user=", &statusInfo->currentUser);

	if (mlndlBoundedStrLen(headerBuf, 512) < 2)
		return MLNDL_RES_STATUS_BAD_FORMAT;

	if (mlndlStrNCmp(headerBuf, "OK", 2) == 0)
		return MLNDL_OK;
	if (mlndlStrNCmp(headerBuf, "NG", 2) != 0)
		return MLNDL_RES_STATUS_BAD_FORMAT;

	mlndlParseHeaderInt(headerBuf, "reason=", &reason);
	switch (reason) {
	case 0:
		return 32;
	case 1:
		return 33;
	case 2:
		return 34;
	case 10:
		return 35;
	case 11:
		return 36;
	case 12:
		return 37;
	case 13:
		return 38;
	case 14:
		return 39;
	case 21:
		return 40;
	case 22:
		return 41;
	case 25:
		return 42;
	case 27:
		return 43;
	case 28:
		return 53;
	case 29:
		return 58;
	case 30:
		return 44;
	case 50:
		return 45;
	case 51:
		return 47;
	case 60:
		return 54;
	case 61:
		return 55;
	case 80:
		return 46;
	case 121:
		return 56;
	case 122:
		return 57;
	default:
		return MLNDL_RES_STATUS_UNKNOWN_REASON;
	}
}

int mlndlBuildInfoHeader(int actionKind, uint8_t *outBox)
{
	if (outBox == NULL)
		return MLNDL_RES_INVALID_PARAM;

	mlndlMemSet(outBox, 0, 16);
	outBox[0] = 0;
	outBox[1] = 0;
	outBox[2] = 0;
	outBox[3] = 16;
	outBox[4] = 'I';
	outBox[5] = 'N';
	outBox[6] = 'F';
	outBox[7] = 'O';
	outBox[8] = 0;
	outBox[9] = 1;

	switch (actionKind) {
	case 0:
		outBox[10] = 1;
		break;
	case 1:
		outBox[10] = 2;
		break;
	case 2:
		outBox[10] = 3;
		break;
	case 3:
	case 4:
		outBox[10] = 5;
		break;
	case 5:
		outBox[10] = 6;
		break;
	case 6:
		outBox[10] = 16;
		break;
	case 7:
		outBox[10] = 17;
		break;
	default:
		return MLNDL_RES_INVALID_PARAM;
	}
	return MLNDL_OK;
}
