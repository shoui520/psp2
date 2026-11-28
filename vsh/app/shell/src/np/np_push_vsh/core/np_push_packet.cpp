/*
 * np_push_packet.cpp: JSON push protocol packet construction, parsing, and field accessors.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_packet.cpp
 * 3.65-CEX: 0x813AD2E2..0x813AEACC
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* Recursive JSON schema allowing arbitrary nested objects/arrays up to depth 11 */
static const NpPushJsonLexerNode kPacketRootNodes[] = {
	{ NP_PUSH_LEXER_NODE_KEY,	NULL, 1 },
	{ NP_PUSH_LEXER_NODE_END,	"",   0 }
};

static const NpPushJsonLexerNode kPacketChildNodes[] = {
	{ NP_PUSH_LEXER_NODE_KEY,	NULL, 1 },
	{ NP_PUSH_LEXER_NODE_ELEMENT,	NULL, 1 },
	{ NP_PUSH_LEXER_NODE_END,	"",   0 }
};

static const NpPushJsonLexerNode *const kPacketParseLevels[] = {
	kPacketRootNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes,
	kPacketChildNodes
};

static const NpPushJsonLexerConfig kPacketParseConfig = {
	kPacketParseLevels,
	11
};

static volatile SceUInt32 s_tranIdSequence = 0;

static void PacketRefDestroy(NpPushRefCounted *base)
{
	Packet *packet = static_cast<Packet *>(base);
	packet->~Packet();
	SceNpCommonPs4_6B428829(GetAllocator(), packet);
}

static const void *const kPacketVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(PacketRefDestroy),
	reinterpret_cast<const void *>(PacketRefDestroy)
};

static void DestroyJsonValue(NpPushJsonValue *value)
{
	if (value == NULL)
		return;
	typedef void (*DestructorFn)(NpPushJsonValue *);
	DestructorFn deleteFn = reinterpret_cast<DestructorFn>(value->vtable[1]);
	deleteFn(value);
}

static const char *PresenceStatusToString(int status)
{
	if (status == PRESENCE_STATUS_ACTIVE)
		return "active";
	if (status == PRESENCE_STATUS_SLEEP)
		return "sleep";
	return "offline";
}

/* 0x813AD2E2 */
Packet *Packet::Create(void)
{
	void *rawMem = SceNpCommonPs4_0C922423(sizeof(Packet), GetAllocator());
	if (rawMem == NULL)
		return NULL;
	return new (rawMem) Packet();
}

/* 0x813AD320 */
void Packet::Destroy(Packet *packet)
{
	if (packet != NULL)
		SceNpCommonPs4_93C97239(packet);
}

/* 0x813AD338 */
Packet::Packet() : m_root(NULL), m_method(PACKET_METHOD_NONE)
{
	SceNpCommonPs4_108738F2(this);
	vtable = &kPacketVtable[2];
}

/* 0x813AD36C */
Packet::~Packet()
{
	if (m_root != NULL) {
		DestroyJsonValue(m_root);
		m_root = NULL;
	}
	SceNpCommonPs4_BA23AD76(this);
}

/* 0x813AD3C0: GenerateTranId */
int Packet::GenerateTranId(char *outBuf, SceSize bufSize)
{
	if (outBuf == NULL || bufSize == 0)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	SceUInt32 seq = SceNpCommonPs4_741F88FE(&s_tranIdSequence) + 1U;
	int written = sceClibSnprintf(outBuf, bufSize, "psp2-%u", seq);
	if (written < 0 || static_cast<SceSize>(written) >= bufSize)
		return SCE_NP_PUSH_ERROR_BUFFER_TOO_SMALL;
	return 0;
}

