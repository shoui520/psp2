#pragma once

#include <audioenc.h>
#include <threadmgr/fast_mutex_object.h>
#include <vnz_wrapper/vnz_wrapper.h>

#define AUDIOENC_TYPE_SILK 0x2007U
#define AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE ((int)0x80861801)
#define AUDIOENC_SILK_ERROR_INVALID_CONFIG ((int)0x80861800)
#define AUDIOENC_SILK_ERROR_INVALID_BIT_RATE ((int)0x80861802)
#define AUDIOENC_ERROR_INVALID_NFRAMES ((int)0x80860082)

typedef struct AudioencInfoSilk {
	SceUInt32 size;
	SceUInt32 samplingRate;
	SceUInt32 maxInternalSamplingRate;
	SceUInt32 bitRate;
	SceUInt32 packetLossPercentage;
	SceUInt32 complexity;
	SceUInt32 useInBandFec;
} AudioencInfoSilk;

typedef union AudioencInfo {
	SceAudioencInfo publicInfo;
	AudioencInfoSilk silk;
} AudioencInfo;

/* Complete host-side command transferred through Venezia RPC. */
typedef struct AudioencCommand {
	SceInt32 handle;
	void *pInputPcm;
	SceUInt32 inputPcmSize;
	void *pOutputEs;
	SceUInt32 outputEsSize;
	SceUInt32 wordLength;
	SceUInt32 reservedControl[2];
	void *pContext;
	SceUInt32 contextSize;
	SceUInt32 reservedContext[2];
	AudioencInfo info;
	SceUInt32 reservedInfo[9];
	SceAudioencOptInfo optInfo;
	SceUInt32 reservedTail[9];
} AudioencCommand;

_Static_assert(sizeof(AudioencCommand) == 192, "Encoder command ABI");

typedef struct AudioencEncoderContext {
	void *pContext;
	SceUInt32 contextSize;
	SceUInt32 channels;
	SceUInt32 active;
} AudioencEncoderContext;

typedef struct AudioencChannelContext {
	void *pContext;
	SceUInt32 contextSize;
	SceInt32 encoderIndex;
} AudioencChannelContext;

typedef struct AudioencEncoderWork {
	SceInt32 internalError;
	SceUInt32 maxPcmSize;
	SceUInt32 maxEsSize;
	SceUInt32 inputPcmSize;
	SceUInt32 outputEsSize;
	AudioencInfo info;
} AudioencEncoderWork;

typedef struct AudioencWorkManager {
	SceUInt32 codecType;
	struct ThreadmgrFastMutex *pMutex;
	SceUInt32 maxEncoders;
	SceUInt32 totalEncoders;
	SceUInt32 usedEncoders;
	SceUInt32 maxChannels;
	SceUInt32 totalChannels;
	SceUInt32 usedChannels;
	void *pContext;
	SceUInt32 contextSize;
	AudioencEncoderContext *pEncoders;
	AudioencChannelContext *pChannels;
	AudioencEncoderWork *pWork;
} AudioencWorkManager;

typedef struct AudioencResident {
	SceUID processStorage;
	struct ThreadmgrFastMutex mutex;
	AudioencWorkManager manager;
	AudioencEncoderContext encoders[1];
	AudioencEncoderWork work[1];
	SceUID owners[1];
} AudioencResident;

typedef struct AudioencWorkBank {
	AudioencWorkManager managers[2];
	AudioencEncoderContext encoders[1];
	AudioencEncoderWork work[1];
} AudioencWorkBank;

typedef struct AudioencProcess {
	struct ThreadmgrFastMutex mutexes[2];
	AudioencWorkBank internal;
	AudioencWorkBank external;
} AudioencProcess;

_Static_assert(sizeof(AudioencInfo) == 28, "Encoder information ABI");
_Static_assert(sizeof(AudioencEncoderContext) == 16, "Encoder context ABI");
_Static_assert(sizeof(AudioencEncoderWork) == 48, "Encoder work ABI");
_Static_assert(sizeof(AudioencWorkManager) == 52, "Encoder work manager ABI");
_Static_assert(sizeof(AudioencResident) == 188, "Resident encoder ABI");
_Static_assert(sizeof(AudioencWorkBank) == 168, "Encoder work bank ABI");
_Static_assert(sizeof(AudioencProcess) == 464, "Encoder process storage ABI");

