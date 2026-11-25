#ifndef NAAC_INTERNAL_H
#define NAAC_INTERNAL_H

#include <scetypes.h>
#include <stddef.h>
#include <stdint.h>

extern void *sceClibMemcpy_safe(void *dst, const void *src, size_t len);
extern void *sceClibMemset(void *dst, int val, size_t len);
extern void *sceClibMemmove(void *dst, const void *src, size_t len);

#define NAAC_HANDLE_MAGIC          0x4341414EU /* "NAAC" */
#define NAAC_WORK_BUFFER_SIZE      454512
#define NAAC_FRAME_SAMPLES         1024
#define NAAC_SHORT_SAMPLES         128
#define NAAC_MAX_CHANNELS          8
#define NAAC_MAX_ELEMENTS          8
#define NAAC_MAX_SFB_LONG          51
#define NAAC_MAX_SFB_SHORT         14
#define NAAC_MAX_GROUPS            8
#define NAAC_MAX_TNS_ORDER         12
#define NAAC_MAX_CB                64
#define NAAC_DEFAULT_COMMENT       "naacEnc Ver. 2.16.1"

#define NAAC_OK                    0
#define NAAC_ERR_INVALID_PARAM     (-1)
#define NAAC_ERR_INVALID_HANDLE    (-2)
#define NAAC_ERR_NO_MEMORY         (-3)
#define NAAC_ERR_INTERNAL          (-4)

#define NAAC_ERROR_BASE            0x806B0000U

typedef enum NaacPcmFormat {
	NAAC_PCM_INT16   = 0,
	NAAC_PCM_INT24   = 1,
	NAAC_PCM_INT32   = 2,
	NAAC_PCM_FLOAT32 = 3
} NaacPcmFormat;

typedef enum NaacBlockType {
	NAAC_LONG_WINDOW  = 0,
	NAAC_START_WINDOW = 1,
	NAAC_SHORT_WINDOW = 2,
	NAAC_STOP_WINDOW  = 3
} NaacBlockType;

typedef enum NaacWindowShape {
	NAAC_SHAPE_SINE = 0,
	NAAC_SHAPE_KBD  = 1
} NaacWindowShape;

typedef enum NaacElementType {
	NAAC_ELEM_SCE = 0,
	NAAC_ELEM_CPE = 1,
	NAAC_ELEM_CCE = 2,
	NAAC_ELEM_LFE = 3,
	NAAC_ELEM_DSE = 4,
	NAAC_ELEM_PCE = 5,
	NAAC_ELEM_FIL = 6,
	NAAC_ELEM_END = 7
} NaacElementType;

typedef struct NaacConfig {
	int32_t numChannels;
	int32_t sampleRate;
	int32_t bitRate;
	int32_t audioObjectType;
	int32_t streamFormat;
	uint32_t enableTns;
	uint32_t enableMs;
	uint32_t disablePns;
	const int32_t *channelOrder;
	const int32_t *channelStride;
	const char *comment;
	uint32_t enablePce;
	uint8_t pceTagBytes[9];
	uint8_t pceReservedTail[3];
	uint32_t enableAdts;
	uint32_t enableLatm;
	uint32_t dualMonoMode;
	uint32_t sbrFlag;
	uint32_t sbrRatio;
	uint32_t psFlag;
	int32_t bandwidthHz;
} NaacConfig;

typedef struct NaacBufferSizes {
	int32_t inputBytes;
	int32_t outputBytes;
} NaacBufferSizes;

typedef struct NaacBitBuffer {
	uint8_t *buffer;
	int32_t capacityBits;
	int32_t bitPos;
	int32_t totalBits;
} NaacBitBuffer;

typedef struct NaacFloatFifo {
	float *samples;
	int32_t capacity;
	int32_t readIndex;
	int32_t writeIndex;
	int32_t count;
} NaacFloatFifo;

