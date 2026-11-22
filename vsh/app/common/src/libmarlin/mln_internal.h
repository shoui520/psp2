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
#define MLN_XML_ERROR_NODE		0x10010001	/* a node of a type the call does not take */
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

/* the parser's state; the writer uses it too, with its flags */
#define MLN_XML_WRITE_EMPTY_TAGS	1	/* an element without content as <name/> */

typedef struct MlnXmlParser {
	MlnXmlStack namespaces;
	unsigned int flags;
	unsigned int reserved[3];
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

/* the Octopus nodes and the links between them the engine lists, by node ID */
typedef struct MlnApiOctopusLink {
	MlnBuffer *from;
	struct MlnApiOctopusLink *next;
	MlnBuffer *to;
} MlnApiOctopusLink;

typedef struct MlnApiOctopusNode {
	struct MlnApiOctopusNode *next;
	MlnApiOctopusLink *links;
	MlnBuffer *id;
} MlnApiOctopusNode;

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
int MlnNodesCollectReachable(MlnApiOctopusNode *nodes, const MlnBuffer *id, MlnIdSet **set);

/* a word, three buffers and a counted list of buffer triples (string.c frees it) */
typedef struct MlnBufferGroup {
	unsigned int tag;
	MlnBuffer *first;
	MlnBuffer *second;
	MlnBuffer *third;
	MlnBuffer ***entries;
	unsigned int entryCount;
} MlnBufferGroup;

/* SceDrmBridgeUser's calls (the SDK's stub library) */
int sceDrmBridgeIsAllowRemotePlayDebug(void);
int SceDrmBridgeUser_6F5DC6CD(void);
int sceDrmBridgeGetCurrentSecureTick(unsigned long long *tick);
int sceDrmBridgeMlnpsnlAuth1(void *input, void *tick, void *output, void *state);
int sceDrmBridgeMlnpsnlAuth2(const void *input, void *state, void *key);

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
/* the library's shared state: buffers (the general heap's term frees the first 8) and the
   core's init count */
typedef struct MlnShared {
	void *buffers[11];
	int coreRefCount;
} MlnShared;

MlnShared *MlnImpl_81006EAC(void);
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

/* xmlwrite.c */
int MlnXmlWriteText(int heap, const MlnBuffer *text, MlnTextBuffer *out);
int MlnXmlWriteElement(int heap, MlnXmlParser *writer, const MlnXmlElement *element,
		       const MlnXmlElement *except, MlnTextBuffer *out);
int MlnXmlWriteNode(int heap, MlnXmlParser *writer, const MlnXmlNode *node,
		    const MlnXmlElement *except, MlnBuffer **out);
int MlnXmlNodeToXml(int heap, const MlnXmlNode *node, MlnBuffer **out);
/* a node as XML text, an element without content as <name/> */
int MlnXmlNodeToText(int heap, const MlnXmlNode *node, MlnBuffer **text);

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
int MlnImpl_81012F48(const MlnBuffer *state, const MlnBuffer *request, MlnBuffer **first,
		     MlnBuffer **second);
int MlnImpl_81013074(const MlnBuffer *state, const MlnBuffer *request, MlnBlob *response,
		     MlnBlob *result);
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
/* a response's values, each a 16-byte header and its data, handed to a visitor */
typedef int (*MlnServiceValueVisitor)(int heap, const unsigned char *value, void *context);
/* the arguments, one by one, handed to a visitor */
typedef int (*MlnServiceArgVisitor)(const MlnServiceArg *arg, void *context);

/* the request writer's place in the buffer it fills */
typedef struct MlnServiceWriter {
	unsigned int written;
	unsigned int size;
	unsigned char *data;
} MlnServiceWriter;

/* MLN_SERVICE_ARG_OBJECT: a buffer between two words */
typedef struct MlnServiceObject {
	unsigned int first;
	MlnBuffer *buffer;
	unsigned int second;
} MlnServiceObject;

/* MLN_SERVICE_ARG_LIST: named values, each with a flag byte */
typedef struct MlnServiceListEntry {
	MlnBuffer *name;
	MlnBuffer *value;
	unsigned char flag;
} MlnServiceListEntry;

typedef struct MlnServiceList {
	unsigned char count;
	MlnServiceListEntry **entries;
} MlnServiceList;

/* MLN_SERVICE_ARG_STRUCT: named entries, each with a counted list of buffer pairs */
typedef struct MlnServicePair {
	MlnBuffer *first;
	MlnBuffer *second;
} MlnServicePair;

typedef struct MlnServiceStructEntry {
	MlnBuffer *name;
	unsigned int pairCount;
	MlnServicePair *pairs;
} MlnServiceStructEntry;

typedef struct MlnServiceStruct {
	unsigned char count;
	MlnServiceStructEntry **entries;
} MlnServiceStruct;

/* MLN_SERVICE_ARG_RECORD: a buffer and, by its kind, 0, 3 or 12 more bytes */
#define MLN_SERVICE_RECORD_PLAIN	0
#define MLN_SERVICE_RECORD_LONG		1	/* head[0..3] and tail */
#define MLN_SERVICE_RECORD_SHORT	2	/* head[0..2] */

typedef struct MlnServiceRecordValue {
	MlnBuffer *buffer;
	unsigned int kind;
	unsigned char head[4];
	unsigned char tail[8];
} MlnServiceRecordValue;

/* the response reader's place in the values it fills */
typedef struct MlnServiceReader {
	unsigned int index;
	MlnServiceArg *items;
} MlnServiceReader;

int MlnImpl_81026DE0(int heap, const MlnBuffer *response, MlnServiceValueVisitor visit,
		     void *context);
int MlnImpl_81026FAC(int heap, const MlnServiceArg *value, int *out);
int MlnImpl_8102704C(int heap, const MlnServiceArg *value, unsigned int *out);
int MlnImpl_810270F4(int heap, const MlnServiceArg *value, void *out);
int MlnImpl_81027140(int heap, MlnServiceArg *value, void **out);
int MlnImpl_8102717C(int heap, MlnServiceArg *value, void **out);
int MlnImpl_810271B8(int heap, MlnServiceArg *value, void **out);
int MlnImpl_810271F4(int heap, MlnServiceArg *value, void **out);
int MlnImpl_81027B50(int heap, const MlnBuffer *response, MlnServiceValueVisitor visit,
		     void *context);
int MlnImpl_81027C00(int heap, const unsigned char *value, void *context);
int MlnImpl_81027C1C(int heap, const unsigned char *value, void *context);
void MlnImpl_81027230(int heap, unsigned int count, MlnServiceArg *items);
int MlnImpl_8102766C(const MlnServiceArg *arg, unsigned int *size);
int MlnImpl_810278C8(const unsigned char **cursor, unsigned int size, void *out,
		     unsigned int *remaining);
int MlnImpl_81027934(unsigned char **cursor, const MlnBuffer *buffer);
int MlnImpl_81027A1C(int heap, const unsigned char **cursor, MlnBuffer **out,
		     unsigned int *remaining);
int MlnImpl_81028818(const MlnServiceArgs *args, MlnServiceArgVisitor visit, void *context);
int MlnImpl_81028880(const MlnServiceArg *arg, void *context);
int MlnImpl_810288D0(const MlnServiceArg *arg, void *context);
void MlnImpl_8102913C(int heap, MlnServiceStruct *value);
void MlnImpl_81029234(int heap, MlnServiceList *list);
void MlnImpl_810292C8(int heap, MlnBufferGroup *group);
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
	unsigned int state[5];
	unsigned int count[2];		/* the message's length in bits, low word first */
	unsigned char buffer[64];
	unsigned int padding;		/* the 96 bytes the callers' frames give it */
} MlnSha1Context;

void MlnImpl_81032404(unsigned int state[5], unsigned char buffer[64]);
void MlnImpl_81033774(MlnSha1Context *context);
void MlnImpl_810337C4(MlnSha1Context *context, const void *data, unsigned int size);
void MlnImpl_81033864(MlnSha1Context *context, unsigned char *digest);
int MlnImpl_8102A394(void *memory, unsigned int size);
int MlnImpl_8102A418(void);

/* octopus.c: the DRM core's calls */
/* a box: its size with the 8-byte header, its four-character type and its payload */
typedef struct MlnBox {
	unsigned int size;
	unsigned char type[4];
	unsigned char *data;
} MlnBox;

/* an attribute box: a box whose payload is a typed value of a number of bits */
typedef struct MlnAttributeBox {
	unsigned int size;
	unsigned char type[4];
	unsigned char flags[2];
	unsigned short encoding;
	unsigned short count;
	unsigned short bits;
	unsigned char *data;
} MlnAttributeBox;

/* the XML namespaces the Octopus objects are read from */
#define MLN_OCTOPUS_NAMESPACE	"http://www.octopus-drm.com/profiles/base/1.0"
#define MLN_PLANKTON_NAMESPACE	"http://www.octopus-drm.com/profiles/base/1.0/plankton"
#define MLN_XMLDSIG_NAMESPACE	"http://www.w3.org/2000/09/xmldsig#"
#define MLN_SAML_NAMESPACE	"urn:oasis:names:tc:SAML:1.0:assertion"

/* an Octopus object or token that is not as its schema says */
#define MLN_OCTOPUS_ERROR_FORMAT	0x230F0002

/*
 * Octopus objects (http://www.octopus-drm.com/profiles/base/1.0), as parsed
 * from their XML. An array of objects is an MlnOctopusArray; what it holds is
 * named by its kind when it is freed (MlnImpl_8103C57C).
 */
#define MLN_OCTOPUS_PROTECTOR		1
#define MLN_OCTOPUS_CONTENT_KEY		2
#define MLN_OCTOPUS_CONTROLLER		3
#define MLN_OCTOPUS_CONTROL		4
#define MLN_OCTOPUS_LINK		5
#define MLN_OCTOPUS_NODE		6
#define MLN_OCTOPUS_REFERENCE		7
#define MLN_OCTOPUS_ATTRIBUTE		8
#define MLN_OCTOPUS_EXTENSION		9
#define MLN_OCTOPUS_KEY			11
#define MLN_OCTOPUS_KEY_PAIR		12
#define MLN_OCTOPUS_BUFFER		13
#define MLN_OCTOPUS_SAML_ATTRIBUTE	14
#define MLN_OCTOPUS_PLAIN		15	/* nothing of its own to free */

typedef struct MlnOctopusArray {
	int count;
	void *items[];
} MlnOctopusArray;

/* an array's items compared, and one written, for its canonical byte sequence */
typedef int (*MlnCanonicalCompare)(const void *a, const void *b);
typedef int (*MlnCanonicalWrite)(const void *item, MlnTextBuffer *out);

int MlnImpl_8105F3AC(const MlnOctopusArray *array, MlnCanonicalCompare compare,
		     MlnCanonicalWrite write, MlnTextBuffer *out);

/* an Attribute's type attribute */
#define MLN_OCTOPUS_ATTRIBUTE_INT	0
#define MLN_OCTOPUS_ATTRIBUTE_STRING	1
#define MLN_OCTOPUS_ATTRIBUTE_BYTES	2
#define MLN_OCTOPUS_ATTRIBUTE_LIST	3
#define MLN_OCTOPUS_ATTRIBUTE_ARRAY	4

typedef struct MlnOctopusAttribute {
	MlnBuffer *name;
	int type;
	union {
		int integer;
		MlnBuffer *string;
		MlnBuffer *bytes;
		MlnOctopusArray *attributes;	/* LIST and ARRAY */
	} value;
} MlnOctopusAttribute;

/* a reference to another object, with the digest of it */
typedef struct MlnOctopusReference {
	MlnBuffer *uid;
	MlnOctopusArray *transforms;	/* buffers: the transform algorithms */
	MlnBuffer *digestMethod;
	MlnBuffer *digestValue;
} MlnOctopusReference;

/* a key of ScubaKeys or a ContentKey: KeyData, as given or xmlenc-encrypted */
typedef struct MlnOctopusKey {
	MlnBuffer *uid;
	unsigned char usage;		/* 0 KeySharing, 1 Confidentiality, 2 none */
	unsigned char format;		/* 0 PKCS#8, 1 X509SPKI, 2 RAW */
	unsigned char encryptionMethod;	/* the EncryptedData's; 2 when given plain */
	MlnBuffer *oaepParams;
	unsigned char keyEncryptionMethod;	/* the EncryptedKey's, 0 without one */
	MlnBuffer *keyName;
	MlnBuffer *encryptedKey;
	MlnBuffer *data;		/* the KeyData or the CipherValue */
	void *key;			/* the key loaded from data (MlnImpl_81080C80 frees it) */
	int state;
} MlnOctopusKey;

/* a PublicKey or a PrivateKey: a key and the uid of its pair */
typedef struct MlnOctopusKeyPair {
	MlnOctopusKey key;
	MlnBuffer *pair;
} MlnOctopusKeyPair;

typedef struct MlnOctopusScubaKeys {
	MlnOctopusArray *secretKeys;	/* MLN_OCTOPUS_KEY */
	MlnOctopusArray *privateKeys;	/* MLN_OCTOPUS_KEY_PAIR */
	MlnOctopusArray *publicKeys;	/* MLN_OCTOPUS_KEY_PAIR */
} MlnOctopusScubaKeys;

