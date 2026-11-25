#include "np_matching2_internal.h"

NpMatching2Context *npMatching2FindContext(SceNpMatching2ContextId ctx_id)
{
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_CONTEXTS; idx++) {
        if (g_np_matching2.contexts[idx].in_use != 0 &&
            g_np_matching2.contexts[idx].ctx_id == ctx_id)
            return &g_np_matching2.contexts[idx];
    }
    return NULL;
}

NpMatching2Room *npMatching2FindRoom(NpMatching2Context *ctx, SceNpMatching2RoomId room_id)
{
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_ROOMS; idx++) {
        if (ctx->rooms[idx].in_use != 0 && ctx->rooms[idx].room_id == room_id)
            return &ctx->rooms[idx];
    }
    return NULL;
}

NpMatching2RoomMember *npMatching2FindMember(NpMatching2Room *room,
                                             SceNpMatching2RoomMemberId member_id)
{
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_MEMBERS; idx++) {
        if (room->members[idx].in_use != 0 && room->members[idx].member_id == member_id)
            return &room->members[idx];
    }
    return NULL;
}

int npMatching2QueueRequest(NpMatching2Context *ctx, u16 event_type,
                            const NpMatching2RequestOptParam *opt,
                            SceNpMatching2RequestId *out_req_id)
{
    if (ctx == NULL || out_req_id == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (ctx->started == 0)
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_STARTED;

    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_REQUESTS; idx++) {
        if (g_np_matching2.requests[idx].in_use == 0) {
            NpMatching2Request *req = &g_np_matching2.requests[idx];
            sceKernelMemset(req, 0, sizeof(*req));
            req->in_use = 1;
            req->event_type = event_type;
            req->ctx_id = ctx->ctx_id;
            req->req_id = g_np_matching2.next_req_id++;
            if (g_np_matching2.next_req_id == 0)
                g_np_matching2.next_req_id = 1;
            req->opt = (opt != NULL) ? *opt : ctx->default_req_opt;
            *out_req_id = req->req_id;
            return 0;
        }
    }
    return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

int npMatching2AbortRequestInternal(SceNpMatching2RequestId req_id)
{
    npMatching2Lock();
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_REQUESTS; idx++) {
        if (g_np_matching2.requests[idx].in_use != 0 &&
            g_np_matching2.requests[idx].req_id == req_id) {
            g_np_matching2.requests[idx].aborted = 1;
            g_np_matching2.requests[idx].in_use = 0;
            npMatching2Unlock();
            return 0;
        }
    }
    npMatching2Unlock();
    return (int)SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND;
}

static int npMatching2SubmitAsync(SceNpMatching2ContextId ctx_id, u16 event_type,
                                  const void *req_param, const NpMatching2RequestOptParam *opt,
                                  SceNpMatching2RequestId *out_req_id)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (req_param == NULL || out_req_id == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    int ret = (ctx != NULL) ? npMatching2QueueRequest(ctx, event_type, opt, out_req_id)
                            : (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    npMatching2Unlock();
    return ret;
}

/* ============================================================================
 * Server, World, User, and Room Requests
 * ============================================================================ */

int sceNpMatching2GetServerIdListLocal(SceNpMatching2ContextId ctx_id,
                                       SceNpMatching2ServerId *server_ids, u32 max_num)
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
    u32 count = ctx->server_count;
    if (server_ids != NULL && max_num > 0) {
        u32 copy_cnt = (count < max_num) ? count : max_num;
        sceKernelMemcpy(server_ids, ctx->server_ids, copy_cnt * sizeof(SceNpMatching2ServerId));
        count = copy_cnt;
    }
    npMatching2Unlock();
    return (int)count;
}

int sceNpMatching2GetServerInfo(SceNpMatching2ContextId ctx_id, const void *req_param,
                                const NpMatching2RequestOptParam *opt,
                                SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0001, req_param, opt, req_id);
}

