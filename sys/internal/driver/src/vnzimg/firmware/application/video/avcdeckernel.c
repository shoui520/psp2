/*
 * The AVC decoder's motion and deblocking kernels (0x892a60-0x894f40): VLIW
 * code the stripes' jobs call, handing them their state in the IVC2
 * coprocessor's registers (here an AvcdecMotion, AvcdecLanes). Each IVC2
 * operation is written as what it does to its lanes.
 */
#include "avcdecdecoder.h"

/* QPc by qPI (Table 8-15) */
static const uint8_t chromaQp[52] = {				/* 0x8f72b4 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35, 35,
	36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39,
};

/* the lanes of the strengths' edges: by edge, the 4x4 blocks (z order) along it */
static const uint8_t verticalBlocks[16] = {
	0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15,
};

static const uint8_t horizontalBlocks[16] = {
	0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15,
};

static inline AvcdecLanes lanesZero(void)
{
	AvcdecLanes zero;

	zero.all = 0;
	return zero;
}

/* 0x892a60: the macroblock's vectors kept, by 4x4 block */
void avcdecStoreVectors(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	AvcdecMbVectors *vectors = stripe->vectors;
	uint32_t i, j;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, vectors);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, (uint8_t *)vectors + 64);
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			vectors->mv[i * 4 + j][0] = motion->mv[i][j].w[0];
			vectors->mv[i * 4 + j][1] = motion->mv[i][j].w[1];
		}
	}
}

/* 0x892b08: a B macroblock's: no vector for the lists an 8x8 block does not use */
void avcdecStoreVectorsB(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	uint32_t unused, i, j;

	for (i = 0; i < 4; i++) {
		unused = lanesNegative(lanesOfBlock(motion->refIdx, i));
		for (j = 0; j < 4; j++)
			motion->mv[i][j] = lanesSelect(unused, lanesZero(), motion->mv[i][j]);
	}
	avcdecStoreVectors(stripe, motion);
}

/*
 * 0x892c08: the macroblock's motion kept for the B pictures that will take
 * it as their colocated one: list 0's, else list 1's
 */
void avcdecStoreColocated(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	AvcdecColocated *colocated = stripe->colocatedOut;
	AvcdecLanes refs = motion->refIdx, swapped, pictures;
	uint32_t none, refIdxZero, i, j, mv[4];

	vnzCacheLine(VNZ_CACHE_INVALIDATE, colocated);
	none = lanesNegative(refs);
	swapped.w[0] = refs.w[1];
	swapped.w[1] = refs.w[0];
	refs = lanesSelect(none, swapped, refs);
	pictures = stripe->mb->refPicture;
	swapped.w[0] = pictures.w[1];
	swapped.w[1] = pictures.w[0];
	pictures = lanesSelect(none, swapped, pictures);
	refIdxZero = lanesEqual(lanesZero(), refs) & 0xf;
	if (stripe->sliceHeader & AVC_SLICE_DIRECT_8X8) {
		/* the corners: 4x4 blocks 0, 5, 10 and 15 */
		for (i = 0; i < 4; i++)
			colocated->corners.pictures[i] = pictures.b[i];
		colocated->corners.refIdxZero = refIdxZero;
		for (i = 0; i < 4; i++)
			colocated->corners.mv[i] = motion->refIdx.b[i] < 0 ?
				motion->mv[i][i].w[1] : motion->mv[i][i].w[0];
		return;
	}
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			mv[j] = motion->refIdx.b[i] < 0 ? motion->mv[i][j].w[1] :
				motion->mv[i][j].w[0];
		colocated->blocks[i][0] = mv[0];
		colocated->blocks[i][1] = mv[1];
		colocated->blocks[i][2] = mv[2] << COLOCATED_SHIFT | (pictures.b[i] & 31);
		colocated->blocks[i][3] = mv[3] << COLOCATED_SHIFT | refIdxZero;
	}
}

/*
 * The median of the neighbours' vectors (8.4.1.3.1), or the one whose
 * reference is the partition's when only one is; the neighbours without
 * the reference give no vector.
 */
static inline AvcdecLanes median(AvcdecMotion *motion)
{
	AvcdecLanes result, low, high, match;
	uint32_t i, one;
	int8_t count[8];

	motion->mvA = lanesSelect(lanesNegative(motion->refA), lanesZero(), motion->mvA);
	motion->mvB = lanesSelect(lanesNegative(motion->refB), lanesZero(), motion->mvB);
	motion->mvC = lanesSelect(lanesNegative(motion->refC), lanesZero(), motion->mvC);
	for (i = 0; i < 4; i++) {
		low.h[i] = motion->mvA.h[i] < motion->mvB.h[i] ? motion->mvA.h[i] :
			motion->mvB.h[i];
		high.h[i] = motion->mvA.h[i] > motion->mvB.h[i] ? motion->mvA.h[i] :
			motion->mvB.h[i];
		if (motion->mvC.h[i] < high.h[i])
			high.h[i] = motion->mvC.h[i];
		result.h[i] = high.h[i] > low.h[i] ? high.h[i] : low.h[i];
	}
	for (i = 0; i < 8; i++)
		count[i] = 0;
	for (i = 0; i < 8; i++) {
		if (motion->current.b[i] == motion->refA.b[i])
			count[i]++;
		if (motion->current.b[i] == motion->refB.b[i])
			count[i]++;
		if (motion->current.b[i] == motion->refC.b[i])
			count[i]++;
	}
	match = lanesSelect(lanesEqual(motion->current, motion->refB), motion->mvB,
		motion->mvA);
	match = lanesSelect(lanesEqual(motion->current, motion->refC), motion->mvC, match);
	motion->mvA = match;
	one = 0;
	for (i = 0; i < 8; i++)
		if (count[i] == 1)
			one |= 1 << i;
	return lanesSelect(one, match, result);
}

