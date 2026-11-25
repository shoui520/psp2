/*
 * SceRazorCapture: SGX543 render target, depth/stencil, texture format tables,
 * and USSE program scanner / mini-VM interpreter.
 */

#include "razor_capture_internal.h"

/* External helpers in sgx_decode.c */
extern int sgxRegBankWrite(SgxRegBank *bank, uint32_t regIdx, uint32_t val);
extern int sgxRegBankRead(const SgxRegBank *bank, uint32_t regIdx, uint32_t *outVal);

/* --- Render Target state --- */

typedef struct SgxRtFormatEntry {
	uint32_t mask;
	uint32_t bytesPerPixel;
} SgxRtFormatEntry;

static const SgxRtFormatEntry s_rtFormatTable[] = {
	{ 0x00000000U, 4 },
	{ 0x10000000U, 3 },
	{ 0x20000000U, 0 },
	{ 0x30000000U, 2 },
	{ 0x40000000U, 2 },
	{ 0x50000000U, 2 },
	{ 0x60000000U, 2 },
	{ 0x70000000U, 2 },
	{ 0x80000000U, 1 },
	{ 0xF0000000U, 2 },
	{ 0x00800000U, 4 },
	{ 0x10800000U, 4 },
	{ 0x20800000U, 2 },
	{ 0x30800000U, 4 },
	{ 0x40800000U, 2 },
	{ 0x50800000U, 4 },
	{ 0x60800000U, 4 },
	{ 0x70800000U, 4 },
	{ 0x80800000U, 1 },
	{ 0x90800000U, 1 },
	{ 0xA0800000U, 2 },
	{ 0xB0800000U, 2 },
	{ 0xC0800000U, 2 },
	{ 0xD0800000U, 4 },
	{ 0xE0800000U, 4 },
	{ 0x01000000U, 8 },
	{ 0x11000000U, 8 },
	{ 0x21000000U, 4 },
	{ 0x31000000U, 4 },
	{ 0x41000000U, 4 },
	{ 0xFFFFFFFFU, 0 },
};

void *sgxRtStateInit(void *rtState, const uint32_t *words)
{
	uint32_t *out = (uint32_t *)rtState;

	out[0] = 0;
	out[7] = 0xFFFFFFFFU;
	out[8] = 0;
	out[9] = 0;
	out[10] = 0;
	out[11] = 0;
	out[1] = words[0];
	out[2] = words[1];
	out[3] = words[2];
	out[4] = words[3];
	out[5] = words[4];
	out[6] = words[5];
	return rtState;
}

uint32_t sgxRtCalcByteSize(const uint32_t *rtState)
{
	uint32_t w1 = rtState[1];
	uint32_t height;
	uint32_t fmt = w1 & 0xF1800000U;
	const SgxRtFormatEntry *e;

	if (((w1 >> 26) & 3U) == 1U)
		height = (((rtState[6] >> 12) & 0xFFFU) + 32U) & 0x1FE0U;
	else
		height = ((rtState[6] >> 12) & 0xFFFU) + 1U;

	for (e = s_rtFormatTable; e->mask != 0xFFFFFFFFU; e++) {
		if (e->mask == fmt)
			return height * e->bytesPerPixel * 2U * ((rtState[3] & 0x3FFFU) + 1U);
	}
	return 0;
}

/* --- Depth / Stencil state --- */

void *sgxDepthStateInit(void *depthState, const void *src)
{
	*(const void **)depthState = src;
	return depthState;
}

int sgxDepthIsDepthEnabled(const uint32_t **depthState, int isStore)
{
	uint32_t w0 = (*depthState)[0];

	if (isStore)
		return (w0 & 0x40000U) != 0;
	return (w0 & 0x4000U) != 0;
}

int sgxDepthIsStencilEnabled(const uint32_t **depthState, int isStore)
{
	uint32_t w0 = (*depthState)[0];

	if (isStore)
		return (w0 & 0x20000U) != 0;
	return (w0 & 0x2000U) != 0;
}

uint32_t sgxDepthGetDepthAddr(const uint32_t **depthState, int isStore)
{
	const uint32_t *ds = *depthState;

	if (isStore)
		return ds[4];
	return ds[6];
}

uint32_t sgxDepthGetStencilAddr(const uint32_t **depthState, int isStore)
{
	const uint32_t *ds = *depthState;
	uint32_t w0 = ds[0];
	uint32_t fmt = isStore ? ((w0 >> 24) & 7U) : ((w0 >> 21) & 7U);

	if (fmt == 1U)
		return isStore ? ds[4] : ds[6];
	return isStore ? ds[5] : ds[7];
}

uint32_t sgxDepthCalcDepthSize(const uint32_t **depthState, int isStore)
{
	const uint32_t *ds = *depthState;
	uint32_t w0 = ds[0];
	uint32_t samples = ((w0 >> 3) & 0xFFU) + 1U;
	uint32_t fmt = isStore ? ((w0 >> 24) & 7U) : ((w0 >> 21) & 7U);
	uint32_t bpp;

	if (fmt <= 1U)
		bpp = 4U;
	else if (fmt == 2U)
		bpp = 2U;
	else
		bpp = 0U;

	return bpp * samples * ((ds[2] + 31U) & 0xFFFFFFE0U) * 32U * ((const uint16_t *)ds)[7];
}

