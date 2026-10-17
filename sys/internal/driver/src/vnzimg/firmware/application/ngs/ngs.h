/*
 * NGS, the Vita's audio synthesis system (Sony's SDK, ngs_top.h): the
 * Venezia side of its systems, racks and voices. The ARM side (libngs) owns
 * these structures and builds them in shared memory; the firmware reads
 * them, runs each voice's chain of modules as VThread jobs, and writes back
 * the voices' states.
 *
 * The names are the SDK's where it gives any (SceNgsSystemInitParams,
 * SceNgsCallbackInfo, the voice states, the module ids); the rest describe
 * what the firmware does with them.
 */
#pragma once

#include "../application.h"

/* Sony's SDK, ngs/Error.h */
#define SCE_NGS_ERROR				((int32_t)0x804A0001)
#define SCE_NGS_ERROR_INVALID_PARAM		((int32_t)0x804A0002)
#define SCE_NGS_ERROR_INVALID_ALIGNMENT		((int32_t)0x804A0003)
#define SCE_NGS_ERROR_NOT_IMPL			((int32_t)0x804A0004)
#define SCE_NGS_ERROR_DEPENDENCY		((int32_t)0x804A0005)
#define SCE_NGS_ERROR_OUT_OF_ASSETS		((int32_t)0x804A0006)
#define SCE_NGS_ERROR_MODULE_NOT_AVAIL		((int32_t)0x804A0007)
#define SCE_NGS_ERROR_RESOURCE_LOCKED		((int32_t)0x804A0008)
#define SCE_NGS_ERROR_PARAM_OUT_OF_RANGE	((int32_t)0x804A0009)
#define SCE_NGS_ERROR_INVALID_VOICE_TYPE	((int32_t)0x804A000A)
#define SCE_NGS_ERROR_SYSTEM_MISMATCH		((int32_t)0x804A000B)
#define SCE_NGS_ERROR_INVALID_HANDLE		((int32_t)0x804A000C)
#define SCE_NGS_ERROR_SIZE_MISMATCH		((int32_t)0x804A000D)
#define SCE_NGS_ERROR_PATCH_NOT_AVAIL		((int32_t)0x804A000E)
#define SCE_NGS_ERROR_PARAM_TYPE_MISMATCH	((int32_t)0x804A000F)
#define SCE_NGS_ERROR_INVALID_STATE		((int32_t)0x804A0010)
#define SCE_NGS_ERROR_INTERNAL_ALLOC		((int32_t)0x804A0011)
#define SCE_NGS_ERROR_INTERNAL_PROCESSING	((int32_t)0x804A0012)
#define SCE_NGS_ERROR_INVALID_BUFFER		((int32_t)0x804A0013)

/* Sony's SDK, ngs/modules/<module>.h */
#define SCE_NGS_AT9_ID				0x5CAA
#define SCE_NGS_INPUT_MIXER_ID			0x5CE0
#define SCE_NGS_COMPRESSOR_ID			0x5CE1
#define SCE_NGS_DISTORTION_ID			0x5CE2
#define SCE_NGS_ENVELOPE_ID			0x5CE3
#define SCE_NGS_FILTER_ID			0x5CE4
#define SCE_NGS_PAUSER_ID			0x5CE5
#define SCE_NGS_PLAYER_ID			0x5CE6
#define SCE_NGS_REVERB_ID			0x5CE7
#define SCE_NGS_GENERATOR_ID			0x5CE8
#define SCE_NGS_MIXER_ID			0x5CE9
#define SCE_NGS_PITCHSHIFT_ID			0x5CEA
#define SCE_NGS_DELAY_ID			0x5CEB
#define SCE_NGS_PARAM_EQ_ID			0x5CEC
#define SCE_NGS_OUTPUT_ID			0x5CED

