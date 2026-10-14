/*
 * PU1: where the pictures and their motion data go in the work memory.
 *
 * The units address the work memory as rows of 8 KB (VIP_WORK_PITCH): an address
 * is the column in 8-byte units from bit 19 up and the row times 8 below it
 * (WORK_AT). A picture takes 3/4 of its height plus 4 rows, at 2 bytes and 64 more
 * per pixel column; its motion data takes 5 rows per macroblock row.
 *
 * For the stream kinds the ARM side names, each size of picture has a fixed
 * layout (layoutFrames): so many frame stores in a set, the pictures' places,
 * then their motion data's. A stream opened after queryWorkMemory is laid out
 * from the queried size instead (layoutQueried).
 */
#include "codec.h"

#define VIP_WORK_PITCH		0x2000
#define WORK_AT(column, row)	((uint32_t)(column) / 8 << 19 | (uint32_t)(row) * 8)
#define WORK_COLUMN(bytes)	((uint32_t)(bytes) / 8 << 19)
#define WORK_ROWS(rows)		((uint32_t)(rows) * 8)

/* the most a picture may be, in macroblocks */
#define MAX_WIDTH_MBS		120
#define MAX_HEIGHT_MBS		68

/* queryWorkMemory: the most frame stores and work memory */
#define QUERY_MAX_FRAMES	20
#define QUERY_MAX_SIZE		0x2000000
#define QUERY_MARGIN		64	/* bytes kept at the end of each 8 KB row */

/* clearing a picture's motion data: a fill of DRAM from local RAM (the bits above
   VIP_DMA_LIST are not known) */
#define CLEAR_MOTION_DMA	(VIP_DMA_LIST | VIP_DMA_FILL | 0x31000)

#define AT WORK_AT

static const uint32_t g_layoutStandard[] = {
	/* the pictures */
	AT(0, 0), AT(4096, 0), AT(0, 820), AT(4096, 820),
	AT(0, 1640), AT(4096, 1640),
	/* their motion data */
	AT(0, 2460), AT(4096, 2460), AT(0, 2596), AT(4096, 2596),
	AT(0, 2732), AT(4096, 2732),
};

static const uint32_t g_layout1920Tall0[] = {
	/* the pictures */
	AT(0, 0), AT(4096, 0), AT(0, 820), AT(4096, 820),
	AT(0, 1640), AT(4096, 1640), AT(4096, 2460),
	/* their motion data */
	AT(0, 2460), AT(2048, 2460), AT(0, 2732), AT(2048, 2732),
	AT(0, 3004), AT(2048, 3004),
};

static const uint32_t g_layout1920Tall1[] = {
	/* the pictures */
	AT(0, 3280), AT(4096, 3280), AT(0, 4100), AT(4096, 4100),
	AT(0, 4920), AT(4096, 4920),
	/* their motion data */
	AT(4096, 2460), AT(6144, 2460), AT(4096, 2732), AT(6144, 2732),
	AT(4096, 3004), AT(6144, 3004),
};

static const uint32_t g_layout1440Tall0[] = {
	/* the pictures */
	AT(0, 0), AT(3008, 0), AT(0, 820), AT(3008, 820),
	AT(0, 1640), AT(3008, 1640), AT(0, 2460), AT(3008, 2460),
	/* their motion data */
	AT(6016, 0), AT(6016, 272), AT(6016, 544), AT(6016, 816),
	AT(6016, 1088), AT(6016, 1360), AT(6016, 1632),
};

static const uint32_t g_layout1440Tall1[] = {
	/* the pictures */
	AT(4096, 2460), AT(0, 3280), AT(3008, 3280), AT(0, 4100),
	AT(3008, 4100), AT(0, 4920), AT(3008, 4920),
	/* their motion data */
	AT(6016, 3280), AT(6016, 3552), AT(6016, 3824), AT(6016, 4100),
	AT(6016, 4372), AT(6016, 4644), AT(6016, 4916),
};

