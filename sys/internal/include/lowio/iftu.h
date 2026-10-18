/* IFTU conversion parameters shared by the driver and its callers. */
#pragma once

#include <vitasdk/build_utils.h>
#include <psp2kern/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SceIftuErrorCode {
	SCE_IFTU_ERROR_INVALID_PLANE = 0x803F0700,
	SCE_IFTU_ERROR_INVALID_PARAM = 0x803F0701,
	SCE_IFTU_ERROR_INVALID_PIXELFORMAT = 0x803F0703,
	SCE_IFTU_ERROR_PLANE_BUSY = 0x803F0704,
} SceIftuErrorCode;

typedef enum SceIftuPixelformat {
	SCE_IFTU_PIXELFORMAT_BGR565 = 0x01,
	SCE_IFTU_PIXELFORMAT_RGB565 = 0x02,
	SCE_IFTU_PIXELFORMAT_BGRA5551 = 0x04,
	SCE_IFTU_PIXELFORMAT_RGBA5551 = 0x08,
	SCE_IFTU_PIXELFORMAT_BGRX8888 = 0x10,
	SCE_IFTU_PIXELFORMAT_RGBX8888 = 0x20,
	SCE_IFTU_PIXELFORMAT_BGRA1010102 = 0x40,
	SCE_IFTU_PIXELFORMAT_RGBA1010102 = 0x80,
	SCE_IFTU_PIXELFORMAT_BGRP = 0x100,
	SCE_IFTU_PIXELFORMAT_RGBX8888_MULT = 0x1000,
	SCE_IFTU_PIXELFORMAT_BGRX8888_MULT = 0x2000,
	SCE_IFTU_PIXELFORMAT_RGBA1010102_MULT = 0x4000,
	SCE_IFTU_PIXELFORMAT_BGRA1010102_MULT = 0x8000,
	SCE_IFTU_PIXELFORMAT_NV12 = 0x10000,
	SCE_IFTU_PIXELFORMAT_YUV420 = 0x20000,
	SCE_IFTU_PIXELFORMAT_YUV422 = 0x200000,
} SceIftuPixelformat;

typedef struct SceIftuCscParams {
	unsigned int post_add_0;
	unsigned int post_add_1_2;
	unsigned int post_clamp_max_0;
	unsigned int post_clamp_min_0;
	unsigned int post_clamp_max_1_2;
	unsigned int post_clamp_min_1_2;
	unsigned int ctm[3][3];
} SceIftuCscParams;

typedef struct SceIftuConvParams {
	unsigned int size;
	unsigned int filterMode;
	SceIftuCscParams *csc_params1;
	SceIftuCscParams *csc_params2;
	unsigned int csc_control;
	unsigned int backgroundColor[3];
	unsigned int alpha;
	unsigned int fieldMode;
} SceIftuConvParams;

typedef struct SceIftuFrameBuf {
	unsigned int pixelformat;
	unsigned int width;
	unsigned int height;
	unsigned int leftover_stride;
	unsigned int leftover_align;
	unsigned int paddr0;
	unsigned int paddr1;
	unsigned int paddr2;
} SceIftuFrameBuf;

typedef struct SceIftuPlaneState {
	SceIftuFrameBuf fb;
	unsigned int sourceConfig[3];
	unsigned int scaleX;
	unsigned int scaleY;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int sampleX;
	unsigned int sampleY;
	unsigned int vtop_padding;
	unsigned int vbot_padding;
	unsigned int hleft_padding;
	unsigned int hright_padding;
} SceIftuPlaneState;

VITASDK_BUILD_ASSERT_EQ(60, SceIftuCscParams);
VITASDK_BUILD_ASSERT_EQ(40, SceIftuConvParams);
VITASDK_BUILD_ASSERT_EQ(32, SceIftuFrameBuf);
VITASDK_BUILD_ASSERT_EQ(84, SceIftuPlaneState);

int SceIftuForDriver_67E37EFC(SceIftuFrameBuf *dst, SceIftuPlaneState *src,
	      SceIftuConvParams *params);	/* vitasdk: ksceIftuCsc */

int SceIftuForDriver_0FCBF457(unsigned int index, SceIftuCscParams *param);
int SceIftuForDriver_357EAE24(unsigned int index, unsigned int alpha);
int SceIftuForDriver_D64F4C6B(unsigned int index, SceIftuCscParams *param);
int SceIftuForDriver_C11F30B3(unsigned int index);	/* vitasdk: ksceIftuDisable */
int SceIftuForDriver_0D7C02F7(unsigned int index);	/* vitasdk: ksceIftuEnable */
/* vitasdk: ksceIftuSetInputFrameBuffer */
int SceIftuForDriver_7CE0C4DA(unsigned int index, SceIftuPlaneState *state, int bilinear, int sync);
/* vitasdk: ksceIftuSetMergeSetting */
int SceIftuForDriver_AF19FD85(unsigned int index, int setting);
/* vitasdk: ksceIftuSetOutputFormat */
int SceIftuForDriver_E6EE2C6B(int index, unsigned int width, unsigned int height, unsigned int pixelFormat, unsigned int planeEnable);

#ifdef __cplusplus
}
#endif
