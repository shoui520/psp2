/*
 * FW2's codec: where the pictures and their motion data go in the work memory.
 * The units address it as rows of 8 KB (VIP_WORK_PITCH): an address is the
 * column in 8-byte units from bit 19 up, the row times 8 below it, and the work
 * memory's 512 MB region in the top 3 bits. The pictures go side by side in
 * bands of rows, each 3/4 of a picture's height plus 4 rows tall at 2 bytes and
 * 64 more per pixel column; their motion data, a column per pixel and so many
 * rows per macroblock row, go beside each band's pictures, then below them all.
 *
 * With a work block the codec lays the frame stores out itself, for as many
 * frames as the open asked or as fit (layoutFrames); with work areas the ARM
 * side placed each frame store's picture and motion data.
 */
#include "codec.h"

#define VIP_WORK_ROW_SHIFT	13
#define WORK_REGION		0xe0000000
#define WORK_COLUMNS		0x1ff80000	/* column / 8 << 19 */
#define WORK_ROWS		0x0007fff8	/* row << 3 */
#define PICTURE_MARGIN		64	/* bytes after a picture's row, and after a band's last */
#define BAND_MARGIN		128	/* a band's right edge keeps this free */
#define BAND_START_ROWS		128	/* the first band starts at a multiple of these */
#define MAX_BANDS		19	/* the original's room; it writes past it beyond */

/* the largest picture, in macroblocks */
#define MAX_FRAME_MBS		0x2000

/* the frame stores when the open names none: level 4.1's DPB, and 3 more */
#define MAX_DPB_BYTES		0xc00000	/* MaxDpbMbs, 32768, at 384 bytes each */
#define MAX_DPB_FRAMES		16
#define EXTRA_FRAMES		3

/* the rows of motion data per macroblock row: with direct_8x8_inference_flag, 4 */
#define MOTION_ROWS_DIRECT_8X8	4
#define MOTION_ROWS		5

VipPictureBuffer g_buffers[VIP_CODEC_FRAMES] VIP_GLOBAL;
uint32_t g_motionData[VIP_CODEC_FRAMES - 1] VIP_GLOBAL;
uint8_t g_bufferCount VIP_GLOBAL;

typedef struct WorkPosition {
	uint32_t x;			/* bytes into the row */
	uint32_t row;
} WorkPosition;

/* the motion data placed so far */
typedef struct MotionPlacement {
	uint32_t width;			/* bytes */
	uint32_t rows;
	uint32_t frames;
	uint32_t placed;
	uint32_t lastRows[2];		/* the last placed's row, and the one before's */
} MotionPlacement;

static void placeMotion(MotionPlacement *motion, uint32_t row)
{
	motion->placed++;
	motion->lastRows[1] = motion->lastRows[0];
	motion->lastRows[0] = row;
}

/*
 * The rows of work memory frames of widthMbs by heightMbs macroblocks take, with
 * motionRowsPerMbRow rows of motion data per macroblock row: 0 if they need more
 * than limit bytes.
 */
uint32_t workMemoryRows(uint32_t widthMbs, uint32_t heightMbs, uint32_t frames, uint32_t limit,
	uint32_t motionRowsPerMbRow)
{
	WorkPosition bands[MAX_BANDS];
	MotionPlacement motion = { 0 };
	uint32_t pitch = widthMbs * 32 + PICTURE_MARGIN;
	uint32_t rows = heightMbs * 16 * 3 / 4 + 4;
	uint32_t limitRows = limit >> VIP_WORK_ROW_SHIFT;
	uint32_t x = 0, row = 0, pictureRow = 0, bandCount = 0;
	uint32_t slots, used, bottom, i, j, k;

	if (!frames)
		return 0;
	motion.width = widthMbs * 16;
	motion.rows = heightMbs * motionRowsPerMbRow;
	motion.frames = frames;

	/* the pictures, a band at a time; each band's free right part is noted */
	for (i = 0; i < frames; i++) {
		if (x + pitch + BAND_MARGIN > VIP_WORK_PITCH) {
			if (bandCount < MAX_BANDS) {
				bands[bandCount].x = x + PICTURE_MARGIN;
				bands[bandCount].row = row;
				bandCount++;
			}
			x = 0;
			row = (row + rows + 15) & ~15u;
			if (limitRows < row + rows)
				return 0;
		}
		pictureRow = row;
		x = (x + pitch + (x < PICTURE_MARGIN ? 2 * PICTURE_MARGIN - 1 : PICTURE_MARGIN - 1)) &
			~(PICTURE_MARGIN - 1u);
	}
	if (bandCount < MAX_BANDS) {
		bands[bandCount].x = x + PICTURE_MARGIN;
		bands[bandCount].row = row;
		bandCount++;
	}

	/* the motion data beside each band, as many down as its pictures' height holds */
	slots = rows / motion.rows;
	for (j = 0; j < bandCount; j++) {
		row = bands[j].row;
		for (k = 0; k < slots; k++, row += motion.rows) {
			for (x = bands[j].x; x + motion.width <= VIP_WORK_PITCH &&
			     motion.placed < motion.frames; x += motion.width)
				placeMotion(&motion, row);
		}
	}

	/* the rest below the pictures */
	used = pictureRow + rows;
	row = (used + 15) & ~15u;
	x = 0;
	while (motion.placed < motion.frames) {
		if (x + motion.width > VIP_WORK_PITCH) {
			row += motion.rows;
			if (limitRows < row + motion.rows)
				return 0;
			x = 0;
		}
		placeMotion(&motion, row);
		x += motion.width;
	}

	/* the one before the last (as it is), with one frame the last */
	bottom = (frames < 2 ? motion.lastRows[0] : motion.lastRows[1]) + motion.rows;
	if (bottom > used)
		used = bottom;
	if (limitRows < used)
		return 0;
	return used;
}

