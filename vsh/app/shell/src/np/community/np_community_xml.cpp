/*
 * np_community_xml.cpp: NP Community XML DOM helpers, HTTP response/signature
 * readers, JID/base64/numeric formatters, and HMAC-SHA1 verification
 * (0x813F5194..0x813F85B3).
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include <libsha1.h>
#include <rtc.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include "np_community_backend.h"

#define NP_BXCE_PARSER_ERR_MASK		-2138758912	/* 0x80852100 */
#define NP_COMMUNITY_SIG_COMMENT_LEN	65U
#define NP_COMMUNITY_PASS_KEY_LEN	128U

/* XOR-0x7F obfuscated 20-byte HMAC-SHA1 keys for signed community XML files (0x8152DB5C..0x8152DBE8) */
static const SceUInt8 s_commentHmacXorKeys[7][SCE_SHA1_DIGEST_SIZE] = {
	/* mode 3 (ranking-list XML) */
	{ 0x68, 0x5E, 0x25, 0x34, 0x69, 0x5D, 0x79, 0x09, 0x6F, 0x57,
	  0x64, 0x00, 0x44, 0x31, 0x5B, 0x51, 0x5E, 0x0C, 0x7E, 0x55 },
	/* mode 4 (storage-list XML) */
	{ 0x26, 0x41, 0x84, 0x9A, 0xE5, 0x41, 0xF1, 0xFD, 0xEB, 0x64,
	  0x5C, 0x7B, 0x97, 0xB8, 0x89, 0x88, 0x59, 0xE5, 0x4C, 0x70 },
	/* mode 5 (searchjid XML) */
	{ 0x59, 0xCB, 0x60, 0x67, 0x57, 0x4D, 0x87, 0x3C, 0xEF, 0xD7,
	  0xB7, 0x5A, 0x30, 0x05, 0x29, 0xC1, 0x8B, 0xFB, 0x75, 0xED },
	/* mode 6 (trophy-status XML) */
	{ 0x47, 0x75, 0xE3, 0x03, 0xBD, 0xF2, 0xAA, 0xDC, 0x8C, 0x58,
	  0xFE, 0xCD, 0x17, 0xF4, 0xCB, 0xAD, 0xB4, 0x3C, 0x5B, 0x83 },
	/* mode 7 (ttl XML) */
	{ 0xE8, 0xAB, 0xDA, 0xE8, 0xF8, 0x1C, 0x08, 0x61, 0x23, 0xFE,
	  0x5D, 0x62, 0x81, 0xBB, 0x0F, 0x0D, 0xEE, 0x90, 0x9A, 0xF5 },
	/* mode 8 (friendlist-status XML) */
	{ 0xF8, 0xA0, 0x5D, 0x44, 0xA0, 0x4E, 0x5C, 0x03, 0x68, 0x12,
	  0x26, 0x1D, 0xB7, 0x0E, 0xE1, 0x70, 0x8E, 0xBC, 0x4D, 0x48 },
	/* mode 9 */
	{ 0xB6, 0x62, 0x7A, 0xE4, 0x42, 0x2F, 0x9D, 0x81, 0xE1, 0x42,
	  0x1F, 0xA2, 0xB1, 0xDB, 0x02, 0x84, 0x1E, 0x64, 0xB9, 0x33 }
};

static void npXmlFreeNodeList(BxceNodeList *list)
{
	if (list != NULL) {
		ScebXCe_1F59F0C4(list);
		ScebXCe_EC560647(list);
	}
}

/* 0x813F5194 */
void npCommunityHmacSha1(const void *key, SceSize keyLen, const void *data, SceSize dataLen,
			 SceUInt8 *outDigest)
{
	SceUInt8 keyBlock[SCE_SHA1_BLOCK_SIZE];
	SceUInt8 innerDigest[SCE_SHA1_DIGEST_SIZE];
	SceSha1Context shaCtx;

	sceClibMemset(keyBlock, 0, sizeof(keyBlock));
	if (keyLen > SCE_SHA1_BLOCK_SIZE)
		sceSha1Digest(key, keyLen, keyBlock);
	else
		sceClibMemcpy_safe(keyBlock, key, keyLen);

	for (SceSize idx = 0U; idx < SCE_SHA1_BLOCK_SIZE; ++idx)
		keyBlock[idx] ^= 0x36U;

	sceSha1BlockInit(&shaCtx);
	sceSha1BlockUpdate(&shaCtx, keyBlock, SCE_SHA1_BLOCK_SIZE);
	sceSha1BlockUpdate(&shaCtx, data, dataLen);
	sceSha1BlockResult(&shaCtx, innerDigest);

	for (SceSize idx = 0U; idx < SCE_SHA1_BLOCK_SIZE; ++idx)
		keyBlock[idx] ^= 0x6AU;

	sceSha1BlockInit(&shaCtx);
	sceSha1BlockUpdate(&shaCtx, keyBlock, SCE_SHA1_BLOCK_SIZE);
	sceSha1BlockUpdate(&shaCtx, innerDigest, SCE_SHA1_DIGEST_SIZE);
	sceSha1BlockResult(&shaCtx, outDigest);
}

