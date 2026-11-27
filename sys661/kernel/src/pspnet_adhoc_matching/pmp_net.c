#include "pmpvar.h"

int __attribute__((noinline))
matchingSendCtrlPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int optlen, const void *opt)
{
    int has_trailer = ((u32)(pkt_type - 2) < 2u);
    int total_len = has_trailer ? (optlen + 20) : (optlen + 4);
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)optlen);
    u32 be_sess;
    int ret;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = (u8)pkt_type;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    if (optlen > 0)
        sceKernelMemcpy(pkt + 4, opt, (u32)optlen);
    if (has_trailer) {
        be_sess = matchingSwap32(peer->own_session_id);
        sceKernelMemcpy(pkt + optlen + 4, &be_sess, 4);
        sceKernelMemset(pkt + optlen + 8, 0, 12);
    }
    ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, pkt, total_len, 0, 1);
    matchingFreeVpl(pkt);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002bc4 */
int __attribute__((noinline))
matchingCreatePdp(MatchingCtx *ctx)
{
    int pdp = sceNetAdhocPdpCreate(g_self_mac, ctx->port, (u32)ctx->rx_buflen, 0);
    if (pdp >= 0) {
        ctx->pdp_id = pdp;
        return 0;
    }
    return pdp;
}

/* 0x002c10 */
int __attribute__((noinline))
matchingDeletePdp(MatchingCtx *ctx)
{
    int ret = sceNetAdhocPdpDelete(ctx->pdp_id, 0);
    ctx->pdp_id = 0;
    return ret;
}

/* 0x002c40 */
int __attribute__((noinline))
matchingSendDataPkt(MatchingCtx *ctx, MatchingPeer *peer, int pkt_type, int datalen, const void *data)
{
    int total_len = datalen + 12;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(datalen + 8));
    u32 be_val;
    int ret;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = (u8)pkt_type;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    be_val = matchingSwap32(peer->own_session_id);
    sceKernelMemcpy(pkt + 4, &be_val, 4);
    be_val = matchingSwap32((pkt_type == PKT_DATA) ? peer->tx_seq : (peer->rx_expected_seq - 1));
    sceKernelMemcpy(pkt + 8, &be_val, 4);
    if (datalen > 0)
        sceKernelMemcpy(pkt + 12, data, (u32)datalen);

    ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, pkt, total_len, 0, 1);
    matchingFreeVpl(pkt);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002d88 */
int __attribute__((noinline))
matchingBuildHelloPkt(MatchingCtx *ctx, int optlen, const void *opt)
{
    int total_len = optlen + 28;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(optlen + 8));
    u32 be_val;

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = PKT_HELLO;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    be_val = matchingSwap32(ctx->hello_delay);
    sceKernelMemcpy(pkt + 4, &be_val, 4);
    be_val = matchingSwap32(ctx->ping_delay);
    sceKernelMemcpy(pkt + 8, &be_val, 4);
    if (optlen > 0)
        sceKernelMemcpy(pkt + 12, opt, (u32)optlen);
    sceKernelMemcpy(pkt + optlen + 12, ctx->hello_trailer, 4);
    sceKernelMemset(pkt + optlen + 16, 0, 12);

    if (ctx->hello_pkt != NULL)
        matchingFreeVpl(ctx->hello_pkt);
    ctx->hello_pkt = pkt;
    ctx->hello_pkt_len = total_len;
    return 0;
}

/* 0x002ebc */
void __attribute__((noinline))
matchingFreeHelloPkt(MatchingCtx *ctx)
{
    if (ctx->hello_pkt_len > 0) {
        matchingFreeVpl(ctx->hello_pkt);
        ctx->hello_pkt = NULL;
        ctx->hello_pkt_len = 0;
    }
}

