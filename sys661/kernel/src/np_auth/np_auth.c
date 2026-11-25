/*
 * ePSP 6.61 sceNpAuth (np_auth.prx)
 * Reimplemented from 3.650.011-TOOL pcff/elf/kd/np_auth.prx
 */

#include <stdint.h>
#include <stddef.h>

typedef int32_t SceUID;
typedef uint32_t SceSize;
typedef uint64_t SceNpTime;

/* NP Auth & Ticket error codes (Sony SDK np_error.h) */
#define SCE_NP_ERROR_STACK_TOO_SHORT                ((int)0x80550004)
#define SCE_NP_ERROR_INVALID_ARGUMENT               ((int)0x80550003)

#define SCE_NP_AUTH_ERROR_ALREADY_INITIALIZED       ((int)0x80550301)
#define SCE_NP_AUTH_ERROR_NOT_INITIALIZED           ((int)0x80550302)
#define SCE_NP_AUTH_ERROR_EINVAL                    ((int)0x80550303)
#define SCE_NP_AUTH_ERROR_ENOMEM                    ((int)0x80550304)
#define SCE_NP_AUTH_ERROR_ESRCH                     ((int)0x80550305)
#define SCE_NP_AUTH_ERROR_EBUSY                     ((int)0x80550306)
#define SCE_NP_AUTH_ERROR_ABORTED                   ((int)0x80550307)
#define SCE_NP_AUTH_ERROR_INVALID_SERVICE_ID        ((int)0x80550308)
#define SCE_NP_AUTH_ERROR_INVALID_CREDENTIAL        ((int)0x80550309)
#define SCE_NP_AUTH_ERROR_INVALID_ENTITLEMENT_ID    ((int)0x8055030a)
#define SCE_NP_AUTH_ERROR_INVALID_DATA_LENGTH       ((int)0x8055030b)
#define SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VER    ((int)0x8055030c)
#define SCE_NP_AUTH_ERROR_STACKSIZE_TOO_SHORT       ((int)0x8055030d)
#define SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND    ((int)0x8055030f)
#define SCE_NP_AUTH_ERROR_SIGNED_OUT                ((int)0x80550310)
#define SCE_NP_AUTH_ERROR_REQUEST_LIMIT             ((int)0x80550381)
#define SCE_NP_AUTH_ERROR_ALREADY_SET               ((int)0x80550382)
#define SCE_NP_AUTH_ERROR_SHORT_READ                ((int)0x80550384)
#define SCE_NP_AUTH_ERROR_SSL_CERT_EXPIRED          ((int)0x80550385)
#define SCE_NP_AUTH_ERROR_SSL_CERT_REVOKED          ((int)0x80550386)
#define SCE_NP_AUTH_ERROR_SSL_CERT_COMMON_NAME      ((int)0x80550387)
#define SCE_NP_AUTH_ERROR_SSL_CERT_UNTRUSTED        ((int)0x80550388)
#define SCE_NP_AUTH_ERROR_SSL_CERT_OTHER            ((int)0x80550389)

#define SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT       ((int)0x80550503)
#define SCE_NP_MANAGER_ERROR_INVALID_TICKET_SIZE    ((int)0x80550505)
#define SCE_NP_UTIL_ERROR_PARSER_FAILED             ((int)0x80550603)

#define SCE_HTTP_ERROR_SSL                          ((int)0x80431075)
#define SCE_HTTPS_ERROR_CERT                        ((int)0x80435060)

#define SCE_NP_TICKET_MAX_SIZE                      65536u
#define SCE_NP_SERVICE_ID_SIZE                      24u
#define SCE_NP_ENTITLEMENT_ID_SIZE                  32u
#define SCE_NP_COOKIE_MAX_SIZE                      1024u
#define SCE_NP_TICKET_PARAM_DATA_LEN                256u
#define SCE_NP_TICKET_PARAM_MAX                     12

/* Imports */
extern int sceNpCore_0366DAB6(char *buf, uint32_t size);
extern int sceNpCore_04096629(int idx, char *buf, uint32_t size);
extern int sceNpCore_243690EE(char *buf, uint32_t size);
extern int sceNpCore_5145344F(int idx, char *name, uint32_t nameSize, char *val, uint32_t valSize);
extern int sceNpCore_515B65E8(int idx, const char *val, uint32_t len);
extern int sceNpCore_8AFAB4A0(void *node, char *buf, uint32_t size);
extern int sceNpCore_9218ACF6(char *name, uint32_t nameSize, char *val, uint32_t valSize);
extern int sceNpCore_B13D27CA(char *url, uint32_t size);
extern int sceNpCore_E7AED5A3(char *buf, uint32_t size);

extern int sceKernelQueryMemoryInfo(uint32_t addr, uint32_t *partitionId, uint32_t *blockId);
extern int sceNetFreeThreadinfo(SceUID thid);

extern int sceNet_lib_B5CE388A(char *dst, const char *src, uint32_t size);
extern int sceNet_lib_CF705E46(char *dst, const char *fmt, ...);
extern uint32_t sceNet_lib_D8722983(const char *str);
extern int sceNet_lib_E0A81C7C(const char *lhs, const char *rhs, uint32_t n);

extern int sceParseUri_49E950EC(char *dst, uint32_t *outLen, uint32_t maxLen, const char *src);
extern int sceParseHttp_AD7BFDEF(const char *headers, uint32_t headersLen, const char *name, const char **outVal, uint32_t *outLen);

extern int sceHttpGetContentLength(int reqId, uint64_t *len);
extern int sceHttpSetRecvTimeOut(int tmplId, uint32_t timeout);
extern int sceHttpAddExtraHeader(int id, const char *name, const char *value, uint32_t mode);
extern int sceHttpGetStatusCode(int reqId, int *statusCode);
extern int sceHttpDeleteConnection(int connId);
extern int sceHttpSetConnectTimeOut(int tmplId, uint32_t timeout);
extern int sceHttpSetSendTimeOut(int tmplId, uint32_t timeout);
extern int sceHttpCreateTemplate(const char *agent, int httpVer, int autoProxy);
extern int sceHttpDeleteRequest(int reqId);
extern int sceHttp_AB1540D5(int reqId, int *sslErr, int *certErr);
extern int sceHttpCreateRequestWithURL(int connId, int method, const char *url, uint64_t contentLen);
extern int sceHttpSendRequest(int reqId, const void *data, uint32_t dataSize);
extern int sceHttpAbortRequest(int reqId);
extern int sceHttpCreateConnectionWithURL(int tmplId, const char *url, int enableKeepalive);
extern int sceHttpGetAllHeader(int reqId, char **header, uint32_t *headerSize);
extern int sceHttpReadData(int reqId, void *data, uint32_t size);
extern int sceHttpDeleteTemplate(int tmplId);
extern int sceHttp_3C478044(int tmplId, int val);
extern int sceHttp_87F1E666(int tmplId, int val);

extern int sceNp_B4063F7A(char *buf);
extern int sceNp_B819A0C8(char *user, char *pass, void *opt);
extern int sceNp_E24DA399(void);

extern int sceKernelCpuSuspendIntr(void);
extern void *sceKernelMemset(void *dst, int c, SceSize n);
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize n);
extern void sceKernelCpuResumeIntr(int intr);

extern int sceKernelGetModuleGPByAddress(uint32_t addr, uint32_t *outGp);

extern int sceKernelSendMbx(SceUID mbxid, void *msg);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelWaitThreadEnd(SceUID thid, uint32_t *timeout);
extern int sceKernelReferVplStatus(SceUID uid, void *status);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int initPriority, int stackSize, uint32_t attr, void *option);
extern SceUID sceKernelCreateVpl(const char *name, int part, int attr, uint32_t size, void *opt);
extern SceUID sceKernelCreateMbx(const char *name, uint32_t attr, void *opt);
extern int sceKernelDeleteMbx(SceUID mbxid);
extern int sceKernelDeleteVpl(SceUID uid);
extern int sceKernelReceiveMbx(SceUID mbxid, void **pmsg, uint32_t *timeout);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelTryAllocateVpl(SceUID uid, uint32_t size, void **data);
extern int sceKernelFreeVpl(SceUID uid, void *data);

/* Kernel VPL status layout (52 bytes) */
typedef struct SceKernelVplStatus {
	SceSize size;
	char name[32];
	uint32_t attr;
	int poolSize;
	int freeSize;
	int numWaitThreads;
} SceKernelVplStatus;

/* Kernel thread creation option (8 bytes) */
typedef struct SceKernelThreadOptParam {
	SceSize size;
	SceUID stackMpid;
} SceKernelThreadOptParam;

/* Kernel mailbox message header (8 bytes) */
typedef struct SceKernelMsgPacket {
	struct SceKernelMsgPacket *next;
	uint8_t msgPriority;
	uint8_t pad[3];
} SceKernelMsgPacket;

/* Sony SDK NP Auth public types (np_auth.h) */
typedef struct SceNpDate {
	uint16_t year;
	uint8_t month;
	uint8_t day;
} SceNpDate;

typedef struct SceNpTicketVersion {
	uint16_t major;
	uint16_t minor;
} SceNpTicketVersion;

typedef int (*SceNpAuthCallback)(int id, int result, void *arg);

