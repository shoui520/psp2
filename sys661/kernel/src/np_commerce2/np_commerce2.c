/*
 * sceNpCommerce2 (np_commerce2.prx) - PS Store Commerce2 Client Library
 * Reconstructed from 6.61 ePSP firmware.
 */

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* Error codes */
#define SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED        ((s32)0x80550F01)
#define SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED    ((s32)0x80550F02)
#define SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT       ((s32)0x80550F03)
#define SCE_NP_COMMERCE2_ERROR_UNSUPPORTED_VERSION    ((s32)0x80550F04)
#define SCE_NP_COMMERCE2_ERROR_CTX_MAX                ((s32)0x80550F05)
#define SCE_NP_COMMERCE2_ERROR_INVALID_INDEX          ((s32)0x80550F06)
#define SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY          ((s32)0x80550F0B)
#define SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND          ((s32)0x80550F0C)
#define SCE_NP_COMMERCE2_ERROR_CTX_ID_EXHAUSTED       ((s32)0x80550F0D)
#define SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND          ((s32)0x80550F0E)
#define SCE_NP_COMMERCE2_ERROR_REQ_ID_EXHAUSTED       ((s32)0x80550F0F)
#define SCE_NP_COMMERCE2_ERROR_ABORTED                ((s32)0x80550F10)
#define SCE_NP_COMMERCE2_ERROR_FORMAT                 ((s32)0x80550F11)
#define SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL ((s32)0x80550F12)
#define SCE_NP_COMMERCE2_ERROR_UNEXPECTED_EOF         ((s32)0x80550F13)
#define SCE_NP_COMMERCE2_ERROR_INVALID_RESPONSE       ((s32)0x80550F14)
#define SCE_NP_COMMERCE2_ERROR_CONSOLE_ID             ((s32)0x80550F15)
#define SCE_NP_COMMERCE2_ERROR_SERVER_MAINTENANCE     ((s32)0x80550F16)
#define SCE_NP_COMMERCE2_ERROR_SERVER_ERROR           ((s32)0x80550F17)
#define SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER    ((s32)0x80550F18)
#define SCE_NP_COMMERCE2_ERROR_REQ_MAX                ((s32)0x80550F19)
#define SCE_NP_COMMERCE2_ERROR_INVALID_ACTION         ((s32)0x80550F81)
#define SCE_NP_COMMERCE2_ERROR_INVALID_VERSION        ((s32)0x80550F83)
#define SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND          ((s32)0x80550F87)
#define SCE_NP_COMMERCE2_ERROR_TLV_SHORT_HEADER       ((s32)0x80550F88)
#define SCE_NP_COMMERCE2_ERROR_TLV_SHORT_MIN          ((s32)0x80550F89)
#define SCE_NP_COMMERCE2_ERROR_TLV_TRUNCATED          ((s32)0x80550F8A)
#define SCE_NP_COMMERCE2_ERROR_ENV_NULL               ((s32)0x80550F8B)
#define SCE_NP_COMMERCE2_ERROR_ENV_TOO_LONG           ((s32)0x80550F8C)
#define SCE_NP_COMMERCE2_ERROR_CONTENT_LEN_OVERFLOW   ((s32)0x80550F90)
#define SCE_NP_COMMERCE2_ERROR_NO_CACHED_SESSION      ((s32)0x80550F91)
#define SCE_NP_COMMERCE2_ERROR_SESSION_EXPIRED        ((s32)0x80550F93)
#define SCE_NP_COMMERCE2_ERROR_INVALID_STORE_TYPE     ((s32)0x80550F96)
#define SCE_NP_COMMERCE2_ERROR_SERVER_BASE            ((s32)0x80551000)
#define SCE_NP_COMMERCE2_ERROR_SERVER_CODE_2B         ((s32)0x8055102B)
#define SCE_NP_COMMERCE2_ERROR_SERVER_CODE_41         ((s32)0x80551041)
#define SCE_NP_ERROR_OUT_OF_STACK                     ((s32)0x80550004)

#define SDK_VER_STACK_CHECK 0x0505000F
#define SDK_VER_600         0x05FFFFFFu
#define SDK_VER_603         0x0602FFFFu

/* Imported functions */
extern int KprintfForUser(const char *fmt, ...);
extern int sceKernelGetCompiledSdkVersion(void);

extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void  sceNetIfhandle_lib_76BAD213(void *ptr);

extern int sceNpCore_52440ABF(char *envBuf);
extern int sceNpCore_E7AED5A3(char *consoleIdBuf, u32 bufSize);

extern int sceRtcGetCurrentTick(u64 *tick);

extern int sceNet_lib_80C9F02A(char *dst, const char *src); /* strcpy */
extern int sceNet_lib_B5CE388A(char *dst, const char *src, u32 n); /* strncpy */
extern int sceNet_lib_CF705E46(char *dst, const char *fmt, ...); /* sprintf */
extern u32 sceNet_lib_D8722983(const char *s); /* strlen */

extern int sceUriEscape(char *out, u32 *outLen, u32 outSize, const char *in);
extern int sceParseHttpResponseHeader(const char *hdr, u32 hdrLen, const char *name, const char **val, u32 *valLen);

extern int sceHttpCreateTemplate(const char *agent, int httpVer, int autoProxy);
extern int sceHttpDeleteTemplate(int tmplId);
extern int sceHttpEnableCookie(int tmplId);
extern int sceHttpSetConnectTimeOut(int id, u32 usec);
extern int sceHttpSetRecvTimeOut(int id, u32 usec);
extern int sceHttpSetSendTimeOut(int id, u32 usec);
extern int sceHttp_87F1E666(int tmplId, int opt);
extern int sceHttp_3C478044(int tmplId, int opt);
extern int sceHttpCreateConnection(int tmplId, const char *host, const char *scheme, u16 port, int enableKeepalive);
extern int sceHttpDeleteConnection(int connId);
extern int sceHttpCreateRequest(int connId, int method, const char *path, u64 contentLength);
extern int sceHttpDeleteRequest(int reqId);
extern int sceHttpAddExtraHeader(int reqId, const char *name, const char *value, int mode);
extern int sceHttpSendRequest(int reqId, const void *data, u32 size);
extern int sceHttpAbortRequest(int reqId);
extern int sceHttpGetStatusCode(int reqId, int *statusCode);
extern int sceHttpGetAllHeader(int reqId, const char **hdr, u32 *hdrSize);
extern int sceHttpGetContentLength(int reqId, u64 *contentLength);
extern int sceHttpReadData(int reqId, void *buf, u32 size);

extern int sceNp_2E4769C3(void);
extern int sceNpGetAccountRegion(char *countryCode, u32 *langCode);
extern int sceNp_B819A0C8(char *username, char *password, u64 *accountId);
extern int sceNp_E24DA399(void);

extern int   sceKernelCpuSuspendIntr(void);
extern void  sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int val, u32 size);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);
extern int   _sceKernelCheckThreadStack(void);
extern u64   sceKernelGetSystemTimeWide(void);

/* Forward declarations of exported internal helpers */
int sceNpCommerce2_06618C81(void *mgr, void *req, u32 ctxId, u32 *reqIdOut);
int sceNpCommerce2_02C307C0(void *mgr, u32 reqId);
int sceNpCommerce2_73F9CD9E(void *mgr, u32 ctxId);
int sceNpCommerce2_851E7844(void *mgr, u32 reqId, void **reqOut);
void sceNpCommerce2_8642C532(void *mgr, void *req);
int sceNpCommerce2_2AFD0CC7(void *mgr, void *ctx, u32 *ctxIdOut, int isInternal);
int sceNpCommerce2_8139E653(void *mgr, u32 ctxId);
int sceNpCommerce2_0A6AB496(void *mgr, u32 ctxId, void **ctxOut, int checkDeleted);
void sceNpCommerce2_51A88E18(void *ctx);
void sceNpCommerce2_77BE411B(void *mgr, void *ctx);
int sceNpCommerce2_8E80E661(void *mgr);
int sceNpCommerce2_CCEEA788(int *tmplIdOut, int *connIdOut, int enableCookie);
int sceNpCommerce2_382803C8(int tmplId, int storeType, int action, const char *dir,
                            const char *actionName, const char *cookie,
                            int *connIdInOut, int *reqIdOut, u64 contentLength);
int sceNpCommerce2_4D43B84D(int httpReqId, u32 *buildVerInOut);
int sceNpCommerce2_D4C319C6(int httpReqId, u8 *bufBase, u8 *bufCur, u32 bufSize, int *truncatedOut);
int sceNpCommerce2_CBA487E9(int tmplId, int connId);
int sceNpCommerce2_80A1A7A0(int httpReqId);
int sceNpCommerce2_AE614CC6(int httpReqId, const void *data, u32 size);
int sceNpCommerce2_44B8A9A9(int httpReqId, void *buf, u32 size);
int sceNpCommerce2_63B96D73(int httpReqId);
int sceNpCommerce2_75DF99A9(int tmplId, int *connIdInOut);
int sceNpCommerce2_66EC0829(char *out, u32 outSize, const char *in, u32 inLen, u32 *outLen);
int sceNpCommerce2_C27FEAB1(void *ctx, int httpReqId, int action, u8 *statusOut, u8 *buf, u32 bufSize);
int sceNpCommerce2_ADAA054C(u8 *buf, u32 bufSize, u32 bodyLen, int isTruncated);
int sceNpCommerce2_F8D175FA(u8 *buf, u32 bufSize, const char *country, const char *lang);
int sceNpCommerce2_FC687AC7(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut, int allowTrunc);
int sceNpCommerce2_3546C737(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut, int allowTrunc);
int sceNpCommerce2_69FEA1C8(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *rangeOut, int allowTrunc);
int sceNpCommerce2_E3455E27(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut);
int sceNpCommerce2_3F6D413C(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut);
int sceNpCommerce2_22CEC5E2(char *out, int outSize, u32 lo, u32 hi, int isDecimal);
int sceNpCommerce2_460A1D92(char *out, u32 outSize);
int sceNpCommerce2_F0900551(void *ctx, int *versionOut);
int sceNpCommerce2_02CADA41(void *ctx, char *username, char *password);
int sceNpCommerce2_157FA8AA(char *consoleId, u32 size);
int sceNpCommerce2_B7911D82(void);
int sceNpCommerce2_FC47B63C(u64 *timeOut);
int sceNpCommerce2_48D77A66(void *res, const void *buf, u32 bufSize);
int sceNpCommerce2_287B6FB3(void *res, u32 idx, void *info);
int sceNpCommerce2_3D88DCCA(void *contentInfo, void *productInfo);
int sceNpCommerce2_1D9C32C0(void *productInfo, void *gameProductInfo);
int sceNpCommerce2_5C691546(void *res, void *productInfo);
int sceNpCommerce2_993C29DB(void *productInfo, void *gameProductInfo);
int sceNpCommerce2_3755FC84(void *ratingInfo, u32 idx, void *desc);

/* Byte-swap helpers */
static inline u16 bswap16(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}

