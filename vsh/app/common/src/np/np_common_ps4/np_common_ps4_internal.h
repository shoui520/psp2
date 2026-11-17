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
#define NP_COMMON_PS4_ERROR_80558003	-2141880317
#define NP_COMMON_PS4_ERROR_80559E03	-2141872637
#define NP_COMMON_PS4_ERROR_80559E0C	-2141872628
#define NP_COMMON_PS4_ERROR_80559E0D	-2141872627
#define NP_COMMON_PS4_ERROR_80559E11	-2141872623
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

struct NpCommonPs4HttpTrans {
	const void *const *vtable;
	const void *const *vtableSecondBase;
	struct NpCommonPs4Allocator *pool;
	void *connPool;
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

/* the connection pool (npext/http/http_conn_pool.cpp) */
int npCommonPs4HttpConnPoolGetConnection(void *connPool, struct NpCommonPs4HttpTemplate *tmpl, const char *scheme,
	const char *serverName, SceUShort16 port, SceInt32 *pConnId);
int npCommonPs4HttpConnPoolGetConnectionWithUrl(void *connPool, struct NpCommonPs4HttpTemplate *tmpl, const char *url,
	SceInt32 *pConnId);
int npCommonPs4HttpConnPoolRelease(void *connPool, SceInt32 connId);
void npCommonPs4HttpConnPoolReportError(void *connPool, SceInt32 connId, int error);

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
int SceNpCommonPs4_AE224009(struct NpCommonPs4JsonValue *value, char boolean);
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
int SceNpCommonPs4_89322005(struct NpCommonPs4JsonParser *parser, void *config, const void *const *const *eventHandler);
int SceNpCommonPs4_EB1B7375(void *lexer, void *config, void *callback, void *context);
int SceNpCommonPs4_F5EE219F(void *lexer, void *data, int size);
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
void SceNpCommonPs4_31DB2D97(void);
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

#ifdef __cplusplus
}
#endif

#endif
