/*
 * SceLibMarlin's private interface: Intertrust's Marlin (Octopus/Nemo) DRM client, the
 * memory it allocates from two heaps of its own (the second for secure data), its byte
 * buffers and its locks.
 */
#ifndef MLN_INTERNAL_H
#define MLN_INTERNAL_H

#include <stddef.h>

/* return codes of the library's helpers */
#define MLN_OK			0
#define MLN_ERROR_NO_MEMORY	2
#define MLN_ERROR_INVALID	5

/* which heap: the general one or the one for secure data */
#define MLN_HEAP_GENERAL	0
#define MLN_HEAP_SECURE		1

/* a byte string: its size and its bytes (inline after the header or allocated) */
typedef struct MlnBuffer {
	unsigned int size;
	unsigned char *data;
} MlnBuffer;

/* a counted array of pointers or words */
typedef struct MlnArray {
	unsigned int count;
	void *items[];
} MlnArray;

/* a block of the heap: the blocks in address order, the free ones also in a list */
typedef struct MlnHeapBlock {
	struct MlnHeapBlock *prev;
	struct MlnHeapBlock *next;
	struct MlnHeapBlock *freePrev;
	struct MlnHeapBlock *freeNext;
} MlnHeapBlock;

#define MLN_HEAP_UNIT		16
#define MLN_HEAP_SMALL_MAX	64
#define MLN_HEAP_PAGE_UNITS	256
#define MLN_HEAP_RUN_CONTINUED	0xff

/* a page of small allocations: a map of runs (length at a run's first unit) and the units */
typedef struct MlnHeapPage {
	struct MlnHeapPage *prev;
	struct MlnHeapPage *next;
	int freeUnits;
	int reserved;
	unsigned char map[MLN_HEAP_PAGE_UNITS];
	unsigned char units[MLN_HEAP_PAGE_UNITS][MLN_HEAP_UNIT];
} MlnHeapPage;

typedef struct MlnHeap {
	MlnHeapBlock *start;
	MlnHeapBlock *end;
	MlnHeapBlock *rover;
	MlnHeapPage *pages;
	unsigned char freeMap[4];
} MlnHeap;

typedef int MlnMutex;

/* one security attribute of a track ('styp' in its 'satr') */
typedef struct MlnTrackAttribute {
	unsigned int type;
	const unsigned char *data;
	int size;
} MlnTrackAttribute;

/* a track's protection boxes, found in its 'sinf' */
typedef struct MlnTrackProtection {
	const unsigned char *contentId;
	int contentIdSize;
	const unsigned char *bundle;
	int bundleSize;
	const unsigned char *hmac;
	const unsigned char *attributes;
	int attributesSize;
	int attributeCount;
	MlnTrackAttribute attribute[16];
} MlnTrackProtection;

/* a session with the client's service */
typedef struct MlnServiceContext {
	int id;
	void *session;
} MlnServiceContext;

/* an Octopus bundle's protectors: each names its content key and the content it protects */
typedef struct MlnContentTarget {
	unsigned int size;
	unsigned char *data;
} MlnContentTarget;

typedef struct MlnProtector {
	MlnBuffer *contentKeyId;
	unsigned int targetCount;
	MlnContentTarget *targets;
} MlnProtector;

typedef struct MlnBundleInfo {
	MlnBuffer *controllerUid;
	unsigned int protectorCount;
	MlnProtector *protectors;
} MlnBundleInfo;

/* the XML parser's nodes, and the lists a path search returns */
typedef struct MlnXmlNode MlnXmlNode;

typedef struct MlnXmlNodeList {
	unsigned int reserved;
	unsigned int count;
	MlnXmlNode *nodes[];
} MlnXmlNodeList;

/* a value a call returns, and the list of them a caller keeps */
typedef struct MlnApiValue {
	int type;
	void *data[4];
} MlnApiValue;

typedef struct MlnApiNode {
	struct MlnApiNode *next;
	int type;
	void *data[4];
} MlnApiNode;

typedef struct MlnApiResult {
	MlnApiValue *value;
	void *next;
} MlnApiResult;

/* which engine an object runs on: the local one, or a remote client */
typedef struct MlnApiKind {
	int type;
	int remote;
} MlnApiKind;

typedef struct MlnApiObject {
	MlnApiKind *kind;
	void *reserved;
	void *data;
	void *engine;
	void *client;
	void *session;
} MlnApiObject;

typedef struct MlnApiInfo {
	unsigned int value[7];
} MlnApiInfo;

