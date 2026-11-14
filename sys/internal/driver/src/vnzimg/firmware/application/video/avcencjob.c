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
		rate->sceneChange = 0;
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
	rate->sceneChange = change;
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

/*
 * 0x83a92c: the sub-pel refinement of a group's macroblocks, each one the
 * inter decision did not skip or take as intra; the group's job in
 * the wavefront is signalled
 */
static void refineMacroblock(AvcencRefineInfo *info);

void avcencJobRefine(uint32_t pictureAddress, uint32_t slicesAddress, uint32_t position,
	uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	AvcencSlices *slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	uint32_t column = position & 0xff, row = (position >> 8) & 0xff, job, step;
	int32_t r, m, rows, columns, size, y;
	VkVThreadContext *context;
	AvcencRefineInfo *info;
	AvcencInterRecord *record;
	uint32_t records;

	info = (AvcencRefineInfo *)vnzPublicOpenPrivate(slices->streamInfo +
		core * CORE_INFO_SIZE, REFINE_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, REFINE_INFO_SIZE);
	info->columns = groupSize(column, picture->widthInMbs);
	info->rows = groupSize(row, picture->heightInMbs);
	info->row = row;
	info->column = column;
	columns = info->columns;
	info->height = picture->heightInMbs * 16;
	info->width = picture->widthInMbs * 16;
	rows = info->rows;
	info->pitch = picture->width;
	info->reference = picture->luma;
	for (r = 0; r < rows; r++) {
		info->mbX = info->column;
		info->mbY = y = info->row + r;
		info->luma = picture->nextLuma + (uint16_t)picture->nextPitch * y * 16 +
			info->mbX * 256;
		info->interRecord = picture->interRecords + picture->widthInMbs * y * 64 +
			info->mbX * 64;
		size = columns * 64;
		records = vnzPublicOpenPrivate(info->interRecord, size);
		info->interRecord = records;
		for (m = 0; m < columns; m++) {
			record = (AvcencInterRecord *)info->interRecord;
			if (!record->skip && !record->intra)
				refineMacroblock(info);
			info->luma += 256;
			info->mbX++;
			info->interRecord += 64;
		}
		vnzPublicClosePrivate(records, size);
	}

	/* the group's job: by the wavefront's order, in pairs of groups */
	column = info->column >> 2;
	step = (info->row >> 2) * 2;
	if (picture->widthInMbs < 9)
		job = picture->index * 8 + picture->jobArgument + picture->jobGroups * (step & 7) +
			((step & 3) >> 2);
	else
		job = picture->index * 8 + picture->jobArgument + picture->jobGroups * (step & 7) +
			((column * 2 + (step & 3)) >> 2);
	context = vkVThreadContextLock((VkVThreadContext *)picture->context);
	vkVThreadJobSignal(context, job + 8);
	vkVThreadContextUnlock(context);
	vnzCachePurge((uint32_t)info, REFINE_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, REFINE_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slices, 64);
}

/*
 * 0x83acda: a block's reference window, from two pixels left of and above
 * its position: the tiles' lines, the picture's edges repeated outside it.
 * Past the left or right edge the position is kept within ten pixels of it
 */
static void refineWindow(AvcencRefineInfo *info, AvcencRefine *refine, int32_t x, int32_t y,
	uint32_t block)
{
	uint32_t reference = info->reference, offset, first, second, third;
	int32_t height = info->height, width = info->width, left = 0, top = 0, line;

	if (x >= 2)
		left = x - 2;
	if (left > width - 16)
		left = width - 16;
	if (y >= 2)
		top = y - 2;
	if (top > height - 12)
		top = height - 12;
	line = top & 15;
	first = reference + (left & ~15) * 16 + (left & 8) + (top & ~15) * info->pitch +
		line * 16;
	if (left & 8) {
		second = first + 248;
		third = first + 256;
	} else {
		second = first + 8;
		third = first + 256;
	}
	if ((left & 7) == 0) {
		third = second;
		second = first;
	} else if ((left & 7) < 5) {
		third = second;
	}
	offset = (left & 7) ? left & 7 : 8;
	avcencRefineLoad(refine, (const uint8_t *)first, (const uint8_t *)second,
		(const uint8_t *)third, offset, line, info->pitch * 16 - 256);

	if (x < 2) {
		if (x < -10)
			x = -10;
		if (x < -5)
			avcencRefineEdgeLeftFar(refine, 2 - x);
		else
			avcencRefineEdgeLeft(refine, 2 - x);
	} else if (x > width - 14) {
		if (x > width + 1)
			x = width + 1;
		if (x < width - 5)
			avcencRefineEdgeRight(refine, x - 2 - (width - 16));
		else
			avcencRefineEdgeRightFar(refine, x - 2 - (width - 16));
	}
	if (y < 2)
		avcencRefineEdgeTop(refine, y - 2);
	else if (y > height - 10)
		avcencRefineEdgeBottom(refine, y - height + 10);
	avcencRefineStore(refine, info->windows[block]);
}

/*
 * 0x83abe4: a block's half pixel candidates, the whole pixel one first.
 * Those above are out of reach when the block is more than three pixels
 * above the macroblock's vector, those below when it is four below
 */
