/*
 * np_rt_util.cpp: SceNetXmpp heap/mspace allocator, 64-bit integer formatter,
 * string/JID/LangText utilities, and XMPP XML stanza serializers
 * (0x81312638..0x8131370B, 0x81314524..0x81314547).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_core.h"

extern "C" {
extern void *SceNpCommon_50D71BB5(void *ptr, SceSize size);
}

namespace {

static int s_heapRefCount = 0;		/* 0x815463E8 */
static int s_useMspace = 0;		/* 0x815463EC */
static void *s_mspace = NULL;		/* 0x815C53E8 */

/* 0x81314524 */
static int createMspace(void **outMspace, void *memblock, SceSize size)
{
	void *mspace = sceClibMspaceCreate(memblock, size);
	if (mspace == NULL)
		return NP_RT_CORE_ERR_INTERNAL;
	*outMspace = mspace;
	return 0;
}

/* 0x81312BE0: Build bXCe attribute list for common XMPP stanza header */
static int buildStanzaHeaderAttrs(const NpRtStanzaInit *header, BxceAttributeList **outAttrs)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_XMPP_ERR_XML;
	}

	int status = 0;
	SceSize required = 0;

	if (header->toJid.domain != NULL) {
		status = SceNpCommon_85699AE2(&header->toJid, NULL, 0, &required);
		if (status == 0) {
			char *jidBuf = reinterpret_cast<char *>(npRtHeapAlloc(required, NULL));
			if (jidBuf == NULL) {
				status = NP_XMPP_ERR_NO_MEMORY;
			} else {
				status = SceNpCommon_85699AE2(&header->toJid, jidBuf, required,
							      NULL);
				if (status == 0) {
					if (ScebXCe_3DD761CF(attrs, "to", jidBuf) != 0)
						status = NP_XMPP_ERR_XML;
				}
				npRtHeapFree(jidBuf, NULL);
			}
		}
	}

	if (status == 0 && header->fromJid.domain != NULL) {
		status = SceNpCommon_85699AE2(&header->fromJid, NULL, 0, &required);
		if (status == 0) {
			char *jidBuf = reinterpret_cast<char *>(npRtHeapAlloc(required, NULL));
			if (jidBuf == NULL) {
				status = NP_XMPP_ERR_NO_MEMORY;
			} else {
				status = SceNpCommon_85699AE2(&header->fromJid, jidBuf, required,
							      NULL);
				if (status == 0) {
					if (ScebXCe_3DD761CF(attrs, "from", jidBuf) != 0)
						status = NP_XMPP_ERR_XML;
				}
				npRtHeapFree(jidBuf, NULL);
			}
		}
	}

	char enumBuf[16];
	if (status == 0 && header->stanzaType != 0) {
		if (npRtFormatStanzaType(header->stanzaType, enumBuf) != 0 ||
		    ScebXCe_3DD761CF(attrs, "type", enumBuf) != 0) {
			status = NP_XMPP_ERR_XML;
		}
	}

	if (status == 0 && header->stanzaId != NULL) {
		if (ScebXCe_3DD761CF(attrs, "id", header->stanzaId) != 0)
			status = NP_XMPP_ERR_XML;
	}

	if (status == 0 && header->lang != 0) {
		status = npRtFormatLanguage(header->lang, enumBuf);
		if (status == 0 && ScebXCe_3DD761CF(attrs, "xml:lang", enumBuf) != 0)
			status = NP_XMPP_ERR_XML;
	}

	if (status != 0) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
		return status;
	}

	*outAttrs = attrs;
	return 0;
}

