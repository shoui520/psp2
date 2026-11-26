/*
 * np_rt_ext.h: SceNpRt XMPP extension payload structures and serializers /
 * parsers for message and presence stanzas (0x81405EE8..0x8140910B).
 */
#ifndef SHELL_NP_RT_EXT_H
#define SHELL_NP_RT_EXT_H

#include <stddef.h>
#include <scetypes.h>
#include <np/np_common.h>
#include <rtc.h>
#include "np_rt_stanza.h"
#include "../imps/np_imps_core.h"

/* Inline binary/reference encoding formats ("inline" XML attribute) */
enum NpRtInlineFormat {
	NP_RT_INLINE_NONE	= 0,
	NP_RT_INLINE_BASE64	= 1,
	NP_RT_INLINE_URI	= 2,
	NP_RT_INLINE_FILEID	= 3
};

/* Presence session extension ("http://playstation.net/np/rt/session", 16 bytes) */
struct NpRtSessionExt {
	int sessionType;		/* +0x00: 1=standby(type=3), 2=polling(type=4) */
	SceUInt32 padAfterType;		/* +0x04 */
	SceRtcTick dateTick;		/* +0x08: "date" attribute */
};

/* Entity capabilities node ("http://jabber.org/protocol/caps", <c>, 16 bytes) */
struct NpRtCapsNode {
	NpRtCapsNode *next;		/* +0x00 */
	char *node;			/* +0x04 */
	char *ver;			/* +0x08 */
	char *ext;			/* +0x0C */
};

/* Stored/delayed stanza metadata ("http://playstation.net/np/rt/stored", 32 bytes) */
struct NpRtStoredExt {
	SceUInt64 storedId;		/* +0x00: "id" attribute */
	SceRtcTick *stampTick;		/* +0x08: "stamp" attribute */
	SceRtcTick *expireTick;		/* +0x0C: "expire" attribute */
	SceUInt8 delayed;		/* +0x10: "delay" == "1" */
	SceUInt8 padAfterDelayed[7];	/* +0x11 */
	SceUInt32 ttl;			/* +0x18: "ttl" attribute on send */
	SceUInt32 padTail;		/* +0x1C */
};

/* Content attachment data item (<data> inside "http://playstation.net/np/rt/content", 28 bytes) */
struct NpRtContentDataNode {
	NpRtContentDataNode *next;	/* +0x00 */
	int inlineFormat;		/* +0x04: NpRtInlineFormat */
	char *mimeType;			/* +0x08: "type" attribute */
	void *dataPtr;			/* +0x0C */
	SceSize dataSize;		/* +0x10 */
	SceUInt32 declaredSize;		/* +0x14: "size" attribute */
	char *dataName;			/* +0x18: "name" attribute */
};

/* Content extension list node ("http://playstation.net/np/rt/content", 32 bytes) */
struct NpRtContentExt {
	NpRtContentExt *next;		/* +0x00 */
	SceNpCommunicationId contextId;	/* +0x04: "context" attribute */
	SceUInt32 headerVersion;	/* +0x10: <header version="..."> */
	void *headerData;		/* +0x14: base64-decoded <header> */
	SceSize headerDataSize;		/* +0x18 */
	NpRtContentDataNode *dataList;	/* +0x1C: <data> list */
};

/* Message metadata extension ("http://playstation.net/np/rt/message", 8 bytes) */
struct NpRtMessageMetaExt {
	char messageKind[4];		/* +0x00: "message_kind" (3 chars + NUL) */
	int convertType;		/* +0x04: "convert_type" (-1 if absent) */
};

/* Aggregated message stanza extensions (24 bytes, 0x81405EE8..0x8140621B) */
struct NpRtMessageExtBundle {
	NpRtContentExt *contentExt;		/* +0x00: .../rt/content */
	NpRtMessageMetaExt *messageExt;		/* +0x04: .../rt/message */
	NpImpsCoreDataNode *extData;		/* +0x08: .../rt/ext */
	NpRtStoredExt *storedExt;		/* +0x0C: .../rt/stored */
	NpImpsCoreNotificationExt *notifExt;	/* +0x10: .../rt/notification */
	SceUInt8 antiblock;			/* +0x14: .../rt/antiblock */
};

/* Aggregated presence stanza extensions (24 bytes, 0x81407044..0x814073BF) */
struct NpRtPresenceExtBundle {
	NpRtSessionExt *sessionExt;		/* +0x00: .../rt/session */
	NpImpsCoreTitleNode *titleList;		/* +0x04: .../rt/presence */
	SceRtcTick *delayTick;			/* +0x08: jabber:x:delay */
	NpRtCapsNode *capsList;			/* +0x0C: .../protocol/caps */
	NpImpsCoreDataNode *extData;		/* +0x10: .../rt/ext */
	NpRtStoredExt *storedExt;		/* +0x14: .../rt/stored */
};

/* Message extension bundle operations (0x81405EE8..0x814061A1) */
void npRtExtFreeMessageBundle(NpRtMessageExtBundle *bundle, void *customAlloc, SceUInt8 flags);
int npRtExtParseMessageBundle(NpRtMessageExtBundle *outBundle, const NpRtMessageStanza *stanza,
			      void *customAlloc, SceUInt8 flags);
int npRtExtCloneMessageBundle(NpRtMessageExtBundle *outBundle, const NpRtMessageExtBundle *src,
			      void *customAlloc, SceUInt8 flags);
int npRtExtSerializeMessageBundle(BxceDocument **outDoc, const NpRtMessageExtBundle *bundle);

/* Presence extension bundle operations (0x81407044..0x81407357) */
void npRtExtFreePresenceBundle(NpRtPresenceExtBundle *bundle, void *customAlloc, SceUInt8 flags);
int npRtExtParsePresenceBundle(NpRtPresenceExtBundle *outBundle, const NpRtPresenceStanza *stanza,
			       void *customAlloc, SceUInt8 flags);
int npRtExtClonePresenceBundle(NpRtPresenceExtBundle *outBundle, const NpRtPresenceExtBundle *src,
			       void *customAlloc, SceUInt8 flags);
int npRtExtSerializePresenceBundle(BxceDocument **outDoc, const NpRtPresenceExtBundle *bundle);

#endif /* SHELL_NP_RT_EXT_H */
