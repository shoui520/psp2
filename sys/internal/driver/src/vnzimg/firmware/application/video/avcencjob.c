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

/* 0x839852: the groups' SAD per group, from each row's sum */
static uint32_t sceneSad(const AvcencPicture *picture, const uint8_t *results)
{
	uint32_t groups = picture->widthInGroups * picture->heightInGroups, sum = 0, row;

	for (row = 0; row < picture->heightInGroups; row++)
		sum += *(const uint32_t *)(results + row * 64 + 60);
	return sum / groups;
}

/* 0x839894: the spread of the groups' vectors, x and y's variances added */
static int32_t sceneSpread(const AvcencPicture *picture, const uint8_t *results)
{
	int32_t width = picture->widthInGroups, height = picture->heightInGroups;
	int32_t groups = width * height, sumX = 0, sumY = 0, squaresX = 0, squaresY = 0;
	int32_t row, column, x, y;

	for (row = 0; row < height; row++) {
		for (column = 0; column < width; column++) {
			x = (int8_t)results[column * 2];
			y = (int8_t)results[column * 2 + 1];
			sumX += x;
			sumY += y;
			squaresX += x * x;
			squaresY += y * y;
		}
		results += 64;
	}
	return (squaresX / groups - sumX * sumX / (groups * groups)) +
		(squaresY / groups - sumY * sumY / (groups * groups));
}

/* 0x839948: the limits past which both mean a new scene, by the picture's size */
static void sceneLimits(const AvcencPicture *picture, int32_t *sad, int32_t *spread)
{
	if (picture->widthInMbs * picture->heightInMbs < 301) {
		*sad = 3000;
		*spread = 60;
	} else {
		*sad = 2000;
		*spread = 120;
	}
}

/*
 * 0x8397b0: a new scene, from the coarse search of a predicted picture's
 * previous one: its SADs and the spread of its vectors both over their limits
 * make the next picture an IDR. Not after an IDR, nor with the option off
 */
void avcencJobScene(uint32_t pictureAddress, uint32_t slicesAddress, uint32_t rateAddress,
	uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	AvcencRate *rate = (AvcencRate *)rateAddress;
	int32_t sadLimit, spreadLimit, sad, spread;
	AvcencSlices *slices;
	uint32_t results;
	int8_t change;

	if (picture->predicted) {
		rate->forceIdr = 0;
		return;
	}
	slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	results = vnzPublicOpenProtected(picture->searchResults, picture->heightInGroups * 64);
	change = 0;
	if (!slices->idr && !picture->option2) {
		sad = sceneSad(picture, (const uint8_t *)results);
		spread = sceneSpread(picture, (const uint8_t *)results);
		sceneLimits(picture, &sadLimit, &spreadLimit);
		if (sadLimit < sad && spreadLimit < spread)
			change = 1;
	}
	vnzPublicCloseProtected(results, picture->heightInGroups * 64);
	vnzPublicCloseProtected((uint32_t)slices, 64);
	rate->forceIdr = change;
}

/* the weights, by QP, of a vector's bits in the inter decision and of the intra one's */
static const uint32_t interWeights[52] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4,
	5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 23, 25, 29, 32, 36, 40, 45, 51, 57,
	64, 72, 81, 91,
};
static const uint32_t intraWeights[52] = {
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 160, 180, 240, 280, 300, 360, 400, 400,
	400, 430, 540, 590, 620, 660, 700, 750, 810, 870, 940, 1020, 1100, 1200,
};

/* the decisions of a macroblock (avcencdecide.c, and below) */
void avcencDecideInter(AvcencDecideInfo *info);
static void avcencDecideLuma(AvcencDecideInfo *info);
static void avcencDecideChroma(AvcencDecideInfo *info);

/* a group's macroblocks: up to four, fewer at the picture's edge */
static inline uint8_t groupSize(uint32_t first, uint32_t size)
{
	return size < first + 4 ? size - first : 4;
}

/*
 * the inter decision's rows: the reference around the coarse search's
 * vector (the group's, or the group to the left or above when it is not
 * whole) brought into the L2 cache ahead, a row of tiles at a time; each
 * macroblock decided on the source's
 */