uint32_t sgxDepthCalcStencilSize(const uint32_t **depthState, int isStore)
{
	const uint32_t *ds = *depthState;
	uint32_t w0 = ds[0];
	uint32_t samples = ((w0 >> 3) & 0xFFU) + 1U;
	uint32_t fmt = isStore ? ((w0 >> 24) & 7U) : ((w0 >> 21) & 7U);
	uint32_t bpp;

	if (fmt == 1U)
		bpp = 4U;
	else
		bpp = (fmt == 0U) ? 1U : 0U;

	return bpp * samples * ((ds[2] + 31U) & 0xFFFFFFE0U) * 32U * ((const uint16_t *)ds)[7];
}

/* --- Texture format lookup & YUV layout --- */

typedef struct SgxTexFormatEntry {
	uint32_t fmt;
	uint32_t bppAndFlags;
	uint32_t blockAndType;
} SgxTexFormatEntry;

static const SgxTexFormatEntry s_texFmtInvalid = { 0xFFFFFFFFU, 0, 0 };

static const SgxTexFormatEntry s_texFmtExtTable[] = {
	{ 0x00000000U, 65538, 8650753 },
	{ 0x01000000U, 65540, 4456449 },
	{ 0x02000000U, 65538, 8650753 },
	{ 0x03000000U, 65540, 4456449 },
	{ 0x04000000U, 65536, 1 },
	{ 0x10000000U, 65536, 1114624 },
	{ 0x11000000U, 65536, 1114880 },
	{ 0x12000000U, 65552, 1114112 },
	{ 0x14000000U, 65540, 17891328 },
	{ 0x15000000U, 65544, 269549570 },
	{ 0x18000000U, 24, 1114112 },
	{ 0x19000000U, 24, 1114112 },
	{ 0x1A000000U, 32, 1114116 },
	{ 0x05000000U, 65540, 4456449 },
	{ 0x06000000U, 65544, 4456449 },
	{ 0x07000000U, 65544, 4456449 },
	{ 0x08000000U, 65540, 4456449 },
	{ 0x09000000U, 65540, 4456449 },
	{ 0x0A000000U, 65544, 4456449 },
	{ 0x0B000000U, 65544, 4456449 },
	{ 0xFFFFFFFFU, 0, 0 },
};

static const SgxTexFormatEntry s_texFmtStdTable[] = {
	{ 0x00000000U, 8, 1114114 },
	{ 0x01000000U, 8, 1114114 },
	{ 0x02000000U, 16, 1114115 },
	{ 0x03000000U, 16, 1114115 },
	{ 0x04000000U, 16, 1114115 },
	{ 0x05000000U, 16, 1114115 },
	{ 0x06000000U, 16, 1114115 },
	{ 0x07000000U, 16, 1114115 },
	{ 0x08000000U, 16, 1114115 },
	{ 0x09000000U, 16, 1114115 },
	{ 0x0A000000U, 16, 1114115 },
	{ 0x0B000000U, 16, 1114115 },
	{ 0x0C000000U, 32, 1114116 },
	{ 0x0D000000U, 32, 1114116 },
	{ 0x0E000000U, 32, 1114116 },
	{ 0x0F000000U, 32, 1114116 },
	{ 0x10000000U, 32, 1114116 },
	{ 0x11000000U, 32, 1114116 },
	{ 0x12000000U, 32, 1114116 },
	{ 0x13000000U, 32, 1114116 },
	{ 0x14000000U, 32, 1114116 },
	{ 0x15000000U, 32, 1114116 },
	{ 0x16000000U, 32, 1114116 },
	{ 0x17000000U, 32, 1114116 },
	{ 0x18000000U, 32, 1114116 },
	{ 0x19000000U, 32, 1114116 },
	{ 0x1A000000U, 32, 1114116 },
	{ 0x1B000000U, 64, 1114117 },
	{ 0x1C000000U, 64, 1114117 },
	{ 0x1D000000U, 64, 1114117 },
	{ 0x1E000000U, 64, 1114117 },
	{ 0x1F000000U, 64, 1114117 },
	{ 0xFFFFFFFFU, 0, 0 },
};

const uint32_t *sgxLookupTexFormat(uint32_t fmt, int isExtended)
{
	const SgxTexFormatEntry *table = isExtended ? s_texFmtExtTable : s_texFmtStdTable;
	const SgxTexFormatEntry *e;

	for (e = table; e->fmt != 0xFFFFFFFFU; e++) {
		if (e->fmt == fmt)
			return &e->fmt;
	}
	return &s_texFmtInvalid.fmt;
}