static void refineCandidates(AvcencRefineInfo *info, AvcencRefine *refine,
	const uint8_t *saved, uint32_t block, int32_t deltaY)
{
	uint16_t (*costs)[4] = info->candidates;

	costs[REFINE_WHOLE][block] = avcencRefineSad(refine);
	avcencRefineHorizontal(refine);
	costs[REFINE_LEFT][block] = avcencRefineHorizontalSad(refine);
	avcencRefineHorizontalRight(refine);
	costs[REFINE_RIGHT][block] = avcencRefineHorizontalSad(refine);
	avcencRefineVertical(refine, saved);
	if (deltaY < -3) {
		costs[REFINE_ABOVE][block] = REFINE_UNAVAILABLE;
		costs[REFINE_BELOW][block] = avcencRefineVerticalSadBelow(refine);
		avcencRefineDiagonal(refine);
		costs[REFINE_ABOVE_LEFT][block] = REFINE_UNAVAILABLE;
		costs[REFINE_BELOW_LEFT][block] = avcencRefineDiagonalSadBelow(refine);
		avcencRefineDiagonalRight(refine);
		costs[REFINE_ABOVE_RIGHT][block] = REFINE_UNAVAILABLE;
		costs[REFINE_BELOW_RIGHT][block] = avcencRefineDiagonalSadBelow(refine);
		return;
	}
	costs[REFINE_ABOVE][block] = avcencRefineVerticalSadAbove(refine);
	if (deltaY >= 4) {
		costs[REFINE_BELOW][block] = REFINE_UNAVAILABLE;
		avcencRefineDiagonal(refine);
		costs[REFINE_ABOVE_LEFT][block] = avcencRefineDiagonalSadAbove(refine);
		costs[REFINE_BELOW_LEFT][block] = REFINE_UNAVAILABLE;
		avcencRefineDiagonalRight(refine);
		costs[REFINE_ABOVE_RIGHT][block] = avcencRefineDiagonalSadAbove(refine);
		costs[REFINE_BELOW_RIGHT][block] = REFINE_UNAVAILABLE;
		return;
	}
	costs[REFINE_BELOW][block] = avcencRefineVerticalSadBelow(refine);
	avcencRefineDiagonal(refine);
	costs[REFINE_ABOVE_LEFT][block] = avcencRefineDiagonalSadAbove(refine);
	costs[REFINE_BELOW_LEFT][block] = avcencRefineDiagonalSadBelow(refine);
	avcencRefineDiagonalRight(refine);
	costs[REFINE_ABOVE_RIGHT][block] = avcencRefineDiagonalSadAbove(refine);
	costs[REFINE_BELOW_RIGHT][block] = avcencRefineDiagonalSadBelow(refine);
}

/*
 * 0x83ae14: the candidates' costs merged by the macroblock's partitions;
 * the 16x8 partitions' second takes the third block's offset
 */
static void refineMerge(AvcencRefineInfo *info, AvcencRefine *refine)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interRecord;
	uint32_t i, block;

	for (i = 0; i < REFINE_CANDIDATES; i++)
		for (block = 0; block < 4; block++)
			refine->candidates[i][block] = info->candidates[i][block];
	if (record->partition == 2) {
		avcencRefineMerge8x16(refine);
	} else if (record->partition == 1) {
		avcencRefineMerge16x8(refine);
		record->deltas[1][0] = record->deltas[2][0];
		record->deltas[1][1] = record->deltas[2][1];
	} else if (record->partition == 0) {
		avcencRefineMerge16x16(refine);
	}
}

/* 0x83ae64: the whole pixel SATDs summed by the partitions */
static void refineCosts(AvcencRefineInfo *info)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interRecord;
	uint16_t *costs = record->costs;

	switch (record->partition) {
	case 0:
		costs[0] = costs[0] + costs[1] + costs[2] + costs[3];
		costs[3] = 0;
		costs[2] = 0;
		costs[1] = 0;
		break;
	case 1:
		costs[0] = costs[0] + costs[1];
		costs[1] = costs[2] + costs[3];
		costs[2] = 0;
		costs[3] = 0;
		break;
	case 2:
		costs[0] = costs[0] + costs[2];
		costs[1] = costs[1] + costs[3];
		costs[2] = 0;
		costs[3] = 0;
		break;
	}
}

/*
 * 0x83ab18: a macroblock's refinement: each 8x8 block's window around its
 * whole pixel position, its SATD there and its candidates; then each
 * partition's best and its vector
 */
static void refineMacroblock(AvcencRefineInfo *info)
{
	AvcencInterRecord *record = (AvcencInterRecord *)info->interRecord;
	AvcencRefine refine;
	int32_t x = info->mbX * 16 + record->vector[0], y = info->mbY * 16 + record->vector[1];
	uint32_t block;

	for (block = 0; block < 4; block++) {
		refineWindow(info, &refine, x + (block & 1) * 8 + record->deltas[block][0],
			y + (block & 2) * 4 + record->deltas[block][1], block);
		avcencRefineLoadSource(&refine, (const uint8_t *)info->luma + (block & 1) * 8 +
			(block & 2) * 64);
		record->costs[block] = avcencRefineSatd(&refine);
		refineCandidates(info, &refine, info->windows[block], block,
			record->deltas[block][1]);
	}
	refineMerge(info, &refine);
	avcencRefineChoose(&refine, (const int8_t (*)[2])record->deltas, record->vectors);
	refineCosts(info);
}

/*
 * the macroblock types the motion job leaves in a record, from bit 24 its
 * coding (3, 0x13 for P_Skip's checks): an intra one's by its 16x16 mode
 * (4 for 4x4 blocks) and its chroma mode, an inter one's by its partition
 */
static const uint32_t intraTypes[5][4] = {
	{ 0x0243, 0x4243, 0x8243, 0xc243 }, { 0x1243, 0x5243, 0x9243, 0xd243 },
	{ 0x2243, 0x6243, 0xa243, 0xe243 }, { 0x3243, 0x7243, 0xb243, 0xf243 },
	{ 0x0203, 0x4203, 0x8203, 0xc203 },
};
static const uint32_t interTypes[4] = { 0x53, 0x56, 0x55, 0x58 };

/* an intra macroblock's SATD scaled up by QP (in Q16, added), into the statistics' cost */
static const uint16_t intraScales[52] = {
	10354, 10354, 10158, 10616, 10747, 9568, 10551, 11468, 12320, 12255, 12713, 13369,
	16252, 17235, 17563, 18153, 20447, 20905, 24510, 26279, 28639, 29818, 31522, 32833,
	36110, 37421, 39190, 39976, 42270, 42991, 44630, 45154, 45875, 45481, 45809, 45350,
	44957, 44761, 43909, 43712, 42991, 42926, 42336, 42532, 42860, 42139, 42270, 43122,
	42598, 43122, 44433, 44957,
};

/* P_Skip's checks: the inter types again, then P_Skip's */
#define SKIP_PARTITION		4
static const uint32_t skipTypes[5] = { 0x53, 0x56, 0x55, 0x58, 0x843 };

#define MACROBLOCK_CODED	0x03000000
#define MACROBLOCK_CHECKED	0x13000000

/* a neighbour's record: whether it is intra */
static inline int32_t recordIntra(uint32_t record)
{
	return (((AvcencMacroblock *)record)->type & 0x300) == 0x200;
}

