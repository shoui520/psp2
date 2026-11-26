#include "np_matching2_internal.h"

/*
 * Server management, worker thread, connection state machine, callback/event
 * dispatch, bitfield/group codecs, SSL/TLS verification, and signaling bridge
 * (0x01CB50..0x022457).
 */

typedef struct NpServiceServerEntry {
    const char *host;
    u16 port;
    u16 serverId;
    u32 status;
    struct NpServiceServerEntry *next;
} NpServiceServerEntry;

typedef struct NpWorkerEventNode {
    struct NpWorkerEventNode *next;
    u8 eventKind;
    u8 padByte;
    SceNpMatching2ContextId ctxId;
    SceNpMatching2RoomId roomId;
    SceNpMatching2RoomMemberId memberId;
    SceNpMatching2Event eventId;
    s32 errorCode;
} NpWorkerEventNode;

typedef struct NpWorkerState {
    s32 threadId;
    s32 eventFlagId;
    u8 *sendBuf;
    u32 sendBufSize;
    u8 *recvBuf;
    u32 recvBufSize;
    SceNetInetFdSet activeSockSet;
    u32 threadStackSize;
    s32 threadPriority;
} NpWorkerState;

typedef struct NpPartialRecvState {
    s32 sockFd;
    s32 mode;
    u32 bufferedLen;
    u32 remainingLen;
} NpPartialRecvState;

typedef struct NpRoomErrorEventData {
    s32 errorCode;
    u32 neededBufSize;
    SceNpMatching2Event eventId;
    u16 padWord;
} NpRoomErrorEventData;

typedef struct NpCertLoaderNode {
    const char *pemStr;
    struct NpCertLoaderNode *next;
} NpCertLoaderNode;

typedef void (*NpReqCallbackFn)(SceNpMatching2ContextId, SceNpMatching2RequestId, SceNpMatching2Event, s32, const void *, void *);
typedef void (*NpSigCallbackFn)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2RoomMemberId, SceNpMatching2Event, s32, void *);
typedef void (*NpRoomEvCallbackFn)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2Event, const void *, void *);
typedef void (*NpRoomMsgCallbackFn)(SceNpMatching2ContextId, SceNpMatching2RoomId, SceNpMatching2RoomMemberId, SceNpMatching2Event, const void *, void *);

static s32 g_workerStopFlag = 0;
static const u8 g_keepaliveZeroByte = 0;
static u32 g_compiledSdkVersion = 0;
static NpWorkerState g_worker;
static NpWorkerEventNode *g_workerEventHead = NULL;
static NpWorkerEventNode *g_workerEventTail = NULL;
static NpPartialRecvState g_partialRecv;

static s32 g_localProfileCached = 0;
static NpMatching2UserProfile g_localProfile;

static s32 g_sslInitState = -1;
static void *g_sslCtxHandle = NULL;
static s32 g_sslRootCertCount = 0;
static void *g_sslRootCerts[5];

s32 g_npSignalingInitialized = 0;

/* Forward declarations for internal worker/SSL helpers */
static s32 NP_MATCHING2_NOINLINE npServerCreateNode(const char *host, u32 status, u16 port,
                                                    SceNpMatching2ServerId serverId, NpMatching2ServerNode **outServer);
static s32 NP_MATCHING2_NOINLINE npWorkerRemoveEventNode(NpWorkerEventNode *node);
static s32 NP_MATCHING2_NOINLINE npWorkerMainThread(u32 args, void *argp);
static s32 NP_MATCHING2_NOINLINE npWorkerSendKeepalive(NpMatching2ServerNode *server);
static s32 NP_MATCHING2_NOINLINE npWorkerHandleConnError(NpMatching2Conn *conn, s32 errorCode);
static s32 NP_MATCHING2_NOINLINE npWorkerCheckQueueTimeouts(NpMatching2ServerNode *server, NpMatching2Conn *conn, NpMatching2ReqQueue *queue);
static s32 NP_MATCHING2_NOINLINE npWorkerStepConn(NpMatching2Conn *conn, SceNetInetFdSet *readFds, SceNetInetFdSet *writeFds,
                                                  s32 *activeOut, s32 *readyOut);
static s32 NP_MATCHING2_NOINLINE npWorkerProcessSendQueue(NpMatching2Conn *conn, SceNetInetFdSet *writeFds);
static s32 NP_MATCHING2_NOINLINE npWorkerProcessRecv(NpMatching2Conn *conn, SceNetInetFdSet *readFds);
static s32 NP_MATCHING2_NOINLINE npWorkerRawSend(NpMatching2Conn *conn, const void *buf, s32 len, s32 isProbe);
static s32 NP_MATCHING2_NOINLINE npWorkerRawRecv(NpMatching2Conn *conn, void *buf, s32 len);
static void NP_MATCHING2_NOINLINE npWorkerKeepaliveCb(SceNpMatching2ContextId ctxId, SceNpMatching2RequestId reqId,
                                                      SceNpMatching2Event eventId, s32 errorCode, const void *eventData, void *cbArg);
static s32 NP_MATCHING2_NOINLINE npWorkerCheckIdleMainConn(NpMatching2Conn *conn, s32 *readyInOut);
static s32 NP_MATCHING2_NOINLINE npWorkerAdvanceConnState(NpMatching2Conn *conn, SceNetInetFdSet *readFds, SceNetInetFdSet *writeFds,
                                                          s32 *activeOut, s32 *readyOut);
static s32 NP_MATCHING2_NOINLINE npWorkerSendIdlePing(NpMatching2Conn *conn);
static s32 NP_MATCHING2_NOINLINE npWorkerCheckSockError(NpMatching2Conn *conn, SceNetInetFdSet *readFds, SceNetInetFdSet *writeFds);
static s32 NP_MATCHING2_NOINLINE npSslVerifyCertCb(void *sslHandle, void **certChain, s32 certCount, s32 unusedFlag, s32 *errOut);
static s32 NP_MATCHING2_NOINLINE npSslLoadRootCerts(void);
static s32 NP_MATCHING2_NOINLINE npSslCheckCommonName(void *subjectName, const char *hostName);
static s32 NP_MATCHING2_NOINLINE npSslDecodePem(const char *pemStr, u8 *outBuf, u32 *outLen, u32 outBufSize);

/* 0x01CB50 */
s32 NP_MATCHING2_NOINLINE npWorkerStartContext(NpMatching2Context *ctx)
{
    NpMatching2UserProfile *profile = NULL;
    NpServiceServerEntry *entryList = NULL;
    s32 svcHandle = 0;
    s32 ret;

    ret = npWorkerGetLocalProfile(&profile);
    if (ret < 0) {
        return ret;
    }

    svcHandle = sceNpService_7EF4312E(profile, 20, &ctx->commId);
    if (svcHandle < 0) {
        return svcHandle;
    }

    ret = sceNpService_76867C01(svcHandle, (void **)&entryList);
    if (ret < 0) {
        if (svcHandle > 0) {
            sceNpService_DD8B5E53(svcHandle);
        }
        return ret;
    }

    {
        NpServiceServerEntry *curEntry = entryList;
        while (curEntry != NULL) {
            NpMatching2ServerNode *serverNode = NULL;
            ret = npServerCreateNode(curEntry->host, curEntry->status, curEntry->port, curEntry->serverId, &serverNode);
            if (ret < 0) {
                break;
            }
            if (serverNode == NULL) {
                break;
            }
            npLockAcquire();
            serverNode->ctx = ctx;
            if (ctx->serverList != NULL) {
                NpMatching2ServerNode *tail = ctx->serverList;
                while (tail->next != NULL) {
                    tail = tail->next;
                }
                tail->next = serverNode;
            } else {
                ctx->serverList = serverNode;
            }
            ctx->serverCount++;
            npLockRelease();
            ret = 0;
            curEntry = curEntry->next;
        }
    }

    if (ret < 0) {
        npLockAcquire();
        while (ctx->serverList != NULL) {
            NpMatching2ServerNode *serverNode = ctx->serverList;
            NpMatching2Context *ownerCtx;

            npLockAcquire();
            ctx->serverList = serverNode->next;
            ctx->serverCount--;
            npLockRelease();

            ownerCtx = serverNode->ctx;
            npConnCloseById(ownerCtx->ctxId, serverNode->mainConnId);
            npConnCloseById(ownerCtx->ctxId, serverNode->authConnId);
            npMatching2Free(serverNode);
            npCtxTryCleanup(ownerCtx);
        }
        npLockRelease();
    }

    if (svcHandle > 0) {
        sceNpService_DD8B5E53(svcHandle);
    }
    return ret;
}

/* 0x01CD9C */
s32 NP_MATCHING2_NOINLINE npWorkerCheckContextBusy(NpMatching2Context *ctx)
{
    NpMatching2ServerNode *cur = NULL;
    NpMatching2ServerNode *head;
    s32 ret = 0;

    if (ctx == NULL) {
        return (s32)0x80550ca0;
    }

    npLockAcquire();
    npLockAcquire();
    head = ctx->serverList;
    if (head != NULL) {
        if (npWorkerIsCurrentThread() != 0 || head->closing != 1) {
            cur = head;
            head->refCount++;
        }
    }
    npLockRelease();

    while (cur != NULL) {
        NpMatching2ServerNode *nextServer = NULL;
        NpMatching2ServerNode *cand;

        npLockAcquire();
        cand = cur->next;
        if (cand != NULL) {
            if (npWorkerIsCurrentThread() != 0 || cand->closing != 1) {
                nextServer = cand;
                cand->refCount++;
            }
        }
        npLockRelease();

        ret = npConnCheckServerBusy(ctx, cur);
        if (ret >= 0) {
            ret = npConnCheckQueueBusy(&cur->pendingQueue);
            if (ret >= 0) {
                ret = npConnCheckQueueBusy(&cur->authWaitQueue);
            }
        }

        npLockAcquire();
        cur->refCount--;
        npLockRelease();
        npServerCheckEmptyAndFree(cur);

        if (ret < 0) {
            if (nextServer != NULL) {
                npLockAcquire();
                nextServer->refCount--;
                npLockRelease();
                npServerCheckEmptyAndFree(nextServer);
            }
            break;
        }
        cur = nextServer;
    }
    npLockRelease();
    return ret;
}

/* 0x01CF44 */
s32 NP_MATCHING2_NOINLINE npWorkerStopContextServers(NpMatching2Context *ctx)
{
    NpMatching2ServerNode *cur = NULL;
    NpMatching2ServerNode *head;
    s32 ret;

    if (ctx == NULL) {
        return (s32)0x80550ca0;
    }

    ret = npWorkerCheckContextBusy(ctx);
    if (ret < 0) {
        return ret;
    }

    npLockAcquire();
    npLockAcquire();
    head = ctx->serverList;
    if (head != NULL) {
        if (npWorkerIsCurrentThread() != 0 || head->closing != 1) {
            cur = head;
            head->refCount++;
        }
    }
    npLockRelease();

    while (cur != NULL) {
        NpMatching2ServerNode *nextServer = NULL;
        NpMatching2ServerNode *cand;

        npLockAcquire();
        cand = cur->next;
        if (cand != NULL) {
            if (npWorkerIsCurrentThread() != 0 || cand->closing != 1) {
                nextServer = cand;
                cand->refCount++;
            }
        }
        npLockRelease();

        ret = npConnCloseAllOnServer(ctx, cur);
        if (ret >= 0) {
            cur->closing = 1;
        }

        npLockAcquire();
        cur->refCount--;
        npLockRelease();
        npServerCheckEmptyAndFree(cur);

        if (ret < 0) {
            if (nextServer != NULL) {
                npLockAcquire();
                nextServer->refCount--;
                npLockRelease();
                npServerCheckEmptyAndFree(nextServer);
            }
            break;
        }
        cur = nextServer;
    }
    npLockRelease();
    return ret;
}

/* 0x01D0E0 */
s32 NP_MATCHING2_NOINLINE npWorkerCleanupContext(NpMatching2Context *ctx)
{
    npLockAcquire();
    while (ctx->serverList != NULL) {
        NpMatching2ServerNode *serverNode = ctx->serverList;
        NpMatching2Context *ownerCtx;

        npLockAcquire();
        ctx->serverList = serverNode->next;
        ctx->serverCount--;
        npLockRelease();

        ownerCtx = serverNode->ctx;
        npConnCloseById(ownerCtx->ctxId, serverNode->mainConnId);
        npConnCloseById(ownerCtx->ctxId, serverNode->authConnId);
        npMatching2Free(serverNode);
        npCtxTryCleanup(ownerCtx);
    }
    npLockRelease();
    return 0;
}

