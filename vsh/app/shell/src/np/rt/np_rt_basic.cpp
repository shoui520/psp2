/*
 * np_rt_basic.cpp: SceNpBasic default XMPP handler for roster pushes
 * ("jabber:iq:roster"), privacy pushes ("jabber:iq:privacy"), roster query,
 * and message/presence forwarding (0x81316C1C..0x813176AB).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sce_atomic.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>
#include "np_rt_core.h"

namespace {

typedef int (*NpBasicIqDispatchFn)(const NpRtSessionIdentity *session, NpRtStanza *stanza);

struct NpBasicIqNamespaceEntry {
	const char *namespaceUri;
	NpBasicIqDispatchFn handler;
};

static SceKernelLwMutexWork s_basicMutex;	/* 0x815C53F0 */
static int s_basicMutexInit = 0;		/* 0x81546500 */
static int s_basicHandlerId = 0;		/* 0x815C5410 */
static NpBasicCallbacks s_basicCallbacks;	/* 0x815C5418 */

static int handleRosterPushIq(const NpRtSessionIdentity *session, NpRtStanza *stanza);
static int handlePrivacyPushIq(const NpRtSessionIdentity *session, NpRtStanza *stanza);

/* 0x81546504: SceNpBasic IQ namespace dispatch table */
static const NpBasicIqNamespaceEntry kBasicIqNamespaces[] = {
	{ "jabber:iq:roster",  handleRosterPushIq },
	{ "jabber:iq:privacy", handlePrivacyPushIq },
	{ NULL,                NULL }
};

/* 0x81316C4C */
static int basicMutexInit(void)
{
	if (s_basicMutexInit > 0)
		return NP_RT_CORE_ERR_ALREADY_INIT;
	int ret = sceKernelCreateLwMutex(&s_basicMutex, "SceNpBasic", 0x2002u, 0, NULL);
	if (ret >= 0)
		s_basicMutexInit = 1;
	return ret;
}

/* 0x81316C9C */
static int basicMutexTerm(void)
{
	if (s_basicMutexInit >= 0)
		return sceKernelDeleteLwMutex(&s_basicMutex);
	return s_basicMutexInit;
}

/* 0x81316E8C */
static void notifyRosterPush(const NpRtSessionIdentity *session, NpBasicRosterList *rosterList)
{
	if (s_basicCallbacks.onRosterPush != NULL)
		s_basicCallbacks.onRosterPush(session, rosterList);
}

/* 0x81316E9E */
static void notifyPrivacyPush(const NpRtSessionIdentity *session, NpBasicPrivacyList *privacyList)
{
	if (s_basicCallbacks.onPrivacyPush != NULL)
		s_basicCallbacks.onPrivacyPush(session, privacyList);
}

/* 0x81316EB0 / 0x81316E52 */
static void onBasicMessageRecv(const NpRtSessionIdentity *session, NpRtMessageStanza *stanza)
{
	if (s_basicCallbacks.onMessageRecv != NULL)
		s_basicCallbacks.onMessageRecv(session, stanza);
}

/* 0x81316EC2 / 0x81316E5A */
static void onBasicPresenceRecv(const NpRtSessionIdentity *session, NpRtPresenceStanza *stanza)
{
	if (s_basicCallbacks.onPresenceRecv != NULL)
		s_basicCallbacks.onPresenceRecv(session, stanza);
}

