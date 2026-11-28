/*
 * np_push_internal.h: Internal types, error codes, SceNpCommonPs4 primitives,
 * and class declarations for SceNpPush VSH (vsh/app/common/src/np/np_push_vsh).
 */
#ifndef NP_PUSH_INTERNAL_H
#define NP_PUSH_INTERNAL_H

#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <ipmi/session.h>
#include "../imps/np_imps_core.h"

/* ============================================================================
 * Error Codes (0x8055A2xx)
 * ============================================================================ */

#define SCE_NP_PUSH_ERROR_INTERNAL			(-2141871616)	/* 0x8055A200 */
#define SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED		(-2141871615)	/* 0x8055A201 */
#define SCE_NP_PUSH_ERROR_NOT_INITIALIZED		(-2141871614)	/* 0x8055A202 */
#define SCE_NP_PUSH_ERROR_INVALID_ARGUMENT		(-2141871613)	/* 0x8055A203 */
#define SCE_NP_PUSH_ERROR_OUT_OF_MEMORY			(-2141871612)	/* 0x8055A204 */
#define SCE_NP_PUSH_ERROR_ABORTED			(-2141871611)	/* 0x8055A205 */
#define SCE_NP_PUSH_ERROR_TIMEOUT			(-2141871610)	/* 0x8055A206 */
#define SCE_NP_PUSH_ERROR_MAX				(-2141871609)	/* 0x8055A207 */
#define SCE_NP_PUSH_ERROR_ALREADY_EXISTS		(-2141871608)	/* 0x8055A208 */
#define SCE_NP_PUSH_ERROR_INVALID_STATE			(-2141871605)	/* 0x8055A20B */
#define SCE_NP_PUSH_ERROR_NOT_FOUND			(-2141871604)	/* 0x8055A20C */
#define SCE_NP_PUSH_ERROR_INVALID_PACKET		(-2141871603)	/* 0x8055A20D */
#define SCE_NP_PUSH_ERROR_UNEXPECTED_PACKET		(-2141871602)	/* 0x8055A20E */
#define SCE_NP_PUSH_ERROR_SERVER_RESPONSE		(-2141871600)	/* 0x8055A210 */
#define SCE_NP_PUSH_ERROR_NOT_CONNECTED			(-2141871599)	/* 0x8055A211 */
#define SCE_NP_PUSH_ERROR_DISCONNECTED			(-2141871598)	/* 0x8055A212 */
#define SCE_NP_PUSH_ERROR_CONNECT_FAILED		(-2141871597)	/* 0x8055A213 */
#define SCE_NP_PUSH_ERROR_SEND_FAILED			(-2141871596)	/* 0x8055A214 */
#define SCE_NP_PUSH_ERROR_RECEIVE_FAILED		(-2141871595)	/* 0x8055A215 */
#define SCE_NP_PUSH_ERROR_PROTOCOL			(-2141871594)	/* 0x8055A216 */
#define SCE_NP_PUSH_ERROR_JSON_PARSE			(-2141871593)	/* 0x8055A217 */
#define SCE_NP_PUSH_ERROR_JSON_BUILD			(-2141871592)	/* 0x8055A218 */
#define SCE_NP_PUSH_ERROR_QUEUE_FULL			(-2141871591)	/* 0x8055A219 */
#define SCE_NP_PUSH_ERROR_BUFFER_TOO_SMALL		(-2141871589)	/* 0x8055A21B */
#define SCE_NP_PUSH_ERROR_KICKED_OUT			(-2141871587)	/* 0x8055A21D */
#define SCE_NP_PUSH_ERROR_SIGNED_OUT			(-2141871586)	/* 0x8055A21E */
#define SCE_NP_PUSH_ERROR_AUTH_FAILED			(-2141871584)	/* 0x8055A220 */
#define SCE_NP_PUSH_ERROR_USER_NOT_FOUND		(-2141871583)	/* 0x8055A221 */
#define SCE_NP_PUSH_ERROR_NO_DATA			(-2141871582)	/* 0x8055A222 */
#define SCE_NP_PUSH_ERROR_INVALID_DATA_TYPE		(-2141871581)	/* 0x8055A223 */
#define SCE_NP_PUSH_ERROR_SERVICE_UNAVAILABLE		(-2141871579)	/* 0x8055A225 */
#define SCE_NP_PUSH_ERROR_RATE_LIMITED			(-2141871578)	/* 0x8055A226 */
#define SCE_NP_PUSH_ERROR_SERVER_RETRY			(-2141871575)	/* 0x8055A229 */
#define SCE_NP_PUSH_ERROR_SERVER_BUSY			(-2141871574)	/* 0x8055A22A */
#define SCE_NP_PUSH_ERROR_SERVER_SHUTDOWN		(-2141871572)	/* 0x8055A22C */
#define SCE_NP_PUSH_ERROR_TLV_PARSE			(-2141871570)	/* 0x8055A22E */
#define SCE_NP_PUSH_ERROR_TLV_BUFFER_TOO_SMALL		(-2141871569)	/* 0x8055A22F */
#define SCE_NP_PUSH_ERROR_TLV_EXDATA_FULL		(-2141871568)	/* 0x8055A230 */
#define SCE_NP_PUSH_ERROR_TLV_ALREADY_SET		(-2141871567)	/* 0x8055A231 */
#define SCE_NP_PUSH_ERROR_UNSUPPORTED			(-2141871559)	/* 0x8055A239 */

#define NP_PUSH_IPC_MAX_EXDATA_COUNT			10
#define NP_PUSH_IPC_EXDATA_KEY_STRIDE			33
#define NP_PUSH_IPC_ONLINE_ID_WIRE_SIZE			32
#define NP_PUSH_IPC_COMM_ID_WIRE_SIZE			12
#define NP_PUSH_IPC_DATA_TYPE_WIRE_SIZE			65

#define SCE_NP_COMMON_PS4_ERROR_JSON_MISSING_KEY	(-2141872594)	/* 0x80559E2E */
#define SCE_HTTP_ERROR_PARSE_HTTP_NOT_FOUND		(-2143084507)	/* 0x80432025 */

#define NP_PUSH_HEAP_SIZE				(128 * 1024)
#define NP_PUSH_MAX_USERS				32
#define NP_PUSH_MAX_IPC_QUEUED_PACKETS			16
#define NP_PUSH_CTL_CMD_QUEUE_CAPACITY			64
#define NP_PUSH_SESSION_CMD_QUEUE_CAPACITY		64
#define NP_PUSH_RECV_PACKET_QUEUE_CAPACITY		32
#define NP_PUSH_RECV_BUFFER_SIZE			(16 * 1024)
#define NP_PUSH_REQUEST_TIMEOUT_USEC			(60U * 1000U * 1000U)
#define NP_PUSH_GRACE_PERIOD_SEC			120
#define NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC		60

#define NP_PUSH_IPC_SERVICE_ID				0
#define NP_PUSH_IPC_EVF_INDEX				0
#define NP_PUSH_IPC_EVENT_USER_NOTIFICATION		(1ULL)
#define NP_PUSH_IPC_EVENT_DATA_TYPE			(2ULL)

#define NP_PUSH_RETURN_CODE_OK				0
#define NP_PUSH_RETURN_CODE_ALREADY_LOGGED_IN		2102273
#define NP_PUSH_RETURN_CODE_ALREADY_LOGGED_OUT		2102275

#if defined(FW_TYPE_CEX)
#define NP_PUSH_BUILD_ID "J01509657"
#elif FW_VERSION == 0x03630011
#define NP_PUSH_BUILD_ID "J01508051"
#else
#define NP_PUSH_BUILD_ID "J01509658"
#endif

#define NP_PUSH_SOURCE_PATH(rel) \
	"W:\\Build\\" NP_PUSH_BUILD_ID "\\vsh\\app\\common\\src\\np\\np_push_vsh\\" rel

#define NP_PUSH_ASSERT(cond, func, rel, line, expr) \
	do { \
		if (!(cond)) { \
			SceNpCommonPs4_F50BD96A(sceClibPrintf( \
				"Assertion in %s(%s:%d): %s(%s)\n", \
				func, NP_PUSH_SOURCE_PATH(rel), line, "ASSERT", expr)); \
		} \
	} while (0)

/* ============================================================================
 * SceNpCommonPs4 Shared Layout Types
 * ============================================================================ */

struct NpPushAllocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
};

struct NpPushAllocatorWithError {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
	int errorCode;
};

struct NpPushMemBlock {
	SceSize size;
	SceUID uid;
	void *base;
};

struct NpPushHeap {
	int state;
	NpPushMemBlock block;
	void *mspace;
};

struct NpPushMutex {
	const void *const *vtable;
	SceUID mutexId;
	SceUInt8 initialized;
};

struct NpPushCond {
	const void *const *vtable;
	SceUID condId;
	SceUID *mutexIdPtr;
};

struct NpPushRefCounted {
	const void *const *vtable;
	volatile SceUInt32 refCount;
};

struct NpPushRingBuf {
	const void *const *vtable;
	SceUInt8 *buffer;
	SceUInt8 *readPtr;
	SceUInt8 *writePtr;
	SceSize size;
	SceSize capacity;
	NpPushAllocator *allocator;
};

struct NpPushEventQueue {
	const void *const *vtable;
	NpPushRingBuf ringBuf;
	NpPushMutex mutex;
	NpPushCond itemCond;
	NpPushCond spaceCond;
	SceSize itemSize;
	SceSize itemCount;
	SceUInt16 initialized;
	SceUInt16 closedFlags;
};

struct NpPushThreadParam {
	const char *name;
	SceSize stackSize;
	int priority;
	int cpuAffinity;
};

struct NpPushThreadBase {
	const void *const *vtable;
	SceUID threadId;
	int priority;
	SceSize stackSize;
	char name[32];
	int cpuAffinity;
	int state;
	int exitStatus;
};

struct NpPushTimerEntry {
	NpPushTimerEntry *next;
	int (*callback)(void *context);
	SceInt64 dueTime;
	void *context;
	SceUInt32 reserved;
};

struct NpPushEventLoopHost {
	const void *const *vtable;
	void *loop;
	SceUInt32 loopStorage[6];
	SceUInt8 active;
	SceUInt8 borrowed;
};

struct NpPushCalloutJob {
	const void *const *vtable;
	void *host;
	int state;
	SceUInt32 reserved;
	NpPushTimerEntry entry;
	void *target;
};

struct NpPushCancelable {
	const void *const *vtable;
	NpPushMutex mutex;
	int cancelErrcode;
	int cancelFlags;
	SceUInt8 cancelable;
	int lockCount;
	NpPushCancelable *childCancel;
	NpPushCancelable *parentCancel;
};

