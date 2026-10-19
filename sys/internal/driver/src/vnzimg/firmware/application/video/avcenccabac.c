/*
 * The AVC encoder's macroblock syntax and its CABAC engine (0x864dcc-0x866524).
 * While the helper holds the unit, the encoder writes the slices itself:
 * each macroblock's syntax elements are binarized here (truncated unary,
 * Exp-Golomb suffixes, significance maps) and the bins go to the VLIW
 * coder. The CABAC engine at 0x4e8000 writes the slice headers, and its
 * output buffers are copied into the stream.
 */
#include "service.h"

#define CABAC			((volatile uint32_t *)0x4e8000)
#define CABAC_RESET		0x00
#define CABAC_COMMAND		0x01
#define CABAC_STATUS		0x02	/* bit 0 busy, bit 9 the buffer switched, 16-24 bytes */
#define CABAC_PICTURE		0x10
#define CABAC_SLICE		0x11
#define CABAC_HEADER		0x12
#define CABAC_BITS		0x14
#define CABAC_VALUE		0x15
#define CABAC_WIDTH		0x13	/* in macroblocks, less one */
#define CABAC_PENDING		0x16	/* bytes before the engine's first */
#define CABAC_WRITE		0x17

#define CABAC_BUFFER(bit)	((bit) ? 0x014e8e00 : 0x014e8c00)
#define CABAC_BUFFER_SIZE	512

/* the engine's command word (job->unitState) */
#define STATE_OPERATION		0x0000000f
#define STATE_BUFFER		0x00000100
#define STATE_BYTES		0x01ff0000
#define OPERATION_NAL_HEADER	3
#define OPERATION_CONTINUE	0
#define OPERATION_SLICE_INIT	5
#define OPERATION_PICTURE	6
#define OPERATION_FLUSH		11
#define OPERATION_SLICE_HEADER	10
#define OPERATION_ALIGN		15

/* the slice header registers */
#define SLICE_HEADER		((volatile uint32_t *)0x4e9000)
#define SLICE_HEADER_WORDS	8

#define WRITER_ENDED		0x8001

#define I_TYPES			26
#define P_TYPES			5
#define PREFIX_INTRA_IN_P	(I_TYPES + P_TYPES)

/* the VLIW coder's */
uint32_t avcencBinsPut(VideoAvcEncodeJob *job, uint32_t value, int32_t count);	/* 0x87461a */
uint32_t avcencBinsFinish(VideoAvcEncodeJob *job);				/* 0x874692 */
uint32_t avcencSwap32(uint32_t word);						/* 0x87477e */

typedef struct Code {
	uint32_t code;
	int32_t length;
} Code;

/* mb_type in I slices, sub_mb_type, mb_type in P slices, and the prefix of intra in P */
static const Code iTypes[I_TYPES] = {
	{ 0, 1 }, { 32, 6 }, { 33, 6 }, { 34, 6 }, { 35, 6 }, { 72, 7 }, { 73, 7 },
	{ 74, 7 }, { 75, 7 }, { 76, 7 }, { 77, 7 }, { 78, 7 }, { 79, 7 }, { 40, 6 },
	{ 41, 6 }, { 42, 6 }, { 43, 6 }, { 88, 7 }, { 89, 7 }, { 90, 7 }, { 91, 7 },
	{ 92, 7 }, { 93, 7 }, { 94, 7 }, { 95, 7 }, { 3, 2 }
};
static const Code subTypes[4] = { { 1, 1 }, { 0, 2 }, { 3, 3 }, { 2, 3 } };
static const Code pTypes[P_TYPES] = { { 0, 3 }, { 3, 3 }, { 2, 3 }, { 1, 3 }, { 0, 0 } };
static const Code intraInP = { 1, 1 };

/* the coefficients' order: 4x4 blocks, and AC blocks without their DC */
static const uint8_t scan4x4[16] = { 0, 1, 5, 6, 2, 4, 7, 12, 3, 8, 11, 13, 9, 10, 14, 15 };
static const uint8_t scanAc[15] = { 1, 5, 6, 2, 4, 7, 12, 3, 8, 11, 13, 9, 10, 14, 15 };

/* n ones and a zero; at the cut-off, no zero */
static inline void truncatedUnary(uint32_t n, uint32_t cutOff, uint32_t *code, int32_t *length)
{
	if (n) {
		*code = (2 << n) - 2;
		*length = n + 1;
	} else {
		*length = 1;
		*code = 0;
	}
	if (n == cutOff) {
		*code >>= 1;
		(*length)--;
	}
}

/* the k-th order Exp-Golomb suffix of value */
static inline void expGolomb(int32_t value, int32_t k, uint32_t *code, int32_t *length)
{
	*code = 0;
	*length = 0;
	for (;;) {
		*code <<= 1;
		if (value < (1 << k))
			break;
		value -= 1 << k;
		k++;
		*code |= 1;
		(*length)++;
	}
	(*length)++;
	while (k--) {
		*code = (*code << 1) | ((value >> k) & 1);
		(*length)++;
	}
}