/* 0x81312DBA: Append localized text child elements (<status>, <subject>, <body>, <text>) */
static int appendLangTextElements(BxceDocument *doc, BxceNodeId parentNode,
				  const NpRtLangTextNode *list, const char *elemName,
				  int addStanzasXmlns)
{
	if (list == NULL)
		return 0;

	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_XMPP_ERR_XML;
	}

	int status = 0;
	for (const NpRtLangTextNode *cur = list; cur != NULL; cur = cur->next) {
		if (cur->lang != 0) {
			char langBuf[4];
			if (npRtFormatLanguage(cur->lang, langBuf) != 0)
				break;
			if (ScebXCe_3DD761CF(attrs, "xml:lang", langBuf) != 0) {
				status = NP_XMPP_ERR_XML;
				break;
			}
		}

		if (addStanzasXmlns == 1) {
			if (ScebXCe_3DD761CF(attrs, "xmlns",
					     "urn:ietf:params:xml:ns:xmpp-stanzas") != 0) {
				status = NP_XMPP_ERR_XML;
				break;
			}
		}

		if (cur->text != NULL) {
			SceSize textLen = sceClibStrnlen(cur->text, 0x7FFFFFFFu);
			if (textLen == 0)
				continue;

			SceSize escapedSize = 0;
			status = SceNpCommon_C2BF5C1F(
				NULL, 0, reinterpret_cast<const SceUInt8 *>(cur->text),
				textLen, &escapedSize);
			if (status < 0)
				break;

			if (escapedSize - 1 == textLen) {
				ScebXCe_AC0E4F67(doc, parentNode, elemName, attrs,
						 cur->text, textLen);
			} else {
				char *escapedBuf = reinterpret_cast<char *>(
					npRtHeapAlloc(escapedSize, NULL));
				status = SceNpCommon_C2BF5C1F(
					escapedBuf, escapedSize,
					reinterpret_cast<const SceUInt8 *>(cur->text),
					textLen, NULL);
				if (status < 0) {
					npRtHeapFree(escapedBuf, NULL);
					break;
				}
				ScebXCe_AC0E4F67(doc, parentNode, elemName, attrs,
						 escapedBuf, escapedSize - 1);
				npRtHeapFree(escapedBuf, NULL);
			}
		}
		ScebXCe_B367042F(attrs);
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return status;
}