typedef struct NaacFloatModulo {
	float *samples;
	int32_t length;
	int32_t cursor;
} NaacFloatModulo;

typedef struct NaacChannelElement {
	int32_t elementType;
	int32_t instanceTag;
	int32_t firstChannel;
	int32_t secondChannel;
} NaacChannelElement;

typedef struct NaacChannelMap {
	int32_t numChannels;
	int32_t numElements;
	int32_t numFrontElements;
	int32_t numSideElements;
	int32_t numBackElements;
	int32_t numLfeElements;
	int32_t hasLfe;
	NaacChannelElement elements[NAAC_MAX_ELEMENTS];
} NaacChannelMap;

typedef struct NaacProgramConfig {
	int32_t elementInstanceTag;
	int32_t objectType;
	int32_t samplingFreqIndex;
	int32_t numFrontChannels;
	int32_t numSideChannels;
	int32_t numBackChannels;
	int32_t numLfeChannels;
	int32_t numAssocData;
	int32_t numValidCc;
	const char *comment;
	int32_t channelConfigIndex;
} NaacProgramConfig;

typedef struct NaacPsyParam {
	int32_t sampleRate;
	int32_t sampleRateIndex;
	int32_t numSfbLong;
	int32_t numSfbShort;
	int32_t sfbOffsetLong[NAAC_MAX_SFB_LONG + 1];
	int32_t sfbOffsetShort[NAAC_MAX_SFB_SHORT + 1];
	int32_t numCbLong;
	int32_t numCbShort;
	float cbWidthLong[NAAC_MAX_CB];
	float cbWidthShort[NAAC_MAX_CB];
	float spreadingLong[NAAC_MAX_CB];
	float spreadingShort[NAAC_MAX_CB];
} NaacPsyParam;

typedef struct NaacTnsData {
	int32_t tnsActive;
	int32_t numFilters;
	int32_t filterOrder;
	int32_t direction;
	float coeffs[NAAC_MAX_TNS_ORDER];
	int32_t quantCoeffs[NAAC_MAX_TNS_ORDER];
} NaacTnsData;

typedef struct NaacSwitchControl {
	int32_t currentWindow;
	int32_t nextWindow;
	int32_t windowShape;
	float prevEnergy;
	float attackRatio;
} NaacSwitchControl;

typedef struct NaacThrData {
	float energy[NAAC_MAX_SFB_LONG];
	float threshold[NAAC_MAX_SFB_LONG];
	float sfbMinSnr[NAAC_MAX_SFB_LONG];
	float perceptualEntropy;
} NaacThrData;

typedef struct NaacThrChannel {
	NaacThrData thr;
	float prevEnergy[NAAC_MAX_SFB_LONG];
} NaacThrChannel;

typedef struct NaacPsyChannel {
	NaacFloatFifo inputFifo;
	NaacFloatModulo historyBuf;
	NaacSwitchControl switchCtrl;
	NaacThrChannel thrChan;
	NaacTnsData tns;
	float timeSignal[NAAC_FRAME_SAMPLES * 2];
	float mdctSpectrum[NAAC_FRAME_SAMPLES];
	float longWindowSine[NAAC_FRAME_SAMPLES];
	float shortWindowSine[NAAC_SHORT_SAMPLES];
} NaacPsyChannel;

typedef struct NaacCodChannel {
	int32_t blockType;
	int32_t windowShape;
	int32_t numWindowGroups;
	int32_t windowGroupLength[NAAC_MAX_GROUPS];
	int32_t maxSfb;
	int32_t globalGain;
	int32_t scaleFactors[NAAC_MAX_SFB_LONG];
	int32_t codebook[NAAC_MAX_SFB_LONG];
	int32_t quantSpectrum[NAAC_FRAME_SAMPLES];
	float spectrum[NAAC_FRAME_SAMPLES];
	float threshold[NAAC_MAX_SFB_LONG];
} NaacCodChannel;

