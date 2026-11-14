/*
 * The AVC encoder's inter decision (0x866524-0x866f60), for each macroblock
 * of the decide job's groups: the window of the reference around the coarse
 * search's vector, the SADs of the candidates around it over a checkerboard
 * of the macroblock's pixels, the best of them for each partition, the
 * whole macroblock's refined a pixel and checked for P_Skip, then the
 * partition chosen by its cost and its vectors refined a pixel
 */
#include "avcencstream.h"

/* the window's band from its first line: the reference's lines from the clamped place */
static inline void windowBand(AvcencDecideInfo *info, AvcencInterBand *band, int32_t left,
	int32_t top)
{
	uint32_t pitch = info->referencePitch;
	const uint8_t *first, *second, *third, *fourth;

	first = (const uint8_t *)(info->reference + (top & -16) * pitch + ((top & 15) << 4) +
		((left & -16) << 4) + (left & 8));
	second = left & 8 ? first + 248 : first + 8;
	third = (top & 15) == 12 ? first + pitch * 16 - 192 : first + 64;
	fourth = (top & 15) == 12 ? second + pitch * 16 - 192 : second + 64;
	if (!(left & 7))
		avcencInterBandLoad(band, first, second, third, fourth);
	else
		avcencInterBandLoadShifted(band, first, second, third, fourth);
}

/* the band's pixels out of the picture's left or right, for the window's place */
static inline void bandEdges(AvcencInterBand *band, int32_t x, int32_t width)
{
	if (x < 1) {
		if (x == 0)
			avcencInterBandLeft4(band);
		else if (x == -4)
			avcencInterBandLeft8(band);
		else
			avcencInterBandLeft12(band);
	} else if (x >= width - 16) {
		if (x == width - 16)
			avcencInterBandRight4(band);
		else if (x == width - 12)
			avcencInterBandRight8(band);
		else
			avcencInterBandRight12(band);
	}
}

/*
 * 0x866576: the window at the vector, three bands of eight lines from four
 * pixels left of and above it, what is out of the picture its edges'
 */
static void loadWindow(AvcencDecideInfo *info, int32_t x, int32_t y)
{
	int32_t width = info->width, height = info->height, left, top;
	AvcencInterBand band;

	left = x >= 4 ? x - 4 : 0;
	if (left > width - 24)
		left = width - 24;
	top = y >= 4 ? y - 4 : 0;
	if (top > height - 8)
		top = height - 8;
	windowBand(info, &band, left, top);
	bandEdges(&band, x, width);
	if (y < -3)
		avcencInterBandTop8(&band);
	else if (y == 0)
		avcencInterBandTop4(&band);
	avcencInterBandStore(&band, info->window);

	top = y + 8 >= 4 ? y + 4 : 0;
	if (top > height - 8)
		top = height - 8;
	windowBand(info, &band, left, top);
	bandEdges(&band, x, width);
	if (y < -7)
		avcencInterBandTop4(&band);
	else if (y >= height - 8)
		avcencInterBandBottom4(&band);
	avcencInterBandStore(&band, info->window + 8);

	top = y + 16 >= 4 ? y + 12 : 0;
	if (top > height - 8)
		top = height - 8;
	windowBand(info, &band, left, top);
	bandEdges(&band, x, width);
	if (y == height - 16)
		avcencInterBandBottom4(&band);
	else if (y >= height - 12)
		avcencInterBandBottom8(&band);
	avcencInterBandStore(&band, info->window + 16);
}

/* 0x866558: the macroblock's vector, the window at it */
static void windowAtVector(AvcencDecideInfo *info, const int16_t *position,
	const int16_t *origin)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interOutput;

	record->vector[0] = position[0] - origin[0];
	record->vector[1] = position[1] - origin[1];
	loadWindow(info, position[0], position[1]);
}

/*
 * 0x866874: the partition of the lowest cost, the whole macroblock unless
 * a smaller one is lower, and its blocks' deltas
 */
