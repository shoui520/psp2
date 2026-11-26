/*
 * hwr_match.c: geometric rule VM, radical tree composition, user-registration
 * matcher, candidate ranking, and character category filtering.
 */
#include "hwr_internal.h"

const int16_t g_hwrGridPosCostTable[64] = {
	 0,  2,  2,  4,  6, 10, 16, 26,
	 2,  0,  2,  2,  4,  6, 10, 16,
	 2,  2,  0,  2,  2,  4,  6, 10,
	 4,  2,  2,  0,  2,  2,  4,  6,
	 6,  4,  2,  2,  0,  2,  2,  4,
	10,  6,  4,  2,  2,  0,  2,  2,
	16, 10,  6,  4,  2,  2,  0,  2,
	26, 16, 10,  6,  4,  2,  2,  0,
};

HwrCandidateList g_hwrFinalList;
uint16_t g_hwrInterStrokeDirMask[36];
HwrDictCharEntry *g_hwrCoarseCharPtrs[HWR_MAX_COARSE_CANDIDATES];
uint8_t g_hwrCoarseRankIndex[HWR_MAX_COARSE_CANDIDATES];
uint16_t g_hwrCoarseCharCodes[HWR_MAX_COARSE_CANDIDATES];
uint16_t g_hwrCoarseCharCount;
uint16_t g_hwrCoarseMaxCandidates;
HwrSharedWorkspace g_hwrWork;
HwrRuleCheckItem g_hwrRuleCheckQueue[HWR_MAX_CANDIDATES];
HwrRuleCheckItem *g_hwrRuleCheckQueuePtr;
uint16_t g_hwrRuleCheckCount;
HwrUserPatternHeader g_hwrUserRefHeader;
HwrUserPatternHeader g_hwrUserInputHeader;
uint32_t g_hwrUserRefSegments[HWR_MAX_PACKED_SEGS];
uint32_t g_hwrUserInputSegments[HWR_MAX_PACKED_SEGS];
HwrUserResult g_hwrUserResults[HWR_MAX_CANDIDATES];
int32_t g_hwrUserResultCount;

int hwrComputeVectorAngle(const HwrPoint *firstStart, const HwrPoint *firstEnd,
			  const HwrPoint *secondStart, const HwrPoint *secondEnd)
{
	int dyFirst = (int)firstEnd->y - (int)firstStart->y;
	int dxFirst = (int)firstEnd->x - (int)firstStart->x;
	int dxSecond = (int)secondEnd->x - (int)secondStart->x;
	int dySecond = (int)secondEnd->y - (int)secondStart->y;
	int dot = dyFirst * dySecond + dxFirst * dxSecond;
	int isObtuse = dot < 0;
	int denom = (dySecond * dySecond + dxSecond * dxSecond) *
		    (dyFirst * dyFirst + dxFirst * dxFirst);
	int numer = dot * dot;
	uint32_t ratio;
	int16_t bucket;
	uint16_t angle;

	if (denom == 0)
		return 0;
	if (numer < 0x100000)
		numer <<= 10;
	else
		denom >>= 10;
	ratio = (uint32_t)(numer / denom);

	for (bucket = 0; bucket < 7; bucket++) {
		if (g_hwrCosSqThresholdPtr[bucket] > ratio)
			break;
	}
	angle = (uint16_t)(7 - bucket);
	if (isObtuse)
		angle = (uint16_t)(15 - angle);
	if (dxFirst * dySecond > dyFirst * dxSecond)
		return (uint16_t)(31 - angle);
	return angle;
}

int hwrGetStrokeRangeBBox(uint16_t startStroke, uint16_t strokeCount,
			  int16_t *minX, int16_t *minY, int16_t *maxX, int16_t *maxY)
{
	uint32_t cacheIdx = (uint32_t)g_hwrRevTriangularPtr[startStroke] + strokeCount;
	HwrBBoxEntry *entry = &g_hwrWork.bboxCache[cacheIdx];

	if (entry->maxY != 0) {
		*minX = (int16_t)entry->minX;
		*minY = (int16_t)entry->minY;
		*maxX = (int16_t)entry->maxX;
		*maxY = (int16_t)entry->maxY;
		return 1;
	}

	{
		uint16_t endStroke = (uint16_t)(startStroke + strokeCount);
		uint16_t idx;

		*minX = g_hwrStrokeBuf->strokeMinX[startStroke];
		*minY = g_hwrStrokeBuf->strokeMinY[startStroke];
		*maxX = g_hwrStrokeBuf->strokeMaxX[startStroke];
		*maxY = g_hwrStrokeBuf->strokeMaxY[startStroke];
		for (idx = (uint16_t)(startStroke + 1); idx < endStroke; idx++) {
			if (g_hwrStrokeBuf->strokeMinX[idx] < (uint16_t)*minX)
				*minX = g_hwrStrokeBuf->strokeMinX[idx];
			if (g_hwrStrokeBuf->strokeMinY[idx] < (uint16_t)*minY)
				*minY = g_hwrStrokeBuf->strokeMinY[idx];
			if (g_hwrStrokeBuf->strokeMaxX[idx] > (uint16_t)*maxX)
				*maxX = g_hwrStrokeBuf->strokeMaxX[idx];
			if (g_hwrStrokeBuf->strokeMaxY[idx] > (uint16_t)*maxY)
				*maxY = g_hwrStrokeBuf->strokeMaxY[idx];
		}
		entry->minX = (uint16_t)*minX;
		entry->minY = (uint16_t)*minY;
		entry->maxX = (uint16_t)*maxX;
		entry->maxY = (uint16_t)*maxY;
	}
	return 1;
}