/* 0x813F5284 */
int npCommunityBase64EncodeAlloc(char **outBase64, const void *src, SceSize srcLen)
{
	SceSize outCap = 4U * ((srcLen + 2U) / 3U) + 1U;
	char *encoded = static_cast<char *>(npCommunityAlloc(outCap));
	if (encoded == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(encoded, 0, outCap);
	int ret = static_cast<int>(
		sceNpCommonBase64Encode(static_cast<const SceUInt8 *>(src), encoded, srcLen));
	if (ret >= 0) {
		*outBase64 = encoded;
	} else {
		npCommunityFree(encoded);
	}
	return ret;
}

/* 0x813F52E2 */
int npCommunityBase64DecodeAlloc(void **outBytes, const char *src, SceSize srcLen)
{
	SceSize outCap = (srcLen & ~3U) - (srcLen >> 2) + 1U;
	SceUInt8 *decoded = static_cast<SceUInt8 *>(npCommunityAlloc(outCap));
	if (decoded == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(decoded, 0, outCap);
	int ret = SceNpCommon_FB1F70F3(src, srcLen, decoded, outCap);
	if (ret >= 0) {
		*outBytes = decoded;
		return ret;
	}

	npCommunityFree(decoded);
	return ret;
}

/* 0x813F533C */
void npCommunityXmlAddPlatformAttrs(BxceAttributeList *attrs)
{
	char fwVersion[8];
	SceNpCommon_0624AE44(fwVersion);
	ScebXCe_3DD761CF(attrs, "platform", "psp2");
	ScebXCe_3DD761CF(attrs, "sv", fwVersion);
}

/* 0x813F538E */
int npCommunityXmlAddIntAttr(BxceAttributeList *attrs, const char *name, SceInt32 value)
{
	char text[16];
	sceClibMemset(text, 0, 13U);
	npCommunityFormatInt32(text, value);
	return ScebXCe_3DD761CF(attrs, name, text);
}

/* 0x813F53DC */
int npCommunityXmlAddInt64Attr(BxceAttributeList *attrs, const char *name, SceInt64 value)
{
	char text[24];
	sceClibMemset(text, 0, 23U);
	npCommunityFormatInt64(text, value);
	return ScebXCe_3DD761CF(attrs, name, text);
}

/* 0x813F542E */
int npCommunityXmlAddUint64Attr(BxceAttributeList *attrs, const char *name, SceUInt64 value)
{
	char text[24];
	sceClibMemset(text, 0, 21U);
	npCommunityFormatUint64(text, value);
	return ScebXCe_3DD761CF(attrs, name, text);
}

/* 0x813F5480 */
int npCommunityXmlAppendCommId(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       const SceNpCommunicationId *commId)
{
	char text[16];
	sceClibMemset(text, 0, 13U);
	int ret = SceNpCommon_E426FEE8(commId, text, 13U);
	if (ret < 0)
		return ret;

	SceSize len = sceClibStrnlen(text, 0x7FFFFFFFU);
	if (ScebXCe_AC0E4F67(doc, parent, tagName, NULL, text, len) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F54F6 */
int npCommunityXmlAppendCommIdVersion(BxceDocument *doc, BxceNodeId parent,
				      BxceAttributeList *attrs, const char *tagName,
				      const SceNpCommunicationId *commId, SceInt32 version)
{
	char verText[12];
	char commText[16];
	sceClibMemset(verText, 0, sizeof(verText));
	npCommunityFormatInt32(verText, version);
	ScebXCe_3DD761CF(attrs, "version", verText);

	sceClibMemset(commText, 0, 13U);
	int ret = SceNpCommon_E426FEE8(commId, commText, 13U);
	if (ret < 0)
		return ret;

	SceSize len = sceClibStrnlen(commText, 0x7FFFFFFFU);
	if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, commText, len) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F55A4 */
int npCommunityXmlAppendInt(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			    const BxceAttributeList *attrs, SceInt32 value)
{
	char text[12];
	sceClibMemset(text, 0, sizeof(text));
	npCommunityFormatInt32(text, value);
	SceSize len = sceClibStrnlen(text, 0x7FFFFFFFU);
	if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, text, len) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F561A */
int npCommunityXmlAppendBase64(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       const void *src, SceSize srcLen)
{
	char *encoded = NULL;
	int ret = npCommunityBase64EncodeAlloc(&encoded, src, srcLen);
	if (ret >= 0) {
		SceSize len = sceClibStrnlen(encoded, 0x7FFFFFFFU);
		if (ScebXCe_AC0E4F67(doc, parent, tagName, NULL, encoded, len) != 0U)
			ret = 0;
		else
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
	}
	if (encoded != NULL)
		npCommunityFree(encoded);
	return ret;
}

/* 0x813F56C6 */
int npCommunityXmlAppendAvatarElement(BxceDocument *doc, BxceNodeId parent,
				      BxceAttributeList *attrs, const char *tagName,
				      const void *ticketBuf, SceSize ticketSize,
				      const char *ticketTagName, const SceInt32 *categoryIdPtr,
				      const SceInt32 *avatarIdPtr, const SceInt32 *sizeTypePtr)
{
	char numText[12];

	if (avatarIdPtr != NULL) {
		sceClibMemset(numText, 0, sizeof(numText));
		npCommunityFormatInt32(numText, *avatarIdPtr);
		int ret = ScebXCe_3DD761CF(attrs, "id", numText);
		if (ret != 0)
			return ret;
	}
	if (categoryIdPtr != NULL) {
		sceClibMemset(numText, 0, sizeof(numText));
		npCommunityFormatInt32(numText, *categoryIdPtr);
		int ret = ScebXCe_3DD761CF(attrs, "category", numText);
		if (ret != 0)
			return ret;
	}
	if (sizeTypePtr != NULL) {
		const char *sizeStr = NULL;
		switch (*sizeTypePtr) {
		case 0:
			sizeStr = "l";
			break;
		case 1:
			sizeStr = "m";
			break;
		case 2:
			sizeStr = "s";
			break;
		case 3:
			sizeStr = "n";
			break;
		default:
			return NP_COMMUNITY_ERR_INVALID_ARG;
		}
		int ret = ScebXCe_3DD761CF(attrs, "size", sizeStr);
		if (ret != 0)
			return ret;
	}

	BxceNodeId elem = ScebXCe_AC0E4F67(doc, parent, tagName, attrs, NULL, 0U);
	if (elem == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	if (ticketBuf != NULL) {
		int ret = npCommunityXmlAppendBase64(doc, elem, ticketTagName, ticketBuf,
						     ticketSize);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x813F5814 */
int npCommunityXmlAppendPanelElement(BxceDocument *doc, BxceNodeId parent,
				     BxceAttributeList *attrs, const char *tagName,
				     const SceInt32 *categoryIdPtr, const SceInt32 *panelIdPtr,
				     const SceInt32 *sizeTypePtr)
{
	char numText[12];

	if (panelIdPtr != NULL) {
		sceClibMemset(numText, 0, sizeof(numText));
		npCommunityFormatInt32(numText, *panelIdPtr);
		int ret = ScebXCe_3DD761CF(attrs, "id", numText);
		if (ret != 0)
			return ret;
	}
	if (categoryIdPtr != NULL) {
		sceClibMemset(numText, 0, sizeof(numText));
		npCommunityFormatInt32(numText, *categoryIdPtr);
		int ret = ScebXCe_3DD761CF(attrs, "category", numText);
		if (ret != 0)
			return ret;
	}
	if (sizeTypePtr != NULL) {
		if (*sizeTypePtr != 0)
			return NP_COMMUNITY_ERR_INVALID_ARG;
		int ret = ScebXCe_3DD761CF(attrs, "size", "s");
		if (ret != 0)
			return ret;
	}

	if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, NULL, 0U) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F58F4 */
int npCommunityXmlAppendJid(BxceDocument *doc, BxceNodeId parent, const BxceAttributeList *attrs,
			    const char *tagName, const SceNpId *npId)
{
	char *jidStr = NULL;
	int ret = npCommunityFormatNpIdToJid(&jidStr, npId);
	if (ret >= 0) {
		SceSize len = sceClibStrnlen(jidStr, 0x7FFFFFFFU);
		if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, jidStr, len) != 0U)
			ret = 0;
		else
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
	}
	if (jidStr != NULL)
		npCommunityFree(jidStr);
	return ret;
}

/* 0x813F597C */
int npCommunityXmlAppendOpenPsId(BxceDocument *doc, BxceNodeId parent,
				 const BxceAttributeList *attrs, const char *tagName)
{
	char openPsIdHex[36];
	npCommunityFormatOpenPsId(openPsIdHex);
	SceSize len = sceClibStrnlen(openPsIdHex, 0x7FFFFFFFU);
	if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, openPsIdHex, len) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F59E0 */
int npCommunityXmlAppendUint32(BxceDocument *doc, BxceNodeId parent, const char *tagName,
			       SceUInt32 value)
{
	char text[24];
	sceClibMemset(text, 0, 21U);
	npCommunityFormatUint64(text, static_cast<SceUInt64>(value));
	SceSize len = sceClibStrnlen(text, 0x7FFFFFFFU);
	if (ScebXCe_AC0E4F67(doc, parent, tagName, NULL, text, len) == 0U)
		return NP_COMMUNITY_ERR_NO_MEMORY;
	return 0;
}

/* 0x813F5A54 */
int npCommunityXmlAppendEscapedText(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				    const char *text)
{
	char *escaped = NULL;
	int ret = npCommunityEscapeXmlAlloc(&escaped, text);
	if (ret >= 0) {
		if (escaped != NULL) {
			SceSize len = sceClibStrnlen(escaped, 0x7FFFFFFFU);
			if (ScebXCe_AC0E4F67(doc, parent, tagName, NULL, escaped, len) != 0U)
				ret = 0;
			else
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
		} else {
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}
	if (escaped != NULL)
		npCommunityFree(escaped);
	return ret;
}

/* 0x813F5AE6 */
int npCommunityXmlAppendLanguages(BxceDocument *doc, BxceNodeId parent, const SceInt32 *languages)
{
	SceInt32 localLangs[4];
	localLangs[0] = languages[0];
	localLangs[1] = languages[1];
	localLangs[2] = languages[2];
	localLangs[3] = languages[3];
	npCommunityCompactLanguages(localLangs);

	if (localLangs[0] >= 0) {
		int ret = npCommunityXmlAppendInt(doc, parent, "language1", NULL, localLangs[0]);
		if (ret < 0)
			return ret;
	}

	if (localLangs[1] < 0) {
		if (ScebXCe_AC0E4F67(doc, parent, "language2", NULL, NULL, 0U) == 0U)
			return NP_COMMUNITY_ERR_NO_MEMORY;
	} else {
		int ret = npCommunityXmlAppendInt(doc, parent, "language2", NULL, localLangs[1]);
		if (ret < 0)
			return ret;
	}

	if (localLangs[2] < 0) {
		if (ScebXCe_AC0E4F67(doc, parent, "language3", NULL, NULL, 0U) == 0U)
			return NP_COMMUNITY_ERR_NO_MEMORY;
	} else {
		int ret = npCommunityXmlAppendInt(doc, parent, "language3", NULL, localLangs[2]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* 0x813F5C72 */
int npCommunityXmlAppendBoundedText(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				    const BxceAttributeList *attrs, const char *text,
				    SceSize maxLen)
{
	char *escaped = NULL;
	int ret;

	if (sceClibStrnlen(text, 0x7FFFFFFFU) > maxLen) {
		ret = NP_COMMUNITY_ERR_INVALID_ARG;
	} else {
		ret = npCommunityEscapeXmlAlloc(&escaped, text);
		if (ret >= 0) {
			if (escaped != NULL) {
				SceSize len = sceClibStrnlen(escaped, 0x7FFFFFFFU);
				if (ScebXCe_AC0E4F67(doc, parent, tagName, attrs, escaped, len) !=
				    0U) {
					ret = 0;
				} else {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				}
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	if (escaped != NULL)
		npCommunityFree(escaped);
	return ret;
}

/* 0x813F5D30 */
int npCommunityXmlAppendRtcTick(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				const BxceAttributeList *attrs, const SceRtcTick *utcTick,
				int stripFracSeconds)
{
	char dateText[32];
	int ret = sceRtcFormatRFC3339(dateText, utcTick, 0);
	if (ret < 0)
		return ret;

	if (stripFracSeconds != 0) {
		SceSize dateLen = sceClibStrnlen(dateText, 0x7FFFFFFFU);
		SceSize suffixLen = sceClibStrnlen(".00Z", 0x7FFFFFFFU);
		if (dateLen >= suffixLen &&
		    sceClibMemcmp(dateText + dateLen - suffixLen, ".00Z", suffixLen) == 0) {
			SceSize zLen = sceClibStrnlen("Z", 0x7FFFFFFFU);
			sceClibMemcpy_safe(dateText + dateLen - suffixLen, "Z", zLen + 1U);
		}
	}

	SceSize finalLen = sceClibStrnlen(dateText, 0x7FFFFFFFU);
	ret = npCommunityXmlAppendBoundedText(doc, parent, tagName, attrs, dateText, finalLen);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813F5E7E */
int npCommunityXmlAppendBase64Checked(BxceDocument *doc, BxceNodeId parent, const char *tagName,
				      const void *src, SceSize srcLen)
{
	char *encoded = NULL;
	int ret = npCommunityBase64EncodeAlloc(&encoded, src, srcLen);
	if (ret >= 0) {
		if (encoded != NULL) {
			SceSize len = sceClibStrnlen(encoded, 0x7FFFFFFFU);
			if (ScebXCe_AC0E4F67(doc, parent, tagName, NULL, encoded, len) != 0U)
				ret = 0;
			else
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
		} else {
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}
	if (encoded != NULL)
		npCommunityFree(encoded);
	return ret;
}

/* 0x813F5F3C */
int npCommunityXmlFindAttr(const char **outValue, const BxceDocument *doc, BxceNodeId node,
			   const char *attrName)
{
	const char *found = NULL;
	BxceAttrNodeId attr = ScebXCe_B034C595(doc, node);
	SceSize nameLen = sceClibStrnlen(attrName, 0x7FFFFFFFU);

	while (attr != 0U) {
		const char *curName = ScebXCe_B4989A58(doc, attr);
		const char *curVal = ScebXCe_84383D8E(doc, attr);
		if (curName != NULL && sceClibStrncasecmp(curName, attrName, nameLen + 1U) == 0) {
			found = curVal;
			break;
		}
		attr = ScebXCe_134F491C(doc, attr);
	}

	*outValue = found;
	return 0;
}

/* 0x813F5FAC */
int npCommunityXmlCheckResult(const BxceDocument *doc, const char *rootTag)
{
	BxceNodeId root = ScebXCe_CED6BECF(doc);
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, root, rootTag);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	const char *resultStr = NULL;
	npCommunityXmlFindAttr(&resultStr, doc, elem, "result");

	if (resultStr != NULL && sceClibStrnlen(resultStr, 0x7FFFFFFFU) == 2U) {
		unsigned int code =
			static_cast<unsigned int>(SceNpCommon_4C4AA451(resultStr, NULL, 16));
		if (code < 0x100U) {
			if (code == 0U)
				ret = 0;
			else
				ret = static_cast<int>(code) + NP_COMMUNITY_ERR_SERVER_BASE;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F6086 */
int npCommunityHttpReadHeaders(NpCommunityBackendTx *tx, SceInt32 *outStatusCode,
			       SceUInt64 *outContentLength, SceUInt32 *outInfoLength,
			       SceUInt8 *outSignature, int sigMode)
{
	char *allHeaders = NULL;
	SceSize allHeadersLen = 0U;
	const char *fieldValue = NULL;
	SceSize fieldLen = 0U;

	if (sigMode != NP_COMMUNITY_SIG_NONE && outSignature == NULL)
		return NP_COMMUNITY_ERR_NULL_POINTER;

	int ret = sceHttpGetStatusCode(tx->httpReqId, outStatusCode);
	if (ret < 0)
		return ret;

	SceInt32 statusCode = *outStatusCode;
	if (statusCode != 200 && tx->serviceType != 27) {
		if (sigMode == NP_COMMUNITY_SIG_COMMENT_RANKING_LIST ||
		    sigMode == NP_COMMUNITY_SIG_COMMENT_STORAGE_LIST ||
		    sigMode == NP_COMMUNITY_SIG_COMMENT_TROPHY_STATUS) {
			return NP_COMMUNITY_ERR_SERVER_HTTP_STATUS;
		}
		return NP_COMMUNITY_ERR_HTTP_STATUS;
	}

	if (statusCode == 304) {
		*outContentLength = 0ULL;
	} else {
		ret = sceHttpGetResponseContentLength(tx->httpReqId, outContentLength);
		if (ret < 0)
			return ret;
	}

	ret = sceHttpGetAllResponseHeaders(tx->httpReqId, &allHeaders, &allHeadersLen);
	if (ret < 0)
		return ret;

	if (sceHttpParseResponseHeader(allHeaders, allHeadersLen, "Sce-Np-Info-Length", &fieldValue,
				       &fieldLen) >= 0 &&
	    outInfoLength != NULL) {
		*outInfoLength = SceNpCommon_5B770B80(fieldValue, NULL, 10);
	}

	if (sigMode == NP_COMMUNITY_SIG_RANKING_HEADER &&
	    sceHttpParseResponseHeader(allHeaders, allHeadersLen, "Sce-Np-Signature", &fieldValue,
				       &fieldLen) >= 0 &&
	    outSignature != NULL) {
		ret = npCommunityParseHexSignature(outSignature, fieldValue, fieldLen);
		if (ret < 0)
			return ret;
	}

	if (sigMode >= NP_COMMUNITY_SIG_COMMENT_RANKING_LIST &&
	    sigMode <= NP_COMMUNITY_SIG_COMMENT_SKIP_HMAC) {
		ret = npCommunityHttpReadCommentSig(tx, outSignature, outContentLength);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* 0x813F61D4 */
int npCommunityHttpRecvXml(NpCommunityBackendTx *tx, BxceDocument **outDoc, int sigMode,
			   SceUInt64 *outRemainingBytes)
{
	return npCommunityHttpRecvXmlEx(tx, outDoc, sigMode, outRemainingBytes, NULL);
}

/* 0x813F61E4 */
int npCommunityHttpExpectBytes(NpCommunityBackendTx *tx, const void *expected, SceSize size)
{
	void *buf = npCommunityAlloc(size);
	if (buf == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	int ret;
	if (tx->aborted == 1) {
		ret = NP_COMMUNITY_ERR_ABORTED;
	} else {
		SceSize actualRead = 0U;
		ret = npCommunityHttpReadExact(tx, buf, size, &actualRead);
		if (ret >= 0) {
			if (actualRead == size && sceClibMemcmp(buf, expected, size) == 0)
				ret = 0;
			else
				ret = NP_COMMUNITY_ERR_PARSE_FAIL;
		}
	}

	npCommunityFree(buf);
	return ret;
}

/* 0x813F627E */
int npCommunityXmlParseNumberAttr(const BxceDocument *doc, BxceNodeId node, SceInt32 *outCount)
{
	const char *numStr = NULL;
	npCommunityXmlFindAttr(&numStr, doc, node, "number");
	if (numStr == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	SceInt32 parsed = SceNpCommon_CC8E6AC6(numStr);
	if (parsed < 0)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	*outCount = parsed;
	return 0;
}

/* 0x813F6302 */
int npCommunityXmlParseIntAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
			       SceInt32 *outValue)
{
	const char *valStr = NULL;
	npCommunityXmlFindAttr(&valStr, doc, node, attrName);
	if (valStr == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	*outValue = SceNpCommon_CC8E6AC6(valStr);
	return 0;
}

/* 0x813F6376 */
int npCommunityXmlParseInt64Attr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
				 SceInt64 *outValue)
{
	const char *valStr = NULL;
	npCommunityXmlFindAttr(&valStr, doc, node, attrName);
	if (valStr == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	*outValue = SceNpCommon_E1FF4E70(valStr);
	return 0;
}

/* 0x813F63EA */
int npCommunityXmlParseHexAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
			       SceUInt32 *outValue)
{
	const char *valStr = NULL;
	npCommunityXmlFindAttr(&valStr, doc, node, attrName);
	if (valStr == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	*outValue = SceNpCommon_5B770B80(valStr, NULL, 16);
	return 0;
}

/* 0x813F6464 */
int npCommunityXmlParseChildUint(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				 SceUInt32 *outValue)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL) {
			*outValue = SceNpCommon_5B770B80(text, NULL, 10);
			ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F64CE */
int npCommunityXmlParseDataFlag(const BxceDocument *doc, BxceNodeId parent, SceInt32 *outHasData)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "data");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL) {
			*outHasData = (SceNpCommon_CC8E6AC6(text) == 1) ? 1 : 0;
			ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F654A */
int npCommunityXmlParseChildDate(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				 SceRtcTick *outTick)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL && sceRtcParseDateTime(outTick, text) >= 0)
			ret = 0;
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F65C0 */
int npCommunityXmlParseText(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
			    char *outBuf, SceSize maxLen)
{
	BxceNodeList *list = NULL;
	char *unescaped = NULL;
	int ret = 0;
	BxceNodeId elem = parent;

	if (tagName != NULL) {
		list = ScebXCe_1FA24DC0(doc, parent, tagName);
		if (list == NULL)
			return NP_COMMUNITY_ERR_PARSE_FAIL;
		elem = ScebXCe_6C46D786(list, 0U);
	}

	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *rawText = ScebXCe_33825EFF(doc, textNode);
		if (rawText != NULL) {
			ret = npCommunityUnescapeXmlAlloc(&unescaped, rawText);
			if (ret >= 0) {
				if (unescaped != NULL &&
				    sceClibStrnlen(unescaped, 0x7FFFFFFFU) <= maxLen) {
					sceClibStrncpy(outBuf, unescaped, maxLen);
					ret = 0;
				} else {
					ret = NP_COMMUNITY_ERR_PARSE_FAIL;
				}
			}
		} else {
			outBuf[0] = '\0';
		}
	} else {
		outBuf[0] = '\0';
	}

	npXmlFreeNodeList(list);
	if (unescaped != NULL)
		npCommunityFree(unescaped);
	return ret;
}

/* 0x813F669A */
int npCommunityXmlParseTusDataInfo(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				   NpTusDataInfo *outInfo)
{
	void *decoded = NULL;
	int ret = 0;

	sceClibMemset(outInfo, 0, sizeof(NpTusDataInfo));
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list != NULL) {
		BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
		BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
		if (textNode == 0U) {
			sceClibMemset(outInfo, 0, sizeof(NpTusDataInfo));
		} else {
			const char *b64Text = ScebXCe_33825EFF(doc, textNode);
			if (b64Text == NULL) {
				outInfo->infoSize = 0U;
			} else {
				SceSize b64Len = sceClibStrnlen(b64Text, 0x7FFFFFFFU);
				ret = npCommunityBase64DecodeAlloc(&decoded, b64Text, b64Len);
				if (ret >= 0) {
					if (decoded == NULL) {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					} else if (ret == 0) {
						outInfo->infoSize = 0U;
					} else {
						SceSize copySize = static_cast<SceSize>(ret);
						if (copySize > sizeof(outInfo->bytes))
							copySize = sizeof(outInfo->bytes);
						outInfo->infoSize = copySize;
						sceClibMemcpy_safe(outInfo->bytes, decoded,
								   copySize);
						ret = 0;
					}
				}
			}
		}
		npXmlFreeNodeList(list);
	}

	if (decoded != NULL)
		npCommunityFree(decoded);
	return ret;
}

/* 0x813F6788 */
int npCommunityXmlParseScoreGameInfo(const BxceDocument *doc, BxceNodeId parent,
				     NpScoreGameInfo *outInfo)
{
	void *decoded = NULL;
	int ret = 0;

	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "info");
	BxceNodeId textNode = 0U;
	const char *b64Text = NULL;
	if (list != NULL) {
		BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
		textNode = ScebXCe_FEA6E4A1(doc, elem);
		if (textNode != 0U)
			b64Text = ScebXCe_33825EFF(doc, textNode);
	}

	if (b64Text == NULL) {
		sceClibMemset(outInfo->bytes, 0, sizeof(outInfo->bytes));
		outInfo->infoSize = 0U;
	} else {
		SceSize b64Len = sceClibStrnlen(b64Text, 0x7FFFFFFFU);
		ret = npCommunityBase64DecodeAlloc(&decoded, b64Text, b64Len);
		if (ret >= 0) {
			if (decoded == NULL) {
				ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			} else if (ret > 0) {
				SceSize copySize = static_cast<SceSize>(ret);
				if (copySize > sizeof(outInfo->bytes))
					copySize = sizeof(outInfo->bytes);
				sceClibMemcpy_safe(outInfo->bytes, decoded, copySize);
				outInfo->infoSize = copySize;
				ret = 0;
			}
		}
	}

	npXmlFreeNodeList(list);
	if (decoded != NULL)
		npCommunityFree(decoded);
	return ret;
}

/* 0x813F6876 */
int npCommunityXmlCheckServerStatus(const BxceDocument *doc, BxceNodeId node, int serviceCategory)
{
	const char *statusStr = NULL;
	npCommunityXmlFindAttr(&statusStr, doc, node, "status");
	if (statusStr == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	SceSize aliveLen = sceClibStrnlen("alive", 0x7FFFFFFFU);
	if (sceClibStrncasecmp(statusStr, "alive", aliveLen + 1U) == 0)
		return 0;

	SceSize maintLen = sceClibStrnlen("maintenance", 0x7FFFFFFFU);
	if (sceClibStrncasecmp(statusStr, "maintenance", maintLen + 1U) == 0) {
		if (serviceCategory == 1)
			return NP_COMMUNITY_ERR_SCORE_MAINTENANCE;
		if (serviceCategory == 2)
			return NP_COMMUNITY_ERR_TUS_MAINTENANCE;
		if (serviceCategory == 4)
			return NP_COMMUNITY_ERR_OTHER_MAINTENANCE;
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	}

	SceSize endLen = sceClibStrnlen("end", 0x7FFFFFFFU);
	if (sceClibStrncasecmp(statusStr, "end", endLen + 1U) == 0) {
		if (serviceCategory == 1)
			return NP_COMMUNITY_ERR_SCORE_END_SERVICE;
		if (serviceCategory == 2)
			return NP_COMMUNITY_ERR_TUS_END_SERVICE;
		if (serviceCategory == 4)
			return NP_COMMUNITY_ERR_OTHER_END_SERVICE;
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	}

	SceSize beforeLen = sceClibStrnlen("before", 0x7FFFFFFFU);
	if (sceClibStrncasecmp(statusStr, "before", beforeLen + 1U) == 0) {
		if (serviceCategory == 1)
			return NP_COMMUNITY_ERR_SCORE_BEFORE_SERVICE;
		if (serviceCategory == 2)
			return NP_COMMUNITY_ERR_TUS_BEFORE_SERVICE;
		if (serviceCategory == 4)
			return NP_COMMUNITY_ERR_OTHER_BEFORE_SERVICE;
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	}

	return NP_COMMUNITY_ERR_PARSE_FAIL;
}

/* 0x813F6A22 */
int npCommunityXmlParseJidChild(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				SceNpId *outNpId)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *jidText = ScebXCe_33825EFF(doc, textNode);
		if (jidText != NULL) {
			ret = npCommunityParseJidToNpId(outNpId, jidText);
			if (ret >= 0)
				ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F6A98 */
int npCommunityXmlParseJidAndPcId(const BxceDocument *doc, BxceNodeId parent, SceNpId *outNpId,
				  SceInt32 *outPcId)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "jid");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *jidText = ScebXCe_33825EFF(doc, textNode);
		if (jidText != NULL) {
			ret = npCommunityParseJidToNpId(outNpId, jidText);
			if (ret >= 0) {
				const char *pcStr = NULL;
				npCommunityXmlFindAttr(&pcStr, doc, elem, "pc");
				if (pcStr != NULL)
					*outPcId = SceNpCommon_4C4AA451(pcStr, NULL, 16);
				else
					*outPcId = 0;
				ret = 0;
			}
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F6BAA */
int npCommunityXmlParseOnlineName(const BxceDocument *doc, BxceNodeId parent, char *outOnlineName,
				  SceInt32 *outUpdFlag, SceRtcTick *outUpdDate)
{
	char *unescaped = NULL;
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "onlinename");
	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;

	if (list != NULL) {
		BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
		BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
		if (textNode != 0U) {
			const char *rawText = ScebXCe_33825EFF(doc, textNode);
			if (rawText != NULL) {
				ret = npCommunityUnescapeXmlAlloc(&unescaped, rawText);
				if (ret >= 0) {
					if (unescaped != NULL &&
					    sceClibStrnlen(unescaped, 0x7FFFFFFFU) <= 48U) {
						sceClibStrncpy(outOnlineName, unescaped, 48U);
						if (outUpdFlag != NULL) {
							const char *updStr = NULL;
							npCommunityXmlFindAttr(&updStr, doc, elem,
									       "upd");
							if (updStr != NULL)
								*outUpdFlag =
									SceNpCommon_CC8E6AC6(
										updStr);
							else
								*outUpdFlag = 0;
						}
						if (outUpdDate != NULL) {
							const char *dateStr = NULL;
							npCommunityXmlFindAttr(&dateStr, doc, elem,
									       "date");
							if (dateStr == NULL ||
							    sceRtcParseDateTime(outUpdDate,
										dateStr) < 0) {
								outUpdDate->tick = 0ULL;
							}
						}
						ret = 0;
					} else {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					}
				}
			}
		}
	}

	if (unescaped != NULL)
		npCommunityFree(unescaped);
	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F6D88 */
int npCommunityXmlParseCountry(const BxceDocument *doc, BxceNodeId parent,
			       SceNpCountryCode *outCountry)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "country");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL) {
			sceClibMemcpy_safe(outCountry->data, text, 2U);
			ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F6DFC */
int npCommunityXmlParseLanguages(const BxceDocument *doc, BxceNodeId parent, SceInt32 *outLanguages)
{
	sceClibMemset(outLanguages, 0, sizeof(SceInt32) * 4U);

	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "language1");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode == 0U) {
		npXmlFreeNodeList(list);
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	}

	SceInt32 parsed = SceNpCommon_CC8E6AC6(ScebXCe_33825EFF(doc, textNode));
	if (parsed > 22)
		parsed = 1;
	outLanguages[0] = parsed;
	npXmlFreeNodeList(list);

	list = ScebXCe_1FA24DC0(doc, parent, "language2");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	elem = ScebXCe_6C46D786(list, 0U);
	textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		parsed = SceNpCommon_CC8E6AC6(ScebXCe_33825EFF(doc, textNode));
		if (parsed > 22)
			parsed = 1;
		outLanguages[1] = parsed;
	} else {
		outLanguages[1] = -1;
	}
	npXmlFreeNodeList(list);

	list = ScebXCe_1FA24DC0(doc, parent, "language3");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	elem = ScebXCe_6C46D786(list, 0U);
	textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		parsed = SceNpCommon_CC8E6AC6(ScebXCe_33825EFF(doc, textNode));
		if (parsed > 22)
			parsed = 1;
		outLanguages[2] = parsed;
	} else {
		outLanguages[2] = -1;
	}
	npXmlFreeNodeList(list);

	npCommunityCompactLanguages(outLanguages);
	return 0;
}

/* 0x813F6F2C */
int npCommunityXmlParseAboutMe(const BxceDocument *doc, BxceNodeId parent, char *outAboutMe)
{
	char *unescaped = NULL;
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "aboutme");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = 0;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	const char *rawText = (textNode != 0U) ? ScebXCe_33825EFF(doc, textNode) : NULL;

	if (rawText != NULL) {
		ret = npCommunityUnescapeXmlAlloc(&unescaped, rawText);
		if (ret >= 0) {
			if (sceClibStrnlen(unescaped, 0x7FFFFFFFU) > 63U) {
				ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			} else {
				sceClibStrncpy(outAboutMe, unescaped, 64U);
				ret = 0;
			}
		}
	} else {
		sceClibMemset(outAboutMe, 0, 64U);
	}

	npXmlFreeNodeList(list);
	if (unescaped != NULL)
		npCommunityFree(unescaped);
	return ret;
}

/* 0x813F6FF4 */
int npCommunityXmlParseAvatarUrl(const BxceDocument *doc, BxceNodeId parent, char *outAvatarUrl,
				 SceInt32 *outAvatarId)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "avatarurl");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = 0;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *urlText = ScebXCe_33825EFF(doc, textNode);
		if (urlText != NULL) {
			if (sceClibStrnlen(urlText, 0x7FFFFFFFU) > 127U) {
				ret = NP_COMMUNITY_ERR_PARSE_FAIL;
			} else {
				sceClibStrncpy(outAvatarUrl, urlText, 127U);
			}
		} else {
			sceClibMemset(outAvatarUrl, 0, 128U);
		}

		if (ret >= 0) {
			const char *idStr = NULL;
			npCommunityXmlFindAttr(&idStr, doc, elem, "id");
			if (idStr != NULL)
				*outAvatarId = SceNpCommon_CC8E6AC6(idStr);
			else
				*outAvatarId = 0;
		}
	} else {
		sceClibMemset(outAvatarUrl, 0, 128U);
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F70F8 */
int npCommunityXmlParsePanelUrl(const BxceDocument *doc, BxceNodeId parent,
				NpCommunityPanelInfo *outPanel)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "panelurl");
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = 0;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode == 0U) {
		sceClibMemset(outPanel, 0, sizeof(NpCommunityPanelInfo));
		npXmlFreeNodeList(list);
		return 0;
	}

	const char *urlText = ScebXCe_33825EFF(doc, textNode);
	if (urlText != NULL) {
		if (sceClibStrnlen(urlText, 0x7FFFFFFFU) > 128U) {
			npXmlFreeNodeList(list);
			return NP_COMMUNITY_ERR_PARSE_FAIL;
		}
		sceClibStrncpy(outPanel->url, urlText, 127U);
	} else {
		sceClibMemset(outPanel, 0, sizeof(NpCommunityPanelInfo));
	}

	const char *idStr = NULL;
	npCommunityXmlFindAttr(&idStr, doc, elem, "id");
	outPanel->panelId = (idStr != NULL) ? SceNpCommon_CC8E6AC6(idStr) : 0;

	const char *bgcStr = NULL;
	npCommunityXmlFindAttr(&bgcStr, doc, elem, "bgc");
	if (bgcStr != NULL) {
		outPanel->bgColor = SceNpCommon_5B770B80(bgcStr, NULL, 16);
		outPanel->panelType = 2;
		ret = 1;
	} else {
		ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F7280 */
int npCommunityXmlParseShortString(const BxceDocument *doc, BxceNodeId parent, const char *tagName,
				   char *outBuf)
{
	sceClibMemset(outBuf, 0, 20U);
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL && sceClibStrnlen(text, 0x7FFFFFFFU) <= 16U) {
			sceClibStrncpy(outBuf, text, 16U);
			ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F731C */
int npCommunityXmlParseBgColorFallback(const BxceDocument *doc, BxceNodeId parent,
				       NpCommunityPanelInfo *outPanel)
{
	NpCommunityPanelInfo localPanel;
	sceClibMemset(&localPanel, 0, sizeof(localPanel));

	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, "ucbgp");
	if (list != NULL) {
		BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
		BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
		if (textNode != 0U) {
			const char *text = ScebXCe_33825EFF(doc, textNode);
			SceUInt64 parsedColor = SceNpCommon_CF843378(text, NULL, 16);
			localPanel.bgColor = static_cast<SceUInt32>(parsedColor & 0xFFFFFFFFULL);
			localPanel.panelType = static_cast<SceInt32>(parsedColor >> 32);
		}
		npXmlFreeNodeList(list);
	}

	if (outPanel != NULL)
		*outPanel = localPanel;
	return 0;
}

/* 0x813F73DA */
int npCommunityXmlParsePremiumAttr(const BxceDocument *doc, BxceNodeId node, const char *attrName,
				   SceInt32 *outIsPremium)
{
	const char *valStr = NULL;
	npCommunityXmlFindAttr(&valStr, doc, node, attrName);
	if (valStr != NULL) {
		SceSize len = sceClibStrnlen(valStr, 0x7FFFFFFFU);
		*outIsPremium = (sceClibStrncmp(valStr, "p", len + 1U) == 0) ? 1 : 0;
	} else {
		*outIsPremium = 0;
	}
	return 0;
}

/* 0x813F7466 */
int npCommunityXmlParseChildUintAndAttr(const BxceDocument *doc, BxceNodeId parent,
					const char *tagName, const char *attrName,
					SceUInt32 *outChildValue, SceUInt32 *outAttrValue)
{
	BxceNodeList *list = ScebXCe_1FA24DC0(doc, parent, tagName);
	if (list == NULL)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	int ret = NP_COMMUNITY_ERR_PARSE_FAIL;
	BxceNodeId elem = ScebXCe_6C46D786(list, 0U);
	BxceNodeId textNode = ScebXCe_FEA6E4A1(doc, elem);
	if (textNode != 0U) {
		const char *text = ScebXCe_33825EFF(doc, textNode);
		if (text != NULL) {
			if (outChildValue != NULL)
				*outChildValue = SceNpCommon_5B770B80(text, NULL, 10);
			if (outAttrValue != NULL) {
				const char *attrVal = NULL;
				npCommunityXmlFindAttr(&attrVal, doc, elem, attrName);
				if (attrVal != NULL)
					*outAttrValue = SceNpCommon_5B770B80(attrVal, NULL, 10);
				else
					*outAttrValue = 0U;
			}
			ret = 0;
		}
	}

	npXmlFreeNodeList(list);
	return ret;
}

/* 0x813F7544 */
static void npCommunityAppendPosInt32(char **cursor, SceInt32 value)
{
	if (value != 0) {
		npCommunityAppendPosInt32(cursor, value / 10);
		**cursor = static_cast<char>((value % 10) + '0');
		++(*cursor);
	}
}

/* 0x813F7584 */
void npCommunityFormatInt32(char *dst, SceInt32 value)
{
	char *cursor = dst;
	if (value < 0) {
		sceClibMemcpy_safe(cursor, "-", 2U);
		++cursor;
		value = -value;
	}
	if (value != 0)
		npCommunityAppendPosInt32(&cursor, value);
	else
		sceClibMemcpy_safe(cursor, "0", 2U);
}

/* 0x813F75DE */
static void npCommunityAppendPosUint64(char **cursor, SceUInt64 value)
{
	if (value != 0ULL) {
		npCommunityAppendPosUint64(cursor, value / 10ULL);
		**cursor = static_cast<char>((value % 10ULL) + '0');
		++(*cursor);
	}
}

/* 0x813F7630 */
void npCommunityFormatUint64(char *dst, SceUInt64 value)
{
	char *cursor = dst;
	if (value != 0ULL)
		npCommunityAppendPosUint64(&cursor, value);
	else
		sceClibMemcpy_safe(cursor, "0", 2U);
}

/* 0x813F7676 */
void npCommunityFormatInt64(char *dst, SceInt64 value)
{
	char *cursor = dst;
	SceUInt64 mag = static_cast<SceUInt64>(value);
	if (value < 0) {
		sceClibMemcpy_safe(cursor, "-", 2U);
		++cursor;
		mag = static_cast<SceUInt64>(-value);
	}
	if (mag != 0ULL)
		npCommunityAppendPosUint64(&cursor, mag);
	else
		sceClibMemcpy_safe(cursor, "0", 2U);
}

/* 0x813F76F4 */
SceInt64 npCommunityParseInt64(const char *text)
{
	if (text == NULL)
		return 0LL;

	SceInt64 accum = 0LL;
	bool isNeg = (text[0] == '-');
	SceSize idx = isNeg ? 1U : 0U;
	while (text[idx] != '\0') {
		if ((sceClibLookCtypeTable(static_cast<unsigned char>(text[idx])) & 8) != 0)
			break;
		if (idx >= 20U)
			break;
		accum = accum * 10LL + static_cast<SceInt64>(text[idx] - '0');
		++idx;
	}
	return isNeg ? -accum : accum;
}

/* 0x813F7778 */
int npCommunityParseHexSignature(SceUInt8 *outDigest, const char *hexStr, SceSize hexLen)
{
	if (hexLen < 40U)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	char pair[4];
	for (SceSize idx = 0U; idx < SCE_SHA1_DIGEST_SIZE; ++idx) {
		sceClibMemcpy_safe(pair, hexStr + idx * 2U, 2U);
		pair[2] = '\0';
		unsigned int byteVal =
			static_cast<unsigned int>(SceNpCommon_4C4AA451(pair, NULL, 16));
		if (byteVal >= 0x100U)
			return NP_COMMUNITY_ERR_PARSE_FAIL;
		outDigest[idx] = static_cast<SceUInt8>(byteVal);
	}
	return 0;
}

/* 0x813F77EA */
int npCommunityHttpReadExact(NpCommunityBackendTx *tx, void *dst, SceSize size, SceSize *outRead)
{
	SceUInt8 *dstBytes = static_cast<SceUInt8 *>(dst);
	SceSize remaining = size;
	int ret = 0;

	while (remaining != 0U) {
		if (tx->aborted == 1)
			return NP_COMMUNITY_ERR_ABORTED;
		ret = sceHttpReadData(tx->httpReqId, dstBytes + (size - remaining), remaining);
		if (ret < 0)
			return ret;
		if (ret == 0)
			break;
		remaining -= static_cast<SceSize>(ret);
	}

	*outRead = size - remaining;
	return 0;
}

/* 0x813F7834 */
int npCommunityHttpReadCommentSig(NpCommunityBackendTx *tx, SceUInt8 *outDigest,
				  SceUInt64 *ioContentLength)
{
	if (tx->aborted == 1)
		return NP_COMMUNITY_ERR_ABORTED;

	char lineBuf[68];
	SceSize bytesRead = 0U;
	int ret = npCommunityHttpReadExact(tx, lineBuf, NP_COMMUNITY_SIG_COMMENT_LEN, &bytesRead);
	if (ret < 0)
		return ret;
	if (bytesRead != NP_COMMUNITY_SIG_COMMENT_LEN)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	SceSize prefixLen = sceClibStrnlen("<!--", 0x7FFFFFFFU) +
			    sceClibStrnlen("Sce-Np-Signature", 0x7FFFFFFFU) +
			    sceClibStrnlen(": ", 0x7FFFFFFFU);
	if (sceClibStrncasecmp(lineBuf, "<!--Sce-Np-Signature: ", prefixLen) != 0)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	ret = npCommunityParseHexSignature(outDigest, lineBuf + prefixLen,
					   NP_COMMUNITY_SIG_COMMENT_LEN - prefixLen);
	if (ret < 0)
		return ret;

	SceSize totalConsumed = NP_COMMUNITY_SIG_COMMENT_LEN;
	char ch = '\0';
	ret = npCommunityHttpReadExact(tx, &ch, 1U, &bytesRead);
	if (ret < 0)
		return ret;
	if (bytesRead != 1U)
		return NP_COMMUNITY_ERR_PARSE_FAIL;
	++totalConsumed;

	if (ch == '\r') {
		ret = npCommunityHttpReadExact(tx, &ch, 1U, &bytesRead);
		if (ret < 0)
			return ret;
		if (bytesRead != 1U)
			return NP_COMMUNITY_ERR_PARSE_FAIL;
		++totalConsumed;
	}

	if (ch != '\n')
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	*ioContentLength -= static_cast<SceUInt64>(totalConsumed);
	return 0;
}

/* 0x813F7A22 */
void npCommunityFormatHexBytes(char *dst, const SceUInt8 *src, SceSize len)
{
	for (SceSize idx = 0U; idx < len; ++idx) {
		sceClibSnprintf(dst, 3U, "%02x", src[idx]);
		dst += 2;
	}
}

/* 0x813F7A4A */
void npCommunityHttpAddInfoLengthHeader(SceInt32 httpReqId, SceSize infoLen)
{
	char lenText[24];
	sceClibMemset(lenText, 0, 21U);
	npCommunityFormatUint64(lenText, static_cast<SceUInt64>(infoLen));
	sceHttpAddRequestHeader(httpReqId, "Sce-Np-Info-Length", lenText, 0U);
}

/* 0x813F7AC0 */
void npCommunityHttpAddRangeHeader(SceInt32 httpReqId, const SceUInt64 *startByte,
				   const SceUInt64 *endByte)
{
	char rangeText[48];
	sceClibMemset(rangeText, 0, sizeof(rangeText));
	sceClibStrlcpy(rangeText, "bytes=", sizeof(rangeText));

	if (startByte != NULL) {
		SceSize curLen = sceClibStrnlen(rangeText, 0x7FFFFFFFU);
		npCommunityFormatUint64(rangeText + curLen, *startByte);
	}
	sceClibStrlcat(rangeText, "-", sizeof(rangeText));
	if (endByte != NULL) {
		SceSize curLen = sceClibStrnlen(rangeText, 0x7FFFFFFFU);
		npCommunityFormatUint64(rangeText + curLen, *endByte);
	}

	sceHttpAddRequestHeader(httpReqId, "Range", rangeText, 0U);
}

/* 0x813F7BA2 */
void npCommunityHttpAddDateHeader(SceInt32 httpReqId, const char *headerName,
				  const SceRtcTick *utcTick)
{
	char dateText[48];
	if (sceRtcFormatRFC2822(dateText, utcTick, 0) >= 0)
		sceHttpAddRequestHeader(httpReqId, headerName, dateText, 0U);
}

/* 0x813F7BE2 */
int npCommunityHttpParseDateHeader(SceInt32 httpReqId, const char *headerName, SceRtcTick *outTick)
{
	char *allHeaders = NULL;
	SceSize allHeadersLen = 0U;
	const char *fieldValue = NULL;
	SceSize fieldLen = 0U;

	int ret = sceHttpGetAllResponseHeaders(httpReqId, &allHeaders, &allHeadersLen);
	if (ret < 0)
		return ret;

	ret = sceHttpParseResponseHeader(allHeaders, allHeadersLen, headerName, &fieldValue,
					 &fieldLen);
	if (ret < 0)
		return ret;

	SceSize bufSize = fieldLen + 1U;
	char *copy = static_cast<char *>(npCommunityAlloc(bufSize));
	if (copy == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibStrlcpy(copy, fieldValue, bufSize);
	ret = sceRtcParseDateTime(outTick, copy);
	npCommunityFree(copy);
	return ret;
}

/* 0x813F7C74 */
int npCommunityParseJidToNpId(SceNpId *outNpId, const char *jidStr)
{
	NpJid jid;
	SceSize needed = 0U;
	int ret = SceNpCommon_D2B9BCA8(&jid, jidStr, NULL, 0U, &needed);
	if (ret < 0)
		return ret;
	if (needed == 0U)
		return NP_COMMUNITY_ERR_PARSE_FAIL;

	char *workBuf = static_cast<char *>(npCommunityAlloc(needed));
	if (workBuf == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	ret = SceNpCommon_D2B9BCA8(&jid, jidStr, workBuf, needed, NULL);
	if (ret >= 0) {
		ret = SceNpCommon_9B7CAA67(outNpId, &jid);
		if (ret >= 0)
			ret = 0;
	}
	npCommunityFree(workBuf);
	return ret;
}

/* 0x813F7D08 */
int npCommunityFormatNpIdToJid(char **outJidStr, const SceNpId *npId)
{
	SceSize workSize = 0U;
	int ret = SceNpCommon_40CC4D15(NULL, npId, NULL, 0U, &workSize);
	if (ret < 0)
		return ret;

	char *workBuf = static_cast<char *>(npCommunityAlloc(workSize));
	if (workBuf == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	char *jidBuf = NULL;
	NpJid jid;
	ret = SceNpCommon_40CC4D15(&jid, npId, workBuf, workSize, NULL);
	if (ret >= 0) {
		jid.resource = NULL;
		SceSize jidSize = 0U;
		ret = SceNpCommon_85699AE2(&jid, NULL, 0U, &jidSize);
		if (ret >= 0) {
			jidBuf = static_cast<char *>(npCommunityAlloc(jidSize));
			if (jidBuf != NULL) {
				ret = SceNpCommon_85699AE2(&jid, jidBuf, jidSize, NULL);
				if (ret >= 0) {
					npCommunityFree(workBuf);
					*outJidStr = jidBuf;
					return 0;
				}
			} else {
				ret = NP_COMMUNITY_ERR_NO_MEMORY;
			}
		}
	}

	npCommunityFree(workBuf);
	if (jidBuf != NULL)
		npCommunityFree(jidBuf);
	return ret;
}

/* 0x813F7DD6 */
int npCommunityEscapeXmlAlloc(char **outEscaped, const char *src)
{
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFU);
	char *escaped = NULL;
	int ret;

	if (srcLen == 0U) {
		escaped = static_cast<char *>(npCommunityAlloc(1U));
		if (escaped != NULL)
			escaped[0] = '\0';
		*outEscaped = escaped;
		return 0;
	}

	SceSize required = 0U;
	ret = SceNpCommon_C2BF5C1F(NULL, 0U, reinterpret_cast<const SceUInt8 *>(src), srcLen,
				   &required);
	if (ret >= 0) {
		escaped = static_cast<char *>(npCommunityAlloc(required));
		if (escaped != NULL) {
			sceClibMemset(escaped, 0, required);
			ret = SceNpCommon_C2BF5C1F(escaped, required,
						   reinterpret_cast<const SceUInt8 *>(src), srcLen,
						   NULL);
			if (ret >= 0) {
				ret = 0;
			} else {
				npCommunityFree(escaped);
				escaped = NULL;
			}
		} else {
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}

	*outEscaped = escaped;
	return ret;
}

/* 0x813F7E8C */
int npCommunityUnescapeXmlAlloc(char **outUnescaped, const char *src)
{
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFU);
	SceSize required = 0U;
	char *unescaped = NULL;

	int ret = SceNpCommon_389F8C14(NULL, 0U, reinterpret_cast<const SceUInt8 *>(src), srcLen,
				       &required);
	if (ret >= 0) {
		unescaped = static_cast<char *>(npCommunityAlloc(required));
		if (unescaped != NULL) {
			ret = SceNpCommon_389F8C14(unescaped, required,
						   reinterpret_cast<const SceUInt8 *>(src), srcLen,
						   NULL);
			if (ret >= 0) {
				ret = 0;
			} else {
				npCommunityFree(unescaped);
				unescaped = NULL;
			}
		} else {
			ret = NP_COMMUNITY_ERR_NO_MEMORY;
		}
	}

	*outUnescaped = unescaped;
	return ret;
}

/* 0x813F7F22 */
int npCommunityXmlVerifyTitleId(const BxceDocument *doc, BxceNodeId node,
				const SceNpCommunicationId *expectedCommId)
{
	char commIdText[16];
	sceClibMemset(commIdText, 0, 13U);
	int ret = SceNpCommon_E426FEE8(expectedCommId, commIdText, 13U);
	if (ret < 0)
		return ret;

	const char *xmlTitleId = NULL;
	if (npCommunityXmlFindAttr(&xmlTitleId, doc, node, "titleid") < 0 || xmlTitleId == NULL)
		return NP_COMMUNITY_ERR_VERIFY_FAIL;

	SceSize len = sceClibStrnlen(commIdText, 0x7FFFFFFFU);
	if (sceClibStrncasecmp(xmlTitleId, commIdText, len + 1U) != 0)
		return NP_COMMUNITY_ERR_VERIFY_FAIL;
	return 0;
}

/* 0x813F7FB8 */
void npCommunityCompactLanguages(SceInt32 *languages)
{
	if (languages == NULL)
		return;

	SceInt32 secondLang = languages[1];
	SceInt32 thirdLang = languages[2];

	if (secondLang >= 0 && (languages[0] < 0 || languages[0] == secondLang)) {
		languages[0] = secondLang;
		languages[1] = -1;
		secondLang = -1;
	}
	if (thirdLang >= 0) {
		if (languages[0] < 0 || languages[0] == thirdLang) {
			languages[0] = thirdLang;
			languages[2] = -1;
		} else if (secondLang < 0 || secondLang == thirdLang) {
			languages[1] = thirdLang;
			languages[2] = -1;
		}
	}
}

/* 0x813F800A */
int npCommunityFormatOpenPsId(char *outHex)
{
	SceUInt8 openPsId[16];
	int ret = sceKernelGetOpenPsId(openPsId);
	if (ret < 0)
		return ret;

	sceClibMemset(outHex, 0, 33U);
	char *cursor = outHex;
	SceSize remaining = 33U;
	for (SceSize idx = 0U; idx < 16U; ++idx) {
		sceClibSnprintf(cursor, remaining, "%02x", openPsId[idx]);
		cursor += 2;
		remaining -= 2U;
	}
	return 0;
}

/* 0x813F8076 */
SceSize npCommunityAppendBounded(char *dst, const char *src, SceSize maxTotal)
{
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFU);
	SceSize dstLen = sceClibStrnlen(dst, 0x7FFFFFFFU);
	if (maxTotal == 0U)
		return 0U;

	if (maxTotal > dstLen + 1U) {
		SceSize avail = maxTotal - dstLen - 1U;
		if (avail > srcLen)
			avail = srcLen;
		sceClibMemcpy_safe(dst + dstLen, src, avail);
		dst[dstLen + avail] = '\0';
		return dstLen + avail;
	}

	dst[maxTotal] = '\0';
	return maxTotal - 1U;
}

/* 0x813F80CC */
int npCommunityHttpRecvXmlEx(NpCommunityBackendTx *tx, BxceDocument **outDoc, int sigMode,
			     SceUInt64 *outRemainingBytes, const void *passphrase)
{
	SceInt32 statusCode = 0;
	SceUInt64 contentLength = 0ULL;
	SceUInt32 infoLength = 0U;
	SceUInt8 expectedSig[SCE_SHA1_DIGEST_SIZE];
	char *xmlBuf = NULL;
	BxceDomParser *parser = NULL;

	int ret = npCommunityHttpReadHeaders(tx, &statusCode, &contentLength, &infoLength,
					     expectedSig, sigMode);
	if (ret >= 0) {
		if (outDoc == NULL) {
			if (tx->serviceType == 26 || tx->serviceType == 27) {
				if (outRemainingBytes != NULL)
					*outRemainingBytes = contentLength;
			} else {
				npCommunityCloseTxHttp(tx);
			}
			return 0;
		}

		if (contentLength == 0ULL) {
			ret = NP_COMMUNITY_ERR_PARSE_FAIL;
		} else {
			bool hasTrailer = false;
			SceSize xmlSize = 0U;
			if (infoLength != 0U) {
				hasTrailer = true;
				if (static_cast<SceUInt64>(infoLength) > contentLength) {
					ret = NP_COMMUNITY_ERR_PARSE_FAIL;
				} else {
					xmlSize = infoLength;
					if (outRemainingBytes != NULL)
						*outRemainingBytes =
							contentLength -
							static_cast<SceUInt64>(infoLength);
				}
			} else if (contentLength > NP_COMMUNITY_MAX_XML_SIZE) {
				ret = NP_COMMUNITY_ERR_RESPONSE_TOO_LARGE;
			} else {
				xmlSize = static_cast<SceSize>(contentLength);
				if (outRemainingBytes != NULL)
					*outRemainingBytes = 0ULL;
			}

			if (ret >= 0) {
				if (xmlSize > NP_COMMUNITY_MAX_XML_SIZE) {
					ret = NP_COMMUNITY_ERR_RESPONSE_TOO_LARGE;
				} else {
					xmlBuf = static_cast<char *>(npCommunityAlloc(xmlSize + 1U));
					if (xmlBuf == NULL) {
						ret = NP_COMMUNITY_ERR_NO_MEMORY;
					} else if (tx->aborted == 1) {
						ret = NP_COMMUNITY_ERR_ABORTED;
					} else {
						SceSize readSize = 0U;
						ret = npCommunityHttpReadExact(tx, xmlBuf, xmlSize,
									       &readSize);
						if (ret >= 0) {
							if (readSize != xmlSize) {
								ret = NP_COMMUNITY_ERR_PARSE_FAIL;
							} else {
								if (!hasTrailer)
									npCommunityCloseTxHttp(tx);
								if (sigMode >=
									    NP_COMMUNITY_SIG_COMMENT_RANKING_LIST &&
								    sigMode <=
									    NP_COMMUNITY_SIG_COMMENT_EXTRA) {
									ret = npCommunityVerifyCommentHmac(
										xmlBuf, xmlSize,
										expectedSig,
										sigMode);
								}
							}
						}
					}
				}
			}

			if (ret >= 0) {
				xmlBuf[xmlSize] = '\0';
				parser = static_cast<BxceDomParser *>(
					ScebXCe_B8DDE741(sizeof(BxceDomParser)));
				if (parser == NULL) {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				} else if (ScebXCe_11D1FC0D(parser) != 0) {
					ret = NP_COMMUNITY_ERR_NO_MEMORY;
				} else {
					ScebXCe_A40944DD(parser, 0);
					if (ScebXCe_BC0D9A6B(parser, xmlBuf, xmlSize, 1) != 0) {
						ret = NP_COMMUNITY_ERR_PARSE_FAIL;
					} else {
						BxceDocument *parsedDoc = parser->document;
						if (parser->parseStatus != NP_BXCE_PARSER_ERR_MASK)
							parser->document = NULL;
						*outDoc = parsedDoc;
						if (parsedDoc == NULL) {
							ret = NP_COMMUNITY_ERR_NO_MEMORY;
						} else {
							if (sigMode ==
							    NP_COMMUNITY_SIG_RANKING_HEADER) {
								ret = npCommunityXmlCheckResult(
									parsedDoc, "ranking");
								if (ret >= 0)
									ret = npCommunityVerifyPassphraseHmac(
										xmlBuf, xmlSize,
										expectedSig,
										sigMode,
										passphrase);
							}
							if (ret >= 0) {
								npCommunityFree(xmlBuf);
								ScebXCe_8FA315F1(parser);
								ScebXCe_EC560647(parser);
								return 0;
							}
						}
					}
				}
			}
		}
	}

	npCommunityCloseTxHttp(tx);
	if (xmlBuf != NULL)
		npCommunityFree(xmlBuf);
	if (parser != NULL) {
		ScebXCe_8FA315F1(parser);
		ScebXCe_EC560647(parser);
	}
	return ret;
}

/* 0x813F8390 */
int npCommunityVerifyCommentHmac(const void *body, SceSize bodyLen, const SceUInt8 *expectedDigest,
				 int sigMode)
{
	if (sigMode < NP_COMMUNITY_SIG_COMMENT_RANKING_LIST ||
	    sigMode > NP_COMMUNITY_SIG_COMMENT_EXTRA) {
		return NP_COMMUNITY_ERR_UNSUPPORTED_SIG;
	}

	const SceUInt8 *xorKey = s_commentHmacXorKeys[sigMode - NP_COMMUNITY_SIG_COMMENT_RANKING_LIST];
	SceSize paddedLen = bodyLen + ((64U - (bodyLen & 63U)) & 63U);
	void *paddedBody = npCommunityAlloc(paddedLen);
	if (paddedBody == NULL)
		return NP_COMMUNITY_ERR_NO_MEMORY;

	sceClibMemset(paddedBody, 0, paddedLen);
	sceClibMemcpy_safe(paddedBody, body, bodyLen);

	SceUInt8 hmacKey[SCE_SHA1_DIGEST_SIZE];
	for (SceSize idx = 0U; idx < SCE_SHA1_DIGEST_SIZE; ++idx)
		hmacKey[idx] = xorKey[idx] ^ 0x7FU;

	SceUInt8 computedDigest[SCE_SHA1_DIGEST_SIZE];
	sceClibMemset(computedDigest, 0, sizeof(computedDigest));
	npCommunityHmacSha1(hmacKey, sizeof(hmacKey), paddedBody, paddedLen, computedDigest);
	npCommunityFree(paddedBody);

	if (sceClibMemcmp(expectedDigest, computedDigest, SCE_SHA1_DIGEST_SIZE) != 0)
		return NP_COMMUNITY_ERR_VERIFY_FAIL;
	return 0;
}

/* 0x813F852E */
int npCommunityVerifyPassphraseHmac(const void *body, SceSize bodyLen,
				    const SceUInt8 *expectedDigest, int sigMode,
				    const void *passphrase)
{
	if (sigMode != NP_COMMUNITY_SIG_RANKING_HEADER)
		return NP_COMMUNITY_ERR_UNSUPPORTED_SIG;

	SceUInt8 computedDigest[SCE_SHA1_DIGEST_SIZE];
	sceClibMemset(computedDigest, 0, sizeof(computedDigest));
	npCommunityHmacSha1(passphrase, NP_COMMUNITY_PASS_KEY_LEN, body, bodyLen, computedDigest);

	if (sceClibMemcmp(expectedDigest, computedDigest, SCE_SHA1_DIGEST_SIZE) != 0)
		return NP_COMMUNITY_ERR_VERIFY_FAIL;
	return 0;
}
