/*
 * scejpeg_csc.c: NEON YCbCr-to-RGBA/RGB565/RGBA5551/RGBA4444 color space
 * conversion for ScePspemu MJPEG/JPEG decoding (0x8101d5f8).
 */
#include "host_common.h"
#include <arm_neon.h>

#define PSPEMU_CSC_ERR_INVALID_POINTER     ((int)0x80650110)
#define PSPEMU_CSC_ERR_UNSUP_COLORSPACE    ((int)0x80650113)
#define PSPEMU_CSC_ERR_UNSUP_IMAGE_SIZE    ((int)0x80650120)

#define PSPEMU_CSC_YUV_444                 0x0101U
#define PSPEMU_CSC_YUV_422                 0x0201U
#define PSPEMU_CSC_YUV_420                 0x0202U
#define PSPEMU_CSC_YUV_BILINEAR_FLAG       0x00100000U

/*
 * Color conversion coefficients in Q10:
 *   [0] = Y offset, [1] = chroma offset (-128), [2] = Y scale, [3] = alpha (255),
 *   [4] = Cr->R,    [5] = Cb->G,                [6] = Cr->G,   [7] = Cb->B
 */
static const int16_t g_jpegCscCoeffs[2][8] = {
	{  0, -128, 1024, 255, 1436, -352, -731, 1815 },
	{ 16, -128, 1192, 255, 1634, -401, -832, 2066 },
};

static inline void cscStore4Rgba(uint8_t *dst, int32x4_t yScaled,
                                 int16x4_t cb, int16x4_t cr,
                                 int16x4_t mat, int16x4_t alphaVec)
{
	static const uint8_t kPermA[8] = { 8, 4, 0, 12, 9, 5, 1, 13 };
	static const uint8_t kPermB[8] = { 10, 6, 2, 14, 11, 7, 3, 15 };
	int16x4_t blueWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cb, mat, 3), 10);
	int16x4_t greenWide = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(yScaled, cb, mat, 1), cr, mat, 2), 10);
	int16x4_t redWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cr, mat, 0), 10);
	uint8x8x2_t bgra;

	bgra.val[0] = vqmovun_s16(vcombine_s16(blueWide, greenWide));
	bgra.val[1] = vqmovun_s16(vcombine_s16(redWide, alphaVec));
	vst1_u8(dst, vtbl2_u8(bgra, vld1_u8(kPermA)));
	vst1_u8(dst + 8, vtbl2_u8(bgra, vld1_u8(kPermB)));
}

static inline void cscStore4Rgb565(uint16_t *dst, int32x4_t yScaled,
                                   int16x4_t cb, int16x4_t cr, int16x4_t mat)
{
	static const uint8_t kLoExpand[8] = { 32, 0, 32, 1, 32, 2, 32, 3 };
	static const uint8_t kHiExpand[8] = { 32, 4, 32, 5, 32, 6, 32, 7 };
	int16x4_t blueWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cb, mat, 3), 10);
	int16x4_t greenWide = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(yScaled, cb, mat, 1), cr, mat, 2), 10);
	int16x4_t redWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cr, mat, 0), 10);
	uint8x8_t bg = vqmovun_s16(vcombine_s16(blueWide, greenWide));
	uint8x8_t rr = vqmovun_s16(vcombine_s16(redWide, redWide));
	uint16x4_t bHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kLoExpand)));
	uint16x4_t gHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kHiExpand)));
	uint16x4_t rHi = vreinterpret_u16_u8(vtbl1_u8(rr, vld1_u8(kLoExpand)));
	uint16x4_t packed = vsri_n_u16(vsri_n_u16(bHi, gHi, 5), rHi, 11);

	vst1_u16(dst, packed);
}

static inline void cscStore4Rgba5551(uint16_t *dst, int32x4_t yScaled,
                                     int16x4_t cb, int16x4_t cr, int16x4_t mat)
{
	static const uint8_t kLoExpand[8] = { 32, 0, 32, 1, 32, 2, 32, 3 };
	static const uint8_t kHiExpand[8] = { 32, 4, 32, 5, 32, 6, 32, 7 };
	int16x4_t blueWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cb, mat, 3), 10);
	int16x4_t greenWide = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(yScaled, cb, mat, 1), cr, mat, 2), 10);
	int16x4_t redWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cr, mat, 0), 10);
	uint8x8_t bg = vqmovun_s16(vcombine_s16(blueWide, greenWide));
	uint8x8_t rr = vqmovun_s16(vcombine_s16(redWide, redWide));
	uint16x4_t aHi = vdup_n_u16(0x8000U);
	uint16x4_t bHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kLoExpand)));
	uint16x4_t gHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kHiExpand)));
	uint16x4_t rHi = vreinterpret_u16_u8(vtbl1_u8(rr, vld1_u8(kLoExpand)));
	uint16x4_t packed = vsri_n_u16(vsri_n_u16(vsri_n_u16(aHi, bHi, 1), gHi, 6), rHi, 11);

	vst1_u16(dst, packed);
}

