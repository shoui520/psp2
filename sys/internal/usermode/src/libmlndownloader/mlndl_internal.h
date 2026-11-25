#ifndef MLNDL_INTERNAL_H
#define MLNDL_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define MLNDL_PLATFORM_PS3 0
#define MLNDL_PLATFORM_PSP_PC 1
#define MLNDL_PLATFORM_PSP 2
#define MLNDL_PLATFORM_DUID 3
#define MLNDL_PLATFORM_DUID_CONTAINER 4
#define MLNDL_PLATFORM_PSP2_PC 5
#define MLNDL_PLATFORM_PSP2 6
#define MLNDL_PLATFORM_PSP2_CONTAINER 7

#define MLNDL_MODE_PSP2_DIRECT 0x01000106
#define MLNDL_MODE_ENGINE_MASK 0x0F000000
#define MLNDL_MODE_ENGINE_MARLIN 0x01000000
#define MLNDL_MODE_VERSION_MASK 0x00FF0000
#define MLNDL_MODE_VERSION_10 0x00000000
#define MLNDL_MODE_VERSION_11 0x00010000
#define MLNDL_MODE_VERSION_20 0x00020000
#define MLNDL_MODE_EPASSWORD_MASK 0x0000FF00

#define MLNDL_ERR_BASE 0x80103800
#define MLNDL_ERR_MARLIN_BASE 0x80104180
#define MLNDL_ERR_OUT_OF_MEMORY ((int32_t)0x80103813)
#define MLNDL_ERR_INVALID_ARG ((int32_t)0x80103814)
#define MLNDL_ERR_INTERNAL ((int32_t)0x80103812)

#define MLNDL_OK 0
#define MLNDL_RES_NOT_REGISTERED 2
#define MLNDL_RES_NOT_PERSONALIZED 3
#define MLNDL_RES_OTHER_ACCOUNT 4
#define MLNDL_RES_DEVICE_REVOKED 5
#define MLNDL_RES_EXPIRED_CERT 6
#define MLNDL_RES_CLOCK_ERROR 8
#define MLNDL_RES_INVALID_STATE 9
#define MLNDL_RES_SERVER_LIMIT 10
#define MLNDL_RES_HTTP_ERROR 16
#define MLNDL_RES_INTERNAL_ERROR 18
#define MLNDL_RES_NO_MEMORY 19
#define MLNDL_RES_INVALID_PARAM 20
#define MLNDL_RES_SERVICE_UNAVAILABLE 22
#define MLNDL_RES_SERVER_MAINTENANCE 23
#define MLNDL_RES_HEADER_VERSION_MISSING 49
#define MLNDL_RES_HEADER_VERSION_MISMATCH 50
#define MLNDL_RES_HEADER_CONTENT_TYPE_MISSING 51
#define MLNDL_RES_HEADER_CONTENT_TYPE_MISMATCH 52
#define MLNDL_RES_ACTION_SERVER_BAD_RESPONSE 65
#define MLNDL_RES_ACTION_SERVER_UNKNOWN_CODE 66
#define MLNDL_RES_ACTION_SERVER_CODE_9001 67
#define MLNDL_RES_ACTION_SERVER_CODE_9002 68
#define MLNDL_RES_ACTION_SERVER_CODE_9003 69
#define MLNDL_RES_STATUS_UNKNOWN_REASON 71
#define MLNDL_RES_STATUS_BAD_FORMAT 72
#define MLNDL_RES_ABORTED 74
#define MLNDL_RES_TRANSPORT_ERROR 85
#define MLNDL_RES_NO_ENTITLEMENT 113
#define MLNDL_RES_ACTION_SERVER_NO_DETAIL 175

#define MLNDL_REG_REGISTERED 0
#define MLNDL_REG_OTHER_USER 1
#define MLNDL_REG_UNREGISTERED 2
#define MLNDL_REG_UNPERSONALIZED 3
#define MLNDL_REG_REVOKED 4
#define MLNDL_REG_EXPIRED 5
#define MLNDL_REG_BROKEN_CERT 6
#define MLNDL_REG_CLOCK_TAMPERED 7
#define MLNDL_REG_CORRUPTED 8
#define MLNDL_REG_NO_ENTITLEMENT 9
#define MLNDL_REG_ENTITLEMENT_BROKEN 10