int sgxCalcYuvLayout(uint32_t width, uint32_t height, uint32_t stride, uint32_t strideHeight,
		     uint8_t useStride, uint8_t planes, uint32_t *outPlanes, int *outSize1, int *outSize2)
{
	uint32_t yStride, yHeight, uvStride, uvHeight, yAlignW, uvAlignW;
	uint32_t off1, off2, pitch1, pitch2, size1, size2;

	if (outSize1 != NULL)
		*outSize1 = 0;
	if (outSize2 != NULL)
		*outSize2 = 0;

	if (useStride) {
		yStride = stride;
		yHeight = strideHeight;
		uvStride = stride >> 1;
		uvHeight = strideHeight >> 1;
		yAlignW = (width + 7U) & ~7U;
		uvAlignW = ((width >> 1) + 7U) & ~7U;
	} else {
		yStride = (width + 7U) & ~7U;
		yHeight = height;
		uvStride = ((width >> 1) + 7U) & ~7U;
		uvHeight = height >> 1;
		yAlignW = yStride;
		uvAlignW = uvStride;
	}

	if (yStride < 8U)
		yStride = 8U;
	if (yHeight < 2U)
		yHeight = 2U;
	if (uvStride < 8U)
		uvStride = 8U;
	if (uvHeight == 0U)
		uvHeight = 1U;
	if (yAlignW < 8U)
		yAlignW = 8U;
	if (uvAlignW < 8U)
		uvAlignW = 8U;

	if (planes == 2U) {
		off1 = yStride * yHeight;
		off2 = 0U;
		pitch1 = 2U * uvAlignW;
		pitch2 = 0U;
		size1 = off1 + 2U * uvAlignW * uvHeight;
		size2 = off1 + 2U * uvStride * uvHeight;
	} else if (planes == 3U) {
		uint32_t uvPlane = uvStride * uvHeight;
		off1 = yStride * yHeight;
		off2 = off1 + uvPlane;
		pitch1 = uvAlignW;
		pitch2 = uvAlignW;
		size1 = off2 + uvAlignW * uvHeight;
		size2 = off2 + uvPlane;
	} else {
		return 0;
	}

	if (outPlanes != NULL) {
		outPlanes[0] = 0U;
		outPlanes[1] = off1;
		outPlanes[2] = off2;
		outPlanes[3] = yAlignW;
		outPlanes[4] = pitch1;
		outPlanes[5] = pitch2;
	}
	if (outSize1 != NULL)
		*outSize1 = (int)size1;
	if (outSize2 != NULL)
		*outSize2 = (int)size2;
	return 1;
}

int sgxIsSwizzledTexType(int texType)
{
	return (uint32_t)(texType - 4) <= 3U;
}

/* --- USSE instruction decoders --- */

int sgxUsseInsnHasEndFlag(const uint32_t *insn)
{
	uint32_t hi = insn[1];
	int endFlag = (hi & 0x40000U) != 0;
	uint32_t op = hi >> 27;

	if (op != 31U) {
		uint32_t rel = op - 3U;
		if (rel <= 0x16U && (((1U << rel) & 0x7BEFB9U) != 0U))
			return endFlag;
		return 0;
	}

	switch ((hi >> 20) & 3U) {
	case 0:
		return (((hi >> 6) & 7U) == 5U) ? endFlag : 0;
	case 2:
		return ((((hi >> 24) & 7U) - 3U) <= 1U) ? endFlag : 0;
	case 3:
		return endFlag;
	default:
		return 0;
	}
}

int sgxUsseInsnIsSpecial(const uint32_t *insn, int *outSubOp, int *outImm, int *outMode)
{
	uint32_t hi = insn[1];

	*outSubOp = 0;
	if ((hi >> 27) != 31U || ((hi >> 20) & 3U) != 0U)
		return 0;
	if ((hi & 0x400000U) == 0U)
		return 0;
	if (((hi >> 24) & 7U) != 2U || (hi & 0x800000U) != 0U)
		return 0;
	if ((hi & 0x40000U) == 0U)
		return 0;

	*outSubOp = (int)((hi >> 8) & 7U);
	if (outImm != NULL)
		*outImm = (int)(insn[0] & 0xFFFFFU);
	if (outMode != NULL)
		*outMode = (int)((hi >> 11) & 3U);
	return 1;
}

int sgxUsseInsnIsBranch(const uint32_t *insn, int32_t *outOffset)
{
	uint32_t hi = insn[1];
	uint32_t off;

	if ((hi >> 27) != 31U || ((hi >> 20) & 3U) != 0U || ((hi >> 6) & 7U) != 1U)
		return 0;
	off = insn[0] & 0xFFFFFU;
	if (off & 0x80000U)
		off |= 0xFFF00000U;
	*outOffset = (int32_t)off;
	return 1;
}

int sgxUsseInsnGetTestPred(const uint32_t *insn)
{
	uint32_t hi = insn[1];
	int bit = (hi & 0x80U) != 0;

	if ((hi & 0x400000U) == 0U)
		return bit ? 3 : 2;
	return bit;
}

int sgxUsseInsnIsBitwiseOr(const uint32_t *insn)
{
	return (insn[1] & 8U) != 0;
}

int sgxUsseDecodeSrc0Bank(int bankBits, int extBit)
{
	if (extBit) {
		if (bankBits == 0)
			return 1;
		if (bankBits == 1)
			return 3;
		return 9;
	}
	if (bankBits == 0)
		return 0;
	if (bankBits == 1)
		return 2;
	return 9;
}

int sgxUsseDecodeSrc1Bank(int bankBits, int extBit)
{
	if (extBit) {
		switch (bankBits) {
		case 0: return 4;
		case 1: return 5;
		case 2: return 6;
		case 3: return 7;
		default: return 9;
		}
	}
	switch (bankBits) {
	case 0: return 0;
	case 1: return 1;
	case 2: return 2;
	case 3: return 3;
	default: return 9;
	}
}

int sgxUsseDecodeSrc2Bank(int bankBits, int extBit)
{
	if (extBit) {
		switch (bankBits) {
		case 0: return 4;
		case 1: return 5;
		case 2: return 6;
		case 3: return 7;
		default: return 9;
		}
	}
	switch (bankBits) {
	case 0: return 0;
	case 1: return 1;
	case 2: return 2;
	case 3: return 3;
	default: return 9;
	}
}

