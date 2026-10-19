/*
 * The parameter sets, both codecs' (PU1's, and FW2's codec). PU0's bitstream unit
 * parses each SPS and PPS and writes the results to the slice ring (pu0/decoder.c
 * spsParsed, ppsParsed; FW2's parsers alike); the codec reads them in through the
 * ring's segments (readParsed) and keeps them by id.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#else
#include "codec.h"
#endif

/* the parse results' first words, as PU0 writes them */
#define SPS_SCALING_MATRIX	(1u << 31)	/* word 0 */
#define SPS_REF_FRAME_OFFSETS	(1u << 30)
#define SPS_PROFILE_SHIFT	22
#define SPS_CONSTRAINT_SET1	20	/* constraint_set0 to 3 from bit 21 down */
#define SPS_LEVEL_SHIFT		10
#define SPS_ID_SHIFT		2
#define SPS_CHROMA_FORMAT	0x3
#define SPS_FRAME_CROPPING	(1u << 1)	/* word 4 */

#define PPS_SCALING_MATRIX	(1u << 31)	/* word 0 */
#define PPS_ID_SHIFT		23

#define CROP_MASK		0x7ff

/* the level from which g_levelAtLeast3 is set */
#define LEVEL_3			30

#ifdef VIP_FW2
/* FW2 reads the data in by the read stream's DMA list (g_streamList) */
uint32_t g_parsed[VIP_PARSED_WORDS] VIP_SLICE_BUFFER(parsed);

VipSps *VIP_GLOBAL g_currentSps;	/* in g_sps, g_pps: command.c */
VipPps *VIP_GLOBAL g_currentPps;

int8_t g_spsTaken VIP_GLOBAL;
int8_t g_levelAtLeast3 VIP_GLOBAL;
#else
uint32_t g_parsed[VIP_PARSED_WORDS] VIP_SLICE_BUFFER(parsed);

VipSps g_sps[VIP_SPS_COUNT];
VipPps g_pps[VIP_PPS_COUNT];
VipSps *g_currentSps;
VipPps *g_currentPps;

int8_t g_spsTaken;		/* a new SPS: the debugger is told */
int8_t g_ppsTaken;		/* a new PPS */
int8_t g_levelAtLeast3;
#endif

#ifdef VIP_FW2
/*
 * Read the next VIP_PARSED_WORDS words of the record's data into g_parsed, from
 * g_dataStart on, through the slice ring's segments, and move g_dataStart on.
 */
void readParsed(void)
{
	volatile uint32_t done = 0;
	VipRingSegment *segment = g_sliceSegments;
	uint32_t cursor = g_dataStart, filled = 0, lists = 0, length, end, i;

	for (i = 0; i < g_sliceSegmentCount; i++, segment++) {
		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		length = end - cursor;
		if (length < sizeof(g_parsed) - filled) {
			if (length & 7)
				length += 8 - (length & 7);
		} else {
			length = sizeof(g_parsed) - filled;
		}
		g_streamList[lists].external = cursor;
		g_streamList[lists].localLength = ((uint32_t)g_parsed + filled) << 16 | length;
		lists++;
		filled += length;
		cursor += length;
		if (cursor == end && i + 1 < g_sliceSegmentCount)
			cursor = segment[1].start;
		if (filled >= sizeof(g_parsed))
			break;
	}
	g_dataStart = cursor;
	queueListDma(VIP_DMA_LIST | VIP_DMA_READ,
		(uint32_t)g_streamList << 16 | lists * sizeof(VipDmaDescriptor), 0, 0, NULL,
		&done);
	while (done == 1)
		;
}
#else
/*
 * Read the next VIP_PARSED_WORDS words of the record's data into g_parsed, from the
 * read stream's cursor on, through its segments.
 */
void readParsed(void)
{
	VipReadStream *stream = &g_readStream;
	VipRingSegment *segment;
	uint32_t filled = 0, length, cursor, end, i;

	for (i = 0; i < stream->segmentCount; i++) {
		segment = &stream->segments[i];
		cursor = stream->cursor;
		if (cursor < segment->start)
			continue;
		end = segment->start + segment->size;
		length = sizeof(g_parsed) - filled;
		if (end - cursor < length) {
			length = end - cursor;
			if (length & 7)
				length += 8 - (length & 7);
		}
		readDram(cursor, (uint32_t)&g_parsed[filled / 4], length);
		filled += length;
		/* the cursor moves on by all that is read so far, not by this length */
		stream->cursor = cursor + (filled & ~3u);
		if (stream->cursor == segment->start + segment->size &&
		    i + 1 < stream->segmentCount)
			stream->cursor = segment[1].start;
		if (filled >= sizeof(g_parsed))
			break;
	}
}
#endif