/* Sony's SDK, ngs_top.h: SCE_NGS_VOICE_STATE_* */
#define SCE_NGS_VOICE_STATE_ACTIVE		1
/* not in the SDK: keyed off, the modules finishing (FINALIZING to the ARM) */
#define NGS_VOICE_STATE_KEYED_OFF		2
#define SCE_NGS_VOICE_STATE_FINALIZING		4
#define SCE_NGS_VOICE_STATE_UNLOADING		8
#define SCE_NGS_VOICE_STATE_PENDING		16
#define SCE_NGS_VOICE_STATE_PAUSED		32
#define SCE_NGS_VOICE_STATE_KEY_OFF		64

/* Sony's SDK, ngs_top.h */
#define SCE_NGS_MAX_SYSTEM_CHANNELS		2
#define SCE_NGS_MAKE_PARAMS_ID(module_id, index, ver) \
	(((module_id) & 0x0000ffff) | (((ver) & 0x000000ff) << 16) | (((index) & 0x000000ff) << 24))

typedef struct SceNgsParamsDescriptor {
	uint32_t id;
	uint32_t size;
} SceNgsParamsDescriptor;

#ifndef max
#define max(a, b)			((a) > (b) ? (a) : (b))
#define min(a, b)			((a) < (b) ? (a) : (b))
#endif

/* 8.24 fixed point */
#define NGS_ONE				(1 << 24)
#define NGS_FLOAT_OF_FIXED		(1.0f / 16777216.0f)

static inline int32_t ngsFixed(float value)
{
	return (int32_t)(value * 16777216.0f);
}

#define NGS_SYSTEMS			8
#define NGS_MODULE_TYPES		32
/* no module: a free callback entry of a voice's status */
#define NGS_NO_MODULE			NGS_MODULE_TYPES

/* the module table's work memory: 32 buffers, each taken under its lock */
#define NGS_WORK_BUFFERS		32
#define NGS_WORK_BUFFER_SIZE		6144
#define NGS_MODULE_TABLE_HEADER		768
#define NGS_MODULE_TABLE_SIZE		(NGS_MODULE_TABLE_HEADER + \
					 NGS_WORK_BUFFERS * NGS_WORK_BUFFER_SIZE)

/* a rack's voices get up to four channel buffers of up to 512 samples */
#define NGS_VOICE_BUFFERS		4
#define NGS_MAX_GRANULARITY		512
#define NGS_SAMPLE_SIZE			4

typedef struct NgsBuffer NgsBuffer;
typedef struct NgsModuleParams NgsModuleParams;

/* the buffers a module reads or writes */
typedef struct NgsBufferList {
	int32_t count;
	NgsBuffer **buffers;
} NgsBufferList;

typedef int32_t (*NgsModuleFunction)(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);

/* one module type: the entries of the module table */
typedef struct NgsModuleType {
	uint32_t moduleId;
	NgsModuleFunction initialize;
	NgsModuleFunction process;
	uint32_t auxiliary;		/* given to the ARM by RPC 327 */
} NgsModuleType;

typedef struct NgsModuleTable {
	NgsModuleType types[NGS_MODULE_TYPES];
	uint32_t workBufferSize;
	uint32_t size;
	uint32_t initializedMark;	/* 6, never read */
	uint8_t *workBuffers;
	volatile uint8_t *workLocks[NGS_WORK_BUFFERS];
	volatile uint8_t *lock;
} NgsModuleTable;

/* sceNgsSystemInit's parameters, as RPC 320 gets them */
typedef struct NgsSystemInitParams {
	uint32_t nMaxRacks;
	uint32_t nMaxVoices;
	uint32_t nGranularity;
	uint32_t nSampleRate;
} NgsSystemInitParams;

typedef struct NgsSystem {
	VkVThreadContext *context;
	VkVThreadResource *resource;
	uint32_t sampleRate;
	uint32_t granularity;
	uint32_t maxRacks;
	uint32_t maxVoices;
	NgsModuleTable *moduleTable;
	uint8_t *rackSlots;		/* maxRacks of 8 bytes, after the system */
	uint32_t size;
	uint8_t *update;		/* the update being run (RPC 321) */
} NgsSystem;