static const uint32_t g_layout1280Tall0[] = {
	/* the pictures */
	AT(0, 0), AT(2688, 0), AT(5376, 0), AT(0, 820),
	AT(2688, 820), AT(5376, 820), AT(0, 1640), AT(2688, 1640),
	AT(2688, 2460),
	/* their motion data */
	AT(5376, 1640), AT(6720, 1640), AT(5376, 1912), AT(6720, 1912),
	AT(5376, 2184), AT(6720, 2184), AT(0, 2460), AT(1344, 2460),
};

static const uint32_t g_layout1280Tall1[] = {
	/* the pictures */
	AT(0, 3280), AT(2688, 3280), AT(5376, 3280), AT(0, 4100),
	AT(2688, 4100), AT(5376, 4100), AT(0, 4920), AT(2688, 4920),
	/* their motion data */
	AT(5376, 4920), AT(6720, 4920), AT(5376, 5192), AT(6720, 5192),
	AT(5376, 5464), AT(6720, 5464), AT(5376, 2460), AT(6720, 2460),
};

static const uint32_t g_layout960Tall0[] = {
	/* the pictures */
	AT(0, 0), AT(2048, 0), AT(4096, 0), AT(6144, 0),
	AT(0, 820), AT(2048, 820), AT(4096, 820), AT(6144, 820),
	AT(0, 1640), AT(2048, 1640), AT(0, 2460),
	/* their motion data */
	AT(4096, 1640), AT(5120, 1640), AT(6144, 1640), AT(7168, 1640),
	AT(4096, 1912), AT(5120, 1912), AT(6144, 1912), AT(7168, 1912),
	AT(4096, 2184), AT(5120, 2184),
};

static const uint32_t g_layout960Tall1[] = {
	/* the pictures */
	AT(0, 3280), AT(2048, 3280), AT(4096, 3280), AT(6144, 3280),
	AT(0, 4100), AT(2048, 4100), AT(4096, 4100), AT(6144, 4100),
	AT(0, 4920), AT(2048, 4920),
	/* their motion data */
	AT(4096, 4920), AT(5120, 4920), AT(6144, 4920), AT(7168, 4920),
	AT(4096, 5192), AT(5120, 5192), AT(6144, 5192), AT(7168, 5192),
	AT(4096, 5464), AT(5120, 5464),
};

static const uint32_t g_layout1920Short0[] = {
	/* the pictures */
	AT(0, 0), AT(4096, 0), AT(0, 820), AT(4096, 820),
	AT(0, 1640), AT(4096, 1640), AT(4096, 2460),
	/* their motion data */
	AT(0, 2460), AT(2048, 2460), AT(0, 2732), AT(2048, 2732),
	AT(0, 3004), AT(2048, 3004),
};

static const uint32_t g_layout1920Short1[] = {
	/* the pictures */
	AT(0, 3280), AT(4096, 3280), AT(0, 4100), AT(4096, 4100),
	AT(0, 4920), AT(4096, 4920),
	/* their motion data */
	AT(4096, 2460), AT(6144, 2460), AT(4096, 2732), AT(6144, 2732),
	AT(4096, 3004), AT(6144, 3004),
};

static const uint32_t g_layout1280Short0[] = {
	/* the pictures */
	AT(0, 0), AT(2688, 0), AT(5376, 0), AT(0, 556),
	AT(2688, 556), AT(5376, 556), AT(0, 1112), AT(2688, 1112),
	AT(5376, 1112), AT(0, 1668), AT(2688, 1668), AT(0, 2460),
	/* their motion data */
	AT(5376, 1668), AT(6720, 1668), AT(5376, 1852), AT(6720, 1852),
	AT(5376, 2036), AT(6720, 2036), AT(0, 2224), AT(1344, 2224),
	AT(2688, 2224), AT(4032, 2224), AT(5376, 2224),
};

static const uint32_t g_layout1280Short1[] = {
	/* the pictures */
	AT(0, 3280), AT(2688, 3280), AT(5376, 3280), AT(0, 3836),
	AT(2688, 3836), AT(5376, 3836), AT(0, 4392), AT(2688, 4392),
	AT(5376, 4392), AT(0, 4948), AT(2688, 4948),
	/* their motion data */
	AT(5376, 4948), AT(6720, 340), AT(5376, 5132), AT(6720, 5132),
	AT(5376, 5316), AT(6720, 5316), AT(0, 5504), AT(1344, 5504),
	AT(2688, 5504), AT(4032, 5504), AT(5376, 5504),
};

