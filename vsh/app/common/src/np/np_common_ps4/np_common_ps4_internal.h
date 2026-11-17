/*
 * SceNpCommonPs4: the library's own types and the exports the library calls itself. Sony's names
 * for them are not known: the exports are named by their NID, the internal names are ours, taken
 * from what the code does.
 */
#ifndef NP_COMMON_PS4_INTERNAL_H
#define NP_COMMON_PS4_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <libhttp.h>
#include "np_common_ps4.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the errors the library returns (0x8055800X: the library's own; Sony's names for them are not
   known) */
#define NP_COMMON_PS4_ERROR_80558001	-2141880319
#define NP_COMMON_PS4_ERROR_80558002	-2141880318
#define NP_COMMON_PS4_ERROR_80558003	-2141880317
#define NP_COMMON_PS4_ERROR_80558006	-2141880314
#define NP_COMMON_PS4_ERROR_80559E03	-2141872637
#define NP_COMMON_PS4_ERROR_80559E0C	-2141872628
#define NP_COMMON_PS4_ERROR_80559E0D	-2141872627
#define NP_COMMON_PS4_ERROR_80559E0E	-2141872626
#define NP_COMMON_PS4_ERROR_80559E10	-2141872624
#define NP_COMMON_PS4_ERROR_80559E11	-2141872623
#define NP_COMMON_PS4_ERROR_80559E09	-2141872631
#define NP_COMMON_PS4_ERROR_80559E12	-2141872622
#define NP_COMMON_PS4_ERROR_80559E13	-2141872621
#define NP_COMMON_PS4_ERROR_80559E2D	-2141872595
#define NP_COMMON_PS4_ERROR_80559E2E	-2141872594
#define NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT	-2141911551	/* 0x80550601 */
#define NP_COMMON_PS4_ERROR_JSON_INVALID_STRING	-2141911550	/* 0x80550602 */
#define NP_COMMON_PS4_ERROR_80550687	-2141911417
#define NP_COMMON_PS4_ERROR_80550689	-2141911415
#define NP_COMMON_PS4_ERROR_80559F7F	-2141872385
#define NP_COMMON_PS4_ERROR_80559E14	-2141872620
#define NP_COMMON_PS4_ERROR_80559E16	-2141872618
#define NP_COMMON_PS4_ERROR_80558004	-2141880316
#define NP_COMMON_PS4_ERROR_INVALID_ARGUMENT	-2141880315	/* 0x80558005 */
#define NP_COMMON_PS4_ERROR_OUT_OF_MEMORY	-2141880313	/* 0x80558007 */
#define NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL	-2141880310	/* 0x80558006 */
#define NP_COMMON_PS4_ERROR_80558008	-2141880312
#define NP_COMMON_PS4_ERROR_80558009	-2141880311
#define NP_COMMON_PS4_ERROR_8055800B	-2141880309
#define NP_COMMON_PS4_ERROR_TIMEOUT	-2141880308	/* 0x8055800c */
#define NP_COMMON_PS4_ERROR_8055800D	-2141880307
#define NP_COMMON_PS4_ERROR_8055800E	-2141880306
#define NP_COMMON_PS4_ERROR_BUSY	-2141880305	/* 0x8055800f */

/* an allocator: three functions and the context they take as their last argument */
struct NpCommonPs4Allocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
};

/* the character classes of sceClibLookCtypeTable */
#define CTYPE_ALPHA	3
#define CTYPE_DIGIT	4
#define CTYPE_SPACE	8

/* npext/memory/memory.cpp: an object that counts its references */
struct NpCommonPs4RefCounted {
	const void *const *vtable;
	volatile SceUInt32 refCount;
};

/* npext/memory/ring_buf.cpp: a ring buffer over a block of memory, and the one that owns its block */
struct NpCommonPs4RingBuf {
	const void *const *vtable;
	SceUInt8 *buf;
	SceUInt8 *rp;
	SceUInt8 *wp;
	SceSize size;
	SceSize bufSize;
};

struct NpCommonPs4RingBufAlloc {
	struct NpCommonPs4RingBuf ringBuf;
	struct NpCommonPs4Allocator *pool;
};

/* npext/np/communication_id.cpp: a SceNpCommunicationId (NPWR12345_00) behind a vtable */
struct NpCommonPs4CommunicationId {
	const void *const *vtable;
	char data[9];
	char term;
	SceUInt8 num;
	char dummy[1];
};

int SceNpCommon_5BC54899(void *communicationId, const char *string);
int SceNpCommon_E426FEE8(void *communicationId, char *buffer, SceSize size);

/* the same ids without the vtable: the layouts of SceNpCommunicationId, SceNpOnlineId and SceNpTitleId */
struct NpCommonPs4CommunicationIdData {
	char data[9];
	char term;
	SceUInt8 num;
	char dummy[1];
};

struct NpCommonPs4OnlineIdData {
	char data[16];
	char term;
	char dummy[3];
};

struct NpCommonPs4TitleIdData {
	char id[13];
	char padding[3];
};

/* npext/np/online_id.cpp: a SceNpOnlineId (16 characters, terminator, padding) behind a vtable */
struct NpCommonPs4OnlineId {
	const void *const *vtable;
	char data[16];
	char term;
	char dummy[3];
};

/* npext/np/title_id.cpp: a SceNpTitleId (NPXX12345_00 and its terminator, padding) behind a vtable */
struct NpCommonPs4TitleId {
	const void *const *vtable;
	char id[13];
	char padding[3];
};

/* npext/stream/stream_reader.cpp: a reader of bytes and the observer of what it reads */
struct NpCommonPs4StreamReader {
	const void *const *vtable;
};

struct NpCommonPs4StreamObserver {
	const void *const *vtable;
};

int SceNpCommon_8C21F489(const void *left, const void *right);

/* the build Sony's assert paths name: one per firmware */
#if defined(FW_TYPE_CEX)
#define NP_COMMON_PS4_BUILD "J01509657"
#elif FW_VERSION == 0x03630011
#define NP_COMMON_PS4_BUILD "J01508051"
#else
#define NP_COMMON_PS4_BUILD "J01509658"
#endif
#define NP_COMMON_PS4_SOURCE(path) "W:\\Build\\" NP_COMMON_PS4_BUILD "\\vsh\\app\\common\\src\\np\\np_common_ps4\\" path