/* Octopus nodes and the links between them, by node ID */
typedef struct MlnOctopusLink {
	MlnBuffer *from;
	struct MlnOctopusLink *next;
	MlnBuffer *to;
} MlnOctopusLink;

typedef struct MlnOctopusNode {
	struct MlnOctopusNode *next;
	MlnOctopusLink *links;
	MlnBuffer *id;
} MlnOctopusNode;

/* a set of IDs */
typedef struct MlnIdSet {
	struct MlnIdSet *next;
	MlnBuffer *id;
} MlnIdSet;

/* mutex.c */
int MlnMutexCreate(MlnMutex *mutex);
int MlnMutexLock(MlnMutex *mutex);
int MlnMutexUnlock(MlnMutex *mutex);
int MlnMutexDelete(MlnMutex *mutex);
int MlnMutexInit(void);
int MlnMutexTerm(void);

/* memory.c */
int MlnHeapInit(int heap, void *memory, unsigned int size);
void *MlnAllocate(int heap, unsigned int size);
void *MlnReallocate(int heap, void *p, unsigned int size);
void MlnFree(int heap, void *p);
void MlnHeapTerm(int heap);
char *MlnStrdup(int heap, const char *s);
void *MlnMemmove(void *dst, const void *src, unsigned int size);
void *MlnMemcpy(void *dst, const void *src, unsigned int size);
int MlnMemcmp(const void *a, const void *b, unsigned int size);
void *MlnMemset(void *dst, int c, unsigned int size);
MlnHeapBlock *MlnHeapMerge(MlnHeap *heap, MlnHeapBlock *a, MlnHeapBlock *b);
void *MlnHeapAllocate(MlnHeap *heap, unsigned int size);
void MlnHeapFree(MlnHeap *heap, void *p);
void MlnHeapAddFree(MlnHeap *heap, MlnHeapBlock *block);

/* buffer.c */
MlnBuffer *MlnBufferInitInline(int heap, MlnBuffer *buffer, unsigned int size);
MlnBuffer *MlnBufferCreate(int heap, unsigned int size, const void *data);
MlnBuffer *MlnBufferCreateFilled(int heap, unsigned int size, int value);
void MlnBufferDestroy(int heap, MlnBuffer *buffer);
MlnBuffer *MlnBufferConcat(int heap, const MlnBuffer *a, const MlnBuffer *b);
int MlnBytesCompare(int heap, unsigned int sizeA, const void *a, unsigned int sizeB,
		    const void *b);
int MlnCompareNull(const void *a, const void *b, int *result);
int MlnBufferCompare(int heap, const MlnBuffer *a, const MlnBuffer *b);
int MlnBufferCompareKey(int heap, const MlnBuffer *a, const MlnBuffer *b);
MlnBuffer *MlnBufferClear(int heap, MlnBuffer *buffer);
MlnBuffer *MlnBufferSet(int heap, MlnBuffer *buffer, unsigned int size, void *data);
MlnBuffer *MlnBufferInitCopy(int heap, MlnBuffer *buffer, unsigned int size, const void *data);
void MlnBufferFreeData(int heap, MlnBuffer *buffer);
int MlnBufferToString(int heap, const MlnBuffer *buffer, char **string);
MlnArray *MlnWordArrayCreate(int heap, int count, const unsigned int *words);
void MlnArrayDestroy(int heap, MlnArray *array);

/* string.c */
unsigned int MlnStrlen(const char *s);

/* api.c */
int SceMarlin_A11B25C7(const unsigned char *data, int size, MlnTrackProtection *info);
int MlnConvertResult(unsigned int result);
int SceMarlin_D09A4419(const void *xml, unsigned int size, MlnBundleInfo **out);
int SceMarlin_73C12840(MlnBundleInfo *info);
int SceMarlin_9609E2DF(MlnBuffer *buffer);
int MlnIdSetAdd(MlnIdSet **set, const MlnBuffer *id);
int MlnNodesCollectReachable(MlnOctopusNode *nodes, const MlnBuffer *id, MlnIdSet **set);

/* not written in C yet: the prototypes are their callers' */
int MlnMemoryInit(int heap, void *memory, unsigned int size);
int MlnMemoryTerm(int heap);
int MlnCoreInit(void *memory, unsigned int size);
int MlnCoreTerm(void);
int MlnPlatformInit(void);
int MlnPlatformTerm(void);
unsigned int MlnStateSerialize(MlnBuffer **state);
int MlnServiceSetState(MlnServiceContext *context, MlnBuffer *state);
int MlnServiceClearState(MlnServiceContext *context);
unsigned int MlnNtohl(unsigned int value);
int MlnImpl_81013350(void *a0, void *a1);
int MlnImpl_81016A5C(void *a0, void *a1);
int MlnImpl_81016B4C(void *a0, void *a1);
int MlnImpl_81018B8C(MlnServiceContext *context, MlnBuffer *serviceProvider, MlnBuffer *token,
		     MlnBuffer *ids, unsigned int *count, MlnBuffer ***items);