struct NpPushHandle {
	const void *const *vtable;
	NpPushCancelable cancelable;
	int refCount;
};

struct NpPushScope {
	NpPushCancelable cancelable;
	NpPushHandle *handle;
};

struct NpPushOnlineIdData {
	char text[16];
	char term;
	char padding[3];
};

struct NpPushOnlineId {
	const void *const *vtable;
	char text[16];
	char term;
	char padding[3];
};

struct NpPushCommunicationIdData {
	char text[9];
	char term;
	SceUInt8 num;
	char padding;
};

struct NpPushCommunicationId {
	const void *const *vtable;
	char text[9];
	char term;
	SceUInt8 num;
	char padding;
};

struct NpPushUserToken {
	const void *const *vtable;
	SceUInt32 reserved;
	char accessToken[264];
};

struct NpPushTitleToken {
	const void *const *vtable;
	SceUInt32 reserved;
	char tokenText[1040];
};

struct NpPushStringHolder {
	const void *const *vtable;
	char *text;
	NpPushAllocatorWithError *allocator;
};

struct NpPushHttpTemplate;
struct NpPushHttpConnPool;

struct NpPushHttpContext {
	const void *const *vtable;
	NpPushAllocatorWithError *allocator;
	NpPushHttpTemplate *httpTemplate;
	NpPushHttpConnPool *connPool;
	SceInt32 libHttpCtxId;
};

struct NpPushHttpTrans {
	const void *const *vtable;
	const void *const *streamVtable;
	NpPushAllocator *allocator;
	NpPushHttpConnPool *connPool;
	SceInt32 connId;
	SceInt32 reqId;
	SceUInt64 receivedSize;
};

struct NpPushNpTrans {
	NpPushHttpTrans httpTrans;
	NpPushOnlineId onlineId;
};

/* JSON DOM & Streaming Parser */
#define NP_PUSH_JSON_TYPE_NULL		0
#define NP_PUSH_JSON_TYPE_BOOL		1
#define NP_PUSH_JSON_TYPE_NUMBER	2
#define NP_PUSH_JSON_TYPE_STRING	3
#define NP_PUSH_JSON_TYPE_ARRAY		4
#define NP_PUSH_JSON_TYPE_OBJECT	5

#define NP_PUSH_LEXER_NODE_END		0
#define NP_PUSH_LEXER_NODE_KEY		1
#define NP_PUSH_LEXER_NODE_ELEMENT	2

#define NP_PUSH_LEXER_TOKEN_NULL	0
#define NP_PUSH_LEXER_TOKEN_TRUE	1
#define NP_PUSH_LEXER_TOKEN_FALSE	2
#define NP_PUSH_LEXER_TOKEN_NUMBER	3
#define NP_PUSH_LEXER_TOKEN_STRING	4

struct NpPushJsonValue {
	const void *const *vtable;
	NpPushAllocator *allocator;
	NpPushJsonValue *parent;
	SceInt32 valueType;
	void *payload;
};

struct NpPushJsonLexerNode {
	int kind;
	const char *name;
	int descendFlag;
};

struct NpPushJsonLexerConfig {
	const NpPushJsonLexerNode *const *levels;
	SceSize numLevels;
};

struct NpPushJsonParser {
	const void *const *vtable;
	NpPushAllocator *allocator;
	unsigned char lexerBytes[1856];
	char *buffer;
	SceSize bufferSize;
	const void *const *const *handler;
};

struct NpPushJsonDocument {
	const void *const *vtable;
	const void *const *handlerVtable;
	NpPushAllocator *allocator;
	void *config;
	NpPushJsonParser *parser;
	NpPushJsonValue *root;
	NpPushJsonValue *current;
	char *keyName;
};

struct NpPushJsonSerializer {
	const void *const *vtable;
	NpPushJsonValue *root;
};

/* WebSocket Open Parameters (52 bytes) */
struct NpWebSocketOpenParam {
	NpPushAllocator allocator;
	SceInt32 libHttpCtxId;
	const char *host;
	SceUShort16 port;
	SceUShort16 reserved;
	const char *path;
	SceInt32 useTls;
	SceUInt32 connectTimeout;
	const char *origin;
	const char *subProtocol;
	SceUInt32 recvTimeout;
};

/* ============================================================================
 * External Imports (SceNpCommonPs4, SceNpCommon, SceHttp, SceNpManager, Shell)
 * ============================================================================ */

