/*
 * np_rt_stanza.cpp: SceNpRt XMPP error parser, ref-counted stanza helpers,
 * XMPP enum conversion tables, and bXCe DOM utilities (0x8130F3E4..0x8131031F).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_stanza.h"

namespace {

struct XmppEnumEntry {
	const char *name;
	int value;
};

/* 0x815461B8: XMPP stanza "type" attribute table */
static const XmppEnumEntry kStanzaTypeTable[] = {
	{ "",             NP_RT_XMPP_TYPE_NONE },
	{ "error",        NP_RT_XMPP_TYPE_ERROR },
	{ "chat",         NP_RT_XMPP_TYPE_CHAT },
	{ "groupchat",    NP_RT_XMPP_TYPE_GROUPCHAT },
	{ "headline",     NP_RT_XMPP_TYPE_HEADLINE },
	{ "normal",       NP_RT_XMPP_TYPE_NORMAL },
	{ "notification", NP_RT_XMPP_TYPE_NOTIFICATION },
	{ "probe",        NP_RT_XMPP_TYPE_PROBE },
	{ "subscribe",    NP_RT_XMPP_TYPE_SUBSCRIBE },
	{ "subscribed",   NP_RT_XMPP_TYPE_SUBSCRIBED },
	{ "unavailable",  NP_RT_XMPP_TYPE_UNAVAILABLE },
	{ "unsubscribe",  NP_RT_XMPP_TYPE_UNSUBSCRIBE },
	{ "unsubscribed", NP_RT_XMPP_TYPE_UNSUBSCRIBED },
	{ "standby",      NP_RT_XMPP_TYPE_STANDBY },
	{ "get",          NP_RT_XMPP_TYPE_GET },
	{ "result",       NP_RT_XMPP_TYPE_RESULT },
	{ "set",          NP_RT_XMPP_TYPE_SET },
	{ NULL,           -1 }
};

/* 0x81546248: XMPP presence <show> table */
static const XmppEnumEntry kPresenceShowTable[] = {
	{ "",     NP_RT_XMPP_SHOW_NONE },
	{ "away", NP_RT_XMPP_SHOW_AWAY },
	{ "chat", NP_RT_XMPP_SHOW_CHAT },
	{ "dnd",  NP_RT_XMPP_SHOW_DND },
	{ "xa",   NP_RT_XMPP_SHOW_XA },
	{ NULL,   -1 }
};

/* 0x81546278: XMPP xml:lang table */
static const XmppEnumEntry kLanguageTable[] = {
	{ "",   NP_RT_XMPP_LANG_NONE },
	{ "en", NP_RT_XMPP_LANG_EN },
	{ NULL, -1 }
};

/* 0x81546290: XMPP <error type="..."> table */
static const XmppEnumEntry kErrorTypeTable[] = {
	{ "",         NP_RT_XMPP_ERRTYPE_NONE },
	{ "cancel",   NP_RT_XMPP_ERRTYPE_CANCEL },
	{ "continue", NP_RT_XMPP_ERRTYPE_CONTINUE },
	{ "modify",   NP_RT_XMPP_ERRTYPE_MODIFY },
	{ "auth",     NP_RT_XMPP_ERRTYPE_AUTH },
	{ "wait",     NP_RT_XMPP_ERRTYPE_WAIT },
	{ NULL,       -1 }
};

