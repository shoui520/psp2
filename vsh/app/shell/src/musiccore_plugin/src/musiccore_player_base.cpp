/*
 * SceShell musiccore_plugin - BasePlayer (common player state and command handlers).
 * Original address range (3.65-CEX): 0x811DC4A0 - 0x811DFD10
 */

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>

extern "C" {
char *sce_paf_strncpy(char *dst, const char *src, size_t maxLen);
int sceKernelGetSystemTime(SceKernelSysClock *clock);
}

static const int MUSICCORE_ERR_FAILED = -1879113726;      /* 0x8FFF0002 */
static const int MUSICCORE_ERR_NO_MEMORY = -1879113724;   /* 0x8FFF0004 */
static const int MUSICCORE_ERR_INVALID_ARG = -1879113723; /* 0x8FFF0005 */
static const int MUSICCORE_ERR_NO_MEDIA = -1879113722;    /* 0x8FFF0006 */
static const int MUSICCORE_ERR_DB_REMOVED = -1879113715;  /* 0x8FFF000D */

namespace sce {
namespace MusicCore {

struct CmdNode {
    void *vtable;
    unsigned int padAlign;
    uint32_t param;
    uint8_t targetChannel;
    uint8_t flagsByte;
    uint16_t cmdCode;
    void *inBuf;
    unsigned int inSize;
    unsigned int inCap;
    void *outBuf;
    unsigned int outSize;
    unsigned int outCap;
    int state;
    void *waiter;
    CmdNode *next;
    CmdNode *prev;
};

struct PlayerPriorityInfo {
    char bgmStatus;
    char bgmReason;
    unsigned char activePlayback;
    unsigned char isPlayable;
    unsigned char isOpen;
    unsigned char isEnabled;
    unsigned char isDbAllowed;
    unsigned char isBusy;
    int priority;
    int subPriority;
    unsigned char wantsPlay;
    unsigned char hasMedia;
    unsigned char hasPending;
    unsigned char padTail;
};

struct PlayerStatusSnapshot {
    unsigned char channelId;
    unsigned char playerKind;
    char bgmStatus;
    char bgmReason;
    int bgmPriority;
    unsigned char playableFlag;
    unsigned char padPlayable[3];
    int seqNum;
    int16_t flags;
    int16_t padFlags;
    int cmdParam;
    int errCode;
    int playState;
    int contentType;
    int padContent;
    int64_t trackId;
    int64_t trackItemId;
    int64_t playlistId;
    int trackIndex;
    int totalTracks;
    int currentTrackNum;
    int durationMs;
    int sampleRate;
    int bitRate;
    int channels;
    int codecType;
    int sourceType;
    int repeatMode;
    int shuffleMode;
    int volumeMode;
    int audioBalance;
    int eqType;
    int eqBandLow;
    int eqBandMid;
    int eqBandHigh;
    int audioNormalizer;
    int extraErr;
    int padTail;
};

struct DecoderMediaState {
    int stateId;
    int playState;
    int64_t playlistId;
    int64_t trackId;
    int64_t trackItemId;
    int trackOrderIndex;
    int trackIndex;
    int positionMs;
    int currentTrackNum;
    int totalTracks;
    int durationMs;
    int sampleRate;
    int bitRate;
    int channels;
    int codecType;
    void *mediaHandle;
    int repeatMode;
    int shuffleMode;
    int seekMode;
    int speedMode;
    int volumeMode;
    int audioBalance;
    int eqType;
    int eqBandLow;
    int eqBandMid;
    int eqBandHigh;
    int audioNormalizer;
    int sourceType;
    int lastErrorType;
    int hasPendingError;
    int extraErr;
    int reservedTail[4];
};

struct MetadataBuffers {
    char *titleBuf;
    size_t titleLen;
    char *artistBuf;
    size_t artistLen;
    char *albumBuf;
    size_t albumLen;
};

struct PlayerCallbacks {
    void (*onStatus)(const PlayerStatusSnapshot *snap, void *userData, int force);
    int (*onExtra)(const void *data, void *userData);
    void (*onMetadata)(int mode, const char *uriBuf, const PlayerStatusSnapshot *snap, void *userData);
    void (*onCap)(const void *data, void *userData);
    void *userData;
};

struct UriWorkHeader {
    void *vtablePtr;
    int mode;
    int headerWords[2];
    char uriStr[2048];
    int padUriTail;
    int listIndex;
    int trackIndex;
    int padTrackId;
    int trackIdLo;
    int trackIdHi;
    int contentKind;
    int extraMeta;
};

struct UriWorkStrMode {
    void *vtablePtr;
    int mode;
    int headerWords[4];
    const char *uriPtr;
};

struct MediaItemHandle {
    virtual ~MediaItemHandle() {}
    virtual int checkOrderListStatus(int *outPrevId, unsigned char *outFlag) { (void)outPrevId; (void)outFlag; return 0; }
    virtual int releaseOrderList(unsigned char clearOwner) { (void)clearOwner; return 0; }
    virtual int getListId() const { return 0; }
    virtual void *getWorkBuf() = 0;
    virtual int navigateTo(int navTarget, unsigned char *outValid) { (void)navTarget; (void)outValid; return 0; }
    virtual unsigned int resolveErrorNavigation(int curIdx, int wrapMode, int dir, unsigned int *ioTried, int *ioErr, unsigned char *outEnd, unsigned char *outWrap) {
        (void)curIdx; (void)wrapMode; (void)dir; (void)ioTried; (void)ioErr; (void)outEnd; (void)outWrap; return 0;
    }
    virtual int resolveCurrentTrackId(int64_t *outTrackId) { (void)outTrackId; return 0; }
    virtual void setTrackPosition(int pos, int64_t *trackId) = 0;
    virtual int checkChangedFlag(unsigned char *outChanged, unsigned char queryLive) { (void)outChanged; (void)queryLive; return 0; }
    virtual int checkValid() { return 0; }
    virtual int checkPlayableAt(int index, int *outStatus) { (void)index; (void)outStatus; return 0; }
    virtual int getListStatus(void *outStatus) { (void)outStatus; return 0; }
    virtual int getContentType() = 0;
    virtual int getPlayState() = 0;
    virtual int getTrackIndex() = 0;
    virtual int64_t getTrackKey() = 0;
    virtual int rebuildOrderList(int shuffleSeed, int64_t anchorRowId, unsigned char *outChanged, unsigned char forceRebuild) {
        (void)shuffleSeed; (void)anchorRowId; (void)outChanged; (void)forceRebuild; return 0;
    }
    virtual int unlockOrderList(int flag) { (void)flag; return 0; }
    virtual int refreshOrderBuffer() { return 0; }
    virtual void populateMetadata(MetadataBuffers *meta) = 0;
};

struct DecoderSession {
    virtual ~DecoderSession() {}
    virtual int onTrackInfoCallback(const void *info) { (void)info; return 0; }
    virtual int onAudioErrorCallback(int errCode) { (void)errCode; return 0; }
    virtual int onBufferReadyCallback() { return 0; }
    virtual int onTrackReadyCallback() { return 0; }
    virtual int onStreamEndCallback(int val) { (void)val; return 0; }
    virtual int onMediaChangedCallback(unsigned char hasNew, void *newMedia, unsigned char hasUri, const void *state, void **ioOld, unsigned char decUri) {
        (void)hasNew; (void)newMedia; (void)hasUri; (void)state; (void)ioOld; (void)decUri; return 0;
    }
    virtual int forwardNotifyMetadata() { return 0; }
    virtual int start(const void *cbs, const int *audioCfg) { (void)cbs; (void)audioCfg; return 0; }
    virtual int stopSession() { return 0; }
    virtual int activate(DecoderMediaState *state, MetadataBuffers *meta, int fadeMs) = 0;
    virtual int deactivate(int fadeMs, uint32_t param, uint32_t cmdHi, unsigned char sync, int extraFlags) = 0;
    virtual int seek(const int *seekParams, uint32_t param, uint32_t cmdHi, unsigned char flag) = 0;
    virtual int play(int param, uint32_t seq) = 0;
    virtual int pause(int resetPos, int param) = 0;
    virtual int stop() = 0;
    virtual int setRepeat(int repeatVal, uint32_t param, uint32_t cmdHi, int flag) = 0;
    virtual int setShuffle(int shuffleSeed, uint32_t param, uint32_t cmdHi, int flag) = 0;
    virtual int cancel() = 0;
    virtual int setMedia(void *media, uint32_t param, uint32_t cmdHi, int shuffleSeed,
                         int isAsync, int clearFlag, int pauseFlag) = 0;
    virtual int setVolume(int vol, int ramp) = 0;
    virtual int setEqualizer() = 0;
    virtual int setAudioNormalizer(int mode, uint32_t param, uint32_t cmdHi, int flag) = 0;
    virtual int seekAndPlay(const int *seekParams, int playParam) = 0;
    virtual int setMediaPlaying(void *media, uint32_t param, uint32_t cmdHi, int shuffleSeed, int isAsync) = 0;
    virtual int onTrackReady() = 0;
    virtual int onAudioError(int errCode, uint32_t param) = 0;
    virtual int onTrackInfo() = 0;
    virtual int onBufferReady() = 0;
    virtual int onStreamEnd() = 0;
    virtual int onAudioPortEvent(int flagA, int flagB, uint32_t origParam, uint32_t origCmdHi, unsigned char isAsync) = 0;
    virtual int suspendForDb() = 0;