static __attribute__((always_inline)) inline int hwrEvalPredicate(uint8_t op, HwrSegment *segA, HwrSegment *segB,
			    const HwrPoint *ptA, const HwrPoint *ptB,
			    uint8_t refA, uint8_t refB,
			    uint8_t loBound, uint8_t hiBound,
			    uint16_t startStroke, uint16_t strokeCount,
			    uint16_t segOffset, int16_t *bboxValid,
			    int16_t *bxMin, int16_t *byMin,
			    int16_t *bxMax, int16_t *byMax)
{
	uint8_t lowOp = op & 0x1Fu;

	if (lowOp <= 7u) {
		switch (op) {
		case 128:
			return (ptA != NULL && ptB != NULL && ptA->x < ptB->x) ? 1 : 0;
		case 129:
			return (ptA != NULL && ptB != NULL && ptA->x > ptB->x) ? 1 : 0;
		case 130:
			return (ptA != NULL && ptB != NULL && ptA->y < ptB->y) ? 1 : 0;
		case 131:
			return (ptA != NULL && ptB != NULL && ptA->y > ptB->y) ? 1 : 0;
		case 132:
			if (segA != NULL && segB != NULL) {
				HwrSegment *cur = segA;

				if (cur->penDown == 0)
					return 0;
				while (cur != segB) {
					cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
					if (cur->penDown == 0)
						return 0;
				}
				return 1;
			}
			return 0;
		case 133:
			if (segA != NULL && segB != NULL) {
				HwrSegment *cur = segA;

				if (cur->penDown == 0)
					return 1;
				while (cur != segB) {
					cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
					if (cur->penDown == 0)
						return 1;
				}
			}
			return 0;
		default:
			return 0;
		}
	}

	if (lowOp <= 13u) {
		if (op > 135u) {
			if (op == 136u) {
				if (ptA != NULL && ptB != NULL && segA != NULL && segB != NULL) {
					int dist = 16 * (hwrOctagonalDistance(
						ptA->x, ptA->y, ptB->x, ptB->y) & 0xFFF);
					uint16_t pathLen = (uint16_t)(segA->length + 1);
					HwrSegment *cur = segA;

					while (cur != segB) {
						cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
						pathLen = (uint16_t)(pathLen + cur->length);
					}
					return (dist >= (int)pathLen * loBound &&
						dist <= (int)pathLen * hiBound) ? 1 : 0;
				}
				return 0;
			}
			if (op == 137u) {
				if (ptA != NULL && ptB != NULL) {
					int diffY;
					int boxH;
					int scaled;

					if (*bboxValid == 0)
						*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
							startStroke, strokeCount,
							bxMin, byMin, bxMax, byMax);
					diffY = (int)ptA->y - (int)ptB->y;
					if (diffY < 0)
						diffY = -diffY;
					boxH = *byMax - *byMin + 1;
					scaled = 16 * (diffY & 0xFFF);
					return (scaled >= boxH * loBound &&
						scaled <= boxH * hiBound) ? 1 : 0;
				}
				return 0;
			}
			if (op == 138u) {
				if (ptA != NULL && ptB != NULL) {
					int diffX;
					int boxW;
					int scaled;

					if (*bboxValid == 0)
						*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
							startStroke, strokeCount,
							bxMin, byMin, bxMax, byMax);
					diffX = (int)ptA->x - (int)ptB->x;
					if (diffX < 0)
						diffX = -diffX;
					boxW = *bxMax - *bxMin + 1;
					scaled = 16 * (diffX & 0xFFF);
					return (scaled >= boxW * loBound &&
						scaled <= boxW * hiBound) ? 1 : 0;
				}
				return 0;
			}
			return 0;
		}
		if (op >= 72u && op <= 76u) {
			if (*bboxValid == 0)
				*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
					startStroke, strokeCount,
					bxMin, byMin, bxMax, byMax);
			switch (op) {
			case 72: {
				int val = 16 * (*bxMin & 0xFFF);
				return (val >= (int)g_hwrCfg.frameWidth * loBound &&
					val <= (int)g_hwrCfg.frameWidth * hiBound) ? 1 : 0;
			}
			case 73: {
				int val = (*byMin >= 3) ? (uint16_t)(16 * *byMin - 48)
							: 16 * (*byMin & 0xFFF);
				return (val >= (int)g_hwrCfg.frameHeight * loBound &&
					val <= (int)g_hwrCfg.frameHeight * hiBound) ? 1 : 0;
			}
			case 74: {
				uint32_t area = (uint32_t)(16 * ((*bxMax - *bxMin + 1) *
								 (*byMax - *byMin + 1)));
				return (area >= g_hwrCfg.frameArea * loBound &&
					area <= g_hwrCfg.frameArea * hiBound) ? 1 : 0;
			}
			case 75: {
				int val = (uint16_t)(16 * (*bxMax - *bxMin) + 16);
				return (val >= (int)g_hwrCfg.frameWidth * loBound &&
					val <= (int)g_hwrCfg.frameWidth * hiBound) ? 1 : 0;
			}
			case 76: {
				int val = (uint16_t)(16 * (*byMax - *byMin) + 16);
				return (val >= (int)g_hwrCfg.frameHeight * loBound &&
					val <= (int)g_hwrCfg.frameHeight * hiBound) ? 1 : 0;
			}
			default:
				return 0;
			}
		}
		return 0;
	}

	if (lowOp <= 19u) {
		switch (op) {
		case 238: {
			HwrSegment *curB = &g_hwrStrokeBuf->segments[
				g_hwrDpAlign.startSeg[refB] + segOffset];
			HwrSegment *endB = &g_hwrStrokeBuf->segments[
				g_hwrDpAlign.endSeg[refB] + segOffset];
			HwrSegment *curA = &g_hwrStrokeBuf->segments[
				g_hwrDpAlign.startSeg[refA] + segOffset];
			HwrSegment *endA = &g_hwrStrokeBuf->segments[
				g_hwrDpAlign.endSeg[refA] + segOffset];
			int16_t lenB = (int16_t)(curB->length + 1);
			uint16_t lenA = (uint16_t)(curA->length + 1);
			int scaledB;

			while (curB != endB) {
				curB = &g_hwrStrokeBuf->segments[curB->nextSeg];
				lenB = (int16_t)(lenB + curB->length);
			}
			while (curA != endA) {
				curA = &g_hwrStrokeBuf->segments[curA->nextSeg];
				lenA = (uint16_t)(lenA + curA->length);
			}
			scaledB = 16 * (lenB & 0xFFF);
			return (scaledB >= (int)lenA * loBound &&
				scaledB <= (int)lenA * hiBound) ? 1 : 0;
		}
		case 143:
			if (ptA != NULL && ptB != NULL) {
				int dist;
				int boxH;

				if (*bboxValid == 0)
					*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
						startStroke, strokeCount,
						bxMin, byMin, bxMax, byMax);
				dist = 16 * (hwrOctagonalDistance(
					ptA->x, ptA->y, ptB->x, ptB->y) & 0xFFF);
				boxH = *byMax - *byMin + 1;
				return (dist >= boxH * loBound && dist <= boxH * hiBound) ? 1 : 0;
			}
			return 0;
		case 142:
			if (ptA != NULL && ptB != NULL) {
				int dist;
				int boxW;

				if (*bboxValid == 0)
					*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
						startStroke, strokeCount,
						bxMin, byMin, bxMax, byMax);
				dist = 16 * (hwrOctagonalDistance(
					ptA->x, ptA->y, ptB->x, ptB->y) & 0xFFF);
				boxW = *bxMax - *bxMin + 1;
				return (dist >= boxW * loBound && dist <= boxW * hiBound) ? 1 : 0;
			}
			return 0;
		case 111:
			if (segA != NULL) {
				int boxH;
				int scaled;

				if (*bboxValid == 0)
					*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
						startStroke, strokeCount,
						bxMin, byMin, bxMax, byMax);
				boxH = *byMax - *byMin + 1;
				scaled = (uint16_t)(16 * segA->length + 16);
				return (scaled >= boxH * loBound && scaled <= boxH * hiBound) ? 1 : 0;
			}
			return 0;
		case 110:
			if (segA != NULL) {
				int boxW;
				int scaled;

				if (*bboxValid == 0)
					*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
						startStroke, strokeCount,
						bxMin, byMin, bxMax, byMax);
				boxW = *bxMax - *bxMin + 1;
				scaled = (uint16_t)(16 * segA->length + 16);
				return (scaled >= boxW * loBound && scaled <= boxW * hiBound) ? 1 : 0;
			}
			return 0;
		case 78: {
			int boxW;
			int scaledH;

			if (*bboxValid == 0)
				*bboxValid = (int16_t)hwrGetStrokeRangeBBox(
					startStroke, strokeCount,
					bxMin, byMin, bxMax, byMax);
			boxW = *bxMax - *bxMin + 1;
			scaledH = 16 * ((*byMax - *byMin + 1) & 0xFFF);
			return (scaledH >= boxW * loBound && scaledH <= boxW * hiBound) ? 1 : 0;
		}
		default:
			return 0;
		}
	}

	if (lowOp <= 22u) {
		if (op == 116u) {
			if (segA != NULL) {
				uint8_t dir = (uint8_t)segA->direction;

				if (loBound > hiBound)
					return (dir <= hiBound || dir >= loBound) ? 1 : 0;
				return (dir >= loBound && dir <= hiBound) ? 1 : 0;
			}
			return 0;
		}
		if (op == 148u) {
			if (ptA != NULL && ptB != NULL) {
				uint16_t dir = (uint16_t)(int8_t)hwrVectorDirection(
					ptA->x, ptA->y, ptB->x, ptB->y);

				if (dir == 16u)
					return 1;
				if (loBound > hiBound)
					return (dir <= hiBound || dir >= loBound) ? 1 : 0;
				return (dir >= loBound && dir <= hiBound) ? 1 : 0;
			}
			return 0;
		}
		if (op == 149u) {
			if (segA != NULL && segB != NULL) {
				HwrSegment *cur = segA;

				for (;;) {
					uint8_t dir = (uint8_t)cur->direction;
					int ok = (loBound > hiBound)
						? (dir <= hiBound || dir >= loBound)
						: (dir >= loBound && dir <= hiBound);

					if (!ok)
						return 0;
					if (cur == segB)
						return 1;
					cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
				}
			}
			return 0;
		}
		return 0;
	}

	if (lowOp <= 25u) {
		if (op == 151u && segA != NULL && segB != NULL) {
			HwrSegment *cur = segA;
			uint16_t jumps = (cur->penDown == 0) ? 1u : 0u;

			while (cur != segB) {
				cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
				if (cur->penDown == 0)
					jumps++;
			}
			return (jumps >= loBound && jumps <= hiBound) ? 1 : 0;
		}
		if (op == 152u && segA != NULL && segB != NULL) {
			HwrSegment *cur = segA;
			uint16_t segCount = 1;

			while (cur != segB) {
				cur = &g_hwrStrokeBuf->segments[cur->nextSeg];
				segCount++;
			}
			return (segCount >= loBound && segCount <= hiBound) ? 1 : 0;
		}
		return 0;
	}

	if (lowOp <= 27u) {
		if (segA != NULL && segB != NULL) {
			HwrSegment *cur = segA;
			int minTurn = 0;
			int maxTurn = 0;

			while (cur != segB) {
				HwrSegment *next = &g_hwrStrokeBuf->segments[cur->nextSeg];
				int turn = g_hwrDirSignedDiffPtr[cur->direction - next->direction + 16];

				if (turn < 0) {
					if (turn < minTurn)
						minTurn = turn;
				} else if (turn > maxTurn) {
					maxTurn = turn;
				}
				cur = next;
			}
			return ((int8_t)loBound <= (int8_t)minTurn &&
				(int8_t)maxTurn <= (int8_t)hiBound) ? 1 : 0;
		}
		return 0;
	}

	if (lowOp <= 29u) {
		if (segA != NULL && segB != NULL) {
			HwrSegment *cur = segA;
			int8_t sumTurn = 0;

			while (cur != segB) {
				HwrSegment *next = &g_hwrStrokeBuf->segments[cur->nextSeg];
				int8_t turn = g_hwrDirSignedDiffPtr[cur->direction - next->direction + 16];

				if (turn == 8)
					return 1;
				sumTurn = (int8_t)(sumTurn + turn);
				cur = next;
			}
			return ((int8_t)loBound <= sumTurn && sumTurn <= (int8_t)hiBound) ? 1 : 0;
		}
		return 0;
	}

	if (op == 190u && segA != NULL) {
		int ang = hwrComputeVectorAngle(&g_hwrStrokeBuf->points[segA->startPt],
						&g_hwrStrokeBuf->points[segA->endPt],
						&g_hwrStrokeBuf->points[segA->startPt],
						ptB);

		if (loBound > hiBound)
			return (ang <= hiBound || ang >= loBound) ? 1 : 0;
		return (ang >= loBound && ang <= hiBound) ? 1 : 0;
	}
	if (op == 254u && segA != NULL && segB != NULL) {
		int ang = hwrComputeVectorAngle(&g_hwrStrokeBuf->points[segA->startPt],
						&g_hwrStrokeBuf->points[segA->endPt],
						&g_hwrStrokeBuf->points[segB->startPt],
						&g_hwrStrokeBuf->points[segB->endPt]);

		if (loBound > hiBound)
			return (ang <= hiBound || ang >= loBound) ? 1 : 0;
		return (ang >= loBound && ang <= hiBound) ? 1 : 0;
	}
	return 0;
}

