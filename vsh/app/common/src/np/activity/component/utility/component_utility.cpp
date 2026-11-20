/*
 * The utility component: the activity server's addresses, the static resource's signature, base64,
 * JID and time conversions (Sony's file: its checks name component/utility/component_utility.cpp;
 * the cached host and trophy icon URLs are np_activity.cpp's)
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include <rtc.h>
#include "../../common/activity_debug.h"
#include "../../common/activity_hmac.h"
#include "../../common/activity_memory.h"
#include "../../np_activity.h"
#include "component_utility.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\utility\\component_utility.cpp"

/* the static resource's signature: the HMAC-SHA1 of the document after its first line (the
   signature comment), padded with zeros to whole 64-byte blocks, as hex */
#define STATIC_RESOURCE_KEY_SIZE	ACTIVITY_SHA1_DIGEST_SIZE
#define STATIC_RESOURCE_KEY_MASK	0x7F
#define STATIC_RESOURCE_BLOCK_SIZE	64

/* the signature's key, each byte XORed with STATIC_RESOURCE_KEY_MASK */
static SceUInt8 s_staticResourceKey[STATIC_RESOURCE_KEY_SIZE] = {
	0xab, 0x4d, 0x25, 0xf2, 0x4f, 0xbe, 0xe9, 0x0e, 0x5a, 0x06,
	0x9f, 0xd8, 0xb0, 0xf0, 0x6b, 0xc5, 0x48, 0x9b, 0x1a, 0x99,
};

/* ConvertSceRtcTickToTimeText's text: room for any RFC 3339 time */
#define UTILITY_TIME_TEXT_SIZE	0x80

ComponentUtility::ComponentUtility()
{
	sceClibMemset(m_host, 0, sizeof(m_host));
	m_npVersionValid = 0;
	sceClibMemset(m_npVersion, 0, sizeof(m_npVersion));
	sceClibMemset(m_npEnvironment, 0, sizeof(m_npEnvironment));
}

ComponentUtility::~ComponentUtility()
{
}

int ComponentUtility::IsValidStaticResource(const char *data, SceSize size, const char *digest,
	SceSize digestSize)
{
	char hex[3];
	SceUInt8 key[STATIC_RESOURCE_KEY_SIZE];
	SceUInt8 hmac[ACTIVITY_SHA1_DIGEST_SIZE];
	SceUInt8 expected[ACTIVITY_SHA1_DIGEST_SIZE];
	const char *line;
	const char *body;
	int bodySize;
	SceSize remaining;
	SceSize bufferSize;
	void *buffer;
	unsigned int value;
	SceSize i;
	int j;

	if (data == NULL || size == 0 || digest == NULL || digestSize == 0)
		return 0;

	/* the signed part starts after the first line */
	line = data;
	remaining = size;
	for (i = 0; i < size; i++) {
		if (line[0] == '\r' && size > 2 && line[1] == '\n') {
			body = line + 2;
			bodySize = remaining - 2;
			goto found;
		}
		if (line[0] == '\n') {
			body = line + 1;
			bodySize = remaining - 1;
			goto found;
		}
		line++;
		remaining--;
	}
	return 0;

found:
	for (j = 0; j < STATIC_RESOURCE_KEY_SIZE; j++)
		key[j] = s_staticResourceKey[j] ^ STATIC_RESOURCE_KEY_MASK;
	sceClibMemset(hmac, 0, sizeof(hmac));
	bufferSize = (STATIC_RESOURCE_BLOCK_SIZE - bodySize % STATIC_RESOURCE_BLOCK_SIZE) %
		STATIC_RESOURCE_BLOCK_SIZE + bodySize;
	buffer = activityMalloc(bufferSize);
	if (buffer == NULL)
		return 0;
	sceClibMemset(buffer, 0, bufferSize);
	sceClibMemcpy_safe(buffer, body, bodySize);
	activityHmacSha1(key, sizeof(key), buffer, bufferSize, hmac);
	activityFree(buffer);

	if (digestSize < 2 * ACTIVITY_SHA1_DIGEST_SIZE)
		return 0;
	for (j = 0; j < ACTIVITY_SHA1_DIGEST_SIZE; j++) {
		sceClibMemcpy_safe(hex, digest, 2);
		hex[2] = '\0';
		value = SceNpCommon_4C4AA451(hex, NULL, 16);
		if (value >= 0x100)
			return 0;
		expected[j] = value;
		digest += 2;
	}
	if (sceClibMemcmp(expected, hmac, sizeof(hmac)) != 0)
		return 0;
	return 1;
}