static inline u32 bswap32(u32 v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

static inline u32 rotr32(u32 v, u32 n) {
    return (v >> n) | (v << (32 - n));
}

/* Data structures */
typedef struct Commerce2Req {
    struct Commerce2Req *next;      /* +0x00 */
    u32 refCount;                   /* +0x04 */
    u32 reqId;                      /* +0x08 */
    u32 flags;                      /* +0x0C: bit0=deleted, bit1=aborted */
    int httpReqId;                  /* +0x10 */
    u16 action;                     /* +0x14 */
    u16 pad;                        /* +0x16 */
} Commerce2Req; /* 24 bytes */

typedef struct Commerce2Ctx {
    struct Commerce2Ctx *next;      /* +0x00 (word 0) */
    u32 refCount;                   /* +0x04 (word 1) */
    u32 version;                    /* +0x08 (word 2) */
    u32 ctxId;                      /* +0x0C (word 3) */
    u32 flags;                      /* +0x10 (word 4): bit0=deleted */
    u32 reserved5;                  /* +0x14 (word 5) */
    int httpTmplId;                 /* +0x18 (word 6) */
    int httpConnId;                 /* +0x1C (word 7) */
    Commerce2Req *reqList;          /* +0x20 (word 8) */
    u8 sessionBuf[1024];            /* +0x24 (words 9..264) */
    u32 sessionLen;                 /* +0x424 (word 265) */
    u32 storeType;                  /* +0x428 (word 266) */
    u32 pad267;                     /* +0x42C (word 267) */
    u64 lastActivityTime;           /* +0x430 (words 268..269) */
    u32 isInternal;                 /* +0x438 (word 270) */
    u32 buildVersion;               /* +0x43C (word 271) */
    char country[4];                /* +0x440 (word 272) */
    char lang[4];                   /* +0x444 (word 273) */
} Commerce2Ctx; /* 1096 bytes */

typedef struct Commerce2FormField {
    u32 len;
    const char *str;
} Commerce2FormField;

/* Global state (BSS) */
static u32 g_coreInitialized = 0;
static u32 g_apiInitialized = 0;
static Commerce2Ctx *g_ctxList = NULL;
static u32 g_ctxIdCounter = 0;
static u32 g_ctxMgrDummy = 0;
static u32 g_reqIdCounter = 0;
static int g_envFlag = 0;
static char g_envStr[16];

/* Read-only tables */
static const char *const g_actionNames[4] = {
    "crSess",
    "delSess",
    "getCat",
    "getProd",
};

static const char *const g_dirNames[9] = {
    "",
    "store/",
    "store/",
    "store/",
    "store/",
    "reg/",
    "cam/",
    "upgrade/",
    "pwd/",
};

static const Commerce2FormField g_formFields[] = {
    {  1, "&" },                    /* 0 */
    {  1, "," },                    /* 1 */
    {  8, "version=" },             /* 2 */
    {  9, "username=" },            /* 3 */
    {  9, "password=" },            /* 4 */
    { 10, "consoleId=" },           /* 5 */
    { 11, "categoryId=" },          /* 6 */
    { 14, "startPosition=" },       /* 7 */
    { 18, "maxCountOfResults=" },   /* 8 */
    { 10, "productId=" },           /* 9 */
    { 10, "storeType=" },           /* 10 */
};

static const Commerce2FormField g_rewardIdField = { 9, "rewardId=" };

static const char *const g_httpHeaders[8] = {
    "Content-Type",       "application/x-www-form-urlencoded",
    "Authorization",      "Basic cjI0NjpHUG9FZmJkYQ==",
    "X-MediaInformation", "PSP/480x272",
    "Accept-Language",    "ja, en, fr, es, de, it, nl, pt, ru, ko, zh, ch",
};

/* DES S-box & PC2 tables (from firmware .rodata 0xA7F4..0xA924) */
static const u8 g_desSbox[256] = {
    0xef, 0xa7, 0x2c, 0x4d, 0x03, 0xdd, 0xea, 0xd1, 0x41, 0x0d, 0xc1, 0xb2, 0xfd, 0x78, 0xbf, 0x0f,
    0xd8, 0x9e, 0x4a, 0x28, 0x74, 0x0b, 0x24, 0xbd, 0x1e, 0xe3, 0x1f, 0xe4, 0x47, 0x95, 0xc2, 0x78,
    0x26, 0x60, 0x79, 0xf6, 0xef, 0x36, 0x47, 0x4a, 0xfb, 0x36, 0xa2, 0x0f, 0x22, 0x4f, 0x7c, 0x93,
    0xb3, 0xf9, 0xb6, 0x8b, 0xd8, 0x60, 0xd9, 0x17, 0x84, 0x5a, 0x68, 0xd1, 0x1e, 0xa3, 0x15, 0xa4,
    0x39, 0x11, 0x80, 0x3a, 0xac, 0x24, 0x56, 0xec, 0xa7, 0xd2, 0x5d, 0xc9, 0x60, 0x87, 0x01, 0x35,
    0x62, 0xc8, 0x33, 0x93, 0xc1, 0x52, 0xfd, 0x56, 0xcd, 0x75, 0xf4, 0x7e, 0xba, 0xec, 0xae, 0xcb,
    0x5c, 0xbb, 0xde, 0x55, 0x96, 0xc1, 0x30, 0x20, 0x90, 0x4c, 0x07, 0xa0, 0x59, 0xba, 0x9b, 0xfe,
    0x05, 0x24, 0xe5, 0x6c, 0x3b, 0xfe, 0x83, 0x89, 0x7a, 0x8f, 0x9b, 0x17, 0x85, 0x19, 0x68, 0x62,
    0x40, 0xda, 0x49, 0x17, 0xfd, 0x13, 0xb4, 0x62, 0x1e, 0x66, 0x2e, 0x4b, 0xc8, 0xaf, 0x83, 0xb1,
    0xe7, 0x49, 0x1f, 0xb4, 0x8a, 0xd0, 0xc2, 0xde, 0x8b, 0x90, 0xb5, 0xd1, 0x21, 0x06, 0x7c, 0x87,
    0xda, 0x8c, 0xa2, 0xc9, 0x43, 0x6a, 0x19, 0x14, 0x64, 0xfb, 0xd8, 0x3c, 0x9f, 0x91, 0xe5, 0x4a,
    0x2d, 0x37, 0x7c, 0x7e, 0x14, 0x8d, 0x2f, 0xa8, 0xb1, 0x0d, 0x83, 0xe2, 0x72, 0x78, 0xda, 0x7d,
    0xf5, 0xbf, 0xf7, 0xa0, 0x5b, 0x49, 0x6b, 0x9f, 0xc8, 0x11, 0x90, 0xf6, 0xb6, 0xf4, 0xfe, 0x5c,
    0x9c, 0x23, 0xc4, 0x6a, 0x37, 0xe5, 0x01, 0x09, 0x76, 0xce, 0x5a, 0x8d, 0xec, 0x3b, 0x97, 0xf0,
    0x39, 0x55, 0x61, 0x0f, 0xa0, 0xbc, 0xa6, 0xe3, 0xa3, 0xa2, 0x3d, 0x53, 0x05, 0x57, 0x40, 0x25,
    0x52, 0xe8, 0x0b, 0x95, 0x6e, 0x22, 0x58, 0x36, 0x0f, 0x74, 0xe6, 0x28, 0xd9, 0xce, 0x3d, 0xcb,
};

static const u16 g_desPc2[24] = {
    0x0301, 0x000c, 0x0713, 0x151a, 0x0d06, 0x110f, 0x1807, 0x0519,
    0x1b03, 0x0a0b, 0x1412, 0x0f16, 0x1309, 0x0414, 0x1005, 0x160e,
    0x0b1b, 0x0102, 0x1a04, 0x1200, 0x0817, 0x020a, 0x1710, 0x0c0d,
};

/* 3DES keys + zero IV (from firmware .data 0xAD00..0xAD20) */
static const u32 g_3desKey1Lo = 0x91fda7e0u;
static const u32 g_3desKey1Hi = 0xcdf8451au;
static const u32 g_3desKey2Lo = 0x9e34012cu;
static const u32 g_3desKey2Hi = 0xf4d56d92u;
static const u32 g_3desKey3Lo = 0x4994516du;
static const u32 g_3desKey3Hi = 0xfd9b80f1u;
static const u8  g_3desIv[8]  = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* =========================================================================
 * DES / 3DES implementation (used by sceNpCommerce2_EE7255FE)
 * ========================================================================= */

static u64 desInitialPerm(u32 a1, u32 a2) {
    u32 t, l, r;
    t = ((a2 >> 4) ^ a1) & 0x0F0F0F0Fu;
    a1 ^= t;
    a2 ^= (t << 4);

    l = (a2 & 0xFFFFu) | ((a1 & 0xFFFFu) << 16);
    r = (a1 & 0xFFFF0000u) | (a2 >> 16);

    t = ((r >> 2) ^ l) & 0x33333333u;
    l ^= t;
    r ^= (t << 2);

    t = ((r >> 8) ^ l) & 0x00FF00FFu;
    l ^= t;
    r ^= (t << 8);

    t = ((l >> 1) ^ r) & 0x55555555u;
    r ^= t;
    l ^= (t << 1);

    return ((u64)l << 32) | r;
}

static u64 desFinalPerm(u32 a1, u32 a2) {
    u32 t, l, r;
    t = ((a2 >> 1) ^ a1) & 0x55555555u;
    a1 ^= t;
    a2 ^= (t << 1);

    t = ((a1 >> 8) ^ a2) & 0x00FF00FFu;
    a1 ^= (t << 8);
    a2 ^= t;

    t = ((a1 >> 2) ^ a2) & 0x33333333u;
    a1 ^= (t << 2);
    a2 ^= t;

    l = (a1 & 0xFFFF0000u) | (a2 >> 16);
    r = (a2 & 0xFFFFu) | ((a1 & 0xFFFFu) << 16);

    t = ((r >> 4) ^ l) & 0x0F0F0F0Fu;
    l ^= t;
    r ^= (t << 4);

    return ((u64)r << 32) | l;
}

static u32 desFeistel(u32 r, const u32 *rk) {
    u32 e0 = (rotr32(r, 31) & 0xFF00003Fu) |
             ((((r >> 3) & 0x3FFFu) << 10) & 0x00FC003Fu) |
             ((((r >> 3) & 0x03FFu) <<  8) & 0x0003F03Fu) |
             (((r >> 3) & 0x003Fu) << 6);
    u32 e1 = (rotr32(r, 9) & 0xFFFC0000u) |
             ((((r >> 15) & 0x3FFFu) << 4) & 0x0003F000u) |
             ((((r >> 15) & 0x03FFu) << 2) & 0x00000FC0u) |
             ((r >> 15) & 0x003Fu);
    u32 x0 = e0 ^ rk[0];
    u32 x1 = e1 ^ rk[1];

    u32 b0 = g_desSbox[4 * ((x0 >>  6) & 0x3F) + 3];
    u32 b1 = g_desSbox[4 * ((x1 >>  6) & 0x3F) + 0];
    u32 b2 = g_desSbox[4 * ((x1 >>  6) & 0x3F) + 1];
    u32 b3 = g_desSbox[4 * ((x1 >> 18) & 0x3F) + 0];
    u32 v6 = (b0 & 0xFFu) | ((b1 & 0xFFu) << 8) | ((b2 & 0xFFu) << 16) | ((b3 & 0xFFu) << 24);

    u8 v8  = g_desSbox[4 * ((x0 >> 12) & 0x3F) + 2];
    u8 v9  = g_desSbox[4 * ((x1 >>  0) & 0x3F) + 1];
    u8 v10 = g_desSbox[4 * ((x1 >> 12) & 0x3F) + 0];

    u32 s = (v6 & 0xF0F000F0u) |
            (((u32)g_desSbox[4 * ((x0 >> 18) & 0x3F) + 2] << 8) & 0x0000F0FFu) |
            (g_desSbox[4 * (x0 & 0x3F) + 3] & 0x0Fu) |
            (((u32)(v8 & 0x0Fu)) << 8) |
            (((u32)(v9 & 0x0Fu)) << 16) |
            (((u32)(v10 & 0x0Fu)) << 24);

    return (rotr32(s, 27) & 0x40402402u) |
           ((u32)v9 << 31) |
           ((((s >> 10) & 7u) << 27) & 0x30402402u) |
           (((s & 0xFu) << 24) & 0x08402402u) |
           ((((s >> 18) & 7u) << 24) & 0x04402402u) |
           ((((s >>  3) & 3u) << 24) & 0x02402402u) |
           ((((u32)(u16)s >> 14) << 23) & 0x01402402u) |
           ((s >> 31) << 23) |
           (((u32)(v8 & 3u) << 20) & 0x00202402u) |
           ((((s >>  3) & 0xFu) << 17) & 0x00102402u) |
           (((u32)(v10 & 0xFu) << 16) & 0x00082402u) |
           (((s & 0x4000u) != 0) << 18) |
           (((s & 3u) << 16) & 0x00022402u) |
           ((((s >> 20) & 7u) << 14) & 0x00012402u) |
           ((((s >> 29) & 3u) << 14) & 0x0000A402u) |
           (((u32)(v10 & 1u)) << 14) |
           (((s & 0x40000u) != 0) << 12) |
           ((s & 1u) << 11) |
           (((s & 0x20000000u) != 0) << 9) |
           ((32u * ((s >> 20) & 0xFu)) & 0x102u) |
           ((16u * ((s >> 10) & 0xFu)) & 0x082u) |
           ((32u * ((s >> 18) & 3u)) & 0x042u) |
           ((8u * (s & 7u)) & 0x022u) |
           ((4u * (v10 & 7u)) & 0x012u) |
           (8u * ((s & 0x400u) != 0)) |
           (4u * ((s & 0x200000u) != 0)) |
           ((s & 0x80u) != 0);
}

static int desKeySchedule(u32 *rk, u32 keyLo, u32 keyHi) {
    u32 c = 0, d = 0;
    u32 bit = 57;
    do {
        u32 t = rotr32(d, 31);
        c = ((c << 1) & 0xFFFFFFEFu) | ((t & 1u) << 4);
        u32 kb = (bit & 0x20u) ? (keyHi >> (bit & 0x1Fu)) : (keyLo >> (bit & 0x1Fu));
        bit = (bit - 8 + (((bit - 8) & 0x40u) ? 1 : 0)) & 0x3Fu;
        d = (t & 0xFFFFFFEFu) | ((kb & 1u) << 4);
    } while (bit != 0);

    u32 bs = bswap32(c);
    u32 c28 = ((bs & 0xF000000Fu) | ((bs & 0xFu) << 4)) >> 4;
    u32 d28 = d >> 4;

    u32 shifts = 0x6AAA9AA5u; /* 1789504165 */
    while (shifts != 0) {
        u32 sh = shifts & 3u;
        c28 = rotr32((c28 & 0x0FFFFFFFu) | ((c28 & 0xFFu) << 28), sh);
        d28 = rotr32((d28 & 0x0FFFFFFFu) | ((d28 & 0xFFu) << 28), sh);
        u32 k0 = 0, k1 = 0;
        int idx = 24;
        while (idx > 0) {
            idx--;
            u16 p = g_desPc2[idx];
            k0 = (k0 << 1) | ((c28 >> (p >> 8)) & 1u);
            k1 = (k1 << 1) | ((d28 >> (p & 0xFFu)) & 1u);
        }
        rk[0] = k0;
        rk[1] = k1;
        rk += 2;
        shifts >>= 2;
    }
    return 0;
}

static u64 desEncryptBlock(const u32 *rk, u64 blk) {
    u64 ip = desInitialPerm((u32)blk, (u32)(blk >> 32));
    u32 r = (u32)ip;
    u32 l = (u32)(ip >> 32);
    int i;
    for (i = 0; i < 16; i += 2) {
        l ^= desFeistel(r, rk + 2 * i);
        r ^= desFeistel(l, rk + 2 * (i + 1));
    }
    return desFinalPerm(l, r);
}

/* =========================================================================
 * Helper: format first two '-' segments of a content/category/product ID
 * ========================================================================= */

int sceNpCommerce2_4484DB5A(const char *src, char *dst, u32 dstSize) {
    const char *p = src;
    char ch;
    u32 len, maxLen;

    if (*p != '\0' && *p != '-') {
        p++;
        while (*p != '\0' && *p != '-')
            p++;
    }
    ch = *p;
    if (ch != '\0') {
        p++;
        ch = *p;
    }
    if (ch != '\0' && ch != '-') {
        p++;
        while (*p != '\0' && *p != '-')
            p++;
    }
    len = (u32)(p - src);
    maxLen = dstSize - 1;
    if (len > maxLen)
        len = maxLen;
    sceKernelMemcpy(dst, src, len);
    dst[len] = '\0';
    return (int)(uintptr_t)dst;
}

int sceNpCommerce2_E3EC9E14(const char *tag, const char *const *ids, u32 count) {
    char buf[32];
    u32 i;

    if (!sceNp_2E4769C3())
        return 0;
    if (!tag || !ids || count == 0)
        return 0;

    sceNpCommerce2_4484DB5A(ids[0], buf, sizeof(buf));
    if (count < 2)
        return KprintfForUser("%s: %s\n", tag, buf);

    KprintfForUser("%s: %s", tag, buf);
    for (i = 1; i < count; i++) {
        sceNpCommerce2_4484DB5A(ids[i], buf, sizeof(buf));
        KprintfForUser(", %s", buf);
    }
    return KprintfForUser("\n");
}

/* =========================================================================
 * Environment / misc helpers
 * ========================================================================= */

static int setEnvString(const char *env) {
    if (!env)
        return SCE_NP_COMMERCE2_ERROR_ENV_NULL;
    if (sceNet_lib_D8722983(env) >= 9)
        return SCE_NP_COMMERCE2_ERROR_ENV_TOO_LONG;
    sceNet_lib_80C9F02A(g_envStr, env);
    return 0;
}

int sceNpCommerce2_460A1D92(char *out, u32 outSize) {
    sceNet_lib_B5CE388A(out, g_envStr, outSize);
    out[outSize - 1] = '\0';
    return 0;
}

int sceNpCommerce2_F0900551(void *ctxPtr, int *versionOut) {
    Commerce2Ctx *ctx = (Commerce2Ctx *)ctxPtr;
    if (ctx->version == 1) {
        *versionOut = 1;
        return 0;
    }
    if (ctx->version == 2) {
        *versionOut = 12;
        return 0;
    }
    return SCE_NP_COMMERCE2_ERROR_INVALID_VERSION;
}

int sceNpCommerce2_02CADA41(void *ctx, char *username, char *password) {
    (void)ctx;
    return sceNp_B819A0C8(username, password, NULL);
}

int sceNpCommerce2_D5932A6C(void *ctx, char *countryOut, u32 countrySize, u32 *langOut) {
    char country[16];
    u32 lang;
    int ret;
    (void)ctx;
    ret = sceNpGetAccountRegion(country, &lang);
    if (ret >= 0) {
        if (countryOut)
            sceNet_lib_B5CE388A(countryOut, country, countrySize);
        if (langOut)
            *langOut = lang;
    }
    return ret;
}

int sceNpCommerce2_157FA8AA(char *consoleId, u32 size) {
    int ret = sceNpCore_E7AED5A3(consoleId, size);
    if (ret < 0)
        return SCE_NP_COMMERCE2_ERROR_CONSOLE_ID;
    return ret;
}

int sceNpCommerce2_B7911D82(void) {
    return g_envFlag;
}

static int setEnvFlag(int flag) {
    g_envFlag = flag;
    return 0;
}

int sceNpCommerce2_FC47B63C(u64 *timeOut) {
    *timeOut = sceKernelGetSystemTimeWide();
    return 0;
}

/* =========================================================================
 * Context & Request Manager
 * ========================================================================= */

int sceNpCommerce2_2AFD0CC7(void *mgr, void *ctxPtr, u32 *ctxIdOut, int isInternal) {
    Commerce2Ctx *ctx = (Commerce2Ctx *)ctxPtr;
    Commerce2Ctx *cur;
    u32 count = 0;
    u32 maxCount = (isInternal == 1) ? 4u : 1u;
    int intr;
    int ret = SCE_NP_COMMERCE2_ERROR_CTX_MAX;
    (void)mgr;

    intr = sceKernelCpuSuspendIntr();
    for (cur = g_ctxList; cur != NULL; cur = cur->next) {
        if ((int)cur->isInternal == isInternal)
            count++;
    }
    if (count < maxCount) {
        u32 nextId = (g_ctxIdCounter == 0xFFFFu) ? 1u : (g_ctxIdCounter + 1u);
        while (nextId != g_ctxIdCounter) {
            int found = 0;
            for (cur = g_ctxList; cur != NULL; cur = cur->next) {
                if (cur->ctxId == nextId) {
                    found = 1;
                    break;
                }
            }
            if (!found)
                break;
            nextId = (nextId == 0xFFFFu) ? 1u : (nextId + 1u);
        }
        ret = SCE_NP_COMMERCE2_ERROR_CTX_ID_EXHAUSTED;
        if (nextId != g_ctxIdCounter) {
            ctx->isInternal = (u32)isInternal;
            ctx->next = g_ctxList;
            g_ctxList = ctx;
            ctx->ctxId = nextId;
            *ctxIdOut = nextId;
            g_ctxIdCounter = nextId;
            ret = 0;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNpCommerce2_8139E653(void *mgr, u32 ctxId) {
    Commerce2Ctx *cur, *prev = NULL;
    int intr;
    int ret = SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND;
    (void)mgr;

    intr = sceKernelCpuSuspendIntr();
    cur = g_ctxList;
    while (cur != NULL && cur->ctxId != ctxId) {
        prev = cur;
        cur = cur->next;
    }
    if (cur != NULL) {
        if (prev != NULL)
            prev->next = cur->next;
        else
            g_ctxList = cur->next;
        cur->next = NULL;

        cur->flags |= 1u;
        if (cur->refCount == 0 && cur->reqList == NULL) {
            if (cur->httpTmplId != 0)
                sceNpCommerce2_CBA487E9(cur->httpTmplId, cur->httpConnId);
            cur->httpTmplId = 0;
            sceNetIfhandle_lib_76BAD213(cur);
        }
        ret = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNpCommerce2_0A6AB496(void *mgr, u32 ctxId, void **ctxOut, int checkDeleted) {
    Commerce2Ctx *cur;
    int intr;
    int ret = SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND;
    (void)mgr;

    intr = sceKernelCpuSuspendIntr();
    cur = g_ctxList;
    while (cur != NULL && cur->ctxId != ctxId)
        cur = cur->next;

    if (cur != NULL && (!checkDeleted || (cur->flags & 1u) == 0)) {
        cur->refCount++;
        *ctxOut = cur;
        ret = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

void sceNpCommerce2_51A88E18(void *ctxPtr) {
    Commerce2Ctx *ctx = (Commerce2Ctx *)ctxPtr;
    if ((ctx->flags & 1u) != 0 && ctx->refCount == 0 && ctx->reqList == NULL) {
        if (ctx->httpTmplId != 0)
            sceNpCommerce2_CBA487E9(ctx->httpTmplId, ctx->httpConnId);
        ctx->httpTmplId = 0;
        sceNetIfhandle_lib_76BAD213(ctx);
    }
}

void sceNpCommerce2_77BE411B(void *mgr, void *ctxPtr) {
    Commerce2Ctx *ctx = (Commerce2Ctx *)ctxPtr;
    int intr;
    (void)mgr;

    intr = sceKernelCpuSuspendIntr();
    ctx->refCount--;
    if ((ctx->flags & 1u) != 0 && ctx->refCount == 0 && ctx->reqList == NULL) {
        if (ctx->httpTmplId != 0)
            sceNpCommerce2_CBA487E9(ctx->httpTmplId, ctx->httpConnId);
        ctx->httpTmplId = 0;
        sceNetIfhandle_lib_76BAD213(ctx);
    }
    sceKernelCpuResumeIntr(intr);
}

int sceNpCommerce2_8E80E661(void *mgr) {
    u32 ids[5];
    u32 count = 0;
    u32 i;
    Commerce2Ctx *cur;
    int intr;

    intr = sceKernelCpuSuspendIntr();
    for (cur = g_ctxList; cur != NULL && count < 5; cur = cur->next)
        ids[count++] = cur->ctxId;
    sceKernelCpuResumeIntr(intr);

    for (i = 0; i < count; i++) {
        sceNpCommerce2_73F9CD9E(mgr, ids[i]);
        sceNpCommerce2_8139E653(mgr, ids[i]);
    }
    return 0;
}

int sceNpCommerce2_06618C81(void *mgr, void *reqPtr, u32 ctxId, u32 *reqIdOut) {
    Commerce2Req *req = (Commerce2Req *)reqPtr;
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *cur;
    u32 count = 0;
    int intr;
    int ret;

    intr = sceKernelCpuSuspendIntr();
    ret = sceNpCommerce2_0A6AB496(mgr, ctxId, (void **)&ctx, 1);
    if (ret >= 0) {
        for (cur = ctx->reqList; cur != NULL; cur = cur->next)
            count++;
        u32 maxReqs = (ctx->isInternal == 1) ? 8u : 1u;
        ret = SCE_NP_COMMERCE2_ERROR_REQ_MAX;
        if (count < maxReqs) {
            u32 nextId = (g_reqIdCounter == 0xFFFFu) ? 1u : (g_reqIdCounter + 1u);
            while (nextId != g_reqIdCounter) {
                Commerce2Ctx *tmpCtx = NULL;
                int findRet = sceNpCommerce2_0A6AB496(mgr, ctxId, (void **)&tmpCtx, 0);
                if (findRet >= 0) {
                    Commerce2Req *r = tmpCtx->reqList;
                    while (r != NULL && (u16)r->reqId != (u16)nextId)
                        r = r->next;
                    if (r == NULL)
                        findRet = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
                }
                if (tmpCtx != NULL)
                    sceNpCommerce2_77BE411B(mgr, tmpCtx);
                if (findRet < 0)
                    break;
                nextId = (nextId == 0xFFFFu) ? 1u : (nextId + 1u);
            }
            ret = SCE_NP_COMMERCE2_ERROR_REQ_ID_EXHAUSTED;
            if (nextId != g_reqIdCounter) {
                u32 fullId = (ctxId << 16) | (u16)nextId;
                g_reqIdCounter = nextId;
                req->next = ctx->reqList;
                req->reqId = fullId;
                ctx->reqList = req;
                *reqIdOut = fullId;
                ret = 0;
            }
        }
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(mgr, ctx);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNpCommerce2_02C307C0(void *mgr, u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *cur, *prev = NULL;
    int intr;
    int ret;

    intr = sceKernelCpuSuspendIntr();
    ret = sceNpCommerce2_0A6AB496(mgr, reqId >> 16, (void **)&ctx, 0);
    if (ret >= 0) {
        cur = ctx->reqList;
        while (cur != NULL && cur->reqId != reqId) {
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            if (prev != NULL)
                prev->next = cur->next;
            else
                ctx->reqList = cur->next;
            cur->next = NULL;
        }
        sceNpCommerce2_77BE411B(mgr, ctx);
        ret = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
        if (cur != NULL) {
            if (cur->httpReqId != 0) {
                sceNpCommerce2_63B96D73(cur->httpReqId);
                cur->flags |= 2u;
            }
            cur->flags |= 1u;
            if (cur->refCount == 0 && (cur->flags & 1u) != 0) {
                if (cur->httpReqId != 0)
                    sceNpCommerce2_80A1A7A0(cur->httpReqId);
                cur->httpReqId = 0;
                sceNetIfhandle_lib_76BAD213(cur);
            }
            ret = 0;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNpCommerce2_73F9CD9E(void *mgr, u32 ctxId) {
    Commerce2Ctx *ctx = NULL;
    u32 ids[8];
    u32 count = 0;
    u32 i;
    int intr;
    int ret;

    intr = sceKernelCpuSuspendIntr();
    ret = sceNpCommerce2_0A6AB496(mgr, ctxId, (void **)&ctx, 0);
    if (ret < 0) {
        sceKernelCpuResumeIntr(intr);
        return ret;
    }
    Commerce2Req *cur;
    for (cur = ctx->reqList; cur != NULL && count < 8; cur = cur->next)
        ids[count++] = cur->reqId;
    sceNpCommerce2_77BE411B(mgr, ctx);
    sceKernelCpuResumeIntr(intr);

    for (i = 0; i < count; i++)
        sceNpCommerce2_02C307C0(mgr, ids[i]);
    return ret;
}

int sceNpCommerce2_851E7844(void *mgr, u32 reqId, void **reqOut) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *found = NULL;
    int intr;
    int ret;

    intr = sceKernelCpuSuspendIntr();
    ret = sceNpCommerce2_0A6AB496(mgr, reqId >> 16, (void **)&ctx, 0);
    if (ret >= 0) {
        Commerce2Req *cur = ctx->reqList;
        while (cur != NULL && (u16)cur->reqId != (u16)reqId)
            cur = cur->next;
        if (cur != NULL)
            found = cur;
        else
            ret = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(mgr, ctx);
    if (ret >= 0) {
        found->refCount++;
        *reqOut = found;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

void sceNpCommerce2_8642C532(void *mgr, void *reqPtr) {
    Commerce2Req *req = (Commerce2Req *)reqPtr;
    int intr;
    (void)mgr;

    intr = sceKernelCpuSuspendIntr();
    req->refCount--;
    if (req->refCount == 0 && (req->flags & 1u) != 0) {
        if (req->httpReqId != 0)
            sceNpCommerce2_80A1A7A0(req->httpReqId);
        req->httpReqId = 0;
        sceNetIfhandle_lib_76BAD213(req);
    }
    sceKernelCpuResumeIntr(intr);
}

/* =========================================================================
 * HTTP Transport Layer
 * ========================================================================= */

int sceNpCommerce2_CCEEA788(int *tmplIdOut, int *connIdOut, int enableCookie) {
    int tmplId = sceHttpCreateTemplate("commerce2/1.0 (PSP (PlayStation Portable); 4.00)", 1, 0);
    int ret = tmplId;
    if (tmplId < 0)
        return ret;

    if (enableCookie == 1) {
        ret = sceHttpEnableCookie(tmplId);
        if (ret < 0)
            goto fail;
    }
    ret = sceHttpSetConnectTimeOut(tmplId, 20000000);
    if (ret < 0)
        goto fail;
    ret = sceHttpSetRecvTimeOut(tmplId, 60000000);
    if (ret < 0)
        goto fail;
    ret = sceHttpSetSendTimeOut(tmplId, 20000000);
    if (ret < 0)
        goto fail;

    if (sceNpCommerce2_B7911D82() <= 0) {
        ret = sceHttp_87F1E666(tmplId, 357);
        if (ret < 0)
            goto fail;
        ret = sceHttp_3C478044(tmplId, 26);
    } else {
        ret = sceHttp_87F1E666(tmplId, 64);
        if (ret < 0)
            goto fail;
        ret = sceHttp_3C478044(tmplId, 1);
    }
    if (ret >= 0) {
        *connIdOut = 0;
        *tmplIdOut = tmplId;
        return ret;
    }

fail:
    if (tmplId != 0)
        sceHttpDeleteTemplate(tmplId);
    return ret;
}

int sceNpCommerce2_382803C8(int tmplId, int storeType, int action, const char *dir,
                            const char *actionName, const char *cookie,
                            int *connIdInOut, int *reqIdOut, u64 contentLength) {
    char host[64];
    char path[64];
    char env[16];
    int connId = *connIdInOut;
    int reqId;
    int ret;
    u32 i;
    (void)storeType;
    (void)action;

    ret = sceNpCommerce2_460A1D92(env, 9);
    if (ret < 0)
        return ret;

    if (connId == 0) {
        sceNet_lib_CF705E46(host, "%s.%s.%s", "native", env, "ac.playstation.net");
        host[63] = '\0';
        connId = sceHttpCreateConnection(tmplId, host, "https", 443, 1);
        if (connId < 0)
            return connId;
        *connIdInOut = connId;
    }

    sceNet_lib_CF705E46(path, "/%s/%s%s.%s", "native", dir, actionName, "action");
    path[63] = '\0';
    reqId = sceHttpCreateRequest(connId, 1, path, contentLength);
    if (reqId < 0)
        return reqId;

    for (i = 0; i < 4; i++) {
        ret = sceHttpAddExtraHeader(reqId, g_httpHeaders[2 * i], g_httpHeaders[2 * i + 1], 0);
        if (ret < 0)
            goto fail;
    }
    if (cookie != NULL && sceNet_lib_D8722983(cookie) != 0) {
        ret = sceHttpAddExtraHeader(reqId, "Cookie", cookie, 0);
        if (ret < 0)
            goto fail;
    }
    *reqIdOut = reqId;
    return ret;

fail:
    if (reqId != 0)
        sceHttpDeleteRequest(reqId);
    return ret;
}

int sceNpCommerce2_4D43B84D(int httpReqId, u32 *buildVerInOut) {
    int statusCode;
    const char *allHdr;
    u32 allHdrLen = 0;
    const char *val;
    u32 valLen;
    int ret;

    ret = sceHttpGetStatusCode(httpReqId, &statusCode);
    if (ret < 0)
        return ret;

    if (*buildVerInOut == 0) {
        if (sceHttpGetAllHeader(httpReqId, &allHdr, &allHdrLen) >= 0 &&
            sceParseHttpResponseHeader(allHdr, allHdrLen, "X-NativeBuildVersion", &val, &valLen) >= 0) {
            if (valLen >= sceNet_lib_D8722983("XX.XX.XX.XX")) {
                u32 ver = 0;
                const char *p = val;
                if ((uintptr_t)val < (uintptr_t)0xFFFFFFF7u) {
                    do {
                        ver = 100u * ver + 10u * (u32)(u8)p[0] + (u32)(u8)p[1] - 528u;
                        p += 3;
                    } while ((uintptr_t)p <= (uintptr_t)val + 9u);
                    *buildVerInOut = ver;
                } else {
                    *buildVerInOut = 0;
                }
            }
        }
    }

    if (statusCode == 500 || statusCode == 503)
        return SCE_NP_COMMERCE2_ERROR_SERVER_MAINTENANCE;
    if (statusCode != 200)
        return SCE_NP_COMMERCE2_ERROR_SERVER_ERROR;
    return 0;
}

int sceNpCommerce2_D4C319C6(int httpReqId, u8 *bufBase, u8 *bufCur, u32 bufSize, int *truncatedOut) {
    u64 contentLen = 0;
    int eof = 0;
    u32 totalRead = 0;
    u32 expectedLen = 0;
    u32 avail;
    u32 toRead;
    int ret;
    u8 probe;

    *truncatedOut = 0;
    sceHttpGetContentLength(httpReqId, &contentLen);
    if ((u32)(contentLen >> 32) != 0)
        return SCE_NP_COMMERCE2_ERROR_CONTENT_LEN_OVERFLOW;

    u32 headerUsed = (u32)(bufCur - bufBase);
    avail = bufSize - headerUsed;
    toRead = avail;
    if ((u32)contentLen != 0) {
        expectedLen = (u32)contentLen - headerUsed;
        toRead = (expectedLen < avail) ? expectedLen : avail;
    }

    while (toRead != 0) {
        ret = sceHttpReadData(httpReqId, bufCur, toRead);
        if (ret < 0)
            return ret;
        if (ret == 0) {
            eof = 1;
            break;
        }
        bufCur += ret;
        toRead -= (u32)ret;
        totalRead += (u32)ret;
    }

    if (contentLen != 0) {
        if (expectedLen < avail) {
            if (totalRead != expectedLen)
                return SCE_NP_COMMERCE2_ERROR_UNEXPECTED_EOF;
        } else {
            *truncatedOut = 1;
        }
        return (int)totalRead;
    }

    if (eof == 1)
        return (int)totalRead;

    ret = sceHttpReadData(httpReqId, &probe, 1);
    if (ret < 0)
        return ret;
    if (ret != 0)
        *truncatedOut = 1;
    return (int)totalRead;
}

int sceNpCommerce2_CBA487E9(int tmplId, int connId) {
    if (connId != 0)
        sceHttpDeleteConnection(connId);
    if (tmplId != 0)
        sceHttpDeleteTemplate(tmplId);
    return 0;
}

int sceNpCommerce2_80A1A7A0(int httpReqId) {
    return sceHttpDeleteRequest(httpReqId);
}

int sceNpCommerce2_AE614CC6(int httpReqId, const void *data, u32 size) {
    return sceHttpSendRequest(httpReqId, data, size);
}

int sceNpCommerce2_44B8A9A9(int httpReqId, void *buf, u32 size) {
    u8 *p = (u8 *)buf;
    u32 rem = size;
    u32 total = 0;
    int ret;

    while (rem != 0) {
        ret = sceHttpReadData(httpReqId, p, rem);
        if (ret < 0)
            return ret;
        if (ret == 0)
            return (int)total;
        p += ret;
        rem -= (u32)ret;
        total += (u32)ret;
    }
    return (int)total;
}

int sceNpCommerce2_63B96D73(int httpReqId) {
    return sceHttpAbortRequest(httpReqId);
}

int sceNpCommerce2_75DF99A9(int tmplId, int *connIdInOut) {
    (void)tmplId;
    if (*connIdInOut != 0)
        sceHttpDeleteConnection(*connIdInOut);
    *connIdInOut = 0;
    return 0;
}

int sceNpCommerce2_66EC0829(char *out, u32 outSize, const char *in, u32 inLen, u32 *outLen) {
    (void)inLen;
    return sceUriEscape(out, outLen, outSize, in);
}

/* =========================================================================
 * Request URL/Body Builders
 * ========================================================================= */

static int resolveActionPath(int storeType, u16 action, const char **dirOut, const char **nameOut) {
    char envBuf[12];
    int ret;

    if (action >= 87)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ACTION;
    if ((u32)(storeType - 1) >= 8u)
        return SCE_NP_COMMERCE2_ERROR_INVALID_STORE_TYPE;

    ret = sceNpCommerce2_460A1D92(envBuf, 9);
    if (ret >= 0) {
        *dirOut = g_dirNames[storeType];
        *nameOut = (action < 5 && action >= 1) ? g_actionNames[action - 1] : "";
    }
    return ret;
}

static int sendUintFormField(int httpReqId, int fieldIdx, u32 val, int prependAmp) {
    char numBuf[16];
    int ret;

    if (prependAmp == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0)
            return ret;
    }
    ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[fieldIdx].str, g_formFields[fieldIdx].len);
    if (ret < 0)
        return ret;

    int n = sceNet_lib_CF705E46(numBuf, "%u", val);
    if (n >= 16)
        return SCE_NP_COMMERCE2_ERROR_FORMAT;
    return sceNpCommerce2_AE614CC6(httpReqId, numBuf, sceNet_lib_D8722983(numBuf));
}

static int buildOrSendCreateSessionBody(int sendMode, int httpReqId, u64 *lenOut,
                                        u32 version, const char *username,
                                        const char *password, const char *consoleId) {
    char escBuf[784];
    char numBuf[16];
    s64 total = 0;
    int ret;

    if (sendMode == 1) {
        ret = sendUintFormField(httpReqId, 2, version, 0);
    } else {
        int n = sceNet_lib_CF705E46(numBuf, "%u", version);
        ret = (n < 16) ? (int)(n + g_formFields[2].len) : SCE_NP_COMMERCE2_ERROR_FORMAT;
    }
    if (ret < 0)
        return ret;
    total += ret;

    ret = sceNpCommerce2_66EC0829(escBuf, 769, username, sceNet_lib_D8722983(username), NULL);
    if (ret < 0)
        return ret;
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[3].str, g_formFields[3].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[3].len + sceNet_lib_D8722983(escBuf) + 1);
    }
    if (ret < 0)
        return ret;
    total += ret;

    ret = sceNpCommerce2_66EC0829(escBuf, 769, password, sceNet_lib_D8722983(password), NULL);
    if (ret < 0)
        return ret;
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[4].str, g_formFields[4].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[4].len + sceNet_lib_D8722983(escBuf) + 1);
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[5].str, g_formFields[5].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, consoleId, sceNet_lib_D8722983(consoleId));
    } else {
        ret = (int)(g_formFields[5].len + sceNet_lib_D8722983(consoleId) + 1);
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode != 1)
        *lenOut = (u64)total;
    return 0;
}

static int sendCreateSessionHttp(int storeType, int tmplId, int *connIdInOut, u16 action,
                                 int *httpReqIdInOut, u32 version, const char *username,
                                 const char *password, const char *consoleId) {
    u64 contentLen = 0;
    const char *dir = NULL;
    const char *name = NULL;
    int pass;
    int ret;

    for (pass = 0; pass < 2; pass++) {
        ret = buildOrSendCreateSessionBody(pass != 0, *httpReqIdInOut, &contentLen,
                                           version, username, password, consoleId);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = resolveActionPath(storeType, action, &dir, &name);
            if (ret >= 0)
                ret = sceNpCommerce2_382803C8(tmplId, storeType, action, dir, name, NULL,
                                              connIdInOut, httpReqIdInOut, contentLen);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int sendDeleteSessionHttp(int storeType, int tmplId, int *connIdInOut, u16 action, int *httpReqIdInOut) {
    const char *dir = NULL;
    const char *name = NULL;
    int ret = resolveActionPath(storeType, action, &dir, &name);
    if (ret >= 0)
        ret = sceNpCommerce2_382803C8(tmplId, storeType, action, dir, name, NULL,
                                      connIdInOut, httpReqIdInOut, 0);
    if (ret >= 0) {
        int sret = sceNpCommerce2_AE614CC6(*httpReqIdInOut, NULL, 0);
        return (sret < 0) ? sret : 0;
    }
    return ret;
}

static int buildOrSendGetCategoryBody(int sendMode, int httpReqId, u64 *lenOut,
                                      int storeType, const char *categoryId,
                                      const char *const *rewardIds, u32 rewardCount,
                                      u32 startPos, u32 maxCount) {
    char escBuf[784];
    char numBuf[16];
    int hasReward = (rewardIds != NULL && rewardCount != 0);
    s64 total = 0;
    int ret;

    if (hasReward)
        storeType = 1;

    sceNet_lib_CF705E46(escBuf, "%u", (u32)storeType);
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[10].str, g_formFields[10].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[10].len + sceNet_lib_D8722983(escBuf));
    }
    if (ret < 0)
        return ret;
    total += ret;

    ret = sceNpCommerce2_66EC0829(escBuf, 769, categoryId, sceNet_lib_D8722983(categoryId), NULL);
    if (ret < 0)
        return ret;
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[6].str, g_formFields[6].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[6].len + sceNet_lib_D8722983(escBuf) + 1);
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (hasReward) {
        ret = sceNpCommerce2_66EC0829(escBuf, 769, rewardIds[0], sceNet_lib_D8722983(rewardIds[0]), NULL);
        if (ret < 0)
            return ret;
        if (sendMode == 1) {
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_rewardIdField.str, g_rewardIdField.len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
        } else {
            ret = (int)(g_rewardIdField.len + sceNet_lib_D8722983(escBuf) + 1);
        }
        if (ret < 0)
            return ret;
        total += ret;
    }

    if (sendMode == 1) {
        ret = sendUintFormField(httpReqId, 7, startPos, 1);
    } else {
        int n = sceNet_lib_CF705E46(numBuf, "%u", startPos);
        ret = (n < 16) ? (int)(n + g_formFields[7].len + 1) : SCE_NP_COMMERCE2_ERROR_FORMAT;
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode == 1) {
        ret = sendUintFormField(httpReqId, 8, maxCount, 1);
    } else {
        int n = sceNet_lib_CF705E46(numBuf, "%u", maxCount);
        ret = (n < 16) ? (int)(n + g_formFields[8].len + 1) : SCE_NP_COMMERCE2_ERROR_FORMAT;
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode != 1)
        *lenOut = (u64)total;
    return 0;
}

static int sendGetCategoryHttp(int storeType, int tmplId, int *connIdInOut, u16 action,
                               int *httpReqIdInOut, const char *categoryId,
                               const char *const *rewardIds, u32 rewardCount,
                               u32 startPos, u32 maxCount) {
    u64 contentLen = 0;
    const char *dir = NULL;
    const char *name = NULL;
    int pass;
    int ret;

    for (pass = 0; pass < 2; pass++) {
        ret = buildOrSendGetCategoryBody(pass != 0, *httpReqIdInOut, &contentLen,
                                         storeType, categoryId, rewardIds, rewardCount,
                                         startPos, maxCount);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = resolveActionPath(storeType, action, &dir, &name);
            if (ret >= 0)
                ret = sceNpCommerce2_382803C8(tmplId, storeType, action, dir, name, NULL,
                                              connIdInOut, httpReqIdInOut, contentLen);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int buildOrSendGetProductBody(int sendMode, int httpReqId, u64 *lenOut,
                                     int storeType, const char *categoryId,
                                     const char *productId, const char *const *rewardIds,
                                     u32 rewardCount) {
    char escBuf[784];
    int hasReward = (rewardIds != NULL && rewardCount != 0);
    s64 total = 0;
    int ret;

    if (hasReward)
        storeType = 1;

    sceNet_lib_CF705E46(escBuf, "%u", (u32)storeType);
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[10].str, g_formFields[10].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[10].len + sceNet_lib_D8722983(escBuf));
    }
    if (ret < 0)
        return ret;
    total += ret;

    ret = sceNpCommerce2_66EC0829(escBuf, 769, productId, sceNet_lib_D8722983(productId), NULL);
    if (ret < 0)
        return ret;
    if (sendMode == 1) {
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[9].str, g_formFields[9].len);
        if (ret < 0) return ret;
        ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
    } else {
        ret = (int)(g_formFields[9].len + sceNet_lib_D8722983(escBuf) + 1);
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (categoryId != NULL && categoryId[0] != '\0') {
        ret = sceNpCommerce2_66EC0829(escBuf, 769, categoryId, sceNet_lib_D8722983(categoryId), NULL);
        if (ret < 0)
            return ret;
        if (sendMode == 1) {
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[6].str, g_formFields[6].len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
        } else {
            ret = (int)(g_formFields[6].len + sceNet_lib_D8722983(escBuf) + 1);
        }
        if (ret < 0)
            return ret;
        total += ret;
    }

    if (hasReward) {
        ret = sceNpCommerce2_66EC0829(escBuf, 769, rewardIds[0], sceNet_lib_D8722983(rewardIds[0]), NULL);
        if (ret < 0)
            return ret;
        if (sendMode == 1) {
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_formFields[0].str, g_formFields[0].len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, g_rewardIdField.str, g_rewardIdField.len);
            if (ret < 0) return ret;
            ret = sceNpCommerce2_AE614CC6(httpReqId, escBuf, sceNet_lib_D8722983(escBuf));
        } else {
            ret = (int)(g_rewardIdField.len + sceNet_lib_D8722983(escBuf) + 1);
        }
        if (ret < 0)
            return ret;
        total += ret;
    }

    if (sendMode != 1)
        *lenOut = (u64)total;
    return 0;
}

static int sendGetProductHttp(int storeType, int tmplId, int *connIdInOut, u16 action,
                              int *httpReqIdInOut, const char *categoryId,
                              const char *productId, const char *const *rewardIds,
                              u32 rewardCount) {
    u64 contentLen = 0;
    const char *dir = NULL;
    const char *name = NULL;
    int pass;
    int ret;

    for (pass = 0; pass < 2; pass++) {
        ret = buildOrSendGetProductBody(pass != 0, *httpReqIdInOut, &contentLen,
                                        storeType, categoryId, productId, rewardIds, rewardCount);
        if (ret < 0)
            return ret;
        if (pass == 0) {
            ret = resolveActionPath(storeType, action, &dir, &name);
            if (ret >= 0)
                ret = sceNpCommerce2_382803C8(tmplId, storeType, action, dir, name, NULL,
                                              connIdInOut, httpReqIdInOut, contentLen);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

/* =========================================================================
 * TLV & Response Binary Parsers
 * ========================================================================= */

int sceNpCommerce2_C27FEAB1(void *ctxPtr, int httpReqId, int action, u8 *statusOut, u8 *buf, u32 bufSize) {
    Commerce2Ctx *ctx = (Commerce2Ctx *)ctxPtr;
    u8 header[56];
    u8 rawStatus[4];
    char hexTmp[256];
    int ret;
    (void)action;

    if (bufSize < 60)
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;

    sceKernelMemset(header, 0, sizeof(header));
    *(u32 *)(header + 0)  = ctx->version;
    *(u32 *)(header + 16) = ctx->buildVersion;
    sceNet_lib_B5CE388A((char *)(header + 20), ctx->country, 4);
    header[23] = 0;
    sceNet_lib_B5CE388A((char *)(header + 24), ctx->lang, 4);
    header[27] = 0;
    sceNet_lib_B5CE388A((char *)(header + 28), ".", 4);
    header[31] = 0;
    sceNet_lib_B5CE388A((char *)(header + 32), ",", 4);
    header[35] = 0;
    sceKernelMemcpy(buf, header, sizeof(header));

    ret = sceNpCommerce2_44B8A9A9(httpReqId, rawStatus, 4);
    if (ret < 0)
        return ret;
    if (ret < 4)
        return SCE_NP_COMMERCE2_ERROR_UNEXPECTED_EOF;

    {
        u32 i;
        for (i = 0; i < 4; i++) {
            u8 hi = rawStatus[i] >> 4;
            u8 lo = rawStatus[i] & 0xFu;
            hexTmp[2 * i]     = (char)(hi < 10 ? hi + '0' : hi + 'a' - 10);
            hexTmp[2 * i + 1] = (char)(lo < 10 ? lo + '0' : lo + 'a' - 10);
        }
        hexTmp[8] = '\0';
        (void)hexTmp;
    }

    *statusOut = rawStatus[1];
    rawStatus[2] = 0;
    rawStatus[3] = 0;
    sceKernelMemcpy(buf + 56, rawStatus, 4);
    return 60;
}

int sceNpCommerce2_ADAA054C(u8 *buf, u32 bufSize, u32 bodyLen, int isTruncated) {
    u32 hdr[14];
    if (bufSize < 56)
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
    sceKernelMemcpy(hdr, buf, 56);
    hdr[2] = bodyLen + 4;
    hdr[3] = (u32)isTruncated;
    sceKernelMemcpy(buf, hdr, 56);
    return 0;
}

int sceNpCommerce2_F8D175FA(u8 *buf, u32 bufSize, const char *country, const char *lang) {
    u8 hdr[56];
    if (bufSize < 56)
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
    sceKernelMemcpy(hdr, buf, 56);
    sceNet_lib_B5CE388A((char *)(hdr + 20), country, 4);
    hdr[23] = 0;
    sceNet_lib_B5CE388A((char *)(hdr + 24), lang, 4);
    hdr[27] = 0;
    sceKernelMemcpy(buf, hdr, 56);
    return 0;
}

int sceNpCommerce2_FC687AC7(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 rawTag, rawLen, t, l;
        if (bufLen < (u32)(cur - buf) + 4u)
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;

        sceKernelMemcpy(&rawTag, cur, 2);
        t = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawLen, cur + 2, 2);
        l = bswap16(rawLen);
        cur += 4;

        if (t == tag) {
            if (bufLen < (u32)(cur - buf) + (u32)minLen)
                return SCE_NP_COMMERCE2_ERROR_TLV_SHORT_HEADER;
            if (l < minLen)
                return SCE_NP_COMMERCE2_ERROR_TLV_SHORT_MIN;
            if (bufLen < (u32)(cur - buf) + (u32)l && !allowTrunc)
                return SCE_NP_COMMERCE2_ERROR_TLV_TRUNCATED;
            if (valLenOut != NULL)
                *valLenOut = l;
            return (int)(cur - buf);
        }
        cur += l;
    }
}

int sceNpCommerce2_3546C737(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 blockLen, rawTag, rawCount;
        int off;
        if (bufLen < (u32)(cur - buf))
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;
        off = sceNpCommerce2_FC687AC7(cur, bufLen - (u32)(cur - buf), 1, 4, &blockLen, allowTrunc);
        if (off < 0)
            return off;
        const u8 *p = cur + off;
        sceKernelMemcpy(&rawTag, p, 2);
        rawTag = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawCount, p + 2, 2);
        rawCount = bswap16(rawCount);
        if (rawTag == tag) {
            if (valLenOut != NULL)
                *valLenOut = (u16)(blockLen - 4);
            if (countOut != NULL)
                *countOut = rawCount;
            return (int)((p - buf) + 4);
        }
        cur = p + blockLen;
    }
}

int sceNpCommerce2_69FEA1C8(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *rangeOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 blockLen, rawTag, rawCount, rawStart, rawTotal;
        int off;
        if (bufLen < (u32)(cur - buf))
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;
        off = sceNpCommerce2_FC687AC7(cur, bufLen - (u32)(cur - buf), 2, 8, &blockLen, allowTrunc);
        if (off < 0)
            return off;
        const u8 *p = cur + off;
        sceKernelMemcpy(&rawTag, p, 2);
        rawTag = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawCount, p + 2, 2);
        rawCount = bswap16(rawCount);
        sceKernelMemcpy(&rawStart, p + 4, 2);
        rawStart = bswap16(rawStart);
        sceKernelMemcpy(&rawTotal, p + 6, 2);
        rawTotal = bswap16(rawTotal);
        if (rawTag == tag) {
            if (valLenOut != NULL)
                *valLenOut = (u16)(blockLen - 8);
            if (rangeOut != NULL) {
                sceKernelMemset(rangeOut, 0, 12);
                rangeOut[0] = rawStart;
                rangeOut[1] = rawCount;
                rangeOut[2] = rawTotal;
            }
            return (int)((p - buf) + 8);
        }
        cur = p + blockLen;
    }
}

int sceNpCommerce2_E3455E27(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut) {
    return sceNpCommerce2_FC687AC7(buf, bufLen, tag, minLen, valLenOut, 0);
}

int sceNpCommerce2_3F6D413C(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut) {
    return sceNpCommerce2_3546C737(buf, bufLen, tag, valLenOut, countOut, 0);
}

static int parseResponseHeader(const u8 *buf, u32 bufSize, u32 *versionOut, u32 *bodyLenOut, u32 *truncOut) {
    u32 hdr[14];
    u8 status[4];

    if (bufSize < 60)
        return SCE_NP_COMMERCE2_ERROR_INVALID_RESPONSE;

    sceKernelMemcpy(hdr, buf, 56);
    sceKernelMemcpy(status, buf + 56, 4);
    if (bufSize < hdr[2] + 56u)
        return SCE_NP_COMMERCE2_ERROR_INVALID_RESPONSE;

    *bodyLenOut = hdr[2] - 4u;
    if (versionOut != NULL)
        *versionOut = hdr[0];
    if (truncOut != NULL)
        *truncOut = hdr[3];
    return 60;
}

static int findIndexedArrayItem(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u32 index, u16 *itemLenOut) {
    u16 arrLen;
    u32 count;
    int off = sceNpCommerce2_3F6D413C(buf, bufLen, tag, &arrLen, &count);
    if (off < 0)
        return off;
    if (index >= count)
        return SCE_NP_COMMERCE2_ERROR_INVALID_INDEX;

    const u8 *arrStart = buf + off;
    const u8 *cur = arrStart;
    u16 itemLen = 0;
    u32 i;
    for (i = 0; i <= index; i++) {
        if (i != 0)
            cur += itemLen;
        int itemOff = sceNpCommerce2_E3455E27(cur, arrLen - (u32)(cur - arrStart), tag, minLen, &itemLen);
        if (itemOff < 0)
            return itemOff;
        cur += itemOff;
    }
    *itemLenOut = itemLen;
    return (int)(cur - buf);
}

static int findIndexedRangeItem(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u32 index, u16 *itemLenOut, int allowTrunc) {
    u32 range[3];
    int off = sceNpCommerce2_69FEA1C8(buf, bufLen, tag, NULL, range, allowTrunc);
    if (off < 0)
        return off;
    if (index >= range[1])
        return SCE_NP_COMMERCE2_ERROR_INVALID_INDEX;

    const u8 *cur = buf + off;
    u16 itemLen = 0;
    u32 i;
    for (i = 0; i <= index; i++) {
        if (i != 0)
            cur += itemLen;
        int itemOff = sceNpCommerce2_E3455E27(cur, bufLen - (u32)(cur - buf), tag, minLen, &itemLen);
        if (itemOff < 0)
            return itemOff;
        cur += itemOff;
    }
    *itemLenOut = itemLen;
    return (int)(cur - buf);
}

static int clampRangeCount(const u32 *base, u16 tag, u16 minLen, u32 *range) {
    u16 itemLen;
    u32 i;
    int ret = 0;
    for (i = 0; i < range[1]; i++) {
        ret = findIndexedRangeItem((const u8 *)(uintptr_t)base[3], base[4], tag, minLen, i, &itemLen, (int)base[6]);
        if (ret < 0) {
            if (i != 0) {
                range[1] = i;
                return 0;
            }
            return ret;
        }
    }
    return ret;
}

/* =========================================================================
 * Structured Info Parsers (Session, Category, Content, Product, SKU, Rating)
 * ========================================================================= */

static int parseAccountInfo(const u32 *base, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)base[3];
    u16 len;
    u16 optLen;
    int off = sceNpCommerce2_E3455E27(src, base[4], 94, 50, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 88);
    out[0] = base[0];
    out[1] = base[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;

    u32 acctId[2];
    sceKernelMemcpy(acctId, p, 8);
    sceNpCommerce2_22CEC5E2((char *)(out + 10), 17, bswap32(acctId[1]), bswap32(acctId[0]), 0);
    out[15] = (u32)(uintptr_t)(p + 8);
    out[16] = (u32)(uintptr_t)(p + 25);
    out[17] = (u32)(uintptr_t)(p + 28);

    u32 tmp;
    sceKernelMemcpy(&tmp, p + 34, 4);
    out[18] = bswap32(tmp);
    sceKernelMemcpy(&tmp, p + 38, 4);
    out[20] = (bswap32(tmp) != 0);
    sceKernelMemcpy(&tmp, p + 42, 4);
    out[21] = bswap32(tmp);

    const u8 *tail = p + 46;
    out[5] = (u32)(uintptr_t)tail;
    out[19] = out[1] + 4;
    off = sceNpCommerce2_E3455E27(tail, len - (u32)(tail - p), 93, 0, &optLen);
    if (off >= 0 && optLen != 0)
        out[19] = (u32)(uintptr_t)(tail + off);
    return off;
}

static int parseSessionResultEx(u32 *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    u16 optLen;
    int off = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (off < 0)
        return off;

    const u8 *body = buf + off;
    sceKernelMemset(out, 0, 56);
    out[0] = ver;
    out[1] = (u32)(uintptr_t)buf;
    out[2] = (u32)off + bodyLen;
    out[3] = (u32)(uintptr_t)body;
    out[4] = bodyLen;
    out[13] = (u32)(uintptr_t)(buf + 4);

    int ret = sceNpCommerce2_E3455E27(body, bodyLen, 164, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out[13] = (u32)(uintptr_t)(body + ret);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    if (ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return 0;

    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 22, NULL, out + 10);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 100, NULL, out + 11);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 88, NULL, out + 12);
    if (ret < 0)
        return ret;
    return 0;
}

static int checkSessionExpiredError(const u8 *buf, u32 bufSize, int *expiredOut) {
    u32 ver, bodyLen;
    u32 count = 0;
    int off = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (off < 0)
        return off;
    *expiredOut = (sceNpCommerce2_3F6D413C(buf + off, bodyLen, 120, NULL, &count) >= 0) ? 1 : 0;
    return 0;
}

static int parseCreateSessionResult(u32 *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    int off = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (off < 0)
        return off;

    const u8 *body = buf + off;
    sceKernelMemset(out, 0, 44);
    out[0] = ver;
    out[1] = (u32)(uintptr_t)buf;
    out[2] = (u32)off + bodyLen;
    out[3] = (u32)(uintptr_t)body;
    out[4] = bodyLen;

    int ret = sceNpCommerce2_3F6D413C(body, bodyLen, 22, NULL, out + 10);
    return (ret < 0) ? ret : 0;
}

static int parseCurrencyInfo(const u32 *base, u32 index, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)base[3];
    u16 itemLen, optLen;
    int off = findIndexedArrayItem(src, base[4], 22, 42, index, &itemLen);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 84);
    out[0] = base[0];
    out[1] = base[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = itemLen;
    out[10] = (u32)(uintptr_t)p;
    out[11] = (u32)(uintptr_t)(p + 4);

    u16 tmp16;
    u32 tmp32;
    sceKernelMemcpy(&tmp16, p + 8, 2);
    *(u16 *)(out + 12) = bswap16(tmp16);
    sceKernelMemcpy(&tmp32, p + 10, 4);
    out[13] = bswap32(tmp32);
    sceKernelMemcpy(&tmp32, p + 14, 4);
    out[14] = bswap32(tmp32);
    sceKernelMemcpy(&tmp32, p + 18, 4);
    out[16] = bswap32(tmp32);
    sceKernelMemcpy(&tmp32, p + 22, 4);
    out[17] = bswap32(tmp32);
    sceKernelMemcpy(&tmp32, p + 26, 4);
    out[18] = bswap32(tmp32);

    const u8 *tail = p + 30;
    u32 rem = itemLen - 30u;
    out[5] = (u32)(uintptr_t)tail;

    int ret = sceNpCommerce2_3F6D413C(tail, rem, 21, NULL, out + 15);
    if (ret < 0)
        return ret;

    out[19] = out[1] + 4;
    ret = sceNpCommerce2_E3455E27(tail, rem, 125, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out[19] = (u32)(uintptr_t)(tail + ret);
    if (ret < 0)
        return ret;

    out[20] = out[1] + 4;
    ret = sceNpCommerce2_E3455E27(tail, rem, 126, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out[20] = (u32)(uintptr_t)(tail + ret);
    return ret;
}

static int parseWalletInfo(const u32 *base, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)base[3];
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, base[4], 87, 4, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 44);
    out[0] = base[0];
    out[1] = base[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[5] = (u32)(uintptr_t)(p + 4);
    u32 tmp;
    sceKernelMemcpy(&tmp, p, 4);
    out[10] = (bswap32(tmp) != 0);
    return off;
}

static int parseTopupAmount(const u32 *base, u32 index, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)base[5];
    u32 rem = base[4] - (u32)(base[5] - base[3]);
    u16 len;
    int off = findIndexedArrayItem(src, rem, 21, 4, index, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 44);
    out[0] = base[0];
    out[1] = base[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    u32 tmp;
    sceKernelMemcpy(&tmp, p, 4);
    out[10] = bswap32(tmp);
    return off;
}

static int parseContentRatingDescriptor(const u32 *base, u32 index, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)base[5];
    u32 rem = base[4] - (u32)(base[5] - base[3]);
    u16 len, optLen;
    int off = findIndexedArrayItem(src, rem, 23, 8, index, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 52);
    out[0] = base[0];
    out[1] = base[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[11] = base[1] + 4;
    out[12] = base[1] + 4;

    u32 type;
    sceKernelMemcpy(&type, p, 4);
    type = bswap32(type);
    out[10] = type;
    const u8 *tail = p + 4;
    out[5] = (u32)(uintptr_t)tail;

    if (type == 1) {
        int ret = sceNpCommerce2_E3455E27(tail, len - 4u, 16, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out[11] = (u32)(uintptr_t)(tail + ret);
        return ret;
    }
    if (type == 2) {
        int ret = sceNpCommerce2_E3455E27(tail, len - 4u, 14, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out[12] = (u32)(uintptr_t)(tail + ret);
        return ret;
    }
    return off;
}

static int parseCategoryInfoBody(const u8 *p, u32 len, u32 bufBase, u32 ver, int hasCounts, u32 *out) {
    u16 optLen;
    sceKernelMemset(out, 0, 80);
    out[0] = ver;
    out[1] = bufBase;
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[10] = (u32)hasCounts;
    out[11] = (u32)(uintptr_t)p;

    u32 rawHi, rawLo;
    sceKernelMemcpy(&rawHi, p + 57, 4);
    sceKernelMemcpy(&rawLo, p + 61, 4);
    u64 epochMs = ((u64)bswap32(rawHi) << 32) | bswap32(rawLo);
    if (epochMs != 0) {
        u64 tick = epochMs * 1000ULL + 62135596800000000ULL;
        out[12] = (u32)tick;
        out[13] = (u32)(tick >> 32);
    } else {
        sceKernelMemset(out + 12, 0, 8);
    }

    const u8 *tail = p + 65;
    if (hasCounts == 1) {
        u32 tmp;
        sceKernelMemcpy(&tmp, p + 65, 4);
        out[18] = bswap32(tmp);
        sceKernelMemcpy(&tmp, p + 69, 4);
        out[19] = bswap32(tmp);
        tail = p + 73;
    }

    out[5] = (u32)(uintptr_t)tail;
    u32 rem = len - (u32)(tail - p);
    out[14] = bufBase + 4;
    int ret = sceNpCommerce2_E3455E27(tail, rem, 6, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out[14] = (u32)(uintptr_t)(tail + ret);

    out[15] = bufBase + 4;
    ret = sceNpCommerce2_E3455E27(tail, rem, 11, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out[15] = (u32)(uintptr_t)(tail + ret);

    out[16] = bufBase + 4;
    ret = sceNpCommerce2_E3455E27(tail, rem, 16, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out[16] = (u32)(uintptr_t)(tail + ret);

    out[17] = bufBase + 4;
    ret = sceNpCommerce2_E3455E27(tail, rem, 4, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out[17] = (u32)(uintptr_t)(tail + ret);
    return 0;
}

static int parseGameProductInfoBody(const u8 *p, u32 len, u32 bufBase, u32 ver, int isDetailed, u32 *out) {
    u8 hdrCopy[56];
    u16 optLen;
    sceKernelMemcpy(hdrCopy, (const void *)(uintptr_t)bufBase, 56);
    int isSdk603 = ((u32)sceKernelGetCompiledSdkVersion() > SDK_VER_603);
    sceKernelMemset(out, 0, isSdk603 ? 128 : 88);

    out[0] = ver;
    out[1] = bufBase;
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[10] = (u32)isDetailed;
    out[11] = (u32)(uintptr_t)p;

    u32 tmp;
    sceKernelMemcpy(&tmp, p + 49, 4);
    out[12] = bswap32(tmp);

    u32 rawHi, rawLo;
    sceKernelMemcpy(&rawHi, p + 53, 4);
    sceKernelMemcpy(&rawLo, p + 57, 4);
    u64 epochMs = ((u64)bswap32(rawHi) << 32) | bswap32(rawLo);
    if (epochMs != 0) {
        u64 tick = epochMs * 1000ULL + 62135596800000000ULL;
        out[14] = (u32)tick;
        out[15] = (u32)(tick >> 32);
    } else {
        sceKernelMemset(out + 14, 0, 8);
    }

    sceKernelMemcpy(&tmp, p + 73, 4);
    u32 flags = bswap32(tmp);
    if (isSdk603) {
        if ((s32)flags < 0)
            out[22] |= 1u;
        if (flags & 0x40000000u)
            out[22] |= 2u;
        if (flags & 0x20000000u)
            out[22] |= 4u;
    }

    const u8 *tail = p + 77;
    u32 rem = (u16)(len - 77);
    out[5] = (u32)(uintptr_t)tail;

    out[16] = bufBase + 4;
    int ret = sceNpCommerce2_E3455E27(tail, rem, 7, 0, &optLen);
    if (ret >= 0 && optLen != 0) out[16] = (u32)(uintptr_t)(tail + ret);
    if (ret >= 0) {
        out[17] = bufBase + 4;
        ret = sceNpCommerce2_E3455E27(tail, rem, 12, 0, &optLen);
        if (ret >= 0 && optLen != 0) out[17] = (u32)(uintptr_t)(tail + ret);
        if (ret >= 0) {
            out[18] = bufBase + 4;
            ret = sceNpCommerce2_E3455E27(tail, rem, 16, 0, &optLen);
            if (ret >= 0 && optLen != 0) out[18] = (u32)(uintptr_t)(tail + ret);
            if (ret >= 0) {
                out[19] = bufBase + 4;
                ret = sceNpCommerce2_E3455E27(tail, rem, 4, 0, &optLen);
                if (ret >= 0 && optLen != 0) out[19] = (u32)(uintptr_t)(tail + ret);
            }
        }
    }

    if (ret >= 0 && isDetailed == 1) {
        u32 fullRem = len - 77u;
        out[20] = bufBase + 4;
        ret = sceNpCommerce2_E3455E27(tail, fullRem, 13, 0, &optLen);
        if (ret >= 0 && optLen != 0) out[20] = (u32)(uintptr_t)(tail + ret);
        if (ret >= 0) {
            out[21] = bufBase + 4;
            ret = sceNpCommerce2_E3455E27(tail, fullRem, 119, 0, &optLen);
            if (ret >= 0 && optLen != 0) out[21] = (u32)(uintptr_t)(tail + ret);
            if (ret >= 0 || ret == SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
                return 0;
        }
    }
    return ret;
}

static int parseContentRatingInfoBody(const u8 *p, u32 len, u32 bufBase, u32 ver, u32 *out) {
    u16 optLen;
    sceKernelMemset(out, 0, 52);
    out[0] = ver;
    out[1] = bufBase;
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[10] = (u32)(uintptr_t)p;
    out[11] = bufBase + 4;
    const u8 *tail = p + 17;
    out[5] = (u32)(uintptr_t)tail;

    int ret = sceNpCommerce2_E3455E27(tail, len - 17u, 16, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out[11] = (u32)(uintptr_t)(tail + ret);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_3F6D413C(tail, len - 17u, 23, NULL, out + 12);
}

typedef struct SkuParseSpec {
    const void *hdrCopy;    /* +0x00 */
    u32 flagMask;           /* +0x04 */
    u32 *outFlags;          /* +0x08 */
    u32 *outSkuId;          /* +0x0C */
    u32 *outSkuType;        /* +0x10 */
    u32 *outCountUntilAuto; /* +0x14 */
    u32 *outTimeUntilAuto;  /* +0x18 */
    u32 *outPurchasability; /* +0x1C */
    u32 *outAnnot;          /* +0x20 */
    u32 *outDownloadable;   /* +0x24 */
    u32 *outPrice;          /* +0x28 */
    u32 *outUnused11;       /* +0x2C */
    u32 *outUnused12;       /* +0x30 */
    u32 *outSalePrice;      /* +0x34 */
    u32 *outSaleType;       /* +0x38 */
} SkuParseSpec;

static int parseSkuFixedFields(const u8 *p, const SkuParseSpec *spec, int isOldSdk) {
    u32 tmp;
    u32 extraFlag = 0;
    const u8 *cur = p + 61;

    *spec->outSkuId = (u32)(uintptr_t)p;
    sceKernelMemcpy(&tmp, p + 57, 4);
    tmp = bswap32(tmp);
    *spec->outSkuType = (tmp == 2) ? 2u : 1u;

    sceKernelMemcpy(&tmp, cur, 4);
    *spec->outCountUntilAuto = bswap32(tmp);
    cur += 4;

    sceKernelMemcpy(&tmp, cur, 4);
    *spec->outTimeUntilAuto = bswap32(tmp);
    cur += 4;

    if (spec->outPurchasability != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outPurchasability = bswap32(tmp);
    }
    cur += 4;

    if (spec->outAnnot != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outAnnot = bswap32(tmp);
    }
    cur += 4;

    sceKernelMemcpy(&tmp, cur, 4);
    *spec->outDownloadable = (bswap32(tmp) != 0);
    cur += 4;

    if (spec->flagMask & 4u) {
        sceKernelMemcpy(&tmp, cur, 4);
        extraFlag = bswap32(tmp);
    }
    cur += 4;

    if (spec->outPrice != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outPrice = bswap32(tmp);
    }
    cur += 4;

    if (spec->outUnused11 != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outUnused11 = bswap32(tmp);
    }
    cur += 4;

    if (spec->outUnused12 != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outUnused12 = bswap32(tmp);
    }
    cur += 4;

    if (!isOldSdk && spec->outSalePrice != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outSalePrice = bswap32(tmp);
    }
    cur += 4;

    if (!isOldSdk && spec->outSaleType != NULL) {
        sceKernelMemcpy(&tmp, cur, 4);
        *spec->outSaleType = bswap32(tmp);
    }
    cur += 4;

    if (!isOldSdk) {
        if (spec->outPurchasability != NULL && *spec->outPurchasability != 0)
            *spec->outFlags |= 1u;
        if (spec->outDownloadable != NULL && *spec->outDownloadable != 0)
            *spec->outFlags |= 2u;
        if (extraFlag != 0)
            *spec->outFlags |= 4u;
    }
    return (int)(cur - p);
}

static int parseGameSkuInfoBody(const u8 *p, u32 len, u32 bufBase, u32 ver, int isDetailed, u32 *out) {
    u8 hdrCopy[56];
    SkuParseSpec spec;
    u16 optLen;
    int isSdk600 = ((u32)sceKernelGetCompiledSdkVersion() > SDK_VER_600);

    sceKernelMemcpy(hdrCopy, (const void *)(uintptr_t)bufBase, 56);
    sceKernelMemset(out, 0, isSdk600 ? 124 : 88);
    out[0] = ver;
    out[1] = bufBase;
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[10] = (u32)isDetailed;

    sceKernelMemset(&spec, 0, sizeof(spec));
    spec.hdrCopy           = hdrCopy;
    spec.flagMask          = 7;
    spec.outFlags          = out + 23;
    spec.outSkuId          = out + 11;
    spec.outSkuType        = out + 12;
    spec.outCountUntilAuto = out + 13;
    spec.outTimeUntilAuto  = out + 14;
    spec.outPurchasability = out + 15;
    spec.outAnnot          = out + 16;
    spec.outDownloadable   = out + 17;
    spec.outPrice          = out + 18;
    spec.outSalePrice      = out + 24;
    spec.outSaleType       = out + 25;

    int fixedLen = parseSkuFixedFields(p, &spec, !isSdk600);
    if (fixedLen < 0)
        return fixedLen;

    const u8 *tail = p + fixedLen + 36;
    u32 rem = len - (u32)(tail - p);
    out[5] = (u32)(uintptr_t)tail;

    if (isDetailed == 0) {
        out[19] = bufBase + 4;
        int ret = sceNpCommerce2_E3455E27(tail, rem, 8, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out[19] = (u32)(uintptr_t)(tail + ret);
        if (ret < 0)
            return ret;
    } else if (isDetailed == 1) {
        u32 skuType = out[12];
        u32 rem16 = (u16)rem;
        out[19] = bufBase + 4;
        int ret = sceNpCommerce2_E3455E27(tail, rem16, 8, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out[19] = (u32)(uintptr_t)(tail + ret);
        if (ret < 0)
            return ret;

        out[20] = bufBase + 4;
        ret = sceNpCommerce2_E3455E27(tail, rem16, 56, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out[20] = (u32)(uintptr_t)(tail + ret);
        if (ret < 0)
            return ret;

        out[21] = bufBase + 4;
        if (skuType == 2) {
            ret = sceNpCommerce2_E3455E27(tail, rem16, 18, 0, &optLen);
            if (ret >= 0 && optLen != 0)
                out[21] = (u32)(uintptr_t)(tail + ret);
            if (ret < 0)
                return ret;
        }
    }

    if (!isSdk600)
        return 0;

    int ret = sceNpCommerce2_3F6D413C(tail, rem, 128, NULL, out + 22);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    ret = sceNpCommerce2_3F6D413C(tail, rem, 149, NULL, out + 26);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    if (isDetailed == 1) {
        ret = sceNpCommerce2_3F6D413C(tail, rem, 156, NULL, out + 27);
        if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
            return ret;
    }
    return 0;
}

static int initCategoryContentsResult(u32 *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen, isTrunc;
    int off = parseResponseHeader(buf, bufSize, &ver, &bodyLen, &isTrunc);
    if (off < 0)
        return off;

    const u8 *body = buf + off;
    sceKernelMemset(out, 0, 52);
    out[0] = ver;
    out[1] = (u32)(uintptr_t)buf;
    out[2] = (u32)off + bodyLen;
    out[3] = (u32)(uintptr_t)body;
    out[4] = bodyLen;
    out[6] = isTrunc;

    int ret = sceNpCommerce2_69FEA1C8(body, bodyLen, 42, NULL, out + 10, (int)isTrunc);
    if (ret < 0)
        return ret;
    ret = clampRangeCount(out, 42, 4, out + 10);
    return (ret < 0) ? ret : 0;
}

static int parseCategoryInfoFromResult(const u32 *res, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)res[3];
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, res[4], 27, 89, &len);
    if (off < 0)
        return off;
    return parseCategoryInfoBody(src + off, len, res[1], res[0], 1, out);
}

static int parseContentInfo(const u32 *res, u32 index, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)res[3];
    u16 len;
    int off = findIndexedRangeItem(src, res[4], 42, 4, index, &len, (int)res[6]);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 44);
    out[0] = res[0];
    out[1] = res[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[5] = (u32)(uintptr_t)(p + 4);
    u32 tmp;
    sceKernelMemcpy(&tmp, p, 4);
    out[10] = bswap32(tmp);
    return off;
}

static int parseCategoryInfoFromContent(const u32 *contentInfo, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)contentInfo[5];
    u32 rem = contentInfo[4] - (u32)(contentInfo[5] - contentInfo[3]);
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, rem, 26, 89, &len);
    if (off < 0)
        return off;
    return parseCategoryInfoBody(src + off, len, contentInfo[1], contentInfo[0], 0, out);
}

static int parseProductInfoFromContent(const u32 *contentInfo, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)contentInfo[5];
    u32 rem = contentInfo[4] - (u32)(contentInfo[5] - contentInfo[3]);
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, rem, 40, 4, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 44);
    out[0] = contentInfo[0];
    out[1] = contentInfo[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[5] = (u32)(uintptr_t)(p + 4);
    u32 tmp;
    sceKernelMemcpy(&tmp, p, 4);
    out[10] = bswap32(tmp);
    return off;
}

static int parseGameProductFromContentProduct(const u32 *prodInfo, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)prodInfo[5];
    u32 rem = prodInfo[4] - (u32)(prodInfo[5] - prodInfo[3]);
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, rem, 33, 81, &len);
    if (off < 0)
        return off;
    return parseGameProductInfoBody(src + off, len, prodInfo[1], prodInfo[0], 0, out);
}

static int parseRatingInfo(const u32 *parentInfo, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)parentInfo[5];
    u32 rem = parentInfo[4] - (u32)(parentInfo[5] - parentInfo[3]);
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, rem, 24, 12, &len);
    if (off < 0)
        return off;
    return parseContentRatingInfoBody(src + off, len, parentInfo[1], parentInfo[0], out);
}

static int initProductInfoResult(u32 *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    int off = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (off < 0)
        return off;

    const u8 *body = buf + off;
    sceKernelMemset(out, 0, 40);
    out[0] = ver;
    out[1] = (u32)(uintptr_t)buf;
    out[2] = (u32)off + bodyLen;
    out[3] = (u32)(uintptr_t)body;
    out[4] = bodyLen;
    return 0;
}

static int parseProductInfoFromResult(const u32 *res, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)res[3];
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, res[4], 41, 4, &len);
    if (off < 0)
        return off;

    const u8 *p = src + off;
    sceKernelMemset(out, 0, 44);
    out[0] = res[0];
    out[1] = res[1];
    out[3] = (u32)(uintptr_t)p;
    out[4] = len;
    out[5] = (u32)(uintptr_t)(p + 4);
    u32 tmp;
    sceKernelMemcpy(&tmp, p, 4);
    out[10] = bswap32(tmp);
    return off;
}

static int parseGameProductFromResultProduct(const u32 *prodInfo, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)prodInfo[5];
    u32 rem = prodInfo[4] - (u32)(prodInfo[5] - prodInfo[3]);
    u16 len;
    int off = sceNpCommerce2_E3455E27(src, rem, 34, 93, &len);
    if (off < 0)
        return off;
    return parseGameProductInfoBody(src + off, len, prodInfo[1], prodInfo[0], 1, out);
}

