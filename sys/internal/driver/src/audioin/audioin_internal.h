#ifndef AUDIOIN_INTERNAL_H
#define AUDIOIN_INTERNAL_H

#include <stdint.h>
#include <threadmgr/fast_mutex.h>
#include <processmgr/processmgr.h>
#include <codec/codec.h>
#include <vnz_wrapper/vnz_wrapper.h>
#include <audio/audio.h>

/* Complete module-owned port record. Unidentified members remain explicit. */
typedef struct AudioInPort {
	unsigned int unidentifiedPortControl;
	int eventFlag;
	unsigned int flags;
	int pid;
	unsigned int grain;
	void *destPtr;
	unsigned int unidentifiedInputControl;
	unsigned int freq;
	int port;
	unsigned int unidentifiedCaptureControl;
	unsigned int transferredSamples;
	int inputResult;
	int16_t *auxiliaryDestinations[2];
	unsigned short gainControl;
	unsigned short unidentifiedGainControl;
	unsigned int unidentifiedPortTail;
} AudioInPort;

typedef struct AudioInCaptureBuffer {
	unsigned int writeCursor;
	unsigned int readCursor;
	unsigned int capacity;
	unsigned int inputGrain;
	unsigned int outputGrain;
	unsigned int pendingSamples;
	int16_t *samples;
} AudioInCaptureBuffer;

/* The initializer and capture worker address all seven records from one base. */
typedef struct AudioInCaptureBuffers {
	AudioInCaptureBuffer input;
	AudioInCaptureBuffer primary48;
	AudioInCaptureBuffer secondary48;
	AudioInCaptureBuffer primary16;
	AudioInCaptureBuffer secondary16;
	AudioInCaptureBuffer voice8;
	AudioInCaptureBuffer combined16;
} AudioInCaptureBuffers;

typedef struct AudioInAecContext {
	unsigned int size;
	int16_t *sampleBuffers[2];
	unsigned int context[2];
} AudioInAecContext;

typedef struct AudioInSampleView {
	int16_t *samples;
	unsigned int sampleCount;
	unsigned int channels;
} AudioInSampleView;

typedef struct AudioInLegacyQueue {
	int16_t samples[360];
	int16_t writeCursor;
	int16_t readCursor;
	int16_t queuedSamples;
	int16_t targetSamples;
	int16_t previousSample;
} AudioInLegacyQueue;

extern AudioInLegacyQueue g_audioInLegacyQueue;

typedef struct AudioInResamplerState {
	float firstStageHistory[2];
	float secondStageHistory[2];
} AudioInResamplerState;

extern AudioInResamplerState g_audioInResamplerState[2];
extern struct ThreadmgrFastMutex g_audioInAecLock;
extern void *g_audioInAecWorkspace;
extern unsigned int g_audioInAecWorkspaceSize;
extern unsigned int g_audioInAecSampleCount;

typedef struct AudioInAecRpcContext {
	int16_t *sampleBuffers[2];
	unsigned int context[2];
	void *workspace;
	unsigned int workspaceSize;
} AudioInAecRpcContext;

_Static_assert(sizeof(AudioInPort) == 64, "AudioInPort size");
_Static_assert(sizeof(AudioInCaptureBuffer) == 28, "AudioInCaptureBuffer size");
_Static_assert(sizeof(AudioInCaptureBuffers) == 196, "AudioInCaptureBuffers size");
_Static_assert(sizeof(AudioInAecContext) == 20, "AudioInAecContext size");
_Static_assert(sizeof(AudioInSampleView) == 12, "AudioInSampleView size");
_Static_assert(sizeof(AudioInLegacyQueue) == 730, "AudioInLegacyQueue size");
_Static_assert(sizeof(AudioInResamplerState) == 16, "AudioInResamplerState size");
_Static_assert(sizeof(AudioInAecRpcContext) == 24, "AudioInAecRpcContext size");