extern "C" {

/* Allocator & Heap */
extern int SceNpCommonPs4_39CF37A2(NpPushHeap *heap, SceSize size, const char *name);
extern void *SceNpCommonPs4_C52C787A(NpPushHeap *heap);
extern void *SceNpCommonPs4_EB6E37A8(void **mspacePtr, SceSize size);
extern void SceNpCommonPs4_1514F370(void **mspacePtr, void *pointer);
extern void *SceNpCommonPs4_EDB4C46E(NpPushHeap *heap, NpPushAllocator *allocator);
extern void *SceNpCommonPs4_5A6F85BC(NpPushHeap *heap, int errorCode,
				     NpPushAllocatorWithError *allocator);
extern void *SceNpCommonPs4_0C922423(SceSize size, NpPushAllocator *allocator);
extern void *SceNpCommonPs4_AA980113(NpPushAllocator *allocator, SceSize size);
extern int SceNpCommonPs4_6B428829(NpPushAllocator *allocator, void *pointer);
extern char *SceNpCommonPs4_1CAB4A2E(NpPushAllocator *allocator, const char *text);
extern int SceNpCommonPs4_BB5282C8(void *object);
extern int SceNpCommonPs4_F50BD96A(int printed);
extern SceUInt32 SceNpCommonPs4_741F88FE(volatile SceUInt32 *counter);

/* RefCounted */
extern NpPushRefCounted *SceNpCommonPs4_108738F2(NpPushRefCounted *object);
extern NpPushRefCounted *SceNpCommonPs4_BA23AD76(NpPushRefCounted *object);
extern int SceNpCommonPs4_64BAC888(NpPushRefCounted *object);
extern int SceNpCommonPs4_93C97239(NpPushRefCounted *object);

/* Mutex */
extern NpPushMutex *SceNpCommonPs4_4266D870(NpPushMutex *mutex);
extern NpPushMutex *SceNpCommonPs4_294F0B1A(NpPushMutex *mutex);
extern int SceNpCommonPs4_8AA7E995(NpPushMutex *mutex, const char *name, char recursive);
extern int SceNpCommonPs4_63EED478(NpPushMutex *mutex);
extern int SceNpCommonPs4_A2A8493B(NpPushMutex *mutex);
extern int SceNpCommonPs4_6BA5A164(NpPushMutex *mutex);
extern int SceNpCommonPs4_C78F5DF5(NpPushMutex *mutex);

/* Thread & EventQueue & Callout */
extern NpPushThreadBase *SceNpCommonPs4_04CBEB81(NpPushThreadBase *thread);
extern NpPushThreadBase *SceNpCommonPs4_502309F4(NpPushThreadBase *thread);
extern int SceNpCommonPs4_C8F1844F(NpPushThreadBase *thread, const NpPushThreadParam *param);
extern int SceNpCommonPs4_8CBDFDB7(NpPushThreadBase *thread);
extern int SceNpCommonPs4_2C0585AD(NpPushThreadBase *thread, int *exitStatus);

extern NpPushEventQueue *SceNpCommonPs4_7E37A6BF(NpPushEventQueue *queue,
						 NpPushAllocator *allocator);
extern NpPushEventQueue *SceNpCommonPs4_3311DD67(NpPushEventQueue *queue);
extern int SceNpCommonPs4_EDE54448(NpPushEventQueue *queue, const char *name,
				   SceSize itemSize, SceSize itemCount);
extern int SceNpCommonPs4_DC7BB7F1(NpPushEventQueue *queue);
extern int SceNpCommonPs4_38E19C68(NpPushEventQueue *queue, const void *item,
				   SceSize size, SceUInt32 timeout);
extern int SceNpCommonPs4_F634B111(NpPushEventQueue *queue, void *item,
				   SceSize size, SceUInt32 timeout);
extern int SceNpCommonPs4_6C3B5D0F(NpPushEventQueue *queue, void *item, SceSize size);
extern int SceNpCommonPs4_3A4163FD(NpPushEventQueue *queue, char flags);

extern NpPushEventLoopHost *SceNpCommonPs4_326EF8D2(NpPushEventLoopHost *host);
extern int SceNpCommonPs4_70073BD1(NpPushEventLoopHost *host, void *loop);
extern NpPushCalloutJob *SceNpCommonPs4_1CC3A645(NpPushCalloutJob *job, void *host);
extern NpPushCalloutJob *SceNpCommonPs4_941B0290(NpPushCalloutJob *job);
extern int SceNpCommonPs4_ED3D9A81(NpPushCalloutJob *job, SceUInt32 delayMsec, void *target);
extern void SceNpCommonPs4_7A30E24E(NpPushCalloutJob *job);

/* Handle, Cancelable, Scope */
extern NpPushHandle *SceNpCommonPs4_2E2B2655(NpPushHandle *handle);
extern NpPushHandle *SceNpCommonPs4_62214062(NpPushHandle *handle);
extern int SceNpCommonPs4_D86527FB(NpPushHandle *handle);
extern int SceNpCommonPs4_6B8D9291(NpPushHandle *handle);
extern int SceNpCommonPs4_FC2480E5(NpPushHandle *handle);
extern int SceNpCommonPs4_A6C7D48D(NpPushCancelable *cancelable, int errcode, int flags);
extern int SceNpCommonPs4_DEAEDDB3(NpPushCancelable *cancelable);
extern NpPushScope *SceNpCommonPs4_5BBB1ABA(NpPushScope *scope);
extern NpPushScope *SceNpCommonPs4_101259AD(NpPushScope *scope);
extern int SceNpCommonPs4_DDFD56E9(NpPushScope *scope, NpPushHandle *handle,
				   int scopeFlags, int scopeReason, int scopeDetail);
extern int SceNpCommonPs4_3D0FA7C9(NpPushScope *scope, int result,
				   int scopeFlags, int scopeReason, int scopeDetail);

/* Time */
extern int SceNpCommonPs4_477BD360(SceRtcTick *tick);
extern int SceNpCommonPs4_5B904477(SceRtcTick *tick);
extern int SceNpCommonPs4_7DABC125(const SceInt64 *now, const SceInt64 *start,
				   SceUInt64 limit, SceUInt64 *remaining);

/* NP Identifiers, Tokens, StringHolder */
extern NpPushOnlineId *SceNpCommonPs4_B0BAB9BE(NpPushOnlineId *onlineId);
extern NpPushOnlineId *SceNpCommonPs4_D0787194(NpPushOnlineId *onlineId, const void *rawData);
extern NpPushOnlineId *SceNpCommonPs4_AEC0DAC5(NpPushOnlineId *onlineId,
					       const NpPushOnlineId *source);
extern NpPushOnlineId *SceNpCommonPs4_1543D7EE(NpPushOnlineId *onlineId);
extern int SceNpCommonPs4_21B2C0B7(const NpPushOnlineId *left, const NpPushOnlineId *right);
extern int SceNpCommonPs4_21F316D6(const NpPushOnlineId *left,
				   const NpPushOnlineIdData *right);

extern NpPushCommunicationId *SceNpCommonPs4_79576C83(NpPushCommunicationId *commId);
extern NpPushCommunicationId *SceNpCommonPs4_511C5E91(NpPushCommunicationId *commId);
extern int SceNpCommonPs4_54F72557(NpPushCommunicationId *commId, const char *text);

extern NpPushUserToken *SceNpCommonPs4_988DFD87(NpPushUserToken *token);
extern NpPushUserToken *SceNpCommonPs4_78CC9FCD(NpPushUserToken *token);
extern int SceNpCommonPs4_191AEA9E(NpPushHandle *handle, NpPushOnlineId *onlineId,
				   NpPushUserToken *token);
extern int SceNpCommonPs4_D95658C6(const NpPushUserToken *userToken, char *buffer, SceSize size);

extern NpPushTitleToken *SceNpCommonPs4_DC4787E9(NpPushTitleToken *token);
extern NpPushTitleToken *SceNpCommonPs4_FD80FE40(NpPushTitleToken *token);
extern const char *SceNpCommonPs4_80D16974(const NpPushTitleToken *token);
extern int SceNpCommonPs4_9BC491E3(NpPushHandle *handle, const char *onlineId,
				   NpPushTitleToken *token);

extern NpPushStringHolder *SceNpCommonPs4_FDA863C0(NpPushStringHolder *holder,
						   NpPushAllocatorWithError *allocator);
extern NpPushStringHolder *SceNpCommonPs4_483168C0(NpPushStringHolder *holder);
extern const char *SceNpCommonPs4_2CE76BE5(const NpPushStringHolder *holder);

/* HTTP Context & NP Transaction */
extern NpPushHttpContext *SceNpCommonPs4_53D6E2C9(NpPushHttpContext *context,
						  NpPushAllocatorWithError *allocator);
extern int SceNpCommonPs4_100E042E(NpPushHttpContext *context, SceInt32 libHttpCtxId,
				   SceInt32 maxConn);
extern NpPushNpTrans *SceNpCommonPs4_3FC6688C(NpPushNpTrans *npTrans,
					      NpPushAllocator *allocator,
					      const NpPushOnlineId *onlineId);
extern NpPushNpTrans *SceNpCommonPs4_47D94EF0(NpPushNpTrans *npTrans);
extern int SceNpCommonPs4_D30A8E84(NpPushHttpTrans *trans, NpPushHttpTemplate *httpTemplate,
				   NpPushHttpConnPool *connPool, SceInt32 method,
				   const char *url, SceUInt64 contentLength);
extern int SceNpCommonPs4_060EB06B(NpPushNpTrans *npTrans, NpPushHandle *handle);
extern int SceNpCommonPs4_57B14D22(NpPushNpTrans *npTrans, NpPushHandle *handle);
extern int SceNpCommonPs4_3FC1A88F(NpPushHttpTrans *trans, NpPushHandle *handle,
				   const void *body, SceSize size);
extern int SceNpCommonPs4_6B6E813E(NpPushHttpTrans *trans, NpPushHandle *handle,
				   SceInt32 *statusCode);
extern int SceNpCommonPs4_C80B0275(NpPushHttpTrans *trans, NpPushHandle *handle,
				   void *buffer, SceSize size, SceSize *received);
extern void SceNpCommonPs4_6DA866D5(int unused, int errorCode);
extern int SceNpCommonPs4_D9298357(const char *baseUrl, const char *path, char *outUrl,
				   SceSize outSize, SceSize *required);
extern int SceNpCommonPs4_47443DCE(NpPushHttpTrans *trans, NpPushHandle *handle,
				   void *buffer, SceSize size, SceSize *received);
extern int SceNpCommonPs4_F81085EE(NpPushHttpTrans *trans, NpPushHandle *handle,
				   const void *buffer, SceSize size, SceSize *sent);
extern int SceNpCommonPs4_8EBA83E1(char *streamBase, NpPushHandle *handle,
				   const void *buffer, SceSize size, SceSize *sent);

/* JSON DOM, Document, Parser, Serializer */
extern NpPushJsonValue *SceNpCommonPs4_D309C0B3(NpPushJsonValue *value,
						NpPushAllocator *allocator, int valueType);
extern int SceNpCommonPs4_13B11D92(NpPushJsonValue *object, const char *name,
				   SceInt32 number, NpPushJsonValue **outItem);
extern int SceNpCommonPs4_27F98F80(NpPushJsonValue *object, const char *name,
				   SceUInt64 number, NpPushJsonValue **outItem);
extern int SceNpCommonPs4_8E8E16FF(NpPushJsonValue *object, const char *name,
				   SceUInt8 boolean, NpPushJsonValue **outItem);
extern int SceNpCommonPs4_977CBCE6(NpPushJsonValue *object, const char *name,
				   const char *text, NpPushJsonValue **outItem);
extern int SceNpCommonPs4_C47C2F93(NpPushJsonValue *object, const char *name,
				   NpPushJsonValue **outItem);
extern NpPushJsonValue *SceNpCommonPs4_42CAF6ED(NpPushJsonValue *object, const char *name);
extern NpPushJsonValue *SceNpCommonPs4_C4525618(NpPushJsonValue *object, const char *name);
extern NpPushJsonValue *SceNpCommonPs4_E8740CE6(NpPushJsonValue *object, const char *name);
extern int SceNpCommonPs4_066F0583(NpPushJsonValue *object, const char *name, SceInt32 *outValue);
extern int SceNpCommonPs4_64EE86C7(NpPushJsonValue *object, const char *name,
				   SceUInt32 *outValue);
extern int SceNpCommonPs4_63174832(NpPushJsonValue *object, const char *name,
				   SceUInt64 *outValue);
extern int SceNpCommonPs4_9C3F1645(NpPushJsonValue *object);
extern NpPushJsonValue *SceNpCommonPs4_24D09586(NpPushJsonValue *object, int index,
						char **outName);
extern const char *SceNpCommonPs4_ED539A9D(NpPushJsonValue *stringValue);
extern int SceNpCommonPs4_86E584A0(NpPushJsonValue *stringValue, char *buffer, SceSize size);
extern int SceNpCommonPs4_3AE5ABF3(const char *text, SceInt32 *outValue, int base,
				   SceSize *outLength);
extern int SceNpCommonPs4_D5B30475(const char *text, SceUInt32 *outValue, int base,
				   SceSize *outLength);

extern NpPushJsonDocument *SceNpCommonPs4_FAE00F18(NpPushJsonDocument *document,
						   NpPushAllocator *allocator, void *config);
extern NpPushJsonDocument *SceNpCommonPs4_B09D64DB(NpPushJsonDocument *document);
extern int SceNpCommonPs4_9A770D9A(NpPushJsonDocument *document, const void *text, int length);
extern int SceNpCommonPs4_B3078188(NpPushJsonDocument *document, NpPushJsonValue **outRoot);

extern NpPushJsonSerializer *SceNpCommonPs4_4A1E54C6(NpPushJsonSerializer *serializer,
						     NpPushAllocator *allocator);
extern NpPushJsonSerializer *SceNpCommonPs4_74F1FA26(NpPushJsonSerializer *serializer);
extern int SceNpCommonPs4_4A94BDE6(NpPushJsonSerializer *serializer, void *buffer,
				   SceSize size, SceSize *outRequired);
extern int SceNpCommonPs4_5F8A1E12(NpPushJsonSerializer *serializer);

extern NpPushJsonParser *SceNpCommonPs4_4F720AFD(NpPushJsonParser *parser,
						 NpPushAllocator *allocator);
extern NpPushJsonParser *SceNpCommonPs4_0A3AA2EF(NpPushJsonParser *parser);
extern int SceNpCommonPs4_89322005(NpPushJsonParser *parser, void *config,
				   const void *const *const *eventHandler);
extern int SceNpCommonPs4_FE98E2A7(NpPushJsonParser *parser, const void *text, int size);

/* SceNpCommon / SceNpManager / SceHttp */
extern int sceNpCommonGetNpEnviroment(char *envName);
extern int sceNpManagerGetNpId(void *outNpId);
extern int sceHttpCreateTemplate(const char *userAgent, int httpVer, int autoProxyConf);
extern int sceHttpDeleteTemplate(int tmplId);
extern int sceHttpAddRequestHeader(int reqId, const char *name, const char *value,
				   unsigned int mode);
extern int sceHttpParseResponseHeader(const char *header, SceSize headerLen, const char *fieldStr,
				      const char **fieldValue, SceSize *valueLen);
extern int sceHttpSetResponseHeaderMaxSize(int id, SceSize headerSize);
extern int sceHttpSetConnectTimeOut(int id, unsigned int usec);
extern int sceHttpSetSendTimeOut(int id, unsigned int usec);
extern int sceHttpSetRecvTimeOut(int id, unsigned int usec);

} /* extern "C" */

/* External Shell Subsystem Bridges */
extern int npImpsCoreRegisterCallbacks(const NpImpsCoreCallbacks *callbacks, int *outCallbackId);
extern int npImpsCoreUnregisterCallbacks(int callbackId);
extern int npIpc2ServerSetEventFlag(IPMI::Session *session, SceUInt32 serviceId,
				    SceUInt32 evfIndex, SceUInt64 bitPattern);
extern int npAsmGetServiceBaseUrl(NpPushHandle *handle, NpPushHttpContext *httpContext,
				  const char *onlineId, const char *serviceName,
				  NpPushStringHolder *outUrl);
extern "C" void *SceNpCommon_659ED07E(void);

/* SceNpWebSocket Internal C++ API (0x813FA7F8..0x813FABF8) */
extern int npWebSocketCreate(const NpWebSocketOpenParam *param, void **outHandle);
extern int npWebSocketDestroy(void *wsHandle);
extern int npWebSocketAddRequestHeader(void *wsHandle, const char *name, SceSize nameLen,
				       const char *value, SceSize valueLen);
extern int npWebSocketConnect(void *wsHandle, char *respHeaderBuf, SceSize respHeaderBufSize,
			      SceSize *outRespHeaderSize);
