#ifndef NP_MATCHING2_INTERNAL_H
#define NP_MATCHING2_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define NP_MATCHING2_NOINLINE __attribute__((noinline))

/* Error codes */
#define SCE_NP_ERROR_OUT_OF_MEMORY                    ((s32)0x80550001)
#define SCE_NP_ERROR_INVALID_ARGUMENT                 ((s32)0x80550003)
#define SCE_NP_ERROR_STACK_OVERFLOW                   ((s32)0x80550004)
#define SCE_NP_ERROR_INVALID_PARAM_COMMON             ((s32)0x80550101)

#define SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED    ((s32)0x80550c01)
#define SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED        ((s32)0x80550c02)
#define SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY          ((s32)0x80550c03)
#define SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT       ((s32)0x80550c04)
#define SCE_NP_MATCHING2_ERROR_CTX_MAX                ((s32)0x80550c05)
#define SCE_NP_MATCHING2_ERROR_CTX_ALREADY_EXISTS     ((s32)0x80550c06)
#define SCE_NP_MATCHING2_ERROR_CTX_NOT_FOUND          ((s32)0x80550c07)
#define SCE_NP_MATCHING2_ERROR_CTX_ALREADY_STARTED    ((s32)0x80550c08)
#define SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED        ((s32)0x80550c09)
#define SCE_NP_MATCHING2_ERROR_BUSY                   ((s32)0x80550c0e)
#define SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND      ((s32)0x80550c17)
#define SCE_NP_MATCHING2_ERROR_NOT_ROOM_OWNER         ((s32)0x80550c16)
#define SCE_NP_MATCHING2_ERROR_CTX_ERROR              ((s32)0x80550c1a)
#define SCE_NP_MATCHING2_ERROR_LISTENER_ALREADY_REG   ((s32)0x80550c1b)
#define SCE_NP_MATCHING2_ERROR_LISTENER_NOT_REG       ((s32)0x80550c1c)
#define SCE_NP_MATCHING2_ERROR_INVALID_LISTENER_ID    ((s32)0x80550c1d)

#define SCE_NP_MATCHING2_ERROR_PACKET_HEADER          ((s32)0x80550c81)
#define SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT       ((s32)0x80550c84)
#define SCE_NP_MATCHING2_ERROR_PACKET_VERSION         ((s32)0x80550c85)
#define SCE_NP_MATCHING2_ERROR_TLV_BUFFER_SMALL       ((s32)0x80550c8a)
#define SCE_NP_MATCHING2_ERROR_TLV_LENGTH             ((s32)0x80550c8b)
#define SCE_NP_MATCHING2_ERROR_TLV_MISSING_REQUIRED   ((s32)0x80550c8c)
#define SCE_NP_MATCHING2_ERROR_PACKET_BUFFER_SMALL    ((s32)0x80550c8d)

#define SCE_NP_MATCHING2_ERROR_CONN_MAX               ((s32)0x80550ca1)
#define SCE_NP_MATCHING2_ERROR_CONN_NOT_FOUND         ((s32)0x80550ca3)
#define SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID      ((s32)0x80550ca9)
#define SCE_NP_MATCHING2_ERROR_INVALID_CAST_TYPE      ((s32)0x80550cac)
#define SCE_NP_MATCHING2_ERROR_INVALID_ATTR_SIZE      ((s32)0x80550caf)
#define SCE_NP_MATCHING2_ERROR_INVALID_FLAG_TYPE      ((s32)0x80550cae)
#define SCE_NP_MATCHING2_ERROR_INVALID_SERVER_ID      ((s32)0x80550cba)
#define SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID        ((s32)0x80550cc2)
#define SCE_NP_MATCHING2_ERROR_INVALID_TARGET_LIST    ((s32)0x80550cc3)
#define SCE_NP_MATCHING2_ERROR_INVALID_BLOCK_FLAG     ((s32)0x80550cc4)
#define SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID        ((s32)0x80550cc5)
#define SCE_NP_MATCHING2_ERROR_INVALID_SORT_METHOD    ((s32)0x80550cc6)
#define SCE_NP_MATCHING2_ERROR_INVALID_MAX_SLOTS      ((s32)0x80550cc7)
#define SCE_NP_MATCHING2_ERROR_INVALID_WORLD_ID       ((s32)0x80550cc8)
#define SCE_NP_MATCHING2_ERROR_INVALID_RANGE_MAX      ((s32)0x80550cc9)
#define SCE_NP_MATCHING2_ERROR_SELF_TARGET            ((s32)0x80550cca)
#define SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN       ((s32)0x80550ccc)
#define SCE_NP_MATCHING2_ERROR_POOL_SIZE_TOO_SMALL    ((s32)0x80550ccb)
#define SCE_NP_MATCHING2_ERROR_INVALID_SERVER_SPEC    ((s32)0x80550cbf)
#define SCE_NP_MATCHING2_ERROR_DESTROYING             ((s32)0x80550ce1)
#define SCE_NP_MATCHING2_ERROR_LOCK_NOT_INIT          ((s32)0x80550ce2)

/* Byte-swap helpers matching Allegrex wsbh / rotr */
static inline u16 npSwap16(u16 value)
{
    return (u16)((value >> 8) | (value << 8));
}

static inline u32 npSwap32(u32 value)
{
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0xFF000000u) >> 24);
}

static inline u64 npSwap64(u64 value)
{
    u32 lowWord = (u32)(value & 0xFFFFFFFFu);
    u32 highWord = (u32)(value >> 32);
    return ((u64)npSwap32(lowWord) << 32) | (u64)npSwap32(highWord);
}

/* Kernel / Network / SSL / NP imports (matching module.yml exact names) */
typedef struct SceKernelVplInfo {
    u32 size;
    char name[32];
    u32 attr;
    s32 poolSize;
    s32 freeSize;
    s32 numWaitThreads;
} SceKernelVplInfo;

typedef struct SceLwMutexWorkarea {
    s32 raw[8];
} SceLwMutexWorkarea;

typedef struct SceNetInetSockaddrIn {
    u8 sinLen;
    u8 sinFamily;
    u16 sinPort;
    u32 sinAddr;
    u16 sinVport;
    u8 sinZero[6];
} SceNetInetSockaddrIn;

typedef struct SceNetInetTimeval {
    s32 tvSec;
    s32 tvUsec;
} SceNetInetTimeval;

typedef struct SceNetInetFdSet {
    u32 fdsBits[8];
} SceNetInetFdSet;

/* Kernel_Library */
extern s32 sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(s32 intrState);
extern void *sceKernelMemset(void *dst, s32 fillByte, u32 size);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);
extern s32 sceKernelLockLwMutex(SceLwMutexWorkarea *workarea, s32 count, u32 *timeout);
extern s32 sceKernelUnlockLwMutex(SceLwMutexWorkarea *workarea, s32 count);
extern s32 _sceKernelCheckThreadStack(void);
extern s32 sceKernelGetThreadId(void);

/* SysMemUserForUser */
extern s32 sceKernelQueryMemoryInfo(const void *addr, s32 *partIdOut, s32 *blockIdOut);
extern u32 sceKernelGetCompiledSdkVersion(void);

/* ThreadManForUser */
extern s32 sceKernelDelayThread(u32 usec);
extern s32 sceKernelCreateSema(const char *name, u32 attr, s32 initVal, s32 maxVal, void *opt);
extern s32 sceKernelDeleteSema(s32 semaId);
extern s32 sceKernelSignalSema(s32 semaId, s32 signalCount);
extern s32 sceKernelWaitSema(s32 semaId, s32 signalCount, u32 *timeout);
extern s32 sceKernelCreateEventFlag(const char *name, u32 attr, u32 initPattern, void *opt);
extern s32 sceKernelDeleteEventFlag(s32 efId);
extern s32 sceKernelSetEventFlag(s32 efId, u32 bits);
extern s32 sceKernelWaitEventFlag(s32 efId, u32 bits, u32 waitMode, u32 *outBits, u32 *timeout);
extern s32 sceKernelCreateMbx(const char *name, u32 attr, void *opt);
extern s32 sceKernelDeleteMbx(s32 mbxId);
extern s32 sceKernelSendMbx(s32 mbxId, void *msg);
extern s32 sceKernelPollMbx(s32 mbxId, void **msgOut);
extern s32 sceKernelCreateMsgPipe(const char *name, s32 partId, u32 attr, void *bufSize, void *opt);
extern s32 sceKernelDeleteMsgPipe(s32 mppId);
extern s32 sceKernelSendMsgPipe(s32 mppId, const void *msg, u32 size, s32 waitMode, u32 *result, u32 *timeout);
extern s32 sceKernelReceiveMsgPipe(s32 mppId, void *buf, u32 size, s32 waitMode, u32 *result, u32 *timeout);
extern s32 sceKernelCreateThread(const char *name, s32 (*entry)(u32, void *), s32 priority, s32 stackSize, u32 attr, void *opt);
extern s32 sceKernelStartThread(s32 thid, u32 arglen, void *argp);
extern s32 sceKernelWaitThreadEnd(s32 thid, u32 *timeout);
extern s32 sceKernelTerminateThread(s32 thid);
extern s32 sceKernelDeleteThread(s32 thid);
extern u32 sceKernelGetSystemTimeLow(void);
extern u64 sceKernelGetSystemTimeWide(void);
extern s32 sceKernelCreateVpl(const char *name, s32 partId, u32 attr, u32 size, void *opt);
extern s32 sceKernelDeleteVpl(s32 vplId);
extern s32 sceKernelTryAllocateVpl(s32 vplId, u32 size, void **outPtr);
extern s32 sceKernelFreeVpl(s32 vplId, void *ptr);
extern s32 sceKernelReferVplStatus(s32 vplId, SceKernelVplInfo *info);
extern s32 sceKernelCreateLwMutex(SceLwMutexWorkarea *workarea, const char *name, u32 attr, s32 initCount, void *opt);
extern s32 sceKernelDeleteLwMutex(SceLwMutexWorkarea *workarea);
extern s32 sceKernelCreateMutex(const char *name, u32 attr, s32 initCount, void *opt);
extern s32 sceKernelDeleteMutex(s32 mutexId);
extern s32 sceKernelLockMutex(s32 mutexId, s32 count, u32 *timeout);
extern s32 sceKernelUnlockMutex(s32 mutexId, s32 count);

/* sceOpenPSID, sceCertLoader, sceNpCore, sceNp, sceNpService */
extern s32 sceOpenPSIDGetOpenPSID(void *psidOut);
extern s32 sceCertLoader_DD629A24(s32 certType, s32 certFlags, void *certListOut, void *buf, u32 bufSize, u32 *sizeOut);
extern s32 sceNpCore_52440ABF(char *clientVerOut);
extern s32 sceNpGetNpId(void *npIdOut);
extern s32 sceNpGetUserProfile(void *profileOut);
extern s32 sceNp_2D8E93D2(const void *selfNpId, const void *peerNpId, s32 *cmpOut);
extern s32 sceNp_4B09907A(char *dst, const char *src, u32 maxLen);
extern s32 sceNp_6CBB0614(void *tickOut);
extern s32 sceNp_9A264EF2(const void *lhsNpId, const void *rhsNpId);
extern s32 sceNp_9B87B19B(const void **ticketPtrOut);
extern s32 sceNp_E90DFBC4(const void *commId, char *strOut, u32 strBufSize);
extern s32 sceNpService_76867C01(s32 handle, void **listOut);
extern s32 sceNpService_7EF4312E(const void *userProfile, s32 serviceId, const void *commId);
extern s32 sceNpService_DD8B5E53(s32 handle);