extern AudioencResident audioencResident;
int sceAudioencInitLibraryForUser(SceUInt32 codecType, SceAudioencInitParam *param);
int sceAudioencTermLibraryForUser(SceUInt32 codecType);
int sceAudioencCreateEncoderForUser(SceAudioencCtrl *ctrl, SceUInt32 codecType);
int sceAudioencCreateEncoderResidentForUser(SceAudioencCtrl *ctrl, SceUInt32 codecType);
int sceAudioencGetContextSizeForUser(const SceAudioencCtrl *ctrl, SceUInt32 codecType);
int sceAudioencCreateEncoderExternalForUser(SceAudioencCtrl *ctrl, SceUInt32 codecType,
					  SceUIntVAddr context, SceUInt32 contextSize);
int sceAudioencDeleteEncoderForUser(SceAudioencCtrl *ctrl);
int sceAudioencDeleteEncoderResidentForUser(SceAudioencCtrl *ctrl);
int sceAudioencDeleteEncoderExternalForUser(SceAudioencCtrl *ctrl, SceUIntVAddr context);
int sceAudioencEncodeForUser(SceAudioencCtrl *ctrl);
int sceAudioencEncodeNFramesForUser(SceAudioencCtrl *ctrl, SceUInt32 nFrames);
int sceAudioencClearContextForUser(SceAudioencCtrl *ctrl);
int sceAudioencGetOptInfoForUser(SceAudioencCtrl *ctrl);
int sceAudioencGetInternalErrorForUser(SceAudioencCtrl *ctrl, int *internalError);
int _audioencInternalInitLibrary(AudioencWorkManager *manager,
			       const SceAudioencInitParam *param, int allocate);
int _audioencInternalExitLibrary(SceUID pid, AudioencWorkManager *manager,
			       int external, int resident);

int _audioencInternalTermLibrary(AudioencWorkManager *manager, int release);

int _audioencInternalCreateEncoder(AudioencWorkManager *manager, SceAudioencCtrl *ctrl,
				   SceUIntVAddr vaContext, SceSize contextSize, int resident);

int audioencIsExternalHandle(SceUInt32 handle);
int audioencIsResidentHandle(SceUInt32 handle);
SceUInt32 audioencGetHandleCodecType(SceUInt32 handle);
SceUInt32 audioencGetHandleIndex(SceUInt32 handle);
int _audioencInternalDeleteEncoder(AudioencWorkManager *manager, SceAudioencCtrl *ctrl,
				   SceUIntVAddr *context);
int _audioencInternalEncode(AudioencWorkManager *manager, SceAudioencCtrl *ctrl,
			   int encode);
int _audioencInternalClearContext(AudioencWorkManager *manager, SceAudioencCtrl *ctrl);
int _audioencInternalGetOptInfo(AudioencWorkManager *manager, SceAudioencCtrl *ctrl);
int _audioencInternalGetInternalError(AudioencWorkManager *manager,
				    SceAudioencCtrl *ctrl, int *userInternalError);
SceUInt32 audioencMakeHandle(int external, int resident, SceUInt32 codecType,
			   SceUInt32 index);
int audioencQueryLibraryContext(AudioencCommand *command);
int audioencDeleteCommand(AudioencCommand *command);
SceUID _audioencGetResidentEncoderOwner(unsigned int encoder);
int _audioencClearResidentEncoderOwner(unsigned int encoder);
int _audioencSetResidentEncoderOwner(unsigned int encoder);
int audioencCreateCommand(AudioencCommand *command);
int audioencEncodeCommand(AudioencCommand *command, int nFrames);
int audioencClearContextCommand(AudioencCommand *command);
int audioencGetOptInfoCommand(AudioencCommand *command);
