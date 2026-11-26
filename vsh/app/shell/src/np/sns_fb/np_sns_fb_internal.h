/*
 * np_sns_fb_internal.h: shared internal types, error codes, and prototypes for
 * the SceNpSnsFb service and fblib client (0x81338ADC..0x8134258F, 0x813F85B4..0x813FA7F7).
 */
#ifndef SHELL_NP_SNS_FB_INTERNAL_H
#define SHELL_NP_SNS_FB_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <rtc.h>
#include <np/np_sns_facebook.h>

/* SceNpSnsFb internal error codes (0x80559401..0x80559418) */
#define NP_SNS_FB_ERR_ALREADY_INITIALIZED	-2141875199	/* 0x80559401 */
#define NP_SNS_FB_ERR_NOT_INITIALIZED		-2141875198	/* 0x80559402 */
#define NP_SNS_FB_ERR_INVALID_ARGUMENT		-2141875197	/* 0x80559403 */
#define NP_SNS_FB_ERR_OUT_OF_MEMORY		-2141875196	/* 0x80559404 */
#define NP_SNS_FB_ERR_EXCEEDS_MAX		-2141875195	/* 0x80559405 */
#define NP_SNS_FB_ERR_ABORTED			-2141875194	/* 0x80559406 */
#define NP_SNS_FB_ERR_REQUEST_FAILED		-2141875193	/* 0x80559407 */
#define NP_SNS_FB_ERR_INTERNAL_ERROR		-2141875192	/* 0x80559408 */
#define NP_SNS_FB_ERR_SESSION_INVALID		-2141875191	/* 0x80559409 */
#define NP_SNS_FB_ERR_INVALID_CREDENTIALS	-2141875190	/* 0x8055940A */
#define NP_SNS_FB_ERR_API_ERROR			-2141875189	/* 0x8055940B */
#define NP_SNS_FB_ERR_NOT_SIGNED_IN		-2141875188	/* 0x8055940C */
#define NP_SNS_FB_ERR_ACCESS_NOT_ALLOWED	-2141875186	/* 0x8055940E */
#define NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND		-2141875185	/* 0x8055940F */
#define NP_SNS_FB_ERR_CONFIG_DISABLED		-2141875184	/* 0x80559410 */
#define NP_SNS_FB_ERR_CANCELED_BY_SYSTEM	-2141875183	/* 0x80559411 */
#define NP_SNS_FB_ERR_CHAT_RESTRICTION		-2141875182	/* 0x80559412 */
#define NP_SNS_FB_ERR_XML_PARSE			-2141875181	/* 0x80559413 */
#define NP_SNS_FB_ERR_ACCOUNT_LOCKED		-2141875180	/* 0x80559414 */
#define NP_SNS_FB_ERR_ACCOUNT_CHECKPOINT	-2141875179	/* 0x80559415 */
#define NP_SNS_FB_ERR_ACCOUNT_DISABLED		-2141875178	/* 0x80559416 */
#define NP_SNS_FB_ERR_PASSWORD_CHANGE_REQUIRED	-2141875177	/* 0x80559417 */
#define NP_SNS_FB_ERR_AUTH_CLIENT_ERROR		-2141875176	/* 0x80559418 */

/* libhttp status/buffer error codes used by fblib */
#define NP_SNS_FB_HTTP_ERR_BUFFER_FULL		-2143088606	/* 0x80431022 */
#define NP_SNS_FB_HTTP_ERR_BAD_STATUS		-2143088511	/* 0x80431081 */

/* POSIX errno values returned by fblib low-level routines */
#define NP_SNS_FB_POSIX_ENOMEM			12
#define NP_SNS_FB_POSIX_EINVAL			22
#define NP_SNS_FB_POSIX_ECANCELED		140

/* Facebook Vita application API key and secret */
#define NP_SNS_FB_API_KEY			"5a50948ea6da096edbba782aad6cbbb4"
#define NP_SNS_FB_APP_SECRET			"5d0c42e44385e46c8ef90354f07baf0f"
#define NP_SNS_FB_REG_CATEGORY			"/CONFIG/NP2/FACEBOOK"
#define NP_SNS_FB_USER_AGENT			"PSP2NpSnsFb-agent/1.0.0"