/* 0x01D1D8 */
s32 NP_MATCHING2_NOINLINE npServerFindByIdAndRef(NpMatching2Context *ctx, SceNpMatching2ServerId serverId,
                                                 NpMatching2ServerNode **outServer)
{
    NpMatching2ServerNode *cur;
    s32 ret = (s32)0x80550c0a;

    if (ctx == NULL) {
        return (s32)0x80550ca0;
    }

    npLockAcquire();
    for (cur = ctx->serverList; cur != NULL; cur = cur->next) {
        if (cur->serverId == serverId) {
            if (npWorkerIsCurrentThread() == 0 && cur->closing == 1) {
                ret = SCE_NP_MATCHING2_ERROR_DESTROYING;
            } else {
                ret = 0;
                if (outServer != NULL) {
                    *outServer = cur;
                    cur->refCount++;
                }
            }
            break;
        }
    }
    npLockRelease();
    return ret;
}

/* 0x01D2AC */
s32 NP_MATCHING2_NOINLINE npServerRelease(NpMatching2ServerNode *server)
{
    if (server == NULL) {
        return (s32)0x80550ca0;
    }
    npLockAcquire();
    server->refCount--;
    npLockRelease();
    npServerCheckEmptyAndFree(server);
    return 0;
}

/* 0x01D2FC */
NpMatching2ServerNode *NP_MATCHING2_NOINLINE npServerGetFirstRef(NpMatching2Context *ctx)
{
    NpMatching2ServerNode *picked = NULL;

    npLockAcquire();
    if (ctx != NULL && ctx->serverList != NULL) {
        NpMatching2ServerNode *head = ctx->serverList;
        if (npWorkerIsCurrentThread() != 0 || head->closing != 1) {
            picked = head;
            head->refCount++;
        }
    }
    npLockRelease();
    return picked;
}

/* 0x01D380 */
NpMatching2ServerNode *NP_MATCHING2_NOINLINE npServerGetNextRef(NpMatching2ServerNode *server)
{
    NpMatching2ServerNode *picked = NULL;

    npLockAcquire();
    if (server != NULL && server->next != NULL) {
        NpMatching2ServerNode *nextServer = server->next;
        if (npWorkerIsCurrentThread() != 0 || nextServer->closing != 1) {
            picked = nextServer;
            nextServer->refCount++;
        }
    }
    npLockRelease();
    return picked;
}

