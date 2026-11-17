/*
 * SceNpCommonPs4, NpAsm: the client the exports at 0x8101121c.. forward to. Sony's names for it
 * are not known (no record or string names its files): the exports are named by their NID, the
 * internal names are ours, taken from what the code does.
 */
#ifndef NP_ASM_H
#define NP_ASM_H

#include "../np_common_ps4_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the errors the NpAsm layer returns (0x80559D4X) */
#define NP_ASM_ERROR_ALREADY_INITIALIZED	-2141872831	/* 0x80559D41 */
#define NP_ASM_ERROR_NOT_INITIALIZED		-2141872830	/* 0x80559D42 */
#define NP_ASM_ERROR_INVALID_ARGUMENT		-2141872829	/* 0x80559D43 */
#define NP_ASM_ERROR_ALREADY_CREATED		-2141872828	/* 0x80559D44 */
#define NP_ASM_ERROR_OUT_OF_MEMORY		-2141872827	/* 0x80559D45 */
#define NP_ASM_ERROR_REQUEST_NOT_FOUND		-2141872826	/* 0x80559D46 */
#define NP_ASM_ERROR_ABORTED			-2141872825	/* 0x80559D47 */
#define NP_ASM_ERROR_80559D49			-2141872823
#define NP_ASM_ERROR_80559D4A			-2141872822
#define NP_ASM_ERROR_80559D4B			-2141872821
#define NP_ASM_ERROR_80559D4D			-2141872819
#define NP_ASM_ERROR_80559D4F			-2141872817
#define NP_ASM_ERROR_80552A0A			-2141902326
#define NP_ASM_ERROR_80552A0B			-2141902325

/* the application ID of the caller the NpAsm layer treats as the system's own */
#define NP_ASM_SYSTEM_APP_ID			4098


/* the account's title id as the layer keeps it: 12 characters (NPXX12345_00) and four bytes of padding */
#define NP_ASM_TITLE_ID_SIZE		16
#define NP_ASM_TITLE_SECRET_SIZE	128

/* the system data file that carries the title id and secret of a title (type 4, "NPTD") */
struct NpAsmSystemData {
	SceUInt32 magic;
	SceUInt32 dataSize;
	SceUInt32 reserved0;
	SceUInt32 reserved1;
	SceUInt8 titleId[NP_ASM_TITLE_ID_SIZE];
	SceUInt8 titleSecret[NP_ASM_TITLE_SECRET_SIZE];
};

/* what an application passes to create a request: the allocator the request's HTTP objects use */
struct NpAsmRequestParam {
	int httpContextId;
	struct NpCommonPs4Allocator *allocator;
};

/* a request of the client (48 bytes): its id, the NpManager login it waits on, the HTTP connection of the pending
   call, the parameters it was created with, whether it was aborted, the list link and the results of the last call */
struct NpAsmRequest {
	const void *const *vtable;
	int id;
	int npManagerId;
	void *connection;
	struct NpAsmRequestParam param;
	SceUInt8 aborted;
	struct NpAsmRequest *next;
	void *results;
	int resultCount;
	int maxAge;
	int cacheStatus;
};

/* the encrypted pass phrase of a title's NpComId: the server's npComInfo entry (312 bytes) and the decrypted one (144) */
struct NpAsmNpComInfo {
	SceUInt8 npComId[12];
	SceUInt8 passPhrase[256];
	SceUInt32 passPhraseSize;
	SceUInt8 iv[36];
	SceUInt32 serviceLabel;
};

struct NpAsmNpComInfoPlain {
	SceUInt8 npComId[12];
	SceUInt8 passPhrase[128];
	SceUInt32 serviceLabel;
};

/* the block cipher the library carries (Rijndael with the block and key size as parameters): the schedule and the
   chaining value */
struct NpCommonPs4AesContext {
	int unknown0;
	int unknown1;
	unsigned int blockWords;
	SceUInt8 schedule[964];
	SceUInt8 iv[32];
};

/* a registered request class: the table at 0x8102b170 holds 128 of them, the rest is allocated */
struct NpAsmRegistryEntry {
	struct NpAsmRegistryEntry *next;
	void *callback;
	void *object;
	int extra;
};

struct NpAsmClient;
struct NpAsmTransport;

extern const unsigned char g_npAsmSystemTitleSecret[NP_ASM_TITLE_SECRET_SIZE];	/* 0x81028e5c */
extern SceUInt8 g_npAsmWrapInitialized;		/* 0x8102b078 */