int sgxUsseDecodeDstBank(int bankBits, int extBit)
{
	if (extBit) {
		switch (bankBits) {
		case 0: return 3;
		case 1: return 5;
		case 2: return 8;
		case 3: return 7;
		default: return 9;
		}
	}
	switch (bankBits) {
	case 0: return 0;
	case 1: return 1;
	case 2: return 2;
	case 3: return 4;
	default: return 9;
	}
}

/* --- USSE program scanner --- */

int sgxScanUsseProgram(SgxUsseProgramInfo *outInfo, uint64_t *entryPtr, uint32_t gpuEntryAddr,
		       int minBytes, int maxBytes, uint8_t checkFooter)
{
	uint64_t *startPtr = entryPtr;
	uint64_t *backScanPtr = entryPtr;
	uint64_t *fwdScanPtr = entryPtr;
	uint64_t *curPtr = entryPtr;
	int sawSpecial7 = 0;

	outInfo->entryPtr = entryPtr;
	outInfo->gpuEntryAddr = gpuEntryAddr;
	outInfo->startPtr = NULL;
	outInfo->insnCount = 0;
	outInfo->byteSize = 0;
	outInfo->entryOffsetInsn = 0;
	outInfo->hasFooter = 0;
	outInfo->hasProgramPtr = 0;
	outInfo->footerHash = 0;
	outInfo->programPtr = 0;

	while (((int)((char *)curPtr - (char *)entryPtr) >> 2) <= maxBytes / 4 &&
	       ((int)((char *)startPtr - (char *)entryPtr) >> 2) >= minBytes / 4) {
		if (startPtr < backScanPtr) {
			uint64_t *p = startPtr;
			uint64_t *newStart = startPtr;
			while (p < backScanPtr) {
				uint64_t insn = *p;
				int32_t off;
				if (sgxUsseInsnIsBranch((const uint32_t *)&insn, &off)) {
					uint64_t *target = p + off;
					if (target < newStart)
						newStart = target;
					if (target > curPtr)
						curPtr = target;
				}
				p++;
			}
			backScanPtr = startPtr;
			startPtr = newStart;
			continue;
		}

		if (fwdScanPtr < curPtr) {
			uint64_t *p = fwdScanPtr;
			uint64_t *newEnd = curPtr;
			uint8_t found7 = 0;
			while (p < curPtr) {
				uint64_t insn = *p;
				int32_t off;
				int subOp;
				if (sgxUsseInsnIsBranch((const uint32_t *)&insn, &off)) {
					uint64_t *target = p + off;
					if (target < startPtr)
						startPtr = target;
					if (target > newEnd)
						newEnd = target;
				}
				if (sgxUsseInsnIsSpecial((const uint32_t *)&insn, &subOp, NULL, NULL) && subOp == 7)
					found7 = 1;
				p++;
			}
			fwdScanPtr = curPtr;
			curPtr = newEnd;
			sawSpecial7 |= found7;
			continue;
		}

		{
			uint64_t insn = *curPtr;
			int32_t off;
			int subOp;

			if (sgxUsseInsnIsBranch((const uint32_t *)&insn, &off)) {
				uint64_t *next = curPtr + 1;
				curPtr += off;
				if (curPtr < startPtr)
					startPtr = curPtr;
				if (curPtr < next)
					curPtr = next;
				continue;
			}
			if (sgxUsseInsnIsSpecial((const uint32_t *)&insn, &subOp, NULL, NULL) && subOp == 7) {
				curPtr++;
				fwdScanPtr = curPtr;
				sawSpecial7 = 1;
				continue;
			}
			if (!sgxUsseInsnHasEndFlag((const uint32_t *)&insn) || !sawSpecial7) {
				curPtr++;
				fwdScanPtr = curPtr;
				continue;
			}

			{
				uint32_t dwords = (uint32_t)(((char *)(curPtr + 1) - (char *)startPtr) >> 2);
				uint32_t bytes = dwords * 4U;

				outInfo->startPtr = startPtr;
				outInfo->insnCount = dwords >> 1;
				outInfo->byteSize = bytes;
				outInfo->entryOffsetInsn = (uint32_t)(((char *)outInfo->entryPtr - (char *)startPtr) >> 2) >> 1;

				if (checkFooter &&
				    ((((int)((char *)(curPtr + 1) - (char *)entryPtr)) & ~3) + 16 <= maxBytes)) {
					const uint32_t *footer = (const uint32_t *)(curPtr + 1);
					if (footer[0] == 0xDEADC0DEU &&
					    (footer[3] == 0xA50C0001U || footer[3] == 0xA50C0002U)) {
						outInfo->hasFooter = 1;
						outInfo->footerHash = footer[2];
						outInfo->hasProgramPtr = (footer[3] == 0xA50C0002U) ? 1U : 0U;
						outInfo->programPtr = (footer[3] == 0xA50C0002U) ? footer[1] : 0U;
						outInfo->byteSize = bytes + 16U;
					}
				}
				return 1;
			}
		}
	}

	return 0;
}