static void choosePartition(AvcencDecideInfo *info, const AvcencInterBest *best)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interOutput;
	const int8_t (*deltas)[2] = best->deltas;
	const int8_t *chosen[4];
	uint16_t costs[4], sads[4], cost, sad;
	uint8_t partition;
	int32_t i;

	avcencInterPartitionCosts(best, info->interWeight, costs, sads);
	if (costs[2] < costs[3]) {
		partition = 1;
		cost = costs[2];
		sad = sads[2];
		chosen[0] = chosen[1] = deltas[INTER_HALF_TOP];
		chosen[2] = chosen[3] = deltas[INTER_HALF_BOTTOM];
	} else {
		partition = 0;
		cost = costs[3];
		sad = sads[3];
		for (i = 0; i < 4; i++)
			chosen[i] = record->deltas[i];
	}
	if (costs[1] < cost) {
		partition = 2;
		sad = sads[1];
		cost = costs[1];
		chosen[0] = chosen[2] = deltas[INTER_HALF_LEFT];
		chosen[1] = chosen[3] = deltas[INTER_HALF_RIGHT];
	}
	if (costs[0] < cost) {
		partition = 3;
		sad = sads[0];
		cost = costs[0];
		for (i = 0; i < 4; i++)
			chosen[i] = deltas[i];
	}
	info->interCost = cost;
	record->partition = partition;
	record->sad = sad;
	for (i = 0; i < 4; i++) {
		record->deltas[i][0] = chosen[i][0];
		record->deltas[i][1] = chosen[i][1];
	}
}

/*
 * 0x866904: the candidates' SADs, the macroblock's top half against the
 * window's first lines, then its bottom half, two lines of candidates at a
 * time
 */
static void searchSads(AvcencDecideInfo *info)
{
	const uint8_t *window = &info->window[0][0], *luma = (const uint8_t *)info->interLuma;
	AvcencInterSearch search;
	int32_t i;

	avcencInterSearchLines(&search, window);
	avcencInterSearchSource(&search, luma);
	for (i = 0; i < 4; i++) {
		avcencInterSearchRows(&search, info->sads + 72 * i);
		avcencInterSearchNext(&search, window + 24 * (9 + 2 * i));
	}
	avcencInterSearchLastRow(&search, info->sads + 288);
	avcencInterSearchSource(&search, luma + 128);
	for (i = 0; i < 4; i++) {
		avcencInterSearchRows(&search, info->sads + 2 + 72 * i);
		avcencInterSearchNext(&search, window + 24 * (17 + 2 * i));
	}
	avcencInterSearchLastRow(&search, info->sads + 290);
}

/* 0x86698a: four lines of the reference's chroma, at a place in its tiles */
static void loadChroma(AvcencInterSkip *skip, uint32_t reference, uint32_t pitch, int32_t x,
	int32_t y)
{
	const uint8_t *tile = (const uint8_t *)(reference + (y >> 3) * pitch * 8 + (x >> 3) * 128 +
		(y & 7) * 8);

	if (x & 7)
		avcencInterSkipChromaShifted(skip, tile, tile + 128, pitch, y & 7, x & 7);
	else
		avcencInterSkipChroma(skip, tile, pitch, y & 7);
}

/*
 * 0x8669ce: P_Skip at the whole macroblock's vector: every 4x4 block's
 * residual of the luma, and of two of each chroma part's (at the delta from
 * the macroblock), under the thresholds
 */