static int parseGameSkuFromProduct(const u32 *gameProd, u32 isDetailed, u32 index, u32 *out) {
    const u8 *src = (const u8 *)(uintptr_t)gameProd[5];
    u32 rem = gameProd[4] - (u32)(gameProd[5] - gameProd[3]);
    u16 len;
    int off;
    if (isDetailed == 1) {
        off = findIndexedArrayItem(src, rem, 29, 89, index, &len);
        if (off < 0)
            return off;
        return parseGameSkuInfoBody(src + off, len, gameProd[1], gameProd[0], 1, out);
    } else {
        off = sceNpCommerce2_E3455E27(src, rem, 28, 85, &len);
        if (off < 0)
            return off;
        return parseGameSkuInfoBody(src + off, len, gameProd[1], gameProd[0], 0, out);
    }
}

/* =========================================================================
 * Session Cache & Core Operations
 * ========================================================================= */

static int postProcessResponse(Commerce2Ctx *ctx, Commerce2Req *req, u8 *buf, u32 bufSize, u32 respSize) {
    u32 acctInfo[24];
    u32 sessRes[16];
    u16 action = req->action;

    if (action == 49 || action == 1 || action == 66) {
        int ret;
        if (action == 49)
            ret = parseSessionResultEx(sessRes, buf, respSize);
        else
            ret = parseCreateSessionResult(sessRes, buf, respSize);
        if (ret >= 0 && parseAccountInfo(sessRes, acctInfo) >= 0) {
            sceNet_lib_B5CE388A(ctx->country, (const char *)(uintptr_t)acctInfo[16], 4);
            ctx->country[3] = '\0';
            sceNet_lib_B5CE388A(ctx->lang, (const char *)(uintptr_t)acctInfo[17], 4);
            ctx->lang[3] = '\0';
            sceNpCommerce2_F8D175FA(buf, bufSize, ctx->country, ctx->lang);
        }
        action = req->action;
    }

    if (action == 49 || action == 1 || action == 29 || action == 61 || action == 66) {
        if (respSize <= 1024u) {
            ctx->sessionLen = respSize;
            sceKernelMemcpy(ctx->sessionBuf, buf, respSize);
        } else {
            ctx->sessionLen = 0;
        }
    }
    return 0;
}