/* 0x002ef4 */
int __attribute__((noinline))
matchingBroadcastHello(MatchingCtx *ctx)
{
    u8 bcast[16];
    int ret;

    sceKernelMemset(bcast, 0xFF, 6);
    ret = sceNetAdhocPdpSend(ctx->pdp_id, bcast, ctx->port, ctx->hello_pkt, ctx->hello_pkt_len, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x002f54 */
int __attribute__((noinline))
matchingBuildMembersPkt(MatchingCtx *ctx)
{
    int count = matchingCountPeersAtLeast(ctx, PEER_STATE_ESTABLISHED);
    int total_len = count * 6 + 10;
    u8 *pkt = (u8 *)matchingAllocVpl(total_len);
    u16 be_len = matchingSwap16((u16)(count * 6 + 6));

    if (pkt == NULL)
        return MATCHING_ERR_NO_SPACE;

    pkt[0] = 1;
    pkt[1] = PKT_KEEPALIVE;
    sceKernelMemcpy(pkt + 2, &be_len, 2);
    sceKernelMemcpy(pkt + 4, g_self_mac, 6);
    matchingCollectPeerMacs(ctx, PEER_STATE_ESTABLISHED, pkt + 10, &count);
    if (ctx->members_pkt != NULL)
        matchingFreeVpl(ctx->members_pkt);
    ctx->members_pkt = pkt;
    ctx->members_pkt_len = total_len;
    return 0;
}

/* 0x003040 */
int __attribute__((noinline))
matchingSendMembersPkt(MatchingCtx *ctx, MatchingPeer *peer)
{
    int ret = sceNetAdhocPdpSend(ctx->pdp_id, peer->mac, ctx->port, ctx->members_pkt, ctx->members_pkt_len, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x003088 */
int __attribute__((noinline))
matchingCopyMembersPkt(MatchingCtx *ctx, const void *pkt, int len)
{
    u8 *buf = (u8 *)matchingAllocVpl(len);
    if (buf == NULL)
        return MATCHING_ERR_NO_SPACE;
    sceKernelMemcpy(buf, pkt, (u32)len);
    if (ctx->members_pkt != NULL)
        matchingFreeVpl(ctx->members_pkt);
    ctx->members_pkt_len = len;
    ctx->members_pkt = buf;
    return 0;
}

/* 0x00311c */
void __attribute__((noinline))
matchingFreeMembersPkt(MatchingCtx *ctx)
{
    if (ctx->members_pkt_len > 0) {
        matchingFreeVpl(ctx->members_pkt);
        ctx->members_pkt = NULL;
        ctx->members_pkt_len = 0;
    }
}

/* 0x003154 */
int __attribute__((noinline))
matchingBroadcastBye(MatchingCtx *ctx)
{
    u8 bcast[16];
    u8 pkt[4];
    int ret;

    sceKernelMemset(bcast, 0xFF, 6);
    pkt[0] = 1;
    pkt[1] = PKT_BYE;
    sceKernelMemset(pkt + 2, 0, 2);
    ret = sceNetAdhocPdpSend(ctx->pdp_id, bcast, ctx->port, pkt, 4, 0, 1);
    return (ret == MATCHING_ERR_PDP_WOULDBLOCK) ? 0 : ret;
}

/* 0x0031d4 */
int __attribute__((noinline))
matchingStartInputThread(MatchingCtx *ctx, int prio, int stack, int mpid)
{
    MatchingThreadOpt opt;
    MatchingCtx *arg = ctx;
    int thid;
    int ret;

    sceKernelMemset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.stack_mpid = mpid;
    thid = sceKernelCreateThread("SceNetAdhocMatchingInput", (void *)matchingInputThread, prio, stack, 0, &opt);
    arg->input_thid = thid;
    if (thid < 0)
        return arg->input_thid;

    ret = sceKernelStartThread(arg->input_thid, 4, &arg);
    if (ret >= 0)
        return 0;
    sceKernelDeleteThread(arg->input_thid);
    return ret;
}

/* 0x0032a4 */
int __attribute__((noinline))
matchingStopInputThread(MatchingCtx *ctx, int *lock_res)
{
    sceNetAdhocSetSocketAlert(ctx->pdp_id, 0x20);
    matchingUnlock(*lock_res);
    sceKernelWaitThreadEnd(ctx->input_thid, NULL);
    *lock_res = matchingLock();
    sceNetFreeThreadinfo(ctx->input_thid);
    return sceKernelDeleteThread(ctx->input_thid);
}

/* 0x00330c */
int matchingInputThread(u32 arglen, void *argp)
{
    MatchingCtx *ctx = *(MatchingCtx **)argp;
    u8 src_mac[16];
    u16 src_port;
    u16 body_len;
    int recv_len;

    (void)arglen;
    for (;;) {
        recv_len = ctx->rx_buflen;
        if (sceNetAdhocPdpRecv(ctx->pdp_id, src_mac, &src_port, ctx->rx_buf, &recv_len, 0, 0) < 0)
            return 0;
        if (sceNet_lib_E0A81C7C(g_self_mac, src_mac, 6) == 0)
            continue;
        if (src_port != ctx->port)
            continue;
        if (ctx->rx_buf[0] != 1)
            continue;
        sceKernelMemcpy(&body_len, ctx->rx_buf + 2, 2);
        {
            int wire_len = (int)matchingSwap16(body_len) + 4;
            int lock_res;
            MatchingPeer *peer;

            if (recv_len < wire_len)
                continue;

            lock_res = matchingLock();
            peer = matchingFindPeerByMac(ctx, src_mac);
            if (peer == NULL) {
                u8 pkt_type = ctx->rx_buf[1];
                if ((pkt_type == PKT_JOIN && (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP)) ||
                    (pkt_type == PKT_HELLO && (u32)(ctx->mode - 2) < 2u)) {
                    peer = matchingCreatePeer(ctx, src_mac);
                }
            }
            if (peer != NULL && (peer->rx_msg.pending_flag & 1u) == 0) {
                u8 *pkt_copy = (u8 *)matchingAllocVpl(recv_len);
                if (pkt_copy != NULL) {
                    sceKernelMemcpy(pkt_copy, ctx->rx_buf, (u32)recv_len);
                    peer->rx_wire_len = wire_len;
                    peer->rx_raw_len = recv_len;
                    peer->rx_pkt = pkt_copy;
                    peer->rx_msg.msg_type = MBX_MSG_PACKET;
                    peer->rx_msg.peer = peer;
                    peer->rx_msg.pending_flag |= 1u;
                    sceKernelSendMbx(ctx->event_mbx, &peer->rx_msg);
                }
            }
            matchingUnlock(lock_res);
        }
    }
}

/* 0x003508 */
