/*
 * hwr_stroke.c: stroke segmentation, noise pruning, corner simplification,
 * direction quantization, and stroke-intersection detection.
 */
#include "hwr_internal.h"

HwrStrokeBuffer g_hwrStrokeBufStorage;

int hwrStrokeInit(void)
{
	g_hwrStrokeBuf = &g_hwrStrokeBufStorage;
	return 0;
}

int hwrStrokeTerm(void)
{
	return 0;
}

void hwrStrokeReset(void)
{
	g_hwrStrokeBuf->strokeCount = 0;
	g_hwrStrokeBuf->activeSegCount = 0;
	g_hwrStrokeBuf->totalSegCount = 0;
	g_hwrStrokeBuf->pointCount = 0;
	g_hwrStrokeBuf->totalLength = 0;
}

int hwrNormalizeSegLengths(void)
{
	uint16_t segIdx = 0;
	int maxLen = 0;

	do {
		HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
		uint16_t prev = seg->prevSeg;
		uint16_t next = seg->nextSeg;

		if (maxLen < (int)seg->length)
			maxLen = seg->length;
		if ((prev == HWR_INVALID_SEG || g_hwrStrokeBuf->segments[prev].penDown == 0) &&
		    (next == HWR_INVALID_SEG || g_hwrStrokeBuf->segments[next].penDown == 0) &&
		    seg->length < 3u) {
			seg->direction = 12;
			next = seg->nextSeg;
		}
		segIdx = next;
	} while (segIdx != HWR_INVALID_SEG);

	if (maxLen != 0) {
		uint16_t scale = (uint16_t)(3584 / maxLen);

		segIdx = 0;
		do {
			HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];

			seg->normLength = (uint8_t)((seg->length * scale) / 512);
			segIdx = seg->nextSeg;
		} while (segIdx != HWR_INVALID_SEG);
		return scale;
	}
	return 0;
}

int hwrCompactSegments(void)
{
	uint16_t dstIdx = 0;
	uint16_t nextIdx = g_hwrStrokeBuf->segments[0].nextSeg;
	uint16_t count = 1;
	uint16_t idx;

	while (nextIdx != HWR_INVALID_SEG) {
		if (nextIdx != count) {
			g_hwrStrokeBuf->segments[count] = g_hwrStrokeBuf->segments[nextIdx];
			g_hwrStrokeBuf->segments[count].prevSeg = dstIdx;
			g_hwrStrokeBuf->segments[dstIdx].nextSeg = count;
		}
		dstIdx++;
		count = (uint16_t)(dstIdx + 1);
		nextIdx = g_hwrStrokeBuf->segments[dstIdx].nextSeg;
	}

	g_hwrStrokeBuf->activeSegCount = count;
	for (idx = 0; idx < g_hwrStrokeBuf->activeSegCount; idx++) {
		uint16_t endPt = g_hwrStrokeBuf->segments[idx].endPt;
		uint16_t targetPt = (uint16_t)(idx + 1);

		if (endPt != targetPt) {
			g_hwrStrokeBuf->points[targetPt] = g_hwrStrokeBuf->points[endPt];
			g_hwrStrokeBuf->segments[idx + 1].startPt = targetPt;
			g_hwrStrokeBuf->segments[idx].endPt = targetPt;
		}
	}

	g_hwrStrokeBuf->totalSegCount = g_hwrStrokeBuf->activeSegCount;
	g_hwrStrokeBuf->pointCount = (uint16_t)(g_hwrStrokeBuf->activeSegCount + 1);
	return g_hwrStrokeBuf->pointCount;
}

int hwrRebuildStrokeMetrics(void)
{
	uint16_t segIdx = 0;
	uint16_t strokeIdx = 0;

	g_hwrStrokeBuf->totalLength = 0;
	g_hwrStrokeBuf->activeSegCount = 0;
	g_hwrStrokeBuf->strokeTotalLen[0] = 0;
	g_hwrStrokeBuf->strokeSegCount[0] = 0;
	g_hwrStrokeBuf->strokeMaxSegLen[0] = 0;
	g_hwrStrokeBuf->strokeFirstSeg[0] = 0;

	do {
		HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
		HwrPoint startPt = g_hwrStrokeBuf->points[seg->startPt];
		HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];

		seg->direction = (int8_t)hwrVectorDirectionAndLength(
			startPt.x, startPt.y, endPt.x, endPt.y, &seg->length);
		if (seg->penDown != 0) {
			g_hwrStrokeBuf->strokeSegCount[strokeIdx]++;
			g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = (uint16_t)(
				g_hwrStrokeBuf->strokeTotalLen[strokeIdx] + seg->length);
			if (g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] < seg->length)
				g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = seg->length;
		} else {
			strokeIdx++;
			g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = 0;
			g_hwrStrokeBuf->strokeSegCount[strokeIdx] = 0;
			g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = 0;
			g_hwrStrokeBuf->strokeFirstSeg[strokeIdx] =
				(uint16_t)(g_hwrStrokeBuf->activeSegCount + 1);
		}
		g_hwrStrokeBuf->totalLength = (uint16_t)(g_hwrStrokeBuf->totalLength + seg->length);
		g_hwrStrokeBuf->activeSegCount++;
		segIdx = seg->nextSeg;
	} while (segIdx != HWR_INVALID_SEG);

	return (int)HWR_INVALID_SEG;
}