extern int npWebSocketDisconnect(void *wsHandle);
extern int npWebSocketSendText(void *wsHandle, const char *text, SceSize length);
extern int npWebSocketSendClose(void *wsHandle);
extern int npWebSocketReceive(void *wsHandle, int *outOpcode, void *buffer,
			      SceSize bufferSize, SceSize *outReceived);
extern int npWebSocketGetResponseHeader(void *wsHandle, const char *headerName,
					const char **outValue, SceSize *outValueLen);

/* SceNpManager / Account Internal Bridges */
extern int npAccountIsSignedIn(void);
extern int npAccountGetCachedNpId(SceNpId *outNpId);
extern int npManagerGetCachedCredentials(void *outLoginId, void *outPassword,
					 SceUInt64 *outAccountId);

/* ============================================================================
 * sce::Np::Push Namespace Forward Declarations & Core Types
 * ============================================================================ */

namespace sce {
namespace Np {
namespace Push {

/* Smart reference pointer for NpPushRefCounted subclasses */
template <typename T>
class RefPtr {
public:
	RefPtr() : m_object(NULL) {}
	explicit RefPtr(T *object) : m_object(object)
	{
		if (m_object != NULL)
			SceNpCommonPs4_64BAC888(m_object);
	}
	RefPtr(const RefPtr &other) : m_object(other.m_object)
	{
		if (m_object != NULL)
			SceNpCommonPs4_64BAC888(m_object);
	}
	~RefPtr()
	{
		Reset();
	}
	RefPtr &operator=(const RefPtr &other)
	{
		if (this != &other) {
			if (m_object != NULL)
				SceNpCommonPs4_93C97239(m_object);
			m_object = other.m_object;
			if (m_object != NULL)
				SceNpCommonPs4_64BAC888(m_object);
		}
		return *this;
	}
	void Assign(T *object)
	{
		if (m_object != object) {
			if (m_object != NULL)
				SceNpCommonPs4_93C97239(m_object);
			m_object = object;
			if (m_object != NULL)
				SceNpCommonPs4_64BAC888(m_object);
		}
	}
	void Adopt(T *object)
	{
		if (m_object != NULL)
			SceNpCommonPs4_93C97239(m_object);
		m_object = object;
	}
	void Reset()
	{
		if (m_object != NULL) {
			SceNpCommonPs4_93C97239(m_object);
			m_object = NULL;
		}
	}
	T *Get() const { return m_object; }
	T *operator->() const { return m_object; }
	bool IsNull() const { return m_object == NULL; }

private:
	T *m_object;
};

/* Scoped mutex lock guard */
class ScopedLock {
public:
	explicit ScopedLock(NpPushMutex *mutex) : m_mutex(mutex)
	{
		if (m_mutex != NULL)
			SceNpCommonPs4_A2A8493B(m_mutex);
	}
	~ScopedLock()
	{
		if (m_mutex != NULL)
			SceNpCommonPs4_6BA5A164(m_mutex);
	}

private:
	NpPushMutex *m_mutex;
};

/* Doubly-linked list node allocated via NpPushAllocator */
template <typename T>
struct ListNode {
	ListNode *prev;
	ListNode *next;
	T value;
};

template <typename T>
class AllocList {
public:
	AllocList() : m_allocator(NULL), m_sentinel(NULL), m_count(0) {}
	explicit AllocList(NpPushAllocator *allocator)
		: m_allocator(allocator), m_sentinel(NULL), m_count(0)
	{
		EnsureSentinel();
	}
	~AllocList()
	{
		Clear();
		if (m_sentinel != NULL && m_allocator != NULL) {
			SceNpCommonPs4_6B428829(m_allocator, m_sentinel);
			m_sentinel = NULL;
		}
	}

	void SetAllocator(NpPushAllocator *allocator)
	{
		m_allocator = allocator;
		EnsureSentinel();
	}

	bool EnsureSentinel()
	{
		if (m_sentinel != NULL)
			return true;
		if (m_allocator == NULL)
			return false;
		ListNode<T> *node = static_cast<ListNode<T> *>(
			SceNpCommonPs4_AA980113(m_allocator, sizeof(ListNode<T>)));
		if (node == NULL)
			return false;
		node->prev = node;
		node->next = node;
		m_sentinel = node;
		m_count = 0;
		return true;
	}

	bool PushBack(const T &val)
	{
		if (!EnsureSentinel())
			return false;
		ListNode<T> *node = static_cast<ListNode<T> *>(
			SceNpCommonPs4_AA980113(m_allocator, sizeof(ListNode<T>)));
		if (node == NULL)
			return false;
		new (&node->value) T(val);
		node->next = m_sentinel;
		node->prev = m_sentinel->prev;
		m_sentinel->prev->next = node;
		m_sentinel->prev = node;
		m_count++;
		return true;
	}

	ListNode<T> *Erase(ListNode<T> *node)
	{
		if (node == NULL || node == m_sentinel)
			return m_sentinel;
		ListNode<T> *nextNode = node->next;
		node->prev->next = node->next;
		node->next->prev = node->prev;
		node->value.~T();
		SceNpCommonPs4_6B428829(m_allocator, node);
		m_count--;
		return nextNode;
	}

	void Clear()
	{
		if (m_sentinel == NULL)
			return;
		ListNode<T> *cur = m_sentinel->next;
		while (cur != m_sentinel)
			cur = Erase(cur);
	}

	ListNode<T> *Begin() const { return (m_sentinel != NULL) ? m_sentinel->next : NULL; }
	ListNode<T> *End() const { return m_sentinel; }
	SceSize Size() const { return m_count; }
	bool Empty() const { return m_count == 0; }

private:
	NpPushAllocator *m_allocator;
	ListNode<T> *m_sentinel;
	SceSize m_count;
};

/* ============================================================================
 * Protocol Version & Core Net / Callout Helpers
 * ============================================================================ */

struct ProtocolVersion {
	SceUInt16 majorVersion;
	SceUInt16 minorVersion;
	SceUInt8 useTitleToken;
	char versionText[8];

	ProtocolVersion();
	ProtocolVersion(SceUInt32 packedVersion, SceUInt8 titleTokenFlag = 0);
	bool Equals(const ProtocolVersion &other) const;
	SceUInt32 ToPacked() const
	{
		return (static_cast<SceUInt32>(majorVersion) << 16) | minorVersion;
	}
};

struct FromAccountInfo {
	SceUInt64 accountId;
	char onlineId[20];
	char opt[4];
	char platform[4];
	SceUInt8 valid;
};

struct PushAccountSnapshot {
	SceNpId npId;
	SceUInt64 accountId;
	SceUInt32 signedIn;
	SceUInt32 reserved;
};

struct PushRequestCompletion {
	int requestId;
	int errorCode;
	SceUInt32 retryIntervalSec;
	int serverStatus;
};

/* TranId helpers (core/np_push_packet.cpp) */
bool TranIdEquals(const char *left, const char *right);
int TranIdCopy(char *dst, const char *src);

struct ProtocolConfigEntry {
	const char *envName;
	SceUInt32 serverAddrVersion;
	SceUInt8 serverAddrUseTitleToken;
	SceUInt32 webSocketVersion;
	SceUInt8 webSocketUseTitleToken;
};

/* Core memory & network helpers (np_push_net.cpp) */
int InitHeap(void);
void TermHeap(void);
NpPushAllocator *GetAllocator(void);
NpPushAllocatorWithError *GetAllocatorWithError(void);
void *Alloc(SceSize size);
void Free(void *pointer);
int InitNet(void);
void TermNet(void);
NpPushHttpContext *GetNpHttpClient(void);

/* Core callout helpers (np_push_callout.cpp) */
int InitCallout(void);
void TermCallout(void);
NpPushEventLoopHost *GetCalloutContext(void);

/* Core protocol version helpers (np_push_core.cpp) */
int InitProtocolConfig(void);
int GetServerAddrProtocolVersion(ProtocolVersion *outVersion);
int GetWebSocketProtocolVersion(ProtocolVersion *outVersion);

/* Forward declarations of core classes */
class Packet;
class DataType;
class Connection;
class ServerAddr;
class GetServerAddrTrans;
class Request;
class UserRequest;
class UserRequestList;
class UserContext;
class SessionCmd;
class SessionCmdQueue;
class Session;
class Context;
class CtlCmd;
class CtlCmdQueue;
class Ctl;
class IpcUserContext;
class IpcProcessContext;
class Dispatcher;

/* ============================================================================
 * Packet (core/np_push_packet.cpp)
 * ============================================================================ */

enum PacketMethod {
	PACKET_METHOD_NONE		= 0,
	PACKET_METHOD_LOGIN		= 1001,
	PACKET_METHOD_LOGOUT		= 1002,
	PACKET_METHOD_CHANGE_STATUS	= 1003,
	PACKET_METHOD_SERVICE_COMM	= 2001,
	PACKET_METHOD_NOTIFICATION	= 3001
};

enum UserPresenceStatus {
	PRESENCE_STATUS_OFFLINE		= 0,
	PRESENCE_STATUS_ACTIVE		= 1,
	PRESENCE_STATUS_SLEEP		= 2
};

class Packet : public NpPushRefCounted {
public:
	static Packet *Create(void);
	static void Destroy(Packet *packet);
	static int Parse(const char *jsonText, SceSize length, RefPtr<Packet> *outPacket);
	static int CreateLoginRequest(const NpPushOnlineId *onlineId, int initialStatus,
				      RefPtr<Packet> *outPacket);
	static int CreateLogoutRequest(const NpPushOnlineId *onlineId, RefPtr<Packet> *outPacket);
	static int CreateChangeStatusRequest(const NpPushOnlineId *onlineId, int newStatus,
					     RefPtr<Packet> *outPacket);
	static int GenerateTranId(char *outBuf, SceSize bufSize);

	Packet();
	~Packet();

	int InitRequest(int methodCode);
	int SetTranId(const char *tranId);
	int SetOnlineId(const NpPushOnlineId *onlineId);
	int SetAuthAccessToken(const char *accessToken);

	int BuildBufSize(SceSize *outSize) const;
	int Build(char *outBuf, SceSize bufSize, SceSize *outWritten) const;