    SceUID m_initSema;
    SceUID m_threadId;
    void *m_cmdQueue;
    void *m_callbacks[3];
    int m_padAlign;
    unsigned char m_stateMachine[4600];
};

struct TimeReport {
    unsigned char channelId;
    unsigned char playerKind;
    unsigned char padHeader[2];
    int positionMs;
    int64_t trackId;
};

struct CapReport {
    unsigned char channelId;
    unsigned char playerKind;
    unsigned char padHeader[2];
    int code;
};

struct OpenPlayerParam {
    uint32_t headerWords[2];
    int priority;
};

struct PlayCmdPayload {
    int headerWord;
    int playParam;
};

struct PauseCmdPayload {
    int headerWord;
    unsigned char resetPos;
    unsigned char padReset[3];
    int pauseParam;
};

struct SeekCmdPayload {
    int headerWord;
    int seekParams[5];
};

extern unsigned int calcMediaQueueSize(int capacity, int a, int b, int c);
extern int decoderGetCurrentPos(void *decoderState);
extern int64_t decoderGetCurrentTrackId(void *decoderState);
extern void *workBufGetRegion(void *workBuf, int index);
extern void computePlaybackRange(int repeatMode, int shuffleMode, int step, int totalTracks,
                                 int speedMode, int *outPrev, int *outNext, int *outCur);
extern int dbManagerRequestUri(void *dbMgr, int mode, void *workBuf, int shuffleSeed,
                               uint32_t param, uint32_t cmdHi, int reqFlags, int playMode, void *extra);
extern int dbManagerReleaseMedia(void *dbMgr, void *media, uint64_t cmdHdr);
extern int dbManagerSeekMedia(void *dbMgr, void *media, int target);
extern int dbManagerQueryDbId(void *dbMgr, void *outBuf, size_t bufSize);
extern int dbManagerSync(void *dbMgr);
extern int dbManagerRefresh(void *dbMgr);
extern int factoryDestroyWorkBuf(void *factory, void *workBuf);

static unsigned char g_playlistDbIdBuf[1028];
static int g_playlistDbIdState = 0;

void *getGlobalTrackTag()
{
    if (g_playlistDbIdState == 2)
        return g_playlistDbIdBuf;
    return NULL;
}

static void resetMediaState(DecoderMediaState *st)
{
    st->stateId = 0;
    st->playState = 0;
    st->playlistId = -1;
    st->trackId = -1;
    st->trackItemId = -1;
    st->trackOrderIndex = -2;
    st->trackIndex = 0;
    st->positionMs = -1;
    st->currentTrackNum = -1;
    st->totalTracks = -1;
    st->durationMs = 0;
    st->mediaHandle = NULL;
    st->repeatMode = 0;
    st->shuffleMode = 0;
    st->seekMode = -1;
    st->speedMode = -1;
    st->volumeMode = 1;
    st->audioBalance = 0x8000;
    st->eqType = 0;
    st->eqBandLow = 0;
    st->eqBandMid = 0;
    st->eqBandHigh = 0;
    st->audioNormalizer = 0;
    st->sourceType = 0;
    st->lastErrorType = 0;
    st->hasPendingError = 0;
    st->extraErr = 0;
}

class BasePlayer {
public:
    static unsigned int calcBaseExtraSize(int capacity);

    BasePlayer(char playerKind, void *factory, int capacity, int *outErr,
               unsigned int totalBytes, unsigned int headerBytes);
    virtual ~BasePlayer();

    virtual void onConstructReserved() {}
    virtual int attach(unsigned char channelId, const PlayerCallbacks *cbs, void *dbMgr, void *audioOut);
    virtual int detach();
    virtual void setAudioFocus(int granted) { (void)granted; }
    virtual int getAudioFocus() { return 0; }
    virtual int getPriority() { return m_priority; }
    virtual int getSubPriority() { return m_subPriority; }
    virtual int open(const void *param, unsigned char *flags);
    virtual int close(unsigned char *flags);
    virtual int isOpen() { return m_isOpen; }
    virtual int isBusy() { return m_isBusy; }
    virtual void onDbSuspend(unsigned char *flags);
    virtual void onDbResume(unsigned char *flags);
    virtual void onPauseForDb(unsigned char *flags);
    virtual PlayerPriorityInfo getPriorityInfo();
    virtual int isActive() { return 0; }
    virtual void onPriorityReserved() {}
    virtual int wantsAudio() { return 0; }
    virtual void onAudioReserved() {}
    virtual int isLocked() { return m_isSuspended; }
    virtual int getContentInfo(MetadataBuffers *outMeta);
    virtual void onIdleActive() {}
    virtual void onIdlePlaylist() {}
    virtual void onIdleReserved() {}
    virtual void setSystemSuspended(int suspended) { m_systemSuspended = (unsigned char)suspended; }
    virtual int getSystemSuspended() { return m_systemSuspended; }
    virtual int isPlayable() { return m_playable; }

    /* Command handlers (vtable slots +112 .. +228) */
    virtual int cmdEnable(CmdNode *node, unsigned char *flags);
    virtual int cmdDisable(CmdNode *node, unsigned char *flags);
    virtual int cmdCancel(CmdNode *node, unsigned char *flags);
    virtual int cmdPlay(CmdNode *node, unsigned char *flags);
    virtual int cmdPause(CmdNode *node, unsigned char *flags);
    virtual int cmdStop(CmdNode *node, unsigned char *flags);
    virtual int cmdSeek(CmdNode *node, unsigned char *flags);
    virtual int cmdSetRepeat(CmdNode *node, unsigned char *flags);
    virtual int cmdSetShuffle(CmdNode *node, unsigned char *flags);
    virtual int cmdSetUriData(CmdNode *node, unsigned char *flags);
    virtual int cmdSetUriString(CmdNode *node, unsigned char *flags);
    virtual int cmdPrepareDb(CmdNode *node, unsigned char *flags);
    virtual int cmdUnsupported();
    virtual int cmdSetAudioBalance(CmdNode *node, unsigned char *flags);
    virtual int cmdSetEqualizer(CmdNode *node, unsigned char *flags);
    virtual int cmdSetAudioNormalizer(CmdNode *node, unsigned char *flags);
    virtual int cmdSeekAndPlay(CmdNode *node, unsigned char *flags);
    virtual int cmdNotifyStatus(CmdNode *node, unsigned char *flags);
    virtual int cmdResetSpeedReq(CmdNode *node, unsigned char *flags);
    virtual int cmdRefreshDbId(CmdNode *node);
    virtual int onDecoderEvent(CmdNode *node, unsigned char *flags);
    virtual int onDecoderTrackReady(CmdNode *node);
    virtual int onDecoderTrackInfo(CmdNode *node);
    virtual int onDecoderAudioError(CmdNode *node, unsigned char *flags);
    virtual int onDecoderBufferReady(CmdNode *node);
    virtual int onDecoderStreamEnd(CmdNode *node);
    virtual int onDecoderMediaChanged(CmdNode *node);
    virtual int onPlaybackError(int errType, unsigned char *flags, unsigned char *outRetryPlay);
    virtual int onDbEvent(CmdNode *node, unsigned char *flags);
    virtual int cmdResetSpeed(uint32_t param, uint32_t cmdHi, unsigned char *flags);

    int dispatchCmd(CmdNode *node, unsigned char *flags);
    int setSuspended(char suspend);
    int buildStatusSnapshot(PlayerStatusSnapshot *outSnap, const DecoderMediaState *state);
    void notifyStatus(int unused, uint32_t param, uint32_t cmdHi, int errCode, unsigned char *flags);
    int activateDecoder(int fadeMs, DecoderSession *decoder);
    int deactivateDecoder(int fadeMs, uint32_t param, uint32_t cmdHi, unsigned char syncFlag);
    int flushTimeCallback();
    int getStatus(PlayerStatusSnapshot *outSnap, int *outPos, unsigned char *outPosValid);
    int formatUriInfo(char *outBuf, int *outMode, const void *uriWork, const DecoderMediaState *state);
    int getExtraStatus(int *outMode, char onlyIfDirty, char *outBuf, unsigned char *outDirty);
    void tickStatus();
    int applyOfflineSeek(const int *seekParams, unsigned char *flags);
    int applySeek(const int *seekParams, int playParam, uint32_t param, uint32_t cmdHi,
                  unsigned char *flags, char startPlay, unsigned char extraFlag);
    void clearPendingUriIfMatches(MediaItemHandle *media);

public:
    char m_playerKind;
    unsigned char m_channelId;
    unsigned char m_padChannel[2];
    void *m_factory;
    void (*m_statusCb)(const PlayerStatusSnapshot *, void *, int);
    int (*m_extraCb)(const void *, void *);
    void (*m_metadataCb)(int, const char *, const PlayerStatusSnapshot *, void *);
    void (*m_capCb)(const void *, void *);
    void *m_userData;
    void *m_dbMgr;
    int m_pendingDbOps;
    DecoderSession *m_decoder;
    void *m_audioOut;
    unsigned char m_isOpen;
    unsigned char m_enabled;
    unsigned char m_dbAllowed;
    unsigned char m_wantsPlay;
    unsigned char m_isBusy;
    unsigned char m_padStateFlags[3];
    int m_priority;
    int m_subPriority;
    int m_pendingUriCount;
    int m_padOfflineAlign;

    DecoderMediaState m_offlineState;
    int m_targetState;
    int m_pendingFadeMs;
    int m_playParam;
    int m_padActiveAlign;
    DecoderMediaState m_activeState;

