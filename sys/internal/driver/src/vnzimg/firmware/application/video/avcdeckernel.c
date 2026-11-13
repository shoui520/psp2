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
