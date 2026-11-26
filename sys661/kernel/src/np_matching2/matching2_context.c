#include "np_matching2_internal.h"

typedef struct NpMatching2CtxTable {
    u32 count;
    NpMatching2Context *head;
    NpMatching2Context *slots[7];
} NpMatching2CtxTable;

static NpMatching2CtxTable g_ctxTable;
static s32 g_lockInitialized;
static SceLwMutexWorkarea g_npLock;
NpMatching2AttrLimits g_npMatching2AttrLimits;

static inline s32 npIsRoomIdInvalid(SceNpMatching2RoomId roomId)
{
    if (roomId == 0)
        return 1;
    if ((u16)(roomId >> 48) == 0)
        return 1;
    if ((u16)(roomId >> 32) == 0)
        return 1;
    if ((u16)(roomId & 0xFFFFu) == 0)
        return 1;
    return 0;
}

static inline s32 npIsMemberIdInvalid(u16 memberId)
{
    if (((u32)(memberId >> 4) - 1u) >= 0x40u)
        return 1;
    if ((memberId & 0xFu) == 0)
        return 1;
    return 0;
}

/* 0x002058 */
NP_MATCHING2_NOINLINE s32 npValidateCreateJoinRoomReq(const SceNpMatching2CreateJoinRoomRequest *req)
{
    u32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if ((req->maxSlot - 1u) >= 0x40u)
        return SCE_NP_MATCHING2_ERROR_INVALID_MAX_SLOTS;
    if (req->worldId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    if (req->roomBinAttrInternalNum >= 3u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomBinAttrInternal != NULL) {
        for (idx = 0; idx < req->roomBinAttrInternalNum; idx++) {
            u16 attrId = req->roomBinAttrInternal[idx].id;
            if ((u16)(attrId - 87u) >= 2u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
            if (req->roomBinAttrInternal[idx].size >= 0x101u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN;
        }
    }

    if (req->roomSearchableIntAttrExternalNum >= 9u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomSearchableIntAttrExternal != NULL) {
        for (idx = 0; idx < req->roomSearchableIntAttrExternalNum; idx++) {
            u16 attrId = req->roomSearchableIntAttrExternal[idx].id;
            if ((u16)(attrId - 76u) >= 8u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
        }
    }

    if (req->roomSearchableBinAttrExternalNum >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomSearchableBinAttrExternal != NULL) {
        for (idx = 0; idx < req->roomSearchableBinAttrExternalNum; idx++) {
            if (req->roomSearchableBinAttrExternal[idx].id != 84u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
            if (req->roomSearchableBinAttrExternal[idx].size >= 0x41u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN;
        }
    }

    if (req->roomBinAttrExternalNum >= 3u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomBinAttrExternal != NULL) {
        for (idx = 0; idx < req->roomBinAttrExternalNum; idx++) {
            u16 attrId = req->roomBinAttrExternal[idx].id;
            if ((u16)(attrId - 85u) >= 2u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
            if (req->roomBinAttrExternal[idx].size >= 0x101u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN;
        }
    }

    if (req->roomMemberBinAttrInternalNum >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomMemberBinAttrInternal != NULL) {
        if (req->roomMemberBinAttrInternal->size >= 0x41u)
            return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN;
        if (req->roomMemberBinAttrInternal->id != 89u)
            return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
    }

    if (req->allowedUserNum >= 0x65u || req->blockedUserNum >= 0x65u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    return 0;
}

/* 0x0022c0 */
NP_MATCHING2_NOINLINE s32 npValidateGetServerInfoReq(const SceNpMatching2GetServerInfoRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->serverId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_SERVER_SPEC;
    return 0;
}

/* 0x0022e4 */
NP_MATCHING2_NOINLINE s32 npValidateGetWorldInfoListReq(const SceNpMatching2GetWorldInfoListRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->serverId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_SERVER_SPEC;
    return 0;
}

/* 0x002308 */
NP_MATCHING2_NOINLINE s32 npValidateSetUserInfoReq(const SceNpMatching2SetUserInfoRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->serverId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_SERVER_SPEC;
    return 0;
}

/* 0x00232c */
NP_MATCHING2_NOINLINE s32 npValidateGetUserInfoListReq(const SceNpMatching2GetUserInfoListRequest *req)
{
    s32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->serverId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_SERVER_SPEC;
    if (req->npId == NULL || req->npIdNum == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    if (req->attrId != NULL) {
        for (idx = 0; idx < (s32)req->attrIdNum; idx++) {
            if (req->attrId[idx] != 95u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
        }
    }
    return 0;
}

/* 0x0023b8 */
NP_MATCHING2_NOINLINE s32 npValidateSearchRoomReq(const SceNpMatching2SearchRoomRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->worldId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_WORLD_ID;
    if (req->maxNum >= 0x15u)
        return SCE_NP_MATCHING2_ERROR_INVALID_RANGE_MAX;
    return 0;
}

/* 0x0023f0 */
NP_MATCHING2_NOINLINE s32 npValidateJoinRoomReq(const SceNpMatching2JoinRoomRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (req->roomMemberBinAttrInternalNum >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomMemberBinAttrInternal != NULL) {
        if (req->roomMemberBinAttrInternal->size >= 0x41u)
            return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_LEN;
        if (req->roomMemberBinAttrInternal->id != 89u)
            return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
    }
    return 0;
}

/* 0x0024a4 */
NP_MATCHING2_NOINLINE s32 npValidateLeaveRoomReq(const SceNpMatching2LeaveRoomRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x00250c */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomDataInternalReq(const SceNpMatching2GetRoomDataInternalRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x002574 */
NP_MATCHING2_NOINLINE s32 npValidateSetRoomDataInternalReq(const SceNpMatching2SetRoomDataInternalRequest *req)
{
    s32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (req->roomBinAttrInternal != NULL) {
        for (idx = 0; idx < (s32)req->roomBinAttrInternalNum; idx++) {
            u16 attrId = req->roomBinAttrInternal[idx].id;
            if ((u16)(attrId - 87u) >= 2u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
        }
    }
    return 0;
}

/* 0x00262c */
NP_MATCHING2_NOINLINE s32 npValidateSetSignalingOptParamReq(const SceNpMatching2SetSignalingOptParamRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x002694 */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomDataExternalListReq(const SceNpMatching2GetRoomDataExternalListRequest *req)
{
    s32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomId == NULL || req->roomIdNum == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (req->roomIdNum >= 0x15u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    for (idx = 0; idx < (s32)req->roomIdNum; idx++) {
        if (npIsRoomIdInvalid(req->roomId[idx]))
            return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    }
    return 0;
}

/* 0x00274c */
NP_MATCHING2_NOINLINE s32 npValidateSetRoomDataExternalReq(const SceNpMatching2SetRoomDataExternalRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x0027b4 */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomMemberDataInternalReq(const SceNpMatching2GetRoomMemberDataInternalRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (npIsMemberIdInvalid(req->memberId))
        return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    return 0;
}

/* 0x002844 */
NP_MATCHING2_NOINLINE s32 npValidateSetRoomMemberDataInternalReq(const SceNpMatching2SetRoomMemberDataInternalRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (req->teamId == 0 && (req->roomMemberBinAttrInternal == NULL || req->roomMemberBinAttrInternalNum == 0))
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->roomMemberBinAttrInternal != NULL && req->roomMemberBinAttrInternal->id != 89u)
        return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
    return 0;
}

/* 0x0028f4 */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomMemberDataInternalListReq(const SceNpMatching2GetRoomMemberDataInternalListRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (req->sortMethod >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_SORT_METHOD;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x002970 */
NP_MATCHING2_NOINLINE s32 npValidateRoomIdAndSortMethod(SceNpMatching2RoomId roomId, u32 sortMethod)
{
    if (npIsRoomIdInvalid(roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (sortMethod >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_SORT_METHOD;
    return 0;
}

/* 0x0029d0 */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomMemberDataInternalLocal(SceNpMatching2RoomId roomId, SceNpMatching2RoomMemberId memberId,
                                                                   const SceNpMatching2AttributeId *attrId, s32 attrIdNum)
{
    s32 idx;

    if (npIsRoomIdInvalid(roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (memberId == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    if (attrId != NULL) {
        for (idx = 0; idx < attrIdNum; idx++) {
            if (attrId[idx] != 89u)
                return SCE_NP_MATCHING2_ERROR_INVALID_ATTR_ID;
        }
    }
    return 0;
}

/* 0x002a74 */
NP_MATCHING2_NOINLINE s32 npValidateGetRoomMemberDataExternalListReq(const SceNpMatching2GetRoomMemberDataExternalListRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    return 0;
}

/* 0x002adc */
NP_MATCHING2_NOINLINE s32 npValidateKickoutRoomMemberReq(const SceNpMatching2KickoutRoomMemberRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (npIsMemberIdInvalid(req->target))
        return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    if (req->blockFlag >= 2u)
        return SCE_NP_MATCHING2_ERROR_INVALID_BLOCK_FLAG;
    return 0;
}

/* 0x002b80 */
NP_MATCHING2_NOINLINE s32 npValidateGrantRoomOwnerReq(const SceNpMatching2GrantRoomOwnerRequest *req)
{
    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (npIsMemberIdInvalid(req->newOwner))
        return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    return 0;
}

/* 0x002c10 */
NP_MATCHING2_NOINLINE s32 npValidateSendRoomChatMessageReq(const SceNpMatching2SendRoomChatMessageRequest *req)
{
    s32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if ((u8)(req->castType - 1u) >= 4u)
        return SCE_NP_MATCHING2_ERROR_INVALID_CAST_TYPE;

    if (req->castType == 2u) {
        if (npIsMemberIdInvalid(req->target.unicastTarget))
            return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    }
    if (req->castType == 3u) {
        if (req->target.multicastTarget.memberId == NULL || req->target.multicastTarget.memberIdNum == 0)
            return SCE_NP_MATCHING2_ERROR_INVALID_TARGET_LIST;
        for (idx = 0; idx < (s32)req->target.multicastTarget.memberIdNum; idx++) {
            if (npIsMemberIdInvalid(req->target.multicastTarget.memberId[idx]))
                return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
        }
    }

    if (req->msg == NULL || req->msgLen == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return 0;
}

/* 0x002d70 */
NP_MATCHING2_NOINLINE s32 npValidateSendRoomMessageReq(const SceNpMatching2SendRoomMessageRequest *req)
{
    s32 idx;

    if (req == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    if (npIsRoomIdInvalid(req->roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if ((u8)(req->castType - 1u) >= 4u)
        return SCE_NP_MATCHING2_ERROR_INVALID_CAST_TYPE;

    if (req->castType == 2u) {
        if (npIsMemberIdInvalid(req->target.unicastTarget))
            return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
    }
    if (req->castType == 3u) {
        if (req->target.multicastTarget.memberId == NULL || req->target.multicastTarget.memberIdNum == 0)
            return SCE_NP_MATCHING2_ERROR_INVALID_TARGET_LIST;
        for (idx = 0; idx < (s32)req->target.multicastTarget.memberIdNum; idx++) {
            if (npIsMemberIdInvalid(req->target.multicastTarget.memberId[idx]))
                return SCE_NP_MATCHING2_ERROR_INVALID_MEMBER_ID;
        }
    }

    if (req->msg == NULL || req->msgLen == 0)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return 0;
}

/* 0x002ed0 */
NP_MATCHING2_NOINLINE s32 npValidateGetSignalingOptParamLocal(SceNpMatching2RoomId roomId, const void *outParam)
{
    if (npIsRoomIdInvalid(roomId))
        return SCE_NP_MATCHING2_ERROR_INVALID_ROOM_ID;
    if (outParam == NULL)
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    return 0;
}

/* 0x002f2c */
NP_MATCHING2_NOINLINE s32 npCtxUnlinkAndFree(NpMatching2Context *ctx)
{
    NpMatching2Context *curr;
    NpMatching2Context *prev = NULL;

    npLockAcquire();
    curr = g_ctxTable.head;
    while (curr != NULL && curr != ctx) {
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL) {
        if (prev != NULL)
            prev->next = ctx->next;
        else
            g_ctxTable.head = ctx->next;
        g_ctxTable.slots[ctx->ctxId - 1u] = NULL;
        g_ctxTable.count--;
        npLockRelease();
        npWorkerCleanupContext(ctx);
        npMatching2Free(ctx);
        return 0;
    }
    npLockRelease();
    return SCE_NP_MATCHING2_ERROR_CTX_NOT_FOUND;
}

/* 0x002fec */
NP_MATCHING2_NOINLINE s32 npCtxTableInit(void)
{
    sceKernelMemset(&g_ctxTable, 0, sizeof(g_ctxTable));
    return 0;
}

/* 0x003018 */
NP_MATCHING2_NOINLINE void npCtxTableTerm(void)
{
}

/* 0x003020 */
NP_MATCHING2_NOINLINE s32 npCtxCheckCanCreate(const SceNpCommId *commId)
{
    s32 ret;
    NpMatching2Context *curr;

    npLockAcquire();
    if (g_ctxTable.count >= 7u) {
        ret = SCE_NP_MATCHING2_ERROR_CTX_MAX;
    } else {
        ret = 0;
        curr = g_ctxTable.head;
        while (curr != NULL) {
            if (sceNet_lib_E0A81C7C(&curr->commId, commId, sizeof(SceNpCommId)) == 0) {
                ret = SCE_NP_MATCHING2_ERROR_CTX_ALREADY_EXISTS;
                break;
            }
            curr = curr->next;
        }
    }
    npLockRelease();
    return ret;
}

/* 0x0030b4 */
NP_MATCHING2_NOINLINE s32 npCtxCreate(const SceNpCommId *commId, const SceNpCommPassphrase *passPhrase,
                                      u32 option, s32 legacyMode, NpMatching2Context **outCtx)
{
    NpMatching2Context *ctx;
    s32 idx;

    ctx = (NpMatching2Context *)npMatching2Alloc(sizeof(NpMatching2Context));
    if (ctx == NULL)
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    sceKernelMemset(ctx, 0, sizeof(NpMatching2Context));
    ctx->option = option;
    sceKernelMemcpy(&ctx->commId, commId, sizeof(SceNpCommId));
    sceKernelMemcpy(&ctx->passPhrase, passPhrase, sizeof(SceNpCommPassphrase));
    if (legacyMode == 2)
        ctx->stateFlags |= 1u;

    npLockAcquire();
    for (idx = 0; idx < 7; idx++) {
        if (g_ctxTable.slots[idx] == NULL) {
            ctx->ctxId = (SceNpMatching2ContextId)(idx + 1);
            break;
        }
    }
    if (g_ctxTable.head != NULL)
        ctx->next = g_ctxTable.head;
    g_ctxTable.head = ctx;
    g_ctxTable.slots[ctx->ctxId - 1u] = ctx;
    g_ctxTable.count++;
    npLockRelease();

    if (outCtx != NULL)
        *outCtx = ctx;
    return 0;
}

/* 0x0031fc */
NP_MATCHING2_NOINLINE s32 npCtxFindAndRef(SceNpMatching2ContextId ctxId, NpMatching2Context **outCtx)
{
    s32 slotIdx = (s32)ctxId - 1;
    NpMatching2Context *ctx;
    s32 ret;

    if (slotIdx < 0 || slotIdx >= 7)
        return SCE_NP_MATCHING2_ERROR_CTX_MAX;

    ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_FOUND;
    npLockAcquire();
    ctx = g_ctxTable.slots[slotIdx];
    if (ctx != NULL) {
        if (npWorkerIsCurrentThread() == 0 && ctx->destroying == 1) {
            ret = SCE_NP_MATCHING2_ERROR_DESTROYING;
        } else {
            ret = 0;
            if (outCtx != NULL) {
                *outCtx = ctx;
                ctx->refCount++;
            }
        }
    }
    npLockRelease();
    return ret;
}

/* 0x0032c4 */
NP_MATCHING2_NOINLINE s32 npCtxTryCleanup(NpMatching2Context *ctx)
{
    s32 canFree = 0;

    npLockAcquire();
    if (ctx->refCount == 0 && ctx->serverList == NULL)
        canFree = 1;
    npLockRelease();

    if (canFree && ctx->destroying == 1)
        return npCtxUnlinkAndFree(ctx);
    return 1;
}

/* 0x003334 */
NP_MATCHING2_NOINLINE s32 npCtxRelease(NpMatching2Context *ctx)
{
    s32 canFree = 0;

    if (ctx == NULL)
        return 0;

    npLockAcquire();
    ctx->refCount--;
    npLockRelease();

    npLockAcquire();
    if (ctx->refCount == 0 && ctx->serverList == NULL)
        canFree = 1;
    npLockRelease();

    if (canFree && ctx->destroying == 1)
        return npCtxUnlinkAndFree(ctx);
    return 0;
}

/* 0x0033c0 */
NP_MATCHING2_NOINLINE s32 npCtxMarkDestroy(NpMatching2Context *ctx)
{
    s32 ret;

    if (ctx->destroying == 1)
        return 0;

    ret = npWorkerCheckContextBusy(ctx);
    if (ret >= 0) {
        ret = npWorkerStopContextServers(ctx);
        if (ret >= 0)
            ctx->destroying = 1;
    }
    return ret;
}

/* 0x003420 */
NP_MATCHING2_NOINLINE NpMatching2Context *npCtxGetFirstRef(void)
{
    NpMatching2Context *found = NULL;
    NpMatching2Context *head;

    npLockAcquire();
    head = g_ctxTable.head;
    if (head != NULL) {
        if (npWorkerIsCurrentThread() != 0 || head->destroying != 1) {
            found = head;
            head->refCount++;
        }
    }
    npLockRelease();
    return found;
}

/* 0x003498 */
NP_MATCHING2_NOINLINE NpMatching2Context *npCtxGetNextRef(NpMatching2Context *ctx)
{
    NpMatching2Context *found = NULL;

    npLockAcquire();
    if (ctx != NULL && ctx->next != NULL) {
        NpMatching2Context *nextCtx = ctx->next;
        if (npWorkerIsCurrentThread() != 0 || nextCtx->destroying != 1) {
            found = nextCtx;
            nextCtx->refCount++;
        }
    }
    npLockRelease();
    return found;
}

/* 0x00351c */
NP_MATCHING2_NOINLINE s32 npCtxHasAnyStarted(void)
{
    s32 started = 0;
    NpMatching2Context *curr = npCtxGetFirstRef();

    while (curr != NULL) {
        NpMatching2Context *nextCtx = npCtxGetNextRef(curr);
        s32 canFree = 0;

        if ((curr->stateFlags & 2u) != 0)
            started = 1;

        npLockAcquire();
        curr->refCount--;
        npLockRelease();

        npLockAcquire();
        if (curr->refCount == 0 && curr->serverList == NULL)
            canFree = 1;
        npLockRelease();

        if (canFree && curr->destroying == 1)
            npCtxUnlinkAndFree(curr);

        curr = nextCtx;
        if (started == 1) {
            if (curr != NULL) {
                s32 freeNext = 0;
                npLockAcquire();
                curr->refCount--;
                npLockRelease();

                npLockAcquire();
                if (curr->refCount == 0 && curr->serverList == NULL)
                    freeNext = 1;
                npLockRelease();

                if (freeNext && curr->destroying == 1)
                    npCtxUnlinkAndFree(curr);
            }
            return 1;
        }
    }
    return 0;
}

/* 0x003670 */
NP_MATCHING2_NOINLINE s32 npCtxCheckStarted(const NpMatching2Context *ctx, s32 requireStarted)
{
    u32 stateFlags = ctx->stateFlags;

    if ((stateFlags & 4u) != 0)
        return SCE_NP_MATCHING2_ERROR_CTX_ERROR;
    if (requireStarted == 1 && (stateFlags & 2u) == 0)
        return SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
    return 0;
}

/* 0x0036b4 */
NP_MATCHING2_NOINLINE s32 npConnFindAndRef(SceNpMatching2ContextId ctxId, u32 connId, NpMatching2Conn **outConn)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2ServerNode *server = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npServerFindByIdAndRef(ctx, (SceNpMatching2ServerId)(connId >> 16), &server);
        if (ret >= 0) {
            u16 oneBasedSlot = (u16)(connId & 0xFFFFu);
            NpMatching2Conn *conn = server->conns[oneBasedSlot - 1u];
            if (conn == NULL) {
                ret = SCE_NP_MATCHING2_ERROR_CONN_NOT_FOUND;
            } else if (npWorkerIsCurrentThread() == 0 && conn->closing == 1) {
                ret = SCE_NP_MATCHING2_ERROR_DESTROYING;
            } else if (outConn != NULL) {
                *outConn = conn;
                conn->refCount++;
            }
        }
    }
    npLockRelease();

    if (server != NULL)
        npServerRelease(server);
    if (ctx != NULL)
        npCtxRelease(ctx);
    return ret;
}

/* 0x0037d0 */
NP_MATCHING2_NOINLINE s32 npConnCloseOrDestroy(NpMatching2Conn *conn)
{
    NpMatching2ServerNode *server;
    u16 oneBasedSlot;

    npLockAcquire();
    if (conn->ownerKind == 2u)
        npRoomDestroy(conn->ownerRoom);

    conn->closing = 1;
    if (conn->refCount != 0 || conn->sendQueue.head != NULL || conn->waitQueue.head != NULL)
        return npLockRelease();

    server = conn->server;
    npLockAcquire();
    oneBasedSlot = (u16)(conn->connId & 0xFFFFu);
    if (server->conns[oneBasedSlot - 1u] != NULL) {
        server->conns[oneBasedSlot - 1u]->server = NULL;
        server->conns[oneBasedSlot - 1u] = NULL;
        conn->server = NULL;
    }
    npLockRelease();
    npLockRelease();

    npConnResetTransport(conn);
    if (conn->hostList.entries != NULL)
        npMatching2Free(conn->hostList.entries);
    sceKernelMemset(&conn->hostList, 0, sizeof(conn->hostList));
    npMatching2Free(conn);
    return npServerCheckEmptyAndFree(server);
}

/* 0x0038d8 */
NP_MATCHING2_NOINLINE s32 npConnCheckServerBusy(NpMatching2Context *ctx, NpMatching2ServerNode *server)
{
    s32 ret = 0;
    s32 idx;

    npLockAcquire();
    for (idx = 0; idx < 15; idx++) {
        NpMatching2Conn *conn = NULL;
        s32 foundRet;
        s32 canDestroy = 0;

        if (server->conns[idx] == NULL)
            continue;

        foundRet = npConnFindAndRef(ctx->ctxId, ((u32)server->serverId << 16) | (u16)(idx + 1), &conn);
        if (foundRet < 0 || conn == NULL) {
            ret = 0;
            continue;
        }

        if (conn->closing != 1) {
            NpMatching2Req *curReq;
            s32 qRet = 0;

            npLockAcquire();
            curReq = npReqQueueGetFirstRef(&conn->sendQueue);
            while (curReq != NULL) {
                NpMatching2Req *nextReq = npReqQueueGetNextRef(curReq);
                if (curReq->completed == 0)
                    qRet = SCE_NP_MATCHING2_ERROR_BUSY;
                npReqRelease(curReq);
                if (qRet < 0) {
                    if (nextReq != NULL)
                        npReqRelease(nextReq);
                    break;
                }
                curReq = nextReq;
            }
            npLockRelease();
            ret = qRet;

            if (ret >= 0) {
                s32 wRet = 0;

                npLockAcquire();
                curReq = npReqQueueGetFirstRef(&conn->waitQueue);
                while (curReq != NULL) {
                    NpMatching2Req *nextReq = npReqQueueGetNextRef(curReq);
                    if (curReq->completed == 0)
                        wRet = SCE_NP_MATCHING2_ERROR_BUSY;
                    npReqRelease(curReq);
                    if (wRet < 0) {
                        if (nextReq != NULL)
                            npReqRelease(nextReq);
                        break;
                    }
                    curReq = nextReq;
                }
                npLockRelease();
                ret = wRet;
            }
        }

        npLockAcquire();
        conn->refCount--;
        if (conn->refCount == 0 && conn->closing == 1 &&
            conn->sendQueue.head == NULL && conn->waitQueue.head == NULL)
            canDestroy = 1;
        npLockRelease();

        if (canDestroy)
            npConnCloseOrDestroy(conn);

        if (ret < 0)
            break;
    }
    npLockRelease();
    return ret;
}

/* 0x003b18 */
NP_MATCHING2_NOINLINE s32 npConnFindBySlotAndRef(NpMatching2Context *ctx, NpMatching2ServerNode *server,
                                                 u16 slotIndex, NpMatching2Conn **outConn)
{
    return npConnFindAndRef(ctx->ctxId, ((u32)server->serverId << 16) | (u16)(slotIndex + 1u), outConn);
}

/* 0x003b4c */
NP_MATCHING2_NOINLINE s32 npConnRelease(NpMatching2Conn *conn)
{
    s32 canDestroy = 0;

    if (conn == NULL)
        return 0;

    npLockAcquire();
    conn->refCount--;
    if (conn->refCount == 0 && conn->closing == 1 &&
        conn->sendQueue.head == NULL && conn->waitQueue.head == NULL)
        canDestroy = 1;
    npLockRelease();

    if (canDestroy)
        return npConnCloseOrDestroy(conn);
    return 0;
}

/* 0x003bdc */
NP_MATCHING2_NOINLINE s32 npConnCreate(NpMatching2ServerNode *server, u32 connKind,
                                       NpMatching2HostList *hostList, NpMatching2Conn **outConn)
{
    NpMatching2Conn *conn;
    s32 ret;
    s32 idx;

    conn = (NpMatching2Conn *)npMatching2Alloc(sizeof(NpMatching2Conn));
    if (conn == NULL)
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    sceKernelMemset(conn, 0, sizeof(NpMatching2Conn));
    sceKernelMemcpy(&conn->hostList, hostList, sizeof(NpMatching2HostList));
    sceKernelMemset(hostList, 0, sizeof(NpMatching2HostList));
    conn->resolverId = -1;

    ret = npConnSetKind(connKind, conn);
    if (ret < 0) {
        npConnCloseOrDestroy(conn);
        return ret;
    }

    npLockAcquire();
    for (idx = 0; idx < 15; idx++) {
        if (server->conns[idx] == NULL)
            break;
    }
    if (idx == 15) {
        ret = SCE_NP_MATCHING2_ERROR_CONN_MAX;
    } else {
        conn->connId = ((u32)server->serverId << 16) | (u16)(idx + 1);
        conn->server = server;
        server->conns[idx] = conn;
        ret = 0;
    }
    npLockRelease();

    if (ret < 0) {
        npConnCloseOrDestroy(conn);
        return ret;
    }

    *outConn = conn;
    conn->curHostIndex = 0;
    return 0;
}

/* 0x003d38 */
NP_MATCHING2_NOINLINE s32 npConnCloseById(SceNpMatching2ContextId ctxId, u32 connId)
{
    NpMatching2Conn *conn = NULL;

    npLockAcquire();
    if (npConnFindAndRef(ctxId, connId, &conn) >= 0) {
        s32 canDestroy = 0;
        npConnCloseOrDestroy(conn);
        if (conn != NULL) {
            npLockAcquire();
            conn->refCount--;
            if (conn->refCount == 0 && conn->closing == 1 &&
                conn->sendQueue.head == NULL && conn->waitQueue.head == NULL)
                canDestroy = 1;
            npLockRelease();
            if (canDestroy)
                npConnCloseOrDestroy(conn);
        }
    }
    return npLockRelease();
}

/* 0x003e00 */
NP_MATCHING2_NOINLINE s32 npConnCheckQueueBusy(NpMatching2ReqQueue *queue)
{
    s32 ret = 0;
    NpMatching2Req *curr;

    npLockAcquire();
    curr = npReqQueueGetFirstRef(queue);
    while (curr != NULL) {
        NpMatching2Req *nextReq = npReqQueueGetNextRef(curr);
        if (curr->inFlight == 0)
            ret = SCE_NP_MATCHING2_ERROR_BUSY;
        npReqRelease(curr);
        if (ret < 0) {
            if (nextReq != NULL)
                npReqRelease(nextReq);
            break;
        }
        curr = nextReq;
    }
    npLockRelease();
    return ret;
}

/* 0x003ea4 */
NP_MATCHING2_NOINLINE s32 npConnCloseAllOnServer(NpMatching2Context *ctx, NpMatching2ServerNode *server)
{
    s32 ret;
    s32 idx;

    ret = npConnCheckServerBusy(ctx, server);
    if (ret < 0)
        return ret;

    npLockAcquire();
    for (idx = 0; idx < 15; idx++) {
        NpMatching2Conn *conn = NULL;

        if (server->conns[idx] == NULL)
            continue;

        ret = npConnFindAndRef(ctx->ctxId, ((u32)server->serverId << 16) | (u16)(idx + 1), &conn);
        if (ret >= 0 && conn != NULL) {
            s32 canDestroy = 0;
            npConnCloseOrDestroy(conn);
            npLockAcquire();
            conn->refCount--;
            if (conn->refCount == 0 && conn->closing == 1 &&
                conn->sendQueue.head == NULL && conn->waitQueue.head == NULL)
                canDestroy = 1;
            npLockRelease();
            if (canDestroy)
                npConnCloseOrDestroy(conn);
        } else {
            ret = 0;
        }
    }
    npLockRelease();
    return ret;
}

/* 0x003fe4 */
NP_MATCHING2_NOINLINE s32 npConnTryFinishDestroy(NpMatching2Conn *conn)
{
    s32 canDestroy = 0;
    s32 intrState;

    intrState = sceKernelCpuSuspendIntr();
    if (conn->closing != 0 && conn->sendQueue.head == NULL &&
        conn->waitQueue.head == NULL && conn->refCount == 0)
        canDestroy = 1;
    sceKernelCpuResumeIntr(intrState);

    if (canDestroy)
        return npConnCloseOrDestroy(conn);
    return 0;
}

/* 0x004064 */
NP_MATCHING2_NOINLINE s32 npCoreInit(u32 poolSize, u32 threadStackSize, s32 threadPriority)
{
    s32 partId;
    s32 ret;

    sceKernelMemset(&g_npMatching2AttrLimits, 0, sizeof(g_npMatching2AttrLimits));
    ret = sceKernelQueryMemoryInfo(&g_npMatching2AttrLimits, &partId, NULL);
    if (ret < 0) {
        npCoreTerm();
        return ret;
    }

    ret = npMatching2VplCreate(partId, poolSize, "SceMatching2");
    if (ret < 0) {
        npCoreTerm();
        return ret;
    }

    sceKernelGetCompiledSdkVersion();
    g_npMatching2AttrLimits.maxUserBinAttr = 1u;
    g_npMatching2AttrLimits.maxRoomBinAttrInternal = 2u;
    g_npMatching2AttrLimits.maxRoomSearchableInt = 8u;
    g_npMatching2AttrLimits.maxRoomGroup = 2u;
    g_npMatching2AttrLimits.maxRoomSearchableBin = 1u;
    g_npMatching2AttrLimits.maxRoomBinAttrExternal = 2u;
    g_npMatching2AttrLimits.maxMemberBinAttrInternal = 1u;
    g_npMatching2AttrLimits.maxRoomMemberDataBin = 1u;

    ret = npSslInitGlobal();
    if (ret >= 0)
        ret = npLockInit();
    if (ret >= 0)
        ret = npCtxTableInit();
    if (ret >= 0)
        ret = npTransportInit();
    if (ret >= 0)
        ret = npWorkerThreadStart((s32)threadStackSize, (u32)threadPriority);

    if (ret < 0) {
        npCoreTerm();
        return ret;
    }
    return 0;
}

/* 0x004198 */
NP_MATCHING2_NOINLINE s32 npCoreTerm(void)
{
    NpMatching2Context *curr;

    npLockAcquire();
    curr = npCtxGetFirstRef();
    while (curr != NULL) {
        NpMatching2Context *nextCtx = npCtxGetNextRef(curr);
        npCoreDestroyContext(curr->ctxId);
        npCtxRelease(curr);
        curr = nextCtx;
    }
    npLockRelease();

    npWorkerThreadStop();
    npTransportTerm();
    npCtxTableTerm();
    npLockTerm();
    npSslTermGlobal();
    npMatching2VplDelete();
    return 0;
}

/* 0x004230 */
NP_MATCHING2_NOINLINE s32 npCoreDestroyContext(SceNpMatching2ContextId ctxId)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    npCoreStopContext(ctxId);
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        npCoreNotifyCtxEvent(ctx, 28576u, 100u, 0);
        ret = npCtxMarkDestroy(ctx);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x0042bc */
NP_MATCHING2_NOINLINE s32 npCoreStopContext(SceNpMatching2ContextId ctxId)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) != 0) {
            npWorkerCloseContextRooms(ctx);
            ctx->stateFlags &= ~2u;
        } else {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        }
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x00435c */
NP_MATCHING2_NOINLINE s32 npCoreAbortRequest(SceNpMatching2RequestId reqId)
{
    s32 found = 0;
    NpMatching2Context *ctx;

    npLockAcquire();
    ctx = npCtxGetFirstRef();
    while (ctx != NULL) {
        NpMatching2Context *nextCtx = npCtxGetNextRef(ctx);
        NpMatching2ServerNode *server = npServerGetFirstRef(ctx);

        while (server != NULL) {
            NpMatching2ServerNode *nextServer = npServerGetNextRef(server);
            NpMatching2Req *req = NULL;

            if (npReqFindInQueue(&server->pendingQueue, reqId, &req) >= 0 ||
                npReqFindInQueue(&server->authWaitQueue, reqId, &req) >= 0) {
                found = 1;
                req->aborted = 1;
                npReqRelease(req);
            } else {
                u32 idx;
                for (idx = 0; idx < 15u; idx++) {
                    NpMatching2Conn *conn = NULL;
                    if (npConnFindBySlotAndRef(ctx, server, (u16)idx, &conn) < 0 || conn == NULL)
                        continue;

                    if (npReqFindInQueue(&conn->sendQueue, reqId, &req) >= 0 ||
                        npReqFindInQueue(&conn->waitQueue, reqId, &req) >= 0) {
                        found = 1;
                        req->aborted = 1;
                        npReqRelease(req);
                    }
                    npConnRelease(conn);
                    if (found == 1)
                        break;
                }
            }

            npServerRelease(server);
            if (found == 1) {
                if (nextServer != NULL)
                    npServerRelease(nextServer);
                break;
            }
            server = nextServer;
        }

        npCtxRelease(ctx);
        if (found == 1) {
            if (nextCtx != NULL)
                npCtxRelease(nextCtx);
            break;
        }
        ctx = nextCtx;
    }
    npLockRelease();

    if (found == 0)
        return SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND;
    return 0;
}

/* 0x00456c */
NP_MATCHING2_NOINLINE s32 npCoreGetServerIdListLocal(SceNpMatching2ContextId ctxId,
                                                     SceNpMatching2ServerId *serverIdList, u32 listNum)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            npLockAcquire();
            if (serverIdList != NULL && listNum != 0) {
                u32 count = 0;
                NpMatching2ServerNode *server = npServerGetFirstRef(ctx);
                while (server != NULL && count < listNum) {
                    NpMatching2ServerNode *nextServer = npServerGetNextRef(server);
                    serverIdList[count] = server->serverId;
                    count++;
                    npServerRelease(server);
                    server = nextServer;
                }
                if (server != NULL)
                    npServerRelease(server);
            }
            ret = (s32)ctx->serverCount;
            npLockRelease();
        }
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    return ret;
}

/* 0x004694 */
NP_MATCHING2_NOINLINE s32 npCoreGetServerInfo(SceNpMatching2ContextId ctxId,
                                              const SceNpMatching2GetServerInfoRequest *reqParam,
                                              const SceNpMatching2RequestOptParam *optParam,
                                              SceNpMatching2RequestId *assignedReqId)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2ServerNode *server = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npServerFindByIdAndRef(ctx, reqParam->serverId, &server);
            if (ret >= 0) {
                s32 isMaint = (server->status == 4u) ? 1 : 0;
                SceNpMatching2ServerId serverIdCopy;
                sceKernelMemset(&serverIdCopy, 0, sizeof(serverIdCopy));
                serverIdCopy = reqParam->serverId;
                ret = npCoreSubmitRequest(ctxId, serverIdCopy, 0, 0, 0, 1u, 1u, 0x1001u,
                                          &serverIdCopy, optParam, isMaint, assignedReqId, 0);
            }
        }
    }
    if (server != NULL)
        npServerRelease(server);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

typedef struct NpGrantOwnerInternalParam {
    SceNpMatching2RoomMemberId newOwner;
    u16 padding;
    u8 optData[20];
} NpGrantOwnerInternalParam;

/* 0x0047ec */
NP_MATCHING2_NOINLINE s32 npCoreGrantRoomOwner(SceNpMatching2ContextId ctxId,
                                               const SceNpMatching2GrantRoomOwnerRequest *reqParam,
                                               const SceNpMatching2RequestOptParam *optParam,
                                               SceNpMatching2RequestId *assignedReqId)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npCtxCheckStarted(ctx, 1);
        if (ret >= 0) {
            ret = npRoomFindByIdAndRef(ctx, reqParam->roomId, &room);
            if (ret >= 0) {
                if (reqParam->newOwner == room->meMember->memberId) {
                    ret = SCE_NP_MATCHING2_ERROR_SELF_TARGET;
                } else {
                    NpGrantOwnerInternalParam internalParam;
                    sceKernelMemset(&internalParam, 0, sizeof(internalParam));
                    sceKernelMemcpy(internalParam.optData, reqParam->optData, sizeof(internalParam.optData));
                    internalParam.newOwner = reqParam->newOwner;
                    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 13u, 0x104u, 0x3210u,
                                              &internalParam, optParam, 0, assignedReqId, 0);
                }
            }
        }
    }
    if (room != NULL)
        npRoomRelease(room);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

typedef struct NpKickoutInternalParam {
    SceNpMatching2RoomMemberId target;
    u8 blockFlag;
    u8 padding;
    u8 optData[20];
} NpKickoutInternalParam;

/* 0x004960 */
NP_MATCHING2_NOINLINE s32 npCoreKickoutRoomMember(SceNpMatching2ContextId ctxId,
                                                  const SceNpMatching2KickoutRoomMemberRequest *reqParam,
                                                  const SceNpMatching2RequestOptParam *optParam,
                                                  SceNpMatching2RequestId *assignedReqId)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npCtxCheckStarted(ctx, 1);
        if (ret >= 0) {
            ret = npRoomFindByIdAndRef(ctx, reqParam->roomId, &room);
            if (ret >= 0) {
                if (reqParam->target == room->meMember->memberId) {
                    ret = SCE_NP_MATCHING2_ERROR_SELF_TARGET;
                } else {
                    NpKickoutInternalParam internalParam;
                    sceKernelMemset(&internalParam, 0, sizeof(internalParam));
                    sceKernelMemcpy(internalParam.optData, reqParam->optData, sizeof(internalParam.optData));
                    internalParam.target = reqParam->target;
                    internalParam.blockFlag = reqParam->blockFlag;
                    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 14u, 0x105u, 0x3211u,
                                              &internalParam, optParam, 0, assignedReqId, 0);
                }
            }
        }
    }
    if (room != NULL)
        npRoomRelease(room);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

typedef struct SceNpMatching2RoomDataInternalOut {
    SceNpMatching2ServerId serverId;
    u16 serverPad;
    SceNpMatching2WorldId worldId;
    u64 lobbyId;
    SceNpMatching2RoomId roomId;
    u64 passwordSlotMask;
    u32 maxSlots;
    void *memberList;
    u32 memberCount;
    void *meMember;
    void *ownerMember;
    SceNpMatching2RoomGroup *groupList;
    u32 groupCount;
    u32 flagAttr;
    SceNpMatching2BinAttrInternal *binAttrInternal;
    u32 binAttrInternalCount;
} SceNpMatching2RoomDataInternalOut;

/* 0x004adc */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                         const SceNpMatching2AttributeId *attrId, u32 attrIdNum,
                                                         void *roomDataOut, void *buf, u32 bufSize)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npRoomFindByIdAndRef(ctx, roomId, &room);
            if (ret >= 0) {
                u8 *writePtr = (u8 *)buf;
                u32 alignRem = (u32)(uintptr_t)writePtr & 3u;
                u32 alignPad = 0;
                u32 matchedCount = 0;
                u32 requiredBytes;
                u32 attrIdx;

                if (alignRem != 0) {
                    alignPad = 4u - alignRem;
                    writePtr += alignPad;
                }

                requiredBytes = alignPad + (u32)(sizeof(SceNpMatching2RoomGroup) * room->groupCount);
                for (attrIdx = 0; attrIdx < room->binAttrInternalCount; attrIdx++) {
                    u32 filterIdx;
                    for (filterIdx = 0; filterIdx < attrIdNum; filterIdx++) {
                        if (attrId[filterIdx] == room->binAttrInternal[attrIdx].attr.id)
                            break;
                    }
                    if (filterIdx != attrIdNum) {
                        matchedCount++;
                        requiredBytes += room->binAttrInternal[attrIdx].attr.size;
                    }
                }
                requiredBytes += matchedCount * (u32)sizeof(SceNpMatching2BinAttrInternal);
                ret = (s32)requiredBytes;

                if (roomDataOut != NULL && bufSize >= requiredBytes) {
                    SceNpMatching2RoomDataInternalOut *outRoom = (SceNpMatching2RoomDataInternalOut *)roomDataOut;
                    sceKernelMemcpy(outRoom, room, sizeof(SceNpMatching2RoomDataInternalOut));
                    outRoom->memberList = NULL;
                    outRoom->meMember = NULL;
                    outRoom->ownerMember = NULL;
                    outRoom->binAttrInternal = NULL;
                    outRoom->binAttrInternalCount = 0;

                    if (room->groupList != NULL && room->groupCount != 0) {
                        u32 groupBytes = (u32)(sizeof(SceNpMatching2RoomGroup) * room->groupCount);
                        outRoom->groupList = (SceNpMatching2RoomGroup *)writePtr;
                        sceKernelMemcpy(writePtr, room->groupList, groupBytes);
                        writePtr += groupBytes;
                        outRoom->groupCount = room->groupCount;
                    }

                    if (matchedCount != 0) {
                        SceNpMatching2BinAttrInternal *dstAttrs = (SceNpMatching2BinAttrInternal *)writePtr;
                        u32 dstPos = 0;
                        u32 attrTableBytes = matchedCount * (u32)sizeof(SceNpMatching2BinAttrInternal);

                        sceKernelMemset(dstAttrs, 0, attrTableBytes);
                        writePtr += attrTableBytes;

                        for (attrIdx = 0; attrIdx < room->binAttrInternalCount; attrIdx++) {
                            u32 filterIdx;
                            for (filterIdx = 0; filterIdx < attrIdNum; filterIdx++) {
                                if (attrId[filterIdx] == room->binAttrInternal[attrIdx].attr.id)
                                    break;
                            }
                            if (filterIdx != attrIdNum) {
                                u32 payloadSize = room->binAttrInternal[attrIdx].attr.size;
                                sceKernelMemcpy(&dstAttrs[dstPos], &room->binAttrInternal[attrIdx],
                                                sizeof(SceNpMatching2BinAttrInternal));
                                dstAttrs[dstPos].attr.ptr = writePtr;
                                sceKernelMemcpy(writePtr, room->binAttrInternal[attrIdx].attr.ptr, payloadSize);
                                writePtr += payloadSize;
                                dstPos++;
                            }
                        }
                        outRoom->binAttrInternal = dstAttrs;
                        outRoom->binAttrInternalCount = matchedCount;
                    }
                    ret = (s32)(writePtr - (u8 *)buf);
                }
            }
        }
    }
    if (room != NULL)
        npRoomRelease(room);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x004e30 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomMemberIdListLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                         SceNpMatching2RoomMemberId *memberIdList, u32 listNum)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npRoomFindByIdAndRef(ctx, roomId, &room);
            if (ret >= 0) {
                NpMatching2RoomMember *member = room->memberList;
                u32 count = 0;
                while (member != NULL) {
                    if (count < listNum)
                        memberIdList[count] = member->memberId;
                    count++;
                    member = member->next;
                }
                ret = (s32)count;
            }
        }
    }
    if (room != NULL)
        npRoomRelease(room);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x004f58 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomMemberDataInternalLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                               SceNpMatching2RoomMemberId memberId,
                                                               const SceNpMatching2AttributeId *attrId, u32 attrIdNum,
                                                               void *memberDataOut, void *buf, u32 bufSize)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    NpMatching2RoomMember *member = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npRoomFindByIdAndRef(ctx, roomId, &room);
            if (ret >= 0) {
                ret = npRoomFindMemberById(room, memberId, &member);
                if (ret >= 0) {
                    u8 *writePtr = (u8 *)buf;
                    u32 alignRem = (u32)(uintptr_t)writePtr & 3u;
                    u32 alignPad = 0;
                    u32 matchedCount = 0;
                    u32 requiredBytes;
                    u32 attrIdx;

                    if (alignRem != 0) {
                        alignPad = 4u - alignRem;
                        writePtr += alignPad;
                    }
                    requiredBytes = alignPad;

                    for (attrIdx = 0; attrIdx < member->binAttrInternalCount; attrIdx++) {
                        u32 filterIdx;
                        for (filterIdx = 0; filterIdx < attrIdNum; filterIdx++) {
                            if (attrId[filterIdx] == member->binAttrInternal[attrIdx].attr.id)
                                break;
                        }
                        if (filterIdx != attrIdNum) {
                            matchedCount++;
                            requiredBytes += member->binAttrInternal[attrIdx].attr.size;
                        }
                    }
                    requiredBytes += matchedCount * (u32)sizeof(SceNpMatching2MemberBinAttrInternal);
                    ret = (s32)requiredBytes;

                    if (memberDataOut != NULL && bufSize >= requiredBytes) {
                        NpMatching2RoomMember *outMember = (NpMatching2RoomMember *)memberDataOut;
                        sceKernelMemcpy(outMember, member, sizeof(NpMatching2RoomMember));
                        outMember->next = NULL;
                        outMember->binAttrInternal = NULL;
                        outMember->binAttrInternalCount = 0;

                        if (matchedCount != 0) {
                            SceNpMatching2MemberBinAttrInternal *dstAttrs = (SceNpMatching2MemberBinAttrInternal *)writePtr;
                            u32 dstPos = 0;
                            u32 attrTableBytes = matchedCount * (u32)sizeof(SceNpMatching2MemberBinAttrInternal);

                            sceKernelMemset(dstAttrs, 0, attrTableBytes);
                            writePtr += attrTableBytes;

                            for (attrIdx = 0; attrIdx < member->binAttrInternalCount; attrIdx++) {
                                u32 filterIdx;
                                for (filterIdx = 0; filterIdx < attrIdNum; filterIdx++) {
                                    if (attrId[filterIdx] == member->binAttrInternal[attrIdx].attr.id)
                                        break;
                                }
                                if (filterIdx != attrIdNum) {
                                    u32 payloadSize = member->binAttrInternal[attrIdx].attr.size;
                                    sceKernelMemcpy(&dstAttrs[dstPos], &member->binAttrInternal[attrIdx],
                                                    sizeof(SceNpMatching2MemberBinAttrInternal));
                                    dstAttrs[dstPos].attr.ptr = writePtr;
                                    sceKernelMemcpy(writePtr, member->binAttrInternal[attrIdx].attr.ptr, payloadSize);
                                    writePtr += payloadSize;
                                    dstPos++;
                                }
                            }
                            outMember->binAttrInternal = dstAttrs;
                            outMember->binAttrInternalCount = matchedCount;
                        }
                        ret = (s32)(writePtr - (u8 *)buf);
                    }
                }
            }
        }
    }
    if (room != NULL)
        npRoomRelease(room);
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x005250 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomPasswordLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                     s32 *withPasswordOut, SceNpMatching2SessionPassword *passwordOut)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npRoomFindByIdAndRef(ctx, roomId, &room);
            if (ret >= 0) {
                if (room->meMember == room->ownerMember) {
                    if (room->hasPassword == 1) {
                        *withPasswordOut = 1;
                        sceKernelMemcpy(passwordOut, &room->password, sizeof(SceNpMatching2SessionPassword));
                    } else {
                        *withPasswordOut = 0;
                    }
                } else {
                    ret = SCE_NP_MATCHING2_ERROR_NOT_ROOM_OWNER;
                }
            }
        }
    }
    npLockRelease();
    return ret;
}

/* 0x00534c */
NP_MATCHING2_NOINLINE void npCoreNotifyCtxEvent(NpMatching2Context *ctx, u16 eventId, u8 eventCause, s32 errorCode)
{
    s32 idx;

    if (ctx == NULL)
        return;

    for (idx = 0; idx < 3; idx++) {
        if (ctx->listeners[idx].inUse != 0) {
            typedef void (*NpCtxEventCb)(SceNpMatching2ContextId, u16, u8, s32, void *);
            NpCtxEventCb cb = (NpCtxEventCb)ctx->listeners[idx].callbacks[8];
            if (cb != NULL)
                cb(ctx->ctxId, eventId, eventCause, errorCode, ctx->listeners[idx].callbacks[9]);
        }
    }
}

/* 0x0053e8 */
NP_MATCHING2_NOINLINE s32 npCoreCreateContext(const SceNpCommId *commId, const SceNpCommPassphrase *passPhrase,
                                              SceNpMatching2ContextId *ctxIdOut, u32 option, s32 legacyMode)
{
    NpMatching2Context *ctx;
    s32 ret;

    npLockAcquire();
    ret = npCtxCheckCanCreate(commId);
    if (ret >= 0) {
        ret = npCtxCreate(commId, passPhrase, option, legacyMode, &ctx);
        if (ret >= 0)
            *ctxIdOut = ctx->ctxId;
    }
    npLockRelease();
    return ret;
}

/* 0x005494 */
NP_MATCHING2_NOINLINE s32 npCoreSetDefaultReqOpt(SceNpMatching2ContextId ctxId,
                                                 const SceNpMatching2RequestOptParam *optParam)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0)
        sceKernelMemcpy(&ctx->defaultReqOpt, optParam, sizeof(SceNpMatching2RequestOptParam));
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x005518 */
NP_MATCHING2_NOINLINE s32 npCoreSetDefaultRoomEvOpt(SceNpMatching2ContextId ctxId,
                                                    const SceNpMatching2RoomEventOptParam *optParam)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0)
        sceKernelMemcpy(&ctx->defaultRoomEvOpt, optParam, sizeof(SceNpMatching2RoomEventOptParam));
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x00559c */
NP_MATCHING2_NOINLINE s32 npCoreSetDefaultRoomMsgOpt(SceNpMatching2ContextId ctxId,
                                                     const SceNpMatching2RoomMessageOptParam *optParam)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0)
        sceKernelMemcpy(&ctx->defaultRoomMsgOpt, optParam, sizeof(SceNpMatching2RoomMessageOptParam));
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x005620 */
NP_MATCHING2_NOINLINE s32 npCoreStartContext(SceNpMatching2ContextId ctxId)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) != 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_ALREADY_STARTED;
        } else {
            ret = npWorkerStartContext(ctx);
            if (ret >= 0)
                ctx->stateFlags |= 2u;
        }
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x0056c4 */
NP_MATCHING2_NOINLINE s32 npCoreRegisterListener(u32 listenerId, SceNpMatching2ContextId ctxId, const void *callbacks)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npCtxCheckStarted(ctx, 1);
        if (ret >= 0) {
            if (listenerId >= 3u) {
                ret = SCE_NP_MATCHING2_ERROR_INVALID_LISTENER_ID;
            } else if (ctx->listeners[listenerId].inUse == 1) {
                ret = SCE_NP_MATCHING2_ERROR_LISTENER_ALREADY_REG;
            } else {
                ctx->listeners[listenerId].slotId = listenerId;
                sceKernelMemcpy(ctx->listeners[listenerId].callbacks, callbacks, 40u);
                ctx->listeners[listenerId].inUse = 1;
            }
        }
        npCtxRelease(ctx);
    }
    return ret;
}