	NpPushJsonValue *GetRootObject(void) const { return m_root; }
	int GetMethod(int *outMethod) const;
	bool HasTranId(void) const;
	int GetTranId(char *outBuf, SceSize bufSize) const;
	int GetTranIdFixed(char *outTranId) const;
	bool HasReturnCode(void) const;
	int GetReturnCode(SceInt32 *outCode) const;
	bool HasRetryInterval(void) const;
	int GetRetryInterval(SceUInt32 *outIntervalSec) const;
	bool HasErrorSource(void) const;
	int GetErrorSource(char *outBuf, SceSize bufSize) const;
	bool HasQuery(void) const;
	int GetQueryType(char *outBuf, SceSize bufSize) const;
	bool HasDataType(void) const;
	int GetDataType(char *outBuf, SceSize bufSize) const;
	bool MatchDataTypeExact(const char *pattern) const;
	bool MatchDataTypePrefix(const char *prefix) const;
	bool HasFrom(void) const;
	int GetFromOnlineId(NpPushOnlineIdData *outOnlineId) const;
	int GetFromAccountInfo(FromAccountInfo *outInfo) const;
	int GetToOnlineId(NpPushOnlineIdData *outOnlineId) const;
	bool HasToCommId(void) const;
	int GetToCommId(NpPushCommunicationIdData *outCommId) const;
	bool HasStatus(void) const;
	int GetStatus(int *outStatus) const;
	NpPushJsonValue *GetBodyData(void) const;
	NpPushJsonValue *GetExtendedData(void) const;
	NpPushJsonValue *GetExtendedDataEntry(int index, char **outKeyName) const;
	bool IsReqTransResponse(void) const;
	bool IsLoginMethod(void) const;
	bool IsNotificationMethod(void) const;
	bool IsServiceQueryRequest(void) const;
	bool IsServiceQueryResponse(void) const;

private:
	NpPushJsonValue *m_root;
	int m_method;
};

/* ============================================================================
 * DataType & Connection (core/np_push_connection.cpp)
 * ============================================================================ */

class DataType {
public:
	DataType();
	DataType(const DataType &other);
	explicit DataType(const char *text);
	~DataType();
	DataType &operator=(const DataType &other);

	int Set(const char *text);
	const char *CStr(void) const { return m_text; }
	bool Equals(const char *other) const;

private:
	char *m_text;
};

struct ConnectionResponseInfo {
	SceUInt32 activeIntervalSec;
	SceUInt32 sleepIntervalSec;
	SceUInt32 retryIntervalSec;
	bool hasActiveInterval;
	bool hasSleepInterval;
	bool hasRetryInterval;
};

class Connection {
public:
	Connection();
	virtual ~Connection();

	int Connect(NpPushHandle *cancelHandle, const char *fqdn,
		    const NpPushOnlineId *onlineId, bool isReconnect,
		    ConnectionResponseInfo *outRespInfo);
	int Disconnect(void);
	int DisconnectWithCancelScope(NpPushHandle *cancelHandle);
	int SendClose(void);
	int SendPacket(const Packet *packet);
	int ReceivePacket(RefPtr<Packet> *outPacket);
	int ParseOnlineIdHeader(const char *headerName, NpPushOnlineIdData *outList,
				int maxCount, int *outCount);
	int ParseRetryIntervalHeader(SceUInt32 *outIntervalSec);
	bool IsConnected(void) const { return m_wsHandle != NULL; }

private:
	void *m_wsHandle;
	char *m_recvBuffer;
	SceSize m_recvBufferSize;
};

class RecvPacketQueue {
public:
	explicit RecvPacketQueue(NpPushMutex *mutex);
	virtual ~RecvPacketQueue();

	int Push(const RefPtr<Packet> &packet);
	void Pop(RefPtr<Packet> *outPacket);
	void Clear(void);

private:
	NpPushMutex *m_mutex;
	AllocList<RefPtr<Packet> > m_list;
};

class ReceiveThread {
public:
	explicit ReceiveThread(Session *owner);
	virtual ~ReceiveThread();
	virtual int Run(void);

	int Start(void);
	NpPushThreadBase *AsThreadBase(void)
	{
		return reinterpret_cast<NpPushThreadBase *>(this);
	}

private:
	SceUID m_threadId;
	int m_priority;
	SceSize m_stackSize;
	char m_name[32];
	int m_cpuAffinity;
	int m_state;
	int m_exitStatus;
	Session *m_owner;
};

/* ============================================================================
 * ServerAddr & GetServerAddrTrans (core/np_push_get_server_addr_trans.cpp)
 * ============================================================================ */

class ServerAddr {
public:
	ServerAddr();
	virtual ~ServerAddr();

	int SetFqdn(const char *fqdn);
	const char *GetFqdn(void) const { return m_fqdn; }
	void Clear(void);

private:
	char *m_fqdn;
};

class GetServerAddrTrans {
public:
	GetServerAddrTrans(NpPushAllocator *allocator, const NpPushOnlineId *onlineId);
	virtual ~GetServerAddrTrans();

	static int FetchServerAddr(NpPushHandle *cancelHandle, const char *baseUrl,
				   const NpPushOnlineId *onlineId, ServerAddr *outServerAddr,
				   SceUInt32 *outRetryIntervalSec);

	int Execute(NpPushHandle *cancelHandle, NpPushHttpContext *httpContext,
		    const char *baseUrl, ServerAddr *outServerAddr,
		    SceUInt32 *outRetryIntervalSec);
	int ReceiveAndParseResponse(NpPushHandle *cancelHandle, ServerAddr *outServerAddr,
				    SceUInt32 *outRetryIntervalSec);

private:
	NpPushNpTrans m_npTrans;
	NpPushAllocator *m_allocator;
};

/* ============================================================================
 * Request (core/np_push_request.cpp)
 * ============================================================================ */

enum RequestType {
	REQUEST_TYPE_TRANSACTION	= 1,
	REQUEST_TYPE_QUERY		= 2
};

enum RequestState {
	REQUEST_STATE_IDLE		= 0,
	REQUEST_STATE_RUNNING		= 1,
	REQUEST_STATE_FINISHED		= 2
};

class Request : public NpPushRefCounted {
public:
	class Handler {
	public:
		virtual ~Handler() {}
		virtual void OnRequestFinished(Request *request) = 0;
	};

	static Request *Create(RequestType requestType, const NpPushOnlineId *onlineId,
			       const RefPtr<Packet> &sendPacket, Handler *handler,
			       void *userContext);
	static void Destroy(Request *request);

	Request(RequestType requestType, const NpPushOnlineId *onlineId,
		const RefPtr<Packet> &sendPacket, Handler *handler, void *userContext);
	~Request();

	int Start(Session *session, NpPushHandle *cancelHandle);
	void Cancel(void);
	void AbortAndUnlink(void);
	void UnlinkIfFinished(RefPtr<Request> *outSelf);
	void InvokeHandler(void);
	int PrepareSendPacket(NpPushHandle *cancelHandle, RefPtr<Packet> *outPacket);
	void OnTimeout(void);
	int SetResultReceivedReqTransResponse(const RefPtr<Packet> &respPacket);
	int SetResultReceivedQueryResponse(const RefPtr<Packet> &respPacket);
	int SetResultError(int errorCode, SceUInt32 retryIntervalSec);

	RequestType GetType(void) const { return m_type; }
	RequestState GetState(void) const { return m_state; }
	int GetRequestId(void) const { return m_requestId; }
	int GetResult(void) const { return m_result; }
	SceUInt32 GetRetryInterval(void) const { return m_retryIntervalSec; }
	const char *GetTranId(void) const { return m_tranId; }
	const RefPtr<Packet> &GetSendPacket(void) const { return m_sendPacket; }
	const RefPtr<Packet> &GetRecvPacket(void) const { return m_recvPacket; }
	void *GetUserContext(void) const { return m_userContext; }

private:
	int SetResultErrorImpl(int errorCode, SceUInt32 retryIntervalSec);
	void NotifyFinished(void);

	NpPushMutex m_mutex;
	NpPushCalloutJob m_timeoutJob;
	RequestType m_type;
	RequestState m_state;
	int m_requestId;
	bool m_needsAuthToken;
	NpPushOnlineId m_onlineId;
	char m_tranId[32];
	RefPtr<Packet> m_sendPacket;
	RefPtr<Packet> m_recvPacket;
	int m_result;
	SceUInt32 m_retryIntervalSec;
	Handler *m_handler;
	void *m_userContext;
};

/* ============================================================================
 * UserRequest & UserRequestList (core/np_push_user_request.cpp)
 * ============================================================================ */

enum UserRequestType {
	USER_REQUEST_SET_PRESENCE_ONLINE	= 1
};

enum UserRequestState {
	USER_REQUEST_STATE_IDLE			= 0,
	USER_REQUEST_STATE_RUNNING		= 1,
	USER_REQUEST_STATE_FINISHED		= 2
};

typedef void (*UserRequestCallback)(int result, void *userContext);

class UserRequest : public NpPushRefCounted {
public:
	static UserRequest *CreateSetPresenceOnline(const NpPushOnlineId *onlineId,
						    UserRequestCallback callback,
						    void *userContext);
	static void Destroy(UserRequest *request);

	UserRequest(UserRequestType requestType, const NpPushOnlineId *onlineId,
		    UserRequestCallback callback, void *userContext);
	~UserRequest();

	UserRequestType GetType(void) const { return m_type; }
	UserRequestState GetState(void) const { return m_state; }
	int GetRequestId(void) const { return m_requestId; }
	void SetState(UserRequestState state) { m_state = state; }
	const NpPushOnlineId &GetOnlineId(void) const { return m_onlineId; }
	int GetResult(void) const { return m_result; }
	void SetResult(int result) { m_result = result; }
	int AttachToUserContext(UserContext *userCtx, bool *outAttached);
	void InvokeCallback(void);

private:
	UserRequestType m_type;
	UserRequestState m_state;
	int m_requestId;
	NpPushOnlineId m_onlineId;
	int m_result;
	UserRequestCallback m_callback;
	void *m_userContext;
	UserContext *m_attachedUser;
};

class UserRequestList {
public:
	UserRequestList();
	~UserRequestList();

	int Init(void);
	void Finalize(void);
	int Register(const RefPtr<UserRequest> &request);
	int SetRunning(const RefPtr<UserRequest> &request);
	int SetFinished(const RefPtr<UserRequest> &request, int result);
	int SetFinishedAll(int result);
	void FindByRequestId(int requestId, RefPtr<UserRequest> *outRequest);
	int FinishAllPending(int result);
	bool PopRegistered(RefPtr<UserRequest> *outRequest);
	bool PopFinished(RefPtr<UserRequest> *outRequest);
	bool HasPending(void) const;

private:
	int SetFinishedImpl(const RefPtr<UserRequest> &request, int result);

