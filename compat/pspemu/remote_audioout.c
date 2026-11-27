/*
 * remote_audioout.c: Kermit audio output server (channel 5), dual-channel
 * mixer, interrupt pacing threads, and 4-tap polyphase resampler.
 */
#include "host_common.h"

#include <string.h>

#define AUDIOOUT_CHANNEL_ID          5
#define AUDIOOUT_RING_SAMPLES        0x40000U
#define AUDIOOUT_RING_MASK           (AUDIOOUT_RING_SAMPLES - 1U)
#define AUDIOOUT_MIX_FRAMES          64U
#define AUDIOOUT_MIX_SAMPLES         (AUDIOOUT_MIX_FRAMES * 2U)
#define AUDIOOUT_MIX_BYTES           (AUDIOOUT_MIX_SAMPLES * sizeof(int16_t))
#define AUDIOOUT_QUEUE_LEN           8U
#define AUDIOOUT_QUEUE_MASK          (AUDIOOUT_QUEUE_LEN - 1U)
#define AUDIOOUT_SAMPLE_RATE         44100

#define AUDIOOUT_EVF_CH0_EVEN_READY  0x00000001U
#define AUDIOOUT_EVF_CH0_ODD_READY   0x00000002U
#define AUDIOOUT_EVF_CH1_EVEN_READY  0x00000010U
#define AUDIOOUT_EVF_CH1_ODD_READY   0x00000020U
#define AUDIOOUT_EVF_CH0_EVEN_FREE   0x00010000U
#define AUDIOOUT_EVF_CH0_ODD_FREE    0x00020000U
#define AUDIOOUT_EVF_CH1_EVEN_FREE   0x00100000U
#define AUDIOOUT_EVF_CH1_ODD_FREE    0x00200000U

#define AUDIOOUT_EVF_WAIT_AND_CLEAR  4U
#define AUDIOOUT_EVF_WAIT_OR_CLEAR   5U

#define AUDIOOUT_ERROR_WAIT_TIMEOUT  ((int)0x800280E3)
#define AUDIOOUT_ERROR_PORT_BUSY     ((int)0x80027201)

#define AUDIOOUT_CMD_CH0_OUTPUT      991
#define AUDIOOUT_CMD_GET_LOW_LATENCY 992
#define AUDIOOUT_CMD_DISABLE_OUTPUT  993
#define AUDIOOUT_CMD_ENABLE_OUTPUT   994
#define AUDIOOUT_CMD_NOP             995
#define AUDIOOUT_CMD_CH1_OUTPUT      996

typedef struct PspemuAudioChZeroReq {
	SceUInt64 timestamp;
	void     *buf;
	uint32_t  pad;
} PspemuAudioChZeroReq;

typedef struct PspemuAudioChOneReq {
	SceUInt64 timestamp;
	int16_t  *buf;
	uint32_t  sampleCount;
	uint32_t  volume;
	int       sampleRate;
} PspemuAudioChOneReq;

/* Initialized fade-control globals shared with me_wrapper_api.c */
int g_audioFadeStep = -1;
int g_audioFadeShift = -1;
int g_audioFadeDir = -1;
int g_audioFadeHoldCount = -1;

/* BSS state */
SceUInt64            g_audioUnmuteDeadline;
int                  g_ch0QueueHead;
int16_t              g_ch1RingBuf[AUDIOOUT_RING_SAMPLES];
uint32_t             g_audioPeriodUs;
SceUID               g_ch1DmaSemaId;
uint32_t             g_ch0ReadIdx;
SceUID               g_ch0SemaId;
PspemuAudioChOneReq  g_ch1Queue[AUDIOOUT_QUEUE_LEN];
SceUID               g_ch1SemaId;
int                  g_audioLowLatencyFlag;
SceKernelLwMutexWork g_audioOutMutex;
int                  g_audioMuteState;
PspemuAudioResampler g_ch1Resampler;
int16_t              g_audioSilenceBuf[AUDIOOUT_MIX_SAMPLES];
int16_t              g_audioMixBuf[2][AUDIOOUT_MIX_SAMPLES];
uint64_t             g_audioPeriod512Us;
int                  g_ch1LastProducedFrames;
uint32_t             g_ch1WriteIdx;
int16_t              g_ch0RingBuf[AUDIOOUT_RING_SAMPLES];
uint32_t             g_audioOutputCalls;
uint32_t             g_ch1ReadIdx;
int                  g_audioOutEnabled;
PspemuAudioChZeroReq g_ch0Queue[AUDIOOUT_QUEUE_LEN];
int                  g_audioOutPort;
uint32_t             g_ch0WriteIdx;
SceKernelLwMutexWork g_audioOutputMutex;
int                  g_ch1QueueHead;
volatile int         g_audioMixerReady;
SceUID               g_audioOutEvfId;
int                  g_audioFadeEnabled;

