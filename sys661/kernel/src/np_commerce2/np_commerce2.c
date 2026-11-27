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

#define SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK          0x0505000F
#define SCE_NP_COMMERCE2_SDK_VER_600                  0x05FFFFFFu
#define SCE_NP_COMMERCE2_SDK_VER_603                  0x0602FFFFu

#define SCE_NP_COMMERCE2_RECV_BUF_SIZE                (256 * 1024)
#define SCE_NP_COMMERCE2_GETCAT_MAX_COUNT             60u

#define SCE_NP_COMMERCE2_SYM_POS_PRE                  0u
#define SCE_NP_COMMERCE2_SYM_POS_POST                 1u

#define SCE_NP_COMMERCE2_CONTENT_RATING_DESC_TYPE_ICON 1u
#define SCE_NP_COMMERCE2_CONTENT_RATING_DESC_TYPE_TEXT 2u

#define SCE_NP_COMMERCE2_CAT_DATA_TYPE_THIN           0
#define SCE_NP_COMMERCE2_CAT_DATA_TYPE_NORMAL         1
#define SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_THIN  0
#define SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_NORMAL 1
#define SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_THIN      0
#define SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL    1

#define SCE_NP_COMMERCE2_EPOCH_TICK_OFFSET            62135596800000000ULL

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
extern int   sceKernelCheckThreadStack(void);
extern u64   sceKernelGetSystemTimeWide(void);

/* =========================================================================
 * Data Structures (Sony Commerce2 SDK & Internal Types)
 * ========================================================================= */

typedef struct SceNpCommerce2ResponseHeader {
    u32 version;                /* +0x00 */
    char emptyStr[4];           /* +0x04 */
    u32 bodySize;               /* +0x08 */
    u32 isTruncated;            /* +0x0C */
    u32 buildVersion;           /* +0x10 */
    char country[4];            /* +0x14 */
    char lang[4];               /* +0x18 */
    char decimalLetter[4];      /* +0x1C */
    char thousandSeparator[4];  /* +0x20 */
    u32 reserved[5];            /* +0x24..+0x37 */
} SceNpCommerce2ResponseHeader; /* 56 bytes */

typedef struct SceNpCommerce2CommonData {
    u32 version;                /* +0x00 */
    const u8 *bufHead;          /* +0x04 */
    u32 bufSize;                /* +0x08 */
    const u8 *data;             /* +0x0C */
    u32 dataSize;               /* +0x10 */
    const u8 *subData;          /* +0x14 */
    u32 overflow;               /* +0x18 */
    u32 reserved[3];            /* +0x1C..+0x27 */
} SceNpCommerce2CommonData;     /* 40 bytes */

typedef struct SceNpCommerce2Range {
    u32 startPosition;          /* +0x00 */
    u32 count;                  /* +0x04 */
    u32 totalCountOfResults;    /* +0x08 */
} SceNpCommerce2Range;          /* 12 bytes */

typedef struct SceNpCommerce2CreateSessionResult {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 countOfCurrencyInfo;             /* +0x28 */
} SceNpCommerce2CreateSessionResult;     /* 44 bytes */

typedef struct SceNpCommerce2SessionResultEx {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 countOfCurrencyInfo;             /* +0x28 */
    u32 countOfServiceInfo;              /* +0x2C */
    u32 countOfPaymentMethod;            /* +0x30 */
    const char *noticeText;              /* +0x34 */
} SceNpCommerce2SessionResultEx;         /* 56 bytes */

typedef struct SceNpCommerce2AccountInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    char accountIdHex[20];               /* +0x28 */
    const char *onlineId;                /* +0x3C */
    const char *countryCode;             /* +0x40 */
    const char *langCode;                /* +0x44 */
    u32 walletBalance;                   /* +0x48 */
    const char *accountName;             /* +0x4C */
    u32 isSubAccount;                    /* +0x50 */
    u32 spendingLimit;                   /* +0x54 */
} SceNpCommerce2AccountInfo;             /* 88 bytes */

typedef struct SceNpCommerce2CurrencyInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    const char *currencyCode;            /* +0x28 */
    const char *currencySymbol;          /* +0x2C */
    u16 decimals;                        /* +0x30 */
    u16 pad;                             /* +0x32 */
    u32 minTopupAmount;                  /* +0x34 */
    u32 maxWalletBalance;                /* +0x38 */
    u32 countOfTopupAmount;              /* +0x3C */
    u32 symbolPosition;                  /* +0x40 */
    u32 symbolWithSpace;                 /* +0x44 */
    u32 currencyAttr;                    /* +0x48 */
    const char *thousandSeparator;       /* +0x4C */
    const char *decimalLetter;           /* +0x50 */
} SceNpCommerce2CurrencyInfo;            /* 84 bytes */

typedef struct SceNpCommerce2WalletInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 isCreditCardRegistered;          /* +0x28 */
} SceNpCommerce2WalletInfo;              /* 44 bytes */

typedef struct SceNpCommerce2TopupAmountInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 amount;                          /* +0x28 */
} SceNpCommerce2TopupAmountInfo;         /* 44 bytes */

typedef struct SceNpCommerce2ContentRatingDescriptor {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 descriptorType;                  /* +0x28 */
    const char *imageUrl;                /* +0x2C */
    const char *contentRatingDescription;/* +0x30 */
} SceNpCommerce2ContentRatingDescriptor; /* 52 bytes */

typedef struct SceNpCommerce2ContentRatingInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    const char *ratingSystemId;          /* +0x28 */
    const char *imageUrl;                /* +0x2C */
    u32 countOfContentRatingDescriptor;  /* +0x30 */
} SceNpCommerce2ContentRatingInfo;       /* 52 bytes */

typedef struct SceNpCommerce2CategoryInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    int dataType;                        /* +0x28 */
    const char *categoryId;              /* +0x2C */
    u64 releaseDate;                     /* +0x30 */
    const char *categoryName;            /* +0x38 */
    const char *categoryDescription;     /* +0x3C */
    const char *imageUrl;                /* +0x40 */
    const char *spName;                  /* +0x44 */
    u32 countOfSubCategory;              /* +0x48 */
    u32 countOfProduct;                  /* +0x4C */
} SceNpCommerce2CategoryInfo;            /* 80 bytes */

typedef struct SceNpCommerce2GameProductInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    int dataType;                        /* +0x28 */
    const char *productId;               /* +0x2C */
    u32 countOfSku;                      /* +0x30 */
    u32 pad;                             /* +0x34 */
    u64 releaseDate;                     /* +0x38 */
    const char *productName;             /* +0x40 */
    const char *productShortDescription; /* +0x44 */
    const char *imageUrl;                /* +0x48 */
    const char *spName;                  /* +0x4C */
    const char *productLongDescription;  /* +0x50 */
    const char *legalDescription;        /* +0x54 */
    u32 productAttr;                     /* +0x58 */
    u32 reserved[9];                     /* +0x5C..+0x7F */
} SceNpCommerce2GameProductInfo;         /* 128 bytes (88 bytes pre-6.03) */

typedef struct SceNpCommerce2GameSkuInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    int dataType;                        /* +0x28 */
    const char *skuId;                   /* +0x2C */
    u32 skuType;                         /* +0x30 */
    u32 countUntilExpiration;            /* +0x34 */
    u32 timeUntilExpiration;             /* +0x38 */
    u32 purchasabilityFlag;              /* +0x3C */
    u32 annotation;                      /* +0x40 */
    u32 downloadable;                    /* +0x44 */
    u32 price;                           /* +0x48 */
    const char *skuName;                 /* +0x4C */
    const char *productId;               /* +0x50 */
    const char *contentLinkUrl;          /* +0x54 */
    u32 countOfRewardInfo;               /* +0x58 */
    u32 skuAttr;                         /* +0x5C */
    u32 salesType;                       /* +0x60 */
    u32 firstPlayExpiration;             /* +0x64 */
    u32 countOfApplicableRewardInfo;     /* +0x68 */
    u32 countOfGameSkuEntitlementInfo;   /* +0x6C */
    u32 reserved[3];                     /* +0x70..+0x7B */
} SceNpCommerce2GameSkuInfo;             /* 124 bytes (88 bytes pre-6.00) */

typedef struct SceNpCommerce2GetCategoryContentsResult {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    SceNpCommerce2Range rangeOfContents; /* +0x28 */
} SceNpCommerce2GetCategoryContentsResult; /* 52 bytes */

typedef struct SceNpCommerce2GetProductInfoResult {
    SceNpCommerce2CommonData commonData; /* +0x00 */
} SceNpCommerce2GetProductInfoResult;    /* 40 bytes */

typedef struct SceNpCommerce2ContentInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 contentType;                     /* +0x28 */
} SceNpCommerce2ContentInfo;             /* 44 bytes */

typedef struct SceNpCommerce2ProductInfo {
    SceNpCommerce2CommonData commonData; /* +0x00 */
    u32 productType;                     /* +0x28 */
} SceNpCommerce2ProductInfo;             /* 44 bytes */

typedef struct SceNpCommerce2SessionInfo {
    char currencyCode[4];                /* +0x00 */
    u32 decimals;                        /* +0x04 */
    char currencySymbol[4];              /* +0x08 */
    u32 symbolPosition;                  /* +0x0C */
    u32 symbolWithSpace;                 /* +0x10 */
    char thousandSeparator[5];           /* +0x14 */
    char decimalLetter[5];               /* +0x19 */
    u8 padding[2];                       /* +0x1E */
    u32 reserved[4];                     /* +0x20..+0x2F */
} SceNpCommerce2SessionInfo;             /* 48 bytes (8 bytes pre-6.00) */

typedef struct Commerce2Req {
    struct Commerce2Req *next;           /* +0x00 */
    u32 refCount;                        /* +0x04 */
    u32 reqId;                           /* +0x08 */
    u32 flags;                           /* +0x0C: bit0=deleted, bit1=aborted */
    int httpReqId;                       /* +0x10 */
    u16 action;                          /* +0x14 */
    u16 pad;                             /* +0x16 */
} Commerce2Req; /* 24 bytes */

typedef struct Commerce2Ctx {
    struct Commerce2Ctx *next;           /* +0x00 */
    u32 refCount;                        /* +0x04 */
    u32 version;                         /* +0x08 */
    u32 ctxId;                           /* +0x0C */
    u32 flags;                           /* +0x10: bit0=deleted */
    u32 reserved;                        /* +0x14 */
    int httpTmplId;                      /* +0x18 */
    int httpConnId;                      /* +0x1C */
    Commerce2Req *reqList;               /* +0x20 */
    u8 sessionBuf[1024];                 /* +0x24 */
    u32 sessionLen;                      /* +0x424 */
    u32 storeType;                       /* +0x428 */
    u32 pad;                             /* +0x42C */
    u64 lastActivityTime;                /* +0x430 */
    u32 isInternal;                      /* +0x438 */
    u32 buildVersion;                    /* +0x43C */
    char country[4];                     /* +0x440 */
    char lang[4];                        /* +0x444 */
} Commerce2Ctx; /* 1096 bytes */

typedef struct Commerce2FormField {
    u32 len;
    const char *str;
} Commerce2FormField;

typedef struct SkuParseSpec {
    const void *hdrCopy;                 /* +0x00 */
    u32 flagMask;                        /* +0x04 */
    u32 *outFlags;                       /* +0x08 */
    const char **outSkuId;               /* +0x0C */
    u32 *outSkuType;                     /* +0x10 */
    u32 *outCountUntilExpiration;        /* +0x14 */
    u32 *outTimeUntilExpiration;         /* +0x18 */
    u32 *outPurchasability;              /* +0x1C */
    u32 *outAnnotation;                  /* +0x20 */
    u32 *outDownloadable;                /* +0x24 */
    u32 *outPrice;                       /* +0x28 */
    u32 *outRegularPrice;                /* +0x2C */
    u32 *outDiscountPrice;               /* +0x30 */
    u32 *outSalesType;                   /* +0x34 */
    u32 *outFirstPlayExpiration;         /* +0x38 */
} SkuParseSpec; /* 60 bytes */

typedef struct DesContext {
    u32 roundKeys[32];                   /* +0x00..+0x7F */
    u8 iv[8];                            /* +0x80..+0x87 */
    u32 pad[2];                          /* +0x88..+0x8F */
} DesContext; /* 144 bytes */

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
int sceNpCommerce2_69FEA1C8(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, SceNpCommerce2Range *rangeOut, int allowTrunc);
int sceNpCommerce2_E3455E27(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut);
int sceNpCommerce2_3F6D413C(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut);
int sceNpCommerce2_22CEC5E2(char *out, int outSize, u64 val, int isDecimal);
int sceNpCommerce2_460A1D92(char *out, u32 outSize);
int sceNpCommerce2_F0900551(void *ctx, int *versionOut);
int sceNpCommerce2_02CADA41(void *ctx, char *username, char *password);
int sceNpCommerce2_157FA8AA(char *consoleId, u32 size);
int sceNpCommerce2_B7911D82(void);
int sceNpCommerce2_FC47B63C(u64 *timeOut);
int sceNpCommerce2_48D77A66(SceNpCommerce2CreateSessionResult *res, const void *buf, u32 bufSize);
int sceNpCommerce2_287B6FB3(const SceNpCommerce2CreateSessionResult *res, u32 idx, SceNpCommerce2CurrencyInfo *info);
int sceNpCommerce2_3D88DCCA(const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2ProductInfo *productInfo);
int sceNpCommerce2_1D9C32C0(const SceNpCommerce2ProductInfo *productInfo, SceNpCommerce2GameProductInfo *gameProductInfo);
int sceNpCommerce2_5C691546(const SceNpCommerce2GetProductInfoResult *res, SceNpCommerce2ProductInfo *productInfo);
int sceNpCommerce2_993C29DB(const SceNpCommerce2ProductInfo *productInfo, SceNpCommerce2GameProductInfo *gameProductInfo);
int sceNpCommerce2_3755FC84(const SceNpCommerce2ContentRatingInfo *ratingInfo, u32 idx, SceNpCommerce2ContentRatingDescriptor *desc);