int hwrSimplifyStrokeCorners(void)
{
	uint16_t changedFlag = 0;
	uint16_t lenThresh = (uint16_t)(g_hwrStrokeBuf->charScale >> 4);
	uint16_t targetSegs;

	do {
		uint16_t curIdx = 0;
		uint16_t nextIdx = g_hwrStrokeBuf->segments[0].nextSeg;
		int16_t mergeCount = 0;

		while (nextIdx != HWR_INVALID_SEG) {
			HwrSegment *cur = &g_hwrStrokeBuf->segments[curIdx];

			if (cur->penDown != 0 && curIdx != 0) {
				uint16_t prevIdx = cur->prevSeg;
				HwrSegment *next = &g_hwrStrokeBuf->segments[nextIdx];
				HwrSegment *prev = &g_hwrStrokeBuf->segments[prevIdx];

				if (next->penDown != 0 && prev->penDown != 0) {
					uint8_t nextDiff = (uint8_t)g_hwrDirAbsDiffPtr[
						next->direction - cur->direction + 16];
					uint8_t prevDiff = (uint8_t)g_hwrDirAbsDiffPtr[
						prev->direction - cur->direction + 16];

					if (cur->length < lenThresh && nextDiff < 4u && prevDiff < 4u) {
						HwrSegment *merged;
						uint16_t mergedIdx;

						if (prevDiff >= nextDiff &&
						    (prevDiff != nextDiff || prev->length < next->length)) {
							next->startPt = cur->startPt;
							next->prevSeg = cur->prevSeg;
							prev->nextSeg = nextIdx;
							merged = next;
							mergedIdx = nextIdx;
						} else {
							prev->endPt = cur->endPt;
							prev->nextSeg = cur->nextSeg;
							next->prevSeg = prevIdx;
							merged = prev;
							mergedIdx = prevIdx;
						}
						merged->direction = (int8_t)hwrVectorDirectionAndLength(
							g_hwrStrokeBuf->points[merged->startPt].x,
							g_hwrStrokeBuf->points[merged->startPt].y,
							g_hwrStrokeBuf->points[merged->endPt].x,
							g_hwrStrokeBuf->points[merged->endPt].y,
							&merged->length);
						curIdx = mergedIdx;
						nextIdx = merged->nextSeg;
						mergeCount++;
						changedFlag = 16;
						continue;
					}
					if (((uint32_t)cur->length + 4u < lenThresh ||
					     (uint32_t)next->length + 4u < lenThresh) &&
					    g_hwrDirAbsDiffPtr[next->direction - cur->direction + 16] < 3) {
						next->startPt = cur->startPt;
						next->prevSeg = cur->prevSeg;
						prev->nextSeg = nextIdx;
						next->direction = (int8_t)hwrVectorDirectionAndLength(
							g_hwrStrokeBuf->points[next->startPt].x,
							g_hwrStrokeBuf->points[next->startPt].y,
							g_hwrStrokeBuf->points[next->endPt].x,
							g_hwrStrokeBuf->points[next->endPt].y,
							&next->length);
						curIdx = nextIdx;
						nextIdx = next->nextSeg;
						mergeCount++;
						changedFlag = 16;
						continue;
					}
				}
			}
			curIdx = nextIdx;
			nextIdx = g_hwrStrokeBuf->segments[curIdx].nextSeg;
		}
		hwrRebuildStrokeMetrics();
		if (mergeCount == 0 && g_hwrStrokeBuf->activeSegCount < 64u)
			break;
		lenThresh++;
	} while (lenThresh <= g_hwrStrokeBuf->charScale);

	targetSegs = 2;
	while (g_hwrStrokeBuf->activeSegCount >= 64u) {
		uint16_t curSeg = 0;
		uint16_t strokeIdx = 0;
		int found = 0;

		for (;;) {
			uint16_t segCnt = g_hwrStrokeBuf->strokeSegCount[strokeIdx];

			if (segCnt == targetSegs) {
				found = 1;
				break;
			}
			{
				uint16_t step;

				for (step = 0; step < segCnt; step++)
					curSeg = g_hwrStrokeBuf->segments[curSeg].nextSeg;
			}
			if (strokeIdx >= g_hwrStrokeBuf->strokeCount)
				break;
			curSeg = g_hwrStrokeBuf->segments[curSeg].nextSeg;
			strokeIdx++;
		}
		if (found) {
			HwrSegment *cur = &g_hwrStrokeBuf->segments[curSeg];
			HwrSegment *next = &g_hwrStrokeBuf->segments[cur->nextSeg];

			cur->nextSeg = next->nextSeg;
			cur->endPt = next->endPt;
			if (next->nextSeg != HWR_INVALID_SEG)
				g_hwrStrokeBuf->segments[next->nextSeg].prevSeg = curSeg;
			hwrRebuildStrokeMetrics();
		} else {
			targetSegs++;
			if (targetSegs > 256u)
				targetSegs = 2;
		}
	}
	return changedFlag;
}