/* sceNet, sceNetApctl, sceNet_lib, sceNetInet, sceNetInet_lib, sceNetResolver, sceNetStun, sceNetUpnp */
extern s32 sceNetFreeThreadinfo(s32 thid);
extern s32 sceNetThreadAbort(s32 thid);
extern s32 sceNetApctlGetInfo(s32 code, void *infoOut);
extern u32 sceNet_lib_1858883D(void);
extern s32 sceNet_lib_384EFE14(const void *msg, u32 msgLen, const void *key, u32 keyLen, void *digestOut);
extern s32 sceNet_lib_A0F16ABD(const char *lhs, const char *rhs);
extern s32 sceNet_lib_A8B6205A(void *timerObj, u32 timeoutUsec, void *cbFunc, u32 cbArg);
extern s32 sceNet_lib_A93A93E9(void *timerObj);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, u32 maxLen);
extern s32 sceNet_lib_B9085A96(const char *lhs, const char *rhs, u32 maxLen);
extern char *sceNet_lib_BCBE14CF(const char *str, s32 ch);
extern u32 sceNet_lib_D8722983(const char *str);
extern s32 sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, u32 size);
extern s32 sceNetInetSocket(s32 domain, s32 sockType, s32 protocol);
extern s32 sceNetInetClose(s32 sockFd);
extern s32 sceNetInetBind(s32 sockFd, const void *addr, u32 addrLen);
extern s32 sceNetInetConnect(s32 sockFd, const void *addr, u32 addrLen);
extern s32 sceNetInetGetsockname(s32 sockFd, void *addr, u32 *addrLen);
extern s32 sceNetInetSetsockopt(s32 sockFd, s32 level, s32 optName, const void *optVal, u32 optLen);
extern s32 sceNetInetGetsockopt(s32 sockFd, s32 level, s32 optName, void *optVal, u32 *optLen);
extern s32 sceNetInetSend(s32 sockFd, const void *buf, u32 len, s32 flags);
extern s32 sceNetInetRecv(s32 sockFd, void *buf, u32 len, s32 flags);
extern s32 sceNetInetSendto(s32 sockFd, const void *buf, u32 len, s32 flags, const void *to, u32 toLen);
extern s32 sceNetInetRecvfrom(s32 sockFd, void *buf, u32 len, s32 flags, void *from, u32 *fromLen);
extern s32 sceNetInetSelect(s32 nfds, void *readFds, void *writeFds, void *exceptFds, void *timeout);
extern s32 sceNetInetGetPspError(void);
extern u32 sceNetInetInetAddr(const char *cp);
extern s32 sceNetInetInetPton(s32 af, const char *src, void *dst);
extern s32 sceNetInet_lib_0238B6DF(void *aesCtx, s32 blockBits, s32 keyBits, const void *keyBytes);
extern s32 sceNetInet_lib_AEE60F84(s32 sockFd, s32 ioctlCmd, void *ioctlArg);
extern s32 sceNetInet_lib_D609AD36(void *aesCtx, const void *srcBlock, void *dstBlock);
extern s32 sceNetResolverCreate(s32 *ridOut, void *buf, u32 bufLen);
extern s32 sceNetResolverDelete(s32 rid);
extern s32 sceNetResolverStartNtoAAsync(s32 rid, const char *hostname, void *addrOut, s32 timeoutSec, s32 retryCount);
extern s32 sceNetResolverPollAsync(s32 rid, s32 *resultOut);
extern s32 sceNetResolverStop(s32 rid);
extern s32 sceNetUpnp_1038E77A(void *outInfo);
extern s32 sceNetStun_4403D9E7(s32 handle);
extern s32 sceNetStun_6CA54FC9(s32 handle, s32 sockFd, u32 userTag);
extern s32 sceNetStun_EBF3C24D(u32 stackSize, s32 priority, const char *host, u16 port, const void *authParam, void *cbFunc, s32 *handleOut);

/* sceSsl_lib */
extern void *sceSsl_lib_0861D934(u32 size);
extern s32 sceSsl_lib_0FF7BA68(void *cert, void *pubKey);
extern void *sceSsl_lib_12C7718F(void *cert);
extern void *sceSsl_lib_28B4DE33(s32 sockFd, s32 closeFlag);
extern const char *sceSsl_lib_2B7BD166(void *oid, s32 oidLen, s32 unusedFlag);
extern void *sceSsl_lib_2E2E2B09(void *sslHandle);
extern s32 sceSsl_lib_37C7B76C(void *cert, void *nowTime, s32 *flagOut, s32 *errOut);
extern s32 sceSsl_lib_3E3133D6(void *x509Name, s32 nid, char *buf, s32 len);
extern void *sceSsl_lib_4BE9030A(void *cert);
extern s32 sceSsl_lib_4C75B8E9(void *x509Name);
extern void *sceSsl_lib_5040E836(void *x509Name, s32 idx);
extern void sceSsl_lib_529A9477(void *ctx, void *verifyCb, void *arg);
extern s32 sceSsl_lib_54A7D8F3(void *sslHandle);
extern void sceSsl_lib_588F2FE8(void *ctx);
extern void sceSsl_lib_5E5C873A(void *ptr);
extern void *sceSsl_lib_6038DEDE(void **outCert, const u8 **pp, s32 len);
extern s32 sceSsl_lib_69F1B678(void *x509Store, void *cert);
extern s32 sceSsl_lib_7D0E41F4(void *lhsName, void *rhsName);
extern void sceSsl_lib_80608663(void *sslHandle);
extern void sceSsl_lib_84833472(void *sslHandle);
extern s32 sceSsl_lib_86D0771E(void *sslHandle);
extern s32 sceSsl_lib_8D484361(void *entry, s32 idx, void *oidOut, void *oidLenOut, void *typeOut, void *valOut, s32 *valLenOut);
extern s32 sceSsl_lib_9299C6CE(void *cert, void *typeOut, void **bufOut, s32 *lenOut);
extern void sceSsl_lib_AEBF278B(void *ctx, s32 mode, void *cb);
extern s32 sceSsl_lib_B3B04C58(void *sslHandle, s32 ret);
extern void *sceSsl_lib_B40D11EA(void);
extern s32 sceSsl_lib_B4D78E98(void *ctx, s32 cmd, u32 val, void *ptr);
extern s32 sceSsl_lib_B7CA8717(void *sslHandle, const void *buf, s32 len);
extern void *sceSsl_lib_B87FFE37(void *cert);
extern void sceSsl_lib_B9C8CCE6(void *sslHandle, void *rbio, void *wbio);
extern s32 sceSsl_lib_BDAD0841(void *x509StoreCtx);
extern void sceSsl_lib_BF55C31C(void *ctx, s32 mode);
extern void sceSsl_lib_C1A2E14B(void *cert);
extern s32 sceSsl_lib_E7C29542(void *sslHandle, void *buf, s32 len);
extern void *sceSsl_lib_EBFB0E3C(void *ctx);
extern void *sceSsl_lib_EC4A18D7(void *bio);
extern void sceSsl_lib_ECE07B61(void *bio, void *appData);
extern s32 sceSsl_lib_EF5A4BD4(void *sslHandle);
extern void *sceSsl_lib_FB8273FE(void *method);

/* Public API types */
typedef u16 SceNpMatching2ContextId;
typedef u16 SceNpMatching2ServerId;
typedef u32 SceNpMatching2WorldId;
typedef u64 SceNpMatching2RoomId;
typedef u16 SceNpMatching2RoomMemberId;
typedef u16 SceNpMatching2AttributeId;
typedef u32 SceNpMatching2RequestId;
typedef u16 SceNpMatching2Event;
typedef u32 SceNpMatching2FlagAttr;

typedef struct SceNpMatching2MemoryStat {
    u32 totalSize;
    u32 maxUsedSize;
    u32 currentFreeSize;
} SceNpMatching2MemoryStat;

typedef struct SceNpId {
    char handle[16];
    u8 term;
    u8 reservedLead[3];
    u8 opt[8];
    u8 reserved[8];
} SceNpId;

typedef struct SceNpCommId {
    char data[9];
    char term;
    u8 num;
    u8 reserved;
} SceNpCommId;

typedef struct SceNpCommPassphrase {
    u8 data[128];
} SceNpCommPassphrase;

typedef struct SceNpMatching2IntAttr {
    SceNpMatching2AttributeId id;
    u16 padding;
    u32 num;
} SceNpMatching2IntAttr;

typedef struct SceNpMatching2BinAttr {
    SceNpMatching2AttributeId id;
    u16 padding;
    void *ptr;
    u32 size;
} SceNpMatching2BinAttr;

typedef struct SceNpMatching2BinAttrInternal {
    u64 updateDate;
    u16 updateMemberId;
    u16 padding;
    SceNpMatching2BinAttr attr;
} SceNpMatching2BinAttrInternal;

typedef struct SceNpMatching2MemberBinAttrInternal {
    u64 updateDate;
    SceNpMatching2BinAttr attr;
    u32 trailingPad;
} SceNpMatching2MemberBinAttrInternal;

typedef struct SceNpMatching2RoomGroup {
    u8 groupId;
    u8 groupPad[3];
    u32 slotNum;
    u32 curGroupMemberNum;
    s32 withLabel;
    u8 label[8];
    s32 withPassword;
} SceNpMatching2RoomGroup;

typedef struct SceNpMatching2SessionPassword {
    u8 data[8];
} SceNpMatching2SessionPassword;

typedef struct SceNpMatching2JoinedSessionInfo {
    u8 sessionType;
    u8 padding;
    u16 serverId;
    u32 worldId;
    u64 lobbyId;
    u64 roomId;
    u64 joinDate;
} SceNpMatching2JoinedSessionInfo;

typedef struct SceNpMatching2RequestOptParam {
    void (*cbFunc)(SceNpMatching2ContextId, SceNpMatching2RequestId, SceNpMatching2Event, s32, const void *, void *);
    void *cbFuncArg;
    u32 timeout;
    u16 appReqId;
    u16 padding;
} SceNpMatching2RequestOptParam;

typedef struct SceNpMatching2RoomEventOptParam {
    void (*cbFunc)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2Event, const void *, void *);
    void *cbFuncArg;
    u32 optFlags[4];
} SceNpMatching2RoomEventOptParam;

typedef struct SceNpMatching2RoomMessageOptParam {
    void (*cbFunc)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2RoomMemberId, SceNpMatching2Event, const void *, void *);
    void *cbFuncArg;
} SceNpMatching2RoomMessageOptParam;

typedef struct SceNpMatching2SignalingOptParam {
    u8 type;
    u8 flag;
    u16 hubMemberId;
    u8 reserved[4];
} SceNpMatching2SignalingOptParam;

typedef struct NpMatching2AttrLimits {
    u32 maxRoomBinAttrInternal;
    u32 maxRoomSearchableInt;
    u32 maxRoomSearchableBin;
    u32 maxRoomBinAttrExternal;
    u32 maxMemberBinAttrInternal;
    u32 maxRoomGroup;
    u32 maxRoomMemberDataBin;
    u32 maxUserBinAttr;
} NpMatching2AttrLimits;

typedef struct NpMatching2ListenerSlot {
    s32 inUse;
    u32 slotId;
    void *callbacks[10];
} NpMatching2ListenerSlot;

typedef struct NpMatching2ServerNode NpMatching2ServerNode;
typedef struct NpMatching2Conn NpMatching2Conn;
typedef struct NpMatching2Room NpMatching2Room;
typedef struct NpMatching2Req NpMatching2Req;

typedef struct NpMatching2Context {
    struct NpMatching2Context *next;
    SceNpMatching2ContextId ctxId;
    SceNpCommId commId;
    SceNpCommPassphrase passPhrase;
    u16 alignPad;
    s32 refCount;
    s32 destroying;
    u32 option;
    u32 stateFlags;
    SceNpMatching2RequestOptParam defaultReqOpt;
    SceNpMatching2RoomEventOptParam defaultRoomEvOpt;
    SceNpMatching2RoomMessageOptParam defaultRoomMsgOpt;
    u32 serverCount;
    NpMatching2ServerNode *serverList;
    void (*sigCb)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2RoomMemberId, SceNpMatching2Event, s32, void *);
    void *sigCbArg;
    NpMatching2ListenerSlot listeners[3];
} NpMatching2Context;

typedef struct NpMatching2ReqQueue {
    NpMatching2Req *head;
    NpMatching2Req *tail;
} NpMatching2ReqQueue;

typedef struct NpMatching2HostEntry {
    char host[130];
    u16 port;
} NpMatching2HostEntry;

typedef struct NpMatching2HostList {
    u32 count;
    NpMatching2HostEntry *entries;
} NpMatching2HostList;

typedef struct NpMatching2SslCtx {
    s32 sockFd;
    void *sslHandle;
    void *sslBio;
    s32 lastError;
    const char *hostName;
} NpMatching2SslCtx;

struct NpMatching2Conn {
    NpMatching2ServerNode *server;
    u32 connId;
    u32 connKind;
    u32 connState;
    s32 refCount;
    s32 closing;
    NpMatching2HostList hostList;
    u32 curHostIndex;
    s32 sockFd;
    SceNetInetSockaddrIn peerAddr;
    NpMatching2SslCtx sslCtx;
    s32 resolverId;
    void *resolverBuf;
    NpMatching2ReqQueue sendQueue;
    NpMatching2ReqQueue waitQueue;
    u8 ownerKind;
    u8 ownerPad[3];
    NpMatching2Room *ownerRoom;
    u32 lastSendTime;
    u32 lastRecvTime;
};

