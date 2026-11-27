/*
 * SceShell musiccore_plugin - controller client API declarations.
 * Original address range (3.65-CEX): 0x811D2030 - 0x811D3484
 */

#ifndef SHELL_MUSICCORE_CTRL_CLIENT_H
#define SHELL_MUSICCORE_CTRL_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

namespace sce {
namespace MusicCore {

struct OpenParams {
    int sampleRate;
    int channels;
    int grainSize;
    int audioMode;
    int priorityHint;
};

struct TrackContentDesc {
    char uri[0x804];
    int listIndex;
    int trackIndex;
    int padTrackId;
    int64_t trackId;
    int shuffleFlag;
    int contentKind;
    int extraMeta;
    int padTail;
};

struct ControlParams {
    int seekTarget;
    int speedValue;
    int trackIndex;
    int seekMode;
    int seekPositionMs;
};

struct ContentInfoBuffers {
    char *titleBuf;
    size_t titleLen;
    char *artistBuf;
    size_t artistLen;
    char *albumBuf;
    size_t albumLen;
};

struct PlaybackStatusOut {
    unsigned char bytes[3];
    unsigned char padHeader[13];
    int16_t flags;
    int statusWordLow;
    int statusWordHigh;
    int playState;
    int contentType;
    int padAlign;
    int64_t trackId;
    int64_t trackItemId;
    int64_t playlistId;
    int words[19];
};

int ctrlInit();
int ctrlTerm();
int ctrlOpenSimple(uint32_t seqId, unsigned char channelType, int priority,
                   unsigned char *outSlot);
int ctrlOpenWithParams(uint32_t seqId, unsigned char channelType, int priority,
                       unsigned char *outSlot, const OpenParams *openParams);
int ctrlClose(uint32_t seqId, unsigned char channelId);
int ctrlSetActiveSlot(uint32_t seqId, unsigned char channelId);
int ctrlSetUriData(uint32_t seqId, unsigned char channelId,
                   const TrackContentDesc *contentDesc);
int ctrlSetUriString(uint32_t seqId, unsigned char channelId, const char *uri,
                     int uriFlags);
int ctrlResumeAudio(uint32_t seqId, unsigned char channelId);
int ctrlPlay(uint32_t seqId, unsigned char channelId, int fadeMs);
int ctrlPause(uint32_t seqId, unsigned char channelId, unsigned char pauseMode,
              int fadeMs);
int ctrlControl(uint32_t seqId, unsigned char channelId,
                const ControlParams *ctrlParams);
int ctrlSetRepeat(uint32_t seqId, unsigned char channelId, int repeatMode);
int ctrlSetShuffle(uint32_t seqId, unsigned char channelId, int enableShuffle);
int ctrlSetVolume(uint32_t seqId, unsigned char channelId, int volume);
int ctrlSetEqualizer(uint32_t seqId, unsigned char channelId,
                     int eqType, int bandLow, int bandMid, int bandHigh);
int ctrlSetAudioNormalizer(uint32_t seqId, unsigned char channelId,
                           int enableNormalizer);
int ctrlSetIdleCallback(uint32_t seqId, void *callback);
int ctrlSetCapCallback(uint32_t seqId, void *callback);
int ctrlSetStatusCallback(uint32_t seqId, void *callback);
int ctrlSetExtraCallback(uint32_t seqId, void *callback);
int ctrlSetTimeCallback(uint32_t seqId, void *callback, int intervalUs);
int ctrlSetMediaCallback(uint32_t seqId, void *callback);
int ctrlDbOpen(uint32_t seqId);
int ctrlDbSuspend(uint32_t seqId, unsigned char suspendFlag);
int ctrlDbClose(uint32_t seqId);
int ctrlDbQuery(uint32_t seqId, const void *trackIds, int trackCount, int queryOpt);
int ctrlGetContentInfo(unsigned char channelId, ContentInfoBuffers *info);
int ctrlGetPlaybackStatus(unsigned char channelId, PlaybackStatusOut *outStatus,
                          void *outPos, void *outPosValid);
int ctrlGetPlaybackMode(unsigned char channelId, int unusedMode, int *outMode,
                        void *workBuf, unsigned char *outFlag);
int ctrlGetState(unsigned char channelId, void *outState);
int ctrlSuspendAudio(uint32_t seqId, unsigned char channelId);
int ctrlSeekAndPlay(uint32_t seqId, unsigned char channelId,
                    const ControlParams *ctrlParams, int fadeMs);
int ctrlStop(uint32_t seqId, unsigned char channelId);
int ctrlClearAudio(uint32_t seqId, unsigned char channelId);

} /* namespace MusicCore */
} /* namespace sce */

#endif /* SHELL_MUSICCORE_CTRL_CLIENT_H */