/* a vtable object starts with two words (offset to top, type information); the object's vtable pointer points past them */
#define NP_COMMON_PS4_VTABLE(table) (&(table)[2])

/* Sony's assert: prints where it failed, then stops (SceNpCommonPs4_F50BD96A) */
#define NP_COMMON_PS4_ASSERT(condition, function, file, line, text) \
	do { \
		if (!(condition)) \
			SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", function, \
				NP_COMMON_PS4_SOURCE(file), line, "ASSERT", text)); \
	} while (0)

/* a mutex of the library: a kernel mutex behind a vtable */
struct NpCommonPs4Mutex {
	const void *const *vtable;
	SceUID mutexId;
	SceUInt8 initialized;
};

/* npext/handle/cancelable.cpp: a handle that can be cancelled, with the handles above and below it */
struct NpCommonPs4Cancelable {
	const void *const *vtable;
	struct NpCommonPs4Mutex mutex;
	int cancelErrcode;
	int cancelFlags;
	SceUInt8 cancelable;
	int lockCount;
	struct NpCommonPs4Cancelable *subCancel;
	struct NpCommonPs4Cancelable *parentCancel;
};

/* npext/handle/handle.cpp, scope.cpp: a reference counted handle that is a Cancelable, and the scope that attaches to one */
struct NpCommonPs4Handle {
	const void *const *vtable;
	struct NpCommonPs4Cancelable cancelable;
	int refCount;
};

struct NpCommonPs4Scope {
	struct NpCommonPs4Cancelable cancelable;
	struct NpCommonPs4Handle *handle;
};

/* npext/http: an HTTP template, a transaction (connection and request) and the connection pool it takes
 * connections from */
struct NpCommonPs4HttpTemplate {
	const void *const *vtable;
	SceInt32 templateId;
};

struct NpCommonPs4HttpTimeouts {
	SceUInt32 connectTimeout;
	SceUInt32 recvTimeout;
	SceUInt32 sendTimeout;
};

/* npext/http/http_conn_pool.cpp: a connection of the pool (kept for reuse while inUse is 0), and the
   pool: a list of connections, at most connNumMax of them */
struct NpCommonPs4HttpConn {
	const void *const *vtable;
	struct NpCommonPs4AllocatorWithError *pool;
	SceInt32 connId;
	SceInt32 templateId;
	char *scheme;
	char *host;
	SceUShort16 port;
	SceInt32 inUse;
	SceUInt8 invalid;
	SceInt32 reportedError;
	struct NpCommonPs4HttpConn *next;
	struct NpCommonPs4HttpConn *prev;
};

struct NpCommonPs4HttpConnPool {
	const void *const *vtable;
	struct NpCommonPs4AllocatorWithError *pool;
	SceInt32 connNumMax;
	SceInt32 connNum;
	struct NpCommonPs4HttpConn *head;
	struct NpCommonPs4HttpConn *tail;
	struct NpCommonPs4Mutex mutex;
};

struct NpCommonPs4HttpTrans {
	const void *const *vtable;
	const void *const *vtableSecondBase;
	struct NpCommonPs4Allocator *pool;
	struct NpCommonPs4HttpConnPool *connPool;
	SceInt32 connId;
	SceInt32 reqId;
	SceUInt64 receivedSize;
};

/* npext/http/http_uri.cpp: a parsed URI whose elements point into one buffer */
struct NpCommonPs4HttpUri {
	const void *const *vtable;
	SceUInt8 opaque;
	char *scheme;
	char *username;
	char *password;
	char *hostname;
	char *path;
	char *query;
	char *fragment;
	SceUShort16 port;
	struct NpCommonPs4Allocator *pool;
	void *buffer;
};

struct NpCommonPs4HttpUri *SceNpCommonPs4_EF053155(struct NpCommonPs4HttpUri *uri, struct NpCommonPs4Allocator *pool);
struct NpCommonPs4HttpUri *SceNpCommonPs4_349710F4(struct NpCommonPs4HttpUri *uri);
int SceNpCommonPs4_A9EAFF5E(struct NpCommonPs4HttpUri *uri, const char *srcUrl);

/* the connection pool (npext/http/http_conn_pool.cpp) */
int npCommonPs4HttpConnPoolGetConnection(struct NpCommonPs4HttpConnPool *connPool, struct NpCommonPs4HttpTemplate *tmpl, const char *scheme,
	const char *serverName, SceUShort16 port, SceInt32 *pConnId);
int npCommonPs4HttpConnPoolGetConnectionWithUrl(struct NpCommonPs4HttpConnPool *connPool, struct NpCommonPs4HttpTemplate *tmpl, const char *url,
	SceInt32 *pConnId);
struct NpCommonPs4HttpConnPool *npCommonPs4HttpConnPoolConstruct(struct NpCommonPs4HttpConnPool *connPool,
	struct NpCommonPs4AllocatorWithError *pool);
struct NpCommonPs4HttpTemplate *npCommonPs4HttpTemplateConstruct(struct NpCommonPs4HttpTemplate *tmpl);
int npCommonPs4HttpTemplateInit(struct NpCommonPs4HttpTemplate *tmpl, int libHttpCtxId, const char *userAgent,
	SceInt32 httpVer, SceUInt8 autoProxyConf);
int npCommonPs4HttpConnPoolInit(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connNumMax);
int npCommonPs4HttpConnPoolRelease(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connId);
int npCommonPs4HttpConnPoolReportError(struct NpCommonPs4HttpConnPool *connPool, SceInt32 connId, int error);

/* npext/http/http_context.cpp: the HTTP template and connection pool the NP requests are made with */
struct NpCommonPs4HttpContext {
	const void *const *vtable;
	struct NpCommonPs4AllocatorWithError *pool;
	struct NpCommonPs4HttpTemplate *tmpl;
	struct NpCommonPs4HttpConnPool *connPool;
	SceInt32 libHttpCtxId;
};

/* npext_npm/np_trans.cpp: an HttpTrans that belongs to an NP user (the online id the tokens are made for) */
struct NpCommonPs4NpTrans {
	struct NpCommonPs4HttpTrans trans;
	struct NpCommonPs4OnlineId onlineId;
};

/* npext_npm/np_token.cpp: the tokens the NP server requests carry (data is a string) */
struct NpCommonPs4UserToken {
	const void *const *vtable;
	SceUInt32 reserved;
	char data[264];
};

struct NpCommonPs4TitleToken {
	const void *const *vtable;
	SceUInt32 reserved;
	char data[1040];
};

int SceNpCommonPs4_47443DCE(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, void *buffer,
	SceSize size, SceSize *pReceived);
int SceNpCommonPs4_F81085EE(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, const void *data,
	SceSize size, SceSize *pSent);
int SceNpCommonPs4_8EBA83E1(char *secondBase, struct NpCommonPs4Handle *handle, const void *data, SceSize size,
	SceSize *pSent);
/* npext_npm/np_service_base_url/get_service_base_url_trans.cpp: a string behind a vtable, allocated from a pool */
struct NpCommonPs4StringHolder {
	const void *const *vtable;
	char *string;
	struct NpCommonPs4AllocatorWithError *pool;
};

struct NpCommonPs4NpTrans *SceNpCommonPs4_3FC6688C(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Allocator *pool,
	const struct NpCommonPs4OnlineId *onlineId);
struct NpCommonPs4NpTrans *SceNpCommonPs4_47D94EF0(struct NpCommonPs4NpTrans *npTrans);
int SceNpCommonPs4_060EB06B(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle);
int SceNpCommonPs4_57B14D22(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle);
int SceNpCommonPs4_49776226(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle);
int SceNpCommonPs4_18DDD712(struct NpCommonPs4NpTrans *npTrans, struct NpCommonPs4Handle *handle);
struct NpCommonPs4StringHolder *SceNpCommonPs4_FDA863C0(struct NpCommonPs4StringHolder *holder,
	struct NpCommonPs4AllocatorWithError *pool);
int SceNpCommonPs4_B0F7DE6E(struct NpCommonPs4StringHolder *holder, const char *string);
const char *SceNpCommonPs4_2CE76BE5(const struct NpCommonPs4StringHolder *holder);
int npCommonPs4NpEnvironmentGet(char *environment);
int sceNpCommonGetNpEnviroment(char *environment);
int SceNpCommonPs4_44FF13E8(struct NpCommonPs4HttpTrans *trans, const char *name, const char *value);
int SceNpCommonPs4_6B6E813E(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, SceInt32 *pStatusCode);
int SceNpCommonPs4_C80B0275(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, void *buffer,
	SceSize size, SceSize *pReceived);
int SceNpCommonPs4_3FC1A88F(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Handle *handle, const void *data,
	SceSize size);
int SceNpCommonPs4_D30A8E84(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4HttpTemplate *tmpl,
	struct NpCommonPs4HttpConnPool *connPool, SceInt32 method, const char *url, SceULong64 contentLength);
struct NpCommonPs4HttpTrans *SceNpCommonPs4_9256CF02(struct NpCommonPs4HttpTrans *trans, struct NpCommonPs4Allocator *pool);
struct NpCommonPs4HttpTrans *SceNpCommonPs4_47C5936B(struct NpCommonPs4HttpTrans *trans);
struct NpCommonPs4OnlineId *SceNpCommonPs4_AEC0DAC5(struct NpCommonPs4OnlineId *onlineId, const struct NpCommonPs4OnlineId *source);
struct NpCommonPs4OnlineId *SceNpCommonPs4_1543D7EE(struct NpCommonPs4OnlineId *onlineId);
struct NpCommonPs4UserToken *SceNpCommonPs4_988DFD87(struct NpCommonPs4UserToken *token);
struct NpCommonPs4UserToken *SceNpCommonPs4_78CC9FCD(struct NpCommonPs4UserToken *token);
struct NpCommonPs4TitleToken *SceNpCommonPs4_DC4787E9(struct NpCommonPs4TitleToken *token);
struct NpCommonPs4TitleToken *SceNpCommonPs4_FD80FE40(struct NpCommonPs4TitleToken *token);
const char *SceNpCommonPs4_80D16974(const struct NpCommonPs4TitleToken *token);
int SceNpCommonPs4_191AEA9E(struct NpCommonPs4Handle *handle, struct NpCommonPs4OnlineId *onlineId, struct NpCommonPs4UserToken *token);
int SceNpCommonPs4_9BC491E3(struct NpCommonPs4Handle *handle, const char *onlineId, struct NpCommonPs4TitleToken *token);
int SceNpManager_737B2820(SceInt32 id);
int SceNpCommonPs4_639086A3(SceInt32 reserved);
int SceNpCommonPs4_530CAFDA(SceInt32 id);
int SceNpCommonPs4_13798894(SceInt32 id, const char *onlineId, void *buffer, SceUInt8 reserved);
int SceNpCommonPs4_68D08C92(SceInt32 id);
int SceNpManager_4110B787(SceInt32 *pId);
int SceNpManager_FDE78C74(SceInt32 id, void *buffer);
int SceNpManager_3907F96A(SceInt32 id);
int SceNpManager_F66A7AAE(void);

/* npext/json: a JSON value (type: null, boolean, number, string, array, object), its parser and its document */
#define NP_COMMON_PS4_JSON_TYPE_NULL	0
#define NP_COMMON_PS4_JSON_TYPE_BOOL	1
#define NP_COMMON_PS4_JSON_TYPE_NUMBER	2
#define NP_COMMON_PS4_JSON_TYPE_STRING	3
#define NP_COMMON_PS4_JSON_TYPE_ARRAY	4
#define NP_COMMON_PS4_JSON_TYPE_OBJECT	5

struct NpCommonPs4JsonValue {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
	struct NpCommonPs4JsonValue *parent;
	SceInt32 type;
	void *data;
};

/* where the document builder writes: the buffer is optional (a null buffer only counts) */
struct NpCommonPs4JsonTree {
	const void **vtable;
	struct NpCommonPs4JsonValue *root;
};

struct NpCommonPs4JsonWriter {
	char *buffer;
	SceSize size;
	SceSize length;
};

/* the parser (npext/json/json_parser.cpp): a lexer that reports to an event handler */
struct NpCommonPs4JsonParser {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
	unsigned char lexer[1856];
	char *buffer;
	SceSize bufferSize;
	const void *const *const *handler;
};

struct NpCommonPs4JsonDocument {
	const void *const *vtable;
	const void *const *handlerVtable;
	struct NpCommonPs4Allocator *pool;
	void *config;
	struct NpCommonPs4JsonParser *parser;
	struct NpCommonPs4JsonValue *root;
	struct NpCommonPs4JsonValue *current;
	char *name;
};

struct NpCommonPs4JsonObject {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
};


/* the values' containers: items of an array are a doubly linked list of nodes, the fields of an object a list of
   named fields */
struct NpCommonPs4JsonListNode {
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonListNode *next;
	struct NpCommonPs4JsonListNode *prev;
};

struct NpCommonPs4JsonItemList {
	struct NpCommonPs4JsonListNode *head;
	struct NpCommonPs4JsonListNode *tail;
};

struct NpCommonPs4JsonItemArray {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
	struct NpCommonPs4JsonItemList list;
};

struct NpCommonPs4JsonField {
	char *name;
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonField *next;
	struct NpCommonPs4JsonField *prev;
};

struct NpCommonPs4JsonFieldList {
	struct NpCommonPs4JsonField *head;
	struct NpCommonPs4JsonField *tail;
};

struct NpCommonPs4JsonFieldSet {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
	struct NpCommonPs4JsonFieldList list;
};

/* what a bool value holds, and what a number or a string value holds */
struct NpCommonPs4JsonBoolData {
	const void *const *vtable;
	SceUInt8 value;
};

struct NpCommonPs4JsonTextData {
	const void *const *vtable;
	struct NpCommonPs4Allocator *pool;
	char *text;
};

struct NpCommonPs4JsonValue *SceNpCommonPs4_D309C0B3(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool, int type);
int SceNpCommonPs4_413D4254(struct NpCommonPs4JsonValue *array, struct NpCommonPs4JsonValue *item);
int SceNpCommonPs4_7833EEC2(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue *value);
int SceNpCommonPs4_AE224009(struct NpCommonPs4JsonValue *value, SceUInt8 boolean);
int SceNpCommonPs4_943736E5(struct NpCommonPs4JsonValue *value, const char *number);
int SceNpCommonPs4_CAD8A67F(struct NpCommonPs4JsonValue *value, const char *string);
int SceNpCommonPs4_9C3F1645(struct NpCommonPs4JsonValue *object);
struct NpCommonPs4JsonValue *SceNpCommonPs4_713C8A08(struct NpCommonPs4JsonValue *object, int index, char **pName);
int SceNpCommonPs4_1E21D357(struct NpCommonPs4JsonValue *array);
struct NpCommonPs4JsonValue *SceNpCommonPs4_2A4A268C(struct NpCommonPs4JsonValue *array, int index);
const char *SceNpCommonPs4_ED539A9D(struct NpCommonPs4JsonValue *string);
const char *SceNpCommonPs4_FAE43676(struct NpCommonPs4JsonValue *number);
int SceNpCommonPs4_C1277FAD(struct NpCommonPs4JsonValue *boolean);
struct NpCommonPs4JsonValue *SceNpCommonPs4_15A2D6E3(struct NpCommonPs4JsonValue *value);
struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsBool(struct NpCommonPs4JsonValue *value);
struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsNumber(struct NpCommonPs4JsonValue *value);
struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsString(struct NpCommonPs4JsonValue *value);
struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsArray(struct NpCommonPs4JsonValue *value);
extern const void *g_jsonNullValueVtable[];
extern const void *g_jsonBoolValueVtable[];
extern const void *g_jsonNumberValueVtable[];
extern const void *g_jsonStringValueVtable[];
extern const void *g_jsonArrayValueVtable[];
extern const void *g_jsonObjectValueVtable[];
struct NpCommonPs4JsonParser *SceNpCommonPs4_4577A207(struct NpCommonPs4JsonParser *parser, struct NpCommonPs4Allocator *pool);
struct NpCommonPs4JsonParser *SceNpCommonPs4_4F720AFD(struct NpCommonPs4JsonParser *parser, struct NpCommonPs4Allocator *pool);
struct NpCommonPs4JsonParser *SceNpCommonPs4_0A3AA2EF(struct NpCommonPs4JsonParser *parser);
int SceNpCommonPs4_7CC0800E(struct NpCommonPs4JsonParser *parser, int unused, void *stream);
int SceNpCommonPs4_89322005(struct NpCommonPs4JsonParser *parser, void *config, const void *const *const *eventHandler);
int SceNpCommonPs4_FE98E2A7(struct NpCommonPs4JsonParser *parser, void *data, int size);

/* a block of memory the library allocated, and its size */
void *NpCommonPs4MspaceAllocate(SceSize size, void *context);
void *NpCommonPs4MspaceReallocate(void *pointer, SceSize size, void *context);
void NpCommonPs4MspaceDeallocate(void *pointer, void *context);

struct NpCommonPs4MemBlock {
	SceSize size;
	SceUID uid;
	void *base;
};

/* a heap: how it was made (1: on a given mspace, 2: on given memory, 0: on its own block), its
   block and the mspace on it */
struct NpCommonPs4Heap {
	int state;
	struct NpCommonPs4MemBlock block;
	void *mspace;
};

/* an allocator that also names the error its caller gets when it fails */
struct NpCommonPs4AllocatorWithError {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
	int error;
};

/* what SceNpCommonPs4_A7165D2D reports of a heap */
struct NpCommonPs4HeapStats {
	SceSize capacity;
	SceSize peakInUse;
	SceSize currentInUse;
};

int SceNpCommonPs4_A6C7D48D(struct NpCommonPs4Cancelable *cancelable, int errcode, int flags);
struct NpCommonPs4Cancelable *SceNpCommonPs4_EB1FBB74(struct NpCommonPs4Cancelable *cancelable);
struct NpCommonPs4Cancelable *SceNpCommonPs4_48074E29(struct NpCommonPs4Cancelable *cancelable);
void SceNpCommonPs4_74001725(struct NpCommonPs4Cancelable *cancelable);
struct NpCommonPs4Cancelable *SceNpCommonPs4_C6E3FE21(struct NpCommonPs4Cancelable *cancelable);
int SceNpCommonPs4_2CA5AE6A(struct NpCommonPs4Cancelable *cancelable);
int SceNpCommonPs4_9BCAE348(struct NpCommonPs4Cancelable *cancelable);
int SceNpCommonPs4_3AAA7DF7(struct NpCommonPs4Cancelable *cancelable);
int SceNpCommonPs4_3C8364C3(struct NpCommonPs4Cancelable *cancelable, int arg1, int arg2, int arg3);
int SceNpCommonPs4_E70C78DB(struct NpCommonPs4Cancelable *cancelable, struct NpCommonPs4Cancelable *sub, int arg1, int arg2, int arg3);
int SceNpCommonPs4_6BF7B5BE(struct NpCommonPs4Cancelable *cancelable, struct NpCommonPs4Cancelable *sub);
void npCommonPs4Breakpoint(void);
struct NpCommonPs4Scope *SceNpCommonPs4_37C3D094(struct NpCommonPs4Scope *scope);
struct NpCommonPs4Scope *SceNpCommonPs4_5BBB1ABA(struct NpCommonPs4Scope *scope);
struct NpCommonPs4Scope *SceNpCommonPs4_9486DE09(struct NpCommonPs4Scope *scope);
void SceNpCommonPs4_A3174D9A(struct NpCommonPs4Scope *scope);
struct NpCommonPs4Scope *SceNpCommonPs4_101259AD(struct NpCommonPs4Scope *scope);
int SceNpCommonPs4_DDFD56E9(struct NpCommonPs4Scope *scope, struct NpCommonPs4Handle *handle, int arg1, int arg2, int arg3);
int SceNpCommonPs4_3D0FA7C9(struct NpCommonPs4Scope *scope, int result, int arg1, int arg2, int arg3);
int SceNpCommonPs4_A2A8493B(struct NpCommonPs4Mutex *mutex);	/* lock */
int SceNpCommonPs4_6BA5A164(struct NpCommonPs4Mutex *mutex);	/* unlock */
int SceNpCommonPs4_8AA7E995(struct NpCommonPs4Mutex *mutex, const char *name, char recursive);
int SceNpCommonPs4_63EED478(struct NpCommonPs4Mutex *mutex);
struct NpCommonPs4Mutex *SceNpCommonPs4_294F0B1A(struct NpCommonPs4Mutex *mutex);
struct NpCommonPs4Mutex *SceNpCommonPs4_4266D870(struct NpCommonPs4Mutex *mutex);
struct NpCommonPs4Mutex *SceNpCommonPs4_4C7BD335(struct NpCommonPs4Mutex *mutex);
int SceNpCommonPs4_AB8AE01C(struct NpCommonPs4Mutex *mutex);
int SceNpCommonPs4_E0ED3312(struct NpCommonPs4Mutex *mutex);
struct NpCommonPs4Mutex *SceNpCommonPs4_3FE4AAB6(struct NpCommonPs4Mutex *mutex);
int SceNpCommonPs4_6EA40010(struct NpCommonPs4Mutex *mutex);
int SceNpCommonPs4_2D74359F(SceUID *pMutex);
int SceNpCommonPs4_D37A4868(SceUID *pMutex);
int SceNpCommonPs4_262D1ACE(SceUID *pMutex);
extern const void *g_mutexVtable[];
extern const void *g_baseVtable[];	/* 0x8102a6b0 */
int SceNpCommonPs4_C78F5DF5(struct NpCommonPs4Mutex *mutex);
int SceNpCommonPs4_F56B0EAB(SceInt64 *pTime);
int SceNpCommonPs4_A25DA29A(SceUInt32 *pTime);
int SceNpCommonPs4_93DA3594(const char *data, int length);
void SceNpCommonPs4_0A7044DA(const unsigned char *data, SceSize size);
int SceNpCommonPs4_F50BD96A(int printed);
int SceNpCommonPs4_BB5282C8(void *object);
struct NpCommonPs4Allocator *SceNpCommonPs4_31DB2D97(struct NpCommonPs4Allocator *pool);
void *NpCommonPs4AllocatorReallocate(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size);
int SceNpCommonPs4_AA980113(struct NpCommonPs4Allocator *allocator, void *pointer);
int SceNpCommonPs4_67BBE5F9(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size);
int SceNpCommonPs4_6B428829(struct NpCommonPs4Allocator *allocator, void *pointer);
char *SceNpCommonPs4_1CAB4A2E(struct NpCommonPs4Allocator *allocator, const char *string);
char *npCommonPs4AllocatorDuplicateString(struct NpCommonPs4Allocator *allocator, const char *string);
char *SceNpCommonPs4_83496021(struct NpCommonPs4Allocator *allocator, const char *string, SceSize maxLength);
void *SceNpCommonPs4_299ADFC3(struct NpCommonPs4Allocator *allocator, SceSize size);
void SceNpCommonPs4_38CC9AF5(struct NpCommonPs4Allocator *allocator, void *pointer);
int SceNpCommonPs4_3AE5ABF3(const char *str, SceInt32 *pValue, int base, SceSize *pLength);
int SceNpCommonPs4_D5B30475(const char *str, SceUInt32 *pValue, int base, SceSize *pLength);
int NpCommonPs4StrToInt64(const char *str, SceInt64 *pValue, int base, SceSize *pLength);
int NpCommonPs4StrToUInt64(const char *str, SceUInt64 *pValue, int base, SceSize *pLength);
int NpCommonPs4UIntToString(char *buf, SceSize size, SceUInt32 value, int base, char flags, SceSize *pLength);
int NpCommonPs4Int64ToString(char *buf, SceSize size, SceInt64 value, int base, char flags, SceSize *pLength);
int NpCommonPs4UInt64ToString(char *buf, SceSize size, SceUInt64 value, int base, char flags, SceSize *pLength);
int NpCommonPs4StringCopy(char *dst, SceSize size, const char *src);
int SceNpCommonPs4_9CAF0FE7(char *buf, SceSize size, SceInt32 value, int base, char flags, SceSize *pLength);
void *SceNpCommonPs4_D154D84F(SceSize size, struct NpCommonPs4Allocator *pool);
int SceNpCommonPs4_5BF297C0(void **pMspace, void *memory, SceSize size);
int SceNpCommonPs4_B3C0FE19(void **pMspace, void *mspace);
void *SceNpCommonPs4_1B0517EF(void **pMspace);
int SceNpCommonPs4_A7165D2D(void **pMspace, struct NpCommonPs4HeapStats *stats);
void *SceNpCommonPs4_EB6E37A8(void **pMspace, SceSize size);
void *SceNpCommonPs4_4117508A(void **pMspace, void *pointer, SceSize size);
void SceNpCommonPs4_1514F370(void **pMspace, void *pointer);
void *SceNpCommonPs4_6EF4BA9A(void *context, struct NpCommonPs4Allocator *allocator);
char *SceNpCommonPs4_EBF8CB79(void **pMspace, const char *string);
char *SceNpCommonPs4_8CBF4614(void **pMspace, const char *string, SceSize maxLength);
int SceNpCommonPs4_167D13A4(SceSize size, const char *name, void **pBase, struct NpCommonPs4MemBlock *block);
int SceNpCommonPs4_39208D69(struct NpCommonPs4MemBlock *block);
int SceNpCommonPs4_39CF37A2(struct NpCommonPs4Heap *heap, SceSize size, const char *name);
int SceNpCommonPs4_87E42067(struct NpCommonPs4Heap *heap, void *mspace);
void *SceNpCommonPs4_C52C787A(struct NpCommonPs4Heap *heap);
SceUInt32 SceNpCommonPs4_741F88FE(volatile SceUInt32 *counter);
SceUInt32 SceNpCommonPs4_7B512A98(volatile SceUInt32 *counter);
int SceNpCommonPs4_221CC7EE(struct NpCommonPs4RingBuf *ringBuf, void *buf, SceSize bufSize);
void *SceNpCommonPs4_D7E49888(struct NpCommonPs4RingBuf *ringBuf, SceSize *pBlockSize);
SceUInt32 SceNpCommonPs4_7AF729FD(struct NpCommonPs4RingBuf *ringBuf, SceSize size);

/* the SceNpCommon event loop (SceNpCommon_9AEF645B and the calls below), its due entries, and a thread of our own
   that does the same with a list sorted by due time (npext/thread/timer.cpp, callback.cpp) */
int SceNpCommon_48314C6E(void *loop);
int SceNpCommon_9AEF645B(void *loop, const char *name, SceSize stackSize, int priority, int cpuAffinity);
int SceNpCommon_BDCD318E(void *loop, void *entry, int *pRemoved);
int SceNpCommon_40724BEA(void *loop, void *entry, SceUInt32 time, int reserved, int (*callback)(void *), void *argument);

struct NpCommonPs4TimerEntry {
	struct NpCommonPs4TimerEntry *next;
	int (*callback)(void *argument);
	SceInt64 dueTime;
	void *argument;
	SceUInt32 reserved;
};

struct NpCommonPs4TimerThread {
	SceUID thread;
	SceUID mutex;
	SceUID cond;
	int running;
	int stopping;
	struct NpCommonPs4TimerEntry *head;
};

int npCommonPs4TimerThreadStart(struct NpCommonPs4TimerThread *timer, const char *name, SceSize stackSize,
	int priority, int cpuAffinity);
int npCommonPs4TimerThreadStop(struct NpCommonPs4TimerThread *timer);
int npCommonPs4TimerThreadAdd(struct NpCommonPs4TimerThread *timer, struct NpCommonPs4TimerEntry *entry,
	SceUInt32 delay, int (*callback)(void *argument), void *argument);
int npCommonPs4TimerThreadRemove(struct NpCommonPs4TimerThread *timer, struct NpCommonPs4TimerEntry *entry,
	int *pRemoved);

/* how a host is started: its thread's name, stack size, priority and CPU affinity */
struct NpCommonPs4HostParam {
	const char *name;
	SceSize stackSize;
	int priority;
	int cpuAffinity;
};

/* what a job runs: any object whose third virtual function is the work */
struct NpCommonPs4JobTarget {
	const void *const *vtable;
};

/* a host that runs timed jobs on a timer thread of its own */
struct NpCommonPs4TimerHost {
	const void *const *vtable;
	struct NpCommonPs4TimerThread timer;
	SceUInt8 active;
	SceUInt8 borrowed;
};

/* a host that runs them on a SceNpCommon event loop, its own (loopData) or one it was given */
struct NpCommonPs4EventLoopHost {
	const void *const *vtable;
	void *loop;
	SceUInt32 loopData[6];
	SceUInt8 active;
	SceUInt8 borrowed;
};

/* a timed job: the entry the host's list holds, and what runs when it is due; state is 0 (idle), -1 (waiting), -2
   (running) or 1 (done) */
struct NpCommonPs4Job {
	const void *const *vtable;
	void *host;
	int state;
	SceUInt32 reserved;
	struct NpCommonPs4TimerEntry entry;
	struct NpCommonPs4JobTarget *target;
};

/* a kernel event flag, created or opened by name, behind a vtable (state: 0 none, 1 created, 2 opened) */
struct NpCommonPs4EventFlag {
	const void *const *vtable;
	SceUID eventFlag;
	SceUInt32 state;
};

/* a condition variable behind a vtable, over the mutex of another object (npext/thread/cond.cpp) */
struct NpCommonPs4Cond {
	const void *const *vtable;
	SceUID cond;
	SceUID *mutex;
};

struct NpCommonPs4Cond *npCommonPs4CondConstruct(struct NpCommonPs4Cond *cond);
struct NpCommonPs4Cond *npCommonPs4CondDestruct(struct NpCommonPs4Cond *cond);
int npCommonPs4CondIsInit(struct NpCommonPs4Cond *cond);
int npCommonPs4CondInit(struct NpCommonPs4Cond *cond, const char *name, struct NpCommonPs4Mutex *mutex);
int npCommonPs4CondDestroy(struct NpCommonPs4Cond *cond);
int npCommonPs4CondWait(struct NpCommonPs4Cond *cond, SceUInt32 timeout);
int npCommonPs4CondSignal(struct NpCommonPs4Cond *cond);
int npCommonPs4CondBroadcast(struct NpCommonPs4Cond *cond);

/* a kernel semaphore, created or opened by name, behind a vtable (state: 0 none, 1 created, 2 opened) */
struct NpCommonPs4SemaphoreObject {
	const void *const *vtable;
	SceUID semaphore;
	SceUInt32 state;
};

/* npext/thread/thread.cpp: a thread that runs the object's third virtual function (state: 0 none, 1 init, 2 running,
   3 joined) */
struct NpCommonPs4Thread {
	const void *const *vtable;
	SceUID thread;
	int priority;
	SceSize stackSize;
	char name[32];
	int cpuAffinity;
	int state;
	int exitStatus;
};

int npCommonPs4ThreadMain(SceSize argSize, void *argp);
SceUID SceNpCommonPs4_1C1B177A(SceUID *pSema, int initCount, int maxCount, const char *name);
int SceNpCommonPs4_908EDA3A(SceUID sema);
SceUID SceNpCommonPs4_EB92EB4E(SceUID *pSema, const char *name);
int SceNpCommonPs4_7272B106(SceUID sema);
int SceNpCommonPs4_2387BCED(SceUID sema, SceUInt32 *pTimeout);
int SceNpCommonPs4_364483E2(SceUID sema);
int SceNpCommonPs4_04EC8796(SceUID *pThread, SceKernelThreadEntry entry, SceSize argSize, void *pArgp,
	int initPriority, SceSize stackSize, int cpuAffinity, const char *name);
int SceNpCommonPs4_CFB92A12(SceUID thread, int *pExitStatus);

/* npext/thread/event_queue.cpp: a queue of fixed size items on a ring buffer, with a condition variable each for the
   items and for the room */
struct NpCommonPs4EventQueue {
	const void *const *vtable;
	struct NpCommonPs4RingBufAlloc ringBuf;
	struct NpCommonPs4Mutex mutex;
	struct NpCommonPs4Cond itemCond;
	struct NpCommonPs4Cond spaceCond;
	SceSize itemSize;
	SceSize itemCount;
	SceUInt16 initialized;
	SceUInt16 closedFlags;
};

SceUID SceNpCommonPs4_85B12451(SceUID *pEventFlag, const char *name, SceUInt32 attr, int reserved, SceUInt32 initPattern);
int SceNpCommonPs4_E42B3A09(SceUID eventFlag);
int SceNpCommonPs4_BF4855F2(SceUID *pEventFlag, const char *name);
int SceNpCommonPs4_73E77BE0(SceUID eventFlag);
int SceNpCommonPs4_78905979(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
	SceUInt64 *pResult, SceUInt32 *pTimeout);
int SceNpCommonPs4_2559AAB6(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
	SceUInt64 *pResult);
int SceNpCommonPs4_48C115EA(SceUID eventFlag, int reserved, SceUInt32 bitPattern);
int SceNpCommonPs4_84EA4F2A(SceUID eventFlag, int reserved, SceUInt32 bitPattern);
int SceNpCommonPs4_F2753B27(SceUID eventFlag, int reserved0, SceUInt32 setPattern, int reserved1, SceUInt32 *pNumWaitThreads);
SceUID SceNpCommonPs4_EDBFFFE9(SceUID *pMutex, const char *name, char recursive);
SceUID SceNpCommonPs4_34C15C21(SceUID *pCond, SceUID *pMutex, const char *name);
int SceNpCommonPs4_AC89B1E7(SceUID *pCond);
int SceNpCommonPs4_3D73D77E(SceUID *pCond);
int SceNpCommonPs4_B599A560(struct NpCommonPs4RingBuf *ringBuf);
SceSize SceNpCommonPs4_2A607C9A(struct NpCommonPs4RingBuf *ringBuf);
int SceNpCommonPs4_548EF805(struct NpCommonPs4RingBuf *ringBuf, const char *data, SceSize length);
int SceNpCommonPs4_DC5607D0(struct NpCommonPs4RingBuf *ringBuf, char *dst, SceSize length);
struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_3245630F(struct NpCommonPs4RingBufAlloc *ringBuf, struct NpCommonPs4Allocator *pool);
struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_A264C490(struct NpCommonPs4RingBufAlloc *ringBuf);
int SceNpCommonPs4_530564F5(struct NpCommonPs4RingBufAlloc *ringBuf, SceSize bufSize);
int SceNpCommonPs4_31D856EA(struct NpCommonPs4RingBufAlloc *ringBuf);
int SceNpCommonPs4_E1F6AE0F(struct NpCommonPs4RingBufAlloc *ringBuf);
int SceNpCommonPs4_9AD6061C(char *dst, SceSize dstSize, const char *src, SceSize srcLength);
int SceNpCommonPs4_1A8B2250(char *dst, SceSize dstSize, const char *src, int srcLength, SceSize *pLength);
int SceNpCommonPs4_12AA95E3(void *dst, SceSize dstSize, const char *src, SceSize srcLength, SceSize *pLength, int *pConsumed);
int npCommonPs4JsonParseHex4(const char *src, unsigned short *pCode);
int npCommonPs4JsonEncodeUtf8(unsigned int code, char *dst);
int SceNpCommonPs4_32155BF8(const char *str, const char **pEnd, int base);
int SceNpCommonPs4_C08B6E1C(int language, char *code);
const char *SceNpCommonPs4_D1AB9B23(int language);
int SceNpCommonPs4_FA1249E8(void);
int SceNpCommonPs4_64C85FD5(void);
extern const char *g_languageCodes[22];
/* the error body's lexer context: whether the body carried an error, and its code (-1 when it has none) */
struct NpCommonPs4ServerErrorContext {
	SceUInt8 valid;
	SceInt32 code;
};
/* the lexer (npext/json/json_lexer.c): a JSON text read as it comes, in pieces, and the paths of its objects
   and arrays that a callback is told of. A config names, for each depth, the keys (or array elements) that
   matter; a node of kind 0 ends the depth's list */
#define NP_COMMON_PS4_LEXER_NODE_END	0
#define NP_COMMON_PS4_LEXER_NODE_KEY	1
#define NP_COMMON_PS4_LEXER_NODE_ELEMENT	2

struct NpCommonPs4LexerNode {
	int kind;
	const char *name;
	int flag;
};

struct NpCommonPs4LexerConfig {
	const struct NpCommonPs4LexerNode *const *levels;
	SceSize numLevels;
};

/* what the callback is told of: an object or array begun (1, 3) or ended (2, 4), a key (5), a value (6) */
#define NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN	1
#define NP_COMMON_PS4_LEXER_EVENT_OBJECT_END	2
#define NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN	3
#define NP_COMMON_PS4_LEXER_EVENT_ARRAY_END	4
#define NP_COMMON_PS4_LEXER_EVENT_KEY	5
#define NP_COMMON_PS4_LEXER_EVENT_VALUE	6

/* a value the lexer hands its callback: its type (0 null, 1 true, 2 false, 3 number, 4 string) and its text */
#define NP_COMMON_PS4_LEXER_TOKEN_NULL	0
#define NP_COMMON_PS4_LEXER_TOKEN_TRUE	1
#define NP_COMMON_PS4_LEXER_TOKEN_FALSE	2
#define NP_COMMON_PS4_LEXER_TOKEN_NUMBER	3
#define NP_COMMON_PS4_LEXER_TOKEN_STRING	4

struct NpCommonPs4LexerToken {
	int type;
	char text[1536];
};

struct NpCommonPs4Lexer;
typedef int (*NpCommonPs4LexerCallback)(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);

/* 1856 bytes */
struct NpCommonPs4Lexer {
	const struct NpCommonPs4LexerConfig *config;
	char name[128];			/* the key being read, then the last key */
	struct NpCommonPs4LexerToken token;
	SceSize depth;
	int nodeIndex[16];		/* by depth: the config's node the path matched */
	int arrayIndex[16];		/* by depth: -1 in an object, else the element's index */
	int state;
	SceSize length;			/* of the key or value read so far */
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];		/* the escape or character a piece of text ended in */
	SceSize partialLength;
	SceSize remaining;
	NpCommonPs4LexerCallback callback;
	void *context;
};
/* the lexer of 0x8100f3dc: the key and the value are read into buffers the caller gives, and the callback is
   handed a token of the type and the buffer */