int sceNpMatching2GetWorldInfoList(SceNpMatching2ContextId ctx_id, const void *req_param,
                                   const NpMatching2RequestOptParam *opt,
                                   SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0002, req_param, opt, req_id);
}

int sceNpMatching2SetUserInfo(SceNpMatching2ContextId ctx_id, const void *req_param,
                              const NpMatching2RequestOptParam *opt,
                              SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0003, req_param, opt, req_id);
}

int sceNpMatching2GetUserInfoList(SceNpMatching2ContextId ctx_id, const void *req_param,
                                  const NpMatching2RequestOptParam *opt,
                                  SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0004, req_param, opt, req_id);
}

int sceNpMatching2CreateJoinRoom(SceNpMatching2ContextId ctx_id, const void *req_param,
                                 const NpMatching2RequestOptParam *opt,
                                 SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0005, req_param, opt, req_id);
}

int sceNpMatching2JoinRoom(SceNpMatching2ContextId ctx_id, const void *req_param,
                           const NpMatching2RequestOptParam *opt,
                           SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0006, req_param, opt, req_id);
}

int sceNpMatching2LeaveRoom(SceNpMatching2ContextId ctx_id, const void *req_param,
                            const NpMatching2RequestOptParam *opt,
                            SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0007, req_param, opt, req_id);
}

int sceNpMatching2GrantRoomOwner(SceNpMatching2ContextId ctx_id, const void *req_param,
                                 const NpMatching2RequestOptParam *opt,
                                 SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0008, req_param, opt, req_id);
}

int sceNpMatching2KickoutRoomMember(SceNpMatching2ContextId ctx_id, const void *req_param,
                                    const NpMatching2RequestOptParam *opt,
                                    SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0009, req_param, opt, req_id);
}

int sceNpMatching2SearchRoom(SceNpMatching2ContextId ctx_id, const void *req_param,
                             const NpMatching2RequestOptParam *opt,
                             SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000A, req_param, opt, req_id);
}

int sceNpMatching2SendRoomMessage(SceNpMatching2ContextId ctx_id, const void *req_param,
                                  const NpMatching2RequestOptParam *opt,
                                  SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000B, req_param, opt, req_id);
}

int sceNpMatching2SendRoomChatMessage(SceNpMatching2ContextId ctx_id, const void *req_param,
                                      const NpMatching2RequestOptParam *opt,
                                      SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000C, req_param, opt, req_id);
}

int sceNpMatching2GetRoomDataExternalList(SceNpMatching2ContextId ctx_id, const void *req_param,
                                          const NpMatching2RequestOptParam *opt,
                                          SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000D, req_param, opt, req_id);
}

int sceNpMatching2SetRoomDataExternal(SceNpMatching2ContextId ctx_id, const void *req_param,
                                      const NpMatching2RequestOptParam *opt,
                                      SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000E, req_param, opt, req_id);
}

int sceNpMatching2GetRoomDataInternal(SceNpMatching2ContextId ctx_id, const void *req_param,
                                      const NpMatching2RequestOptParam *opt,
                                      SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x000F, req_param, opt, req_id);
}

int sceNpMatching2SetRoomDataInternal(SceNpMatching2ContextId ctx_id, const void *req_param,
                                      const NpMatching2RequestOptParam *opt,
                                      SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0010, req_param, opt, req_id);
}

int sceNpMatching2GetRoomMemberDataExternalList(SceNpMatching2ContextId ctx_id, const void *req_param,
                                                const NpMatching2RequestOptParam *opt,
                                                SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0011, req_param, opt, req_id);
}

int sceNpMatching2GetRoomMemberDataInternalList(SceNpMatching2ContextId ctx_id, const void *req_param,
                                                const NpMatching2RequestOptParam *opt,
                                                SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0012, req_param, opt, req_id);
}

int sceNpMatching2GetRoomMemberDataInternal(SceNpMatching2ContextId ctx_id, const void *req_param,
                                            const NpMatching2RequestOptParam *opt,
                                            SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0013, req_param, opt, req_id);
}