/* 0x893198: the prediction in the lanes given, the others' kept */
void avcdecMvPredictLanes(AvcdecMotion *motion, uint32_t lanes)
{
	AvcdecLanes predicted = median(motion);

	motion->predictor = lanesSelect(lanes, predicted, motion->predictor);
}

/* 0x893200: the left's vector when it alone is available, else the median */
void avcdecMvPredict(AvcdecMotion *motion, uint32_t onlyA)
{
	if (onlyA) {
		motion->predictor = lanesSelect(lanesNegative(motion->refA), lanesZero(),
			motion->mvA);
		return;
	}
	motion->predictor = median(motion);
}

/*
 * 0x893278: the left's vector when it alone is available, or when neither
 * the one above nor the one above right has the partition's reference;
 * else the median
 */
void avcdecMvPredictUnlessA(AvcdecMotion *motion, uint32_t onlyA)
{
	AvcdecLanes low, high, result;
	uint32_t neither, i;

	if (onlyA) {
		motion->predictor = motion->mvA;
		return;
	}
	motion->mvB = lanesSelect(lanesNegative(motion->refB), lanesZero(), motion->mvB);
	motion->mvC = lanesSelect(lanesNegative(motion->refC), lanesZero(), motion->mvC);
	for (i = 0; i < 4; i++) {
		low.h[i] = motion->mvA.h[i] < motion->mvB.h[i] ? motion->mvA.h[i] :
			motion->mvB.h[i];
		high.h[i] = motion->mvA.h[i] > motion->mvB.h[i] ? motion->mvA.h[i] :
			motion->mvB.h[i];
		if (motion->mvC.h[i] < high.h[i])
			high.h[i] = motion->mvC.h[i];
		result.h[i] = high.h[i] > low.h[i] ? high.h[i] : low.h[i];
	}
	neither = lanesDiffer(motion->current, motion->refB) &
		lanesDiffer(motion->current, motion->refC);
	motion->predictor = lanesSelect(neither, motion->mvA, result);
}

/* 0x8932c0: the median */
void avcdecMvMedian(AvcdecMotion *motion)
{
	motion->predictor = median(motion);
}

/*
 * 0x893f68: the strengths where coefficients are (2), and on the edges next
 * to an intra macroblock (4): the vertical edges' lanes, then the
 * horizontal ones'
 */
void avcdecStrengthsCoded(uint32_t vertical, uint32_t horizontal, uint32_t leftIntra,
	uint32_t topIntra, AvcdecLanes strengths[4])
{
	uint32_t i;

	for (i = 0; i < 16; i++) {
		strengths[i / 8].b[i % 8] = (vertical >> verticalBlocks[i]) & 1 ? 2 : 0;
		strengths[2 + i / 8].b[i % 8] = (horizontal >> horizontalBlocks[i]) & 1 ? 2 : 0;
	}
	for (i = 0; i < 8; i++) {
		if (leftIntra & (1 << i))
			strengths[0].b[i] = 4;
		if (topIntra & (1 << i))
			strengths[2].b[i] = 4;
	}
}

/*
 * 0x894fd8: the macroblock's QPc, and the filter's indexA and indexB
 * (8.7.2.2) on its left edge, inside it and on its top edge, for luma and
 * chroma
 */
void avcdecFilterIndexes(AvcdecStripe *stripe)
{
	const VideoHelperSlice *slice = stripe->slice;
	const AvcdecMbState *left = stripe->left;
	const AvcdecTop *top = stripe->top;
	AvcdecMbFilter *filter = stripe->filter;
	int32_t qp = (uint8_t)stripe->qp, index, value;
	uint8_t qpChroma, edges[6];
	uint32_t i;

	index = qp + slice->chromaQpIndexOffset;
	if (index > 51)
		index = 51;
	if (index < 0)
		index = 0;
	qpChroma = chromaQp[index];
	stripe->mb->qpChroma = qpChroma;
	/* qPav of each edge: the left's, the macroblock's own, the top's */
	edges[0] = (qp + (uint8_t)left->qp + 1) >> 1;
	edges[1] = qp;
	edges[2] = (qp + (uint8_t)top->qp + 1) >> 1;
	edges[3] = (qpChroma + left->qpChroma + 1) >> 1;
	edges[4] = qpChroma;
	edges[5] = (qpChroma + top->qpChroma + 1) >> 1;
	for (i = 0; i < 3; i++) {
		value = (int8_t)(edges[i] + slice->alphaOffset);
		filter->indexA[i] = value > 51 ? 51 : value < 0 ? 0 : value;
		value = (int8_t)(edges[i] + slice->betaOffset);
		filter->indexB[i] = value > 51 ? 51 : value < 0 ? 0 : value;
		value = (int8_t)(edges[3 + i] + slice->alphaOffset);
		filter->indexAChroma[i] = value > 51 ? 51 : value < 0 ? 0 : value;
		value = (int8_t)(edges[3 + i] + slice->betaOffset);
		filter->indexBChroma[i] = value > 51 ? 51 : value < 0 ? 0 : value;
	}
}