static void decideInterRows(AvcencPicture *picture, AvcencSlices *slices,
	AvcencDecideInfo *info, uint32_t column, uint32_t row)
{
	int32_t group = column >> 2, groupRow = row >> 2, r, m, x, y, lines, tiles, size;
	uint32_t luma, chroma, records;

	info->interColumns = groupSize(column, picture->widthInMbs);
	info->interRows = groupSize(row, picture->heightInMbs);
	info->interRow = row;
	info->interColumn = column;
	info->width = picture->widthInMbs * 16;
	info->height = picture->heightInMbs * 16;
	info->referencePitch = picture->width;
	info->referenceChromaPitch = picture->chromaWidth;
	info->interWeight = interWeights[picture->rateQp];
	info->reference = picture->luma;
	info->referenceChroma = picture->chroma;
	if ((int8_t)info->interColumns != 4)
		group--;
	if ((int8_t)info->interRows != 4)
		groupRow--;
	info->results = vnzPublicOpenProtected(picture->searchResults + groupRow * 64, 64);
	info->vector = (const int8_t *)(info->results + group * 2);
	info->lumaThreshold = slices->lumaThreshold;
	info->lumaShift = slices->lumaShift;
	info->scaled = slices->scaled;
	for (r = 0; r < info->interRows; r++) {
		if (r == 0) {
			/* the window: 80x32 of the reference, at least, from the vector */
			x = column * 16 + info->vector[0] * 4 - 4;
			if (x > info->width - 80)
				x = info->width - 80;
			if (x < 0)
				x = 0;
			tiles = 4;
			if (info->width != 64)
				tiles = (x & 15) < 12 ? 5 : 6;
			y = (int32_t)(row + r) * 16 + info->vector[1] * 4 - 4;
			if (y > info->height - 32)
				y = info->height - 32;
			if (y < 0)
				y = 0;
			lines = (y & 15) < 12 ? 2 : 3;
			y = y >> 4 << 4;
			luma = info->reference + y * info->referencePitch + (x >> 4) * 256;
			size = tiles * 256;
			chroma = info->referenceChroma + (y >> 1) * info->referenceChromaPitch +
				((x + 1) >> 5) * 256;
			tiles = (tiles + 1) >> 1 << 8;
			while (lines-- > 0) {
				vnzL2Sync(luma, size);
				vnzL2Sync(chroma, tiles);
				y += 16;
				luma += info->referencePitch * 16;
				chroma += info->referenceChromaPitch * 8;
			}
			info->prefetchLuma = luma;
			info->prefetchLine = y;
			info->prefetchLumaSize = size;
			info->prefetchChroma = chroma;
			info->prefetchChromaSize = tiles;
		} else if (info->prefetchLine < info->height) {
			vnzL2Sync(info->prefetchLuma, info->prefetchLumaSize);
			vnzL2Sync(info->prefetchChroma, info->prefetchChromaSize);
			info->prefetchLine += 16;
			info->prefetchLuma += info->referencePitch * 16;
			info->prefetchChroma += info->referenceChromaPitch * 8;
		}
		info->interY = row + r;
		info->interX = column;
		info->interLuma = picture->nextLuma + (uint16_t)picture->nextPitch * (row + r) * 16 +
			column * 256;
		info->interChroma = picture->nextChroma +
			(uint16_t)picture->nextChromaPitch * (row + r) * 8 + column * 128;
		info->interOutput = picture->interRecords + picture->widthInMbs * (row + r) * 64 +
			column * 64;
		size = info->interColumns * 64;
		records = vnzPublicOpenPrivate(info->interOutput, size);
		vnzCacheInvalidateInside(records, size);
		info->interOutput = records;
		for (m = 0; m < info->interColumns; m++) {
			vnzL2Sync(info->interLuma, 256);
			if (!(m & 1))
				vnzL2Sync(info->interChroma, 256);
			avcencDecideInter(info);
			info->interX++;
			info->interLuma += 256;
			info->interChroma += 128;
			info->interOutput += 64;
		}
		vnzPublicClosePrivate(records, size);
	}
	vnzPublicCloseProtected(info->results, 64);
}

/*
 * the final decision's rows: each macroblock's intra decision, and the
 * choice against the inter one. In a predicted picture a macroblock the
 * inter decision skipped is left so
 */
