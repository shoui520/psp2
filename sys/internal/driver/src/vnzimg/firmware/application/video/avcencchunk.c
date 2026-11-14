/*
 * The AVC encoder's chunk stages (0x83e260-): the jobs that take a chunk of a
 * row of macroblocks once the motion jobs decided it. The first deblocks
 * the chunk encoded before it in the row (its bS, then the luma's and the
 * chroma's edges in the picture's samples) and writes its macroblocks'
 * records into the output.
 */
#include "avcencstream.h"

#define SLICE_I			2

static void deblockSetup(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info);
static void macroblockRecord(AvcencChunkInfo *info);
static void macroblockChecked(AvcencChunkInfo *info);
static void macroblockNext(AvcencChunkInfo *info);
static void codedClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info);
static void lumaOpen(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info);
static void lumaStart(AvcencChunkInfo *info);
static void lumaNext(AvcencChunkInfo *info);
static void lumaClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info);
static void chromaOpen(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info);
static void chromaStart(AvcencChunkInfo *info);
static void chromaNext(AvcencChunkInfo *info);
static void deblockClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info);
static void deblockLuma(AvcencChunkInfo *info);
static void deblockChroma(AvcencChunkInfo *info);

/*
 * 0x83e260: the first stage: the last chunk's bS and macroblock records,
 * its luma and chroma deblocked; the row's progress counted, the third
 * stage signalled when the chunk ends the row and the others are done
 */
void avcencJobChunkStage1(uint32_t pictureAddress, uint32_t slotAddress, uint32_t unused,
	uint32_t unused2)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	uint32_t mbs, i, rewind, bit, chunk, slotMbs, slotFirstMb, last, signal, ready;
	AvcencFrameRecord *frameRecord;
	AvcencChunkInfo *info;
	AvcencOutputSlot *slot;
	VkVThreadContext *context;

	slot = (AvcencOutputSlot *)vnzPublicOpenProtected(slotAddress, 64);
	info = (AvcencChunkInfo *)vnzPublicOpenPrivate(slot->outputInfo +
		core * CHUNK_INFO_SIZE, CHUNK_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, CHUNK_INFO_SIZE);
	deblockSetup(picture, slot, info);
	mbs = info->mbs;
	for (i = 0; i < mbs; i++) {
		macroblockRecord(info);
		macroblockChecked(info);
		if ((info->type & MB_KIND) == MB_INTRA)
			avcencStrengthsIntra(info);
		else
			avcencStrengthsInter(info);
		macroblockNext(info);
	}
	codedClose(picture, slot, info);
	rewind = mbs * 64;
	info->strengths -= rewind;
	info->current -= rewind;
	lumaOpen(picture, slot, info);
	for (i = 0; i < mbs; i++) {
		lumaStart(info);
		deblockLuma(info);
		lumaNext(info);
	}
	lumaClose(picture, slot, info);
	info->strengths -= rewind;
	info->current -= rewind;
	chromaOpen(picture, slot, info);
	for (i = 0; i < mbs; i++) {
		chromaStart(info);
		deblockChroma(info);
		chromaNext(info);
	}
	deblockClose(picture, slot, info);

	frameRecord = (AvcencFrameRecord *)picture->frameRecord;
	bit = 1 << (slot->row & 31);
	chunk = slot->chunk;
	last = (uint32_t)(info->firstMb + info->mbs) == picture->widthInMbs;
	slotMbs = slot->mbs;
	slotFirstMb = slot->firstMb;
	vnzCachePurge((uint32_t)info, CHUNK_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, CHUNK_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slot, 64);
	if (last) {
		vnzSpinLock(picture->lock);
		frameRecord->rowsEncoded |= bit;
		vnzSpinUnlock(picture->lock);
		return;
	}
	vnzSpinLock(picture->lock);
	ready = frameRecord->progress[1] & bit;
	if (slotFirstMb + slotMbs == picture->widthInMbs) {
		if (ready && (frameRecord->progress[2] & bit)) {
			frameRecord->progress[1] &= ~bit;
			frameRecord->progress[2] &= ~bit;
			signal = 1;
		} else {
			frameRecord->progress[0] |= bit;
			signal = 0;
		}
		vnzSpinUnlock(picture->lock);
		if (signal) {
			context = vkVThreadContextLock(picture->outputContext);
			vkVThreadJobSignal(context, chunk + picture->chunkSteps * 2);
			vkVThreadContextUnlock(context);
		}
		return;
	}
	if (ready && (frameRecord->progress[2] & bit)) {
		frameRecord->progress[2] &= ~bit;
		frameRecord->progress[1] &= ~bit;
		frameRecord->rowsEncoded |= bit;
	} else {
		frameRecord->progress[0] |= bit;
	}
	vnzSpinUnlock(picture->lock);
}

/*
 * 0x83e48c: the macroblock's record in the output: mb_type (by the slice's
 * type, Intra16x16's from its prediction and coded_block_pattern), the
 * pattern and the coded blocks, from its coded blocks' records
 */
static void macroblockRecord(AvcencChunkInfo *info)
{
	const AvcencLumaCoded *luma = (const AvcencLumaCoded *)info->lumaCoded;
	const AvcencChromaCoded *chroma = (const AvcencChromaCoded *)info->chromaCoded;
	VideoAvcMacroblock *macroblock = (VideoAvcMacroblock *)info->macroblock;
	uint32_t type = ((const AvcencMacroblock *)info->current)->type, coded, i;
	uint8_t mbType, offset;

	info->type = type;
	coded = luma->blocks | luma->dc << 24 | chroma->ac << 16 | chroma->dc << 25;
	info->coded = coded;
	info->leftBlocks = luma->leftBlocks;
	info->topBlocks = luma->topBlocks;
	macroblock->cbp = chroma->cbp << 4 | luma->cbp;
	for (i = 0; i < 4; i++)
		macroblock->subTypes[i] = 0;
	macroblock->reserved = 0;
	macroblock->reserved2[0] = 0;
	macroblock->qpDelta = 0;
	macroblock->coded = coded;
	macroblock->skip = (type >> MB_SKIP_SHIFT) & 1;
	if (type & MB_INTRA) {
		if (((type >> 6) & 0xb) == 8) {
			mbType = 0;			/* I_NxN */
		} else {
			mbType = (chroma->cbp << 2 | ((type >> 12) & 3)) + 1;
			mbType += luma->cbp ? 12 : 0;
		}
		offset = (info->sliceType & 7) == SLICE_I ? 0 : 5;
	} else {
		mbType = ~type & 3;
		offset = (type >> 10) & 1;
	}
	macroblock->type = mbType + offset;
}

/*
 * 0x83e56c: the stage's records: the row's deblocking record (the chunk to
 * deblock is the one it kept, this one kept for the next), the chunk's
 * macroblock records and neighbours, its coded blocks, bS and output records
 */
