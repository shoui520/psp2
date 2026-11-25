#include "np_matching2_internal.h"

NpMatching2Globals g_np_matching2;

void *npMatching2Alloc(u32 size)
{
    void *ptr = NULL;
    if (g_np_matching2.vpl_id <= 0 || size == 0)
        return NULL;
    if (sceKernelTryAllocateVpl(g_np_matching2.vpl_id, size, &ptr) < 0)
        return NULL;
    g_np_matching2.cur_used += size;
    if (g_np_matching2.cur_used > g_np_matching2.max_used)
        g_np_matching2.max_used = g_np_matching2.cur_used;
    return ptr;
}

void npMatching2Free(void *ptr)
{
    if (g_np_matching2.vpl_id > 0 && ptr != NULL)
        sceKernelFreeVpl(g_np_matching2.vpl_id, ptr);
}

void npMatching2Lock(void)
{
    if (g_np_matching2.lock_mutex > 0)
        sceKernelLockMutex(g_np_matching2.lock_mutex, 1, NULL);
}

void npMatching2Unlock(void)
{
    if (g_np_matching2.lock_mutex > 0)
        sceKernelUnlockMutex(g_np_matching2.lock_mutex, 1);
}

int sceNpMatching2Init(int thread_prio, int cpu_affinity, u32 pool_size, int sig_prio)
{
    (void)cpu_affinity;
    if (pool_size < NP_MATCHING2_MIN_POOL_SIZE)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_POOL_SIZE;
    if (_sceKernelCheckThreadStack() < 4064)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized != 0)
        return (int)SCE_NP_MATCHING2_ERROR_ALREADY_INITIALIZED;

    sceKernelMemset(&g_np_matching2, 0, sizeof(g_np_matching2));
    g_np_matching2.pool_size = pool_size;
    g_np_matching2.next_ctx_id = 1;
    g_np_matching2.next_req_id = 1;
    g_np_matching2.sig_udp_sock = -1;

    g_np_matching2.vpl_id = sceKernelCreateVpl("SceMatching2", 2, 0, pool_size, NULL);
    if (g_np_matching2.vpl_id < 0)
        return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    g_np_matching2.lock_mutex = sceKernelCreateMutex("SceNpMatching2Lock", 0x200, 0, NULL);
    if (g_np_matching2.lock_mutex < 0) {
        sceKernelDeleteVpl(g_np_matching2.vpl_id);
        g_np_matching2.vpl_id = 0;
        return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }

    int ret = npMatching2CalloutInit(thread_prio, 0x2000);
    if (ret < 0) {
        sceKernelDeleteMutex(g_np_matching2.lock_mutex);
        sceKernelDeleteVpl(g_np_matching2.vpl_id);
        return ret;
    }

    ret = npMatching2SignalingInit(0x2000, sig_prio);
    if (ret < 0) {
        npMatching2CalloutTerm();
        sceKernelDeleteMutex(g_np_matching2.lock_mutex);
        sceKernelDeleteVpl(g_np_matching2.vpl_id);
        return ret;
    }

    g_np_matching2.initialized = 1;
    return 0;
}

int sceNpMatching2Term(void)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    npMatching2CalloutTerm();
    npMatching2SignalingTerm();

    if (g_np_matching2.lock_mutex > 0) {
        sceKernelDeleteMutex(g_np_matching2.lock_mutex);
        g_np_matching2.lock_mutex = 0;
    }
    if (g_np_matching2.vpl_id > 0) {
        sceKernelDeleteVpl(g_np_matching2.vpl_id);
        g_np_matching2.vpl_id = 0;
    }

    g_np_matching2.initialized = 0;
    return 0;
}

int sceNpMatching2GetMemoryStat(NpMatching2MemoryStat *stat)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (stat == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    stat->pool_size = g_np_matching2.pool_size;
    stat->cur_used_size = g_np_matching2.cur_used;
    stat->max_used_size = g_np_matching2.max_used;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2CreateContext(const void *comm_id, const void *pass_phrase,
                                SceNpMatching2ContextId *ctx_id, u32 option)
{
    if (_sceKernelCheckThreadStack() < 4016)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (comm_id == NULL || pass_phrase == NULL || ctx_id == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    u8 np_id[48];
    sceKernelMemset(np_id, 0, sizeof(np_id));
    int ret = sceNpGetNpId(np_id);
    if (ret < 0)
        return ret;

    npMatching2Lock();
    int slot = -1;
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_CONTEXTS; idx++) {
        if (g_np_matching2.contexts[idx].in_use == 0) {
            slot = idx;
            break;
        }
    }
    if (slot < 0) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_MAX_CONTEXTS;
    }

    NpMatching2Context *ctx = &g_np_matching2.contexts[slot];
    sceKernelMemset(ctx, 0, sizeof(*ctx));
    ctx->in_use = 1;
    ctx->ctx_id = g_np_matching2.next_ctx_id++;
    if (g_np_matching2.next_ctx_id == 0)
        g_np_matching2.next_ctx_id = 1;
    sceKernelMemcpy(ctx->comm_id, comm_id, sizeof(ctx->comm_id));
    sceKernelMemcpy(ctx->pass_phrase, pass_phrase, sizeof(ctx->pass_phrase));
    sceKernelMemcpy(ctx->np_id, np_id, sizeof(ctx->np_id));
    ctx->option = option;
    *ctx_id = ctx->ctx_id;
    npMatching2Unlock();

    return npMatching2SignalingCreateContext(np_id, ctx->ctx_id);
}

int sceNpMatching2DestroyContext(SceNpMatching2ContextId ctx_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    npMatching2SignalingDestroyContext(ctx_id);
    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    sceKernelMemset(ctx, 0, sizeof(*ctx));
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SetDefaultRequestOptParam(SceNpMatching2ContextId ctx_id,
                                            const NpMatching2RequestOptParam *opt)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (opt == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    ctx->default_req_opt = *opt;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SetDefaultRoomEventOptParam(SceNpMatching2ContextId ctx_id,
                                              const NpMatching2RoomEventOptParam *opt)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (opt == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    ctx->default_room_ev_opt = *opt;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2SetDefaultRoomMessageOptParam(SceNpMatching2ContextId ctx_id,
                                                const NpMatching2RoomMessageOptParam *opt)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (opt == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    ctx->default_room_msg_opt = *opt;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2ContextStart(SceNpMatching2ContextId ctx_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    if (ctx->started != 0) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_ALREADY_STARTED;
    }
    ctx->started = 1;
    npMatching2Unlock();

    return npMatching2SignalingStartContext(ctx_id);
}

int sceNpMatching2ContextStop(SceNpMatching2ContextId ctx_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;

    npMatching2SignalingStopContext(ctx_id);
    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    ctx->started = 0;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2AbortRequest(SceNpMatching2RequestId req_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    return npMatching2AbortRequestInternal(req_id);
}
