/*
 * SceJpegArm NEON inverse DCT (8x8, 4x4, 2x2) and YCbCr-to-RGBA color space conversion.
 */

#include "jpegarm_internal.h"

/*
 * Loeffler/AAN scaled 16-bit NEON IDCT constants:
 *   rot = { tan(pi/8), tan(pi/16), tan(3*pi/16), -tan(3*pi/16) } in Q15
 *   diag = { cos(pi/4), -cos(pi/4), 128, 0 }
 */
static const int16_t kIdctRot[4] = { 0x3505, 0x1976, 0x5587, (int16_t)0xAA79 };
static const int16_t kIdctDiag[4] = { 0x5A82, (int16_t)0xA57E, 0x0080, 0x0000 };

/* Prescale factors for 8x8 IDCT rows (left 4 columns and right 4 columns) */
static const int16_t kPrescale0L[4] = { 0x1000, 0x1631, 0x14E8, 0x12D0 };
static const int16_t kPrescale0R[4] = { 0x1000, 0x12D0, 0x14E8, 0x1631 };
static const int16_t kPrescale1L[4] = { 0x1631, 0x1EC8, 0x1CFF, 0x1A19 };
static const int16_t kPrescale1R[4] = { 0x1631, 0x1A19, 0x1CFF, 0x1EC8 };
static const int16_t kPrescale2L[4] = { 0x14E8, 0x1CFF, 0x1B50, 0x1895 };
static const int16_t kPrescale2R[4] = { 0x14E8, 0x1895, 0x1B50, 0x1CFF };
static const int16_t kPrescale3L[4] = { 0x12D0, 0x1A19, 0x1895, 0x161F };
static const int16_t kPrescale3R[4] = { 0x12D0, 0x161F, 0x1895, 0x1A19 };

/* Prescale factors for 4x4 IDCT */
static const int16_t kPrescale4x4Even[4] = { 0x1000, 0x14E8, 0x1000, 0x14E8 };
static const int16_t kPrescale4x4Odd[4]  = { 0x14E8, 0x1B50, 0x14E8, 0x1B50 };

/* JFIF YCbCr->RGBA conversion matrix in Q13 (with input chroma shifted left by 2):
 *   lane 0: +1.40200 (Cr -> R)
 *   lane 1: -0.34414 (Cb -> G)
 *   lane 2: -0.71414 (Cr -> G)
 *   lane 3: +1.77200 (Cb -> B)
 */
static const int16_t kCscMatrix[4] = { 0x2CDD, (int16_t)0xF4FE, (int16_t)0xE927, 0x38B4 };

static inline void transpose4x4_s16(int16x4_t *rowA, int16x4_t *rowB, int16x4_t *rowC, int16x4_t *rowD)
{
	int16x4x2_t termA = vtrn_s16(*rowA, *rowB);
	int16x4x2_t termB = vtrn_s16(*rowC, *rowD);
	int32x2x2_t quadA = vtrn_s32(vreinterpret_s32_s16(termA.val[0]), vreinterpret_s32_s16(termB.val[0]));
	int32x2x2_t quadB = vtrn_s32(vreinterpret_s32_s16(termA.val[1]), vreinterpret_s32_s16(termB.val[1]));

	*rowA = vreinterpret_s16_s32(quadA.val[0]);
	*rowB = vreinterpret_s16_s32(quadB.val[0]);
	*rowC = vreinterpret_s16_s32(quadA.val[1]);
	*rowD = vreinterpret_s16_s32(quadB.val[1]);
}