/* an Extension; type 1 carries ScubaKeys */
#define MLN_OCTOPUS_EXTENSION_DIGEST	0
#define MLN_OCTOPUS_EXTENSION_SCUBA	1

typedef struct MlnOctopusExtension {
	int type;
	MlnBuffer *uid;
	int critical;
	MlnOctopusArray *transforms;	/* the Digest's */
	MlnBuffer *digestMethod;
	MlnBuffer *digestValue;
	MlnBuffer *subject;
	MlnOctopusScubaKeys *scubaKeys;
} MlnOctopusExtension;

typedef struct MlnOctopusNode {
	MlnBuffer *uid;
	MlnOctopusArray *attributes;
	MlnOctopusArray *extensions;
	MlnOctopusExtension *scubaKeys;	/* the extension with the node's ScubaKeys */
} MlnOctopusNode;

typedef struct MlnOctopusControl {
	MlnBuffer *uid;
	MlnOctopusArray *attributes;
	MlnOctopusArray *extensions;
	MlnBuffer *protocol;
	MlnBuffer *type;		/* the CodeModule's */
	MlnBuffer *codeModule;
	MlnBuffer *signer;		/* who signed the link that carries it */
} MlnOctopusControl;

typedef struct MlnOctopusLink {
	MlnBuffer *uid;
	MlnBuffer *from;
	MlnBuffer *to;
	MlnOctopusArray *attributes;
	MlnOctopusArray *extensions;
	MlnOctopusControl *control;
} MlnOctopusLink;

typedef struct MlnOctopusProtector {
	MlnBuffer *uid;
	MlnOctopusReference *contentKey;
	MlnOctopusArray *protectedTargets;	/* MLN_OCTOPUS_REFERENCE */
} MlnOctopusProtector;

typedef struct MlnOctopusContentKey {
	MlnBuffer *uid;
	MlnOctopusKey *secretKey;
	MlnOctopusArray *attributes;
	MlnOctopusArray *extensions;
} MlnOctopusContentKey;

typedef struct MlnOctopusController {
	MlnBuffer *uid;
	MlnOctopusReference *control;
	MlnOctopusArray *controlledTargets;	/* MLN_OCTOPUS_REFERENCE */
	MlnOctopusArray *attributes;
	MlnOctopusArray *extensions;
} MlnOctopusController;

/* a SAML AttributeStatement's Attribute, its name "AttributeNamespace:AttributeName" */
#define MLN_OCTOPUS_SAML_INTEGER	0	/* xsd:nonNegativeInteger */
#define MLN_OCTOPUS_SAML_STRING		1
#define MLN_OCTOPUS_SAML_EMPTY		2	/* an AttributeValue without text */

typedef struct MlnOctopusSamlAttribute {
	MlnBuffer *name;
	int type;
	union {
		unsigned int integer;
		MlnBuffer *string;
	} value;
} MlnOctopusSamlAttribute;

/* a SAML 1.0 Assertion */
typedef struct MlnOctopusSamlAssertion {
	int reserved;
	MlnBuffer *assertionId;
	MlnDateTime issueInstant;
	MlnDateTime *notBefore;		/* Conditions, or NULL */
	MlnDateTime *notOnOrAfter;
	MlnOctopusArray *attributes;	/* MLN_OCTOPUS_SAML_ATTRIBUTE */
	MlnBuffer *nameIdentifier;	/* AttributeStatement/Subject */
} MlnOctopusSamlAssertion;

/* a bundle's starfish BroadcastKeyBlock, both base64-decoded */
typedef struct MlnOctopusBroadcastKeyBlock {
	MlnBuffer *revocationInformation;
	MlnBuffer *encryptedBroadcastKeys;
} MlnOctopusBroadcastKeyBlock;

/* a /Bundle/Extension and the bundle's Control it is about */
typedef struct MlnOctopusBundleExtension {
	MlnOctopusExtension *extension;
	MlnOctopusControl *control;
} MlnOctopusBundleExtension;

/* an Octopus /Bundle */
typedef struct MlnOctopusBundle {
	MlnOctopusControl *control;
	MlnOctopusController *controller;
	int contentKeyCount;			/* -1 until a list gives it */
	MlnOctopusArray *contentKeys;		/* MLN_OCTOPUS_CONTENT_KEY */
	MlnOctopusBroadcastKeyBlock *broadcastKeyBlock;
	MlnOctopusArray *protectors;		/* MLN_OCTOPUS_PROTECTOR */
	MlnOctopusBundleExtension *extension;
	MlnOctopusArray *attributes;
} MlnOctopusBundle;

/*
 * an XML signature (http://www.w3.org/2000/09/xmldsig#) as MlnImpl_81059934
 * reads it, and one of its SignedInfo's References
 */
typedef struct MlnXmlDsigReference {
	MlnBuffer *uri;
	int transformCount;
	MlnBuffer **transforms;		/* the Transform algorithms */
	MlnBuffer *digestMethod;
	MlnBuffer *digestValue;
} MlnXmlDsigReference;

#define MLN_XMLDSIG_KEYINFO_KEYNAME	0
#define MLN_XMLDSIG_KEYINFO_X509	1	/* X509Data/X509Certificate */
#define MLN_XMLDSIG_KEYINFO_X509SKI	2	/* X509Data/X509SKI, also in a SecurityTokenReference */

typedef struct MlnXmlDsigSignature {
	MlnBuffer *id;
	MlnBuffer *canonicalizationMethod;
	MlnBuffer *signatureMethod;
	int referenceCount;
	MlnXmlDsigReference *references;
	MlnBuffer *signedInfo;		/* the SignedInfo as XML */
	MlnBuffer *signatureValue;
	int keyInfoType;
	union {
		MlnBuffer *keyName;	/* or the X509SKI */
		struct {
			int count;
			MlnBuffer **certificates;
		} x509;
	} keyInfo;
} MlnXmlDsigSignature;

/* a buffer, two lists of buffers and another buffer (freed by MlnImpl_81042A88) */
typedef struct MlnBufferGroups {
	MlnBuffer *first;
	struct {
		int count;
		MlnBuffer **items;
	} lists[2];
	MlnBuffer *last;
} MlnBufferGroups;

/* a bundle and, for each of its content keys, the SignedInfo and signature over it */
typedef struct MlnOctopusBundleSignature {
	MlnBuffer *signedInfo;
	MlnBuffer *signature;
} MlnOctopusBundleSignature;

typedef struct MlnOctopusSignedBundle {
	MlnOctopusBundle *bundle;
	MlnOctopusBundleSignature *signatures;
} MlnOctopusSignedBundle;

/* a Plankton value, of a Parameter's or a ValueList's ValueBlock */
#define MLN_PLANKTON_INTEGER		1
#define MLN_PLANKTON_REAL		2
#define MLN_PLANKTON_STRING		3
#define MLN_PLANKTON_DATE		4
#define MLN_PLANKTON_PARAMETER		5	/* a ParameterBlock */
#define MLN_PLANKTON_RESOURCE		7
#define MLN_PLANKTON_VALUE_LIST		8
#define MLN_PLANKTON_BYTE_ARRAY		9

struct MlnPlanktonValueList;
struct MlnPlanktonParameterBlock;

typedef struct MlnPlanktonValue {
	int type;
	union {
		int integer;				/* INTEGER, DATE */
		MlnBuffer *buffer;			/* STRING, RESOURCE, BYTE_ARRAY */
		struct MlnPlanktonParameterBlock *parameters;
		struct MlnPlanktonValueList *list;
	} value;
} MlnPlanktonValue;

/* a Parameter (its name) or a ValueList (no name): its values */
typedef struct MlnPlanktonValueList {
	MlnBuffer *name;
	unsigned int count;
	MlnPlanktonValue *values;
} MlnPlanktonValueList;

typedef struct MlnPlanktonParameterBlock {
	unsigned int count;
	MlnPlanktonValueList *parameters;
} MlnPlanktonParameterBlock;

/*
 * a stored record: a header of a big-endian tag, item count and id, and the
 * items, a list of buffers
 */
typedef struct MlnRecordHeader {
	unsigned int tag;
	int count;
	MlnBuffer *id;
} MlnRecordHeader;

typedef struct MlnRecordItem {
	struct MlnRecordItem *previous;
	struct MlnRecordItem *next;
	MlnBuffer *data;
} MlnRecordItem;

typedef struct MlnRecord {
	MlnRecordHeader *header;
	MlnRecordItem *first;
	MlnRecordItem *current;
	MlnRecordItem *last;
	int count;
} MlnRecord;

typedef struct MlnRecordArray {
	int count;
	MlnRecord **records;
} MlnRecordArray;

int MlnImpl_8103B4A0(const MlnBuffer *data, MlnRecord **record, unsigned int *size);
MlnOctopusArray *MlnImpl_8103B758(int count);
int MlnImpl_8103B7A4(MlnOctopusKey *key);
int MlnImpl_8103B808(MlnOctopusKeyPair *pair);
int MlnImpl_8103B878(MlnOctopusSamlAttribute *attribute);
int MlnImpl_8103B8C0(MlnOctopusBroadcastKeyBlock *block);
int MlnImpl_8103BA60(MlnOctopusAttribute *attribute);
int MlnImpl_8103BBA0(MlnOctopusLink *link);
int MlnImpl_8103BD20(MlnOctopusProtector *protector);
int MlnImpl_8103BE9C(MlnOctopusController *controller);
int MlnImpl_8103C028(MlnOctopusReference *reference);
int MlnImpl_8103C0B8(MlnOctopusExtension *extension);
int MlnImpl_8103C16C(MlnOctopusContentKey *contentKey);
int MlnImpl_8103C284(MlnOctopusControl *control);
int MlnImpl_8103C370(MlnOctopusScubaKeys *keys);
int MlnImpl_8103C57C(MlnOctopusArray *array, int kind);
MlnOctopusBundle *MlnImpl_8103CAA4(void);
int MlnImpl_8103CAD8(MlnOctopusBundle *bundle);
int MlnImpl_8103CC38(MlnOctopusBundleExtension *extension);
int MlnImpl_8103CD0C(MlnOctopusSamlAssertion *assertion);
int MlnImpl_8103CDE0(MlnOctopusKey *key, MlnBuffer *uid, int usage, int format,
		     int encryptionMethod, MlnBuffer *oaepParams, MlnBuffer *keyName,
		     int keyEncryptionMethod, MlnBuffer *encryptedKey, MlnBuffer *data);
int MlnImpl_8103CE44(MlnOctopusKeyPair *pair, MlnBuffer *keyPair, MlnBuffer *uid, int usage,
		     int format, int encryptionMethod, MlnBuffer *oaepParams,
		     MlnBuffer *keyName, int keyEncryptionMethod, MlnBuffer *encryptedKey,
		     MlnBuffer *data);
void MlnImpl_8103CEB0(MlnPlanktonValueList *list);
void MlnImpl_8103CFC0(MlnPlanktonParameterBlock *block);
void MlnImpl_8103D01C(MlnPlanktonValue *value);
int MlnImpl_8103D0DC(const MlnRecordArray *array, MlnBuffer **out);
int MlnImpl_8103D30C(const MlnBuffer *data, MlnRecordArray *array);
int MlnImpl_8103D4AC(MlnBuffer *data, const MlnBuffer *id, MlnRecord **out);
void MlnImpl_8103D5B8(MlnRecord *record);
int MlnImpl_8103D628(MlnXmlNode *node, MlnOctopusArray **transforms, MlnBuffer **digestMethod,
		     MlnBuffer **digestValue);
int MlnImpl_8103D890(MlnXmlNode *node, unsigned int anonymous, MlnOctopusAttribute **out);
int MlnImpl_8103DCBC(MlnXmlNode *node, MlnOctopusArray **out);
int MlnImpl_8103DDB0(MlnXmlNode *node, int pair, void **out);
int MlnImpl_81082230(MlnXmlNode *node, MlnBuffer **encryptionMethod, MlnBuffer **oaepParams,
		     MlnBuffer **keyName, MlnBuffer **keyEncryptionMethod, MlnBuffer **encryptedKey,
		     MlnBuffer **cipherValue);
int MlnImpl_8103E4FC(const void *data, unsigned int size, unsigned char *digest,
		     int digestSize);
int MlnImpl_810570A8(const void *data, unsigned int size, unsigned char *digest);
int MlnImpl_8105713C(const void *data, unsigned int size, unsigned char *digest);
int MlnImpl_81057024(void **context);
int MlnImpl_81057078(void *context, unsigned char *digest);
int MlnImpl_81057098(void *context, const void *data, unsigned int size);
int MlnImpl_810570B8(void **context);
int MlnImpl_8105710C(void *context, unsigned char *digest);
int MlnImpl_8105712C(void *context, const void *data, unsigned int size);
int MlnImpl_8105714C(void **context);
int MlnImpl_810571C4(void *context, unsigned int size, void *out);
int MlnImpl_81057244(void *context);
int MlnImpl_8103EF84(const MlnOctopusArray *transforms);
int MlnImpl_8103E540(const MlnBuffer *token, int type, MlnOctopusSamlAssertion **out,
		     int verify);
