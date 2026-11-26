/*
 * np_rt_imps.cpp: SceNpRt IMPS stanza construction, parsing, cloning, and
 * session IQ/presence/message send helpers (0x813F3FE8..0x813F4D73).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>
#include <np_common/np_common.h>
#include "np_rt_core.h"
#include "np_rt_ext.h"
#include "../imps/np_imps_core.h"

namespace {

typedef void (*ImpsIqDoneCallback)(const NpRtSessionIdentity *session, int transactionId, int result);

/* 0x8131C5AC: Adapter invoking caller's NpRtSendDoneCallback stored in userArg */
static void onRtImpsSendDone(const NpRtSessionIdentity *session, int transactionId, int result,
			     void *userArg)
{
	NpRtSendDoneCallback cb = reinterpret_cast<NpRtSendDoneCallback>(userArg);
	if (cb != NULL)
		cb(session, transactionId, result, NULL);
}

/* 0x813F41E2: Adapter translating IQ response stanza into NP error code */
static void onRtImpsIqResponse(const NpRtSessionIdentity *session, int transactionId, int result,
			       NpRtStanza *response, void *userArg)
{
	int status = result;
	if (result >= 0) {
		if (response->stanzaType == NP_RT_XMPP_TYPE_ERROR) {
			NpXmppErrorStanza xmppErr;
			status = npRtParseXmppError(&xmppErr, response);
			if (status >= 0) {
				status = (xmppErr.errorKind == NP_RT_XMPP_SOURCE_CLIENT)
					? NP_RT_CORE_ERR_INTERNAL
					: static_cast<int>(0x80551100u | xmppErr.errorCode);
				npRtFreeXmppErrorTextList(&xmppErr);
			}
		} else if (response->stanzaType == NP_RT_XMPP_TYPE_RESULT) {
			status = 0;
		} else {
			status = NP_RT_CORE_ERR_INTERNAL;
		}
	}

	ImpsIqDoneCallback cb = reinterpret_cast<ImpsIqDoneCallback>(userArg);
	if (cb != NULL)
		cb(session, transactionId, status);
}

static void fillPresenceBundleFromUpdate(NpRtPresenceExtBundle *bundle,
					 const NpImpsCorePresenceUpdate *update)
{
	sceClibMemset(bundle, 0, sizeof(NpRtPresenceExtBundle));
	bundle->titleList = update->titleList;
	bundle->delayTick = reinterpret_cast<SceRtcTick *>(update->lastOnlineTick);
	bundle->capsList = update->capsList;
	bundle->extData = update->dataList;
	bundle->storedExt = update->storedExt;
}

static void fillMessageBundleFromMessage(NpRtMessageExtBundle *bundle,
					 const NpImpsCoreMessage *msg)
{
	sceClibMemset(bundle, 0, sizeof(NpRtMessageExtBundle));
	bundle->contentExt = reinterpret_cast<NpRtContentExt *>(msg->messageExt);
	bundle->messageExt = reinterpret_cast<NpRtMessageMetaExt *>(msg->contentExt);
	bundle->extData = msg->extData;
	bundle->storedExt = reinterpret_cast<NpRtStoredExt *>(msg->storedExt);
	bundle->notifExt = msg->notificationExt;
	bundle->antiblock = msg->storeOffline;
}

} /* namespace */

/* 0x813F3FE8 */
int npRtImpsSendPresence(int rtSessionHandle, int transactionId, NpRtPresenceStanza *stanza,
			 NpRtSendDoneCallback callback)
{
	return npRtSendPresence(rtSessionHandle, npBasicGetHandlerId(), transactionId, stanza,
				onRtImpsSendDone, reinterpret_cast<void *>(callback));
}

/* 0x813F4012 */
int npRtImpsSendOfflinePresence(int rtSessionHandle)
{
	int txId = 0;
	int ret = npRtAllocTransactionId(rtSessionHandle, &txId);
	if (ret < 0)
		return ret;

	NpRtStanzaInit init;
	sceClibMemset(&init, 0, sizeof(init));
	init.stanzaType = NP_RT_XMPP_TYPE_UNAVAILABLE;

	NpRtPresenceStanza *stanza = NULL;
	ret = npRtAllocPresenceStanza(&stanza, &init);
	if (ret >= 0) {
		ret = npRtSendPresence(rtSessionHandle, npBasicGetHandlerId(), txId, stanza,
				       onRtImpsSendDone, NULL);
		if (ret >= 0)
			ret = 0;
	}
	if (stanza != NULL)
		npRtReleasePresenceStanza(stanza);
	return ret;
}