static inline void idct1d8(int16x4_t *valA, int16x4_t *valB, int16x4_t *valC, int16x4_t *valD,
	int16x4_t *valE, int16x4_t *valF, int16x4_t *valG, int16x4_t *valH)
{
	int16x4_t rot = vld1_s16(kIdctRot);
	int16x4_t diag = vld1_s16(kIdctDiag);

	/* Odd part */
	int16x4_t oddH = vqadd_s16(*valB, vqrdmulh_lane_s16(*valH, rot, 1));
	int16x4_t oddB = vqsub_s16(vqrdmulh_lane_s16(*valB, rot, 1), *valH);
	int16x4_t oddF = vqadd_s16(vqrdmulh_lane_s16(*valF, rot, 2), *valD);
	int16x4_t oddD = vqadd_s16(*valF, vqrdmulh_lane_s16(*valD, rot, 3));

	int16x4_t byteH = vqadd_s16(oddH, oddF);
	int16x4_t byteF = vqsub_s16(oddH, oddF);
	int16x4_t byteB = vqadd_s16(oddB, oddD);
	int16x4_t byteD = vqsub_s16(oddB, oddD);

	int16x4_t cosE = vqadd_s16(byteF, byteD);
	int16x4_t cosC = vqsub_s16(byteF, byteD);

	/* Even part */
	int16x4_t evenA = vqadd_s16(*valA, *valE);
	int16x4_t evenB = vqsub_s16(*valA, *valE);
	int16x4_t evenD = vqadd_s16(*valC, vqrdmulh_lane_s16(*valG, rot, 0));
	int16x4_t evenC = vqsub_s16(vqrdmulh_lane_s16(*valC, rot, 0), *valG);

	int16x4_t diffA = vqadd_s16(evenA, evenD);
	int16x4_t diffD = vqsub_s16(evenA, evenD);
	int16x4_t diffB = vqadd_s16(evenB, evenC);
	int16x4_t diffC = vqsub_s16(evenB, evenC);

	/* Final butterfly */
	*valA = vqadd_s16(diffA, byteH);
	*valH = vqsub_s16(diffA, byteH);
	*valB = vqadd_s16(vqrdmulh_lane_s16(cosE, diag, 0), diffB);
	*valG = vqadd_s16(vqrdmulh_lane_s16(cosE, diag, 1), diffB);
	*valC = vqadd_s16(vqrdmulh_lane_s16(cosC, diag, 0), diffC);
	*valF = vqadd_s16(vqrdmulh_lane_s16(cosC, diag, 1), diffC);
	*valD = vqadd_s16(diffD, byteB);
	*valE = vqsub_s16(diffD, byteB);
}

static inline void idct1d8Half(int16x4_t *valA, int16x4_t *valB, int16x4_t *valC, int16x4_t *valD,
	int16x4_t *valE, int16x4_t *valF, int16x4_t *valG, int16x4_t *valH)
{
	int16x4_t rot = vld1_s16(kIdctRot);
	int16x4_t diag = vld1_s16(kIdctDiag);

	/* Only rows 0..3 are non-zero (valE..valH are 0) */
	int16x4_t oddB = vqrdmulh_lane_s16(*valB, rot, 1);
	int16x4_t oddD = vqrdmulh_lane_s16(*valD, rot, 3);

	int16x4_t byteH = vqadd_s16(*valB, *valD);
	int16x4_t byteF = vqsub_s16(*valB, *valD);
	int16x4_t byteB = vqadd_s16(oddB, oddD);
	int16x4_t byteD = vqsub_s16(oddB, oddD);

	int16x4_t cosE = vqadd_s16(byteF, byteD);
	int16x4_t cosC = vqsub_s16(byteF, byteD);

	int16x4_t evenC = vqrdmulh_lane_s16(*valC, rot, 0);
	int16x4_t diffA = vqadd_s16(*valA, *valC);
	int16x4_t diffD = vqsub_s16(*valA, *valC);
	int16x4_t diffB = vqadd_s16(*valA, evenC);
	int16x4_t diffC = vqsub_s16(*valA, evenC);

	*valA = vqadd_s16(diffA, byteH);
	*valH = vqsub_s16(diffA, byteH);
	*valB = vqadd_s16(vqrdmulh_lane_s16(cosE, diag, 0), diffB);
	*valG = vqadd_s16(vqrdmulh_lane_s16(cosE, diag, 1), diffB);
	*valC = vqadd_s16(vqrdmulh_lane_s16(cosC, diag, 0), diffC);
	*valF = vqadd_s16(vqrdmulh_lane_s16(cosC, diag, 1), diffC);
	*valD = vqadd_s16(diffD, byteB);
	*valE = vqsub_s16(diffD, byteB);
}

static inline void storeIdctRow8(uint8_t *dst, int16x4_t lo, int16x4_t hi, int16x8_t bias128)
{
	int16x8_t row = vcombine_s16(lo, hi);
	uint8x8_t pix = vqmovun_s16(vqaddq_s16(row, bias128));
	vst1_u8(dst, pix);
}