int hwrEvaluateRules(const uint8_t *ruleBuf, uint16_t startStroke,
		     uint16_t strokeCount, uint16_t segOffset, int16_t passArg)
{
	uint8_t opStack[18];
	uint16_t sp = 0;
	int16_t bxMin = 0, byMin = 0, bxMax = 0, byMax = 0;
	int16_t bboxValid = 0;
	HwrSegment *segA = NULL;
	HwrSegment *segB = NULL;
	const HwrPoint *ptA = NULL;
	const HwrPoint *ptB = NULL;
	uint8_t refA = 0;
	uint8_t refB = 0;
	uint8_t loBound = 0;
	uint8_t hiBound = 0;
	uint16_t penalty = 0;
	uint16_t targetCode = 0;
	int16_t curWeight = 63;
	int16_t useRulePenalty = 0;
	int16_t rem = (int16_t)(ruleBuf[2] - 4);
	const uint8_t *pc = ruleBuf + 4;

	g_hwrRuleEvalCallCount++;

	while (rem != 0) {
		uint8_t op;

		if (passArg > 0) {
			passArg--;
			if (passArg == 0)
				return penalty;
		}
		op = *pc++;
		rem--;

		if (op == 206u) {
			if (passArg >= 0)
				return penalty;
			if (targetCode != 0 && penalty < 63u)
				hwrInsertRuleCandidate((uint16_t)(-1 - passArg), targetCode, penalty);
			targetCode = (uint16_t)(((uint16_t)pc[0] << 8) | pc[1]);
			pc += 2;
			rem -= 2;
			penalty = 0;
			continue;
		}

		if ((op & 0xC0u) == 0) {
			curWeight = op;
			continue;
		}
		if (op == 201u && curWeight == 63) {
			useRulePenalty = 1;
			continue;
		}

		if ((op & 0xE0u) == 0xC0u) {
			switch (op) {
			case 192:
			case 195:
			case 196:
			case 200:
			case 201:
				sp++;
				opStack[sp] = op;
				break;
			case 193:
			case 194:
				sp += 2;
				opStack[sp] = op;
				opStack[sp - 1] = 255u;
				break;
			default:
				break;
			}
			continue;
		}

		if ((op & 0x20u) != 0) {
			uint8_t idx = *pc++;

			rem--;
			refA = idx;
			segA = &g_hwrStrokeBuf->segments[g_hwrDpAlign.mainSeg[idx] + segOffset];
		} else if ((op & 0xE0u) == 0x80u) {
			uint8_t idx = *pc++;

			rem--;
			if ((idx & 0x80u) != 0) {
				refA = idx & 0x7Fu;
				segA = &g_hwrStrokeBuf->segments[g_hwrDpAlign.endSeg[refA] + segOffset];
				ptA = &g_hwrStrokeBuf->points[segA->endPt];
			} else {
				refA = idx;
				segA = &g_hwrStrokeBuf->segments[g_hwrDpAlign.startSeg[refA] + segOffset];
				ptA = &g_hwrStrokeBuf->points[segA->startPt];
			}
		}

		if ((op & 0x40u) != 0) {
			if ((op & 0xE0u) == 0xE0u) {
				uint8_t idx = *pc++;

				rem--;
				refB = idx;
				segB = &g_hwrStrokeBuf->segments[g_hwrDpAlign.mainSeg[idx] + segOffset];
			}
		} else {
			uint8_t idx = *pc++;

			rem--;
			if ((idx & 0x80u) != 0) {
				refB = idx & 0x7Fu;
				segB = &g_hwrStrokeBuf->segments[g_hwrDpAlign.endSeg[refB] + segOffset];
				ptB = &g_hwrStrokeBuf->points[segB->endPt];
			} else {
				refB = idx;
				segB = &g_hwrStrokeBuf->segments[g_hwrDpAlign.startSeg[refB] + segOffset];
				ptB = &g_hwrStrokeBuf->points[segB->startPt];
			}
		}

		if ((op & 0x1Fu) >= 8u) {
			loBound = pc[0];
			hiBound = pc[1];
			pc += 2;
			rem -= 2;
		}

		{
			int cond;
			int skipEval = 0;

			if (sp != 0) {
				uint8_t top = opStack[sp];

				if (top == 195u) {
					cond = 1;
					sp--;
					skipEval = 1;
				} else if (top == 196u) {
					cond = 0;
					sp--;
					skipEval = 1;
				} else if (top == 201u && g_hwrFineMatchPass == 0) {
					cond = 1;
					sp--;
					skipEval = 1;
				} else if (top == 200u && g_hwrFineMatchPass == 1) {
					cond = 1;
					sp--;
					skipEval = 1;
				}
			}

			if (!skipEval) {
				cond = hwrEvalPredicate(op, segA, segB, ptA, ptB, refA, refB,
							loBound, hiBound, startStroke, strokeCount,
							segOffset, &bboxValid,
							&bxMin, &byMin, &bxMax, &byMax);
			}

			{
				int waitingOperand = 0;

				while (sp != 0) {
					uint8_t top = opStack[sp];

					if (top == 0xC0u) {
						cond = (cond != 0) ? 0 : 1;
						sp--;
					} else if (top == 0xC1u) {
						uint8_t leftVal = opStack[sp - 1];

						if (leftVal == 255u && cond == 0) {
							sp--;
							opStack[sp] = 0xC4u;
							waitingOperand = 1;
							break;
						}
						if (leftVal == 255u) {
							opStack[sp - 1] = (uint8_t)cond;
							waitingOperand = 1;
							break;
						}
						sp -= 2;
						cond &= leftVal;
					} else if (top == 0xC2u) {
						uint8_t leftVal = opStack[sp - 1];

						if (leftVal == 255u && cond == 1) {
							sp--;
							opStack[sp] = 0xC3u;
							waitingOperand = 1;
							break;
						}
						if (leftVal == 255u) {
							opStack[sp - 1] = (uint8_t)cond;
							waitingOperand = 1;
							break;
						}
						sp -= 2;
						cond |= leftVal;
					} else if (top == 0xC3u) {
						cond = 1;
						sp--;
					} else if (top == 0xC4u) {
						cond = 0;
						sp--;
					} else if (top == 0xC8u) {
						cond = (cond != 0 || g_hwrFineMatchPass == 1) ? 1 : 0;
						sp--;
					} else if (top == 0xC9u) {
						cond = (cond != 0 || g_hwrFineMatchPass == 0) ? 1 : 0;
						sp--;
					} else {
						waitingOperand = 1;
						break;
					}
				}

				if (!waitingOperand && sp == 0) {
					if (cond == 0) {
						int16_t addWeight = useRulePenalty ? (int16_t)g_hwrRulePenalty : curWeight;

						penalty = (uint16_t)(penalty + addWeight);
						if (passArg >= 0 && penalty >= 63u)
							return 63;
					}
					useRulePenalty = 0;
				}
			}
		}
	}

	if (targetCode != 0) {
		if (penalty < 63u)
			hwrInsertRuleCandidate((uint16_t)(-1 - passArg), targetCode, penalty);
		else
			return 63;
	}
	return penalty;
}