/* 0x813F409A */
int npRtImpsSendStandbyIq(int rtSessionHandle, int transactionId, int standbyMode,
			  NpRtSendDoneCallback callback)
{
	NpRtStanzaInit init;
	sceClibMemset(&init, 0, sizeof(init));
	init.stanzaType = NP_RT_XMPP_TYPE_SET;

	NpRtStanza *stanza = NULL;
	int ret = npRtAllocStanza(&stanza, &init);
	if (ret < 0) {
		npRtReleaseStanza(stanza);
		return ret;
	}

	const char *typeStr = NULL;
	if (standbyMode == 1)
		typeStr = "3";
	else if (standbyMode == 2)
		typeStr = "4";

	BxceDocument *doc = NULL;
	BxceNodeId rootNode = 0;
	BxceAttributeList *attrs = NULL;

	if (typeStr == NULL) {
		ret = NP_RT_CORE_ERR_INVALID_ARG;
	} else {
		ret = npRtCreateXmlDocWithRoot(&doc, &rootNode, "query",
					       "http://playstation.net/np/rt/session");
		if (ret >= 0)
			ret = npRtCreateAttrList(&attrs);
		if (ret >= 0)
			ret = npRtAttrListAddString(attrs, "type", typeStr);
		if (ret >= 0) {
			if (ScebXCe_AC0E4F67(doc, rootNode, "x", attrs, NULL, 0) != 0) {
				npRtDestroyAttrList(attrs);
				stanza->xmlDoc = doc;
				doc = NULL;
				attrs = NULL;
				ret = 0;
			} else {
				ret = NP_RT_CORE_ERR_XML_ATTR;
			}
		}
	}

	if (ret < 0) {
		npRtDestroyAttrList(attrs);
		npRtDestroyBxceDocument(doc);
	} else {
		ret = npRtSendIqRequest(rtSessionHandle, npBasicGetHandlerId(), transactionId,
					stanza, onRtImpsIqResponse,
					reinterpret_cast<void *>(callback));
		if (ret >= 0)
			ret = 0;
	}

	npRtReleaseStanza(stanza);
	return ret;
}

