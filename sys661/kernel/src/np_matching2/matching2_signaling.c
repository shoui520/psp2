#include "np_matching2_internal.h"

/*
 * P2P signaling public API, signaling state machine, main thread, room/peer
 * signaling manager, session/request/connection pools (0x022458..0x027C6B).
 */

typedef struct NpSigStunAddrInfo {
    u32 mappedAddr;
    u16 mappedPort;
    u16 padWord;
} NpSigStunAddrInfo;

typedef struct NpSigConnInfoAddrPort {
    u32 addr;
    u16 port;
    u16 padWord;
} NpSigConnInfoAddrPort;

typedef struct NpSigRoomEvJoinData {
    NpMatching2RoomMember *member;
} NpSigRoomEvJoinData;

typedef struct NpSigRoomEvOwnerData {
    SceNpMatching2RoomMemberId prevOwner;
    SceNpMatching2RoomMemberId newOwner;
} NpSigRoomEvOwnerData;

static NpSigGlobal g_sigGlobal;
static s32 g_sigSessionCursorPool1 = 8;
static s32 g_sigSessionCursorPool2 = 24;
static s32 g_sigSessionCursorPool3 = 16;
static u32 g_sigReqIdCursor = 0;
static u32 g_sigConnIdCursor = 0;

/* Forward declarations for internal signaling handlers */
static s32 NP_MATCHING2_NOINLINE npSigDispatchPipeMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleConnectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleDisconnectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleStunSharedMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleStunSymMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleCandReadyMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleSignalSendResultMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleOfferMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleSelectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerCloseMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerKeepaliveMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleTimeoutMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleForceCloseMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleRetryTimerMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigFinalizeConn(NpSigGlobal *sig, NpSigConn *conn);
static s32 NP_MATCHING2_NOINLINE npSigSendLocalCands(NpSigGlobal *sig, NpSigConn *conn);
static s32 NP_MATCHING2_NOINLINE npSigStartConnect(NpSigGlobal *sig, NpSigConn *conn);
static s32 NP_MATCHING2_NOINLINE npSigDispatchAdminMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleSessionRemovedMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static void NP_MATCHING2_NOINLINE npSigHandlePollTimerMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigCancelPeerNetInfo(SceNpMatching2ContextId ctxId, u32 reqId);
static s32 NP_MATCHING2_NOINLINE npSigGetPeerNetInfoResult(SceNpMatching2ContextId ctxId, u32 reqId, void *resultOut);
static s32 NP_MATCHING2_NOINLINE npSigOpenPeerConn(s32 sessionId, const SceNpId *peerNpId, u32 *connIdInOut);
static s32 NP_MATCHING2_NOINLINE npSigQueryConnStatus(s32 sessionId, u32 connId, s32 *statusOut, u32 *addrOut, u16 *portOut);
static s32 NP_MATCHING2_NOINLINE npSigQueryConnInfo(s32 sessionId, u32 connId, s32 code, void *infoOut);
static s32 NP_MATCHING2_NOINLINE npSigDetachAndCloseConn(s32 sessionId, u32 connId);
static s32 NP_MATCHING2_NOINLINE npSigStopMainThread(NpSigGlobal *sig);
static s32 NP_MATCHING2_NOINLINE npSigStopEchoThread(NpSigGlobal *sig);
static s32 NP_MATCHING2_NOINLINE npSigMainThread(u32 args, void *argp);
static s32 NP_MATCHING2_NOINLINE npSigGetConnStatus(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                    SceNpMatching2RoomMemberId memberId, s32 *statusOut, u32 *addrOut, u16 *portOut);
static s32 NP_MATCHING2_NOINLINE npSigGetConnInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                  SceNpMatching2RoomMemberId memberId, s32 code, void *infoOut);
static void NP_MATCHING2_NOINLINE npSigSyncRoomPeers(NpSigGlobal *sig, NpSigSession *session, NpSigRoom *room);
static s32 NP_MATCHING2_NOINLINE npSigReqListener(SceNpMatching2ContextId ctxId, SceNpMatching2RequestId reqId,
                                                  SceNpMatching2Event eventId, s32 errorCode, const void *eventData);
static void NP_MATCHING2_NOINLINE npSigRoomEvListener(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                      SceNpMatching2Event eventId, const void *eventData);
static s32 NP_MATCHING2_NOINLINE npSigSessionCb(s32 sessionId, u32 connId, u16 eventId, s32 errorCode, void *cbArg);
static void NP_MATCHING2_NOINLINE npSigOnRoomJoined(NpSigGlobal *sig, NpSigSession *session, const NpMatching2Room *room);
static s32 NP_MATCHING2_NOINLINE npSigOnRoomLeft(NpSigGlobal *sig, NpSigSession *session, u32 roomIdLo, u32 roomIdHi);

/* 0x022458 */
s32 sceNpMatching2SignalingGetConnectionStatus(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                               SceNpMatching2RoomMemberId memberId, s32 *connStatus,
                                               u32 *peerAddr, u16 *peerPort)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigGetConnStatus(ctxId, roomId, memberId, connStatus, peerAddr, peerPort);
}

/* 0x022518 */
s32 sceNpMatching2SignalingGetConnectionInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                             SceNpMatching2RoomMemberId memberId, s32 code, void *info)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigGetConnInfo(ctxId, roomId, memberId, code, info);
}

/* 0x0225C8 */
s32 sceNpMatching2SignalingGetLocalNetInfo(NpSigLocalNetInfo *netInfo)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigGetLocalNetInfo(netInfo);
}

/* 0x022628 */
s32 sceNpMatching2SignalingGetPeerNetInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                          SceNpMatching2RoomMemberId memberId, u32 *reqId)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    if (reqId == NULL) {
        return (s32)0x80550e15;
    }
    return npSigStartPeerNetInfo(ctxId, roomId, memberId, reqId);
}

/* 0x0226D0 */
s32 sceNpMatching2SignalingCancelPeerNetInfo(SceNpMatching2ContextId ctxId, u32 reqId)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigCancelPeerNetInfo(ctxId, reqId);
}

/* 0x022740 */
s32 sceNpMatching2SignalingGetPeerNetInfoResult(SceNpMatching2ContextId ctxId, u32 reqId, void *result)
{
    if (sceKernelCheckThreadStack() < 4080) {
        return SCE_NP_ERROR_STACK_OVERFLOW;
    }
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigGetPeerNetInfoResult(ctxId, reqId, result);
}

/* 0x0227C0 */
static s32 NP_MATCHING2_NOINLINE npSigDispatchPipeMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;

    switch (localMsg.msgKind) {
        case 5:
            return npSigHandleConnectMsg(sig, &localMsg);
        case 6:
            return npSigHandleDisconnectMsg(sig, &localMsg);
        case 7:
        case 8:
            return npSigHandleStunSharedMsg(sig, &localMsg);
        case 9:
        case 10:
            return npSigHandleSignalSendResultMsg(sig, &localMsg);
        case 11:
            return npSigHandleOfferMsg(sig, &localMsg);
        case 12:
            return npSigHandleSelectMsg(sig, &localMsg);
        case 13:
        case 32:
            return npSigHandlePeerCloseMsg(sig, &localMsg);
        case 14:
            return npSigHandleTimeoutMsg(sig, &localMsg);
        case 21:
            return npSigHandleCandReadyMsg(sig, &localMsg);
        case 22:
            return npSigHandleForceCloseMsg(sig, &localMsg);
        case 25:
            return npSigHandleRetryTimerMsg(sig, &localMsg);
        case 30:
        case 31:
            return npSigHandleStunSymMsg(sig, &localMsg);
        case 33:
        case 34:
            return npSigHandlePeerKeepaliveMsg(sig, &localMsg);
        default:
            return 0;
    }
}

