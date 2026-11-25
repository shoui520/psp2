/*
 * sceMGVideo_Service (mgvideo.prx) - MagicGate Video DRM Service
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
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR   ((s32)0x80000023)
#define SCE_KERNEL_ERROR_NO_MEMORY      ((s32)0x80020190)

#define MGV_ERR_UNSUPPORTED             ((s32)0x8050C001)
#define MGV_ERR_INVALID_ARG             ((s32)0x8050C002)
#define MGV_ERR_INVALID_DATA            ((s32)0x8050C003)
#define MGV_ERR_NO_MEMORY               ((s32)0x8050C004)
#define MGV_ERR_IO_ERROR                ((s32)0x8050C005)
#define MGV_ERR_NOT_INITIALIZED         ((s32)0x8050C011)
#define MGV_ERR_NOT_OPENED              ((s32)0x8050C012)
#define MGV_ERR_NOT_VERIFIED            ((s32)0x8050C013)
#define MGV_ERR_HASH_MISMATCH           ((s32)0x8050C014)
#define MGV_ERR_NO_SLOT                 ((s32)0x8050C015)
#define MGV_ERR_MGR_ERROR_21            ((s32)0x8050C021)
#define MGV_ERR_MGR_ERROR_22            ((s32)0x8050C022)
#define MGV_ERR_MGR_ERROR_23            ((s32)0x8050C023)
#define MGV_ERR_MGR_ERROR_24            ((s32)0x8050C024)
#define MGV_ERR_MGR_ERROR_25            ((s32)0x8050C025)
#define MGV_ERR_MGR_ERROR_26            ((s32)0x8050C026)
#define MGV_ERR_MGR_ERROR_27            ((s32)0x8050C027)
#define MGV_ERR_BAD_EKB                 ((s32)0x8050C031)
#define MGV_ERR_MGR_ERROR_32            ((s32)0x8050C032)
#define MGV_ERR_UNSUPPORTED_VER         ((s32)0x8050C033)
#define MGV_ERR_RULE_M2                 ((s32)0x8050C041)
#define MGV_ERR_RULE_M3                 ((s32)0x8050C042)
#define MGV_ERR_RULE_M4                 ((s32)0x8050C043)
#define MGV_ERR_RULE_M5                 ((s32)0x8050C044)
#define MGV_ERR_RULE_M6                 ((s32)0x8050C045)
#define MGV_ERR_RULE_M7                 ((s32)0x8050C046)
#define MGV_ERR_RULE_M8                 ((s32)0x8050C047)
#define MGV_ERR_EMPTY_ENTRY             ((s32)0x8050C048)
#define MGV_ERR_NEED_PLAY_START         ((s32)0x8050C04E)
#define MGV_ERR_MSG_FORMAT              ((s32)0x8050C051)
#define MGV_ERR_MSG_VERSION             ((s32)0x8050C052)
#define MGV_ERR_MSG_SESSION             ((s32)0x8050C053)
#define MGV_ERR_MSG_MAC                 ((s32)0x8050C054)
#define MGV_ERR_MSG_TIMEOUT             ((s32)0x8050C055)

/* Imports: SysclibForKernel */
extern void *memset(void *dst, int c, size_t n);
extern int   memcmp(const void *a, const void *b, size_t n);
extern void *memmove(void *dst, const void *src, size_t n);
extern void *memcpy(void *dst, const void *src, size_t n);
extern char *strncpy(char *dst, const char *src, size_t n);

/* Imports: InterruptManagerForKernel */
extern int  sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);

/* Imports: ThreadManForKernel */
extern int sceKernelCreateFpl(const char *name, int partId, u32 attr, u32 blockSize, u32 blocks, void *opt);
extern int sceKernelTryAllocateFpl(int fplId, void **data);
extern int sceKernelDeleteFpl(int fplId);
extern int sceKernelExtendKernelStack(u32 size, int (*cb)(void *), void *arg);
extern int sceKernelDelayThread(u32 usec);

/* Imports: sceSuspendForKernel */
extern int sceKernelPowerLock(int lockType);
extern int sceKernelPowerUnlock(int lockType);

/* Imports: sceSysEventForKernel */
typedef struct SceSysEventHandler {
    int size;
    const char *name;
    int typeMask;
    int (*handler)(int evId, const char *evName, void *param, int *result);
    int r28;
    int busy;
    struct SceSysEventHandler *next;
    int reserved[9];
} SceSysEventHandler;

extern int sceKernelRegisterSysEventHandler(SceSysEventHandler *handler);
extern int sceKernelUnregisterSysEventHandler(SceSysEventHandler *handler);

/* Imports: sceRtc_driver */
extern int sceRtcGetCurrentTick(u64 *tick);
extern int sceRtc_driver_66054C2A(const u64 *tick);
extern int sceRtcGetTick(const void *time, u64 *tick);
extern int sceRtc_driver_8413CADC(u64 *dst, const u64 *src, int hours);
extern u64 sceRtc_driver_C0F36B91(void);
extern u32 sceRtc_driver_C66D9686(void);
extern int sceRtc_driver_CEEF238F(u64 *tick);
extern int sceRtcSetTick(void *time, const u64 *tick);

/* Imports: sceMgr_driver */
extern int sceMgr_driver_02117F1E(void *out8, void *rngCtx);
extern int sceMgr_driver_09C11491(void *buf, u32 size);
extern int sceMgr_driver_0C813CF1(int dstSlot, const void *src, int srcSlot);
extern int sceMgr_driver_0EBA1587(void *dst, const void *src, u32 size, int keySlot, const void *iv);
extern int sceMgr_driver_156A83C3(int handle, void *rngCtx);
extern int sceMgr_driver_1B4DB19C(void *mac, const void *data, const void *iv, u32 size, int keySlot);
extern int sceMgr_driver_1B63AFF9(void *out32, int keySlot1, int keySlot2);
extern int sceMgr_driver_20610CDF(int handle);
extern int sceMgr_driver_275C6686(int handle, int page, const void *buf, u32 size);
extern int sceMgr_driver_2D1CB2B8(u64 *out);
extern int sceMgr_driver_39E49050(int handle, int page, void *buf, u32 size);
extern int sceMgr_driver_3B548024(void *out, int keySlot);
extern int sceMgr_driver_401F71DC(void);
extern int sceMgr_driver_45EA1DB5(void);
extern int sceMgr_driver_4FD151B7(void *mac, const void *data, const void *iv, u32 size, int keySlot);
extern int sceMgr_driver_5A63B6A4(void);
extern int sceMgr_driver_5BABFFAE(void *rngCtx);
extern int sceMgr_driver_5C5AB33B(void *mac, const void *data, const void *iv, u32 size, const void *key);
extern int sceMgr_driver_6B4C5BC5(void *rngCtx);
extern int sceMgr_driver_735972DB(int handle, void *out16);
extern int sceMgr_driver_7B11D7AD(void);
extern int sceMgr_driver_7DB1B0E5(int dstSlot, const void *keyData, int srcSlot);
extern int sceMgr_driver_7EC9DF8B(int handle, int page, void *buf, u32 size);
extern int sceMgr_driver_7F37BECF(void);
extern int sceMgr_driver_84B044C7(void);
extern int sceMgr_driver_85F518AB(void *ekbId, const void *ekbData, u32 ekbSize, int keySlot);
extern int sceMgr_driver_864EA078(void *buf, u32 size);
extern int sceMgr_driver_9315D849(int dstSlot, const void *src, int srcSlot);
extern int sceMgr_driver_97B245D5(int handle, void *out16);
extern int sceMgr_driver_9973CD5B(int keySlot, const void *keyData);
extern int sceMgr_driver_9AE6ED58(void *dst, const void *src, u32 size, int keySlot, const void *iv);
extern int sceMgr_driver_A329A73B(void *mac, const void *data, const void *iv, u32 size, int keySlot);
extern int sceMgr_driver_A3A302CC(int dstSlot, const void *src, int srcSlot);
extern int sceMgr_driver_B40FEAD1(int handle);
extern int sceMgr_driver_B53B2B5B(void *mac, const void *data, const void *iv, u32 size);
extern int sceMgr_driver_B8626BA2(int handle);
extern int sceMgr_driver_BBF6C182(int handle, int page, const void *buf, u32 size);
extern int sceMgr_driver_C0B28CED(int keyType);
extern int sceMgr_driver_C3C8C1FC(int dstSlot, const void *src, int srcSlot);
extern int sceMgr_driver_CEE87932(void);
extern int sceMgr_driver_CF1EC2ED(void *dst, const void *src, const void *iv);
extern int sceMgr_driver_DA7D8C82(void *mac, const void *data, const void *iv, u32 size, int keySlot);
extern int sceMgr_driver_E6A203F8(int keySlot);
extern int sceMgr_driver_EA241D32(int dstSlot, const void *src, int srcSlot);
extern int sceMgr_driver_EBED61A7(void *out32, int keySlot1, int keySlot2);
extern int sceMgr_driver_EDEAAA00(int keySlot, const void *ekbData, u32 ekbSize);
extern int sceMgr_driver_FDF97B95(void *dst, const void *src, const void *iv, u32 size);

/* Inline helpers */
static inline u32 getK1(void) {
    u32 k1;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    return k1;
}

static inline u16 bswap16(u16 v) {
    return (u16)((v >> 8) | (v << 8));
}

static inline u32 bswap32(u32 v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0xFF000000u) >> 24);
}

static inline u16 readBe16(const u8 *p) {
    return (u16)(((u16)p[0] << 8) | p[1]);
}

/* =========================================================================
 * Structures
 * ========================================================================= */

typedef struct MgvBufDesc {
    void *ptr;
    u32 size;
} MgvBufDesc;

typedef struct MgvStreamSlot {
    u32 iv[4];          /* +0x00 (words 0..3) */
    const u8 *keyPtr;   /* +0x10 (word 4): non-NULL for CTR/range mode */
    u32 rangeMin;       /* +0x14 (word 5) */
    u32 rangeMax;       /* +0x18 (word 6) */
    s32 keyMode;        /* +0x1C (word 7): 0=unused, -1=plain/copy, or ptr to key16 */
    u32 streamType;     /* +0x20 (word 8) */
    u32 pad[3];         /* +0x24..+0x2F (48 bytes total) */
} MgvStreamSlot;

typedef struct MgvContentCtx {
    u8 savedKey[16];            /* +0x00 (words 0..3) */
    struct MgvMediaCtx *media;  /* +0x10 (word 4) */
    u8 inUse;                   /* +0x14 */
    u8 suspended;               /* +0x15 */
    s16 keyCount;               /* +0x16 */
    u8 cid[24];                 /* +0x18 (words 6..11) */
    int mgrKeySlot;             /* +0x30 (word 12) */
    u32 pad13[3];               /* +0x34..+0x3F */
    u8 keys[48];                /* +0x40..+0x6F (3 x 16B keys) */
    MgvStreamSlot streams[4];   /* +0x70..+0x12F (4 x 48B = 192B) */
} MgvContentCtx; /* 304 bytes (0x130) */

typedef struct MgvMediaCtx {
    u8 pad0[1984];              /* +0x000..+0x7BF */
    u8 icvTable[512];           /* +0x7C0..+0x9BF (32 x 16B) */
    u8 hashTable[256];          /* +0x9C0..+0xABF (32 x 8B) */
    u8 ekbMacTable[256];        /* +0xAC0..+0xBBF (32 x 8B) */
    u8 rootKey[16];             /* +0xBC0..+0xBCF (words 752..755) */
    u32 pad756;                 /* +0xBD0 (word 756) */
    int mgrHandle;              /* +0xBD4 (word 757) */
    u32 state;                  /* +0xBD8 (word 758): 0=init, 1=open, 2=mounted, 3=verified */
    u32 verifiedMask;           /* +0xBDC (word 759) */
    u32 mediaFlags;             /* +0xBE0 (word 760) */
    int mediaType;              /* +0xBE4 (word 761): 0=MS, 1=HDD, -1=none */
    int refCount;               /* +0xBE8 (word 762) */
    int devType;                /* +0xBEC (word 763): 16=HDD, 32=MS */
    u32 pad764[2];              /* +0xBF0..+0xBF7 */
    int macKeySlot;             /* +0xBF8 (word 766) */
    int (*deriveIkvKey)(int dstSlot, const void *src, int srcSlot);     /* +0xBFC (word 767) */
    int (*deriveContentKey)(int dstSlot, const void *src, int srcSlot); /* +0xC00 (word 768) */
    int (*exportKey)(void *out32, int keySlot1, int keySlot2);          /* +0xC04 (word 769) */
    int (*getMediaId)(int handle, void *out16);                         /* +0xC08 (word 770) */
    int (*getEkb)(void *buf, u32 size);                                 /* +0xC0C (word 771) */
    int (*openMedia)(struct MgvMediaCtx *ctx, void *rngCtx);            /* +0xC10 (word 772) */
    void (*closeMedia)(struct MgvMediaCtx *ctx);                        /* +0xC14 (word 773) */
    int (*termMedia)(struct MgvMediaCtx *ctx);                          /* +0xC18 (word 774) */
    int (*readPage)(struct MgvMediaCtx *ctx, int page, u8 *buf);        /* +0xC1C (word 775) */
    int (*writePage)(struct MgvMediaCtx *ctx, int page, u8 *buf);       /* +0xC20 (word 776) */
    int (*erasePage)(struct MgvMediaCtx *ctx, int page, u8 *buf);       /* +0xC24 (word 777) */
    u64 challengeTick;          /* +0xC28 (words 778..779) */
    u8 challengeNonce[16];      /* +0xC30 (words 780..783) */
    s16 lastKeyIndex;           /* +0xC40 */
    s16 lastTitleIndex;         /* +0xC42 */
    u32 pad785[3];              /* +0xC44..+0xC4F (3152 bytes total) */
} MgvMediaCtx;

typedef struct MgvCapList {
    const char *const *entries;
    int count;
} MgvCapList;

/* =========================================================================
 * Static Read-Only Data
 * ========================================================================= */