/* 0x813F4270 */
int npRtImpsBuildPresenceStanza(const NpImpsCorePresenceUpdate *update,
				NpRtPresenceStanza **outStanza)
{
	NpRtPresenceStanza *stanza = reinterpret_cast<NpRtPresenceStanza *>(
		npRtHeapAlloc(sizeof(NpRtPresenceStanza), NULL));
	if (stanza == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(stanza, 0, sizeof(NpRtPresenceStanza));
	int ret = npRtStanzaHeaderInit(reinterpret_cast<NpRtStanzaInit *>(&stanza->toJid),
				       NP_RT_XMPP_TYPE_NONE, NULL, NULL);
	if (ret >= 0) {
		stanza->show = update->platform;
		if (update->comment != NULL)
			ret = npRtAllocLangTextNode(&stanza->statusList, update->comment,
						    NP_RT_XMPP_LANG_NONE);
		if (ret >= 0) {
			stanza->priority = 0;
			stanza->xmlDoc = NULL;

			NpRtPresenceExtBundle bundle;
			fillPresenceBundleFromUpdate(&bundle, update);
			ret = npRtExtSerializePresenceBundle(&stanza->xmlDoc, &bundle);
			if (ret >= 0) {
				*outStanza = stanza;
				return 0;
			}
		}
	}

	npRtReleasePresenceStanza(stanza);
	return ret;
}

/* 0x813F4324 */
int npRtImpsParsePresenceStanza(NpImpsCorePresenceUpdate **outUpdate,
				const NpRtPresenceStanza *stanza, void *customAlloc)
{
	NpRtPresenceExtBundle bundle;
	sceClibMemset(&bundle, 0, sizeof(bundle));

	if (stanza->stanzaType == NP_RT_XMPP_TYPE_ERROR) {
		NpXmppErrorStanza xmppErr;
		int errRet = npRtParsePresenceError(&xmppErr, stanza);
		if (errRet >= 0) {
			errRet = (xmppErr.errorKind == NP_RT_XMPP_SOURCE_CLIENT)
				? NP_RT_CORE_ERR_INTERNAL
				: static_cast<int>(0x80551100u | xmppErr.errorCode);
			npRtFreeXmppErrorTextList(&xmppErr);
		}
		return errRet;
	}

	NpImpsCorePresenceUpdate *update = reinterpret_cast<NpImpsCorePresenceUpdate *>(
		npRtHeapAlloc(sizeof(NpImpsCorePresenceUpdate), customAlloc));
	if (update == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(update, 0, sizeof(NpImpsCorePresenceUpdate));

	int ret = npRtExtParsePresenceBundle(&bundle, stanza, customAlloc, 0);
	if (ret >= 0) {
		update->titleList = bundle.titleList;
		bundle.titleList = NULL;
		update->lastOnlineTick = reinterpret_cast<paf::rtc::Tick *>(bundle.delayTick);
		bundle.delayTick = NULL;
		update->capsList = bundle.capsList;
		bundle.capsList = NULL;
		update->dataList = bundle.extData;
		bundle.extData = NULL;
		update->storedExt = bundle.storedExt;
		bundle.storedExt = NULL;

		update->platform = -1;
		switch (stanza->stanzaType) {
		case NP_RT_XMPP_TYPE_NONE:
			update->onlineStatus = 0;
			update->platform = stanza->show;
			break;
		case NP_RT_XMPP_TYPE_SUBSCRIBE:
			update->onlineStatus = 4;
			break;
		case NP_RT_XMPP_TYPE_SUBSCRIBED:
			update->onlineStatus = 5;
			break;
		case NP_RT_XMPP_TYPE_UNAVAILABLE:
			update->onlineStatus = 3;
			break;
		case NP_RT_XMPP_TYPE_UNSUBSCRIBE:
			update->onlineStatus = 6;
			break;
		case NP_RT_XMPP_TYPE_UNSUBSCRIBED:
			update->onlineStatus = 7;
			break;
		case NP_RT_XMPP_TYPE_STANDBY:
			update->onlineStatus = 2;
			if (bundle.sessionExt != NULL && bundle.sessionExt->sessionType == 1)
				update->onlineStatus = 1;
			break;
		default:
			update->onlineStatus = -1;
			break;
		}

		if (stanza->statusList != NULL)
			ret = npRtDupString(&update->comment, stanza->statusList->text,
					    customAlloc, 0);
		else
			update->comment = NULL;

		if (ret >= 0) {
			npRtExtFreePresenceBundle(&bundle, customAlloc, 0);
			*outUpdate = update;
			return 0;
		}
	}

	npRtExtFreePresenceBundle(&bundle, customAlloc, 0);
	NpRtPresenceExtBundle cleanup;
	fillPresenceBundleFromUpdate(&cleanup, update);
	npRtExtFreePresenceBundle(&cleanup, customAlloc, 0);
	npRtHeapFree(update, customAlloc);
	return ret;
}

/* 0x813F44C4 */
void npRtImpsFreePresenceUpdate(NpImpsCorePresenceUpdate *update, void *customAlloc)
{
	if (update == NULL)
		return;

	NpRtPresenceExtBundle bundle;
	fillPresenceBundleFromUpdate(&bundle, update);
	npRtExtFreePresenceBundle(&bundle, customAlloc, 0);
	npRtHeapFree(update, customAlloc);
}

/* 0x813F451E */
int npRtImpsClonePresenceUpdate(NpImpsCorePresenceUpdate **outCopy,
				const NpImpsCorePresenceUpdate *src)
{
	NpRtPresenceExtBundle srcBundle;
	fillPresenceBundleFromUpdate(&srcBundle, src);

	NpRtPresenceExtBundle dstBundle;
	sceClibMemset(&dstBundle, 0, sizeof(dstBundle));

	NpImpsCorePresenceUpdate *copy = reinterpret_cast<NpImpsCorePresenceUpdate *>(
		npRtHeapAlloc(sizeof(NpImpsCorePresenceUpdate), NULL));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, sizeof(NpImpsCorePresenceUpdate));
	copy->onlineStatus = src->onlineStatus;
	copy->platform = src->platform;

	int ret = 0;
	if (src->comment != NULL)
		ret = npRtDupString(&copy->comment, src->comment, NULL, 0);
	if (ret >= 0)
		ret = npRtExtClonePresenceBundle(&dstBundle, &srcBundle, NULL, 0);
	if (ret >= 0) {
		copy->titleList = dstBundle.titleList;
		dstBundle.titleList = NULL;
		copy->lastOnlineTick = reinterpret_cast<paf::rtc::Tick *>(dstBundle.delayTick);
		dstBundle.delayTick = NULL;
		copy->capsList = dstBundle.capsList;
		dstBundle.capsList = NULL;
		copy->dataList = dstBundle.extData;
		dstBundle.extData = NULL;
		copy->storedExt = dstBundle.storedExt;
		dstBundle.storedExt = NULL;
		*outCopy = copy;
		return 0;
	}

	npRtExtFreePresenceBundle(&dstBundle, NULL, 0);
	npRtImpsFreePresenceUpdate(copy, NULL);
	return ret;
}

/* 0x813F4618 */
int npRtImpsBuildMessageStanza(const SceNpId *targetNpId, const NpImpsCoreMessage *msg,
			       NpRtMessageStanza **outStanza)
{
	NpJid targetJid;
	sceClibMemset(&targetJid, 0, sizeof(targetJid));

	SceSize jidBufSize = 0;
	int ret = SceNpCommon_40CC4D15(&targetJid, targetNpId, NULL, 0, &jidBufSize);
	if (ret < 0)
		return ret;

	char *jidBuf = reinterpret_cast<char *>(npRtHeapAlloc(jidBufSize, NULL));
	if (jidBuf == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	NpRtMessageStanza *stanza = NULL;
	ret = SceNpCommon_40CC4D15(&targetJid, targetNpId, jidBuf, jidBufSize, NULL);
	if (ret >= 0) {
		stanza = reinterpret_cast<NpRtMessageStanza *>(
			npRtHeapAlloc(sizeof(NpRtMessageStanza), NULL));
		if (stanza == NULL) {
			ret = NP_RT_CORE_ERR_NO_MEMORY;
		} else {
			sceClibMemset(stanza, 0, sizeof(NpRtMessageStanza));
			int xmppType = -1;
			switch (msg->type) {
			case 0:
				xmppType = NP_RT_XMPP_TYPE_CHAT;
				break;
			case 1:
				xmppType = NP_RT_XMPP_TYPE_GROUPCHAT;
				break;
			case 2:
				xmppType = NP_RT_XMPP_TYPE_NORMAL;
				break;
			case 3:
				xmppType = NP_RT_XMPP_TYPE_HEADLINE;
				break;
			case 4:
				xmppType = NP_RT_XMPP_TYPE_NOTIFICATION;
				break;
			default:
				ret = NP_RT_CORE_ERR_INVALID_ARG;
				break;
			}

			if (ret >= 0)
				ret = npRtStanzaHeaderInit(
					reinterpret_cast<NpRtStanzaInit *>(&stanza->toJid),
					xmppType, NULL, &targetJid);
			if (ret >= 0 && msg->subject != NULL)
				ret = npRtAllocLangTextNode(&stanza->subjectList, msg->subject,
							    NP_RT_XMPP_LANG_NONE);
			if (ret >= 0 && msg->body != NULL)
				ret = npRtAllocLangTextNode(&stanza->bodyList, msg->body,
							    NP_RT_XMPP_LANG_NONE);
			if (ret >= 0) {
				NpRtMessageExtBundle bundle;
				fillMessageBundleFromMessage(&bundle, msg);
				ret = npRtExtSerializeMessageBundle(&stanza->xmlDoc, &bundle);
			}
			if (ret >= 0) {
				npRtHeapFree(jidBuf, NULL);
				*outStanza = stanza;
				return 0;
			}
		}
	}

	npRtHeapFree(jidBuf, NULL);
	npRtReleaseMessageStanza(stanza);
	return ret;
}

/* 0x813F4782 */
int npRtImpsParseMessageStanza(NpImpsCoreMessage **outMsg, const NpRtMessageStanza *stanza,
			       void *customAlloc)
{
	if (stanza->stanzaType == NP_RT_XMPP_TYPE_ERROR) {
		NpXmppErrorStanza xmppErr;
		int errRet = npRtParseMessageError(&xmppErr, stanza);
		if (errRet >= 0) {
			errRet = (xmppErr.errorKind == NP_RT_XMPP_SOURCE_CLIENT)
				? NP_RT_CORE_ERR_INTERNAL
				: static_cast<int>(0x80551100u | xmppErr.errorCode);
			npRtFreeXmppErrorTextList(&xmppErr);
		}
		return errRet;
	}

	NpImpsCoreMessage *msg = reinterpret_cast<NpImpsCoreMessage *>(
		npRtHeapAlloc(sizeof(NpImpsCoreMessage), customAlloc));
	if (msg == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(msg, 0, sizeof(NpImpsCoreMessage));

	switch (stanza->stanzaType) {
	case NP_RT_XMPP_TYPE_CHAT:
		msg->type = 0;
		break;
	case NP_RT_XMPP_TYPE_GROUPCHAT:
		msg->type = 1;
		break;
	case NP_RT_XMPP_TYPE_HEADLINE:
		msg->type = 3;
		break;
	case NP_RT_XMPP_TYPE_NORMAL:
		msg->type = 2;
		break;
	case NP_RT_XMPP_TYPE_NOTIFICATION:
		msg->type = 4;
		break;
	default:
		msg->type = -1;
		break;
	}

	int ret = 0;
	if (stanza->subjectList != NULL)
		ret = npRtDupString(&msg->subject, stanza->subjectList->text, customAlloc, 0);
	if (ret >= 0 && stanza->bodyList != NULL)
		ret = npRtDupString(&msg->body, stanza->bodyList->text, customAlloc, 0);
	if (ret >= 0 && stanza->threadId != NULL)
		ret = npRtDupString(&msg->thread, stanza->threadId, customAlloc, 0);

	if (ret >= 0) {
		NpRtMessageExtBundle bundle;
		sceClibMemset(&bundle, 0, sizeof(bundle));
		ret = npRtExtParseMessageBundle(&bundle, stanza, customAlloc, 0);
		if (ret >= 0) {
			msg->extData = bundle.extData;
			msg->messageExt = bundle.contentExt;
			msg->storedExt = bundle.storedExt;
			msg->notificationExt = bundle.notifExt;
			msg->contentExt = bundle.messageExt;
			*outMsg = msg;
			return 0;
		}
	}

	npRtImpsFreeMessage(msg, customAlloc);
	return ret;
}

/* 0x813F492C */
void npRtImpsFreeMessage(NpImpsCoreMessage *msg, void *customAlloc)
{
	if (msg == NULL)
		return;

	npRtFreeString(msg->subject, customAlloc, 0);
	npRtFreeString(msg->body, customAlloc, 0);
	npRtFreeString(msg->thread, customAlloc, 0);

	NpRtMessageExtBundle bundle;
	fillMessageBundleFromMessage(&bundle, msg);
	npRtExtFreeMessageBundle(&bundle, customAlloc, 0);
	npRtHeapFree(msg, customAlloc);
}

/* 0x813F49AC */
int npRtImpsCloneMessage(NpImpsCoreMessage **outCopy, const NpImpsCoreMessage *src)
{
	NpRtMessageExtBundle srcBundle;
	fillMessageBundleFromMessage(&srcBundle, src);

	NpRtMessageExtBundle dstBundle;
	sceClibMemset(&dstBundle, 0, sizeof(dstBundle));

	NpImpsCoreMessage *copy = reinterpret_cast<NpImpsCoreMessage *>(
		npRtHeapAlloc(sizeof(NpImpsCoreMessage), NULL));
	if (copy == NULL)
		return NP_RT_CORE_ERR_NO_MEMORY;

	sceClibMemset(copy, 0, sizeof(NpImpsCoreMessage));
	copy->type = src->type;

	int ret = 0;
	if (src->subject != NULL)
		ret = npRtDupString(&copy->subject, src->subject, NULL, 0);
	if (ret >= 0 && src->body != NULL)
		ret = npRtDupString(&copy->body, src->body, NULL, 0);
	if (ret >= 0 && src->thread != NULL)
		ret = npRtDupString(&copy->thread, src->thread, NULL, 0);
	if (ret >= 0)
		ret = npRtExtCloneMessageBundle(&dstBundle, &srcBundle, NULL, 0);

	if (ret >= 0) {
		copy->extData = dstBundle.extData;
		dstBundle.extData = NULL;
		copy->messageExt = dstBundle.contentExt;
		dstBundle.contentExt = NULL;
		copy->storedExt = dstBundle.storedExt;
		dstBundle.storedExt = NULL;
		copy->notificationExt = dstBundle.notifExt;
		dstBundle.notifExt = NULL;
		copy->contentExt = dstBundle.messageExt;
		dstBundle.messageExt = NULL;
		copy->storeOffline = dstBundle.antiblock;
		*outCopy = copy;
		return 0;
	}

	npRtExtFreeMessageBundle(&dstBundle, NULL, 0);
	npRtImpsFreeMessage(copy, NULL);
	return ret;
}

/* 0x813F4B0C */
int npRtImpsSendMessage(int rtSessionHandle, int transactionId, NpRtMessageStanza *stanza,
			NpRtSendDoneCallback callback)
{
	return npRtSendMessage(rtSessionHandle, npBasicGetHandlerId(), transactionId, stanza,
			       onRtImpsSendDone, reinterpret_cast<void *>(callback));
}

/* 0x813F4B38 */
int npRtImpsFetchStored(int rtSessionHandle, int transactionId, NpRtSendDoneCallback callback)
{
	NpRtStanzaInit init;
	sceClibMemset(&init, 0, sizeof(init));
	init.stanzaType = NP_RT_XMPP_TYPE_GET;

	NpRtStanza *stanza = NULL;
	int ret = npRtAllocStanza(&stanza, &init);
	if (ret < 0) {
		npRtReleaseStanza(stanza);
		return ret;
	}

	BxceDocument *doc = NULL;
	BxceNodeId rootNode = 0;
	BxceAttributeList *attrs = NULL;

	ret = npRtCreateXmlDocWithRoot(&doc, &rootNode, "fetch",
				       "http://playstation.net/np/rt/stored");
	if (ret >= 0) {
		ret = npRtCreateAttrList(&attrs);
		if (ret >= 0)
			ret = npRtAttrListAddString(attrs, "resource", "1");
		if (ret >= 0) {
			if (ScebXCe_AC0E4F67(doc, rootNode, "x", attrs, NULL, 0) != 0) {
				npRtDestroyAttrList(attrs);
				stanza->xmlDoc = doc;
				doc = NULL;
				attrs = NULL;
				ret = 0;
			} else {
				ret = NP_RT_CORE_ERR_XML_ATTR;
			}
		}
		if (ret < 0) {
			npRtDestroyAttrList(attrs);
			npRtDestroyBxceDocument(doc);
		}
	}

	if (ret >= 0) {
		ret = npRtSendIqRequest(rtSessionHandle, npBasicGetHandlerId(), transactionId,
					stanza, onRtImpsIqResponse,
					reinterpret_cast<void *>(callback));
		if (ret >= 0)
			ret = 0;
	}

	npRtReleaseStanza(stanza);
	return ret;
}

/* 0x813F4C5A */
int npRtImpsDeleteStored(int rtSessionHandle, int transactionId, SceUInt64 storedId,
			 NpRtSendDoneCallback callback)
{
	NpRtStanzaInit init;
	sceClibMemset(&init, 0, sizeof(init));
	init.stanzaType = NP_RT_XMPP_TYPE_GET;

	NpRtStanza *stanza = NULL;
	int ret = npRtAllocStanza(&stanza, &init);
	if (ret < 0) {
		npRtReleaseStanza(stanza);
		return ret;
	}

	BxceDocument *doc = NULL;
	BxceNodeId rootNode = 0;
	BxceAttributeList *attrs = NULL;

	ret = npRtCreateXmlDocWithRoot(&doc, &rootNode, "delete",
				       "http://playstation.net/np/rt/stored");
	if (ret >= 0)
		ret = npRtCreateAttrList(&attrs);
	if (ret >= 0)
		ret = npRtAttrListAddUInt64(attrs, "id", storedId);
	if (ret >= 0) {
		if (ScebXCe_AC0E4F67(doc, rootNode, "item", attrs, NULL, 0) != 0) {
			stanza->xmlDoc = doc;
			doc = NULL;
			ret = 0;
		} else {
			ret = NP_RT_CORE_ERR_INTERNAL;
		}
	}

	if (ret < 0) {
		npRtDestroyAttrList(attrs);
		npRtDestroyBxceDocument(doc);
	} else {
		ret = npRtSendIqRequest(rtSessionHandle, npBasicGetHandlerId(), transactionId,
					stanza, onRtImpsIqResponse,
					reinterpret_cast<void *>(callback));
		if (ret >= 0)
			ret = 0;
	}

	npRtReleaseStanza(stanza);
	return ret;
}
