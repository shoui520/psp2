/*
 * SceDisplay (os0:kd/display.skprx) driver interface additions to vitasdk.
 */

#pragma once

#include <psp2kern/display.h>
#include <stddef.h>

int SceDisplayForDriver_9E3C6DC6(SceUInt32 head, SceUInt32 brightness);

/* Extended frame buffer parameter accepted when frameBuf.size is 0x1c. */
typedef struct DisplayFrameBufParam {
	SceDisplayFrameBuf frameBuf;
	SceUInt32 resolution;
} DisplayFrameBufParam;

_Static_assert(sizeof(DisplayFrameBufParam) == 28,
	       "extended display frame buffer parameter size");
_Static_assert(offsetof(DisplayFrameBufParam, resolution) == 24,
	       "extended display frame buffer resolution");

/* SceDisplayForDriver_B3C6D647 (0x1c bytes). */
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

int SceDisplayForDriver_B3C6D647(SceUInt32 head,
		SceDisplayResolutionInfo *info);

int SceDisplayForDriver_AF5EE5BE(SceUInt32 head, SceUInt32 vic, SceUInt32 mode);
int __attribute__((pcs("aapcs-vfp"))) SceDisplayForDriver_EB390A76(float scale,
		SceUInt32 head, SceUInt32 plane, SceUInt32 flags);
int SceDisplayForDriver_496032D6(SceUInt32 head);
int SceDisplayForDriver_43347565(SceUInt32 head);
int SceDisplayForDriver_8D79D187(SceUInt32 head, int mode);

/* capture the frame being shown on head into fb (by DMA) */
int SceDisplayForDriver_707EEE2E(SceUID pid, int head, int index, SceDisplayFrameBuf *fb);

/* the frame buffer set on head (index 0: current) */
int SceDisplayForDriver_19F94C63(int head, int index, SceDisplayFrameBuf *fb, int sync);

void SceDisplayForDriver_086DEFB6(SceUInt32 *status, SceUID pid);
int SceDisplayForDriver_332C5410(int head);
int SceDisplayForDriver_BC76296A(int enable);
int SceDisplayForDriver_6B198052(SceUInt32 head, SceUInt32 setting, SceUInt32 alpha);

int SceDisplayForDriver_12A77662(SceUInt32 head, SceUInt32 plane);
int SceDisplayForDriver_16466675(int head, int plane, const SceDisplayFrameBuf *pParam, int sync);
int SceDisplayForDriver_19140ACD(int head, int enable);
int SceDisplayForDriver_1C0C9C4A(void);
int SceDisplayForDriver_289D82FE(const SceDisplayFrameBuf *pParam, int sync);
int SceDisplayForDriver_32287576(int head, unsigned int vcount);
int SceDisplayForDriver_35466D63(SceUInt32 head, SceUInt32 plane);
int SceDisplayForDriver_3BC165EF(SceUID pid, int head, int plane, SceDisplayFrameBufInfo *info);
int SceDisplayForDriver_3D95D478(SceUInt32 head, int mode);
int SceDisplayForDriver_40F1469C(unsigned int vcount);
int SceDisplayForDriver_46F186C3(void);
int SceDisplayForDriver_4AE2A2B1(int head, SceUID uid);
int SceDisplayForDriver_4B27191F(SceUID uid);
int SceDisplayForDriver_5AFE6CD3(int *width, int *height);
int SceDisplayForDriver_61421AAF(int head, unsigned int vcount);
int SceDisplayForDriver_6DC8F0F5(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount);
int SceDisplayForDriver_6E22990E(SceUID uid);
int SceDisplayForDriver_77ED8B3A(unsigned int vcount);
int SceDisplayForDriver_7911958E(SceUInt32 head, float *rate, int *interlaced);
int SceDisplayForDriver_7FB0BD28(SceUID uid);
int SceDisplayForDriver_863EACBE(unsigned int vcount);
int SceDisplayForDriver_8B5DA27B(int head);
int SceDisplayForDriver_8F3C0E19(int head);
int SceDisplayForDriver_984C27E7(void);
int SceDisplayForDriver_9D7F203C(void);
int SceDisplayForDriver_B027433E(int head, SceUID uid);
int SceDisplayForDriver_B0CED8BC(SceUID pid, SceDisplayFrameBuf *fb);
int SceDisplayForDriver_B54962A1(int head, int plane, SceUID pid);
int SceDisplayForDriver_B80CA224(int head);
int SceDisplayForDriver_C8E554C5(void);
int SceDisplayForDriver_D2CED235(SceUInt32 head, SceUInt32 *vic, SceUInt32 *mode);
int SceDisplayForDriver_D4C812E5(SceUID pid, SceUInt32 head, SceUInt32 plane, SceDisplayFrameBuf *fb);
int SceDisplayForDriver_E6D27E0A(unsigned int vcount);
int SceDisplayForDriver_EEDA2E54(SceDisplayFrameBuf *pParam, int sync);
int SceDisplayForDriver_F116D0B4(SceUID pid, SceDisplayFrameBuf *fb);
int SceDisplayForDriver_F83C95B1(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount);
int SceDisplayForDriver_FA7CE579(int head, SceUID uid);
int _sceDisplayGetMaximumFrameBufResolution(int *pWidth, int *pHeight);
int sceDisplayGetRefreshRate(float *pFps);
int sceDisplayGetVcount(void);
int sceDisplayGetVcountInternal(SceUInt32 head);
int sceDisplayRegisterVblankStartCallback(SceUID uid);
int sceDisplayUnregisterVblankStartCallback(SceUID uid);
int sceDisplayWaitSetFrameBuf(void);
int sceDisplayWaitSetFrameBufCB(void);
int sceDisplayWaitSetFrameBufMulti(SceUInt32 vcount);
int sceDisplayWaitSetFrameBufMultiCB(SceUInt32 vcount);
int sceDisplayWaitVblankStart(void);
int sceDisplayWaitVblankStartCB(void);
int sceDisplayWaitVblankStartMulti(SceUInt32 vcount);
int sceDisplayWaitVblankStartMultiCB(SceUInt32 vcount);