void jpegArmIdct8x8(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride)
{
	int16x4_t colA = vqrdmulh_s16(vld1_s16(kPrescale0L), vmul_s16(vld1_s16(coef + 0),  vld1_s16(quant + 0)));
	int16x4_t rowA = vqrdmulh_s16(vld1_s16(kPrescale0R), vmul_s16(vld1_s16(coef + 4),  vld1_s16(quant + 4)));
	int16x4_t colB = vqrdmulh_s16(vld1_s16(kPrescale1L), vmul_s16(vld1_s16(coef + 8),  vld1_s16(quant + 8)));
	int16x4_t rowB = vqrdmulh_s16(vld1_s16(kPrescale1R), vmul_s16(vld1_s16(coef + 12), vld1_s16(quant + 12)));
	int16x4_t colC = vqrdmulh_s16(vld1_s16(kPrescale2L), vmul_s16(vld1_s16(coef + 16), vld1_s16(quant + 16)));
	int16x4_t rowC = vqrdmulh_s16(vld1_s16(kPrescale2R), vmul_s16(vld1_s16(coef + 20), vld1_s16(quant + 20)));
	int16x4_t colD = vqrdmulh_s16(vld1_s16(kPrescale3L), vmul_s16(vld1_s16(coef + 24), vld1_s16(quant + 24)));
	int16x4_t rowD = vqrdmulh_s16(vld1_s16(kPrescale3R), vmul_s16(vld1_s16(coef + 28), vld1_s16(quant + 28)));
	int16x4_t colE = vqrdmulh_s16(vld1_s16(kPrescale0L), vmul_s16(vld1_s16(coef + 32), vld1_s16(quant + 32)));
	int16x4_t rowE = vqrdmulh_s16(vld1_s16(kPrescale0R), vmul_s16(vld1_s16(coef + 36), vld1_s16(quant + 36)));
	int16x4_t colF = vqrdmulh_s16(vld1_s16(kPrescale3L), vmul_s16(vld1_s16(coef + 40), vld1_s16(quant + 40)));
	int16x4_t rowF = vqrdmulh_s16(vld1_s16(kPrescale3R), vmul_s16(vld1_s16(coef + 44), vld1_s16(quant + 44)));
	int16x4_t colG = vqrdmulh_s16(vld1_s16(kPrescale2L), vmul_s16(vld1_s16(coef + 48), vld1_s16(quant + 48)));
	int16x4_t rowG = vqrdmulh_s16(vld1_s16(kPrescale2R), vmul_s16(vld1_s16(coef + 52), vld1_s16(quant + 52)));
	int16x4_t colH = vqrdmulh_s16(vld1_s16(kPrescale1L), vmul_s16(vld1_s16(coef + 56), vld1_s16(quant + 56)));
	int16x4_t rowH = vqrdmulh_s16(vld1_s16(kPrescale1R), vmul_s16(vld1_s16(coef + 60), vld1_s16(quant + 60)));

	/* Column pass */
	idct1d8(&colA, &colB, &colC, &colD, &colE, &colF, &colG, &colH);
	idct1d8(&rowA, &rowB, &rowC, &rowD, &rowE, &rowF, &rowG, &rowH);

	/* Transpose 8x8 */
	transpose4x4_s16(&colA, &colB, &colC, &colD);
	transpose4x4_s16(&colE, &colF, &colG, &colH);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&rowE, &rowF, &rowG, &rowH);

	/* Row pass (top 4 rows in colA..colD + rowA..rowD, bottom 4 rows in colE..colH + rowE..rowH) */
	idct1d8(&colA, &colB, &colC, &colD, &rowA, &rowB, &rowC, &rowD);
	idct1d8(&colE, &colF, &colG, &colH, &rowE, &rowF, &rowG, &rowH);

	/* Transpose back to row-major */
	transpose4x4_s16(&colA, &colB, &colC, &colD);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&colE, &colF, &colG, &colH);
	transpose4x4_s16(&rowE, &rowF, &rowG, &rowH);

	int16x8_t bias128 = vdupq_n_s16(128);
	storeIdctRow8(out + 0 * stride, colA, rowA, bias128);
	storeIdctRow8(out + 1 * stride, colB, rowB, bias128);
	storeIdctRow8(out + 2 * stride, colC, rowC, bias128);
	storeIdctRow8(out + 3 * stride, colD, rowD, bias128);
	storeIdctRow8(out + 4 * stride, colE, rowE, bias128);
	storeIdctRow8(out + 5 * stride, colF, rowF, bias128);
	storeIdctRow8(out + 6 * stride, colG, rowG, bias128);
	storeIdctRow8(out + 7 * stride, colH, rowH, bias128);
}