	NpPushMutex m_mutex;
	AllocList<RefPtr<UserRequest> > m_registered;
	AllocList<RefPtr<UserRequest> > m_running;
	AllocList<RefPtr<UserRequest> > m_finished;
	bool m_initialized;
};

/* ============================================================================
 * SessionCmd, SessionCmdQueue & SessionThread (core/np_push_session_cmd.cpp)
 * ============================================================================ */

enum SessionCmdType {
	SESSION_CMD_INVALID		= -1,
	SESSION_CMD_CONNECT		= 1,
	SESSION_CMD_DISCONNECT		= 2,
	SESSION_CMD_SEND_REQUEST	= 3,
	SESSION_CMD_RECV_PACKET		= 4,
	SESSION_CMD_RECV_ERROR		= 5,
	SESSION_CMD_STOP		= 6
};

class SessionCmd {
public:
	SessionCmd();
	explicit SessionCmd(SessionCmdType cmdType);
	virtual ~SessionCmd();

	SessionCmdType type;
	int errorCode;
	void *payload;
};

class SessionCmdQueue {
public:
	SessionCmdQueue();
	virtual ~SessionCmdQueue();

	int Init(void);
	void Finalize(void);
	int Push(const SessionCmd *cmd);
	int Pop(SessionCmd *outCmd, SceUInt32 timeoutUsec);
	int TryPop(SessionCmd *outCmd);
	void Abort(void);

private:
	NpPushEventQueue m_queue;
	bool m_initialized;
};

class SessionThread {
public:
	explicit SessionThread(Session *owner);
	virtual ~SessionThread();
	virtual int Run(void);

	int Start(void);
	NpPushThreadBase *AsThreadBase(void)
	{
		return reinterpret_cast<NpPushThreadBase *>(this);
	}

private:
	SceUID m_threadId;
	int m_priority;
	SceSize m_stackSize;
	char m_name[32];
	int m_cpuAffinity;
	int m_state;
	int m_exitStatus;
	Session *m_owner;
};

/* ============================================================================
 * Session (core/np_push_session.cpp)
 * ============================================================================ */

enum SessionState {
	SESSION_STATE_INIT		= 0,
	SESSION_STATE_IDLE		= 1,
	SESSION_STATE_CONNECTING	= 2,
	SESSION_STATE_ESTABLISHED	= 3,
	SESSION_STATE_DISCONNECTING	= 4,
	SESSION_STATE_STOPPING		= 5,
	SESSION_STATE_ERROR		= 6
};

class Session {
public:
	class Handler {
	public:
		virtual ~Handler() {}
		virtual void OnSessionConnected(Session *session,
						const ConnectionResponseInfo *respInfo) = 0;
		virtual void OnSessionConnectFailed(Session *session, int errorCode,
						    SceUInt32 retryIntervalSec) = 0;
		virtual void OnSessionDisconnected(Session *session, int errorCode) = 0;
		virtual void OnSessionReceivedPacket(Session *session,
						     const RefPtr<Packet> &packet) = 0;
	};

	Session();
	~Session();

	int Init(void);
	void Finalize(void);
	int RegisterHandler(Handler *handler);
	int Start(void);
	int Stop(void);

	int Connect(const NpPushOnlineId *onlineId, bool isReconnect);
	int Disconnect(void);
	int SendRequest(const RefPtr<Request> &request);
	int PostCommandChecked(const SessionCmd *cmd, const SceUInt32 *allowedStateMask);
	int PostConnectCommand(void);
	int PostDisconnectCommand(void);
	int SendLoginRequest(const NpPushOnlineId *onlineId, bool activeStatus,
			     bool includePresence, Request::Handler *handler, int *outRequestId);
	int SendLogoutRequest(const NpPushOnlineId *onlineId, Request::Handler *handler,
			      int *outRequestId);
	int SendChangeStatusRequest(const NpPushOnlineId *onlineId, int newStatus,
				    const SceUInt64 *timestamp, Request::Handler *handler,
				    int *outRequestId);
	int StopReceiveThreadAndConnection(void);
	int StopSessionThread(void);
	void OnConnectTimeout(void);
	SessionState GetState(void) const { return m_state; }
	int GetSessionId(void) const { return m_sessionId; }
	SceUInt32 GetRetryIntervalSec(void) const { return m_retryIntervalSec; }

	void ThreadMain(void);
	void ReceiveMain(void);

private:
	int StateIdle(const SessionCmd *cmd);
	int StateConnecting(const SessionCmd *cmd);
	int StateEstablished(const SessionCmd *cmd);
	int StateDisconnecting(const SessionCmd *cmd);
	int StateInvalid(const SessionCmd *cmd);

	int OpenConnection(const NpPushOnlineId *onlineId, bool isReconnect,
			   ConnectionResponseInfo *outRespInfo, SceUInt32 *outRetryIntervalSec);
	void ShutdownConnection(void);
	void DispatchReceivedPacket(const RefPtr<Packet> &packet);
	void ErrorRequestAll(int errorCode);
	int StartReceiveThread(void);
	void StopReceiveThread(void);
	void PopCompletedRequest(RefPtr<Request> *outRequest);
	void NotifyCompletedRequests(void);
	void PeekActiveRequest(RefPtr<Request> *outRequest);
	void FindRequestById(int requestId, RefPtr<Request> *outRequest);
	void FindRequestByTranId(const char *tranId, RefPtr<Request> *outRequest);
	void CancelCommandRequest(const SessionCmd *cmd);
	void ChangeState(SessionState nextState, int errorCode);
	void InspectResponseErrorSource(const RefPtr<Packet> &packet);
	void HandleReqTransResponse(const RefPtr<Packet> &packet);
	void HandleQueryResponse(const RefPtr<Packet> &packet);
	void NotifyNotificationPacket(const RefPtr<Packet> &packet);
	void NotifyServiceCommPacket(const RefPtr<Packet> &packet);
	void NotifyServiceQueryPacket(const RefPtr<Packet> &packet);
	int ProcessRequestCommand(int requestId);

	NpPushMutex m_mutex;
	int m_sessionId;
	SessionState m_state;
	Handler *m_handler;
	AllocList<Handler *> m_handlers;
	SessionCmdQueue m_cmdQueue;
	SessionThread *m_sessionThread;
	ReceiveThread *m_receiveThread;
	bool m_sessionThreadRunning;
	bool m_receiveThreadRunning;
	NpPushHandle m_cancelHandle;
	Connection m_connection;
	RecvPacketQueue m_recvQueue;
	NpPushOnlineId m_connectOnlineId;
	bool m_isReconnect;
	SceUInt32 m_retryIntervalSec;
	AllocList<RefPtr<Request> > m_activeRequests;
	bool m_initialized;
};

/* ============================================================================
 * UserContext (core/np_push_user_context.cpp)
 * ============================================================================ */

class UserContext : public Request::Handler {
public:
	class Handler {
	public:
		virtual ~Handler() {}
		virtual void OnUserContextRequestFinished(UserContext *userContext,
							  Request *request) = 0;
		virtual void OnUserContextRetryTimer(UserContext *userContext) = 0;
	};

	UserContext(const NpPushOnlineId *onlineId, Handler *handler);
	~UserContext();

	const NpPushOnlineId &GetOnlineId(void) const { return m_onlineId; }
	int GetDesiredStatus(void) const { return m_desiredStatus; }
	int GetServerStatus(void) const { return m_serverStatus; }
	void SetDesiredStatus(int status) { m_desiredStatus = status; }
	void SetServerStatus(int status) { m_serverStatus = status; }
	void ResetOnDisconnect(void);

	int RequestStart(Session *session, NpPushHandle *cancelHandle);
	int StartPresenceOnlineRequest(const RefPtr<UserRequest> &userRequest,
				       Session *session, NpPushHandle *cancelHandle);
	void SetUserRequestResult(int result);
	void CancelPendingRequests(void);
	bool HasActiveRequest(void) const { return !m_activeRequest.IsNull(); }
	bool NeedsSync(void) const { return m_desiredStatus != m_serverStatus; }
	void ScheduleRetry(SceUInt32 delaySec);
	void CancelRetry(void);
	void OnRetryTimeout(void);
	int UpdateAccountStatus(const PushAccountSnapshot *snapshot);
	int MarkSignedOut(void);
	void SetServerPresenceState(int nextState);
	void SetInitialServerPresenceState(int nextState);
	SceUInt32 GetNextTimerDelayUsec(const SceInt64 *nowTick) const;
	void ExpireGracePeriod(const SceInt64 *nowTick, bool *outRemovable);
	int SyncStateWithSession(Session *session, const SceInt64 *nowTick,
				 int activeIntervalSec, bool *outRemovable);
	int HandleRequestResult(const PushRequestCompletion *completion);
	void ResetWithError(int errorCode);

	virtual void OnRequestFinished(Request *request);

private:
	NpPushOnlineId m_onlineId;
	int m_desiredStatus;
	int m_serverStatus;
	int m_pendingTargetStatus;
	int m_activeRequestId;
	SceInt64 m_lastSyncTick;
	SceInt64 m_graceStartTick;
	SceInt64 m_retryStartTick;
	int m_retryCount;
	SceUInt32 m_retryIntervalSec;
	RefPtr<Request> m_activeRequest;
	RefPtr<UserRequest> m_pendingUserRequest;
	NpPushCalloutJob m_retryJob;
	Handler *m_handler;
};

/* ============================================================================
 * Context (core/np_push_context.cpp)
 * ============================================================================ */

class Context {
public:
	Context();
	virtual ~Context();

	int Init(Session::Handler *sessionHandler, UserContext::Handler *userHandler);
	int InitMutex(void);
	void Finalize(void);

	Session *GetSession(void) const { return m_session; }
	int GetSessionId(void);
	SceUInt32 GetSessionRetryInterval(void);
	void TeardownSession(SceUInt32 *outRetryIntervalSec);
	void ResetAllUsersWithError(int errorCode);
	int AllocateUserContext(const NpPushOnlineId *onlineId, UserContext::Handler *userHandler);
	UserContext *FindUserContext(const NpPushOnlineId *onlineId) const;
	UserContext *FindUserContextWithIndex(const NpPushOnlineId *onlineId,
					      SceUInt32 *outIndex);
	UserContext *GetOrCreateUserContext(const NpPushOnlineId *onlineId,
					    UserContext::Handler *userHandler);
	void DestroyUserContextAt(int index);
	void RemoveUserContext(const NpPushOnlineId *onlineId);
	void ClearAllUserContexts(void);

	int SyncSignedInUsers(UserContext::Handler *userHandler);
	int ConnectSession(bool isReconnect);
	int UpdateInitialSessionStatus(void);
	int UpdateUserContexts(void);
	int UpdateAllUserContexts(int activeIntervalSec, UserContext::Handler *userHandler);
	int HandleRequestResult(UserContext *userContext, Request *request);
	int HandleRequestResultById(const PushRequestCompletion *completion);
	int DispatchUserRequest(const RefPtr<UserRequest> &userRequest, bool *outQueued);
	int StartPresenceOnlineRequest(const RefPtr<UserRequest> &userRequest);
	void ResetUsersOnDisconnect(void);
	bool HasAnyActiveUsers(void) const;
	void OnCalloutTimer(void);