/* the ARM's handle on everything: the systems and the module table */
typedef struct NgsGlobal {
	uint32_t systemCount;
	NgsSystem *systems[NGS_SYSTEMS];
	NgsModuleTable *moduleTable;
} NgsGlobal;

/* a voice's channel buffers (built in work memory by ngsBufferCreate) */
struct NgsBuffer {
	uint32_t header[4];		/* the rack's, for each output */
	uint32_t reserved;
	uint32_t samples;
	int16_t channels;
	uint16_t channelMask;		/* channels that hold samples */
	uint8_t **channelData;
	uint32_t reserved3;
	uint32_t flags;			/* NGS_BUFFER_* */
	uint32_t channelSize;
	uint8_t *data[];		/* channelData points here */
};

#define NGS_BUFFER_VALID		0x1
#define NGS_BUFFER_END			0x2

/* where a module of a rack keeps its state and parameters in a voice */
typedef struct NgsModuleSlot {
	uint32_t userParamsOffset;
	uint32_t settingsOffset;
	uint32_t hasSettings;
	uint32_t statesOffset;
	uint32_t hasStates;
} NgsModuleSlot;

/* a rack: the voices that share one definition (SceNgsRackDescription) */
typedef struct NgsRack {
	uint32_t moduleCount;
	uint32_t channels;
	uint32_t reserved;
	uint32_t patchesPerOutput;
	uint32_t reserved1;
	uint32_t size;
	uint32_t bufferHeader[4];
	NgsModuleFunction *process;	/* module ids until RPC 323 resolves them */
	NgsModuleFunction *initialize;
	uint8_t *inputCounts;		/* per module */
	uint8_t *outputCounts;
	uint8_t *moduleFlags;
	NgsModuleSlot *modules;
	uint32_t reserved2[5];
	uint32_t scratchSize;
	uint32_t volumesOffset;		/* of the patches' applied volumes in a voice's settings */
	uint32_t maskOffset;		/* of the voice masks in a voice's settings */
	uint32_t *finishedVoices;	/* the voices to report, and their count */
	uint32_t *finishedCount;
	volatile uint8_t *lock;
	uint32_t bufferCount;
} NgsRack;

/* rack module flags, when the module is bypassed */
#define NGS_MODULE_PASS_THROUGH		0x1
#define NGS_MODULE_NO_COPY		0x2

/* a module's callback raised in an update (SceNgsCallbackInfo's data) */
typedef struct NgsCallback {
	uint32_t module;		/* NGS_NO_MODULE when free */
	uint32_t callbackPtr;
	int32_t callbackData;
	int32_t callbackData2;
} NgsCallback;

/* what a voice reports back to the ARM after each update */
typedef struct NgsVoiceStatus {
	uint32_t activeModules;
	uint32_t state;
	uint32_t reserved[2];
	int16_t error;
	int16_t errorModule;
	NgsCallback callbacks[2];
} NgsVoiceStatus;

/* a voice's state, from the update status */
#define NGS_STATUS_UNCHANGED		0x40
#define NGS_STATUS_CALLBACK_ONLY	0x80

/* the masks at the rack's maskOffset of a voice's params */
typedef struct NgsVoiceMasks {
	uint32_t enabled;		/* modules that run */
	uint32_t reset;			/* modules to initialize first */
} NgsVoiceMasks;