static void deblockSetup(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info)
{
	uint32_t row = slot->row, chunkWidth = picture->chunkWidth, line = row & 15;
	uint32_t firstMb, mbs, parity, current, coded, strengths, macroblocks;
	const AvcencOutput *output;
	AvcencDeblockRow *deblockRow;

	info->row = row;
	deblockRow = (AvcencDeblockRow *)vnzPublicOpenPrivate(picture->command[4] + line * 128,
		128);
	info->deblockRow = (uint32_t)deblockRow;
	parity = deblockRow->parity;
	firstMb = deblockRow->firstMb;
	mbs = deblockRow->mbs;
	info->column = firstMb;
	info->firstMb = firstMb;
	info->mbs = mbs;
	deblockRow->firstMb = slot->firstMb;
	deblockRow->mbs = slot->mbs;
	deblockRow->parity = slot->parity;

	current = picture->macroblocks + picture->widthInMbs * row * 64 + firstMb * 64;
	info->current = current;
	info->left = current;
	info->above = current;
	if (firstMb)
		info->left = current - 64;
	if (row)
		info->above -= picture->widthInMbs * 64;

	coded = picture->lumaCoded + line * 64;
	if (parity)
		coded += 1024;
	info->lumaCoded = vnzPublicOpenProtected(coded, 64);
	coded = picture->chromaCoded + line * 64;
	if (parity)
		coded += 1024;
	info->chromaCoded = vnzPublicOpenProtected(coded, 64);

	strengths = vnzPublicOpenPrivate(picture->strengths + line * chunkWidth * 64,
		chunkWidth * 64);
	info->strengths = strengths + (chunkWidth - mbs) * 64;

	output = (const AvcencOutput *)slot->output;
	macroblocks = vnzPublicOpenPrivate(output->macroblocks +
		(row - output->firstRow) * ((const AvcencLayout *)picture->layout)->macroblocksPitch,
		((const AvcencLayout *)picture->layout)->macroblocksPitch);
	info->macroblocks = macroblocks;
	info->macroblock = macroblocks + firstMb * 16;
	info->width = picture->widthInMbs * 16;
	info->height = picture->heightInMbs * 16;
	info->sliceType = slot->sliceType;
	info->qp = picture->rateQp;
	info->chromaQp = picture->rateChromaQp;
}

/* 0x83e6c2: nothing, after each macroblock's record */
static void macroblockChecked(AvcencChunkInfo *info)
{
}

/* 0x83e6c4: the next macroblock's records */
static void macroblockNext(AvcencChunkInfo *info)
{
	if (info->column)
		info->left += 64;
	info->above += 64;
	info->column++;
	info->lumaCoded += sizeof(AvcencLumaCoded);
	info->current += 64;
	info->macroblock += sizeof(VideoAvcMacroblock);
	info->chromaCoded += sizeof(AvcencChromaCoded);
	info->strengths += 64;
}

/* 0x83e720: the coded blocks' records closed, from the chunk's first */
static void codedClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info)
{
	uint32_t size = info->mbs * 8;

	vnzPublicCloseProtected(info->lumaCoded - size, 64);
	vnzPublicCloseProtected(info->chromaCoded - size, 64);
}

/*
 * 0x83e750: the luma's samples opened: the chunk's macroblocks (and the one
 * on its left), and above them
 */
static void lumaOpen(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info)
{
	uint32_t firstMb = info->firstMb, row = slot->row, mbs = info->mbs, width, base;

	info->column = firstMb;
	info->row = row;
	width = (uint16_t)picture->width;
	base = picture->luma + firstMb * 256;
	if (firstMb)
		info->luma = vnzPublicOpenPrivate(base + row * width * 16 - 256,
			mbs * 256 + 256) + 256;
	else
		info->luma = vnzPublicOpenPrivate(base + row * width * 16, mbs * 256);
	if (row)
		info->lumaAbove = vnzPublicOpenPrivate(base + (row - 1) * width * 16, mbs * 256);
}

/* 0x83e7ce: the macroblock's type, for its deblocking */
static void lumaStart(AvcencChunkInfo *info)
{
	info->type = ((const AvcencMacroblock *)info->current)->type;
}

/* 0x83e7da */
static void lumaNext(AvcencChunkInfo *info)
{
	info->strengths += 64;
	info->current += 64;
	info->column++;
	info->luma += 256;
	info->lumaAbove += 256;
}

/* 0x83e812: the luma's samples closed */
static void lumaClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info)
{
	uint32_t firstMb = info->firstMb, size = info->mbs * 256, luma;

	if (slot->row)
		vnzPublicClosePrivate(info->lumaAbove - size, size);
	luma = info->luma - size;
	if (firstMb)
		vnzPublicClosePrivate(luma - 256, size + 256);
	else
		vnzPublicClosePrivate(luma, size);
}

/* 0x83e85e: the chroma's samples opened */
static void chromaOpen(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info)
{
	uint32_t firstMb = info->firstMb, row = slot->row, mbs = info->mbs, width, base;

	info->column = firstMb;
	info->row = row;
	width = (uint16_t)picture->chromaWidth;
	base = picture->chroma + firstMb * 128;
	if (firstMb)
		info->chroma = vnzPublicOpenPrivate(base + row * width * 8 - 128,
			mbs * 128 + 128) + 128;
	else
		info->chroma = vnzPublicOpenPrivate(base + row * width * 8, mbs * 128);
	if (row)
		info->chromaAbove = vnzPublicOpenPrivate(base + (row - 1) * width * 8, mbs * 128);
}

/* 0x83e8dc */
static void chromaStart(AvcencChunkInfo *info)
{
	info->type = ((const AvcencMacroblock *)info->current)->type;
}

/* 0x83e8e8 */
static void chromaNext(AvcencChunkInfo *info)
{
	info->strengths += 64;
	info->current += 64;
	info->column++;
	info->chroma += 128;
	info->chromaAbove += 128;
}

/*
 * 0x83e920: the output's records, the chroma's samples, the row's record
 * and the bS closed
 */
static void deblockClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencChunkInfo *info)
{
	uint32_t mbs = info->mbs, chunkWidth = picture->chunkWidth, firstMb = info->firstMb;
	uint32_t size = mbs * 128, chroma;

	vnzPublicClosePrivate(info->macroblocks,
		((const AvcencLayout *)picture->layout)->macroblocksPitch);
	if (slot->row)
		vnzPublicClosePrivate(info->chromaAbove - size, size);
	chroma = info->chroma - size;
	if (firstMb)
		vnzPublicClosePrivate(chroma - 128, size + 128);
	else
		vnzPublicClosePrivate(chroma, size);
	vnzPublicClosePrivate(info->deblockRow, 128);
	size = chunkWidth * 64;
	vnzCachePurge(info->strengths - size, size);
	vnzPublicClosePrivate(info->strengths - size, size);
}

/* an edge's p, four rows of eight samples `pitch` apart */
static inline void edgeLoad(uint8_t (*p)[8], const uint8_t *rows, uint32_t pitch)
{
	uint32_t row, i;

	for (row = 0; row < 4; row++)
		for (i = 0; i < 8; i++)
			p[row][i] = rows[row * pitch + i];
}

/* and its last three rows back, filtered */
static inline void edgeStore(const uint8_t (*p)[8], uint8_t *rows, uint32_t pitch)
{
	uint32_t row, i;

	for (row = 1; row < 4; row++)
		for (i = 0; i < 8; i++)
			rows[row * pitch + i] = p[row][i];
}

/*
 * 0x83e9ac: half the macroblock's luma deblocked: its vertical edges turned
 * (the left one against the last macroblock's columns the row's record
 * kept), then its horizontal ones (the top one against the macroblock
 * above, or the upper half)
 */
