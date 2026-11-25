#ifndef SCE_G729_INTERNAL_H
#define SCE_G729_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

extern void *sceClibMemset(void *dst, int ch, SceSize len);

/* SceG729 error codes */
#define SCE_G729_OK                     0
#define SCE_G729_ERROR_INVALID_PARAM    ((int32_t)0x80680001)
#define SCE_G729_ERROR_NOT_INITIALIZED  ((int32_t)0x80680002)
#define SCE_G729_ERROR_INTERNAL         ((int32_t)0x80680004)
#define SCE_G729_ERROR_CORRUPT_DATA     ((int32_t)0x80680005)

/* ITU-T G.729C+ frame and filter constants */
#define G729_FRAME_LEN       80
#define G729_SUBFRAME_LEN    40
#define G729_LPC_ORDER       10
#define G729_LPC_ORDER_BWD   30
#define G729_LPC_WINDOW_LEN  240
#define G729_PITCH_MIN       20
#define G729_PITCH_MAX       143
#define G729_INTERPOL_LEN    10
#define G729_MA_NP           4
#define G729_NC_ORDER        5
#define G729_PAST_SPEECH_LEN 240
#define G729_MAX_PRM         20

/* Rate selector values for sceG729EncodeInit */
#define G729_RATE_6400       0U
#define G729_RATE_8000       1U
#define G729_RATE_11800      2U

/* Bit lengths per 10 ms frame */
#define G729_BITS_DTX        0U
#define G729_BITS_SID        15U
#define G729_BITS_SID_ALT    16U
#define G729_BITS_6400       64U
#define G729_BITS_8000       80U
#define G729_BITS_11800      118U

/* Context sizes reported by Sony's GetContextSize exports */
#define G729_ENCODER_CONTEXT_SIZE  6048U
#define G729_DECODER_CONTEXT_SIZE  4836U

/* Pre-process / post-process 2nd-order IIR high-pass filter state */
typedef struct G729HpfState {
	float xHistPrev;
	float xHistPrevPrev;
	float yHistPrev;
	float yHistPrevPrev;
} G729HpfState;

/* VAD / DTX / CNG encoder state */
typedef struct G729VadState {
	float meanLsf[G729_LPC_ORDER];
	float meanFullEnergy;
	float meanLowEnergy;
	float meanZeroCross;
	float minEnergy;
	float minEnergyPrev;
	float minEnergyNext;
	float energyFloor;
	int32_t frameCounter;
	int32_t hangoverCount;
	int32_t burstCount;
	int32_t speechFlag;
	int32_t sidFrameCount;
	int32_t prevVadDecision;
	float sidGain;
	float curGain;
	float sidLsf[G729_LPC_ORDER];
	float curLsf[G729_LPC_ORDER];
	float pastExcEnergy[2];
	uint32_t prngSeed;
} G729VadState;

/* Backward LPC / music detector state (Annex E) */
typedef struct G729BwdState {
	float synthBuf[G729_LPC_ORDER_BWD + G729_FRAME_LEN + 40];
	float bwdLpc[G729_LPC_ORDER_BWD + 1];
	float bwdLpcPrev[G729_LPC_ORDER_BWD + 1];
	float autoCorr[G729_LPC_ORDER_BWD + 1];
	float predGainHist[4];
	int32_t bwdDominantCount;
	int32_t prevMode;
	int32_t globalStationarity;
	int32_t musicFlag;
	float meanPitchGain;
} G729BwdState;

/* Core encoder DSP state */
typedef struct G729CoderState {
	float oldSpeech[G729_PAST_SPEECH_LEN];
	float *newSpeech;
	float *speechPtr;
	float *windowPtr;
	float oldWsp[G729_FRAME_LEN + G729_PITCH_MAX];
	float *wspPtr;
	float oldExc[G729_FRAME_LEN + G729_PITCH_MAX + G729_INTERPOL_LEN];
	float *excPtr;
	float zeroBuf[G729_SUBFRAME_LEN];
	float lspOld[G729_LPC_ORDER];
	float lspOldQ[G729_LPC_ORDER];
	float freqPrev[G729_MA_NP][G729_LPC_ORDER];
	float pastQuaEnergy[4];
	float memSyn[G729_LPC_ORDER_BWD];
	float memW0[G729_LPC_ORDER];
	float memW[G729_LPC_ORDER];
	float memErr[G729_LPC_ORDER + G729_SUBFRAME_LEN];
	float sharp;
	G729HpfState hpf;
	G729VadState vad;
	G729BwdState bwd;
	int32_t internalError;
	int32_t frameNumber;
} G729CoderState;

