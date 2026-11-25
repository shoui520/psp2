#ifndef NP_MATCHING2_INTERNAL_H
#define NP_MATCHING2_INTERNAL_H

#include <stddef.h>
#include "common_header.h"

/* sceNpMatching2 error codes (facility 0x055) */
#define SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK       0x80550004
#define SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK       0x80550C04
#define SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED      0x80551801
#define SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED          0x80551802
#define SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY            0x80551803
#define SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT         0x80551804
#define SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND        0x80551805
#define SCE_NP_MATCHING2_ERROR_CONTEXT_ALREADY_STARTED  0x80551806
#define SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED      0x80551807
#define SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND        0x80551808
#define SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND           0x80551809
#define SCE_NP_MATCHING2_ERROR_MEMBER_NOT_FOUND         0x8055180A
#define SCE_NP_MATCHING2_ERROR_SERVER_NOT_FOUND         0x8055180B
#define SCE_NP_MATCHING2_ERROR_MAX_CONTEXTS             0x8055180C
#define SCE_NP_MATCHING2_ERROR_INVALID_POOL_SIZE        0x805518CB

#define NP_MATCHING2_MIN_POOL_SIZE      0x3800U
#define NP_MATCHING2_MIN_STACK_FREE     4080
#define NP_MATCHING2_MAX_CONTEXTS       4
#define NP_MATCHING2_MAX_SERVERS        16
#define NP_MATCHING2_MAX_ROOMS          4
#define NP_MATCHING2_MAX_MEMBERS        32
#define NP_MATCHING2_MAX_REQUESTS       32
#define NP_MATCHING2_MAX_PEERS          16

typedef u16 SceNpMatching2ContextId;
typedef u16 SceNpMatching2ServerId;
typedef u32 SceNpMatching2WorldId;
typedef u64 SceNpMatching2RoomId;
typedef u16 SceNpMatching2RoomMemberId;
typedef u32 SceNpMatching2RequestId;

typedef void (*SceNpMatching2RequestCallback)(
    SceNpMatching2ContextId ctx_id, SceNpMatching2RequestId req_id,
    u16 event, s32 error_code, const void *data, void *arg);

typedef void (*SceNpMatching2RoomEventCallback)(
    SceNpMatching2ContextId ctx_id, SceNpMatching2RoomId room_id,
    u16 event, const void *data, void *arg);

typedef void (*SceNpMatching2RoomMessageCallback)(
    SceNpMatching2ContextId ctx_id, SceNpMatching2RoomId room_id,
    SceNpMatching2RoomMemberId src_member_id, u16 event, const void *data, void *arg);

typedef void (*SceNpMatching2SignalingCallback)(
    SceNpMatching2ContextId ctx_id, SceNpMatching2RoomId room_id,
    SceNpMatching2RoomMemberId peer_member_id, u16 event, s32 error_code, void *arg);

typedef struct NpMatching2MemoryStat {
    u32 pool_size;
    u32 max_used_size;
    u32 cur_used_size;
} NpMatching2MemoryStat;

typedef struct NpMatching2RequestOptParam {
    SceNpMatching2RequestCallback cb_func;
    void *cb_arg;
    u32 timeout;
    u16 app_req_id;
    u16 reserved;
} NpMatching2RequestOptParam;

typedef struct NpMatching2RoomEventOptParam {
    SceNpMatching2RoomEventCallback cb_func;
    void *cb_arg;
} NpMatching2RoomEventOptParam;

typedef struct NpMatching2RoomMessageOptParam {
    SceNpMatching2RoomMessageCallback cb_func;
    void *cb_arg;
} NpMatching2RoomMessageOptParam;

typedef struct NpMatching2SignalingOptParam {
    u8 type;
    u8 flag;
    u16 hub_member_id;
    u32 reserved[2];
} NpMatching2SignalingOptParam;

typedef struct NpMatching2RoomPassword {
    u8 data[8];
} NpMatching2RoomPassword;

typedef struct NpMatching2RoomMember {
    u8 in_use;
    u8 is_owner;
    SceNpMatching2RoomMemberId member_id;
    u8 np_id[48];
    u8 nat_type;
    u8 reserved[3];
    u32 join_date_lo;
    u32 join_date_hi;
    u8 bin_attr[64];
    u32 bin_attr_size;
} NpMatching2RoomMember;