static inline void cscStore4Rgba4444(uint16_t *dst, int32x4_t yScaled,
                                     int16x4_t cb, int16x4_t cr, int16x4_t mat)
{
	static const uint8_t kLoExpand[8] = { 32, 0, 32, 1, 32, 2, 32, 3 };
	static const uint8_t kHiExpand[8] = { 32, 4, 32, 5, 32, 6, 32, 7 };
	int16x4_t blueWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cb, mat, 3), 10);
	int16x4_t greenWide = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(yScaled, cb, mat, 1), cr, mat, 2), 10);
	int16x4_t redWide = vqrshrn_n_s32(vmlal_lane_s16(yScaled, cr, mat, 0), 10);
	uint8x8_t bg = vqmovun_s16(vcombine_s16(blueWide, greenWide));
	uint8x8_t rr = vqmovun_s16(vcombine_s16(redWide, redWide));
	uint16x4_t aHi = vdup_n_u16(0xF000U);
	uint16x4_t bHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kLoExpand)));
	uint16x4_t gHi = vreinterpret_u16_u8(vtbl1_u8(bg, vld1_u8(kHiExpand)));
	uint16x4_t rHi = vreinterpret_u16_u8(vtbl1_u8(rr, vld1_u8(kLoExpand)));
	uint16x4_t packed = vsri_n_u16(vsri_n_u16(vsri_n_u16(aHi, bHi, 4), gHi, 8), rHi, 12);

	vst1_u16(dst, packed);
}

static inline int16x4_t cscExpand2Chroma(uint16_t pair, int16x4_t chromaBias)
{
	static const uint8_t kDup2[8] = { 0, 32, 0, 32, 1, 32, 1, 32 };
	uint8x8_t raw = vreinterpret_u8_u32(vdup_n_u32((uint32_t)pair));
	return vadd_s16(vreinterpret_s16_u8(vtbl1_u8(raw, vld1_u8(kDup2))), chromaBias);
}

static inline int16x4_t cscBilinearChroma4(uint8x8_t curRow, uint8x8_t adjRow,
                                           uint8x8_t tblA, uint8x8_t tblB,
                                           int16x4_t chromaBias)
{
	int16x4_t wThree = vdup_n_s16(3);
	int16x4_t wNine = vdup_n_s16(9);
	int16x4_t nearCur = vreinterpret_s16_u8(vtbl1_u8(curRow, tblB));
	int16x4_t farCur = vreinterpret_s16_u8(vtbl1_u8(curRow, tblA));
	int16x4_t nearAdj = vreinterpret_s16_u8(vtbl1_u8(adjRow, tblB));
	int16x4_t farAdj = vreinterpret_s16_u8(vtbl1_u8(adjRow, tblA));
	int16x4_t sum = vmla_lane_s16(vmla_lane_s16(farAdj, vadd_s16(farCur, nearAdj), wThree, 0),
	                              nearCur, wNine, 0);
	return vadd_s16(vrshr_n_s16(sum, 4), chromaBias);
}