/* mvd: a prefix to 9, a third-order suffix, a sign */
void avcencPutMvd(VideoAvcEncodeJob *job, int32_t mvd)
{
	int32_t magnitude = mvd < 0 ? -mvd : mvd;
	uint32_t code;
	int32_t length;

	truncatedUnary(magnitude >= 9 ? 9 : magnitude, 9, &code, &length);
	if (!code) {
		avcencBinsPut(job, code, length);
		return;
	}
	avcencBinsPut(job, code, length);
	if (magnitude >= 9) {
		expGolomb(magnitude - 9, 3, &code, &length);
		avcencBinsPut(job, code, length);
	}
	if (mvd)
		avcencBinsPut(job, mvd < 1, 1);
}

/* coeff_abs_level_minus1: a prefix to 14, a zeroth-order suffix */
void avcencPutLevel(VideoAvcEncodeJob *job, int32_t level)
{
	int32_t magnitude = level < 0 ? -level : level;
	uint32_t code;
	int32_t length;

	truncatedUnary(magnitude >= 14 ? 14 : magnitude, 14, &code, &length);
	if (magnitude < 14) {
		avcencBinsPut(job, code, length);
		return;
	}
	avcencBinsPut(job, code, length);
	expGolomb(magnitude - 14, 0, &code, &length);
	avcencBinsPut(job, code, length);
}

/* coded_block_pattern: the luma bits, the lowest first, then chroma to 2 */
void avcencPutCbp(VideoAvcEncodeJob *job, uint32_t luma, uint32_t chroma)
{
	uint32_t code;
	int32_t length;

	code = (luma & 1) << 3 | ((luma >> 1) & 1) << 2 | ((luma >> 2) & 1) << 1 |
		((luma >> 3) & 1);
	avcencBinsPut(job, code, 4);
	truncatedUnary(chroma, 2, &code, &length);
	avcencBinsPut(job, code, length);
}

/* mb_qp_delta, mapped as se(v) is, in unary */
void avcencPutQpDelta(VideoAvcEncodeJob *job, int32_t delta)
{
	int32_t codeNum = delta >= 1 ? delta * 2 - 1 : -(delta * 2);

	avcencBinsPut(job, (2 << codeNum) - 2, codeNum + 1);
}

/* the 4x4 prediction modes (not for Intra16x16), then the chroma's */
void avcencPutIntraModes(VideoAvcEncodeJob *job, uint32_t intra16x16)
{
	const int8_t *modes;
	uint32_t code, mode;
	int32_t length, i;

	if (!intra16x16) {
		for (i = 0; i < 16; i++) {
			modes = (const int8_t *)job->predModes;
			avcencBinsPut(job, (modes[i] >> 7) & 1, 1);
			if (!((modes[i] >> 7) & 1)) {
				mode = modes[i] & 0xf;
				avcencBinsPut(job, (mode & 1) << 2 | ((mode >> 1) & 1) << 1 |
					((mode >> 2) & 1), 3);
			}
		}
		job->predModes += 16;
	}
	truncatedUnary(*(const uint8_t *)job->chromaModes, 3, &code, &length);
	avcencBinsPut(job, code, length);
	job->chromaModes++;
}

static void putMvdPair(VideoAvcEncodeJob *job)
{
	const int16_t *mvd = (const int16_t *)job->mvds;
	int32_t vertical = mvd[1];

	avcencPutMvd(job, mvd[0]);
	avcencPutMvd(job, vertical);
	job->mvds += 4;
}

/* 16x16: one vector; 16x8 and 8x16: two */
void avcencPutMvds(VideoAvcEncodeJob *job, int32_t partitions)
{
	putMvdPair(job);
	if (partitions >= 1)
		putMvdPair(job);
}

/* 8x8 partitions: their types, then their vectors */
void avcencPutSubMacroblocks(VideoAvcEncodeJob *job)
{
	const VideoAvcMacroblock *mb = (const VideoAvcMacroblock *)job->macroblock;
	int32_t i;

	for (i = 0; i < 4; i++)
		avcencBinsPut(job, subTypes[mb->subTypes[i]].code, subTypes[mb->subTypes[i]].length);
	for (i = 0; i < 4; i++) {
		putMvdPair(job);
		if (mb->subTypes[i] < 1)
			continue;
		putMvdPair(job);
		if (mb->subTypes[i] < 3)
			continue;
		putMvdPair(job);
		putMvdPair(job);
	}
}

int32_t avcencLastCoefficient(const int16_t *coefficients, int32_t count)
{
	int32_t i = count - 1;

	while (i >= 1 && !coefficients[i])
		i--;
	return i;
}

/* the significance map, then the levels from the last */
static inline void putResidual(VideoAvcEncodeJob *job, const int16_t *coefficients,
	int32_t count)
{
	int32_t last = avcencLastCoefficient(coefficients, count);
	int32_t i, value;
	uint32_t significant, isLast;

	for (i = 0; i < count - 1; i++) {
		significant = coefficients[i] != 0;
		avcencBinsPut(job, significant, 1);
		if (significant) {
			isLast = last == i;
			avcencBinsPut(job, isLast, 1);
			if (isLast)
				count = i + 1;
		}
	}
	value = coefficients[count - 1];
	avcencPutLevel(job, (value < 0 ? -value : value) - 1);
	avcencBinsPut(job, value < 0, 1);
	for (i = count - 2; i >= 0; i--) {
		value = coefficients[i];
		if (!value)
			continue;
		avcencPutLevel(job, (value < 0 ? -value : value) - 1);
		avcencBinsPut(job, value < 0, 1);
	}
}