static const uint32_t g_layout960Medium0[] = {
	/* the pictures */
	AT(0, 0), AT(2048, 0), AT(4096, 0), AT(6144, 0),
	AT(0, 556), AT(2048, 556), AT(4096, 556), AT(6144, 556),
	AT(0, 1112), AT(2048, 1112), AT(4096, 1112), AT(6144, 1112),
	AT(0, 1668), AT(2048, 1668), AT(0, 2460),
	/* their motion data */
	AT(4096, 1668), AT(5120, 1668), AT(6144, 1668), AT(7168, 1668),
	AT(4096, 1852), AT(5120, 1852), AT(6144, 1852), AT(7168, 1852),
	AT(4096, 2036), AT(5120, 2036), AT(6144, 2036), AT(7168, 2036),
	AT(0, 2224), AT(1024, 2224),
};

static const uint32_t g_layout960Medium1[] = {
	/* the pictures */
	AT(0, 3280), AT(2048, 3280), AT(4096, 3280), AT(6144, 3280),
	AT(0, 3836), AT(2048, 3836), AT(4096, 3836), AT(6144, 3836),
	AT(0, 4392), AT(2048, 4392), AT(4096, 4392), AT(6144, 4392),
	AT(0, 4948), AT(2048, 4948),
	/* their motion data */
	AT(4096, 4948), AT(5120, 4948), AT(6144, 4948), AT(7168, 4948),
	AT(4096, 5132), AT(5120, 5132), AT(6144, 5132), AT(7168, 5132),
	AT(4096, 5316), AT(5120, 5316), AT(6144, 5316), AT(7168, 5316),
	AT(0, 5504), AT(1024, 5504),
};

static const uint32_t g_layout960Short0[] = {
	/* the pictures */
	AT(0, 0), AT(2048, 0), AT(4096, 0), AT(6144, 0),
	AT(0, 412), AT(2048, 412), AT(4096, 412), AT(6144, 412),
	AT(0, 824), AT(2048, 824), AT(4096, 824), AT(6144, 824),
	AT(0, 1236), AT(2048, 1236), AT(4096, 1236), AT(6144, 1236),
	AT(0, 1648), AT(2048, 1648), AT(0, 2460),
	/* their motion data */
	AT(4096, 1648), AT(5120, 1648), AT(6144, 1648), AT(7168, 1648),
	AT(4096, 1784), AT(5120, 1784), AT(6144, 1784), AT(7168, 1784),
	AT(4096, 1920), AT(5120, 1920), AT(6144, 1920), AT(7168, 1920),
	AT(0, 2060), AT(1024, 2060), AT(2048, 2060), AT(3072, 2060),
	AT(4096, 2060), AT(5120, 2060),
};

static const uint32_t g_layout960Short1[] = {
	/* the pictures */
	AT(0, 3280), AT(2048, 3280), AT(4096, 3280), AT(6144, 3280),
	AT(0, 3692), AT(2048, 3692), AT(4096, 3692), AT(6144, 3692),
	AT(0, 4104), AT(2048, 4104), AT(4096, 4104), AT(6144, 4104),
	AT(0, 4516), AT(2048, 4516), AT(4096, 4516), AT(6144, 4516),
	AT(0, 4928), AT(2048, 4928),
	/* their motion data */
	AT(4096, 4928), AT(5120, 4928), AT(6144, 4928), AT(7168, 4928),
	AT(4096, 5064), AT(5120, 5064), AT(6144, 5064), AT(7168, 5064),
	AT(4096, 5200), AT(5120, 5200), AT(6144, 5200), AT(7168, 5200),
	AT(0, 5340), AT(1024, 5340), AT(2048, 5340), AT(3072, 5340),
	AT(4096, 5340), AT(5120, 5340),
};

