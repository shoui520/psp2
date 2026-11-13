/*
 * The AVC decoder's motion, deblocking and prediction kernels (0x892a60-0x898f80): VLIW
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

/* the residual's dequantisation (8.5.12.1, flat): v by QP % 6 and position */
const int16_t avcdecLevelScale[6][16] = {			/* 0x91b1f8 */
	{ 10, 13, 10, 13, 13, 16, 13, 16, 10, 13, 10, 13, 13, 16, 13, 16 },
	{ 11, 14, 11, 14, 14, 18, 14, 18, 11, 14, 11, 14, 14, 18, 14, 18 },
	{ 13, 16, 13, 16, 16, 20, 16, 20, 13, 16, 13, 16, 16, 20, 16, 20 },
	{ 14, 18, 14, 18, 18, 23, 18, 23, 14, 18, 14, 18, 18, 23, 18, 23 },
	{ 16, 20, 16, 20, 20, 25, 20, 25, 16, 20, 16, 20, 20, 25, 20, 25 },
	{ 18, 23, 18, 23, 23, 29, 23, 29, 18, 23, 18, 23, 23, 29, 23, 29 },
};

/* |QP / 6 - 6| by QP + 36 (QP from -36): the shifts */
const uint8_t avcdecQpShift[88] = {				/* 0x91b2b8 */
	6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8,
};

/* QP % 6 */
const uint8_t avcdecQpRemainder[54] = {				/* 0x91b310 */
	0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
	0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
	0, 1, 2, 3, 4, 5,
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
 * The colocated 8x8 block's four vectors (8.4.1.2.1), by 4x4 block: under
 * direct_8x8_inference its corner's for all four
 */
static inline void colocatedVectors(const AvcdecColocated *colocated, uint32_t inferred,
	uint32_t block, AvcdecLanes col[2])
{
	if (inferred) {
		col[0].w[0] = colocated->corners.mv[block];
		col[0].w[1] = colocated->corners.mv[block];
		col[1] = col[0];
		return;
	}
	col[0].w[0] = colocated->blocks[block][0];
	col[0].w[1] = colocated->blocks[block][1];
	col[1].w[0] = (int32_t)colocated->blocks[block][2] >> COLOCATED_SHIFT;
	col[1].w[1] = (int32_t)colocated->blocks[block][3] >> COLOCATED_SHIFT;
}

/*
 * 0x892e10: an 8x8 block's temporal direct prediction (8.4.1.2.3): list 0's
 * reference the colocated block's picture, list 1's index 0, the vectors
 * the colocated ones scaled by the pictures' distances
 */
void avcdecDirectTemporal(AvcdecStripe *stripe, AvcdecMotion *motion, uint32_t inferred,
	uint32_t block)
{
	const VideoHelperSlice *slice = stripe->slice;
	const AvcdecColocated *colocated = stripe->colocated;
	AvcdecMbState *mb = stripe->mb;
	AvcdecLanes col[2], l0, l1;
	int32_t picture, refIdx, scale;
	uint32_t i, j;

	if (inferred)
		picture = colocated->corners.pictures[block];
	else
		picture = colocated->blocks[block][2] & 31;
	if (inferred ? picture < 0 : picture == 31) {
		/* intra: no vector, list 0's index 0 */
		refIdx = 0;
		col[0] = lanesZero();
		col[1] = lanesZero();
	} else {
		colocatedVectors(colocated, inferred, block, col);
		refIdx = slice->refIdxOfPicture[picture];
	}
	mb->refIdx.b[4 + block] = 0;
	mb->refIdx.b[block] = refIdx;
	motion->refIdx = mb->refIdx;
	scale = slice->distScaleFactor[refIdx];
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 4; j++) {
			l0.h[j] = (scale * col[i].h[j] + 128) >> 8;
			l1.h[j] = l0.h[j] - col[i].h[j];
		}
		motion->mv[block][2 * i].w[0] = l0.w[0];
		motion->mv[block][2 * i].w[1] = l1.w[0];
		motion->mv[block][2 * i + 1].w[0] = l0.w[1];
		motion->mv[block][2 * i + 1].w[1] = l1.w[1];
	}
}

/*
 * 0x892fb0: an 8x8 block's spatial direct prediction (8.4.1.2.2): the
 * neighbours' vector (the caller's predictor, for the references in current),
 * 0 where the reference is none, or index 0 and the colocated block's vector
 * is within one sample (colZeroFlag). Without direct_8x8_inference current
 * is left with 2 where the vector is 0.
 */
void avcdecDirectSpatial(AvcdecStripe *stripe, AvcdecMotion *motion, uint32_t inferred,
	uint32_t block)
{
	const AvcdecColocated *colocated = stripe->colocated;
	AvcdecLanes col[2], sub;
	uint32_t refIdxZero, i, j;
	uint8_t colZero[4] = { 0, 0, 0, 0 }, zero[8];
	int32_t x, y;

	if (inferred)
		refIdxZero = colocated->corners.refIdxZero;
	else
		refIdxZero = colocated->blocks[0][3];
	if ((refIdxZero & 31) >> block & 1 && stripe->slice->colocatedShortTerm) {
		colocatedVectors(colocated, inferred, block, col);
		for (j = 0; j < 4; j++) {
			x = col[j >> 1].h[(j & 1) * 2];
			y = col[j >> 1].h[(j & 1) * 2 + 1];
			colZero[j] = (x < 0 ? -x : x) < 2 && (y < 0 ? -y : y) < 2;
		}
	}
	for (i = 0; i < 8; i++)
		zero[i] = motion->current.b[i] < 0 ||
			(colZero[i & 3] && motion->current.b[i] == 0);
	if (inferred) {
		for (i = 0; i < 8; i++)
			sub.b[i] = zero[i] ? 0 : motion->predictor.b[i];
		for (j = 0; j < 4; j++)
			motion->mv[block][j] = sub;
		return;
	}
	for (i = 0; i < 8; i++)
		motion->current.b[i] = zero[i] ? 2 : 0;
	for (j = 0; j < 4; j++) {
		motion->mv[block][j].w[0] = zero[j] ? 0 : motion->predictor.w[0];
		motion->mv[block][j].w[1] = zero[4 + j] ? 0 : motion->predictor.w[1];
	}
}

/* the median of three vectors, by component */
static inline AvcdecLanes lanesMedian(AvcdecLanes a, AvcdecLanes b, AvcdecLanes c)
{
	AvcdecLanes low, high;
	uint32_t i;

	for (i = 0; i < 4; i++) {
		low.h[i] = a.h[i] < b.h[i] ? a.h[i] : b.h[i];
		high.h[i] = a.h[i] > b.h[i] ? a.h[i] : b.h[i];
		if (c.h[i] < high.h[i])
			high.h[i] = c.h[i];
		if (low.h[i] > high.h[i])
			high.h[i] = low.h[i];
	}
	return high;
}

/* a neighbour's vector where its reference is used, 0 where not */
static inline AvcdecLanes vectorUsed(AvcdecLanes refs, AvcdecLanes mv)
{
	return lanesSelect(lanesNegative(refs), lanesZero(), mv);
}

/*
 * The median of the neighbours' vectors (8.4.1.3.1), or the one whose
 * reference is the partition's when only one is; the neighbours without
 * the reference give no vector.
 */