int ComponentUtility::GetProtocol(int api)
{
	if (IsFunctionServer())
		return GetSecureProtocol(api);

	switch (api) {
	case UTILITY_API_STATIC_RESOURCE:
		return UTILITY_PROTOCOL_HTTP;
	case UTILITY_API_NONE:
	case UTILITY_API_GET_ACCOUNT_SETTING:
	case UTILITY_API_SET_ACCOUNT_SETTING:
	case UTILITY_API_CHECK_STATUS:
	case UTILITY_API_GET_TIMELINE:
	case UTILITY_API_GET_TIMELINE_BY_TITLE:
	case UTILITY_API_GET_ACCOUNT_STATUS:
	case UTILITY_API_GET_ACCOUNT_STATUS_BY_TITLE:
	case UTILITY_API_GET_STATUS_INFO:
	case UTILITY_API_GET_STATUS_DETAIL:
	case UTILITY_API_SET_MULTIPLE_STATUS:
	case UTILITY_API_DELETE_STATUS:
	case UTILITY_API_SET_COMMENT:
	case UTILITY_API_DELETE_COMMENT:
	case UTILITY_API_SET_VOTE:
	case UTILITY_API_DELETE_VOTE:
		return UTILITY_PROTOCOL_HTTPS;
	default:
		return UTILITY_PROTOCOL_HTTP;
	}
}

/* the protocol of the function server's paths, where the reads go over HTTP */
int ComponentUtility::GetSecureProtocol(int api)
{
	switch (api) {
	case UTILITY_API_STATIC_RESOURCE:
	case UTILITY_API_CHECK_STATUS:
	case UTILITY_API_GET_TIMELINE:
	case UTILITY_API_GET_TIMELINE_BY_TITLE:
	case UTILITY_API_GET_ACCOUNT_STATUS:
	case UTILITY_API_GET_ACCOUNT_STATUS_BY_TITLE:
	case UTILITY_API_GET_STATUS_INFO:
	case UTILITY_API_GET_STATUS_DETAIL:
		return UTILITY_PROTOCOL_HTTP;
	case UTILITY_API_NONE:
	case UTILITY_API_GET_ACCOUNT_SETTING:
	case UTILITY_API_SET_ACCOUNT_SETTING:
	case UTILITY_API_SET_MULTIPLE_STATUS:
	case UTILITY_API_DELETE_STATUS:
	case UTILITY_API_SET_COMMENT:
	case UTILITY_API_DELETE_COMMENT:
	case UTILITY_API_SET_VOTE:
	case UTILITY_API_DELETE_VOTE:
		return UTILITY_PROTOCOL_HTTPS;
	default:
		return UTILITY_PROTOCOL_HTTP;
	}
}

const char *ComponentUtility::GetUserAgent(int api)
{
	return "PSP2Activity-agent/1.0.0";
}

const char *ComponentUtility::GetScheme(int api)
{
	switch (GetProtocol(api)) {
	case UTILITY_PROTOCOL_HTTP:
		return "http://";
	case UTILITY_PROTOCOL_HTTPS:
		return "https://";
	default:
		return "http://";
	}
}

const char *ComponentUtility::GetHost(int api)
{
	const char *host;

	if (api == UTILITY_API_STATIC_RESOURCE) {
		sceClibSnprintf(m_host, sizeof(m_host), "%s%s%s", "static-resource.",
			GetNpEnvironment(), ".community.playstation.net");
		return m_host;
	}
	host = NULL;
	if (s_hostValid != 0)
		host = s_host;
	return host;
}