void jpegArmIdct8x8Half(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride)
{
	int16x4_t colA = vqrdmulh_s16(vld1_s16(kPrescale0L), vmul_s16(vld1_s16(coef + 0),  vld1_s16(quant + 0)));
	int16x4_t rowA = vqrdmulh_s16(vld1_s16(kPrescale0R), vmul_s16(vld1_s16(coef + 4),  vld1_s16(quant + 4)));
	int16x4_t colB = vqrdmulh_s16(vld1_s16(kPrescale1L), vmul_s16(vld1_s16(coef + 8),  vld1_s16(quant + 8)));
	int16x4_t rowB = vqrdmulh_s16(vld1_s16(kPrescale1R), vmul_s16(vld1_s16(coef + 12), vld1_s16(quant + 12)));
	int16x4_t colC = vqrdmulh_s16(vld1_s16(kPrescale2L), vmul_s16(vld1_s16(coef + 16), vld1_s16(quant + 16)));
	int16x4_t rowC = vqrdmulh_s16(vld1_s16(kPrescale2R), vmul_s16(vld1_s16(coef + 20), vld1_s16(quant + 20)));
	int16x4_t colD = vqrdmulh_s16(vld1_s16(kPrescale3L), vmul_s16(vld1_s16(coef + 24), vld1_s16(quant + 24)));
	int16x4_t rowD = vqrdmulh_s16(vld1_s16(kPrescale3R), vmul_s16(vld1_s16(coef + 28), vld1_s16(quant + 28)));
	int16x4_t colE, colF, colG, colH;
	int16x4_t rowE, rowF, rowG, rowH;

	/* Column pass (bottom 4 rows are zero) */
	idct1d8Half(&colA, &colB, &colC, &colD, &colE, &colF, &colG, &colH);
	idct1d8Half(&rowA, &rowB, &rowC, &rowD, &rowE, &rowF, &rowG, &rowH);

	/* Transpose 8x8 */
	transpose4x4_s16(&colA, &colB, &colC, &colD);
	transpose4x4_s16(&colE, &colF, &colG, &colH);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&rowE, &rowF, &rowG, &rowH);

	/* Row pass */
	idct1d8(&colA, &colB, &colC, &colD, &rowA, &rowB, &rowC, &rowD);
	idct1d8(&colE, &colF, &colG, &colH, &rowE, &rowF, &rowG, &rowH);

	/* Transpose back */
	transpose4x4_s16(&colA, &colB, &colC, &colD);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&colE, &colF, &colG, &colH);
	transpose4x4_s16(&rowE, &rowF, &rowG, &rowH);

	int16x8_t bias128 = vdupq_n_s16(128);
	storeIdctRow8(out + 0 * stride, colA, rowA, bias128);
	storeIdctRow8(out + 1 * stride, colB, rowB, bias128);
	storeIdctRow8(out + 2 * stride, colC, rowC, bias128);
	storeIdctRow8(out + 3 * stride, colD, rowD, bias128);
	storeIdctRow8(out + 4 * stride, colE, rowE, bias128);
	storeIdctRow8(out + 5 * stride, colF, rowF, bias128);
	storeIdctRow8(out + 6 * stride, colG, rowG, bias128);
	storeIdctRow8(out + 7 * stride, colH, rowH, bias128);
}

static inline void idct1d4(int16x4_t *valA, int16x4_t *valB, int16x4_t *valC, int16x4_t *valD)
{
	int16x4_t tanPi8 = vdup_n_s16(0x3505);
	int16x4_t oddA = vqsub_s16(vqrdmulh_s16(tanPi8, *valB), *valD);
	int16x4_t oddB = vqadd_s16(vqrdmulh_s16(tanPi8, *valD), *valB);
	int16x4_t evenA = vqadd_s16(*valA, *valC);
	int16x4_t evenB = vqsub_s16(*valA, *valC);

	*valA = vqadd_s16(evenA, oddB);
	*valB = vqadd_s16(evenB, oddA);
	*valC = vqsub_s16(evenB, oddA);
	*valD = vqsub_s16(evenA, oddB);
}