	const NpPushOnlineId &GetSessionOnlineId(void) const { return m_sessionOnlineId; }
	void SetSessionOnlineId(const NpPushOnlineId *onlineId);
	SceUInt32 GetActiveIntervalSec(void) const { return m_activeIntervalSec; }
	SceUInt32 GetSleepIntervalSec(void) const { return m_sleepIntervalSec; }
	void UpdateIntervals(const ConnectionResponseInfo *respInfo);

private:
	NpPushMutex m_mutex;
	Ctl *m_ownerCtl;
	Session *m_session;
	bool m_initialStatusUpdated;
	NpPushCalloutJob m_updateTimerJob;
	UserContext *m_users[NP_PUSH_MAX_USERS];
	NpPushOnlineId m_sessionOnlineId;
	bool m_hasSessionOnlineId;
	NpPushHandle m_cancelHandle;
	SceUInt32 m_activeIntervalSec;
	SceUInt32 m_sleepIntervalSec;
	bool m_initialized;
};

/* ============================================================================
 * CtlCmd, CtlCmdQueue & CtlThread (core/np_push_ctl_cmd.cpp)
 * ============================================================================ */

enum CtlCmdType {
	CTL_CMD_START				= 1,
	CTL_CMD_STOP				= 2,
	CTL_CMD_SIGNED_IN			= 3,
	CTL_CMD_SIGNED_OUT			= 4,
	CTL_CMD_SESSION_CONNECTED		= 5,
	CTL_CMD_SESSION_CONNECT_FAILED		= 6,
	CTL_CMD_SESSION_DISCONNECTED		= 7,
	CTL_CMD_SESSION_RECV_PACKET		= 8,
	CTL_CMD_USER_REQUEST			= 9,
	CTL_CMD_REQUEST_FINISHED		= 10,
	CTL_CMD_RECONNECT_TIMER			= 11,
	CTL_CMD_USER_RETRY_TIMER		= 12,
	CTL_CMD_SET_PRESENCE_STATUS		= 13
};

class CtlCmd {
public:
	CtlCmd();
	explicit CtlCmd(CtlCmdType cmdType);
	virtual ~CtlCmd();

	CtlCmdType type;
	int statusCode;
	SceUInt32 retryIntervalSec;
	void *target;
	void *payload;
	int extraStatus;
};

class CtlCmdQueue {
public:
	CtlCmdQueue();
	virtual ~CtlCmdQueue();

	int Init(void);
	void Finalize(void);
	int Push(const CtlCmd *cmd);
	int Pop(CtlCmd *outCmd, SceUInt32 timeoutUsec);
	int TryPop(CtlCmd *outCmd);
	void Abort(void);

private:
	NpPushEventQueue m_queue;
	bool m_initialized;
};

class CtlThread {
public:
	explicit CtlThread(Ctl *owner);
	virtual ~CtlThread();
	virtual int Run(void);

	int Start(void);
	NpPushThreadBase *AsThreadBase(void)
	{
		return reinterpret_cast<NpPushThreadBase *>(this);
	}

private:
	SceUID m_threadId;
	int m_priority;
	SceSize m_stackSize;
	char m_name[32];
	int m_cpuAffinity;
	int m_state;
	int m_exitStatus;
	Ctl *m_owner;
};

/* ============================================================================
 * Ctl (core/np_push_ctl.cpp)
 * ============================================================================ */

enum CtlState {
	CTL_STATE_INIT			= 0,
	CTL_STATE_IDLE			= 1,
	CTL_STATE_RUNNING		= 2,
	CTL_STATE_CONNECTING		= 3,
	CTL_STATE_ESTABLISHED		= 4,
	CTL_STATE_DISCONNECTING		= 5,
	CTL_STATE_ERROR			= 6,
	CTL_STATE_STOPPING		= 7
};

class Ctl : public Session::Handler, public UserContext::Handler {
public:
	class Handler {
	public:
		virtual ~Handler() {}
		virtual void OnCtlReceivedPacket(const RefPtr<Packet> &packet) = 0;
		virtual void OnCtlDisconnected(int reason) = 0;
		virtual void OnCtlServiceCommPacket(int sessionId, const RefPtr<Packet> &packet) {}
		virtual void OnCtlServiceQueryRequest(int sessionId, const RefPtr<Packet> &packet) {}
		virtual void OnCtlServiceQueryResponse(int sessionId, void *callback, void *userArg,
						       int errorCode, const RefPtr<Packet> &packet) {}
	};

	static int InitInstance(void);
	static void TermInstance(void);
	static Ctl *GetInstance(void);

	Ctl();
	virtual ~Ctl();

	int Init(void);
	void Finalize(void);
	int RegisterHandler(Handler *handler);
	int Start(void);
	int Stop(void);

	int PostSignedIn(const NpPushOnlineId *onlineId);
	int PostSignedOut(void);
	int PostSetPresenceOnline(const NpPushOnlineId *onlineId,
				  UserRequestCallback callback, void *userContext);
	int PostSetPresenceStatus(const NpPushOnlineId *onlineId, int status);
	int PostCommandChecked(const CtlCmd *cmd, const SceUInt32 *allowedStateMask);
	int PostWakeupCommand(void);
	bool IsImpsOnline(void);
	int EnsureContextInitialized(void);
	void FailCommandUserRequest(const CtlCmd *cmd, int errorCode);
	void NotifyHandlersNotification(int sessionId, const RefPtr<Packet> &packet);
	void NotifyHandlersServiceComm(int sessionId, const RefPtr<Packet> &packet);
	void NotifyHandlersServiceQueryRequest(int sessionId, const RefPtr<Packet> &packet);
	void NotifyHandlersServiceQueryResponse(int sessionId, void *callback, void *userArg,
						int errorCode, const RefPtr<Packet> &packet);
	static void OnImpsStateChanged(int impsContext, int impsState);

	void ThreadMain(void);
	void OnReconnectTimeout(void);

	/* Session::Handler */
	virtual void OnSessionConnected(Session *session,
					const ConnectionResponseInfo *respInfo);
	virtual void OnSessionConnectFailed(Session *session, int errorCode,
					    SceUInt32 retryIntervalSec);
	virtual void OnSessionDisconnected(Session *session, int errorCode);
	virtual void OnSessionReceivedPacket(Session *session,
					     const RefPtr<Packet> &packet);

	/* UserContext::Handler */
	virtual void OnUserContextRequestFinished(UserContext *userContext, Request *request);
	virtual void OnUserContextRetryTimer(UserContext *userContext);

private:
	int StateIdle(const CtlCmd *cmd);
	int StateRunning(const CtlCmd *cmd);
	int StateConnecting(const CtlCmd *cmd);
	int StateEstablished(const CtlCmd *cmd);
	int StateDisconnecting(const CtlCmd *cmd);
	int StateError(const CtlCmd *cmd);
	int StateInvalid(const CtlCmd *cmd);

	int ConnectSession(bool isReconnect);
	void DestroyContext(void);
	void ScheduleReconnect(SceUInt32 delaySec);
	void CancelReconnect(void);
	int ProcessUserRequestStartPresenceOnline(const RefPtr<UserRequest> &userRequest);
	int StateIdle_ProcessUserRequestSetPresenceOnline(const RefPtr<UserRequest> &userRequest);
	int StateRunning_ProcessUserRequestSetPresenceOnline(
		const RefPtr<UserRequest> &userRequest);
	int StateConnecting_ProcessUserRequestSetPresenceOnline(
		const RefPtr<UserRequest> &userRequest);
	int StateEstablished_ProcessUserRequestSetPresenceOnline(
		const RefPtr<UserRequest> &userRequest);
	void DrainFinishedUserRequests(void);

	NpPushMutex m_mutex;
	CtlState m_state;
	Handler *m_handler;
	AllocList<Handler *> m_handlers;
	Context *m_context;
	CtlCmdQueue m_cmdQueue;
	UserRequestList m_userRequestList;
	CtlThread *m_ctlThread;
	bool m_ctlThreadRunning;
	NpPushCalloutJob m_reconnectJob;
	int m_impsCallbackId;
	bool m_isSignedIn;
	NpPushOnlineId m_signedInOnlineId;
	bool m_initialized;
};

/* ============================================================================
 * IPC Contexts & Dispatcher (service/np_push_ipc_context.cpp, np_push_dispatcher.cpp)
 * ============================================================================ */

enum FilterMatchType {
	FILTER_MATCH_EXACT	= 1,
	FILTER_MATCH_PREFIX	= 2
};

typedef void (*PushNotificationCallback)(const char *dataType, const char *fromOnlineId,
					 const char *toOnlineId, const char *bodyJson,
					 SceSize bodyJsonSize, void *userContext);

typedef void (*PushCommCallback)(const char *dataType, const char *queryType,
				 const char *fromOnlineId, const char *toOnlineId,
				 const char *bodyJson, SceSize bodyJsonSize,
				 const char *extendedJson, SceSize extendedJsonSize,
				 void *userContext);

struct DataTypeFilterEntry {
	FilterMatchType matchType;
	char dataType[64];
};

class IpcUserContext {
public:
	IpcUserContext(SceUInt32 userContextId, const NpPushOnlineId *onlineId);
	virtual ~IpcUserContext();

	SceUInt32 GetId(void) const { return m_userContextId; }
	const NpPushOnlineId &GetOnlineId(void) const { return m_onlineId; }
	int EnqueuePacket(const RefPtr<Packet> &packet);
	int DequeuePacket(RefPtr<Packet> *outPacket);
	int PeekPacket(RefPtr<Packet> *outPacket) const;
	void PeekPacketRef(RefPtr<Packet> *outPacket) const;
	SceSize GetQueuedCount(void) const { return m_packetQueue.Size(); }

private:
	SceUInt32 m_userContextId;
	NpPushOnlineId m_onlineId;
	AllocList<RefPtr<Packet> > m_packetQueue;
};

class IpcProcessContext {
public:
	explicit IpcProcessContext(IPMI::Session *session);
	virtual ~IpcProcessContext();

	IPMI::Session *GetSession(void) const { return m_session; }
	int CreateIpcUserContext(const NpPushOnlineId *onlineId, SceUInt32 *outUserContextId);
	int DeleteIpcUserContext(SceUInt32 userContextId);
	int DeleteIpcUserContextByOnlineId(const NpPushOnlineId *onlineId);
	int AddDataTypeFilter(FilterMatchType matchType, const char *dataType);
	int RemoveDataTypeFilter(FilterMatchType matchType, const char *dataType);
	int RemoveDataTypeFilterByName(const char *dataType);