/* Inline helpers */
static __inline__ __attribute__((always_inline)) u16 bswap16(u16 val) {
    return (u16)((val >> 8) | (val << 8));
}

static __inline__ __attribute__((always_inline)) u32 bswap32(u32 val) {
    return ((val & 0x000000FFu) << 24) |
           ((val & 0x0000FF00u) <<  8) |
           ((val & 0x00FF0000u) >>  8) |
           ((val & 0xFF000000u) >> 24);
}

static __inline__ __attribute__((always_inline)) u32 rotr32(u32 val, u32 shift) {
    return (val >> shift) | (val << (32 - shift));
}

static __inline__ __attribute__((always_inline)) const char *emptyResponseStr(const u8 *bufHead) {
    return (const char *)(bufHead + 4);
}

static __inline__ __attribute__((always_inline)) void initCommonData(
    SceNpCommerce2CommonData *common, u32 version, const u8 *bufHead,
    u32 bufSize, const u8 *data, u32 dataSize) {
    sceKernelMemset(common, 0, sizeof(SceNpCommerce2CommonData));
    common->version = version;
    common->bufHead = bufHead;
    common->bufSize = bufSize;
    common->data = data;
    common->dataSize = dataSize;
}

static __inline__ __attribute__((always_inline)) void initChildCommonData(
    SceNpCommerce2CommonData *common, u32 version, const u8 *bufHead,
    const u8 *data, u32 dataSize) {
    sceKernelMemset(common, 0, sizeof(SceNpCommerce2CommonData));
    common->version = version;
    common->bufHead = bufHead;
    common->data = data;
    common->dataSize = dataSize;
}