typedef struct NpMatching2BlobBuf {
    u32 len;
    u8 bytes[512];
} NpMatching2BlobBuf;

struct NpMatching2ServerNode {
    NpMatching2Context *ctx;
    NpMatching2ServerNode *next;
    SceNpMatching2ServerId serverId;
    u16 worldCount;
    u8 reservedMeta[12];
    u8 status;
    u8 statusPad[3];
    u32 sessionId;
    NpMatching2BlobBuf sessionToken;
    NpMatching2BlobBuf sessionKey;
    NpMatching2BlobBuf keepaliveCookie;
    u32 mainConnId;
    u32 authConnId;
    s32 refCount;
    s32 closing;
    NpMatching2Conn *conns[15];
    NpMatching2Room *roomList;
    NpMatching2ReqQueue pendingQueue;
    NpMatching2ReqQueue authWaitQueue;
    s32 authInProgress;
    u32 ownedRoomCount;
    u8 ownedWorldCounts[64];
    u32 lastOwnedLeaveTime;
    u32 keepaliveIntervalUsec;
    u32 keepalivePad;
    u64 nextKeepaliveTime;
    s32 keepaliveInFlight;
    u32 tailPad;
};

typedef struct NpMatching2RoomMember {
    struct NpMatching2RoomMember *next;
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
    u64 joinDate;
    SceNpMatching2RoomMemberId memberId;
    u8 teamId;
    u8 padByte;
    SceNpMatching2RoomGroup *roomGroup;
    u8 natType;
    u8 natPad[3];
    u32 flagAttr;
    SceNpMatching2MemberBinAttrInternal *binAttrInternal;
    u32 binAttrInternalCount;
} NpMatching2RoomMember;

struct NpMatching2Room {
    SceNpMatching2ServerId serverId;
    u16 serverPad;
    SceNpMatching2WorldId worldId;
    u64 lobbyId;
    SceNpMatching2RoomId roomId;
    u64 passwordSlotMask;
    u32 maxSlots;
    NpMatching2RoomMember *memberList;
    u32 memberCount;
    NpMatching2RoomMember *meMember;
    NpMatching2RoomMember *ownerMember;
    SceNpMatching2RoomGroup *groupList;
    u32 groupCount;
    u32 flagAttr;
    SceNpMatching2BinAttrInternal *binAttrInternal;
    u32 binAttrInternalCount;
    NpMatching2ServerNode *server;
    NpMatching2Conn *conn;
    NpMatching2Room *next;
    s32 refCount;
    s32 destroying;
    u32 sigOptLen;
    SceNpMatching2SignalingOptParam sigOptParam;
    s32 hasPassword;
    SceNpMatching2SessionPassword password;
    u16 roomHandle;
    u16 handlePad;
    NpMatching2BlobBuf roomKey;
    NpMatching2BlobBuf roomCookie;
    SceNpMatching2RoomEventOptParam roomEvOpt;
    SceNpMatching2RoomMessageOptParam roomMsgOpt;
    u32 tailReserved[2];
};

struct NpMatching2Req {
    NpMatching2Req *next;
    s32 reqSource;
    u8 reqKind;
    u8 reqPad[3];
    SceNpMatching2RequestId reqId;
    s32 refCount;
    u32 txId;
    SceNpMatching2Event eventId;
    SceNpMatching2ContextId ctxId;
    u32 connId;
    u16 protoCmd;
    u16 nextProtoCmd;
    u32 nextConnId;
    void *paramCopy;
    s32 isMaintenance;
    s32 hasOptParam;
    SceNpMatching2RequestOptParam optParam;
    u32 timeoutUsec;
    u32 startTimeLow;
    s32 resultCode;
    s32 completed;
    s32 aborted;
    s32 inFlight;
};

/* Public request/response parameter structs (exact PSP SDK layouts) */
typedef struct SceNpMatching2GetServerInfoRequest {
    SceNpMatching2ServerId serverId;
} SceNpMatching2GetServerInfoRequest;

typedef struct SceNpMatching2GetWorldInfoListRequest {
    SceNpMatching2ServerId serverId;
} SceNpMatching2GetWorldInfoListRequest;

typedef struct SceNpMatching2SetUserInfoRequest {
    SceNpMatching2ServerId serverId;
    u16 padding;
    SceNpMatching2BinAttr *userBinAttr;
    u32 userBinAttrNum;
} SceNpMatching2SetUserInfoRequest;

typedef struct SceNpMatching2GetUserInfoListRequest {
    SceNpMatching2ServerId serverId;
    u16 padding;
    const SceNpId *npId;
    u32 npIdNum;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
    s32 option;
} SceNpMatching2GetUserInfoListRequest;

typedef struct SceNpMatching2GetRoomMemberDataExternalListRequest {
    SceNpMatching2RoomId roomId;
} SceNpMatching2GetRoomMemberDataExternalListRequest;

typedef struct SceNpMatching2SetRoomDataExternalRequest {
    SceNpMatching2RoomId roomId;
    const SceNpMatching2IntAttr *roomSearchableIntAttrExternal;
    u32 roomSearchableIntAttrExternalNum;
    const SceNpMatching2BinAttr *roomSearchableBinAttrExternal;
    u32 roomSearchableBinAttrExternalNum;
    const SceNpMatching2BinAttr *roomBinAttrExternal;
    u32 roomBinAttrExternalNum;
} SceNpMatching2SetRoomDataExternalRequest;

typedef struct SceNpMatching2GetRoomDataExternalListRequest {
    const SceNpMatching2RoomId *roomId;
    u32 roomIdNum;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
} SceNpMatching2GetRoomDataExternalListRequest;

typedef struct SceNpMatching2SetSignalingOptParamRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2SignalingOptParam sigOptParam;
} SceNpMatching2SetSignalingOptParamRequest;

typedef struct SceNpMatching2CreateJoinRoomRequest {
    SceNpMatching2WorldId worldId;
    u32 worldPad;
    u64 lobbyId;
    u32 maxSlot;
    SceNpMatching2FlagAttr flagAttr;
    const SceNpMatching2BinAttr *roomBinAttrInternal;
    u32 roomBinAttrInternalNum;
    const SceNpMatching2IntAttr *roomSearchableIntAttrExternal;
    u32 roomSearchableIntAttrExternalNum;
    const SceNpMatching2BinAttr *roomSearchableBinAttrExternal;
    u32 roomSearchableBinAttrExternalNum;
    const SceNpMatching2BinAttr *roomBinAttrExternal;
    u32 roomBinAttrExternalNum;
    const SceNpMatching2SessionPassword *roomPassword;
    const void *groupConfig;
    u32 groupConfigNum;
    const u64 *passwordSlotMask;
    const SceNpId *allowedUser;
    u32 allowedUserNum;
    const SceNpId *blockedUser;
    u32 blockedUserNum;
    const void *joinRoomGroupLabel;
    const SceNpMatching2BinAttr *roomMemberBinAttrInternal;
    u32 roomMemberBinAttrInternalNum;
    u8 teamId;
    u8 padBytes[3];
    const SceNpMatching2SignalingOptParam *sigOptParam;
    u32 tailPad;
} SceNpMatching2CreateJoinRoomRequest;

typedef struct SceNpMatching2JoinRoomRequest {
    SceNpMatching2RoomId roomId;
    const SceNpMatching2SessionPassword *roomPassword;
    const void *joinRoomGroupLabel;
    const SceNpMatching2BinAttr *roomMemberBinAttrInternal;
    u32 roomMemberBinAttrInternalNum;
    u8 optData[20];
    u8 teamId;
    u8 padBytes[3];
} SceNpMatching2JoinRoomRequest;

typedef struct SceNpMatching2LeaveRoomRequest {
    SceNpMatching2RoomId roomId;
    u8 optData[20];
} SceNpMatching2LeaveRoomRequest;

typedef struct SceNpMatching2GrantRoomOwnerRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId newOwner;
    u16 padding;
    u8 optData[20];
} SceNpMatching2GrantRoomOwnerRequest;

typedef struct SceNpMatching2KickoutRoomMemberRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId target;
    u8 blockFlag;
    u8 padding;
    u8 optData[20];
} SceNpMatching2KickoutRoomMemberRequest;

typedef struct SceNpMatching2SearchRoomRequest {
    s32 option;
    SceNpMatching2WorldId worldId;
    u64 lobbyId;
    u32 startIndex;
    u32 maxNum;
    SceNpMatching2FlagAttr flagFilter;
    SceNpMatching2FlagAttr flagAttr;
    const void *intFilter;
    u32 intFilterNum;
    const void *binFilter;
    u32 binFilterNum;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
} SceNpMatching2SearchRoomRequest;

typedef struct SceNpMatching2SendRoomChatMessageRequest {
    SceNpMatching2RoomId roomId;
    u8 castType;
    u8 padBytes[3];
    union {
        SceNpMatching2RoomMemberId unicastTarget;
        struct {
            const SceNpMatching2RoomMemberId *memberId;
            u32 memberIdNum;
        } multicastTarget;
        u8 multicastTargetTeamId;
    } target;
    const void *msg;
    u32 msgLen;
    u32 option;
} SceNpMatching2SendRoomChatMessageRequest;

typedef struct SceNpMatching2SendRoomMessageRequest {
    SceNpMatching2RoomId roomId;
    u8 castType;
    u8 padBytes[3];
    union {
        SceNpMatching2RoomMemberId unicastTarget;
        struct {
            const SceNpMatching2RoomMemberId *memberId;
            u32 memberIdNum;
        } multicastTarget;
        u8 multicastTargetTeamId;
    } target;
    const void *msg;
    u32 msgLen;
    u32 option;
} SceNpMatching2SendRoomMessageRequest;

typedef struct SceNpMatching2SetRoomDataInternalRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2FlagAttr flagFilter;
    SceNpMatching2FlagAttr flagAttr;
    const SceNpMatching2BinAttr *roomBinAttrInternal;
    u32 roomBinAttrInternalNum;
    const void *passwordConfig;
    u32 passwordConfigNum;
    const u64 *passwordSlotMask;
    const u16 *ownerPrivilegeRank;
    u32 ownerPrivilegeRankNum;
    u32 tailPad;
} SceNpMatching2SetRoomDataInternalRequest;

typedef struct SceNpMatching2GetRoomDataInternalRequest {
    SceNpMatching2RoomId roomId;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
} SceNpMatching2GetRoomDataInternalRequest;

typedef struct SceNpMatching2SetRoomMemberDataInternalRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId memberId;
    u8 teamId;
    u8 padByte;
    u32 memberPad;
    SceNpMatching2FlagAttr flagFilter;
    SceNpMatching2FlagAttr flagAttr;
    const SceNpMatching2BinAttr *roomMemberBinAttrInternal;
    u32 roomMemberBinAttrInternalNum;
} SceNpMatching2SetRoomMemberDataInternalRequest;

typedef struct SceNpMatching2GetRoomMemberDataInternalRequest {
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId memberId;
    u16 padWord;
    u32 reservedWord;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
} SceNpMatching2GetRoomMemberDataInternalRequest;

typedef struct SceNpMatching2GetRoomMemberDataInternalListRequest {
    SceNpMatching2RoomId roomId;
    const SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
    u32 sortMethod;
} SceNpMatching2GetRoomMemberDataInternalListRequest;

/* Shared globals */
extern s32 g_npMatching2VplId;
extern s32 g_npMatching2VplMaxUsed;
extern s32 g_npMatching2Initialized;
extern NpMatching2AttrLimits g_npMatching2AttrLimits;

/* matching2_api.c */
s32 npMatching2Base64Decode(char *outBuf, const u8 *inStr, s32 inLen);
s32 npMatching2VplCreate(s32 partId, u32 poolSize, const char *name);
s32 npMatching2VplDelete(void);
s32 npMatching2VplGetStat(SceNpMatching2MemoryStat *statOut);
void *npMatching2Alloc(u32 size);
s32 npMatching2Free(void *ptr);
s32 npMatching2RegisterInternalListener(u32 listenerId, SceNpMatching2ContextId ctxId, const void *callbacks);
s32 npMatching2UnregisterInternalListener(u32 listenerId, SceNpMatching2ContextId ctxId);
s32 npMatching2SendRoomMessageInternal(SceNpMatching2ContextId ctxId, const SceNpMatching2SendRoomMessageRequest *reqParam,
                                       const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId, s32 reqSource);

