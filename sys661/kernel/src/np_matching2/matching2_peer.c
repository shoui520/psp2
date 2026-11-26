#include "np_matching2_internal.h"

/* Forward declarations for static internal helpers */
static s32 NP_MATCHING2_NOINLINE npSigTlvReadU32(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag, u32 *valOut);
static s32 NP_MATCHING2_NOINLINE npSigBuildSignalTlv(u8 *outBuf, u32 outBufSize, u32 msgType,
                                                     const NpSigSignalPayload *payload, u32 *txIdOut, u32 *sizeOut);
static s32 NP_MATCHING2_NOINLINE npSigParsePeerNetInfoTlv(const u8 *buf, u32 bufLen,
                                                          NpSigLocalNetInfo *infoOut, u32 *reqIdOut);
static s32 NP_MATCHING2_NOINLINE npSigHandleIncomingSignalMsg(NpSigGlobal *sig, NpSigSession *session,
                                                              NpSigRoom *room, NpSigRoomMember *member,
                                                              const u8 *msgBuf, u32 msgLen);
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerNetInfoReqMsg(NpSigGlobal *sig, NpSigSession *session,
                                                              NpSigRoom *room, NpSigRoomMember *member,
                                                              const u8 *msgBuf, u32 msgLen);
static s32 NP_MATCHING2_NOINLINE npSigTlvReadBytes(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag,
                                                   void *outBuf, u32 expectedLen, const u8 **ptrOut, u32 *lenOut);
static s32 NP_MATCHING2_NOINLINE npSigTlvReadU16(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag, u16 *valOut);
static s32 NP_MATCHING2_NOINLINE npSigBuildPeerNetInfoTlv(const NpSigLocalNetInfo *info, u8 *outBuf,
                                                          u32 outBufSize, u32 *sizeOut, u32 reqId);
static s32 NP_MATCHING2_NOINLINE npSigParseSignalTlv(const u8 *buf, u32 bufLen, NpSigSignalPayload *payloadOut,
                                                     s32 *pipeKindOut, u32 *txIdOut);
static s32 NP_MATCHING2_NOINLINE npSigHandleProbeTimerEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleEchoReqEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleEchoRespEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigHandleBwTimerEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg);
static s32 NP_MATCHING2_NOINLINE npSigQueueEchoSend(NpSigGlobal *sig, s32 sockFd, const void *pktBuf, u32 pktLen,
                                                    s32 flags, const SceNetInetSockaddrIn *dstAddr, u32 addrLen);
static s32 NP_MATCHING2_NOINLINE npSigBuildEchoPacket(u8 *pktBuf, u8 pktType, NpSigConn *conn,
                                                      u32 stampHi, u32 stampLo, u32 peerAddr, u16 peerPort);
static s32 NP_MATCHING2_NOINLINE npSigSelectBestCand(NpSigConn *conn, NpSigCand *bestCand, u32 nowLo, u32 nowHi);
static s32 NP_MATCHING2_NOINLINE npSigPollEchoSockets(NpSigGlobal *sig, s32 *needRebindOut);
static u32 NP_MATCHING2_NOINLINE npSigComputeBandwidth(NpSigConn *conn);
static s32 NP_MATCHING2_NOINLINE npSigRecvEchoPacket(NpSigGlobal *sig, s32 sockFd);
static s32 NP_MATCHING2_NOINLINE npSigParseEchoPacket(void *parsedOut, const u8 *rawBuf);

typedef struct NpSigRoomMsgPayloadView {
    u32 reservedLead[3];
    const u8 *msgBuf;
    u32 msgLen;
} NpSigRoomMsgPayloadView;

typedef struct NpSigStunAuthParam {
    u32 version;
    u32 keyId;
    u8 key[20];
    u8 reserved[12];
} NpSigStunAuthParam;

typedef struct NpSigEchoParsedPacket {
    SceNpId srcNpId;
    SceNpId dstNpId;
    u16 srcTagNet;
    u16 dstTagNet;
    u32 stampHiNet;
    u32 stampLoNet;
    u32 reflectedAddr;
    u16 reflectedPort;
    u8 pktType;
    u8 alignPad;
    u32 bandwidthNet;
} NpSigEchoParsedPacket;

static const u8 g_sigStunAuthKey[20] = {
    0xa0, 0x07, 0x0e, 0x61, 0xad, 0x3f, 0xca, 0x50, 0x6d, 0xb5,
    0xad, 0x3d, 0x57, 0x13, 0x32, 0xce, 0xa7, 0x32, 0x0d, 0x9e
};

static inline __attribute__((always_inline)) s32 npSigTlvWriteU32(u8 *outBuf, u32 outBufSize, u32 *cursorInOut, u16 tag, u32 value)
{
    u32 valNet = value;
    if (outBuf != NULL) {
        u16 hdrNet;
        if (outBufSize < *cursorInOut + 8u) {
            return (s32)0x80550e08;
        }
        hdrNet = npSwap16(tag);
        sceKernelMemcpy(&outBuf[*cursorInOut], &hdrNet, 2);
        hdrNet = npSwap16(4u);
        sceKernelMemcpy(&outBuf[*cursorInOut + 2u], &hdrNet, 2);
        valNet = npSwap32(valNet);
        sceKernelMemcpy(&outBuf[*cursorInOut + 4u], &valNet, 4);
    }
    *cursorInOut += 8u;
    return 0;
}

static inline __attribute__((always_inline)) s32 npSigTlvWriteU16(u8 *outBuf, u32 outBufSize, u32 *cursorInOut, u16 tag, u16 value)
{
    u16 valNet = value;
    if (outBuf != NULL) {
        u16 hdrNet;
        if (outBufSize < *cursorInOut + 6u) {
            return (s32)0x80550e08;
        }
        hdrNet = npSwap16(tag);
        sceKernelMemcpy(&outBuf[*cursorInOut], &hdrNet, 2);
        hdrNet = npSwap16(2u);
        sceKernelMemcpy(&outBuf[*cursorInOut + 2u], &hdrNet, 2);
        valNet = npSwap16(valNet);
        sceKernelMemcpy(&outBuf[*cursorInOut + 4u], &valNet, 2);
    }
    *cursorInOut += 6u;
    return 0;
}

static inline __attribute__((always_inline)) s32 npSigTlvWriteRaw(u8 *outBuf, u32 outBufSize, u32 *cursorInOut, u16 tag, const void *src, u16 len)
{
    if (outBuf != NULL) {
        u16 hdrNet;
        if (outBufSize < *cursorInOut + (u32)len + 4u) {
            return (s32)0x80550e08;
        }
        hdrNet = npSwap16(tag);
        sceKernelMemcpy(&outBuf[*cursorInOut], &hdrNet, 2);
        hdrNet = npSwap16(len);
        sceKernelMemcpy(&outBuf[*cursorInOut + 2u], &hdrNet, 2);
        if (src != NULL) {
            sceKernelMemcpy(&outBuf[*cursorInOut + 4u], src, len);
        }
    }
    *cursorInOut += (u32)len + 4u;
    return 0;
}

static u32 g_sigSignalTxSeq;

/* 0x027C6C */
s32 NP_MATCHING2_NOINLINE npSigFreeRefList(NpSigSessionRef *refList)
{
    s32 ret = 0;
    NpSigSessionRef *cur = refList;

    while (cur != NULL) {
        NpSigSessionRef *next = cur->next;
        ret = npMatching2Free(cur);
        cur = next;
    }
    return ret;
}