/* Public encoder context (exact size = 6048 bytes) */
typedef struct G729EncoderContext {
	uint32_t activeRate;
	uint32_t activeVad;
	int32_t encodedBits;
	int32_t initialized;
	uint32_t paramRate;
	uint32_t paramVad;
	uint32_t statusRate;
	uint32_t statusVad;
	int32_t statusBits;
	uint32_t statusError;
	G729CoderState coder;
	uint8_t contextTail[G729_ENCODER_CONTEXT_SIZE - 40U - sizeof(G729CoderState)];
} G729EncoderContext;

/* Post-filter state */
typedef struct G729PostFilterState {
	float resBuf[G729_PITCH_MAX + G729_SUBFRAME_LEN + 10];
	float memSynPst[G729_LPC_ORDER_BWD];
	float memZeroPst[G729_LPC_ORDER_BWD];
	float memPre;
	float pastGain;
	float gammaDen;
	float gammaNum;
	float gammaHarm;
	int32_t lpcOrder;
	int32_t shortTermOrder;
} G729PostFilterState;

/* Core decoder DSP state */
typedef struct G729DecoderCoreState {
	float oldExc[G729_FRAME_LEN + G729_PITCH_MAX + G729_INTERPOL_LEN];
	float *excPtr;
	float lspOld[G729_LPC_ORDER];
	float freqPrev[G729_MA_NP][G729_LPC_ORDER];
	float pastQuaEnergy[4];
	float memSyn[G729_LPC_ORDER_BWD];
	float synthFrame[G729_FRAME_LEN];
	float sharp;
	int32_t oldT0;
	float gainCode;
	float gainPitch;
	float prevGainCode;
	float prevGainPitch;
	float prevPitchGainHist[5];
	float prevCodeGainHist[5];
	float prevOnsetGain;
	int32_t prevPrevBfi;
	int32_t prevBfi;
	int32_t prevFrameType;
	uint32_t prngSeed;
	float sidGain;
	float curGain;
	float sidLsf[G729_LPC_ORDER];
	G729HpfState postHpf;
	G729PostFilterState postFilter;
	G729BwdState bwd;
	int32_t lastOrder;
	int32_t lastRate;
	int32_t voicingFlag;
	int32_t prmBuf[G729_MAX_PRM + 4];
	int32_t internalError;
} G729DecoderCoreState;

/* Public decoder context (exact size = 4836 bytes) */
typedef struct G729DecoderContext {
	uint32_t activeBitLen;
	uint32_t activeBfi;
	int32_t initialized;
	uint32_t paramBitLen;
	uint32_t paramBfi;
	int32_t decodedBitLen;
	uint32_t statusBfi;
	uint32_t statusError;
	G729DecoderCoreState core;
	uint8_t contextTail[G729_DECODER_CONTEXT_SIZE - 32U - sizeof(G729DecoderCoreState)];
} G729DecoderContext;

/* Tables (g729_tables.c) */
extern const float g_g729HamWindow[G729_LPC_WINDOW_LEN];
extern const float g_g729LagWindow[G729_LPC_ORDER];
extern const float g_g729LspInit[G729_LPC_ORDER];
extern const float g_g729MaPred[2][G729_MA_NP][G729_LPC_ORDER];
extern const float g_g729MaPredSum[2][G729_LPC_ORDER];
extern const float g_g729MaPredSumInv[2][G729_LPC_ORDER];
extern const float g_g729GainCodebookA[8][2];
extern const float g_g729GainCodebookB[16][2];
extern const float g_g729Inter3L[31];
extern const float g_g729Inter3[13];
extern const float g_g729ChebGrid[61];
extern const int32_t g_g729BitsNo8k[11];
extern const int32_t g_g729BitsNo64k[11];
extern const int32_t g_g729BitsNo118kFwd[14];
extern const int32_t g_g729BitsNo118kBwd[12];
extern const int32_t g_g729BitsNoSid[4];

/* Math helpers (g729_math.c) */
float g729Fabsf(float value);
float g729Sqrtf(float value);
float g729Floorf(float value);
float g729Cosf(float angle);
float g729Acosf(float value);
float g729Log10f(float value);
float g729Powf(float base, float exponent);
uint16_t g729PrngNext(uint32_t *seed);