static inline AvcdecLanes median(AvcdecMotion *motion)
{
	AvcdecLanes result, match;
	uint32_t i, one;
	int8_t count[8];

	motion->mvA = vectorUsed(motion->refA, motion->mvA);
	motion->mvB = vectorUsed(motion->refB, motion->mvB);
	motion->mvC = vectorUsed(motion->refC, motion->mvC);
	result = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
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

/*
 * Within an 8x8 block: the median, or the vector of the partition beside
 * it (whose reference is the block's) when neither of the others has the
 * reference. A, then B, the known one.
 */
static inline AvcdecLanes knownA(AvcdecMotion *motion)
{
	uint32_t neither;

	motion->mvB = vectorUsed(motion->refB, motion->mvB);
	motion->mvC = vectorUsed(motion->refC, motion->mvC);
	neither = lanesDiffer(motion->current, motion->refB) &
		lanesDiffer(motion->current, motion->refC);
	return lanesSelect(neither, motion->mvA,
		lanesMedian(motion->mvA, motion->mvB, motion->mvC));
}

static inline AvcdecLanes knownB(AvcdecMotion *motion)
{
	uint32_t neither;

	motion->mvA = vectorUsed(motion->refA, motion->mvA);
	motion->mvC = vectorUsed(motion->refC, motion->mvC);
	neither = lanesDiffer(motion->current, motion->refA) &
		lanesDiffer(motion->current, motion->refC);
	return lanesSelect(neither, motion->mvB,
		lanesMedian(motion->mvA, motion->mvB, motion->mvC));
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
		motion->predictor = vectorUsed(motion->refA, motion->mvA);
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
	if (onlyA) {
		motion->predictor = motion->mvA;
		return;
	}
	motion->predictor = knownA(motion);
}

/* 0x8932c0: the median */
void avcdecMvMedian(AvcdecMotion *motion)
{
	motion->predictor = median(motion);
}

static inline AvcdecLanes vectorOf(const uint32_t mv[2])
{
	AvcdecLanes lanes;

	lanes.w[0] = mv[0];
	lanes.w[1] = mv[1];
	return lanes;
}

/*
 * 0x893320: 8x8 block 0's sub-macroblock partitions (8.4.1.3): the left's
 * and the above's neighbours outside the macroblock
 */
void avcdecMvPredict8x8Block0(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	const AvcdecTop *top = stripe->top;
	uint32_t sub = (uint8_t)stripe->subKinds[0], onlyA;
	AvcdecLanes (*mv)[4] = motion->mv;

	motion->mvA = vectorOf(leftVectors->mv[5]);
	motion->mvB = vectorOf(top->mv[0]);
	onlyA = stripe->leftAvailable & !stripe->topAvailable;
	motion->current = lanesOfBlock(motion->refIdx, 0);
	motion->refA = lanesOfBlock(stripe->left->refIdx, 1);
	motion->refB = lanesOfBlock(top->refIdx, 2);
	if (!(sub & (3 << AVC_MB_PARTS_SHIFT))) {		/* 8x8 */
		motion->mvC = vectorOf(top->mv[2]);
		motion->refC = lanesOfBlock(top->refIdx, 3);
		avcdecMvPredict(motion, onlyA);
		mv[0][0] = lanesAdd(mv[0][0], motion->predictor);
		mv[0][3] = mv[0][2] = mv[0][1] = mv[0][0];
	} else if (sub & 2) {					/* 8x4 */
		motion->mvC = vectorOf(top->mv[2]);
		motion->refC = lanesOfBlock(top->refIdx, 3);
		avcdecMvPredict(motion, onlyA);
		mv[0][0] = lanesAdd(mv[0][0], motion->predictor);
		motion->mvA = vectorOf(leftVectors->mv[7]);
		motion->mvC = vectorOf(leftVectors->mv[5]);
		motion->refC = motion->refA;
		motion->mvB = mv[0][0];
		motion->predictor = knownB(motion);
		mv[0][2] = lanesAdd(mv[0][2], motion->predictor);
		mv[0][3] = mv[0][2];
		mv[0][1] = mv[0][0];
	} else {
		motion->mvC = vectorOf(top->mv[1]);
		motion->refC = motion->refB;
		avcdecMvPredict(motion, onlyA);
		mv[0][0] = lanesAdd(mv[0][0], motion->predictor);
		motion->mvB = vectorOf(top->mv[1]);
		motion->mvC = vectorOf(top->mv[2]);
		motion->refC = lanesOfBlock(top->refIdx, 3);
		if (sub & 1) {					/* 4x8 */
			mv[0][2] = mv[0][0];
			motion->mvA = mv[0][0];
			avcdecMvPredictUnlessA(motion, !stripe->topAvailable);
			mv[0][1] = lanesAdd(mv[0][1], motion->predictor);
			mv[0][3] = mv[0][1];
		} else {					/* 4x4 */
			motion->mvA = mv[0][0];
			avcdecMvPredictUnlessA(motion, !stripe->topAvailable);
			mv[0][1] = lanesAdd(mv[0][1], motion->predictor);
			motion->mvA = vectorUsed(motion->refA, vectorOf(leftVectors->mv[7]));
			motion->mvB = mv[0][0];
			motion->mvC = mv[0][1];
			motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
			mv[0][2] = lanesAdd(mv[0][2], motion->predictor);
			motion->mvA = mv[0][2];
			motion->mvB = mv[0][1];
			motion->mvC = mv[0][0];
			motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
			mv[0][3] = lanesAdd(mv[0][3], motion->predictor);
		}
	}
}

/* 0x893648: 8x8 block 1's: the above's and the above right's */
void avcdecMvPredict8x8Block1(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const AvcdecTop *top = stripe->top;
	uint32_t sub = (uint8_t)stripe->subKinds[1], onlyA;
	AvcdecLanes (*mv)[4] = motion->mv;

	motion->refA = lanesOfBlock(motion->refIdx, 0);
	motion->current = lanesOfBlock(motion->refIdx, 1);
	motion->refB = lanesOfBlock(top->refIdx, 3);
	motion->mvB = vectorOf(top->mv[2]);
	motion->mvA = mv[0][1];
	onlyA = !stripe->topAvailable;
	if (!(sub & (3 << AVC_MB_PARTS_SHIFT)) || (sub & 2)) {
		/* 8x8, 8x4: C above right, else above left */
		if (stripe->topRightAvailable) {
			motion->refC = lanesOfBlock(stripe->topRight->refIdx, 2);
			motion->mvC = vectorOf(stripe->topRight->mv[0]);
			onlyA &= !stripe->topRightAvailable;
		} else {
			motion->refC = lanesOfBlock(top->refIdx, 2);
			motion->mvC = vectorOf(top->mv[1]);
		}
		avcdecMvPredict(motion, onlyA);
		mv[1][0] = lanesAdd(mv[1][0], motion->predictor);
		if (!(sub & (3 << AVC_MB_PARTS_SHIFT))) {
			mv[1][3] = mv[1][2] = mv[1][1] = mv[1][0];
			return;
		}
		motion->mvC = mv[0][1];
		motion->refC = motion->refA;
		motion->mvA = mv[0][3];
		motion->mvB = mv[1][0];
		motion->predictor = knownB(motion);
		mv[1][2] = lanesAdd(mv[1][2], motion->predictor);
		mv[1][3] = mv[1][2];
		mv[1][1] = mv[1][0];
		return;
	}
	motion->mvC = vectorOf(top->mv[3]);
	motion->refC = motion->refB;
	avcdecMvPredict(motion, onlyA);
	mv[1][0] = lanesAdd(mv[1][0], motion->predictor);
	if (sub & 1)						/* 4x8 */
		mv[1][2] = mv[1][0];
	if (stripe->topRightAvailable) {
		motion->refC = lanesOfBlock(stripe->topRight->refIdx, 2);
		motion->mvC = vectorOf(stripe->topRight->mv[0]);
		onlyA &= !stripe->topRightAvailable;
	} else {
		motion->mvC = vectorOf(top->mv[2]);
		motion->refC = motion->refB;
	}
	motion->mvB = vectorOf(top->mv[3]);
	motion->mvA = mv[1][0];
	avcdecMvPredictUnlessA(motion, onlyA);
	mv[1][1] = lanesAdd(mv[1][1], motion->predictor);
	if (sub & 1) {
		mv[1][3] = mv[1][1];
		return;
	}
	/* 4x4: the lower two from inside the macroblock */
	motion->mvA = vectorUsed(motion->refA, mv[0][3]);
	motion->mvB = mv[1][0];
	motion->mvC = mv[1][1];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[1][2] = lanesAdd(mv[1][2], motion->predictor);
	motion->mvA = mv[1][2];
	motion->mvB = mv[1][1];
	motion->mvC = mv[1][0];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[1][3] = lanesAdd(mv[1][3], motion->predictor);
}

/* 0x893a70: 8x8 block 2's: the left's */
void avcdecMvPredict8x8Block2(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	uint32_t sub = (uint8_t)stripe->subKinds[2];
	AvcdecLanes (*mv)[4] = motion->mv;

	motion->mvA = vectorOf(leftVectors->mv[13]);
	motion->refB = lanesOfBlock(motion->refIdx, 0);
	motion->refA = lanesOfBlock(stripe->left->refIdx, 3);
	motion->current = lanesOfBlock(motion->refIdx, 2);
	motion->mvB = mv[0][2];
	if (!(sub & (3 << AVC_MB_PARTS_SHIFT)) || (sub & 2)) {	/* 8x8, 8x4 */
		motion->mvC = mv[1][2];
		motion->refC = lanesOfBlock(motion->refIdx, 1);
		avcdecMvMedian(motion);
		mv[2][0] = lanesAdd(mv[2][0], motion->predictor);
		if (!(sub & (3 << AVC_MB_PARTS_SHIFT))) {
			mv[2][3] = mv[2][2] = mv[2][1] = mv[2][0];
			return;
		}
		motion->mvA = vectorOf(leftVectors->mv[15]);
		motion->mvC = vectorOf(leftVectors->mv[13]);
		motion->refC = motion->refA;
		motion->mvB = mv[2][0];
		motion->predictor = knownB(motion);
		mv[2][2] = lanesAdd(mv[2][2], motion->predictor);
		mv[2][3] = mv[2][2];
		mv[2][1] = mv[2][0];
		return;
	}
	motion->refC = motion->refB;
	motion->mvC = mv[0][3];
	avcdecMvMedian(motion);
	mv[2][0] = lanesAdd(mv[2][0], motion->predictor);
	motion->mvC = mv[1][2];
	motion->mvA = mv[2][0];
	motion->mvB = mv[0][3];
	motion->refC = lanesOfBlock(motion->refIdx, 1);
	if (sub & 1) {						/* 4x8 */
		motion->predictor = knownA(motion);
		mv[2][1] = lanesAdd(mv[2][1], motion->predictor);
		mv[2][3] = mv[2][1];
		mv[2][2] = mv[2][0];
		return;
	}
	/* 4x4 */
	motion->predictor = knownA(motion);
	mv[2][1] = lanesAdd(mv[2][1], motion->predictor);
	motion->mvA = vectorUsed(motion->refA, vectorOf(leftVectors->mv[15]));
	motion->mvB = mv[2][0];
	motion->mvC = mv[2][1];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[2][2] = lanesAdd(mv[2][2], motion->predictor);
	motion->mvA = mv[2][2];
	motion->mvB = mv[2][1];
	motion->mvC = mv[2][0];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[2][3] = lanesAdd(mv[2][3], motion->predictor);
}

/* 0x893d18: 8x8 block 3's: all inside the macroblock */
void avcdecMvPredict8x8Block3(AvcdecStripe *stripe, AvcdecMotion *motion)
{
	uint32_t sub = (uint8_t)stripe->subKinds[3];
	AvcdecLanes (*mv)[4] = motion->mv;

	motion->mvA = mv[2][1];
	motion->refA = lanesOfBlock(motion->refIdx, 2);
	motion->refB = lanesOfBlock(motion->refIdx, 1);
	motion->current = lanesOfBlock(motion->refIdx, 3);
	motion->mvB = mv[1][2];
	if (!(sub & (3 << AVC_MB_PARTS_SHIFT)) || (sub & 2)) {	/* 8x8, 8x4 */
		motion->mvC = mv[0][3];
		motion->refC = lanesOfBlock(motion->refIdx, 0);
		avcdecMvMedian(motion);
		mv[3][0] = lanesAdd(mv[3][0], motion->predictor);
		if (!(sub & (3 << AVC_MB_PARTS_SHIFT))) {
			mv[3][3] = mv[3][2] = mv[3][1] = mv[3][0];
			return;
		}
		motion->mvC = mv[2][1];
		motion->refC = motion->refA;
		motion->mvA = mv[2][3];
		motion->mvB = mv[3][0];
		motion->predictor = knownB(motion);
		mv[3][2] = lanesAdd(mv[3][2], motion->predictor);
		mv[3][3] = mv[3][2];
		mv[3][1] = mv[3][0];
		return;
	}
	motion->refC = motion->refB;
	motion->mvC = mv[1][3];
	avcdecMvMedian(motion);
	mv[3][0] = lanesAdd(mv[3][0], motion->predictor);
	motion->mvC = mv[1][2];
	motion->refC = motion->refB;
	motion->mvB = mv[1][3];
	motion->mvA = mv[3][0];
	motion->predictor = knownA(motion);
	mv[3][1] = lanesAdd(mv[3][1], motion->predictor);
	if (sub & 1) {						/* 4x8 */
		mv[3][3] = mv[3][1];
		mv[3][2] = mv[3][0];
		return;
	}
	/* 4x4 */
	motion->mvB = mv[3][0];
	motion->mvA = vectorUsed(motion->refA, mv[2][3]);
	motion->mvC = mv[3][1];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[3][2] = lanesAdd(mv[3][2], motion->predictor);
	motion->mvA = mv[3][2];
	motion->mvB = mv[3][1];
	motion->mvC = mv[3][0];
	motion->predictor = lanesMedian(motion->mvA, motion->mvB, motion->mvC);
	mv[3][3] = lanesAdd(mv[3][3], motion->predictor);
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

/* a 4x4 block's list 0 vector, by row and column */
static inline uint32_t vectorAt(const AvcdecLanes (*mv)[4], uint32_t row, uint32_t column)
{
	return mv[(row >> 1) * 2 + (column >> 1)][(row & 1) * 2 + (column & 1)].w[0];
}

/* a component four quarter samples apart or more, the difference in 16 bits */
static inline uint32_t componentFar(int16_t a, int16_t b)
{
	int32_t difference = a - b;

	return (int16_t)(difference < 0 ? -difference : difference) >= 4;
}

/* 0x894450, by edge: two vectors four quarter samples apart or more (8.7.2.1) */
static inline uint32_t vectorsFar(uint32_t a, uint32_t b)
{
	return componentFar((int16_t)a, (int16_t)b) ||
		componentFar((int16_t)(a >> 16), (int16_t)(b >> 16));
}

/*
 * 0x8943b0 and 0x894ca0, by edge: how two 8x8 blocks' pictures pair, list
 * by list (bit 0) and crossed (bit 1); none, they differ
 */
#define PICTURES_SAME		1
#define PICTURES_CROSSED	2

static inline uint32_t picturePairing(AvcdecLanes pictures, uint32_t block,
	AvcdecLanes others, uint32_t other)
{
	uint32_t pairing = 0;

	if (pictures.b[block] == others.b[other] &&
	    pictures.b[4 + block] == others.b[4 + other])
		pairing |= PICTURES_SAME;
	if (pictures.b[block] == others.b[4 + other] &&
	    pictures.b[4 + block] == others.b[other])
		pairing |= PICTURES_CROSSED;
	return pairing;
}

/*
 * 0x894010: a P macroblock's bS 1 (8.7.2.1) where it is 0: the edges between
 * blocks of different pictures or with vectors apart, on the left and top
 * against the neighbour's vector of the same picture. The edges of the
 * picture (and of the slice, under idc 2) none.
 */
void avcdecStrengthsP(AvcdecStripe *stripe, const AvcdecMotion *motion, uint32_t left,
	uint32_t top)
{
	static const uint8_t leftBlocks[4] = { 5, 7, 13, 15 };
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	const AvcdecTop *above = stripe->top;
	AvcdecLanes pictures = stripe->mb->refPicture;
	uint8_t *vertical = stripe->filter->strength.edges[1];
	uint8_t *horizontal = stripe->filter->strength.edges[0];
	uint32_t i, half, pairing, differ[2], inner[2], neighbour;

	/* the vertical edges, by row */
	for (half = 0; half < 2; half++) {
		pairing = picturePairing(pictures, 2 * half, stripe->left->refPicture,
			2 * half + 1);
		differ[half] = !pairing;
		inner[half] = pictures.b[2 * half] != pictures.b[2 * half + 1];
		for (i = 2 * half; i < 2 * half + 2; i++) {
			neighbour = leftVectors->mv[leftBlocks[i]][pairing & PICTURES_SAME ? 0 : 1];
			if (!vertical[i])
				vertical[i] = differ[half] ||
					vectorsFar(neighbour, vectorAt(motion->mv, i, 0));
			if (!vertical[4 + i])
				vertical[4 + i] = vectorsFar(vectorAt(motion->mv, i, 0),
					vectorAt(motion->mv, i, 1));
			if (!vertical[8 + i])
				vertical[8 + i] = inner[half] ||
					vectorsFar(vectorAt(motion->mv, i, 1),
						vectorAt(motion->mv, i, 2));
			if (!vertical[12 + i])
				vertical[12 + i] = vectorsFar(vectorAt(motion->mv, i, 2),
					vectorAt(motion->mv, i, 3));
		}
	}
	if (!left)
		for (i = 0; i < 4; i++)
			vertical[i] = 0;
	/* the horizontal ones, by column */
	for (half = 0; half < 2; half++) {
		pairing = picturePairing(pictures, half, above->refPicture, 2 + half);
		differ[half] = !pairing;
		inner[half] = pictures.b[half] != pictures.b[2 + half];
		for (i = 2 * half; i < 2 * half + 2; i++) {
			neighbour = above->mv[i][pairing & PICTURES_SAME ? 0 : 1];
			if (!horizontal[i])
				horizontal[i] = differ[half] ||
					vectorsFar(neighbour, vectorAt(motion->mv, 0, i));
			if (!horizontal[4 + i])
				horizontal[4 + i] = vectorsFar(vectorAt(motion->mv, 0, i),
					vectorAt(motion->mv, 1, i));
			if (!horizontal[8 + i])
				horizontal[8 + i] = inner[half] ||
					vectorsFar(vectorAt(motion->mv, 1, i),
						vectorAt(motion->mv, 2, i));
			if (!horizontal[12 + i])
				horizontal[12 + i] = vectorsFar(vectorAt(motion->mv, 2, i),
					vectorAt(motion->mv, 3, i));
		}
	}
	if (!top)
		for (i = 0; i < 4; i++)
			horizontal[i] = 0;
}

/* any component of either list four quarter samples apart or more */
static inline uint32_t lanesFar(AvcdecLanes a, AvcdecLanes b)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		if (componentFar(a.h[i], b.h[i]))
			return 1;
	return 0;
}

static inline AvcdecLanes lanesCrossed(AvcdecLanes lanes)
{
	AvcdecLanes crossed;

	crossed.w[0] = lanes.w[1];
	crossed.w[1] = lanes.w[0];
	return crossed;
}

/*
 * A B macroblock's 8x8 block's vectors on its edges in one direction, two
 * 4x4 blocks each: its outer edge against the neighbour's (near, the left's,
 * the above's or the 8x8 block's beside it) paired as the pictures pair, its
 * inner edge between its first and second lines. Bits 0 and 1 the outer
 * edge's, 4 and 5 the inner's.
 */
static uint32_t edgesFarB(AvcdecLanes near[2], const AvcdecLanes first[2],
	const AvcdecLanes second[2], uint32_t unused, uint32_t block, uint32_t pairing,
	uint32_t samePictures)
{
	AvcdecLanes crossed, crossedSecond;
	uint32_t far = 0, other = 0, i, neighbour;

	/* 0x894d78: the lists the block uses against the neighbour's */
	for (i = 0; i < 2; i++) {
		if (unused & (block << 4)) {			/* list 0 only */
			neighbour = pairing == PICTURES_SAME ? near[i].w[0] : near[i].w[1];
			near[i].w[0] = neighbour;
			near[i].w[1] = first[i].w[1];
		} else if (unused & block) {			/* list 1 only */
			neighbour = pairing == PICTURES_SAME ? near[i].w[1] : near[i].w[0];
			near[i].w[0] = first[i].w[0];
			near[i].w[1] = neighbour;
		} else if (pairing == PICTURES_CROSSED) {
			near[i] = lanesCrossed(near[i]);
		}
		far |= lanesFar(near[i], first[i]) << i | lanesFar(first[i], second[i]) << (4 + i);
	}
	if (pairing == (PICTURES_SAME | PICTURES_CROSSED)) {
		/* 0x894ea0: either way: 1 only where both are apart */
		for (i = 0; i < 2; i++) {
			crossed = lanesCrossed(first[i]);
			crossedSecond = samePictures ? second[i] : crossed;
			other |= lanesFar(near[i], crossed) << i |
				lanesFar(crossed, crossedSecond) << (4 + i);
		}
		far &= other;
	} else if (samePictures) {
		/* 0x894f40: both lists one picture: the inner edge either way */
		for (i = 0; i < 2; i++)
			other |= lanesFar(lanesCrossed(first[i]), second[i]) << (4 + i);
		far &= other;
	}
	return far;
}

/* the edges' bS 1 from edgesFarB, where some is 0 */
static inline void strengthsFarB(uint8_t *strengths, uint32_t line, AvcdecLanes near[2],
	const AvcdecLanes first[2], const AvcdecLanes second[2], uint32_t unused,
	uint32_t block, uint32_t pairing, uint32_t samePictures)
{
	uint32_t far, i;

	if (strengths[line] && strengths[line + 1] && strengths[4 + line] &&
	    strengths[5 + line])
		return;
	far = edgesFarB(near, first, second, unused, block, pairing, samePictures);
	for (i = 0; i < 2; i++) {
		if (far & (1 << i) && !strengths[line + i])
			strengths[line + i] = 1;
		if (far & (16 << i) && !strengths[4 + line + i])
			strengths[4 + line + i] = 1;
	}
}

/*
 * 0x8944a0: a B macroblock's bS 1 where it is 0 (8.7.2.1): the edges between
 * blocks of different pictures, or of the same ones with vectors apart
 */
void avcdecStrengthsB(AvcdecStripe *stripe, const AvcdecMotion *motion, uint32_t left,
	uint32_t top)
{
	const AvcdecMbVectors *leftVectors = stripe->leftVectors;
	const AvcdecTop *above = stripe->top;
	const AvcdecLanes (*mv)[4] = motion->mv;
	AvcdecLanes pictures = stripe->mb->refPicture, near[2], first[2], second[2];
	uint8_t *vertical = stripe->filter->strength.edges[1];
	uint8_t *horizontal = stripe->filter->strength.edges[0];
	uint32_t unused = 0, outer[2], inner[2], half, block, i;

	for (i = 0; i < 8; i++)
		if (pictures.b[i] < 0)
			unused |= 1 << i;

	/* the vertical edges: by half, the left's and between the 8x8 blocks */
	for (half = 0; half < 2; half++) {
		outer[half] = picturePairing(pictures, 2 * half, stripe->left->refPicture,
			2 * half + 1);
		inner[half] = picturePairing(pictures, 2 * half + 1, pictures, 2 * half);
		for (i = 2 * half; i < 2 * half + 2; i++) {
			if (!outer[half] && !vertical[i])
				vertical[i] = 1;
			if (!inner[half] && !vertical[8 + i])
				vertical[8 + i] = 1;
		}
	}
	for (half = 0; half < 2; half++) {
		block = 2 * half;
		near[0] = vectorOf(leftVectors->mv[half ? 13 : 5]);
		near[1] = vectorOf(leftVectors->mv[half ? 15 : 7]);
		first[0] = mv[block][0];
		first[1] = mv[block][2];
		second[0] = mv[block][1];
		second[1] = mv[block][3];
		strengthsFarB(vertical, 2 * half, near, first, second, unused, 1 << block,
			outer[half], pictures.b[block] == pictures.b[4 + block]);
	}
	for (half = 0; half < 2; half++) {
		block = 2 * half + 1;
		near[0] = mv[block - 1][1];
		near[1] = mv[block - 1][3];
		first[0] = mv[block][0];
		first[1] = mv[block][2];
		second[0] = mv[block][1];
		second[1] = mv[block][3];
		strengthsFarB(vertical + 8, 2 * half, near, first, second, unused, 1 << block,
			inner[half], pictures.b[block] == pictures.b[4 + block]);
	}
	if (!left)
		for (i = 0; i < 4; i++)
			vertical[i] = 0;

	/* the horizontal ones: by half, the above's and between the 8x8 blocks */
	for (half = 0; half < 2; half++) {
		outer[half] = picturePairing(pictures, half, above->refPicture, 2 + half);
		inner[half] = picturePairing(pictures, 2 + half, pictures, half);
		for (i = 2 * half; i < 2 * half + 2; i++) {
			if (!outer[half] && !horizontal[i])
				horizontal[i] = 1;
			if (!inner[half] && !horizontal[8 + i])
				horizontal[8 + i] = 1;
		}
	}
	for (half = 0; half < 2; half++) {
		block = half;
		near[0] = vectorOf(above->mv[2 * half]);
		near[1] = vectorOf(above->mv[2 * half + 1]);
		first[0] = mv[block][0];
		first[1] = mv[block][1];
		second[0] = mv[block][2];
		second[1] = mv[block][3];
		strengthsFarB(horizontal, 2 * half, near, first, second, unused, 1 << block,
			outer[half], pictures.b[block] == pictures.b[4 + block]);
	}
	for (half = 0; half < 2; half++) {
		block = 2 + half;
		near[0] = mv[half][2];
		near[1] = mv[half][3];
		first[0] = mv[block][0];
		first[1] = mv[block][1];
		second[0] = mv[block][2];
		second[1] = mv[block][3];
		strengthsFarB(horizontal + 8, 2 * half, near, first, second, unused, 1 << block,
			inner[half], pictures.b[block] == pictures.b[4 + block]);
	}
	if (!top)
		for (i = 0; i < 4; i++)
			horizontal[i] = 0;
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

/*
 * The luma prediction's kernels (0x895140-0x897dd8). The reference frame
 * is in macroblock tiles: a 16x16 tile's rows 16 bytes apart, the tiles of
 * a row 256 bytes apart, the tile rows pitch * 16 apart. A fetch reads the
 * samples a block's filter needs from the picture clamped to its edges;
 * its loads take the samples 8 bytes at a time and shift them in by the
 * column (CSAR0), its edge kernels then repeat the picture's edge samples
 * over what lies outside it.
 */

/* cpfsftbs0 q, p by CSAR0 = k: the eight bytes from 8 - k on of p then q */
static inline void funnel(uint8_t *out, const uint8_t *p, const uint8_t *q, uint32_t k)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		out[i] = i < k ? p[8 - k + i] : q[i - k];
}

/* a load's next row (y the window's top): into the tile below past a tile's last */
static inline uint32_t rowStep(uint32_t y, uint32_t row, uint32_t step)
{
	return ((y + row + 1) & 15) ? 16 : 16 + step;
}

/* rows of eight samples from two streams */
static void loadRows(AvcdecInter *inter, uint32_t first, uint32_t count, const uint8_t *address,
	const uint8_t *next, uint32_t y, uint32_t step, uint32_t shift)
{
	uint32_t row, advance;

	for (row = first; row < first + count; row++) {
		funnel(inter->window[row], address, next, shift);
		advance = rowStep(y, row, step);
		address += advance;
		next += advance;
	}
}

/* rows of sixteen from two streams: the second half the second's, shifted round */
static void loadRowsPair(AvcdecInter *inter, uint32_t first, uint32_t count,
	const uint8_t *address, const uint8_t *next, uint32_t y, uint32_t step, uint32_t shift)
{
	uint32_t row, advance, i;

	for (row = first; row < first + count; row++) {
		if (shift) {
			funnel(inter->window[row], address, next, shift);
			funnel(inter->window[row] + 8, next, next, shift);
		} else {
			for (i = 0; i < 8; i++) {
				inter->window[row][i] = address[i];
				inter->window[row][8 + i] = next[i];
			}
		}
		advance = rowStep(y, row, step);
		address += advance;
		next += advance;
	}
}

/* rows of sixteen from three streams */
static void loadRowsWide(AvcdecInter *inter, uint32_t first, uint32_t count,
	const uint8_t *address, const uint8_t *next, const uint8_t *third, uint32_t y,
	uint32_t step, uint32_t shift)
{
	uint32_t row, advance;

	for (row = first; row < first + count; row++) {
		funnel(inter->window[row], address, next, shift);
		funnel(inter->window[row] + 8, next, third, shift);
		advance = rowStep(y, row, step);
		address += advance;
		next += advance;
		third += advance;
	}
}

/* 0x895140: a 4x4 block's four rows */
void avcdecLoad4(AvcdecInter *inter, const uint8_t *address, const uint8_t *next, uint32_t y,
	uint32_t step, uint32_t shift)
{
	loadRows(inter, 0, 4, address, next, y, step, shift);
}

/* 0x895218: the five rows below them a vertical filter reads */
void avcdecLoad4Tall(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift)
{
	loadRows(inter, 4, 5, address, next, y, step, shift);
}

/* 0x895350: four rows of nine samples and more, for a horizontal filter */
void avcdecLoad4Wide(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift)
{
	loadRowsPair(inter, 0, 4, address, next, y, step, shift);
}

/* 0x895448 */
void avcdecLoad4WideTall(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift)
{
	loadRowsPair(inter, 4, 5, address, next, y, step, shift);
}

/* 0x8955a8: an 8x8 block's eight rows */
void avcdecLoad8(AvcdecInter *inter, const uint8_t *address, const uint8_t *next, uint32_t y,
	uint32_t step, uint32_t shift)
{
	loadRows(inter, 0, 8, address, next, y, step, shift);
}

/* 0x895860 */
void avcdecLoad8Tall(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift)
{
	loadRows(inter, 8, 5, address, next, y, step, shift);
}

/* 0x895998 */
void avcdecLoad8Wide(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift)
{
	loadRowsWide(inter, 0, 8, address, next, third, y, step, shift);
}

/* 0x895d78 */
void avcdecLoad8WideTall(AvcdecInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift)
{
	loadRowsWide(inter, 8, 5, address, next, third, y, step, shift);
}

/* the picture's left edge `shift` columns in: its first column repeated before it */
static void edgeLeft(AvcdecInter *inter, uint32_t first, uint32_t count, uint32_t width,
	int32_t shift)
{
	uint32_t row;
	int32_t column;

	for (row = first; row < first + count; row++)
		for (column = width - 1; column >= 0; column--)
			inter->window[row][column] =
				inter->window[row][column >= shift ? column - shift : 0];
}

/* the right edge: column `last`'s sample repeated past it */
static void edgeRight(AvcdecInter *inter, uint32_t first, uint32_t count, int32_t last,
	int32_t shift)
{
	uint32_t row;
	int32_t column;

	for (row = first; row < first + count; row++)
		for (column = 0; column <= last; column++)
			inter->window[row][column] =
				inter->window[row][column + shift < last ? column + shift : last];
}

/* the top edge: the first row repeated above it */
static void edgeTop(AvcdecInter *inter, int32_t count, int32_t shift)
{
	int32_t row;
	uint32_t column;

	for (row = count - 1; row >= 0; row--)
		for (column = 0; column < 16; column++)
			inter->window[row][column] =
				inter->window[row >= shift ? row - shift : 0][column];
}

/* the bottom edge: the last row repeated below it */
static void edgeBottom(AvcdecInter *inter, int32_t count, int32_t shift)
{
	int32_t row, last = count - 1;
	uint32_t column;

	for (row = 0; row < count; row++)
		for (column = 0; column < 16; column++)
			inter->window[row][column] =
				inter->window[row + shift < last ? row + shift : last][column];
}

/* 0x895f30: a 4x4 block's rows, `shift` columns left of the picture */
void avcdecEdgeLeft4(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 0, 4, 8, shift);
}

