#include "pmpvar.h"

void __attribute__((noinline))
matchingRemovePeer(MatchingCtx *ctx, MatchingPeer *peer)
{
    MatchingPeer *prev = NULL;
    MatchingPeer *cur = ctx->peers;

    while (cur != NULL && cur != peer) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL)
        return;

    if (prev != NULL)
        prev->next = cur->next;
    else
        ctx->peers = cur->next;

    if (peer->opt_len > 0)
        sceKernelFreeVpl(g_vpl_id, peer->opt_buf);
    if (peer->rx_pkt != NULL)
        sceKernelFreeVpl(g_vpl_id, peer->rx_pkt);
    if (peer->data_len > 0)
        sceKernelFreeVpl(g_vpl_id, peer->data_buf);
    sceKernelFreeVpl(g_vpl_id, peer);
}

/* 0x003a58 */
void __attribute__((noinline))
matchingSetPeerState(MatchingCtx *ctx, MatchingPeer *peer, int new_state)
{
    int old_state = peer->state;
    if (old_state == new_state)
        return;

    if ((old_state == PEER_STATE_ACCEPTED || old_state == PEER_STATE_SELECT_SENT) &&
        new_state != PEER_STATE_ACCEPTED && new_state != PEER_STATE_SELECT_SENT &&
        peer->opt_len > 0) {
        sceKernelFreeVpl(g_vpl_id, peer->opt_buf);
        peer->opt_len = 0;
        peer->opt_buf = NULL;
    }
    if (old_state != PEER_STATE_ESTABLISHED && new_state == PEER_STATE_ESTABLISHED) {
        peer->rx_expected_seq = 0;
        peer->tx_seq = 0;
    }
    if (old_state == PEER_STATE_ESTABLISHED && new_state != PEER_STATE_ESTABLISHED)
        matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);

    peer->state = new_state;
    if (old_state == PEER_STATE_ESTABLISHED || new_state == PEER_STATE_ESTABLISHED)
        matchingBuildMembersPkt(ctx);
}

/* 0x003b50 */
void __attribute__((noinline))
matchingSetPeerDataState(MatchingCtx *ctx, MatchingPeer *peer, int new_state)
{
    int old_state = peer->data_state;
    int closing = (old_state == DATA_STATE_SENDING) & (new_state == DATA_STATE_IDLE);
    (void)ctx;
    if (old_state != new_state) {
        if (closing && peer->data_len > 0) {
            sceKernelFreeVpl(g_vpl_id, peer->data_buf);
            peer->data_len = 0;
            peer->data_buf = NULL;
        }
        peer->data_state = new_state;
    }
}

/* 0x003bc4 */
int __attribute__((noinline))
matchingCreateLock(void)
{
    int ret = sceKernelCreateLwMutex(g_mutex_work, "SceNetAdhocMatchingLock", 0x200, 0, NULL);
    if (ret >= 0) {
        g_mutex_created = 1;
        return 0;
    }
    return ret;
}

/* 0x003c14 */
void __attribute__((noinline))
matchingDeleteLock(void)
{
    if (g_mutex_created != 0)
        sceKernelDeleteLwMutex(g_mutex_work);
    g_mutex_created = 0;
}

/* 0x003c58 */
int __attribute__((noinline))
matchingLock(void)
{
    return sceKernelLockLwMutex(g_mutex_work, 1, NULL);
}

/* 0x003c80 */
void __attribute__((noinline))
matchingUnlock(int lock_res)
{
    if (lock_res == 0)
        sceKernelUnlockLwMutex(g_mutex_work, 1);
}

/* 0x003cb8 */
int __attribute__((noinline))
matchingInitCtxList(void)
{
    g_ctx_list = NULL;
    return 0;
}

/* 0x003cc8 */
int __attribute__((noinline))
matchingFreeAllCtxs(void)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL) {
        MatchingCtx *next = cur->next;
        sceKernelFreeVpl(g_vpl_id, cur->rx_buf);
        cur->rx_buf = NULL;
        sceKernelFreeVpl(g_vpl_id, cur);
        cur = next;
    }
    return 0;
}

/* 0x003d34 */
int __attribute__((noinline))
matchingCheckNoActiveCtx(void)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL) {
        if (cur->running != 0)
            return MATCHING_ERR_BUSY;
        cur = cur->next;
    }
    return 0;
}