#define MLNDL_ACTION_REGISTER 0
#define MLNDL_ACTION_LICENSE 1
#define MLNDL_ACTION_DEREGISTER 2
#define MLNDL_ACTION_SUBSCRIBE 3
#define MLNDL_ACTION_SUBSCRIBE_LINK_ONLY 4
#define MLNDL_ACTION_UNSUBSCRIBE 5

#define MLNDL_CFG_ACCOUNT (1 << 0)
#define MLNDL_CFG_LOGIN (1 << 1)
#define MLNDL_CFG_PASSWORD (1 << 2)
#define MLNDL_CFG_URLS (1 << 4)
#define MLNDL_CFG_ENTITLEMENT (1 << 5)

#define MLNDL_CHECK_ACCOUNT (1 << 0)
#define MLNDL_CHECK_CREDENTIALS (1 << 1)
#define MLNDL_CHECK_REGISTER_URL (1 << 2)
#define MLNDL_CHECK_DEREGISTER_URL (1 << 3)
#define MLNDL_CHECK_LICENSE_URL (1 << 4)
#define MLNDL_CHECK_PERSONALIZE_URL (1 << 5)
#define MLNDL_CHECK_SUBSCRIBE_URL (1 << 6)
#define MLNDL_CHECK_UNSUBSCRIBE_URL (1 << 7)
#define MLNDL_CHECK_ENTITLEMENT (1 << 8)

#define MLNDL_MAX_LICENSE_SIZE 40960
#define MLNDL_MARLIN_HEAP_SIZE 0x100000

typedef struct MlndlBuffer {
	uint32_t size;
	uint8_t *bytes;
} MlndlBuffer;

typedef struct MlndlDateTime {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint32_t microsecond;
} MlndlDateTime;

typedef struct MlndlRtcTick {
	uint64_t tick;
} MlndlRtcTick;

typedef struct MlndlMarlinDate {
	int32_t year;
	int32_t month;
	int32_t day;
	int32_t hour;
	int32_t minute;
	int32_t second;
	int32_t millisecond;
} MlndlMarlinDate;

typedef struct MlndlMarlinPair {
	struct MlndlMarlinPair *next;
	MlndlBuffer *parentId;
	MlndlBuffer *memberId;
} MlndlMarlinPair;

typedef struct MlndlMarlinErrorDetail {
	uint32_t code;
	void *detail;
	MlndlBuffer *message;
} MlndlMarlinErrorDetail;

typedef struct MlndlMarlinActionResult {
	struct MlndlMarlinActionResult *next;
	uint32_t kind;
	void *id;
	void *name;
	void *payload;
	void *extra;
} MlndlMarlinActionResult;

typedef struct MlndlMarlinKind {
	int32_t magic;
	void *descriptor;
} MlndlMarlinKind;

typedef struct MlndlSession {
	MlndlMarlinKind *kind;
	void *marlinHandle;
	uint32_t flags;
} MlndlSession;

typedef struct MlndlStatusInfo {
	int32_t maxConsole;
	int32_t currentConsole;
	int32_t maxTotalConsole;
	int32_t currentTotalConsole;
	int32_t maxUser;
	int32_t currentUser;
} MlndlStatusInfo;

typedef struct MlndlConfig {
	int32_t accountIdLen;
	uint8_t *accountId;
	int32_t entitlementIdLen;
	uint8_t *entitlementId;
	const char *loginId;
	const char *password;
	const char *personalizeUrl;
	const char *registerUrl;
	const char *deregisterUrl;
	const char *licenseUrl;
	const char *subscribeUrl;
	const char *unsubscribeUrl;
	const char *serviceProviderUrn;
} MlndlConfig;

typedef struct MlndlHttpContext {
	int32_t templateId;
	int32_t connectionId;
	int32_t requestId;
	char *url;
} MlndlHttpContext;

typedef struct MlndlHttpHeader {
	const char *name;
	const char *value;
} MlndlHttpHeader;

