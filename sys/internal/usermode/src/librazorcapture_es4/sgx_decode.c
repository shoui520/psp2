/*
 * SceRazorCapture: SGX543 TAG/ISP state, texture mipmap/cubemap layout,
 * PDS program scanner & interpreter, and register bank helpers.
 */

#include "razor_capture_internal.h"

/* --- Utility helpers --- */

uint32_t razorFloatToBits(float f)
{
	union {
		float f;
		uint32_t u;
	} u;
	u.f = f;
	return u.u;
}

uint32_t razorNextPow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1U;
}

void *razorMemcpy(void *dst, const void *src, SceSize n)
{
	return sceClibMemcpy_safe(dst, src, n);
}

void *razorMemset(void *dst, int c, SceSize n)
{
	return sceClibMemset(dst, c, n);
}

void *razorZeroInit(void *dst)
{
	*(uint32_t *)dst = 0;
	return dst;
}

/* --- SGX TAG / ISP state unpack and merge --- */

void sgxTagStateInit(void *tagState, const uint32_t *saRegs)
{
	uint32_t *dst = (uint32_t *)tagState;
	const uint32_t *p = saRegs;
	uint32_t mask = *p++;

	dst[0] = mask;
	if (mask & 0x0001U)
		dst[1] = *p++;
	if (mask & 0x0002U)
		dst[2] = *p++;
	if (mask & 0x0004U)
		dst[3] = *p++;
	if (mask & 0x0008U)
		dst[4] = *p++;
	if (mask & 0x0010U)
		dst[5] = *p++;
	if (mask & 0x0020U)
		dst[6] = *p++;
	if (mask & 0x0040U) {
		dst[7] = *p++;
		dst[8] = *p++;
		dst[9] = *p++;
	}
	if (mask & 0x0080U) {
		dst[10] = *p++;
		dst[11] = *p++;
	}
	if (mask & 0x0100U) {
		dst[12] = *p++;
		dst[13] = *p++;
		dst[14] = *p++;
		dst[15] = *p++;
		dst[16] = *p++;
		dst[17] = *p++;
	}
	if (mask & 0x0200U)
		dst[18] = *p++;
	if (mask & 0x0400U)
		dst[19] = *p++;
	if (mask & 0x0800U)
		dst[20] = *p++;
	if (mask & 0x1000U)
		dst[21] = *p++;
	if (mask & 0x2000U)
		dst[22] = *p++;
	if (mask & 0x4000U)
		dst[23] = *p++;
	if (mask & 0x8000U)
		dst[24] = *p++;
	dst[25] = (uint32_t)(p - saRegs);
}

void sgxTagStateMerge(void *srcVoid, void *dstVoid)
{
	const uint32_t *src = (const uint32_t *)srcVoid;
	uint32_t *dst = (uint32_t *)dstVoid;
	uint32_t mask = src[0];

	if (mask & 0x0001U) {
		dst[1] = src[1];
		dst[0] |= 0x0001U;
	}
	if (mask & 0x0002U) {
		dst[2] = src[2];
		dst[0] |= 0x0002U;
	}
	if (mask & 0x0004U) {
		dst[3] = src[3];
		dst[0] |= 0x0004U;
	}
	if (mask & 0x0008U) {
		dst[4] = src[4];
		dst[0] |= 0x0008U;
	}
	if (mask & 0x0010U) {
		dst[5] = src[5];
		dst[0] |= 0x0010U;
	}
	if (mask & 0x0020U) {
		dst[6] = src[6];
		dst[0] |= 0x0020U;
	}
	if (mask & 0x0040U) {
		dst[7] = src[7];
		dst[8] = src[8];
		dst[9] = src[9];
		dst[0] |= 0x0040U;
	}
	if (mask & 0x0080U) {
		dst[10] = src[10];
		dst[11] = src[11];
		dst[0] |= 0x0080U;
	}
	if (mask & 0x0100U) {
		dst[12] = src[12];
		dst[13] = src[13];
		dst[14] = src[14];
		dst[15] = src[15];
		dst[16] = src[16];
		dst[17] = src[17];
		dst[0] |= 0x0100U;
	}
	if (mask & 0x0200U) {
		dst[18] = src[18];
		dst[0] |= 0x0200U;
	}
	if (mask & 0x0400U) {
		dst[19] = src[19];
		dst[0] |= 0x0400U;
	}
	if (mask & 0x0800U) {
		dst[20] = src[20];
		dst[0] |= 0x0800U;
	}
	if (mask & 0x1000U) {
		dst[21] = src[21];
		dst[0] |= 0x1000U;
	}
	if (mask & 0x2000U) {
		dst[22] = src[22];
		dst[0] |= 0x2000U;
	}
	if (mask & 0x4000U) {
		dst[23] = src[23];
		dst[0] |= 0x4000U;
	}
	if (mask & 0x8000U) {
		dst[24] = src[24];
		dst[0] |= 0x8000U;
	}
}

/* --- PDS DMA descriptor helpers --- */