/* 0x895f50 */
void avcdecEdgeLeft4Tall(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 4, 5, 8, shift);
}

/* 0x895f78: the rows of a horizontal filter */
void avcdecEdgeLeft4Wide(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 0, 4, 16, shift);
}

/* 0x895fa8: all of them left of it */
void avcdecEdgeLeft4WideFar(AvcdecInter *inter)
{
	edgeLeft(inter, 0, 4, 16, 16);
}

/* 0x895fc8 */
void avcdecEdgeLeft4WideTall(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 4, 5, 16, shift);
}

/* 0x896008 */
void avcdecEdgeLeft4WideFarTall(AvcdecInter *inter)
{
	edgeLeft(inter, 4, 5, 16, 16);
}

/* 0x896030 */
void avcdecEdgeLeft8(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 0, 8, 8, shift);
}

/* 0x896070 */
void avcdecEdgeLeft8Tall(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 8, 5, 8, shift);
}

/* 0x896098: under eight columns out */
void avcdecEdgeLeft8Wide(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 0, 8, 16, shift);
}

/* 0x8960f8: eight or more: the first half all the edge's */
void avcdecEdgeLeft8WideFar(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 0, 8, 16, shift);
}

/* 0x896158 */
void avcdecEdgeLeft8WideTall(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 8, 5, 16, shift);
}

