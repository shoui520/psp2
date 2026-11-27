/*
 * np_rt_xmpp.cpp: SceNetXmpp incremental SAX stanza parser (XmppSaxParser),
 * 40-second whitespace keepalive, TLS/SSL + Epoll + TCP transport, and XMPP
 * stream client (0x8131370C..0x81313DE3, 0x81314548..0x8131616B,
 * 0x81316970..0x81316C1B).
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <net/socket.h>
#include <net/epoll.h>
#include <net/resolver.h>
#include <net/inet.h>
#include <net/errno.h>
extern "C" {
#include <libssl/sslinternal.h>
}
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_core.h"

extern "C" {
extern int SceHttp_A9A39BA1(void *sslCtx, void *arg);
extern void sce_paf_memset(void *dst, int ch, SceSize len);
}

namespace {

static const SceSize kXmppRecvBufSize = 2048;
static const SceSize kStreamOpenBufSize = 512;
static const int kXmppKeepAliveIntervalUsec = 40000000;	/* 40s */
static const int kXmppKeepAliveSlackUsec = 1000000;	/* 1s */
static const int kXmppRecvTimeoutUsec = 120000000;	/* 120s */
static const int kXmppSendTimeoutUsec = 10000000;	/* 10s */

static const SceUInt32 kSslCtxOptions = 0x22001FFBu;
static const SceUInt32 kSslConnMode = 0x00040000u;
static const int kSslCtrlSessionCache = 44;
static const SceUInt32 kHttpsVerifyFlags = 101u;

static const int kNetErrAborted = -2143223548;		/* 0x80410104 */
static const int kNetErrWouldBlock = -2143223492;	/* 0x8041013C */
static const int kNetErrTimedOut = -2143223517;		/* 0x80410123 */

/* 40-byte SSL verification context passed to SceHttp_A9A39BA1 */
struct NpXmppSslVerifyCtx {
	SceUInt32 sslFlags;
	const char *hostname;
	int *sslError;
	SceUInt32 *sslErrorDetail;
	void *callback;
	void *userArg;
	void *(*alloc)(void *arg, SceSize size);
	void *(*realloc)(void *arg, void *ptr, SceSize size);
	void (*free)(void *arg, void *ptr);
	void *allocatorArg;
};

/* Active parse sub-mode inside XmppStanzaBuilder */
enum XmppBuilderMode {
	XMPP_BMODE_ROOT			= 0,
	XMPP_BMODE_MSG_BODY		= 1,
	XMPP_BMODE_MSG_SUBJECT		= 2,
	XMPP_BMODE_MSG_THREAD		= 3,
	XMPP_BMODE_MSG_EXT_DOM		= 4,
	XMPP_BMODE_PRES_SHOW		= 5,
	XMPP_BMODE_PRES_STATUS		= 6,
	XMPP_BMODE_PRES_PRIORITY	= 7,
	XMPP_BMODE_PRES_EXT_DOM		= 8,
	XMPP_BMODE_IQ_DOM		= 9,
	XMPP_BMODE_STREAM_DOM		= 10
};

struct XmppNodeStackEntry {
	BxceNodeId node;
	XmppNodeStackEntry *next;
};

/* Incremental stanza builder (24 bytes, 0x81314BEC..0x8131569F) */
struct XmppStanzaBuilder {
	void *stanzaOrDoc;		/* +0x00 */
	int kind;			/* +0x04: NpRtStanzaKind */
	int parseMode;			/* +0x08: XmppBuilderMode */
	BxceNodeId currentNode;		/* +0x0C */
	BxceNodeId docRootNode;		/* +0x10 */
	XmppNodeStackEntry *parentStack; /* +0x14 */
};

struct XmppQueueNode {
	XmppStanzaBuilder *builder;
	XmppQueueNode *next;
};

struct XmppBuilderQueue {
	XmppQueueNode *head;
	XmppQueueNode *tail;
};

static void enqueueCompletedBuilder(XmppBuilderQueue *queue, XmppStanzaBuilder *builder)
{
	XmppQueueNode *node = reinterpret_cast<XmppQueueNode *>(
		sceNpCommonMallocNpServerName(sizeof(XmppQueueNode)));
	if (node == NULL)
		return;

	node->builder = builder;
	node->next = NULL;
	if (queue->head != NULL)
		queue->tail->next = node;
	else
		queue->head = node;
	queue->tail = node;
}

/* 0x81314BEC */
static int builderInit(XmppStanzaBuilder *builder, int kind)
{
	builder->kind = kind;
	builder->parseMode = XMPP_BMODE_ROOT;

	if (kind == NP_RT_STANZA_KIND_STREAM_END) {
		builder->stanzaOrDoc = NULL;
		builder->currentNode = 0;
		builder->docRootNode = 0;
		builder->parentStack = NULL;
		return 0;
	}

	BxceDocument *doc = reinterpret_cast<BxceDocument *>(
		sceNpCommonMallocNpServerName(sizeof(BxceDocument)));
	if (doc == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	if (ScebXCe_FAB50608(doc, 1) != SCE_BXCE_ERROR_NO_MEMORY) {
		bool allocOk = false;
		switch (kind) {
		case NP_RT_STANZA_KIND_IQ: {
			NpRtStanza *iq = reinterpret_cast<NpRtStanza *>(
				npRtHeapAlloc(sizeof(NpRtStanza), NULL));
			builder->stanzaOrDoc = iq;
			if (iq != NULL) {
				sceClibMemset(iq, 0, sizeof(*iq));
				iq->xmlDoc = doc;
				allocOk = true;
			}
			break;
		}
		case NP_RT_STANZA_KIND_PRESENCE: {
			NpRtPresenceStanza *pres = reinterpret_cast<NpRtPresenceStanza *>(
				npRtHeapAlloc(sizeof(NpRtPresenceStanza), NULL));
			builder->stanzaOrDoc = pres;
			if (pres != NULL) {
				sceClibMemset(pres, 0, sizeof(*pres));
				pres->xmlDoc = doc;
				allocOk = true;
			}
			break;
		}
		case NP_RT_STANZA_KIND_MESSAGE: {
			NpRtMessageStanza *msg = reinterpret_cast<NpRtMessageStanza *>(
				npRtHeapAlloc(sizeof(NpRtMessageStanza), NULL));
			builder->stanzaOrDoc = msg;
			if (msg != NULL) {
				sceClibMemset(msg, 0, sizeof(*msg));
				msg->xmlDoc = doc;
				allocOk = true;
			}
			break;
		}
		default:
			builder->stanzaOrDoc = doc;
			allocOk = true;
			break;
		}

		if (allocOk) {
			BxceNodeId docRoot = ScebXCe_75E1B095(doc);
			builder->docRootNode = docRoot;
			builder->currentNode = docRoot;
			builder->parentStack = NULL;
			return 0;
		}
	}

	ScebXCe_B0891B13(doc);
	sceNpCommonFreeNpServerName(doc);
	return NP_XMPP_ERR_NO_MEMORY;
}

/* 0x81314CBE */
static void builderDestroy(XmppStanzaBuilder *builder)
{
	if (builder->kind == NP_RT_STANZA_KIND_STREAM_END)
		return;

	switch (builder->kind) {
	case NP_RT_STANZA_KIND_IQ:
		if (builder->stanzaOrDoc != NULL)
			npRtReleaseStanza(reinterpret_cast<NpRtStanza *>(builder->stanzaOrDoc));
		break;
	case NP_RT_STANZA_KIND_PRESENCE:
		if (builder->stanzaOrDoc != NULL)
			npRtReleasePresenceStanza(
				reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc));
		break;
	case NP_RT_STANZA_KIND_MESSAGE:
		if (builder->stanzaOrDoc != NULL)
			npRtReleaseMessageStanza(
				reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc));
		break;
	default:
		if (builder->stanzaOrDoc != NULL) {
			ScebXCe_B0891B13(reinterpret_cast<BxceDocument *>(builder->stanzaOrDoc));
			sceNpCommonFreeNpServerName(builder->stanzaOrDoc);
		}
		break;
	}
}