int npAsmRegistryAdd(void *object, void *callback, const int *pExtra);
void npAsmWrapSetInitialized(SceUInt8 value);
SceUInt8 npAsmWrapIsInitialized(void);
void npAsmNotifyServiceError(int result, const void *pOnlineId);
int npAsmIsSystemApp(void);
int npAsmWrapInit(int param1, const int *pParam);
int npAsmWrapTerm(void);
int npAsmWrapConnectionSetTitleId(const void *pTitleId);
int npAsmWrapConnectionClearTitleId(void);
int npAsmWrapConnectionQuery(int a1, int a2);
int npAsmSetTitleId(const void *pTitleId, const void *pTitleSecret, int a3);
int npAsmWrapGetTokenLimitation(void *pExpired, void *pTick);
int npAsmWrapRequestGetMaxAge(int requestId, int *pMaxAge);
int npAsmGetSystemTitleId(void *pTitleId, void *pTitleSecret);
int npAsmGetTitleId(void *pTitleId, void *pTitleSecret);
int npAsmGetTitleIdUnchecked(void *pTitleId, void *pTitleSecret);
int npAsmRequestCreate(const void *pParam);
int npAsmRequestDestroy(int requestId);
int npAsmRequestGetTitleToken(SceInt32 requestId, const char *onlineId, void *buffer, SceUInt8 flag);
int npAsmGetTokenLimitation(void *pExpired, void *pTick);
int npAsmCallbackUpdate(void *pTitleId, void *serviceName, void *buffer);
int npAsmCallbackCheck(void *pTitleId, void *serviceName, void *pValid, void *buffer, int size);
int npAsmRequestGetBaseUrl(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size);
int npAsmRequestGetBaseUrlChecked(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size);
int npAsmDecryptNpComInfo(const struct NpAsmNpComInfo *entry, const void *key, struct NpAsmNpComInfoPlain *plain);
int npAsmRequestGetNpComInfo(int requestId, const void *pOnlineId, const char *npComId, const char *titleId,
	const unsigned char *titleSecret, int serviceLabel, void *pOut);
int npAsmRequestParseCacheControl(struct NpAsmRequest *request);
int npAsmRequestUpdateCacheControl(struct NpAsmRequest *request);
int npAsmRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *id, const char *serviceName, unsigned int *pCount);
int npAsmRequestCopyRelatedTitleIds(int requestId, void *buffer, unsigned int count);
int npAsmRequestGetMaxAge(int requestId, int *pMaxAge);
int npAsmRequestAbort(SceInt32 requestId);
int npAsmSecondaryRequestGetBaseUrl(int requestId, const void *pOnlineId, void *pTitleId, void *serviceName, void *buffer, int size);
int npAsmSecondaryRequestGetNpComInfo(int requestId, const void *pOnlineId, const char *npComId, int serviceLabel, void *pOut);
int npAsmSecondaryRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *id, const char *serviceName,
	unsigned int *pCount);
int npAsmSecondaryWrapRequestGetRelatedTitleIds(int requestId, const void *pOnlineId, const char *id, const char *serviceName,
	unsigned int *pCount);
int npAsmSecondaryRequestCopyRelatedTitleIds(int requestId, void *buffer, unsigned int count);
int npAsmSecondaryRequestGetMaxAge(int requestId, int *pMaxAge);
int npAsmSecondaryReloadTitleId(void);

/* the client (0x81015f9e..): the objects of the layer */
struct NpAsmClient *npAsmClientGet(void);
struct NpAsmClient *npAsmClientGetSecondary(void);
int npAsmClientCreate(int param1, const int *pParam, int notDevelopmentMode);
int npAsmClientDestroy(int next);
struct NpAsmTransport *npAsmClientGetTransport(struct NpAsmClient *client);
void npAsmClientLock(struct NpAsmClient *client);
void npAsmClientUnlock(struct NpAsmClient *client);
struct NpAsmRequest *npAsmClientFindRequest(struct NpAsmClient *client, int requestId);
void npAsmClientAddRequest(struct NpAsmClient *client, struct NpAsmRequest *request);
void npAsmClientRemoveRequest(struct NpAsmClient *client, struct NpAsmRequest *request);
int npAsmClientGetHttpContextId(struct NpAsmClient *client);
struct NpCommonPs4Allocator *npAsmClientGetAllocator(struct NpAsmClient *client);
struct NpCommonPs4Allocator *npAsmClientGetRequestAllocator(struct NpAsmClient *client);
struct NpCommonPs4Allocator *npAsmClientGetAllocatorForBuffers(struct NpAsmClient *client);
int npAsmClientGetTitleId(struct NpAsmClient *client, void *pTitleId, void *pTitleSecret, SceUInt8 check);
int npAsmClientIsSystemTitle(struct NpAsmClient *client);
int npAsmClientSetLimit(struct NpAsmClient *client, int limit);
int npAsmClientSetTitleId(struct NpAsmClient *client, const void *pTitleId, const void *pTitleSecret, SceUInt8 fromSystem);
int npAsmSecondaryClientCreate(void);
int npAsmSecondaryClientDestroy(void);
int npAsmServiceInit(void);
void npAsmServiceTerm(int next);
int npAsmConnectionSetTitleId(void *connection, const void *pTitleId);
int npAsmConnectionClearTitleId(void *connection);
int npAsmConnectionQuery(void *connection, int a1, int a2);