static const uint32_t g_layout720Short0[] = {
	/* the pictures */
	AT(0, 0), AT(1568, 0), AT(3136, 0), AT(4704, 0),
	AT(6272, 0), AT(0, 544), AT(1568, 544), AT(3136, 544),
	AT(4704, 544), AT(6272, 544), AT(0, 1088), AT(1568, 1088),
	AT(3136, 1088), AT(4704, 1088), AT(6272, 1088), AT(0, 1632),
	AT(1568, 1632), AT(3136, 1632), AT(0, 2460),
	/* their motion data */
	AT(4704, 1638), AT(5488, 1638), AT(6272, 1638), AT(7056, 1638),
	AT(4704, 1904), AT(5488, 1904), AT(6272, 1904), AT(7056, 1904),
	AT(0, 2176), AT(784, 2176), AT(1568, 2176), AT(2352, 2176),
	AT(3136, 2176), AT(3920, 2176), AT(4704, 2176), AT(5488, 2176),
	AT(6272, 2176), AT(7056, 2176),
};

static const uint32_t g_layout720Short1[] = {
	/* the pictures */
	AT(0, 3280), AT(1568, 3280), AT(3136, 3280), AT(4704, 3280),
	AT(6272, 3280), AT(0, 3824), AT(1568, 3824), AT(3136, 3824),
	AT(4704, 3824), AT(6272, 3824), AT(0, 4368), AT(1568, 4368),
	AT(3136, 4368), AT(4704, 4368), AT(6272, 4368), AT(0, 4912),
	AT(1568, 4912), AT(3136, 4912), AT(0, 0),
	/* their motion data */
	AT(4704, 4912), AT(5488, 4912), AT(6272, 4912), AT(7056, 4912),
	AT(4704, 5184), AT(5488, 5184), AT(6272, 5184), AT(7056, 5184),
	AT(0, 5456), AT(784, 5456), AT(1568, 5456), AT(2352, 5456),
	AT(3136, 5456), AT(3920, 5456), AT(4704, 5456), AT(5488, 5456),
	AT(6272, 5456), AT(7056, 5456),
};
#undef AT

/* a size of picture: its layouts in the two sets of frame stores */
typedef struct VipLayout {
	uint8_t minWidthMbs;		/* the class: at least this wide ... */
	uint8_t minHeightMbs;		/* ... and tall */
	uint8_t count;			/* frame stores in a set */
	uint16_t width;			/* of the frame stores */
	uint16_t height;
	const uint32_t *pictures[2];	/* by set; set 0 has one more picture, for */
	uint8_t motion[2];		/* VIP_MEMORY_SIZED; where the motion data start */
} VipLayout;

static const VipLayout g_layouts[] = {
	{ 91, 47, 6, 1920, 1088, { g_layout1920Tall0, g_layout1920Tall1 }, { 7, 6 } },
	{ 81, 47, 7, 1440, 1088, { g_layout1440Tall0, g_layout1440Tall1 }, { 8, 7 } },
	{ 61, 47, 8, 1280, 1088, { g_layout1280Tall0, g_layout1280Tall1 }, { 9, 8 } },
	{ 0, 47, 10, 960, 1088, { g_layout960Tall0, g_layout960Tall1 }, { 11, 10 } },
	{ 81, 0, 6, 1920, 1088, { g_layout1920Short0, g_layout1920Short1 }, { 7, 6 } },
	{ 61, 0, 11, 1280, 720, { g_layout1280Short0, g_layout1280Short1 }, { 12, 11 } },
	{ 46, 35, 14, 960, 720, { g_layout960Medium0, g_layout960Medium1 }, { 15, 14 } },
	{ 46, 0, 18, 960, 544, { g_layout960Short0, g_layout960Short1 }, { 19, 18 } },
	{ 0, 0, 18, 720, 720, { g_layout720Short0, g_layout720Short1 }, { 19, 19 } },
};

uint32_t g_motionData[VIP_FRAME_SET];
uint8_t g_bufferCount;
VipMemoryQuery g_memoryQuery;

static void placePicture(uint32_t i, uint32_t offset)
{
	setFramePicture(g_frameHandles[i], g_workBase + offset);
	g_buffers[i].picture = g_workBase + offset;
}