typedef struct SceNpAuthRequestParameter {
	SceSize size;
	SceNpTicketVersion version;
	const char *serviceId;
	const void *cookie;
	SceSize cookieSize;
	const char *entitlementId;
	uint32_t consumedCount;
	SceNpAuthCallback ticketCb;
	void *cbArg;
} SceNpAuthRequestParameter;

typedef struct SceNpEntitlement {
	char id[SCE_NP_ENTITLEMENT_ID_SIZE];
	SceNpTime createdDate;
	SceNpTime expireDate;
	uint32_t type;
	int32_t remainingCount;
	uint32_t consumedCount;
	uint8_t padding[4];
} SceNpEntitlement;

typedef struct SceNpAuthMemStat {
	int poolSize;
	int maxUsedSize;
	int freeSize;
} SceNpAuthMemStat;

/* Internal NP Auth structures */
typedef struct SceNpAuthParamNode {
	struct SceNpAuthParamNode *next; /* 0x00 */
	uint32_t id;                     /* 0x04 */
	uint32_t len;                    /* 0x08 */
	char data[];                     /* 0x0c */
} SceNpAuthParamNode;

typedef struct SceNpAuthRequest {
	struct SceNpAuthRequest *next;   /* 0x00 */
	int reqId;                       /* 0x04 */
	uint8_t state;                   /* 0x08 */
	uint8_t hasEntitlement;          /* 0x09 */
	uint8_t useConsoleId;            /* 0x0a */
	uint8_t statePad;                /* 0x0b */
	SceNpTicketVersion version;      /* 0x0c */
	SceNpAuthParamNode *paramsHead;  /* 0x10 */
	SceNpAuthParamNode *paramsTail;  /* 0x14 */
	uint32_t activeParamMask;        /* 0x18 */
	SceNpAuthCallback preCb;         /* 0x1c */
	SceNpAuthCallback doneCb;        /* 0x20 */
	void *cbArg;                     /* 0x24 */
	int httpReqId;                   /* 0x28 */
	uint32_t httpPad;                /* 0x2c */
	SceKernelMsgPacket mbxHeader;    /* 0x30 */
	int64_t msgReqId;                /* 0x38 */
	struct SceNpAuthRequest *self;   /* 0x40 */
	uint32_t selfPad;                /* 0x44 */
	uint32_t preGp;                  /* 0x48 */
	uint32_t doneGp;                 /* 0x4c */
} SceNpAuthRequest;

typedef struct SceNpAuthMbxMessage {
	SceKernelMsgPacket header;       /* 0x00 */
	int64_t reqId;                   /* 0x08 */
	SceNpAuthRequest *req;           /* 0x10 */
} SceNpAuthMbxMessage;

typedef struct SceNpAuthTicketBuf {
	const uint8_t *buf;              /* 0x00 */
	uint8_t verBytes[4];             /* 0x04 */
	uint32_t size;                   /* 0x08 */
	uint32_t offset;                 /* 0x0c */
} SceNpAuthTicketBuf;

typedef struct SceNpAuthV3EntStream {
	uint16_t tag;                    /* 0x00 */
	uint16_t len;                    /* 0x02 */
	const uint8_t *ptr;              /* 0x04 */
	uint32_t pos;                    /* 0x08 */
} SceNpAuthV3EntStream;

typedef struct SceNpAuthV3EntHeader {
	uint8_t flags;                   /* 0x00 */
	uint8_t idLen;                   /* 0x01 */
} SceNpAuthV3EntHeader;

typedef struct SceNpAuthVersionHandler {
	uint16_t major;
	uint16_t minor;
	void *handler;
} SceNpAuthVersionHandler;

typedef int (*SceNpAuthTicketParamFn)(SceNpAuthTicketBuf *, int, void *);
typedef int (*SceNpAuthEntListFn)(SceNpAuthTicketBuf *, uint8_t *, uint32_t);
typedef int (*SceNpAuthEntFindFn)(SceNpAuthTicketBuf *, const char *, SceNpEntitlement *);

/* Forward declarations */
static int npAuthGetTicketParamV2(SceNpAuthTicketBuf *tbuf, int ticketParamId, void *outBuf);
static int npAuthGetTicketParamV3(SceNpAuthTicketBuf *tbuf, int ticketParamId, void *outBuf);
static int npAuthListEntitlementsV2(SceNpAuthTicketBuf *tbuf, uint8_t *outBuf, uint32_t maxCount);
static int npAuthListEntitlementsV3(SceNpAuthTicketBuf *tbuf, uint8_t *outBuf, uint32_t maxCount);
static int npAuthFindEntitlementV2(SceNpAuthTicketBuf *tbuf, const char *entId, SceNpEntitlement *outEnt);
static int npAuthFindEntitlementV3(SceNpAuthTicketBuf *tbuf, const char *entId, SceNpEntitlement *outEnt);
static int npAuthGetEntitlementById(const void *buf, uint32_t len, const char *entId, SceNpEntitlement *outEnt);
static int npAuthSeekTicketTag(SceNpAuthTicketBuf *tbuf, uint16_t targetTag, int skipRoot);
static int npAuthReadTicketField(SceNpAuthTicketBuf *tbuf, void *outBuf);
static int npAuthReadEntitlementIdV3(SceNpAuthV3EntStream *stream, const char *prefix, uint32_t prefixLen, const SceNpAuthV3EntHeader *hdr, char *outId);
static int npAuthReadEntitlementBodyV3(SceNpAuthV3EntStream *stream, const SceNpAuthV3EntHeader *hdr, SceNpEntitlement *outEnt);

int npAuthWorkerThread(SceSize args, void *argp);
static int npAuthCreateRequestInternal(int *outId, const SceNpTicketVersion *ver, const char *serviceId, int useCid);
static int npAuthDestroyRequestInternal(int reqId);
static SceNpAuthRequest *npAuthFindRequest(int reqId, SceNpAuthRequest **outPrev);
static int npAuthBuildPostBody(SceNpAuthRequest *req, char *buf, uint32_t bufSize);
static int npAuthSetCredentials(SceNpAuthRequest *req, const char *user, const char *pass);
static int npAuthSetEntitlementConsume(SceNpAuthRequest *req, const char *entId, uint32_t consumed);
static int npAuthSetCookie(SceNpAuthRequest *req, const char *cookie, uint32_t cookieLen);
static int npAuthSetParam(SceNpAuthRequest *req, uint32_t keyId, const void *data, uint32_t len);
static int npAuthClearParam(SceNpAuthRequest *req, uint32_t keyId);
static int npAuthExecuteRequest(SceNpAuthRequest *req);
static int npAuthSendHttpPost(SceNpAuthRequest *req, const void *postData, uint32_t postLen);
static int npAuthParseHttpResponse(SceNpAuthRequest *req);
static int npAuthReadHttpBody(SceNpAuthRequest *req, void *buf, uint32_t size);
static int npAuthOpenHttpConnection(int *outTmpl, int *outConn, char *agentBuf, uint32_t agentSize, char *urlBuf, uint32_t urlSize);
static int npAuthTranslateHttpError(int httpReqId, int err);

int sceNpAuth_61BB18B3(int reqId);
int sceNpAuth_72BB0467(int reqId);
int sceNpAuth_B714FBDD(int reqId, const char *user, const char *pass);
int sceNpAuth_CE85B3B8(int *outId, const SceNpTicketVersion *ver, const char *serviceId, int useCid);
int sceNpAuth_D99455DD(int reqId);
int sceNpAuth_DAD65284(int reqId, SceNpAuthCallback preCb, SceNpAuthCallback doneCb, void *arg);

/* .rodata tables */
static const struct {
	uint16_t type;
	uint16_t size;
} g_ticketV2Schema[11] = {
	{ 0x0008, 0x0014 },
	{ 0x0001, 0x0004 },
	{ 0x0007, 0x0008 },
	{ 0x0007, 0x0008 },
	{ 0x0002, 0x0008 },
	{ 0x0004, 0x0020 },
	{ 0x0008, 0x0004 },
	{ 0x0004, 0x0004 },
	{ 0x0008, 0x0018 },
	{ 0x0001, 0x0004 },
	{ 0x0007, 0x0008 },
};

static const struct {
	uint32_t id;
	uint16_t type;
	uint16_t size;
} g_ticketV3Schema[12] = {
	{ 0,  0x0008, 0x0014 },
	{ 1,  0x0001, 0x0004 },
	{ 2,  0x0007, 0x0008 },
	{ 3,  0x0007, 0x0008 },
	{ 4,  0x0002, 0x0008 },
	{ 5,  0x0004, 0x0020 },
	{ 6,  0x0008, 0x0004 },
	{ 7,  0x0004, 0x0004 },
	{ 8,  0x0008, 0x0018 },
	{ 11, 0x3011, 0x0004 },
	{ 9,  0x0001, 0x0004 },
	{ 10, 0x0007, 0x0008 },
};

/* .data */
static SceUID g_vplId = -1;               /* 0x3890 */
static int32_t g_vplMaxUsed = -1;         /* 0x3894 */
static char g_npHeaderVal[12] = "        "; /* 0x3898 */

static const SceNpTicketVersion g_supportedVers[] = {
	{ 2, 1 },
	{ 3, 0 },
	{ 0, 0 }
};

static SceNpAuthVersionHandler g_paramHandlers[] = {
	{ 2, 1, (void *)npAuthGetTicketParamV2 },
	{ 3, 0, (void *)npAuthGetTicketParamV3 },
	{ 0, 0, NULL }
};

