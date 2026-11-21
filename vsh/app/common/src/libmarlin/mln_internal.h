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
#define MLN_ERROR_INVALID_ARGUMENT	3
#define MLN_ERROR_INVALID	5
#define MLN_ERROR_FORMAT	0x30000001	/* a value of the wrong type */
#define MLN_ERROR_UNSUPPORTED	0x30000002	/* a value type the library does not know */

/* the XML parser's errors */
#define MLN_XML_ERROR_SYNTAX		0x10010003
#define MLN_XML_ERROR_END_TAG		0x10010005	/* an end tag naming another element */
#define MLN_XML_ERROR_NODE_TYPE		0x10010006	/* an absolute path or axis on the wrong node */
#define MLN_XML_ERROR_PREDICATE		0x10010007	/* a '[' without its ']' */
#define MLN_XML_ERROR_NOT_FOUND		0x10010008	/* no such node or name */
#define MLN_XML_ERROR_NOT_ELEMENT	0x10010009	/* a node that is not an element */
#define MLN_XML_ERROR_NOT_UNIQUE	0x1001000A	/* a path selecting more than one node */

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

/*
 * The XML parser's tree. Its objects are reference counted: the count is the number of
 * references beyond the first, and a release frees the object when it is 0 (a negative
 * count marks one that is never freed).
 */

/* an attribute or a namespace declaration (its prefix and URI) */
typedef struct MlnXmlAttribute {
	int refCount;
	MlnBuffer *name;
	MlnBuffer *value;
} MlnXmlAttribute;

typedef struct MlnXmlAttributeArray {
	int count;
	MlnXmlAttribute *items[];
} MlnXmlAttributeArray;

struct MlnXmlElement;

/* an element's content: text or a child element */
#define MLN_XML_CONTENT_TEXT	0
#define MLN_XML_CONTENT_ELEMENT	1

typedef struct MlnXmlContent {
	int refCount;
	int kind;
	union {
		MlnBuffer *text;
		struct MlnXmlElement *element;
	} data;
} MlnXmlContent;

typedef struct MlnXmlContentArray {
	int refCount;
	int count;
	MlnXmlContent *items[];
} MlnXmlContentArray;

typedef struct MlnXmlElement {
	int refCount;
	MlnBuffer *name;
	MlnXmlAttributeArray *namespaces;	/* the declarations in scope */
	MlnXmlAttributeArray *attributes;
	MlnXmlContentArray *content;
} MlnXmlElement;

/* what precedes a document's root element: its XML declaration's version and encoding */
typedef struct MlnXmlProlog {
	int refCount;
	MlnBuffer *version;
	MlnBuffer *encoding;
} MlnXmlProlog;

typedef struct MlnXmlDocument {
	int refCount;
	MlnXmlProlog *prolog;
	MlnXmlElement *root;
} MlnXmlDocument;

/* a reference to a part of the tree, which a caller holds and frees */
#define MLN_XML_NODE_DOCUMENT	0
#define MLN_XML_NODE_NAMESPACE	1
#define MLN_XML_NODE_ATTRIBUTE	2
#define MLN_XML_NODE_ELEMENT	3
#define MLN_XML_NODE_TEXT	4

typedef struct MlnXmlNode {
	int refCount;
	int type;
	union {
		MlnXmlDocument *document;
		MlnXmlAttribute *attribute;
		MlnXmlElement *element;
		MlnBuffer *text;
	} object;
} MlnXmlNode;

typedef struct MlnXmlNodeList {
	int refCount;
	int count;
	MlnXmlNode *nodes[];
} MlnXmlNodeList;

/* a singly linked list of the parser's objects */
typedef struct MlnXmlLink {
	void *item;
	struct MlnXmlLink *next;
} MlnXmlLink;

/* how a list's items are released */
#define MLN_XML_LINK_ATTRIBUTE	0
#define MLN_XML_LINK_CONTENT	1
#define MLN_XML_LINK_NODE	2

/* the namespace declarations in scope, innermost first */
typedef struct MlnXmlStack {
	MlnXmlLink *top;
	int depth;
} MlnXmlStack;

typedef struct MlnXmlParser {
	MlnXmlStack namespaces;
	unsigned int reserved[4];
} MlnXmlParser;

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

/* a request the engine asks the application to send */
typedef struct MlnHttpHeader {
	char *name;
	char *value;
} MlnHttpHeader;

