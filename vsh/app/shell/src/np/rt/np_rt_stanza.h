/*
 * np_rt_stanza.h: SceNpRt XMPP stanza, error parser, enum conversion, and
 * bXCe DOM helper interfaces (0x8130F3E4..0x8131031F).
 */
#ifndef SHELL_NP_RT_STANZA_H
#define SHELL_NP_RT_STANZA_H

#include <stddef.h>
#include <scetypes.h>
#include <np_common/np_common.h>

struct BxceDocument;
struct BxceAttributeList;
typedef SceUInt32 BxceNodeId;

/* SceNpRt / SceNetXmpp error codes */
#define NP_RT_CORE_ERR_INTERNAL		-2141908480	/* 0x80551200 */
#define NP_RT_CORE_ERR_INVALID_ARG	-2141908477	/* 0x80551203 */
#define NP_RT_CORE_ERR_NO_MEMORY	-2141908476	/* 0x80551204 */
#define NP_RT_CORE_ERR_NOT_FOUND	-2141908439	/* 0x80551229 */
#define NP_RT_CORE_ERR_XML_ATTR		-2141908438	/* 0x8055122A */

/* XMPP stanza kind tags (used by NpRtAnyStanza) */
enum NpRtStanzaKind {
	NP_RT_STANZA_KIND_NONE		= 0,
	NP_RT_STANZA_KIND_MESSAGE	= 1,
	NP_RT_STANZA_KIND_PRESENCE	= 2,
	NP_RT_STANZA_KIND_IQ		= 3,
	NP_RT_STANZA_KIND_STREAM	= 4,
	NP_RT_STANZA_KIND_STREAM_END	= 5
};

/* XMPP stanza "type" attribute enum (0x815461B8) */
enum NpRtXmppStanzaType {
	NP_RT_XMPP_TYPE_NONE		= 0,
	NP_RT_XMPP_TYPE_ERROR		= 1,
	NP_RT_XMPP_TYPE_CHAT		= 2,
	NP_RT_XMPP_TYPE_GROUPCHAT	= 3,
	NP_RT_XMPP_TYPE_HEADLINE	= 4,
	NP_RT_XMPP_TYPE_NORMAL		= 5,
	NP_RT_XMPP_TYPE_NOTIFICATION	= 6,
	NP_RT_XMPP_TYPE_PROBE		= 7,
	NP_RT_XMPP_TYPE_SUBSCRIBE	= 8,
	NP_RT_XMPP_TYPE_SUBSCRIBED	= 9,
	NP_RT_XMPP_TYPE_UNAVAILABLE	= 10,
	NP_RT_XMPP_TYPE_UNSUBSCRIBE	= 11,
	NP_RT_XMPP_TYPE_UNSUBSCRIBED	= 12,
	NP_RT_XMPP_TYPE_STANDBY		= 13,
	NP_RT_XMPP_TYPE_GET		= 14,
	NP_RT_XMPP_TYPE_RESULT		= 15,
	NP_RT_XMPP_TYPE_SET		= 16
};

/* XMPP presence <show> enum (0x81546248) */
enum NpRtXmppPresenceShow {
	NP_RT_XMPP_SHOW_NONE		= 0,
	NP_RT_XMPP_SHOW_AWAY		= 1,
	NP_RT_XMPP_SHOW_CHAT		= 2,
	NP_RT_XMPP_SHOW_DND		= 3,
	NP_RT_XMPP_SHOW_XA		= 4
};

/* XMPP xml:lang enum (0x81546278) */
enum NpRtXmppLanguage {
	NP_RT_XMPP_LANG_NONE		= 0,
	NP_RT_XMPP_LANG_EN		= 1
};

/* XMPP <error type="..."> enum (0x81546290) */
enum NpRtXmppErrorType {
	NP_RT_XMPP_ERRTYPE_NONE		= 0,
	NP_RT_XMPP_ERRTYPE_CANCEL	= 1,
	NP_RT_XMPP_ERRTYPE_CONTINUE	= 2,
	NP_RT_XMPP_ERRTYPE_MODIFY	= 3,
	NP_RT_XMPP_ERRTYPE_AUTH		= 4,
	NP_RT_XMPP_ERRTYPE_WAIT		= 5
};