int sceJpegMjpegCsc(void *dstRgb, const void *srcYuv, uint32_t widthHeight,
                    int pitch, int pixelFormat, int yuvFormat)
{
	uint32_t width;
	uint32_t height;
	uint32_t lumaSize;
	uint32_t pitchDiff;
	int rangeIdx;
	int fmtBits;
	uint16_t yuvBase;
	const int16_t *coeffs;
	int16x4_t params;
	int16x4_t mat;
	uint16x4_t yOffset;
	int16x4_t chromaBias;
	int16x4_t yScale;
	int16x4_t alphaVec;

	if (dstRgb == NULL || srcYuv == NULL)
		return PSPEMU_CSC_ERR_INVALID_POINTER;
	if ((((uintptr_t)dstRgb | (uintptr_t)srcYuv) & 3U) != 0U)
		return PSPEMU_CSC_ERR_INVALID_POINTER;
	if (((uint32_t)pixelFormat & ~0x13U) != 0U)
		return PSPEMU_CSC_ERR_UNSUP_COLORSPACE;

	width = widthHeight >> 16;
	height = widthHeight & 0xFFFFU;
	rangeIdx = pixelFormat >> 4;
	fmtBits = pixelFormat & 3;
	lumaSize = width * height;
	pitchDiff = (uint32_t)pitch - width;

	if (width == 0U || height == 0U)
		return PSPEMU_CSC_ERR_UNSUP_IMAGE_SIZE;
	if ((width & 7U) != 0U || width > (uint32_t)pitch)
		return PSPEMU_CSC_ERR_UNSUP_IMAGE_SIZE;
	if (fmtBits != 0 && (pitchDiff & 1U) != 0U)
		return PSPEMU_CSC_ERR_UNSUP_IMAGE_SIZE;

	coeffs = g_jpegCscCoeffs[rangeIdx];
	params = vld1_s16(coeffs);
	mat = vld1_s16(coeffs + 4);
	yOffset = vreinterpret_u16_s16(vdup_lane_s16(params, 0));
	chromaBias = vdup_lane_s16(params, 1);
	yScale = vdup_lane_s16(params, 2);
	alphaVec = vdup_lane_s16(params, 3);
	yuvBase = (uint16_t)(yuvFormat & 0xFFFF);

	if (yuvBase == PSPEMU_CSC_YUV_422) {
		const uint8_t *yPlane = (const uint8_t *)srcYuv;
		const uint8_t *cbPlane = yPlane + lumaSize;
		const uint8_t *crPlane = cbPlane + (lumaSize >> 1);
		uint8_t *dst = (uint8_t *)dstRgb;
		uint32_t yRowStep = ((width - 1U) & ~7U) + 8U;
		uint32_t cRowStep = yRowStep >> 1;
		uint32_t row;

		if (fmtBits != 0)
			return PSPEMU_CSC_ERR_UNSUP_COLORSPACE;

		for (row = 0U; row < height; row++) {
			const uint8_t *yPtr = yPlane;
			const uint32_t *cbPtr = (const uint32_t *)cbPlane;
			const uint32_t *crPtr = (const uint32_t *)crPlane;
			uint32_t x;

			for (x = 0U; x < width; x += 8U) {
				uint16x8_t lumaWide = vmovl_u8(vld1_u8(yPtr));
				uint32_t cbWord = *cbPtr++;
				uint32_t crWord = *crPtr++;
				int32x4_t yLo = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_low_u16(lumaWide), yOffset)), yScale);
				int32x4_t yHi = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_high_u16(lumaWide), yOffset)), yScale);

				cscStore4Rgba(dst, yLo,
				              cscExpand2Chroma((uint16_t)cbWord, chromaBias),
				              cscExpand2Chroma((uint16_t)crWord, chromaBias),
				              mat, alphaVec);
				cscStore4Rgba(dst + 16, yHi,
				              cscExpand2Chroma((uint16_t)(cbWord >> 16), chromaBias),
				              cscExpand2Chroma((uint16_t)(crWord >> 16), chromaBias),
				              mat, alphaVec);
				yPtr += 8;
				dst += 32;
			}
			yPlane += yRowStep;
			cbPlane += cRowStep;
			crPlane += cRowStep;
			dst += pitchDiff * 4U;
		}
		return 0;
	}

	if (yuvBase == PSPEMU_CSC_YUV_444) {
		const uint8_t *yPlane = (const uint8_t *)srcYuv;
		const uint8_t *cbPlane = yPlane + lumaSize;
		const uint8_t *crPlane = cbPlane + lumaSize;
		uint8_t *dst = (uint8_t *)dstRgb;
		uint32_t rowStep = ((width - 1U) & ~7U) + 8U;
		uint32_t row;

		if (fmtBits != 0)
			return PSPEMU_CSC_ERR_UNSUP_COLORSPACE;

		for (row = 0U; row < height; row++) {
			const uint8_t *yPtr = yPlane;
			const uint8_t *cbPtr = cbPlane;
			const uint8_t *crPtr = crPlane;
			uint32_t x;

			for (x = 0U; x < width; x += 8U) {
				uint16x8_t lumaWide = vmovl_u8(vld1_u8(yPtr));
				uint16x8_t cbWide = vmovl_u8(vld1_u8(cbPtr));
				uint16x8_t crWide = vmovl_u8(vld1_u8(crPtr));
				int32x4_t yLo = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_low_u16(lumaWide), yOffset)), yScale);
				int32x4_t yHi = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_high_u16(lumaWide), yOffset)), yScale);

				cscStore4Rgba(dst, yLo,
				              vadd_s16(chromaBias, vreinterpret_s16_u16(vget_low_u16(cbWide))),
				              vadd_s16(chromaBias, vreinterpret_s16_u16(vget_low_u16(crWide))),
				              mat, alphaVec);
				cscStore4Rgba(dst + 16, yHi,
				              vadd_s16(chromaBias, vreinterpret_s16_u16(vget_high_u16(cbWide))),
				              vadd_s16(chromaBias, vreinterpret_s16_u16(vget_high_u16(crWide))),
				              mat, alphaVec);
				yPtr += 8;
				cbPtr += 8;
				crPtr += 8;
				dst += 32;
			}
			yPlane += rowStep;
			cbPlane += rowStep;
			crPlane += rowStep;
			dst += pitchDiff * 4U;
		}
		return 0;
	}

	if (yuvBase != PSPEMU_CSC_YUV_420)
		return PSPEMU_CSC_ERR_UNSUP_COLORSPACE;
	if ((height & 1U) != 0U)
		return PSPEMU_CSC_ERR_UNSUP_IMAGE_SIZE;

	if (((uint32_t)yuvFormat & PSPEMU_CSC_YUV_BILINEAR_FLAG) != 0U) {
		static const uint8_t kNearTbl[8] = { 2, 32, 2, 32, 3, 32, 3, 32 };
		static const uint8_t kFarLeftEdge[8] = { 2, 32, 3, 32, 2, 32, 4, 32 };
		static const uint8_t kFarNormal[8] = { 1, 32, 3, 32, 2, 32, 4, 32 };
		static const uint8_t kFarRightEdge[8] = { 1, 32, 3, 32, 2, 32, 3, 32 };
		const uint8_t *yPlane = (const uint8_t *)srcYuv;
		const uint8_t *cbPlane = yPlane + lumaSize;
		const uint8_t *crPlane = cbPlane + (lumaSize >> 2);
		uint8_t *dst = (uint8_t *)dstRgb;
		int cStride = (int)(width >> 1);
		uint8x8_t tblNear = vld1_u8(kNearTbl);
		uint32_t row;

		if (fmtBits != 0)
			return PSPEMU_CSC_ERR_UNSUP_COLORSPACE;

		for (row = 0U; row < height; row++) {
			int adjRowOff;
			const uint8_t *cbRow = cbPlane + (row >> 1) * (uint32_t)cStride;
			const uint8_t *crRow = crPlane + (row >> 1) * (uint32_t)cStride;
			const uint8_t *yPtr = yPlane + row * width;
			uint8x8_t tblLo = vld1_u8(kFarLeftEdge);
			uint32_t x;

			if ((row & 1U) == 0U)
				adjRowOff = (row == 0U) ? 0 : -cStride;
			else
				adjRowOff = (row + 1U >= height) ? 0 : cStride;

			for (x = 0U; x < width; x += 8U) {
				uint8x8_t tblHi = (x + 8U >= width) ? vld1_u8(kFarRightEdge) : vld1_u8(kFarNormal);
				const uint8_t *cbCur = cbRow + (x >> 1);
				const uint8_t *crCur = crRow + (x >> 1);
				uint64_t cbCurLo64 = *(const uint64_t *)(cbCur - 4) >> 16;
				uint64_t cbAdjLo64 = *(const uint64_t *)(cbCur + adjRowOff - 4) >> 16;
				uint64_t crCurLo64 = *(const uint64_t *)(crCur - 4) >> 16;
				uint64_t crAdjLo64 = *(const uint64_t *)(crCur + adjRowOff - 4) >> 16;
				uint8x8_t cbCurHi = vld1_u8(cbCur);
				uint8x8_t cbAdjHi = vld1_u8(cbCur + adjRowOff);
				uint8x8_t crCurHi = vld1_u8(crCur);
				uint8x8_t crAdjHi = vld1_u8(crCur + adjRowOff);
				uint16x8_t lumaWide = vmovl_u8(vld1_u8(yPtr));
				int32x4_t yLo = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_low_u16(lumaWide), yOffset)), yScale);
				int32x4_t yHi = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_high_u16(lumaWide), yOffset)), yScale);

				cscStore4Rgba(dst, yLo,
				              cscBilinearChroma4(vreinterpret_u8_u64(vcreate_u64(cbCurLo64)),
				                                 vreinterpret_u8_u64(vcreate_u64(cbAdjLo64)),
				                                 tblLo, tblNear, chromaBias),
				              cscBilinearChroma4(vreinterpret_u8_u64(vcreate_u64(crCurLo64)),
				                                 vreinterpret_u8_u64(vcreate_u64(crAdjLo64)),
				                                 tblLo, tblNear, chromaBias),
				              mat, alphaVec);
				cscStore4Rgba(dst + 16, yHi,
				              cscBilinearChroma4(cbCurHi, cbAdjHi, tblHi, tblNear, chromaBias),
				              cscBilinearChroma4(crCurHi, crAdjHi, tblHi, tblNear, chromaBias),
				              mat, alphaVec);
				tblLo = vld1_u8(kFarNormal);
				yPtr += 8;
				dst += 32;
			}
			dst += pitchDiff * 4U;
		}
		return 0;
	}

	/* YCbCr 4:2:0 box-filtered (nearest chroma) for RGBA8888 / RGB565 / RGBA5551 / RGBA4444 */
	{
		const uint8_t *yPlane = (const uint8_t *)srcYuv;
		const uint8_t *cbPlane = yPlane + lumaSize;
		const uint8_t *crPlane = cbPlane + (lumaSize >> 2);
		uint32_t yRowStep = ((width - 1U) & ~7U) + 8U;
		uint32_t cRowStep = yRowStep >> 1;
		uint32_t row;

		if (fmtBits == 0) {
			uint8_t *dst = (uint8_t *)dstRgb;
			if (pspemuCompatHasFlag(0x20000U) != 0)
				alphaVec = vdup_n_s16(0);

			for (row = 0U; row < height; row++) {
				const uint8_t *yPtr = yPlane + row * yRowStep;
				const uint32_t *cbPtr = (const uint32_t *)(cbPlane + (row >> 1) * cRowStep);
				const uint32_t *crPtr = (const uint32_t *)(crPlane + (row >> 1) * cRowStep);
				uint32_t x;

				for (x = 0U; x < width; x += 8U) {
					uint16x8_t lumaWide = vmovl_u8(vld1_u8(yPtr));
					uint32_t cbWord = *cbPtr++;
					uint32_t crWord = *crPtr++;
					int32x4_t yLo = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_low_u16(lumaWide), yOffset)), yScale);
					int32x4_t yHi = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_high_u16(lumaWide), yOffset)), yScale);

					cscStore4Rgba(dst, yLo,
					              cscExpand2Chroma((uint16_t)cbWord, chromaBias),
					              cscExpand2Chroma((uint16_t)crWord, chromaBias),
					              mat, alphaVec);
					cscStore4Rgba(dst + 16, yHi,
					              cscExpand2Chroma((uint16_t)(cbWord >> 16), chromaBias),
					              cscExpand2Chroma((uint16_t)(crWord >> 16), chromaBias),
					              mat, alphaVec);
					yPtr += 8;
					dst += 32;
				}
				dst += pitchDiff * 4U;
			}
		} else {
			uint16_t *dst = (uint16_t *)dstRgb;

			for (row = 0U; row < height; row++) {
				const uint8_t *yPtr = yPlane + row * yRowStep;
				const uint32_t *cbPtr = (const uint32_t *)(cbPlane + (row >> 1) * cRowStep);
				const uint32_t *crPtr = (const uint32_t *)(crPlane + (row >> 1) * cRowStep);
				uint32_t x;

				for (x = 0U; x < width; x += 8U) {
					uint16x8_t lumaWide = vmovl_u8(vld1_u8(yPtr));
					uint32_t cbWord = *cbPtr++;
					uint32_t crWord = *crPtr++;
					int32x4_t yLo = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_low_u16(lumaWide), yOffset)), yScale);
					int32x4_t yHi = vmull_s16(vreinterpret_s16_u16(vqsub_u16(vget_high_u16(lumaWide), yOffset)), yScale);
					int16x4_t cbLo = cscExpand2Chroma((uint16_t)cbWord, chromaBias);
					int16x4_t crLo = cscExpand2Chroma((uint16_t)crWord, chromaBias);
					int16x4_t cbHi = cscExpand2Chroma((uint16_t)(cbWord >> 16), chromaBias);
					int16x4_t crHi = cscExpand2Chroma((uint16_t)(crWord >> 16), chromaBias);

					if (fmtBits == 1) {
						cscStore4Rgb565(dst, yLo, cbLo, crLo, mat);
						cscStore4Rgb565(dst + 4, yHi, cbHi, crHi, mat);
					} else if (fmtBits == 2) {
						cscStore4Rgba5551(dst, yLo, cbLo, crLo, mat);
						cscStore4Rgba5551(dst + 4, yHi, cbHi, crHi, mat);
					} else {
						cscStore4Rgba4444(dst, yLo, cbLo, crLo, mat);
						cscStore4Rgba4444(dst + 4, yHi, cbHi, crHi, mat);
					}
					yPtr += 8;
					dst += 8;
				}
				dst += pitchDiff;
			}
		}
		return 0;
	}
}