#define MLN_HTTP_GET	1
#define MLN_HTTP_POST	2

typedef struct MlnHttpRequest {
	int method;
	char *url;
	unsigned int headerCount;
	MlnHttpHeader **headers;
	MlnBuffer *body;
} MlnHttpRequest;

/* what one step of a transaction gives: a value for the caller, or a request to send */
typedef struct MlnApiResult {
	MlnApiValue *value;
	MlnHttpRequest *request;
} MlnApiResult;

/* the application's HTTP transport */
typedef struct MlnTransport {
	int (*open)(const char *url, void **connection);
	int (*close)(void *connection);
	int (*setHeader)(void *connection, const char *name, const char *value);
	int (*send)(void *connection, int post, unsigned int size, const void *body,
		    unsigned int *responseSize, void **response);
	void (*release)(void *response);
} MlnTransport;

/* which engine an object runs on: the local one, or a remote client */
typedef struct MlnApiKind {
	int type;
	int remote;
	MlnBuffer *address;
} MlnApiKind;

/* what a remote client reports about itself */
typedef struct MlnApiVersion {
	unsigned short major;
	unsigned short minor;
	unsigned short build;
	MlnBuffer *name;
	MlnBuffer *id;
} MlnApiVersion;

typedef struct MlnApiStatus {
	int type;
	unsigned short major;
	unsigned short minor;
	MlnBuffer *name;
	MlnBuffer *id;
	unsigned short build;
} MlnApiStatus;

typedef struct MlnApiObject {
	MlnApiKind *kind;
	int status;		/* the last step's failure, passed to the next */
	void *data;
	void *engine;
	void *client;
	void *session;
} MlnApiObject;

struct MlnIdSet;

/* a time broken down, as sceRtcSetTick gives it */
typedef struct MlnDateTime {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int microsecond;
	int utcOffset;		/* seconds */
	int daylightSaving;
} MlnDateTime;

/* the part of a time the interface gives */
typedef struct MlnApiDate {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int microsecond;
} MlnApiDate;

/*
 * What a license's control grants, read from the extended status block of its evaluation:
 * obligations, callbacks and permissions, and the descriptions and constraints. A count
 * of items whose names are not known, set where the parameter was marked critical.
 */
#define MLN_OBLIGATION_METER_PLAY_DURATION	1
#define MLN_OBLIGATION_RUN_AGENT_ON_PEER	2
#define MLN_OBLIGATION_OUTPUT_CONTROL		3
#define MLN_OBLIGATION_CUSTOM			4

typedef struct MlnObligation {
	int type;
	unsigned int flags;		/* bit 0: critical */
	MlnBuffer *controlId;
	union {
		MlnBuffer *agentId;
		struct MlnOutputControl *control;
	} data;
	int instanceId;
	MlnBuffer *contextId;
	struct MlnValueList *parameters;
} MlnObligation;

#define MLN_CALLBACK_ON_ACCEPT		1
#define MLN_CALLBACK_ON_TIME		2
#define MLN_CALLBACK_ON_TIME_ELAPSED	3
#define MLN_CALLBACK_ON_EVENT		4
#define MLN_CALLBACK_ON_AGENT_COMPLETION 5

#define MLN_CALLBACK_EVENT_PLAY		1
#define MLN_CALLBACK_EVENT_STOP		2
#define MLN_CALLBACK_EVENT_TIMECODE	3
#define MLN_CALLBACK_EVENT_SEEK		4

typedef struct MlnCallback {
	int type;
	unsigned int flags;		/* bit 0: critical */
	int routineType;
	MlnBuffer *routine;
	int cookie;
	union {
		struct MlnDateTime *time;	/* OnTime */
		int value;			/* OnTimeElapsed, OnAgentCompletion, OnEvent's event */
	} when;
	int eventStart;			/* OnEvent */
	int eventEnd;
} MlnCallback;

#define MLN_PERMISSION_OUTPUT_CONTROL_OVERRIDE	1

typedef struct MlnPermission {
	int type;
	unsigned int flags;		/* bit 0: critical */
	MlnBuffer *controlId;
	struct MlnOutputControl *control;
} MlnPermission;