void sgxFindUsseTexSamples(const SgxUsseProgramInfo *info, int *outOffsets, int isSecondary)
{
	uint32_t count = info->insnCount;
	const uint32_t *insns = (const uint32_t *)info->startPtr;
	int targetBank = isSecondary ? 3 : 2;
	uint32_t i;

	for (i = 0; i < count; i++) {
		uint32_t lo = insns[2 * i];
		uint32_t hi = insns[2 * i + 1];

		if ((hi >> 27) == 28U) {
			if (sgxUsseDecodeSrc1Bank((int)(lo >> 30), (hi & 0x20000U) != 0) == targetBank) {
				int reg = 2 * (int)((lo >> 7) & 0x7FU);
				if ((uint32_t)outOffsets[0] <= 0x3FU) {
					outOffsets[outOffsets[0] + 2] = reg;
					outOffsets[0]++;
				}
			}
		}
	}
}

int sgxUsseGetFooter(const SgxUsseProgramInfo *info, uint32_t *outHash, uint8_t *outHasProg, uint32_t *outProg)
{
	*outHash = info->footerHash;
	*outHasProg = (info->hasProgramPtr != 0) ? 1 : 0;
	*outProg = (info->hasProgramPtr != 0) ? info->programPtr : 0;
	return info->hasFooter != 0;
}

/* --- USSE Mini-VM for PDS/SA secondary attribute evaluation --- */

typedef struct SgxUsseIndexState {
	int32_t wrapMode;       /* +0  (vm + 20) */
	int32_t repeatInc[4];   /* +4  (vm + 24..36) */
	int32_t wrapSize;       /* +20 (vm + 40) */
	int32_t indexOffset;    /* +24 (vm + 44) */
	int32_t baseOffset;     /* +28 (vm + 48) */
	int32_t bankBase[3];    /* +32 (vm + 52..60) */
	uint8_t usePerLaneInc;  /* +44 (vm + 64) */
	uint8_t pad[3];
} SgxUsseIndexState;

typedef struct SgxUsseVm {
	uint32_t errorFlags;    /* +0 */
	const SgxUsseProgramInfo *program; /* +4 */
	void *emitOut;          /* +8 */
	const uint64_t *pc;     /* +12 */
	uint8_t pred[4];        /* +16 */
	SgxUsseIndexState idx;  /* +20 .. +67 */
	uint32_t tempRegs[1024]; /* +68 (17..1040) */
	uint32_t outRegs[1024];  /* +4164 (1041..2064) */
	SgxRegBank *paBank;     /* +8260 (2065) */
	SgxRegBank *saBank;     /* +8264 (2066) */
	uint32_t reserved;      /* +8268 (2067) */
} SgxUsseVm;