struct NpCommonPs4Lexer3Token {
	int type;
	char *text;
};

struct NpCommonPs4Lexer3;
typedef int (*NpCommonPs4Lexer3Callback)(struct NpCommonPs4Lexer3 *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4Lexer3Token *token, int arrayIndex, void *context);

/* 208 bytes */
struct NpCommonPs4Lexer3 {
	const struct NpCommonPs4LexerConfig *config;
	char *name;
	SceSize nameSize;
	int tokenType;
	char *tokenText;
	SceSize tokenSize;
	SceSize depth;
	int nodeIndex[16];
	int arrayIndex[16];
	int state;
	SceSize length;
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];
	SceSize partialLength;
	SceSize remaining;
	NpCommonPs4Lexer3Callback callback;
	void *context;
};

/* the lexer of 0x81010168: the third one, told where in the text everything is, its values handed over in pieces
   when they are longer than the buffer */
struct NpCommonPs4Lexer4Token {
	int type;
	char *text;
	int complete;			/* 1 the whole value, 0 a piece of it */
};

struct NpCommonPs4Lexer4;
typedef int (*NpCommonPs4Lexer4Callback)(struct NpCommonPs4Lexer4 *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4Lexer4Token *token, int arrayIndex, SceSize offset, void *context);

/* 220 bytes */
struct NpCommonPs4Lexer4 {
	const struct NpCommonPs4LexerConfig *config;
	char *name;
	SceSize nameSize;
	int tokenType;
	char *tokenText;
	SceSize tokenSize;
	SceSize depth;
	int nodeIndex[16];
	int arrayIndex[16];
	int state;
	SceSize position;		/* of the next character in all the text fed */
	SceSize tokenStart;		/* of the key or value being read */
	SceSize length;
	const char *literal;		/* "true", "false" or "null" while one is read */
	int matched;
	int matchedFlag;
	int mode;
	SceSize baseDepth;
	char partial[12];
	SceSize partialLength;
	SceSize remaining;
	NpCommonPs4Lexer4Callback callback;
	void *context;
};