static SceNpAuthVersionHandler g_entByIdHandlers[] = {
	{ 2, 1, (void *)npAuthFindEntitlementV2 },
	{ 3, 0, (void *)npAuthFindEntitlementV3 },
	{ 0, 0, NULL }
};

static SceNpAuthVersionHandler g_entListHandlers[] = {
	{ 2, 1, (void *)npAuthListEntitlementsV2 },
	{ 3, 0, (void *)npAuthListEntitlementsV3 },
	{ 0, 0, NULL }
};

/* .bss */
static SceUID g_mbxId;                    /* 0x3900 */
static uint32_t g_stackSize;              /* 0x3904 */
static SceUID g_threadId;                 /* 0x3908 */
static int g_initialized;                 /* 0x390c */
static SceNpAuthRequest *g_reqList;       /* 0x3910 */
static int g_lastReqId;                   /* 0x3914 */
static SceNpAuthVersionHandler *g_curParamHandler;    /* 0x3918 */
static SceNpAuthVersionHandler *g_curEntIdHandler;    /* 0x391c */
static SceNpAuthVersionHandler *g_curEntListHandler;  /* 0x3920 */

/* === npAuthCreateVpl @ 0x0000 === */
static __attribute__((noinline)) int npAuthCreateVpl(int part, uint32_t size, const char *name)
{
	SceUID vpl = sceKernelCreateVpl(name, part, 0, size, NULL);
	if (vpl >= 0) {
		g_vplId = vpl;
		g_vplMaxUsed = 0;
	}
	return vpl;
}

/* === npAuthDeleteVpl @ 0x0044 === */
static __attribute__((noinline)) void npAuthDeleteVpl(void)
{
	if (g_vplId > 0) {
		sceKernelDeleteVpl(g_vplId);
		g_vplId = -1;
	}
}

/* === npAuthGetVplStat @ 0x0080 === */
static __attribute__((noinline)) int npAuthGetVplStat(SceNpAuthMemStat *outStat)
{
	SceKernelVplStatus status;
	if (outStat == NULL) {
		return (int)0x80550101;
	}
	status.size = sizeof(SceKernelVplStatus);
	int res = sceKernelReferVplStatus(g_vplId, &status);
	if (res >= 0) {
		sceKernelMemset(outStat, 0, sizeof(SceNpAuthMemStat));
		outStat->maxUsedSize = g_vplMaxUsed;
		outStat->freeSize = status.freeSize;
		outStat->poolSize = status.poolSize;
		res = 0;
	}
	return res;
}

/* === npAuthAlloc @ 0x0104 === */
static __attribute__((noinline)) void *npAuthAlloc(uint32_t size)
{
	SceKernelVplStatus status;
	void *ptr;
	if (sceKernelTryAllocateVpl(g_vplId, size, &ptr) < 0) {
		return NULL;
	}
	status.size = sizeof(SceKernelVplStatus);
	if (sceKernelReferVplStatus(g_vplId, &status) == 0) {
		int32_t used = status.poolSize - status.freeSize;
		if (used > g_vplMaxUsed) {
			g_vplMaxUsed = used;
		}
	}
	return ptr;
}

/* === npAuthFree @ 0x0178 === */
static __attribute__((noinline)) void npAuthFree(void *ptr)
{
	if (ptr != NULL) {
		sceKernelFreeVpl(g_vplId, ptr);
	}
}

static __inline__ __attribute__((always_inline)) void npAuthShutdownWorker(void)
{
	SceNpAuthRequest quitReq;
	g_stackSize = 0;
	npAuthDeleteVpl();
	sceKernelMemset(&quitReq, 0, sizeof(quitReq));
	quitReq.msgReqId = 0;
	if (g_mbxId > 0) {
		sceKernelSendMbx(g_mbxId, &quitReq.mbxHeader);
	}
	if (g_threadId > 0) {
		if (sceKernelWaitThreadEnd(g_threadId, NULL) >= 0) {
			sceKernelDeleteThread(g_threadId);
		}
		sceNetFreeThreadinfo(g_threadId);
		g_threadId = 0;
	}
	if (g_mbxId > 0) {
		sceKernelDeleteMbx(g_mbxId);
		g_mbxId = 0;
	}
}

/* === sceNpAuth_A1DE86F8 (sceNpAuthInit) @ 0x01a0 === */
int sceNpAuth_A1DE86F8(uint32_t poolSize, uint32_t stackSize, int prio)
{
	SceKernelThreadOptParam threadOpt;
	uint32_t partitionId;
	int res;

	if (_sceKernelCheckThreadStack() < 0xfd0) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (stackSize < 0x3800) {
		return SCE_NP_AUTH_ERROR_STACKSIZE_TOO_SHORT;
	}

	if (g_initialized > 0) {
		res = SCE_NP_AUTH_ERROR_ALREADY_INITIALIZED;
	} else {
		res = sceKernelCreateMbx("SceNpAuthWorker", 0, NULL);
		if (res >= 0) {
			g_mbxId = res;
			res = sceKernelQueryMemoryInfo((uint32_t)(uintptr_t)&g_initialized, &partitionId, NULL);
		}
		if (res >= 0) {
			res = npAuthCreateVpl((int)partitionId, poolSize, "SceAuthMain");
		}
		if (res >= 0) {
			g_stackSize = stackSize;
			sceKernelMemset(&threadOpt, 0, sizeof(threadOpt));
			threadOpt.size = sizeof(threadOpt);
			threadOpt.stackMpid = (SceUID)partitionId;
			res = sceKernelCreateThread("SceNpAuthWorker", npAuthWorkerThread, prio, (int)stackSize, 0, &threadOpt);
			if (res >= 0) {
				g_threadId = res;
				res = sceKernelStartThread(res, 0, NULL);
			}
		}
		if (res >= 0) {
			g_initialized = 1;
			return 0;
		}
	}

	npAuthShutdownWorker();
	return res;
}

/* === sceNpAuth_CD86A656 (sceNpAuthCreateStartRequest) @ 0x03a8 === */
int sceNpAuth_CD86A656(const SceNpAuthRequestParameter *reqParam)
{
	char user[80];
	char pass[48];
	int reqId = 0;

	if (_sceKernelCheckThreadStack() < 0xf70) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (reqParam == NULL || reqParam->size != sizeof(SceNpAuthRequestParameter)) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}

	int res = sceNpAuth_CE85B3B8(&reqId, &reqParam->version, reqParam->serviceId, 0);
	if (res >= 0) {
		res = sceNp_B819A0C8(user, pass, NULL);
	}
	if (res >= 0) {
		res = sceNpAuth_B714FBDD(reqId, user, pass);
	}
	if (res >= 0 && reqParam->cookie != NULL) {
		if (g_initialized != 0) {
			SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
			if (req == NULL) {
				res = SCE_NP_AUTH_ERROR_ESRCH;
			} else {
				res = npAuthSetCookie(req, (const char *)reqParam->cookie, reqParam->cookieSize);
			}
		} else {
			res = SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
		}
	}
	if (res >= 0 && reqParam->entitlementId != NULL) {
		if (g_initialized == 0) {
			res = SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
		} else {
			SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
			if (req == NULL) {
				res = SCE_NP_AUTH_ERROR_ESRCH;
			} else {
				res = npAuthSetEntitlementConsume(req, reqParam->entitlementId, reqParam->consumedCount);
			}
		}
	}
	if (res >= 0) {
		res = sceNpAuth_DAD65284(reqId, NULL, reqParam->ticketCb, reqParam->cbArg);
	}
	if (res < 0) {
		if (reqId != 0) {
			sceNpAuth_72BB0467(reqId);
		}
		return res;
	}
	return reqId;
}

/* === sceNpAuth_72BB0467 (sceNpAuthDestroyRequest) @ 0x0568 === */
int sceNpAuth_72BB0467(int reqId)
{
	if (_sceKernelCheckThreadStack() < 0xfe0) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	return npAuthDestroyRequestInternal(reqId);
}

/* === sceNpAuth_D99455DD (sceNpAuthAbortRequest) @ 0x05c4 === */
int sceNpAuth_D99455DD(int reqId)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	if (reqId == 0) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	int state = sceNpAuth_61BB18B3(reqId);
	if (state >= 0) {
		if (state < 2) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 3u) {
				req->state = 3;
			}
			sceKernelCpuResumeIntr(intr);
			return 0;
		}
		if (state == 2) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 3u) {
				req->state = 3;
			}
			sceKernelCpuResumeIntr(intr);
			if (req->httpReqId != 0) {
				return sceHttpAbortRequest(req->httpReqId);
			}
			return 0;
		}
	}
	return SCE_NP_AUTH_ERROR_ABORTED;
}