/* 0x813AD420: Parse */
int Packet::Parse(const char *jsonText, SceSize length, RefPtr<Packet> *outPacket)
{
	NP_PUSH_ASSERT(jsonText != NULL, "Parse", "core\\np_push_packet.cpp", 112,
		       "jsonText != NULL");
	NP_PUSH_ASSERT(outPacket != NULL, "Parse", "core\\np_push_packet.cpp", 113,
		       "outPacket != NULL");

	NpPushJsonDocument document;
	SceNpCommonPs4_FAE00F18(&document, GetAllocator(),
				const_cast<NpPushJsonLexerConfig *>(&kPacketParseConfig));

	int ret = SceNpCommonPs4_9A770D9A(&document, jsonText, static_cast<int>(length));
	if (ret < 0) {
		SceNpCommonPs4_B09D64DB(&document);
		return SCE_NP_PUSH_ERROR_JSON_PARSE;
	}

	NpPushJsonValue *root = NULL;
	ret = SceNpCommonPs4_B3078188(&document, &root);
	SceNpCommonPs4_B09D64DB(&document);
	if (ret < 0 || root == NULL)
		return SCE_NP_PUSH_ERROR_JSON_PARSE;

	if (root->valueType != NP_PUSH_JSON_TYPE_OBJECT) {
		DestroyJsonValue(root);
		return SCE_NP_PUSH_ERROR_INVALID_PACKET;
	}

	Packet *packet = Packet::Create();
	if (packet == NULL) {
		DestroyJsonValue(root);
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	packet->m_root = root;
	SceInt32 parsedMethod = PACKET_METHOD_NONE;
	if (SceNpCommonPs4_066F0583(root, "method", &parsedMethod) >= 0)
		packet->m_method = parsedMethod;
	outPacket->Adopt(packet);
	return 0;
}

/* 0x813AD5A0: BuildBufSize */
int Packet::BuildBufSize(SceSize *outSize) const
{
	NP_PUSH_ASSERT(m_root != NULL, "BuildBufSize", "core\\np_push_packet.cpp", 168,
		       "m_root != NULL");
	NP_PUSH_ASSERT(outSize != NULL, "BuildBufSize", "core\\np_push_packet.cpp", 169,
		       "outSize != NULL");

	NpPushJsonSerializer serializer;
	SceNpCommonPs4_4A1E54C6(&serializer, GetAllocator());
	serializer.root = m_root;

	SceSize required = 0;
	int ret = SceNpCommonPs4_4A94BDE6(&serializer, NULL, 0, &required);
	serializer.root = NULL;
	SceNpCommonPs4_74F1FA26(&serializer);

	if (ret < 0)
		return SCE_NP_PUSH_ERROR_JSON_BUILD;

	*outSize = required;
	return 0;
}

/* 0x813AD658: Build */
int Packet::Build(char *outBuf, SceSize bufSize, SceSize *outWritten) const
{
	NP_PUSH_ASSERT(m_root != NULL, "Build", "core\\np_push_packet.cpp", 198,
		       "m_root != NULL");
	NP_PUSH_ASSERT(outBuf != NULL, "Build", "core\\np_push_packet.cpp", 199,
		       "outBuf != NULL");

	NpPushJsonSerializer serializer;
	SceNpCommonPs4_4A1E54C6(&serializer, GetAllocator());
	serializer.root = m_root;

	SceSize required = 0;
	int ret = SceNpCommonPs4_4A94BDE6(&serializer, outBuf, bufSize, &required);
	serializer.root = NULL;
	SceNpCommonPs4_74F1FA26(&serializer);

	if (ret < 0)
		return SCE_NP_PUSH_ERROR_JSON_BUILD;

	if (outWritten != NULL)
		*outWritten = required;
	return 0;
}

/* 0x813AD718: InitRequest */
int Packet::InitRequest(int methodCode)
{
	NP_PUSH_ASSERT(m_root == NULL, "InitRequest", "core\\np_push_packet.cpp", 232,
		       "m_root == NULL");

	void *rawVal = SceNpCommonPs4_0C922423(sizeof(NpPushJsonValue), GetAllocator());
	if (rawVal == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	NpPushJsonValue *root = SceNpCommonPs4_D309C0B3(static_cast<NpPushJsonValue *>(rawVal),
							GetAllocator(), NP_PUSH_JSON_TYPE_OBJECT);
	int ret = SceNpCommonPs4_13B11D92(root, "method", methodCode, NULL);
	if (ret < 0) {
		DestroyJsonValue(root);
		return ret;
	}

	m_root = root;
	m_method = methodCode;
	return 0;
}

/* 0x813AD7D8: SetTranId */
int Packet::SetTranId(const char *tranId)
{
	NP_PUSH_ASSERT(m_root != NULL, "SetTranId", "core\\np_push_packet.cpp", 258,
		       "m_root != NULL");
	NP_PUSH_ASSERT(tranId != NULL, "SetTranId", "core\\np_push_packet.cpp", 259,
		       "tranId != NULL");

	return SceNpCommonPs4_977CBCE6(m_root, "tranId", tranId, NULL);
}

/* 0x813AD848: SetOnlineId */
int Packet::SetOnlineId(const NpPushOnlineId *onlineId)
{
	NP_PUSH_ASSERT(m_root != NULL, "SetOnlineId", "core\\np_push_packet.cpp", 274,
		       "m_root != NULL");
	NP_PUSH_ASSERT(onlineId != NULL, "SetOnlineId", "core\\np_push_packet.cpp", 275,
		       "onlineId != NULL");

	NpPushJsonValue *fromObj = SceNpCommonPs4_42CAF6ED(m_root, "from");
	if (fromObj == NULL) {
		int ret = SceNpCommonPs4_C47C2F93(m_root, "from", &fromObj);
		if (ret < 0)
			return ret;
	}

	return SceNpCommonPs4_977CBCE6(fromObj, "onlineId", onlineId->text, NULL);
}

/* 0x813AD908: SetAuthAccessToken */
int Packet::SetAuthAccessToken(const char *accessToken)
{
	NP_PUSH_ASSERT(m_root != NULL, "SetAuthAccessToken", "core\\np_push_packet.cpp", 299,
		       "m_root != NULL");
	NP_PUSH_ASSERT(accessToken != NULL, "SetAuthAccessToken", "core\\np_push_packet.cpp", 300,
		       "accessToken != NULL");

	NpPushJsonValue *authObj = SceNpCommonPs4_42CAF6ED(m_root, "auth");
	if (authObj == NULL) {
		int ret = SceNpCommonPs4_C47C2F93(m_root, "auth", &authObj);
		if (ret < 0)
			return ret;
	}

	return SceNpCommonPs4_977CBCE6(authObj, "accessToken", accessToken, NULL);
}

/* 0x813AD9CC: CreateLoginRequest */
int Packet::CreateLoginRequest(const NpPushOnlineId *onlineId, int initialStatus,
			       RefPtr<Packet> *outPacket)
{
	if (onlineId == NULL || outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	RefPtr<Packet> packet;
	packet.Adopt(Packet::Create());
	if (packet.IsNull())
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	int ret = packet->InitRequest(PACKET_METHOD_LOGIN);
	if (ret < 0)
		return ret;

	ret = packet->SetOnlineId(onlineId);
	if (ret < 0)
		return ret;

	NpPushJsonValue *bodyObj = NULL;
	ret = SceNpCommonPs4_C47C2F93(packet->m_root, "body", &bodyObj);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_977CBCE6(bodyObj, "status", PresenceStatusToString(initialStatus),
				      NULL);
	if (ret < 0)
		return ret;

	*outPacket = packet;
	return 0;
}

/* 0x813ADB10: CreateLogoutRequest */
int Packet::CreateLogoutRequest(const NpPushOnlineId *onlineId, RefPtr<Packet> *outPacket)
{
	if (onlineId == NULL || outPacket == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	RefPtr<Packet> packet;
	packet.Adopt(Packet::Create());
	if (packet.IsNull())
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	int ret = packet->InitRequest(PACKET_METHOD_LOGOUT);
	if (ret < 0)
		return ret;

	ret = packet->SetOnlineId(onlineId);
	if (ret < 0)
		return ret;

	*outPacket = packet;
	return 0;
}

/* 0x813ADBE0: CreateChangeStatusRequest */
int Packet::CreateChangeStatusRequest(const NpPushOnlineId *onlineId, int newStatus,
				      RefPtr<Packet> *outPacket)
{
	NP_PUSH_ASSERT(onlineId != NULL, "CreateChangeStatusRequest",
		       "core\\np_push_packet.cpp", 386, "onlineId != NULL");
	NP_PUSH_ASSERT(outPacket != NULL, "CreateChangeStatusRequest",
		       "core\\np_push_packet.cpp", 387, "outPacket != NULL");

	RefPtr<Packet> packet;
	packet.Adopt(Packet::Create());
	if (packet.IsNull())
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	int ret = packet->InitRequest(PACKET_METHOD_CHANGE_STATUS);
	if (ret < 0)
		return ret;

	ret = packet->SetOnlineId(onlineId);
	if (ret < 0)
		return ret;

	NpPushJsonValue *bodyObj = NULL;
	ret = SceNpCommonPs4_C47C2F93(packet->m_root, "body", &bodyObj);
	if (ret < 0)
		return ret;

	ret = SceNpCommonPs4_977CBCE6(bodyObj, "status", PresenceStatusToString(newStatus), NULL);
	if (ret < 0)
		return ret;

	*outPacket = packet;
	return 0;
}

/* 0x813ADD40: GetMethod */
int Packet::GetMethod(int *outMethod) const
{
	if (m_root == NULL || outMethod == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	SceInt32 method = 0;
	int ret = SceNpCommonPs4_066F0583(m_root, "method", &method);
	if (ret < 0)
		return ret;

	*outMethod = method;
	return 0;
}

/* 0x813ADD98: HasTranId */
bool Packet::HasTranId(void) const
{
	if (m_root == NULL)
		return false;
	return SceNpCommonPs4_E8740CE6(m_root, "tranId") != NULL;
}

/* 0x813ADDC4: GetTranId */
int Packet::GetTranId(char *outBuf, SceSize bufSize) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetTranId", "core\\np_push_packet.cpp", 442,
		       "m_root != NULL");

	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "tranId");
	if (val == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	return SceNpCommonPs4_86E584A0(val, outBuf, bufSize);
}

/* 0x813ADE2C: HasReturnCode */
bool Packet::HasReturnCode(void) const
{
	NP_PUSH_ASSERT(m_root != NULL, "HasReturnCode", "core\\np_push_packet.cpp", 459,
		       "m_root != NULL");
	return SceNpCommonPs4_C4525618(m_root, "returnCode") != NULL;
}

/* 0x813ADE6C: GetReturnCode */
int Packet::GetReturnCode(SceInt32 *outCode) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetReturnCode", "core\\np_push_packet.cpp", 469,
		       "m_root != NULL");
	return SceNpCommonPs4_066F0583(m_root, "returnCode", outCode);
}

/* 0x813ADEC0: HasRetryInterval */
bool Packet::HasRetryInterval(void) const
{
	NP_PUSH_ASSERT(m_root != NULL, "HasRetryInterval", "core\\np_push_packet.cpp", 482,
		       "m_root != NULL");
	return SceNpCommonPs4_C4525618(m_root, "retryInterval") != NULL;
}

/* 0x813ADF00: GetRetryInterval */
int Packet::GetRetryInterval(SceUInt32 *outIntervalSec) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetRetryInterval", "core\\np_push_packet.cpp", 492,
		       "m_root != NULL");
	return SceNpCommonPs4_64EE86C7(m_root, "retryInterval", outIntervalSec);
}

