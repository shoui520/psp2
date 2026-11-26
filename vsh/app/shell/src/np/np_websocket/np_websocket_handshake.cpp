/*
 * np_websocket_handshake.cpp: RFC 6455 HTTP/1.1 Upgrade request generation,
 * response reading, status parsing, and Sec-WebSocket-Accept SHA-1 verification.
 * Sony path: vsh/app/common/src/np/np_websocket/
 * 3.65-CEX: 0x813FC858..0x813FD413, 0x81402A12..0x81402D33
 */
#include "np_websocket_internal.h"
#include <libsha1.h>

static const char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

namespace sce {
namespace Np {
namespace WebSocket {

/* 0x813FC8EE: Format "GET <path> HTTP/1.1" request line */
static int FormatRequestLine(char *outBuf, SceSize bufSize, const char *method,
			     const char *path, const char *httpVer, SceSize *outRemaining)
{
	SceSize methodLen = sceClibStrnlen(method, static_cast<SceSize>(-1));
	SceSize pathLen = sceClibStrnlen(path, static_cast<SceSize>(-1));
	SceSize verLen = sceClibStrnlen(httpVer, static_cast<SceSize>(-1));

	if (methodLen + pathLen + verLen + 3U > bufSize)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	outBuf[0] = '\0';
	SceSize rem = bufSize - 1U;
	sceClibStrncat(outBuf, method, rem);
	rem -= methodLen;
	sceClibStrncat(outBuf, " ", rem--);
	sceClibStrncat(outBuf, path, rem);
	rem -= pathLen;
	sceClibStrncat(outBuf, " ", rem--);
	sceClibStrncat(outBuf, httpVer, rem);
	*outRemaining = rem - verLen;
	return 0;
}

/* 0x813FC9DC: Append "\r\nName: Value" header line */
static int AppendHeaderLine(char *outBuf, SceSize remaining, const char *name,
			    const char *value, SceSize *outRemaining)
{
	SceSize nameLen = sceClibStrnlen(name, static_cast<SceSize>(-1));
	SceSize valueLen = sceClibStrnlen(value, static_cast<SceSize>(-1));

	if (nameLen + valueLen + 4U > remaining)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	sceClibStrncat(outBuf, "\r\n", remaining);
	remaining -= 2U;
	sceClibStrncat(outBuf, name, remaining);
	remaining -= nameLen;
	sceClibStrncat(outBuf, ": ", remaining);
	remaining -= 2U;
	sceClibStrncat(outBuf, value, remaining);
	*outRemaining = remaining - valueLen;
	return 0;
}

/* 0x813FCD96: Append "\r\n\r\n" header terminator */
static int AppendHeaderTerminator(char *outBuf, SceSize remaining, SceSize *outRemaining)
{
	if (remaining < 4U)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	sceClibStrncat(outBuf, "\r\n\r\n", remaining);
	*outRemaining = remaining - 4U;
	return 0;
}

/* 0x813FC858 */
WsHandshake::WsHandshake()
	: m_hasResponse(0), m_responseHeaderLen(0), m_expectSubProtocol(0)
{
	sceClibMemset(m_clientKey, 0, sizeof(m_clientKey));
	sceClibMemset(m_responseBuf, 0, sizeof(m_responseBuf));
	sceClibMemset(m_subProtocol, 0, sizeof(m_subProtocol));
	GenerateKey();
}

/* 0x813FC892 */
WsHandshake::~WsHandshake()
{
}

void WsHandshake::ResetResponse(void)
{
	sceClibMemset(m_responseBuf, 0, sizeof(m_responseBuf));
	m_hasResponse = 0;
	m_responseHeaderLen = 0;
}

/* 0x813FC8CA / 0x813FD212: Generate 16 random bytes and Base64-encode as Sec-WebSocket-Key */
int WsHandshake::GenerateKey(void)
{
	SceUInt8 nonce[16];
	int ret = sceKernelGetRandomNumber(nonce, sizeof(nonce));
	if (ret < 0)
		return ret;

	sceClibMemset(m_clientKey, 0, sizeof(m_clientKey));
	return SceNpCommonPs4_EC456A83(nonce, m_clientKey, sizeof(nonce));
}

/* 0x813FCA8A: Build standard RFC 6455 opening handshake request headers */
int WsHandshake::BuildUpgradeRequest(const char *host, const char *path, const char *origin,
				     const char *subProtocol, char *outBuf, SceSize bufSize)
{
	if (host == NULL || outBuf == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	const char *requestPath = (path != NULL && path[0] != '\0') ? path : "/";
	SceSize remaining = 0;

	int ret = FormatRequestLine(outBuf, bufSize, "GET", requestPath, "HTTP/1.1", &remaining);
	if (ret < 0)
		return ret;

	ret = AppendHeaderLine(outBuf, remaining, "Host", host, &remaining);
	if (ret < 0)
		return ret;

	ret = AppendHeaderLine(outBuf, remaining, "Upgrade", "websocket", &remaining);
	if (ret < 0)
		return ret;

	ret = AppendHeaderLine(outBuf, remaining, "Connection", "Upgrade", &remaining);
	if (ret < 0)
		return ret;

	if (origin != NULL) {
		ret = AppendHeaderLine(outBuf, remaining, "Origin", origin, &remaining);
		if (ret < 0)
			return ret;
	}

	ret = AppendHeaderLine(outBuf, remaining, "Sec-WebSocket-Key", m_clientKey, &remaining);
	if (ret < 0)
		return ret;

	if (subProtocol != NULL) {
		if (sceClibStrnlen(subProtocol, static_cast<SceSize>(-1)) > NP_WS_PROTOCOL_MAX)
			return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
		sceClibStrncpy(m_subProtocol, subProtocol, sizeof(m_subProtocol));
		m_subProtocol[NP_WS_PROTOCOL_MAX] = '\0';
		m_expectSubProtocol = 1;
		ret = AppendHeaderLine(outBuf, remaining, "Sec-WebSocket-Protocol", subProtocol,
				       &remaining);
		if (ret < 0)
			return ret;
	} else {
		m_expectSubProtocol = 0;
	}

	ret = AppendHeaderLine(outBuf, remaining, "Sec-WebSocket-Version", "13", &remaining);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813FCCE4: Append caller-supplied header line */
int WsHandshake::AppendCustomHeader(const char *name, const char *value, char *outBuf,
				    SceSize bufSize)
{
	if (name == NULL || value == NULL || outBuf == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	SceSize currentLen = sceClibStrnlen(outBuf, bufSize);
	if (bufSize <= currentLen)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	SceSize remaining = bufSize - currentLen - 1U;
	int ret = AppendHeaderLine(outBuf, remaining, name, value, &remaining);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813FCDDA: Terminate handshake request with blank line */
int WsHandshake::TerminateRequest(char *outBuf, SceSize bufSize)
{
	if (outBuf == NULL)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	SceSize currentLen = sceClibStrnlen(outBuf, bufSize);
	if (bufSize <= currentLen)
		return SCE_NP_WS_ERROR_INVALID_ARGUMENT;

	SceSize remaining = bufSize - currentLen - 1U;
	int ret = AppendHeaderTerminator(outBuf, remaining, &remaining);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813FCFD2: Locate end of HTTP response headers (\n\r\n or \n\n) */
int WsHandshake::FindHeaderEndOffset(void) const
{
	for (SceSize pos = 0; pos < NP_WS_HTTP_BUF_SIZE - 2U; ++pos) {
		if (m_responseBuf[pos] == '\n' && m_responseBuf[pos + 1] == '\r' &&
		    m_responseBuf[pos + 2] == '\n') {
			return static_cast<int>(pos + 3U);
		}
	}
	for (SceSize pos = 0; pos < NP_WS_HTTP_BUF_SIZE - 1U; ++pos) {
		if (m_responseBuf[pos] == '\n' && m_responseBuf[pos + 1] == '\n')
			return static_cast<int>(pos + 2U);
	}
	return -1;
}

/* 0x81402A12: Read HTTP handshake response into m_responseBuf until header terminator */
int WsHandshake::ReceiveResponse(WsNet *net, SceSize *outTotalReceived, SceSize *outHeaderEnd)
{
	ResetResponse();

	char *cursor = m_responseBuf;
	SceSize spaceLeft = sizeof(m_responseBuf);
	if (spaceLeft == 0)
		return SCE_NP_WS_ERROR_HANDSHAKE;

	int headerEnd = -1;
	for (;;) {
		int recvBytes = net->Recv(cursor, spaceLeft);
		if (recvBytes < 0)
			return recvBytes;
		if (recvBytes == 0)
			return SCE_NP_WS_ERROR_HANDSHAKE;

		cursor += recvBytes;
		spaceLeft -= static_cast<SceSize>(recvBytes);
		headerEnd = FindHeaderEndOffset();
		if (headerEnd >= 0)
			break;
		if (spaceLeft == 0)
			return SCE_NP_WS_ERROR_HANDSHAKE;
	}

	*outTotalReceived = sizeof(m_responseBuf) - spaceLeft;
	*outHeaderEnd = static_cast<SceSize>(headerEnd);
	m_responseHeaderLen = static_cast<SceSize>(headerEnd);
	m_hasResponse = 1;
	return 0;
}

/* 0x813FCE62: Parse HTTP status code from the first response line */
int WsHandshake::ParseStatusCode(void) const
{
	if (m_hasResponse == 0)
		return SCE_NP_WS_ERROR_NO_RESPONSE;

	char statusLine[256];
	sceClibMemset(statusLine, 0, sizeof(statusLine));
	bool foundCrlf = false;

	for (SceSize idx = 0; idx < sizeof(statusLine) - 1U; ++idx) {
		statusLine[idx] = m_responseBuf[idx];
		if (idx > 1 && statusLine[idx - 1] == '\r' && statusLine[idx] == '\n') {
			statusLine[idx + 1] = '\0';
			foundCrlf = true;
			break;
		}
	}
	if (!foundCrlf)
		return SCE_NP_WS_ERROR_PROTOCOL;

	int majorVer = 0;
	int minorVer = 0;
	int statusCode = 0;
	const char *reasonPhrase = NULL;
	SceSize phraseLen = 0;

	SceSize lineLen = sceClibStrnlen(statusLine, sizeof(statusLine));
	int ret = sceHttpParseStatusLine(statusLine, lineLen, &majorVer, &minorVer, &statusCode,
					 &reasonPhrase, &phraseLen);
	return (ret >= 0) ? statusCode : ret;
}

/* 0x813FCF80: Look up a named HTTP response header */
int WsHandshake::FindResponseHeader(const char *headerName, const char **outValue,
				    SceSize *outValueLen) const
{
	if (m_hasResponse == 0)
		return SCE_NP_WS_ERROR_NO_RESPONSE;

	int ret = sceHttpParseResponseHeader(m_responseBuf, sizeof(m_responseBuf), headerName,
					     outValue, outValueLen);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813FD318: Verify Sec-WebSocket-Accept == Base64(SHA1(key + GUID)) */
int WsHandshake::VerifyAcceptKey(const char *acceptValue) const
{
	char combined[128];
	SceUChar8 sha1Digest[SCE_SHA1_DIGEST_SIZE];
	char expectedAccept[128];

	sceClibMemset(combined, 0, sizeof(combined));
	sceClibMemset(sha1Digest, 0, sizeof(sha1Digest));
	sceClibStrncpy(combined, m_clientKey, sizeof(combined) - 1U);

	SceSize keyLen = sceClibStrnlen(m_clientKey, sizeof(m_clientKey));
	SceSize remaining = sizeof(combined) - keyLen;
	sceClibStrncat(combined, kWebSocketGuid, remaining);

	SceSize combinedLen = sceClibStrnlen(combined, sizeof(combined));
	int ret = sceSha1Digest(combined, static_cast<SceUInt32>(combinedLen), sha1Digest);
	if (ret < 0)
		return ret;

	sceClibMemset(expectedAccept, 0, sizeof(expectedAccept));
	ret = SceNpCommonPs4_EC456A83(sha1Digest, expectedAccept, sizeof(sha1Digest));
	if (ret < 0)
		return ret;

	if (sceClibStrncmp(acceptValue, expectedAccept, sizeof(expectedAccept)) != 0)
		return SCE_NP_WS_ERROR_HANDSHAKE;
	return 0;
}

/* 0x813FD078: Validate 101 Switching Protocols + RFC 6455 upgrade headers */
int WsHandshake::VerifyResponse(void) const
{
	int statusCode = ParseStatusCode();
	if (statusCode < 0)
		return statusCode;
	if (statusCode != 101)
		return SCE_NP_WS_ERROR_HTTP_STATUS;

	const char *headerVal = NULL;
	SceSize headerLen = 0;

	int ret = FindResponseHeader("Upgrade", &headerVal, &headerLen);
	if (ret < 0)
		return ret;
	if (sceClibStrncasecmp("websocket", headerVal, headerLen) != 0)
		return SCE_NP_WS_ERROR_HANDSHAKE;

	ret = FindResponseHeader("Connection", &headerVal, &headerLen);
	if (ret < 0)
		return ret;
	if (sceClibStrncasecmp("Upgrade", headerVal, headerLen) != 0)
		return SCE_NP_WS_ERROR_HANDSHAKE;

	if (m_expectSubProtocol != 0) {
		ret = FindResponseHeader("Sec-WebSocket-Protocol", &headerVal, &headerLen);
		if (ret < 0)
			return ret;
		if (sceClibStrncasecmp(m_subProtocol, headerVal, headerLen) != 0)
			return SCE_NP_WS_ERROR_HANDSHAKE;
	}

	const char *acceptRaw = NULL;
	SceSize acceptLen = 0;
	ret = FindResponseHeader("Sec-WebSocket-Accept", &acceptRaw, &acceptLen);
	if (ret < 0)
		return ret;
	if (acceptLen >= NP_WS_KEY_BUF_SIZE)
		return SCE_NP_WS_ERROR_HANDSHAKE;

	char acceptStr[NP_WS_KEY_BUF_SIZE];
	sceClibMemset(acceptStr, 0, sizeof(acceptStr));
	sceClibStrncpy(acceptStr, acceptRaw, acceptLen);
	return VerifyAcceptKey(acceptStr);
}

} /* namespace WebSocket */
} /* namespace Np */
} /* namespace sce */