int hwrMatchModelList(HwrStrokeModel *model, uint16_t startStroke,
		      uint16_t strokeCount, uint16_t bestLimit)
{
	int16_t minX, minY, maxX, maxY;
	uint16_t firstSeg = g_hwrPackedFeat.strokeStartSeg[startStroke];
	uint16_t lastSeg = g_hwrPackedFeat.strokeEndSeg[startStroke + strokeCount - 1];
	uint16_t aspect;
	int bestScore = bestLimit;

	g_hwrPackedFeat.startStroke = startStroke;
	g_hwrPackedFeat.strokeCount = strokeCount;
	g_hwrPackedFeat.segCount = (uint16_t)(lastSeg - firstSeg + 1);
	g_hwrPackedFeat.segPtr = &g_hwrPackedFeat.segments[firstSeg];

	hwrGetStrokeRangeBBox(startStroke, strokeCount, &minX, &minY, &maxX, &maxY);
	aspect = (uint16_t)((16 * (maxY - minY) + 16) / (maxX - minX + 1));
	if (aspect > 240u)
		aspect = 240u;

	while (model != (HwrStrokeModel *)HWR_INVALID_PTR) {
		int extraTol = 0;
		int minStrokeAdj = g_hwrPackedFeat.strokeCount;

		if (model->strictStrokeCount == 0) {
			extraTol = g_hwrCfg.extraStrokeTol;
			minStrokeAdj = g_hwrPackedFeat.strokeCount + g_hwrCfg.missingStrokeTol;
		}
		if (model->minStrokes <= minStrokeAdj &&
		    model->maxStrokes + extraTol >= g_hwrPackedFeat.strokeCount &&
		    model->minAspect <= (int)aspect && model->maxAspect >= (int)aspect) {
			const uint8_t *rules = model->ruleBytecode;

			if (rules == NULL ||
			    (rules[0] <= (uint8_t)g_hwrPackedFeat.segCount &&
			     rules[1] >= (uint8_t)g_hwrPackedFeat.segCount)) {
				int score = (int)hwrDpMatch(&g_hwrDpAlign, model->segments,
							    model->segmentCount,
							    (uint16_t)bestScore);

				if (score < bestScore && model->ruleBytecode[2] > 4u) {
					score = (uint16_t)(score + hwrEvaluateRules(
						model->ruleBytecode, startStroke, strokeCount,
						firstSeg, 0));
				}
				if (score < bestScore)
					bestScore = score;
			}
		}
		model = model->nextByChar;
	}
	return bestScore;
}

int hwrClearBBoxCache(void)
{
	uint16_t idx;

	for (idx = 0; idx < (uint32_t)g_hwrRevTriangularPtr[0] + 32u; idx++)
		g_hwrWork.bboxCache[idx].maxY = 0;
	return 1;
}

int hwrExtractUserPattern(HwrUserPatternHeader *header, uint32_t *outSegments)
{
	HwrPoint origin = g_hwrStrokeBuf->points[g_hwrStrokeBuf->segments[0].startPt];
	uint8_t minX = origin.x;
	uint8_t maxX = origin.x;
	uint8_t minY = origin.y;
	uint8_t maxY = origin.y;
	uint16_t segIdx = 0;
	uint16_t count = 0;
	int span;
	int idx;

	header->maxX = maxX;
	header->minX = minX;
	header->minY = minY;
	header->segmentCount = 0;
	header->maxY = maxY;

	do {
		const HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
		HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];

		if (minX > endPt.x) {
			minX = endPt.x;
			header->minX = minX;
		}
		if (minY > endPt.y) {
			minY = endPt.y;
			header->minY = minY;
		}
		if (maxX < endPt.x) {
			maxX = endPt.x;
			header->maxX = maxX;
		}
		if (maxY < endPt.y) {
			maxY = endPt.y;
			header->maxY = maxY;
		}
		segIdx = seg->nextSeg;
		count++;
		header->segmentCount = count;
	} while (segIdx != HWR_INVALID_SEG);

	span = (int)maxX - (int)minX + 1;
	if (span < (int)maxY - (int)minY + 1)
		span = (int)maxY - (int)minY + 1;

	segIdx = 0;
	count = 0;
	do {
		const HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
		HwrPoint startPt = g_hwrStrokeBuf->points[seg->startPt];
		HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];
		uint32_t packed = outSegments[segIdx];

		packed = (packed & 0xFFFFFFF0u) | ((uint32_t)seg->direction & 0x0Fu);
		packed = (packed & 0xFFFFFF0Fu) |
			 (((uint32_t)((8 * seg->length) / span) & 0x0Fu) << 4);
		packed = (packed & 0xFFF8FFFFu) |
			 (((uint32_t)((8 * (startPt.x - header->minX)) / span) & 7u) << 16);
		packed = (packed & 0xF8FFFFFFu) |
			 (((uint32_t)((8 * (startPt.y - header->minY)) / span) & 7u) << 24);
		packed = (packed & 0xFFC7FFFFu) |
			 (((uint32_t)((8 * (endPt.x - header->minX)) / span) & 7u) << 19);
		packed = (packed & 0xC7FFFFFFu) |
			 (((uint32_t)((8 * (endPt.y - header->minY)) / span) & 7u) << 27);
		packed = (packed & 0xFF3F00FFu) | (((uint32_t)seg->penDown & 1u) << 15);
		outSegments[segIdx] = packed;
		segIdx = seg->nextSeg;
		count++;
	} while (segIdx != HWR_INVALID_SEG);

	for (idx = 0; idx < (int)count; idx++) {
		uint32_t cur = outSegments[idx] & 0xFF3FFFFFu;
		int run = 1;
		int dirSum = 0;
		int scan;

		outSegments[idx] = cur;
		for (scan = idx + 1; scan < (int)count && run < 4 && dirSum < 6; scan++, run++) {
			uint32_t next = outSegments[scan];
			uint32_t updated = (cur & 0xFF3FFFFFu) | (((uint32_t)run & 3u) << 22);

			if ((next & 0xF0u) != 0) {
				int diff = g_hwrDirAbsDiffPtr[(int)(cur & 0x0Fu) - (int)(next & 0x0Fu) + 16];

				if (((next >> 4) & 0x0Fu) > 2u) {
					if (diff > 2)
						break;
				} else if (diff > 4) {
					break;
				}
				dirSum += diff;
			}
			outSegments[idx] = updated;
			cur = updated;
		}
	}
	return (int16_t)count;
}

void hwrResetUserResults(void)
{
	hwrMemset((uint8_t *)g_hwrUserResults, 0, sizeof(g_hwrUserResults));
	g_hwrUserResultCount = 0;
}

int hwrCopyUserResults(uint32_t *outCodes, int16_t *outScores)
{
	int count;
	int idx;

	if (outCodes == NULL || g_hwrUserRegBuf == NULL)
		return -1;
	count = g_hwrUserResultCount;
	for (idx = 0; idx < count; idx++) {
		int16_t score = g_hwrUserResults[idx].score;

		outCodes[idx] = g_hwrUserResults[idx].code;
		outScores[idx] = (score >= 20) ? score : 20;
	}
	return (int16_t)count;
}

int hwrUserDpMatch(void)
{
	uint16_t rowCost[66];
	uint16_t refLen = g_hwrUserRefHeader.segmentCount;
	uint16_t inputLen = g_hwrUserInputHeader.segmentCount;
	uint16_t colLeft = 0;
	uint16_t colRight = 0;
	uint16_t bestCol = 255;
	uint16_t rowMin = 255;
	uint16_t beamWidth = 12;
	uint16_t col;
	uint16_t row;

	if (refLen != 0) {
		uint32_t inX = (g_hwrUserInputSegments[0] >> 16) & 7u;
		uint32_t inY = (g_hwrUserInputSegments[0] >> 24) & 7u;

		for (col = 0; col < refLen; col++) {
			uint32_t refX = (g_hwrUserRefSegments[col] >> 16) & 7u;
			uint32_t refY = (g_hwrUserRefSegments[col] >> 24) & 7u;
			int16_t cost = (int16_t)(g_hwrGridPosCostTable[(refX << 3) | inX] +
						 g_hwrGridPosCostTable[(refY << 3) | inY]);

			if (col == 0) {
				rowCost[0] = (uint16_t)cost;
				rowMin = rowCost[0];
			} else {
				uint16_t acc = (uint16_t)(rowCost[col - 1] + cost);

				rowCost[col] = acc;
				colRight = (uint16_t)(col - 1);
				if (inputLen != 1 && (int)(rowMin + beamWidth) < (int)acc) {
					uint16_t fill;

					for (fill = col; fill < refLen; fill++)
						rowCost[fill] = 255u;
					break;
				}
			}
			beamWidth = (uint16_t)(beamWidth + 6);
		}
	}

	if (inputLen <= 1)
		return rowCost[refLen - 1];

	for (row = 1; row < inputLen; row++) {
		uint16_t pruneLimit = g_hwrDpPrunePtr[row];
		uint32_t inDir = g_hwrUserInputSegments[row] & 0x0Fu;
		uint16_t diagPrev = 255u;
		uint16_t leftPrev = 255u;
		uint16_t curMin = 255u;
		uint16_t curBeam = 12u;
		uint16_t scanCol;

		for (col = 0; col < colLeft; col++)
			rowCost[col] = 255u;

		for (col = colLeft; col <= colRight; col++) {
			uint16_t upCost = rowCost[col];
			uint16_t bestPrev = diagPrev;
			uint16_t curVal;

			if (bestPrev > upCost)
				bestPrev = upCost;
			if (bestPrev > leftPrev)
				bestPrev = leftPrev;
			curVal = (uint16_t)(g_hwrDirCostPtr[
				((g_hwrUserRefSegments[col] & 0x0Fu) - inDir + 16u) & 0x0Fu] + bestPrev);
			rowCost[col] = curVal;
			if (curVal < curMin) {
				curMin = curVal;
				bestCol = col;
			}
			diagPrev = upCost;
			leftPrev = curVal;
		}

		for (col = (uint16_t)(colRight + 1); col < refLen; col++) {
			uint16_t upCost = rowCost[col];
			uint16_t bestPrev = diagPrev;
			uint16_t curVal;

			if (diagPrev > upCost) {
				bestPrev = upCost;
				if (leftPrev < upCost)
					bestPrev = leftPrev;
			} else if (leftPrev < diagPrev) {
				curBeam = (uint16_t)(curBeam + 6);
				bestPrev = leftPrev;
			}
			curVal = (uint16_t)(g_hwrDirCostPtr[
				((g_hwrUserRefSegments[col] & 0x0Fu) - inDir + 16u) & 0x0Fu] + bestPrev);
			rowCost[col] = curVal;
			if (curVal < curMin) {
				curMin = curVal;
				bestCol = col;
			} else if ((int)(curMin + curBeam) < (int)curVal) {
				uint16_t fill;

				colRight = (uint16_t)(col - 1);
				for (fill = col; fill < refLen; fill++)
					rowCost[fill] = 255u;
				break;
			}
			leftPrev = curVal;
			diagPrev = upCost;
			colRight = col;
		}

		for (scanCol = colLeft; scanCol < bestCol; scanCol++) {
			if ((unsigned int)(curMin + curBeam) > rowCost[scanCol]) {
				colLeft = scanCol;
				break;
			}
		}
		if (pruneLimit < curMin)
			return 255;
	}
	return rowCost[refLen - 1];
}

