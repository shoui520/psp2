/*
 * np_rt_ext.cpp: SceNpRt XMPP extension payload serializers, parsers,
 * cloners, and destructors for message and presence stanzas
 * (0x81405EE8..0x8140910B).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <rtc.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_core.h"
#include "np_rt_ext.h"

extern "C" {
SceInt64 SceNpCommon_E1FF4E70(const char *str);
}

namespace {

struct NamedEnumEntry {
	const char *name;
	int value;
};

/* 0x81549614: Notification extension type table */
static const NamedEnumEntry kNotifTypeTable[] = {
	{ "mail", 0 },
	{ NULL,   -1 }
};

/* 0x81549624: Inline data encoding attribute table */
static const NamedEnumEntry kInlineFormatTable[] = {
	{ "",       NP_RT_INLINE_NONE },
	{ "base64", NP_RT_INLINE_BASE64 },
	{ "uri",    NP_RT_INLINE_URI },
	{ "fileid", NP_RT_INLINE_FILEID },
	{ NULL,     -1 }
};

/* 0x8154967C: Presence title mode table */
static const NamedEnumEntry kPresenceModeTable[] = {
	{ "",       0 },
	{ "game",   1 },
	{ "video",  2 },
	{ "avchat", 3 },
	{ "music",  4 },
	{ NULL,     -1 }
};

static const char *lookupNameByValue(const NamedEnumEntry *table, int value)
{
	for (const NamedEnumEntry *cur = table; cur->name != NULL; ++cur) {
		if (cur->value == value)
			return cur->name;
	}
	return NULL;
}

static int lookupValueByName(const NamedEnumEntry *table, const char *str)
{
	if (str == NULL)
		return -1;
	SceSize len = sceClibStrnlen(str, 0x7FFFFFFFu);
	for (const NamedEnumEntry *cur = table; cur->name != NULL; ++cur) {
		if (sceClibStrncmp(str, cur->name, len) == 0)
			return cur->value;
	}
	return -1;
}

/* 0x814064D8 */
static const char *inlineFormatToString(int format)
{
	if (format == NP_RT_INLINE_BASE64)
		return "base64";
	if (format == NP_RT_INLINE_URI)
		return "uri";
	if (format == NP_RT_INLINE_FILEID)
		return "fileid";
	return NULL;
}

/* 0x8140650E */
static int parseInlineFormat(const char *str)
{
	if (str == NULL)
		return 0;
	return lookupValueByName(kInlineFormatTable, str);
}

static int appendXmlEscapedElement(BxceDocument *doc, BxceNodeId parentNode,
				   const char *tagName, const char *text)
{
	SceSize textLen = sceClibStrnlen(text, 0x7FFFFFFFu);
	if (textLen == 0)
		return 0;

	SceSize escapedSize = 0;
	int ret = SceNpCommon_C2BF5C1F(
		NULL, 0, reinterpret_cast<const SceUInt8 *>(text), textLen, &escapedSize);
	if (ret < 0)
		return ret;

	if (escapedSize - 1 == textLen) {
		if (ScebXCe_AC0E4F67(doc, parentNode, tagName, NULL, text, textLen) == 0)
			return NP_RT_CORE_ERR_XML_ATTR;
		return 0;
	}

	char *escapedBuf = reinterpret_cast<char *>(npRtHeapAlloc(escapedSize, NULL));
	ret = SceNpCommon_C2BF5C1F(
		escapedBuf, escapedSize, reinterpret_cast<const SceUInt8 *>(text), textLen, NULL);
	if (ret < 0) {
		npRtHeapFree(escapedBuf, NULL);
		return ret;
	}

	BxceNodeId node = ScebXCe_AC0E4F67(doc, parentNode, tagName, NULL,
					   escapedBuf, escapedSize - 1);
	npRtHeapFree(escapedBuf, NULL);
	return (node != 0) ? 0 : NP_RT_CORE_ERR_XML_ATTR;
}

/* --------------------------------------------------------------------------
 * Tick / delay helpers (0x8140762C..0x814076E3)
 * -------------------------------------------------------------------------- */

/* 0x8140769C */
static void freeTickOrSession(void *ptr, void *customAlloc)
{
	npRtHeapFree(ptr, customAlloc);
}