/* 0x81314D04 */
static int builderPushDomElement(XmppStanzaBuilder *builder, const char *name,
				 const BxceAttributeList *attrList)
{
	BxceDocument *doc = NULL;
	switch (builder->kind) {
	case NP_RT_STANZA_KIND_MESSAGE:
		builder->parseMode = XMPP_BMODE_MSG_EXT_DOM;
		doc = reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc)->xmlDoc;
		break;
	case NP_RT_STANZA_KIND_PRESENCE:
		builder->parseMode = XMPP_BMODE_PRES_EXT_DOM;
		doc = reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc)->xmlDoc;
		break;
	case NP_RT_STANZA_KIND_IQ:
		builder->parseMode = XMPP_BMODE_IQ_DOM;
		doc = reinterpret_cast<NpRtStanza *>(builder->stanzaOrDoc)->xmlDoc;
		break;
	default:
		builder->parseMode = XMPP_BMODE_STREAM_DOM;
		doc = reinterpret_cast<BxceDocument *>(builder->stanzaOrDoc);
		break;
	}

	BxceNodeId newNode = ScebXCe_AC0E4F67(doc, builder->currentNode, name, attrList, NULL, 0);
	if (newNode == 0)
		return NP_XMPP_ERR_NO_MEMORY;

	XmppNodeStackEntry *entry = reinterpret_cast<XmppNodeStackEntry *>(
		sceNpCommonMallocNpServerName(sizeof(XmppNodeStackEntry)));
	entry->node = builder->currentNode;
	entry->next = builder->parentStack;
	builder->parentStack = entry;
	builder->currentNode = newNode;
	return 0;
}

static int appendEmptyLangTextNode(NpRtLangTextNode **headPtr, const BxceAttributeList *attrList)
{
	int langVal = 0;
	BxceAttr *langAttr = ScebXCe_2AD8DA26(attrList, "lang", 4);
	if (langAttr != NULL)
		npRtParseLanguage(&langVal, langAttr->value,
				  static_cast<SceSize>(langAttr->valueLength));

	NpRtLangTextNode *node = reinterpret_cast<NpRtLangTextNode *>(
		npRtHeapAlloc(sizeof(NpRtLangTextNode), NULL));
	if (node == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	sceClibMemset(node, 0, sizeof(*node));
	node->lang = langVal;

	if (*headPtr == NULL) {
		*headPtr = node;
	} else {
		NpRtLangTextNode *tail = *headPtr;
		while (tail->next != NULL)
			tail = tail->next;
		tail->next = node;
	}
	return 0;
}

/* 0x81314D86 */
static int builderStartPresenceChild(XmppStanzaBuilder *builder, NpRtPresenceStanza *stanza,
				     const char *name, const BxceAttributeList *attrList)
{
	if (sceClibStrncmp(name, "show", 5u) == 0) {
		builder->parseMode = XMPP_BMODE_PRES_SHOW;
		return 0;
	}
	if (sceClibStrncmp(name, "status", 7u) == 0) {
		appendEmptyLangTextNode(&stanza->statusList, attrList);
		builder->parseMode = XMPP_BMODE_PRES_STATUS;
		return 0;
	}
	if (sceClibStrncmp(name, "priority", 9u) == 0) {
		builder->parseMode = XMPP_BMODE_PRES_PRIORITY;
		return 0;
	}
	builderPushDomElement(builder, name, attrList);
	return 0;
}

/* 0x81314E7E */
static int builderStartMessageChild(XmppStanzaBuilder *builder, NpRtMessageStanza *stanza,
				    const char *name, const BxceAttributeList *attrList)
{
	if (sceClibStrncmp(name, "body", 5u) == 0) {
		int ret = appendEmptyLangTextNode(&stanza->bodyList, attrList);
		builder->parseMode = XMPP_BMODE_MSG_BODY;
		return ret;
	}
	if (sceClibStrncmp(name, "subject", 8u) == 0) {
		int ret = appendEmptyLangTextNode(&stanza->subjectList, attrList);
		builder->parseMode = XMPP_BMODE_MSG_SUBJECT;
		return ret;
	}
	if (sceClibStrncmp(name, "thread", 7u) == 0) {
		builder->parseMode = XMPP_BMODE_MSG_THREAD;
		return 0;
	}
	return builderPushDomElement(builder, name, attrList);
}

/* 0x81315036 */
static int builderStartChild(XmppStanzaBuilder *builder, const char *name,
			     const BxceAttributeList *attrList)
{
	if (builder->parseMode == XMPP_BMODE_MSG_EXT_DOM ||
	    (builder->parseMode >= XMPP_BMODE_PRES_EXT_DOM &&
	     builder->parseMode <= XMPP_BMODE_STREAM_DOM)) {
		return builderPushDomElement(builder, name, attrList);
	}

	switch (builder->kind) {
	case NP_RT_STANZA_KIND_MESSAGE:
		return builderStartMessageChild(
			builder, reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc),
			name, attrList);
	case NP_RT_STANZA_KIND_PRESENCE:
		return builderStartPresenceChild(
			builder, reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc),
			name, attrList);
	case NP_RT_STANZA_KIND_IQ:
		builder->parseMode = XMPP_BMODE_IQ_DOM;
		return builderPushDomElement(builder, name, attrList);
	default:
		return builderPushDomElement(builder, name, attrList);
	}
}

static int parseJidAttribute(NpJid *outJid, const char *jidStr)
{
	SceSize required = 0;
	int ret = SceNpCommon_D2B9BCA8(NULL, jidStr, NULL, 0, &required);
	if (ret < 0) {
		sceClibPrintf("error parsing URI... (0x%x)\n\n", ret);
		return ret;
	}

	char *pool = reinterpret_cast<char *>(npRtHeapAlloc(required, NULL));
	if (pool == NULL) {
		sceClibPrintf("failed to malloc pool\n");
		return 0;
	}

	ret = SceNpCommon_D2B9BCA8(outJid, jidStr, pool, required, NULL);
	if (ret < 0) {
		sceClibPrintf("error parsing JID... (0x%x)\n\n", ret);
		npRtHeapFree(pool, NULL);
		return ret;
	}
	return 0;
}

/* 0x813150C0 */
static int builderStartRoot(XmppStanzaBuilder *builder, const char *name,
			    const BxceAttributeList *attrList)
{
	NpJid *toJid = NULL;
	NpJid *fromJid = NULL;
	char **stanzaIdPtr = NULL;
	int *stanzaTypePtr = NULL;
	int *langPtr = NULL;

	switch (builder->kind) {
	case NP_RT_STANZA_KIND_MESSAGE: {
		NpRtMessageStanza *msg = reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc);
		builder->parseMode = XMPP_BMODE_ROOT;
		toJid = &msg->toJid;
		fromJid = &msg->fromJid;
		stanzaIdPtr = &msg->stanzaId;
		stanzaTypePtr = &msg->stanzaType;
		langPtr = &msg->lang;
		break;
	}
	case NP_RT_STANZA_KIND_PRESENCE: {
		NpRtPresenceStanza *pres =
			reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc);
		builder->parseMode = XMPP_BMODE_ROOT;
		toJid = &pres->toJid;
		fromJid = &pres->fromJid;
		stanzaIdPtr = &pres->stanzaId;
		stanzaTypePtr = &pres->stanzaType;
		langPtr = &pres->lang;
		break;
	}
	case NP_RT_STANZA_KIND_IQ: {
		NpRtStanza *iq = reinterpret_cast<NpRtStanza *>(builder->stanzaOrDoc);
		builder->parseMode = XMPP_BMODE_IQ_DOM;
		toJid = &iq->toJid;
		fromJid = &iq->fromJid;
		stanzaIdPtr = &iq->stanzaId;
		stanzaTypePtr = &iq->stanzaType;
		langPtr = &iq->lang;
		break;
	}
	default:
		builder->parseMode = XMPP_BMODE_STREAM_DOM;
		return builderPushDomElement(builder, name, attrList);
	}

	int status = 0;
	BxceAttr *toAttr = ScebXCe_2AD8DA26(attrList, "to", 2);
	if (toAttr != NULL)
		status = parseJidAttribute(toJid, toAttr->value);

	BxceAttr *fromAttr = ScebXCe_2AD8DA26(attrList, "from", 4);
	if (fromAttr != NULL)
		status = parseJidAttribute(fromJid, fromAttr->value);

	BxceAttr *idAttr = ScebXCe_2AD8DA26(attrList, "id", 2);
	if (idAttr != NULL) {
		SceSize idLen = static_cast<SceSize>(idAttr->valueLength);
		char *idCopy = reinterpret_cast<char *>(npRtHeapAlloc(idLen + 1, NULL));
		if (idCopy != NULL) {
			sceClibMemcpy_safe(idCopy, idAttr->value, idLen);
			idCopy[idLen] = '\0';
		}
		*stanzaIdPtr = idCopy;
	}

	BxceAttr *typeAttr = ScebXCe_2AD8DA26(attrList, "type", 4);
	if (typeAttr != NULL)
		status = npRtParseStanzaType(stanzaTypePtr, typeAttr->value,
					     static_cast<SceSize>(typeAttr->valueLength));

	BxceAttr *langAttr = ScebXCe_2AD8DA26(attrList, "lang", 4);
	if (langAttr != NULL)
		return npRtParseLanguage(langPtr, langAttr->value,
					 static_cast<SceSize>(langAttr->valueLength));

	return status;
}

