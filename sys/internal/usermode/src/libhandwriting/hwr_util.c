/*
 * hwr_util.c: coarse directional-histogram classifier, grid normalization,
 * version signature verification, and internal string/memory routines.
 */
#include "hwr_internal.h"

const HwrVersionAuthData g_hwrVersionAuth = {
	{
		{ 0x3a, 0x2e, 0x42, 0xb5, 0xa3, 0x24, 0xd7, 0x91, 0xf5, 0x2f, 0xd9, 0x96 },
		{ 0x7e, 0xc0, 0x41, 0x88, 0xbb, 0x74, 0xd0, 0x94, 0xf6, 0xeb, 0xc4, 0x51 },
		{ 0x21, 0x15, 0x41, 0xad, 0x84, 0x19, 0xa9, 0xd3, 0xac, 0xe1, 0x2d, 0x8a },
		{ 0x24, 0x79, 0x41, 0xd0, 0x8e, 0x77, 0xff, 0x68, 0x66, 0xcd, 0x41, 0xc9 },
		{ 0x5e, 0x0d, 0x4d, 0x44, 0xb3, 0xb2, 0x33, 0x46, 0x6f, 0x7a, 0x7c, 0xad },
		{ 0xe5, 0x2c, 0x40, 0xf0, 0x87, 0x74, 0x6e, 0x8b, 0x44, 0x6c, 0x9f, 0x47 },
		{ 0xe1, 0xa1, 0x4f, 0xbd, 0xa0, 0xad, 0x25, 0xee, 0x4c, 0xea, 0x1e, 0x2e },
		{ 0xba, 0xf6, 0x41, 0x76, 0xa9, 0xf0, 0x1d, 0xde, 0x39, 0x4f, 0x68, 0xa9 },
		{ 0x71, 0x8b, 0x45, 0x6c, 0xaa, 0x29, 0xcc, 0x5a, 0xaa, 0x10, 0xd3, 0x5f },
		{ 0x27, 0xe7, 0x4b, 0x59, 0xb4, 0xab, 0xe8, 0xfb, 0x1a, 0x37, 0x2c, 0x18 },
		{ 0x48, 0x6c, 0x41, 0x32, 0xb6, 0x4c, 0x1d, 0xbd, 0xc7, 0x8b, 0xfb, 0x5a },
		{ 0x2b, 0x94, 0x4b, 0xdf, 0xba, 0x94, 0x87, 0x19, 0x7f, 0xf7, 0x85, 0x85 },
		{ 0x4b, 0x27, 0x43, 0x22, 0x83, 0xcd, 0x48, 0xec, 0xdb, 0x2a, 0x9a, 0xd2 },
		{ 0xb8, 0xec, 0x41, 0x0d, 0x85, 0xf9, 0x9c, 0xe0, 0x23, 0xf7, 0x33, 0xb4 },
		{ 0xd9, 0xe4, 0x4b, 0x3d, 0x94, 0x54, 0x35, 0x9c, 0x76, 0xfb, 0xb5, 0xd9 },
		{ 0x73, 0x2d, 0x42, 0x73, 0x8a, 0x1b, 0x59, 0x41, 0x58, 0x1a, 0x81, 0x7e },
	},
	"HWRSCEIV31318",
};

const uint8_t g_hwrVersionCipher[24] = {
	0x63, 0x13, 0xc3, 0x94, 0x19, 0x4b, 0x8c, 0xdf, 0xf9, 0xba, 0xd1, 0x94,
	0xce, 0x87, 0x4f, 0x19, 0x4c, 0x7f, 0xc6, 0xf7, 0xb6, 0x85, 0xb4, 0x85,
};

uint16_t g_hwrCoarseResultCount;
uint16_t g_hwrCoarseTemplateCount;
HwrCoarseMatch g_hwrCoarseMatches[HWR_MAX_COARSE_CANDIDATES];
const uint8_t *g_hwrCoarseFeatures;
const uint32_t *g_hwrCoarseGroupPool;
const uint8_t *g_hwrCoarseMinStrokes;
const uint32_t *g_hwrCoarseGroupPtrs[30000];
uint8_t g_hwrGridBoundsX[8];
uint8_t g_hwrGridBoundsY[8];
uint8_t g_hwrResampledPoints[136];

int hwrCoarseInit(void)
{
	HwrCoarseHeader *coarse = g_hwrDictHeader->coarseHeader;
	uint16_t count = coarse->templateCount;
	uint16_t idx;
	uint32_t poolPos = 0;

	g_hwrCoarseDict = coarse;
	g_hwrCoarseTemplateCount = count;
	g_hwrCoarseMinStrokes = coarse->minStrokes;
	g_hwrCoarseFeatures = coarse->featureVectors;
	g_hwrCoarseMaxCandidates = (coarse->maxCandidates != 0) ? coarse->maxCandidates : 32u;
	g_hwrCoarseGroupPool = coarse->groupPool;

	for (idx = 0; idx < count; idx++) {
		g_hwrCoarseGroupPtrs[idx] = &coarse->groupPool[poolPos];
		poolPos += coarse->groupPool[poolPos] + 1u;
	}
	return 0;
}