int hwrMatchUserEntries(void)
{
	int entryIdx;

	if (g_hwrUserRegBuf == NULL)
		return 0;

	hwrExtractUserPattern(&g_hwrUserInputHeader, g_hwrUserInputSegments);
	g_hwrUserResultCount = 0;

	for (entryIdx = (int16_t)g_hwrUserRegCapacity - 1; entryIdx >= 0; entryIdx--) {
		const HwrUserEntry *entry = &g_hwrUserRegBuf[entryIdx];
		int score;
		int found = 0;
		int pos;

		if (entry->used == 0)
			continue;
		hwrMemcpyFast(&g_hwrUserRefHeader, &entry->header, sizeof(HwrUserPatternHeader));
		hwrMemcpyFast(g_hwrUserRefSegments, entry->segments, sizeof(g_hwrUserRefSegments));
		score = hwrUserDpMatch();
		if (score != 255) {
			int strokeCnt = g_hwrStrokeBuf->strokeCount;

			score = (12 - strokeCnt) / 2 - strokeCnt + score + 32;
			if (score > 250)
				score = 255;
		}

		for (pos = 0; pos < g_hwrUserResultCount; pos++) {
			if (g_hwrUserResults[pos].code == entry->header.internalCode) {
				found = 1;
				if (score < g_hwrUserResults[pos].score) {
					int bubble;

					g_hwrUserResults[pos].score = (int16_t)score;
					for (bubble = pos; bubble > 0; bubble--) {
						if (g_hwrUserResults[bubble - 1].score >
						    g_hwrUserResults[bubble].score) {
							HwrUserResult swapRes = g_hwrUserResults[bubble - 1];

							g_hwrUserResults[bubble - 1] = g_hwrUserResults[bubble];
							g_hwrUserResults[bubble] = swapRes;
						}
					}
				}
				break;
			}
		}

		if (!found) {
			int insIdx = g_hwrUserResultCount;
			int bubble;

			if (insIdx >= HWR_MAX_CANDIDATES)
				insIdx = HWR_MAX_CANDIDATES - 1;
			g_hwrUserResults[insIdx].code = (uint16_t)entry->header.internalCode;
			g_hwrUserResults[insIdx].score = (int16_t)score;
			g_hwrUserResultCount = insIdx + 1;
			for (bubble = insIdx; bubble > 0; bubble--) {
				if (g_hwrUserResults[bubble - 1].score >
				    g_hwrUserResults[bubble].score) {
					HwrUserResult swapRes = g_hwrUserResults[bubble - 1];

					g_hwrUserResults[bubble - 1] = g_hwrUserResults[bubble];
					g_hwrUserResults[bubble] = swapRes;
				}
			}
		}
	}
	return (int16_t)g_hwrUserResultCount;
}

int hwrRefineCandidates(HwrCandidateList *finalList, HwrCandidateList *stageList)
{
	uint8_t priorities[64];
	uint16_t idx = 0;
	uint16_t queueIdx;

	g_hwrRuleCheckCount = 0;
	g_hwrRuleCheckQueuePtr = g_hwrRuleCheckQueue;

	while (idx < finalList->count) {
		uint16_t runEnd = (uint16_t)(idx + 1);
		uint16_t baseDist = finalList->items[idx].distance;

		while (runEnd < finalList->count && finalList->items[runEnd].distance == baseDist)
			runEnd++;

		if ((int)(runEnd - idx) >= 2) {
			uint16_t pos;
			uint16_t sel;

			for (pos = idx; pos < runEnd; pos++)
				priorities[pos] = (uint8_t)hwrGetCoarsePriority(finalList->items[pos].code);

			for (sel = idx; sel < (uint16_t)(runEnd - 1); sel++) {
				uint16_t bestPos = sel;
				uint8_t bestPrio = priorities[sel];
				uint16_t scan;
				HwrCandidate swapCand;

				for (scan = (uint16_t)(sel + 1); scan < runEnd; scan++) {
					if (priorities[scan] < bestPrio) {
						bestPrio = priorities[scan];
						bestPos = scan;
					}
				}
				swapCand = finalList->items[bestPos];
				finalList->items[bestPos] = finalList->items[sel];
				finalList->items[sel] = swapCand;
				priorities[bestPos] = priorities[sel];
				priorities[sel] = bestPrio;
			}
		}
		idx = runEnd;
	}

	for (idx = 0; idx < finalList->count; idx++) {
		uint16_t subIdx = finalList->items[idx].subIndex;

		if (subIdx != 999u) {
			g_hwrRuleCheckQueue[g_hwrRuleCheckCount].model =
				(HwrStrokeModel *)stageList->items[subIdx].pattern;
			g_hwrRuleCheckQueue[g_hwrRuleCheckCount].listIndex = idx;
			g_hwrRuleCheckCount++;
		}
	}

	if (g_hwrRuleCheckCount != 0) {
		g_hwrPackedFeat.startStroke = 0;
		g_hwrPackedFeat.strokeCount = g_hwrStrokeBuf->strokeCount;
		g_hwrPackedFeat.segCount = g_hwrStrokeBuf->activeSegCount;
		g_hwrPackedFeat.segPtr = g_hwrPackedFeat.segments;

		for (queueIdx = 0; queueIdx < g_hwrRuleCheckCount; queueIdx++) {
			uint16_t targetIdx = g_hwrRuleCheckQueue[queueIdx].listIndex;
			HwrStrokeModel *model;
			uint16_t candIdx;

			if (targetIdx >= HWR_MAX_CANDIDATES)
				break;
			for (candIdx = 0; candIdx < finalList->count; candIdx++) {
				if (candIdx < targetIdx)
					finalList->items[candIdx].strokeCount = 0;
				else if (candIdx == targetIdx)
					finalList->items[candIdx].strokeCount = 32;
				else
					finalList->items[candIdx].strokeCount = 127;
			}
			model = g_hwrRuleCheckQueue[queueIdx].model;
			if (hwrDpMatch(&g_hwrDpAlign, model->segments,
				       model->segmentCount, 127u) != 255u) {
				hwrEvaluateRules(model->ruleBytecode, 0,
						 g_hwrStrokeBuf->strokeCount, 0,
						 (int16_t)(-1 - targetIdx));
			}
		}
	}
	return g_hwrRuleCheckCount;
}

static __attribute__((always_inline)) inline void hwrGetCorner(uint16_t startStroke, uint16_t endStroke, uint8_t cornerMode,
			 uint16_t *outX, uint16_t *outY)
{
	uint16_t curX;
	uint16_t curY;
	uint16_t idx;

	if (cornerMode == 3u) {
		curX = g_hwrStrokeBuf->strokeMaxX[startStroke];
		curY = g_hwrStrokeBuf->strokeMaxY[startStroke];
		for (idx = (uint16_t)(startStroke + 1); idx <= endStroke; idx++) {
			if (g_hwrStrokeBuf->strokeMaxX[idx] > curX)
				curX = g_hwrStrokeBuf->strokeMaxX[idx];
			if (g_hwrStrokeBuf->strokeMaxY[idx] > curY)
				curY = g_hwrStrokeBuf->strokeMaxY[idx];
		}
	} else if (cornerMode == 2u) {
		curX = g_hwrStrokeBuf->strokeMaxX[startStroke];
		curY = g_hwrStrokeBuf->strokeMinY[startStroke];
		for (idx = (uint16_t)(startStroke + 1); idx <= endStroke; idx++) {
			if (g_hwrStrokeBuf->strokeMaxX[idx] > curX)
				curX = g_hwrStrokeBuf->strokeMaxX[idx];
			if (g_hwrStrokeBuf->strokeMinY[idx] < curY)
				curY = g_hwrStrokeBuf->strokeMinY[idx];
		}
	} else if (cornerMode == 1u) {
		curX = g_hwrStrokeBuf->strokeMinX[startStroke];
		curY = g_hwrStrokeBuf->strokeMaxY[startStroke];
		for (idx = (uint16_t)(startStroke + 1); idx <= endStroke; idx++) {
			if (g_hwrStrokeBuf->strokeMinX[idx] < curX)
				curX = g_hwrStrokeBuf->strokeMinX[idx];
			if (g_hwrStrokeBuf->strokeMaxY[idx] > curY)
				curY = g_hwrStrokeBuf->strokeMaxY[idx];
		}
	} else {
		curX = g_hwrStrokeBuf->strokeMinX[startStroke];
		curY = g_hwrStrokeBuf->strokeMinY[startStroke];
		for (idx = (uint16_t)(startStroke + 1); idx <= endStroke; idx++) {
			if (g_hwrStrokeBuf->strokeMinX[idx] < curX)
				curX = g_hwrStrokeBuf->strokeMinX[idx];
			if (g_hwrStrokeBuf->strokeMinY[idx] < curY)
				curY = g_hwrStrokeBuf->strokeMinY[idx];
		}
	}
	*outX = curX;
	*outY = curY;
}