typedef struct MlnRightsInfo {
	unsigned int flags;		/* bit 0: a critical parameter not understood */
	unsigned int obligationCount;
	MlnObligation *obligations;
	unsigned int callbackCount;
	MlnCallback *callbacks;
	unsigned int permissionCount;
	MlnPermission *permissions;
	unsigned int descriptionFlags;	/* bits 0-2: the three below */
	MlnBuffer *description;
	MlnBuffer *shortDescription;
	MlnBuffer *longDescription;
	unsigned int idFlags;		/* bit 0: ids are given, bit 1: requiredLicenseStatus */
	unsigned int idCount;		/* NodeReachabilityRequired */
	MlnBuffer **ids;
	int requiredLicenseStatus;
	unsigned int constraintFlags;	/* MLN_RIGHTS_* */
	unsigned int notBeforeKind;	/* how many times; 2: the later counts */
	unsigned int notAfterKind;	/* how many times; 2: the earlier counts */
	struct MlnDateTime *notBefore[2];
	struct MlnDateTime *notAfter[2];
	struct MlnDateTime *notDuring[2];
	int notLongerThan;
	int notMoreThan;
	unsigned int reserved;		/* SpatialConstraints: read, nothing kept */
	unsigned int groupFlags;	/* bit 0: membership, bit 1: identity */
	MlnBuffer *membership;
	MlnBuffer *identity;
	unsigned int deviceFlags;	/* MLN_DEVICE_* */
	MlnBuffer *deviceType;
	MlnBuffer *deviceFeature;
	MlnBuffer *deviceId;
	int versionMajor;
	int versionMinor;
	int dcsFreshnessInterval;
	unsigned int counterFlags;	/* bit 0: repeatCount */
	int repeatCount[2];
	unsigned int reserved2;
	struct {			/* the Custom obligation's values */
		int integer;
		int real;
		MlnBuffer *string;
		struct MlnDateTime *date;
		MlnBuffer *resource;
		MlnBuffer *text;
	} custom;
} MlnRightsInfo;

#define MLN_RIGHTS_NOT_BEFORE		0x1
#define MLN_RIGHTS_NOT_AFTER		0x2
#define MLN_RIGHTS_NOT_DURING		0x4
#define MLN_RIGHTS_NOT_LONGER_THAN	0x8
#define MLN_RIGHTS_NOT_MORE_THAN	0x10

#define MLN_DEVICE_TYPE			0x001
#define MLN_DEVICE_FEATURE		0x002
#define MLN_DEVICE_ID			0x004
#define MLN_DEVICE_LICENSE_SUSPENSION	0x008
#define MLN_DEVICE_METER_PLAY_DURATION	0x010
#define MLN_DEVICE_TRUSTED_TIME		0x020
#define MLN_DEVICE_VERSION_MAJOR	0x040
#define MLN_DEVICE_VERSION_MINOR	0x080
#define MLN_DEVICE_DCS_FRESHNESS	0x100

/* the summary of a license's rights: times in minutes, -1 when not given */
typedef struct MlnRightsSummary {
	unsigned short state;
	int notBefore;
	int notAfter;
	int notMoreThan;
	int notLongerThan;
	unsigned int idCount;
	MlnBuffer **ids;
} MlnRightsSummary;

/* the same, for the caller: the IDs as a set, the times as dates */
typedef struct MlnApiRights {
	struct MlnIdSet *ids;
	MlnApiDate notBefore;
	MlnApiDate notAfter;
	int notMoreThan;
	int notLongerThan;
} MlnApiRights;

/* what a report carries, by its type */
typedef union MlnApiReportData {
	MlnRightsSummary rights;
	MlnBuffer *buffer;
	void *value;
	struct {
		int first;
		int second;
	} times;
	struct {
		unsigned int count;
		MlnBuffer **items;
	} items;
} MlnApiReportData;

/* a request the engine has decoded from a message, and its arguments */
typedef struct MlnApiRequest {
	int type;
	void *arg[3];
} MlnApiRequest;

/* what processing a request reports back to the engine */
typedef struct MlnApiReport {
	int type;
	MlnApiReportData data;
} MlnApiReport;

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

/* a list of buffer pairs, from an engine's list of items two by two */
typedef struct MlnApiPair {
	struct MlnApiPair *next;
	MlnBuffer *key;
	MlnBuffer *value;
} MlnApiPair;

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
int SceMarlin_D09A4419(unsigned int size, const void *xml, MlnBundleInfo **out);
int SceMarlin_73C12840(MlnBundleInfo *info);
int SceMarlin_9609E2DF(MlnBuffer *buffer);
int SceMarlin_45B29121(MlnApiNode *list);
int SceMarlin_8FEB4F94(MlnApiObject *object);
int MlnApi_8100326C(MlnApiNode **list, MlnApiValue **value);
int MlnApi_81003818(MlnApiObject *object, MlnBuffer **response, MlnApiResult **result);
int MlnApi_810039A8(MlnApiObject *object, void *a1, void *a2, void *a3);
int MlnApi_8100370C(MlnApiObject *object, int status, MlnApiResult *result, MlnApiReport *report,
		    MlnBuffer **response);