void jpegArmIdct4x4(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride)
{
	int16x4_t sEven = vld1_s16(kPrescale4x4Even);
	int16x4_t sOdd  = vld1_s16(kPrescale4x4Odd);

	int16x4_t rowA = vqrdmulh_s16(sEven, vmul_s16(vld1_s16(coef + 0),  vld1_s16(quant + 0)));
	int16x4_t rowB = vqrdmulh_s16(sOdd,  vmul_s16(vld1_s16(coef + 8),  vld1_s16(quant + 8)));
	int16x4_t rowC = vqrdmulh_s16(sEven, vmul_s16(vld1_s16(coef + 16), vld1_s16(quant + 16)));
	int16x4_t rowD = vqrdmulh_s16(sOdd,  vmul_s16(vld1_s16(coef + 24), vld1_s16(quant + 24)));

	idct1d4(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);
	idct1d4(&rowA, &rowB, &rowC, &rowD);
	transpose4x4_s16(&rowA, &rowB, &rowC, &rowD);

	int16x8_t bias128 = vdupq_n_s16(128);
	uint8x8_t top = vqmovun_s16(vqaddq_s16(vcombine_s16(rowA, rowB), bias128));
	uint8x8_t bot = vqmovun_s16(vqaddq_s16(vcombine_s16(rowC, rowD), bias128));

	vst1_lane_u32((uint32_t *)(out + 0 * stride), vreinterpret_u32_u8(top), 0);
	vst1_lane_u32((uint32_t *)(out + 1 * stride), vreinterpret_u32_u8(top), 1);
	vst1_lane_u32((uint32_t *)(out + 2 * stride), vreinterpret_u32_u8(bot), 0);
	vst1_lane_u32((uint32_t *)(out + 3 * stride), vreinterpret_u32_u8(bot), 1);
}

void jpegArmIdct2x2(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride)
{
	int16x4_t rowA = vmul_s16(vld1_s16(coef + 0), vld1_s16(quant + 0));
	int16x4_t rowB = vmul_s16(vld1_s16(coef + 8), vld1_s16(quant + 8));

	int16x4x2_t termA = vtrn_s16(vqadd_s16(rowA, rowB), vqsub_s16(rowA, rowB));
	int16x4_t a = termA.val[0];
	int16x4_t b = termA.val[1];
	int16x4x2_t termB = vtrn_s16(vqadd_s16(a, b), vqsub_s16(a, b));

	int16x8_t bias = vdupq_n_s16(1024);
	int16x8_t combined = vcombine_s16(termB.val[0], termB.val[1]);
	uint8x8_t pix = vqrshrun_n_s16(vqaddq_s16(combined, bias), 3);

	vst1_lane_u16((uint16_t *)(out + 0 * stride), vreinterpret_u16_u8(pix), 0);
	vst1_lane_u16((uint16_t *)(out + 1 * stride), vreinterpret_u16_u8(pix), 2);
}

static inline uint8_t clampByte(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return (uint8_t)val;
}

void jpegArmDequantAndIdctBlock(JpegArmDecoder *dec, int skipOutput)
{
	int blockIdx = 0;
	JpegArmComponent *comp = dec->mcuCompPtrs[0];

	while (comp != NULL) {
		int16_t *srcCoef = comp->coefPtr;
		uint8_t *dstOut = comp->outPtr;
		int16_t *quant = comp->quantTable;

		if (skipOutput) {
			comp->coefPtr = srcCoef + (dec->coefBlockSteps[blockIdx] >> 1);
		} else {
			int16_t block[64];
			int maxOffset = 0;

			block[0] = srcCoef[0];
			for (int i = 0; i < 63; i++) {
				int byteOff = g_jpegArmZigzagByteOffset[i];
				int16_t coeff = srcCoef[i + 1];
				block[byteOff >> 1] = coeff;
				if (coeff != 0 && byteOff > maxOffset)
					maxOffset = byteOff;
			}

			comp->coefPtr = srcCoef + (dec->coefBlockSteps[blockIdx] >> 1);

			if (comp->downscale == 1) {
				if (maxOffset <= 63)
					jpegArmIdct8x8Half(block, quant, dstOut, (int)comp->outStride);
				else
					jpegArmIdct8x8(block, quant, dstOut, (int)comp->outStride);
			} else if (comp->downscale == 2) {
				jpegArmIdct4x4(block, quant, dstOut, (int)comp->outStride);
			} else if (comp->downscale == 4) {
				jpegArmIdct2x2(block, quant, dstOut, (int)comp->outStride);
			} else {
				int dc = (block[0] * quant[0] + 1024) >> 3;
				dstOut[0] = clampByte(dc);
			}
		}

		comp->outPtr = dstOut + dec->mcuBlockSteps[blockIdx];
		blockIdx++;
		comp = dec->mcuCompPtrs[blockIdx];
	}
}

