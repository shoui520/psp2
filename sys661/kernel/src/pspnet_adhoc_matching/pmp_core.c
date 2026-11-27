#include "pmpvar.h"

int __attribute__((noinline))
_sce_pspnet_pmp_event_start(MatchingCtx *ctx, int prio, int stack, int mpid)
{
    MatchingThreadOpt opt;
    MatchingCtx *arg = ctx;
    int mbx;
    int thid;
    int ret;

    mbx = sceKernelCreateMbx("SceNetAdhocMatchingEvent", 0, NULL);
    arg->event_mbx = mbx;
    if (mbx < 0)
        return arg->event_mbx;

    sceKernelMemset(&opt, 0, sizeof(opt));
    opt.size = sizeof(opt);
    opt.stack_mpid = mpid;
    thid = sceKernelCreateThread("SceNetAdhocMatchingEvent", (void *)pmp_event_thread, prio, stack, 0, &opt);
    arg->event_thid = thid;
    if (thid < 0) {
        ret = arg->event_thid;
    } else {
        ret = sceKernelStartThread(arg->event_thid, 4, &arg);
        if (ret >= 0)
            return 0;
        sceKernelDeleteThread(arg->event_thid);
    }
    sceKernelDeleteMbx(arg->event_mbx);
    return ret;
}

/* 0x0016ac */
int __attribute__((noinline))
_sce_pspnet_pmp_event_term(MatchingCtx *ctx, int *lock_res)
{
    sceKernelDeleteMbx(ctx->event_mbx);
    _sce_pspnet_pmp_unlock(*lock_res);
    sceKernelWaitThreadEnd(ctx->event_thid, NULL);
    *lock_res = _sce_pspnet_pmp_lock();
    sceNetFreeThreadinfo(ctx->event_thid);
    return sceKernelDeleteThread(ctx->event_thid);
}

/* 0x001710 */
int pmp_event_thread(u32 arglen, void *argp)
{
    MatchingCtx *ctx = *(MatchingCtx **)argp;
    void *raw_msg;

    (void)arglen;
    while (sceKernelReceiveMbx(ctx->event_mbx, &raw_msg, NULL) >= 0) {
        MatchingMbxMsg *msg = (MatchingMbxMsg *)raw_msg;
        int lock_res = _sce_pspnet_pmp_lock();
        int msg_type = msg->msg_type;
        MatchingPeer *peer = msg->peer;
        int intr = sceKernelCpuSuspendIntr();
        msg->pending_flag &= ~1u;
        sceKernelCpuResumeIntr(intr);

        switch (msg_type) {
        case MBX_MSG_PACKET:
            pmp_eventmsg_recvmsg(ctx, peer);
            if (peer->rx_pkt != NULL)
                _sce_pspnet_pmp_free(peer->rx_pkt);
            peer->rx_raw_len = 0;
            peer->rx_pkt = NULL;
            peer->rx_wire_len = 0;
            break;
        case MBX_MSG_CTRL_RETRY:
            pmp_eventmsg_rexmttimer(ctx, peer);
            break;
        case MBX_MSG_KEEPALIVE:
            if (peer->state == PEER_STATE_ESTABLISHED) {
                peer->ping_retry--;
                if (peer->ping_retry > 0) {
                    if (ctx->mode == MATCHING_MODE_HOST ||
                        (ctx->mode == MATCHING_MODE_PTP && _sce_pspnet_pmp_is_parent(peer) != 0)) {
                        _sce_pspnet_pmp_sendmsg_keepaliverequest(ctx, peer);
                    }
                    _sce_pspnet_pmp_settimer_keepalive(ctx, peer);
                } else {
                    _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
                    _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
                    matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
                }
            }
            break;
        case MBX_MSG_HELLO:
            if (_sce_pspnet_pmp_handle_getnum(ctx, PEER_STATE_ACCEPTED) + 1 < ctx->max_peers)
                _sce_pspnet_pmp_sendmsg_hello(ctx);
            _sce_pspnet_pmp_settimer_hello(ctx, ctx->hello_delay);
            break;
        case MBX_MSG_DATA_RETRY:
            if (peer->data_state == DATA_STATE_SENDING) {
                peer->data_retry--;
                if (peer->data_retry <= 0) {
                    matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA_TIMEOUT, peer->mac, 0, NULL);
                } else {
                    matchingSendDataPkt(ctx, peer, PKT_DATA, peer->data_len, peer->data_buf);
                    matchingSetDataRetryAlarm(ctx, peer);
                }
            }
            break;
        default:
            break;
        }

        if (peer != NULL &&
            peer->pending_free == 1 &&
            (peer->rx_msg.pending_flag & 1u) == 0 &&
            (peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
            matchingRemovePeer(ctx, peer);
        }
        _sce_pspnet_pmp_unlock(lock_res);
    }
    return 0;
}