int hwrCoarseTerm(void)
{
	return 0;
}

static void hwrDeduplicateCoarseChars(int16_t *countPtr)
{
	int16_t count = *countPtr;
	int16_t outer;

	for (outer = 0; outer < count - 1; outer++) {
		int16_t inner;

		for (inner = (int16_t)(outer + 1); inner < count; inner++) {
			if (g_hwrCoarseCharPtrs[outer] == g_hwrCoarseCharPtrs[inner]) {
				int16_t mv;

				for (mv = inner; mv < count; mv++) {
					g_hwrCoarseCharPtrs[mv] = g_hwrCoarseCharPtrs[mv + 1];
					g_hwrCoarseRankIndex[mv] = g_hwrCoarseRankIndex[mv + 1];
					g_hwrCoarseMatches[mv] = g_hwrCoarseMatches[mv + 1];
				}
				count--;
				inner--;
			}
		}
	}
	*countPtr = count;
}

int hwrRunCoarseMatch(void)
{
	uint8_t normFeat[64];
	uint16_t matchCount;
	uint16_t idx;
	int16_t charCount;
	uint16_t baseCharCount;
	uint16_t tailDist;

	hwrBuildCoarseInputFeature();
	matchCount = (uint16_t)hwrSearchCoarseTemplates(
		g_hwrWork.grid.featureHist, g_hwrStrokeBuf->strokeCount);
	g_hwrCoarseCharCount = 0;

	if (matchCount >= 2u) {
		for (idx = 0; idx < matchCount; idx++) {
			const uint8_t *tmpl = &g_hwrCoarseFeatures[
				(uint32_t)g_hwrCoarseMatches[idx].templateIdx << 6];
			int16_t row;

			for (row = 0; row < 4; row++) {
				int16_t col;

				for (col = 0; col < 4; col++) {
					int base = row * 16 + col * 4;
					int inSum = g_hwrWork.grid.featureHist[base] +
						    g_hwrWork.grid.featureHist[base + 1] +
						    g_hwrWork.grid.featureHist[base + 2] +
						    g_hwrWork.grid.featureHist[base + 3];
					int refSum = tmpl[base] + tmpl[base + 1] +
						     tmpl[base + 2] + tmpl[base + 3];
					int diff = inSum - refSum;

					if (diff < 0)
						diff = -diff;
					g_hwrCoarseMatches[idx].distance = (uint16_t)(
						g_hwrCoarseMatches[idx].distance + diff / 4);
				}
			}
		}

		hwrMemset(normFeat, 0, sizeof(normFeat));
		{
			uint16_t bin;
			int total = 0;

			for (bin = 0; bin < 64u; bin++)
				total += (int16_t)g_hwrWork.grid.featureHist[bin];
			if (total != 0) {
				for (bin = 0; bin < 64u; bin++) {
					g_hwrWork.grid.featureHist[bin] = (uint16_t)(
						(3584u * (uint32_t)(int16_t)g_hwrWork.grid.featureHist[bin]) /
						(uint32_t)total);
				}
			}
		}

		for (idx = 0; idx < matchCount; idx++) {
			const uint8_t *tmpl = &g_hwrCoarseFeatures[
				(uint32_t)g_hwrCoarseMatches[idx].templateIdx << 6];
			int total = 0;
			int16_t bin;

			for (bin = 0; bin < 64; bin++)
				total += tmpl[bin];
			if (total != 0) {
				for (bin = 0; bin < 64; bin++)
					normFeat[bin] = (uint8_t)((3584u * tmpl[bin]) / (uint32_t)total);
			}
			for (bin = 0; bin < 64; bin++) {
				int diff = (int)g_hwrWork.grid.featureHist[bin] - (int)normFeat[bin];

				if (diff < 0)
					diff = -diff;
				g_hwrCoarseMatches[idx].distance = (uint16_t)(
					g_hwrCoarseMatches[idx].distance + diff / 8);
			}
		}
	}

	for (idx = 0; (int)idx < (int)matchCount - 1; idx++) {
		uint16_t scan;

		for (scan = (uint16_t)(idx + 1); scan < matchCount; scan++) {
			if (g_hwrCoarseMatches[idx].distance > g_hwrCoarseMatches[scan].distance) {
				HwrCoarseMatch swapMatch = g_hwrCoarseMatches[idx];

				g_hwrCoarseMatches[idx] = g_hwrCoarseMatches[scan];
				g_hwrCoarseMatches[scan] = swapMatch;
			}
		}
	}

	charCount = (int16_t)g_hwrCoarseCharCount;
	for (idx = 0; idx < matchCount; idx++) {
		const uint32_t *group;

		if (g_hwrCoarseMatches[idx].distance == 0xFFFFu)
			break;
		group = g_hwrCoarseGroupPtrs[g_hwrCoarseMatches[idx].templateIdx];
		g_hwrCoarseRankIndex[charCount] = (charCount < 255) ? (uint8_t)charCount : 255u;
		g_hwrCoarseCharPtrs[charCount] = (HwrDictCharEntry *)(uintptr_t)group[1];
		charCount++;
	}

	hwrDeduplicateCoarseChars(&charCount);
	g_hwrCoarseCharCount = (uint16_t)charCount;
	baseCharCount = (uint16_t)charCount;
	tailDist = (uint16_t)(g_hwrCoarseMatches[baseCharCount - 1].distance + 1);

	for (idx = 0; idx < baseCharCount && charCount < HWR_MAX_COARSE_CANDIDATES; idx++) {
		const uint32_t *group;
		uint32_t groupLen;
		uint16_t sub;

		if (g_hwrCoarseMatches[idx].distance == 0xFFFFu)
			break;
		group = g_hwrCoarseGroupPtrs[g_hwrCoarseMatches[idx].templateIdx];
		groupLen = group[0];
		if (groupLen <= 1u)
			continue;
		for (sub = 2; sub <= groupLen && charCount < HWR_MAX_COARSE_CANDIDATES; sub++) {
			g_hwrCoarseMatches[charCount].templateIdx =
				g_hwrCoarseMatches[idx].templateIdx;
			g_hwrCoarseMatches[charCount].distance = tailDist;
			g_hwrCoarseCharPtrs[charCount] =
				(HwrDictCharEntry *)(uintptr_t)group[sub];
			charCount++;
			hwrDeduplicateCoarseChars(&charCount);
			g_hwrCoarseCharCount = (uint16_t)charCount;
		}
	}

	for (idx = 0; idx < (uint16_t)charCount; idx++)
		g_hwrCoarseCharCodes[idx] = g_hwrCoarseCharPtrs[idx]->code;
	while (idx < HWR_MAX_COARSE_CANDIDATES) {
		g_hwrCoarseCharCodes[idx] = 0;
		idx++;
	}
	return (uint16_t)charCount;
}