/* matching2_context.c — validators */
s32 npValidateCreateJoinRoomReq(const SceNpMatching2CreateJoinRoomRequest *req);
s32 npValidateGetServerInfoReq(const SceNpMatching2GetServerInfoRequest *req);
s32 npValidateGetWorldInfoListReq(const SceNpMatching2GetWorldInfoListRequest *req);
s32 npValidateSetUserInfoReq(const SceNpMatching2SetUserInfoRequest *req);
s32 npValidateGetUserInfoListReq(const SceNpMatching2GetUserInfoListRequest *req);
s32 npValidateSearchRoomReq(const SceNpMatching2SearchRoomRequest *req);
s32 npValidateJoinRoomReq(const SceNpMatching2JoinRoomRequest *req);
s32 npValidateLeaveRoomReq(const SceNpMatching2LeaveRoomRequest *req);
s32 npValidateGetRoomDataInternalReq(const SceNpMatching2GetRoomDataInternalRequest *req);
s32 npValidateSetRoomDataInternalReq(const SceNpMatching2SetRoomDataInternalRequest *req);
s32 npValidateSetSignalingOptParamReq(const SceNpMatching2SetSignalingOptParamRequest *req);
s32 npValidateGetRoomDataExternalListReq(const SceNpMatching2GetRoomDataExternalListRequest *req);
s32 npValidateSetRoomDataExternalReq(const SceNpMatching2SetRoomDataExternalRequest *req);
s32 npValidateGetRoomMemberDataInternalReq(const SceNpMatching2GetRoomMemberDataInternalRequest *req);
s32 npValidateSetRoomMemberDataInternalReq(const SceNpMatching2SetRoomMemberDataInternalRequest *req);
s32 npValidateGetRoomMemberDataInternalListReq(const SceNpMatching2GetRoomMemberDataInternalListRequest *req);
s32 npValidateRoomIdAndSortMethod(SceNpMatching2RoomId roomId, u32 sortMethod);
s32 npValidateGetRoomMemberDataInternalLocal(SceNpMatching2RoomId roomId, SceNpMatching2RoomMemberId memberId,
                                             const SceNpMatching2AttributeId *attrId, s32 attrIdNum);
s32 npValidateGetRoomMemberDataExternalListReq(const SceNpMatching2GetRoomMemberDataExternalListRequest *req);
s32 npValidateKickoutRoomMemberReq(const SceNpMatching2KickoutRoomMemberRequest *req);
s32 npValidateGrantRoomOwnerReq(const SceNpMatching2GrantRoomOwnerRequest *req);
s32 npValidateSendRoomChatMessageReq(const SceNpMatching2SendRoomChatMessageRequest *req);
s32 npValidateSendRoomMessageReq(const SceNpMatching2SendRoomMessageRequest *req);
s32 npValidateGetSignalingOptParamLocal(SceNpMatching2RoomId roomId, const void *outParam);

/* matching2_context.c — context, conn, core API handlers, lock */
s32 npCtxUnlinkAndFree(NpMatching2Context *ctx);
s32 npCtxTableInit(void);
void npCtxTableTerm(void);
s32 npCtxCheckCanCreate(const SceNpCommId *commId);
s32 npCtxCreate(const SceNpCommId *commId, const SceNpCommPassphrase *passPhrase, u32 option, s32 legacyMode, NpMatching2Context **outCtx);
s32 npCtxFindAndRef(SceNpMatching2ContextId ctxId, NpMatching2Context **outCtx);
s32 npCtxTryCleanup(NpMatching2Context *ctx);
s32 npCtxRelease(NpMatching2Context *ctx);
s32 npCtxMarkDestroy(NpMatching2Context *ctx);
NpMatching2Context *npCtxGetFirstRef(void);
NpMatching2Context *npCtxGetNextRef(NpMatching2Context *ctx);
s32 npCtxHasAnyStarted(void);
s32 npCtxCheckStarted(const NpMatching2Context *ctx, s32 requireStarted);
s32 npConnFindAndRef(SceNpMatching2ContextId ctxId, u32 connId, NpMatching2Conn **outConn);
s32 npConnCloseOrDestroy(NpMatching2Conn *conn);
s32 npConnCheckServerBusy(NpMatching2Context *ctx, NpMatching2ServerNode *server);
s32 npConnFindBySlotAndRef(NpMatching2Context *ctx, NpMatching2ServerNode *server, u16 slotIndex, NpMatching2Conn **outConn);
s32 npConnRelease(NpMatching2Conn *conn);
s32 npConnCreate(NpMatching2ServerNode *server, u32 connKind, NpMatching2HostList *hostList, NpMatching2Conn **outConn);
s32 npConnCloseById(SceNpMatching2ContextId ctxId, u32 connId);
s32 npConnCheckQueueBusy(NpMatching2ReqQueue *queue);
s32 npConnCloseAllOnServer(NpMatching2Context *ctx, NpMatching2ServerNode *server);
s32 npConnTryFinishDestroy(NpMatching2Conn *conn);
s32 npCoreInit(u32 poolSize, u32 threadStackSize, s32 threadPriority);
s32 npCoreTerm(void);
s32 npCoreDestroyContext(SceNpMatching2ContextId ctxId);
s32 npCoreStopContext(SceNpMatching2ContextId ctxId);
s32 npCoreAbortRequest(SceNpMatching2RequestId reqId);
s32 npCoreGetServerIdListLocal(SceNpMatching2ContextId ctxId, SceNpMatching2ServerId *serverIdList, u32 listNum);
s32 npCoreGetServerInfo(SceNpMatching2ContextId ctxId, const SceNpMatching2GetServerInfoRequest *reqParam,
                        const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGrantRoomOwner(SceNpMatching2ContextId ctxId, const SceNpMatching2GrantRoomOwnerRequest *reqParam,
                         const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreKickoutRoomMember(SceNpMatching2ContextId ctxId, const SceNpMatching2KickoutRoomMemberRequest *reqParam,
                            const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetRoomDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                   const SceNpMatching2AttributeId *attrId, u32 attrIdNum,
                                   void *roomDataOut, void *buf, u32 bufSize);
s32 npCoreGetRoomMemberIdListLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                   SceNpMatching2RoomMemberId *memberIdList, u32 listNum);
s32 npCoreGetRoomMemberDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                         SceNpMatching2RoomMemberId memberId, const SceNpMatching2AttributeId *attrId,
                                         u32 attrIdNum, void *memberDataOut, void *buf, u32 bufSize);
s32 npCoreGetRoomPasswordLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                               s32 *withPasswordOut, SceNpMatching2SessionPassword *passwordOut);
void npCoreNotifyCtxEvent(NpMatching2Context *ctx, u16 eventId, u8 eventCause, s32 errorCode);
s32 npCoreCreateContext(const SceNpCommId *commId, const SceNpCommPassphrase *passPhrase,
                        SceNpMatching2ContextId *ctxIdOut, u32 option, s32 legacyMode);
s32 npCoreSetDefaultReqOpt(SceNpMatching2ContextId ctxId, const SceNpMatching2RequestOptParam *optParam);
s32 npCoreSetDefaultRoomEvOpt(SceNpMatching2ContextId ctxId, const SceNpMatching2RoomEventOptParam *optParam);
s32 npCoreSetDefaultRoomMsgOpt(SceNpMatching2ContextId ctxId, const SceNpMatching2RoomMessageOptParam *optParam);
s32 npCoreStartContext(SceNpMatching2ContextId ctxId);
s32 npCoreRegisterListener(u32 listenerId, SceNpMatching2ContextId ctxId, const void *callbacks);
s32 npCoreUnregisterListener(u32 listenerId, SceNpMatching2ContextId ctxId);
s32 npCoreGetWorldInfoList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetWorldInfoListRequest *reqParam,
                           const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSetUserInfo(SceNpMatching2ContextId ctxId, const SceNpMatching2SetUserInfoRequest *reqParam,
                      const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetUserInfoList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetUserInfoListRequest *reqParam,
                          const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetRoomMemberDataExternalList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetRoomMemberDataExternalListRequest *reqParam,
                                        const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSetRoomDataExternal(SceNpMatching2ContextId ctxId, const SceNpMatching2SetRoomDataExternalRequest *reqParam,
                              const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSetSignalingOptParam(SceNpMatching2ContextId ctxId, const SceNpMatching2SetSignalingOptParamRequest *reqParam,
                               const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetSignalingOptParamLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                    SceNpMatching2SignalingOptParam *optParamOut);
s32 npCoreGetRoomDataExternalList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetRoomDataExternalListRequest *reqParam,
                                  const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreCreateJoinRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2CreateJoinRoomRequest *reqParam,
                         const SceNpMatching2RequestOptParam *optParam, const SceNpMatching2RoomEventOptParam *roomEvOpt,
                         const SceNpMatching2RoomMessageOptParam *roomMsgOpt, SceNpMatching2RequestId *assignedReqId);
s32 npCoreJoinRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2JoinRoomRequest *reqParam,
                   const SceNpMatching2RequestOptParam *optParam, const SceNpMatching2RoomEventOptParam *roomEvOpt,
                   const SceNpMatching2RoomMessageOptParam *roomMsgOpt, SceNpMatching2RequestId *assignedReqId);