/* a neighbour's vector, from its inter record, as four lanes: its sign in the upper two */
static inline void loadVector(int16_t *lanes, uint32_t interRecord, uint32_t block)
{
	const int16_t *vector = ((AvcencInterRecord *)interRecord)->vectors[block];

	lanes[0] = vector[0];
	lanes[1] = vector[1];
	lanes[2] = vector[1] < 0 ? -1 : 0;
	lanes[3] = lanes[2];
}

static inline void copyLanes(int16_t *to, const int16_t *from)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		to[i] = from[i];
}

static inline void clearLanes(int16_t *lanes)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		lanes[i] = 0;
}

/* the macroblock is the row's last */
static inline int32_t lastColumn(const AvcencMotionInfo *info)
{
	return info->mbX == ((int32_t)info->width >> 4) - 1;
}

/*
 * 0x83aed4: the motion job's settings: the reference's planes, the
 * picture's size, the weights and the thresholds of its QP, and the row of
 * groups' statistics
 */
static void motionSetup(AvcencPicture *picture, AvcencSlices *slices, AvcencMotionInfo *info,
	int32_t row)
{
	info->sliceType = slices->sliceType;
	info->reference = picture->luma;
	info->referenceChroma = picture->chroma;
	info->width = picture->widthInMbs * 16;
	info->height = picture->heightInMbs * 16;
	info->pitch = picture->width;
	info->chromaPitch = picture->chromaWidth;
	info->sourcePitch = picture->nextPitch;
	info->sourceChromaPitch = picture->nextChromaPitch;
	info->interWeight = interWeights[(uint8_t)picture->rateQp];
	info->intraWeight = intraWeights[(uint8_t)picture->rateQp];
	info->qp = picture->rateQp;
	info->lumaThreshold2 = slices->lumaThreshold2;
	info->lumaShift2 = slices->lumaShift2;
	info->lumaThreshold = slices->lumaThreshold;
	info->lumaShift = slices->lumaShift;
	info->chromaThreshold = slices->chromaThreshold;
	info->chromaShift = slices->chromaShift;
	info->stats = (AvcencMotionStats *)vnzPublicOpenPrivate(picture->rowsMotion +
		((row >> 1) % 16) * 64, 64);
}

/* 0x83afae: the statistics closed */
static void motionStatsClose(AvcencPicture *picture, AvcencSlices *slices,
	AvcencMotionInfo *info)
{
	vnzPublicClosePrivate((uint32_t)info->stats, 64);
}

/*
 * 0x83afb8: a row's macroblocks from a column: the source's, their records
 * and those of their neighbours: left of the first, and above (from above
 * left of the first to above right of the last, those in the picture)
 */
static void motionRow(AvcencPicture *picture, AvcencSlices *slices, AvcencMotionInfo *info,
	int32_t column, int32_t row, int32_t columns)
{
	int32_t width = picture->widthInMbs, count, i;
	uint32_t records, interRecords, current;
	int8_t sliceStart = 0;

	for (i = 0; i < slices->count; i++) {
		if (slices->firstRow[i] == row) {
			sliceStart = 1;
			break;
		}
	}
	info->mbY = row;
	info->mbX = column;
	info->row = row;
	info->firstColumn = column;
	info->sliceStart = sliceStart;
	info->columns = columns;
	info->luma = picture->nextLuma + (uint16_t)picture->nextPitch * row * 16 + column * 256;
	info->chroma = picture->nextChroma + (uint16_t)picture->nextChromaPitch * row * 8 +
		column * 128;
	records = picture->macroblockRecords + row * width * 64 + column * 64;
	interRecords = picture->interRecords + row * width * 64 + column * 64;
	current = vnzPublicOpenPrivate(records, columns * 64);
	info->interRecord = vnzPublicOpenPrivate(interRecords, columns * 64);
	info->record = current;
	if (column >= 1) {
		current = vnzPublicOpenProtected(records - 64, 64);
		info->interLeft = vnzPublicOpenProtected(interRecords - 64, 64);
		info->recordLeft = current;
	}
	if (row >= 1) {
		count = column + columns < width ? columns + 2 : columns + 1;
		records = picture->macroblockRecords + (row - 1) * width * 64 + column * 64;
		interRecords = picture->interRecords + (row - 1) * width * 64 + column * 64;
		if (column >= 1) {
			current = vnzPublicOpenProtected(records - 64, count * 64);
			info->interAboveLeft = vnzPublicOpenProtected(interRecords - 64, count * 64);
			info->recordAboveLeft = current;
			info->interAbove = info->interAboveLeft + 64;
			info->recordAbove = current + 64;
		} else {
			count--;
			current = vnzPublicOpenProtected(records, count * 64);
			info->interAbove = vnzPublicOpenProtected(interRecords, count * 64);
			info->recordAbove = current;
		}
		if (column < width - 1) {
			info->interAboveRight = info->interAbove + 64;
			info->recordAboveRight = info->recordAbove + 64;
		}
	}
	info->macroblockAbove = info->recordAbove;
	info->macroblockLeft = info->recordLeft;
	info->macroblock = info->record;
}

/*
 * 0x83b180: the next macroblock: its records and its neighbours'. The
 * first's left ones are closed, the left ones are then the last
 */
static void motionNext(AvcencPicture *picture, AvcencMotionInfo *info)
{
	uint32_t mbX = info->mbX;

	info->interRecord += 64;
	info->record += 64;
	if (info->firstColumn == mbX) {
		if (mbX >= 1) {
			vnzPublicCloseProtected(info->recordLeft, 64);
			vnzPublicCloseProtected(info->interLeft, 64);
		}
		info->recordLeft = info->record - 64;
		info->interLeft = info->interRecord - 64;
	} else {
		info->interLeft += 64;
		info->recordLeft += 64;
	}
	if (info->row >= 1) {
		info->interAbove += 64;
		info->recordAbove += 64;
		if ((int32_t)mbX < picture->widthInMbs - 1) {
			info->interAboveRight += 64;
			info->recordAboveRight += 64;
		}
		if (mbX == 0) {
			info->recordAboveLeft = info->recordAbove - 64;
			info->interAboveLeft = info->interAbove - 64;
		} else {
			info->interAboveLeft += 64;
			info->recordAboveLeft += 64;
		}
	}
	info->mbX = mbX + 1;
	info->macroblockAbove = info->recordAbove;
	info->macroblock = info->record;
	info->macroblockLeft = info->recordLeft;
	info->chroma += 128;
	info->luma += 256;
}