int jpegArmIdctProgressiveRowYCbCr(JpegArmDecoder *dec)
{
	uint32_t activeCols = (dec->curMcuRow < (int)dec->regionStartY) ? 0U : (uint32_t)dec->regionWidthMcu;
	int col = dec->mcuColOffset;

	do {
		jpegArmDequantAndIdctBlock(dec, (uint32_t)col >= activeCols);
		col++;
	} while (col < (int)dec->compMcuCols - (int)dec->regionStartX);

	dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
	return 0;
}

static inline void cscStore4Rgba(uint8_t *dst, int remaining,
	int16x4_t yVec, int16x4_t cbVec, int16x4_t crVec, int16x4_t mat)
{
	int16x4_t minusHalfRange = vdup_n_s16(-128);
	int16x4_t cb = vshl_n_s16(vadd_s16(cbVec, minusHalfRange), 2);
	int16x4_t cr = vshl_n_s16(vadd_s16(crVec, minusHalfRange), 2);

	int16x4_t b = vqadd_s16(yVec, vqrdmulh_lane_s16(cb, mat, 3));
	int16x4_t g = vqadd_s16(vqadd_s16(yVec, vqrdmulh_lane_s16(cb, mat, 1)), vqrdmulh_lane_s16(cr, mat, 2));
	int16x4_t r = vqadd_s16(yVec, vqrdmulh_lane_s16(cr, mat, 0));
	int16x4_t a = vdup_n_s16(255);

	uint8x8_t bg = vqmovun_s16(vcombine_s16(b, g));
	uint8x8_t ra = vqmovun_s16(vcombine_s16(r, a));

	/* Interleave R, G, B, A for 4 pixels */
	uint8x8x2_t tbl;
	tbl.val[0] = bg;
	tbl.val[1] = ra;

	static const uint8_t kPosA[8] = { 8, 4, 0, 12, 9, 5, 1, 13 };
	static const uint8_t kPosB[8] = { 10, 6, 2, 14, 11, 7, 3, 15 };
	uint8x8_t pairAB = vtbl2_u8(tbl, vld1_u8(kPosA));
	uint8x8_t pairCD = vtbl2_u8(tbl, vld1_u8(kPosB));

	if (remaining >= 4) {
		vst1_u8(dst, pairAB);
		vst1_u8(dst + 8, pairCD);
	} else {
		uint32_t *out32 = (uint32_t *)dst;
		out32[0] = vget_lane_u32(vreinterpret_u32_u8(pairAB), 0);
		if (remaining >= 2) {
			out32[1] = vget_lane_u32(vreinterpret_u32_u8(pairAB), 1);
			if (remaining == 3)
				out32[2] = vget_lane_u32(vreinterpret_u32_u8(pairCD), 0);
		}
	}
}