static void deblockLumaHalf(AvcencChunkInfo *info, AvcDeblock *deblock, uint32_t half)
{
	const AvcencStrengths *strengths = (const AvcencStrengths *)info->strengths;
	const AvcencDeblockRow *deblockRow = (const AvcencDeblockRow *)info->deblockRow;
	uint8_t *samples = (uint8_t *)info->luma + half * 128, *above;
	uint32_t qp = info->qp, pair, word, row, i;

	avcencDeblockLoad(deblock, samples);
	avcencDeblockTurnLeft(deblock);
	avcencDeblockTurnRight(deblock);
	pair = strengths->pairs[STRENGTHS_VERTICAL][0][half];
	if (pair) {
		for (row = 0; row < 4; row++)
			for (i = 0; i < 8; i++)
				deblock->p[row][i] = deblockRow->lumaColumns[half][row][i];
		if (avcencDeblockEdge(deblock, qp, qp, pair))
			avcencDeblockStoreLeft(deblock, samples - 256 + 12);
	}
	pair = strengths->pairs[STRENGTHS_VERTICAL][1][half];
	if (pair)
		avcencDeblockInner(deblock, qp, qp, pair);
	pair = strengths->pairs[STRENGTHS_VERTICAL][2][half];
	if (pair)
		avcencDeblockMiddle(deblock, qp, qp, pair);
	pair = strengths->pairs[STRENGTHS_VERTICAL][3][half];
	if (pair)
		avcencDeblockInnerLast(deblock, qp, qp, pair);
	avcencDeblockTurnLeft(deblock);
	avcencDeblockTurnRight(deblock);
	if (!half) {
		word = strengths->words[STRENGTHS_HORIZONTAL][0];
		above = (uint8_t *)info->lumaAbove + 12 * 16;
	} else {
		word = strengths->words[STRENGTHS_HORIZONTAL][2];
		above = samples - 4 * 16;
	}
	if (word & 0xffff) {
		edgeLoad(deblock->p, above, 16);
		avcencDeblockEdge(deblock, qp, qp, word & 0xffff);
		edgeStore((const uint8_t (*)[8])deblock->p, above, 16);
	}
	if (word >> 16) {
		edgeLoad(deblock->p, above + 8, 16);
		avcencDeblockEdgeRight(deblock, qp, qp, word >> 16);
		edgeStore((const uint8_t (*)[8])deblock->p, above + 8, 16);
	}
	word = strengths->words[STRENGTHS_HORIZONTAL][half ? 3 : 1];
	if (word & 0xffff)
		avcencDeblockInner(deblock, qp, qp, word & 0xffff);
	if (word >> 16)
		avcencDeblockInnerRight(deblock, qp, qp, word >> 16);
	avcencDeblockStore(deblock, samples);
}

/* 0x83eb74: the macroblock's luma; its last columns kept for the next one */
static void deblockLuma(AvcencChunkInfo *info)
{
	AvcDeblock deblock;

	deblockLumaHalf(info, &deblock, 0);
	deblockLumaHalf(info, &deblock, 1);
	avcencDeblockKeepLeft(&deblock, (const uint8_t *)info->luma,
		((AvcencDeblockRow *)info->deblockRow)->lumaColumns);
}

/* 0x83eb96: nothing, unreferenced */
static void __attribute__((unused)) deblockLumaUnused(AvcencChunkInfo *info)
{
}

/*
 * 0x83eb98: the macroblock's chroma: its vertical edges turned (as needed),
 * then its horizontal ones; its last columns kept
 */
static void deblockChroma(AvcencChunkInfo *info)
{
	const AvcencStrengths *strengths = (const AvcencStrengths *)info->strengths;
	AvcencDeblockRow *deblockRow = (AvcencDeblockRow *)info->deblockRow;
	uint8_t *samples = (uint8_t *)info->chroma, *above;
	uint32_t qp = info->chromaQp, turned = 0, word, component, i;
	AvcChromaDeblock deblock;

	avcencChromaDeblockLoad(&deblock, samples);
	word = strengths->words[STRENGTHS_VERTICAL][0];
	if (word) {
		for (component = 0; component < 2; component++)
			for (i = 0; i < 8; i++) {
				deblock.p[component][0][i] = deblockRow->chromaColumns[component][0][i];
				deblock.p[component][1][i] = deblockRow->chromaColumns[component][1][i];
			}
		turned = avcencChromaDeblockTurn(&deblock);
		if (avcencChromaDeblockEdge(&deblock, qp, qp, word))
			avcencChromaDeblockStoreLeft(&deblock, samples - 128 + 7);
	}
	word = strengths->words[STRENGTHS_VERTICAL][2];
	if (word) {
		if (!turned)
			turned = avcencChromaDeblockTurn(&deblock);
		avcencChromaDeblockInner(&deblock, qp, qp, word);
	}
	if (turned)
		avcencChromaDeblockTurn(&deblock);
	word = strengths->words[STRENGTHS_HORIZONTAL][0];
	if (word) {
		above = (uint8_t *)info->chromaAbove + 6 * 8;
		for (i = 0; i < 8; i++) {
			deblock.p[0][0][i] = above[i];
			deblock.p[0][1][i] = above[8 + i];
			deblock.p[1][0][i] = above[64 + i];
			deblock.p[1][1][i] = above[72 + i];
		}
		avcencChromaDeblockEdge(&deblock, qp, qp, word);
		for (i = 0; i < 8; i++) {
			above[8 + i] = deblock.p[0][1][i];
			above[72 + i] = deblock.p[1][1][i];
		}
	}
	word = strengths->words[STRENGTHS_HORIZONTAL][2];
	if (word)
		avcencChromaDeblockInner(&deblock, qp, qp, word);
	avcencChromaDeblockStore(&deblock, samples);
	avcencChromaDeblockKeepLeft(&deblock, deblockRow->chromaColumns);
}

/* 0x83ec60: nothing, unreferenced */
static void __attribute__((unused)) deblockChromaUnused(AvcencChunkInfo *info)
{
}

/*
 * The second stage: the chunk's macroblocks coded. The inter ones are
 * predicted first (their 8x8 blocks' half sample vectors), then each one's
 * residual transformed and quantised, its levels written to the output and
 * the macroblock reconstructed in the picture's samples; the intra ones
 * predicted from their reconstructed neighbours as they are coded.
 */

/* the decimation's score of a level by the zeros below it (x264's) */
static const uint8_t decimateScores[16] = {			/* 0x8f817c */
	3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* qbits, 15 + QP / 6 */
static const uint8_t quantShifts[52] = {			/* 0x8f819c */
	15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17,
	18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20,
	21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23,
};

/* QP % 6 */
static const uint8_t qpRemainder[52] = {			/* 0x8f81d0 */
	0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
	0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
	0, 1, 2, 3,
};

/* the most a macroblock's levels take: sixteen 4x4 blocks and one, 32 bytes each */
#define MB_LEVELS_SIZE		(17 * 32)

static void codingSetup(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info);
static void codingType(AvcencChunkInfo *info);
static void predictNext(AvcencChunkInfo *info);
static void codingStart(AvcencChunkInfo *info);
static void codingNext(AvcencChunkInfo *info);
static void codingClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info);
static void predictInter(AvcencChunkInfo *info);
static void codeInter(AvcencChunkInfo *info);
static void codeIntra(AvcencChunkInfo *info);

/*
 * 0x83ec64: the second stage: the chunk's inter macroblocks predicted, then
 * every macroblock coded; the row's progress counted, the third stage
 * signalled when the chunk ends the row and the others are done
 */