static void decideRows(AvcencPicture *picture, AvcencSlices *slices, AvcencDecideInfo *info,
	int8_t idr)
{
	int32_t r, m, i, size;
	uint32_t interRecords, records;
	int32_t y;

	for (r = 0; r < info->rows; r++) {
		info->mbX = info->column;
		info->mbY = y = info->row + r;
		info->sliceStart = 0;
		for (i = 0; i < slices->count; i++) {
			if (slices->firstRow[i] == y) {
				info->sliceStart = 1;
				break;
			}
		}
		info->luma = picture->nextLuma + (uint16_t)picture->nextPitch * y * 16 +
			info->column * 256;
		info->chroma = picture->nextChroma + (uint16_t)picture->nextChromaPitch * y * 8 +
			info->column * 128;
		info->record = picture->macroblockRecords + picture->widthInMbs * y * 64 +
			info->column * 64;
		info->interRecord = picture->interRecords + picture->widthInMbs * y * 64 +
			info->column * 64;
		size = info->columns * 64;
		interRecords = vnzPublicOpenProtected(info->interRecord, size);
		info->interRecord = interRecords;
		records = vnzPublicOpenPrivate(info->record, size);
		vnzCacheInvalidateInside(records, size);
		info->record = records;
		for (m = 0; m < info->columns; m++) {
			if (idr) {
				vnzL2Sync(info->luma, 256);
				if (!(m & 1))
					vnzL2Sync(info->chroma, 256);
				avcencDecideLuma(info);
				avcencDecideChroma(info);
			} else if (((int8_t *)info->interRecord)[57] == 0) {
				avcencDecideLuma(info);
				avcencDecideChroma(info);
			} else {
				((AvcencMacroblock *)info->record)->intraCost = 0xffff;
			}
			info->luma += 256;
			info->chroma += 128;
			info->mbX++;
			info->record += 64;
			info->interRecord += 64;
		}
		vnzPublicClosePrivate(records, size);
		vnzPublicCloseProtected(interRecords, size);
	}
}

/*
 * 0x839970: the decisions of a group's macroblocks: the inter one (not in an
 * IDR picture), then the intra one and the choice; the group's job in the
 * wavefront is signalled
 */
void avcencJobDecide(uint32_t pictureAddress, uint32_t slicesAddress, uint32_t position,
	uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	AvcencSlices *slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	uint32_t column = position & 0xff, row = (position >> 8) & 0xff, job, step;
	VkVThreadContext *context;
	AvcencDecideInfo *info;
	int8_t idr;

	info = (AvcencDecideInfo *)vnzPublicOpenPrivate(slices->streamInfo +
		core * DECIDE_INFO_SIZE, DECIDE_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, DECIDE_INFO_SIZE);
	idr = slices->idr;
	if (!idr)
		decideInterRows(picture, slices, info, column, row);

	info->columns = groupSize(column, picture->widthInMbs);
	info->rows = groupSize(row, picture->heightInMbs);
	info->column = column;
	info->row = row;
	info->heightInMbs = picture->heightInMbs;
	info->widthInMbs = picture->widthInMbs;
	info->sliceType = slices->sliceType;
	info->pitch = picture->nextPitch;
	info->chromaPitch = picture->nextChromaPitch;
	info->sourceLuma = picture->nextLuma;
	info->sourceChroma = picture->nextChroma;
	info->intraWeight = intraWeights[picture->rateQp];
	decideRows(picture, slices, info, idr);

	/* the group's job: by the wavefront's order, the IDR picture's in pairs of groups */
	column = info->column >> 2;
	row = info->row >> 2;
	if (slices->idr) {
		step = row * 2;
		if (picture->widthInMbs < 9)
			job = picture->index * 8 + picture->jobArgument +
				picture->jobGroups * (step & 7) + ((step & 3) >> 2);
		else
			job = picture->index * 8 + picture->jobArgument +
				picture->jobGroups * (step & 7) + ((column * 2 + (step & 3)) >> 2);
	} else {
		job = picture->index * 8 + picture->jobs * 4 + (row & 3) * picture->jobs + column;
	}
	context = vkVThreadContextLock((VkVThreadContext *)picture->context);
	vkVThreadJobSignal(context, job + 8);
	vkVThreadContextUnlock(context);
	vnzCachePurge((uint32_t)info, DECIDE_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, DECIDE_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slices, 64);
}

/* a pair's SADs into the row's candidates */
static inline void storeCosts(AvcencIntraRow *row, uint32_t candidate, uint32_t block,
	const uint16_t *sads)
{
	row->costs[candidate][block] = sads[0];
	row->costs[candidate][block + 1] = sads[1];
}