int hwrGetCoarsePriority(uint16_t code)
{
	uint8_t alnumRank = 0;
	uint16_t idx;

	for (idx = 0; idx < g_hwrCoarseCharCount; idx++) {
		uint16_t cur = g_hwrCoarseCharCodes[idx];
		int isCommon = (cur >= '0' && cur <= '9') ||
			       (cur >= 'A' && cur <= 'Z') ||
			       (cur >= 'a' && cur <= 'z') ||
			       (cur >= 0x3041u && cur <= 0x309Fu) ||
			       (cur >= 0x30A1u && cur <= 0x30FFu);

		if (isCommon) {
			if (cur == code)
				return alnumRank;
			alnumRank++;
		} else if (idx < HWR_MAX_COARSE_CANDIDATES && cur == code) {
			return (uint8_t)(idx + 10);
		}
	}
	return 255;
}

char *hwrStrcpy(char *dst, const char *src)
{
	char *out = dst;
	char ch;

	if (dst == NULL || src == NULL)
		return NULL;
	ch = *src;
	if (ch != '\0') {
		do {
			*out++ = ch;
			ch = *++src;
		} while (ch != '\0');
	}
	*out = '\0';
	return dst;
}

int hwrStrlen(const char *str)
{
	const char *ptr = str;
	char ch = *str;

	if (((uintptr_t)str & 3u) != 0) {
		const char *aligned = (const char *)(((uintptr_t)str & ~(uintptr_t)3u) + 4u);

		while (ch != '\0' && ptr < aligned) {
			ptr++;
			ch = *ptr;
		}
	}
	if (ch == '\0')
		return (int)(ptr - str);

	for (;;) {
		uint32_t word = *(const uint32_t *)ptr;

		if ((word & 0x000000FFu) == 0)
			return (int)(ptr - str);
		if ((word & 0x0000FF00u) == 0)
			return (int)(ptr - str + 1);
		if ((word & 0x00FF0000u) == 0)
			return (int)(ptr - str + 2);
		if ((word & 0xFF000000u) == 0)
			return (int)(ptr - str + 3);
		ptr += 4;
	}
}