/* 0x83b2b6: the row's records closed, and those above it */
static void motionRowClose(AvcencPicture *picture, AvcencSlices *slices, AvcencMotionInfo *info)
{
	uint32_t row = info->row, first = info->firstColumn, columns = info->columns, count;

	vnzPublicClosePrivate(info->record - columns * 64, columns * 64);
	vnzPublicClosePrivate(info->interRecord - columns * 64, columns * 64);
	if (row == 0)
		return;
	count = columns;
	if (first != 0)
		count++;
	if (first + columns != picture->widthInMbs) {
		count++;
		vnzPublicCloseProtected(info->recordAboveRight - count * 64, count * 64);
		vnzPublicCloseProtected(info->interAboveRight - count * 64, count * 64);
	} else {
		vnzPublicCloseProtected(info->recordAbove - count * 64, count * 64);
		vnzPublicCloseProtected(info->interAbove - count * 64, count * 64);
	}
}

/*
 * 0x83b798: the macroblock's final choice between the intra decision and
 * the inter one, each its SATD and its weighted bits; the record's type,
 * the inter one's vectors, and the row of groups' statistics. An intra
 * slice's has no inter choice and weighs its bits less
 */
static void motionChoose(AvcencMotionInfo *info, uint32_t intraSlice)
{
	AvcencInterRecord *interRecord = (AvcencInterRecord *)info->interRecord;
	AvcencMacroblock *record = (AvcencMacroblock *)info->record;
	AvcencMacroblock *macroblock = (AvcencMacroblock *)info->macroblock;
	AvcencMotionStats *stats;
	uint32_t weight = info->interWeight, interCost = info->interCost, intraCost, extra, type;

	if (intraSlice) {
		intraCost = (uint16_t)(macroblock->intraCost + (macroblock->intraBits + 32) * weight);
		interCost = 0xffff;
		extra = 0;
	} else {
		if (macroblock->intraCost == 0xffff) {
			intraCost = 0xffff;
		} else {
			intraCost = (uint16_t)(macroblock->intraCost +
				weight * (macroblock->intraBits + 50));
			if (interRecord->intra)
				interCost = 0xffff;
		}
		extra = 18;
	}
	if ((int32_t)intraCost < (int32_t)interCost) {
		stats = info->stats;
		type = intraTypes[macroblock->lumaMode][macroblock->chromaMode];
		if ((int8_t)macroblock->lumaMode != 4)
			stats->intra16x16Count++;
		stats->cost += ((intraScales[(uint8_t)info->qp] * (uint32_t)macroblock->intraCost) >>
			16) + macroblock->intraCost;
		stats->bits += macroblock->intraBits + extra;
		stats->intraCount++;
	} else {
		copyLanes(record->vectors[0], interRecord->vectors[0]);
		copyLanes(record->vectors[2], interRecord->vectors[2]);
		copyLanes(record->differences[0], interRecord->differences[0]);
		copyLanes(record->differences[2], interRecord->differences[2]);
		type = interTypes[interRecord->partition];
		stats = info->stats;
		stats->cost += info->interSatd;
		stats->bits += info->interBits;
	}
	record->type = type | MACROBLOCK_CODED;
}

/* modes of a macroblock that is not 4x4 blocks: DC */
static const uint8_t dcModes[16] = {
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
};

/* a neighbour's record: its luma is 4x4 blocks */
static inline int32_t recordBlocks(uint32_t record)
{
	return ((((AvcencMacroblock *)record)->type >> 6) & 0xb) == 8;
}

/*
 * 0x83b8b0: the 4x4 blocks' modes coded (predicted from the neighbours'
 * modes, DC from a neighbour out of the picture or the slice) and the
 * intra decision's bits: three for a mode not predicted, the chroma mode's
 */
static void motionModes(AvcencMotionInfo *info)
{
	AvcencMacroblock *record = (AvcencMacroblock *)info->record, *macroblock;
	const uint8_t *above = dcModes, *left = dcModes;
	uint32_t noLeft, sliceStart = info->sliceStart, matches, bits, chromaBits;

	if (info->mbY >= 1 && recordBlocks(info->recordAbove))
		above = ((AvcencMacroblock *)info->macroblockAbove)->blockModes;
	if (info->mbX >= 1 && recordBlocks(info->recordLeft))
		left = ((AvcencMacroblock *)info->macroblockLeft)->blockModes;
	noLeft = info->mbX < 1;
	matches = avcencMotionModes(above, left, record->blockModes,
		noLeft | sliceStart | noLeft << 2 | sliceStart << 1 | sliceStart << 4 |
		sliceStart << 5, noLeft | noLeft << 2, record->modeCodes);
	macroblock = (AvcencMacroblock *)info->macroblock;
	bits = (int8_t)macroblock->lumaMode == 4 ? (16 - matches) * 3 : 0;
	switch (macroblock->chromaMode) {
	case 0:
		chromaBits = 1;
		break;
	case 1:
		chromaBits = 2;
		break;
	case 2:
	case 3:
		chromaBits = 3;
		break;
	default:
		chromaBits = 0;
		break;
	}
	macroblock->intraBits = bits + chromaBits;
}

static void motionPredict16x16(AvcencMotionInfo *info, AvcencMotion *motion);
static void motionPredict16x8(AvcencMotionInfo *info, AvcencMotion *motion);
static void motionPredict8x16(AvcencMotionInfo *info, AvcencMotion *motion);
static void motionPredict8x8(AvcencMotionInfo *info, AvcencMotion *motion);

/*
 * 0x83b9a0: the inter decision's coding: its blocks' vectors whole (each
 * partition's in all its blocks' lanes), their differences from their
 * predictions, their bits and its cost
 */
