/*
 * The AVC encoder's macroblock helpers: the bS a macroblock's edges are
 * deblocked with (0x866f60-0x867060)
 */
#include "avcencstream.h"

/*
 * 0x866f60: an intra macroblock's: 4 on its edges (but on the picture's),
 * 3 inside
 */
void avcencStrengthsIntra(AvcencChunkInfo *info)
{
	AvcencStrengths *strengths = (AvcencStrengths *)info->strengths;
	uint32_t left = info->column != 0, top = info->row != 0, edge, group;

	for (edge = 0; edge < 4; edge++)
		for (group = 0; group < 4; group++) {
			strengths->edges[STRENGTHS_VERTICAL][edge][group] =
				edge ? 3 : left ? 4 : 0;
			strengths->edges[STRENGTHS_HORIZONTAL][edge][group] =
				edge ? 3 : top ? 4 : 0;
		}
}

/*
 * 0x866ff4: an inter macroblock's where coefficients are: by 4x4 block (z
 * order), it or the one on its left, or above it, has some; the edges next
 * to an intra macroblock
 */
static void strengthsCoded(AvcencChunkInfo *info, AvcencStrengthLanes *lanes)
{
	const AvcencMacroblock *left = (const AvcencMacroblock *)info->left;
	const AvcencMacroblock *above = (const AvcencMacroblock *)info->above;
	uint32_t coded = info->coded, leftIntra, topIntra, vertical, horizontal;

	leftIntra = left->type & MB_INTRA ? 15 : 0;
	topIntra = above->type & MB_INTRA ? 15 : 0;
	vertical = ((info->leftBlocks >> 5) & 0x505) | ((coded << 3) & 0x5050) |
		((coded << 1) & 0xaaaa) | coded;
	horizontal = ((info->topBlocks >> 10) & 0x33) | ((coded << 6) & 0x3300) |
		((coded << 2) & 0xcccc) | coded;
	avcencStrengthsCoded(lanes, vertical, horizontal, leftIntra, topIntra);
}

/*
 * 0x866f76: an inter macroblock's: its coefficients', then its vectors'
 * over them; none on the picture's edges
 */
void avcencStrengthsInter(AvcencChunkInfo *info)
{
	AvcencStrengths *strengths = (AvcencStrengths *)info->strengths;
	AvcencStrengthLanes lanes;
	uint32_t edge, group;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, strengths);
	strengthsCoded(info, &lanes);
	avcencStrengthsVectors(&lanes,
		(const int16_t (*)[2])((const AvcencMacroblock *)info->current)->vectors,
		(const int16_t (*)[2])((const AvcencMacroblock *)info->left)->vectors,
		(const int16_t (*)[2])((const AvcencMacroblock *)info->above)->vectors);
	if (!info->column)
		for (group = 0; group < 4; group++)
			lanes.edges[0][group] = 0;
	if (!info->row)
		for (group = 0; group < 4; group++)
			lanes.edges[2][group] = 0;
	for (edge = 0; edge < 4; edge++)
		for (group = 0; group < 4; group++) {
			strengths->edges[STRENGTHS_VERTICAL][edge][group] =
				lanes.edges[edge >> 1][(edge & 1) * 4 + group];
			strengths->edges[STRENGTHS_HORIZONTAL][edge][group] =
				lanes.edges[2 + (edge >> 1)][(edge & 1) * 4 + group];
		}
}