extern AudioInPort g_audioInPorts[4];
extern struct ThreadmgrFastMutex g_audioInLock;
extern unsigned int g_audioInOpenPortCount;
extern int g_audioInAppliedInput;
extern int g_audioInRequestEventFlag;
extern int g_audioInRequestFlags;
extern int g_audioInAecEventFlag;
extern int g_audioInAecEnabled;
extern int g_audioInCaptureDevice;
extern int16_t *g_audioInAecSamples[2];
extern volatile int g_audioInAecFlags;
extern AudioInAecContext g_audioInAecContext;
extern int g_audioInOwnerPid;
extern int g_audioInInitialized;
extern int g_audioInInputThreadId;
extern int g_audioInAecThreadId;
extern unsigned int g_audioInUnidentifiedState;
extern void (*g_audioInPortClosed)(int pid);
extern unsigned int g_audioInCaptureGrain;
extern unsigned int g_audioInCaptureFreq;
extern AudioInCaptureBuffers g_audioInBuffers;
#define g_audioInInputBuffer (g_audioInBuffers.input)
#define g_audioInPrimary48Buffer (g_audioInBuffers.primary48)
#define g_audioInSecondary48Buffer (g_audioInBuffers.secondary48)
#define g_audioInPrimary16Buffer (g_audioInBuffers.primary16)
#define g_audioInSecondary16Buffer (g_audioInBuffers.secondary16)
#define g_audioInVoice8Buffer (g_audioInBuffers.voice8)
#define g_audioInCombined16Buffer (g_audioInBuffers.combined16)
extern int16_t g_audioInPrimary48Samples[1536];
extern int16_t g_audioInSecondary48Samples[1536];
extern int16_t g_audioInInputSamples[6144] __attribute__((aligned(16)));
extern int16_t g_audioInPrimary16Samples[1024];
extern int16_t g_audioInSecondary16Samples[1024];
extern int16_t g_audioInVoice8Samples[256];
extern int16_t g_audioInCombined16Samples[1024];
extern int16_t g_audioInAecReferenceSamples[1024];
extern int16_t g_audioInCombined8Samples[256];
extern unsigned int g_audioInRequestedInput;
extern int g_audioInChannelSelector;
extern AudioInSampleView g_audioInSampleViews[9];
extern unsigned int g_audioInReadyViews;
extern int g_audioInOwnerUnmuteDelay;
extern int g_audioInDeviceStateEnabled;
extern int g_audioInDeviceChangeCountdown;
extern int g_audioInStartupSilenceFrames;
extern unsigned int g_audioInLegacyCaptureControl;
extern void (*g_audioInPortOpened)(int pid);
extern const unsigned char g_audioInInternalAecControl[16];
extern const unsigned char g_audioInExternalAecControl[16];
extern const unsigned char g_audioInAlternateAecControl[16];
extern const int16_t g_audioInSilenceSamples[768];
extern const char g_audioInProcessNameFormat[];
extern const char g_audioInMutexName[];
extern const char g_audioInRequestEventName[];
extern const char g_audioInAecRequestEventName[];
extern const char g_audioInInputThreadName[];
extern const char g_audioInAecThreadName[];
extern const char g_audioInModuleName[];
extern const char g_audioInAecMutexName[];

int audioInReturnZero(void);
unsigned int configureAudioInCaptureBuffers(void);
int audioInProcessExitHandler(SceUID pid, ProcEventParam *param, void *common);
int audioInProcessKillHandler(SceUID pid, ProcEventParam *param, void *common);
int audioInProcessStopHandler(SceUID pid, int eventType, ProcEventParam *param,
		void *common);
int audioInProcessStartHandler(SceUID pid, int eventType, ProcEventParam *param,
		void *common);
int audioInSysEventHandler(int resume, int eventId, void *args, void *opt);
int audioInCaptureThread(SceSize size, void *argument) __attribute__((noreturn));
int audioInAecThread(SceSize size, void *argument);
void *initializeAudioInCaptureBuffers(void);
int writeAudioInLegacyQueue(const int16_t *samples, int count);

/* aec.c */
int initializeAudioInAecLibrary(void);
int startAudioInAec(AudioInAecContext *context);
int processAudioInAecBuffers(AudioInAecContext *context, int16_t *first,
		int16_t *second, int gain, unsigned int flags);
int configureAudioInAec(AudioInAecContext *context, const void *control);

/* legacy.c */
void initializeAudioInLegacyQueue(void);
int readAudioInLegacyQueue(int16_t *samples, unsigned int count);
void resetAudioInLegacyQueue(void);

/* resampler.c */
void initializeAudioInResampler(void);
void resampleAudioInCapture(unsigned int stream, const int16_t *input,
		int16_t *output, unsigned int count, int channel);

/* Process port teardown shared by the kill and exit callbacks. */
#define CLOSE_PROCESS_PORT(index) do { \
	if (pid == g_audioInPorts[index].pid) { \
		if (g_audioInPorts[index].eventFlag != 0) { \
			if (g_audioInPorts[index].flags & 0x10) { \
				g_audioInPorts[index].flags &= 0xffffff79; \
				sceKernelSetEventFlag(g_audioInPorts[index].eventFlag, 1024); \
			} else { \
				sceKernelDeleteEventFlag(g_audioInPorts[index].eventFlag); \
				g_audioInPorts[index].eventFlag = 0; \
			} \
		} \
		g_audioInPorts[index].flags = 0; \
		g_audioInPorts[index].pid = 0; \
		g_audioInOpenPortCount--; \
		g_audioInPorts[index].port = 0; \
		if (g_audioInPortClosed != 0) \
			g_audioInPortClosed(pid); \
	} \
} while (0)

/* aec_core.c */
int initializeAudioInAecMutex(void);
int createAudioInAecContext(AudioInAecContext *context);
int processAudioInAecContext(AudioInAecContext *context, int gain, unsigned int flags);
int configureAudioInAecContext(AudioInAecContext *context, const void *control);

/* aec_rpc.c */
int queryAudioInAecWorkspace(AudioInAecRpcContext *context);
int initializeAudioInAecContextRpc(AudioInAecRpcContext *context);
int processAudioInAecContextRpc(AudioInAecRpcContext *context, int gain, unsigned int flags);
int configureAudioInAecContextRpc(AudioInAecRpcContext *context, const void *control);

#endif