int hwrCheckRadicalLayout(const HwrDictCharEntry *entry, uint16_t leftStart,
			  uint16_t leftCount, uint16_t rightStart, uint16_t rightCount)
{
	uint16_t leftEnd = (uint16_t)(leftStart + leftCount - 1);
	uint16_t rightEnd = (uint16_t)(rightStart + rightCount - 1);
	uint16_t ruleIdx;

	for (ruleIdx = 0; ruleIdx < 2u; ruleIdx++) {
		uint8_t rule = entry->layoutRules[ruleIdx];
		uint8_t relation = rule >> 4;
		uint16_t leftX, leftY;
		uint16_t rightX, rightY;

		if (relation == 0)
			return 1;
		hwrGetCorner(leftStart, leftEnd, rule & 3u, &leftX, &leftY);
		hwrGetCorner(rightStart, rightEnd, (rule >> 2) & 3u, &rightX, &rightY);

		switch (relation) {
		case 1:
			if (leftX > rightX)
				return 0;
			break;
		case 2:
			if (leftX < rightX)
				return 0;
			break;
		case 3:
			if (leftY > rightY)
				return 0;
			break;
		case 4:
			if (leftY > rightY || leftX > rightX)
				return 0;
			break;
		case 5:
			if (leftY > rightY || leftX < rightX)
				return 0;
			break;
		case 6:
			if (leftY < rightY)
				return 0;
			break;
		case 7:
			if (leftY < rightY || leftX > rightX)
				return 0;
			break;
		case 8:
			if (leftY < rightY || leftX < rightX)
				return 0;
			break;
		case 9: {
			uint16_t lMinX, lMinY, lMaxX, lMaxY;
			uint16_t rMinX, rMinY, rMaxX, rMaxY;
			int rW, lW, rH, lH;

			hwrGetCorner(leftStart, leftEnd, 0, &lMinX, &lMinY);
			hwrGetCorner(leftStart, leftEnd, 3, &lMaxX, &lMaxY);
			hwrGetCorner(rightStart, rightEnd, 0, &rMinX, &rMinY);
			hwrGetCorner(rightStart, rightEnd, 3, &rMaxX, &rMaxY);
			rW = (int)rMaxX - (int)rMinX;
			lW = (int)lMaxX - (int)lMinX;
			rH = (int)rMaxY - (int)rMinY;
			lH = (int)lMaxY - (int)lMinY;
			if (!((int)(((uint32_t)lMinX + lMaxX) >> 1) <= (int)rMinX &&
			      rW <= lW && rH <= lH &&
			      (rW <= lW / 2 || rH <= lH / 2))) {
				return 0;
			}
			break;
		}
		default:
			break;
		}
	}
	return 1;
}

static __attribute__((always_inline)) inline int hwrMatchSplitPair(const HwrDictCharEntry *firstRad,
			     const HwrDictCharEntry *secondRad,
			     uint16_t startStroke, uint16_t splitStroke,
			     uint16_t firstCount, uint16_t secondCount,
			     int layoutOk, int bestScore)
{
	int score;

	if (startStroke == 0) {
		int firstScore;

		g_hwrRulePenalty = 32;
		firstScore = hwrMatchRadicalTree(firstRad, 0, firstCount);
		g_hwrRulePenalty = 10;
		if (firstScore == 127)
			return bestScore;
		score = (int)((uint32_t)(firstScore +
					 hwrMatchRadicalTree(secondRad, splitStroke, secondCount)) >> 1);
	} else if (firstCount >= secondCount) {
		int secondScore = hwrMatchRadicalTree(secondRad, splitStroke, secondCount);
		int firstScore;

		if (secondScore == 127)
			return bestScore;
		firstScore = hwrMatchRadicalTree(firstRad, startStroke, firstCount);
		if (firstScore == 127)
			return bestScore;
		score = (int)((uint32_t)(firstScore + secondScore) >> 1);
	} else {
		int firstScore = hwrMatchRadicalTree(firstRad, startStroke, firstCount);
		int secondScore;

		if (firstScore == 127)
			return bestScore;
		secondScore = hwrMatchRadicalTree(secondRad, splitStroke, secondCount);
		if (secondScore == 127)
			return bestScore;
		score = (int)((uint32_t)(firstScore + secondScore) >> 1);
	}

	if (layoutOk == 0)
		score = (uint16_t)(score + 10);
	if (score < bestScore)
		bestScore = score;
	return bestScore;
}

int hwrMatchRadicalTree(const HwrDictCharEntry *entry, uint16_t startStroke,
			uint16_t strokeCount)
{
	int bestScore = 127;

	if (strokeCount >= 2u && entry->splitDirMask != 0) {
		const HwrDictCharEntry *leftRad = entry->leftRadical;
		const HwrDictCharEntry *rightRad = entry->rightRadical;
		uint16_t splitCount;

		for (splitCount = 1; splitCount < strokeCount; splitCount++) {
			uint16_t splitStroke = (uint16_t)(startStroke + splitCount);

			if (g_hwrStrokeBuf->strokeGroup[splitStroke - 1] ==
			    g_hwrStrokeBuf->strokeGroup[splitStroke])
				continue;

			if ((g_hwrInterStrokeDirMask[splitStroke] & entry->splitDirMask) != 0 &&
			    leftRad->minStrokes <= splitCount && leftRad->maxStrokes >= splitCount) {
				uint16_t remCount = (uint16_t)(strokeCount - splitCount);

				if (rightRad->minStrokes <= remCount && rightRad->maxStrokes >= remCount) {
					int layoutOk = hwrCheckRadicalLayout(
						entry, startStroke, splitCount,
						splitStroke, remCount);

					bestScore = hwrMatchSplitPair(
						leftRad, rightRad, startStroke, splitStroke,
						splitCount, remCount, layoutOk, bestScore);
				}
			}

			if (entry->splitDirMask == 126u &&
			    (g_hwrInterStrokeDirMask[splitStroke] & 0x7Eu) != 0) {
				uint16_t remCount = (uint16_t)(strokeCount - splitCount);

				if (leftRad->minStrokes <= remCount && leftRad->maxStrokes >= remCount &&
				    rightRad->minStrokes <= splitCount && rightRad->maxStrokes >= splitCount) {
					int layoutOk = hwrCheckRadicalLayout(
						entry, splitStroke, remCount,
						startStroke, splitCount);

					bestScore = hwrMatchSplitPair(
						rightRad, leftRad, startStroke, splitStroke,
						splitCount, remCount, layoutOk, bestScore);
				}
			}
		}
	}

	if (entry->firstModel != (HwrStrokeModel *)HWR_INVALID_PTR)
		return hwrMatchModelList(entry->firstModel, startStroke, strokeCount,
					 (uint16_t)bestScore);
	return bestScore;
}

static __attribute__((always_inline)) inline int hwrFilterByMode(uint16_t *codePtr)
{
	uint16_t code = *codePtr;
	uint16_t cat = (uint16_t)hwrGetCharCategory(code);

	if ((cat & g_hwrModeMask) != 0)
		return 1;
	if ((g_hwrModeMask & SCE_HANDWRITING_MODE_NUMBER) != 0)
		return 0;
	if ((g_hwrModeMask & SCE_HANDWRITING_MODE_UALPHA) != 0 && cat == 8u) {
		if (code == 223u || code == 255u)
			return 0;
		if (code < 0x100u || code > 0x17Fu)
			*codePtr = (uint16_t)(code - 32u);
		else if (code != 312u && code != 329u && code != 376u && code != 383u)
			*codePtr = (uint16_t)(code - 1u);
		return 1;
	}
	if ((g_hwrModeMask & SCE_HANDWRITING_MODE_LALPHA) != 0 && cat == 4u) {
		if (code < 0x100u || code > 0x17Fu)
			*codePtr = (uint16_t)(code + 32u);
		else if (code != 312u && code != 329u && code != 376u && code != 383u)
			*codePtr = (uint16_t)(code + 1u);
		return 1;
	}
	return 0;
}