typedef struct MlndlHttpCallbacks {
	int (*open)(const char *url, MlndlHttpContext **outContext);
	int (*close)(MlndlHttpContext *context);
	int (*addHeader)(MlndlHttpContext *context, const char *name, const char *value);
	int (*request)(MlndlHttpContext *context, int method, uint32_t reqLen,
		const void *reqBody, uint32_t *outLen, void **outBody);
	void (*freeBody)(void *body);
} MlndlHttpCallbacks;

/* SceMarlin imports */
int SceMarlin_83B44D33(void *workMem, uint32_t workSize, void *secMem, uint32_t secSize);
int SceMarlin_C76DA6C0(void);
int SceMarlin_9CD4CE1E(MlndlMarlinKind *kind, void **outHandle, void *outStatus);
int SceMarlin_8FEB4F94(void *handle);
int SceMarlin_9609E2DF(MlndlBuffer *buffer);
int SceMarlin_45B29121(MlndlMarlinActionResult *list);
int SceMarlin_08CAD267(void *handle, MlndlMarlinPair **outList);
int SceMarlin_BCED6CC4(void *handle, MlndlBuffer *serviceProvider, MlndlBuffer *identity,
	MlndlMarlinPair **outList);
int SceMarlin_CCA99803(MlndlMarlinPair *list);
int SceMarlin_3AA04BBF(void *handle, MlndlBuffer *parentId, MlndlBuffer *memberId,
	MlndlMarlinDate *startDate, MlndlMarlinDate *endDate);
int SceMarlin_7E65734B(void *handle, MlndlBuffer *parentId, MlndlBuffer *memberId);
int SceMarlin_E8276179(void *handle, uint32_t infoType, MlndlBuffer **outInfo);
int SceMarlin_236015FA(void *handle);
int SceMarlin_C4FF32EF(void *handle, MlndlBuffer **outRequest, MlndlStatusInfo *statusInfo);
int SceMarlin_14097094(void *handle, MlndlBuffer *response);
int SceMarlin_88AC64AD(void *handle, MlndlBuffer *actionToken, MlndlBuffer *serviceProvider,
	MlndlBuffer *identity, const MlndlHttpCallbacks *callbacks,
	MlndlMarlinActionResult **outResults);

/* SceHttp imports */
int sceHttpCreateTemplate(const char *userAgent, int httpVer, int autoProxyConf);
int sceHttpDeleteTemplate(int tmplId);
int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int enableKeepalive);
int sceHttpDeleteConnection(int connId);
int sceHttpCreateRequestWithURL(int connId, int method, const char *url, uint64_t contentLength);
int sceHttpDeleteRequest(int reqId);
int sceHttpAddRequestHeader(int id, const char *name, const char *value, uint32_t mode);
int sceHttpSendRequest(int reqId, const void *postData, uint32_t size);
int sceHttpGetStatusCode(int reqId, int *statusCode);
int sceHttpReadData(int reqId, void *buffer, uint32_t size);
int sceHttpGetAllResponseHeaders(int reqId, char **header, uint32_t *headerSize);
int sceHttpParseResponseHeader(const char *header, uint32_t headerLen, const char *fieldStr,
	const char **fieldValue, uint32_t *valueLen);
int sceHttpAbortRequest(int reqId);

/* SceNpCommon / SceNpManager / SceNpCommerce2 imports */
int sceNpCommonGetNpEnviroment(char *envBuf);
int SceNpManager_DDF30B0E(char *loginId, void *onlineId, uint8_t *accountId);
int SceNpCommerce2_AA090F5E(uint32_t version, uint32_t *outCtxId);
int SceNpCommerce2_1EC16D19(uint32_t ctxId);
int SceNpCommerce2_2C6028A0(uint32_t ctxId, void *userInfo);
int sceNpCommerce2CreateSessionCreateReq(uint32_t ctxId, uint32_t *outReqId);
int sceNpCommerce2CreateSessionStart(uint32_t reqId);
int sceNpCommerce2CreateSessionGetResult(uint32_t reqId, void *buf, uint32_t bufSize,
	uint32_t *outSize);
