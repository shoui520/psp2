/*
 * SceCodec (os0:kd/codec.skprx, the WM1803 driver, or codec_cx.skprx): SceCodecForDriver, each
 * export declared once; both drivers define it with this prototype.
 */
#ifndef SCE_CODEC_H
#define SCE_CODEC_H

#include <scetypes.h>
#include <lowio/pervasive.h>
#include <clockgen/clockgen.h>

int SceCodecForDriver_23BF8884(unsigned int reg, unsigned int value);
int SceCodecForDriver_41CFFE48(int *level0, int *level1);
int SceCodecForDriver_4FA36937(unsigned int reg);
int SceCodecForDriver_762BF716(unsigned int outputs);
int SceCodecForDriver_8932F886(SceUInt32 volume);
int SceCodecForDriver_C1DB7BAF(unsigned int mode);
int SceCodecForDriver_CCDE1B04(unsigned int mode);
int SceCodecForDriver_CF398D8A(void);
int SceCodecForDriver_E483B663(int enable);

#endif