/* 0x813152D4 */
static int builderAppendDomText(XmppStanzaBuilder *builder, BxceDocument *doc,
				const char *text, SceSize len)
{
	BxceNodeId existing = ScebXCe_FEA6E4A1(doc, builder->currentNode);
	if (existing == 0) {
		BxceNodeId textNode = ScebXCe_AEB2CE13(doc, text, len);
		if (textNode == 0)
			return NP_XMPP_ERR_NO_MEMORY;
		if (ScebXCe_3BF2F11B(doc, builder->currentNode, 0, textNode) == 0) {
			ScebXCe_EC560647(reinterpret_cast<void *>(textNode));
			return NP_XMPP_ERR_NO_MEMORY;
		}
		return 0;
	}

	const char *oldText = ScebXCe_33825EFF(doc, existing);
	SceSize oldLen = sceClibStrnlen(oldText, 0x7FFFFFFFu);
	SceSize combinedLen = oldLen + len;

	char *combined = reinterpret_cast<char *>(npRtHeapAlloc(combinedLen + 1, NULL));
	if (combined == NULL)
		return 0;

	sceClibMemcpy_safe(combined, ScebXCe_33825EFF(doc, existing), oldLen);
	sceClibMemcpy_safe(combined + oldLen, text, len);
	combined[combinedLen] = '\0';

	BxceNodeId newTextNode = ScebXCe_AEB2CE13(doc, combined, combinedLen);
	combined[0] = '\0';
	npRtHeapFree(combined, NULL);
	if (newTextNode == 0)
		return NP_XMPP_ERR_NO_MEMORY;

	ScebXCe_568CE24D(doc, existing, builder->currentNode);
	if (ScebXCe_3BF2F11B(doc, builder->currentNode, 0, newTextNode) != 0) {
		ScebXCe_2C2628BB(doc, existing);
		return 0;
	}

	ScebXCe_2C2628BB(doc, newTextNode);
	if (ScebXCe_3BF2F11B(doc, builder->currentNode, 0, existing) == 0)
		ScebXCe_2C2628BB(doc, existing);
	return NP_XMPP_ERR_NO_MEMORY;
}

/* 0x813153F6 */
static int builderAppendLangText(NpRtLangTextNode *head, const char *text, SceSize len)
{
	NpRtLangTextNode *tail = head;
	while (tail->next != NULL)
		tail = tail->next;

	SceSize unescapedSize = 0;
	int ret = SceNpCommon_389F8C14(
		NULL, 0, reinterpret_cast<const SceUInt8 *>(text), len, &unescapedSize);
	if (ret < 0)
		return ret;

	if (tail->text != NULL) {
		SceSize oldLen = sceClibStrnlen(tail->text, 0x7FFFFFFFu);
		char *merged = reinterpret_cast<char *>(
			npRtHeapAlloc(oldLen + unescapedSize, NULL));
		if (merged != NULL) {
			sceClibMemcpy_safe(merged, tail->text, oldLen);
			ret = SceNpCommon_389F8C14(
				merged + oldLen, unescapedSize,
				reinterpret_cast<const SceUInt8 *>(text), len, NULL);
			if (ret < 0) {
				npRtHeapFree(merged, NULL);
				return ret;
			}
			npRtHeapFree(tail->text, NULL);
			tail->text = merged;
		}
		return 0;
	}

	char *decoded = reinterpret_cast<char *>(npRtHeapAlloc(unescapedSize, NULL));
	if (decoded != NULL) {
		ret = SceNpCommon_389F8C14(
			decoded, unescapedSize,
			reinterpret_cast<const SceUInt8 *>(text), len, NULL);
		if (ret < 0) {
			npRtHeapFree(decoded, NULL);
			tail->text = NULL;
			return ret;
		}
	}
	tail->text = decoded;
	return 0;
}

/* 0x813154F2 */
static int builderHandleCharacters(XmppStanzaBuilder *builder, const char *ch, SceSize len)
{
	if (builder->currentNode == 0)
		return NP_XMPP_ERR_XML;

	switch (builder->parseMode) {
	case XMPP_BMODE_MSG_BODY:
		return builderAppendLangText(
			reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc)->bodyList,
			ch, len);
	case XMPP_BMODE_PRES_STATUS:
		return builderAppendLangText(
			reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc)->statusList,
			ch, len);
	case XMPP_BMODE_MSG_SUBJECT:
		return builderAppendLangText(
			reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc)->subjectList,
			ch, len);
	case XMPP_BMODE_MSG_THREAD: {
		NpRtMessageStanza *msg =
			reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc);
		if (msg->threadId == NULL) {
			char *copy = reinterpret_cast<char *>(npRtHeapAlloc(len + 1, NULL));
			if (copy != NULL) {
				sceClibMemcpy_safe(copy, ch, len);
				copy[len] = '\0';
				msg->threadId = copy;
			}
		}
		return 0;
	}
	case XMPP_BMODE_MSG_EXT_DOM:
		return builderAppendDomText(
			builder,
			reinterpret_cast<NpRtMessageStanza *>(builder->stanzaOrDoc)->xmlDoc,
			ch, len);
	case XMPP_BMODE_PRES_SHOW:
		npRtParsePresenceShow(
			&reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc)->show,
			ch, len);
		return 0;
	case XMPP_BMODE_PRES_PRIORITY:
		reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc)->priority =
			static_cast<SceInt8>(SceNpCommon_4C4AA451(ch, NULL, 10));
		return 0;
	case XMPP_BMODE_PRES_EXT_DOM:
		return builderAppendDomText(
			builder,
			reinterpret_cast<NpRtPresenceStanza *>(builder->stanzaOrDoc)->xmlDoc,
			ch, len);
	case XMPP_BMODE_IQ_DOM:
		return builderAppendDomText(
			builder,
			reinterpret_cast<NpRtStanza *>(builder->stanzaOrDoc)->xmlDoc,
			ch, len);
	case XMPP_BMODE_STREAM_DOM:
		return builderAppendDomText(
			builder,
			reinterpret_cast<BxceDocument *>(builder->stanzaOrDoc),
			ch, len);
	default:
		return 0;
	}
}

/* 0x813155FC */
static int builderEndElement(XmppStanzaBuilder *builder)
{
	if (builder->parseMode == XMPP_BMODE_MSG_EXT_DOM ||
	    (builder->parseMode >= XMPP_BMODE_PRES_EXT_DOM &&
	     builder->parseMode <= XMPP_BMODE_STREAM_DOM)) {
		XmppNodeStackEntry *top = builder->parentStack;
		BxceNodeId popped = 0;
		if (top != NULL) {
			builder->parentStack = top->next;
			popped = top->node;
			sceNpCommonFreeNpServerName(top);
		}
		if (popped != 0)
			builder->currentNode = popped;
		else
			builder->parseMode = XMPP_BMODE_ROOT;
	}

	if (builder->currentNode == builder->docRootNode)
		builder->parseMode = XMPP_BMODE_ROOT;
	return builder->kind;
}