static void motionInter(AvcencMotionInfo *info, AvcencMotion *motion)
{
	AvcencInterRecord *interRecord = (AvcencInterRecord *)info->interRecord;
	uint32_t weight, satd, approximate, bits, i;

	avcencMotionVectors(motion, interRecord->vector,
		(const int16_t (*)[2])interRecord->vectors, interRecord->costs);
	weight = info->interWeight;
	satd = motion->satd;
	switch (interRecord->partition) {
	case 0:
		motionPredict16x16(info, motion);
		avcencMotionBits(motion);
		approximate = motion->bits[0] + 4;
		avcencMotionCodeBits(motion);
		bits = motion->bits[0] + 4;
		for (i = 0; i < 2; i++) {
			motion->vectors[0][2 + i] = motion->vectors[0][i];
			motion->vectors[1][i] = motion->vectors[0][i];
			motion->vectors[1][2 + i] = motion->vectors[0][i];
		}
		break;
	case 1:
		motionPredict16x8(info, motion);
		avcencMotionBits(motion);
		approximate = (uint16_t)(motion->bits[0] + motion->bits[1]) + 5;
		avcencMotionCodeBits(motion);
		bits = (uint16_t)(motion->bits[0] + motion->bits[1]) + 5;
		for (i = 0; i < 2; i++) {
			motion->vectors[1][i] = motion->vectors[0][2 + i];
			motion->vectors[1][2 + i] = motion->vectors[0][2 + i];
			motion->vectors[0][2 + i] = motion->vectors[0][i];
		}
		break;
	case 2:
		motionPredict8x16(info, motion);
		avcencMotionBits(motion);
		approximate = (uint16_t)(motion->bits[0] + motion->bits[1]) + 5;
		avcencMotionCodeBits(motion);
		bits = (uint16_t)(motion->bits[0] + motion->bits[1]) + 5;
		copyLanes(motion->vectors[1], motion->vectors[0]);
		break;
	default:
		motionPredict8x8(info, motion);
		avcencMotionBits8x8(motion);
		approximate = (uint16_t)(motion->bits[0] + motion->bits[1]) + 11;
		avcencMotionCodeBits8x8(motion);
		bits = (uint16_t)(motion->bits[0] + motion->bits[1]) + 11;
		break;
	}
	info->interBits = bits;
	info->interSatd = satd;
	info->interCost = satd + weight * approximate;
	copyLanes(interRecord->vectors[0], motion->vectors[0]);
	copyLanes(interRecord->vectors[2], motion->vectors[1]);
	copyLanes(interRecord->differences[0], motion->differences[0]);
	copyLanes(interRecord->differences[2], motion->differences[1]);
}

/* the differences of the first two blocks' vectors from their predictions */
static inline void motionDifferences(AvcencMotion *motion, uint32_t pair)
{
	uint32_t i;

	for (i = 0; i < 4; i++)
		motion->differences[pair][i] = motion->vectors[pair][i] -
			motion->predictions[pair][i];
}

/*
 * the corner neighbour, available or not: above right (its block 2), at
 * the row's end the block of the macroblock given (above left's 3, or
 * above's 2)
 */
static void motionCorner(AvcencMotionInfo *info, AvcencMotion *motion, int16_t *lanes,
	uint32_t record, uint32_t interRecord, uint32_t block)
{
	motion->unavailable |= 1 << MOTION_CORNER;
	if (!lastColumn(info)) {
		if (recordIntra(info->recordAboveRight))
			return;
		loadVector(lanes, info->interAboveRight, 2);
	} else {
		if (recordIntra(record))
			return;
		loadVector(lanes, interRecord, block);
	}
	motion->unavailable &= ~(1 << MOTION_CORNER);
}

/* the left neighbour, its blocks 1 and 3 */
static uint32_t motionLeft(AvcencMotionInfo *info, AvcencMotion *motion, int16_t *right,
	int16_t *rightBelow)
{
	motion->unavailable = 1 << MOTION_LEFT;
	if (info->mbX < 1 || recordIntra(info->recordLeft))
		return 0;
	loadVector(right, info->interLeft, 1);
	if (rightBelow != 0)
		loadVector(rightBelow, info->interLeft, 3);
	motion->unavailable = 0;
	return 1;
}

/*
 * 0x83baa8: a 16x16 partition's prediction: the median of the left
 * neighbour, the one above and the corner; on a slice's first row the left
 * one's (or none)
 */
static void motionPredict16x16(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int16_t (*neighbours)[4] = motion->neighbours[0];

	motionLeft(info, motion, neighbours[MOTION_LEFT], 0);
	if (info->sliceStart) {
		if (motion->unavailable & 1 << MOTION_LEFT)
			clearLanes(motion->predictions[0]);
		else
			copyLanes(motion->predictions[0], neighbours[MOTION_LEFT]);
	} else {
		motion->unavailable |= 1 << MOTION_ABOVE;
		if (!recordIntra(info->recordAbove)) {
			loadVector(neighbours[MOTION_ABOVE], info->interAbove, 2);
			motion->unavailable &= ~(1 << MOTION_ABOVE);
		}
		motionCorner(info, motion, neighbours[MOTION_CORNER], info->recordAboveLeft,
			info->interAboveLeft, 3);
		avcencMotionMedian(motion);
	}
	motionDifferences(motion, 0);
}

/*
 * 0x83bb6c: two 16x8 partitions': the top one's is the one above's
 * (without it the median), the bottom one's the left one's block 3, without
 * it the top partition's vector
 */
static void motionPredict16x8(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int16_t (*neighbours)[4] = motion->neighbours[0], *lower = motion->neighbours[1][MOTION_LEFT];
	uint32_t i;

	motionLeft(info, motion, neighbours[MOTION_LEFT], lower);
	if (info->sliceStart) {
		if (motion->unavailable & 1 << MOTION_LEFT)
			clearLanes(motion->predictions[0]);
		else
			copyLanes(motion->predictions[0], neighbours[MOTION_LEFT]);
	} else if (!recordIntra(info->recordAbove)) {
		loadVector(motion->predictions[0], info->interAbove, 2);
	} else {
		motion->unavailable |= 1 << MOTION_ABOVE;
		motionCorner(info, motion, neighbours[MOTION_CORNER], info->recordAboveLeft,
			info->interAboveLeft, 3);
		avcencMotionMedian(motion);
	}
	if (motion->unavailable & 1 << MOTION_LEFT)
		copyLanes(lower, motion->vectors[0]);
	for (i = 0; i < 2; i++)
		motion->predictions[0][2 + i] = lower[i];
	motionDifferences(motion, 0);
}