int MlnApi_81003464(MlnBuffer *serviceProvider, MlnBuffer *token, unsigned int *count,
		    MlnBuffer ***items);
int MlnApi_81003A9C(MlnBuffer *license, MlnRightsSummary **summary);
int MlnApi_810037B0(const int *minutes, MlnApiDate *date);
int MlnApi_810026B8(void *a0, void *a1);
int SceMarlin_A43BC777(void *a0);
int SceMarlin_23838449(void *a0);
int SceMarlin_FB95B259(void *a0, void *a1, void *a2);
int SceMarlin_92A6A48F(void *a0, void *a1, void *a2);
int SceMarlin_A2E65A7E(void *a0, void *a1, void *a2);
int MlnImpl_8101EE90(void *client, MlnBuffer *license, MlnRightsSummary **summary);
int MlnImpl_8101F33C(void *client, int *a1);
int MlnImpl_81081400(void *a, void *b);
int MlnImpl_81081498(void *time, int *minutes);
int MlnIdSetAdd(MlnIdSet **set, const MlnBuffer *id);
int MlnNodesCollectReachable(MlnOctopusNode *nodes, const MlnBuffer *id, MlnIdSet **set);

/* three buffers and a counted list of buffer triples (string.c frees it) */
typedef struct MlnBufferGroup {
	unsigned int reserved;
	MlnBuffer *first;
	MlnBuffer *second;
	MlnBuffer *third;
	MlnBuffer ***entries;
	unsigned int entryCount;
} MlnBufferGroup;

/* SceDrmBridgeUser's calls (the SDK's stub library) */
int sceDrmBridgeIsAllowRemotePlayDebug(void);
int SceDrmBridgeUser_6F5DC6CD(void);

/* string.c */
unsigned short MlnHtons(unsigned int value);
unsigned int MlnNtohl(unsigned int value);
unsigned int MlnHtonl(unsigned int value);
char *MlnStrncpy(char *dst, const char *src, unsigned int n);
int MlnStrcmp(const char *a, const char *b);
int MlnStrncmp(const char *a, const char *b, unsigned int n);
char *MlnStrncat(char *dst, const char *src, unsigned int n);
char *MlnStrstr(const char *haystack, const char *needle);
char *MlnStrchr(const char *s, int c);
char *MlnStrrchr(const char *s, int c);
unsigned int MlnStrspn(const char *s, const char *accept);
void MlnImpl_81006CD4(int heap, MlnBuffer **buffers);
void MlnImpl_81006D14(int heap, MlnBufferGroup *group);
int MlnPlatformMode(void);
void **MlnImpl_81006EAC(void);
int MlnMemoryInit(int heap, void *memory, unsigned int size);
int MlnMemoryTerm(int heap);

/* time.c */
int MlnImpl_8100702C(void);
unsigned int MlnTimeNow(void);
int MlnImpl_81007098(void);
int MlnTimeGetTick(unsigned int *tick);
int MlnTimeToSeconds(const MlnDateTime *time, int *seconds);
int MlnTimeFromSeconds(const unsigned int *seconds, int utcOffset, int daylightSaving,
		       MlnDateTime *time);
int MlnTimeFromMinutes(const unsigned int *minutes, MlnDateTime *time);
int MlnTimeToMinutes(const MlnDateTime *time, unsigned int *minutes);

/* xml.c */
int MlnXmlSkipComment(MlnBuffer *cursor);
MlnXmlNode *MlnXmlNodeRetain(int heap, MlnXmlNode *node);
int MlnXmlReadText(int heap, MlnBuffer **text, MlnBuffer *cursor, int delimiter);
int MlnXmlParseProlog(int heap, MlnXmlParser *parser, MlnXmlProlog **out, MlnBuffer *cursor);
int MlnXmlCompareAttributeNames(int heap, const MlnXmlAttributeArray *namespaces,
				const MlnBuffer *elementName, const MlnBuffer *a, const MlnBuffer *b);