/* 0x81316ED4 */
static int sendBasicIqError(const NpRtSessionIdentity *session, NpRtStanza *stanza, int errCode)
{
	NpXmppErrorStanza errStanza;
	sceClibMemset(&errStanza, 0, sizeof(errStanza));

	if (errCode == NP_RT_CORE_ERR_NOT_INITIALIZED ||
	    errCode == NP_RT_CORE_ERR_INVALID_ARG ||
	    errCode == NP_RT_CORE_ERR_NO_MEMORY) {
		errStanza.errorType = NP_RT_XMPP_ERRTYPE_WAIT;
		errStanza.condition = NP_RT_XMPP_COND_RESOURCE_CONSTRAINT;
	} else {
		errStanza.errorType = NP_RT_XMPP_ERRTYPE_CANCEL;
		errStanza.condition = NP_RT_XMPP_COND_SERVICE_UNAVAILABLE;
	}

	errStanza.textList = NULL;
	errStanza.errorKind = NP_RT_XMPP_SOURCE_CLIENT;
	errStanza.errorCode = 0;
	errStanza.origStanza = stanza;

	int ret = npRtSendIqError(session->sessionId, s_basicHandlerId, &errStanza);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81316D54 */
static void onBasicIqRecv(const NpRtSessionIdentity *session, NpRtStanza *stanza)
{
	SceUInt32 nsCount = 0;
	int ret = npRtCollectNamespaces(stanza->xmlDoc, NULL, &nsCount);
	if (ret < 0) {
		sendBasicIqError(session, stanza, ret);
		return;
	}

	const char **nsList = reinterpret_cast<const char **>(
		npRtHeapAlloc(nsCount * sizeof(const char *), NULL));
	if (nsList == NULL) {
		sendBasicIqError(session, stanza, NP_RT_CORE_ERR_NO_MEMORY);
		return;
	}

	ret = npRtCollectNamespaces(stanza->xmlDoc, nsList, NULL);
	if (ret < 0) {
		npRtHeapFree(nsList, NULL);
		sendBasicIqError(session, stanza, ret);
		return;
	}

	bool handled = false;
	for (const NpBasicIqNamespaceEntry *entry = kBasicIqNamespaces;
	     !handled && entry->namespaceUri != NULL; ++entry) {
		SceSize targetLen = sceClibStrnlen(entry->namespaceUri, 0x7FFFFFFFu);
		for (SceUInt32 idx = 0; idx < nsCount; ++idx) {
			if (sceClibStrncmp(nsList[idx], entry->namespaceUri, targetLen + 1) == 0) {
				ret = entry->handler(session, stanza);
				handled = true;
				break;
			}
		}
	}

	npRtHeapFree(nsList, NULL);
	if (ret < 0)
		sendBasicIqError(session, stanza, ret);
}

/* 0x81316CBC */
static int basicRegisterDefaultHandler(void)
{
	if (s_basicHandlerId != 0)
		return NP_RT_CORE_ERR_ALREADY_INIT;

	NpRtHandlerConfig config;
	config.namespaces = NULL;
	config.onStateChanged = NULL;
	config.onIqRecv = onBasicIqRecv;
	config.onMessageRecv = onBasicMessageRecv;
	config.onPresenceRecv = onBasicPresenceRecv;

	int ret = npRtRegisterDefaultHandler(&config, &s_basicHandlerId);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81316D2E */
static int basicUnregisterDefaultHandler(void)
{
	if (s_basicHandlerId == 0)
		return NP_RT_CORE_ERR_NOT_INITIALIZED;

	int ret = npRtUnregisterHandler(s_basicHandlerId);
	return (ret >= 0) ? 0 : ret;
}

static void freeRosterListImmediate(NpBasicRosterList *list)
{
	if (list == NULL)
		return;
	if (list->items != NULL) {
		for (SceUInt32 idx = 0; idx < list->count; ++idx) {
			if (list->items[idx] != NULL)
				npRtHeapFree(list->items[idx], NULL);
		}
		npRtHeapFree(list->items, NULL);
		list->items = NULL;
	}
	npRtHeapFree(list, NULL);
}

static void freePrivacyListImmediate(NpBasicPrivacyList *list)
{
	if (list == NULL)
		return;
	if (list->listName != NULL)
		npRtFreeString(list->listName, NULL, 0);
	npRtHeapFree(list, NULL);
}

static int parseSubscriptionAttr(const BxceDocument *xmlDoc, BxceNodeId itemNode)
{
	const char *subAttr = ScebXCe_753D9382(xmlDoc, itemNode, "subscription");
	if (subAttr == NULL)
		return NP_BASIC_SUB_NONE;

	const char *askAttr = ScebXCe_753D9382(xmlDoc, itemNode, "ask");
	const char *askedAttr = ScebXCe_753D9382(xmlDoc, itemNode, "asked");

	if (sceClibStrncmp(subAttr, "remove", 7u) == 0)
		return NP_BASIC_SUB_REMOVE;
	if (sceClibStrncmp(subAttr, "to", 3u) == 0)
		return NP_BASIC_SUB_OUTGOING;
	if (sceClibStrncmp(subAttr, "from", 5u) == 0)
		return NP_BASIC_SUB_FRIEND;
	if (sceClibStrncmp(subAttr, "both", 5u) == 0)
		return NP_BASIC_SUB_FRIEND;
	if (sceClibStrncmp(subAttr, "none", 5u) == 0) {
		if (askedAttr != NULL && sceClibStrncmp(askedAttr, "subscribe", 10u) == 0)
			return NP_BASIC_SUB_ASKED_IN;
		if (askAttr != NULL && sceClibStrncmp(askAttr, "subscribe", 10u) == 0)
			return NP_BASIC_SUB_OUTGOING;
	}
	return NP_BASIC_SUB_NONE;
}

/* 0x81316FAE */
static int parseRosterQuery(BxceDocument *xmlDoc, BxceNodeId queryNode,
			    NpBasicRosterList **outList)
{
	NpBasicRosterList *list = reinterpret_cast<NpBasicRosterList *>(
		npRtHeapAlloc(sizeof(NpBasicRosterList), NULL));
	if (list == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;
	sceClibMemset(list, 0, sizeof(*list));

	BxceNodeList *itemNodes = ScebXCe_1FA24DC0(xmlDoc, queryNode, "item");
	if (itemNodes == NULL) {
		freeRosterListImmediate(list);
		return NP_RT_CORE_ERR_NOT_FOUND;
	}

	SceUInt32 validNodes = 0;
	SceUInt32 totalNodes = static_cast<SceUInt32>(itemNodes->length);
	for (SceUInt32 idx = 0; idx < totalNodes; ++idx) {
		if (ScebXCe_6C46D786(itemNodes, idx) != 0)
			++validNodes;
	}

	list->items = reinterpret_cast<NpBasicRosterItem **>(
		npRtHeapAlloc(validNodes * sizeof(NpBasicRosterItem *), NULL));
	if (list->items == NULL) {
		ScebXCe_1F59F0C4(itemNodes);
		ScebXCe_EC560647(itemNodes);
		freeRosterListImmediate(list);
		return NP_RT_CORE_ERR_NO_MEMORY;
	}
	if (validNodes > 0)
		sceClibMemset(list->items, 0, validNodes * sizeof(NpBasicRosterItem *));

	int status = 0;
	for (SceUInt32 idx = 0; idx < totalNodes; ++idx) {
		BxceNodeId itemNode = ScebXCe_6C46D786(itemNodes, idx);
		if (itemNode == 0)
			continue;

		int subscription = parseSubscriptionAttr(xmlDoc, itemNode);
		const char *jidAttr = ScebXCe_753D9382(xmlDoc, itemNode, "jid");
		if (jidAttr == NULL)
			continue;

		NpBasicRosterItem *item = reinterpret_cast<NpBasicRosterItem *>(
			npRtHeapAlloc(sizeof(NpBasicRosterItem), NULL));
		if (item == NULL) {
			status = NP_RT_CORE_ERR_NO_MEMORY;
			break;
		}

		status = npRtParseJidToNpId(jidAttr, &item->npId);
		if (status < 0) {
			npRtHeapFree(item, NULL);
			break;
		}

		item->subscription = subscription;
		list->items[idx] = item;
		++list->count;
	}

	ScebXCe_1F59F0C4(itemNodes);
	ScebXCe_EC560647(itemNodes);

	if (status < 0) {
		freeRosterListImmediate(list);
		return status;
	}

	*outList = list;
	return 0;
}

/* 0x81317232 */
static int handleRosterPushIq(const NpRtSessionIdentity *session, NpRtStanza *stanza)
{
	if (stanza->stanzaType != NP_RT_XMPP_TYPE_SET)
		return 0;

	BxceNodeId queryNode = 0;
	NpBasicRosterList *rosterList = NULL;
	int ret = npRtFindElementWithXmlns(stanza->xmlDoc, &queryNode, "query",
					   "jabber:iq:roster");
	if (ret >= 0)
		ret = parseRosterQuery(stanza->xmlDoc, queryNode, &rosterList);

	if (ret >= 0) {
		notifyRosterPush(session, rosterList);
		ret = 0;
	} else {
		freeRosterListImmediate(rosterList);
		rosterList = NULL;
	}

	npBasicReleaseRosterList(rosterList);
	return ret;
}

/* 0x813173A0 */
static void onRosterGetResponse(const NpRtSessionIdentity *session, int transactionId,
				int result, NpRtStanza *response, void *userArg)
{
	NpBasicRosterGetCallback callback =
		reinterpret_cast<NpBasicRosterGetCallback>(userArg);
	NpBasicRosterList *rosterList = NULL;
	int status = result;

	if (status >= 0) {
		if (response->stanzaType == NP_RT_XMPP_TYPE_ERROR) {
			NpXmppErrorStanza xmppErr;
			status = npRtParseXmppError(&xmppErr, response);
			if (status >= 0) {
				status = (xmppErr.errorKind != NP_RT_XMPP_SOURCE_CLIENT)
						 ? static_cast<int>(0x80551100u |
								    xmppErr.errorCode)
						 : NP_RT_CORE_ERR_INTERNAL;
				npRtFreeXmppErrorTextList(&xmppErr);
			}
		} else if (response->stanzaType == NP_RT_XMPP_TYPE_RESULT) {
			BxceNodeId queryNode = 0;
			status = npRtFindElementWithXmlns(response->xmlDoc, &queryNode, "query",
							  "jabber:iq:roster");
			if (status >= 0)
				status = parseRosterQuery(response->xmlDoc, queryNode, &rosterList);
			if (status < 0) {
				freeRosterListImmediate(rosterList);
				rosterList = NULL;
			}
		} else {
			status = NP_RT_CORE_ERR_INTERNAL;
		}
	}

	if (status < 0) {
		npBasicReleaseRosterList(rosterList);
		rosterList = NULL;
	}

	if (callback != NULL)
		callback(session, transactionId, status, rosterList);
	npBasicReleaseRosterList(rosterList);
}

/* 0x8131752C */
static int handlePrivacyPushIq(const NpRtSessionIdentity *session, NpRtStanza *stanza)
{
	if (stanza->stanzaType != NP_RT_XMPP_TYPE_SET)
		return 0;

	BxceNodeId queryNode = 0;
	NpBasicPrivacyList *privacyList = NULL;
	int ret = npRtFindElementWithXmlns(stanza->xmlDoc, &queryNode, "query",
					   "jabber:iq:privacy");
	if (ret >= 0) {
		NpBasicPrivacyList *parsed = reinterpret_cast<NpBasicPrivacyList *>(
			npRtHeapAlloc(sizeof(NpBasicPrivacyList), NULL));
		if (parsed == NULL) {
			ret = NP_RT_CORE_ERR_NO_MEMORY;
		} else {
			sceClibMemset(parsed, 0, sizeof(*parsed));
			BxceNodeList *lists = ScebXCe_1FA24DC0(stanza->xmlDoc, queryNode, "list");
			if (lists == NULL || lists->length == 0) {
				ret = NP_RT_CORE_ERR_NOT_FOUND;
			} else {
				BxceNodeId firstList = ScebXCe_6C46D786(lists, 0);
				const char *nameAttr = ScebXCe_753D9382(stanza->xmlDoc,
									firstList, "name");
				ret = npRtDupString(&parsed->listName, nameAttr, NULL, 0);
			}
			if (lists != NULL) {
				ScebXCe_1F59F0C4(lists);
				ScebXCe_EC560647(lists);
			}
			if (ret >= 0) {
				privacyList = parsed;
				ret = 0;
			} else {
				freePrivacyListImmediate(parsed);
			}
		}
	}

	if (ret >= 0) {
		notifyPrivacyPush(session, privacyList);
		ret = 0;
	}

	npBasicReleasePrivacyList(privacyList);
	return ret;
}

} /* namespace */

/* --------------------------------------------------------------------------
 * Public SceNpBasic API (0x81316C1C..0x813176AB)
 * -------------------------------------------------------------------------- */
/* 0x81316C1C */
int npBasicInit(void)
{
	int ret = basicMutexInit();
	if (ret >= 0)
		ret = basicRegisterDefaultHandler();
	if (ret >= 0)
		return 0;
	basicMutexTerm();
	return ret;
}

/* 0x81316C3E */
int npBasicTerm(void)
{
	basicUnregisterDefaultHandler();
	return basicMutexTerm();
}

/* 0x81316E64 */
void npBasicSetCallbacks(const NpBasicCallbacks *callbacks)
{
	if (callbacks != NULL)
		sceClibMemcpy_safe(&s_basicCallbacks, callbacks, sizeof(s_basicCallbacks));
	else
		sceClibMemset(&s_basicCallbacks, 0, sizeof(s_basicCallbacks));
}

/* 0x81316F54 */
void npBasicReleaseRosterList(NpBasicRosterList *rosterList)
{
	if (rosterList == NULL)
		return;
	if (sceAtomicDecrement32AcqRel(&rosterList->refCount) == 0)
		freeRosterListImmediate(rosterList);
}

/* 0x813172F6 */
int npBasicRequestRoster(int sessionHandle, int transactionId, NpBasicRosterGetCallback callback)
{
	NpRtStanzaInit init;
	sceClibMemset(&init, 0, sizeof(init));
	init.stanzaType = NP_RT_XMPP_TYPE_GET;

	NpRtStanza *stanza = NULL;
	int ret = npRtAllocStanza(&stanza, &init);
	if (ret >= 0) {
		ret = npRtCreateXmlDocWithRoot(&stanza->xmlDoc, NULL, "query", "jabber:iq:roster");
		if (ret >= 0) {
			ret = npRtSendIqRequest(sessionHandle, s_basicHandlerId, transactionId,
						stanza, onRosterGetResponse,
						reinterpret_cast<void *>(callback));
		}
	}

	npRtReleaseStanza(stanza);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813174F4 */
void npBasicReleasePrivacyList(NpBasicPrivacyList *privacyList)
{
	if (privacyList == NULL)
		return;
	if (sceAtomicDecrement32AcqRel(&privacyList->refCount) == 0)
		freePrivacyListImmediate(privacyList);
}

int npBasicGetHandlerId(void)
{
	return s_basicHandlerId;
}