int hwrUpdateSingleStrokeMetrics(uint16_t startSeg)
{
	uint16_t strokeIdx = g_hwrStrokeBuf->strokeCount;
	uint16_t segIdx = startSeg;

	g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = 0;
	g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = 0;

	while (segIdx != HWR_INVALID_SEG) {
		HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
		HwrPoint startPt = g_hwrStrokeBuf->points[seg->startPt];
		HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];

		seg->direction = (int8_t)hwrVectorDirectionAndLength(
			startPt.x, startPt.y, endPt.x, endPt.y, &seg->length);
		strokeIdx = g_hwrStrokeBuf->strokeCount;
		g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = (uint16_t)(
			g_hwrStrokeBuf->strokeTotalLen[strokeIdx] + seg->length);
		if (g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] < seg->length)
			g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = seg->length;
		segIdx = seg->nextSeg;
	}
	return (int)segIdx;
}

int hwrTrimShortEndpoints(void)
{
	uint16_t strokeIdx = g_hwrStrokeBuf->strokeCount;
	uint16_t firstSeg = g_hwrStrokeBuf->strokeFirstSeg[strokeIdx];
	uint16_t segCnt = g_hwrStrokeBuf->strokeSegCount[strokeIdx];
	uint16_t lenThresh;
	int changed = 0;

	if (strokeIdx >= 6u)
		lenThresh = (uint16_t)((g_hwrStrokeBuf->charScale >> 5) + 1);
	else
		lenThresh = (uint16_t)((g_hwrStrokeBuf->charScale >> 4) + 2);

	if (((segCnt > 3u && g_hwrStrokeBuf->strokeTotalLen[strokeIdx] < 4u * lenThresh) ||
	     (segCnt > 4u && g_hwrStrokeBuf->strokeTotalLen[strokeIdx] < 6u * lenThresh)) &&
	    g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] < lenThresh) {
		return 0;
	}

	while (g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] >= 2u) {
		HwrSegment *head = &g_hwrStrokeBuf->segments[firstSeg];
		HwrSegment *second;

		if (head->length > lenThresh)
			break;
		second = &g_hwrStrokeBuf->segments[head->nextSeg];
		head->endPt = second->endPt;
		head->nextSeg = second->nextSeg;
		if (second->nextSeg != HWR_INVALID_SEG)
			g_hwrStrokeBuf->segments[second->nextSeg].prevSeg = firstSeg;
		g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount]--;
		head->direction = (int8_t)hwrVectorDirectionAndLength(
			g_hwrStrokeBuf->points[head->startPt].x,
			g_hwrStrokeBuf->points[head->startPt].y,
			g_hwrStrokeBuf->points[head->endPt].x,
			g_hwrStrokeBuf->points[head->endPt].y,
			&head->length);
		changed = 8;
	}

	while (g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] >= 2u) {
		uint16_t tailIdx = firstSeg;
		uint16_t prevIdx;
		HwrSegment *tail;
		HwrSegment *prev;

		while (g_hwrStrokeBuf->segments[tailIdx].nextSeg != HWR_INVALID_SEG)
			tailIdx = g_hwrStrokeBuf->segments[tailIdx].nextSeg;
		tail = &g_hwrStrokeBuf->segments[tailIdx];
		if (tail->length > lenThresh)
			break;
		prevIdx = tail->prevSeg;
		prev = &g_hwrStrokeBuf->segments[prevIdx];
		prev->endPt = tail->endPt;
		prev->nextSeg = HWR_INVALID_SEG;
		g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount]--;
		prev->direction = (int8_t)hwrVectorDirectionAndLength(
			g_hwrStrokeBuf->points[prev->startPt].x,
			g_hwrStrokeBuf->points[prev->startPt].y,
			g_hwrStrokeBuf->points[prev->endPt].x,
			g_hwrStrokeBuf->points[prev->endPt].y,
			&prev->length);
		changed = 8;
	}

	if (changed != 0)
		hwrUpdateSingleStrokeMetrics(firstSeg);
	return changed;
}