/* 0x896198 */
void avcdecEdgeLeft8WideFarTall(AvcdecInter *inter, int32_t shift)
{
	edgeLeft(inter, 8, 5, 16, shift);
}

/* 0x8961d8: the picture's last column `shift` in from the window's */
void avcdecEdgeRight4(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 0, 4, 7, shift);
}

/* 0x8961f8 */
void avcdecEdgeRight4Tall(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 4, 5, 7, shift);
}

/* 0x896220: nine columns, the last ones past the picture */
void avcdecEdgeRight4Wide(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 0, 4, 8, shift);
}

/* 0x896240: all of them past it: the ninth column's sample */
void avcdecEdgeRight4WideFar(AvcdecInter *inter)
{
	edgeRight(inter, 0, 4, 8, 8);
}

/* 0x896250 */
void avcdecEdgeRight4WideTall(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 4, 5, 8, shift);
}

/* 0x896278 */
void avcdecEdgeRight4WideFarTall(AvcdecInter *inter)
{
	edgeRight(inter, 4, 5, 8, 8);
}

/* 0x896290 */
void avcdecEdgeRight8(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 0, 8, 7, shift);
}

/* 0x8962d0 */
void avcdecEdgeRight8Tall(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 8, 5, 7, shift);
}

/* 0x8962f8 */
void avcdecEdgeRight8Wide(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 0, 8, 15, shift);
}