s32 npCoreLeaveRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2LeaveRoomRequest *reqParam,
                    const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSearchRoom(SceNpMatching2ContextId ctxId, const SceNpMatching2SearchRoomRequest *reqParam,
                     const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSendRoomChatMessage(SceNpMatching2ContextId ctxId, const SceNpMatching2SendRoomChatMessageRequest *reqParam,
                              const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSendRoomMessage(SceNpMatching2ContextId ctxId, const SceNpMatching2SendRoomMessageRequest *reqParam,
                          const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId, s32 reqSource);
s32 npCoreSetRoomDataInternal(SceNpMatching2ContextId ctxId, const SceNpMatching2SetRoomDataInternalRequest *reqParam,
                              const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetRoomDataInternal(SceNpMatching2ContextId ctxId, const SceNpMatching2GetRoomDataInternalRequest *reqParam,
                              const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSetRoomMemberDataInternal(SceNpMatching2ContextId ctxId, const SceNpMatching2SetRoomMemberDataInternalRequest *reqParam,
                                    const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetRoomMemberDataInternal(SceNpMatching2ContextId ctxId, const SceNpMatching2GetRoomMemberDataInternalRequest *reqParam,
                                    const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreGetRoomMemberDataInternalList(SceNpMatching2ContextId ctxId, const SceNpMatching2GetRoomMemberDataInternalListRequest *reqParam,
                                        const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreRegisterSignalingCb(SceNpMatching2ContextId ctxId, void *cbFunc, void *cbArg);
s32 npCoreDispatchSignalingCb(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                              SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, s32 errorCode);
s32 npCoreSubmitRoomInfoInternal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                 const SceNpMatching2RequestOptParam *optParam, SceNpMatching2RequestId *assignedReqId);
s32 npCoreSubmitRequest(SceNpMatching2ContextId ctxId, SceNpMatching2ServerId serverId, SceNpMatching2RoomId roomId,
                        u64 lobbyId, s32 requireAuth, u8 reqKind, SceNpMatching2Event eventId, u16 protoCmd,
                        const void *reqParam, const SceNpMatching2RequestOptParam *optParam, s32 isMaintenance,
                        SceNpMatching2RequestId *assignedReqId, s32 reqSource);
s32 npLockInit(void);
s32 npLockTerm(void);
s32 npLockAcquire(void);
s32 npLockRelease(void);

/* Wire structures shared between matching2_proto.c and matching2_transport.c */
typedef struct NpProtoFrameHeader {
    u16 versionWord;
    u16 totalLen;
} NpProtoFrameHeader;

typedef struct NpProtoBodyHeader {
    u16 cmd;
    u8 flags;
    u8 pad;
    u32 txId;
    u8 route[4];
    u8 hmac[16];
} NpProtoBodyHeader;

typedef struct NpProtoRoutePair {
    u16 firstWord;
    u16 secondWord;
} NpProtoRoutePair;

typedef struct NpProtoIntFilterWire {
    u8 op;
    u8 pad[3];
    u16 attrId;
    u16 attrPad;
    u32 value;
} NpProtoIntFilterWire;

typedef struct NpProtoBinFilterWire {
    u8 op;
    u8 pad[3];
    u16 attrId;
    u16 attrPad;
    const void *ptr;
    u32 size;
} NpProtoBinFilterWire;

typedef struct NpProtoRangeWire {
    u32 startIndex;
    u32 maxNum;
} NpProtoRangeWire;

typedef struct NpProtoMsgOptData {
    u8 flag;
    u8 pad[3];
    s32 hasProfile;
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
} NpProtoMsgOptData;

typedef struct NpProtoOptDataBlock {
    u8 data[16];
    u32 size;
} NpProtoOptDataBlock;

typedef struct NpProtoGroupConfigWire {
    u8 groupId;
    u8 withPassword;
    u8 label[8];
} NpProtoGroupConfigWire;

typedef struct NpProtoRedirectInfo {
    u8 kind;
    u8 padByte;
    u16 extraWord;
    u8 statusByte;
    u8 hostPad;
    u16 port;
    char host[130];
    u16 alignPad;
} NpProtoRedirectInfo;

typedef struct NpMatching2ShortBlob {
    u32 len;
    u8 bytes[8];
} NpMatching2ShortBlob;

typedef struct NpMatching2GroupConfigInput {
    u32 slotNum;
    s32 withLabel;
    u8 label[8];
    s32 withPassword;
} NpMatching2GroupConfigInput;

typedef struct NpMatching2PasswordConfigInput {
    u8 groupId;
    u8 padBytes[3];
    s32 withPassword;
} NpMatching2PasswordConfigInput;

typedef struct NpMatching2UserProfile {
    SceNpId npId;
    char onlineName[52];
    char avatarUrl[128];
} NpMatching2UserProfile;

/* Request wrapper structs shared between matching2_context.c, matching2_session.c, and matching2_transport.c */
typedef struct NpSetUserInfoWrap {
    SceNpMatching2SetUserInfoRequest *req;
} NpSetUserInfoWrap;

typedef struct NpGetUserInfoWrap {
    SceNpMatching2GetUserInfoListRequest *req;
    char (*npHandles)[20];
} NpGetUserInfoWrap;

typedef struct NpGetRoomMemberExtWrap {
    NpMatching2Context *ctx;
    u32 reservedPad;
    SceNpMatching2RoomId roomId;
    SceNpMatching2AttributeId *attrList;
    u32 attrCount;
} NpGetRoomMemberExtWrap;

typedef struct NpSetRoomDataExtWrap {
    SceNpMatching2SetRoomDataExternalRequest *req;
} NpSetRoomDataExtWrap;

typedef struct NpCtxReqPairWrap {
    NpMatching2Context *ctx;
    void *req;
} NpCtxReqPairWrap;

typedef struct NpSetSigOptWrap {
    SceNpMatching2SetSignalingOptParamRequest *req;
    u32 sigOptLen;
    SceNpMatching2SignalingOptParam sigOptParam;
} NpSetSigOptWrap;

typedef struct NpCreateJoinWrap {
    SceNpMatching2CreateJoinRoomRequest *req;
    SceNpMatching2RoomEventOptParam *roomEvOpt;
    SceNpMatching2RoomMessageOptParam *roomMsgOpt;
    char (*allowedHandles)[20];
    char (*blockedHandles)[20];
    u16 roomNum;
    u16 roomNumPad;
    NpMatching2BlobBuf roomTicket;
    u8 sigOptWire[32];
    s32 hasSigOpt;
    NpMatching2BlobBuf roomCookie;
    u8 groupBitmap[32];
    s32 hasGroupBitmap;
    NpProtoGroupConfigWire *groupConfigs;
    u32 groupConfigCount;
    NpMatching2Room *createdRoom;
    NpMatching2ShortBlob *sigOptBlob;
} NpCreateJoinWrap;

typedef struct NpJoinRoomWrap {
    SceNpMatching2JoinRoomRequest *req;
    SceNpMatching2RoomEventOptParam *roomEvOpt;
    SceNpMatching2RoomMessageOptParam *roomMsgOpt;
    NpProtoRedirectInfo redirectInfo;
    NpMatching2BlobBuf roomTicket;
    u8 sigOptWire[32];
    s32 hasSigOpt;
    NpMatching2BlobBuf roomCookie;
    NpMatching2Room *joinedRoom;
} NpJoinRoomWrap;

typedef struct NpLeaveRoomWrap {
    NpProtoOptDataBlock optData;
    NpMatching2BlobBuf notifyCookie;
} NpLeaveRoomWrap;

typedef struct NpGrantRoomOwnerWrap {
    SceNpMatching2RoomMemberId newOwner;
    u16 padWord;
    NpProtoOptDataBlock optData;
} NpGrantRoomOwnerWrap;

typedef struct NpKickoutMemberWrap {
    SceNpMatching2RoomMemberId target;
    u8 blockFlag;
    u8 padByte;
    NpProtoOptDataBlock optData;
} NpKickoutMemberWrap;

typedef struct NpSendRoomChatWrap {
    SceNpMatching2SendRoomChatMessageRequest *req;
} NpSendRoomChatWrap;

typedef struct NpSendRoomMsgWrap {
    SceNpMatching2SendRoomMessageRequest *req;
} NpSendRoomMsgWrap;

typedef struct NpSetRoomDataInternalWrap {
    NpMatching2ShortBlob extraAttr;
    SceNpMatching2SetRoomDataInternalRequest *req;
    u64 computedPasswordMask;
    s32 hasComputedMask;
    u32 tailPad;
} NpSetRoomDataInternalWrap;

typedef struct NpGetRoomAttrWrap {
    NpMatching2Context *ctx;
    SceNpMatching2RoomMemberId memberId;
    u16 padWord;
    SceNpMatching2AttributeId *attrId;
    u32 attrIdNum;
} NpGetRoomAttrWrap;

typedef struct NpSetRoomMemberDataInternalWrap {
    SceNpMatching2SetRoomMemberDataInternalRequest *req;
    NpMatching2ShortBlob extraAttr;
} NpSetRoomMemberDataInternalWrap;

/* External API response structures built by matching2_transport.c / matching2_session.c / matching2_worker.c */
typedef struct SceNpMatching2UserInfoOut {
    struct SceNpMatching2UserInfoOut *next;
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
    SceNpMatching2BinAttr *userBinAttr;
    u32 userBinAttrNum;
    SceNpMatching2JoinedSessionInfo *joinedSessionList;
    u32 joinedSessionNum;
} SceNpMatching2UserInfoOut;

typedef struct SceNpMatching2RoomOwnerInfoOut {
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
} SceNpMatching2RoomOwnerInfoOut;

typedef struct SceNpMatching2RoomDataExternalOut {
    struct SceNpMatching2RoomDataExternalOut *next;
    SceNpMatching2ServerId serverId;
    u16 serverPad;
    SceNpMatching2WorldId worldId;
    u32 worldPad;
    u64 lobbyId;
    SceNpMatching2RoomId roomId;
    u32 maxSlot;
    u32 curMemberNum;
    u64 passwordSlotMask;
    SceNpMatching2RoomOwnerInfoOut *owner;
    SceNpMatching2RoomGroup *roomGroup;
    u32 roomGroupNum;
    SceNpMatching2FlagAttr flagAttr;
    SceNpMatching2IntAttr *searchableIntAttr;
    u32 searchableIntAttrNum;
    SceNpMatching2BinAttr *searchableBinAttr;
    u32 searchableBinAttrNum;
    SceNpMatching2BinAttr *binAttrExternal;
    u32 binAttrExternalNum;
} SceNpMatching2RoomDataExternalOut;

/* Signaling shared structures */
typedef struct NpSigTimer {
    u32 opaque[9];
} NpSigTimer;

typedef struct NpSigPipeMsg {
    u32 msgKind;
    u32 targetId;
    u32 payloadWord;
    u32 auxWord;
} NpSigPipeMsg;

typedef struct NpSigSessionRef {
    struct NpSigSessionRef *next;
    s32 sessionId;
} NpSigSessionRef;

typedef struct NpSigCand {
    struct NpSigCand *next;
    u32 candType;
    u32 peerAddr;
    u32 mappedAddr;
    u16 peerPort;
    u16 mappedPort;
    u32 flags;
    u32 rttUsec;
    s32 sockFd;
    u16 localPort;
    u8 sentCount;
    u8 recvCount;
} NpSigCand;

typedef struct NpSigSignalPayload {
    u16 srcTag;
    u16 dstTag;
    u8 sessionKey[16];
    u16 natType;
    u16 natPad;
    NpSigCand *candList;
} NpSigSignalPayload;

typedef struct NpSigRoomMember {
    SceNpMatching2RoomMemberId memberId;
    SceNpId npId;
    u8 alignPad[2];
    u32 connId;
    s32 connState;
    s32 lastError;
    SceNpMatching2RoomMemberId leftMemberId;
    u16 leftPad;
} NpSigRoomMember;

typedef struct NpSigRoom {
    struct NpSigRoom *next;
    u32 alignPad;
    u32 roomIdLo;
    u32 roomIdHi;
    SceNpMatching2RoomMemberId meMemberId;
    SceNpMatching2RoomMemberId ownerMemberId;
    s32 maxPeers;
    SceNpMatching2SignalingOptParam sigOptParam;
    NpSigRoomMember *peers;
    u32 tailPad;
} NpSigRoom;

typedef struct NpSigSession {
    struct NpSigSession *next;
    s32 sessionId;
    SceNpId npId;
    s32 (*cbFunc)(s32, u32, u16, s32, void *);
    void *cbArg;
    s32 refCount;
    u32 stateFlags;
    SceNpMatching2ContextId ctxId;
    u16 ctxPad;
    NpSigRoom *roomList;
} NpSigSession;

typedef struct NpSigReq {
    struct NpSigReq *next;
    s32 sessionId;
    u32 reqId;
    u32 subKey;
    s32 refCount;
    u32 stateFlags;
    u32 extraKey;
} NpSigReq;

typedef struct NpSigPeerNetInfoNode {
    struct NpSigPeerNetInfoNode *next;
    s32 sessionId;
    u32 reqId;
    u32 infoSize;
    u32 localAddr;
    u32 mappedAddr;
    u32 natType;
    u32 upnpStatus;
    u32 upnpMapped;
    u16 mtuSize;
    u16 mtuPad;
} NpSigPeerNetInfoNode;

typedef struct NpSigConn {
    struct NpSigConn *next;
    struct NpSigConn *symNext;
    s32 state;
    u32 connId;
    SceNpId selfNpId;
    SceNpId peerNpId;
    u16 localSessionTag;
    u16 peerSessionTag;
    u8 localKey[16];
    u8 peerKey[16];
    u32 localMappedAddr;
    u32 peerSelectedAddr;
    u16 localMappedPort;
    u16 peerSelectedPort;
    u32 lastTxId;
    NpSigTimer connectTimer;
    NpSigTimer echoTimer;
    NpSigTimer bwTimer;
    u64 echoDeadlineTick;
    u32 bwLastStampLo;
    u32 bwLastStampHi;
    u64 bwFirstRecvTick;
    NpSigCand *candList;
    NpSigSessionRef *sessionRefs;
    u16 peerNatType;
    u16 refCount;
    u32 peerBandwidth;
    u32 flags;
    s32 symUdpSock;
    s32 symSigSock;
    u16 symLocalPort;
    u16 symMappedPort;
    s32 selectedSockFd;
    u16 selectedLocalPort;
    u8 bwProbeSentCount;
    u8 bwSampleIndex;
    u32 bwSamples[8];
    u32 rttSampleCount;
    s32 rttLastSample;
    s32 rttSamples[6];
    u32 connectBudgetUsec;
    u32 tailPad;
} NpSigConn;

typedef struct NpSigGlobal {
    char clientVer[12];
    u32 stackSize;
    s32 priority;
    s32 mainThid;
    s32 echoThid;
    s32 mainMutex;
    s32 msgPipe;
    s32 echoMbx;
    s32 selectMutex;
    u32 localKey[4];
    s32 stunHandle;
    u64 stunCacheExpireTick;
    u32 stunMappedAddr;
    u16 stunMappedPort;
    u16 stunPortPad;
    u16 stunState;
    u16 symStunBusy;
    s32 stopEchoFlag;
    SceNpId selfNpId;
    u16 sharedBindPort;
    u16 natType;
    s32 sharedUdpSock;
    s32 sharedSigSock;
    s32 connCount;
    NpSigTimer pollTimer;
    NpSigSession *sessionList;
    NpSigReq *reqList;
    NpSigConn *connList;
    NpSigPeerNetInfoNode *peerNetInfoList;
    NpSigConn *symConnList;
    u32 tailPad;
} NpSigGlobal;

typedef struct NpSigUpnpInfo {
    u32 upnpStatus;
    u32 natDetectStatus;
    u16 natType;
    u16 natPad;
    u32 extAddr;
    u32 isSymmetric;
    u32 symKind;
    s32 portDelta;
    u32 reservedWord;
    u16 mtuSize;
    u16 upnpPort;
    u16 upnpAddrPad;
    u16 upnpAddrLo;
    u16 upnpAddrHi;
    u16 upnpTailPad;
    u32 upnpMapped;
} NpSigUpnpInfo;

typedef struct NpSigLocalNetInfo {
    u32 size;
    u32 localAddr;
    u32 mappedAddr;
    u32 natType;
    u32 upnpStatus;
    u32 upnpMapped;
    u16 mtuSize;
    u16 padWord;
} NpSigLocalNetInfo;

typedef struct NpSigEchoMbxMsg {
    u32 mbxHdr[2];
    s32 sockFd;
    u32 pktLen;
    SceNetInetSockaddrIn dstAddr;
    u32 addrLen;
    void *pktBuf;
} NpSigEchoMbxMsg;

typedef struct NpSigEchoEventData {
    u32 peerAddr;
    u32 reflectedAddr;
    u16 peerPort;
    u16 reflectedPort;
    u32 stampHi;
    u32 stampLo;
    s32 recvSockFd;
} NpSigEchoEventData;

typedef struct NpSigIoctlPeerParam {
    u16 reservedWord;
    u16 peerPort;
    u32 peerAddr;
    u8 reservedBytes[8];
    u8 peerKey[16];
    u16 localPort;
    u16 tailPad;
} NpSigIoctlPeerParam;

typedef struct NpProtoWorldInfo {
    struct NpProtoWorldInfo *next;
    SceNpMatching2WorldId worldId;
    u32 numOfLobby;
    u32 maxNumOfLobby;
    u32 curNumOfLobbyMember;
    u32 curNumOfRoom;
    u32 curNumOfRoomMember;
    s32 withAttr;
    u8 attrData[32];
} NpProtoWorldInfo;

typedef struct NpProtoUserInfo {
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
    SceNpMatching2BinAttr userBinAttr;
    SceNpMatching2JoinedSessionInfo *joinedSessionList;
    u32 joinedSessionNum;
} NpProtoUserInfo;

typedef struct NpProtoRoomDataExternal {
    SceNpMatching2WorldId worldId;
    u32 worldPad;
    u64 lobbyId;
    SceNpMatching2RoomId roomId;
    u32 flagAttr;
    u16 maxSlot;
    u8 reservedMeta[16];
    u8 publicSlotMeta[32];
    u16 padBeforeTime;
    u64 openDate;
    SceNpMatching2IntAttr searchableIntAttr[8];
    SceNpMatching2BinAttr searchableBinAttr;
    SceNpMatching2BinAttr binAttrExternal[2];
    SceNpId *ownerNpId;
    char *ownerOnlineName;
    char *ownerAvatarUrl;
    u32 extMetaLen;
    u8 extMeta[12];
} NpProtoRoomDataExternal;

typedef struct NpProtoRoomMemberExternal {
    struct NpProtoRoomMemberExternal *next;
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
    u64 joinDate;
    u8 natType;
    u8 tailPad[7];
} NpProtoRoomMemberExternal;

typedef struct NpProtoRoomDataInternalWire {
    u16 roomHandle;
    u16 handlePad;
    u32 flagAttr;
    u16 maxSlot;
    u8 roomMeta[16];
    u16 metaPad[3];
    u64 passwordSlotMask;
    u8 publicMeta[32];
    SceNpMatching2BinAttrInternal binAttrInternal[2];
    u32 roomAttrLen;
    u8 roomAttrBytes[8];
    NpProtoGroupConfigWire *groupList;
    u32 groupCount;
    u32 tailPad;
} NpProtoRoomDataInternalWire;

typedef struct NpProtoRoomMemberInternalWire {
    SceNpMatching2RoomMemberId memberId;
    u16 memberPad;
    u32 alignPad;
    u64 joinDate;
    u32 flagAttr;
    u8 natType;
    u8 teamId;
    u16 attrPad;
    SceNpId npId;
    char *onlineName;
    char *avatarUrl;
    u32 padBeforeAttr;
    SceNpMatching2MemberBinAttrInternal binAttrInternal;
    u32 extraAttrLen;
    u8 extraAttrBytes[12];
} NpProtoRoomMemberInternalWire;

/* matching2_proto.c public prototypes */
s32 npProtoParseFrameHeader(const u8 *buf, u32 bufLen, NpProtoFrameHeader *outHdr);
s32 npProtoParseRespHeader(const u8 *buf, u32 bufLen, NpProtoBodyHeader *outHdr);
s32 npProtoParseNotifyHeader(const u8 *buf, u32 bufLen, NpProtoBodyHeader *outHdr);
s32 npProtoBuildAuthReq(u8 *outBuf, u32 outBufSize, u32 txId, const char *clientVer, const SceNpCommId *commId, u16 serverId, const SceNpId *npId, const char *onlineName, const char *avatarUrl, const SceNpCommPassphrase *passPhrase, const void *ticket, u16 ticketLen, const void *psid);
s32 npProtoBuildGetUserInfoListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const void *hmacKey, const SceNpCommId *commId, u16 serverId, const char (*handles)[20], u32 handleCount, const SceNpMatching2AttributeId *attrIds, u32 attrIdCount);
s32 npProtoBuildGetServerInfoReq(u8 *outBuf, u32 outBufSize, u32 txId, const void *hmacKey, const SceNpCommId *commId, u16 serverId);
s32 npProtoParseGetServerInfoResp(const u8 *buf, u32 bufLen, u16 *statusOut, u8 *versionOut);
s32 npProtoParseAuthResp(const u8 *buf, u32 bufLen, u32 *sessionIdOut, NpMatching2BlobBuf *sessionTokenOut, NpMatching2BlobBuf *sessionKeyOut, NpMatching2BlobBuf *keepaliveCookieOut, u16 *keepaliveIntervalOut);
s32 npProtoBuildKeepaliveReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const u16 *worldIds, u32 worldIdCount);
s32 npProtoParseKeepaliveResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut);
s32 npProtoBuildSetUserInfoReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const SceNpMatching2BinAttr *userAttr);
s32 npProtoParseSetUserInfoResp(void);
s32 npProtoParseGetUserInfoListResp(const u8 *buf, u32 bufLen, u16 *countOut, SceNpMatching2ServerId serverId, NpProtoUserInfo **listOut);
void npProtoFreeUserInfoList(NpProtoUserInfo *list, u16 count);
s32 npProtoBuildGetWorldInfoListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey);
s32 npProtoParseGetWorldInfoListResp(const u8 *buf, u32 bufLen, u32 *countOut, NpProtoWorldInfo **listOut, SceNpMatching2ServerId serverId);
s32 npProtoBuildSearchRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, u16 protoCmd, u16 worldNum, u32 unusedPad, u64 lobbyId, const NpProtoRangeWire *range, u32 flagFilter, u32 flagAttr, const NpProtoIntFilterWire *intFilters, u32 intFilterNum, const NpProtoBinFilterWire *binFilters, u32 binFilterNum, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
void npProtoFreeRoomDataExternalList(NpProtoRoomDataExternal *list, u16 count);
s32 npProtoBuildCreateRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie, const SceNpMatching2IntAttr *intAttrs, u32 intAttrNum, const SceNpMatching2BinAttr *binAttrs, u32 binAttrNum, const NpMatching2BlobBuf *extMeta);
s32 npProtoBuildGetRoomMemberExtListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, u32 unusedPad, u16 roomNum, u16 worldNum, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
s32 npProtoBuildSetRoomDataExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, u32 unusedPad, u16 roomNum, u16 worldNum, const SceNpMatching2IntAttr *intAttrs, u32 intAttrNum, const SceNpMatching2BinAttr *binAttrs, u32 binAttrNum, const NpMatching2BlobBuf *extMeta);
s32 npProtoBuildGetRoomDataExtListReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const SceNpMatching2RoomId *roomIds, u32 roomIdNum, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
s32 npProtoBuildReserveRoomSlotReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, u16 worldNum, u64 lobbyId, SceNpMatching2RoomId roomId);
s32 npProtoBuildReserveJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, SceNpMatching2WorldId worldId, SceNpMatching2RoomId roomId);
s32 npProtoParseSearchRoomResp(const u8 *buf, u32 bufLen, u32 *startIndexOut, u32 *totalOut, u32 *countOut, NpProtoRoomDataExternal **listOut, SceNpMatching2ServerId serverId);
s32 npProtoParseRedirectResp(const u8 *buf, u32 bufLen, NpProtoRedirectInfo *redirectOut, s32 *foundOut);
s32 npProtoBuildJoinRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie);
s32 npProtoParseJoinRoomExtResp(void);
s32 npProtoBuildLeaveRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie);
s32 npProtoParseLeaveRoomExtResp(void);
s32 npProtoBuildUpdateRoomExtReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie);
s32 npProtoParseUpdateRoomExtResp(void);
s32 npProtoParseGetRoomMemberExtListResp(const u8 *buf, u32 bufLen, u16 *countOut, NpProtoRoomMemberExternal **listOut);
void npProtoFreeRoomMemberExtList(NpProtoRoomMemberExternal *list, u16 count);
s32 npProtoParseSetRoomDataExtResp(void);
s32 npProtoParseGetRoomDataExtListResp(const u8 *buf, u32 bufLen, u16 *countOut, NpProtoRoomDataExternal **listOut, SceNpMatching2ServerId serverId);
s32 npProtoParseReserveRoomResp(const u8 *buf, u32 bufLen, u16 *roomHandleOut, NpProtoRedirectInfo *redirectOut, NpMatching2BlobBuf *ticketOut, NpMatching2HostList *hostListOut, void *sigOptOut, s32 *hasSigOptOut);
s32 npProtoBuildReleaseRoomSlotReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *roomCookie);
s32 npProtoParseReleaseRoomSlotResp(void);
s32 npProtoParseEmptyResp(void);
s32 npProtoBuildRoomKeepaliveReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *sessionToken, const NpMatching2BlobBuf *keepaliveCookie, u8 flags);
s32 npProtoParseRoomKeepaliveResp(const u8 *buf, u32 bufLen, NpMatching2HostList *hostListOut);
s32 npProtoBuildCreateJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u32 sessionId, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *sessionToken, const SceNpId *npId, const NpMatching2BlobBuf *keepaliveCookie, const NpMatching2BlobBuf *roomTicket, u32 flagAttr, s32 hasFlagAttr, u32 maxSlot, const void *publicMeta, const SceNpMatching2BinAttr *roomBinAttrs, u32 roomBinAttrNum, const SceNpMatching2SessionPassword *password, const u64 *passwordSlotMask, const NpProtoGroupConfigWire *groups, u32 groupNum, const char (*allowedUsers)[20], u32 allowedUserNum, const char (*blockedUsers)[20], u32 blockedUserNum, const NpMatching2BlobBuf *roomAttr, u8 natType, u8 teamId, const SceNpMatching2BinAttr *memberBinAttr, const NpMatching2BlobBuf *memberExtraAttr, const void *joinGroupLabel);
s32 npProtoBuildJoinRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, const NpMatching2BlobBuf *sessionKey, const NpMatching2BlobBuf *sessionToken, const NpMatching2BlobBuf *keepaliveCookie, const NpMatching2BlobBuf *roomToken, u8 natType, u8 teamId, const SceNpMatching2BinAttr *memberBinAttr, const NpMatching2BlobBuf *memberExtraAttr, const SceNpMatching2SessionPassword *password, const void *joinGroupLabel, const NpMatching2BlobBuf *roomTicket, const NpProtoOptDataBlock *optData);
s32 npProtoBuildSetRoomDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, u32 flagFilter, u32 flagAttr, const SceNpMatching2BinAttr *roomBinAttrs, u32 roomBinAttrNum, const u64 *passwordSlotMask, const NpMatching2BlobBuf *roomAttr, u8 ownerPrivFlag, const SceNpMatching2RoomMemberId *ownerPrivRank, u32 ownerPrivRankNum);
s32 npProtoBuildSetRoomMemberDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId senderMemberId, const NpMatching2BlobBuf *roomKey, SceNpMatching2RoomMemberId targetMemberId, u32 flagFilter, u32 flagAttr, u8 natType, u8 teamId, const SceNpMatching2BinAttr *memberBinAttr, const NpMatching2BlobBuf *memberExtraAttr, u8 notifyFlag);
s32 npProtoBuildGetRoomMemberDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId senderMemberId, const NpMatching2BlobBuf *roomKey, SceNpMatching2RoomMemberId targetMemberId, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
s32 npProtoBuildSendRoomChatReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, const void *castMask, const void *msg, u16 msgLen, const NpProtoMsgOptData *optData);
s32 npProtoParseRoomChatNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *srcMemberIdOut, void *msgIdOut, s32 *hasMsgIdOut, void **msgOut, u32 *msgLenOut, NpProtoMsgOptData *optDataOut, void *filteredOut);
s32 npProtoBuildSendRoomMsgReq(u8 *outBuf, u32 outBufSize, u8 hdrFlags, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, const void *castMask, const void *msg, u16 msgLen, const NpProtoMsgOptData *optData);
s32 npProtoParseRoomMsgNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *srcMemberIdOut, void *msgIdOut, s32 *hasMsgIdOut, void **msgOut, u32 *msgLenOut, NpProtoMsgOptData *optDataOut);
s32 npProtoBuildGrantRoomOwnerReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, SceNpMatching2RoomMemberId newOwnerId, const NpProtoOptDataBlock *optData);
s32 npProtoBuildKickoutRoomMemberReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, SceNpMatching2RoomMemberId targetMemberId, u8 blockFlag, s32 hasBlockFlag, const NpProtoOptDataBlock *optData);
s32 npProtoParseCreateJoinRoomResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *roomKeyOut, NpMatching2BlobBuf *roomCookieOut, NpMatching2BlobBuf *roomTokenOut, NpProtoRoomDataInternalWire *roomDataOut, NpProtoRoomMemberInternalWire *memberDataOut);
void npProtoFreeRoomBinAttrIntArray(SceNpMatching2BinAttrInternal *attrs, u16 count);
void npProtoFreeRoomDataInt(NpProtoRoomDataInternalWire *roomData);
s32 npProtoParseJoinRoomResp(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *meMemberIdOut, NpMatching2BlobBuf *roomKeyOut, NpMatching2BlobBuf *roomCookieOut, NpMatching2BlobBuf *roomTokenOut, NpProtoRoomDataInternalWire *roomDataOut, u16 *memberCountOut, NpProtoRoomMemberInternalWire **memberListOut);
void npProtoFreeRoomMemberIntList(NpProtoRoomMemberInternalWire *list, u16 count);
s32 npProtoBuildLeaveRoomReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, const NpProtoOptDataBlock *optData);
s32 npProtoParseLeaveRoomResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut);
s32 npProtoBuildGetRoomMemberIntListReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
s32 npProtoParseGetRoomMemberIntListResp(const u8 *buf, u32 bufLen, u16 *countOut, NpProtoRoomMemberInternalWire **listOut);
s32 npProtoBuildGetRoomDataIntReq(u8 *outBuf, u32 outBufSize, u32 txId, u16 roomHandle, SceNpMatching2RoomMemberId memberId, const NpMatching2BlobBuf *roomKey, const SceNpMatching2AttributeId *attrIds, u32 attrIdNum);
s32 npProtoParseGetRoomDataIntResp(const u8 *buf, u32 bufLen, NpProtoRoomDataInternalWire *roomDataOut);
s32 npProtoParseSetRoomDataIntResp(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut, u64 *updateDateOut);
s32 npProtoParseSetRoomMemberDataIntResp(const u8 *buf, u32 bufLen, u64 *updateDateOut);
s32 npProtoParseGetRoomMemberDataIntResp(const u8 *buf, u32 bufLen, NpProtoRoomMemberInternalWire *memberDataOut);
void npProtoFreeRoomMemberInt(NpProtoRoomMemberInternalWire *memberData);
s32 npProtoParseSendRoomChatResp(const u8 *buf, u32 bufLen, void *msgIdOut, void *filteredOut);
s32 npProtoParseSendRoomMsgResp(const u8 *buf, u32 bufLen, void *msgIdOut);
s32 npProtoParseMemberJoinedNotify(const u8 *buf, u32 bufLen, NpProtoRoomMemberInternalWire *memberOut, NpMatching2BlobBuf *cookieOut, NpProtoGroupConfigWire *groupOut, s32 *hasGroupOut, NpProtoOptDataBlock *optDataOut);
s32 npProtoParseMemberUpdateNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *memberIdOut, u32 *flagFilterOut, u32 *flagAttrOut, u8 *natTypeOut, u8 *teamIdOut, SceNpMatching2MemberBinAttrInternal *binAttrOut, NpMatching2BlobBuf *extraAttrOut);
void npProtoFreeMemberBinAttrIntArray(SceNpMatching2MemberBinAttrInternal *attrs, u32 count);
s32 npProtoParseRoomDataUpdateNotify(const u8 *buf, u32 bufLen, NpMatching2BlobBuf *cookieOut, u32 *flagFilterOut, u32 *flagAttrOut, u16 maxBinAttrs, SceNpMatching2BinAttrInternal *binAttrOut, NpMatching2BlobBuf *roomAttrOut, u64 *passwordSlotMaskOut, s32 *hasPasswordMaskOut);
s32 npProtoParseMemberLeftNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *memberIdOut, SceNpId *npIdOut, u8 *reasonOut, NpMatching2BlobBuf *cookieOut, NpProtoOptDataBlock *optDataOut, NpProtoGroupConfigWire *groupOut, s32 *hasGroupOut);
s32 npProtoParseGrantRoomOwnerResp(void);
s32 npProtoParseKickoutRoomMemberResp(void);
s32 npProtoParseRoomOwnerChangedNotify(const u8 *buf, u32 bufLen, SceNpMatching2RoomMemberId *newOwnerIdOut, SceNpId *npIdOut, u8 *reasonOut, NpMatching2BlobBuf *cookieOut, SceNpMatching2SessionPassword *passwordOut, s32 *hasPasswordOut, NpProtoOptDataBlock *optDataOut);
s32 npProtoParseRoomDestroyedNotify(const u8 *buf, u32 bufLen, u8 *reasonOut, NpMatching2BlobBuf *cookieOut, NpProtoOptDataBlock *optDataOut);