/*
 * 0x83bc42: two 8x16 partitions': the left one's is the left neighbour's
 * (without it the median of the above's blocks 2 and 3), the right one's
 * the corner's (without it the median of the left partition's vector, the
 * above's block 3 and none)
 */
static void motionPredict8x16(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int16_t (*first)[4] = motion->neighbours[0], (*second)[4] = motion->neighbours[1];
	uint32_t aboveIntra = 1;

	if (!info->sliceStart && !recordIntra(info->recordAbove)) {
		loadVector(first[MOTION_ABOVE], info->interAbove, 2);
		loadVector(first[MOTION_CORNER], info->interAbove, 3);
		aboveIntra = 0;
	}
	if (aboveIntra) {
		clearLanes(first[MOTION_ABOVE]);
		clearLanes(first[MOTION_CORNER]);
	}
	if (!motionLeft(info, motion, first[MOTION_LEFT], 0))
		clearLanes(first[MOTION_LEFT]);
	else
		copyLanes(first[MOTION_CORNER], first[MOTION_LEFT]);
	copyLanes(second[MOTION_LEFT], motion->vectors[0]);
	if (!info->sliceStart) {
		if (!recordIntra(info->recordAbove))
			loadVector(second[MOTION_ABOVE], info->interAbove, 3);
		if (aboveIntra)
			copyLanes(second[MOTION_ABOVE], motion->vectors[0]);
		motionCorner(info, motion, second[MOTION_CORNER], info->recordAbove,
			info->interAbove, 2);
		if (motion->unavailable & 1 << MOTION_CORNER)
			clearLanes(second[MOTION_CORNER]);
		else
			copyLanes(second[MOTION_ABOVE], second[MOTION_CORNER]);
	} else {
		copyLanes(second[MOTION_ABOVE], motion->vectors[0]);
	}
	avcencMotionMedianPairs(motion);
	motionDifferences(motion, 0);
}

/*
 * 0x83bd58: four 8x8 blocks': each the median of its neighbours; without
 * the macroblock above, blocks 0's are the left one's, block 1's corner
 * its left block's vector without the above right one too
 */
static void motionPredict8x8(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int16_t (*first)[4] = motion->neighbours[0], (*second)[4] = motion->neighbours[1];
	uint32_t aboveIntra = 1;

	if (!motionLeft(info, motion, first[MOTION_LEFT], motion->leftBelow))
		clearLanes(first[MOTION_LEFT]);
	if (!info->sliceStart && !recordIntra(info->recordAbove)) {
		loadVector(first[MOTION_ABOVE], info->interAbove, 2);
		loadVector(first[MOTION_CORNER], info->interAbove, 3);
		aboveIntra = 0;
	}
	copyLanes(second[MOTION_LEFT], motion->vectors[0]);
	if (aboveIntra) {
		copyLanes(first[MOTION_ABOVE], first[MOTION_LEFT]);
		copyLanes(first[MOTION_CORNER], first[MOTION_LEFT]);
	}
	if (!info->sliceStart) {
		if (!recordIntra(info->recordAbove))
			loadVector(second[MOTION_ABOVE], info->interAbove, 3);
		if (aboveIntra)
			clearLanes(second[MOTION_ABOVE]);
		motionCorner(info, motion, second[MOTION_CORNER], info->recordAbove,
			info->interAbove, 2);
		if (motion->unavailable & 1 << MOTION_CORNER)
			clearLanes(second[MOTION_CORNER]);
		if (aboveIntra && motion->unavailable & 1 << MOTION_CORNER)
			copyLanes(second[MOTION_CORNER], motion->vectors[0]);
	} else {
		copyLanes(second[MOTION_ABOVE], motion->vectors[0]);
	}
	avcencMotionMedianPairs(motion);
	avcencMotionMedianLower(motion);
	motionDifferences(motion, 1);
	motionDifferences(motion, 0);
}

/*
 * 0x83be80: P_Skip's vector, the median of the neighbours (an intra one
 * zero); zero without the left one or the one above, or when either's is
 * zero
 */
static void motionSkipVector(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int16_t (*neighbours)[4] = motion->neighbours[0];
	int32_t vector = 1;

	if (info->mbX < 1 || info->sliceStart) {
		info->skipVector[0] = 0;
		info->skipVector[1] = 0;
		return;
	}
	motion->unavailable = 0;
	if (recordIntra(info->recordLeft)) {
		motion->unavailable |= 1 << MOTION_LEFT;
		clearLanes(neighbours[MOTION_LEFT]);
	} else {
		vector = *(const int32_t *)((AvcencInterRecord *)info->interLeft)->vectors[1];
		loadVector(neighbours[MOTION_LEFT], info->interLeft, 1);
		if (vector == 0)
			goto zero;
	}
	if (recordIntra(info->recordAbove)) {
		clearLanes(neighbours[MOTION_ABOVE]);
		motion->unavailable |= 1 << MOTION_ABOVE;
	} else {
		vector = *(const int32_t *)((AvcencInterRecord *)info->interAbove)->vectors[2];
		loadVector(neighbours[MOTION_ABOVE], info->interAbove, 2);
	}
	if (vector == 0)
		goto zero;
	motionCorner(info, motion, neighbours[MOTION_CORNER], info->recordAboveLeft,
		info->interAboveLeft, 3);
	if (motion->unavailable & 1 << MOTION_CORNER)
		clearLanes(neighbours[MOTION_CORNER]);
	avcencMotionSkipMedian(motion, info->skipVector);
	return;
zero:
	info->skipVector[0] = 0;
	info->skipVector[1] = 0;
}

/* 0x83bf66: a band of the reference's luma, 16 pixels from a column */
static void motionLoadLuma(AvcencMotion *motion, uint32_t reference, int32_t pitch, int32_t x,
	int32_t y)
{
	uint32_t first = reference + (y >> 4) * pitch * 16 + (x >> 4) * 256 + (y & 15) * 16 +
		(x & 8), second = x & 8 ? first + 248 : first + 8;

	if (x & 7)
		avcencMotionLoadLumaShifted(motion, (const uint8_t *)first, (const uint8_t *)second,
			pitch, y & 15, x & 7);
	else
		avcencMotionLoadLuma(motion, (const uint8_t *)first, (const uint8_t *)second, pitch,
			y & 15);
}