/* The layout of the stream kinds: VIP_MEMORY_STANDARD has one for every size. */
static void layoutStandard(void)
{
	const uint32_t *layout = g_layoutStandard;
	uint32_t i;

	g_bufferCount = 6;
	for (i = 0; i < g_bufferCount; i++) {
		placePicture(i, layout[i]);
		g_motionData[i] = g_workBase + layout[g_bufferCount + i];
		g_buffers[i].colocated = g_workBase + layout[g_bufferCount + i];
	}
}

static void layoutClass(const VipLayout *class, uint32_t mode)
{
	uint32_t set = g_frameSet ? 1 : 0, first = set * VIP_FRAME_SET, i;
	const uint32_t *layout = class->pictures[set];

	g_bufferCount = class->count;
	for (i = 0; i < g_bufferCount; i++) {
		placePicture(first + i, layout[i]);
		g_motionData[i] = g_workBase + layout[class->motion[set] + i];
		setFrameWidth(g_frameHandles[first + i], class->width);
		setFrameHeight(g_frameHandles[first + i], class->height);
	}
	if (set || mode != VIP_MEMORY_SIZED)
		return;
	/* one frame store more, without motion data */
	g_bufferCount = class->count + 1;
	placePicture(class->count, layout[class->count]);
	setFrameWidth(g_frameHandles[class->count], class->width);
	setFrameHeight(g_frameHandles[class->count], class->height);
}

/*
 * Lay out the frame stores for the current SPS's picture size, and clear the
 * pictures' motion data; -1 if the picture is too large.
 */
int layoutFrames(uint32_t mode)
{
	VipSps *sps = g_currentSps;
	uint32_t widthMbs, heightMbs, count, i;
	const VipLayout *class;

	widthMbs = sps->picWidthInMbsMinus1 + 1;
	heightMbs = (sps->picHeightInMapUnitsMinus1 + 1) * (2 - sps->frameMbsOnly);
	if (memoryQueryValid()) {
		if (layoutQueried(widthMbs * 16, heightMbs * 16))
			return -1;
	} else {
		if (widthMbs > MAX_WIDTH_MBS || heightMbs > MAX_HEIGHT_MBS)
			return -1;
		if (mode == VIP_MEMORY_STANDARD) {
			layoutStandard();
		} else {
			for (class = g_layouts; ; class++) {
				if (widthMbs >= class->minWidthMbs &&
				    heightMbs >= class->minHeightMbs)
					break;
			}
			layoutClass(class, mode);
		}
	}

	count = g_bufferCount;
	if (!g_frameSet && mode == VIP_MEMORY_SIZED)
		count--;
	for (i = 0; i < count; i++)
		clearByDma(CLEAR_MOTION_DMA, g_motionData[i] + heightMbs * 32 + 16,
			(uint32_t)g_codecDma.clear.zeros << 16 | 1);
	return 0;
}