/* XMPP stanza error condition enum (0x815462C8) */
enum NpRtXmppErrorCondition {
	NP_RT_XMPP_COND_NONE			= 0,
	NP_RT_XMPP_COND_BAD_REQUEST		= 1,
	NP_RT_XMPP_COND_CONFLICT		= 2,
	NP_RT_XMPP_COND_FEATURE_NOT_IMPL	= 3,
	NP_RT_XMPP_COND_FORBIDDEN		= 4,
	NP_RT_XMPP_COND_GONE			= 5,
	NP_RT_XMPP_COND_INTERNAL_SERVER_ERROR	= 6,
	NP_RT_XMPP_COND_ITEM_NOT_FOUND		= 7,
	NP_RT_XMPP_COND_JID_MALFORMED		= 8,
	NP_RT_XMPP_COND_NOT_ACCEPTABLE		= 9,
	NP_RT_XMPP_COND_NOT_ALLOWED		= 10,
	NP_RT_XMPP_COND_NOT_AUTHORIZED		= 11,
	NP_RT_XMPP_COND_PAYMENT_REQUIRED	= 12,
	NP_RT_XMPP_COND_RECIPIENT_UNAVAILABLE	= 13,
	NP_RT_XMPP_COND_REDIRECT		= 14,
	NP_RT_XMPP_COND_REGISTRATION_REQUIRED	= 15,
	NP_RT_XMPP_COND_REMOTE_SERVER_NOT_FOUND	= 16,
	NP_RT_XMPP_COND_REMOTE_SERVER_TIMEOUT	= 17,
	NP_RT_XMPP_COND_RESOURCE_CONSTRAINT	= 18,
	NP_RT_XMPP_COND_SERVICE_UNAVAILABLE	= 19,
	NP_RT_XMPP_COND_SUBSCRIPTION_REQUIRED	= 20,
	NP_RT_XMPP_COND_UNDEFINED_CONDITION	= 21,
	NP_RT_XMPP_COND_UNEXPECTED_REQUEST	= 22
};

/* PlayStation RT error source enum (0x81546388) */
enum NpRtXmppErrorSource {
	NP_RT_XMPP_SOURCE_NONE		= 0,
	NP_RT_XMPP_SOURCE_SERVER	= 1,
	NP_RT_XMPP_SOURCE_CLIENT	= 2
};

/* Linked-list node for localized text elements (subject, body, status, error text) */
struct NpRtLangTextNode {
	NpRtLangTextNode *next;		/* +0x00 */
	int lang;			/* +0x04 */
	char *text;			/* +0x08 */
};

/* Common 44-byte XMPP stanza header embedded in Message, Presence, and IQ stanzas */
struct NpRtStanzaInit {
	NpJid toJid;			/* +0x00 (16 bytes) */
	NpJid fromJid;			/* +0x10 (16 bytes) */
	const char *stanzaId;		/* +0x20 */
	int stanzaType;			/* +0x24 */
	int lang;			/* +0x28 */
};

/* XMPP IQ stanza (52 bytes, 0x8130F9C6 / 0x8130FA1E) */
struct NpRtStanza {
	volatile SceInt32 refCount;	/* +0x00 */
	NpJid toJid;			/* +0x04 (16 bytes) */
	NpJid fromJid;			/* +0x14 (16 bytes) */
	char *stanzaId;			/* +0x24 */
	int stanzaType;			/* +0x28 */
	int lang;			/* +0x2C */
	BxceDocument *xmlDoc;		/* +0x30 */
};

/* XMPP Message stanza (64 bytes, 0x8130F89C) */
struct NpRtMessageStanza {
	volatile SceInt32 refCount;	/* +0x00 */
	NpJid toJid;			/* +0x04 (16 bytes) */
	NpJid fromJid;			/* +0x14 (16 bytes) */
	char *stanzaId;			/* +0x24 */
	int stanzaType;			/* +0x28 */
	int lang;			/* +0x2C */
	NpRtLangTextNode *subjectList;	/* +0x30 */
	NpRtLangTextNode *bodyList;	/* +0x34 */
	char *threadId;			/* +0x38 */
	BxceDocument *xmlDoc;		/* +0x3C */
};

/* XMPP Presence stanza (68 bytes, 0x8130F90A / 0x8130F968) */
struct NpRtPresenceStanza {
	volatile SceInt32 refCount;	/* +0x00 */
	NpJid toJid;			/* +0x04 (16 bytes) */
	NpJid fromJid;			/* +0x14 (16 bytes) */
	char *stanzaId;			/* +0x24 */
	int stanzaType;			/* +0x28 */
	int lang;			/* +0x2C */
	int show;			/* +0x30 */
	NpRtLangTextNode *statusList;	/* +0x34 */
	SceInt8 priority;		/* +0x38 */
	SceUInt8 padAfterPriority[7];	/* +0x39 */
	BxceDocument *xmlDoc;		/* +0x40 */
};

/* Tagged stanza container (8 bytes, 0x8130FA7A) */
struct NpRtAnyStanza {
	int kind;			/* +0x00: NpRtStanzaKind */
	void *stanza;			/* +0x04 */
};

/* Parsed / outbound XMPP error payload (28 bytes, 0x8130F3E4 / 0x813135CA / 0x81316ED4) */
struct NpXmppErrorStanza {
	int errorType;			/* +0x00: NpRtXmppErrorType */
	int condition;			/* +0x04: NpRtXmppErrorCondition */
	NpRtLangTextNode *textList;	/* +0x08 */
	int errorKind;			/* +0x0C: NpRtXmppErrorSource (1=server, 2=client) */
	SceUInt8 errorCode;		/* +0x10: hex code from <x xmlns=".../np/rt/error"> */
	SceUInt8 padAfterCode[7];	/* +0x11 */
	NpRtStanza *origStanza;		/* +0x18 */
};