static __inline__ __attribute__((always_inline)) const char *formatContentIdPrefixInline(
    const char *src, char *dst, u32 dstSize) {
    const char *cur = src;
    char ch;
    u32 len, maxLen;

    if (*cur != '\0' && *cur != '-') {
        cur++;
        while (*cur != '\0' && *cur != '-')
            cur++;
    }
    ch = *cur;
    if (ch != '\0') {
        cur++;
        ch = *cur;
    }
    if (ch != '\0' && ch != '-') {
        cur++;
        while (*cur != '\0' && *cur != '-')
            cur++;
    }
    len = (u32)(cur - src);
    maxLen = dstSize - 1;
    if (len > maxLen)
        len = maxLen;
    sceKernelMemcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* Global state (BSS) */
static u32 g_coreInitialized = 0;
static u32 g_apiInitialized = 0;
static Commerce2Ctx *g_ctxList = NULL;
static u32 g_ctxIdCounter = 0;
static u32 g_ctxMgr = 0;
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
static u64 g_tripleDesKeys[3] = {
    0xcdf8451a91fda7e0ULL,
    0xf4d56d929e34012cULL,
    0xfd9b80f14994516dULL,
};
static u8 g_tripleDesIv[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* =========================================================================
 * DES / 3DES implementation (used by sceNpCommerce2_EE7255FE)
 * ========================================================================= */

static __attribute__((noinline)) u64 desInitialPerm(u32 loWord, u32 hiWord) {
    u32 mix, left, right;
    mix = ((hiWord >> 4) ^ loWord) & 0x0F0F0F0Fu;
    loWord ^= mix;
    hiWord ^= (mix << 4);

    left = (hiWord & 0xFFFFu) | ((loWord & 0xFFFFu) << 16);
    right = (loWord & 0xFFFF0000u) | (hiWord >> 16);

    mix = ((right >> 2) ^ left) & 0x33333333u;
    left ^= mix;
    right ^= (mix << 2);

    mix = ((right >> 8) ^ left) & 0x00FF00FFu;
    left ^= mix;
    right ^= (mix << 8);

    mix = ((left >> 1) ^ right) & 0x55555555u;
    right ^= mix;
    left ^= (mix << 1);

    return ((u64)left << 32) | right;
}

static __attribute__((noinline)) u64 desFinalPerm(u32 loWord, u32 hiWord) {
    u32 mix, left, right;
    mix = ((hiWord >> 1) ^ loWord) & 0x55555555u;
    loWord ^= mix;
    hiWord ^= (mix << 1);

    mix = ((loWord >> 8) ^ hiWord) & 0x00FF00FFu;
    loWord ^= (mix << 8);
    hiWord ^= mix;

    mix = ((loWord >> 2) ^ hiWord) & 0x33333333u;
    loWord ^= (mix << 2);
    hiWord ^= mix;

    left = (loWord & 0xFFFF0000u) | (hiWord >> 16);
    right = (hiWord & 0xFFFFu) | ((loWord & 0xFFFFu) << 16);

    mix = ((right >> 4) ^ left) & 0x0F0F0F0Fu;
    left ^= mix;
    right ^= (mix << 4);

    return ((u64)right << 32) | left;
}

static __attribute__((noinline)) u32 desFeistel(u32 halfBlock, const u32 *roundKey) {
    u32 expLo = (rotr32(halfBlock, 31) & 0xFF00003Fu) |
                ((((halfBlock >> 3) & 0x3FFFu) << 10) & 0x00FC003Fu) |
                ((((halfBlock >> 3) & 0x03FFu) <<  8) & 0x0003F03Fu) |
                (((halfBlock >> 3) & 0x003Fu) << 6);
    u32 expHi = (rotr32(halfBlock, 9) & 0xFFFC0000u) |
                ((((halfBlock >> 15) & 0x3FFFu) << 4) & 0x0003F000u) |
                ((((halfBlock >> 15) & 0x03FFu) << 2) & 0x00000FC0u) |
                ((halfBlock >> 15) & 0x003Fu);
    u32 xorLo = expLo ^ roundKey[0];
    u32 xorHi = expHi ^ roundKey[1];

    u32 sboxLoA = g_desSbox[4 * ((xorLo >>  6) & 0x3F) + 3];
    u32 sboxHiA = g_desSbox[4 * ((xorHi >>  6) & 0x3F) + 0];
    u32 sboxHiB = g_desSbox[4 * ((xorHi >>  6) & 0x3F) + 1];
    u32 sboxHiC = g_desSbox[4 * ((xorHi >> 18) & 0x3F) + 0];
    u32 packed = (sboxLoA & 0xFFu) |
                 ((sboxHiA & 0xFFu) << 8) |
                 ((sboxHiB & 0xFFu) << 16) |
                 ((sboxHiC & 0xFFu) << 24);

    u8 sboxLoB = g_desSbox[4 * ((xorLo >> 12) & 0x3F) + 2];
    u8 sboxHiD = g_desSbox[4 * ((xorHi >>  0) & 0x3F) + 1];
    u8 sboxHiE = g_desSbox[4 * ((xorHi >> 12) & 0x3F) + 0];

    u32 subst = (packed & 0xF0F000F0u) |
                (((u32)g_desSbox[4 * ((xorLo >> 18) & 0x3F) + 2] << 8) & 0x0000F0FFu) |
                (g_desSbox[4 * (xorLo & 0x3F) + 3] & 0x0Fu) |
                (((u32)(sboxLoB & 0x0Fu)) << 8) |
                (((u32)(sboxHiD & 0x0Fu)) << 16) |
                (((u32)(sboxHiE & 0x0Fu)) << 24);

    return (rotr32(subst, 27) & 0x40402402u) |
           ((u32)sboxHiD << 31) |
           ((((subst >> 10) & 7u) << 27) & 0x30402402u) |
           (((subst & 0xFu) << 24) & 0x08402402u) |
           ((((subst >> 18) & 7u) << 24) & 0x04402402u) |
           ((((subst >>  3) & 3u) << 24) & 0x02402402u) |
           ((((u32)(u16)subst >> 14) << 23) & 0x01402402u) |
           ((subst >> 31) << 23) |
           (((u32)(sboxLoB & 3u) << 20) & 0x00202402u) |
           ((((subst >>  3) & 0xFu) << 17) & 0x00102402u) |
           (((u32)(sboxHiE & 0xFu) << 16) & 0x00082402u) |
           (((subst >> 14) & 1u) << 18) |
           (((subst & 3u) << 16) & 0x00022402u) |
           ((((subst >> 20) & 7u) << 14) & 0x00012402u) |
           ((((subst >> 29) & 3u) << 14) & 0x0000A402u) |
           (((u32)(sboxHiE & 1u)) << 14) |
           (((subst >> 18) & 1u) << 12) |
           ((subst & 1u) << 11) |
           (((subst >> 29) & 1u) << 9) |
           ((32u * ((subst >> 20) & 0xFu)) & 0x102u) |
           ((16u * ((subst >> 10) & 0xFu)) & 0x082u) |
           ((32u * ((subst >> 18) & 3u)) & 0x042u) |
           ((8u * (subst & 7u)) & 0x022u) |
           ((4u * (sboxHiE & 7u)) & 0x012u) |
           (((subst >> 10) & 1u) << 3) |
           (((subst >> 21) & 1u) << 2) |
           ((subst >> 7) & 1u);
}

static __attribute__((noinline)) int desKeySchedule(u32 *roundKeys, u64 key) {
    u32 keyLo = (u32)key;
    u32 keyHi = (u32)(key >> 32);
    u32 leftPerm = 0, rightPerm = 0;
    u32 bitPos = 57;

    do {
        u32 rotRight = rotr32(rightPerm, 31);
        leftPerm = ((leftPerm << 1) & 0xFFFFFFEFu) | ((rotRight & 1u) << 4);
        u32 keyBit = (bitPos & 0x20u) ? (keyHi >> (bitPos & 0x1Fu)) : (keyLo >> (bitPos & 0x1Fu));
        bitPos = (bitPos - 8 + (((bitPos - 8) & 0x40u) ? 1 : 0)) & 0x3Fu;
        rightPerm = (rotRight & 0xFFFFFFEFu) | ((keyBit & 1u) << 4);
    } while (bitPos != 0);

    u32 swappedLeft = bswap32(leftPerm);
    u32 leftHalf = ((swappedLeft & 0xF000000Fu) | ((swappedLeft & 0xFu) << 4)) >> 4;
    u32 rightHalf = rightPerm >> 4;

    u32 shifts = 0x6AAA9AA5u;
    while (shifts != 0) {
        u32 rotAmt = shifts & 3u;
        leftHalf = rotr32((leftHalf & 0x0FFFFFFFu) | ((leftHalf & 0xFFu) << 28), rotAmt);
        rightHalf = rotr32((rightHalf & 0x0FFFFFFFu) | ((rightHalf & 0xFFu) << 28), rotAmt);
        u32 subkeyLo = 0, subkeyHi = 0;
        int pcIdx = 24;
        while (pcIdx > 0) {
            pcIdx--;
            u16 pair = g_desPc2[pcIdx];
            subkeyLo = (subkeyLo << 1) | ((leftHalf >> (pair >> 8)) & 1u);
            subkeyHi = (subkeyHi << 1) | ((rightHalf >> (pair & 0xFFu)) & 1u);
        }
        roundKeys[0] = subkeyLo;
        roundKeys[1] = subkeyHi;
        roundKeys += 2;
        shifts >>= 2;
    }
    return 0;
}

static __attribute__((noinline)) u64 desEncryptBlock(const u32 *roundKeys, u64 block) {
    const u32 *end = roundKeys + 32;
    u64 ip = desInitialPerm((u32)block, (u32)(block >> 32));
    u32 right = (u32)ip;
    u32 left = (u32)(ip >> 32);
    do {
        left ^= desFeistel(right, roundKeys);
        right ^= desFeistel(left, roundKeys + 2);
        roundKeys += 4;
    } while (roundKeys != end);
    return desFinalPerm(left, right);
}

/* =========================================================================
 * Helper: format first two '-' segments of a content/category/product ID
 * ========================================================================= */

int sceNpCommerce2_4484DB5A(const char *src, char *dst, u32 dstSize) {
    const char *cur = src;
    char ch;
    u32 len, maxLen;

    if (*cur != '\0' && *cur != '-') {
        cur++;
        while (*cur != '\0' && *cur != '-')
            cur++;
    }
    ch = *cur;
    if (ch != '\0') {
        cur++;
        ch = *cur;
    }
    if (ch != '\0' && ch != '-') {
        cur++;
        while (*cur != '\0' && *cur != '-')
            cur++;
    }
    len = (u32)(cur - src);
    maxLen = dstSize - 1;
    if (len > maxLen)
        len = maxLen;
    sceKernelMemcpy(dst, src, len);
    dst[len] = '\0';
    return (int)(uintptr_t)dst;
}

int sceNpCommerce2_E3EC9E14(const char *tag, const char *const *ids, u32 count) {
    char formatted[32];
    u32 idx;

    if (!sceNp_2E4769C3())
        return 0;
    if (tag == NULL || ids == NULL || count == 0)
        return 0;

    formatContentIdPrefixInline(ids[0], formatted, sizeof(formatted));
    if (count < 2)
        return KprintfForUser("%s: %s\n", tag, formatted);

    KprintfForUser("%s: %s", tag, formatted);
    for (idx = 1; idx < count; idx++) {
        formatContentIdPrefixInline(ids[idx], formatted, sizeof(formatted));
        KprintfForUser(", %s", formatted);
    }
    return KprintfForUser("\n");
}

/* =========================================================================
 * Environment / misc helpers
 * ========================================================================= */

static __attribute__((noinline)) int setEnvString(const char *env) {
    if (env == NULL)
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
        if (countryOut != NULL)
            sceNet_lib_B5CE388A(countryOut, country, countrySize);
        if (langOut != NULL)
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

static __attribute__((noinline)) int setEnvFlag(int flag) {
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
    u32 idx;
    Commerce2Ctx *cur;
    int intr;

    intr = sceKernelCpuSuspendIntr();
    for (cur = g_ctxList; cur != NULL && count < 5; cur = cur->next)
        ids[count++] = cur->ctxId;
    sceKernelCpuResumeIntr(intr);

    for (idx = 0; idx < count; idx++) {
        sceNpCommerce2_73F9CD9E(mgr, ids[idx]);
        sceNpCommerce2_8139E653(mgr, ids[idx]);
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
                Commerce2Ctx *lookupCtx = NULL;
                int findRet = sceNpCommerce2_0A6AB496(mgr, ctxId, (void **)&lookupCtx, 0);
                if (findRet >= 0) {
                    Commerce2Req *iter = lookupCtx->reqList;
                    while (iter != NULL && (u16)iter->reqId != (u16)nextId)
                        iter = iter->next;
                    if (iter == NULL)
                        findRet = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
                }
                if (lookupCtx != NULL)
                    sceNpCommerce2_77BE411B(mgr, lookupCtx);
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
    u32 idx;
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

    for (idx = 0; idx < count; idx++)
        sceNpCommerce2_02C307C0(mgr, ids[idx]);
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

    do {
        if (enableCookie == 1) {
            ret = sceHttpEnableCookie(tmplId);
            if (ret < 0)
                break;
        }
        ret = sceHttpSetConnectTimeOut(tmplId, 20000000);
        if (ret < 0)
            break;
        ret = sceHttpSetRecvTimeOut(tmplId, 60000000);
        if (ret < 0)
            break;
        ret = sceHttpSetSendTimeOut(tmplId, 20000000);
        if (ret < 0)
            break;

        if (sceNpCommerce2_B7911D82() <= 0) {
            ret = sceHttp_87F1E666(tmplId, 357);
            if (ret < 0)
                break;
            ret = sceHttp_3C478044(tmplId, 26);
        } else {
            ret = sceHttp_87F1E666(tmplId, 64);
            if (ret < 0)
                break;
            ret = sceHttp_3C478044(tmplId, 1);
        }
        if (ret >= 0) {
            *connIdOut = 0;
            *tmplIdOut = tmplId;
            return ret;
        }
    } while (0);

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
    u32 idx;
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

    do {
        for (idx = 0; idx < 4; idx++) {
            ret = sceHttpAddExtraHeader(reqId, g_httpHeaders[2 * idx], g_httpHeaders[2 * idx + 1], 0);
            if (ret < 0)
                break;
        }
        if (ret < 0)
            break;
        if (cookie != NULL && sceNet_lib_D8722983(cookie) != 0) {
            ret = sceHttpAddExtraHeader(reqId, "Cookie", cookie, 0);
            if (ret < 0)
                break;
        }
        *reqIdOut = reqId;
        return ret;
    } while (0);

    if (reqId != 0)
        sceHttpDeleteRequest(reqId);
    return ret;
}

int sceNpCommerce2_4D43B84D(int httpReqId, u32 *buildVerInOut) {
    int statusCode;
    const char *allHdr;
    u32 allHdrLen = 0;
    const char *hdrVal;
    u32 hdrValLen;
    int ret;

    ret = sceHttpGetStatusCode(httpReqId, &statusCode);
    if (ret < 0)
        return ret;

    if (*buildVerInOut == 0) {
        if (sceHttpGetAllHeader(httpReqId, &allHdr, &allHdrLen) >= 0 &&
            sceParseHttpResponseHeader(allHdr, allHdrLen, "X-NativeBuildVersion", &hdrVal, &hdrValLen) >= 0) {
            if (hdrValLen >= sceNet_lib_D8722983("XX.XX.XX.XX")) {
                u32 ver = 0;
                const char *cur = hdrVal;
                if ((uintptr_t)hdrVal < (uintptr_t)0xFFFFFFF7u) {
                    do {
                        ver = 100u * ver + 10u * (u32)(u8)cur[0] + (u32)(u8)cur[1] - 528u;
                        cur += 3;
                    } while ((uintptr_t)cur <= (uintptr_t)hdrVal + 9u);
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
    u8 *cur = (u8 *)buf;
    u32 rem = size;
    u32 total = 0;
    int ret;

    while (rem != 0) {
        ret = sceHttpReadData(httpReqId, cur, rem);
        if (ret < 0)
            return ret;
        if (ret == 0)
            return (int)total;
        cur += ret;
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

static __attribute__((noinline)) int resolveActionPath(
    int storeType, u16 action, const char **dirOut, const char **nameOut) {
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

static __attribute__((noinline)) int sendUintFormField(
    int httpReqId, int fieldIdx, u32 val, int prependAmp) {
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

    int printed = sceNet_lib_CF705E46(numBuf, "%u", val);
    if (printed >= 16)
        return SCE_NP_COMMERCE2_ERROR_FORMAT;
    return sceNpCommerce2_AE614CC6(httpReqId, numBuf, sceNet_lib_D8722983(numBuf));
}

static __attribute__((noinline)) int buildOrSendCreateSessionBody(
    int sendMode, int httpReqId, u64 *lenOut,
    u32 version, const char *username,
    const char *password, const char *consoleId) {
    char escBuf[784];
    char numBuf[16];
    s64 total = 0;
    int ret;

    if (sendMode == 1) {
        ret = sendUintFormField(httpReqId, 2, version, 0);
    } else {
        int printed = sceNet_lib_CF705E46(numBuf, "%u", version);
        ret = (printed < 16) ? (int)(printed + g_formFields[2].len) : SCE_NP_COMMERCE2_ERROR_FORMAT;
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

static __attribute__((noinline)) int sendCreateSessionHttp(
    int storeType, int tmplId, int *connIdInOut, u16 action,
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

static __attribute__((noinline)) int sendDeleteSessionHttp(
    int storeType, int tmplId, int *connIdInOut, u16 action, int *httpReqIdInOut) {
    const char *dir = NULL;
    const char *name = NULL;
    int ret = resolveActionPath(storeType, action, &dir, &name);
    if (ret >= 0)
        ret = sceNpCommerce2_382803C8(tmplId, storeType, action, dir, name, NULL,
                                      connIdInOut, httpReqIdInOut, 0);
    if (ret >= 0) {
        int sendRet = sceNpCommerce2_AE614CC6(*httpReqIdInOut, NULL, 0);
        return (sendRet < 0) ? sendRet : 0;
    }
    return ret;
}

static __attribute__((noinline)) int buildOrSendGetCategoryBody(
    int sendMode, int httpReqId, u64 *lenOut,
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
        int printed = sceNet_lib_CF705E46(numBuf, "%u", startPos);
        ret = (printed < 16) ? (int)(printed + g_formFields[7].len + 1) : SCE_NP_COMMERCE2_ERROR_FORMAT;
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode == 1) {
        ret = sendUintFormField(httpReqId, 8, maxCount, 1);
    } else {
        int printed = sceNet_lib_CF705E46(numBuf, "%u", maxCount);
        ret = (printed < 16) ? (int)(printed + g_formFields[8].len + 1) : SCE_NP_COMMERCE2_ERROR_FORMAT;
    }
    if (ret < 0)
        return ret;
    total += ret;

    if (sendMode != 1)
        *lenOut = (u64)total;
    return 0;
}

static __attribute__((noinline)) int sendGetCategoryHttp(
    int storeType, int tmplId, int *connIdInOut, u16 action,
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

static __attribute__((noinline)) int buildOrSendGetProductBody(
    int sendMode, int httpReqId, u64 *lenOut,
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

static __attribute__((noinline)) int sendGetProductHttp(
    int storeType, int tmplId, int *connIdInOut, u16 action,
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
    SceNpCommerce2ResponseHeader header;
    u8 rawStatus[4];
    char hexDump[256];
    int ret;
    (void)action;

    if (bufSize < 60)
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;

    sceKernelMemset(&header, 0, sizeof(header));
    header.version = ctx->version;
    header.buildVersion = ctx->buildVersion;
    sceNet_lib_B5CE388A(header.country, ctx->country, 4);
    header.country[3] = '\0';
    sceNet_lib_B5CE388A(header.lang, ctx->lang, 4);
    header.lang[3] = '\0';
    sceNet_lib_B5CE388A(header.decimalLetter, ".", 4);
    header.decimalLetter[3] = '\0';
    sceNet_lib_B5CE388A(header.thousandSeparator, ",", 4);
    header.thousandSeparator[3] = '\0';
    sceKernelMemcpy(buf, &header, sizeof(header));

    ret = sceNpCommerce2_44B8A9A9(httpReqId, rawStatus, 4);
    if (ret < 0)
        return ret;
    if (ret < 4)
        return SCE_NP_COMMERCE2_ERROR_UNEXPECTED_EOF;

    {
        u32 idx = 0;
        do {
            u32 chunk = 4u - idx;
            u32 limit;
            char *outPtr = hexDump;
            if (chunk >= 129u)
                chunk = 128u;
            limit = idx + chunk;
            while (idx < limit) {
                u32 hiNib = rawStatus[idx] >> 4;
                u32 loNib = rawStatus[idx] & 0xFu;
                *outPtr++ = (char)(hiNib < 10u ? hiNib + '0' : hiNib + 'a' - 10u);
                *outPtr++ = (char)(loNib < 10u ? loNib + '0' : loNib + 'a' - 10u);
                idx++;
            }
            *outPtr = '\0';
        } while (idx < 4u);
        (void)hexDump;
    }

    *statusOut = rawStatus[1];
    rawStatus[2] = 0;
    rawStatus[3] = 0;
    sceKernelMemcpy(buf + 56, rawStatus, 4);
    return 60;
}

int sceNpCommerce2_ADAA054C(u8 *buf, u32 bufSize, u32 bodyLen, int isTruncated) {
    SceNpCommerce2ResponseHeader hdr;
    if (bufSize < sizeof(hdr))
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
    sceKernelMemcpy(&hdr, buf, sizeof(hdr));
    hdr.bodySize = bodyLen + 4;
    hdr.isTruncated = (u32)isTruncated;
    sceKernelMemcpy(buf, &hdr, sizeof(hdr));
    return 0;
}

int sceNpCommerce2_F8D175FA(u8 *buf, u32 bufSize, const char *country, const char *lang) {
    SceNpCommerce2ResponseHeader hdr;
    if (bufSize < sizeof(hdr))
        return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
    sceKernelMemcpy(&hdr, buf, sizeof(hdr));
    sceNet_lib_B5CE388A(hdr.country, country, 4);
    hdr.country[3] = '\0';
    sceNet_lib_B5CE388A(hdr.lang, lang, 4);
    hdr.lang[3] = '\0';
    sceKernelMemcpy(buf, &hdr, sizeof(hdr));
    return 0;
}

int sceNpCommerce2_FC687AC7(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 rawTag, rawLen, parsedTag, parsedLen;
        if (bufLen < (u32)(cur - buf) + 4u)
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;

        sceKernelMemcpy(&rawTag, cur, 2);
        parsedTag = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawLen, cur + 2, 2);
        parsedLen = bswap16(rawLen);
        cur += 4;

        if (parsedTag == tag) {
            if (bufLen < (u32)(cur - buf) + (u32)minLen)
                return SCE_NP_COMMERCE2_ERROR_TLV_SHORT_HEADER;
            if (parsedLen < minLen)
                return SCE_NP_COMMERCE2_ERROR_TLV_SHORT_MIN;
            if (bufLen < (u32)(cur - buf) + (u32)parsedLen && !allowTrunc)
                return SCE_NP_COMMERCE2_ERROR_TLV_TRUNCATED;
            if (valLenOut != NULL)
                *valLenOut = parsedLen;
            return (int)(cur - buf);
        }
        cur += parsedLen;
    }
}

int sceNpCommerce2_3546C737(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 blockLen, rawTag, rawCount;
        int foundOff;
        if (bufLen < (u32)(cur - buf))
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;
        foundOff = sceNpCommerce2_FC687AC7(cur, bufLen - (u32)(cur - buf), 1, 4, &blockLen, allowTrunc);
        if (foundOff < 0)
            return foundOff;
        const u8 *entry = cur + foundOff;
        sceKernelMemcpy(&rawTag, entry, 2);
        rawTag = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawCount, entry + 2, 2);
        rawCount = bswap16(rawCount);
        if (rawTag == tag) {
            if (valLenOut != NULL)
                *valLenOut = (u16)(blockLen - 4);
            if (countOut != NULL)
                *countOut = rawCount;
            return (int)((entry - buf) + 4);
        }
        cur = entry + blockLen;
    }
}

int sceNpCommerce2_69FEA1C8(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, SceNpCommerce2Range *rangeOut, int allowTrunc) {
    const u8 *cur = buf;
    while (1) {
        u16 blockLen, rawTag, rawCount, rawStart, rawTotal;
        int foundOff;
        if (bufLen < (u32)(cur - buf))
            return SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND;
        foundOff = sceNpCommerce2_FC687AC7(cur, bufLen - (u32)(cur - buf), 2, 8, &blockLen, allowTrunc);
        if (foundOff < 0)
            return foundOff;
        const u8 *entry = cur + foundOff;
        sceKernelMemcpy(&rawTag, entry, 2);
        rawTag = (u16)(bswap16(rawTag) & 0x0FFFu);
        sceKernelMemcpy(&rawCount, entry + 2, 2);
        rawCount = bswap16(rawCount);
        sceKernelMemcpy(&rawStart, entry + 4, 2);
        rawStart = bswap16(rawStart);
        sceKernelMemcpy(&rawTotal, entry + 6, 2);
        rawTotal = bswap16(rawTotal);
        if (rawTag == tag) {
            if (valLenOut != NULL)
                *valLenOut = (u16)(blockLen - 8);
            if (rangeOut != NULL) {
                sceKernelMemset(rangeOut, 0, sizeof(SceNpCommerce2Range));
                rangeOut->startPosition = rawStart;
                rangeOut->count = rawCount;
                rangeOut->totalCountOfResults = rawTotal;
            }
            return (int)((entry - buf) + 8);
        }
        cur = entry + blockLen;
    }
}

int sceNpCommerce2_E3455E27(const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u16 *valLenOut) {
    return sceNpCommerce2_FC687AC7(buf, bufLen, tag, minLen, valLenOut, 0);
}

int sceNpCommerce2_3F6D413C(const u8 *buf, u32 bufLen, u16 tag, u16 *valLenOut, u32 *countOut) {
    return sceNpCommerce2_3546C737(buf, bufLen, tag, valLenOut, countOut, 0);
}

static __attribute__((noinline)) int parseResponseHeader(
    const u8 *buf, u32 bufSize, u32 *versionOut, u32 *bodyLenOut, u32 *truncOut) {
    SceNpCommerce2ResponseHeader hdr;
    u8 status[4];

    if (bufSize < 60)
        return SCE_NP_COMMERCE2_ERROR_INVALID_RESPONSE;

    sceKernelMemcpy(&hdr, buf, sizeof(hdr));
    sceKernelMemcpy(status, buf + 56, 4);
    if (bufSize < hdr.bodySize + 56u)
        return SCE_NP_COMMERCE2_ERROR_INVALID_RESPONSE;

    *bodyLenOut = hdr.bodySize - 4u;
    if (versionOut != NULL)
        *versionOut = hdr.version;
    if (truncOut != NULL)
        *truncOut = hdr.isTruncated;
    return 60;
}

static __attribute__((noinline)) int findIndexedArrayItem(
    const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u32 index, u16 *itemLenOut) {
    u16 arrLen;
    u32 count;
    int startOff = sceNpCommerce2_3F6D413C(buf, bufLen, tag, &arrLen, &count);
    if (startOff < 0)
        return startOff;
    if (index >= count)
        return SCE_NP_COMMERCE2_ERROR_INVALID_INDEX;

    const u8 *arrStart = buf + startOff;
    const u8 *cur = arrStart;
    u16 itemLen = 0;
    u32 idx;
    for (idx = 0; idx <= index; idx++) {
        if (idx != 0)
            cur += itemLen;
        int itemOff = sceNpCommerce2_E3455E27(cur, arrLen - (u32)(cur - arrStart), tag, minLen, &itemLen);
        if (itemOff < 0)
            return itemOff;
        cur += itemOff;
    }
    *itemLenOut = itemLen;
    return (int)(cur - buf);
}

static __attribute__((noinline)) int findIndexedRangeItem(
    const u8 *buf, u32 bufLen, u16 tag, u16 minLen, u32 index, u16 *itemLenOut, int allowTrunc) {
    SceNpCommerce2Range range;
    int startOff = sceNpCommerce2_69FEA1C8(buf, bufLen, tag, NULL, &range, allowTrunc);
    if (startOff < 0)
        return startOff;
    if (index >= range.count)
        return SCE_NP_COMMERCE2_ERROR_INVALID_INDEX;

    const u8 *cur = buf + startOff;
    u16 itemLen = 0;
    u32 idx;
    for (idx = 0; idx <= index; idx++) {
        if (idx != 0)
            cur += itemLen;
        int itemOff = sceNpCommerce2_E3455E27(cur, bufLen - (u32)(cur - buf), tag, minLen, &itemLen);
        if (itemOff < 0)
            return itemOff;
        cur += itemOff;
    }
    *itemLenOut = itemLen;
    return (int)(cur - buf);
}

static __attribute__((noinline)) int clampRangeCount(
    const SceNpCommerce2CommonData *common, u16 tag, u16 minLen, SceNpCommerce2Range *range) {
    u16 itemLen;
    u32 idx;
    int ret = 0;
    for (idx = 0; idx < range->count; idx++) {
        ret = findIndexedRangeItem(common->data, common->dataSize, tag, minLen, idx, &itemLen, (int)common->overflow);
        if (ret < 0) {
            if (idx != 0) {
                range->count = idx;
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

static __attribute__((noinline)) int parseAccountInfo(
    const SceNpCommerce2CommonData *base, SceNpCommerce2AccountInfo *out) {
    const u8 *src = base->data;
    u16 len;
    u16 optLen;
    int foundOff = sceNpCommerce2_E3455E27(src, base->dataSize, 94, 50, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2AccountInfo));
    initChildCommonData(&out->commonData, base->version, base->bufHead, payload, len);

    u32 rawAcct[2];
    sceKernelMemcpy(rawAcct, payload, 8);
    u64 acctId = ((u64)bswap32(rawAcct[0]) << 32) | bswap32(rawAcct[1]);
    sceNpCommerce2_22CEC5E2(out->accountIdHex, 17, acctId, 0);
    out->onlineId = (const char *)(payload + 8);
    out->countryCode = (const char *)(payload + 25);
    out->langCode = (const char *)(payload + 28);

    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload + 34, 4);
    out->walletBalance = bswap32(rawWord);
    sceKernelMemcpy(&rawWord, payload + 38, 4);
    out->isSubAccount = (bswap32(rawWord) != 0);
    sceKernelMemcpy(&rawWord, payload + 42, 4);
    out->spendingLimit = bswap32(rawWord);

    const u8 *tail = payload + 46;
    out->commonData.subData = tail;
    out->accountName = emptyResponseStr(out->commonData.bufHead);
    foundOff = sceNpCommerce2_E3455E27(tail, len - (u32)(tail - payload), 93, 0, &optLen);
    if (foundOff >= 0 && optLen != 0)
        out->accountName = (const char *)(tail + foundOff);
    return foundOff;
}

static __attribute__((noinline)) int parseSessionResultEx(
    SceNpCommerce2SessionResultEx *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    u16 optLen;
    int hdrOff = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (hdrOff < 0)
        return hdrOff;

    const u8 *body = buf + hdrOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2SessionResultEx));
    initCommonData(&out->commonData, ver, buf, (u32)hdrOff + bodyLen, body, bodyLen);
    out->noticeText = emptyResponseStr(buf);

    int ret = sceNpCommerce2_E3455E27(body, bodyLen, 164, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out->noticeText = (const char *)(body + ret);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    if (ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return 0;

    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 22, NULL, &out->countOfCurrencyInfo);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 100, NULL, &out->countOfServiceInfo);
    if (ret < 0)
        return ret;
    ret = sceNpCommerce2_3F6D413C(body, bodyLen, 88, NULL, &out->countOfPaymentMethod);
    if (ret < 0)
        return ret;
    return 0;
}

static __attribute__((noinline)) int checkSessionExpiredError(
    const u8 *buf, u32 bufSize, int *expiredOut) {
    u32 ver, bodyLen;
    u32 count = 0;
    int hdrOff = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (hdrOff < 0)
        return hdrOff;
    *expiredOut = (sceNpCommerce2_3F6D413C(buf + hdrOff, bodyLen, 120, NULL, &count) >= 0) ? 1 : 0;
    return 0;
}

static __attribute__((noinline)) int parseCreateSessionResult(
    SceNpCommerce2CreateSessionResult *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    int hdrOff = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (hdrOff < 0)
        return hdrOff;

    const u8 *body = buf + hdrOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2CreateSessionResult));
    initCommonData(&out->commonData, ver, buf, (u32)hdrOff + bodyLen, body, bodyLen);

    int ret = sceNpCommerce2_3F6D413C(body, bodyLen, 22, NULL, &out->countOfCurrencyInfo);
    return (ret < 0) ? ret : 0;
}

static __attribute__((noinline)) int parseCurrencyInfo(
    const SceNpCommerce2CreateSessionResult *res, u32 index, SceNpCommerce2CurrencyInfo *out) {
    const u8 *src = res->commonData.data;
    u16 itemLen, thousandLen, decimalLen;
    int foundOff = findIndexedArrayItem(src, res->commonData.dataSize, 22, 42, index, &itemLen);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2CurrencyInfo));
    initChildCommonData(&out->commonData, res->commonData.version, res->commonData.bufHead, payload, itemLen);
    out->currencyCode = (const char *)payload;
    out->currencySymbol = (const char *)(payload + 4);

    u16 rawDecimals;
    u32 rawMinTopup, rawMaxWallet, rawSymPos, rawSymSpace, rawAttr;
    sceKernelMemcpy(&rawDecimals, payload + 8, 2);
    out->decimals = bswap16(rawDecimals);
    sceKernelMemcpy(&rawMinTopup, payload + 10, 4);
    out->minTopupAmount = bswap32(rawMinTopup);
    sceKernelMemcpy(&rawMaxWallet, payload + 14, 4);
    out->maxWalletBalance = bswap32(rawMaxWallet);
    sceKernelMemcpy(&rawSymPos, payload + 18, 4);
    out->symbolPosition = bswap32(rawSymPos);
    sceKernelMemcpy(&rawSymSpace, payload + 22, 4);
    out->symbolWithSpace = bswap32(rawSymSpace);
    sceKernelMemcpy(&rawAttr, payload + 26, 4);
    out->currencyAttr = bswap32(rawAttr);

    const u8 *tail = payload + 30;
    out->commonData.subData = tail;
    u32 rem = itemLen - (u32)(tail - out->commonData.data);

    int ret = sceNpCommerce2_3F6D413C(tail, rem, 21, NULL, &out->countOfTopupAmount);
    if (ret < 0)
        return ret;

    out->thousandSeparator = emptyResponseStr(out->commonData.bufHead);
    ret = sceNpCommerce2_E3455E27(tail, itemLen - (u32)(tail - out->commonData.data), 125, 0, &thousandLen);
    if (ret >= 0 && thousandLen != 0)
        out->thousandSeparator = (const char *)(tail + ret);
    if (ret < 0)
        return ret;

    out->decimalLetter = emptyResponseStr(out->commonData.bufHead);
    ret = sceNpCommerce2_E3455E27(tail, itemLen - (u32)(tail - out->commonData.data), 126, 0, &decimalLen);
    if (ret >= 0 && decimalLen != 0)
        out->decimalLetter = (const char *)(tail + ret);
    return ret;
}

static __attribute__((noinline)) int getCurrencyInfoWrapper(
    const SceNpCommerce2CreateSessionResult *res, u32 index, SceNpCommerce2CurrencyInfo *out) {
    return parseCurrencyInfo(res, index, out);
}

static __attribute__((noinline)) int parseWalletInfo(
    const SceNpCommerce2CommonData *base, SceNpCommerce2WalletInfo *out) {
    const u8 *src = base->data;
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, base->dataSize, 87, 4, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2WalletInfo));
    initChildCommonData(&out->commonData, base->version, base->bufHead, payload, len);
    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload, 4);
    out->commonData.subData = payload + 4;
    out->isCreditCardRegistered = (bswap32(rawWord) != 0);
    return foundOff;
}

static __attribute__((noinline)) int parseTopupAmount(
    const SceNpCommerce2CommonData *base, u32 index, SceNpCommerce2TopupAmountInfo *out) {
    const u8 *src = base->subData;
    u32 rem = base->dataSize - (u32)(base->subData - base->data);
    u16 len;
    int foundOff = findIndexedArrayItem(src, rem, 21, 4, index, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2TopupAmountInfo));
    initChildCommonData(&out->commonData, base->version, base->bufHead, payload, len);
    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload, 4);
    out->amount = bswap32(rawWord);
    return foundOff;
}

static __attribute__((noinline)) int parseContentRatingDescriptor(
    const SceNpCommerce2ContentRatingInfo *ratingInfo, u32 index,
    SceNpCommerce2ContentRatingDescriptor *out) {
    const u8 *src = ratingInfo->commonData.subData;
    u32 rem = ratingInfo->commonData.dataSize - (u32)(src - ratingInfo->commonData.data);
    u16 len, optLen;
    int foundOff = findIndexedArrayItem(src, rem, 23, 8, index, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2ContentRatingDescriptor));
    initChildCommonData(&out->commonData, ratingInfo->commonData.version,
                        ratingInfo->commonData.bufHead, payload, len);
    out->imageUrl = emptyResponseStr(ratingInfo->commonData.bufHead);
    out->contentRatingDescription = emptyResponseStr(ratingInfo->commonData.bufHead);

    u32 descType;
    sceKernelMemcpy(&descType, payload, 4);
    descType = bswap32(descType);
    out->descriptorType = descType;
    const u8 *tail = payload + 4;
    out->commonData.subData = tail;

    if (descType == SCE_NP_COMMERCE2_CONTENT_RATING_DESC_TYPE_ICON) {
        out->imageUrl = emptyResponseStr(out->commonData.bufHead);
        int ret = sceNpCommerce2_E3455E27(tail, len - 4u, 16, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out->imageUrl = (const char *)(tail + ret);
        return ret;
    }
    if (descType == SCE_NP_COMMERCE2_CONTENT_RATING_DESC_TYPE_TEXT) {
        out->contentRatingDescription = emptyResponseStr(out->commonData.bufHead);
        int ret = sceNpCommerce2_E3455E27(tail, len - 4u, 14, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out->contentRatingDescription = (const char *)(tail + ret);
        return ret;
    }
    return foundOff;
}

static __attribute__((noinline)) int parseCategoryInfoBody(
    const u8 *payload, u32 len, const u8 *bufHead, u32 ver, int dataType,
    SceNpCommerce2CategoryInfo *out) {
    u16 optLen;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2CategoryInfo));
    initChildCommonData(&out->commonData, ver, bufHead, payload, len);
    out->dataType = dataType;
    out->categoryId = (const char *)payload;

    u32 rawDate[2];
    sceKernelMemcpy(rawDate, payload + 57, 8);
    u64 epochMs = ((u64)bswap32(rawDate[0]) << 32) | bswap32(rawDate[1]);
    if (epochMs != 0) {
        out->releaseDate = epochMs * 1000ULL + SCE_NP_COMMERCE2_EPOCH_TICK_OFFSET;
    } else {
        sceKernelMemset(&out->releaseDate, 0, sizeof(out->releaseDate));
    }

    const u8 *tail = payload + 65;
    if (dataType == SCE_NP_COMMERCE2_CAT_DATA_TYPE_NORMAL) {
        u32 rawWord;
        sceKernelMemcpy(&rawWord, payload + 65, 4);
        out->countOfSubCategory = bswap32(rawWord);
        sceKernelMemcpy(&rawWord, payload + 69, 4);
        out->countOfProduct = bswap32(rawWord);
        tail = payload + 73;
    }

    out->commonData.subData = tail;
    u32 rem = len - (u32)(tail - payload);
    out->categoryName = emptyResponseStr(out->commonData.bufHead);
    int ret = sceNpCommerce2_E3455E27(tail, rem, 6, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out->categoryName = (const char *)(tail + ret);

    out->categoryDescription = emptyResponseStr(out->commonData.bufHead);
    ret = sceNpCommerce2_E3455E27(tail, rem, 11, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out->categoryDescription = (const char *)(tail + ret);

    out->imageUrl = emptyResponseStr(out->commonData.bufHead);
    ret = sceNpCommerce2_E3455E27(tail, rem, 16, 0, &optLen);
    if (ret < 0) return 0;
    if (optLen != 0) out->imageUrl = (const char *)(tail + ret);

    out->spName = emptyResponseStr(out->commonData.bufHead);
    ret = sceNpCommerce2_E3455E27(tail, rem, 4, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out->spName = (const char *)(tail + ret);
    return 0;
}

static __attribute__((noinline)) int parseGameProductInfoBody(
    const u8 *payload, u32 len, const u8 *bufHead, u32 ver, int dataType,
    SceNpCommerce2GameProductInfo *out) {
    SceNpCommerce2ResponseHeader hdrCopy;
    u16 optLen;
    sceKernelMemcpy(&hdrCopy, bufHead, sizeof(hdrCopy));
    int isSdk603 = ((u32)sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_603);
    sceKernelMemset(out, 0, isSdk603 ? sizeof(SceNpCommerce2GameProductInfo) : 88u);
    initChildCommonData(&out->commonData, ver, bufHead, payload, len);
    out->dataType = dataType;
    out->productId = (const char *)payload;

    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload + 49, 4);
    out->countOfSku = bswap32(rawWord);

    u32 rawDate[2];
    sceKernelMemcpy(rawDate, payload + 53, 8);
    u64 epochMs = ((u64)bswap32(rawDate[0]) << 32) | bswap32(rawDate[1]);
    if (epochMs != 0) {
        out->releaseDate = epochMs * 1000ULL + SCE_NP_COMMERCE2_EPOCH_TICK_OFFSET;
    } else {
        sceKernelMemset(&out->releaseDate, 0, sizeof(out->releaseDate));
    }

    sceKernelMemcpy(&rawWord, payload + 73, 4);
    u32 rawFlags = bswap32(rawWord);
    if (isSdk603) {
        if ((s32)rawFlags < 0)
            out->productAttr |= 1u;
        if (rawFlags & 0x40000000u)
            out->productAttr |= 2u;
        if (rawFlags & 0x20000000u)
            out->productAttr |= 4u;
    }

    const u8 *tail = payload + 77;
    u32 rem = (u16)(len - 77);
    out->commonData.subData = tail;

    out->productName = emptyResponseStr(out->commonData.bufHead);
    int ret = sceNpCommerce2_E3455E27(tail, rem, 7, 0, &optLen);
    if (ret >= 0 && optLen != 0) out->productName = (const char *)(tail + ret);
    if (ret >= 0) {
        out->productShortDescription = emptyResponseStr(out->commonData.bufHead);
        ret = sceNpCommerce2_E3455E27(tail, rem, 12, 0, &optLen);
        if (ret >= 0 && optLen != 0) out->productShortDescription = (const char *)(tail + ret);
        if (ret >= 0) {
            out->imageUrl = emptyResponseStr(out->commonData.bufHead);
            ret = sceNpCommerce2_E3455E27(tail, rem, 16, 0, &optLen);
            if (ret >= 0 && optLen != 0) out->imageUrl = (const char *)(tail + ret);
            if (ret >= 0) {
                out->spName = emptyResponseStr(out->commonData.bufHead);
                ret = sceNpCommerce2_E3455E27(tail, rem, 4, 0, &optLen);
                if (ret >= 0 && optLen != 0) out->spName = (const char *)(tail + ret);
            }
        }
    }

    if (ret >= 0 && dataType == SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_NORMAL) {
        u32 fullRem = len - 77u;
        out->productLongDescription = emptyResponseStr(out->commonData.bufHead);
        ret = sceNpCommerce2_E3455E27(tail, fullRem, 13, 0, &optLen);
        if (ret >= 0 && optLen != 0) out->productLongDescription = (const char *)(tail + ret);
        if (ret >= 0) {
            out->legalDescription = emptyResponseStr(out->commonData.bufHead);
            ret = sceNpCommerce2_E3455E27(tail, fullRem, 119, 0, &optLen);
            if (ret >= 0 && optLen != 0) out->legalDescription = (const char *)(tail + ret);
            if (ret >= 0 || ret == SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
                return 0;
        }
    }
    return ret;
}

static __attribute__((noinline)) int parseContentRatingInfoBody(
    const u8 *payload, u32 len, const u8 *bufHead, u32 ver,
    SceNpCommerce2ContentRatingInfo *out) {
    u16 optLen;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2ContentRatingInfo));
    initChildCommonData(&out->commonData, ver, bufHead, payload, len);
    out->ratingSystemId = (const char *)payload;
    out->imageUrl = emptyResponseStr(bufHead);
    const u8 *tail = payload + 17;
    out->commonData.subData = tail;

    int ret = sceNpCommerce2_E3455E27(tail, len - 17u, 16, 0, &optLen);
    if (ret >= 0 && optLen != 0)
        out->imageUrl = (const char *)(tail + ret);
    if (ret < 0)
        return ret;
    return sceNpCommerce2_3F6D413C(tail, len - 17u, 23, NULL, &out->countOfContentRatingDescriptor);
}

static __attribute__((noinline)) int parseSkuFixedFields(
    const u8 *payload, const SkuParseSpec *spec, int isOldSdk) {
    u32 rawWord;
    u32 extraAttr = 0;
    const u8 *cur = payload + 61;

    *spec->outSkuId = (const char *)payload;
    sceKernelMemcpy(&rawWord, payload + 57, 4);
    rawWord = bswap32(rawWord);
    *spec->outSkuType = (rawWord == 2) ? 2u : 1u;

    sceKernelMemcpy(&rawWord, cur, 4);
    *spec->outCountUntilExpiration = bswap32(rawWord);
    cur += 4;

    sceKernelMemcpy(&rawWord, cur, 4);
    *spec->outTimeUntilExpiration = bswap32(rawWord);
    cur += 4;

    if (spec->outPurchasability != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outPurchasability = bswap32(rawWord);
    }
    cur += 4;

    if (spec->outAnnotation != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outAnnotation = bswap32(rawWord);
    }
    cur += 4;

    sceKernelMemcpy(&rawWord, cur, 4);
    *spec->outDownloadable = (bswap32(rawWord) != 0);
    cur += 4;

    if (spec->flagMask & 4u) {
        sceKernelMemcpy(&rawWord, cur, 4);
        extraAttr = bswap32(rawWord);
    }
    cur += 4;

    if (spec->outPrice != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outPrice = bswap32(rawWord);
    }
    cur += 4;

    if (spec->outRegularPrice != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outRegularPrice = bswap32(rawWord);
    }
    cur += 4;

    if (spec->outDiscountPrice != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outDiscountPrice = bswap32(rawWord);
    }
    cur += 4;

    if (!isOldSdk && spec->outSalesType != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outSalesType = bswap32(rawWord);
    }
    cur += 4;

    if (!isOldSdk && spec->outFirstPlayExpiration != NULL) {
        sceKernelMemcpy(&rawWord, cur, 4);
        *spec->outFirstPlayExpiration = bswap32(rawWord);
    }
    cur += 4;

    if (!isOldSdk) {
        if (spec->outPurchasability != NULL && *spec->outPurchasability != 0)
            *spec->outFlags |= 1u;
        if (spec->outDownloadable != NULL && *spec->outDownloadable != 0)
            *spec->outFlags |= 2u;
        if (extraAttr != 0)
            *spec->outFlags |= 4u;
    }
    return (int)(cur - payload);
}

static __attribute__((noinline)) int parseGameSkuInfoBody(
    const u8 *payload, u32 len, const u8 *bufHead, u32 ver, int dataType,
    SceNpCommerce2GameSkuInfo *out) {
    SceNpCommerce2ResponseHeader hdrCopy;
    SkuParseSpec spec;
    u16 optLen;
    int isSdk600 = ((u32)sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_600);

    sceKernelMemcpy(&hdrCopy, bufHead, sizeof(hdrCopy));
    sceKernelMemset(out, 0, isSdk600 ? sizeof(SceNpCommerce2GameSkuInfo) : 88u);
    initChildCommonData(&out->commonData, ver, bufHead, payload, len);
    out->dataType = dataType;

    sceKernelMemset(&spec, 0, sizeof(spec));
    spec.hdrCopy                 = &hdrCopy;
    spec.flagMask                = 7;
    spec.outFlags                = &out->skuAttr;
    spec.outSkuId                = &out->skuId;
    spec.outSkuType              = &out->skuType;
    spec.outCountUntilExpiration = &out->countUntilExpiration;
    spec.outTimeUntilExpiration  = &out->timeUntilExpiration;
    spec.outPurchasability       = &out->purchasabilityFlag;
    spec.outAnnotation           = &out->annotation;
    spec.outDownloadable         = &out->downloadable;
    spec.outPrice                = &out->price;
    spec.outSalesType            = &out->salesType;
    spec.outFirstPlayExpiration  = &out->firstPlayExpiration;

    int fixedLen = parseSkuFixedFields(payload, &spec, !isSdk600);
    if (fixedLen < 0)
        return fixedLen;

    const u8 *tail = payload + fixedLen + 36;
    u32 rem = len - (u32)(tail - payload);
    out->commonData.subData = tail;

    if (dataType == SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_THIN) {
        out->skuName = emptyResponseStr(out->commonData.bufHead);
        int ret = sceNpCommerce2_E3455E27(tail, rem, 8, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out->skuName = (const char *)(tail + ret);
        if (ret < 0)
            return ret;
    } else if (dataType == SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL) {
        u32 skuType = out->skuType;
        u32 shortRem = (u16)rem;
        out->skuName = emptyResponseStr(out->commonData.bufHead);
        int ret = sceNpCommerce2_E3455E27(tail, shortRem, 8, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out->skuName = (const char *)(tail + ret);
        if (ret < 0)
            return ret;

        out->productId = emptyResponseStr(out->commonData.bufHead);
        ret = sceNpCommerce2_E3455E27(tail, shortRem, 56, 0, &optLen);
        if (ret >= 0 && optLen != 0)
            out->productId = (const char *)(tail + ret);
        if (ret < 0)
            return ret;

        out->contentLinkUrl = emptyResponseStr(out->commonData.bufHead);
        if (skuType == 2) {
            out->contentLinkUrl = emptyResponseStr(out->commonData.bufHead);
            ret = sceNpCommerce2_E3455E27(tail, shortRem, 18, 0, &optLen);
            if (ret >= 0 && optLen != 0)
                out->contentLinkUrl = (const char *)(tail + ret);
            if (ret < 0)
                return ret;
        }
    }

    if (!isSdk600)
        return 0;

    int ret = sceNpCommerce2_3F6D413C(tail, rem, 128, NULL, &out->countOfRewardInfo);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    ret = sceNpCommerce2_3F6D413C(tail, rem, 149, NULL, &out->countOfApplicableRewardInfo);
    if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
        return ret;
    if (dataType == SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL) {
        ret = sceNpCommerce2_3F6D413C(tail, rem, 156, NULL, &out->countOfGameSkuEntitlementInfo);
        if (ret < 0 && ret != SCE_NP_COMMERCE2_ERROR_TLV_NOT_FOUND)
            return ret;
    }
    return 0;
}

static __attribute__((noinline)) int initCategoryContentsResult(
    SceNpCommerce2GetCategoryContentsResult *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen, isTrunc;
    int hdrOff = parseResponseHeader(buf, bufSize, &ver, &bodyLen, &isTrunc);
    if (hdrOff < 0)
        return hdrOff;

    const u8 *body = buf + hdrOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2GetCategoryContentsResult));
    initCommonData(&out->commonData, ver, buf, (u32)hdrOff + bodyLen, body, bodyLen);
    out->commonData.overflow = isTrunc;

    int ret = sceNpCommerce2_69FEA1C8(body, bodyLen, 42, NULL, &out->rangeOfContents, (int)isTrunc);
    if (ret < 0)
        return ret;
    ret = clampRangeCount(&out->commonData, 42, 4, &out->rangeOfContents);
    return (ret < 0) ? ret : 0;
}

static __attribute__((noinline)) int parseCategoryInfoFromResult(
    const SceNpCommerce2GetCategoryContentsResult *res, SceNpCommerce2CategoryInfo *out) {
    const u8 *src = res->commonData.data;
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, res->commonData.dataSize, 27, 89, &len);
    if (foundOff < 0)
        return foundOff;
    return parseCategoryInfoBody(src + foundOff, len, res->commonData.bufHead,
                                 res->commonData.version, SCE_NP_COMMERCE2_CAT_DATA_TYPE_NORMAL, out);
}

static __attribute__((noinline)) int parseContentInfo(
    const SceNpCommerce2GetCategoryContentsResult *res, u32 index, SceNpCommerce2ContentInfo *out) {
    const u8 *src = res->commonData.data;
    u16 len;
    int foundOff = findIndexedRangeItem(src, res->commonData.dataSize, 42, 4, index, &len, (int)res->commonData.overflow);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2ContentInfo));
    initChildCommonData(&out->commonData, res->commonData.version, res->commonData.bufHead, payload, len);
    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload, 4);
    out->commonData.subData = payload + 4;
    out->contentType = bswap32(rawWord);
    return foundOff;
}