int hwrMergeTinySegments(void)
{
	uint16_t firstSeg = g_hwrStrokeBuf->strokeFirstSeg[g_hwrStrokeBuf->strokeCount];
	uint16_t lenThresh = (uint16_t)(g_hwrStrokeBuf->charScale >> 3);
	int changed = 0;

	for (;;) {
		uint16_t curIdx = firstSeg;
		uint16_t removed = 0;

		while (curIdx != HWR_INVALID_SEG) {
			HwrSegment *cur = &g_hwrStrokeBuf->segments[curIdx];
			uint16_t prevIdx = cur->prevSeg;
			int absorbedPrev = 0;

			if (curIdx != firstSeg && prevIdx != HWR_INVALID_SEG) {
				HwrSegment *prev = &g_hwrStrokeBuf->segments[prevIdx];

				if (prev->length < lenThresh &&
				    (cur->length > 16u * prev->length ||
				     (cur->length > 4u * prev->length &&
				      g_hwrDirAbsDiffPtr[cur->direction - prev->direction + 16] < 2))) {
					if (prevIdx == firstSeg) {
						prev->nextSeg = cur->nextSeg;
						prev->endPt = cur->endPt;
						prev->length = (uint16_t)(prev->length + cur->length);
						if (cur->nextSeg != HWR_INVALID_SEG)
							g_hwrStrokeBuf->segments[cur->nextSeg].prevSeg = prevIdx;
						curIdx = prevIdx;
					} else {
						cur->prevSeg = prev->prevSeg;
						cur->startPt = prev->startPt;
						cur->length = (uint16_t)(prev->length + cur->length);
						if (prev->prevSeg != HWR_INVALID_SEG)
							g_hwrStrokeBuf->segments[prev->prevSeg].nextSeg = curIdx;
					}
					removed++;
					changed = 4;
					absorbedPrev = 1;
				}
			}

			if (!absorbedPrev) {
				uint16_t nextIdx = cur->nextSeg;

				if (nextIdx != HWR_INVALID_SEG) {
					HwrSegment *next = &g_hwrStrokeBuf->segments[nextIdx];

					if (next->length < lenThresh &&
					    (cur->length > 16u * next->length ||
					     (cur->length > 4u * next->length &&
					      g_hwrDirAbsDiffPtr[cur->direction - next->direction + 16] < 2))) {
						uint16_t afterIdx = next->nextSeg;

						cur->nextSeg = afterIdx;
						if (afterIdx != HWR_INVALID_SEG)
							g_hwrStrokeBuf->segments[afterIdx].prevSeg = curIdx;
						cur->endPt = next->endPt;
						cur->length = (uint16_t)(cur->length + next->length);
						removed++;
						changed = 2;
						nextIdx = afterIdx;
					}
				}
				curIdx = nextIdx;
			}
		}

		if (removed == 0)
			break;
		g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] = (uint16_t)(
			g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] - removed);
		hwrUpdateSingleStrokeMetrics(firstSeg);
	}
	return changed;
}

int hwrCollapseZigzags(void)
{
	uint16_t firstSeg = g_hwrStrokeBuf->strokeFirstSeg[g_hwrStrokeBuf->strokeCount];
	uint16_t lenThresh = (uint16_t)(g_hwrStrokeBuf->charScale >> 2);
	uint16_t changed = 0;

	for (;;) {
		uint16_t curIdx = firstSeg;
		uint16_t nextIdx = g_hwrStrokeBuf->segments[firstSeg].nextSeg;
		uint16_t removed = 0;

		while (nextIdx != HWR_INVALID_SEG) {
			if (curIdx != firstSeg) {
				HwrSegment *cur = &g_hwrStrokeBuf->segments[curIdx];
				uint16_t prevIdx = cur->prevSeg;
				HwrSegment *prev = &g_hwrStrokeBuf->segments[prevIdx];
				HwrSegment *next = &g_hwrStrokeBuf->segments[nextIdx];

				if (prev->direction == next->direction) {
					int diff = g_hwrDirAbsDiffPtr[next->direction - cur->direction + 16];

					if (diff < 2 || diff * (int)cur->length < (int)lenThresh) {
						uint16_t afterIdx = next->nextSeg;

						prev->endPt = next->endPt;
						prev->nextSeg = afterIdx;
						if (afterIdx != HWR_INVALID_SEG)
							g_hwrStrokeBuf->segments[afterIdx].prevSeg = prevIdx;
						prev->direction = (int8_t)hwrVectorDirectionAndLength(
							g_hwrStrokeBuf->points[prev->startPt].x,
							g_hwrStrokeBuf->points[prev->startPt].y,
							g_hwrStrokeBuf->points[prev->endPt].x,
							g_hwrStrokeBuf->points[prev->endPt].y,
							&prev->length);
						curIdx = prevIdx;
						nextIdx = prev->nextSeg;
						removed = (uint16_t)(removed + 2);
						changed = 2;
						continue;
					}
				}
			}
			curIdx = nextIdx;
			nextIdx = g_hwrStrokeBuf->segments[curIdx].nextSeg;
		}
		if (removed == 0)
			break;
		g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] = (uint16_t)(
			g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] - removed);
		hwrUpdateSingleStrokeMetrics(firstSeg);
	}
	return changed;
}

