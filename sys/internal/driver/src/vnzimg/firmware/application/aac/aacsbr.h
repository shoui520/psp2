/*
 * Spectral band replication (ISO/IEC 14496-3 4.6.18): the state an SBR
 * decoder keeps per element, and per channel what the fill element sent.
 * The layouts are filled in as the SBR code is.
 */
#pragma once

struct AacDecoder;

/* the frequency band tables (ISO/IEC 14496-3 4.6.18.3.2) */
typedef struct AacSbrBands {
	uint8_t numLow;			/* N_low */
	uint8_t numHigh;		/* N_high */
	uint8_t numNoise;		/* N_Q */
	uint8_t numMaster;		/* N_master */
	uint8_t kx;			/* the first SBR band */
	uint8_t stop;			/* the band after the last */
	uint8_t limiter[13];		/* the limiter band table */
	uint8_t numLimiter;
	uint8_t noiseBands;		/* N_Q again */
	uint8_t reserved2[3];
	uint8_t *table[2];		/* f_TableLow, f_TableHigh */
	uint8_t low[25];
	uint8_t high[49];
	uint8_t noise[6];		/* f_TableNoise */
	uint8_t master[49];		/* f_Master */
	uint8_t reserved3[3];
} AacSbrBands;

/* sbr_header(), and the stream's timing */
typedef struct AacSbrHeader {
	int8_t syncState;		/* 2 once a header was read */
	int8_t error;			/* frames must not be delta coded in time */
	int8_t previousError;		/* the last frame's */
	uint8_t timeSlots;
	uint8_t timeStep;
	uint8_t reserved2;
	uint16_t frameSize;		/* core samples */
	uint32_t outputRate;
	uint16_t status;
	uint8_t mode;			/* 1, or 2 for an SCE's */
	uint8_t ampResolution;
	uint8_t startFreq;
	uint8_t stopFreq;
	uint8_t xoverBand;
	uint8_t freqScale;
	uint8_t alterScale;
	uint8_t noiseBands;
	uint8_t limiterBands;
	uint8_t limiterGains;
	uint8_t interpolFreq;
	uint8_t smoothingMode;
	uint8_t reserved3[2];
	AacSbrBands *bands;
	uint8_t reserved4[4];
} AacSbrHeader;

/* a QMF bank (ISO/IEC 14496-3 4.6.18.4) */
typedef struct AacSbrQmf {
	const int32_t *window;		/* aacSbrQmfWindow */
	const int32_t *cos;		/* the analysis bank's twiddles */
	const int32_t *sin;
	int32_t *analysisBuffer;	/* 320 samples */
	int32_t *synthesisBuffer;	/* 1280 samples */
	int32_t numSlots;		/* numTimeSlots * RATE */
	int32_t numBands;		/* 32, or 64 synthesising at twice the rate */
	int32_t status;			/* the header's */
	int32_t kx;
	int32_t stop;			/* the band after the last */
	int32_t bufferSize;		/* 320 or 640 */
	uint8_t created;
	uint8_t reserved[3];
} AacSbrQmf;

/* one patch of the HF generator (4.6.18.6.3) */
typedef struct AacSbrPatch {
	uint8_t source;			/* the first QMF band copied */
	uint8_t sourceEnd;
	uint8_t target;			/* where it goes */
	uint8_t targetStart;
	uint8_t offset;			/* target - source */
	uint8_t numBands;
	uint8_t reserved[2];
} AacSbrPatch;

typedef struct AacSbrPatches {
	uint8_t numSlots;
	uint8_t numPatches;
	uint8_t sourceLow;		/* the lowest band any patch copies */
	uint8_t sourceHigh;		/* and the highest */
	int32_t bandwidth[5];		/* the chirp factors, Q27 */
	AacSbrPatch patch[6];
	uint8_t noise[12];		/* f_TableNoise from its second entry */
} AacSbrPatches;

/* the HF generator's state */
typedef struct AacSbrHf {
	AacSbrPatches *patches;
	int32_t previousBandwidth[5];	/* the last frame's chirp factors, Q27 */
	uint8_t reserved[4];
	int32_t *lpcReal[2];		/* 32 values each */
	int32_t *lpcImag[2];
} AacSbrHf;