static __attribute__((noinline)) int parseCategoryInfoFromContent(
    const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2CategoryInfo *out) {
    const u8 *src = contentInfo->commonData.subData;
    u32 rem = contentInfo->commonData.dataSize - (u32)(src - contentInfo->commonData.data);
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, rem, 26, 89, &len);
    if (foundOff < 0)
        return foundOff;
    return parseCategoryInfoBody(src + foundOff, len, contentInfo->commonData.bufHead,
                                 contentInfo->commonData.version, SCE_NP_COMMERCE2_CAT_DATA_TYPE_THIN, out);
}

static __attribute__((noinline)) int parseProductInfoFromContent(
    const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2ProductInfo *out) {
    const u8 *src = contentInfo->commonData.subData;
    u32 rem = contentInfo->commonData.dataSize - (u32)(src - contentInfo->commonData.data);
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, rem, 40, 4, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2ProductInfo));
    initChildCommonData(&out->commonData, contentInfo->commonData.version,
                        contentInfo->commonData.bufHead, payload, len);
    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload, 4);
    out->commonData.subData = payload + 4;
    out->productType = bswap32(rawWord);
    return foundOff;
}

static __attribute__((noinline)) int parseGameProductFromContentProduct(
    const SceNpCommerce2ProductInfo *prodInfo, SceNpCommerce2GameProductInfo *out) {
    const u8 *src = prodInfo->commonData.subData;
    u32 rem = prodInfo->commonData.dataSize - (u32)(src - prodInfo->commonData.data);
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, rem, 33, 81, &len);
    if (foundOff < 0)
        return foundOff;
    return parseGameProductInfoBody(src + foundOff, len, prodInfo->commonData.bufHead,
                                    prodInfo->commonData.version, SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_THIN, out);
}

