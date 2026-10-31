/* Internal wave-engine interface used by paf::sound::Output. */
#ifndef PAF_AVOUT_WAVE_H
#define PAF_AVOUT_WAVE_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int WaveInit(void);
int WaveInitExt(void);
int WaveStart(int priority, SceUInt attr, int affinity, int memblockType);
int WaveKeyOff(int voice);
int WaveStop(void);
int WaveExit(void);
int WaveSetVoice(int voice, const char *buffer, unsigned int size,
		 int samplerate, int loop);
int WaveIsPlaying(int voice);
int WaveKeyOn(int voice, int left, int right);
int WaveKeyOffAll(void);

#ifdef __cplusplus
}
#endif

#endif