/* what a channel's next frame is decoded against */
typedef struct AacSbrPrevious {
	int32_t envelope[48];
	int32_t noise[5];
	uint8_t ampResolution;
	uint8_t lastBorder;		/* t_E[L_E] less numTimeSlots */
	int8_t coupling;
	uint8_t invfMode[5];
} AacSbrPrevious;

/* the envelope adjuster's state; filled in with its code */
/*
 * The envelope adjuster's state between frames: the last gains and noise
 * levels for the smoothing, and the generators' positions
 */
typedef struct AacSbrAdjust {
	int32_t *history[3];		/* gains, noise levels (48 each), gain exponents */
	int8_t noiseExponent;		/* history[1]'s shared exponent */
	uint8_t reserved[3];
	int32_t first;			/* no history yet: the first envelope fills it */
	int32_t noiseIndex;		/* the noise table's position, 0-511 */
	int32_t previousTransient;	/* envelope 0 when the last frame's transient ended it, or -1 */
	uint32_t sineMask[3];		/* the bands with a sinusoid, 16 bits per word */
	uint8_t phaseIndex;		/* the sinusoids' phase, 0-3 */
	uint8_t reserved2[3];
} AacSbrAdjust;

/* one channel of an SBR element */
typedef struct AacSbrState {
	int32_t *history;		/* AAC_BUFFER_HISTORY */
	AacSbrQmf analysis;
	AacSbrQmf synthesis;
	AacSbrAdjust adjust;
	AacSbrHf hf;
	int32_t timing[7];
} AacSbrState;

typedef struct AacSbrElement {
	AacSbrPrevious *previous;
	AacSbrState state;
} AacSbrElement;

/* parametric stereo; filled in with the PS code */
/*
 * The LPC covariance of one QMF band over the slots (ISO/IEC 14496-3
 * 4.6.18.6.2), every value at one shared scale
 */
typedef struct AacSbrCovariance {
	int32_t phi11;			/* sum of |x(n - 1)|^2 */
	int32_t phi22;			/* sum of |x(n - 2)|^2 */
	int32_t phi01Real;		/* sum of x(n) x*(n - 1) */
	int32_t phi02Real;		/* sum of x(n) x*(n - 2) */
	int32_t phi12Real;		/* sum of x(n - 1) x*(n - 2) */
	int32_t phi01Imag;		/* the imaginary parts; not for a real band */
	int32_t phi02Imag;
	int32_t phi12Imag;
	int32_t determinant;		/* phi11 phi22 - |phi12|^2 */
} AacSbrCovariance;

/* the hybrid filterbank splitting QMF bands 0-2 into 6 (from 8), 2 and 2 subbands */
typedef struct AacPsHybrid {
	const int8_t *bandSplit;	/* each QMF band's filter: 8 or 2 bands */
	int32_t length;			/* the filters' delay, 12 */
	int32_t *buffer[2];		/* real, imaginary filter input: length + 1 used of length + 4 */
	int32_t *delayReal[3];		/* each QMF band's last length slots */
	int32_t *delayImag[3];
	int32_t *filtered[2];		/* the filter's real, imaginary output; 8 values */
} AacPsHybrid;