uint32_t sgxDmaByteSize(const uint32_t *dmaEntry)
{
	return 4U * ((dmaEntry[1] & 0xFFU) + 1U);
}

int sgxDmaFindReg(const uint32_t *dmaEntry, uint32_t regIdx, uint32_t *outOffset)
{
	uint32_t ctrl = dmaEntry[1];
	uint32_t base = (ctrl >> 8) & 0xFFFU;
	uint32_t diff;

	if (regIdx < base)
		return 0;
	diff = regIdx - base;
	if (diff > (ctrl & 0xFFU))
		return 0;
	*outOffset = diff;
	return 1;
}

/* --- SGX texture layout / size calculators --- */

static inline __attribute__((always_inline)) uint32_t sgxTexWidth(uint32_t w1)
{
	switch (w1 & 0xE0000000U) {
	case 0x00000000U:
	case 0x40000000U:
		return 1U << ((w1 >> 16) & 0xFU);
	case 0x60000000U:
	case 0x80000000U:
	case 0xA0000000U:
	case 0xC0000000U:
	case 0xE0000000U:
		return ((w1 >> 12) & 0xFFFU) + 1U;
	default:
		return 0U;
	}
}

static inline __attribute__((always_inline)) uint32_t sgxTexHeight(uint32_t w1)
{
	switch (w1 & 0xE0000000U) {
	case 0x00000000U:
	case 0x40000000U:
		return 1U << (w1 & 0xFU);
	case 0x60000000U:
	case 0x80000000U:
	case 0xA0000000U:
	case 0xC0000000U:
	case 0xE0000000U:
		return (w1 & 0xFFFU) + 1U;
	default:
		return 0U;
	}
}

int sgxCalcTexLevelSize(const void *texWordsVoid, uint32_t mipCount, void *outLevelsVoid, uint32_t *outTotal)
{
	const uint32_t *tw = (const uint32_t *)texWordsVoid;
	uint32_t *levels = (uint32_t *)outLevelsVoid;
	const uint8_t *fmtInfo;
	uint32_t w0 = tw[0];
	uint32_t w1 = tw[1];
	uint32_t typeBits = w1 & 0xE0000000U;
	uint32_t height, width, bpp, layoutType;
	uint32_t potW, potH, minW, minH;
	uint32_t offset = 0;
	int useMipStride;
	uint32_t i;

	if (levels != NULL)
		sceClibMemset(levels, 0, 36U * mipCount);

	fmtInfo = (const uint8_t *)sgxLookupTexFormat(w1 & 0x1F000000U, (int)(w0 >> 31));
	useMipStride = (typeBits != 0xC0000000U) && (((w0 >> 17) & 0xFU) != 15U);
	height = sgxTexHeight(w1);
	width = sgxTexWidth(w1);
	bpp = *(const uint16_t *)(fmtInfo + 4);

	switch (typeBits) {
	case 0x00000000U: layoutType = 0; break;
	case 0x40000000U: layoutType = 1; break;
	case 0x60000000U: layoutType = 2; break;
	case 0x80000000U: layoutType = 3; break;
	case 0xC0000000U: layoutType = 4; break;
	case 0xA0000000U: layoutType = 5; break;
	case 0xE0000000U: layoutType = 6; break;
	default: layoutType = 7; break;
	}

	potW = razorNextPow2(width);
	potH = razorNextPow2(height);
	minH = fmtInfo[10] & 0xFU;
	minW = fmtInfo[10] >> 4;

	if ((sgxIsSwizzledTexType((int)((w0 >> 6) & 7U)) ||
	     sgxIsSwizzledTexType((int)((w0 >> 3) & 7U))) &&
	    ((fmtInfo[6] & 1U) == 0U)) {
		uint32_t logW = 0, logH = 0, maxLog, t;
		offset = (52U * bpp) >> 3;
		for (t = width >> 1; t != 0U; t >>= 1)
			logW++;
		for (t = height >> 1; t != 0U; t >>= 1)
			logH++;
		maxLog = (logH >= logW) ? logH : logW;
		for (i = maxLog; ; i--) {
			uint32_t dim = (height >> i >= width >> i) ? (height >> i) : (width >> i);
			uint32_t borderBytes = (4U * bpp * dim) >> 3;
			if (levels != NULL && i < mipCount) {
				levels[9 * i + 7] = offset;
				levels[9 * i + 8] = borderBytes;
			}
			offset += borderBytes;
			if (i == 0U)
				break;
		}
	}

	if (mipCount != 0U) {
		uint32_t lastSize = 0;
		for (i = 0; i < mipCount; i++) {
			int yuvSz1 = 0, yuvSz2 = 0;
			uint32_t strideW = 0, strideH = 0, pitch = 0, levelSize = 0, stepSize = 0;

			switch (layoutType) {
			case 0:
			case 1: {
				uint32_t cw = (width >= minW) ? width : minW;
				uint32_t ch = (height >= minH) ? height : minH;
				strideW = cw;
				strideH = ch;
				pitch = (bpp * cw) >> 3;
				levelSize = (bpp * ch * cw + 7U) >> 3;
				stepSize = levelSize;
				break;
			}
			case 2:
				if (fmtInfo[9] != 0U) {
					sgxCalcYuvLayout(width, height, potW, potH, (uint8_t)useMipStride,
							 fmtInfo[9], NULL, &yuvSz1, &yuvSz2);
					levelSize = (uint32_t)yuvSz1;
					stepSize = (uint32_t)yuvSz2;
				} else {
					strideW = (width + 7U) & ~7U;
					strideH = potH;
					pitch = (bpp * strideW) >> 3;
					levelSize = height * pitch;
					stepSize = (((potW + 7U) & ~7U) * bpp * potH) >> 3;
				}
				break;
			case 3: {
				uint32_t tw32 = (potW < 32U) ? 32U : potW;
				uint32_t th32 = (potH < 32U) ? 32U : potH;
				pitch = (bpp * ((width + 31U) & ~31U)) >> 3;
				levelSize = ((height + 31U) & ~31U) * pitch;
				stepSize = (bpp * tw32 * th32) >> 3;
				break;
			}
			case 4:
				pitch = 4U * (((((w0 >> 1) & 3U) << 13) |
					       ((((w0 >> 17) & 0x3FFU) << 3)) |
					       ((w0 >> 9) & 7U)) + 1U);
				levelSize = pitch * height;
				stepSize = levelSize;
				break;
			case 5:
			case 6: {
				uint32_t cw = razorNextPow2((width >= minW) ? width : minW);
				uint32_t ch = razorNextPow2((height >= minH) ? height : minH);
				strideW = cw;
				strideH = ch;
				pitch = (bpp * cw) >> 3;
				levelSize = (bpp * ch * cw + 7U) >> 3;
				stepSize = levelSize;
				break;
			}
			default:
				break;
			}

			if (levels != NULL) {
				levels[9 * i + 0] = potW;
				levels[9 * i + 1] = potH;
				levels[9 * i + 2] = strideW;
				levels[9 * i + 3] = strideH;
				levels[9 * i + 4] = offset;
				levels[9 * i + 5] = levelSize;
				levels[9 * i + 6] = pitch;
			}

			lastSize = levelSize;
			width >>= 1;
			height >>= 1;
			potW >>= 1;
			potH >>= 1;
			if (i + 1U < mipCount)
				offset += stepSize;
		}
		offset += lastSize;
	}

	if (outTotal != NULL)
		*outTotal = offset;
	return 0;
}