static int32_t checkSkip(AvcencDecideInfo *info, int32_t dx, int32_t dy)
{
	const int16_t *threshold = info->lumaThreshold;
	const uint8_t *luma = (const uint8_t *)info->interLuma;
	int32_t width, height, x, y, cx, cy, i;
	AvcencInterSkip skip;

	for (i = 0; i < 8; i++)
		skip.thresholds[i >> 2][i & 3] =
			(uint16_t)threshold[i] >> (info->lumaShift & 15);
	for (i = 0; i < 4; i++) {
		avcencInterSkipLoad(&skip, (const uint8_t (*)[24])info->window + dy + 4 + 4 * i, dx + 4,
			dy & 1);
		if (!avcencInterSkipLuma(&skip, luma + 64 * i))
			return 0;
	}
	width = info->width >> 1;
	height = info->height >> 1;
	cx = (dx + info->interX * 16) >> 1;
	cy = (dy + info->interY * 16) >> 1;
	x = 0;
	if (cx >= 0) {
		x = cx;
		if (x > width - 8)
			x = width - 8;
	}
	y = 0;
	if (cy >= 0) {
		y = cy;
		if (y > height - 4)
			y = height - 4;
	}
	loadChroma(&skip, info->referenceChroma, info->referenceChromaPitch, x, y);
	avcencInterSkipChromaEdges(&skip, cx, cy, width, height);
	if (!avcencInterSkipChromaCheck(&skip, (const uint8_t *)info->interChroma, 1))
		return 0;
	cy += 4;
	y = 0;
	if (cy >= 0) {
		y = cy;
		if (y > height - 4)
			y = height - 4;
	}
	loadChroma(&skip, info->referenceChroma, info->referenceChromaPitch, x, y);
	avcencInterSkipChromaEdges(&skip, cx, cy, width, height);
	return avcencInterSkipChromaCheck(&skip, (const uint8_t *)info->interChroma + 32, 0) != 0;
}

static void neighbourSads(AvcencDecideInfo *info, uint16_t *sads, uint32_t first, uint32_t second,
	uint16_t (*lanes)[4]);

/*
 * 0x866b2e: the whole macroblock's delta a pixel to the neighbour of the
 * lowest SAD, if lower; out of the picture it is intra, under the skip
 * thresholds P_Skip. Otherwise the search's result is kept for the
 * partitions
 */
static int32_t decideWhole(AvcencDecideInfo *info, AvcencInterBest *best)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interOutput;
	uint16_t lanes[4][4], up, down, left, right;
	int32_t dx, dy, x, y, i;
	uint32_t lowest, chosen;

	info->best = *best;
	neighbourSads(info, info->neighbours[0], INTER_WHOLE, INTER_WHOLE, lanes);
	dx = info->best.deltas[INTER_WHOLE][0];
	dy = info->best.deltas[INTER_WHOLE][1];
	up = lanes[2][0] + lanes[0][0] + lanes[2][1] + lanes[0][1];
	down = lanes[2][2] + lanes[0][2] + lanes[2][3] + lanes[0][3];
	left = lanes[3][0] + lanes[1][0] + lanes[3][1] + lanes[1][1];
	right = lanes[3][2] + lanes[1][2] + lanes[3][3] + lanes[1][3];
	lowest = (uint16_t)info->best.whole[0] - 2 * ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
	chosen = 5;
	if (up < lowest) {
		chosen = 0;
		lowest = up;
	}
	if (down < lowest) {
		chosen = 1;
		lowest = down;
	}
	if (left < lowest) {
		chosen = 2;
		lowest = left;
	}
	if (right < lowest)
		chosen = 3;
	if (chosen == 3)
		dx++;
	else if (chosen == 2)
		dx--;
	else if (chosen == 1)
		dy++;
	else if (chosen == 0)
		dy--;
	if (info->scaled) {
		x = (info->interX << 4) + record->vector[0] + dx;
		y = (info->interY << 4) + record->vector[1] + dy;
		if (x < -2 || y < -2 || info->width < x + 14 || info->height < y + 14) {
			record->intra = 1;
			record->skip = 0;
			record->sad = 32767;
			return 1;
		}
	}
	for (i = 0; i < 4; i++) {
		record->deltas[i][0] = dx;
		record->deltas[i][1] = dy;
		record->vectors[i][0] = dx * 4;
		record->vectors[i][1] = dy * 4;
	}
	if (checkSkip(info, dx, dy)) {
		record->skip = 1;
		record->intra = 0;
		record->partition = 0;
		record->sad = 0;
		return 1;
	}
	record->intra = 0;
	record->skip = 0;
	*best = info->best;
	return 0;
}