int hwrMergeSameDirection(void)
{
	uint16_t firstSeg = g_hwrStrokeBuf->strokeFirstSeg[g_hwrStrokeBuf->strokeCount];
	int changed = 0;

	for (;;) {
		uint16_t anchorIdx = firstSeg;
		int8_t anchorDir = g_hwrStrokeBuf->segments[firstSeg].direction;
		uint16_t curIdx = g_hwrStrokeBuf->segments[firstSeg].nextSeg;
		uint16_t removed = 0;

		while (curIdx != HWR_INVALID_SEG) {
			HwrSegment *cur = &g_hwrStrokeBuf->segments[curIdx];

			if (cur->direction == anchorDir) {
				uint16_t nextIdx = cur->nextSeg;

				if (nextIdx == HWR_INVALID_SEG) {
					HwrSegment *anchor = &g_hwrStrokeBuf->segments[anchorIdx];

					anchor->nextSeg = HWR_INVALID_SEG;
					anchor->endPt = cur->endPt;
				}
				removed++;
				changed = 1;
				curIdx = nextIdx;
			} else {
				HwrSegment *anchor = &g_hwrStrokeBuf->segments[anchorIdx];

				anchor->nextSeg = curIdx;
				cur->prevSeg = anchorIdx;
				anchor->endPt = cur->startPt;
				anchorIdx = curIdx;
				anchorDir = cur->direction;
				curIdx = cur->nextSeg;
			}
		}
		if (removed == 0)
			break;
		g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] = (uint16_t)(
			g_hwrStrokeBuf->strokeSegCount[g_hwrStrokeBuf->strokeCount] - removed);
		hwrUpdateSingleStrokeMetrics(firstSeg);
	}
	return changed;
}