static inline void keepDc(const AvcencIntra *intra, AvcencIntraPredictions *predictions)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		predictions->dc[i] = intra->prediction[0][i];
}

/* 0x83a184: the first pair's candidates, all five */
static void intraCostsFirst(AvcencIntra *intra, AvcencIntraRow *row)
{
	AvcencIntraPredictions *predictions = &row[1].predictions;
	uint16_t sads[2];

	avcencIntraDcFirst(intra);
	keepDc(intra, predictions);
	avcencIntraSadDcFirst(intra, sads);
	storeCosts(row, INTRA_DC, 0, sads);
	avcencIntraHorizontalFirst(intra);
	avcencIntraSadHorizontalFirst(intra, sads);
	storeCosts(row, INTRA_HORIZONTAL, 0, sads);
	avcencIntraDownLeftFirst(intra, predictions);
	avcencIntraSadDownLeftFirst(intra, sads);
	storeCosts(row, INTRA_DOWN_LEFT, 0, sads);
	avcencIntraVerticalFirst(intra);
	avcencIntraSadVerticalFirst(intra, sads);
	storeCosts(row, INTRA_VERTICAL, 0, sads);
	avcencIntraDownRightFirst(intra, predictions);
	avcencIntraSadDownRightFirst(intra, sads);
	storeCosts(row, INTRA_DOWN_RIGHT, 0, sads);
}

/* 0x83a204: the second pair's, all five */
static void intraCostsSecond(AvcencIntra *intra, AvcencIntraRow *row)
{
	AvcencIntraPredictions *predictions = &row[2].predictions;
	uint16_t sads[2];

	avcencIntraDcSecond(intra);
	keepDc(intra, predictions);
	avcencIntraSadDcSecond(intra, sads);
	storeCosts(row, INTRA_DC, 2, sads);
	avcencIntraHorizontalSecond(intra);
	avcencIntraSadHorizontalSecond(intra, sads);
	storeCosts(row, INTRA_HORIZONTAL, 2, sads);
	avcencIntraDownLeftSecond(intra, predictions);
	avcencIntraSadDownLeftSecond(intra, sads);
	storeCosts(row, INTRA_DOWN_LEFT, 2, sads);
	avcencIntraVerticalSecond(intra);
	avcencIntraSadVerticalSecond(intra, sads);
	storeCosts(row, INTRA_VERTICAL, 2, sads);
	avcencIntraDownRightSecond(intra, predictions);
	avcencIntraSadDownRightSecond(intra, sads);
	storeCosts(row, INTRA_DOWN_RIGHT, 2, sads);
}

/* 0x83a286: the first pair's, without a left macroblock: block 0 has no horizontal or down right */
static void intraCostsFirstTop(AvcencIntra *intra, AvcencIntraRow *row)
{
	AvcencIntraPredictions *predictions = &row[1].predictions;
	uint16_t sads[2];

	avcencIntraDcFirstTop(intra);
	keepDc(intra, predictions);
	avcencIntraSadDcFirst(intra, sads);
	storeCosts(row, INTRA_DC, 0, sads);
	avcencIntraHorizontalFirst(intra);
	avcencIntraSadHorizontalFirst(intra, sads);
	sads[0] = INTRA_UNAVAILABLE;
	storeCosts(row, INTRA_HORIZONTAL, 0, sads);
	avcencIntraDownLeftFirst(intra, predictions);
	avcencIntraSadDownLeftFirst(intra, sads);
	storeCosts(row, INTRA_DOWN_LEFT, 0, sads);
	avcencIntraVerticalFirst(intra);
	avcencIntraSadVerticalFirst(intra, sads);
	storeCosts(row, INTRA_VERTICAL, 0, sads);
	avcencIntraDownRightFirst(intra, predictions);
	avcencIntraSadDownRightFirst(intra, sads);
	sads[0] = INTRA_UNAVAILABLE;
	storeCosts(row, INTRA_DOWN_RIGHT, 0, sads);
}

/* 0x83a316: the first pair's, with nothing above: DC and horizontal */
static void intraCostsFirstLeft(AvcencIntra *intra, AvcencIntraRow *row)
{
	uint16_t sads[2];

	avcencIntraDcFirstLeft(intra);
	keepDc(intra, &row[1].predictions);
	avcencIntraSadDcFirst(intra, sads);
	storeCosts(row, INTRA_DC, 0, sads);
	avcencIntraHorizontalFirst(intra);
	avcencIntraSadHorizontalFirst(intra, sads);
	storeCosts(row, INTRA_HORIZONTAL, 0, sads);
}

