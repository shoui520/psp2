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
/* two positive results the result-name table does not name (names ours) */
#define SCE_BXCE_INFO_GENERIC_MESSAGE			8724480		/* 0x00852000 */
#define SCE_BXCE_INFO_RETRY				8724481		/* 0x00852001 */
#define SCE_BXCE_ERROR_NO_MEMORY			-2138759168	/* 0x80852000 */
#define SCE_BXCE_ERROR_INVALID_ARGUMENT			-2138759166	/* 0x80852002 */
#define SCE_BXCE_ERROR_DUPLICATE_ATTRIBUTE_NAME		-2138759126	/* 0x8085202A */
#define SCE_BXCE_ERROR_XML_UNEXPECTED_EOF		-2138759158	/* 0x8085200A */
#define SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT	-2138759130	/* 0x80852026 */
/* results the result-name table does not name (names ours) */
#define SCE_BXCE_ERROR_PARSE_SUSPENDED			-2138758912	/* 0x80852100 */
#define SCE_BXCE_ERROR_HANDLER_ABORTED			-2138758911	/* 0x80852101 */
#define SCE_BXCE_ERROR_INVALID_CHARACTER		-2138757888	/* 0x80852500 */
#define SCE_BXCE_ERROR_INVALID_DECIMAL			-2138757886	/* 0x80852502 */
#define SCE_BXCE_ERROR_INVALID_HEXADECIMAL		-2138757885	/* 0x80852503 */
#define SCE_BXCE_ERROR_XML_SYNTAX_ERROR			-2138759157	/* 0x8085200B */
#define SCE_BXCE_ERROR_XML_END_TAG_MISMATCH		-2138759155	/* 0x8085200D */
#define SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR		-2138759151	/* 0x80852011 */
#define SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET	-2138759149	/* 0x80852013 */
#define SCE_BXCE_ERROR_XML_EXPECTED_EQUALITY_OPERATOR	-2138759148	/* 0x80852014 */
#define SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON		-2138759147	/* 0x80852015 */
#define SCE_BXCE_ERROR_XML_EXPECTED_QUOTE		-2138759143	/* 0x80852019 */
#define SCE_BXCE_ERROR_XML_END_OF_COMMENT_NOT_FOUND	-2138759146	/* 0x80852016 */
#define SCE_BXCE_ERROR_XML_END_OF_CDSECT_NOT_FOUND	-2138759145	/* 0x80852017 */
#define SCE_BXCE_ERROR_XML_INVALID_PI			-2138759131	/* 0x80852025 */
#define SCE_BXCE_ERROR_XML_END_OF_DTD_NOT_FOUND		-2138759127	/* 0x80852029 */
#define SCE_BXCE_ERROR_XML_NULL_HANDLER			-2138759132	/* 0x80852024 */
#define SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND	-2138759129	/* 0x80852027 */
#define SCE_BXCE_ERROR_DOM_NOT_FOUND_ERROR		-2138758651	/* 0x80852205 */
#define SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR		-2138758647	/* 0x80852209 */
#define SCE_BXCE_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR		-2138758645	/* 0x8085220B */
#define SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR	-2138758644	/* 0x8085220C */
#define SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR		-2138758643	/* 0x8085220D */
#define SCE_BXCE_ERROR_DOM_ENITITY_ERROR		-2138758642	/* 0x8085220E */
#define SCE_BXCE_ERROR_EXCEED_LIMIT			-2138758640	/* 0x80852210 */

/* the most attributes of a DOM element, the longest attribute name, the most unique names */
#define BXCE_DOM_ATTR_MAX		128
#define BXCE_DOM_ATTR_NAME_MAX		0x80
#define BXCE_DOM_UNIQUE_NAME_MAX	0xFFFF
#define BXCE_DOM_ATTR_BUFFER_SIZE	0x400

/* the sizes of a new table and name table, the limit of a table, the longest element name */
#define BXCE_TABLE_INITIAL_SIZE		5240
#define BXCE_NAMES_INITIAL_SIZE		2024
#define BXCE_TABLE_LIMIT		0x1000000
#define BXCE_NAME_MAX			0x400

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

/* the attributes an element waits to have written: name offset and text position pairs */
#define BXCE_PENDING_ATTR_MAX		128