/* 0x896358: eight or more columns past it */
void avcdecEdgeRight8WideFar(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 0, 8, 15, shift);
}

/* 0x8963b8 */
void avcdecEdgeRight8WideTall(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 8, 5, 15, shift);
}

/* 0x8963f8 */
void avcdecEdgeRight8WideFarTall(AvcdecInter *inter, int32_t shift)
{
	edgeRight(inter, 8, 5, 15, shift);
}

/* 0x896438: the block's first row `-y` rows below the picture's top */
void avcdecEdgeTop4(AvcdecInter *inter, int32_t y)
{
	edgeTop(inter, 4, -y);
}

/* 0x896468 */
void avcdecEdgeTop4Tall(AvcdecInter *inter, int32_t y)
{
	edgeTop(inter, 9, -y);
}

/* 0x8964c0 */
void avcdecEdgeTop8(AvcdecInter *inter, int32_t y)
{
	edgeTop(inter, 8, -y);
}

/* 0x896510 */
void avcdecEdgeTop8Tall(AvcdecInter *inter, int32_t y)
{
	edgeTop(inter, 13, -y);
}

/* 0x896588: the window's last row `below` rows past the picture's */
void avcdecEdgeBottom4(AvcdecInter *inter, int32_t below)
{
	edgeBottom(inter, 4, below);
}

/* 0x8965b8 */
void avcdecEdgeBottom4Tall(AvcdecInter *inter, int32_t below)
{
	edgeBottom(inter, 9, below);
}

/* 0x896610 */
void avcdecEdgeBottom8(AvcdecInter *inter, int32_t below)
{
	edgeBottom(inter, 8, below);
}

/* 0x896660 */
void avcdecEdgeBottom8Tall(AvcdecInter *inter, int32_t below)
{
	edgeBottom(inter, 13, below);
}

/* 0x8966d8: a whole-sample 4x4 block, the window's corner */
void avcdecFull4(AvcdecInter *inter)
{
	uint32_t row, column;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++)
			inter->small[row][column] = inter->window[row][column];
}

/* 0x8966e0: a vertical filter's taps, the window's nine rows */
void avcdecTapsVertical4(AvcdecInter *inter)
{
	uint32_t row, column;

	for (row = 0; row < 9; row++)
		for (column = 0; column < 4; column++)
			inter->taps[row][column] = inter->window[row][column];
}

/* 0x896700: a horizontal filter's, the window's nine columns turned into rows */
void avcdecTapsHorizontal4(AvcdecInter *inter)
{
	uint32_t row, column;

	for (row = 0; row < 9; row++)
		for (column = 0; column < 4; column++)
			inter->taps[row][column] = inter->window[column][row];
}

static inline uint8_t clipSample(int32_t value)
{
	return value < 0 ? 0 : value > 255 ? 255 : value;
}

/* the 6-tap filter (1, -5, 20, 20, -5, 1) over `step` apart */
static inline int32_t sixTap(const uint8_t *sample, uint32_t step)
{
	return sample[0] - 5 * sample[step] + 20 * sample[2 * step] + 20 * sample[3 * step] -
		5 * sample[4 * step] + sample[5 * step];
}

/* cpaveu3.b */
static inline uint8_t averageSamples(uint32_t a, uint32_t b)
{
	return (a + b + 1) >> 1;
}

/* a half sample down a column of the window */
static inline uint8_t halfVertical(const AvcdecInter *inter, uint32_t row, uint32_t column)
{
	return clipSample((sixTap(&inter->window[row][column], 16) + 16) >> 5);
}

/* along a row */
static inline uint8_t halfHorizontal(const AvcdecInter *inter, uint32_t row, uint32_t column)
{
	return clipSample((sixTap(&inter->window[row][column], 1) + 16) >> 5);
}

/* the centre (j): the rows' sums unrounded, then down the column */
static inline uint8_t halfCentre(const AvcdecInter *inter, uint32_t row, uint32_t column)
{
	int32_t sums[6];
	uint32_t i;

	for (i = 0; i < 6; i++)
		sums[i] = (int16_t)sixTap(&inter->window[row + i][column], 1);
	return clipSample((sums[0] - 5 * sums[1] + 20 * sums[2] + 20 * sums[3] - 5 * sums[4] +
		sums[5] + 512) >> 10);
}

/*
 * The kernels' mode: 2 the half sample alone; 16 averaged with a whole or
 * half sample at its side, 1 the one right, 4 the one below; 8 that side
 * the centre's horizontal (i, k) rather than its vertical (f, q).
 */
#define LUMA_AVERAGE		0x10
#define LUMA_COLUMNS		0x08
#define LUMA_BELOW		0x04
#define LUMA_RIGHT		0x01

/* 0x896748: a 4x4 block's vertical half samples (h), or d and n */
void avcdecLumaVertical4(AvcdecInter *inter, uint32_t mode)
{
	uint32_t row, column, side = 2 + ((mode & LUMA_BELOW) != 0);
	uint8_t value;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++) {
			value = clipSample((sixTap(&inter->taps[row][column], 4) + 16) >> 5);
			if (mode & LUMA_AVERAGE)
				value = averageSamples(value, inter->taps[row + side][column]);
			inter->small[row][column] = value;
		}
}

/* 0x8967a8: horizontal ones (b), or a and c: down the turned taps, then turned back */
void avcdecLumaHorizontal4(AvcdecInter *inter, uint32_t mode)
{
	uint32_t row, column, side = 2 + ((mode & 3) == 3);
	uint8_t value;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++) {
			value = clipSample((sixTap(&inter->taps[column][row], 4) + 16) >> 5);
			if (mode & LUMA_AVERAGE)
				value = averageSamples(value, inter->taps[column + side][row]);
			inter->small[row][column] = value;
		}
}

/* the centre's (j), or f, q, i or k with the half sample beside */
static void centre(AvcdecInter *inter, uint8_t *out, uint32_t stride, uint32_t size,
	uint32_t mode)
{
	uint32_t row, column, side = 2 + ((mode & LUMA_BELOW) != 0);
	uint8_t value;

	for (row = 0; row < size; row++)
		for (column = 0; column < size; column++) {
			value = halfCentre(inter, row, column);
			if (mode & LUMA_AVERAGE) {
				if (mode & LUMA_COLUMNS)
					value = averageSamples(value,
						halfVertical(inter, row, column + side));
				else
					value = averageSamples(value,
						halfHorizontal(inter, row + side, column));
			}
			out[row * stride + column] = value;
		}
}

/* the diagonal quarter samples (e, g, p, r): a horizontal and a vertical half averaged */
static void diagonal(AvcdecInter *inter, uint8_t *out, uint32_t stride, uint32_t size,
	uint32_t mode)
{
	uint32_t row, column, below = 2 + ((mode & LUMA_BELOW) != 0);
	uint32_t right = 2 + (mode & LUMA_RIGHT);

	for (row = 0; row < size; row++)
		for (column = 0; column < size; column++)
			out[row * stride + column] = averageSamples(
				halfHorizontal(inter, row + below, column),
				halfVertical(inter, row, column + right));
}

/* 0x896820 */
void avcdecLumaCentre4(AvcdecInter *inter, uint32_t mode)
{
	centre(inter, inter->small[0], 4, 4, mode);
}

/* 0x896c18 */
void avcdecLumaDiagonal4(AvcdecInter *inter, uint32_t mode)
{
	diagonal(inter, inter->small[0], 4, 4, mode);
}

/* 0x896d90: an 8x8 block's vertical ones, the window from two rows above */
void avcdecLumaVertical8(AvcdecInter *inter, uint32_t mode)
{
	uint32_t row, column, side = 2 + ((mode & LUMA_BELOW) != 0);
	uint8_t value;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++) {
			value = halfVertical(inter, row, column);
			if (mode & LUMA_AVERAGE)
				value = averageSamples(value, inter->window[row + side][column]);
			inter->block[row][column] = value;
		}
}