static __attribute__((noinline)) int parseRatingInfo(
    const SceNpCommerce2CommonData *parentCommon, SceNpCommerce2ContentRatingInfo *out) {
    const u8 *src = parentCommon->subData;
    u32 rem = parentCommon->dataSize - (u32)(src - parentCommon->data);
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, rem, 24, 12, &len);
    if (foundOff < 0)
        return foundOff;
    return parseContentRatingInfoBody(src + foundOff, len, parentCommon->bufHead,
                                      parentCommon->version, out);
}

static __attribute__((noinline)) int initProductInfoResult(
    SceNpCommerce2GetProductInfoResult *out, const u8 *buf, u32 bufSize) {
    u32 ver, bodyLen;
    int hdrOff = parseResponseHeader(buf, bufSize, &ver, &bodyLen, NULL);
    if (hdrOff < 0)
        return hdrOff;

    const u8 *body = buf + hdrOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2GetProductInfoResult));
    initCommonData(&out->commonData, ver, buf, (u32)hdrOff + bodyLen, body, bodyLen);
    return 0;
}

static __attribute__((noinline)) int parseProductInfoFromResult(
    const SceNpCommerce2GetProductInfoResult *res, SceNpCommerce2ProductInfo *out) {
    const u8 *src = res->commonData.data;
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, res->commonData.dataSize, 41, 4, &len);
    if (foundOff < 0)
        return foundOff;

    const u8 *payload = src + foundOff;
    sceKernelMemset(out, 0, sizeof(SceNpCommerce2ProductInfo));
    initChildCommonData(&out->commonData, res->commonData.version, res->commonData.bufHead, payload, len);
    u32 rawWord;
    sceKernelMemcpy(&rawWord, payload, 4);
    out->commonData.subData = payload + 4;
    out->productType = bswap32(rawWord);
    return foundOff;
}

