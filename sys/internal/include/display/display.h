/*
 * SceDisplay (os0:kd/display.skprx): SceDisplayForDriver, the display's exports, each declared
 * once with its definition as the prototype.
 */

#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <display.h>
#include <stddef.h>

/* name unknown: a plane's frame buffer and who owns it (SceDisplayForDriver_3BC165EF) */
typedef struct SceDisplayFrameBufInfo {
	SceSize size;			/* 0x00: sizeof(SceDisplayFrameBufInfo) */
	SceUID pid;			/* 0x04: the owner */
	SceUInt32 vblankcount;		/* 0x08: vblanks shown */
	uintptr_t paddr;		/* 0x0c: the base's physical address */
	SceDisplayFrameBuf framebuf;	/* 0x10 */
	SceUInt32 resolution;		/* 0x28 */
} SceDisplayFrameBufInfo;

_Static_assert(sizeof(SceDisplayFrameBufInfo) == 0x2c, "SceDisplayFrameBufInfo");

/* SceDisplayForDriver_EE5EB52D / SceDisplayForDriver_40ACFE51 (0x14 bytes). */
typedef struct SceDisplayViewportConf {
	SceSize size;			/* 0x00: sizeof(SceDisplayViewportConf) */
	SceUInt32 x;			/* 0x04: in a 960x544 frame */
	SceUInt32 y;			/* 0x08 */
	SceUInt32 width;		/* 0x0c */
	SceUInt32 height;		/* 0x10 */
} SceDisplayViewportConf;

/* vitasdk: ksceDisplayGetActualViewportConf */
int SceDisplayForDriver_40ACFE51(SceUInt32 head, SceUInt32 plane, SceDisplayViewportConf *conf);
int SceDisplayForDriver_8D9A1CCE(SceUInt32 head, SceUInt32 *type);	/* vitasdk: ksceDisplayGetDeviceType */
/* vitasdk: ksceDisplaySetBrightness */
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

/* sceDisplayGetResolutionInfoInternal (0x1c bytes). */
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

int sceDisplayGetResolutionInfoInternal(SceUInt32 head,
		SceDisplayResolutionInfo *info);

/* vitasdk: ksceDisplaySetOutputMode */
int SceDisplayForDriver_AF5EE5BE(SceUInt32 head, SceUInt32 vic, SceUInt32 mode);
int __attribute__((pcs("aapcs-vfp"))) SceDisplayForDriver_EB390A76(float scale,
		SceUInt32 head, SceUInt32 plane, SceUInt32 flags);
int sceDisplayEnableHead(SceUInt32 head);
int sceDisplayDisableHead(SceUInt32 head);
/* vitasdk: ksceDisplaySetColorSpaceMode */
int SceDisplayForDriver_8D79D187(SceUInt32 head, int mode);

/* capture the frame being shown on head into fb (by DMA) */
/* vitasdk: ksceDisplayCaptureFrameBufDMACInternal */
int SceDisplayForDriver_707EEE2E(SceUID pid, int head, int index, SceDisplayFrameBuf *fb);

/* the frame buffer set on head (index 0: current) */
int sceDisplayGetFrameBufInternal(int head, int index, SceDisplayFrameBuf *fb, int sync);

void SceDisplayForDriver_086DEFB6(SceUInt32 *status, SceUID pid);
int SceDisplayForDriver_332C5410(int head);
int SceDisplayForDriver_BC76296A(int enable);
/* vitasdk: ksceDisplaySetMergeConf */
int SceDisplayForDriver_6B198052(SceUInt32 head, SceUInt32 setting, SceUInt32 alpha);