/*
 * A document (1124 bytes). Its tables hold the bX form: the node tokens, the texts and the
 * element and attribute names, each string after its encoded length.
 * PLACEHOLDER: the fields named fieldNN are not read yet; they are named from their users as
 * the functions that use them are written.
 */
typedef struct BxceDocument {
	int isExternal;			/* the tables are not the document's own */
	char *nodes;
	char *texts;
	char *elementNames;
	char *attrNames;
	int status;			/* the last error */
	int field24;
	SceUInt8 isBinary;		/* the bX form, else the DOM form */
	SceUInt8 padding29[3];
	SceUInt32 nodesCapacity;
	SceUInt32 textsCapacity;
	SceUInt32 elementNamesCapacity;
	SceUInt32 attrNamesCapacity;
	SceUInt32 nodesSize;
	SceUInt32 textsSize;
	SceUInt32 elementNamesSize;
	SceUInt32 attrNamesSize;
	int field64;
	SceUInt32 pendingAttr[2 * BXCE_PENDING_ATTR_MAX];
	int pendingAttrCount;
	int attrNode;			/* where the last attributes were written */
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

/*
 * bXCe's heap: blocks with boundary tags carved from a small pool, and blocks of their own from
 * the allocator below it. A block's header is its previous block's size when that one is free
 * (0 when it is in use, 0x80000000 for a block of its own) and its own size (negative in use).
 */
typedef struct BxceHeapBlock {
	int prevSize;
	int size;
	struct BxceHeapBlock *next;	/* a free block's list links */
	struct BxceHeapBlock *prev;
} BxceHeapBlock;

#define BXCE_HEAP_OWN_BLOCK		((int)0x80000000)
#define BXCE_HEAP_POOL_SIZE		0x80
#define BXCE_HEAP_DEFAULT_SIZE		0x32000

/* called when the pool is exhausted; SCE_BXCE_INFO_RETRY asks for another try */
typedef int (*BxceHeapOutOfMemory)(SceSize size);

typedef struct BxceHeap {
	BxceHeapOutOfMemory outOfMemory;
	BxceHeapBlock *freeList;	/* its sentinel, at the start of the pool */
	void *memory;			/* what is freed with the heap: the heap itself */
	SceUInt32 headerSize;
	SceUInt32 overhead;		/* added to every request */
	int reserved[4];		/* nothing reads or writes them */
	SceUInt8 initialized;
	SceUInt8 padding[7];
	char pool[BXCE_HEAP_POOL_SIZE];
} BxceHeap;

/* the heap behind them, whether it was made, whether the application made it */
extern SceUInt8 g_bxceHeapCreated;
extern BxceHeap *g_bxceHeap;
extern int g_bxceInitializeCount;
extern SceUInt8 g_bxceHeapByApplication;

int bxceHeapInitializePool(BxceHeap *heap, void *memory, SceUInt32 size);
int bxceHeapCreate(SceSize size, BxceHeapOutOfMemory outOfMemory);
int ScebXCe_EF4AF9F9(SceSize size, BxceHeapOutOfMemory outOfMemory);	/* create the heap */
void bxceHeapDelete(void);
void ScebXCe_2ECC8276(void);					/* delete the heap */
void *bxceHeapAllocFromPool(BxceHeap *heap, SceSize size);
void *bxceHeapAlloc(BxceHeap *heap, SceSize size);
void bxceHeapFree(BxceHeap *heap, void *ptr);
void *bxceHeapExtend(BxceHeap *heap, void *ptr, SceSize extra);

/* the allocator under the heap */
void *bxceMspaceMalloc(SceSize size);
void bxceMspaceDestroy(void);
void *bxceDefaultAllocate(SceSize size);
void bxceDefaultDeallocate(void *ptr);

/* the C library calls, each through a function of its own */
void *bxceMemset(void *dst, int c, SceSize len);
void *bxceMemcpy(void *dst, const void *src, SceSize len);
void *bxceMemmove(void *dst, const void *src, SceSize len);
int bxceStrncmp(const char *s1, const char *s2, SceSize len);
int bxceStrncmpTag(const char *s1, const char *s2, SceSize len);
int bxceMemcmp(const void *s1, const void *s2, SceSize len);
SceSize bxceStrnlen(const char *s, SceSize maxlen);
SceSize bxceStrlcpy(char *dst, const char *src, SceSize size);
int bxceSprintf(char *dst, const char *format, ...);	/* at most 1 MiB */

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

/* the parser's growing buffers: the bytes, how many are used, how many are allocated */
#define BXCE_BUFFER_INITIAL_SIZE	20

typedef struct BxceBuffer {
	char *data;
	int length;
	int capacity;
} BxceBuffer;

int bxceBufferGrow(char **data, int *capacity, int extra);

/* the parser's class of each byte value (expat's byte types, with letters, '.' and 5- and 6-byte leads of their own) */
enum {
	BXCE_BT_NONXML, BXCE_BT_MALFORM, BXCE_BT_LT, BXCE_BT_AMP, BXCE_BT_RSQB,
	BXCE_BT_LEAD2, BXCE_BT_LEAD3, BXCE_BT_LEAD4, BXCE_BT_TRAIL, BXCE_BT_CR,
	BXCE_BT_LF, BXCE_BT_GT, BXCE_BT_QUOT, BXCE_BT_APOS, BXCE_BT_EQUALS,
	BXCE_BT_QUEST, BXCE_BT_EXCL, BXCE_BT_SOL, BXCE_BT_SEMI, BXCE_BT_NUM,
	BXCE_BT_LSQB, BXCE_BT_S, BXCE_BT_NMSTRT, BXCE_BT_COLON, BXCE_BT_HEX,
	BXCE_BT_DIGIT, BXCE_BT_NAME, BXCE_BT_MINUS, BXCE_BT_OTHER, BXCE_BT_NONASCII,
	BXCE_BT_PERCNT, BXCE_BT_LPAR, BXCE_BT_RPAR, BXCE_BT_AST, BXCE_BT_PLUS,
	BXCE_BT_COMMA, BXCE_BT_VERBAR, BXCE_BT_LETTER, BXCE_BT_PERIOD, BXCE_BT_LEAD5,
	BXCE_BT_LEAD6
};

/* what bxceParserLookupReference finds an entity reference to be */
enum {
	BXCE_REF_AMP, BXCE_REF_QUOT, BXCE_REF_LT, BXCE_REF_GT, BXCE_REF_APOS,
	BXCE_REF_UNKNOWN, BXCE_REF_CHAR, BXCE_REF_ERROR
};

/*
 * The tokenizer both parsers share (the SAX parser and the document builder put theirs at their
 * start): the input chunk, the state it stopped in, the token being read and the buffers that
 * keep what a chunk's end split.
 */
#define BXCE_PARSER_SAVED_MAX		16


/* the states a chunk's end can leave the tokenizer in, where the next chunk resumes (values Sony's) */
enum {
	BXCE_STATE_DTD = 3,
	BXCE_STATE_TAG_NAME = 5,
	BXCE_STATE_ATTRIBUTES = 7,
	BXCE_STATE_ATTR_NAME = 8,
	BXCE_STATE_ATTR_EQUALS = 9,
	BXCE_STATE_ATTR_QUOTE = 10,
	BXCE_STATE_ATTR_VALUE = 12,
	BXCE_STATE_ATTR_CLOSE = 13,
	BXCE_STATE_TAG_CLOSE = 15,
	BXCE_STATE_END_TAG_NAME = 18,
	BXCE_STATE_END_TAG_CLOSE = 19,
	BXCE_STATE_CHAR_DATA = 21,
	BXCE_STATE_COMMENT = 23,
	BXCE_STATE_PI = 25,
	BXCE_STATE_CDATA = 27,
	BXCE_STATE_CONTENT = 29,
	BXCE_STATE_REFERENCE = 31
};

/* the token the tokenizer found (the SDK's TokenType values) */
enum {
	BXCE_TOKEN_START_TAG = 1,
	BXCE_TOKEN_END_TAG = 2,
	BXCE_TOKEN_CHAR_DATA = 3,
	BXCE_TOKEN_DTD = 4,
	BXCE_TOKEN_DTD_END = 5,
	BXCE_TOKEN_PI = 6,
	BXCE_TOKEN_PI_END = 7,
	BXCE_TOKEN_CDATA = 8,
	BXCE_TOKEN_CDATA_END = 9,
	BXCE_TOKEN_COMMENT = 10,
	BXCE_TOKEN_COMMENT_END = 11,
	BXCE_TOKEN_UNEXPECTED = 12,
	BXCE_TOKEN_EMPTY = 14,		/* the chunk ended between tokens */
	BXCE_TOKEN_ENTITY = 15
};

/* the names of the open elements, the innermost first */
typedef struct BxceNameStackEntry {
	char *name;
	int length;
	struct BxceNameStackEntry *next;
} BxceNameStackEntry;

typedef struct BxceNameStack {
	BxceNameStackEntry *top;
} BxceNameStack;

typedef struct BxceParser {
	struct BxceParserState *saved[BXCE_PARSER_SAVED_MAX];
	SceUInt8 skipIgnorableWhiteSpace;
	SceUInt8 resolveEntity;		/* '&' ends character data */
	SceUInt8 resolveAttrEntity;	/* attribute values have their references decoded */
	SceUInt8 padding;
	int state;
	int token;
	int lastToken;
	char pending[12];		/* the start of a "<!" the chunk's end split */
	int pendingLength;
	char terminator;
	int terminatorMatched;
	const char *input;
	int position;
	int size;
	SceUInt8 isFinal;
	SceUInt8 documentElementSeen;
	char quote;
	BxceBuffer elementName;
	BxceBuffer characters;
	BxceBuffer terminatorWindow;
	SceUInt8 emptyElement;
	BxceNameStack *nameStack;
	BxceAttributeList *attrList;
	BxceAttr *attr;
	int status;
	const char *text;
	int textLength;
	const char *skippedText;
	int skippedTextLength;
	const char *tokenStart;
	int reserved;			/* zeroed by bxceParserTerminate; nothing reads it */
	int consumed;
} BxceParser;

/*
 * The SAX parser's handler: the SDK's sce::Xml::Sax::DocumentHandler, its strings passed as
 * pointer and length, with one method more before fatalError and characters with the text's
 * offset in the document after it.
 */
class BxceDocumentHandler {
public:
	virtual ~BxceDocumentHandler() = 0;
	virtual int startDocument(void *userData) = 0;
	virtual int startElement(void *userData, const char *name, int length, const BxceAttributeList *attrList) = 0;
	virtual int characters(void *userData, const char *ch, int length) = 0;
	virtual int endElement(void *userData, const char *name, int length) = 0;
	virtual int endDocument(void *userData) = 0;
	virtual int skippedText(void *userData, int tokenType, const char *text, int length) = 0;
	virtual int entityData(void *userData, int entityType, const char *name, int length) = 0;
	virtual int reserved(void *userData) = 0;	/* the parser never calls it */
	virtual void fatalError(void *userData, int errCode) = 0;
	virtual int charactersWithOffset(void *userData, const char *ch, int length, int offset) = 0;
};

/* a SAX parser's saved state: the tokenizer's fields that go on, the handler and its user data */
typedef struct BxceParserState {
	SceUInt8 skipIgnorableWhiteSpace;
	SceUInt8 resolveEntity;
	SceUInt8 resolveAttrEntity;
	int state;
	int token;
	int lastToken;
	SceUInt8 documentElementSeen;
	char quote;
	int pendingLength;
	char terminator;
	int terminatorMatched;
	const char *input;
	int position;
	int size;
	SceUInt8 isFinal;
	BxceBuffer elementName;
	BxceBuffer characters;
	BxceBuffer terminatorWindow;
	SceUInt8 emptyElement;
	BxceNameStack *nameStack;
	BxceAttributeList *attrList;
	BxceAttr *attr;
	int status;
	const char *text;
	int textLength;
	const char *skippedText;
	int skippedTextLength;
	const char *tokenStart;
	BxceDocumentHandler *handler;
	void *userData;
	int reserved[5];
	int consumed;
	SceUInt8 characterOffsets;
} BxceParserState;


/* the SAX parser: the tokenizer, the user's data and handler */
typedef struct BxceSaxParser {
	BxceParser parser;
	void *userData;
	BxceDocumentHandler *handler;
	SceUInt8 characterOffsets;	/* characters are reported with their offset in the document */
} BxceSaxParser;

/* the text of each predefined entity, by BXCE_REF_* */
extern const char *g_bxceEntityText[5];

/* an attribute's growing name and value */
int bxceAttrAppendName(BxceAttr *attr, const char *name, int length);
int bxceAttrAppendValue(BxceAttr *attr, const char *value, int length);

/* the name stack */
int bxceNameStackInitialize(BxceNameStack *stack);
int bxceNameStackTerminate(BxceNameStack *stack);
int bxceNameStackPush(BxceNameStack *stack, char *name, int length);
BxceNameStackEntry *bxceNameStackPop(BxceNameStack *stack);
int bxceNameStackIsEmpty(const BxceNameStack *stack);

/* character references */
int ScebXCe_DFB92627(SceUInt32 *value, const char *digits, SceUInt32 length);	/* hexadecimal */
int ScebXCe_CC190FA8(SceUInt32 *value, const char *digits, SceUInt32 length);	/* decimal */
int ScebXCe_BD9D727B(SceUInt32 value, char **out, int *length);		/* to UTF-8 */

/* the tokenizer */
void bxceParserResetBuffer(BxceParser *parser, BxceBuffer *buffer);
void bxceParserReleaseState(BxceParser *parser);
int bxceParserReadAttrName(BxceParser *parser, BxceAttr *attr);
int bxceParserReadAttrValue(BxceParser *parser, BxceAttr *attr);
int bxceParserLookupReference(BxceParser *parser, BxceBuffer *reference);
int bxceParserDecodeAttrValue(BxceParser *parser, BxceAttr *attr);
int bxceParserSkipChar(BxceParser *parser, char c);
int bxceParserSkipSpace(BxceParser *parser);
int bxceParserResumeAttribute(BxceParser *parser, BxceAttributeList *list);
int bxceParserReadAttributes(BxceParser *parser, BxceAttributeList *list);
int bxceParserReadName(BxceParser *parser, BxceBuffer *name);
int bxceParserReadStartTag(BxceParser *parser, BxceBuffer *name, BxceAttributeList *list);
int bxceParserScanText(BxceParser *parser, const char **text, int *length);
int bxceParserReadEndTag(BxceParser *parser);
int bxceParserReadMarkup(BxceParser *parser);
int bxceParserReadToken(BxceParser *parser);
void bxceParserNop(void);
int bxceParserTokenAllowed(BxceParser *parser);
int bxceParserScanDoctype(BxceParser *parser, const char **text, int *length, char *terminator);
int bxceParserScanUntil(BxceParser *parser, const char **text, int *length, const char *terminator,
			int *matched, int terminatorLength);
int bxceParserReadReference(BxceParser *parser, BxceBuffer *name);
int bxceParserSaveState(BxceParser *parser, int slot);
int bxceParserRestoreState(BxceParser *parser, SceUInt8 slot);
int bxceParserReset(BxceParser *parser);
int bxceParserInitialize(BxceParser *parser);
int bxceParserTerminate(BxceParser *parser);
void ScebXCe_A40944DD(BxceParser *parser, SceUInt8 skip);	/* setSkipIgnorableWhiteSpace */
void ScebXCe_FF7CB737(BxceParser *parser, SceUInt8 resolve);	/* setResolveEntity */
void bxceParserSetResolveAttrEntity(BxceParser *parser, SceUInt8 resolve);
int bxceParserStateRelease(BxceParserState *state);
int bxceIsUtf8Bom(const char *s);

/* the open elements' parent nodes, their entries kept in a pool for reuse (0x81008f4e..0x81008fea) */
typedef struct BxceNodeStackEntry {
	BxceNodeId node;
	struct BxceNodeStackEntry *next;
} BxceNodeStackEntry;

typedef struct BxceNodeStack {
	BxceNodeStackEntry *top;
	BxceAttrPool *pool;
} BxceNodeStack;

int bxceNodeStackInitialize(BxceNodeStack *stack);
int bxceNodeStackTerminate(BxceNodeStack *stack);
int bxceNodeStackPush(BxceNodeStack *stack, BxceNodeId node);
BxceNodeId bxceNodeStackPop(BxceNodeStack *stack);

/* the document builder (the SDK's DocumentBuilder): the tokenizer and the document it writes */
typedef struct BxceDocumentBuilder {
	BxceParser parser;
	SceUInt8 skipIgnorableText;	/* CDATA, PIs and the DTD are left out of the document */
	BxceDocument *document;
	BxceNodeId parent;		/* the element new nodes go under */
	SceUInt32 skippedNode;		/* the skipped text a chunk's end split, to extend */
	SceUInt32 textNode;		/* the text node character data goes on extending */
	BxceNodeStack *nodeStack;
} BxceDocumentBuilder;

int bxceDocumentBuilderParseChunk(BxceDocumentBuilder *builder);
int ScebXCe_11D1FC0D(BxceDocumentBuilder *builder);		/* initialize */
int ScebXCe_8FA315F1(BxceDocumentBuilder *builder);		/* terminate */
int bxceDocumentBuilderReset(BxceDocumentBuilder *builder);
int ScebXCe_BC0D9A6B(BxceDocumentBuilder *builder, const char *input, int size, SceUInt8 isFinal);	/* parse */

/* the XML writer: a document's nodes as XML text */
typedef struct BxceSerializer {
	int reserved;			/* nothing here reads or writes it */
	SceUInt8 tagOpen;		/* the last start tag still wants its '>' or "/>" */
} BxceSerializer;

BxceSerializer *ScebXCe_DC8483D4(BxceSerializer *serializer);	/* constructor */
BxceSerializer *ScebXCe_8EA5A077(BxceSerializer *serializer);	/* constructor */
void bxceSerializerNop(void);
void ScebXCe_BE1AD985(void);				/* destructor */
void ScebXCe_D24E80F2(void);				/* destructor */
void bxceSerializerNop2(void);
int bxceSerializerWriteNodes(BxceSerializer *serializer, const BxceDocument *doc, BxceNodeId node,
			     char **buffer, SceUInt32 *length, SceUInt32 *capacity);
int ScebXCe_D7FE96A2(BxceSerializer *serializer, const BxceDocument *doc, char **out, SceUInt32 *outSize,
		     SceUInt8 writeDeclaration);	/* serialize */

/* the SAX parser */
int bxceSaxParserParseChunk(BxceSaxParser *sax);
int ScebXCe_C85586BD(BxceSaxParser *sax);		/* initialize */
int ScebXCe_928189AB(BxceSaxParser *sax);		/* terminate */
int ScebXCe_9E06C329(BxceSaxParser *sax);		/* reset */
int ScebXCe_4E9A7061(BxceSaxParser *sax, const char *input, int size, SceUInt8 isFinal);	/* parse */
int ScebXCe_3CAF2B78(BxceSaxParser *sax);		/* save the parse */
int ScebXCe_A12B0F90(BxceSaxParser *sax, SceUInt8 slot);	/* restore a saved parse */

/* a node list (the SDK's NodeList): its entries in a doubly linked list */
typedef struct BxceNodeListEntry {
	BxceNodeId node;
	struct BxceNodeListEntry *next;
	struct BxceNodeListEntry *prev;
} BxceNodeListEntry;

typedef struct BxceNodeList {
	const struct BxceDocument *doc;
	BxceNodeListEntry *head;
	BxceNodeListEntry *tail;
	int length;
} BxceNodeList;

BxceNodeList *bxceNodeListInitialize(BxceNodeList *list, const BxceDocument *doc);
void ScebXCe_1F59F0C4(BxceNodeList *list);			/* clear */
BxceNodeId bxceNodeListInsertFirst(BxceNodeList *list, BxceNodeId node);
BxceNodeId bxceNodeListInsertLast(BxceNodeList *list, BxceNodeId node);
BxceNodeId bxceNodeListRemoveItem(BxceNodeList *list, BxceNodeId node);
BxceNodeId bxceNodeListFindItemByName(const BxceNodeList *list, const char *name);
BxceNodeId bxceNodeListFindItem(const BxceNodeList *list, BxceNodeId node);
BxceNodeId ScebXCe_6C46D786(const BxceNodeList *list, SceUInt32 index);	/* item */

/* the name hash tables */
void *bxceNameHashInitialize(BxceNameHash *hash);
void bxceNameHashClear(BxceNameHash *hash);
int bxceNameHashIntern(BxceNameHash *hash, const char *name, int length, char *table,
		       SceUInt32 *size);
char *bxceGrowTable(char *table, SceUInt32 needed, SceUInt32 *capacity);

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

BxceNodeId ScebXCe_134F491C(const BxceDocument *doc, BxceNodeId node);	/* getNextAttr */
BxceNodeId ScebXCe_B034C595(const BxceDocument *doc, BxceNodeId node);	/* getFirstAttr */
BxceNodeId ScebXCe_75E1B095(const BxceDocument *doc);			/* getDocRoot */
BxceNodeId bxceDocumentGetParent(BxceDocument *doc, BxceNodeId node);
BxceNodeId ScebXCe_3BF2F11B(BxceDocument *doc, BxceNodeId parent, BxceNodeId after,
			    BxceNodeId child);				/* insert a DOM node */
BxceNodeId ScebXCe_AEB2CE13(BxceDocument *doc, const char *text, SceUInt32 length); /* createText */

const char *ScebXCe_84383D8E(const BxceDocument *doc, BxceNodeId attr);	/* getAttrValue */
void bxceDocumentCopyNodes(BxceDocument *doc, const BxceDocument *src, BxceNodeId node,
			   BxceNodeId after, BxceNodeId parent, BxceNodeId last);
int ScebXCe_FAB50608(BxceDocument *doc, SceUInt8 isDom);		/* initialize */
void bxceDocumentNop(void);
int ScebXCe_35CDE77C(const BxceDocument *doc, BxceNodeId node);		/* getEntityType */
const char *bxceDocumentGetEntityText(const BxceDocument *doc, BxceNodeId node);
void ScebXCe_2C2628BB(BxceDocument *doc, BxceNodeId node);		/* free DOM nodes */
int ScebXCe_B0891B13(BxceDocument *doc);				/* terminate */
BxceNodeList *ScebXCe_1FA24DC0(const BxceDocument *doc, BxceNodeId node, const char *name);
BxceNodeId ScebXCe_AC0E4F67(BxceDocument *doc, BxceNodeId parent, const char *name,
			    const BxceAttributeList *attrs, const char *text,
			    SceUInt32 textLength);			/* appendElement */
BxceNodeId ScebXCe_568CE24D(BxceDocument *doc, BxceNodeId child, BxceNodeId parent); /* removeChild */
BxceNodeId ScebXCe_62C76804(BxceDocument *doc, BxceNodeId parent, BxceNodeId after,
			    const BxceDocument *src, BxceNodeId node);	/* import nodes */
BxceNodeList *ScebXCe_2B9B57AE(const BxceDocument *doc, BxceNodeId node);	/* getChildNodes */
const char *ScebXCe_753D9382(const BxceDocument *doc, BxceNodeId node, const char *name);
BxceNodeId bxceDocumentCreateElement(BxceDocument *doc, const char *name,
				     const BxceAttributeList *attrs, const char *text,
				     SceUInt32 textLength);

BxceNodeList *bxceGetElementsByTagName(const BxceDocument *doc, BxceNodeId node,
					const char *name, BxceNodeList *list);

/* the bX writer */
int bxceDocumentAllocateTables(BxceDocument *doc);
int bxceDocumentInitializeWriter(BxceDocument *doc);
void bxceFindParent(const BxceDocument *doc, BxceNodeId node, BxceNodeId target, int *found,
		    BxceNodeId *parent);
void bxceWriteLength(BxceDocument *doc, SceUInt32 value, char *table, SceUInt32 *pos);
int bxceDocumentAddText(BxceDocument *doc, const char *text, SceUInt32 length);
int bxceDocumentWriteAttributes(BxceDocument *doc, const SceUInt32 *attrs, SceUInt32 count,
				char *nodes, SceUInt32 *pos, int reserved);
BxceNodeId bxceDocumentAppendElement(BxceDocument *doc, BxceNodeId parent, const char *name,
				     int length, const BxceAttributeList *attrs);
SceUInt32 bxceDocumentAppendText(BxceDocument *doc, BxceNodeId parent, const char *text,
				 SceUInt32 length);
int bxceDocumentWriteSkipped(BxceDocument *doc, SceUInt32 value, int token);
SceUInt32 bxceDocumentAppendSkipped(BxceDocument *doc, BxceNodeId parent, const char *text,
				    SceUInt32 length, int kind, int entity);
SceUInt32 bxceDocumentAddSkipped(BxceDocument *doc, const char *text, SceUInt32 length,
				 int token);
int bxceDocumentExtendText(BxceDocument *doc, SceUInt32 text, const char *more, SceUInt32 length);
int bxceDocumentFlushAttributes(BxceDocument *doc);

#ifdef __cplusplus
}
#endif

#endif /* _BXCE_INTERNAL_H */