/* 0x027C9C */
s32 NP_MATCHING2_NOINLINE npSigPopPeerNetInfoResult(NpSigGlobal *sig, s32 sessionId, u32 reqId,
                                                    NpSigPeerNetInfoNode **outResult)
{
    s32 lockRet;
    s32 ret = (s32)0x80550e13;
    NpSigPeerNetInfoNode *cur;
    NpSigPeerNetInfoNode *prev = NULL;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    cur = sig->peerNetInfoList;
    while (cur != NULL) {
        if (cur->sessionId == sessionId && cur->reqId == reqId) {
            if (prev != NULL) {
                prev->next = cur->next;
            } else {
                sig->peerNetInfoList = cur->next;
            }
            cur->next = NULL;
            *outResult = cur;
            ret = 0;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027D80 */
s32 NP_MATCHING2_NOINLINE npSigClearPeerNetInfoResults(NpSigGlobal *sig, s32 sessionId)
{
    s32 lockRet;
    s32 ret;
    NpSigPeerNetInfoNode *cur;
    NpSigPeerNetInfoNode *prev = NULL;

    ret = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    lockRet = ret;
    cur = sig->peerNetInfoList;
    while (cur != NULL) {
        NpSigPeerNetInfoNode *next = cur->next;
        if (cur->sessionId == sessionId) {
            if (prev != NULL) {
                prev->next = next;
            } else {
                sig->peerNetInfoList = next;
            }
            ret = npMatching2Free(cur);
        } else {
            prev = cur;
        }
        cur = next;
    }

    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x027E30 */
s32 NP_MATCHING2_NOINLINE npSigReadLocalNetInfo(NpSigLocalNetInfo *infoOut)
{
    NpSigUpnpInfo upnpInfo;
    char ipText[128];
    s32 ret;

    ret = sceNetApctlGetInfo(8, ipText);
    if (ret >= 0) {
        sceNetInetInetPton(2, ipText, &infoOut->localAddr);
    }
    if (ret >= 0) {
        sceKernelMemset(&upnpInfo, 0, sizeof(upnpInfo));
        ret = sceNetUpnp_1038E77A(&upnpInfo);
        if (ret >= 0) {
            if (infoOut->size != 24) {
                if (infoOut->size != 28) {
                    return (s32)0x80550e15;
                }
                infoOut->mtuSize = upnpInfo.mtuSize;
            }
            if (upnpInfo.natDetectStatus == 2) {
                infoOut->natType = (u32)upnpInfo.natType;
                infoOut->mappedAddr = upnpInfo.extAddr;
            } else {
                infoOut->mappedAddr = 0xFFFFFFFFu;
                infoOut->natType = 0;
            }
            infoOut->upnpStatus = upnpInfo.upnpStatus;
            infoOut->upnpMapped = (upnpInfo.upnpMapped == 2) ? 1u : 0u;
        }
    }
    return ret;
}

/* 0x027F2C */
s32 NP_MATCHING2_NOINLINE npSigOpenSymmetricSocks(NpSigGlobal *sig, NpSigConn *conn)
{
    SceNetInetSockaddrIn udpAddr;
    SceNetInetSockaddrIn p2pAddr;
    u32 addrLen;
    s32 lockRet;
    s32 sockFd;
    s32 ret;
    s32 failed = 0;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    sockFd = sceNetInetSocket(2, 2, 0);
    if (sockFd < 0) {
        failed = 1;
    } else {
        conn->symUdpSock = sockFd;
        sceKernelMemset(&udpAddr, 0, sizeof(udpAddr));
        udpAddr.sinFamily = 2;
        udpAddr.sinPort = 0;
        udpAddr.sinAddr = 0;
        if (sceNetInetBind(conn->symUdpSock, &udpAddr, sizeof(udpAddr)) < 0) {
            failed = 1;
        } else {
            addrLen = sizeof(udpAddr);
            if (sceNetInetGetsockname(conn->symUdpSock, &udpAddr, &addrLen) < 0) {
                failed = 1;
            } else {
                sockFd = sceNetInetSocket(2, 6, 0);
                if (sockFd < 0) {
                    failed = 1;
                } else {
                    conn->symSigSock = sockFd;
                    sceKernelMemset(&p2pAddr, 0, sizeof(p2pAddr));
                    p2pAddr.sinFamily = 2;
                    p2pAddr.sinPort = udpAddr.sinPort;
                    p2pAddr.sinAddr = 0;
                    p2pAddr.sinVport = npSwap16(0xFFFEu);
                    ret = sceNetInetBind(conn->symSigSock, &p2pAddr, sizeof(p2pAddr));
                    if (ret < 0) {
                        failed = 1;
                    } else {
                        conn->symNext = sig->symConnList;
                        conn->symLocalPort = udpAddr.sinPort;
                        sig->symConnList = conn;
                    }
                }
            }
        }
    }

    if (failed != 0) {
        ret = sceNetInetGetPspError();
        if (conn->symSigSock >= 0) {
            sceNetInetClose(conn->symSigSock);
            conn->symSigSock = -1;
        }
        if (conn->symUdpSock >= 0) {
            sceNetInetClose(conn->symUdpSock);
            conn->symUdpSock = -1;
        }
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0280C0 */
s32 NP_MATCHING2_NOINLINE npSigCloseSymmetricSocks(NpSigGlobal *sig, NpSigConn *conn)
{
    s32 udpSock = -1;
    s32 sigSock = -1;
    s32 lockRet;
    s32 ret;

    ret = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    lockRet = ret;
    if (conn->symSigSock >= 0) {
        NpSigConn *cur = sig->symConnList;
        NpSigConn *prev = NULL;
        while (cur != NULL && cur != conn) {
            prev = cur;
            cur = cur->symNext;
        }
        if (cur != NULL) {
            if (prev != NULL) {
                prev->symNext = conn->symNext;
            } else {
                sig->symConnList = conn->symNext;
            }
            cur->symNext = NULL;
        }
        sigSock = conn->symSigSock;
        conn->symSigSock = -1;
        if (conn->symUdpSock >= 0) {
            udpSock = conn->symUdpSock;
            conn->symUdpSock = -1;
        }
    }
    if (lockRet == 0) {
        ret = sceKernelUnlockMutex(sig->mainMutex, 1);
    }

    if (udpSock >= 0 && sigSock >= 0) {
        s32 selLock = sceKernelLockMutex(sig->selectMutex, 1, NULL);
        ret = selLock;
        if (sigSock >= 0) {
            ret = sceNetInetClose(sigSock);
        }
        if (udpSock >= 0) {
            ret = sceNetInetClose(udpSock);
        }
        if (selLock == 0) {
            return sceKernelUnlockMutex(sig->selectMutex, 1);
        }
    }
    return ret;
}

/* 0x0281EC */
s32 NP_MATCHING2_NOINLINE npSigFindSessionUnlocked(NpSigGlobal *sig, s32 sessionId, NpSigSession **outSession)
{
    NpSigSession *cur = sig->sessionList;

    while (cur != NULL) {
        if (cur->sessionId == sessionId) {
            if (outSession != NULL) {
                *outSession = cur;
                cur->refCount++;
            }
            return 0;
        }
        cur = cur->next;
    }
    return (s32)0x80550e05;
}

/* 0x028244 */
s32 NP_MATCHING2_NOINLINE npSigFindSession(NpSigGlobal *sig, s32 sessionId, NpSigSession **outSession)
{
    s32 lockRet;
    s32 ret = (s32)0x80550e05;
    NpSigSession *cur;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    cur = sig->sessionList;
    while (cur != NULL) {
        if (cur->sessionId == sessionId) {
            if (outSession != NULL) {
                *outSession = cur;
                cur->refCount++;
            }
            ret = 0;
            break;
        }
        cur = cur->next;
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028304 */
s32 NP_MATCHING2_NOINLINE npSigReleaseSession(NpSigGlobal *sig, NpSigSession *session)
{
    s32 lockRet;
    s32 ret;

    ret = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    lockRet = ret;
    session->refCount--;
    if (session->refCount == 0 && (session->stateFlags & 1u) != 0) {
        ret = npMatching2Free(session);
    }
    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028394 */
s32 NP_MATCHING2_NOINLINE npSigFindReqById(NpSigGlobal *sig, u32 reqId, NpSigReq **outReq)
{
    return npSigFindReqByKey(sig, reqId, outReq, 0);
}

/* 0x0283B0 */
s32 NP_MATCHING2_NOINLINE npSigReleaseReq(NpSigGlobal *sig, NpSigReq *reqNode)
{
    s32 lockRet;
    s32 ret;

    ret = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    lockRet = ret;
    reqNode->refCount--;
    if (reqNode->refCount == 0 && (reqNode->stateFlags & 1u) != 0) {
        ret = npMatching2Free(reqNode);
    }
    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028440 */
s32 NP_MATCHING2_NOINLINE npSigDeleteReqById(NpSigGlobal *sig, u32 reqId)
{
    return npSigUnregisterReq(sig, reqId, 0);
}

/* 0x02845C */
s32 NP_MATCHING2_NOINLINE npSigFindConnById(NpSigGlobal *sig, u32 connId, NpSigConn **outConn)
{
    s32 lockRet;
    s32 ret;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    ret = npSigFindConnInternal(sig, connId, outConn, 2, 0);
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x0284E8 */
s32 NP_MATCHING2_NOINLINE npSigFindConnByIdAll(NpSigGlobal *sig, u32 connId, NpSigConn **outConn)
{
    s32 lockRet;
    s32 ret;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    ret = npSigFindConnInternal(sig, connId, outConn, 2, 1);
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028574 */
s32 NP_MATCHING2_NOINLINE npSigFindConnByTxId(NpSigGlobal *sig, u32 txId, NpSigConn **outConn)
{
    s32 lockRet;
    s32 ret;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    ret = npSigFindConnInternal(sig, txId, outConn, 0, 0);
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028600 */
NpSigCand *NP_MATCHING2_NOINLINE npSigFindCandByAddr(NpSigCand *candList, u32 ipAddr, u16 port)
{
    NpSigCand *cur = candList;

    while (cur != NULL) {
        if (cur->peerAddr == ipAddr && cur->peerPort == port) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* 0x02864C */
NpSigSessionRef *NP_MATCHING2_NOINLINE npSigFindSessionRef(NpSigConn *conn, s32 sessionId)
{
    NpSigSessionRef *cur = conn->sessionRefs;

    while (cur != NULL) {
        if (cur->sessionId == sessionId) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* 0x028678 */
s32 NP_MATCHING2_NOINLINE npSigPushPeerNetInfoResult(NpSigGlobal *sig, NpSigPeerNetInfoNode *resultNode)
{
    s32 lockRet;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    resultNode->next = sig->peerNetInfoList;
    sig->peerNetInfoList = resultNode;
    if (lockRet == 0) {
        return sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return lockRet;
}

/* 0x0286D8 */
s32 NP_MATCHING2_NOINLINE npSigGetLocalIpAddr(u32 *ipOut)
{
    char ipText[128];
    s32 ret;

    ret = sceNetApctlGetInfo(8, ipText);
    if (ret >= 0) {
        sceNetInetInetPton(2, ipText, ipOut);
    }
    return ret;
}

/* 0x02872C */
s32 NP_MATCHING2_NOINLINE npSigSendSimpleSignal(NpSigConn *conn, u32 msgType)
{
    NpSigSignalPayload payload;

    sceKernelMemset(&payload, 0, sizeof(payload));
    payload.srcTag = conn->localSessionTag;
    payload.dstTag = conn->peerSessionTag;
    return npSigSendSignalPacket(conn, msgType, &payload);
}

/* 0x028788 */
s32 NP_MATCHING2_NOINLINE npSigFindReqByKey(NpSigGlobal *sig, u32 keyVal, NpSigReq **outReq, s32 keyKind)
{
    s32 lockRet;
    s32 ret = (s32)0x80550e07;
    NpSigReq *cur;

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    cur = sig->reqList;
    while (cur != NULL) {
        u32 curKey;
        if (keyKind == 0) {
            curKey = cur->reqId;
        } else if (keyKind == 1) {
            curKey = cur->subKey;
        } else if (keyKind == 3) {
            curKey = cur->extraKey;
        } else {
            break;
        }
        if (curKey == keyVal) {
            if (outReq != NULL) {
                *outReq = cur;
                cur->refCount++;
            }
            ret = 0;
            break;
        }
        cur = cur->next;
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    return ret;
}

/* 0x028888 */
s32 NP_MATCHING2_NOINLINE npSigFindConnInternal(NpSigGlobal *sig, u32 keyVal, NpSigConn **outConn,
                                                s32 keyKind, s32 includeClosing)
{
    NpSigConn *cur = sig->connList;

    while (cur != NULL) {
        if (includeClosing == 0 && (cur->flags & 0x20u) != 0) {
            cur = cur->next;
            continue;
        }
        if (keyKind != 0) {
            if (keyKind != 2) {
                return (s32)0x80550e0e;
            }
            if (cur->connId == keyVal) {
                if (outConn != NULL) {
                    *outConn = cur;
                    cur->refCount++;
                }
                return 0;
            }
        } else {
            if (cur->lastTxId == keyVal) {
                if (outConn != NULL) {
                    *outConn = cur;
                    cur->refCount++;
                }
                return 0;
            }
        }
        cur = cur->next;
    }
    return (s32)0x80550e0e;
}

/* 0x02890C */
s32 NP_MATCHING2_NOINLINE npSigSendSignalPacket(NpSigConn *conn, u32 msgType, const NpSigSignalPayload *payload)
{
    NpSigGlobal *sig = npSigGetGlobal();
    SceNpMatching2ContextId ctxId;
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId memberId;
    SceNpMatching2RequestId sentReqId;
    u32 pktLen;
    s32 ret;

    ret = npSigFindPeerRoomMember(sig, &conn->selfNpId, &conn->peerNpId, &ctxId, &roomId, &memberId, 0);
    if (ret >= 0) {
        ret = npSigBuildSignalTlv(NULL, 0, msgType, payload, NULL, &pktLen);
        if (ret >= 0) {
            u8 *pktBuf = (u8 *)npMatching2Alloc(pktLen);
            ret = (s32)0x80550e03;
            if (pktBuf != NULL) {
                ret = npSigBuildSignalTlv(pktBuf, pktLen, msgType, payload, &conn->lastTxId, NULL);
                if (ret >= 0) {
                    SceNpMatching2SendRoomMessageRequest sendReq;
                    sceKernelMemset(&sendReq, 0, sizeof(sendReq));
                    sendReq.roomId = roomId;
                    sendReq.castType = 2;
                    sendReq.target.unicastTarget = memberId;
                    sendReq.msg = pktBuf;
                    sendReq.msgLen = pktLen;
                    ret = npMatching2SendRoomMessageInternal(ctxId, &sendReq, NULL, &sentReqId, 2);
                }
                npMatching2Free(pktBuf);
            }
        }
    }
    return ret;
}

/* 0x028A58 */
s32 NP_MATCHING2_NOINLINE npSigStartPeerNetInfo(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                SceNpMatching2RoomMemberId memberId, u32 *reqIdOut)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session;
    NpSigReq *reqNode;
    u8 *pktBuf;
    s32 ret;

    ret = npSigFindSessionByCtxId(sig, ctxId, &session);
    if (ret < 0) {
        return ret;
    }

    reqNode = (NpSigReq *)npMatching2Alloc(sizeof(NpSigReq));
    if (reqNode == NULL) {
        return ret;
    }
    sceKernelMemset(reqNode, 0, sizeof(NpSigReq));
    reqNode->sessionId = session->sessionId;
    ret = npSigRegisterReq(sig, reqNode, 0x21000000u, reqIdOut);
    if (ret < 0) {
        npMatching2Free(reqNode);
        return ret;
    }

    pktBuf = (u8 *)npMatching2Alloc(16);
    if (pktBuf == NULL) {
        npSigDeleteReqById(sig, *reqIdOut);
        return (s32)0x80550e03;
    }

    {
        u32 bufCap = 16;
        u32 cursor = 0;

        if (npSigTlvWriteU32(pktBuf, bufCap, &cursor, 1u, 5u) >= 0 &&
            npSigTlvWriteU32(pktBuf, bufCap, &cursor, 0x13u, *reqIdOut) >= 0) {
            SceNpMatching2SendRoomMessageRequest sendReq;
            SceNpMatching2RequestId sentReqId;

            sceKernelMemset(&sendReq, 0, sizeof(sendReq));
            sendReq.roomId = roomId;
            sendReq.castType = 2;
            sendReq.target.unicastTarget = memberId;
            sendReq.msg = pktBuf;
            sendReq.msgLen = bufCap;
            ret = npMatching2SendRoomMessageInternal(ctxId, &sendReq, NULL, &sentReqId, 2);
            if (ret >= 0) {
                void *checkAlloc = npMatching2Alloc(sizeof(NpSigReq));
                if (checkAlloc != NULL) {
                    npMatching2Free(pktBuf);
                    return ret;
                }
                ret = (s32)0x80550e03;
            }
            npSigDeleteReqById(sig, *reqIdOut);
            npMatching2Free(pktBuf);
            return ret;
        }
        ret = (s32)0x80550e08;
        npSigDeleteReqById(sig, *reqIdOut);
        npMatching2Free(pktBuf);
        return ret;
    }
}

/* 0x028D64 */
void NP_MATCHING2_NOINLINE npSigRoomMsgListener(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId,
                                                const void *msgInfo)
{
    NpSigGlobal *sig = npSigGetGlobal();
    NpSigSession *session = NULL;
    s32 lockRet;
    NpSigRoom *room;
    u32 roomIdLo = (u32)(roomId & 0xFFFFFFFFu);
    u32 roomIdHi = (u32)(roomId >> 32);

    if (npSigFindSessionByCtxId(sig, ctxId, &session) < 0) {
        return;
    }

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    room = session->roomList;
    while (room != NULL) {
        if (room->roomIdLo == roomIdLo && room->roomIdHi == roomIdHi) {
            break;
        }
        room = room->next;
    }

    if (room != NULL) {
        s32 peerIdx;
        for (peerIdx = 0; peerIdx < room->maxPeers; peerIdx++) {
            if (room->peers[peerIdx].memberId == memberId) {
                break;
            }
        }
        if (peerIdx != room->maxPeers && eventId == 0x2102) {
            const NpSigRoomMsgPayloadView *view = (const NpSigRoomMsgPayloadView *)msgInfo;
            u32 cursor = 0;
            u32 msgType = 0;
            if (npSigTlvReadU32(view->msgBuf, view->msgLen, &cursor, 1u, &msgType) >= 0) {
                switch (msgType) {
                    case 1:
                    case 2:
                    case 3:
                    case 7:
                    case 8:
                        npSigHandleIncomingSignalMsg(sig, session, room, &room->peers[peerIdx],
                                                     view->msgBuf, view->msgLen);
                        break;

                    case 4: {
                        u32 ackCursor = 0;
                        u32 ackType = 0;
                        u32 ackTxId = 0;
                        u32 ackErr = 0;
                        s32 parseRet;

                        parseRet = npSigTlvReadU32(view->msgBuf, view->msgLen, &ackCursor, 1u, &ackType);
                        if (parseRet >= 0 && ackType == 4u) {
                            ackCursor = 0;
                            parseRet = npSigTlvReadU32(view->msgBuf, view->msgLen, &ackCursor, 0x11u, &ackTxId);
                            if (parseRet >= 0) {
                                ackCursor = 0;
                                parseRet = npSigTlvReadU32(view->msgBuf, view->msgLen, &ackCursor, 0x12u, &ackErr);
                                if (parseRet >= 0) {
                                    NpSigPipeMsg pipeMsg;
                                    sceKernelMemset(&pipeMsg, 0, sizeof(pipeMsg));
                                    if (ackErr != 0) {
                                        pipeMsg.msgKind = 10;
                                        pipeMsg.targetId = ackTxId;
                                        pipeMsg.payloadWord = ackErr | 0x8002AC00u;
                                    } else {
                                        pipeMsg.msgKind = 9;
                                        pipeMsg.targetId = ackTxId;
                                    }
                                    npSigSendPipeMsg(&pipeMsg);
                                }
                            }
                        }
                        break;
                    }

                    case 5:
                        npSigHandlePeerNetInfoReqMsg(sig, session, room, &room->peers[peerIdx],
                                                     view->msgBuf, view->msgLen);
                        break;

                    case 6: {
                        NpSigPeerNetInfoNode *resultNode = (NpSigPeerNetInfoNode *)npMatching2Alloc(sizeof(NpSigPeerNetInfoNode));
                        if (resultNode != NULL) {
                            u32 parsedReqId = 0;
                            NpSigLocalNetInfo parsedInfo;
                            sceKernelMemset(resultNode, 0, sizeof(NpSigPeerNetInfoNode));
                            sceKernelMemset(&parsedInfo, 0, sizeof(parsedInfo));
                            if (npSigParsePeerNetInfoTlv(view->msgBuf, view->msgLen, &parsedInfo, &parsedReqId) >= 0) {
                                resultNode->sessionId = session->sessionId;
                                resultNode->reqId = parsedReqId;
                                resultNode->infoSize = parsedInfo.size;
                                resultNode->localAddr = parsedInfo.localAddr;
                                resultNode->mappedAddr = parsedInfo.mappedAddr;
                                resultNode->natType = parsedInfo.natType;
                                resultNode->upnpStatus = parsedInfo.upnpStatus;
                                resultNode->upnpMapped = parsedInfo.upnpMapped;
                                resultNode->mtuSize = parsedInfo.mtuSize;
                                npSigPushPeerNetInfoResult(sig, resultNode);
                                npSigNotifyCb(session, parsedReqId, 0x5103u, 0);
                            }
                        }
                        npSigReleaseSession(sig, session);
                        break;
                    }

                    default:
                        break;
                }
            }
        }
    }

    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }
    if (session != NULL) {
        npSigReleaseSession(sig, session);
    }
}

/* 0x0290E0 */
static s32 NP_MATCHING2_NOINLINE npSigTlvReadU32(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag, u32 *valOut)
{
    u32 pos = *cursorInOut;

    while (pos < bufLen) {
        u16 rawTag = 0;
        u16 rawLen = 0;
        u16 curTag;
        u16 curLen;
        u32 nextPos;

        if (bufLen < pos + 4u) {
            break;
        }
        sceKernelMemcpy(&rawTag, &buf[pos], 2);
        sceKernelMemcpy(&rawLen, &buf[*cursorInOut + 2u], 2);
        curTag = npSwap16(rawTag);
        curLen = npSwap16(rawLen);
        nextPos = *cursorInOut + (u32)curLen + 4u;
        if (bufLen < nextPos) {
            break;
        }
        if (curTag == tag) {
            if (valOut != NULL) {
                u32 rawVal = 0;
                if (curLen != 4u) {
                    return (s32)0x80550e09;
                }
                sceKernelMemcpy(&rawVal, &buf[*cursorInOut + 4u], 4);
                *valOut = npSwap32(rawVal);
            }
            *cursorInOut = (u32)curLen + 4u;
            return 0;
        }
        *cursorInOut = nextPos;
        pos = nextPos;
    }
    return (s32)0x80550e09;
}

/* 0x029214 */
static s32 NP_MATCHING2_NOINLINE npSigBuildSignalTlv(u8 *outBuf, u32 outBufSize, u32 msgType,
                                                     const NpSigSignalPayload *payload, u32 *txIdOut, u32 *sizeOut)
{
    u32 cursor = 0;
    u32 nextSeq;
    u32 txId;
    u16 tagNet;
    u16 lenNet;
    u32 wordNet;
    u16 halfNet;

    if (outBuf != NULL) {
        if (outBufSize < 8u) {
            return (s32)0x80550e08;
        }
        tagNet = npSwap16(1u);
        sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
        lenNet = npSwap16(4u);
        sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
        wordNet = npSwap32(msgType);
        sceKernelMemcpy(&outBuf[cursor + 4u], &wordNet, 4);
    }
    cursor += 8u;

    nextSeq = (g_sigSignalTxSeq == 0xFFFFu) ? 1u : (g_sigSignalTxSeq + 1u);
    g_sigSignalTxSeq = nextSeq;
    txId = nextSeq | 0x42000000u;

    if (outBuf != NULL) {
        if (outBufSize < cursor + 8u) {
            return (s32)0x80550e08;
        }
        tagNet = npSwap16(0x11u);
        sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
        lenNet = npSwap16(4u);
        sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
        wordNet = npSwap32(txId);
        sceKernelMemcpy(&outBuf[cursor + 4u], &wordNet, 4);
    }
    cursor += 8u;

    if (outBuf != NULL) {
        if (outBufSize < cursor + 6u) {
            return (s32)0x80550e08;
        }
        tagNet = npSwap16(2u);
        sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
        lenNet = npSwap16(2u);
        sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
        halfNet = npSwap16(payload->srcTag);
        sceKernelMemcpy(&outBuf[cursor + 4u], &halfNet, 2);
    }
    cursor += 6u;

    if (outBuf != NULL) {
        if (outBufSize < cursor + 6u) {
            return (s32)0x80550e08;
        }
        tagNet = npSwap16(3u);
        sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
        lenNet = npSwap16(2u);
        sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
        halfNet = npSwap16(payload->dstTag);
        sceKernelMemcpy(&outBuf[cursor + 4u], &halfNet, 2);
    }
    cursor += 6u;

    if (msgType == 1u || msgType == 2u) {
        if (outBuf != NULL) {
            if (outBufSize < cursor + 20u) {
                return (s32)0x80550e08;
            }
            tagNet = npSwap16(4u);
            sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
            lenNet = npSwap16(16u);
            sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
            sceKernelMemcpy(&outBuf[cursor + 4u], payload->sessionKey, 16);
        }
        cursor += 20u;
    }

    if (msgType == 1u) {
        if (outBuf != NULL) {
            if (outBufSize < cursor + 6u) {
                return (s32)0x80550e08;
            }
            tagNet = npSwap16(0x14u);
            sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
            lenNet = npSwap16(2u);
            sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
            halfNet = npSwap16(payload->natType);
            sceKernelMemcpy(&outBuf[cursor + 4u], &halfNet, 2);
        }
        cursor += 6u;
    }

    if (msgType == 1u || msgType == 2u) {
        NpSigCand *cand = payload->candList;
        while (cand != NULL) {
            u32 candStart = cursor;
            u16 bodyLen;

            if (outBuf != NULL) {
                if (outBufSize < cursor + 4u) {
                    return (s32)0x80550e08;
                }
                tagNet = npSwap16(5u);
                sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                lenNet = npSwap16(0u);
                sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
            }
            cursor += 4u;

            if (outBuf != NULL) {
                if (outBufSize < cursor + 8u) {
                    return (s32)0x80550e08;
                }
                tagNet = npSwap16(6u);
                sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                lenNet = npSwap16(4u);
                sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
                wordNet = npSwap32(cand->candType);
                sceKernelMemcpy(&outBuf[cursor + 4u], &wordNet, 4);
            }
            cursor += 8u;

            if (outBuf != NULL) {
                if (outBufSize < cursor + 8u) {
                    return (s32)0x80550e08;
                }
                tagNet = npSwap16(7u);
                sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                lenNet = npSwap16(4u);
                sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
                sceKernelMemcpy(&outBuf[cursor + 4u], &cand->peerAddr, 4);
            }
            cursor += 8u;

            if (outBuf != NULL) {
                if (outBufSize < cursor + 6u) {
                    return (s32)0x80550e08;
                }
                tagNet = npSwap16(8u);
                sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                lenNet = npSwap16(2u);
                sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
                halfNet = npSwap16(cand->peerPort);
                sceKernelMemcpy(&outBuf[cursor + 4u], &halfNet, 2);
            }
            cursor += 6u;

            if (msgType == 2u) {
                if (outBuf != NULL) {
                    if (outBufSize < cursor + 8u) {
                        return (s32)0x80550e08;
                    }
                    tagNet = npSwap16(9u);
                    sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                    lenNet = npSwap16(4u);
                    sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
                    sceKernelMemcpy(&outBuf[cursor + 4u], &cand->mappedAddr, 4);
                }
                cursor += 8u;

                if (outBuf != NULL) {
                    if (outBufSize < cursor + 6u) {
                        return (s32)0x80550e08;
                    }
                    tagNet = npSwap16(10u);
                    sceKernelMemcpy(&outBuf[cursor], &tagNet, 2);
                    lenNet = npSwap16(2u);
                    sceKernelMemcpy(&outBuf[cursor + 2u], &lenNet, 2);
                    halfNet = npSwap16(cand->mappedPort);
                    sceKernelMemcpy(&outBuf[cursor + 4u], &halfNet, 2);
                }
                cursor += 6u;
            }

            bodyLen = (u16)(cursor - candStart - 4u);
            if (outBuf != NULL) {
                if (outBufSize < candStart + (u32)bodyLen + 4u) {
                    return (s32)0x80550e08;
                }
                tagNet = npSwap16(5u);
                sceKernelMemcpy(&outBuf[candStart], &tagNet, 2);
                lenNet = npSwap16(bodyLen);
                sceKernelMemcpy(&outBuf[candStart + 2u], &lenNet, 2);
            }
            cand = cand->next;
        }
    }

    if (txIdOut != NULL) {
        *txIdOut = txId;
    }
    if (sizeOut != NULL) {
        *sizeOut = cursor;
    }
    return 0;
}

/* 0x029A38 */
static s32 NP_MATCHING2_NOINLINE npSigParsePeerNetInfoTlv(const u8 *buf, u32 bufLen,
                                                          NpSigLocalNetInfo *infoOut, u32 *reqIdOut)
{
    u32 cursor = 0;
    u32 msgType = 0;
    s32 ret;

    ret = npSigTlvReadU32(buf, bufLen, &cursor, 1u, &msgType);
    if (ret < 0) {
        return ret;
    }
    if (msgType != 6u) {
        return (s32)0x80550e09;
    }

    cursor = 0;
    ret = npSigTlvReadBytes(buf, bufLen, &cursor, 11u, &infoOut->localAddr, 4u, NULL, NULL);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadBytes(buf, bufLen, &cursor, 12u, &infoOut->mappedAddr, 4u, NULL, NULL);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU32(buf, bufLen, &cursor, 13u, &infoOut->upnpStatus);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU32(buf, bufLen, &cursor, 14u, &infoOut->natType);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU32(buf, bufLen, &cursor, 15u, &infoOut->upnpMapped);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU16(buf, bufLen, &cursor, 16u, &infoOut->mtuSize);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU32(buf, bufLen, &cursor, 19u, reqIdOut);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

/* 0x029BD0 */
static s32 NP_MATCHING2_NOINLINE npSigHandleIncomingSignalMsg(NpSigGlobal *sig, NpSigSession *session,
                                                              NpSigRoom *room, NpSigRoomMember *member,
                                                              const u8 *msgBuf, u32 msgLen)
{
    NpSigSignalPayload *payload;
    NpSigConn *conn = NULL;
    s32 pipeKind = 0;
    u32 txId = 0;
    s32 parseOk = 0;
    u8 errByte = 3u;
    s32 ret = 0;
    u8 *ackBuf = NULL;

    payload = (NpSigSignalPayload *)npMatching2Alloc(sizeof(NpSigSignalPayload));
    if (payload != NULL) {
        sceKernelMemset(payload, 0, sizeof(NpSigSignalPayload));
        ret = npSigParseSignalTlv(msgBuf, msgLen, payload, &pipeKind, &txId);
        errByte = (u8)ret;
        if (ret >= 0) {
            if (pipeKind == 11) {
                if (payload->dstTag != 0) {
                    ret = npSigFindConnByNpId(sig, &session->npId, &member->npId, &conn);
                    errByte = (u8)ret;
                    if (ret >= 0) {
                        if (conn->peerSessionTag == 0) {
                            if (conn->localSessionTag == payload->dstTag) {
                                parseOk = 1;
                            } else {
                                errByte = 14u;
                            }
                        } else if (conn->peerSessionTag == payload->srcTag &&
                                   conn->localSessionTag == payload->dstTag) {
                            parseOk = 1;
                        } else {
                            errByte = 14u;
                        }
                    }
                } else {
                    for (;;) {
                        ret = npSigGetOrCreateConn(sig, &session->npId, &member->npId, &conn);
                        errByte = (u8)ret;
                        if (ret < 0) {
                            break;
                        }
                        if (conn->peerSessionTag == 0 || conn->peerSessionTag == payload->srcTag) {
                            parseOk = 1;
                            break;
                        }
                        {
                            s32 lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
                            NpSigPipeMsg pipeMsg;
                            conn->flags |= 0x20u;
                            if (lockRet == 0) {
                                sceKernelUnlockMutex(sig->mainMutex, 1);
                            }
                            sceKernelMemset(&pipeMsg, 0, sizeof(pipeMsg));
                            pipeMsg.msgKind = 32;
                            pipeMsg.targetId = conn->connId;
                            ret = npSigSendPipeMsg(&pipeMsg);
                            errByte = (u8)ret;
                            if (ret < 0) {
                                break;
                            }
                            npSigReleaseConn(sig, conn);
                            conn = NULL;
                        }
                    }
                }
            } else if (pipeKind == 12 || pipeKind == 13 || pipeKind == 33 || pipeKind == 34) {
                ret = npSigFindConnByNpId(sig, &session->npId, &member->npId, &conn);
                errByte = (u8)ret;
                if (ret >= 0) {
                    if (conn->peerSessionTag == payload->srcTag && conn->localSessionTag == payload->dstTag) {
                        parseOk = 1;
                    } else {
                        errByte = 14u;
                    }
                }
            } else {
                parseOk = 1;
            }
        }
    }

    (void)errByte;
    ackBuf = (u8 *)npMatching2Alloc(24u);
    if (ackBuf != NULL) {
        u32 ackCap = 24u;
        u32 cursor = 0;

        if (npSigTlvWriteU32(ackBuf, ackCap, &cursor, 1u, 4u) >= 0 &&
            npSigTlvWriteU32(ackBuf, ackCap, &cursor, 0x11u, txId) >= 0 &&
            npSigTlvWriteU32(ackBuf, ackCap, &cursor, 0x12u, 0u) >= 0) {
            SceNpMatching2SendRoomMessageRequest sendReq;
            SceNpMatching2RequestId sentReqId;

            sceKernelMemset(&sendReq, 0, sizeof(sendReq));
            sendReq.roomId = ((u64)room->roomIdHi << 32) | (u64)room->roomIdLo;
            sendReq.castType = 2;
            sendReq.target.unicastTarget = member->memberId;
            sendReq.msg = ackBuf;
            sendReq.msgLen = ackCap;
            ret = npMatching2SendRoomMessageInternal(session->ctxId, &sendReq, NULL, &sentReqId, 2);
        }
        npMatching2Free(ackBuf);

        if (parseOk != 0) {
            NpSigPipeMsg pipeMsg;
            sceKernelMemset(&pipeMsg, 0, sizeof(pipeMsg));
            pipeMsg.msgKind = (u32)pipeKind;
            pipeMsg.targetId = conn->connId;
            pipeMsg.payloadWord = (u32)payload;
            ret = npSigSendPipeMsg(&pipeMsg);
            if (ret >= 0) {
                return npSigReleaseConn(sig, conn);
            }
        }
    }

    if (conn != NULL) {
        ret = npSigReleaseConn(sig, conn);
    }
    if (payload != NULL) {
        if (payload->candList != NULL) {
            npSigFreeCandList(payload->candList);
        }
        return npMatching2Free(payload);
    }
    return ret;
}

/* 0x02A330 */
static s32 NP_MATCHING2_NOINLINE npSigHandlePeerNetInfoReqMsg(NpSigGlobal *sig, NpSigSession *session,
                                                              NpSigRoom *room, NpSigRoomMember *member,
                                                              const u8 *msgBuf, u32 msgLen)
{
    u32 cursor = 0;
    u32 reqId;
    u32 pktLen;
    s32 ret;

    (void)sig;
    ret = npSigTlvReadU32(msgBuf, msgLen, &cursor, 0x13u, &reqId);
    if (ret >= 0) {
        NpSigLocalNetInfo localInfo;
        sceKernelMemset(&localInfo, 0, sizeof(localInfo));
        localInfo.size = sizeof(localInfo);
        ret = npSigReadLocalNetInfo(&localInfo);
        if (ret >= 0) {
            ret = npSigBuildPeerNetInfoTlv(&localInfo, NULL, 0, &pktLen, 0);
            if (ret >= 0) {
                u8 *pktBuf = (u8 *)npMatching2Alloc(pktLen);
                if (pktBuf != NULL) {
                    ret = npSigBuildPeerNetInfoTlv(&localInfo, pktBuf, pktLen, NULL, reqId);
                    if (ret >= 0) {
                        SceNpMatching2SendRoomMessageRequest sendReq;
                        SceNpMatching2RequestId sentReqId = 0;
                        sceKernelMemset(&sendReq, 0, sizeof(sendReq));
                        sendReq.roomId = ((u64)room->roomIdHi << 32) | (u64)room->roomIdLo;
                        sendReq.castType = 2;
                        sendReq.target.unicastTarget = member->memberId;
                        sendReq.msg = pktBuf;
                        sendReq.msgLen = pktLen;
                        ret = npMatching2SendRoomMessageInternal(session->ctxId, &sendReq, NULL, &sentReqId, 2);
                    }
                    return npMatching2Free(pktBuf);
                }
                ret = 0;
            }
        }
    }
    return ret;
}

/* 0x02A47C */
static s32 NP_MATCHING2_NOINLINE npSigTlvReadBytes(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag,
                                                   void *outBuf, u32 expectedLen, const u8 **ptrOut, u32 *lenOut)
{
    u32 pos = *cursorInOut;

    while (pos < bufLen) {
        u16 rawTag = 0;
        u16 rawLen = 0;
        u16 curTag;
        u16 curLen;
        u32 nextPos;

        if (bufLen < pos + 4u) {
            break;
        }
        sceKernelMemcpy(&rawTag, &buf[pos], 2);
        sceKernelMemcpy(&rawLen, &buf[*cursorInOut + 2u], 2);
        curTag = npSwap16(rawTag);
        curLen = npSwap16(rawLen);
        nextPos = *cursorInOut + (u32)curLen + 4u;
        if (bufLen < nextPos) {
            break;
        }
        if (curTag == tag) {
            if (outBuf != NULL) {
                if ((u32)curLen != expectedLen) {
                    return (s32)0x80550e09;
                }
                sceKernelMemcpy(outBuf, &buf[*cursorInOut + 4u], expectedLen);
            }
            if (ptrOut != NULL) {
                *ptrOut = &buf[*cursorInOut + 4u];
            }
            if (lenOut != NULL) {
                *lenOut = (u32)curLen;
            }
            *cursorInOut += (u32)curLen + 4u;
            return 0;
        }
        *cursorInOut = nextPos;
        pos = nextPos;
    }
    return (s32)0x80550e09;
}

/* 0x02A5F4 */
static s32 NP_MATCHING2_NOINLINE npSigTlvReadU16(const u8 *buf, u32 bufLen, u32 *cursorInOut, u16 tag, u16 *valOut)
{
    u32 pos = *cursorInOut;

    while (pos < bufLen) {
        u16 rawTag = 0;
        u16 rawLen = 0;
        u16 curTag;
        u16 curLen;
        u32 nextPos;

        if (bufLen < pos + 4u) {
            break;
        }
        sceKernelMemcpy(&rawTag, &buf[pos], 2);
        sceKernelMemcpy(&rawLen, &buf[*cursorInOut + 2u], 2);
        curTag = npSwap16(rawTag);
        curLen = npSwap16(rawLen);
        nextPos = *cursorInOut + (u32)curLen + 4u;
        if (bufLen < nextPos) {
            break;
        }
        if (curTag == tag) {
            if (valOut != NULL) {
                u16 rawVal = 0;
                if (curLen != 2u) {
                    return (s32)0x80550e09;
                }
                sceKernelMemcpy(&rawVal, &buf[*cursorInOut + 4u], 2);
                *valOut = npSwap16(rawVal);
            }
            *cursorInOut = (u32)curLen + 4u;
            return 0;
        }
        *cursorInOut = nextPos;
        pos = nextPos;
    }
    return (s32)0x80550e09;
}

/* 0x02A728 */
static s32 NP_MATCHING2_NOINLINE npSigBuildPeerNetInfoTlv(const NpSigLocalNetInfo *info, u8 *outBuf,
                                                          u32 outBufSize, u32 *sizeOut, u32 reqId)
{
    u32 cursor = 0;
    s32 ret;

    ret = npSigTlvWriteU32(outBuf, outBufSize, &cursor, 1u, 6u);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteRaw(outBuf, outBufSize, &cursor, 11u, &info->localAddr, 4u);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteRaw(outBuf, outBufSize, &cursor, 12u, &info->mappedAddr, 4u);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteU32(outBuf, outBufSize, &cursor, 13u, info->upnpStatus);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteU32(outBuf, outBufSize, &cursor, 14u, info->natType);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteU32(outBuf, outBufSize, &cursor, 15u, info->upnpMapped);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteU16(outBuf, outBufSize, &cursor, 16u, info->mtuSize);
    if (ret < 0) {
        return ret;
    }
    ret = npSigTlvWriteU32(outBuf, outBufSize, &cursor, 19u, reqId);
    if (ret < 0) {
        return ret;
    }
    if (sizeOut != NULL) {
        *sizeOut = cursor;
    }
    return 0;
}

/* 0x02AC48 */
static s32 NP_MATCHING2_NOINLINE npSigParseSignalTlv(const u8 *buf, u32 bufLen, NpSigSignalPayload *payloadOut,
                                                     s32 *pipeKindOut, u32 *txIdOut)
{
    NpSigCand *head = NULL;
    NpSigCand *tail = NULL;
    u32 cursor = 0;
    u32 msgType = 0;
    s32 mappedKind;
    s32 ret;

    ret = npSigTlvReadU32(buf, bufLen, &cursor, 1u, &msgType);
    if (ret < 0) {
        return ret;
    }

    if (msgType == 1u) {
        mappedKind = 11;
    } else if (msgType == 2u) {
        mappedKind = 12;
    } else if (msgType == 3u) {
        mappedKind = 13;
    } else if (msgType == 7u) {
        mappedKind = 33;
    } else if (msgType == 8u) {
        mappedKind = 34;
    } else {
        return (s32)0x80550e09;
    }
    *pipeKindOut = mappedKind;

    cursor = 0;
    ret = npSigTlvReadU32(buf, bufLen, &cursor, 0x11u, txIdOut);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU16(buf, bufLen, &cursor, 2u, &payloadOut->srcTag);
    if (ret < 0) {
        return ret;
    }
    cursor = 0;
    ret = npSigTlvReadU16(buf, bufLen, &cursor, 3u, &payloadOut->dstTag);
    if (ret < 0) {
        return ret;
    }

    if (*pipeKindOut == 11 || *pipeKindOut == 12) {
        cursor = 0;
        ret = npSigTlvReadBytes(buf, bufLen, &cursor, 4u, payloadOut->sessionKey, 16u, NULL, NULL);
        if (ret < 0) {
            return ret;
        }
    }

    if (*pipeKindOut == 11) {
        cursor = 0;
        if (npSigTlvReadU16(buf, bufLen, &cursor, 0x14u, &payloadOut->natType) < 0) {
            payloadOut->natType = 0;
        }
    }

    if (*pipeKindOut != 11 && *pipeKindOut != 12) {
        payloadOut->candList = NULL;
        return 0;
    }

    cursor = 0;
    for (;;) {
        const u8 *candBuf = NULL;
        u32 candLen = 0;
        u32 innerCursor;
        NpSigCand *cand;

        ret = npSigTlvReadBytes(buf, bufLen, &cursor, 5u, NULL, 0, &candBuf, &candLen);
        if (ret < 0) {
            break;
        }
        cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
        if (cand == NULL) {
            ret = (s32)0x80550e03;
            while (head != NULL) {
                NpSigCand *next = head->next;
                npMatching2Free(head);
                head = next;
            }
            return ret;
        }
        sceKernelMemset(cand, 0, sizeof(NpSigCand));

        innerCursor = 0;
        ret = npSigTlvReadU32(candBuf, candLen, &innerCursor, 6u, &cand->candType);
        if (ret >= 0) {
            innerCursor = 0;
            ret = npSigTlvReadBytes(candBuf, candLen, &innerCursor, 7u, &cand->peerAddr, 4u, NULL, NULL);
        }
        if (ret >= 0) {
            innerCursor = 0;
            ret = npSigTlvReadU16(candBuf, candLen, &innerCursor, 8u, &cand->peerPort);
        }
        if (ret >= 0 && *pipeKindOut == 12) {
            innerCursor = 0;
            ret = npSigTlvReadBytes(candBuf, candLen, &innerCursor, 9u, &cand->mappedAddr, 4u, NULL, NULL);
            if (ret >= 0) {
                innerCursor = 0;
                ret = npSigTlvReadU16(candBuf, candLen, &innerCursor, 10u, &cand->mappedPort);
            }
        }
        if (ret < 0) {
            npMatching2Free(cand);
            while (head != NULL) {
                NpSigCand *next = head->next;
                npMatching2Free(head);
                head = next;
            }
            return ret;
        }
        if (head != NULL) {
            tail->next = cand;
            tail = cand;
        } else {
            head = cand;
            tail = cand;
        }
    }

    if (head != NULL) {
        payloadOut->candList = head;
        return 0;
    }
    return ret;
}

/* 0x02AFC4 */
s32 NP_MATCHING2_NOINLINE npSigSendBandwidthProbe(NpSigGlobal *sig, NpSigConn *conn)
{
    u8 pktBuf[1120];
    SceNetInetSockaddrIn dstAddr;
    u32 tickPair[2];
    s32 offset;
    s32 sendIdx;
    s32 ret = 0;

    sceNp_6CBB0614(tickPair);
    npSigBuildEchoPacket(pktBuf, 3u, conn, tickPair[1], tickPair[0], 0, 0);
    for (offset = 0; offset < 1024; offset += 4) {
        u32 randWord = sceNet_lib_1858883D();
        s32 remain = 1024 - offset;
        s32 copyLen = (remain < 4) ? remain : 4;
        sceKernelMemcpy(&pktBuf[offset + 88], &randWord, (u32)copyLen);
    }

    sceKernelMemset(&dstAddr, 0, sizeof(dstAddr));
    dstAddr.sinFamily = 2;
    dstAddr.sinPort = conn->peerSelectedPort;
    dstAddr.sinAddr = conn->peerSelectedAddr;
    dstAddr.sinVport = npSwap16(0xFFFEu);

    for (sendIdx = 0; sendIdx < 2; sendIdx++) {
        ret = npSigQueueEchoSend(sig, conn->selectedSockFd, pktBuf, 1112u, 0, &dstAddr, sizeof(dstAddr));
        if (ret < 0) {
            return ret;
        }
    }

    if (conn->bwProbeSentCount < 8u) {
        sceNet_lib_A8B6205A(&conn->bwTimer, 500000u, npSigTimerBwCb, conn->connId);
        conn->bwProbeSentCount++;
        return ret;
    }
    sceNet_lib_A8B6205A(&conn->bwTimer, (sceNet_lib_1858883D() / 108u) + 50000000u, npSigTimerBwCb, conn->connId);
    return ret;
}

/* 0x02B178 */
s32 NP_MATCHING2_NOINLINE npSigEchoThread(u32 args, void *argp)
{
    NpSigGlobal *sig = *(NpSigGlobal **)argp;
    NpSigUpnpInfo upnpInfo;
    SceNetInetSockaddrIn udpAddr;
    SceNetInetSockaddrIn sigAddr;
    NpSigStunAuthParam authParam;
    char hostBuf[48];
    char envCode[16];
    u16 bindPort = 0;
    s32 setupOk = 0;

    (void)args;
    while (sig->stopEchoFlag == 0) {
        sceKernelMemset(&upnpInfo, 0, sizeof(upnpInfo));
        if (sceNetUpnp_1038E77A(&upnpInfo) < 0) {
            break;
        }
        if (upnpInfo.natDetectStatus != 0) {
            if (upnpInfo.natDetectStatus == 2 && upnpInfo.upnpStatus == 2) {
                if (upnpInfo.upnpMapped == 2) {
                    bindPort = npSwap16(upnpInfo.upnpPort);
                    setupOk = 1;
                } else {
                    s32 udpSock = sceNetInetSocket(2, 2, 0);
                    if (udpSock >= 0) {
                        sceKernelMemset(&udpAddr, 0, sizeof(udpAddr));
                        udpAddr.sinFamily = (u8)upnpInfo.upnpStatus;
                        udpAddr.sinPort = npSwap16(upnpInfo.upnpPort);
                        if (sceNetInetBind(udpSock, &udpAddr, sizeof(udpAddr)) >= 0) {
                            sig->sharedUdpSock = udpSock;
                            bindPort = npSwap16(upnpInfo.upnpPort);
                            setupOk = 1;
                        } else {
                            sceNetInetClose(udpSock);
                        }
                    }
                }
            } else {
                s32 udpSock = sceNetInetSocket(2, 2, 0);
                if (udpSock < 0) {
                    sceNetInetGetPspError();
                } else {
                    u32 addrLen = sizeof(udpAddr);
                    sceKernelMemset(&udpAddr, 0, sizeof(udpAddr));
                    udpAddr.sinFamily = 2;
                    if (sceNetInetBind(udpSock, &udpAddr, sizeof(udpAddr)) >= 0 &&
                        sceNetInetGetsockname(udpSock, &udpAddr, &addrLen) >= 0) {
                        sig->sharedUdpSock = udpSock;
                        bindPort = udpAddr.sinPort;
                        setupOk = 1;
                    } else {
                        sceNetInetGetPspError();
                        sceNetInetClose(udpSock);
                    }
                }
            }
            break;
        }
        sceKernelDelayThread(100000u);
    }

    if (setupOk != 0) {
        u32 remainLen;
        s32 needRebind;

        sig->sharedBindPort = bindPort;
        sig->natType = upnpInfo.natType;
        envCode[0] = (char)sig->selfNpId.opt[2];
        envCode[1] = (char)sig->selfNpId.opt[3];
        envCode[2] = '\0';

        sceKernelMemset(&authParam, 0, sizeof(authParam));
        sceKernelMemset(hostBuf, 0, 33);
        sceNp_4B09907A(hostBuf, envCode, 33u);
        remainLen = 33u - sceNet_lib_D8722983(envCode);
        sceNp_4B09907A(hostBuf, ".", remainLen);
        remainLen -= 1u;
        sceNp_4B09907A(hostBuf, sig->clientVer, remainLen);
        remainLen -= sceNet_lib_D8722983(sig->clientVer);
        sceNp_4B09907A(hostBuf, ".", remainLen);
        sceNp_4B09907A(hostBuf, "stun.playstation.net", remainLen - 1u);

        authParam.version = 1u;
        authParam.keyId = npSwap32(1u);
        sceKernelMemcpy(authParam.key, g_sigStunAuthKey, 20);
        sceNetStun_EBF3C24D(sig->stackSize, sig->priority, hostBuf, 3478u, &authParam, npSigStunCb, &sig->stunHandle);

        for (;;) {
            s32 sigSock = sceNetInetSocket(2, 6, 0);
            u16 ioctlPort;

            if (sigSock < 0) {
                sceNetInetGetPspError();
                break;
            }
            sig->sharedSigSock = sigSock;
            ioctlPort = sig->sharedBindPort;
            if (sceNetInet_lib_AEE60F84(sigSock, (s32)0x800250CF, &ioctlPort) < 0) {
                break;
            }
            sceKernelMemset(&sigAddr, 0, sizeof(sigAddr));
            sigAddr.sinFamily = 2;
            sigAddr.sinPort = npSwap16(0x0E4Au);
            sigAddr.sinAddr = 0;
            sigAddr.sinVport = npSwap16(0xFFFEu);
            if (sceNetInetBind(sig->sharedSigSock, &sigAddr, sizeof(sigAddr)) < 0) {
                sceNetInetGetPspError();
                break;
            }

            needRebind = 0;
            while (sig->stopEchoFlag == 0) {
                s32 pollRet;
                for (;;) {
                    void *msgPtr = NULL;
                    pollRet = sceKernelPollMbx(sig->echoMbx, &msgPtr);
                    if (pollRet == (s32)0x80020192) {
                        break;
                    }
                    if (pollRet < 0) {
                        break;
                    }
                    if (msgPtr != NULL) {
                        NpSigEchoMbxMsg *mbxMsg = (NpSigEchoMbxMsg *)msgPtr;
                        sceNetInetSendto(mbxMsg->sockFd, mbxMsg->pktBuf, mbxMsg->pktLen, 0,
                                         &mbxMsg->dstAddr, mbxMsg->addrLen);
                        npMatching2Free(mbxMsg->pktBuf);
                        npMatching2Free(mbxMsg);
                    }
                }
                if (pollRet < 0 && pollRet != (s32)0x80020192) {
                    break;
                }
                needRebind = 0;
                if (npSigPollEchoSockets(sig, &needRebind) < 0) {
                    if (needRebind != 0) {
                        break;
                    }
                    break;
                }
                if (needRebind != 0) {
                    break;
                }
            }
            if (needRebind == 0) {
                break;
            }
        }
    }

    if (sig->sharedSigSock >= 0) {
        u16 resetPort = npSwap16(0x0E4Au);
        sceNetInet_lib_AEE60F84(sig->sharedSigSock, (s32)0x800250CF, &resetPort);
        sceNetInetClose(sig->sharedSigSock);
    }
    sig->sharedSigSock = -1;
    if (sig->stunHandle != 0) {
        sceNetStun_4403D9E7(sig->stunHandle);
    }
    sig->stunHandle = 0;
    if (sig->sharedUdpSock >= 0) {
        sceNetInetClose(sig->sharedUdpSock);
        sig->sharedUdpSock = -1;
    }
    sig->sharedBindPort = npSwap16(0x0E4Au);
    sig->natType = 0;
    sceNetFreeThreadinfo(0);
    return 0;
}

/* 0x02B5EC */
s32 NP_MATCHING2_NOINLINE npSigDispatchEchoEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigPipeMsg copy = *msg;

    switch (copy.msgKind) {
        case 15:
            return npSigHandleProbeTimerEvent(sig, &copy);
        case 16:
            return npSigHandleEchoReqEvent(sig, &copy);
        case 17:
            return npSigHandleEchoRespEvent(sig, &copy);
        case 24:
            return npSigHandleBwTimerEvent(sig, &copy);
        default:
            return 0;
    }
}

/* 0x02B690 */
static s32 NP_MATCHING2_NOINLINE npSigHandleProbeTimerEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    u8 pktBuf[96];
    SceNetInetSockaddrIn dstAddr;
    u32 tickPair[2];
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        u64 nowTick;
        sceNp_6CBB0614(tickPair);
        nowTick = ((u64)tickPair[1] << 32) | (u64)tickPair[0];

        if (conn->state == 7) {
            if (conn->echoDeadlineTick >= nowTick) {
                NpSigCand *cand;
                npSigBuildEchoPacket(pktBuf, 1u, conn, tickPair[1], tickPair[0], 0, 0);
                sceKernelMemset(&dstAddr, 0, sizeof(dstAddr));
                dstAddr.sinFamily = 2;
                for (cand = conn->candList; cand != NULL; cand = cand->next) {
                    if ((cand->flags & 2u) == 0) {
                        dstAddr.sinAddr = cand->peerAddr;
                        dstAddr.sinPort = cand->peerPort;
                        dstAddr.sinVport = npSwap16(0xFFFEu);
                        if (cand->sockFd < 0) {
                            cand->sockFd = sig->sharedSigSock;
                            cand->localPort = sig->sharedBindPort;
                            if (conn->symSigSock >= 0 && cand->candType == 3u) {
                                cand->sockFd = conn->symSigSock;
                                cand->localPort = conn->symLocalPort;
                            }
                        }
                        ret = npSigQueueEchoSend(sig, cand->sockFd, pktBuf, 88u, 0, &dstAddr, sizeof(dstAddr));
                        cand->sentCount++;
                    }
                }
                sceNet_lib_A8B6205A(&conn->echoTimer, 500000u, npSigTimerProbeCb, conn->connId);
                if ((conn->flags & 0x10u) != 0) {
                    conn->flags &= ~0x10u;
                    sig->symStunBusy = 0;
                }
            } else {
                NpSigCand *cand;
                NpSigCand *bestCand = NULL;
                for (cand = conn->candList; cand != NULL; cand = cand->next) {
                    if ((cand->flags & 1u) != 0) {
                        if (bestCand == NULL || cand->candType < bestCand->candType) {
                            bestCand = cand;
                        }
                    }
                }
                if (bestCand == NULL) {
                    ret = (s32)0x80550e0f;
                } else {
                    ret = npSigSelectBestCand(conn, bestCand, tickPair[0], tickPair[1]);
                    if (ret >= 0 && (conn->flags & 0x10u) != 0) {
                        conn->flags &= ~0x10u;
                        sig->symStunBusy = 0;
                    }
                }
            }
        } else if (conn->state > 7 && conn->state < 11) {
            if (conn->echoDeadlineTick < nowTick) {
                ret = (s32)0x80550e11;
            } else {
                npSigBuildEchoPacket(pktBuf, 1u, conn, tickPair[1], tickPair[0], 0, 0);
                sceKernelMemset(&dstAddr, 0, sizeof(dstAddr));
                dstAddr.sinFamily = 2;
                dstAddr.sinPort = conn->peerSelectedPort;
                dstAddr.sinAddr = conn->peerSelectedAddr;
                dstAddr.sinVport = npSwap16(0xFFFEu);
                ret = npSigQueueEchoSend(sig, conn->selectedSockFd, pktBuf, 88u, 0, &dstAddr, sizeof(dstAddr));
                if (ret >= 0) {
                    u32 sampleIdx = conn->rttSampleCount;
                    conn->rttSamples[sampleIdx % 6u] = conn->rttLastSample;
                    conn->rttLastSample = 0;
                    conn->rttSampleCount = sampleIdx + 1u;
                    sceNet_lib_A8B6205A(&conn->echoTimer, 10000000u, npSigTimerProbeCb, conn->connId);
                }
            }
        }
    }

    if (conn != NULL && ret < 0) {
        ret = npSigCloseConn(sig, conn, ret, 0);
    }
    if (conn != NULL) {
        return npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x02BAA0 */
static s32 NP_MATCHING2_NOINLINE npSigHandleEchoReqEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigEchoEventData *evData = (NpSigEchoEventData *)msg->payloadWord;
    NpSigConn *conn = NULL;
    u8 pktBuf[96];
    SceNetInetSockaddrIn dstAddr;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        if (conn->state == 7) {
            NpSigCand *cand;
            for (cand = conn->candList; cand != NULL; cand = cand->next) {
                if (cand->peerAddr == evData->peerAddr && cand->peerPort == evData->peerPort) {
                    break;
                }
            }
            if (cand == NULL) {
                cand = (NpSigCand *)npMatching2Alloc(sizeof(NpSigCand));
                if (cand == NULL) {
                    ret = (s32)0x80550e03;
                } else {
                    sceKernelMemset(cand, 0, sizeof(NpSigCand));
                    cand->candType = 2u;
                    cand->peerAddr = evData->peerAddr;
                    cand->peerPort = evData->peerPort;
                    cand->sockFd = evData->recvSockFd;
                    cand->localPort = (evData->recvSockFd == conn->symSigSock) ? conn->symLocalPort : sig->sharedBindPort;
                    cand->next = conn->candList;
                    conn->candList = cand;
                }
            }
        }

        if (ret >= 0 && conn->state >= 7 && conn->state < 11) {
            npSigBuildEchoPacket(pktBuf, 2u, conn, evData->stampHi, evData->stampLo,
                                 evData->peerAddr, evData->peerPort);
            sceKernelMemset(&dstAddr, 0, sizeof(dstAddr));
            dstAddr.sinFamily = 2;
            dstAddr.sinAddr = evData->peerAddr;
            dstAddr.sinPort = evData->peerPort;
            dstAddr.sinVport = npSwap16(0xFFFEu);
            ret = npSigQueueEchoSend(sig, evData->recvSockFd, pktBuf, 88u, 0, &dstAddr, sizeof(dstAddr));
        }
    }

    if (conn != NULL && ret < 0) {
        ret = npSigCloseConn(sig, conn, ret, 0);
    }
    if (conn != NULL) {
        ret = npSigReleaseConn(sig, conn);
    }
    if (evData != NULL) {
        return npMatching2Free(evData);
    }
    return ret;
}

/* 0x02BCD4 */
static s32 NP_MATCHING2_NOINLINE npSigHandleEchoRespEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigEchoEventData *evData = (NpSigEchoEventData *)msg->payloadWord;
    NpSigConn *conn = NULL;
    u32 tickPair[2];
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0) {
        u32 rttUsec;
        sceNp_6CBB0614(tickPair);
        rttUsec = tickPair[0] - evData->stampLo;

        if (conn->state == 7) {
            NpSigCand *cand;
            for (cand = conn->candList; cand != NULL; cand = cand->next) {
                if (cand->peerAddr == evData->peerAddr && cand->peerPort == evData->peerPort) {
                    break;
                }
            }
            if (cand != NULL) {
                cand->recvCount++;
                if ((cand->flags & 1u) == 0) {
                    NpSigCand *iter;
                    s32 hasHigherPriority = 0;

                    cand->flags |= 1u;
                    cand->mappedAddr = evData->reflectedAddr;
                    cand->mappedPort = evData->reflectedPort;
                    cand->rttUsec = rttUsec;

                    for (iter = conn->candList; iter != NULL; iter = iter->next) {
                        if (cand->candType < iter->candType) {
                            iter->flags |= 2u;
                        }
                    }
                    for (iter = conn->candList; iter != NULL; iter = iter->next) {
                        if (iter->candType < cand->candType) {
                            hasHigherPriority = 1;
                            break;
                        }
                    }
                    if (hasHigherPriority != 0) {
                        u64 soonTick = (((u64)tickPair[1] << 32) | (u64)tickPair[0]) + 1000000ULL;
                        if (soonTick < conn->echoDeadlineTick) {
                            conn->echoDeadlineTick = soonTick;
                        }
                    } else {
                        ret = npSigSelectBestCand(conn, cand, tickPair[0], tickPair[1]);
                    }
                }
            }
        } else if (conn->state > 7 && conn->state < 11) {
            if (conn->peerSelectedAddr == evData->peerAddr && conn->peerSelectedPort == evData->peerPort) {
                u64 nowTick = ((u64)tickPair[1] << 32) | (u64)tickPair[0];
                conn->rttLastSample = (s32)rttUsec;
                conn->echoDeadlineTick = nowTick + 60000000ULL;
            }
        }
    }

    if (conn != NULL) {
        ret = npSigReleaseConn(sig, conn);
    }
    if (evData != NULL) {
        return npMatching2Free(evData);
    }
    return ret;
}

/* 0x02BF34 */
static s32 NP_MATCHING2_NOINLINE npSigHandleBwTimerEvent(NpSigGlobal *sig, const NpSigPipeMsg *msg)
{
    NpSigConn *conn = NULL;
    s32 ret;

    ret = npSigFindConnById(sig, msg->targetId, &conn);
    if (ret >= 0 && conn->state == 10) {
        ret = npSigSendBandwidthProbe(sig, conn);
    }
    if (conn != NULL && ret < 0) {
        ret = npSigCloseConn(sig, conn, ret, 0);
    }
    if (conn != NULL) {
        return npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x02BFF8 */
static s32 NP_MATCHING2_NOINLINE npSigQueueEchoSend(NpSigGlobal *sig, s32 sockFd, const void *pktBuf, u32 pktLen,
                                                    s32 flags, const SceNetInetSockaddrIn *dstAddr, u32 addrLen)
{
    NpSigEchoMbxMsg *mbxMsg;
    void *pktCopy;

    (void)flags;
    mbxMsg = (NpSigEchoMbxMsg *)npMatching2Alloc(sizeof(NpSigEchoMbxMsg));
    if (mbxMsg == NULL) {
        return (s32)0x80550e03;
    }
    pktCopy = npMatching2Alloc(pktLen);
    if (pktCopy == NULL) {
        npMatching2Free(mbxMsg);
        return (s32)0x80550e03;
    }
    sceKernelMemset(mbxMsg, 0, sizeof(NpSigEchoMbxMsg));
    sceKernelMemcpy(pktCopy, pktBuf, pktLen);
    mbxMsg->sockFd = sockFd;
    mbxMsg->pktLen = pktLen;
    mbxMsg->pktBuf = pktCopy;
    sceKernelMemcpy(&mbxMsg->dstAddr, dstAddr, addrLen);
    mbxMsg->addrLen = addrLen;
    sceKernelSendMbx(sig->echoMbx, mbxMsg);
    return sceNetThreadAbort(sig->echoThid);
}

/* 0x02C0FC */
static s32 NP_MATCHING2_NOINLINE npSigBuildEchoPacket(u8 *pktBuf, u8 pktType, NpSigConn *conn,
                                                      u32 stampHi, u32 stampLo, u32 peerAddr, u16 peerPort)
{
    u16 halfNet;
    u32 wordNet;

    sceKernelMemset(pktBuf, 0, 88);
    sceKernelMemcpy(&pktBuf[0], &pktType, 1);
    sceNet_lib_B5CE388A((char *)&pktBuf[4], conn->selfNpId.handle, 16);
    sceKernelMemcpy(&pktBuf[20], conn->selfNpId.opt, 8);
    sceKernelMemcpy(&pktBuf[28], conn->selfNpId.reserved, 8);
    sceNet_lib_B5CE388A((char *)&pktBuf[36], conn->peerNpId.handle, 16);
    sceKernelMemcpy(&pktBuf[52], conn->peerNpId.opt, 8);
    sceKernelMemcpy(&pktBuf[60], conn->peerNpId.reserved, 8);

    halfNet = npSwap16(conn->localSessionTag);
    sceKernelMemcpy(&pktBuf[68], &halfNet, 2);
    halfNet = npSwap16(conn->peerSessionTag);
    sceKernelMemcpy(&pktBuf[70], &halfNet, 2);

    wordNet = npSwap32(stampHi);
    sceKernelMemcpy(&pktBuf[72], &wordNet, 4);
    wordNet = npSwap32(stampLo);
    sceKernelMemcpy(&pktBuf[76], &wordNet, 4);

    if (pktType == 2u) {
        u32 maskWord = ((u32)npSwap16(conn->localSessionTag) << 16) | (u32)npSwap16(conn->peerSessionTag);
        wordNet = peerAddr ^ maskWord;
        sceKernelMemcpy(&pktBuf[80], &wordNet, 4);
        halfNet = (u16)(npSwap16(conn->localSessionTag) ^ peerPort);
        sceKernelMemcpy(&pktBuf[84], &halfNet, 2);
    } else if (pktType == 3u) {
        wordNet = npSwap32(npSigComputeBandwidth(conn));
        sceKernelMemcpy(&pktBuf[80], &wordNet, 4);
    }
    return 0;
}

/* 0x02C2E8 */
static s32 NP_MATCHING2_NOINLINE npSigSelectBestCand(NpSigConn *conn, NpSigCand *bestCand, u32 nowLo, u32 nowHi)
{
    NpSigSignalPayload payload;
    NpSigCand candCopy;
    s32 cmpRes = 0;
    s32 ret;

    sceNet_lib_A93A93E9(&conn->echoTimer);
    sceKernelMemset(&candCopy, 0, sizeof(candCopy));
    sceKernelMemcpy(&candCopy, bestCand, sizeof(candCopy));
    candCopy.next = NULL;

    sceKernelMemset(&payload, 0, sizeof(payload));
    payload.srcTag = conn->localSessionTag;
    payload.dstTag = conn->peerSessionTag;
    sceKernelMemcpy(payload.sessionKey, conn->localKey, 16);
    payload.candList = &candCopy;

    ret = npSigSendSignalPacket(conn, 2u, &payload);
    if (ret >= 0) {
        ret = sceNp_2D8E93D2(&conn->selfNpId, &conn->peerNpId, &cmpRes);
        if (ret >= 0) {
            s32 sampleIdx;
            u64 nowTick = ((u64)nowHi << 32) | (u64)nowLo;
            if (cmpRes >= 0) {
                conn->localMappedAddr = bestCand->mappedAddr;
                conn->localMappedPort = bestCand->mappedPort;
                conn->peerSelectedAddr = bestCand->peerAddr;
                conn->peerSelectedPort = bestCand->peerPort;
                conn->selectedSockFd = bestCand->sockFd;
                conn->selectedLocalPort = bestCand->localPort;
            }
            conn->echoDeadlineTick = nowTick + 60000000ULL;
            for (sampleIdx = 0; sampleIdx < 6; sampleIdx++) {
                if (sampleIdx < (s32)bestCand->recvCount) {
                    conn->rttSamples[sampleIdx] = (s32)bestCand->rttUsec;
                } else if (sampleIdx < (s32)bestCand->sentCount) {
                    conn->rttSamples[sampleIdx] = 0;
                } else {
                    conn->rttSamples[sampleIdx] = -1;
                }
            }
            conn->rttSampleCount = (u32)bestCand->sentCount;
            conn->state = 8;
            conn->rttLastSample = -1;
            sceNet_lib_A8B6205A(&conn->echoTimer, 10000000u, npSigTimerProbeCb, conn->connId);
        }
    }
    return ret;
}

/* 0x02C4BC */
static s32 NP_MATCHING2_NOINLINE npSigPollEchoSockets(NpSigGlobal *sig, s32 *needRebindOut)
{
    SceNetInetFdSet readFds;
    SceNetInetTimeval tv;
    s32 maxSock;
    s32 lockRet;
    s32 selLock;
    s32 selRet;
    NpSigConn *symConn;
    s32 wordIdx;

    *needRebindOut = 0;
    for (wordIdx = 0; wordIdx < 8; wordIdx++) {
        readFds.fdsBits[wordIdx] = 0;
    }

    maxSock = sig->sharedSigSock;
    readFds.fdsBits[maxSock >> 5] |= 1u << (maxSock & 31);

    lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
    for (symConn = sig->symConnList; symConn != NULL; symConn = symConn->symNext) {
        s32 symSock = symConn->symSigSock;
        readFds.fdsBits[symSock >> 5] |= 1u << (symSock & 31);
        if (symSock > maxSock) {
            maxSock = symSock;
        }
    }
    if (lockRet == 0) {
        sceKernelUnlockMutex(sig->mainMutex, 1);
    }

    tv.tvSec = 0;
    tv.tvUsec = 200000;
    selLock = sceKernelLockMutex(sig->selectMutex, 1, NULL);
    selRet = sceNetInetSelect(maxSock + 1, &readFds, NULL, NULL, &tv);
    if (selLock == 0) {
        sceKernelUnlockMutex(sig->selectMutex, 1);
    }

    if (selRet != 0) {
        s32 pspErr = sceNetInetGetPspError();
        if (pspErr == (s32)0x80010009) {
            return 0;
        }
        pspErr = sceNetInetGetPspError();
        if (pspErr == (s32)0x80010004) {
            return 0;
        }
        if (selRet < 0) {
            return sceNetInetGetPspError();
        }
        if ((readFds.fdsBits[sig->sharedSigSock >> 5] & (1u << (sig->sharedSigSock & 31))) != 0) {
            s32 recvRet = npSigRecvEchoPacket(sig, sig->sharedSigSock);
            if (recvRet == (s32)0x80010004) {
                return 0;
            }
            if (recvRet < 0) {
                sceNetInetClose(sig->sharedSigSock);
                sig->sharedSigSock = -1;
                *needRebindOut = 1;
                return recvRet;
            }
        }

        lockRet = sceKernelLockMutex(sig->mainMutex, 1, NULL);
        for (symConn = sig->symConnList; symConn != NULL; symConn = symConn->symNext) {
            s32 symSock = symConn->symSigSock;
            if ((readFds.fdsBits[symSock >> 5] & (1u << (symSock & 31))) != 0) {
                if (npSigRecvEchoPacket(sig, symSock) == (s32)0x80010004) {
                    break;
                }
            }
        }
        if (lockRet == 0) {
            sceKernelUnlockMutex(sig->mainMutex, 1);
        }
    }
    return 0;
}

/* 0x02C75C */
static u32 NP_MATCHING2_NOINLINE npSigComputeBandwidth(NpSigConn *conn)
{
    u32 sorted[8];
    s32 count;

    sceKernelMemset(sorted, 0, sizeof(sorted));
    for (count = 0; count < 8; count++) {
        u32 sample = conn->bwSamples[count];
        s32 insertPos = 0;
        if (sample == 0) {
            break;
        }
        while (insertPos < 8) {
            if (sorted[insertPos] == 0 || sample < sorted[insertPos]) {
                s32 shiftIdx = count;
                while (shiftIdx > insertPos) {
                    sorted[shiftIdx] = sorted[shiftIdx - 1];
                    shiftIdx--;
                }
                sorted[insertPos] = sample;
                break;
            }
            insertPos++;
        }
    }

    if (count != 0) {
        u32 median;
        if ((count & 1) != 0) {
            median = sorted[(count - 1) / 2];
        } else {
            median = (sorted[(count / 2) - 1] + sorted[count / 2]) >> 1;
        }
        return (u32)(9184000000ULL / (u64)median);
    }
    return 0;
}

/* 0x02C888 */
static s32 NP_MATCHING2_NOINLINE npSigRecvEchoPacket(NpSigGlobal *sig, s32 sockFd)
{
    u8 rawBuf[2048];
    SceNetInetSockaddrIn fromAddr;
    NpSigEchoParsedPacket parsed;
    NpSigConn *conn = NULL;
    u32 fromLen = sizeof(fromAddr);
    u64 nowTick = 0;
    s32 ret = 0;

    if (sceNetInetRecvfrom(sockFd, rawBuf, sizeof(rawBuf), 0, &fromAddr, &fromLen) < 0) {
        return sceNetInetGetPspError();
    }

    sceNp_6CBB0614(&nowTick);
    npSigParseEchoPacket(&parsed, rawBuf);
    if (npSigFindConnByNpId(sig, &parsed.dstNpId, &parsed.srcNpId, &conn) >= 0) {
        if (conn->peerSessionTag == npSwap16(parsed.srcTagNet) &&
            conn->localSessionTag == npSwap16(parsed.dstTagNet)) {
            if (parsed.pktType == 1u || parsed.pktType == 2u) {
                NpSigEchoEventData *evData = (NpSigEchoEventData *)npMatching2Alloc(sizeof(NpSigEchoEventData));
                if (evData != NULL) {
                    NpSigPipeMsg pipeMsg;
                    u32 sentBytes = 0;

                    evData->peerAddr = fromAddr.sinAddr;
                    evData->peerPort = fromAddr.sinPort;
                    evData->reflectedAddr = parsed.reflectedAddr;
                    evData->reflectedPort = parsed.reflectedPort;
                    evData->stampHi = npSwap32(parsed.stampHiNet);
                    evData->stampLo = npSwap32(parsed.stampLoNet);
                    evData->recvSockFd = sockFd;

                    pipeMsg.msgKind = (parsed.pktType == 2u) ? 17u : 16u;
                    pipeMsg.targetId = conn->connId;
                    pipeMsg.payloadWord = (u32)evData;
                    pipeMsg.auxWord = 0;

                    ret = sceKernelSendMsgPipe(sig->msgPipe, &pipeMsg, sizeof(pipeMsg), 0, &sentBytes, NULL);
                    if (ret < 0) {
                        npMatching2Free(evData);
                    }
                }
            } else if (parsed.pktType == 3u) {
                u32 stampHi = npSwap32(parsed.stampHiNet);
                u32 stampLo = npSwap32(parsed.stampLoNet);
                if (conn->bwLastStampLo == stampLo && conn->bwLastStampHi == stampHi) {
                    u32 slotIdx = (u32)conn->bwSampleIndex;
                    conn->bwSampleIndex = (u8)((slotIdx + 1u) & 7u);
                    conn->bwSamples[slotIdx] = (u32)nowTick - (u32)conn->bwFirstRecvTick;
                } else {
                    conn->bwLastStampLo = stampLo;
                    conn->bwLastStampHi = stampHi;
                    conn->bwFirstRecvTick = nowTick;
                }
                conn->peerBandwidth = parsed.bandwidthNet;
            }
        }
    }

    if (conn != NULL) {
        npSigReleaseConn(sig, conn);
    }
    return ret;
}

/* 0x02CAF8 */
static s32 NP_MATCHING2_NOINLINE npSigParseEchoPacket(void *parsedOut, const u8 *rawBuf)
{
    NpSigEchoParsedPacket *parsed = (NpSigEchoParsedPacket *)parsedOut;

    sceKernelMemset(parsed, 0, 4);
    sceKernelMemcpy(&parsed->pktType, &rawBuf[0], 1);
    if (parsed->pktType >= 1u && parsed->pktType <= 3u) {
        sceKernelMemcpy(parsed->srcNpId.handle, &rawBuf[4], 16);
        sceKernelMemcpy(parsed->srcNpId.opt, &rawBuf[20], 8);
        sceKernelMemcpy(parsed->srcNpId.reserved, &rawBuf[28], 8);
        sceKernelMemcpy(parsed->dstNpId.handle, &rawBuf[36], 16);
        sceKernelMemcpy(parsed->dstNpId.opt, &rawBuf[52], 8);
        sceKernelMemcpy(parsed->dstNpId.reserved, &rawBuf[60], 8);
        sceKernelMemcpy(&parsed->srcTagNet, &rawBuf[68], 2);
        sceKernelMemcpy(&parsed->dstTagNet, &rawBuf[70], 2);
        sceKernelMemcpy(&parsed->stampHiNet, &rawBuf[72], 4);
        sceKernelMemcpy(&parsed->stampLoNet, &rawBuf[76], 4);
    }
    if (parsed->pktType == 2u) {
        sceKernelMemcpy(&parsed->reflectedAddr, &rawBuf[80], 4);
        parsed->reflectedAddr ^= ((u32)parsed->srcTagNet << 16) | (u32)parsed->dstTagNet;
        sceKernelMemcpy(&parsed->reflectedPort, &rawBuf[84], 2);
        parsed->reflectedPort ^= parsed->srcTagNet;
    } else if (parsed->pktType == 3u) {
        sceKernelMemcpy(&parsed->bandwidthNet, &rawBuf[80], 4);
    }
    return 0;
}