int MlnImpl_8105BF78(MlnXmlNode *signature, int *digestSize);
int MlnImpl_8103FF44(MlnXmlNode *node, MlnOctopusControl **out);
int MlnImpl_8103F6F0(const MlnBuffer *token, MlnOctopusNode **out, int type, int verify);
int MlnImpl_81061330(const MlnOctopusScubaKeys *keys, MlnBuffer **out);
int MlnImpl_810611BC(const MlnOctopusNode *node, MlnBuffer **out);
int MlnImpl_81059558(MlnXmlNode *node, MlnXmlNode *transform, MlnBuffer **out);
int MlnImpl_8105BA14(MlnXmlNode *signature, int usage, int count, MlnBuffer *const *references,
		     unsigned char **digests, int *digestTypes, const MlnBuffer *certificate,
		     int *verified);
int MlnImpl_8105B568(MlnXmlNode *signature, int usage, int count, MlnBuffer *const *references,
		     unsigned char **digests, int *digestTypes, const MlnBuffer *certificate,
		     MlnBuffer **signer, int *verified);
int MlnImpl_81059934(MlnXmlNode *node, MlnXmlDsigSignature **out);
int MlnImpl_8105D018(MlnXmlDsigReference *reference);
int MlnImpl_8105A3D8(MlnXmlNode *node, MlnXmlNode *document, MlnXmlDsigSignature **out);
int MlnImpl_8105C234(MlnXmlNode *signedInfo, const MlnBuffer *signatureValue, int keyInfo,
		     MlnBuffer **certificates, const char *id, MlnXmlNode **out);
int MlnImpl_8105C7F0(int count, int hash, const int *transforms, const char *const *uris,
		     unsigned char **digests, MlnXmlNode **out);
int MlnImpl_8105CF28(int count, int hash, const int *transforms, const char *const *uris,
		     unsigned char **digests, void *rsa, int keyInfo, MlnBuffer **certificates,
		     const char *id, MlnXmlNode **out);
int MlnImpl_8105BA50(MlnXmlNode *signature, MlnXmlNode *document, int usage, int count,
		     MlnBuffer *const *references, unsigned char **digests, int *digestTypes,
		     const MlnBuffer *certificate, int *verified);
int MlnImpl_8105B230(const MlnXmlDsigSignature *signature);
int MlnImpl_8105B140(const MlnBuffer *uri, int count, MlnBuffer *const *ids);
int MlnImpl_8105B364(const MlnXmlDsigReference *reference, int transforms, int digestSize,
		     const unsigned char *digest);
int MlnImpl_8105D0AC(MlnXmlDsigSignature *signature);
int MlnImpl_81082614(MlnXmlNode *node, MlnOctopusReference **out);
int MlnImpl_81060F80(const MlnOctopusControl *control, MlnBuffer **out);
int MlnImpl_81060DE0(const MlnOctopusController *controller, MlnBuffer **out);
int MlnImpl_810403F4(const MlnBuffer *token, MlnOctopusSignedBundle **out);
int MlnImpl_81041F48(MlnOctopusSignedBundle *signedBundle, MlnOctopusContentKey **contentKeys);
int MlnImpl_81080C74(void *key);
int MlnImpl_810614EC(const MlnOctopusLink *link, MlnBuffer **out);
int MlnImpl_810429F8(const MlnBuffer *token, int verify, MlnOctopusNode **out);
int MlnImpl_81042A88(MlnBufferGroups *groups);
int MlnImpl_81042B18(const MlnBuffer *token, int type, MlnOctopusSamlAssertion **out,
		     int verify);
int MlnImpl_81042B38(MlnXmlNode *node, MlnOctopusControl **out);
int MlnImpl_81042B58(const MlnBuffer *token, int type, const MlnBuffer *role,
		     MlnBuffer **nameIdentifier);
int MlnImpl_81042D10(MlnXmlNode *node, MlnOctopusArray **out);
int MlnImpl_81042E1C(MlnXmlNodeList *nodes, MlnPlanktonParameterBlock **out);
int MlnImpl_81042FC0(MlnXmlNodeList *nodes, MlnPlanktonValueList *list);
int MlnImpl_81052BD0(const char *s);

/*
 * store.c: the license store, records in numbered tables kept by the store
 * layer (0x8105dc0c writes a record, NULL deletes it; 0x8105dedc reads one:
 * 0 read, 1 none, 6 a record whose check failed)
 */
typedef struct MlnStoreDigest {
	unsigned char bytes[20];
	MlnBuffer buffer;		/* over bytes once set */
} MlnStoreDigest;

/*
 * a stored link: either the digests of its ends and id, or (byValue) the ids
 * themselves, hashed when it is checked
 */
typedef struct MlnStoreLink {
	struct MlnStoreLink *next;
	unsigned char toDigest[20];
	unsigned char fromDigest[20];
	unsigned char idDigest[20];
	int byValue;
	unsigned int key;		/* its record in table 7 */
	MlnBuffer *from;
	MlnBuffer *to;
	MlnBuffer *value1;
	MlnBuffer *value2;
} MlnStoreLink;

/* keys to add to or remove from a stored record */
typedef struct MlnRecordKeys {
	int count;
	MlnBuffer **keys;
} MlnRecordKeys;

/* an update of the stored record with id */
typedef struct MlnRecordUpdate {
	int reset;			/* drop the record's items first */
	unsigned int tag;		/* the record's new tag; one not newer stops */
	MlnBuffer *id;
	MlnRecordKeys *add;
	MlnRecordKeys *remove;
} MlnRecordUpdate;

typedef struct MlnRecordUpdates {
	unsigned int count;
	MlnRecordUpdate **updates;
} MlnRecordUpdates;

int MlnImpl_810436E4(const MlnBuffer *first, const MlnBuffer *second);
int MlnImpl_810437BC(MlnStoreLink **links, MlnStoreLink *link);
int MlnImpl_81043860(const MlnBuffer *id, const MlnRecordArray *records, MlnRecord **out,
		     int *stored);
MlnStoreDigest *MlnImpl_81043A08(void);
int MlnImpl_81043DE0(const MlnBuffer *to, const MlnBuffer *from, const MlnBuffer *record,
		     const MlnBuffer *value1, const MlnBuffer *value2);
int MlnImpl_81043F1C(const MlnBuffer *to);
unsigned int MlnImpl_8104411C(int *count, MlnStoreLink **out);
unsigned int MlnImpl_810442D4(MlnStoreLink **links, MlnBuffer **record);
int MlnImpl_8104435C(MlnStoreLink *links);
unsigned int MlnImpl_8104436C(const MlnBuffer *digest, MlnBuffer **record);
int MlnImpl_81049D54(MlnStoreLink *links, const MlnBuffer *digest, MlnStoreLink **found);
int MlnImpl_81044824(const MlnBuffer *from, const MlnBuffer *to, MlnBuffer **record);
int MlnImpl_81044A34(int kind);
int MlnImpl_81044D64(MlnBuffer **out);
int MlnImpl_81044DEC(int kind, MlnOctopusSamlAssertion **out);
int MlnImpl_81044F44(const MlnRecordUpdates *updates);
int MlnImpl_810455D8(MlnBuffer **out);
int MlnImpl_81045630(const MlnBuffer *id, unsigned int *tag);

/* the two buffers stored in table 12 */
typedef struct MlnStoreKeyPair {
	int version;			/* 2 */
	MlnBuffer *first;		/* key 1 */
	MlnBuffer *second;		/* key 2 */
} MlnStoreKeyPair;

int MlnImpl_810456B0(const MlnStoreKeyPair *pair);
int MlnImpl_8104570C(MlnArray **out);
int MlnImpl_810457D4(const MlnBuffer *data, const MlnBuffer *value);
int MlnImpl_810459F0(const MlnBuffer *data, MlnBuffer **out);
int MlnImpl_81045A3C(const MlnBuffer *data);
int MlnImpl_81045E74(MlnBuffer **out);
int MlnImpl_81045EE0(const MlnBuffer *data);
int MlnImpl_81045EF8(int table, MlnBuffer **out);
int MlnImpl_81046264(int table, const unsigned char *entry, const MlnBuffer *index);

/* a set of bits, the first one the top bit of bits[0] */
typedef struct MlnBitset {
	int count;
	unsigned char bits[];
} MlnBitset;

int MlnImpl_81046354(int count, MlnBitset **out);
int MlnImpl_810463B4(MlnBitset *set);
int MlnImpl_810463CC(MlnBitset *set, int index, int value);
int MlnImpl_81046420(const MlnBitset *set, int value, int *index);
int MlnImpl_8104657C(int kind, const MlnBuffer *index, const unsigned char *id1,
		     const unsigned char *id2, const unsigned char *id3,
		     const unsigned char **entry, int *free);
int MlnImpl_810476C0(int kind);
int MlnImpl_81046BFC(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, int links);
int MlnImpl_8104A110(MlnStoreLink **links, const unsigned char *entry);
int MlnImpl_810467C0(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, MlnBuffer value);
int MlnImpl_8104A300(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     const MlnBuffer *value1, const MlnBuffer *value2);
int MlnImpl_81049DDC(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     MlnStoreLink **found);
int MlnImpl_810475F4(int table);
const MlnBuffer *MlnImpl_81047E34(void);
int MlnImpl_81049874(MlnStoreLink **links);
int MlnImpl_8104990C(const MlnStoreLink *links);
int MlnImpl_81049918(MlnStoreLink *links);
int MlnImpl_81049978(MlnStoreLink **links, const MlnBuffer *from, const MlnBuffer *to,
		     const MlnBuffer *id, const MlnBuffer *value1, const MlnBuffer *value2,
		     unsigned int *key);
int MlnImpl_81049F14(unsigned int key);
int MlnImpl_81043A84(int step, unsigned int size, const void *data);
int MlnImpl_81043AE4(int step, MlnBuffer **out);
int MlnImpl_81043D94(void);
int MlnImpl_81046F98(int kind, const unsigned char *id1, const unsigned char *id2,
		     const unsigned char *id3, MlnBuffer **out, int links);
int MlnImpl_810472BC(unsigned int *count, int table);
int MlnImpl_81047424(unsigned int i, int table, MlnBuffer **out);
int MlnImpl_81047CDC(const unsigned char *digest);
int MlnImpl_81048CF8(const MlnStoreLink *links, const unsigned char *digest);
int MlnImpl_81048FF0(const MlnStoreLink *links);
int MlnImpl_810492F8(const MlnStoreLink *links, const MlnStoreLink *link);
int MlnImpl_81049464(MlnStoreLink **out);
int MlnImpl_81048DF0(MlnStoreLink *links, const MlnBuffer *from, const MlnBuffer *to,
		     MlnStoreLink **found, int *free);
int MlnImpl_81047D7C(int ret, int miss, unsigned int flag, unsigned int *found,
		     unsigned int *damaged);
int MlnImpl_81049E34(MlnStoreLink **links, MlnStoreLink *link);
int MlnImpl_8105D444(unsigned int kind, unsigned int hash, unsigned int key, MlnBuffer **value);
int MlnImpl_8105D52C(void);
int MlnImpl_8105D620(unsigned int kind, unsigned int hash, unsigned int key, int dirty,
		     MlnBuffer *value);
int MlnImpl_8105DAD0(unsigned int kind, int index, const MlnBuffer *value);
int MlnImpl_8105DC0C(unsigned int kind, int index, const MlnBuffer *value);
void MlnImpl_8105DD94(MlnBuffer *value);
int MlnImpl_8105DDA8(unsigned int kind, int index, MlnBuffer **out);
int MlnImpl_8105DEDC(unsigned int kind, int index, MlnBuffer **out);
int MlnImpl_8105ED38(unsigned int key, const MlnBuffer *value, MlnBuffer **out);
int MlnImpl_8105F124(unsigned int key, const MlnBuffer *value, MlnBuffer **out);
unsigned int MlnImpl_8105E060(int dirty, MlnBuffer **out);
unsigned int MlnImpl_8105E5E4(const MlnBuffer *state);
unsigned int MlnImpl_8105EA2C(void);
int MlnImpl_8105EC7C(unsigned int key, const MlnBuffer *value, MlnBuffer **out);
int MlnImpl_8105EC88(unsigned char *key);
MlnBuffer *MlnImpl_8106A08C(int heap);
int MlnImpl_8106A0AC(int algorithm, const MlnBuffer *key, const MlnBuffer *message,
		     MlnBuffer **out);
int MlnImpl_8106A0A4(void);
void MlnImpl_8107FB58(const unsigned char *key, unsigned int keySize,
		      const unsigned char *message, unsigned int messageSize, unsigned char *out);
void MlnImpl_8107FBAC(const unsigned char *key, unsigned int keySize,
		      const unsigned char *message, unsigned int messageSize, unsigned char *out);
unsigned int MlnImpl_81052EBC(unsigned int *seconds);
int MlnImpl_81052944(const MlnDateTime *a, const MlnDateTime *b);
int MlnImpl_810529E8(int value, int base, int size, char *buf);
/* prefix, the buffer's text and suffix joined into a new string */
int MlnImpl_81082EA4(const char *prefix, const MlnBuffer *value, const char *suffix, char **out);
int MlnImpl_81052CA8(int value, int base);
int MlnImpl_81061898(int kind, const MlnOctopusKey *key, const MlnBuffer *message,
		     const MlnBuffer *signature);