/* 0x003d6c */
int __attribute__((noinline))
matchingCreateVpl(int poolsize)
{
    int vpl = sceKernelCreateVpl("SceNetAdhocMatchingMemoryPool", 2, 0, (u32)poolsize, NULL);
    g_vpl_id = vpl;
    if (vpl >= 0) {
        g_pool_max_alloc = 0;
        return 0;
    }
    return vpl;
}

/* 0x003dbc */
void __attribute__((noinline))
matchingDeleteVpl(void)
{
    if (g_vpl_id >= 0) {
        sceKernelDeleteVpl(g_vpl_id);
        g_vpl_id = -1;
    }
}

/* 0x003df8 */
void *__attribute__((noinline))
matchingAllocVpl(int size)
{
    void *ptr;
    MatchingVplInfo info;
    if (sceKernelTryAllocateVpl(g_vpl_id, (u32)size, &ptr) < 0)
        return NULL;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    return ptr;
}

/* 0x003e6c */
int __attribute__((noinline))
matchingFreeVpl(void *ptr)
{
    return sceKernelFreeVpl(g_vpl_id, ptr);
}

/* 0x003e94 */
int __attribute__((noinline))
matchingGetPoolMaxAllocInternal(void)
{
    return g_pool_max_alloc;
}

/* 0x003ea0 */
int __attribute__((noinline))
matchingGetPoolStatInternal(MatchingPoolStat *stat)
{
    MatchingVplInfo info;
    int ret;

    info.size = sizeof(info);
    ret = sceKernelReferVplStatus(g_vpl_id, &info);
    if (ret >= 0) {
        stat->size = (u32)info.pool_size;
        stat->maxsize = (u32)g_pool_max_alloc;
        stat->freesize = (u32)info.free_size;
        ret = 0;
    }
    return ret;
}

/* 0x003f04 */
int __attribute__((noinline))
matchingAllocCtx(u16 port)
{
    MatchingCtx *cur = g_ctx_list;
    int cand;
    int chosen = MATCHING_ERR_ID_NOT_AVAIL;
    MatchingCtx *ctx;
    void *raw = NULL;
    MatchingVplInfo info;

    while (cur != NULL) {
        if (cur->port == port)
            return MATCHING_ERR_PORT_IN_USE;
        cur = cur->next;
    }

    cand = (g_last_ctx_id == 15) ? 1 : (g_last_ctx_id + 1);
    while (cand != g_last_ctx_id) {
        MatchingCtx *it = g_ctx_list;
        while (it != NULL && it->id != cand)
            it = it->next;
        if (it == NULL) {
            chosen = cand;
            g_last_ctx_id = cand;
            break;
        }
        cand = (cand == 15) ? 1 : (cand + 1);
    }

    if (chosen < 0)
        return chosen;

    if (sceKernelTryAllocateVpl(g_vpl_id, sizeof(MatchingCtx), &raw) < 0)
        return MATCHING_ERR_NO_SPACE;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    ctx = (MatchingCtx *)raw;
    if (ctx == NULL)
        return MATCHING_ERR_NO_SPACE;

    sceKernelMemset(ctx, 0, sizeof(MatchingCtx));
    ctx->id = chosen;
    ctx->next = g_ctx_list;
    g_ctx_list = ctx;
    return chosen;
}

/* 0x00407c */
int __attribute__((noinline))
matchingFreeCtx(MatchingCtx *ctx)
{
    MatchingCtx *cur = g_ctx_list;
    MatchingCtx *prev = NULL;

    if (cur != NULL) {
        while (cur != NULL && cur != ctx) {
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            if (prev != NULL)
                prev->next = ctx->next;
            else
                g_ctx_list = ctx->next;
            cur->next = NULL;
        }
    }
    return sceKernelFreeVpl(g_vpl_id, ctx);
}

/* 0x0040e8 */
MatchingCtx *__attribute__((noinline))
matchingFindCtxById(int id)
{
    MatchingCtx *cur = g_ctx_list;
    while (cur != NULL && cur->id != id)
        cur = cur->next;
    return cur;
}