#define NP_SNS_FB_ACCOUNT_MAX			320
#define NP_SNS_FB_SESSION_KEY_MAX		151
#define NP_SNS_FB_SESSION_SECRET_MAX		33
#define NP_SNS_FB_UID_MAX			21
#define NP_SNS_FB_MAX_PERMISSIONS		12
#define NP_SNS_FB_REQ_ID_BASE			0x10000000
#define NP_SNS_FB_REQ_ID_MAX			0x1FFFFFFF

typedef void *(*NpSnsFbAllocCallback)(SceSize size);
typedef void (*NpSnsFbFreeCallback)(void *ptr);

/* Stored Facebook session credentials (461 bytes = 0x1CD) */
struct NpSnsFbSession {
	char sessionKey[NP_SNS_FB_SESSION_KEY_MAX];		/* +0x000 (151 bytes) */
	char sessionSecret[NP_SNS_FB_SESSION_SECRET_MAX];	/* +0x097 (33 bytes) */
	char uid[NP_SNS_FB_UID_MAX];				/* +0x0B8 (21 bytes) */
	char accessToken[256];					/* +0x0CD (256 bytes) */
};

/* Short session info returned to IPMI callers (205 bytes = 0xCD) */
struct NpSnsFbSessionShort {
	char sessionKey[NP_SNS_FB_SESSION_KEY_MAX];		/* +0x000 */
	char sessionSecret[NP_SNS_FB_SESSION_SECRET_MAX];	/* +0x097 */
	char uid[NP_SNS_FB_UID_MAX];				/* +0x0B8 */
};

/* Error detail populated by XML error_response SAX parsing (16 bytes) */
struct NpSnsFbErrorInfo {
	SceInt32 errorCode;					/* +0x00 */
	char *errorMsgBuf;					/* +0x04 */
	SceSize errorMsgBufSize;				/* +0x08 */
	SceUInt8 truncated;					/* +0x0C */
	SceUInt8 tailPad[3];					/* +0x0D */
};

/* Single permission description and grant status (513 bytes = 0x201) */
struct NpSnsFbPermissionValue {
	char header[256];					/* +0x000 */
	char summary[256];					/* +0x100 */
	SceUInt8 granted;					/* +0x200 */
};

/* Output buffer for getPermissionsInfo (4 + 12 * 513 bytes) */
struct NpSnsFbPermissionsInfoResult {
	SceInt32 count;						/* +0x000 */
	NpSnsFbPermissionValue entries[NP_SNS_FB_MAX_PERMISSIONS]; /* +0x004 */
};

/* Output buffer for checkPermissions (516 bytes = 0x204) */
struct NpSnsFbCheckPermissionsResult {
	SceInt32 status;					/* +0x000 */
	char grantedPermissions[256];				/* +0x004 */
	char ungrantedPermissions[256];				/* +0x104 */
};

/* Output buffer for getAccessToken (4104+ bytes: expiration + long token) */
struct NpSnsFbAccessTokenOutput {
	SceUInt64 expiration;					/* +0x000 */
	char accessToken[SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX + 1]; /* +0x008 */
};

/* fblib low-level transport configuration (24 bytes = 0x18) */
struct NpSnsFbLibConfig {
	SceInt32 httpTemplateId;				/* +0x00 */
	const char *apiKey;					/* +0x04 */
	const char *appSecret;					/* +0x08 */
	NpSnsFbAllocCallback allocFunc;				/* +0x0C */
	NpSnsFbFreeCallback freeFunc;				/* +0x10 */
	SceSize workBufSize;					/* +0x14 */
};

/* fblib per-request HTTP cancellation lock & handles (48 bytes = 0x30) */
struct NpSnsFbLibRequest {
	SceKernelLwMutexWork mutexWork;				/* +0x00 (32 bytes) */
	SceUInt8 aborted;					/* +0x20 */
	SceUInt8 requestActive;					/* +0x21 */
	SceUInt8 flagPad[2];					/* +0x22 */
	SceInt32 httpConnId;					/* +0x24 */
	SceInt32 httpReqId;					/* +0x28 */
	SceInt32 reservedTail;					/* +0x2C */
};

/* fblib REST parameter key/value pair */
struct NpSnsFbParamPair {
	const char *key;
	const char *value;
};

