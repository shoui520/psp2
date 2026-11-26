/*
 * np_push_ipc_data.cpp: Server-side TLV serializer for SceNpPush IPC notification
 * payloads (root container, body JSON block, and extended data key/value pairs).
 * Sony path: vsh/app/common/src/np/np_push/ipc/np_push_ipc_data.cpp
 * 3.65-CEX: 0x814037E4..0x81403BA3
 */
#include "../np_push_internal.h"

#define NP_PUSH_IPC_TLV_HEADER_SIZE		4U
#define NP_PUSH_IPC_TLV_TAG_ROOT		1U
#define NP_PUSH_IPC_TLV_TAG_BODY		2U
#define NP_PUSH_IPC_TLV_TAG_EXDATA_ENTRY	3U
#define NP_PUSH_IPC_TLV_TAG_BODY_JSON		0x1001U
#define NP_PUSH_IPC_TLV_TAG_EXDATA_KEY		0x1002U
#define NP_PUSH_IPC_TLV_TAG_EXDATA_JSON		0x1003U

#define NP_PUSH_IPC_DATA_SOURCE_PATH \
	"W:\\Build\\" NP_PUSH_BUILD_ID "\\vsh\\app\\common\\src\\np\\np_push\\ipc\\np_push_ipc_data.cpp"