int MlnImpl_81060FE8(const MlnOctopusContentKey *contentKey, MlnBuffer **out);
int MlnImpl_81061964(int kind, const MlnOctopusKey *key, unsigned int size, const void *data,
		     unsigned int digestSize, const void *digest);
int MlnImpl_8103EFDC(MlnXmlNode *node, MlnOctopusExtension **out);
int MlnImpl_8103F62C(MlnXmlNodeList *list, MlnOctopusArray **out);
int MlnImpl_81040368(MlnOctopusSignedBundle *signedBundle);
int MlnImpl_81009B50(int heap, unsigned int size, const unsigned char *text, int *value);
void MlnImpl_81080C80(void *key);

int MlnImpl_8102A228(unsigned int error);
int MlnImpl_8102A33C(const MlnBox *box, unsigned char *out);
void MlnImpl_81080BF4(void);
void MlnImpl_81080BF8(void);
unsigned int MlnImpl_8103B29C(const MlnBuffer *state);
unsigned int MlnImpl_81043A78(void);
unsigned int MlnImpl_81043A6C(const MlnBuffer *state);
unsigned int MlnImpl_81043A60(int kind, MlnBuffer **out);
unsigned int MlnImpl_81043C70(int step);
unsigned int MlnImpl_8104A448(void);
unsigned int MlnImpl_8104A4B8(void);
unsigned int MlnImpl_8104A558(void);

/*
 * The core context (0x3c bytes, zeroed by MlnImpl_81062024): what one
 * open of the core holds, filled from the store by MlnImpl_8104A560.
 */
typedef struct MlnCoreContext MlnCoreContext;

/* the control VM, the link graph and the key store under a core context */

/* what the control VM is given of its host: the link graph and its functions */
typedef struct MlnCoreHost {
	void *graph;
	void *functions[7];
} MlnCoreHost;

/* a host function of System.Host.GetTrustedTime and GetLocalTrustedTime */
typedef unsigned int (*MlnVmHostTimeFunction)(MlnCoreHost *host, unsigned int *minutes,
					      int *trusted);
/* a host function of System.Host.GetLocalTime and GetLocalTimeOffset */
typedef unsigned int (*MlnVmHostValueFunction)(MlnCoreHost *host, int *value);

#define MLN_VM_HOST_TRUSTED_TIME	2	/* functions[] of each system call */
#define MLN_VM_HOST_LOCAL_TIME		3
#define MLN_VM_HOST_LOCAL_TIME_OFFSET	4
#define MLN_VM_HOST_LOCAL_TRUSTED_TIME	5

#define MLN_VM_MODULES		4	/* the code modules a VM's group registers */
#define MLN_VM_INSTANCES	8	/* the VMs of a group */
#define MLN_VM_CALL_DEPTH	128

/* a routine a code module exports */
typedef struct MlnVmExport {
	char *name;
	unsigned int nameSize;		/* with its terminating NUL */
	unsigned int offset;		/* in the module's code */
} MlnVmExport;

/* a loaded code module: its code follows the header */
typedef struct MlnVmModule {
	unsigned int codeSize;
	unsigned int dataOffset;	/* where its data segment is in the VM's memory */
	int exportCount;
	MlnVmExport *exports;
	const unsigned char *signer;	/* the signer's data and size, as registered */
	unsigned int signerSize;
	MlnOctopusArray *attributes;
	struct MlnVmModule *next;
	unsigned char code[];
} MlnVmModule;

/* a VM's memory: the data segments from the bottom, the stack down from the top */
typedef struct MlnVmMemory {
	unsigned int size;
	unsigned int dataSize;		/* the data segments' end */
	unsigned char data[];
} MlnVmMemory;

struct MlnVm;

/* a group of VMs: the code modules registered and the VMs spawned */
typedef struct MlnVmGroup {
	int moduleCount;
	MlnBuffer *codeModules[MLN_VM_MODULES];
	MlnBuffer *uids[MLN_VM_MODULES];
	MlnBuffer *signers[MLN_VM_MODULES];
	MlnOctopusArray *attributes[MLN_VM_MODULES];
	int vmCount;
	struct MlnVm *vms[MLN_VM_INSTANCES];
} MlnVmGroup;

/* a Plankton VM (MlnImpl_8106C54C) */
typedef struct MlnVm {
	MlnVmMemory *memory;
	MlnVmModule *modules;
	unsigned char *sp;
	MlnCoreHost *host;
	unsigned int children;		/* a bit per VM of the group this one spawned */
	MlnVmGroup *group;
	int index;			/* in the group's vms */
	int maxStack;			/* the deepest stack, in words */
	int maxCalls;			/* the deepest call nesting */
} MlnVm;

/* a routine's run (MlnImpl_8106A780), as the system calls see it */
typedef struct MlnVmContext {
	MlnVm *vm;
	MlnVmModule *module;
	MlnVmMemory *memory;
	MlnVmExport *routine;
	const unsigned char *ip;
	unsigned char *sp;
	int calls;
	const unsigned char *returns[MLN_VM_CALL_DEPTH];
	const unsigned char *codeStart;
	const unsigned char *codeEnd;
	unsigned char *data;
	unsigned char *dataEnd;
	unsigned char *stackLow;
	unsigned char *stackHigh;
} MlnVmContext;

unsigned int MlnImpl_8106A15C(unsigned int result, int *code);
unsigned int MlnImpl_8106A264(MlnVmModule **list);
unsigned int MlnImpl_8106A590(MlnVm *vm);
unsigned int MlnImpl_8106A780(MlnVm *vm, const char *routine, int *status);
unsigned int MlnImpl_8106BDA0(MlnVm *vm, const MlnBuffer *uid);
unsigned int MlnImpl_8106C794(MlnVm *vm, const char *routine);
unsigned int MlnImpl_8106C54C(int size, MlnCoreHost *host, MlnVm **vm);
unsigned int MlnImpl_8106C694(MlnVm *vm);
unsigned int MlnImpl_8106C6F8(MlnVm *vm, MlnBuffer *codeModule, MlnBuffer *uid, MlnBuffer *protocol,
			      MlnBuffer *type, MlnBuffer *signer, MlnOctopusArray *attributes);
unsigned int MlnImpl_8106C788(MlnVm *vm, MlnBuffer *uid);
unsigned int MlnImpl_8106C828(MlnVm *vm, unsigned int *words);
unsigned int MlnImpl_8106C970(MlnVm *vm, unsigned int *value);
unsigned int MlnImpl_8106C86C(MlnVm *vm, unsigned int value);
unsigned int MlnImpl_8106C8CC(MlnVm *vm, unsigned int size, const unsigned char *data);
unsigned int MlnImpl_8106C7A4(MlnVm *vm, int address, int size, unsigned char *out);
unsigned int MlnImpl_8106C9D4(MlnVm *vm);
unsigned int MlnImpl_8106C9F4(MlnVm *vm, unsigned int address, MlnBuffer **out, unsigned int *type);

/* the VM's stack and system calls (0x81083010-) */
unsigned int MlnImpl_81083010(MlnVmContext *context, unsigned int first, unsigned int second);
unsigned int MlnImpl_810830A4(MlnVmContext *context, int *value);
unsigned int MlnImpl_81083118(MlnVmContext *context, int *top, int *next);
unsigned int MlnImpl_810831C0(MlnVmContext *context, int *top, int *next);
unsigned int MlnImpl_8108325C(MlnVmContext *context);
unsigned int MlnImpl_81083408(MlnVmContext *context);
unsigned int MlnImpl_81083638(MlnVmContext *context);
unsigned int MlnImpl_810836DC(MlnVmContext *context);
unsigned int MlnImpl_810837CC(MlnVmContext *context);
unsigned int MlnImpl_81083A58(MlnVmContext *context);

/*
 * hostobject.c: the host's object tree, as the control VM's system calls see it
 * (0x8106CB50-): a graph of the personality node, the bundle, the SAML
 * assertions and the parameters, and the objects made from it on demand.
 */
#define MLN_HOST_OBJECT_CONTAINER	0
#define MLN_HOST_OBJECT_INTEGER		1
#define MLN_HOST_OBJECT_STRING		2
#define MLN_HOST_OBJECT_BYTES		3

/* a path that names nothing, and a ninth value list for the eight slots */
#define MLN_HOST_ERROR_NOT_FOUND	0x23060002
#define MLN_HOST_ERROR_NO_SLOT		0x23060003

#define MLN_HOST_ASSERTIONS	256
#define MLN_HOST_SLOTS		8
#define MLN_HOST_SLOT_FIRST	25	/* the slots' object ids, 25-32 */

struct MlnHostGraph;
struct MlnHostObject;

typedef int (*MlnHostObjectGetByIndex)(struct MlnHostGraph *graph, struct MlnHostObject *object,
				       int index, struct MlnHostObject **out);
typedef int (*MlnHostObjectGetByName)(struct MlnHostGraph *graph, struct MlnHostObject *object,
				      const char *name, unsigned int size,
				      struct MlnHostObject **out);

typedef struct MlnHostObject {
	int id;				/* the tree's node, a slot's, or -1 */
	MlnBuffer *name;
	int type;			/* MLN_HOST_OBJECT_* */
	int hasChildren;
	union {
		int integer;
		MlnBuffer *buffer;	/* STRING and BYTES */
		void *container;	/* what the callbacks read the children from */
	} value;
	int childCount;
	MlnHostObjectGetByIndex getByIndex;
	MlnHostObjectGetByName getByName;
} MlnHostObject;

/* a node of the tree: its id, its first child and the next of its siblings, or -1 */
typedef struct MlnHostNode {
	int id;
	int firstChild;
	int nextSibling;
	const char *name;
} MlnHostNode;

/* a value list or an attribute list the VM was given an object of */
#define MLN_HOST_SLOT_VALUE_LIST	0	/* MlnPlanktonValueList */
#define MLN_HOST_SLOT_ATTRIBUTES	1	/* MlnOctopusArray of MlnOctopusAttribute */

typedef struct MlnHostSlot {
	int id;
	int kind;			/* MLN_HOST_SLOT_*, -1 when free */
	void *value;
} MlnHostSlot;

/* a node the links reach from the personality, the node it is reached from and the link's control */
typedef struct MlnHostLinkTarget {
	MlnBuffer *uid;
	struct MlnHostLinkTarget *from;
	MlnOctopusControl *control;
} MlnHostLinkTarget;

typedef struct MlnHostLinks {
	MlnOctopusArray *links;		/* MLN_OCTOPUS_LINK */
	MlnOctopusArray *targets;	/* MlnHostLinkTarget, the personality's first */
} MlnHostLinks;

typedef struct MlnHostGraph {
	MlnOctopusNode *personality;
	MlnOctopusBundle *bundle;
	MlnOctopusSamlAssertion *assertions[MLN_HOST_ASSERTIONS];
	unsigned int current;		/* the assertion last given as an object, or -1 */
	MlnHostLinks *links;
	MlnBuffer *licenseSuspension;
	void *records;			/* freed by MlnImpl_8107B4A0 */
	MlnBuffer *contextId;
	MlnPlanktonParameterBlock *parameters;
	MlnHostSlot slots[MLN_HOST_SLOTS];
} MlnHostGraph;