static void refreshIdleConnection(Commerce2Ctx *ctx) {
    u64 now;
    if (sceNpCommerce2_FC47B63C(&now) < 0)
        return;
    u64 diff = now - ctx->lastActivityTime;
    if (diff > 9999999ULL)
        sceNpCommerce2_75DF99A9(ctx->httpTmplId, &ctx->httpConnId);
}

static int coreInit(const char *envStr, int envFlag) {
    int ret = setEnvString(envStr);
    if (ret >= 0) {
        ret = setEnvFlag(envFlag);
        if (ret >= 0) {
            sceKernelMemset(&g_ctxMgrDummy, 0, 4);
            return 0;
        }
    }
    return ret;
}

static int coreInitWrapper(const char *envStr, int envFlag) {
    if (g_coreInitialized != 0)
        return SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;
    int ret = coreInit(envStr, envFlag);
    if (ret < 0)
        return ret;
    g_coreInitialized = 1;
    return 0;
}

static int coreTermWrapper(void) {
    if (g_coreInitialized != 0) {
        sceNpCommerce2_8E80E661(&g_ctxMgrDummy);
        g_coreInitialized = 0;
    }
    return 0;
}

static int createCtxInternal(u32 version, u32 *ctxIdOut, int isInternal) {
    if (version >= 3u)
        return SCE_NP_COMMERCE2_ERROR_UNSUPPORTED_VERSION;
    if (ctxIdOut == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

    Commerce2Ctx *ctx = (Commerce2Ctx *)sceNetIfhandle_lib_15CFE3C0(sizeof(Commerce2Ctx));
    if (ctx == NULL)
        return SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY;

    sceKernelMemset(ctx, 0, sizeof(Commerce2Ctx));
    ctx->version = version;
    ctx->storeType = (ctx->isInternal == 1) ? 1u : 3u;

    int ret = sceNpCommerce2_CCEEA788(&ctx->httpTmplId, &ctx->httpConnId, isInternal);
    if (ret >= 0) {
        ret = sceNpCommerce2_2AFD0CC7(&g_ctxMgrDummy, ctx, ctxIdOut, isInternal);
        if (ret >= 0)
            return ret;
    }

    if (ctx->httpTmplId != 0)
        sceNpCommerce2_CBA487E9(ctx->httpTmplId, ctx->httpConnId);
    sceNetIfhandle_lib_76BAD213(ctx);
    return ret;
}

static int destroyCtxInternal(u32 ctxId) {
    Commerce2Ctx *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, ctxId, (void **)&ctx, 1);
    if (ret >= 0)
        ret = sceNpCommerce2_8139E653(&g_ctxMgrDummy, ctxId);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int createReqInternal(u32 ctxId, u16 action, u32 *reqIdOut) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, ctxId, (void **)&ctx, 1);
    if (ret >= 0) {
        req = (Commerce2Req *)sceNetIfhandle_lib_15CFE3C0(sizeof(Commerce2Req));
        if (req == NULL) {
            ret = SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(req, 0, sizeof(Commerce2Req));
            req->action = action;
            ret = sceNpCommerce2_06618C81(&g_ctxMgrDummy, req, ctxId, reqIdOut);
            if (ret < 0)
                sceNetIfhandle_lib_76BAD213(req);
        }
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int destroyReqInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret >= 0) {
        ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
        if (ret >= 0) {
            sceNpCommerce2_02C307C0(&g_ctxMgrDummy, reqId);
            ret = 0;
        }
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int abortReqInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret >= 0) {
        ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
        if (ret >= 0) {
            sceNpCommerce2_63B96D73(req->httpReqId);
            req->flags |= 2u;
            ret = 0;
        }
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int getResultInternal(u32 reqId, u8 *buf, u32 bufSize, u32 *fillSizeOut) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    u8 serverStatus = 0;
    int isTrunc = 0;
    int expired = 0;
    int ret;

    if (buf == NULL || bufSize == 0 || fillSizeOut == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

    ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret < 0)
        goto cleanup;
    ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
    if (ret < 0)
        goto cleanup;

    if ((req->action == 3 || req->action == 4) && ctx->isInternal != 1 && bufSize < 0x40000u) {
        ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
        goto cleanup;
    }

    ret = sceNpCommerce2_4D43B84D(req->httpReqId, &ctx->buildVersion);
    if (ret < 0)
        goto cleanup;

    int hdrLen = sceNpCommerce2_C27FEAB1(ctx, req->httpReqId, req->action, &serverStatus, buf, bufSize);
    if (hdrLen < 0) {
        ret = hdrLen;
        goto cleanup;
    }

    int bodyLen = sceNpCommerce2_D4C319C6(req->httpReqId, buf, buf + hdrLen, bufSize, &isTrunc);
    if (bodyLen < 0) {
        ret = bodyLen;
        goto cleanup;
    }

    if (isTrunc == 1) {
        u16 a = req->action;
        if (a != 3 && a != 5 && a != 7 && a != 53 && a != 54 && a != 55 && a != 76) {
            ret = SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
            goto cleanup;
        }
    }

    ret = sceNpCommerce2_ADAA054C(buf, bufSize, (u32)bodyLen, isTrunc);
    if (ret < 0)
        goto cleanup;

    *fillSizeOut = (u32)(hdrLen + bodyLen);
    if (serverStatus == 0) {
        postProcessResponse(ctx, req, buf, bufSize, *fillSizeOut);
        ret = 0;
        goto cleanup;
    }

    ret = SCE_NP_COMMERCE2_ERROR_SERVER_BASE | (u32)serverStatus;
    if (req->action == 38 && ret == (SCE_NP_COMMERCE2_ERROR_SERVER_BASE | 1)) {
        ret = SCE_NP_COMMERCE2_ERROR_SERVER_CODE_41;
        goto cleanup;
    }
    if ((req->action == 23 || req->action == 64) && ret == (SCE_NP_COMMERCE2_ERROR_SERVER_BASE | 1)) {
        ret = SCE_NP_COMMERCE2_ERROR_SERVER_CODE_2B;
        goto cleanup;
    }

    checkSessionExpiredError(buf, *fillSizeOut, &expired);
    if (expired == 1)
        ret = SCE_NP_COMMERCE2_ERROR_SESSION_EXPIRED;

cleanup:
    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int createSessionStartInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    char consoleId[80];
    char password[48];
    char username[80];
    int version;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_F0900551(ctx, &version);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_02CADA41(ctx, username, password);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_157FA8AA(consoleId, 65);
    if (ret < 0) goto cleanup;

    refreshIdleConnection(ctx);
    ret = sendCreateSessionHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                req->action, &req->httpReqId, (u32)version,
                                username, password, consoleId);
    if (ret >= 0)
        sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);