int hwrProcessStroke(HwrRawStroke *stroke)
{
	uint16_t strokeIdx = g_hwrStrokeBuf->strokeCount;
	uint16_t ptCount;
	uint16_t segLen;
	uint16_t ptIdx;
	uint16_t prevPtIdx;
	int16_t allowLoopCheck = 1;
	uint16_t prevLoopW = 100;
	uint16_t prevLoopH = 100;

	g_hwrStrokeBuf->strokeFirstPt[strokeIdx] = g_hwrStrokeBuf->pointCount;
	g_hwrStrokeBuf->strokeFirstSeg[strokeIdx] = g_hwrStrokeBuf->totalSegCount;
	g_hwrStrokeBuf->strokeSegCount[strokeIdx] = 0;
	g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = 0;
	g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = 0;

	if (strokeIdx >= HWR_MAX_STROKES)
		return -1;
	ptCount = stroke->pointCount;
	if (ptCount > 256u)
		return -1;
	if (ptCount == 1) {
		stroke->pointCount = 2;
		stroke->points[1].x = stroke->points[0].x;
		stroke->points[1].y = (uint8_t)(stroke->points[0].y + 1);
		ptCount = 2;
	}

	if (strokeIdx == 0) {
		g_hwrStrokeBuf->charMinX = 255u;
		g_hwrStrokeBuf->charMinY = 255u;
		g_hwrStrokeBuf->charMaxX = 0;
		g_hwrStrokeBuf->charMaxY = 0;
	} else {
		uint16_t lastPt = (uint16_t)(g_hwrStrokeBuf->pointCount - 1);
		uint16_t newSegIdx = g_hwrStrokeBuf->totalSegCount;
		HwrSegment *jumpSeg = &g_hwrStrokeBuf->segments[newSegIdx];
		int dir = hwrVectorDirectionAndLength(
			g_hwrStrokeBuf->points[lastPt].x,
			g_hwrStrokeBuf->points[lastPt].y,
			stroke->points[0].x,
			stroke->points[0].y,
			&segLen);

		if (dir == 12 && segLen == 0)
			dir = g_hwrStrokeBuf->segments[newSegIdx - 1].direction;
		jumpSeg->startPt = lastPt;
		jumpSeg->endPt = g_hwrStrokeBuf->pointCount;
		jumpSeg->prevSeg = (uint16_t)(newSegIdx - 1);
		jumpSeg->nextSeg = HWR_INVALID_SEG;
		jumpSeg->direction = (int8_t)dir;
		jumpSeg->length = (uint16_t)(segLen + 1);
		jumpSeg->penDown = 0;
		g_hwrStrokeBuf->segments[newSegIdx - 1].nextSeg = newSegIdx;
		g_hwrStrokeBuf->totalLength = (uint16_t)(g_hwrStrokeBuf->totalLength + segLen);
		g_hwrStrokeBuf->totalSegCount++;
		g_hwrStrokeBuf->strokeFirstSeg[strokeIdx]++;
		ptCount = stroke->pointCount;
	}

	{
		uint16_t dstPt = g_hwrStrokeBuf->pointCount;
		uint8_t firstX = stroke->points[0].x;
		uint8_t firstY = stroke->points[0].y;

		g_hwrStrokeBuf->points[dstPt].x = firstX;
		g_hwrStrokeBuf->points[dstPt].y = firstY;
		g_hwrStrokeBuf->strokeMinX[strokeIdx] = firstX;
		g_hwrStrokeBuf->strokeMaxX[strokeIdx] = firstX;
		g_hwrStrokeBuf->strokeMinY[strokeIdx] = firstY;
		g_hwrStrokeBuf->strokeMaxY[strokeIdx] = firstY;
		g_hwrStrokeBuf->pointCount++;
	}

	prevPtIdx = 0;
	for (ptIdx = 1; ptIdx < ptCount; ptIdx++) {
		uint8_t curX = stroke->points[ptIdx].x;
		uint8_t curY = stroke->points[ptIdx].y;
		int dir;
		int emit = 0;

		if (g_hwrStrokeBuf->strokeMinX[strokeIdx] > curX)
			g_hwrStrokeBuf->strokeMinX[strokeIdx] = curX;
		if (g_hwrStrokeBuf->strokeMaxX[strokeIdx] < curX)
			g_hwrStrokeBuf->strokeMaxX[strokeIdx] = curX;
		if (g_hwrStrokeBuf->strokeMinY[strokeIdx] > curY)
			g_hwrStrokeBuf->strokeMinY[strokeIdx] = curY;
		if (g_hwrStrokeBuf->strokeMaxY[strokeIdx] < curY)
			g_hwrStrokeBuf->strokeMaxY[strokeIdx] = curY;

		dir = hwrVectorDirectionAndLength(
			stroke->points[prevPtIdx].x, stroke->points[prevPtIdx].y,
			curX, curY, &segLen);
		ptCount = stroke->pointCount;
		if (ptIdx + 1 == ptCount) {
			if (dir == 12 && segLen == 0)
				stroke->points[ptIdx].y++;
			emit = 1;
		} else if (segLen >= 2u) {
			emit = 1;
		}

		if (emit) {
			uint16_t dstPt = g_hwrStrokeBuf->pointCount;
			uint16_t dstSeg = g_hwrStrokeBuf->totalSegCount;
			HwrSegment *seg = &g_hwrStrokeBuf->segments[dstSeg];

			g_hwrStrokeBuf->points[dstPt] = stroke->points[ptIdx];
			seg->startPt = (uint16_t)(dstPt - 1);
			seg->endPt = dstPt;
			seg->prevSeg = (uint16_t)(dstSeg - 1);
			seg->nextSeg = HWR_INVALID_SEG;
			seg->direction = (int8_t)dir;
			seg->length = (uint16_t)(segLen + 1);
			seg->penDown = 1;
			if (dstSeg != 0)
				g_hwrStrokeBuf->segments[dstSeg - 1].nextSeg = dstSeg;
			g_hwrStrokeBuf->pointCount++;
			g_hwrStrokeBuf->totalSegCount++;
			g_hwrStrokeBuf->strokeSegCount[strokeIdx]++;
			g_hwrStrokeBuf->strokeTotalLen[strokeIdx] = (uint16_t)(
				g_hwrStrokeBuf->strokeTotalLen[strokeIdx] + segLen);
			if (g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] < segLen)
				g_hwrStrokeBuf->strokeMaxSegLen[strokeIdx] = segLen;
			g_hwrStrokeBuf->totalLength = (uint16_t)(g_hwrStrokeBuf->totalLength + segLen);
			prevPtIdx = ptIdx;
		}
	}

	if (g_hwrStrokeBuf->charMinX > g_hwrStrokeBuf->strokeMinX[strokeIdx])
		g_hwrStrokeBuf->charMinX = g_hwrStrokeBuf->strokeMinX[strokeIdx];
	if (g_hwrStrokeBuf->charMaxX < g_hwrStrokeBuf->strokeMaxX[strokeIdx])
		g_hwrStrokeBuf->charMaxX = g_hwrStrokeBuf->strokeMaxX[strokeIdx];
	if (g_hwrStrokeBuf->charMinY > g_hwrStrokeBuf->strokeMinY[strokeIdx])
		g_hwrStrokeBuf->charMinY = g_hwrStrokeBuf->strokeMinY[strokeIdx];
	if (g_hwrStrokeBuf->charMaxY < g_hwrStrokeBuf->strokeMaxY[strokeIdx])
		g_hwrStrokeBuf->charMaxY = g_hwrStrokeBuf->strokeMaxY[strokeIdx];

	g_hwrStrokeBuf->charWidth = (uint16_t)(g_hwrStrokeBuf->charMaxX - g_hwrStrokeBuf->charMinX + 1);
	g_hwrStrokeBuf->charHeight = (uint16_t)(g_hwrStrokeBuf->charMaxY - g_hwrStrokeBuf->charMinY + 1);
	if (g_hwrStrokeBuf->charWidth <= g_hwrStrokeBuf->charHeight)
		g_hwrStrokeBuf->charScale = (uint16_t)(g_hwrStrokeBuf->charHeight +
						       (int)g_hwrStrokeBuf->charWidth / 2);
	else
		g_hwrStrokeBuf->charScale = (uint16_t)(g_hwrStrokeBuf->charWidth +
						       (int)g_hwrStrokeBuf->charHeight / 2);

	do {
		while ((uint16_t)(hwrMergeSameDirection() + hwrCollapseZigzags() +
				  hwrMergeTinySegments()) != 0)
			;

		if (allowLoopCheck != 0 && strokeIdx < 6u &&
		    g_hwrStrokeBuf->strokeSegCount[strokeIdx] >= 3u) {
			uint16_t firstSeg = g_hwrStrokeBuf->strokeFirstSeg[strokeIdx];
			uint16_t scanSeg = firstSeg;
			uint16_t lastInsideSeg = HWR_INVALID_SEG;
			HwrPoint origin = g_hwrStrokeBuf->points[
				g_hwrStrokeBuf->segments[firstSeg].startPt];
			int minX = origin.x;
			int maxX = origin.x;
			int minY = origin.y;
			int maxY = origin.y;
			uint16_t boxW = 0;
			uint16_t boxH = 0;
			uint16_t absTurn = 0;
			int16_t netTurn = 0;
			uint16_t insideCount = 0;
			uint16_t insideLen = 0;
			int16_t absNetTurn;
			int16_t turnDiff;

			while (scanSeg != HWR_INVALID_SEG) {
				HwrSegment *seg = &g_hwrStrokeBuf->segments[scanSeg];
				HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];

				prevLoopW = boxW;
				prevLoopH = boxH;
				if (minX > endPt.x)
					minX = endPt.x;
				if (maxX < endPt.x)
					maxX = endPt.x;
				boxW = (uint16_t)(maxX - minX);
				if (minY > endPt.y)
					minY = endPt.y;
				if (maxY < endPt.y)
					maxY = endPt.y;
				boxH = (uint16_t)(maxY - minY);

				if (lastInsideSeg != HWR_INVALID_SEG) {
					int delta = g_hwrStrokeBuf->segments[lastInsideSeg].direction -
						    seg->direction;

					absTurn = (uint16_t)(absTurn + g_hwrDirAbsDiffPtr[delta + 16]);
					netTurn = (int16_t)(netTurn + g_hwrDirSignedDiffPtr[delta + 16]);
				}
				if (boxW > 6u || boxH > 6u)
					break;
				lastInsideSeg = scanSeg;
				insideLen = (uint16_t)(insideLen + seg->length);
				scanSeg = seg->nextSeg;
				insideCount++;
			}

			if (boxW <= 6u || boxH <= 6u) {
				prevLoopW = boxW;
				prevLoopH = boxH;
			}
			absNetTurn = netTurn > 0 ? netTurn : (int16_t)(-netTurn);
			turnDiff = (int16_t)(absTurn - absNetTurn);
			if (turnDiff <= 0)
				turnDiff = (int16_t)(-turnDiff);

			if ((absTurn < 12u || absTurn > 24u || turnDiff > 8 ||
			     prevLoopW < 3u || prevLoopH < 3u) &&
			    2 * (int)(g_hwrStrokeBuf->strokeTotalLen[strokeIdx] - insideLen) <=
				(int)insideLen &&
			    insideCount >= 2u &&
			    (absTurn > 8u || prevLoopW <= 3u || prevLoopH <= 3u)) {
				g_hwrStrokeBuf->segments[firstSeg].endPt =
					g_hwrStrokeBuf->segments[lastInsideSeg].endPt;
				g_hwrStrokeBuf->segments[firstSeg].nextSeg = scanSeg;
				g_hwrStrokeBuf->strokeSegCount[strokeIdx] = (uint16_t)(
					g_hwrStrokeBuf->strokeSegCount[strokeIdx] - insideCount + 1);
				if (scanSeg != HWR_INVALID_SEG)
					g_hwrStrokeBuf->segments[scanSeg].prevSeg = firstSeg;
				allowLoopCheck = 0;
			}
		}
	} while (hwrTrimShortEndpoints() != 0);

	hwrSimplifyStrokeCorners();
	hwrCompactSegments();
	hwrNormalizeSegLengths();
	g_hwrStrokeBuf->strokeCount++;
	g_hwrStrokeBuf->activeSegCount = g_hwrStrokeBuf->totalSegCount;
	return (int16_t)g_hwrStrokeBuf->strokeCount;
}