/* Bitstream packing / unpacking (g729_bits.c) */
int32_t g729ParityPitch(int32_t pitchIndex);
int32_t g729CheckParityPitch(int32_t pitchIndex, int32_t parityBit);
int32_t g729Prm2Bits(const int32_t *prm, uint8_t *outBytes, int32_t *outBitCount, uint32_t rate);
int32_t g729Bits2Prm(const uint8_t *inBytes, int32_t *prm, uint32_t bitLength, int32_t *bfi);

/* LPC / LSP / Filter helpers (g729_lpc.c) */
void g729Autocorr(const float *samples, int32_t order, float *corr);
void g729LagWindow(int32_t order, float *corr);
float g729Levinson(const float *corr, float *lpc, float *refl, int32_t order);
void g729AzLsp(const float *lpc, float *lsp, const float *oldLsp);
void g729LspAz(const float *lsp, float *lpc);
void g729IntQlpc(const float *lspOld, const float *lspNew, float *azBoth);
void g729IntLpc(const float *lspOld, const float *lspNew, float *lsfInt, float *lsfNew, float *azBoth);
void g729LspLsf(const float *lsp, float *lsf, int32_t order);
void g729LsfLsp(const float *lsf, float *lsp, int32_t order);
void g729WeightAz(const float *lpc, float gamma, int32_t order, float *weighted);
void g729Residu(const float *lpc, const float *input, float *residual, int32_t length, int32_t order);
void g729SynFilt(const float *lpc, const float *exc, float *synth, int32_t length, float *mem, int32_t update, int32_t order);
void g729Convolve(const float *vecA, const float *vecB, float *out, int32_t length);
void g729QuaLsp(const float *lsp, float *lspQ, int32_t *indices, float freqPrev[G729_MA_NP][G729_LPC_ORDER]);
void g729DecLsp(const int32_t *indices, float *lspQ, int32_t bfi, float freqPrev[G729_MA_NP][G729_LPC_ORDER]);
void g729BwdLpcAnalysis(G729BwdState *bwd, const float *synthFrame);

/* Encoder core (g729_coder.c) */
void g729PreProcessInit(G729HpfState *hpf);
void g729PreProcess(float *signal, int32_t length, G729HpfState *hpf);
void g729VadInit(G729VadState *vad);
int32_t g729VadDecision(G729VadState *vad, const float *refl, const float *lsf, const float *corr);
void g729InitCoder(int32_t vadEnable, G729CoderState *state);
int32_t g729CoderFrame(int32_t *prm, int32_t frameNum, int32_t vadEnable, uint32_t rate, G729CoderState *state);

/* Decoder core (g729_decoder.c) */
void g729PostProcessInit(G729HpfState *hpf);
void g729PostProcess(float *signal, int32_t length, G729HpfState *hpf);
void g729PostFilterInit(G729PostFilterState *pf);
void g729PostFilter(int32_t pitchDelay, const float *synthIn, const float *lpc, float *synthOut, int32_t *voicing, int32_t maxOrder, int32_t order, G729PostFilterState *pf);
void g729PostFilterShort(const float *synthIn, const float *lpc, const int32_t *pitchDelays, float *synthOut, G729PostFilterState *pf);
void g729InitDecoder(G729DecoderCoreState *state);
void g729DecodeFrame(const int32_t *prm, int32_t voicing, float *synthOut, float *azDec, int32_t *pitchDelays, int32_t *outOrder, G729DecoderCoreState *state);

/* Public API exports (g729_api.c) */
int32_t sceG729EncodeGetContextSize(void);
int32_t sceG729EncodeInit(void *context, uint32_t rate, uint32_t vadEnable);
int32_t sceG729EncodeReset(void *context);
int32_t sceG729EncodeCore(void *context, const int16_t *pcmIn, uint8_t *bitstreamOut);
int32_t sceG729DecodeGetContextSize(void);
int32_t sceG729DecodeReset(void *context);
int32_t sceG729DecodeInit(void *context);
int32_t sceG729DecodeCore(void *context, uint32_t bitLength, uint32_t bfi, const uint8_t *bitstreamIn, int16_t *pcmOut);

#endif /* SCE_G729_INTERNAL_H */
