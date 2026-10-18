#ifndef SCE_CODEC_H
#define SCE_CODEC_H

#include <psp2kern/types.h>
#include <lowio/pervasive.h>

int SceCodecForDriver_8932F886(SceUInt32 volume);
int SceCodecForDriver_762BF716(unsigned int outputs);

/* Low-level dependencies used by SceCodec; NID names remain unknown. */
int SceClockgenForDriver_5130682D(void);

#endif