/* parametric stereo (ISO/IEC 14496-3 8.6.4); the rest is filled in with the PS code */
typedef struct AacPs {
	uint32_t disabled;		/* skip ps_data() */
	int32_t scale;			/* the delay lines' exponent */
	uint8_t stop;			/* the SBR band the delay lines were cleared to */
	int8_t iidPrevious[34];		/* the last frame's last envelope */
	int8_t iccPrevious[34];
	uint8_t headerValid;
	uint8_t dataAvailable;
	uint8_t iidEnable;
	uint8_t iccEnable;
	uint8_t extEnable;
	uint8_t iidMode;
	uint8_t iccMode;
	uint8_t fineIid;
	uint8_t frameClass;
	uint8_t numEnvelopes;
	uint8_t borders[7];		/* the slots the envelopes start at */
	uint8_t iidDt[5];
	uint8_t iccDt[5];
	int8_t iid[7][34];
	int8_t icc[7][34];
	int32_t *qmfDelayReal[41];	/* 14 slots for the first 12 bands, 1 above */
	int32_t *qmfDelayImag[41];
	int32_t ringPosition;		/* the decorrelators' ring, counting down 15-0 */
	int32_t *subbandDelay[46];	/* 32 values, the first 20 */
	int32_t *subbandDelay2[20];	/* 32 values, the first 10 */
	int32_t *power[3];		/* per gain band: peak, smoothed power, smoothed peak excess */
	int32_t *subband[4];		/* the slot's hybrid subbands: real, imaginary, then
					 * decorrelated real, imaginary; 10 each */
	AacPsHybrid hybrid;
	int32_t delayPosition;		/* the 14-slot delay lines' position, counting 0-13 */
	int32_t h[4][22];		/* the mixing matrix at the envelope's end, Q27 */
	int32_t hStart[4][22];		/* and at its start */
	int32_t hDelta[4][22];		/* its step per slot */
} AacPs;

/* an SBR decoder: an SCE's, or a CPE's two channels */
typedef struct AacSbr {
	uint32_t numChannels;
	AacSbrElement element[2];
	AacSbrHeader header;
	uint32_t reserved;
	AacPs ps;
	int32_t *work[3];
	uint8_t reserved2[8];
} AacSbr;

/* sbr_grid() (ISO/IEC 14496-3 4.6.18.3.3) */
typedef struct AacSbrGrid {
	uint8_t frameClass;		/* SBR_FIXFIX ... SBR_VARVAR */
	uint8_t numEnvelopes;		/* L_E */
	uint8_t borders[6];		/* t_E */
	uint8_t freqRes[5];		/* r */
	int8_t pointer;			/* the envelope a noise border splits at; -1 if none */
	uint8_t numNoise;		/* L_Q */
	uint8_t noiseBorders[3];	/* t_Q */
} AacSbrGrid;

#define SBR_FIXFIX	0
#define SBR_FIXVAR	1
#define SBR_VARFIX	2
#define SBR_VARVAR	3

/* one channel's sbr_channel_pair_element() or sbr_single_channel_element() */
typedef struct AacSbrFrame {
	int32_t numEnvelopeValues;	/* over all envelopes */
	AacSbrGrid grid;
	uint8_t dfEnvelope[5];		/* bs_df_env: delta coded in time */
	uint8_t dfNoise[2];
	uint8_t invfMode[5];
	int8_t coupling;		/* 0; 1 left, 2 right of a coupled pair */
	uint8_t reserved;
	int32_t ampResolution;
	uint8_t harmonics[48];		/* bs_add_harmonic */
	int32_t envelope[240];		/* E, as sent */
	int32_t noise[10];		/* Q, as sent */
	uint8_t reserved2[20];
} AacSbrFrame;


#define EXT_SBR_DATA		13
#define EXT_SBR_DATA_CRC	14

/* the SBR extension payload a fill element carried for a channel's element */
typedef struct AacSbrChannel {
	uint32_t element;		/* the SCE or CPE it extends */
	uint32_t extensionType;		/* EXT_SBR_DATA or EXT_SBR_DATA_CRC */
	uint32_t present;		/* its bytes; 0 if this block had none */
	uint8_t data[272];
} AacSbrChannel;

/*
 * The SBR and PS payloads' reader: a circular buffer read MSB first
 * through a 16-bit cache.
 */
typedef struct AacSbrBits {
	const uint8_t *start;
	const uint8_t *last;
	const uint8_t *cursor;
	const uint8_t *buffer;
	int8_t cacheBits;		/* unread bits in the cache */
	uint8_t cacheSize;
	uint16_t cache;
	uint16_t reserved;
	int8_t overrun;			/* fewer bits remain than are cached */
	uint8_t reserved2;
	int32_t remaining;		/* bits */
	int32_t consumed;
} AacSbrBits;

void aacSbrBitsInit(AacSbrBits *bits, const uint8_t *buffer, int32_t size);
void aacSbrBitsCopy(const AacSbrBits *from, AacSbrBits *to);
uint32_t aacSbrReadBits(AacSbrBits *bits, uint8_t count);
void aacSbrSkipBits(AacSbrBits *bits, int32_t count);
int32_t aacSbrClamp(int32_t value, int32_t low, int32_t high);
int32_t aacSbrHalve(int32_t value);