void avcencJobChunkStage2(uint32_t pictureAddress, uint32_t slotAddress, uint32_t unused,
	uint32_t unused2)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	uint32_t mbs, i, bit, firstMb, end, chunk, widthInMbs, signal, ready;
	AvcencFrameRecord *frameRecord;
	AvcencChunkInfo *info;
	AvcencOutputSlot *slot;
	VkVThreadContext *context;

	slot = (AvcencOutputSlot *)vnzPublicOpenProtected(slotAddress, 64);
	info = (AvcencChunkInfo *)vnzPublicOpenPrivate(slot->outputInfo +
		core * CHUNK_INFO_SIZE, CHUNK_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, CHUNK_INFO_SIZE);
	mbs = slot->mbs;
	codingSetup(picture, slot, info);
	for (i = 0; i < mbs; i++) {
		codingType(info);
		if ((info->type & MB_KIND) != MB_INTRA)
			predictInter(info);
		predictNext(info);
	}
	info->luma -= mbs * 256;
	info->current -= mbs * 64;
	info->source -= mbs * 256;
	info->column = slot->firstMb;
	for (i = 0; i < mbs; i++) {
		vnzL2Sync(info->source, 256);
		codingStart(info);
		if ((info->type & MB_KIND) == MB_INTRA)
			codeIntra(info);
		else
			codeInter(info);
		codingNext(info);
	}
	codingClose(picture, slot, info);

	bit = 1 << (slot->row & 31);
	firstMb = slot->firstMb;
	end = firstMb + slot->mbs;
	frameRecord = (AvcencFrameRecord *)picture->frameRecord;
	chunk = slot->chunk;
	vnzCachePurge((uint32_t)info, CHUNK_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, CHUNK_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slot, 64);
	widthInMbs = picture->widthInMbs;
	if (!firstMb) {
		if (widthInMbs == end) {
			signal = 1;
		} else {
			vnzSpinLock(picture->lock);
			if (frameRecord->progress[2] & bit) {
				frameRecord->progress[2] &= ~bit;
				frameRecord->rowsEncoded |= bit;
			} else {
				frameRecord->progress[1] |= bit;
			}
			vnzSpinUnlock(picture->lock);
			signal = 0;
		}
	} else {
		vnzSpinLock(picture->lock);
		ready = frameRecord->progress[2] & bit;
		if (widthInMbs == end) {
			if (ready && (frameRecord->progress[0] & bit)) {
				frameRecord->progress[2] &= ~bit;
				frameRecord->progress[0] &= ~bit;
				signal = 1;
			} else {
				frameRecord->progress[1] |= bit;
				signal = 0;
			}
		} else {
			if (ready && (frameRecord->progress[0] & bit)) {
				frameRecord->progress[2] &= ~bit;
				frameRecord->progress[0] &= ~bit;
				frameRecord->rowsEncoded |= bit;
			} else {
				frameRecord->progress[1] |= bit;
			}
			signal = 0;
		}
		vnzSpinUnlock(picture->lock);
	}
	if (signal) {
		context = vkVThreadContextLock(picture->outputContext);
		vkVThreadJobSignal(context, chunk + picture->chunkSteps * 2);
		vkVThreadContextUnlock(context);
	}
}

/*
 * 0x83ee58: the stage's records: the row's deblocking record started with
 * the row, the chunk's samples, coded blocks, macroblock records and
 * neighbours; the output's levels from where the row's last chunk left
 * them; the quantisation's parameters by the picture's QP
 */
static void codingSetup(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info)
{
	const AvcencLayout *layout = (const AvcencLayout *)picture->layout;
	uint32_t column = slot->firstMb, row = slot->row, mbs = slot->mbs, line = row & 15;
	uint32_t luma, coded, base, offset, size, levels, window, qp, quantShift, unit;
	const AvcencOutput *output;
	AvcencDeblockRow *deblockRow;
	AvcencRowLevels *rowLevels;

	info->column = column;
	info->row = row;
	if (!column) {
		deblockRow = (AvcencDeblockRow *)(picture->command[4] + line * 128);
		deblockRow->firstMb = 0;
		deblockRow->mbs = slot->mbs;
		deblockRow->parity = slot->parity;
	}
	luma = picture->luma + row * picture->width * 16 + column * 256;
	vnzL2Flush(luma, mbs * 256);
	info->luma = vnzPublicOpenPrivate(luma, mbs * 256);
	info->source = picture->clientLuma + column * 256 + row * picture->clientPitch * 16;
	coded = picture->lumaCoded + line * 64;
	if (slot->parity)
		coded += 1024;
	info->lumaCoded = vnzPublicOpenPrivate(coded, 64);
	info->current = picture->macroblocks + picture->widthInMbs * row * 64 + column * 64;
	info->rowLevels = vnzPublicOpenPrivate(picture->command[3] + line * 64, 64);
	info->neighbours = vnzPublicOpenPrivate(picture->command[1] + column * 64, mbs * 64 + 64);

	output = (const AvcencOutput *)slot->output;
	rowLevels = (AvcencRowLevels *)info->rowLevels;
	base = output->levels + (row - output->firstRow) * layout->levelsPitch;
	size = mbs * MB_LEVELS_SIZE;
	if (column) {
		base += rowLevels->used;
		offset = rowLevels->used & 63;
		size = ((base + size + 63) & ~63) - (base - offset);
		window = vnzPublicOpenPrivate(base - offset, size);
		levels = window + offset;
	} else {
		vnzL2Flush((base + 255) & ~255, (picture->widthInMbs * 64 + 255) & ~255);
		size = (size + 63) & ~63;
		window = vnzPublicOpenPrivate(base, size);
		levels = window;
		rowLevels->used = 0;
	}
	info->levels = window;
	info->levelsSize = size;
	rowLevels->next = levels;

	info->sliceType = slot->sliceType;
	info->width = picture->widthInMbs * 16;
	info->height = picture->heightInMbs * 16;
	info->pitch = picture->width;
	info->reference = picture->previousLuma;
	info->hasAbove = !(output->withInfo && output->firstRow == row);
	info->hasLeft = info->column > 0;
	qp = picture->rateQp;
	info->qp = qp;
	quantShift = quantShifts[qp];
	info->quantShift = quantShift;
	unit = 1 << (quantShift - 11);
	info->intraRounding = unit * 682;
	info->interRounding = unit * 342;
	info->quantScale = (uint32_t)avcencQuantScale[qpRemainder[qp]];
	info->levelScale = (uint32_t)avcdecLevelScale[avcdecQpRemainder[qp]];
	info->levelShift = avcdecQpShift[qp + 36];
}

/* 0x83f0b6: the macroblock's type, for its prediction */
static void codingType(AvcencChunkInfo *info)
{
	info->type = ((const AvcencMacroblock *)info->current)->type;
}

/* 0x83f0c2: the next macroblock's prediction */
static void predictNext(AvcencChunkInfo *info)
{
	info->source += 256;
	info->current += 64;
	info->column++;
	info->luma += 256;
}

/* 0x83f0ee: a macroblock's coding started: its type, its neighbours above */
static void codingStart(AvcencChunkInfo *info)
{
	uint32_t widthInMbs = info->width >> 4;

	info->type = ((const AvcencMacroblock *)info->current)->type;
	info->hasAboveRight = info->hasAbove && info->column != widthInMbs - 1;
	info->hasAboveLeft = info->hasAbove && info->column != 0;
	info->coded = 0;
}

/*
 * 0x83f130: the next macroblock's: this one's coded blocks recorded for the
 * deblocking, with its neighbours', and for the macroblocks right and below
 */