typedef struct NaacMsChannel {
	int32_t msMaskPresent;
	uint8_t msUsed[NAAC_MAX_SFB_LONG];
} NaacMsChannel;

typedef struct NaacPsychoacoustic {
	int32_t numChannels;
	NaacPsyParam psyParam;
	NaacPsyChannel channels[NAAC_MAX_CHANNELS];
	NaacCodChannel codChannels[NAAC_MAX_CHANNELS];
	NaacMsChannel msChannels[NAAC_MAX_ELEMENTS];
} NaacPsychoacoustic;

typedef struct NaacLoopKernel {
	int32_t numChannels;
	int32_t bitRate;
	int32_t sampleRate;
	int32_t averageBitsPerFrame;
	int32_t bitReservoir;
	int32_t maxBitReservoir;
} NaacLoopKernel;

typedef struct NaacFrameInfo {
	int32_t sampleRate;
	int32_t sampleRateIndex;
	int32_t numChannels;
	int32_t bitRate;
	int32_t audioObjectType;
	int32_t frameBits;
	int32_t writeAdts;
	int32_t protectionAbsent;
	int32_t channelConfig;
} NaacFrameInfo;

typedef struct NaacBitstreamEncoder {
	NaacFrameInfo frameInfo;
	NaacChannelMap channelMap;
	int32_t totalBitsWritten;
	int32_t headerBits;
} NaacBitstreamEncoder;

typedef struct NaacCoreInfo {
	int32_t sampleRate;
	float cutoffFreqHz;
	int32_t numChannels;
	int32_t encoderDelay;
	int32_t bitRate;
	int32_t audioObjectType;
	int32_t sampleRateIndex;
	int32_t maxFrameBits;
} NaacCoreInfo;

typedef struct NaacEncoderInstance {
	int32_t isOpen;
	int32_t writeHeader;
	int32_t optionMode;
	NaacCoreInfo info;
	NaacChannelMap channelMap;
	NaacPsychoacoustic psycho;
	NaacLoopKernel loopKernel;
	NaacBitstreamEncoder bitEnc;
} NaacEncoderInstance;

typedef struct NaacLowpassFilter {
	int32_t numChannels;
	int32_t tapCount;
	int32_t delaySamples;
	float cutoffNorm;
	float prevSample[NAAC_MAX_CHANNELS];
} NaacLowpassFilter;

typedef void (*NaacPcmConvertFn)(float *dst, const void *src, int32_t sampleCount);

typedef struct NaacHandle {
	uint32_t magic;
	int32_t isConfigured;
	NaacEncoderInstance *coreEncoder;
	int32_t lastCoreError;
	NaacCoreInfo *coreInfo;
	NaacChannelMap *channelMap;
	int32_t numChannels;
	int32_t flushFramesRemaining;
	int32_t encoderDelay;
	int32_t channelOrder[NAAC_MAX_CHANNELS];
	int32_t channelStride[NAAC_MAX_CHANNELS];
	uint8_t ascBuffer[32];
	int32_t ascBytes;
	uint8_t adifBuffer[64];
	int32_t adifBytes;
	char commentBuffer[256];
	uint8_t latmBuffer[64];
	int32_t latmBytes;
	int32_t pcmFormat;
	int32_t bytesPerSample;
	NaacPcmConvertFn convertPcm;
	NaacLowpassFilter lowpass;
	int32_t highBitrateMode;
	int32_t bufferedSamples;
	float pcmWork[NAAC_FRAME_SAMPLES * NAAC_MAX_CHANNELS];
	float cutoffGain;
	float cutoffNorm;
	int32_t filterOrder;
	float filterAlpha;
	int32_t pnsDisable;
	void *workBuffer;
	NaacEncoderInstance coreStorage;
} NaacHandle;

/* Math helpers */
float naacFabsf(float x);
float naacSqrtf(float x);
float naacCosf(float x);
float naacSinf(float x);
float naacLog10f(float x);
float naacPowf(float base, float exponent);
float naacAtanf(float x);

