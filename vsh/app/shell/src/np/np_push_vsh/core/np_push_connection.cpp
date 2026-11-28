/*
 * np_push_connection.cpp: WebSocket push connection wrapper and DataType string helper.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_connection.cpp
 * 3.65-CEX: 0x813B4C74..0x813B555B
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static const SceUShort16 kPushWebSocketPort = 443;
static const char kPushWebSocketPath[] = "/np/push";
static const SceSize kRespHeaderBufSize = 4096;

/* ============================================================================
 * DataType (0x813B54F8..0x813B555B)
 * ============================================================================ */

/* 0x813B54F8 */
DataType::DataType() : m_text(NULL) {}

DataType::DataType(const DataType &other) : m_text(NULL)
{
	if (other.m_text != NULL)
		Set(other.m_text);
}

DataType::DataType(const char *text) : m_text(NULL)
{
	if (text != NULL)
		Set(text);
}

DataType::~DataType()
{
	if (m_text != NULL) {
		SceNpCommonPs4_6B428829(GetAllocator(), m_text);
		m_text = NULL;
	}
}

DataType &DataType::operator=(const DataType &other)
{
	if (this != &other)
		Set(other.m_text);
	return *this;
}

/* 0x813B5518 */
int DataType::Set(const char *text)
{
	if (m_text != NULL) {
		SceNpCommonPs4_6B428829(GetAllocator(), m_text);
		m_text = NULL;
	}
	if (text == NULL)
		return 0;

	m_text = SceNpCommonPs4_1CAB4A2E(GetAllocator(), text);
	if (m_text == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x813B5544 */
bool DataType::Equals(const char *other) const
{
	if (m_text == NULL || other == NULL)
		return m_text == other;
	return sceClibStrncmp(m_text, other, 64) == 0;
}

/* ============================================================================
 * Connection (0x813B4C74..0x813B54F7)
 * ============================================================================ */

static bool ParseHeaderUInt32(const char *headers, SceSize headersLen, const char *headerName,
			      SceUInt32 *outValue)
{
	const char *valueStr = NULL;
	SceSize valueLen = 0;
	int ret = sceHttpParseResponseHeader(headers, headersLen, headerName, &valueStr, &valueLen);
	if (ret < 0 || valueStr == NULL || valueLen == 0)
		return false;

	char numBuf[32];
	SceSize copyLen = (valueLen < sizeof(numBuf) - 1) ? valueLen : (sizeof(numBuf) - 1);
	sceClibMemcpy_safe(numBuf, valueStr, copyLen);
	numBuf[copyLen] = '\0';

	SceUInt32 parsed = 0;
	if (SceNpCommonPs4_D5B30475(numBuf, &parsed, 10, NULL) < 0)
		return false;

	*outValue = parsed;
	return true;
}

/* 0x813B4C74 */
Connection::Connection()
	: m_wsHandle(NULL), m_recvBuffer(NULL), m_recvBufferSize(NP_PUSH_RECV_BUFFER_SIZE)
{
}

/* 0x813B4C8C */
Connection::~Connection()
{
	Disconnect();
}

/* 0x813B4D10: Connect */
int Connection::Connect(NpPushHandle *cancelHandle, const char *fqdn,
			const NpPushOnlineId *onlineId, bool isReconnect,
			ConnectionResponseInfo *outRespInfo)
{
	if (fqdn == NULL || onlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	if (outRespInfo != NULL)
		sceClibMemset(outRespInfo, 0, sizeof(*outRespInfo));

	ProtocolVersion protoVer;
	int ret = GetWebSocketProtocolVersion(&protoVer);
	if (ret < 0)
		return ret;

	NpPushUserToken userToken;
	SceNpCommonPs4_988DFD87(&userToken);
	NpPushOnlineId localOnlineId;
	SceNpCommonPs4_AEC0DAC5(&localOnlineId, onlineId);

	ret = SceNpCommonPs4_191AEA9E(cancelHandle, &localOnlineId, &userToken);
	SceNpCommonPs4_1543D7EE(&localOnlineId);
	if (ret < 0) {
		SceNpCommonPs4_78CC9FCD(&userToken);
		return ret;
	}

	char authHeader[320];
	sceClibMemset(authHeader, 0, sizeof(authHeader));
	ret = SceNpCommonPs4_D95658C6(&userToken, authHeader, sizeof(authHeader));
	SceNpCommonPs4_78CC9FCD(&userToken);
	if (ret < 0)
		return ret;

	NpPushTitleToken titleToken;
	SceNpCommonPs4_DC4787E9(&titleToken);
	bool hasTitleToken = false;
	if (protoVer.useTitleToken != 0) {
		ret = SceNpCommonPs4_9BC491E3(cancelHandle, onlineId->text, &titleToken);
		if (ret < 0) {
			SceNpCommonPs4_FD80FE40(&titleToken);
			return ret;
		}
		hasTitleToken = true;
	}

	NpPushHttpContext *httpClient = GetNpHttpClient();
	NpWebSocketOpenParam openParam;
	sceClibMemset(&openParam, 0, sizeof(openParam));
	openParam.allocator = *GetAllocator();
	openParam.libHttpCtxId = httpClient->libHttpCtxId;
	openParam.host = fqdn;
	openParam.port = kPushWebSocketPort;
	openParam.path = kPushWebSocketPath;
	openParam.useTls = 1;
	openParam.connectTimeout = 30U * 1000U * 1000U;
	openParam.recvTimeout = 0;

	void *wsHandle = NULL;
	ret = npWebSocketCreate(&openParam, &wsHandle);
	if (ret < 0) {
		SceNpCommonPs4_FD80FE40(&titleToken);
		return ret;
	}

	ret = npWebSocketAddRequestHeader(wsHandle, "Authorization", sizeof("Authorization") - 1,
					  authHeader, sceClibStrnlen(authHeader, sizeof(authHeader)));
	if (ret >= 0) {
		char verHeader[32];
		sceClibSnprintf(verHeader, sizeof(verHeader), "%u.%u",
				protoVer.majorVersion, protoVer.minorVersion);
		ret = npWebSocketAddRequestHeader(wsHandle, "X-NP-PROTOCOL-VERSION",
						  sizeof("X-NP-PROTOCOL-VERSION") - 1,
						  verHeader, sceClibStrnlen(verHeader, sizeof(verHeader)));
	}
	if (ret >= 0 && isReconnect) {
		ret = npWebSocketAddRequestHeader(wsHandle, "X-NP-RECONNECTION",
						  sizeof("X-NP-RECONNECTION") - 1,
						  "true", sizeof("true") - 1);
	}
	if (ret >= 0 && hasTitleToken) {
		const char *tokenStr = SceNpCommonPs4_80D16974(&titleToken);
		ret = npWebSocketAddRequestHeader(wsHandle, "X-NP-TITLE-TOKEN",
						  sizeof("X-NP-TITLE-TOKEN") - 1,
						  tokenStr, sceClibStrnlen(tokenStr, 1040));
	}
	SceNpCommonPs4_FD80FE40(&titleToken);

	char *respHeaders = NULL;
	if (ret >= 0) {
		respHeaders = static_cast<char *>(
			SceNpCommonPs4_AA980113(GetAllocator(), kRespHeaderBufSize));
		if (respHeaders == NULL)
			ret = SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	SceSize respHeaderSize = 0;
	if (ret >= 0) {
		sceClibMemset(respHeaders, 0, kRespHeaderBufSize);
		ret = npWebSocketConnect(wsHandle, respHeaders, kRespHeaderBufSize, &respHeaderSize);
	}

	if (ret >= 0 && outRespInfo != NULL && respHeaderSize > 0) {
		outRespInfo->hasActiveInterval = ParseHeaderUInt32(
			respHeaders, respHeaderSize, "X-NP-ACTIVE", &outRespInfo->activeIntervalSec);
		outRespInfo->hasSleepInterval = ParseHeaderUInt32(
			respHeaders, respHeaderSize, "X-NP-SLEEP", &outRespInfo->sleepIntervalSec);
		outRespInfo->hasRetryInterval = ParseHeaderUInt32(
			respHeaders, respHeaderSize, "X-NP-RETRY-INTERVAL",
			&outRespInfo->retryIntervalSec);
	}

	if (respHeaders != NULL)
		SceNpCommonPs4_6B428829(GetAllocator(), respHeaders);

	if (ret < 0) {
		npWebSocketDestroy(wsHandle);
		return ret;
	}

	m_recvBuffer = static_cast<char *>(
		SceNpCommonPs4_AA980113(GetAllocator(), m_recvBufferSize));
	if (m_recvBuffer == NULL) {
		npWebSocketDisconnect(wsHandle);
		npWebSocketDestroy(wsHandle);
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	m_wsHandle = wsHandle;
	return 0;
}

/* 0x813B51B0: Disconnect */
int Connection::Disconnect(void)
{
	if (m_wsHandle != NULL) {
		npWebSocketDisconnect(m_wsHandle);
		npWebSocketDestroy(m_wsHandle);
		m_wsHandle = NULL;
	}
	if (m_recvBuffer != NULL) {
		SceNpCommonPs4_6B428829(GetAllocator(), m_recvBuffer);
		m_recvBuffer = NULL;
	}
	return 0;
}

/* 0x813B5240: SendClose */
int Connection::SendClose(void)
{
	if (m_wsHandle == NULL)
		return SCE_NP_PUSH_ERROR_NOT_CONNECTED;
	return npWebSocketSendClose(m_wsHandle);
}

/* 0x813B5288: SendPacket */
int Connection::SendPacket(const Packet *packet)
{
	NP_PUSH_ASSERT(packet != NULL, "SendPacket", "core\\np_push_connection.cpp", 286,
		       "packet != NULL");

	if (m_wsHandle == NULL)
		return SCE_NP_PUSH_ERROR_NOT_CONNECTED;

	SceSize neededSize = 0;
	int ret = packet->BuildBufSize(&neededSize);
	if (ret < 0)
		return ret;

	char *sendBuf = static_cast<char *>(
		SceNpCommonPs4_AA980113(GetAllocator(), neededSize + 1));
	if (sendBuf == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	SceSize writtenSize = 0;
	ret = packet->Build(sendBuf, neededSize + 1, &writtenSize);
	if (ret >= 0) {
		sendBuf[writtenSize] = '\0';
		ret = npWebSocketSendText(m_wsHandle, sendBuf, writtenSize);
	}

	SceNpCommonPs4_6B428829(GetAllocator(), sendBuf);
	return ret;
}

/* 0x813B53B8: ReceivePacket */
int Connection::ReceivePacket(RefPtr<Packet> *outPacket)
{
	if (m_wsHandle == NULL || m_recvBuffer == NULL)
		return SCE_NP_PUSH_ERROR_NOT_CONNECTED;
	if (outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	int opcode = 0;
	SceSize receivedSize = 0;
	int ret = npWebSocketReceive(m_wsHandle, &opcode, m_recvBuffer,
				     m_recvBufferSize - 1, &receivedSize);
	if (ret < 0)
		return ret;

	m_recvBuffer[receivedSize] = '\0';
	return Packet::Parse(m_recvBuffer, receivedSize, outPacket);
}

/* 0x813B4CF0 */
int Connection::ParseOnlineIdHeader(const char *headerName, NpPushOnlineIdData *outList,
				    int maxCount, int *outCount)
{
	sceClibMemset(outList, 0, sizeof(NpPushOnlineIdData) * static_cast<SceSize>(maxCount));
	const char *valueStr = NULL;
	SceSize valueLen = 0;
	int ret = npWebSocketGetResponseHeader(m_wsHandle, headerName, &valueStr, &valueLen);
	if (ret < 0) {
		*outCount = 0;
		return 0;
	}

	const char *cursor = valueStr;
	const char *end = valueStr + valueLen;
	int parsedCount = 0;
	while (cursor < end) {
		while (cursor < end && *cursor == ' ')
			++cursor;
		const char *tokenStart = cursor;
		while (cursor < end && *cursor != ',')
			++cursor;
		SceSize tokenLen = static_cast<SceSize>(cursor - tokenStart);
		if (tokenLen > 0 && parsedCount < maxCount && tokenLen <= 16) {
			sceClibStrncpy(outList[parsedCount].text, tokenStart, tokenLen);
			++parsedCount;
		}
		if (cursor < end && *cursor == ',')
			++cursor;
	}

	*outCount = parsedCount;
	return 0;
}

/* 0x813B4DB0 */
int Connection::ParseRetryIntervalHeader(SceUInt32 *outIntervalSec)
{
	const char *valueStr = NULL;
	SceSize valueLen = 0;
	int ret = npWebSocketGetResponseHeader(m_wsHandle, "X-NP-RETRY-INTERVAL",
					       &valueStr, &valueLen);
	if (ret == SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND) {
		*outIntervalSec = NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC;
		return 0;
	}
	if (ret < 0)
		return ret;

	SceInt32 parsed = NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC;
	if (SceNpCommonPs4_3AE5ABF3(valueStr, &parsed, 10, NULL) < 0 || parsed < 0)
		*outIntervalSec = NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC;
	else
		*outIntervalSec = static_cast<SceUInt32>(parsed);
	return 0;
}

/* 0x813B51C6 */
int Connection::DisconnectWithCancelScope(NpPushHandle *cancelHandle)
{
	NpPushScope cancelScope;
	SceNpCommonPs4_5BBB1ABA(&cancelScope);
	int ret = SceNpCommonPs4_DDFD56E9(&cancelScope, cancelHandle, 0, 0, 0);
	if (ret >= 0) {
		int closeRet = npWebSocketSendClose(m_wsHandle);
		ret = SceNpCommonPs4_3D0FA7C9(&cancelScope, closeRet, 0, 0, 0);
	}
	SceNpCommonPs4_101259AD(&cancelScope);
	return (ret >= 0) ? 0 : ret;
}

/* ============================================================================
 * RecvPacketQueue (0x813B555C..0x813B56E0) & ReceiveThread (0x813B56FC..0x813B57A9)
 * ============================================================================ */

/* 0x813B555C */
RecvPacketQueue::RecvPacketQueue(NpPushMutex *mutex)
	: m_mutex(mutex), m_list(GetAllocator())
{
}

/* 0x813B5580 / 0x813B55E4 */
RecvPacketQueue::~RecvPacketQueue()
{
	m_list.Clear();
}

/* 0x813B55F4 */
int RecvPacketQueue::Push(const RefPtr<Packet> &packet)
{
	ScopedLock lock(m_mutex);
	if (!m_list.PushBack(packet))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x813B564C */
void RecvPacketQueue::Pop(RefPtr<Packet> *outPacket)
{
	ScopedLock lock(m_mutex);
	if (m_list.Empty()) {
		outPacket->Reset();
		return;
	}
	*outPacket = m_list.Begin()->value;
	m_list.Erase(m_list.Begin());
}

/* 0x813B56E0 */
void RecvPacketQueue::Clear(void)
{
	ScopedLock lock(m_mutex);
	m_list.Clear();
}

/* 0x813B56FC */
ReceiveThread::ReceiveThread(Session *owner)
	: m_owner(owner)
{
	SceNpCommonPs4_04CBEB81(AsThreadBase());
}

/* 0x813B5718 / 0x813B5734 */
ReceiveThread::~ReceiveThread()
{
	SceNpCommonPs4_502309F4(AsThreadBase());
}

int ReceiveThread::Run(void)
{
	if (m_owner != NULL)
		m_owner->ReceiveMain();
	return 0;
}

/* 0x813B5754 */
int ReceiveThread::Start(void)
{
	NpPushThreadParam threadParam;
	threadParam.name = "SceNpPushReceive";
	threadParam.stackSize = 0x2000;
	threadParam.priority = 0x10000100;
	threadParam.cpuAffinity = 0;
	return SceNpCommonPs4_C8F1844F(AsThreadBase(), &threadParam);
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