int MlnImpl_81020094(void *session, void *a1, MlnApiValue *value, void *next, void *a4, void *a5);
int MlnImpl_810072B0(void *a0, MlnApiInfo *info);
int MlnImpl_8101A020(void *engine);
int MlnImpl_8101A030(void *engine);
int MlnImpl_8101E628(void *client);
int MlnImpl_8101E658(void *client);
int MlnImpl_81019B44(int a0, void *a1, void *a2, void *a3, int a4, void **engine);
int MlnImpl_8101E380(void *client, void *a1, void *a2, void *a3, int a4);
int MlnXmlParse(int heap, const void *xml, unsigned int size, MlnXmlNode **document);
int MlnXmlFindNode(int heap, MlnXmlNode *node, const char *path, const char *ns, MlnXmlNode **found);
int MlnXmlFindNodes(int heap, MlnXmlNode *node, const char *path, const char *ns,
		    MlnXmlNodeList **found);
int MlnXmlGetAttribute(int heap, MlnXmlNode *node, const char *ns, const char *name,
		       MlnBuffer **value);
int MlnXmlGetText(int heap, MlnXmlNode *node, MlnBuffer **text);
int MlnXmlNodeFree(int heap, MlnXmlNode *node);
int MlnXmlListFree(int heap, MlnXmlNodeList *list);
void MlnImpl_81006D14(int heap, void *a1);
unsigned int MlnImpl_81010068(int heap, void *a1);
void MlnImpl_810136AC(void);
int MlnImpl_810136E0(void *a0, void *a1, void *a2, void *a3, unsigned char a4, void *a5);
int MlnImpl_81013804(void *a0, void *a1, MlnBuffer **a2);
int MlnImpl_810147D8(void *a0, void *a1);
int MlnImpl_81014860(void *a0, void *a1, void *a2, void *a3);
int MlnImpl_810148F8(void *a0, void *a1, void *a2);
int MlnImpl_81014A10(void *a0, void *a1, void *a2, void *a3, void *a4);
int MlnImpl_81014B8C(void *a0, void *a1);
int MlnImpl_81014C20(void *a0, void *a1, void *a2);
int MlnImpl_81014D30(void *a0, void *a1);
int MlnImpl_81014DC4(void *a0, void *a1);
int MlnImpl_81014E28(void *a0, void *a1, void *a2);
int MlnImpl_81014F24(void *a0, void *a1);
int MlnImpl_81014FA8(void *a0, void *a1);
int MlnImpl_8101503C(void *a0, void *a1, void *a2);
int MlnImpl_81015168(void *a0);
int MlnImpl_8101521C(void *a0, void *a1, void *a2, void *a3, void *a4);
int MlnImpl_81015374(void *a0, void *a1, void *a2);
int MlnImpl_8101549C(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6);
int MlnImpl_81015620(void *a0, void *a1, void *a2, void *a3);
int MlnImpl_81015798(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5);
int MlnImpl_810158FC(void *a0, void *a1, void *a2);
int MlnImpl_81015A24(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7);
int MlnImpl_81015BB0(void *a0, void *a1, void *a2);
int MlnImpl_81015CD8(void *a0, void *a1);
int MlnImpl_81015DDC(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6);
int MlnImpl_81015EA4(void *a0, void *a1, void *a2);
int MlnImpl_81015F3C(void *a0);
int MlnImpl_81016034(void *a0);
int MlnImpl_810160E8(void *a0, void *a1, void *a2);
int MlnImpl_8101617C(void *a0, void *a1, void *a2);
int MlnImpl_81016270(void *a0, void *a1);
int MlnImpl_810162F4(void *a0, void *a1, void *a2);
int MlnImpl_8101632C(void *a0, void *a1, void *a2);
int MlnImpl_81016364(void *a0, void *a1, void *a2);
int MlnImpl_810163A8(void *a0, void *a1);
int MlnImpl_810163E4(void *a0, void *a1, void *a2, void *a3);
int MlnImpl_810166AC(void *a0, void *a1, void *a2);
int MlnImpl_81016844(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5);
int MlnImpl_8101698C(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6);

#endif /* MLN_INTERNAL_H */