/*
 * The most frames (VIP_CODEC_FRAMES at most) of the size that fit in limit bytes,
 * 0 if none.
 */
uint32_t framesFitting(uint32_t widthMbs, uint32_t heightMbs, uint32_t limit,
	uint32_t motionRowsPerMbRow)
{
	uint32_t frames;

	for (frames = VIP_CODEC_FRAMES; frames >= 1; frames--) {
		if (workMemoryRows(widthMbs, heightMbs, frames, limit, motionRowsPerMbRow))
			break;
	}
	return frames;
}

static uint32_t workAddress(uint32_t column, uint32_t row)
{
	return (g_workBase & WORK_REGION) | (column >> 3 << 19 & WORK_COLUMNS) |
		(row << 3 & WORK_ROWS);
}

static uint32_t workColumn(uint32_t address)
{
	return (address & WORK_COLUMNS) >> 16;
}

static uint32_t workRow(uint32_t address)
{
	return (address & WORK_ROWS) >> 3;
}

/* The first band's row: a multiple of BAND_START_ROWS from the work memory's start. */
static uint32_t firstBandRow(void)
{
	uint32_t row = workRow(g_workBase);

	if (workColumn(g_workBase))
		row++;
	return (row + BAND_START_ROWS - 1) & ~(BAND_START_ROWS - 1);
}

/* -1 if the codec cannot decode pictures of the size. */
static int checkPictureSize(uint32_t widthMbs, uint32_t heightMbs)
{
	if (!widthMbs || !heightMbs || widthMbs * heightMbs > MAX_FRAME_MBS ||
	    widthMbs > MAX_WIDTH_MBS || heightMbs > MAX_HEIGHT_MBS)
		return -1;
	return 0;
}

/*
 * Lay frames frame stores of widthMbs by heightMbs macroblocks out in the work
 * block, as workMemoryRows counts them, and the motion data of all but one (of
 * the one, with one): -1 if the size cannot be decoded or they do not fit. 0
 * frames: as many as level 4.1's DPB holds, and 3 more.
 */