static void codingNext(AvcencChunkInfo *info)
{
	AvcencLumaCoded *lumaCoded = (AvcencLumaCoded *)info->lumaCoded;
	AvcEdges *neighbour = (AvcEdges *)info->neighbours;
	uint32_t coded = info->coded, type = info->type;

	info->source += 256;
	info->current += 64;
	info->column++;
	info->luma += 256;
	info->hasLeft = 1;
	lumaCoded->blocks = coded;
	lumaCoded->dc = (coded >> 24) & 1;
	lumaCoded->cbp = (type >> 16) & 15;
	lumaCoded->leftBlocks = neighbour->leftBlocks;
	lumaCoded->topBlocks = neighbour->topBlocks;
	info->lumaCoded += sizeof(AvcencLumaCoded);
	neighbour[0].topBlocks = coded;
	info->neighbours += sizeof(AvcEdges);
	neighbour[1].leftBlocks = coded;
}

/*
 * 0x83f1d0: the records closed; the levels the chunk wrote counted in the
 * row's, from the line the next chunk opens them at
 */
static void codingClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencChunkInfo *info)
{
	AvcencRowLevels *rowLevels = (AvcencRowLevels *)info->rowLevels;
	uint32_t mbs = slot->mbs, used = rowLevels->used;

	rowLevels->used = used + (rowLevels->next - info->levels) - (used & 63);
	vnzPublicClosePrivate(info->levels, info->levelsSize);
	vnzPublicClosePrivate(info->rowLevels, 64);
	vnzPublicClosePrivate(info->neighbours - mbs * 64, mbs * 64 + 64);
	vnzPublicClosePrivate(info->lumaCoded - mbs * 8, 64);
	vnzPublicClosePrivate(info->luma - mbs * 256, mbs * 256);
}

static void predict8x8(AvcencChunkInfo *info, AvcInter *inter, int32_t x, int32_t y,
	const int16_t *vector);
static void predictionStore(const AvcInter *inter, uint8_t *samples);

/*
 * 0x83f244 (0x83f240 jumps to it): an inter macroblock's prediction, by
 * 8x8 block, into its samples
 */
static void predictInter(AvcencChunkInfo *info)
{
	uint8_t *luma = (uint8_t *)info->luma;
	const AvcencMacroblock *macroblock = (const AvcencMacroblock *)info->current;
	int32_t x = info->column * 16, y = info->row * 16;
	uint32_t block;
	AvcInter inter;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 64);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 128);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 192);
	for (block = 0; block < 4; block++) {
		predict8x8(info, &inter, x + (block & 1) * 8, y + (block & 2) * 4,
			macroblock->vectors[block]);
		predictionStore(&inter, luma + (block & 1) * 8 + (block & 2) * 64);
	}
}

static void fetch8(const AvcencChunkInfo *info, AvcInter *inter, const uint8_t *reference,
	int32_t x, int32_t y);
static void fetch8Vertical(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y);
static void fetch8Horizontal(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y);
static void fetch8Both(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y);

/* 0x83f2c6: an 8x8 block at (x, y) plus its vector, by the vector's half samples */
static void predict8x8(AvcencChunkInfo *info, AvcInter *inter, int32_t x, int32_t y,
	const int16_t *vector)
{
	const uint8_t *reference = (const uint8_t *)info->reference;
	int32_t px = (vector[0] >> 2) + x, py = (vector[1] >> 2) + y;

	switch ((vector[1] & 2) << 2 | (vector[0] & 2)) {
	case 0:
		fetch8(info, inter, reference, px, py);
		break;
	case 2:
		fetch8Horizontal(info, inter, reference, px, py);
		avcencLumaHorizontal8(inter, LUMA_HALF);
		break;
	case 8:
		fetch8Vertical(info, inter, reference, px, py);
		avcencLumaVertical8(inter, LUMA_HALF);
		break;
	default:
		fetch8Both(info, inter, reference, px, py);
		avcencLumaCentre8(inter, LUMA_HALF);
		break;
	}
}

static inline int32_t clampTo(int32_t value, int32_t low, int32_t high)
{
	if (value < low)
		value = low;
	if (value > high)
		value = high;
	return value;
}

/* a sample's address in the tiled picture: by tile row, by tile, by row and column in it */
static inline const uint8_t *tiledAddress(const uint8_t *reference, uint32_t pitch, int32_t x,
	int32_t y)
{
	return reference + (y & ~15) * pitch + (x & ~15) * 16 + (x & 8) + (y & 15) * 16;
}

/* the 8 bytes after an aligned 8: the tile's second half, or the next tile's first */
static inline const uint8_t *tiledNext(const uint8_t *address, int32_t x)
{
	return (x & 8) ? address + 248 : address + 8;
}

/* the 8 bytes of the three streams of a sixteen-sample row */
static inline void tiledStreams(const uint8_t *address, int32_t x, const uint8_t **next,
	const uint8_t **third)
{
	*next = tiledNext(address, x);
	*third = address + 256;
	if (!(x & 7)) {
		*third = *next;
		*next = address;
	} else if ((x & 7) < 4) {
		*third = *next;
	}
}

/* 0x83f328: an 8x8 block of whole samples */
static void fetch8(const AvcencChunkInfo *info, AvcInter *inter, const uint8_t *reference,
	int32_t x, int32_t y)
{
	int32_t width = info->width, height = info->height, pitch = info->pitch;
	int32_t cx = clampTo(x, 0, width - 8), cy = clampTo(y, 0, height - 8);
	const uint8_t *address = tiledAddress(reference, pitch, cx, cy), *next;
	uint32_t row, column;

	next = (cx & 7) ? tiledNext(address, cx) : address;
	avcencLoad8(inter, address, next, cy, pitch * 16 - 256, -cx & 7);
	if (x < 0)
		avcencEdgeLeft8(inter, -(x > -7 ? x : -7));
	else if (x > width - 8)
		avcencEdgeRight8(inter, (x < width - 1 ? x : width - 1) - (width - 8));
	if (y < 0)
		avcencEdgeTop8(inter, y);
	else if (y > height - 8)
		avcencEdgeBottom8(inter, y - height + 8);
	/* the registers loaded are the block's */
	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			inter->block[row][column] = inter->window[row][column];
}

/* 0x83f40e: from two rows above to three below, for the vertical filter */
static void fetch8Vertical(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y)
{
	int32_t width = info->width, height = info->height, pitch = info->pitch;
	int32_t cx = clampTo(x, 0, width - 8), cy, step = pitch * 16 - 256;
	const uint8_t *address, *next;

	cy = y >= 2 ? y - 2 : 0;
	if (cy > height - 13)
		cy = height - 13;
	address = tiledAddress(reference, pitch, cx, cy);
	next = (cx & 7) ? tiledNext(address, cx) : address;
	avcencLoad8(inter, address, next, cy, step, -cx & 7);
	if (cy & 8) {
		next += step;
		address += step;
	}
	avcencLoad8Tall(inter, address + 128, next + 128, cy, step, -cx & 7);
	if (x < 0) {
		x = x > -7 ? x : -7;
		avcencEdgeLeft8(inter, -x);
		avcencEdgeLeft8Tall(inter, -x);
	} else if (x > width - 8) {
		x = (x < width - 1 ? x : width - 1) - (width - 8);
		avcencEdgeRight8(inter, x);
		avcencEdgeRight8Tall(inter, x);
	}
	if (y < 2)
		avcencEdgeTop8Tall(inter, y - 2);
	else if (y > height - 11)
		avcencEdgeBottom8Tall(inter, y - height + 11);
}