int MlnImpl_8106CB50(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106CB58(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106CB60(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106CB78(MlnHostObject *object);
int MlnImpl_8106CBC4(int index, const char *path, int *outIndex, const char **outPath);
int MlnImpl_8106CD00(const MlnPlanktonParameterBlock *block, const char *path, unsigned int size,
		     int top, MlnPlanktonValueList **out);
int MlnImpl_8106CF58(const MlnOctopusSamlAttribute *attribute, MlnHostObject **out);
int MlnImpl_8106D138(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106D1C8(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106D200(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106D318(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106D434(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106D530(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106D64C(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106D748(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106D864(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106D960(const MlnOctopusAttribute *attribute, MlnHostObject **out);
int MlnImpl_8106DC1C(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106DCC0(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106DCF8(MlnHostGraph *graph, const MlnPlanktonParameterBlock *block, const char *path,
		     int top, MlnHostObject **out);
int MlnImpl_8106E060(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106E0C0(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106E10C(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out);
int MlnImpl_8106E20C(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out);
int MlnImpl_8106E57C(MlnHostGraph *graph, int index, MlnOctopusArray *attributes,
		     MlnHostObject **out);
unsigned int MlnImpl_8106EE94(MlnHostGraph *graph, MlnPlanktonParameterBlock *parameters);
unsigned int MlnImpl_8106EEA8(MlnHostGraph *graph, MlnOctopusBundle *bundle);
/* the control VM's host functions; their parameters come with their C */
unsigned int MlnImpl_8106F504();
unsigned int MlnImpl_81070318();
unsigned int MlnImpl_8106F314();
unsigned int MlnImpl_8106F388();
unsigned int MlnImpl_8106F3F4();
unsigned int MlnImpl_8106F438();
unsigned int MlnImpl_81070424();
unsigned int MlnImpl_8106C6C4(MlnVm *vm);
unsigned int MlnImpl_8106EC8C(MlnHostGraph **graph);
unsigned int MlnImpl_8106ED54(MlnHostGraph *graph);
unsigned int MlnImpl_8106EE80(MlnHostGraph *graph, MlnOctopusNode *node);
unsigned int MlnImpl_8106EEBC(MlnHostGraph *graph, MlnOctopusArray *links);
unsigned int MlnImpl_8106F264(MlnHostGraph *graph, MlnOctopusSamlAssertion *assertion);
void MlnImpl_8106F2C8(MlnHostGraph *graph);
unsigned int MlnImpl_8106F300(MlnHostGraph *graph, MlnBuffer *contextId);
unsigned int MlnImpl_810706F4(MlnHostGraph *graph, MlnArray *pair);
unsigned int MlnImpl_81070808(MlnHostGraph *graph, MlnArray **pair);
int MlnImpl_8107B4A0(void *records);
unsigned int MlnImpl_810710F8(void **keys);
void MlnImpl_81071278(void *keys);
unsigned int MlnImpl_810712F0(void *keys, const MlnOctopusKey *key);
unsigned int MlnImpl_81071368(void *keys, const MlnOctopusKey *key);
unsigned int MlnImpl_810715FC(void *keys, MlnBuffer *keyName, unsigned int method, void *rsa,
			      MlnBuffer *out);
unsigned int MlnImpl_8107148C(void *keys, MlnBuffer *uid, MlnOctopusBroadcastKeyBlock *block);
unsigned int MlnImpl_8107132C(void *keys, const MlnOctopusKeyPair *key);

unsigned int MlnImpl_8104A560(MlnCoreContext **context, MlnBuffer **state);
unsigned int MlnImpl_81062024(MlnCoreContext **context);
void MlnImpl_81062060(MlnCoreContext *context);
void MlnImpl_810628E4(MlnCoreContext *context);
unsigned int MlnImpl_81062164(MlnCoreContext *context, MlnOctopusNode *node);
unsigned int MlnImpl_81062224(MlnCoreContext *context, MlnOctopusArray *links);
unsigned int MlnImpl_810623F0(MlnCoreContext *context, MlnOctopusSamlAssertion *assertion);
unsigned int MlnImpl_81062450(MlnCoreContext *context, MlnArray *pair);
/* the device's credentials in the cache (credentials.c) */
int MlnImpl_81064238(const MlnBufferGroups *groups);
int MlnImpl_81064340(MlnBufferGroups **out);
unsigned int MlnImpl_810644A0(MlnOctopusSamlAssertion **out);
int MlnImpl_8106455C(const MlnBuffer *xml);
int MlnImpl_81064728(const MlnBuffer *der, int *count, MlnBuffer ***items);
int MlnImpl_81064838(const MlnBuffer *packed, MlnBuffer **first, MlnBuffer **second,
		     MlnBuffer **third);
int MlnImpl_81064A18(const MlnBuffer *first, int count1, MlnBuffer **items1, int count2,
		     MlnBuffer **items2, MlnBuffer **out);

/*
 * The handle table in MlnShared's buffers[4]: 0x400 slots and whether
 * MlnImpl_81069F10 has set it up.
 */
#define MLN_HANDLE_TABLE_SLOTS	0x400

typedef struct MlnHandleTable {
	void *slots[MLN_HANDLE_TABLE_SLOTS];
	int initialized;
} MlnHandleTable;

/* a view of an array of object slots (MlnImpl_8107A3D0-) */
typedef struct MlnSlotArray {
	void **items;
	unsigned int count;
} MlnSlotArray;

unsigned int MlnImpl_8107A3D0(MlnSlotArray *array);
unsigned int MlnImpl_8107A428(MlnSlotArray *array, unsigned int *index);
unsigned int MlnImpl_8107A48C(MlnSlotArray *array, unsigned int index, void *object);
unsigned int MlnImpl_8107A4C0(MlnSlotArray *array, unsigned int index, void **out);

/* a pending response of a core handle (freed by MlnImpl_81069A9C) */
typedef struct MlnCoreResponse {
	unsigned int entryOffset;	/* the content's entry in data, 0 when none (MlnImpl_81069AE4) */
	int pending;		/* data still to be written to the store */
	unsigned int minutes;	/* the entry's first use (MlnImpl_81069D28) */
	MlnBuffer *request;
	MlnBuffer *data;	/* the store's table 14 */
} MlnCoreResponse;

/* what a handle of MlnImpl_8104A8D8 names (0x28 bytes) */
typedef struct MlnCoreHandle {
	MlnCoreContext *context;
	unsigned int flags;	/* MLN_CORE_HANDLE_BUNDLE */
	MlnOctopusSignedBundle *bundle;
	int reservedC;
	void *memory;
	MlnCoreResponse *response;
	int outcome;		/* set by MlnImpl_8104B4C4, 0 when a record handle opens */
	MlnBuffer *callbackRoutine;	/* the rights' first callback of type 1 */
	int callbackCookie;
	int kind;		/* the last action (MlnImpl_8104B4C4), 4 until one */
} MlnCoreHandle;

#define MLN_CORE_HANDLE_BUNDLE	0x1	/* a bundle was given (MlnImpl_8104AB18) */
#define MLN_CORE_HANDLE_RECORD	0x2	/* a record handle was opened (MlnImpl_8104AF48) */
#define MLN_CORE_HANDLE_DONE	0x4	/* an action was taken (MlnImpl_8104B4C4) */

/* a record handle of MlnImpl_8104AF48 (0x20 bytes): a record of a core handle */
typedef struct MlnCoreRecordHandle {
	void *object;		/* made by MlnImpl_81061BBC */
	MlnCoreHandle *core;
	MlnOctopusContentKey *contentKey;	/* the record's, from the core context (MlnImpl_8106273C) */
	MlnBuffer *buffer;	/* the record's buffer */
	unsigned int kind;	/* MLN_SERVICE_RECORD_*, with head and tail as in the record */
	unsigned char head[4];
	unsigned char tail[8];
} MlnCoreRecordHandle;

unsigned int MlnImpl_81061BBC(void **out);
unsigned int MlnImpl_81061C00(void *track);
unsigned int MlnImpl_81061C1C(void *object, int kind, const unsigned char *data);
unsigned int MlnImpl_81082834(MlnCoreContext *context, MlnCoreHandle *core);
unsigned int MlnImpl_8106249C(MlnCoreContext *context, MlnArray **pair);
unsigned int MlnImpl_81062BE8(MlnCoreContext *context, int kind, MlnPlanktonParameterBlock *parameters,
			      MlnOctopusContentKey **contentKeys, MlnBuffer **rights);
unsigned int MlnImpl_81062EDC(MlnCoreContext *context, int count,
			      MlnOctopusContentKey **contentKeys);
unsigned int MlnImpl_810629C8(MlnCoreContext *context, int kind,
			      MlnPlanktonParameterBlock *parameters, MlnBuffer **out);
unsigned int MlnImpl_8104C130(int handle, const MlnBuffer *publicKey, int *count,
			      MlnBuffer ***first, MlnBuffer ***second, MlnBuffer ***third);
unsigned int MlnImpl_8106340C(int count, MlnOctopusControl **controls, const MlnBuffer *agent,
			      void *value, MlnPlanktonParameterBlock *parameters, MlnArray *pair,
			      unsigned int *status, MlnBuffer **data, MlnArray **result);
unsigned int MlnImpl_81063F38(MlnCoreContext *context, void *rsa, int *count,
			      MlnBuffer ***encryptedKeys, MlnBuffer ***keyIds,
			      MlnBuffer ***contentKeyIds);
unsigned int MlnImpl_81063CE4(MlnCoreContext *context, const MlnServiceObject *object,
			      unsigned int size, const unsigned char *data, MlnBuffer **rights);
unsigned int MlnImpl_8106314C(MlnCoreContext *context, MlnOctopusControl *control,
			      MlnBuffer **out);
unsigned int MlnImpl_81062F20(MlnCoreContext *context, int kind,
			      MlnPlanktonParameterBlock *parameters, MlnBuffer **out);
unsigned int MlnImpl_8106273C(MlnCoreContext *context, MlnBuffer *buffer,
			      MlnOctopusContentKey **contentKey);
unsigned int MlnImpl_81061C64(void *object, const unsigned char *data, unsigned int size,
			      const MlnOctopusKey *key, unsigned char *out,
			      unsigned int *outSize);
unsigned int MlnImpl_81061B2C(const MlnOctopusKey *key, const MlnBuffer *data, MlnBuffer **out);

unsigned int MlnImpl_8104AE1C(int handle, MlnXmlNode *node);
unsigned int MlnImpl_81062524(MlnCoreContext *context, MlnOctopusBundle *bundle);
unsigned int MlnImpl_81069968(MlnBuffer *data, int kind, MlnCoreResponse **out);

unsigned int MlnImpl_81069F10(MlnHandleTable *table);
unsigned int MlnImpl_81069F68(void *object, MlnHandleTable *table, int *handle);
unsigned int MlnImpl_81069FE8(int handle, MlnHandleTable *table, void *out);
unsigned int MlnImpl_8106A048(int handle, void *object, MlnHandleTable *table);
int MlnImpl_81069A9C(MlnCoreResponse *response);
unsigned int MlnImpl_81069AE4(MlnCoreResponse *response, const MlnBuffer *id);
unsigned int MlnImpl_81069C38(MlnCoreResponse *response, const MlnBuffer *id, unsigned int minutes);
unsigned int MlnImpl_81069D28(MlnCoreResponse *response, unsigned int minutes);
unsigned int MlnImpl_81069D90(MlnCoreResponse *response);

/* license.c */
int MlnImpl_8104D15C(int handle, unsigned int size, const unsigned char *data);
unsigned int MlnImpl_8104D16C(int handle, int kind, MlnBuffer **out);
unsigned int MlnImpl_8104D208(const MlnBuffer *controller, const MlnBuffer *token);

/*
 * A NEMO message's header (0x30 bytes): the parts set with MlnImpl_8107A04C, a part's buffer at
 * the index of its bit. The part names are ours, from where this library fills them.
 */
#define MLN_NEMO_PART_ACTION		0x002	/* the action's URN */
#define MLN_NEMO_PART_ORIGIN		0x004	/* a license transfer's: the requester before */
#define MLN_NEMO_PART_RECIPIENT		0x008	/* a license transfer's: the sender before */
#define MLN_NEMO_PART_SENDER		0x010	/* the sender's node, from its credentials */
#define MLN_NEMO_PART_REQUESTER		0x100	/* a request's sender */
#define MLN_NEMO_PART_CONFIRMER		0x200	/* a license transfer step's sender */

typedef struct MlnNemoHeader {
	int count;			/* the parts present */
	unsigned int present;		/* their MLN_NEMO_PART_* bits */
	MlnBuffer *parts[10];		/* by bit, MLN_NEMO_PART_SENDER's in parts[4] */
} MlnNemoHeader;

/* a NEMO message a service exchange sends and receives (0x7c bytes) */
typedef struct MlnNemoMessage {
	MlnXmlNode *body;		/* the payload sent */
	MlnBufferGroups *credentials;	/* the sender's */
	unsigned char nodeCount;
	MlnXmlNode **nodes;		/* nodes sent with the payload */
	MlnXmlNodeList *bundles;	/* a license transfer response's */
	MlnBuffer *received;
	MlnBuffer *transferKey;		/* dropped once a license transfer's move is sent */
	MlnBuffer *peer;		/* the name the service was given */
	int reserved[4];
	void *keys[2];			/* both set before a request is made */
	int reserved2;
	MlnNemoHeader header;
	int reserved3[2];
	int integrity;			/* the request is signed */
	int confidentiality;		/* and encrypted */
} MlnNemoMessage;

/* the protocol of a service exchange */
#define MLN_EXCHANGE_REGISTRATION	2	/* the registration service */
#define MLN_EXCHANGE_LICENSE		3	/* the license service */
#define MLN_EXCHANGE_TRANSFER		6	/* a license transfer between devices */

/* an exchange's errors (exchange.c) */
#define MLN_EXCHANGE_ERROR_FORMAT	0x23000001	/* a payload not as expected */
#define MLN_EXCHANGE_ERROR_MESSAGE	0x23000002	/* the message could not be made or read */
#define MLN_EXCHANGE_ERROR_FAULT	0x23000009	/* no fault to give */

/* a service exchange (MlnImpl_81068844, 0x80 bytes) */
typedef struct MlnExchange {
	int protocol;			/* MLN_EXCHANGE_* */
	MlnNemoMessage message;
} MlnExchange;

/* the NEMO message (0x81072240-): its parts, sending and receiving */
unsigned int MlnImpl_81072240(MlnNemoMessage *message, unsigned char kind, unsigned char step,
			      MlnBuffer **out);
unsigned int MlnImpl_81074EC4(unsigned int size, const void *data, MlnNemoMessage *message,
			      const char *name, const char *uri, unsigned char kind,
			      unsigned char step,
			      int faultKind, int faultStep, MlnXmlNode **payload);
unsigned int MlnImpl_81078EB8(const MlnBuffer *xml, const char *prefix, const char *uri,
			      MlnXmlNode **node);
unsigned int MlnImpl_810799B0(MlnNemoHeader *header);
unsigned int MlnImpl_81079A7C(MlnNemoMessage *message, MlnBuffer *first, MlnBuffer *second);
unsigned int MlnImpl_81079EE8(const MlnBuffer *credential, MlnBuffer **node);
unsigned int MlnImpl_8107A04C(MlnNemoHeader *header, unsigned int part, const MlnBuffer *value);
unsigned int MlnImpl_8107A15C(MlnNemoMessage *message);
unsigned int MlnImpl_8107A19C(MlnNemoMessage *message);

/* exchange.c */
unsigned int MlnImpl_810685FC(unsigned int ret);
unsigned int MlnImpl_81068798(MlnExchange *exchange);
unsigned int MlnImpl_810688CC(MlnExchange *exchange);

/*
 * A service handle of MlnImpl_8104D5E0 (0x1c bytes), in MlnShared's
 * buffers[5]: a protocol exchange of a kind, with the values it was given.
 */
typedef struct MlnCoreServiceHandle {
	int state;			/* the kind in bits 8 and up, the messages done below */
	MlnExchange *exchange;		/* the protocol's (MlnImpl_81068844) */
	MlnServiceList *values;		/* a copy of the values given */
	MlnBuffer *messages[4];
} MlnCoreServiceHandle;

unsigned int MlnImpl_81068844(int protocol, MlnExchange **exchange);
int MlnImpl_81068888(MlnExchange *exchange);
unsigned int MlnImpl_8108299C(MlnBufferGroups **groups);
int MlnImpl_81066CE0(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, const MlnBuffer *name,
		     const MlnBuffer *payload, MlnBuffer **request);
unsigned int MlnImpl_81082DC0(int ret);
unsigned int MlnImpl_81066F74(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **node);
unsigned int MlnImpl_81082B04(void *exchange);
unsigned int MlnImpl_81082BC4(MlnXmlNode **node);
unsigned int MlnImpl_81082B44(MlnXmlNode *document, MlnXmlNode **node);
unsigned int MlnImpl_81067494(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **link, MlnXmlNode **agent);
int MlnImpl_81067618(MlnExchange *exchange, MlnXmlNode *node, MlnBuffer **reply);
unsigned int MlnImpl_81059600(const MlnBuffer *chain, MlnBuffer ***certificates, int *count);
unsigned int MlnImpl_81082A60(MlnBuffer **certificates, int count, int kind);
unsigned int MlnImpl_81068A8C(MlnExchange *exchange, MlnBuffer *first, MlnBuffer *second);
int MlnImpl_81067CA4(MlnExchange *exchange, int step, MlnBufferGroups **credentials,
		     const MlnBuffer *peer, MlnXmlNode **document, MlnBuffer **request);
unsigned int MlnImpl_81050210(int handle, int kind, MlnXmlNode **document,
			      const MlnBuffer *extra, MlnBlob *out);
unsigned int MlnImpl_81068060(MlnExchange *exchange, int step, const char *name,
			      const MlnBuffer *response, MlnXmlNode **document);
unsigned int MlnImpl_81050378(int handle, int kind, const char *name,
			      const MlnBuffer *message, MlnXmlNode **document);
unsigned int MlnImpl_81054880(void **cipher, int algorithm, const void *iv, unsigned int ivSize,
			      const void *nonce, const void *key, unsigned int keySize);
unsigned int MlnImpl_81054A90(void *cipher);
unsigned int MlnImpl_81054AC8(void *cipher, const void *in, unsigned int size, void *out,
			      unsigned int *outSize);
unsigned int MlnImpl_81054EDC(const void *iv, const void *key, const void *in, unsigned int size,
			      void *out, unsigned int *outSize);
unsigned int MlnImpl_810550B0(const void *key, const void *in, void *out);
int MlnImpl_81086D54(void *context, unsigned int keyBits, unsigned int blockBits,
		     const void *key);
void MlnImpl_81087358(void *context, const unsigned char *in, unsigned char *out);
void MlnImpl_81087612(void *context, const unsigned char *in, unsigned char *out);
unsigned int MlnImpl_81054C2C(void *cipher, const void *in, unsigned int size, void *out,
			      unsigned int *outSize);
unsigned int MlnImpl_81054F84(const void *iv, const void *key, const void *in, unsigned int size,
			      void *out, unsigned int *outSize);
unsigned int MlnImpl_8105D280(unsigned char *data, unsigned int *size);
unsigned int MlnImpl_8105D3F4(unsigned char *data, unsigned int *size);
unsigned int MlnImpl_8105D34C(unsigned char *data);
unsigned int MlnImpl_8105D428(unsigned char *data);
unsigned int MlnImpl_81066588(void *context, MlnXmlNode *document, int *category);
unsigned int MlnImpl_81066A28(MlnXmlNode *document, int *value, MlnBuffer **payload);
unsigned int MlnImpl_81065920(void *context, int count, MlnBuffer **encryptedKeys,
			      MlnBuffer **keyIds, MlnBuffer **contentKeyIds, MlnBuffer *pair,
			      MlnBuffer *answer, MlnXmlNode **document);
unsigned int MlnImpl_81065848(MlnXmlNode *document, MlnXmlNode **payload);
unsigned int MlnImpl_81068C90(MlnXmlNode *payload, MlnArray *keys, int kind, MlnXmlNode **node,
			      MlnArray **pending);
unsigned int MlnImpl_81066888(void *context, MlnXmlNode *node, MlnXmlNode **document);
unsigned int MlnImpl_810651F4(MlnXmlNode *document, MlnBuffer **request, MlnBuffer **token);
unsigned int MlnImpl_8106632C(void *context, const MlnBuffer *license);
unsigned int MlnImpl_81066B84(const MlnBuffer *request, MlnBuffer **transfer, MlnXmlNode **node);
unsigned int MlnImpl_8106668C(const MlnBuffer *name, int date, MlnXmlNode **node);
unsigned int MlnImpl_81066548(void *context, MlnXmlNode **control, MlnXmlNode **signature);
unsigned int MlnImpl_81065430(const MlnBuffer *agentId, const MlnBuffer *contextId,
			      const MlnBuffer *controlId, MlnXmlNode *parameter, MlnXmlNode *agent,
			      MlnXmlNode *context, MlnXmlNode **document);
unsigned int MlnImpl_810504EC(unsigned int r, unsigned int fallback);
unsigned int MlnImpl_81067BA4(MlnExchange *exchange, const MlnBuffer *response,
			      MlnXmlNode **agent);
int MlnImpl_81067814(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, MlnXmlNode *from,
		     MlnXmlNode *to, const MlnBuffer *name, const MlnBuffer *payload,
		     MlnBuffer **request);
unsigned int MlnImpl_810684CC(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **license);
int MlnImpl_810681E4(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, MlnXmlNode *node,
		     const MlnBuffer *name, const MlnBuffer *payload, MlnBuffer **request);
unsigned int MlnImpl_81082CB4(const MlnBuffer *xml, MlnXmlNode **node, MlnBlob *detail);
int MlnImpl_810670A0(MlnExchange *exchange, MlnBufferGroups **credentials, int nodeCount,
		     MlnXmlNode **nodes, MlnXmlNode *from, MlnXmlNode *fromOverride,
		     MlnXmlNode *to, const MlnBuffer *name, const MlnBuffer *payload,
		     MlnBuffer **request);
int MlnImpl_8105F2C4(int *id, unsigned int *flags);
unsigned int MlnImpl_81052D14(int id, int mode, unsigned int start, int owner);
unsigned int MlnImpl_81052D84(int *id, int *mode);
unsigned int MlnImpl_81052E04(unsigned int *start);
unsigned int MlnImpl_81052E60(int *owner);
unsigned int MlnImpl_81080BFC(void **handle);
unsigned int MlnImpl_81081374(int full);
unsigned int MlnImpl_81080C1C(void *handle);
int MlnImpl_81080F80(void *key, int sha256, const MlnBuffer *message,
		     const MlnBuffer *signature, int *valid);
int MlnImpl_81080FF4(void *key, int sha256, const MlnBuffer *message,
		     const MlnBuffer *digest, int *valid);
unsigned int MlnImpl_8108106C(void *key, const MlnBuffer *data, MlnBuffer **out);
int MlnImpl_81072020(void *const *key, unsigned int flags, const void *parameters,
		     const MlnBuffer *in, void *out, unsigned int *outSize);
int MlnImpl_81061858(const MlnOctopusKey *key, unsigned int flags, const void *parameters,
		     const MlnBuffer *in, void *out, unsigned int *outSize);
int MlnImpl_810485B4(void);
unsigned int MlnImpl_8108136C(int full, unsigned int size, const void *data);
unsigned int MlnImpl_81042A60(const MlnBuffer *token, MlnOctopusNode **out, int verify);
int MlnImpl_8103B8F8(MlnOctopusNode *node);
int MlnImpl_8102A484(unsigned int mode);
int MlnImpl_8102A5B0(void *id, MlnBlob *out);
int MlnImpl_8102A69C(MlnBlob *out);
int MlnImpl_8102A7F0(const MlnBuffer *state);
int MlnImpl_8102A914(void);
int MlnImpl_8102AAD0(void *p);
unsigned int MlnImpl_8103B190(int *present);
unsigned int MlnImpl_81043CF0(MlnBuffer **state);
unsigned int MlnImpl_81043D7C(const MlnBuffer *state);

/* the CRLNumber extension of a CRL, as the CRL store reports it (0x8103b3ec) */
typedef struct MlnX509CrlNumber {
	unsigned char present;
	unsigned char number[20];
} MlnX509CrlNumber;

unsigned int MlnImpl_8103B3EC(MlnX509CrlNumber *number);
unsigned int MlnImpl_810393D0(int kind, MlnBuffer **out);
unsigned int MlnImpl_81047DC4(const unsigned char *id);
unsigned int MlnImpl_81047E00(void);
int MlnImpl_8102AAE8(int *loaded);
int MlnImpl_8102AE84(const MlnBuffer *state);
MlnX509CrlNumber MlnImpl_8102AFBC(void);
unsigned int MlnImpl_8102B054(void);
int MlnImpl_8102B064(const MlnBuffer *id, MlnBlob *out);
int MlnImpl_8102B198(const MlnBuffer *id);
int MlnImpl_8102B2E0(void);
unsigned int MlnImpl_81048214(unsigned int *flags);
unsigned int MlnImpl_81047E48(const MlnBuffer *id, unsigned int *flags);
int MlnImpl_810827B0(const unsigned char *id);
unsigned int MlnImpl_810486D4(const MlnBuffer *id);
unsigned int MlnImpl_810488A8(const MlnBuffer *id);
int MlnImpl_8102B404(void *id, unsigned int *flags);
int MlnImpl_8102B590(const MlnBuffer *id, MlnBlob *out);
int MlnImpl_8102B718(const MlnBuffer *id, MlnBlob *out);

unsigned int MlnImpl_81045A84(void);
unsigned int MlnImpl_810422E8(MlnBuffer *entry, MlnOctopusLink **link);
unsigned int MlnImpl_81043B94(MlnOctopusNode **base);
int MlnImpl_8102B950(unsigned int *count, MlnBuffer ***entries, MlnBlob *state);
unsigned int MlnImpl_81044984(MlnBuffer *key, MlnBuffer *value, const MlnBuffer *first,
			      const MlnBuffer *second);
unsigned int MlnImpl_810444DC(const MlnBuffer *first, const MlnBuffer *second, unsigned int *count,
			      MlnBuffer ***entries);
unsigned int MlnImpl_810458B0(const MlnBuffer *context, const MlnBuffer *data);
int MlnImpl_8102BDBC(const MlnBuffer *first, const MlnBuffer *second, const MlnBuffer *id,
		     unsigned int *count, MlnBuffer ***entries);
int MlnImpl_8102C08C(const MlnBuffer *context, const MlnBuffer *data, MlnBlob *out);
int MlnImpl_8102C1E8(unsigned char kind, MlnBlob *out);
int MlnImpl_8102C64C(unsigned int error);
unsigned int MlnImpl_8104A8D8(int *handle);
int MlnImpl_8104A9CC(int handle);
int MlnImpl_8102C664(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102C77C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102C7FC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102C870(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102C95C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102CAAC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102CBF8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102CCA4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102CDB4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102CEEC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_8102CFB0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102D05C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_8102D138(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102D1D4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104D5E0(int kind, MlnServiceList *list, int *handle);
int MlnImpl_8104DAA4(int handle);
int MlnImpl_8102D240(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102D454(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8104DC4C(int handle, unsigned int size, const unsigned char *data,
			      const char *name, MlnBlob *out);
int MlnImpl_8102D4D4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8104DE10(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102D6F8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104E18C(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, const MlnBuffer *fourth,
			      MlnBlob *first, MlnBlob *second);
int MlnImpl_8102D8B8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104E45C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second,
			      MlnBlob *third);
int MlnImpl_8102DB4C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104E9B8(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, MlnBlob *first,
			      MlnBlob *second);
int MlnImpl_8102DD64(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104EBD8(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102DFC8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104F044(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, const MlnBuffer *fourth,
			      const MlnBuffer *fifth, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102E188(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104F2F4(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102E468(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104FC70(int handle, unsigned int firstSize, const unsigned char *first,
			      unsigned int secondSize, const unsigned char *second,
			      unsigned int thirdSize, const unsigned char *third);
int MlnImpl_8102E628(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_81050110(int handle, const MlnBuffer *first, const MlnBuffer *second);
int MlnImpl_8102E7A8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);

/* the core's form of a buffer group: each buffer with its size */
typedef struct MlnCoreGroupEntry {
	MlnBuffer *first;
	unsigned int firstSize;
	MlnBuffer *second;
	unsigned int secondSize;
	MlnBuffer *third;
	unsigned int thirdSize;
} MlnCoreGroupEntry;

typedef struct MlnCoreGroup {
	unsigned int tag;
	MlnBuffer *first;
	unsigned int firstSize;
	MlnBuffer *second;
	unsigned int secondSize;
	MlnBuffer *third;
	unsigned int thirdSize;
	MlnCoreGroupEntry *entries;
	unsigned int entryCount;
} MlnCoreGroup;

unsigned int MlnImpl_8104F620(int handle, MlnCoreGroup *group);

/* a SOAP fault of a service exchange (MlnImpl_81068C4C), kept by the exchange */
typedef struct MlnServiceFault {
	MlnBuffer *code;
	MlnBuffer *string;
	MlnBuffer *actor;
	MlnBuffer ***details;		/* each three buffers, the first its name */
	unsigned int detailCount;
} MlnServiceFault;

unsigned int MlnImpl_81068C4C(MlnExchange *exchange, MlnServiceFault **fault);
unsigned int MlnImpl_81079814(MlnNemoMessage *message, MlnServiceFault **fault);
int MlnImpl_8102E8A0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8104AB18(int handle, unsigned int size, const unsigned char *data,
			      MlnBlob *out);
int MlnImpl_8102EBD0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104AF48(int handle, const MlnServiceRecordValue *record, int *opened);
int MlnImpl_8104B1C8(int handle);
int MlnImpl_8102ED08(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102EED4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8104B2AC(int handle, int kind, int flags, MlnBlob *out);
unsigned int MlnImpl_8104B4C4(int handle, int kind, MlnBuffer *const *transfer, MlnBlob *out,
			      MlnBlob *state);
unsigned int MlnImpl_8104BFB0(int handle, int kind, int flags, MlnBlob *out);
int MlnImpl_8102EF48(int command, const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104C32C(int handle, const MlnServiceObject *object, unsigned int size,
			      const unsigned char *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102F1B0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104C7B4(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_8102F408(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104CDA0(int handle, unsigned int offset, unsigned int size,
			      unsigned int *position, unsigned int limit);
int MlnImpl_8102F5E4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8104CF00(int handle, int kind, unsigned int firstSize,
			      const unsigned char *first, unsigned int secondSize,
			      const unsigned char *second);
int MlnImpl_8102F83C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102F9A0(int command, const MlnServiceReply *reply, MlnServiceArgs **out,
		     int *status);
int MlnImpl_81052600(void);
int MlnImpl_81052608(void);
int MlnImpl_8102FA24(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102FA70(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_81052610(unsigned int kind, unsigned int capacity, MlnBlob *out);
int MlnImpl_8102FAAC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_8105272C(unsigned int size, const unsigned char *data, int plain,
			      MlnBlob *out);
int MlnImpl_8102FC18(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_8102FCCC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8102FDB4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_81050778(int *handle, MlnServiceList *list, unsigned int *firstCount,
			      MlnBuffer ***firsts, unsigned int *secondCount, MlnBuffer ***seconds,
			      MlnBuffer **third, MlnBuffer **fourth);
int MlnImpl_810505B8(int handle);

/*
 * A session with a registration service (MlnImpl_81050778), in MlnShared's buffers[7]: its
 * service handle (kind 8) and core handle, with what the session keeps.
 */
typedef struct MlnRegistrationSession {
	void *context;			/* MlnImpl_81064D8C's */
	int service;
	int core;
	MlnBuffer *buffer;
	MlnBuffer *token;		/* the caller's, taken */
	MlnRightsInfo *rights;
	MlnXmlNode *node;
} MlnRegistrationSession;

int MlnImpl_81064CD8(MlnXmlNode *node, const char *name, MlnXmlNode **out);
unsigned int MlnImpl_81064D8C(void **context);
unsigned int MlnImpl_81064DD4(void *context);
unsigned int MlnImpl_81064E18(MlnBufferGroups **info);
unsigned int MlnImpl_81064E68(MlnBuffer **request);
unsigned int MlnImpl_81064EC0(void *context, MlnBuffer *request, const MlnBuffer *data,
			      MlnXmlNode **document);
struct MlnX509Certificates;
unsigned int MlnImpl_81082E44(int count, MlnBuffer **items, struct MlnX509Certificates *out);
int MlnImpl_8102FE68(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_81030648(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_81050E58(int handle, const MlnBuffer *data, MlnBlob *out);
int MlnImpl_810306C8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
unsigned int MlnImpl_81051084(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_81030858(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8105152C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_81030A24(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8105183C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second);
int MlnImpl_81030BD0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_81051F2C(int handle, const MlnBuffer *data, const MlnBuffer *extra,
			      MlnBlob *first, MlnBlob *second);
int MlnImpl_81030D90(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
unsigned int MlnImpl_8104D018(int handle, const MlnBuffer *data, MlnBuffer **out);
int MlnImpl_81030FA8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8103110C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_81031288(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_8104CA50(const MlnBuffer *from, const MlnBuffer *to, unsigned int *notBefore,
		     unsigned int *notAfter, MlnBlob *state);
int MlnImpl_8104D144(int *handle);
int MlnImpl_8104D150(int handle);
int MlnImpl_8103149C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_810316BC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result);
int MlnImpl_810317AC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status);
int MlnImpl_810318D4(const MlnBuffer *state, const MlnBuffer *request, MlnBlob *response,
		     MlnBlob *result);
int MlnImpl_81031A40(unsigned int command, MlnServiceReply *reply, MlnBuffer **response,
		     MlnBuffer **result);
int MlnImpl_8102AA38(MlnBlob *blob, MlnBuffer *out);

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

/* an entry of a kind 4 list: a key and a list of its own */
typedef struct MlnConfigListEntry {
	MlnBuffer *key;
	MlnConfigList values;
} MlnConfigListEntry;

int MlnImpl_81033AE8(MlnConfigList *list);

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

int MlnImpl_81033D9C(unsigned int size, const void *xml, MlnServiceConfig **out);
int MlnImpl_810341B8(MlnServiceConfig *config);
int MlnImpl_81034224(MlnActionToken *token);
int MlnImpl_810342E8(unsigned int size, const void *xml, MlnActionToken **out);
int MlnImpl_810818F0(MlnXmlNode *node, MlnBuffer **id);

/* a node of a DER (ASN.1) tree */
typedef struct MlnAsn1Node {
	union {
		struct {
			unsigned char tagClass;
			unsigned char constructed;
			unsigned char tag;
			unsigned char flags;	/* MLN_ASN1_* */
		};
		unsigned int header;	/* the four as one word, MLN_ASN1_IDENTIFIER() */
	};
	union {
		struct {
			unsigned int length;	/* bytes, or a constructed node's children */
			union {
				unsigned char *data;
				struct MlnAsn1Node **children;
			};
		};
		MlnBuffer value;	/* a primitive node's length and data */
	};
	const unsigned char *encoding;	/* the node's whole encoding, header first */
	unsigned int encodingLength;
} MlnAsn1Node;

/* tagClass, constructed and tag together, compared with MLN_ASN1_ID() */
#define MLN_ASN1_IDENTIFIER(node)	((node)->header & 0xFFFFFF)
#define MLN_ASN1_ID(tagClass, constructed, tag) \
	((unsigned int)(tagClass) | (unsigned int)(constructed) << 8 | (unsigned int)(tag) << 16)
#define MLN_ASN1_TAG_BOOLEAN	1
#define MLN_ASN1_TAG_INTEGER	2
#define MLN_ASN1_TAG_BIT_STRING	3
#define MLN_ASN1_TAG_OID	6
#define MLN_ASN1_TAG_UTF8_STRING	12
#define MLN_ASN1_TAG_SEQUENCE	16
#define MLN_ASN1_TAG_SET	17
#define MLN_ASN1_TAG_UTC_TIME	23
#define MLN_ASN1_TAG_GENERALIZED_TIME	24
#define MLN_ASN1_INDEFINITE	0x01	/* the encoding ends with end-of-contents octets */
#define MLN_ASN1_CONTENTS	0x02	/* data holds the encoded contents, also a constructed node's */
#define MLN_ASN1_DATA_BORROWED	0x04	/* a primitive node's data belongs to another */
#define MLN_ASN1_ENCODED	0x08	/* encodingLength holds the encoded size */
#define MLN_ASN1_STATIC		0x80	/* the node is not the tree's to free */

/* the child at index of a constructed node, NULL when there is none */
static inline MlnAsn1Node *MlnX509Child(const MlnAsn1Node *node, unsigned int index)
{
	if (node == NULL || !node->constructed || index >= node->length)
		return NULL;
	return node->children[index];
}


int MlnImpl_810353EC(int heap, MlnAsn1Node *node);
int MlnImpl_81035440(const MlnAsn1Node *node, int pivot, MlnDateTime *time);
int MlnImpl_810356A8(int heap, const unsigned char *der, const unsigned char *end,
		     const unsigned char **next, MlnAsn1Node *node);
int MlnImpl_81035884(const MlnAsn1Node *node, int depth);
int MlnImpl_81035ABC(int heap, MlnAsn1Node *node);
int MlnImpl_810360E8(int heap, int tagClass, unsigned char constructed, int tag,
		     unsigned int length, const void *data, int borrowed, MlnAsn1Node **out);
int MlnImpl_81036204(const MlnAsn1Node *node, unsigned int *size);
int MlnImpl_81036364(const MlnAsn1Node *node, unsigned char **pos);
int MlnImpl_810368BC(int heap, const MlnAsn1Node *node, unsigned int *size);
int MlnImpl_8103696C(int heap, const MlnAsn1Node *node, unsigned char *buffer, unsigned int *size);
int MlnImpl_810369B8(int heap, MlnAsn1Node *node, int definite);
int MlnImpl_81036A04(int heap, const MlnAsn1Node *node);
int MlnImpl_81036A14(int heap, const MlnAsn1Node *node, MlnAsn1Node **out);
int MlnImpl_81036CA8(int heap, unsigned int size, const unsigned char *encoding,
		     MlnAsn1Node **out);
int MlnImpl_81036D28(int heap, int tagClass, unsigned char constructed, int tag,
		     unsigned int length, const void *contents, MlnAsn1Node **out);
int MlnImpl_81036E34(int heap, int tagClass, int tag, MlnAsn1Node **out);
int MlnImpl_81036EBC(int heap, int tagClass, int tag, unsigned int length,
		     const void *contents, MlnAsn1Node **out);
int MlnImpl_81036FC8(int heap, MlnAsn1Node *node);
int MlnImpl_81036FD4(int heap, unsigned char *data);
int MlnImpl_81036FE4(int heap, MlnAsn1Node *parent, int index, MlnAsn1Node *child);
int MlnImpl_81037084(int heap, int mode, const unsigned char *der, const unsigned char *end,
		     unsigned int *size, const unsigned char **next, MlnAsn1Node **out);
int MlnImpl_81037660(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **contents, int *length, unsigned int *encodingSize);
int MlnImpl_810378B0(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **next);
int MlnImpl_810378E4(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **next, MlnAsn1Node **out);
int MlnImpl_81037918(int heap, const unsigned char *der, unsigned int size,
		     const unsigned char **next, MlnAsn1Node **out);
int MlnImpl_81037948(int heap, MlnAsn1Node *parent, MlnAsn1Node *child);
int MlnImpl_810379E0(int heap, MlnAsn1Node *parent, int index);
int MlnImpl_81037A80(int heap, MlnAsn1Node *parent, int index, MlnAsn1Node *child,
		     MlnAsn1Node **old);
int MlnImpl_81037B00(int heap, MlnAsn1Node *parent, int index, MlnAsn1Node **old);
MlnAsn1Node *MlnImpl_81037B80(int heap, const MlnAsn1Node *parent, int tagClass, int tag);
int MlnImpl_81037E6C(int heap, MlnAsn1Node **sequence);
int MlnImpl_81037BF4(int heap, int value, MlnAsn1Node **out);
int MlnImpl_81037D68(int heap, unsigned int length, const void *data, MlnAsn1Node **out);
int MlnImpl_81037E4C(int heap, int value, MlnAsn1Node **out);
int MlnImpl_81037EC8(int heap, const MlnAsn1Node *node, int *value);
int MlnImpl_81037F54(int heap, const MlnAsn1Node *node, int *value);
int MlnImpl_81037FE0(int heap, const MlnAsn1Node *node, int size, unsigned char *out);
int MlnImpl_8103814C(int heap, const MlnAsn1Node *node, unsigned int size, unsigned char *out,
		     int *bits);
int MlnImpl_81038218(int heap, const MlnAsn1Node *node, const unsigned char **data,
		     unsigned int *length);
int MlnImpl_8103826C(int heap, const MlnAsn1Node *node, int *value);
int MlnImpl_810382F8(int heap, const MlnAsn1Node *node, int pivot, MlnDateTime *time);
int MlnImpl_8103833C(int heap, const MlnAsn1Node *node, MlnDateTime *time);
int MlnImpl_8103837C(int heap, const MlnAsn1Node *node, int pivot, int *seconds);
int MlnImpl_810383F8(int heap, const MlnAsn1Node *node, int *seconds);
int MlnImpl_81038470(int heap, int tag, MlnAsn1Node *node);
int MlnImpl_81038484(int heap, int tag, MlnAsn1Node *child, MlnAsn1Node **out);
int MlnImpl_8103857C(int heap, const MlnAsn1Node *node);
int MlnImpl_81038600(int heap, const unsigned char *der, unsigned int size, MlnAsn1Node *nodes,
		     int max, int *count);

/* X.509: the algorithms the certificates name (the table at 0x81088DE8) */
#define MLN_X509_ALGORITHM_SHA1_RSA	1	/* 1.2.840.113549.1.1.5 */
#define MLN_X509_ALGORITHM_SHA256_RSA	2	/* 1.2.840.113549.1.1.11 */
#define MLN_X509_ALGORITHM_RSA		3	/* 1.2.840.113549.1.1.1 */

/* an AlgorithmIdentifier: the algorithm, its OID and its parameters' encoding */
typedef struct MlnX509Algorithm {
	int algorithm;			/* MLN_X509_ALGORITHM_*, 0 for another */
	MlnBuffer oid;
	MlnBuffer parameters;
} MlnX509Algorithm;

/* a SubjectPublicKeyInfo: the key's algorithm and its bits */
typedef struct MlnX509PublicKey {
	MlnX509Algorithm algorithm;
	MlnBuffer key;
} MlnX509PublicKey;

/* the extensions the certificates name (the table at 0x81088E18) */
#define MLN_X509_EXTENSION_AUTHORITY_KEY_IDENTIFIER	1	/* 2.5.29.35 */
#define MLN_X509_EXTENSION_SUBJECT_KEY_IDENTIFIER	2	/* 2.5.29.14 */
#define MLN_X509_EXTENSION_KEY_USAGE			3	/* 2.5.29.15 */
#define MLN_X509_EXTENSION_BASIC_CONSTRAINTS		4	/* 2.5.29.19 */
#define MLN_X509_EXTENSION_CRL_DISTRIBUTION_POINTS	5	/* 2.5.29.31 */
#define MLN_X509_EXTENSION_CERTIFICATE_POLICIES		6	/* 2.5.29.32 */
#define MLN_X509_EXTENSION_CRL_NUMBER			7	/* 2.5.29.20 */
#define MLN_X509_EXTENSION_CERTIFICATE_ISSUER		8	/* 2.5.29.29 */
#define MLN_X509_EXTENSION_ISSUING_DISTRIBUTION_POINT	9	/* 2.5.29.28 */

/* an Extension: its OID (and the algorithm table's id for it), criticality and value */
typedef struct MlnX509Extension {
	int id;
	MlnBuffer oid;
	int critical;
	MlnBuffer value;
} MlnX509Extension;

typedef struct MlnX509Extensions {
	int count;
	MlnX509Extension **items;
} MlnX509Extensions;

typedef struct MlnX509Validity {
	MlnDateTime notBefore;
	MlnDateTime notAfter;
} MlnX509Validity;

/* a TBSCertificate; names are kept as their DER encodings */
typedef struct MlnX509Tbs {
	MlnBuffer encoding;		/* the signed bytes */
	int version;			/* 2: v3 */
	MlnBuffer serialNumber;
	MlnX509Algorithm signature;
	MlnBuffer issuer;
	MlnX509Validity validity;
	MlnBuffer subject;
	MlnX509PublicKey publicKey;
	MlnX509Extensions *extensions;
} MlnX509Tbs;

/* a Certificate */
typedef struct MlnX509Certificate {
	MlnX509Tbs *tbs;
	MlnX509Algorithm signatureAlgorithm;
	MlnBuffer signature;
} MlnX509Certificate;

/* a revokedCertificates entry of a CRL */
typedef struct MlnX509CrlEntry {
	MlnBuffer serialNumber;
	MlnDateTime revocationDate;
	MlnBuffer certificateIssuer;	/* the CRL's issuer unless the entry names one */
} MlnX509CrlEntry;

typedef struct MlnX509CrlEntries {
	int count;
	MlnX509CrlEntry **items;
} MlnX509CrlEntries;

/* an IssuingDistributionPoint, its distributionPoint not kept */
typedef struct MlnX509CrlIssuingDistributionPoint {
	int onlyContainsUserCerts;
	int onlyContainsCACerts;
	unsigned char onlySomeReasons[2];
	int onlySomeReasonsBits;
	int indirectCRL;
	int onlyContainsAttributeCerts;
} MlnX509CrlIssuingDistributionPoint;

/* the crlExtensions the library understands */
typedef struct MlnX509CrlExtensions {
	MlnBuffer authorityKeyIdentifier;
	MlnX509CrlNumber crlNumber;
	MlnX509CrlIssuingDistributionPoint issuingDistributionPoint;
} MlnX509CrlExtensions;

/* a TBSCertList with the extensions it understands */
typedef struct MlnX509CrlTbs {
	MlnBuffer encoding;		/* the signed bytes */
	int version;
	MlnX509Algorithm signature;
	MlnBuffer issuer;
	MlnDateTime thisUpdate;
	MlnDateTime nextUpdate;
	MlnX509CrlEntries *revokedCertificates;
	MlnX509CrlExtensions extensions;
} MlnX509CrlTbs;

/* a CertificateList */
typedef struct MlnX509CrlCertificate {
	MlnX509CrlTbs *tbs;
	MlnX509Algorithm signatureAlgorithm;
	MlnBuffer signature;
} MlnX509CrlCertificate;

/* a parsed CRL and its DER */
typedef struct MlnX509Crl {
	MlnBuffer *der;
	MlnX509CrlCertificate *certificate;
} MlnX509Crl;

/* the CRL the library checks certificates against (MlnShared's buffers[3]) */
typedef struct MlnX509CrlStore {
	MlnX509Crl *crl;
} MlnX509CrlStore;

/* copies of buffers (certificates' DER, policy OIDs), room for capacity of them */
typedef struct MlnBufferList {
	int count;
	int capacity;
	MlnBuffer **items;
} MlnBufferList;

/* the certificates of a path, the trust anchor's child first */
typedef struct MlnX509Certificates {
	int count;
	MlnBuffer **items;
} MlnX509Certificates;

/* a list of copies of count buffers, in the reverse order (credentials.c) */
int MlnImpl_81064614(int count, MlnBuffer **items, MlnX509Certificates **out);

/* an RSA public key */
typedef struct MlnX509RsaKey {
	int algorithm;			/* MLN_X509_ALGORITHM_RSA */
	MlnBuffer modulus;
	MlnBuffer exponent;
} MlnX509RsaKey;

/* what a path is validated against */
typedef struct MlnX509ValidationParameters {
	int depth;			/* the certificates in the path */
	int checkValidity;		/* 1: each certificate valid at time */
	MlnDateTime time;
	MlnBuffer anchorName;		/* the trust anchor's subject, DER */
	MlnX509RsaKey anchorKey;
	int policyCount;
	MlnBuffer *policies;		/* the acceptable policy OIDs */
	unsigned int keyUsage;		/* MLN_X509_KEY_USAGE_*: the end certificate's needed usage */
	int checkRevocation;
} MlnX509ValidationParameters;

/* the state of a path validation (RFC 5280 6.1) */
typedef struct MlnX509Validation {
	int checkValidity;
	int checkRevocation;
	MlnDateTime time;
	int depth;
	MlnBufferList *userPolicies;
	unsigned int keyUsage;
	int maxPathLength;
	MlnBufferList *validPolicies;
	int index;			/* the certificate being processed, from 1 */
	MlnBuffer *workingIssuer;
	MlnBuffer *modulus;		/* the working public key */
	MlnBuffer *exponent;
	MlnBuffer *keyIdentifier;	/* the issuer's subject key identifier */
} MlnX509Validation;

typedef struct MlnX509BasicConstraints {
	int ca;
	int pathLength;			/* -1: no constraint */
} MlnX509BasicConstraints;

#define MLN_X509_KEY_USAGE_KEY_CERT_SIGN	0x20

/* a certificate with its own copy of the DER it was parsed from */
typedef struct MlnX509 {
	MlnBuffer *der;
	MlnX509Certificate *certificate;
} MlnX509;

int MlnImpl_81038718(const MlnX509PublicKey *publicKey, MlnBuffer **modulus,
		     MlnBuffer **exponent);
int MlnImpl_81038874(const MlnAsn1Node *node, MlnX509PublicKey *publicKey);
int MlnImpl_81038900(const MlnBuffer *der, MlnX509Certificate **out);
int MlnImpl_81038DB4(int capacity, MlnBufferList **out);
int MlnImpl_81038E90(MlnX509 *x509);
int MlnImpl_81038EF4(const MlnBuffer *der, MlnX509 **out);
int MlnImpl_81039000(const MlnBuffer *der, MlnBuffer **modulus, MlnBuffer **exponent);
int MlnImpl_810390C0(const MlnX509 *x509, MlnBuffer **out);
int MlnImpl_81039228(const MlnX509 *x509, MlnBuffer **modulus, MlnBuffer **exponent);
int MlnImpl_81039298(const MlnX509 *x509, MlnBuffer **out);
int MlnImpl_81039418(const MlnX509ValidationParameters *parameters,
		     const MlnX509Certificates *certificates);
int MlnImpl_8103A888(const MlnX509Certificates *certificates, MlnBuffer **out);
int MlnImpl_8103AA04(const MlnAsn1Node *node, MlnX509Algorithm *algorithm);
int MlnImpl_8103AB18(const MlnAsn1Node *node, MlnDateTime *time);
int MlnImpl_8103AC08(const MlnAsn1Node *node, MlnBuffer *name);
int MlnImpl_8103AC34(MlnX509Extensions *extensions);
int MlnImpl_8103AC98(const MlnAsn1Node *node, MlnX509Extensions **out);
int MlnImpl_8103AFFC(const MlnBuffer *value, MlnBuffer *keyIdentifier, int heap);
int MlnImpl_8103B0C4(const MlnDateTime *earlier, const MlnDateTime *later);
int MlnImpl_8103B1E0(const MlnBuffer *serialNumber, const MlnBuffer *issuer);
void MlnImpl_8105725C(MlnX509ValidationParameters *parameters, int anchor);
int MlnImpl_81057314(int purpose, const MlnX509Certificates *certificates);
int MlnImpl_810583CC(const MlnBuffer *der, MlnX509Crl **crl);
/* the trust anchors by the platform mode (0x81058CA4-) */
const unsigned char *MlnImpl_81058CA4(unsigned int anchor);
const unsigned char *MlnImpl_81058CFC(unsigned int anchor);
const unsigned char *MlnImpl_81058D58(unsigned int anchor);
unsigned int MlnImpl_81058D90(unsigned int anchor);
unsigned int MlnImpl_81058DEC(unsigned int anchor);
unsigned int MlnImpl_81058E20(unsigned int anchor);
int MlnImpl_81058AD0(MlnX509Crl *crl);
int MlnImpl_81058B80(const MlnX509Crl *crl, int *current);
int MlnImpl_81058C0C(const MlnX509CrlNumber *a, const MlnX509CrlNumber *b, int *order);
/* RSA (0x81055670-) */
int MlnImpl_81055670(void **rsa, const MlnBuffer *modulus, const MlnBuffer *publicExponent,
		     const MlnBuffer *privateExponent, const MlnBuffer *prime1,
		     const MlnBuffer *prime2, const MlnBuffer *exponent1, const MlnBuffer *exponent2,
		     const MlnBuffer *coefficient);
int MlnImpl_81055ABC(void *rsa);
int MlnImpl_81055B3C(void **rsa, MlnBuffer *modulus, MlnBuffer *publicExponent);
int MlnImpl_81055BB4(void **values, MlnBuffer *modulus, MlnBuffer *publicExponent,
		     MlnBuffer *privateExponent, MlnBuffer *prime1, MlnBuffer *prime2,
		     MlnBuffer *exponent1, MlnBuffer *exponent2, MlnBuffer *coefficient);
int MlnImpl_81055CA8(void *rsa);
unsigned int MlnImpl_81056A74(void *rsa, int hash, const MlnBuffer *in, MlnBuffer **out);
int MlnImpl_81056F0C(void *rsa, int hash, const MlnBuffer *message, const MlnBuffer *signature);
int MlnImpl_81034D38(const char *usage, int count, MlnBuffer **certificates, char **out);
int MlnImpl_81035018(int encryptionCount, MlnBuffer **encryptionCertificates, int signingCount,
		     MlnBuffer **signingCertificates, const MlnBuffer *role,
		     const MlnBuffer *nodeId, char **out);
int MlnImpl_810368D0(int heap, MlnAsn1Node *node, unsigned char **data, unsigned int *size);
int MlnImpl_81081890(MlnXmlNode *node, const char *name, const char *ns, MlnXmlNodeList **list);
int MlnImpl_81081BB4(MlnXmlNodeList **list, MlnConfigList *out, const char *role);

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