/* 0x815462C8: XMPP stanza error condition table */
static const XmppEnumEntry kErrorConditionTable[] = {
	{ "",                        NP_RT_XMPP_COND_NONE },
	{ "bad-request",             NP_RT_XMPP_COND_BAD_REQUEST },
	{ "conflict",                NP_RT_XMPP_COND_CONFLICT },
	{ "feature-not-implemented", NP_RT_XMPP_COND_FEATURE_NOT_IMPL },
	{ "forbidden",               NP_RT_XMPP_COND_FORBIDDEN },
	{ "gone",                    NP_RT_XMPP_COND_GONE },
	{ "internal-server-error",   NP_RT_XMPP_COND_INTERNAL_SERVER_ERROR },
	{ "item-not-found",          NP_RT_XMPP_COND_ITEM_NOT_FOUND },
	{ "jid-malformed",           NP_RT_XMPP_COND_JID_MALFORMED },
	{ "not-acceptable",          NP_RT_XMPP_COND_NOT_ACCEPTABLE },
	{ "not-allowed",             NP_RT_XMPP_COND_NOT_ALLOWED },
	{ "not-authorized",          NP_RT_XMPP_COND_NOT_AUTHORIZED },
	{ "payment-required",        NP_RT_XMPP_COND_PAYMENT_REQUIRED },
	{ "recipient-unavailable",   NP_RT_XMPP_COND_RECIPIENT_UNAVAILABLE },
	{ "redirect",                NP_RT_XMPP_COND_REDIRECT },
	{ "registration-required",   NP_RT_XMPP_COND_REGISTRATION_REQUIRED },
	{ "remote-server-not-found", NP_RT_XMPP_COND_REMOTE_SERVER_NOT_FOUND },
	{ "remote-server-timeout",   NP_RT_XMPP_COND_REMOTE_SERVER_TIMEOUT },
	{ "resource-constraint",     NP_RT_XMPP_COND_RESOURCE_CONSTRAINT },
	{ "service-unavailable",     NP_RT_XMPP_COND_SERVICE_UNAVAILABLE },
	{ "subscription-required",   NP_RT_XMPP_COND_SUBSCRIPTION_REQUIRED },
	{ "undefined-condition",     NP_RT_XMPP_COND_UNDEFINED_CONDITION },
	{ "unexpected-request",      NP_RT_XMPP_COND_UNEXPECTED_REQUEST },
	{ NULL,                      -1 }
};

/* 0x81546388: PlayStation RT error source table */
static const XmppEnumEntry kErrorSourceTable[] = {
	{ "",       NP_RT_XMPP_SOURCE_NONE },
	{ "server", NP_RT_XMPP_SOURCE_SERVER },
	{ "client", NP_RT_XMPP_SOURCE_CLIENT },
	{ NULL,     -1 }
};

static int lookupEnumFromString(const XmppEnumEntry *table, int *outValue,
				const char *str, SceSize len)
{
	if (str == NULL) {
		*outValue = 0;
		return 0;
	}
	*outValue = -1;
	for (const XmppEnumEntry *cur = table; cur->name != NULL; ++cur) {
		if (sceClibStrncmp(str, cur->name, len) == 0) {
			*outValue = cur->value;
			return 0;
		}
	}
	return 0;
}

static int formatEnumToString(const XmppEnumEntry *table, int value, char *outBuf)
{
	if (outBuf == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	for (const XmppEnumEntry *cur = table; cur->name != NULL; ++cur) {
		if (cur->value == value) {
			SceSize len = sceClibStrnlen(cur->name, 0x7FFFFFFFu);
			sceClibMemcpy_safe(outBuf, cur->name, len);
			outBuf[len] = '\0';
			return (cur->value == -1) ? NP_RT_CORE_ERR_INVALID_ARG : 0;
		}
	}
	return NP_RT_CORE_ERR_INVALID_ARG;
}

static void freeStanzaHeaderFields(NpJid *fromJid, NpJid *toJid, char **stanzaId)
{
	npRtFreeJid(fromJid);
	npRtFreeJid(toJid);
	if (*stanzaId != NULL) {
		npRtHeapFree(*stanzaId, NULL);
		*stanzaId = NULL;
	}
}

static void destroyNodeList(BxceNodeList *nodeList)
{
	if (nodeList != NULL) {
		ScebXCe_1F59F0C4(nodeList);
		ScebXCe_EC560647(nodeList);
	}
}

} /* namespace */

/* --------------------------------------------------------------------------
 * XMPP Error Parsing (0x8130F3E4..0x8130F79F)
 * -------------------------------------------------------------------------- */