/* 0x83bfb4: a band of its chroma, eight pixels of U and V from a column */
static void motionLoadChroma(AvcencMotion *motion, uint32_t reference, int32_t pitch,
	int32_t x, int32_t y)
{
	uint32_t first = reference + (y >> 3) * pitch * 8 + (x >> 3) * 128 + (y & 7) * 8;

	if (x & 7)
		avcencMotionLoadChromaShifted(motion, (const uint8_t *)first,
			(const uint8_t *)first + 128, pitch, y & 7, x & 7);
	else
		avcencMotionLoadChroma(motion, (const uint8_t *)first, pitch, y & 7);
}

/* the band's top line: in the picture, its last four lines at most */
static inline int32_t bandTop(int32_t y, int32_t height)
{
	if (y < 0)
		return 0;
	return y < height - 4 ? y : height - 4;
}

/*
 * 0x83bff8: P_Skip's check: the source less the reference at the skip
 * vector (its whole pixels), each 4x4 block transformed, below the
 * thresholds of the QP: the luma's and two of the chroma's eight. Not on
 * the picture's first row or column
 */
static uint32_t motionSkipCheck(AvcencMotionInfo *info, AvcencMotion *motion)
{
	int32_t x, y, width, height, left, band, i, shift;

	if ((int8_t)info->mbX == 0 || (int8_t)info->mbY == 0)
		return 0;
	shift = (int8_t)info->lumaShift2 & 15;
	for (i = 0; i < 4; i++) {
		motion->thresholds[0][i] = (uint16_t)info->lumaThreshold2[i] >> shift;
		motion->thresholds[1][i] = (uint16_t)info->lumaThreshold2[4 + i] >> shift;
	}
	width = info->width;
	height = info->height;
	x = info->mbX * 16 + (info->skipVector[0] >> 2);
	y = info->mbY * 16 + (info->skipVector[1] >> 2);
	left = 0;
	if (x >= 0)
		left = x < width - 16 ? x : width - 16;
	for (band = 0; band < 4; band++) {
		motionLoadLuma(motion, info->reference, info->pitch, left,
			bandTop(y + band * 4, height));
		avcencMotionEdgeLuma(motion, x, y + band * 4, width, height);
		if (!avcencMotionSkipLuma(motion, (const uint8_t *)info->luma + band * 64))
			return 0;
	}

	height >>= 1;
	x >>= 1;
	width >>= 1;
	y >>= 1;
	shift = (int8_t)info->chromaShift & 15;
	for (i = 0; i < 4; i++) {
		motion->thresholds[0][i] = (uint16_t)info->chromaThreshold[i] >> shift;
		motion->thresholds[1][i] = (uint16_t)info->chromaThreshold[4 + i] >> shift;
	}
	left = 0;
	if (x >= 0)
		left = x < width - 8 ? x : width - 8;
	for (band = 0; band < 2; band++) {
		motionLoadChroma(motion, info->referenceChroma, info->chromaPitch, left,
			bandTop(y + band * 4, height));
		avcencMotionEdgeChroma(motion, x, y + band * 4, width, height);
		if (!avcencMotionSkipChroma(motion, (const uint8_t *)info->chroma + band * 32,
				band == 0))
			return 0;
	}
	return 1;
}

/* P_Skip taken: its vector in each block's, no differences */
static void motionSkip(AvcencMotionInfo *info, uint32_t type)
{
	AvcencInterRecord *interRecord = (AvcencInterRecord *)info->interRecord;
	AvcencMacroblock *record = (AvcencMacroblock *)info->record;
	uint32_t block;

	for (block = 0; block < 4; block++) {
		interRecord->vectors[block][0] = info->skipVector[0];
		interRecord->vectors[block][1] = info->skipVector[1];
		record->differences[block][0] = 0;
		record->differences[block][1] = 0;
		record->vectors[block][0] = info->skipVector[0];
		record->vectors[block][1] = info->skipVector[1];
	}
	interRecord->partition = SKIP_PARTITION;
	record->type = type | MACROBLOCK_CHECKED;
}

/* 0x83c210: P_Skip, when its check passes; whether it does */
static uint32_t motionSkipFound(AvcencMotionInfo *info, AvcencMotion *motion)
{
	AvcencMotionStats *stats;

	if (!motionSkipCheck(info, motion))
		return 0;
	stats = info->stats;
	stats->skipCount++;
	stats->skipFoundCount++;
	motionSkip(info, skipTypes[SKIP_PARTITION]);
	return 1;
}

/*
 * 0x83c27e: a macroblock the inter decision skips: P_Skip when its vector
 * is the skip vector (in whole pixels) or the check passes, else a 16x16
 * partition
 */
static void motionSkipDecided(AvcencMotionInfo *info, AvcencMotion *motion)
{
	AvcencInterRecord *interRecord = (AvcencInterRecord *)info->interRecord;
	AvcencMacroblock *record = (AvcencMacroblock *)info->record;
	AvcencMotionStats *stats = info->stats;
	int16_t vector[2];
	uint32_t block;

	vector[0] = interRecord->vector[0] * 4 + interRecord->vectors[0][0];
	vector[1] = interRecord->vector[1] * 4 + interRecord->vectors[0][1];
	if ((info->skipVector[0] & ~3) == vector[0] && (info->skipVector[1] & ~3) == vector[1]) {
		stats->skipCount++;
	} else if (motionSkipCheck(info, motion)) {
		stats->skipCount++;
		stats->skipDecidedCount++;
	} else {
		motionInter(info, motion);
		stats->interCount++;
		stats->bits += info->interBits;
		for (block = 0; block < 4; block++) {
			record->differences[block][0] = interRecord->differences[block & 1][0];
			record->differences[block][1] = interRecord->differences[block & 1][1];
			record->vectors[block][0] = interRecord->vectors[block & 1][0];
			record->vectors[block][1] = interRecord->vectors[block & 1][1];
		}
		interRecord->partition = 0;
		record->type = skipTypes[0] | MACROBLOCK_CHECKED;
		return;
	}
	motionSkip(info, skipTypes[SKIP_PARTITION]);
}

/*
 * 0x83b34c: the motion job: the final choice of a band of a row of groups'
 * macroblocks, on the wavefront (rows of up to eight, each one a column
 * left of the last). The first job clears the statistics, the last sums
 * them; the jobs it lets start are signalled, the step's count of jobs
 * running taken down
 */
