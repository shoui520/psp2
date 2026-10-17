/*
 * SceDisplay (os0:kd/display.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/display.h>
#include <stddef.h>

int ksceDisplaySetBrightness(SceUInt32 head, SceUInt32 brightness);

/* Extended frame buffer parameter accepted when frameBuf.size is 0x1c. */
typedef struct DisplayFrameBufParam {
	SceDisplayFrameBuf frameBuf;
	SceUInt32 resolution;
} DisplayFrameBufParam;

_Static_assert(sizeof(DisplayFrameBufParam) == 28,
	       "extended display frame buffer parameter size");
_Static_assert(offsetof(DisplayFrameBufParam, resolution) == 24,
	       "extended display frame buffer resolution");

/* ksceDisplayGetResolutionInfoInternal (0x1c bytes). */
typedef struct SceDisplayResolutionInfo {
	SceSize size;			/* 0x00: sizeof(SceDisplayResolutionInfo) */
	SceUInt32 vic;			/* 0x04 */
	SceUInt32 width;		/* 0x08 */
	SceUInt32 height;		/* 0x0c */
	SceUInt32 mode;			/* 0x10 */
	int interlaced;			/* 0x14 */
	float refreshRate;		/* 0x18 */
} SceDisplayResolutionInfo;

_Static_assert(sizeof(SceDisplayResolutionInfo) == 28,
	       "display resolution information size");
_Static_assert(offsetof(SceDisplayResolutionInfo, vic) == 4,
	       "display resolution VIC");
_Static_assert(offsetof(SceDisplayResolutionInfo, width) == 8,
	       "display resolution width");
_Static_assert(offsetof(SceDisplayResolutionInfo, height) == 12,
	       "display resolution height");
_Static_assert(offsetof(SceDisplayResolutionInfo, mode) == 16,
	       "display resolution mode");
_Static_assert(offsetof(SceDisplayResolutionInfo, interlaced) == 20,
	       "display interlaced output");
_Static_assert(offsetof(SceDisplayResolutionInfo, refreshRate) == 24,
	       "display refresh rate");

int ksceDisplayGetResolutionInfoInternal(SceUInt32 head,
		SceDisplayResolutionInfo *info);

int ksceDisplaySetOutputMode(SceUInt32 head, SceUInt32 vic, SceUInt32 mode);
int __attribute__((pcs("aapcs-vfp"))) ksceDisplaySetScaleConf(float scale,
		SceUInt32 head, SceUInt32 plane, SceUInt32 flags);
int ksceDisplayEnableHead(SceUInt32 head);
int ksceDisplayDisableHead(SceUInt32 head);
int ksceDisplaySetColorSpaceMode(SceUInt32 head, int mode);

/* capture the frame being shown on head into fb (by DMA) */
int ksceDisplayCaptureFrameBufDMACInternal(SceUID pid, int head, int index, SceDisplayFrameBuf *fb);

/* the frame buffer set on head (index 0: current) */
int ksceDisplayGetFrameBufInternal(int head, int index, SceDisplayFrameBuf *fb, int sync);

void SceDisplayForDriver_086DEFB6(SceUInt32 *status, SceUID pid);
int SceDisplayForDriver_332C5410(int head);
int SceDisplayForDriver_BC76296A(int enable);
int ksceDisplaySetMergeConf(SceUInt32 head, SceUInt32 setting, SceUInt32 alpha);