/* aacfixed.c */
int32_t aacFixedNorm64(const int64_t *value);
int32_t aacFixedSaturate(const int64_t *value);
int32_t aacFixedToInt(int64_t *value, int32_t fraction);
void aacFixedFromInt(int64_t *out, int32_t value, int32_t fraction);
int32_t aacFixedShiftLeft(int32_t value, int32_t shift, int32_t fraction);
int32_t aacFixedShiftRight(int32_t value, int32_t shift, int32_t fraction);
int32_t aacFixedNorm(int32_t value);
void aacFixedMultiply(int64_t *out, int32_t a, int32_t b, int32_t fraction);
void aacFixedAdd(int64_t *out, int32_t a, int32_t b, int32_t fraction);
void aacFixedShiftLeft64(int64_t *out, const int64_t *value, int32_t shift, int32_t fraction);
void aacFixedShiftRight64(int64_t *out, const int64_t *value, int32_t shift, int32_t fraction);
void aacSbrFill(int8_t *to, int8_t value, int32_t count);
void aacSbrCopy(int8_t *to, const int8_t *from, int32_t count);
void aacSbrCopy12(int32_t *to, const int32_t *from);
uint8_t aacSbrStartBand(uint32_t rate, uint32_t index);

/* aacsbr.c */
int32_t aacSbrHeaderParse(AacSbrHeader *header, AacSbrBits *bits, uint8_t type,
	AacSbrHeader *previous);
int32_t aacSbrHarmonics(AacSbrHeader *header, AacSbrFrame *frame, AacSbrBits *bits);
int32_t aacSbrSingleChannelElement(AacSbrHeader *header, AacSbrFrame *frame, AacPs *ps,
	AacSbrBits *bits, uint8_t *numBands);
int32_t aacSbrChannelPairElement(AacSbrHeader *header, AacSbrFrame *left, AacSbrFrame *right,
	AacSbrBits *bits, uint8_t *numBands);

/* aacsbrbands.c */
int32_t aacSbrResetBands(AacSbrHeader *header, int32_t *scratch);
int32_t aacSbrLog2Ratio(int32_t low, int32_t high);
int32_t aacSbrDivide(int32_t dividend, int32_t divisor);
void aacSbrSort(uint8_t *values, int32_t count);
int32_t aacSbrHuffman(const int8_t (*tree)[2], AacSbrBits *bits);

/* aacsbrtables.c */
extern const int8_t aacSbrEnvelopeTime15[120][2];
extern const int8_t aacSbrEnvelopeFreq15[120][2];
extern const int8_t aacSbrBalanceTime15[48][2];
extern const int8_t aacSbrBalanceFreq15[48][2];
extern const int8_t aacSbrEnvelopeTime30[62][2];
extern const int8_t aacSbrEnvelopeFreq30[62][2];
extern const int8_t aacSbrBalanceTime30[24][2];
extern const int8_t aacSbrBalanceFreq30[24][2];
extern const int8_t aacSbrNoiseTime30[62][2];
extern const int8_t aacSbrNoiseBalanceTime30[24][2];
extern const int8_t aacPsIidTimeCoarse[28][2];
extern const int8_t aacPsIidFreqCoarse[28][2];
extern const int8_t aacPsIccTime[14][2];
extern const int8_t aacPsIccFreq[14][2];
extern const int8_t aacPsIidTimeFine[60][2];
extern const int8_t aacPsIidFreqFine[60][2];

/* aacsbrhf.c */
int32_t aacSbrBuildPatches(AacSbrHf *hf, uint8_t kx, const uint8_t *master, uint8_t numMaster,
	const uint8_t *noise, uint8_t numNoise, uint8_t stop, uint32_t rate);
int32_t aacSbrHfInit(AacSbrHf *hf, uint8_t kx, const uint8_t *master, uint8_t numMaster,
	uint8_t stop, int32_t timeSlots, uint8_t numSlots, const uint8_t *noise,
	uint8_t numNoise, uint32_t rate, uint8_t index, int32_t channel,
	struct AacDecoder *decoder);