cleanup:
    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int deleteSessionStartInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
    if (ret < 0) goto cleanup;

    refreshIdleConnection(ctx);
    ret = sendDeleteSessionHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                req->action, &req->httpReqId);
    if (ret >= 0) {
        sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);
        ctx->sessionLen = 0;
    }

cleanup:
    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int getCategoryStartInternal(u32 reqId, const char *categoryId,
                                    const char *const *rewardIds, u32 rewardCount,
                                    u32 startPos, u32 maxCount) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
    if (ret < 0) goto cleanup;

    if (ctx->isInternal == 0 && maxCount >= 61u) {
        ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    refreshIdleConnection(ctx);
    ret = sendGetCategoryHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                              req->action, &req->httpReqId, categoryId,
                              rewardIds, rewardCount, startPos, maxCount);
    if (ret >= 0)
        sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);

cleanup:
    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int getProductStartInternal(u32 reqId, const char *categoryId, const char *productId,
                                   const char *const *rewardIds, u32 rewardCount) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, reqId >> 16, (void **)&ctx, 1);
    if (ret < 0) goto cleanup;
    ret = sceNpCommerce2_851E7844(&g_ctxMgrDummy, reqId, (void **)&req);
    if (ret < 0) goto cleanup;

    refreshIdleConnection(ctx);
    ret = sendGetProductHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                             req->action, &req->httpReqId, categoryId,
                             productId, rewardIds, rewardCount);
    if (ret >= 0)
        sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);