/*
 * 0x866c8c: a partition's delta a pixel to the neighbour of the lowest SAD,
 * if under the partition's: the lanes the partition's SADs are in
 */
static void refineDelta(AvcencDecideInfo *info, uint32_t index, const uint16_t *vertical,
	const uint16_t *horizontal)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interOutput;
	uint32_t lowest = record->sad, chosen, sads[4];
	int8_t *delta;

	switch (index) {
	case 0:
	case 2:
	case 4:
		sads[0] = vertical[0];
		sads[1] = vertical[2];
		sads[2] = horizontal[0];
		sads[3] = horizontal[2];
		break;
	case 6:
	case 7:
	case 8:
		sads[0] = vertical[0];
		sads[1] = vertical[1];
		sads[2] = horizontal[0];
		sads[3] = horizontal[1];
		break;
	default:
		sads[0] = vertical[1];
		sads[1] = vertical[3];
		sads[2] = horizontal[1];
		sads[3] = horizontal[3];
		break;
	}
	chosen = 5;
	if (sads[0] < lowest) {
		chosen = 0;
		lowest = sads[0];
	}
	if (sads[1] < lowest) {
		chosen = 1;
		lowest = sads[1];
	}
	if (sads[2] < lowest) {
		chosen = 2;
		lowest = sads[2];
	}
	if (sads[3] < lowest)
		chosen = 3;
	delta = info->best.deltas[index];
	if (chosen == 3)
		delta[0]++;
	else if (chosen == 2)
		delta[0]--;
	else if (chosen == 1)
		delta[1]++;
	else if (chosen == 0)
		delta[1]--;
}

/* two partitions' SADs summed, lane by lane */
static inline void sumLanes(uint16_t *sum, const uint16_t *first, const uint16_t *second)
{
	int32_t i;

	for (i = 0; i < 4; i++)
		sum[i] = first[i] + second[i];
}

/* and a block's pair */
static inline void pairLanes(uint16_t *sum, const uint16_t *lanes)
{
	sum[0] = lanes[0] + lanes[1];
	sum[1] = lanes[2] + lanes[3];
	sum[2] = 0;
	sum[3] = 0;
}

/* 0x866d58: the chosen partition's deltas refined a pixel, into the record */
static void refinePartition(AvcencDecideInfo *info)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interOutput;
	const int8_t (*deltas)[2] = (const int8_t (*)[2])info->best.deltas;
	uint16_t lanes[4][4], vertical[4], horizontal[4];
	const int8_t *chosen[4];
	int32_t i;

	switch (record->partition) {
	case 3:
		neighbourSads(info, info->neighbours[0], 0, 2, lanes);
		refineDelta(info, 0, lanes[0], lanes[1]);
		refineDelta(info, 2, lanes[2], lanes[3]);
		neighbourSads(info, info->neighbours[1], 1, 3, lanes);
		refineDelta(info, 1, lanes[0], lanes[1]);
		refineDelta(info, 3, lanes[2], lanes[3]);
		for (i = 0; i < 4; i++)
			chosen[i] = deltas[i];
		break;
	case 2:
		neighbourSads(info, info->neighbours[0], INTER_HALF_LEFT, INTER_HALF_LEFT, lanes);
		sumLanes(vertical, lanes[2], lanes[0]);
		sumLanes(horizontal, lanes[3], lanes[1]);
		refineDelta(info, INTER_HALF_LEFT, vertical, horizontal);
		neighbourSads(info, info->neighbours[1], INTER_HALF_RIGHT, INTER_HALF_RIGHT, lanes);
		sumLanes(vertical, lanes[2], lanes[0]);
		sumLanes(horizontal, lanes[3], lanes[1]);
		refineDelta(info, INTER_HALF_RIGHT, vertical, horizontal);
		chosen[0] = chosen[2] = deltas[INTER_HALF_LEFT];
		chosen[1] = chosen[3] = deltas[INTER_HALF_RIGHT];
		break;
	case 1:
		neighbourSads(info, info->neighbours[0], INTER_HALF_TOP, INTER_HALF_BOTTOM, lanes);
		pairLanes(vertical, lanes[0]);
		pairLanes(horizontal, lanes[1]);
		refineDelta(info, INTER_HALF_TOP, vertical, horizontal);
		pairLanes(vertical, lanes[2]);
		pairLanes(horizontal, lanes[3]);
		refineDelta(info, INTER_HALF_BOTTOM, vertical, horizontal);
		chosen[0] = chosen[1] = deltas[INTER_HALF_TOP];
		chosen[2] = chosen[3] = deltas[INTER_HALF_BOTTOM];
		break;
	default:
		return;
	}
	for (i = 0; i < 4; i++) {
		record->deltas[i][0] = chosen[i][0];
		record->deltas[i][1] = chosen[i][1];
	}
}

