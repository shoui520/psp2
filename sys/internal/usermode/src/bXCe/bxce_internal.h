/*
 * ScebXCe's internal interface: Sony's XML library with its binary document form ("bX"), a fork
 * of the SDK's sce::Xml. No debug record, symbol or SDK stub names any of it: the vocabulary is
 * the SDK's xml headers' (AttributeList, Attr, getLength, addAttribute, ...) and the module's own
 * result names; every name here is ours (agent namecheck list).
 */
#ifndef _BXCE_INTERNAL_H
#define _BXCE_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The results. The module's result-name table (ScebXCe_998552A5) names them 0x1000 below the
 * values its code returns (NoMemory 0x80851000, returned as 0x80852000): the names are the
 * table's.
 */
#define SCE_BXCE_ERROR_NO_MEMORY			-2138759168	/* 0x80852000 */
#define SCE_BXCE_ERROR_INVALID_ARGUMENT			-2138759166	/* 0x80852002 */
#define SCE_BXCE_ERROR_DUPLICATE_ATTRIBUTE_NAME		-2138759126	/* 0x8085202A */

/* the longest name or value addAttribute measures */
#define BXCE_ATTR_STRING_MAX		0x100000

/* an attribute: its name and value in buffers that grow */
typedef struct BxceAttr {
	int nameCapacity;
	int valueCapacity;
	int nameLength;
	int valueLength;
	char *name;
	char *value;
	struct BxceAttr *next;
	int reserved;			/* zeroed by bxceAttrInitialize; nothing reads it */
} BxceAttr;

/* the attributes a list gave back, kept for reuse */
#define BXCE_ATTR_POOL_SIZE		16

typedef struct BxceAttrPool {
	BxceAttr *attr[BXCE_ATTR_POOL_SIZE];
	int count;
} BxceAttrPool;

/* an attribute list: a singly linked list of attributes over a pool */
typedef struct BxceAttributeList {
	BxceAttrPool *pool;
	BxceAttr *head;
	BxceAttr *tail;
	int length;
	int reserved;			/* zeroed by terminate; nothing reads it */
} BxceAttributeList;

/*
 * A node: in the bX form an offset into the document's node table (0 for none), in the DOM form a
 * pointer to a DOM node (an element or text) or to an encoded attribute.
 */
typedef SceUInt32 BxceNodeId;

/* a DOM form node */
typedef struct BxceDomNode {
	SceUInt32 info;			/* token; an element's name offset from bit 8 */
	SceUInt32 text;			/* text offset, small or encoded in its bytes */
	struct BxceDomNode *child;
	struct BxceDomNode *sibling;
} BxceDomNode;

/* the interned-name hash tables */
#define BXCE_NAME_HASH_SIZE		211

typedef struct BxceNameHashEntry {
	int offset;			/* of the name in its table, after its length byte */
	struct BxceNameHashEntry *next;
} BxceNameHashEntry;

typedef struct BxceNameHash {
	BxceNameHashEntry *bucket[BXCE_NAME_HASH_SIZE];
} BxceNameHash;

/*
 * A document (1124 bytes). Its tables hold the bX form: the node tokens, the texts and the
 * element and attribute names, each string after its encoded length.
 * PLACEHOLDER: the fields named fieldNN are not read yet; they are named from their users as
 * the functions that use them are written.
 */
typedef struct BxceDocument {
	int isExternal;			/* the tables are not the document's own */
	const char *nodes;
	const char *texts;
	const char *elementNames;
	const char *attrNames;
	int field20;
	int field24;
	SceUInt8 isBinary;		/* the bX form, else the DOM form */
	SceUInt8 padding29[3];
	int field32;
	int field36;
	int field40;
	int field44;
	int field48;
	int field52;
	int elementNamesSize;
	int attrNamesSize;
	SceUInt8 field64[1028];
	int field1092;
	int field1096;
	BxceNameHash *attrNameHash;
	BxceNameHash *elementNameHash;
	int field1108;
	int field1112;
	BxceDomNode *domRoot;		/* the DOM form's document node */
	int field1120;
} BxceDocument;

/* the allocator: bXCe's malloc and free over its mspace */
void *ScebXCe_B8DDE741(SceSize size);
void ScebXCe_EC560647(void *ptr);

/* deletes the default allocator, if it made one */
void bxceDeleteDefaultAllocator(void);

/* the C library calls, each through a function of its own */
void *bxceMemcpy(void *dst, const void *src, SceSize len);
int bxceMemcmp(const void *s1, const void *s2, SceSize len);
SceSize bxceStrnlen(const char *s, SceSize maxlen);

/* the attribute pool */
int bxceAttrPoolInitialize(BxceAttrPool *pool);
BxceAttr *bxceAttrPoolPop(BxceAttrPool *pool);
int bxceAttrPoolPush(BxceAttrPool *pool, BxceAttr *attr);

/* an attribute */
BxceAttr *bxceAttrReset(BxceAttr *attr);
int bxceAttrSetName(BxceAttr *attr, const char *name, int length);
int bxceAttrSetValue(BxceAttr *attr, const char *value, int length);
int bxceAttrInitialize(BxceAttr *attr);
int bxceAttrTerminate(BxceAttr *attr);

/* an attribute list */
BxceAttr *bxceAttributeListNewAttr(BxceAttributeList *list);
int bxceAttributeListAppend(BxceAttributeList *list, BxceAttr *attr);
int ScebXCe_E7C8B20D(BxceAttributeList *list);		/* initialize */
int ScebXCe_B367042F(BxceAttributeList *list);		/* clear */
int ScebXCe_9C2F9897(BxceAttributeList *list);		/* terminate */
BxceAttr *ScebXCe_2AD8DA26(const BxceAttributeList *list, const char *name, SceSize length);
BxceAttr *ScebXCe_01DC4E22(const BxceAttributeList *list, SceSize index);
int ScebXCe_69F4B8B6(const BxceAttributeList *list);	/* getLength */
int ScebXCe_3DD761CF(BxceAttributeList *list, const char *name, const char *value);

/* the document */
int bxceReadLength(const SceUInt8 *p);
const int *ScebXCe_578CC576(void);
BxceDocument *ScebXCe_63593407(BxceDocument *doc);
void ScebXCe_6E809E76(void);
BxceNodeId ScebXCe_6FC9DC3F(const BxceDocument *doc, BxceNodeId node);	/* getSibling */
BxceNodeId ScebXCe_CED6BECF(const BxceDocument *doc);			/* getRoot */
BxceNodeId ScebXCe_FEA6E4A1(const BxceDocument *doc, BxceNodeId node);	/* getFirstChild */
const char *ScebXCe_B4989A58(const BxceDocument *doc, BxceNodeId node);	/* getAttrName */
const char *ScebXCe_2FAD3DFA(const BxceDocument *doc, BxceNodeId node);	/* getSkippedText */
const char *ScebXCe_33825EFF(const BxceDocument *doc, BxceNodeId node);	/* getText */
const char *ScebXCe_1189A2B1(const BxceDocument *doc, BxceNodeId node);	/* getNodeName */
int ScebXCe_FA18A039(const BxceDocument *doc, BxceNodeId node);		/* getNodeType */

#ifdef __cplusplus
}
#endif

#endif /* _BXCE_INTERNAL_H */