static int layoutPictures(uint32_t widthMbs, uint32_t heightMbs, uint32_t motionRowsPerMbRow,
	uint32_t frames)
{
	uint32_t pictures[VIP_CODEC_FRAMES], bands[MAX_BANDS], motion[VIP_CODEC_FRAMES];
	uint32_t pitch, rows, motionWidth, motionRows, motionCount, limitRow;
	uint32_t x, row, bandCount = 0, placed = 0, bottom, i, j, k;

	if (checkPictureSize(widthMbs, heightMbs))
		return -1;
	if (motionRowsPerMbRow != MOTION_ROWS_DIRECT_8X8 && motionRowsPerMbRow != MOTION_ROWS)
		return -1;
	if (frames > VIP_CODEC_FRAMES)
		return -1;
	if (!frames) {
		frames = MAX_DPB_BYTES / (widthMbs * heightMbs * 384);
		if (frames > MAX_DPB_FRAMES)
			frames = MAX_DPB_FRAMES;
		frames += EXTRA_FRAMES;
	}
	motionCount = frames == 1 ? 1 : frames - 1;
	motionWidth = widthMbs * 16;
	motionRows = heightMbs * motionRowsPerMbRow;
	pitch = widthMbs * 32 + PICTURE_MARGIN;
	rows = heightMbs * 16 * 3 / 4 + 4;

	row = firstBandRow();
	limitRow = workRow(g_workBase) + (g_workSize >> VIP_WORK_ROW_SHIFT);
	if (limitRow < row)
		return -1;

	/* the pictures, a band at a time; each band's free right part is noted */
	x = 0;
	for (i = 0; i < frames; i++) {
		if (x + pitch + BAND_MARGIN > VIP_WORK_PITCH) {
			if (bandCount < MAX_BANDS)
				bands[bandCount++] = workAddress(x + PICTURE_MARGIN, row);
			x = 0;
			row = (row + rows + 15) & ~15u;
			if (limitRow < row + rows)
				return -1;
		}
		pictures[i] = workAddress(x, row);
		x = (x + pitch + (x < PICTURE_MARGIN ? 2 * PICTURE_MARGIN - 1 : PICTURE_MARGIN - 1)) &
			~(PICTURE_MARGIN - 1u);
	}
	if (bandCount < MAX_BANDS)
		bands[bandCount++] = workAddress(x + PICTURE_MARGIN, row);

	/* the motion data beside each band, as many down as its pictures' height holds */
	for (j = 0; j < bandCount; j++) {
		row = workRow(bands[j]);
		for (k = 0; k < rows / motionRows; k++, row += motionRows) {
			for (x = workColumn(bands[j]); x + motionWidth <= VIP_WORK_PITCH &&
			     placed < motionCount; x += motionWidth)
				motion[placed++] = workAddress(x, row);
		}
	}

	/* the rest below the pictures */
	row = (workRow(pictures[frames - 1]) + rows + 15) & ~15u;
	for (x = 0; placed < motionCount; x += motionWidth) {
		if (x + motionWidth > VIP_WORK_PITCH) {
			row += motionRows;
			if (limitRow < row + motionRows)
				return -1;
			x = 0;
		}
		motion[placed++] = workAddress(x, row);
	}

	bottom = workRow(pictures[frames - 1]) + rows;
	if (workRow(motion[motionCount - 1]) + motionRows > bottom)
		bottom = workRow(motion[motionCount - 1]) + motionRows;
	if (limitRow < bottom)
		return -1;

	for (i = 0; i < frames; i++) {
		g_buffers[i].picture = pictures[i];
		setFramePicture(g_frameHandles[i], pictures[i]);
		setFrameWidth(g_frameHandles[i], widthMbs * 16);
		setFrameHeight(g_frameHandles[i], heightMbs * 16);
	}
	for (k = 0; k < motionCount; k++) {
		g_buffers[k].colocated = motion[k];
		g_motionData[k] = motion[k];
	}
	g_bufferCount = frames;
	return 0;
}

/*
 * Take the frame stores' pictures and motion data from the work areas; one area
 * is a picture's address alone, its motion data there too.
 */
static void layoutAreas(uint32_t count, const VipWorkArea *areas)
{
	uint32_t i;

	g_bufferCount = count;
	for (i = 0; i + 1 < count; i++) {
		g_frameHandles[i]->frame->picture = areas[i].picture;
		g_buffers[i].picture = areas[i].picture;
		g_buffers[i].colocated = areas[i].motion;
	}
	g_frameHandles[i]->frame->picture = areas[i].picture;
	g_buffers[i].picture = areas[i].picture;
	g_buffers[i].colocated = count < 2 ? areas[i].picture : areas[i].motion;
}

/*
 * Lay the frame stores out for the current SPS's picture size (workKind:
 * VIP_WORK_*), and clear the motion data: -1 if they do not fit. frames: the
 * open's area count, which for a work block may be 0, as many as fit.
 */
int layoutFrames(uint32_t workKind, uint32_t frames)
{
	VipSps *sps = g_currentSps;
	uint32_t widthMbs = sps->picWidthInMbsMinus1 + 1;
	uint32_t heightMbs = (sps->picHeightInMapUnitsMinus1 + 1) << (sps->frameMbsOnly ^ 1);
	uint32_t motionRows = sps->direct8x8Inference ? MOTION_ROWS_DIRECT_8X8 : MOTION_ROWS;
	int i;

	if (workKind == VIP_WORK_BLOCK) {
		if (!frames)
			frames = framesFitting(widthMbs, heightMbs, g_workSize, motionRows);
		if (layoutPictures(widthMbs, heightMbs, motionRows, frames & 0xff))
			return -1;
	} else if (workKind == VIP_WORK_AREAS) {
		layoutAreas(frames, (const VipWorkArea *)g_workBase);
		for (i = 0; i < g_bufferCount; i++) {
			setFrameWidth(g_frameHandles[i], widthMbs);
			setFrameHeight(g_frameHandles[i], heightMbs);
		}
	}

	if (workKind == VIP_WORK_BLOCK) {
		for (i = 0; i < g_bufferCount - 1; i++)
			clearByDma(CLEAR_MOTION_DMA, g_motionData[i] + heightMbs * 32 + 16,
				(uint32_t)g_codecDma.clear.zeros << 16 | 1);
	}
	return 0;
}