void avcencPutBlock(VideoAvcEncodeJob *job, const int16_t *coefficients)
{
	int16_t block[16];
	int32_t i;

	for (i = 0; i < 16; i++)
		block[scan4x4[i]] = coefficients[i];
	putResidual(job, block, 16);
}

void avcencPutAcBlock(VideoAvcEncodeJob *job, const int16_t *coefficients)
{
	int16_t block[15];
	int32_t i;

	for (i = 0; i < 15; i++)
		block[scanAc[i] - 1] = coefficients[1 + i];
	putResidual(job, block, 15);
}

void avcencPutChromaDc(VideoAvcEncodeJob *job, const int16_t *coefficients)
{
	int16_t block[4];
	int32_t i;

	for (i = 0; i < 4; i++)
		block[i] = coefficients[i];
	putResidual(job, block, 4);
}

/* Intra16x16: the DC block, then the AC blocks when luma is coded */
void avcencPutIntra16x16Residual(VideoAvcEncodeJob *job, const VideoHelperJob *run,
	uint32_t cbpLuma)
{
	uint32_t coded = ((const VideoAvcMacroblock *)job->macroblock)->coded;
	const int16_t *coefficients = (const int16_t *)job->luma;
	uint32_t dc = (coded >> 24) & 1;
	int32_t i;

	avcencBinsPut(job, dc, 1);
	if (dc)
		avcencPutBlock(job, coefficients);
	if (dc || cbpLuma)
		coefficients += 16;
	if (cbpLuma) {
		for (i = 0; i < 16; i++) {
			avcencBinsPut(job, (coded >> i) & 1, 1);
			if ((coded >> i) & 1) {
				avcencPutAcBlock(job, coefficients);
				coefficients += 16;
			}
		}
	}
	job->luma = (uint32_t)coefficients;
}

/* the coded 8x8 blocks' 4x4 blocks */
void avcencPutLumaResidual(VideoAvcEncodeJob *job, const VideoHelperJob *run, uint32_t cbpLuma)
{
	uint32_t coded = ((const VideoAvcMacroblock *)job->macroblock)->coded;
	const int16_t *coefficients = (const int16_t *)job->luma;
	int32_t i, j;

	if (!cbpLuma)
		return;
	for (i = 0; i < 4; i++) {
		if (!((cbpLuma >> i) & 1))
			continue;
		for (j = i * 4; j < i * 4 + 4; j++) {
			avcencBinsPut(job, (coded >> j) & 1, 1);
			if ((coded >> j) & 1) {
				avcencPutBlock(job, coefficients);
				coefficients += 16;
			}
		}
	}
	job->luma = (uint32_t)coefficients;
}

/* chroma: the DC blocks (cbp 1, 2), then the AC blocks (cbp 2) */
void avcencPutChromaResidual(VideoAvcEncodeJob *job, const VideoHelperJob *run,
	uint32_t cbpChroma)
{
	uint32_t coded = ((const VideoAvcMacroblock *)job->macroblock)->coded;
	const int16_t *cb = (const int16_t *)job->cb;
	const int16_t *cr = (const int16_t *)job->cr;
	uint32_t acCb = (coded >> 16) & 0xf;
	uint32_t acCr = (coded >> 20) & 0xf;
	uint32_t bit;
	int32_t i;

	if (cbpChroma & 3) {
		bit = (coded >> 25) & 1;
		avcencBinsPut(job, bit, 1);
		if (bit) {
			avcencPutChromaDc(job, cb);
			cb += 4;
		} else if (cbpChroma == 2) {
			cb += 4;
		}
		bit = (coded >> 26) & 1;
		avcencBinsPut(job, bit, 1);
		if (bit) {
			avcencPutChromaDc(job, cr);
			cr += 4;
		} else if (cbpChroma == 2) {
			cr += 4;
		}
	}
	if (cbpChroma & 2) {
		for (i = 0; i < 4; i++) {
			avcencBinsPut(job, (acCb >> i) & 1, 1);
			if ((acCb >> i) & 1) {
				avcencPutAcBlock(job, cb);
				cb += 16;
			}
		}
		if (!acCb && !((coded >> 25) & 1))
			cb -= 4;
		for (i = 0; i < 4; i++) {
			avcencBinsPut(job, (acCr >> i) & 1, 1);
			if ((acCr >> i) & 1) {
				avcencPutAcBlock(job, cr);
				cr += 16;
			}
		}
		if (!acCr && !((coded >> 26) & 1))
			cr -= 4;
	}
	job->cr = (uint32_t)cr;
	job->cb = (uint32_t)cb;
}

/* the coded block pattern of an Intra16x16 type */
static inline void intra16x16Cbp(int32_t type, uint32_t *luma, uint32_t *chroma)
{
	if (type >= 13) {
		*luma = 15;
		*chroma = (type - 13) >> 2;
	} else {
		*luma = 0;
		*chroma = (type - 1) >> 2;
	}
}

