#include "np_matching2_internal.h"

/*
 * Request lifecycle, request deep-copy/free dispatch table, room/member
 * management, external room list conversion, and server teardown (0x017450..0x01CB4F).
 */

typedef s32 (*NpReqCopyFn)(const void *srcParam, void **copyOut);
typedef void (*NpReqFreeFn)(void *paramCopy, NpMatching2Req *failedReq);

typedef struct NpReqParamHandler {
    NpReqCopyFn copyFn;
    NpReqFreeFn freeFn;
} NpReqParamHandler;

static u8 g_reqSeqCounter = 1;

/* Forward declarations for request parameter copy/free table */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetUserInfo(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetUserInfoList(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomDataExt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomDataExtList(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqCreateJoinRoom(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqJoinRoom(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSearchRoom(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSendRoomChat(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSendRoomMsg(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomDataInt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomDataInt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomMemberDataInt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqSetSigOpt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqServerId(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberExtList(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqBlobBuf(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqLeaveRoom(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGrantRoomOwner(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqKickoutMember(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberDataInt(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberIntList(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npCopyReqRoomId(const void *srcParam, void **copyOut);
static s32 NP_MATCHING2_NOINLINE npBuildAttrIdList(const SceNpMatching2AttributeId *srcIds, u32 srcCount,
                                                   s32 addNpId, s32 addOnlineName, s32 addAvatarUrl,
                                                   u32 extraSlotCount, SceNpMatching2AttributeId **outList,
                                                   u32 *outCount);
static s32 NP_MATCHING2_NOINLINE npCopyBinFilterArray(const NpProtoBinFilterWire *srcFilters, u32 filterCount,
                                                      NpProtoBinFilterWire **outFilters);

static void NP_MATCHING2_NOINLINE npFreeReqCreateJoinRoom(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqJoinRoom(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSearchRoom(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqServerId(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSetUserInfo(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetUserInfoList(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberExtList(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomDataExt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomDataExtList(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqBlobBuf(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqLeaveRoom(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGrantRoomOwner(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqKickoutMember(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSendRoomChat(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSendRoomMsg(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomDataInt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomDataInt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomMemberDataInt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberDataInt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberIntList(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqSetSigOpt(void *paramCopy, NpMatching2Req *failedReq);
static void NP_MATCHING2_NOINLINE npFreeReqRoomId(void *paramCopy, NpMatching2Req *failedReq);

static const NpReqParamHandler g_reqParamHandlers[25] = {
    { NULL,                             NULL },
    { npCopyReqServerId,                npFreeReqServerId },
    { NULL,                             NULL },
    { NULL,                             NULL },
    { npCopyReqSetUserInfo,             npFreeReqSetUserInfo },
    { npCopyReqGetUserInfoList,         npFreeReqGetUserInfoList },
    { npCopyReqGetRoomMemberExtList,    npFreeReqGetRoomMemberExtList },
    { npCopyReqSetRoomDataExt,          npFreeReqSetRoomDataExt },
    { npCopyReqGetRoomDataExtList,      npFreeReqGetRoomDataExtList },
    { npCopyReqCreateJoinRoom,          npFreeReqCreateJoinRoom },
    { npCopyReqJoinRoom,                npFreeReqJoinRoom },
    { npCopyReqBlobBuf,                 npFreeReqBlobBuf },
    { npCopyReqLeaveRoom,               npFreeReqLeaveRoom },
    { npCopyReqGrantRoomOwner,          npFreeReqGrantRoomOwner },
    { npCopyReqKickoutMember,           npFreeReqKickoutMember },
    { npCopyReqSearchRoom,              npFreeReqSearchRoom },
    { npCopyReqSendRoomChat,            npFreeReqSendRoomChat },
    { npCopyReqSendRoomMsg,             npFreeReqSendRoomMsg },
    { npCopyReqSetRoomDataInt,          npFreeReqSetRoomDataInt },
    { npCopyReqGetRoomDataInt,          npFreeReqGetRoomDataInt },
    { npCopyReqSetRoomMemberDataInt,    npFreeReqSetRoomMemberDataInt },
    { npCopyReqGetRoomMemberDataInt,    npFreeReqGetRoomMemberDataInt },
    { npCopyReqGetRoomMemberIntList,    npFreeReqGetRoomMemberIntList },
    { npCopyReqSetSigOpt,               npFreeReqSetSigOpt },
    { npCopyReqRoomId,                  npFreeReqRoomId },
};

/* 0x017450 */
s32 NP_MATCHING2_NOINLINE npReqCreate(SceNpMatching2ContextId ctxId, s32 reqSource, u8 reqKind, SceNpMatching2Event eventId,
                                      u16 protoCmd, const void *reqParam, NpMatching2Conn *conn,
                                      const SceNpMatching2RequestOptParam *optParam, s32 isMaintenance,
                                      SceNpMatching2RequestId *assignedReqId, NpMatching2Req **outReq)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Req *req = NULL;
    void *paramCopy = NULL;
    s32 ret = 0;

    if (g_reqParamHandlers[reqKind].copyFn != NULL) {
        ret = g_reqParamHandlers[reqKind].copyFn(reqParam, &paramCopy);
    }

    if (ret >= 0) {
        req = (NpMatching2Req *)npMatching2Alloc(sizeof(NpMatching2Req));
        ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        if (req != NULL) {
            ret = npCtxFindAndRef(ctxId, &ctx);
            if (ret >= 0) {
                u8 seq;
                u16 appReqId;
                u32 baseId;

                sceKernelMemset(req, 0, sizeof(NpMatching2Req));
                req->reqSource = reqSource;
                req->eventId = eventId;
                req->protoCmd = protoCmd;
                req->txId = 0;
                req->reqKind = reqKind;
                if (optParam != NULL) {
                    req->hasOptParam = 1;
                    sceKernelMemcpy(&req->optParam, optParam, sizeof(SceNpMatching2RequestOptParam));
                }
                req->paramCopy = paramCopy;
                req->isMaintenance = isMaintenance;
                paramCopy = NULL;
                req->startTimeLow = sceKernelGetSystemTimeLow();

                if (req->hasOptParam == 1 && req->optParam.timeout != 0) {
                    req->timeoutUsec = req->optParam.timeout;
                } else if (ctx->defaultReqOpt.timeout != 0) {
                    req->timeoutUsec = ctx->defaultReqOpt.timeout;
                } else {
                    req->timeoutUsec = 30000000;
                }

                req->ctxId = ctxId;
                req->connId = conn->connId;
                seq = g_reqSeqCounter;
                g_reqSeqCounter = (u8)(seq + 1);
                if (g_reqSeqCounter == 255) {
                    g_reqSeqCounter = 1;
                }
                baseId = (u32)(((u16)reqKind << 8) | seq);
                appReqId = (req->hasOptParam == 1) ? req->optParam.appReqId : ctx->defaultReqOpt.appReqId;
                req->reqId = baseId | ((u32)appReqId << 16);

                if (assignedReqId != NULL) {
                    *assignedReqId = req->reqId;
                }
                if (outReq != NULL) {
                    *outReq = req;
                }
                npCtxRelease(ctx);
                return 0;
            }
        }
        if (req != NULL) {
            npReqAbortOnSubmitFail(ctx, req, ret);
        }
    }

    if (paramCopy != NULL && g_reqParamHandlers[reqKind].freeFn != NULL) {
        g_reqParamHandlers[reqKind].freeFn(paramCopy, NULL);
    }
    if (ctx != NULL) {
        npCtxRelease(ctx);
    }
    return ret;
}

/* 0x0176C0 */
s32 NP_MATCHING2_NOINLINE npReqRemoveFromQueue(NpMatching2Req *req, NpMatching2ReqQueue *queue,
                                               NpMatching2Conn *conn, NpMatching2ServerNode *server)
{
    NpMatching2Req *cur;
    NpMatching2Req *prev = NULL;
    s32 ret = 0;

    if (req == NULL || queue == NULL) {
        return (s32)0x80550ca0;
    }

    npLockAcquire();
    cur = queue->head;
    while (cur != NULL && cur != req) {
        prev = cur;
        cur = cur->next;
    }
    if (cur != NULL) {
        if (prev != NULL) {
            prev->next = cur->next;
        } else {
            queue->head = cur->next;
        }
        if (cur == queue->tail) {
            queue->tail = prev;
        }
        cur->next = NULL;
    } else {
        ret = (s32)0x80550ca6;
    }
    npLockRelease();

    if (ret >= 0) {
        if (conn != NULL) {
            npConnTryFinishDestroy(conn);
        }
        if (server != NULL) {
            npWorkerCheckContextBusy(server->ctx);
        }
    }
    return ret;
}

/* 0x0177C4 */
s32 NP_MATCHING2_NOINLINE npReqAbortOnSubmitFail(NpMatching2Context *ctx, NpMatching2Req *req, s32 errorCode)
{
    s32 ret = 0;

    if (req != NULL) {
        req->resultCode = errorCode;
        ret = npWorkerOnAuthComplete(ctx, req, errorCode);
        req->completed = 1;
    }
    return ret;
}

/* 0x0177F8 */
s32 NP_MATCHING2_NOINLINE npReqRebindConn(SceNpMatching2ContextId ctxId, NpMatching2Req *req, NpMatching2Conn *conn)
{
    req->ctxId = ctxId;
    req->connId = conn->connId;
    return 0;
}

/* 0x01780C */
s32 NP_MATCHING2_NOINLINE npReqEnqueue(NpMatching2Req *req, NpMatching2Conn *conn, NpMatching2ReqQueue *queue)
{
    NpMatching2ReqQueue *targetQueue = queue;

    if (req == NULL || queue == NULL) {
        return (s32)0x80550ca0;
    }
    if (req->isMaintenance == 1) {
        targetQueue = &conn->server->pendingQueue;
    }

    npLockAcquire();
    if (targetQueue->tail != NULL) {
        targetQueue->tail->next = req;
    } else {
        targetQueue->head = req;
    }
    targetQueue->tail = req;
    req->next = NULL;
    if (req->isMaintenance == 0 && conn != NULL) {
        npConnMarkConnectPending(conn);
    }
    npLockRelease();
    return 0;
}

/* 0x0178CC */
s32 NP_MATCHING2_NOINLINE npReqCountQueue(NpMatching2ReqQueue *queue)
{
    NpMatching2Req *cur;
    s32 count = 0;

    if (queue == NULL) {
        return (s32)0x80550ca0;
    }
    npLockAcquire();
    for (cur = queue->head; cur != NULL; cur = cur->next) {
        count++;
    }
    npLockRelease();
    return count;
}

/* 0x017930 */
s32 NP_MATCHING2_NOINLINE npReqPopHead(NpMatching2ReqQueue *queue, NpMatching2Req **outReq,
                                       NpMatching2Conn *conn, NpMatching2ServerNode *server)
{
    NpMatching2Req *picked = NULL;
    s32 ret = 0;

    if (queue != NULL) {
        npLockAcquire();
        if (queue->head != NULL) {
            NpMatching2Req *head = queue->head;
            npLockAcquire();
            if (npWorkerIsCurrentThread() != 0 || head->completed != 1) {
                picked = head;
                head->refCount++;
            }
            npLockRelease();
        }
    }
    npLockRelease();

    if (picked != NULL) {
        ret = npReqRemoveFromQueue(picked, queue, conn, server);
    }
    *outReq = picked;
    return ret;
}

/* 0x017A1C */
s32 NP_MATCHING2_NOINLINE npReqFindInQueue(NpMatching2ReqQueue *queue, SceNpMatching2RequestId reqId, NpMatching2Req **outReq)
{
    NpMatching2Req *cur;
    s32 ret = SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND;

    npLockAcquire();
    for (cur = queue->head; cur != NULL; cur = cur->next) {
        if (cur->reqId == reqId) {
            *outReq = NULL;
            npLockAcquire();
            if (npWorkerIsCurrentThread() != 0 || cur->completed != 1) {
                *outReq = cur;
                cur->refCount++;
                ret = 0;
            }
            npLockRelease();
            break;
        }
    }
    npLockRelease();
    return ret;
}

/* 0x017AEC */
s32 NP_MATCHING2_NOINLINE npReqFindByTxId(u32 txId, NpMatching2Conn *conn, NpMatching2Req **outReq)
{
    NpMatching2Req *cur;
    s32 ret = SCE_NP_MATCHING2_ERROR_REQUEST_NOT_FOUND;

    npLockAcquire();
    for (cur = conn->waitQueue.head; cur != NULL; cur = cur->next) {
        if (cur->txId == txId) {
            *outReq = NULL;
            npLockAcquire();
            if (npWorkerIsCurrentThread() != 0 || cur->completed != 1) {
                *outReq = cur;
                cur->refCount++;
                ret = 0;
            }
            npLockRelease();
            break;
        }
    }
    npLockRelease();
    return ret;
}

/* 0x017BBC */
NpMatching2Req *NP_MATCHING2_NOINLINE npReqQueueGetFirstRef(NpMatching2ReqQueue *queue)
{
    NpMatching2Req *picked = NULL;

    if (queue != NULL) {
        npLockAcquire();
        if (queue->head != NULL) {
            NpMatching2Req *head = queue->head;
            npLockAcquire();
            if (npWorkerIsCurrentThread() != 0 || head->completed != 1) {
                picked = head;
                head->refCount++;
            }
            npLockRelease();
        }
    }
    npLockRelease();
    return picked;
}

/* 0x017C4C */
NpMatching2Req *NP_MATCHING2_NOINLINE npReqQueueGetNextRef(NpMatching2Req *req)
{
    NpMatching2Req *picked = NULL;

    if (req == NULL) {
        return NULL;
    }

    npLockAcquire();
    if (req->next != NULL) {
        NpMatching2Req *nextReq = req->next;
        npLockAcquire();
        if (npWorkerIsCurrentThread() != 0 || nextReq->completed != 1) {
            picked = nextReq;
            nextReq->refCount++;
        }
        npLockRelease();
    }
    npLockRelease();
    return picked;
}

/* 0x017CE0 */
s32 NP_MATCHING2_NOINLINE npReqRelease(NpMatching2Req *req)
{
    s32 shouldFree = 0;

    if (req == NULL) {
        return 0;
    }

    npLockAcquire();
    req->refCount--;
    if (req->refCount == 0 && req->completed == 1) {
        shouldFree = 1;
    }
    npLockRelease();

    if (shouldFree) {
        NpMatching2Req *failedReq = (req->resultCode < 0) ? req : NULL;
        u8 reqKind = req->reqKind;
        void *paramCopy = req->paramCopy;

        if (failedReq != NULL && failedReq->protoCmd == 0x2001) {
            npConnCloseById(failedReq->ctxId, failedReq->connId);
        }
        if (g_reqParamHandlers[reqKind].freeFn != NULL) {
            g_reqParamHandlers[reqKind].freeFn(paramCopy, failedReq);
        }
        return npMatching2Free(req);
    }
    return 1;
}

/* 0x017DCC */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetUserInfo(const void *srcParam, void **copyOut)
{
    const NpSetUserInfoWrap *srcWrap = (const NpSetUserInfoWrap *)srcParam;
    NpSetUserInfoWrap *dstWrap;
    SceNpMatching2SetUserInfoRequest *dstReq;
    SceNpMatching2BinAttr *attrs = NULL;
    u32 idx;

    dstWrap = (NpSetUserInfoWrap *)npMatching2Alloc(sizeof(NpSetUserInfoWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpSetUserInfoWrap));
        dstReq = (SceNpMatching2SetUserInfoRequest *)npMatching2Alloc(sizeof(SceNpMatching2SetUserInfoRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SetUserInfoRequest));
            dstWrap->req = dstReq;
            dstReq->serverId = srcWrap->req->serverId;
            if (srcWrap->req->userBinAttr == NULL) {
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
            attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcWrap->req->userBinAttrNum);
            if (attrs != NULL) {
                sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcWrap->req->userBinAttrNum);
                for (idx = 0; idx < srcWrap->req->userBinAttrNum; idx++) {
                    attrs[idx].id = srcWrap->req->userBinAttr[idx].id;
                    attrs[idx].ptr = npMatching2Alloc(srcWrap->req->userBinAttr[idx].size);
                    if (attrs[idx].ptr == NULL) {
                        break;
                    }
                    sceKernelMemcpy(attrs[idx].ptr, srcWrap->req->userBinAttr[idx].ptr, srcWrap->req->userBinAttr[idx].size);
                    attrs[idx].size = srcWrap->req->userBinAttr[idx].size;
                }
                if (idx == srcWrap->req->userBinAttrNum) {
                    dstReq->userBinAttr = attrs;
                    dstReq->userBinAttrNum = srcWrap->req->userBinAttrNum;
                    if (copyOut != NULL) {
                        *copyOut = dstWrap;
                    }
                    return 0;
                }
                npWorkerFreeBinAttrArray(attrs, srcWrap->req->userBinAttrNum);
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSetUserInfo(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x017FA4 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetUserInfoList(const void *srcParam, void **copyOut)
{
    const NpGetUserInfoWrap *srcWrap = (const NpGetUserInfoWrap *)srcParam;
    NpGetUserInfoWrap *dstWrap;
    SceNpMatching2GetUserInfoListRequest *dstReq;
    SceNpMatching2AttributeId *attrList = NULL;
    u32 attrCount = 0;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpGetUserInfoWrap *)npMatching2Alloc(sizeof(NpGetUserInfoWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpGetUserInfoWrap));
        dstReq = (SceNpMatching2GetUserInfoListRequest *)npMatching2Alloc(sizeof(SceNpMatching2GetUserInfoListRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2GetUserInfoListRequest));
            dstWrap->req = dstReq;
            dstReq->serverId = srcWrap->req->serverId;
            dstReq->option = srcWrap->req->option;

            if (srcWrap->req->npId != NULL && srcWrap->req->npIdNum != 0) {
                char (*handles)[20] = (char (*)[20])npMatching2Alloc(20 * srcWrap->req->npIdNum);
                if (handles == NULL) {
                    npFreeReqGetUserInfoList(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                {
                    u32 idx;
                    for (idx = 0; idx < srcWrap->req->npIdNum; idx++) {
                        sceKernelMemcpy(handles[idx], &srcWrap->req->npId[idx], 20);
                    }
                }
                dstWrap->npHandles = handles;
                dstReq->npIdNum = srcWrap->req->npIdNum;
            }

            ret = npBuildAttrIdList(srcWrap->req->attrId, srcWrap->req->attrIdNum,
                                    1, (srcWrap->req->option & 1) != 0, (srcWrap->req->option & 2) != 0,
                                    1, &attrList, &attrCount);
            if (ret >= 0) {
                dstReq->attrId = attrList;
                dstReq->attrIdNum = attrCount;
                attrList[attrCount - 1] = 30;
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqGetUserInfoList(dstWrap, NULL);
    }
    return ret;
}

/* 0x018164 */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomDataExt(const void *srcParam, void **copyOut)
{
    const NpSetRoomDataExtWrap *srcWrap = (const NpSetRoomDataExtWrap *)srcParam;
    NpSetRoomDataExtWrap *dstWrap;
    SceNpMatching2SetRoomDataExternalRequest *dstReq;
    SceNpMatching2BinAttr *mergedBins = NULL;
    u32 totalBins = 0;
    u32 idx;

    dstWrap = (NpSetRoomDataExtWrap *)npMatching2Alloc(sizeof(NpSetRoomDataExtWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpSetRoomDataExtWrap));
        if (srcWrap->req == NULL) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        dstReq = (SceNpMatching2SetRoomDataExternalRequest *)npMatching2Alloc(sizeof(SceNpMatching2SetRoomDataExternalRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SetRoomDataExternalRequest));
            dstWrap->req = dstReq;
            dstReq->roomId = srcWrap->req->roomId;

            if (srcWrap->req->roomSearchableIntAttrExternal != NULL) {
                u32 intBytes = sizeof(SceNpMatching2IntAttr) * srcWrap->req->roomSearchableIntAttrExternalNum;
                SceNpMatching2IntAttr *intCopy = (SceNpMatching2IntAttr *)npMatching2Alloc(intBytes);
                if (intCopy == NULL) {
                    npFreeReqSetRoomDataExt(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(intCopy, srcWrap->req->roomSearchableIntAttrExternal, intBytes);
                dstReq->roomSearchableIntAttrExternal = intCopy;
                dstReq->roomSearchableIntAttrExternalNum = srcWrap->req->roomSearchableIntAttrExternalNum;
            }

            if (srcWrap->req->roomSearchableBinAttrExternal == NULL &&
                srcWrap->req->roomBinAttrExternal == NULL) {
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }

            totalBins = srcWrap->req->roomSearchableBinAttrExternalNum + srcWrap->req->roomBinAttrExternalNum;
            mergedBins = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * totalBins);
            if (mergedBins != NULL) {
                s32 allocOk = 1;
                sceKernelMemset(mergedBins, 0, sizeof(SceNpMatching2BinAttr) * totalBins);
                for (idx = 0; idx < srcWrap->req->roomSearchableBinAttrExternalNum; idx++) {
                    const SceNpMatching2BinAttr *srcAttr = &srcWrap->req->roomSearchableBinAttrExternal[idx];
                    mergedBins[idx].id = srcAttr->id;
                    mergedBins[idx].ptr = npMatching2Alloc(srcAttr->size);
                    if (mergedBins[idx].ptr == NULL) {
                        allocOk = 0;
                        break;
                    }
                    sceKernelMemcpy(mergedBins[idx].ptr, srcAttr->ptr, srcAttr->size);
                    mergedBins[idx].size = srcAttr->size;
                }
                if (allocOk) {
                    u32 baseIdx = srcWrap->req->roomSearchableBinAttrExternalNum;
                    for (idx = 0; idx < srcWrap->req->roomBinAttrExternalNum; idx++) {
                        const SceNpMatching2BinAttr *srcAttr = &srcWrap->req->roomBinAttrExternal[idx];
                        mergedBins[baseIdx + idx].id = srcAttr->id;
                        mergedBins[baseIdx + idx].ptr = npMatching2Alloc(srcAttr->size);
                        if (mergedBins[baseIdx + idx].ptr == NULL) {
                            allocOk = 0;
                            break;
                        }
                        sceKernelMemcpy(mergedBins[baseIdx + idx].ptr, srcAttr->ptr, srcAttr->size);
                        mergedBins[baseIdx + idx].size = srcAttr->size;
                    }
                }
                if (allocOk) {
                    dstReq->roomBinAttrExternal = mergedBins;
                    dstReq->roomBinAttrExternalNum = totalBins;
                    if (copyOut != NULL) {
                        *copyOut = dstWrap;
                    }
                    return 0;
                }
                npWorkerFreeBinAttrArray(mergedBins, totalBins);
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSetRoomDataExt(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x018448 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomDataExtList(const void *srcParam, void **copyOut)
{
    const NpCtxReqPairWrap *srcWrap = (const NpCtxReqPairWrap *)srcParam;
    const SceNpMatching2GetRoomDataExternalListRequest *srcReq = (const SceNpMatching2GetRoomDataExternalListRequest *)srcWrap->req;
    NpCtxReqPairWrap *dstWrap;
    SceNpMatching2GetRoomDataExternalListRequest *dstReq;
    SceNpMatching2AttributeId *attrList = NULL;
    u32 attrCount = 0;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpCtxReqPairWrap *)npMatching2Alloc(sizeof(NpCtxReqPairWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpCtxReqPairWrap));
        if (srcReq == NULL) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        dstReq = (SceNpMatching2GetRoomDataExternalListRequest *)npMatching2Alloc(sizeof(SceNpMatching2GetRoomDataExternalListRequest));
        if (dstReq != NULL) {
            SceNpMatching2RoomId *roomIds;
            u32 roomIdBytes = sizeof(SceNpMatching2RoomId) * srcReq->roomIdNum;

            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2GetRoomDataExternalListRequest));
            dstWrap->req = dstReq;
            roomIds = (SceNpMatching2RoomId *)npMatching2Alloc(roomIdBytes);
            if (roomIds != NULL) {
                sceKernelMemcpy(roomIds, srcReq->roomId, roomIdBytes);
                dstReq->roomId = roomIds;
                dstReq->roomIdNum = srcReq->roomIdNum;
                ret = npBuildAttrIdList(srcReq->attrId, srcReq->attrIdNum,
                                        1, (srcWrap->ctx->option & 1) != 0, (srcWrap->ctx->option & 2) != 0,
                                        1, &attrList, &attrCount);
                if (ret >= 0) {
                    dstReq->attrId = attrList;
                    dstReq->attrIdNum = attrCount;
                    attrList[attrCount - 1] = 65;
                    if (copyOut != NULL) {
                        *copyOut = dstWrap;
                    }
                    return 0;
                }
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqGetRoomDataExtList(dstWrap, NULL);
    }
    return ret;
}

/* 0x0185BC */
static s32 NP_MATCHING2_NOINLINE npCopyReqCreateJoinRoom(const void *srcParam, void **copyOut)
{
    const NpCreateJoinWrap *srcWrap = (const NpCreateJoinWrap *)srcParam;
    const SceNpMatching2CreateJoinRoomRequest *srcReq = srcWrap->req;
    NpCreateJoinWrap *dstWrap;
    SceNpMatching2CreateJoinRoomRequest *dstReq;
    u32 idx;

    dstWrap = (NpCreateJoinWrap *)npMatching2Alloc(sizeof(NpCreateJoinWrap));
    if (dstWrap == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstWrap, 0, sizeof(NpCreateJoinWrap));

    dstReq = (SceNpMatching2CreateJoinRoomRequest *)npMatching2Alloc(sizeof(SceNpMatching2CreateJoinRoomRequest));
    if (dstReq == NULL) {
        npFreeReqCreateJoinRoom(dstWrap, NULL);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2CreateJoinRoomRequest));
    dstWrap->req = dstReq;

    dstReq->worldId = srcReq->worldId;
    dstReq->maxSlot = srcReq->maxSlot;
    dstReq->flagAttr = srcReq->flagAttr;
    dstReq->teamId = srcReq->teamId;

    if (srcReq->roomBinAttrInternal != NULL) {
        SceNpMatching2BinAttr *attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcReq->roomBinAttrInternalNum);
        if (attrs == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcReq->roomBinAttrInternalNum);
        for (idx = 0; idx < srcReq->roomBinAttrInternalNum; idx++) {
            attrs[idx].id = srcReq->roomBinAttrInternal[idx].id;
            attrs[idx].ptr = npMatching2Alloc(srcReq->roomBinAttrInternal[idx].size);
            if (attrs[idx].ptr == NULL) {
                npWorkerFreeBinAttrArray(attrs, srcReq->roomBinAttrInternalNum);
                npFreeReqCreateJoinRoom(dstWrap, NULL);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(attrs[idx].ptr, srcReq->roomBinAttrInternal[idx].ptr, srcReq->roomBinAttrInternal[idx].size);
            attrs[idx].size = srcReq->roomBinAttrInternal[idx].size;
        }
        dstReq->roomBinAttrInternal = attrs;
        dstReq->roomBinAttrInternalNum = srcReq->roomBinAttrInternalNum;
    }

    if (srcReq->roomPassword != NULL) {
        SceNpMatching2SessionPassword *pwCopy = (SceNpMatching2SessionPassword *)npMatching2Alloc(8);
        if (pwCopy == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(pwCopy, srcReq->roomPassword, 8);
        dstReq->roomPassword = pwCopy;
    }

    if (srcReq->groupConfig != NULL) {
        u32 bytes = 20 * srcReq->groupConfigNum;
        void *cfgCopy = npMatching2Alloc(bytes);
        if (cfgCopy == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(cfgCopy, srcReq->groupConfig, bytes);
        dstReq->groupConfig = cfgCopy;
        dstReq->groupConfigNum = srcReq->groupConfigNum;
    }

    if (srcReq->joinRoomGroupLabel != NULL) {
        void *labelCopy = npMatching2Alloc(8);
        if (labelCopy == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(labelCopy, srcReq->joinRoomGroupLabel, 8);
        dstReq->joinRoomGroupLabel = labelCopy;
    }

    if (srcReq->passwordSlotMask != NULL) {
        u64 *maskCopy = (u64 *)npMatching2Alloc(8);
        if (maskCopy == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(maskCopy, srcReq->passwordSlotMask, 8);
        dstReq->passwordSlotMask = maskCopy;
    }

    if (srcReq->allowedUser != NULL) {
        char (*handles)[20] = (char (*)[20])npMatching2Alloc(20 * srcReq->allowedUserNum);
        if (handles == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        for (idx = 0; idx < srcReq->allowedUserNum; idx++) {
            sceKernelMemcpy(handles[idx], &srcReq->allowedUser[idx], 20);
        }
        dstWrap->allowedHandles = handles;
        dstReq->allowedUserNum = srcReq->allowedUserNum;
    }

    if (srcReq->blockedUser != NULL) {
        char (*handles)[20] = (char (*)[20])npMatching2Alloc(20 * srcReq->blockedUserNum);
        if (handles == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        for (idx = 0; idx < srcReq->blockedUserNum; idx++) {
            sceKernelMemcpy(handles[idx], &srcReq->blockedUser[idx], 20);
        }
        dstWrap->blockedHandles = handles;
        dstReq->blockedUserNum = srcReq->blockedUserNum;
    }

    if (srcReq->roomMemberBinAttrInternal != NULL) {
        SceNpMatching2BinAttr *attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
        if (attrs == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
        for (idx = 0; idx < srcReq->roomMemberBinAttrInternalNum; idx++) {
            attrs[idx].id = srcReq->roomMemberBinAttrInternal[idx].id;
            attrs[idx].ptr = npMatching2Alloc(srcReq->roomMemberBinAttrInternal[idx].size);
            if (attrs[idx].ptr == NULL) {
                npWorkerFreeBinAttrArray(attrs, srcReq->roomMemberBinAttrInternalNum);
                npFreeReqCreateJoinRoom(dstWrap, NULL);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(attrs[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].size);
            attrs[idx].size = srcReq->roomMemberBinAttrInternal[idx].size;
        }
        dstReq->roomMemberBinAttrInternal = attrs;
        dstReq->roomMemberBinAttrInternalNum = srcReq->roomMemberBinAttrInternalNum;
    }

    if (srcReq->roomSearchableIntAttrExternal != NULL) {
        u32 intBytes = sizeof(SceNpMatching2IntAttr) * srcReq->roomSearchableIntAttrExternalNum;
        SceNpMatching2IntAttr *intCopy = (SceNpMatching2IntAttr *)npMatching2Alloc(intBytes);
        if (intCopy == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(intCopy, srcReq->roomSearchableIntAttrExternal, intBytes);
        dstReq->roomSearchableIntAttrExternal = intCopy;
        dstReq->roomSearchableIntAttrExternalNum = srcReq->roomSearchableIntAttrExternalNum;
    }

    if (srcReq->roomSearchableBinAttrExternal != NULL || srcReq->roomBinAttrExternal != NULL) {
        u32 totalBins = srcReq->roomSearchableBinAttrExternalNum + srcReq->roomBinAttrExternalNum;
        SceNpMatching2BinAttr *mergedBins = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * totalBins);
        if (mergedBins == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(mergedBins, 0, sizeof(SceNpMatching2BinAttr) * totalBins);
        for (idx = 0; idx < srcReq->roomSearchableBinAttrExternalNum; idx++) {
            const SceNpMatching2BinAttr *srcAttr = &srcReq->roomSearchableBinAttrExternal[idx];
            mergedBins[idx].id = srcAttr->id;
            mergedBins[idx].ptr = npMatching2Alloc(srcAttr->size);
            if (mergedBins[idx].ptr == NULL) {
                npWorkerFreeBinAttrArray(mergedBins, totalBins);
                npFreeReqCreateJoinRoom(dstWrap, NULL);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(mergedBins[idx].ptr, srcAttr->ptr, srcAttr->size);
            mergedBins[idx].size = srcAttr->size;
        }
        {
            u32 baseIdx = srcReq->roomSearchableBinAttrExternalNum;
            for (idx = 0; idx < srcReq->roomBinAttrExternalNum; idx++) {
                const SceNpMatching2BinAttr *srcAttr = &srcReq->roomBinAttrExternal[idx];
                mergedBins[baseIdx + idx].id = srcAttr->id;
                mergedBins[baseIdx + idx].ptr = npMatching2Alloc(srcAttr->size);
                if (mergedBins[baseIdx + idx].ptr == NULL) {
                    npWorkerFreeBinAttrArray(mergedBins, totalBins);
                    npFreeReqCreateJoinRoom(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(mergedBins[baseIdx + idx].ptr, srcAttr->ptr, srcAttr->size);
                mergedBins[baseIdx + idx].size = srcAttr->size;
            }
        }
        dstReq->roomBinAttrExternal = mergedBins;
        dstReq->roomBinAttrExternalNum = totalBins;
    }

    if (srcWrap->roomEvOpt != NULL) {
        dstWrap->roomEvOpt = (SceNpMatching2RoomEventOptParam *)npMatching2Alloc(sizeof(SceNpMatching2RoomEventOptParam));
        if (dstWrap->roomEvOpt == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(dstWrap->roomEvOpt, srcWrap->roomEvOpt, sizeof(SceNpMatching2RoomEventOptParam));
    }

    if (srcWrap->roomMsgOpt != NULL) {
        dstWrap->roomMsgOpt = (SceNpMatching2RoomMessageOptParam *)npMatching2Alloc(sizeof(SceNpMatching2RoomMessageOptParam));
        if (dstWrap->roomMsgOpt == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(dstWrap->roomMsgOpt, srcWrap->roomMsgOpt, sizeof(SceNpMatching2RoomMessageOptParam));
    }

    if (srcReq->sigOptParam != NULL) {
        dstWrap->sigOptBlob = (NpMatching2ShortBlob *)npMatching2Alloc(sizeof(NpMatching2ShortBlob));
        if (dstWrap->sigOptBlob == NULL) {
            npFreeReqCreateJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(dstWrap->sigOptBlob, 0, sizeof(NpMatching2ShortBlob));
        sceKernelMemcpy(dstWrap->sigOptBlob->bytes, srcReq->sigOptParam, 8);
        dstWrap->sigOptBlob->len = 8;
    }

    if (copyOut != NULL) {
        *copyOut = dstWrap;
    }
    return 0;
}

/* 0x018D78 */
static s32 NP_MATCHING2_NOINLINE npCopyReqJoinRoom(const void *srcParam, void **copyOut)
{
    const NpJoinRoomWrap *srcWrap = (const NpJoinRoomWrap *)srcParam;
    const SceNpMatching2JoinRoomRequest *srcReq = srcWrap->req;
    NpJoinRoomWrap *dstWrap;
    SceNpMatching2JoinRoomRequest *dstReq;
    u32 idx;

    dstWrap = (NpJoinRoomWrap *)npMatching2Alloc(sizeof(NpJoinRoomWrap));
    if (dstWrap == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstWrap, 0, sizeof(NpJoinRoomWrap));

    dstReq = (SceNpMatching2JoinRoomRequest *)npMatching2Alloc(sizeof(SceNpMatching2JoinRoomRequest));
    if (dstReq == NULL) {
        npFreeReqJoinRoom(dstWrap, NULL);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2JoinRoomRequest));
    dstWrap->req = dstReq;

    dstReq->roomId = srcReq->roomId;
    dstReq->teamId = srcReq->teamId;
    sceKernelMemcpy(dstReq->optData, srcReq->optData, 20);

    if (srcReq->roomMemberBinAttrInternal != NULL) {
        SceNpMatching2BinAttr *attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
        if (attrs == NULL) {
            npFreeReqJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
        for (idx = 0; idx < srcReq->roomMemberBinAttrInternalNum; idx++) {
            attrs[idx].id = srcReq->roomMemberBinAttrInternal[idx].id;
            attrs[idx].ptr = npMatching2Alloc(srcReq->roomMemberBinAttrInternal[idx].size);
            if (attrs[idx].ptr == NULL) {
                npWorkerFreeBinAttrArray(attrs, srcReq->roomMemberBinAttrInternalNum);
                npFreeReqJoinRoom(dstWrap, NULL);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(attrs[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].size);
            attrs[idx].size = srcReq->roomMemberBinAttrInternal[idx].size;
        }
        dstReq->roomMemberBinAttrInternal = attrs;
        dstReq->roomMemberBinAttrInternalNum = srcReq->roomMemberBinAttrInternalNum;
    }

    if (srcReq->roomPassword != NULL) {
        SceNpMatching2SessionPassword *pwCopy = (SceNpMatching2SessionPassword *)npMatching2Alloc(8);
        if (pwCopy == NULL) {
            npFreeReqJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(pwCopy, srcReq->roomPassword, 8);
        dstReq->roomPassword = pwCopy;
    }

    if (srcReq->joinRoomGroupLabel != NULL) {
        void *labelCopy = npMatching2Alloc(8);
        if (labelCopy == NULL) {
            npFreeReqJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(labelCopy, srcReq->joinRoomGroupLabel, 8);
        dstReq->joinRoomGroupLabel = labelCopy;
    }

    if (srcWrap->roomEvOpt != NULL) {
        dstWrap->roomEvOpt = (SceNpMatching2RoomEventOptParam *)npMatching2Alloc(sizeof(SceNpMatching2RoomEventOptParam));
        if (dstWrap->roomEvOpt == NULL) {
            npFreeReqJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(dstWrap->roomEvOpt, srcWrap->roomEvOpt, sizeof(SceNpMatching2RoomEventOptParam));
    }

    if (srcWrap->roomMsgOpt != NULL) {
        dstWrap->roomMsgOpt = (SceNpMatching2RoomMessageOptParam *)npMatching2Alloc(sizeof(SceNpMatching2RoomMessageOptParam));
        if (dstWrap->roomMsgOpt == NULL) {
            npFreeReqJoinRoom(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(dstWrap->roomMsgOpt, srcWrap->roomMsgOpt, sizeof(SceNpMatching2RoomMessageOptParam));
    }

    if (copyOut != NULL) {
        *copyOut = dstWrap;
    }
    return 0;
}

/* 0x019070 */
static s32 NP_MATCHING2_NOINLINE npCopyReqSearchRoom(const void *srcParam, void **copyOut)
{
    const NpCtxReqPairWrap *srcWrap = (const NpCtxReqPairWrap *)srcParam;
    const SceNpMatching2SearchRoomRequest *srcReq = (const SceNpMatching2SearchRoomRequest *)srcWrap->req;
    NpCtxReqPairWrap *dstWrap;
    SceNpMatching2SearchRoomRequest *dstReq;
    SceNpMatching2AttributeId *attrList = NULL;
    u32 attrCount = 0;
    s32 addNpId = 0;
    s32 addOnlineName = 0;
    s32 addAvatarUrl = 0;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpCtxReqPairWrap *)npMatching2Alloc(sizeof(NpCtxReqPairWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpCtxReqPairWrap));
        dstReq = (SceNpMatching2SearchRoomRequest *)npMatching2Alloc(sizeof(SceNpMatching2SearchRoomRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SearchRoomRequest));
            dstWrap->req = dstReq;
            dstReq->worldId = srcReq->worldId;
            dstReq->startIndex = srcReq->startIndex;
            dstReq->maxNum = srcReq->maxNum;
            dstReq->flagFilter = srcReq->flagFilter;
            dstReq->flagAttr = srcReq->flagAttr;
            dstReq->option = srcReq->option;

            if (srcReq->intFilter != NULL && srcReq->intFilterNum != 0) {
                u32 bytes = sizeof(NpProtoIntFilterWire) * srcReq->intFilterNum;
                void *intCopy = npMatching2Alloc(bytes);
                if (intCopy == NULL) {
                    npFreeReqSearchRoom(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(intCopy, srcReq->intFilter, bytes);
                dstReq->intFilter = intCopy;
                dstReq->intFilterNum = srcReq->intFilterNum;
            }

            if (srcReq->binFilter != NULL && srcReq->binFilterNum != 0) {
                NpProtoBinFilterWire *binCopy = NULL;
                ret = npCopyBinFilterArray((const NpProtoBinFilterWire *)srcReq->binFilter, srcReq->binFilterNum, &binCopy);
                if (ret < 0) {
                    npFreeReqSearchRoom(dstWrap, NULL);
                    return ret;
                }
                dstReq->binFilter = binCopy;
                dstReq->binFilterNum = srcReq->binFilterNum;
            }

            if ((dstReq->option & 7) != 0) {
                addNpId = 1;
                if ((dstReq->option & 2) != 0) {
                    addOnlineName = 1;
                }
                if ((dstReq->option & 4) != 0) {
                    addAvatarUrl = 1;
                }
            }

            ret = npBuildAttrIdList(srcReq->attrId, srcReq->attrIdNum,
                                    addNpId, addOnlineName, addAvatarUrl,
                                    1, &attrList, &attrCount);
            if (ret >= 0) {
                dstReq->attrId = attrList;
                dstReq->attrIdNum = attrCount;
                attrList[attrCount - 1] = 65;
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSearchRoom(dstWrap, NULL);
    }
    return ret;
}

/* 0x0192AC */
static s32 NP_MATCHING2_NOINLINE npCopyReqSendRoomChat(const void *srcParam, void **copyOut)
{
    const NpSendRoomChatWrap *srcWrap = (const NpSendRoomChatWrap *)srcParam;
    NpSendRoomChatWrap *dstWrap;
    SceNpMatching2SendRoomChatMessageRequest *dstReq;
    void *msgCopy;

    dstWrap = (NpSendRoomChatWrap *)npMatching2Alloc(sizeof(NpSendRoomChatWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpSendRoomChatWrap));
        dstReq = (SceNpMatching2SendRoomChatMessageRequest *)npMatching2Alloc(sizeof(SceNpMatching2SendRoomChatMessageRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SendRoomChatMessageRequest));
            dstWrap->req = dstReq;
            dstReq->castType = srcWrap->req->castType;
            sceKernelMemcpy(&dstReq->target, &srcWrap->req->target, sizeof(dstReq->target));
            dstReq->msgLen = srcWrap->req->msgLen;
            dstReq->option = srcWrap->req->option;

            if (dstReq->castType == 3) {
                const SceNpMatching2RoomMemberId *srcIds;
                u32 bytes;
                SceNpMatching2RoomMemberId *idsCopy;
                dstReq->target.multicastTarget.memberId = NULL;
                srcIds = srcWrap->req->target.multicastTarget.memberId;
                bytes = sizeof(SceNpMatching2RoomMemberId) * srcWrap->req->target.multicastTarget.memberIdNum;
                idsCopy = (SceNpMatching2RoomMemberId *)npMatching2Alloc(bytes);
                if (idsCopy == NULL) {
                    npFreeReqSendRoomChat(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(idsCopy, srcIds, bytes);
                dstReq->target.multicastTarget.memberId = idsCopy;
            }

            msgCopy = npMatching2Alloc(dstReq->msgLen);
            if (msgCopy != NULL) {
                sceKernelMemcpy(msgCopy, srcWrap->req->msg, dstReq->msgLen);
                dstReq->msg = msgCopy;
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSendRoomChat(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x019434 */
static s32 NP_MATCHING2_NOINLINE npCopyReqSendRoomMsg(const void *srcParam, void **copyOut)
{
    const NpSendRoomMsgWrap *srcWrap = (const NpSendRoomMsgWrap *)srcParam;
    NpSendRoomMsgWrap *dstWrap;
    SceNpMatching2SendRoomMessageRequest *dstReq;
    void *msgCopy;

    dstWrap = (NpSendRoomMsgWrap *)npMatching2Alloc(sizeof(NpSendRoomMsgWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpSendRoomMsgWrap));
        dstReq = (SceNpMatching2SendRoomMessageRequest *)npMatching2Alloc(sizeof(SceNpMatching2SendRoomMessageRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SendRoomMessageRequest));
            dstWrap->req = dstReq;
            dstReq->castType = srcWrap->req->castType;
            sceKernelMemcpy(&dstReq->target, &srcWrap->req->target, sizeof(dstReq->target));
            dstReq->msgLen = srcWrap->req->msgLen;
            dstReq->option = srcWrap->req->option;

            if (dstReq->castType == 3) {
                const SceNpMatching2RoomMemberId *srcIds;
                u32 bytes;
                SceNpMatching2RoomMemberId *idsCopy;
                dstReq->target.multicastTarget.memberId = NULL;
                srcIds = srcWrap->req->target.multicastTarget.memberId;
                bytes = sizeof(SceNpMatching2RoomMemberId) * srcWrap->req->target.multicastTarget.memberIdNum;
                idsCopy = (SceNpMatching2RoomMemberId *)npMatching2Alloc(bytes);
                if (idsCopy == NULL) {
                    npFreeReqSendRoomMsg(dstWrap, NULL);
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(idsCopy, srcIds, bytes);
                dstReq->target.multicastTarget.memberId = idsCopy;
            }

            msgCopy = npMatching2Alloc(dstReq->msgLen);
            if (msgCopy != NULL) {
                sceKernelMemcpy(msgCopy, srcWrap->req->msg, dstReq->msgLen);
                dstReq->msg = msgCopy;
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSendRoomMsg(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x0195BC */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomDataInt(const void *srcParam, void **copyOut)
{
    const NpSetRoomDataInternalWrap *srcWrap = (const NpSetRoomDataInternalWrap *)srcParam;
    const SceNpMatching2SetRoomDataInternalRequest *srcReq = srcWrap->req;
    NpSetRoomDataInternalWrap *dstWrap;
    SceNpMatching2SetRoomDataInternalRequest *dstReq;
    u32 idx;

    dstWrap = (NpSetRoomDataInternalWrap *)npMatching2Alloc(sizeof(NpSetRoomDataInternalWrap));
    if (dstWrap == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstWrap, 0, sizeof(NpSetRoomDataInternalWrap));

    dstReq = (SceNpMatching2SetRoomDataInternalRequest *)npMatching2Alloc(sizeof(SceNpMatching2SetRoomDataInternalRequest));
    if (dstReq == NULL) {
        npFreeReqSetRoomDataInt(dstWrap, NULL);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SetRoomDataInternalRequest));
    dstWrap->req = dstReq;

    dstReq->roomId = srcReq->roomId;
    dstReq->flagFilter = srcReq->flagFilter;
    dstReq->flagAttr = srcReq->flagAttr;

    if (srcReq->roomBinAttrInternal != NULL) {
        SceNpMatching2BinAttr *attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcReq->roomBinAttrInternalNum);
        if (attrs == NULL) {
            npFreeReqSetRoomDataInt(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcReq->roomBinAttrInternalNum);
        for (idx = 0; idx < srcReq->roomBinAttrInternalNum; idx++) {
            attrs[idx].id = srcReq->roomBinAttrInternal[idx].id;
            attrs[idx].ptr = npMatching2Alloc(srcReq->roomBinAttrInternal[idx].size);
            if (attrs[idx].ptr == NULL) {
                npWorkerFreeBinAttrArray(attrs, srcReq->roomBinAttrInternalNum);
                npFreeReqSetRoomDataInt(dstWrap, NULL);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(attrs[idx].ptr, srcReq->roomBinAttrInternal[idx].ptr, srcReq->roomBinAttrInternal[idx].size);
            attrs[idx].size = srcReq->roomBinAttrInternal[idx].size;
        }
        dstReq->roomBinAttrInternal = attrs;
        dstReq->roomBinAttrInternalNum = srcReq->roomBinAttrInternalNum;
    }

    if (srcReq->passwordConfig != NULL) {
        u32 bytes = 8 * srcReq->passwordConfigNum;
        void *cfgCopy = npMatching2Alloc(bytes);
        if (cfgCopy == NULL) {
            npFreeReqSetRoomDataInt(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(cfgCopy, srcReq->passwordConfig, bytes);
        dstReq->passwordConfig = cfgCopy;
        dstReq->passwordConfigNum = srcReq->passwordConfigNum;
    }

    if (srcReq->passwordSlotMask != NULL) {
        u64 *maskCopy = (u64 *)npMatching2Alloc(8);
        if (maskCopy == NULL) {
            npFreeReqSetRoomDataInt(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemcpy(maskCopy, srcReq->passwordSlotMask, 8);
        dstReq->passwordSlotMask = maskCopy;
    }

    if (srcReq->ownerPrivilegeRank != NULL) {
        u32 bytes = sizeof(u16) * srcReq->ownerPrivilegeRankNum;
        u16 *rankCopy = (u16 *)npMatching2Alloc(bytes);
        if (rankCopy == NULL) {
            npFreeReqSetRoomDataInt(dstWrap, NULL);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(rankCopy, 0, bytes);
        sceKernelMemcpy(rankCopy, srcReq->ownerPrivilegeRank, bytes);
        dstReq->ownerPrivilegeRank = rankCopy;
        dstReq->ownerPrivilegeRankNum = srcReq->ownerPrivilegeRankNum;
    }

    if (copyOut != NULL) {
        *copyOut = dstWrap;
    }
    return 0;
}

/* 0x0198A8 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomDataInt(const void *srcParam, void **copyOut)
{
    const NpGetRoomAttrWrap *srcWrap = (const NpGetRoomAttrWrap *)srcParam;
    NpGetRoomAttrWrap *dstWrap;

    dstWrap = (NpGetRoomAttrWrap *)npMatching2Alloc(sizeof(NpGetRoomAttrWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpGetRoomAttrWrap));
        const SceNpMatching2AttributeId *srcAttrIds = srcWrap->attrId;
        dstWrap->memberId = srcWrap->memberId;
        if (srcAttrIds == NULL || srcWrap->attrIdNum == 0) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        {
            u32 bytes = sizeof(SceNpMatching2AttributeId) * srcWrap->attrIdNum;
            SceNpMatching2AttributeId *idsCopy = (SceNpMatching2AttributeId *)npMatching2Alloc(bytes);
            if (idsCopy != NULL) {
                sceKernelMemset(idsCopy, 0, bytes);
                sceKernelMemcpy(idsCopy, srcAttrIds, bytes);
                dstWrap->attrId = idsCopy;
                dstWrap->attrIdNum = srcWrap->attrIdNum;
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqGetRoomDataInt(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x0199B0 */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetRoomMemberDataInt(const void *srcParam, void **copyOut)
{
    const NpSetRoomMemberDataInternalWrap *srcWrap = (const NpSetRoomMemberDataInternalWrap *)srcParam;
    const SceNpMatching2SetRoomMemberDataInternalRequest *srcReq = srcWrap->req;
    NpSetRoomMemberDataInternalWrap *dstWrap;
    SceNpMatching2SetRoomMemberDataInternalRequest *dstReq;
    u32 idx;

    dstWrap = (NpSetRoomMemberDataInternalWrap *)npMatching2Alloc(sizeof(NpSetRoomMemberDataInternalWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpSetRoomMemberDataInternalWrap));
        dstReq = (SceNpMatching2SetRoomMemberDataInternalRequest *)npMatching2Alloc(sizeof(SceNpMatching2SetRoomMemberDataInternalRequest));
        if (dstReq != NULL) {
            sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SetRoomMemberDataInternalRequest));
            dstWrap->req = dstReq;
            dstReq->memberId = srcReq->memberId;
            dstReq->teamId = srcReq->teamId;
            dstReq->flagFilter = srcReq->flagFilter;
            dstReq->flagAttr = srcReq->flagAttr;
            if (srcReq->roomMemberBinAttrInternal == NULL) {
                if (copyOut != NULL) {
                    *copyOut = dstWrap;
                }
                return 0;
            }
            {
                SceNpMatching2BinAttr *attrs = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
                if (attrs != NULL) {
                    sceKernelMemset(attrs, 0, sizeof(SceNpMatching2BinAttr) * srcReq->roomMemberBinAttrInternalNum);
                    for (idx = 0; idx < srcReq->roomMemberBinAttrInternalNum; idx++) {
                        attrs[idx].id = srcReq->roomMemberBinAttrInternal[idx].id;
                        attrs[idx].ptr = npMatching2Alloc(srcReq->roomMemberBinAttrInternal[idx].size);
                        if (attrs[idx].ptr == NULL) {
                            break;
                        }
                        sceKernelMemcpy(attrs[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].ptr, srcReq->roomMemberBinAttrInternal[idx].size);
                        attrs[idx].size = srcReq->roomMemberBinAttrInternal[idx].size;
                    }
                    if (idx == srcReq->roomMemberBinAttrInternalNum) {
                        dstReq->roomMemberBinAttrInternal = attrs;
                        dstReq->roomMemberBinAttrInternalNum = srcReq->roomMemberBinAttrInternalNum;
                        if (copyOut != NULL) {
                            *copyOut = dstWrap;
                        }
                        return 0;
                    }
                    npWorkerFreeBinAttrArray(attrs, srcReq->roomMemberBinAttrInternalNum);
                }
            }
        }
    }

    if (dstWrap != NULL) {
        npFreeReqSetRoomMemberDataInt(dstWrap, NULL);
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x019BA4 */
static s32 NP_MATCHING2_NOINLINE npCopyReqSetSigOpt(const void *srcParam, void **copyOut)
{
    const NpSetSigOptWrap *srcWrap = (const NpSetSigOptWrap *)srcParam;
    NpSetSigOptWrap *dstWrap;
    SceNpMatching2SetSignalingOptParamRequest *dstReq;

    if (srcWrap == NULL || copyOut == NULL || srcWrap->req == NULL) {
        return (s32)0x80550ca0;
    }

    dstWrap = (NpSetSigOptWrap *)npMatching2Alloc(sizeof(NpSetSigOptWrap));
    if (dstWrap == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstWrap, 0, sizeof(NpSetSigOptWrap));

    dstReq = (SceNpMatching2SetSignalingOptParamRequest *)npMatching2Alloc(sizeof(SceNpMatching2SetSignalingOptParamRequest));
    if (dstReq == NULL) {
        npFreeReqSetSigOpt(dstWrap, NULL);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstReq, 0, sizeof(SceNpMatching2SetSignalingOptParamRequest));
    dstWrap->req = dstReq;
    dstWrap->sigOptLen = 8;
    dstReq->roomId = srcWrap->req->roomId;
    sceKernelMemcpy(&dstWrap->sigOptParam, &srcWrap->req->sigOptParam, sizeof(SceNpMatching2SignalingOptParam));
    *copyOut = dstWrap;
    return 0;
}

/* 0x019CAC */
static s32 NP_MATCHING2_NOINLINE npCopyReqServerId(const void *srcParam, void **copyOut)
{
    void *copy = npMatching2Alloc(2);
    if (copy == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(copy, 0, 2);
    sceKernelMemcpy(copy, srcParam, 2);
    if (copyOut != NULL) {
        *copyOut = copy;
    }
    return 0;
}

/* 0x019D28 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberExtList(const void *srcParam, void **copyOut)
{
    const NpGetRoomMemberExtWrap *srcWrap = (const NpGetRoomMemberExtWrap *)srcParam;
    NpGetRoomMemberExtWrap *dstWrap;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpGetRoomMemberExtWrap *)npMatching2Alloc(sizeof(NpGetRoomMemberExtWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpGetRoomMemberExtWrap));
        dstWrap->roomId = srcWrap->roomId;
        ret = npBuildAttrIdList(NULL, 0, 0,
                                (srcWrap->ctx->option & 1) != 0,
                                (srcWrap->ctx->option & 2) != 0,
                                0, &dstWrap->attrList, &dstWrap->attrCount);
        if (ret >= 0) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        npFreeReqGetRoomMemberExtList(dstWrap, NULL);
    }
    return ret;
}

/* 0x019DFC */
static s32 NP_MATCHING2_NOINLINE npCopyReqBlobBuf(const void *srcParam, void **copyOut)
{
    void *copy = npMatching2Alloc(sizeof(NpMatching2BlobBuf));
    if (copy == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemcpy(copy, srcParam, sizeof(NpMatching2BlobBuf));
    if (copyOut != NULL) {
        *copyOut = copy;
    }
    return 0;
}

/* 0x019E68 */
static s32 NP_MATCHING2_NOINLINE npCopyReqLeaveRoom(const void *srcParam, void **copyOut)
{
    void *copy = npMatching2Alloc(sizeof(NpLeaveRoomWrap));
    if (copy == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemcpy(copy, srcParam, sizeof(NpLeaveRoomWrap));
    if (copyOut != NULL) {
        *copyOut = copy;
    }
    return 0;
}

/* 0x019ED4 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGrantRoomOwner(const void *srcParam, void **copyOut)
{
    void *copy = npMatching2Alloc(sizeof(NpGrantRoomOwnerWrap));
    if (copy == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemcpy(copy, srcParam, sizeof(NpGrantRoomOwnerWrap));
    if (copyOut != NULL) {
        *copyOut = copy;
    }
    return 0;
}

/* 0x019F40 */
static s32 NP_MATCHING2_NOINLINE npCopyReqKickoutMember(const void *srcParam, void **copyOut)
{
    void *copy = npMatching2Alloc(sizeof(NpKickoutMemberWrap));
    if (copy == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemcpy(copy, srcParam, sizeof(NpKickoutMemberWrap));
    if (copyOut != NULL) {
        *copyOut = copy;
    }
    return 0;
}

/* 0x019FAC */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberDataInt(const void *srcParam, void **copyOut)
{
    const NpGetRoomAttrWrap *srcWrap = (const NpGetRoomAttrWrap *)srcParam;
    NpGetRoomAttrWrap *dstWrap;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpGetRoomAttrWrap *)npMatching2Alloc(sizeof(NpGetRoomAttrWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpGetRoomAttrWrap));
        dstWrap->memberId = srcWrap->memberId;
        ret = npBuildAttrIdList(srcWrap->attrId, srcWrap->attrIdNum, 0,
                                (srcWrap->ctx->option & 1) != 0,
                                (srcWrap->ctx->option & 2) != 0,
                                0, &dstWrap->attrId, &dstWrap->attrIdNum);
        if (ret >= 0) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        npFreeReqGetRoomMemberDataInt(dstWrap, NULL);
    }
    return ret;
}

/* 0x01A078 */
static s32 NP_MATCHING2_NOINLINE npCopyReqGetRoomMemberIntList(const void *srcParam, void **copyOut)
{
    const NpGetRoomAttrWrap *srcWrap = (const NpGetRoomAttrWrap *)srcParam;
    NpGetRoomAttrWrap *dstWrap;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    dstWrap = (NpGetRoomAttrWrap *)npMatching2Alloc(sizeof(NpGetRoomAttrWrap));
    if (dstWrap != NULL) {
        sceKernelMemset(dstWrap, 0, sizeof(NpGetRoomAttrWrap));
        ret = npBuildAttrIdList(srcWrap->attrId, srcWrap->attrIdNum, 0,
                                (srcWrap->ctx->option & 1) != 0,
                                (srcWrap->ctx->option & 2) != 0,
                                0, &dstWrap->attrId, &dstWrap->attrIdNum);
        if (ret >= 0) {
            if (copyOut != NULL) {
                *copyOut = dstWrap;
            }
            return 0;
        }
        npFreeReqGetRoomMemberIntList(dstWrap, NULL);
    }
    return ret;
}

/* 0x01A13C */
static s32 NP_MATCHING2_NOINLINE npCopyReqRoomId(const void *srcParam, void **copyOut)
{
    const SceNpMatching2RoomId *srcId = (const SceNpMatching2RoomId *)srcParam;
    SceNpMatching2RoomId *dstId;

    if (srcId == NULL || copyOut == NULL || *srcId == 0) {
        return (s32)0x80550ca0;
    }

    dstId = (SceNpMatching2RoomId *)npMatching2Alloc(sizeof(SceNpMatching2RoomId));
    if (dstId == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstId, 0, sizeof(SceNpMatching2RoomId));
    *dstId = *srcId;
    *copyOut = dstId;
    return 0;
}

/* 0x01A1F0 */
static s32 NP_MATCHING2_NOINLINE npBuildAttrIdList(const SceNpMatching2AttributeId *srcIds, u32 srcCount,
                                                   s32 addNpId, s32 addOnlineName, s32 addAvatarUrl,
                                                   u32 extraSlotCount, SceNpMatching2AttributeId **outList,
                                                   u32 *outCount)
{
    u32 totalCount = srcCount + (addNpId == 1) + (addOnlineName == 1) + (addAvatarUrl == 1) + extraSlotCount;
    SceNpMatching2AttributeId *list;
    u32 writeIdx;

    if (totalCount == 0) {
        *outList = NULL;
        *outCount = 0;
        return 0;
    }

    list = (SceNpMatching2AttributeId *)npMatching2Alloc(sizeof(SceNpMatching2AttributeId) * totalCount);
    if (list == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }

    sceKernelMemset(list, 0, sizeof(SceNpMatching2AttributeId) * totalCount);
    sceKernelMemcpy(list, srcIds, sizeof(SceNpMatching2AttributeId) * srcCount);
    writeIdx = srcCount;
    if (addNpId == 1) {
        list[writeIdx++] = 11;
    }
    if (addOnlineName == 1) {
        list[writeIdx++] = 12;
    }
    if (addAvatarUrl == 1) {
        list[writeIdx++] = 13;
    }
    *outList = list;
    *outCount = totalCount;
    return 0;
}

/* 0x01A358 */
static s32 NP_MATCHING2_NOINLINE npCopyBinFilterArray(const NpProtoBinFilterWire *srcFilters, u32 filterCount,
                                                      NpProtoBinFilterWire **outFilters)
{
    NpProtoBinFilterWire *dstFilters;
    u32 idx;

    dstFilters = (NpProtoBinFilterWire *)npMatching2Alloc(sizeof(NpProtoBinFilterWire) * filterCount);
    if (dstFilters == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstFilters, 0, sizeof(NpProtoBinFilterWire) * filterCount);

    for (idx = 0; idx < filterCount; idx++) {
        dstFilters[idx].op = srcFilters[idx].op;
        dstFilters[idx].attrId = srcFilters[idx].attrId;
        if (srcFilters[idx].ptr != NULL && srcFilters[idx].size != 0) {
            void *bufCopy = npMatching2Alloc(srcFilters[idx].size);
            if (bufCopy == NULL) {
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(bufCopy, srcFilters[idx].ptr, srcFilters[idx].size);
            dstFilters[idx].ptr = bufCopy;
            dstFilters[idx].size = srcFilters[idx].size;
        }
    }
    *outFilters = dstFilters;
    return 0;
}

/* 0x01A468 */
static void NP_MATCHING2_NOINLINE npFreeReqCreateJoinRoom(void *paramCopy, NpMatching2Req *failedReq)
{
    NpCreateJoinWrap *wrap = (NpCreateJoinWrap *)paramCopy;

    if (wrap == NULL) {
        return;
    }

    if (failedReq != NULL) {
        if (failedReq->protoCmd == 0x1202) {
            if (wrap->createdRoom != NULL && wrap->createdRoom->conn != NULL) {
                npConnCloseOrDestroy(wrap->createdRoom->conn);
            }
        } else if (failedReq->protoCmd == 0x3200) {
            npConnCloseById(failedReq->ctxId, failedReq->connId);
        }
    }

    if (wrap->req != NULL) {
        if (wrap->req->roomBinAttrInternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomBinAttrInternal, wrap->req->roomBinAttrInternalNum);
        }
        if (wrap->req->roomPassword != NULL) {
            npMatching2Free((void *)wrap->req->roomPassword);
        }
        if (wrap->req->groupConfig != NULL) {
            npMatching2Free((void *)wrap->req->groupConfig);
        }
        if (wrap->req->joinRoomGroupLabel != NULL) {
            npMatching2Free((void *)wrap->req->joinRoomGroupLabel);
        }
        if (wrap->req->passwordSlotMask != NULL) {
            npMatching2Free((void *)wrap->req->passwordSlotMask);
        }
        if (wrap->req->roomMemberBinAttrInternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomMemberBinAttrInternal, wrap->req->roomMemberBinAttrInternalNum);
        }
        if (wrap->req->roomSearchableIntAttrExternal != NULL) {
            npMatching2Free((void *)wrap->req->roomSearchableIntAttrExternal);
        }
        if (wrap->req->roomBinAttrExternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomBinAttrExternal, wrap->req->roomBinAttrExternalNum);
        }
        npMatching2Free(wrap->req);
    }

    if (wrap->roomEvOpt != NULL) {
        npMatching2Free(wrap->roomEvOpt);
    }
    if (wrap->roomMsgOpt != NULL) {
        npMatching2Free(wrap->roomMsgOpt);
    }
    if (wrap->allowedHandles != NULL) {
        npMatching2Free(wrap->allowedHandles);
    }
    if (wrap->blockedHandles != NULL) {
        npMatching2Free(wrap->blockedHandles);
    }
    if (wrap->groupConfigs != NULL) {
        npMatching2Free(wrap->groupConfigs);
    }
    if (wrap->sigOptBlob != NULL) {
        npMatching2Free(wrap->sigOptBlob);
    }
    npMatching2Free(wrap);
}

/* 0x01A684 */
static void NP_MATCHING2_NOINLINE npFreeReqJoinRoom(void *paramCopy, NpMatching2Req *failedReq)
{
    NpJoinRoomWrap *wrap = (NpJoinRoomWrap *)paramCopy;

    if (wrap == NULL) {
        return;
    }

    if (failedReq != NULL) {
        if (failedReq->protoCmd == 0x1203) {
            if (wrap->joinedRoom != NULL && wrap->joinedRoom->conn != NULL) {
                npConnCloseOrDestroy(wrap->joinedRoom->conn);
            }
        } else if (failedReq->protoCmd == 0x3201) {
            npConnCloseById(failedReq->ctxId, failedReq->connId);
        }
    }

    if (wrap->req != NULL) {
        if (wrap->req->roomMemberBinAttrInternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomMemberBinAttrInternal, wrap->req->roomMemberBinAttrInternalNum);
        }
        if (wrap->req->roomPassword != NULL) {
            npMatching2Free((void *)wrap->req->roomPassword);
        }
        if (wrap->req->joinRoomGroupLabel != NULL) {
            npMatching2Free((void *)wrap->req->joinRoomGroupLabel);
        }
        npMatching2Free(wrap->req);
    }

    if (wrap->roomEvOpt != NULL) {
        npMatching2Free(wrap->roomEvOpt);
    }
    if (wrap->roomMsgOpt != NULL) {
        npMatching2Free(wrap->roomMsgOpt);
    }
    npMatching2Free(wrap);
}

/* 0x01A7A4 */
static void NP_MATCHING2_NOINLINE npFreeReqSearchRoom(void *paramCopy, NpMatching2Req *failedReq)
{
    NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }

    if (wrap->req != NULL) {
        SceNpMatching2SearchRoomRequest *searchReq = (SceNpMatching2SearchRoomRequest *)wrap->req;
        if (searchReq->intFilter != NULL) {
            npMatching2Free((void *)searchReq->intFilter);
        }
        if (searchReq->binFilter != NULL) {
            const NpProtoBinFilterWire *binFilters = (const NpProtoBinFilterWire *)searchReq->binFilter;
            u32 idx;
            for (idx = 0; idx < searchReq->binFilterNum; idx++) {
                if (binFilters[idx].ptr != NULL) {
                    npMatching2Free((void *)binFilters[idx].ptr);
                }
            }
            npMatching2Free((void *)searchReq->binFilter);
        }
        if (searchReq->attrId != NULL) {
            npMatching2Free((void *)searchReq->attrId);
        }
        npMatching2Free(searchReq);
    }
    npMatching2Free(wrap);
}

/* 0x01A884 */
static void NP_MATCHING2_NOINLINE npFreeReqServerId(void *paramCopy, NpMatching2Req *failedReq)
{
    (void)failedReq;
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01A8A4 */
static void NP_MATCHING2_NOINLINE npFreeReqSetUserInfo(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSetUserInfoWrap *wrap = (NpSetUserInfoWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->userBinAttr != NULL) {
            npWorkerFreeBinAttrArray(wrap->req->userBinAttr, wrap->req->userBinAttrNum);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01A900 */
static void NP_MATCHING2_NOINLINE npFreeReqGetUserInfoList(void *paramCopy, NpMatching2Req *failedReq)
{
    NpGetUserInfoWrap *wrap = (NpGetUserInfoWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->npHandles != NULL) {
        npMatching2Free(wrap->npHandles);
    }
    if (wrap->req != NULL) {
        if (wrap->req->attrId != NULL) {
            npMatching2Free((void *)wrap->req->attrId);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01A978 */
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberExtList(void *paramCopy, NpMatching2Req *failedReq)
{
    NpGetRoomMemberExtWrap *wrap = (NpGetRoomMemberExtWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->attrList != NULL) {
        npMatching2Free(wrap->attrList);
    }
    npMatching2Free(wrap);
}

/* 0x01A9C0 */
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomDataExt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSetRoomDataExtWrap *wrap = (NpSetRoomDataExtWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->roomSearchableIntAttrExternal != NULL) {
            npMatching2Free((void *)wrap->req->roomSearchableIntAttrExternal);
        }
        if (wrap->req->roomBinAttrExternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomBinAttrExternal, wrap->req->roomBinAttrExternalNum);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01AA38 */
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomDataExtList(void *paramCopy, NpMatching2Req *failedReq)
{
    NpCtxReqPairWrap *wrap = (NpCtxReqPairWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        SceNpMatching2GetRoomDataExternalListRequest *extReq = (SceNpMatching2GetRoomDataExternalListRequest *)wrap->req;
        if (extReq->roomId != NULL) {
            npMatching2Free((void *)extReq->roomId);
            extReq = (SceNpMatching2GetRoomDataExternalListRequest *)wrap->req;
        }
        if (extReq->attrId != NULL) {
            npMatching2Free((void *)extReq->attrId);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01AAB0 */
static void NP_MATCHING2_NOINLINE npFreeReqBlobBuf(void *paramCopy, NpMatching2Req *failedReq)
{
    (void)failedReq;
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01AAD0 */
static void NP_MATCHING2_NOINLINE npFreeReqLeaveRoom(void *paramCopy, NpMatching2Req *failedReq)
{
    if (failedReq != NULL && failedReq->protoCmd != 0x1205) {
        npConnCloseById(failedReq->ctxId, failedReq->connId);
    }
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01AB20 */
static void NP_MATCHING2_NOINLINE npFreeReqGrantRoomOwner(void *paramCopy, NpMatching2Req *failedReq)
{
    (void)failedReq;
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01AB40 */
static void NP_MATCHING2_NOINLINE npFreeReqKickoutMember(void *paramCopy, NpMatching2Req *failedReq)
{
    (void)failedReq;
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01AB60 */
static void NP_MATCHING2_NOINLINE npFreeReqSendRoomChat(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSendRoomChatWrap *wrap = (NpSendRoomChatWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->castType == 3) {
            npMatching2Free((void *)wrap->req->target.multicastTarget.memberId);
        }
        if (wrap->req->msg != NULL) {
            npMatching2Free((void *)wrap->req->msg);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01ABD8 */
static void NP_MATCHING2_NOINLINE npFreeReqSendRoomMsg(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSendRoomMsgWrap *wrap = (NpSendRoomMsgWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->castType == 3) {
            npMatching2Free((void *)wrap->req->target.multicastTarget.memberId);
        }
        if (wrap->req->msg != NULL) {
            npMatching2Free((void *)wrap->req->msg);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01AC50 */
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomDataInt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSetRoomDataInternalWrap *wrap = (NpSetRoomDataInternalWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->roomBinAttrInternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomBinAttrInternal, wrap->req->roomBinAttrInternalNum);
        }
        if (wrap->req->passwordConfig != NULL) {
            npMatching2Free((void *)wrap->req->passwordConfig);
        }
        if (wrap->req->passwordSlotMask != NULL) {
            npMatching2Free((void *)wrap->req->passwordSlotMask);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01ACE4 */
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomDataInt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->attrId != NULL) {
        npMatching2Free(wrap->attrId);
    }
    npMatching2Free(wrap);
}

/* 0x01AD2C */
static void NP_MATCHING2_NOINLINE npFreeReqSetRoomMemberDataInt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSetRoomMemberDataInternalWrap *wrap = (NpSetRoomMemberDataInternalWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        if (wrap->req->roomMemberBinAttrInternal != NULL) {
            npWorkerFreeBinAttrArray((SceNpMatching2BinAttr *)wrap->req->roomMemberBinAttrInternal, wrap->req->roomMemberBinAttrInternalNum);
        }
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01AD88 */
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberDataInt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->attrId != NULL) {
        npMatching2Free(wrap->attrId);
    }
    npMatching2Free(wrap);
}

/* 0x01ADD0 */
static void NP_MATCHING2_NOINLINE npFreeReqGetRoomMemberIntList(void *paramCopy, NpMatching2Req *failedReq)
{
    NpGetRoomAttrWrap *wrap = (NpGetRoomAttrWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->attrId != NULL) {
        npMatching2Free(wrap->attrId);
    }
    npMatching2Free(wrap);
}

/* 0x01AE18 */
static void NP_MATCHING2_NOINLINE npFreeReqSetSigOpt(void *paramCopy, NpMatching2Req *failedReq)
{
    NpSetSigOptWrap *wrap = (NpSetSigOptWrap *)paramCopy;

    (void)failedReq;
    if (wrap == NULL) {
        return;
    }
    if (wrap->req != NULL) {
        npMatching2Free(wrap->req);
    }
    npMatching2Free(wrap);
}

/* 0x01AE60 */
static void NP_MATCHING2_NOINLINE npFreeReqRoomId(void *paramCopy, NpMatching2Req *failedReq)
{
    (void)failedReq;
    if (paramCopy != NULL) {
        npMatching2Free(paramCopy);
    }
}

/* 0x01AE80 */
s32 NP_MATCHING2_NOINLINE npRoomAlloc(SceNpMatching2ServerId serverId, u16 worldNum, u16 lobbyPart, u16 roomNum,
                                      NpMatching2Conn *conn, const SceNpMatching2RoomEventOptParam *roomEvOpt,
                                      const SceNpMatching2RoomMessageOptParam *roomMsgOpt, NpMatching2Room **outRoom)
{
    NpMatching2Room *room;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    room = (NpMatching2Room *)npMatching2Alloc(sizeof(NpMatching2Room));
    if (room != NULL) {
        sceKernelMemset(room, 0, sizeof(NpMatching2Room));
        room->worldId = ((u32)serverId << 16) | (u32)worldNum;
        room->roomId = ((u64)(((u32)serverId << 16) | (u32)worldNum) << 32) | ((u32)lobbyPart << 16) | (u32)roomNum;
        room->serverId = serverId;

        room->binAttrInternal = (SceNpMatching2BinAttrInternal *)npMatching2Alloc(
            sizeof(SceNpMatching2BinAttrInternal) * g_npMatching2AttrLimits.maxRoomBinAttrInternal);
        if (room->binAttrInternal != NULL) {
            u32 attrCount = g_npMatching2AttrLimits.maxRoomBinAttrInternal;
            room->binAttrInternalCount = attrCount;
            sceKernelMemset(room->binAttrInternal, 0, sizeof(SceNpMatching2BinAttrInternal) * attrCount);
            room->binAttrInternal[0].attr.id = 87;
            room->binAttrInternal[1].attr.id = 88;

            ret = npConnFindAndRef(conn->server->ctx->ctxId, conn->connId, &room->conn);
            if (ret >= 0) {
                conn->ownerKind = 2;
                conn->ownerRoom = room;
                if (roomEvOpt == NULL) {
                    roomEvOpt = &conn->server->ctx->defaultRoomEvOpt;
                }
                sceKernelMemcpy(&room->roomEvOpt, roomEvOpt, sizeof(SceNpMatching2RoomEventOptParam));
                if (roomMsgOpt == NULL) {
                    roomMsgOpt = &conn->server->ctx->defaultRoomMsgOpt;
                }
                sceKernelMemcpy(&room->roomMsgOpt, roomMsgOpt, sizeof(SceNpMatching2RoomMessageOptParam));
                if (outRoom != NULL) {
                    *outRoom = room;
                }
                return 0;
            }
        }
        npRoomDestroy(room);
    }
    return ret;
}

/* 0x01B0AC */
s32 NP_MATCHING2_NOINLINE npRoomUpdateDataInternal(NpMatching2Room *room, const u32 *flagAttr,
                                                   SceNpMatching2BinAttrInternal *binAttrs, u32 binAttrNum,
                                                   SceNpMatching2BinAttrInternal **updatedAttrsOut,
                                                   u32 *updatedAttrCountInOut, const u64 *passwordSlotMask,
                                                   s32 *passwordChangedOut,
                                                   SceNpMatching2RoomGroup **updatedGroupsOut,
                                                   u32 *updatedGroupCountInOut)
{
    u32 updatedCount = 0;
    u32 maxUpdatedAttrs = 0;
    u32 maxUpdatedGroups = 0;
    u32 idx;
    s32 ret = 0;

    if (updatedAttrsOut != NULL && updatedAttrCountInOut != NULL) {
        maxUpdatedAttrs = *updatedAttrCountInOut;
    }
    if (updatedGroupsOut != NULL && updatedGroupCountInOut != NULL) {
        maxUpdatedGroups = *updatedGroupCountInOut;
        *updatedGroupCountInOut = 0;
    }
    if (passwordChangedOut != NULL) {
        *passwordChangedOut = 0;
    }
    if (flagAttr != NULL) {
        room->flagAttr = *flagAttr;
    }

    for (idx = 0; idx < binAttrNum; idx++) {
        if (binAttrs[idx].attr.id != 0) {
            u32 dstIdx;
            for (dstIdx = 0; dstIdx < room->binAttrInternalCount; dstIdx++) {
                if (room->binAttrInternal[dstIdx].attr.id == binAttrs[idx].attr.id) {
                    break;
                }
            }
            if (dstIdx != g_npMatching2AttrLimits.maxRoomBinAttrInternal) {
                if (room->binAttrInternal[dstIdx].attr.ptr != NULL) {
                    npMatching2Free(room->binAttrInternal[dstIdx].attr.ptr);
                }
                sceKernelMemcpy(&room->binAttrInternal[dstIdx], &binAttrs[idx], sizeof(SceNpMatching2BinAttrInternal));
                sceKernelMemset(&binAttrs[idx], 0, sizeof(SceNpMatching2BinAttrInternal));
                if (updatedCount < maxUpdatedAttrs) {
                    updatedAttrsOut[updatedCount++] = &room->binAttrInternal[dstIdx];
                }
            }
        }
    }

    if (updatedAttrsOut != NULL && updatedAttrCountInOut != NULL) {
        *updatedAttrCountInOut = updatedCount;
    }

    if (passwordSlotMask != NULL) {
        if (room->groupList != NULL && room->groupCount != 0) {
            ret = npWorkerSyncRoomGroups(NULL, 0, passwordSlotMask,
                                         room->groupList, room->groupCount,
                                         updatedGroupsOut, &maxUpdatedGroups);
            if (ret >= 0 && updatedGroupCountInOut != NULL) {
                *updatedGroupCountInOut = maxUpdatedGroups;
            }
        } else {
            sceKernelMemcpy(&room->passwordSlotMask, passwordSlotMask, sizeof(u64));
            if (passwordChangedOut != NULL) {
                *passwordChangedOut = 1;
            }
        }
    }
    return ret;
}

/* 0x01B2EC */
s32 NP_MATCHING2_NOINLINE npRoomCreateMember(const SceNpId *npId, const char *onlineName, const char *avatarUrl,
                                             u64 joinDate, SceNpMatching2RoomMemberId memberId, u8 teamId, u8 natType,
                                             u32 flagAttr, NpMatching2Room *room, NpMatching2RoomMember **outMember)
{
    NpMatching2RoomMember *member;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    member = (NpMatching2RoomMember *)npMatching2Alloc(sizeof(NpMatching2RoomMember));
    if (member != NULL) {
        sceKernelMemset(member, 0, sizeof(NpMatching2RoomMember));
        sceKernelMemcpy(&member->npId, npId, sizeof(SceNpId));
        member->joinDate = joinDate;
        member->memberId = memberId;
        member->teamId = teamId;
        member->natType = natType;
        member->flagAttr = flagAttr;

        if (onlineName != NULL) {
            member->onlineName = (char *)npMatching2Alloc(52);
            if (member->onlineName == NULL) {
                npRoomFreeMember(member);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(member->onlineName, onlineName, 52);
        }

        if (avatarUrl != NULL) {
            member->avatarUrl = (char *)npMatching2Alloc(128);
            if (member->avatarUrl == NULL) {
                npRoomFreeMember(member);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemcpy(member->avatarUrl, avatarUrl, 128);
        }

        if (room->groupList != NULL) {
            u32 groupIdx;
            u16 slotAccum = 0;
            for (groupIdx = 0; groupIdx < room->groupCount; groupIdx++) {
                slotAccum = (u16)(slotAccum + (u16)room->groupList[groupIdx].slotNum);
                if (slotAccum >= ((u32)memberId >> 4)) {
                    break;
                }
            }
            if (groupIdx == room->groupCount) {
                npRoomFreeMember(member);
                return (s32)0x80550ca9;
            }
            member->roomGroup = &room->groupList[groupIdx];
        } else {
            member->roomGroup = NULL;
        }

        {
            u32 attrCount = g_npMatching2AttrLimits.maxMemberBinAttrInternal;
            member->binAttrInternal = (SceNpMatching2MemberBinAttrInternal *)npMatching2Alloc(
                sizeof(SceNpMatching2MemberBinAttrInternal) * attrCount);
            if (member->binAttrInternal != NULL) {
                member->binAttrInternalCount = attrCount;
                sceKernelMemset(member->binAttrInternal, 0, sizeof(SceNpMatching2MemberBinAttrInternal) * attrCount);
                member->binAttrInternal[0].attr.id = 89;
                if (outMember != NULL) {
                    *outMember = member;
                }
                return 0;
            }
        }
        npRoomFreeMember(member);
    }
    return ret;
}

/* 0x01B514 */
s32 NP_MATCHING2_NOINLINE npRoomInsertMember(const SceNpId *npId, const char *onlineName, const char *avatarUrl,
                                             u64 joinDate, SceNpMatching2RoomMemberId memberId, u8 teamId, u8 natType,
                                             u32 flagAttr, const SceNpId *selfNpId, NpMatching2Room *room,
                                             NpMatching2RoomMember **outMember)
{
    NpMatching2RoomMember *member = NULL;
    s32 ret;

    ret = npRoomCreateMember(npId, onlineName, avatarUrl, joinDate, memberId, teamId, natType, flagAttr, room, &member);
    if (ret >= 0) {
        s32 intr = sceKernelCpuSuspendIntr();
        NpMatching2RoomMember *cur = room->memberList;
        NpMatching2RoomMember *prev = NULL;

        while (cur != NULL && ((u32)member->memberId >> 4) >= ((u32)cur->memberId >> 4)) {
            prev = cur;
            cur = cur->next;
        }
        if (prev != NULL) {
            member->next = cur;
            prev->next = member;
        } else {
            member->next = room->memberList;
            room->memberList = member;
        }
        sceKernelCpuResumeIntr(intr);

        room->memberCount++;
        if (sceNet_lib_A0F16ABD(npId->handle, selfNpId->handle) == 0) {
            room->meMember = member;
        }
        if ((s32)member->flagAttr < 0) {
            room->ownerMember = member;
        }
        if (outMember != NULL) {
            *outMember = member;
        }
        return 0;
    }

    if (member != NULL) {
        npRoomFreeMember(member);
    }
    return ret;
}

/* 0x01B664 */
s32 NP_MATCHING2_NOINLINE npRoomFreeMember(NpMatching2RoomMember *member)
{
    if (member == NULL) {
        return 0;
    }
    if (member->onlineName != NULL) {
        npMatching2Free(member->onlineName);
    }
    if (member->avatarUrl != NULL) {
        npMatching2Free(member->avatarUrl);
    }
    if (member->binAttrInternal != NULL) {
        npProtoFreeMemberBinAttrIntArray(member->binAttrInternal, member->binAttrInternalCount);
        npMatching2Free(member->binAttrInternal);
    }
    return npMatching2Free(member);
}

/* 0x01B6EC */
s32 NP_MATCHING2_NOINLINE npRoomUpdateMember(NpMatching2RoomMember *member, u32 flagFilter, u32 flagAttr,
                                             u8 teamId, u8 natType,
                                             SceNpMatching2MemberBinAttrInternal *binAttrs, u32 binAttrNum,
                                             SceNpMatching2MemberBinAttrInternal **updatedOut, u32 *updatedCountInOut)
{
    u32 updatedCount = 0;
    u32 maxUpdated = 0;
    u32 idx;

    if (updatedOut != NULL && updatedCountInOut != NULL) {
        maxUpdated = *updatedCountInOut;
    }
    if (flagFilter != 0) {
        member->flagAttr = flagAttr;
    }
    if (teamId != 0) {
        member->teamId = teamId;
    }
    if (natType != 0) {
        member->natType = natType;
    }

    for (idx = 0; idx < binAttrNum; idx++) {
        if (binAttrs[idx].attr.id != 0) {
            u32 dstIdx;
            for (dstIdx = 0; dstIdx < member->binAttrInternalCount; dstIdx++) {
                if (member->binAttrInternal[dstIdx].attr.id == binAttrs[idx].attr.id) {
                    break;
                }
            }
            if (dstIdx != g_npMatching2AttrLimits.maxMemberBinAttrInternal) {
                if (member->binAttrInternal[dstIdx].attr.ptr != NULL) {
                    npMatching2Free(member->binAttrInternal[dstIdx].attr.ptr);
                }
                sceKernelMemcpy(&member->binAttrInternal[dstIdx], &binAttrs[idx], sizeof(SceNpMatching2MemberBinAttrInternal));
                sceKernelMemset(&binAttrs[idx], 0, sizeof(SceNpMatching2MemberBinAttrInternal));
                if (updatedCount < maxUpdated) {
                    updatedOut[updatedCount++] = &member->binAttrInternal[dstIdx];
                }
            }
        }
    }

    if (updatedOut != NULL && updatedCountInOut != NULL) {
        *updatedCountInOut = updatedCount;
    }
    return 0;
}

/* 0x01B894 */
s32 NP_MATCHING2_NOINLINE npRoomCopyMemberBinAttrs(NpMatching2RoomMember *member, const SceNpMatching2BinAttr *srcAttrs,
                                                   u32 srcCount, const u64 *updateDate)
{
    u32 idx;

    for (idx = 0; idx < srcCount; idx++) {
        if (srcAttrs[idx].id != 0) {
            u32 dstIdx;
            for (dstIdx = 0; dstIdx < member->binAttrInternalCount; dstIdx++) {
                if (member->binAttrInternal[dstIdx].attr.id == srcAttrs[idx].id) {
                    break;
                }
            }
            if (dstIdx != member->binAttrInternalCount) {
                void *bufCopy = npMatching2Alloc(srcAttrs[idx].size);
                member->binAttrInternal[dstIdx].attr.ptr = bufCopy;
                if (bufCopy == NULL) {
                    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                }
                sceKernelMemcpy(bufCopy, srcAttrs[idx].ptr, srcAttrs[idx].size);
                member->binAttrInternal[dstIdx].attr.size = srcAttrs[idx].size;
                sceKernelMemcpy(&member->binAttrInternal[dstIdx].updateDate, updateDate, sizeof(u64));
            }
        }
    }
    return 0;
}

/* 0x01B9C8 */
s32 NP_MATCHING2_NOINLINE npRoomRemoveMemberById(NpMatching2Room *room, SceNpMatching2RoomMemberId memberId,
                                                 NpMatching2RoomMember **outMember)
{
    s32 intr;
    NpMatching2RoomMember *cur;
    NpMatching2RoomMember *prev = NULL;
    s32 ret = 0;

    intr = sceKernelCpuSuspendIntr();
    cur = room->memberList;
    while (cur != NULL && cur->memberId != memberId) {
        prev = cur;
        cur = cur->next;
    }
    if (cur != NULL) {
        if (prev != NULL) {
            prev->next = cur->next;
        } else {
            room->memberList = cur->next;
        }
        cur->next = NULL;
        room->memberCount--;
        if (outMember != NULL) {
            *outMember = cur;
        }
    } else {
        ret = (s32)0x80550cab;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

/* 0x01BA8C */
void NP_MATCHING2_NOINLINE npRoomDestroy(NpMatching2Room *room)
{
    s32 intr;
    NpMatching2ServerNode *server;
    NpMatching2Conn *conn;
    NpMatching2RoomMember *member;

    if (room == NULL) {
        return;
    }

    intr = sceKernelCpuSuspendIntr();
    room->destroying = 1;
    server = room->server;
    conn = room->conn;
    if (room->refCount != 0) {
        sceKernelCpuResumeIntr(intr);
        return;
    }

    room->server = NULL;
    room->conn = NULL;
    if (server != NULL) {
        s32 innerIntr = sceKernelCpuSuspendIntr();
        NpMatching2Room *cur = server->roomList;
        NpMatching2Room *prev = NULL;
        while (cur != NULL && cur != room) {
            prev = cur;
            cur = cur->next;
        }
        if (cur != NULL) {
            if (prev != NULL) {
                prev->next = cur->next;
            } else {
                server->roomList = cur->next;
            }
            cur->server = NULL;
        }
        sceKernelCpuResumeIntr(innerIntr);
    }
    sceKernelCpuResumeIntr(intr);

    member = room->memberList;
    while (member != NULL) {
        NpMatching2RoomMember *nextMember = member->next;
        npRoomFreeMember(member);
        member = nextMember;
    }

    if (room->groupList != NULL) {
        npMatching2Free(room->groupList);
    }
    if (room->binAttrInternal != NULL) {
        npProtoFreeRoomBinAttrIntArray(room->binAttrInternal, (u16)room->binAttrInternalCount);
        npMatching2Free(room->binAttrInternal);
    }
    npMatching2Free(room);

    if (conn != NULL) {
        conn->ownerKind = 0;
        conn->ownerRoom = NULL;
        npConnRelease(conn);
    }
}

/* 0x01BBF0 */
s32 NP_MATCHING2_NOINLINE npRoomFindByIdAndRef(NpMatching2Context *ctx, SceNpMatching2RoomId roomId, NpMatching2Room **outRoom)
{
    NpMatching2ServerNode *server = NULL;
    NpMatching2Room *cur;
    s32 intr;
    s32 ret;

    ret = npServerFindByIdAndRef(ctx, (SceNpMatching2ServerId)(roomId >> 48), &server);
    if (ret < 0) {
        return ret;
    }

    intr = sceKernelCpuSuspendIntr();
    for (cur = server->roomList; cur != NULL; cur = cur->next) {
        if (cur->roomId == roomId) {
            break;
        }
    }
    if (cur == NULL) {
        ret = (s32)0x80550caa;
    } else if (cur->destroying == 1) {
        ret = SCE_NP_MATCHING2_ERROR_DESTROYING;
    } else if (outRoom != NULL) {
        *outRoom = cur;
        cur->refCount++;
    }
    sceKernelCpuResumeIntr(intr);
    npServerRelease(server);
    return ret;
}

/* 0x01BCDC */
s32 NP_MATCHING2_NOINLINE npRoomConvertExternalList(NpProtoRoomDataExternal *srcList, u16 count,
                                                    const SceNpMatching2AttributeId *attrIds, u16 attrIdCount,
                                                    SceNpMatching2RoomDataExternalOut **outList)
{
    SceNpMatching2RoomDataExternalOut *dstList;
    SceNpMatching2RoomDataExternalOut *prevNode = NULL;
    u32 idx;
    s32 ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    if (srcList == NULL) {
        return 0;
    }

    dstList = (SceNpMatching2RoomDataExternalOut *)npMatching2Alloc(sizeof(SceNpMatching2RoomDataExternalOut) * count);
    if (dstList == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstList, 0, sizeof(SceNpMatching2RoomDataExternalOut) * count);

    for (idx = 0; idx < count; idx++) {
        NpProtoRoomDataExternal *src = &srcList[idx];
        SceNpMatching2RoomDataExternalOut *dst = &dstList[idx];
        u32 intMatchCount = 0;
        u32 searchBinMatchCount = 0;
        u32 extBinMatchCount = 0;
        u16 attrId;
        u32 aIdx;

        dst->serverId = (u16)(src->worldId >> 16);
        dst->worldId = src->worldId;
        dst->roomId = src->roomId;
        dst->flagAttr = src->flagAttr;
        dst->maxSlot = src->maxSlot;

        ret = npWorkerCountActiveSlots(src->reservedMeta, &dst->curMemberNum);
        if (ret < 0) {
            npRoomFreeExternalList(dstList, count);
            return ret;
        }

        ret = npWorkerDecodeRoomGroups(src->reservedMeta, src->publicSlotMeta,
                                       NULL, 0, src->openDate,
                                       &dst->roomGroup, &dst->roomGroupNum);
        if (ret < 0) {
            npRoomFreeExternalList(dstList, count);
            return ret;
        }
        if (dst->roomGroup == NULL) {
            dst->passwordSlotMask = src->openDate;
        }

        for (attrId = 76; attrId < 84; attrId++) {
            for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
                if (attrIds[aIdx] == attrId) {
                    intMatchCount++;
                }
            }
        }
        if (intMatchCount != 0) {
            u32 slot = 0;
            u32 srcIdx;
            dst->searchableIntAttr = (SceNpMatching2IntAttr *)npMatching2Alloc(sizeof(SceNpMatching2IntAttr) * intMatchCount);
            if (dst->searchableIntAttr == NULL) {
                npRoomFreeExternalList(dstList, count);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            dst->searchableIntAttrNum = intMatchCount;
            sceKernelMemset(dst->searchableIntAttr, 0, sizeof(SceNpMatching2IntAttr) * intMatchCount);
            for (attrId = 76; attrId < 84; attrId++) {
                for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
                    if (attrIds[aIdx] == attrId) {
                        dst->searchableIntAttr[slot++].id = attrId;
                    }
                }
            }
            for (srcIdx = 0; srcIdx < 8; srcIdx++) {
                if (src->searchableIntAttr[srcIdx].id != 0) {
                    u32 dstIdx;
                    for (dstIdx = 0; dstIdx < dst->searchableIntAttrNum; dstIdx++) {
                        if (dst->searchableIntAttr[dstIdx].id == src->searchableIntAttr[srcIdx].id) {
                            dst->searchableIntAttr[dstIdx].num = src->searchableIntAttr[srcIdx].num;
                            break;
                        }
                    }
                }
            }
        }

        for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
            if (attrIds[aIdx] == 84) {
                searchBinMatchCount++;
            }
        }
        if (searchBinMatchCount != 0) {
            u32 slot = 0;
            dst->searchableBinAttr = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * searchBinMatchCount);
            if (dst->searchableBinAttr == NULL) {
                npRoomFreeExternalList(dstList, count);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            dst->searchableBinAttrNum = searchBinMatchCount;
            sceKernelMemset(dst->searchableBinAttr, 0, sizeof(SceNpMatching2BinAttr) * searchBinMatchCount);
            for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
                if (attrIds[aIdx] == 84) {
                    dst->searchableBinAttr[slot++].id = 84;
                }
            }
            if (src->searchableBinAttr.id != 0) {
                u32 dstIdx;
                for (dstIdx = 0; dstIdx < dst->searchableBinAttrNum; dstIdx++) {
                    if (dst->searchableBinAttr[dstIdx].id == src->searchableBinAttr.id) {
                        sceKernelMemcpy(&dst->searchableBinAttr[dstIdx], &src->searchableBinAttr, sizeof(SceNpMatching2BinAttr));
                        sceKernelMemset(&src->searchableBinAttr, 0, sizeof(SceNpMatching2BinAttr));
                        break;
                    }
                }
            }
        }

        for (attrId = 85; attrId < 87; attrId++) {
            for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
                if (attrIds[aIdx] == attrId) {
                    extBinMatchCount++;
                }
            }
        }
        if (extBinMatchCount != 0) {
            u32 slot = 0;
            u32 srcIdx;
            dst->binAttrExternal = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * extBinMatchCount);
            if (dst->binAttrExternal == NULL) {
                npRoomFreeExternalList(dstList, count);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            dst->binAttrExternalNum = extBinMatchCount;
            sceKernelMemset(dst->binAttrExternal, 0, sizeof(SceNpMatching2BinAttr) * extBinMatchCount);
            for (attrId = 85; attrId < 87; attrId++) {
                for (aIdx = 0; aIdx < attrIdCount; aIdx++) {
                    if (attrIds[aIdx] == attrId) {
                        dst->binAttrExternal[slot++].id = attrId;
                    }
                }
            }
            for (srcIdx = 0; srcIdx < 2; srcIdx++) {
                if (src->binAttrExternal[srcIdx].id != 0) {
                    u32 dstIdx;
                    for (dstIdx = 0; dstIdx < dst->binAttrExternalNum; dstIdx++) {
                        if (dst->binAttrExternal[dstIdx].id == src->binAttrExternal[srcIdx].id) {
                            sceKernelMemcpy(&dst->binAttrExternal[dstIdx], &src->binAttrExternal[srcIdx], sizeof(SceNpMatching2BinAttr));
                            sceKernelMemset(&src->binAttrExternal[srcIdx], 0, sizeof(SceNpMatching2BinAttr));
                            break;
                        }
                    }
                }
            }
        }

        if (src->ownerNpId != NULL) {
            dst->owner = (SceNpMatching2RoomOwnerInfoOut *)npMatching2Alloc(sizeof(SceNpMatching2RoomOwnerInfoOut));
            if (dst->owner == NULL) {
                npRoomFreeExternalList(dstList, count);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            sceKernelMemset(dst->owner, 0, sizeof(SceNpMatching2RoomOwnerInfoOut));
            sceKernelMemcpy(&dst->owner->npId, src->ownerNpId, sizeof(SceNpId));
            if (src->ownerOnlineName != NULL) {
                dst->owner->onlineName = src->ownerOnlineName;
                src->ownerOnlineName = NULL;
            }
            if (src->ownerAvatarUrl != NULL) {
                dst->owner->avatarUrl = src->ownerAvatarUrl;
                src->ownerAvatarUrl = NULL;
            }
        }

        if (prevNode != NULL) {
            prevNode->next = dst;
        }
        prevNode = dst;
    }

    if (outList != NULL) {
        *outList = dstList;
    }
    return 0;
}

/* 0x01C3AC */
s32 NP_MATCHING2_NOINLINE npRoomFreeExternalList(SceNpMatching2RoomDataExternalOut *list, u16 count)
{
    u32 idx;

    if (list == NULL) {
        return 0;
    }
    for (idx = 0; idx < count; idx++) {
        SceNpMatching2RoomDataExternalOut *item = &list[idx];
        if (item->roomGroup != NULL) {
            npMatching2Free(item->roomGroup);
        }
        if (item->searchableIntAttr != NULL) {
            npMatching2Free(item->searchableIntAttr);
        }
        if (item->searchableBinAttr != NULL) {
            npWorkerFreeBinAttrArray(item->searchableBinAttr, item->searchableBinAttrNum);
        }
        if (item->binAttrExternal != NULL) {
            npWorkerFreeBinAttrArray(item->binAttrExternal, item->binAttrExternalNum);
        }
        if (item->owner != NULL) {
            if (item->owner->onlineName != NULL) {
                npMatching2Free(item->owner->onlineName);
            }
            if (item->owner->avatarUrl != NULL) {
                npMatching2Free(item->owner->avatarUrl);
            }
            npMatching2Free(item->owner);
        }
    }
    return npMatching2Free(list);
}

/* 0x01C4CC */
s32 NP_MATCHING2_NOINLINE npRoomAbortJoinOnError(NpMatching2Req *req, s32 errorCode, const void *optData)
{
    NpMatching2Context *ctx = NULL;
    NpMatching2Room *room = NULL;
    NpMatching2Conn *roomConn = NULL;
    NpJoinRoomWrap *wrap = (NpJoinRoomWrap *)req->paramCopy;
    struct {
        u8 cause;
        u8 padBytes[3];
        s32 errorCode;
        u8 optData[20];
    } destroyEv;
    SceNpMatching2RequestOptParam optParam;
    s32 ret;

    ret = npCtxFindAndRef(req->ctxId, &ctx);
    if (ret < 0) {
        return ret;
    }

    ret = npRoomFindByIdAndRef(ctx, wrap->req->roomId, &room);
    if (ret >= 0) {
        sceKernelMemset(&destroyEv, 0, sizeof(destroyEv));
        destroyEv.cause = 7;
        if (optData != NULL) {
            sceKernelMemcpy(destroyEv.optData, optData, 20);
        }
        destroyEv.errorCode = errorCode;
        npWorkerDispatchRoomEvent(0, room, 0x1104, &destroyEv);

        npLockAcquire();
        if (room->conn != NULL) {
            ret = npConnFindAndRef(ctx->ctxId, room->conn->connId, &roomConn);
            npLockRelease();
            if (ret >= 0) {
                npWorkerCancelConnReqs(roomConn, errorCode);
                if (room->meMember == room->ownerMember) {
                    npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), roomConn->server);
                    if (room->memberCount == 1 || (s32)room->flagAttr >= 0) {
                        sceKernelMemset(&optParam, 0, sizeof(optParam));
                        optParam.timeout = 30000000;
                        npCoreSubmitRoomInfoInternal(roomConn->server->ctx->ctxId, room->roomId, &optParam, NULL);
                    }
                }
                npRoomDestroy(room);
                npConnCloseOrDestroy(roomConn);
            }
        } else {
            npLockRelease();
        }
    }

    if (room != NULL) {
        npRoomRelease(room);
    }
    if (roomConn != NULL) {
        npConnRelease(roomConn);
    }
    if (ctx != NULL) {
        npCtxRelease(ctx);
    }
    return ret;
}

/* 0x01C710 */
s32 NP_MATCHING2_NOINLINE npRoomAddMembersFromWire(NpProtoRoomMemberInternalWire *members, u16 count, NpMatching2Room *room)
{
    NpMatching2UserProfile *profile = NULL;
    NpMatching2RoomMember *inserted = NULL;
    u32 idx;
    s32 ret;

    ret = npWorkerGetLocalProfile(&profile);
    if (ret < 0) {
        return ret;
    }

    for (idx = 0; idx < count; idx++) {
        NpProtoRoomMemberInternalWire *wire = &members[idx];
        ret = npRoomInsertMember(&wire->npId, wire->onlineName, wire->avatarUrl,
                                 wire->joinDate, wire->memberId,
                                 wire->natType, wire->teamId, wire->flagAttr,
                                 &profile->npId, room, &inserted);
        if (ret < 0) {
            break;
        }
        if (room->roomEvOpt.optFlags[3] == 1) {
            sceKernelMemcpy(inserted->binAttrInternal, &wire->binAttrInternal, sizeof(SceNpMatching2MemberBinAttrInternal));
            sceKernelMemset(&wire->binAttrInternal, 0, sizeof(SceNpMatching2MemberBinAttrInternal));
        }
    }
    return ret;
}

/* 0x01C830 */
s32 NP_MATCHING2_NOINLINE npRoomAttachToServer(NpMatching2ServerNode *server, NpMatching2Room *room)
{
    s32 intr = sceKernelCpuSuspendIntr();
    room->server = server;
    room->next = server->roomList;
    server->roomList = room;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x01C87C */
void NP_MATCHING2_NOINLINE npRoomRelease(NpMatching2Room *room)
{
    s32 shouldDestroy = 0;
    s32 intr = sceKernelCpuSuspendIntr();

    room->refCount--;
    if (room->refCount == 0 && room->destroying == 1) {
        shouldDestroy = 1;
    }
    sceKernelCpuResumeIntr(intr);
    if (shouldDestroy) {
        npRoomDestroy(room);
    }
}

/* 0x01C8F0 */
s32 NP_MATCHING2_NOINLINE npRoomGetFromConn(NpMatching2Conn *conn, NpMatching2Room **outRoom)
{
    if (conn->ownerKind != 2) {
        return (s32)0x80550ca7;
    }
    *outRoom = conn->ownerRoom;
    return 0;
}

/* 0x01C91C */
s32 NP_MATCHING2_NOINLINE npRoomFindMemberById(NpMatching2Room *room, SceNpMatching2RoomMemberId memberId,
                                               NpMatching2RoomMember **outMember)
{
    s32 intr = sceKernelCpuSuspendIntr();
    NpMatching2RoomMember *cur = room->memberList;
    s32 ret = 0;

    while (cur != NULL && cur->memberId != memberId) {
        cur = cur->next;
    }
    if (cur != NULL) {
        if (outMember != NULL) {
            *outMember = cur;
        }
    } else {
        ret = (s32)0x80550caa;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

/* 0x01C9AC */
s32 NP_MATCHING2_NOINLINE npRoomSetOwner(NpMatching2Room *room, NpMatching2RoomMember *newOwner)
{
    s32 intr = sceKernelCpuSuspendIntr();
    NpMatching2RoomMember *prevOwner = room->ownerMember;

    room->ownerMember = newOwner;
    prevOwner->flagAttr &= ~0x80000000u;
    newOwner->flagAttr |= 0x80000000u;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x01CA0C */
s32 NP_MATCHING2_NOINLINE npServerCheckEmptyAndFree(NpMatching2ServerNode *server)
{
    s32 canFree = 0;

    npLockAcquire();
    if (server->refCount <= 0) {
        u32 slot;
        canFree = 1;
        for (slot = 0; slot < 15; slot++) {
            if (server->conns[slot] != NULL) {
                canFree = 0;
                break;
            }
        }
        if (canFree) {
            if (npConnCheckQueueBusy(&server->pendingQueue) < 0 ||
                npConnCheckQueueBusy(&server->authWaitQueue) < 0) {
                canFree = 0;
            }
        }
    }
    npLockRelease();

    if (canFree == 1 && server->closing == 1) {
        NpMatching2Context *ctx = server->ctx;
        NpMatching2ServerNode *cur;
        NpMatching2ServerNode *prev = NULL;

        npLockAcquire();
        for (cur = ctx->serverList; cur != NULL && cur != server; cur = cur->next) {
            prev = cur;
        }
        if (cur != NULL) {
            if (prev != NULL) {
                prev->next = server->next;
            } else {
                ctx->serverList = server->next;
            }
            ctx->serverCount--;
        }
        npLockRelease();

        npConnCloseById(ctx->ctxId, server->mainConnId);
        npConnCloseById(ctx->ctxId, server->authConnId);
        npMatching2Free(server);
        return npCtxTryCleanup(ctx);
    }
    return 1;
}