void jpegArmCscYCbCrH4ToRgba(JpegArmDecoder *dec)
{
	int width = (int)dec->mcuPixelWidth;
	int curX = (int)dec->rgbaCurX;
	int totalWidth = (int)dec->rgbaWidth;

	if (curX + width > totalWidth) {
		width = totalWidth - curX;
		if (width <= 0)
			return;
	}

	uint32_t yStride = dec->components[0].outStride;
	uint32_t cStride = dec->components[1].outStride;
	uint8_t *dstRow = dec->rgbaCurPtr;
	int rowCount = dec->rgbaRowHeight;
	uint8_t vMask = dec->vSampMask;
	const uint8_t *yRow = dec->components[0].outPtr;
	const uint8_t *cbRow = dec->components[1].outPtr;
	const uint8_t *crRow = dec->components[2].outPtr;
	int16x4_t mat = vld1_s16(kCscMatrix);

	for (int row = 0; row < rowCount; row++) {
		int16x4_t yHi = vdup_n_s16(0);
		int cIdx = 0;
		uint8_t *dst = dstRow;

		for (int x = 0; x < width; x += 4, cIdx++, dst += 16) {
			int16x4_t yLo;
			if ((x & 4) == 0) {
				uint16x8_t luma16 = vmovl_u8(vld1_u8(yRow + x));
				yLo = vreinterpret_s16_u16(vget_low_u16(luma16));
				yHi = vreinterpret_s16_u16(vget_high_u16(luma16));
			} else {
				yLo = yHi;
			}

			int16x4_t cb = vdup_n_s16((int16_t)cbRow[cIdx]);
			int16x4_t cr = vdup_n_s16((int16_t)crRow[cIdx]);
			cscStore4Rgba(dst, width - x, yLo, cb, cr, mat);
		}

		yRow += yStride;
		if ((row & vMask) == vMask) {
			cbRow += cStride;
			crRow += cStride;
		}
		dstRow += 4 * dec->rgbaWidth;
	}

	dec->rgbaCurX += (uint32_t)width;
	dec->rgbaCurPtr += 4 * width;
}

void jpegArmCscYCbCrH2ToRgba(JpegArmDecoder *dec)
{
	int width = (int)dec->mcuPixelWidth;
	int curX = (int)dec->rgbaCurX;
	int totalWidth = (int)dec->rgbaWidth;

	if (curX + width > totalWidth) {
		width = totalWidth - curX;
		if (width <= 0)
			return;
	}

	uint32_t yStride = dec->components[0].outStride;
	uint32_t cStride = dec->components[1].outStride;
	uint8_t *dstRow = dec->rgbaCurPtr;
	int rowCount = dec->rgbaRowHeight;
	uint8_t vMask = dec->vSampMask;
	const uint8_t *yRow = dec->components[0].outPtr;
	const uint8_t *cbRow = dec->components[1].outPtr;
	const uint8_t *crRow = dec->components[2].outPtr;
	int16x4_t mat = vld1_s16(kCscMatrix);

	for (int row = 0; row < rowCount; row++) {
		int16x4_t yHi = vdup_n_s16(0);
		int cIdx = 0;
		uint8_t *dst = dstRow;

		for (int x = 0; x < width; x += 4, cIdx += 2, dst += 16) {
			int16x4_t yLo;
			if ((x & 4) == 0) {
				uint16x8_t luma16 = vmovl_u8(vld1_u8(yRow + x));
				yLo = vreinterpret_s16_u16(vget_low_u16(luma16));
				yHi = vreinterpret_s16_u16(vget_high_u16(luma16));
			} else {
				yLo = yHi;
			}

			int16_t cbVals[4] = { cbRow[cIdx], cbRow[cIdx], cbRow[cIdx + 1], cbRow[cIdx + 1] };
			int16_t crVals[4] = { crRow[cIdx], crRow[cIdx], crRow[cIdx + 1], crRow[cIdx + 1] };
			cscStore4Rgba(dst, width - x, yLo, vld1_s16(cbVals), vld1_s16(crVals), mat);
		}

		yRow += yStride;
		if ((row & vMask) == vMask) {
			cbRow += cStride;
			crRow += cStride;
		}
		dstRow += 4 * dec->rgbaWidth;
	}

	dec->rgbaCurX += (uint32_t)width;
	dec->rgbaCurPtr += 4 * width;
}