void avcencPutIntraMacroblock(VideoAvcEncodeJob *job, const VideoHelperJob *run)
{
	const VideoAvcMacroblock *mb = (const VideoAvcMacroblock *)job->macroblock;
	int32_t type = mb->type;
	uint32_t cbp = mb->cbp;
	int32_t qpDelta = mb->qpDelta;
	uint32_t luma, chroma;

	avcencBinsPut(job, iTypes[type].code, iTypes[type].length);
	avcencPutIntraModes(job, type);
	if (type >= 1) {
		avcencPutQpDelta(job, qpDelta);
		intra16x16Cbp(type, &luma, &chroma);
		avcencPutIntra16x16Residual(job, run, luma);
	} else {
		luma = cbp & 0xf;
		chroma = (cbp >> 4) & 3;
		avcencPutCbp(job, luma, chroma);
		if (cbp)
			avcencPutQpDelta(job, qpDelta);
		avcencPutLumaResidual(job, run, luma);
	}
	avcencPutChromaResidual(job, run, chroma);
}

void avcencPutInterMacroblock(VideoAvcEncodeJob *job, const VideoHelperJob *run)
{
	const VideoAvcMacroblock *mb = (const VideoAvcMacroblock *)job->macroblock;
	int32_t type = mb->type;
	uint32_t cbp = mb->cbp;
	int32_t qpDelta = mb->qpDelta;
	int32_t intra = type - 5;
	uint32_t code, luma, chroma;
	int32_t length;

	if (type >= 5) {
		code = iTypes[intra].code | (intraInP.code << iTypes[intra].length);
		length = iTypes[intra].length + intraInP.length;
	} else {
		code = pTypes[type].code;
		length = pTypes[type].length;
	}
	avcencBinsPut(job, code, length);
	if (intra >= 1) {
		avcencPutIntraModes(job, intra);
		avcencPutQpDelta(job, qpDelta);
		intra16x16Cbp(intra, &luma, &chroma);
		avcencPutIntra16x16Residual(job, run, luma);
		avcencPutChromaResidual(job, run, chroma);
		return;
	}
	luma = cbp & 0xf;
	chroma = (cbp >> 4) & 3;
	if (intra == 0)
		avcencPutIntraModes(job, 0);
	else if (type < 3)
		avcencPutMvds(job, type);
	else
		avcencPutSubMacroblocks(job);
	avcencPutCbp(job, luma, chroma);
	if (cbp)
		avcencPutQpDelta(job, qpDelta);
	avcencPutLumaResidual(job, run, luma);
	avcencPutChromaResidual(job, run, chroma);
}

void avcencRowBegin(VideoAvcEncodeJob *job, const VideoHelperJob *run, uint32_t row);

/* end_of_slice_flag after each macroblock; the slice ends with the job's last */
static uint32_t endOfSlice(VideoAvcEncodeJob *job, const VideoHelperJob *run,
	const VideoAvcWriter *stream, int32_t written, uint32_t end)
{
	if (run->sliceUsed && (written == (uint16_t)run->count ||
	    (uint16_t)run->first + written == stream->macroblocks))
		end = 1;
	if (!avcencBinsPut(job, end, 1) && !end)
		videoLongJump(job->jump, WRITER_ENDED);
	return end;
}

static void sliceEnd(VideoAvcEncodeJob *job, VideoAvcWriter *stream, uint32_t end)
{
	if (end && avcencBinsFinish(job) != 1 && end == 1)
		videoLongJump(job->jump, WRITER_ENDED);
	stream->sliceState[0] = 0;
	stream->sliceState[1] = 0;
}

void avcencPutISlice(VideoAvcEncodeJob *job, const VideoHelperJob *run)
{
	VideoAvcWriter *stream = job->stream;
	int32_t width = stream->widthInMacroblocks;
	int32_t count = (uint16_t)run->count;
	int32_t x = run->firstX;
	int32_t written = 0;
	uint32_t row = 0, end = 0;

	if (written < count) {
		do {
			avcencRowBegin(job, run, row);
			for (; x < width; x++) {
				written++;
				avcencPutIntraMacroblock(job, run);
				end = endOfSlice(job, run, stream, written, end);
				job->macroblock += sizeof(VideoAvcMacroblock);
			}
			row++;
			x = 0;
		} while (width * row < count);
	}
	sliceEnd(job, stream, end);
}

void avcencPutPSlice(VideoAvcEncodeJob *job, const VideoHelperJob *run)
{
	VideoAvcWriter *stream = job->stream;
	int32_t width = stream->widthInMacroblocks;
	int32_t count = (uint16_t)run->count;
	int32_t x = run->firstX;
	int32_t written = 0;
	uint32_t row = 0, end = 0, skip;

	if (written < count) {
		do {
			avcencRowBegin(job, run, row);
			for (; x < width; x++) {
				skip = ((const VideoAvcMacroblock *)job->macroblock)->skip;
				avcencBinsPut(job, skip, 1);
				written++;
				if (!skip)
					avcencPutInterMacroblock(job, run);
				end = endOfSlice(job, run, stream, written, end);
				job->macroblock += sizeof(VideoAvcMacroblock);
			}
			row++;
			x = 0;
		} while (width * row < count);
	}
	sliceEnd(job, stream, end);
}

void avcencSliceData(VideoAvcEncodeJob *job, const VideoHelperJob *run)
{
	if ((job->stream->sliceType & 7) == 2)
		avcencPutISlice(job, run);
	else
		avcencPutPSlice(job, run);
}

