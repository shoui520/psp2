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

/* strtol.c: a communication ID from its text, and its text ("NPWR00000_00") */
int SceNpCommon_5BC54899(SceNpCommunicationId *commId, const char *str);
int SceNpCommon_E426FEE8(const SceNpCommunicationId *commId, char *str, SceSize size);

/* jid.c: the NP environment's name ("np" on the retail network) */
int sceNpCommonGetNpEnviroment(char *environment);

#ifdef __cplusplus
}
#endif

#endif /* NP_COMMON_NP_COMMON_H */