/* 0x8131564C */
static int builderTakeStanza(XmppStanzaBuilder *builder, void **outStanza)
{
	if (outStanza == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	if (builder->kind == NP_RT_STANZA_KIND_STREAM_END)
		*outStanza = NULL;
	else
		*outStanza = builder->stanzaOrDoc;
	builder->stanzaOrDoc = NULL;
	return 0;
}

/* 0x81315694 */
static bool builderIsStackEmpty(const XmppStanzaBuilder *builder)
{
	return builder->parentStack == NULL;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * XmppSaxParser (vtable 0x8153CD90, 0x81314548..0x81314BE8)
 * -------------------------------------------------------------------------- */
class XmppSaxParser : public BxceDocumentHandler {
public:
	/* 0x81314638 */
	XmppSaxParser()
		: m_saxParser(NULL),
		  m_currentBuilder(NULL),
		  m_completedQueue(NULL),
		  m_savedSlot(-1),
		  m_depth(0),
		  m_lastError(0)
	{
	}

	virtual ~XmppSaxParser() {}

	/* 0x81314658 */
	int initialize()
	{
		m_completedQueue = NULL;
		m_saxParser = NULL;

		m_completedQueue = reinterpret_cast<XmppBuilderQueue *>(
			sceNpCommonMallocNpServerName(sizeof(XmppBuilderQueue)));
		if (m_completedQueue != NULL) {
			m_completedQueue->head = NULL;
			m_completedQueue->tail = NULL;

			m_saxParser = reinterpret_cast<BxceSaxParser *>(
				sceNpCommonMallocNpServerName(sizeof(BxceSaxParser)));
			if (m_saxParser != NULL) {
				if (ScebXCe_C85586BD(m_saxParser) == 0) {
					ScebXCe_9E06C329(m_saxParser);
					ScebXCe_FF7CB737(&m_saxParser->parser, 1);
					ScebXCe_A40944DD(&m_saxParser->parser, 0);
					m_saxParser->handler = this;
					m_saxParser->characterOffsets = 0;
					m_savedSlot = -1;
					m_depth = 0;
					m_currentBuilder = NULL;
					m_lastError = 0;
					return 0;
				}
				sceNpCommonFreeNpServerName(m_saxParser);
				m_saxParser = NULL;
			}
		}

		terminate();
		return NP_XMPP_ERR_NO_MEMORY;
	}

	/* 0x81314712 */
	int terminate()
	{
		if (m_saxParser != NULL) {
			ScebXCe_928189AB(m_saxParser);
			sceNpCommonFreeNpServerName(m_saxParser);
			m_saxParser = NULL;
		}
		if (m_completedQueue != NULL) {
			XmppQueueNode *node = m_completedQueue->head;
			while (node != NULL) {
				m_completedQueue->head = node->next;
				if (node->builder != NULL) {
					builderDestroy(node->builder);
					sceNpCommonFreeNpServerName(node->builder);
				}
				sceNpCommonFreeNpServerName(node);
				node = m_completedQueue->head;
			}
			sceNpCommonFreeNpServerName(m_completedQueue);
			m_completedQueue = NULL;
		}
		return 0;
	}

	/* 0x8131475E */
	int feedChunk(const char *input, int size)
	{
		if (m_savedSlot >= 0)
			ScebXCe_A12B0F90(m_saxParser, static_cast<SceUInt8>(m_savedSlot));

		int ret = ScebXCe_4E9A7061(m_saxParser, input, size, 0);
		switch (ret) {
		case SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS:
			m_savedSlot = ScebXCe_3CAF2B78(m_saxParser);
			return 0;
		case SCE_BXCE_ERROR_NO_MEMORY:
			return NP_XMPP_ERR_NO_MEMORY;
		case SCE_BXCE_ERROR_XML_PARSE_ABORTED: {
			int err = m_lastError;
			m_lastError = 0;
			return err;
		}
		case 0:
			return 0;
		default:
			sceClibPrintf("invalid format %x (%d)\n", ret, ret);
			sceClibPrintf("bXResult_XML_InvalidDocumentElement %x (%d)\n",
				      SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT,
				      SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT);
			return ret;
		}
	}

	/* 0x813147F8 */
	XmppStanzaBuilder *dequeueBuilder()
	{
		XmppQueueNode *node = m_completedQueue->head;
		if (node == NULL)
			return NULL;

		m_completedQueue->head = node->next;
		if (m_completedQueue->head == NULL)
			m_completedQueue->tail = NULL;
		XmppStanzaBuilder *builder = node->builder;
		sceNpCommonFreeNpServerName(node);
		return builder;
	}

	/* 0x81314816 */
	virtual int startElement(void *userData, const char *name, int length,
				 const BxceAttributeList *attrList)
	{
		(void)userData;
		(void)length;
		++m_depth;

		int ret = 0;
		if (m_currentBuilder != NULL) {
			ret = builderStartChild(m_currentBuilder, name, attrList);
			if (ret < 0) {
				builderDestroy(m_currentBuilder);
				sceNpCommonFreeNpServerName(m_currentBuilder);
				m_depth = 0;
				m_currentBuilder = NULL;
				m_lastError = ret;
				return ret;
			}
		} else {
			XmppStanzaBuilder *builder = reinterpret_cast<XmppStanzaBuilder *>(
				sceNpCommonMallocNpServerName(sizeof(XmppStanzaBuilder)));
			if (builder == NULL) {
				--m_depth;
				m_lastError = NP_XMPP_ERR_NO_MEMORY;
				return NP_XMPP_ERR_NO_MEMORY;
			}

			int kind = NP_RT_STANZA_KIND_STREAM;
			if (sceClibStrncmp(name, "message", 8u) == 0)
				kind = NP_RT_STANZA_KIND_MESSAGE;
			else if (sceClibStrncmp(name, "presence", 9u) == 0)
				kind = NP_RT_STANZA_KIND_PRESENCE;
			else if (sceClibStrncmp(name, "iq", 3u) == 0)
				kind = NP_RT_STANZA_KIND_IQ;

			ret = builderInit(builder, kind);
			if (ret < 0) {
				sceNpCommonFreeNpServerName(builder);
				--m_depth;
				m_lastError = ret;
				return ret;
			}

			ret = builderStartRoot(builder, name, attrList);
			if (ret < 0) {
				builderDestroy(builder);
				sceNpCommonFreeNpServerName(builder);
				m_depth = 0;
				m_lastError = ret;
				return ret;
			}
			m_currentBuilder = builder;
		}

		if (sceClibStrncmp(name, "stream:stream", 14u) == 0) {
			builderEndElement(m_currentBuilder);
			enqueueCompletedBuilder(m_completedQueue, m_currentBuilder);
			m_currentBuilder = NULL;
			m_depth = 0;
		}

		m_lastError = ret;
		return ret;
	}

	/* 0x8131496A */
	virtual int characters(void *userData, const char *ch, int length)
	{
		(void)userData;
		int ret = 0;
		if (m_currentBuilder != NULL)
			ret = builderHandleCharacters(m_currentBuilder, ch,
						      static_cast<SceSize>(length));
		m_lastError = ret;
		return ret;
	}

	/* 0x81314986 */
	virtual int endElement(void *userData, const char *name, int length)
	{
		(void)userData;
		(void)length;
		--m_depth;

		int ret = 0;
		if (sceClibStrncmp(name, "stream:stream", 14u) == 0) {
			XmppStanzaBuilder *endBuilder = reinterpret_cast<XmppStanzaBuilder *>(
				sceNpCommonMallocNpServerName(sizeof(XmppStanzaBuilder)));
			if (endBuilder != NULL) {
				ret = builderInit(endBuilder, NP_RT_STANZA_KIND_STREAM_END);
				if (ret >= 0) {
					enqueueCompletedBuilder(m_completedQueue, endBuilder);
					ret = 0;
				} else {
					sceNpCommonFreeNpServerName(endBuilder);
				}
			} else {
				ret = NP_XMPP_ERR_NO_MEMORY;
			}
		} else if (m_currentBuilder != NULL) {
			builderEndElement(m_currentBuilder);
			bool finishStanza = false;
			if (sceClibStrncmp(name, "message", 8u) == 0 ||
			    sceClibStrncmp(name, "presence", 9u) == 0 ||
			    sceClibStrncmp(name, "iq", 3u) == 0) {
				finishStanza = (m_depth == 0);
			} else {
				int kind = m_currentBuilder->kind;
				if ((kind < NP_RT_STANZA_KIND_MESSAGE ||
				     kind > NP_RT_STANZA_KIND_IQ) &&
				    builderIsStackEmpty(m_currentBuilder) && m_depth == 0) {
					finishStanza = true;
				}
			}

			if (finishStanza) {
				enqueueCompletedBuilder(m_completedQueue, m_currentBuilder);
				m_currentBuilder = NULL;
				m_depth = 0;
			}
		}

		m_lastError = ret;
		return ret;
	}

	/* 0x81314B16 */
	virtual int entityData(void *userData, int entityType, const char *name, int length)
	{
		switch (entityType) {
		case BXCE_REF_AMP:
			characters(userData, "&", 1);
			break;
		case BXCE_REF_QUOT:
			characters(userData, "\"", 1);
			break;
		case BXCE_REF_LT:
			characters(userData, "<", 1);
			break;
		case BXCE_REF_GT:
			characters(userData, ">", 1);
			break;
		case BXCE_REF_APOS:
			characters(userData, "'", 1);
			break;
		case BXCE_REF_CHAR:
			characters(userData, name, length);
			break;
		default:
			characters(userData, "&", 1);
			characters(userData, name, length);
			characters(userData, ";", 1);
			break;
		}
		return 0;
	}

	/* 0x81314BDA */
	virtual void fatalError(void *userData, int errCode)
	{
		(void)userData;
		sceClibPrintf("******************************fatal error: %x\n", errCode);
	}

private:
	BxceSaxParser *m_saxParser;		/* +0x04 */
	XmppStanzaBuilder *m_currentBuilder;	/* +0x08 */
	XmppBuilderQueue *m_completedQueue;	/* +0x0C */
	int m_savedSlot;			/* +0x10 */
	int m_depth;				/* +0x14 */
	int m_lastError;			/* +0x18 */
};

namespace {

/* 0x81314548 */
static int parserCreate(XmppSaxParser **outParser)
{
	if (outParser == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	void *mem = npRtHeapAlloc(sizeof(XmppSaxParser), NULL);
	if (mem == NULL)
		return NP_XMPP_ERR_NO_MEMORY;

	XmppSaxParser *parser = new (mem) XmppSaxParser();
	int ret = parser->initialize();
	if (ret != 0) {
		parser->~XmppSaxParser();
		npRtHeapFree(parser, NULL);
		return ret;
	}

	*outParser = parser;
	return 0;
}

/* 0x813145A6 */
static int parserDestroy(XmppSaxParser *parser)
{
	parser->terminate();
	parser->~XmppSaxParser();
	npRtHeapFree(parser, NULL);
	return 0;
}

/* 0x813145C2 */
static int parserFeed(XmppSaxParser *parser, const char *buf, int len)
{
	return parser->feedChunk(buf, len);
}

/* 0x813145CA */
static int parserPopStanza(XmppSaxParser *parser, NpRtAnyStanza *outStanza)
{
	if (outStanza == NULL)
		return NP_XMPP_ERR_INVALID_ARG;

	XmppStanzaBuilder *builder = parser->dequeueBuilder();
	if (builder == NULL) {
		outStanza->kind = NP_RT_STANZA_KIND_NONE;
		outStanza->stanza = NULL;
		return 0;
	}

	int kind = builder->kind;
	int ret = builderTakeStanza(builder, &outStanza->stanza);
	switch (kind) {
	case NP_RT_STANZA_KIND_MESSAGE:
	case NP_RT_STANZA_KIND_PRESENCE:
	case NP_RT_STANZA_KIND_IQ:
	case NP_RT_STANZA_KIND_STREAM_END:
		outStanza->kind = kind;
		break;
	default:
		outStanza->kind = NP_RT_STANZA_KIND_STREAM;
		break;
	}
	sceNpCommonFreeNpServerName(builder);
	return ret;
}

/* --------------------------------------------------------------------------
 * Keepalive Tracker (0x813156A0..0x81315827)
 * -------------------------------------------------------------------------- */
/* 0x813156A0 */
static void keepAliveInit(NpRtKeepAlive *ka, int intervalUsec)
{
	sceClibMemset(ka, 0, sizeof(*ka));
	ka->intervalUsec = intervalUsec;
}

/* 0x813156B2 */
static int keepAliveGetWait(const NpRtKeepAlive *ka, SceUInt8 *outEnabled,
			    SceUInt32 *outRemainingUsec)
{
	SceUInt64 now = 0;
	SceNpCommon_55F7FCF7(&now);

	if (ka->intervalUsec <= 0) {
		*outEnabled = 0;
		return 0;
	}

	if (ka->lastSentUsec == 0 || now <= ka->lastSentUsec) {
		*outRemainingUsec = 0;
	} else {
		SceUInt64 elapsed = now - ka->lastSentUsec;
		SceUInt64 interval = static_cast<SceUInt64>(ka->intervalUsec);
		if (elapsed >= interval)
			*outRemainingUsec = 0;
		else
			*outRemainingUsec = static_cast<SceUInt32>(interval - elapsed);
	}
	*outEnabled = 1;
	return 0;
}

/* 0x81315776 */
static int keepAliveTick(NpRtKeepAlive *ka, NpRtXmppClient *client, int forceSend)
{
	if (ka->intervalUsec < 0)
		return 0;

	if (forceSend == 0) {
		SceUInt64 now = 0;
		SceNpCommon_55F7FCF7(&now);
		if (ka->lastSentUsec != 0 && now > ka->lastSentUsec &&
		    (now - ka->lastSentUsec + static_cast<SceUInt64>(kXmppKeepAliveSlackUsec)) <
			    static_cast<SceUInt64>(ka->intervalUsec)) {
			return 0;
		}
		ka->lastSentUsec = now;
	}

	int ret = npRtXmppClientSend(client, "\n", 1);
	return (ret >= 0) ? 0 : ret;
}

/* --------------------------------------------------------------------------
 * TCP Socket, Resolver, Epoll, and TLS/SSL Transport (0x81315828..0x8131616B,
 * 0x81316970..0x81316C1B)
 * -------------------------------------------------------------------------- */
/* 0x81316C10 */
static void *tcpResolverAlloc(SceSize size, SceNetId rid, const char *name, void *user)
{
	(void)rid;
	(void)name;
	(void)user;
	return npRtHeapAlloc(size, NULL);
}

/* 0x81316146 */
static void *sslVerifyAlloc(void *arg, SceSize size)
{
	(void)arg;
	return npRtHeapAlloc(size, NULL);
}

/* 0x81316152 */
static void *sslVerifyRealloc(void *arg, void *ptr, SceSize size)
{
	(void)arg;
	return npRtHeapRealloc(ptr, size, NULL);
}

/* 0x81316160 */
static void sslVerifyFree(void *arg, void *ptr)
{
	(void)arg;
	npRtHeapFree(ptr, NULL);
}

/* 0x81315DFC */
static int sslAlertLogNoop(void)
{
	return 0;
}

/* 0x81315F6A */
static void sslInfoCallback(void *ssl, int where, int ret)
{
	(void)ssl;
	if ((where & 0x4000) == 0)
		return;

	int alertCode = static_cast<SceInt8>(ret);
	if ((where & 8) != 0) {
		switch (alertCode) {
		case 0:
		case 10:
		case 20:
		case 30:
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			sslAlertLogNoop();
			break;
		default:
			break;
		}
	}
	if ((where & 4) != 0) {
		switch (alertCode) {
		case 0:
		case 10:
		case 20:
		case 30:
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			sslAlertLogNoop();
			break;
		default:
			break;
		}
	}
}

static void sslMessageCallbackNoop(void)
{
}

/* 0x81316970 */
static int tcpOpen(NpRtXmppClient *client)
{
	if (client->netBase.resolverId >= 0 || client->netBase.socketId >= 0)
		return NP_RT_CORE_ERR_NET_ALREADY_OPEN;

	SceNetResolverParam param;
	param.allocate = tcpResolverAlloc;
	param.free = npRtResolverFree;
	param.user = NULL;
	client->netBase.aborted = 0;

	int rid = sceNetResolverCreate("SceNpCore", &param, 0);
	client->netBase.resolverId = rid;
	int err = rid;
	if (rid >= 0) {
		int sock = sceNetSocket("SceNpCore", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
		client->netBase.socketId = sock;
		if (sock >= 0)
			return 0;
		err = sock;
	}

	if (client->netBase.resolverId >= 0) {
		sceNetResolverDestroy(client->netBase.resolverId);
		client->netBase.resolverId = -1;
	}
	if (client->netBase.socketId >= 0) {
		sceNetSocketClose(client->netBase.socketId);
		client->netBase.socketId = -1;
	}
	return err;
}

/* 0x81316A20 */
static int tcpClose(NpRtXmppClient *client)
{
	if (client->netBase.resolverId < 0 && client->netBase.socketId < 0)
		return NP_RT_CORE_ERR_NET_NOT_OPEN;

	if (client->netBase.resolverId >= 0) {
		sceNetResolverDestroy(client->netBase.resolverId);
		client->netBase.resolverId = -1;
	}
	if (client->netBase.socketId >= 0) {
		sceNetSocketClose(client->netBase.socketId);
		client->netBase.socketId = -1;
	}
	client->netBase.aborted = 0;
	return 0;
}

/* 0x81316A5E */
static int tcpConnect(NpRtXmppClient *client, const char *host, SceUInt16 port)
{
	if (client->netBase.resolverId < 0 && client->netBase.socketId < 0)
		return NP_RT_CORE_ERR_NET_NOT_OPEN;
	if (client->netBase.aborted != 0)
		return NP_RT_CORE_ERR_ABORTED;

	sceClibMemset(&client->remoteAddr, 0, sizeof(client->remoteAddr));
	int ret = sceNetResolverStartNtoa(client->netBase.resolverId, host,
					  &client->remoteAddr.sin_addr, 0, 0, 0);
	if (client->netBase.aborted != 0)
		return NP_RT_CORE_ERR_ABORTED;
	if (ret < 0)
		return ret;

	client->remoteAddr.sin_len = sizeof(SceNetSockaddrIn);
	client->remoteAddr.sin_family = SCE_NET_AF_INET;
	client->remoteAddr.sin_port = sceNetHtons(port);

	ret = sceNetConnect(client->netBase.socketId,
			    reinterpret_cast<const SceNetSockaddr *>(&client->remoteAddr),
			    sizeof(client->remoteAddr));
	if (client->netBase.aborted != 0)
		return NP_RT_CORE_ERR_ABORTED;
	if (ret == kNetErrAborted)
		return ret;
	if (ret == kNetErrWouldBlock || ret == kNetErrTimedOut)
		return NP_RT_CORE_ERR_WOULD_BLOCK;
	return (ret >= 0) ? 0 : ret;
}

/* 0x81316B30 */
static int tcpAbort(NpRtXmppClient *client, int abortMask)
{
	if (client->netBase.resolverId < 0 && client->netBase.socketId < 0)
		return NP_RT_CORE_ERR_NET_NOT_OPEN;

	if ((abortMask & 4) != 0)
		client->netBase.aborted = 1;

	if (client->netBase.socketId >= 0) {
		int sockFlags = 0;
		if ((abortMask & 2) != 0)
			sockFlags |= 2;
		if ((abortMask & 1) != 0)
			sockFlags |= 1;
		sceNetSocketAbort(client->netBase.socketId, sockFlags);
	}
	if (client->netBase.resolverId >= 0)
		sceNetResolverAbort(client->netBase.resolverId, ((abortMask & 2) != 0) ? 1 : 0);
	return 0;
}

/* 0x81316B90 */
static int tcpSetRecvTimeout(NpRtXmppClient *client, int timeoutUsec)
{
	int optVal = timeoutUsec;
	int ret = sceNetSetsockopt(client->netBase.socketId, SCE_NET_SOL_SOCKET,
				   SCE_NET_SO_RCVTIMEO, &optVal, sizeof(optVal));
	return (ret >= 0) ? 0 : ret;
}

/* 0x81316BD0 */
static int tcpSetSendTimeout(NpRtXmppClient *client, int timeoutUsec)
{
	int optVal = timeoutUsec;
	int ret = sceNetSetsockopt(client->netBase.socketId, SCE_NET_SOL_SOCKET,
				   SCE_NET_SO_SNDTIMEO, &optVal, sizeof(optVal));
	return (ret >= 0) ? 0 : ret;
}

/* 0x81315828 */
static void sslTransportInit(NpRtXmppClient *client)
{
	npRtNetBaseInit(&client->netBase);
	client->ssl = NULL;
	client->epollId = -1;
	client->sslCtx = NULL;
	client->sslVerifyCtx = NULL;
	client->recvTimeoutUsec = -1;
	client->sendTimeoutUsec = -1;
}

/* 0x81315844 */
static int sslTransportIsOpen(const NpRtXmppClient *client)
{
	return npRtNetBaseIsOpen(&client->netBase);
}

/* 0x8131584E */
static int sslTransportOpen(NpRtXmppClient *client)
{
	bool mutexCreated = false;
	int ret = sceKernelCreateLwMutex(&client->sslMutex, "SceNpCoreNet", 0x2002u, 0, NULL);
	if (ret >= 0) {
		mutexCreated = true;
		ret = tcpOpen(client);
		if (ret >= 0) {
			SceNetId eid = sceNetEpollCreate("SceNpCore", 0);
			client->epollId = eid;
			if (eid >= 0)
				return 0;
			ret = eid;
		}
	}

	if (client->epollId >= 0) {
		sceNetEpollDestroy(client->epollId);
		client->epollId = -1;
	}
	if (npRtNetBaseIsOpen(&client->netBase) != 0)
		tcpClose(client);
	if (mutexCreated)
		sceKernelDeleteLwMutex(&client->sslMutex);
	return ret;
}

/* 0x813158D2 */
static void sslTransportClose(NpRtXmppClient *client)
{
	if (client->ssl != NULL) {
		SceSslInternal_EC9FAF42(client->ssl);
		SceSslInternal_19B2BB58(client->ssl);
		client->ssl = NULL;
		SceSslInternal_EF9EBAC7(client->sslCtx);
		client->sslCtx = NULL;
		npRtHeapFree(client->sslVerifyCtx, NULL);
		client->sslVerifyCtx = NULL;
	}

	if (npRtNetBaseIsOpen(&client->netBase) != 0) {
		sceNetEpollDestroy(client->epollId);
		tcpClose(client);
		sceKernelDeleteLwMutex(&client->sslMutex);
	}
}

/* 0x8131591C */
static int sslTransportConnect(NpRtXmppClient *client, const char *host, SceUInt16 port)
{
	int ret = tcpConnect(client, host, port);
	if (ret < 0)
		return ret;

	void *sslLib = NULL;
	ret = SceSslInternal_2185BB05(&sslLib);
	if (ret >= 0) {
		void *method = SceSslInternal_A374CD7B();
		client->sslCtx = SceSslInternal_E746465E(sslLib, method);
		if (client->sslCtx == NULL) {
			ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP,
						      NP_XMPP_ERR_SSL_HANDSHAKE);
		} else {
			SceUInt8 rngSeed[64];
			ret = sceKernelGetRandomNumber(rngSeed, sizeof(rngSeed));
			if (ret >= 0) {
				int seedErr = SceSslInternal_67FB1CD3(client->sslCtx, rngSeed,
								      sizeof(rngSeed));
				if (seedErr != 0) {
					ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_SSL,
								      seedErr);
				}
			}

			if (ret >= 0) {
				SceSslInternal_FBEB1FD7(client->sslCtx, kSslCtxOptions);
				NpXmppSslVerifyCtx *verifyCtx =
					reinterpret_cast<NpXmppSslVerifyCtx *>(
						npRtHeapAlloc(sizeof(NpXmppSslVerifyCtx), NULL));
				client->sslVerifyCtx = verifyCtx;
				if (verifyCtx == NULL) {
					ret = NP_RT_CORE_ERR_NO_MEMORY;
				} else {
					sceClibMemset(verifyCtx, 0, sizeof(*verifyCtx));
					verifyCtx->sslFlags = kHttpsVerifyFlags;
					verifyCtx->hostname = host;
					verifyCtx->sslError = &client->sslError;
					verifyCtx->sslErrorDetail = &client->sslErrorDetail;
					verifyCtx->callback = NULL;
					verifyCtx->userArg = NULL;
					verifyCtx->alloc = sslVerifyAlloc;
					verifyCtx->realloc = sslVerifyRealloc;
					verifyCtx->free = sslVerifyFree;
					verifyCtx->allocatorArg = NULL;
					ret = SceHttp_A9A39BA1(client->sslCtx, verifyCtx);
				}
			}

			if (ret >= 0) {
				SceSslInternal_CC7F4211(client->sslCtx, kSslCtrlSessionCache, 0,
							NULL);
				client->ssl = reinterpret_cast<struct ssl_st *>(
					SceSslInternal_D28834E6(client->sslCtx));
				if (client->ssl == NULL) {
					ret = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP,
								      NP_XMPP_ERR_SSL_HANDSHAKE);
				} else {
					SceSslInternal_0DB6C206(client->ssl, kSslConnMode);
					void *bio = SceSslInternal_FFE2ED51(
						client->netBase.socketId, 0);
					SceSslInternal_4A1557A7(client->ssl, bio, bio);
					SceSslInternal_F2C9B744(client->ssl, sslInfoCallback);
					SceSslInternal_F71DB06F(client->ssl, sslMessageCallbackNoop,
								NULL);
					SceSslInternal_CEE46B33(client->ssl);

					bool done = false;
					while (!done && ret >= 0) {
						*sceNetErrnoLoc() = 0;
						int hsRet = SceSslInternal_E5EDD91B(client->ssl);
						int sslErr = SceSslInternal_2F0FC3CD(client->ssl,
										     hsRet);
						switch (sslErr) {
						case SSL_INTERNAL_ERROR_NONE:
							done = true;
							ret = 0;
							break;
						case SSL_INTERNAL_ERROR_SSL:
							ret = SceSslInternal_461C5990(
								SSL_INTERNAL_LIBRARY_HTTP,
								NP_XMPP_ERR_SSL_HANDSHAKE);
							break;
						case SSL_INTERNAL_ERROR_WANT_READ:
						case SSL_INTERNAL_ERROR_WANT_WRITE:
						case SSL_INTERNAL_ERROR_SYSCALL:
							ret = NP_XMPP_ERR_SSL_HANDSHAKE;
							if (*sceNetErrnoLoc() > 0)
								ret = static_cast<int>(
									static_cast<SceUInt32>(
										*sceNetErrnoLoc()) |
									0x80410100u);
							break;
						case SSL_INTERNAL_ERROR_ZERO_RETURN:
							ret = NP_XMPP_ERR_SSL_HANDSHAKE;
							break;
						default:
							break;
						}
					}
					if (done)
						return 0;
				}
			}
		}
	}

	if (client->ssl != NULL) {
		SceSslInternal_19B2BB58(client->ssl);
		client->ssl = NULL;
	}
	if (client->sslCtx != NULL) {
		SceSslInternal_EF9EBAC7(client->sslCtx);
		client->sslCtx = NULL;
	}
	if (client->sslVerifyCtx != NULL) {
		npRtHeapFree(client->sslVerifyCtx, NULL);
		client->sslVerifyCtx = NULL;
	}
	return ret;
}

/* 0x81315B82 */
static int sslTransportWrite(NpRtXmppClient *client, const void *buf, SceSize len)
{
	int ret = sceKernelLockLwMutex(&client->sslMutex, 1, NULL);
	if (ret < 0)
		return ret;

	int written = sceSslInternalWrite(client->ssl, buf, static_cast<int>(len));
	int sslErr = SceSslInternal_2F0FC3CD(client->ssl, written);
	int result = written;

	switch (sslErr) {
	case SSL_INTERNAL_ERROR_NONE:
		result = written;
		break;
	case SSL_INTERNAL_ERROR_SSL:
		result = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP, NP_XMPP_ERR_SSL_WRITE);
		if (result == NP_XMPP_ERR_SSL_WRITE && written < 0)
			result = written;
		break;
	case SSL_INTERNAL_ERROR_WANT_READ:
	case SSL_INTERNAL_ERROR_WANT_WRITE:
	case SSL_INTERNAL_ERROR_SYSCALL:
		result = (written < 0) ? written : NP_XMPP_ERR_SSL_WRITE;
		break;
	case SSL_INTERNAL_ERROR_ZERO_RETURN:
		result = NP_XMPP_ERR_SSL_WRITE;
		break;
	default:
		result = written;
		break;
	}

	sceKernelUnlockLwMutex(&client->sslMutex, 1);
	return result;
}

/* 0x81315C42 */
static int sslTransportRead(NpRtXmppClient *client, void *buf, SceSize len)
{
	int timeoutUsec = client->recvTimeoutUsec;
	if (SceSslInternal_D776E2F8(client->ssl) <= 0) {
		SceNetEpollEvent regEvent;
		SceNetEpollEvent waitEvent;
		sce_paf_memset(&regEvent, 0, sizeof(regEvent));
		regEvent.data.fd = client->netBase.socketId;
		regEvent.events = SCE_NET_EPOLLIN;

		int ret = sceNetEpollControl(client->epollId, SCE_NET_EPOLL_CTL_ADD,
					     client->netBase.socketId, &regEvent);
		if (ret < 0)
			return ret;

		int waitRet = sceNetEpollWait(client->epollId, &waitEvent, 1, timeoutUsec);
		if (client->netBase.aborted != 0)
			return NP_RT_CORE_ERR_ABORTED;
		if (waitRet == 0)
			return NP_RT_CORE_ERR_TIMEOUT;
		if (waitRet < 0)
			return waitRet;

		ret = sceNetEpollControl(client->epollId, SCE_NET_EPOLL_CTL_DEL,
					 client->netBase.socketId, NULL);
		if (ret < 0)
			return ret;
	}

	int ret = sceKernelLockLwMutex(&client->sslMutex, 1, NULL);
	if (ret < 0)
		return ret;

	int bytesRead = SceSslInternal_4F781BB6(client->ssl, buf, static_cast<int>(len));
	int sslErr = SceSslInternal_2F0FC3CD(client->ssl, bytesRead);
	int result = bytesRead;

	switch (sslErr) {
	case SSL_INTERNAL_ERROR_NONE:
		result = bytesRead;
		break;
	case SSL_INTERNAL_ERROR_SSL:
		result = SceSslInternal_461C5990(SSL_INTERNAL_LIBRARY_HTTP, NP_XMPP_ERR_SSL_READ);
		if (result == NP_XMPP_ERR_SSL_READ && bytesRead < 0)
			result = bytesRead;
		break;
	case SSL_INTERNAL_ERROR_WANT_READ:
	case SSL_INTERNAL_ERROR_WANT_WRITE:
	case SSL_INTERNAL_ERROR_SYSCALL:
		result = (bytesRead < 0) ? bytesRead : NP_XMPP_ERR_SSL_READ;
		break;
	case SSL_INTERNAL_ERROR_ZERO_RETURN:
		result = NP_XMPP_ERR_SSL_READ;
		break;
	default:
		result = bytesRead;
		break;
	}

	sceKernelUnlockLwMutex(&client->sslMutex, 1);
	return result;
}

/* 0x81315DA4 */
static int sslTransportAbort(NpRtXmppClient *client, int abortMask)
{
	int ret = tcpAbort(client, abortMask);
	if (ret < 0)
		return ret;
	if (client->epollId >= 0)
		sceNetEpollAbort(client->epollId, ((abortMask & 1) != 0) ? 1 : 0);
	return 0;
}

/* 0x81315DCE */
static int sslTransportSetRecvTimeout(NpRtXmppClient *client, int timeoutUsec)
{
	int ret = tcpSetRecvTimeout(client, timeoutUsec);
	if (ret < 0)
		return ret;
	client->recvTimeoutUsec = timeoutUsec;
	return 0;
}

/* 0x81315DE4 */
static int sslTransportSetSendTimeout(NpRtXmppClient *client, int timeoutUsec)
{
	int ret = tcpSetSendTimeout(client, timeoutUsec);
	if (ret < 0)
		return ret;
	client->sendTimeoutUsec = timeoutUsec;
	return 0;
}

static int validateStreamOpenHeader(BxceDocument *doc)
{
	BxceNodeId root = ScebXCe_CED6BECF(doc);
	const char *xmlnsVal = NULL;
	const char *streamXmlnsVal = NULL;
	const char *fromVal = NULL;
	const char *versionVal = NULL;
	const char *idVal = NULL;

	for (BxceNodeId attr = ScebXCe_B034C595(doc, root);
	     attr != 0;
	     attr = ScebXCe_134F491C(doc, attr)) {
		const char *attrName = ScebXCe_B4989A58(doc, attr);
		const char *attrVal = ScebXCe_84383D8E(doc, attr);

		if (sceClibStrncmp(attrName, "xmlns:stream",
				   sceClibStrnlen("xmlns:stream", 0x7FFFFFFFu)) == 0) {
			streamXmlnsVal = attrVal;
		} else if (sceClibStrncmp(attrName, "xmlns",
					  sceClibStrnlen("xmlns", 0x7FFFFFFFu)) == 0) {
			xmlnsVal = attrVal;
		} else if (sceClibStrncmp(attrName, "from",
					  sceClibStrnlen("from", 0x7FFFFFFFu)) == 0) {
			fromVal = attrVal;
		} else if (sceClibStrncmp(attrName, "version",
					  sceClibStrnlen("version", 0x7FFFFFFFu)) == 0) {
			versionVal = attrVal;
		} else if (sceClibStrncmp(attrName, "id",
					  sceClibStrnlen("id", 0x7FFFFFFFu)) == 0) {
			idVal = attrVal;
		}
	}

	if (xmlnsVal == NULL || streamXmlnsVal == NULL || idVal == NULL ||
	    fromVal == NULL || versionVal == NULL) {
		return NP_RT_CORE_ERR_STREAM_ATTR_MISSING;
	}
	if (sceClibStrncmp(xmlnsVal, "jabber:client",
			   sceClibStrnlen("jabber:client", 0x7FFFFFFFu)) != 0) {
		return NP_RT_CORE_ERR_STREAM_BAD_XMLNS;
	}
	if (sceClibStrncmp(streamXmlnsVal, "http://etherx.jabber.org/streams",
			   sceClibStrnlen("http://etherx.jabber.org/streams", 0x7FFFFFFFu)) != 0) {
		return NP_RT_CORE_ERR_STREAM_BAD_XMLNS;
	}
	if (sceClibStrncmp(versionVal, "1.0", sceClibStrnlen("1.0", 0x7FFFFFFFu)) != 0)
		return NP_RT_CORE_ERR_STREAM_BAD_VERSION;
	return 0;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Public XMPP Stream Client API (0x8131370C..0x81313DE3)
 * -------------------------------------------------------------------------- */
/* 0x8131370C */
int npRtXmppClientInit(NpRtXmppClient *client)
{
	sslTransportInit(client);
	client->parser = NULL;
	client->recvBuf = NULL;
	return 0;
}

/* 0x8131371C */
void npRtXmppClientTerm(NpRtXmppClient *client)
{
	sslTransportClose(client);
	if (client->parser != NULL) {
		parserDestroy(client->parser);
		client->parser = NULL;
	}
	if (client->recvBuf != NULL) {
		npRtHeapFree(client->recvBuf, NULL);
		client->recvBuf = NULL;
		client->recvBufSize = 0;
	}
}

/* 0x81313742 */
int npRtXmppClientIsOpen(const NpRtXmppClient *client)
{
	return sslTransportIsOpen(client);
}

/* 0x8131374A */
int npRtXmppClientOpen(NpRtXmppClient *client)
{
	if (sslTransportIsOpen(client) != 0)
		return NP_RT_CORE_ERR_XMPP_ALREADY_OPEN;

	int ret = NP_RT_CORE_ERR_NO_MEMORY;
	char *buf = reinterpret_cast<char *>(npRtHeapAlloc(kXmppRecvBufSize, NULL));
	if (buf != NULL) {
		client->recvBuf = buf;
		client->recvBufSize = kXmppRecvBufSize;

		XmppSaxParser *parser = NULL;
		ret = parserCreate(&parser);
		if (ret >= 0) {
			client->parser = parser;
			client->streamState = NP_RT_STREAM_NONE;
			ret = sslTransportOpen(client);
			if (ret >= 0)
				ret = sslTransportSetRecvTimeout(client, kXmppRecvTimeoutUsec);
			if (ret >= 0)
				ret = sslTransportSetSendTimeout(client, kXmppSendTimeoutUsec);
			if (ret >= 0)
				return 0;
		}
	}

	npRtXmppClientTerm(client);
	return ret;
}

/* 0x81313816 */
int npRtXmppClientClose(NpRtXmppClient *client)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;
	npRtXmppClientTerm(client);
	return 0;
}

/* 0x81313852 */
int npRtXmppClientAbort(NpRtXmppClient *client, int abortMask)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;
	return sslTransportAbort(client, abortMask);
}