/* 0x83f534: from two columns left to three right, for the horizontal filter */
static void fetch8Horizontal(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y)
{
	int32_t width = info->width, height = info->height, pitch = info->pitch;
	int32_t cx = x >= 2 ? x - 2 : 0, cy = clampTo(y, 0, height - 8);
	const uint8_t *address, *next, *third;

	if (cx > width - 16)
		cx = width - 16;
	address = tiledAddress(reference, pitch, cx, cy);
	tiledStreams(address, cx, &next, &third);
	avcencLoad8Wide(inter, address, next, third, cy, pitch * 16 - 256, -cx & 7);
	if (x < 2) {
		x = x > -11 ? x : -11;
		if (x < -5)
			avcencEdgeLeft8WideFar(inter, 2 - x);
		else
			avcencEdgeLeft8Wide(inter, 2 - x);
	} else if (x > width - 14) {
		if (x > width + 1)
			x = width + 1;
		if (x < width - 5)
			avcencEdgeRight8Wide(inter, x - width + 14);
		else
			avcencEdgeRight8WideFar(inter, x - width + 14);
	}
	if (y < 0)
		avcencEdgeTop8(inter, y);
	else if (y > height - 8)
		avcencEdgeBottom8(inter, y - height + 8);
}

/* 0x83f65a: both, for the centre */
static void fetch8Both(const AvcencChunkInfo *info, AvcInter *inter,
	const uint8_t *reference, int32_t x, int32_t y)
{
	int32_t width = info->width, height = info->height, pitch = info->pitch;
	int32_t cx = x >= 2 ? x - 2 : 0, cy = y >= 2 ? y - 2 : 0, step = pitch * 16 - 256;
	const uint8_t *address, *next, *third;

	if (cx > width - 16)
		cx = width - 16;
	if (cy > height - 13)
		cy = height - 13;
	address = tiledAddress(reference, pitch, cx, cy);
	tiledStreams(address, cx, &next, &third);
	avcencLoad8Wide(inter, address, next, third, cy, step, -cx & 7);
	if (cy & 8) {
		third += step;
		next += step;
		address += step;
	}
	avcencLoad8WideTall(inter, address + 128, next + 128, third + 128, cy, step, -cx & 7);
	if (x < 2) {
		x = x > -11 ? x : -11;
		if (x < -5) {
			avcencEdgeLeft8WideFar(inter, 2 - x);
			avcencEdgeLeft8WideFarTall(inter, 2 - x);
		} else {
			avcencEdgeLeft8Wide(inter, 2 - x);
			avcencEdgeLeft8WideTall(inter, 2 - x);
		}
	} else if (x > width - 14) {
		if (x > width + 1)
			x = width + 1;
		if (x < width - 5) {
			avcencEdgeRight8Wide(inter, x - width + 14);
			avcencEdgeRight8WideTall(inter, x - width + 14);
		} else {
			avcencEdgeRight8WideFar(inter, x - width + 14);
			avcencEdgeRight8WideFarTall(inter, x - width + 14);
		}
	}
	if (y < 2)
		avcencEdgeTop8Tall(inter, y - 2);
	else if (y > height - 11)
		avcencEdgeBottom8Tall(inter, y - height + 11);
}

/* 0x83f7d8: the predicted 8x8 block into the macroblock's samples, 16 apart */
static void predictionStore(const AvcInter *inter, uint8_t *samples)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			samples[row * 16 + column] = inter->block[row][column];
}

/* 0x83f7fc: the macroblock's bottom row, the one below's above */
static void keepBottom(const uint8_t *luma, AvcEdges *neighbour)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		neighbour->above[i] = luma[240 + i];
}

static void codeInterResidual(AvcencChunkInfo *info);

/*
 * 0x83f80a: an inter macroblock coded: its residual (unless the skip check
 * found none); its right column and corner kept for an intra macroblock
 * right of it, its bottom row for the one below
 */
static void codeInter(AvcencChunkInfo *info)
{
	uint32_t widthInMbs = info->width >> 4, nextIntra = 0;
	const AvcencMacroblock *current = (const AvcencMacroblock *)info->current;
	AvcEdges *neighbour;
	uint8_t *luma;

	if (!((info->type >> MB_CHECKED_SHIFT) & 1))
		codeInterResidual(info);
	if (info->column != widthInMbs - 1)
		nextIntra = (current[1].type & MB_KIND) == MB_INTRA;
	luma = (uint8_t *)info->luma;
	neighbour = (AvcEdges *)info->neighbours;
	if (nextIntra) {
		neighbour[1].aboveLeft = neighbour[0].above[15];
		avcencKeepRight(luma, neighbour[1].left);
	}
	keepBottom(luma, neighbour);
}

/* the 4x4 blocks' zigzag scan (8.5.6), the order cpperm takes the levels in (ccr8, ccr9) */
static const uint8_t zigzag[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };

static inline int16_t scanned(const AvcencResidual *residual, uint32_t i)
{
	return residual->block[zigzag[i] >> 2][zigzag[i] & 3];
}

/* 0x83f87a: a block's decimation score (x264's), its levels all 1 or less */
static uint32_t decimateScore(const AvcencResidual *residual)
{
	uint32_t score = 0, run, i = 16;

	/* from the last level down: each one's score by the zeros below it */
	while (i && !scanned(residual, i - 1))
		i--;
	while (i) {
		i--;
		for (run = 0; i && !scanned(residual, i - 1); run++)
			i--;
		score += decimateScores[run];
	}
	return score;
}

/* the 4x4 block's levels into the output */
static inline void levelsStore(const AvcencResidual *residual, int16_t *levels)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		levels[i] = residual->block[i >> 2][i & 3];
}

/* an 8x8 block's prediction back into the macroblock's samples */
static inline void blockRestore(uint8_t *luma, const uint8_t *saved, uint32_t offset)
{
	uint32_t row, column;

	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++)
			luma[offset + row * 16 + column] = saved[offset + row * 16 + column];
}

static void residualSetup(const AvcencChunkInfo *info, AvcencResidual *residual);

/*
 * 0x83f8c2: an inter macroblock's residual: its 4x4 blocks' levels into the
 * output and reconstructed; an 8x8 block's dropped (its prediction put back)
 * when they score under 4, all of them when they score under 6
 */
static void codeInterResidual(AvcencChunkInfo *info)
{
	AvcencRowLevels *rowLevels = (AvcencRowLevels *)info->rowLevels;
	const uint8_t *source = (const uint8_t *)info->source;
	uint8_t *luma = (uint8_t *)info->luma;
	uint32_t block, half, index, offset, left, right, score, total = 0;
	uint32_t coded = 0, cbp = 0, blockCoded, blockCbp;
	int16_t *levels, *start;
	AvcencResidual residual;
	uint8_t saved[256];

	residualSetup(info, &residual);
	levels = (int16_t *)rowLevels->next;
	for (block = 0; block < 4; block++) {
		offset = (block & 1) * 8 + (block & 2) * 64;
		start = levels;
		blockCoded = 0;
		blockCbp = 0;
		score = 0;
		index = block * 4;
		for (half = 0; half < 2; half++, offset += 64) {
			left = avcencResidualPair(&residual, luma + offset, source + offset,
				saved + offset);
			if (left) {
				levelsStore(&residual, levels);
				levels += 16;
				blockCoded |= 1 << index;
				blockCbp |= 1 << block;
				if (score < 9)
					score += avcencLevelsPastOne(&residual) ? 9 :
						decimateScore(&residual);
				avcencInverseLevels(&residual);
				avcencReconstructLeft(&residual);
			}
			index++;
			right = avcencResidualRight(&residual);
			if (right) {
				levelsStore(&residual, levels);
				levels += 16;
				blockCoded |= 1 << index;
				blockCbp |= 1 << block;
				if (score < 9)
					score += avcencLevelsPastOne(&residual) ? 9 :
						decimateScore(&residual);
				avcencInverseLevels(&residual);
				avcencReconstructRight(&residual);
			}
			index++;
			if (left || right)
				avcencStorePair(&residual, luma + offset);
		}
		offset -= 128;
		if (score < 4 && blockCoded) {
			blockRestore(luma, saved, offset);
			levels = start;
		} else {
			coded |= blockCoded;
			cbp |= blockCbp;
		}
		total += score;
	}
	if (total < 6 && coded) {
		for (block = 0; block < 4; block++)
			if (cbp & (1 << block))
				blockRestore(luma, saved, (block & 1) * 8 + (block & 2) * 64);
		return;
	}
	rowLevels->next = (uint32_t)levels;
	info->type |= (cbp & 15) << 16;
	info->coded = coded;
}