/* A VIP_PARSED_SPS record: keep the SPS by its id. */
void takeSps(void)
{
	VipSps *sps;
	uint32_t word0, id, refFrameOffsets, word, i;

	g_spsTaken = 1;
	readParsed();
	word0 = g_parsed[0];
	id = word0 >> SPS_ID_SHIFT & 0xff;
	sps = &g_sps[id < VIP_SPS_COUNT ? id : 0];
	g_currentSps = sps;
	sps->scalingMatrixPresent = word0 >> 31 & 1;
	sps->profileIdc = word0 >> SPS_PROFILE_SHIFT;
	refFrameOffsets = word0 >> 30 & 1;
	sps->constraintSet1 = word0 >> SPS_CONSTRAINT_SET1 & 1;
	sps->levelIdc = word0 >> SPS_LEVEL_SHIFT;
	g_levelAtLeast3 = sps->levelIdc >= LEVEL_3;
	if ((int8_t)sps->profileIdc != VIP_PROFILE_HIGH)
		sps->chroma = 1;
	else
		sps->chroma = (word0 & SPS_CHROMA_FORMAT) != 0;

	sps->log2MaxFrameNumMinus4 = g_parsed[1] >> 28 & 0xf;
	sps->picOrderCntType = g_parsed[1] >> 26 & 3;
	sps->log2MaxPicOrderCntLsbMinus4 = g_parsed[1] >> 22 & 0xf;
	sps->deltaPicOrderAlwaysZero = g_parsed[1] >> 21 & 1;
	sps->numRefFramesInPicOrderCntCycle = g_parsed[1] >> 13 & 0xff;
	sps->offsetForNonRefPic = g_parsed[2];
	sps->offsetForTopToBottomField = g_parsed[3];
	sps->maxNumRefFrames = g_parsed[4] >> 27 & 0x1f;
	sps->picWidthInMbsMinus1 = g_parsed[4] >> 16;
	sps->picHeightInMapUnitsMinus1 = g_parsed[4] >> 8;
	sps->frameMbsOnly = g_parsed[4] >> 4 & 1;
	sps->mbAdaptiveFrameField = sps->frameMbsOnly ? 0 : g_parsed[4] >> 3 & 1;
	sps->direct8x8Inference = g_parsed[4] >> 2 & 1;
	if (g_parsed[4] & SPS_FRAME_CROPPING) {
		sps->frameCropLeft = g_parsed[5] >> 21 & CROP_MASK;
		sps->frameCropRight = g_parsed[5] >> 5 & CROP_MASK;
		sps->frameCropTop = g_parsed[6] >> 21 & CROP_MASK;
		sps->frameCropBottom = g_parsed[6] >> 5 & CROP_MASK;
	} else {
		sps->frameCropLeft = 0;
		sps->frameCropRight = 0;
		sps->frameCropTop = 0;
		sps->frameCropBottom = 0;
	}

	word = SPS_WORDS;
	if (sps->scalingMatrixPresent)
		word = takeScalingMatrix(&sps->scaling, sps->scalingListPresent, word);
	if (refFrameOffsets) {
		for (i = 0; i < sps->numRefFramesInPicOrderCntCycle; i++) {
			sps->offsetForRefFrame[i] = g_parsed[word++];
			if (word == VIP_PARSED_WORDS) {
				readParsed();
				word = 0;
			}
		}
	}
	sps->valid = 1;
	for (i = 0; i < VIP_DISPLAY_RECTANGLES; i++) {
		g_frameInfo.display[i].width = 0;
		g_frameInfo.display[i].height = 0;
	}
	g_timeStarted = 0;
}

/* A VIP_PARSED_PPS record: keep the PPS by its id. */
void takePps(void)
{
	VipPps *pps;
	uint32_t word0, word1;

#ifndef VIP_FW2
	g_ppsTaken = 1;
#endif
	readParsed();
	word0 = g_parsed[0];
	pps = &g_pps[word0 >> PPS_ID_SHIFT & 0xff];
	g_currentPps = pps;
	pps->scalingMatrixPresent = word0 >> 31 & 1;
	pps->spsId = word0 >> 15;
	if (pps->spsId >= VIP_SPS_COUNT)
		pps->spsId = 0;
	pps->entropyCodingMode = word0 >> 14 & 1;
	pps->bottomFieldPicOrderInFramePresent = word0 >> 13 & 1;
	pps->numRefIdxL0DefaultActiveMinus1 = word0 >> 8 & 0x1f;
	pps->numRefIdxL1DefaultActiveMinus1 = word0 >> 3 & 0x1f;
	word1 = g_parsed[1];
	pps->weightedPred = word1 >> 31 & 1;
	pps->weightedBipredIdc = word1 >> 29 & 3;
	pps->picInitQpMinus26 = word1 >> 23 & 0x3f;
	pps->chromaQpIndexOffset = word1 >> 18 & 0x1f;
	pps->deblockingFilterControlPresent = word1 >> 17 & 1;
	pps->transform8x8Mode = word1 >> 16 & 1;
	pps->constrainedIntraPred = word1 >> 15 & 1;
	pps->secondChromaQpIndexOffset = word1 >> 10 & 0x1f;
	if (pps->scalingMatrixPresent)
		takeScalingMatrix(&pps->scaling, pps->scalingListPresent, PPS_WORDS);
	pps->valid = 1;
}