struct NpSnsFbParamList {
	SceUInt32 count;
	NpSnsFbParamPair *pairs;
};

/* Stream-publish client init config (12 bytes) */
struct NpSnsFbStreamInitParam {
	SceInt32 httpTemplateId;				/* +0x00 */
	NpSnsFbAllocCallback allocFunc;				/* +0x04 */
	NpSnsFbFreeCallback freeFunc;				/* +0x08 */
};

/* Stream-publish per-call app config (16 bytes) */
struct NpSnsFbStreamAppConfig {
	const char *apiKey;					/* +0x00 */
	const char *appSecret;					/* +0x04 */
	SceSize workBufSize;					/* +0x08 */
	SceInt32 httpTemplateId;				/* +0x0C */
};

/* Stream-publish request context (48 bytes = 0x30) */
struct NpSnsFbStreamRequest {
	NpSnsFbLibRequest *activeLibReq;			/* +0x00 */
	SceUInt32 alignPad;					/* +0x04 */
	SceKernelLwMutexWork mutexWork;				/* +0x08 (32 bytes) */
	SceUInt8 aborted;					/* +0x28 */
	SceUInt8 tailPad[7];					/* +0x29 */
};

/* Base class for fblib heap-allocated objects (vtable 0x8154179C) */
class NpSnsFbAllocObject {
public:
	NpSnsFbAllocObject();
	virtual ~NpSnsFbAllocObject();

	static void *operator new(SceSize size);
	static void operator delete(void *ptr);
};

/* LwMutex wrapper class (vtable 0x815417AC, 40 bytes = 0x28) */
class NpSnsFbLwMutex : public NpSnsFbAllocObject {
public:
	explicit NpSnsFbLwMutex(const char *name);
	virtual ~NpSnsFbLwMutex();

	int lock();
	int unlock();

private:
	SceUInt32 alignPad;					/* +0x04 */
	SceKernelLwMutexWork m_work;				/* +0x08 */
};

/* RAII lock guard for NpSnsFbLwMutex (vtable 0x815417BC, 8 bytes) */
class NpSnsFbScopedLock : public NpSnsFbAllocObject {
public:
	explicit NpSnsFbScopedLock(NpSnsFbLwMutex *mutex);
	virtual ~NpSnsFbScopedLock();

private:
	NpSnsFbLwMutex *m_mutex;				/* +0x04 */
};

/* Linked-list permission entry node (vtable 0x8153CE40, 592 bytes = 0x250) */
class NpSnsFbPermissionNode : public NpSnsFbAllocObject {
public:
	NpSnsFbPermissionNode();
	virtual ~NpSnsFbPermissionNode();

	char permissionName[65];				/* +0x004 */
	NpSnsFbPermissionValue value;				/* +0x045 (513 bytes) */
	SceUInt8 tailPad[2];					/* +0x246 */
	NpSnsFbPermissionNode *prev;				/* +0x248 */
	NpSnsFbPermissionNode *next;				/* +0x24C */
};

/* Ordered map of Facebook permission names to header/summary/granted (vtable 0x8153CE50, 596 bytes) */
class NpSnsFbPermissionMap {
public:
	NpSnsFbPermissionMap();
	virtual ~NpSnsFbPermissionMap();

	NpSnsFbPermissionValue *findOrInsert(const char *permName);
	NpSnsFbPermissionNode *first() const { return m_head.next; }

private:
	NpSnsFbPermissionNode m_head;				/* +0x004 */
};

/* bXCe SAX handler C++ base interface (vtable 0x8153B298 / 0x815417D4) */
class BxceSaxHandler {
public:
	virtual ~BxceSaxHandler();
	virtual int onStartDocument(void *userData);
	virtual int onStartElement(void *userData, const char *name, SceSize nameLen,
				   const void *attrs);
	virtual int onCharacters(void *userData, const char *text, SceSize textLen);
	virtual int onEndElement(void *userData, const char *name, SceSize nameLen);
	virtual int onComment(void *userData, const char *text, SceSize textLen);
	virtual int onProcessingInstruction(void *userData, const char *target, SceSize targetLen,
					    const char *content, SceSize contentLen);
	virtual int onEntityReference(void *userData, int entityKind, const char *entityName,
				      SceSize entityLen);
	virtual int onCdataSection(void *userData, const char *text, SceSize textLen);
	virtual void onEndDocument(void *userData);
	virtual int onXmlDeclaration(void *userData, const char *version, SceSize versionLen);
};