static __attribute__((noinline)) int parseGameProductFromResultProduct(
    const SceNpCommerce2ProductInfo *prodInfo, SceNpCommerce2GameProductInfo *out) {
    const u8 *src = prodInfo->commonData.subData;
    u32 rem = prodInfo->commonData.dataSize - (u32)(src - prodInfo->commonData.data);
    u16 len;
    int foundOff = sceNpCommerce2_E3455E27(src, rem, 34, 93, &len);
    if (foundOff < 0)
        return foundOff;
    return parseGameProductInfoBody(src + foundOff, len, prodInfo->commonData.bufHead,
                                    prodInfo->commonData.version, SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_NORMAL, out);
}

static __attribute__((noinline)) int parseGameSkuFromProduct(
    const SceNpCommerce2GameProductInfo *gameProd, u32 dataType, u32 index,
    SceNpCommerce2GameSkuInfo *out) {
    const u8 *src = gameProd->commonData.subData;
    u32 rem = gameProd->commonData.dataSize - (u32)(src - gameProd->commonData.data);
    u16 len;
    int foundOff;
    if (dataType == SCE_NP_COMMERCE2_GAME_PRODUCT_DATA_TYPE_NORMAL) {
        foundOff = findIndexedArrayItem(src, rem, 29, 89, index, &len);
        if (foundOff < 0)
            return foundOff;
        return parseGameSkuInfoBody(src + foundOff, len, gameProd->commonData.bufHead,
                                    gameProd->commonData.version, SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_NORMAL, out);
    } else {
        foundOff = sceNpCommerce2_E3455E27(src, rem, 28, 85, &len);
        if (foundOff < 0)
            return foundOff;
        return parseGameSkuInfoBody(src + foundOff, len, gameProd->commonData.bufHead,
                                    gameProd->commonData.version, SCE_NP_COMMERCE2_GAME_SKU_DATA_TYPE_THIN, out);
    }
}

/* =========================================================================
 * Session Cache & Core Operations
 * ========================================================================= */

