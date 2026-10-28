/* The shared, eight-voice wave engine used by paf::sound::Output. */
/* The wave thread uses ThreadMgr's extended stack/TLS option structure. */
#if defined(__SNC__)
#define PAF_ASSERT_NAME_(line) pafSizeCheck##line
#define PAF_ASSERT_NAME(line) PAF_ASSERT_NAME_(line)
#define _Static_assert(condition, message) \
	typedef char PAF_ASSERT_NAME(__LINE__)[(condition) ? 1 : -1]
#endif
#define PSP2_KERNEL 1
#include <threadmgr/threadmgr.h>
#undef PSP2_KERNEL
#include "wave.h"
#include "sas.h"
#include <audioout.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/memblock.h>
#include <stddef.h>
#if defined(__SNC__)
#undef _Static_assert
#undef PAF_ASSERT_NAME
#undef PAF_ASSERT_NAME_
#endif

enum {
	WAVE_UNINITIALIZED = 0,
	WAVE_INITIALIZED = 1,
	WAVE_STARTED = 3,
	WAVE_ERROR_INIT = -2143027199,       /* 0x80440001 */
	WAVE_ERROR_EXIT = -2143027198,       /* 0x80440002 */
	WAVE_ERROR_START = -2143027197,      /* 0x80440003 */
	WAVE_ERROR_STOP = -2143027196,       /* 0x80440004 */
	WAVE_ERROR_PRIORITY = -2143027182    /* 0x80440012 */
};

struct WaveThreadArgs {
	SceUID threadId;
	int port;
	int priority;
};

static int waveState;
static int waveExitRequested;
static SceUID waveEventFlag = -1;
struct WaveThreadArgs waveThreadArgs;
static SasContext *waveSas;
static unsigned char waveSasBuffer[0x8000] __attribute__((aligned(16)));
static int waveVolume[2];
static int waveExtVolume[2];
static SceInt16 waveOutputBuffer[2][1024 * 2] __attribute__((aligned(16)));

/* Internal audio service import; libaudioout's public header omits this port. */
extern int sceAudioOutOpenExtPort(int len, int param);
int WaveMain(SceSize argSize, void *argp);

int WaveInit(void)
{
	SceSize size;
	int result;

	if (waveState != WAVE_UNINITIALIZED)
		return WAVE_ERROR_INIT;
	waveThreadArgs.port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
			1024, 48000, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (waveThreadArgs.port < 0)
		return WAVE_ERROR_INIT;
	waveVolume[0] = SCE_AUDIO_VOLUME_0DB;
	waveVolume[1] = SCE_AUDIO_VOLUME_0DB;
	sceAudioOutSetVolume(waveThreadArgs.port,
			SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
			waveVolume);
	result = SasGetNeededMemorySize("numVoices=8 numReverbs=0", &size);
	if (result < 0)
		return result;
	result = SasInit("numVoices=8 numReverbs=0", waveSasBuffer, size, &waveSas);
	if (result < 0)
		return result;
	result = SasSetGrain(waveSas, 1024);
	if (result < 0)
		return result;
	waveState = WAVE_INITIALIZED;
	return 0;
}

