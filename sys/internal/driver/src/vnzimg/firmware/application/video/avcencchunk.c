/*
 * The AVC encoder's chunk stages (0x83e260-): the jobs that take a chunk of a
 * row of macroblocks once the motion jobs decided it. The first deblocks
 * the chunk encoded before it in the row (its bS, then the luma's and the
 * chroma's edges in the picture's samples) and writes its macroblocks'
 * records into the output.
 */
#include "avcencstream.h"

#define MB_SKIP_SHIFT		11	/* AvcencMacroblock's type */
#define MB_INTRA		0x200
#define MB_KIND			0x300
#define SLICE_I			2

static void deblockSetup(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencDeblockInfo *info);
static void macroblockRecord(AvcencDeblockInfo *info);
static void macroblockChecked(AvcencDeblockInfo *info);
static void macroblockNext(AvcencDeblockInfo *info);
static void codedClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencDeblockInfo *info);
static void lumaOpen(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencDeblockInfo *info);
static void lumaStart(AvcencDeblockInfo *info);
static void lumaNext(AvcencDeblockInfo *info);
static void lumaClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencDeblockInfo *info);
static void chromaOpen(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencDeblockInfo *info);
static void chromaStart(AvcencDeblockInfo *info);
static void chromaNext(AvcencDeblockInfo *info);
static void deblockClose(AvcencPicture *picture, AvcencOutputSlot *slot,
	AvcencDeblockInfo *info);
static void deblockLuma(AvcencDeblockInfo *info);
static void deblockChroma(AvcencDeblockInfo *info);

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
	AvcencDeblockInfo *info;
	AvcencOutputSlot *slot;
	VkVThreadContext *context;

	slot = (AvcencOutputSlot *)vnzPublicOpenProtected(slotAddress, 64);
	info = (AvcencDeblockInfo *)vnzPublicOpenPrivate(slot->outputInfo +
		core * DEBLOCK_INFO_SIZE, DEBLOCK_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, DEBLOCK_INFO_SIZE);
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
	vnzCachePurge((uint32_t)info, DEBLOCK_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, DEBLOCK_INFO_SIZE);
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
static void macroblockRecord(AvcencDeblockInfo *info)
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
	AvcencDeblockInfo *info)
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
static void macroblockChecked(AvcencDeblockInfo *info)
{
}

/* 0x83e6c4: the next macroblock's records */
static void macroblockNext(AvcencDeblockInfo *info)
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
	AvcencDeblockInfo *info)
{
	uint32_t size = info->mbs * 8;

	vnzPublicCloseProtected(info->lumaCoded - size, 64);
	vnzPublicCloseProtected(info->chromaCoded - size, 64);
}

/*
 * 0x83e750: the luma's samples opened: the chunk's macroblocks (and the one
 * on its left), and above them
 */
static void lumaOpen(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencDeblockInfo *info)
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
static void lumaStart(AvcencDeblockInfo *info)
{
	info->type = ((const AvcencMacroblock *)info->current)->type;
}

/* 0x83e7da */
static void lumaNext(AvcencDeblockInfo *info)
{
	info->strengths += 64;
	info->current += 64;
	info->column++;
	info->luma += 256;
	info->lumaAbove += 256;
}

/* 0x83e812: the luma's samples closed */
static void lumaClose(AvcencPicture *picture, AvcencOutputSlot *slot, AvcencDeblockInfo *info)
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
	AvcencDeblockInfo *info)
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
static void chromaStart(AvcencDeblockInfo *info)
{
	info->type = ((const AvcencMacroblock *)info->current)->type;
}

/* 0x83e8e8 */
static void chromaNext(AvcencDeblockInfo *info)
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
	AvcencDeblockInfo *info)
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
static void deblockLumaHalf(AvcencDeblockInfo *info, AvcDeblock *deblock, uint32_t half)
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
static void deblockLuma(AvcencDeblockInfo *info)
{
	AvcDeblock deblock;

	deblockLumaHalf(info, &deblock, 0);
	deblockLumaHalf(info, &deblock, 1);
	avcencDeblockKeepLeft(&deblock, (const uint8_t *)info->luma,
		((AvcencDeblockRow *)info->deblockRow)->lumaColumns);
}

/* 0x83eb96: nothing, unreferenced */
static void __attribute__((unused)) deblockLumaUnused(AvcencDeblockInfo *info)
{
}

/*
 * 0x83eb98: the macroblock's chroma: its vertical edges turned (as needed),
 * then its horizontal ones; its last columns kept
 */
static void deblockChroma(AvcencDeblockInfo *info)
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
static void __attribute__((unused)) deblockChromaUnused(AvcencDeblockInfo *info)
{
}