int hwrStrokesIntersect(uint16_t firstStroke, uint16_t secondStroke)
{
	uint16_t firstStart = g_hwrStrokeBuf->strokeFirstSeg[firstStroke];
	uint16_t firstEnd = (uint16_t)(firstStart + g_hwrStrokeBuf->strokeSegCount[firstStroke]);
	uint16_t secondStart = g_hwrStrokeBuf->strokeFirstSeg[secondStroke];
	uint16_t secondEnd = (uint16_t)(secondStart + g_hwrStrokeBuf->strokeSegCount[secondStroke]);
	uint16_t idxA;
	uint16_t idxB;

	for (idxA = firstStart; idxA < firstEnd; idxA++) {
		const HwrSegment *segA = &g_hwrStrokeBuf->segments[idxA];
		HwrPoint ptAStart;
		HwrPoint ptAEnd;
		int dxA;
		int dyA;

		if (segA->length < 8u || g_hwrStrokeBuf->strokeSegCount[secondStroke] == 0)
			continue;
		ptAStart = g_hwrStrokeBuf->points[segA->startPt];
		ptAEnd = g_hwrStrokeBuf->points[segA->endPt];
		dxA = (int)ptAStart.x - (int)ptAEnd.x;
		dyA = (int)ptAStart.y - (int)ptAEnd.y;

		for (idxB = secondStart; idxB < secondEnd; idxB++) {
			const HwrSegment *segB = &g_hwrStrokeBuf->segments[idxB];
			HwrPoint ptBStart;
			HwrPoint ptBEnd;
			int dxB;
			int dyB;
			int denom;

			if (segB->length < 8u)
				continue;
			ptBStart = g_hwrStrokeBuf->points[segB->startPt];
			ptBEnd = g_hwrStrokeBuf->points[segB->endPt];
			dxB = (int)ptBStart.x - (int)ptBEnd.x;
			dyB = (int)ptBStart.y - (int)ptBEnd.y;
			denom = dxB * dyA - dxA * dyB;
			if (denom != 0) {
				int diffX = (int)ptAStart.x - (int)ptBStart.x;
				int diffY = (int)ptAStart.y - (int)ptBStart.y;
				int numA = 10 * (diffY * dxB - diffX * dyB);
				int numB = 10 * (dxA * diffY - diffX * dyA);

				if (denom < 0) {
					denom = -denom;
					numA = -numA;
					numB = -numB;
				}
				if (numA >= 4 * denom && numB >= 4 * denom &&
				    numA <= 6 * denom && numB <= 6 * denom)
					return 1;
			}
		}
	}
	return 0;
}