int MlnXmlParseElement(int heap, MlnXmlParser *parser, MlnXmlElement **out, MlnBuffer *cursor);
int MlnXmlParseContent(int heap, MlnXmlParser *parser, MlnXmlContent **out, MlnBuffer *cursor);
int MlnXmlNodeAppendXml(int heap, unsigned int size, const void *xml, MlnXmlNode *node);
int MlnXmlNodeReplaceChildXml(int heap, unsigned int size, const void *xml, MlnXmlNode *child,
			      MlnXmlNode *parent);
int MlnXmlParseInt(int heap, unsigned int size, const char *text, int *value);
int MlnXmlParseDateTime(int heap, unsigned int size, const char *text, MlnDateTime *time);
int MlnXmlElementMatchPredicate(const MlnXmlElement *element, const char *predicate,
				int length, int position);
int MlnXmlElementInNamespace(const MlnXmlElement *element, const void *uri, unsigned int size);
int MlnXmlElementMatchStep(int heap, MlnXmlElement *element, int *position, const char *name,
			   int nameLength, const char *predicate, int predicateLength,
			   const void *uri, unsigned int uriSize, MlnXmlLink **matches);
int MlnXmlContentMatchDescendants(int heap, MlnXmlContent *content, int *position,
				  const char *name, int nameLength, const char *predicate,
				  int predicateLength, const void *uri, unsigned int uriSize,
				  MlnXmlLink **matches);
int MlnXmlNodeMatchStep(int heap, MlnXmlNode *node, int *position, const char *name,
			int nameLength, const char *predicate, int predicateLength,
			const void *uri, unsigned int uriSize, MlnXmlLink **matches);
int MlnXmlNodeApplyStep(int heap, MlnXmlNode *node, const char *name, int nameLength, int axis,
			const char *predicate, int predicateLength, const void *uri,
			unsigned int uriSize, MlnXmlLink **matches);

/* xmlnode.c */
int MlnXmlNodeGetElement(int heap, MlnXmlNode *node, MlnXmlNode **out);
int MlnXmlNodeGetName(int heap, MlnXmlNode *node, MlnBuffer **name);
int MlnXmlBufferSet(int heap, MlnBuffer **buffer, const void *data, unsigned int size);
int MlnXmlBufferTruncate(int heap, MlnBuffer **buffer, unsigned int size);
int MlnXmlLinkCreateEmpty(int heap, MlnXmlLink **out);
int MlnXmlLinksFree(int heap, MlnXmlLink **list, int kind);
int MlnXmlLinkPush(int heap, MlnXmlLink **list, void *item);
int MlnXmlLinkAppend(int heap, MlnXmlLink **list, void *item);
int MlnXmlAttributeInsertSorted(int heap, MlnXmlLink **list, MlnXmlAttribute *attribute);
int MlnXmlLinksPop(int heap, MlnXmlLink **list, int kind);
int MlnXmlLinksToAttributeArray(int heap, MlnXmlLink *list, MlnXmlAttributeArray **out);
int MlnXmlAttributesSortUnique(int heap, MlnXmlLink *list, MlnXmlAttributeArray **out);
int MlnXmlLinksToContentArray(int heap, MlnXmlLink *list, MlnXmlContentArray **out);
int MlnXmlLinksToNodeList(int heap, MlnXmlLink *list, MlnXmlNodeList **out);
int MlnXmlParserTerm(int heap, MlnXmlParser *parser);
int MlnXmlStackPush(int heap, MlnXmlStack *stack, MlnXmlAttribute *declaration);
int MlnXmlStackPop(int heap, MlnXmlStack *stack);
int MlnXmlStackGetNamespaces(int heap, MlnXmlStack *stack, MlnXmlAttributeArray **out);
int MlnXmlContentCreate(int heap, MlnXmlContent **out);
int MlnXmlContentArrayCreate(int heap, MlnXmlContentArray **out, int count);
int MlnXmlContentArrayRelease(int heap, MlnXmlContentArray **array);
int MlnXmlAttributeCreate(int heap, MlnXmlAttribute **out);
int MlnXmlAttributeRelease(int heap, MlnXmlAttribute **attribute);
int MlnXmlAttributeArrayCreate(int heap, MlnXmlAttributeArray **out, int count);
int MlnXmlAttributeArrayRelease(int heap, MlnXmlAttributeArray **array);
int MlnXmlElementCreate(int heap, MlnXmlElement **out);
int MlnXmlElementRelease(int heap, MlnXmlElement **element);
int MlnXmlContentRelease(int heap, MlnXmlContent **content);
int MlnXmlPrologCreate(int heap, MlnXmlProlog **out);
int MlnXmlPrologRelease(int heap, MlnXmlProlog **prolog);
int MlnXmlDocumentCreate(int heap, MlnXmlDocument **out);
int MlnXmlDocumentRelease(int heap, MlnXmlDocument **document);
int MlnXmlNodeCreate(int heap, MlnXmlNode **out);
int MlnXmlNodeCreateDocument(int heap, MlnXmlNode **out, MlnXmlDocument *document);
int MlnXmlNodeCreateElement(int heap, MlnXmlNode **out, MlnXmlElement *element);
int MlnXmlNodeCreateNamespace(int heap, MlnXmlNode **out, MlnXmlAttribute *declaration);
int MlnXmlNodeCreateAttribute(int heap, MlnXmlNode **out, MlnXmlAttribute *attribute);
int MlnXmlNodeCreateText(int heap, MlnXmlNode **out, const MlnBuffer *text);
int MlnXmlNodeRelease(int heap, MlnXmlNode **node);
int MlnXmlNodeListCreate(int heap, MlnXmlNodeList **out, int count);
int MlnXmlNodeListRelease(int heap, MlnXmlNodeList **list);
int MlnXmlParserInit(int heap, MlnXmlParser *parser);