/* 0x83a35e: the first pair's, with nothing above or to the left */
static void intraCostsFirstAlone(AvcencIntra *intra, AvcencIntraRow *row)
{
	uint16_t sads[2];

	avcencIntraDcFirstAlone(intra);
	keepDc(intra, &row[1].predictions);
	avcencIntraSadDcFirst(intra, sads);
	storeCosts(row, INTRA_DC, 0, sads);
	avcencIntraHorizontalFirst(intra);
	avcencIntraSadHorizontalFirst(intra, sads);
	sads[0] = INTRA_UNAVAILABLE;
	storeCosts(row, INTRA_HORIZONTAL, 0, sads);
}

/* 0x83a3ae: the second pair's, with nothing above */
static void intraCostsSecondLeft(AvcencIntra *intra, AvcencIntraRow *row)
{
	uint16_t sads[2];

	avcencIntraDcSecondLeft(intra);
	keepDc(intra, &row[2].predictions);
	avcencIntraSadDcSecond(intra, sads);
	storeCosts(row, INTRA_DC, 2, sads);
	avcencIntraHorizontalSecond(intra);
	avcencIntraSadHorizontalSecond(intra, sads);
	storeCosts(row, INTRA_HORIZONTAL, 2, sads);
}

/*
 * the 16x16 prediction, for a macroblock flat enough (its variance under
 * the QP's weight); 4x4 blocks otherwise
 */
static inline int32_t intraFlat(const AvcencDecideInfo *info)
{
	return (int32_t)avcencIntraVariance((const uint8_t *)info->luma) < info->intraWeight;
}

/* 0x83a3f8 */
static void intraLuma(AvcencDecideInfo *info)
{
	((AvcencMacroblock *)info->record)->lumaMode =
		intraFlat(info) ? avcencIntraLuma(info->intraRows) : 4;
}

/* 0x83a430 */
static void intraLumaTop(AvcencDecideInfo *info)
{
	((AvcencMacroblock *)info->record)->lumaMode =
		intraFlat(info) ? avcencIntraLumaTop(info->intraRows) : 4;
}

/* 0x83a468 */
static void intraLumaLeft(AvcencDecideInfo *info)
{
	((AvcencMacroblock *)info->record)->lumaMode =
		intraFlat(info) ? avcencIntraLumaLeft(info->intraRows) : 4;
}

/* 0x83a4a0: DC, with nothing above or to the left */
static void intraLumaAlone(AvcencDecideInfo *info)
{
	((AvcencMacroblock *)info->record)->lumaMode = intraFlat(info) ? 2 : 4;
}

/* the places of a row's modes in the record */
static const uint8_t rowModes[4] = { 0, 2, 8, 10 };

/*
 * 0x83a4d0: the macroblock's luma intra decision: each 4x4 block's mode,
 * by the SAD of the five predictions from the source's own pixels (those
 * it has), the blocks' SATD with them, and the 16x16 prediction's
 */