void aacSbrAnalysisInit(AacSbrQmf *qmf, int32_t *timing, int32_t numSlots, int32_t kx,
	int32_t stop, int32_t status, int32_t index, int32_t downsampled, int32_t channel,
	struct AacDecoder *decoder);
void aacSbrSynthesisInit(AacSbrQmf *qmf, int32_t *timing, int32_t numSlots, int32_t kx,
	int32_t stop, int32_t status, uint8_t created, int32_t index, int32_t downsampled,
	int32_t channel, struct AacDecoder *decoder);

/* aacsbrqmf.c */
extern const int32_t aacSbrQmfWindow[1280];
extern const int32_t aacSbrQmfAnalysisWindow[320];
extern const int32_t aacSbrDctSin32[32];
extern const int32_t aacSbrQmfCos32[32];
extern const int32_t aacSbrQmfSin32[32];
extern const int32_t aacSbrQmfCos64[32];
extern const int32_t aacSbrQmfSin64[32];

/* aacps.c */
int32_t aacPsParse(AacPs *ps, AacSbrBits *bits, int32_t count);

/*
 * Dequantised envelope and noise values: a Q26 mantissa in bits 8-27 and
 * a signed exponent in the low byte.
 */
#define SBR_MANTISSA(v)		((v) & 0x0fffff00)
#define SBR_EXPONENT(v)		((int8_t)(v))
#define SBR_FLOAT(m, e)		(SBR_MANTISSA(m) + ((e) & 0xff))

/* aacsbrdata.c */
int32_t aacSbrCrcCheck(AacSbrBits *bits, int32_t count, int32_t *work);
void aacSbrDecodeData(AacSbrHeader *header, AacSbrFrame *left, AacSbrPrevious *leftPrevious,
	AacSbrFrame *right, AacSbrPrevious *rightPrevious, int32_t *work);
void aacPsDecode(AacPs *ps);
void aacPsApply(AacPs *ps, int32_t *timing);
void aacPsUpdate(AacPs *ps, int32_t envelope, int32_t stop);
int32_t aacPsHeadroom(AacPs *ps);
void aacPsRescale(AacPs *ps, int32_t shift);

/* aacsbrelement.c */
void aacSbrAdjustReset(AacSbrAdjust *adjust);

/* aacsbrchannel.c */
void aacSbrSynthesis(int32_t **real, int32_t **imag, int32_t first, int32_t *timing,
	int32_t *output, AacSbrQmf *qmf, AacPs *ps, int32_t psChannel, uint32_t downsampled,
	int32_t *work);
void aacSbrChannelDecode(AacSbrState *state, int32_t *output, int32_t *low, int32_t *high,
	int32_t *buffer, AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous,
	int32_t synced, AacPs *ps, AacSbrQmf *psSynthesis, int32_t *psTiming,
	uint32_t downsampled, int32_t *work, uint8_t created);
int32_t aacSbrStateReset(AacSbrState *state, AacSbrHeader *header,
	AacSbrPrevious *previous, int32_t *work, uint8_t created);

/* aacsbrkernels.c, the IVC2 kernels */
void aacSbrScale(int32_t *values, int32_t count, int32_t shift);
int32_t aacSbrBufferHeadroom(const int32_t *values, int32_t count);
int32_t aacSbrHeadroom(int32_t **real, int32_t **imag, int32_t low, int32_t high,
	int32_t first, int32_t last);
void aacSbrRescale(int32_t **real, int32_t **imag, int32_t low, int32_t high, int32_t first,
	int32_t last, int32_t shift);
int32_t aacSbrHeadroomReal(int32_t **real, int32_t low, int32_t high, int32_t first,
	int32_t last);
void aacSbrRescaleReal(int32_t **real, int32_t low, int32_t high, int32_t first,
	int32_t last, int32_t shift);