/* Cross-file prototypes implemented in matching2_transport.c / matching2_session.c / matching2_worker.c */
s32 npTransportInit(void);
s32 npTransportTerm(void);
s32 npTransportResolveHost(NpMatching2Conn *conn, s32 *isNumericOut, u32 hostIndex);
s32 npTransportOpenSocket(NpMatching2Conn *conn);
s32 npTransportClearNonBlocking(s32 sockFd);
s32 npConnSetKind(u32 connKind, NpMatching2Conn *conn);
s32 npTransportSelectHostPort(NpMatching2Conn *conn, u32 hostIndex);
s32 npConnResetTransport(NpMatching2Conn *conn);
s32 npConnMarkConnectPending(NpMatching2Conn *conn);
s32 npTransportPollResolver(NpMatching2Conn *conn, s32 *doneOut);
s32 npTransportGetMaxSockFdPlusOne(void);
s32 npTransportParseFrames(NpMatching2Conn *conn, u8 *pktBuf, u32 pktLen, u32 maxBufLen, u32 *neededLenOut, u32 *consumedOut);
s32 npTransportBuildReqPacket(NpMatching2Conn *conn, NpMatching2Req *req, u8 *buf, u32 bufSize, s32 *writtenLenOut);
s32 npTransportSendRoomExtCookie(NpMatching2Conn *conn, NpMatching2BlobBuf *cookie, u16 protoCmd);

