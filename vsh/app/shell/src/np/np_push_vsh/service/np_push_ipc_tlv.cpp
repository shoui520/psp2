/*
 * np_push_ipc_tlv.cpp: Server-side TLV primitives (header read/write, TlvString,
 * and TlvJsonValue) for SceNpPush IPC notification buffers.
 * Sony path: vsh/app/common/src/np/np_push/ipc/np_push_ipc_tlv.cpp
 * 3.65-CEX: 0x81403BA4..0x81404081
 */
#include "../np_push_internal.h"
#include <net/inet.h>

#define NP_PUSH_IPC_TLV_HEADER_SIZE	4U

#define NP_PUSH_IPC_TLV_SOURCE_PATH \
	"W:\\Build\\" NP_PUSH_BUILD_ID "\\vsh\\app\\common\\src\\np\\np_push\\ipc\\np_push_ipc_tlv.cpp"

namespace sce {
namespace Np {
namespace Push {

/* 0x81403BA4: Write network-order 16-bit TLV type + length header */
int WriteTlvHeader(SceUInt16 tlvType, SceUInt16 payloadLength, char *outBuffer,
		   SceSize bufferSize, SceSize *outWritten)
{
	if (outBuffer != NULL && bufferSize < NP_PUSH_IPC_TLV_HEADER_SIZE)
		return SCE_NP_PUSH_ERROR_TLV_BUFFER_TOO_SMALL;

	SceUInt16 netType = sceNetHtons(tlvType);
	if (outBuffer != NULL)
		sceClibMemcpy_safe(outBuffer, &netType, sizeof(netType));

	SceUInt16 netLen = sceNetHtons(payloadLength);
	if (outBuffer != NULL)
		sceClibMemcpy_safe(outBuffer + sizeof(netType), &netLen, sizeof(netLen));

	if (outWritten != NULL)
		*outWritten = NP_PUSH_IPC_TLV_HEADER_SIZE;
	return 0;
}

/* 0x81403C1E: Read and validate network-order 16-bit TLV header */
int ReadTlvHeader(SceUInt16 expectedType, SceUInt16 *outLength, const char *srcBuffer,
		  SceSize bufferSize, SceSize *outConsumed)
{
	if (bufferSize < NP_PUSH_IPC_TLV_HEADER_SIZE)
		return SCE_NP_PUSH_ERROR_TLV_PARSE;

	SceUInt16 netType = 0;
	SceUInt16 netLen = 0;
	sceClibMemcpy_safe(&netType, srcBuffer, sizeof(netType));
	SceUInt16 actualType = sceNetNtohs(netType);
	sceClibMemcpy_safe(&netLen, srcBuffer + sizeof(netType), sizeof(netLen));
	SceUInt16 payloadLen = sceNetNtohs(netLen);

	*outLength = payloadLen;
	if (outConsumed != NULL)
		*outConsumed = NP_PUSH_IPC_TLV_HEADER_SIZE;

	if (actualType != expectedType)
		return SCE_NP_PUSH_ERROR_TLV_PARSE;
	if (bufferSize < static_cast<SceSize>(payloadLen) + NP_PUSH_IPC_TLV_HEADER_SIZE)
		return SCE_NP_PUSH_ERROR_TLV_PARSE;
	return 0;
}

/* 0x81403CBC / 0x81403CD8: TlvBase */
TlvBase::TlvBase(SceUInt16 tlvType) : m_tlvType(tlvType)
{
	SceNpCommonPs4_108738F2(this);
}

TlvBase::~TlvBase()
{
	SceNpCommonPs4_BA23AD76(this);
}

/* 0x81403CF8..0x81403EDE: TlvString */
TlvString::TlvString(SceUInt16 tlvType) : TlvBase(tlvType), m_text(NULL), m_length(0)
{
}

TlvString::~TlvString()
{
}

/* 0x81403D18 */
SceSize TlvString::GetValueSize(void) const
{
	if (m_text == NULL)
		return 0;
	return sceClibStrnlen(m_text, static_cast<SceSize>(-1)) + 1;
}

/* 0x81403D2E */
int TlvString::Serialize(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const
{
	SceUInt16 valueLen = static_cast<SceUInt16>(GetValueSize());
	SceSize headerBytes = 0;
	int ret = WriteTlvHeader(m_tlvType, valueLen, outBuffer, bufferSize, &headerBytes);
	if (ret < 0)
		return ret;

	SceSize copyLen = 0;
	if (m_text != NULL)
		copyLen = sceClibStrnlen(m_text, static_cast<SceSize>(-1)) + 1;

	if (outBuffer != NULL) {
		if (bufferSize - headerBytes < copyLen)
			return SCE_NP_PUSH_ERROR_TLV_BUFFER_TOO_SMALL;
		if (copyLen > 0) {
			sceClibMemcpy_safe(outBuffer + headerBytes, m_text, copyLen);
			outBuffer[headerBytes + copyLen - 1] = '\0';
		}
	}

	if (outWritten != NULL)
		*outWritten = headerBytes + copyLen;
	return 0;
}

/* 0x81403E1E */
int TlvString::Unserialize(char *srcBuffer, SceSize bufferSize, SceSize *outConsumed)
{
	SceUInt16 valueLen = 0;
	SceSize headerBytes = 0;
	int ret = ReadTlvHeader(m_tlvType, &valueLen, srcBuffer, bufferSize, &headerBytes);
	if (ret < 0)
		return ret;

	if (valueLen != 0) {
		if (bufferSize - headerBytes < valueLen) {
			SceNpCommonPs4_F50BD96A(sceClibPrintf(
				"Assertion in %s(%s:%d): %s(%s)\n",
				"Unserialize", NP_PUSH_IPC_TLV_SOURCE_PATH, 219, "ASSERT",
				"dataSize-total >= len"));
		}
		srcBuffer[headerBytes + valueLen - 1] = '\0';
		m_text = srcBuffer + headerBytes;
		m_length = valueLen - 1;
	} else {
		m_text = NULL;
		m_length = 0;
	}

	if (outConsumed != NULL)
		*outConsumed = headerBytes + valueLen;
	return 0;
}

/* 0x81403EDE..0x81404081: TlvJsonValue */
TlvJsonValue::TlvJsonValue(SceUInt16 tlvType) : TlvBase(tlvType), m_jsonValue(NULL)
{
}

TlvJsonValue::~TlvJsonValue()
{
}

/* 0x81403EFE */
SceSize TlvJsonValue::GetValueSize(void) const
{
	if (m_jsonValue == NULL)
		return 0;

	NpPushJsonSerializer serializer;
	SceNpCommonPs4_4A1E54C6(&serializer, GetAllocator());
	serializer.root = m_jsonValue;
	SceSize serializedSize = static_cast<SceSize>(SceNpCommonPs4_5F8A1E12(&serializer));
	serializer.root = NULL;
	SceNpCommonPs4_74F1FA26(&serializer);
	return serializedSize;
}

/* 0x81403F40 */
int TlvJsonValue::Serialize(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const
{
	SceUInt16 valueLen = static_cast<SceUInt16>(GetValueSize());
	SceSize headerBytes = 0;
	int ret = WriteTlvHeader(m_tlvType, valueLen, outBuffer, bufferSize, &headerBytes);
	if (ret < 0)
		return ret;

	SceSize totalWritten = headerBytes;
	if (m_jsonValue != NULL) {
		NpPushJsonSerializer serializer;
		SceNpCommonPs4_4A1E54C6(&serializer, GetAllocator());
		serializer.root = m_jsonValue;

		SceSize expectedLen = static_cast<SceSize>(SceNpCommonPs4_5F8A1E12(&serializer));
		if (outBuffer != NULL) {
			if (bufferSize - totalWritten < expectedLen) {
				serializer.root = NULL;
				SceNpCommonPs4_74F1FA26(&serializer);
				return SCE_NP_PUSH_ERROR_TLV_BUFFER_TOO_SMALL;
			}
			SceSize actualLen = 0;
			ret = SceNpCommonPs4_4A94BDE6(&serializer, outBuffer + totalWritten,
						      bufferSize - totalWritten, &actualLen);
			if (ret < 0) {
				serializer.root = NULL;
				SceNpCommonPs4_74F1FA26(&serializer);
				return ret;
			}
			if (actualLen != expectedLen) {
				SceNpCommonPs4_F50BD96A(sceClibPrintf(
					"Assertion in %s(%s:%d): %s(%s)\n",
					"Serialize", NP_PUSH_IPC_TLV_SOURCE_PATH, 295, "ASSERT",
					"size == dataLen"));
			}
		}

		serializer.root = NULL;
		SceNpCommonPs4_74F1FA26(&serializer);
		totalWritten += expectedLen;
	}

	if (outWritten != NULL)
		*outWritten = totalWritten;
	return 0;
}

/* 0x81404076: JSON TLV unserialization is not supported on the server side */
int TlvJsonValue::Unserialize(char *srcBuffer, SceSize bufferSize, SceSize *outConsumed)
{
	(void)srcBuffer;
	(void)bufferSize;
	(void)outConsumed;
	return SCE_NP_PUSH_ERROR_INTERNAL;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
