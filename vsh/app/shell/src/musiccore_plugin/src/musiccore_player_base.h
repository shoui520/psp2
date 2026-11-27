/*
 * SceShell musiccore_plugin - shared BasePlayer hierarchy and state structures.
 * Layout verified against 3.65-CEX BasePlayer (0x81532EBC, 472 bytes, 58 slots),
 * MusicPlayer (0x81532FAC), ExclusiveMusicPlayer (0x815337F8), and
 * MusicAppPlayer (0x815338E8).
 */

#ifndef SHELL_MUSICCORE_PLAYER_BASE_H
#define SHELL_MUSICCORE_PLAYER_BASE_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

#include <thread/thread.h>

namespace sce {
namespace MusicCore {

struct CmdAckWaiter;

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
    CmdAckWaiter *waiter;
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
    int (*onStatus)(const PlayerStatusSnapshot *snap, void *userData, unsigned char force);
    int (*onExtra)(const unsigned char *data, void *userData);
    int (*onMetadata)(int mode, const void *meta, PlayerStatusSnapshot *snap, void *userData);
    int (*onCap)(const unsigned char *data, void *userData);
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
    virtual int checkOrderListStatus(int *outPrevId, unsigned char *outFlag)
    {
        (void)outPrevId;
        (void)outFlag;
        return 0;
    }
    virtual int releaseOrderList(unsigned char clearOwner)
    {
        (void)clearOwner;
        return 0;
    }
    virtual int getListId() const { return 0; }
    virtual void *getWorkBuf() = 0;
    virtual int navigateTo(int navTarget, unsigned char *outValid)
    {
        (void)navTarget;
        (void)outValid;
        return 0;
    }
    virtual unsigned int resolveErrorNavigation(int curIdx, int wrapMode, int dir,
                                                unsigned int *ioTried, int *ioErr,
                                                unsigned char *outEnd, unsigned char *outWrap)
    {
        (void)curIdx;
        (void)wrapMode;
        (void)dir;
        (void)ioTried;
        (void)ioErr;
        (void)outEnd;
        (void)outWrap;
        return 0;
    }
    virtual int resolveCurrentTrackId(int64_t *outTrackId)
    {
        (void)outTrackId;
        return 0;
    }
    virtual void setTrackPosition(int pos, int64_t *trackId) = 0;
    virtual int checkChangedFlag(unsigned char *outChanged, unsigned char queryLive)
    {
        (void)outChanged;
        (void)queryLive;
        return 0;
    }
    virtual int checkValid() { return 0; }
    virtual int checkPlayableAt(int index, int *outStatus)
    {
        (void)index;
        (void)outStatus;
        return 0;
    }
    virtual int getListStatus(void *outStatus)
    {
        (void)outStatus;
        return 0;
    }
    virtual int getContentType() = 0;
    virtual int getPlayState() = 0;
    virtual int getTrackIndex() = 0;
    virtual int64_t getTrackKey() = 0;
    virtual int rebuildOrderList(int shuffleSeed, int64_t anchorRowId, unsigned char *outChanged,
                                 unsigned char forceRebuild)
    {
        (void)shuffleSeed;
        (void)anchorRowId;
        (void)outChanged;
        (void)forceRebuild;
        return 0;
    }
    virtual int unlockOrderList(int flag)
    {
        (void)flag;
        return 0;
    }
    virtual int refreshOrderBuffer() { return 0; }
    virtual void populateMetadata(MetadataBuffers *meta) = 0;
};

struct DecoderSession {
    virtual ~DecoderSession() {}
    virtual int onTrackInfoCallback(const void *info)
    {
        (void)info;
        return 0;
    }
    virtual int onAudioErrorCallback(int errCode)
    {
        (void)errCode;
        return 0;
    }
    virtual int onBufferReadyCallback() { return 0; }
    virtual int onTrackReadyCallback() { return 0; }
    virtual int onStreamEndCallback(int val)
    {
        (void)val;
        return 0;
    }
    virtual int onMediaChangedCallback(unsigned char hasNew, void *newMedia, unsigned char hasUri,
                                       const void *state, void **ioOld, unsigned char decUri)
    {
        (void)hasNew;
        (void)newMedia;
        (void)hasUri;
        (void)state;
        (void)ioOld;
        (void)decUri;
        return 0;
    }
    virtual int forwardNotifyMetadata() { return 0; }
    virtual int start(const void *cbs, const int *audioCfg)
    {
        (void)cbs;
        (void)audioCfg;
        return 0;
    }
    virtual int stopSession() { return 0; }
    virtual int activate(DecoderMediaState *state, MetadataBuffers *meta, int fadeMs) = 0;
    virtual int deactivate(int fadeMs, uint32_t param, uint32_t cmdHi, unsigned char sync,
                           int extraFlags) = 0;
    virtual int seek(const int *seekParams, uint32_t param, uint32_t cmdHi,
                     unsigned char flag) = 0;
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
    virtual int setMediaPlaying(void *media, uint32_t param, uint32_t cmdHi, int shuffleSeed,
                                int isAsync) = 0;
    virtual int onTrackReady() = 0;
    virtual int onAudioError(int errCode, uint32_t param) = 0;
    virtual int onTrackInfo() = 0;
    virtual int onBufferReady() = 0;
    virtual int onStreamEnd() = 0;
    virtual int onAudioPortEvent(int flagA, int flagB, uint32_t origParam, uint32_t origCmdHi,
                                 unsigned char isAsync) = 0;
    virtual int suspendForDb() = 0;

