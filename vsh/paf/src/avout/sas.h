/* Handle-based SAS entry points linked into ScePaf. */
#ifndef PAF_AVOUT_SAS_H
#define PAF_AVOUT_SAS_H

#include <scetypes.h>

typedef struct SasContext SasContext;

int SasGetNeededMemorySize(const char *config, SceSize *size);
int SasInit(const char *config, void *buffer, SceSize size, SasContext **context);
int SasExit(SasContext *context, void **buffer, SceSize *size);
int SasSetGrain(SasContext *context, unsigned int grain);
int SasCoreWithMix(SasContext *context, SceInt16 *output, int left, int right);
int SasSetSimpleADSR(SasContext *context, int voice, unsigned int adsr1,
		     unsigned int adsr2);
int SasSetPitch(SasContext *context, int voice, int pitch);
int SasSetVoice(SasContext *context, int voice, const void *buffer,
	       SceSize size, unsigned int loop);
int SasGetEndState(SasContext *context, int voice);
int SasSetVolume(SasContext *context, int voice, int left, int right,
		 int wetLeft, int wetRight);
int SasSetKeyOn(SasContext *context, int voice);
int SasSetKeyOff(SasContext *context, int voice);

#endif