	int ReceivePacket(const RefPtr<Packet> &packet);
	int ReceiveNotificationDataTypePacket(const RefPtr<Packet> &packet);
	int ReceiveUserNotificationPacket(const RefPtr<Packet> &packet);

	int DequeueDataTypePacket(RefPtr<Packet> *outPacket);
	int PeekDataTypePacket(RefPtr<Packet> *outPacket);
	void PeekDataTypePacketRef(RefPtr<Packet> *outPacket);
	int DequeueUserPacket(RefPtr<Packet> *outPacket);
	int PeekUserPacket(RefPtr<Packet> *outPacket);

private:
	SceSize CountQueuedUserPackets(void) const;
	int NotifyDataTypeEvent(void);
	int NotifyUserEvent(void);

	IPMI::Session *m_session;
	SceUInt32 m_nextUserContextId;
	AllocList<IpcUserContext *> m_userContexts;
	AllocList<DataTypeFilterEntry> m_filters;
	AllocList<RefPtr<Packet> > m_dataTypeQueue;
};

struct IpcJsonEntry {
	RefPtr<Packet> ownerPacket;
	const char *keyName;
	NpPushJsonValue *jsonValue;

	IpcJsonEntry() : keyName(NULL), jsonValue(NULL) {}
	~IpcJsonEntry() { Reset(); }

	void Reset(void)
	{
		ownerPacket.Reset();
		keyName = NULL;
		jsonValue = NULL;
	}
};

struct IpcExDataPair {
	const char *keyName;
	NpPushJsonValue *jsonValue;
};

struct IpcNotificationPayload {
	NpPushJsonValue *bodyValue;
	IpcExDataPair exDataPairs[NP_PUSH_IPC_MAX_EXDATA_COUNT];

	void Init(void);
	int SetBody(NpPushJsonValue *value);
	int AddExData(const char *keyName, NpPushJsonValue *value);
	SceSize GetPayloadSize(void) const;
	int Build(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const;
};

/* TLV wire-format helpers (service/np_push_ipc_tlv.cpp) */
int WriteTlvHeader(SceUInt16 tlvType, SceUInt16 payloadLength, char *outBuffer,
		   SceSize bufferSize, SceSize *outWritten);
int ReadTlvHeader(SceUInt16 expectedType, SceUInt16 *outLength, const char *srcBuffer,
		  SceSize bufferSize, SceSize *outConsumed);

class TlvBase : public NpPushRefCounted {
public:
	explicit TlvBase(SceUInt16 tlvType);
	virtual ~TlvBase();
	virtual SceSize GetValueSize(void) const = 0;
	virtual int Serialize(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const = 0;
	virtual int Unserialize(char *srcBuffer, SceSize bufferSize, SceSize *outConsumed) = 0;

protected:
	SceUInt16 m_tlvType;
};

class TlvString : public TlvBase {
public:
	explicit TlvString(SceUInt16 tlvType);
	virtual ~TlvString();
	virtual SceSize GetValueSize(void) const;
	virtual int Serialize(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const;
	virtual int Unserialize(char *srcBuffer, SceSize bufferSize, SceSize *outConsumed);

	void SetValue(const char *text) { m_text = text; }
	const char *GetValue(void) const { return m_text; }
	SceSize GetLength(void) const { return m_length; }

private:
	const char *m_text;
	SceSize m_length;
};

class TlvJsonValue : public TlvBase {
public:
	explicit TlvJsonValue(SceUInt16 tlvType);
	virtual ~TlvJsonValue();
	virtual SceSize GetValueSize(void) const;
	virtual int Serialize(char *outBuffer, SceSize bufferSize, SceSize *outWritten) const;
	virtual int Unserialize(char *srcBuffer, SceSize bufferSize, SceSize *outConsumed);

	void SetValue(NpPushJsonValue *value) { m_jsonValue = value; }
	NpPushJsonValue *GetValue(void) const { return m_jsonValue; }

private:
	NpPushJsonValue *m_jsonValue;
};

struct InternalCallbackEntry {
	int handleId;
	bool isCommCallback;
	FilterMatchType matchType;
	char dataType[64];
	PushNotificationCallback notificationCb;
	PushCommCallback commCb;
	void *userContext;
};

class Dispatcher : public Ctl::Handler {
public:
	static int InitInstance(void);
	static void TermInstance(void);
	static Dispatcher *GetInstance(void);

	Dispatcher();
	virtual ~Dispatcher();

	int Init(void);
	void Finalize(void);

	int NewIpcProcessContext(IPMI::Session *session, IpcProcessContext **outContext);
	int DeleteIpcProcessContext(IPMI::Session *session);
	IpcProcessContext *FindIpcProcessContext(IPMI::Session *session);

	int RegisterNotificationOnlineId(IPMI::Session *session, const NpPushOnlineId *onlineId);
	int UnregisterNotificationOnlineId(IPMI::Session *session, const NpPushOnlineId *onlineId);
	int PeekNotificationOnlineId(IPMI::Session *session, SceUInt32 *outHasMore,
				     void *outToOnlineId, void *outFromOnlineId,
				     void *outCommId, char *outDataType);
	int GetNotificationOnlineId(IPMI::Session *session, SceUInt32 flags,
				    const char *extKeys, SceInt32 extKeyCount,
				    void *outToOnlineId, void *outFromOnlineId,
				    void *outCommId, char *outDataType,
				    IpcJsonEntry *outBody, IpcJsonEntry *outExtEntries,
				    SceInt32 maxExtCount);
	int RegisterNotificationDataType(IPMI::Session *session, const DataType *dataType,
					 SceInt32 matchType);
	int UnregisterNotificationDataType(IPMI::Session *session, const DataType *dataType);
	int PeekNotificationDataType(IPMI::Session *session, SceUInt32 *outHasMore,
				     void *outToOnlineId, void *outFromOnlineId,
				     void *outCommId, char *outDataType);
	int GetNotificationDataType(IPMI::Session *session, SceUInt32 flags,
				    void *outToOnlineId, void *outFromOnlineId,
				    void *outCommId, char *outDataType,
				    IpcJsonEntry *outBody, IpcJsonEntry *outExtEntries,
				    SceInt32 maxExtCount);

	int RegisterNotificationCallback(const char *dataType, FilterMatchType matchType,
					 PushNotificationCallback callback, void *userContext,
					 int *outHandle);
	int RegisterCommCallback(const char *dataType, FilterMatchType matchType,
				 PushCommCallback callback, void *userContext,
				 int *outHandle);
	int UnregisterCallback(int handleId);

	/* Ctl::Handler */
	virtual void OnCtlReceivedPacket(const RefPtr<Packet> &packet);
	virtual void OnCtlDisconnected(int reason);
	virtual void OnCtlServiceCommPacket(int sessionId, const RefPtr<Packet> &packet);
	virtual void OnCtlServiceQueryRequest(int sessionId, const RefPtr<Packet> &packet);
	virtual void OnCtlServiceQueryResponse(int sessionId, void *callback, void *userArg,
					       int errorCode, const RefPtr<Packet> &packet);

private:
	int ReceiveNotificationPacket(const RefPtr<Packet> &packet);
	int ReceiveServiceCommunicationPacket(const RefPtr<Packet> &packet);
	int ReceiveServiceQueryRequestPacket(const RefPtr<Packet> &packet);
	void DispatchCommCallbacks(const void *toOnlineId, const void *fromOnlineId,
				   const void *commId, const char *dataType,
				   const void *bodyPtr, SceSize bodySize);
	void DispatchNotificationCallbacks(const void *toOnlineId, const void *fromOnlineId,
					   const char *dataType, const void *bodyPtr,
					   SceSize bodySize);
	void DispatchToIpcProcesses(int sessionId, const RefPtr<Packet> &packet);
	void DispatchServiceCommCallbacks(int sessionId, const void *toOnlineId,
					  const void *fromOnlineId, const void *commId,
					  const char *dataType, const void *bodyPtr,
					  SceSize bodySize);
	int ExtractNotificationPacket(const RefPtr<Packet> &packet,
				      NpPushOnlineIdData *outToOnlineId,
				      NpPushOnlineIdData *outFromOnlineId,
				      NpPushCommunicationIdData *outCommId,
				      char *outDataType, IpcJsonEntry *outBody);
	void DispatchServiceQueryCallbacks(int sessionId, const void *toOnlineId,
					   const void *fromOnlineId,
					   const FromAccountInfo *fromAccount,
					   const void *commId, int queryType,
					   const char *tranId, const char *dataType,
					   const void *bodyPtr, SceSize bodySize);
	int ExtractServiceQueryRequest(const RefPtr<Packet> &packet,
				       NpPushOnlineIdData *outToOnlineId,
				       NpPushOnlineIdData *outFromOnlineId,
				       FromAccountInfo *outFromAccount,
				       bool *outHasCommId,
				       NpPushCommunicationIdData *outCommId,
				       char *outTranId, int *outQueryType,
				       char *outDataType, IpcJsonEntry *outBody);
	int ExtractServiceQueryResponse(const RefPtr<Packet> &packet,
					NpPushOnlineIdData *outToOnlineId,
					NpPushOnlineIdData *outFromOnlineId,
					FromAccountInfo *outFromAccount,
					bool *outHasCommId,
					NpPushCommunicationIdData *outCommId,
					int *outQueryType, int *outErrorSource,
					int *outReturnCode, char *outDataType,
					IpcJsonEntry *outBody);
	void DispatchServiceQueryResponse(int sessionId, void *callback, void *userArg,
					  int errorCode, const RefPtr<Packet> &packet);
	static int SerializeJsonValue(NpPushJsonValue *value, char **outBuffer, SceSize *outSize);

	NpPushMutex m_mutex;
	AllocList<IpcProcessContext *> m_ipcProcesses;
	AllocList<InternalCallbackEntry> m_callbacks;
	int m_nextCallbackHandle;
	bool m_initialized;
};

/* Service Init/Term helpers (core/np_push_core.cpp) */
int InitService(void);
int TermService(void);

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */

/* Top-level C/C++ entry points called by SceShell startup and plugins */
extern int sceNpPushInit(void);
extern int sceNpPushTerm(void);
extern int sceNpPushStart(void);
extern int shellNpPushRegisterService(const char *servicePrefix, int matchMode,
				      void *callback, void *userContext, int *outHandle);
extern int shellNpPushRegisterTitle(const char *servicePrefix, int matchMode,
				    void *callback, void *userContext, int *outHandle);
extern void shellNpPushUnregister(int handle);

#endif /* NP_PUSH_INTERNAL_H */
