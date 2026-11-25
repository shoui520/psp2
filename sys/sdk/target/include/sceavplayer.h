/* SCE CONFIDENTIAL
 * PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 */

#ifndef _SCE_AVPLAYER_SCEAVPLAYER_H
#define _SCE_AVPLAYER_SCEAVPLAYER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SceAvPlayerHandle;

typedef enum SceAvPlayerStreamType {
	SCE_AVPLAYER_VIDEO,
	SCE_AVPLAYER_AUDIO,
	SCE_AVPLAYER_TIMEDTEXT,
	SCE_AVPLAYER_UNKNOWN
} SceAvPlayerStreamType;

typedef enum SceAvPlayerEvents {
	SCE_AVPLAYER_STATE_STOP          = 0x01,
	SCE_AVPLAYER_STATE_READY         = 0x02,
	SCE_AVPLAYER_STATE_PLAY          = 0x03,
	SCE_AVPLAYER_STATE_PAUSE         = 0x04,
	SCE_AVPLAYER_STATE_BUFFERING     = 0x05,
	SCE_AVPLAYER_TIMED_TEXT_DELIVERY = 0x10,
	SCE_AVPLAYER_WARNING_ID          = 0x20,
	SCE_AVPLAYER_ENCRYPTION          = 0x30
} SceAvPlayerEvents;

typedef enum SceAvPlayerTrickSpeeds {
	SCE_AVPLAYER_SPEED_NORMAL = 100,
	SCE_AVPLAYER_SPEED_FF_2X  = 200,
	SCE_AVPLAYER_SPEED_FF_4X  = 400,
	SCE_AVPLAYER_SPEED_FF_8X  = 800,
	SCE_AVPLAYER_SPEED_FF_16X = 1600,
	SCE_AVPLAYER_SPEED_FF_MAX = 3200,
	SCE_AVPLAYER_SPEED_RW_8X  = -800,
	SCE_AVPLAYER_SPEED_RW_16X = -1600,
	SCE_AVPLAYER_SPEED_RW_MAX = -3200
} SceAvPlayerTrickSpeeds;

typedef enum SceAvPlayerErrorCodes {
	SCE_AVPLAYER_NO_ERR                  = 0x00,
	SCE_AVPLAYER_ERR_INVALID_PARAMS      = (int)0x806A0001,
	SCE_AVPLAYER_ERR_OPERATION_FAILED    = (int)0x806A0002,
	SCE_AVPLAYER_ERR_NO_MEMORY           = (int)0x806A0003,
	SCE_AVPLAYER_ERR_NOT_SUPPORTED       = (int)0x806A0004,
	SCE_AVPLAYER_WAR_FILE_NONINTERLEAVED = (int)0x806A00A0,
	SCE_AVPLAYER_WAR_LOOPING_BACK        = (int)0x806A00A1,
	SCE_AVPLAYER_WAR_JUMP_COMPLETE       = (int)0x806A00A3,
	SCE_AVPLAYER_INFO_MARLIN_ENCRY       = (int)0x806A00B0,
	SCE_AVPLAYER_INFO_PLAYREADY_ENCRY    = (int)0x806A00B4,
	SCE_AVPLAYER_INFO_AES_ENCRY          = (int)0x806A00B5,
	SCE_AVPLAYER_INFO_OTHER_ENCRY        = (int)0x806A00BF
} SceAvPlayerErrorCodes;

typedef enum SceAvPlayerDebuglevels {
	SCE_AVPLAYER_DBG_NONE,
	SCE_AVPLAYER_DBG_INFO,
	SCE_AVPLAYER_DBG_WARNINGS,
	SCE_AVPLAYER_DBG_ALL
} SceAvPlayerDebuglevels;

typedef struct SceAvPlayerAudio {
	uint16_t channelCount;
	uint8_t  reserved[2];
	uint32_t sampleRate;
	uint32_t size;
	uint8_t  languageCode[4];
} SceAvPlayerAudio;

typedef struct SceAvPlayerVideo {
	uint32_t width;
	uint32_t height;
	float    aspectRatio;
	uint8_t  languageCode[4];
} SceAvPlayerVideo;

typedef struct SceAvPlayerTextPosition {
	uint16_t top;
	uint16_t left;
	uint16_t bottom;
	uint16_t right;
} SceAvPlayerTextPosition;

typedef struct SceAvPlayerTimedText {
	uint8_t                 languageCode[4];
	uint16_t                textSize;
	uint16_t                fontSize;
	SceAvPlayerTextPosition position;
} SceAvPlayerTimedText;

typedef union SceAvPlayerStreamDetails {
	uint8_t              reserved[16];
	SceAvPlayerAudio     audio;
	SceAvPlayerVideo     video;
	SceAvPlayerTimedText subs;
} SceAvPlayerStreamDetails;

