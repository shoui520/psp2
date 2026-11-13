/*
 * The AVC encoder's preparing jobs (0x838b48-0x83c350), which the stream
 * thread sets on the other cores: the next source's motion plane, the
 * reference's, the scene change, the motion search, the intra and the
 * decision over groups of four rows, and the rate control's
 */
#include "avcencstream.h"

/* 0x838e30: a line of a source in tiles 16 pixels wide */
static const uint8_t *sourceLine(uint32_t source, int32_t line, uint32_t pitch)
{
	return (const uint8_t *)(source + line / 16 * (pitch * 16) + line % 16 * 16);
}

/* 0x838e5c: the last 16 pixels of a band's width, which ends there */
static void downscaleTail(AvcencDownscale *downscale, const uint8_t *source)
{
	avcencDownscaleLoadRight(downscale, source + 8);
	avcencDownscaleColumns0(downscale);
	avcencDownscaleEdgeRightTail(downscale);
}

/*
 * 0x838e7a: the horizontal pass over a block of 32 pixels (two tiles), the
 * first one's left half already loaded: the next block's is loaded with it
 */
static void downscaleBlock(AvcencDownscale *downscale, const uint8_t *source, int32_t x,
	int32_t width)
{
	if (x == 0) {
		avcencDownscaleLoadLeft(downscale, source);
		avcencDownscaleEdgeLeft(downscale);
	}
	avcencDownscaleLoadRight(downscale, source + 8);
	avcencDownscaleColumns0(downscale);
	avcencDownscaleLoadLeft(downscale, source + 256);
	avcencDownscaleColumns2(downscale);
	avcencDownscaleLoadRight(downscale, source + 264);
	avcencDownscaleColumns4(downscale);
	if (x + 32 != width) {
		avcencDownscaleLoadLeft(downscale, source + 512);
		avcencDownscaleColumns6(downscale);
	} else {
		avcencDownscaleEdgeRight(downscale);
	}
}

/*
 * 0x838d2c: a band of eight lines into two lines of the motion plane, four
 * samples a block of 32 pixels. The plane's lines start four bytes to the
 * left, with the first sample, and end with the last one to eight bytes
 */
static void loadBand(AvcencLoad *load, int32_t line)
{
	AvcencDownscale downscale;
	const uint8_t *source = sourceLine(load->source, line, load->pitch);
	uint8_t *top = load->motion - 4, *bottom = top + load->motionWidth;
	uint8_t *lines = load->lines, *samples = downscale.samples;
	int32_t width = load->width, x, i, count;
	uint32_t y = load->y, wide;

	for (x = 0; x <= width - 32; x += 32) {
		downscaleBlock(&downscale, source, x, width);
		if (!(line & 8) && x <= width - 64)
			vnzL2Sync((uint32_t)source + 1024, 512);
		avcencDownscaleVertical(&downscale, lines, y);
		if (x == 0) {
			for (i = 0; i < 5; i++)
				top[i] = samples[0];
			for (i = 1; i < 4; i++)
				top[4 + i] = samples[i];
			for (i = 0; i < 5; i++)
				bottom[i] = samples[4];
			for (i = 1; i < 4; i++)
				bottom[4 + i] = samples[4 + i];
			top += 8;
			bottom += 8;
		} else {
			for (i = 0; i < 4; i++)
				top[i] = samples[i];
			for (i = 0; i < 4; i++)
				bottom[i] = samples[4 + i];
			top += 4;
			bottom += 4;
		}
		source += 512;
		lines += 24;
	}
	wide = x & 32;
	if (x != width) {
		downscaleTail(&downscale, source);
		avcencDownscaleVerticalTail(&downscale, lines, y);
		count = wide ? 8 : 4;
		top[0] = samples[0];
		for (i = 1; i < count; i++)
			top[i] = samples[1];
		bottom[0] = samples[4];
		for (i = 1; i < count; i++)
			bottom[i] = samples[5];
	} else if (!wide) {
		for (i = 0; i < 4; i++)
			top[i] = samples[3];
		for (i = 0; i < 4; i++)
			bottom[i] = samples[7];
	}
}

/*
 * 0x838b48: the next source's motion plane, four rows of macroblocks from
 * the position, in bands of eight lines (a quarter of the lines, an eighth
 * of the columns). The frame record counts the groups loaded; the last one
 * signals the picture's job
 */
void avcencJobLoad(uint32_t pictureAddress, uint32_t slicesAddress, uint32_t position,
	uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	AvcencSlices *slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	uint32_t motionWidth, rows, size, motion, i;
	AvcencFrameRecord *frameRecord;
	VkVThreadContext *context;
	AvcencLoad *load;
	int32_t group, last;

	load = (AvcencLoad *)vnzPublicOpenPrivate(slices->streamInfo + core * CORE_INFO_SIZE, 64);
	vnzCacheInvalidateInside((uint32_t)load, 64);
	motionWidth = (uint16_t)picture->motionWidth;
	rows = 4;
	if (picture->heightInMbs < position + 4)
		rows = picture->heightInMbs - position;
	load->firstRow = position;
	load->rows = rows;
	load->width = picture->widthInMbs * 16;
	load->height = picture->heightInMbs * 16;
	load->y = position * 16;
	load->pitch = picture->nextPitch;
	load->motionWidth = motionWidth;
	load->lines = (uint8_t *)picture->reference;
	size = (int32_t)(load->rows * motionWidth * 16) / 4;
	motion = picture->motion + position * 4 * motionWidth;
	vnzL2Flush(motion, size);
	load->motionOpened = vnzPublicOpenPrivate(motion, size);
	vnzCacheInvalidateInside(load->motionOpened, size);
	load->motion = (uint8_t *)load->motionOpened + 4;
	load->source = picture->nextLuma + position * 16 * (uint16_t)picture->nextPitch;
	vnzL2Sync(picture->reference, (motionWidth * 6 + 255) & ~255);
	load->linesOpened = vnzPublicOpenPrivate(picture->reference, motionWidth * 6);
	for (i = 0; i < load->rows * 2u; i++) {
		if (!(i * 8 & 8))
			vnzL2Sync(load->pitch * i * 8 + load->source, 1024);
		load->lines = (uint8_t *)load->linesOpened;
		loadBand(load, i * 8);
		load->motion += (uint16_t)picture->motionWidth * 2;
		load->y += 8;
	}
	vnzPublicClosePrivate(load->motionOpened,
		(int32_t)(load->rows * (uint16_t)picture->motionWidth * 16) / 4);
	vnzPublicClosePrivate(load->linesOpened, (uint16_t)picture->motionWidth * 6);
	frameRecord = (AvcencFrameRecord *)picture->frameRecord;
	last = picture->heightInGroups;
	if (last * 4 == picture->heightInMbs)
		last--;
	group = load->firstRow >> 2;
	frameRecord->loaded = group;
	if (group == last) {
		context = vkVThreadContextLock((VkVThreadContext *)picture->context);
		vkVThreadJobSignal(context, picture->index * 8 + 24);
		vkVThreadContextUnlock(context);
	}
	vnzCachePurge((uint32_t)load, 64);
	vnzPublicClosePrivate((uint32_t)load, 64);
	vnzPublicCloseProtected((uint32_t)slices, 64);
}