/* the row's buffers: the job's, a row's strides on */
void avcencRowBegin(VideoAvcEncodeJob *job, const VideoHelperJob *run, uint32_t row)
{
	const VideoAvcWriter *stream = job->stream;
	const int16_t *strides = stream->unitParameters;

	job->macroblock = run->buffers[0] + row * (uint16_t)strides[0];
	job->predModes = run->buffers[1] + row * (uint16_t)strides[1];
	job->chromaModes = run->buffers[2] + row * (uint16_t)strides[2];
	job->rowBuffer3 = run->buffers[3] + row * (uint16_t)strides[3];
	job->mvds = run->buffers[4] + row * (uint16_t)strides[4];
	job->luma = run->buffers[5] + row * (uint16_t)strides[5];
	job->cb = run->buffers[6] + row * (uint16_t)strides[6];
	job->cr = run->buffers[7] + row * (uint16_t)strides[6];
}

/* the engine's byte count into the state; or its buffer switched */
void avcencCabacSync(VideoAvcEncodeJob *job, uint32_t keep)
{
	uint32_t status = CABAC[CABAC_STATUS];
	uint32_t bytes = (status >> 16) & 0x1ff;

	if (!keep) {
		job->unitState &= ~STATE_BYTES;
		job->unitState &= ~STATE_BUFFER;
	} else if ((status >> 8) & 2) {
		job->unitState = (job->unitState & ~STATE_BUFFER) |
			((~(job->unitState >> 8) & 1) << 8);
		job->unitState &= ~STATE_BYTES;
	} else {
		job->unitState = (job->unitState & ~STATE_BYTES) | ((bytes & 0x1ff) << 16);
	}
}

/* the slice header's fields for the engine */
void avcencCabacSliceRegisters(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb)
{
	const VideoAvcPictureInfo *picture = job->picture;
	const VideoAvcWriter *stream = job->stream;
	uint32_t type = options->idr ? 5 : 1;
	uint32_t sliceType = stream->sliceType & 7;
	uint32_t codedType = sliceType < 5 ? sliceType + 5 : sliceType;
	int32_t qpDelta = picture->qp - 26;
	int32_t alpha = picture->alphaOffset >> 1;
	int32_t beta = picture->betaOffset >> 1;
	uint32_t frameNum = stream->frameNum;
	uint32_t idrPicId = stream->idrPicId;
	int32_t i;

	for (i = 0; i < SLICE_HEADER_WORDS; i++)
		SLICE_HEADER[i] = 0;
	SLICE_HEADER[0] |= 0x4;
	if (type == 5)
		SLICE_HEADER[0] |= 0x8;
	else
		SLICE_HEADER[0] &= ~0x8;
	SLICE_HEADER[0] = (SLICE_HEADER[0] & ~0xfff0) | ((codedType & 0xf) << 12);
	SLICE_HEADER[0] = (SLICE_HEADER[0] & 0xe000ffff) | ((firstMb & 0x1fff) << 16);
	SLICE_HEADER[0] &= ~0x60000000;
	SLICE_HEADER[0] &= ~0x80000000;
	SLICE_HEADER[1] = (SLICE_HEADER[1] & 0x3fff) | (frameNum << 16);
	SLICE_HEADER[2] = idrPicId << 16;
	SLICE_HEADER[3] &= 0xffff;
	SLICE_HEADER[4] &= 0xffff;
	SLICE_HEADER[5] &= 0xffff;
	SLICE_HEADER[6] &= 0x00000fff;
	SLICE_HEADER[7] = (SLICE_HEADER[7] & ~0x7f800) | ((beta & 0xf) << 11) | ((alpha & 0xf) << 15);
	SLICE_HEADER[7] &= ~0x180000;
	SLICE_HEADER[7] = (SLICE_HEADER[7] & ~0x0fe00000) | ((qpDelta & 0x7f) << 21);
	if (sliceType == 2)
		SLICE_HEADER[7] |= 0x30000000;
	else
		SLICE_HEADER[7] &= ~0x30000000;
	SLICE_HEADER[7] &= ~0xc0000000;
}

/* the engine's other buffer, whole, into the stream */
void avcencCabacCopyBlock(VideoAvcEncodeJob *job)
{
	uint32_t buffer = CABAC_BUFFER(!((job->unitState >> 8) & 1));

	vnzCachePurge(buffer, CABAC_BUFFER_SIZE);
	avcencBitsCopyBlock(job->writer, (const uint32_t *)buffer);
}

/* the engine's buffer, size bytes of it, into the stream */
void avcencCabacCopy(VideoAvcEncodeJob *job, int32_t size)
{
	uint32_t buffer = CABAC_BUFFER((job->unitState >> 8) & 1);

	vnzCachePurge(buffer, CABAC_BUFFER_SIZE);
	avcencBitsCopy(job->writer, (const uint32_t *)buffer, size);
}

static void cabacRun(const VideoAvcEncodeJob *job)
{
	CABAC[CABAC_COMMAND] = job->unitState;
	while (CABAC[CABAC_STATUS] & 1)
		;
}

void avcencCabacBegin(VideoAvcEncodeJob *job)
{
	job->unitState = 0;
	CABAC[CABAC_RESET] = 1;
	CABAC[CABAC_PENDING] = 0;
}