typedef struct SceAvPlayerStreamInfo {
	uint32_t                 type;
	uint8_t                  reserved[4];
	SceAvPlayerStreamDetails details;
	uint64_t                 duration;
	uint64_t                 startTime;
} SceAvPlayerStreamInfo;

typedef struct SceAvPlayerFrameInfo {
	uint8_t                  *pData;
	uint8_t                  reserved[4];
	uint64_t                 timeStamp;
	SceAvPlayerStreamDetails details;
} SceAvPlayerFrameInfo;

typedef void *(*SceAvPlayerAllocate)(void *argP, uint32_t argAlignment, uint32_t argSize);
typedef void (*SceAvPlayerDeallocate)(void *argP, void *argMemory);
typedef void *(*SceAvPlayerAllocateTexture)(void *argP, uint32_t argAlignment, uint32_t argSize);
typedef void (*SceAvPlayerDeallocateTexture)(void *argP, void *argMemory);

typedef struct SceAvPlayerMemAllocator {
	void                        *objectPointer;
	SceAvPlayerAllocate          allocate;
	SceAvPlayerDeallocate        deallocate;
	SceAvPlayerAllocateTexture   allocateTexture;
	SceAvPlayerDeallocateTexture deallocateTexture;
} SceAvPlayerMemAllocator;

typedef int (*SceAvPlayerOpenFile)(void *argP, const char *argFilename);
typedef int (*SceAvPlayerCloseFile)(void *argP);
typedef int (*SceAvPlayerReadOffsetFile)(void *argP, uint8_t *argBuffer, uint64_t argPosition, uint32_t argLength);
typedef uint64_t (*SceAvPlayerSizeFile)(void *argP);

typedef struct SceAvPlayerFileReplacement {
	void                      *objectPointer;
	SceAvPlayerOpenFile        open;
	SceAvPlayerCloseFile       close;
	SceAvPlayerReadOffsetFile  readOffset;
	SceAvPlayerSizeFile        size;
} SceAvPlayerFileReplacement;

typedef void (*SceAvPlayerEventCallback)(void *p, int32_t argEventId, int32_t argSourceId, void *argEventData);

typedef struct SceAvPlayerEventReplacement {
	void                     *objectPointer;
	SceAvPlayerEventCallback  eventCallback;
} SceAvPlayerEventReplacement;

typedef struct SceAvPlayerInitData {
	SceAvPlayerMemAllocator     memoryReplacement;
	SceAvPlayerFileReplacement  fileReplacement;
	SceAvPlayerEventReplacement eventReplacement;
	SceAvPlayerDebuglevels      debugLevel;
	uint32_t                    basePriority;
	int32_t                     numOutputVideoFrameBuffers;
	bool                        autoStart;
	uint8_t                     reserved[3];
	const char                 *defaultLanguage;
} SceAvPlayerInitData;

typedef struct SceAvPlayerPostInitData {
	uint32_t demuxVideoBufferSize;
	uint8_t  reserved[128];
} SceAvPlayerPostInitData;

SceAvPlayerHandle sceAvPlayerInit(SceAvPlayerInitData *pInit);
int32_t sceAvPlayerPostInit(SceAvPlayerHandle h, SceAvPlayerPostInitData *pPostInit);
int32_t sceAvPlayerClose(SceAvPlayerHandle h);
int32_t sceAvPlayerAddSource(SceAvPlayerHandle h, const char *argFilename);
int32_t sceAvPlayerStreamCount(SceAvPlayerHandle h);
int32_t sceAvPlayerGetStreamInfo(SceAvPlayerHandle h, uint32_t argStreamID, SceAvPlayerStreamInfo *argInfo);
int32_t sceAvPlayerEnableStream(SceAvPlayerHandle h, uint32_t argStreamID);
int32_t sceAvPlayerDisableStream(SceAvPlayerHandle h, uint32_t argStreamID);
int32_t sceAvPlayerStart(SceAvPlayerHandle h);
int32_t sceAvPlayerStop(SceAvPlayerHandle h);
int32_t sceAvPlayerPause(SceAvPlayerHandle h);
int32_t sceAvPlayerResume(SceAvPlayerHandle h);
int32_t sceAvPlayerSetLooping(SceAvPlayerHandle h, bool loopFlag);
bool sceAvPlayerIsActive(SceAvPlayerHandle h);
bool sceAvPlayerGetAudioData(SceAvPlayerHandle h, SceAvPlayerFrameInfo *audioInfo);
bool sceAvPlayerGetVideoData(SceAvPlayerHandle h, SceAvPlayerFrameInfo *videoInfo);
uint64_t sceAvPlayerCurrentTime(SceAvPlayerHandle h);
int32_t sceAvPlayerJumpToTime(SceAvPlayerHandle h, uint64_t argJumpTimeMsec);
int32_t sceAvPlayerSetTrickSpeed(SceAvPlayerHandle h, int32_t argTrickSpeed);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_AVPLAYER_SCEAVPLAYER_H */