/* 0x814076A4 */
static int cloneTick(const SceRtcTick *src, SceRtcTick **outCopy, void *customAlloc)
{
	if (src == NULL) {
		*outCopy = NULL;
		return 0;
	}
	SceRtcTick *copy = reinterpret_cast<SceRtcTick *>(npRtHeapAlloc(sizeof(SceRtcTick), customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;
	sceClibMemset(copy, 0, sizeof(SceRtcTick));
	sceClibMemcpy_safe(copy, src, sizeof(SceRtcTick));
	*outCopy = copy;
	return 0;
}

/* 0x8140762C */
static int parseDelayStamp(SceRtcTick **outTick, BxceNodeId elemNode, BxceDocument *doc,
			   void *customAlloc)
{
	const char *stampStr = ScebXCe_753D9382(doc, elemNode, "stamp");
	if (stampStr == NULL) {
		*outTick = NULL;
		return 0;
	}

	SceRtcTick utc;
	int ret = sceRtcParseDateTime(&utc, stampStr);
	if (ret < 0)
		return ret;

	SceRtcTick *copy = reinterpret_cast<SceRtcTick *>(npRtHeapAlloc(sizeof(SceRtcTick), customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;
	sceClibMemcpy_safe(copy, &utc, sizeof(SceRtcTick));
	*outTick = copy;
	return 0;
}

/* --------------------------------------------------------------------------
 * Notification extension ("http://playstation.net/np/rt/notification")
 * 0x8140621C..0x814064D7
 * -------------------------------------------------------------------------- */

/* 0x81406332 */
static void freeNotificationExt(NpImpsCoreNotificationExt *notif, void *customAlloc, SceUInt8 flags)
{
	if (notif == NULL)
		return;
	npRtFreeString(const_cast<char *>(notif->text), customAlloc, flags);
	npRtHeapFree(notif, customAlloc);
}

/* 0x8140621C */
static int serializeNotificationExt(BxceDocument *doc, const NpImpsCoreNotificationExt *notif)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	int ret = 0;
	if (ScebXCe_E7C8B20D(attrs) != 0) {
		ret = NP_RT_CORE_ERR_XML_ATTR;
	} else if (ScebXCe_3DD761CF(attrs, "xmlns",
				    "http://playstation.net/np/rt/notification") != 0) {
		ret = NP_RT_CORE_ERR_NOT_FOUND;
	} else {
		const char *typeStr = lookupNameByValue(kNotifTypeTable, notif->kind);
		if (ScebXCe_3DD761CF(attrs, "type", typeStr) != 0) {
			ret = NP_RT_CORE_ERR_NOT_FOUND;
		} else {
			BxceNodeId root = ScebXCe_75E1B095(doc);
			BxceNodeId xNode = ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0);
			if (xNode == 0) {
				ret = NP_RT_CORE_ERR_NOT_FOUND;
			} else if (notif->text == NULL) {
				return 0;
			} else {
				SceSize len = sceClibStrnlen(notif->text, 0x7FFFFFFFu);
				if (ScebXCe_AC0E4F67(doc, xNode, "mid", NULL, notif->text, len) != 0)
					return 0;
				ret = NP_RT_CORE_ERR_NOT_FOUND;
			}
		}
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x8140634C */
static int parseNotificationExt(NpImpsCoreNotificationExt **outNotif, BxceNodeId elemNode,
				BxceDocument *doc, void *customAlloc, SceUInt8 flags)
{
	NpImpsCoreNotificationExt *notif = reinterpret_cast<NpImpsCoreNotificationExt *>(
		npRtHeapAlloc(sizeof(NpImpsCoreNotificationExt), customAlloc));
	if (notif == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(notif, 0, sizeof(NpImpsCoreNotificationExt));
	notif->kind = -1;

	const char *typeStr = ScebXCe_753D9382(doc, elemNode, "type");
	if (typeStr != NULL)
		notif->kind = lookupValueByName(kNotifTypeTable, typeStr);

	const char *midText = NULL;
	npRtGetElementText(doc, elemNode, "mid", &midText);
	if (midText != NULL) {
		char *dupText = NULL;
		int ret = npRtDupString(&dupText, midText, customAlloc, flags);
		if (ret < 0) {
			freeNotificationExt(notif, customAlloc, flags);
			return ret;
		}
		notif->text = dupText;
	}

	*outNotif = notif;
	return 0;
}

/* 0x8140645E */
static int cloneNotificationExt(const NpImpsCoreNotificationExt *src,
				NpImpsCoreNotificationExt **outCopy,
				void *customAlloc, SceUInt8 flags)
{
	if (src == NULL) {
		*outCopy = NULL;
		return 0;
	}

	NpImpsCoreNotificationExt *copy = reinterpret_cast<NpImpsCoreNotificationExt *>(
		npRtHeapAlloc(sizeof(NpImpsCoreNotificationExt), customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, sizeof(NpImpsCoreNotificationExt));
	copy->kind = src->kind;

	char *dupText = NULL;
	int ret = npRtDupString(&dupText, src->text, customAlloc, flags);
	if (ret < 0) {
		freeNotificationExt(copy, customAlloc, flags);
		return ret;
	}
	copy->text = dupText;
	*outCopy = copy;
	return 0;
}

/* --------------------------------------------------------------------------
 * Ext data extension ("http://playstation.net/np/rt/ext")
 * 0x81406554..0x81407043
 * -------------------------------------------------------------------------- */

/* 0x81406AB4 */
static void freeExtDataList(NpImpsCoreDataNode *head, void *customAlloc, SceUInt8 flags)
{
	NpImpsCoreDataNode *cur = head;
	while (cur != NULL) {
		NpImpsCoreDataNode *next = cur->next;
		npRtFreeString(cur->mimeType, customAlloc, flags);
		if (cur->valid == NP_RT_INLINE_BASE64)
			npRtHeapFree(cur->dataPtr, customAlloc);
		else
			npRtFreeString(cur->dataPtr, customAlloc, flags);
		npRtFreeString(cur->dataName, customAlloc, flags);
		npRtHeapFree(cur, customAlloc);
		cur = next;
	}
}

/* 0x81406554 */
static int serializeExtDataList(BxceDocument *doc, const NpImpsCoreDataNode *head)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	for (const NpImpsCoreDataNode *node = head; node != NULL && ret >= 0; node = node->next) {
		if (node->targetCommId.data[0] == '\0')
			continue;

		if (ScebXCe_3DD761CF(attrs, "xmlns", "http://playstation.net/np/rt/ext") != 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		char commIdText[16];
		if (node->commId.data[0] != '\0') {
			ret = SceNpCommon_E426FEE8(&node->commId, commIdText, 13);
			if (ret < 0)
				break;
			if (ScebXCe_3DD761CF(attrs, "context", commIdText) != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (node->activeState == 1) {
			if (ScebXCe_3DD761CF(attrs, "active", "0") != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		} else if (node->activeState == 2) {
			if (ScebXCe_3DD761CF(attrs, "active", "1") != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		BxceNodeId root = ScebXCe_75E1B095(doc);
		BxceNodeId xNode = ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0);
		if (xNode == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		ret = SceNpCommon_E426FEE8(&node->targetCommId, commIdText, 13);
		if (ret < 0)
			break;
		if (ScebXCe_AC0E4F67(doc, xNode, "origin", NULL, commIdText, 13) == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		if (node->defaultName[0] != '\0') {
			ret = appendXmlEscapedElement(doc, xNode, "defaultName", node->defaultName);
			if (ret < 0)
				break;
		}

		if (node->localName[0] != '\0' && sceClibStrnlen(node->localName, 0x7FFFFFFFu) != 0) {
			ret = appendXmlEscapedElement(doc, xNode, "localName", node->localName);
			if (ret < 0)
				break;

			char langBuf[8];
			SceSize langLen = 0;
			ret = npRtFormatUInt64(node->localLang, langBuf, 5, 10, &langLen);
			if (ret < 0)
				break;
			if (ScebXCe_AC0E4F67(doc, xNode, "localLang", NULL, langBuf, langLen) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (node->dataPtr != NULL && node->dataSize != 0) {
			ScebXCe_B367042F(attrs);

			if (node->mimeType != NULL &&
			    ScebXCe_3DD761CF(attrs, "type", node->mimeType) != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}

			if (node->valid != 0 && node->valid != -1) {
				const char *inlineStr = inlineFormatToString(node->valid);
				if (inlineStr != NULL &&
				    ScebXCe_3DD761CF(attrs, "inline", inlineStr) != 0) {
					ret = NP_RT_CORE_ERR_XML_ATTR;
					break;
				}
			}

			if (node->declaredSize != 0) {
				ret = npRtAttrListAddUInt64(attrs, "size", node->declaredSize);
				if (ret < 0)
					break;
			}

			if (node->dataName != NULL &&
			    ScebXCe_3DD761CF(attrs, "name", node->dataName) != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}

			if (node->valid == NP_RT_INLINE_BASE64) {
				SceSize encodedCap = 4u * ((node->dataSize + 2u) / 3u) + 1u;
				char *encodedBuf = reinterpret_cast<char *>(
					npRtHeapAlloc(encodedCap, NULL));
				if (encodedBuf == NULL) {
					ret = NP_RT_CORE_ERR_NO_MEMORY;
					break;
				}
				sceClibMemset(encodedBuf, 0, encodedCap);
				int encLen = static_cast<int>(sceNpCommonBase64Encode(
					reinterpret_cast<const SceUInt8 *>(node->dataPtr),
					encodedBuf, node->dataSize));
				if (encLen < 0) {
					npRtHeapFree(encodedBuf, NULL);
					ret = encLen;
					break;
				}
				encodedBuf[encLen] = '\0';
				BxceNodeId dataNode = ScebXCe_AC0E4F67(
					doc, xNode, "data", attrs, encodedBuf, encodedCap);
				npRtHeapFree(encodedBuf, NULL);
				if (dataNode == 0) {
					ret = NP_RT_CORE_ERR_XML_ATTR;
					break;
				}
			} else {
				if (ScebXCe_AC0E4F67(doc, xNode, "data", attrs,
						     reinterpret_cast<const char *>(node->dataPtr),
						     node->dataSize) == 0) {
					ret = NP_RT_CORE_ERR_XML_ATTR;
					break;
				}
			}
		}

		ScebXCe_B367042F(attrs);
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x81406AFE */
static int parseExtDataElement(NpImpsCoreDataNode **listHead, BxceNodeId elemNode, BxceDocument *doc,
			       void *customAlloc, SceUInt8 flags)
{
	if (listHead == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	const char *contextStr = ScebXCe_753D9382(doc, elemNode, "context");
	const char *activeStr = ScebXCe_753D9382(doc, elemNode, "active");

	const char *originStr = NULL;
	const char *defaultNameStr = NULL;
	const char *localNameStr = NULL;
	const char *localLangStr = NULL;
	npRtGetElementText(doc, elemNode, "origin", &originStr);
	npRtGetElementText(doc, elemNode, "defaultName", &defaultNameStr);
	npRtGetElementText(doc, elemNode, "localName", &localNameStr);
	npRtGetElementText(doc, elemNode, "localLang", &localLangStr);

	SceUInt8 parsedLocalLang = 0;
	if (localLangStr != NULL)
		parsedLocalLang = static_cast<SceUInt8>(SceNpCommon_5B770B80(localLangStr, NULL, 10));

	const char *mimeTypeStr = NULL;
	const char *dataNameStr = NULL;
	const char *dataText = NULL;
	SceSize dataTextLen = 0;
	int parsedInline = NP_RT_INLINE_NONE;
	SceUInt32 parsedDeclaredSize = 0;

	BxceNodeList *dataList = ScebXCe_1FA24DC0(doc, elemNode, "data");
	if (dataList != NULL) {
		if (dataList->length != 0) {
			BxceNodeId dataElem = ScebXCe_6C46D786(dataList, 0);
			if (dataElem != 0) {
				BxceNodeId textChild = ScebXCe_FEA6E4A1(doc, dataElem);
				if (textChild != 0) {
					mimeTypeStr = ScebXCe_753D9382(doc, dataElem, "type");
					dataNameStr = ScebXCe_753D9382(doc, dataElem, "name");
					const char *inlineStr = ScebXCe_753D9382(doc, dataElem, "inline");
					if (inlineStr != NULL)
						parsedInline = lookupValueByName(kInlineFormatTable, inlineStr);
					const char *sizeStr = ScebXCe_753D9382(doc, dataElem, "size");
					if (sizeStr != NULL)
						parsedDeclaredSize = SceNpCommon_5B770B80(sizeStr, NULL, 10);
					dataText = ScebXCe_33825EFF(doc, textChild);
					dataTextLen = sceClibStrnlen(dataText, 0x7FFFFFFFu);
				}
			}
		}
		ScebXCe_1F59F0C4(dataList);
		ScebXCe_EC560647(dataList);
	}

	NpImpsCoreDataNode *node = reinterpret_cast<NpImpsCoreDataNode *>(
		npRtHeapAlloc(sizeof(NpImpsCoreDataNode), customAlloc));
	if (node == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(NpImpsCoreDataNode));
	node->valid = parsedInline;

	int ret = 0;
	if (parsedInline == NP_RT_INLINE_BASE64) {
		SceSize decodedCap = (dataTextLen & ~3u) - (dataTextLen >> 2);
		void *decodedBuf = npRtHeapAlloc(decodedCap, customAlloc);
		node->dataPtr = decodedBuf;
		if (decodedBuf == NULL) {
			freeExtDataList(node, customAlloc, flags);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(decodedBuf, 0, decodedCap);
		ret = SceNpCommon_FB1F70F3(dataText, dataTextLen,
					   reinterpret_cast<SceUInt8 *>(decodedBuf), decodedCap);
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
		node->dataSize = static_cast<SceSize>(ret);
	} else {
		char *dupPayload = NULL;
		ret = npRtDupString(&dupPayload, dataText, customAlloc, flags);
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
		node->dataPtr = dupPayload;
		node->dataSize = dataTextLen;
	}

	node->next = NULL;
	ret = npRtDupString(&node->mimeType, mimeTypeStr, customAlloc, flags);
	if (ret < 0) {
		freeExtDataList(node, customAlloc, flags);
		return ret;
	}

	node->declaredSize = parsedDeclaredSize;
	ret = npRtDupString(&node->dataName, dataNameStr, customAlloc, flags);
	if (ret < 0) {
		freeExtDataList(node, customAlloc, flags);
		return ret;
	}

	node->localLang = parsedLocalLang;
	if (contextStr != NULL) {
		ret = SceNpCommon_5BC54899(&node->commId, contextStr);
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
	}
	if (originStr != NULL) {
		ret = SceNpCommon_5BC54899(&node->targetCommId, originStr);
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
	}
	if (defaultNameStr != NULL) {
		ret = npRtXmlUnescapeToBuf(defaultNameStr, node->defaultName, sizeof(node->defaultName));
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
	}
	if (localNameStr != NULL) {
		ret = npRtXmlUnescapeToBuf(localNameStr, node->localName, sizeof(node->localName));
		if (ret < 0) {
			freeExtDataList(node, customAlloc, flags);
			return ret;
		}
	}

	if (activeStr == NULL) {
		node->activeState = 0;
	} else if (sceClibStrncmp(activeStr, "0", 2u) == 0) {
		node->activeState = 1;
	} else if (sceClibStrncmp(activeStr, "1", 2u) == 0) {
		node->activeState = 2;
	} else {
		node->activeState = -1;
	}

	NpImpsCoreDataNode **tail = listHead;
	while (*tail != NULL)
		tail = &(*tail)->next;
	*tail = node;
	return 0;
}

/* 0x81406ED8 */
static int cloneExtDataList(const NpImpsCoreDataNode *srcHead, NpImpsCoreDataNode **outHead,
			    void *customAlloc, SceUInt8 flags)
{
	if (srcHead == NULL) {
		*outHead = NULL;
		return 0;
	}

	NpImpsCoreDataNode *copyHead = NULL;
	NpImpsCoreDataNode *copyTail = NULL;

	for (const NpImpsCoreDataNode *src = srcHead; src != NULL; src = src->next) {
		NpImpsCoreDataNode *node = reinterpret_cast<NpImpsCoreDataNode *>(
			npRtHeapAlloc(sizeof(NpImpsCoreDataNode), customAlloc));
		if (node == NULL) {
			freeExtDataList(copyHead, customAlloc, flags);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(node, 0, sizeof(NpImpsCoreDataNode));

		if (copyTail != NULL)
			copyTail->next = node;
		else
			copyHead = node;
		copyTail = node;

		int ret = npRtDupString(&node->mimeType, src->mimeType, customAlloc, flags);
		if (ret < 0) {
			freeExtDataList(copyHead, customAlloc, flags);
			return ret;
		}

		node->valid = src->valid;
		node->dataSize = src->dataSize;
		node->declaredSize = src->declaredSize;

		if (src->valid == NP_RT_INLINE_BASE64) {
			if (src->dataPtr != NULL) {
				void *buf = npRtHeapAlloc(src->dataSize, customAlloc);
				node->dataPtr = buf;
				if (buf == NULL) {
					freeExtDataList(copyHead, customAlloc, flags);
					return NP_RT_CORE_ERR_NO_MEMORY;
				}
				sceClibMemcpy_safe(buf, src->dataPtr, src->dataSize);
			}
		} else {
			char *dupPayload = NULL;
			ret = npRtDupString(&dupPayload, reinterpret_cast<const char *>(src->dataPtr),
					    customAlloc, flags);
			if (ret < 0) {
				freeExtDataList(copyHead, customAlloc, flags);
				return ret;
			}
			node->dataPtr = dupPayload;
		}

		ret = npRtDupString(&node->dataName, src->dataName, customAlloc, flags);
		if (ret < 0) {
			freeExtDataList(copyHead, customAlloc, flags);
			return ret;
		}

		sceClibMemcpy_safe(&node->commId, &src->commId, sizeof(SceNpCommunicationId));
		sceClibMemcpy_safe(&node->targetCommId, &src->targetCommId, sizeof(SceNpCommunicationId));
		sceClibMemcpy_safe(node->defaultName, src->defaultName, sizeof(node->defaultName));
		sceClibMemcpy_safe(node->localName, src->localName, sizeof(node->localName));
		node->localLang = src->localLang;
		node->activeState = src->activeState;
	}

	*outHead = copyHead;
	return 0;
}

/* --------------------------------------------------------------------------
 * Stored extension ("http://playstation.net/np/rt/stored")
 * 0x814073C0..0x8140762B
 * -------------------------------------------------------------------------- */

/* 0x81407474 */
static void freeStoredExt(NpRtStoredExt *stored, void *customAlloc)
{
	if (stored == NULL)
		return;
	freeTickOrSession(stored->stampTick, customAlloc);
	freeTickOrSession(stored->expireTick, customAlloc);
	npRtHeapFree(stored, customAlloc);
}

/* 0x814073C0 */
static int serializeStoredExt(BxceDocument *doc, const NpRtStoredExt *stored)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	int ret = 0;
	if (ScebXCe_E7C8B20D(attrs) != 0) {
		ret = NP_RT_CORE_ERR_XML_ATTR;
	} else if (ScebXCe_3DD761CF(attrs, "xmlns",
				    "http://playstation.net/np/rt/stored") != 0) {
		ret = NP_RT_CORE_ERR_NOT_FOUND;
	} else {
		if (stored->ttl != 0)
			ret = npRtAttrListAddUInt64(attrs, "ttl", stored->ttl);
		if (ret >= 0) {
			BxceNodeId root = ScebXCe_75E1B095(doc);
			if (ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0) != 0)
				return 0;
			ret = NP_RT_CORE_ERR_NOT_FOUND;
		}
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x81407496 */
static int parseStoredExt(NpRtStoredExt **outStored, BxceNodeId elemNode, BxceDocument *doc,
			  void *customAlloc)
{
	NpRtStoredExt *stored = reinterpret_cast<NpRtStoredExt *>(
		npRtHeapAlloc(sizeof(NpRtStoredExt), customAlloc));
	if (stored == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(stored, 0, sizeof(NpRtStoredExt));

	const char *idStr = ScebXCe_753D9382(doc, elemNode, "id");
	if (idStr != NULL)
		stored->storedId = static_cast<SceUInt64>(SceNpCommon_E1FF4E70(idStr));

	int ret = parseDelayStamp(&stored->stampTick, elemNode, doc, customAlloc);
	if (ret < 0) {
		freeStoredExt(stored, customAlloc);
		return ret;
	}

	const char *expireStr = ScebXCe_753D9382(doc, elemNode, "expire");
	if (expireStr != NULL) {
		SceRtcTick utc;
		ret = sceRtcParseDateTime(&utc, expireStr);
		if (ret < 0) {
			freeStoredExt(stored, customAlloc);
			return ret;
		}
		SceRtcTick *expCopy = reinterpret_cast<SceRtcTick *>(
			npRtHeapAlloc(sizeof(SceRtcTick), customAlloc));
		if (expCopy == NULL) {
			freeStoredExt(stored, customAlloc);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemcpy_safe(expCopy, &utc, sizeof(SceRtcTick));
		stored->expireTick = expCopy;
	} else {
		stored->expireTick = NULL;
	}

	stored->delayed = 0;
	const char *delayStr = ScebXCe_753D9382(doc, elemNode, "delay");
	if (delayStr != NULL && SceNpCommon_CC8E6AC6(delayStr) == 1)
		stored->delayed = 1;

	*outStored = stored;
	return 0;
}

/* 0x814075A4 */
static int cloneStoredExt(const NpRtStoredExt *src, NpRtStoredExt **outCopy, void *customAlloc)
{
	if (src == NULL) {
		*outCopy = NULL;
		return 0;
	}

	NpRtStoredExt *copy = reinterpret_cast<NpRtStoredExt *>(
		npRtHeapAlloc(sizeof(NpRtStoredExt), customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, sizeof(NpRtStoredExt));
	copy->storedId = src->storedId;

	int ret = cloneTick(src->stampTick, &copy->stampTick, customAlloc);
	if (ret < 0) {
		freeStoredExt(copy, customAlloc);
		return ret;
	}
	ret = cloneTick(src->expireTick, &copy->expireTick, customAlloc);
	if (ret < 0) {
		freeStoredExt(copy, customAlloc);
		return ret;
	}

	copy->delayed = src->delayed;
	copy->ttl = src->ttl;
	*outCopy = copy;
	return 0;
}

/* --------------------------------------------------------------------------
 * Antiblock extension ("http://playstation.net/np/rt/antiblock")
 * 0x814076E4..0x8140777F
 * -------------------------------------------------------------------------- */

/* 0x814076E4 */
static int serializeAntiblockExt(BxceDocument *doc)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	if (ScebXCe_3DD761CF(attrs, "xmlns",
			     "http://playstation.net/np/rt/antiblock") != 0) {
		ret = NP_RT_CORE_ERR_XML_ATTR;
	} else {
		BxceNodeId root = ScebXCe_75E1B095(doc);
		if (ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0) == 0)
			ret = NP_RT_CORE_ERR_XML_ATTR;
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* --------------------------------------------------------------------------
 * Entity capabilities extension ("http://jabber.org/protocol/caps", <c>)
 * 0x81407780..0x81407AFF
 * -------------------------------------------------------------------------- */

/* 0x814078AC */
static void freeCapsList(NpRtCapsNode *head, void *customAlloc, SceUInt8 flags)
{
	NpRtCapsNode *cur = head;
	while (cur != NULL) {
		NpRtCapsNode *next = cur->next;
		npRtFreeString(cur->node, customAlloc, flags);
		npRtFreeString(cur->ver, customAlloc, flags);
		npRtFreeString(cur->ext, customAlloc, flags);
		npRtHeapFree(cur, customAlloc);
		cur = next;
	}
}

/* 0x81407780 */
static int serializeCapsList(BxceDocument *doc, const NpRtCapsNode *head)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_INTERNAL;
	}

	int ret = 0;
	for (const NpRtCapsNode *cur = head; cur != NULL; cur = cur->next) {
		if (cur->node == NULL || cur->ver == NULL)
			continue;

		if (ScebXCe_3DD761CF(attrs, "xmlns", "http://jabber.org/protocol/caps") != 0 ||
		    ScebXCe_3DD761CF(attrs, "node", cur->node) != 0 ||
		    ScebXCe_3DD761CF(attrs, "ver", cur->ver) != 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}
		if (cur->ext != NULL && ScebXCe_3DD761CF(attrs, "ext", cur->ext) != 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		BxceNodeId root = ScebXCe_75E1B095(doc);
		if (ScebXCe_AC0E4F67(doc, root, "c", attrs, NULL, 0) == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}
		ScebXCe_B367042F(attrs);
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x814078E8 */
static int parseCapsList(NpRtCapsNode **outHead, BxceNodeList *nodeList, BxceDocument *doc,
			 void *customAlloc, SceUInt8 flags)
{
	NpRtCapsNode *head = NULL;
	NpRtCapsNode *tail = NULL;

	for (SceUInt32 idx = 0; idx < static_cast<SceUInt32>(nodeList->length); ++idx) {
		BxceNodeId elem = ScebXCe_6C46D786(nodeList, idx);
		if (elem == 0)
			continue;

		const char *xmlns = ScebXCe_753D9382(doc, elem, "xmlns");
		if (xmlns == NULL ||
		    sceClibStrncmp(xmlns, "http://jabber.org/protocol/caps", 32u) != 0)
			continue;

		NpRtCapsNode *caps = reinterpret_cast<NpRtCapsNode *>(
			npRtHeapAlloc(sizeof(NpRtCapsNode), customAlloc));
		if (caps == NULL) {
			freeCapsList(head, customAlloc, flags);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(caps, 0, sizeof(NpRtCapsNode));

		if (tail != NULL)
			tail->next = caps;
		else
			head = caps;
		tail = caps;

		int ret = npRtDupString(&caps->node, ScebXCe_753D9382(doc, elem, "node"),
					customAlloc, flags);
		if (ret < 0) {
			freeCapsList(head, customAlloc, flags);
			return ret;
		}
		ret = npRtDupString(&caps->ver, ScebXCe_753D9382(doc, elem, "ver"),
				    customAlloc, flags);
		if (ret < 0) {
			freeCapsList(head, customAlloc, flags);
			return ret;
		}
		ret = npRtDupString(&caps->ext, ScebXCe_753D9382(doc, elem, "ext"),
				    customAlloc, flags);
		if (ret < 0) {
			freeCapsList(head, customAlloc, flags);
			return ret;
		}
	}

	*outHead = head;
	return 0;
}

/* 0x81407A2E */
static int cloneCapsList(const NpRtCapsNode *srcHead, NpRtCapsNode **outHead,
			 void *customAlloc, SceUInt8 flags)
{
	if (srcHead == NULL) {
		*outHead = NULL;
		return 0;
	}

	NpRtCapsNode *copyHead = NULL;
	NpRtCapsNode *copyTail = NULL;

	for (const NpRtCapsNode *src = srcHead; src != NULL; src = src->next) {
		NpRtCapsNode *caps = reinterpret_cast<NpRtCapsNode *>(
			npRtHeapAlloc(sizeof(NpRtCapsNode), customAlloc));
		if (caps == NULL) {
			freeCapsList(copyHead, customAlloc, flags);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(caps, 0, sizeof(NpRtCapsNode));

		if (copyTail != NULL)
			copyTail->next = caps;
		else
			copyHead = caps;
		copyTail = caps;

		int ret = npRtDupString(&caps->node, src->node, customAlloc, flags);
		if (ret < 0) {
			freeCapsList(copyHead, customAlloc, flags);
			return ret;
		}
		ret = npRtDupString(&caps->ver, src->ver, customAlloc, flags);
		if (ret < 0) {
			freeCapsList(copyHead, customAlloc, flags);
			return ret;
		}
		ret = npRtDupString(&caps->ext, src->ext, customAlloc, flags);
		if (ret < 0) {
			freeCapsList(copyHead, customAlloc, flags);
			return ret;
		}
	}

	*outHead = copyHead;
	return 0;
}

/* --------------------------------------------------------------------------
 * Content extension ("http://playstation.net/np/rt/content")
 * 0x81407B00..0x814084A3
 * -------------------------------------------------------------------------- */

static void freeContentDataNode(NpRtContentDataNode *dataNode, void *customAlloc, SceUInt8 flags)
{
	if (dataNode == NULL)
		return;
	if (dataNode->inlineFormat == NP_RT_INLINE_BASE64) {
		npRtHeapFree(dataNode->dataPtr, customAlloc);
		dataNode->dataPtr = NULL;
	} else {
		npRtFreeString(dataNode->dataPtr, customAlloc, flags);
	}
	npRtFreeString(dataNode->mimeType, customAlloc, flags);
	npRtFreeString(dataNode->dataName, customAlloc, flags);
}

/* 0x81407E60 */
static void freeContentExt(NpRtContentExt *content, void *customAlloc, SceUInt8 flags)
{
	if (content == NULL)
		return;

	if (content->headerData != NULL) {
		npRtHeapFree(content->headerData, customAlloc);
		content->headerData = NULL;
	}

	NpRtContentDataNode *cur = content->dataList;
	while (cur != NULL) {
		NpRtContentDataNode *next = cur->next;
		freeContentDataNode(cur, customAlloc, flags);
		npRtHeapFree(cur, customAlloc);
		cur = next;
	}
	npRtHeapFree(content, customAlloc);
}

/* 0x81407B00 */
static int serializeContentExtList(BxceDocument *doc, const NpRtContentExt *head)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_INTERNAL;
	}

	int ret = 0;
	for (const NpRtContentExt *cur = head; cur != NULL && ret >= 0; cur = cur->next) {
		if (ScebXCe_3DD761CF(attrs, "xmlns",
				     "http://playstation.net/np/rt/content") != 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		if (cur->contextId.data[0] != '\0') {
			char ctxBuf[16];
			ret = SceNpCommon_E426FEE8(&cur->contextId, ctxBuf, 13);
			if (ret < 0)
				break;
			if (ScebXCe_3DD761CF(attrs, "context", ctxBuf) != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		BxceNodeId root = ScebXCe_75E1B095(doc);
		BxceNodeId xNode = ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0);
		if (xNode == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}
		ScebXCe_B367042F(attrs);

		if (cur->headerData == NULL || cur->headerDataSize == 0) {
			ret = NP_RT_CORE_ERR_INVALID_ARG;
			break;
		}

		SceSize hdrEncCap = 4u * ((cur->headerDataSize + 2u) / 3u) + 1u;
		char *hdrEncBuf = reinterpret_cast<char *>(npRtHeapAlloc(hdrEncCap, NULL));
		if (hdrEncBuf == NULL) {
			ret = NP_RT_CORE_ERR_NO_MEMORY;
			break;
		}
		sceClibMemset(hdrEncBuf, 0, hdrEncCap);
		int hdrEncLen = static_cast<int>(sceNpCommonBase64Encode(
			reinterpret_cast<const SceUInt8 *>(cur->headerData),
			hdrEncBuf, cur->headerDataSize));
		if (hdrEncLen < 0) {
			npRtHeapFree(hdrEncBuf, NULL);
			ret = hdrEncLen;
			break;
		}
		hdrEncBuf[hdrEncLen] = '\0';

		ret = npRtAttrListAddUInt64(attrs, "version", cur->headerVersion);
		if (ret < 0) {
			npRtHeapFree(hdrEncBuf, NULL);
			break;
		}

		BxceNodeId hdrNode = ScebXCe_AC0E4F67(doc, xNode, "header", attrs,
						      hdrEncBuf, hdrEncCap);
		npRtHeapFree(hdrEncBuf, NULL);
		if (hdrNode == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}
		ScebXCe_B367042F(attrs);

		for (const NpRtContentDataNode *data = cur->dataList;
		     data != NULL && ret >= 0; data = data->next) {
			if (data->dataPtr != NULL && data->dataSize != 0) {
				ScebXCe_B367042F(attrs);

				if (data->mimeType != NULL &&
				    ScebXCe_3DD761CF(attrs, "type", data->mimeType) != 0) {
					ret = NP_RT_CORE_ERR_XML_ATTR;
					break;
				}
				if (data->inlineFormat != 0 && data->inlineFormat != -1) {
					const char *inlineStr = inlineFormatToString(data->inlineFormat);
					if (inlineStr != NULL &&
					    ScebXCe_3DD761CF(attrs, "inline", inlineStr) != 0) {
						ret = NP_RT_CORE_ERR_XML_ATTR;
						break;
					}
				}
				if (data->declaredSize != 0) {
					ret = npRtAttrListAddUInt64(attrs, "size", data->declaredSize);
					if (ret < 0)
						break;
				}
				if (data->dataName != NULL &&
				    ScebXCe_3DD761CF(attrs, "name", data->dataName) != 0) {
					ret = NP_RT_CORE_ERR_XML_ATTR;
					break;
				}

				if (data->inlineFormat == NP_RT_INLINE_BASE64) {
					SceSize encCap = 4u * ((data->dataSize + 2u) / 3u) + 1u;
					char *encBuf = reinterpret_cast<char *>(
						npRtHeapAlloc(encCap, NULL));
					if (encBuf == NULL) {
						ret = NP_RT_CORE_ERR_NO_MEMORY;
						break;
					}
					sceClibMemset(encBuf, 0, encCap);
					int encLen = static_cast<int>(sceNpCommonBase64Encode(
						reinterpret_cast<const SceUInt8 *>(data->dataPtr),
						encBuf, data->dataSize));
					if (encLen < 0) {
						ret = encLen;
						break;
					}
					encBuf[encLen] = '\0';
					BxceNodeId dataNode = ScebXCe_AC0E4F67(
						doc, xNode, "data", attrs, encBuf, encCap);
					if (dataNode == 0) {
						ret = NP_RT_CORE_ERR_XML_ATTR;
						break;
					}
					npRtHeapFree(encBuf, NULL);
				} else {
					if (ScebXCe_AC0E4F67(
						    doc, xNode, "data", attrs,
						    reinterpret_cast<const char *>(data->dataPtr),
						    data->dataSize) == 0) {
						ret = NP_RT_CORE_ERR_XML_ATTR;
						break;
					}
				}
			}
			ScebXCe_B367042F(attrs);
		}
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x81407ECA */
static int parseContentExtElement(NpRtContentExt **listHead, BxceNodeId elemNode, BxceDocument *doc,
				  void *customAlloc, SceUInt8 flags)
{
	const char *contextStr = ScebXCe_753D9382(doc, elemNode, "context");
	BxceNodeList *headerList = ScebXCe_1FA24DC0(doc, elemNode, "header");
	if (headerList == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	const char *headerText = NULL;
	const char *versionStr = NULL;
	if (headerList->length != 0) {
		BxceNodeId hdrElem = ScebXCe_6C46D786(headerList, 0);
		if (hdrElem != 0) {
			BxceNodeId textChild = ScebXCe_FEA6E4A1(doc, hdrElem);
			if (textChild != 0)
				headerText = ScebXCe_33825EFF(doc, textChild);
			versionStr = ScebXCe_753D9382(doc, hdrElem, "version");
		}
	}
	ScebXCe_1F59F0C4(headerList);
	ScebXCe_EC560647(headerList);

	if (headerText == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	NpRtContentExt *content = reinterpret_cast<NpRtContentExt *>(
		npRtHeapAlloc(sizeof(NpRtContentExt), customAlloc));
	if (content == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(content, 0, sizeof(NpRtContentExt));

	if (contextStr != NULL) {
		int ret = SceNpCommon_5BC54899(&content->contextId, contextStr);
		if (ret < 0) {
			freeContentExt(content, customAlloc, flags);
			return ret;
		}
	}

	SceSize hdrTextLen = sceClibStrnlen(headerText, 0x7FFFFFFFu);
	SceSize hdrDecCap = (hdrTextLen & ~3u) - (hdrTextLen >> 2);
	void *hdrBuf = npRtHeapAlloc(hdrDecCap, customAlloc);
	if (hdrBuf == NULL) {
		freeContentExt(content, customAlloc, flags);
		return NP_RT_CORE_ERR_NO_MEMORY;
	}
	sceClibMemset(hdrBuf, 0, hdrDecCap);
	int hdrDecLen = SceNpCommon_FB1F70F3(headerText, hdrTextLen,
					     reinterpret_cast<SceUInt8 *>(hdrBuf), hdrDecCap);
	if (hdrDecLen < 0) {
		npRtHeapFree(hdrBuf, customAlloc);
		freeContentExt(content, customAlloc, flags);
		return hdrDecLen;
	}

	content->headerData = hdrBuf;
	content->headerDataSize = static_cast<SceSize>(hdrDecLen);
	content->headerVersion = (versionStr != NULL) ? SceNpCommon_5B770B80(versionStr, NULL, 10) : 0u;

	SceUInt32 parsedSize = 0;
	BxceNodeList *dataList = ScebXCe_1FA24DC0(doc, elemNode, "data");
	if (dataList != NULL) {
		int ret = 0;
		for (SceUInt32 idx = 0; idx < static_cast<SceUInt32>(dataList->length) && ret >= 0; ++idx) {
			BxceNodeId dataElem = ScebXCe_6C46D786(dataList, idx);
			if (dataElem == 0)
				continue;
			BxceNodeId textChild = ScebXCe_FEA6E4A1(doc, dataElem);
			if (textChild == 0)
				continue;

			const char *mimeStr = ScebXCe_753D9382(doc, dataElem, "type");
			const char *nameStr = ScebXCe_753D9382(doc, dataElem, "name");
			const char *inlineStr = ScebXCe_753D9382(doc, dataElem, "inline");
			int inlineFmt = (inlineStr != NULL) ? parseInlineFormat(inlineStr) : 0;
			const char *sizeStr = ScebXCe_753D9382(doc, dataElem, "size");
			if (sizeStr != NULL)
				parsedSize = SceNpCommon_5B770B80(sizeStr, NULL, 10);

			const char *payloadText = ScebXCe_33825EFF(doc, textChild);
			SceSize payloadLen = sceClibStrnlen(payloadText, 0x7FFFFFFFu);

			NpRtContentDataNode *dataNode = reinterpret_cast<NpRtContentDataNode *>(
				npRtHeapAlloc(sizeof(NpRtContentDataNode), customAlloc));
			if (dataNode == NULL) {
				ret = NP_RT_CORE_ERR_NO_MEMORY;
				break;
			}
			sceClibMemset(dataNode, 0, sizeof(NpRtContentDataNode));
			dataNode->inlineFormat = inlineFmt;

			if (inlineFmt == NP_RT_INLINE_BASE64) {
				SceSize decCap = (payloadLen & ~3u) - (payloadLen >> 2);
				void *decBuf = npRtHeapAlloc(decCap, customAlloc);
				if (decBuf == NULL) {
					freeContentDataNode(dataNode, customAlloc, flags);
					ret = NP_RT_CORE_ERR_NO_MEMORY;
					break;
				}
				sceClibMemset(decBuf, 0, decCap);
				int decLen = SceNpCommon_FB1F70F3(
					payloadText, payloadLen,
					reinterpret_cast<SceUInt8 *>(decBuf), decCap);
				if (decLen < 0) {
					npRtHeapFree(decBuf, customAlloc);
					freeContentDataNode(dataNode, customAlloc, flags);
					ret = decLen;
					break;
				}
				dataNode->dataPtr = decBuf;
				dataNode->dataSize = static_cast<SceSize>(decLen);
			} else {
				char *dupPayload = NULL;
				ret = npRtDupString(&dupPayload, payloadText, customAlloc, flags);
				if (ret < 0) {
					freeContentDataNode(dataNode, customAlloc, flags);
					break;
				}
				dataNode->dataPtr = dupPayload;
				dataNode->dataSize = payloadLen;
			}

			dataNode->next = NULL;
			ret = npRtDupString(&dataNode->mimeType, mimeStr, customAlloc, flags);
			if (ret < 0) {
				freeContentDataNode(dataNode, customAlloc, flags);
				break;
			}
			dataNode->declaredSize = parsedSize;
			ret = npRtDupString(&dataNode->dataName, nameStr, customAlloc, flags);
			if (ret < 0) {
				freeContentDataNode(dataNode, customAlloc, flags);
				break;
			}

			NpRtContentDataNode **dataTail = &content->dataList;
			while (*dataTail != NULL)
				dataTail = &(*dataTail)->next;
			*dataTail = dataNode;
		}
		ScebXCe_1F59F0C4(dataList);
		ScebXCe_EC560647(dataList);
		if (ret < 0) {
			freeContentExt(content, customAlloc, flags);
			return ret;
		}
	}

	NpRtContentExt **tail = listHead;
	while (*tail != NULL)
		tail = &(*tail)->next;
	*tail = content;
	return 0;
}

/* 0x814082CC */
static int cloneContentExtList(const NpRtContentExt *srcHead, NpRtContentExt **outHead,
			       void *customAlloc, SceUInt8 flags)
{
	if (srcHead == NULL) {
		*outHead = NULL;
		return 0;
	}

	NpRtContentExt *copyHead = NULL;
	NpRtContentExt *copyTail = NULL;

	for (const NpRtContentExt *src = srcHead; src != NULL; src = src->next) {
		NpRtContentExt *content = reinterpret_cast<NpRtContentExt *>(
			npRtHeapAlloc(sizeof(NpRtContentExt), customAlloc));
		if (content == NULL) {
			freeContentExt(copyHead, customAlloc, flags);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(content, 0, sizeof(NpRtContentExt));

		if (copyTail != NULL)
			copyTail->next = content;
		else
			copyHead = content;
		copyTail = content;

		sceClibMemcpy_safe(&content->contextId, &src->contextId, sizeof(SceNpCommunicationId));
		content->headerVersion = src->headerVersion;
		if (src->headerDataSize != 0) {
			void *hdrBuf = npRtHeapAlloc(src->headerDataSize, customAlloc);
			content->headerData = hdrBuf;
			if (hdrBuf == NULL) {
				freeContentExt(copyHead, customAlloc, flags);
				return NP_RT_CORE_ERR_NO_MEMORY;
			}
			sceClibMemcpy_safe(hdrBuf, src->headerData, src->headerDataSize);
			content->headerDataSize = src->headerDataSize;
		}

		NpRtContentDataNode *dataTail = NULL;
		for (const NpRtContentDataNode *srcData = src->dataList;
		     srcData != NULL; srcData = srcData->next) {
			NpRtContentDataNode *dataNode = reinterpret_cast<NpRtContentDataNode *>(
				npRtHeapAlloc(sizeof(NpRtContentDataNode), customAlloc));
			if (dataNode == NULL) {
				freeContentExt(copyHead, customAlloc, flags);
				return NP_RT_CORE_ERR_NO_MEMORY;
			}
			sceClibMemset(dataNode, 0, sizeof(NpRtContentDataNode));

			if (dataTail != NULL)
				dataTail->next = dataNode;
			else
				content->dataList = dataNode;
			dataTail = dataNode;

			dataNode->inlineFormat = srcData->inlineFormat;
			dataNode->dataSize = srcData->dataSize;
			dataNode->declaredSize = srcData->declaredSize;

			if (srcData->inlineFormat == NP_RT_INLINE_BASE64) {
				if (srcData->dataPtr != NULL) {
					void *buf = npRtHeapAlloc(srcData->dataSize, customAlloc);
					dataNode->dataPtr = buf;
					if (buf == NULL) {
						freeContentExt(copyHead, customAlloc, flags);
						return NP_RT_CORE_ERR_NO_MEMORY;
					}
					sceClibMemcpy_safe(buf, srcData->dataPtr, srcData->dataSize);
				}
			} else {
				char *dupPayload = NULL;
				int ret = npRtDupString(
					&dupPayload,
					reinterpret_cast<const char *>(srcData->dataPtr),
					customAlloc, flags);
				if (ret < 0) {
					freeContentExt(copyHead, customAlloc, flags);
					return ret;
				}
				dataNode->dataPtr = dupPayload;
			}

			int ret = npRtDupString(&dataNode->dataName, srcData->dataName,
						customAlloc, flags);
			if (ret < 0) {
				freeContentExt(copyHead, customAlloc, flags);
				return ret;
			}
		}
	}

	*outHead = copyHead;
	return 0;
}

/* --------------------------------------------------------------------------
 * Message metadata extension ("http://playstation.net/np/rt/message")
 * 0x814084A4..0x814085FB
 * -------------------------------------------------------------------------- */

/* 0x81408578 */
static void freeMessageMetaExt(NpRtMessageMetaExt *meta, void *customAlloc)
{
	if (meta != NULL)
		npRtHeapFree(meta, customAlloc);
}

/* 0x814084A4 */
static int serializeMessageMetaExt(BxceDocument *doc, const NpRtMessageMetaExt *meta)
{
	if (meta == NULL)
		return 0;

	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	if (ScebXCe_3DD761CF(attrs, "xmlns",
			     "http://playstation.net/np/rt/message") != 0) {
		ret = NP_RT_CORE_ERR_XML_ATTR;
	} else if (meta->messageKind[0] != '\0' &&
		   ScebXCe_3DD761CF(attrs, "message_kind", meta->messageKind) != 0) {
		ret = NP_RT_CORE_ERR_XML_ATTR;
	} else {
		BxceNodeId root = ScebXCe_75E1B095(doc);
		if (ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0) == 0)
			ret = NP_RT_CORE_ERR_XML_ATTR;
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x81408582 */
static int parseMessageMetaExt(NpRtMessageMetaExt **outMeta, BxceNodeId elemNode, BxceDocument *doc,
			       void *customAlloc)
{
	NpRtMessageMetaExt *meta = reinterpret_cast<NpRtMessageMetaExt *>(
		npRtHeapAlloc(sizeof(NpRtMessageMetaExt), customAlloc));
	if (meta == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(meta, 0, sizeof(NpRtMessageMetaExt));
	meta->convertType = -1;

	const char *kindStr = ScebXCe_753D9382(doc, elemNode, "message_kind");
	if (kindStr != NULL) {
		sceClibStrncpy(meta->messageKind, kindStr, 4u);
		meta->messageKind[3] = '\0';
	}

	const char *convStr = ScebXCe_753D9382(doc, elemNode, "convert_type");
	if (convStr != NULL)
		meta->convertType = SceNpCommon_CC8E6AC6(convStr);

	if (*outMeta != NULL)
		*outMeta = meta;
	return 0;
}

/* --------------------------------------------------------------------------
 * Presence title extension ("http://playstation.net/np/rt/presence")
 * 0x814085FC..0x81409033
 * -------------------------------------------------------------------------- */

/* 0x81408C72 */
static void freePresenceTitleList(NpImpsCoreTitleNode *head, void *customAlloc)
{
	NpImpsCoreTitleNode *cur = head;
	while (cur != NULL) {
		NpImpsCoreTitleNode *next = cur->next;
		npRtHeapFree(cur, customAlloc);
		cur = next;
	}
}

/* 0x814085FC */
static int serializePresenceTitleList(BxceDocument *doc, const NpImpsCoreTitleNode *head)
{
	if (head == NULL)
		return 0;

	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	for (const NpImpsCoreTitleNode *title = head; title != NULL && ret >= 0; title = title->next) {
		if (title->platform == 0)
			continue;

		const char *modeStr = lookupNameByValue(kPresenceModeTable, title->platform);
		if (modeStr == NULL)
			continue;

		if (ScebXCe_3DD761CF(attrs, "xmlns",
				     "http://playstation.net/np/rt/presence") != 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		if (title->updatePriority == 1) {
			if (ScebXCe_3DD761CF(attrs, "active", "0") != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		} else if (title->updatePriority == 2) {
			if (ScebXCe_3DD761CF(attrs, "active", "1") != 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		BxceNodeId root = ScebXCe_75E1B095(doc);
		BxceNodeId xNode = ScebXCe_AC0E4F67(doc, root, "x", attrs, NULL, 0);
		if (xNode == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		SceSize modeLen = sceClibStrnlen(modeStr, 0x7FFFFFFFu);
		if (ScebXCe_AC0E4F67(doc, xNode, "mode", NULL, modeStr, modeLen) == 0) {
			ret = NP_RT_CORE_ERR_XML_ATTR;
			break;
		}

		if (title->titleId.data[0] != '\0') {
			char objIdBuf[16];
			ret = SceNpCommon_E426FEE8(&title->titleId, objIdBuf, 13);
			if (ret < 0)
				break;
			if (ScebXCe_AC0E4F67(doc, xNode, "objectId", NULL, objIdBuf, 13) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (title->titleIdStr[0] != '\0') {
			ret = appendXmlEscapedElement(doc, xNode, "defaultName", title->titleIdStr);
			if (ret < 0)
				break;
		}

		if (title->titleName[0] != '\0' && sceClibStrnlen(title->titleName, 0x7FFFFFFFu) != 0) {
			ret = appendXmlEscapedElement(doc, xNode, "localName", title->titleName);
			if (ret < 0)
				break;

			char langBuf[8];
			SceSize langLen = 0;
			ret = npRtFormatUInt64(title->isSystemTitle, langBuf, 5, 10, &langLen);
			if (ret < 0)
				break;
			if (ScebXCe_AC0E4F67(doc, xNode, "localLang", NULL, langBuf, langLen) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (title->status[0] != '\0') {
			SceSize statusLen = sceClibStrnlen(title->status, 0x7FFFFFFFu);
			if (statusLen != 0) {
				SceSize escapedSize = 0;
				ret = SceNpCommon_C2BF5C1F(
					NULL, 0,
					reinterpret_cast<const SceUInt8 *>(title->status),
					statusLen, &escapedSize);
				if (ret < 0)
					break;

				/* Sony's residual debug hex-dump formatter into stack scratch */
				char dumpLine[128];
				const SceUInt8 *hexSrc = reinterpret_cast<const SceUInt8 *>(title->status);
				const SceUInt8 *asciiSrc = hexSrc;
				SceSize hexPos = 0;
				SceSize asciiPos = 0;
				while (hexPos < statusLen) {
					char *out = dumpLine;
					int lineDone = 0;
					while (!lineDone) {
						if (hexPos >= statusLen) {
							for (SceSize col = hexPos & 0xFu; col < 16u; ++col) {
								out[0] = ' ';
								out[1] = ' ';
								out += 2;
								if ((col & 1u) == 1u) {
									*out = ' ';
									++out;
								}
							}
							break;
						}
						SceUInt8 ch = *hexSrc++;
						sceClibSnprintf(out, sizeof(dumpLine), "%.2x", ch);
						out += 2;
						if ((hexPos & 1u) == 1u) {
							*out = ' ';
							++out;
						}
						if ((hexPos & 0xFu) == 0xFu)
							lineDone = 1;
						++hexPos;
					}
					*out++ = ' ';
					int asciiDone = 0;
					while (!asciiDone && asciiPos < statusLen) {
						SceUInt8 ch = *asciiSrc++;
						*out++ = (ch >= 0x20u && ch <= 0x7Eu) ? static_cast<char>(ch) : '.';
						if ((asciiPos & 0xFu) == 0xFu)
							asciiDone = 1;
						++asciiPos;
					}
					*out = '\0';
				}

				if (escapedSize - 1 == statusLen) {
					if (ScebXCe_AC0E4F67(doc, xNode, "gameSpecified", NULL,
							     title->status, statusLen) == 0) {
						ret = NP_RT_CORE_ERR_XML_ATTR;
						break;
					}
				} else {
					char *escapedBuf = reinterpret_cast<char *>(
						npRtHeapAlloc(escapedSize, NULL));
					ret = SceNpCommon_C2BF5C1F(
						escapedBuf, escapedSize,
						reinterpret_cast<const SceUInt8 *>(title->status),
						statusLen, NULL);
					if (ret < 0) {
						npRtHeapFree(escapedBuf, NULL);
						break;
					}
					BxceNodeId gsNode = ScebXCe_AC0E4F67(
						doc, xNode, "gameSpecified", NULL,
						escapedBuf, escapedSize - 1);
					npRtHeapFree(escapedBuf, NULL);
					if (gsNode == 0) {
						ret = NP_RT_CORE_ERR_XML_ATTR;
						break;
					}
				}
			}
		}

		const char *presTypeStr = NULL;
		if (title->presenceType == 1)
			presTypeStr = "default";
		else if (title->presenceType == 2)
			presTypeStr = "joinable";
		else if (title->presenceType == 3)
			presTypeStr = "party-only";
		else if (title->presenceType == 4)
			presTypeStr = "join-game-ack";

		if (presTypeStr != NULL) {
			SceSize typeLen = sceClibStrnlen(presTypeStr, 0x7FFFFFFFu);
			if (ScebXCe_AC0E4F67(doc, xNode, "type", NULL, presTypeStr, typeLen) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (title->joinableCommId.data[0] != '\0') {
			char joinIdBuf[16];
			ret = SceNpCommon_E426FEE8(&title->joinableCommId, joinIdBuf, 13);
			if (ret < 0)
				break;
			if (ScebXCe_AC0E4F67(doc, xNode, "joiningId", NULL, joinIdBuf, 13) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		if (title->contentId[0] != '\0') {
			SceSize contentIdLen = sceClibStrnlen(title->contentId, sizeof(title->contentId));
			if (ScebXCe_AC0E4F67(doc, xNode, "contentId", NULL,
					     title->contentId, contentIdLen) == 0) {
				ret = NP_RT_CORE_ERR_XML_ATTR;
				break;
			}
		}

		ScebXCe_B367042F(attrs);
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return ret;
}

/* 0x81408C86 */
static int parsePresenceTitleElement(NpImpsCoreTitleNode **listHead, BxceNodeId elemNode,
				     BxceDocument *doc, void *customAlloc)
{
	NpImpsCoreTitleNode *title = reinterpret_cast<NpImpsCoreTitleNode *>(
		npRtHeapAlloc(sizeof(NpImpsCoreTitleNode), customAlloc));
	if (title == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(title, 0, sizeof(NpImpsCoreTitleNode));

	const char *activeStr = ScebXCe_753D9382(doc, elemNode, "active");
	if (activeStr == NULL) {
		title->updatePriority = 0;
	} else if (sceClibStrncmp(activeStr, "0", 2u) == 0) {
		title->updatePriority = 1;
	} else if (sceClibStrncmp(activeStr, "1", 2u) == 0) {
		title->updatePriority = 2;
	} else {
		title->updatePriority = -1;
	}

	const char *modeStr = NULL;
	npRtGetElementText(doc, elemNode, "mode", &modeStr);
	if (modeStr != NULL)
		title->platform = lookupValueByName(kPresenceModeTable, modeStr);

	const char *objectIdStr = NULL;
	npRtGetElementText(doc, elemNode, "objectId", &objectIdStr);
	if (objectIdStr != NULL) {
		int ret = SceNpCommon_5BC54899(&title->titleId, objectIdStr);
		if (ret < 0) {
			freePresenceTitleList(title, customAlloc);
			return ret;
		}
	}

	const char *defaultNameStr = NULL;
	npRtGetElementText(doc, elemNode, "defaultName", &defaultNameStr);
	if (defaultNameStr != NULL) {
		int ret = npRtXmlUnescapeToBuf(defaultNameStr, title->titleIdStr,
					       sizeof(title->titleIdStr));
		if (ret < 0) {
			freePresenceTitleList(title, customAlloc);
			return ret;
		}
	}

	const char *localNameStr = NULL;
	npRtGetElementText(doc, elemNode, "localName", &localNameStr);
	if (localNameStr != NULL) {
		int ret = npRtXmlUnescapeToBuf(localNameStr, title->titleName,
					       sizeof(title->titleName));
		if (ret < 0) {
			freePresenceTitleList(title, customAlloc);
			return ret;
		}
	}

	const char *localLangStr = NULL;
	npRtGetElementText(doc, elemNode, "localLang", &localLangStr);
	if (localLangStr != NULL)
		title->isSystemTitle = static_cast<SceUInt8>(SceNpCommon_5B770B80(localLangStr, NULL, 10));

	const char *gameSpecStr = NULL;
	npRtGetElementText(doc, elemNode, "gameSpecified", &gameSpecStr);
	if (gameSpecStr != NULL) {
		int ret = npRtXmlUnescapeToBuf(gameSpecStr, title->status, sizeof(title->status));
		if (ret < 0) {
			freePresenceTitleList(title, customAlloc);
			return ret;
		}
	}

	title->presenceType = 0;
	const char *typeStr = NULL;
	npRtGetElementText(doc, elemNode, "type", &typeStr);
	if (typeStr != NULL) {
		if (sceClibStrncmp(typeStr, "default", 8u) == 0)
			title->presenceType = 1;
		else if (sceClibStrncmp(typeStr, "joinable", 9u) == 0)
			title->presenceType = 2;
		else if (sceClibStrncmp(typeStr, "party-only", 11u) == 0)
			title->presenceType = 3;
		else if (sceClibStrncmp(typeStr, "join-game-ack", 14u) == 0)
			title->presenceType = 4;
		else
			title->presenceType = -1;
	}

	const char *joiningIdStr = NULL;
	npRtGetElementText(doc, elemNode, "joiningId", &joiningIdStr);
	if (joiningIdStr != NULL) {
		int ret = SceNpCommon_5BC54899(&title->joinableCommId, joiningIdStr);
		if (ret < 0) {
			freePresenceTitleList(title, customAlloc);
			return ret;
		}
	}

	const char *contentIdStr = NULL;
	npRtGetElementText(doc, elemNode, "contentId", &contentIdStr);
	if (contentIdStr != NULL) {
		sceClibStrncpy(title->contentId, contentIdStr, sizeof(title->contentId));
		title->contentId[sizeof(title->contentId) - 1] = '\0';
	}

	NpImpsCoreTitleNode **tail = listHead;
	while (*tail != NULL)
		tail = &(*tail)->next;
	*tail = title;
	return 0;
}

/* 0x81408FAC */
static int clonePresenceTitleList(const NpImpsCoreTitleNode *srcHead, NpImpsCoreTitleNode **outHead,
				  void *customAlloc)
{
	if (srcHead == NULL) {
		*outHead = NULL;
		return 0;
	}

	NpImpsCoreTitleNode *copyHead = NULL;
	NpImpsCoreTitleNode *copyTail = NULL;

	for (const NpImpsCoreTitleNode *src = srcHead; src != NULL; src = src->next) {
		NpImpsCoreTitleNode *title = reinterpret_cast<NpImpsCoreTitleNode *>(
			npRtHeapAlloc(sizeof(NpImpsCoreTitleNode), customAlloc));
		if (title == NULL) {
			freePresenceTitleList(copyHead, customAlloc);
			return NP_RT_CORE_ERR_NO_MEMORY;
		}
		sceClibMemset(title, 0, sizeof(NpImpsCoreTitleNode));
		if (copyTail != NULL)
			copyTail->next = title;
		else
			copyHead = title;
		copyTail = title;

		sceClibMemcpy_safe(title, src, sizeof(NpImpsCoreTitleNode));
		title->next = NULL;
	}

	*outHead = copyHead;
	return 0;
}

/* --------------------------------------------------------------------------
 * Session extension ("http://playstation.net/np/rt/session")
 * 0x81409034..0x8140910B
 * -------------------------------------------------------------------------- */

/* 0x81409034 */
static int parseSessionExt(NpRtSessionExt **outSession, BxceNodeId elemNode, BxceDocument *doc,
			   void *customAlloc)
{
	NpRtSessionExt *session = reinterpret_cast<NpRtSessionExt *>(
		npRtHeapAlloc(sizeof(NpRtSessionExt), customAlloc));
	if (session == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	session->sessionType = 0;
	session->dateTick.tick = 0;

	const char *dateStr = ScebXCe_753D9382(doc, elemNode, "date");
	if (dateStr != NULL) {
		int ret = sceRtcParseDateTime(&session->dateTick, dateStr);
		if (ret < 0) {
			npRtHeapFree(session, customAlloc);
			return ret;
		}
	}

	const char *typeStr = ScebXCe_753D9382(doc, elemNode, "type");
	if (typeStr != NULL) {
		int typeVal = SceNpCommon_CC8E6AC6(typeStr);
		if (typeVal == 3)
			session->sessionType = 1;
		else if (typeVal == 4)
			session->sessionType = 2;
	}

	*outSession = session;
	return 0;
}

/* 0x814090CC */
static int cloneSessionExt(const NpRtSessionExt *src, NpRtSessionExt **outCopy, void *customAlloc)
{
	if (src == NULL) {
		*outCopy = NULL;
		return 0;
	}

	NpRtSessionExt *copy = reinterpret_cast<NpRtSessionExt *>(
		npRtHeapAlloc(sizeof(NpRtSessionExt), customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, sizeof(NpRtSessionExt));
	copy->sessionType = src->sessionType;
	copy->dateTick = src->dateTick;
	*outCopy = copy;
	return 0;
}

/* --------------------------------------------------------------------------
 * Message & Presence extension namespace dispatch tables
 * -------------------------------------------------------------------------- */

typedef int (*MsgExtParseHandler)(NpRtMessageExtBundle *bundle, BxceNodeId elemNode,
				  BxceDocument *doc, void *customAlloc, SceUInt8 flags);

typedef int (*PresExtParseHandler)(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode,
				   BxceDocument *doc, void *customAlloc, SceUInt8 flags);

struct MsgExtDispatchEntry {
	const char *xmlns;
	MsgExtParseHandler handler;
};

struct PresExtDispatchEntry {
	const char *xmlns;
	PresExtParseHandler handler;
};

/* 0x814061A2 */
static int onMsgMetaExt(NpRtMessageExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parseMessageMetaExt(&bundle->messageExt, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814061BC */
static int onMsgExtData(NpRtMessageExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			void *customAlloc, SceUInt8 flags)
{
	int ret = parseExtDataElement(&bundle->extData, elemNode, doc, customAlloc, flags);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814061D6 */
static int onMsgContentExt(NpRtMessageExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			   void *customAlloc, SceUInt8 flags)
{
	int ret = parseContentExtElement(&bundle->contentExt, elemNode, doc, customAlloc, flags);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814061EE */
static int onMsgStoredExt(NpRtMessageExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			  void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parseStoredExt(&bundle->storedExt, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81406200 */
static int onMsgNotificationExt(NpRtMessageExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
				void *customAlloc, SceUInt8 flags)
{
	int ret = parseNotificationExt(&bundle->notifExt, elemNode, doc, customAlloc, flags);
	return (ret >= 0) ? 0 : ret;
}

/* 0x815495E4 */
static const MsgExtDispatchEntry kMsgExtDispatchTable[] = {
	{ "http://playstation.net/np/rt/message",      onMsgMetaExt },
	{ "http://playstation.net/np/rt/ext",          onMsgExtData },
	{ "http://playstation.net/np/rt/content",      onMsgContentExt },
	{ "http://playstation.net/np/rt/stored",       onMsgStoredExt },
	{ "http://playstation.net/np/rt/notification", onMsgNotificationExt },
	{ NULL,                                        NULL }
};

/* 0x81407358 */
static int onPresSessionExt(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			    void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parseSessionExt(&bundle->sessionExt, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81407368 */
static int onPresTitleExt(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			  void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parsePresenceTitleElement(&bundle->titleList, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81407382 */
static int onPresExtData(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			 void *customAlloc, SceUInt8 flags)
{
	int ret = parseExtDataElement(&bundle->extData, elemNode, doc, customAlloc, flags);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8140739C */
static int onPresDelayExt(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			  void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parseDelayStamp(&bundle->delayTick, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x814073AE */
static int onPresStoredExt(NpRtPresenceExtBundle *bundle, BxceNodeId elemNode, BxceDocument *doc,
			   void *customAlloc, SceUInt8 flags)
{
	(void)flags;
	int ret = parseStoredExt(&bundle->storedExt, elemNode, doc, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8154964C */
static const PresExtDispatchEntry kPresExtDispatchTable[] = {
	{ "http://playstation.net/np/rt/session",  onPresSessionExt },
	{ "http://playstation.net/np/rt/presence", onPresTitleExt },
	{ "http://playstation.net/np/rt/ext",      onPresExtData },
	{ "jabber:x:delay",                        onPresDelayExt },
	{ "http://playstation.net/np/rt/stored",   onPresStoredExt },
	{ NULL,                                    NULL }
};

} /* namespace */

/* --------------------------------------------------------------------------
 * Public Message Extension Bundle API (0x81405EE8..0x814061A1)
 * -------------------------------------------------------------------------- */

/* 0x81405EE8 */
void npRtExtFreeMessageBundle(NpRtMessageExtBundle *bundle, void *customAlloc, SceUInt8 flags)
{
	freeExtDataList(bundle->extData, customAlloc, flags);
	bundle->extData = NULL;
	freeContentExt(bundle->contentExt, customAlloc, flags);
	bundle->contentExt = NULL;
	freeStoredExt(bundle->storedExt, customAlloc);
	bundle->storedExt = NULL;
	freeNotificationExt(bundle->notifExt, customAlloc, flags);
	bundle->notifExt = NULL;
	freeMessageMetaExt(bundle->messageExt, customAlloc);
	bundle->messageExt = NULL;
}

/* 0x81405F2E */
int npRtExtParseMessageBundle(NpRtMessageExtBundle *outBundle, const NpRtMessageStanza *stanza,
			      void *customAlloc, SceUInt8 flags)
{
	BxceDocument *doc = stanza->xmlDoc;
	sceClibMemset(outBundle, 0, sizeof(NpRtMessageExtBundle));
	if (doc == NULL)
		return 0;

	BxceNodeId root = ScebXCe_75E1B095(doc);
	BxceNodeList *xNodes = ScebXCe_1FA24DC0(doc, root, "x");
	if (xNodes == NULL)
		return 0;

	int ret = 0;
	for (SceUInt32 idx = 0; idx < static_cast<SceUInt32>(xNodes->length) && ret >= 0; ++idx) {
		BxceNodeId elem = ScebXCe_6C46D786(xNodes, idx);
		if (elem == 0)
			continue;

		const char *xmlns = ScebXCe_753D9382(doc, elem, "xmlns");
		if (xmlns == NULL)
			continue;

		for (const MsgExtDispatchEntry *cur = kMsgExtDispatchTable; cur->xmlns != NULL; ++cur) {
			SceSize nsLen = sceClibStrnlen(cur->xmlns, 0x7FFFFFFFu);
			if (sceClibStrncmp(xmlns, cur->xmlns, nsLen + 1u) == 0) {
				ret = cur->handler(outBundle, elem, doc, customAlloc, flags);
				break;
			}
		}
	}

	ScebXCe_1F59F0C4(xNodes);
	ScebXCe_EC560647(xNodes);

	if (ret < 0)
		npRtExtFreeMessageBundle(outBundle, customAlloc, flags);
	return ret;
}

/* 0x81406058 */
int npRtExtCloneMessageBundle(NpRtMessageExtBundle *outBundle, const NpRtMessageExtBundle *src,
			      void *customAlloc, SceUInt8 flags)
{
	SceRtcTick *clonedMeta = NULL;
	int ret = cloneTick(reinterpret_cast<const SceRtcTick *>(src->messageExt),
			    &clonedMeta, customAlloc);
	if (ret < 0)
		return ret;
	outBundle->messageExt = reinterpret_cast<NpRtMessageMetaExt *>(clonedMeta);
	ret = cloneContentExtList(src->contentExt, &outBundle->contentExt, customAlloc, flags);
	if (ret < 0)
		return ret;
	ret = cloneExtDataList(src->extData, &outBundle->extData, customAlloc, flags);
	if (ret < 0)
		return ret;
	ret = cloneStoredExt(src->storedExt, &outBundle->storedExt, customAlloc);
	if (ret < 0)
		return ret;
	ret = cloneNotificationExt(src->notifExt, &outBundle->notifExt, customAlloc, flags);
	if (ret < 0)
		return ret;
	outBundle->antiblock = src->antiblock;
	return 0;
}

/* 0x814060BE */
int npRtExtSerializeMessageBundle(BxceDocument **outDoc, const NpRtMessageExtBundle *bundle)
{
	*outDoc = NULL;
	if (bundle->extData == NULL && bundle->antiblock == 0 &&
	    bundle->contentExt == NULL && bundle->notifExt == NULL)
		return 0;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	if (bundle->messageExt != NULL)
		ret = serializeMessageMetaExt(doc, bundle->messageExt);
	if (ret >= 0 && bundle->extData != NULL)
		ret = serializeExtDataList(doc, bundle->extData);
	if (ret >= 0 && bundle->contentExt != NULL)
		ret = serializeContentExtList(doc, bundle->contentExt);
	if (ret >= 0 && bundle->antiblock != 0)
		ret = serializeAntiblockExt(doc);
	if (ret >= 0 && bundle->storedExt != NULL)
		ret = serializeStoredExt(doc, bundle->storedExt);
	if (ret >= 0 && bundle->notifExt != NULL)
		ret = serializeNotificationExt(doc, bundle->notifExt);

	if (ret < 0) {
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
		return ret;
	}

	if (ScebXCe_CED6BECF(doc) != 0) {
		*outDoc = doc;
	} else {
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}
	return 0;
}

/* --------------------------------------------------------------------------
 * Public Presence Extension Bundle API (0x81407044..0x81407357)
 * -------------------------------------------------------------------------- */

/* 0x81407044 */
void npRtExtFreePresenceBundle(NpRtPresenceExtBundle *bundle, void *customAlloc, SceUInt8 flags)
{
	freeTickOrSession(bundle->sessionExt, customAlloc);
	bundle->sessionExt = NULL;
	freePresenceTitleList(bundle->titleList, customAlloc);
	bundle->titleList = NULL;
	freeTickOrSession(bundle->delayTick, customAlloc);
	bundle->delayTick = NULL;
	freeCapsList(bundle->capsList, customAlloc, flags);
	bundle->capsList = NULL;
	freeExtDataList(bundle->extData, customAlloc, flags);
	bundle->extData = NULL;
	freeStoredExt(bundle->storedExt, customAlloc);
	bundle->storedExt = NULL;
}

/* 0x81407094 */
int npRtExtParsePresenceBundle(NpRtPresenceExtBundle *outBundle, const NpRtPresenceStanza *stanza,
			       void *customAlloc, SceUInt8 flags)
{
	BxceDocument *doc = stanza->xmlDoc;
	sceClibMemset(outBundle, 0, sizeof(NpRtPresenceExtBundle));
	if (doc == NULL)
		return 0;

	int ret = 0;
	BxceNodeId root = ScebXCe_75E1B095(doc);
	BxceNodeList *xNodes = ScebXCe_1FA24DC0(doc, root, "x");
	if (xNodes != NULL) {
		for (SceUInt32 idx = 0; idx < static_cast<SceUInt32>(xNodes->length) && ret >= 0; ++idx) {
			BxceNodeId elem = ScebXCe_6C46D786(xNodes, idx);
			if (elem == 0)
				continue;

			const char *xmlns = ScebXCe_753D9382(doc, elem, "xmlns");
			if (xmlns == NULL)
				continue;

			for (const PresExtDispatchEntry *cur = kPresExtDispatchTable;
			     cur->xmlns != NULL; ++cur) {
				SceSize nsLen = sceClibStrnlen(cur->xmlns, 0x7FFFFFFFu);
				if (sceClibStrncmp(xmlns, cur->xmlns, nsLen + 1u) == 0) {
					ret = cur->handler(outBundle, elem, doc, customAlloc, flags);
					break;
				}
			}
		}
		ScebXCe_1F59F0C4(xNodes);
		ScebXCe_EC560647(xNodes);
		if (ret < 0) {
			npRtExtFreePresenceBundle(outBundle, customAlloc, flags);
			return ret;
		}
	}

	BxceNodeId capsRoot = ScebXCe_75E1B095(doc);
	BxceNodeList *cNodes = ScebXCe_1FA24DC0(doc, capsRoot, "c");
	if (cNodes != NULL) {
		ret = parseCapsList(&outBundle->capsList, cNodes, doc, customAlloc, flags);
		ScebXCe_1F59F0C4(cNodes);
		ScebXCe_EC560647(cNodes);
		if (ret < 0) {
			npRtExtFreePresenceBundle(outBundle, customAlloc, flags);
			return ret;
		}
	}

	return 0;
}

/* 0x81407226 */
int npRtExtClonePresenceBundle(NpRtPresenceExtBundle *outBundle, const NpRtPresenceExtBundle *src,
			       void *customAlloc, SceUInt8 flags)
{
	int ret = cloneSessionExt(src->sessionExt, &outBundle->sessionExt, customAlloc);
	if (ret < 0)
		return ret;
	ret = clonePresenceTitleList(src->titleList, &outBundle->titleList, customAlloc);
	if (ret < 0)
		return ret;
	ret = cloneTick(src->delayTick, &outBundle->delayTick, customAlloc);
	if (ret < 0)
		return ret;
	ret = cloneCapsList(src->capsList, &outBundle->capsList, customAlloc, flags);
	if (ret < 0)
		return ret;
	ret = cloneExtDataList(src->extData, &outBundle->extData, customAlloc, flags);
	if (ret < 0)
		return ret;
	ret = cloneStoredExt(src->storedExt, &outBundle->storedExt, customAlloc);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81407298 */
int npRtExtSerializePresenceBundle(BxceDocument **outDoc, const NpRtPresenceExtBundle *bundle)
{
	*outDoc = NULL;
	if (bundle->titleList == NULL && bundle->capsList == NULL && bundle->extData == NULL)
		return 0;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_RT_CORE_ERR_XML_ATTR;
	}

	int ret = 0;
	if (bundle->titleList != NULL)
		ret = serializePresenceTitleList(doc, bundle->titleList);
	if (ret >= 0 && bundle->capsList != NULL)
		ret = serializeCapsList(doc, bundle->capsList);
	if (ret >= 0 && bundle->extData != NULL)
		ret = serializeExtDataList(doc, bundle->extData);
	if (ret >= 0 && bundle->storedExt != NULL)
		ret = serializeStoredExt(doc, bundle->storedExt);

	if (ret < 0) {
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
		return ret;
	}

	if (ScebXCe_CED6BECF(doc) != 0) {
		*outDoc = doc;
	} else {
		ScebXCe_B0891B13(doc);
		sceNpCommonFreeNpServerName(doc);
	}
	return 0;
}