static int serializeDocumentToString(BxceDocument *doc, char **outXml)
{
	BxceSerializer *serializer = reinterpret_cast<BxceSerializer *>(
		sceNpCommonMallocNpServerName(sizeof(BxceSerializer)));
	if (serializer == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	ScebXCe_DC8483D4(serializer);
	int status = 0;
	if (ScebXCe_D7FE96A2(serializer, doc, outXml, NULL, 0) != 0)
		status = NP_XMPP_ERR_XML;

	ScebXCe_BE1AD985();
	sceNpCommonFreeNpServerName(serializer);
	return status;
}

static void extractHeaderFromStanza(NpRtStanzaInit *outHeader, const NpJid *toJid,
				    const NpJid *fromJid, const char *stanzaId,
				    int stanzaType, int lang)
{
	outHeader->toJid = *toJid;
	outHeader->fromJid = *fromJid;
	outHeader->stanzaId = stanzaId;
	outHeader->stanzaType = stanzaType;
	outHeader->lang = lang;
}

/* 0x81312F3E */
static int buildPresenceXml(const NpRtPresenceStanza *stanza, BxceDocument *doc,
			    BxceNodeId parentNode, SceUInt8 skipRootElem)
{
	BxceAttributeList *attrs = NULL;
	int status = 0;

	if (skipRootElem == 0) {
		NpRtStanzaInit header;
		extractHeaderFromStanza(&header, &stanza->toJid, &stanza->fromJid,
					stanza->stanzaId, stanza->stanzaType, stanza->lang);
		status = buildStanzaHeaderAttrs(&header, &attrs);
		if (status != 0)
			return status;
		parentNode = ScebXCe_AC0E4F67(doc, parentNode, "presence", attrs, NULL, 0);
	}

	char textBuf[12];
	if (stanza->show != 0) {
		status = npRtFormatPresenceShow(stanza->show, textBuf);
		if (status == 0) {
			SceSize showLen = sceClibStrnlen(textBuf, 0x7FFFFFFFu);
			ScebXCe_AC0E4F67(doc, parentNode, "show", NULL, textBuf, showLen);
		}
	}

	if (status == 0)
		status = appendLangTextElements(doc, parentNode, stanza->statusList, "status", 0);

	if (status == 0) {
		BxceNodeId insertAfter = 0;
		if (stanza->priority != 0) {
			sceClibSnprintf(textBuf, 3u, "%d", static_cast<int>(stanza->priority));
			SceSize prioLen = sceClibStrnlen(textBuf, 0x7FFFFFFFu);
			insertAfter = ScebXCe_AC0E4F67(doc, parentNode, "priority", NULL,
						       textBuf, prioLen);
		}

		if (stanza->xmlDoc != NULL) {
			BxceNodeId extNode = ScebXCe_CED6BECF(stanza->xmlDoc);
			while (extNode != 0) {
				if (ScebXCe_62C76804(doc, parentNode, insertAfter,
						     stanza->xmlDoc, extNode) == 0) {
					status = NP_XMPP_ERR_XML;
					break;
				}
				extNode = ScebXCe_6FC9DC3F(stanza->xmlDoc, extNode);
			}
		}
	}

	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
	return status;
}

/* 0x81313120 */
static int buildIqXml(const NpRtStanza *stanza, BxceDocument *doc,
		      BxceNodeId parentNode, SceUInt8 skipRootElem)
{
	BxceAttributeList *attrs = NULL;
	int status = 0;

	if (skipRootElem == 0) {
		NpRtStanzaInit header;
		extractHeaderFromStanza(&header, &stanza->toJid, &stanza->fromJid,
					stanza->stanzaId, stanza->stanzaType, stanza->lang);
		status = buildStanzaHeaderAttrs(&header, &attrs);
		if (status != 0)
			return status;
		parentNode = ScebXCe_AC0E4F67(doc, parentNode, "iq", attrs, NULL, 0);
	}

	if (stanza->xmlDoc != NULL) {
		BxceNodeId extNode = ScebXCe_CED6BECF(stanza->xmlDoc);
		while (extNode != 0) {
			if (ScebXCe_62C76804(doc, parentNode, 0, stanza->xmlDoc, extNode) == 0) {
				status = NP_XMPP_ERR_XML;
				break;
			}
			extNode = ScebXCe_6FC9DC3F(stanza->xmlDoc, extNode);
		}
	}

	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
	return status;
}

/* 0x8131327C */
static int buildMessageXml(const NpRtMessageStanza *stanza, BxceDocument *doc,
			   BxceNodeId parentNode, SceUInt8 skipRootElem)
{
	BxceAttributeList *attrs = NULL;
	int status = 0;

	if (skipRootElem == 0) {
		NpRtStanzaInit header;
		extractHeaderFromStanza(&header, &stanza->toJid, &stanza->fromJid,
					stanza->stanzaId, stanza->stanzaType, stanza->lang);
		status = buildStanzaHeaderAttrs(&header, &attrs);
		if (status != 0)
			return status;
		parentNode = ScebXCe_AC0E4F67(doc, parentNode, "message", attrs, NULL, 0);
	}

	status = appendLangTextElements(doc, parentNode, stanza->subjectList, "subject", 0);
	if (status == 0)
		status = appendLangTextElements(doc, parentNode, stanza->bodyList, "body", 0);

	if (status == 0) {
		BxceNodeId insertAfter = 0;
		if (stanza->threadId != NULL) {
			SceSize threadLen = sceClibStrnlen(stanza->threadId, 0x7FFFFFFFu);
			insertAfter = ScebXCe_AC0E4F67(doc, parentNode, "thread", NULL,
						       stanza->threadId, threadLen);
		}

		if (stanza->xmlDoc != NULL) {
			BxceNodeId extNode = ScebXCe_CED6BECF(stanza->xmlDoc);
			while (extNode != 0) {
				if (ScebXCe_62C76804(doc, parentNode, insertAfter,
						     stanza->xmlDoc, extNode) == 0) {
					status = NP_XMPP_ERR_XML;
					break;
				}
				extNode = ScebXCe_6FC9DC3F(stanza->xmlDoc, extNode);
			}
		}
	}

	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
	return status;
}

/* 0x8131343E */
static int appendErrorElement(BxceDocument *doc, BxceNodeId parentNode,
			      const NpXmppErrorStanza *errStanza)
{
	BxceAttributeList *attrs = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	if (attrs == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(attrs) != 0) {
		sceNpCommonFreeNpServerName(attrs);
		return NP_XMPP_ERR_XML;
	}

	int status = NP_XMPP_ERR_XML;
	char buf[28];

	bool headerOk = true;
	if (errStanza->errorType != 0) {
		if (npRtFormatErrorType(errStanza->errorType, buf) != 0 ||
		    ScebXCe_3DD761CF(attrs, "type", buf) != 0) {
			headerOk = false;
		}
	}

	if (headerOk) {
		BxceNodeId errorNode = ScebXCe_AC0E4F67(doc, parentNode, "error", attrs, NULL, 0);
		bool condOk = true;

		if (errStanza->condition != 0) {
			ScebXCe_B367042F(attrs);
			if (ScebXCe_3DD761CF(attrs, "xmlns",
					     "urn:ietf:params:xml:ns:xmpp-stanzas") != 0 ||
			    npRtFormatErrorCondition(errStanza->condition, buf) != 0) {
				condOk = false;
			} else {
				ScebXCe_AC0E4F67(doc, errorNode, buf, attrs, NULL, 0);
			}
		}

		if (condOk) {
			status = 0;
			if (errStanza->textList != NULL)
				status = appendLangTextElements(doc, errorNode,
								errStanza->textList, "text", 1);

			if (status == 0 && errStanza->errorKind == NP_RT_XMPP_SOURCE_CLIENT) {
				ScebXCe_B367042F(attrs);
				sceClibSnprintf(buf, 0x19u, "%02x",
						static_cast<unsigned int>(errStanza->errorCode));
				if (ScebXCe_3DD761CF(attrs, "code", buf) != 0 ||
				    ScebXCe_3DD761CF(attrs, "source", "client") != 0 ||
				    ScebXCe_3DD761CF(attrs, "xmlns",
						     "http://playstation.net/np/rt/error") != 0) {
					status = NP_XMPP_ERR_XML;
				} else {
					ScebXCe_AC0E4F67(doc, errorNode, "x", attrs, NULL, 0);
				}
			}
		}
	}

	ScebXCe_9C2F9897(attrs);
	sceNpCommonFreeNpServerName(attrs);
	return status;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Heap / Mspace Allocator (0x81312638..0x8131277B)
 * -------------------------------------------------------------------------- */
/* 0x81312638 */
int npRtHeapInit(void *memblock, SceSize size)
{
	if (s_heapRefCount == 0) {
		if (memblock != NULL) {
			int ret = createMspace(&s_mspace, memblock, size);
			if (ret < 0)
				return ret;
			s_useMspace = 1;
		} else {
			s_useMspace = 0;
		}
	}
	++s_heapRefCount;
	return 0;
}

/* 0x8131269C */
void npRtHeapTerm(void)
{
	if (s_heapRefCount != 0) {
		--s_heapRefCount;
		if (s_heapRefCount == 0) {
			if (s_useMspace != 0)
				sceClibMspaceDestroy(s_mspace);
			s_useMspace = 0;
		}
	}
}

/* 0x813126E6 */
void *npRtHeapAlloc(SceSize size, void *customAlloc)
{
	if (customAlloc != NULL) {
		NpRtCustomAllocator *alloc = reinterpret_cast<NpRtCustomAllocator *>(customAlloc);
		return alloc->alloc(size, alloc->userArg);
	}
	if (s_useMspace != 0)
		return sceClibMspaceMalloc(s_mspace, size);
	return sceNpCommonMallocNpServerName(size);
}

/* 0x81312716 */
void *npRtHeapRealloc(void *ptr, SceSize size, void *customAlloc)
{
	if (customAlloc != NULL) {
		NpRtCustomAllocator *alloc = reinterpret_cast<NpRtCustomAllocator *>(customAlloc);
		return alloc->realloc(ptr, size, alloc->userArg);
	}
	if (s_useMspace != 0)
		return sceClibMspaceRealloc(s_mspace, ptr, size);
	return SceNpCommon_50D71BB5(ptr, size);
}

/* 0x81312748 */
void npRtHeapFree(void *ptr, void *customAlloc)
{
	if (ptr == NULL)
		return;
	if (customAlloc != NULL) {
		NpRtCustomAllocator *alloc = reinterpret_cast<NpRtCustomAllocator *>(customAlloc);
		alloc->free(ptr, alloc->userArg);
	} else if (s_useMspace != 0) {
		sceClibMspaceFree(s_mspace, ptr);
	} else {
		sceNpCommonFreeNpServerName(ptr);
	}
}

/* --------------------------------------------------------------------------
 * Integer Formatting, Strings, JID, and LangText Utilities (0x8131277C..0x81312BDF)
 * -------------------------------------------------------------------------- */
/* 0x8131277C */
int npRtFormatUInt64(SceUInt64 value, char *outBuf, SceSize outBufSize,
		     int base, SceSize *outRequired)
{
	SceUInt32 radix = static_cast<SceUInt32>(base);
	SceUInt64 remaining = value;
	SceSize digitCount = 1;

	while (remaining >= radix) {
		remaining /= radix;
		++digitCount;
	}

	if (outBuf != NULL) {
		if (outBufSize <= digitCount)
			return NP_RT_CORE_ERR_BUF_TOO_SMALL;

		outBuf[digitCount] = '\0';
		SceUInt64 curVal = value;
		for (SceSize idx = digitCount; idx > 0; --idx) {
			SceUInt32 digit = static_cast<SceUInt32>(curVal % radix);
			curVal /= radix;
			if (radix == 16 && digit > 9)
				outBuf[idx - 1] = static_cast<char>('a' + (digit - 10));
			else
				outBuf[idx - 1] = static_cast<char>('0' + digit);
		}
	}

	if (outRequired != NULL)
		*outRequired = digitCount;
	return 0;
}

/* 0x81312886 */
int npRtDupString(char **outStr, const char *src, void *customAlloc, SceUInt8 flags)
{
	if (src == NULL) {
		*outStr = NULL;
		return 0;
	}

	if ((flags & 1u) != 0) {
		*outStr = const_cast<char *>(src);
		return 0;
	}

	SceSize len = sceClibStrnlen(src, 0x7FFFFFFFu);
	char *copy = reinterpret_cast<char *>(npRtHeapAlloc(len + 1, customAlloc));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, len + 1);
	sceClibMemcpy_safe(copy, src, len);
	*outStr = copy;
	return 0;
}

/* 0x813128E4 */
void npRtFreeString(void *ptr, void *customAlloc, SceUInt8 flags)
{
	if ((flags & 1u) == 0)
		npRtHeapFree(ptr, customAlloc);
}

/* 0x813128F2 */
int npRtXmlUnescapeToBuf(const char *src, char *dst, SceSize dstSize)
{
	SceSize srcLen = sceClibStrnlen(src, 0x7FFFFFFFu);
	if (dstSize <= srcLen)
		return NP_RT_CORE_ERR_INVALID_ARG;

	SceSize unescapedSize = 0;
	int ret = SceNpCommon_389F8C14(
		NULL, 0, reinterpret_cast<const SceUInt8 *>(src), srcLen, &unescapedSize);
	if (ret < 0)
		return ret;

	char *decoded = reinterpret_cast<char *>(npRtHeapAlloc(unescapedSize, NULL));
	if (decoded == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	ret = SceNpCommon_389F8C14(
		decoded, unescapedSize, reinterpret_cast<const SceUInt8 *>(src), srcLen, NULL);
	if (ret < 0) {
		npRtHeapFree(decoded, NULL);
		return ret;
	}

	sceClibMemcpy_safe(dst, decoded, unescapedSize);
	if (dstSize > unescapedSize)
		sceClibMemset(dst + unescapedSize, 0, dstSize - unescapedSize);
	npRtHeapFree(decoded, NULL);
	return ret;
}

/* 0x813129B0 */
int npRtCopyJid(NpJid *dst, const NpJid *src)
{
	if (src->node == NULL) {
		dst->node = NULL;
		dst->domain = NULL;
		dst->resource = NULL;
		return 0;
	}

	SceSize nodeLen = sceClibStrnlen(src->node, 0x7FFFFFFFu);
	SceSize domainLen = sceClibStrnlen(src->domain, 0x7FFFFFFFu);
	SceSize resourceLen = sceClibStrnlen(src->resource, 0x7FFFFFFFu);
	SceSize totalSize = nodeLen + domainLen + resourceLen + 3;

	char *pool = reinterpret_cast<char *>(npRtHeapAlloc(totalSize, NULL));
	if (pool == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(pool, 0, totalSize);
	sceClibStrncpy(pool, src->node, nodeLen);
	dst->node = pool;

	char *domainDst = pool + nodeLen + 1;
	sceClibStrncpy(domainDst, src->domain, domainLen);
	dst->domain = domainDst;

	char *resourceDst = domainDst + domainLen + 1;
	sceClibStrncpy(resourceDst, src->resource, resourceLen);
	dst->resource = resourceDst;
	return 0;
}

/* 0x81312A6C */
void npRtFreeJid(NpJid *jid)
{
	if (jid == NULL)
		return;
	if (jid->node != NULL)
		npRtHeapFree(const_cast<char *>(jid->node), NULL);
	jid->node = NULL;
	jid->domain = NULL;
	jid->resource = NULL;
}

/* 0x81312A86 */
int npRtParseJidToNpId(const char *jidStr, SceNpId *outNpId)
{
	SceSize required = 0;
	int ret = SceNpCommon_D2B9BCA8(NULL, jidStr, NULL, 0, &required);
	if (ret < 0)
		return ret;

	char *pool = reinterpret_cast<char *>(npRtHeapAlloc(required, NULL));
	if (pool == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	NpJid parsedJid;
	ret = SceNpCommon_D2B9BCA8(&parsedJid, jidStr, pool, required, NULL);
	if (ret >= 0) {
		ret = SceNpCommon_9B7CAA67(outNpId, &parsedJid);
		if (ret >= 0)
			ret = 0;
	}
	npRtHeapFree(pool, NULL);
	return ret;
}

/* 0x81312B12 */
void npRtFreeLangTextList(NpRtLangTextNode *head)
{
	NpRtLangTextNode *cur = head;
	while (cur != NULL) {
		NpRtLangTextNode *next = cur->next;
		if (cur->text != NULL) {
			npRtHeapFree(cur->text, NULL);
			cur->text = NULL;
		}
		npRtHeapFree(cur, NULL);
		cur = next;
	}
}

/* 0x81312B36 */
int npRtAllocLangTextNode(NpRtLangTextNode **outNode, const char *text, int lang)
{
	NpRtLangTextNode *node = reinterpret_cast<NpRtLangTextNode *>(
		npRtHeapAlloc(sizeof(NpRtLangTextNode), NULL));
	if (node == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	node->next = NULL;
	node->lang = lang;

	int ret = 0;
	if (text != NULL) {
		SceSize len = sceClibStrnlen(text, 0x7FFFFFFFu);
		char *copy = reinterpret_cast<char *>(npRtHeapAlloc(len + 1, NULL));
		if (copy == NULL) {
			ret = NP_RT_CORE_ERR_NO_MEMORY;
		} else {
			sceClibMemset(copy, 0, len + 1);
			sceClibMemcpy_safe(copy, text, len);
			node->text = copy;
		}
	} else {
		node->text = NULL;
	}

	if (ret < 0) {
		npRtFreeLangTextList(node);
		return ret;
	}

	*outNode = node;
	return 0;
}

/* --------------------------------------------------------------------------
 * XMPP Stanza XML Serializers (0x8131306E..0x8131370B)
 * -------------------------------------------------------------------------- */
/* 0x8131306E */
int npXmppSerializePresence(const NpRtPresenceStanza *stanza, char **outXml)
{
	if (outXml == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_XMPP_ERR_NO_MEMORY;
	}

	BxceNodeId root = ScebXCe_75E1B095(doc);
	int status = buildPresenceXml(stanza, doc, root, 0);
	if (status == 0)
		status = serializeDocumentToString(doc, outXml);

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return status;
}

/* 0x813131CA */
int npXmppSerializeIq(const NpRtStanza *stanza, char **outXml)
{
	if (outXml == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_XMPP_ERR_NO_MEMORY;
	}

	BxceNodeId root = ScebXCe_75E1B095(doc);
	int status = buildIqXml(stanza, doc, root, 0);
	if (status == 0)
		status = serializeDocumentToString(doc, outXml);

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return status;
}

/* 0x8131338C */
int npXmppSerializeMessage(const NpRtMessageStanza *stanza, char **outXml)
{
	if (outXml == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_XMPP_ERR_NO_MEMORY;
	}

	BxceNodeId root = ScebXCe_75E1B095(doc);
	int status = buildMessageXml(stanza, doc, root, 0);
	if (status == 0)
		status = serializeDocumentToString(doc, outXml);

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return status;
}

/* 0x813135CA */
int npXmppSerializeIqError(const NpXmppErrorStanza *errStanza, char **outXml)
{
	if (outXml == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_XMPP_ERR_NO_MEMORY;
	}

	const NpRtStanza *orig = errStanza->origStanza;
	NpRtStanzaInit replyHeader;
	sceClibMemset(&replyHeader, 0, sizeof(replyHeader));
	replyHeader.toJid.node = orig->fromJid.node;
	replyHeader.toJid.domain = orig->fromJid.domain;
	replyHeader.toJid.resource = orig->fromJid.resource;
	replyHeader.fromJid.node = orig->toJid.node;
	replyHeader.fromJid.domain = orig->toJid.domain;
	replyHeader.fromJid.resource = orig->toJid.resource;
	replyHeader.stanzaId = orig->stanzaId;
	replyHeader.stanzaType = NP_RT_XMPP_TYPE_ERROR;
	replyHeader.lang = orig->lang;

	BxceAttributeList *attrs = NULL;
	int status = buildStanzaHeaderAttrs(&replyHeader, &attrs);
	if (status == 0) {
		BxceNodeId root = ScebXCe_75E1B095(doc);
		BxceNodeId iqNode = ScebXCe_AC0E4F67(doc, root, "iq", attrs, NULL, 0);
		status = buildIqXml(orig, doc, iqNode, 1);
		if (status == 0) {
			status = appendErrorElement(doc, iqNode, errStanza);
			if (status == 0)
				status = serializeDocumentToString(doc, outXml);
		}
	}

	if (attrs != NULL) {
		ScebXCe_9C2F9897(attrs);
		sceNpCommonFreeNpServerName(attrs);
	}
	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return status;
}