typedef struct NpMatching2Room {
    u8 in_use;
    u8 has_password;
    u16 max_slots;
    SceNpMatching2RoomId room_id;
    SceNpMatching2ServerId server_id;
    u16 world_id_lo;
    SceNpMatching2WorldId world_id;
    SceNpMatching2RoomMemberId owner_member_id;
    SceNpMatching2RoomMemberId self_member_id;
    NpMatching2RoomPassword password;
    NpMatching2SignalingOptParam sig_opt;
    u32 flag_attr;
    u8 bin_attr_internal[64];
    u32 bin_attr_internal_size;
    u8 bin_attr_external[64];
    u32 bin_attr_external_size;
    u32 member_count;
    NpMatching2RoomMember members[NP_MATCHING2_MAX_MEMBERS];
} NpMatching2Room;

typedef struct NpMatching2Request {
    u8 in_use;
    u8 aborted;
    u16 event_type;
    SceNpMatching2RequestId req_id;
    SceNpMatching2ContextId ctx_id;
    NpMatching2RequestOptParam opt;
} NpMatching2Request;

typedef struct NpMatching2Context {
    u8 in_use;
    u8 started;
    SceNpMatching2ContextId ctx_id;
    u8 comm_id[12];
    u8 pass_phrase[128];
    u8 np_id[48];
    u32 option;
    NpMatching2RequestOptParam default_req_opt;
    NpMatching2RoomEventOptParam default_room_ev_opt;
    NpMatching2RoomMessageOptParam default_room_msg_opt;
    SceNpMatching2SignalingCallback sig_cb;
    void *sig_cb_arg;
    u32 server_count;
    SceNpMatching2ServerId server_ids[NP_MATCHING2_MAX_SERVERS];
    NpMatching2Room rooms[NP_MATCHING2_MAX_ROOMS];
} NpMatching2Context;

typedef struct NpSignalingConn {
    u8 in_use;
    u8 status;
    SceNpMatching2ContextId ctx_id;
    SceNpMatching2RoomId room_id;
    SceNpMatching2RoomMemberId peer_member_id;
    u16 peer_port;
    u32 peer_addr;
    u32 mapped_addr;
    u16 mapped_port;
    u16 rtt_ms;
    u32 nat_type;
    u32 packet_loss;
} NpSignalingConn;

typedef struct NpSignalingNetInfo {
    u32 size;
    u32 nat_type;
    u32 local_addr;
    u32 mapped_addr;
    u32 upnp_status;
    u32 stun_status;
} NpSignalingNetInfo;

typedef struct NpMatching2Globals {
    u32 initialized;
    SceUID vpl_id;
    SceUID lock_mutex;
    SceUID callout_thread;
    SceUID callout_mbx;
    SceUID sig_thread;
    SceUID sig_evf;
    u32 pool_size;
    u32 cur_used;
    u32 max_used;
    u32 next_req_id;
    u16 next_ctx_id;
    u16 sig_initialized;
    int sig_udp_sock;
    NpSignalingNetInfo local_net_info;
    NpMatching2Context contexts[NP_MATCHING2_MAX_CONTEXTS];
    NpMatching2Request requests[NP_MATCHING2_MAX_REQUESTS];
    NpSignalingConn conns[NP_MATCHING2_MAX_PEERS];
} NpMatching2Globals;

extern NpMatching2Globals g_np_matching2;