int WaveInitExt(void)
{
	SceSize size;
	int result;

	if (waveState != WAVE_UNINITIALIZED)
		return WAVE_ERROR_INIT;
	waveThreadArgs.port = sceAudioOutOpenExtPort(1024,
			SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	if (waveThreadArgs.port < 0)
		return WAVE_ERROR_INIT;
	waveExtVolume[0] = SCE_AUDIO_VOLUME_0DB;
	waveExtVolume[1] = SCE_AUDIO_VOLUME_0DB;
	sceAudioOutSetVolume(waveThreadArgs.port,
			SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
			waveExtVolume);
	result = SasGetNeededMemorySize("numVoices=8 numReverbs=0", &size);
	if (result < 0)
		return result;
	result = SasInit("numVoices=8 numReverbs=0", waveSasBuffer, size, &waveSas);
	if (result < 0)
		return result;
	result = SasSetGrain(waveSas, 1024);
	if (result < 0)
		return result;
	waveState = WAVE_INITIALIZED;
	return 0;
}

int WaveStart(int priority, SceUInt attr, int affinity, int memblockType)
{
	SceKernelThreadOptParam option;
	const SceKernelThreadOptParam *threadOption = NULL;
	int result;

	if (waveState != WAVE_INITIALIZED)
		return WAVE_ERROR_START;
	waveEventFlag = sceKernelCreateEventFlag("SceWaveMainEvf",
			SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (waveEventFlag < 0)
		return waveEventFlag;
	waveExitRequested = 0;
	if (memblockType != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW) {
		option.uStackMemType = memblockType;
		option.uTLSMemType = memblockType;
		option.size = sizeof(option);
		option.attr = 6;
		threadOption = &option;
	}
	waveThreadArgs.threadId = sceKernelCreateThread("SceWaveMain", WaveMain,
			priority, SCE_KERNEL_8KiB, attr, affinity, threadOption);
	if (waveThreadArgs.threadId >= 0) {
		waveThreadArgs.priority = priority;
		if (!sceKernelStartThread(waveThreadArgs.threadId,
					  sizeof(waveThreadArgs), &waveThreadArgs)) {
			sceKernelDelayThread(10000);
			waveState = WAVE_STARTED;
			return 0;
		}
		result = WAVE_ERROR_START;
	} else if (waveThreadArgs.threadId == SCE_KERNEL_ERROR_ILLEGAL_PRIORITY) {
		result = WAVE_ERROR_PRIORITY;
	} else {
		result = WAVE_ERROR_START;
	}
	sceKernelDeleteEventFlag(waveEventFlag);
	waveEventFlag = -1;
	return result;
}

int WaveKeyOff(int voice)
{
	int result = SasSetKeyOff(waveSas, voice);

	sceKernelSetEventFlag(waveEventFlag, 1);
	return result;
}

int WaveStop(void)
{
	if (waveState != WAVE_STARTED)
		return WAVE_ERROR_STOP;
	for (int voice = 0; voice < 8; ++voice) {
		SasSetKeyOff(waveSas, voice);
		sceKernelSetEventFlag(waveEventFlag, 1);
	}
	waveExitRequested = 1;
	sceKernelSetEventFlag(waveEventFlag, 1);
	sceKernelWaitThreadEnd(waveThreadArgs.threadId, NULL, NULL);
	sceKernelDeleteThread(waveThreadArgs.threadId);
	waveExitRequested = 0;
	waveState = WAVE_INITIALIZED;
	sceKernelDeleteEventFlag(waveEventFlag);
	waveEventFlag = -1;
	return 0;
}

int WaveExit(void)
{
	void *buffer;
	SceSize size;
	int result;

	if (waveState != WAVE_INITIALIZED)
		return WAVE_ERROR_EXIT;
	sceAudioOutReleasePort(waveThreadArgs.port);
	result = SasExit(waveSas, &buffer, &size);
	waveState = WAVE_UNINITIALIZED;
	return result;
}

int WaveSetVoice(int voice, const char *buffer, unsigned int size,
		 int samplerate, int loop)
{
	SasSetSimpleADSR(waveSas, voice, 0, 0);
	SasSetPitch(waveSas, voice, (int)((unsigned int)samplerate << 12) / 48000);
	if (buffer[0] == 'V' && buffer[1] == 'A' &&
	    buffer[2] == 'G' && buffer[3] == 'p') {
		buffer += 48;
		size -= 48;
	}
	return SasSetVoice(waveSas, voice, buffer, size, loop == 2);
}

int WaveIsPlaying(int voice)
{
	return SasGetEndState(waveSas, voice) == 0;
}

int WaveKeyOn(int voice, int left, int right)
{
	int result;

	SasSetKeyOff(waveSas, voice);
	SasSetVolume(waveSas, voice, (int)((unsigned int)left << 12) / 127,
		     (int)((unsigned int)right << 12) / 127, 0, 0);
	result = SasSetKeyOn(waveSas, voice);
	sceKernelSetEventFlag(waveEventFlag, 1);
	return result;
}

int WaveKeyOffAll(void)
{
	int result;

	for (int voice = 0; voice < 8; ++voice) {
		SasSetKeyOff(waveSas, voice);
		result = sceKernelSetEventFlag(waveEventFlag, 1);
	}
	return result;
}

int WaveMain(SceSize argSize, void *argp)
{
	struct WaveThreadArgs *args = argp;
	int buffer = 0;

	SasCoreWithMix(waveSas, waveOutputBuffer[0], 0, 0);
	while (!waveExitRequested) {
		int voice = 0;
		while (SasGetEndState(waveSas, voice)) {
			if (++voice >= 8) {
				sceKernelWaitEventFlag(waveEventFlag, 1,
					SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
				break;
			}
		}
		SasCoreWithMix(waveSas, waveOutputBuffer[buffer], 0, 0);
		sceAudioOutOutput(args->port, waveOutputBuffer[buffer]);
		buffer ^= 1;
	}
	sceAudioOutOutput(args->port, NULL);
	return 0;
}
