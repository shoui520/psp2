/*
 * SceLibMarlin's private interface: Intertrust's Marlin (Octopus/Nemo) DRM client, the
 * memory it allocates from two heaps of its own (the second for secure data), its byte
 * buffers and its locks.
 */
#ifndef MLN_INTERNAL_H
#define MLN_INTERNAL_H

#include <stddef.h>
#include <pfsmgr/pfsmgr.h>

/* return codes of the library's helpers */
#define MLN_OK			0
#define MLN_ERROR_NO_MEMORY	2
#define MLN_ERROR_INVALID_ARGUMENT	3
#define MLN_ERROR_INVALID	5

/* a big-endian word, read as the library reads it */
#define MLN_BE32(p)		((unsigned int)(p)[0] << 24 | (p)[1] << 16 | (p)[3] | (p)[2] << 8)
#define MLN_CORE_ERROR_INVALID	4
#define MLN_ENGINE_ERROR_FORMAT	0x1f0001	/* a token, configuration or response the engine cannot read */

/* the area of the application data the library keeps its files in, and a path's room */
#define MLN_FILE_AREA		1
#define MLN_PATH_MAX		256
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

/*
 * The transport to the service: transact sends a request and gives the response, release
 * frees the response. Both unset: the library's own service, in this process.
 */
typedef struct MlnServiceSession {
	void *cookie;
	int (*transact)(void *cookie, const MlnBuffer *request, MlnBuffer **response);
	int (*release)(MlnBuffer *response);
} MlnServiceSession;

/* a session with the client's service */
typedef struct MlnServiceContext {
	int id;
	MlnServiceSession *session;
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

typedef struct MlnHttpHeaders {
	unsigned int count;
	MlnHttpHeader **items;
} MlnHttpHeaders;

typedef struct MlnHttpRequest {
	int method;
	char *url;
	MlnHttpHeaders headers;
	MlnBuffer *body;
} MlnHttpRequest;

void MlnFree(int heap, void *p);

static inline __attribute__((always_inline)) void MlnHttpHeaderArrayFree(MlnHttpHeader **items,
									 unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (items[i] != NULL) {
			MlnFree(MLN_HEAP_SECURE, items[i]->name);
			MlnFree(MLN_HEAP_SECURE, items[i]->value);
			MlnFree(MLN_HEAP_SECURE, items[i]);
		}
	}
	MlnFree(MLN_HEAP_SECURE, items);
}