typedef struct NgsVoice {
	uint32_t size;
	uint32_t reserved[2];
	uint32_t settingsSize;
	uint32_t reserved2;
	uint32_t statesSize;
	uint32_t reserved3;
	uint32_t userParamsSize;
	uint32_t reserved4[2];
	uint32_t handle;		/* reported when the voice changed */
	uint32_t reserved5[5];
	uint32_t state;			/* SCE_NGS_VOICE_STATE_* */
	uint32_t bypassed;		/* module mask */
	uint32_t reset;			/* modules to initialize, added to the masks */
	uint32_t reserved6;
	uint32_t statusSize;
	uint8_t *settings;		/* the modules' working parameters, and the masks */
	uint8_t *states;		/* what the modules report (SceNgs*States) */
	uint8_t *userParams;		/* the modules' parameters as the user set them */
	uint8_t *inputRoutes;		/* a byte per module input */
	uint8_t *outputRoutes;
	uint32_t reserved7;
	NgsRack *rack;
	struct NgsPatch *patches;	/* patchesPerOutput for each output */
	NgsVoiceStatus *status;
} NgsVoice;

#define NGS_MIXER_PORTS			6
#define NGS_MIXER_CHANNELS		2

/* an input mixer's settings: the ports other voices' patches write into */
typedef struct NgsInputMixerSettings {
	uint32_t ports;
	uint32_t channels;
	volatile uint8_t *locks[NGS_MIXER_PORTS];	/* held while a patch writes */
	int32_t *buffers[NGS_MIXER_PORTS][NGS_MIXER_CHANNELS];
	uint32_t reset[NGS_MIXER_PORTS];		/* 1: clear the port before use */
} NgsInputMixerSettings;

/* a voice's patch to another voice's input mixer */
typedef struct NgsPatch {
	uint32_t reserved[2];
	NgsInputMixerSettings *mixer;
	const int32_t *volumes;		/* the matrix asked for, 8.24, [from][to] */
} NgsPatch;

/* a route byte: a buffer index, or 0x80 | index << 4 | channel */
#define NGS_ROUTE_CHANNEL		0x80

/* the module flags of a run */
#define NGS_RUN_ENABLED			0x1
#define NGS_RUN_BYPASSED		0x2
#define NGS_RUN_KEEP_PENDING		0x4
#define NGS_RUN_STOP			0x8

/* what a module function gets besides its buffers */
struct NgsModuleParams {
	uint32_t sampleRate;
	uint32_t voiceState;
	uint32_t scratchSize;
	uint8_t *scratch;
	uint32_t flags;			/* NGS_RUN_*, modules may add KEEP_PENDING and STOP */
	uint32_t userParamCount;	/* the module's user parameters, when initializing */
	uint8_t **userParams;
	uint32_t dataCount;		/* its settings and states */
	uint8_t **data;
	uint32_t *extra;
	uint32_t callbackRequested;
	int32_t callbackData;
	int32_t callbackData2;
	uint32_t callbackPtr;
};

/* the jobs of an update, in order */
typedef struct NgsJob {
	uint32_t type;			/* NGS_JOB_* */
	uint32_t offset;		/* of the job's object in the update; a task's address */
	uint32_t wanted;		/* signals before it runs; a task's job to signal */
} NgsJob;

#define NGS_JOB_VOICE			0
#define NGS_JOB_BARRIER			1
#define NGS_JOB_TASK			2
#define NGS_JOB_NONE			3

/* a buffer the ARM gives or takes in an update */
typedef struct NgsTransfer {
	uint32_t inputAddress;
	uint32_t inputSize;
	uint32_t outputAddress;
	uint32_t outputSize;
	uint32_t reserved;
	uint32_t output2Address;
	uint32_t output2Size;
	uint32_t *done;
} NgsTransfer;

/* sceNgsSystemUpdate's request (RPC 321) */
typedef struct NgsUpdate {
	uint32_t reserved;
	uint32_t jobCount;
	uint32_t reserved2[3];
	uint32_t transferCount;
	uint32_t transferCapacity;
	NgsJob *jobs;
	NgsTransfer *transfers;
} NgsUpdate;

/* a barrier between the jobs of an update: signals other jobs */
typedef struct NgsBarrier {
	uint32_t reserved[4];
	int32_t signalCount;
	int32_t next;			/* -1: the last, which wakes the update */
	uint32_t reserved2;
	uint32_t size;
	uint32_t *signals;
} NgsBarrier;