static const u32 g_zero16[4] = { 0, 0, 0, 0 };
static const u8  g_maxBcdTime[8] = { 0x99, 0x99, 0x12, 0x31, 0x23, 0x59, 0x59, 0x99 };
static const u8  g_minBcdTime[8] = { 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 };
static const u8  g_defaultAvout[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
static const u8  g_zeroPeriod[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Embedded fallback EKB2 (860 bytes from firmware 0xA380) */
static const u8 g_embeddedEkb2[860] = {
    0x45, 0x4b, 0x42, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x23, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb0, 0xda, 0xe7, 0x67, 0x65, 0xd3, 0xf6, 0x49, 0x42, 0x1d, 0xe0, 0xe6, 0x7f, 0x75, 0x26, 0x08,
    0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x02, 0xb0, 0x00, 0x00, 0x00, 0x34, 0xa4, 0x92, 0x49, 0x81,
    0x24, 0x4a, 0x3f, 0x92, 0x51, 0x00, 0x00, 0x04, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc9, 0xcf, 0x4c, 0xc6, 0xb4, 0xf2, 0x59, 0x89,
    0x0a, 0x7b, 0x6b, 0xf2, 0xaf, 0xbd, 0x43, 0x32, 0x39, 0x5c, 0x4d, 0xde, 0xbe, 0x48, 0x5a, 0xf9,
    0xf9, 0xab, 0x25, 0xc1, 0xe0, 0xc2, 0xe4, 0x9f, 0xfb, 0xd5, 0xe9, 0xf6, 0x66, 0x3f, 0x2e, 0x9d,
    0x16, 0x46, 0x3a, 0x2a, 0xe8, 0x64, 0x46, 0xf8, 0xde, 0xe3, 0x2d, 0x02, 0xc4, 0x82, 0xe8, 0xd6,
    0x0a, 0x57, 0xb5, 0x9d, 0x2f, 0x99, 0x92, 0x86, 0x5b, 0x54, 0x9f, 0x4d, 0x6b, 0x1b, 0x3d, 0x7d,
    0x4d, 0xb4, 0x6b, 0xd9, 0x2c, 0x4f, 0x54, 0x48, 0x02, 0x3e, 0xdd, 0x45, 0xd1, 0x2c, 0xe1, 0x4e,
    0xd8, 0xc5, 0xb0, 0x69, 0xc9, 0x97, 0x85, 0x7b, 0xd0, 0x26, 0x66, 0xc1, 0x4d, 0x0f, 0x6b, 0x75,
    0x75, 0x5d, 0x5e, 0x16, 0x57, 0x80, 0xe0, 0x24, 0x11, 0x86, 0x2d, 0x3f, 0x57, 0xdc, 0x68, 0xa7,
    0x39, 0xbf, 0x87, 0x83, 0x1d, 0x4c, 0xe4, 0x3f, 0xf3, 0xb1, 0x5d, 0x52, 0x5a, 0x0e, 0x8c, 0xdf,
    0xe3, 0xb2, 0x76, 0x2c, 0x06, 0x2c, 0x3d, 0x5b, 0x07, 0x14, 0xa4, 0x32, 0x2b, 0x24, 0xae, 0xcb,
    0x98, 0x92, 0xc7, 0x54, 0xab, 0x43, 0x09, 0x2a, 0x20, 0x76, 0x76, 0x4a, 0xba, 0xab, 0xca, 0xdb,
    0x46, 0x24, 0x9b, 0xf8, 0x00, 0xb8, 0x1d, 0x78, 0x54, 0x43, 0xcd, 0x6a, 0x3b, 0x26, 0xd5, 0x66,
    0xc6, 0x13, 0x67, 0x32, 0xc0, 0xed, 0x44, 0xb9, 0xca, 0xb9, 0xf9, 0x6e, 0xd5, 0xa1, 0x55, 0x91,
    0xb7, 0x05, 0x2f, 0xa1, 0x15, 0xb3, 0x62, 0xb9, 0x15, 0x86, 0x44, 0x5e, 0x33, 0xa1, 0x38, 0x90,
    0xb9, 0xc7, 0xc8, 0x1d, 0xaf, 0x54, 0x53, 0x22, 0x58, 0xa5, 0x8a, 0x6f, 0xb0, 0x20, 0xbd, 0xe2,
    0xa0, 0x83, 0x8d, 0xe0, 0xfe, 0x62, 0x32, 0xd8, 0xbb, 0x2a, 0x8f, 0x2a, 0xa9, 0x3f, 0x7d, 0x94,
    0xbc, 0xa6, 0xd9, 0x0d, 0x49, 0x1d, 0x65, 0x2a, 0x2c, 0x56, 0xda, 0x72, 0x2c, 0x73, 0xb3, 0xae,
    0x79, 0x44, 0x2a, 0xfa, 0x09, 0x57, 0x3f, 0xdf, 0xeb, 0x5b, 0xd6, 0x72, 0xfd, 0xc5, 0x37, 0xbd,
    0x75, 0xb0, 0x49, 0xdc, 0xf2, 0xc6, 0xf8, 0xc1, 0xc5, 0x32, 0x43, 0xb0, 0xec, 0xc4, 0x9e, 0x10,
    0x29, 0xb4, 0x6c, 0x43, 0x4e, 0x5d, 0x12, 0x72, 0xf2, 0x84, 0x83, 0xd9, 0x2c, 0x22, 0x6b, 0x51,
    0x02, 0x07, 0x97, 0xbf, 0x17, 0x77, 0x02, 0xff, 0x71, 0x96, 0x5d, 0x9f, 0x3e, 0x31, 0x0e, 0xb2,
    0x5d, 0xac, 0xa5, 0x0a, 0x57, 0xd9, 0xbb, 0x12, 0x56, 0x8a, 0x93, 0x2f, 0xa3, 0xe6, 0x5a, 0x1f,
    0xde, 0x5f, 0xaf, 0xc6, 0x3e, 0xa3, 0x85, 0x79, 0x25, 0x18, 0xf4, 0xe2, 0xf1, 0x11, 0xa7, 0xa3,
    0x5a, 0xf5, 0x5f, 0x44, 0xcf, 0xc6, 0x65, 0x6b, 0x26, 0x10, 0x96, 0xa8, 0x06, 0xf8, 0x8e, 0x39,
    0x3e, 0x3c, 0xc9, 0xbc, 0x99, 0xfd, 0x35, 0xa8, 0x38, 0x70, 0x0e, 0xad, 0x24, 0xf3, 0x6c, 0xd1,
    0x3c, 0x72, 0xc7, 0x19, 0xc4, 0xce, 0x86, 0x4e, 0x80, 0xf5, 0xfd, 0xc6, 0xa6, 0x23, 0x9e, 0x59,
    0x08, 0x58, 0x3a, 0x44, 0x37, 0x12, 0x7a, 0x80, 0x07, 0xa2, 0x16, 0x3d, 0xf8, 0xc8, 0xa3, 0x19,
    0x56, 0x72, 0xf3, 0x84, 0xe4, 0x58, 0xb5, 0x40, 0x21, 0x38, 0xa1, 0xa2, 0xe4, 0x28, 0x5e, 0x66,
    0x3e, 0x34, 0x08, 0x1e, 0x8d, 0x22, 0x97, 0x44, 0x0e, 0xe8, 0x43, 0x93, 0x31, 0x85, 0xc1, 0x7e,
    0x04, 0xe2, 0x31, 0xc8, 0x1c, 0x8c, 0xe7, 0x76, 0xb5, 0x37, 0xe5, 0xbc, 0x63, 0xda, 0x4f, 0xa5,
    0xe2, 0xbd, 0x5b, 0x91, 0x77, 0xb0, 0x1f, 0x4c, 0x7d, 0xfd, 0x92, 0xf7, 0x09, 0x58, 0xe4, 0x74,
    0x80, 0xf1, 0x82, 0x4e, 0xb9, 0x3f, 0xf8, 0x79, 0xd0, 0x3f, 0x60, 0x51, 0xa8, 0xd3, 0xc9, 0x66,
    0x6f, 0x2a, 0xd0, 0x36, 0x3c, 0x55, 0xb3, 0x24, 0x66, 0x12, 0x4e, 0xfb, 0x5f, 0xfd, 0x35, 0xbf,
    0xe6, 0x22, 0xf1, 0x92, 0x02, 0xc6, 0x3a, 0x0a, 0x78, 0x48, 0x6f, 0x9e, 0x40, 0x25, 0xf8, 0x8d,
    0xe1, 0xdc, 0x2e, 0x66, 0x7a, 0x52, 0x3c, 0xca, 0xf6, 0x6a, 0x30, 0xb6, 0x06, 0xc3, 0x15, 0x18,
    0xe6, 0x32, 0xb4, 0x29, 0xc2, 0xec, 0xe7, 0xde, 0x48, 0x5c, 0xd4, 0x4c, 0x5a, 0x6f, 0x37, 0x8d,
    0x90, 0x74, 0x0c, 0xae, 0xa8, 0x54, 0x67, 0x08, 0x5d, 0xb6, 0x00, 0x13, 0x14, 0xe7, 0x6b, 0x9a,
    0xb5, 0x49, 0x05, 0xb9, 0x29, 0x13, 0x94, 0x29, 0x7c, 0xde, 0x6b, 0xd2, 0x04, 0x72, 0xd6, 0x81,
    0xac, 0xa3, 0xa5, 0x6b, 0x31, 0x53, 0x63, 0x36, 0x68, 0xde, 0x6d, 0x17, 0xef, 0xb7, 0x65, 0xae,
    0x2a, 0x73, 0x46, 0xa4, 0xd2, 0xce, 0x70, 0x7b, 0x79, 0xe9, 0x17, 0xae, 0x1f, 0x30, 0x57, 0x1a,
    0x4c, 0x7d, 0x03, 0xb4, 0x92, 0x8c, 0x1b, 0xc9, 0x1b, 0x9a, 0x85, 0x5d, 0xcb, 0x06, 0x95, 0xf8,
    0x73, 0x53, 0x71, 0x7f, 0x08, 0x1d, 0x28, 0xfc, 0x01, 0x94, 0x3f, 0xdd, 0x9d, 0x1e, 0xb0, 0x36,
    0x43, 0xeb, 0x5b, 0x15, 0xec, 0x8d, 0xfc, 0x54, 0xc0, 0xd8, 0x66, 0xb6, 0xae, 0xcb, 0x45, 0x26,
    0xf1, 0x38, 0x57, 0x48, 0x70, 0xcb, 0x2f, 0x12, 0x00, 0x00, 0x00, 0x26, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x28, 0x98, 0x0d, 0x77, 0xbc, 0x3c, 0x99, 0x3c, 0x72, 0xb8, 0x33, 0xc8, 0x31,
    0x38, 0x38, 0x31, 0xc4, 0xe7, 0x01, 0x9b, 0x5d, 0x5e, 0xb3, 0xcd, 0x6f, 0x2d, 0x3f, 0x5b, 0x6d,
    0xe6, 0xca, 0xd6, 0xf0, 0xd7, 0x2a, 0xc1, 0x94, 0x5a, 0x8d, 0xb7, 0x9b,
};

static const char *const g_msCapsClock1[9] = {
    "P_PlayAVOUT    ",
    "P_PlayAVOUT2   ",
    "P_PlayPeriod1  ",
    "P_PlayPeriod2  ",
    "T_ClockType1   ",
    "T_MGR_MS       ",
    "T_MagicGateV   ",
    "T_SVR          ",
    "V_SCEI_PSP     ",
};

static const char *const g_msCapsClock2[10] = {
    "P_PlayAVOUT    ",
    "P_PlayAVOUT2   ",
    "P_PlayPeriod1  ",
    "P_PlayPeriod2  ",
    "T_ClockType1   ",
    "T_ClockType2   ",
    "T_MGR_MS       ",
    "T_MagicGateV   ",
    "T_SVR          ",
    "V_SCEI_PSP     ",
};

static const char *const g_hddCapsClock1[9] = {
    "P_PlayAVOUT    ",
    "P_PlayAVOUT2   ",
    "P_PlayPeriod1  ",
    "P_PlayPeriod2  ",
    "T_ClockType1   ",
    "T_MGR_HDD      ",
    "T_MagicGateV   ",
    "T_SVR          ",
    "V_SCEI_PSP     ",
};

static const char *const g_hddCapsClock2[10] = {
    "P_PlayAVOUT    ",
    "P_PlayAVOUT2   ",
    "P_PlayPeriod1  ",
    "P_PlayPeriod2  ",
    "T_ClockType1   ",
    "T_ClockType2   ",
    "T_MGR_HDD      ",
    "T_MagicGateV   ",
    "T_SVR          ",
    "V_SCEI_PSP     ",
};

static const MgvCapList g_msCapLists[2] = {
    { g_msCapsClock1, 9 },
    { g_msCapsClock2, 10 },
};

static const MgvCapList g_hddCapLists[2] = {
    { g_hddCapsClock1, 9 },
    { g_hddCapsClock2, 10 },
};

static const char *const g_tagsBind[2]   = { "!REFID      ", "BIND_L      " };
static const char *const g_tagsCopy[2]   = { "!REFID      ", "COPY_L      " };
static const char *const g_tagsMove[2]   = { "!REFID      ", "MOVE_L      " };
static const char *const g_tagsPlay[5]   = { "!CID        ", "!REFID      ", "PLAY_L      ", "PLAY_P      ", "PLAY_S      " };
static const char *const g_tagsPlay4[4]  = { "!REFID      ", "PLAY_L      ", "PLAY_P      ", "PLAY_S      " };
static const char *const g_tagsPerm4[4]  = { "!REFID      ", "BIND_L      ", "COPY_L      ", "MOVE_L      " };

/* =========================================================================
 * Global State (BSS & Data)
 * ========================================================================= */

static u8 g_pageBuf[1024];
static int g_initRefCount = 0;
static u8 g_rngCtx[16];
static int g_mediaFplIds[2] = { -1, -1 };
static MgvMediaCtx *g_mediaCtxs[2] = { NULL, NULL };
static MgvContentCtx g_contentCtxs[2];

static int mgvSysEventHandler(int evId, const char *evName, void *param, int *result);

static SceSysEventHandler g_sysEventHandler = {
    .size = 0x40,
    .name = "SceMgVideo",
    .typeMask = 0x00FF0000,
    .handler = mgvSysEventHandler,
};

/* =========================================================================
 * Error Translation & Address Validation Helpers
 * ========================================================================= */

static int mapMgrError(int err) {
    if (err == (int)0x80501001) return MGV_ERR_MGR_ERROR_21;
    if (err < (int)0x80501001) {
        if (err == (int)0x80500001) return MGV_ERR_MGR_ERROR_32;
        if (err == (int)0x80500002) return MGV_ERR_NOT_OPENED;
        if ((u32)(err - (int)0x804FF000) < 3u || (u32)(err - (int)0x80500000) < 3u)
            return MGV_ERR_IO_ERROR;
        return err;
    }
    if (err == (int)0x80501005) return MGV_ERR_NOT_OPENED;
    if (err > (int)0x80501005) {
        if (err == (int)0x80501007) return MGV_ERR_MGR_ERROR_26;
        if (err < (int)0x80501007)  return MGV_ERR_MGR_ERROR_25;
        if (err == (int)0x80501009) return MGV_ERR_MGR_ERROR_27;
        return err;
    }
    if (err == (int)0x80501003) return MGV_ERR_MGR_ERROR_24;
    if (err < (int)0x80501003)  return MGV_ERR_MGR_ERROR_22;
    return MGV_ERR_MGR_ERROR_23;
}

static int mapRuleError(int err) {
    switch (err) {
        case -1:  return MGV_ERR_INVALID_DATA;
        case -2:  return MGV_ERR_RULE_M2;
        case -3:  return MGV_ERR_RULE_M3;
        case -4:  return MGV_ERR_RULE_M4;
        case -5:  return MGV_ERR_RULE_M5;
        case -6:  return MGV_ERR_RULE_M6;
        case -7:  return MGV_ERR_RULE_M7;
        case -8:  return MGV_ERR_RULE_M8;
        case -15: return MGV_ERR_NEED_PLAY_START;
        default:  return err;
    }
}

static int checkBoxSizeAddr(const u32 *boxPtr) {
    u32 k1 = getK1();
    u32 addr = (u32)(uintptr_t)boxPtr;
    if ((k1 & addr & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    u32 sz = bswap32(*boxPtr);
    if ((k1 & ((addr + sz) | addr | sz) & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return (int)sz;
}

static int copyBufDesc(MgvBufDesc *dst, const MgvBufDesc *src) {
    u32 k1 = getK1();
    u32 srcAddr = (u32)(uintptr_t)src;
    if (src == NULL) {
        dst->ptr = NULL;
        dst->size = 0;
        return 0;
    }
    if ((k1 & srcAddr & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    dst->ptr = src->ptr;
    dst->size = src->size;
    u32 p = (u32)(uintptr_t)dst->ptr;
    u32 s = dst->size;
    if (p == 0)
        return 0;
    if ((k1 & ((p + s) | p | s) & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return 0;
}

static int checkUserRangeUncached(const void *ptr, u32 size) {
    u32 k1 = getK1();
    u32 addr = (u32)(uintptr_t)ptr & 0x9FFFFFFFu;
    if ((k1 & ((addr + size) | addr | size) & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return 0;
}

static int checkUserRange(const void *ptr, u32 size) {
    u32 k1 = getK1();
    u32 addr = (u32)(uintptr_t)ptr;
    if ((k1 & ((addr + size) | addr | size) & 0x80000000u) != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return 0;
}

static int stripEkb2Header(MgvBufDesc *desc) {
    if (desc->ptr == NULL)
        return MGV_ERR_INVALID_ARG;
    if (desc->size < 0x50u)
        return MGV_ERR_BAD_EKB;
    if (*(const u32 *)desc->ptr != 0x32424B45u) /* "EKB2" */
        return MGV_ERR_BAD_EKB;
    desc->ptr = (u8 *)desc->ptr + 32;
    desc->size -= 32;
    return 0;
}

static int checkIkvDesc(const MgvBufDesc *desc) {
    if (desc->ptr == NULL) return MGV_ERR_INVALID_ARG;
    if (desc->size != 0x3EB0u) return MGV_ERR_INVALID_DATA;
    return 0;
}

static int checkLckDesc(const MgvBufDesc *desc) {
    if (desc->ptr == NULL) return MGV_ERR_INVALID_ARG;
    if (desc->size < 0x50u) return MGV_ERR_INVALID_DATA;
    return 0;
}

static int checkMinDesc(const MgvBufDesc *desc, u32 minSize) {
    if (desc->ptr == NULL) return MGV_ERR_INVALID_ARG;
    if (desc->size < minSize) return MGV_ERR_INVALID_DATA;
    return 0;
}

/* =========================================================================
 * BCD Time & RTC Helpers
 * ========================================================================= */

static int bcdToDec(u8 b) {
    u32 lo = b & 0xFu;
    u32 hi = b >> 4;
    if (lo < 10 && hi < 10)
        return (int)(10u * hi + lo);
    return -1;
}

static int bcd2ToDec(u16 w) {
    int lo = bcdToDec((u8)w);
    if (lo < 0) return -1;
    int hi = bcdToDec((u8)(w >> 8));
    if (hi < 0) return -1;
    return 100 * hi + lo;
}

static int bcdTimeToTick(u64 *tickOut, const u8 *bcd) {
    if (bcd == NULL) {
        *tickOut = 0;
        return 0;
    }
    struct {
        u16 year;
        u16 month;
        u16 day;
        u16 hour;
        u16 minute;
        u16 second;
        u32 microsecond;
    } dt;
    u16 rawYear;
    memcpy(&rawYear, bcd, 2);
    int yr = bcd2ToDec(bswap16(rawYear));
    if ((u32)(yr - 1) >= 9999u) return -1;
    dt.year = (u16)yr;
    int mo = bcdToDec(bcd[2]);
    if ((u32)(mo - 1) >= 12u) return -1;
    dt.month = (u16)mo;
    int dy = bcdToDec(bcd[3]);
    if ((u32)(dy - 1) >= 31u) return -1;
    dt.day = (u16)dy;
    int hr = bcdToDec(bcd[4]);
    if ((u32)hr >= 24u) return -1;
    dt.hour = (u16)hr;
    int mn = bcdToDec(bcd[5]);
    if ((u32)mn >= 60u) return -1;
    dt.minute = (u16)mn;
    int sc = bcdToDec(bcd[6]);
    if ((u32)sc >= 62u) return -1;
    dt.second = (u16)sc;
    int cs = bcdToDec(bcd[7]);
    u32 us = (u32)(10000 * cs);
    if (us > 999999u) return -1;
    dt.microsecond = us;
    if (sceRtcGetTick(&dt, tickOut) < 0)
        return -1;
    return 0;
}

static u8 decToBcd(u8 v) {
    return (u8)(((v / 10u) << 4) | (v % 10u));
}

static void pspTimeToBcd(u8 *bcd, const u16 *dt) {
    bcd[0] = decToBcd((u8)(dt[0] / 100u));
    bcd[1] = decToBcd((u8)(dt[0] % 100u));
    bcd[2] = decToBcd((u8)dt[1]);
    bcd[3] = decToBcd((u8)dt[2]);
    bcd[4] = decToBcd((u8)dt[3]);
    bcd[5] = decToBcd((u8)dt[4]);
    bcd[6] = decToBcd((u8)dt[5]);
    u32 us;
    memcpy(&us, dt + 6, 4);
    bcd[7] = decToBcd((u8)(us / 10000u));
}

static int addHoursToBcdTime(u8 *bcd, int hours) {
    u64 tick;
    u16 dt[8];
    if (bcdTimeToTick(&tick, bcd) < 0)
        return -2;
    int ret = sceRtc_driver_8413CADC(&tick, &tick, hours);
    if (ret < 0) return ret;
    ret = sceRtcSetTick(dt, &tick);
    if (ret < 0) return ret;
    pspTimeToBcd(bcd, dt);
    return 0;
}

static int getCurrentBcdTime(u8 *bcd, int secureClock) {
    u64 tick;
    u16 dt[8];
    int ret = secureClock ? sceRtc_driver_CEEF238F(&tick) : sceRtcGetCurrentTick(&tick);
    if (ret < 0) return -3;
    ret = sceRtcSetTick(dt, &tick);
    if (ret < 0) return ret;
    pspTimeToBcd(bcd, dt);
    return 0;
}

static int setSecureClockFromBcd(const u8 *bcd) {
    u64 tick;
    if (bcdTimeToTick(&tick, bcd) < 0)
        return -2;
    if (sceRtc_driver_66054C2A(&tick) < 0)
        return -1;
    return 0;
}

/* =========================================================================
 * License / Rule Engine (KEYRING, !CID, !REFID, PLAY_L/P/S, BIND/COPY/MOVE)
 * ========================================================================= */

static const u8 *findParamInSection(const u8 *data, int len, const char *key8) {
    if (data == NULL)
        return NULL;
    while (len >= 16) {
        if (memcmp(data, key8, 8) == 0)
            return data + 8;
        data += 16;
        len -= 16;
    }
    return NULL;
}

static const MgvCapList *getMediaCapLists(int mediaType) {
    if (mediaType == 0) return g_msCapLists;
    if (mediaType == 1) return g_hddCapLists;
    return NULL;
}

static int checkPlayPeriod2(const u8 *playS, const u8 *curTime) {
    const u8 *data = playS ? (playS + 16) : NULL;
    int len = playS ? (16 * readBe16(playS + 14)) : 0;

    const u8 *endT2 = findParamInSection(data, len, "end_t2  ");
    if (endT2 == NULL || memcmp(g_maxBcdTime, endT2, 8) == 0)
        return -15;
    const u8 *startT2 = findParamInSection(data, len, "start_t2");
    if (startT2 == NULL)
        startT2 = g_maxBcdTime;
    if (curTime == NULL)
        return -3;
    if (memcmp(curTime, startT2, 8) >= 0)
        return (memcmp(curTime, endT2, 8) <= 0) ? 0 : -5;
    return (memcmp(startT2, endT2, 8) <= 0) ? -4 : -5;
}

static int checkPlayPeriod(const u8 *playP, const u8 *playS, const u8 *curTime) {
    const u8 *data = playP ? (playP + 16) : NULL;
    int len = playP ? (16 * readBe16(playP + 14)) : 0;

    const u8 *period = findParamInSection(data, len, "period  ");
    if (period == NULL)
        period = g_zeroPeriod;
    if (readBe16(period + 6) != 0) {
        int r = checkPlayPeriod2(playS, curTime);
        if (r < 0)
            return r;
    }

    const u8 *startT = findParamInSection(data, len, "start_t ");
    if (startT == NULL)
        startT = g_minBcdTime;
    const u8 *endT = findParamInSection(data, len, "end_t   ");
    if (endT == NULL)
        endT = g_maxBcdTime;

    if (curTime == NULL) {
        if (memcmp(g_minBcdTime, startT, 8) != 0)
            return -3;
        return (memcmp(g_maxBcdTime, endT, 8) == 0) ? 0 : -3;
    }
    if (memcmp(curTime, startT, 8) >= 0)
        return (memcmp(curTime, endT, 8) <= 0) ? 0 : -5;
    return (memcmp(startT, endT, 8) <= 0) ? -4 : -5;
}

static int getPlayParams(const u8 *playP, const u8 **periodOut, const u8 **startOut,
                         const u8 **endOut, const u8 **avoutOut) {
    const u8 *data = playP ? (playP + 16) : NULL;
    int len = playP ? (16 * readBe16(playP + 14)) : 0;
    if (periodOut) *periodOut = findParamInSection(data, len, "period  ");
    if (startOut)  *startOut  = findParamInSection(data, len, "start_t ");
    if (endOut)    *endOut    = findParamInSection(data, len, "end_t   ");
    if (avoutOut)  *avoutOut  = findParamInSection(data, len, "avout   ");
    return 0;
}

static int getPlayStateParams(const u8 *playS, const u8 **start2Out, const u8 **end2Out) {
    const u8 *data = playS ? (playS + 16) : NULL;
    int len = playS ? (16 * readBe16(playS + 14)) : 0;
    if (start2Out) *start2Out = findParamInSection(data, len, "start_t2");
    if (end2Out)   *end2Out   = findParamInSection(data, len, "end_t2  ");
    return 0;
}

static int findSections(const u8 *buf, int bufLen, const char *const *names, const u8 **outPtrs, int count) {
    int i, found = 0;
    const u8 *end = buf + bufLen;
    for (i = 0; i < count; i++)
        outPtrs[i] = NULL;
    if (bufLen < 17 || count < 1)
        return 0;

    while (bufLen >= 17) {
        int bodyLen = 16 * readBe16(buf + 14);
        for (i = 0; i < count; i++) {
            if (outPtrs[i] == NULL && names[i] != NULL && memcmp(buf, names[i], 12) == 0) {
                if (buf + bodyLen <= end) {
                    outPtrs[i] = buf;
                    found++;
                }
            }
        }
        buf += bodyLen + 16;
        bufLen -= bodyLen + 16;
    }
    return found;
}

static int extractCid(u8 *cidOut, const u8 *cidSec) {
    if (cidSec == NULL) return -2;
    if ((u32)(16 * readBe16(cidSec + 14)) < 32u) return -2;
    memcpy(cidOut, cidSec + 16, 24);
    return 16;
}

static int extractRefId(u8 *refIdOut, const u8 *refSec) {
    if (refSec == NULL) return 0;
    if ((u32)(16 * readBe16(refSec + 14)) < 16u) return -2;
    memcpy(refIdOut, refSec + 16, 16);
    if (refIdOut[0] == 2)
        return 16;
    int i;
    for (i = 0; i < 4; i++) {
        if (refIdOut[i] != 0)
            return -2;
    }
    return 0;
}

static int checkRefPlayFlag(const u8 *ref) {
    return (ref[1] == 0) ? 0 : -2;
}

static int checkRefBindFlag(const u8 *ref) {
    u8 v = ref[2];
    if (v == 0) return 0;
    if (v == 2 || v == 3) return 1;
    return -2;
}

static int checkRefTransferFlag(const u8 *ref, int op) {
    u8 v = ref[3];
    if (v == 0) return 0;
    if (v == 2) return (op == 4) ? 1 : -2;
    if (v == 3) return 1;
    return -2;
}

static int capListContains(const u8 *cap15, const MgvCapList *list) {
    int lo = 0;
    int hi = list->count - 1;
    while (lo <= hi) {
        int mid = lo + ((hi - lo) >> 1);
        int cmp = memcmp(cap15, list->entries[mid], 15);
        if (cmp == 0) return 1;
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return 0;
}

static int mapMissingCapError(const u8 *cap15) {
    if (memcmp(cap15, "P_PlayPeriod1  ", 15) == 0 ||
        memcmp(cap15, "P_PlayPeriod2  ", 15) == 0 ||
        memcmp(cap15, "T_ClockType1   ", 15) == 0 ||
        memcmp(cap15, "T_ClockType2   ", 15) == 0)
        return -3;
    if (memcmp(cap15, "T_Grouping     ", 15) == 0 ||
        memcmp(cap15, "P_ShareChkSID  ", 15) == 0)
        return -9;
    if (memcmp(cap15, "P_BindCount    ", 15) == 0 ||
        memcmp(cap15, "P_CopyCount    ", 15) == 0)
        return -6;
    if (cap15[0] == 'V' && cap15[1] == '_')
        return -8;
    return -2;
}

static int evalRuleList(const u8 *sec, const MgvCapList *list) {
    const u8 *cur = sec + 16;
    const u8 *end = cur + 16 * readBe16(sec + 14);
    const u8 *last = NULL;

    if (cur >= end)
        return -2;

    while (cur < end) {
        u8 op = cur[15];
        if ((op & 0x80u) == 0) {
            if (op == 0) return -2;
            if (op == 1) return 0;
            last = cur;
            cur += capListContains(cur, list) ? 16 : (16 * op);
        } else {
            last = cur;
            switch (op) {
                case 0x80:
                    if (capListContains(cur, list)) return -2;
                    cur += 16;
                    break;
                case 0x81:
                    if (capListContains(cur, list)) return 0;
                    cur += 16;
                    break;
                case 0x82:
                    if (!capListContains(cur, list))
                        return mapMissingCapError(cur);
                    cur += 16;
                    break;
                case 0x83:
                    if (!capListContains(cur, list))
                        return 0;
                    cur += 16;
                    break;
                default:
                    return -2;
            }
        }
    }
    if (last != NULL && last[15] == 0x81)
        return mapMissingCapError(last);
    return -2;
}

static int verifyRefPlay(const u8 *refSec, const u8 *playL) {
    u8 ref[16];
    int r = extractRefId(ref, refSec);
    if (r < 0) return r;
    int flag = 0;
    if (r != 0) {
        flag = checkRefPlayFlag(ref);
        if (flag < 0) return flag;
    }
    if (playL != NULL && flag != 0) return -1;
    if (playL == NULL && flag == 0) return -2;
    return flag;
}

static int evalPlayRules(const MgvCapList *lists, const u8 *refSec, const u8 *playL,
                         const u8 *playP, const u8 *playS) {
    u8 curTime[16];
    int r = verifyRefPlay(refSec, playL);
    if (r < 0) return r;
    r = evalRuleList(playL, &lists[1]);
    if (r < 0) return r;

    const u8 *tp = (getCurrentBcdTime(curTime, 1) >= 0) ? curTime : NULL;
    r = checkPlayPeriod(playP, playS, tp);
    if (r >= 0) return 0;

    if (evalRuleList(playL, &lists[0]) >= 0) {
        tp = (getCurrentBcdTime(curTime, 0) >= 0) ? curTime : NULL;
        r = checkPlayPeriod(playP, playS, tp);
        if (r >= 0) return 0;
    }
    return r;
}

static int verifyRefBind(const u8 *refSec, const u8 *bindL) {
    u8 ref[16];
    int r = extractRefId(ref, refSec);
    if (r < 0) return r;
    int flag = 0;
    if (r != 0) {
        flag = checkRefBindFlag(ref);
        if (flag < 0) return flag;
    }
    if (bindL != NULL && flag != 0) return -1;
    if (bindL == NULL && flag == 0) return -2;
    return flag;
}

static int evalBindRules(const MgvCapList *lists, const u8 *refSec, const u8 *bindL) {
    int r = verifyRefBind(refSec, bindL);
    if (r < 0) return r;
    if (bindL == NULL) return 0;
    r = evalRuleList(bindL, &lists[0]);
    return (r >= 0) ? 0 : r;
}

static int verifyRefTransfer(const u8 *refSec, const u8 *ruleL, int op) {
    u8 ref[16];
    int r = extractRefId(ref, refSec);
    if (r < 0) return r;
    int flag = 0;
    if (r != 0) {
        flag = checkRefTransferFlag(ref, op);
        if (flag < 0) return flag;
    }
    if (ruleL != NULL && flag != 0) return -1;
    if (ruleL == NULL && flag == 0) return -2;
    return flag;
}

static int evalTransferRules(const MgvCapList *lists, const u8 *refSec, const u8 *ruleL, int op) {
    int r = verifyRefTransfer(refSec, ruleL, op);
    if (r < 0) return r;
    if (ruleL == NULL) return 0;
    r = evalRuleList(ruleL, &lists[0]);
    return (r >= 0) ? 0 : r;
}

static int checkBindLicense(int mediaType, const u8 *data, int len) {
    const u8 *secs[2];
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL) return -1;
    int r = findSections(data, len, g_tagsBind, secs, 2);
    if (r < 0) return r;
    r = evalBindRules(lists, secs[0], secs[1]);
    return (r < 0) ? r : 0;
}

static int checkCopyLicense(int mediaType, const u8 *data, int len) {
    const u8 *secs[2];
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL) return -1;
    int r = findSections(data, len, g_tagsCopy, secs, 2);
    if (r < 0) return r;
    return evalTransferRules(lists, secs[0], secs[1], 3);
}

static int checkMoveLicense(int mediaType, const u8 *data, int len) {
    const u8 *secs[2];
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL) return -1;
    int r = findSections(data, len, g_tagsMove, secs, 2);
    if (r < 0) return r;
    r = evalTransferRules(lists, secs[0], secs[1], 4);
    return (r < 0) ? r : 0;
}

static int checkPlayLicense(int mediaType, u8 *cidOut, const u8 *data, int len) {
    const u8 *secs[5];
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL) return -1;
    int r = findSections(data, len, g_tagsPlay, secs, 5);
    if (r < 0) return r;
    if (cidOut != NULL) {
        r = extractCid(cidOut, secs[0]);
        if (r < 0) return r;
    }
    r = evalPlayRules(lists, secs[1], secs[2], secs[3], secs[4]);
    return (r < 0) ? r : 0;
}

static int getPlayClockType(const MgvCapList *lists, const u8 *refSec, const u8 *playL) {
    int r = verifyRefPlay(refSec, playL);
    if (r < 0) return r;
    r = evalRuleList(playL, &lists[1]);
    if (r < 0) return r;
    return (evalRuleList(playL, &lists[0]) >= 0) ? 1 : 2;
}

static int queryPlayInfo(const MgvCapList *lists, u32 *out, const u8 *data, int len) {
    const u8 *secs[4];
    const u8 *period, *startT, *endT, *avout, *startT2, *endT2;
    int r = findSections(data, len, g_tagsPlay4, secs, 4);
    if (r < 0) return r;
    int clkType = getPlayClockType(lists, secs[0], secs[1]);
    if (clkType < 0) return clkType;

    u64 nowTick;
    int tr = (clkType < 2) ? sceRtcGetCurrentTick(&nowTick) : sceRtc_driver_CEEF238F(&nowTick);
    if (tr < 0) nowTick = 0;
    memcpy(out, &nowTick, 8);

    getPlayParams(secs[2], &period, &startT, &endT, &avout);
    getPlayStateParams(secs[3], &startT2, &endT2);

    u64 tmp;
    if (bcdTimeToTick(&tmp, startT) < 0) return -1;
    memcpy(out + 2, &tmp, 8);
    if (bcdTimeToTick(&tmp, endT) < 0) return -1;
    memcpy(out + 4, &tmp, 8);

    if (avout == NULL) avout = g_defaultAvout;
    memcpy(out + 6, avout, 8);

    if (period == NULL) period = g_zeroPeriod;
    out[12] = 3600u * readBe16(period + 6);

    if (bcdTimeToTick(&tmp, startT2) < 0) return -1;
    memcpy(out + 8, &tmp, 8);
    if (bcdTimeToTick(&tmp, endT2) < 0) return -1;
    memcpy(out + 10, &tmp, 8);
    return 0;
}

static int queryPermInfo(const MgvCapList *lists, u32 *out, const u8 *data, int len, int op) {
    const u8 *secs[4];
    int r = findSections(data, len, g_tagsPerm4, secs, 4);
    if (r < 0) return r;

    int copyOk = -1, moveOk = -1;
    if (op != 4) {
        r = evalTransferRules(lists, secs[0], secs[2], 3);
        if (r < 0 && op == 3) return r;
        copyOk = r;
    }
    if (op != 3) {
        r = evalTransferRules(lists, secs[0], secs[3], 4);
        if (r < 0 && op == 4) return r;
        moveOk = r;
    }
    r = evalBindRules(lists, secs[0], secs[1]);
    if (r < 0) {
        if (r != -7) return r;
        copyOk = -7;
        moveOk = -7;
    }
    out[0] = (u32)-1;
    out[1] = 0;
    out[2] = 0;
    if (op == 2) {
        out[3] = (copyOk >= 0) ? 1u : 0u;
        out[4] = (moveOk >= 0) ? 1u : 0u;
    }
    return 0;
}

static int queryLicenseInfo(int mediaType, int queryType, u32 *out, const u8 *data, int len) {
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL) return -1;
    if (queryType == 1) return queryPlayInfo(lists, out, data, len);
    if (queryType >= 2 && queryType <= 4) return queryPermInfo(lists, out, data, len, queryType);
    return -1;
}

static int insertPlayStartTimeFields(u8 *base, int curLen, u32 maxLen, u8 *playS,
                                     u8 **start2InOut, u8 **end2InOut) {
    if (playS != NULL) {
        if (*start2InOut == NULL && *end2InOut == NULL) {
            u32 newLen = (u32)curLen + 32u;
            if (maxLen < newLen) return -1;
            memmove(playS + 48, playS + 16, (size_t)((base + curLen) - (playS + 16)));
            u16 blocks = (u16)(readBe16(playS + 14) + 2);
            playS[14] = (u8)(blocks >> 8);
            playS[15] = (u8)blocks;
            memcpy(playS + 16, "start_t2", 8);
            memcpy(playS + 32, "end_t2  ", 8);
            *start2InOut = playS + 24;
            *end2InOut   = playS + 40;
            return (int)newLen;
        }
        u32 newLen = (u32)curLen + 16u;
        if (maxLen < newLen) return -1;
        u8 *insPos = (*start2InOut != NULL) ? (*start2InOut + 8) : (*end2InOut - 8);
        memmove(insPos + 16, insPos, (size_t)((base + curLen) - insPos));
        u16 blocks = (u16)(readBe16(playS + 14) + 1);
        playS[14] = (u8)(blocks >> 8);
        playS[15] = (u8)blocks;
        if (*start2InOut == NULL) {
            memcpy(insPos, "start_t2", 8);
            *start2InOut = insPos + 8;
            *end2InOut += 16;
            return (int)newLen;
        }
        memcpy(insPos, "end_t2  ", 8);
        *end2InOut = insPos + 8;
        return (int)newLen;
    } else {
        u32 newLen = (u32)curLen + 48u;
        if (maxLen < newLen) return -1;
        u8 *p = base + curLen;
        memcpy(p, "PLAY_S      ", 12);
        p[12] = 0;
        p[13] = 8;
        p[14] = 0;
        p[15] = 2;
        memcpy(p + 16, "start_t2", 8);
        memcpy(p + 32, "end_t2  ", 8);
        *start2InOut = p + 24;
        *end2InOut   = p + 40;
        return (int)newLen;
    }
}

static int activatePlayPeriodBody(u8 *base, int curLen, u32 maxLen, const MgvCapList *lists,
                                  const u8 *refSec, const u8 *playL, const u8 *playP, u8 *playS) {
    const u8 *period;
    u8 *start2 = NULL, *end2 = NULL;
    u8 curTime[8];

    getPlayParams(playP, &period, NULL, NULL, NULL);
    if (period == NULL) period = g_zeroPeriod;
    u16 hours = readBe16(period + 6);
    if (hours == 0) return 0;

    int clkType = getPlayClockType(lists, refSec, playL);
    if (clkType < 0) return clkType;
    if (getCurrentBcdTime(curTime, clkType >= 2) < 0)
        return -3;

    getPlayStateParams(playS, (const u8 **)&start2, (const u8 **)&end2);
    if (end2 != NULL && memcmp(g_maxBcdTime, end2, 8) != 0)
        return 0;

    if (playS == NULL || start2 == NULL || end2 == NULL) {
        int newLen = insertPlayStartTimeFields(base, curLen, maxLen, playS, &start2, &end2);
        if (newLen < 0) return newLen;
        curLen = newLen;
    }
    memcpy(start2, curTime, 8);
    addHoursToBcdTime(curTime, hours);
    memcpy(end2, curTime, 8);
    return curLen;
}

static int activatePlayPeriod(int mediaType, u8 *data, u32 curLen, u32 maxLen) {
    const u8 *secs[4];
    const MgvCapList *lists = getMediaCapLists(mediaType);
    if (lists == NULL || maxLen < curLen) return -1;
    int r = findSections(data, (int)curLen, g_tagsPlay4, secs, 4);
    if (r < 0) return r;
    return activatePlayPeriodBody(data, (int)curLen, maxLen, lists,
                                  secs[0], secs[1], secs[2], (u8 *)secs[3]);
}

static u32 measureLicenseLength(const u8 *buf, u32 maxLen) {
    const u8 *end = buf + (maxLen & ~15u);
    const u8 *cur = buf;
    while (cur < end) {
        u32 bodyLen = 16u * readBe16(cur + 14);
        if (bodyLen == 0) {
            int i, allZero = 1;
            for (i = 0; i < 14; i++) {
                if (cur[i] != 0) {
                    allZero = 0;
                    break;
                }
            }
            if (allZero)
                break;
        }
        cur += bodyLen + 16;
    }
    return (u32)(cur - buf);
}

static int buildLicenseSections(u8 *dst, u32 dstSize, const u8 *cid24, const u8 *avout8,
                                int permMode, int noSvr, const char *comment) {
    u8 *p = dst;
    int hasAvout2 = 0;
    if (dstSize < 0xE0u)
        return -1;
    if (avout8[5] & 8u) {
        hasAvout2 = 1;
        noSvr = 0;
    }
    if (comment != NULL) {
        memcpy(p, "!COMMENT    ", 12);
        p[12] = 0; p[13] = 16; p[14] = 0; p[15] = 1;
        memset(p + 16, 0, 16);
        strncpy((char *)(p + 16), comment, 15);
        p += 32;
    }

    memcpy(p, "!CID        ", 12);
    p[12] = 0; p[13] = 16; p[14] = 0; p[15] = 2;
    memcpy(p + 16, cid24, 24);
    memset(p + 40, 0, 8);
    p += 48;

    memcpy(p, "!REFID      ", 12);
    p[12] = 0; p[13] = 16; p[14] = 0; p[15] = 1;
    memset(p + 16, 0, 16);
    p[16] = 2; p[17] = 0; p[18] = 0; p[19] = 0; p[20] = 0;
    if (permMode == 0) {
        p[18] = 2; p[19] = 3; p[20] = 3;
    } else if (permMode == 1) {
        p[18] = noSvr ? 2 : 3;
        p[19] = 3; p[20] = 3;
    } else if (permMode == 2) {
        p[18] = noSvr ? 2 : 3;
        p[19] = 2; p[20] = 3;
    } else {
        return -1;
    }
    p += 32;

    u8 *playL = p;
    memcpy(playL, "PLAY_L      ", 12);
    playL[12] = 0; playL[13] = 16; playL[14] = 0;
    u8 *cur = playL + 16;
    if (!noSvr) {
        memcpy(cur, "T_SVR          ", 15);
        cur[15] = 0x82;
        cur += 16;
    }
    if (hasAvout2) {
        memcpy(cur, "P_PlayAVOUT    ", 15);
        cur[15] = 2;
        cur += 16;
        memcpy(cur, "P_PlayAVOUT2   ", 15);
        cur[15] = 0x82;
        cur += 16;
    }
    u8 cnt = (u8)((cur - playL) >> 4);
    memcpy(cur, "T_MagicGateV   ", 15);
    cur[15] = 0x81;
    playL[15] = cnt;
    cur += 16;

    memcpy(cur, "PLAY_P      ", 12);
    cur[12] = 0; cur[13] = 8; cur[14] = 0; cur[15] = 1;
    memcpy(cur + 16, "avout   ", 8);
    memcpy(cur + 24, avout8, 8);
    cur += 32;
    return (int)(cur - dst);
}

/* =========================================================================
 * MGR Crypto & Keyring / IKV / Page Helpers
 * ========================================================================= */

static int calcMac(void *macOut, const void *data, const void *iv, u32 size, int keySlot) {
    int ret;
    if (keySlot >= 0)
        ret = sceMgr_driver_5C5AB33B(macOut, data, iv, size, g_zero16);
    else
        ret = sceMgr_driver_B53B2B5B(macOut, data, iv, size);
    return (ret < 0) ? mapMgrError(ret) : 0;
}

static int loadEkbKeySlot(const MgvBufDesc *ekbDesc) {
    int slot = sceMgr_driver_C0B28CED(0);
    if (slot < 0)
        return mapMgrError(slot);
    int ret = sceMgr_driver_EDEAAA00(slot, ekbDesc->ptr, ekbDesc->size);
    if (ret < 0) {
        sceMgr_driver_E6A203F8(slot);
        if (ret == (int)0x80501004 || ret == (int)0x80501002)
            return MGV_ERR_BAD_EKB;
        return mapMgrError(ret);
    }
    return slot;
}

static int deriveContentKeySlot(MgvMediaCtx *ctx, const void *encKey, const MgvBufDesc *ekbDesc) {
    int ekbSlot = loadEkbKeySlot(ekbDesc);
    if (ekbSlot < 0)
        return ekbSlot;
    int keySlot = sceMgr_driver_C0B28CED(4);
    int ret = keySlot;
    if (keySlot >= 0)
        ret = ctx->deriveContentKey(keySlot, encKey, ekbSlot);
    if (ret < 0) {
        sceMgr_driver_E6A203F8(ekbSlot);
        if (keySlot >= 0)
            sceMgr_driver_E6A203F8(keySlot);
        return mapMgrError(ret);
    }
    sceMgr_driver_E6A203F8(ekbSlot);
    return keySlot;
}

static int saveContentKey(MgvContentCtx *cctx) {
    int ret = sceMgr_driver_3B548024(cctx->savedKey, cctx->mgrKeySlot);
    if (ret < 0)
        return mapMgrError(ret);
    cctx->suspended = 0;
    return ret;
}

static int restoreContentKey(MgvContentCtx *cctx) {
    int ret = sceMgr_driver_9973CD5B(cctx->mgrKeySlot, cctx->savedKey);
    return (ret < 0) ? mapMgrError(ret) : ret;
}

static int deriveMsgKeySlot(const void *encKey, const MgvBufDesc *ekbDesc) {
    int ekbSlot = loadEkbKeySlot(ekbDesc);
    if (ekbSlot < 0)
        return ekbSlot;
    int keySlot = sceMgr_driver_C0B28CED(4);
    int ret = keySlot;
    if (keySlot >= 0)
        ret = sceMgr_driver_A3A302CC(keySlot, encKey, ekbSlot);
    if (ret < 0) {
        sceMgr_driver_E6A203F8(ekbSlot);
        if (keySlot >= 0)
            sceMgr_driver_E6A203F8(keySlot);
        return mapMgrError(ret);
    }
    sceMgr_driver_E6A203F8(ekbSlot);
    return keySlot;
}

static int parseKeyringHeader(const u8 *kr) {
    if (memcmp(kr, "KEYRING     ", 12) != 0)
        return MGV_ERR_INVALID_DATA;
    if (readBe16(kr + 18) != 3)
        return MGV_ERR_UNSUPPORTED_VER;
    u32 blocks = readBe16(kr + 14);
    if (blocks < 4)
        return MGV_ERR_INVALID_DATA;
    u32 totalBytes = 16u * blocks;
    u32 hdrLen = readBe16(kr + 16);
    if (hdrLen < 0x40u || totalBytes < hdrLen)
        return MGV_ERR_INVALID_DATA;
    u32 keyCount = readBe16(kr + 20);
    if (keyCount > 32u || hdrLen - 32u < 16u * keyCount)
        return MGV_ERR_INVALID_DATA;
    return (int)(totalBytes + 16u);
}

static int generateKeyring(u8 *dst, u8 *keyInfo, int mode) {
    int ret, i;
    if (mode < 0 || mode > 2)
        return MGV_ERR_INVALID_DATA;
    keyInfo[32] = 0;
    keyInfo[33] = (u8)mode;

    for (i = 0; i < 32; i += 8) {
        ret = sceMgr_driver_02117F1E(keyInfo + i, g_rngCtx);
        if (ret < 0) return mapMgrError(ret);
    }

    memcpy(dst, "KEYRING     ", 12);
    dst[12] = 0; dst[13] = 16; dst[14] = 0; dst[15] = 5;
    dst[16] = 0; dst[17] = 80; dst[18] = 0; dst[19] = 3;
    dst[20] = 0; dst[21] = 3;  dst[22] = 0; dst[23] = 16;
    memset(dst + 24, 0, 8);
    memcpy(dst + 32, keyInfo, 16);

    for (i = 2; i >= 0; i--) {
        u8 *entry = dst + 48 + 16 * i;
        ret = sceMgr_driver_02117F1E(entry, g_rngCtx);
        if (ret < 0) return mapMgrError(ret);
        if (mode != 2 && mode != 4) {
            ret = sceMgr_driver_02117F1E(entry + 8, g_rngCtx);
            if (ret < 0) return mapMgrError(ret);
        } else {
            *(u32 *)(entry + 8)  = 0;
            *(u32 *)(entry + 12) = 0xFFFFFFFFu;
        }
    }
    return 16 * readBe16(dst + 14) + 16;
}

static int getMediaId(MgvMediaCtx *ctx, void *out16) {
    int ret = ctx->getMediaId(ctx->mgrHandle, out16);
    return (ret < 0) ? mapMgrError(ret) : 0;
}

static int verifyMediaId(MgvMediaCtx *ctx, const u32 *expected16) {
    u32 id[4];
    int ret = getMediaId(ctx, id);
    if (ret < 0) return ret;
    if (id[0] != expected16[0] || id[1] != expected16[1] ||
        id[2] != expected16[2] || id[3] != expected16[3])
        return MGV_ERR_INVALID_DATA;
    return 0;
}

static int verifyEkbMac(MgvMediaCtx *ctx, int keyIdx, const u32 *ekbMac, const MgvBufDesc *ekbDesc, int ekbSlot) {
    if (keyIdx >= 0) {
        const u32 *cached = (const u32 *)(ctx->ekbMacTable + 8 * keyIdx);
        if (ekbMac[0] == cached[0] && ekbMac[1] == cached[1])
            return 0;
    }
    int ret = sceMgr_driver_85F518AB(ctx->rootKey, ekbDesc->ptr, ekbDesc->size, ekbSlot);
    if (ret < 0) {
        if (ret == (int)0x804FE004)
            return MGV_ERR_INVALID_ARG;
        return mapMgrError(ret);
    }
    return 0;
}

static int deriveIkvMacs(MgvMediaCtx *ctx, void *icvOut, void *hashOut, void *ekbMacOut,
                         const u8 *hdr48, int ekbSlot, int macSlot) {
    int slot = sceMgr_driver_C0B28CED(0);
    if (slot < 0) return mapMgrError(slot);

    int ret = ctx->deriveIkvKey(slot, hdr48, ekbSlot);
    if (ret < 0 ||
        (ret = sceMgr_driver_A329A73B(ekbMacOut, hdr48 + 16, NULL, 32, slot)) < 0 ||
        (ret = sceMgr_driver_1B4DB19C(icvOut, hdr48 + 16, NULL, 32, slot)) < 0 ||
        (ret = calcMac(hashOut, hdr48 + 16, NULL, 16, macSlot)) < 0) {
        sceMgr_driver_E6A203F8(slot);
        return mapMgrError(ret);
    }
    return slot;
}

typedef struct IkvVerifyArgs {
    MgvMediaCtx *ctx;
    void *icvOut;
    void *hashOut;
    void *ekbMacOut;
    const u8 *ikvBody;
    int ekbSlot;
} IkvVerifyArgs;

static int verifyIkvStackCb(void *arg) {
    IkvVerifyArgs *a = (IkvVerifyArgs *)arg;
    u8 chunk[4096];
    u8 hdr48[48];
    memcpy(hdr48, a->ikvBody, 48);

    int slot = deriveIkvMacs(a->ctx, a->icvOut, a->hashOut, a->ekbMacOut, hdr48, a->ekbSlot, a->ctx->macKeySlot);
    if (slot < 0)
        return slot;

    int ret = verifyMediaId(a->ctx, (const u32 *)(hdr48 + 16));
    if (ret >= 0) {
        const u8 *src = a->ikvBody + 48;
        int rem = 15984;
        while (rem > 0) {
            int n = (rem > 4096) ? 4096 : rem;
            memcpy(chunk, src, (size_t)n);
            int mret = sceMgr_driver_1B4DB19C(a->icvOut, chunk, a->icvOut, (u32)n, slot);
            if (mret < 0) {
                ret = mapMgrError(mret);
                break;
            }
            ret = calcMac(a->hashOut, chunk, a->hashOut, (u32)n, a->ctx->macKeySlot);
            if (ret < 0)
                break;
            src += (n & ~15);
            rem -= n;
        }
    }
    sceMgr_driver_E6A203F8(slot);
    return ret;
}

static int verifyIkv(MgvMediaCtx *ctx, void *icvOut, void *hashOut, void *ekbMacOut,
                     const u8 *ikvBuf, int ekbSlot) {
    IkvVerifyArgs args = { ctx, icvOut, hashOut, ekbMacOut, ikvBuf + 16, ekbSlot };
    int ret = sceKernelExtendKernelStack(5120, verifyIkvStackCb, &args);
    return (ret < 0) ? ret : 0;
}

static int readIkvEntryVerified(MgvMediaCtx *ctx, u32 *entryOut, const u8 *ikvBuf, int keyIdx, int titleIdx) {
    if ((ctx->verifiedMask & (1u << keyIdx)) == 0)
        return MGV_ERR_NOT_VERIFIED;

    u32 off = (u32)(16 * (titleIdx - 1));
    memcpy(entryOut, ikvBuf + 64 + off, 16);

    u32 hash[2] = { 0, 0 };
    int ret = calcMac(hash, ikvBuf + 32, hash, 16, ctx->macKeySlot);
    if (ret < 0) return ret;
    ret = calcMac(hash, ikvBuf + 64, hash, off, ctx->macKeySlot);
    if (ret < 0) return ret;
    ret = calcMac(hash, entryOut, hash, 16, ctx->macKeySlot);
    if (ret < 0) return ret;
    u32 tailLen = 15968u - off;
    if (tailLen != 0) {
        ret = calcMac(hash, ikvBuf + 80 + off, hash, tailLen, ctx->macKeySlot);
        if (ret < 0) return ret;
    }

    const u32 *expected = (const u32 *)(ctx->hashTable + 8 * keyIdx);
    if (hash[0] != expected[0] || hash[1] != expected[1])
        return MGV_ERR_HASH_MISMATCH;
    return 0;
}

typedef struct OpenLckArgs {
    MgvMediaCtx *ctx;
    u8 *keyringHdrOut;
    u8 *cidOut;
    int keyIdx;
    int titleIdx;
    const MgvBufDesc *lckDesc;
    const u8 *ikvBuf;
    const MgvBufDesc *ekbDesc;
} OpenLckArgs;

static int openLckStackCb(void *arg) {
    OpenLckArgs *a = (OpenLckArgs *)arg;
    u8 lckCopy[4656];
    u32 ikvEntry[4];
    u32 mac[4];
    int keySlot = -1;

    int ret = readIkvEntryVerified(a->ctx, ikvEntry, a->ikvBuf, a->keyIdx, a->titleIdx);
    if (ret < 0) goto done;
    if ((ikvEntry[0] | ikvEntry[1] | ikvEntry[2] | ikvEntry[3]) == 0) {
        ret = MGV_ERR_EMPTY_ENTRY;
        goto done;
    }

    const u8 *lckBody = (const u8 *)a->lckDesc->ptr + 16;
    int krLen = parseKeyringHeader(lckBody);
    if (krLen < 0) {
        ret = krLen;
        goto done;
    }
    memcpy(a->keyringHdrOut, lckBody, 96);

    keySlot = deriveContentKeySlot(a->ctx, a->keyringHdrOut + 32, a->ekbDesc);
    if (keySlot < 0) {
        ret = keySlot;
        goto done;
    }

    u32 bodyLen = a->lckDesc->size - 16u;
    if (bodyLen >= 0x1231u) {
        ret = MGV_ERR_INVALID_DATA;
        goto done;
    }
    memcpy(lckCopy, lckBody, bodyLen);

    ret = sceMgr_driver_DA7D8C82(mac, lckCopy, NULL, bodyLen, keySlot);
    if (ret < 0) {
        ret = mapMgrError(ret);
        goto done;
    }
    if (mac[0] != ikvEntry[0] || mac[1] != ikvEntry[1] ||
        mac[2] != ikvEntry[2] || mac[3] != ikvEntry[3]) {
        ret = MGV_ERR_HASH_MISMATCH;
        goto done;
    }

    int ruleRet = checkPlayLicense(a->ctx->mediaType, a->cidOut, lckCopy + (krLen & ~15), (int)(bodyLen - (u32)krLen));
    ret = (ruleRet < 0) ? mapRuleError(ruleRet) : keySlot;

done:
    if (ret < 0 && keySlot >= 0)
        sceMgr_driver_E6A203F8(keySlot);
    return ret;
}

static int openLck(MgvMediaCtx *ctx, u8 *keyringHdrOut, u8 *cidOut, int keyIdx, int titleIdx,
                   const MgvBufDesc *lckDesc, const u8 *ikvBuf, const MgvBufDesc *ekbDesc) {
    OpenLckArgs args = { ctx, keyringHdrOut, cidOut, keyIdx, titleIdx, lckDesc, ikvBuf, ekbDesc };
    return sceKernelExtendKernelStack(5680, openLckStackCb, &args);
}

/* =========================================================================
 * EKB & Page Read/Write/Erase Backends (MS & HDD)
 * ========================================================================= */

static int getHddEkb(void *buf, u32 size) {
    u8 hdr[48];
    int ret = sceMgr_driver_09C11491(hdr, 48);
    if (ret < 0) return mapMgrError(ret);
    if (ret == 0) return 0;
    if (buf != NULL) {
        int r2 = sceMgr_driver_09C11491(buf, size);
        if (r2 < 0) return mapMgrError(r2);
    }
    return ret;
}

static int getMsEkb(void *buf, u32 size) {
    u32 hdr[12];
    int ret = sceMgr_driver_864EA078(hdr, 48);
    if (ret < 0) return mapMgrError(ret);
    u32 ekbVer = bswap32(hdr[8]);
    if (ekbVer >= 0x10000u && ekbVer <= 0x10023u) {
        if (size > sizeof(g_embeddedEkb2))
            size = sizeof(g_embeddedEkb2);
        if (buf != NULL)
            memcpy(buf, g_embeddedEkb2, size);
        return (int)sizeof(g_embeddedEkb2);
    }
    if (buf != NULL) {
        int r2 = sceMgr_driver_864EA078(buf, size);
        if (r2 < 0) return mapMgrError(r2);
    }
    return ret;
}

static int hddReadPage(MgvMediaCtx *ctx, int page, u8 *buf) {
    int ret = sceMgr_driver_39E49050(ctx->mgrHandle, page, buf, 1024);
    if (ret < 0) return mapMgrError(ret);
    if (buf[0] != (u8)page || readBe16(buf + 2) != 0x200u || (buf[1] & 0xE0u) != 0xA0u)
        return MGV_ERR_MGR_ERROR_23;
    return 0;
}

static int hddWritePage(MgvMediaCtx *ctx, int page, u8 *buf) {
    u32 seq;
    memcpy(&seq, buf + 4, 4);
    seq = bswap32(bswap32(seq) + 1);
    buf[0] = (u8)page;
    buf[1] = 0xA0;
    buf[2] = 0x02;
    buf[3] = 0x00;
    memcpy(buf + 4, &seq, 4);

    int ret = sceMgr_driver_BBF6C182(ctx->mgrHandle, page, buf, 1024);
    if (ret < 0) {
        int err = mapMgrError(ret);
        if (err != MGV_ERR_IO_ERROR)
            return err;
    }
    ret = hddReadPage(ctx, page, buf);
    if (ret < 0 && ret != MGV_ERR_MGR_ERROR_23)
        return ret;
    u32 readSeq;
    memcpy(&readSeq, buf + 4, 4);
    return (readSeq == seq) ? 0 : MGV_ERR_IO_ERROR;
}

static int hddErasePage(MgvMediaCtx *ctx, int page, u8 *buf) {
    (void)page;
    (void)buf;
    return sceMgr_driver_B40FEAD1(ctx->mgrHandle);
}

static int checkMediaWritable(MgvMediaCtx *ctx) {
    if (ctx->mediaFlags & 1u) return MGV_ERR_MGR_ERROR_26;
    if (ctx->mediaFlags & 2u) return MGV_ERR_MGR_ERROR_25;
    return 0;
}

static int isBlankPage(const u8 *buf) {
    if (readBe16(buf + 2) != 0 || (buf[1] & 0xE0u) != 0)
        return 0;
    const u32 *p = (const u32 *)(buf + 8);
    const u32 *end = (const u32 *)(buf + 1024);
    while (p < end) {
        if (p[0] != 0xFFFFFFFFu || p[1] != 0xFFFFFFFFu)
            return 0;
        p += 2;
    }
    return 1;
}

static int msReadPage(MgvMediaCtx *ctx, int page, u8 *buf) {
    u32 mac[2];
    int ret = sceMgr_driver_7EC9DF8B(ctx->mgrHandle, page, buf, 1024);
    if (ret < 0) return mapMgrError(ret);
    if (readBe16(buf + 2) != 0x200u || (buf[1] & 0xE0u) != 0xA0u)
        return MGV_ERR_MGR_ERROR_23;
    if (*(const u32 *)(buf + 520) == 0 && *(const u32 *)(buf + 524) == 0)
        return MGV_ERR_NOT_OPENED;
    if ((s8)buf[1] < 0) {
        const void *iv = (buf[1] & 0x40u) ? g_zero16 : NULL;
        ret = sceMgr_driver_CF1EC2ED(buf + 8, buf + 8, iv);
        if (ret < 0) return mapMgrError(ret);
    }
    ret = sceMgr_driver_B53B2B5B(mac, buf, NULL, 520);
    if (ret < 0) return mapMgrError(ret);
    if (mac[0] != *(const u32 *)(buf + 520) || mac[1] != *(const u32 *)(buf + 524))
        return MGV_ERR_NOT_OPENED;
    return 0;
}

static int msWritePage(MgvMediaCtx *ctx, int page, u8 *buf) {
    u32 seq;
    memcpy(&seq, buf + 4, 4);
    seq = bswap32(bswap32(seq) + 1);
    buf[0] = (u8)page;
    buf[1] = 0xA0;
    buf[2] = 0x02;
    buf[3] = 0x00;
    memcpy(buf + 4, &seq, 4);

    int ret = sceMgr_driver_B53B2B5B(buf + 520, buf, NULL, 520);
    if (ret < 0) return mapMgrError(ret);
    if ((s8)buf[1] < 0) {
        ret = sceMgr_driver_FDF97B95(buf + 8, buf + 8, NULL, 512);
        if (ret < 0) return mapMgrError(ret);
    }
    ret = sceMgr_driver_275C6686(ctx->mgrHandle, page, buf, 1024);
    if (ret < 0) {
        int err = mapMgrError(ret);
        if (err != MGV_ERR_IO_ERROR)
            return err;
        sceKernelDelayThread(500000);
    }
    ret = msReadPage(ctx, page, buf);
    if (ret < 0) return ret;
    u32 readSeq;
    memcpy(&readSeq, buf + 4, 4);
    return (readSeq == seq) ? 0 : MGV_ERR_IO_ERROR;
}

static int msErasePage(MgvMediaCtx *ctx, int page, u8 *buf) {
    int ret = msReadPage(ctx, page, buf);
    if (ret < 0 && ret != MGV_ERR_MGR_ERROR_23 && ret != MGV_ERR_NOT_OPENED)
        return ret;
    u32 seq;
    memcpy(&seq, buf + 4, 4);
    seq = bswap32(bswap32(seq) + 1);
    memset(buf, 0xFF, 1024);
    buf[0] = (u8)page;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    memcpy(buf + 4, &seq, 4);

    ret = sceMgr_driver_275C6686(ctx->mgrHandle, page, buf, 1024);
    if (ret < 0) {
        int err = mapMgrError(ret);
        if (err != MGV_ERR_IO_ERROR)
            return err;
        sceKernelDelayThread(500000);
    }
    ret = msReadPage(ctx, page, buf);
    if (ret < 0 && ret != MGV_ERR_MGR_ERROR_23)
        return ret;
    u32 readSeq;
    memcpy(&readSeq, buf + 4, 4);
    return (readSeq == seq) ? 0 : MGV_ERR_IO_ERROR;
}

static int loadIcvTable(MgvMediaCtx *ctx, u8 *buf) {
    ctx->verifiedMask = 0;
    memset(ctx->hashTable, 0, sizeof(ctx->hashTable));
    memset(ctx->ekbMacTable, 0, sizeof(ctx->ekbMacTable));
    ctx->lastKeyIndex = -1;
    ctx->lastTitleIndex = -1;

    int ret = ctx->readPage(ctx, 1, buf);
    if (ret < 0) {
        if (ret != MGV_ERR_MGR_ERROR_23)
            return ret;
        if ((ctx->mediaFlags & 0x20u) != 0 || isBlankPage(buf))
            ctx->mediaFlags |= 8u;
        memset(buf, 0, 1024);
    }
    memcpy(ctx->icvTable, buf + 8, 512);
    return 0;
}

/* =========================================================================
 * Temporary FPL Allocator & Content Slot Management
 * ========================================================================= */

static int allocTempBuf(void **outPtr, u32 size) {
    if (size >= 0x52B1u)
        return MGV_ERR_MSG_FORMAT;
    int fpl = sceKernelCreateFpl("mgvideo", 1, 0x4000, size + 16, 1, NULL);
    if (fpl < 0)
        return (fpl == SCE_KERNEL_ERROR_NO_MEMORY) ? MGV_ERR_NO_MEMORY : fpl;
    void *blk = NULL;
    int ret = sceKernelTryAllocateFpl(fpl, &blk);
    if (ret < 0) {
        sceKernelDeleteFpl(fpl);
        return (ret == SCE_KERNEL_ERROR_NO_MEMORY) ? MGV_ERR_NO_MEMORY : ret;
    }
    *(int *)blk = fpl;
    *outPtr = (u8 *)blk + 16;
    return 0;
}

static int freeTempBuf(void *ptr) {
    int fpl = *(int *)((u8 *)ptr - 16);
    return sceKernelDeleteFpl(fpl);
}

static int allocContentSlot(MgvMediaCtx *media) {
    int i;
    int intr = sceKernelCpuSuspendIntr();
    for (i = 0; i < 2; i++) {
        if (g_contentCtxs[i].inUse == 0) {
            g_contentCtxs[i].inUse = 1;
            g_contentCtxs[i].media = media;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return (i < 2) ? i : MGV_ERR_NO_SLOT;
}

static void freeContentSlot(u32 idx) {
    if (idx >= 2u) return;
    if (g_contentCtxs[idx].mgrKeySlot >= 0) {
        sceMgr_driver_E6A203F8(g_contentCtxs[idx].mgrKeySlot);
        g_contentCtxs[idx].mgrKeySlot = -1;
    }
    g_contentCtxs[idx].inUse = 0;
}

static MgvContentCtx *getContentSlot(u32 idx) {
    if (idx < 2u && g_contentCtxs[idx].inUse != 0 && g_contentCtxs[idx].media != NULL)
        return &g_contentCtxs[idx];
    return NULL;
}

static void freeAllContentSlots(void) {
    int intr = sceKernelCpuSuspendIntr();
    u32 i;
    for (i = 0; i < 2u; i++) {
        if (g_contentCtxs[i].inUse != 0)
            freeContentSlot(i);
    }
    sceKernelCpuResumeIntr(intr);
}

static void initContentSlots(void) {
    memset(g_contentCtxs, 0, sizeof(g_contentCtxs));
    g_contentCtxs[0].mgrKeySlot = -1;
    g_contentCtxs[1].mgrKeySlot = -1;
}

static void markAllSuspended(void) {
    int i;
    for (i = 0; i < 2; i++) {
        if (g_contentCtxs[i].inUse != 0)
            g_contentCtxs[i].suspended = 1;
    }
}

static int mgvSysEventHandler(int evId, const char *evName, void *param, int *result) {
    (void)evName; (void)param; (void)result;
    if (evId == 0x100000)
        markAllSuspended();
    return 0;
}

static void clearChallenge(MgvMediaCtx *ctx) {
    ctx->challengeTick = 0;
    ctx->lastKeyIndex = -1;
    ctx->lastTitleIndex = -1;
    memset(ctx->challengeNonce, 0, sizeof(ctx->challengeNonce));
}

/* =========================================================================
 * Module Lifecycle & Media Context Management
 * ========================================================================= */

static int mgvCoreInit(void) {
    if (g_initRefCount > 0) {
        int prev = g_initRefCount;
        g_initRefCount++;
        return prev;
    }
    initContentSlots();
    int ret = sceMgr_driver_7B11D7AD();
    if (ret < 0) return mapMgrError(ret);
    ret = sceMgr_driver_5BABFFAE(g_rngCtx);
    if (ret < 0) {
        int err = mapMgrError(ret);
        sceMgr_driver_84B044C7();
        return err;
    }
    g_initRefCount = 1;
    sceKernelRegisterSysEventHandler(&g_sysEventHandler);
    return 0;
}

static int mgvCoreTerm(void) {
    if (g_initRefCount <= 0)
        return MGV_ERR_NOT_INITIALIZED;
    g_initRefCount--;
    if (g_initRefCount > 0)
        return g_initRefCount;
    sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
    freeAllContentSlots();
    sceMgr_driver_84B044C7();
    return 0;
}

static int findMediaCtx(int devType, MgvMediaCtx **out, u32 minState) {
    if (g_initRefCount <= 0) {
        *out = NULL;
        return MGV_ERR_NOT_INITIALIZED;
    }
    int i;
    MgvMediaCtx *found = NULL;
    for (i = 0; i < 2; i++) {
        if (g_mediaCtxs[i] != NULL && g_mediaCtxs[i]->devType == devType) {
            found = g_mediaCtxs[i];
            break;
        }
    }
    if (found == NULL || found->refCount <= 0) {
        *out = NULL;
        return MGV_ERR_INVALID_ARG;
    }
    if (found->state < minState) {
        if (minState == 2) return MGV_ERR_NOT_OPENED;
        if (minState == 3) return MGV_ERR_NOT_VERIFIED;
        return MGV_ERR_NOT_INITIALIZED;
    }
    *out = found;
    return 0;
}

static int allocMediaCtx(MgvMediaCtx **out, int devType) {
    if (devType != 16 && devType != 32)
        return MGV_ERR_INVALID_ARG;
    int freeIdx = -1;
    int i;
    for (i = 1; i >= 0; i--) {
        if (g_mediaCtxs[i] != NULL) {
            if (g_mediaCtxs[i]->devType == devType) {
                *out = g_mediaCtxs[i];
                return i;
            }
        } else {
            freeIdx = i;
        }
    }
    if (freeIdx < 0)
        return MGV_ERR_NO_SLOT;

    if (g_mediaFplIds[freeIdx] <= 0) {
        int fpl = sceKernelCreateFpl("mgvideo", 1, 0, sizeof(MgvMediaCtx), 1, NULL);
        if (fpl == SCE_KERNEL_ERROR_NO_MEMORY)
            fpl = MGV_ERR_NO_MEMORY;
        g_mediaFplIds[freeIdx] = fpl;
    }
    void *mem = NULL;
    int ret = sceKernelTryAllocateFpl(g_mediaFplIds[freeIdx], &mem);
    if (ret < 0)
        return (ret == SCE_KERNEL_ERROR_NO_MEMORY) ? MGV_ERR_NO_MEMORY : ret;

    MgvMediaCtx *ctx = (MgvMediaCtx *)mem;
    g_mediaCtxs[freeIdx] = ctx;
    ctx->devType = devType;
    ctx->refCount = 0;
    *out = ctx;
    return freeIdx;
}

static void freeMediaCtx(MgvMediaCtx *ctx) {
    int i;
    for (i = 0; i < 2; i++) {
        if (g_mediaCtxs[i] == ctx) {
            g_mediaCtxs[i] = NULL;
            sceKernelDeleteFpl(g_mediaFplIds[i]);
            g_mediaFplIds[i] = -1;
            break;
        }
    }
}

static int hddOpenMedia(MgvMediaCtx *ctx, void *rngCtx) {
    (void)rngCtx;
    ctx->mediaFlags = 32;
    ctx->macKeySlot = -2;
    return 0;
}

static void hddCloseMedia(MgvMediaCtx *ctx) {
    (void)ctx;
}

static int msOpenMedia(MgvMediaCtx *ctx, void *rngCtx) {
    int h = sceMgr_driver_6B4C5BC5(rngCtx);
    int ret = h;
    if (h >= 0) {
        ctx->mgrHandle = h;
        ret = sceMgr_driver_20610CDF(h);
        if (ret >= 0) {
            ctx->mediaFlags = (u32)ret;
            if ((ret & 4) == 0) {
                ret = MGV_ERR_MGR_ERROR_27;
                goto fail;
            }
            ctx->macKeySlot = -1;
            ret = sceMgr_driver_156A83C3(ctx->mgrHandle, rngCtx);
            if (ret >= 0)
                return ret;
        }
    }
    ret = mapMgrError(ret);
fail:
    if (ret < 0 && ctx->macKeySlot >= 0) {
        sceMgr_driver_E6A203F8(ctx->macKeySlot);
        ctx->macKeySlot = -1;
    }
    return ret;
}

static void msCloseMedia(MgvMediaCtx *ctx) {
    sceMgr_driver_B8626BA2(ctx->mgrHandle);
    if (ctx->macKeySlot >= 0) {
        sceMgr_driver_E6A203F8(ctx->macKeySlot);
        ctx->macKeySlot = -1;
    }
}

static int msTermMedia(MgvMediaCtx *ctx) {
    if (ctx->mgrHandle >= 0)
        ctx->mgrHandle = -1;
    return sceMgr_driver_5A63B6A4();
}

static int hddTermMedia(MgvMediaCtx *ctx) {
    if (ctx->mgrHandle >= 0)
        ctx->mgrHandle = -1;
    return sceMgr_driver_401F71DC();
}

static int initMsMedia(MgvMediaCtx *ctx) {
    u32 ekbHdr[16];
    ctx->deriveIkvKey     = sceMgr_driver_C3C8C1FC;
    ctx->deriveContentKey = sceMgr_driver_0C813CF1;
    ctx->exportKey        = sceMgr_driver_1B63AFF9;
    ctx->getMediaId       = sceMgr_driver_97B245D5;
    ctx->getEkb           = getMsEkb;
    ctx->openMedia        = msOpenMedia;
    ctx->closeMedia       = msCloseMedia;
    ctx->termMedia        = msTermMedia;
    ctx->readPage         = msReadPage;
    ctx->writePage        = msWritePage;
    ctx->erasePage        = msErasePage;
    ctx->mediaFlags       = 0;
    ctx->mediaType        = -1;

    int ret = sceMgr_driver_45EA1DB5();
    if (ret >= 0) {
        ctx->mediaType = 0;
        ret = ctx->getEkb(ekbHdr, 64);
        if (ret >= 0) {
            ctx->mgrHandle = -1;
            memcpy(ctx->rootKey, ekbHdr + 12, 16);
            return 0;
        }
    }
    ret = mapMgrError(ret);
    if (ctx->mediaType >= 0)
        sceMgr_driver_5A63B6A4();
    return ret;
}

static int initHddMedia(MgvMediaCtx *ctx) {
    u32 ekbHdr[16];
    ctx->deriveIkvKey     = sceMgr_driver_9315D849;
    ctx->deriveContentKey = sceMgr_driver_EA241D32;
    ctx->exportKey        = sceMgr_driver_EBED61A7;
    ctx->getMediaId       = sceMgr_driver_735972DB;
    ctx->getEkb           = getHddEkb;
    ctx->openMedia        = hddOpenMedia;
    ctx->closeMedia       = hddCloseMedia;
    ctx->termMedia        = hddTermMedia;
    ctx->readPage         = hddReadPage;
    ctx->writePage        = hddWritePage;
    ctx->erasePage        = hddErasePage;
    ctx->mediaFlags       = 32;
    ctx->mediaType        = -1;

    int ret = sceMgr_driver_7F37BECF();
    if (ret >= 0) {
        ctx->mediaType = 1;
        ret = ctx->getEkb(ekbHdr, 64);
        if (ret >= 0) {
            memcpy(ctx->rootKey, ekbHdr + 12, 16);
            ret = sceMgr_driver_CEE87932();
            if (ret >= 0) {
                ctx->mgrHandle = ret;
                return ret;
            }
        }
    }
    ret = mapMgrError(ret);
    if (ctx->mediaType >= 0)
        sceMgr_driver_401F71DC();
    return ret;
}

static int openMediaCtx(MgvMediaCtx *ctx) {
    if (ctx->refCount > 0) {
        ctx->refCount++;
        return 0;
    }
    ctx->macKeySlot = -1;
    ctx->state = 0;
    clearChallenge(ctx);
    int ret = (ctx->devType == 16) ? initHddMedia(ctx) : initMsMedia(ctx);
    if (ret >= 0) {
        ctx->state = 1;
        ctx->refCount = 1;
        return 0;
    }
    return ret;
}

static int closeMediaCtx(MgvMediaCtx *ctx) {
    ctx->refCount--;
    if (ctx->refCount <= 0) {
        ctx->termMedia(ctx);
        if (ctx->macKeySlot >= 0)
            sceMgr_driver_E6A203F8(ctx->macKeySlot);
        freeMediaCtx(ctx);
        return -1;
    }
    return ctx->refCount;
}

/* =========================================================================
 * MP4 sinf/schm/schi/ksid/kyid/ealg Parsing & Stream Slots
 * ========================================================================= */

static int matchBoxHeader(const u32 *box, const char *fourcc) {
    if (bswap32(*box) < 8u)
        return -1;
    return (memcmp(box + 1, fourcc, 4) == 0) ? 0 : -1;
}

static int matchFullBoxHeader(const u8 *box, const char *fourcc, u8 ver) {
    if (bswap32(*(const u32 *)box) < 12u)
        return -1;
    if (memcmp(box + 4, fourcc, 4) == 0 && box[8] == ver)
        return 0;
    return -1;
}

static int parseSchiBox(u32 *algOut, u8 *cidOut, u32 *ivOut, const u32 *schi) {
    const u8 *ksid = NULL, *kyid = NULL, *ealg = NULL;
    const u8 *cur = (const u8 *)(schi + 2);
    const u8 *end = (const u8 *)schi + bswap32(*schi);

    while (cur < end) {
        u32 sz = bswap32(*(const u32 *)cur);
        if (cur + sz > end || sz == 0)
            break;
        if (ksid == NULL && matchFullBoxHeader(cur, "ksid", 0) == 0) {
            if (sz < 0x24u) return MGV_ERR_INVALID_DATA;
            ksid = cur;
        } else if (kyid == NULL && matchFullBoxHeader(cur, "kyid", 0) == 0) {
            if (sz < 0x0Eu) return MGV_ERR_INVALID_DATA;
            kyid = cur;
        } else if (ealg == NULL && matchFullBoxHeader(cur, "ealg", 0) == 0) {
            if (sz < 0x10u) return MGV_ERR_INVALID_DATA;
            ealg = cur;
            u32 alg = bswap32(*(const u32 *)(cur + 12));
            *algOut = alg;
            if (alg == 8 && sz - 16u < 16u)
                return MGV_ERR_INVALID_DATA;
        }
        cur += sz;
    }

    if (ksid == NULL || kyid == NULL || ealg == NULL)
        return MGV_ERR_INVALID_DATA;

    memcpy(cidOut, ksid + 12, 24);
    memcpy(ivOut, ealg + 16, 16);
    return (int)readBe16(kyid + 12);
}

static int parseSinfBox(const u32 **schiOut, const u8 *sinf, u32 sinfSize) {
    const u8 *schm = NULL, *schi = NULL;
    const u8 *cur = sinf + 8;
    const u8 *end = sinf + sinfSize;

    while (cur < end) {
        u32 sz = bswap32(*(const u32 *)cur);
        if (cur + sz > end || sz == 0)
            break;
        if (matchFullBoxHeader(cur, "schm", 0) == 0) {
            if (schm != NULL || sz < 0x12u)
                return MGV_ERR_INVALID_DATA;
            schm = cur;
        } else if (matchBoxHeader((const u32 *)cur, "schi") == 0) {
            if (sz < 8u)
                return MGV_ERR_INVALID_DATA;
            schi = cur;
            break;
        }
        cur += sz;
    }
    if (schm == NULL || schi == NULL)
        return MGV_ERR_INVALID_DATA;
    if (memcmp(schm + 12, "prot", 4) != 0)
        return MGV_ERR_UNSUPPORTED_VER;
    if (readBe16(schm + 16) > 0x100u)
        return MGV_ERR_UNSUPPORTED_VER;
    *schiOut = (const u32 *)schi;
    return 0;
}

static int allocStreamSlot(MgvContentCtx *cctx) {
    int i;
    int intr = sceKernelCpuSuspendIntr();
    for (i = 0; i < 4; i++) {
        if (cctx->streams[i].keyMode == 0) {
            cctx->streams[i].keyMode = -1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return (i < 4) ? i : MGV_ERR_NO_SLOT;
}

static int freeStreamSlot(u32 contentIdx, int streamIdx) {
    MgvContentCtx *cctx = getContentSlot(contentIdx);
    if (cctx == NULL || streamIdx < 0 || streamIdx >= 4)
        return MGV_ERR_INVALID_ARG;
    if (cctx->streams[streamIdx].keyMode == 0)
        return MGV_ERR_INVALID_ARG;
    cctx->streams[streamIdx].keyMode = 0;
    return 0;
}

static int lookupStream(int handle, MgvStreamSlot **slotOut, MgvContentCtx **cctxOut) {
    if (handle < 0)
        return MGV_ERR_INVALID_ARG;
    MgvContentCtx *cctx = getContentSlot((u32)handle >> 8);
    if (cctx == NULL || cctx->media == NULL)
        return MGV_ERR_INVALID_ARG;
    if (cctx->media->state < 2u)
        return MGV_ERR_NOT_OPENED;
    u32 sidx = (u32)handle & 0xFFu;
    if (sidx >= 4u)
        return MGV_ERR_INVALID_ARG;
    *slotOut = &cctx->streams[sidx];
    if (cctxOut != NULL)
        *cctxOut = cctx;
    return cctx->mgrKeySlot;
}

static int ensureKeyLoaded(MgvContentCtx *cctx) {
    if ((cctx->media->mediaFlags & 0x20u) == 0) {
        u8 tmp[16];
        int ret = getMediaId(cctx->media, tmp);
        return (ret < 0) ? ret : 0;
    }
    if (cctx->suspended != 0) {
        int ret = restoreContentKey(cctx);
        if (ret < 0) return ret;
        cctx->suspended = 0;
    }
    return 0;
}

/* =========================================================================
 * IKV Update Helpers
 * ========================================================================= */

typedef struct IkvUpdateArgs {
    MgvMediaCtx *ctx;
    int keyIdx;
    u8 *entries;
    const s16 *indices;
    int count;
    const u32 *newVal;
    void *icvOut;
    u32 *hashInOut;
    int keySlot;
} IkvUpdateArgs;

static int updateIkvChunk(int baseIdx, const s16 *indices, int count, const u32 *newVal,
                          u8 *chunk, u32 chunkSize, void *icvInOut, u32 *newHash, u32 *oldHash, int keySlot) {
    int alreadyDiff = (oldHash[0] != newHash[0] || oldHash[1] != newHash[1]);
    int ret = calcMac(oldHash, chunk, oldHash, chunkSize, -1);
    if (ret < 0) return ret;

    int modified = 0;
    int i;
    for (i = 0; i < count; i++) {
        int t = indices[i];
        if ((u32)(t - 1) >= 999u)
            return MGV_ERR_INVALID_ARG;
        int rel = (t - 1) - baseIdx;
        if (rel >= 0 && rel < (int)(chunkSize >> 4)) {
            memcpy(chunk + 16 * rel, newVal, 16);
            modified++;
        }
    }

    if (alreadyDiff || modified > 0) {
        ret = calcMac(newHash, chunk, newHash, chunkSize, -1);
        if (ret < 0) return ret;
    } else {
        newHash[0] = oldHash[0];
        newHash[1] = oldHash[1];
    }

    if (newHash[0] == oldHash[0] && newHash[1] == oldHash[1])
        modified = 0;

    ret = sceMgr_driver_1B4DB19C(icvInOut, chunk, icvInOut, chunkSize, keySlot);
    return (ret < 0) ? mapMgrError(ret) : modified;
}

static int updateIkvStackCb(void *arg) {
    IkvUpdateArgs *a = (IkvUpdateArgs *)arg;
    u8 chunk[4096];
    u32 oldHash[2] = { a->hashInOut[0], a->hashInOut[1] };
    int rem = 15984;
    int baseIdx = 0;
    int totalMod = 0;

    while (rem > 0) {
        u32 n = (rem > 4096) ? 4096u : (u32)rem;
        memcpy(chunk, a->entries + 16 * baseIdx, n);
        int r = updateIkvChunk(baseIdx, a->indices, a->count, a->newVal,
                               chunk, n, a->icvOut, a->hashInOut, oldHash, a->keySlot);
        if (r < 0) return r;
        totalMod += r;
        baseIdx += (int)(n >> 4);
        rem -= (int)n;
    }

    const u32 *expected = (const u32 *)(a->ctx->hashTable + 8 * a->keyIdx);
    if ((oldHash[0] != expected[0] || oldHash[1] != expected[1]) &&
        (expected[0] | expected[1]) != 0)
        return MGV_ERR_HASH_MISMATCH;

    if (totalMod > 0) {
        int i;
        for (i = a->count - 1; i >= 0; i--) {
            memcpy(a->entries + 16 * (a->indices[i] - 1), a->newVal, 16);
        }
    }
    return totalMod;
}

static int updateIkvEntries(IkvUpdateArgs *args) {
    return sceKernelExtendKernelStack(5120, updateIkvStackCb, args);
}

static int writeSingleIkvEntry(MgvMediaCtx *ctx, u32 keyIdx, int titleIdx, const u8 *entryMac,
                               u8 *ikvBuf, const MgvBufDesc *ekbDesc) {
    const u32 *curIcv = (const u32 *)(ctx->icvTable + 16 * keyIdx);
    if ((curIcv[0] | curIcv[1] | curIcv[2] | curIcv[3]) == 0) {
        memset(ikvBuf, 0, 16048);
        *(u32 *)ikvBuf = 0x54534B4Du; /* "MKST" */
        int ret = sceMgr_driver_02117F1E(ikvBuf + 16, g_rngCtx);
        if (ret < 0) return mapMgrError(ret);
        ret = sceMgr_driver_02117F1E(ikvBuf + 24, g_rngCtx);
        if (ret < 0) return mapMgrError(ret);
        if ((*(u32 *)(ikvBuf + 16) | *(u32 *)(ikvBuf + 20) |
             *(u32 *)(ikvBuf + 24) | *(u32 *)(ikvBuf + 28)) == 0) {
            ret = sceMgr_driver_02117F1E(ikvBuf + 16, g_rngCtx);
            if (ret < 0) return mapMgrError(ret);
        }
        ret = getMediaId(ctx, ikvBuf + 32);
        if (ret < 0) return ret;
    }

    int ekbSlot = loadEkbKeySlot(ekbDesc);
    if (ekbSlot < 0) return ekbSlot;

    u8 hdr48[48];
    memcpy(hdr48, ikvBuf + 16, 48);
    u32 ekbId = *(const u32 *)ekbDesc->ptr;
    memcpy(hdr48 + 32, &ekbId, 4);

    u8 newIcv[16];
    u32 newHash[2], newEkbMac[2];
    int slot = deriveIkvMacs(ctx, newIcv, newHash, newEkbMac, hdr48, ekbSlot, ctx->macKeySlot);
    int ret = slot;
    if (slot >= 0) {
        ret = verifyMediaId(ctx, (const u32 *)(hdr48 + 16));
        if (ret >= 0)
            ret = verifyEkbMac(ctx, (int)keyIdx, newEkbMac, ekbDesc, ekbSlot);
        if (ret >= 0) {
            s16 idx = (s16)titleIdx;
            IkvUpdateArgs uargs = {
                ctx, (int)keyIdx, ikvBuf + 64, &idx, 1,
                (const u32 *)entryMac, newIcv, newHash, slot
            };
            ret = updateIkvEntries(&uargs);
            if (ret >= 0) {
                memcpy(ikvBuf + 48, &ekbId, 4);
                memcpy(ctx->ekbMacTable + 8 * keyIdx, newEkbMac, 8);
                memcpy(ctx->hashTable + 8 * keyIdx, newHash, 8);
                memcpy(ctx->icvTable + 16 * keyIdx, newIcv, 16);
                *(u16 *)(ikvBuf + 6) = 35;
                ret = 0;
            }
        }
        sceMgr_driver_E6A203F8(slot);
    }
    sceMgr_driver_E6A203F8(ekbSlot);
    return ret;
}

/* =========================================================================
 * MGL3 / MGV1 Message Parsing & Verification
 * ========================================================================= */

static int parseMgl3Header(const u8 *buf, int bufSize) {
    if (*(const u32 *)buf != 0x33504B4Du) /* "MKP3" */
        return MGV_ERR_INVALID_DATA;
    u16 len = *(const u16 *)(buf + 6);
    if (bufSize < (int)len || len < 0x70u)
        return MGV_ERR_INVALID_DATA;
    u16 type = *(const u16 *)(buf + 4);
    if (type != 508 && type != 509 && type != 510)
        return MGV_ERR_INVALID_DATA;
    return (int)len;
}

static int buildMgl3Packet(u8 *macOut, u8 *buf, int bodyLen, int keySlot, int pktType) {
    *(u32 *)buf = 0x33504B4Du; /* "MKP3" */
    *(u16 *)(buf + 4) = (u16)pktType;
    *(u16 *)(buf + 6) = (u16)(bodyLen + 16);
    *(u32 *)(buf + 8) = 0;
    *(u32 *)(buf + 12) = 0;

    if (pktType == 508) {
        memcpy(macOut, buf + 48, 16);
    } else if (pktType != 509 && pktType != 510) {
        return MGV_ERR_INVALID_ARG;
    }

    if (keySlot >= 0) {
        int ret = sceMgr_driver_3B548024(buf + 48, keySlot);
        if (ret < 0) return mapMgrError(ret);
        ret = sceMgr_driver_DA7D8C82(macOut, buf, NULL, (u32)bodyLen, keySlot);
        if (ret < 0) return mapMgrError(ret);
    }
    return 0;
}

static int verifyMgl3Packet(MgvMediaCtx *ctx, const u8 *pkt, u8 *payload, int ekbSlot) {
    int keySlot = sceMgr_driver_C0B28CED(4);
    if (keySlot < 0) return mapMgrError(keySlot);

    u32 bodyLen = (u32)(*(const u16 *)(pkt + 6) - 16);
    const u8 *tailMac = pkt + (bodyLen & ~15u);
    int ret;

    if (*(const u16 *)(pkt + 4) == 508) {
        ret = ctx->deriveContentKey(keySlot, tailMac, ekbSlot);
        if (ret < 0) { ret = mapMgrError(ret); goto fail; }
        ret = sceMgr_driver_DA7D8C82(payload + 32, tailMac, NULL, 16, keySlot);
        if (ret < 0) { ret = mapMgrError(ret); goto fail; }
        ret = ctx->deriveContentKey(keySlot, payload + 32, ekbSlot);
        if (ret < 0) { ret = mapMgrError(ret); goto fail; }
        return keySlot;
    } else {
        u32 calc[4];
        ret = sceMgr_driver_9973CD5B(keySlot, payload + 32);
        if (ret < 0) { ret = mapMgrError(ret); goto fail; }
        ret = sceMgr_driver_DA7D8C82(calc, pkt, NULL, bodyLen, keySlot);
        if (ret < 0) { ret = mapMgrError(ret); goto fail; }
        if (memcmp(calc, tailMac, 16) != 0) {
            ret = MGV_ERR_INVALID_DATA;
            goto fail;
        }
        return keySlot;
    }
fail:
    sceMgr_driver_E6A203F8(keySlot);
    return ret;
}

static int verifyLckForTransfer(MgvMediaCtx *ctx, int keyIdx, int titleIdx,
                                const u8 *lckBody, int bodyLen, const u8 *ikvBuf,
                                const MgvBufDesc *ekbDesc, int op) {
    u32 ikvEntry[4];
    u32 mac[4];
    int keySlot = -1;

    int ret = readIkvEntryVerified(ctx, ikvEntry, ikvBuf, keyIdx, titleIdx);
    if (ret < 0) goto done;
    if ((ikvEntry[0] | ikvEntry[1] | ikvEntry[2] | ikvEntry[3]) == 0) {
        ret = MGV_ERR_EMPTY_ENTRY;
        goto done;
    }

    int krLen = parseKeyringHeader(lckBody);
    if (krLen < 0) { ret = krLen; goto done; }

    keySlot = deriveContentKeySlot(ctx, lckBody + 32, ekbDesc);
    if (keySlot < 0) { ret = keySlot; goto done; }

    ret = sceMgr_driver_DA7D8C82(mac, lckBody, NULL, (u32)bodyLen, keySlot);
    if (ret < 0) { ret = mapMgrError(ret); goto done; }
    if (memcmp(mac, ikvEntry, 16) != 0) {
        ret = MGV_ERR_HASH_MISMATCH;
        goto done;
    }

    const u8 *rules = lckBody + (krLen & ~15);
    int rulesLen = bodyLen - krLen;
    int rret = (op == 3) ? checkCopyLicense(ctx->mediaType, rules, rulesLen)
             : (op == 4) ? checkMoveLicense(ctx->mediaType, rules, rulesLen)
             : -2;
    ret = (rret < 0) ? mapRuleError(rret) : keySlot;

done:
    if (ret < 0 && keySlot >= 0)
        sceMgr_driver_E6A203F8(keySlot);
    return ret;
}

static int generateCid(u8 *cidOut) {
    u64 id;
    u8 rnd[8];
    int ret = sceMgr_driver_2D1CB2B8(&id);
    if (ret < 0) return mapMgrError(ret);
    ret = sceMgr_driver_02117F1E(rnd, g_rngCtx);
    if (ret < 0) return mapMgrError(ret);
    memset(cidOut, 0, 8);
    cidOut[3] = 1;
    cidOut[4] = 2;
    memcpy(cidOut + 8, &id, 8);
    memcpy(cidOut + 16, rnd, 8);
    return 0;
}

static int createLicenseBody(const MgvBufDesc *outDesc, u8 *keyInfo, const u8 *avout8, int mode) {
    u8 flags = avout8[5];
    u32 sub = (flags >> 1) & 3u;
    int permMode = ((flags & 1u) == 0) ? 1 : 0;
    if (sub != 0) {
        if (sub != 2 || (flags & 1u) == 0)
            return MGV_ERR_INVALID_ARG;
        permMode = 2;
    }

    memset(keyInfo, 0, 144);
    int ret = generateCid(keyInfo + 34);
    if (ret < 0) return ret;

    u8 *dst = (u8 *)outDesc->ptr;
    int krLen = generateKeyring(dst + 16, keyInfo, mode);
    if (krLen < 0) return krLen;
    int off = 16 + krLen;

    int secLen = buildLicenseSections(dst + off, outDesc->size, keyInfo + 34, avout8, permMode, 0, NULL);
    if (secLen < 0)
        return MGV_ERR_INVALID_ARG;
    off += secLen;

    u8 mac[16];
    buildMgl3Packet(mac, dst, off, -1, 508);
    memcpy(dst + off, mac, 16);
    return off + 16;
}

static int generateChallengeNonce(MgvMediaCtx *ctx, u8 *nonceOut) {
    int ret = sceMgr_driver_02117F1E(nonceOut, g_rngCtx);
    if (ret < 0) return mapMgrError(ret);
    ret = sceMgr_driver_02117F1E(nonceOut + 8, g_rngCtx);
    if (ret < 0) return mapMgrError(ret);
    if ((*(u32 *)(nonceOut + 0) | *(u32 *)(nonceOut + 4) |
         *(u32 *)(nonceOut + 8) | *(u32 *)(nonceOut + 12)) == 0) {
        ret = sceMgr_driver_02117F1E(nonceOut, g_rngCtx);
        if (ret < 0) return mapMgrError(ret);
    }
    clearChallenge(ctx);
    memcpy(ctx->challengeNonce, nonceOut, 16);
    return 0;
}

static int parseMgv1Header(int expectedType, const u8 *buf, int bufSize) {
    if (*(const u32 *)buf != 0x314E4B4Du) /* "MKN1" */
        return MGV_ERR_MSG_FORMAT;
    u16 type = *(const u16 *)(buf + 4);
    if (type != (u16)expectedType)
        return MGV_ERR_MSG_FORMAT;
    int len = (int)readBe16(buf + 6);
    if (bufSize < len)
        return MGV_ERR_MSG_FORMAT;
    if (type == 259 && len >= 0x90) return len;
    if (type == 260 && len == 64)   return 64;
    return MGV_ERR_MSG_FORMAT;
}

static int verifyMgv1Mac(const u8 *msg, const u8 *sig, u32 sigLen) {
    if (sig[0] != 2)
        return MGV_ERR_MSG_VERSION;
    if (sigLen < 0x20u)
        return MGV_ERR_MSG_FORMAT;
    u32 ekbLen = readBe16(sig + 4);
    if (sigLen < ekbLen)
        return MGV_ERR_MSG_FORMAT;

    u32 msgLen = readBe16(msg + 6);
    int slot = sceMgr_driver_C0B28CED(0);
    if (slot < 0) return mapMgrError(slot);

    int ret = sceMgr_driver_EDEAAA00(slot, sig + 32, ekbLen);
    if (ret < 0) {
        sceMgr_driver_E6A203F8(slot);
        if (ret == (int)0x80501004 || ret == (int)0x80501002)
            return MGV_ERR_MSG_MAC;
        return mapMgrError(ret);
    }

    int res = MGV_ERR_MSG_FORMAT;
    if (*(const u16 *)(sig + 2) == 0x1000u) {
        u32 mac[4];
        ret = sceMgr_driver_4FD151B7(mac, msg, NULL, msgLen, slot);
        if (ret < 0) {
            res = mapMgrError(ret);
        } else if (memcmp(mac, sig + 16, 16) == 0) {
            res = 0;
        } else {
            res = MGV_ERR_MSG_MAC;
        }
    }
    sceMgr_driver_E6A203F8(slot);
    return res;
}

static int buildClockChallenge(MgvMediaCtx *ctx, const MgvBufDesc *outDesc) {
    if (outDesc->size < 0x30u)
        return MGV_ERR_INVALID_ARG;
    u8 *dst = (u8 *)outDesc->ptr;
    *(u32 *)dst = 0x314E4B4Du; /* "MKN1" */
    *(u16 *)(dst + 4) = 4;
    dst[6] = 0; dst[7] = 48;
    memset(dst + 8, 0, 24);
    dst[16] = 2;
    int ret = generateChallengeNonce(ctx, dst + 32);
    if (ret < 0) return ret;
    ctx->challengeTick = sceRtc_driver_C0F36B91();
    return 48;
}

static int applyClockResponse(MgvMediaCtx *ctx, const u8 *payload) {
    if (bswap32(*(const u32 *)(payload + 8)) != 0)
        return MGV_ERR_MSG_FORMAT;
    if (ctx->challengeTick == 0)
        return MGV_ERR_MSG_SESSION;
    u64 now = sceRtc_driver_C0F36B91();
    u64 timeoutMs = bswap32(*(const u32 *)(payload + 12));
    u64 maxDelta = timeoutMs * (u64)(sceRtc_driver_C66D9686() / 1000u);
    if (now - ctx->challengeTick > maxDelta)
        return MGV_ERR_MSG_TIMEOUT;
    if (setSecureClockFromBcd(payload) < 0)
        return MGV_ERR_MSG_FORMAT;
    ctx->challengeTick = 0;
    memset(ctx->challengeNonce, 0, 16);
    return 0;
}

static int parseBindPayload(const u8 *payload, u32 msgLen) {
    if (payload[2] != 1 || payload[3] != 2 || payload[4] != 1)
        return MGV_ERR_MSG_FORMAT;
    u32 bodyLen = readBe16(payload);
    if (msgLen < bodyLen + 80u)
        return MGV_ERR_MSG_FORMAT;
    u32 krLen = readBe16(payload + 8);
    if (krLen < 0x40u || bodyLen < krLen)
        return MGV_ERR_MSG_FORMAT;
    if ((u32)parseKeyringHeader(payload + 16) != krLen)
        return MGV_ERR_MSG_FORMAT;
    u32 lckLen = krLen + readBe16(payload + 10);
    if (lckLen >= 0x1231u || bodyLen < lckLen)
        return MGV_ERR_MSG_FORMAT;
    u32 ekbLen = readBe16(payload + 12);
    if (ekbLen < 48u || ekbLen > 0x4000u || bodyLen < lckLen + ekbLen)
        return MGV_ERR_MSG_FORMAT;
    return (int)lckLen;
}

static int buildBindChallenge(MgvMediaCtx *ctx, const MgvBufDesc *outDesc) {
    if (ctx->state < 2u) return MGV_ERR_NOT_OPENED;
    if (outDesc->size < 0x40u) return MGV_ERR_INVALID_ARG;
    u8 *dst = (u8 *)outDesc->ptr;
    *(u32 *)dst = 0x314E4B4Du; /* "MKN1" */
    *(u16 *)(dst + 4) = 3;
    dst[6] = 0; dst[7] = 64;
    memset(dst + 8, 0, 24);
    int ret = getMediaId(ctx, dst + 32);
    if (ret < 0) return ret;
    ret = checkMediaWritable(ctx);
    if (ret < 0) return ret;
    ret = generateChallengeNonce(ctx, dst + 48);
    if (ret < 0) return ret;
    return 64;
}

static int rewrapBindKey(MgvMediaCtx *ctx, u8 *payload, int lckLen, const MgvBufDesc *ekbDesc) {
    MgvBufDesc innerEkb;
    innerEkb.ptr  = payload + 16 + (lckLen & ~15);
    innerEkb.size = readBe16(payload + 12);

    int keySlot = deriveMsgKeySlot(payload + 48, &innerEkb);
    if (keySlot < 0) return keySlot;

    int ekbSlot = loadEkbKeySlot(ekbDesc);
    int ret = ekbSlot;
    if (ekbSlot >= 0) {
        ret = verifyEkbMac(ctx, -1, NULL, ekbDesc, ekbSlot);
        if (ret >= 0)
            ret = ctx->exportKey(payload + 48, keySlot, ekbSlot);
        sceMgr_driver_E6A203F8(ekbSlot);
        if (ret >= 0)
            return keySlot;
    }
    sceMgr_driver_E6A203F8(keySlot);
    return ret;
}

/* =========================================================================
 * Exported Functions (sceMgVideo_driver + syslib)
 * ========================================================================= */

int sceMgVideo_driver_EC989B72(void) {
    return 0;
}

int sceMgVideo_driver_B593C4A9(void);

int module_stop(void) {
    while (sceMgVideo_driver_B593C4A9() > 0)
        ;
    return 0;
}

int sceMgVideo_driver_44B6C121(void) {
    MgvMediaCtx *ctx = NULL;
    int initRet = mgvCoreInit();
    if (initRet < 0)
        return initRet;

    int rMs = allocMediaCtx(&ctx, 32);
    if (rMs >= 0) {
        rMs = openMediaCtx(ctx);
        if (rMs < 0) freeMediaCtx(ctx);
    }

    int rHdd = allocMediaCtx(&ctx, 16);
    if (rHdd >= 0) {
        rHdd = openMediaCtx(ctx);
        if (rHdd < 0) freeMediaCtx(ctx);
    }

    if (rMs >= 0) return 0;
    if (rHdd >= 0) return initRet;
    mgvCoreTerm();
    return rMs;
}

int sceMgVideo_driver_B593C4A9(void) {
    MgvMediaCtx *ctx = NULL;
    if (findMediaCtx(16, &ctx, 1u) >= 0)
        closeMediaCtx(ctx);
    if (findMediaCtx(32, &ctx, 1u) >= 0)
        closeMediaCtx(ctx);
    return mgvCoreTerm();
}

int sceMgVideo_driver_7894CCE9(int devType) {
    MgvMediaCtx *ctx = NULL;
    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret < 0) return ret;
    if (ctx->state >= 2u)
        return (int)ctx->mediaFlags;
    freeAllContentSlots();
    ret = ctx->openMedia(ctx, g_rngCtx);
    if (ret < 0) return ret;
    ret = loadIcvTable(ctx, g_pageBuf);
    if (ret < 0) return ret;
    ctx->state = 2;
    return (int)ctx->mediaFlags;
}

int sceMgVideo_driver_C189C37B(int devType, u32 keyIdx, const MgvBufDesc *ikvArg, const MgvBufDesc *ekbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc ikvDesc, ekbDesc;
    u32 icv[4], hash[2], ekbMac[2];
    int ekbSlot = -1;
    int ret;

    ret = findMediaCtx(devType, &ctx, 2u);
    if (ret < 0) goto done;
    if (keyIdx >= 32u) { ret = MGV_ERR_INVALID_ARG; goto done; }
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;

    if (ikvDesc.ptr != NULL) {
        if (ikvDesc.size != 16048u) { ret = MGV_ERR_INVALID_DATA; goto done; }
        ret = copyBufDesc(&ekbDesc, ekbArg);
        if (ret < 0) goto done;
        ret = stripEkb2Header(&ekbDesc);
        if (ret < 0) goto done;
        if (bswap32(*(const u32 *)ekbDesc.ptr) != bswap32(*(const u32 *)((const u8 *)ikvDesc.ptr + 48))) {
            ret = MGV_ERR_INVALID_ARG;
            goto done;
        }
        ekbSlot = loadEkbKeySlot(&ekbDesc);
        if (ekbSlot < 0) { ret = ekbSlot; goto done; }
        ret = verifyIkv(ctx, icv, hash, ekbMac, (const u8 *)ikvDesc.ptr, ekbSlot);
        if (ret < 0) goto free_slot;
    } else {
        memset(icv, 0, sizeof(icv));
    }

    if (memcmp(icv, ctx->icvTable + 16 * keyIdx, 16) != 0) {
        ret = MGV_ERR_HASH_MISMATCH;
        goto free_slot;
    }

    if (ikvDesc.ptr != NULL) {
        memcpy(ctx->ekbMacTable + 8 * keyIdx, ekbMac, 8);
        memcpy(ctx->hashTable + 8 * keyIdx, hash, 8);
    } else {
        memset(ctx->ekbMacTable + 8 * keyIdx, 0, 8);
        memset(ctx->hashTable + 8 * keyIdx, 0, 8);
    }
    ctx->state = 3;
    ctx->verifiedMask |= (1u << keyIdx);
    ret = 0;

free_slot:
    if (ekbSlot >= 0)
        sceMgr_driver_E6A203F8(ekbSlot);
done:
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_8CFD3E08(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *lckArg, const MgvBufDesc *ikvArg, const MgvBufDesc *ekbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc lckDesc, ikvDesc, ekbDesc;
    u8 keyringHdr[96];
    u8 cid[24];
    int slot = -1;
    int ret;

    ret = findMediaCtx(devType, &ctx, 2u);
    if (ret < 0) goto done;
    if (ctx->state < 3u) { ret = MGV_ERR_NOT_VERIFIED; goto done; }
    if (keyIdx >= 32u || (u32)(titleIdx - 1) >= 999u) { ret = MGV_ERR_INVALID_ARG; goto done; }

    ret = copyBufDesc(&lckDesc, lckArg);
    if (ret < 0) goto done;
    ret = checkLckDesc(&lckDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto done;

    slot = allocContentSlot(ctx);
    if (slot < 0) { ret = slot; goto done; }

    MgvContentCtx *cctx = getContentSlot((u32)slot);
    int needLock = (ctx->mediaFlags & 0x20u) != 0;
    if (needLock) sceKernelPowerLock(0);

    int keySlot = openLck(ctx, keyringHdr, cid, (int)keyIdx, titleIdx, &lckDesc, (const u8 *)ikvDesc.ptr, &ekbDesc);
    if (keySlot < 0) {
        if (needLock) sceKernelPowerUnlock(0);
        ret = keySlot;
        goto fail_slot;
    }
    cctx->mgrKeySlot = keySlot;
    if (needLock) {
        ret = saveContentKey(cctx);
        sceKernelPowerUnlock(0);
        if (ret < 0) goto fail_slot;
    }

    memcpy(cctx->keys, keyringHdr + 48, 48);
    cctx->keyCount = (s16)readBe16(keyringHdr + 20);
    memcpy(cctx->cid, cid, 24);
    ret = slot;
    goto done;

fail_slot:
    if (slot >= 0)
        freeContentSlot((u32)slot);
done:
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_68E28BCE(u32 contentIdx) {
    if (g_initRefCount > 0 && getContentSlot(contentIdx) != NULL)
        freeContentSlot(contentIdx);
    return 0;
}

int sceMgVideo_driver_5BDB10E1(u32 contentIdx, const u32 *sinfBox) {
    int streamIdx = -1;
    int ret = MGV_ERR_NOT_INITIALIZED;
    if (g_initRefCount <= 0) goto done;

    MgvContentCtx *cctx = getContentSlot(contentIdx);
    if (cctx == NULL) { ret = MGV_ERR_INVALID_ARG; goto done; }
    if (cctx->media->state < 2u) { ret = MGV_ERR_NOT_OPENED; goto done; }
    if (sinfBox == NULL) { ret = MGV_ERR_INVALID_ARG; goto done; }

    int sinfSize = checkBoxSizeAddr(sinfBox);
    if (sinfSize < 0) { ret = sinfSize; goto done; }

    const u32 *schi = NULL;
    ret = parseSinfBox(&schi, (const u8 *)sinfBox, (u32)sinfSize);
    if (ret < 0) goto done;

    streamIdx = allocStreamSlot(cctx);
    if (streamIdx < 0) { ret = streamIdx; goto done; }

    MgvStreamSlot *slot = &cctx->streams[streamIdx];
    u32 alg = 0;
    u8 cid[24];
    int keyId = parseSchiBox(&alg, cid, slot->iv, schi);
    if (keyId < 0) { ret = keyId; goto done; }
    if (memcmp(cid, cctx->cid, 24) != 0 || keyId >= cctx->keyCount) {
        ret = MGV_ERR_INVALID_DATA;
        goto done;
    }
    if (keyId >= 3) { ret = MGV_ERR_UNSUPPORTED_VER; goto done; }

    slot->streamType = (keyId == 0) ? 1u : (keyId == 1) ? 2u : 0u;
    const u8 *keyPtr = cctx->keys + 16 * keyId;
    if (alg == 8) {
        slot->keyPtr = keyPtr;
        slot->rangeMin = bswap32(*(const u32 *)(keyPtr + 8));
        slot->rangeMax = bswap32(*(const u32 *)(keyPtr + 12));
    } else if (alg == 9) {
        slot->keyPtr = NULL;
    } else {
        ret = MGV_ERR_UNSUPPORTED_VER;
        goto done;
    }
    slot->keyMode = (s32)(uintptr_t)keyPtr;
    ret = (int)((contentIdx << 8) | (u8)streamIdx);

done:
    if (ret < 0 && (s32)contentIdx >= 0)
        freeStreamSlot(contentIdx, streamIdx);
    return ret;
}

int sceMgVideo_driver_3F5EC8C7(int streamHandle) {
    u32 cidx = (u32)streamHandle >> 8;
    u8 sidx = (u8)streamHandle;
    if (g_initRefCount > 0 && getContentSlot(cidx) != NULL)
        freeStreamSlot(cidx, sidx);
    return 0;
}

int sceMgVideo_driver_8B2D15A2(int streamHandle) {
    if (g_initRefCount <= 0)
        return MGV_ERR_NOT_INITIALIZED;
    MgvStreamSlot *slot = NULL;
    int ret = lookupStream(streamHandle, &slot, NULL);
    if (ret < 0) return ret;
    return (int)slot->streamType;
}

int sceMgVideo_driver_68908973(u32 contentIdx, const u8 *hdr, u32 hdrSize, u32 format) {
    int streamIdx = -1;
    int ret = MGV_ERR_NOT_INITIALIZED;
    if (g_initRefCount <= 0) goto done;

    MgvContentCtx *cctx = getContentSlot(contentIdx);
    if (cctx == NULL) { ret = MGV_ERR_INVALID_ARG; goto done; }
    if (cctx->media->state < 2u) { ret = MGV_ERR_NOT_OPENED; goto done; }
    if (hdr == NULL || hdrSize < 0x40u) { ret = MGV_ERR_INVALID_ARG; goto done; }
    ret = checkUserRangeUncached(hdr, hdrSize);
    if (ret < 0) goto done;

    streamIdx = allocStreamSlot(cctx);
    if (streamIdx < 0) { ret = streamIdx; goto done; }

    MgvStreamSlot *slot = &cctx->streams[streamIdx];
    const u8 *ivSrc;
    const u8 *cidSrc;
    int keyId;
    int encMode;
    switch (format) {
        case 2:
            encMode = readBe16(hdr + 22);
            ivSrc = (hdrSize >= 0xA0u) ? (hdr + 144) : (const u8 *)g_zero16;
            keyId = 0;
            cidSrc = hdr + 24;
            break;
        case 3:
            encMode = readBe16(hdr + 8);
            ivSrc = (hdrSize >= 0x50u) ? (hdr + 64) : (const u8 *)g_zero16;
            keyId = 0;
            cidSrc = hdr + 32;
            break;
        case 4:
            encMode = readBe16(hdr + 8);
            ivSrc = (hdrSize >= 0x40u) ? (hdr + 48) : (const u8 *)g_zero16;
            keyId = 2;
            cidSrc = hdr + 16;
            break;
        default:
            ret = MGV_ERR_INVALID_ARG;
            goto done;
    }

    memcpy(slot->iv, ivSrc, 16);
    if (memcmp(cidSrc, cctx->cid, 24) != 0 || keyId >= cctx->keyCount) {
        ret = MGV_ERR_INVALID_DATA;
        goto done;
    }

    const u8 *keyPtr = cctx->keys + 16 * keyId;
    if (encMode == 0) {
        slot->keyMode = -1;
    } else if (encMode == 1) {
        slot->keyPtr = NULL;
        slot->keyMode = (s32)(uintptr_t)keyPtr;
    } else if (encMode == 2) {
        if (keyId != 2) {
            slot->keyPtr = keyPtr;
            slot->rangeMin = bswap32(*(const u32 *)(keyPtr + 8));
            slot->rangeMax = bswap32(*(const u32 *)(keyPtr + 12));
        } else {
            slot->keyPtr = NULL;
        }
        slot->keyMode = (s32)(uintptr_t)keyPtr;
    } else {
        ret = MGV_ERR_UNSUPPORTED_VER;
        goto done;
    }
    ret = (int)((contentIdx << 8) | (u8)streamIdx);

done:
    if (ret < 0 && (s32)contentIdx >= 0)
        freeStreamSlot(contentIdx, streamIdx);
    return ret;
}

int sceMgVideo_driver_AD6C48A0(int streamHandle) {
    u32 cidx = (u32)streamHandle >> 8;
    u8 sidx = (u8)streamHandle;
    if (g_initRefCount > 0 && getContentSlot(cidx) != NULL)
        freeStreamSlot(cidx, sidx);
    return 0;
}

int sceMgVideo_driver_6F1D18F4(int streamHandle, u8 *dst, const u8 *src, u32 size) {
    MgvStreamSlot *slot = NULL;
    MgvContentCtx *cctx = NULL;
    int ret = MGV_ERR_NOT_INITIALIZED;

    if (g_initRefCount <= 0) goto done;
    if (streamHandle < 0 || dst == NULL || src == NULL || (size & 0xFu) != 0 || size < 0x11u) {
        ret = MGV_ERR_INVALID_ARG;
        goto done;
    }
    ret = checkUserRangeUncached(src, size);
    if (ret < 0) goto done;
    ret = checkUserRangeUncached(dst, size);
    if (ret < 0) goto done;

    ret = lookupStream(streamHandle, &slot, &cctx);
    if (ret < 0) goto done;
    if (slot->keyMode == 0 || slot->keyMode == -1) {
        ret = MGV_ERR_INVALID_ARG;
        goto done;
    }

    u32 payloadSize = size - 16u;
    const void *keyData = (const void *)(uintptr_t)slot->keyMode;
    const void *ivData  = src;
    u32 ctrKey[4];

    if (slot->keyPtr != NULL) {
        u32 rawIdx = *(const u32 *)(src + 12);
        u32 idx = bswap32(rawIdx);
        if (idx < slot->rangeMin || idx >= slot->rangeMax) {
            ret = MGV_ERR_INVALID_DATA;
            goto done;
        }
        ctrKey[0] = *(const u32 *)(slot->keyPtr + 0);
        ctrKey[1] = *(const u32 *)(slot->keyPtr + 4);
        ctrKey[2] = 0;
        ctrKey[3] = rawIdx;
        keyData = ctrKey;
        ivData  = slot->iv;
    }

    int decSlot = sceMgr_driver_C0B28CED(5);
    if (decSlot < 0) {
        ret = mapMgrError(decSlot);
        goto done;
    }

    sceKernelPowerLock(0);
    ret = ensureKeyLoaded(cctx);
    if (ret >= 0)
        ret = sceMgr_driver_7DB1B0E5(decSlot, keyData, cctx->mgrKeySlot);
    if (ret >= 0)
        ret = sceMgr_driver_9AE6ED58(dst, src + 16, payloadSize, decSlot, ivData);
    sceKernelPowerUnlock(0);
    sceMgr_driver_E6A203F8(decSlot);

    if (ret < 0) {
        ret = mapMgrError(ret);
    } else {
        u8 pad = dst[payloadSize - 1];
        ret = (pad < 17u) ? (int)(payloadSize - pad) : MGV_ERR_INVALID_DATA;
    }

done:
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && cctx != NULL && cctx->media != NULL)
        cctx->media->state = 1;
    return ret;
}

static int cryptStreamCommon(int streamHandle, u8 *dst, const u8 *src, u32 size, u32 blockIdx, const void *ivArg,
                             int (*cryptFn)(void *, const void *, u32, int, const void *)) {
    MgvStreamSlot *slot = NULL;
    MgvContentCtx *cctx = NULL;
    int ret = MGV_ERR_NOT_INITIALIZED;

    if (g_initRefCount <= 0) goto done;
    if (streamHandle < 0 || dst == NULL || src == NULL || size < 16u || (size & 0xFu) != 0) {
        ret = MGV_ERR_INVALID_ARG;
        goto done;
    }
    ret = checkUserRangeUncached(src, size);
    if (ret < 0) goto done;
    ret = checkUserRangeUncached(dst, size);
    if (ret < 0) goto done;
    if (ivArg != NULL && ((getK1() << 11) & (u32)(uintptr_t)ivArg & 0x80000000u) != 0) {
        ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        goto done;
    }

    ret = lookupStream(streamHandle, &slot, &cctx);
    if (ret < 0) goto done;
    if (slot->keyMode == 0) {
        ret = MGV_ERR_INVALID_ARG;
        goto done;
    }
    if (slot->keyMode == -1) {
        memcpy(dst, src, size);
        return (int)size;
    }

    const void *keyData = (const void *)(uintptr_t)slot->keyMode;
    u32 ctrKey[4];
    if (slot->keyPtr != NULL) {
        if (blockIdx < slot->rangeMin || blockIdx >= slot->rangeMax) {
            ret = MGV_ERR_INVALID_DATA;
            goto done;
        }
        ctrKey[0] = *(const u32 *)(slot->keyPtr + 0);
        ctrKey[1] = *(const u32 *)(slot->keyPtr + 4);
        ctrKey[2] = 0;
        ctrKey[3] = bswap32(blockIdx);
        keyData = ctrKey;
    }

    const void *iv = (ivArg != NULL) ? ivArg : slot->iv;
    int decSlot = sceMgr_driver_C0B28CED(5);
    if (decSlot < 0) {
        ret = mapMgrError(decSlot);
        goto done;
    }

    sceKernelPowerLock(0);
    ret = ensureKeyLoaded(cctx);
    if (ret >= 0)
        ret = sceMgr_driver_7DB1B0E5(decSlot, keyData, cctx->mgrKeySlot);
    if (ret >= 0)
        ret = cryptFn(dst, src, size, decSlot, iv);
    sceKernelPowerUnlock(0);
    sceMgr_driver_E6A203F8(decSlot);

    ret = (ret < 0) ? mapMgrError(ret) : (int)size;

done:
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && cctx != NULL && cctx->media != NULL)
        cctx->media->state = 1;
    return ret;
}

int sceMgVideo_driver_26A29916(int streamHandle, u8 *dst, const u8 *src, u32 size, u32 blockIdx, const void *ivArg) {
    return cryptStreamCommon(streamHandle, dst, src, size, blockIdx, ivArg, sceMgr_driver_9AE6ED58);
}

int sceMgVideo_driver_AFA4D611(int streamHandle, u8 *dst, const u8 *src, u32 size, u32 blockIdx, const void *ivArg) {
    return cryptStreamCommon(streamHandle, dst, src, size, blockIdx, ivArg, sceMgr_driver_0EBA1587);
}

int sceMgVideo_driver_37E95490(int devType, u32 keyIdx, const MgvBufDesc *ikvArg,
                               const s16 *indices, int count, const MgvBufDesc *ekbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc ikvDesc, ekbDesc;
    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret < 0) goto done;
    if (keyIdx >= 32u) { ret = MGV_ERR_INVALID_ARG; goto done; }

    if (count == -1) {
        memset(ctx->ekbMacTable + 8 * keyIdx, 0, 8);
        memset(ctx->hashTable + 8 * keyIdx, 0, 8);
        memset(ctx->icvTable + 16 * keyIdx, 0, 16);
        ctx->state = 3;
        ctx->verifiedMask |= (1u << keyIdx);
        return 0;
    }
    if (count < 0) { ret = MGV_ERR_INVALID_ARG; goto done; }
    if (ctx->state < 3u || (ctx->verifiedMask & (1u << keyIdx)) == 0) {
        ret = MGV_ERR_NOT_VERIFIED;
        goto done;
    }

    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto done;
    if (count > 0) {
        if (indices == NULL) { ret = MGV_ERR_INVALID_ARG; goto done; }
        ret = checkUserRangeUncached(indices, (u32)(2 * count));
        if (ret < 0) goto done;
    }
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto done;

    int ekbSlot = loadEkbKeySlot(&ekbDesc);
    if (ekbSlot < 0) { ret = ekbSlot; goto done; }

    u8 *ikvBuf = (u8 *)ikvDesc.ptr;
    u8 hdr48[48];
    memcpy(hdr48, ikvBuf + 16, 48);
    u32 ekbId = *(const u32 *)ekbDesc.ptr;
    memcpy(hdr48 + 32, &ekbId, 4);

    u8 newIcv[16];
    u32 newHash[2], newEkbMac[2];
    int slot = deriveIkvMacs(ctx, newIcv, newHash, newEkbMac, hdr48, ekbSlot, ctx->macKeySlot);
    ret = slot;
    if (slot >= 0) {
        ret = verifyMediaId(ctx, (const u32 *)(hdr48 + 16));
        if (ret >= 0)
            ret = verifyEkbMac(ctx, (int)keyIdx, newEkbMac, &ekbDesc, ekbSlot);
        if (ret >= 0) {
            IkvUpdateArgs uargs = {
                ctx, (int)keyIdx, ikvBuf + 64, indices, count,
                g_zero16, newIcv, newHash, slot
            };
            ret = updateIkvEntries(&uargs);
            if (ret >= 0) {
                memcpy(ikvBuf + 48, &ekbId, 4);
                memcpy(ctx->ekbMacTable + 8 * keyIdx, newEkbMac, 8);
                memcpy(ctx->hashTable + 8 * keyIdx, newHash, 8);
                memcpy(ctx->icvTable + 16 * keyIdx, newIcv, 16);
                *(u16 *)(ikvBuf + 6) = 35;
                ret = 0;
            }
        }
        sceMgr_driver_E6A203F8(slot);
    }
    sceMgr_driver_E6A203F8(ekbSlot);

done:
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_62FF3C70(int devType) {
    MgvMediaCtx *ctx = NULL;
    int ret = findMediaCtx(devType, &ctx, 3u);
    if (ret >= 0) {
        clearChallenge(ctx);
        ret = ctx->readPage(ctx, 1, g_pageBuf);
        if (ret >= 0 || (ret == MGV_ERR_MGR_ERROR_23 && (ctx->mediaFlags & 8u) != 0)) {
            memcpy(g_pageBuf + 8, ctx->icvTable, 512);
            ret = ctx->writePage(ctx, 1, g_pageBuf);
        }
    }
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_39712740(int devType) {
    MgvMediaCtx *ctx = NULL;
    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret >= 0) {
        ctx->state = 1;
        ret = ctx->openMedia(ctx, g_rngCtx);
        if (ret >= 0) {
            ctx->verifiedMask = 0;
            ret = ctx->erasePage(ctx, 1, g_pageBuf);
            if (ret >= 0) {
                ret = loadIcvTable(ctx, g_pageBuf);
                if (ret >= 0) {
                    ctx->state = 3;
                    ctx->verifiedMask = 0xFFFFFFFFu;
                    ret = 0;
                }
            }
        }
    }
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_2C046DAE(int devType, u32 *out, u32 outSize, const MgvBufDesc *lckArg, u32 queryType) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc lckDesc;
    u32 tmp[13];

    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret < 0) return ret;
    if (out == NULL) return MGV_ERR_INVALID_ARG;
    ret = checkUserRange(out, outSize);
    if (ret < 0) return ret;
    ret = copyBufDesc(&lckDesc, lckArg);
    if (ret < 0) return ret;
    ret = checkLckDesc(&lckDesc);
    if (ret < 0) return ret;

    const u8 *body = (const u8 *)lckDesc.ptr + 16;
    int bodyLen = (int)(lckDesc.size - 16u);

    switch (queryType) {
        case 1:
            if (outSize < 0x40u) return MGV_ERR_INVALID_ARG;
            ret = queryLicenseInfo(ctx->mediaType, 1, tmp, body, bodyLen);
            if (ret < 0) return mapRuleError(ret);
            memset(out, 0, outSize);
            memcpy(out, tmp, 52);
            return 0;
        case 2:
            if (outSize < 0x14u) return MGV_ERR_INVALID_ARG;
            ret = queryLicenseInfo(ctx->mediaType, 2, tmp, body, bodyLen);
            if (ret < 0) return mapRuleError(ret);
            memcpy(out, tmp, 20);
            return 0;
        case 3:
        case 4:
            if (outSize < 0x0Cu) return MGV_ERR_INVALID_ARG;
            ret = queryLicenseInfo(ctx->mediaType, (int)queryType, tmp, body, bodyLen);
            if (ret < 0) return mapRuleError(ret);
            memcpy(out, tmp, 12);
            return 0;
        default:
            return MGV_ERR_INVALID_ARG;
    }
}

int sceMgVideo_driver_85471E34(int devType, const MgvBufDesc *ekbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc desc;
    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret < 0) return ret;
    ret = copyBufDesc(&desc, ekbArg);
    if (ret < 0) return ret;
    if (desc.ptr == NULL)
        desc.size = 0;
    ret = ctx->getEkb(desc.ptr, desc.size);
    if (ret >= 0 && desc.ptr != NULL) {
        if (desc.size >= 7u) ((u8 *)desc.ptr)[6] = 35;
        if (desc.size >= 8u) ((u8 *)desc.ptr)[7] = 0;
    }
    return ret;
}

static int exportLicenseCommon(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *outArg, const MgvBufDesc *lckArg,
                               const MgvBufDesc *ekbArg, const MgvBufDesc *ikvArg,
                               const MgvBufDesc *newEkbArg, int op) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc outDesc, lckDesc, ekbDesc, ikvDesc, newEkbDesc;
    void *tmpBuf = NULL;
    int keySlot = -1;
    int newEkbSlot = -1;
    int newSlot = -1;
    int ret;

    ret = findMediaCtx(devType, &ctx, 3u);
    if (ret < 0) goto cleanup;
    if (keyIdx >= 32u || (u32)(titleIdx - 1) >= 999u) { ret = MGV_ERR_INVALID_ARG; goto cleanup; }

    ret = copyBufDesc(&lckDesc, lckArg);
    if (ret < 0) goto cleanup;
    ret = checkLckDesc(&lckDesc);
    if (ret < 0) goto cleanup;
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto cleanup;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto cleanup;
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto cleanup;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto cleanup;
    if (newEkbArg != NULL) {
        ret = copyBufDesc(&newEkbDesc, newEkbArg);
        if (ret < 0) goto cleanup;
        ret = stripEkb2Header(&newEkbDesc);
        if (ret < 0) goto cleanup;
    }
    ret = copyBufDesc(&outDesc, outArg);
    if (ret < 0) goto cleanup;
    u32 totalSize = lckDesc.size + 16u;
    ret = checkMinDesc(&outDesc, totalSize);
    if (ret < 0) goto cleanup;
    if (totalSize >= 0x52B1u) { ret = MGV_ERR_INVALID_DATA; goto cleanup; }

    ret = allocTempBuf(&tmpBuf, totalSize);
    if (ret < 0) goto cleanup;

    u32 bodyLen = lckDesc.size - 16u;
    memcpy((u8 *)tmpBuf + 16, (const u8 *)lckDesc.ptr + 16, bodyLen);

    keySlot = verifyLckForTransfer(ctx, (int)keyIdx, titleIdx, (const u8 *)tmpBuf + 16,
                                   (int)bodyLen, (const u8 *)ikvDesc.ptr, &ekbDesc, op);
    if (keySlot < 0) { ret = keySlot; goto cleanup; }

    u32 pktBodyLen = (op == 3) ? 16u : (bodyLen + 16u);
    int pktType = (op == 3) ? 509 : 510;
    u8 mac[16];
    ret = buildMgl3Packet(mac, (u8 *)tmpBuf, (int)pktBodyLen, keySlot, pktType);
    if (ret < 0) goto cleanup;

    memcpy(outDesc.ptr, tmpBuf, pktBodyLen);
    if (op == 3) {
        memcpy((u8 *)outDesc.ptr + pktBodyLen, mac, 16);
        ret = (int)(pktBodyLen + 16u);
        goto cleanup;
    }

    sceMgr_driver_E6A203F8(keySlot);
    keySlot = -1;

    newEkbSlot = loadEkbKeySlot(&newEkbDesc);
    if (newEkbSlot < 0) { ret = newEkbSlot; goto cleanup; }

    u8 *ikvBuf = (u8 *)ikvDesc.ptr;
    u8 hdr48[48];
    memcpy(hdr48, ikvBuf + 16, 48);
    u32 newEkbId = *(const u32 *)newEkbDesc.ptr;
    memcpy(hdr48 + 32, &newEkbId, 4);

    u8 newIcv[16];
    u32 newHash[2], newEkbMac[2];
    newSlot = deriveIkvMacs(ctx, newIcv, newHash, newEkbMac, hdr48, newEkbSlot, ctx->macKeySlot);
    if (newSlot < 0) { ret = newSlot; goto cleanup; }

    ret = verifyMediaId(ctx, (const u32 *)(hdr48 + 16));
    if (ret < 0) goto cleanup;
    ret = verifyEkbMac(ctx, (int)keyIdx, newEkbMac, &newEkbDesc, newEkbSlot);
    if (ret < 0) goto cleanup;

    s16 idx = (s16)titleIdx;
    u32 zeros[4] = { 0, 0, 0, 0 };
    IkvUpdateArgs uargs = {
        ctx, (int)keyIdx, ikvBuf + 64, &idx, 1, zeros, newIcv, newHash, newSlot
    };
    ret = updateIkvEntries(&uargs);
    if (ret < 0) goto cleanup;

    memcpy(ikvBuf + 48, &newEkbId, 4);
    memcpy(ctx->ekbMacTable + 8 * keyIdx, newEkbMac, 8);
    memcpy(ctx->hashTable + 8 * keyIdx, newHash, 8);
    memcpy(ctx->icvTable + 16 * keyIdx, newIcv, 16);
    ikvBuf[6] = 35;
    ikvBuf[7] = 0;
    memcpy((u8 *)outDesc.ptr + pktBodyLen, mac, 16);
    ret = (int)(pktBodyLen + 16u);

cleanup:
    if (newSlot >= 0)    sceMgr_driver_E6A203F8(newSlot);
    if (newEkbSlot >= 0) sceMgr_driver_E6A203F8(newEkbSlot);
    if (keySlot >= 0)    sceMgr_driver_E6A203F8(keySlot);
    if (tmpBuf != NULL)  freeTempBuf(tmpBuf);
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_913FBC5A(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *outArg, const MgvBufDesc *lckArg,
                               const MgvBufDesc *ekbArg, const MgvBufDesc *ikvArg,
                               const MgvBufDesc *newEkbArg) {
    return exportLicenseCommon(devType, keyIdx, titleIdx, outArg, lckArg, ekbArg, ikvArg, newEkbArg, 3);
}

int sceMgVideo_driver_70960650(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *outArg, const MgvBufDesc *lckArg,
                               const MgvBufDesc *ekbArg, const MgvBufDesc *ikvArg,
                               const MgvBufDesc *newEkbArg) {
    if (newEkbArg == NULL)
        return MGV_ERR_INVALID_ARG;
    return exportLicenseCommon(devType, keyIdx, titleIdx, outArg, lckArg, ekbArg, ikvArg, newEkbArg, 4);
}

int sceMgVideo_driver_362459AD(int devType, const MgvBufDesc *outArg, u8 *keyInfo, const u8 *avout8, u32 mode) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc outDesc;
    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret < 0) return ret;
    ret = copyBufDesc(&outDesc, outArg);
    if (ret < 0) return ret;
    if (checkMinDesc(&outDesc, 0x5240u) < 0)
        return MGV_ERR_INVALID_ARG;
    ret = checkUserRangeUncached(keyInfo, 144);
    if (ret < 0) return ret;
    ret = checkUserRangeUncached(avout8, 8);
    if (ret < 0) return ret;
    if (mode >= 3u)
        return MGV_ERR_UNSUPPORTED_VER;
    return createLicenseBody(&outDesc, keyInfo, avout8, (int)mode);
}

int sceMgVideo_driver_AC0757DF(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *pktArg, const MgvBufDesc *ekbArg,
                               const MgvBufDesc *ikvArg, const MgvBufDesc *ikvEkbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc pktDesc, ekbDesc, ikvDesc, ikvEkbDesc;
    void *tmpBuf = NULL;
    int ret;

    ret = findMediaCtx(devType, &ctx, 3u);
    if (ret < 0) goto done;
    if (keyIdx >= 32u || (u32)(titleIdx - 1) >= 999u) { ret = MGV_ERR_INVALID_ARG; goto done; }

    ret = copyBufDesc(&pktDesc, pktArg);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvEkbDesc, ikvEkbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ikvEkbDesc);
    if (ret < 0) goto done;
    if ((ctx->verifiedMask & (1u << keyIdx)) == 0) { ret = MGV_ERR_NOT_VERIFIED; goto done; }

    u32 allocLen = (pktDesc.size < 0x52B1u) ? pktDesc.size : 21168u;
    ret = allocTempBuf(&tmpBuf, allocLen);
    if (ret < 0) goto done;
    memcpy(tmpBuf, pktDesc.ptr, allocLen);

    int pktLen = parseMgl3Header((const u8 *)tmpBuf, (int)pktDesc.size);
    if (pktLen < 0) { ret = pktLen; goto done; }
    if ((u32)pktLen >= 0x52B1u) { ret = MGV_ERR_INVALID_DATA; goto done; }

    u8 *payload = (u8 *)tmpBuf + 16;
    u32 payloadLen = (u32)(pktLen - 16);
    int ekbSlot = loadEkbKeySlot(&ekbDesc);
    if (ekbSlot < 0) { ret = ekbSlot; goto done; }

    int keySlot = verifyMgl3Packet(ctx, (const u8 *)tmpBuf, payload, ekbSlot);
    if (keySlot >= 0) {
        u32 krBytes = (u32)readBe16(payload + 12) * ((u32)readBe16(payload + 14) + 1u);
        int bindRet = checkBindLicense(ctx->mediaType, payload + krBytes, (int)(payloadLen - 16u - krBytes));
        if (bindRet < 0) {
            ret = mapRuleError(bindRet);
            sceMgr_driver_E6A203F8(keySlot);
        } else {
            ret = ctx->exportKey(payload + 32, keySlot, ekbSlot);
            if (ret >= 0) {
                u8 mac[16];
                ret = sceMgr_driver_DA7D8C82(mac, payload, NULL, payloadLen - 16u, keySlot);
                if (ret < 0) ret = mapMgrError(ret);
                if (ret >= 0) {
                    sceMgr_driver_E6A203F8(keySlot);
                    keySlot = -1;
                    ret = writeSingleIkvEntry(ctx, keyIdx, titleIdx, mac, (u8 *)ikvDesc.ptr, &ikvEkbDesc);
                    if (ret >= 0) {
                        u8 *out = (u8 *)pktDesc.ptr;
                        *(u32 *)out = 0x53434B4Cu; /* "LCKS" */
                        *(u16 *)(out + 4) = 0;
                        out[6] = 35; out[7] = 0;
                        *(u32 *)(out + 8) = *(const u32 *)ekbDesc.ptr;
                        *(u32 *)(out + 12) = 0;
                        memcpy(out + 16, payload, payloadLen - 16u);
                        ret = (int)payloadLen;
                    }
                }
            }
            if (keySlot >= 0)
                sceMgr_driver_E6A203F8(keySlot);
        }
    } else {
        ret = keySlot;
    }
    sceMgr_driver_E6A203F8(ekbSlot);

done:
    if (tmpBuf != NULL)
        freeTempBuf(tmpBuf);
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_B8F2B2B5(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *lckArg, const MgvBufDesc *ekbArg,
                               const MgvBufDesc *ikvArg, const MgvBufDesc *ikvEkbArg, int reserved) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc lckDesc, ekbDesc, ikvDesc, ikvEkbDesc;
    void *tmpBuf = NULL;
    int ret;

    ret = findMediaCtx(devType, &ctx, 3u);
    if (ret < 0) goto done;
    if (keyIdx >= 32u || (u32)(titleIdx - 1) >= 999u) { ret = MGV_ERR_INVALID_ARG; goto done; }

    ret = copyBufDesc(&lckDesc, lckArg);
    if (ret < 0) goto done;
    ret = checkLckDesc(&lckDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvEkbDesc, ikvEkbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ikvEkbDesc);
    if (ret < 0) goto done;
    if (reserved != 0) { ret = MGV_ERR_INVALID_ARG; goto done; }
    if ((ctx->verifiedMask & (1u << keyIdx)) == 0) { ret = MGV_ERR_NOT_VERIFIED; goto done; }

    u32 maxBody = (lckDesc.size < 0x52B1u) ? (lckDesc.size - 16u) : 21152u;
    u32 actualBody = measureLicenseLength((const u8 *)lckDesc.ptr + 16, maxBody);
    lckDesc.size = actualBody + 16u;

    ret = allocTempBuf(&tmpBuf, maxBody);
    if (ret < 0) goto done;
    memcpy(tmpBuf, lckDesc.ptr, lckDesc.size);
    lckDesc.ptr = tmpBuf;

    u8 keyringHdr[96];
    int keySlot = openLck(ctx, keyringHdr, NULL, (int)keyIdx, titleIdx, &lckDesc, (const u8 *)ikvDesc.ptr, &ekbDesc);
    if (keySlot >= 0) {
        sceMgr_driver_E6A203F8(keySlot);
        ret = 0;
        goto done;
    }
    ret = keySlot;
    if (keySlot == MGV_ERR_NEED_PLAY_START) {
        u32 krBytes = (u32)readBe16(keyringHdr + 12) * ((u32)readBe16(keyringHdr + 14) + 1u);
        u8 *rules = (u8 *)tmpBuf + 16 + krBytes;
        u32 rulesLen = actualBody - krBytes;
        int r = checkBindLicense(ctx->mediaType, rules, (int)rulesLen);
        if (r < 0) { ret = mapRuleError(r); goto done; }

        int newRulesLen = activatePlayPeriod(ctx->mediaType, rules, rulesLen, maxBody - krBytes);
        if (newRulesLen < 0) { ret = mapRuleError(newRulesLen); goto done; }
        if (newRulesLen == 0) { ret = 0; goto done; }

        u32 newTotal = krBytes + (u32)newRulesLen;
        r = checkPlayLicense(ctx->mediaType, NULL, rules, newRulesLen);
        if (r < 0) { ret = mapRuleError(r); goto done; }

        keySlot = deriveContentKeySlot(ctx, keyringHdr + 32, &ekbDesc);
        if (keySlot < 0) { ret = keySlot; goto done; }

        u8 mac[16];
        ret = sceMgr_driver_DA7D8C82(mac, (const u8 *)tmpBuf + 16, NULL, newTotal, keySlot);
        if (ret < 0) ret = mapMgrError(ret);
        if (ret >= 0) {
            ret = writeSingleIkvEntry(ctx, keyIdx, titleIdx, mac, (u8 *)ikvDesc.ptr, &ikvEkbDesc);
            if (ret >= 0) {
                u8 *out = (u8 *)lckArg->ptr;
                *(u32 *)out = 0x53434B4Cu; /* "LCKS" */
                *(u16 *)(out + 4) = 0;
                out[6] = 35; out[7] = 0;
                *(u32 *)(out + 8) = *(const u32 *)ekbDesc.ptr;
                *(u32 *)(out + 12) = 0;
                memcpy(out + 16, (const u8 *)tmpBuf + 16, newTotal);
                ret = (int)(newTotal + 16u);
            }
        }
        sceMgr_driver_E6A203F8(keySlot);
    }

done:
    if (tmpBuf != NULL)
        freeTempBuf(tmpBuf);
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_BD83E45F(const MgvBufDesc *msgArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc msgDesc;
    u8 msgHdr[64];

    int ret = findMediaCtx(32, &ctx, 1u);
    if (ret < 0)
        ret = findMediaCtx(16, &ctx, 1u);
    if (ret < 0) return ret;

    ret = copyBufDesc(&msgDesc, msgArg);
    if (ret < 0) return ret;
    ret = checkMinDesc(&msgDesc, 0x40u);
    if (ret < 0) return ret;

    memcpy(msgHdr, msgDesc.ptr, sizeof(msgHdr));
    ret = parseMgv1Header(260, msgHdr, (int)msgDesc.size);
    if (ret < 0) return ret;
    if (memcmp(msgHdr + 32, ctx->challengeNonce, 16) != 0)
        return MGV_ERR_MSG_SESSION;

    ret = verifyMgv1Mac(msgHdr, (const u8 *)msgDesc.ptr + 64, msgDesc.size - 64u);
    if (ret < 0) return ret;
    return applyClockResponse(ctx, msgHdr + 16);
}

int sceMgVideo_driver_DAA1AAA7(int devType, u32 keyIdx, int titleIdx,
                               const MgvBufDesc *msgArg, const MgvBufDesc *ekbArg,
                               const MgvBufDesc *ikvArg, const MgvBufDesc *ikvEkbArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc msgDesc, ekbDesc, ikvDesc, ikvEkbDesc;
    void *tmpBuf = NULL;
    int ret;

    ret = findMediaCtx(devType, &ctx, 3u);
    if (ret < 0) goto done;
    if (keyIdx >= 32u || (u32)(titleIdx - 1) >= 999u) { ret = MGV_ERR_INVALID_ARG; goto done; }

    if ((ctx->lastKeyIndex >= 0 && ctx->lastKeyIndex != (s16)keyIdx) ||
        (ctx->lastTitleIndex >= 0 && ctx->lastTitleIndex != (s16)titleIdx)) {
        ret = MGV_ERR_MSG_SESSION;
        goto done;
    }
    if ((ctx->verifiedMask & (1u << keyIdx)) == 0) { ret = MGV_ERR_NOT_VERIFIED; goto done; }

    ret = copyBufDesc(&msgDesc, msgArg);
    if (ret < 0) goto done;
    ret = checkMinDesc(&msgDesc, 0x90u);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvDesc, ikvArg);
    if (ret < 0) goto done;
    ret = checkIkvDesc(&ikvDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ekbDesc, ekbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ekbDesc);
    if (ret < 0) goto done;
    ret = copyBufDesc(&ikvEkbDesc, ikvEkbArg);
    if (ret < 0) goto done;
    ret = stripEkb2Header(&ikvEkbDesc);
    if (ret < 0) goto done;

    u32 allocLen = (msgDesc.size < 0x52B1u) ? msgDesc.size : 21168u;
    ret = allocTempBuf(&tmpBuf, allocLen);
    if (ret < 0) goto done;
    memcpy(tmpBuf, msgDesc.ptr, allocLen);

    int msgLen = parseMgv1Header(259, (const u8 *)tmpBuf, (int)msgDesc.size);
    if (msgLen < 0) { ret = msgLen; goto done; }
    if ((u32)msgLen >= 0x52B1u) { ret = MGV_ERR_MSG_FORMAT; goto done; }

    ret = verifyMgv1Mac((const u8 *)tmpBuf, (const u8 *)msgDesc.ptr + ((u32)msgLen & ~15u), msgDesc.size - (u32)msgLen);
    if (ret < 0) goto done;

    u8 *payload = (u8 *)tmpBuf + 32;
    int lckLen = parseBindPayload(payload, (u32)msgLen);
    if (lckLen < 0) { ret = lckLen; goto done; }

    u32 alignedLen = (u32)msgLen & ~15u;
    if (memcmp((const u8 *)tmpBuf + alignedLen - 32, ctx->challengeNonce, 16) != 0) {
        ret = MGV_ERR_MSG_SESSION;
        goto done;
    }
    ret = verifyMediaId(ctx, (const u32 *)((const u8 *)tmpBuf + alignedLen - 48));
    if (ret < 0) goto done;

    u32 krBytes = readBe16((const u8 *)tmpBuf + 40);
    int bindRet = checkBindLicense(ctx->mediaType, payload + 16 + krBytes, lckLen - (int)krBytes);
    if (bindRet < 0) {
        ret = mapRuleError(bindRet);
        if (ret == MGV_ERR_INVALID_DATA)
            ret = MGV_ERR_MSG_FORMAT;
        goto done;
    }

    int keySlot = rewrapBindKey(ctx, payload, lckLen, &ekbDesc);
    if (keySlot < 0) { ret = keySlot; goto done; }

    u8 mac[16];
    ret = sceMgr_driver_DA7D8C82(mac, (const u8 *)tmpBuf + 48, NULL, (u32)lckLen, keySlot);
    if (ret < 0) ret = mapMgrError(ret);
    if (ret >= 0) {
        sceMgr_driver_E6A203F8(keySlot);
        keySlot = -1;
        ret = writeSingleIkvEntry(ctx, keyIdx, titleIdx, mac, (u8 *)ikvDesc.ptr, &ikvEkbDesc);
        if (ret >= 0) {
            ctx->lastKeyIndex = (s16)keyIdx;
            ctx->lastTitleIndex = (s16)titleIdx;
            u8 *out = (u8 *)msgDesc.ptr;
            *(u32 *)out = 0x53434B4Cu; /* "LCKS" */
            *(u16 *)(out + 4) = 0;
            out[6] = 35; out[7] = 0;
            *(u32 *)(out + 8) = *(const u32 *)ekbDesc.ptr;
            *(u32 *)(out + 12) = 0;
            memcpy(out + 16, (const u8 *)tmpBuf + 48, (size_t)lckLen);
            ret = lckLen + 16;
        }
    }
    if (keySlot >= 0)
        sceMgr_driver_E6A203F8(keySlot);

done:
    if (tmpBuf != NULL)
        freeTempBuf(tmpBuf);
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_B8304DF9(int devType, u32 cmd, const MgvBufDesc *outArg) {
    MgvMediaCtx *ctx = NULL;
    MgvBufDesc outDesc;
    if (devType == 16 && cmd == 0x400u)
        devType = 32;

    int ret = findMediaCtx(devType, &ctx, 1u);
    if (ret >= 0) {
        if (ctx->state == 0) {
            ret = MGV_ERR_NOT_INITIALIZED;
        } else {
            ret = copyBufDesc(&outDesc, outArg);
            if (ret >= 0) {
                if (outDesc.ptr == NULL) {
                    ret = MGV_ERR_INVALID_ARG;
                } else if (cmd == 768u) {
                    ret = buildBindChallenge(ctx, &outDesc);
                } else if (cmd == 1024u) {
                    ret = buildClockChallenge(ctx, &outDesc);
                } else {
                    ret = MGV_ERR_INVALID_ARG;
                }
            }
        }
    }
    if ((u32)(ret - MGV_ERR_MGR_ERROR_21) < 2u && ctx != NULL)
        ctx->state = 1;
    return ret;
}

int sceMgVideo_driver_8678713F(void) {
    return MGV_ERR_UNSUPPORTED;
}