static __attribute__((noinline)) int postProcessResponse(
    Commerce2Ctx *ctx, Commerce2Req *req, u8 *buf, u32 bufSize, u32 respSize) {
    SceNpCommerce2AccountInfo acctInfo;
    SceNpCommerce2SessionResultEx sessRes;
    u16 action = req->action;

    if (action == 49 || action == 1 || action == 66) {
        int ret;
        if (action == 49)
            ret = parseSessionResultEx(&sessRes, buf, respSize);
        else
            ret = parseCreateSessionResult((SceNpCommerce2CreateSessionResult *)&sessRes, buf, respSize);
        if (ret >= 0 && parseAccountInfo(&sessRes.commonData, &acctInfo) >= 0) {
            sceNet_lib_B5CE388A(ctx->country, acctInfo.countryCode, 4);
            ctx->country[3] = '\0';
            sceNet_lib_B5CE388A(ctx->lang, acctInfo.langCode, 4);
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

static __inline__ __attribute__((always_inline)) void refreshIdleConnection(Commerce2Ctx *ctx) {
    u64 now;
    if (sceNpCommerce2_FC47B63C(&now) >= 0) {
        u64 elapsed = now - ctx->lastActivityTime;
        if (elapsed > 9999999ULL)
            sceNpCommerce2_75DF99A9(ctx->httpTmplId, &ctx->httpConnId);
    }
}

static __attribute__((noinline)) int coreInit(const char *envStr, int envFlag) {
    int ret = setEnvString(envStr);
    if (ret >= 0) {
        ret = setEnvFlag(envFlag);
        if (ret >= 0) {
            sceKernelMemset(&g_ctxMgr, 0, sizeof(g_ctxMgr));
            return 0;
        }
    }
    return ret;
}

static __attribute__((noinline)) int coreInitWrapper(const char *envStr, int envFlag) {
    if (g_coreInitialized != 0)
        return SCE_NP_COMMERCE2_ERROR_ALREADY_INITIALIZED;
    int ret = coreInit(envStr, envFlag);
    if (ret < 0)
        return ret;
    g_coreInitialized = 1;
    return 0;
}

static __attribute__((noinline)) int termCoreContexts(void) {
    return sceNpCommerce2_8E80E661(&g_ctxMgr);
}

static __attribute__((noinline)) int destroyAllContextsInternal(void) {
    return sceNpCommerce2_8E80E661(&g_ctxMgr);
}

static __attribute__((noinline)) int coreTermWrapper(void) {
    if (g_coreInitialized != 0) {
        termCoreContexts();
        g_coreInitialized = 0;
    }
    return 0;
}

static __attribute__((noinline)) int createCtxInternal(u32 version, u32 *ctxIdOut, int isInternal) {
    if (version >= 3u)
        return SCE_NP_COMMERCE2_ERROR_UNSUPPORTED_VERSION;
    if (ctxIdOut == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

    Commerce2Ctx *ctx = (Commerce2Ctx *)sceNetIfhandle_lib_15CFE3C0(sizeof(Commerce2Ctx));
    int ret = SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY;
    if (ctx != NULL) {
        sceKernelMemset(ctx, 0, sizeof(Commerce2Ctx));
        ctx->version = version;
        ctx->storeType = (ctx->isInternal == 1) ? 1u : 3u;

        ret = sceNpCommerce2_CCEEA788(&ctx->httpTmplId, &ctx->httpConnId, isInternal);
        if (ret >= 0) {
            ret = sceNpCommerce2_2AFD0CC7(&g_ctxMgr, ctx, ctxIdOut, isInternal);
            if (ret >= 0)
                return ret;
        }
        if (ctx->httpTmplId != 0)
            sceNpCommerce2_CBA487E9(ctx->httpTmplId, ctx->httpConnId);
        sceNetIfhandle_lib_76BAD213(ctx);
    }
    return ret;
}

static __attribute__((noinline)) int destroyCtxInternal(u32 ctxId) {
    Commerce2Ctx *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, ctxId, (void **)&ctx, 1);
    if (ret >= 0)
        ret = sceNpCommerce2_8139E653(&g_ctxMgr, ctxId);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int createReqInternal(u32 ctxId, u16 action, u32 *reqIdOut) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, ctxId, (void **)&ctx, 1);
    if (ret >= 0) {
        req = (Commerce2Req *)sceNetIfhandle_lib_15CFE3C0(sizeof(Commerce2Req));
        if (req == NULL) {
            ret = SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY;
        } else {
            sceKernelMemset(req, 0, sizeof(Commerce2Req));
            req->action = action;
            ret = sceNpCommerce2_06618C81(&g_ctxMgr, req, ctxId, reqIdOut);
            if (ret < 0)
                sceNetIfhandle_lib_76BAD213(req);
        }
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int createSessionCreateReqInternal(u32 ctxId, u32 *reqIdOut) {
    return createReqInternal(ctxId, 1, reqIdOut);
}

static __attribute__((noinline)) int deleteSessionCreateReqInternal(u32 ctxId, u32 *reqIdOut) {
    return createReqInternal(ctxId, 2, reqIdOut);
}

static __attribute__((noinline)) int getCategoryCreateReqInternal(u32 ctxId, u32 *reqIdOut) {
    return createReqInternal(ctxId, 3, reqIdOut);
}

static __attribute__((noinline)) int getProductCreateReqInternal(u32 ctxId, u32 *reqIdOut) {
    return createReqInternal(ctxId, 4, reqIdOut);
}

static __attribute__((noinline)) int destroyReqInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
    if (ret >= 0) {
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret >= 0) {
            sceNpCommerce2_02C307C0(&g_ctxMgr, reqId);
            ret = 0;
        }
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int abortReqInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
    if (ret >= 0) {
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret >= 0) {
            sceNpCommerce2_63B96D73(req->httpReqId);
            req->flags |= 2u;
            ret = 0;
        }
    }
    if (req != NULL)
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int getResultInternal(u32 reqId, u8 *buf, u32 bufSize, u32 *fillSizeOut) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    u8 serverStatus = 0;
    int isTrunc = 0;
    int expired = 0;
    int ret;

    do {
        if (buf == NULL || bufSize == 0 || fillSizeOut == NULL) {
            ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
            break;
        }

        ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
        if (ret < 0)
            break;
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret < 0)
            break;

        if ((req->action == 3 || req->action == 4) && ctx->isInternal != 1 && bufSize < SCE_NP_COMMERCE2_RECV_BUF_SIZE) {
            ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        ret = sceNpCommerce2_4D43B84D(req->httpReqId, &ctx->buildVersion);
        if (ret < 0)
            break;

        int hdrLen = sceNpCommerce2_C27FEAB1(ctx, req->httpReqId, req->action, &serverStatus, buf, bufSize);
        if (hdrLen < 0) {
            ret = hdrLen;
            break;
        }

        int bodyLen = sceNpCommerce2_D4C319C6(req->httpReqId, buf, buf + hdrLen, bufSize, &isTrunc);
        if (bodyLen < 0) {
            ret = bodyLen;
            break;
        }

        if (isTrunc == 1) {
            u16 act = req->action;
            if (act != 3 && act != 5 && act != 7 && act != 53 && act != 54 && act != 55 && act != 76) {
                ret = SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
                break;
            }
        }

        ret = sceNpCommerce2_ADAA054C(buf, bufSize, (u32)bodyLen, isTrunc);
        if (ret < 0)
            break;

        *fillSizeOut = (u32)(hdrLen + bodyLen);
        if (serverStatus == 0) {
            postProcessResponse(ctx, req, buf, bufSize, *fillSizeOut);
            ret = 0;
            break;
        }

        ret = SCE_NP_COMMERCE2_ERROR_SERVER_BASE | (u32)serverStatus;
        if (req->action == 38 && ret == (SCE_NP_COMMERCE2_ERROR_SERVER_BASE | 1)) {
            ret = SCE_NP_COMMERCE2_ERROR_SERVER_CODE_41;
            break;
        }
        if ((req->action == 23 || req->action == 64) && ret == (SCE_NP_COMMERCE2_ERROR_SERVER_BASE | 1)) {
            ret = SCE_NP_COMMERCE2_ERROR_SERVER_CODE_2B;
            break;
        }

        checkSessionExpiredError(buf, *fillSizeOut, &expired);
        if (expired == 1)
            ret = SCE_NP_COMMERCE2_ERROR_SESSION_EXPIRED;
    } while (0);

    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int createSessionStartInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    char consoleId[80];
    char password[48];
    char username[80];
    int version;
    int ret;

    do {
        ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
        if (ret < 0) break;
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret < 0) break;
        ret = sceNpCommerce2_F0900551(ctx, &version);
        if (ret < 0) break;
        ret = sceNpCommerce2_02CADA41(ctx, username, password);
        if (ret < 0) break;
        ret = sceNpCommerce2_157FA8AA(consoleId, 65);
        if (ret < 0) break;

        refreshIdleConnection(ctx);
        ret = sendCreateSessionHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                    req->action, &req->httpReqId, (u32)version,
                                    username, password, consoleId);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);
    } while (0);

    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int deleteSessionStartInternal(u32 reqId) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    do {
        ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
        if (ret < 0) break;
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret < 0) break;

        refreshIdleConnection(ctx);
        ret = sendDeleteSessionHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                    req->action, &req->httpReqId);
        if (ret >= 0) {
            sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);
            ctx->sessionLen = 0;
        }
    } while (0);

    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int getCategoryStartInternal(
    u32 reqId, const char *categoryId,
    const char *const *rewardIds, u32 rewardCount,
    u32 startPos, u32 maxCount) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    do {
        ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
        if (ret < 0) break;
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret < 0) break;

        if (ctx->isInternal == 0 && maxCount > SCE_NP_COMMERCE2_GETCAT_MAX_COUNT) {
            ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
            break;
        }

        refreshIdleConnection(ctx);
        ret = sendGetCategoryHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                  req->action, &req->httpReqId, categoryId,
                                  rewardIds, rewardCount, startPos, maxCount);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);
    } while (0);

    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int getProductStartInternal(
    u32 reqId, const char *categoryId, const char *productId,
    const char *const *rewardIds, u32 rewardCount) {
    Commerce2Ctx *ctx = NULL;
    Commerce2Req *req = NULL;
    int ret;

    do {
        ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, reqId >> 16, (void **)&ctx, 1);
        if (ret < 0) break;
        ret = sceNpCommerce2_851E7844(&g_ctxMgr, reqId, (void **)&req);
        if (ret < 0) break;

        refreshIdleConnection(ctx);
        ret = sendGetProductHttp((int)ctx->storeType, ctx->httpTmplId, &ctx->httpConnId,
                                 req->action, &req->httpReqId, categoryId,
                                 productId, rewardIds, rewardCount);
        if (ret >= 0)
            sceNpCommerce2_FC47B63C(&ctx->lastActivityTime);
    } while (0);

    if (req != NULL) {
        if (req->flags & 2u)
            ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
        sceNpCommerce2_8642C532(&g_ctxMgr, req);
    }
    if (ctx != NULL)
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

static __attribute__((noinline)) int getCachedSessionInternal(
    u32 ctxId, void *buf, u32 bufSize, u32 *sizeOut) {
    Commerce2Ctx *ctx = NULL;
    int ret = sceNpCommerce2_0A6AB496(&g_ctxMgr, ctxId, (void **)&ctx, 1);
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
        sceNpCommerce2_77BE411B(&g_ctxMgr, ctx);
    return ret;
}

/* =========================================================================
 * 3DES Token Generator & 64-bit Formatting
 * ========================================================================= */

static __inline__ __attribute__((always_inline)) u8 hexNibble(char ch) {
    if ((u8)(ch - 'a') < 6u) return (u8)(ch - 'a' + 10);
    if ((u8)(ch - 'A') < 6u) return (u8)(ch - 'A' + 10);
    return (u8)(ch - '0');
}

static __inline__ __attribute__((always_inline)) void desCbcEncrypt32(
    DesContext *desCtx, u64 key, const u8 iv[8], const u8 in[32], u8 out[32]) {
    int ret = desKeySchedule(desCtx->roundKeys, key);
    if (ret >= 0) {
        u32 idx;
        for (idx = 0; idx < 8; idx++)
            desCtx->iv[idx] = iv[idx];
        ret = 0;
    }
    if (ret >= 0) {
        u32 blkOff;
        for (blkOff = 0; blkOff < 32; blkOff += 8) {
            u8 swapped[8];
            u32 byteIdx;
            for (byteIdx = 0; byteIdx < 8; byteIdx++)
                swapped[7 - byteIdx] = in[blkOff + byteIdx] ^ desCtx->iv[byteIdx];
            u64 blockVal;
            sceKernelMemcpy(&blockVal, swapped, 8);
            blockVal = desEncryptBlock(desCtx->roundKeys, blockVal);
            sceKernelMemcpy(swapped, &blockVal, 8);
            for (byteIdx = 0; byteIdx < 8; byteIdx++) {
                u8 cipherByte = swapped[7 - byteIdx];
                desCtx->iv[byteIdx] = cipherByte;
                out[blkOff + byteIdx] = cipherByte;
            }
        }
    }
}

int sceNpCommerce2_EE7255FE(int unused, u8 *outToken) {
    u8 plain[32];
    u8 intermediate[32];
    char consoleIdStr[80];
    DesContext desCtxFirst, desCtxSecond, desCtxThird;
    u64 tick, accountId;
    int ret;
    u32 idx;
    (void)unused;

    sceKernelMemset(plain, 0, sizeof(plain));
    ret = sceRtcGetCurrentTick(&tick);
    if (ret < 0)
        return ret;

    u64 epochMs = (tick - SCE_NP_COMMERCE2_EPOCH_TICK_OFFSET) / 1000ULL;
    u32 msBe[2];
    msBe[0] = bswap32((u32)(epochMs >> 32));
    msBe[1] = bswap32((u32)epochMs);
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

    for (idx = 0; idx < 16; idx++) {
        u8 hiNib = hexNibble(consoleIdStr[2 * idx]);
        u8 loNib = hexNibble(consoleIdStr[2 * idx + 1]);
        plain[16 + idx] = (u8)((hiNib << 4) | loNib);
    }

    desCbcEncrypt32(&desCtxFirst, g_tripleDesKeys[0], g_tripleDesIv, plain, outToken);
    desCbcEncrypt32(&desCtxSecond, g_tripleDesKeys[1], g_tripleDesIv, outToken, intermediate);
    desCbcEncrypt32(&desCtxThird, g_tripleDesKeys[2], g_tripleDesIv, intermediate, outToken);

    return ret;
}

int sceNpCommerce2_22CEC5E2(char *out, int outSize, u64 val, int isDecimal) {
    (void)outSize;
    if (isDecimal != 1) {
        u32 lo = (u32)val;
        u32 hi = (u32)(val >> 32);
        return sceNet_lib_CF705E46(
            out, "%02x%02x%02x%02x%02x%02x%02x%02x",
            (hi >> 24) & 0xFFu, (hi >> 16) & 0xFFu, (hi >> 8) & 0xFFu, hi & 0xFFu,
            (lo >> 24) & 0xFFu, (lo >> 16) & 0xFFu, (lo >> 8) & 0xFFu, lo & 0xFFu);
    }

    u64 divisor = 1;
    int powIdx;
    for (powIdx = 19; powIdx > 0; powIdx--)
        divisor *= 10ULL;

    int started = 0;
    int digits = 19;
    u32 acc = 0;
    char *cur = out;

    do {
        u32 quotient = (u32)(val / divisor);
        u32 digit = quotient - acc;
        if (started || digit != 0) {
            *cur++ = (char)(digit + '0');
            started = 1;
        }
        divisor /= 10ULL;
        acc = 10u * (acc + digit);
        digits--;
    } while (digits >= 0);
    *cur = '\0';
    return 0;
}

/* =========================================================================
 * Public API Exports (with SDK version & thread stack checks)
 * ========================================================================= */

#define CHECK_STACK(minStack) \
    do { \
        if (sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK && \
            sceKernelCheckThreadStack() < (minStack)) \
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
    return destroyAllContextsInternal();
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
    return createSessionCreateReqInternal(ctxId, reqId);
}

int sceNpCommerce2CreateSessionStart(u32 reqId) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    return createSessionStartInternal(reqId);
}

static __attribute__((noinline)) int createSessionGetResultChecked(
    u32 reqId, void *buf, u32 bufSize, u32 *fillSize, int checkBuf) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK &&
        ((checkBuf == 1) & (bufSize < SCE_NP_COMMERCE2_RECV_BUF_SIZE)))
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    return getResultInternal(reqId, (u8 *)buf, bufSize, fillSize);
}