    unsigned char m_isSuspended;
    unsigned char m_padSuspended[7];
    paf::thread::Mutex m_metaMutex;
    MetadataBuffers m_metaBufs;
    paf::thread::Mutex *m_metaMutexPtr;
    int m_capacity;
    void *m_mediaQueue;
    int m_shuffleSeed;
    void *m_lastUriWorkBuf;
    unsigned char m_uriDirty;
    unsigned char m_padUriDirty[3];
    int m_savedTargetState;
    int m_savedSeekMode;
    int m_savedSeekParam;
    int m_pendingSeekCount;
    unsigned char m_systemSuspended;
    unsigned char m_playable;
    unsigned char m_padPlayable[2];
    int m_seqNum;
};

unsigned int BasePlayer::calcBaseExtraSize(int capacity)
{
    unsigned int sz = calcMediaQueueSize(capacity, 2, 2, 56);
    return ((sz - 1) & ~7u) + 8;
}

BasePlayer::BasePlayer(char playerKind, void *factory, int capacity, int *outErr,
                       unsigned int totalBytes, unsigned int headerBytes)
    : m_playerKind(playerKind),
      m_channelId(0xFF),
      m_factory(factory),
      m_statusCb(NULL),
      m_extraCb(NULL),
      m_metadataCb(NULL),
      m_capCb(NULL),
      m_userData(NULL),
      m_dbMgr(NULL),
      m_pendingDbOps(0),
      m_decoder(NULL),
      m_audioOut(NULL),
      m_isOpen(0),
      m_enabled(1),
      m_dbAllowed(1),
      m_wantsPlay(0),
      m_isBusy(0),
      m_priority(-1),
      m_subPriority(-1),
      m_pendingUriCount(-1),
      m_targetState(7),
      m_pendingFadeMs(-1),
      m_playParam(-1),
      m_isSuspended(0),
      m_metaMutex("SceShellMusicCoreCtrlMetaInf", 0),
      m_metaMutexPtr(&m_metaMutex),
      m_capacity(capacity),
      m_mediaQueue(NULL),
      m_shuffleSeed(0),
      m_lastUriWorkBuf(NULL),
      m_uriDirty(0),
      m_savedTargetState(-1),
      m_savedSeekMode(-1),
      m_savedSeekParam(-1),
      m_pendingSeekCount(0),
      m_systemSuspended(0),
      m_playable(0),
      m_seqNum(0)
{
    resetMediaState(&m_offlineState);
    resetMediaState(&m_activeState);

    *outErr = MUSICCORE_ERR_FAILED;
    m_metaMutex.Lock();
    m_metaBufs.titleBuf = NULL;
    m_metaBufs.titleLen = 0;
    m_metaBufs.artistBuf = NULL;
    m_metaBufs.artistLen = 0;
    m_metaBufs.albumBuf = NULL;
    m_metaBufs.albumLen = 0;
    m_metaMutex.Unlock();

    if (m_factory == NULL) {
        *outErr = MUSICCORE_ERR_INVALID_ARG;
        return;
    }

    unsigned int needed = calcBaseExtraSize(m_capacity) + 472;
    unsigned int alignedHdr = ((headerBytes - 1) & ~7u) + 8;
    if (totalBytes >= needed && alignedHdr <= totalBytes)
        *outErr = 0;
    else
        *outErr = MUSICCORE_ERR_NO_MEMORY;
}

BasePlayer::~BasePlayer()
{
    if (m_mediaQueue != NULL) {
        struct Destructible {
            virtual ~Destructible() {}
        };
        ((Destructible *)m_mediaQueue)->~Destructible();
    }
}

int BasePlayer::attach(unsigned char channelId, const PlayerCallbacks *cbs, void *dbMgr, void *audioOut)
{
    if (cbs->onStatus == NULL || cbs->onExtra == NULL || cbs->onCap == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    m_channelId = channelId;
    m_statusCb = cbs->onStatus;
    m_extraCb = cbs->onExtra;
    m_metadataCb = cbs->onMetadata;
    m_capCb = cbs->onCap;
    m_userData = cbs->userData;
    m_dbMgr = dbMgr;
    m_audioOut = audioOut;
    m_pendingDbOps = 0;
    return 0;
}

int BasePlayer::detach()
{
    m_statusCb = NULL;
    m_extraCb = NULL;
    m_metadataCb = NULL;
    m_capCb = NULL;
    m_userData = NULL;
    m_dbMgr = NULL;
    m_audioOut = NULL;
    m_pendingDbOps = 0;
    m_channelId = 0xFF;
    return 0;
}

int BasePlayer::open(const void *param, unsigned char *flags)
{
    if (param == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prio = ((const OpenPlayerParam *)param)->priority;
    if (prio == -1)
        prio = m_subPriority;
    m_priority = prio;

    int prevWant = wantsAudio();
    m_isOpen = 1;
    if (prevWant != wantsAudio())
        *flags |= 4u;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::close(unsigned char *flags)
{
    int prevWant = wantsAudio();
    m_isBusy = 0;
    m_isOpen = 0;
    m_wantsPlay = 0;
    int newWant = wantsAudio();
    if (prevWant != newWant)
        *flags |= 4u;
    if (newWant == 0 && m_decoder == NULL)
        *flags |= 8u;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::setSuspended(char suspend)
{
    if (suspend != 1) {
        m_isSuspended = 0;
        return 0;
    }

    if (m_decoder != NULL) {
        m_isSuspended = 1;
        int ret = m_decoder->suspendForDb();
        if (ret != 0)
            m_isSuspended = 0;
        return ret;
    }

    if (m_offlineState.mediaHandle == NULL) {
        m_isSuspended = 0;
        return 0;
    }
    if (m_dbMgr == NULL) {
        m_isSuspended = 0;
        return MUSICCORE_ERR_FAILED;
    }

    MediaItemHandle *media = (MediaItemHandle *)m_offlineState.mediaHandle;
    media->setTrackPosition(m_offlineState.trackOrderIndex, &m_offlineState.trackId);
    int ret = dbManagerSeekMedia(m_dbMgr, media, -262143);
    if (ret != 0) {
        m_isSuspended = 0;
    } else {
        m_isSuspended = 1;
        ++m_pendingSeekCount;
    }
    return ret;
}

void BasePlayer::onDbResume(unsigned char *flags)
{
    int prevWant = wantsAudio();
    m_dbAllowed = 1;
    if (prevWant != wantsAudio())
        *flags |= 4u;
}

void BasePlayer::onPauseForDb(unsigned char *flags)
{
    int prevWant = wantsAudio();
    m_dbAllowed = 0;
    if (prevWant != wantsAudio())
        *flags |= 4u;
}

void BasePlayer::onDbSuspend(unsigned char *flags)
{
    onPauseForDb(flags);
}

PlayerPriorityInfo BasePlayer::getPriorityInfo()
{
    PlayerPriorityInfo info;
    info.bgmStatus = -1;
    info.bgmReason = -1;
    info.activePlayback = (m_decoder != NULL && m_isBusy == 1) ? 1 : 0;
    info.isPlayable = 1;
    info.isOpen = m_isOpen;
    info.isEnabled = m_enabled;
    info.isDbAllowed = m_dbAllowed;
    info.isBusy = m_isBusy;
    info.priority = m_priority;
    info.subPriority = m_subPriority;
    info.wantsPlay = m_wantsPlay;
    info.hasMedia = ((m_decoder != NULL && m_activeState.mediaHandle != NULL) ||
                     (m_decoder == NULL && m_offlineState.mediaHandle != NULL)) ? 1 : 0;
    info.hasPending = (m_pendingUriCount != -1) ? 1 : 0;
    info.padTail = 0;
    return info;
}

int BasePlayer::dispatchCmd(CmdNode *node, unsigned char *flags)
{
    uint16_t cmd = node->cmdCode;
    if (cmd == 0x3400)
        return onDbEvent(node, flags);

    if (cmd >= 0x3200 && cmd <= 0x3207) {
        switch (cmd) {
        case 0x3200: return onDecoderEvent(node, flags);
        case 0x3201: return onDecoderTrackReady(node);
        case 0x3203: return onDecoderTrackInfo(node);
        case 0x3204: return onDecoderAudioError(node, flags);
        case 0x3205: return onDecoderBufferReady(node);
        case 0x3206: return onDecoderStreamEnd(node);
        case 0x3207: return onDecoderMediaChanged(node);
        default: return MUSICCORE_ERR_INVALID_ARG;
        }
    }

    if (cmd >= 0x3102 && cmd <= 0x311B) {
        switch (cmd) {
        case 0x3102: return cmdEnable(node, flags);
        case 0x3103: return cmdDisable(node, flags);
        case 0x3105: return cmdCancel(node, flags);
        case 0x310A: return cmdPlay(node, flags);
        case 0x310B: return cmdPause(node, flags);
        case 0x310C: return cmdStop(node, flags);
        case 0x310D: return cmdSeek(node, flags);
        case 0x310E: return cmdSetRepeat(node, flags);
        case 0x310F: return cmdSetShuffle(node, flags);
        case 0x3111: return cmdSetUriData(node, flags);
        case 0x3112: return cmdSetUriString(node, flags);
        case 0x3113: return cmdPrepareDb(node, flags);
        case 0x3114: return cmdSetAudioBalance(node, flags);
        case 0x3115: return cmdSetEqualizer(node, flags);
        case 0x3116: return cmdSetAudioNormalizer(node, flags);
        case 0x3117: return cmdSeekAndPlay(node, flags);
        case 0x3118: return cmdNotifyStatus(node, flags);
        case 0x3119: return cmdResetSpeedReq(node, flags);
        case 0x311A: return cmdUnsupported();
        case 0x311B: return cmdRefreshDbId(node);
        default: return MUSICCORE_ERR_INVALID_ARG;
        }
    }

    return MUSICCORE_ERR_INVALID_ARG;
}

int BasePlayer::buildStatusSnapshot(PlayerStatusSnapshot *outSnap, const DecoderMediaState *state)
{
    int64_t trackId = state->trackId;
    if (trackId == -2)
        trackId = -1;
    outSnap->playState = state->playState;
    outSnap->trackId = trackId;

    MediaItemHandle *media = (MediaItemHandle *)state->mediaHandle;
    if (media != NULL) {
        unsigned int contentType = (unsigned int)media->getContentType();
        if (contentType <= 1) {
            outSnap->contentType = (contentType != 0) ? 1 : 0;
            outSnap->trackItemId = -1;
            outSnap->playlistId = -1;
        } else if (contentType == 2) {
            outSnap->contentType = 2;
            outSnap->trackItemId = state->trackItemId;
            outSnap->playlistId = state->playlistId;
        } else {
            outSnap->contentType = -1;
            outSnap->trackItemId = -1;
            outSnap->playlistId = -1;
        }
    } else {
        outSnap->contentType = -1;
        outSnap->trackItemId = -1;
        outSnap->playlistId = -1;
    }

    outSnap->trackIndex = state->trackIndex;
    outSnap->totalTracks = state->totalTracks;
    outSnap->currentTrackNum = state->currentTrackNum;
    outSnap->durationMs = state->durationMs;
    outSnap->sampleRate = state->sampleRate;
    outSnap->bitRate = state->bitRate;
    outSnap->channels = state->channels;
    outSnap->codecType = state->codecType;
    outSnap->sourceType = state->sourceType;
    outSnap->repeatMode = state->repeatMode;
    outSnap->shuffleMode = (state->shuffleMode != 0) ? 1 : 0;
    outSnap->volumeMode = state->volumeMode;
    outSnap->audioBalance = state->audioBalance;
    outSnap->eqType = state->eqType;
    outSnap->eqBandLow = state->eqBandLow;
    outSnap->eqBandMid = state->eqBandMid;
    outSnap->eqBandHigh = state->eqBandHigh;
    outSnap->audioNormalizer = state->audioNormalizer;
    outSnap->extraErr = state->extraErr;
    return outSnap->extraErr;
}

void BasePlayer::notifyStatus(int unused, uint32_t param, uint32_t cmdHi, int errCode, unsigned char *flags)
{
    (void)unused;
    PlayerStatusSnapshot snap;
    snap.flags = (int16_t)(cmdHi >> 16);
    snap.cmdParam = (int)param;
    snap.errCode = errCode;
    buildStatusSnapshot(&snap, &m_offlineState);

    if (m_statusCb != NULL) {
        snap.seqNum = m_seqNum;
        snap.channelId = m_channelId;
        snap.playerKind = (unsigned char)m_playerKind;
        m_statusCb(&snap, m_userData, (*flags & 0x10) != 0);
    }
}

int BasePlayer::activateDecoder(int fadeMs, DecoderSession *decoder)
{
    if (m_decoder != NULL)
        return 0;

    m_activeState = m_offlineState;
    m_decoder = decoder;
    ++m_pendingSeekCount;

    int ret = decoder->activate(&m_offlineState, &m_metaBufs, fadeMs);
    if (ret != 0) {
        m_decoder = NULL;
    } else {
        resetMediaState(&m_offlineState);
        m_playParam = -1;
    }
    return ret;
}

int BasePlayer::deactivateDecoder(int fadeMs, uint32_t param, uint32_t cmdHi, unsigned char syncFlag)
{
    if (m_decoder == NULL)
        return 0;

    int ret = m_decoder->deactivate(fadeMs, param, cmdHi, syncFlag, 0);
    if (ret == 0)
        m_playParam = -1;
    return ret;
}

int BasePlayer::flushTimeCallback()
{
    if (m_extraCb == NULL)
        return 0;

    TimeReport rep;
    rep.channelId = m_channelId;
    rep.playerKind = (unsigned char)m_playerKind;

    int pos = (m_decoder != NULL)
                  ? decoderGetCurrentPos(m_decoder->m_stateMachine)
                  : m_offlineState.positionMs;
    if (pos == -1 || pos == -2 || pos == -3 || pos == -4)
        return 0;

    int64_t trackId = (m_decoder != NULL)
                          ? decoderGetCurrentTrackId(m_decoder->m_stateMachine)
                          : m_offlineState.trackId;
    if (trackId == -2)
        trackId = -1;

    rep.positionMs = pos;
    rep.trackId = trackId;
    return m_extraCb(&rep, m_userData);
}

int BasePlayer::getStatus(PlayerStatusSnapshot *outSnap, int *outPos, unsigned char *outPosValid)
{
    buildStatusSnapshot(outSnap, (m_decoder != NULL) ? &m_activeState : &m_offlineState);
    outSnap->playerKind = (unsigned char)m_playerKind;
    outSnap->channelId = m_channelId;
    outSnap->seqNum = m_seqNum;

    *outPosValid = 1;
    int pos = (m_decoder != NULL)
                  ? decoderGetCurrentPos(m_decoder->m_stateMachine)
                  : m_offlineState.positionMs;
    if (pos == -1 || pos == -2 || pos == -3 || pos == -4)
        *outPosValid = 0;
    else
        *outPos = pos;
    return 0;
}

int BasePlayer::formatUriInfo(char *outBuf, int *outMode, const void *uriWork, const DecoderMediaState *state)
{
    if (uriWork == NULL)
        return MUSICCORE_ERR_FAILED;

    const UriWorkHeader *hdr = (const UriWorkHeader *)uriWork;
    int mode = hdr->mode;
    *outMode = mode;

    if (mode == 1) {
        const UriWorkStrMode *strHdr = (const UriWorkStrMode *)uriWork;
        sce_paf_strncpy(outBuf, strHdr->uriPtr, 0x400u);
        outBuf[1024] = '\0';
        return 0;
    }
    if (mode == 2) {
        sce_paf_strncpy(outBuf, hdr->uriStr, 0x800u);
        outBuf[2048] = '\0';
        int *outWords = (int *)outBuf;
        outWords[513] = hdr->listIndex;
        outWords[514] = hdr->trackIndex;
        outWords[516] = hdr->trackIdLo;
        outWords[517] = hdr->trackIdHi;
        outWords[518] = (state->shuffleMode != 0) ? 1 : 0;
        outWords[519] = hdr->contentKind;
        outWords[520] = hdr->extraMeta;
        return 0;
    }
    if (mode == 3)
        return 0;
    return MUSICCORE_ERR_INVALID_ARG;
}

int BasePlayer::getExtraStatus(int *outMode, char onlyIfDirty, char *outBuf, unsigned char *outDirty)
{
    if (onlyIfDirty == 1 && m_uriDirty == 0) {
        *outMode = 4;
        *outDirty = 0;
        return 0;
    }
    if (m_lastUriWorkBuf == NULL) {
        *outMode = 4;
        return 0;
    }

    *outDirty = m_uriDirty;
    m_uriDirty = 0;
    void *region = workBufGetRegion(m_lastUriWorkBuf, 0);
    return formatUriInfo(outBuf, outMode, region, &m_offlineState);
}

void BasePlayer::tickStatus()
{
    PlayerStatusSnapshot snap;
    snap.flags = -1;
    snap.cmdParam = 0;
    snap.errCode = 0;
    buildStatusSnapshot(&snap, (m_decoder != NULL) ? &m_activeState : &m_offlineState);

    if (m_statusCb != NULL) {
        snap.seqNum = m_seqNum;
        snap.channelId = m_channelId;
        snap.playerKind = (unsigned char)m_playerKind;
        m_statusCb(&snap, m_userData, 0);
    }
}

int BasePlayer::cmdEnable(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3102 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    m_enabled = 1;
    if (prevWant != wantsAudio())
        *flags |= 4u;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdDisable(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3103 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    m_enabled = 0;
    int newWant = wantsAudio();
    if (prevWant != newWant)
        *flags |= 4u;
    if (newWant == 0 && m_decoder == NULL)
        *flags |= 8u;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdCancel(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3105 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    if (m_decoder != NULL) {
        int ret = m_decoder->cancel();
        *flags &= ~0x10u;
        return ret;
    }
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdPlay(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310A || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    int playParam = ((const PlayCmdPayload *)node->inBuf)->playParam;

    if (m_decoder != NULL) {
        int ret = m_decoder->play(playParam, node->param);
        if (ret == 0) {
            *flags &= ~0x10u;
            m_targetState = 4;
        }
        return ret;
    }

    int ret = 0;
    if (m_pendingDbOps == 0) {
        if (m_offlineState.mediaHandle != NULL)
            m_wantsPlay = 1;
        else
            ret = MUSICCORE_ERR_NO_MEDIA;
    }
    *flags |= 0x10u;
    if (ret == 0) {
        m_playParam = playParam;
        m_targetState = 4;
    }
    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::cmdPause(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310B || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    const PauseCmdPayload *pausePayload = (const PauseCmdPayload *)node->inBuf;
    unsigned char resetPos = pausePayload->resetPos;
    int pauseParam = pausePayload->pauseParam;
    m_savedTargetState = -1;

    int ret = 0;
    if (m_decoder != NULL) {
        ret = m_decoder->pause(resetPos, pauseParam);
        if (ret == 0) {
            *flags &= ~0x10u;
            m_targetState = 7;
        }
    } else {
        m_wantsPlay = 0;
        if (resetPos == 1)
            m_offlineState.positionMs = 0;
        m_playParam = pauseParam;
        *flags |= 0x10u;
        m_targetState = 7;
    }

    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::cmdStop(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310C || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    int ret = 0;
    if (m_decoder != NULL) {
        ret = m_decoder->stop();
        if (ret != 0)
            return ret;
        *flags &= ~0x10u;
    } else {
        m_wantsPlay = 0;
        *flags |= 0x10u;
    }

    m_targetState = 5;
    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::applyOfflineSeek(const int *seekParams, unsigned char *flags)
{
    if (seekParams == NULL)
        return MUSICCORE_ERR_FAILED;

    if (seekParams[1] != (int)0x80000000u)
        m_offlineState.volumeMode = seekParams[1];
    if (seekParams[2] != -2)
        m_offlineState.seekMode = seekParams[2];
    if (seekParams[3] != -2)
        m_offlineState.speedMode = seekParams[3];

    if (seekParams[0] == -1) {
        if (seekParams[4] != -2) {
            computePlaybackRange(m_offlineState.seekMode, m_offlineState.speedMode,
                                 seekParams[4], m_offlineState.durationMs,
                                 m_offlineState.volumeMode,
                                 &m_offlineState.currentTrackNum, &m_offlineState.totalTracks,
                                 &m_offlineState.positionMs);
        }
        *flags |= 0x10u;
        return 0;
    }

    int ret = dbManagerSeekMedia(m_dbMgr, m_offlineState.mediaHandle, seekParams[0]);
    if (ret == 0) {
        *flags &= ~0x10u;
        ++m_pendingSeekCount;
    }
    return ret;
}

int BasePlayer::applySeek(const int *seekParams, int playParam, uint32_t param, uint32_t cmdHi,
                          unsigned char *flags, char startPlay, unsigned char extraFlag)
{
    if (seekParams == NULL)
        return MUSICCORE_ERR_FAILED;

    if (seekParams[1] == 1)
        m_savedTargetState = -1;
    else if (seekParams[1] != (int)0x80000000u)
        m_savedTargetState = m_targetState;

    if (m_decoder != NULL) {
        int ret = (startPlay == 1)
                      ? m_decoder->seekAndPlay(seekParams, playParam)
                      : m_decoder->seek(seekParams, param, cmdHi, extraFlag);
        if (ret == 0)
            *flags &= ~0x10u;
        return ret;
    }

    if (m_offlineState.mediaHandle == NULL && m_pendingDbOps == 0)
        return MUSICCORE_ERR_NO_MEDIA;

    int ret = applyOfflineSeek(seekParams, flags);
    if (ret == 0 && startPlay == 1) {
        if (m_pendingDbOps == 0 && m_offlineState.mediaHandle != NULL)
            m_wantsPlay = 1;
        m_playParam = playParam;
        m_targetState = 4;
    }
    return ret;
}

int BasePlayer::cmdSeek(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310D || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                     ((uint32_t)node->cmdCode << 16);
    const int *seekParams = ((const SeekCmdPayload *)node->inBuf)->seekParams;
    return applySeek(seekParams, -1, node->param, cmdHi, flags, 0, 0);
}

int BasePlayer::cmdSetRepeat(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310E || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const int *payload = (const int *)node->inBuf;
    int repeatVal = payload[1];
    if (m_decoder != NULL) {
        uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                         ((uint32_t)node->cmdCode << 16);
        int ret = m_decoder->setRepeat(repeatVal, node->param, cmdHi, 0);
        *flags &= ~0x10u;
        return ret;
    }

    m_offlineState.repeatMode = repeatVal;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdSetShuffle(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x310F || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_dbMgr == NULL)
        return MUSICCORE_ERR_FAILED;

    const int *payload = (const int *)node->inBuf;
    int shuffleArg = payload[1];
    m_shuffleSeed = shuffleArg;

    if (m_decoder != NULL) {
        uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                         ((uint32_t)node->cmdCode << 16);
        int ret = m_decoder->setShuffle(shuffleArg, node->param, cmdHi, 0);
        *flags &= ~0x10u;
        return ret;
    }

    *flags |= 0x10u;
    if (m_offlineState.mediaHandle != NULL) {
        int ret = dbManagerSeekMedia(m_dbMgr, m_offlineState.mediaHandle, -1);
        if (ret == 0) {
            *flags &= ~0x10u;
            ++m_pendingSeekCount;
        }
        return ret;
    }
    return 0;
}

int BasePlayer::cmdSetUriData(CmdNode *node, unsigned char *flags)
{
    int ret = 0;
    const int *payload = (const int *)node->inBuf;

    if (node->cmdCode != 0x3111 || payload == NULL) {
        ret = MUSICCORE_ERR_INVALID_ARG;
    } else if (m_factory == NULL || m_dbMgr == NULL) {
        ret = MUSICCORE_ERR_FAILED;
    } else {
        if (m_playerKind == 1 && g_playlistDbIdState == 0) {
            g_playlistDbIdState = 1;
            dbManagerQueryDbId(m_dbMgr, g_playlistDbIdBuf, 1025);
        }

        int seedMode = payload[8];
        if (seedMode == 1 || m_shuffleSeed != 0) {
            SceKernelSysClock clockVal;
            sceKernelGetSystemTime(&clockVal);
            int seed = (int)clockVal.quad;
            if (clockVal.quad == 0)
                seed = 23841636;
            m_shuffleSeed = seed;
        } else if (seedMode != 0) {
            m_shuffleSeed = (m_decoder != NULL)
                                ? m_activeState.shuffleMode
                                : m_offlineState.shuffleMode;
        } else {
            m_shuffleSeed = 0;
        }

        ret = dbManagerSync(m_dbMgr);
        if (ret == 0) {
            uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                             ((uint32_t)node->cmdCode << 16);
            void *workBuf = (void *)(uintptr_t)payload[1];
            if (m_decoder != NULL && payload[9] != 1)
                ret = m_decoder->setMedia(NULL, node->param, cmdHi, m_shuffleSeed, 0, 1, 0);
            if (ret == 0) {
                ret = dbManagerRequestUri(m_dbMgr, 2, workBuf, m_shuffleSeed,
                                          node->param, cmdHi, 0, payload[9], m_mediaQueue);
            }
            if (ret != 0) {
                factoryDestroyWorkBuf(m_factory, workBuf);
            } else {
                ++m_pendingDbOps;
                if (payload[9] == 1)
                    m_targetState = 4;
                else if (payload[9] == 2)
                    m_targetState = 7;
                *flags &= ~0x10u;
            }
        }
    }

    if (ret != 0 && m_pendingUriCount > 0)
        --m_pendingUriCount;
    return ret;
}

int BasePlayer::cmdSetUriString(CmdNode *node, unsigned char *flags)
{
    int ret = 0;
    const int *payload = (const int *)node->inBuf;

    if (node->cmdCode != 0x3112 || payload == NULL) {
        ret = MUSICCORE_ERR_INVALID_ARG;
    } else if (m_factory == NULL || m_dbMgr == NULL) {
        ret = MUSICCORE_ERR_FAILED;
    } else {
        if (m_playerKind == 1 && g_playlistDbIdState == 0) {
            g_playlistDbIdState = 1;
            dbManagerQueryDbId(m_dbMgr, g_playlistDbIdBuf, 1025);
        }

        void *workBuf = (void *)(uintptr_t)payload[1];
        uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                         ((uint32_t)node->cmdCode << 16);
        if (m_decoder != NULL) {
            ret = m_decoder->setMedia(NULL, node->param, cmdHi, 0, 0, 0, 0);
            if (ret != 0) {
                factoryDestroyWorkBuf(m_factory, workBuf);
                if (m_pendingUriCount > 0)
                    --m_pendingUriCount;
                return ret;
            }
        } else {
            m_offlineState.sourceType = payload[4];
            *flags |= 0x10u;
        }

        ret = dbManagerRequestUri(m_dbMgr, 1, workBuf, m_offlineState.shuffleMode,
                                  node->param, cmdHi, 0, -1, m_mediaQueue);
        if (ret != 0) {
            factoryDestroyWorkBuf(m_factory, workBuf);
        } else {
            ++m_pendingDbOps;
            *flags &= ~0x10u;
        }
    }

    if (ret != 0 && m_pendingUriCount > 0)
        --m_pendingUriCount;
    return ret;
}

int BasePlayer::cmdPrepareDb(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3113 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_factory == NULL || m_dbMgr == NULL)
        return MUSICCORE_ERR_FAILED;

    if (m_playerKind == 1 && g_playlistDbIdState == 0) {
        g_playlistDbIdState = 1;
        dbManagerQueryDbId(m_dbMgr, g_playlistDbIdBuf, 1025);
    }
    *flags &= ~0x10u;
    return 0;
}

int BasePlayer::cmdUnsupported()
{
    return MUSICCORE_ERR_INVALID_ARG;
}

int BasePlayer::cmdSetAudioBalance(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3114 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const int *payload = (const int *)node->inBuf;
    if (m_decoder != NULL) {
        int ret = m_decoder->setVolume(payload[1], payload[2]);
        *flags &= ~0x10u;
        return ret;
    }

    m_offlineState.audioBalance = payload[1];
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdSetEqualizer(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3115 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const int *payload = (const int *)node->inBuf;
    if (m_decoder != NULL) {
        int ret = m_decoder->setEqualizer();
        if (ret == 0)
            *flags &= ~0x10u;
        return ret;
    }

    m_offlineState.eqType = payload[1];
    m_offlineState.eqBandLow = payload[2];
    m_offlineState.eqBandMid = payload[3];
    m_offlineState.eqBandHigh = payload[4];
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdSetAudioNormalizer(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3116 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    const int *payload = (const int *)node->inBuf;
    int mode = payload[1];
    if (m_decoder != NULL) {
        uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                         ((uint32_t)node->cmdCode << 16);
        int ret = m_decoder->setAudioNormalizer(mode, node->param, cmdHi, 0);
        if (ret == 0)
            *flags &= ~0x10u;
        return ret;
    }

    m_offlineState.audioNormalizer = mode;
    *flags |= 0x10u;
    return 0;
}

int BasePlayer::cmdSeekAndPlay(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3117 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    const int *payload = (const int *)node->inBuf;
    uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                     ((uint32_t)node->cmdCode << 16);
    int ret = applySeek(&payload[1], payload[6], node->param, cmdHi, flags, 1, 0);
    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::cmdNotifyStatus(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3118 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    *flags |= 0x10u;
    if (prevWant != wantsAudio())
        *flags |= 4u;
    return 0;
}

int BasePlayer::cmdResetSpeedReq(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3119 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    uint32_t cmdHi = (uint32_t)node->targetChannel | ((uint32_t)node->flagsByte << 8) |
                     ((uint32_t)node->cmdCode << 16);
    int ret = cmdResetSpeed(node->param, cmdHi, flags);
    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::cmdRefreshDbId(CmdNode *node)
{
    if (node->cmdCode != 0x311B || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    if (m_playerKind == 1) {
        g_playlistDbIdState = 1;
        dbManagerQueryDbId(m_dbMgr, g_playlistDbIdBuf, 1025);
    }
    return 0;
}

void BasePlayer::clearPendingUriIfMatches(MediaItemHandle *media)
{
    if (media == NULL)
        return;
    void *workBuf = media->getWorkBuf();
    if (workBuf != NULL && m_lastUriWorkBuf != NULL) {
        if (*((int *)workBuf + 6) == *((int *)m_lastUriWorkBuf + 6)) {
            m_uriDirty = 0;
            m_lastUriWorkBuf = NULL;
        }
    }
}

int BasePlayer::onDecoderEvent(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3200 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_dbMgr == NULL)
        return MUSICCORE_ERR_FAILED;

    struct DecoderEventPayload {
        int headerWord;
        uint16_t flagsWord;
        uint16_t subCode;
        int errCode;
        int padErr;
        DecoderMediaState state;
        uint32_t origParam;
        uint8_t origTarget;
        uint8_t origFlags;
        uint16_t origCmdCode;
        unsigned char dbFlag;
    };
    const DecoderEventPayload *ev = (const DecoderEventPayload *)node->inBuf;
    int prevWant = wantsAudio();
    int ret = 0;

    if ((ev->origFlags & 0x80) == 0)
        *flags |= 0x10u;

    uint16_t subCode = ev->subCode;
    if (subCode == 0x1204) {
        if (ev->state.hasPendingError != 0) {
            unsigned char retryPlay = 0;
            onPlaybackError(ev->state.lastErrorType, flags, &retryPlay);
        }
    } else if (subCode == 0x1205) {
        if (m_offlineState.mediaHandle != NULL) {
            MediaItemHandle *oldMedia = (MediaItemHandle *)m_offlineState.mediaHandle;
            clearPendingUriIfMatches(oldMedia);
            m_offlineState.mediaHandle = NULL;
            uint64_t cmdHdr = ((uint64_t)((uint32_t)node->targetChannel |
                                          ((uint32_t)node->flagsByte << 8) |
                                          ((uint32_t)node->cmdCode << 16)) << 32) |
                              node->param;
            ret = dbManagerReleaseMedia(m_dbMgr, oldMedia, cmdHdr);
        }

        m_offlineState = m_activeState;
        resetMediaState(&m_activeState);

        if (m_isOpen == 0) {
            if (m_offlineState.mediaHandle != NULL) {
                MediaItemHandle *oldMedia = (MediaItemHandle *)m_offlineState.mediaHandle;
                clearPendingUriIfMatches(oldMedia);
                m_offlineState.mediaHandle = NULL;
                ret = dbManagerReleaseMedia(m_dbMgr, oldMedia, node->param);
            }
            resetMediaState(&m_offlineState);
            m_targetState = 7;
            m_pendingFadeMs = -1;
            m_playParam = -1;
        }

        m_decoder = NULL;
        if (m_pendingSeekCount > 0)
            --m_pendingSeekCount;
        if (ev->errCode == 0)
            *flags |= 8u;
    } else if (subCode == 0x1201) {
        unsigned char retryPlay = 0;
        onPlaybackError(ev->state.lastErrorType, flags, &retryPlay);
        if (retryPlay == 1 && m_decoder != NULL)
            ret = m_decoder->play(-1, ev->origParam);
        if (m_isSuspended == 1) {
            *flags |= 0x40u;
            if (ev->origCmdCode == 0x8108)
                *flags |= 0x80u;
            else
                *flags &= ~0x80u;
            m_isSuspended = 0;
        }
    } else if (subCode > 0x1110 && subCode <= 0x1112) {
        if (ev->errCode != 0 && ev->state.extraErr != 4) {
            m_savedTargetState = -1;
            m_wantsPlay = 0;
            m_targetState = 7;
        }
    } else if (subCode == 0x110F) {
        if (ev->dbFlag & 1)
            *flags |= 0x80u;
        else
            *flags &= ~0x80u;

        if (m_isSuspended == 1) {
            if (ev->errCode == 0 && ev->state.stateId == 6) {
                *flags &= ~0x40u;
                m_isSuspended = 1;
            } else {
                *flags |= 0x40u;
                m_isSuspended = 0;
            }
        }
    } else if (subCode == 0x1106) {
        if (ev->errCode == 0 && ev->state.stateId == 7) {
            if (m_targetState == 7 || m_targetState == 5)
                m_wantsPlay = 0;
        }
    } else if (subCode == 0x1105) {
        if (ev->errCode != 0) {
            m_wantsPlay = 0;
            m_targetState = 7;
        }
    } else if (subCode == 0x1102) {
        flags[1] |= 1u;
    }

    if (m_decoder != NULL) {
        m_activeState = ev->state;
        if (m_activeState.hasPendingError != 0)
            m_activeState.hasPendingError = 0;
    }

    int forceNotify = 0;
    if ((*flags & 0x10) != 0) {
        *flags &= ~0x10u;
        forceNotify = 1;
    }

    flushTimeCallback();

    PlayerStatusSnapshot snap;
    snap.flags = (int16_t)ev->origCmdCode;
    snap.cmdParam = (int)ev->origParam;
    snap.errCode = ev->errCode;
    buildStatusSnapshot(&snap, (m_decoder != NULL) ? &ev->state : &m_offlineState);
    snap.playerKind = (unsigned char)m_playerKind;
    snap.channelId = m_channelId;
    snap.seqNum = m_seqNum;
    if (m_statusCb != NULL)
        m_statusCb(&snap, m_userData, forceNotify);

    if (prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::onDecoderTrackReady(CmdNode *node)
{
    if (node->cmdCode != 0x3201 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_decoder != NULL)
        return m_decoder->onTrackReady();
    return 0;
}

int BasePlayer::onDecoderTrackInfo(CmdNode *node)
{
    if (node->cmdCode != 0x3203 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    struct TrackInfoMsg {
        int header[8];
        int64_t playlistId;
        int64_t trackId;
        int trackItemWords[3];
        int trackIndex;
        int positionMs;
        int currentTrackNum;
        int totalTracks;
        int durationMs;
        int sampleRate;
        int bitRate;
        int channels;
        int codecType;
        int playbackSettings[5];
        int volumeMode;
        int audioEffectSettings[6];
        int sourceType;
    };
    const TrackInfoMsg *msg = (const TrackInfoMsg *)node->inBuf;
    if (msg->playlistId != -1) {
        m_activeState.playlistId = msg->playlistId;
        m_activeState.trackId = msg->trackId;
        m_activeState.trackIndex = msg->trackIndex;
        m_activeState.positionMs = msg->positionMs;
        m_activeState.currentTrackNum = msg->currentTrackNum;
        m_activeState.totalTracks = msg->totalTracks;
        m_activeState.durationMs = msg->durationMs;
        m_activeState.sampleRate = msg->sampleRate;
        m_activeState.bitRate = msg->bitRate;
        m_activeState.channels = msg->channels;
        m_activeState.codecType = msg->codecType;
        m_activeState.sourceType = msg->sourceType;
        m_activeState.volumeMode = msg->volumeMode;
    }

    if (m_decoder != NULL)
        return m_decoder->onTrackInfo();
    return 0;
}

int BasePlayer::onDecoderAudioError(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3204 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    int prevWant = wantsAudio();
    const int *payload = (const int *)node->inBuf;
    int errArg = payload[2];
    int ret = 0;

    if (m_decoder != NULL) {
        ret = m_decoder->onAudioError(errArg, node->param);
    } else {
        if (m_capCb != NULL) {
            CapReport rep;
            rep.channelId = m_channelId;
            rep.playerKind = (unsigned char)m_playerKind;
            rep.code = errArg;
            m_capCb(&rep, m_userData);
        }
        m_wantsPlay = 0;
        m_targetState = 7;
    }

    if (ret == 0 && prevWant != wantsAudio())
        *flags |= 4u;
    return ret;
}

int BasePlayer::onDecoderBufferReady(CmdNode *node)
{
    if (node->cmdCode != 0x3205 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_decoder != NULL)
        return m_decoder->onBufferReady();
    return 0;
}

int BasePlayer::onDecoderStreamEnd(CmdNode *node)
{
    if (node->cmdCode != 0x3206 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_decoder != NULL)
        return m_decoder->onStreamEnd();
    return 0;
}

int BasePlayer::onDecoderMediaChanged(CmdNode *node)
{
    if (node->cmdCode != 0x3207 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_dbMgr == NULL)
        return MUSICCORE_ERR_FAILED;

    struct DecoderMediaUpdate {
        uint32_t origParam;
        uint32_t origCmdHi;
        int padHeader;
        unsigned char hasNewMedia;
        unsigned char padHasNew[3];
        MediaItemHandle *newMedia;
        unsigned char hasUriFlag;
        unsigned char padHasUri[3];
        DecoderMediaState newState;
        MediaItemHandle *oldMedia;
        unsigned char decrementUri;
    };
    const DecoderMediaUpdate *upd = (const DecoderMediaUpdate *)node->inBuf;
    int ret = 0;

    if (upd->oldMedia != NULL) {
        if (upd->decrementUri == 1) {
            if (m_pendingUriCount > 0)
                --m_pendingUriCount;
            if (m_pendingUriCount == 0)
                m_pendingUriCount = -1;
        }
        clearPendingUriIfMatches(upd->oldMedia);
        if (m_offlineState.mediaHandle == upd->oldMedia)
            m_offlineState.mediaHandle = NULL;
        uint64_t cmdHdr = ((uint64_t)((uint32_t)node->targetChannel |
                                      ((uint32_t)node->flagsByte << 8) |
                                      ((uint32_t)node->cmdCode << 16)) << 32) |
                          node->param;
        ret = dbManagerReleaseMedia(m_dbMgr, upd->oldMedia, cmdHdr);
    }

    if (upd->hasNewMedia == 1) {
        if (upd->hasUriFlag == 1 && m_pendingUriCount > 0)
            --m_pendingUriCount;

        if (upd->newMedia != NULL) {
            void *workBuf = upd->newMedia->getWorkBuf();
            m_lastUriWorkBuf = workBuf;
            if (workBuf != NULL)
                m_uriDirty = 1;
        }

        if (m_metadataCb != NULL && upd->newMedia != NULL) {
            void *workBuf = upd->newMedia->getWorkBuf();
            if (workBuf != NULL) {
                void *region = workBufGetRegion(workBuf, 0);
                int mode = 4;
                char uriBuf[2088];
                if (formatUriInfo(uriBuf, &mode, region, &upd->newState) == 0) {
                    PlayerStatusSnapshot snap;
                    snap.flags = (int16_t)(upd->origCmdHi >> 16);
                    snap.cmdParam = (int)upd->origParam;
                    snap.errCode = 0;
                    buildStatusSnapshot(&snap, &upd->newState);
                    snap.channelId = m_channelId;
                    snap.playerKind = (unsigned char)m_playerKind;
                    snap.seqNum = m_seqNum;
                    m_metadataCb(mode, uriBuf, &snap, m_userData);
                }
            }
        }
        return 0;
    }

    return ret;
}

int BasePlayer::onPlaybackError(int errType, unsigned char *flags, unsigned char *outRetryPlay)
{
    (void)flags;
    if (errType == 3 || errType == 4 || errType == 5) {
        if (m_targetState != 7 && m_targetState != 5)
            return 0;
    } else if (errType == 7) {
        if (m_savedTargetState == 4) {
            *outRetryPlay = 1;
            return 0;
        }
    } else if (errType != 1 && errType != 2 && errType != 6) {
        return 0;
    }

    if (m_capCb != NULL) {
        CapReport rep;
        rep.channelId = m_channelId;
        rep.playerKind = (unsigned char)m_playerKind;
        rep.code = 0;
        m_capCb(&rep, m_userData);
    }
    m_wantsPlay = 0;
    m_targetState = 7;
    return 0;
}

int BasePlayer::getContentInfo(MetadataBuffers *outMeta)
{
    if (outMeta == NULL)
        return MUSICCORE_ERR_INVALID_ARG;

    if (m_metaMutexPtr != NULL)
        m_metaMutexPtr->Lock();
    if (outMeta->titleBuf != NULL && outMeta->titleLen != 0)
        outMeta->titleBuf[0] = '\0';
    if (outMeta->artistBuf != NULL && outMeta->artistLen != 0)
        outMeta->artistBuf[0] = '\0';
    if (outMeta->albumBuf != NULL && outMeta->albumLen != 0)
        outMeta->albumBuf[0] = '\0';
    if (m_metaMutexPtr != NULL)
        m_metaMutexPtr->Unlock();
    return 0;
}

int BasePlayer::onDbEvent(CmdNode *node, unsigned char *flags)
{
    if (node->cmdCode != 0x3400 || node->inBuf == NULL)
        return MUSICCORE_ERR_INVALID_ARG;
    if (m_dbMgr == NULL)
        return MUSICCORE_ERR_FAILED;

    struct DbEventPayload {
        int headerWord;
        uint16_t flagsWord;
        uint16_t subCode;
        int errCode;
        int padErr;
        uint32_t origParam;
        uint16_t origFlags;
        uint16_t origCmdCode;
        MediaItemHandle *media;
        void *workBuf;
        unsigned char dbFlag;
        unsigned char padTrackHeader[23];
        int64_t playlistId;
        int64_t trackId;
        int64_t trackItemId;
        char trackPathAndFlags[2052];
        int sampleRate;
        int codecType;
        int bitRate;
        int channels;
        int durationMs;
        int trackPlayStats[4];
        int sourceType;
        int trackMediaRefs[5];
        int seekStep;
        int trackIndex;
        int64_t savedPlaylistId;
        int seekArgFirst;
        int seekArgSecond;
        uint32_t seekTarget;
        int repeatMode;
        unsigned char isNewMedia;
        unsigned char padNewMedia[7];
        int shuffleMode;
        int playMode;
        int audioNormalizer;
        int eqType;
        int eqBands[3];
        int audioBalance;
        int dbStatus;
    };
    DbEventPayload *ev = (DbEventPayload *)node->inBuf;

    if ((ev->origFlags & 0x8000) == 0)
        *flags |= 0x10u;

    int ret = 0;
    uint16_t subCode = ev->subCode;

    if (subCode == 0x21B0 || (subCode >= 0x21A1 && subCode <= 0x21A5)) {
        ret = 0;
    } else if (subCode == 0x21A0) {
        if (m_playerKind == 1) {
            m_offlineState.repeatMode = ev->repeatMode;
            m_offlineState.shuffleMode = ev->shuffleMode;
            m_offlineState.audioNormalizer = ev->audioNormalizer;
            m_offlineState.eqType = ev->eqType;
            m_offlineState.audioBalance = ev->audioBalance;
            ret = dbManagerRefresh(m_dbMgr);
        }
    } else if (subCode == 0x2180) {
        if (ev->errCode == 0)
            g_playlistDbIdState = 2;
        *flags &= ~0x10u;
    } else if (subCode == 0x2101) {
        if (ev->errCode == 0 && ev->workBuf != NULL) {
            void *wb = ev->workBuf;
            ev->workBuf = NULL;
            if (m_factory != NULL)
                factoryDestroyWorkBuf(m_factory, wb);
        }
    } else if (subCode == 0x2100) {
        if (ev->errCode != 0) {
            if (ev->workBuf != NULL && m_factory != NULL) {
                factoryDestroyWorkBuf(m_factory, ev->workBuf);
                ev->workBuf = NULL;
            }
            if (m_pendingUriCount > 0)
                --m_pendingUriCount;
        }

        if (m_decoder != NULL) {
            if (m_pendingDbOps != 0)
                --m_pendingDbOps;
            void *media = (ev->errCode == 0) ? (void *)ev->media : NULL;
            int isAsync = ((ev->origFlags & 0x8000) != 0) ? 1 : 0;
            uint32_t origHi = (uint32_t)ev->origFlags | ((uint32_t)ev->origCmdCode << 16);

            if (ev->playMode == 1 && m_targetState == 4) {
                ret = m_decoder->setMediaPlaying(media, ev->origParam, origHi, m_shuffleSeed, isAsync);
            } else if (ev->playMode == 2) {
                int pauseFlag = (m_targetState != 4) ? 1 : 0;
                ret = m_decoder->setMedia(media, ev->origParam, origHi, m_shuffleSeed, isAsync, 0, pauseFlag);
            } else {
                ret = m_decoder->setMedia(media, ev->origParam, origHi, m_shuffleSeed, isAsync, 0, 0);
            }

            if (ret != 0) {
                if (ev->media != NULL) {
                    clearPendingUriIfMatches(ev->media);
                    if (m_offlineState.mediaHandle == ev->media)
                        m_offlineState.mediaHandle = NULL;
                    dbManagerReleaseMedia(m_dbMgr, ev->media, node->param);
                }
                if (m_pendingUriCount > 0)
                    --m_pendingUriCount;
            } else {
                *flags &= ~0x10u;
            }
        } else {
            ret = ev->errCode;
            if (ret == 0 && ev->media != NULL) {
                (void)ev->media->getTrackKey();
                ret = dbManagerSeekMedia(m_dbMgr, ev->media, -2);
                if (ret != 0) {
                    clearPendingUriIfMatches(ev->media);
                    if (m_offlineState.mediaHandle == ev->media)
                        m_offlineState.mediaHandle = NULL;
                    dbManagerReleaseMedia(m_dbMgr, ev->media, node->param);
                } else {
                    *flags &= ~0x10u;
                    ++m_pendingSeekCount;
                }
            }
            if (ret != 0) {
                if (m_pendingDbOps != 0)
                    --m_pendingDbOps;
                if (m_pendingDbOps == 0 && m_offlineState.mediaHandle == NULL)
                    m_targetState = 7;
                if (m_pendingUriCount > 0)
                    --m_pendingUriCount;
            }
        }
    } else if (subCode == 0x2102) {
        if (m_pendingSeekCount > 0)
            --m_pendingSeekCount;

        if (m_decoder != NULL) {
            if ((ev->seekTarget >> 16) == 0xFFFCu) {
                m_isSuspended = 1;
                ret = m_decoder->suspendForDb();
                if (ret != 0) {
                    *flags |= 0x40u;
                    if ((ev->seekTarget & 0xFFFFu) != 0)
                        *flags |= 0x80u;
                    else
                        *flags &= ~0x80u;
                    m_isSuspended = 0;
                    if (m_pendingUriCount > 0)
                        --m_pendingUriCount;
                }
            }
        } else {
            ret = ev->errCode;
            if (ev->media != NULL) {
                if ((ev->seekTarget >> 16) == 0xFFFCu) {
                    *flags |= 0x40u;
                    if ((ev->seekTarget & 0xFFFFu) != 0)
                        *flags |= 0x80u;
                    else
                        *flags &= ~0x80u;
                    m_isSuspended = 0;
                }

                if (ev->isNewMedia == 1) {
                    if (m_pendingDbOps != 0)
                        --m_pendingDbOps;

                    MediaItemHandle *toRelease = ev->media;
                    if (ev->errCode != 0) {
                        if (m_pendingUriCount > 0)
                            --m_pendingUriCount;
                    } else {
                        toRelease = (MediaItemHandle *)m_offlineState.mediaHandle;
                        m_offlineState.mediaHandle = ev->media;
                        m_offlineState.lastErrorType = 0;
                        m_offlineState.playState = ev->media->getPlayState();
                        m_offlineState.playlistId = ev->playlistId;
                        m_offlineState.trackId = ev->trackId;
                        m_offlineState.trackItemId = ev->trackItemId;
                        m_offlineState.trackOrderIndex = ev->media->getTrackIndex();
                        m_offlineState.trackIndex = ev->trackIndex;
                        m_offlineState.durationMs = ev->durationMs;
                        m_offlineState.sampleRate = ev->sampleRate;
                        m_offlineState.bitRate = ev->bitRate;
                        m_offlineState.channels = ev->channels;
                        m_offlineState.codecType = ev->codecType;
                        m_offlineState.sourceType = ev->sourceType;
                        m_offlineState.shuffleMode = ev->shuffleMode;
                        computePlaybackRange(m_offlineState.seekMode, m_offlineState.speedMode,
                                             -1, ev->durationMs, m_offlineState.volumeMode,
                                             &m_offlineState.currentTrackNum, &m_offlineState.totalTracks,
                                             &m_offlineState.positionMs);

                        void *workBuf = ev->media->getWorkBuf();
                        m_lastUriWorkBuf = workBuf;
                        if (workBuf != NULL)
                            m_uriDirty = 1;

                        if (m_metadataCb != NULL && workBuf != NULL) {
                            void *region = workBufGetRegion(workBuf, 0);
                            int mode = 4;
                            char uriBuf[2088];
                            if (formatUriInfo(uriBuf, &mode, region, &m_offlineState) == 0) {
                                PlayerStatusSnapshot snap;
                                snap.flags = (int16_t)ev->origCmdCode;
                                snap.cmdParam = (int)ev->origParam;
                                snap.errCode = 0;
                                buildStatusSnapshot(&snap, &m_offlineState);
                                snap.channelId = m_channelId;
                                snap.playerKind = (unsigned char)m_playerKind;
                                snap.seqNum = m_seqNum;
                                m_metadataCb(mode, uriBuf, &snap, m_userData);
                            }
                        }

                        ev->media->populateMetadata(&m_metaBufs);
                        if (m_pendingUriCount > 0)
                            --m_pendingUriCount;
                    }

                    bool releaseFailed = false;
                    if (toRelease != NULL) {
                        clearPendingUriIfMatches(toRelease);
                        if (m_offlineState.mediaHandle == toRelease)
                            m_offlineState.mediaHandle = NULL;
                        uint64_t cmdHdr = ((uint64_t)((uint32_t)node->targetChannel |
                                                      ((uint32_t)node->flagsByte << 8) |
                                                      ((uint32_t)node->cmdCode << 16)) << 32) |
                                          node->param;
                        if (dbManagerReleaseMedia(m_dbMgr, toRelease, cmdHdr) != 0)
                            releaseFailed = true;
                    }
                    if (releaseFailed) {
                        ev->isNewMedia = 0;
                    }
                } else if (ret == 0 && ev->media == m_offlineState.mediaHandle) {
                    if (ev->dbStatus == MUSICCORE_ERR_DB_REMOVED) {
                        clearPendingUriIfMatches(ev->media);
                        m_offlineState.mediaHandle = NULL;
                        uint64_t cmdHdr = ((uint64_t)((uint32_t)node->targetChannel |
                                                      ((uint32_t)node->flagsByte << 8) |
                                                      ((uint32_t)node->cmdCode << 16)) << 32) |
                                          node->param;
                        ret = dbManagerReleaseMedia(m_dbMgr, ev->media, cmdHdr);
                        resetMediaState(&m_offlineState);
                        getContentInfo(&m_metaBufs);
                    } else if (ev->dbStatus == 0) {
                        m_offlineState.lastErrorType = 0;
                        if (m_offlineState.playlistId != ev->playlistId)
                            ev->media->populateMetadata(&m_metaBufs);
                        m_offlineState.playlistId = ev->playlistId;
                        m_offlineState.trackId = ev->trackId;
                        m_offlineState.trackItemId = ev->trackItemId;
                        m_offlineState.trackOrderIndex = ev->media->getTrackIndex();
                        m_offlineState.trackIndex = ev->trackIndex;
                        m_offlineState.durationMs = ev->durationMs;
                        m_offlineState.sampleRate = ev->sampleRate;
                        m_offlineState.bitRate = ev->bitRate;
                        m_offlineState.channels = ev->channels;
                        m_offlineState.codecType = ev->codecType;
                        m_offlineState.sourceType = ev->sourceType;
                        m_offlineState.shuffleMode = ev->shuffleMode;
                        computePlaybackRange(m_offlineState.seekMode, m_offlineState.speedMode,
                                             ev->seekStep, ev->durationMs, m_offlineState.volumeMode,
                                             &m_offlineState.currentTrackNum, &m_offlineState.totalTracks,
                                             &m_offlineState.positionMs);
                    }
                }
            } else if (m_offlineState.mediaHandle != NULL) {
                ret = dbManagerSeekMedia(m_dbMgr, m_offlineState.mediaHandle, (int)ev->seekTarget);
                if (ret != 0) {
                    if (ev->isNewMedia == 1 && m_pendingDbOps != 0)
                        --m_pendingDbOps;
                    if ((ev->seekTarget >> 16) == 0xFFFCu) {
                        *flags |= 0x40u;
                        if ((ev->seekTarget & 0xFFFFu) != 0)
                            *flags |= 0x80u;
                        else
                            *flags &= ~0x80u;
                        m_isSuspended = 0;
                    }
                } else {
                    *flags &= ~0x10u;
                    ++m_pendingSeekCount;
                }
            }

            if (ev->isNewMedia == 1 && m_pendingDbOps == 0) {
                if (m_offlineState.mediaHandle != NULL && ret == 0) {
                    int prevWant = wantsAudio();
                    m_wantsPlay = (m_targetState == 4) ? 1 : 0;
                    if (prevWant != wantsAudio())
                        *flags |= 4u;
                } else {
                    m_targetState = 7;
                }
            }
        }
    } else {
        ret = MUSICCORE_ERR_INVALID_ARG;
    }

    if (ev->dbFlag == 1) {
        *flags = (*flags & ~0x80u) | 0x20u;
    }
    int forceNotify = 0;
    if ((*flags & 0x10) != 0) {
        *flags &= ~0x10u;
        forceNotify = 1;
    }

    flushTimeCallback();

    PlayerStatusSnapshot snap;
    snap.flags = (int16_t)ev->origCmdCode;
    snap.cmdParam = (int)ev->origParam;
    snap.errCode = ev->errCode;
    buildStatusSnapshot(&snap, (m_decoder != NULL) ? &m_activeState : &m_offlineState);
    if (m_statusCb != NULL) {
        snap.seqNum = m_seqNum;
        snap.channelId = m_channelId;
        snap.playerKind = (unsigned char)m_playerKind;
        m_statusCb(&snap, m_userData, forceNotify);
    }
    return ret;
}

int BasePlayer::cmdResetSpeed(uint32_t param, uint32_t cmdHi, unsigned char *flags)
{
    int seekParams[5];
    seekParams[0] = -1;
    seekParams[1] = 1;
    seekParams[2] = -2;
    seekParams[3] = -2;
    seekParams[4] = -2;

    if (m_savedTargetState != 7)
        return applySeek(seekParams, -1, param, cmdHi, flags, 0, 0);

    applySeek(seekParams, -1, param, cmdHi, flags, 0, 1);
    m_savedTargetState = -1;

    int ret = 0;
    if (m_decoder != NULL) {
        ret = m_decoder->pause(0, -1);
        if (ret != 0)
            return ret;
        *flags &= ~0x10u;
    } else {
        m_wantsPlay = 0;
        m_playParam = -1;
        *flags |= 0x10u;
    }
    m_targetState = 7;
    return ret;
}

struct PlayerCommandPacket;
struct TrackMetadataBuffers;

size_t getPlayerBasePoolSize(void *heap)
{
    (void)heap;
    return BasePlayer::calcBaseExtraSize(1);
}

void playerBaseConstruct(void *self, int playerType, void *controller,
                         void *dbMgr, int *outErr, unsigned int slotSize, int hdrSize)
{
    (void)dbMgr;
    new (self) BasePlayer(static_cast<char>(playerType), controller, 1, outErr,
                          slotSize, static_cast<unsigned int>(hdrSize));
}

void playerBaseDestroy(void *self)
{
    if (self != NULL)
        static_cast<BasePlayer *>(self)->~BasePlayer();
}

int playerBaseStopAndReset(void *self)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->close(&flags);
}

int playerBaseClearActiveList(void *self)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdStop(NULL, &flags);
}

void playerBaseResetMetadataStrings(void *self, TrackMetadataBuffers *meta)
{
    static_cast<BasePlayer *>(self)->getContentInfo(reinterpret_cast<MetadataBuffers *>(meta));
}

int playerBaseHandleSeek(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdSeek(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseHandleSkipNext(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdSeekAndPlay(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseHandleSkipPrev(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdCancel(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseHandleSetRepeat(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdSetRepeat(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseHandleSetShuffle(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdSetShuffle(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseOnTrackEvent(void *self, const PlayerCommandPacket *cmd)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->onDecoderEvent(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

int playerBaseResumeOrStart(void *self)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdPlay(NULL, &flags);
}

int playerBasePause(void *self)
{
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdPause(NULL, &flags);
}

int playerBaseHandleNotify(void *self, const PlayerCommandPacket *cmd, int extra)
{
    (void)extra;
    unsigned char flags = 0;
    return static_cast<BasePlayer *>(self)->cmdNotifyStatus(
        reinterpret_cast<CmdNode *>(const_cast<PlayerCommandPacket *>(cmd)), &flags);
}

unsigned int basePlayerCalcExtraSize(int capacity)
{
    return BasePlayer::calcBaseExtraSize(capacity);
}

void basePlayerConstruct(void *self, char kind, void *factory, int capacity,
                         int *outErr, unsigned int slotSize, unsigned int hdrSize)
{
    new (self) BasePlayer(kind, factory, capacity, outErr, slotSize, hdrSize);
}

void basePlayerDestruct(void *self)
{
    if (self != NULL)
        static_cast<BasePlayer *>(self)->~BasePlayer();
}

int playerDispatchCmd(BasePlayer *player, CmdNode *node, unsigned char *flags)
{
    return (player != NULL) ? player->dispatchCmd(node, flags) : -1;
}

void playerNotifyStatus(BasePlayer *player, int unused, uint32_t param, uint32_t cmdHi,
                        int errCode, unsigned char *flags)
{
    if (player != NULL)
        player->notifyStatus(unused, param, cmdHi, errCode, flags);
}

int playerActivateDecoder(BasePlayer *player, int fadeMs, void *decoderMgr)
{
    return (player != NULL)
        ? player->activateDecoder(fadeMs, static_cast<DecoderSession *>(decoderMgr))
        : -1;
}

int playerDeactivateDecoder(BasePlayer *player, int fadeMs, uint32_t param, uint32_t cmdHi,
                            unsigned char syncFlag)
{
    return (player != NULL) ? player->deactivateDecoder(fadeMs, param, cmdHi, syncFlag) : -1;
}

int playerFlushStatus(BasePlayer *player)
{
    return (player != NULL) ? player->flushTimeCallback() : -1;
}

int playerGetStatus(BasePlayer *player, void *statusBuf, void *outPos, void *outPosValid)
{
    return (player != NULL)
        ? player->getStatus(static_cast<PlayerStatusSnapshot *>(statusBuf),
                            static_cast<int *>(outPos), static_cast<unsigned char *>(outPosValid))
        : -1;
}

int playerGetExtraStatus(BasePlayer *player, int *outMode, int flag, void *workBuf,
                         unsigned char *outDirty)
{
    return (player != NULL)
        ? player->getExtraStatus(outMode, static_cast<char>(flag),
                                 static_cast<char *>(workBuf), outDirty)
        : -1;
}

void playerTick(BasePlayer *player)
{
    if (player != NULL)
        player->tickStatus();
}

int playerSetSuspended(BasePlayer *player, int suspended)
{
    return (player != NULL) ? player->setSuspended(static_cast<char>(suspended)) : -1;
}

unsigned int calcMediaQueueSize(int capacity, int itemSize, int slotCount, int headerSize)
{
    return static_cast<unsigned int>(capacity * itemSize * slotCount + headerSize);
}

void computePlaybackRange(int repeatMode, int shuffleMode, int step, int totalTracks,
                          int currentIndex, int *outNextIndex, int *outWrapped, int *outValid)
{
    (void)shuffleMode;
    int nextIdx = currentIndex + step;
    int wrapped = 0;
    int valid = 1;
    if (totalTracks <= 0) {
        nextIdx = 0;
        valid = 0;
    } else if (nextIdx < 0) {
        if (repeatMode != 0) {
            nextIdx = totalTracks - 1;
            wrapped = 1;
        } else {
            nextIdx = 0;
            valid = 0;
        }
    } else if (nextIdx >= totalTracks) {
        if (repeatMode != 0) {
            nextIdx = 0;
            wrapped = 1;
        } else {
            nextIdx = totalTracks - 1;
            valid = 0;
        }
    }
    if (outNextIndex != NULL)
        *outNextIndex = nextIdx;
    if (outWrapped != NULL)
        *outWrapped = wrapped;
    if (outValid != NULL)
        *outValid = valid;
}

} /* namespace MusicCore */
} /* namespace sce */