int sgxTexGetPalette(const void *texWordsVoid, uint32_t *outGpuAddr, int *outEntryCount)
{
	const uint32_t *tw = (const uint32_t *)texWordsVoid;
	const uint8_t *fmtInfo;
	uint8_t palScale;

	*outGpuAddr = 0;
	*outEntryCount = 0;
	fmtInfo = (const uint8_t *)sgxLookupTexFormat(tw[1] & 0x1F000000U, (int)(tw[0] >> 31));
	palScale = fmtInfo[11];
	if (palScale != 0U) {
		*outEntryCount = 16 * (int)palScale;
		*outGpuAddr = tw[3] << 6;
		return 1;
	}
	return 0;
}

uint32_t sgxTexMaxMipLevels(const void *texWordsVoid)
{
	const uint32_t *tw = (const uint32_t *)texWordsVoid;
	uint32_t w1 = tw[1];
	uint32_t h = sgxTexHeight(w1);
	uint32_t w = sgxTexWidth(w1);
	uint32_t maxDim = (h >= w) ? h : w;
	uint32_t levels = 0;

	while (maxDim != 0U) {
		maxDim >>= 1;
		levels++;
	}
	return levels;
}

uint32_t sgxCalcCubemapLayout(const void *texWordsVoid, uint32_t *outFaceOffsets, uint32_t *outTotal)
{
	const uint32_t *tw = (const uint32_t *)texWordsVoid;
	uint32_t w0 = tw[0];
	uint32_t w1 = tw[1];
	uint32_t typeBits = w1 & 0xE0000000U;
	uint32_t w = sgxTexWidth(w1);
	uint32_t h = sgxTexHeight(w1);
	uint32_t mipCount;
	const uint8_t *fmtInfo;
	int alignedFaces = 0;
	uint32_t total = 0;

	if (typeBits == 0xC0000000U) {
		mipCount = 1U;
	} else {
		uint32_t m = (w0 >> 17) & 0xFU;
		mipCount = (m == 15U) ? 1U : (m + 1U);
	}

	fmtInfo = (const uint8_t *)sgxLookupTexFormat(w1 & 0x1F000000U, (int)(w0 >> 31));
	switch (fmtInfo[8]) {
	case 0: alignedFaces = 1; break;
	case 1:
	case 2: alignedFaces = (w > 16U || h > 16U); break;
	case 3:
	case 4: alignedFaces = (w > 8U || h > 8U); break;
	case 5: alignedFaces = (w > 4U || h > 4U); break;
	default: alignedFaces = 0; break;
	}

	if (typeBits == 0xC0000000U || ((w0 >> 17) & 0xFU) == 15U || !alignedFaces) {
		uint32_t faceSize = 0;
		sgxCalcTexLevelSize(tw, mipCount, NULL, &faceSize);
		total = 6U * faceSize;
		if (outFaceOffsets != NULL) {
			outFaceOffsets[0] = 0U;
			outFaceOffsets[1] = faceSize;
			outFaceOffsets[2] = 2U * faceSize;
			outFaceOffsets[3] = 3U * faceSize;
			outFaceOffsets[4] = 4U * faceSize;
			outFaceOffsets[5] = 5U * faceSize;
		}
	} else {
		uint32_t maxLevels = sgxTexMaxMipLevels(tw);
		uint32_t faceSize = 0;
		uint32_t f1, f2, f3, f4, f5;

		sgxCalcTexLevelSize(tw, maxLevels, NULL, &faceSize);
		f1 = (faceSize + 2047U) & ~2047U;
		f2 = (faceSize + 2047U + f1) & ~2047U;
		f3 = (faceSize + 2047U + f2) & ~2047U;
		f4 = (faceSize + 2047U + f3) & ~2047U;
		f5 = (faceSize + 2047U + f4) & ~2047U;
		total = f5 + faceSize;
		if (outFaceOffsets != NULL) {
			outFaceOffsets[0] = 0U;
			outFaceOffsets[1] = f1;
			outFaceOffsets[2] = f2;
			outFaceOffsets[3] = f3;
			outFaceOffsets[4] = f4;
			outFaceOffsets[5] = f5;
		}
	}

	if (outTotal != NULL)
		*outTotal = total;
	return total;
}