/* 256-phase 4-tap polyphase FIR filter table (Q15 coefficients) */
static const int16_t g_resampleFilter[256][4] = {
	{ -179, 1285, 25828, 1157 }, { -183, 1350, 25829, 1095 }, { -187, 1416, 25829, 1033 }, { -191, 1482, 25827, 973 },
	{ -195, 1551, 25822, 913 }, { -199, 1619, 25816, 855 }, { -203, 1690, 25807, 797 }, { -207, 1759, 25799, 740 },
	{ -211, 1831, 25788, 683 }, { -216, 1904, 25775, 628 }, { -220, 1978, 25759, 574 }, { -224, 2051, 25744, 520 },
	{ -229, 2127, 25726, 467 }, { -233, 2203, 25706, 415 }, { -237, 2279, 25685, 364 }, { -242, 2358, 25661, 314 },
	{ -248, 2437, 25637, 265 }, { -251, 2516, 25609, 217 }, { -256, 2596, 25582, 169 }, { -261, 2678, 25552, 122 },
	{ -266, 2760, 25521, 76 }, { -270, 2843, 25487, 31 }, { -275, 2926, 25453, -13 }, { -280, 3011, 25416, -56 },
	{ -285, 3098, 25378, -100 }, { -291, 3184, 25339, -141 }, { -296, 3272, 25296, -181 }, { -301, 3360, 25255, -223 },
	{ -307, 3449, 25210, -261 }, { -312, 3539, 25163, -299 }, { -318, 3630, 25117, -338 }, { -323, 3721, 25067, -374 },
	{ -329, 3814, 25017, -411 }, { -335, 3907, 24965, -446 }, { -342, 4001, 24911, -479 }, { -348, 4096, 24856, -513 },
	{ -353, 4191, 24799, -546 }, { -360, 4288, 24741, -578 }, { -365, 4385, 24680, -609 }, { -371, 4483, 24620, -641 },
	{ -378, 4582, 24557, -670 }, { -384, 4681, 24493, -699 }, { -391, 4781, 24428, -727 }, { -397, 4883, 24360, -755 },
	{ -404, 4984, 24293, -782 }, { -411, 5087, 24223, -808 }, { -418, 5190, 24152, -833 }, { -425, 5294, 24080, -858 },
	{ -432, 5399, 24006, -882 }, { -439, 5504, 23931, -905 }, { -446, 5611, 23854, -928 }, { -455, 5718, 23777, -949 },
	{ -461, 5826, 23698, -972 }, { -469, 5933, 23618, -991 }, { -476, 6042, 23536, -1011 }, { -484, 6153, 23452, -1030 },
	{ -492, 6263, 23369, -1049 }, { -500, 6374, 23284, -1067 }, { -509, 6486, 23198, -1084 }, { -516, 6598, 23110, -1101 },
	{ -524, 6711, 23021, -1117 }, { -532, 6825, 22930, -1132 }, { -541, 6939, 22840, -1147 }, { -549, 7053, 22748, -1161 },
	{ -557, 7168, 22655, -1175 }, { -566, 7285, 22560, -1188 }, { -576, 7402, 22465, -1200 }, { -583, 7518, 22368, -1212 },
	{ -592, 7637, 22269, -1223 }, { -601, 7754, 22172, -1234 }, { -610, 7873, 22072, -1244 }, { -619, 7994, 21970, -1254 },
	{ -628, 8113, 21869, -1263 }, { -638, 8234, 21767, -1272 }, { -647, 8355, 21663, -1280 }, { -656, 8475, 21559, -1287 },
	{ -666, 8599, 21453, -1295 }, { -675, 8721, 21346, -1301 }, { -686, 8844, 21240, -1307 }, { -694, 8967, 21131, -1313 },
	{ -704, 9091, 21022, -1318 }, { -713, 9215, 20912, -1323 }, { -723, 9340, 20802, -1328 }, { -733, 9465, 20690, -1331 },
	{ -743, 9590, 20578, -1334 }, { -753, 9717, 20465, -1338 }, { -763, 9842, 20351, -1339 }, { -773, 9970, 20235, -1341 },
	{ -783, 10096, 20121, -1343 }, { -793, 10223, 20005, -1344 }, { -803, 10351, 19888, -1345 }, { -813, 10479, 19771, -1346 },
	{ -824, 10609, 19652, -1346 }, { -834, 10737, 19534, -1346 }, { -844, 10866, 19414, -1345 }, { -854, 10996, 19293, -1344 },
	{ -864, 11125, 19173, -1343 }, { -875, 11255, 19052, -1341 }, { -885, 11385, 18930, -1339 }, { -895, 11516, 18808, -1338 },
	{ -906, 11646, 18685, -1334 }, { -916, 11776, 18562, -1331 }, { -926, 11907, 18438, -1328 }, { -937, 12039, 18313, -1324 },
	{ -947, 12171, 18188, -1321 }, { -957, 12303, 18063, -1318 }, { -967, 12433, 17937, -1312 }, { -977, 12565, 17810, -1307 },
	{ -988, 12698, 17684, -1303 }, { -999, 12830, 17557, -1297 }, { -1008, 12963, 17428, -1292 }, { -1018, 13095, 17300, -1286 },
	{ -1028, 13227, 17173, -1281 }, { -1038, 13360, 17043, -1274 }, { -1048, 13492, 16915, -1268 }, { -1058, 13625, 16786, -1262 },
	{ -1068, 13758, 16656, -1255 }, { -1077, 13890, 16526, -1248 }, { -1087, 14023, 16396, -1241 }, { -1096, 14155, 16266, -1234 },
	{ -1106, 14288, 16135, -1226 }, { -1115, 14421, 16003, -1218 }, { -1124, 14553, 15873, -1211 }, { -1134, 14686, 15742, -1203 },
	{ -1143, 14818, 15610, -1194 }, { -1153, 14951, 15479, -1186 }, { -1160, 15083, 15347, -1179 }, { -1169, 15214, 15215, -1169 },
	{ -1179, 15347, 15083, -1160 }, { -1186, 15479, 14951, -1153 }, { -1194, 15610, 14818, -1143 }, { -1203, 15742, 14686, -1134 },
	{ -1211, 15873, 14553, -1124 }, { -1218, 16003, 14421, -1115 }, { -1226, 16135, 14288, -1106 }, { -1234, 16266, 14155, -1096 },
	{ -1241, 16396, 14023, -1087 }, { -1248, 16526, 13890, -1077 }, { -1255, 16656, 13758, -1068 }, { -1262, 16786, 13625, -1058 },
	{ -1268, 16915, 13492, -1048 }, { -1274, 17043, 13360, -1038 }, { -1281, 17173, 13227, -1028 }, { -1286, 17300, 13095, -1018 },
	{ -1292, 17428, 12963, -1008 }, { -1297, 17557, 12830, -999 }, { -1303, 17684, 12698, -988 }, { -1307, 17810, 12565, -977 },
	{ -1312, 17937, 12433, -967 }, { -1318, 18063, 12303, -957 }, { -1321, 18188, 12171, -947 }, { -1324, 18313, 12039, -937 },
	{ -1328, 18438, 11907, -926 }, { -1331, 18562, 11776, -916 }, { -1334, 18685, 11646, -906 }, { -1338, 18808, 11516, -895 },
	{ -1339, 18930, 11385, -885 }, { -1341, 19052, 11255, -875 }, { -1343, 19173, 11125, -864 }, { -1344, 19293, 10996, -854 },
	{ -1345, 19414, 10866, -844 }, { -1346, 19534, 10737, -834 }, { -1346, 19652, 10609, -824 }, { -1346, 19771, 10479, -813 },
	{ -1345, 19887, 10352, -803 }, { -1344, 20005, 10223, -793 }, { -1344, 20121, 10097, -783 }, { -1341, 20235, 9970, -773 },
	{ -1339, 20351, 9842, -763 }, { -1338, 20465, 9717, -753 }, { -1334, 20577, 9591, -743 }, { -1331, 20690, 9465, -733 },
	{ -1327, 20802, 9339, -723 }, { -1323, 20912, 9215, -713 }, { -1318, 21022, 9091, -704 }, { -1313, 21131, 8967, -694 },
	{ -1307, 21240, 8844, -686 }, { -1301, 21346, 8721, -675 }, { -1295, 21453, 8599, -666 }, { -1287, 21559, 8475, -656 },
	{ -1280, 21663, 8355, -647 }, { -1272, 21767, 8234, -638 }, { -1263, 21869, 8113, -628 }, { -1254, 21970, 7994, -619 },
	{ -1244, 22072, 7873, -610 }, { -1234, 22172, 7754, -601 }, { -1223, 22269, 7637, -592 }, { -1212, 22368, 7518, -583 },
	{ -1200, 22465, 7402, -576 }, { -1188, 22560, 7285, -566 }, { -1176, 22655, 7169, -557 }, { -1161, 22747, 7054, -549 },
	{ -1147, 22840, 6939, -541 }, { -1132, 22930, 6825, -532 }, { -1117, 23021, 6711, -524 }, { -1101, 23110, 6598, -516 },
	{ -1084, 23198, 6486, -509 }, { -1067, 23284, 6374, -500 }, { -1049, 23369, 6263, -492 }, { -1030, 23452, 6153, -484 },
	{ -1011, 23536, 6042, -476 }, { -991, 23618, 5933, -469 }, { -972, 23698, 5826, -461 }, { -949, 23777, 5718, -455 },
	{ -928, 23854, 5611, -446 }, { -905, 23930, 5505, -439 }, { -882, 24006, 5399, -432 }, { -858, 24080, 5294, -425 },
	{ -833, 24152, 5190, -418 }, { -808, 24223, 5087, -411 }, { -782, 24293, 4984, -404 }, { -755, 24360, 4883, -397 },
	{ -727, 24428, 4781, -391 }, { -699, 24493, 4681, -384 }, { -670, 24557, 4582, -378 }, { -641, 24620, 4483, -371 },
	{ -609, 24680, 4385, -365 }, { -578, 24741, 4288, -360 }, { -546, 24799, 4191, -353 }, { -513, 24856, 4096, -348 },
	{ -479, 24911, 4001, -342 }, { -446, 24965, 3907, -335 }, { -411, 25017, 3814, -329 }, { -374, 25067, 3721, -323 },
	{ -338, 25117, 3630, -318 }, { -299, 25163, 3539, -312 }, { -261, 25210, 3449, -307 }, { -223, 25255, 3360, -301 },
	{ -181, 25296, 3272, -296 }, { -141, 25339, 3184, -291 }, { -100, 25378, 3098, -285 }, { -56, 25415, 3012, -280 },
	{ -13, 25452, 2927, -275 }, { 31, 25487, 2843, -270 }, { 76, 25521, 2760, -266 }, { 122, 25552, 2678, -261 },
	{ 169, 25582, 2596, -256 }, { 217, 25609, 2516, -251 }, { 265, 25637, 2437, -248 }, { 314, 25661, 2358, -242 },
	{ 364, 25684, 2280, -237 }, { 415, 25706, 2203, -233 }, { 467, 25726, 2127, -229 }, { 520, 25744, 2051, -224 },
	{ 574, 25759, 1978, -220 }, { 628, 25775, 1904, -216 }, { 683, 25788, 1831, -211 }, { 740, 25799, 1759, -207 },
	{ 797, 25807, 1690, -203 }, { 855, 25816, 1619, -199 }, { 913, 25822, 1551, -195 }, { 973, 25827, 1482, -191 },
	{ 1033, 25829, 1416, -187 }, { 1095, 25829, 1350, -183 }, { 1157, 25828, 1285, -179 }, { 1213, 25666, 1213, -1 },
};