/* 0x81313874 */
int npRtXmppClientSend(NpRtXmppClient *client, const void *buf, SceSize len)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;

	int ret = sslTransportWrite(client, buf, len);
	if (ret == kNetErrAborted)
		return NP_RT_CORE_ERR_ABORTED;
	if (ret >= 0)
		return (static_cast<SceSize>(ret) == len) ? 0 : NP_RT_CORE_ERR_SHORT_SEND;
	return ret;
}

/* 0x813138C6 */
int npRtXmppClientConnect(NpRtXmppClient *client, const char *host, SceUInt16 port,
			  const char *domain)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;

	char *openXml = reinterpret_cast<char *>(npRtHeapAlloc(kStreamOpenBufSize, NULL));
	if (openXml == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	keepAliveInit(&client->keepAlive, kXmppKeepAliveIntervalUsec);
	int ret = sslTransportConnect(client, host, port);
	if (ret >= 0) {
		client->streamState = NP_RT_STREAM_OPENING;
		sceClibSnprintf(
			openXml, kStreamOpenBufSize,
			"<?xml version='1.0'?><stream:stream xmlns='jabber:client' "
			"xmlns:stream='http://etherx.jabber.org/streams' "
			"xmlns:np='http://playstation.net/np/rt/streams' "
			"to='%s' version='1.0' np:clientversion='%s'>",
			domain, "1.0");
		SceSize openLen = sceClibStrnlen(openXml, 0x7FFFFFFFu);
		ret = npRtXmppClientSend(client, openXml, openLen);
		if (ret >= 0)
			ret = 0;
	}

	npRtHeapFree(openXml, NULL);
	return ret;
}