void avcencJobMotion(uint32_t pictureAddress, uint32_t slicesAddress, uint32_t position,
	uint32_t unused)
{
	AvcencPicture *picture = (AvcencPicture *)pictureAddress;
	uint32_t core = ((int32_t)vnzReadControl(VNZ_CR_ID) >> 16) & 7;
	AvcencSlices *slices = (AvcencSlices *)vnzPublicOpenProtected(slicesAddress, 64);
	int32_t column = position & 0xff, row = (position >> 8) & 0xff, width, height, last;
	int32_t lastRow, endRow, first, x, y, i, job, step;
	AvcencFrameRecord *frameRecord;
	AvcencInterRecord *interRecord;
	AvcencMotionStats *stats;
	AvcencMotionInfo *info;
	AvcencMotion motion;
	VkVThreadContext *context;
	uint32_t total;

	info = (AvcencMotionInfo *)vnzPublicOpenPrivate(slices->streamInfo +
		core * CORE_INFO_SIZE, MOTION_INFO_SIZE);
	vnzCacheInvalidateInside((uint32_t)info, MOTION_INFO_SIZE);
	clearLanes(motion.differences[1]);
	height = picture->heightInMbs;
	width = picture->widthInMbs;
	if (column + 7 >= width - 1)
		last = width - 1;
	else if (column != 0)
		last = column + 7;
	else if ((row & 7) == 6)
		last = 1;
	else if ((row & 7) == 4)
		last = 3;
	else if ((row & 7) == 2)
		last = 5;
	else
		last = 7;
	lastRow = height - 1;
	endRow = row + 1 < lastRow ? row + 1 : lastRow;
	first = column;
	if (column == 0 && row == 0) {
		stats = (AvcencMotionStats *)vnzPublicOpenPrivate(picture->rowsMotion, 1024);
		for (i = 0; i < 16; i++) {
			stats[i].cost = 0;
			stats[i].bits = 0;
			stats[i].average = 0;
			stats[i].intraCount = 0;
			stats[i].intra16x16Count = 0;
			stats[i].interCount = 0;
			stats[i].reserved = 0;
			stats[i].skipCount = 0;
			stats[i].skipDecidedCount = 0;
			stats[i].skipFoundCount = 0;
		}
		vnzPublicClosePrivate((uint32_t)stats, 1024);
	}
	motionSetup(picture, slices, info, row);
	for (y = row; y <= endRow; y++) {
		motionRow(picture, slices, info, first, y, last - first + 1);
		for (x = first; x <= last; x++) {
			if ((slices->sliceType & 7) == 2) {
				motionModes(info);
				motionChoose(info, 1);
			} else {
				interRecord = (AvcencInterRecord *)info->interRecord;
				if (interRecord->skip) {
					motionSkipVector(info, &motion);
					motionSkipDecided(info, &motion);
				} else if (interRecord->intra) {
					motionModes(info);
					motionChoose(info, 0);
				} else {
					motionSkipVector(info, &motion);
					if (!motionSkipFound(info, &motion)) {
						if (((AvcencMacroblock *)info->macroblock)->intraCost !=
								0xffff)
							motionModes(info);
						motionInter(info, &motion);
						motionChoose(info, 0);
					}
				}
			}
			motionNext(picture, info);
		}
		motionRowClose(picture, slices, info);
		if (first >= 1)
			first--;
		if (last != width - 1)
			last--;
	}
	motionStatsClose(picture, slices, info);

	if (last == width - 1 && endRow == lastRow) {
		stats = (AvcencMotionStats *)vnzPublicOpenPrivate(picture->rowsMotion, 1024);
		for (i = 1; i < 16; i++) {
			stats[0].cost += stats[i].cost;
			stats[0].bits += stats[i].bits;
			stats[0].average += stats[i].average;
			stats[0].intraCount += stats[i].intraCount;
			stats[0].intra16x16Count += stats[i].intra16x16Count;
			stats[0].interCount += stats[i].interCount;
			stats[0].reserved += stats[i].reserved;
			stats[0].skipCount += stats[i].skipCount;
			stats[0].skipDecidedCount += stats[i].skipDecidedCount;
			stats[0].skipFoundCount += stats[i].skipFoundCount;
		}
		total = picture->widthInMbs * picture->heightInMbs;
		stats[0].average /= total;
		vnzPublicClosePrivate((uint32_t)stats, 1024);
	}

	/* the jobs it lets start: the rows of groups below's, the next band's */
	frameRecord = (AvcencFrameRecord *)picture->frameRecord;
	context = vkVThreadContextLock((VkVThreadContext *)picture->context);
	step = (row + 2) >> 1;
	if (width < 9) {
		if (row + 2 < height)
			job = picture->index * 8 + picture->jobArgument +
				picture->jobGroups * (step & 7) + (((column >> 1) + (step & 3)) >> 2) + 8;
		else
			job = picture->jobs * 8 + picture->index * 8 + 9;
		vkVThreadJobSignal(context, job);
	} else {
		if (row + 2 < height && ((row & 6) != 6 || column >= 1))
			vkVThreadJobSignal(context, picture->index * 8 + picture->jobArgument +
				picture->jobGroups * (step & 7) + ((((column - 2) >> 1) + (step & 3)) >> 2) +
				8);
		if (column + 8 < width) {
			step = row >> 1;
			vkVThreadJobSignal(context, picture->index * 8 + picture->jobArgument +
				picture->jobGroups * (step & 7) + ((((column + 8) >> 1) + (step & 3)) >> 2) +
				8);
		} else if (row + 2 >= height) {
			vkVThreadJobSignal(context, picture->jobs * 8 + picture->index * 8 + 9);
		}
	}
	vkVThreadContextUnlock(context);
	i = ((column >> 1) + (row >> 1) * 5) >> 2;
	vnzSpinLock(picture->lock);
	frameRecord->started[i & 7]--;
	vnzSpinUnlock(picture->lock);
	vnzCachePurge((uint32_t)info, MOTION_INFO_SIZE);
	vnzPublicClosePrivate((uint32_t)info, MOTION_INFO_SIZE);
	vnzPublicCloseProtected((uint32_t)slices, 64);
}