/* what one step of a transaction gives: a value for the caller, or a request to send */
typedef struct MlnApiResult {
	MlnApiValue *value;
	MlnHttpRequest *request;
	int progress;			/* what the step did (1: the actions start), */
	unsigned int progressTotal;	/* the steps of the whole token */
	unsigned int progressDone;	/* and those done */
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
	MlnServiceSession *remote;	/* the remote client's transport, NULL for the local engine */
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
	struct MlnEngine *engine;
	struct MlnClient *client;
	struct MlnSession *session;
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
	int state;			/* 0 or 0x100100 */
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
int MlnApi_810026B8(MlnServiceContext *context, MlnBuffer **esb);
int SceMarlin_A43BC777(MlnServiceContext *context);
int SceMarlin_23838449(MlnServiceContext *context);
int SceMarlin_FB95B259(MlnServiceContext *context, unsigned int size, void *data);
int SceMarlin_92A6A48F(MlnServiceContext *context, int value, MlnRightsInfo **rights);
int SceMarlin_A2E65A7E(MlnServiceContext *context, int value, MlnRightsInfo **rights);
int MlnImpl_8101EE90(struct MlnClient *client, MlnBuffer *license, MlnRightsSummary **summary);
int MlnImpl_8101F33C(struct MlnClient *client, int *state);
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

/* a growing byte buffer, its capacity whole 4 KiB pages (textbuffer.c) */
typedef struct MlnTextBuffer {
	unsigned int size;
	unsigned char *data;
	unsigned int capacity;
} MlnTextBuffer;

MlnTextBuffer *MlnTextBufferCreate(int heap, unsigned int size, const void *data);
int MlnTextBufferAppend(int heap, MlnTextBuffer *buffer, unsigned int size, const void *data);
int MlnTextBufferSetSize(int heap, MlnTextBuffer *buffer, unsigned int size);
int MlnTextBufferCompare(int heap, const MlnTextBuffer *a, const MlnTextBuffer *b);
MlnTextBuffer *MlnTextBufferCreateFilled(int heap, unsigned int size, int value);
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
#define MLN_VALUE_BYTE_ARRAY		8

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
int MlnValueFree(int heap, MlnValue *value);
int MlnValueListFree(int heap, MlnValueList *list);
int MlnExtendedParameterFree(int heap, MlnExtendedParameter *extended);
int MlnParameterFree(int heap, MlnParameter *parameter);
int MlnValueListDecode(int heap, const unsigned int **cursor, MlnValueList **out);
int MlnValueDecode(int heap, const unsigned int **cursor, MlnValue **out);
int MlnRightsInfoFree(int heap, MlnRightsInfo *info);
int MlnRightsInfoParse(int heap, unsigned int size, const void *data, MlnRightsInfo **out);

/* a decoded extended status block, Octopus's ExtendedStatusBlock */
#define MLN_ESB_GLOBAL_UNSUPPORTED	0x01	/* a block this library cannot act on */

typedef struct MlnEsb {
	unsigned int globalFlags;
	unsigned int category;
	unsigned int subCategory;
	unsigned int localFlags;	/* bit 0: obligations, 1: callbacks, 2-7: the constraint groups */
	unsigned int cacheDurationType;
	unsigned int cacheDurationValue;
	MlnValueList *parameters;
} MlnEsb;

int MlnEsbDecode(int heap, unsigned int size, const void *data, MlnEsb **out);
/* base64: lineLength characters a line (0: one line), lineEnd 0 "\n", 1 "\r\n", 2 "\r" */
int MlnImpl_81026814(int heap, const MlnBuffer *in, int lineLength, unsigned int lineEnd,
		     MlnBuffer **out);
int MlnImpl_81026A9C(int heap, const MlnBuffer *in, MlnBuffer **out);
int MlnEsbFree(int heap, MlnEsb *esb);
int MlnEsbCheckGlobalFlags(int heap, const MlnEsb *esb);

/* not written in C yet: the prototypes are their callers' */
/* core.c */
typedef struct MlnBlob {
	MlnBuffer *buffer;
	unsigned int size;
} MlnBlob;

int MlnCoreInit(void *memory, unsigned int size);
int MlnCoreTerm(void);
int MlnImpl_81012F48(void *a0, const MlnBuffer *a1, MlnBuffer **first, MlnBuffer **second);
int MlnImpl_81013074(void *a0, const MlnBuffer *a1, MlnBlob *first, MlnBlob *second);
int MlnHttpHeadersCreate(unsigned int count, MlnHttpHeader *const *headers, MlnHttpHeaders *out);
void MlnHttpHeadersFree(MlnHttpHeaders *headers);
void MlnHttpRequestFree(MlnHttpRequest *request);
void MlnApiValueFree(MlnApiValue *value);
void MlnApiResultFree(MlnApiResult *result);
void MlnApiRequestClear(MlnApiRequest *request);
void MlnApiReportClear(MlnApiReport *report);

/* two buffers and a flag the API layer hands the core */
typedef struct MlnBufferPair {
	MlnBuffer *first;
	MlnBuffer *second;
	unsigned char flag;
} MlnBufferPair;

void MlnBufferPairFree(MlnBufferPair *pair);
int MlnBufferPairCreate(unsigned int firstSize, const void *firstData, unsigned int secondSize,
			const void *secondData, unsigned char flag, MlnBufferPair **out);
int MlnCompositeIdCreate(const MlnBuffer *accountId, const MlnBuffer *entitlementId,
			 MlnBuffer **out);
int MlnStateFilePath(unsigned int id, int create, char *path);
int MlnStateFileRead(unsigned int id, unsigned char *data, unsigned int capacity,
		     unsigned int *size);
int MlnStateGroupRead(int group, unsigned char *end, unsigned char *data, unsigned int *size);
int MlnStateCollect(unsigned int *size, const MlnBuffer *buffer);
int MlnStateFileWrite(unsigned int id, const void *data, unsigned int size);
int MlnStateSerialize(MlnBuffer **state);
int MlnStateRestore(const MlnBuffer *state);
/* a call into the client's service: typed arguments, a reply and a status */
#define MLN_SERVICE_ARG_HANDLE		0
#define MLN_SERVICE_ARG_SIZE		1	/* the value points to a nonzero size */
#define MLN_SERVICE_ARG_INTEGER		2
#define MLN_SERVICE_ARG_ID		3	/* the value points to an identifier */
#define MLN_SERVICE_ARG_BLOCK28		4	/* the value points to 28 bytes (sized at 0x8102766c) */
#define MLN_SERVICE_ARG_OFFSET		5	/* the value points to an offset (also given back) */
#define MLN_SERVICE_ARG_BUFFER		6
#define MLN_SERVICE_ARG_FLAG		7	/* the value points to a flag */
#define MLN_SERVICE_ARG_RECORD21	8	/* the value points to a MlnServiceRecord */
#define MLN_SERVICE_ARG_OBJECT		9	/* an object led by a buffer (sized at 0x8102766c) */
#define MLN_SERVICE_ARG_STRUCT		10	/* an object handed over as it is */
#define MLN_SERVICE_ARG_LIST		11	/* a counted list of entries (sized at 0x8102766c) */
#define MLN_SERVICE_ARG_GROUP		12	/* a buffer group */
#define MLN_SERVICE_ARG_RECORD		13	/* a record led by a buffer (sized at 0x8102766c) */

#define MLN_SERVICE_SET_STATE		0x411
#define MLN_SERVICE_CLEAR_STATE		0x412

#define MLN_SERVICE_STATUS_PARTIAL	29

typedef struct MlnServiceArg {
	int type;
	void *value;
} MlnServiceArg;

typedef struct MlnServiceArgs {
	unsigned int count;
	MlnServiceArg *items;
} MlnServiceArgs;

/* a service reply has the arguments' form: its values, each a type and a value */
typedef MlnServiceArgs MlnServiceReply;

int MlnImpl_81014574(MlnServiceContext *context, int command, int value, MlnRightsInfo **rights,
		     MlnBuffer **esb);
int MlnServiceSetState(MlnServiceContext *context, MlnBuffer *state);
int MlnServiceClearState(MlnServiceContext *context);

#define MLN_SERVICE_ERROR_REPLY		512	/* a reply of another type */
#define MLN_SERVICE_STATUS_MORE		68	/* a reply that continues */
#define MLN_SERVICE_ERROR_COMMAND	513	/* the reply to another command */
#define MLN_SERVICE_ERROR_UNAVAILABLE	515	/* no reply, or a command a remote session refuses */
#define MLN_SERVICE_ERROR_TRANSPORT	516

/* what a session's transact gives back */
#define MLN_TRANSPORT_OK		0
#define MLN_TRANSPORT_INVALID		1
#define MLN_TRANSPORT_NO_MEMORY		2
#define MLN_TRANSPORT_FAILED		3

/* the application's data for a service command, and its release */
typedef int (*MlnServiceDataGet)(void *cookie, void *arg, void **data, unsigned int *size);
typedef int (*MlnServiceDataRelease)(void *cookie, void *data);

int MlnImpl_810147D8(MlnServiceContext *context, int value);
int MlnImpl_81014860(MlnServiceContext *context, unsigned int size, void *data, MlnBuffer *buffer);
int MlnImpl_810148F8(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81014A10(MlnServiceContext *context, MlnServiceDataGet get,
		     MlnServiceDataRelease release, void *arg, void *cookie);
int MlnImpl_81014B8C(MlnServiceContext *context, MlnBuffer *buffer);
int MlnImpl_81014C20(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81014D30(MlnServiceContext *context, MlnBuffer *buffer);
int MlnImpl_81014DC4(MlnServiceContext *context);
int MlnImpl_81014E28(MlnServiceContext *context, MlnBuffer *buffer, unsigned int *out);
int MlnImpl_81014F24(MlnServiceContext *context, MlnBuffer *buffer);
int MlnImpl_81014FA8(MlnServiceContext *context, MlnBuffer *buffer);
int MlnImpl_8101503C(MlnServiceContext *context, int value, void *data);
int MlnImpl_81015168(MlnServiceContext *context);
int MlnImpl_8101521C(MlnServiceContext *context, unsigned int size, void *data, const char *name,
		     MlnBuffer **out);
int MlnImpl_81015374(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_8101549C(MlnServiceContext *context, unsigned int size, void *data, const char *name,
		     MlnBuffer *other, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81015620(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **outFirst,
		     MlnBuffer **outSecond);
int MlnImpl_81015798(MlnServiceContext *context, unsigned int size, void *data, const char *name,
		     MlnBuffer *in, MlnBuffer **out);
int MlnImpl_810158FC(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81015A24(MlnServiceContext *context, unsigned int size, void *data, const char *name,
		     MlnBuffer *other, MlnBuffer *extra, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81015BB0(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81015CD8(MlnServiceContext *context, MlnBufferGroup **out);
int MlnImpl_81015DDC(MlnServiceContext *context, unsigned int firstSize, void *firstData,
		     unsigned int secondSize, void *secondData, unsigned int thirdSize,
		     void *thirdData);
int MlnImpl_81015EA4(MlnServiceContext *context, MlnBuffer *in, MlnBuffer *other);
int MlnImpl_81015F3C(MlnServiceContext *context);
int MlnImpl_81016034(MlnServiceContext *context);
int MlnImpl_810160E8(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_8101617C(MlnServiceContext *context, void *record, int *out);
int MlnImpl_81016270(MlnServiceContext *context, int id);
int MlnImpl_810162F4(MlnServiceContext *context, int value, MlnRightsInfo **rights);
int MlnImpl_8101632C(MlnServiceContext *context, int value, MlnRightsInfo **rights);
int MlnImpl_81016364(MlnServiceContext *context, int value, MlnRightsInfo **rights);
int MlnImpl_810163A8(MlnServiceContext *context, MlnBuffer **esb);

/* what a rights evaluation (command 0x42f) is limited to: its kind and up to two values */
typedef struct MlnEvaluationLimit {
	int kind;
	unsigned int value;
	unsigned int first;
	unsigned int second;
} MlnEvaluationLimit;

#define MLN_LIMIT_KIND_WORD	5	/* the value as a word, and kind 4's limit */
#define MLN_LIMIT_KIND_COUNT	4	/* value 4 carries two words */

int MlnImpl_810163E4(MlnServiceContext *context, void *object, const MlnEvaluationLimit *limit,
		     MlnRightsInfo **rights);
int MlnImpl_810166AC(MlnServiceContext *context, MlnBuffer *in, MlnRightsInfo **rights);
int MlnImpl_81016844(MlnServiceContext *context, int id, unsigned int offset, unsigned int size,
		     unsigned int *position, unsigned int count);
int MlnImpl_8101698C(MlnServiceContext *context, int id, int value, unsigned int firstSize,
		     void *firstData, unsigned int secondSize, void *secondData);
int MlnImpl_81016A5C(MlnServiceContext *context, int id, MlnBuffer **out);
int MlnImpl_81016B4C(MlnServiceContext *context, int id, unsigned int value, MlnBuffer **out);
int MlnImpl_81016C70(MlnServiceContext *context);
int MlnImpl_81016D68(MlnServiceContext *context);
int MlnImpl_81016E1C(MlnServiceContext *context, MlnBuffer **out);
int MlnImpl_81016F20(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81017038(MlnServiceContext *context, MlnBuffer *in);
int MlnImpl_810170C4(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_81017158(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_810171EC(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_81017280(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_81017314(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_810173A8(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_8101743C(MlnServiceContext *context, unsigned int size, void *data, MlnBuffer **out);
int MlnImpl_8101753C(MlnServiceContext *context, unsigned int firstSize, void *firstData,
		     unsigned int secondSize, void *secondData);
int MlnImpl_810175FC(MlnServiceContext *context, unsigned int size, void *data);
int MlnImpl_81017690(MlnServiceContext *context, void *block);
int MlnImpl_81017710(MlnServiceContext *context);

/* the 21-byte value of a service reply of type 8 */
typedef struct __attribute__((packed)) MlnServiceRecord {
	unsigned int words[5];
	unsigned char flag;
} MlnServiceRecord;

MlnServiceRecord MlnImpl_81017764(MlnServiceContext *context);
void MlnImpl_8101782C(MlnServiceContext *context);
int MlnImpl_81017D1C(MlnServiceContext *context, void **list, unsigned int firstCount,
		     MlnBuffer **first, unsigned int secondCount, MlnBuffer **second,
		     const MlnBuffer *third, const MlnBuffer *fourth);
int MlnImpl_81018220(MlnServiceContext *context);
int MlnImpl_810182D0(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81018400(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81018528(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81018650(MlnServiceContext *context, MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81018784(MlnServiceContext *context, MlnBuffer *in, MlnBuffer *other,
		     MlnBuffer **out);
int MlnImpl_810188E8(MlnServiceContext *context, MlnBuffer *other, MlnBuffer *in, int *first,
		     int *second);
int MlnImpl_81018A2C(MlnServiceContext *context, unsigned int *count, MlnBuffer ***buffers);
int MlnImpl_81018B8C(MlnServiceContext *context, MlnBuffer *serviceProvider, MlnBuffer *token,
		     MlnBuffer *ids, unsigned int *count, MlnBuffer ***items);
int MlnImpl_81018D00(MlnServiceContext *context, void **out);
int MlnImpl_81018DA0(MlnServiceContext *context, MlnBuffer *in);
int MlnImpl_81018E08(MlnServiceContext *context, MlnBuffer *other, MlnBuffer *in);
int MlnImpl_81018EC4(MlnServiceContext *context);
int MlnImpl_81018F50(MlnServiceContext *context);
int MlnImpl_81018FD0(MlnServiceContext *context, unsigned int *size, void *data, int value);
int MlnImpl_81019108(MlnServiceContext *context, MlnBuffer *in);
unsigned short MlnImpl_81019274(int command);
int MlnImpl_81017880(MlnServiceContext *context, void *list, unsigned int *firstCount,
		     MlnBuffer ***first, unsigned int *secondCount, MlnBuffer ***second,
		     MlnBuffer **third, MlnBuffer **fourth);
int MlnImpl_81026F48(int heap, const MlnServiceArg *value, MlnServiceRecord *out);
int MlnImpl_810270BC(int heap, const MlnServiceArg *value, unsigned int *out);
/* not written in C yet */
int MlnImpl_81027084(int heap, const MlnServiceArg *value, int *out);
int MlnImpl_81026FE4(int heap, const MlnServiceArg *value, unsigned int *out);
int MlnImpl_8102701C(int heap, const MlnServiceArg *value, int *out);
int MlnImpl_81026ED0(int heap, MlnServiceArg *value, MlnBufferGroup **out);
int MlnServiceCall(MlnServiceSession *session, int command, const MlnServiceArgs *args,
		   MlnServiceReply **reply, int *status);
int MlnImpl_810194B4(MlnBuffer *response);
int MlnImpl_810194CC(const MlnBuffer *request, MlnBuffer **response);
int MlnImpl_81019760(void *cookie, const MlnBuffer *request, MlnBuffer **response);
int MlnImpl_81019798(const MlnBuffer *request, MlnBuffer **response);
int MlnImpl_81019860(MlnServiceSession *session, int command);
/* the service message header: four big-endian words */
int MlnImpl_81026D94(const MlnServiceReply *reply, unsigned int count);
void MlnImpl_81026BFC(unsigned char *header, unsigned int command);
void MlnImpl_81026C1C(unsigned char *header, unsigned int size);
void MlnImpl_81026C3C(unsigned char *header, unsigned int status);
void MlnImpl_81026C5C(unsigned char *header, unsigned int value);
void MlnImpl_81026C7C(unsigned char *header, unsigned int first);
void MlnImpl_81026C9C(unsigned char *header, unsigned int second);
unsigned int MlnImpl_81026CBC(const unsigned char *header);
unsigned int MlnImpl_81026CE0(const unsigned char *header);
int MlnImpl_81026D04(const unsigned char *header);
unsigned int MlnImpl_81026D28(const unsigned char *header);
unsigned int MlnImpl_81026D4C(const unsigned char *header);
unsigned int MlnImpl_81026D70(const unsigned char *header);
int MlnImpl_81026DE0(int heap, const MlnServiceArgs *args, MlnBuffer **out);
int MlnImpl_81026FAC(int heap, const MlnServiceArg *value, int *out);
int MlnImpl_8102704C(int heap, const MlnServiceArg *value, unsigned int *out);
int MlnImpl_810270F4(int heap, const MlnServiceArg *value, void *out);
int MlnImpl_81027140(int heap, MlnServiceArg *value, void **out);
int MlnImpl_8102717C(int heap, MlnServiceArg *value, void **out);
int MlnImpl_810271B8(int heap, MlnServiceArg *value, void **out);
int MlnImpl_810271F4(int heap, MlnServiceArg *value, void **out);
int MlnImpl_81027B50(int heap, const MlnServiceArgs *args, MlnBuffer **out);
int MlnImpl_810273C4(int heap, int command, int value, const MlnServiceArgs *args,
		     MlnBuffer **request);
int MlnImpl_8102752C(int heap, const MlnBuffer *response, MlnServiceReply **reply);
void MlnImpl_81027390(int heap, MlnServiceReply *reply);
int MlnImpl_81026F0C(int heap, MlnServiceArg *value, MlnBuffer **out);
int MlnImpl_81026E20(int heap, unsigned int count, MlnServiceArgs **out);
int MlnImpl_810191C4(int ret);

/* the file layer's errors: the platform's (0x11060001-0x11060008) and the results they give */
#define MLN_FILE_ERROR_SPACE	2
#define MLN_FILE_ERROR_INVALID	5
#define MLN_FILE_ERROR_PLATFORM	0x11060001
#define MLN_FILE_ERROR_NOT_FOUND	0x11060002

typedef struct MlnFileInfo {
	unsigned int type;	/* 1 a file, 0 a directory */
	unsigned int size;
} MlnFileInfo;

int MlnImpl_810262B4(int area, unsigned int *size, char *path);
int MlnImpl_810256EC(unsigned int *size, char *path, const char *name);
int MlnImpl_8102581C(const char *directory);
int MlnImpl_8102619C(int area, const char *path);
int MlnImpl_81026324(int area, const char *path, MlnFileInfo *info);
int MlnImpl_810259F8(int area, const char *path, int mode, void **file);
int MlnImpl_810260E4(int area, void *file, unsigned int *size);
int MlnImpl_81025C1C(int area, void *file, unsigned int size, unsigned int *read, void *data);
int MlnImpl_81025BE8(int area, void *file);
int MlnImpl_8102590C(int area, const char *path);
int MlnImpl_81025F5C(int area, void *file, unsigned int size, const void *data);
signed char MlnImpl_81026430(void);
int MlnImpl_81025E74(int area, void *file, int delimiter, unsigned int size,
		     unsigned int *read, unsigned char *data);
int MlnImpl_81025FD4(int area, void *file, int offset, int whence);
int MlnImpl_81026084(int area, void *file, int *position);
int MlnImpl_810263D4(const char *path, int *isDirectory);

/* the application data mount the client keeps its databases in */
extern ScePfsRndDriveId s_mountPoint;

typedef struct MlnSha1Context {
	unsigned int words[24];
} MlnSha1Context;

void MlnImpl_81033774(MlnSha1Context *context);
void MlnImpl_810337C4(MlnSha1Context *context, const void *data, unsigned int size);
void MlnImpl_81033864(MlnSha1Context *context, unsigned char *digest);
int MlnImpl_8102A394(void *memory, unsigned int size);
int MlnImpl_8102A418(void);
int MlnImpl_8102AA38(MlnBlob *blob, MlnBuffer *out);
int MlnImpl_810318D4(void *a0, const MlnBuffer *a1, MlnBlob *first, MlnBlob *second);

int MlnPlatformInit(void);
int MlnPlatformTerm(void);
int MlnImpl_81020094(struct MlnSession *session, int status, MlnApiValue *value,
		     MlnHttpRequest *request, MlnApiReport *report, MlnBuffer **response);
int MlnImpl_8101F954(struct MlnSession *session, MlnBuffer *message, int status, void *data,
		     MlnApiRequest *request);
int MlnImpl_8101F1F0(struct MlnClient *client, void *a1, MlnBuffer **found);
int MlnImpl_8101F0B8(struct MlnClient *client, void *a1, void *a2);
int MlnImpl_8101DFA0(struct MlnClient *client, void *a1, void *a2, int *first, int *second);
int MlnImpl_8101EC08(struct MlnClient *client, unsigned int *count, MlnBuffer ***items);
int MlnImpl_8101E118(struct MlnClient *client, MlnBuffer *a1, MlnBuffer *a2, unsigned int *count,
		     MlnBuffer ***items);
int MlnImpl_8101F4B0(struct MlnClient *client, void **handle);
int MlnImpl_8101F5F4(struct MlnClient *client, MlnBuffer *a1);
int MlnImpl_8101F724(struct MlnClient *client, MlnBuffer *token, MlnBuffer **found);
int MlnImpl_8102045C(struct MlnSession *session, int mode);
int MlnImpl_8101DD24(MlnApiKind *kind, MlnBuffer *address, struct MlnClient **client,
		     MlnApiVersion *version);
void MlnImpl_8101DF70(struct MlnClient *client);
int MlnImpl_8101F870(MlnApiKind *kind, MlnBuffer *address, struct MlnSession **session);
void MlnImpl_8101F92C(struct MlnSession *session);
int MlnImpl_8101E628(struct MlnClient *client);
int MlnImpl_8101E658(struct MlnClient *client, MlnBuffer **message, MlnApiResult **result);
int MlnImpl_8101E9EC(struct MlnClient *client, MlnApiResult *step, MlnBuffer **message);
int MlnImpl_8101E380(struct MlnClient *client, const MlnBuffer *token, MlnBuffer *account,
		     MlnBuffer *secret, int start);

/* a message of the remote client's protocol, as decoded */
typedef struct MlnMessage {
	int type;
	void *payload;
} MlnMessage;

#define MLN_MESSAGE_HTTP_GET		0x402	/* the service asks for a request sent */
#define MLN_MESSAGE_HTTP_POST		0x403
#define MLN_MESSAGE_HTTP_BODY		0x404
#define MLN_MESSAGE_VALUE		0x405
#define MLN_MESSAGE_STATUS		0x406
#define MLN_MESSAGE_LIST		0x503
#define MLN_MESSAGE_RIGHTS		0x501
#define MLN_MESSAGE_HANDLE		0x10a1
#define MLN_MESSAGE_STATE		0x10a7
#define MLN_MESSAGE_FOUND		0x10af
#define MLN_MESSAGE_TOKEN		0x10b1
#define MLN_MESSAGE_PAIR		0x505
#define MLN_MESSAGE_PAIRS		0x507
#define MLN_MESSAGE_VERSION		0x602
#define MLN_MESSAGE_RESULT		0x604
#define MLN_MESSAGE_NONE		0x1000

typedef struct MlnMessageGet {
	MlnBuffer *url;
} MlnMessageGet;

typedef struct MlnMessagePost {
	unsigned short method;		/* 0 only */
	MlnBuffer *url;
	MlnBuffer *body;
	MlnBuffer *headers;		/* "Name: value" lines, CR LF after each */
} MlnMessagePost;

/* type 0x400, the client's account: a token, or none and the account and its secret */
typedef struct MlnMessageAccount {
	unsigned short noToken;
	const MlnBuffer *token;
	MlnBuffer *account;
	MlnBuffer *secret;
} MlnMessageAccount;

typedef struct MlnMessageValue {
	unsigned short kind;		/* 1 only */
	void *data;
} MlnMessageValue;

typedef struct MlnMessageStatus {
	unsigned char reserved;
	unsigned char failed;
	unsigned char result;		/* 1: a result to fetch */
} MlnMessageStatus;

typedef struct MlnMessageResult {
	int status;			/* 0x101 a value, 0x10100 none or the error, 0x100 busy */
	int error;
	void *data;
} MlnMessageResult;

/* a list of buffer lists: each item is a pair, its first and second buffer */
typedef struct MlnMessageList {
	unsigned int count;
	MlnBuffer **buffers;
} MlnMessageList;

typedef struct MlnMessagePairs {
	unsigned int count;
	MlnMessageList **items;
} MlnMessagePairs;

#define MLN_RESULT_VALUE	0x101
#define MLN_RESULT_BUSY		0x100
#define MLN_RESULT_NONE		0x10100

/* a remote client: the engine runs in another process, reached through the kind's transport */
typedef struct MlnClient {
	MlnApiKind *kind;
	unsigned short build;		/* the client's build, from its version */
	int busy;			/* an action token runs */
	MlnBuffer *account;
	MlnBuffer *secret;
	MlnMessage pending;		/* the request the client asked for, MLN_MESSAGE_NONE when none */
} MlnClient;

/*
 * The other end: the process that runs the engine for a remote client, decoding each of its
 * messages into a request for the engine.
 */
typedef struct MlnSession {
	MlnApiKind *kind;
	MlnBuffer *address;
	int mode;
	unsigned int allowed;		/* the requests the client may send now, 1 when none */
} MlnSession;

int MlnImpl_810222A0(const MlnBuffer *address, MlnBuffer **request);
int MlnImpl_81022A1C(MlnBuffer **request);
int MlnImpl_81022478(MlnBuffer *first, MlnBuffer *second, MlnBuffer **request);
int MlnImpl_81022570(MlnBuffer *first, MlnBuffer *second, MlnBuffer **request);
int MlnImpl_81021734(int noToken, const MlnBuffer *token, MlnBuffer *account, MlnBuffer *secret,
		     MlnBuffer **request);
int MlnImpl_810217E4(int type, MlnBuffer **request);
int MlnImpl_81021BAC(MlnBuffer *response, MlnBuffer **request);
int MlnImpl_81021864(const char *url, MlnBuffer **request);
int MlnImpl_8102190C(int reserved, const char *url, MlnBuffer *body, MlnHttpHeaders *headers,
		     MlnBuffer **request);
int MlnImpl_81022180(MlnBuffer **request);
int MlnImpl_81021D70(MlnBuffer *license, MlnBuffer **request);
int MlnImpl_81022C10(MlnBuffer *first, MlnBuffer *second, MlnBuffer **request);
int MlnImpl_81022D54(void *arg, MlnBuffer **request);
int MlnImpl_810228FC(MlnBuffer **request);
int MlnImpl_81022EFC(MlnBuffer **request);
int MlnImpl_810230AC(MlnBuffer *arg, MlnBuffer **request);
int MlnImpl_810231D4(MlnBuffer *token, MlnBuffer **request);
int MlnImpl_8102371C(const MlnBuffer *reply, MlnMessage *message);
int MlnImpl_81022C98(int status, int result, MlnBuffer **reply);
int MlnImpl_81022B3C(int status, int error, void *data, MlnBuffer **reply);
int MlnImpl_81021CD4(int kind, void *data, MlnBuffer **message);
int MlnImpl_810223C8(int major, int minor, int build, MlnBuffer *name, MlnBuffer *id,
		     MlnBuffer **message);
int MlnImpl_81022500(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_81021F74(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_81021DE8(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_8102285C(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_81022E7C(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_81022608(MlnApiReportData *data, MlnBuffer **message);
int MlnImpl_8102302C(MlnBuffer *buffer, MlnBuffer **message);
int MlnImpl_810232FC(MlnApiReportData *data, MlnBuffer **message);
void MlnImpl_8102337C(unsigned int count, MlnBuffer **buffers);
void MlnImpl_810233C4(MlnMessage *message);
/* the list and pair-list fields, in another file */
int MlnImpl_8108150C(const unsigned char *p, unsigned int left, void **payload);
void MlnImpl_81020470(unsigned char **cursor, unsigned int count, MlnBuffer **buffers);
void MlnImpl_810205A4(unsigned char *p, MlnMessagePairs *pairs);
unsigned int MlnImpl_810206C0(const MlnBufferGroup *group);
int MlnImpl_81020B20(unsigned char *p, const MlnMessage *message);
int MlnImpl_81021640(const MlnMessage *message, MlnBuffer **out);
int MlnImpl_81020784(const MlnMessage *message, unsigned int *size);
int MlnImpl_8101D5D4(MlnClient *client, MlnApiValue **value);
int MlnImpl_8101D7C0(MlnMessage *message, MlnApiResult **out);
int MlnImpl_8101DC14(MlnClient *client, int failed, int result, MlnApiResult **out);
int MlnImpl_8101DCB4(MlnClient *client, const MlnMessageStatus *status);

int MlnXmlParse(int heap, unsigned int size, const void *xml, MlnXmlNode **document);
int MlnXmlFindNode(int heap, MlnXmlNode *node, const char *path, const char *ns, MlnXmlNode **found);
int MlnXmlFindNodes(int heap, MlnXmlNode *node, const char *path, const char *ns,
		    MlnXmlNodeList **out);
int MlnXmlGetAttribute(int heap, MlnXmlNode *node, const char *ns, const char *name,
		       MlnBuffer **value);
int MlnXmlGetText(int heap, MlnXmlNode *node, MlnBuffer **text);
int MlnXmlNodeFree(int heap, MlnXmlNode *node);
int MlnXmlListFree(int heap, MlnXmlNodeList *list);

/*
 * A list in an action token or a service configuration: kind 1 holds buffers, 2 and 3
 * pairs of buffers, 4 and 5 entries with lists of their own.
 */
typedef struct MlnConfigList {
	unsigned short kind;
	unsigned short count;
	void *items;
} MlnConfigList;

/* the kinds of action an action token asks for */
#define MLN_ACTION_LICENSE_ACQUISITION	1
#define MLN_ACTION_NODE_ACQUISITION	2
#define MLN_ACTION_LINK_ACQUISITION	3
#define MLN_ACTION_DEREGISTRATION	4
#define MLN_ACTION_GENERIC		5	/* an Action element, typed by its xsi:type */

typedef struct MlnAction {
	int type;
	MlnBuffer *id;
	MlnBuffer *businessToken;
	MlnBuffer *uid[2];
} MlnAction;

/* a Marlin Broadband action token (urn:marlin:broadband:1-2:nemo:services:action-token) */
typedef struct MlnActionToken {
	MlnConfigList resourceLocations;
	MlnBuffer *broadbandServiceId;
	MlnBuffer *configVersion;
	int actionCount;
	MlnAction **actions;
} MlnActionToken;

/* a service of a configuration: its node, the keys of its protocol, its roles, its address */
typedef struct MlnServiceEndpoint {
	MlnBuffer *id;
	MlnBuffer *nodeId;
	MlnBuffer *requestEncryptionKey;
	MlnBuffer *responseSigningKey;
	MlnConfigList roles;
	char *location;
	unsigned int reserved[2];
} MlnServiceEndpoint;

/* a service's BroadbandServiceConfig, kept from an earlier action token */
typedef struct MlnServiceConfig {
	MlnBuffer *broadbandServiceId;
	MlnBuffer *broadbandServiceFriendlyName;
	MlnBuffer *configVersion;
	MlnConfigList registrationService;
	MlnConfigList licenseService;
	unsigned char reserved[16];
	MlnConfigList nemoPersonalizationService;
	MlnConfigList octopusPersonalizationService;
	MlnConfigList licenseTransferService;
	unsigned int reserved2;
} MlnServiceConfig;

/* a directory's files, as the platform lists them */
typedef struct MlnFileList {
	struct MlnFileList *next;
	char *path;
} MlnFileList;

typedef struct MlnBufferPairList {
	unsigned char count;
	MlnBufferPair **items;
} MlnBufferPairList;

/* how far an engine is: the steps done, of all the token's */
typedef struct MlnEngineProgress {
	unsigned int done;
	unsigned int total;
} MlnEngineProgress;

/* the engine: runs an action token's actions with the service, step by step */
typedef struct MlnEngine {
	int kind;
	int state;
	MlnServiceSession session;
	MlnServiceContext localContext;		/* the context when the caller gives none */
	MlnServiceContext *context;
	MlnBuffer *account;
	MlnBuffer *secret;
	MlnActionToken *token;
	MlnServiceConfig *config;
	unsigned int action;			/* the token's action it runs */
	unsigned int resourceIndex;
	unsigned int resourceCount;
	char **resources;
	MlnBufferPairList *extensions;
	MlnEngineProgress progress;
} MlnEngine;

void MlnEngineFree(MlnEngine *engine);
int MlnImpl_810199E4(MlnEngine *engine);
int MlnEngineDestroy(MlnEngine *engine);
int MlnEngineStep(MlnEngine *engine, MlnBuffer **message, MlnApiResult **out);
int MlnImpl_8101A360(MlnEngine *engine, MlnApiResult **out);
int MlnImpl_8101A59C(MlnBuffer **out);
int MlnEngineCreate(int kind, const MlnBuffer *token, const MlnBuffer *account,
		    const MlnBuffer *secret, MlnServiceContext *context, MlnEngine **out);
/* not written in C yet */
int MlnActionTokenParse(unsigned int size, const void *xml, MlnActionToken **out);
int MlnActionTokenFree(MlnActionToken *token);
int MlnServiceConfigParse(unsigned int size, const void *xml, MlnServiceConfig **out);
int MlnServiceConfigFree(MlnServiceConfig *config);
int MlnImpl_81026438(const char *directory);
int MlnImpl_810265E8(int heap, const char *directory, MlnFileList **files);
int MlnImpl_810257DC(int heap, MlnFileList *files);
int MlnImpl_8101ACD0(MlnEngine *engine, MlnActionToken *token, MlnApiResult *result);
int MlnImpl_8101D468(const MlnBuffer *serviceId, int *exists);
int MlnImpl_8101D408(const MlnBuffer *serviceId, MlnBuffer **config);
int MlnImpl_8101D520(const MlnBuffer *serviceId);
int MlnImpl_8101D29C(const MlnBuffer *serviceId, const MlnBuffer *config);
int MlnImpl_8101D5B4(const char *path, MlnBuffer **config);
int MlnImpl_8101CECC(int create, const MlnBuffer *serviceId, char *path);
int MlnImpl_8101D020(const char *path, MlnBuffer **config);
int MlnImpl_81035018(unsigned int firstCount, MlnBuffer **first, unsigned int secondCount,
		     MlnBuffer **second, MlnBuffer *third, MlnBuffer *fourth, char **text);
int MlnImpl_810353A8(char **text);
int MlnImpl_8101AF30(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101B244(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101B324(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101B444(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101B850(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101BC60(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101BD9C(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101BE94(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101BFEC(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101C0F4(MlnEngine *engine, MlnBuffer **message, MlnApiResult *result, int hasValue);
int MlnImpl_8101B158(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101CA44(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101C880(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101B7D8(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101C6B4(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101BBE4(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101C500(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101CC94(MlnEngine *engine, MlnApiResult *result);
int MlnImpl_8101AD74(MlnEngine *engine, MlnApiResult *result, MlnBuffer **message);
int MlnImpl_8101C1E8(MlnEngine *engine, int service, char **nodeId, char **location);
int MlnImpl_8101CBF0(char *url, int method, MlnBuffer *body, MlnHttpRequest **out);

#endif /* MLN_INTERNAL_H */