/* 0x813139B0 */
int npRtXmppClientSendStreamClose(NpRtXmppClient *client)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;

	int ret = npRtXmppClientSend(client, "</stream:stream>", 16);
	client->streamState = NP_RT_STREAM_CLOSING;
	return ret;
}

/* 0x81313A1C */
int npRtXmppClientCheckKeepAlive(NpRtXmppClient *client, SceUInt8 *outEnabled,
				 SceUInt32 *outRemainingUsec)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;
	return keepAliveGetWait(&client->keepAlive, outEnabled, outRemainingUsec);
}

/* 0x81313A44 */
int npRtXmppClientSendKeepAlive(NpRtXmppClient *client)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;
	return keepAliveTick(&client->keepAlive, client, 0);
}

/* 0x81313A68 */
int npRtXmppClientRecvStanza(NpRtXmppClient *client, NpRtAnyStanza *outStanza)
{
	if (sslTransportIsOpen(client) == 0)
		return NP_RT_CORE_ERR_XMPP_NOT_OPEN;
	if (outStanza->kind != NP_RT_STANZA_KIND_NONE || outStanza->stanza != NULL)
		return NP_RT_CORE_ERR_INVALID_ARG;

	if (client->streamState != NP_RT_STREAM_OPENING &&
	    client->streamState != NP_RT_STREAM_OPEN) {
		npRtReleaseAnyStanza(outStanza);
		client->streamState = NP_RT_STREAM_ERROR;
		return NP_RT_CORE_ERR_INVALID_STREAM_STATE;
	}

	int status = 0;
	for (;;) {
		while (outStanza->kind == NP_RT_STANZA_KIND_NONE) {
			int popRet = parserPopStanza(client->parser, outStanza);
			if (popRet < 0)
				return popRet;
			if (outStanza->kind != NP_RT_STANZA_KIND_NONE)
				break;

			int bytesRead = sslTransportRead(client, client->recvBuf,
							 client->recvBufSize);
			if (bytesRead == kNetErrAborted) {
				status = NP_RT_CORE_ERR_ABORTED;
				break;
			}
			if (bytesRead < 0) {
				status = bytesRead;
				break;
			}
			if (bytesRead == 0) {
				status = NP_RT_CORE_ERR_DISCONNECTED;
				break;
			}

			if (bytesRead != 1 || client->recvBuf[0] != '\n') {
				int feedRet = parserFeed(client->parser, client->recvBuf,
							 bytesRead);
				if (feedRet < 0) {
					status = feedRet;
					break;
				}
			}
		}

		if (status < 0)
			break;

		if (outStanza->kind == NP_RT_STANZA_KIND_STREAM) {
			BxceDocument *doc = reinterpret_cast<BxceDocument *>(outStanza->stanza);
			BxceNodeId root = ScebXCe_CED6BECF(doc);
			int isStreamError = 0;
			status = npRtMatchElementName(doc, root, "stream:error", &isStreamError);
			if (status < 0)
				break;
			if (isStreamError != 0) {
				NpXmppErrorStanza streamErr;
				int parseRet = npRtParseStreamError(&streamErr, doc);
				if (parseRet >= 0) {
					status = (streamErr.errorCode != 0)
							 ? static_cast<int>(0x80551100u |
									    streamErr.errorCode)
							 : NP_RT_CORE_ERR_STREAM_ERROR;
					npRtFreeXmppErrorTextList(&streamErr);
				} else {
					status = parseRet;
				}
				break;
			}
		}

		if (client->streamState == NP_RT_STREAM_OPEN) {
			if (outStanza->kind == NP_RT_STANZA_KIND_STREAM_END)
				client->streamState = NP_RT_STREAM_CLOSING;
			return 0;
		}
		if (client->streamState != NP_RT_STREAM_OPENING) {
			status = NP_RT_CORE_ERR_INVALID_STREAM_STATE;
			break;
		}

		if (outStanza->kind != NP_RT_STANZA_KIND_STREAM) {
			npRtReleaseAnyStanza(outStanza);
			continue;
		}

		BxceDocument *doc = reinterpret_cast<BxceDocument *>(outStanza->stanza);
		BxceNodeId root = ScebXCe_CED6BECF(doc);
		int isStreamOpen = 0;
		status = npRtMatchElementName(doc, root, "stream:stream", &isStreamOpen);
		if (status < 0)
			break;
		if (isStreamOpen == 0)
			return 0;

		status = validateStreamOpenHeader(doc);
		if (status < 0)
			break;

		client->streamState = NP_RT_STREAM_OPEN;
		return 0;
	}

	npRtReleaseAnyStanza(outStanza);
	client->streamState = NP_RT_STREAM_ERROR;
	return status;
}