/* a request (0x81013d7c..) */
struct NpAsmRequest *npAsmRequestInit(struct NpAsmRequest *request, const struct NpAsmRequestParam *pParam);
int npAsmRequestGetId(struct NpAsmRequest *request);
int npAsmRequestGetNpManagerId(struct NpAsmRequest *request);
void npAsmRequestSetNpManagerId(struct NpAsmRequest *request, int npManagerId);
void *npAsmRequestGetConnection(struct NpAsmRequest *request);
void npAsmRequestSetConnection(struct NpAsmRequest *request, void *connection);
struct NpAsmRequestParam *npAsmRequestGetParam(struct NpAsmRequest *request);
int npAsmRequestIsAborted(struct NpAsmRequest *request);
int npAsmRequestCancel(struct NpAsmRequest *request);
int npAsmRequestFreeResults(struct NpAsmRequest *request, struct NpCommonPs4Allocator *allocator);

/* the token cache (0x81013364..) and the callbacks of the system's own client */
void *npAsmGetCallbackTable(void);
int npAsmCallbackTableCheck(void *table, void *pTitleId, void *serviceName, void *pValid, void *buffer, int size);
int npAsmCallbackTableUpdate(void *table, void *pTitleId, void *serviceName, void *buffer);
int npAsmTransportGetTitleToken(struct NpAsmTransport *transport, struct NpAsmRequest *request, const void *pOnlineId, void *tokenEntry, int force);
int npAsmTransportIsTokenValid(struct NpAsmTransport *transport, const void *pOnlineId, SceUInt8 *pValid, SceUInt64 *pExpire);
int npAsmCallbackTableSet(void *table, const int *pCallbacks);
void npAsmCallbackTableClear(void *table);
int npAsmTransportGetTokenLimitation(struct NpAsmTransport *transport, void *pExpired, void *pTick);

/* the HTTP connection to the ASM server (0x8101463c..) */
int npAsmConnectionCreate(int httpContextId, struct NpCommonPs4Allocator *allocator, void **pConnection);
int npAsmConnectionDestroy(void *connection);
int npAsmConnectionGetBaseUrl(void *connection, const void *titleId, const void *serviceName, const void *accessToken,
	const void *tokenEntry, void *buffer, int size);
int npAsmConnectionGetNpComInfo(void *connection, const char *titleId, const char *npComId, const void *accessToken,
	const void *tokenEntry, struct NpAsmNpComInfo **pList, unsigned int *pCount);
int npAsmConnectionGetRelatedTitleIds(void *connection, const char *serviceName, const char *id, const void *accessToken,
	const void *tokenEntry, void **pList, unsigned int *pCount);
int npAsmConnectionGetHeaderSize(void *connection, const char *name, unsigned int *pSize);
int npAsmConnectionGetHeader(void *connection, const char *name, char *buffer, unsigned int size);

int npCommonPs4AesInit(struct NpCommonPs4AesContext *context, unsigned int blockBits, unsigned int keyBits, const void *key);
int npCommonPs4AesDecryptBlock(struct NpCommonPs4AesContext *context, const void *in, void *out);

int SceNpCommonPs4_D84BF4B7(int a1);
int SceNpCommonPs4_1096EAD4(int a1);
void *SceNpCommonPs4_A0599F24(void);
extern int sceAppMgrIsDevelopmentMode(void);
extern SceUInt32 SceLibKernel_9F793F84(void);
int sceAppMgrGetSystemDataFile(int type, void *buffer, SceSize size);
SceUInt32 sceNetNtohl(SceUInt32 net32);

#ifdef __cplusplus
}
#endif

#endif