int sceNpCommerce2DestroyReq(uint32_t reqId);
int SceNpCommerce2_1BFF9C73(void *sessionInfo, void *buf, uint32_t bufSize);
int SceNpCommerce2_8ECAE666(uint32_t ctxId, uint32_t flag);
int SceNpCommerce2_07C16199(uint32_t ctxId, uint32_t *outReqId);
int SceNpCommerce2_D35F548B(uint32_t reqId);
int SceNpCommerce2_B7387C96(uint32_t reqId, void *buf, uint32_t bufSize, uint32_t *outSize);
int SceNpCommerce2_5953F48D(void *tokenResult, void *buf, uint32_t bufSize);
int SceNpCommerce2_C737A929(uint32_t ctxId, uint32_t *outReqId);
int SceNpCommerce2_2DC2A0F9(uint32_t reqId);
int SceNpCommerce2_7E3C9196(uint32_t reqId, void *buf, uint32_t bufSize, uint32_t *outSize);

/* SceRtcUser / SceDrmBridgeUser imports */
int sceRtcSetTick(MlndlDateTime *dateTime, const MlndlRtcTick *tick);
int sceRtcGetTick(const MlndlDateTime *dateTime, MlndlRtcTick *tick);
int sceDrmBridgeGetCurrentSecureTick(MlndlRtcTick *tick);

/* Engine (mlndl_engine.c) */
int mlndlMarlinInit(void);
int mlndlMarlinTerm(void);
int mlndlCheckRegistration(const MlndlConfig *config, int mode, MlndlSession *session,
	int *outStatus);
int mlndlPersonalize(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo);
int mlndlRegister(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo);
int mlndlDeregister(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo);
int mlndlCheckEntitlement(const MlndlConfig *config, int mode, MlndlSession *session,
	int *outStatus);
int mlndlSubscribe(const MlndlConfig *config, int mode, MlndlSession *session, int linkOnly,
	MlndlStatusInfo *statusInfo);
int mlndlUnsubscribe(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlStatusInfo *statusInfo);
int mlndlGetEntitlementPeriod(const MlndlConfig *config, int mode, MlndlSession *session,
	MlndlDateTime *outStart, MlndlDateTime *outEnd);
int mlndlPurgeExpiredEntitlements(int mode, MlndlSession *session,
	const MlndlDateTime *currentTime);
int mlndlAcquireLicense(const MlndlConfig *config, const char *contentId, int mode,
	MlndlSession *session, uint32_t *outSize, void **outLicense);
int mlndlGetMaxLicenseSize(const MlndlConfig *config, uint32_t *outSize);
int mlndlFreeBuffer(MlndlBuffer *buffer);
int mlndlAllocBuffer(uint32_t size, const void *source, MlndlBuffer **outBuffer);
int mlndlCompareBuffer(const MlndlBuffer *left, const MlndlBuffer *right);

/* HTTP client (mlndl_http.c) */
int mlndlHttpCancel(void);
int mlndlHttpOpen(const char *url, MlndlHttpContext **outContext);
int mlndlHttpClose(MlndlHttpContext *context);
int mlndlHttpAddHeader(MlndlHttpContext *context, const char *name, const char *value);
int mlndlHttpRequest(MlndlHttpContext *context, int isGet, uint32_t reqLen,
	const void *reqBody, uint32_t *outLen, void **outBody);
int mlndlHttpGetResponseHeader(MlndlHttpContext *context, const char *name, char *outValue);
int mlndlHttpAbort(void);
void mlndlHttpFreeBody(void *body);
int mlndlFindSubstring(const char *haystack, const char *needle, const char **outMatch);
int mlndlParseHeaderInt(const char *header, const char *key, int *outValue);
int mlndlParseStatusHeader(MlndlHttpContext *context, const char *headerName,
	MlndlStatusInfo *statusInfo);
int mlndlBuildInfoHeader(int actionKind, uint8_t *outBox);

/* Protocol (mlndl_protocol.c) */
int mlndlMapMarlinError(uint32_t marlinErr);
int mlndlGetDeviceInfo(int mode, MlndlMarlinKind *kind, void *marlinHandle, uint32_t infoType,
	uint32_t *inOutSize, void *outBuffer);