/* a function run in an update with work memory */
typedef struct NgsTask {
	uint32_t address;
	uint32_t size;
	void (*function)(uint32_t address, uint32_t size, uint8_t *work, uint32_t workSize);
	uint32_t workSize;
} NgsTask;

/* an array of locks for the ARM (RPC 325, 326) */
typedef struct NgsLockArray {
	uint32_t count;
	uint32_t reserved;
	volatile uint8_t *locks[];
} NgsLockArray;

/* application/ngs/system.c */
int32_t ngsSystemCreate(NgsGlobal *global, uint32_t limits, uint32_t granularity,
	uint32_t sampleRate);
int32_t ngsSystemUpdate(NgsGlobal *global, NgsUpdate *update, uint32_t size, uint32_t index);
int32_t ngsSystemRelease(NgsGlobal *global, int32_t index);
int32_t ngsRackInit(NgsGlobal *global, int32_t index, NgsRack *rack, uint32_t size);
int32_t ngsRackRelease(NgsGlobal *global, int32_t index, NgsRack *rack, uint32_t size);
int32_t ngsLocksCreate(NgsLockArray *locks, uint32_t size);
int32_t ngsLocksDelete(NgsLockArray *locks, uint32_t size);
uint32_t ngsModuleAuxiliary(NgsGlobal *global, uint32_t moduleId);
uint32_t ngsRoundUp256(uint32_t size);
uint32_t ngsRoundUp(uint32_t size, uint32_t alignment);
uint32_t ngsCursorAllocate(uint32_t *cursor, uint32_t size, uint32_t alignment);
uint32_t ngsCursorAllocate256(uint32_t *cursor, uint32_t size);

/* the samples a module works on at a time */
#define NGS_BLOCK			64

/* the filter's parameters and state, shared with the parametric EQ */
#define SCE_NGS_FILTER_PARAMS_STRUCT_ID		SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_FILTER_ID, 1, 1)
#define SCE_NGS_FILTER_PARAMS_COEFF_STRUCT_ID	SCE_NGS_MAKE_PARAMS_ID(SCE_NGS_FILTER_ID, 2, 1)

/* SceNgsFilterParams, SceNgsFilterParamsCoEff: one per channel */
typedef struct NgsFilterUserParams {
	SceNgsParamsDescriptor desc;
	uint32_t eFilterMode;
	float fFrequency;
	float fResonance;
	float fGain;
} NgsFilterUserParams;

typedef struct NgsFilterUserCoefficients {
	SceNgsParamsDescriptor desc;
	float fB0;
	float fB1;
	float fB2;
	float fA1;
	float fA2;
} NgsFilterUserCoefficients;

/* y0 = x0 b0 + x1 b1 + x2 b2 + y1 a1 + y2 a2, in 8.24 (a1, a2 negated) */
typedef struct NgsBiquad {
	int32_t b0;
	int32_t b1;
	int32_t b2;
	int32_t a1;
	int32_t a2;
	int32_t x1;
	int32_t x2;
	int32_t y1;
	int32_t y2;
} NgsBiquad;

/* application/ngs/math.c */
int32_t ngsDivide(int32_t a, int32_t b);
int32_t ngsLog2(uint32_t x);
int32_t ngsExp2(int32_t x);
uint32_t ngsSqrt(uint32_t x);
int32_t ngsDecibelsToLevel(int32_t decibels);
int32_t ngsLevelToDecibels(int32_t level);

/* application/ngs/buffer.c */
NgsBuffer *ngsBufferCreate(int32_t channels, uint32_t sampleSize, uint32_t samples,
	uint32_t *cursor, uint32_t measure);
uint32_t ngsBufferSize(int32_t channels, uint32_t sampleSize, uint32_t samples);