/* 0x896ec8: horizontal ones, the window from two columns left */
void avcdecLumaHorizontal8(AvcdecInter *inter, uint32_t mode)
{
	uint32_t row, column, side = mode & 3;
	uint8_t value;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++) {
			value = halfHorizontal(inter, row, column);
			if (mode & LUMA_AVERAGE)
				value = averageSamples(value, inter->window[row][column + side]);
			inter->block[row][column] = value;
		}
}

/* 0x897068 */
void avcdecLumaCentre8(AvcdecInter *inter, uint32_t mode)
{
	centre(inter, inter->block[0], 8, 8, mode);
}

/* 0x897720 */
void avcdecLumaDiagonal8(AvcdecInter *inter, uint32_t mode)
{
	diagonal(inter, inter->block[0], 8, 8, mode);
}

/* 0x897a38: list 0's block, as the prediction stored it (16 apart) */
void avcdecLoadList0(AvcdecInter *inter, const uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			inter->list0[row][column] = samples[row * 16 + column];
}

/* 0x897a80: explicit weighting (8.4.2.3.2): ((p w + round) >> shift) + o */
void avcdecWeight(AvcdecInter *inter, int32_t weight, int32_t offset, int32_t round,
	int32_t shift)
{
	uint32_t row, column;
	int32_t value;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++) {
			value = inter->block[row][column] * weight + round;
			inter->block[row][column] = clipSample((value >> shift) + offset);
		}
}

/* 0x897ba8: both lists': ((p0 w0 + p1 w1 + round) >> shift) + o */
void avcdecWeightBi(AvcdecInter *inter, int32_t weight0, int32_t weight1, int32_t offset,
	int32_t round, int32_t shift)
{
	uint32_t row, column;
	int32_t value;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++) {
			value = inter->list0[row][column] * weight0 +
				inter->block[row][column] * weight1 + round;
			inter->block[row][column] = clipSample((value >> shift) + offset);
		}
}

/* 0x897d68: the default (8.4.2.3.1): the lists' blocks averaged */
void avcdecAverage(AvcdecInter *inter)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			inter->block[row][column] = averageSamples(inter->block[row][column],
				inter->list0[row][column]);
}

/* 0x897d90: a 4x4 block put in its place in the 8x8 one */
void avcdecPlace4(AvcdecInter *inter, uint32_t block)
{
	uint32_t row, column, top = (block & 2) * 2, left = (block & 1) * 4;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++)
			inter->block[top + row][left + column] = inter->small[row][column];
}

/*
 * The residual's kernels (0x898f80-0x89a768): the 4x4 inverse transform
 * (8.5.12) of the dequantised levels, in 16-bit saturating lanes, added to
 * the prediction; the Intra16x16 DC's Hadamard (8.5.10); the intra
 * predictions (8.3.1.2, 8.3.3).
 */

/* cpsadd3.h, cpssub3.h */
static inline int16_t addSaturate(int32_t a, int32_t b)
{
	int32_t sum = a + b;

	return sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum;
}

/* a quadrant of the 8x8 block: the residual added, clipped */
static void addQuadrant(AvcdecRecon *recon, uint32_t block)
{
	uint32_t row, column, top = (block & 2) * 2, left = (block & 1) * 4;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++)
			recon->block[top + row][left + column] =
				clipSample(recon->block[top + row][left + column] +
				recon->residual[row][column]);
}

/* 0x898f80: an inter macroblock's first 4x4 block of the 8x8 */
void avcdecAddResidual0(AvcdecRecon *recon)
{
	addQuadrant(recon, 0);
}

/* 0x898f98 */
void avcdecAddResidual1(AvcdecRecon *recon)
{
	addQuadrant(recon, 1);
}

/* 0x898fb0 */
void avcdecAddResidual2(AvcdecRecon *recon)
{
	addQuadrant(recon, 2);
}

/* 0x898fc8 */
void avcdecAddResidual3(AvcdecRecon *recon)
{
	addQuadrant(recon, 3);
}

/* an 8x8 block of the macroblock's samples (16 apart) */
static void loadBlock(AvcdecRecon *recon, const uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			recon->block[row][column] = samples[row * 16 + column];
}

static void storeBlock(const AvcdecRecon *recon, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			samples[row * 16 + column] = recon->block[row][column];
}

/* 0x898fe0: the prediction of an inter 8x8 block */
void avcdecLoadBlock(AvcdecRecon *recon, const uint8_t *samples)
{
	loadBlock(recon, samples);
}

/* 0x899030: and the block reconstructed back */
void avcdecStoreBlock(const AvcdecRecon *recon, uint8_t *samples)
{
	storeBlock(recon, samples);
}

/* 0x899080: its right column, for the intra macroblock right */
void avcdecKeepRight(const AvcdecRecon *recon, uint8_t *right)
{
	uint32_t row;

	for (row = 0; row < 8; row++)
		right[row] = recon->block[row][7];
}

/* 0x8990a0: a block without residual's, from the samples */
void avcdecKeepRightOf(const uint8_t *samples, uint8_t *right)
{
	uint32_t row;

	for (row = 0; row < 8; row++)
		right[row] = samples[row * 16 + 7];
}

/* the 4x4 transform of a row or column (a, b, c, d) in place, `step` apart */
static void inverse4(int16_t *x, uint32_t step)
{
	int16_t x0 = x[0], x1 = x[step], x2 = x[2 * step], x3 = x[3 * step];
	int16_t e = addSaturate(x0, x2), f = addSaturate(x0, -x2);
	int16_t g = addSaturate(x1 >> 1, -x3), h = addSaturate(x1, x3 >> 1);

	x[0] = addSaturate(e, h);
	x[step] = addSaturate(f, g);
	x[2 * step] = addSaturate(f, -g);
	x[3 * step] = addSaturate(e, -h);
}

/* the levels dequantised, rows then columns transformed, (x + 32) >> 6 */
static void inverseBlock(AvcdecRecon *recon, const int16_t *coef, uint32_t withDc)
{
	int16_t *x = recon->residual[0];
	uint32_t i;

	for (i = 0; i < 16; i++)
		x[i] = (int16_t)((coef[i] * recon->scale[i]) << recon->shift);
	if (withDc)
		x[0] = recon->blockDc[0];
	for (i = 0; i < 4; i++)
		inverse4(&x[i * 4], 1);
	for (i = 0; i < 4; i++)
		inverse4(&x[i], 4);
	for (i = 0; i < 16; i++)
		x[i] = addSaturate(x[i], 32) >> 6;
}

/*
 * 0x899110: an Intra16x16 macroblock's DC levels: the 4x4 Hadamard, then
 * dequantised ((f v) << (QP / 6 - 2), or rounded right below QP 12), by
 * 8x8 block.
 */
void avcdecDcIntra16(AvcdecRecon *recon, const int16_t *coef, uint32_t qp)
{
	int16_t f[16], t[16];
	int32_t value, shift;
	uint32_t i, row, column;

	for (i = 0; i < 4; i++) {
		t[i * 4] = coef[i * 4] + coef[i * 4 + 1] + coef[i * 4 + 2] + coef[i * 4 + 3];
		t[i * 4 + 1] = coef[i * 4] + coef[i * 4 + 1] - coef[i * 4 + 2] - coef[i * 4 + 3];
		t[i * 4 + 2] = coef[i * 4] - coef[i * 4 + 1] - coef[i * 4 + 2] + coef[i * 4 + 3];
		t[i * 4 + 3] = coef[i * 4] - coef[i * 4 + 1] + coef[i * 4 + 2] - coef[i * 4 + 3];
	}
	for (i = 0; i < 4; i++) {
		f[i] = t[i] + t[4 + i] + t[8 + i] + t[12 + i];
		f[4 + i] = t[i] + t[4 + i] - t[8 + i] - t[12 + i];
		f[8 + i] = t[i] - t[4 + i] - t[8 + i] + t[12 + i];
		f[12 + i] = t[i] - t[4 + i] + t[8 + i] - t[12 + i];
	}
	shift = avcdecQpShift[qp + 24];
	for (i = 0; i < 16; i++) {
		value = f[i] * avcdecLevelScale[avcdecQpRemainder[qp]][0];
		if (qp < 12)
			value = (value + (1 << (shift - 1))) >> shift;
		else
			value <<= shift;
		row = i >> 2;
		column = i & 3;
		recon->dc[(row & 2) + (column >> 1)][(row & 1) * 2 + (column & 1)] =
			(int16_t)value;
	}
}

/* 0x8992a0: an Intra16x16 4x4 block's: its AC levels and the DC */
void avcdecIdctAc(AvcdecRecon *recon, const int16_t *coef)
{
	inverseBlock(recon, coef, 1);
}

/* 0x8993b0: one with the DC alone */
void avcdecIdctDc(AvcdecRecon *recon)
{
	int16_t value = addSaturate(recon->blockDc[0], 32) >> 6;
	uint32_t row, column;

	for (row = 0; row < 4; row++)
		for (column = 0; column < 4; column++)
			recon->residual[row][column] = value;
}

/* 0x8993d8: a 4x4 block's levels */
void avcdecIdct(AvcdecRecon *recon, const int16_t *coef)
{
	inverseBlock(recon, coef, 0);
}

/* 0x8994d8: an Intra4x4 macroblock's 4x4 blocks */
void avcdecAddIntra0(AvcdecRecon *recon)
{
	addQuadrant(recon, 0);
}

/* 0x8994f8 */
void avcdecAddIntra1(AvcdecRecon *recon)
{
	addQuadrant(recon, 1);
}

/* 0x899518 */
void avcdecAddIntra2(AvcdecRecon *recon)
{
	addQuadrant(recon, 2);
}

/* 0x899538 */
void avcdecAddIntra3(AvcdecRecon *recon)
{
	addQuadrant(recon, 3);
}

/* 0x899558 */
void avcdecStoreIntra(const AvcdecRecon *recon, uint8_t *samples)
{
	storeBlock(recon, samples);
}

/*
 * 0x8995a8: an Intra4x4 macroblock's neighbours: the row above, the one
 * above right (or its last sample repeated where that macroblock is not
 * there), the column left; the first 8x8 block's of them.
 */
