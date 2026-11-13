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

/* the window's next lines: the eighth (when more) and the ninth */
typedef const uint8_t *(*SearchNext)(AvcencSearch *search, const uint8_t *window,
	int32_t pitch, int32_t more);

/*
 * 0x839562: the SADs of the window's places from 16 lines up to its line,
 * two lines at a time, the last ones past the rows the window has the
 * same line again; the seventeenth into its own column
 */
static void searchRows(AvcencSearch *search, const uint8_t *window, int32_t pitch,
	SearchNext next, uint16_t (*sads)[20], int32_t offsets, int32_t rows)
{
	int32_t line;

	for (line = 0; line < rows; line += 2) {
		window = next(search, window, pitch, line);
		avcencSearchPair(search, &sads[0][(line & 3) * 4 + (line >> 2)], offsets);
		avcencSearchShift(search);
	}
	for (line = rows; line < 16; line += 2) {
		window = next(search, window, 0, line);
		avcencSearchPair(search, &sads[0][(line & 3) * 4 + (line >> 2)], offsets);
		avcencSearchShift(search);
	}
	next(search, window, 0, 1);
	avcencSearchLast(search, &sads[0][16], offsets);
}

/* 0x8395fe: the window's next lines */
static const uint8_t *searchNext(AvcencSearch *search, const uint8_t *window,
	int32_t pitch, int32_t more)
{
	if (more) {
		avcencSearchLine(search, 7, window);
		window += pitch;
	}
	avcencSearchLine(search, 8, window);
	return window + pitch;
}

/* 0x839618: at the picture's left edge */
static const uint8_t *searchNextLeft(AvcencSearch *search, const uint8_t *window,
	int32_t pitch, int32_t more)
{
	if (more) {
		avcencSearchLineLeft(search, 7, window);
		window += pitch;
	}
	avcencSearchLineLeft(search, 8, window);
	return window + pitch;
}

/* 0x839636: at its right edge */
static const uint8_t *searchNextRight(AvcencSearch *search, const uint8_t *window,
	int32_t pitch, int32_t more)
{
	if (more) {
		avcencSearchLineRight(search, 7, window);
		window += pitch;
	}
	avcencSearchLineRight(search, 8, window);
	return window + pitch;
}

/* the kernels for the plane's width: 64, 128, 192, else 256 samples */
static void searchBlockLoad(AvcencSearch *search, const uint8_t *block, uint32_t pitch)
{
	if (pitch == 64)
		avcencSearchBlock64(search, block);
	else if (pitch == 128)
		avcencSearchBlock128(search, block);
	else if (pitch == 192)
		avcencSearchBlock192(search, block);
	else
		avcencSearchBlock256(search, block);
}

static void searchWindow(AvcencSearch *search, const uint8_t *window, uint32_t pitch)
{
	if (pitch == 64)
		avcencSearchWindow64(search, window);
	else if (pitch == 128)
		avcencSearchWindow128(search, window);
	else if (pitch == 192)
		avcencSearchWindow192(search, window);
	else
		avcencSearchWindow256(search, window);
}

static void searchWindowLeft(AvcencSearch *search, const uint8_t *window, uint32_t pitch)
{
	if (pitch == 64)
		avcencSearchWindowLeft64(search, window);
	else if (pitch == 128)
		avcencSearchWindowLeft128(search, window);
	else if (pitch == 192)
		avcencSearchWindowLeft192(search, window);
	else
		avcencSearchWindowLeft256(search, window);
}

static void searchWindowRight(AvcencSearch *search, const uint8_t *window, uint32_t pitch)
{
	if (pitch == 64)
		avcencSearchWindowRight64(search, window);
	else if (pitch == 128)
		avcencSearchWindowRight128(search, window);
	else if (pitch == 192)
		avcencSearchWindowRight192(search, window);
	else
		avcencSearchWindowRight256(search, window);
}

/*
 * 0x839654: the block's vector: the lowest cost over the places across and
 * the lines down, the top and bottom halves' SADs together. Each lane takes
 * every fourth line, from 8 up; the seventeenth line goes to the last lane
 * alone. Of the lanes' lowest the first wins a tie
 */