uint32_t sgxCalcTexByteSize(const void *texWordsVoid)
{
	const uint32_t *tw = (const uint32_t *)texWordsVoid;
	uint32_t typeBits = tw[1] & 0xE0000000U;
	uint32_t mipCount;
	uint32_t size = 0;

	if (typeBits == 0xC0000000U || ((tw[0] >> 17) & 0xFU) == 15U)
		mipCount = 1U;
	else
		mipCount = ((tw[0] >> 17) & 0xFU) + 1U;

	sgxCalcTexLevelSize(tw, mipCount, NULL, &size);

	if (typeBits == 0x40000000U || typeBits == 0xE0000000U) {
		if (((tw[0] >> 17) & 0xFU) != 15U)
			sgxCalcCubemapLayout(tw, NULL, &size);
		else
			size *= 6U;
	}
	return size;
}

/* --- PDS program scanner --- */

SgxPdsProgram *sgxPdsProgramInit(SgxPdsProgram *prog)
{
	prog->codePtr = NULL;
	prog->gpuAddr = 0;
	prog->dataSize = 0;
	prog->codeWords = 0;
	return prog;
}

int sgxPdsProgramScan(SgxPdsProgram *prog, uint32_t *codePtr, uint32_t gpuAddr, uint32_t dataSize, int maxBytes)
{
	uint32_t maxBranchTarget = 0;
	uint32_t *cur = (uint32_t *)((uintptr_t)codePtr + (dataSize & ~3U));
	uint32_t *end = (uint32_t *)((uintptr_t)codePtr + ((uint32_t)(maxBytes & ~3)));
	uint32_t count = 0;

	prog->codePtr = codePtr;
	prog->gpuAddr = gpuAddr;
	prog->dataSize = dataSize;

	while (cur < end) {
		uint32_t word = *cur++;
		count++;
		if ((word >> 30) == 2U) {
			uint32_t op = (word >> 27) & 7U;
			if (op == 2U || op == 3U) {
				uint32_t target = word & 0x7FFFFU;
				if (target > maxBranchTarget)
					maxBranchTarget = target;
			} else if (op == 5U && count > maxBranchTarget) {
				prog->codeWords = count;
				return 1;
			}
		}
	}

	prog->codeWords = (uint32_t)(((intptr_t)cur - (intptr_t)codePtr) >> 2);
	return 0;
}

uint32_t sgxPdsProgramByteSize(const SgxPdsProgram *prog)
{
	return prog->dataSize + 4U * prog->codeWords;
}

/* --- PDS VM --- */

typedef struct SgxPdsVm {
	const SgxPdsProgram *program; /* +0 */
	void *output;                 /* +4 */
	void *callback;               /* +8 */
	int32_t flags;                /* +12 */
	uint32_t ir0;                 /* +16 */
	uint32_t ir1;                 /* +20 */
	uint32_t doutu;               /* +24 */
	uint32_t reserved;            /* +28 */
	uint32_t tempRegs[64];        /* +32 (8..71) */
	uint32_t constRegs[64];       /* +288 (72..135) */
} SgxPdsVm;