void avcdecIntraNeighbours(AvcdecRecon *recon, const AvcdecEdges *edges,
	uint32_t aboveRight)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		recon->mbAbove[i] = edges->above[i];
	for (i = 0; i < 8; i++)
		recon->mbAboveRight[i] = aboveRight ? edges[1].above[i] : edges->above[15];
	for (i = 0; i < 16; i++)
		recon->above[i] = edges->above[i];
	for (i = 0; i < 8; i++)
		recon->left[i] = edges->left[i];
	recon->aboveLeft = edges->aboveLeft;
}

/* 0x8995f0: the next 8x8 block's, from the block just reconstructed */
void avcdecIntraNext(AvcdecRecon *recon, uint32_t block, const AvcdecEdges *edges)
{
	uint32_t i;

	switch (block) {
	case 0:
		for (i = 0; i < 8; i++) {
			recon->bottom[i] = recon->block[7][i];
			recon->above[i] = recon->mbAbove[8 + i];
			recon->above[8 + i] = recon->mbAboveRight[i];
			recon->left[i] = recon->block[i][7];
		}
		recon->aboveLeft = recon->mbAbove[7];
		break;
	case 1:
		for (i = 0; i < 8; i++) {
			recon->bottom[8 + i] = recon->block[7][i];
			recon->above[i] = recon->bottom[i];
			recon->above[8 + i] = recon->bottom[8 + i];
			recon->left[i] = edges->left[8 + i];
		}
		recon->aboveLeft = edges->left[7];
		break;
	case 2:
		for (i = 0; i < 8; i++) {
			recon->above[i] = recon->bottom[8 + i];
			recon->above[8 + i] = recon->bottom[15];
			recon->left[i] = recon->block[i][7];
		}
		recon->aboveLeft = recon->bottom[7];
		break;
	}
}

/*
 * A 4x4 block's Intra4x4 prediction (8.3.1.2) by its mode, at (x, y) in
 * the 8x8 block: its neighbours the 8x8 block's or, inside it, the blocks
 * reconstructed before it; above right of the last block not there.
 */
static void intra4x4(AvcdecRecon *recon, uint32_t block, uint32_t mode, uint32_t leftAvailable,
	uint32_t aboveAvailable)
{
	uint32_t x0 = (block & 1) * 4, y0 = (block & 2) * 2, x, y, i, sum;
	int32_t edge[17], z;		/* the left column bottom up, the corner, the row above */
	int32_t *top = &edge[9], *side = &edge[7];	/* top[-1] the corner, side[-y] left */
	uint8_t *out;

	for (i = 0; i < 8; i++)
		top[i] = y0 ? (x0 && i >= 4 ? recon->block[y0 - 1][7] : recon->block[y0 - 1][x0 + i]) :
			recon->above[x0 + i];
	top[-1] = x0 ? (y0 ? recon->block[y0 - 1][x0 - 1] : recon->above[x0 - 1]) :
		(y0 ? recon->left[y0 - 1] : recon->aboveLeft);
	for (i = 0; i < 4; i++)
		side[-(int32_t)i] = x0 ? recon->block[y0 + i][x0 - 1] : recon->left[y0 + i];
	leftAvailable |= x0;
	aboveAvailable |= y0;
	for (y = 0; y < 4; y++) {
		out = &recon->block[y0 + y][x0];
		for (x = 0; x < 4; x++) {
			switch (mode) {
			case 0:		/* vertical */
				out[x] = top[x];
				break;
			case 1:		/* horizontal */
				out[x] = side[-(int32_t)y];
				break;
			case 2:		/* DC */
				sum = 0;
				for (i = 0; i < 4; i++)
					sum += top[i] + side[-(int32_t)i];
				if (leftAvailable && aboveAvailable)
					out[x] = (sum + 4) >> 3;
				else if (leftAvailable)
					out[x] = (sum - top[0] - top[1] - top[2] - top[3] + 2) >> 2;
				else if (aboveAvailable)
					out[x] = (top[0] + top[1] + top[2] + top[3] + 2) >> 2;
				else
					out[x] = 128;
				break;
			case 3:		/* diagonal down left */
				if (x == 3 && y == 3)
					out[x] = (top[6] + 3 * top[7] + 2) >> 2;
				else
					out[x] = (top[x + y] + 2 * top[x + y + 1] + top[x + y + 2] + 2) >> 2;
				break;
			case 4:		/* diagonal down right: along the edge, the corner between */
				z = (int32_t)x - (int32_t)y;
				out[x] = (edge[9 + z - 2] + 2 * edge[9 + z - 1] + edge[9 + z] + 2) >> 2;
				break;
			case 5:		/* vertical right */
				z = 2 * (int32_t)x - (int32_t)y;
				if (z >= 0 && !(z & 1))
					out[x] = (top[x - (y >> 1) - 1] + top[x - (y >> 1)] + 1) >> 1;
				else if (z >= 0)
					out[x] = (top[x - (y >> 1) - 2] + 2 * top[x - (y >> 1) - 1] +
						top[x - (y >> 1)] + 2) >> 2;
				else if (z == -1)
					out[x] = (side[0] + 2 * top[-1] + top[0] + 2) >> 2;
				else
					out[x] = (side[-(int32_t)y + 1] + 2 * side[-(int32_t)y + 2] +
						side[-(int32_t)y + 3] + 2) >> 2;
				break;
			case 6:		/* horizontal down */
				z = 2 * (int32_t)y - (int32_t)x;
				if (z >= 0 && !(z & 1))
					out[x] = (side[-(int32_t)(y - (x >> 1)) + 1] +
						side[-(int32_t)(y - (x >> 1))] + 1) >> 1;
				else if (z >= 0)
					out[x] = (side[-(int32_t)(y - (x >> 1)) + 2] +
						2 * side[-(int32_t)(y - (x >> 1)) + 1] +
						side[-(int32_t)(y - (x >> 1))] + 2) >> 2;
				else if (z == -1)
					out[x] = (side[0] + 2 * top[-1] + top[0] + 2) >> 2;
				else
					out[x] = (top[x - 1] + 2 * top[x - 2] + top[x - 3] + 2) >> 2;
				break;
			case 7:		/* vertical left */
				if (!(y & 1))
					out[x] = (top[x + (y >> 1)] + top[x + (y >> 1) + 1] + 1) >> 1;
				else
					out[x] = (top[x + (y >> 1)] + 2 * top[x + (y >> 1) + 1] +
						top[x + (y >> 1) + 2] + 2) >> 2;
				break;
			default:	/* horizontal up */
				z = (int32_t)x + 2 * (int32_t)y;
				if (z > 5)
					out[x] = side[-3];
				else if (z == 5)
					out[x] = (side[-2] + 3 * side[-3] + 2) >> 2;
				else if (!(z & 1))
					out[x] = (side[-(int32_t)(y + (x >> 1))] +
						side[-(int32_t)(y + (x >> 1)) - 1] + 1) >> 1;
				else
					out[x] = (side[-(int32_t)(y + (x >> 1))] +
						2 * side[-(int32_t)(y + (x >> 1)) - 1] +
						side[-(int32_t)(y + (x >> 1)) - 2] + 2) >> 2;
				break;
			}
		}
	}
}

/* 0x899670: the 8x8 block's first 4x4 block */
void avcdecIntra4x4Block0(AvcdecRecon *recon, uint32_t mode, uint32_t left, uint32_t above)
{
	intra4x4(recon, 0, mode, left, above);
}

/* 0x8997e8 */
void avcdecIntra4x4Block1(AvcdecRecon *recon, uint32_t mode, uint32_t above)
{
	intra4x4(recon, 1, mode, 1, above);
}

/* 0x899940 */
void avcdecIntra4x4Block2(AvcdecRecon *recon, uint32_t mode, uint32_t left)
{
	intra4x4(recon, 2, mode, left, 1);
}

/* 0x899a90 */
void avcdecIntra4x4Block3(AvcdecRecon *recon, uint32_t mode)
{
	intra4x4(recon, 3, mode, 1, 1);
}

/*
 * 0x899bb8: what the macroblocks after an Intra4x4 one predict from: its
 * right column and the corner for the one right, its last row for the one
 * below.
 */
void avcdecIntraKeep(const AvcdecRecon *recon, const uint8_t *samples, AvcdecEdges *edges)
{
	uint32_t i;

	edges[1].aboveLeft = recon->mbAbove[15];
	for (i = 0; i < 16; i++) {
		edges->above[i] = samples[240 + i];
		edges[1].left[i] = samples[i * 16 + 15];
	}
}

/* 0x899bf8: an Intra16x16 macroblock's 8x8 block, predicted */
void avcdecLoadIntra16(AvcdecRecon *recon, const uint8_t *samples)
{
	loadBlock(recon, samples);
}

/* the next 4x4 block's DC first */
static void nextDc(AvcdecRecon *recon)
{
	recon->blockDc[0] = recon->blockDc[1];
	recon->blockDc[1] = recon->blockDc[2];
	recon->blockDc[2] = recon->blockDc[3];
}

/* 0x899c48 */
void avcdecAddIntra16Block0(AvcdecRecon *recon)
{
	addQuadrant(recon, 0);
	nextDc(recon);
}

/* 0x899c70 */
void avcdecAddIntra16Block1(AvcdecRecon *recon)
{
	addQuadrant(recon, 1);
	nextDc(recon);
}

/* 0x899c98 */
void avcdecAddIntra16Block2(AvcdecRecon *recon)
{
	addQuadrant(recon, 2);
	nextDc(recon);
}

/* 0x899cc0 */
void avcdecAddIntra16Block3(AvcdecRecon *recon)
{
	addQuadrant(recon, 3);
}

/* 0x899ce0 */
void avcdecStoreIntra16(const AvcdecRecon *recon, uint8_t *samples)
{
	storeBlock(recon, samples);
}

/* 0x899d30: the block's right column, `*right` moved on past it */
void avcdecKeepRightIntra16(const AvcdecRecon *recon, uint8_t **right)
{
	avcdecKeepRight(recon, *right);
	*right += 8;
}

/* 0x899d58: Intra16x16 vertical: the row above down the macroblock */
void avcdecIntra16Vertical(const AvcdecEdges *edges, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 16; row++)
		for (column = 0; column < 16; column++)
			samples[row * 16 + column] = edges->above[column];
}