s32 npReqCreate(SceNpMatching2ContextId ctxId, s32 reqSource, u8 reqKind, SceNpMatching2Event eventId,
                u16 protoCmd, const void *reqParam, NpMatching2Conn *conn, const SceNpMatching2RequestOptParam *optParam,
                s32 isMaintenance, SceNpMatching2RequestId *assignedReqId, NpMatching2Req **outReq);
s32 npReqRemoveFromQueue(NpMatching2Req *req, NpMatching2ReqQueue *queue, NpMatching2Conn *conn, NpMatching2ServerNode *server);
s32 npReqAbortOnSubmitFail(NpMatching2Context *ctx, NpMatching2Req *req, s32 errorCode);
s32 npReqRebindConn(SceNpMatching2ContextId ctxId, NpMatching2Req *req, NpMatching2Conn *conn);
s32 npReqEnqueue(NpMatching2Req *req, NpMatching2Conn *conn, NpMatching2ReqQueue *queue);
s32 npReqCountQueue(NpMatching2ReqQueue *queue);
s32 npReqPopHead(NpMatching2ReqQueue *queue, NpMatching2Req **outReq, NpMatching2Conn *conn, NpMatching2ServerNode *server);
s32 npReqFindInQueue(NpMatching2ReqQueue *queue, SceNpMatching2RequestId reqId, NpMatching2Req **outReq);
s32 npReqFindByTxId(u32 txId, NpMatching2Conn *conn, NpMatching2Req **outReq);
NpMatching2Req *npReqQueueGetFirstRef(NpMatching2ReqQueue *queue);
NpMatching2Req *npReqQueueGetNextRef(NpMatching2Req *req);
s32 npReqRelease(NpMatching2Req *req);

s32 npRoomAlloc(SceNpMatching2ServerId serverId, u16 worldNum, u16 lobbyPart, u16 roomNum,
                NpMatching2Conn *conn, const SceNpMatching2RoomEventOptParam *roomEvOpt,
                const SceNpMatching2RoomMessageOptParam *roomMsgOpt, NpMatching2Room **outRoom);
s32 npRoomUpdateDataInternal(NpMatching2Room *room, const u32 *flagAttr,
                             SceNpMatching2BinAttrInternal *binAttrs, u32 binAttrNum,
                             SceNpMatching2BinAttrInternal **updatedAttrsOut, u32 *updatedAttrCountInOut,
                             const u64 *passwordSlotMask, s32 *passwordChangedOut,
                             SceNpMatching2RoomGroup **updatedGroupsOut, u32 *updatedGroupCountInOut);
s32 npRoomCreateMember(const SceNpId *npId, const char *onlineName, const char *avatarUrl,
                       u64 joinDate, SceNpMatching2RoomMemberId memberId, u8 teamId, u8 natType,
                       u32 flagAttr, NpMatching2Room *room, NpMatching2RoomMember **outMember);
s32 npRoomInsertMember(const SceNpId *npId, const char *onlineName, const char *avatarUrl,
                       u64 joinDate, SceNpMatching2RoomMemberId memberId, u8 teamId, u8 natType,
                       u32 flagAttr, const SceNpId *selfNpId, NpMatching2Room *room, NpMatching2RoomMember **outMember);
s32 npRoomFreeMember(NpMatching2RoomMember *member);
s32 npRoomUpdateMember(NpMatching2RoomMember *member, u32 flagFilter, u32 flagAttr, u8 teamId, u8 natType,
                       SceNpMatching2MemberBinAttrInternal *binAttrs, u32 binAttrNum,
                       SceNpMatching2MemberBinAttrInternal **updatedOut, u32 *updatedCountInOut);
s32 npRoomCopyMemberBinAttrs(NpMatching2RoomMember *member, const SceNpMatching2BinAttr *srcAttrs,
                             u32 srcCount, const u64 *updateDate);
s32 npRoomRemoveMemberById(NpMatching2Room *room, SceNpMatching2RoomMemberId memberId, NpMatching2RoomMember **outMember);
void npRoomDestroy(NpMatching2Room *room);
s32 npRoomFindByIdAndRef(NpMatching2Context *ctx, SceNpMatching2RoomId roomId, NpMatching2Room **outRoom);
s32 npRoomConvertExternalList(NpProtoRoomDataExternal *srcList, u16 count,
                              const SceNpMatching2AttributeId *attrIds, u16 attrIdCount,
                              SceNpMatching2RoomDataExternalOut **outList);