/* 0x813ADF54: HasErrorSource */
bool Packet::HasErrorSource(void) const
{
	NP_PUSH_ASSERT(m_root != NULL, "HasErrorSource", "core\\np_push_packet.cpp", 505,
		       "m_root != NULL");
	return SceNpCommonPs4_E8740CE6(m_root, "errorSource") != NULL;
}

/* 0x813ADF94: GetErrorSource */
int Packet::GetErrorSource(char *outBuf, SceSize bufSize) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetErrorSource", "core\\np_push_packet.cpp", 515,
		       "m_root != NULL");
	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "errorSource");
	if (val == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;
	return SceNpCommonPs4_86E584A0(val, outBuf, bufSize);
}

/* 0x813ADFFC: HasDataType */
bool Packet::HasDataType(void) const
{
	NP_PUSH_ASSERT(m_root != NULL, "HasDataType", "core\\np_push_packet.cpp", 531,
		       "m_root != NULL");
	return SceNpCommonPs4_E8740CE6(m_root, "dataType") != NULL;
}

/* 0x813AE03C: GetDataType */
int Packet::GetDataType(char *outBuf, SceSize bufSize) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetDataType", "core\\np_push_packet.cpp", 541,
		       "m_root != NULL");
	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "dataType");
	if (val == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;
	return SceNpCommonPs4_86E584A0(val, outBuf, bufSize);
}