int hwrStrcmp(const char *lhs, const char *rhs)
{
	if (lhs == NULL || rhs == NULL)
		return -1;
	while (*lhs == *rhs) {
		if (*lhs == '\0')
			return 0;
		lhs++;
		rhs++;
	}
	return (int)*lhs - (int)*rhs;
}

uint8_t *hwrMemset(uint8_t *dst, uint8_t value, uint32_t len)
{
	uint8_t *ptr = dst;

	if (dst == NULL)
		return NULL;
	if (((uintptr_t)dst & 3u) != 0) {
		uint8_t *aligned = (uint8_t *)(((uintptr_t)dst & ~(uintptr_t)3u) + 4u);

		while (len != 0 && ptr < aligned) {
			*ptr++ = value;
			len--;
		}
	}
	if (len != 0) {
		uint32_t fillWord = (uint32_t)value | ((uint32_t)value << 8) |
				    ((uint32_t)value << 16) | ((uint32_t)value << 24);
		uint32_t words = len >> 2;
		uint32_t rem = len & 3u;

		while (words-- != 0) {
			*(uint32_t *)ptr = fillWord;
			ptr += 4;
		}
		while (rem-- != 0)
			*ptr++ = value;
	}
	return dst;
}

int hwrMemcmp(const char *lhs, const char *rhs, int len)
{
	int diff = 0;

	while (len != 0) {
		diff = (int)*lhs - (int)*rhs;
		if (diff != 0)
			break;
		lhs++;
		rhs++;
		len--;
	}
	return diff;
}

void *hwrMemcpyFast(void *dst, const void *src, uint32_t len)
{
	uint8_t *out = (uint8_t *)dst;
	const uint8_t *in = (const uint8_t *)src;

	if (len >= 16u && ((((uintptr_t)in | (uintptr_t)out) & 3u) == 0)) {
		uint32_t *outW = (uint32_t *)out;
		const uint32_t *inW = (const uint32_t *)in;

		while (len >= 16u) {
			outW[0] = inW[0];
			outW[1] = inW[1];
			outW[2] = inW[2];
			outW[3] = inW[3];
			outW += 4;
			inW += 4;
			len -= 16u;
		}
		while (len >= 4u) {
			*outW++ = *inW++;
			len -= 4u;
		}
		out = (uint8_t *)outW;
		in = (const uint8_t *)inW;
	}
	while (len-- != 0)
		*out++ = *in++;
	return dst;
}

int hwrVerifyVersionSignature(void)
{
	uint8_t plain[24];
	uint8_t cipher[24];
	int idLen;
	uint8_t sum = 0;
	int idx;
	const uint8_t *keyRow;

	idLen = hwrStrlen(g_hwrVersionAuth.versionId);
	if (idLen <= 0)
		return -1;
	hwrMemset(cipher, 0, sizeof(cipher));
	hwrMemset(plain, 0, sizeof(plain));
	hwrMemcpyFast(plain, g_hwrVersionAuth.versionId, (uint32_t)idLen);
	for (idx = 0; idx < idLen; idx++)
		sum = (uint8_t)(sum + plain[idx]);

	keyRow = g_hwrVersionAuth.keyRows[sum & 0x0Fu];
	hwrMemset(plain, 0, sizeof(plain));
	hwrMemcpyFast(cipher, g_hwrVersionCipher, sizeof(cipher));
	for (idx = 0; idx < 12; idx++) {
		plain[idx] = cipher[2 * idx] ^ keyRow[idx];
		plain[idx + 12] = cipher[2 * idx + 1] ^ keyRow[idx];
	}
	idLen = hwrStrlen(g_hwrVersionAuth.versionId);
	if (hwrMemcmp((const char *)plain, g_hwrVersionAuth.versionId, idLen) != 0)
		return -1;
	return 0;
}