/* 0x83fc68: the inter quantisation's parameters into the coprocessor */
static void residualSetup(const AvcencChunkInfo *info, AvcencResidual *residual)
{
	const int16_t *quantScale = (const int16_t *)info->quantScale;
	const int16_t *levelScale = (const int16_t *)info->levelScale;
	uint32_t i;

	residual->rounding = info->interRounding;
	residual->quantShift = info->quantShift;
	residual->levelShift = (uint8_t)info->levelShift;
	for (i = 0; i < 4; i++) {
		residual->quantScale[0][i] = quantScale[i];
		residual->quantScale[1][i] = quantScale[4 + i];
		residual->levelScale[0][i] = levelScale[i];
		residual->levelScale[1][i] = levelScale[4 + i];
	}
}

/* 0x83fc9e-0x83fec0: an Intra16x16 macroblock's residual, its levels into the output */
static void dcFirst(AvcencResidual *residual, int16_t *blockDc);
static void dcSecond(AvcencResidual *residual, int16_t *blockDc);
static void dcThird(AvcencResidual *residual, int16_t *blockDc);
static void dcFourth(AvcencResidual *residual, int16_t *blockDc);
static void dcStore(const int16_t *dc, int16_t *levels);
static void intra16Setup(AvcencResidual *residual, int16_t (*dc)[4], uint32_t quantShift,
	int32_t rounding, const int16_t *quantScale);

/* a 4x4 block's AC levels quantised, into the output where any is not 0 */
static inline void intra16Levels(AvcencResidual *residual, int16_t **levels, uint32_t *coded,
	uint32_t *cbp, uint32_t index)
{
	avcencQuantise(residual);
	if (avcencLevelsAny(residual)) {
		levelsStore(residual, *levels);
		*levels += 16;
		*coded |= 1 << index;
		*cbp |= 1 << (index >> 2);
	}
}

/*
 * 0x83fc9e: each 4x4 block transformed, its DC taken out and its AC levels
 * quantised (after the DC levels' place); the DCs through the Hadamard and
 * quantised, their levels first where any is not 0
 */
static void codeIntra16(AvcencChunkInfo *info)
{
	AvcencRowLevels *rowLevels = (AvcencRowLevels *)info->rowLevels;
	const uint8_t *source = (const uint8_t *)info->source;
	const uint8_t *luma = (const uint8_t *)info->luma;
	const int16_t *quantScale = (const int16_t *)info->quantScale;
	uint32_t quantShift = info->quantShift, rounding = info->intraRounding;
	uint32_t block, index, offset, i, coded = 0, cbp = 0;
	int16_t *levels, dc[4][4], raster[4][4], blockDc[4];
	AvcencResidual residual;

	levels = (int16_t *)rowLevels->next + 16;
	intra16Setup(&residual, dc, quantShift, rounding, quantScale);
	for (block = 0; block < 4; block++) {
		offset = (block & 1) * 8 + (block & 2) * 64;
		index = block * 4;
		avcencIntra16Pair(&residual, source + offset, luma + offset);
		dcFirst(&residual, blockDc);
		intra16Levels(&residual, &levels, &coded, &cbp, index++);
		avcencIntra16Right(&residual);
		dcSecond(&residual, blockDc);
		intra16Levels(&residual, &levels, &coded, &cbp, index++);
		avcencIntra16Pair(&residual, source + offset + 64, luma + offset + 64);
		dcThird(&residual, blockDc);
		intra16Levels(&residual, &levels, &coded, &cbp, index++);
		avcencIntra16Right(&residual);
		dcFourth(&residual, blockDc);
		intra16Levels(&residual, &levels, &coded, &cbp, index);
		for (i = 0; i < 4; i++)
			dc[block][i] = blockDc[i];
	}
	for (i = 0; i < 16; i++)
		raster[i >> 2][i & 3] = dc[((i >> 2) & 2) + ((i & 3) >> 1)]
			[((i >> 2) & 1) * 2 + (i & 1)];
	avcencDcTransform(raster);
	avcencDcQuantSetup(&residual, quantShift + 1, rounding << 1, quantScale, 0);
	avcencQuantiseDc(&residual, raster);
	if (avcencDcAny(raster[0])) {
		dcStore(raster[0], (int16_t *)rowLevels->next);
		coded |= 1 << 24;
	}
	info->type |= (cbp & 15) << 16;
	info->coded = coded;
}

/* 0x83fe5c: the 8x8 block's first 4x4 block's DC, out of its coefficients */
static void dcFirst(AvcencResidual *residual, int16_t *blockDc)
{
	blockDc[0] = residual->block[0][0];
	blockDc[1] = 0;
	blockDc[2] = 0;
	blockDc[3] = 0;
	residual->block[0][0] = 0;
}

/* 0x83fe6a: the second's */
static void dcSecond(AvcencResidual *residual, int16_t *blockDc)
{
	blockDc[1] = residual->block[0][0];
	residual->block[0][0] = 0;
}

/* 0x83fe7c */
static void dcThird(AvcencResidual *residual, int16_t *blockDc)
{
	blockDc[2] = residual->block[0][0];
	residual->block[0][0] = 0;
}

/* 0x83fe8e */
static void dcFourth(AvcencResidual *residual, int16_t *blockDc)
{
	blockDc[3] = residual->block[0][0];
	residual->block[0][0] = 0;
}

/* 0x83fe9c: the DC levels, by row */
static void dcStore(const int16_t *dc, int16_t *levels)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		levels[i] = dc[i];
}

/* 0x83feb6: the dequantisation's parameters, by the row's parity */
static void reconstructSetup(AvcRecon *recon, uint32_t levelShift, const int16_t *levelScale)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		recon->scale[i] = levelScale[((i >> 2) & 1) * 4 + (i & 3)];
	recon->shift = levelShift;
}

/* 0x83fec0: the quantisation's, the DCs cleared */
static void intra16Setup(AvcencResidual *residual, int16_t (*dc)[4], uint32_t quantShift,
	int32_t rounding, const int16_t *quantScale)
{
	uint32_t i;

	for (i = 0; i < 4; i++) {
		residual->quantScale[0][i] = quantScale[i];
		residual->quantScale[1][i] = quantScale[4 + i];
	}
	residual->quantShift = quantShift;
	residual->rounding = rounding;
	for (i = 0; i < 16; i++)
		dc[i >> 2][i & 3] = 0;
}

static void predictIntra16(AvcencChunkInfo *info);
static void reconstructIntra16(AvcencChunkInfo *info);
static void codeIntra4x4(AvcencChunkInfo *info);