void jpegArmCscYCbCrH1ToRgba(JpegArmDecoder *dec)
{
	int width = (int)dec->mcuPixelWidth;
	int curX = (int)dec->rgbaCurX;
	int totalWidth = (int)dec->rgbaWidth;

	if (curX + width > totalWidth) {
		width = totalWidth - curX;
		if (width <= 0)
			return;
	}

	uint32_t yStride = dec->components[0].outStride;
	uint32_t cStride = dec->components[1].outStride;
	uint8_t *dstRow = dec->rgbaCurPtr;
	int rowCount = dec->rgbaRowHeight;
	uint8_t vMask = dec->vSampMask;
	const uint8_t *yRow = dec->components[0].outPtr;
	const uint8_t *cbRow = dec->components[1].outPtr;
	const uint8_t *crRow = dec->components[2].outPtr;
	int16x4_t mat = vld1_s16(kCscMatrix);

	for (int row = 0; row < rowCount; row++) {
		int16x4_t yHi = vdup_n_s16(0);
		int16x4_t cbHi = vdup_n_s16(0);
		int16x4_t crHi = vdup_n_s16(0);
		uint8_t *dst = dstRow;

		for (int x = 0; x < width; x += 4, dst += 16) {
			int16x4_t yLo, cbLo, crLo;
			if ((x & 4) == 0) {
				uint16x8_t luma16  = vmovl_u8(vld1_u8(yRow + x));
				uint16x8_t cb16 = vmovl_u8(vld1_u8(cbRow + x));
				uint16x8_t cr16 = vmovl_u8(vld1_u8(crRow + x));
				yLo  = vreinterpret_s16_u16(vget_low_u16(luma16));
				yHi  = vreinterpret_s16_u16(vget_high_u16(luma16));
				cbLo = vreinterpret_s16_u16(vget_low_u16(cb16));
				cbHi = vreinterpret_s16_u16(vget_high_u16(cb16));
				crLo = vreinterpret_s16_u16(vget_low_u16(cr16));
				crHi = vreinterpret_s16_u16(vget_high_u16(cr16));
			} else {
				yLo = yHi;
				cbLo = cbHi;
				crLo = crHi;
			}

			cscStore4Rgba(dst, width - x, yLo, cbLo, crLo, mat);
		}

		yRow += yStride;
		if ((row & vMask) == vMask) {
			cbRow += cStride;
			crRow += cStride;
		}
		dstRow += 4 * dec->rgbaWidth;
	}

	dec->rgbaCurX += (uint32_t)width;
	dec->rgbaCurPtr += 4 * width;
}

int jpegArmIdctProgressiveRowRgba(JpegArmDecoder *dec)
{
	uint8_t mcuBuf[3076];
	void (*cscFn)(JpegArmDecoder *);
	uint32_t activeCols = (dec->curMcuRow < (int)dec->regionStartY) ? 0U : (uint32_t)dec->regionWidthMcu;
	int col = dec->mcuColOffset;

	if (dec->hSampMask == 0)
		cscFn = jpegArmCscYCbCrH1ToRgba;
	else if (dec->hSampMask == 1)
		cscFn = jpegArmCscYCbCrH2ToRgba;
	else
		cscFn = jpegArmCscYCbCrH4ToRgba;

	uint8_t *plane0 = mcuBuf;
	uint8_t *plane1 = plane0 + dec->components[0].outStride * dec->components[0].outHeight;
	uint8_t *plane2 = plane1 + dec->components[1].outStride * dec->components[1].outHeight;
	uint8_t *plane3 = plane2 + dec->components[2].outStride * dec->components[2].outHeight;

	dec->components[0].outPtr = plane0;
	dec->components[1].outPtr = plane1;
	dec->components[2].outPtr = plane2;
	dec->components[3].outPtr = plane3;

	uint32_t rowHeight = dec->components[0].outHeight;
	if (dec->rgbaCurY + rowHeight > dec->rgbaHeight)
		rowHeight = dec->rgbaHeight - dec->rgbaCurY;
	dec->rgbaRowHeight = (int16_t)rowHeight;

	if (col + (int)dec->regionStartX == 0) {
		dec->rgbaCurPtr = dec->rgbaOutPtr;
		dec->rgbaCurX = 0;
	}

	do {
		jpegArmDequantAndIdctBlock(dec, (uint32_t)col >= activeCols);
		dec->components[0].outPtr = plane0;
		dec->components[1].outPtr = plane1;
		dec->components[2].outPtr = plane2;
		dec->components[3].outPtr = plane3;
		if (dec->rgbaRowHeight > 0)
			cscFn(dec);
		col++;
	} while (col < (int)dec->mcuCols - (int)dec->regionStartX);

	dec->rgbaCurY += dec->components[0].outHeight;
	dec->rgbaOutPtr += dec->rgbaWidth * 4 * dec->components[0].outHeight;
	dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
	return 0;
}