/* xmlbuild.c: what an element is built from */
typedef struct MlnXmlBuildAttribute {
	const char *name;
	const char *value;
} MlnXmlBuildAttribute;

#define MLN_XML_BUILD_STRING	0	/* a C string */
#define MLN_XML_BUILD_NODE	1	/* an element node */
#define MLN_XML_BUILD_BUFFER	2	/* text in a buffer */
#define MLN_XML_BUILD_INT	3	/* an integer, as decimal text */
#define MLN_XML_BUILD_DATETIME	4	/* a dateTime, as text */
#define MLN_XML_BUILD_NUMBER	5	/* a big-endian unsigned integer, as decimal text */

typedef struct MlnXmlBuildContent {
	int kind;
	union {
		const char *string;
		MlnXmlNode *node;
		MlnBuffer *buffer;
		int integer;
		const MlnDateTime *time;
	} value;
} MlnXmlBuildContent;

int MlnXmlIntToText(int heap, int value, MlnBuffer **text);
int MlnXmlBytesToDecimal(int heap, const MlnBuffer *number, MlnBuffer **text);
int MlnXmlDateTimeToText(int heap, const MlnDateTime *time, MlnBuffer **text);
int MlnXmlElementInheritNamespaces(int heap, MlnXmlElement *element,
				   MlnXmlAttributeArray *namespaces);
int MlnXmlElementAppendContents(int heap, MlnXmlElement *element, int count,
				MlnXmlContent **items);
int MlnXmlElementRemoveChild(int heap, MlnXmlElement *element, MlnXmlElement *child);
int MlnXmlElementReplaceChild(int heap, MlnXmlElement *element, MlnXmlElement *child,
			      MlnXmlContent *content);
int MlnXmlElementBuild(int heap, const char *name, const char *prefix, const char *uri,
		       int attributeCount, const MlnXmlBuildAttribute *attributeList,
		       int contentCount, MlnXmlBuildContent *contentList, int release,
		       MlnXmlNode **out);
int MlnXmlNodeAddAttribute(int heap, MlnXmlNode *node, const char *name, const char *value);
int MlnXmlNodeAddText(int heap, MlnXmlNode *node, const char *text);
int MlnXmlNodeSetText(int heap, MlnXmlNode *node, const char *text);
int MlnXmlNodeAppendChild(int heap, MlnXmlNode *parent, MlnXmlNode *child, int release);
int MlnXmlNodeSerialize(int heap, MlnXmlNode *node, const char *version, const char *encoding,
			MlnBuffer **out);
int MlnXmlNodesSerialize(int heap, int count, MlnXmlNode **nodes, MlnBuffer **out);
int MlnXmlNodeRemoveChild(int heap, MlnXmlNode *parent, MlnXmlNode *child);
int MlnXmlNodeToElement(int heap, MlnXmlNode *node, MlnXmlNode **out);

/* a growing byte buffer, its capacity whole 4 KiB pages (not written in C yet) */
typedef struct MlnTextBuffer {
	unsigned int size;
	unsigned char *data;
	unsigned int capacity;
} MlnTextBuffer;