const char *ComponentUtility::GetPath(int api)
{
	switch (api) {
	case UTILITY_API_NONE:
		return NULL;
	case UTILITY_API_STATIC_RESOURCE:
		return "/np/resource/activity/activity.xml";
	case UTILITY_API_GET_ACCOUNT_SETTING:
		return "/activity/sec/get_account_setting";
	case UTILITY_API_SET_ACCOUNT_SETTING:
		return "/activity/sec/set_account_setting";
	case UTILITY_API_CHECK_STATUS:
		if (IsFunctionServer())
			return "/activity/func/check_status";
		return "/activity/sec/check_status";
	case UTILITY_API_GET_TIMELINE:
		if (IsFunctionServer())
			return "/activity/func/get_timeline_status_id_list";
		return "/activity/sec/get_timeline_status_id_list";
	case UTILITY_API_GET_TIMELINE_BY_TITLE:
		if (IsFunctionServer())
			return "/activity/func/get_timeline_status_id_list";
		return "/activity/sec/get_timeline_status_id_list";
	case UTILITY_API_GET_ACCOUNT_STATUS:
		if (IsFunctionServer())
			return "/activity/func/get_multiple_account_status_id_list";
		return "/activity/sec/get_multiple_account_status_id_list";
	case UTILITY_API_GET_ACCOUNT_STATUS_BY_TITLE:
		if (IsFunctionServer())
			return "/activity/func/get_multiple_account_status_id_list";
		return "/activity/sec/get_multiple_account_status_id_list";
	case UTILITY_API_GET_STATUS_INFO:
		if (IsFunctionServer())
			return "/activity/func/get_status_info_list";
		return "/activity/sec/get_status_info_list";
	case UTILITY_API_GET_STATUS_DETAIL:
		if (IsFunctionServer())
			return "/activity/func/get_status_info_list";
		return "/activity/sec/get_status_info_list";
	case UTILITY_API_SET_MULTIPLE_STATUS:
		return "/activity/sec/set_multiple_status";
	case UTILITY_API_DELETE_STATUS:
		return "/activity/sec/delete_status";
	case UTILITY_API_SET_COMMENT:
		return "/activity/sec/set_comment";
	case UTILITY_API_DELETE_COMMENT:
		return "/activity/sec/delete_comment";
	case UTILITY_API_SET_VOTE:
		return "/activity/sec/set_vote";
	case UTILITY_API_DELETE_VOTE:
		return "/activity/sec/delete_vote";
	default:
		return NULL;
	}
}

const char *ComponentUtility::GetNpEnvironment()
{
	if (sceNpCommonGetNpEnviroment(m_npEnvironment) < 0)
		return "np";
	return m_npEnvironment;
}

SceUShort16 ComponentUtility::GetPort(int protocol)
{
	switch (protocol) {
	case UTILITY_PROTOCOL_HTTP:
		return 80;
	case UTILITY_PROTOCOL_HTTPS:
		return 443;
	default:
		return 80;
	}
}

const char *ComponentUtility::GetXmlDeclaration()
{
	return "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
}

const char *ComponentUtility::GetPlatform()
{
	return "psp2";
}

const char *ComponentUtility::GetNpVersion()
{
	if (m_npVersionValid == 0) {
		SceNpCommon_0624AE44(m_npVersion);
		m_npVersionValid = 1;
	}
	return m_npVersion;
}

SceSize ComponentUtility::GetBase64EncodedSize(SceSize size)
{
	return (size + 2) / 3 * 4 + 1;
}

int ComponentUtility::Base64Encode(const void *src, SceSize size, char *dst, SceSize dstSize)
{
	SceSize encodedSize;
	int ret;

	encodedSize = GetBase64EncodedSize(size);
	if (encodedSize > dstSize)
		return 0;
	sceClibMemset(dst, 0, encodedSize);
	ret = sceNpCommonBase64Encode((const SceUInt8 *)src, dst, size);
	if (ret < 0 || (SceSize)ret > encodedSize)
		return 0;
	dst[ret] = '\0';
	return ret;
}