void aacSbrCosineTransform(int32_t *data, int32_t length, const int32_t *sine, int32_t *work);
void aacFft(int32_t *data, int32_t length);
void aacSbrModulateReal(int32_t *data, int32_t length, int32_t *out);
void aacSbrModulateImag(int32_t *data, int32_t length, int32_t *out);
void aacSbrModulate64(int32_t *data, int32_t imaginary);
void aacSbrSynthesisTransform(int32_t *data, int32_t length, int32_t *work);
void aacSbrAnalysis(const int32_t *input, int32_t *timing, int32_t **real, int32_t **imag,
	AacSbrQmf *qmf, int32_t *work, uint8_t created);
void aacSbrHfGenerate(AacSbrHf *hf, int32_t *timing, int32_t **real, int32_t **imag,
	int32_t timeStep, int32_t start, int32_t end, int32_t noiseBands, const uint8_t *invfMode,
	const uint8_t *previousInvfMode, int32_t *work);
void aacSbrHfGenerateReal(AacSbrHf *hf, int32_t *timing, int32_t **real, int32_t *aliasDegree,
	int32_t timeStep, int32_t start, int32_t end, int32_t noiseBands, const uint8_t *invfMode,
	const uint8_t *previousInvfMode, int32_t *work);
void aacSbrAdjustEnvelopes(int32_t *timing, AacSbrAdjust *adjust, AacSbrHeader *header,
	AacSbrFrame *frame, int32_t **real, int32_t **imag, int32_t *aliasDegree, int32_t *work,
	uint8_t created);
int32_t aacSbrLimiterBands(uint8_t *limiter, uint8_t *numLimiter, const uint8_t *low,
	int32_t numLow, const AacSbrPatch *patches, int32_t numPatches, int32_t limiterBands,
	int32_t *work);
void aacSbrSynthesisSlot(int32_t *real, int32_t *imag, int32_t lowShift, int32_t highShift,
	int32_t outputShift, AacSbrQmf *qmf, int32_t *work, uint32_t downsampled);
void aacSbrSynthesisSlotReal(int32_t *real, int32_t *imag, int32_t lowShift,
	int32_t highShift, int32_t outputShift, AacSbrQmf *qmf, int32_t *work,
	uint32_t downsampled);
void aacSbrSynthesisOutput(int32_t *output, AacSbrQmf *qmf, uint32_t downsampled);
void aacPsMix(AacPs *ps, int32_t **real, int32_t **imag, int32_t *rightReal,
	int32_t *rightImag, int32_t *timing, int32_t slot, int32_t kx, int32_t *work);
void aacSbrFloatAdd(int32_t am, int8_t ae, int32_t bm, int8_t be, int32_t *mantissa,
	int8_t *exponent);
void aacSbrFloatDivide(int32_t am, int8_t ae, int32_t bm, int8_t be, int32_t *mantissa,
	int8_t *exponent);
void aacSbrFloatSqrt(int32_t *mantissa, int8_t *exponent);
int32_t aacPsCos(int32_t angle);
int32_t aacPsSin(int32_t angle);
int32_t aacPsInit(AacPs *ps, int32_t *history, int32_t *state, int32_t *buffer);
int32_t *aacPsTake(int32_t **cursor, int32_t count, int32_t clear);
void aacPsHybridInit(AacPsHybrid *hybrid, int32_t **cursor);
void aacPsHybridAnalysis(const int32_t *real, const int32_t *imag, int32_t *outReal,
	int32_t *outImag, AacPsHybrid *hybrid, int32_t shift, int32_t *work);
void aacPsHybridSynthesis(const int32_t *real, const int32_t *imag, int32_t *outReal,
	int32_t *outImag, AacPsHybrid *hybrid);

/* VLIW kernels */
void aacSbrCalcBands(uint8_t *diff, int32_t start, int32_t stop, int32_t count);
int32_t aacSbrQuotient(int32_t a, int32_t b);
int32_t aacSbrNorm64(int64_t value);
void aacSbrCovariance(AacSbrCovariance *out, const int32_t *real, const int32_t *imag,
	int32_t count);
void aacSbrCovarianceReal(AacSbrCovariance *out, const int32_t *real, int32_t count);
void aacSbrChirpFactors(AacSbrHf *hf, int32_t count, const uint8_t *invfMode,
	const uint8_t *previousInvfMode, int32_t *out);