/* === sceNpAuth_CE85B3B8 @ 0x06f0 === */
int sceNpAuth_CE85B3B8(int *outId, const SceNpTicketVersion *ver, const char *serviceId, int useCid)
{
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	if (outId == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	return npAuthCreateRequestInternal(outId, ver, serviceId, useCid);
}

/* === sceNpAuth_B714FBDD @ 0x072c === */
int sceNpAuth_B714FBDD(int reqId, const char *user, const char *pass)
{
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	return npAuthSetCredentials(req, user, pass);
}

/* === sceNpAuth_DAD65284 @ 0x07a0 === */
int sceNpAuth_DAD65284(int reqId, SceNpAuthCallback preCb, SceNpAuthCallback doneCb, void *arg)
{
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	if (reqId == 0 || doneCb == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	int intr = sceKernelCpuSuspendIntr();
	req->preCb = preCb;
	if (sceKernelGetModuleGPByAddress((uint32_t)(uintptr_t)preCb, &req->preGp) < 0) {
		register uint32_t gpReg asm("gp");
		req->preGp = gpReg;
	}
	req->doneCb = doneCb;
	if (sceKernelGetModuleGPByAddress((uint32_t)(uintptr_t)doneCb, &req->doneGp) < 0) {
		register uint32_t gpReg asm("gp");
		req->doneGp = gpReg;
	}
	req->msgReqId = (int64_t)reqId;
	req->cbArg = arg;
	req->self = req;
	sceKernelCpuResumeIntr(intr);

	int res = sceKernelSendMbx(g_mbxId, &req->mbxHeader);
	if (res >= 0) {
		intr = sceKernelCpuSuspendIntr();
		if (req->state <= 1u) {
			req->state = 1;
		}
		sceKernelCpuResumeIntr(intr);
	}
	return res;
}

/* === sceNpAuth_61BB18B3 @ 0x08f0 === */
int sceNpAuth_61BB18B3(int reqId)
{
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	if (reqId == 0) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	int intr = sceKernelCpuSuspendIntr();
	int state = req->state;
	sceKernelCpuResumeIntr(intr);
	return state;
}

/* === npAuthGetStackSize @ 0x0960 === */
static __attribute__((noinline)) uint32_t npAuthGetStackSize(void)
{
	return g_stackSize;
}

/* === sceNpAuth_4EC1F667 (sceNpAuthTerm) @ 0x096c === */
void sceNpAuth_4EC1F667(void)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return;
	}
	if (g_initialized == 0) {
		return;
	}
	while (g_reqList != NULL) {
		sceNpAuth_D99455DD(g_reqList->reqId);
		npAuthDestroyRequestInternal(g_reqList->reqId);
	}
	npAuthShutdownWorker();
	g_initialized = 0;
}

/* === sceNpAuth_3F1C1F70 (sceNpAuthGetTicket) @ 0x0a84 === */
int sceNpAuth_3F1C1F70(int reqId, void *buf, uint32_t size)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	if ((reqId == 0) | (buf == NULL) || size == 0) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	SceNpAuthRequest *req = npAuthFindRequest(reqId, NULL);
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	return npAuthReadHttpBody(req, buf, size);
}

/* === sceNpAuth_F4531ADC @ 0x0b3c === */
int sceNpAuth_F4531ADC(SceNpAuthMemStat *outStat)
{
	if (_sceKernelCheckThreadStack() < 0xff0) {
		return SCE_NP_ERROR_STACK_TOO_SHORT;
	}
	if (g_initialized == 0) {
		return SCE_NP_AUTH_ERROR_NOT_INITIALIZED;
	}
	return npAuthGetVplStat(outStat);
}

/* === npAuthWorkerThread @ 0x0b98 === */
__attribute__((noinline)) int npAuthWorkerThread(SceSize args, void *argp)
{
	void *rawMsg = NULL;
	(void)args;
	(void)argp;

	while (sceKernelReceiveMbx(g_mbxId, &rawMsg, NULL) >= 0) {
		SceNpAuthMbxMessage *msg = (SceNpAuthMbxMessage *)rawMsg;
		if (msg->reqId == 0) {
			break;
		}
		SceNpAuthRequest *req = msg->req;
		if (req != NULL) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 2u) {
				req->state = 2;
			}
			sceKernelCpuResumeIntr(intr);
		}
		int res = npAuthExecuteRequest(msg->req);
		req = msg->req;
		if (req != NULL) {
			int intr = sceKernelCpuSuspendIntr();
			if (req->state <= 4u) {
				req->state = 4;
			}
			sceKernelCpuResumeIntr(intr);
		}
		if (res == 1) {
			npAuthDestroyRequestInternal(msg->req->reqId);
		}
	}
	return 0;
}

static __inline__ __attribute__((always_inline)) void npAuthFreeRequestNodes(SceNpAuthRequest *req)
{
	while (req->paramsHead != NULL) {
		SceNpAuthParamNode *nextNode = req->paramsHead->next;
		npAuthFree(req->paramsHead);
		req->paramsHead = nextNode;
	}
	npAuthFree(req);
}

/* === npAuthCreateRequestInternal @ 0x0ca0 === */
static __attribute__((noinline)) int npAuthCreateRequestInternal(int *outId, const SceNpTicketVersion *ver, const char *serviceId, int useCid)
{
	uint32_t svcLen = 0;
	if (serviceId != NULL) {
		svcLen = sceNet_lib_D8722983(serviceId);
	}
	if (svcLen - 1u >= SCE_NP_SERVICE_ID_SIZE) {
		return SCE_NP_AUTH_ERROR_INVALID_SERVICE_ID;
	}
	SceNpAuthRequest *req = (SceNpAuthRequest *)npAuthAlloc(sizeof(SceNpAuthRequest));
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_ENOMEM;
	}
	sceKernelMemset(req, 0, sizeof(SceNpAuthRequest));
	sceKernelMemcpy(&req->version, ver, sizeof(SceNpTicketVersion));
	req->useConsoleId = (uint8_t)useCid;
	req->state = 0;
	req->hasEntitlement = 0;

	int res = npAuthSetParam(req, 2, serviceId, svcLen);
	if (res >= 0) {
		int intr = sceKernelCpuSuspendIntr();
		int nextId = (g_lastReqId == 16) ? 1 : (g_lastReqId + 1);
		int foundId = SCE_NP_AUTH_ERROR_REQUEST_LIMIT;
		while (nextId != g_lastReqId) {
			if (npAuthFindRequest(nextId, NULL) == NULL) {
				foundId = nextId;
				g_lastReqId = nextId;
				break;
			}
			nextId = (nextId == 16) ? 1 : (nextId + 1);
		}
		if (foundId >= 0) {
			*outId = foundId;
			req->next = g_reqList;
			req->reqId = foundId;
			g_reqList = req;
			sceKernelCpuResumeIntr(intr);
			return res;
		}
		sceKernelCpuResumeIntr(intr);
		res = foundId;
	}

	npAuthFreeRequestNodes(req);
	return res;
}

/* === npAuthDestroyRequestInternal @ 0x0e74 === */
static __attribute__((noinline)) int npAuthDestroyRequestInternal(int reqId)
{
	SceNpAuthRequest *prev = NULL;
	int intr = sceKernelCpuSuspendIntr();
	SceNpAuthRequest *req = npAuthFindRequest(reqId, &prev);
	if (req == NULL) {
		sceKernelCpuResumeIntr(intr);
		return SCE_NP_AUTH_ERROR_ESRCH;
	}
	int state = sceNpAuth_61BB18B3(reqId);
	if ((uint32_t)(state - 1) < 3u) {
		sceKernelCpuResumeIntr(intr);
		return SCE_NP_AUTH_ERROR_EBUSY;
	}
	if (prev != NULL) {
		prev->next = req->next;
	} else {
		g_reqList = g_reqList->next;
	}
	sceKernelCpuResumeIntr(intr);

	npAuthFreeRequestNodes(req);
	return 0;
}