static void avcencDecideLuma(AvcencDecideInfo *info)
{
	AvcencMacroblock *record = (AvcencMacroblock *)info->record;
	const uint8_t *luma = (const uint8_t *)info->luma;
	AvcencIntraRow *rows = info->intraRows;
	AvcencIntra intra;
	uint32_t cost;
	int32_t row, left;

	left = info->mbX >= 1;
	if (!info->sliceStart) {
		if (left)
			avcencIntraLoad(&intra, luma, luma - (uint16_t)info->pitch * 16 + 240,
				info->mbX != info->widthInMbs - 1);
		else
			avcencIntraLoadTop(&intra, luma, luma - (uint16_t)info->pitch * 16 + 240);
		cost = 0;
		for (row = 0; row < 4; row++) {
			if (left) {
				if (row)
					avcencIntraLoadRow(&intra, luma + row * 64, row == 2);
				intraCostsFirst(&intra, &rows[row]);
			} else {
				if (row)
					avcencIntraLoadRowAlone(&intra, luma + row * 64, row == 2);
				intraCostsFirstTop(&intra, &rows[row]);
			}
			intraCostsSecond(&intra, &rows[row]);
			cost += avcencIntraChoose(&intra, &rows[row], record->blockModes + rowModes[row]);
		}
		record->intraCost = cost;
		if (left)
			intraLuma(info);
		else
			intraLumaTop(info);
		return;
	}
	if (left) {
		avcencIntraLoadRow(&intra, luma, 1);
		intraCostsFirstLeft(&intra, rows);
	} else {
		avcencIntraLoadRowAlone(&intra, luma, 1);
		intraCostsFirstAlone(&intra, rows);
	}
	intraCostsSecondLeft(&intra, rows);
	cost = avcencIntraChooseTwo(&intra, rows, record->blockModes);
	for (row = 1; row < 4; row++) {
		if (left) {
			avcencIntraLoadRow(&intra, luma + row * 64, row == 2);
			intraCostsFirst(&intra, &rows[row]);
		} else {
			avcencIntraLoadRowAlone(&intra, luma + row * 64, row == 2);
			intraCostsFirstTop(&intra, &rows[row]);
		}
		intraCostsSecond(&intra, &rows[row]);
		cost += avcencIntraChoose(&intra, &rows[row], record->blockModes + rowModes[row]);
	}
	record->intraCost = cost;
	if (left)
		intraLumaLeft(info);
	else
		intraLumaAlone(info);
}

/* 0x83a7d8: a part's four candidates */
static void chromaCosts(AvcencChroma *chroma, AvcencIntraRow *row, uint32_t part)
{
	row->chromaCosts[CHROMA_HORIZONTAL][part] = avcencChromaSad(chroma);
	avcencChromaDc(chroma);
	row->chromaCosts[CHROMA_DC][part] = avcencChromaSadHalves(chroma);
	avcencChromaPlane(chroma);
	row->chromaCosts[CHROMA_PLANE][part] = avcencChromaSad(chroma);
	avcencChromaVertical(chroma);
	row->chromaCosts[CHROMA_VERTICAL][part] = avcencChromaSadHalves(chroma);
}

/* 0x83a816: without a left macroblock: DC and vertical */
static void chromaCostsTop(AvcencChroma *chroma, AvcencIntraRow *row, uint32_t part)
{
	avcencChromaDcTop(chroma);
	row->chromaCosts[CHROMA_DC][part] = avcencChromaSadHalves(chroma);
	avcencChromaVertical(chroma);
	row->chromaCosts[CHROMA_VERTICAL][part] = avcencChromaSadHalves(chroma);
}

/* 0x83a842: with nothing above: horizontal and DC */
static void chromaCostsLeft(AvcencChroma *chroma, AvcencIntraRow *row, uint32_t part)
{
	row->chromaCosts[CHROMA_HORIZONTAL][part] = avcencChromaSad(chroma);
	avcencChromaDcLeft(chroma);
	row->chromaCosts[CHROMA_DC][part] = avcencChromaSadHalves(chroma);
}

/*
 * 0x83a868: the macroblock's chroma intra decision: the prediction (of
 * those it has neighbours for) with the lowest SAD over U and V
 */
static void avcencDecideChroma(AvcencDecideInfo *info)
{
	const uint8_t *chroma = (const uint8_t *)info->chroma, *top;
	AvcencIntraRow *row = info->intraRows;
	AvcencChroma part;
	uint8_t mode;

	if (!info->sliceStart) {
		top = chroma - (uint16_t)info->chromaPitch * 8 + 56;
		if (info->mbX >= 1) {
			avcencChromaLoad(&part, chroma, top);
			chromaCosts(&part, row, 0);
			avcencChromaLoad(&part, chroma + 64, top + 64);
			chromaCosts(&part, row, 1);
			mode = avcencChromaChoose(row);
		} else {
			avcencChromaLoadTop(&part, chroma, top);
			chromaCostsTop(&part, row, 0);
			avcencChromaLoadTop(&part, chroma + 64, top + 64);
			chromaCostsTop(&part, row, 1);
			mode = avcencChromaChooseTop(row);
		}
	} else if (info->mbX >= 1) {
		avcencChromaLoadLeft(&part, chroma);
		chromaCostsLeft(&part, row, 0);
		avcencChromaLoadLeft(&part, chroma + 64);
		chromaCostsLeft(&part, row, 1);
		mode = avcencChromaChooseLeft(row);
	} else {
		mode = 0;
	}
	((AvcencMacroblock *)info->record)->chromaMode = mode;
}