int sgxUsseResolveRegIndex(const SgxUsseIndexState *idx, int reg, int lane, int bank)
{
	int baseTable[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int bankBase = 0;
	int laneOff;

	baseTable[0] = idx->bankBase[0];
	baseTable[2] = idx->bankBase[1];
	baseTable[3] = idx->bankBase[2];
	if (bank <= 8)
		bankBase = baseTable[bank];

	laneOff = idx->usePerLaneInc ? idx->repeatInc[lane & 3] : (idx->repeatInc[0] * lane);
	if (reg < bankBase)
		return laneOff + reg;

	{
		int rel = reg + idx->indexOffset - bankBase;
		int wrap = idx->wrapSize;

		if (rel >= wrap) {
			if (idx->wrapMode == 1)
				rel -= wrap;
			else if (idx->wrapMode == 2)
				rel = wrap - 1;
			else if (idx->wrapMode == 3)
				rel = 2 * wrap - rel - 2;
		} else if (rel < 0) {
			if (idx->wrapMode == 1)
				rel += wrap;
			else if (idx->wrapMode == 2)
				rel = 0;
			else if (idx->wrapMode == 3)
				rel = -rel;
		}
		return bankBase + laneOff + rel + idx->baseOffset;
	}
}

SgxUsseVm *sgxUsseVmInit(SgxUsseVm *vm)
{
	vm->errorFlags = 0;
	vm->program = NULL;
	vm->emitOut = NULL;
	vm->pc = NULL;
	vm->pred[0] = 0;
	vm->pred[1] = 0;
	vm->pred[2] = 0;
	vm->pred[3] = 0;
	vm->idx.wrapMode = 0;
	vm->idx.usePerLaneInc = 0;
	vm->idx.indexOffset = 0;
	vm->idx.baseOffset = 0;
	vm->idx.bankBase[0] = 0;
	vm->idx.bankBase[1] = 0;
	vm->idx.bankBase[2] = 0;
	vm->paBank = NULL;
	vm->saBank = NULL;
	vm->reserved = 0;
	vm->idx.wrapSize = 1023;
	vm->idx.repeatInc[0] = 1;
	vm->idx.repeatInc[1] = 1;
	vm->idx.repeatInc[2] = 1;
	vm->idx.repeatInc[3] = 1;
	return vm;
}

SgxUsseVm *sgxUsseVmSetProgram(SgxUsseVm *vm, const SgxUsseProgramInfo *program, SgxRegBank *paBank)
{
	vm->program = program;
	vm->paBank = paBank;
	vm->errorFlags = 0;
	vm->saBank = NULL;
	return vm;
}

uint32_t sgxUsseVmReadReg(SgxUsseVm *vm, uint32_t regIdx, uint32_t immVal, int bank)
{
	uint32_t val;

	switch (bank) {
	case 0:
		if (regIdx >= 0x400U) {
			vm->errorFlags |= 2U;
			return 0xCDCDCDCDU;
		}
		return vm->tempRegs[regIdx];
	case 1:
		if (regIdx >= 0x400U) {
			vm->errorFlags |= 2U;
			return 0xCDCDCDCDU;
		}
		return vm->outRegs[regIdx];
	case 2:
		if (vm->paBank != NULL && sgxRegBankRead(vm->paBank, regIdx, &val))
			return val;
		vm->errorFlags |= 4U;
		return 0xCDCDCDCDU;
	case 3:
		if (vm->saBank != NULL && sgxRegBankRead(vm->saBank, regIdx, &val))
			return val;
		vm->errorFlags |= 4U;
		return 0xCDCDCDCDU;
	case 6:
		return immVal;
	default:
		vm->errorFlags |= 4U;
		return 0xCDCDCDCDU;
	}
}

void sgxUsseVmWriteReg(SgxUsseVm *vm, uint32_t regIdx, int bank, uint32_t val)
{
	switch (bank) {
	case 0:
		if (regIdx >= 0x400U)
			vm->errorFlags |= 2U;
		else
			vm->tempRegs[regIdx] = val;
		break;
	case 1:
		if (regIdx >= 0x400U)
			vm->errorFlags |= 2U;
		else
			vm->outRegs[regIdx] = val;
		break;
	case 2:
		if (vm->paBank == NULL)
			vm->errorFlags |= 4U;
		else if (!sgxRegBankWrite(vm->paBank, regIdx, val))
			vm->errorFlags |= 2U;
		break;
	case 3:
		if (vm->saBank == NULL)
			vm->errorFlags |= 4U;
		else if (!sgxRegBankWrite(vm->saBank, regIdx, val))
			vm->errorFlags |= 2U;
		break;
	default:
		vm->errorFlags |= 4U;
		break;
	}
}

void sgxUsseVmExecMov(SgxUsseVm *vm, const uint32_t *insn)
{
	uint32_t lo = insn[0];
	uint32_t hi = insn[1];
	int src0Bank = sgxUsseDecodeSrc0Bank((hi & 4U) != 0, 0);
	int src1Reg = (int)((lo >> 7) & 0x7FU);
	int dstReg = (int)((lo >> 21) & 0x7FU);
	int src1Bank = sgxUsseDecodeSrc1Bank((int)(lo >> 30), (hi & 0x20000U) != 0);
	int src2Bank = sgxUsseDecodeSrc2Bank((int)((lo >> 28) & 3U), (hi & 0x10000U) != 0);
	int dstBank = sgxUsseDecodeDstBank((int)(hi & 3U), (hi & 0x80000U) != 0);
	uint32_t repeat = (hi & 0x200000U) ? (((hi >> 12) & 0xFU) + 1U) : 1U;
	int condType = (int)((hi >> 8) & 7U);
	uint32_t lane;

	if (condType == 0) {
		for (lane = 0; lane < repeat; lane++) {
			int sIdx = sgxUsseResolveRegIndex(&vm->idx, src1Reg, (int)lane, src1Bank);
			int dIdx = sgxUsseResolveRegIndex(&vm->idx, dstReg, (int)lane, dstBank);
			uint32_t val = sgxUsseVmReadReg(vm, (uint32_t)sIdx, (uint32_t)src1Reg, src1Bank);
			sgxUsseVmWriteReg(vm, (uint32_t)dIdx, dstBank, val);
		}
		return;
	}

	{
		int src0Reg = (int)((lo >> 14) & 0x7FU);
		int src2Reg = (int)(lo & 0x7FU);
		int testPred = sgxUsseInsnGetTestPred(insn);

		for (lane = 0; lane < repeat; lane++) {
			int r0 = sgxUsseResolveRegIndex(&vm->idx, src0Reg, (int)lane, src0Bank);
			uint32_t v0 = sgxUsseVmReadReg(vm, (uint32_t)r0, (uint32_t)src0Reg, src0Bank);
			int r1 = sgxUsseResolveRegIndex(&vm->idx, src1Reg, (int)lane, src1Bank);
			uint32_t v1 = sgxUsseVmReadReg(vm, (uint32_t)r1, (uint32_t)src1Reg, src1Bank);
			int r2 = sgxUsseResolveRegIndex(&vm->idx, src2Reg, (int)lane, src2Bank);
			uint32_t v2 = sgxUsseVmReadReg(vm, (uint32_t)r2, (uint32_t)src2Reg, src2Bank);
			int rd = sgxUsseResolveRegIndex(&vm->idx, dstReg, (int)lane, dstBank);
			uint8_t cond[4];

			switch (condType) {
			case 1: {
				uint32_t res = 0;
				int b;
				for (b = 0; b < 4; b++) {
					int8_t sv = (int8_t)((v0 >> (b * 8)) & 0xFFU);
					cond[0] = (sv == 0);
					cond[1] = (sv != 0);
					cond[2] = (sv < 0);
					cond[3] = (sv >= 0);
					res |= (cond[testPred] ? v1 : v2) & (0xFFU << (b * 8));
				}
				sgxUsseVmWriteReg(vm, (uint32_t)rd, dstBank, res);
				break;
			}
			case 2: {
				uint32_t res = 0;
				int h;
				for (h = 0; h < 2; h++) {
					int16_t sv = (int16_t)((v0 >> (h * 16)) & 0xFFFFU);
					cond[0] = (sv == 0);
					cond[1] = (sv != 0);
					cond[2] = (sv < 0);
					cond[3] = (sv >= 0);
					res |= (cond[testPred] ? v1 : v2) & (0xFFFFU << (h * 16));
				}
				sgxUsseVmWriteReg(vm, (uint32_t)rd, dstBank, res);
				break;
			}
			case 3: {
				int32_t sv = (int32_t)v0;
				cond[0] = (sv == 0);
				cond[1] = (sv != 0);
				cond[2] = (sv < 0);
				cond[3] = (sv >= 0);
				sgxUsseVmWriteReg(vm, (uint32_t)rd, dstBank, cond[testPred] ? v1 : v2);
				break;
			}
			case 4: {
				union { uint32_t u; float f; } fv;
				fv.u = v0;
				cond[0] = (v0 == 0U);
				cond[1] = (v0 != 0U);
				cond[2] = (fv.f < 0.0f);
				cond[3] = (fv.f >= 0.0f);
				sgxUsseVmWriteReg(vm, (uint32_t)rd, dstBank, cond[testPred] ? v1 : v2);
				break;
			}
			case 5:
				vm->errorFlags |= 8U;
				break;
			default:
				break;
			}
		}
	}
}

void sgxUsseVmExecEmit(SgxUsseVm *vm, const uint32_t *insn)
{
	uint32_t lo = insn[0];
	uint32_t hi = insn[1];
	int src0Bank = sgxUsseDecodeSrc0Bank((hi & 4U) != 0, (hi & 0x80000U) != 0);
	uint32_t src0Reg = (lo >> 14) & 0x7FU;
	uint32_t src1Reg = (lo >> 7) & 0x7FU;
	uint32_t src2Reg = lo & 0x7FU;
	int src1Bank = sgxUsseDecodeSrc1Bank((int)(lo >> 30), (hi & 0x20000U) != 0);
	int src2Bank = sgxUsseDecodeSrc2Bank((int)((lo >> 28) & 3U), (hi & 0x10000U) != 0);
	uint32_t v0 = sgxUsseVmReadReg(vm, src0Reg, src0Reg, src0Bank);
	uint32_t v1 = sgxUsseVmReadReg(vm, src1Reg, src1Reg, src1Bank);
	uint32_t v2 = sgxUsseVmReadReg(vm, src2Reg, src2Reg, src2Bank);
	uint32_t emitType = (hi >> 14) & 3U;

	if (emitType == 0U) {
		vm->errorFlags |= 8U;
		return;
	}
	if (emitType != 1U)
		return;

	{
		uint32_t target = (hi >> 12) & 3U;
		if (target == 1U || target == 2U) {
			vm->errorFlags |= 8U;
		} else if (target != 0U) {
			vm->errorFlags |= 0x10U;
		} else {
			uint32_t tagBuf[30];
			uint32_t *emitList = (uint32_t *)vm->emitOut;
			uint32_t count;

			sceClibMemset(tagBuf, 0, sizeof(tagBuf));
			razorZeroInit(tagBuf);
			count = emitList[0];
			if (count <= 2U) {
				uint32_t *dst = &emitList[30 * count + 2];
				razorMemcpy(dst, tagBuf, 0x78U);
				emitList[0] = count + 1U;
				sgxTagStateInit(dst, vm->outRegs);
				dst[26] = v0;
				dst[27] = v1;
				dst[28] = v2;
				dst[29] = (((hi >> 3) & 0x3FU) << 6) |
					  (((hi >> 22) & 3U) << 12) |
					  ((lo >> 22) & 0x3FU);
			}
		}
	}
}

void sgxUsseVmExecBitwise(SgxUsseVm *vm, const uint32_t *insn)
{
	uint32_t lo = insn[0];
	uint32_t hi = insn[1];
	int src1Bank = sgxUsseDecodeSrc1Bank((int)(lo >> 30), (hi & 0x20000U) != 0);
	int src2Bank = sgxUsseDecodeSrc2Bank((int)((lo >> 28) & 3U), (hi & 0x10000U) != 0);
	int dstBank = sgxUsseDecodeDstBank((int)(hi & 3U), (hi & 0x80000U) != 0);
	uint32_t repeat = (hi & 0x200000U) ? (((hi >> 12) & 0xFU) + 1U) : 1U;
	int src1Reg = (int)((lo >> 7) & 0x7FU);
	int src2Reg = (int)(lo & 0x7FU);
	int dstReg = (int)((lo >> 21) & 0x7FU);
	uint32_t lane;

	if (sgxUsseInsnIsBitwiseOr(insn) != 1) {
		vm->errorFlags |= 8U;
		return;
	}

	for (lane = 0; lane < repeat; lane++) {
		int r1 = sgxUsseResolveRegIndex(&vm->idx, src1Reg, (int)lane, src1Bank);
		int r2 = sgxUsseResolveRegIndex(&vm->idx, src2Reg, (int)lane, src2Bank);
		uint32_t v1 = sgxUsseVmReadReg(vm, (uint32_t)r1, (uint32_t)src1Reg, src1Bank);
		uint32_t v2 = sgxUsseVmReadReg(vm, (uint32_t)r2, (uint32_t)src2Reg, src2Bank);
		int rd = sgxUsseResolveRegIndex(&vm->idx, dstReg, (int)lane, dstBank);
		sgxUsseVmWriteReg(vm, (uint32_t)rd, dstBank, v1 | v2);
	}
}

static const uint8_t s_usseSwizzleTable[16][4] = {
	{ 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 2, 2, 2, 2 }, { 3, 3, 3, 3 },
	{ 0, 1, 2, 3 }, { 1, 2, 3, 3 }, { 0, 1, 2, 2 }, { 0, 0, 1, 2 },
	{ 0, 1, 0, 1 }, { 0, 1, 3, 2 }, { 2, 0, 1, 3 }, { 2, 3, 2, 3 },
	{ 1, 2, 0, 2 }, { 0, 0, 1, 1 }, { 0, 2, 3, 3 }, { 0, 1, 2, 4 },
};

void sgxUsseVmExecSmp(SgxUsseVm *vm, const uint32_t *insn)
{
	uint32_t hi = insn[1];
	uint32_t lo = insn[0];
	int fmt;
	int srcReg, dstReg;
	int dstBank, srcBank;
	uint32_t writeMask, repeat, lane;
	const uint8_t *swiz;

	if ((hi & 0xC000U) != 0U)
		return;

	fmt = (int)((hi >> 8) & 7U);
	srcReg = (int)((lo >> 6) & 0x3FU);
	dstReg = (int)((lo >> 18) & 0x3FU);
	if ((uint32_t)(fmt - 3) <= 2U) {
		srcReg *= 2;
		dstReg *= 2;
	}
	dstBank = sgxUsseDecodeDstBank((int)(hi & 3U), (hi & 0x80000U) != 0);
	srcBank = sgxUsseDecodeSrc1Bank((int)(lo >> 30), (hi & 0x20000U) != 0);
	if (fmt != 5)
		return;

	writeMask = (lo >> 24) & 0xFU;
	swiz = s_usseSwizzleTable[(hi >> 3) & 0xFU];
	repeat = ((hi >> 12) & 3U) + 1U;

	for (lane = 0; lane < repeat; lane++) {
		int sIdx = sgxUsseResolveRegIndex(&vm->idx, srcReg, (int)lane, srcBank);
		int dIdx = sgxUsseResolveRegIndex(&vm->idx, dstReg, (int)lane, dstBank);
		uint32_t vals[5];
		int c;

		vals[4] = razorFloatToBits(1.0f);
		for (c = 0; c < 4; c++)
			vals[c] = sgxUsseVmReadReg(vm, (uint32_t)(sIdx + c), 0, srcBank);

		for (c = 0; c < 4; c++) {
			if (writeMask & (1U << c))
				sgxUsseVmWriteReg(vm, (uint32_t)(dIdx + c), dstBank, vals[swiz[c]]);
		}
	}
}

uint32_t sgxUsseVmRun(SgxUsseVm *vm, void *emitOut)
{
	const uint64_t *pc;

	vm->emitOut = emitOut;
	vm->errorFlags = 0;
	pc = vm->program->startPtr;
	vm->pc = pc;
	if (pc == NULL)
		return 64U;

	vm->idx.wrapMode = 0;
	vm->idx.usePerLaneInc = 0;
	vm->idx.indexOffset = 0;
	vm->idx.baseOffset = 0;
	vm->idx.bankBase[0] = 0;
	vm->idx.bankBase[1] = 0;
	vm->idx.bankBase[2] = 0;
	vm->pred[0] = 0;
	vm->pred[1] = 0;
	vm->pred[2] = 0;
	vm->pred[3] = 0;
	vm->idx.repeatInc[0] = 1;
	vm->idx.repeatInc[1] = 1;
	vm->idx.repeatInc[2] = 1;
	vm->idx.repeatInc[3] = 1;
	vm->idx.wrapSize = 1023;

	while (1) {
		union { uint64_t u64; uint32_t u32[2]; } insn;
		uint32_t op;
		int execute = 1;

		insn.u64 = *pc;
		vm->pc = pc + 1;
		op = insn.u32[1] >> 27;

		if (op - 16U <= 9U) {
			uint8_t ptab[4] = { 1, vm->pred[0], vm->pred[1], (uint8_t)(vm->pred[0] ^ 1) };
			execute = ptab[(insn.u32[1] >> 25) & 3U];
		} else if (op <= 15U || (op >= 28U && op <= 30U)) {
			uint8_t ptab[8] = {
				1, vm->pred[0], vm->pred[1], vm->pred[2],
				vm->pred[3], (uint8_t)(vm->pred[0] ^ 1), (uint8_t)(vm->pred[1] ^ 1), 1
			};
			execute = ptab[(insn.u32[1] >> 24) & 7U];
		}

		if (execute) {
			switch (op) {
			case 5U:
				sgxUsseVmExecMov(vm, insn.u32);
				break;
			case 7U:
				sgxUsseVmExecSmp(vm, insn.u32);
				break;
			case 10U:
				sgxUsseVmExecBitwise(vm, insn.u32);
				break;
			case 31U: {
				uint32_t sub = (insn.u32[1] >> 20) & 3U;
				if (sub == 2U) {
					if (((insn.u32[1] >> 24) & 7U) == 3U)
						sgxUsseVmExecEmit(vm, insn.u32);
					else
						vm->errorFlags |= 8U;
				} else if (sub != 0U) {
					vm->errorFlags |= 8U;
				} else if (!(((insn.u32[1] & 0x400000U) != 0U && ((insn.u32[1] >> 24) & 7U) == 2U) ||
					     (((insn.u32[1] >> 6) & 7U) == 5U))) {
					vm->errorFlags |= 1U;
				}
				break;
			}
			default:
				break;
			}
		}

		if (sgxUsseInsnHasEndFlag(insn.u32))
			return vm->errorFlags;
		pc = vm->pc;
	}
}
