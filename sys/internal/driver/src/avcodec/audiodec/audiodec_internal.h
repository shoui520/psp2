#pragma once

#include <audiodec.h>
#include <threadmgr/fast_mutex_object.h>
#include <vnz_wrapper/vnz_wrapper.h>

#define AUDIODEC_TYPE_SILK 0x1007U
#define AUDIODEC_TYPE_AAC_MULTICHANNEL 0x1008U

int sceAudiodecInitLibraryForUser(SceUInt32 codecType, SceAudiodecInitParam *userParam);
int sceAudiodecTermLibraryForUser(SceUInt32 codecType);
int sceAudiodecCreateDecoderForUser(SceAudiodecCtrl *userCtrl, SceUInt32 codecType);
int sceAudiodecCreateDecoderResidentForUser(SceAudiodecCtrl *userCtrl, SceUInt32 codecType);
int sceAudiodecGetContextSizeForUser(const SceAudiodecCtrl *userCtrl, SceUInt32 codecType);
int sceAudiodecCreateDecoderExternalForUser(SceAudiodecCtrl *userCtrl,
		SceUInt32 codecType, SceUIntVAddr vaContext, SceUInt32 contextSize);
int sceAudiodecDeleteDecoderForUser(SceAudiodecCtrl *userCtrl);
int sceAudiodecDeleteDecoderResidentForUser(SceAudiodecCtrl *userCtrl);
int sceAudiodecDeleteDecoderExternalForUser(SceAudiodecCtrl *userCtrl,
		SceUIntVAddr *vaContext);
int sceAudiodecDecodeForUser(SceAudiodecCtrl *userCtrl);
int sceAudiodecDecodeNFramesForUser(SceAudiodecCtrl *userCtrl, SceUInt32 nFrames);
int sceAudiodecDecodeNStreamsForUser(SceAudiodecCtrl *const *userCtrls,
		SceUInt32 nStreams);
int sceAudiodecPartlyDecodeForUser(SceAudiodecCtrl *userCtrl,
		SceUInt32 startSample, SceUInt32 nSamples);
int sceAudiodecClearContextForUser(SceAudiodecCtrl *userCtrl);
int sceAudiodecGetInternalErrorForUser(SceAudiodecCtrl *userCtrl, int *internalError);

/* The internal speech decoder copies and validates this complete eight-byte input. */
typedef struct AudiodecInfoSilk {
	SceUInt32 size;
	SceUInt32 samplingRate;
} AudiodecInfoSilk;

/* The multichannel AAC input extends the public AAC information by one word. */
typedef struct AudiodecInfoAacMultichannel {
	SceAudiodecInfoAac aac;
	SceUInt32 downmix;
} AudiodecInfoAacMultichannel;

typedef union AudiodecInfo {
	SceAudiodecInfo publicInfo;
	AudiodecInfoSilk silk;
	AudiodecInfoAacMultichannel aacMultichannel;
} AudiodecInfo;

/* Per-decoder state allocated by the work manager, one complete 56-byte entry. */
/* Host-side command copied into the Venezia RPC memory area. */
typedef struct AudiodecCommand {
	SceInt32 handle;
	SceUInt8 *pEs;
	SceUInt32 inputEsSize;
	void *pPcm;
	SceUInt32 outputPcmSize;
	SceUInt32 wordLength;
	SceUInt32 reservedControl[2];
	void *pContext;
	SceUInt32 contextSize;
	SceUInt32 reservedContext[2];
	AudiodecInfo info;
	void *channelContexts[SCE_AUDIODEC_AT9_MAX_CH_IN_DECODER];
	SceUInt32 reservedResult[6];
	SceUInt32 channelContextSize;
	SceUInt32 maxEsSize;
	SceUInt32 bufferedFrames;
	SceUInt32 reservedTail[2];
} AudiodecCommand;

_Static_assert(sizeof(AudiodecCommand) == 128, "Decoder command ABI");

typedef struct AudiodecDecoderWork {
	SceInt32 internalError;
	SceUInt32 maxEsSize;
	SceUInt32 maxPcmSize;
	SceUInt32 inputEsSize;
	SceUInt32 outputPcmSize;
	AudiodecInfo info;
	SceUInt32 samplesPerFrame;
	SceUInt32 bufferedFrames;
} AudiodecDecoderWork;

_Static_assert(sizeof(AudiodecDecoderWork) == 56, "Decoder work ABI");

typedef struct AudiodecDecoderContext {
	void *pContext;
	SceUInt32 contextSize;
	SceUInt32 channels;
	SceUInt32 active;
	volatile SceKernelSpinlock decodeLock;
	volatile SceKernelSpinlock lockGuard;
} AudiodecDecoderContext;

typedef struct AudiodecChannelContext {
	void *pContext;
	SceUInt32 contextSize;
	SceInt32 decoderIndex;
} AudiodecChannelContext;

typedef struct AudiodecWorkManager {
	SceUInt32 codecType;
	struct ThreadmgrFastMutex *pMutex;
	SceUInt32 maxDecoders;
	SceUInt32 totalDecoders;
	SceUInt32 usedDecoders;
	SceUInt32 maxChannels;
	SceUInt32 totalChannels;
	SceUInt32 usedChannels;
	void *pContext;
	SceUInt32 contextSize;
	AudiodecDecoderContext *pDecoders;
	AudiodecChannelContext *pChannels;
	AudiodecDecoderWork *pWork;
} AudiodecWorkManager;