/* 0x899d98: horizontal */
void avcdecIntra16Horizontal(const uint8_t *left, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 16; row++)
		for (column = 0; column < 16; column++)
			samples[row * 16 + column] = left[row];
}

/* 0x899e30: DC, of the neighbours there */
void avcdecIntra16Dc(const uint8_t *left, const AvcdecEdges *edges, uint32_t leftAvailable,
	uint32_t aboveAvailable, uint8_t *samples)
{
	uint32_t sumLeft = 0, sumAbove = 0, i, value;

	for (i = 0; i < 16; i++) {
		sumLeft += left[i];
		sumAbove += edges->above[i];
	}
	if (leftAvailable && aboveAvailable)
		value = (sumLeft + sumAbove + 16) >> 5;
	else if (leftAvailable)
		value = (sumLeft + 8) >> 4;
	else if (aboveAvailable)
		value = (sumAbove + 8) >> 4;
	else
		value = 128;
	for (i = 0; i < 256; i++)
		samples[i] = value;
}

/* 0x899f98: plane */
void avcdecIntra16Plane(const uint8_t *left, const AvcdecEdges *edges, uint32_t aboveLeft,
	uint8_t *samples)
{
	const uint8_t *above = edges->above;
	int32_t h = 0, v = 0, a, b, c, x, y;

	for (x = 0; x < 8; x++) {
		h += (x + 1) * (above[8 + x] - (x < 7 ? above[6 - x] : (int32_t)aboveLeft));
		v += (x + 1) * (left[8 + x] - (x < 7 ? left[6 - x] : (int32_t)aboveLeft));
	}
	a = 16 * (left[15] + above[15]);
	b = (5 * h + 32) >> 6;
	c = (5 * v + 32) >> 6;
	for (y = 0; y < 16; y++)
		for (x = 0; x < 16; x++)
			samples[y * 16 + x] = clipSample((a + b * (x - 7) + c * (y - 7) + 16) >> 5);
}

/*
 * The luma's deblocking kernels (0x897dd8-0x898f80, 8.7.2): half a
 * macroblock in the coprocessor, its 8x8 halves turned so its vertical
 * edges are filtered as rows; each edge's four lines of eight samples by
 * their bS (two 4-sample segments), indexA's alpha and tC0 and indexB's
 * beta.
 */

/* alpha' (Table 8-16) by indexA */
static const uint8_t deblockAlpha[52] = {			/* 0x8f7478 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 5, 6, 7, 8, 9, 10, 12, 13,
	15, 17, 20, 22, 25, 28, 32, 36, 40, 45, 50, 56, 63, 71, 80, 90, 101, 113, 127, 144,
	162, 182, 203, 226, 255, 255,
};

/* beta' by indexB */
static const uint8_t deblockBeta[52] = {			/* 0x8f74ac */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
	6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17,
	18, 18,
};

/* tC0' (Table 8-17) by bS and indexA */
static const uint8_t deblockTc0[4][52] = {			/* 0x8f73a8 */
	{ 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
	  1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 6, 6, 7, 8, 9, 10, 11, 13 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
	  1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 7, 8, 8, 10, 11, 12, 13, 15, 17 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	  1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 23, 25 },
};

static inline int32_t absolute(int32_t value)
{
	return value < 0 ? -value : value;
}

static inline int32_t clip3(int32_t low, int32_t high, int32_t value)
{
	return value < low ? low : value > high ? high : value;
}

/*
 * One line across an edge, p[0] p3 to p[3] p0 and q[0] q0 to q[3] q3,
 * `step` apart: filtered by its bS (8.7.2.3, 8.7.2.4); 1 where it is.
 */
static uint32_t filterLine(uint8_t *const p[4], uint8_t *const q[4], uint32_t offset,
	uint32_t bS, uint32_t indexA, uint32_t indexB)
{
	int32_t p3 = p[0][offset], p2 = p[1][offset], p1 = p[2][offset], p0 = p[3][offset];
	int32_t q0 = q[0][offset], q1 = q[1][offset], q2 = q[2][offset], q3 = q[3][offset];
	int32_t alpha = deblockAlpha[indexA], beta = deblockBeta[indexB], tc0, tc, delta;
	uint32_t ap, aq;

	if (!bS || absolute(p0 - q0) >= alpha || absolute(p1 - p0) >= beta ||
	    absolute(q1 - q0) >= beta)
		return 0;
	ap = absolute(p2 - p0) < beta;
	aq = absolute(q2 - q0) < beta;
	if (bS < 4) {
		tc0 = deblockTc0[bS][indexA];
		tc = tc0 + ap + aq;
		delta = clip3(-tc, tc, (((q0 - p0) << 2) + (p1 - q1) + 4) >> 3);
		p[3][offset] = clipSample(p0 + delta);
		q[0][offset] = clipSample(q0 - delta);
		if (ap)
			p[2][offset] = p1 + clip3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1);
		if (aq)
			q[1][offset] = q1 + clip3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1);
		return 1;
	}
	if (ap && absolute(p0 - q0) < ((alpha >> 2) + 2)) {
		p[3][offset] = (p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3;
		p[2][offset] = (p2 + p1 + p0 + q0 + 2) >> 2;
		p[1][offset] = (2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3;
	} else {
		p[3][offset] = (2 * p1 + p0 + q1 + 2) >> 2;
	}
	if (aq && absolute(p0 - q0) < ((alpha >> 2) + 2)) {
		q[0][offset] = (p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3;
		q[1][offset] = (p0 + q0 + q1 + q2 + 2) >> 2;
		q[2][offset] = (2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3;
	} else {
		q[0][offset] = (2 * q1 + q0 + p1 + 2) >> 2;
	}
	return 1;
}

/* an edge's eight lines; the lanes filtered, by bit */
static uint32_t filterEdge(uint8_t *const p[4], uint8_t *const q[4], uint32_t indexA,
	uint32_t indexB, uint32_t strengths)
{
	uint32_t lane, filtered = 0;

	for (lane = 0; lane < 8; lane++)
		if (filterLine(p, q, lane, (strengths >> (lane & 4) * 2) & 0xff, indexA, indexB))
			filtered |= 1 << lane;
	return filtered;
}

/* 0x897dd8: half the macroblock's samples */
void avcdecDeblockLoad(AvcdecDeblock *deblock, const uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 16; column++)
			deblock->rows[row][column] = samples[row * 16 + column];
}

/* 0x897e60 */
void avcdecDeblockStore(const AvcdecDeblock *deblock, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 16; column++)
			samples[row * 16 + column] = deblock->rows[row][column];
}

/* 0x897ef0: the left macroblock's columns filtered, turned back into its rows */
void avcdecDeblockStoreLeft(const AvcdecDeblock *deblock, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 4; column++)
			samples[row * 16 + column] = deblock->p[column][row];
}

/* 0x897f50: the macroblock's right four columns, turned, for the next one's left edge */
void avcdecDeblockKeepLeft(AvcdecStripe *stripe)
{
	const uint8_t *luma = stripe->luma;
	uint32_t half, column, row;

	for (half = 0; half < 2; half++)
		for (column = 0; column < 4; column++)
			for (row = 0; row < 8; row++)
				stripe->rowData->leftColumns[half][column][row] =
					luma[(half * 8 + row) * 16 + 12 + column];
}

/* an 8x8 half turned over its diagonal */
static void turn(AvcdecDeblock *deblock, uint32_t left)
{
	uint32_t row, column;
	uint8_t sample;

	for (row = 0; row < 8; row++)
		for (column = row + 1; column < 8; column++) {
			sample = deblock->rows[row][left + column];
			deblock->rows[row][left + column] = deblock->rows[column][left + row];
			deblock->rows[column][left + row] = sample;
		}
}

/* 0x898028 */
void avcdecDeblockTurnLeft(AvcdecDeblock *deblock)
{
	turn(deblock, 0);
}

/* 0x898088 */
void avcdecDeblockTurnRight(AvcdecDeblock *deblock)
{
	turn(deblock, 8);
}

/* four rows of the half from `row`, from `column` */
static inline void edgeRows(AvcdecDeblock *deblock, uint8_t *rows[4], uint32_t row,
	uint32_t column)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		rows[i] = &deblock->rows[row + i][column];
}

/* 0x898200: a macroblock edge: p the neighbour's, q the half's left 8x8 (bS 4 too) */
uint32_t avcdecDeblockEdge(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4] = { deblock->p[0], deblock->p[1], deblock->p[2], deblock->p[3] }, *q[4];

	edgeRows(deblock, q, 0, 0);
	return filterEdge(p, q, indexA, indexB, strengths);
}

/* 0x898520: q the right 8x8 */
uint32_t avcdecDeblockEdgeRight(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4] = { deblock->p[0], deblock->p[1], deblock->p[2], deblock->p[3] }, *q[4];

	edgeRows(deblock, q, 0, 8);
	return filterEdge(p, q, indexA, indexB, strengths);
}

/* 0x898840: the edge inside the left 8x8 */
void avcdecDeblockInner(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4], *q[4];

	edgeRows(deblock, p, 0, 0);
	edgeRows(deblock, q, 4, 0);
	filterEdge(p, q, indexA, indexB, strengths);
}

/* 0x898a10: inside the right 8x8 */
void avcdecDeblockInnerRight(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4], *q[4];

	edgeRows(deblock, p, 0, 8);
	edgeRows(deblock, q, 4, 8);
	filterEdge(p, q, indexA, indexB, strengths);
}

/* 0x898be0: between the turned 8x8 halves (the vertical edge in the middle) */
void avcdecDeblockMiddle(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4], *q[4];

	edgeRows(deblock, p, 4, 0);
	edgeRows(deblock, q, 0, 8);
	filterEdge(p, q, indexA, indexB, strengths);
}

/* 0x898db0: inside the turned right 8x8 (the last vertical edge) */
void avcdecDeblockInnerLast(AvcdecDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	uint8_t *p[4], *q[4];

	edgeRows(deblock, p, 0, 8);
	edgeRows(deblock, q, 4, 8);
	filterEdge(p, q, indexA, indexB, strengths);
}