int hwrSearchCoarseTemplates(const uint16_t *hist, uint16_t strokeCount)
{
	uint8_t inputFeat[64];
	uint16_t limit = g_hwrCoarseMaxCandidates;
	uint16_t totalTemplates = g_hwrCoarseTemplateCount;
	uint16_t foundCount = 0;
	uint32_t maxAcceptDist = 6400u;
	uint16_t bin;
	uint16_t tmplIdx;

	g_hwrCoarseResultCount = 0;
	for (bin = 0; bin < 64u; bin++)
		inputFeat[bin] = (uint8_t)hist[bin];

	for (tmplIdx = 0; tmplIdx < totalTemplates; tmplIdx++) {
		const uint8_t *tmplFeat = &g_hwrCoarseFeatures[(uint32_t)tmplIdx << 6];
		uint32_t l1Dist = 0;

		if (strokeCount > g_hwrCoarseMinStrokes[tmplIdx])
			continue;

		if (foundCount == 0) {
			int allSame = 1;
			int check;

			for (check = 0; check < 63; check++) {
				if (tmplFeat[check] != tmplFeat[check + 1]) {
					allSame = 0;
					break;
				}
			}
			limit = allSame ? 80u : g_hwrCoarseMaxCandidates;
		}

		if ((g_hwrCfg.charCategoryMask & 0x8000u) != 0) {
			const uint32_t *group = g_hwrCoarseGroupPtrs[tmplIdx];
			uint32_t groupLen = group[0];
			uint32_t skipped = 0;
			uint32_t sub;

			for (sub = 0; sub < groupLen; sub++) {
				const HwrDictCharEntry *entry =
					(const HwrDictCharEntry *)(uintptr_t)group[sub + 1];
				uint8_t cat = entry->category;

				if (((1u << ((cat & 0x0Fu) - 1u)) & g_hwrCfg.charCategoryMask) == 0 &&
				    (cat & 0x80u) != 0) {
					skipped++;
				}
			}
			if (skipped == groupLen)
				continue;
		}

		for (bin = 0; bin < 64u; bin++) {
			int diff = (int16_t)((int)inputFeat[bin] - (int)tmplFeat[bin]);

			if (diff <= 0)
				diff = -diff;
			l1Dist += (uint32_t)diff;
		}

		if (l1Dist < maxAcceptDist) {
			uint16_t insPos = 0;

			while (insPos < foundCount &&
			       l1Dist > g_hwrCoarseMatches[insPos].distance) {
				insPos++;
			}

			if (foundCount >= limit) {
				int16_t mv;
				uint32_t nextLimit;

				for (mv = (int16_t)(foundCount - 1); mv > (int16_t)insPos; mv--)
					g_hwrCoarseMatches[mv] = g_hwrCoarseMatches[mv - 1];
				g_hwrCoarseMatches[insPos].templateIdx = tmplIdx;
				g_hwrCoarseMatches[insPos].distance = (uint16_t)l1Dist;
				nextLimit = (uint32_t)g_hwrCoarseMatches[0].distance + 600u;
				if (g_hwrCoarseMatches[foundCount - 1].distance < nextLimit)
					nextLimit = g_hwrCoarseMatches[foundCount - 1].distance;
				maxAcceptDist = nextLimit;
			} else {
				int16_t mv;

				for (mv = (int16_t)foundCount; mv > (int16_t)insPos; mv--)
					g_hwrCoarseMatches[mv] = g_hwrCoarseMatches[mv - 1];
				g_hwrCoarseMatches[insPos].templateIdx = tmplIdx;
				g_hwrCoarseMatches[insPos].distance = (uint16_t)l1Dist;
				foundCount++;
				g_hwrCoarseResultCount = foundCount;
				if (foundCount == limit)
					maxAcceptDist = g_hwrCoarseMatches[foundCount - 1].distance;
			}
		}
	}

	if (foundCount > 5u) {
		uint16_t keep = 5u;

		while (keep < foundCount && keep < limit && keep < totalTemplates &&
		       g_hwrCoarseMatches[keep].distance <= g_hwrCoarseMatches[0].distance + 500u) {
			keep++;
		}
		g_hwrCoarseResultCount = keep;
		return keep;
	}
	return foundCount;
}

void hwrComputeGridBoundaries(const uint16_t *projection, uint8_t *bounds)
{
	uint8_t first = 0;
	uint8_t last = 127u;

	bounds[0] = 0;
	bounds[1] = 0;
	bounds[2] = 0;
	bounds[3] = 0;
	bounds[4] = 0;

	while (first < 128u && projection[first] == 0)
		first++;
	while (last > first && projection[last] == 0)
		last--;
	if (last > first)
		last++;

	if (last > first) {
		int16_t span = (int16_t)(last - first + 1);
		int16_t pad = (span < 11) ? (int16_t)((11 - span) / 2) : 0;
		int16_t lo = (int16_t)(first - pad - 1);
		int16_t hi = (int16_t)(last + pad + 1);
		uint8_t startBound = (lo >= 0) ? (uint8_t)lo : 0u;
		uint8_t endBound = (hi < 128) ? (uint8_t)hi : 127u;
		uint32_t totalWeight = 0;
		uint32_t acc = 0;
		uint16_t pos;
		uint32_t quarterWeight, halfWeight, threeQuarterWeight;
		uint16_t idx;

		bounds[0] = startBound;
		bounds[4] = endBound;
		for (pos = startBound; pos < endBound; pos++)
			totalWeight += (uint32_t)projection[pos] + 3u;

		quarterWeight = totalWeight >> 2;
		halfWeight = totalWeight >> 1;
		threeQuarterWeight = (3u * totalWeight) >> 2;

		pos = startBound;
		bounds[1] = startBound;
		while (pos < endBound && acc <= quarterWeight) {
			acc += (uint32_t)projection[pos] + 3u;
			pos++;
		}
		if (pos < endBound)
			bounds[1] = (uint8_t)pos;
		else
			pos = bounds[1];

		bounds[2] = (uint8_t)pos;
		while (pos < endBound && acc <= halfWeight) {
			acc += (uint32_t)projection[pos] + 3u;
			pos++;
		}
		if (pos < endBound)
			bounds[2] = (uint8_t)pos;
		else
			pos = bounds[2];

		bounds[3] = (uint8_t)pos;
		while (pos < endBound && acc <= threeQuarterWeight) {
			acc += (uint32_t)projection[pos] + 3u;
			pos++;
		}
		if (pos < endBound)
			bounds[3] = (uint8_t)pos;

		for (idx = 4; idx != 0; idx--) {
			if (bounds[idx] == bounds[idx - 1] && bounds[idx - 1] != 0)
				bounds[idx - 1]--;
		}
	}
}