extern const struct NpCommonPs4LexerConfig g_serverErrorConfig;		/* 0x81028a40 */
extern const struct NpCommonPs4LexerConfig g_serverErrorConfigEither;	/* 0x81028a8c */
int npCommonPs4LexerMatch(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	int *pIndex, int *pFlag);
int npCommonPs4LexerCopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength);
int npCommonPs4Lexer2Match(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	int *pIndex, int *pFlag);
int npCommonPs4Lexer2CopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength);
int SceNpCommonPs4_738FF77D(struct NpCommonPs4Lexer *lexer, const struct NpCommonPs4LexerConfig *config, NpCommonPs4LexerCallback callback, void *context);
int SceNpCommonPs4_6C941C30(struct NpCommonPs4Lexer *lexer, const char *data, int size);
int SceNpCommonPs4_EB1B7375(struct NpCommonPs4Lexer *lexer, const struct NpCommonPs4LexerConfig *config, NpCommonPs4LexerCallback callback, void *context);
int SceNpCommonPs4_F5EE219F(struct NpCommonPs4Lexer *lexer, const char *src, int size);
int npCommonPs4Lexer3Match(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	SceSize nameLength, int *pIndex, int *pFlag);
int npCommonPs4Lexer3CopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength);
int SceNpCommonPs4_CAFD09C2(struct NpCommonPs4Lexer3 *lexer, const struct NpCommonPs4LexerConfig *config, char *name, SceSize nameSize,
	char *text, SceSize textSize, NpCommonPs4Lexer3Callback callback, void *context);