/* Bitbuffer & AAC bitstream (bit_aac.c) */
void naacBitBufInit(NaacBitBuffer *bb, uint8_t *storage, int32_t capacityBytes);
void naacBitBufWrite(NaacBitBuffer *bb, uint32_t value, int32_t numBits);
uint32_t naacBitBufRead(NaacBitBuffer *bb, int32_t numBits);
int32_t naacBitBufGetBitCount(const NaacBitBuffer *bb);
int32_t byteAlignment(NaacBitBuffer *bb);
int32_t __f98encodeADTSHeader(NaacBitBuffer *bb, const NaacFrameInfo *frameInfo, int32_t rawDataBytes);
int32_t writeCouplingChannelElement(NaacBitBuffer *bb, const NaacCodChannel *chan);
int32_t naacWriteAudioSpecificConfig(NaacBitBuffer *bb, const NaacCoreInfo *info, int32_t *outBits);
int32_t naacWriteStreamMuxConfig(NaacBitBuffer *bb, const NaacCoreInfo *info, int32_t *outBits);
int32_t naacWriteSingleChannelElement(NaacBitBuffer *bb, const NaacCodChannel *chan, const NaacPsyParam *param, int32_t tag);
int32_t naacWriteChannelPairElement(NaacBitBuffer *bb, const NaacCodChannel *left, const NaacCodChannel *right,
	const NaacMsChannel *ms, const NaacPsyParam *param, int32_t tag);
int32_t naacWriteLfeElement(NaacBitBuffer *bb, const NaacCodChannel *chan, const NaacPsyParam *param, int32_t tag);

/* Frame info (bit_fram.c) */
int32_t __f98InitFrameInfo(NaacFrameInfo *info, int32_t sampleRate, int32_t numChannels, int32_t bitRate, int32_t writeAdts);

/* Bitstream encoder (bit_enc.c) */
int32_t __f98CreateBitstreamEncoder(NaacBitstreamEncoder *enc, const NaacChannelMap *map,
	int32_t sampleRate, int32_t bitRate, int32_t writeAdts);
int32_t __f98AdvanceBitstreamEncoder(NaacBitstreamEncoder *enc, const NaacCodChannel *channels,
	const NaacMsChannel *msChannels, const NaacPsyParam *param, uint8_t *outBuf, int32_t maxOutBytes, int32_t *outBytes);
int32_t __f98WriteBitstream(NaacBitstreamEncoder *enc, NaacBitBuffer *bb, const NaacCodChannel *channels,
	const NaacMsChannel *msChannels, const NaacPsyParam *param);

/* Channel mapping & program config (cm_utils.c, prconfig.c) */
int32_t __f98aacInitChannelMappingFromConfiguration(NaacChannelMap *map, int32_t numChannels);
int32_t __f98aacMakeDefinedProgramConfig(NaacProgramConfig *pce, const NaacChannelMap *map, int32_t sampleRateIndex);

/* Wrapper interface (aacencifc.c) */
int32_t __f98aacSetOption(NaacEncoderInstance *inst, int32_t option);
int32_t __f98aacOpen(NaacEncoderInstance *inst, const NaacChannelMap *map, int32_t sampleRate,
	int32_t bitRate, int32_t writeAdts);
int32_t aacOpen2(NaacEncoderInstance *inst, const NaacChannelMap *map, int32_t sampleRate,
	int32_t bitRate, int32_t writeAdts);
int32_t __f98aacGetInfo(const NaacEncoderInstance *inst, NaacCoreInfo *outInfo);
int32_t naacCoreEncodeFrame(NaacEncoderInstance *inst, const float *interleavedPcm,
	uint8_t *outBuf, int32_t *outBytes, int32_t maxOutBytes);