cleanup:
    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgrDummy, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

static int getCachedSessionInternal(u32 ctxId, void *buf, u32 bufSize, u32 *sizeOut) {
    Commerce2Ctx *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgrDummy, ctxId, (void **)&ctx, 1);
    if (ret >= 0) {
        if (ctx->sessionLen == 0) {
            ret = SCE_NP_COMMERCE2_ERROR_NO_CACHED_SESSION;
        } else if (bufSize < ctx->sessionLen) {
            ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
        } else {
            sceKernelMemcpy(buf, ctx->sessionBuf, ctx->sessionLen);
            *sizeOut = ctx->sessionLen;
        }
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgrDummy, ctx);
    return ret;
}

/* =========================================================================
 * 3DES Token Generator & 64-bit Formatting
 * ========================================================================= */

static u8 hexNibble(char c) {
    if (c >= 'a' && c <= 'f') return (u8)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (u8)(c - 'A' + 10);
    return (u8)(c - '0');
}

static void desCbcEncrypt32(const u32 *rk, const u8 iv[8], const u8 in[32], u8 out[32]) {
    u8 state[8];
    u32 blk, i;
    for (i = 0; i < 8; i++)
        state[i] = iv[i];

    for (blk = 0; blk < 32; blk += 8) {
        u8 xbuf[8];
        for (i = 0; i < 8; i++)
            xbuf[7 - i] = in[blk + i] ^ state[i];
        u64 val;
        sceKernelMemcpy(&val, xbuf, 8);
        val = desEncryptBlock(rk, val);
        sceKernelMemcpy(xbuf, &val, 8);
        for (i = 0; i < 8; i++) {
            state[i] = xbuf[7 - i];
            out[blk + i] = state[i];
        }
    }
}