void hwrBuildGridMaps(uint16_t pointCount, const uint8_t *pointBytes)
{
	uint16_t projX[128];
	uint16_t projY[128];
	uint16_t smooth[128];
	uint16_t idx;
	uint16_t segCount = (uint16_t)(pointCount - 1);

	for (idx = 0; idx < 128u; idx++) {
		projX[idx] = 0;
		projY[idx] = 0;
	}

	for (idx = 0; idx < segCount; idx++) {
		const uint8_t *cur = &pointBytes[2 * idx];
		int16_t xCur = cur[0] & 0x7Fu;
		int16_t yCur = cur[1];
		int16_t xEnd = cur[2] & 0x7Fu;
		int16_t yEnd = cur[3];
		int16_t dx = (int16_t)(xEnd - xCur);
		int16_t dy = (int16_t)(yEnd - yCur);
		int16_t stepX = (dx >= 0) ? 1 : -1;
		int16_t stepY = (dy >= 0) ? 1 : -1;

		if (xCur == xEnd) {
			while (yCur != yEnd) {
				projY[yCur]++;
				yCur = (int16_t)(yCur + stepY);
				projX[xCur]++;
			}
		} else if (dy == 0) {
			while (xCur != xEnd) {
				projX[xCur]++;
				xCur = (int16_t)(xCur + stepX);
				projY[yCur]++;
			}
		} else {
			int16_t adx = (dx > 0) ? dx : (int16_t)(-dx);
			int16_t ady = (dy > 0) ? dy : (int16_t)(-dy);
			int16_t twoAdx = (int16_t)(2 * adx);
			int16_t twoAdy = (int16_t)(2 * ady);

			if (twoAdx <= twoAdy) {
				int16_t err = (int16_t)(twoAdx - ady);

				while (yCur != yEnd) {
					if (err > 0 || (err == 0 && stepY == 1)) {
						xCur = (int16_t)(xCur + stepX);
						err = (int16_t)(err - twoAdy);
					}
					projX[xCur]++;
					yCur = (int16_t)(yCur + stepY);
					projY[yCur]++;
					err = (int16_t)(err + twoAdx);
				}
			} else {
				int16_t err = (int16_t)(twoAdy - adx);

				while (xCur != xEnd) {
					if (err > 0 || (err == 0 && stepX == 1)) {
						yCur = (int16_t)(yCur + stepY);
						err = (int16_t)(err - twoAdx);
					}
					xCur = (int16_t)(xCur + stepX);
					projX[xCur]++;
					projY[yCur]++;
					err = (int16_t)(err + twoAdy);
				}
			}
		}
	}

	smooth[0] = (uint16_t)(projX[0] + projX[1]);
	for (idx = 1; idx < 127u; idx++)
		smooth[idx] = (uint16_t)(projX[idx - 1] + projX[idx] + projX[idx + 1]);
	smooth[127] = (uint16_t)(projX[126] + projX[127]);
	hwrComputeGridBoundaries(smooth, g_hwrGridBoundsX);

	smooth[0] = (uint16_t)(projY[0] + projY[1]);
	for (idx = 1; idx < 127u; idx++)
		smooth[idx] = (uint16_t)(projY[idx - 1] + projY[idx] + projY[idx + 1]);
	smooth[127] = (uint16_t)(2u * projY[127]);
	hwrComputeGridBoundaries(smooth, g_hwrGridBoundsY);

	for (idx = 0; idx < 32u; idx++) {
		g_hwrWork.grid.mapX[idx] = 0;
		g_hwrWork.grid.mapY[idx] = 0;
		g_hwrWork.grid.mapX[160 + idx] = 12;
		g_hwrWork.grid.mapY[160 + idx] = 48;
	}
	for (idx = 0; idx < 128u; idx++) {
		int16_t cellX;
		int16_t cellY;

		if (idx < g_hwrGridBoundsX[1])
			cellX = 0;
		else if (idx < g_hwrGridBoundsX[2])
			cellX = 4;
		else if (idx < g_hwrGridBoundsX[3])
			cellX = 8;
		else
			cellX = 12;
		g_hwrWork.grid.mapX[32 + idx] = cellX;

		if (idx < g_hwrGridBoundsY[1])
			cellY = 0;
		else if (idx < g_hwrGridBoundsY[2])
			cellY = 16;
		else if (idx < g_hwrGridBoundsY[3])
			cellY = 32;
		else
			cellY = 48;
		g_hwrWork.grid.mapY[32 + idx] = cellY;
	}
}