    SceUID m_initSema;
    SceUID m_threadId;
    void *m_cmdQueue;
    void *m_callbacks[3];
    int m_padAlign;
    unsigned char m_stateMachine[4600];
};

struct SoundOutExclusiveParam {
    int version;
    int configWords[5];
};

extern unsigned int calcMediaQueueSize(int capacity, int itemSize, int slotCount, int headerSize);

class BasePlayer {
public:
    static unsigned int calcBaseExtraSize(int capacity);

    BasePlayer(char playerKind, void *factory, int capacity, int *outErr,
               unsigned int totalBytes, unsigned int headerBytes);
    virtual ~BasePlayer();

    virtual unsigned int calcAllocSize()
    {
        return ((calcMediaQueueSize(m_capacity, 2, 2, 56) - 1) & ~7u) + 480;
    }
    virtual int attach(unsigned char channelId, const PlayerCallbacks *cbs, void *dbMgr,
                       void *audioOut);
    virtual int detach();
    virtual void setAudioFocus(int granted) { (void)granted; }
    virtual int getAudioFocus() { return 0; }
    virtual int getPriority() { return m_priority; }
    virtual int getSubPriority() { return m_subPriority; }
    virtual int open(const void *param, unsigned char *flags);
    virtual int close(unsigned char *flags);
    virtual int isOpen() { return m_isOpen; }
    virtual int isBusy() { return m_isBusy; }
    virtual void onDbResume(unsigned char *flags);
    virtual void onPauseForDb(unsigned char *flags);
    virtual PlayerPriorityInfo getPriorityInfo();
    virtual int isActive()
    {
        PlayerPriorityInfo info = getPriorityInfo();
        return isActiveForInfo(info);
    }
    virtual int isActiveForInfo(PlayerPriorityInfo info) = 0;
    virtual int wantsAudio()
    {
        PlayerPriorityInfo info = getPriorityInfo();
        return wantsAudioForInfo(info);
    }
    virtual int wantsAudioForInfo(PlayerPriorityInfo info) = 0;
    virtual int isLocked() { return m_isSuspended; }
    virtual int getContentInfo(MetadataBuffers *outMeta);
    virtual void onIdleActive() {}
    virtual void onIdlePlaylist() {}
    virtual int getSystemSuspended() { return m_systemSuspended; }
    virtual void setSystemSuspended(int suspended) { m_systemSuspended = (unsigned char)suspended; }
    virtual int isPlayable() { return m_playable; }
    virtual int hasNoPendingSeek() { return m_pendingSeekCount <= 0; }

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
    virtual int cmdClearPlaylist(CmdNode *node, unsigned char *flags);
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
    virtual int onDecoderStreamEnd(CmdNode *node, unsigned char *flags);
    virtual int onDecoderMediaChanged(CmdNode *node);
    virtual int onPlaybackError(int errType, unsigned char *flags, unsigned char *outRetryPlay);
    virtual int onDbEvent(CmdNode *node, unsigned char *flags);
    virtual int cmdResetSpeed(uint32_t param, uint32_t cmdHi, unsigned char *flags);

    int dispatchCmd(CmdNode *node, unsigned char *flags);
    int setSuspended(char suspend);
    int buildStatusSnapshot(PlayerStatusSnapshot *outSnap, const DecoderMediaState *state);
    void notifyStatus(int unused, uint32_t param, uint32_t cmdHi, int errCode,
                      unsigned char *flags);
    int activateDecoder(int fadeMs, DecoderSession *decoder);
    int deactivateDecoder(int fadeMs, uint32_t param, uint32_t cmdHi, unsigned char syncFlag);
    int flushTimeCallback();
    int getStatus(PlayerStatusSnapshot *outSnap, int *outPos, unsigned char *outPosValid);
    int formatUriInfo(char *outBuf, int *outMode, const void *uriWork,
                      const DecoderMediaState *state);
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
    int (*m_statusCb)(const PlayerStatusSnapshot *, void *, unsigned char);
    int (*m_extraCb)(const unsigned char *, void *);
    int (*m_metadataCb)(int, const void *, PlayerStatusSnapshot *, void *);
    int (*m_capCb)(const unsigned char *, void *);
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

} /* namespace MusicCore */
} /* namespace sce */

#endif /* SHELL_MUSICCORE_PLAYER_BASE_H */