int sceNpMatching2SetRoomMemberDataInternal(SceNpMatching2ContextId ctx_id, const void *req_param,
                                            const NpMatching2RequestOptParam *opt,
                                            SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0014, req_param, opt, req_id);
}

int sceNpMatching2SetSignalingOptParam(SceNpMatching2ContextId ctx_id, const void *req_param,
                                       const NpMatching2RequestOptParam *opt,
                                       SceNpMatching2RequestId *req_id)
{
    return npMatching2SubmitAsync(ctx_id, 0x0015, req_param, opt, req_id);
}

/* ============================================================================
 * Local Room Cache Queries
 * ============================================================================ */

int sceNpMatching2GetRoomDataInternalLocal(SceNpMatching2ContextId ctx_id,
                                           SceNpMatching2RoomId room_id,
                                           void *buf, u32 buf_size)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || buf_size == 0)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    NpMatching2Room *room = npMatching2FindRoom(ctx, room_id);
    if (room == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND;
    }
    u32 copy_sz = room->bin_attr_internal_size < buf_size ? room->bin_attr_internal_size : buf_size;
    sceKernelMemcpy(buf, room->bin_attr_internal, copy_sz);
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2GetRoomMemberIdListLocal(SceNpMatching2ContextId ctx_id,
                                           SceNpMatching2RoomId room_id,
                                           SceNpMatching2RoomMemberId *member_ids, u32 max_num)
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
    NpMatching2Room *room = npMatching2FindRoom(ctx, room_id);
    if (room == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND;
    }

    u32 count = 0;
    int idx;
    for (idx = 0; idx < NP_MATCHING2_MAX_MEMBERS; idx++) {
        if (room->members[idx].in_use != 0) {
            if (member_ids != NULL && count < max_num)
                member_ids[count] = room->members[idx].member_id;
            count++;
        }
    }
    npMatching2Unlock();
    return (int)count;
}

int sceNpMatching2GetRoomMemberDataInternalLocal(SceNpMatching2ContextId ctx_id,
                                                 SceNpMatching2RoomId room_id,
                                                 SceNpMatching2RoomMemberId member_id,
                                                 void *buf, u32 buf_size)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (buf == NULL || buf_size == 0)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    NpMatching2Room *room = npMatching2FindRoom(ctx, room_id);
    if (room == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND;
    }
    NpMatching2RoomMember *mem = npMatching2FindMember(room, member_id);
    if (mem == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_MEMBER_NOT_FOUND;
    }
    u32 copy_sz = mem->bin_attr_size < buf_size ? mem->bin_attr_size : buf_size;
    sceKernelMemcpy(buf, mem->bin_attr, copy_sz);
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2GetRoomPasswordLocal(SceNpMatching2ContextId ctx_id,
                                       SceNpMatching2RoomId room_id,
                                       NpMatching2RoomPassword *password)
{
    if (_sceKernelCheckThreadStack() < NP_MATCHING2_MIN_STACK_FREE)
        return (int)SCE_NP_MATCHING2_ERROR_INSUFFICIENT_STACK;
    if (g_np_matching2.initialized == 0)
        return (int)SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    if (password == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    npMatching2Lock();
    NpMatching2Context *ctx = npMatching2FindContext(ctx_id);
    if (ctx == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_CONTEXT_NOT_FOUND;
    }
    NpMatching2Room *room = npMatching2FindRoom(ctx, room_id);
    if (room == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND;
    }
    *password = room->password;
    npMatching2Unlock();
    return 0;
}

int sceNpMatching2GetSignalingOptParamLocal(SceNpMatching2ContextId ctx_id,
                                            SceNpMatching2RoomId room_id,
                                            NpMatching2SignalingOptParam *opt)
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
    NpMatching2Room *room = npMatching2FindRoom(ctx, room_id);
    if (room == NULL) {
        npMatching2Unlock();
        return (int)SCE_NP_MATCHING2_ERROR_ROOM_NOT_FOUND;
    }
    *opt = room->sig_opt;
    npMatching2Unlock();
    return 0;
}
