/*
 * SceNpCommon's exports other NP modules use: its streaming XML parser and the communication
 * ID's text form. The XML types are laid out from the original; their names are ours.
 */
#ifndef NP_COMMON_NP_COMMON_H
#define NP_COMMON_NP_COMMON_H

#include <scetypes.h>
#include <np/np_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* an element of an XML schema level; a level ends with an empty name */
typedef struct NpXmlElement {
	const char *name;
	int hasText;			/* 1: the element's text is reported */
} NpXmlElement;

/* the elements the parser reports, level by level */
typedef struct NpXmlSchema {
	const NpXmlElement *const *levels;
	SceUInt32 levelCount;
} NpXmlSchema;

#define NP_XML_EVENT_START		1	/* name: the element */
#define NP_XML_EVENT_END		2
#define NP_XML_EVENT_TEXT		3	/* the element ends; value: its text */
#define NP_XML_EVENT_ATTRIBUTE		4	/* name, value: the attribute */

struct NpXmlParser;

typedef int (*NpXmlCallback)(struct NpXmlParser *parser, int event, SceUInt32 depth,
			     SceUInt32 index, const char *name, const char *value, void *arg);

/* a streaming XML parser, its state kept between the pieces of a document */
typedef struct NpXmlParser {
	const NpXmlSchema *schema;	/* 0x000 */
	char name[256];			/* 0x004: the element or attribute name */
	char value[256];		/* 0x104: the attribute value or element text */
	char pending[8];		/* 0x204: a character split between two pieces */
	SceSize pendingLength;		/* 0x20C */
	SceSize pendingNeeded;		/* 0x210: the bytes it still needs */
	SceUInt32 depth;		/* 0x214 */
	SceUInt32 index[6];		/* 0x218: the schema element at each level */
	SceUInt32 state;		/* 0x230: NP_XML_STATE_* */
	SceUInt32 flags;		/* 0x234: in a name, 1 once it ended; NP_XML_FLAG_* in a value */
	SceSize length;			/* 0x238: of name or value */
	int closed;			/* 0x23C: the element ends in this tag */
	NpXmlCallback callback;		/* 0x240 */
	void *arg;			/* 0x244 */
} NpXmlParser;

/* xml.c: the parser for a schema, and the next size bytes of its document */
int SceNpCommon_650D111B(NpXmlParser *parser, const NpXmlSchema *schema,
			 NpXmlCallback callback, void *arg);
int SceNpCommon_88B18A5B(NpXmlParser *parser, const char *data, int size);

/* strtol.c: strtol and strtoul */
int SceNpCommon_4C4AA451(const char *str, const char **end, int base);
unsigned int SceNpCommon_5B770B80(const char *str, const char **end, int base);
/* strtol.c: a communication ID from its text, and its text ("NPWR00000_00") */
int SceNpCommon_5BC54899(SceNpCommunicationId *commId, const char *str);
int SceNpCommon_E426FEE8(const SceNpCommunicationId *commId, char *str, SceSize size);

/* jid.c: the NP environment's name ("np" on the retail network) */
int sceNpCommonGetNpEnviroment(char *environment);

/* a Jabber ID, node@domain/resource, its parts pointing into one buffer */
typedef struct NpJid {
	const char *node;		/* 0x00: the online ID */
	const char *domain;		/* 0x04: "xx.yy.<environment>.playstation.net" */
	const char *resource;		/* 0x08: the platform */
	SceUInt32 reserved;		/* 0x0C */
} NpJid;

/* jid.c: a JID's parts and text, the NP ID it names and the JID of an NP ID; without jid or buf
   they only measure (*required gets the buffer size they need) */
int SceNpCommon_D2B9BCA8(NpJid *jid, const char *str, char *buf, SceSize bufSize,
			 SceSize *required);
int SceNpCommon_85699AE2(const NpJid *jid, char *buf, SceSize bufSize, SceSize *required);
int SceNpCommon_9B7CAA67(SceNpId *npId, const NpJid *jid);
int SceNpCommon_40CC4D15(NpJid *jid, const SceNpId *npId, char *buf, SceSize bufSize,
			 SceSize *required);

/* xml.c: XML text escaped and unescaped; without dst they only measure */
int SceNpCommon_C2BF5C1F(char *dst, SceSize dstSize, const SceUInt8 *src, SceSize srcSize,
			 SceSize *required);
int SceNpCommon_389F8C14(char *dst, SceSize dstSize, const SceUInt8 *src, SceSize srcSize,
			 SceSize *required);

/* base64.c */
SceSize sceNpCommonBase64Encode(const SceUInt8 *src, char *dst, SceSize size);

/* util.c: SceNpCommon's heap */
void *sceNpCommonMallocNpServerName(SceSize size);
void sceNpCommonFreeNpServerName(void *ptr);
/* util.c: NP full test (the registry's on a test or development unit, else the settings') */
SceBool SceNpCommon_BBC13621(void);
/* util.c: the system software version as "major.minor" */
void SceNpCommon_0624AE44(char *version);

#ifdef __cplusplus
}
#endif

#endif /* NP_COMMON_NP_COMMON_H */
