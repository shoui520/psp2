/*
 * SceShell musiccore_plugin - StateManager (playback state command queue & decoder callbacks).
 * Original address range (3.65-CEX): 0x811E2130 - 0x811E4670
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

#include "musiccore_audio_output.h"
#include "musiccore_decoder_manager.h"
#include "musiccore_heap.h"
#include "musiccore_queue.h"
#include "musiccore_state_machine.h"
#include "musiccore_state_manager.h"

extern "C" {
void *sce_paf_memset(void *dst, int val, size_t size);
void *sce_paf_memcpy(void *dst, const void *src, size_t size);
int sceKernelDeleteSema(SceUID semaId);
int sceKernelDeleteThread(SceUID threadId);
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
int sceKernelSignalSema(SceUID semaId, int signalCount);
}

static const int MUSICCORE_ERR_FAILED = -1879113726;        /* 0x8FFF0002 */
static const int MUSICCORE_ERR_QUEUE_RETRY = -1879113725;   /* 0x8FFF0003 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;     /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723;   /* 0x8FFF0005 */
static const int MUSICCORE_ERR_QUEUE_TIMEOUT = -1879113714; /* 0x8FFF000E */

namespace sce {
namespace MusicCore {

static void initDefaultMediaSnapshot(StateMediaSnapshot *st)
{
    sce_paf_memset(st, 0, sizeof(*st));
    st->playlistId = -1;
    st->trackId = -1;
    st->trackItemId = -1;
    st->trackOrderIndex = -2;
    st->positionMs = -1;
    st->currentTrackNum = -1;
    st->totalTracks = -1;
    st->seekMode = -1;
    st->speedMode = -1;
    st->volumeMode = 1;
    st->audioBalance = 0x8000;
    st->targetState = 7;
    st->pendingFadeMs = -1;
    st->playParam = -1;
}

static void initDefaultCmdPayload(StateCmdPayload *req)
{
    sce_paf_memset(req, 0, sizeof(*req));
    initDefaultMediaSnapshot(&req->media);
    req->seekTarget = -1;
    req->seekVolumeMode = (int)0x80000000u;
    req->seekMode = -2;
    req->seekSpeedMode = -2;
    req->seekStep = -2;
}

static uint64_t makeStateCmdHeader(uint16_t cmdCode, uint32_t origParam, uint32_t origCmdHi,
                                   unsigned char isAsync)
{
    uint32_t hi = ((uint32_t)cmdCode << 16) | (origCmdHi & 0x7FFFu) |
                  ((uint32_t)(isAsync & 1u) << 15);
    return ((uint64_t)hi << 32) | (uint64_t)origParam;
}

StateManager::StateManager()
    : m_initSema(-1),
      m_threadId(-1),
      m_cmdQueue(NULL),
      m_padAlign(0)
{
    m_cbs.onResult = NULL;
    m_cbs.onMessage = NULL;
    m_cbs.userData = NULL;

    new (&m_stateMachine) StateMachine(this);
    new (m_decoderMgrArena) DecoderManager();
    sce_paf_memset(m_sharedSyncArena, 0, sizeof(m_sharedSyncArena));

    SceUID sema = sceKernelCreateSema("SceShellMusicCoreStateMgrWait", 0, 0, 1, NULL);
    if (sema >= 0)
        m_initSema = sema;
}

StateManager::~StateManager()
{
    stop();
    if (m_initSema >= 0)
        sceKernelDeleteSema(m_initSema);
    reinterpret_cast<DecoderManager *>(m_decoderMgrArena)->~DecoderManager();
    reinterpret_cast<StateMachine *>(&m_stateMachine)->~StateMachine();
}

int StateManager::enqueueCmd(uint64_t cmdHdr, const StateCmdPayload *req)
{
    if (m_threadId == -1 || m_cmdQueue == NULL)
        return MUSICCORE_ERR_FAILED;
    if (req == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    CmdNode *node = NULL;
    int ret = m_cmdQueue->allocNode(&node, NULL);
    if (ret == 0 && node != NULL) {
        node->param = static_cast<uint32_t>(cmdHdr & 0xFFFFFFFFULL);
        node->flagsAndTarget = static_cast<uint16_t>((cmdHdr >> 32) & 0xFFFFu);
        node->cmdCode = static_cast<uint16_t>(cmdHdr >> 48);
        if (node->inBuf != NULL) {
            SceSize copyBytes = (sizeof(*req) <= node->inCap) ? sizeof(*req) : node->inCap;
            sce_paf_memcpy(node->inBuf, req, copyBytes);
            node->inSize = static_cast<unsigned int>(copyBytes);
        }
        ret = m_cmdQueue->enqueue(node, NULL, NULL);
    }
    return (ret == 0) ? MUSICCORE_ERR_FAILED : ret;
}

int StateManager::stop()
{
    if (m_threadId != -1) {
        StateCmdPayload req;
        initDefaultCmdPayload(&req);
        uint64_t stopHdr = (uint64_t)0x81008000u << 32;
        enqueueCmd(stopHdr, &req);

        if (sceKernelWaitThreadEnd(m_threadId, NULL, NULL) >= 0)
            sceKernelDeleteThread(m_threadId);
        m_threadId = -1;
    }

    if (m_cmdQueue != NULL) {
        m_cmdQueue->~CmdQueue();
        localFree(m_cmdQueue);
        m_cmdQueue = NULL;
    }

    reinterpret_cast<DecoderManager *>(m_decoderMgrArena)->setEventCallbacks(NULL, NULL, NULL, NULL);
    reinterpret_cast<DecoderManager *>(m_decoderMgrArena)->setOutputCallbacks(NULL, NULL, NULL, NULL, NULL);
    m_cbs.onResult = NULL;
    m_cbs.onMessage = NULL;
    m_cbs.userData = NULL;
    return 0;
}

int StateManager::start(const StateManagerCallbacks *cbs, const int *audioCfg)
{
    if (cbs == NULL || cbs->onResult == NULL || cbs->onMessage == NULL || cbs->userData == NULL)
        return MUSICCORE_ERR_FAILED;

    m_cbs = *cbs;
    int ret = reinterpret_cast<AudioOutput *>(const_cast<int *>(audioCfg))->setPositionCallback(
        reinterpret_cast<AudioOutput::PositionCallback>(thunkAudioError), this);
    if (ret != 0)
        return ret;

    ret = reinterpret_cast<DecoderManager *>(m_decoderMgrArena)->setOutputCallbacks(
        reinterpret_cast<void *>(audioCfg[0]), reinterpret_cast<void *>(audioCfg[1]),
        reinterpret_cast<void *>(audioCfg[2]), reinterpret_cast<void *>(audioCfg[3]),
        const_cast<int *>(audioCfg));
    if (ret != 0)
        return ret;

    ret = reinterpret_cast<DecoderManager *>(m_decoderMgrArena)->setEventCallbacks(
        reinterpret_cast<CodecAdapter::MediaReadyCallback>(thunkTrackReady),
        reinterpret_cast<CodecAdapter::ErrorCallback>(thunkTrackInfo),
        reinterpret_cast<CodecAdapter::EndOfStreamCallback>(thunkBufferReady), this);
    if (ret != 0)
        return ret;

    ret = reinterpret_cast<StateMachine *>(&m_stateMachine)->attach(
        const_cast<int *>(audioCfg), m_decoderMgrArena, m_sharedSyncArena);
    if (ret != 0)
        return ret;

    int err = 0;
    SceSize queueBytes = CmdQueue::calcRequiredSize(sizeof(StateCmdPayload), 0u, 64u);
    void *queueMem = localAlloc(queueBytes);
    if (queueMem == NULL)
        return MUSICCORE_ERR_NO_MEMORY;

    CmdQueue *queue = new (queueMem) CmdQueue("SceShellMusicCoreStateMgrThr",
                                              sizeof(StateCmdPayload), 0u, 64u, &err, queueBytes);
    if (queue == NULL)
        return MUSICCORE_ERR_NO_MEMORY;
    if (err != 0)
        return err;

    m_cmdQueue = queue;
    SceUID tid = sceKernelCreateThread("SceShellMusicCoreStateManager",
                                       threadEntry, 0x100000FF, 0x5000u, 0, 0, NULL);
    if (tid < 0)
        return tid;

    StateManager *selfArg = this;
    int startErr = sceKernelStartThread(tid, sizeof(selfArg), &selfArg);
    if (startErr != 0)
        return startErr;

    int waitErr = sceKernelWaitSema(m_initSema, 1, NULL);
    if (waitErr < 0)
        return waitErr;

    m_threadId = tid;
    return err;
}

int StateManager::activate(const StateMediaSnapshot *initState, void *metaBufs, int fadeMs,
                           uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    if (initState != NULL)
        sce_paf_memcpy(&req.media, initState, sizeof(req.media));
    req.fadeMs = fadeMs;
    req.metaBufs = metaBufs;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1101u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::deactivate(int fadeMs, uint32_t origParam, uint32_t origCmdHi,
                             unsigned char isAsync, unsigned char syncFlag)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.fadeMs = fadeMs;
    req.syncFlag = syncFlag;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1102u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::seek(const int *seekParams, uint32_t origParam, uint32_t origCmdHi,
                       unsigned char isAsync)
{
    if (seekParams == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.seekTarget = seekParams[0];
    req.seekVolumeMode = seekParams[1];
    req.seekMode = seekParams[2];
    req.seekSpeedMode = seekParams[3];
    req.seekStep = seekParams[4];
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x110Bu, origParam, origCmdHi, isAsync), &req);
}

int StateManager::play(int playParam, uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.playParamVal = playParam;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1105u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::pause(unsigned char resetPos, int fadeParam, uint32_t origParam,
                        uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.resetPos = resetPos;
    req.playParamVal = fadeParam;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1106u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::stopPlayback(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1107u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setRepeat(int repeatVal, uint32_t origParam, uint32_t origCmdHi,
                            unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.repeatValue = repeatVal;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1108u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setShuffle(int shuffleVal, uint32_t origParam, uint32_t origCmdHi,
                             unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.shuffleValue = shuffleVal;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1109u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::cancel(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1103u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setMedia(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi,
                           int shuffleVal, unsigned char isAsync, int unusedFlag,
                           unsigned char pauseFlag)
{
    (void)unusedFlag;
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.newMediaHandle = mediaHandle;
    req.shuffleValue = shuffleVal;
    req.syncFlag = pauseFlag;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1104u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setVolume(int vol, int ramp, uint32_t origParam, uint32_t origCmdHi,
                            unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.volumeLevel = vol;
    req.volumeRamp = ramp;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x110Cu, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setEqualizer(int eqType, int bandLow, int bandMid, int bandHigh,
                               uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.eqType = eqType;
    req.eqBandLow = bandLow;
    req.eqBandMid = bandMid;
    req.eqBandHigh = bandHigh;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x110Du, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setAudioNormalizer(int mode, uint32_t origParam, uint32_t origCmdHi,
                                     unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.normalizerMode = mode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x110Eu, origParam, origCmdHi, isAsync), &req);
}

int StateManager::seekAndPlay(const int *seekParams, int playParam, uint32_t origParam,
                              uint32_t origCmdHi, unsigned char isAsync)
{
    if (seekParams == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.seekTarget = seekParams[0];
    req.seekVolumeMode = seekParams[1];
    req.seekMode = seekParams[2];
    req.seekSpeedMode = seekParams[3];
    req.seekStep = seekParams[4];
    req.playParamVal = playParam;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1111u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::setMediaPlaying(void *mediaHandle, uint32_t origParam, uint32_t origCmdHi,
                                  int shuffleVal, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.newMediaHandle = mediaHandle;
    req.shuffleValue = shuffleVal;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1112u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onTrackReady(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1202u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onAudioError(int errCode, uint32_t origParam, uint32_t origCmdHi,
                               unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.errorCode = errCode;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1201u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onTrackInfo(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1200u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onBufferReady(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1204u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onStreamEnd(uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x1205u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::onAudioPortEvent(int flagA, int flagB, uint32_t origParam, uint32_t origCmdHi,
                                   unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    req.portFlag = (flagA == 1 || flagB == 1) ? 1 : 0;
    return enqueueCmd(makeStateCmdHeader(0x1206u, origParam, origCmdHi, isAsync), &req);
}

int StateManager::suspendForDb(uint32_t origParam, uint32_t origCmdHi, unsigned char suspendFlag,
                               unsigned char isAsync)
{
    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    req.suspendFlag = suspendFlag;
    req.origParam = origParam;
    req.origCmdHi = origCmdHi;
    return enqueueCmd(makeStateCmdHeader(0x110Fu, origParam, origCmdHi, isAsync), &req);
}

int StateManager::checkAndDeferNode(StateQueueNode *node, unsigned char *outDeferred)
{
    if (node == NULL || outDeferred == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    *outDeferred = 0;
    uint16_t waitCode = 0x1100u;
    uint16_t cmdCode = (uint16_t)(node->cmdHdr >> 48);
    int defer = reinterpret_cast<StateMachine *>(&m_stateMachine)->shouldDeferCmd(cmdCode, &waitCode);
    *outDeferred = (unsigned char)defer;
    if (defer == 0)
        return 0;

    int ret = m_cmdQueue->requeue(reinterpret_cast<CmdNode *>(node), 1u);
    if (ret != 0)
        return ret;

    for (int idx = 1; ; ++idx) {
        CmdNode *peekRaw = NULL;
        ret = m_cmdQueue->peekAt(&peekRaw, idx);
        if (ret != 0)
            break;
        StateQueueNode *peekNode = reinterpret_cast<StateQueueNode *>(peekRaw);
        uint16_t peekCode = (uint16_t)(peekNode->cmdHdr >> 48);
        if (peekCode == waitCode) {
            ret = m_cmdQueue->moveToFront(peekRaw, 0u);
            if (ret == 0) {
                m_cmdQueue->flushRequeued(1);
            }
            return ret;
        }
    }
    return ret;
}

int StateManager::runThreadLoop()
{
    int sigErr = sceKernelSignalSema(m_initSema, 1);
    if (sigErr < 0)
        return sigErr;

    StateCmdPayload req;
    initDefaultCmdPayload(&req);
    bool isIdle = false;
    bool stopping = false;

    while (true) {
        CmdNode *rawNode = NULL;
        int deqErr = m_cmdQueue->dequeue(&rawNode, NULL);
        if (deqErr != 0) {
            if (deqErr == MUSICCORE_ERR_QUEUE_RETRY || deqErr == MUSICCORE_ERR_QUEUE_TIMEOUT) {
                sceKernelDelayThread(5000);
                continue;
            }
            return 0;
        }

        if (rawNode == NULL)
            continue;

        StateQueueNode *node = reinterpret_cast<StateQueueNode *>(rawNode);
        if (node->inSize > sizeof(StateCmdPayload)) {
            m_cmdQueue->freeNode(rawNode, NULL);
            continue;
        }

        uint64_t cmdHdr = node->cmdHdr;
        sce_paf_memset(&req, 0, sizeof(req));
        sce_paf_memcpy(&req, node->inBuf, sizeof(req));

        unsigned char deferred = 0;
        if (checkAndDeferNode(node, &deferred) != 0)
            return 0;
        if (deferred == 1)
            continue;

        m_cmdQueue->freeNode(rawNode, NULL);

        if ((int64_t)cmdHdr < 0) {
            uint16_t stopCode = (uint16_t)(cmdHdr >> 48);
            if (stopCode == 0x8100u) {
                stopping = true;
                if (isIdle)
                    return 0;
                StateCmdPayload deactReq;
                initDefaultCmdPayload(&deactReq);
                StateMediaSnapshot outMedia;
                initDefaultMediaSnapshot(&outMedia);
                int outTail[4];
                outTail[0] = 7;
                outTail[1] = -1;
                outTail[2] = -1;
                unsigned char outFlags[4];
                outFlags[0] = 0;
                reinterpret_cast<StateMachine *>(&m_stateMachine)->dispatchCmd(
                    0x1102u, reinterpret_cast<const StateDispatchReq *>(&deactReq),
                    reinterpret_cast<StateSnapshotOut *>(&outMedia), outTail, outFlags);
                if (outMedia.stateId == 0)
                    return 0;
                isIdle = false;
            }
            continue;
        }

        StateResultEvent ev;
        sce_paf_memset(&ev, 0, sizeof(ev));
        initDefaultMediaSnapshot(&ev.media);

        uint16_t cmdCode = (uint16_t)(cmdHdr >> 48);
        int err = reinterpret_cast<StateMachine *>(&m_stateMachine)->dispatchCmd(
            cmdCode, reinterpret_cast<const StateDispatchReq *>(&req),
            reinterpret_cast<StateSnapshotOut *>(&ev.media), &ev.media.targetState, ev.extraFlags);
        ev.cmdHdr = cmdHdr;
        ev.errCode = err;
        ev.origParam = req.origParam;
        ev.origCmdHi = req.origCmdHi;
        m_cbs.onResult(&ev, m_cbs.userData);

        isIdle = (ev.media.stateId == 0);
        if (stopping) {
            if (isIdle)
                return 0;
            StateCmdPayload deactReq;
            initDefaultCmdPayload(&deactReq);
            StateMediaSnapshot outMedia;
            initDefaultMediaSnapshot(&outMedia);
            int outTail[4];
            outTail[0] = 7;
            outTail[1] = -1;
            outTail[2] = -1;
            unsigned char outFlags[4];
            outFlags[0] = 0;
            reinterpret_cast<StateMachine *>(&m_stateMachine)->dispatchCmd(
                0x1102u, reinterpret_cast<const StateDispatchReq *>(&deactReq),
                reinterpret_cast<StateSnapshotOut *>(&outMedia), outTail, outFlags);
            if (outMedia.stateId == 0)
                return 0;
            isIdle = false;
        }
    }
}

int StateManager::threadEntry(SceSize args, void *argp)
{
    (void)args;
    StateManager *self = *(StateManager **)argp;
    int ret = (self != NULL) ? self->runThreadLoop() : 0;
    SceThreadmgrCoredumpTime_sceKernelExitThread(1);
    return ret;
}

int StateManager::onBufferReadyCallback()
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    StateNotifyMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32058000u;
    return m_cbs.onMessage(&msg, m_cbs.userData, 0);
}

int StateManager::onAudioErrorCallback(int errCode)
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    StateNotifyMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32048000u;
    msg.errorCode = errCode;
    return m_cbs.onMessage(&msg, m_cbs.userData, 0);
}

int StateManager::forwardNotifyMetadata()
{
    void *smOwner = m_stateMachine.activeMedia;
    if (smOwner != NULL) {
        typedef void (*NotifyFn)(void *);
        NotifyFn *vtable = *(NotifyFn **)smOwner;
        vtable[28](smOwner);
    }
    return 0;
}

int StateManager::onTrackReadyCallback()
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    StateNotifyMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32018000u;
    return m_cbs.onMessage(&msg, m_cbs.userData, 0);
}

int StateManager::onTrackInfoCallback(const DecoderTrackInfoMsg *info)
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    StateNotifyMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32038000u;
    if (info != NULL) {
        msg.media.playlistId = info->playlistId;
        msg.media.trackId = info->trackId;
        msg.media.trackIndex = info->trackIndex;
        msg.media.positionMs = info->positionMs;
        msg.media.currentTrackNum = info->currentTrackNum;
        msg.media.totalTracks = info->totalTracks;
        msg.media.durationMs = info->durationMs;
        msg.media.sampleRate = info->sampleRate;
        msg.media.bitRate = info->bitRate;
        msg.media.channels = info->channels;
        msg.media.codecType = info->codecType;
        msg.media.sourceType = info->sourceType;
        msg.media.volumeMode = info->volumeMode;
    }
    return m_cbs.onMessage(&msg, m_cbs.userData, 0);
}

int StateManager::onStreamEndCallback(int val)
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    StateNotifyMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32068000u;
    msg.extraVal = val;
    return m_cbs.onMessage(&msg, m_cbs.userData, 0);
}

int StateManager::onMediaChangedCallback(unsigned char hasNewMedia, void *newMedia,
                                         unsigned char hasUriFlag,
                                         const StateMediaSnapshot *state,
                                         void **ioOldMedia, unsigned char decrementUri)
{
    if (m_cbs.onMessage == NULL)
        return MUSICCORE_ERR_FAILED;

    if (hasNewMedia != 1 && *ioOldMedia == NULL)
        return 0;

    StateMediaChangedMsg msg;
    sce_paf_memset(&msg, 0, sizeof(msg));
    initDefaultMediaSnapshot(&msg.media);
    msg.param = 0;
    msg.cmdHi = 0x32078000u;
    msg.hasNewMedia = hasNewMedia;
    if (hasNewMedia == 1) {
        msg.newMedia = newMedia;
        msg.hasUriFlag = hasUriFlag;
        if (state != NULL)
            msg.media = *state;
    }
    msg.oldMedia = *ioOldMedia;
    msg.decrementUri = decrementUri;

    int ret = m_cbs.onMessage(&msg, m_cbs.userData, 0);
    if (ret == 0)
        *ioOldMedia = NULL;
    return ret;
}

int StateManager::thunkTrackReady(StateManager *self)
{
    return self->onTrackReadyCallback();
}

int StateManager::thunkTrackInfo(StateManager *self)
{
    return self->onTrackInfoCallback(NULL);
}

int StateManager::thunkBufferReady(StateManager *self)
{
    return self->onBufferReadyCallback();
}

int StateManager::thunkAudioError(StateManager *self, int errCode)
{
    return self->onAudioErrorCallback(errCode);
}

} /* namespace MusicCore */
} /* namespace sce */
