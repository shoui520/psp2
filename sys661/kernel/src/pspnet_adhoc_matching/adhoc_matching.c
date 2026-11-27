#include "pmpvar.h"

int g_vpl_id = -1;
int g_initialized;
int g_pool_max_alloc;
int g_mutex_created;
MatchingCtx *g_ctx_list;
int g_last_ctx_id;
u8 g_mutex_work[0x20];
u8 g_self_mac[6];


/* 0x000000 */
int sceNetAdhocMatchingInit(int poolsize)
{
    u8 mac[16];
    int ret;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized > 0)
        return MATCHING_ERR_ALREADY_INITIALIZED;

    ret = matchingCreateLock();
    if (ret < 0)
        return ret;
    ret = matchingCreateVpl(poolsize);
    if (ret >= 0) {
        ret = matchingInitCtxList();
        if (ret >= 0) {
            ret = sceWlanGetEtherAddr(mac);
            if (ret >= 0) {
                sceKernelMemcpy(g_self_mac, mac, 6);
                g_initialized = 1;
                return 0;
            }
            matchingFreeAllCtxs();
        }
        matchingDeleteVpl();
    }
    matchingDeleteLock();
    return ret;
}

/* 0x0000dc */
int sceNetAdhocMatchingCreate(int mode, int maxpeers, u16 port, int bufsize,
                              u32 hellodelay, u32 pingdelay, int initcount,
                              u32 msgdelay, SceNetAdhocMatchingHandler callback)
{
    int lock_res;
    int ret;

    if (sceKernelCheckThreadStack() < 3008)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ret = MATCHING_ERR_INVALID_MODE;
    if ((u32)(mode - 1) < 3u) {
        ret = MATCHING_ERR_INVALID_MAXNUM;
        if ((u32)(maxpeers - 2) < 15u) {
            ret = MATCHING_ERR_RXBUF_TOO_SHORT;
            if (bufsize >= maxpeers * 6 + 4) {
                ret = MATCHING_ERR_INVALID_ARG;
                if (((mode != MATCHING_MODE_HOST && mode != MATCHING_MODE_PTP) ||
                     (hellodelay != 0 && pingdelay != 0)) &&
                    initcount >= 0 && msgdelay != 0) {
                    int id = matchingAllocCtx(port);
                    ret = id;
                    if (id >= 0) {
                        MatchingCtx *ctx = matchingFindCtxById(id);
                        void *rx_buf;
                        ctx->mode = mode;
                        ctx->max_peers = (mode == MATCHING_MODE_HOST) ? maxpeers : 2;
                        ctx->port = port;
                        ctx->rx_buflen = bufsize;
                        rx_buf = matchingAllocVpl(bufsize);
                        ctx->rx_buf = (u8 *)rx_buf;
                        if (rx_buf != NULL) {
                            u32 cur_gp;
                            ctx->hello_delay = hellodelay;
                            ctx->ping_delay = pingdelay;
                            ctx->init_count = initcount;
                            ctx->msg_delay = msgdelay;
                            ctx->handler = callback;
                            ctx->hello_alarm_id = -1;
                            ctx->hello_trailer[0] = 1;
                            ctx->hello_trailer[1] = 0;
                            ctx->hello_trailer[2] = 0;
                            ctx->hello_trailer[3] = 0;
                            ctx->peers = NULL;
                            if (sceKernelGetModuleGPByAddress((void *)callback, &ctx->handler_gp) < 0) {
                                __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
                                ctx->handler_gp = cur_gp;
                            }
                            ret = id;
                        } else {
                            matchingFreeCtx(ctx);
                            ret = MATCHING_ERR_NO_SPACE;
                        }
                    }
                }
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000304 */
int sceNetAdhocMatchingStop(int matchingid)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            ctx->running = 1;
            matchingStopInputThread(ctx, &lock_res);
            matchingStopEventThread(ctx, &lock_res);
            if (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP) {
                matchingCancelHelloAlarm(ctx);
                matchingFreeHelloPkt(ctx);
                ctx->hello_msg.pending_flag &= ~1u;
            }
            matchingDestroyAllPeers(ctx);
            matchingFreeMembersPkt(ctx);
            matchingDeletePdp(ctx);
            ctx->running = 0;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000424 */
int sceNetAdhocMatchingDelete(int matchingid)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running != 0) {
            ret = MATCHING_ERR_IS_RUNNING;
        } else {
            matchingFreeVpl(ctx->rx_buf);
            ctx->rx_buf = NULL;
            matchingFreeCtx(ctx);
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0004f0 */
int sceNetAdhocMatchingSelectTarget(int matchingid, const u8 *target, int optlen, const void *opt)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx == NULL) {
        ret = MATCHING_ERR_INVALID_ID;
    } else if (ctx->running != 2) {
        ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
        if (peer == NULL) {
            ret = MATCHING_ERR_UNKNOWN_TARGET;
        } else if ((u32)optlen > 0xFFD7u || (optlen > 0 && opt == NULL)) {
            ret = MATCHING_ERR_INVALID_OPTLEN;
        } else {
            int active_count = matchingCountPeersAtLeast(ctx, PEER_STATE_ACCEPTED);
            switch (peer->state) {
            case PEER_STATE_IDLE:
                if (ctx->mode == MATCHING_MODE_HOST) {
                    ret = MATCHING_ERR_TARGET_NOT_READY;
                } else if (active_count + 1 >= ctx->max_peers) {
                    ret = MATCHING_ERR_EXCEED_MAXNUM;
                } else {
                    if (peer->opt_len > 0) {
                        matchingFreeVpl(peer->opt_buf);
                        peer->opt_len = 0;
                        peer->opt_buf = NULL;
                    }
                    if (optlen > 0) {
                        void *buf = matchingAllocVpl(optlen);
                        peer->opt_buf = buf;
                        if (buf == NULL) {
                            ret = MATCHING_ERR_NO_SPACE;
                            break;
                        }
                        sceKernelMemcpy(buf, opt, (u32)optlen);
                        peer->opt_len = optlen;
                    }
                    peer->own_session_id++;
                    if (peer->own_session_id == 0)
                        peer->own_session_id = 1;
                    matchingSendCtrlPkt(ctx, peer, PKT_JOIN, peer->opt_len, peer->opt_buf);
                    matchingSetCtrlRetryAlarm(ctx, peer);
                    matchingSetPeerState(ctx, peer, PEER_STATE_SELECT_SENT);
                    peer->select_retry = ctx->init_count;
                }
                break;
            case PEER_STATE_JOIN_RECV:
                if (active_count + 1 >= ctx->max_peers) {
                    ret = MATCHING_ERR_EXCEED_MAXNUM;
                } else {
                    if (peer->opt_len > 0) {
                        matchingFreeVpl(peer->opt_buf);
                        peer->opt_len = 0;
                        peer->opt_buf = NULL;
                    }
                    if (optlen > 0) {
                        void *buf = matchingAllocVpl(optlen);
                        peer->opt_buf = buf;
                        if (buf == NULL) {
                            ret = MATCHING_ERR_NO_SPACE;
                            break;
                        }
                        sceKernelMemcpy(buf, opt, (u32)optlen);
                        peer->opt_len = optlen;
                    }
                    peer->own_session_id++;
                    if (peer->own_session_id == 0)
                        peer->own_session_id = 1;
                    matchingSendCtrlPkt(ctx, peer, PKT_ACCEPT, peer->opt_len, peer->opt_buf);
                    matchingSetCtrlRetryAlarm(ctx, peer);
                    matchingSetPeerState(ctx, peer, PEER_STATE_ACCEPTED);
                    peer->select_retry = ctx->init_count;
                }
                break;
            case PEER_STATE_ACCEPTED:
            case PEER_STATE_SELECT_SENT:
                ret = MATCHING_ERR_REQUEST_IN_PROGRESS;
                break;
            case PEER_STATE_ESTABLISHED:
                ret = MATCHING_ERR_ALREADY_ESTABLISHED;
                break;
            default:
                break;
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0007f4 */
int sceNetAdhocMatchingSetHelloOpt(int matchingid, int optlen, const void *opt)
{
    int lock_res;
    int ret;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ret = MATCHING_ERR_INVALID_ID;
    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        ret = MATCHING_ERR_INVALID_MODE;
        if (ctx->mode != MATCHING_MODE_CLIENT) {
            ret = MATCHING_ERR_NOT_RUNNING;
            if (ctx->running == 2)
                ret = matchingBuildHelloPkt(ctx, optlen, opt);
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0008e0 */
int sceNetAdhocMatchingGetHelloOpt(int matchingid, int *optlen, void *opt)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (optlen == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->mode == MATCHING_MODE_CLIENT) {
            ret = MATCHING_ERR_INVALID_MODE;
        } else if (ctx->running == 2) {
            int len = ctx->hello_pkt_len - 12;
            if (opt != NULL && len > 0) {
                if (*optlen < len)
                    len = *optlen;
                sceKernelMemcpy(opt, ctx->hello_pkt + 12, (u32)len);
            }
            *optlen = len;
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000a08 */
int sceNetAdhocMatchingGetMembers(int matchingid, int *buflen, void *buf)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (((u32)buf & 3u) != 0)
        return MATCHING_ERR_UNALIGNED;
    if (buflen == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2)
            matchingGetMembersInternal(ctx, buflen, buf);
        else
            ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000b00 */
int sceNetAdhocMatchingSendData(int matchingid, const u8 *target, int datalen, const void *data)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
            if (peer != NULL) {
                if ((u32)(datalen - 1) > 0xFFD6u) {
                    ret = MATCHING_ERR_INVALID_DATALEN;
                } else if (data != NULL) {
                    if (peer->state == PEER_STATE_ESTABLISHED) {
                        if (peer->data_state == DATA_STATE_IDLE) {
                            void *buf;
                            if (peer->data_len > 0) {
                                matchingFreeVpl(peer->data_buf);
                                peer->data_len = 0;
                                peer->data_buf = NULL;
                            }
                            buf = matchingAllocVpl(datalen);
                            peer->data_buf = buf;
                            if (buf != NULL) {
                                sceKernelMemcpy(buf, data, (u32)datalen);
                                peer->data_len = datalen;
                                peer->tx_seq++;
                                matchingSendDataPkt(ctx, peer, PKT_DATA, datalen, peer->data_buf);
                                matchingSetDataRetryAlarm(ctx, peer);
                                matchingSetPeerDataState(ctx, peer, DATA_STATE_SENDING);
                                peer->data_retry = ctx->init_count;
                            } else {
                                ret = MATCHING_ERR_NO_SPACE;
                            }
                        } else if (peer->data_state == DATA_STATE_SENDING) {
                            ret = MATCHING_ERR_DATA_BUSY;
                        }
                    } else {
                        ret = MATCHING_ERR_NOT_ESTABLISHED;
                    }
                } else {
                    ret = MATCHING_ERR_INVALID_ARG;
                }
            } else {
                ret = MATCHING_ERR_UNKNOWN_TARGET;
            }
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000d00 */
int sceNetAdhocMatchingAbortSendData(int matchingid, const u8 *target)
{
    int lock_res;
    int ret = 0;
    MatchingCtx *ctx;

    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(matchingid);
    if (ctx != NULL) {
        if (ctx->running == 2) {
            MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
            if (peer != NULL) {
                matchingCancelDataAlarm(peer);
                matchingSetPeerDataState(ctx, peer, DATA_STATE_IDLE);
            } else {
                ret = MATCHING_ERR_UNKNOWN_TARGET;
            }
        } else {
            ret = MATCHING_ERR_NOT_RUNNING;
        }
    } else {
        ret = MATCHING_ERR_INVALID_ID;
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x000e08 */
int sceNetAdhocMatchingTerm(void)
{
    int ret;

    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return 0;

    ret = matchingCheckNoActiveCtx();
    if (ret >= 0) {
        matchingFreeAllCtxs();
        matchingDeleteVpl();
        matchingDeleteLock();
        g_initialized = 0;
    }
    return ret;
}

/* 0x000e88 */
int sceNetAdhocMatchingStart(int matchingid, int evthpri, int evthstack, int inthpri, int inthstack, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3024)
        return MATCHING_ERR_STACK_SMALL;
    return matchingStartInternal(matchingid, evthpri, evthstack, 2, inthpri, inthstack, 2, optlen, opt);
}

/* 0x000f34 */
int sceNetAdhocMatching_E8454C65(int matchingid, int evthpri, int evthstack, int evthmpid,
                                 int inthpri, int inthstack, int inthmpid, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3024)
        return MATCHING_ERR_STACK_SMALL;
    return matchingStartInternal(matchingid, evthpri, evthstack, evthmpid, inthpri, inthstack, inthmpid, optlen, opt);
}

/* 0x000ff0 */
int sceNetAdhocMatchingCancelTarget(int matchingid, const u8 *target)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    return matchingCancelTargetInternal(matchingid, target, 0, NULL);
}

/* 0x001050 */
int sceNetAdhocMatchingCancelTargetWithOpt(int matchingid, const u8 *target, int optlen, const void *opt)
{
    if (sceKernelCheckThreadStack() < 3040)
        return MATCHING_ERR_STACK_SMALL;
    return matchingCancelTargetInternal(matchingid, target, optlen, opt);
}

/* 0x0010c8 */
int sceNetAdhocMatchingGetPoolMaxAlloc(void)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    return matchingGetPoolMaxAllocInternal();
}

/* 0x001104 */
int sceNetAdhocMatchingGetPoolStat(MatchingPoolStat *poolstat)
{
    if (sceKernelCheckThreadStack() < 3056)
        return MATCHING_ERR_STACK_SMALL;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (poolstat == NULL)
        return MATCHING_ERR_INVALID_ARG;
    return matchingGetPoolStatInternal(poolstat);
}

/* 0x00116c */
int __attribute__((noinline))
matchingStartInternal(int id, int evth_prio, int evth_stack, int evth_mpid,
                      int inth_prio, int inth_stack, int inth_mpid, int optlen, const void *opt)
{
    int lock_res;
    MatchingCtx *ctx;
    int ret;

    (void)inth_stack;
    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (evth_stack < 3072)
        return MATCHING_ERR_STACKSIZE_TOO_SHORT;
    if ((evth_mpid != 2 && evth_mpid != 5) || (inth_mpid != 2 && inth_mpid != 5))
        return MATCHING_ERR_INVALID_ID;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(id);
    ret = MATCHING_ERR_INVALID_ID;
    if (ctx != NULL) {
        ret = MATCHING_ERR_IS_RUNNING;
        if (ctx->running == 0) {
            if (((u32)optlen <= 0xFFCFu && (optlen <= 0 || opt != NULL)) ||
                sceKernelGetCompiledSdkVersion() <= 0x05FFFFFF) {
                ret = matchingCreatePdp(ctx);
                if (ret >= 0) {
                    ret = matchingStartEventThread(ctx, evth_prio, evth_stack, evth_mpid);
                    if (ret >= 0) {
                        ret = matchingStartInputThread(ctx, inth_prio, 3072, inth_mpid);
                        if (ret >= 0) {
                            if (ctx->mode == MATCHING_MODE_HOST || ctx->mode == MATCHING_MODE_PTP) {
                                ret = matchingBuildHelloPkt(ctx, optlen, opt);
                                if (ret >= 0) {
                                    u32 now = sceKernelGetSystemTimeLow();
                                    matchingSetHelloAlarm(ctx, now / (0xFFFFFFFFu / ctx->hello_delay + 1u));
                                }
                            }
                            if (ret >= 0) {
                                matchingBuildMembersPkt(ctx);
                                ctx->min_stack = evth_stack - 3072;
                                ctx->running = 2;
                                matchingUnlock(lock_res);
                                return 0;
                            }
                            matchingStopInputThread(ctx, &lock_res);
                        }
                        matchingStopEventThread(ctx, &lock_res);
                    }
                    matchingDeletePdp(ctx);
                }
            } else {
                ret = MATCHING_ERR_INVALID_OPTLEN;
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x0013cc */
int __attribute__((noinline))
matchingCancelTargetInternal(int id, const u8 *target, int optlen, const void *opt)
{
    int lock_res;
    int ret = 0;
    void *opt_copy = NULL;
    MatchingCtx *ctx;

    if (g_initialized == 0)
        return MATCHING_ERR_NOT_INITIALIZED;
    if (target == NULL)
        return MATCHING_ERR_INVALID_ARG;

    lock_res = matchingLock();
    if (lock_res < 0)
        return lock_res;

    ctx = matchingFindCtxById(id);
    if (ctx == NULL) {
        ret = MATCHING_ERR_INVALID_ID;
    } else if (ctx->running != 2) {
        ret = MATCHING_ERR_NOT_RUNNING;
    } else {
        MatchingPeer *peer = matchingFindPeerByMac(ctx, target);
        if (peer == NULL) {
            ret = MATCHING_ERR_UNKNOWN_TARGET;
        } else if ((u32)optlen > 0xFFD7u || (optlen > 0 && opt == NULL)) {
            ret = MATCHING_ERR_INVALID_OPTLEN;
        } else if (peer->state != PEER_STATE_IDLE) {
            int ok = 1;
            if (peer->state > 0 && peer->state < 6 && optlen > 0) {
                opt_copy = matchingAllocVpl(optlen);
                if (opt_copy == NULL) {
                    ret = MATCHING_ERR_NO_SPACE;
                    ok = 0;
                } else {
                    sceKernelMemcpy(opt_copy, opt, (u32)optlen);
                }
            }
            if (ok) {
                matchingCancelPeerAlarms(peer);
                matchingSendCtrlPkt(ctx, peer, PKT_CANCEL, optlen, opt);
                matchingSetPeerState(ctx, peer, PEER_STATE_IDLE);
                if (peer->opt_len > 0) {
                    matchingFreeVpl(peer->opt_buf);
                    peer->opt_len = 0;
                    peer->opt_buf = NULL;
                }
                if (optlen > 0) {
                    peer->opt_buf = opt_copy;
                    peer->opt_len = optlen;
                }
            }
        }
    }

    matchingUnlock(lock_res);
    return ret;
}

/* 0x001598 */