/* 0x022920 */
static s32 NP_MATCHING2_NOINLINE npSigHandleConnectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    u32 connId = msg->targetId;
    s32 sessionId = (s32)msg->auxWord;
    NpSigSession *session = NULL;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindSession(sig, sessionId, &session);
    if (ret >= 0) {
        ret = npSigFindConnById(sig, connId, &conn);
        if (ret >= 0) {
            if (conn->state == 1) {
                conn->connectBudgetUsec = 30000000;
                ret = npSigStartConnect(sig, conn);
            } else if (conn->state == 10) {
                sceNet_lib_A93A93E9(&conn->connectTimer);
                npSigNotifyCb(session, conn->connId, 0x5102, 0);
                if ((conn->flags & 0x40u) == 0) {
                    ret = npSigSendSimpleSignal(conn, 7);
                    if (ret >= 0) {
                        conn->flags |= 0x40u;
                    }
                }
                if (ret >= 0 && (conn->flags & 0x80u) != 0) {
                    npSigNotifyCb(session, conn->connId, 0x5106, 0);
                }
            }
        }
    }

    if (session != NULL) {
        if (conn != NULL && ret < 0) {
            npSigCloseConn(sig, conn, ret, 3);
        }
        npSigReleaseSession(sig, session);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x022AC8 */
static s32 NP_MATCHING2_NOINLINE npSigHandleDisconnectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0 && conn->state == 10 && conn->sessionRefs == NULL) {
        sceNet_lib_A93A93E9(&conn->connectTimer);
        sceNet_lib_A8B6205A(&conn->connectTimer, 60000000, npSigTimerTimeoutCb, conn->connId);
        if ((conn->flags & 0x40u) != 0) {
            ret = npSigSendSimpleSignal(conn, 8);
            if (ret >= 0) {
                conn->flags &= ~0x40u;
            }
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x022BE8 */
static s32 NP_MATCHING2_NOINLINE npSigHandleStunSharedMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg outMsg = *msg;
    NpSigConn *conn;
    s32 lockRet;
    s32 ret = 0;

    if (outMsg.msgKind == 7) {
        sig->stunMappedAddr = outMsg.payloadWord;
        sig->stunMappedPort = (u16)outMsg.auxWord;
        sceNp_6CBB0614(&sig->stunCacheExpireTick);
        sig->stunCacheExpireTick += 30000000ULL;
        sig->stunState = 2;
    } else if (outMsg.msgKind == 8) {
        sig->stunState = 0;
    }

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (conn = sig->connList; conn != NULL; conn = conn->next) {
        if (conn->state == 3) {
            u32 sentBytes = 0;
            outMsg.msgKind = 21;
            outMsg.targetId = conn->connId;
            ret = sceKernelSendMsgPipe(sig->msgPipe, &outMsg, sizeof(outMsg), 0, &sentBytes, NULL);
            if (ret < 0) {
                break;
            }
        }
    }
    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x022D40 */
static s32 NP_MATCHING2_NOINLINE npSigHandleStunSymMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, localMsg.targetId, &conn);
    if (ret >= 0) {
        if (localMsg.msgKind == 30) {
            conn->flags &= ~8u;
            conn->symMappedPort = (u16)localMsg.auxWord;
        } else if (localMsg.msgKind == 31) {
            conn->flags &= ~8u;
            ret = npSigCloseSymmetricSocks(sig, conn);
        }

        if (conn->state == 4) {
            u32 sentBytes = 0;
            localMsg.msgKind = 21;
            localMsg.targetId = conn->connId;
            ret = sceKernelSendMsgPipe(sig->msgPipe, &localMsg, sizeof(localMsg), 0, &sentBytes, NULL);
        }
    }

    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x022E50 */
static s32 NP_MATCHING2_NOINLINE npSigHandleCandReadyMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0 && (conn->state == 3 || conn->state == 4)) {
        ret = npSigSendLocalCands(sig, conn);
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x022F14 */
static s32 NP_MATCHING2_NOINLINE npSigHandleSignalSendResultMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnByTxId(sig, localMsg.targetId, &conn);
    if (ret >= 0) {
        conn->lastTxId = 0;
        if (localMsg.msgKind == 9) {
            if (conn->state == 5) {
                if ((conn->flags & 2u) == 0) {
                    conn->state = 6;
                } else {
                    u32 sentBytes = 0;
                    conn->state = 7;
                    sceNp_6CBB0614(&conn->echoDeadlineTick);
                    conn->echoDeadlineTick += 10000000ULL;
                    localMsg.msgKind = 15;
                    localMsg.targetId = conn->connId;
                    ret = sceKernelSendMsgPipe(sig->msgPipe, &localMsg, sizeof(localMsg), 0, &sentBytes, NULL);
                }
            } else if (conn->state == 8) {
                if ((conn->flags & 4u) == 0) {
                    conn->state = 9;
                } else {
                    ret = npSigFinalizeConn(sig, conn);
                }
            }
        } else if (localMsg.msgKind == 10) {
            if (conn->state == 5 || conn->state == 8) {
                ret = (s32)localMsg.payloadWord;
            }
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x0230AC */
static s32 NP_MATCHING2_NOINLINE npSigHandleOfferMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;
    NpSigSignalPayload *payload = (NpSigSignalPayload *)localMsg.payloadWord;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, localMsg.targetId, &conn);
    if (ret >= 0) {
        NpSigCand *cand;
        conn->peerSessionTag = payload->srcTag;
        sceKernelMemcpy(conn->peerKey, payload->sessionKey, 16);
        conn->peerNatType = payload->natType;
        if (conn->candList != NULL) {
            npSigFreeCandList(conn->candList);
        }
        conn->candList = payload->candList;
        payload->candList = NULL;
        conn->flags |= 2u;

        for (cand = conn->candList; cand != NULL; cand = cand->next) {
            cand->sockFd = -1;
        }

        if (conn->state == 1) {
            conn->connectBudgetUsec = 30000000;
            ret = npSigStartConnect(sig, conn);
        } else if (conn->state == 6) {
            u32 sentBytes = 0;
            conn->state = 7;
            sceNp_6CBB0614(&conn->echoDeadlineTick);
            conn->echoDeadlineTick += 10000000ULL;
            localMsg.msgKind = 15;
            localMsg.targetId = conn->connId;
            ret = sceKernelSendMsgPipe(sig->msgPipe, &localMsg, sizeof(localMsg), 0, &sentBytes, NULL);
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (payload != NULL) {
        if (payload->candList != NULL) {
            npSigFreeCandList(payload->candList);
        }
        npMatching2Free(payload);
    }
    return ret;
}

/* 0x0232AC */
static s32 NP_MATCHING2_NOINLINE npSigHandleSelectMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigSignalPayload *payload = (NpSigSignalPayload *)msg->payloadWord;
    NpSigConn *conn = NULL;
    s32 npCmp = 0;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        ret = sceNp_2D8E93D2(&conn->selfNpId, &conn->peerNpId, &npCmp);
        if (ret >= 0) {
            NpSigCand *cand;
            if (npCmp < 0) {
                NpSigCand *selCand = payload->candList;
                conn->localMappedAddr = selCand->peerAddr;
                conn->localMappedPort = selCand->peerPort;
                conn->peerSelectedAddr = selCand->mappedAddr;
                conn->peerSelectedPort = selCand->mappedPort;
            }

            conn->selectedSockFd = -1;
            for (cand = conn->candList; cand != NULL; cand = cand->next) {
                if (cand->peerAddr == conn->peerSelectedAddr && cand->peerPort == conn->peerSelectedPort) {
                    conn->selectedSockFd = cand->sockFd;
                    conn->selectedLocalPort = cand->localPort;
                }
            }

            if (conn->selectedSockFd < 0) {
                conn->selectedSockFd = sig->sharedSigSock;
                conn->selectedLocalPort = sig->sharedBindPort;
                if (conn->symSigSock >= 0 && payload->candList->candType == 3) {
                    conn->selectedSockFd = conn->symSigSock;
                    conn->selectedLocalPort = conn->symLocalPort;
                }
            }

            conn->flags |= 4u;
            if (conn->state == 9) {
                ret = npSigFinalizeConn(sig, conn);
            }
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (payload != NULL) {
        if (payload->candList != NULL) {
            npSigFreeCandList(payload->candList);
        }
        npMatching2Free(payload);
    }
    return ret;
}

/* 0x02347C */
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerCloseMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigSignalPayload *payload = (NpSigSignalPayload *)msg->payloadWord;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        if (msg->msgKind == 13) {
            ret = npSigFindConnById(sig, msg->targetId, &conn);
        } else if (msg->msgKind == 32) {
            ret = npSigFindConnByIdAll(sig, msg->targetId, &conn);
        }
        if (ret >= 0) {
            ret = (s32)0x80550e10;
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (payload != NULL) {
        if (payload->candList != NULL) {
            npSigFreeCandList(payload->candList);
        }
        npMatching2Free(payload);
    }
    return ret;
}

/* 0x02359C */
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerKeepaliveMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigSignalPayload *payload = (NpSigSignalPayload *)msg->payloadWord;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        if (msg->msgKind == 33) {
            conn->flags |= 0x80u;
            if (conn->state == 10) {
                npSigNotifyConnEvent(conn, 0x5104, 0);
                npSigNotifyConnEvent(conn, 0x5106, 0);
            }
        } else if (msg->msgKind == 34) {
            conn->flags &= ~0x80u;
            if (conn->state == 10) {
                npSigNotifyConnEvent(conn, 0x5105, 0);
            }
        }
    }

    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 2);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (payload != NULL) {
        if (payload->candList != NULL) {
            npSigFreeCandList(payload->candList);
        }
        npMatching2Free(payload);
    }
    return ret;
}

/* 0x0236FC */
static s32 NP_MATCHING2_NOINLINE npSigHandleTimeoutMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        ret = (s32)0x80550e11;
    }
    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x0237AC */
static s32 NP_MATCHING2_NOINLINE npSigHandleForceCloseMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0 && msg->msgKind == 22) {
        ret = (s32)0x80550e18;
    }
    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, (msg->msgKind == 22) ? 3 : 0);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x023880 */
