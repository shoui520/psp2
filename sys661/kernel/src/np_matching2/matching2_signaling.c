#include "np_matching2_internal.h"

static int npSignalingMainThread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    u32 out_bits = 0;
    while (g_np_matching2.sig_initialized != 0) {
        u32 timeout = 100000U;
        sceKernelWaitEventFlag(g_np_matching2.sig_evf, 0x01U, 0x11U, &out_bits, &timeout);
        if ((out_bits & 0x02U) != 0)
            break;
    }
    return 0;
}

int npMatching2SignalingInit(u32 stack_size, int prio)
{
    if (g_np_matching2.sig_initialized != 0)
        return 0;

    g_np_matching2.sig_evf = sceKernelCreateEventFlag("SceNpSignaling", 0, 0, NULL);
    if (g_np_matching2.sig_evf < 0)
        return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    g_np_matching2.sig_initialized = 1;
    g_np_matching2.local_net_info.size = sizeof(NpSignalingNetInfo);
    g_np_matching2.local_net_info.nat_type = 2;

    int eff_prio = (prio > 0) ? prio : 40;
    g_np_matching2.sig_thread = sceKernelCreateThread(
        "SceNpSignalingMain", (void *)npSignalingMainThread, eff_prio, stack_size, 0, NULL);
    if (g_np_matching2.sig_thread >= 0)
        sceKernelStartThread(g_np_matching2.sig_thread, 0, NULL);

    return 0;
}

int npMatching2SignalingTerm(void)
{
    if (g_np_matching2.sig_initialized == 0)
        return 0;

    g_np_matching2.sig_initialized = 0;
    if (g_np_matching2.sig_evf > 0)
        sceKernelSetEventFlag(g_np_matching2.sig_evf, 0x03U);
    if (g_np_matching2.sig_thread > 0) {
        u32 timeout = 500000U;
        sceKernelWaitThreadEnd(g_np_matching2.sig_thread, &timeout);
        sceKernelDeleteThread(g_np_matching2.sig_thread);
        g_np_matching2.sig_thread = 0;
    }
    if (g_np_matching2.sig_evf > 0) {
        sceKernelDeleteEventFlag(g_np_matching2.sig_evf);
        g_np_matching2.sig_evf = 0;
    }
    if (g_np_matching2.sig_udp_sock >= 0) {
        sceNetInetClose(g_np_matching2.sig_udp_sock);
        g_np_matching2.sig_udp_sock = -1;
    }
    return 0;
}

int npMatching2SignalingCreateContext(const void *np_id, SceNpMatching2ContextId ctx_id)
{
    (void)np_id;
    (void)ctx_id;
    return 0;
}

int npMatching2SignalingDestroyContext(SceNpMatching2ContextId ctx_id)
{
    npMatching2Lock();
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_PEERS; idx++) {
        if (g_np_matching2.conns[idx].in_use != 0 &&
            g_np_matching2.conns[idx].ctx_id == ctx_id)
            sceKernelMemset(&g_np_matching2.conns[idx], 0, sizeof(NpSignalingConn));
    }
    npMatching2Unlock();
    return 0;
}

int npMatching2SignalingStartContext(SceNpMatching2ContextId ctx_id)
{
    (void)ctx_id;
    if (g_np_matching2.sig_udp_sock < 0)
        g_np_matching2.sig_udp_sock = sceNetInetSocket(2, 2, 0);
    return 0;
}

int npMatching2SignalingStopContext(SceNpMatching2ContextId ctx_id)
{
    return npMatching2SignalingDestroyContext(ctx_id);
}

static NpSignalingConn *npSignalingFindConn(SceNpMatching2ContextId ctx_id,
                                            SceNpMatching2RoomId room_id,
                                            SceNpMatching2RoomMemberId peer_id)
{
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_PEERS; idx++) {
        NpSignalingConn *c = &g_np_matching2.conns[idx];
        if (c->in_use != 0 && c->ctx_id == ctx_id &&
            c->room_id == room_id && c->peer_member_id == peer_id)
            return c;
    }
    return NULL;
}

int sceNpMatching2RegisterSignalingCallback(SceNpMatching2ContextId ctx_id,
                                            SceNpMatching2SignalingCallback cb_func, void *cb_arg)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    ctx->sig_cb = cb_func;
    ctx->sig_cb_arg = cb_arg;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SignalingGetConnectionStatus(SceNpMatching2ContextId ctx_id,
                                               SceNpMatching2RoomId room_id,
                                               SceNpMatching2RoomMemberId peer_id,
                                               u32 *conn_status, u32 *peer_addr, u16 *peer_port)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (conn_status == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpSignalingConn *conn = npSignalingFindConn(ctx_id, room_id, peer_id);
    if (conn != NULL) {
        *conn_status = conn->status;
        if (peer_addr != NULL) *peer_addr = conn->peer_addr;
        if (peer_port != NULL) *peer_port = conn->peer_port;
    } else {
        *conn_status = 0;
        if (peer_addr != NULL) *peer_addr = 0;
        if (peer_port != NULL) *peer_port = 0;
    }
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SignalingGetConnectionInfo(SceNpMatching2ContextId ctx_id,
                                             SceNpMatching2RoomId room_id,
                                             SceNpMatching2RoomMemberId peer_id,
                                             int code, u32 *out_info)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (out_info == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpSignalingConn *conn = npSignalingFindConn(ctx_id, room_id, peer_id);
    if (conn == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_MEMBER_NOT_FOUND;
    }
    if (code == 1)      *out_info = conn->rtt_ms;
    else if (code == 2) *out_info = conn->packet_loss;
    else if (code == 3) *out_info = conn->nat_type;
    else                *out_info = conn->mapped_addr;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SignalingGetLocalNetInfo(NpSignalingNetInfo *info)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (info == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    *info = g_np_matching2.local_net_info;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SignalingGetPeerNetInfo(SceNpMatching2ContextId ctx_id,
                                          const void *np_id, SceNpMatching2RequestId *req_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (np_id == NULL || req_id == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    int ret = (ctx != NULL) ? npMatching2QueueRequest(ctx, 0x0020, NULL, req_id)
                            : (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    npMatching2Unlock();
    return ret;
}

int sceNpMatching2SignalingCancelPeerNetInfo(SceNpMatching2ContextId ctx_id,
                                             SceNpMatching2RequestId req_id)
{
    (void)ctx_id;
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return npMatching2AbortRequestInternal(req_id);
}

int sceNpMatching2SignalingGetPeerNetInfoResult(SceNpMatching2ContextId ctx_id,
                                                SceNpMatching2RequestId req_id,
                                                NpSignalingNetInfo *info)
{
    (void)ctx_id;
    (void)req_id;
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_SIGNALING_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (info == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    sceKernelMemset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    return 0;
}