int hwrRecognizeCandidates(HwrCandidate *outResults, uint16_t maxOut)
{
	HwrCandidate workCands[HWR_MAX_CANDIDATES];
	uint16_t totalStrokes = g_hwrStrokeBuf->strokeCount;
	uint16_t coarseCount;
	uint16_t strokeIdx;
	uint16_t segIdx;
	uint16_t idx;
	uint16_t outCount;
	int16_t prevCode = -1;
	HwrCandidateList *stageList;

	for (strokeIdx = 1; (int)strokeIdx < (int)totalStrokes - 1; strokeIdx++)
		g_hwrStagePtr->lists[strokeIdx - 1].count = 1;

	g_hwrStrokeBuf->strokeGroup[0] = 0;
	for (strokeIdx = 1; strokeIdx < g_hwrStrokeBuf->strokeCount; strokeIdx++) {
		uint16_t prev;

		g_hwrStrokeBuf->strokeGroup[strokeIdx] = (uint8_t)strokeIdx;
		for (prev = 0; prev < strokeIdx; prev++) {
			if (hwrStrokesIntersect(prev, strokeIdx) != 0) {
				uint16_t fill;

				for (fill = (uint16_t)(prev + 1); fill <= strokeIdx; fill++) {
					g_hwrStrokeBuf->strokeGroup[fill] =
						g_hwrStrokeBuf->strokeGroup[prev];
					g_hwrStagePtr->lists[fill - 1].count = 0;
				}
				break;
			}
		}
	}

	for (strokeIdx = 1; strokeIdx < g_hwrStrokeBuf->strokeCount; strokeIdx++)
		hwrPackStrokeFeatures((uint16_t)(strokeIdx - 1));
	hwrPackStrokeFeatures((uint16_t)(g_hwrStrokeBuf->strokeCount - 1));

	g_hwrFineMatchPass = 1;
	g_hwrRulePenalty = 63;
	coarseCount = (uint16_t)hwrRunCoarseMatch();
	hwrClearBBoxCache();
	hwrMatchUserEntries();
	hwrRunStrokeMatchPass((int16_t)g_hwrStrokeBuf->strokeCount, 1u);
	g_hwrFineMatchPass = 0;
	g_hwrRulePenalty = 10;

	{
		uint16_t jumpIdx = 0;

		for (segIdx = 0; segIdx < g_hwrStrokeBuf->activeSegCount; segIdx++) {
			const HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];

			if (seg->penDown == 0) {
				HwrPoint startPt = g_hwrStrokeBuf->points[seg->startPt];
				HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];
				int dist = hwrOctagonalDistance(startPt.x, startPt.y, endPt.x, endPt.y);
				uint16_t dirBit;

				if (dist >= 6) {
					uint16_t bucket = (uint16_t)((seg->direction + 1) >> 1);

					if (bucket == 8u)
						bucket = 0;
					dirBit = (uint16_t)(1u << bucket);
				} else {
					dirBit = 255u;
				}
				jumpIdx++;
				g_hwrInterStrokeDirMask[jumpIdx] = dirBit;
			}
		}
	}

	g_hwrFinalList.items = workCands;
	g_hwrFinalList.capacity = maxOut;
	g_hwrFinalList.count = 0;
	g_hwrFinalList.maxScore = 127;
	for (idx = 0; idx < maxOut; idx++) {
		workCands[idx].pattern = NULL;
		workCands[idx].distance = 0;
		workCands[idx].code = 0;
		workCands[idx].strokeCount = 0;
		workCands[idx].subIndex = 0;
	}

	for (idx = 0; idx < coarseCount; idx++) {
		HwrDictCharEntry *entry = g_hwrCoarseCharPtrs[idx];
		uint16_t lastCoarseDist = g_hwrCoarseMatches[coarseCount - 1].distance;
		uint16_t firstCoarseDist = g_hwrCoarseMatches[0].distance;
		uint16_t curCoarseDist = g_hwrCoarseMatches[idx].distance;

		if (coarseCount >= 2u &&
		    ((uint32_t)(g_hwrCoarseMatches[1].distance - firstCoarseDist) << 7) >=
		    20u * lastCoarseDist) {
			uint16_t score = (uint16_t)((140 * (curCoarseDist - firstCoarseDist)) /
						    lastCoarseDist + 40);

			hwrInsertCandidate(&g_hwrFinalList, entry, score, 0, 0, 999, 2);
		} else if (coarseCount >= 2u && idx < 16u && g_hwrStrokeBuf->strokeCount >= 6u) {
			uint16_t score = (uint16_t)((140 * (curCoarseDist - firstCoarseDist)) /
						    lastCoarseDist + 75);

			hwrInsertCandidate(&g_hwrFinalList, entry, score, 0, 0, 999, 2);
		}

		if (entry->splitDirMask != 0) {
			uint16_t curStrokes = g_hwrStrokeBuf->strokeCount;

			if (curStrokes >= entry->minStrokes && curStrokes <= entry->maxStrokes &&
			    prevCode != (int16_t)entry->code) {
				int treeScore;

				prevCode = (int16_t)entry->code;
				treeScore = hwrMatchRadicalTree(entry, 0, curStrokes);
				if (lastCoarseDist != 0) {
					treeScore = (uint16_t)(treeScore +
						(140 * (curCoarseDist - firstCoarseDist)) /
						lastCoarseDist);
				}
				if (treeScore < (int)g_hwrFinalList.maxScore) {
					hwrInsertCandidate(&g_hwrFinalList, entry,
							   (uint16_t)(treeScore + 38),
							   0, 0, 999, 2);
				}
			}
		}
	}

	stageList = &g_hwrStagePtr->lists[g_hwrStrokeBuf->strokeCount - 1];
	for (idx = 0; idx < stageList->count; idx++) {
		uint16_t dist = stageList->items[idx].distance;

		if (dist < g_hwrFinalList.maxScore) {
			HwrStrokeModel *model = (HwrStrokeModel *)stageList->items[idx].pattern;

			hwrInsertCandidate(&g_hwrFinalList, model->ownerChar,
					   (uint16_t)(dist + 38), 0, 0, (int16_t)idx, 2);
		}
	}

	hwrSortCandidates(&g_hwrFinalList, 0, (int16_t)(g_hwrFinalList.count - 1));
	hwrRefineCandidates(&g_hwrFinalList, stageList);

	outCount = 0;
	for (idx = 0; idx < g_hwrFinalList.count && idx < maxOut; idx++) {
		uint16_t candCode = workCands[idx].code;
		int dup = 0;
		uint16_t check;

		if (!hwrFilterByMode(&candCode))
			continue;
		workCands[idx].code = candCode;
		for (check = 0; check < outCount; check++) {
			if (outResults[check].code == candCode) {
				dup = 1;
				break;
			}
		}
		if (!dup) {
			outResults[outCount] = workCands[idx];
			outCount++;
		}
	}

	if (coarseCount != 0) {
		uint16_t insertPos = 0;

		for (idx = 0; idx < coarseCount; idx++) {
			HwrDictCharEntry *entry = g_hwrCoarseCharPtrs[idx];

			if (entry == NULL)
				break;
			if (entry->splitDirMask == 0 &&
			    entry->firstModel == (HwrStrokeModel *)HWR_INVALID_PTR) {
				int exists = 0;
				uint16_t check;

				for (check = 0; check < outCount; check++) {
					if (outResults[check].code == entry->code) {
						exists = 1;
						break;
					}
				}
				if (!exists) {
					for (;;) {
						uint16_t limit = (outCount < maxOut)
							? outCount : (uint16_t)(maxOut - 1);
						uint16_t scan;
						int inserted = 0;

						if (insertPos >= limit)
							break;
						for (scan = 0; scan < coarseCount; scan++) {
							HwrDictCharEntry *other = g_hwrCoarseCharPtrs[scan];

							if (other == NULL)
								break;
							if (other->code == outResults[insertPos].code) {
								if (g_hwrCoarseMatches[idx].distance <
								    g_hwrCoarseMatches[scan].distance) {
									int shiftTop = (outCount < maxOut - 1)
										? outCount : (int)maxOut - 2;
									int mv;

									for (mv = shiftTop; mv >= (int)insertPos; mv--)
										outResults[mv + 1] = outResults[mv];
									outResults[insertPos].code = entry->code;
									outResults[insertPos].distance = 0;
									if (outCount < maxOut - 1) {
										g_hwrFinalList.count++;
										outCount++;
									}
									inserted = 1;
								}
								break;
							}
						}
						if (inserted)
							break;
						insertPos++;
					}
				}
			}
		}
	}

	g_hwrFinalList.count = outCount;

	if (outCount == 0 && coarseCount != 0) {
		for (idx = 0; idx < coarseCount; idx++) {
			uint16_t fallbackCode = g_hwrCoarseCharCodes[idx];

			if (fallbackCode == 0)
				continue;
			if (hwrFilterByMode(&fallbackCode)) {
				uint16_t lastDist = g_hwrCoarseMatches[coarseCount - 1].distance;
				uint16_t firstDist = g_hwrCoarseMatches[0].distance;
				uint16_t curDist = g_hwrCoarseMatches[idx].distance;

				g_hwrCoarseCharCodes[idx] = fallbackCode;
				outResults[outCount].pattern = NULL;
				outResults[outCount].code = fallbackCode;
				outResults[outCount].distance = (uint16_t)(
					(((int)curDist + 2 * (int)lastDist - 3 * (int)firstDist) << 6) /
					((int)lastDist + 1));
				outResults[outCount].strokeCount = 0;
				outCount++;
				break;
			}
		}
	}

	if (outCount > maxOut)
		outCount = maxOut;
	g_hwrFinalList.count = outCount;

	{
		uint16_t prevScore = 0;

		for (idx = 0; idx < g_hwrFinalList.count; idx++) {
			if (outResults[idx].distance != 0) {
				prevScore = outResults[idx].distance;
			} else {
				uint16_t nextScore = 255u;
				uint16_t scan;

				for (scan = (uint16_t)(idx + 1); scan < g_hwrFinalList.count; scan++) {
					if (outResults[scan].distance != 0) {
						nextScore = outResults[scan].distance;
						break;
					}
				}
				if (prevScore == 0)
					prevScore = (uint16_t)(nextScore / 2);
				outResults[idx].distance = (uint16_t)((prevScore + nextScore) / 2);
			}
		}
	}

	for (idx = 0; (int)idx < g_hwrUserResultCount; idx++) {
		int16_t userScore = g_hwrUserResults[idx].score;
		uint16_t userCode = g_hwrUserResults[idx].code;
		uint16_t cat;
		uint16_t existingIdx;
		uint16_t targetIdx;

		if (userScore >= (int)g_hwrFinalList.maxScore)
			break;
		cat = (uint16_t)hwrGetCharCategory(userCode);
		if (cat == 0 || (cat & 0x2000u) != 0)
			cat = 4096u;
		if ((cat & g_hwrModeMask) == 0)
			continue;

		existingIdx = 0;
		while (existingIdx < g_hwrFinalList.count &&
		       outResults[existingIdx].code != userCode) {
			existingIdx++;
		}
		targetIdx = 0;
		while (targetIdx < g_hwrFinalList.count &&
		       userScore > (int)outResults[targetIdx].distance) {
			targetIdx++;
		}
		if (targetIdx <= existingIdx) {
			int canInsert = 0;

			if (existingIdx != g_hwrFinalList.count) {
				canInsert = 1;
			} else if (g_hwrFinalList.count < maxOut) {
				g_hwrFinalList.count++;
				canInsert = 1;
			} else if (targetIdx != g_hwrFinalList.count) {
				canInsert = 1;
			}

			if (canInsert) {
				int shiftTop = (existingIdx >= maxOut)
					? (int)maxOut - 2 : (int)existingIdx - 1;
				int mv;

				for (mv = shiftTop; mv >= (int)targetIdx; mv--)
					outResults[mv + 1] = outResults[mv];
				outResults[targetIdx].code = userCode;
				outResults[targetIdx].distance = (uint16_t)(userScore >= 20 ? userScore : 20);
			}
		}
	}

	return (int16_t)g_hwrFinalList.count;
}