/* 0x83fee8: an intra macroblock's: Intra16x16 or by 4x4 block */
static void codeIntra(AvcencChunkInfo *info)
{
	uint8_t *luma = (uint8_t *)info->luma;

	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 64);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 128);
	vnzCacheLine(VNZ_CACHE_INVALIDATE, luma + 192);
	if (((info->type >> 6) & 0xb) == 9) {
		predictIntra16(info);
		codeIntra16(info);
		reconstructIntra16(info);
	} else {
		codeIntra4x4(info);
	}
}

/*
 * 0x83ff2e: an Intra4x4 macroblock's, by 8x8 block and 4x4 block in it:
 * each predicted by its mode, its levels into the output where any is not
 * 0 and its residual added; what the macroblocks after it predict from
 */
static void codeIntra4x4(AvcencChunkInfo *info)
{
	AvcencRowLevels *rowLevels = (AvcencRowLevels *)info->rowLevels;
	AvcEdges *edges = (AvcEdges *)info->neighbours;
	const uint8_t *source = (const uint8_t *)info->source;
	uint8_t *luma = (uint8_t *)info->luma;
	const uint8_t *modes = ((const AvcencMacroblock *)info->current)->blockModes;
	const int16_t *quantScale = (const int16_t *)info->quantScale;
	const int16_t *levelScale = (const int16_t *)info->levelScale;
	uint32_t quantShift = info->quantShift, levelShift = (uint8_t)info->levelShift;
	uint32_t rounding = info->intraRounding, coded = 0, cbp = 0, index = 0, block, offset;
	int32_t lefts = info->column ? 15 : 10, aboves = info->hasAbove ? 15 : 12;
	int16_t *levels = (int16_t *)rowLevels->next;
	const uint8_t *blockSource;
	AvcRecon recon;

	avcencIntraNeighbours(&recon, edges, (uint8_t)info->hasAboveRight != 0);
	for (block = 0; block < 4; block++) {
		offset = (block & 1) * 8 + (block & 2) * 64;
		blockSource = source + offset;
		avcencIntra4x4Block0(&recon, *modes++, lefts & 1, aboves & 1);
		if (avcencIntraResidual(&recon, blockSource, quantShift, rounding, quantScale, 0)) {
			memcpy(levels, recon.residual, 32);
			levels += 16;
			avcencIntraInverse(&recon, levelShift, levelScale);
			avcencAddIntra0(&recon);
			coded |= 1 << index;
			cbp |= 1 << block;
		}
		index++;
		avcencIntra4x4Block1(&recon, *modes++, aboves & 1);
		if (avcencIntraResidual(&recon, blockSource + 4, quantShift, rounding, quantScale,
				0)) {
			memcpy(levels, recon.residual, 32);
			levels += 16;
			avcencIntraInverse(&recon, levelShift, levelScale);
			avcencAddIntra1(&recon);
			coded |= 1 << index;
			cbp |= 1 << block;
		}
		index++;
		avcencIntra4x4Block2(&recon, *modes++, lefts & 1);
		if (avcencIntraResidual(&recon, blockSource + 64, quantShift, rounding, quantScale,
				0)) {
			memcpy(levels, recon.residual, 32);
			levels += 16;
			avcencIntraInverse(&recon, levelShift, levelScale);
			avcencAddIntra2(&recon);
			coded |= 1 << index;
			cbp |= 1 << block;
		}
		index++;
		avcencIntra4x4Block3(&recon, *modes++);
		if (avcencIntraResidual(&recon, blockSource + 68, quantShift, rounding, quantScale,
				0)) {
			memcpy(levels, recon.residual, 32);
			levels += 16;
			avcencIntraInverse(&recon, levelShift, levelScale);
			avcencAddIntra3(&recon);
			coded |= 1 << index;
			cbp |= 1 << block;
		}
		index++;
		avcencStoreIntra(&recon, luma + offset);
		avcencIntraNext(&recon, block, edges);
		lefts >>= 1;
		aboves >>= 1;
	}
	info->type |= (cbp & 15) << 16;
	info->coded = coded;
	rowLevels->next = (uint32_t)levels;
	avcencIntraKeep(&recon, luma, edges);
}

/* 0x840162: an Intra16x16 macroblock's prediction, by its mode */
static void predictIntra16(AvcencChunkInfo *info)
{
	const AvcEdges *edges = (const AvcEdges *)info->neighbours;
	uint8_t *luma = (uint8_t *)info->luma;

	switch ((info->type >> 12) & 3) {
	case 0:
		avcencIntra16Vertical(edges, luma);
		break;
	case 1:
		avcencIntra16Horizontal(edges->left, luma);
		break;
	case 2:
		avcencIntra16Dc(edges->left, edges, info->column != 0,
			(uint8_t)info->hasAbove != 0, luma);
		break;
	default:
		avcencIntra16Plane(edges->left, edges, edges->aboveLeft, luma);
		break;
	}
}

/* 0x8401ce: the 8x8 block's DC levels */
static void intra16Dc(AvcRecon *recon, uint32_t block)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		recon->blockDc[i] = recon->dc[block][i];
}

/*
 * 0x8401f0: and reconstructed from its levels as the decoder does; what
 * the macroblocks right and below predict from, where they are coded
 */
static void reconstructIntra16(AvcencChunkInfo *info)
{
	AvcencRowLevels *rowLevels = (AvcencRowLevels *)info->rowLevels;
	AvcEdges *edges = (AvcEdges *)info->neighbours;
	uint8_t *luma = (uint8_t *)info->luma, *samples;
	uint8_t *right = edges[1].left, *bottom = edges->above;
	uint32_t type = info->type, coded = info->coded & 0xffff, block, i;
	uint32_t keepRight = (type >> 24) & 1, keepBottom = (type >> 25) & 1;
	int16_t *levels = (int16_t *)rowLevels->next;
	AvcRecon recon;

	edges[1].aboveLeft = edges->above[15];
	if ((info->coded >> 24) & 1) {
		avcencDcIntra16(&recon, levels, info->qp);
		levels += 16;
	} else {
		if ((type >> 16) & 15)
			levels += 16;
		for (i = 0; i < 16; i++)
			recon.dc[i >> 2][i & 3] = 0;
	}
	reconstructSetup(&recon, (uint8_t)info->levelShift, (const int16_t *)info->levelScale);
	for (block = 0; block < 4; block++) {
		samples = luma + (block & 1) * 8 + (block & 2) * 64;
		intra16Dc(&recon, block);
		avcencLoadIntra16(&recon, samples);
		if (coded & 1) {
			avcencIdctAc(&recon, levels);
			levels += 16;
		} else {
			avcencIdctDc(&recon);
		}
		avcencAddIntra16Block0(&recon);
		if (coded & 2) {
			avcencIdctAc(&recon, levels);
			levels += 16;
		} else {
			avcencIdctDc(&recon);
		}
		avcencAddIntra16Block1(&recon);
		if (coded & 4) {
			avcencIdctAc(&recon, levels);
			levels += 16;
		} else {
			avcencIdctDc(&recon);
		}
		avcencAddIntra16Block2(&recon);
		if (coded & 8) {
			avcencIdctAc(&recon, levels);
			levels += 16;
		} else {
			avcencIdctDc(&recon);
		}
		avcencAddIntra16Block3(&recon);
		avcencStoreIntra16(&recon, samples);
		if (keepRight && (block & 1))
			avcencKeepRightIntra16(&recon, &right);
		if (keepBottom && (block & 2)) {
			memcpy(bottom, recon.block[7], 8);
			bottom += 8;
		}
		coded >>= 4;
	}
	rowLevels->next = (uint32_t)levels;
}