/* 0x01D404 */
s32 NP_MATCHING2_NOINLINE npServerRecordOwnedRoom(u16 worldNum, NpMatching2ServerNode *server)
{
    s32 intr = sceKernelCpuSuspendIntr();
    server->ownedRoomCount++;
    if (worldNum != 0) {
        server->ownedWorldCounts[worldNum - 1]++;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x01D464 */
s32 NP_MATCHING2_NOINLINE npServerReleaseOwnedRoom(u16 worldNum, NpMatching2ServerNode *server)
{
    s32 intr = sceKernelCpuSuspendIntr();
    server->ownedRoomCount--;
    if (worldNum != 0) {
        server->ownedWorldCounts[worldNum - 1]--;
    }
    server->lastOwnedLeaveTime = sceKernelGetSystemTimeLow();
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x01D4D8 */
static s32 NP_MATCHING2_NOINLINE npServerCreateNode(const char *host, u32 status, u16 port,
                                                    SceNpMatching2ServerId serverId, NpMatching2ServerNode **outServer)
{
    NpMatching2ServerNode *server;
    NpMatching2HostList mainHostList;
    NpMatching2HostList authHostList;
    NpMatching2Conn *conn = NULL;
    s32 ret;

    sceKernelMemset(&mainHostList, 0, sizeof(mainHostList));
    sceKernelMemset(&authHostList, 0, sizeof(authHostList));

    server = (NpMatching2ServerNode *)npMatching2Alloc(sizeof(NpMatching2ServerNode));
    if (server == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(server, 0, sizeof(NpMatching2ServerNode));
    server->serverId = serverId;
    if (status == 2) {
        server->status = 4;
    }

    mainHostList.count = 1;
    mainHostList.entries = (NpMatching2HostEntry *)npMatching2Alloc(sizeof(NpMatching2HostEntry));
    if (mainHostList.entries == NULL) {
        npMatching2Free(server);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(mainHostList.entries, 0, sizeof(NpMatching2HostEntry));
    sceNet_lib_B5CE388A(mainHostList.entries[0].host, host, 128);
    mainHostList.entries[0].port = port;

    authHostList.count = 1;
    authHostList.entries = (NpMatching2HostEntry *)npMatching2Alloc(sizeof(NpMatching2HostEntry));
    if (authHostList.entries == NULL) {
        npMatching2Free(mainHostList.entries);
        npMatching2Free(server);
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(authHostList.entries, 0, sizeof(NpMatching2HostEntry));
    sceNet_lib_B5CE388A(authHostList.entries[0].host, host, 128);
    authHostList.entries[0].port = 0;

    ret = npConnCreate(server, 1, &mainHostList, &conn);
    if (ret < 0) {
        npMatching2Free(mainHostList.entries);
        npMatching2Free(authHostList.entries);
        npMatching2Free(server);
        return ret;
    }
    server->mainConnId = conn->connId;
    conn = NULL;

    ret = npConnCreate(server, 0, &authHostList, &conn);
    if (ret < 0) {
        npConnCloseById(server->ctx->ctxId, server->mainConnId);
        npMatching2Free(authHostList.entries);
        npMatching2Free(server);
        return ret;
    }
    server->authConnId = conn->connId;

    if (outServer != NULL) {
        *outServer = server;
    }
    return 0;
}

/* 0x01D6E8 */
s32 NP_MATCHING2_NOINLINE npWorkerThreadStart(u32 threadStackSize, s32 threadPriority)
{
    NpWorkerState *workerPtr = &g_worker;
    u32 threadOpt[2];
    s32 ret;

    g_compiledSdkVersion = sceKernelGetCompiledSdkVersion();
    g_workerStopFlag = 0;
    sceKernelMemset(&g_partialRecv, 0, sizeof(g_partialRecv));
    sceKernelMemset(&g_worker, 0, sizeof(g_worker));
    g_worker.threadId = -1;
    g_worker.eventFlagId = -1;
    g_worker.activeSockSet.fdsBits[0] = 0;
    g_worker.activeSockSet.fdsBits[7] = 0;
    g_worker.activeSockSet.fdsBits[1] = 0;
    g_worker.activeSockSet.fdsBits[2] = 0;
    g_worker.activeSockSet.fdsBits[3] = 0;
    g_worker.activeSockSet.fdsBits[4] = 0;
    g_worker.activeSockSet.fdsBits[5] = 0;
    g_worker.activeSockSet.fdsBits[6] = 0;
    g_worker.threadStackSize = threadStackSize;
    g_worker.threadPriority = threadPriority;

    g_worker.sendBuf = (u8 *)npMatching2Alloc(0x2000);
    if (g_worker.sendBuf == NULL) {
        npWorkerThreadStop();
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    g_worker.sendBufSize = 0x2000;

    if (g_compiledSdkVersion > 0x0505000Fu) {
        g_worker.recvBuf = (u8 *)npMatching2Alloc(24576);
        if (g_worker.recvBuf == NULL) {
            npWorkerThreadStop();
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        g_worker.recvBufSize = 24576;
    } else {
        g_worker.recvBuf = g_worker.sendBuf;
        g_worker.recvBufSize = 0x2000;
    }

    ret = sceKernelCreateEventFlag("SceNpMatching2", 0, 0, NULL);
    if (ret < 0) {
        npWorkerThreadStop();
        return ret;
    }
    g_worker.eventFlagId = ret;

    sceKernelMemset(threadOpt, 0, sizeof(threadOpt));
    threadOpt[0] = sizeof(threadOpt);
    ret = sceKernelCreateThread("SceNpMatching2", npWorkerMainThread, g_worker.threadPriority,
                                g_worker.threadStackSize, 0, threadOpt);
    if (ret < 0) {
        npWorkerThreadStop();
        return ret;
    }
    g_worker.threadId = ret;

    ret = sceKernelStartThread(g_worker.threadId, sizeof(workerPtr), &workerPtr);
    if (ret < 0) {
        npWorkerThreadStop();
        return ret;
    }
    return 0;
}

/* 0x01D8B4 */
s32 NP_MATCHING2_NOINLINE npWorkerThreadStop(void)
{
    g_workerStopFlag = 1;
    sceKernelSetEventFlag(g_worker.eventFlagId, 1);

    if (g_worker.threadId >= 0) {
        sceKernelWaitThreadEnd(g_worker.threadId, NULL);
        sceKernelDeleteThread(g_worker.threadId);
        sceNetFreeThreadinfo(g_worker.threadId);
        g_worker.threadId = -1;
    }
    if (g_worker.eventFlagId >= 0) {
        sceKernelDeleteEventFlag(g_worker.eventFlagId);
        g_worker.eventFlagId = -1;
    }
    if (g_worker.sendBuf != NULL) {
        npMatching2Free(g_worker.sendBuf);
        g_worker.sendBuf = NULL;
    }
    if (g_compiledSdkVersion > 0x0505000Fu) {
        if (g_worker.recvBuf != NULL) {
            npMatching2Free(g_worker.recvBuf);
            g_worker.recvBuf = NULL;
        }
    } else {
        g_worker.recvBuf = NULL;
    }
    return 0;
}

/* 0x01D998 */
static s32 NP_MATCHING2_NOINLINE npWorkerRemoveEventNode(NpWorkerEventNode *node)
{
    s32 intr;
    NpWorkerEventNode *cur;
    s32 ret = 0;

    if (node == NULL) {
        return (s32)0x80550ca0;
    }

    intr = sceKernelCpuSuspendIntr();
    cur = g_workerEventHead;
    while (cur != NULL && cur != node) {
        cur = cur->next;
    }
    if (cur != NULL) {
        g_workerEventHead = cur->next;
        if (cur == g_workerEventTail) {
            g_workerEventTail = NULL;
        }
        cur->next = NULL;
        npMatching2Free(node);
    } else {
        ret = (s32)0x80550ca0;
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

/* 0x01DA48 */
s32 NP_MATCHING2_NOINLINE npWorkerRegisterSockFd(s32 sockFd)
{
    g_worker.activeSockSet.fdsBits[sockFd >> 5] |= (1u << (sockFd & 31));
    return 0;
}

/* 0x01DA7C */
s32 NP_MATCHING2_NOINLINE npWorkerUnregisterSockFd(s32 sockFd)
{
    g_worker.activeSockSet.fdsBits[sockFd >> 5] &= ~(1u << (sockFd & 31));
    if (g_partialRecv.sockFd == sockFd && g_partialRecv.mode != 0) {
        g_partialRecv.remainingLen = 0;
        g_partialRecv.mode = 0;
    }
    return 0;
}

/* 0x01DADC */
s32 NP_MATCHING2_NOINLINE npWorkerWakeup(void)
{
    sceKernelSetEventFlag(g_worker.eventFlagId, 1);
    return 0;
}

/* 0x01DB04 */
s32 NP_MATCHING2_NOINLINE npWorkerIsCurrentThread(void)
{
    return sceKernelGetThreadId() == g_worker.threadId;
}

/* 0x01DB30 */
s32 NP_MATCHING2_NOINLINE npWorkerPostEvent(u8 eventKind, SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                            SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, s32 errorCode)
{
    NpWorkerEventNode *node;
    s32 intr;

    node = (NpWorkerEventNode *)npMatching2Alloc(sizeof(NpWorkerEventNode));
    if (node == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }

    node->eventKind = eventKind;
    node->ctxId = ctxId;
    node->roomId = roomId;
    node->memberId = memberId;
    node->eventId = eventId;
    node->errorCode = errorCode;

    intr = sceKernelCpuSuspendIntr();
    if (g_workerEventTail != NULL) {
        g_workerEventTail->next = node;
        g_workerEventTail = node;
    } else {
        g_workerEventHead = node;
        g_workerEventTail = node;
    }
    node->next = NULL;
    sceKernelCpuResumeIntr(intr);

    sceKernelSetEventFlag(g_worker.eventFlagId, 1);
    return 0;
}

/* 0x01DC20 */
static s32 NP_MATCHING2_NOINLINE npWorkerMainThread(u32 args, void *argp)
{
    NpWorkerState *worker = *(NpWorkerState **)argp;
    SceNetInetFdSet readFds;
    SceNetInetFdSet writeFds;
    SceNetInetTimeval zeroTv;
    u32 waitTimeout = 100000;
    u32 outBits = 0;
    s32 hadActiveStep = 0;
    s32 hadAnyCtx = 0;
    s32 stepActive = 0;
    s32 stepReady = 0;

    (void)args;
    do {
        s32 waitRet;
        NpWorkerEventNode *evNode;
        NpMatching2Context *ctx;

        sceKernelMemset(&readFds, 0, sizeof(readFds));
        if (sceNet_lib_E0A81C7C(&worker->activeSockSet, &readFds, sizeof(readFds)) != 0 || hadActiveStep == 1) {
            waitTimeout = 100000;
        } else if (g_workerStopFlag != 1 && npCtxHasAnyStarted() == 0) {
            waitTimeout = 0;
        } else {
            waitTimeout = 1000000;
        }

        waitRet = sceKernelWaitEventFlag(worker->eventFlagId, 0xFFFFFFFFu, 33, &outBits,
                                         (waitTimeout != 0) ? &waitTimeout : NULL);
        if (waitRet < 0 && waitRet != (s32)0x800201A8) {
            break;
        }

        sceKernelMemcpy(&readFds, &worker->activeSockSet, sizeof(readFds));
        sceKernelMemcpy(&writeFds, &worker->activeSockSet, sizeof(writeFds));
        sceKernelMemset(&zeroTv, 0, sizeof(zeroTv));
        sceNetInetSelect(npTransportGetMaxSockFdPlusOne(), &readFds, &writeFds, NULL, &zeroTv);

        evNode = g_workerEventHead;
        while (evNode != NULL) {
            NpWorkerEventNode *nextNode = evNode->next;
            if (evNode->eventKind == 1) {
                npWorkerInvokeSignalingCb(evNode->ctxId, evNode->roomId, evNode->memberId,
                                          evNode->eventId, evNode->errorCode);
            }
            npWorkerRemoveEventNode(evNode);
            evNode = nextNode;
        }

        npLockAcquire();
        ctx = npCtxGetFirstRef();
        hadActiveStep = 0;
        hadAnyCtx = 0;
        if (ctx != NULL) {
            hadAnyCtx = 1;
            while (ctx != NULL) {
                NpMatching2Context *nextCtx = npCtxGetNextRef(ctx);
                NpMatching2ServerNode *server = npServerGetFirstRef(ctx);

                while (server != NULL) {
                    NpMatching2ServerNode *nextServer = npServerGetNextRef(server);
                    u16 slot;

                    if (g_partialRecv.mode == 0 || g_compiledSdkVersion > 0x0505000Fu) {
                        NpMatching2Conn *mainConn = NULL;
                        if (npConnFindAndRef(server->ctx->ctxId, server->mainConnId, &mainConn) >= 0) {
                            NpMatching2Req *maintReq = npReqQueueGetFirstRef(&server->pendingQueue);
                            while (maintReq != NULL) {
                                NpMatching2Req *nextReq = npReqQueueGetNextRef(maintReq);
                                s32 writtenLen = 0;
                                s32 rmRet;

                                npTransportBuildReqPacket(mainConn, maintReq, g_worker.sendBuf, g_worker.sendBufSize, &writtenLen);
                                rmRet = npReqRemoveFromQueue(maintReq, &server->pendingQueue, NULL, server);
                                if (rmRet >= 0) {
                                    npReqAbortOnSubmitFail(server->ctx, maintReq, rmRet);
                                }
                                npReqRelease(maintReq);
                                maintReq = nextReq;
                            }
                        }
                        if (mainConn != NULL) {
                            npConnRelease(mainConn);
                        }
                    }

                    npWorkerCheckQueueTimeouts(server, NULL, &server->authWaitQueue);

                    if (server->sessionId != 0) {
                        u64 nowTick = sceKernelGetSystemTimeWide();
                        u32 margin = (server->keepaliveIntervalUsec * 20u) / 100u;
                        u64 remaining;

                        if (server->nextKeepaliveTime >= nowTick) {
                            remaining = server->nextKeepaliveTime - nowTick;
                        } else {
                            remaining = server->nextKeepaliveTime - nowTick - 1ULL;
                        }
                        if (remaining < (u64)margin && server->keepaliveInFlight != 1) {
                            npWorkerSendKeepalive(server);
                        }
                    }

                    for (slot = 0; slot < 15; slot++) {
                        NpMatching2Conn *conn = NULL;
                        if (npConnFindBySlotAndRef(ctx, server, slot, &conn) >= 0 && conn != NULL) {
                            if (conn->closing == 1) {
                                npWorkerHandleConnError(conn, (s32)0x80550c18);
                            } else {
                                npWorkerCheckQueueTimeouts(NULL, conn, &conn->sendQueue);
                                npWorkerCheckQueueTimeouts(NULL, conn, &conn->waitQueue);
                            }
                            if (npWorkerStepConn(conn, &readFds, &writeFds, &stepActive, &stepReady) >= 0) {
                                if (stepActive == 1) {
                                    hadActiveStep = 1;
                                }
                                if (stepReady != 0) {
                                    if ((g_partialRecv.mode != 0 && g_compiledSdkVersion <= 0x0505000Fu) ||
                                        npWorkerProcessSendQueue(conn, &writeFds) >= 0) {
                                        npWorkerProcessRecv(conn, &readFds);
                                    }
                                }
                            }
                            npConnRelease(conn);
                        }
                    }

                    npServerRelease(server);
                    server = nextServer;
                }

                npCtxRelease(ctx);
                ctx = nextCtx;
            }
        }
        npLockRelease();
    } while (g_workerStopFlag == 0 || hadAnyCtx != 0);

    return 0;
}

/* 0x01E1E8 */
static s32 NP_MATCHING2_NOINLINE npWorkerSendKeepalive(NpMatching2ServerNode *server)
{
    NpMatching2Conn *mainConn = NULL;
    NpMatching2Req *keepaliveReq = NULL;
    SceNpMatching2RequestOptParam optParam;
    s32 ret;

    ret = npConnFindAndRef(server->ctx->ctxId, server->mainConnId, &mainConn);
    if (ret >= 0) {
        sceKernelMemset(&optParam, 0, sizeof(optParam));
        optParam.cbFunc = npWorkerKeepaliveCb;
        optParam.cbFuncArg = server;
        optParam.timeout = 60000000;
        ret = npReqCreate(server->ctx->ctxId, 0, 2, 0xA101, 0x1003, NULL, mainConn, &optParam, 0, NULL, &keepaliveReq);
        if (ret >= 0) {
            ret = npReqEnqueue(keepaliveReq, mainConn, &mainConn->sendQueue);
            if (ret >= 0) {
                sceKernelSetEventFlag(g_worker.eventFlagId, 1);
                server->keepaliveInFlight = 1;
                if (mainConn != NULL) {
                    npConnRelease(mainConn);
                }
                return ret;
            }
        }
    }

    if (keepaliveReq != NULL) {
        npReqAbortOnSubmitFail(server->ctx, keepaliveReq, ret);
    }
    if (mainConn != NULL) {
        npConnRelease(mainConn);
    }
    return ret;
}

/* 0x01E314 */
static s32 NP_MATCHING2_NOINLINE npWorkerHandleConnError(NpMatching2Conn *conn, s32 errorCode)
{
    s32 cancelRet = npWorkerCancelConnReqs(conn, errorCode);

    if (conn->ownerKind == 2) {
        NpMatching2Room *room = conn->ownerRoom;
        struct {
            u8 cause;
            u8 padBytes[3];
            s32 errorCode;
            u8 optData[20];
        } destroyEv;

        if (room->meMember == room->ownerMember) {
            npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), conn->server);
            npCoreSubmitRoomInfoInternal(conn->server->ctx->ctxId, room->roomId, NULL, NULL);
        }
        sceKernelMemset(&destroyEv, 0, sizeof(destroyEv));
        destroyEv.cause = 7;
        destroyEv.errorCode = errorCode;
        npWorkerDispatchRoomEvent(0, room, 0x1104, &destroyEv);
    }

    if (conn->connKind == 0) {
        npConnResetTransport(conn);
    } else if (conn->connKind == 1) {
        npConnResetTransport(conn);
        if (conn->closing != 1 && conn->server->ownedRoomCount != 0) {
            npWorkerSendKeepalive(conn->server);
        }
    } else {
        npConnCloseOrDestroy(conn);
    }
    return cancelRet;
}

/* 0x01E444 */
static s32 NP_MATCHING2_NOINLINE npWorkerCheckQueueTimeouts(NpMatching2ServerNode *server, NpMatching2Conn *conn,
                                                            NpMatching2ReqQueue *queue)
{
    NpMatching2Context *ctx = (server != NULL) ? server->ctx : conn->server->ctx;
    NpMatching2Req *req = npReqQueueGetFirstRef(queue);

    while (req != NULL) {
        NpMatching2Req *nextReq = npReqQueueGetNextRef(req);
        s32 failCode = 0;

        if (req->aborted == 1) {
            failCode = (s32)0x80550c18;
        } else if (req->inFlight == 1) {
            failCode = (s32)0x80550c19;
        } else {
            u32 nowLow = sceKernelGetSystemTimeLow();
            u32 elapsed = (nowLow < req->startTimeLow) ? (nowLow - req->startTimeLow - 1u) : (nowLow - req->startTimeLow);
            if (elapsed >= req->timeoutUsec) {
                failCode = (s32)0x80550c14;
            }
        }

        if (failCode != 0) {
            npWorkerDispatchReqCb(req, failCode, NULL);
            if (npReqRemoveFromQueue(req, queue, conn, server) >= 0) {
                npReqAbortOnSubmitFail(ctx, req, failCode);
            }
        }

        npReqRelease(req);
        req = nextReq;
    }
    return 0;
}

/* 0x01E598 */
static s32 NP_MATCHING2_NOINLINE npWorkerStepConn(NpMatching2Conn *conn, SceNetInetFdSet *readFds, SceNetInetFdSet *writeFds,
                                                  s32 *activeOut, s32 *readyOut)
{
    s32 ret;

    *activeOut = 0;
    *readyOut = 0;
    ret = npWorkerAdvanceConnState(conn, readFds, writeFds, activeOut, readyOut);
    if (ret < 0) {
        *activeOut = 0;
        return ret;
    }

    if (*readyOut != 0) {
        u32 nowLow = sceKernelGetSystemTimeLow();
        if (((conn->connKind == 3 || conn->connKind == 4 || conn->connKind == 1)) && conn->connState == 7) {
            u32 elapsed = (nowLow < conn->lastRecvTime) ? (nowLow - conn->lastRecvTime - 1u) : (nowLow - conn->lastRecvTime);
            if (elapsed > 20000000u) {
                npWorkerHandleConnError(conn, (s32)0x80550c15);
                *readyOut = 0;
                *activeOut = 0;
                return (s32)0x80550c15;
            }
        }
        if (*readyOut != 0) {
            ret = npWorkerCheckIdleMainConn(conn, readyOut);
            if (ret < 0) {
                *activeOut = 0;
                return ret;
            }
            if (*readyOut != 0) {
                ret = npWorkerSendIdlePing(conn);
                *activeOut = 0;
                if (ret < 0) {
                    return ret;
                }
            } else {
                *activeOut = 0;
            }
        } else {
            *activeOut = 0;
        }
        return 0;
    }
    return 0;
}

/* 0x01E6C0 */
static s32 NP_MATCHING2_NOINLINE npWorkerProcessSendQueue(NpMatching2Conn *conn, SceNetInetFdSet *writeFds)
{
    s32 sockFailed = 0;
    s32 ret = 0;

    if ((writeFds->fdsBits[conn->sockFd >> 5] & (1u << (conn->sockFd & 31))) != 0) {
        NpMatching2Req *req = npReqQueueGetFirstRef(&conn->sendQueue);
        while (req != NULL) {
            NpMatching2Req *nextReq = npReqQueueGetNextRef(req);
            s32 writtenLen = 0;
            s32 buildRet = npTransportBuildReqPacket(conn, req, g_worker.sendBuf, g_worker.sendBufSize, &writtenLen);

            if (buildRet < 0) {
                npWorkerDispatchReqCb(req, buildRet, NULL);
                ret = npReqRemoveFromQueue(req, &conn->sendQueue, conn, NULL);
                if (ret >= 0) {
                    npReqAbortOnSubmitFail(conn->server->ctx, req, ret);
                }
            } else {
                ret = npWorkerRawSend(conn, g_worker.sendBuf, writtenLen, 0);
                if (ret == (s32)0x80550cb9) {
                    /* Would block; leave in sendQueue and retry next tick */
                } else if (ret < 0) {
                    sockFailed = 1;
                } else {
                    npLockAcquire();
                    ret = npReqRemoveFromQueue(req, &conn->sendQueue, conn, NULL);
                    if (ret >= 0) {
                        ret = npReqEnqueue(req, conn, &conn->waitQueue);
                        if (ret >= 0) {
                            npLockRelease();
                        } else {
                            npLockRelease();
                            npWorkerDispatchReqCb(req, ret, NULL);
                            npReqAbortOnSubmitFail(conn->server->ctx, req, ret);
                        }
                    } else {
                        npLockRelease();
                        npWorkerDispatchReqCb(req, ret, NULL);
                    }
                }
            }

            npReqRelease(req);
            if (sockFailed == 1) {
                if (nextReq != NULL) {
                    npReqRelease(nextReq);
                }
                break;
            }
            req = nextReq;
        }
        if (!sockFailed) {
            ret = 0;
        }
    }

    if (sockFailed == 1) {
        npWorkerHandleConnError(conn, ret);
    }
    return ret;
}

/* 0x01E8F4 */
static s32 NP_MATCHING2_NOINLINE npWorkerProcessRecv(NpMatching2Conn *conn, SceNetInetFdSet *readFds)
{
    u8 *recvDst = g_worker.recvBuf;
    u32 recvCap = g_worker.recvBufSize;
    s32 sockFd = conn->sockFd;
    u32 neededLen = 0;
    u32 consumedLen = 0;
    s32 recvLen;
    u32 totalAvail;
    s32 ret;

    if (g_partialRecv.mode != 0) {
        if (g_partialRecv.sockFd != sockFd) {
            return 0;
        }
        recvCap = g_partialRecv.remainingLen;
        if (g_worker.recvBufSize < recvCap) {
            recvCap = g_worker.recvBufSize;
        }
        recvDst = g_worker.recvBuf + g_partialRecv.bufferedLen;
    }

    if ((readFds->fdsBits[sockFd >> 5] & (1u << (sockFd & 31))) == 0) {
        return 0;
    }

    recvLen = npWorkerRawRecv(conn, recvDst, (s32)recvCap);
    if (recvLen == (s32)0x80550cb9) {
        return 0;
    }
    if (recvLen <= 0) {
        ret = (recvLen == 0) ? (s32)0x80550c0f : recvLen;
        npWorkerHandleConnError(conn, ret);
        return ret;
    }

    totalAvail = (u32)recvLen;
    if (g_partialRecv.mode == 1) {
        if (g_partialRecv.remainingLen == (u32)recvLen) {
            g_partialRecv.remainingLen = 0;
            g_partialRecv.mode = 0;
            return 0;
        }
        if ((u32)recvLen < g_partialRecv.remainingLen) {
            g_partialRecv.remainingLen -= (u32)recvLen;
        }
        return 0;
    }

    if (g_partialRecv.mode == 2) {
        totalAvail = (u32)recvLen + g_partialRecv.bufferedLen;
    }

    ret = npTransportParseFrames(conn, g_worker.recvBuf, totalAvail, g_worker.recvBufSize, &neededLen, &consumedLen);
    if (ret >= 0) {
        if (g_partialRecv.mode != 0) {
            g_partialRecv.remainingLen = 0;
            g_partialRecv.mode = 0;
        }
        return 0;
    }

    if (ret == (s32)0x80550ccd) {
        g_partialRecv.sockFd = conn->sockFd;
        g_partialRecv.mode = 1;
        g_partialRecv.remainingLen = neededLen - (totalAvail - consumedLen);
        g_partialRecv.bufferedLen = 0;
        return ret;
    }

    if (ret == SCE_NP_MATCHING2_ERROR_PACKET_TOO_SHORT) {
        g_partialRecv.sockFd = conn->sockFd;
        g_partialRecv.mode = 2;
        g_partialRecv.remainingLen = neededLen - (totalAvail - consumedLen);
        g_partialRecv.bufferedLen = totalAvail - consumedLen;
        if (consumedLen != 0) {
            sceKernelMemcpy(g_worker.recvBuf, g_worker.recvBuf + consumedLen, totalAvail - consumedLen);
        }
        return ret;
    }

    if (g_partialRecv.mode != 0) {
        g_partialRecv.remainingLen = 0;
        g_partialRecv.mode = 0;
    }
    npWorkerHandleConnError(conn, ret);
    return ret;
}

/* 0x01EB30 */
static s32 NP_MATCHING2_NOINLINE npWorkerRawSend(NpMatching2Conn *conn, const void *buf, s32 len, s32 isProbe)
{
    s32 sent;

    if (conn->connKind != 0) {
        sent = sceNetInetSend(conn->sockFd, buf, (u32)len, 0);
        if (sent < 0) {
            return sceNetInetGetPspError();
        }
    } else {
        sent = npSslWrite(&conn->sslCtx, buf, len, 0);
        if (sent < 0) {
            return sent;
        }
    }

    if (sent > 0) {
        if (conn->connKind == 1 && isProbe == 0) {
            conn->server->lastOwnedLeaveTime = sceKernelGetSystemTimeLow();
        }
        conn->lastSendTime = sceKernelGetSystemTimeLow();
    }
    return sent;
}

/* 0x01EBE4 */
static s32 NP_MATCHING2_NOINLINE npWorkerRawRecv(NpMatching2Conn *conn, void *buf, s32 len)
{
    s32 recvd;

    if (conn->connKind != 0) {
        recvd = sceNetInetRecv(conn->sockFd, buf, (u32)len, 0);
        if (recvd < 0) {
            return sceNetInetGetPspError();
        }
    } else {
        recvd = npSslRead(&conn->sslCtx, buf, len, 0);
        if (recvd < 0) {
            return recvd;
        }
    }

    if (recvd > 0) {
        conn->lastRecvTime = sceKernelGetSystemTimeLow();
    }
    return recvd;
}

/* 0x01EC64 */
static void NP_MATCHING2_NOINLINE npWorkerKeepaliveCb(SceNpMatching2ContextId ctxId, SceNpMatching2RequestId reqId,
                                                      SceNpMatching2Event eventId, s32 errorCode,
                                                      const void *eventData, void *cbArg)
{
    NpMatching2ServerNode *server = (NpMatching2ServerNode *)cbArg;

    (void)ctxId;
    (void)reqId;
    (void)eventId;
    (void)eventData;
    if (errorCode >= 0) {
        server->nextKeepaliveTime = sceKernelGetSystemTimeWide() + (u64)server->keepaliveIntervalUsec;
    }
    server->keepaliveInFlight = 0;
}

/* 0x01ECB4 */
static s32 NP_MATCHING2_NOINLINE npWorkerCheckIdleMainConn(NpMatching2Conn *conn, s32 *readyInOut)
{
    s32 intr;
    s32 ret = 0;

    intr = sceKernelCpuSuspendIntr();
    if (conn->connKind == 1 && conn->server->ownedRoomCount == 0) {
        s32 sendCount = npReqCountQueue(&conn->sendQueue);
        ret = sendCount;
        if (sendCount >= 0) {
            s32 waitCount = npReqCountQueue(&conn->waitQueue);
            ret = waitCount;
            if (waitCount >= 0) {
                u32 nowLow = sceKernelGetSystemTimeLow();
                if (sendCount + waitCount == 0) {
                    u32 lastTime = conn->server->lastOwnedLeaveTime;
                    u32 elapsed = (nowLow < lastTime) ? (nowLow - lastTime - 1u) : (nowLow - lastTime);
                    if (elapsed > 60000000u - 1u) {
                        npConnResetTransport(conn);
                        *readyInOut = 0;
                    }
                }
                ret = 0;
            }
        }
    }
    sceKernelCpuResumeIntr(intr);
    return ret;
}

/* 0x01EDA8 */
static s32 NP_MATCHING2_NOINLINE npWorkerAdvanceConnState(NpMatching2Conn *conn, SceNetInetFdSet *readFds,
                                                          SceNetInetFdSet *writeFds, s32 *activeOut, s32 *readyOut)
{
    s32 ret = 0;
    s32 doneFlag = 0;

    if (conn->connState == 0 || conn->connState == 3) {
        return 0;
    }

    if (conn->connState >= 5) {
        ret = npWorkerCheckSockError(conn, readFds, writeFds);
        if (ret < 0) {
            if (conn->hostList.count == 1) {
                return ret;
            }
            conn->curHostIndex++;
            if (conn->curHostIndex >= conn->hostList.count) {
                conn->curHostIndex = 0;
                npWorkerHandleConnError(conn, ret);
                npTransportSelectHostPort(conn, conn->curHostIndex);
                conn->connState = 0;
                return ret;
            }
            npConnResetTransport(conn);
            npTransportSelectHostPort(conn, conn->curHostIndex);
            *activeOut = 1;
            conn->connState = 1;
            return 0;
        }

        if ((writeFds->fdsBits[conn->sockFd >> 5] & (1u << (conn->sockFd & 31))) == 0) {
            if (conn->sendQueue.head != NULL) {
                *activeOut = 1;
            }
            return 0;
        }
    }

    switch (conn->connState) {
        case 1:
            ret = npTransportResolveHost(conn, &doneFlag, conn->curHostIndex);
            if (ret >= 0) {
                conn->connState = (doneFlag == 1) ? 4 : 2;
                *activeOut = 1;
                return 0;
            }
            if (conn->hostList.count == 1) {
                return ret;
            }
            conn->curHostIndex++;
            if (conn->curHostIndex < conn->hostList.count) {
                npConnResetTransport(conn);
                npTransportSelectHostPort(conn, conn->curHostIndex);
                conn->connState = 1;
                *activeOut = 1;
                return 0;
            }
            conn->curHostIndex = 0;
            npWorkerHandleConnError(conn, ret);
            npTransportSelectHostPort(conn, conn->curHostIndex);
            conn->connState = 0;
            return ret;

        case 2:
            ret = npTransportPollResolver(conn, &doneFlag);
            if (ret < 0) {
                if (conn->hostList.count == 1) {
                    return ret;
                }
                conn->curHostIndex++;
                if (conn->curHostIndex >= conn->hostList.count) {
                    conn->curHostIndex = 0;
                    npWorkerHandleConnError(conn, ret);
                    npTransportSelectHostPort(conn, conn->curHostIndex);
                    conn->connState = 0;
                    return ret;
                }
                npConnResetTransport(conn);
                npTransportSelectHostPort(conn, conn->curHostIndex);
                *activeOut = 1;
                conn->connState = 1;
                return 0;
            }
            if (doneFlag == 1) {
                conn->connState = 4;
            }
            *activeOut = 1;
            return 0;

        case 4:
            ret = npTransportOpenSocket(conn);
            if (ret < 0) {
                if (conn->hostList.count == 1) {
                    return ret;
                }
                conn->curHostIndex++;
                if (conn->curHostIndex >= conn->hostList.count) {
                    conn->curHostIndex = 0;
                    npWorkerHandleConnError(conn, ret);
                    npTransportSelectHostPort(conn, conn->curHostIndex);
                    conn->connState = 0;
                    return ret;
                }
                npConnResetTransport(conn);
                npTransportSelectHostPort(conn, conn->curHostIndex);
                *activeOut = 1;
                conn->connState = 1;
                return 0;
            }
            conn->connState = 5;
            *activeOut = 1;
            return 0;

        case 5: {
            u32 nowLow = sceKernelGetSystemTimeLow();
            conn->lastSendTime = nowLow;
            conn->lastRecvTime = nowLow;
            if (conn->connKind != 0) {
                if (conn->connKind == 1) {
                    conn->server->lastOwnedLeaveTime = sceKernelGetSystemTimeLow();
                }
                npTransportClearNonBlocking(conn->sockFd);
                conn->connState = 7;
                *readyOut = 1;
                return 0;
            }

            conn->sslCtx.sockFd = conn->sockFd;
            {
                u32 idx;
                for (idx = 0; idx < conn->hostList.count; idx++) {
                    conn->sslCtx.hostName = conn->hostList.entries[idx].host;
                    ret = npSslOpenConn(&conn->sslCtx);
                    if (ret == 0) {
                        break;
                    }
                }
            }
            if (ret >= 0) {
                conn->connState = 7;
                *readyOut = 1;
                return 0;
            }
            npWorkerHandleConnError(conn, ret);
            return ret;
        }

        default:
            *readyOut = 1;
            return 0;
    }
}

/* 0x01F098 */
static s32 NP_MATCHING2_NOINLINE npWorkerSendIdlePing(NpMatching2Conn *conn)
{
    switch (conn->connKind) {
        case 1:
        case 3:
        case 4: {
            u32 nowLow = sceKernelGetSystemTimeLow();
            u32 elapsed = (nowLow < conn->lastSendTime) ? (nowLow - conn->lastSendTime - 1u) : (nowLow - conn->lastSendTime);
            if (elapsed > 8000000u - 1u) {
                s32 ret = npWorkerRawSend(conn, &g_keepaliveZeroByte, 1, 1);
                if (ret < 0 && ret != (s32)0x80550cb9) {
                    npWorkerHandleConnError(conn, ret);
                    return ret;
                }
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

/* 0x01F180 */
static s32 NP_MATCHING2_NOINLINE npWorkerCheckSockError(NpMatching2Conn *conn, SceNetInetFdSet *readFds, SceNetInetFdSet *writeFds)
{
    s32 sockFd = conn->sockFd;
    s32 sockErr = 0;
    u32 optLen = 4;

    if (sockFd > 0 &&
        ((readFds->fdsBits[sockFd >> 5] & (1u << (sockFd & 31))) != 0 ||
         (writeFds->fdsBits[sockFd >> 5] & (1u << (sockFd & 31))) != 0)) {
        if (sceNetInetGetsockopt(sockFd, 0xFFFF, 4103, &sockErr, &optLen) < 0) {
            return sceNetInetGetPspError();
        }
        if (sockErr < 0) {
            return sockErr;
        }
    }
    return 0;
}

/* 0x01F230 */
s32 NP_MATCHING2_NOINLINE npWorkerCloseContextRooms(NpMatching2Context *ctx)
{
    NpMatching2ServerNode *server;

    npLockAcquire();
    server = npServerGetFirstRef(ctx);
    while (server != NULL) {
        NpMatching2ServerNode *nextServer = npServerGetNextRef(server);
        NpMatching2Req *req;
        u16 slot;

        req = npReqQueueGetFirstRef(&server->pendingQueue);
        while (req != NULL) {
            NpMatching2Req *nextReq = npReqQueueGetNextRef(req);
            req->inFlight = 1;
            npReqRelease(req);
            req = nextReq;
        }

        req = npReqQueueGetFirstRef(&server->authWaitQueue);
        while (req != NULL) {
            NpMatching2Req *nextReq = npReqQueueGetNextRef(req);
            req->inFlight = 1;
            npReqRelease(req);
            req = nextReq;
        }

        for (slot = 0; slot < 15; slot++) {
            NpMatching2Conn *conn = NULL;
            if (npConnFindBySlotAndRef(ctx, server, slot, &conn) >= 0 && conn != NULL) {
                conn->closing = 1;
                npConnRelease(conn);
            }
        }

        npServerRelease(server);
        server = nextServer;
    }
    npLockRelease();
    return 0;
}

/* 0x01F370 */
s32 NP_MATCHING2_NOINLINE npWorkerDispatchReqCb(NpMatching2Req *req, s32 errorCode, const void *eventData)
{
    NpMatching2Context *ctx = NULL;

    npLockAcquire();
    if ((req->txId >> 28) != 1) {
        if (errorCode < 0) {
            npWorkerHandleReqFailCleanup(req, errorCode);
        }
        if (req != NULL && req->eventId != 0) {
            u16 cmd = req->protoCmd;
            if (cmd < 0x1203u || (cmd >= 0x1206u && (u32)(cmd - 0x120Au) >= 2u)) {
                if (npCtxFindAndRef(req->ctxId, &ctx) >= 0) {
                    if ((req->txId >> 28) == 2) {
                        if (req->hasOptParam == 1 && req->optParam.cbFunc != NULL) {
                            s32 cbErr = (req->reqKind == 12) ? 0 : errorCode;
                            const void *cbData = (req->reqKind == 12) ? NULL : eventData;
                            req->optParam.cbFunc(ctx->ctxId, req->reqId, req->eventId, cbErr, cbData, req->optParam.cbFuncArg);
                        } else if (ctx->listeners[2].inUse == 1 && ctx->listeners[2].callbacks[0] != NULL) {
                            NpReqCallbackFn fn = (NpReqCallbackFn)ctx->listeners[2].callbacks[0];
                            fn(req->ctxId, req->reqId, req->eventId, errorCode, eventData, ctx->listeners[2].callbacks[1]);
                        }
                    } else {
                        NpReqCallbackFn cbFn;
                        void *cbArg;

                        if (ctx->listeners[2].inUse == 1 && ctx->listeners[2].callbacks[0] != NULL) {
                            NpReqCallbackFn fn = (NpReqCallbackFn)ctx->listeners[2].callbacks[0];
                            fn(req->ctxId, req->reqId, req->eventId, errorCode, eventData, ctx->listeners[2].callbacks[1]);
                        }
                        if (req->hasOptParam == 1 && req->optParam.cbFunc != NULL) {
                            cbFn = req->optParam.cbFunc;
                            cbArg = req->optParam.cbFuncArg;
                        } else {
                            cbFn = ctx->defaultReqOpt.cbFunc;
                            cbArg = ctx->defaultReqOpt.cbFuncArg;
                        }
                        if (cbFn != NULL) {
                            s32 cbErr = (req->reqKind == 12) ? 0 : errorCode;
                            const void *cbData = (req->reqKind == 12) ? NULL : eventData;
                            cbFn(ctx->ctxId, req->reqId, req->eventId, cbErr, cbData, cbArg);
                        }
                    }
                }
            }
        }
    }

    if (ctx != NULL) {
        npCtxRelease(ctx);
    }
    npLockRelease();
    return 0;
}

/* 0x01F574 */
s32 NP_MATCHING2_NOINLINE npWorkerInvokeSignalingCb(SceNpMatching2ContextId ctxId, SceNpMatching2RoomId roomId,
                                                    SceNpMatching2RoomMemberId memberId, SceNpMatching2Event eventId, s32 errorCode)
{
    NpMatching2Context *ctx = NULL;
    s32 ret;

    npLockAcquire();
    ret = npCtxFindAndRef(ctxId, &ctx);
    if (ret >= 0) {
        u32 idx;
        if (ctx->sigCb != NULL) {
            ctx->sigCb(ctx->ctxId, roomId, memberId, eventId, errorCode, ctx->sigCbArg);
        }
        for (idx = 0; idx < 3; idx++) {
            if (ctx->listeners[idx].inUse != 0 && ctx->listeners[idx].callbacks[6] != NULL) {
                NpSigCallbackFn fn = (NpSigCallbackFn)ctx->listeners[idx].callbacks[6];
                fn(ctx->ctxId, roomId, memberId, eventId, errorCode, ctx->listeners[idx].callbacks[7]);
            }
        }
    }
    if (ctx != NULL) {
        npCtxRelease(ctx);
    }
    npLockRelease();
    return ret;
}

/* 0x01F6B4 */
s32 NP_MATCHING2_NOINLINE npWorkerDispatchRoomEvent(s32 reqSource, NpMatching2Room *room, SceNpMatching2Event eventId,
                                                    const void *eventData)
{
    NpMatching2Context *ctx = room->server->ctx;

    npLockAcquire();
    if (reqSource == 2) {
        if (ctx->listeners[2].inUse == 1 && ctx->listeners[2].callbacks[2] != NULL) {
            NpRoomEvCallbackFn fn = (NpRoomEvCallbackFn)ctx->listeners[2].callbacks[2];
            fn(ctx->ctxId, room->roomId, eventId, eventData, ctx->listeners[2].callbacks[3]);
        }
    } else {
        u32 idx;
        for (idx = 0; idx < 3; idx++) {
            if (ctx->listeners[idx].inUse != 0 && ctx->listeners[idx].callbacks[2] != NULL) {
                NpRoomEvCallbackFn fn = (NpRoomEvCallbackFn)ctx->listeners[idx].callbacks[2];
                fn(ctx->ctxId, room->roomId, eventId, eventData, ctx->listeners[idx].callbacks[3]);
            }
        }
        if (room->roomEvOpt.cbFunc != NULL) {
            room->roomEvOpt.cbFunc(room->conn->server->ctx->ctxId, room->roomId, eventId, eventData, room->roomEvOpt.cbFuncArg);
        } else if (ctx->defaultRoomEvOpt.cbFunc != NULL) {
            ctx->defaultRoomEvOpt.cbFunc(ctx->ctxId, room->roomId, eventId, eventData, ctx->defaultRoomEvOpt.cbFuncArg);
        }
    }
    npLockRelease();
    return 0;
}

/* 0x01F80C */
s32 NP_MATCHING2_NOINLINE npWorkerDispatchRoomErrorEvent(s32 reqSource, NpMatching2Room *room, s32 errorCode,
                                                         u32 eventId, u32 extraLen)
{
    NpRoomErrorEventData errEv;
    struct {
        u8 cause;
        u8 padBytes[3];
        s32 errorCode;
        u8 optData[20];
    } destroyEv;

    sceKernelMemset(&errEv, 0, sizeof(errEv));
    errEv.errorCode = errorCode;
    errEv.neededBufSize = extraLen;
    errEv.eventId = eventId;
    npWorkerDispatchRoomEvent(reqSource, room, 0x1109, &errEv);

    if (eventId >= 0x1101 && (eventId < 0x1103 || (u32)(eventId - 0x1104) < 4u)) {
        NpMatching2Conn *roomConn = room->conn;
        NpMatching2Req *req = NULL;

        while (npReqPopHead(&roomConn->sendQueue, &req, roomConn, NULL) >= 0 && req != NULL) {
            npWorkerDispatchReqCb(req, errorCode, NULL);
            npReqAbortOnSubmitFail(roomConn->server->ctx, req, errorCode);
            npReqRelease(req);
        }
        while (npReqPopHead(&roomConn->waitQueue, &req, roomConn, NULL) >= 0 && req != NULL) {
            npWorkerDispatchReqCb(req, errorCode, NULL);
            npReqAbortOnSubmitFail(roomConn->server->ctx, req, errorCode);
            npReqRelease(req);
        }

        if (room->meMember == room->ownerMember) {
            npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), roomConn->server);
            npCoreSubmitRoomInfoInternal(roomConn->server->ctx->ctxId, room->roomId, NULL, NULL);
        }

        sceKernelMemset(&destroyEv, 0, sizeof(destroyEv));
        destroyEv.cause = 8;
        destroyEv.errorCode = errorCode;
        npWorkerDispatchRoomEvent(0, room, 0x1104, &destroyEv);
        npConnCloseOrDestroy(roomConn);
    }
    return 0;
}

/* 0x01F9FC */
s32 NP_MATCHING2_NOINLINE npWorkerEncodeRoomGroups(const NpMatching2GroupConfigInput *configs, u32 configCount,
                                                   u8 *groupBitmapOut, NpProtoGroupConfigWire **outGroupWires,
                                                   u32 *outGroupWireCount, u64 *passwordMaskOut)
{
    u32 labeledCount = 0;
    NpProtoGroupConfigWire *wires = NULL;
    NpProtoGroupConfigWire *wireOut;
    u32 curSlot = 1;
    u32 idx;

    sceKernelMemset(groupBitmapOut, 0, 32);
    *outGroupWires = NULL;
    *outGroupWireCount = 0;
    *passwordMaskOut = 0;

    for (idx = 0; idx < configCount; idx++) {
        if (configs[idx].withLabel == 1) {
            labeledCount++;
        }
    }

    if (labeledCount != 0) {
        wires = (NpProtoGroupConfigWire *)npMatching2Alloc(sizeof(NpProtoGroupConfigWire) * labeledCount);
        if (wires == NULL) {
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        sceKernelMemset(wires, 0, sizeof(NpProtoGroupConfigWire) * labeledCount);
    }

    wireOut = wires;
    for (idx = 0; idx < configCount; idx++) {
        u32 groupNum = idx + 1;
        u32 slotNum = configs[idx].slotNum;
        u32 slot;

        if (curSlot + slotNum - 1 >= 65) {
            return (s32)0x80550c0b;
        }

        for (slot = curSlot; slot < curSlot + slotNum; slot++) {
            u32 bitPos = slot * 4;
            u32 remBits = bitPos & 7;
            u32 byteIdx = (remBits == 0) ? ((bitPos >> 3) - 1) : (bitPos >> 3);
            u32 shift = (remBits == 0) ? 0 : 4;

            if (byteIdx >= 32) {
                return (s32)0x80550cad;
            }
            groupBitmapOut[byteIdx] = (u8)((groupBitmapOut[byteIdx] & ~(0xFu << shift)) | ((groupNum & 0xFu) << shift));
        }
        curSlot += slotNum;

        if (wires != NULL && configs[idx].withLabel == 1) {
            wireOut->groupId = (u8)groupNum;
            wireOut->withPassword = 1;
            sceKernelMemcpy(wireOut->label, configs[idx].label, 8);
            wireOut++;
        }

        if (configs[idx].withPassword == 1) {
            *passwordMaskOut |= (1ULL << (63 - idx));
        }
    }

    *outGroupWires = wires;
    *outGroupWireCount = labeledCount;
    return 0;
}

/* 0x01FE10 */
s32 NP_MATCHING2_NOINLINE npWorkerDecodeRoomGroups(const u8 *slotBitmap, const u8 *groupBitmap,
                                                   const NpProtoGroupConfigWire *groupWires, u32 groupWireCount,
                                                   u64 passwordMask, SceNpMatching2RoomGroup **outGroups, u32 *outCount)
{
    u8 zeroBuf[32];
    u16 groupSlotCounts[16];
    u16 groupMemberCounts[16];
    u32 maxGroupId = 0;
    u32 lastGroupId = 0;
    u32 slot;
    SceNpMatching2RoomGroup *groups;
    u32 idx;
    s32 ret;

    sceKernelMemset(zeroBuf, 0, sizeof(zeroBuf));
    if (groupBitmap == NULL || sceNet_lib_E0A81C7C(groupBitmap, zeroBuf, 32) == 0) {
        *outGroups = NULL;
        *outCount = 0;
        return 0;
    }

    sceKernelMemset(groupSlotCounts, 0, 30);
    sceKernelMemset(groupMemberCounts, 0, 30);

    for (slot = 1; slot <= 64; slot++) {
        u32 groupVal = 0;
        u32 occVal = 0;

        ret = npWorkerReadBitField(groupBitmap, 32, 4, slot, &groupVal);
        if (ret < 0) {
            return ret;
        }
        groupVal &= 0xFFu;
        if (groupVal == 0) {
            break;
        }
        if (lastGroupId != groupVal && lastGroupId + 1 != groupVal) {
            return (s32)0x80550ca8;
        }
        lastGroupId = groupVal;
        groupSlotCounts[groupVal - 1]++;
        if (maxGroupId < groupVal) {
            maxGroupId = groupVal;
        }

        ret = npWorkerReadBitField(slotBitmap, 16, 2, slot, &occVal);
        if (ret < 0) {
            return ret;
        }
        if (occVal != 0) {
            groupMemberCounts[groupVal - 1]++;
        }
    }

    groups = (SceNpMatching2RoomGroup *)npMatching2Alloc(sizeof(SceNpMatching2RoomGroup) * maxGroupId);
    if (groups == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(groups, 0, 4);

    for (idx = 0; idx < maxGroupId; idx++) {
        u32 wIdx;
        groups[idx].groupId = (u8)(idx + 1);
        groups[idx].slotNum = groupSlotCounts[idx];
        groups[idx].curGroupMemberNum = groupMemberCounts[idx];

        for (wIdx = 0; wIdx < groupWireCount; wIdx++) {
            if (groupWires[wIdx].groupId == (u8)(idx + 1)) {
                break;
            }
        }
        if (wIdx != groupWireCount && groupWires[wIdx].withPassword != 0) {
            sceKernelMemcpy(groups[idx].label, groupWires[wIdx].label, 8);
            groups[idx].withLabel = 1;
        } else {
            groups[idx].withLabel = 0;
        }
        groups[idx].withPassword = ((passwordMask & (1ULL << (63 - idx))) != 0) ? 1 : 0;
    }

    *outGroups = groups;
    *outCount = maxGroupId;
    return 0;
}

/* 0x020138 */
s32 NP_MATCHING2_NOINLINE npWorkerSyncRoomGroups(const NpProtoGroupConfigWire *groupWires, u32 groupWireCount,
                                                 const u64 *passwordMask, SceNpMatching2RoomGroup *groupList, u32 groupCount,
                                                 SceNpMatching2RoomGroup **updatedOut, u32 *updatedCountInOut)
{
    u32 updatedCount = 0;
    u32 maxUpdated = 0;
    u32 idx;

    if (updatedOut != NULL && updatedCountInOut != NULL) {
        maxUpdated = *updatedCountInOut;
    }

    for (idx = 0; idx < groupCount; idx++) {
        u32 wIdx;
        for (wIdx = 0; wIdx < groupWireCount; wIdx++) {
            if (groupWires[wIdx].groupId == groupList[idx].groupId) {
                break;
            }
        }
        if (wIdx != groupWireCount) {
            if (groupWires[wIdx].withPassword != 0) {
                sceKernelMemcpy(groupList[idx].label, groupWires[wIdx].label, 8);
                groupList[idx].withLabel = 1;
            } else {
                sceKernelMemset(groupList[idx].label, 0, 8);
                groupList[idx].withLabel = 0;
            }
        }
        if (passwordMask != NULL) {
            u32 bitShift = 8u - (u32)groupList[idx].groupId;
            groupList[idx].withPassword = (s32)(((*passwordMask >> bitShift) & 1ULL) != 0);
            if (updatedCount < maxUpdated) {
                updatedOut[updatedCount++] = &groupList[idx];
            }
        }
    }

    if (updatedOut != NULL && updatedCountInOut != NULL) {
        *updatedCountInOut = updatedCount;
    }
    return 0;
}

static s32 npSetTwoBitSlot(u8 *maskOut, SceNpMatching2RoomMemberId memberId)
{
    u32 bitPos = 2u * ((u32)memberId >> 4);
    u32 remBits = bitPos & 7u;
    u32 byteIdx = (remBits == 0) ? ((bitPos >> 3) - 1u) : (bitPos >> 3);
    u32 shift = (remBits == 0) ? 0u : (8u - remBits);
    u32 lowBits = (u32)(memberId & 0xFu);

    if (byteIdx >= 16) {
        return (s32)0x80550cad;
    }
    maskOut[byteIdx] = (u8)((maskOut[byteIdx] & ~(3u << shift)) | ((lowBits << shift) & (3u << shift)));
    return 0;
}

/* 0x020300 */
s32 NP_MATCHING2_NOINLINE npWorkerBuildCastMask(NpMatching2Room *room, u8 castType, const void *targetUnion, u8 *maskOut)
{
    sceKernelMemset(maskOut, 0, 16);

    if (castType == 1) {
        return 0;
    }
    if (castType == 2) {
        const SceNpMatching2RoomMemberId *memberIdPtr = (const SceNpMatching2RoomMemberId *)targetUnion;
        return npSetTwoBitSlot(maskOut, *memberIdPtr);
    }
    if (castType == 3) {
        const struct {
            const SceNpMatching2RoomMemberId *memberId;
            u32 memberIdNum;
        } *multicast = targetUnion;
        u32 idx;
        for (idx = 0; idx < multicast->memberIdNum; idx++) {
            s32 ret = npSetTwoBitSlot(maskOut, multicast->memberId[idx]);
            if (ret < 0) {
                return ret;
            }
        }
        return 0;
    }
    if (castType == 4) {
        u8 teamId = *(const u8 *)targetUnion;
        NpMatching2RoomMember *member;
        for (member = room->memberList; member != NULL; member = member->next) {
            if (member->teamId == teamId) {
                s32 ret = npSetTwoBitSlot(maskOut, member->memberId);
                if (ret < 0) {
                    return ret;
                }
            }
        }
        return 0;
    }
    return (s32)0x80550cac;
}

/* 0x020874 */
s32 NP_MATCHING2_NOINLINE npWorkerConvertUserInfoList(NpProtoUserInfo *srcList, u16 count, SceNpMatching2UserInfoOut **outList)
{
    SceNpMatching2UserInfoOut *dstList;
    SceNpMatching2UserInfoOut *prev = NULL;
    u32 idx;

    if (srcList == NULL) {
        return 0;
    }

    dstList = (SceNpMatching2UserInfoOut *)npMatching2Alloc(sizeof(SceNpMatching2UserInfoOut) * count);
    if (dstList == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(dstList, 0, sizeof(SceNpMatching2UserInfoOut) * count);

    for (idx = 0; idx < count; idx++) {
        NpProtoUserInfo *src = &srcList[idx];
        SceNpMatching2UserInfoOut *dst = &dstList[idx];
        u32 maxUserAttrs = g_npMatching2AttrLimits.maxUserBinAttr;

        sceKernelMemcpy(&dst->npId, &src->npId, 44);
        sceKernelMemset(&src->npId, 0, 44);

        dst->userBinAttr = (SceNpMatching2BinAttr *)npMatching2Alloc(sizeof(SceNpMatching2BinAttr) * maxUserAttrs);
        if (dst->userBinAttr == NULL) {
            npWorkerFreeUserInfoList(dstList, count);
            return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
        }
        dst->userBinAttrNum = maxUserAttrs;
        sceKernelMemset(dst->userBinAttr, 0, sizeof(SceNpMatching2BinAttr) * maxUserAttrs);
        dst->userBinAttr[0].id = 95;

        if (src->userBinAttr.id != 0) {
            u32 attrIdx;
            for (attrIdx = 0; attrIdx < dst->userBinAttrNum; attrIdx++) {
                if (dst->userBinAttr[attrIdx].id == src->userBinAttr.id) {
                    sceKernelMemcpy(&dst->userBinAttr[attrIdx], &src->userBinAttr, sizeof(SceNpMatching2BinAttr));
                    sceKernelMemset(&src->userBinAttr, 0, sizeof(SceNpMatching2BinAttr));
                    break;
                }
            }
        }

        if (src->joinedSessionList != NULL && src->joinedSessionNum != 0) {
            u32 bytes = sizeof(SceNpMatching2JoinedSessionInfo) * src->joinedSessionNum;
            dst->joinedSessionList = (SceNpMatching2JoinedSessionInfo *)npMatching2Alloc(bytes);
            if (dst->joinedSessionList == NULL) {
                npWorkerFreeUserInfoList(dstList, count);
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            dst->joinedSessionNum = src->joinedSessionNum;
            sceKernelMemcpy(dst->joinedSessionList, src->joinedSessionList, bytes);
            sceKernelMemset(src->joinedSessionList, 0, bytes);
        }

        if (prev != NULL) {
            prev->next = dst;
        }
        prev = dst;
    }

    if (outList != NULL) {
        *outList = dstList;
    }
    return 0;
}

/* 0x020AB0 */
s32 NP_MATCHING2_NOINLINE npWorkerFreeUserInfoList(SceNpMatching2UserInfoOut *list, u16 count)
{
    u32 idx;

    if (list == NULL) {
        return 0;
    }
    for (idx = 0; idx < count; idx++) {
        SceNpMatching2UserInfoOut *item = &list[idx];
        if (item->onlineName != NULL) {
            npMatching2Free(item->onlineName);
        }
        if (item->avatarUrl != NULL) {
            npMatching2Free(item->avatarUrl);
        }
        if (item->userBinAttr != NULL) {
            u32 attrIdx;
            for (attrIdx = 0; attrIdx < item->userBinAttrNum; attrIdx++) {
                if (item->userBinAttr[attrIdx].ptr != NULL) {
                    npMatching2Free(item->userBinAttr[attrIdx].ptr);
                }
            }
            npMatching2Free(item->userBinAttr);
        }
        if (item->joinedSessionList != NULL) {
            npMatching2Free(item->joinedSessionList);
        }
    }
    return npMatching2Free(list);
}

/* 0x020BDC */
s32 NP_MATCHING2_NOINLINE npWorkerOnAuthComplete(NpMatching2Context *ctx, NpMatching2Req *req, s32 resultCode)
{
    NpMatching2Conn *authConn = NULL;
    NpMatching2Conn *mainConn = NULL;

    npLockAcquire();
    if (req->protoCmd == 0x1002 && npConnFindAndRef(ctx->ctxId, req->connId, &authConn) >= 0) {
        NpMatching2ServerNode *server = authConn->server;
        server->authInProgress = 0;

        if (resultCode >= 0) {
            if (npConnFindAndRef(ctx->ctxId, server->mainConnId, &mainConn) >= 0) {
                NpMatching2Req *waitReq = npReqQueueGetFirstRef(&server->authWaitQueue);
                while (waitReq != NULL) {
                    NpMatching2Req *nextReq = npReqQueueGetNextRef(waitReq);
                    s32 rmRet = npReqRemoveFromQueue(waitReq, &server->authWaitQueue, NULL, server);
                    if (rmRet < 0) {
                        npWorkerDispatchReqCb(waitReq, rmRet, NULL);
                    } else {
                        s32 enqRet = npReqEnqueue(waitReq, mainConn, &mainConn->sendQueue);
                        if (enqRet < 0) {
                            npWorkerDispatchReqCb(waitReq, enqRet, NULL);
                            npReqAbortOnSubmitFail(server->ctx, waitReq, enqRet);
                        }
                    }
                    npReqRelease(waitReq);
                    waitReq = nextReq;
                }
                npConnRelease(mainConn);
            }
        } else if (npWorkerIsCurrentThread() != 0) {
            NpMatching2Req *waitReq = npReqQueueGetFirstRef(&server->authWaitQueue);
            while (waitReq != NULL) {
                NpMatching2Req *nextReq = npReqQueueGetNextRef(waitReq);
                npWorkerDispatchReqCb(waitReq, resultCode, NULL);
                if (npReqRemoveFromQueue(waitReq, &server->authWaitQueue, NULL, server) >= 0) {
                    npReqAbortOnSubmitFail(server->ctx, waitReq, resultCode);
                }
                npReqRelease(waitReq);
                waitReq = nextReq;
            }
        }
    }
    if (authConn != NULL) {
        npConnRelease(authConn);
    }
    npLockRelease();
    return 0;
}

/* 0x020DDC */
s32 NP_MATCHING2_NOINLINE npWorkerGetLocalProfile(NpMatching2UserProfile **profileOut)
{
    s32 ret = 0;

    if (g_localProfileCached == 0) {
        ret = sceNpGetUserProfile(&g_localProfile);
        if (ret < 0) {
            return ret;
        }
    }
    *profileOut = &g_localProfile;
    return 0;
}

/* 0x020E3C */
s32 NP_MATCHING2_NOINLINE npWorkerCancelConnReqs(NpMatching2Conn *conn, s32 errorCode)
{
    NpMatching2Req *req = NULL;
    s32 ret;

    while (npReqPopHead(&conn->sendQueue, &req, conn, NULL) >= 0 && req != NULL) {
        npWorkerDispatchReqCb(req, errorCode, NULL);
        npReqAbortOnSubmitFail(conn->server->ctx, req, errorCode);
        npReqRelease(req);
    }

    while ((ret = npReqPopHead(&conn->waitQueue, &req, conn, NULL)) >= 0 && req != NULL) {
        npWorkerDispatchReqCb(req, errorCode, NULL);
        npReqAbortOnSubmitFail(conn->server->ctx, req, errorCode);
        npReqRelease(req);
    }
    return ret;
}

/* 0x020F34 */
s32 NP_MATCHING2_NOINLINE npWorkerDispatchRoomMsg(s32 reqSource, NpMatching2Room *room, SceNpMatching2RoomMemberId srcMemberId,
                                                  SceNpMatching2Event eventId, const void *eventData)
{
    NpMatching2Context *ctx = room->server->ctx;

    if (reqSource == 1) {
        npLockRelease();
        return 0;
    }

    npLockAcquire();
    if (reqSource == 2) {
        if (ctx->listeners[2].inUse == 1 && ctx->listeners[2].callbacks[4] != NULL) {
            NpRoomMsgCallbackFn fn = (NpRoomMsgCallbackFn)ctx->listeners[2].callbacks[4];
            fn(ctx->ctxId, room->roomId, srcMemberId, eventId, eventData, ctx->listeners[2].callbacks[5]);
        }
    } else if (room->roomMsgOpt.cbFunc != NULL) {
        room->roomMsgOpt.cbFunc(room->conn->server->ctx->ctxId, room->roomId, srcMemberId,
                                eventId, eventData, room->roomMsgOpt.cbFuncArg);
    } else if (ctx->defaultRoomMsgOpt.cbFunc != NULL) {
        ctx->defaultRoomMsgOpt.cbFunc(ctx->ctxId, room->roomId, srcMemberId,
                                      eventId, eventData, ctx->defaultRoomMsgOpt.cbFuncArg);
    }
    npLockRelease();
    return 0;
}

/* 0x021054 */
s32 NP_MATCHING2_NOINLINE npWorkerBuildPasswordMask(const SceNpMatching2RoomGroup *groups, u32 groupCount,
                                                    const NpMatching2PasswordConfigInput *configs, u32 configCount,
                                                    u64 *maskOut)
{
    u64 mask = 0;
    u32 idx;

    for (idx = 0; idx < groupCount; idx++) {
        if (groups[idx].withPassword != 0) {
            mask |= (u64)(s64)(1 << (8 - groups[idx].groupId));
        }
    }
    for (idx = 0; idx < configCount; idx++) {
        s32 bit = 1 << (8 - configs[idx].groupId);
        if (configs[idx].withPassword == 1) {
            mask |= (u64)(s64)bit;
        } else {
            mask &= (u64)(s64)(~bit);
        }
    }
    *maskOut = mask;
    return 0;
}

/* 0x021140 */
s32 NP_MATCHING2_NOINLINE npWorkerFreeBinAttrArray(SceNpMatching2BinAttr *attrs, u32 count)
{
    u32 idx;

    if (attrs != NULL) {
        for (idx = 0; idx < count; idx++) {
            if (attrs[idx].ptr != NULL) {
                npMatching2Free(attrs[idx].ptr);
            }
        }
        npMatching2Free(attrs);
    }
    return 0;
}

/* 0x0211C8 */
s32 NP_MATCHING2_NOINLINE npWorkerCountActiveSlots(const u8 *slotBitmap, u32 *countOut)
{
    u32 slot;
    u32 val = 0;
    s32 ret = 0;

    *countOut = 0;
    for (slot = 1; slot <= 64; slot++) {
        ret = npWorkerReadBitField(slotBitmap, 16, 2, slot, &val);
        if (ret < 0) {
            break;
        }
        if (val != 0) {
            (*countOut)++;
        }
    }
    return ret;
}

/* 0x021250 */
s32 NP_MATCHING2_NOINLINE npWorkerDecodeCastTarget(u8 castType, NpMatching2Room *room, const u8 *castMask,
                                                   void *targetOut, s32 *allocatedOut)
{
    if (castType == 3) {
        struct {
            SceNpMatching2RoomMemberId *memberId;
            u32 memberIdNum;
        } *multicast = targetOut;
        s32 ret = npWorkerExpandMemberBitmask(room, castMask, &multicast->memberId, &multicast->memberIdNum);
        if (ret >= 0) {
            *allocatedOut = 1;
        }
        return ret;
    }
    if (castType == 4) {
        *(u8 *)targetOut = room->meMember->teamId;
    } else if (castType == 2) {
        *(SceNpMatching2RoomMemberId *)targetOut = room->meMember->memberId;
    }
    return 0;
}

/* 0x0212EC */
void NP_MATCHING2_NOINLINE npWorkerHandleReqFailCleanup(NpMatching2Req *req, s32 errorCode)
{
    if (req == NULL) {
        return;
    }
    if (req->reqKind == 10) {
        if (req->protoCmd == 0x1203) {
            npRoomAbortJoinOnError(req, errorCode, NULL);
        }
    } else if (req->reqKind == 12 && req->protoCmd == 0x3202) {
        NpMatching2Conn *roomConn = NULL;
        if (npConnFindAndRef(req->ctxId, req->connId, &roomConn) >= 0) {
            NpMatching2Room *room = roomConn->ownerRoom;
            if (room != NULL && room->meMember == room->ownerMember) {
                npServerReleaseOwnedRoom((u16)((room->roomId >> 32) & 0xFFFFu), roomConn->server);
                if ((s32)room->flagAttr >= 0 || room->memberCount == 1) {
                    SceNpMatching2RequestOptParam optParam;
                    sceKernelMemset(&optParam, 0, sizeof(optParam));
                    optParam.timeout = 30000000;
                    npCoreSubmitRoomInfoInternal(roomConn->server->ctx->ctxId, room->roomId, &optParam, NULL);
                }
            }
            npConnRelease(roomConn);
        }
    }
}

/* 0x021410 */
s32 NP_MATCHING2_NOINLINE npWorkerReadBitField(const u8 *bitmap, u32 bitmapBytes, u32 fieldBits,
                                               u32 oneBasedIndex, u32 *valueOut)
{
    u32 bitPos;
    u32 remBits;
    u32 byteIdx;
    u32 shift;
    u32 lowBitsCount;
    u32 mask;

    if (oneBasedIndex == 0) {
        return (s32)0x80550cad;
    }
    bitPos = oneBasedIndex * fieldBits;
    remBits = bitPos & 7u;
    byteIdx = (remBits == 0) ? ((bitPos >> 3) - 1u) : (bitPos >> 3);
    if (byteIdx >= bitmapBytes) {
        return (s32)0x80550cad;
    }

    shift = (remBits == 0) ? 0u : (8u - remBits);
    *valueOut = 0;
    lowBitsCount = (8u - shift < fieldBits) ? (8u - shift) : fieldBits;
    mask = (1u << lowBitsCount) - 1u;
    *valueOut |= ((u32)bitmap[byteIdx] >> shift) & mask;

    if (shift + fieldBits > 8u) {
        u32 highMask = ((1u << fieldBits) - 1u) & ~((1u << (8u - shift)) - 1u);
        *valueOut |= ((u32)bitmap[byteIdx - 1] << (8u - shift)) & highMask;
    }
    return 0;
}

/* 0x0214FC */
s32 NP_MATCHING2_NOINLINE npWorkerExpandMemberBitmask(NpMatching2Room *room, const u8 *castMask,
                                                      SceNpMatching2RoomMemberId **listOut, u32 *countOut)
{
    u32 activeCount = 0;
    u32 slot;
    u32 val = 0;
    SceNpMatching2RoomMemberId *memberIds;
    u32 matchedCount = 0;
    s32 ret;

    for (slot = 1; slot <= 64; slot++) {
        ret = npWorkerReadBitField(castMask, 16, 2, slot, &val);
        if (ret < 0) {
            return ret;
        }
        if (val != 0) {
            activeCount++;
        }
    }

    memberIds = (SceNpMatching2RoomMemberId *)npMatching2Alloc(sizeof(SceNpMatching2RoomMemberId) * activeCount);
    if (memberIds == NULL) {
        return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }
    sceKernelMemset(memberIds, 0, sizeof(SceNpMatching2RoomMemberId) * activeCount);

    for (slot = 1; slot <= 64; slot++) {
        ret = npWorkerReadBitField(castMask, 16, 2, slot, &val);
        if (ret < 0) {
            return ret;
        }
        if (val != 0) {
            NpMatching2RoomMember *member;
            for (member = room->memberList; member != NULL; member = member->next) {
                if (((u32)member->memberId >> 4) == slot) {
                    memberIds[matchedCount++] = member->memberId;
                    break;
                }
            }
        }
    }

    *listOut = memberIds;
    *countOut = matchedCount;
    return 0;
}

/* 0x021668 */
s32 NP_MATCHING2_NOINLINE npSslInitGlobal(void)
{
    void *method;
    s32 ret;

    if (g_sslInitState >= 0) {
        return (s32)0x80550cb1;
    }

    method = sceSsl_lib_B40D11EA();
    g_sslCtxHandle = sceSsl_lib_FB8273FE(method);
    if (g_sslCtxHandle != NULL) {
        sceSsl_lib_B4D78E98(g_sslCtxHandle, 32, 0x2000FFFFu, NULL);
        sceSsl_lib_AEBF278B(g_sslCtxHandle, 5, NULL);
        sceSsl_lib_529A9477(g_sslCtxHandle, npSslVerifyCertCb, NULL);
        sceSsl_lib_BF55C31C(g_sslCtxHandle, 0);
        ret = npSslLoadRootCerts();
        if (ret >= 0) {
            g_sslInitState = 1;
            return 0;
        }
    } else {
        ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }

    if (g_sslCtxHandle != NULL) {
        sceSsl_lib_588F2FE8(g_sslCtxHandle);
        g_sslCtxHandle = NULL;
    }
    return ret;
}

/* 0x021758 */
s32 NP_MATCHING2_NOINLINE npSslOpenConn(NpMatching2SslCtx *sslCtx)
{
    void *bio;

    sslCtx->sslHandle = sceSsl_lib_EBFB0E3C(g_sslCtxHandle);
    if (sslCtx->sslHandle != NULL) {
        bio = sceSsl_lib_28B4DE33(sslCtx->sockFd, 0);
        if (bio != NULL) {
            sceSsl_lib_B9C8CCE6(sslCtx->sslHandle, bio, bio);
            sceSsl_lib_ECE07B61(bio, sslCtx);
            sceSsl_lib_80608663(sslCtx->sslHandle);
            return 0;
        }
    }

    if (sslCtx->sslHandle != NULL) {
        sceSsl_lib_84833472(sslCtx->sslHandle);
        sslCtx->sslHandle = NULL;
    }
    return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
}

/* 0x0217F8 */
s32 NP_MATCHING2_NOINLINE npSslWrite(NpMatching2SslCtx *sslCtx, const void *buf, s32 len, s32 flags)
{
    s32 ret;
    s32 sslErr;

    (void)flags;
    sslCtx->lastError = 0;
    ret = sceSsl_lib_B7CA8717(sslCtx->sslHandle, buf, len);
    sslErr = sceSsl_lib_B3B04C58(sslCtx->sslHandle, ret);
    switch ((u32)sslErr) {
        case 1:
            return (sslCtx->lastError != 0) ? sslCtx->lastError : (s32)0x80550c11;
        case 5:
            if (sceNetInetGetPspError() == (s32)0x8001000B) {
                sslCtx->lastError = (s32)0x80550cb9;
            }
            return (sslCtx->lastError != 0) ? sslCtx->lastError : (s32)0x80550c12;
        default:
            return ret;
    }
}

/* 0x0218AC */
s32 NP_MATCHING2_NOINLINE npSslRead(NpMatching2SslCtx *sslCtx, void *buf, s32 len, s32 flags)
{
    s32 ret;
    s32 sslErr;

    (void)flags;
    sslCtx->lastError = 0;
    ret = sceSsl_lib_E7C29542(sslCtx->sslHandle, buf, len);
    sslErr = sceSsl_lib_B3B04C58(sslCtx->sslHandle, ret);
    switch ((u32)sslErr) {
        case 1:
            return (sslCtx->lastError != 0) ? sslCtx->lastError : (s32)0x80550c11;
        case 5:
            if (sceNetInetGetPspError() == (s32)0x8001000B) {
                sslCtx->lastError = (s32)0x80550cb9;
            }
            return (sslCtx->lastError != 0) ? sslCtx->lastError : (s32)0x80550c13;
        default:
            return ret;
    }
}

/* 0x021960 */
s32 NP_MATCHING2_NOINLINE npSslTermGlobal(void)
{
    if (g_sslInitState >= 0) {
        s32 idx;
        for (idx = 0; idx < 5; idx++) {
            if (g_sslRootCerts[idx] != NULL) {
                sceSsl_lib_C1A2E14B(g_sslRootCerts[idx]);
                g_sslRootCerts[idx] = NULL;
            }
        }
        g_sslRootCertCount = 0;
        if (g_sslCtxHandle != NULL) {
            sceSsl_lib_588F2FE8(g_sslCtxHandle);
            g_sslCtxHandle = NULL;
        }
        g_sslInitState = -1;
    }
    return 0;
}

/* 0x0219FC */
static s32 NP_MATCHING2_NOINLINE npSslVerifyCertCb(void *sslHandle, void **certChain, s32 certCount,
                                                   s32 unusedFlag, s32 *errOut)
{
    void *bio = sceSsl_lib_2E2E2B09(sslHandle);
    NpMatching2SslCtx *sslCtx = (NpMatching2SslCtx *)sceSsl_lib_EC4A18D7(bio);
    void *leafSubject;
    s32 idx;
    s32 chainIdx;

    (void)unusedFlag;
    if (g_sslInitState < 0) {
        sslCtx->lastError = (s32)0x80550cb1;
        return 0;
    }
    if (certCount == 0) {
        sslCtx->lastError = (s32)0x80550cb2;
        return 0;
    }

    for (idx = 0; idx < certCount; idx++) {
        u8 serialType[4];
        void *serialBuf = NULL;
        s32 serialLen = 0;
        u32 notBefore[8];
        s32 validCount = 0;

        if (sceSsl_lib_9299C6CE(certChain[idx], serialType, &serialBuf, &serialLen) != 0) {
            if (serialBuf != NULL) {
                sceSsl_lib_5E5C873A(serialBuf);
                serialBuf = NULL;
            }
        }
        if (sceSsl_lib_4BE9030A(certChain[idx]) == NULL) {
            *errOut = 2;
        }
        sceSsl_lib_37C7B76C(certChain[idx], notBefore, &validCount, errOut);
    }

    leafSubject = sceSsl_lib_12C7718F(certChain[0]);
    if (leafSubject != NULL && npSslCheckCommonName(leafSubject, sslCtx->hostName) < 0) {
        sslCtx->lastError = (s32)0x80550cb3;
        return 0;
    }

    for (chainIdx = 0; ; chainIdx++) {
        void *issuerName = sceSsl_lib_4BE9030A(certChain[chainIdx]);
        s32 rootIdx;

        for (rootIdx = 0; rootIdx < g_sslRootCertCount; rootIdx++) {
            void *rootIssuer = sceSsl_lib_4BE9030A(g_sslRootCerts[rootIdx]);
            if (issuerName != NULL && rootIssuer != NULL &&
                sceSsl_lib_7D0E41F4(rootIssuer, issuerName) == 0) {
                void *rootPubKey = sceSsl_lib_B87FFE37(g_sslRootCerts[rootIdx]);
                if (rootPubKey == NULL) {
                    sslCtx->lastError = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                    *errOut = 17;
                    return 0;
                }
                if (sceSsl_lib_0FF7BA68(certChain[chainIdx], rootPubKey) == 0) {
                    sslCtx->lastError = (s32)0x80550c10;
                    return 0;
                }
                while (chainIdx > 0) {
                    void *parentPubKey = sceSsl_lib_B87FFE37(certChain[chainIdx]);
                    if (parentPubKey == NULL) {
                        sslCtx->lastError = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                        *errOut = 17;
                        return 0;
                    }
                    chainIdx--;
                    if (sceSsl_lib_0FF7BA68(certChain[chainIdx], parentPubKey) == 0) {
                        return 0;
                    }
                }
                *errOut = 0;
                return 1;
            }
        }

        if (chainIdx + 1 >= certCount) {
            sslCtx->lastError = (s32)0x80550cb4;
            return 0;
        }
        {
            void *nextSubject = sceSsl_lib_12C7718F(certChain[chainIdx + 1]);
            if (issuerName == NULL || nextSubject == NULL ||
                sceSsl_lib_7D0E41F4(issuerName, nextSubject) != 0) {
                sslCtx->lastError = (s32)0x80550cb5;
                return 0;
            }
        }
    }
}

/* 0x021D38 */
static s32 NP_MATCHING2_NOINLINE npSslLoadRootCerts(void)
{
    void *certBuf = NULL;
    u8 *derBuf = NULL;
    NpCertLoaderNode *certList = NULL;
    u32 requiredSize = 0;
    u32 loadedSize = 0;
    s32 ret;
    s32 idx;

    ret = sceCertLoader_DD629A24(3, 31, NULL, NULL, 0, &requiredSize);
    if (ret >= 0) {
        if (requiredSize != 7100) {
            ret = (s32)0x80550cb8;
        } else {
            certBuf = sceSsl_lib_0861D934(7100);
            if (certBuf == NULL) {
                ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            } else {
                ret = sceCertLoader_DD629A24(3, 31, &certList, certBuf, requiredSize, &loadedSize);
                if (ret >= 0 && certList != NULL) {
                    if (ret != 5) {
                        ret = (s32)0x80550cb7;
                    } else {
                        NpCertLoaderNode *cur = certList;
                        s32 outIdx = 0;

                        g_sslRootCertCount = 5;
                        for (idx = 0; idx < 5; idx++) {
                            if (g_sslRootCerts[idx] != NULL) {
                                sceSsl_lib_C1A2E14B(g_sslRootCerts[idx]);
                                g_sslRootCerts[idx] = NULL;
                            }
                        }
                        sceKernelMemset(g_sslRootCerts, 0, sizeof(void *) * (u32)g_sslRootCertCount);

                        while (cur != NULL) {
                            const u8 *derCursor;
                            void *parsedCert;

                            ret = npSslDecodePem(cur->pemStr, NULL, &requiredSize, 0);
                            if (ret < 0) {
                                break;
                            }
                            derBuf = (u8 *)sceSsl_lib_0861D934(requiredSize);
                            if (derBuf == NULL) {
                                ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                                break;
                            }
                            ret = npSslDecodePem(cur->pemStr, derBuf, &loadedSize, requiredSize);
                            if (ret < 0) {
                                break;
                            }
                            derCursor = derBuf;
                            parsedCert = sceSsl_lib_6038DEDE(NULL, &derCursor, (s32)loadedSize);
                            if (parsedCert == NULL) {
                                ret = SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
                                break;
                            }
                            sceSsl_lib_5E5C873A(derBuf);
                            derBuf = NULL;
                            g_sslRootCerts[outIdx++] = parsedCert;
                            cur = cur->next;
                        }
                        if (cur == NULL) {
                            sceSsl_lib_5E5C873A(certBuf);
                            return 0;
                        }
                    }
                }
            }
        }
    }

    for (idx = 0; idx < 5; idx++) {
        if (g_sslRootCerts[idx] != NULL) {
            sceSsl_lib_C1A2E14B(g_sslRootCerts[idx]);
            g_sslRootCerts[idx] = NULL;
        }
    }
    if (certBuf != NULL) {
        sceSsl_lib_5E5C873A(certBuf);
    }
    if (derBuf != NULL) {
        sceSsl_lib_5E5C873A(derBuf);
    }
    return ret;
}

/* 0x021F8C */
static s32 NP_MATCHING2_NOINLINE npSslCheckCommonName(void *subjectName, const char *hostName)
{
    s32 matched = 0;
    s32 entryCount = sceSsl_lib_4C75B8E9(subjectName);
    s32 idx;

    if (g_sslInitState < 0) {
        return (s32)0x80550cb1;
    }

    for (idx = 0; idx < entryCount; idx++) {
        void *entry = sceSsl_lib_5040E836(subjectName, idx);
        void *oidPtr = NULL;
        s32 oidLen = 0;
        u8 valType[4];
        const void *valPtr = NULL;
        s32 valLen = 0;

        if (sceSsl_lib_8D484361(entry, 0, &oidPtr, &oidLen, valType, &valPtr, &valLen) != 0) {
            const char *oidName = sceSsl_lib_2B7BD166(oidPtr, oidLen, 0);
            if (oidName == NULL) {
                oidName = "<UNKNOWN>";
            }
            if (sceNet_lib_A0F16ABD(oidName, "commonName") == 0 && valLen < 255) {
                char cnBuf[256];
                char *cnDomain;
                char *hostDomain;

                sceKernelMemcpy(cnBuf, valPtr, (u32)valLen);
                cnBuf[valLen] = '\0';
                cnDomain = sceNet_lib_BCBE14CF(cnBuf, '.');
                hostDomain = sceNet_lib_BCBE14CF(hostName, '.');
                if (sceNet_lib_B9085A96(cnDomain, hostDomain, (u32)(valLen - (s32)(cnDomain - cnBuf))) == 0 &&
                    sceNet_lib_D8722983(cnDomain) == sceNet_lib_D8722983(hostDomain)) {
                    matched = 1;
                }
            }
        }
    }
    return matched ? 0 : -1;
}

/* 0x022134 */
static s32 NP_MATCHING2_NOINLINE npSslDecodePem(const char *pemStr, u8 *outBuf, u32 *outLen, u32 outBufSize)
{
    s32 writeOutput = (outBuf != NULL);
    u32 totalOut = 0;
    const char *dashStart;
    const char *bodyStart;
    const char *bodyEnd;
    const char *cursor;

    dashStart = sceNet_lib_BCBE14CF(pemStr, '-');
    if (dashStart == NULL) {
        return (s32)0x80550cb6;
    }
    bodyStart = sceNet_lib_BCBE14CF(dashStart, '\n');
    if (bodyStart == NULL) {
        return (s32)0x80550cb6;
    }
    bodyEnd = sceNet_lib_BCBE14CF(bodyStart, '-');
    if (bodyEnd == NULL) {
        return (s32)0x80550cb6;
    }

    cursor = bodyStart + 1;
    while (cursor < bodyEnd) {
        const char *lfPos = sceNet_lib_BCBE14CF(cursor, '\n');
        const char *crPos;
        u32 lineLen;
        u32 decodedLen;

        if (lfPos == NULL) {
            return (s32)0x80550cb6;
        }
        crPos = sceNet_lib_BCBE14CF(cursor, '\r');
        if (crPos != NULL && crPos < lfPos) {
            lineLen = (u32)(lfPos - cursor - 1);
        } else {
            lineLen = (u32)(lfPos - cursor);
        }
        decodedLen = 3u * (lineLen >> 2);
        if (writeOutput) {
            s32 decRet;
            if (outBufSize < totalOut + decodedLen) {
                return SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
            }
            decRet = npMatching2Base64Decode((char *)(outBuf + totalOut), (const u8 *)cursor, (s32)lineLen);
            if (decRet < 0) {
                return decRet;
            }
            decodedLen = (u32)decRet;
        }
        totalOut += decodedLen;
        cursor = lfPos + 1;
    }

    if (outLen != NULL) {
        *outLen = writeOutput ? totalOut : (totalOut + 1);
    }
    return 0;
}

/* 0x0222D0 */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeInit(u32 stackSize, s32 threadPriority)
{
    s32 ret = (s32)0x80550e02;

    if (g_npSignalingInitialized <= 0) {
        ret = npSigInit(stackSize, threadPriority);
        if (ret >= 0) {
            ret = npSigInitContextManager();
            if (ret >= 0) {
                g_npSignalingInitialized = 1;
            }
        }
    }
    return ret;
}

/* 0x022330 */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeTerm(void)
{
    s32 ret = (s32)0x80550e01;

    if (g_npSignalingInitialized != 0) {
        npSigTermContextManager();
        ret = npSigTerm();
        if (ret >= 0) {
            g_npSignalingInitialized = 0;
        }
    }
    return ret;
}

/* 0x022388 */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeAddContext(const SceNpId *npId, SceNpMatching2ContextId ctxId)
{
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigAddContext(npId, ctxId);
}

/* 0x0223BC */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeRemoveContext(SceNpMatching2ContextId ctxId)
{
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigRemoveContext(ctxId);
}

/* 0x0223F0 */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeStartContext(SceNpMatching2ContextId ctxId)
{
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigStartContext(ctxId);
}

/* 0x022424 */
s32 NP_MATCHING2_NOINLINE npSignalingBridgeStopContext(SceNpMatching2ContextId ctxId)
{
    if (g_npSignalingInitialized == 0) {
        return (s32)0x80550e01;
    }
    return npSigStopContext(ctxId);
}