/* Imported kernel / network / NP / SSL functions */
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern void *sceKernelMemset(void *dst, int val, u32 len);
extern SceUID sceKernelCreateVpl(const char *name, int part, int attr, u32 size, void *opt);
extern int sceKernelDeleteVpl(SceUID uid);
extern int sceKernelTryAllocateVpl(SceUID uid, u32 size, void **data);
extern int sceKernelFreeVpl(SceUID uid, void *data);
extern int sceKernelReferVplStatus(SceUID uid, void *info);
extern SceUID sceKernelCreateMutex(const char *name, u32 attr, s32 init_count, void *opt);
extern int sceKernelDeleteMutex(SceUID uid);
extern int sceKernelLockMutex(SceUID uid, s32 count, u32 *timeout);
extern int sceKernelUnlockMutex(SceUID uid, s32 count);
extern SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID uid);
extern int sceKernelSetEventFlag(SceUID uid, u32 bits);
extern int sceKernelWaitEventFlag(SceUID uid, u32 bits, u32 wait, u32 *out_bits, u32 *timeout);
extern SceUID sceKernelCreateMbx(const char *name, u32 attr, void *opt);
extern int sceKernelDeleteMbx(SceUID uid);
extern int sceKernelSendMbx(SceUID uid, void *msg);
extern int sceKernelPollMbx(SceUID uid, void **msg);
extern SceUID sceKernelCreateThread(const char *name, void *entry, s32 prio, u32 stack, u32 attr, void *opt);
extern int sceKernelStartThread(SceUID thid, u32 arglen, void *argp);
extern int sceKernelTerminateThread(SceUID thid);
extern int sceKernelWaitThreadEnd(SceUID thid, u32 *timeout);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelDelayThread(u32 usec);
extern u32 sceKernelGetSystemTimeLow(void);
extern int sceNpGetNpId(void *np_id);
extern int sceNpGetUserProfile(void *profile);
extern int sceNetApctlGetInfo(int code, void *info);
extern int sceNetInetSocket(int domain, int type, int protocol);
extern int sceNetInetBind(int sock, const void *addr, u32 addrlen);
extern int sceNetInetConnect(int sock, const void *addr, u32 addrlen);
extern int sceNetInetSend(int sock, const void *buf, u32 len, int flags);
extern int sceNetInetRecv(int sock, void *buf, u32 len, int flags);
extern int sceNetInetSendto(int sock, const void *buf, u32 len, int flags, const void *to, u32 tolen);
extern int sceNetInetRecvfrom(int sock, void *buf, u32 len, int flags, void *from, u32 *fromlen);
extern int sceNetInetClose(int sock);
extern int sceNetInetSetsockopt(int sock, int level, int optname, const void *optval, u32 optlen);
extern int sceNetInetGetsockopt(int sock, int level, int optname, void *optval, u32 *optlen);
extern int sceNetInetGetsockname(int sock, void *name, u32 *namelen);
extern int sceNetInetSelect(int nfds, void *rfds, void *wfds, void *efds, void *tv);
extern int sceNetInetGetPspError(void);
extern int sceNetResolverCreate(int *rid, void *buf, u32 buflen);
extern int sceNetResolverDelete(int rid);
extern int sceNetResolverStartNtoAAsync(int rid, const char *hostname, void *addr, u32 timeout, int retry);
extern int sceNetResolverPollAsync(int rid, int *ret);
extern int sceNetResolverStop(int rid);
extern int sceOpenPSIDGetOpenPSID(void *psid);

/* Internal helpers */
void *npMatching2Alloc(u32 size);
void npMatching2Free(void *ptr);
void npMatching2Lock(void);
void npMatching2Unlock(void);
NpMatching2Context *npMatching2FindContext(SceNpMatching2ContextId ctx_id);
NpMatching2Room *npMatching2FindRoom(NpMatching2Context *ctx, SceNpMatching2RoomId room_id);
NpMatching2RoomMember *npMatching2FindMember(NpMatching2Room *room, SceNpMatching2RoomMemberId member_id);
int npMatching2QueueRequest(NpMatching2Context *ctx, u16 event_type,
                            const NpMatching2RequestOptParam *opt, SceNpMatching2RequestId *out_req_id);
int npMatching2AbortRequestInternal(SceNpMatching2RequestId req_id);
int npMatching2SignalingInit(u32 stack_size, int prio);
int npMatching2SignalingTerm(void);
int npMatching2SignalingCreateContext(const void *np_id, SceNpMatching2ContextId ctx_id);
int npMatching2SignalingDestroyContext(SceNpMatching2ContextId ctx_id);
int npMatching2SignalingStartContext(SceNpMatching2ContextId ctx_id);
int npMatching2SignalingStopContext(SceNpMatching2ContextId ctx_id);
int npMatching2CalloutInit(int thread_prio, u32 stack_size);
int npMatching2CalloutTerm(void);
int npMatching2TlsConnect(const char *host, u16 port, int *out_sock);

#endif /* NP_MATCHING2_INTERNAL_H */