/* 0x004118 */
MatchingPeer *__attribute__((noinline))
matchingCreatePeer(MatchingCtx *ctx, const u8 *mac)
{
    MatchingPeer *peer;
    void *raw = NULL;
    MatchingVplInfo info;
    u32 sess;

    if (sceKernelTryAllocateVpl(g_vpl_id, sizeof(MatchingPeer), &raw) < 0)
        return NULL;
    info.size = sizeof(info);
    if (sceKernelReferVplStatus(g_vpl_id, &info) == 0) {
        int used = info.pool_size - info.free_size;
        if (used > g_pool_max_alloc)
            g_pool_max_alloc = used;
    }
    peer = (MatchingPeer *)raw;
    if (peer == NULL)
        return NULL;

    sceKernelMemset(peer, 0, sizeof(MatchingPeer));
    matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
    sceKernelMemcpy(peer->mac, mac, 6);
    peer->event_mbx = ctx->event_mbx;
    sess = sceNet_lib_1858883D();
    peer->own_session_id = (sess == 0) ? 1u : sess;
    peer->ctrl_alarm_id = -1;
    matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
    peer->data_alarm_id = -1;
    peer->next = ctx->peers;
    ctx->peers = peer;
    return peer;
}

/* 0x004230 */
MatchingPeer *__attribute__((noinline))
matchingFindPeerByMac(MatchingCtx *ctx, const u8 *mac)
{
    MatchingPeer *cur = ctx->peers;
    while (cur != NULL) {
        if (sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0 && cur->pending_free == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/* 0x004294 */
int __attribute__((noinline))
matchingDestroyAllPeers(MatchingCtx *ctx)
{
    MatchingPeer *cur = ctx->peers;
    while (cur != NULL) {
        MatchingPeer *next = cur->next;
        matchingCancelPeerAlarms(cur);
        if (cur->opt_len > 0)
            sceKernelFreeVpl(g_vpl_id, cur->opt_buf);
        if (cur->data_len > 0)
            sceKernelFreeVpl(g_vpl_id, cur->data_buf);
        if (cur->rx_pkt != NULL)
            sceKernelFreeVpl(g_vpl_id, cur->rx_pkt);
        sceKernelFreeVpl(g_vpl_id, cur);
        cur = next;
    }
    ctx->peers = NULL;
    return matchingBroadcastBye(ctx);
}

/* 0x004354 */
void __attribute__((noinline))
matchingCollectPeerMacs(MatchingCtx *ctx, int min_state, u8 *out, int *count_inout)
{
    MatchingPeer *cur = ctx->peers;
    int count = 0;
    u8 *dst = out;

    while (cur != NULL) {
        if (cur->state >= min_state) {
            if (out != NULL) {
                if (count >= *count_inout) {
                    *count_inout = count;
                    return;
                }
                sceKernelMemcpy(dst, cur->mac, 6);
            }
            dst += 6;
            count++;
        }
        cur = cur->next;
    }
    *count_inout = count;
}

/* 0x004404 */
int __attribute__((noinline))
matchingCountPeersAtLeast(MatchingCtx *ctx, int min_state)
{
    MatchingPeer *cur = ctx->peers;
    int count = 0;
    while (cur != NULL) {
        if (cur->state >= min_state)
            count++;
        cur = cur->next;
    }
    return count;
}

/* 0x00443c */
void __attribute__((noinline))
matchingGetMembersInternal(MatchingCtx *ctx, int *buflen, void *buf)
{
    int written = 0;
    int off = 4;
    MatchingMember *prev = NULL;
    MatchingMember *base = (MatchingMember *)buf;

    while (off < ctx->members_pkt_len) {
        int next_len = written + (int)sizeof(MatchingMember);
        if (buf != NULL) {
            MatchingMember *slot = &base[written / (int)sizeof(MatchingMember)];
            if (*buflen < next_len) {
                *buflen = written;
                return;
            }
            sceKernelMemcpy(slot->mac, ctx->members_pkt + off, 6);
            slot->next = NULL;
            if (prev != NULL)
                prev->next = slot;
            prev = slot;
        }
        off += 6;
        written = next_len;
    }
    *buflen = written;
}

/* 0x004504 */
int __attribute__((noinline))
matchingCompareMac(const MatchingPeer *peer)
{
    u32 peer_hi = ((u32)peer->mac[0] << 16) | ((u32)peer->mac[1] << 8) | peer->mac[2];
    u32 self_hi = ((u32)g_self_mac[0] << 16) | ((u32)g_self_mac[1] << 8) | g_self_mac[2];
    u32 peer_lo;
    u32 self_lo;

    if (self_hi < peer_hi)
        return 1;
    if (peer_hi < self_hi)
        return 0;

    peer_lo = ((u32)peer->mac[3] << 16) | ((u32)peer->mac[4] << 8) | peer->mac[5];
    self_lo = ((u32)g_self_mac[3] << 16) | ((u32)g_self_mac[4] << 8) | g_self_mac[5];
    if (self_lo < peer_lo)
        return 1;
    if (peer_lo < self_lo)
        return 0;
    return -1;
}