/* the stream's bytes since its last 8-byte boundary go to the engine first */
void avcencCabacAlign(VideoAvcEncodeJob *job)
{
	VideoAvcWriter *writer = job->writer;
	uint32_t *buffer;
	uint32_t bytes;

	avcencCabacSync(job, 0);
	bytes = writer->position & 7;
	job->unitState = (job->unitState & ~STATE_BYTES) | ((bytes & 0x1ff) << 16);
	if (bytes) {
		buffer = (uint32_t *)CABAC_BUFFER((job->unitState >> 8) & 1);
		vnzCachePurge((uint32_t)buffer, CABAC_BUFFER_SIZE);
		buffer[0] = avcencSwap32(*(const uint32_t *)(writer->position - bytes));
		if (bytes >= 5)
			buffer[1] = avcencSwap32(*(const uint32_t *)(writer->position - bytes + 4));
		vnzCacheFlush((uint32_t)buffer, CABAC_BUFFER_SIZE);
		__asm__ __volatile__("syncm" ::: "memory");
		CABAC[CABAC_PENDING] = bytes;
		writer->position -= bytes;
		writer->job += bytes;
	}
	job->unitState |= OPERATION_ALIGN;
	CABAC[CABAC_COMMAND] = job->unitState;
	while (CABAC[CABAC_STATUS] & 1)
		;
}

void avcencCabacNalHeader(VideoAvcEncodeJob *job, uint32_t header)
{
	avcencCabacSync(job, 1);
	job->unitState = (job->unitState & ~STATE_OPERATION) | OPERATION_NAL_HEADER;
	CABAC[CABAC_BITS] = 8;
	CABAC[CABAC_VALUE] = header;
	CABAC[CABAC_WRITE] = 1;
	cabacRun(job);
}

void avcencCabacSliceHeader(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb)
{
	const VideoAvcWriter *stream = job->stream;
	uint32_t log2;

	avcencCabacSliceRegisters(options, job, firstMb);
	log2 = stream->log2MaxFrameNum;
	avcencCabacSync(job, 1);
	log2 -= 4;
	job->unitState = (job->unitState & ~STATE_OPERATION) | OPERATION_SLICE_HEADER;
	CABAC[CABAC_HEADER] = 0x10000000 | ((log2 & 0xf) << 23) | 0x400000 | 0x20000 | 1;
	cabacRun(job);
}

void avcencCabacSliceInit(VideoAvcEncodeJob *job, uint32_t sliceType)
{
	const VideoAvcPictureInfo *picture = job->picture;
	uint32_t word;

	avcencCabacSync(job, 1);
	job->unitState = (job->unitState & ~STATE_OPERATION) | OPERATION_SLICE_INIT;
	word = (sliceType & 3) << 8;
	if ((sliceType & 7) == 2)
		word |= 0xc0;
	else
		word &= ~0xc0;
	word = ((word & ~0x3f) | (picture->qp & 0x3f)) & ~0xfc00;
	CABAC[CABAC_SLICE] = word;
	cabacRun(job);
}

/* the engine filled a buffer: on with the next */
void avcencCabacResume(VideoAvcEncodeJob *job)
{
	const VideoAvcWriter *stream = job->stream;
	uint32_t sliceType = stream->sliceType & 7;
	uint32_t status, word = 0;

	avcencCabacSync(job, 1);
	status = CABAC[CABAC_STATUS];
	if ((status >> 8) & 3) {
		job->unitState &= ~STATE_OPERATION;
	} else {
		job->unitState = (job->unitState & ~STATE_OPERATION) | OPERATION_PICTURE;
		word = (word & ~0xd & ~0x30) | ((sliceType & 3) << 4);
		word &= ~0xc0;
		CABAC[CABAC_PICTURE] = word;
		CABAC[CABAC_WIDTH] = stream->widthInMacroblocks - 1;
	}
	CABAC[CABAC_BITS] = 0xffffffff;
	CABAC[CABAC_COMMAND] = job->unitState;
}

/* the engine's last bytes into the stream, each full buffer as it fills */
void avcencCabacFlush(VideoAvcEncodeJob *job)
{
	uint32_t status, pending;

	avcencCabacSync(job, 1);
	job->unitState = (job->unitState & ~STATE_OPERATION) | OPERATION_FLUSH;
	CABAC[CABAC_BITS] = 0;
	CABAC[CABAC_COMMAND] = job->unitState;
	while ((status = CABAC[CABAC_STATUS]) & 1)
		;
	while (((status >> 8) & 3) == 2) {
		avcencCabacResume(job);
		avcencCabacCopyBlock(job);
		while ((status = CABAC[CABAC_STATUS]) & 1)
			;
	}
	pending = CABAC[CABAC_PENDING];
	if ((status >> 16) & 0x1ff) {
		if ((status >> 8) & 2)
			avcencCabacCopyBlock(job);
		else
			avcencCabacCopy(job, pending & 0x1ff);
	}
}

#define RUN_TYPES_SIZE(count)		((count) * 16)
#define RUN_MODES_SIZE(count)		((count) * 16)
#define RUN_CHROMA_SIZE(count)		(count)
#define RUN_MVDS_SIZE(count)		((count) * 128)
#define RUN_LUMA_SIZE(count)		((count) * 17 * 32)
#define RUN_CHROMA_COEFFICIENTS_SIZE(count)	((count) * 17 * 8)