int SceNpCommonPs4_D7AB56B8(struct NpCommonPs4Lexer3 *lexer, const char *src, int size);
int npCommonPs4Lexer4Match(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	SceSize nameLength, int *pIndex, int *pFlag);
int npCommonPs4Lexer4CopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength);
int SceNpCommonPs4_8BF34333(struct NpCommonPs4Lexer4 *lexer, const struct NpCommonPs4LexerConfig *config, char *name, SceSize nameSize,
	char *text, SceSize textSize, NpCommonPs4Lexer4Callback callback, void *context);
int SceNpCommonPs4_40E4EF01(struct NpCommonPs4Lexer4 *lexer, const char *src, int size);
int SceNpCommonPs4_AC633131(struct NpCommonPs4Lexer *lexer, struct NpCommonPs4ServerErrorContext *context);
int SceNpCommonPs4_358A8F68(struct NpCommonPs4Lexer *lexer, struct NpCommonPs4ServerErrorContext *context);
int SceNpCommonPs4_AD229434(struct NpCommonPs4Lexer *lexer, const char *data, int size);
int _sceNpServerErrorJsonGetErrorCode(int status, const char *body, int size, SceUInt8 either);
int npCommonPs4ServerErrorOnCode(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int npCommonPs4ServerErrorOnCodeEither(struct NpCommonPs4Lexer *lexer, int event, int depth, int node, const char *name,
	const struct NpCommonPs4LexerToken *token, int arrayIndex, void *context);
int SceNpCommonPs4_FB63A152(int status, const char *body, int size);
int SceNpCommonPs4_CF591432(int status, const char *body, int size);
int SceNpCommonPs4_69090D80(int requestId);
int SceNpCommonPs4_EF0A4C60(int requestId, const char *prefix);
int SceNpCommon_BBC13621(void);
int SceNpManager_72936716(const char *notification, int reserved);
SceInt32 sceHttpUriUnescape(char *out, SceSize *require, SceSize prepare, const char *in);
int sceRegMgrSystemParamGetInt(int id, int *value);

#ifdef __cplusplus
}
#endif

#endif