/* 0x813AE066: Prefix match helper on dataType */
bool Packet::MatchDataTypePrefix(const char *prefix) const
{
	if (m_root == NULL || prefix == NULL)
		return false;
	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "dataType");
	if (val == NULL)
		return false;
	const char *text = SceNpCommonPs4_ED539A9D(val);
	if (text == NULL)
		return false;
	SceSize prefixLen = sceClibStrnlen(prefix, 64);
	return sceClibStrncmp(text, prefix, prefixLen) == 0;
}

/* Exact match helper on dataType */
bool Packet::MatchDataTypeExact(const char *pattern) const
{
	if (m_root == NULL || pattern == NULL)
		return false;
	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "dataType");
	if (val == NULL)
		return false;
	const char *text = SceNpCommonPs4_ED539A9D(val);
	if (text == NULL)
		return false;
	return sceClibStrncmp(text, pattern, 64) == 0;
}

/* 0x813AE140: HasQuery */
bool Packet::HasQuery(void) const
{
	NP_PUSH_ASSERT(m_root != NULL, "HasQuery", "core\\np_push_packet.cpp", 574,
		       "m_root != NULL");
	return SceNpCommonPs4_42CAF6ED(m_root, "query") != NULL;
}

/* 0x813AE180: GetQueryType */
int Packet::GetQueryType(char *outBuf, SceSize bufSize) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetQueryType", "core\\np_push_packet.cpp", 584,
		       "m_root != NULL");
	NpPushJsonValue *queryObj = SceNpCommonPs4_42CAF6ED(m_root, "query");
	if (queryObj == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;
	NpPushJsonValue *typeVal = SceNpCommonPs4_E8740CE6(queryObj, "type");
	if (typeVal == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;
	return SceNpCommonPs4_86E584A0(typeVal, outBuf, bufSize);
}

/* 0x813AE280: GetFromOnlineId */
int Packet::GetFromOnlineId(NpPushOnlineIdData *outOnlineId) const
{
	if (m_root == NULL || outOnlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushJsonValue *fromObj = SceNpCommonPs4_42CAF6ED(m_root, "from");
	if (fromObj == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	NpPushJsonValue *idVal = SceNpCommonPs4_E8740CE6(fromObj, "onlineId");
	if (idVal == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	sceClibMemset(outOnlineId, 0, sizeof(*outOnlineId));
	return SceNpCommonPs4_86E584A0(idVal, outOnlineId->text, sizeof(outOnlineId->text) + 1);
}

/* 0x813AE380: GetToOnlineId */
int Packet::GetToOnlineId(NpPushOnlineIdData *outOnlineId) const
{
	if (m_root == NULL || outOnlineId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushJsonValue *toObj = SceNpCommonPs4_42CAF6ED(m_root, "to");
	if (toObj == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	NpPushJsonValue *idVal = SceNpCommonPs4_E8740CE6(toObj, "onlineId");
	if (idVal == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	sceClibMemset(outOnlineId, 0, sizeof(*outOnlineId));
	return SceNpCommonPs4_86E584A0(idVal, outOnlineId->text, sizeof(outOnlineId->text) + 1);
}

/* 0x813AE480: GetToCommId */
int Packet::GetToCommId(NpPushCommunicationIdData *outCommId) const
{
	if (m_root == NULL || outCommId == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushJsonValue *toObj = SceNpCommonPs4_42CAF6ED(m_root, "to");
	if (toObj == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	NpPushJsonValue *commVal = SceNpCommonPs4_E8740CE6(toObj, "npCommunicationId");
	if (commVal == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	const char *commStr = SceNpCommonPs4_ED539A9D(commVal);
	if (commStr == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	NpPushCommunicationId parsed;
	SceNpCommonPs4_79576C83(&parsed);
	int ret = SceNpCommonPs4_54F72557(&parsed, commStr);
	if (ret >= 0) {
		sceClibMemcpy_safe(outCommId->text, parsed.text, sizeof(outCommId->text));
		outCommId->term = parsed.term;
		outCommId->num = parsed.num;
		outCommId->padding = parsed.padding;
	}
	SceNpCommonPs4_511C5E91(&parsed);
	return ret;
}

/* 0x813AE600: GetStatus */
int Packet::GetStatus(int *outStatus) const
{
	if (m_root == NULL || outStatus == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	NpPushJsonValue *bodyObj = SceNpCommonPs4_42CAF6ED(m_root, "body");
	if (bodyObj == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	NpPushJsonValue *statusVal = SceNpCommonPs4_E8740CE6(bodyObj, "status");
	if (statusVal == NULL)
		return SCE_NP_PUSH_ERROR_NOT_FOUND;

	const char *statusText = SceNpCommonPs4_ED539A9D(statusVal);
	if (statusText == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_PACKET;

	if (sceClibStrncmp(statusText, "active", 7) == 0) {
		*outStatus = PRESENCE_STATUS_ACTIVE;
		return 0;
	}
	if (sceClibStrncmp(statusText, "sleep", 6) == 0) {
		*outStatus = PRESENCE_STATUS_SLEEP;
		return 0;
	}
	if (sceClibStrncmp(statusText, "offline", 8) == 0) {
		*outStatus = PRESENCE_STATUS_OFFLINE;
		return 0;
	}
	return SCE_NP_PUSH_ERROR_INVALID_PACKET;
}

/* 0x813AE780: GetBodyData */
NpPushJsonValue *Packet::GetBodyData(void) const
{
	if (m_root == NULL)
		return NULL;
	return SceNpCommonPs4_42CAF6ED(m_root, "body");
}

/* 0x813AE7D0: GetExtendedData */
NpPushJsonValue *Packet::GetExtendedData(void) const
{
	if (m_root == NULL)
		return NULL;
	return SceNpCommonPs4_42CAF6ED(m_root, "extendedData");
}

/* 0x813ADB3E */
int Packet::GetTranIdFixed(char *outTranId) const
{
	NP_PUSH_ASSERT(m_root != NULL, "GetTranId", "core\\np_push_packet.cpp", 398,
		       "m_obj != 0");

	NpPushJsonValue *val = SceNpCommonPs4_E8740CE6(m_root, "tranId");
	if (val == NULL)
		return SCE_NP_PUSH_ERROR_TIMEOUT;

	const char *text = SceNpCommonPs4_ED539A9D(val);
	int ret = TranIdCopy(outTranId, text);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813AE0E6 */
bool Packet::HasFrom(void) const
{
	return SceNpCommonPs4_C4525618(m_root, "from") != NULL;
}

/* 0x813AE1F4 */
int Packet::GetFromAccountInfo(FromAccountInfo *outInfo) const
{
	NpPushJsonValue *fromObj = SceNpCommonPs4_C4525618(m_root, "from");
	if (fromObj == NULL)
		return SCE_NP_PUSH_ERROR_MAX;

	int ret = SceNpCommonPs4_63174832(fromObj, "accountId", &outInfo->accountId);
	if (ret < 0)
		return ret;

	sceClibMemset(outInfo->onlineId, 0, sizeof(outInfo->onlineId) + sizeof(outInfo->opt) +
		      sizeof(outInfo->platform) + sizeof(outInfo->valid));

	NpPushJsonValue *onlineIdVal = SceNpCommonPs4_E8740CE6(fromObj, "onlineId");
	if (onlineIdVal == NULL)
		return SCE_NP_PUSH_ERROR_MAX;
	const char *onlineIdStr = SceNpCommonPs4_ED539A9D(onlineIdVal);
	sceClibStrncpy(outInfo->onlineId, onlineIdStr, 16);

	NpPushJsonValue *optVal = SceNpCommonPs4_E8740CE6(fromObj, "opt");
	if (optVal == NULL)
		return SCE_NP_PUSH_ERROR_MAX;
	const char *optStr = SceNpCommonPs4_ED539A9D(optVal);
	if (sceClibStrnlen(optStr, 0xFFFFFFFFU) != 4)
		return SCE_NP_PUSH_ERROR_MAX;
	sceClibMemcpy_safe(outInfo->opt, optStr, 4);

	NpPushJsonValue *platformVal = SceNpCommonPs4_E8740CE6(fromObj, "platform");
	if (platformVal == NULL)
		return SCE_NP_PUSH_ERROR_MAX;
	const char *platformStr = SceNpCommonPs4_ED539A9D(platformVal);

	int platformCode = 0;
	if (sceClibStrncmp(platformStr, "ORBIS", 6) == 0 ||
	    sceClibStrncmp(platformStr, "PS4", 4) == 0) {
		platformCode = 3;
	} else if (sceClibStrncmp(platformStr, "PS3", 4) == 0) {
		platformCode = 1;
	} else if (sceClibStrncmp(platformStr, "PSVITA", 7) == 0) {
		platformCode = 2;
	}

	if (platformCode == 2) {
		outInfo->platform[0] = 'p';
		outInfo->platform[1] = 's';
		outInfo->platform[2] = 'p';
		outInfo->platform[3] = '2';
	} else if (platformCode == 3) {
		outInfo->platform[0] = 'p';
		outInfo->platform[1] = 's';
		outInfo->platform[2] = '4';
		outInfo->platform[3] = '\0';
	} else if (platformCode == 1) {
		outInfo->platform[0] = 'p';
		outInfo->platform[1] = 's';
		outInfo->platform[2] = '3';
		outInfo->platform[3] = '\0';
	}

	outInfo->valid = 1;
	return 0;
}

/* 0x813AE484 */
bool Packet::HasToCommId(void) const
{
	NpPushJsonValue *toObj = SceNpCommonPs4_C4525618(m_root, "to");
	return toObj != NULL && SceNpCommonPs4_E8740CE6(toObj, "npComId") != NULL;
}

/* 0x813AE5AC */
bool Packet::HasStatus(void) const
{
	return m_method == PACKET_METHOD_LOGIN &&
	       SceNpCommonPs4_E8740CE6(m_root, "status") != NULL;
}

/* 0x813AE6F0 */
NpPushJsonValue *Packet::GetExtendedDataEntry(int index, char **outKeyName) const
{
	NpPushJsonValue *bodyObj = SceNpCommonPs4_C4525618(m_root, "body");
	if (bodyObj == NULL)
		return NULL;
	NpPushJsonValue *extObj = SceNpCommonPs4_C4525618(bodyObj, "extendedData");
	if (extObj == NULL)
		return NULL;

	int count = SceNpCommonPs4_9C3F1645(extObj);
	if (count <= 0)
		return NULL;
	for (int checkIdx = 0; checkIdx < count; ++checkIdx) {
		char *probeKey = NULL;
		if (SceNpCommonPs4_24D09586(extObj, checkIdx, &probeKey) == NULL)
			return NULL;
	}
	if (index >= count)
		return NULL;

	NpPushJsonValue *entryObj = SceNpCommonPs4_24D09586(extObj, index, outKeyName);
	if (entryObj == NULL)
		return NULL;
	return SceNpCommonPs4_C4525618(entryObj, "value");
}

/* 0x813AE7D0 */
bool Packet::IsReqTransResponse(void) const
{
	if (m_method < 1000 || m_method >= 2000)
		return false;
	NP_PUSH_ASSERT(m_root != NULL, "HasReturnCode", "core\\np_push_packet.cpp", 489,
		       "m_obj != 0");
	return SceNpCommonPs4_42CAF6ED(m_root, "returnCode") != NULL;
}

/* 0x813AE83C */
bool Packet::IsLoginMethod(void) const
{
	return m_method == PACKET_METHOD_LOGIN;
}

/* 0x813AE84C */
bool Packet::IsNotificationMethod(void) const
{
	return m_method == PACKET_METHOD_NOTIFICATION;
}

/* 0x813AE914 */
bool Packet::IsServiceQueryRequest(void) const
{
	if (m_method != PACKET_METHOD_SERVICE_COMM)
		return false;
	NP_PUSH_ASSERT(m_root != NULL, "HasQuery", "core\\np_push_packet.cpp", 578,
		       "m_obj != 0");
	if (SceNpCommonPs4_E8740CE6(m_root, "query") == NULL)
		return false;
	char queryBuf[32];
	if (GetQueryType(queryBuf, sizeof(queryBuf)) != 0)
		return false;
	return sceClibStrncmp(queryBuf, "request", 8) == 0 ||
	       sceClibStrncmp(queryBuf, "notify", 7) == 0;
}

/* 0x813AE9AC */
bool Packet::IsServiceQueryResponse(void) const
{
	if (m_method != PACKET_METHOD_SERVICE_COMM)
		return false;
	NP_PUSH_ASSERT(m_root != NULL, "HasQuery", "core\\np_push_packet.cpp", 578,
		       "m_obj != 0");
	if (SceNpCommonPs4_E8740CE6(m_root, "query") == NULL)
		return false;
	char queryBuf[32];
	if (GetQueryType(queryBuf, sizeof(queryBuf)) != 0)
		return false;
	return sceClibStrncmp(queryBuf, "response", 9) == 0 ||
	       sceClibStrncmp(queryBuf, "error", 6) == 0;
}

/* 0x813AEA44 */
bool TranIdEquals(const char *left, const char *right)
{
	return sceClibStrncmp(left, right, 32) == 0;
}

/* 0x813AEA9E */
int TranIdCopy(char *dst, const char *src)
{
	if (sceClibStrnlen(src, 0xFFFFFFFFU) >= 32)
		return SCE_NP_PUSH_ERROR_BUFFER_TOO_SMALL;
	sceClibStrncpy(dst, src, 32);
	dst[31] = '\0';
	return 0;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