static int audioOutChZeroBufferThread(SceSize args, void *argp)
{
	SceUID semaid = g_ch0SemaId;
	unsigned int queueIdx = 0;

	(void)args;
	(void)argp;

	while (sceKernelWaitSema(semaid, 1, NULL) == 0) {
		const void *srcBuf = g_ch0Queue[queueIdx].buf;

		if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
			break;
		uint32_t writeIdx = g_ch0WriteIdx;
		if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
			break;

		memcpy(&g_ch0RingBuf[writeIdx], srcBuf, AUDIOOUT_MIX_BYTES);

		unsigned int waitBits = (queueIdx & 1U) ? AUDIOOUT_EVF_CH0_EVEN_FREE : AUDIOOUT_EVF_CH0_ODD_FREE;
		if (sceKernelWaitEventFlag(g_audioOutEvfId, waitBits, AUDIOOUT_EVF_WAIT_OR_CLEAR, NULL, NULL) < 0)
			break;

		if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
			break;
		g_ch0WriteIdx = (writeIdx + AUDIOOUT_MIX_SAMPLES) & AUDIOOUT_RING_MASK;
		if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
			break;

		unsigned int setBits = (queueIdx & 1U) ? AUDIOOUT_EVF_CH0_ODD_READY : AUDIOOUT_EVF_CH0_EVEN_READY;
		if (sceKernelSetEventFlag(g_audioOutEvfId, setBits) < 0)
			break;

		queueIdx = (queueIdx + 1U) & AUDIOOUT_QUEUE_MASK;
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static int audioOutChZeroIntrThread(SceSize args, void *argp)
{
	SceUID evfId = g_audioOutEvfId;
	unsigned int waitBits = AUDIOOUT_EVF_CH0_EVEN_READY;
	unsigned int pingPong = 0;
	unsigned int queueIdx = 0;
	uint16_t tickCount = 0;
	SceUInt64 targetTime = 0;
	SceUInt64 baseTime = 0;
	SceUInt64 anchorTime = 0;
	unsigned int outBits;

	(void)args;
	(void)argp;

	for (;;) {
		int ret = sceKernelWaitEventFlag(evfId, waitBits, AUDIOOUT_EVF_WAIT_AND_CLEAR, &outBits, NULL);
		if (ret != AUDIOOUT_ERROR_WAIT_TIMEOUT && ret < 0)
			break;

		if (ret == 0 && (outBits & waitBits) != 0) {
			SceUInt64 reqTime = g_ch0Queue[queueIdx].timestamp;

			if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
				break;
			uint64_t periodUs = (uint64_t)g_audioPeriodUs;
			uint64_t period512Us = g_audioPeriod512Us;
			if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
				break;

			if (targetTime >= reqTime || periodUs >= reqTime - targetTime) {
				tickCount++;
				if ((tickCount & 0x1FFU) != 0) {
					targetTime += periodUs;
				} else {
					if (anchorTime == 0)
						break;
					tickCount &= 0x1FFU;
					baseTime += period512Us;
					targetTime = baseTime;
				}
			} else {
				tickCount = 0;
				baseTime = reqTime;
				anchorTime = reqTime;
				targetTime = reqTime;
			}

			queueIdx = (queueIdx + 1U) & AUDIOOUT_QUEUE_MASK;

			while (sceKernelGetProcessTimeWide() < targetTime)
				sceKernelDelayThread(10U);

			int intrRet;
			do {
				intrRet = sceCompatInterrupt(1);
				if (intrRet == PSPEMU_ERROR_NOT_SUPPORTED)
					sceKernelDelayThread(10000U);
			} while (intrRet == PSPEMU_ERROR_NOT_SUPPORTED);

			if (intrRet != 0)
				break;

			unsigned int freeBits = pingPong ? AUDIOOUT_EVF_CH0_ODD_FREE : AUDIOOUT_EVF_CH0_EVEN_FREE;
			if (sceKernelSetEventFlag(g_audioOutEvfId, freeBits) < 0)
				break;

			pingPong ^= 1U;
		}

		waitBits = pingPong ? AUDIOOUT_EVF_CH0_ODD_READY : AUDIOOUT_EVF_CH0_EVEN_READY;
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static int audioOutChOneIntrThread(SceSize args, void *argp)
{
	SceUID evfId = g_audioOutEvfId;
	unsigned int waitBits = AUDIOOUT_EVF_CH1_EVEN_READY;
	unsigned int pingPong = 0;
	unsigned int queueIdx = 0;
	unsigned int tickCount = 0;
	int prevSampleRate = 0;
	uint64_t prevDuration = 0;
	uint64_t accumSamples = 0;
	SceUInt64 prevTargetTime = 0;
	SceUInt64 baseTime = 0;
	SceUInt64 anchorTime = 0;
	unsigned int outBits;

	(void)args;
	(void)argp;

	for (;;) {
		int ret = sceKernelWaitEventFlag(evfId, waitBits, AUDIOOUT_EVF_WAIT_AND_CLEAR, &outBits, NULL);
		if (ret != AUDIOOUT_ERROR_WAIT_TIMEOUT && ret < 0)
			break;

		if (ret == 0 && (outBits & waitBits) != 0) {
			PspemuAudioChOneReq *req = &g_ch1Queue[queueIdx];
			int sampleRate = req->sampleRate;
			SceUInt64 reqTime = req->timestamp;
			uint32_t sampleCount = req->sampleCount;

			if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
				break;
			uint32_t periodUs = g_audioPeriodUs;
			uint64_t period512Us = g_audioPeriod512Us;
			if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
				break;

			int64_t denom = (int64_t)(uint32_t)(sampleRate << 6);
			int64_t frameDuration = ((int64_t)((uint64_t)periodUs * (uint64_t)sampleCount) * AUDIOOUT_SAMPLE_RATE) / denom;
			SceUInt64 targetTime;

			if (prevTargetTime >= reqTime || prevDuration >= reqTime - prevTargetTime) {
				if (sampleRate == prevSampleRate) {
					accumSamples += (uint64_t)sampleCount;
					tickCount++;
					if ((tickCount & 0x1FFU) != 0) {
						targetTime = prevTargetTime + (uint64_t)frameDuration;
					} else {
						if (anchorTime == 0)
							break;
						tickCount = 0;
						int64_t scaled = (int64_t)(period512Us * (uint64_t)AUDIOOUT_SAMPLE_RATE * accumSamples);
						accumSamples = 0;
						baseTime += (uint64_t)(scaled / denom) >> 9;
						targetTime = baseTime;
					}
				} else {
					baseTime = reqTime;
					anchorTime = reqTime;
					accumSamples = 0;
					tickCount = 0;
					targetTime = reqTime;
				}
			} else {
				baseTime = reqTime;
				tickCount = 0;
				anchorTime = reqTime;
				accumSamples = 0;
				targetTime = reqTime;
			}

			queueIdx = (queueIdx + 1U) & AUDIOOUT_QUEUE_MASK;

			while (sceKernelGetProcessTimeWide() < targetTime)
				sceKernelDelayThread(10U);

			int intrRet;
			do {
				intrRet = sceCompatInterrupt(2);
				if (intrRet == PSPEMU_ERROR_NOT_SUPPORTED)
					sceKernelDelayThread(10000U);
			} while (intrRet == PSPEMU_ERROR_NOT_SUPPORTED);

			if (intrRet != 0)
				break;

			unsigned int freeBits = pingPong ? AUDIOOUT_EVF_CH1_ODD_FREE : AUDIOOUT_EVF_CH1_EVEN_FREE;
			if (sceKernelSetEventFlag(g_audioOutEvfId, freeBits) < 0)
				break;

			pingPong ^= 1U;
			prevDuration = (uint64_t)frameDuration;
			prevTargetTime = targetTime;
			prevSampleRate = sampleRate;
		}

		waitBits = pingPong ? AUDIOOUT_EVF_CH1_ODD_READY : AUDIOOUT_EVF_CH1_EVEN_READY;
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static int audioOutChOneBufferThread(SceSize args, void *argp)
{
	SceUID semaid = g_ch1SemaId;
	unsigned int queueIdx = 0;

	(void)args;
	(void)argp;

	while (sceKernelWaitSema(semaid, 1, NULL) == 0) {
		PspemuAudioChOneReq *req = &g_ch1Queue[queueIdx];
		int sampleRate = req->sampleRate;

		if (sampleRate != g_ch1Resampler.srcRate)
			audioResamplerInit(&g_ch1Resampler, sampleRate, AUDIOOUT_SAMPLE_RATE);

		const int16_t *src = req->buf;
		int remaining = (int)req->sampleCount;
		unsigned int volume = req->volume;

		if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
			break;
		uint32_t writeIdx = g_ch1WriteIdx;
		if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
			break;

		int produced;
		if (sampleRate == AUDIOOUT_SAMPLE_RATE) {
			int totalSamples = remaining * 2;
			if (totalSamples > 0) {
				uint32_t ringPos = writeIdx;
				for (int i = 0; i < totalSamples; i++) {
					int32_t scaled = (int32_t)(((int64_t)(int)volume * (int64_t)src[i]) / 0x8000);
					if (scaled > 32767)
						scaled = 32767;
					if (scaled < -32768)
						scaled = -32768;
					g_ch1RingBuf[ringPos & AUDIOOUT_RING_MASK] = (int16_t)scaled;
					ringPos++;
				}
			}
			produced = remaining;
		} else {
			produced = 0;
			int more;
			do {
				uint32_t availFrames = (AUDIOOUT_RING_SAMPLES - writeIdx) >> 1;
				if (availFrames == 0) {
					availFrames = AUDIOOUT_RING_SAMPLES >> 1;
					writeIdx = 0;
				}
				more = audioResamplerProcess(
					&g_ch1Resampler,
					src,
					remaining,
					&g_ch1RingBuf[writeIdx],
					(int)availFrames,
					volume);
				remaining -= g_ch1Resampler.consumedFrames;
				produced += g_ch1Resampler.producedFrames;
				src += 2 * g_ch1Resampler.consumedFrames;
				writeIdx += 2U * (uint32_t)g_ch1Resampler.producedFrames;
			} while (more != 0);
		}

		unsigned int waitBits = (queueIdx & 1U) ? AUDIOOUT_EVF_CH1_EVEN_FREE : AUDIOOUT_EVF_CH1_ODD_FREE;
		if (sceKernelWaitEventFlag(g_audioOutEvfId, waitBits, AUDIOOUT_EVF_WAIT_OR_CLEAR, NULL, NULL) < 0)
			break;

		if (sampleRate == AUDIOOUT_SAMPLE_RATE) {
			if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
				break;
			g_ch1WriteIdx = (writeIdx + 2U * (uint32_t)produced) & AUDIOOUT_RING_MASK;
			if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
				break;
		} else {
			if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
				break;
			g_ch1WriteIdx = writeIdx & AUDIOOUT_RING_MASK;
			if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
				break;
		}

		unsigned int setBits = (queueIdx & 1U) ? AUDIOOUT_EVF_CH1_ODD_READY : AUDIOOUT_EVF_CH1_EVEN_READY;
		g_ch1LastProducedFrames = produced;
		if (sceKernelSetEventFlag(g_audioOutEvfId, setBits) < 0)
			break;

		queueIdx = (queueIdx + 1U) & AUDIOOUT_QUEUE_MASK;
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static int audioOutOpThread(SceSize args, void *argp);
static int audioOutMixerThread(SceSize args, void *argp);

int remoteAudioOutInit(void)
{
	int ret;
	SceUID opThid;
	SceUID ch0BufThid;
	SceUID ch1BufThid;
	SceUID ch0IntrThid;
	SceUID ch1IntrThid;
	SceUID mixerThid;

	ret = sceKernelCreateLwMutex(&g_audioOutputMutex, "ScePspemuAudioOutOutput", 0, 0, NULL);
	if (ret != 0)
		return ret;

	ret = sceKernelCreateSema("ScePspemuAudioOutCh0", 0, 0, 2, NULL);
	g_ch0SemaId = ret;
	if (ret <= 0)
		return ret;

	ret = sceKernelCreateSema("ScePspemuAudioOutCh1", 0, 0, 2, NULL);
	g_ch1SemaId = ret;
	if (ret <= 0)
		return ret;

	ret = sceKernelCreateSema("ScePspemuAudioOutCh1DMA", 0, 0, 2, NULL);
	g_ch1DmaSemaId = ret;
	if (ret <= 0)
		return ret;

	opThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioOutOp",
		audioOutOpThread,
		64,
		0x2000U,
		0,
		0x20000,
		NULL);
	if (opThid < 0)
		return opThid;

	ret = sceKernelStartThread(opThid, 0, NULL);
	if (ret != 0)
		return ret;

	ret = sceKernelCreateEventFlag(
		"ScePspemuAudioOut",
		0x1000U,
		AUDIOOUT_EVF_CH0_ODD_FREE | AUDIOOUT_EVF_CH1_ODD_FREE,
		NULL);
	g_audioOutEvfId = ret;
	if (ret <= 0)
		return ret;

	ch0BufThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioCh0Buffer",
		audioOutChZeroBufferThread,
		67,
		0x1000U,
		0,
		0x20000,
		NULL);
	if (ch0BufThid < 0)
		return ch0BufThid;

	ret = sceKernelStartThread(ch0BufThid, 0, NULL);
	if (ret != 0)
		return ret;

	ch1BufThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioCh1Buffer",
		audioOutChOneBufferThread,
		67,
		0x1000U,
		0,
		0x20000,
		NULL);
	if (ch1BufThid < 0)
		return ch1BufThid;

	ret = sceKernelStartThread(ch1BufThid, 0, NULL);
	if (ret != 0)
		return ret;

	ch0IntrThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioOutCh0Intr",
		audioOutChZeroIntrThread,
		65,
		0x1000U,
		0,
		0x20000,
		NULL);
	if (ch0IntrThid < 0)
		return ch0IntrThid;

	ch1IntrThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioOutCh1Intr",
		audioOutChOneIntrThread,
		65,
		0x1000U,
		0,
		0x20000,
		NULL);
	if (ch1IntrThid < 0)
		return ch1IntrThid;

	mixerThid = sceKernelCreateThread(
		"ScePspemuRemoteAudioOutMixer",
		audioOutMixerThread,
		65,
		0x1000U,
		0,
		0x20000,
		NULL);
	if (mixerThid < 0)
		return mixerThid;

	int compatFlags = pspemuCompatGetParam(12);
	if (compatFlags > 0) {
		if ((compatFlags & 0x80) != 0) {
			g_audioFadeDir = 0;
			g_audioFadeShift = 0;
			g_audioFadeHoldCount = 3;
			g_audioFadeStep = 1;
			g_audioFadeEnabled = 1;
		}
		if ((compatFlags & 0x1000) != 0)
			g_audioLowLatencyFlag = 1;
	}

	ret = sceKernelCreateLwMutex(&g_audioOutMutex, "ScePspemuAudioOut", 0, 0, NULL);
	if (ret != 0)
		return ret;

	int port = sceAudioOutOpenPort(
		SCE_AUDIO_OUT_PORT_TYPE_BGM,
		AUDIOOUT_MIX_FRAMES,
		AUDIOOUT_SAMPLE_RATE,
		SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	g_audioOutPort = port;
	if (port <= 0)
		return (port == 0) ? 0 : port;

	int vol[2] = { 0x8000, 0x8000 };
	ret = sceAudioOutSetVolume(
		port,
		SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
		vol);
	if (ret != 0)
		return ret;

	ret = sceKernelStartThread(mixerThid, 0, NULL);
	if (ret != 0)
		return ret;

	while (g_audioMixerReady == 0)
		sceKernelDelayThread(100U);

	ret = sceKernelStartThread(ch0IntrThid, 0, NULL);
	if (ret != 0)
		return ret;

	ret = sceKernelStartThread(ch1IntrThid, 0, NULL);
	if (ret != 0)
		return ret;

	return 0;
}

int remoteAudioOutSuspend(int mode)
{
	int vol[2];
	int ret;

	if (mode == 1) {
		vol[0] = 0;
		vol[1] = 0;
		ret = sceAudioOutSetVolume(
			g_audioOutPort,
			SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
			vol);
		if (ret == 0)
			g_audioMuteState = 1;
		return ret;
	}

	if (mode == 2) {
		g_audioMuteState = 2;
		g_audioUnmuteDeadline = sceKernelGetProcessTimeWide() + 1000000ULL;
		return 0;
	}

	vol[0] = 0x8000;
	vol[1] = 0x8000;
	ret = sceAudioOutSetVolume(
		g_audioOutPort,
		SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
		vol);
	if (ret == 0)
		g_audioMuteState = 0;
	return ret;
}

static int audioOutOpThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;

	(void)args;
	(void)argp;

	audioResamplerInit(&g_ch1Resampler, AUDIOOUT_SAMPLE_RATE, AUDIOOUT_SAMPLE_RATE);

	for (;;) {
		int64_t retVal;
		int cmd = remotefsWaitAndGetRequest(AUDIOOUT_CHANNEL_ID, &req);

		switch (cmd) {
		case AUDIOOUT_CMD_CH0_OUTPUT: {
			void *hostBuf = remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, AUDIOOUT_MIX_BYTES);
			int idx = g_ch0QueueHead;
			g_ch0Queue[idx].timestamp = sceKernelGetProcessTimeWide();
			g_ch0Queue[g_ch0QueueHead].buf = hostBuf;
			int sigRet = sceKernelSignalSema(g_ch0SemaId, 1);
			if (sigRet == 0)
				g_ch0QueueHead = (g_ch0QueueHead + 1) & (int)AUDIOOUT_QUEUE_MASK;
			retVal = (int64_t)(uint32_t)sigRet;
			break;
		}
		case AUDIOOUT_CMD_GET_LOW_LATENCY:
			retVal = (int64_t)(uint32_t)g_audioLowLatencyFlag;
			break;
		case AUDIOOUT_CMD_DISABLE_OUTPUT:
			if (sceKernelLockLwMutex(&g_audioOutMutex, 1, NULL) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			g_audioOutEnabled = 0;
			if (sceKernelUnlockLwMutex(&g_audioOutMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			remoteAudioOutSuspend(2);
			retVal = 0;
			break;
		case AUDIOOUT_CMD_ENABLE_OUTPUT:
			if (sceKernelLockLwMutex(&g_audioOutMutex, 1, NULL) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			g_audioOutEnabled = 1;
			if (sceKernelUnlockLwMutex(&g_audioOutMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			retVal = 0;
			break;
		case AUDIOOUT_CMD_NOP:
			retVal = 0;
			break;
		case AUDIOOUT_CMD_CH1_OUTPUT: {
			uint32_t sampleCount = (uint32_t)req->args[1];
			int16_t *hostBuf = (int16_t *)remotefsTranslateGuestAddr((uint32_t)req->args[0], 1, 4U * sampleCount);
			int idx = g_ch1QueueHead;
			uint32_t volume = (uint32_t)req->args[2];
			int sampleRate = (int)req->args[3];
			g_ch1Queue[idx].timestamp = sceKernelGetProcessTimeWide();
			g_ch1Queue[g_ch1QueueHead].buf = hostBuf;
			g_ch1Queue[g_ch1QueueHead].sampleCount = sampleCount;
			g_ch1Queue[g_ch1QueueHead].volume = volume;
			g_ch1Queue[g_ch1QueueHead].sampleRate = sampleRate;
			int sigRet = sceKernelSignalSema(g_ch1SemaId, 1);
			if (sigRet == 0)
				g_ch1QueueHead = (g_ch1QueueHead + 1) & (int)AUDIOOUT_QUEUE_MASK;
			retVal = (int64_t)(uint32_t)sigRet;
			break;
		}
		default:
			pspemuRecordTitleListFlags(16U);
			retVal = -1;
			break;
		}

		remotefsReturnValue(AUDIOOUT_CHANNEL_ID, req, retVal);
	}
}

static inline void audioOutEmitBlock(const void *buf)
{
	if (g_audioMuteState == 2 && g_audioUnmuteDeadline < sceKernelGetProcessTimeWide())
		remoteAudioOutSuspend(0);
	do {
		g_audioOutputCalls++;
	} while (sceAudioOutOutput(g_audioOutPort, buf) == AUDIOOUT_ERROR_PORT_BUSY);
}

static int audioOutMixerThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	audioOutEmitBlock(g_audioSilenceBuf);

	SceUInt64 calibStart = sceKernelGetProcessTimeWide();
	for (int i = 0; i < 128; i++)
		audioOutEmitBlock(g_audioSilenceBuf);
	SceUInt64 windowStart = sceKernelGetProcessTimeWide();

	if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	g_audioPeriodUs = (uint32_t)((int64_t)(windowStart - calibStart) >> 7);
	g_audioPeriod512Us = ((windowStart - calibStart) << 9) >> 7;
	if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_audioMixerReady = 1;

	uint32_t prevCh0Read = 0;
	uint32_t prevCh1Read = 0;
	int activeMask = 0;
	int blockCounter = 0;
	unsigned int mixBufSel = 0;
	uint32_t ch1Frames = 0;

	for (;;) {
		if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		uint32_t ch0Write = g_ch0WriteIdx;
		uint32_t ch0Read = g_ch0ReadIdx;
		uint32_t ch1Read = g_ch1ReadIdx;
		uint32_t ch1Write = g_ch1WriteIdx;
		if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		int ch1HasData = (ch1Read != ch1Write);
		if (ch1HasData) {
			uint32_t availSamples = (ch1Read < ch1Write)
				? (ch1Write - ch1Read)
				: (ch1Write + AUDIOOUT_RING_SAMPLES - ch1Read);
			uint32_t frames = availSamples >> 1;
			if (frames >= AUDIOOUT_MIX_FRAMES)
				frames = AUDIOOUT_MIX_FRAMES;
			ch1Frames = frames;
		}

		if (activeMask == 0) {
			audioOutEmitBlock(g_audioSilenceBuf);
			blockCounter++;
		} else {
			int16_t *outBuf = g_audioMixBuf[mixBufSel];

			if (activeMask == 2) {
				const int16_t *ch1Ptr = &g_ch1RingBuf[prevCh1Read];
				const int16_t *ch1End = &g_ch1RingBuf[AUDIOOUT_RING_SAMPLES - 1U];
				for (uint32_t i = 0; i < ch1Frames; i++) {
					outBuf[2 * i] = ch1Ptr[0];
					outBuf[2 * i + 1] = ch1Ptr[1];
					ch1Ptr += 2;
					if (ch1Ptr > ch1End)
						ch1Ptr = g_ch1RingBuf;
				}
				memset(&outBuf[2 * ch1Frames], 0, AUDIOOUT_MIX_BYTES - 4U * ch1Frames);
			} else if (activeMask == 3) {
				const int16_t *ch0Ptr = &g_ch0RingBuf[prevCh0Read];
				const int16_t *ch1Ptr = &g_ch1RingBuf[prevCh1Read];
				const int16_t *ch1End = &g_ch1RingBuf[AUDIOOUT_RING_SAMPLES - 1U];

				for (uint32_t i = 0; i < ch1Frames; i++) {
					int32_t left = (int32_t)ch1Ptr[0] + (int32_t)ch0Ptr[2 * i];
					int32_t right = (int32_t)ch1Ptr[1] + (int32_t)ch0Ptr[2 * i + 1];
					ch1Ptr += 2;
					if (ch1Ptr > ch1End)
						ch1Ptr = g_ch1RingBuf;
					if (left >= 32767)
						left = 32767;
					if (right >= 32767)
						right = 32767;
					if (left < -32768)
						left = -32768;
					if (right < -32768)
						right = -32768;
					outBuf[2 * i] = (int16_t)left;
					outBuf[2 * i + 1] = (int16_t)right;
				}
				for (uint32_t i = ch1Frames; i < AUDIOOUT_MIX_FRAMES; i++) {
					outBuf[2 * i] = ch0Ptr[2 * i];
					outBuf[2 * i + 1] = ch0Ptr[2 * i + 1];
				}
			} else {
				/* activeMask == 1: channel 0 only (with optional fade envelope) */
				if (g_audioFadeEnabled != 0 && g_audioFadeDir == -1) {
					if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
					int shift = g_audioFadeShift;
					for (int i = 0; i < (int)AUDIOOUT_MIX_SAMPLES; i++)
						outBuf[i] = (int16_t)(g_ch0RingBuf[prevCh0Read + (uint32_t)i] >> shift);
					if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

					g_audioFadeShift += g_audioFadeStep;
					if (g_audioFadeShift > 16) {
						g_audioFadeDir = 1;
						g_audioFadeShift = 16;
					}
				} else if (g_audioFadeEnabled != 0 && g_audioFadeDir == 1) {
					if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
					int shift = g_audioFadeShift;
					for (int i = 0; i < (int)AUDIOOUT_MIX_SAMPLES; i++)
						outBuf[i] = (int16_t)(g_ch0RingBuf[prevCh0Read + (uint32_t)i] >> shift);
					if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
						pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

					if (g_audioFadeHoldCount <= 0) {
						g_audioFadeHoldCount = 3;
						g_audioFadeShift -= g_audioFadeStep;
					} else {
						g_audioFadeHoldCount--;
					}
					if (g_audioFadeShift < 0) {
						g_audioFadeDir = 0;
						g_audioFadeShift = 0;
					}
				} else {
					memcpy(outBuf, &g_ch0RingBuf[prevCh0Read], AUDIOOUT_MIX_BYTES);
				}
			}

			const void *emitPtr = (g_audioMuteState == 1) ? (const void *)g_audioSilenceBuf : (const void *)outBuf;
			audioOutEmitBlock(emitPtr);
			mixBufSel ^= 1U;
			blockCounter++;
		}

		if (blockCounter == 512) {
			SceUInt64 now = sceKernelGetProcessTimeWide();
			uint64_t elapsed = now - windowStart;
			uint32_t avgPeriod = (uint32_t)((int64_t)elapsed >> 9);
			uint32_t diff = (avgPeriod > 1451U) ? (avgPeriod - 1451U) : (1451U - avgPeriod);

			if (diff <= 43U && avgPeriod != 0) {
				if (sceKernelLockLwMutex(&g_audioOutputMutex, 1, NULL) < 0)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				g_audioPeriod512Us = elapsed;
				g_audioPeriodUs = avgPeriod;
				if (sceKernelUnlockLwMutex(&g_audioOutputMutex, 1) < 0)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			}
			windowStart = now;
			blockCounter = 0;
		}

		if (ch0Read != ch0Write)
			g_ch0ReadIdx = (g_ch0ReadIdx + AUDIOOUT_MIX_SAMPLES) & AUDIOOUT_RING_MASK;
		if (ch1HasData)
			g_ch1ReadIdx = (g_ch1ReadIdx + 2U * ch1Frames) & AUDIOOUT_RING_MASK;

		prevCh1Read = ch1Read;
		prevCh0Read = ch0Read;
		activeMask = (ch0Read != ch0Write ? 1 : 0) | (ch1HasData ? 2 : 0);
	}
}

uint32_t __attribute__((noinline, noclone)) audioResamplerInit(PspemuAudioResampler *resampler, int srcRate, int dstRate)
{
	uint32_t step;

	resampler->srcRate = srcRate;
	resampler->dstRate = dstRate;
	resampler->consumedFrames = 0;
	resampler->producedFrames = 0;
	resampler->phaseFrac = 0;

	if (dstRate != 0) {
		step = ((uint32_t)srcRate << 16) / (uint32_t)dstRate;
		resampler->phaseStep = step;
		resampler->ringReadIdx = 0;
		resampler->ringWriteIdx = 0;
		return step;
	}

	resampler->phaseStep = 0;
	resampler->ringReadIdx = 0;
	resampler->ringWriteIdx = 0;
	return 0;
}

int __attribute__((noinline, noclone)) audioResamplerProcess(PspemuAudioResampler *resampler, const int16_t *inSamples, int inFrames,
                          int16_t *outSamples, int maxOutFrames, unsigned int volume)
{
	int readIdx = resampler->ringReadIdx;
	int writeIdx = resampler->ringWriteIdx;
	int consumed = 0;
	int produced = 0;
	int ret;

	if (maxOutFrames <= 0) {
		ret = 1;
	} else {
		ret = 1;
		do {
			if (readIdx + 3 >= writeIdx) {
				if (readIdx > 60) {
					int keep = 64 - readIdx;
					if (keep > 0)
						memcpy(&resampler->ringBuf[0][0], &resampler->ringBuf[readIdx][0], (size_t)keep * 4U);
					writeIdx = keep;
					readIdx = 0;
				}

				int copyFrames = inFrames - consumed;
				if (64 - writeIdx < copyFrames)
					copyFrames = 64 - writeIdx;
				if (copyFrames > 0) {
					memcpy(&resampler->ringBuf[writeIdx][0], &inSamples[2 * consumed], (size_t)copyFrames * 4U);
					writeIdx += copyFrames;
					consumed += copyFrames;
				}

				if (writeIdx <= readIdx + 3) {
					ret = 0;
					break;
				}
			}

			uint32_t phase = resampler->phaseFrac;
			const int16_t *taps = g_resampleFilter[(phase >> 8) & 0xFFU];
			const int16_t *win = &resampler->ringBuf[readIdx][0];

			uint32_t nextPhase = phase + resampler->phaseStep;
			readIdx += (int)(nextPhase >> 16);
			resampler->phaseFrac = nextPhase & 0xFFFFU;

			int32_t sumLeft = (int32_t)win[0] * (int32_t)taps[3]
			                + (int32_t)win[2] * (int32_t)taps[2]
			                + (int32_t)win[4] * (int32_t)taps[1]
			                + (int32_t)win[6] * (int32_t)taps[0];
			int32_t sumRight = (int32_t)win[1] * (int32_t)taps[3]
			                 + (int32_t)win[3] * (int32_t)taps[2]
			                 + (int32_t)win[5] * (int32_t)taps[1]
			                 + (int32_t)win[7] * (int32_t)taps[0];

			int32_t filtLeft = (13107 * (sumLeft >> 15)) / 0x4000;
			int32_t filtRight = (13107 * (sumRight >> 15)) / 0x4000;

			int32_t outLeft = (int32_t)(((int64_t)filtLeft * (int64_t)volume) / 0x8000);
			int32_t outRight = (int32_t)(((int64_t)filtRight * (int64_t)volume) / 0x8000);

			if (outLeft > 32767)
				outLeft = 32767;
			if (outRight > 32767)
				outRight = 32767;
			if (outLeft < -32768)
				outLeft = -32768;
			if (outRight < -32768)
				outRight = -32768;

			outSamples[0] = (int16_t)outLeft;
			outSamples[1] = (int16_t)outRight;
			outSamples += 2;
			produced++;
		} while (produced != maxOutFrames);
	}

	resampler->ringReadIdx = readIdx;
	resampler->ringWriteIdx = writeIdx;
	resampler->consumedFrames = consumed;
	resampler->producedFrames = produced;
	return ret;
}