int sceNpCommerce2_EE7255FE(int unused, u8 *out32) {
    u8 plain[32];
    u8 mid[32];
    char consoleIdStr[80];
    u32 rk1[36], rk2[36], rk3[36];
    u64 tick, accountId;
    int ret;
    u32 i;
    (void)unused;

    sceKernelMemset(plain, 0, sizeof(plain));
    ret = sceRtcGetCurrentTick(&tick);
    if (ret < 0)
        return ret;

    u64 ms = (tick - 62135596800000000ULL) / 1000ULL;
    u32 msBe[2];
    msBe[0] = bswap32((u32)(ms >> 32));
    msBe[1] = bswap32((u32)ms);
    sceKernelMemcpy(plain, msBe, 8);

    if (sceNp_B819A0C8(NULL, NULL, &accountId) < 0)
        accountId = 0;
    u32 acctBe[2];
    acctBe[0] = bswap32((u32)(accountId >> 32));
    acctBe[1] = bswap32((u32)accountId);
    sceKernelMemcpy(plain + 8, acctBe, 8);

    ret = sceNpCore_E7AED5A3(consoleIdStr, 65);
    if (ret < 0)
        return SCE_NP_COMMERCE2_ERROR_CONSOLE_ID;

    for (i = 0; i < 16; i++)
        plain[16 + i] = (u8)((hexNibble(consoleIdStr[2 * i]) << 4) | hexNibble(consoleIdStr[2 * i + 1]));

    if (desKeySchedule(rk1, g_3desKey1Lo, g_3desKey1Hi) >= 0)
        desCbcEncrypt32(rk1, g_3desIv, plain, out32);
    if (desKeySchedule(rk2, g_3desKey2Lo, g_3desKey2Hi) >= 0)
        desCbcEncrypt32(rk2, g_3desIv, out32, mid);
    if (desKeySchedule(rk3, g_3desKey3Lo, g_3desKey3Hi) >= 0)
        desCbcEncrypt32(rk3, g_3desIv, mid, out32);

    return ret;
}