/* bXCe SAX parser handle (216 bytes = 0xD8, allocated via ScebXCe_B8DDE741) */
struct BxceSaxParser {
	SceUInt8 stateStorage[204];				/* +0x00 */
	void *userData;						/* +0xCC */
	BxceSaxHandler *handler;				/* +0xD0 */
	SceUInt8 abortFlag;					/* +0xD4 */
	SceUInt8 tailPad[3];					/* +0xD5 */
};

/* Extern C imports for bXCe SAX parser, SceNpCommon, and Registry */
extern "C" {
extern void *ScebXCe_B8DDE741(SceSize size);
extern void ScebXCe_EC560647(void *ptr);
extern int ScebXCe_C85586BD(BxceSaxParser *parser);
extern int ScebXCe_928189AB(BxceSaxParser *parser);
extern int ScebXCe_FF7CB737(BxceSaxParser *parser, int option);
extern SceUInt32 ScebXCe_4E9A7061(BxceSaxParser *parser, const void *buf, SceSize len,
				  int isFinal);

extern int SceNpCommon_55F7FCF7(SceUInt64 *outMicros);
extern int SceNpCommon_CC8E6AC6(const char *str);
extern void *sceNpCommonMallocNpServerName(SceSize size);
extern void sceNpCommonFreeNpServerName(void *ptr);

extern int sceHttpSetAcceptEncodingGZIPEnabled(SceInt32 id, SceBool enable);

extern int sceRegMgrGetKeyStrForUser(const char *category, const char *name, char *buf,
				     SceSize size);
extern int sceRegMgrSetKeyStrForUser(const char *category, const char *name, const char *buf,
				     SceSize size);
extern int sceRegMgrGetKeyIntForUser(const char *category, const char *name, SceInt32 *outVal);
extern int sceRegMgrSetKeyIntForUser(const char *category, const char *name, SceInt32 val);
extern int sceRegMgrResetRegistryLvForUser(const char *path, SceInt32 level);
}

/* External NP Manager / Community bridges */
extern void *npCommunityAlloc(SceSize size);
extern void npCommunityFree(void *ptr);
extern int npAccountIsSignedIn(void);
extern int npAccountCreateRequest(void);
extern int npAccountDestroyRequest(int reqHandle);
extern int npAccountAbortRequest(int reqHandle);
extern int npAccountGetProfileAndMeta(int reqHandle, void *profileBuf, void *metaBuf);

/* fblib allocator & runtime (np_sns_fb_lib.cpp, 0x813F85B4..0x813FA7F7) */
int npSnsFbMemInitCallbacks(NpSnsFbAllocCallback allocFunc, NpSnsFbFreeCallback freeFunc);
void npSnsFbMemTerm(void);
void *npSnsFbMemAlloc(SceSize size);
void npSnsFbMemFree(void *ptr);

int npSnsFbLibCreateRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest **outReq);
int npSnsFbLibDestroyRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest *req);
int npSnsFbLibAbortRequest(const NpSnsFbLibConfig *config, NpSnsFbLibRequest *req);
int npSnsFbLibAuthLogin(const NpSnsFbLibConfig *config, NpSnsFbSession *outSession,
			NpSnsFbLibRequest *req, const char *email, const char *password,
			SceInt32 *outFbError);
int npSnsFbLibGetLoggedInUser(const NpSnsFbLibConfig *config, const NpSnsFbSession *session,
			      NpSnsFbLibRequest *req, char *outUid, SceInt32 *outFbError);
int npSnsFbLibParsePermissionsInfoResponse(SceInt32 httpConnId, SceInt32 httpReqId,
					   NpSnsFbPermissionMap *permMap,
					   NpSnsFbErrorInfo *errDetail);
int npSnsFbLibParseCheckPermissionsResponse(SceInt32 httpConnId, SceInt32 httpReqId,
					    NpSnsFbPermissionMap *permMap,
					    NpSnsFbErrorInfo *errDetail);