typedef struct AudiodecResident {
	SceUID processStorage;
	struct ThreadmgrFastMutex mutex;
	AudiodecWorkManager manager;
	AudiodecDecoderContext decoders[7];
	AudiodecDecoderWork work[7];
	SceUID owners[7];
} AudiodecResident;

_Static_assert(sizeof(AudiodecDecoderContext) == 24, "Decoder context ABI");
_Static_assert(sizeof(AudiodecChannelContext) == 12, "Channel context ABI");
_Static_assert(sizeof(AudiodecWorkManager) == 52, "Work manager ABI");
_Static_assert(sizeof(AudiodecResident) == 708, "Resident decoder ABI");

typedef struct AudiodecWorkBank {
	AudiodecWorkManager managers[6];
	AudiodecDecoderContext decoders[41];
	AudiodecChannelContext channels[16];
	AudiodecDecoderWork work[41];
} AudiodecWorkBank;

typedef struct AudiodecProcess {
	struct ThreadmgrFastMutex mutexes[6];
	AudiodecWorkBank internal;
	AudiodecWorkBank external;
} AudiodecProcess;

_Static_assert(sizeof(AudiodecWorkBank) == 3784, "Decoder work bank ABI");
_Static_assert(sizeof(AudiodecProcess) == 7952, "Decoder process storage ABI");

int _audiodecInternalInitLibrary(AudiodecWorkManager *manager,
			       const SceAudiodecInitParam *param, int allocate);
int _audiodecInternalExitLibrary(SceUID pid, AudiodecWorkManager *manager,
			       int external, int resident);
int _audiodecInternalTermLibrary(AudiodecWorkManager *manager, int release);
int _audiodecInternalCreateDecoder(AudiodecWorkManager *manager,
				   SceAudiodecCtrl *ctrl, SceUIntVAddr vaContext,
				   SceSize contextSize, int resident);
int atrac9GetMaxEsSize(const SceUInt8 configData[4]);

_Static_assert(sizeof(AudiodecInfoSilk) == 8, "Speech decoder information ABI");
_Static_assert(sizeof(AudiodecInfoAacMultichannel) == 24, "Multichannel AAC information ABI");

int atrac9GetChannelCount(const SceUInt8 configData[4]);
int atrac9GetSamplesPerFrame(const SceUInt8 configData[4]);
int atrac9ValidateConfig(const SceAudiodecCtrl *ctrl);
SceUInt32 audiodecGetHandleCodecType(SceUInt32 handle);
SceUInt32 audiodecGetHandleIndex(SceUInt32 handle);
int audiodecCalculateEsSize(SceUInt32 codecType, AudiodecDecoderWork *work,
			  SceUInt32 nFrames);
int audiodecCalculatePcmSize(SceUInt32 codecType, AudiodecDecoderWork *work,
			   SceUInt32 nFrames, SceUInt32 startSample,
			   SceUInt32 nSamples);

int audiodecIsExternalHandle(SceUInt32 handle);
int audiodecIsResidentHandle(SceUInt32 handle);
int _audiodecInternalDeleteDecoder(AudiodecWorkManager *manager,
				   SceAudiodecCtrl *ctrl, SceUIntVAddr *vaContext);

int _audiodecInternalDecode(AudiodecWorkManager *manager, SceAudiodecCtrl *ctrl,
			   SceUInt32 nFrames, SceUInt32 startSample,
			   SceUInt32 nSamples);

int _audiodecInternalDecodeNStreams(AudiodecWorkManager *const *managers,
				  SceAudiodecCtrl *ctrls, SceUInt32 nStreams);

int _audiodecInternalClearContext(AudiodecWorkManager *manager, SceAudiodecCtrl *ctrl);

int _audiodecInternalGetInternalError(AudiodecWorkManager *manager,
				    SceAudiodecCtrl *ctrl, int *internalError);

SceInt32 audiodecMakeHandle(int external, int resident, SceUInt32 codecType,
			   SceUInt32 index);
int audiodecQueryLibraryContext(AudiodecCommand *command);

SceUID _audiodecGetResidentDecoderOwner(unsigned int decoder);
int _audiodecClearResidentDecoderOwner(unsigned int decoder);
int audiodecPrepareCommand(int operation, AudiodecCommand *command,
			  AudiodecWorkManager *manager, const SceAudiodecCtrl *ctrl);
int audiodecDeleteCommand(AudiodecCommand *command);
int _audiodecSetResidentDecoderOwner(unsigned int decoder);
int audiodecCreateCommand(AudiodecCommand *command);
int audiodecPrepareDecodeCommand(AudiodecCommand *command,
				AudiodecWorkManager *manager, const SceAudiodecCtrl *ctrl);
int audiodecDecodeCommand(AudiodecCommand *command, SceUInt32 nFrames,
			 SceUInt32 startSample, SceUInt32 nSamples);
int audiodecDecodeNStreamsCommand(AudiodecCommand *commands, SceUInt32 nStreams);
int audiodecClearContextCommand(AudiodecCommand *command);

void audiodecFinishDecodeCommand(VeneziaRpcThunk *thunk);
int audiodecPackCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
		       unsigned int *packed);
int audiodecPackRpcMemory(VeneziaRpcThunk *thunk, const unsigned int *input,
			 unsigned int *packed);
int audiodecPackDecodeCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			     unsigned int *packed);
void audiodecFinishCommand(struct VeneziaRpcThunk *thunk);
int audiodecFinishRpcMemory(struct VeneziaRpcThunk *thunk);