static s32 NP_MATCHING2_NOINLINE npSigHandleRetryTimerMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0 && conn->state == 2) {
        ret = npSigStartConnect(sig, conn);
    }
    if (conn != NULL && ret < 0) {
        npSigCloseConn(sig, conn, ret, 3);
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x023944 */
static s32 NP_MATCHING2_NOINLINE npSigFinalizeConn(NpSigGlobal *sig, NpSigConn *conn)
{
    NpSigIoctlPeerParam peerParam;
    s32 sockFd;
    s32 ret;

    sceNet_lib_A93A93E9(&conn->connectTimer);
    sceKernelMemset(&peerParam, 0, sizeof(peerParam));
    peerParam.peerPort = conn->peerSelectedPort;
    peerParam.peerAddr = conn->peerSelectedAddr;
    peerParam.localPort = conn->selectedLocalPort;
    sceKernelMemcpy(peerParam.peerKey, conn->peerKey, 16);

    sockFd = sceNetInetSocket(2, 6, 0);
    if (sockFd < 0) {
        return sceNetInetGetPspError();
    }
    ret = sceNetInet_lib_AEE60F84(sockFd, (s32)0x802450CB, &peerParam);
    sceNetInetClose(sockFd);
    if (ret < 0) {
        return ret;
    }

    conn->state = 10;
    npSigNotifyConnEvent(conn, 0x5102, 0);
    if (conn->sessionRefs != NULL) {
        if ((conn->flags & 0x40u) == 0) {
            ret = npSigSendSimpleSignal(conn, 7);
            if (ret < 0) {
                return ret;
            }
            conn->flags |= 0x40u;
        }
    } else {
        sceNet_lib_A8B6205A(&conn->connectTimer, 60000000, npSigTimerTimeoutCb, conn->connId);
    }
    if ((conn->flags & 0x80u) != 0) {
        npSigNotifyConnEvent(conn, 0x5106, 0);
    }
    return npSigSendBandwidthProbe(sig, conn);
}

/* 0x023AC4 */
static s32 NP_MATCHING2_NOINLINE npSigSendLocalCands(NpSigGlobal *sig, NpSigConn *conn)
{
    NpSigCand *candList = NULL;
    NpSigCand *cand = NULL;
    NpSigUpnpInfo upnpInfo;
    NpSigSignalPayload payload;
    u32 localIp = 0;
    s32 ret;

    sceKernelMemset(&upnpInfo, 0, sizeof(upnpInfo));
    sceNetUpnp_1038E77A(&upnpInfo);
    conn->localSessionTag = (u16)conn->connId;
    sceKernelMemcpy(conn->localKey, sig->localKey, 16);

    cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
    if (cand == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(cand, 0, sizeof(NpSigCand));
    cand->candType = 1;
    ret = npSigGetLocalIpAddr(&localIp);
    if (ret < 0) {
        npMatching2Free(cand);
        return ret;
    }
    cand->peerAddr = localIp;
    cand->peerPort = sig->sharedBindPort;
    candList = cand;
    cand = NULL;

    if (upnpInfo.natDetectStatus == 2) {
        if (upnpInfo.upnpMapped == 2) {
            if (npSigFindCandByAddr(candList, upnpInfo.extAddr, sig->sharedBindPort) == NULL) {
                cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                if (cand == NULL) {
                    npSigFreeCandList(candList);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemset(cand, 0, sizeof(NpSigCand));
                cand->candType = 4;
                cand->peerAddr = upnpInfo.extAddr;
                cand->peerPort = sig->sharedBindPort;
                cand->next = candList;
                candList = cand;
                cand = NULL;
            }
        } else {
            if (upnpInfo.upnpStatus == 2) {
                u32 upnpExtIp = ((u32)upnpInfo.upnpAddrHi << 16) | (u32)upnpInfo.upnpAddrLo;
                if (npSigFindCandByAddr(candList, upnpExtIp, sig->sharedBindPort) == NULL) {
                    cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                    if (cand == NULL) {
                        npSigFreeCandList(candList);
                        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                    }
                    sceKernelMemset(cand, 0, sizeof(NpSigCand));
                    cand->candType = 4;
                    cand->peerAddr = upnpExtIp;
                    cand->peerPort = sig->sharedBindPort;
                    cand->next = candList;
                    candList = cand;
                    cand = NULL;
                }
            }

            if (conn->symSigSock < 0) {
                if (sig->stunState == 2 &&
                    npSigFindCandByAddr(candList, sig->stunMappedAddr, sig->stunMappedPort) == NULL) {
                    cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                    if (cand == NULL) {
                        npSigFreeCandList(candList);
                        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                    }
                    sceKernelMemset(cand, 0, sizeof(NpSigCand));
                    cand->candType = 3;
                    cand->peerAddr = sig->stunMappedAddr;
                    cand->peerPort = sig->stunMappedPort;
                    cand->next = candList;
                    candList = cand;
                    cand = NULL;
                }
            } else if (conn->symMappedPort != 0) {
                if (npSigFindCandByAddr(candList, upnpInfo.extAddr, conn->symMappedPort) == NULL) {
                    cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                    if (cand == NULL) {
                        npSigFreeCandList(candList);
                        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                    }
                    sceKernelMemset(cand, 0, sizeof(NpSigCand));
                    cand->candType = 3;
                    cand->peerAddr = upnpInfo.extAddr;
                    cand->peerPort = conn->symMappedPort;
                    cand->next = candList;
                    candList = cand;
                    cand = NULL;
                }
                {
                    s32 step;
                    for (step = 1; step > 0; step--) {
                        u16 predPort = npSwap16((u16)(npSwap16(conn->symMappedPort) + (u16)(step * upnpInfo.portDelta)));
                        if (npSigFindCandByAddr(candList, upnpInfo.extAddr, predPort) == NULL) {
                            cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                            if (cand == NULL) {
                                npSigFreeCandList(candList);
                                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                            }
                            sceKernelMemset(cand, 0, sizeof(NpSigCand));
                            cand->candType = 3;
                            cand->peerAddr = upnpInfo.extAddr;
                            cand->peerPort = predPort;
                            cand->next = candList;
                            candList = cand;
                            cand = NULL;
                        }
                    }
                }
            } else if (npSigFindCandByAddr(candList, upnpInfo.extAddr, conn->symLocalPort) == NULL) {
                cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                if (cand == NULL) {
                    npSigFreeCandList(candList);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemset(cand, 0, sizeof(NpSigCand));
                cand->candType = 3;
                cand->peerAddr = upnpInfo.extAddr;
                cand->peerPort = conn->symLocalPort;
                cand->next = candList;
                candList = cand;
                cand = NULL;
            }
        }
    }

    sceKernelMemset(&payload, 0, sizeof(payload));
    payload.srcTag = conn->localSessionTag;
    payload.dstTag = conn->peerSessionTag;
    sceKernelMemcpy(payload.sessionKey, conn->localKey, 16);
    payload.natType = upnpInfo.natType;
    payload.candList = candList;

    ret = npSigSendSignalPacket(conn, 1, &payload);
    if (ret >= 0) {
        conn->state = 5;
    }
    if (candList != NULL) {
        npSigFreeCandList(candList);
    }
    return ret;
}

/* 0x023EF4 */
static s32 NP_MATCHING2_NOINLINE npSigStartConnect(NpSigGlobal *sig, NpSigConn *conn)
{
    NpSigUpnpInfo upnpInfo;
    u64 nowTick = 0;
    s32 ret = 0;

    sceKernelMemset(&upnpInfo, 0, sizeof(upnpInfo));
    sceNetUpnp_1038E77A(&upnpInfo);

    if (upnpInfo.natDetectStatus == 0) {
        conn->state = 2;
        if (conn->connectBudgetUsec < 100000u) {
            return (s32)0x80550e11;
        }
        sceNet_lib_A8B6205A(&conn->connectTimer, 100000, npSigTimerRetryCb, conn->connId);
        conn->connectBudgetUsec -= 100000;
        return 0;
    }

    if (sig->stunHandle != 0 && sig->sharedUdpSock >= 0 &&
        upnpInfo.natDetectStatus == 2 && upnpInfo.upnpMapped != 2 && upnpInfo.isSymmetric == 0) {
        sceNp_6CBB0614(&nowTick);
        if (sig->stunState == 2 && nowTick < sig->stunCacheExpireTick) {
            ret = npSigSendLocalCands(sig, conn);
            if (ret < 0) {
                return ret;
            }
        } else {
            if (sig->stunState != 1) {
                ret = sceNetStun_6CA54FC9(sig->stunHandle, sig->sharedUdpSock, 0);
                if (ret < 0) {
                    return ret;
                }
                sig->stunState = 1;
            }
            conn->state = 3;
        }
    } else if (upnpInfo.natDetectStatus != 2 || upnpInfo.upnpMapped == 2 || upnpInfo.isSymmetric == 0) {
        ret = npSigSendLocalCands(sig, conn);
        if (ret < 0) {
            return ret;
        }
    } else if (upnpInfo.symKind == 1) {
        npSigOpenSymmetricSocks(sig, conn);
        ret = npSigSendLocalCands(sig, conn);
        if (ret < 0) {
            return ret;
        }
    } else if (sig->symStunBusy == 1) {
        conn->state = 2;
        if (conn->connectBudgetUsec < 100000u) {
            return (s32)0x80550e11;
        }
    } else {
        conn->flags |= 8u;
        npSigOpenSymmetricSocks(sig, conn);
        ret = sceNetStun_6CA54FC9(sig->stunHandle, conn->symUdpSock, conn->connId);
        if (ret < 0) {
            npSigCloseSymmetricSocks(sig, conn);
            conn->flags &= ~8u;
            ret = npSigSendLocalCands(sig, conn);
            if (ret < 0) {
                return ret;
            }
        }
        sig->symStunBusy = 1;
        conn->state = 4;
        conn->flags |= 0x10u;
    }

    if (conn->state == 2) {
        sceNet_lib_A8B6205A(&conn->connectTimer, 100000, npSigTimerRetryCb, conn->connId);
        conn->connectBudgetUsec -= 100000;
    } else {
        sceNet_lib_A8B6205A(&conn->connectTimer, conn->connectBudgetUsec, npSigTimerTimeoutCb, conn->connId);
    }
    return ret;
}

/* 0x02418C */
static s32 NP_MATCHING2_NOINLINE npSigDispatchAdminMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;

    if (localMsg.msgKind == 20) {
        return npSigHandleSessionRemovedMsg(sig, &localMsg);
    }
    if (localMsg.msgKind == 26) {
        npSigHandlePollTimerMsg(sig, &localMsg);
    }
    return 0;
}

/* 0x0241F8 */
static s32 NP_MATCHING2_NOINLINE npSigHandleSessionRemovedMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    s32 removedSessionId = (s32)msg->auxWord;
    s32 lockRet;
    NpSigConn *conn;
    NpSigReq *reqNode;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);

    conn = sig->connList;
    while (conn != NULL) {
        NpSigConn *nextConn = conn->next;
        NpSigSessionRef *ref = conn->sessionRefs;
        NpSigSessionRef *prevRef = NULL;

        conn->refCount++;
        while (ref != NULL && ref->sessionId != removedSessionId) {
            prevRef = ref;
            ref = ref->next;
        }
        if (ref != NULL) {
            if (prevRef != NULL) {
                prevRef->next = ref->next;
            } else {
                conn->sessionRefs = ref->next;
            }
            npMatching2Free(ref);
            if (conn->sessionRefs == NULL) {
                npSigCloseConn(sig, conn, (s32)0x80550e10, 1);
            }
        }
        npSigReleaseConn(sig, conn);
        conn = nextConn;
    }

    reqNode = sig->reqList;
    while (reqNode != NULL) {
        NpSigReq *nextReq = reqNode->next;
        reqNode->refCount++;
        if (reqNode->sessionId == removedSessionId && (reqNode->reqId & 0x21000000u) == 0x21000000u) {
            npSigDeleteReqById(sig, reqNode->reqId);
        }
        npSigReleaseReq(sig, reqNode);
        reqNode = nextReq;
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return lockRet;
}

/* 0x02439C */
static void NP_MATCHING2_NOINLINE npSigHandlePollTimerMsg(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    SceNetInetFdSet readFds;
    SceNetInetTimeval zeroTv;
    u8 drainBuf[2048];
    s32 lockRet;
    NpSigConn *symConn;

    (void)msg;
    if (sig->echoThid <= 0) {
        return;
    }

    if (sig->stunHandle != 0 && sig->sharedUdpSock >= 0 && sig->stunState != 1) {
        s32 recvRet;
        do {
            sceKernelMemset(&readFds, 0, sizeof(readFds));
            readFds.fdsBits[sig->sharedUdpSock >> 5] |= (1u << (sig->sharedUdpSock & 31));
            zeroTv.tvSec = 0;
            zeroTv.tvUsec = 0;
            if (sceNetInetSelect(sig->sharedUdpSock + 1, &readFds, NULL, NULL, &zeroTv) <= 0) {
                break;
            }
            recvRet = sceNetInetRecvfrom(sig->sharedUdpSock, drainBuf, sizeof(drainBuf), 0, NULL, NULL);
        } while (recvRet >= 0);
        sceNetInetGetPspError();
    }

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (symConn = sig->symConnList; symConn != NULL; symConn = symConn->symNext) {
        if (symConn->symUdpSock >= 0 && (symConn->flags & 8u) == 0) {
            do {
                sceKernelMemset(&readFds, 0, sizeof(readFds));
                readFds.fdsBits[symConn->symUdpSock >> 5] |= (1u << (symConn->symUdpSock & 31));
                zeroTv.tvSec = 0;
                zeroTv.tvUsec = 0;
            } while (sceNetInetSelect(symConn->symUdpSock + 1, &readFds, NULL, NULL, &zeroTv) > 0 &&
                     sceNetInetRecvfrom(symConn->symUdpSock, drainBuf, sizeof(drainBuf), 0, NULL, NULL) >= 0);
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }

    sceNet_lib_A8B6205A(&sig->pollTimer, 200000, npSigTimerPollCb, 0);
}

/* 0x0245C0 */
s32 NP_MATCHING2_NOINLINE npSigInit(u32 stackSize, s32 priority)
{
    NpSigGlobal *sig = &g_sigGlobal;
    u32 threadOpt[2];
    s32 sockFd;
    s32 ret;

    sceKernelMemset(sig, 0, sizeof(NpSigGlobal));

    ret = sceKernelCreateMutex("SceNpSignaling", 512, 0, NULL);
    if (ret >= 0) {
        sig->mainMutex = ret;
        ret = sceKernelCreateMutex("SceNpSignaling2", 512, 0, NULL);
        if (ret >= 0) {
            sig->selectMutex = ret;
            ret = sceKernelCreateMsgPipe("SceNpSignaling", 2, 0, (void *)1024, NULL);
            if (ret >= 0) {
                sig->msgPipe = ret;
                ret = sceKernelCreateMbx("SceNpSignaling", 0, NULL);
                if (ret >= 0) {
                    sig->echoMbx = ret;
                    sockFd = sceNetInetSocket(2, 6, 0);
                    if (sockFd < 0) {
                        ret = sceNetInetGetPspError();
                    } else {
                        ret = sceNetInet_lib_AEE60F84(sockFd, 0x200050C8, NULL);
                        sceNetInetClose(sockFd);
                        if (ret >= 0) {
                            sceNpCore_52440ABF(sig->clientVer);
                            sig->priority = priority;
                            sig->stackSize = stackSize;
                            sceKernelMemset(threadOpt, 0, sizeof(threadOpt));
                            threadOpt[0] = sizeof(threadOpt);
                            ret = sceKernelCreateThread("SceNpSignalingMain", npSigMainThread, priority, stackSize, 0, threadOpt);
                            if (ret >= 0) {
                                sig->mainThid = ret;
                                ret = sceKernelStartThread(sig->mainThid, sizeof(sig), &sig);
                                if (ret >= 0) {
                                    sceNetFreeThreadinfo(sceKernelGetThreadId());
                                    return ret;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (sig->mainThid > 0) {
        sceKernelTerminateThread(sig->mainThid);
        sceKernelDeleteThread(sig->mainThid);
        sceNetFreeThreadinfo(sig->mainThid);
        sig->mainThid = 0;
    }
    if (sig->echoMbx > 0) {
        sceKernelDeleteMbx(sig->echoMbx);
        sig->echoMbx = 0;
    }
    if (sig->msgPipe > 0) {
        sceKernelDeleteMsgPipe(sig->msgPipe);
        sig->msgPipe = 0;
    }
    if (sig->selectMutex > 0) {
        sceKernelDeleteMutex(sig->selectMutex);
        sig->selectMutex = 0;
    }
    if (sig->mainMutex > 0) {
        sceKernelDeleteMutex(sig->mainMutex);
        sig->mainMutex = 0;
    }
    sceNetFreeThreadinfo(sceKernelGetThreadId());
    return ret;
}

/* 0x02483C */
s32 NP_MATCHING2_NOINLINE npSigTerm(void)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigSession *session;
    NpSigReq *reqNode;
    NpSigConn *conn;
    NpSigPeerNetInfoNode *infoNode;
    s32 sockFd;

    npSigStopEchoThread(sig);
    npSigStopMainThread(sig);

    session = sig->sessionList;
    while (session != NULL) {
        NpSigSession *next = session->next;
        npMatching2Free(session);
        session = next;
    }

    reqNode = sig->reqList;
    while (reqNode != NULL) {
        NpSigReq *next = reqNode->next;
        npMatching2Free(reqNode);
        reqNode = next;
    }

    conn = sig->connList;
    while (conn != NULL) {
        NpSigConn *next = conn->next;
        if (conn->candList != NULL) {
            npSigFreeCandList(conn->candList);
        }
        if (conn->sessionRefs != NULL) {
            npSigFreeRefList(conn->sessionRefs);
        }
        npMatching2Free(conn);
        conn = next;
    }

    infoNode = sig->peerNetInfoList;
    while (infoNode != NULL) {
        NpSigPeerNetInfoNode *next = infoNode->next;
        npMatching2Free(infoNode);
        infoNode = next;
    }

    sockFd = sceNetInetSocket(2, 6, 0);
    if (sockFd < 0) {
        sceNetInetGetPspError();
    } else {
        sceNetInet_lib_AEE60F84(sockFd, 0x200050C8, NULL);
        sceNetInetClose(sockFd);
    }

    if (sig->echoMbx > 0) {
        void *msgPtr = NULL;
        while (sceKernelPollMbx(sig->echoMbx, &msgPtr) == 0) {
            if (msgPtr != NULL) {
                NpSigEchoMbxMsg *mbxMsg = (NpSigEchoMbxMsg *)msgPtr;
                if (mbxMsg->pktBuf != NULL) {
                    npMatching2Free(mbxMsg->pktBuf);
                }
                npMatching2Free(mbxMsg);
            }
        }
        sceKernelDeleteMbx(sig->echoMbx);
        sig->echoMbx = 0;
    }
    if (sig->msgPipe > 0) {
        sceKernelDeleteMsgPipe(sig->msgPipe);
        sig->msgPipe = 0;
    }
    if (sig->selectMutex > 0) {
        sceKernelDeleteMutex(sig->selectMutex);
        sig->selectMutex = 0;
    }
    if (sig->mainMutex > 0) {
        sceKernelDeleteMutex(sig->mainMutex);
        sig->mainMutex = 0;
    }
    sceNetFreeThreadinfo(sceKernelGetThreadId());
    return 0;
}

/* 0x024A34 */
static s32 NP_MATCHING2_NOINLINE npSigCancelPeerNetInfo(SceNpMatching2ContextId ctxId, u32 reqId)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigSession *session = NULL;
    NpSigReq *reqNode = NULL;
    NpSigPeerNetInfoNode *resultNode = NULL;
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        if (npSigFindReqById(sig, reqId, &reqNode) >= 0 && reqNode != NULL) {
            npSigReleaseReq(sig, reqNode);
            npSigDeleteReqById(sig, reqId);
            return 0;
        }
        ret = npSigPopPeerNetInfoResult(sig, session->sessionId, reqId, &resultNode);
        if (ret < 0) {
            return (s32)0x80550e07;
        }
        if (resultNode != NULL) {
            npMatching2Free(resultNode);
        }
    }
    return ret;
}

/* 0x024B1C */
static s32 NP_MATCHING2_NOINLINE npSigGetPeerNetInfoResult(SceNpMatching2ContextId ctxId, u32 reqId, void *resultOut)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigSession *session = NULL;
    NpSigPeerNetInfoNode *resultNode;
    NpSigLocalNetInfo *outInfo = (NpSigLocalNetInfo *)resultOut;
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret < 0) {
        return ret;
    }
    ret = npSigPopPeerNetInfoResult(sig, session->sessionId, reqId, &resultNode);
    if (ret < 0) {
        return ret;
    }

    if (outInfo->size == 24) {
        outInfo->localAddr = resultNode->localAddr;
        outInfo->mappedAddr = resultNode->mappedAddr;
        outInfo->natType = resultNode->natType;
        outInfo->upnpStatus = resultNode->upnpStatus;
        outInfo->upnpMapped = resultNode->upnpMapped;
        npMatching2Free(resultNode);
        return ret;
    }
    if (outInfo->size == 28) {
        outInfo->mtuSize = resultNode->mtuSize;
        outInfo->localAddr = resultNode->localAddr;
        outInfo->mappedAddr = resultNode->mappedAddr;
        outInfo->natType = resultNode->natType;
        outInfo->upnpStatus = resultNode->upnpStatus;
        outInfo->upnpMapped = resultNode->upnpMapped;
        npMatching2Free(resultNode);
        return ret;
    }
    return (s32)0x80550e15;
}

/* 0x024C08 */
static s32 NP_MATCHING2_NOINLINE npSigOpenPeerConn(s32 sessionId, const SceNpId *peerNpId, u32 *connIdInOut)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigSession *session = NULL;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindSession(sig, sessionId, &session);
    if (ret >= 0) {
        if (sceNp_9A264EF2(&session->npId, peerNpId) == 0) {
            ret = (s32)0x80550e16;
        } else {
            ret = npSigGetOrCreateConn(sig, &session->npId, peerNpId, &conn);
            if (ret >= 0) {
                ret = npSigAttachSessionToConn(sig, conn, sessionId);
                if (ret >= 0) {
                    u32 prevConnId = *connIdInOut;
                    NpSigPipeMsg pipeMsg;
                    u32 sentBytes = 0;

                    *connIdInOut = conn->connId;
                    pipeMsg.msgKind = 5;
                    pipeMsg.targetId = conn->connId;
                    pipeMsg.payloadWord = 0;
                    pipeMsg.auxWord = (u32)session->sessionId;
                    ret = sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
                    if (ret < 0) {
                        npSigDetachSessionFromConn(sig, conn, sessionId);
                        *connIdInOut = prevConnId;
                    }
                }
            }
        }
    }

    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x024D70 */
static s32 NP_MATCHING2_NOINLINE npSigQueryConnStatus(s32 sessionId, u32 connId, s32 *statusOut, u32 *addrOut, u16 *portOut)
{
    NpSigGlobal *sig = &g_sigGlobal;
    s32 lockRet;
    NpSigConn *conn;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (conn = sig->connList; conn != NULL && conn->connId != connId; conn = conn->next) {
    }

    *statusOut = 0;
    if (conn != NULL) {
        NpSigSessionRef *ref;
        for (ref = conn->sessionRefs; ref != NULL && ref->sessionId != sessionId; ref = ref->next) {
        }
        if (ref != NULL) {
            if (conn->state == 10) {
                *statusOut = 2;
                sceKernelMemcpy(addrOut, &conn->peerSelectedAddr, 4);
                *portOut = conn->peerSelectedPort;
            } else if (conn->state != 0) {
                *statusOut = 1;
            }
        }
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return 0;
}

/* 0x024EB4 */
static s32 NP_MATCHING2_NOINLINE npSigQueryConnInfo(s32 sessionId, u32 connId, s32 code, void *infoOut)
{
    NpSigGlobal *sig = &g_sigGlobal;
    s32 lockRet;
    NpSigConn *conn;
    s32 ret = 0;

    (void)sessionId;
    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (conn = sig->connList; conn != NULL && conn->connId != connId; conn = conn->next) {
    }

    if (conn == NULL) {
        ret = (s32)0x80550e0e;
    } else {
        switch (code) {
            case 1:
                if (conn->state != 10) {
                    ret = (s32)0x80550e14;
                } else {
                    u32 sum = 0;
                    u32 count = 0;
                    s32 idx;
                    for (idx = 0; idx < 6; idx++) {
                        if (conn->rttSamples[idx] != -1 && conn->rttSamples[idx] != 0) {
                            sum += (u32)conn->rttSamples[idx];
                            count++;
                        }
                    }
                    *(u32 *)infoOut = (count != 0) ? (sum / count) : 0;
                }
                break;

            case 2:
                if (conn->state != 10) {
                    ret = (s32)0x80550e14;
                } else {
                    *(u32 *)infoOut = conn->peerBandwidth;
                }
                break;

            case 3:
                sceKernelMemcpy(infoOut, &conn->peerNpId, sizeof(SceNpId));
                break;

            case 4:
                if (conn->state != 10) {
                    ret = (s32)0x80550e14;
                } else {
                    NpSigConnInfoAddrPort *ap = (NpSigConnInfoAddrPort *)infoOut;
                    ap->addr = conn->peerSelectedAddr;
                    ap->port = conn->peerSelectedPort;
                }
                break;

            case 5:
                if (conn->state != 10) {
                    ret = (s32)0x80550e14;
                } else {
                    NpSigConnInfoAddrPort *ap = (NpSigConnInfoAddrPort *)infoOut;
                    ap->addr = conn->localMappedAddr;
                    ap->port = conn->localMappedPort;
                }
                break;

            case 6:
                if (conn->state != 10) {
                    ret = (s32)0x80550e14;
                } else {
                    s32 total = 0;
                    s32 lost = 0;
                    s32 idx;
                    for (idx = 0; idx < 6; idx++) {
                        if (conn->rttSamples[idx] != -1) {
                            if (conn->rttSamples[idx] == 0) {
                                lost++;
                            }
                            total++;
                        }
                    }
                    *(s32 *)infoOut = (100 * lost) / total;
                }
                break;

            default:
                ret = (s32)0x80550e15;
                break;
        }
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0250E8 */
static s32 NP_MATCHING2_NOINLINE npSigDetachAndCloseConn(s32 sessionId, u32 connId)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigSession *session = NULL;
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindSession(sig, sessionId, &session);
    if (ret >= 0) {
        ret = npSigFindConnById(sig, connId, &conn);
        if (ret >= 0) {
            NpSigPipeMsg pipeMsg;
            u32 sentBytes = 0;
            npSigDetachSessionFromConn(sig, conn, sessionId);
            pipeMsg.msgKind = 22;
            pipeMsg.targetId = conn->connId;
            pipeMsg.payloadWord = 0;
            pipeMsg.auxWord = 0;
            ret = sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
        }
    }
    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x0251F0 */
static s32 NP_MATCHING2_NOINLINE npSigStopMainThread(NpSigGlobal *sig)
{
    s32 ret = 0;

    if (sig->msgPipe > 0) {
        NpSigPipeMsg stopMsg;
        u32 sentBytes = 0;
        stopMsg.msgKind = 1;
        stopMsg.targetId = 0;
        stopMsg.payloadWord = 0;
        stopMsg.auxWord = 0;
        ret = sceKernelSendMsgPipe(sig->msgPipe, &stopMsg, sizeof(stopMsg), 0, &sentBytes, NULL);
    }
    if (sig->mainThid > 0) {
        sceKernelWaitThreadEnd(sig->mainThid, NULL);
        sceKernelDeleteThread(sig->mainThid);
        sceNetFreeThreadinfo(sig->mainThid);
        sig->mainThid = 0;
    }
    return ret;
}

/* 0x02527C */
static s32 NP_MATCHING2_NOINLINE npSigStopEchoThread(NpSigGlobal *sig)
{
    s32 ret = 0;

    sceNet_lib_A93A93E9(&sig->pollTimer);
    if (sig->echoThid > 0) {
        void *msgPtr = NULL;
        sig->stopEchoFlag = 1;
        ret = sceNetThreadAbort(sig->echoThid);
        sceKernelWaitThreadEnd(sig->echoThid, NULL);
        sceKernelDeleteThread(sig->echoThid);
        sceNetFreeThreadinfo(sig->echoThid);
        while (sceKernelPollMbx(sig->echoMbx, &msgPtr) == 0) {
            if (msgPtr != NULL) {
                NpSigEchoMbxMsg *mbxMsg = (NpSigEchoMbxMsg *)msgPtr;
                if (mbxMsg->pktBuf != NULL) {
                    npMatching2Free(mbxMsg->pktBuf);
                }
                npMatching2Free(mbxMsg);
            }
        }
    }
    sig->echoThid = 0;
    return ret;
}

/* 0x025340 */
s32 NP_MATCHING2_NOINLINE npSigNotifyConnEvent(NpSigConn *conn, u16 eventId, s32 errorCode)
{
    NpSigGlobal *sig = &g_sigGlobal;
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    if (eventId == 0x5104) {
        NpSigSession *session;
        for (session = sig->sessionList; session != NULL; session = session->next) {
            if (session->cbFunc != NULL &&
                sceNp_9A264EF2(&session->npId, &conn->selfNpId) == 0 &&
                npSigFindSessionRef(conn, session->sessionId) == NULL) {
                ret = session->cbFunc(session->sessionId, conn->connId, 0x5104, 0, session->cbArg);
            }
        }
    } else {
        NpSigSessionRef *ref;
        for (ref = conn->sessionRefs; ref != NULL; ref = ref->next) {
            NpSigSession *session = NULL;
            ret = npSigFindSession(sig, ref->sessionId, &session);
            if (ret >= 0) {
                if (session->cbFunc != NULL) {
                    session->cbFunc(session->sessionId, conn->connId, eventId, errorCode, session->cbArg);
                }
                npSigReleaseSession(sig, session);
            }
        }
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0254BC */
s32 NP_MATCHING2_NOINLINE npSigStunCb(s32 stunHandle, u32 userTag, s32 status, const void *addrInfo)
{
    NpSigGlobal *sig = &g_sigGlobal;

    if (stunHandle == sig->stunHandle) {
        NpSigPipeMsg pipeMsg;
        u32 sentBytes = 0;

        if (status == 2) {
            const NpSigStunAddrInfo *info = (const NpSigStunAddrInfo *)addrInfo;
            if (userTag == 0) {
                pipeMsg.msgKind = 7;
                pipeMsg.payloadWord = info->mappedAddr;
                pipeMsg.auxWord = (u32)npSwap16(info->mappedPort);
                return sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
            }
            pipeMsg.msgKind = 30;
            pipeMsg.payloadWord = info->mappedAddr;
            pipeMsg.auxWord = (u32)npSwap16(info->mappedPort);
        } else {
            if (userTag == 0) {
                pipeMsg.msgKind = 8;
                pipeMsg.payloadWord = (u32)status;
                return sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
            }
            pipeMsg.msgKind = 31;
            pipeMsg.payloadWord = (u32)status;
        }
        pipeMsg.targetId = userTag;
        return sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
    }
    return sig->stunHandle;
}

/* 0x025588 */
s32 NP_MATCHING2_NOINLINE npSigInitContextManager(void)
{
    NpSigGlobal *sig = &g_sigGlobal;
    NpSigIoctlPeerParam keyParam;
    u32 threadOpt[2];
    s32 sockFd;
    s32 idx;
    s32 ret;

    for (idx = 0; idx < 4; idx++) {
        sig->localKey[idx] = sceNet_lib_1858883D();
    }

    sceKernelMemset(&keyParam, 0, sizeof(keyParam));
    sceKernelMemcpy(keyParam.peerKey, sig->localKey, 16);
    sockFd = sceNetInetSocket(2, 6, 0);
    if (sockFd < 0) {
        return sceNetInetGetPspError();
    }
    sceNetInet_lib_AEE60F84(sockFd, (s32)0x802450C9, &keyParam);
    sceNetInetClose(sockFd);

    sig->sharedSigSock = -1;
    sig->sharedBindPort = npSwap16(3658);
    sig->sharedUdpSock = -1;
    sig->stopEchoFlag = 0;
    sceKernelMemset(&sig->selfNpId, 0, sizeof(SceNpId));

    ret = sceNpGetNpId(&sig->selfNpId);
    if (ret >= 0) {
        sceKernelMemset(threadOpt, 0, sizeof(threadOpt));
        threadOpt[0] = sizeof(threadOpt);
        ret = sceKernelCreateThread("SceNpSignalingEcho", npSigEchoThread, sig->priority, 0x4000, 0, threadOpt);
        if (ret >= 0) {
            sig->echoThid = ret;
            ret = sceKernelStartThread(sig->echoThid, sizeof(sig), &sig);
            if (ret < 0) {
                sceKernelTerminateThread(sig->echoThid);
                sceKernelDeleteThread(sig->echoThid);
                sceNetFreeThreadinfo(sig->echoThid);
                sig->echoThid = 0;
            } else {
                sceNet_lib_A8B6205A(&sig->pollTimer, 200000, npSigTimerPollCb, 0);
            }
        }
    }
    return ret;
}

/* 0x025754 */
NpSigGlobal *NP_MATCHING2_NOINLINE npSigGetGlobal(void)
{
    return &g_sigGlobal;
}

/* 0x025760 */
s32 NP_MATCHING2_NOINLINE npSigCreateSession(const SceNpId *npId, s32 (*cbFunc)(s32, u32, u16, s32, void *),
                                             void *cbArg, s32 *sessionIdOut, s32 poolKind)
{
    NpSigSession *session;
    s32 ret;

    session = (NpSigSession *)npMatching2Alloc(sizeof(NpSigSession));
    if (session == NULL) {
        return (s32)0x80550e03;
    }
    sceKernelMemset(session, 0, sizeof(NpSigSession));
    sceKernelMemcpy(&session->npId, npId, sizeof(SceNpId));
    session->cbFunc = cbFunc;
    session->cbArg = cbArg;

    ret = npSigRegisterSession(&g_sigGlobal, session, sessionIdOut, poolKind);
    if (ret < 0) {
        npMatching2Free(session);
    }
    return ret;
}

/* 0x02583C */
s32 NP_MATCHING2_NOINLINE npSigDeleteSession(s32 sessionId)
{
    npSigClearPeerNetInfoResults(&g_sigGlobal, sessionId);
    npSigUnregisterSession(&g_sigGlobal, sessionId);
    return 0;
}

/* 0x025888 */
s32 NP_MATCHING2_NOINLINE npSigGetLocalNetInfo(NpSigLocalNetInfo *infoOut)
{
    return npSigReadLocalNetInfo(infoOut);
}

/* 0x0258A4 */
s32 NP_MATCHING2_NOINLINE npSigTimerTimeoutCb(u32 connId)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    pipeMsg.msgKind = 14;
    pipeMsg.targetId = connId;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
}

/* 0x0258E8 */
s32 NP_MATCHING2_NOINLINE npSigTimerRetryCb(u32 connId)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    pipeMsg.msgKind = 25;
    pipeMsg.targetId = connId;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
}

/* 0x02592C */
s32 NP_MATCHING2_NOINLINE npSigTimerPollCb(void)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    pipeMsg.msgKind = 26;
    pipeMsg.targetId = 0;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
}

/* 0x025968 */
s32 NP_MATCHING2_NOINLINE npSigTimerProbeCb(u32 connId)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    pipeMsg.msgKind = 15;
    pipeMsg.targetId = connId;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
}

/* 0x0259AC */
s32 NP_MATCHING2_NOINLINE npSigTimerBwCb(u32 connId)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    pipeMsg.msgKind = 24;
    pipeMsg.targetId = connId;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
}

/* 0x0259F0 */
s32 NP_MATCHING2_NOINLINE npSigNotifyCb(NpSigSession *session, u32 connId, u16 eventId, s32 errorCode)
{
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(g_sigGlobal.mainMutex, 1, NULL);
    if (session->cbFunc != NULL) {
        ret = session->cbFunc(session->sessionId, connId, eventId, errorCode, session->cbArg);
    }
    if (lockRet == 0) {
        return sceKernelUnlockMutex(g_sigGlobal.mainMutex, 1);
    }
    return ret;
}

/* 0x025AA4 */
s32 NP_MATCHING2_NOINLINE npSigSendPipeMsg(const NpSigPipeMsg *msg)
{
    NpSigPipeMsg localMsg = *msg;
    u32 sentBytes = 0;
    return sceKernelSendMsgPipe(g_sigGlobal.msgPipe, &localMsg, sizeof(localMsg), 0, &sentBytes, NULL);
}

/* 0x025AFC */
s32 NP_MATCHING2_NOINLINE npSigTermContextManager(void)
{
    s32 sockFd;
    s32 ret;

    sockFd = sceNetInetSocket(2, 6, 0);
    if (sockFd < 0) {
        sceNetInetGetPspError();
    } else {
        sceNetInet_lib_AEE60F84(sockFd, 0x200050C8, NULL);
        sceNetInetClose(sockFd);
    }
    ret = npSigStopEchoThread(&g_sigGlobal);
    g_sigGlobal.stunState = 0;
    return ret;
}

/* 0x025B7C */
static s32 NP_MATCHING2_NOINLINE npSigMainThread(u32 args, void *argp)
{
    NpSigGlobal *sig = *(NpSigGlobal **)argp;
    NpSigPipeMsg pipeMsg;
    u32 recvBytes = 0;

    (void)args;
    while (sceKernelReceiveMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &recvBytes, NULL) >= 0 &&
           pipeMsg.msgKind != 1) {
        npSigDispatchAdminMsg(sig, &pipeMsg);
        npSigDispatchPipeMsg(sig, &pipeMsg);
        npSigDispatchEchoEvent(sig, &pipeMsg);
    }
    sceNetFreeThreadinfo(sceKernelGetThreadId());
    return 0;
}

/* 0x025C18 */
static s32 NP_MATCHING2_NOINLINE npSigGetConnStatus(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                    SceNpMatching2RoomMemberId memberId, s32 *statusOut, u32 *addrOut, u16 *portOut)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    u32 roomIdLo = (u32)(roomId & 0xFFFFFFFFu);
    u32 roomIdHi = (u32)(roomId >> 32);
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
        NpSigRoom *room;
        s32 found = 0;

        for (room = session->roomList; room != NULL; room = room->next) {
            if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                s32 idx;
                for (idx = 0; idx < room->maxPeers; idx++) {
                    if (room->peers[idx].memberId == memberId) {
                        ret = npSigQueryConnStatus(session->sessionId, room->peers[idx].connId, statusOut, addrOut, portOut);
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
        if (!found) {
            ret = (s32)0x80550e19;
        }
        if (lockRet == 0) {
            sceKernelUnlockMutex(sig->mainMutex, 1);
        }
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x025D84 */
static s32 NP_MATCHING2_NOINLINE npSigGetConnInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                  SceNpMatching2RoomMemberId memberId, s32 code, void *infoOut)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    u32 roomIdLo = (u32)(roomId & 0xFFFFFFFFu);
    u32 roomIdHi = (u32)(roomId >> 32);
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
        NpSigRoom *room;
        s32 found = 0;

        for (room = session->roomList; room != NULL; room = room->next) {
            if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                s32 idx;
                for (idx = 0; idx < room->maxPeers; idx++) {
                    if (room->peers[idx].memberId == memberId) {
                        ret = npSigQueryConnInfo(session->sessionId, room->peers[idx].connId, code, infoOut);
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
        if (!found) {
            ret = (s32)0x80550e19;
        }
        if (lockRet == 0) {
            sceKernelUnlockMutex(sig->mainMutex, 1);
        }
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x025EE0 */
s32 NP_MATCHING2_NOINLINE npSigFindSessionByCtxId(NpSigGlobal *sig, SceNpMatching2ContextId ctxId, NpSigSession **outSession)
{
    s32 lockRet;
    NpSigSession *cur;
    s32 ret = (s32)0x80550e05;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (cur = sig->sessionList; cur != NULL; cur = cur->next) {
        if (cur->ctxId == ctxId) {
            ret = npSigFindSessionUnlocked(sig, cur->sessionId, outSession);
            break;
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x025F98 */
s32 NP_MATCHING2_NOINLINE npSigFindPeerRoomMember(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId,
                                                  SceNpMatching2ContextId *ctxIdOut, SceNpMatching2RoomId *roomIdOut,
                                                  SceNpMatching2RoomMemberId *memberIdOut, u32 filterConnId)
{
    s32 lockRet;
    NpSigSession *session;
    s32 ret = (s32)0x80550e19;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (session = sig->sessionList; session != NULL; session = session->next) {
        if (session->ctxId != 0 && sceNp_9A264EF2(&session->npId, selfNpId) == 0) {
            NpSigRoom *room;
            for (room = session->roomList; room != NULL; room = room->next) {
                if (room->sigOptParam.type != 0) {
                    s32 idx;
                    for (idx = 0; idx < room->maxPeers; idx++) {
                        NpSigRoomMember *peer = &room->peers[idx];
                        if (peer->memberId != 0 &&
                            sceNp_9A264EF2(&peer->npId, peerNpId) == 0 &&
                            (filterConnId == 0 || filterConnId == peer->connId)) {
                            if (ctxIdOut != NULL) {
                                *ctxIdOut = session->ctxId;
                            }
                            if (roomIdOut != NULL) {
                                *roomIdOut = ((u64)room->roomIdHi << 32) | (u64)room->roomIdLo;
                            }
                            if (memberIdOut != NULL) {
                                *memberIdOut = peer->memberId;
                            }
                            ret = 0;
                            break;
                        }
                    }
                    if (ret == 0) {
                        break;
                    }
                }
            }
            if (ret == 0) {
                break;
            }
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x026168 */
static void NP_MATCHING2_NOINLINE npSigSyncRoomPeers(NpSigGlobal *sig, NpSigSession *session, NpSigRoom *room)
{
    SceNpMatching2RoomId roomId = ((u64)room->roomIdHi << 32) | (u64)room->roomIdLo;
    s32 idx;

    for (idx = 0; idx < room->maxPeers; idx++) {
        NpSigRoomMember *peer = &room->peers[idx];
        s32 wantConn = 0;

        if (peer->memberId != 0) {
            if (room->sigOptParam.type == 1) {
                wantConn = 1;
            } else if (room->sigOptParam.type == 2) {
                SceNpMatching2RoomMemberId hubId = room->sigOptParam.hubMemberId;
                if (hubId == 0) {
                    hubId = room->ownerMemberId;
                }
                if (peer->memberId == hubId || room->meMemberId == hubId) {
                    wantConn = 1;
                }
            }
        }

        if (wantConn) {
            if (peer->connId == 0 && peer->connState == 0 && peer->lastError == 0) {
                s32 openRet = npSigOpenPeerConn(session->sessionId, &peer->npId, &peer->connId);
                if (openRet < 0) {
                    peer->lastError = openRet;
                    npCoreDispatchSignalingCb(session->ctxId, roomId, peer->memberId, 0x5101, openRet);
                } else {
                    peer->connState = 1;
                }
            }
        } else if (peer->connId != 0 && peer->connState > 0) {
            u32 oldConnId = peer->connId;
            peer->connId = 0;
            peer->connState = 0;
            if (npSigFindPeerRoomMember(sig, &session->npId, &peer->npId, NULL, NULL, NULL, oldConnId) == (s32)0x80550e19) {
                npSigDetachAndCloseConn(session->sessionId, oldConnId);
            }
            if (peer->memberId != 0) {
                if (sceKernelGetCompiledSdkVersion() > 0x0500000Fu) {
                    npCoreDispatchSignalingCb(session->ctxId, roomId, peer->memberId, 0x5101, (s32)0x80550e18);
                }
            } else if (peer->leftMemberId != 0) {
                if (sceKernelGetCompiledSdkVersion() > 0x0500000Fu) {
                    npCoreDispatchSignalingCb(session->ctxId, roomId, peer->leftMemberId, 0x5101, (s32)0x80550e18);
                }
                peer->leftMemberId = 0;
            }
        }
    }
}

/* 0x0263B4 */
static s32 NP_MATCHING2_NOINLINE npSigReqListener(SceNpMatching2ContextId ctxId, SceNpMatching2RequestId reqId,
                                                  SceNpMatching2Event eventId, s32 errorCode, const void *eventData)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 ret;

    (void)reqId;
    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0 && errorCode >= 0) {
        if (eventId == 0x0101 || eventId == 0x0102) {
            const NpMatching2Room *const *roomPtr = (const NpMatching2Room *const *)eventData;
            npSigOnRoomJoined(sig, session, *roomPtr);
        } else if (eventId == 0x0103) {
            const u32 *roomIdWords = (const u32 *)eventData;
            ret = npSigOnRoomLeft(sig, session, roomIdWords[0], roomIdWords[1]);
        }
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x02649C */
static void NP_MATCHING2_NOINLINE npSigRoomEvListener(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                      SceNpMatching2Event eventId, const void *eventData)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    u32 roomIdLo = (u32)(roomId & 0xFFFFFFFFu);
    u32 roomIdHi = (u32)(roomId >> 32);

    if (npSigFindSessionByCtxId(sig, ctxId, &session) >= 0) {
        switch (eventId) {
            case 0x1101: {
                const NpSigRoomEvJoinData *ev = (const NpSigRoomEvJoinData *)eventData;
                s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
                NpSigRoom *room;

                for (room = session->roomList; room != NULL; room = room->next) {
                    if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                        s32 idx;
                        for (idx = 0; idx < room->maxPeers; idx++) {
                            if (room->peers[idx].memberId == 0) {
                                sceKernelMemset(&room->peers[idx], 0, sizeof(NpSigRoomMember));
                                room->peers[idx].memberId = ev->member->memberId;
                                sceKernelMemcpy(&room->peers[idx].npId, &ev->member->npId, sizeof(SceNpId));
                                break;
                            }
                        }
                        npSigSyncRoomPeers(sig, session, room);
                        break;
                    }
                }
                if (lockRet == 0) {
                    sceKernelUnlockMutex(sig->mainMutex, 1);
                }
                break;
            }

            case 0x1102: {
                const NpSigRoomEvJoinData *ev = (const NpSigRoomEvJoinData *)eventData;
                s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
                NpSigRoom *room;

                for (room = session->roomList; room != NULL; room = room->next) {
                    if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                        s32 idx;
                        for (idx = 0; idx < room->maxPeers; idx++) {
                            if (room->peers[idx].memberId == ev->member->memberId) {
                                room->peers[idx].leftMemberId = room->peers[idx].memberId;
                                room->peers[idx].memberId = 0;
                                break;
                            }
                        }
                        npSigSyncRoomPeers(sig, session, room);
                        break;
                    }
                }
                if (lockRet == 0) {
                    sceKernelUnlockMutex(sig->mainMutex, 1);
                }
                break;
            }

            case 0x1103:
            case 0x1104:
                npSigOnRoomLeft(sig, session, roomIdLo, roomIdHi);
                break;

            case 0x1105: {
                const NpSigRoomEvOwnerData *ev = (const NpSigRoomEvOwnerData *)eventData;
                s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
                NpSigRoom *room;

                for (room = session->roomList; room != NULL; room = room->next) {
                    if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                        room->ownerMemberId = ev->newOwner;
                        npSigSyncRoomPeers(sig, session, room);
                        break;
                    }
                }
                if (lockRet == 0) {
                    sceKernelUnlockMutex(sig->mainMutex, 1);
                }
                break;
            }

            case 0x1108: {
                s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
                NpSigRoom *room;

                for (room = session->roomList; room != NULL; room = room->next) {
                    if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
                        sceKernelMemcpy(&room->sigOptParam, eventData, sizeof(SceNpMatching2SignalingOptParam));
                        npSigSyncRoomPeers(sig, session, room);
                        break;
                    }
                }
                if (lockRet == 0) {
                    sceKernelUnlockMutex(sig->mainMutex, 1);
                }
                break;
            }

            default:
                break;
        }
    }

    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
}

/* 0x026808 */
static s32 NP_MATCHING2_NOINLINE npSigSessionCb(s32 sessionId, u32 connId, u16 eventId, s32 errorCode, void *cbArg)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 ret;

    (void)cbArg;
    ret = npSigFindSession(sig, sessionId, &session);
    if (ret >= 0) {
        s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
        NpSigRoom *room;

        for (room = session->roomList; room != NULL; room = room->next) {
            SceNpMatching2RoomId roomId = ((u64)room->roomIdHi << 32) | (u64)room->roomIdLo;
            s32 idx;
            for (idx = 0; idx < room->maxPeers; idx++) {
                NpSigRoomMember *peer = &room->peers[idx];
                if (peer->memberId != 0 && peer->connId == connId) {
                    if (eventId == 0x5101) {
                        peer->lastError = errorCode;
                        peer->connId = 0;
                        peer->connState = 0;
                        ret = npCoreDispatchSignalingCb(session->ctxId, roomId, peer->memberId, 0x5101, errorCode);
                    } else if (eventId == 0x5102) {
                        peer->connState = 2;
                        ret = npCoreDispatchSignalingCb(session->ctxId, roomId, peer->memberId, 0x5102, 0);
                    }
                } else if (peer->memberId != 0 && eventId == 0x5103) {
                    ret = npCoreDispatchSignalingCb(session->ctxId, roomId, peer->memberId, 0x5103, errorCode);
                }
            }
        }
        if (lockRet == 0) {
            sceKernelUnlockMutex(sig->mainMutex, 1);
        }
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x0269E4 */
s32 NP_MATCHING2_NOINLINE npSigAddContext(const SceNpId *npId, SceNpMatching2ContextId ctxId)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 sessionId = 0;
    s32 ret;

    ret = npSigCreateSession(npId, npSigSessionCb, NULL, &sessionId, 2);
    if (ret >= 0) {
        ret = npSigFindSession(sig, sessionId, &session);
        if (ret >= 0) {
            session->ctxId = ctxId;
        }
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    if (sessionId != 0 && ret < 0) {
        npSigDeleteSession(sessionId);
    }
    return ret;
}

/* 0x026ABC */
s32 NP_MATCHING2_NOINLINE npSigRemoveContext(SceNpMatching2ContextId ctxId)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        npSigDeleteSession(session->sessionId);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x026B3C */
s32 NP_MATCHING2_NOINLINE npSigStartContext(SceNpMatching2ContextId ctxId)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    void *callbacks[10];
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        sceKernelMemset(callbacks, 0, sizeof(callbacks));
        callbacks[0] = (void *)npSigReqListener;
        callbacks[2] = (void *)npSigRoomEvListener;
        callbacks[4] = (void *)npSigRoomMsgListener;
        ret = npMatching2RegisterInternalListener(2, ctxId, callbacks);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x026C00 */
s32 NP_MATCHING2_NOINLINE npSigStopContext(SceNpMatching2ContextId ctxId)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret >= 0) {
        ret = npMatching2UnregisterInternalListener(2, ctxId);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
    return ret;
}

/* 0x026C8C */
static void NP_MATCHING2_NOINLINE npSigOnRoomJoined(NpSigGlobal *sig, NpSigSession *session, const NpMatching2Room *room)
{
    s32 lockRet;
    NpSigRoom *sigRoom;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    sigRoom = (NpSigRoom *)npMatching2Alloc(sizeof(NpSigRoom));
    if (sigRoom != NULL) {
        sceKernelMemset(sigRoom, 0, sizeof(NpSigRoom));
        sigRoom->meMemberId = room->meMember->memberId;
        sigRoom->ownerMemberId = room->ownerMember->memberId;
        sigRoom->maxPeers = (s32)(room->maxSlots - 1);
        sigRoom->roomIdLo = (u32)(room->roomId & 0xFFFFFFFFu);
        sigRoom->roomIdHi = (u32)(room->roomId >> 32);

        sceNpMatching2GetSignalingOptParamLocal(session->ctxId, room->roomId, &sigRoom->sigOptParam);
        if (sigRoom->maxPeers == 0) {
            npSigSyncRoomPeers(sig, session, sigRoom);
            sigRoom->next = session->roomList;
            session->roomList = sigRoom;
        } else {
            sigRoom->peers = (NpSigRoomMember *)npMatching2Alloc(sizeof(NpSigRoomMember) * (u32)sigRoom->maxPeers);
            if (sigRoom->peers != NULL) {
                NpMatching2RoomMember *member;
                sceKernelMemset(sigRoom->peers, 0, sizeof(NpSigRoomMember) * (u32)sigRoom->maxPeers);
                for (member = room->memberList; member != NULL; member = member->next) {
                    if (sceNp_9A264EF2(&member->npId, &session->npId) != 0) {
                        s32 idx;
                        for (idx = 0; idx < sigRoom->maxPeers; idx++) {
                            if (sigRoom->peers[idx].memberId == 0) {
                                sceKernelMemset(&sigRoom->peers[idx], 0, sizeof(NpSigRoomMember));
                                sigRoom->peers[idx].memberId = member->memberId;
                                sceKernelMemcpy(&sigRoom->peers[idx].npId, &member->npId, sizeof(SceNpId));
                                break;
                            }
                        }
                    }
                }
                npSigSyncRoomPeers(sig, session, sigRoom);
                sigRoom->next = session->roomList;
                session->roomList = sigRoom;
            } else {
                npMatching2Free(sigRoom);
            }
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
}

/* 0x026E94 */
static s32 NP_MATCHING2_NOINLINE npSigOnRoomLeft(NpSigGlobal *sig, NpSigSession *session, u32 roomIdLo, u32 roomIdHi)
{
    s32 lockRet;
    NpSigRoom *cur;
    NpSigRoom *prev = NULL;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (cur = session->roomList; cur != NULL; cur = cur->next) {
        if (cur->roomIdLo == roomIdLo && cur->roomIdHi == roomIdHi) {
            break;
        }
        prev = cur;
    }

    if (cur != NULL) {
        s32 idx;
        if (prev != NULL) {
            prev->next = cur->next;
        } else {
            session->roomList = cur->next;
        }
        cur->next = NULL;

        for (idx = 0; idx < cur->maxPeers; idx++) {
            if (cur->peers[idx].memberId != 0) {
                cur->peers[idx].leftMemberId = cur->peers[idx].memberId;
                cur->peers[idx].memberId = 0;
            }
        }
        npSigSyncRoomPeers(sig, session, cur);
        if (cur->peers != NULL) {
            npMatching2Free(cur->peers);
        }
        ret = npMatching2Free(cur);
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x02700C */
s32 NP_MATCHING2_NOINLINE npSigRegisterSession(NpSigGlobal *sig, NpSigSession *session, s32 *sessionIdOut, s32 poolKind)
{
    s32 *cursorPtr;
    s32 minId;
    s32 maxId;
    s32 startId;
    s32 candId;
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    if (poolKind == 1) {
        cursorPtr = &g_sigSessionCursorPool1;
        minId = 1;
        maxId = 8;
    } else if (poolKind == 2) {
        cursorPtr = &g_sigSessionCursorPool2;
        minId = 17;
        maxId = 24;
    } else {
        cursorPtr = &g_sigSessionCursorPool3;
        minId = 9;
        maxId = 16;
    }

    startId = *cursorPtr;
    candId = (startId == maxId) ? minId : (startId + 1);
    while (candId != startId) {
        NpSigSession *cur;
        for (cur = sig->sessionList; cur != NULL && cur->sessionId != candId; cur = cur->next) {
        }
        if (cur == NULL) {
            break;
        }
        candId = (candId == maxId) ? minId : (candId + 1);
    }

    if (candId == startId) {
        ret = (s32)0x80550e04;
    } else {
        session->sessionId = candId;
        session->next = sig->sessionList;
        sig->sessionList = session;
        *sessionIdOut = candId;
        *cursorPtr = candId;
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027160 */
s32 NP_MATCHING2_NOINLINE npSigUnregisterSession(NpSigGlobal *sig, s32 sessionId)
{
    NpSigPipeMsg pipeMsg;
    u32 sentBytes = 0;
    s32 lockRet;
    NpSigSession *cur;
    NpSigSession *prev = NULL;
    s32 ret;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    pipeMsg.msgKind = 20;
    pipeMsg.targetId = 0;
    pipeMsg.payloadWord = 0;
    pipeMsg.auxWord = (u32)sessionId;
    ret = sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);

    for (cur = sig->sessionList; cur != NULL && cur->sessionId != sessionId; cur = cur->next) {
        prev = cur;
    }
    if (cur != NULL) {
        if (prev != NULL) {
            prev->next = cur->next;
        } else {
            sig->sessionList = cur->next;
        }
        cur->next = NULL;
        cur->cbFunc = NULL;
        cur->cbArg = NULL;
        cur->stateFlags |= 1u;
        if (cur->refCount == 0) {
            ret = npMatching2Free(cur);
        }
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x02725C */
s32 NP_MATCHING2_NOINLINE npSigRegisterReq(NpSigGlobal *sig, NpSigReq *reqNode, u32 reqMask, u32 *reqIdOut)
{
    u32 candId;
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    candId = (g_sigReqIdCursor == 0xFFFFu) ? 1u : (g_sigReqIdCursor + 1u);
    while (candId != g_sigReqIdCursor) {
        NpSigReq *cur;
        for (cur = sig->reqList; cur != NULL && (cur->reqId & 0xFFFFu) != candId; cur = cur->next) {
        }
        if (cur == NULL) {
            break;
        }
        candId = (candId == 0xFFFFu) ? 1u : (candId + 1u);
    }

    if (candId == g_sigReqIdCursor) {
        ret = (s32)0x80550e06;
    } else {
        u32 fullId = reqMask | candId;
        reqNode->reqId = fullId;
        reqNode->next = sig->reqList;
        sig->reqList = reqNode;
        *reqIdOut = fullId;
        g_sigReqIdCursor = candId;
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027378 */
s32 NP_MATCHING2_NOINLINE npSigUnregisterReq(NpSigGlobal *sig, u32 keyVal, s32 keyKind)
{
    s32 lockRet;
    NpSigReq *cur;
    NpSigReq *prev = NULL;
    s32 ret;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    ret = lockRet;
    cur = sig->reqList;
    if (cur == NULL) {
        return ret;
    }

    while (cur != NULL) {
        if (keyKind == 0 || keyKind == 1) {
            u32 cmpVal = (keyKind == 1) ? cur->subKey : cur->reqId;
            if (cmpVal == keyVal) {
                if (prev != NULL) {
                    prev->next = cur->next;
                } else {
                    sig->reqList = cur->next;
                }
                cur->next = NULL;
                cur->stateFlags |= 1u;
                if (cur->refCount == 0) {
                    ret = npMatching2Free(cur);
                }
                break;
            }
            prev = cur;
            cur = cur->next;
        } else if (keyKind == 3) {
            prev = cur;
            if (cur->extraKey == keyVal) {
                break;
            }
            cur = cur->next;
        } else {
            break;
        }
    }

    if (cur == NULL && keyKind != 3) {
        return ret;
    }
    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027494 */
s32 NP_MATCHING2_NOINLINE npSigFindConnByNpId(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId, NpSigConn **outConn)
{
    s32 lockRet;
    NpSigConn *cur;
    s32 ret = (s32)0x80550e0e;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (cur = sig->connList; cur != NULL; cur = cur->next) {
        if ((cur->flags & 0x20u) == 0 &&
            sceNp_9A264EF2(selfNpId, &cur->selfNpId) == 0 &&
            sceNp_9A264EF2(peerNpId, &cur->peerNpId) == 0) {
            if (outConn != NULL) {
                *outConn = cur;
                cur->refCount++;
            }
            ret = 0;
            break;
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x02759C */
s32 NP_MATCHING2_NOINLINE npSigGetOrCreateConn(NpSigGlobal *sig, const SceNpId *selfNpId, const SceNpId *peerNpId, NpSigConn **outConn)
{
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    if (g_sigConnIdCursor == 0) {
        g_sigConnIdCursor = sceNet_lib_1858883D() / 0x10002u;
    }

    if (npSigFindConnByNpId(sig, selfNpId, peerNpId, outConn) < 0) {
        u32 candId = (g_sigConnIdCursor == 0xFFFFu) ? 1u : (g_sigConnIdCursor + 1u);
        while (candId != g_sigConnIdCursor) {
            if (npSigFindConnInternal(sig, candId, NULL, 2, 0) < 0) {
                break;
            }
            candId = (candId == 0xFFFFu) ? 1u : (candId + 1u);
        }

        if (candId == g_sigConnIdCursor) {
            ret = (s32)0x80550e0d;
        } else {
            NpSigConn *conn;
            if (sig->connCount >= 64) {
                ret = (s32)0x80550e17;
            }
            conn = (NpSigConn *)npMatching2Alloc(sizeof(NpSigConn));
            if (conn != NULL) {
                sceKernelMemset(conn, 0, sizeof(NpSigConn));
                conn->state = 1;
                conn->connId = candId;
                sceKernelMemcpy(&conn->selfNpId, selfNpId, sizeof(SceNpId));
                sceKernelMemcpy(&conn->peerNpId, peerNpId, sizeof(SceNpId));
                conn->symUdpSock = -1;
                conn->symSigSock = -1;
                conn->next = sig->connList;
                sig->connList = conn;
                g_sigConnIdCursor = candId;
                conn->refCount++;
                sig->connCount++;
                *outConn = conn;
            } else {
                ret = (s32)0x80550e03;
            }
        }
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0277A0 */
s32 NP_MATCHING2_NOINLINE npSigReleaseConn(NpSigGlobal *sig, NpSigConn *conn)
{
    s32 lockRet;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    if (conn->state == 0) {
        NpSigConn *cur = sig->connList;
        NpSigConn *prev = NULL;
        while (cur != NULL && cur != conn) {
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            if (prev != NULL) {
                prev->next = conn->next;
            } else {
                sig->connList = conn->next;
            }
            cur->next = NULL;
        }
        conn->flags |= 1u;
    }

    conn->refCount--;
    if (conn->refCount == 0 && (conn->flags & 1u) != 0) {
        if (conn->candList != NULL) {
            npSigFreeCandList(conn->candList);
        }
        if (conn->sessionRefs != NULL) {
            npSigFreeRefList(conn->sessionRefs);
        }
        ret = npMatching2Free(conn);
        sig->connCount--;
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0278C8 */
s32 NP_MATCHING2_NOINLINE npSigFreeCandList(NpSigCand *candList)
{
    s32 ret = 0;
    NpSigCand *cur = candList;

    while (cur != NULL) {
        NpSigCand *next = cur->next;
        ret = npMatching2Free(cur);
        cur = next;
    }
    return ret;
}

/* 0x0278F8 */
s32 NP_MATCHING2_NOINLINE npSigCloseConn(NpSigGlobal *sig, NpSigConn *conn, s32 errorCode, s32 closeKind)
{
    s32 ret;

    (void)closeKind;
    if (conn->state >= 2) {
        sceNet_lib_A93A93E9(&conn->connectTimer);
    }
    if (conn->state >= 7) {
        sceNet_lib_A93A93E9(&conn->echoTimer);
    }
    if (conn->state >= 5) {
        NpSigSignalPayload payload;
        sceKernelMemset(&payload, 0, sizeof(payload));
        payload.srcTag = conn->localSessionTag;
        payload.dstTag = conn->peerSessionTag;
        sceKernelMemcpy(payload.sessionKey, conn->localKey, 16);
        npSigSendSignalPacket(conn, 3, &payload);
    }
    if (conn->state >= 10) {
        NpSigIoctlPeerParam peerParam;
        s32 sockFd;
        sceNet_lib_A93A93E9(&conn->bwTimer);
        sceKernelMemset(&peerParam, 0, sizeof(peerParam));
        peerParam.peerPort = conn->peerSelectedPort;
        peerParam.peerAddr = conn->peerSelectedAddr;
        sceKernelMemcpy(peerParam.peerKey, conn->peerKey, 16);
        sockFd = sceNetInetSocket(2, 6, 0);
        if (sockFd < 0) {
            sceNetInetGetPspError();
        } else {
            sceNetInet_lib_AEE60F84(sockFd, (s32)0x802450CC, &peerParam);
            sceNetInetClose(sockFd);
        }
    }

    ret = npSigCloseSymmetricSocks(sig, conn);
    if ((conn->flags & 0x10u) != 0) {
        conn->flags &= ~0x10u;
        sig->symStunBusy = 0;
    }
    if (conn->state != 0) {
        conn->state = 0;
        return npSigNotifyConnEvent(conn, 0x5101, errorCode);
    }
    return ret;
}

/* 0x027AA4 */
s32 NP_MATCHING2_NOINLINE npSigAttachSessionToConn(NpSigGlobal *sig, NpSigConn *conn, s32 sessionId)
{
    s32 lockRet;
    NpSigSessionRef *cur;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (cur = conn->sessionRefs; cur != NULL && cur->sessionId != sessionId; cur = cur->next) {
    }
    if (cur == NULL) {
        NpSigSessionRef *ref = (NpSigSessionRef *)npMatching2Alloc(sizeof(NpSigSessionRef));
        if (ref != NULL) {
            sceKernelMemset(ref, 0, sizeof(NpSigSessionRef));
            ref->sessionId = sessionId;
            ref->next = conn->sessionRefs;
            conn->sessionRefs = ref;
        } else {
            ret = (s32)0x80550e03;
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027B90 */
s32 NP_MATCHING2_NOINLINE npSigDetachSessionFromConn(NpSigGlobal *sig, NpSigConn *conn, s32 sessionId)
{
    s32 lockRet;
    NpSigSessionRef *cur;
    NpSigSessionRef *prev = NULL;
    s32 ret = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (cur = conn->sessionRefs; cur != NULL && cur->sessionId != sessionId; cur = cur->next) {
        prev = cur;
    }
    if (cur != NULL) {
        if (prev != NULL) {
            prev->next = cur->next;
        } else {
            conn->sessionRefs = cur->next;
        }
        cur->next = NULL;
        npMatching2Free(cur);
    } else {
        ret = (s32)0x8002a781;
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}