/* 0x001a40 */
void __attribute__((noinline))
pmp_eventmsg_recvmsg(MatchingCtx *ctx, MatchingPeer *peer)
{
    u32 pkt_sess_id = 0;
    u32 pkt_seq;
    int mode = ctx->mode;
    u32 pkt_type = peer->rx_pkt[1];
    int active_count;

    if (mode == MATCHING_MODE_CLIENT) {
        if (pkt_type == PKT_JOIN || pkt_type == PKT_KEEPALIVE_ACK)
            return;
    } else if (mode == MATCHING_MODE_HOST) {
        if (pkt_type == PKT_HELLO || pkt_type == PKT_KEEPALIVE)
            return;
    } else if (mode == MATCHING_MODE_PTP) {
        u32 blocked = (_sce_pspnet_pmp_is_parent(peer) != 0) ? PKT_KEEPALIVE : PKT_KEEPALIVE_ACK;
        if (pkt_type == blocked)
            return;
    }

    if (pkt_type == PKT_JOIN || pkt_type == PKT_ACCEPT) {
        int wire_len = peer->rx_wire_len;
        if (peer->rx_raw_len - wire_len >= 16) {
            sceKernelMemcpy(&pkt_sess_id, peer->rx_pkt + wire_len, 4);
            pkt_sess_id = matchingSwap32(pkt_sess_id);
            if (pkt_sess_id != peer->remote_session_id) {
                switch (peer->state) {
                case PEER_STATE_JOIN_RECV:
                case PEER_STATE_ACCEPTED:
                    _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
                    _sce_pspnet_pmp_stoptimer(peer);
                    matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
                    break;
                case PEER_STATE_ESTABLISHED:
                    _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
                    _sce_pspnet_pmp_stoptimer(peer);
                    matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, 0, NULL);
                    break;
                default:
                    break;
                }
            }
        }
    }

    active_count = _sce_pspnet_pmp_handle_getnum(ctx, PEER_STATE_ACCEPTED);
    switch (pkt_type) {
    case PKT_HELLO:
        if (peer->rx_wire_len - 4 >= 8 && peer->state == PEER_STATE_IDLE) {
            u32 h_delay, p_delay;
            int wire_len;
            sceKernelMemcpy(&h_delay, peer->rx_pkt + 4, 4);
            peer->peer_hello_delay = matchingSwap32(h_delay);
            sceKernelMemcpy(&p_delay, peer->rx_pkt + 8, 4);
            peer->peer_ping_delay = matchingSwap32(p_delay);
            wire_len = peer->rx_wire_len;
            if (peer->rx_raw_len - wire_len >= 16)
                sceKernelMemcpy(&peer->peer_caps, peer->rx_pkt + wire_len, 4);
            if (active_count + 1 < ctx->max_peers) {
                int optlen = peer->rx_wire_len - 12;
                void *opt = peer->rx_pkt + 12;
                if (optlen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_HELLO, peer->mac, optlen, opt);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_HELLO, peer->mac, 0, NULL);
            }
        }
        break;
    case PKT_JOIN:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            if (active_count + 1 >= ctx->max_peers) {
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            } else {
                int optlen = peer->rx_wire_len - 4;
                void *opt = peer->rx_pkt + 4;
                _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_JOIN_RECV);
                peer->remote_session_id = pkt_sess_id;
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_JOIN_ACK, 0, NULL);
                if (optlen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_JOIN, peer->mac, optlen, opt);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_JOIN, peer->mac, 0, NULL);
            }
            break;
        case PEER_STATE_JOIN_RECV:
            if (active_count + 1 < ctx->max_peers) {
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_JOIN_ACK, 0, NULL);
            } else {
                _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
                _sce_pspnet_pmp_stoptimer(peer);
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
            }
            break;
        case PEER_STATE_ACCEPTED:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            _sce_pspnet_pmp_settimer_rexmt(ctx, peer);
            break;
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_ACCEPTED);
            peer->remote_session_id = pkt_sess_id;
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            _sce_pspnet_pmp_settimer_rexmt(ctx, peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_ACCEPT:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_ESTABLISHED);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_COMPLETE, 0, NULL);
            _sce_pspnet_pmp_settimer_keepalive(ctx, peer);
            peer->ping_retry = ctx->init_count;
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_ESTABLISHED);
            peer->remote_session_id = pkt_sess_id;
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_COMPLETE, 0, NULL);
            _sce_pspnet_pmp_settimer_keepalive(ctx, peer);
            peer->ping_retry = ctx->init_count;
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_ACCEPT, peer->mac, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_COMPLETE, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_COMPLETE:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_SELECT_SENT:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_ESTABLISHED);
            _sce_pspnet_pmp_settimer_keepalive(ctx, peer);
            peer->ping_retry = ctx->init_count;
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        default:
            break;
        }
        break;
    case PKT_CANCEL:
        switch (peer->state) {
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_ACCEPTED: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_CANCEL, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_SELECT_SENT: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_REJECT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_REJECT, peer->mac, 0, NULL);
            break;
        }
        case PEER_STATE_ESTABLISHED: {
            int optlen = peer->rx_wire_len - 4;
            void *opt = peer->rx_pkt + 4;
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            if (optlen > 0)
                matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, optlen, opt);
            else
                matchingInvokeHandler(ctx, MATCHING_EV_LEFT, peer->mac, 0, NULL);
            break;
        }
        default:
            break;
        }
        break;
    case PKT_KEEPALIVE:
        switch (peer->state) {
        case PEER_STATE_IDLE:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
            break;
        case PEER_STATE_JOIN_RECV:
        case PEER_STATE_SELECT_SENT:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ACCEPTED:
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_ESTABLISHED);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_KEEPALIVE_ACK, 0, NULL);
            _sce_pspnet_pmp_settimer_keepalive(ctx, peer);
            peer->ping_retry = ctx->init_count;
            if (ctx->mode == MATCHING_MODE_CLIENT)
                _sce_pspnet_pmp_savemsg_keepaliverequest(ctx, peer->rx_pkt, peer->rx_wire_len);
            matchingInvokeHandler(ctx, MATCHING_EV_ESTABLISHED, peer->mac, 0, NULL);
            break;
        case PEER_STATE_ESTABLISHED:
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_KEEPALIVE_ACK, 0, NULL);
            peer->ping_retry = ctx->init_count;
            if (ctx->mode == MATCHING_MODE_CLIENT)
                _sce_pspnet_pmp_savemsg_keepaliverequest(ctx, peer->rx_pkt, peer->rx_wire_len);
            break;
        default:
            break;
        }
        break;
    case PKT_KEEPALIVE_ACK:
        if (peer->state == PEER_STATE_ESTABLISHED) {
            peer->ping_retry = ctx->init_count;
        } else if (peer->state == PEER_STATE_IDLE) {
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, peer->opt_len, peer->opt_buf);
        } else if (peer->state >= PEER_STATE_JOIN_RECV && peer->state < PEER_STATE_ESTABLISHED) {
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_ERROR, peer->mac, 0, NULL);
        }
        break;
    case PKT_BYE:
        if (peer->state >= PEER_STATE_JOIN_RECV && peer->state <= PEER_STATE_ESTABLISHED) {
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_stoptimer(peer);
        }
        matchingInvokeHandler(ctx, MATCHING_EV_BYE, peer->mac, 0, NULL);
        peer->pending_free = 1;
        break;
    case PKT_JOIN_ACK:
        if (peer->state == PEER_STATE_SELECT_SENT)
            peer->select_retry = ctx->init_count;
        /* When already established, fall through to DATA/DATA_ACK handling */
        if (peer->state != PEER_STATE_ESTABLISHED)
            break;
        /* fall through */
    case PKT_DATA:
    case PKT_DATA_ACK:
        if (peer->state != PEER_STATE_ESTABLISHED)
            break;
        sceKernelMemcpy(&pkt_sess_id, peer->rx_pkt + 4, 4);
        pkt_sess_id = matchingSwap32(pkt_sess_id);
        sceKernelMemcpy(&pkt_seq, peer->rx_pkt + 8, 4);
        pkt_seq = matchingSwap32(pkt_seq);
        if (pkt_sess_id != peer->remote_session_id)
            break;
        if (pkt_type == PKT_DATA) {
            if (pkt_seq >= peer->rx_expected_seq) {
                int datalen = peer->rx_wire_len - 12;
                void *data = peer->rx_pkt + 12;
                peer->rx_expected_seq = pkt_seq + 1;
                if (datalen > 0)
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA, peer->mac, datalen, data);
                else
                    matchingInvokeHandler(ctx, MATCHING_EV_DATA, peer->mac, 0, NULL);
            }
            matchingSendDataPkt(ctx, peer, PKT_DATA_ACK, 0, NULL);
        } else if (peer->data_state == DATA_STATE_SENDING && pkt_seq == peer->tx_seq) {
            matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
            matchingInvokeHandler(ctx, MATCHING_EV_DATA_CONFIRM, peer->mac, 0, NULL);
        }
        break;
    default:
        break;
    }
}

/* 0x002884 */
void __attribute__((noinline))
pmp_eventmsg_rexmttimer(MatchingCtx *ctx, MatchingPeer *peer)
{
    if (peer->state == PEER_STATE_SELECT_SENT) {
        if ((peer->peer_caps & 1u) != 0) {
            peer->select_retry--;
            if (peer->select_retry <= 0) {
                _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
                matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
            } else {
                _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
                _sce_pspnet_pmp_settimer_rexmt(ctx, peer);
            }
        } else {
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
            _sce_pspnet_pmp_settimer_rexmt(ctx, peer);
        }
    }
    if (peer->state == PEER_STATE_ACCEPTED) {
        peer->select_retry--;
        if (peer->select_retry <= 0) {
            _sce_pspnet_pmp_state_change(ctx, peer, PEER_STATE_IDLE);
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_CANCEL, 0, NULL);
            matchingInvokeHandler(ctx, MATCHING_EV_TIMEOUT, peer->mac, 0, NULL);
        } else {
            _sce_pspnet_pmp_sendmsg(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
            _sce_pspnet_pmp_settimer_rexmt(ctx, peer);
        }
    }
}

/* 0x002a6c */