void hwrExtractDirectionalFeature(uint16_t pointCount, const uint8_t *pointBytes)
{
	uint16_t segCount = (uint16_t)(pointCount - 1);
	int16_t radius;
	int16_t kernelWidth;
	uint16_t idx;
	uint8_t row;

	hwrBuildGridMaps(pointCount, pointBytes);
	for (idx = 0; idx < 64u; idx++)
		g_hwrWork.grid.featureHist[idx] = 0;

	{
		int16_t radY = (int16_t)((g_hwrGridBoundsY[4] - g_hwrGridBoundsY[0] + 1) / 10);
		int16_t radX = (int16_t)((g_hwrGridBoundsX[4] - g_hwrGridBoundsX[0] + 1) / 10);

		radius = (radX < radY) ? radX : radY;
		kernelWidth = (int16_t)(2 * radius + 1);
	}

	for (idx = 0; idx < segCount; idx++) {
		const uint8_t *cur = &pointBytes[2 * idx];
		uint8_t xCur = cur[0] & 0x7Fu;
		uint8_t yCur = cur[1];
		uint8_t xEnd = cur[2] & 0x7Fu;
		uint8_t yEnd = cur[3];
		int16_t dx = (int16_t)((int)xEnd - (int)xCur);
		int16_t dy = (int16_t)((int)yEnd - (int)yCur);
		int16_t stepX = (dx >= 0) ? 1 : -1;
		int16_t stepY = (dy >= 0) ? 1 : -1;
		int diagDir = (stepX == stepY) ? 3 : 2;
		int16_t off;

		if (xCur == xEnd) {
			while (yCur != yEnd) {
				int16_t yBin = (int16_t)(g_hwrWork.grid.mapY[32 + yCur] + 1);

				for (off = (int16_t)(-radius); off <= radius; off++)
					g_hwrWork.grid.featureHist[g_hwrWork.grid.mapX[32 + xCur + off] + yBin]++;
				yCur = (uint8_t)(yCur + stepY);
			}
		} else if (yCur == yEnd) {
			while (xCur != xEnd) {
				int16_t xBin = g_hwrWork.grid.mapX[32 + xCur];

				for (off = (int16_t)(-radius); off <= radius; off++)
					g_hwrWork.grid.featureHist[xBin + g_hwrWork.grid.mapY[32 + yCur + off]]++;
				xCur = (uint8_t)(xCur + stepX);
			}
		} else {
			int16_t adx = (dx > 0) ? dx : (int16_t)(-dx);
			int16_t ady = (dy > 0) ? dy : (int16_t)(-dy);
			int16_t twoAdx = (int16_t)(2 * adx);
			int16_t twoAdy = (int16_t)(2 * ady);

			if (twoAdx <= twoAdy) {
				int16_t err = (int16_t)(twoAdx - ady);

				while (yCur != yEnd) {
					uint8_t nextY = (uint8_t)(yCur + stepY);

					if (err > 0 || (err == 0 && stepY == 1)) {
						err = (int16_t)(err - twoAdy + twoAdx);
						xCur = (uint8_t)(xCur + stepX);
						if (diagDir == 2) {
							for (off = (int16_t)(-radius); off <= radius; off++) {
								g_hwrWork.grid.featureHist[
									g_hwrWork.grid.mapX[32 + xCur + off] +
									g_hwrWork.grid.mapY[32 + nextY + off] + 2]++;
							}
						} else {
							for (off = (int16_t)(-radius); off <= radius; off++) {
								g_hwrWork.grid.featureHist[
									g_hwrWork.grid.mapX[32 + xCur + off] +
									g_hwrWork.grid.mapY[32 + nextY - off] + 3]++;
							}
						}
					} else {
						int16_t yBin = (int16_t)(g_hwrWork.grid.mapY[32 + nextY] + 1);

						err = (int16_t)(err + twoAdx);
						for (off = (int16_t)(-radius); off <= radius; off++) {
							g_hwrWork.grid.featureHist[
								g_hwrWork.grid.mapX[32 + xCur + off] + yBin]++;
						}
					}
					yCur = nextY;
				}
			} else {
				int16_t err = (int16_t)(twoAdy - adx);

				while (xCur != xEnd) {
					uint8_t nextX = (uint8_t)(xCur + stepX);

					if (err > 0 || (err == 0 && stepX == 1)) {
						yCur = (uint8_t)(yCur + stepY);
						err = (int16_t)(err - twoAdx + twoAdy);
						if (diagDir == 2) {
							for (off = (int16_t)(-radius); off <= radius; off++) {
								g_hwrWork.grid.featureHist[
									g_hwrWork.grid.mapX[32 + nextX + off] +
									g_hwrWork.grid.mapY[32 + yCur + off] + 2]++;
							}
						} else {
							for (off = (int16_t)(-radius); off <= radius; off++) {
								g_hwrWork.grid.featureHist[
									g_hwrWork.grid.mapX[32 + nextX + off] +
									g_hwrWork.grid.mapY[32 + yCur - off] + 3]++;
							}
						}
					} else {
						int16_t xBin = g_hwrWork.grid.mapX[32 + nextX];

						err = (int16_t)(err + twoAdy);
						for (off = (int16_t)(-radius); off <= radius; off++) {
							g_hwrWork.grid.featureHist[
								xBin + g_hwrWork.grid.mapY[32 + yCur + off]]++;
						}
					}
					xCur = nextX;
				}
			}
		}
	}

	for (idx = 0; idx < 64u; idx++) {
		if ((int16_t)g_hwrWork.grid.featureHist[idx] >= 1024)
			g_hwrWork.grid.featureHist[idx] = 1023u;
	}

	for (row = 0; row < 4u; row++) {
		uint8_t col;

		for (col = 0; col < 4u; col++) {
			uint16_t *cell = &g_hwrWork.grid.featureHist[row * 16 + col * 4];
			int16_t cellW = (int16_t)(g_hwrGridBoundsX[col + 1] - g_hwrGridBoundsX[col]);
			int16_t cellH = (int16_t)(g_hwrGridBoundsY[row + 1] - g_hwrGridBoundsY[row]);
			int16_t minDim = (cellW < cellH) ? cellW : cellH;

			if (cellW <= 0)
				cell[0] = 0;
			else
				cell[0] = (uint16_t)(((int)cell[0] << 6) / (kernelWidth * cellW));

			if (cellH <= 0)
				cell[1] = 0;
			else
				cell[1] = (uint16_t)(((int)cell[1] << 6) / (kernelWidth * cellH));

			if (minDim <= 0) {
				cell[2] = 0;
				cell[3] = 0;
			} else {
				int denom = minDim * kernelWidth;

				cell[2] = (uint16_t)(((int)cell[2] << 6) / denom);
				cell[3] = (uint16_t)(((int)cell[3] << 6) / denom);
			}
		}
	}

	for (idx = 0; idx < 64u; idx++) {
		if ((int16_t)g_hwrWork.grid.featureHist[idx] > 255)
			g_hwrWork.grid.featureHist[idx] = 255u;
	}
}