int sceNpCommerce2CreateSessionGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return createSessionGetResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2_48D77A66(SceNpCommerce2CreateSessionResult *res, const void *buf, u32 bufSize) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((buf == NULL) | (bufSize == 0))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCreateSessionResult(res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2_287B6FB3(const SceNpCommerce2CreateSessionResult *res, u32 idx, SceNpCommerce2CurrencyInfo *info) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((res == NULL) | (info == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return getCurrencyInfoWrapper(res, idx, info);
}

int sceNpCommerce2_4DB0E04F(const SceNpCommerce2CommonData *res, SceNpCommerce2WalletInfo *walletInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((res == NULL) | (walletInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseWalletInfo(res, walletInfo);
}

int sceNpCommerce2_6795EE19(const SceNpCommerce2CommonData *currInfo, u32 idx, SceNpCommerce2TopupAmountInfo *topupInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((currInfo == NULL) | (topupInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseTopupAmount(currInfo, idx, topupInfo);
}

int sceNpCommerce2_7B00DE40(u32 ctxId, u32 *reqId) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return deleteSessionCreateReqInternal(ctxId, reqId);
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
    return getCategoryCreateReqInternal(ctxId, reqId);
}

static __attribute__((noinline)) int getCategoryStartWrapper(
    u32 reqId, const char *categoryId,
    const char *const *rewardIds, u32 rewardCount,
    u32 startPos, u32 maxCount, int isPublic) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (categoryId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1 &&
        sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK &&
        maxCount > SCE_NP_COMMERCE2_GETCAT_MAX_COUNT)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1 && sceNp_2E4769C3()) {
        char formatted[32];
        formatContentIdPrefixInline(categoryId, formatted, sizeof(formatted));
        KprintfForUser("%s: %s\n", "GetCategoryContents", formatted);
    }
    return getCategoryStartInternal(reqId, categoryId, rewardIds, rewardCount, startPos, maxCount);
}

int sceNpCommerce2GetCategoryContentsStart(u32 reqId, const char *categoryId, u32 startPos, u32 maxCount) {
    return getCategoryStartWrapper(reqId, categoryId, NULL, 0, startPos, maxCount, 1);
}

static __attribute__((noinline)) int getCategoryContentsGetResultChecked(
    u32 reqId, void *buf, u32 bufSize, u32 *fillSize, int checkBuf) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK &&
        ((checkBuf == 1) & (bufSize < SCE_NP_COMMERCE2_RECV_BUF_SIZE)))
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    return getResultInternal(reqId, (u8 *)buf, bufSize, fillSize);
}

int sceNpCommerce2GetCategoryContentsGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return getCategoryContentsGetResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2InitGetCategoryContentsResult(SceNpCommerce2GetCategoryContentsResult *res, const void *buf, u32 bufSize) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((buf == NULL) | (bufSize == 0))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return initCategoryContentsResult(res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2DestroyGetCategoryContentsResult(void) {
    CHECK_STACK(4096);
    return 0;
}

int sceNpCommerce2GetCategoryInfo(const SceNpCommerce2GetCategoryContentsResult *res, SceNpCommerce2CategoryInfo *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((res == NULL) | (info == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCategoryInfoFromResult(res, info);
}

int sceNpCommerce2GetContentInfo(const SceNpCommerce2GetCategoryContentsResult *res, u32 index, SceNpCommerce2ContentInfo *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((res == NULL) | (info == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseContentInfo(res, index, info);
}

int sceNpCommerce2GetCategoryInfoFromContentInfo(const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2CategoryInfo *info) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((contentInfo == NULL) | (info == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseCategoryInfoFromContent(contentInfo, info);
}

int sceNpCommerce2_3D88DCCA(const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2ProductInfo *productInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((contentInfo == NULL) | (productInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseProductInfoFromContent(contentInfo, productInfo);
}

int sceNpCommerce2_1D9C32C0(const SceNpCommerce2ProductInfo *productInfo, SceNpCommerce2GameProductInfo *gameProductInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((productInfo == NULL) | (gameProductInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseGameProductFromContentProduct(productInfo, gameProductInfo);
}

int sceNpCommerce2GetContentRatingInfoFromCategoryInfo(const SceNpCommerce2CategoryInfo *catInfo, SceNpCommerce2ContentRatingInfo *ratingInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((catInfo == NULL) | (ratingInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseRatingInfo(&catInfo->commonData, ratingInfo);
}

int sceNpCommerce2_3755FC84(const SceNpCommerce2ContentRatingInfo *ratingInfo, u32 idx, SceNpCommerce2ContentRatingDescriptor *desc) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((ratingInfo == NULL) | (desc == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseContentRatingDescriptor(ratingInfo, idx, desc);
}

int sceNpCommerce2GetContentRatingDescriptor(const SceNpCommerce2ContentRatingInfo *ratingInfo, u32 idx, SceNpCommerce2ContentRatingDescriptor *desc) {
    CHECK_STACK(4080);
    return sceNpCommerce2_3755FC84(ratingInfo, idx, desc);
}

int sceNpCommerce2GetProductInfoCreateReq(u32 ctxId, u32 *reqId) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (reqId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return getProductCreateReqInternal(ctxId, reqId);
}

static __attribute__((noinline)) int getProductStartWrapper(
    u32 reqId, const char *categoryId, const char *productId,
    const char *const *rewardIds, u32 rewardCount, int isPublic) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (productId == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    if (isPublic == 1 && sceNp_2E4769C3()) {
        char formatted[32];
        formatContentIdPrefixInline(productId, formatted, sizeof(formatted));
        KprintfForUser("%s: %s\n", "GetProductInfo", formatted);
    }
    return getProductStartInternal(reqId, categoryId, productId, rewardIds, rewardCount);
}

int sceNpCommerce2GetProductInfoStart(u32 reqId, const char *categoryId, const char *productId) {
    return getProductStartWrapper(reqId, categoryId, productId, NULL, 0, 1);
}

static __attribute__((noinline)) int getProductInfoGetResultChecked(
    u32 reqId, void *buf, u32 bufSize, u32 *fillSize, int checkBuf) {
    CHECK_STACK(14320);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (sceKernelGetCompiledSdkVersion() > SCE_NP_COMMERCE2_SDK_VER_STACK_CHECK &&
        ((checkBuf == 1) & (bufSize < SCE_NP_COMMERCE2_RECV_BUF_SIZE)))
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    return getResultInternal(reqId, (u8 *)buf, bufSize, fillSize);
}

int sceNpCommerce2GetProductInfoGetResult(u32 reqId, void *buf, u32 bufSize, u32 *fillSize) {
    return getProductInfoGetResultChecked(reqId, buf, bufSize, fillSize, 1);
}

int sceNpCommerce2InitGetProductInfoResult(SceNpCommerce2GetProductInfoResult *res, const void *buf, u32 bufSize) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((buf == NULL) | (bufSize == 0))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return initProductInfoResult(res, (const u8 *)buf, bufSize);
}

int sceNpCommerce2DestroyGetProductInfoResult(void) {
    CHECK_STACK(4096);
    return 0;
}

int sceNpCommerce2_5C691546(const SceNpCommerce2GetProductInfoResult *res, SceNpCommerce2ProductInfo *productInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((res == NULL) | (productInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseProductInfoFromResult(res, productInfo);
}

int sceNpCommerce2_993C29DB(const SceNpCommerce2ProductInfo *productInfo, SceNpCommerce2GameProductInfo *gameProductInfo) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((productInfo == NULL) | (gameProductInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseGameProductFromResultProduct(productInfo, gameProductInfo);
}

int sceNpCommerce2GetContentRatingInfoFromGameProductInfo(const SceNpCommerce2GameProductInfo *gameProd, SceNpCommerce2ContentRatingInfo *ratingInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((gameProd == NULL) | (ratingInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseRatingInfo(&gameProd->commonData, ratingInfo);
}

int sceNpCommerce2GetGameSkuInfoFromGameProductInfo(const SceNpCommerce2GameProductInfo *gameProd, u32 index, SceNpCommerce2GameSkuInfo *skuInfo) {
    CHECK_STACK(4080);
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((gameProd == NULL) | (skuInfo == NULL))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return parseGameSkuFromProduct(gameProd, (u32)gameProd->dataType, index, skuInfo);
}

int sceNpCommerce2_7EB30CAA(u32 ctxId, void *buf, u32 bufSize, u32 *sizeOut) {
    if (g_coreInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if ((buf == NULL) | (bufSize == 0))
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
    return getCachedSessionInternal(ctxId, buf, bufSize, sizeOut);
}

int sceNpCommerce2GetSessionInfo(SceNpCommerce2SessionInfo *sessionInfoOut, const void *buf, u32 bufSize) {
    SceNpCommerce2CreateSessionResult sessRes;
    SceNpCommerce2CurrencyInfo currInfo;
    int ret;
    CHECK_STACK(3936);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    if (((buf == NULL) | (bufSize == 0)) || sessionInfoOut == NULL)
        return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

    ret = sceNpCommerce2_48D77A66(&sessRes, buf, bufSize);
    if (ret < 0) return ret;
    ret = sceNpCommerce2_287B6FB3(&sessRes, 0, &currInfo);
    if (ret < 0) return ret;

    u32 sdkVer = (u32)sceKernelGetCompiledSdkVersion();
    sceKernelMemset(sessionInfoOut, 0, (sdkVer > SCE_NP_COMMERCE2_SDK_VER_600) ? sizeof(SceNpCommerce2SessionInfo) : 8u);
    sceKernelMemcpy(sessionInfoOut->currencyCode, currInfo.currencyCode, 3);
    sessionInfoOut->currencyCode[3] = '\0';
    sessionInfoOut->decimals = currInfo.decimals;

    if (sdkVer > SCE_NP_COMMERCE2_SDK_VER_600) {
        sceKernelMemcpy(sessionInfoOut->currencySymbol, currInfo.currencySymbol, 3);
        sessionInfoOut->currencySymbol[3] = '\0';
        sessionInfoOut->symbolPosition = currInfo.symbolPosition;
        sessionInfoOut->symbolWithSpace = currInfo.symbolWithSpace;
        sceKernelMemcpy(sessionInfoOut->thousandSeparator, currInfo.thousandSeparator, 4);
        sessionInfoOut->thousandSeparator[4] = '\0';
        sceKernelMemcpy(sessionInfoOut->decimalLetter, currInfo.decimalLetter, 4);
        sessionInfoOut->decimalLetter[4] = '\0';
    }
    return 0;
}

int sceNpCommerce2GetGameProductInfoFromContentInfo(const SceNpCommerce2ContentInfo *contentInfo, SceNpCommerce2GameProductInfo *gameProd) {
    SceNpCommerce2ProductInfo prodInfo;
    int ret;
    CHECK_STACK(4032);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    ret = sceNpCommerce2_3D88DCCA(contentInfo, &prodInfo);
    if (ret < 0) return ret;
    return sceNpCommerce2_1D9C32C0(&prodInfo, gameProd);
}

int sceNpCommerce2GetGameProductInfo(const SceNpCommerce2GetProductInfoResult *res, SceNpCommerce2GameProductInfo *gameProd) {
    SceNpCommerce2ProductInfo prodInfo;
    int ret;
    CHECK_STACK(4032);
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
    ret = sceNpCommerce2_5C691546(res, &prodInfo);
    if (ret < 0) return ret;
    return sceNpCommerce2_993C29DB(&prodInfo, gameProd);
}

static __inline__ __attribute__((always_inline)) int appendPriceStr(char *dst, char *end, const char *str) {
    u32 len = sceNet_lib_D8722983(str);
    if (dst + len >= end)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    sceNet_lib_80C9F02A(dst, str);
    return (int)sceNet_lib_D8722983(str);
}

int sceNpCommerce2GetPrice(const SceNpCommerce2SessionInfo *sessionInfo, char *outBuf, u32 outBufSize, u32 price) {
    char formatted[32];
    char numStr[32];
    char *cur = formatted;
    char *end = formatted + sizeof(formatted);
    int ret = 0;

    if (sceKernelCheckThreadStack() < 4080)
        return SCE_NP_ERROR_OUT_OF_STACK;
    if (g_apiInitialized == 0)
        return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;

    if (sessionInfo->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_PRE) {
        char *start = cur;
        ret = appendPriceStr(cur, end, sessionInfo->currencyCode);
        if (ret < 0) return ret;
        cur += ret;
        if (sessionInfo->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_PRE) {
            ret = appendPriceStr(cur, end, " ");
            if (ret < 0) return ret;
            cur += ret;
        }
        ret = (int)(cur - start);
    }

    sceNet_lib_CF705E46(numStr, "%u", price);
    numStr[31] = '\0';
    u32 numLen = sceNet_lib_D8722983(numStr);
    if (cur + numLen + (numLen - 1) / 3u + 1 >= end)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;

    s32 intDigits = (s32)sceNet_lib_D8722983(numStr) - (s32)sessionInfo->decimals;
    if (intDigits <= 0) {
        ret = appendPriceStr(cur, end, "0");
        if (ret < 0) return ret;
        cur += ret;
    } else {
        u32 idx;
        for (idx = 0; idx < (u32)intDigits; idx++) {
            *cur++ = numStr[idx];
            *cur = '\0';
            u32 rem = (u32)intDigits - idx - 1;
            if (rem != 0 && (rem % 3u) == 0) {
                ret = appendPriceStr(cur, end, sessionInfo->thousandSeparator);
                if (ret < 0) return ret;
                cur += ret;
            }
        }
    }

    if (sessionInfo->decimals != 0 && sceNet_lib_D8722983(sessionInfo->decimalLetter) != 0) {
        ret = appendPriceStr(cur, end, sessionInfo->decimalLetter);
        if (ret < 0) return ret;
        cur += ret;
        if (intDigits > 0) {
            ret = appendPriceStr(cur, end, numStr + intDigits);
            if (ret < 0) return ret;
            cur += ret;
        } else {
            u32 zeroIdx;
            for (zeroIdx = 0; zeroIdx < (u32)(-intDigits); zeroIdx++) {
                ret = appendPriceStr(cur, end, "0");
                if (ret < 0) return ret;
                cur += ret;
            }
            ret = appendPriceStr(cur, end, numStr);
            if (ret < 0) return ret;
            cur += ret;
        }
    }

    if (sessionInfo->symbolPosition == SCE_NP_COMMERCE2_SYM_POS_POST) {
        char *start = cur;
        ret = appendPriceStr(cur, end, " ");
        if (ret < 0) return ret;
        cur += ret;
        ret = appendPriceStr(cur, end, sessionInfo->currencyCode);
        if (ret < 0) return ret;
        cur += ret;
        ret = (int)(cur - start);
    }

    if (sceNet_lib_D8722983(formatted) >= outBufSize)
        return SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUFFER;
    sceNet_lib_80C9F02A(outBuf, formatted);
    return ret;
}