/* the modules (application/ngs/<module>.c) */
int32_t ngsCompressorInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsCompressorProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsDistortionInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsDistortionProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsEnvelopeInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsEnvelopeProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsFilterInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsFilterProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsInputMixerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsInputMixerProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsMixerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsMixerProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsOutputInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsOutputProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsParamEqInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsParamEqProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPauserInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPauserProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPlayerInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPlayerProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsReverbInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsReverbProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsReverbAuxiliary(void *state, void *buffers, uint8_t *scratch);
int32_t ngsGeneratorInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsGeneratorProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsAt9Init(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsAt9Process(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsDelayInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsDelayProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPitchShiftInit(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);
int32_t ngsPitchShiftProcess(NgsBufferList *inputs, NgsBufferList *outputs, NgsModuleParams *params);

/* a window on the user's memory, as the ARM set it for a player */
typedef struct NgsUserWindow {
	uint32_t address;
	uint32_t mapped;
	uint32_t size;
} NgsUserWindow;

/* player.c: 16-bit PCM to 8.24, shared with the AT9 player */
void ngsPcmMono(int32_t *destination, const int16_t *source, int32_t samples);
void ngsPcmSplit(int32_t *left, int32_t *right, const int16_t *source, int32_t samples);
void ngsPcmLeft(int32_t *destination, const int16_t *source, int32_t samples);
void ngsPcmRight(int32_t *destination, const int16_t *source, int32_t samples);

/* math.c */
#define NGS_WINDOW_SIZE			256
extern const int32_t ngsWindow[NGS_WINDOW_SIZE];
/* where fades start in ngsWindow */
extern const int32_t ngsFadeStart;

/* reverb.c */
void ngsFadeOut(NgsBufferList *inputs);

/* VLIW code (0x8787c0, 0x8788f8): base^exponent and log10, in 8.24 */
int32_t ngsPower(int32_t base, int32_t exponent);
int32_t ngsLog10(int32_t x);

/* VLIW kernels on blocks of samples */
int32_t ngsMultiply(int32_t a, int32_t b);			/* 0x878778 */
void ngsClear(void *destination, uint32_t size);		/* 0x878838 */
int32_t ngsSin(int32_t x);					/* 0x878970 */
int32_t ngsSinCos(int32_t x, int32_t *cosine);			/* 0x878a68 */
int32_t ngsCos(int32_t x);					/* 0x878ba0 */
int32_t ngsPeakLevel(const int32_t *data, uint32_t count);	/* 0x878ca0 */
int32_t ngsRmsLevel(const int32_t *data, uint32_t count);	/* 0x878d58 */
void ngsCompressorApply(const int32_t *input, int32_t *output, uint32_t count,
	int32_t level, void *settings, NgsModuleParams *params, int32_t channel); /* 0x878e98 */
void ngsDistortionApply(const int32_t *input, int32_t *output, void *settings,
	uint32_t count);						/* 0x879590 */
int32_t ngsEnvelopeRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x879f68 */
void ngsCopy(void *destination, const void *source, uint32_t size);	/* 0x878890 */
int32_t ngsFilterDesign(float frequency, float resonance, uint32_t mode, float gain,
	NgsBiquad *filter, uint32_t sampleRate);			/* 0x87ac68 */
void ngsFilterStereo(const int32_t *input, const int32_t *input2, int32_t *output,
	int32_t *output2, uint32_t samples, NgsBiquad *filter, NgsBiquad *filter2); /* 0x87bf80 */
int32_t ngsInputMixerBypass(NgsBufferList *outputs, NgsModuleParams *params); /* 0x87c378 */
void ngsInputMixerClear(int32_t *buffer, uint32_t size);
int32_t ngsInputMixerRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x87c528 */
int32_t ngsMixerRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x87cc30 */
void ngsOutputRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x87da30 */
int32_t ngsPauserRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x87dd68 */
void ngsGeneratorSine(int32_t *output, void *settings, uint32_t samples,
	int32_t last);							/* 0x87e960 */
uint32_t ngsGeneratorPulse(int32_t *output, void *settings, uint32_t samples,
	NgsModuleParams *params);					/* 0x87e648 */
uint32_t ngsGeneratorTriangle(int32_t *output, void *settings, uint32_t samples,
	NgsModuleParams *params);					/* 0x87ebd0 */
uint32_t ngsGeneratorSaw(int32_t *output, void *settings, uint32_t samples,
	NgsModuleParams *params);					/* 0x87eef8 */
/* the ATRAC9 decoder (at9/at9decoder.c); a call decodes the next frame of the superframe */
struct At9DecoderState;

int32_t ngsAt9DecoderInit(struct At9DecoderState *decoder, int32_t channels, uint8_t **work,
	const uint32_t *config, int32_t physical);				/* 0x8809d8 */
void ngsAt9DecoderReset(struct At9DecoderState *decoder);			/* 0x881338 */
int32_t ngsAt9ConfigInfo(const uint32_t *config, int32_t *rateIndex, int32_t *channelConfig,
	uint32_t *superframeBytes, uint32_t *framesPerSuperframe);	/* 0x881558 */
int32_t ngsAt9Skip(struct At9DecoderState *decoder, const uint8_t *data, int32_t *used); /* 0x880b28 */
int32_t ngsAt9Decode(struct At9DecoderState *decoder, const uint8_t *data, int32_t *used,
	int16_t *pcm);							/* 0x880c80 */
int32_t ngsAt9SkipSuperframe(struct At9DecoderState *decoder, const uint8_t *data, uint32_t *used); /* 0x880f00 */
int32_t ngsAt9DecodeSuperframe(struct At9DecoderState *decoder, const uint8_t *data, uint32_t *used,
	int16_t *pcm);							/* 0x881088 */
int32_t ngsAt9DecodeRange(struct At9DecoderState *decoder, const uint8_t *data, uint32_t *used,
	int16_t *pcm, uint32_t skip, uint32_t count);			/* 0x881170 */
/* the bytes and the frames of the superframe still to come */
int32_t ngsAt9DecoderInputLeft(struct At9DecoderState *decoder);		/* 0x8814e8 */
int32_t ngsAt9DecoderOutputLeft(struct At9DecoderState *decoder);		/* 0x881510 */
int32_t ngsAt9DecoderChannels(struct At9DecoderState *decoder);		/* 0x881538 */
int32_t ngsAt9DecoderSamples(struct At9DecoderState *decoder);			/* 0x881548 */
int32_t ngsAt9ChannelCount(int32_t channelConfig);			/* 0x8817b0 */
void ngsDelayRun(void *settings, const int32_t *input, int32_t *output, uint32_t samples,
	int32_t active);						/* 0x87f1d0 */
void ngsDelayBypass(void *settings, const int32_t *input, int32_t *output,
	uint32_t samples);						/* 0x87fd90 */
int32_t ngsPitchShiftRun(NgsBufferList *inputs, NgsBufferList *outputs,
	NgsModuleParams *params);					/* 0x8805f8 */
void ngsHevagDecode(int32_t *history, const uint8_t *block, int16_t *samples,
	int16_t filter, int16_t shift);					/* 0x8add60 */
int32_t ngsAdpcmResample(uint32_t *fraction, int32_t *output, const int16_t *input,
	int32_t samples, int32_t rate);					/* 0x8adec8 */
int32_t ngsPlayerResample(uint32_t *fraction, int32_t *output, const int16_t *input,
	int32_t inputSamples, int32_t samples, int32_t rate, int32_t channels,
	int32_t channel);						/* 0x8ae2e0 */

/* VLIW code (0x877d48): a voice's routed channel, after its module ran */
void ngsRouteChannel(NgsBuffer *buffer, NgsVoice *voice, int32_t channel);
