/*
 * np_websocket_util_stream.cpp: RFC 6455 incoming frame stream buffer (defragmentation,
 * masking, control-frame extraction) and outgoing masked client frame builder.
 * Sony path: vsh/app/common/src/np/np_websocket/util/np_websocket_util_stream.cpp
 * 3.65-CEX: 0x813FD414..0x813FE2A7, 0x814026D8..0x814029B7
 */
#include "../np_websocket_internal.h"
#include <libmt19937.h>

#define NP_WS_STREAM_SOURCE_PATH \
	NP_WS_SOURCE_PATH("util\\np_websocket_util_stream.cpp")

#define NP_WS_STREAM_ASSERT(cond, func, line, expr) \
	do { \
		if (!(cond)) { \
			SceNpCommonPs4_F50BD96A(sceClibPrintf( \
				"Assertion in %s(%s:%d): %s(%s)\n", \
				func, NP_WS_STREAM_SOURCE_PATH, line, "ASSERT", expr)); \
		} \
	} while (0)

namespace sce {
namespace Np {
namespace WebSocket {

/* 0x813FD414 */
WsStream::WsStream(NpPushAllocator *allocator, SceSize bufferCapacity, WsNet *net)
	: m_net(net),
	  m_allocator(allocator),
	  m_capacity(bufferCapacity),
	  m_pBuffer(NULL),
	  m_writeIndex(0),
	  m_readIndex(0),
	  m_hasData(0)
{
}

/* 0x813FD470 */
WsStream::~WsStream()
{
	Finalize();
}

/* 0x813FD4AE */
int WsStream::Init(void)
{
	if (m_allocator == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}
	if (m_capacity == 0)
		m_capacity = NP_WS_DEFAULT_STREAM_BUF_SIZE;

	m_pBuffer = static_cast<SceUInt8 *>(SceNpCommonPs4_AA980113(m_allocator, m_capacity));
	if (m_pBuffer == NULL) {
		Finalize();
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;
	}

	m_writeIndex = 0;
	m_readIndex = 0;
	m_hasData = 0;
	return 0;
}

/* 0x813FD514 */
void WsStream::Finalize(void)
{
	if (m_allocator != NULL && m_pBuffer != NULL)
		SceNpCommonPs4_6B428829(m_allocator, m_pBuffer);
	m_pBuffer = NULL;
	m_allocator = NULL;
}

/* 0x813FD54E: Append newly received wire bytes to the stream buffer */
int WsStream::AppendData(const void *srcData, SceSize dataLen)
{
	if (dataLen == 0)
		return SCE_NP_WS_ERROR_INVALID_STATE;
	if (m_writeIndex + dataLen > m_capacity)
		return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

	sceClibMemcpy_safe(m_pBuffer + m_writeIndex, srcData, dataLen);
	m_writeIndex += dataLen;
	m_hasData = 1;
	return 0;
}

/* 0x813FD5C2: Compact consumed prefix bytes out of m_pBuffer */
SceSize WsStream::CompactBuffer(void)
{
	NP_WS_STREAM_ASSERT(m_readIndex <= m_writeIndex, "CompactBuffer", 223,
			    "m_readIndex <= m_writeIndex");

	SceSize remaining = m_writeIndex - m_readIndex;
	sceClibMemmove(m_pBuffer, m_pBuffer + m_readIndex, remaining);
	m_readIndex = 0;
	m_writeIndex = remaining;
	return remaining;
}

/* 0x813FD982: Excise an embedded control frame from m_pBuffer without disturbing fragments */
SceSize WsStream::removeFromBuffer(const WsParsedFrame &frame)
{
	NP_WS_STREAM_ASSERT(frame.start <= frame.end, "removeFromBuffer", 237,
			    "frame.start <= frame.end");
	NP_WS_STREAM_ASSERT(frame.end <= m_pBuffer + m_writeIndex, "removeFromBuffer", 238,
			    "frame.end <= m_pBuffer + m_writeIndex");

	SceSize startOff = static_cast<SceSize>(frame.start - m_pBuffer);
	SceSize endOff = static_cast<SceSize>(frame.end - m_pBuffer);
	SceSize frameLen = endOff - startOff;

	sceClibMemmove(frame.start, frame.end, m_writeIndex - endOff);
	if (m_readIndex >= startOff) {
		if (m_readIndex >= endOff)
			m_readIndex -= frameLen;
		else
			m_readIndex = startOff;
	}
	m_writeIndex -= frameLen;
	return m_writeIndex;
}

/* 0x813FD8EE: XOR payload with 4-byte RFC 6455 masking key */
void WsStream::ApplyMaskKey(SceUInt8 *dst, const SceUInt8 *src, SceSize len, SceUInt32 maskKey)
{
	SceUInt8 keyBytes[4];
	sceClibMemcpy_safe(keyBytes, &maskKey, sizeof(keyBytes));
	for (SceSize idx = 0; idx < len; ++idx)
		dst[idx] = src[idx] ^ keyBytes[idx & 3U];
}

bool WsStream::IsControlOpcode(int opcode)
{
	return (opcode & 0x8) != 0;
}

/* 0x813FDF02: Parse 7-bit, 16-bit, or 64-bit network-order payload length */
int WsStream::ParsePayloadLength(const SceUInt8 *buf, SceSize bufLen, SceSize offset,
				 SceUInt64 *outLength, int *outRawLenCode)
{
	if (offset + 2U > bufLen)
		return 1;

	int lenCode = buf[offset + 1] & 0x7F;
	if (outRawLenCode != NULL)
		*outRawLenCode = lenCode;

	if (lenCode < 126) {
		*outLength = static_cast<SceUInt64>(lenCode);
		return 0;
	}
	if (lenCode == 126) {
		if (offset + 4U > bufLen)
			return 1;
		SceUInt16 netLen = 0;
		sceClibMemcpy_safe(&netLen, buf + offset + 2, sizeof(netLen));
		*outLength = static_cast<SceUInt64>(sceNetNtohs(netLen));
		return 0;
	}
	if (offset + 10U > bufLen)
		return 1;
	SceUInt64 netLen64 = 0;
	sceClibMemcpy_safe(&netLen64, buf + offset + 2, sizeof(netLen64));
	*outLength = sceNetNtohll(netLen64);
	return 0;
}

/* 0x813FE196: Step offset across one frame header + payload */
int WsStream::AdvanceFrameOffset(SceSize currentOffset, SceSize *outNextOffset,
				 const SceUInt8 *buf, SceSize bufLen) const
{
	SceUInt64 payloadLen = 0;
	int lenCode = 0;
	int ret = ParsePayloadLength(buf, bufLen, currentOffset, &payloadLen, &lenCode);
	if (ret != 0)
		return ret;

	if (currentOffset + 2U > bufLen)
		return 1;
	bool masked = (buf[currentOffset + 1] & 0x80U) != 0;

	SceSize headerLen = 2U;
	if (lenCode == 126)
		headerLen = 4U;
	else if (lenCode == 127)
		headerLen = 10U;
	if (masked)
		headerLen += 4U;

	*outNextOffset = currentOffset + headerLen + static_cast<SceSize>(payloadLen);
	return 0;
}

/* 0x813FE114: Locate byte offset of the Nth frame inside buf */
int WsStream::FindFrameOffset(int frameIndex, const SceUInt8 *buf, SceSize bufLen,
			      SceSize *outOffset) const
{
	*outOffset = 0;
	for (int idx = 0; idx < frameIndex; ++idx) {
		int ret = AdvanceFrameOffset(*outOffset, outOffset, buf, bufLen);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/* 0x813FDAA8: Parse frame header and validate completeness at frameIndex */
int WsStream::ParseFrameAtIndex(int frameIndex, const SceUInt8 *buf, SceSize bufLen,
				WsParsedFrame *outFrame) const
{
	SceSize offset = 0;
	int ret = FindFrameOffset(frameIndex, buf, bufLen, &offset);
	if (ret != 0)
		return ret;

	if (offset + 2U > bufLen)
		return 1;

	int fin = (buf[offset] & 0x80U) != 0 ? 1 : 0;
	int opcode = buf[offset] & 0x0F;
	if (IsControlOpcode(opcode) && fin == 0)
		return SCE_NP_WS_ERROR_PROTOCOL;

	int masked = (buf[offset + 1] & 0x80U) != 0 ? 1 : 0;
	SceUInt64 payloadLen = 0;
	int lenCode = 0;
	ret = ParsePayloadLength(buf, bufLen, offset, &payloadLen, &lenCode);
	if (ret != 0)
		return ret;

	if (IsControlOpcode(opcode) && lenCode > static_cast<int>(NP_WS_CTRL_PAYLOAD_MAX))
		return SCE_NP_WS_ERROR_PROTOCOL;

	SceSize headerLen = 2U;
	if (lenCode == 126)
		headerLen = 4U;
	else if (lenCode == 127)
		headerLen = 10U;

	SceUInt32 maskKey = 0;
	if (masked != 0) {
		if (offset + headerLen + 4U > bufLen)
			return 1;
		sceClibMemcpy_safe(&maskKey, buf + offset + headerLen, sizeof(maskKey));
		headerLen += 4U;
	}

	if (static_cast<SceUInt64>(offset + headerLen) + payloadLen > bufLen)
		return 1;

	SceUInt8 *mutableBase = const_cast<SceUInt8 *>(buf);
	outFrame->opcode = opcode;
	outFrame->fin = fin;
	outFrame->masked = masked;
	outFrame->maskKey = maskKey;
	outFrame->payloadLen = static_cast<SceSize>(payloadLen);
	outFrame->start = mutableBase + offset;
	outFrame->payload = mutableBase + offset + headerLen;
	outFrame->end = mutableBase + offset + headerLen + static_cast<SceSize>(payloadLen);
	return 0;
}

/* 0x813FD644: Reassemble a complete data message or extract an interleaved control frame */
int WsStream::GetMessage(SceSize outCapacity, char *outBuf, int *outMsgType, SceSize *outMsgLen,
			 int *outIsControl, WsControlFrame *outCtrlFrame)
{
	NP_WS_STREAM_ASSERT(m_readIndex == 0, "GetMessage", 271, "m_readIndex == 0");

	*outMsgLen = 0;
	*outMsgType = 0;
	*outIsControl = 0;

	SceSize remainingCap = outCapacity;
	SceUInt8 *writeCursor = reinterpret_cast<SceUInt8 *>(outBuf);
	int frameIndex = 0;

	for (;;) {
		WsParsedFrame frame;
		int ret = ParseFrameAtIndex(frameIndex, m_pBuffer, m_writeIndex, &frame);
		if (ret < 0)
			return ret;
		if (ret == 1) {
			m_hasData = 0;
			return 1;
		}

		switch (frame.opcode) {
		case NP_WS_OPCODE_CONTINUATION:
		case NP_WS_OPCODE_TEXT:
		case NP_WS_OPCODE_BINARY:
			if (remainingCap < frame.payloadLen)
				return SCE_NP_WS_ERROR_OUT_OF_MEMORY;

			if (frame.masked != 0)
				ApplyMaskKey(writeCursor, frame.payload, frame.payloadLen,
					     frame.maskKey);
			else
				sceClibMemcpy_safe(writeCursor, frame.payload, frame.payloadLen);

			writeCursor += frame.payloadLen;
			remainingCap -= frame.payloadLen;

			if (frame.opcode == NP_WS_OPCODE_TEXT)
				*outMsgType = NP_WS_MSG_TYPE_TEXT;
			else if (frame.opcode == NP_WS_OPCODE_BINARY)
				*outMsgType = NP_WS_MSG_TYPE_BINARY;

			if (frame.fin == 0) {
				++frameIndex;
				continue;
			}

			*outMsgLen = outCapacity - remainingCap;
			if (*outMsgType == NP_WS_MSG_TYPE_TEXT && remainingCap != 0)
				outBuf[*outMsgLen] = '\0';

			m_readIndex = static_cast<SceSize>(frame.end - m_pBuffer);
			CompactBuffer();
			if (m_writeIndex == 0)
				m_hasData = 0;
			return 0;

		case NP_WS_OPCODE_CLOSE:
		case NP_WS_OPCODE_PING:
		case NP_WS_OPCODE_PONG:
			NP_WS_STREAM_ASSERT(
				NP_WS_CTRL_PAYLOAD_MAX >= frame.payloadLen, "GetMessage", 329,
				"sizeof(pCtrlFrame->body) >= frame.payloadLen");

			*outIsControl = 1;
			outCtrlFrame->opcode = frame.opcode;
			outCtrlFrame->payloadLen =
				(frame.payloadLen > NP_WS_CTRL_PAYLOAD_MAX)
					? NP_WS_CTRL_PAYLOAD_MAX
					: frame.payloadLen;
			if (outCtrlFrame->payloadLen > 0) {
				if (frame.masked != 0) {
					ApplyMaskKey(outCtrlFrame->body, frame.payload,
						     outCtrlFrame->payloadLen, frame.maskKey);
				} else {
					sceClibMemcpy_safe(outCtrlFrame->body, frame.payload,
							   outCtrlFrame->payloadLen);
				}
			}
			*outMsgLen = 0;
			removeFromBuffer(frame);
			return 0;

		default:
			return SCE_NP_WS_ERROR_PROTOCOL;
		}
	}
}

/* 0x8140274C: Encode a client-to-server masked RFC 6455 frame */
int WsFrameBuilder::BuildClientFrame(SceUInt8 *outBuf, SceSize outBufSize, int opcode,
				     const void *payload, SceSize payloadLen,
				     SceSize *outFrameLen)
{
	SceInt64 clockUsec = 0;
	SceNpCommonPs4_F56B0EAB(&clockUsec);
	SceUInt32 prngSeed = sceKernelGetProcessTimeLow() * static_cast<SceUInt32>(clockUsec);

	SceUInt8 *cursor = outBuf;
	SceSize remaining = outBufSize;

	*cursor++ = static_cast<SceUInt8>((opcode & 0x0F) | 0x80);
	--remaining;

	SceUInt8 lenCode = 0;
	if (payloadLen <= NP_WS_CTRL_PAYLOAD_MAX)
		lenCode = static_cast<SceUInt8>(payloadLen);
	else if (payloadLen <= 0xFFFFU)
		lenCode = 126;
	else
		lenCode = 127;

	*cursor++ = static_cast<SceUInt8>((lenCode & 0x7F) | 0x80);
	--remaining;

	if (lenCode == 126) {
		SceUInt16 netLen = sceNetHtons(static_cast<SceUInt16>(payloadLen));
		sceClibMemcpy_safe(cursor, &netLen, sizeof(netLen));
		cursor += sizeof(netLen);
		remaining -= sizeof(netLen);
	} else if (lenCode == 127) {
		SceUInt64 netLen64 = sceNetHtonll(static_cast<SceUInt64>(payloadLen));
		sceClibMemcpy_safe(cursor, &netLen64, sizeof(netLen64));
		cursor += sizeof(netLen64);
		remaining -= sizeof(netLen64);
	}

	SceMt19937Context mtCtx;
	int ret = sceMt19937Init(&mtCtx, prngSeed);
	if (ret < 0)
		return ret;

	SceUInt32 maskKey = sceNetHtonl(sceMt19937UInt(&mtCtx));
	sceClibMemcpy_safe(cursor, &maskKey, sizeof(maskKey));
	cursor += sizeof(maskKey);
	remaining -= sizeof(maskKey);

	if (payloadLen > 0) {
		WsStream::ApplyMaskKey(cursor, static_cast<const SceUInt8 *>(payload),
				       payloadLen, maskKey);
		remaining -= payloadLen;
	}

	*outFrameLen = outBufSize - remaining;
	return 0;
}

} /* namespace WebSocket */
} /* namespace Np */
} /* namespace sce */