typedef struct SgxPdsOutput {
	uint32_t douttCount;          /* +0 */
	uint32_t douttPad;            /* +4 */
	uint32_t doutt[64];           /* +8 .. +263 */
	uint32_t doutaCount;          /* +264 */
	uint32_t doutaPad;            /* +268 */
	uint32_t douta[4][4];         /* +272 .. +335 */
	uint32_t doutdCount;          /* +336 */
	uint32_t doutdPad;            /* +340 */
	uint32_t doutd[50][2];        /* +344 .. +743 */
	uint32_t doutuCount;          /* +744 */
	uint32_t doutuPad;            /* +748 */
	uint32_t doutu[32][4];        /* +752 .. +1263 */
	uint32_t doutcCount;          /* +1264 */
	uint32_t doutcPad;            /* +1268 */
	uint32_t doutc[6][2];         /* +1272 .. +1319 */
	uint32_t dmaCpuPtrs[50];      /* +1320 .. +1519 */
} SgxPdsOutput;

typedef int (*SgxPdsOpFn)(SgxPdsVm *vm, const uint32_t *insn);

static int sgxPdsOpUnsupported1(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported2(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported3(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported4(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported5(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported6(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }
static int sgxPdsOpUnsupported7(SgxPdsVm *vm, const uint32_t *insn) { (void)vm; (void)insn; return 1; }

int sgxPdsOpMov32(SgxPdsVm *vm, const uint32_t *insn)
{
	uint32_t w = *insn;
	uint32_t srcBank = (w >> 15) & 3U;
	uint32_t srcIdx = (w >> 9) & 0x3FU;
	int dstIsConst = (w & 0x80U) != 0U;
	uint32_t dstIdx = (w >> 1) & 0x3FU;
	uint32_t *src;
	uint32_t *dst;

	if (srcBank == 0U)
		src = &vm->tempRegs[srcIdx];
	else if (srcBank == 1U)
		src = &vm->constRegs[srcIdx];
	else
		return 0;

	dst = dstIsConst ? &vm->constRegs[dstIdx] : &vm->tempRegs[dstIdx];
	*dst = *src;
	return 0;
}

int sgxPdsOpMov64(SgxPdsVm *vm, const uint32_t *insn)
{
	uint32_t w = *insn;
	uint32_t srcBank = (w >> 15) & 3U;
	uint32_t srcIdx = (w >> 10) & 0x1FU;
	int dstIsConst = (w & 0x80U) != 0U;
	uint32_t dstIdx = (w >> 2) & 0x1FU;
	uint64_t *src;
	uint64_t *dst;

	if (srcBank == 0U)
		src = (uint64_t *)&vm->tempRegs[2U * srcIdx];
	else if (srcBank == 1U)
		src = (uint64_t *)&vm->constRegs[2U * srcIdx];
	else
		return 0;

	dst = dstIsConst ? (uint64_t *)&vm->constRegs[2U * dstIdx]
			 : (uint64_t *)&vm->tempRegs[2U * dstIdx];
	*dst = *src;
	return 0;
}

int sgxPdsOpAdd(SgxPdsVm *vm, const uint32_t *insn)
{
	uint32_t w = *insn;
	uint32_t src0Idx = (w >> 17) & 0x3FU;
	uint32_t src1Idx = (w >> 9) & 0x3FU;
	int dstIsConst = (w & 0x40U) != 0U;
	uint32_t dstIdx = w & 0x3FU;
	uint32_t v0;

	if ((w & 0x800000U) == 0U) {
		v0 = vm->tempRegs[src0Idx];
	} else if (src0Idx == 0U) {
		v0 = vm->ir0;
	} else if (src0Idx == 1U) {
		v0 = vm->ir1;
	} else {
		v0 = 0U;
	}

	if (dstIsConst)
		vm->constRegs[dstIdx] = v0 + vm->constRegs[src1Idx];
	else
		vm->tempRegs[dstIdx] = v0 + vm->constRegs[src1Idx];
	return 0;
}

int sgxPdsOpMul(SgxPdsVm *vm, const uint32_t *insn)
{
	uint32_t w = *insn;
	uint32_t src0Idx = (w >> 16) & 0x7FU;
	uint32_t src1Idx = (w >> 8) & 0x7FU;
	int dstIsConst = (w & 0x40U) != 0U;
	uint32_t dstIdx = w & 0x3FU;
	int32_t v0, v1;

	if ((w & 0x800000U) != 0U) {
		const uint16_t *ir16 = (const uint16_t *)&vm->ir0;
		v0 = (src0Idx < 4U) ? (int32_t)ir16[src0Idx] : 0;
	} else {
		v0 = (int32_t)((const uint16_t *)vm->tempRegs)[src0Idx];
	}

	v1 = (int32_t)((const uint16_t *)vm->constRegs)[src1Idx];
	if (vm->flags < 0) {
		v0 = (int16_t)v0;
		v1 = (int16_t)v1;
	}

	if (dstIsConst)
		vm->constRegs[dstIdx] = (uint32_t)(v0 * v1);
	else
		vm->tempRegs[dstIdx] = (uint32_t)(v0 * v1);
	return 0;
}

static inline __attribute__((always_inline)) int sgxPdsEmitCommon(SgxPdsVm *vm, const uint32_t *insn, int doAdd)
{
	uint32_t w = *insn;
	uint32_t s0Idx = (w >> 18) & 0x1FU;
	uint32_t s1Idx = (w >> 12) & 0x1FU;
	uint32_t vals[4] = { 0, 0, 0, 0 };
	uint32_t ids[4] = { 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU };
	int err = 0;
	uint32_t dest = w & 0xFU;
	uint32_t swiz[4];
	uint32_t outVals[4];
	uint32_t outIds[4];
	struct {
		uint32_t v[4];
		uint32_t id[4];
		uint32_t dest;
		uint32_t isAdd;
	} cbInfo;

	if (w & 0x800000U) {
		if (s0Idx == 0U) {
			vals[0] = vm->ir0;
			ids[0] = 0x80000000U;
		} else if (s0Idx == 1U) {
			vals[0] = vm->ir1;
			ids[0] = 0x80000001U;
		} else {
			err = 4;
		}
	} else {
		vals[0] = vm->tempRegs[2U * s0Idx];
		vals[1] = vm->tempRegs[2U * s0Idx + 1U];
		ids[0] = 4U * s0Idx;
		ids[1] = 4U * s0Idx + 1U;
	}

	if (w & 0x20000U) {
		if (s1Idx == 0U) {
			vals[0] = vm->ir0;
			vals[1] = 0U;
			ids[0] = 0x80000000U;
		} else if (s1Idx == 1U) {
			vals[0] = vm->ir1;
			vals[1] = 0U;
			ids[0] = 0x80000001U;
		} else {
			if (s1Idx == 2U) {
				vals[0] = 0U;
				vals[1] = 0U;
			}
			err = 4;
		}
	} else {
		vals[2] = vm->constRegs[2U * s1Idx];
		vals[3] = vm->constRegs[2U * s1Idx + 1U];
		ids[2] = 4U * s1Idx + 2U;
		ids[3] = (4U * s1Idx + 2U) | 3U;
	}

	if (dest == 4U) {
		if (w & 0x800U) {
			swiz[0] = 2; swiz[1] = 3; swiz[2] = 0; swiz[3] = 1;
		} else {
			swiz[0] = 0; swiz[1] = 1; swiz[2] = 2; swiz[3] = 3;
		}
	} else {
		swiz[0] = (w >> 10) & 3U;
		swiz[1] = (w >> 8) & 3U;
		swiz[2] = (w >> 6) & 3U;
		swiz[3] = (w >> 4) & 3U;
	}

	outVals[0] = vals[swiz[0]];
	outVals[1] = vals[swiz[1]];
	outVals[2] = vals[swiz[2]];
	outVals[3] = vals[swiz[3]];
	outIds[0] = ids[swiz[0]];
	outIds[1] = ids[swiz[1]];
	outIds[2] = ids[swiz[2]];
	outIds[3] = ids[swiz[3]];

	cbInfo.v[0] = outVals[0];
	cbInfo.v[1] = outVals[1];
	cbInfo.v[2] = outVals[2];
	cbInfo.v[3] = outVals[3];
	cbInfo.id[0] = outIds[0];
	cbInfo.id[1] = outIds[1];
	cbInfo.id[2] = outIds[2];
	cbInfo.id[3] = outIds[3];
	cbInfo.dest = dest;
	cbInfo.isAdd = (uint32_t)doAdd;

	if (err != 0)
		return err;

	{
		SgxPdsOutput *out = (SgxPdsOutput *)vm->output;
		uint32_t word0 = doAdd ? (outVals[0] + outVals[3]) : outVals[0];
		uint32_t emit[4];

		switch (dest) {
		case 1U:
			vm->doutu = word0;
			return 0;
		case 2U:
			if (out->douttCount <= 63U) {
				out->doutt[out->douttCount] = word0;
				out->douttCount++;
			}
			return 0;
		case 3U:
			emit[0] = word0;
			emit[1] = outVals[1];
			if (out->doutdCount <= 49U) {
				out->doutd[out->doutdCount][0] = emit[0];
				out->doutd[out->doutdCount][1] = emit[1];
				out->doutdCount++;
			}
			if (vm->callback != NULL) {
				void (**vtbl)(void *, const void *, const void *) = *(void (***)(void *, const void *, const void *))vm->callback;
				vtbl[2](vm->callback, &cbInfo, emit);
			}
			return 0;
		case 4U:
			emit[0] = word0;
			emit[1] = outVals[1];
			emit[2] = outVals[2];
			emit[3] = doAdd ? 0U : outVals[3];
			if (out->doutuCount <= 31U) {
				out->doutu[out->doutuCount][0] = emit[0];
				out->doutu[out->doutuCount][1] = emit[1];
				out->doutu[out->doutuCount][2] = emit[2];
				out->doutu[out->doutuCount][3] = emit[3];
				out->doutuCount++;
			}
			if (vm->callback != NULL) {
				void (**vtbl)(void *, const void *, const void *) = *(void (***)(void *, const void *, const void *))vm->callback;
				vtbl[3](vm->callback, &cbInfo, emit);
			}
			return 0;
		case 5U:
			emit[0] = word0;
			emit[1] = outVals[1];
			emit[2] = outVals[2];
			emit[3] = doAdd ? 0U : outVals[3];
			if (out->doutaCount <= 3U) {
				out->douta[out->doutaCount][0] = emit[0];
				out->douta[out->doutaCount][1] = emit[1];
				out->douta[out->doutaCount][2] = emit[2];
				out->douta[out->doutaCount][3] = emit[3];
				out->doutaCount++;
			}
			if (vm->callback != NULL) {
				void (**vtbl)(void *, const void *, const void *) = *(void (***)(void *, const void *, const void *))vm->callback;
				vtbl[4](vm->callback, &cbInfo, emit);
			}
			return 0;
		case 6U:
			emit[0] = word0;
			emit[1] = outVals[1];
			if (out->doutcCount <= 5U) {
				out->doutc[out->doutcCount][0] = emit[0];
				out->doutc[out->doutcCount][1] = emit[1];
				out->doutcCount++;
			}
			if (vm->callback != NULL) {
				void (**vtbl)(void *, const void *, const void *) = *(void (***)(void *, const void *, const void *))vm->callback;
				vtbl[5](vm->callback, &cbInfo, emit);
			}
			return 0;
		case 7U:
			return 8;
		default:
			return 4;
		}
	}
}

int sgxPdsOpEmit(SgxPdsVm *vm, const uint32_t *insn)
{
	return sgxPdsEmitCommon(vm, insn, 0);
}

int sgxPdsOpEmitAdd(SgxPdsVm *vm, const uint32_t *insn)
{
	return sgxPdsEmitCommon(vm, insn, 1);
}

/* --- Register bank (512 dwords + 64-byte valid bitmap) --- */

SgxRegBank *sgxRegBankInit(SgxRegBank *bank)
{
	int i;

	razorMemset(bank->valid, 0, sizeof(bank->valid));
	for (i = 0; i < 512; i++)
		bank->regs[i] = 0xCDCDCDCDU;
	return bank;
}

void sgxRegBankReset(SgxRegBank *bank)
{
	int i;

	razorMemset(bank->valid, 0, sizeof(bank->valid));
	for (i = 0; i < 512; i++)
		bank->regs[i] = 0xCDCDCDCDU;
}

int sgxRegBankWrite(SgxRegBank *bank, uint32_t regIdx, uint32_t val)
{
	if (regIdx >= 512U)
		return 0;
	bank->regs[regIdx] = val;
	bank->valid[regIdx >> 3] |= (uint8_t)(1U << (regIdx & 7U));
	return 1;
}

int sgxRegBankRead(const SgxRegBank *bank, uint32_t regIdx, uint32_t *outVal)
{
	*outVal = 0;
	if (regIdx >= 512U)
		return 0;
	if ((bank->valid[regIdx >> 3] & (1U << (regIdx & 7U))) == 0U)
		return 0;
	*outVal = bank->regs[regIdx];
	return 1;
}

/* --- PDS VM init / load / run / DMA extraction --- */

SgxPdsVm *sgxPdsVmInit(SgxPdsVm *vm)
{
	vm->program = NULL;
	vm->output = NULL;
	vm->callback = NULL;
	vm->flags = 0;
	vm->ir0 = 0;
	vm->ir1 = 0;
	vm->doutu = 0;
	vm->reserved = 0;
	return vm;
}

static inline __attribute__((always_inline)) void sgxPdsLoadConstants(SgxPdsVm *vm, const SgxPdsProgram *prog)
{
	const uint32_t *src = prog->codePtr;
	uint32_t n = prog->dataSize >> 2;
	uint32_t i;

	for (i = 0; i < n; i++) {
		uint32_t bank = (i & 2U) ? 64U : 0U;
		uint32_t slot = 2U * (i >> 2) + (i & 1U);
		vm->tempRegs[bank + slot] = src[i];
	}
}

void sgxPdsVmSetupWithIr(SgxPdsVm *vm, const SgxPdsProgram *prog, uint32_t ir0, uint32_t ir1, uint8_t isSigned)
{
	vm->program = prog;
	vm->ir0 = ir0;
	vm->ir1 = ir1;
	vm->flags = isSigned ? 8 : 0;
	sgxPdsLoadConstants(vm, prog);
}

void sgxPdsVmSetupA(SgxPdsVm *vm, const SgxPdsProgram *prog)
{
	vm->program = prog;
	vm->ir0 = 0;
	vm->ir1 = 0;
	vm->flags = 0;
	sgxPdsLoadConstants(vm, prog);
}

void sgxPdsVmSetupB(SgxPdsVm *vm, const SgxPdsProgram *prog)
{
	vm->program = prog;
	vm->ir0 = 0;
	vm->ir1 = 0;
	vm->flags = 0;
	sgxPdsLoadConstants(vm, prog);
}

static const SgxPdsOpFn s_pdsOpTable0[8] = {
	sgxPdsOpEmit,
	sgxPdsOpUnsupported1,
	sgxPdsOpMov32,
	sgxPdsOpMov64,
	sgxPdsOpUnsupported2,
	sgxPdsOpEmitAdd,
	sgxPdsOpUnsupported7,
	sgxPdsOpUnsupported7,
};

static const SgxPdsOpFn s_pdsOpTable1[8] = {
	sgxPdsOpAdd,
	sgxPdsOpUnsupported3,
	sgxPdsOpUnsupported4,
	sgxPdsOpUnsupported5,
	sgxPdsOpMul,
	sgxPdsOpUnsupported6,
	sgxPdsOpUnsupported7,
	sgxPdsOpUnsupported7,
};

int sgxPdsVmRun(SgxPdsVm *vm, void *output, void *callback)
{
	const SgxPdsProgram *prog = vm->program;
	const uint32_t *cur = (const uint32_t *)((uintptr_t)prog->codePtr + (prog->dataSize & ~3U));
	const uint32_t *end = cur + prog->codeWords;
	int status = 0;

	vm->output = output;
	vm->callback = callback;

	if (cur >= end)
		return 2;

	while (cur < end) {
		uint32_t insn = *cur++;
		uint32_t cc = insn & 0x07000000U;
		int pred = 1;

		switch (cc) {
		case 0x00000000U: pred = (vm->flags & 0x01) != 0; break;
		case 0x01000000U: pred = (vm->flags & 0x02) != 0; break;
		case 0x02000000U: pred = (vm->flags & 0x04) != 0; break;
		case 0x03000000U: pred = (vm->flags & 0x08) != 0; break;
		case 0x04000000U: pred = (vm->flags & 0x10) != 0; break;
		case 0x05000000U: pred = (vm->flags & 0x40) != 0; break;
		case 0x06000000U: pred = (vm->flags & 0x80) != 0; break;
		default: pred = 1; break;
		}

		if (pred) {
			uint32_t grp = insn >> 30;
			uint32_t op = (insn >> 27) & 7U;

			if (grp == 2U && op == 5U)
				return status;
			if (grp == 0U) {
				status |= s_pdsOpTable0[op](vm, &insn);
			} else if (grp == 1U) {
				status |= s_pdsOpTable1[op](vm, &insn);
			} else if (grp == 3U && (op == 6U || op == 7U)) {
				uint32_t srcBank = (insn >> 22) & 3U;
				uint32_t shift = (insn >> 8) & 0x7FU;
				int32_t val = 0;
				int err = 0;
				uint32_t res;

				if (srcBank != 0U)
					err = 8;
				else
					val = (int32_t)vm->tempRegs[(insn >> 16) & 0x3FU];

				if (op == 7U)
					res = (vm->flags >= 0) ? ((uint32_t)val >> shift) : (uint32_t)(val >> shift);
				else
					res = (uint32_t)val << shift;

				if (insn & 0x40U) {
					status |= 8;
				} else {
					status |= err;
					vm->tempRegs[insn & 0x3FU] = res;
				}
			} else {
				status |= 1;
			}
		}
	}

	return status | 2;
}

int sgxPdsOutputReadDmaReg(const SgxPdsOutput *out, int regIdx, uint32_t *outVal)
{
	uint32_t i;

	*outVal = 0;
	for (i = 0; i < out->doutdCount; i++) {
		uint32_t ctrl = out->doutd[i][1];
		int rel = regIdx - (int)((ctrl >> 8) & 0xFFFU);
		if (rel >= 0 && (uint32_t)(4 * rel) < sgxDmaByteSize(out->doutd[i])) {
			uint32_t cpuPtr = out->dmaCpuPtrs[i];
			if (cpuPtr != 0U) {
				*outVal = *(const uint32_t *)(uintptr_t)(cpuPtr + (uint32_t)(4 * rel));
				return 1;
			}
		}
	}
	return 0;
}

int sgxPdsOutputApplyToRegBank(const SgxPdsOutput *out, SgxRegBank *bank)
{
	int ok = 1;
	uint32_t i, j;

	for (i = 0; i < out->doutdCount; i++) {
		uint32_t cpuPtr = out->dmaCpuPtrs[i];
		if (cpuPtr != 0U) {
			uint32_t ctrl = out->doutd[i][1];
			uint32_t base = (ctrl >> 8) & 0xFFFU;
			uint32_t count = (ctrl & 0xFFU) + 1U;
			const uint32_t *src = (const uint32_t *)(uintptr_t)cpuPtr;
			for (j = 0; j < count; j++) {
				if (!sgxRegBankWrite(bank, base + j, src[j]))
					ok = 0;
			}
		} else {
			ok = 0;
		}
	}

	for (i = 0; i < out->doutcCount; i++) {
		uint32_t val = out->doutc[i][0];
		uint32_t dstReg = (out->doutc[i][1] >> 8) & 0xFFFU;
		if (!sgxRegBankWrite(bank, dstReg, val))
			ok = 0;
	}

	return ok;
}