void hwrBuildCoarseInputFeature(void)
{
	uint16_t ptCount = 0;
	uint16_t strokeIdx;

	if (g_hwrRawByteCount <= 256u) {
		hwrExtractDirectionalFeature((uint16_t)(g_hwrRawByteCount >> 1), g_hwrDpPathBuf);
		return;
	}

	for (strokeIdx = 0; strokeIdx < g_hwrStrokeBuf->strokeCount; strokeIdx++) {
		uint16_t segIdx = g_hwrStrokeBuf->strokeFirstSeg[strokeIdx];
		uint16_t segTotal = g_hwrStrokeBuf->strokeSegCount[strokeIdx];
		uint16_t step;

		for (step = 0; step < segTotal; step++) {
			const HwrSegment *seg = &g_hwrStrokeBuf->segments[segIdx];
			HwrPoint startPt = g_hwrStrokeBuf->points[seg->startPt];

			g_hwrResampledPoints[2 * ptCount] = startPt.x | 0x80u;
			g_hwrResampledPoints[2 * ptCount + 1] = startPt.y;
			ptCount++;
			if ((uint16_t)(step + 1) == segTotal) {
				HwrPoint endPt = g_hwrStrokeBuf->points[seg->endPt];

				g_hwrResampledPoints[2 * ptCount] = endPt.x;
				g_hwrResampledPoints[2 * ptCount + 1] = endPt.y;
				ptCount++;
			} else {
				segIdx = seg->nextSeg;
			}
		}
	}
	hwrExtractDirectionalFeature(ptCount, g_hwrResampledPoints);
}