void naacCoreSetHeaderMode(NaacEncoderInstance *inst, int32_t writeHeader);
void naacCoreClose(NaacEncoderInstance *inst);

/* Psychoacoustic & common (fifo_buf.c, psyparam.c, cb_param.c, calcwindow.c, window.c, sw_cont.c, psy_chan.c, cod_chan.c, ms_chan.c, thr_chan.c, tns.c, psymain.c) */
int32_t __f98CreateFloatFifoBuffer(NaacFloatFifo *fifo, float *backing, int32_t capacity);
int32_t CreateFloatModuloBuffer(NaacFloatModulo *mod, float *backing, int32_t length);
void naacFifoPush(NaacFloatFifo *fifo, const float *src, int32_t count);
void naacFifoPop(NaacFloatFifo *fifo, float *dst, int32_t count);

int32_t naacSampleRateToIndex(int32_t sampleRate);
int32_t __f98CreatePsyParam(NaacPsyParam *param, int32_t sampleRate);
int32_t GetPsyBlockParam(const NaacPsyParam *param, int32_t blockType, int32_t *outSfbCount);
int32_t GetAdaptBlockParam(const NaacPsyParam *param, int32_t blockType, int32_t *outCbCount);

void CalculateBarcCbWidthVal(int32_t sampleRate, int32_t numLines, float *outWidths, int32_t *outNumCb);
void CalculateFixedBarcCbWidthVal(int32_t sampleRate, int32_t numLines, float *outWidths, int32_t *outNumCb);
void CalculateSpreadingFunction(int32_t numCb, const float *widths, float *outSpread);

int32_t __f98CalculateLongWindow(float *window, int32_t length, int32_t shape);
int32_t __f98CalculateShortWindow(float *window, int32_t length, int32_t shape);
void __f98SetWindow(NaacPsyChannel *chan, int32_t blockType, int32_t shape);
void naacComputeMdct(const float *timeIn, float *specOut, int32_t n);

int32_t __f98CreateSwitchControlChannel(NaacSwitchControl *ctrl);
int32_t naacAdvanceSwitchControl(NaacSwitchControl *ctrl, const float *pcm, int32_t length);

int32_t __f98CreatePsyChannel(NaacPsyChannel *chan, const NaacPsyParam *param);
void naacAdvancePsyChannel(NaacPsyChannel *chan, const float *pcm, const NaacPsyParam *param);

int32_t __f98CreateCodChannel(NaacCodChannel *cod, const NaacPsyParam *param);
void naacFillCodChannel(NaacCodChannel *cod, const NaacPsyChannel *psy, const NaacPsyParam *param);

int32_t __f98CreateMSChannel(NaacMsChannel *ms);
void __f98AdvanceMSChannel(NaacMsChannel *ms, NaacCodChannel *left, NaacCodChannel *right, const NaacPsyParam *param);

int32_t __f98CreateThrData(NaacThrData *thr);
int32_t __f98CreateThrChannel(NaacThrChannel *chan);
void naacComputeThresholds(NaacThrChannel *chan, const float *spectrum, const NaacPsyParam *param);

int32_t __f98CreateTnsData(NaacTnsData *tns);
void naacApplyTns(NaacTnsData *tns, float *spectrum, int32_t length);

int32_t __f98CreatePsychoacoustic(NaacPsychoacoustic *psy, int32_t numChannels, int32_t sampleRate);
int32_t naacAdvancePsychoacoustic(NaacPsychoacoustic *psy, const float *interleavedPcm,
	const NaacChannelMap *map, int32_t enableMs, int32_t enableTns);

/* Quantization loop (loopmain.c) */
int32_t __f98CreateLoopKernel(NaacLoopKernel *loop, int32_t numChannels, int32_t sampleRate, int32_t bitRate);
int32_t __f98AdvanceLoopKernel(NaacLoopKernel *loop, NaacCodChannel *channels, const NaacPsyParam *param);

#endif /* NAAC_INTERNAL_H */