/* vitasdk: ksceDisplayWaitSetFrameBufInternal */
int SceDisplayForDriver_12A77662(SceUInt32 head, SceUInt32 plane);
int sceDisplaySetFrameBufInternal(int head, int plane, const SceDisplayFrameBuf *pParam, int sync);
int SceDisplayForDriver_19140ACD(int head, int enable);	/* vitasdk: ksceDisplaySetInvertColors */
int sceDisplayWaitSetFrameBuf(void);
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf *pParam, int sync);
/* vitasdk: ksceDisplayWaitVblankStartMultiInternal */
int SceDisplayForDriver_32287576(int head, unsigned int vcount);
/* vitasdk: ksceDisplayWaitSetFrameBufCBInternal */
int SceDisplayForDriver_35466D63(SceUInt32 head, SceUInt32 plane);
/* vitasdk: ksceDisplayGetProcFrameBufInternal */
int SceDisplayForDriver_3BC165EF(SceUID pid, int head, int plane, SceDisplayFrameBufInfo *info);
int SceDisplayForDriver_3D95D478(SceUInt32 head, int mode);
int sceDisplayWaitVblankStartMulti(unsigned int vcount);
int sceDisplayWaitVblankStartCB(void);
/* vitasdk: ksceDisplayRegisterVblankStartCallbackInternal */
int SceDisplayForDriver_4AE2A2B1(int head, SceUID uid);
int sceDisplayUnregisterVblankStartCallback(SceUID uid);
/* vitasdk: ksceDisplayGetMaximumFrameBufResolution */
int SceDisplayForDriver_5AFE6CD3(int *width, int *height);
/* vitasdk: ksceDisplayWaitVblankStartMultiCBInternal */
int SceDisplayForDriver_61421AAF(int head, unsigned int vcount);
/* vitasdk: ksceDisplayWaitSetFrameBufMultiCBInternal */
int SceDisplayForDriver_6DC8F0F5(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount);
int SceDisplayForDriver_6E22990E(SceUID uid);	/* vitasdk: ksceDisplayRegisterFrameBufCallback */
int sceDisplayWaitVblankStartMultiCB(unsigned int vcount);
/* vitasdk: ksceDisplayGetRefreshRateInternal */
int SceDisplayForDriver_7911958E(SceUInt32 head, float *rate, int *interlaced);
int sceDisplayRegisterVblankStartCallback(SceUID uid);
int sceDisplayWaitSetFrameBufMultiCB(unsigned int vcount);
int sceDisplayGetVcountInternal(int head);
int SceDisplayForDriver_8F3C0E19(int head);	/* vitasdk: ksceDisplayWaitVblankStartCBInternal */
int sceDisplayWaitVblankStart(void);
int sceDisplayWaitSetFrameBufCB(void);
/* vitasdk: ksceDisplayUnregisterVblankStartCallbackInternal */
int SceDisplayForDriver_B027433E(int head, SceUID uid);
/* vitasdk: ksceDisplayCaptureFrameBufIFTU */
int SceDisplayForDriver_B0CED8BC(SceUID pid, SceDisplayFrameBuf *fb);
/* vitasdk: ksceDisplaySetOwner */
int SceDisplayForDriver_B54962A1(int head, int plane, SceUID pid);
int sceDisplayWaitVblankStartInternal(int head);
int sceDisplayGetPrimaryHead(void);
/* vitasdk: ksceDisplayGetOutputMode */
int SceDisplayForDriver_D2CED235(SceUInt32 head, SceUInt32 *vic, SceUInt32 *mode);
/* vitasdk: ksceDisplayCaptureFrameBufIFTUInternal */
int SceDisplayForDriver_D4C812E5(SceUID pid, SceUInt32 head, SceUInt32 plane, SceDisplayFrameBuf *fb);
int sceDisplayWaitSetFrameBufMulti(unsigned int vcount);
/* vitasdk: ksceDisplaySetViewportConf */
int SceDisplayForDriver_EE5EB52D(SceUInt32 head, SceUInt32 plane, const SceDisplayViewportConf *conf);
int sceDisplayGetFrameBuf(SceDisplayFrameBuf *pParam, int sync);
/* vitasdk: ksceDisplayCaptureFrameBufDMAC */
int SceDisplayForDriver_F116D0B4(SceUID pid, SceDisplayFrameBuf *fb);
/* vitasdk: ksceDisplayWaitSetFrameBufMultiInternal */
int SceDisplayForDriver_F83C95B1(SceUInt32 head, SceUInt32 plane, SceUInt32 vcount);
/* vitasdk: ksceDisplayRegisterFrameBufCallbackInternal */
int SceDisplayForDriver_FA7CE579(int head, SceUID uid);
int _sceDisplayGetMaximumFrameBufResolution(int *pWidth, int *pHeight);
int sceDisplayGetRefreshRate(float *pFps);
int sceDisplayGetVcount(void);
int sceDisplayGetVcountInternalForUser(SceUInt32 head);
int sceDisplayRegisterVblankStartCallbackForUser(SceUID uid);
int sceDisplayUnregisterVblankStartCallbackForUser(SceUID uid);
int sceDisplayWaitSetFrameBufForUser(void);
int sceDisplayWaitSetFrameBufCBForUser(void);
int sceDisplayWaitSetFrameBufMultiForUser(SceUInt32 vcount);
int sceDisplayWaitSetFrameBufMultiCBForUser(SceUInt32 vcount);
int sceDisplayWaitVblankStartForUser(void);
int sceDisplayWaitVblankStartCBForUser(void);
int sceDisplayWaitVblankStartMultiForUser(SceUInt32 vcount);
int sceDisplayWaitVblankStartMultiCBForUser(SceUInt32 vcount);