int mlndlValidateSession(int mode, MlndlMarlinKind *kind, const MlndlConfig *config);
int mlndlValidateHandle(int mode, MlndlMarlinKind *kind);
int mlndlValidateConfig(uint16_t requiredMask, const MlndlConfig *config);
int mlndlBuildIdentityBoxes(const MlndlConfig *config, MlndlBuffer **outServiceProvider,
	MlndlBuffer **outAccountBox, MlndlBuffer **outCombinedBox);
int mlndlQueryRegistrationStatus(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int *outStatus);
int mlndlQueryEntitlementStatus(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int *outStatus);
int mlndlAppendQueryParam(uint8_t platform, int paramKind, uint32_t valueLen,
	const void *valueBytes, uint32_t *inOutLen, char *queryBuf);
int mlndlFetchActionToken(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int actionKind, const char *contentId, uint32_t *outLen,
	void **outBody, MlndlStatusInfo *statusInfo);
int mlndlProcessActionToken(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, uint32_t tokenLen, void *tokenBody, int unusedFlag,
	MlndlBuffer **outLicense);
int mlndlProtoRegister(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo);
int mlndlProtoAcquireLicense(const MlndlConfig *config, const char *contentId, int mode,
	MlndlMarlinKind *kind, void *marlinHandle, int unusedFlag, MlndlBuffer **outLicense);
int mlndlProtoDeregister(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo);
int mlndlProtoSubscribe(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int linkOnly, int unusedFlag, MlndlStatusInfo *statusInfo);
int mlndlProtoUnsubscribe(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, int unusedFlag, MlndlStatusInfo *statusInfo);
int mlndlProtoGetValidity(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlDateTime *outStart, MlndlDateTime *outEnd);
int mlndlProtoPurgeExpired(int mode, MlndlMarlinKind *kind, void *marlinHandle,
	const MlndlDateTime *currentTime);
int mlndlProtoRemoveRegistration(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, uint32_t onlyMatching);
int mlndlProtoResetRegistration(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle);
int mlndlCbHttpOpen(const char *url, MlndlHttpContext **outContext);
int mlndlCbHttpClose(MlndlHttpContext *context);
int mlndlCbHttpAddHeader(MlndlHttpContext *context, const char *name, const char *value);
int mlndlCbHttpRequest(MlndlHttpContext *context, int method, uint32_t reqLen,
	const void *reqBody, uint32_t *outLen, void **outBody);
void mlndlCbHttpFree(void *body);
int mlndlProtoPersonalize(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlStatusInfo *statusInfo);
int mlndlProtoPersonalizeChecked(const MlndlConfig *config, int mode, MlndlMarlinKind *kind,
	void *marlinHandle, MlndlStatusInfo *statusInfo);

/* Memory & string utilities (mlndl_mem.c) */
void *mlndlMemSet(void *dest, uint8_t value, uint32_t count);
int mlndlMemCmp(const void *left, const void *right, uint32_t count);
void *mlndlMemCpy(void *dest, const void *src, uint32_t count);
int mlndlHeapInit(int poolSelector, void *memory, uint32_t size);
void *mlndlAlloc(int poolSelector, uint32_t size);
void *mlndlRealloc(int poolSelector, void *block, uint32_t size);
void mlndlFree(int poolSelector, void *block);
void mlndlHeapTerm(int poolSelector);
void *mlndlCopyBytes(void *dest, const void *src, uint32_t count);
int mlndlFormatString(char *dest, int maxLen, const char *format, ...);
char *mlndlStrNCpy(char *dest, const char *src, uint32_t count);
int mlndlStrNCmp(const char *left, const char *right, uint32_t count);

/* NP commerce & configuration (mlndl_np.c) */
int mlndlFetchNpToken(char *outToken, uint32_t tokenBufSize);
int mlndlBuildMdsUrl(const char *servicePath, const char *npEnv, char *outUrl);
int mlndlBuildConfig(uint8_t flags, const char *token, const char *entitlementId,
	MlndlConfig *outConfig);

#endif