int npSnsFbLibParseAccessTokenResponse(SceInt32 httpConnId, SceInt32 httpReqId,
				       char *outAccessToken, SceUInt64 *outExpires,
				       NpSnsFbErrorInfo *errDetail);

int npSnsFbStreamInit(const NpSnsFbStreamInitParam *param);
int npSnsFbStreamTerm(void);
int npSnsFbStreamCreateRequest(NpSnsFbStreamRequest **outReq);
int npSnsFbStreamDestroyRequest(NpSnsFbStreamRequest *req);
int npSnsFbStreamAbortRequest(NpSnsFbStreamRequest *req);
int npSnsFbStreamPublishSimple(NpSnsFbStreamRequest *req, const NpSnsFbStreamAppConfig *appCfg,
			       const NpSnsFbSession *session, const char *message,
			       const char *attachment, const char *actionLinks,
			       const char *errMsgBuf, SceSize errMsgBufSize,
			       SceInt32 *outFbError);

/* SceNpSnsFbUtil API (np_sns_fb_util.cpp, 0x81338ADC..0x8133C993) */
int npSnsFbUtilInit(void);
int npSnsFbUtilTerm(void);
int npSnsFbUtilIsInitialized(void);
int npSnsFbUtilCreateRequest(void);
int npSnsFbUtilDestroyRequest(SceInt32 reqId);
int npSnsFbUtilAbortRequest(SceInt32 reqId);
int npSnsFbUtilAbortRequestBySystem(SceInt32 reqId);

int npSnsFbUtilCheckAccount(SceInt32 reqId, SceInt32 *outSignedIn, SceInt32 *outChatAllowed);
int npSnsFbUtilAuthAndBind(SceInt32 reqId, const char *email, const char *password,
			   SceInt32 *outFbError);
int npSnsFbUtilPostFeed(SceInt32 reqId, const char *message, const char *attachment,
			const char *actionLinks, char *errMsgBuf, SceSize errMsgBufSize,
			SceInt32 *outFbError);
int npSnsFbUtilGetPermissionsInfo(SceInt32 reqId, SceInt32 langIndex, const char *permList,
				  NpSnsFbPermissionsInfoResult *outResult, char *errMsgBuf,
				  SceSize errMsgBufSize, SceInt32 *outFbError);
int npSnsFbUtilCheckPermissions(SceInt32 reqId, const char *accessToken, const char *permList,
				NpSnsFbCheckPermissionsResult *outResult, char *errMsgBuf,
				SceSize errMsgBufSize, SceInt32 *outFbError);
int npSnsFbUtilGetAccessToken(SceInt32 reqId, const char *proxiedAppId, const char *remoteAppId,
			      const char *permissions, NpSnsFbAccessTokenOutput *outToken,
			      char *errMsgBuf, SceSize errMsgBufSize, SceInt32 *outFbError);
int npSnsFbUtilGetAccessTokenWithPermCheck(SceInt32 reqId, const char *proxiedAppId,
					   const char *remoteAppId, const char *permissions,
					   NpSnsFbAccessTokenOutput *outToken, char *errMsgBuf,
					   SceSize errMsgBufSize, SceInt32 *outFbError);

int npSnsFbUtilGetAccountName(char *outAccount, SceInt32 bufSize);
int npSnsFbUtilGetSessionShort(NpSnsFbSessionShort *outSession);
int npSnsFbUtilGetShareConfig(SceInt32 *outShareTrophy, SceInt32 *outSharePurchase,
			      SceInt32 *outShareRating, SceInt32 *outGameIntegrations);
int npSnsFbUtilSetShareConfig(const SceInt32 *shareTrophy, const SceInt32 *sharePurchase,
			      const SceInt32 *shareRating, const SceInt32 *gameIntegrations);
int npSnsFbUtilClearAllConfig(void);
int npSnsFbUtilClearSessionConfig(void);
int npSnsFbUtilClearShareConfig(void);
int npSnsFbUtilIsShareTrophyEnabled(SceInt32 *outEnabled);
int npSnsFbUtilIsSharePurchaseEnabled(SceInt32 *outEnabled);
int npSnsFbUtilIsShareRatingEnabled(SceInt32 *outEnabled);
SceBool npSnsFbUtilIsAccountBound(void);

#endif /* SHELL_NP_SNS_FB_INTERNAL_H */