MlnTextBuffer *MlnTextBufferCreate(int heap, unsigned int size, const void *data);
int MlnTextBufferAppend(int heap, MlnTextBuffer *buffer, unsigned int size, const void *data);
void MlnTextBufferDestroy(int heap, MlnTextBuffer *buffer);
/* an element node as XML text (not written in C yet) */
int MlnXmlNodeToText(int heap, MlnXmlNode *node, MlnBuffer **text);

/* esb.c: an extended status block's values */
#define MLN_VALUE_INTEGER		0
#define MLN_VALUE_REAL			1
#define MLN_VALUE_STRING		2
#define MLN_VALUE_DATE			3
#define MLN_VALUE_PARAMETER		4
#define MLN_VALUE_EXTENDED_PARAMETER	5
#define MLN_VALUE_RESOURCE		6
#define MLN_VALUE_LIST			7
#define MLN_VALUE_BYTES			8	/* the Custom obligation's last value */

struct MlnValue;

typedef struct MlnParameter {
	MlnBuffer *name;
	struct MlnValue *value;
} MlnParameter;

typedef struct MlnExtendedParameter {
	MlnBuffer *name;
	struct MlnValue *value;
	int flags;
} MlnExtendedParameter;

typedef struct MlnValueList {
	int count;
	struct MlnValue *items[];
} MlnValueList;

typedef struct MlnValue {
	int type;
	union {
		int integer;			/* an integer, a real's bits, a date */
		MlnBuffer *buffer;		/* a string, a resource */
		MlnParameter *parameter;
		MlnExtendedParameter *extended;
		MlnValueList *list;
	} data;
} MlnValue;

/* an output control an action's status block names */
#define MLN_OUTPUT_CONTROL_BASIC_CCI	1
#define MLN_OUTPUT_CONTROL_DTCP		2

#define MLN_OUTPUT_CONTROL_UNKNOWN		0x01	/* a parameter or control not known */
#define MLN_OUTPUT_CONTROL_EPN			0x02
#define MLN_OUTPUT_CONTROL_CCI			0x04
#define MLN_OUTPUT_CONTROL_IMAGE_CONSTRAINT	0x08
#define MLN_OUTPUT_CONTROL_DIGITAL_ONLY		0x10
#define MLN_OUTPUT_CONTROL_APS			0x20
#define MLN_OUTPUT_CONTROL_RETENTION_MOVE_MODE	0x40
#define MLN_OUTPUT_CONTROL_RETENTION_STATE	0x80

typedef struct MlnOutputControl {
	int kind;
	unsigned int flags;
	struct {
		int epn;
		int cci;
		int imageConstraintToken;
		int digitalOnlyToken;
		int aps;
	} basic;
	struct {
		int retentionMoveMode;
		int retentionState;
		int epn;
		int cci;
		int imageConstraintToken;
		int aps;
	} dtcp;
} MlnOutputControl;

int MlnOutputControlParse(int heap, const MlnValueList *parameters, const MlnBuffer *id,
			  int report, unsigned int *flags, MlnOutputControl **out);
int MlnValueCopy(int heap, const MlnValue *value, MlnValue **out);
/* not written in C yet */
void MlnValueFree(int heap, MlnValue *value);
void MlnValueListFree(int heap, MlnValueList *list);
void MlnExtendedParameterFree(int heap, MlnExtendedParameter *extended);
int MlnRightsInfoFree(int heap, MlnRightsInfo *info);
int MlnRightsInfoParse(int heap, unsigned int size, const void *data, MlnRightsInfo **out);

/* a decoded extended status block (not written in C yet) */
typedef struct MlnEsb {
	unsigned int reserved0[3];
	unsigned int flags;		/* bit 0: obligations, 1: callbacks, 2-7: the constraint groups */
	unsigned int reserved1[2];
	MlnValueList *parameters;
} MlnEsb;

int MlnEsbDecode(int heap, unsigned int size, const void *data, MlnEsb **out);
int MlnImpl_81026814(int heap, void *value, void *a2, void *a3, MlnBuffer **out);
void MlnEsbFree(int heap, MlnEsb *esb);