/* === npAuthFindRequest @ 0x0f64 === */
static __attribute__((noinline)) SceNpAuthRequest *npAuthFindRequest(int reqId, SceNpAuthRequest **outPrev)
{
	SceNpAuthRequest *prev = NULL;
	SceNpAuthRequest *cur = g_reqList;
	while (cur != NULL) {
		if (cur->reqId == reqId) {
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	if (outPrev != NULL) {
		*outPrev = prev;
	}
	return cur;
}

/* === npAuthBuildPostBody @ 0x0fa0 === */
static __attribute__((noinline)) int npAuthBuildPostBody(SceNpAuthRequest *req, char *buf, uint32_t bufSize)
{
	char prefix[32];
	char numStr[16];
	uint32_t encLen;
	int res = 0;

	if (req == NULL) {
		sceKernelMemset(prefix, 0, sizeof(prefix));
		return SCE_NP_AUTH_ERROR_EINVAL;
	}

	if ((req->activeParamMask & 1u) == 0) {
		sceNet_lib_CF705E46(numStr, "%u", req->hasEntitlement);
		uint32_t len = sceNet_lib_D8722983(numStr);
		res = npAuthSetParam(req, 0, numStr, len);
	}

	if (res == 0) {
		if (req->useConsoleId != 0) {
			res = sceNpCore_243690EE(buf, bufSize);
			if (res >= 0) {
				res = npAuthSetParam(req, 1, buf, (uint32_t)res);
				if (res >= 0 || res == SCE_NP_AUTH_ERROR_ALREADY_SET) {
					req->useConsoleId = 0;
					res = 0;
				}
			}
		} else {
			res = npAuthClearParam(req, 1);
			if (res >= 0 || res == SCE_NP_AUTH_ERROR_ESRCH) {
				res = 0;
			}
		}
	}

	if (res == 0 && (req->activeParamMask & 0x100u) == 0) {
		res = sceNpCore_E7AED5A3(buf, bufSize);
		if (res >= 0) {
			res = npAuthSetParam(req, 8, buf, 0x40);
		}
	}

	if (res >= 0) {
		uint32_t total = 0;
		SceNpAuthParamNode *node;
		for (node = req->paramsHead; node != NULL; node = node->next) {
			res = sceNpCore_8AFAB4A0(node, prefix, sizeof(prefix));
			if (res < 0) {
				break;
			}
			encLen = (uint32_t)res;
			if (encLen >= bufSize) {
				res = SCE_NP_AUTH_ERROR_ENOMEM;
				break;
			}
			sceNet_lib_B5CE388A(buf, prefix, encLen);
			buf += encLen;
			bufSize -= encLen;
			total += encLen;

			res = sceParseUri_49E950EC(buf, &encLen, bufSize, node->data);
			uint32_t added = encLen - 1u;
			total += added;
			buf += added;
			bufSize -= added;
			if (res < 0) {
				break;
			}
		}
		if (node == NULL) {
			sceKernelMemset(prefix, 0, sizeof(prefix));
			return (int)total;
		}
	}

	sceKernelMemset(prefix, 0, sizeof(prefix));
	return res;
}

/* === npAuthSetCredentials @ 0x11dc === */
static __attribute__((noinline)) int npAuthSetCredentials(SceNpAuthRequest *req, const char *user, const char *pass)
{
	uint32_t ulen = (user != NULL) ? sceNet_lib_D8722983(user) : 0;
	uint32_t plen = (pass != NULL) ? sceNet_lib_D8722983(pass) : 0;
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	if ((ulen == 0) | (plen == 0)) {
		return SCE_NP_AUTH_ERROR_INVALID_CREDENTIAL;
	}
	int res = npAuthSetParam(req, 3, user, ulen);
	if (res >= 0) {
		res = npAuthSetParam(req, 4, pass, plen);
	}
	return res;
}

/* === npAuthSetEntitlementConsume @ 0x12ac === */
static __attribute__((noinline)) int npAuthSetEntitlementConsume(SceNpAuthRequest *req, const char *entId, uint32_t consumed)
{
	char numStr[16];
	uint32_t len = (entId != NULL) ? sceNet_lib_D8722983(entId) : 0;
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	if (len - 1u >= SCE_NP_ENTITLEMENT_ID_SIZE) {
		return SCE_NP_AUTH_ERROR_INVALID_ENTITLEMENT_ID;
	}
	req->hasEntitlement = 1;
	int res = npAuthSetParam(req, 5, entId, len);
	if (res >= 0) {
		sceNet_lib_CF705E46(numStr, "%u", consumed);
		uint32_t nlen = sceNet_lib_D8722983(numStr);
		res = npAuthSetParam(req, 6, numStr, nlen);
	}
	return res;
}

/* === npAuthSetCookie @ 0x136c === */
static __attribute__((noinline)) int npAuthSetCookie(SceNpAuthRequest *req, const char *cookie, uint32_t cookieLen)
{
	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	if (((cookie == NULL) | (cookieLen == 0)) || cookieLen > SCE_NP_COOKIE_MAX_SIZE) {
		return SCE_NP_AUTH_ERROR_INVALID_DATA_LENGTH;
	}
	return npAuthSetParam(req, 7, cookie, cookieLen);
}

/* === npAuthSetParam @ 0x13d0 === */
static __attribute__((noinline)) int npAuthSetParam(SceNpAuthRequest *req, uint32_t keyId, const void *data, uint32_t len)
{
	char prefix[32];
	int res;

	if (((req == NULL) | (data == NULL)) || keyId >= 9u) {
		res = SCE_NP_AUTH_ERROR_EINVAL;
	} else if ((req->activeParamMask & (1u << keyId)) != 0) {
		res = SCE_NP_AUTH_ERROR_ALREADY_SET;
	} else {
		SceNpAuthParamNode *node = (SceNpAuthParamNode *)npAuthAlloc(len + 13u);
		if (node == NULL) {
			res = SCE_NP_AUTH_ERROR_ENOMEM;
		} else {
			node->next = NULL;
			node->id = keyId;
			node->len = len;
			sceKernelMemcpy(node->data, data, len);
			node->data[len] = '\0';

			res = sceNpCore_8AFAB4A0(node, prefix, sizeof(prefix));
			if (res >= 0) {
				if (prefix[0] == '&') {
					if (req->paramsHead != NULL) {
						req->paramsTail->next = node;
					} else {
						req->paramsHead = node;
					}
					req->paramsTail = node;
				} else {
					if (req->paramsTail == NULL) {
						req->paramsTail = node;
					}
					node->next = req->paramsHead;
					req->paramsHead = node;
				}
				req->activeParamMask |= (1u << keyId);
				res = 0;
			}
		}
	}

	sceKernelMemset(prefix, 0, sizeof(prefix));
	return res;
}

/* === npAuthClearParam @ 0x1528 === */
static __attribute__((noinline)) int npAuthClearParam(SceNpAuthRequest *req, uint32_t keyId)
{
	if (req == NULL || keyId >= 9u) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	SceNpAuthParamNode *prev = NULL;
	SceNpAuthParamNode *cur = req->paramsHead;
	uint32_t mask = ~(1u << keyId);
	while (cur != NULL) {
		if (cur->id == keyId) {
			if (prev == NULL) {
				req->paramsHead = cur->next;
			} else {
				prev->next = cur->next;
			}
			if (cur->next == NULL) {
				req->paramsTail = prev;
			}
			npAuthFree(cur);
			req->activeParamMask &= mask;
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}
	return SCE_NP_AUTH_ERROR_ESRCH;
}

/* === npAuthExecuteRequest @ 0x15ec === */
static __attribute__((noinline)) int npAuthExecuteRequest(SceNpAuthRequest *req)
{
	int tmplId = 0;
	int connId = 0;
	int cbRes = 0;
	char *workBuf = NULL;

	if (req == NULL) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}

	int verOk = 0;
	for (int i = 0; g_supportedVers[i].major != 0; i++) {
		if (g_supportedVers[i].major == req->version.major && g_supportedVers[i].minor == req->version.minor) {
			verOk = 1;
			break;
		}
	}

	int res = SCE_NP_AUTH_ERROR_UNSUPPORTED_TICKET_VER;
	if (verOk) {
		workBuf = (char *)npAuthAlloc(0x832);
		if (workBuf == NULL) {
			res = SCE_NP_AUTH_ERROR_ENOMEM;
		} else {
			char *bodyBuf = workBuf + 0x32;
			sceKernelMemset(workBuf, 0, 0x832);
			res = npAuthOpenHttpConnection(&tmplId, &connId, bodyBuf, 0x800, workBuf, 0x32);
			if (res >= 0) {
				if (req->preCb != NULL) {
					register uint32_t gpReg asm("gp");
					uint32_t savedGp = gpReg;
					gpReg = req->preGp;
					int stk = _sceKernelCheckThreadStack();
					int maxStk = (int)npAuthGetStackSize();
					if (stk - maxStk > 0) {
						uint32_t padSize = ((uint32_t)(stk - (int)npAuthGetStackSize()) + 15u) & ~15u;
						void * volatile stackGuard = __builtin_alloca(padSize);
						(void)stackGuard;
						req->preCb(req->reqId, connId, req->cbArg);
					} else {
						req->preCb(req->reqId, connId, req->cbArg);
					}
					gpReg = savedGp;
				}

				res = npAuthBuildPostBody(req, bodyBuf, 0x800);
				if (res >= 0) {
					int postLen = res;
					if (sceNpAuth_61BB18B3(req->reqId) == 3) {
						res = SCE_NP_AUTH_ERROR_ABORTED;
					} else {
						res = sceHttpCreateRequestWithURL(connId, 1, workBuf, (uint64_t)(uint32_t)postLen);
						if (res >= 0) {
							req->httpReqId = res;
							res = npAuthSendHttpPost(req, bodyBuf, (uint32_t)postLen);
							if (res == 0) {
								res = npAuthParseHttpResponse(req);
							}
							if (sceNpAuth_61BB18B3(req->reqId) == 3) {
								res = SCE_NP_AUTH_ERROR_ABORTED;
							}
						}
					}
				}
			}
		}
	}

	if (req->doneCb != NULL) {
		int stk = _sceKernelCheckThreadStack();
		register uint32_t gpReg asm("gp");
		uint32_t savedGp = gpReg;
		gpReg = req->doneGp;
		int maxStk = (int)npAuthGetStackSize();
		if (stk - maxStk > 0) {
			uint32_t padSize = ((uint32_t)(stk - (int)npAuthGetStackSize()) + 15u) & ~15u;
			void * volatile stackGuard = __builtin_alloca(padSize);
			(void)stackGuard;
			cbRes = req->doneCb(req->reqId, res, req->cbArg);
		} else {
			cbRes = req->doneCb(req->reqId, res, req->cbArg);
		}
		gpReg = savedGp;
	}

	if (req->httpReqId > 0) {
		sceHttpDeleteRequest(req->httpReqId);
		req->httpReqId = 0;
	}
	if (tmplId > 0) {
		sceHttpDeleteTemplate(tmplId);
	}
	if (connId > 0) {
		sceHttpDeleteConnection(connId);
	}
	if (workBuf != NULL) {
		npAuthFree(workBuf);
	}
	return cbRes;
}

/* === npAuthSendHttpPost @ 0x191c === */
static __attribute__((noinline)) int npAuthSendHttpPost(SceNpAuthRequest *req, const void *postData, uint32_t postLen)
{
	char hdrName[32];
	char hdrVal[48];
	char verStr[16];

	sceNet_lib_CF705E46(verStr, "%d.%d", req->version.major, req->version.minor);
	int idx = 0;
	int done = 0;
	do {
		int res = sceNpCore_5145344F(idx, hdrName, sizeof(hdrName), hdrVal, sizeof(hdrVal));
		if (res > 0) {
			done = 1;
		}
		if (res < 0) {
			return res;
		}
		if (idx == 0) {
			sceNet_lib_B5CE388A(hdrVal, verStr, sizeof(hdrVal));
		}
		res = sceHttpAddExtraHeader(req->httpReqId, hdrName, hdrVal, 0);
		idx++;
		if (res < 0) {
			return res;
		}
	} while (!done);

	int res = sceNpCore_9218ACF6(hdrName, sizeof(hdrName), g_npHeaderVal, 4);
	if (res < 0) {
		return res;
	}
	res = sceNp_B4063F7A(g_npHeaderVal + 4);
	if (res < 0) {
		return res;
	}
	res = sceHttpAddExtraHeader(req->httpReqId, hdrName, g_npHeaderVal, 0);
	if (res < 0) {
		return res;
	}
	res = sceHttpSendRequest(req->httpReqId, postData, postLen);
	if (res < 0) {
		return npAuthTranslateHttpError(req->httpReqId, res);
	}
	sceKernelMemset(hdrName, 0, sizeof(hdrName));
	sceKernelMemset(hdrVal, 0, sizeof(hdrVal));
	sceKernelMemset(g_npHeaderVal, 0x20, 4);
	sceKernelMemset(g_npHeaderVal + 4, 0x20, 4);
	return 0;
}

/* === npAuthParseHttpResponse @ 0x1ad4 === */
static __attribute__((noinline)) int npAuthParseHttpResponse(SceNpAuthRequest *req)
{
	char hdrName[32];
	int statusCode;
	char *allHdr;
	uint32_t allHdrLen;
	const char *valPtr;
	uint32_t valLen;
	uint64_t contentLen;

	int res = sceHttpGetStatusCode(req->httpReqId, &statusCode);
	if (res < 0) {
		return res;
	}
	allHdrLen = 0;
	res = sceHttpGetAllHeader(req->httpReqId, &allHdr, &allHdrLen);
	if (res < 0) {
		return res;
	}

	int idx = 0;
	int done = 0;
	do {
		res = sceNpCore_04096629(idx, hdrName, sizeof(hdrName));
		if (res > 0) {
			done = 1;
		}
		if (res < 0) {
			return res;
		}
		res = sceParseHttp_AD7BFDEF(allHdr, allHdrLen, hdrName, &valPtr, &valLen);
		if (res < 0) {
			return res;
		}
		res = sceNpCore_515B65E8(idx, valPtr, valLen);
		idx++;
		if (res < 0) {
			return res;
		}
	} while (!done);

	res = sceHttpGetContentLength(req->httpReqId, &contentLen);
	if (res < 0) {
		return res;
	}
	if (contentLen == 0 || contentLen > SCE_NP_TICKET_MAX_SIZE) {
		return SCE_NP_AUTH_ERROR_INVALID_DATA_LENGTH;
	}
	return (int)contentLen;
}

/* === npAuthReadHttpBody @ 0x1bf8 === */
static __attribute__((noinline)) int npAuthReadHttpBody(SceNpAuthRequest *req, void *buf, uint32_t size)
{
	if (req == NULL || req->httpReqId == 0 || buf == NULL || size == 0) {
		return SCE_NP_AUTH_ERROR_EINVAL;
	}
	uint32_t total = 0;
	uint8_t *dst = (uint8_t *)buf;
	int n;
	do {
		n = sceHttpReadData(req->httpReqId, dst, size);
		if (n < 0) {
			return npAuthTranslateHttpError(req->httpReqId, n);
		}
		size -= (uint32_t)n;
		dst += n;
		total += (uint32_t)n;
	} while (n >= 0 && size != 0);
	if (size != 0) {
		return SCE_NP_AUTH_ERROR_SHORT_READ;
	}
	return (int)total;
}

/* === npAuthOpenHttpConnection @ 0x1cc8 === */
static __attribute__((noinline)) int npAuthOpenHttpConnection(int *outTmpl, int *outConn, char *agentBuf, uint32_t agentSize, char *urlBuf, uint32_t urlSize)
{
	int res = sceNpCore_0366DAB6(agentBuf, agentSize);
	if (res < 0) {
		return res;
	}
	res = sceHttpCreateTemplate(agentBuf, 0, 0);
	if (res < 0) {
		return res;
	}
	*outTmpl = res;

	res = sceHttpSetConnectTimeOut(*outTmpl, 20000000);
	if (res < 0) {
		return res;
	}
	res = sceHttpSetSendTimeOut(*outTmpl, 20000000);
	if (res < 0) {
		return res;
	}
	res = sceHttpSetRecvTimeOut(*outTmpl, 20000000);
	if (res < 0) {
		return res;
	}

	if (sceNp_E24DA399() > 0) {
		res = sceHttp_87F1E666(*outTmpl, 0x40);
		if (res < 0) {
			return res;
		}
		res = sceHttp_3C478044(*outTmpl, 1);
	} else {
		res = sceHttp_87F1E666(*outTmpl, 0xe5);
		if (res < 0) {
			return res;
		}
		res = sceHttp_3C478044(*outTmpl, 0x1a);
	}
	if (res < 0) {
		return res;
	}

	res = sceNpCore_B13D27CA(urlBuf, urlSize);
	if (res < 0) {
		return res;
	}
	res = sceHttpCreateConnectionWithURL(*outTmpl, urlBuf, 1);
	if (res < 0) {
		return res;
	}
	*outConn = res;
	return 0;
}

/* === npAuthTranslateHttpError @ 0x1e0c === */
static __attribute__((noinline)) int npAuthTranslateHttpError(int httpReqId, int err)
{
	int sslErr;
	int certErr;
	if (err != SCE_HTTP_ERROR_SSL) {
		return err;
	}
	int res = sceHttp_AB1540D5(httpReqId, &sslErr, &certErr);
	if (res < 0) {
		return res;
	}
	if (sslErr != SCE_HTTPS_ERROR_CERT) {
		return sslErr;
	}
	if (certErr & 4) {
		return SCE_NP_AUTH_ERROR_SSL_CERT_EXPIRED;
	}
	if (certErr & 0x20) {
		return SCE_NP_AUTH_ERROR_SSL_CERT_REVOKED;
	}
	if (certErr & 2) {
		return SCE_NP_AUTH_ERROR_SSL_CERT_COMMON_NAME;
	}
	if (certErr & 1) {
		return SCE_NP_AUTH_ERROR_SSL_CERT_UNTRUSTED;
	}
	return SCE_NP_AUTH_ERROR_SSL_CERT_OTHER;
}

/* === sceNpAuth_5A3CB57A (sceNpAuthGetTicketParam) @ 0x1eb4 === */
int sceNpAuth_5A3CB57A(const void *buf, uint32_t len, int ticketParamId, void *outTicketParam)
{
	SceNpAuthTicketBuf tbuf;
	sceKernelMemset(outTicketParam, 0, SCE_NP_TICKET_PARAM_DATA_LEN);
	if (buf == NULL || outTicketParam == NULL) {
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	}
	if (len - 4u > 0xfffcu) {
		return SCE_NP_MANAGER_ERROR_INVALID_TICKET_SIZE;
	}
	if ((uint32_t)ticketParamId >= (uint32_t)SCE_NP_TICKET_PARAM_MAX) {
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(tbuf.verBytes, buf, 4);
	tbuf.verBytes[0] >>= 4;

	g_curParamHandler = g_paramHandlers;
	while (g_curParamHandler->major != 0) {
		if (g_curParamHandler->major == tbuf.verBytes[0] && g_curParamHandler->minor == tbuf.verBytes[1]) {
			break;
		}
		g_curParamHandler++;
	}
	if (g_curParamHandler->major == 0) {
		return SCE_NP_AUTH_ERROR_SIGNED_OUT;
	}
	return ((SceNpAuthTicketParamFn)g_curParamHandler->handler)(&tbuf, ticketParamId, outTicketParam);
}

/* === sceNpAuth_75FB0AE3 (sceNpAuthGetEntitlementIdList) @ 0x2000 === */
int sceNpAuth_75FB0AE3(const void *buf, uint32_t len, uint8_t *outBuf, uint32_t maxCount)
{
	SceNpAuthTicketBuf tbuf;
	if (buf == NULL) {
		return SCE_NP_MANAGER_ERROR_INVALID_ARGUMENT;
	}
	if (len - 4u > 0xfffcu) {
		return SCE_NP_MANAGER_ERROR_INVALID_TICKET_SIZE;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(tbuf.verBytes, buf, 4);
	tbuf.verBytes[0] >>= 4;

	g_curEntListHandler = g_entListHandlers;
	while (g_curEntListHandler->major != 0) {
		if (g_curEntListHandler->major == tbuf.verBytes[0] && g_curEntListHandler->minor == tbuf.verBytes[1]) {
			break;
		}
		g_curEntListHandler++;
	}
	if (g_curEntListHandler->major == 0) {
		return SCE_NP_AUTH_ERROR_SIGNED_OUT;
	}
	return ((SceNpAuthEntListFn)g_curEntListHandler->handler)(&tbuf, outBuf, maxCount);
}

/* === sceNpAuth_6900F084 (sceNpAuthGetEntitlementById) @ 0x2114 === */
int sceNpAuth_6900F084(const void *buf, uint32_t len, const char *entId, SceNpEntitlement *outEnt)
{
	SceNpEntitlement entBuf;
	int res = npAuthGetEntitlementById(buf, len, entId, &entBuf);
	if (outEnt != NULL) {
		sceKernelMemcpy(outEnt, &entBuf, sizeof(SceNpEntitlement));
	}
	return res;
}

/* === npAuthGetTicketParamV2 @ 0x2164 === */
static __attribute__((noinline)) int npAuthGetTicketParamV2(SceNpAuthTicketBuf *tbuf, int ticketParamId, void *outBuf)
{
	if (ticketParamId >= 11) {
		return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	}
	int res = npAuthSeekTicketTag(tbuf, 0x3000, 1);
	if (res != 0) {
		return res;
	}
	for (int i = 0; i <= ticketParamId; i++) {
		int tag = npAuthReadTicketField(tbuf, outBuf);
		if (tag != g_ticketV2Schema[i].type) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
	}
	return 0;
}

/* === npAuthGetTicketParamV3 @ 0x2220 === */
static __attribute__((noinline)) int npAuthGetTicketParamV3(SceNpAuthTicketBuf *tbuf, int ticketParamId, void *outBuf)
{
	if (ticketParamId >= SCE_NP_TICKET_PARAM_MAX) {
		return SCE_NP_AUTH_ERROR_TICKET_PARAM_NOT_FOUND;
	}
	int res = npAuthSeekTicketTag(tbuf, 0x3000, 1);
	if (res != 0) {
		return res;
	}
	for (int i = 0; i < SCE_NP_TICKET_PARAM_MAX; i++) {
		int tag = npAuthReadTicketField(tbuf, outBuf);
		if (tag != g_ticketV3Schema[i].type) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		if ((int)g_ticketV3Schema[i].id == ticketParamId) {
			break;
		}
	}
	return 0;
}

/* === npAuthListEntitlementsV2 @ 0x22dc === */
static __attribute__((noinline)) int npAuthListEntitlementsV2(SceNpAuthTicketBuf *tbuf, uint8_t *outBuf, uint32_t maxCount)
{
	uint8_t fieldBuf[SCE_NP_TICKET_PARAM_DATA_LEN];
	uint32_t count = 0;
	while (npAuthSeekTicketTag(tbuf, 0x3001, 1) == 0) {
		if (npAuthReadTicketField(tbuf, fieldBuf) != 8) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		if (outBuf != NULL && count < maxCount) {
			sceKernelMemcpy(outBuf, fieldBuf, SCE_NP_ENTITLEMENT_ID_SIZE);
		}
		count++;
		outBuf += SCE_NP_ENTITLEMENT_ID_SIZE;
	}
	return (int)count;
}

/* === npAuthListEntitlementsV3 @ 0x23a4 === */
static __attribute__((noinline)) int npAuthListEntitlementsV3(SceNpAuthTicketBuf *tbuf, uint8_t *outBuf, uint32_t maxCount)
{
	SceNpAuthV3EntStream entStream;
	SceNpAuthV3EntHeader entHdr;
	char prefix[SCE_NP_TICKET_PARAM_DATA_LEN];
	uint8_t flagsByte;

	tbuf->offset = 4;
	int res = npAuthGetTicketParamV3(tbuf, 8, prefix);
	if (res < 0) {
		return res;
	}
	uint32_t prefixLen = sceNet_lib_D8722983(prefix);
	tbuf->offset = 4;
	if (npAuthSeekTicketTag(tbuf, 0x3010, 0) != 0) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	if (tbuf->offset + 4u >= tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemcpy(&entStream, tbuf->buf + tbuf->offset, 4);
	entStream.tag = (uint16_t)((entStream.tag >> 8) | (entStream.tag << 8));
	entStream.len = (uint16_t)((entStream.len >> 8) | (entStream.len << 8));
	if (tbuf->offset + entStream.len + 4u > tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	entStream.ptr = tbuf->buf + tbuf->offset + 4;
	entStream.pos = 0;

	uint32_t count = 0;
	while (entStream.pos < entStream.len) {
		if (entStream.pos + 1u > entStream.len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		sceKernelMemcpy(&flagsByte, entStream.ptr + entStream.pos, 1);
		entStream.pos++;
		entHdr.flags = (uint8_t)(((flagsByte >> 6) & 3u) | (((flagsByte >> 5) & 1u) << 2));
		uint8_t idLen = flagsByte & 0x1fu;
		entHdr.idLen = (idLen != 0) ? idLen : SCE_NP_ENTITLEMENT_ID_SIZE;

		char *dstId = (outBuf != NULL && count < maxCount) ? (char *)(outBuf + (count << 5)) : NULL;
		res = npAuthReadEntitlementIdV3(&entStream, prefix, prefixLen, &entHdr, dstId);
		if (res < 0) {
			return res;
		}
		uint32_t skip = (entHdr.flags & 4u) ? 16u : 8u;
		if ((entHdr.flags & 3u) == 1u) {
			skip += 8u;
		}
		if (entStream.pos + skip > entStream.len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		entStream.pos += skip;
		count++;
	}
	return (int)count;
}

/* === npAuthFindEntitlementV2 @ 0x25fc === */
static __attribute__((noinline)) int npAuthFindEntitlementV2(SceNpAuthTicketBuf *tbuf, const char *entId, SceNpEntitlement *outEnt)
{
	uint8_t fieldBuf[SCE_NP_TICKET_PARAM_DATA_LEN];
	while (npAuthSeekTicketTag(tbuf, 0x3001, 1) == 0) {
		if (npAuthReadTicketField(tbuf, fieldBuf) != 8) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		uint32_t len = sceNet_lib_D8722983(entId) + 1u;
		if (sceNet_lib_E0A81C7C(entId, (const char *)fieldBuf, len) == 0) {
			sceKernelMemset(outEnt, 0, sizeof(SceNpEntitlement));
			sceKernelMemcpy(outEnt->id, fieldBuf, SCE_NP_ENTITLEMENT_ID_SIZE);
			if (npAuthReadTicketField(tbuf, fieldBuf) != 7) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(&outEnt->createdDate, fieldBuf, sizeof(SceNpTime));
			if (npAuthReadTicketField(tbuf, fieldBuf) != 7) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(&outEnt->expireDate, fieldBuf, sizeof(SceNpTime));
			if (npAuthReadTicketField(tbuf, fieldBuf) != 1) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(&outEnt->type, fieldBuf, sizeof(uint32_t));
			if (outEnt->type != 1u) {
				return 0;
			}
			if (npAuthReadTicketField(tbuf, fieldBuf) != 1) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(&outEnt->remainingCount, fieldBuf, sizeof(int32_t));
			if (npAuthReadTicketField(tbuf, fieldBuf) != 1) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(&outEnt->consumedCount, fieldBuf, sizeof(uint32_t));
			return 0;
		}
	}
	return SCE_NP_AUTH_ERROR_ESRCH;
}

/* === npAuthFindEntitlementV3 @ 0x2790 === */
static __attribute__((noinline)) int npAuthFindEntitlementV3(SceNpAuthTicketBuf *tbuf, const char *entId, SceNpEntitlement *outEnt)
{
	SceNpAuthV3EntStream entStream;
	SceNpAuthV3EntHeader entHdr;
	char prefix[SCE_NP_TICKET_PARAM_DATA_LEN];
	char curId[SCE_NP_ENTITLEMENT_ID_SIZE];
	uint8_t flagsByte;

	uint32_t eidLen = sceNet_lib_D8722983(entId) + 1u;
	if (eidLen > SCE_NP_ENTITLEMENT_ID_SIZE) {
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	}
	tbuf->offset = 4;
	int res = npAuthGetTicketParamV3(tbuf, 8, prefix);
	if (res < 0) {
		return res;
	}
	uint32_t prefixLen = sceNet_lib_D8722983(prefix);
	tbuf->offset = 4;
	if (npAuthSeekTicketTag(tbuf, 0x3010, 0) != 0) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	if (npAuthSeekTicketTag(tbuf, 0x3010, 0) != 0) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	if (tbuf->offset + 4u >= tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemcpy(&entStream, tbuf->buf + tbuf->offset, 4);
	entStream.tag = (uint16_t)((entStream.tag >> 8) | (entStream.tag << 8));
	entStream.len = (uint16_t)((entStream.len >> 8) | (entStream.len << 8));
	if (tbuf->offset + entStream.len + 4u > tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	entStream.ptr = tbuf->buf + tbuf->offset + 4;
	entStream.pos = 0;

	while (entStream.pos < entStream.len) {
		if (entStream.pos + 1u > entStream.len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		sceKernelMemcpy(&flagsByte, entStream.ptr + entStream.pos, 1);
		entStream.pos++;
		entHdr.flags = (uint8_t)(((flagsByte >> 6) & 3u) | (((flagsByte >> 5) & 1u) << 2));
		uint8_t idLen = flagsByte & 0x1fu;
		entHdr.idLen = (idLen != 0) ? idLen : SCE_NP_ENTITLEMENT_ID_SIZE;

		res = npAuthReadEntitlementIdV3(&entStream, prefix, prefixLen, &entHdr, curId);
		if (res < 0) {
			return res;
		}
		if (sceNet_lib_E0A81C7C(entId, curId, eidLen) == 0) {
			sceKernelMemset(outEnt, 0, sizeof(SceNpEntitlement));
			res = npAuthReadEntitlementBodyV3(&entStream, &entHdr, outEnt);
			if (res < 0) {
				return res;
			}
			sceKernelMemcpy(outEnt->id, entId, eidLen);
			return 0;
		}
		uint32_t skip = (entHdr.flags & 4u) ? 16u : 8u;
		if ((entHdr.flags & 3u) == 1u) {
			skip += 8u;
		}
		if (entStream.pos + skip > entStream.len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		entStream.pos += skip;
	}
	return SCE_NP_AUTH_ERROR_ESRCH;
}

/* === npAuthGetEntitlementById @ 0x2a68 === */
static __attribute__((noinline)) int npAuthGetEntitlementById(const void *buf, uint32_t len, const char *entId, SceNpEntitlement *outEnt)
{
	SceNpAuthTicketBuf tbuf;
	if (buf == NULL || entId == NULL || outEnt == NULL) {
		return SCE_NP_ERROR_INVALID_ARGUMENT;
	}
	if (len - 4u > 0xfffcu) {
		return SCE_NP_MANAGER_ERROR_INVALID_TICKET_SIZE;
	}
	tbuf.buf = (const uint8_t *)buf;
	tbuf.size = len;
	tbuf.offset = 4;
	sceKernelMemcpy(tbuf.verBytes, buf, 4);
	tbuf.verBytes[0] >>= 4;

	g_curEntIdHandler = g_entByIdHandlers;
	while (g_curEntIdHandler->major != 0) {
		if (g_curEntIdHandler->major == tbuf.verBytes[0] && g_curEntIdHandler->minor == tbuf.verBytes[1]) {
			break;
		}
		g_curEntIdHandler++;
	}
	if (g_curEntIdHandler->major == 0) {
		return SCE_NP_AUTH_ERROR_SIGNED_OUT;
	}
	return ((SceNpAuthEntFindFn)g_curEntIdHandler->handler)(&tbuf, entId, outEnt);
}

/* === npAuthSeekTicketTag @ 0x2b94 === */
static __attribute__((noinline)) int npAuthSeekTicketTag(SceNpAuthTicketBuf *tbuf, uint16_t targetTag, int skipRoot)
{
	uint16_t hdr[2];
	uint32_t off = tbuf->offset;
	if (off + 4u >= tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	while (1) {
		sceKernelMemcpy(hdr, tbuf->buf + off, 4);
		uint16_t tag = (uint16_t)((hdr[0] >> 8) | (hdr[0] << 8));
		uint16_t len = (uint16_t)((hdr[1] >> 8) | (hdr[1] << 8));
		hdr[0] = tag;
		hdr[1] = len;
		if (tag == targetTag && (skipRoot & 1) == 0) {
			tbuf->offset = off;
			return 0;
		}
		off += 4u;
		if ((tag & 0xf000u) == 0) {
			if (tag != 0) {
				if (off + len > tbuf->size) {
					return SCE_NP_UTIL_ERROR_PARSER_FAILED;
				}
				off += len;
			}
		} else if ((tag & 0xf010u) == 0x3010u) {
			if (off + len > tbuf->size) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			off += len;
		}
		if (hdr[0] == targetTag) {
			tbuf->offset = off;
			return 0;
		}
		if (off + 4u >= tbuf->size) {
			break;
		}
	}
	return SCE_NP_UTIL_ERROR_PARSER_FAILED;
}

/* === npAuthReadTicketField @ 0x2cb4 === */
static __attribute__((noinline)) int npAuthReadTicketField(SceNpAuthTicketBuf *tbuf, void *outBuf)
{
	uint16_t hdr[2];
	uint32_t off = tbuf->offset;
	if (off + 4u >= tbuf->size) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemcpy(hdr, tbuf->buf + off, 4);
	uint16_t tag = (uint16_t)((hdr[0] >> 8) | (hdr[0] << 8));
	uint16_t len = (uint16_t)((hdr[1] >> 8) | (hdr[1] << 8));
	hdr[0] = tag;
	hdr[1] = len;
	if (off + len + 4u > tbuf->size || tag == 0 || (tag & 0xf010u) == 0x3000u) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemset(outBuf, 0, SCE_NP_TICKET_PARAM_DATA_LEN);
	uint32_t dataOff = off + 4u;
	uint8_t *outBytes = (uint8_t *)outBuf;

	if (tag == 1u) {
		uint32_t rawWord;
		sceKernelMemcpy(&rawWord, tbuf->buf + dataOff, 4);
		*(uint32_t *)outBuf = __builtin_bswap32(rawWord);
	} else if (tag == 2u || tag == 7u) {
		uint8_t rawBytes[8];
		sceKernelMemcpy(rawBytes, tbuf->buf + dataOff, 8);
		uint64_t val = 0;
		for (int i = 0; i < 8; i++) {
			val = (val << 8) | rawBytes[i];
		}
		*(uint64_t *)outBuf = val;
	} else if (tag == 4u || tag == 8u) {
		if (len < SCE_NP_TICKET_PARAM_DATA_LEN) {
			sceKernelMemcpy(outBuf, tbuf->buf + dataOff, len);
			outBytes[len] = 0;
		} else {
			sceKernelMemcpy(outBuf, tbuf->buf + dataOff, SCE_NP_TICKET_PARAM_DATA_LEN - 1u);
			outBytes[SCE_NP_TICKET_PARAM_DATA_LEN - 1u] = 0;
		}
	} else if (tag == 0x3011u) {
		uint32_t rawDate;
		SceNpDate *outDate = (SceNpDate *)outBuf;
		sceKernelMemcpy(&rawDate, tbuf->buf + dataOff, 4);
		uint32_t swapped = __builtin_bswap32(rawDate);
		outDate->year = (uint16_t)(swapped >> 16);
		outDate->month = (uint8_t)(swapped >> 8);
		outDate->day = (uint8_t)swapped;
	}
	tbuf->offset = dataOff + hdr[1];
	return hdr[0];
}

/* === npAuthReadEntitlementIdV3 @ 0x2f1c === */
static __attribute__((noinline)) int npAuthReadEntitlementIdV3(SceNpAuthV3EntStream *stream, const char *prefix, uint32_t prefixLen, const SceNpAuthV3EntHeader *hdr, char *outId)
{
	uint8_t idBuf[SCE_NP_ENTITLEMENT_ID_SIZE];
	uint8_t idLen = hdr->idLen;

	if (stream->pos + idLen > stream->len) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemcpy(idBuf, stream->ptr + stream->pos, idLen);
	stream->pos += idLen;

	if (outId != NULL) {
		if (hdr->idLen < 9u) {
			if (prefixLen + hdr->idLen + 2u > SCE_NP_ENTITLEMENT_ID_SIZE) {
				return SCE_NP_UTIL_ERROR_PARSER_FAILED;
			}
			sceKernelMemcpy(outId, prefix, prefixLen);
			outId[prefixLen] = '-';
			sceKernelMemcpy(outId + prefixLen + 1u, idBuf, hdr->idLen);
			outId[prefixLen + 1u + hdr->idLen] = '\0';
		} else {
			sceKernelMemset(outId, 0, SCE_NP_ENTITLEMENT_ID_SIZE);
			sceKernelMemcpy(outId, idBuf, hdr->idLen);
		}
	}
	return 0;
}

/* === npAuthReadEntitlementBodyV3 @ 0x3050 === */
static __attribute__((noinline)) int npAuthReadEntitlementBodyV3(SceNpAuthV3EntStream *stream, const SceNpAuthV3EntHeader *hdr, SceNpEntitlement *outEnt)
{
	uint8_t rawBytes[8];

	outEnt->type = hdr->flags & 3u;
	if (stream->pos + 8u > stream->len) {
		return SCE_NP_UTIL_ERROR_PARSER_FAILED;
	}
	sceKernelMemcpy(rawBytes, stream->ptr + stream->pos, 8);
	stream->pos += 8u;
	uint64_t createdTime = 0;
	for (int i = 0; i < 8; i++) {
		createdTime = (createdTime << 8) | rawBytes[i];
	}
	outEnt->createdDate = createdTime;

	if (hdr->flags & 4u) {
		if (stream->pos + 8u > stream->len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		sceKernelMemcpy(rawBytes, stream->ptr + stream->pos, 8);
		stream->pos += 8u;
		uint64_t expireTime = 0;
		for (int i = 0; i < 8; i++) {
			expireTime = (expireTime << 8) | rawBytes[i];
		}
		outEnt->expireDate = expireTime;
	} else {
		outEnt->expireDate = 0;
	}

	if ((hdr->flags & 3u) == 1u) {
		uint32_t rawCount;
		if (stream->pos + 4u > stream->len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		sceKernelMemcpy(&rawCount, stream->ptr + stream->pos, 4);
		stream->pos += 4u;
		outEnt->remainingCount = (int32_t)__builtin_bswap32(rawCount);

		if (stream->pos + 4u > stream->len) {
			return SCE_NP_UTIL_ERROR_PARSER_FAILED;
		}
		sceKernelMemcpy(&rawCount, stream->ptr + stream->pos, 4);
		stream->pos += 4u;
		outEnt->consumedCount = __builtin_bswap32(rawCount);
	} else {
		outEnt->consumedCount = 0;
		outEnt->remainingCount = 0;
	}
	return 0;
}