static void searchChoose(AvcencSearchInfo *info)
{
	static const int16_t none[4] = { 32767, 32767, 32767, 32767 };
	int16_t top[4], bottom[4];
	AvcencChoice choice;
	int32_t group, place, lane, best;

	choice.last[0] = (int8_t)info->lastVector;
	choice.last[1] = (int8_t)(info->lastVector >> 8);
	choice.lengthWeight = info->lengthWeight;
	choice.changeWeight = info->changeWeight;
	for (lane = 0; lane < 4; lane++) {
		choice.cost[lane] = 32767;
		choice.y[lane] = lane * 4 - 8;
	}
	for (group = 0; group < 4; group++) {
		choice.x = (group & 1) - 24;
		for (lane = 0; lane < 4; lane++) {
			choice.candidates[lane][0] = choice.x;
			choice.candidates[lane][1] = choice.y[lane];
		}
		for (place = 0; place <= 24 - (group & 1); place++)
			avcencSearchChoose(&choice, (const int16_t *)&info->top[place][group * 4],
				(const int16_t *)&info->bottom[place][group * 4]);
		for (lane = 0; lane < 4; lane++)
			choice.y[lane]++;
	}
	choice.x = -24;
	for (lane = 0; lane < 4; lane++) {
		choice.y[lane] = 8;
		choice.candidates[lane][0] = choice.x;
		choice.candidates[lane][1] = 8;
	}
	for (lane = 0; lane < 3; lane++) {
		top[lane] = none[lane];
		bottom[lane] = none[lane];
	}
	for (place = 0; place < SEARCH_PLACES; place++) {
		top[3] = info->top[place][16];
		bottom[3] = info->bottom[place][16];
		avcencSearchChoose(&choice, top, bottom);
	}
	best = 3;
	if ((uint16_t)choice.cost[2] < (uint16_t)choice.cost[3])
		best = 2;
	if ((uint16_t)choice.cost[1] < (uint16_t)choice.cost[best])
		best = 1;
	if ((uint16_t)choice.cost[0] < (uint16_t)choice.cost[best])
		best = 0;
	*info->cursor = choice.vector[best];
	info->sum += (uint16_t)choice.sad[best];
	info->lastVector = choice.vector[best];
}

/*
 * 0x8390fc: a 64x64 block's search, a quarter the size in the planes: the
 * block's top half against the window from 8 lines above it (the top's
 * first line at the picture's), its bottom half from its line, each in
 * three parts across, from 12 samples left of it. At the picture's edges
 * the edge's samples stand for those past it
 */
static void searchBlock(AvcencSearchInfo *info)
{
	int32_t x = info->x, y = info->y, width = info->width, height = info->height;
	int32_t pitch = info->motionWidth, column, row, left, right, top, bottom;
	const uint8_t *window, *start;
	AvcencSearch search;
	SearchNext next;

	/* the right edge's kernels repeat each line's last sample (CPERM0) */
	left = x == 0 ? 12 : (x == 64) * 4;
	if (x + 64 == width)
		right = 12;
	else if (x + 80 == width)
		right = 14;
	else if (x + 96 == width)
		right = 16;
	else if (x + 112 == width)
		right = 18;
	else if (x + 128 == width)
		right = 20;
	else if (x + 144 == width)
		right = 22;
	else
		right = 24;
	top = (y == 0) * 8;
	if (y + 64 == height)
		bottom = 8;
	else if (y + 80 == height)
		bottom = 12;
	else
		bottom = 16;
	column = x >> 3;
	row = y >> 2;

	searchBlockLoad(&search, info->current + (row - info->firstRow * 4) * pitch + column,
		pitch);
	window = info->previous + (row + top - 8) * pitch + column - 12;
	if (left < 5) {
		if (top) {
			start = avcencSearchTop(&search, window);
		} else {
			searchWindow(&search, window, pitch);
			start = window + pitch * 8;
		}
		window += 8;
		next = searchNext;
	} else {
		window += 8;
		if (top) {
			start = avcencSearchTopLeft(&search, window);
		} else {
			searchWindowLeft(&search, window, pitch);
			start = window + pitch * 8;
		}
		next = searchNextLeft;
	}
	searchRows(&search, start, pitch, next, &info->top[0], 7, 16);
	if (top) {
		start = avcencSearchTop(&search, window);
	} else {
		searchWindow(&search, window, pitch);
		start = window + pitch * 8;
	}
	searchRows(&search, start, pitch, searchNext, &info->top[8], 7, 16);
	window += 8;
	if (right >= 16) {
		if (top) {
			start = avcencSearchTop(&search, window);
		} else {
			searchWindow(&search, window, pitch);
			start = window + pitch * 8;
		}
		next = searchNext;
	} else {
		if (top) {
			start = avcencSearchTopRight(&search, window);
		} else {
			searchWindowRight(&search, window, pitch);
			start = window + pitch * 8;
		}
		next = searchNextRight;
	}
	searchRows(&search, start, pitch, next, &info->top[16], 8, 16);

	searchBlockLoad(&search,
		info->current + (row - info->firstRow * 4 + 8) * pitch + column, pitch);
	window = info->previous + row * pitch + column - 12;
	if (left < 5) {
		searchWindow(&search, window, pitch);
		start = window + pitch * 8;
		window += 8;
		next = searchNext;
	} else {
		window += 8;
		searchWindowLeft(&search, window, pitch);
		start = window + pitch * 8;
		next = searchNextLeft;
	}
	searchRows(&search, start, pitch, next, &info->bottom[0], 7, bottom);
	searchWindow(&search, window, pitch);
	searchRows(&search, window + pitch * 8, pitch, searchNext, &info->bottom[8], 7, bottom);
	window += 8;
	if (right >= 16) {
		searchWindow(&search, window, pitch);
		next = searchNext;
	} else {
		searchWindowRight(&search, window, pitch);
		next = searchNextRight;
	}
	searchRows(&search, window + pitch * 8, pitch, next, &info->bottom[16], 8, bottom);
	searchChoose(info);
}