/* 0x8130F3E4 */
int npRtParseErrorDocument(NpXmppErrorStanza *outError, BxceDocument *xmlDoc,
			   const char *errorElemName, const char *errorXmlns,
			   SceSize xmlnsLen)
{
	if (outError == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;
	if (xmlDoc == NULL)
		return 0;

	sceClibMemset(outError, 0, sizeof(*outError));

	BxceNodeId rootNode = ScebXCe_75E1B095(xmlDoc);
	BxceNodeList *errNodes = ScebXCe_1FA24DC0(xmlDoc, rootNode, errorElemName);
	if (errNodes == NULL || errNodes->length == 0) {
		destroyNodeList(errNodes);
		return 0;
	}

	BxceNodeId errNode = ScebXCe_6C46D786(errNodes, 0);
	if (errNode == 0) {
		destroyNodeList(errNodes);
		return 0;
	}

	const char *typeAttr = ScebXCe_753D9382(xmlDoc, errNode, "type");
	if (typeAttr != NULL) {
		SceSize typeLen = sceClibStrnlen(typeAttr, 0x7FFFFFFFu);
		int ret = npRtParseErrorType(&outError->errorType, typeAttr, typeLen);
		if (ret < 0) {
			destroyNodeList(errNodes);
			return ret;
		}
	} else {
		outError->errorType = 0;
	}

	outError->condition = 0;
	outError->textList = NULL;
	outError->errorCode = 0;

	NpRtLangTextNode *textTail = NULL;
	bool conditionSet = false;
	int status = 0;

	for (BxceNodeId child = ScebXCe_FEA6E4A1(xmlDoc, errNode);
	     child != 0;
	     child = ScebXCe_6FC9DC3F(xmlDoc, child)) {
		const char *childXmlns = ScebXCe_753D9382(xmlDoc, child, "xmlns");
		if (childXmlns == NULL)
			continue;

		if (sceClibStrncmp(childXmlns, errorXmlns, xmlnsLen) == 0) {
			const char *tagName = ScebXCe_1189A2B1(xmlDoc, child);
			if (tagName == NULL)
				continue;

			if (sceClibStrncmp(tagName, "text", 5u) == 0) {
				BxceNodeId textChild = ScebXCe_FEA6E4A1(xmlDoc, child);
				if (textChild == 0)
					continue;
				const char *textVal = ScebXCe_33825EFF(xmlDoc, textChild);
				if (textVal == NULL)
					continue;

				NpRtLangTextNode *textNode = reinterpret_cast<NpRtLangTextNode *>(
					npRtHeapAlloc(sizeof(NpRtLangTextNode), NULL));
				if (textNode == NULL) {
					status = NP_RT_CORE_ERR_NO_MEMORY;
					break;
				}
				textNode->next = NULL;
				textNode->lang = 0;
				textNode->text = const_cast<char *>(textVal);

				const char *langAttr = ScebXCe_753D9382(xmlDoc, child, "xml:lang");
				if (langAttr != NULL) {
					SceSize langLen = sceClibStrnlen(langAttr, 0x7FFFFFFFu);
					status = npRtParseLanguage(&textNode->lang, langAttr, langLen);
					if (status < 0) {
						npRtHeapFree(textNode, NULL);
						break;
					}
				}

				if (textTail != NULL) {
					textTail->next = textNode;
					textTail = textNode;
				} else {
					outError->textList = textNode;
					textTail = textNode;
				}
			} else if (!conditionSet) {
				int condVal = -1;
				SceSize tagLen = sceClibStrnlen(tagName, 0x7FFFFFFFu);
				status = npRtParseErrorCondition(&condVal, tagName, tagLen);
				if (status < 0)
					break;
				if (condVal != -1 && condVal != 0) {
					outError->condition = condVal;
					conditionSet = true;
				}
			}
		} else if (sceClibStrncmp(childXmlns, "http://playstation.net/np/rt/error", 35u) == 0) {
			const char *tagName = ScebXCe_1189A2B1(xmlDoc, child);
			if (tagName != NULL && sceClibStrncmp(tagName, "x", 2u) == 0) {
				SceUInt8 codeByte = 0;
				const char *codeAttr = ScebXCe_753D9382(xmlDoc, child, "code");
				if (codeAttr != NULL) {
					codeByte = static_cast<SceUInt8>(
						SceNpCommon_5B770B80(codeAttr, NULL, 16));
				}
				outError->errorCode = codeByte;

				const char *sourceAttr = ScebXCe_753D9382(xmlDoc, child, "source");
				if (sourceAttr != NULL) {
					SceSize sourceLen = sceClibStrnlen(sourceAttr, 0x7FFFFFFFu);
					status = npRtParseErrorSource(&outError->errorKind,
								      sourceAttr, sourceLen);
					if (status < 0)
						break;
				} else {
					outError->errorKind = 0;
				}
			}
		}
	}

	destroyNodeList(errNodes);
	if (status < 0)
		npRtFreeXmppErrorTextList(outError);
	return status;
}

/* 0x8130F6B6 */
int npRtParseStreamError(NpXmppErrorStanza *outError, BxceDocument *xmlDoc)
{
	int ret = npRtParseErrorDocument(outError, xmlDoc, "stream:error",
					 "urn:ietf:params:xml:ns:xmpp-streams", 36u);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130F6DE */
void npRtFreeXmppErrorTextList(NpXmppErrorStanza *errorStanza)
{
	NpRtLangTextNode *cur = errorStanza->textList;
	while (cur != NULL) {
		NpRtLangTextNode *next = cur->next;
		npRtHeapFree(cur, NULL);
		cur = next;
	}
	errorStanza->textList = NULL;
}

/* 0x8130F6F2 */
int npRtParseMessageError(NpXmppErrorStanza *outError, const NpRtMessageStanza *stanza)
{
	if (stanza->stanzaType != NP_RT_XMPP_TYPE_ERROR)
		return NP_RT_CORE_ERR_INVALID_ARG;
	int ret = npRtParseErrorDocument(outError, stanza->xmlDoc, "error",
					 "urn:ietf:params:xml:ns:xmpp-stanzas", 36u);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130F72C */
int npRtParsePresenceError(NpXmppErrorStanza *outError, const NpRtPresenceStanza *stanza)
{
	if (stanza->stanzaType != NP_RT_XMPP_TYPE_ERROR)
		return NP_RT_CORE_ERR_INVALID_ARG;
	int ret = npRtParseErrorDocument(outError, stanza->xmlDoc, "error",
					 "urn:ietf:params:xml:ns:xmpp-stanzas", 36u);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8130F766 */
int npRtParseXmppError(NpXmppErrorStanza *outError, const NpRtStanza *stanza)
{
	if (stanza->stanzaType != NP_RT_XMPP_TYPE_ERROR)
		return NP_RT_CORE_ERR_INVALID_ARG;
	int ret = npRtParseErrorDocument(outError, stanza->xmlDoc, "error",
					 "urn:ietf:params:xml:ns:xmpp-stanzas", 36u);
	return (ret >= 0) ? 0 : ret;
}

/* --------------------------------------------------------------------------
 * Stanza Header and Ref-Counted Lifecycle (0x8130F7A0..0x8130FACF)
 * -------------------------------------------------------------------------- */
/* 0x8130F7A0 */
int npRtStanzaHeaderInit(NpRtStanzaInit *header, int stanzaType,
			 const NpJid *toJid, const NpJid *fromJid)
{
	sceClibMemset(header, 0, sizeof(*header));
	header->stanzaType = stanzaType;
	header->stanzaId = NULL;
	header->lang = 0;

	int ret = 0;
	if (toJid != NULL)
		ret = npRtCopyJid(&header->toJid, toJid);
	if (ret >= 0 && fromJid != NULL)
		ret = npRtCopyJid(&header->fromJid, fromJid);

	if (ret < 0) {
		char *idPtr = const_cast<char *>(header->stanzaId);
		freeStanzaHeaderFields(&header->fromJid, &header->toJid, &idPtr);
		header->stanzaId = idPtr;
	}
	return ret;
}

/* 0x8130F808 */
int npRtStanzaHeaderCopy(NpRtStanzaInit *dst, const NpRtStanzaInit *src)
{
	sceClibMemset(dst, 0, sizeof(*dst));

	char *copiedId = NULL;
	int ret = npRtCopyJid(&dst->fromJid, &src->fromJid);
	if (ret >= 0)
		ret = npRtCopyJid(&dst->toJid, &src->toJid);
	if (ret >= 0)
		ret = npRtDupString(&copiedId, src->stanzaId, NULL, 0);

	if (ret < 0) {
		freeStanzaHeaderFields(&dst->fromJid, &dst->toJid, &copiedId);
		dst->stanzaId = NULL;
		return ret;
	}

	dst->stanzaId = copiedId;
	dst->stanzaType = src->stanzaType;
	dst->lang = src->lang;
	return 0;
}

/* 0x8130F878 */
int npRtStanzaHeaderSetId(NpRtStanzaInit *header, const char *stanzaId)
{
	if (header->stanzaId != NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	char *copiedId = NULL;
	int ret = npRtDupString(&copiedId, stanzaId, NULL, 0);
	if (ret < 0)
		return ret;
	header->stanzaId = copiedId;
	return 0;
}

/* 0x8130F89C */
void npRtReleaseMessageStanza(NpRtMessageStanza *stanza)
{
	if (stanza == NULL)
		return;
	if (sceAtomicDecrement32AcqRel(&stanza->refCount) == 0) {
		freeStanzaHeaderFields(&stanza->fromJid, &stanza->toJid, &stanza->stanzaId);
		npRtFreeLangTextList(stanza->subjectList);
		npRtFreeLangTextList(stanza->bodyList);
		if (stanza->threadId != NULL)
			npRtHeapFree(stanza->threadId, NULL);
		npRtDestroyBxceDocument(stanza->xmlDoc);
		npRtHeapFree(stanza, NULL);
	}
}

/* 0x8130F90A */
void npRtReleasePresenceStanza(NpRtPresenceStanza *stanza)
{
	if (stanza == NULL)
		return;
	if (sceAtomicDecrement32AcqRel(&stanza->refCount) == 0) {
		freeStanzaHeaderFields(&stanza->fromJid, &stanza->toJid, &stanza->stanzaId);
		npRtFreeLangTextList(stanza->statusList);
		npRtDestroyBxceDocument(stanza->xmlDoc);
		npRtHeapFree(stanza, NULL);
	}
}

static int copyHeaderFields(NpJid *dstFrom, NpJid *dstTo, char **dstId,
			    int *dstType, int *dstLang, const NpRtStanzaInit *src)
{
	NpRtStanzaInit copiedHeader;
	int ret = npRtStanzaHeaderCopy(&copiedHeader, src);
	if (ret < 0)
		return ret;
	*dstFrom = copiedHeader.fromJid;
	*dstTo = copiedHeader.toJid;
	*dstId = const_cast<char *>(copiedHeader.stanzaId);
	*dstType = copiedHeader.stanzaType;
	*dstLang = copiedHeader.lang;
	return 0;
}

/* 0x8130F968 */
int npRtAllocPresenceStanza(NpRtPresenceStanza **outStanza, const NpRtStanzaInit *init)
{
	NpRtPresenceStanza *stanza = reinterpret_cast<NpRtPresenceStanza *>(
		npRtHeapAlloc(sizeof(NpRtPresenceStanza), NULL));
	if (stanza == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(stanza, 0, sizeof(*stanza));
	int ret = copyHeaderFields(&stanza->fromJid, &stanza->toJid, &stanza->stanzaId,
				   &stanza->stanzaType, &stanza->lang, init);
	if (ret < 0) {
		npRtReleasePresenceStanza(stanza);
		return ret;
	}
	*outStanza = stanza;
	return 0;
}

/* 0x8130F9AC */
void *npRtAddRefStanza(void *stanza)
{
	volatile SceInt32 *refPtr = reinterpret_cast<volatile SceInt32 *>(stanza);
	sceAtomicIncrement32AcqRel(refPtr);
	return stanza;
}

/* 0x8130F9C6 */
void npRtReleaseStanza(NpRtStanza *stanza)
{
	if (stanza == NULL)
		return;
	if (sceAtomicDecrement32AcqRel(&stanza->refCount) == 0) {
		freeStanzaHeaderFields(&stanza->fromJid, &stanza->toJid, &stanza->stanzaId);
		npRtDestroyBxceDocument(stanza->xmlDoc);
		npRtHeapFree(stanza, NULL);
	}
}

/* 0x8130FA1E */
int npRtAllocStanza(NpRtStanza **outStanza, const NpRtStanzaInit *init)
{
	NpRtStanza *stanza = reinterpret_cast<NpRtStanza *>(
		npRtHeapAlloc(sizeof(NpRtStanza), NULL));
	if (stanza == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(stanza, 0, sizeof(*stanza));
	int ret = copyHeaderFields(&stanza->fromJid, &stanza->toJid, &stanza->stanzaId,
				   &stanza->stanzaType, &stanza->lang, init);
	if (ret < 0) {
		npRtReleaseStanza(stanza);
		return ret;
	}
	*outStanza = stanza;
	return 0;
}

/* 0x8130FA64 */
void npRtDestroyBxceDocument(BxceDocument *xmlDoc)
{
	if (xmlDoc != NULL) {
		ScebXCe_B0891B13(xmlDoc);
		sceNpCommonFreeNpServerName(xmlDoc);
	}
}

/* 0x8130FA7A, 0x8130FAC8 */
int npRtReleaseAnyStanza(NpRtAnyStanza *anyStanza)
{
	switch (anyStanza->kind) {
	case NP_RT_STANZA_KIND_MESSAGE:
		npRtReleaseMessageStanza(
			reinterpret_cast<NpRtMessageStanza *>(anyStanza->stanza));
		break;
	case NP_RT_STANZA_KIND_PRESENCE:
		npRtReleasePresenceStanza(
			reinterpret_cast<NpRtPresenceStanza *>(anyStanza->stanza));
		break;
	case NP_RT_STANZA_KIND_IQ:
		npRtReleaseStanza(reinterpret_cast<NpRtStanza *>(anyStanza->stanza));
		break;
	case NP_RT_STANZA_KIND_STREAM:
		npRtDestroyBxceDocument(reinterpret_cast<BxceDocument *>(anyStanza->stanza));
		break;
	default:
		break;
	}
	anyStanza->kind = NP_RT_STANZA_KIND_NONE;
	anyStanza->stanza = NULL;
	return 0;
}

/* --------------------------------------------------------------------------
 * XMPP String <-> Enum Converters (0x8130FAD0..0x8130FE53)
 * -------------------------------------------------------------------------- */
/* 0x8130FAD0 */
int npRtParseStanzaType(int *outType, const char *str, SceSize len)
{
	return lookupEnumFromString(kStanzaTypeTable, outType, str, len);
}

/* 0x8130FB14 */
int npRtFormatStanzaType(int stanzaType, char *outBuf)
{
	return formatEnumToString(kStanzaTypeTable, stanzaType, outBuf);
}

/* 0x8130FB76 */
int npRtParsePresenceShow(int *outShow, const char *str, SceSize len)
{
	return lookupEnumFromString(kPresenceShowTable, outShow, str, len);
}

/* 0x8130FBBA */
int npRtFormatPresenceShow(int show, char *outBuf)
{
	return formatEnumToString(kPresenceShowTable, show, outBuf);
}

/* 0x8130FC1C */
int npRtParseLanguage(int *outLang, const char *str, SceSize len)
{
	return lookupEnumFromString(kLanguageTable, outLang, str, len);
}

/* 0x8130FC60 */
int npRtFormatLanguage(int lang, char *outBuf)
{
	return formatEnumToString(kLanguageTable, lang, outBuf);
}

/* 0x8130FCC2 */
int npRtParseErrorType(int *outErrorType, const char *str, SceSize len)
{
	return lookupEnumFromString(kErrorTypeTable, outErrorType, str, len);
}

/* 0x8130FD06 */
int npRtFormatErrorType(int errorType, char *outBuf)
{
	return formatEnumToString(kErrorTypeTable, errorType, outBuf);
}

/* 0x8130FD68 */
int npRtParseErrorCondition(int *outCondition, const char *str, SceSize len)
{
	return lookupEnumFromString(kErrorConditionTable, outCondition, str, len);
}

/* 0x8130FDAC */
int npRtFormatErrorCondition(int condition, char *outBuf)
{
	return formatEnumToString(kErrorConditionTable, condition, outBuf);
}

/* 0x8130FE0E */
int npRtParseErrorSource(int *outSource, const char *str, SceSize len)
{
	return lookupEnumFromString(kErrorSourceTable, outSource, str, len);
}

/* --------------------------------------------------------------------------
 * bXCe DOM Utilities (0x8130FE54..0x8131031F)
 * -------------------------------------------------------------------------- */
/* 0x8130FE54 */
int npRtCollectNamespaces(BxceDocument *xmlDoc, const char **outNamespaces, SceUInt32 *outCount)
{
	if (xmlDoc == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;
	if (outNamespaces == NULL && outCount == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	SceUInt32 count = 0;
	for (BxceNodeId child = ScebXCe_CED6BECF(xmlDoc);
	     child != 0;
	     child = ScebXCe_6FC9DC3F(xmlDoc, child)) {
		for (BxceNodeId attr = ScebXCe_B034C595(xmlDoc, child);
		     attr != 0;
		     attr = ScebXCe_134F491C(xmlDoc, attr)) {
			const char *attrName = ScebXCe_B4989A58(xmlDoc, attr);
			if (sceClibStrncmp(attrName, "xmlns", 6u) == 0) {
				if (outNamespaces != NULL)
					outNamespaces[count] = ScebXCe_84383D8E(xmlDoc, attr);
				++count;
			}
		}
	}

	if (outCount != NULL)
		*outCount = count;
	return 0;
}

/* 0x8130FF08 */
int npRtMatchElementName(BxceDocument *xmlDoc, BxceNodeId nodeId, const char *expectedName,
			 int *outMatched)
{
	*outMatched = 0;
	if (nodeId == 0)
		return NP_RT_CORE_ERR_INTERNAL;

	int nodeType = ScebXCe_FA18A039(xmlDoc, nodeId);
	if (nodeType != 5 && nodeType != 4)
		return NP_RT_CORE_ERR_INTERNAL;

	const char *elemName = ScebXCe_1189A2B1(xmlDoc, nodeId);
	SceSize expectedLen = sceClibStrnlen(expectedName, 0x7FFFFFFFu);
	*outMatched = (sceClibStrncmp(elemName, expectedName, expectedLen) == 0) ? 1 : 0;
	return 0;
}

/* 0x8130FF76 */
int npRtGetElementText(BxceDocument *xmlDoc, BxceNodeId parentNodeId, const char *tagName,
		       const char **outText)
{
	*outText = NULL;
	BxceNodeList *nodeList = ScebXCe_1FA24DC0(xmlDoc, parentNodeId, tagName);
	if (nodeList == NULL)
		return 0;

	if (nodeList->length != 0) {
		BxceNodeId elemNode = ScebXCe_6C46D786(nodeList, 0);
		if (elemNode != 0) {
			BxceNodeId textNode = ScebXCe_FEA6E4A1(xmlDoc, elemNode);
			if (textNode != 0)
				*outText = ScebXCe_33825EFF(xmlDoc, textNode);
		}
	}
	destroyNodeList(nodeList);
	return 0;
}

/* 0x8130FFBC */
int npRtAttrListAddString(BxceAttributeList *attrList, const char *attrName, const char *value)
{
	return (ScebXCe_3DD761CF(attrList, attrName, value) != 0) ? NP_RT_CORE_ERR_XML_ATTR : 0;
}

/* 0x8130FFD6 */
int npRtAttrListAddUInt64(BxceAttributeList *attrList, const char *attrName, SceUInt64 value)
{
	SceSize formattedLen = 0;
	int ret = npRtFormatUInt64(value, NULL, 0, 10, &formattedLen);
	if (ret < 0)
		return ret;
	if (formattedLen == 0)
		return 0;

	char *numBuf = reinterpret_cast<char *>(npRtHeapAlloc(formattedLen + 1, NULL));
	if (numBuf == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	ret = npRtFormatUInt64(value, numBuf, formattedLen + 1, 10, NULL);
	if (ret >= 0) {
		if (ScebXCe_3DD761CF(attrList, attrName, numBuf) != 0)
			ret = NP_RT_CORE_ERR_XML_ATTR;
		else
			ret = 0;
	}
	npRtHeapFree(numBuf, NULL);
	return ret;
}

/* 0x8131008C */
int npRtCreateAttrList(BxceAttributeList **outAttrList)
{
	BxceAttributeList *list = reinterpret_cast<BxceAttributeList *>(
		sceNpCommonMallocNpServerName(sizeof(BxceAttributeList)));
	*outAttrList = list;
	if (list == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_E7C8B20D(list) != 0) {
		sceNpCommonFreeNpServerName(list);
		*outAttrList = NULL;
		return NP_RT_CORE_ERR_INTERNAL;
	}
	return 0;
}

/* 0x813100C6 */
void npRtDestroyAttrList(BxceAttributeList *attrList)
{
	if (attrList != NULL) {
		ScebXCe_9C2F9897(attrList);
		sceNpCommonFreeNpServerName(attrList);
	}
}

/* 0x813100DA */
int npRtCreateXmlDocWithRoot(BxceDocument **outDoc, BxceNodeId *outRootNode,
			     const char *rootTagName, const char *rootXmlns)
{
	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != 0) {
		sceNpCommonFreeNpServerName(doc);
		return NP_RT_CORE_ERR_INTERNAL;
	}

	BxceAttributeList *attrList = NULL;
	int ret = npRtCreateAttrList(&attrList);
	if (ret < 0) {
		npRtDestroyBxceDocument(doc);
		return ret;
	}

	if (ScebXCe_3DD761CF(attrList, "xmlns", rootXmlns) != 0) {
		npRtDestroyAttrList(attrList);
		npRtDestroyBxceDocument(doc);
		return NP_RT_CORE_ERR_INTERNAL;
	}

	BxceNodeId docRoot = ScebXCe_75E1B095(doc);
	BxceNodeId rootElem = ScebXCe_AC0E4F67(doc, docRoot, rootTagName, attrList, 0, 0);
	npRtDestroyAttrList(attrList);

	if (rootElem == 0) {
		npRtDestroyBxceDocument(doc);
		return NP_RT_CORE_ERR_INTERNAL;
	}

	*outDoc = doc;
	if (outRootNode != NULL)
		*outRootNode = rootElem;
	return 0;
}

/* 0x813101CA */
int npRtFindElementWithXmlns(BxceDocument *xmlDoc, BxceNodeId *outNode,
			     const char *tagName, const char *expectedXmlns)
{
	BxceNodeId docRoot = ScebXCe_75E1B095(xmlDoc);
	BxceNodeList *nodeList = ScebXCe_1FA24DC0(xmlDoc, docRoot, tagName);
	if (nodeList == NULL)
		return NP_RT_CORE_ERR_NOT_FOUND;

	BxceNodeId foundNode = 0;
	if (nodeList->length != 0)
		foundNode = ScebXCe_6C46D786(nodeList, 0);
	destroyNodeList(nodeList);

	if (foundNode == 0)
		return NP_RT_CORE_ERR_NOT_FOUND;

	const char *actualXmlns = ScebXCe_753D9382(xmlDoc, foundNode, "xmlns");
	SceSize expectedLen = sceClibStrnlen(expectedXmlns, 0x7FFFFFFFu);
	if (actualXmlns != NULL && sceClibStrncmp(actualXmlns, expectedXmlns, expectedLen + 1) == 0) {
		*outNode = foundNode;
		return 0;
	}
	return NP_RT_CORE_ERR_NOT_FOUND;
}

/* 0x81310252 */
int npRtCollectExactXmlns(BxceDocument *xmlDoc, const char **outNamespaces, SceUInt32 *outCount)
{
	if (xmlDoc == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;
	if (outNamespaces == NULL && outCount == NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	SceUInt32 count = 0;
	SceSize keyLen = sceClibStrnlen("xmlns", 0x7FFFFFFFu);

	for (BxceNodeId child = ScebXCe_CED6BECF(xmlDoc);
	     child != 0;
	     child = ScebXCe_6FC9DC3F(xmlDoc, child)) {
		for (BxceNodeId attr = ScebXCe_B034C595(xmlDoc, child);
		     attr != 0;
		     attr = ScebXCe_134F491C(xmlDoc, attr)) {
			const char *attrName = ScebXCe_B4989A58(xmlDoc, attr);
			if (sceClibStrncmp(attrName, "xmlns", keyLen + 1) == 0) {
				if (outNamespaces != NULL)
					outNamespaces[count] = ScebXCe_84383D8E(xmlDoc, attr);
				++count;
			}
		}
	}

	if (outCount != NULL)
		*outCount = count;
	return 0;
}