s32 npRoomFreeExternalList(SceNpMatching2RoomDataExternalOut *list, u16 count);
s32 npRoomAbortJoinOnError(NpMatching2Req *req, s32 errorCode, const void *optData);
s32 npRoomAddMembersFromWire(NpProtoRoomMemberInternalWire *members, u16 count, NpMatching2Room *room);
s32 npRoomAttachToServer(NpMatching2ServerNode *server, NpMatching2Room *room);
void npRoomRelease(NpMatching2Room *room);
s32 npRoomGetFromConn(NpMatching2Conn *conn, NpMatching2Room **outRoom);
s32 npRoomFindMemberById(NpMatching2Room *room, SceNpMatching2RoomMemberId memberId, NpMatching2RoomMember **outMember);
s32 npRoomSetOwner(NpMatching2Room *room, NpMatching2RoomMember *newOwner);
s32 npServerCheckEmptyAndFree(NpMatching2ServerNode *server);

s32 npWorkerStartContext(NpMatching2Context *ctx);
s32 npWorkerCheckContextBusy(NpMatching2Context *ctx);
s32 npWorkerStopContextServers(NpMatching2Context *ctx);
s32 npWorkerCleanupContext(NpMatching2Context *ctx);
s32 npServerFindByIdAndRef(NpMatching2Context *ctx, SceNpMatching2ServerId serverId, NpMatching2ServerNode **outServer);
s32 npServerRelease(NpMatching2ServerNode *server);
NpMatching2ServerNode *npServerGetFirstRef(NpMatching2Context *ctx);
NpMatching2ServerNode *npServerGetNextRef(NpMatching2ServerNode *server);
s32 npServerRecordOwnedRoom(u16 worldNum, NpMatching2ServerNode *server);
s32 npServerReleaseOwnedRoom(u16 worldNum, NpMatching2ServerNode *server);
s32 npWorkerThreadStart(u32 threadStackSize, s32 threadPriority);
s32 npWorkerThreadStop(void);
s32 npWorkerRegisterSockFd(s32 sockFd);
s32 npWorkerUnregisterSockFd(s32 sockFd);
s32 npWorkerWakeup(void);
s32 npWorkerIsCurrentThread(void);
s32 npWorkerPostEvent(u8 eventKind, SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                      SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, s32 errorCode);
s32 npWorkerCloseContextRooms(NpMatching2Context *ctx);
s32 npWorkerDispatchReqCb(NpMatching2Req *req, s32 errorCode, const void *eventData);
s32 npWorkerInvokeSignalingCb(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                              SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, s32 errorCode);
s32 npWorkerDispatchRoomEvent(s32 reqSource, NpMatching2Room *room, SceNpMatching2Event eventId, const void *eventData);
s32 npWorkerDispatchRoomErrorEvent(s32 reqSource, NpMatching2Room *room, s32 errorCode, u32 eventId, u32 extraLen);
s32 npWorkerEncodeRoomGroups(const NpMatching2GroupConfigInput *configs, u32 configCount, u8 *groupBitmapOut,
                             NpProtoGroupConfigWire **outGroupWires, u32 *outGroupWireCount, u64 *passwordMaskOut);
s32 npWorkerDecodeRoomGroups(const u8 *slotBitmap, const u8 *groupBitmap, const NpProtoGroupConfigWire *groupWires,
                             u32 groupWireCount, u64 passwordMask, SceNpMatching2RoomGroup **outGroups, u32 *outCount);
s32 npWorkerSyncRoomGroups(const NpProtoGroupConfigWire *groupWires, u32 groupWireCount, const u64 *passwordMask,
                           SceNpMatching2RoomGroup *groupList, u32 groupCount,
                           SceNpMatching2RoomGroup **updatedOut, u32 *updatedCountInOut);
s32 npWorkerBuildCastMask(NpMatching2Room *room, u8 castType, const void *targetUnion, u8 *maskOut);
s32 npWorkerConvertUserInfoList(NpProtoUserInfo *srcList, u16 count, SceNpMatching2UserInfoOut **outList);
s32 npWorkerFreeUserInfoList(SceNpMatching2UserInfoOut *list, u16 count);
s32 npWorkerOnAuthComplete(NpMatching2Context *ctx, NpMatching2Req *req, s32 resultCode);
s32 npWorkerGetLocalProfile(NpMatching2UserProfile **profileOut);
s32 npWorkerCancelConnReqs(NpMatching2Conn *conn, s32 errorCode);
s32 npWorkerDispatchRoomMsg(s32 reqSource, NpMatching2Room *room, SceNpMatching2RoomMemberId srcMemberId,
                            SceNpMatching2Event eventId, const void *eventData);
s32 npWorkerBuildPasswordMask(const SceNpMatching2RoomGroup *groups, u32 groupCount,
                              const NpMatching2PasswordConfigInput *configs, u32 configCount, u64 *maskOut);
s32 npWorkerFreeBinAttrArray(SceNpMatching2BinAttr *attrs, u32 count);
s32 npWorkerCountActiveSlots(const u8 *slotBitmap, u32 *countOut);
s32 npWorkerDecodeCastTarget(u8 castType, NpMatching2Room *room, const u8 *castMask, void *targetOut, s32 *allocatedOut);
void npWorkerHandleReqFailCleanup(NpMatching2Req *req, s32 errorCode);
s32 npWorkerReadBitField(const u8 *bitmap, u32 bitmapBytes, u32 fieldBits, u32 oneBasedIndex, u32 *valueOut);
s32 npWorkerExpandMemberBitmask(NpMatching2Room *room, const u8 *castMask, SceNpMatching2RoomMemberId **listOut, u32 *countOut);
s32 npSslInitGlobal(void);
s32 npSslOpenConn(NpMatching2SslCtx *sslCtx);
s32 npSslWrite(NpMatching2SslCtx *sslCtx, const void *buf, s32 len, s32 flags);
s32 npSslRead(NpMatching2SslCtx *sslCtx, void *buf, s32 len, s32 flags);
s32 npSslTermGlobal(void);

/* Signaling lifecycle bridge (matching2_worker.c -> matching2_signaling.c) */
s32 npSignalingBridgeInit(u32 stackSize, s32 threadPriority);
s32 npSignalingBridgeTerm(void);
s32 npSignalingBridgeAddContext(const SceNpId *npId, SceNpMatching2ContextId ctxId);
s32 npSignalingBridgeRemoveContext(SceNpMatching2ContextId ctxId);
s32 npSignalingBridgeStartContext(SceNpMatching2ContextId ctxId);
s32 npSignalingBridgeStopContext(SceNpMatching2ContextId ctxId);

/* matching2_signaling.c <-> matching2_peer.c */
extern s32 g_npSignalingInitialized;
s32 npSigInit(u32 stackSize, s32 priority);
s32 npSigTerm(void);
s32 npSigInitContextManager(void);
s32 npSigTermContextManager(void);
s32 npSigAddContext(const SceNpId *npId, SceNpMatching2ContextId ctxId);
s32 npSigRemoveContext(SceNpMatching2ContextId ctxId);
s32 npSigStartContext(SceNpMatching2ContextId ctxId);
s32 npSigStopContext(SceNpMatching2ContextId ctxId);
s32 npSigCreateSession(const SceNpId *npId, s32 (*cbFunc)(s32, u32, u16, s32, void *), void *cbArg, s32 *sessionIdOut, s32 poolKind);
s32 npSigDeleteSession(s32 sessionId);
s32 npSigGetLocalNetInfo(NpSigLocalNetInfo *infoOut);
NpSigGlobal *npSigGetGlobal(void);
s32 npSigSendPipeMsg(const NpSigPipeMsg *msg);
s32 npSigNotifyCb(NpSigSession *session, u32 connId, u16 eventId, s32 errorCode);
s32 npSigNotifyConnEvent(NpSigConn *conn, u16 eventId, s32 errorCode);
s32 npSigTimerTimeoutCb(u32 connId);
s32 npSigTimerRetryCb(u32 connId);
s32 npSigTimerPollCb(void);
s32 npSigTimerProbeCb(u32 connId);
s32 npSigTimerBwCb(u32 connId);
s32 npSigStunCb(s32 stunHandle, u32 userTag, s32 status, const void *addrInfo);
s32 npSigFindSessionByCtxId(NpSigGlobal *sig, SceNpMatching2ContextId ctxId, NpSigSession **outSession);
s32 npSigFindPeerRoomMember(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId,
                            SceNpMatching2ContextId *ctxIdOut, SceNpMatching2RoomId *roomIdOut,
                            SceNpMatching2RoomMemberId *memberIdOut, u32 filterConnId);
s32 npSigRegisterSession(NpSigGlobal *sig, NpSigSession *session, s32 *sessionIdOut, s32 poolKind);
s32 npSigUnregisterSession(NpSigGlobal *sig, s32 sessionId);
s32 npSigRegisterReq(NpSigGlobal *sig, NpSigReq *reqNode, u32 reqMask, u32 *reqIdOut);
s32 npSigUnregisterReq(NpSigGlobal *sig, u32 keyVal, s32 keyKind);
s32 npSigFindConnByNpId(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId, NpSigConn **outConn);
s32 npSigGetOrCreateConn(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId, NpSigConn **outConn);
s32 npSigReleaseConn(NpSigGlobal *sig, NpSigConn *conn);
s32 npSigFreeCandList(NpSigCand *candList);
s32 npSigCloseConn(NpSigGlobal *sig, NpSigConn *conn, s32 errorCode, s32 closeKind);
s32 npSigAttachSessionToConn(NpSigGlobal *sig, NpSigConn *conn, s32 sessionId);
s32 npSigDetachSessionFromConn(NpSigGlobal *sig, NpSigConn *conn, s32 sessionId);
s32 npSigFreeRefList(NpSigSessionRef *refList);
s32 npSigPopPeerNetInfoResult(NpSigGlobal *sig, s32 sessionId, u32 reqId, NpSigPeerNetInfoNode **outResult);
s32 npSigClearPeerNetInfoResults(NpSigGlobal *sig, s32 sessionId);
s32 npSigReadLocalNetInfo(NpSigLocalNetInfo *infoOut);
s32 npSigOpenSymmetricSocks(NpSigGlobal *sig, NpSigConn *conn);
s32 npSigCloseSymmetricSocks(NpSigGlobal *sig, NpSigConn *conn);
s32 npSigFindSessionUnlocked(NpSigGlobal *sig, s32 sessionId, NpSigSession **outSession);
s32 npSigFindSession(NpSigGlobal *sig, s32 sessionId, NpSigSession **outSession);
s32 npSigReleaseSession(NpSigGlobal *sig, NpSigSession *session);
s32 npSigFindReqById(NpSigGlobal *sig, u32 reqId, NpSigReq **outReq);
s32 npSigReleaseReq(NpSigGlobal *sig, NpSigReq *reqNode);
s32 npSigDeleteReqById(NpSigGlobal *sig, u32 reqId);
s32 npSigFindConnById(NpSigGlobal *sig, u32 connId, NpSigConn **outConn);
s32 npSigFindConnByIdAll(NpSigGlobal *sig, u32 connId, NpSigConn **outConn);
s32 npSigFindConnByTxId(NpSigGlobal *sig, u32 txId, NpSigConn **outConn);
NpSigCand *npSigFindCandByAddr(NpSigCand *candList, u32 ipAddr, u16 port);
NpSigSessionRef *npSigFindSessionRef(NpSigConn *conn, s32 sessionId);
s32 npSigPushPeerNetInfoResult(NpSigGlobal *sig, NpSigPeerNetInfoNode *resultNode);
s32 npSigGetLocalIpAddr(u32 *ipOut);
s32 npSigSendSimpleSignal(NpSigConn *conn, u32 msgType);
s32 npSigFindReqByKey(NpSigGlobal *sig, u32 keyVal, NpSigReq **outReq, s32 keyKind);
s32 npSigFindConnInternal(NpSigGlobal *sig, u32 keyVal, NpSigConn **outConn, s32 keyKind, s32 includeClosing);
s32 npSigSendSignalPacket(NpSigConn *conn, u32 msgType, const NpSigSignalPayload *payload);
s32 npSigStartPeerNetInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                          SceNpMatching2RoomMemberId memberId, u32 *reqIdOut);
void npSigRoomMsgListener(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                          SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, const void *msgInfo);
s32 npSigSendBandwidthProbe(NpSigGlobal *sig, NpSigConn *conn);
s32 npSigEchoThread(u32 args, void *argp);
s32 npSigDispatchEchoEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg);
s32 sceNpMatching2GetSignalingOptParamLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                            SceNpMatching2SignalingOptParam *optParamOut);

#endif /* NP_MATCHING2_INTERNAL_H */