int sceNpCommerce2_22CEC5E2(char *out, int outSize, u32 lo, u32 hi, int isDecimal) {
    (void)outSize;
    if (isDecimal != 1) {
        return sceNet_lib_CF705E46(
            out, "%02x%02x%02x%02x%02x%02x%02x%02x",
            (hi >> 24) & 0xFFu, (hi >> 16) & 0xFFu, (hi >> 8) & 0xFFu, hi & 0xFFu,
            (lo >> 24) & 0xFFu, (lo >> 16) & 0xFFu, (lo >> 8) & 0xFFu, lo & 0xFFu);
    }

    u64 val = ((u64)hi << 32) | lo;
    u64 div = 10000000000000000000ULL;
    int started = 0;
    int digits = 19;
    u32 acc = 0;
    char *p = out;

    do {
        u32 q = (u32)(val / div);
        u32 d = q - acc;
        if (started || d != 0) {
            *p++ = (char)(d + '0');
            started = 1;
        }
        div /= 10ULL;
        acc = 10u * (acc + d);
        digits--;
    } while (digits >= 0);
    *p = '\0';
    return 0;
}

/* =========================================================================
 * Public API Exports (with SDK version & thread stack checks)
 * ========================================================================= */

#define CHECK_STACK(minStack) \
    do { \
        if (sceKernelGetCompiledSdkVersion() > SDK_VER_STACK_CHECK && \
            _sceKernelCheckThreadStack() < (minStack)) \
            return SCE_NP_ERROR_OUT_OF_STACK; \
    } while (0)

int sceNpCommerce2Init(void) {
    char env[16];
    int ret;
    CHECK_STACK(4064);
    if (g_apiInitialized != 0)
        return SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;
    ret = sceNpCore_52440ABF(env);
    if (ret < 0) return ret;
    env[8] = '\0';
    int envFlag = sceNp_E24DA399();
    ret = coreInitWrapper(env, envFlag);
    if (ret < 0) return ret;
    g_apiInitialized = 1;
    return 0;
}

int sceNpCommerce2Term(void) {
    CHECK_STACK(4080);
    if (g_apiInitialized != 0) {
        coreTermWrapper();
        g_apiInitialized = 0;
    }
    return 0;
}

int sceNpCommerce2CreateCtx(u32 version, u32 *ctxId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return createCtxInternal(version, ctxId, 0);
}

int sceNpCommerce2_8026FA46(u32 version, u32 *ctxId) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return createCtxInternal(version, ctxId, 1);
}

int sceNpCommerce2_5BDD4913(void) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return sceNpCommerce2_8E80E661(&g_ctxMgrDummy);
}

int sceNpCommerce2DestroyCtx(u32 ctxId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return destroyCtxInternal(ctxId);
}

int sceNpCommerce2_C3A3EAED(u32 ctxId) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return destroyCtxInternal(ctxId);
}

int sceNpCommerce2DestroyReq(u32 reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return destroyReqInternal(reqId);
}

int sceNpCommerce2AbortReq(u32 reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return abortReqInternal(reqId);
}

int sceNpCommerce2CreateSessionCreateReq(u32 ctxId, u32 *reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return createReqInternal(ctxId, 1, reqId);
}

int sceNpCommerce2CreateSessionStart(u32 reqId) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return createSessionStartInternal(reqId);
}

static int getResultChecked(u32 reqId, void *buf, u32 bufSize, u32 *fillSize, int checkBuf) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (sceKernelGetCompiledSdkVersion() > SDK_VER_STACK_CHECK && checkBuf == 1 && bufSize < 0x40000u)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    return getResultInternal(reqId, (u8 *)buf, bufSize, fillSize);
}

int sceNpCommerce2CreateSessionGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return getResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2_48D77A66(void *res, const void *buf, u32 bufSize) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || bufSize == 0)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCreateSessionResult((u32 *)res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2_287B6FB3(void *res, u32 idx, void *info) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (res == NULL || info == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCurrencyInfo((const u32 *)res, idx, (u32 *)info);
}

int sceNpCommerce2_4DB0E04F(void *res, void *walletInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (res == NULL || walletInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseWalletInfo((const u32 *)res, (u32 *)walletInfo);
}

int sceNpCommerce2_6795EE19(void *currInfo, u32 idx, void *topupInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (currInfo == NULL || topupInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseTopupAmount((const u32 *)currInfo, idx, (u32 *)topupInfo);
}

int sceNpCommerce2_7B00DE40(u32 ctxId, u32 *reqId) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return createReqInternal(ctxId, 2, reqId);
}

int sceNpCommerce2_DA206690(u32 reqId) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return deleteSessionStartInternal(reqId);
}

int sceNpCommerce2_AED78BDE(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return getResultInternal(reqId, (u8 *)buf, bufSize, fillSize);
}

int sceNpCommerce2GetCategoryContentsCreateReq(u32 ctxId, u32 *reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return createReqInternal(ctxId, 3, reqId);
}

static int getCategoryStartWrapper(u32 reqId, const char *categoryId,
                                   const char *const *rewardIds, u32 rewardCount,
                                   u32 startPos, u32 maxCount, int isPublic) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (categoryId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1 && sceKernelGetCompiledSdkVersion() > SDK_VER_STACK_CHECK && maxCount >= 61u)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1)
        sceNpCommerce2_E3EC9E14("GetCategoryContents", &categoryId, 1);
    return getCategoryStartInternal(reqId, categoryId, rewardIds, rewardCount, startPos, maxCount);
}

int sceNpCommerce2GetCategoryContentsStart(u32 reqId, const char *categoryId, u32 startPos, u32 maxCount) {
    return getCategoryStartWrapper(reqId, categoryId, NULL, 0, startPos, maxCount, 1);
}

int sceNpCommerce2GetCategoryContentsGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return getResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2InitGetCategoryContentsResult(void *res, const void *buf, u32 bufSize) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || bufSize == 0)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return initCategoryContentsResult((u32 *)res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2DestroyGetCategoryContentsResult(void) {
    CHECK_STACK(4096);
    return 0;
}

int sceNpCommerce2GetCategoryInfo(void *res, void *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (res == NULL || info == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCategoryInfoFromResult((const u32 *)res, (u32 *)info);
}

int sceNpCommerce2GetContentInfo(void *res, u32 index, void *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (res == NULL || info == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseContentInfo((const u32 *)res, index, (u32 *)info);
}

int sceNpCommerce2GetCategoryInfoFromContentInfo(void *contentInfo, void *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (contentInfo == NULL || info == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCategoryInfoFromContent((const u32 *)contentInfo, (u32 *)info);
}

int sceNpCommerce2_3D88DCCA(void *contentInfo, void *productInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (contentInfo == NULL || productInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseProductInfoFromContent((const u32 *)contentInfo, (u32 *)productInfo);
}

int sceNpCommerce2_1D9C32C0(void *productInfo, void *gameProductInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (productInfo == NULL || gameProductInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseGameProductFromContentProduct((const u32 *)productInfo, (u32 *)gameProductInfo);
}

int sceNpCommerce2GetContentRatingInfoFromCategoryInfo(void *catInfo, void *ratingInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (catInfo == NULL || ratingInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseRatingInfo((const u32 *)catInfo, (u32 *)ratingInfo);
}

int sceNpCommerce2_3755FC84(void *ratingInfo, u32 idx, void *desc) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (ratingInfo == NULL || desc == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseContentRatingDescriptor((const u32 *)ratingInfo, idx, (u32 *)desc);
}

int sceNpCommerce2GetContentRatingDescriptor(void *ratingInfo, u32 idx, void *desc) {
    CHECK_STACK(4080);
    return sceNpCommerce2_3755FC84(ratingInfo, idx, desc);
}

int sceNpCommerce2GetProductInfoCreateReq(u32 ctxId, u32 *reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return createReqInternal(ctxId, 4, reqId);
}

static int getProductStartWrapper(u32 reqId, const char *categoryId, const char *productId,
                                  const char *const *rewardIds, u32 rewardCount, int isPublic) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (productId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1)
        sceNpCommerce2_E3EC9E14("GetProductInfo", &productId, 1);
    return getProductStartInternal(reqId, categoryId, productId, rewardIds, rewardCount);
}

int sceNpCommerce2GetProductInfoStart(u32 reqId, const char *categoryId, const char *productId) {
    return getProductStartWrapper(reqId, categoryId, productId, NULL, 0, 1);
}

int sceNpCommerce2GetProductInfoGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return getResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2InitGetProductInfoResult(void *res, const void *buf, u32 bufSize) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || bufSize == 0)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return initProductInfoResult((u32 *)res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2DestroyGetProductInfoResult(void) {
    CHECK_STACK(4096);
    return 0;
}

int sceNpCommerce2_5C691546(void *res, void *productInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (res == NULL || productInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseProductInfoFromResult((const u32 *)res, (u32 *)productInfo);
}

int sceNpCommerce2_993C29DB(void *productInfo, void *gameProductInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (productInfo == NULL || gameProductInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseGameProductFromResultProduct((const u32 *)productInfo, (u32 *)gameProductInfo);
}

int sceNpCommerce2GetContentRatingInfoFromGameProductInfo(void *gameProd, void *ratingInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (gameProd == NULL || ratingInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseRatingInfo((const u32 *)gameProd, (u32 *)ratingInfo);
}

int sceNpCommerce2GetGameSkuInfoFromGameProductInfo(void *gameProd, u32 index, void *skuInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (gameProd == NULL || skuInfo == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    u32 *gp = (u32 *)gameProd;
    return parseGameSkuFromProduct(gp, gp[10], index, (u32 *)skuInfo);
}

int sceNpCommerce2_7EB30CAA(u32 ctxId, void *buf, u32 bufSize, u32 *sizeOut) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || bufSize == 0)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return getCachedSessionInternal(ctxId, buf, bufSize, sizeOut);
}

int sceNpCommerce2GetSessionInfo(u8 *sessionInfoOut, const void *buf, u32 bufSize) {
    u32 sessRes[12];
    u32 currInfo[24];
    int ret;
    CHECK_STACK(3936);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (sessionInfoOut == NULL || buf == NULL || bufSize == 0)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

    ret = sceNpCommerce2_48D77A66(sessRes, buf, bufSize);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_287B6FB3(sessRes, 0, currInfo);
    if (ret < 0) return ret;

    u32 sdkVer = (u32)sceKernelGetCompiledSdkVersion();
    sceKernelMemset(sessionInfoOut, 0, (sdkVer > SDK_VER_600) ? 48 : 8);
    sceKernelMemcpy(sessionInfoOut, (const void *)(uintptr_t)currInfo[10], 3);
    sessionInfoOut[3] = 0;
    *(u32 *)(sessionInfoOut + 4) = *(u16 *)(currInfo + 12);

    if (sdkVer > SDK_VER_600) {
        sceKernelMemcpy(sessionInfoOut + 8, (const void *)(uintptr_t)currInfo[11], 3);
        sessionInfoOut[11] = 0;
        *(u32 *)(sessionInfoOut + 12) = currInfo[16];
        *(u32 *)(sessionInfoOut + 16) = currInfo[17];
        sceKernelMemcpy(sessionInfoOut + 20, (const void *)(uintptr_t)currInfo[19], 4);
        sessionInfoOut[24] = 0;
        sceKernelMemcpy(sessionInfoOut + 25, (const void *)(uintptr_t)currInfo[20], 4);
        sessionInfoOut[29] = 0;
    }
    return 0;
}

int sceNpCommerce2GetGameProductInfoFromContentInfo(void *contentInfo, void *gameProd) {
    u32 prodInfo[12];
    int ret;
    CHECK_STACK(4032);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    ret = sceNpCommerce2_3D88DCCA(contentInfo, prodInfo);
    if (ret < 0) return ret;
    return sceNpCommerce2_1D9C32C0(prodInfo, gameProd);
}

int sceNpCommerce2GetGameProductInfo(void *res, void *gameProd) {
    u32 prodInfo[12];
    int ret;
    CHECK_STACK(4032);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    ret = sceNpCommerce2_5C691546(res, prodInfo);
    if (ret < 0) return ret;
    return sceNpCommerce2_993C29DB(prodInfo, gameProd);
}

static int appendStr(char **p, char *end, const char *s) {
    u32 len = sceNet_lib_D8722983(s);
    if (*p + len >= end)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    sceNet_lib_80C9F02A(*p, s);
    *p += len;
    return (int)len;
}

int sceNpCommerce2GetPrice(const u8 *sessionInfo, char *outBuf, u32 outBufSize, u32 price) {
    char tmp[32];
    char numStr[32];
    char *p = tmp;
    char *end = tmp + sizeof(tmp);
    int ret = 0;

    if (_sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_OUT_OF_STACK;
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;

    const char *currencySymbol = (const char *)sessionInfo;
    u32 decimals = *(const u32 *)(sessionInfo + 4);
    u32 symbolPos = *(const u32 *)(sessionInfo + 12);
    const char *thousandSep = (const char *)(sessionInfo + 20);
    const char *decimalSep = (const char *)(sessionInfo + 25);

    if (symbolPos == 0) {
        ret = appendStr(&p, end, currencySymbol);
        if (ret < 0) return ret;
        ret = appendStr(&p, end, " ");
        if (ret < 0) return ret;
    }

    sceNet_lib_CF705E46(numStr, "%u", price);
    numStr[31] = '\0';
    u32 numLen = sceNet_lib_D8722983(numStr);
    if (p + numLen + (numLen - 1) / 3u + 1 >= end)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;

    s32 intDigits = (s32)numLen - (s32)decimals;
    if (intDigits <= 0) {
        ret = appendStr(&p, end, "0");
        if (ret < 0) return ret;
    } else {
        u32 i;
        for (i = 0; i < (u32)intDigits; i++) {
            *p++ = numStr[i];
            *p = '\0';
            u32 rem = (u32)intDigits - i - 1;
            if (rem != 0 && (rem % 3u) == 0) {
                ret = appendStr(&p, end, thousandSep);
                if (ret < 0) return ret;
            }
        }
    }

    if (decimals != 0 && sceNet_lib_D8722983(decimalSep) != 0) {
        ret = appendStr(&p, end, decimalSep);
        if (ret < 0) return ret;
        if (intDigits > 0) {
            ret = appendStr(&p, end, numStr + intDigits);
            if (ret < 0) return ret;
        } else {
            u32 z;
            for (z = 0; z < (u32)(-intDigits); z++) {
                ret = appendStr(&p, end, "0");
                if (ret < 0) return ret;
            }
            ret = appendStr(&p, end, numStr);
            if (ret < 0) return ret;
        }
    }

    if (symbolPos == 1) {
        ret = appendStr(&p, end, " ");
        if (ret < 0) return ret;
        ret = appendStr(&p, end, currencySymbol);
        if (ret < 0) return ret;
    }

    if (sceNet_lib_D8722983(tmp) >= outBufSize)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    sceNet_lib_80C9F02A(outBuf, tmp);
    return 0;
}