/* Place the frame stores and motion data from what queryWorkMemory worked out. */
int layoutQueried(uint32_t width, uint32_t height)
{
	VipMemoryQuery *query = &g_memoryQuery;
	uint32_t pitch, rows, motionRows, across, bands, columns, perColumn;
	uint32_t remainder, fullBands, offset, band, i = 0, j, k;

	if (query->width < width || query->height < height)
		return -1;
	g_bufferCount = query->frames;
	pitch = query->width * 2 + QUERY_MARGIN;
	rows = query->height * 3 / 4 + 4;
	motionRows = (query->height >> 4) * 5;
	across = (VIP_WORK_PITCH - QUERY_MARGIN) / pitch;
	bands = (query->frames + across - 1) / across;

	/* the pictures, across each band of rows */
	for (band = 0; band < bands; band++) {
		offset = WORK_ROWS(rows) * band;
		for (k = 0; k < across; k++) {
			if (i >= query->frames)
				continue;
			placePicture(i, offset);
			setFrameWidth(g_frameHandles[i], query->width);
			setFrameHeight(g_frameHandles[i], query->height);
			i++;
			offset += WORK_COLUMN(pitch);
		}
	}

	/* the motion data: beside the full bands, beside the last one, then below */
	i = 0;
	columns = (VIP_WORK_PITCH - across * pitch) / query->width;
	perColumn = query->frames / across * rows / motionRows;
	for (j = 0; j < perColumn; j++) {
		offset = WORK_COLUMN(across * pitch) + WORK_ROWS(motionRows) * j;
		for (k = 0; k < columns; k++) {
			if (i >= query->frames)
				continue;
			g_motionData[i] = g_workBase + offset;
			g_buffers[i].colocated = g_workBase + offset;
			i++;
			offset += WORK_COLUMN(query->width);
		}
	}
	if (i >= query->frames)
		return 0;
	remainder = query->frames % across;
	columns = (VIP_WORK_PITCH - remainder * pitch) / query->width;
	perColumn = (remainder != 0) * rows / motionRows;
	if (perColumn) {
		fullBands = query->frames / across;
		for (j = 0; j < perColumn; j++) {
			offset = WORK_ROWS(fullBands * rows) + WORK_ROWS(motionRows) * j +
				WORK_COLUMN(query->frames % across * pitch);
			for (k = 0; k < columns; k++) {
				if (i >= query->frames)
					continue;
				g_motionData[i] = g_workBase + offset;
				g_buffers[i].colocated = g_workBase + offset;
				i++;
				offset += WORK_COLUMN(query->width);
			}
		}
	}
	columns = VIP_WORK_PITCH / query->width;
	for (j = 0; i < query->frames; j++) {
		offset = WORK_ROWS(bands * rows) + WORK_ROWS(motionRows) * j;
		for (k = 0; k < columns; k++) {
			if (i >= query->frames)
				continue;
			g_motionData[i] = g_workBase + offset;
			g_buffers[i].colocated = g_workBase + offset;
			i++;
			offset += WORK_COLUMN(query->width);
		}
	}
	return 0;
}

uint32_t memoryQueryValid(void)
{
	return g_memoryQuery.valid;
}

/*
 * The work memory a stream needs for pictures of up to width by height with so many
 * reference frames: 0, or -1 if it cannot be decoded. Remembered for the open that
 * follows; 0 by 0 forgets it.
 */
int queryWorkMemory(int width, int height, uint32_t references, uint32_t *size)
{
	VipMemoryQuery *query = &g_memoryQuery;
	uint32_t frames = references + 3, pitch, rows, motionRows, across, bands;
	uint32_t remainder, placed, used;

	if (!width && !height) {
		query->valid = 0;
		query->width = 0;
		query->height = 0;
		query->frames = 0;
		query->size = 0;
		*size = QUERY_MAX_SIZE;
		return 0;
	}
	if (frames >= QUERY_MAX_FRAMES)
		return -1;
	width = (width + 15) / 16 * 16;
	height = (height + 15) / 16 * 16;
	pitch = width * 2 + QUERY_MARGIN;
	rows = height * 3 / 4 + 4;
	motionRows = height / 16 * 5;
	across = (VIP_WORK_PITCH - QUERY_MARGIN) / pitch;
	bands = (across + frames - 1) / across;
	used = bands * rows;

	/* the motion data that fit beside the pictures, then rows below them */
	placed = (VIP_WORK_PITCH - across * pitch) / (uint32_t)width *
		(frames / across * rows / motionRows);
	if (placed < frames) {
		remainder = frames % across;
		placed += (VIP_WORK_PITCH - remainder * pitch) / (uint32_t)width *
			((remainder != 0) * rows / motionRows);
		while (placed < frames) {
			used += motionRows;
			placed += VIP_WORK_PITCH / (uint32_t)width;
		}
	}
	used = (used + 31) >> 5 << 18;
	if (used > QUERY_MAX_SIZE)
		return -1;
	query->size = used;
	query->frames = frames;
	query->height = height;
	query->width = width;
	query->valid = 1;
	*size = used;
	return 0;
}

void clearMemoryQuery(void)
{
	g_memoryQuery.width = 0;
	g_memoryQuery.height = 0;
	g_memoryQuery.frames = 0;
	g_memoryQuery.size = 0;
	g_memoryQuery.valid = 0;
}