/* 0x0057b0 */
NP_MATCHING2_NOINLINE s32 npCoreUnregisterListener(u32 listenerId, SceNpMatching2ContextId ctxId)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npCtxCheckStarted(ctx, 1);
        if (ret >= 0) {
            if (listenerId >= 3u) {
                ret = SCE_NP_MATCHING2_ERROR_INVALID_LISTENER_ID;
            } else if (ctx->listeners[listenerId].inUse == 0) {
                ret = SCE_NP_MATCHING2_ERROR_LISTENER_NOT_REG;
            } else {
                ctx->listeners[listenerId].slotId = 0;
                sceKernelMemset(ctx->listeners[listenerId].callbacks, 0, 40u);
                ctx->listeners[listenerId].inUse = 0;
            }
        }
        npCtxRelease(ctx);
    }
    return ret;
}

/* 0x00587c */
NP_MATCHING2_NOINLINE s32 npCoreGetWorldInfoList(SceNpMatching2ContextId ctxId,
                                                 const SceNpMatching2GetWorldInfoListRequest *reqParam,
                                                 const SceNpMatching2RequestOptParam *optParam,
                                                 SceNpMatching2RequestId *assignedReqId)
{
    s32 ret;

    npLockAcquire();
    ret = npCoreSubmitRequest(ctxId, reqParam->serverId, 0, 0, 1, 3u, 2u, 0x1008u,
                              NULL, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x005918 */
NP_MATCHING2_NOINLINE s32 npCoreSetUserInfo(SceNpMatching2ContextId ctxId,
                                            const SceNpMatching2SetUserInfoRequest *reqParam,
                                            const SceNpMatching2RequestOptParam *optParam,
                                            SceNpMatching2RequestId *assignedReqId)
{
    const SceNpMatching2SetUserInfoRequest *wrapped;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrapped, 0, sizeof(wrapped));
    wrapped = reqParam;
    ret = npCoreSubmitRequest(ctxId, reqParam->serverId, 0, 0, 1, 4u, 3u, 0x1006u,
                              &wrapped, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x0059cc */
NP_MATCHING2_NOINLINE s32 npCoreGetUserInfoList(SceNpMatching2ContextId ctxId,
                                                const SceNpMatching2GetUserInfoListRequest *reqParam,
                                                const SceNpMatching2RequestOptParam *optParam,
                                                SceNpMatching2RequestId *assignedReqId)
{
    NpGetUserInfoWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2GetUserInfoListRequest *)reqParam;
    ret = npCoreSubmitRequest(ctxId, reqParam->serverId, 0, 0, 0, 5u, 4u, 0x1007u,
                              &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x005a80 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomMemberDataExternalList(SceNpMatching2ContextId ctxId,
                                                              const SceNpMatching2GetRoomMemberDataExternalListRequest *reqParam,
                                                              const SceNpMatching2RequestOptParam *optParam,
                                                              SceNpMatching2RequestId *assignedReqId)
{
    NpGetRoomMemberExtWrap wrap;
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.roomId = reqParam->roomId;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        SceNpMatching2ServerId serverId = (SceNpMatching2ServerId)(reqParam->roomId >> 48);
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, serverId, 0, 0, 1, 6u, 5u, 0x1206u,
                                  &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x005b88 */
NP_MATCHING2_NOINLINE s32 npCoreSetRoomDataExternal(SceNpMatching2ContextId ctxId,
                                                    const SceNpMatching2SetRoomDataExternalRequest *reqParam,
                                                    const SceNpMatching2RequestOptParam *optParam,
                                                    SceNpMatching2RequestId *assignedReqId)
{
    const SceNpMatching2SetRoomDataExternalRequest *wrapped;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrapped, 0, sizeof(wrapped));
    wrapped = reqParam;
    ret = npCoreSubmitRequest(ctxId, (SceNpMatching2ServerId)(reqParam->roomId >> 48), 0, 0, 1,
                              7u, 6u, 0x1207u, &wrapped, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x005c3c */
NP_MATCHING2_NOINLINE s32 npCoreSetSignalingOptParam(SceNpMatching2ContextId ctxId,
                                                     const SceNpMatching2SetSignalingOptParamRequest *reqParam,
                                                     const SceNpMatching2RequestOptParam *optParam,
                                                     SceNpMatching2RequestId *assignedReqId)
{
    NpSetSigOptWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2SetSignalingOptParamRequest *)reqParam;
    ret = npCoreSubmitRequest(ctxId, (SceNpMatching2ServerId)(reqParam->roomId >> 48), reqParam->roomId, 0, 0,
                              23u, 0x10Eu, 0x3204u, &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x005cf0 */
NP_MATCHING2_NOINLINE s32 npCoreGetSignalingOptParamLocal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                          SceNpMatching2SignalingOptParam *optParamOut)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        ret = npRoomFindByIdAndRef(ctx, roomId, &room);
        if (ret >= 0 && optParamOut != NULL) {
            u32 copyLen = room->sigOptLen;
            if (copyLen >= 9u)
                copyLen = 8u;
            sceKernelMemcpy(optParamOut, &room->sigOptParam, copyLen);
        }
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    if (room != NULL)
        npRoomRelease(room);
    npLockRelease();
    return ret;
}

/* 0x005dd8 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomDataExternalList(SceNpMatching2ContextId ctxId,
                                                        const SceNpMatching2GetRoomDataExternalListRequest *reqParam,
                                                        const SceNpMatching2RequestOptParam *optParam,
                                                        SceNpMatching2RequestId *assignedReqId)
{
    NpCtxReqPairWrap wrap;
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (void *)reqParam;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        SceNpMatching2ServerId serverId = (SceNpMatching2ServerId)(reqParam->roomId[0] >> 48);
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, serverId, 0, 0, 1, 8u, 7u, 0x1208u,
                                  &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x005ed8 */
NP_MATCHING2_NOINLINE s32 npCoreCreateJoinRoom(SceNpMatching2ContextId ctxId,
                                               const SceNpMatching2CreateJoinRoomRequest *reqParam,
                                               const SceNpMatching2RequestOptParam *optParam,
                                               const SceNpMatching2RoomEventOptParam *roomEvOpt,
                                               const SceNpMatching2RoomMessageOptParam *roomMsgOpt,
                                               SceNpMatching2RequestId *assignedReqId)
{
    NpCreateJoinWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2CreateJoinRoomRequest *)reqParam;
    wrap.roomEvOpt = (SceNpMatching2RoomEventOptParam *)roomEvOpt;
    wrap.roomMsgOpt = (SceNpMatching2RoomMessageOptParam *)roomMsgOpt;
    ret = npCoreSubmitRequest(ctxId, (SceNpMatching2ServerId)(reqParam->worldId >> 16), 0, 0, 1,
                              9u, 0x101u, 0x1209u, &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x005fac */
NP_MATCHING2_NOINLINE s32 npCoreJoinRoom(SceNpMatching2ContextId ctxId,
                                         const SceNpMatching2JoinRoomRequest *reqParam,
                                         const SceNpMatching2RequestOptParam *optParam,
                                         const SceNpMatching2RoomEventOptParam *roomEvOpt,
                                         const SceNpMatching2RoomMessageOptParam *roomMsgOpt,
                                         SceNpMatching2RequestId *assignedReqId)
{
    NpJoinRoomWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2JoinRoomRequest *)reqParam;
    wrap.roomEvOpt = (SceNpMatching2RoomEventOptParam *)roomEvOpt;
    wrap.roomMsgOpt = (SceNpMatching2RoomMessageOptParam *)roomMsgOpt;
    ret = npCoreSubmitRequest(ctxId, (SceNpMatching2ServerId)(reqParam->roomId >> 48), 0, 0, 1,
                              10u, 0x102u, 0x1209u, &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x006080 */
NP_MATCHING2_NOINLINE s32 npCoreLeaveRoom(SceNpMatching2ContextId ctxId,
                                          const SceNpMatching2LeaveRoomRequest *reqParam,
                                          const SceNpMatching2RequestOptParam *optParam,
                                          SceNpMatching2RequestId *assignedReqId)
{
    NpLeaveRoomWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    sceKernelMemcpy(&wrap.optData, reqParam->optData, sizeof(wrap.optData));
    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 12u, 0x103u, 0x3202u,
                              &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x006140 */
NP_MATCHING2_NOINLINE s32 npCoreSearchRoom(SceNpMatching2ContextId ctxId,
                                           const SceNpMatching2SearchRoomRequest *reqParam,
                                           const SceNpMatching2RequestOptParam *optParam,
                                           SceNpMatching2RequestId *assignedReqId)
{
    NpCtxReqPairWrap wrap;
    NpMatching2Context *ctx = NULL;
    SceNpMatching2ServerId serverId;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    serverId = (SceNpMatching2ServerId)(reqParam->worldId >> 16);
    wrap.req = (void *)reqParam;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        u16 protoCmd = ((reqParam->option & 0x10) != 0) ? 0x120Cu : 0x1200u;
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, serverId, 0, 0, 1,
                                  15u, 0x106u, protoCmd, &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x006258 */
NP_MATCHING2_NOINLINE s32 npCoreSendRoomChatMessage(SceNpMatching2ContextId ctxId,
                                                    const SceNpMatching2SendRoomChatMessageRequest *reqParam,
                                                    const SceNpMatching2RequestOptParam *optParam,
                                                    SceNpMatching2RequestId *assignedReqId)
{
    const SceNpMatching2SendRoomChatMessageRequest *wrapped;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrapped, 0, sizeof(wrapped));
    wrapped = reqParam;
    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 16u, 0x107u, 0x3208u,
                              &wrapped, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x00630c */
NP_MATCHING2_NOINLINE s32 npCoreSendRoomMessage(SceNpMatching2ContextId ctxId,
                                                const SceNpMatching2SendRoomMessageRequest *reqParam,
                                                const SceNpMatching2RequestOptParam *optParam,
                                                SceNpMatching2RequestId *assignedReqId,
                                                s32 reqSource)
{
    const SceNpMatching2SendRoomMessageRequest *wrapped;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrapped, 0, sizeof(wrapped));
    wrapped = reqParam;
    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 17u, 0x108u, 0x320Au,
                              &wrapped, optParam, 0, assignedReqId, reqSource);
    npLockRelease();
    return ret;
}

/* 0x0063cc */
NP_MATCHING2_NOINLINE s32 npCoreSetRoomDataInternal(SceNpMatching2ContextId ctxId,
                                                    const SceNpMatching2SetRoomDataInternalRequest *reqParam,
                                                    const SceNpMatching2RequestOptParam *optParam,
                                                    SceNpMatching2RequestId *assignedReqId)
{
    NpSetRoomDataInternalWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2SetRoomDataInternalRequest *)reqParam;
    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 18u, 0x109u, 0x3204u,
                              &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x006480 */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomDataInternal(SceNpMatching2ContextId ctxId,
                                                    const SceNpMatching2GetRoomDataInternalRequest *reqParam,
                                                    const SceNpMatching2RequestOptParam *optParam,
                                                    SceNpMatching2RequestId *assignedReqId)
{
    NpGetRoomAttrWrap wrap;
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.attrId = (SceNpMatching2AttributeId *)reqParam->attrId;
    wrap.attrIdNum = reqParam->attrIdNum;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 19u, 0x10Au, 0x3205u,
                                  &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x006588 */
NP_MATCHING2_NOINLINE s32 npCoreSetRoomMemberDataInternal(SceNpMatching2ContextId ctxId,
                                                          const SceNpMatching2SetRoomMemberDataInternalRequest *reqParam,
                                                          const SceNpMatching2RequestOptParam *optParam,
                                                          SceNpMatching2RequestId *assignedReqId)
{
    NpSetRoomMemberDataInternalWrap wrap;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.req = (SceNpMatching2SetRoomMemberDataInternalRequest *)reqParam;
    ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 20u, 0x10Bu, 0x3206u,
                              &wrap, optParam, 0, assignedReqId, 0);
    npLockRelease();
    return ret;
}

/* 0x00663c */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomMemberDataInternal(SceNpMatching2ContextId ctxId,
                                                          const SceNpMatching2GetRoomMemberDataInternalRequest *reqParam,
                                                          const SceNpMatching2RequestOptParam *optParam,
                                                          SceNpMatching2RequestId *assignedReqId)
{
    NpGetRoomAttrWrap wrap;
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.memberId = reqParam->memberId;
    wrap.attrId = (SceNpMatching2AttributeId *)reqParam->attrId;
    wrap.attrIdNum = reqParam->attrIdNum;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 21u, 0x10Cu, 0x3207u,
                                  &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x00674c */
NP_MATCHING2_NOINLINE s32 npCoreGetRoomMemberDataInternalList(SceNpMatching2ContextId ctxId,
                                                              const SceNpMatching2GetRoomMemberDataInternalListRequest *reqParam,
                                                              const SceNpMatching2RequestOptParam *optParam,
                                                              SceNpMatching2RequestId *assignedReqId)
{
    NpGetRoomAttrWrap wrap;
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&wrap, 0, sizeof(wrap));
    wrap.attrId = (SceNpMatching2AttributeId *)reqParam->attrId;
    wrap.attrIdNum = reqParam->attrIdNum;
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        wrap.ctx = ctx;
        ret = npCoreSubmitRequest(ctxId, 0, reqParam->roomId, 0, 0, 22u, 0x10Du, 0x3203u,
                                  &wrap, optParam, 0, assignedReqId, 0);
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return ret;
}

/* 0x006854 */
NP_MATCHING2_NOINLINE s32 npCoreRegisterSignalingCb(SceNpMatching2ContextId ctxId, void *cbFunc, void *cbArg)
{
    NpMatching2Context *ctx = NULL;

    npLockAcquire();
    if (npCtxFindAndRef(ctxId, &ctx) >= 0) {
        if (npCtxCheckStarted(ctx, 0) >= 0) {
            ctx->sigCb = cbFunc;
            ctx->sigCbArg = cbArg;
        }
    }
    if (ctx != NULL)
        npCtxRelease(ctx);
    npLockRelease();
    return 0;
}

/* 0x0068ec */
NP_MATCHING2_NOINLINE s32 npCoreDispatchSignalingCb(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                    SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId,
                                                    s32 errorCode)
{
    if (npWorkerIsCurrentThread() != 1)
        return npWorkerPostEvent(1, ctxId, roomId, memberId, eventId, errorCode);

    npWorkerInvokeSignalingCb(ctxId, roomId, memberId, eventId, errorCode);
    return 0;
}

/* 0x0069a8 */
NP_MATCHING2_NOINLINE s32 npCoreSubmitRoomInfoInternal(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                       const SceNpMatching2RequestOptParam *optParam,
                                                       SceNpMatching2RequestId *assignedReqId)
{
    SceNpMatching2RoomId roomIdCopy;
    s32 ret;

    npLockAcquire();
    sceKernelMemset(&roomIdCopy, 0, sizeof(roomIdCopy));
    roomIdCopy = roomId;
    ret = npCoreSubmitRequest(ctxId, (SceNpMatching2ServerId)(roomId >> 48), 0, 0, 0, 24u, 0xA102u, 0x120Bu,
                              &roomIdCopy, optParam, 0, assignedReqId, 1);
    npLockRelease();
    return ret;
}

/* 0x006a78 */
NP_MATCHING2_NOINLINE s32 npCoreSubmitRequest(SceNpMatching2ContextId ctxId, SceNpMatching2ServerId serverId,
                                              SceNpMatching2RoomId roomId, u64 lobbyId, s32 requireAuth,
                                              u8 reqKind, SceNpMatching2Event eventId, u16 protoCmd,
                                              const void *reqParam, const SceNpMatching2RequestOptParam *optParam,
                                              s32 isMaintenance, SceNpMatching2RequestId *assignedReqId, s32 reqSource)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2ServerNode *server = NULL;
    NpMatching2Room *room = NULL;
    NpMatching2Conn *conn = NULL;
    NpMatching2Req *req = NULL;
    s32 lockedForAuth = 0;
    s32 waitForAuth = 0;
    u16 deferredCmd = 0;
    s32 ret;
    u32 targetConnId = 0;

    (void)lobbyId;

    if (roomId != 0)
        serverId = (SceNpMatching2ServerId)(roomId >> 48);

    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        if ((ctx->stateFlags & 2u) == 0) {
            ret = SCE_NP_MATCHING2_ERROR_CTX_NOT_STARTED;
        } else {
            ret = npServerFindByIdAndRef(ctx, serverId, &server);
            if (ret >= 0) {
                if (roomId != 0) {
                    ret = npRoomFindByIdAndRef(ctx, roomId, &room);
                    if (ret >= 0)
                        targetConnId = room->conn->connId;
                } else if (serverId == 0) {
                    ret = SCE_NP_MATCHING2_ERROR_INVALID_SERVER_ID;
                } else {
                    targetConnId = server->mainConnId;
                }

                if (ret >= 0) {
                    if (requireAuth == 1) {
                        lockedForAuth = 1;
                        npLockAcquire();
                        if (server->sessionId == 0) {
                            if (server->authInProgress != 0) {
                                waitForAuth = 1;
                            } else {
                                targetConnId = server->authConnId;
                                deferredCmd = protoCmd;
                                protoCmd = 0x1002u;
                                server->authInProgress = 1;
                            }
                        }
                    }

                    ret = npConnFindAndRef(ctxId, targetConnId, &conn);
                    if (ret >= 0) {
                        ret = npReqCreate(ctxId, reqSource, reqKind, eventId, protoCmd, reqParam,
                                          conn, optParam, isMaintenance, assignedReqId, &req);
                        if (ret >= 0) {
                            if (waitForAuth != 1 && deferredCmd != 0)
                                req->nextProtoCmd = deferredCmd;
                            if (waitForAuth == 1)
                                ret = npReqEnqueue(req, NULL, &server->authWaitQueue);
                            else
                                ret = npReqEnqueue(req, conn, &conn->sendQueue);
                            if (ret >= 0)
                                npWorkerWakeup();
                        }
                    }
                }
            }
        }
    }

    if (lockedForAuth == 1)
        npLockRelease();

    if (ret < 0 && req != NULL)
        npReqAbortOnSubmitFail(ctx, req, ret);

    if (conn != NULL)
        npConnRelease(conn);
    if (room != NULL)
        npRoomRelease(room);
    if (server != NULL)
        npServerRelease(server);
    if (ctx != NULL)
        npCtxRelease(ctx);
    return ret;
}

/* 0x006d78 */
NP_MATCHING2_NOINLINE s32 npLockInit(void)
{
    s32 ret = 0;

    if (g_lockInitialized == 0) {
        ret = sceKernelCreateLwMutex(&g_npLock, "SceNpMatching2Lock", 0x200u, 0, NULL);
        if (ret >= 0)
            g_lockInitialized = 1;
    }
    return ret;
}

/* 0x006ddc */
NP_MATCHING2_NOINLINE s32 npLockTerm(void)
{
    s32 ret = g_lockInitialized;

    if (g_lockInitialized == 1) {
        ret = sceKernelDeleteLwMutex(&g_npLock);
        g_lockInitialized = 0;
    }
    return ret;
}

/* 0x006e20 */
NP_MATCHING2_NOINLINE s32 npLockAcquire(void)
{
    if (g_lockInitialized != 0)
        return sceKernelLockLwMutex(&g_npLock, 1, NULL);
    return SCE_NP_MATCHING2_ERROR_LOCK_NOT_INIT;
}

/* 0x006e60 */
NP_MATCHING2_NOINLINE s32 npLockRelease(void)
{
    if (g_lockInitialized != 0)
        return sceKernelUnlockLwMutex(&g_npLock, 1);
    return SCE_NP_MATCHING2_ERROR_LOCK_NOT_INIT;
}
