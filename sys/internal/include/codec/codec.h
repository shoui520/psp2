#ifndef SCE_CODEC_H
#define SCE_CODEC_H

#include <scetypes.h>
#include <lowio/pervasive.h>
#include <clockgen/clockgen.h>

int SceCodecForDriver_8932F886(SceUInt32 volume);
int SceCodecForDriver_762BF716(unsigned int outputs);

/* Low-level dependencies used by SceCodec; NID names remain unknown. */

#endif