int hwrVectorDirectionAndLength(int16_t xStart, int16_t yStart,
				int16_t xEnd, int16_t yEnd, uint16_t *outLen)
{
	uint16_t localLen;
	int dx = (int16_t)(xEnd - xStart);
	int dy = (int16_t)(yEnd - yStart);

	if (outLen == NULL)
		outLen = &localLen;
	if ((dx | dy) == 0) {
		*outLen = 0;
		return 12;
	}
	if (dx < 0) {
		int adx = -dx;

		if (dy < 0) {
			int ady = -dy;

			if (adx < ady) {
				*outLen = (uint16_t)((adx >> 1) + ady);
				return (2 * adx >= ady) ? 5 : 4;
			}
			*outLen = (uint16_t)((ady >> 1) + adx);
			return (adx > 2 * ady) ? 7 : 6;
		}
		if (adx <= dy) {
			*outLen = (uint16_t)(dy + (adx >> 1));
			return (2 * adx >= dy) ? 10 : 11;
		}
		*outLen = (uint16_t)((dy >> 1) + adx);
		return (adx > 2 * dy) ? 8 : 9;
	}
	if (dy <= 0) {
		int ady = -dy;

		if (dx <= ady) {
			*outLen = (uint16_t)((dx >> 1) + ady);
			if (2 * dx >= ady)
				return 2;
			return (dx > 0) ? 3 : 4;
		}
		*outLen = (uint16_t)(dx + (ady >> 1));
		return (dx <= 2 * ady) ? 1 : 0;
	}
	if (dx < dy) {
		*outLen = (uint16_t)(dy + (dx >> 1));
		return (2 * dx >= dy) ? 13 : 12;
	}
	*outLen = (uint16_t)(dx + (dy >> 1));
	return (dx > 2 * dy) ? 15 : 14;
}

int hwrVectorDirection(int16_t xStart, int16_t yStart, int16_t xEnd, int16_t yEnd)
{
	int dx = (int16_t)(xEnd - xStart);
	int dy = (int16_t)(yEnd - yStart);

	if (dx == 0 && dy == 0)
		return 16;
	if (dx < 0) {
		int adx = -dx;

		if (dy < 0) {
			int ady = -dy;

			if (adx < ady)
				return (2 * adx >= ady) ? 5 : 4;
			return (adx > 2 * ady) ? 7 : 6;
		}
		if (adx <= dy)
			return (2 * adx >= dy) ? 10 : 11;
		return (adx > 2 * dy) ? 8 : 9;
	}
	if (dy <= 0) {
		int ady = -dy;

		if (dx <= ady) {
			if (2 * dx >= ady)
				return 2;
			return (dx > 0) ? 3 : 4;
		}
		return (dx <= 2 * ady) ? 1 : 0;
	}
	if (dx < dy)
		return (2 * dx >= dy) ? 13 : 12;
	return (dx > 2 * dy) ? 15 : 14;
}