SceSize ComponentUtility::GetBase64DecodedSize(SceSize size)
{
	return size / 4 * 3;
}

int ComponentUtility::Base64Decode(const char *src, SceSize size, void *dst, SceSize dstSize)
{
	SceSize decodedSize;
	int ret;

	decodedSize = GetBase64DecodedSize(size);
	if (decodedSize > dstSize)
		return 0;
	sceClibMemset(dst, 0, decodedSize);
	ret = SceNpCommon_FB1F70F3(src, size, (SceUInt8 *)dst, dstSize);
	if (ret < 0)
		return 0;
	return ret;
}

int ComponentUtility::ConvertNpIdToJid(const SceNpId *npId, char *jid, SceSize size)
{
	NpJid npJid;
	SceSize required;
	char *buffer;
	int ret;

#line 558
	ACTIVITY_ASSERT(npId != NULL, "ConvertNpIdToJid");
	ACTIVITY_ASSERT(jid != NULL, "ConvertNpIdToJid");
	required = 0;
	if (SceNpCommon_40CC4D15(NULL, npId, NULL, 0, &required) < 0 || required == 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	buffer = (char *)activityMalloc(required);
	if (buffer == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (SceNpCommon_40CC4D15(&npJid, npId, buffer, required, NULL) < 0) {
		activityFree(buffer);
		return NP_ACTIVITY_ERROR_INTERNAL;
	}
	npJid.resource = NULL;
	if (SceNpCommon_85699AE2(&npJid, NULL, 0, &required) < 0) {
		activityFree(buffer);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	if (required > size) {
		activityFree(buffer);
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	ret = SceNpCommon_85699AE2(&npJid, jid, required, NULL);
	activityFree(buffer);
	if (ret < 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	return ret;
}

int ComponentUtility::ConvertJidToNpId(const char *jid, SceNpId *npId)
{
	NpJid npJid;
	SceSize required;
	char *buffer;
	int ret;

#line 619
	ACTIVITY_ASSERT(jid != NULL, "ConvertJidToNpId");
	ACTIVITY_ASSERT(npId != NULL, "ConvertJidToNpId");
	required = 0;
	if (SceNpCommon_D2B9BCA8(&npJid, jid, NULL, 0, &required) < 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (required == 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	buffer = (char *)activityMalloc(required);
	if (buffer == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	if (SceNpCommon_D2B9BCA8(&npJid, jid, buffer, required, NULL) < 0) {
		activityFree(buffer);
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	ret = SceNpCommon_9B7CAA67(npId, &npJid);
	activityFree(buffer);
	if (ret < 0)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	return ret;
}

int ComponentUtility::ConvertTimeTextToSceRtcTick(const char *text, SceRtcTick *tick)
{
	int ret;

#line 667
	ACTIVITY_ASSERT(text != NULL, "ConvertTimeTextToSceRtcTick");
	ACTIVITY_ASSERT(tick != NULL, "ConvertTimeTextToSceRtcTick");
	ret = sceRtcParseRFC3339(tick, text);
	if (ret < 0)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	return ret;
}

int ComponentUtility::ConvertSceRtcTickToTimeText(const SceRtcTick *tick, char *text,
	SceSize size)
{
	int ret;

	if (size < UTILITY_TIME_TEXT_SIZE)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
#line 697
	ACTIVITY_ASSERT(tick != NULL, "ConvertSceRtcTickToTimeText");
	ACTIVITY_ASSERT(text != NULL, "ConvertSceRtcTickToTimeText");
	ret = sceRtcFormatRFC3339(text, tick, 0);
	if (ret < 0)
		return NP_ACTIVITY_ERROR_INVALID_TIME;
	return ret;
}

int ComponentUtility::GetCurrentNetworkTime(SceRtcTick *tick)
{
#line 804
	ACTIVITY_ASSERT(tick != NULL, "GetCurrentNetworkTime");
	return sceRtcGetCurrentNetworkTick(tick);
}