/* a delta's neighbours out of the window: their lanes, the two blocks', unavailable */
static inline void outOfWindow(uint16_t *lanes, int32_t delta)
{
	if (delta == -4)
		lanes[0] = lanes[1] = INTER_UNAVAILABLE;
	else if (delta == 4)
		lanes[2] = lanes[3] = INTER_UNAVAILABLE;
}

/* a half's neighbours: the other pixels, against the window's lines around the delta */
static inline void halfNeighbours(AvcencDecideInfo *info, uint16_t *sads, const uint8_t *luma,
	const int8_t *delta, int32_t line)
{
	const uint8_t *window = &info->window[0][0];
	AvcencInterSearch search;

	avcencInterSearchSourceOther(&search, luma);
	avcencInterSearchLines(&search, window + 24 * (delta[1] + line));
	avcencInterSearchShift(&search, window + 24 * (delta[1] + line + 9));
	if (delta[1] & 1)
		avcencInterNeighboursOdd(&search, sads, (delta[0] + 4) >> 1);
	else
		avcencInterNeighboursEven(&search, sads, (delta[0] + 4) >> 1);
}

/*
 * 0x866e34: the SADs at the neighbours of two partitions' deltas, the
 * first's over the macroblock's top half and the second's over its bottom
 * half, those out of the window unavailable: by lanes, each half's above
 * and below, then left and right, each of its two blocks
 */
static void neighbourSads(AvcencDecideInfo *info, uint16_t *sads, uint32_t first, uint32_t second,
	uint16_t (*lanes)[4])
{
	const uint8_t *luma = (const uint8_t *)info->interLuma;
	const int8_t *top = info->best.deltas[first], *bottom = info->best.deltas[second];
	int32_t i, j;

	halfNeighbours(info, sads, luma, top, 3);
	halfNeighbours(info, sads + 8, luma + 128, bottom, 11);
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			lanes[i][j] = sads[4 * i + j];
	outOfWindow(lanes[1], top[0]);
	outOfWindow(lanes[0], top[1]);
	outOfWindow(lanes[3], bottom[0]);
	outOfWindow(lanes[2], bottom[1]);
}

/*
 * 0x866524: the macroblock's inter decision: the window at the coarse
 * search's vector, the search, the whole macroblock's choice and, unless
 * that decided, the partitions'
 */
void avcencDecideInter(AvcencDecideInfo *info)
{
	int16_t position[2], origin[2];
	AvcencInterBest best;

	avcencInterPosition((const uint8_t *)info->vector, info->interX, info->interY,
		info->width, info->height, position, origin);
	windowAtVector(info, position, origin);
	searchSads(info);
	avcencInterBest(info->sads, &best, info->best.deltas);
	if (!decideWhole(info, &best)) {
		choosePartition(info, &best);
		refinePartition(info);
	}
}