/* XMPP error parsers (0x8130F3E4..0x8130F79F) */
int npRtParseErrorDocument(NpXmppErrorStanza *outError, BxceDocument *xmlDoc,
			   const char *errorElemName, const char *errorXmlns,
			   SceSize xmlnsLen);
int npRtParseStreamError(NpXmppErrorStanza *outError, BxceDocument *xmlDoc);
void npRtFreeXmppErrorTextList(NpXmppErrorStanza *errorStanza);
int npRtParseMessageError(NpXmppErrorStanza *outError, const NpRtMessageStanza *stanza);
int npRtParsePresenceError(NpXmppErrorStanza *outError, const NpRtPresenceStanza *stanza);
int npRtParseXmppError(NpXmppErrorStanza *outError, const NpRtStanza *stanza);

/* Stanza header and ref-counted lifecycle helpers (0x8130F7A0..0x8130FACF) */
int npRtStanzaHeaderInit(NpRtStanzaInit *header, int stanzaType,
			 const NpJid *toJid, const NpJid *fromJid);
int npRtStanzaHeaderCopy(NpRtStanzaInit *dst, const NpRtStanzaInit *src);
int npRtStanzaHeaderSetId(NpRtStanzaInit *header, const char *stanzaId);
void npRtReleaseMessageStanza(NpRtMessageStanza *stanza);
void npRtReleasePresenceStanza(NpRtPresenceStanza *stanza);
int npRtAllocPresenceStanza(NpRtPresenceStanza **outStanza, const NpRtStanzaInit *init);
void *npRtAddRefStanza(void *stanza);
void npRtReleaseStanza(NpRtStanza *stanza);
int npRtAllocStanza(NpRtStanza **outStanza, const NpRtStanzaInit *init);
void npRtDestroyBxceDocument(BxceDocument *xmlDoc);
int npRtReleaseAnyStanza(NpRtAnyStanza *anyStanza);

/* XMPP attribute string <-> enum tables (0x8130FAD0..0x8130FE53) */
int npRtParseStanzaType(int *outType, const char *str, SceSize len);
int npRtFormatStanzaType(int stanzaType, char *outBuf);
int npRtParsePresenceShow(int *outShow, const char *str, SceSize len);
int npRtFormatPresenceShow(int show, char *outBuf);
int npRtParseLanguage(int *outLang, const char *str, SceSize len);
int npRtFormatLanguage(int lang, char *outBuf);
int npRtParseErrorType(int *outErrorType, const char *str, SceSize len);
int npRtFormatErrorType(int errorType, char *outBuf);
int npRtParseErrorCondition(int *outCondition, const char *str, SceSize len);
int npRtFormatErrorCondition(int condition, char *outBuf);
int npRtParseErrorSource(int *outSource, const char *str, SceSize len);

/* bXCe DOM helpers (0x8130FE54..0x8131031F) */
int npRtCollectNamespaces(BxceDocument *xmlDoc, const char **outNamespaces, SceUInt32 *outCount);
int npRtMatchElementName(BxceDocument *xmlDoc, BxceNodeId nodeId, const char *expectedName,
			 int *outMatched);
int npRtGetElementText(BxceDocument *xmlDoc, BxceNodeId parentNodeId, const char *tagName,
		       const char **outText);
int npRtAttrListAddString(BxceAttributeList *attrList, const char *attrName, const char *value);
int npRtAttrListAddUInt64(BxceAttributeList *attrList, const char *attrName, SceUInt64 value);
int npRtCreateAttrList(BxceAttributeList **outAttrList);
void npRtDestroyAttrList(BxceAttributeList *attrList);
int npRtCreateXmlDocWithRoot(BxceDocument **outDoc, BxceNodeId *outRootNode,
			     const char *rootTagName, const char *rootXmlns);
int npRtFindElementWithXmlns(BxceDocument *xmlDoc, BxceNodeId *outNode,
			     const char *tagName, const char *expectedXmlns);
int npRtCollectExactXmlns(BxceDocument *xmlDoc, const char **outNamespaces, SceUInt32 *outCount);

/* SceNpRt memory and string helpers (implemented in Batch F: 0x813126E6..0x81312B50) */
extern void *npRtHeapAlloc(SceSize size, void *customAlloc);
extern void npRtHeapFree(void *ptr, void *customAlloc);
extern int npRtFormatUInt64(SceUInt64 value, char *outBuf, SceSize outBufSize,
			    int base, SceSize *outRequired);
extern int npRtDupString(char **outStr, const char *src, void *customAlloc, SceUInt8 flags);
extern int npRtCopyJid(NpJid *dst, const NpJid *src);
extern void npRtFreeJid(NpJid *jid);
extern void npRtFreeLangTextList(NpRtLangTextNode *head);

#endif /* SHELL_NP_RT_STANZA_H */