/* the run's macroblock buffers, to the encoder and back */
void avcencRunOpen(const VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoHelperJob *run)
{
	uint32_t count = run->commandSize;

	run->buffers[0] = vnzPublicOpenPrivate(run->buffers[0], RUN_TYPES_SIZE(count));
	run->buffers[1] = vnzPublicOpenPrivate(run->buffers[1], RUN_MODES_SIZE(count));
	run->buffers[2] = vnzPublicOpenPrivate(run->buffers[2], RUN_CHROMA_SIZE(count));
	run->buffers[4] = vnzPublicOpenPrivate(run->buffers[4], RUN_MVDS_SIZE(count));
	run->buffers[5] = vnzPublicOpenPrivate(run->buffers[5], RUN_LUMA_SIZE(count));
	run->buffers[6] = vnzPublicOpenPrivate(run->buffers[6],
		RUN_CHROMA_COEFFICIENTS_SIZE(count));
	run->buffers[7] = vnzPublicOpenPrivate(run->buffers[7],
		RUN_CHROMA_COEFFICIENTS_SIZE(count));
}

void avcencRunClose(const VideoHelperShared *shared, VideoAvcEncodeJob *job,
	VideoHelperJob *run)
{
	uint32_t count = run->commandSize;

	run->buffers[0] = vnzPublicClosePrivate(run->buffers[0], RUN_TYPES_SIZE(count));
	run->buffers[1] = vnzPublicClosePrivate(run->buffers[1], RUN_MODES_SIZE(count));
	run->buffers[2] = vnzPublicClosePrivate(run->buffers[2], RUN_CHROMA_SIZE(count));
	run->buffers[4] = vnzPublicClosePrivate(run->buffers[4], RUN_MVDS_SIZE(count));
	run->buffers[5] = vnzPublicClosePrivate(run->buffers[5], RUN_LUMA_SIZE(count));
	run->buffers[6] = vnzPublicClosePrivate(run->buffers[6],
		RUN_CHROMA_COEFFICIENTS_SIZE(count));
	run->buffers[7] = vnzPublicClosePrivate(run->buffers[7],
		RUN_CHROMA_COEFFICIENTS_SIZE(count));
}

/*
 * A skipped picture's slice data in CABAC, coded here: every macroblock's
 * mb_skip_flag and end_of_slice_flag (H.264 9.3.4).
 */
static const uint8_t rangeTabLps[64][4] = {
	{ 128, 176, 208, 240 }, { 128, 167, 197, 227 }, { 128, 158, 187, 216 }, { 123, 150, 178, 205 },
	{ 116, 142, 169, 195 }, { 111, 135, 160, 185 }, { 105, 128, 152, 175 }, { 100, 122, 144, 166 },
	{  95, 116, 137, 158 }, {  90, 110, 130, 150 }, {  85, 104, 123, 142 }, {  81,  99, 117, 135 },
	{  77,  94, 111, 128 }, {  73,  89, 105, 122 }, {  69,  85, 100, 116 }, {  66,  80,  95, 110 },
	{  62,  76,  90, 104 }, {  59,  72,  86,  99 }, {  56,  69,  81,  94 }, {  53,  65,  77,  89 },
	{  51,  62,  73,  85 }, {  48,  59,  69,  80 }, {  46,  56,  66,  76 }, {  43,  53,  63,  72 },
	{  41,  50,  59,  69 }, {  39,  48,  56,  65 }, {  37,  45,  54,  62 }, {  35,  43,  51,  59 },
	{  33,  41,  48,  56 }, {  32,  39,  46,  53 }, {  30,  37,  43,  50 }, {  29,  35,  41,  48 },
	{  27,  33,  39,  45 }, {  26,  31,  37,  43 }, {  24,  30,  35,  41 }, {  23,  28,  33,  39 },
	{  22,  27,  32,  37 }, {  21,  26,  30,  35 }, {  20,  24,  29,  33 }, {  19,  23,  27,  31 },
	{  18,  22,  26,  30 }, {  17,  21,  25,  28 }, {  16,  20,  23,  27 }, {  15,  19,  22,  25 },
	{  14,  18,  21,  24 }, {  14,  17,  20,  23 }, {  13,  16,  19,  22 }, {  12,  15,  18,  21 },
	{  12,  14,  17,  20 }, {  11,  14,  16,  19 }, {  11,  13,  15,  18 }, {  10,  12,  15,  17 },
	{  10,  12,  14,  16 }, {   9,  11,  13,  15 }, {   9,  11,  12,  14 }, {   8,  10,  12,  14 },
	{   8,   9,  11,  13 }, {   7,   9,  11,  12 }, {   7,   9,  10,  12 }, {   7,   8,  10,  11 },
	{   6,   8,   9,  11 }, {   6,   7,   9,  10 }, {   6,   7,   8,   9 }, {   2,   2,   2,   2 }
};

static const uint8_t transIdxLps[64] = {
	 0,  0,  1,  2,  2,  4,  4,  5,  6,  7,  8,  9,  9, 11, 11, 12,
	13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
	24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
	33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63
};

static const uint8_t transIdxMps[64] = {
	 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63
};

#define SKIP_FLAG_STATE		13	/* pStateIdx 6, valMPS 1 */
#define SKIP_FLAG_CONTEXT	11	/* ctxIdx of mb_skip_flag in P slices */