namespace sce {
namespace Np {
namespace Push {

/* 0x814037E4: Wire size of one extended-data TLV pair (header + key TLV + JSON TLV) */
static SceSize CalcExDataPairSize(const IpcExDataPair *pair)
{
	if (pair->keyName == NULL || pair->jsonValue == NULL)
		return 0;

	TlvString keyTlv(NP_PUSH_IPC_TLV_TAG_EXDATA_KEY);
	keyTlv.SetValue(pair->keyName);
	SceSize keyLen = keyTlv.GetValueSize();

	TlvJsonValue valueTlv(NP_PUSH_IPC_TLV_TAG_EXDATA_JSON);
	valueTlv.SetValue(pair->jsonValue);
	SceSize valueLen = valueTlv.GetValueSize();

	return keyLen + valueLen + 3U * NP_PUSH_IPC_TLV_HEADER_SIZE;
}

/* 0x81403864: Total serialized payload size inside the outer root TLV */
SceSize IpcNotificationPayload::GetPayloadSize(void) const
{
	SceSize totalSize = 0;

	if (bodyValue != NULL) {
		TlvJsonValue bodyTlv(NP_PUSH_IPC_TLV_TAG_BODY_JSON);
		bodyTlv.SetValue(bodyValue);
		totalSize = bodyTlv.GetValueSize() + 2U * NP_PUSH_IPC_TLV_HEADER_SIZE;
	}

	for (int index = 0; index < NP_PUSH_IPC_MAX_EXDATA_COUNT; ++index)
		totalSize += CalcExDataPairSize(&exDataPairs[index]);

	return totalSize;
}

/* 0x814038D2: Serialize one extended-data key/value TLV container */
static int SerializeExDataPair(const IpcExDataPair *pair, char *outBuffer, SceSize bufferSize,
			       SceSize *outWritten)
{
	SceSize written = 0;

	if (pair->keyName != NULL && pair->jsonValue != NULL) {
		TlvString keyTlv(NP_PUSH_IPC_TLV_TAG_EXDATA_KEY);
		keyTlv.SetValue(pair->keyName);

		TlvJsonValue valueTlv(NP_PUSH_IPC_TLV_TAG_EXDATA_JSON);
		valueTlv.SetValue(pair->jsonValue);

		SceUInt16 innerLen = static_cast<SceUInt16>(
			keyTlv.GetValueSize() + valueTlv.GetValueSize() +
			2U * NP_PUSH_IPC_TLV_HEADER_SIZE);

		SceSize stepWritten = 0;
		int ret = WriteTlvHeader(NP_PUSH_IPC_TLV_TAG_EXDATA_ENTRY, innerLen, outBuffer,
					 bufferSize, &stepWritten);
		if (ret < 0)
			return ret;
		written += stepWritten;

		ret = keyTlv.Serialize(outBuffer + written, bufferSize - written, &stepWritten);
		if (ret < 0)
			return ret;
		written += stepWritten;

		ret = valueTlv.Serialize(outBuffer + written, bufferSize - written, &stepWritten);
		if (ret < 0)
			return ret;
		written += stepWritten;
	}

	*outWritten = written;
	return 0;
}

/* 0x814039DE: Serialize the body JSON TLV container */
static int SerializeBodySection(NpPushJsonValue *bodyValue, char *outBuffer, SceSize bufferSize,
				SceSize *outWritten)
{
	SceSize written = 0;

	if (bodyValue != NULL) {
		TlvJsonValue bodyTlv(NP_PUSH_IPC_TLV_TAG_BODY_JSON);
		bodyTlv.SetValue(bodyValue);

		SceUInt16 innerLen = static_cast<SceUInt16>(
			bodyTlv.GetValueSize() + NP_PUSH_IPC_TLV_HEADER_SIZE);

		SceSize stepWritten = 0;
		int ret = WriteTlvHeader(NP_PUSH_IPC_TLV_TAG_BODY, innerLen, outBuffer,
					 bufferSize, &stepWritten);
		if (ret < 0)
			return ret;
		written += stepWritten;

		ret = bodyTlv.Serialize(outBuffer + written, bufferSize - written, &stepWritten);
		if (ret < 0)
			return ret;
		written += stepWritten;
	}

	*outWritten = written;
	return 0;
}

void IpcNotificationPayload::Init(void)
{
	bodyValue = NULL;
	sceClibMemset(exDataPairs, 0, sizeof(exDataPairs));
}

/* 0x81403B5A: Attach notification body JSON value */
int IpcNotificationPayload::SetBody(NpPushJsonValue *value)
{
	if (bodyValue != NULL)
		return SCE_NP_PUSH_ERROR_TLV_ALREADY_SET;
	bodyValue = value;
	return 0;
}

/* 0x81403B76: Append an extended-data key/value JSON pair */
int IpcNotificationPayload::AddExData(const char *keyName, NpPushJsonValue *value)
{
	for (int index = 0; index < NP_PUSH_IPC_MAX_EXDATA_COUNT; ++index) {
		if (exDataPairs[index].keyName == NULL) {
			exDataPairs[index].keyName = keyName;
			exDataPairs[index].jsonValue = value;
			return 0;
		}
	}
	return SCE_NP_PUSH_ERROR_TLV_EXDATA_FULL;
}

/* 0x81403A82: Build the full TLV notification buffer */
int IpcNotificationPayload::Build(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const
{
	SceUInt16 payloadLen = static_cast<SceUInt16>(GetPayloadSize());
	SceSize stepWritten = 0;
	int ret = WriteTlvHeader(NP_PUSH_IPC_TLV_TAG_ROOT, payloadLen, outBuffer, bufferSize,
				 &stepWritten);
	if (ret < 0)
		return ret;

	SceSize totalWritten = stepWritten;
	ret = SerializeBodySection(bodyValue, outBuffer + totalWritten,
				   bufferSize - totalWritten, &stepWritten);
	if (ret < 0)
		return ret;
	totalWritten += stepWritten;

	for (int index = 0; index < NP_PUSH_IPC_MAX_EXDATA_COUNT; ++index) {
		ret = SerializeExDataPair(&exDataPairs[index], outBuffer + totalWritten,
					  bufferSize - totalWritten, &stepWritten);
		if (ret < 0)
			return ret;
		totalWritten += stepWritten;
	}

	if (totalWritten != GetPayloadSize() + NP_PUSH_IPC_TLV_HEADER_SIZE) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf(
			"Assertion in %s(%s:%d): %s(%s)\n",
			"Build", NP_PUSH_IPC_DATA_SOURCE_PATH, 122, "ASSERT",
			"total == Size()"));
	}

	*outWritten = totalWritten;
	return 0;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