void hwrInsertRuleCandidate(uint16_t baseIdx, uint16_t code, uint16_t penalty)
{
	uint16_t baseDist = g_hwrFinalList.items[baseIdx].distance;
	uint16_t insPos = 0;

	while (insPos < g_hwrFinalList.count) {
		HwrCandidate *cand = &g_hwrFinalList.items[insPos];

		if (cand->distance > baseDist || cand->strokeCount > penalty)
			break;
		if (cand->code == code)
			return;
		insPos++;
	}

	if (insPos < g_hwrFinalList.count) {
		uint16_t dupPos = insPos;
		uint16_t mv;
		uint16_t queueIdx;

		while (dupPos < g_hwrFinalList.count &&
		       g_hwrFinalList.items[dupPos].code != code) {
			dupPos++;
		}
		if (dupPos != g_hwrFinalList.count) {
			for (mv = dupPos; (uint16_t)(mv + 1) < g_hwrFinalList.count; mv++)
				g_hwrFinalList.items[mv] = g_hwrFinalList.items[mv + 1];
			g_hwrFinalList.count--;
		}
		for (mv = 31; mv > insPos; mv--)
			g_hwrFinalList.items[mv] = g_hwrFinalList.items[mv - 1];

		for (queueIdx = 0; queueIdx < g_hwrRuleCheckCount; queueIdx++) {
			if (g_hwrRuleCheckQueue[queueIdx].listIndex >= insPos)
				g_hwrRuleCheckQueue[queueIdx].listIndex++;
		}
	}

	if (insPos < g_hwrFinalList.capacity) {
		HwrCandidate *slot = &g_hwrFinalList.items[insPos];

		slot->distance = baseDist;
		slot->code = code;
		slot->strokeCount = penalty;
		slot->subIndex = 255;
		slot->pattern = (void *)HWR_INVALID_PTR;
		if (g_hwrFinalList.count < HWR_MAX_CANDIDATES)
			g_hwrFinalList.count++;
	}
}

void hwrInsertCandidate(HwrCandidateList *list, void *pattern, uint16_t distance,
			uint16_t code, uint16_t strokeCount, int16_t subIndex,
			int16_t mode)
{
	uint16_t idx;

	if (mode == 2) {
		if (pattern != NULL) {
			code = ((HwrDictCharEntry *)pattern)->code;
			if (code == 0)
				return;
		}
		for (idx = 0; idx < list->count; idx++) {
			if (list->items[idx].pattern == pattern) {
				if (list->items[idx].distance > distance) {
					list->items[idx].distance = distance;
					if (list->count == list->capacity) {
						uint16_t worst = 0;
						uint16_t scan;

						list->maxScore = 0;
						for (scan = 0; scan < list->capacity; scan++) {
							if (worst < list->items[scan].distance) {
								worst = list->items[scan].distance;
								list->maxScore = worst;
								list->worstIndex = scan;
							}
						}
					}
				}
				return;
			}
		}
	} else if (mode != 0) {
		for (idx = 0; idx < list->count; idx++) {
			if (list->items[idx].pattern == pattern &&
			    list->items[idx].code == code &&
			    list->items[idx].strokeCount == strokeCount) {
				return;
			}
		}
	}

	if (list->count >= list->capacity) {
		HwrCandidate *slot = &list->items[list->worstIndex];

		slot->distance = distance;
		slot->code = code;
		slot->pattern = pattern;
		slot->strokeCount = strokeCount;
		slot->subIndex = (uint16_t)subIndex;
	} else {
		HwrCandidate *slot = &list->items[list->count++];

		slot->distance = distance;
		slot->code = code;
		slot->pattern = pattern;
		slot->strokeCount = strokeCount;
		slot->subIndex = (uint16_t)subIndex;
	}

	if (list->count == list->capacity) {
		uint16_t worst = 0;
		uint16_t scan;

		list->maxScore = 0;
		for (scan = 0; scan < list->capacity; scan++) {
			if (worst < list->items[scan].distance) {
				worst = list->items[scan].distance;
				list->maxScore = worst;
				list->worstIndex = scan;
			}
		}
	}
}

void hwrSortCandidates(HwrCandidateList *list, int16_t low, int16_t high)
{
	while (low < high) {
		int16_t left = low;
		int16_t right = high;
		int mid = (low + high) / 2;
		uint16_t pivotDist = list->items[mid].distance;
		uintptr_t pivotPtr = (uintptr_t)list->items[mid].pattern;

		while (left <= right) {
			while (list->items[left].distance < pivotDist ||
			       (list->items[left].distance == pivotDist &&
				(uintptr_t)list->items[left].pattern < pivotPtr)) {
				left++;
			}
			while (list->items[right].distance > pivotDist ||
			       (list->items[right].distance == pivotDist &&
				(uintptr_t)list->items[right].pattern > pivotPtr)) {
				right--;
			}
			if (left <= right) {
				HwrCandidate swapCand = list->items[left];

				list->items[left] = list->items[right];
				list->items[right] = swapCand;
				left++;
				right--;
			}
		}
		if (low < right)
			hwrSortCandidates(list, low, right);
		low = left;
	}
}

int hwrGetCharCategory(uint16_t code)
{
	uint16_t result = 0;
	const uint8_t *cursor = (const uint8_t *)g_hwrDictHeader->charTable;
	uint16_t idx;

	for (idx = 0; idx < g_hwrDictHeader->charCount; idx++) {
		const HwrDictCharEntry *entry = (const HwrDictCharEntry *)cursor;

		if (entry->code == code)
			result = (uint16_t)(1u << ((entry->category & 0x7Fu) - 1u));
		cursor += 24u + 4u * entry->extraCount;
	}
	switch (code) {
	case 'A': case 'B': case 'E': case 'H': case 'I':
	case 'K': case 'M': case 'N': case 'O': case 'P':
	case 'T': case 'X': case 'Z':
		result |= 0x40u;
		break;
	default:
		break;
	}
	return result;
}