/*
 * 0x838ee8: the coarse search of a group's 64x64 blocks (four rows of
 * macroblocks) in the previous picture's motion plane, a vector and a SAD
 * each into the group's results, their SADs' sum last. The frame record
 * keeps the group's index; the picture's job is signalled
 */
void avcencJobLoadReference(uint32_t pictureAddress, uint32_t slicesAddress,
	uint32_t position, uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	AvcencSlices *slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	int32_t pitch, first, last, blocks, i, left;
	AvcencFrameRecord *frameRecord;
	VkVThreadContext *context;
	AvcencSearchInfo *info;
	uint32_t current;
	uint8_t group;

	info = (AvcencSearchInfo *)vnzPublicOpenPrivate(slices->streamInfo +
		core * CORE_INFO_SIZE, CORE_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, CORE_INFO_SIZE);
	info->lengthWeight = 40;
	info->changeWeight = 12;
	pitch = (uint16_t)picture->motionWidth;
	if (picture->heightInMbs < position + 4)
		info->rows = picture->heightInMbs - position;
	else
		info->rows = 4;
	info->firstRow = position;
	info->motionWidth = pitch;
	info->x = 0;
	info->reserved2 = 0;
	info->y = position * 16;
	info->width = picture->widthInMbs * 16;
	info->height = picture->heightInMbs * 16;
	info->previous = (const uint8_t *)picture->previousMotion + 4;
	/* the lines the search reads, from 8 above; its end is in pixels */
	first = position * 4 - 8;
	if (first < 0)
		first = 0;
	last = info->height;
	if (!(last < (int32_t)position * 4 + 24))
		last = position * 4 + 24;
	vnzL2Sync(picture->previousMotion + first * pitch, (last - first) * pitch);
	current = vnzPublicOpenProtected(picture->motion + position * 4 * pitch,
		(int32_t)(info->rows * pitch * 16) / 4);
	info->current = (const uint8_t *)current + 4;
	info->results = picture->searchResults + (position >> 2) * 64;
	info->results = vnzPublicOpenPrivate(info->results, 64);
	info->cursor = (uint16_t *)info->results;
	info->sumPointer = (uint32_t *)(info->results + 60);
	info->sum = 0;
	info->lastVector = 0;
	blocks = info->width / 64;
	for (i = 0; i < blocks; i++) {
		info->x = i * 64;
		left = info->width - i * 64;
		info->columns = left < 64 ? left >> 4 : 4;
		if ((int8_t)info->rows == 4 && (int8_t)info->columns == 4) {
			searchBlock(info);
			info->cursor++;
		}
	}
	vnzPublicCloseProtected((uint32_t)info->current - 4,
		(int32_t)(info->rows * (uint16_t)picture->motionWidth * 16) / 4);
	*info->sumPointer = info->sum;
	vnzPublicClosePrivate(info->results, 64);
	group = info->firstRow >> 2;
	frameRecord = (AvcencFrameRecord *)picture->frameRecord;
	frameRecord->references[group & 3] = group;
	context = vkVThreadContextLock((VkVThreadContext *)picture->context);
	vkVThreadJobSignal(context, picture->index * 8 + 24);
	vkVThreadContextUnlock(context);
	vnzCachePurge((uint32_t)info, CORE_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, CORE_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slices, 64);
}