/* not written in C yet: the prototypes are their callers' */
int MlnCoreInit(void *memory, unsigned int size);
int MlnCoreTerm(void);
int MlnPlatformInit(void);
int MlnPlatformTerm(void);
unsigned int MlnStateSerialize(MlnBuffer **state);
int MlnServiceSetState(MlnServiceContext *context, MlnBuffer *state);
int MlnServiceClearState(MlnServiceContext *context);
int MlnImpl_81013350(void *a0, void *a1);
int MlnImpl_81016A5C(void *a0, void *a1, void *a2);
int MlnImpl_81016B4C(void *a0, void *a1);
int MlnImpl_81018B8C(MlnServiceContext *context, MlnBuffer *serviceProvider, MlnBuffer *token,
		     MlnBuffer *ids, unsigned int *count, MlnBuffer ***items);
int MlnImpl_81020094(void *session, int status, MlnApiValue *value, MlnHttpRequest *request,
		     void *a4, void *a5);
void MlnImpl_81013420(MlnApiResult *result);
void MlnImpl_810134E0(MlnApiRequest *request);
void MlnImpl_8101359C(MlnApiReport *report);
int MlnImpl_8101F954(void *session, MlnBuffer *message, int status, void *data,
		     MlnApiRequest *request);
int MlnImpl_8101A360(void *engine, MlnApiResult **result);
int MlnImpl_8101A59C(MlnBuffer **handle);
int MlnImpl_81018E08(MlnServiceContext *context, void *a1, void *a2);
int MlnImpl_8101F1F0(void *client, void *a1, MlnBuffer **found);
int MlnImpl_8101F0B8(void *client, void *a1, void *a2);
int MlnImpl_8101DFA0(void *client, void *a1, void *a2, int *first, int *second);
int MlnImpl_810188E8(MlnServiceContext *context, void *a1, void *a2, int *first, int *second);
int MlnImpl_81018A2C(MlnServiceContext *context, unsigned int *count, MlnBuffer ***items);
int MlnImpl_8101EC08(void *client, unsigned int *count, MlnBuffer ***items);
int MlnImpl_8101E118(void *client, MlnBuffer *a1, MlnBuffer *a2, unsigned int *count,
		     MlnBuffer ***items);
int MlnImpl_81018D00(MlnServiceContext *context, void **a1);
int MlnImpl_81018DA0(MlnServiceContext *context, MlnBuffer *a1);
int MlnImpl_8101F4B0(void *client, void **a1);
int MlnImpl_8101F5F4(void *client, MlnBuffer *a1);
int MlnImpl_8101F724(void *client, MlnBuffer *token, MlnBuffer **found);
int MlnImpl_8102045C(void *session);
int MlnImpl_8101A020(void *engine);
int MlnImpl_8101DD24(MlnApiKind *kind, MlnBuffer *address, void **client, MlnApiVersion *version);
int MlnImpl_8101DF70(void *client);
int MlnImpl_8101F870(MlnApiKind *kind, MlnBuffer *address, void **session);
int MlnImpl_8101F92C(void *session);
int MlnImpl_8101A030(void *engine, MlnBuffer **message, MlnApiResult **result);
int MlnImpl_8101E628(void *client);
int MlnImpl_8101E658(void *client, MlnBuffer **message, MlnApiResult **result);
int MlnImpl_8101E9EC(void *client, MlnApiResult *step, MlnBuffer **message);
int MlnImpl_81019B44(int a0, void *a1, void *a2, void *a3, int a4, void **engine);
int MlnImpl_8101E380(void *client, void *a1, void *a2, void *a3, int a4);
int MlnXmlParse(int heap, unsigned int size, const void *xml, MlnXmlNode **document);
int MlnXmlFindNode(int heap, MlnXmlNode *node, const char *path, const char *ns, MlnXmlNode **found);
int MlnXmlFindNodes(int heap, MlnXmlNode *node, const char *path, const char *ns,
		    MlnXmlNodeList **out);
int MlnXmlGetAttribute(int heap, MlnXmlNode *node, const char *ns, const char *name,
		       MlnBuffer **value);
int MlnXmlGetText(int heap, MlnXmlNode *node, MlnBuffer **text);
int MlnXmlNodeFree(int heap, MlnXmlNode *node);
int MlnXmlListFree(int heap, MlnXmlNodeList *list);
void MlnImpl_810136AC(void);
int MlnImpl_810136E0(void *a0, void *a1, void *a2, void *a3, unsigned char a4, void *a5);
int MlnImpl_81013804(void *a0, void *a1, MlnBuffer **a2);
int MlnImpl_810147D8(void *a0, int a1);
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