uint32_t avcencCabacSkipMacroblock(VideoAvcEncodeJob *job);
uint32_t avcencCabacSkipFlag(VideoAvcEncodeJob *job);
uint32_t avcencCabacEndOfSlice(VideoAvcEncodeJob *job);
void avcencCabacDecision(VideoAvcEncodeJob *job, uint32_t context, uint32_t bin);
void avcencCabacRenormalize(VideoAvcEncodeJob *job);
void avcencCabacPutBit(VideoAvcEncodeJob *job, uint32_t bit);
void avcencCabacTerminate(VideoAvcEncodeJob *job, uint32_t bin);
void avcencCabacEncodeFlush(VideoAvcEncodeJob *job);

/* InitEncoder (9.3.4.1) */
void avcencCabacReset(VideoAvcEncodeJob *job, uint32_t value)
{
	VideoAvcCabacCoder *coder = job->coder;

	coder->state = SKIP_FLAG_STATE;
	coder->outstanding = 0;
	coder->firstBit = 1;
	coder->range = 510;
	coder->low = 0;
}

void avcencCabacInit(VideoAvcEncodeJob *job, uint32_t sliceType)
{
	VideoAvcCabacCoder *coder = job->coder;

	coder->x = 0;
	coder->lastX = job->stream->widthInMacroblocks - 1;
}

/* every macroblock skipped, to the slice's end */
void avcencCabacSkipped(VideoAvcEncodeJob *job, uint32_t sliceType)
{
	VideoAvcCabacCoder *coder = job->coder;

	if (sliceType >= 5)
		sliceType -= 5;
	coder->count = 0;
	if (sliceType == 0)
		while (!avcencCabacSkipMacroblock(job))
			;
}

uint32_t avcencCabacSkipMacroblock(VideoAvcEncodeJob *job)
{
	avcencCabacSkipFlag(job);
	return avcencCabacEndOfSlice(job);
}

uint32_t avcencCabacSkipFlag(VideoAvcEncodeJob *job)
{
	avcencCabacDecision(job, SKIP_FLAG_CONTEXT, 1);
	return 1;
}

uint32_t avcencCabacEndOfSlice(VideoAvcEncodeJob *job)
{
	VideoAvcCabacCoder *coder = job->coder;
	uint32_t last = coder->count == job->skipRun - 1;

	avcencCabacTerminate(job, last);
	if (coder->x < coder->lastX)
		coder->x++;
	else
		coder->x = 0;
	coder->count++;
	return last;
}

/* EncodeDecision (9.3.4.2), on the coder's one context */
void avcencCabacDecision(VideoAvcEncodeJob *job, uint32_t context, uint32_t bin)
{
	VideoAvcCabacCoder *coder = job->coder;
	uint32_t pStateIdx = coder->state >> 1;
	uint32_t valMps = coder->state & 1;
	uint32_t rangeLps = rangeTabLps[pStateIdx][(coder->range >> 6) & 3];

	coder->range -= rangeLps;
	if (bin != valMps) {
		coder->low += coder->range;
		coder->range = rangeLps;
		if (pStateIdx == 0)
			valMps = 1 - valMps;
		coder->state = (transIdxLps[pStateIdx] << 1) | valMps;
	} else {
		coder->state = (transIdxMps[pStateIdx] << 1) | valMps;
	}
	avcencCabacRenormalize(job);
}

/* RenormE (9.3.4.3) */
void avcencCabacRenormalize(VideoAvcEncodeJob *job)
{
	VideoAvcCabacCoder *coder = job->coder;

	while (coder->range < 256) {
		if (coder->low < 256) {
			avcencCabacPutBit(job, 0);
		} else if (coder->low >= 512) {
			coder->low -= 512;
			avcencCabacPutBit(job, 1);
		} else {
			coder->low -= 256;
			coder->outstanding++;
		}
		coder->range <<= 1;
		coder->low <<= 1;
	}
}

/* PutBit (9.3.4.3) */
void avcencCabacPutBit(VideoAvcEncodeJob *job, uint32_t bit)
{
	VideoAvcCabacCoder *coder = job->coder;
	VideoAvcWriter *writer = job->writer;

	if (coder->firstBit)
		coder->firstBit = 0;
	else
		avcencBitsPut(writer, 1, bit);
	while (coder->outstanding > 0) {
		avcencBitsPut(writer, 1, 1 - bit);
		coder->outstanding--;
	}
}

/* EncodeTerminate (9.3.4.5) */
void avcencCabacTerminate(VideoAvcEncodeJob *job, uint32_t bin)
{
	VideoAvcCabacCoder *coder = job->coder;

	coder->range -= 2;
	if (bin) {
		coder->low += coder->range;
		avcencCabacEncodeFlush(job);
	} else {
		avcencCabacRenormalize(job);
	}
}

/* EncodeFlush (9.3.4.6) */
void avcencCabacEncodeFlush(VideoAvcEncodeJob *job)
{
	VideoAvcCabacCoder *coder = job->coder;
	VideoAvcWriter *writer = job->writer;

	coder->range = 2;
	avcencCabacRenormalize(job);
	avcencCabacPutBit(job, (coder->low >> 9) & 1);
	avcencBitsPut(writer, 2, ((coder->low >> 7) & 3) | 1);
}
