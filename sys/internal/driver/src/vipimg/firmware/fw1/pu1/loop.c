/*
 * PU1: the macroblock loops and the motion saving, task 0's code overlays.
 * startPicture loads one loop at runMacroblocks for the picture: the SD loop, the
 * HD loop (g_standardPicture clear: the prediction unit is not waited for) or the
 * MBAFF loop; and at saveMacroblockMotion what a reference picture keeps of its
 * motion. A loop runs the slice's macroblocks through the units as a pipeline:
 * each turn, each macroblock in it moves a stage on, and a new one starts at
 * stage 0 while unit 0 says the slice goes on (VIP_CB_SLICE_RUNNING).
 *
 *   stage 0  unit 0 decodes it; the macroblock before's motion is saved, and the
 *            next direct macroblock's colocated motion goes in
 *   stage 1  the prediction unit
 *   stage 2  unit 1; the row above it is read in
 *   stage 3  unit 3, once unit 2 is past the macroblocks before
 *   stage 4  unit 4, the deblocking
 *   stage 6  written out to the picture, once the next was deblocked (that
 *            changes its right edge), or at the end of its row
 *
 * An MBAFF frame goes by pairs: the row above is read at stage 1, the deblocking
 * is at stage 6 and the writes at stage 9, each on every other turn.
 * Each overlay is one function; what they share is inlined into each. The
 * registers, and the offsets and DMA parameters of the writes, are known only
 * from what the loops do with them.
 *
 * FW2's loops are these without the error log and the implicit weights' fix.
 * FW2 also has a resident loop, for small pictures (runSmallLoop), and the
 * writes as a resident function (writeMacroblock); fw1 keeps that function,
 * unreachable, and it is not written for it.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#define LOOP_LOG(error)
#else
#include "codec.h"
#define LOOP_LOG(error)		logError(error)
#endif

#define LOOP_INLINE		static inline __attribute__((always_inline))
#define LOOP_TEXT(overlay)	__attribute__((section("." #overlay ".text")))

/* the loops, and their errors in the log: 0x3000 | loop << 4 | ERROR_* */
#define LOOP_SD			0
#define LOOP_HD			1
#define LOOP_MBAFF		2
#define LOOP_ERROR(loop, error)	(0x3000 | (loop) << 4 | (error))
#define ERROR_PREDICTION	0
#define ERROR_ENTROPY		1
#define ERROR_UNIT3		2

/* what the loops return (decodeSlice takes all but LOOP_DONE as an error) */
#define LOOP_DONE		0
#define LOOP_ENTROPY_FAILED	(-1)	/* or the prediction unit stopped */
#define LOOP_UNIT3_FAILED	(-2)
#define LOOP_ENTROPY_TIMEOUT	(-3)	/* unit 0 did not finish: it was reset */
#define LOOP_PICTURE_END	(-4)	/* the slice went past the picture's end */
#define LOOP_PREDICTION_FAILED	(-10)

#define STAGE(n)		(1u << (n))
#define FRAME_STAGES		0x07f	/* a macroblock still in the frame loops */
#define FRAME_SLOT_STAGES	0x0fc	/* stages 2-7: the deblocking's slots move on */
#define MBAFF_STAGES		0x3ff
#define MBAFF_SLOT_STAGES	0x7fc

#define WAIT_SPINS		2000	/* polls before a unit is taken to be stuck */
#define PREDICTION_WINDOW	0x8c	/* see startEntropy */
#define DEBLOCK_PAIR		4	/* VIP_CB_UNIT_COMMAND(4): an MBAFF pair */

/* the writes out of the units' RAM and the reads of the row above into it */
#define WRITE_DMA		(VIP_DMA_LIST | 0x31100 | VIP_DMA_BANK1)
#define NEIGHBOUR_DMA		(VIP_DMA_LIST | 0x31200 | VIP_DMA_BANK1 | VIP_DMA_READ)
#define MOTION_ALL_DMA		(VIP_DMA_LIST | 0x31000)
#define EDGE_LENGTH		2	/* a descriptor of a picture edge's */

/* the implicit weights' words: their flags, and two DistScaleFactors in each other */
#define WEIGHTS_FIRST		0x80000000	/* the first word's factors are used */
#define WEIGHTS_SECOND		0x10000000
#define WEIGHTS_FLAGS		0
#define WEIGHTS_FACTORS0	7
#define WEIGHTS_FACTORS1	8
#define DEFAULT_SCALE_FACTOR	128	/* w0 = w1 = 32 */

/* the next of a three-way pipeline slot: 0, 1, 2 */
LOOP_INLINE uint32_t nextPipelineSlot(uint32_t slot)
{
	if (!slot)
		return 1;
	return slot == 1 ? 2 : 0;
}

/* the deblocking's slots on: 8 takes 9's, 9 the next of four */
LOOP_INLINE void moveDeblockingSlots(void)
{
	uint32_t slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(9));

	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(8), slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(9), (slot + 1) & 3);
}

/* the slot of the macroblock written out: 8's last but one */
LOOP_INLINE void setMacroblockSlot(void)
{
	vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT,
		(vipReadControlBus(VIP_CB_PIPELINE_SLOT(8)) + 3) & 3);
}

/* Stage 1: the prediction unit on its next buffer; LOOP_PREDICTION_FAILED if it failed. */
LOOP_INLINE int startPrediction(int loop)
{
	uint32_t error = vipReadControlBus(VIP_CB_PREDICTION_ERROR);
	uint32_t slot = vipReadControlBus(VIP_CB_PREDICTION_STAGE);

	if (error) {
		LOOP_LOG(LOOP_ERROR(loop, ERROR_PREDICTION));
		return LOOP_PREDICTION_FAILED;
	}
	vipWriteControlBus(VIP_CB_PREDICTION_STAGE, nextPipelineSlot(slot));
	vipWriteControlBus(VIP_CB_PREDICTION_COMMAND, VIP_UNIT_START);
	return 0;
}

/*
 * Stage 0: unit 0 on the next macroblock. The HD and MBAFF loops first wait while
 * the prediction unit's count is 1 to PREDICTION_WINDOW - 1 past unit 0's (mod
 * 256), and give up when the prediction unit has stopped: -1.
 */
LOOP_INLINE int startEntropy(int loop)
{
	uint32_t slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(0)), entropy, ahead;

	vipWriteControlBus(VIP_CB_MACROBLOCK_NUMBER, g_macroblockCount);
	slot = nextPipelineSlot(slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(0), slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(2), slot);
	if (loop != LOOP_SD) {
		for (;;) {
			entropy = vipReadControlBus(VIP_CB_SLICE_COUNTER);
			ahead = (vipReadControlBus(VIP_CB_PREDICTION_SLICE) - entropy) & 0xff;
			if (!ahead || ahead >= PREDICTION_WINDOW)
				break;
			if (!vipReadControlBus(VIP_CB_PREDICTION_COMMAND))
				return -1;
		}
	}
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(0), g_entropyCommand);
	return 0;
}

/* Stage 2: unit 1 on its next buffers. */
LOOP_INLINE void startTransform(void)
{
	uint32_t slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(1));
	uint32_t half = vipReadControlBus(VIP_CB_PIPELINE_SLOT(4));

	slot = nextPipelineSlot(slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(1), slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(3), slot);
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(4), !half);
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(1), VIP_UNIT_START);
}

/*
 * Stage 3: unit 3 on its next buffers (slot 7 too when moveSlot7), once unit 2 is
 * past the macroblocks before; when it is not within WAIT_SPINS polls, unit 2 is
 * restarted past them. LOOP_UNIT3_FAILED when unit 3 failed.
 */
LOOP_INLINE int startUnit3(int loop, int moveSlot7)
{
	uint32_t slot, half, count = g_predictionCount, spins;

	if (vipReadControlBus(VIP_CB_UNIT_ERROR(3))) {
		LOOP_LOG(LOOP_ERROR(loop, ERROR_UNIT3));
		return LOOP_UNIT3_FAILED;
	}
	slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(6));
	half = vipReadControlBus(VIP_CB_PIPELINE_SLOT(5));
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(6), nextPipelineSlot(slot));
	vipWriteControlBus(VIP_CB_PIPELINE_SLOT(5), !half);
	if (moveSlot7) {
		slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(7));
		vipWriteControlBus(VIP_CB_PIPELINE_SLOT(7), (slot + 1) & 3);
	}

	for (spins = 0; vipReadControlBus(VIP_CB_UNIT2_COUNT) <= count; ) {
		if (++spins == WAIT_SPINS) {
			vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
			vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
			vipWriteControlBus(VIP_CB_UNIT2_COUNT, g_predictionCount + 1);
			break;
		}
	}
	vipWriteControlBus(VIP_CB_UNIT_COMMAND(3), VIP_UNIT_START);
	g_predictionCount++;
	return 0;
}

/*
 * Wait for unit 0's macroblock; -1 when it is not done within WAIT_SPINS polls,
 * and unit 0 and the read stream's interrupts are reset.
 */
LOOP_INLINE int waitEntropy(void)
{
	uint32_t spins;

	for (spins = 0; vipReadControlBus(VIP_CB_UNIT_COMMAND(0)); ) {
		if (++spins == WAIT_SPINS) {
			mep_di();
			vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 0);
			vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM0);
			vipAcknowledgeInterrupt(VIP_INTERRUPT_READ_STREAM1);
			vipAcknowledgeInterrupt(VIP_INTERRUPT_8);
			mep_ei();
			vipWriteControlBus(VIP_CB_UNIT_ENABLE(0), 1);
			vipWriteControlBus(VIP_CB_UNIT_MODE(0), VIP_ENTROPY_UNIT_MODE);
			return -1;
		}
	}
	return 0;
}

#ifndef VIP_FW2
/* a DistScaleFactor (11 bits) whose >> 2 is below -64 or above 128 */
LOOP_INLINE int scaleFactorOutOfRange(uint32_t factor)
{
	if (factor & 0x400)
		return (factor & 0x3fc) < 0x300;
	return (factor & 0x3fc) > 0x200;
}

/* a word's two factors, those out of range made the default; 1 if one was */
LOOP_INLINE int fixScaleFactors(uint32_t factors, uint32_t *fixed)
{
	uint32_t high = factors >> 16 & 0x7ff, low = factors & 0x7ff;
	int changed = 0;

	if (scaleFactorOutOfRange(factors >> 16)) {
		high = DEFAULT_SCALE_FACTOR;
		changed = 1;
	}
	if (scaleFactorOutOfRange(factors)) {
		low = DEFAULT_SCALE_FACTOR;
		changed = 1;
	}
	*fixed = high << 16 | low;
	return changed;
}

/*
 * Implicit weights (8.4.2.3.1): the units take DistScaleFactor as it is, so where
 * its >> 2 is out of range, which makes w0 = w1 = 32, the decoded macroblock's
 * factors are made DEFAULT_SCALE_FACTOR. As the firmware has it, when the second
 * word is not used, the first word goes into it too when it changed.
 */
LOOP_INLINE void fixImplicitWeights(void)
{
	uint32_t slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(2)), flags, fixed;
	volatile uint32_t *weights = (volatile uint32_t *)(VIP_UNIT_RAM +
		VIP_UNIT_IMPLICIT_WEIGHTS + slot * VIP_UNIT_IMPLICIT_WEIGHTS_SIZE);
	int changed;

	flags = weights[WEIGHTS_FLAGS];
	if (!(flags & WEIGHTS_FIRST))
		return;
	changed = fixScaleFactors(weights[WEIGHTS_FACTORS0], &fixed);
	if (changed)
		weights[WEIGHTS_FACTORS0] = fixed;
	if (flags & WEIGHTS_SECOND)
		changed = fixScaleFactors(weights[WEIGHTS_FACTORS1], &fixed);
	if (changed)
		weights[WEIGHTS_FACTORS1] = fixed;
	mep_syncm();
}

#endif

/* a write of a picture edge's: EDGE_LENGTH from local in the units' RAM */
LOOP_INLINE void listEdge(VipDmaDescriptor *descriptor, uint32_t external, uint32_t local)
{
	descriptor->external = external;
	descriptor->localLength = local << 16 | EDGE_LENGTH;
}

/*
 * Write the macroblock at g_mbX, y from the units out to the picture (the
 * picture channel gives where); in the picture's top and bottom rows its edges
 * also go to the margin above and below. loadMacroblockLoop listed the local
 * sides of loop[6] and loop[8].
 */
LOOP_INLINE void writeFrameMacroblock(uint32_t y)
{
	VipDmaDescriptor *loop = g_codecDma.loop, *list;
	volatile uint32_t done;
	uint32_t address = vipReadControlBus(VIP_CB_PICTURE_ADDRESS);
	uint32_t slot, count, parameter0, parameter1;

	if (!y) {
		slot = macroblockSlot();
		if (!g_fieldPic) {
			listEdge(&loop[0], address + 32, slot + 320);
			listEdge(&loop[1], address + 24, slot + 336);
			listEdge(&loop[2], address + VIP_BOTTOM_FIELD_OFFSET + 32, slot + 512);
			listEdge(&loop[3], address + VIP_BOTTOM_FIELD_OFFSET + 24, slot + 528);
			count = 4;
		} else {
			listEdge(&loop[0], address + 80, slot + 320);
			listEdge(&loop[1], address + 72, slot + 336);
			count = 2;
		}
		/* in a picture one row high the bottom's use the list next: wait */
		if (!g_heightMapUnitsMinus1) {
			queueListDma(WRITE_DMA, (uint32_t)loop << 16 | count * sizeof(*loop),
				8, 0x20000, NULL, &done);
			while (done == 1)
				;
		} else {
			queueListDma(WRITE_DMA, (uint32_t)loop << 16 | count * sizeof(*loop),
				8, 0x20000, NULL, NULL);
		}
	}
	if (g_lastMbRow == y) {
		slot = macroblockSlot();
		if (!g_fieldPic) {
			listEdge(&loop[0], address + 136, slot + 496);
			listEdge(&loop[1], address + VIP_BOTTOM_FIELD_OFFSET + 136, slot + 688);
			listEdge(&loop[2], address + 144, slot + 480);
			listEdge(&loop[3], address + VIP_BOTTOM_FIELD_OFFSET + 144, slot + 672);
			count = 4;
		} else {
			listEdge(&loop[0], address + 280, slot + 688);
			listEdge(&loop[1], address + 288, slot + 672);
			count = 2;
		}
		queueListDma(WRITE_DMA, (uint32_t)loop << 16 | count * sizeof(*loop), 8,
			0x20000, NULL, NULL);
	}

	if (!g_fieldPic) {
		parameter1 = 0x10000;
		parameter0 = 136;
		if (!y) {
			address += 40;
			parameter0 = 96;
		}
	} else {
		parameter1 = 0x20000;
		parameter0 = 280;
		if (!y) {
			parameter0 = 192;
			address += 88;
		}
	}
	list = y ? &loop[6] : &loop[8];
	list->external = address;
	queueListDma(WRITE_DMA, (uint32_t)list << 16 | sizeof(*list), parameter0, parameter1,
		NULL, &g_stageDone[0]);
	vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);
}

/* Read the row above the macroblock at g_neighbourX, g_neighbourY in; next. */
LOOP_INLINE void readFrameNeighbours(void)
{
	VipDmaDescriptor *loop = g_codecDma.loop;
	uint32_t address, parameter0, parameter1;

	if (g_neighbourY) {
		address = vipReadControlBus(VIP_CB_NEIGHBOUR_ADDRESS);
		if (!g_fieldPic) {
			parameter1 = 0x10000;
			parameter0 = 40;
		} else {
			parameter0 = 88;
			parameter1 = 0x20000;
		}
		loop[4].external = address;
		queueListDma(NEIGHBOUR_DMA, (uint32_t)&loop[4] << 16 | sizeof(loop[4]),
			parameter0, parameter1, NULL, &g_stageDone[1]);
		vipWriteControlBus(VIP_CB_NEIGHBOUR_COMMAND, VIP_UNIT_START);
	}
	if (g_neighbourX == g_widthMbsMinus1) {
		g_neighbourY++;
		g_neighbourX = 0;
	} else {
		g_neighbourX++;
	}
}

/*
 * The MBAFF loop's write: the pair at g_mbX, y (2 * g_mbY), of field macroblocks
 * when field.
 */
LOOP_INLINE void writeMbaffPair(uint32_t y, uint32_t field)
{
	VipDmaDescriptor *loop = g_codecDma.loop, *list;
	volatile uint32_t done;
	uint32_t address = vipReadControlBus(VIP_CB_PICTURE_ADDRESS);
	uint32_t slot, top, bottom, count, parameter0, parameter1;

	if (!y) {
		top = address + 72;
		slot = macroblockSlot();
		listEdge(&loop[0], top + 8, slot + 320);
		listEdge(&loop[1], top, slot + 336);
		if (field) {
			listEdge(&loop[2], top + VIP_BOTTOM_FIELD_OFFSET + 8, slot + 704);
			listEdge(&loop[3], top + VIP_BOTTOM_FIELD_OFFSET, slot + 720);
		} else {
			listEdge(&loop[2], top + VIP_BOTTOM_FIELD_OFFSET + 8, slot + 512);
			listEdge(&loop[3], top + VIP_BOTTOM_FIELD_OFFSET, slot + 528);
		}
		if (!g_lastMbRow) {
			queueListDma(WRITE_DMA, (uint32_t)loop << 16 | 4 * sizeof(*loop), 8,
				0x20000, NULL, &done);
			while (done == 1)
				;
		} else {
			queueListDma(WRITE_DMA, (uint32_t)loop << 16 | 4 * sizeof(*loop), 8,
				0x20000, NULL, NULL);
		}
	}
	if (g_lastMbRow == y) {
		slot = macroblockSlot();
		bottom = address + 280;
		if (!field) {
			listEdge(&loop[0], bottom, slot + 880);
			listEdge(&loop[2], bottom + 8, slot + 864);
		} else {
			listEdge(&loop[0], bottom, slot + 688);
			listEdge(&loop[2], bottom + 8, slot + 672);
		}
		listEdge(&loop[1], bottom + VIP_BOTTOM_FIELD_OFFSET, slot + 1072);
		listEdge(&loop[3], bottom + VIP_BOTTOM_FIELD_OFFSET + 8, slot + 1056);
		queueListDma(WRITE_DMA, (uint32_t)loop << 16 | 4 * sizeof(*loop), 8, 0x20000,
			NULL, NULL);
	}

	if (!field) {
		parameter1 = 0x10000;
		if (!y) {
			loop[6].external = address + 88;
			loop[7].external = address + 88 + 96;
			list = &loop[6];
			count = 2;
			parameter0 = 96;
		} else {
			loop[8].external = address;
			list = &loop[8];
			count = 1;
			parameter0 = 280;
		}
	} else {
		parameter1 = 0x20000;
		if (!y) {
			loop[9].external = address + 88;
			loop[10].external = address + 88 + VIP_BOTTOM_FIELD_OFFSET;
			list = &loop[9];
			parameter0 = 192;
		} else {
			loop[11].external = address;
			loop[12].external = address + VIP_BOTTOM_FIELD_OFFSET;
			list = &loop[11];
			parameter0 = 280;
		}
		count = 2;
	}
	queueListDma(WRITE_DMA, (uint32_t)list << 16 | count * sizeof(*list), parameter0,
		parameter1, NULL, &g_stageDone[0]);
	vipWriteControlBus(VIP_CB_PICTURE_COMMAND, VIP_UNIT_START);
}

/* The MBAFF loop's read of the row above a pair, both fields' lines. */
LOOP_INLINE void readMbaffNeighbours(void)
{
	VipDmaDescriptor *loop = g_codecDma.loop;
	uint32_t address = vipReadControlBus(VIP_CB_NEIGHBOUR_ADDRESS);

	loop[4].external = address;
	loop[5].external = address + VIP_BOTTOM_FIELD_OFFSET;
	queueListDma(NEIGHBOUR_DMA, (uint32_t)&loop[4] << 16 | 2 * sizeof(loop[4]), 88,
		0x20000, NULL, &g_stageDone[1]);
	vipWriteControlBus(VIP_CB_NEIGHBOUR_COMMAND, VIP_UNIT_START);
}

/* a macroblock started at stage 0: past the picture's end, unit 0 is stopped */
LOOP_INLINE void countMacroblock(uint32_t *stages, int *result)
{
	g_sliceMbAddress++;
	g_macroblockCount++;
	if (g_sliceMbAddress >= g_pictureMbs) {
		vipWriteControlBus(VIP_CB_SLICE_RUNNING, 0);
		*stages &= 0xfe;
		*result = LOOP_PICTURE_END;
	}
}

/* the SD and HD loops, of a frame or field picture */
LOOP_INLINE int runFrameLoop(int loop)
{
	uint32_t stages = STAGE(0), status, busy;
	int result = LOOP_DONE, error;

	for (;;) {
		do {
			status = vipReadControlBus(VIP_CB_PREDICTION_STATUS);
			busy = vipReadControlBus(VIP_CB_UNIT_COMMAND(3));
		} while (status == VIP_PREDICTION_FULL || busy);

		if (stages & STAGE(1)) {
			error = startPrediction(loop);
			if (error)
				return error;
		}
		if (loop == LOOP_SD)
			while (vipReadControlBus(VIP_CB_PREDICTION_COMMAND))
				;
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(1)))
			;
		if (stages & STAGE(0)) {
			if (loop == LOOP_SD && g_colocatedPreloaded == 1)
				pushColocatedMotion();
			if (startEntropy(loop))
				return LOOP_ENTROPY_FAILED;
		}
		if (stages & STAGE(2))
			startTransform();
		while (g_stageDone[0] == 1)
			;
		while (g_stageDone[1] == 1)
			;
		if (stages & STAGE(3)) {
			error = startUnit3(loop, 1);
			if (error)
				return error;
		}
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(4)))
			;
		if (stages & FRAME_SLOT_STAGES)
			moveDeblockingSlots();
		if (stages & STAGE(4))
			vipWriteControlBus(VIP_CB_UNIT_COMMAND(4), VIP_UNIT_START);
		setMacroblockSlot();

		/*
		 * The slice's first deblocked: the slice before's last, left of it, is
		 * written now (the picture channel starts there).
		 */
		if ((stages & (STAGE(5) | STAGE(6))) == STAGE(5) && g_mbX)
			writeFrameMacroblock(g_mbY);
		if (stages & STAGE(6)) {
			if ((stages & STAGE(5)) || g_mbX == g_widthMbsMinus1)
				writeFrameMacroblock(g_mbY);
			if (g_mbX == g_widthMbsMinus1) {
				g_mbY++;
				g_mbX = 0;
			} else {
				g_mbX++;
			}
		}
		if (stages & STAGE(2))
			readFrameNeighbours();
		if (stages & STAGE(0)) {
			if (g_motionSave == VIP_MOTION_SAVE_INFERRED)
				saveMacroblockMotion();
			if (g_directMacroblock)
				g_directMacroblock();
		}

		if (waitEntropy())
			return LOOP_ENTROPY_TIMEOUT;
#ifndef VIP_FW2
		if (g_implicitWeights == 1)
			fixImplicitWeights();
#endif
		if (loop == LOOP_SD && g_motionSave == VIP_MOTION_SAVE_ALL &&
		    (stages & STAGE(0)))
			saveMacroblockMotion();
		if (vipReadControlBus(VIP_CB_UNIT_ERROR(0))) {
			LOOP_LOG(LOOP_ERROR(loop, ERROR_ENTROPY));
			return LOOP_ENTROPY_FAILED;
		}

		stages = (stages << 1 | vipReadControlBus(VIP_CB_SLICE_RUNNING)) & 0xff;
		if (!(stages & FRAME_STAGES))
			break;
		if (stages & STAGE(0))
			countMacroblock(&stages, &result);
	}
	g_sliceMbAddress++;
	g_macroblockCount++;
	return result;
}

LOOP_TEXT(loop_sd) int runSdLoop(void)
{
	return runFrameLoop(LOOP_SD);
}

LOOP_TEXT(loop_hd) int runHdLoop(void)
{
	return runFrameLoop(LOOP_HD);
}

/*
 * The MBAFF loop. g_pairState is 0 on a pair's top macroblock's turns, when the
 * pair stages are done; g_fieldHistory keeps which macroblocks were field
 * macroblocks.
 */
LOOP_TEXT(loop_mbaff) int runMbaffLoop(void)
{
	uint32_t stages = STAGE(0), status, busy, predicting, entropy;
	int result = LOOP_DONE, error;

	for (;;) {
		do {
			status = vipReadControlBus(VIP_CB_PREDICTION_STATUS);
			busy = vipReadControlBus(VIP_CB_UNIT_COMMAND(3));
		} while (status == VIP_PREDICTION_FULL || busy);

		predicting = stages & STAGE(1);
		if (predicting) {
			error = startPrediction(LOOP_MBAFF);
			if (error)
				return error;
		}
		if (g_standardPicture)
			while (vipReadControlBus(VIP_CB_PREDICTION_COMMAND))
				;
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(1)))
			;
		entropy = stages & STAGE(0);
		if (entropy && startEntropy(LOOP_MBAFF))
			return LOOP_ENTROPY_FAILED;
		if (stages & STAGE(2))
			startTransform();
		if (g_pairState) {
			while (g_stageDone[0] == 1)
				;
			while (g_stageDone[1] == 1)
				;
		}
		if (stages & STAGE(3)) {
			error = startUnit3(LOOP_MBAFF, g_pairState);
			if (error)
				return error;
		}
		if (!g_pairState) {
			while (vipReadControlBus(VIP_CB_UNIT_COMMAND(4)))
				;
			if (stages & MBAFF_SLOT_STAGES)
				moveDeblockingSlots();
			if (stages & STAGE(6))
				vipWriteControlBus(VIP_CB_UNIT_COMMAND(4), DEBLOCK_PAIR);
		}
		setMacroblockSlot();

		/* as the frame loops', the slice before's last pair by its last macroblock */
		if ((stages & (STAGE(8) | STAGE(9))) == STAGE(8) && g_mbX && !g_pairState)
			writeMbaffPair(g_mbY << 1, g_lastFieldMacroblock);
		if (stages & STAGE(9)) {
			if (!g_pairState && ((stages & STAGE(8)) || g_mbX == g_mbaffLastX))
				writeMbaffPair(g_mbY << 1, g_fieldHistory >> 8 & 1);
			if (g_mbX == g_mbaffLastX) {
				g_mbY++;
				g_mbX = 0;
			} else {
				g_mbX++;
			}
		}
		if (predicting) {
			if (!g_pairState && g_neighbourY)
				readMbaffNeighbours();
			if (g_neighbourX == g_mbaffLastX) {
				g_neighbourY++;
				g_neighbourX = 0;
			} else {
				g_neighbourX++;
			}
		}
		if (entropy) {
			if (g_motionSave == VIP_MOTION_SAVE_INFERRED)
				saveMacroblockMotion();
			if (g_directMacroblock)
				g_directMacroblock();
		}

		if (waitEntropy())
			return LOOP_ENTROPY_TIMEOUT;
#ifndef VIP_FW2
		if (g_implicitWeights == 1)
			fixImplicitWeights();
#endif
		if (vipReadControlBus(VIP_CB_UNIT_ERROR(0))) {
			LOOP_LOG(LOOP_ERROR(LOOP_MBAFF, ERROR_ENTROPY));
			return LOOP_ENTROPY_FAILED;
		}

		g_fieldHistory = g_fieldHistory << 1 |
			vipReadControlBus(VIP_CB_FIELD_MACROBLOCK);
		stages = (stages << 1 | vipReadControlBus(VIP_CB_SLICE_RUNNING)) & 0xffff;
		if (!(stages & MBAFF_STAGES))
			break;
		/* as the firmware has it, the stop also drops stages 8 and 9 */
		if (stages & STAGE(0))
			countMacroblock(&stages, &result);
		g_pairState = !g_pairState;
	}
	g_sliceMbAddress++;
	g_macroblockCount++;
	return result;
}

/*
 * VIP_MOTION_SAVE_INFERRED: keep the motion of the macroblock unit 0 decoded last
 * with the others of its row, as saveMotion does at the slice's end, and write the
 * records out each time VIP_MOTION_RECORDS were kept.
 */
LOOP_TEXT(motion_inferred) void saveInferredMotion(void)
{
	uint32_t count = g_macroblockCount, *record, address;

	if (!count)
		return;
	record = g_motionBuffer.records[(count - 1) & 0xe];
	address = vipReadControlBus(VIP_CB_MOTION_ADDRESS);
	vipWriteControlBus(VIP_CB_MOTION_COMMAND, VIP_UNIT_START);
	while (g_listsDone == 1)
		;
	if (count & 1) {
		record[0] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x0);
		record[1] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1);
		record[2] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x2);
		record[3] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x3);
		record[4] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x4);
		record[6] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x5);
		record[8] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xe);
		record[10] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xf);
		record[5] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x6);
		record[11] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xd);
		record[9] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xc);
		record[7] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x7);
		record[12] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x8);
		record[13] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x9);
		record[14] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xa);
		record[15] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0xb);
	} else {
		record += VIP_MOTION_RECORD_WORDS;
		record[0] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x0);
		record[1] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x1);
		record[2] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x2);
		record[3] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x3);
		record[4] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x4);
		record[6] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x5);
		record[8] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xe);
		record[10] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xf);
		record[5] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x6);
		record[7] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x7);
		record[9] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xc);
		record[11] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xd);
		record[12] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x8);
		record[13] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0x9);
		record[14] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xa);
		record[15] = vipReadControlBus(VIP_CB_MOTION_BANK1 + 0xb);
	}
	g_motionList[(count - 1) & (VIP_MOTION_RECORDS - 1)].external = address;
	if (!(count & (VIP_MOTION_RECORDS - 1)))
		queueListDma(VIP_MOTION_DMA, (uint32_t)g_motionList << 16 | sizeof(g_motionList),
			g_motionSize, 0x10000, NULL, &g_listsDone);
}

/* VIP_MOTION_SAVE_ALL (the SD loop's): the decoded macroblock's motion written out. */
LOOP_TEXT(motion_all) void saveAllMotion(void)
{
	uint32_t *motion = g_motionBuffer.macroblock.motion;
	VipDmaDescriptor *list = &g_motionBuffer.macroblock.list;
	uint32_t address = vipReadControlBus(VIP_CB_MOTION_ADDRESS);

	vipWriteControlBus(VIP_CB_MOTION_COMMAND, VIP_UNIT_START);
	while (g_listsDone == 1)
		;
	motion[0] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x0);
	motion[1] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x2);
	motion[2] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x4);
	motion[3] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x6);
	motion[4] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x10);
	motion[5] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x11);
	motion[6] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x12);
	motion[7] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x13);
	motion[8] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x14);
	motion[9] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x15);
	motion[10] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x16);
	motion[11] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x17);
	motion[12] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x18);
	motion[13] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x19);
	motion[14] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1a);
	motion[15] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1b);
	motion[16] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1c);
	motion[17] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1d);
	motion[18] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1e);
	motion[19] = vipReadControlBus(VIP_CB_MOTION_BANK0 + 0x1f);
	list->external = address;
	list->localLength = (uint32_t)motion << 16 | sizeof(g_motionBuffer.macroblock.motion);
	queueListDma(MOTION_ALL_DMA, (uint32_t)list << 16 | sizeof(*list), 0, 0, NULL,
		&g_listsDone);
}

#ifdef VIP_FW2
/*
 * FW2: the frame loops' write of the macroblock at g_mbX, y, or the MBAFF loop's
 * of the pair at g_mbX, y, as one resident function.
 */
static void writeMacroblock(uint32_t y, uint32_t mbaff, uint32_t field)
{
	if (!mbaff)
		writeFrameMacroblock(y);
	else
		writeMbaffPair(y, field);
}

/*
 * FW2's loop for pictures narrower than 8 macroblocks or one map unit high
 * (decodeSlice's runLoop): each macroblock goes through all the units before the
 * next starts. A macroblock is written out once the next one was deblocked, the
 * last of a row at once; a slice's first write is of the macroblock before it,
 * except at a row's start. Pictures one macroblock wide write each at once.
 */
int runSmallLoop(void)
{
	uint32_t slot, count, spins, running;

	if (g_sliceMbAddress) {
		if (!g_mbaffFrame) {
			if (g_mbX)
				g_mbX--;
		} else if (g_mbX) {
			g_mbX -= 2;
		} else {
			g_mbY--;
			g_mbX = g_mbaffLastX - 1;
		}
	}

	do {
		slot = nextPipelineSlot(vipReadControlBus(VIP_CB_PIPELINE_SLOT(0)));
		vipWriteControlBus(VIP_CB_PIPELINE_SLOT(0), slot);
		vipWriteControlBus(VIP_CB_PIPELINE_SLOT(2), slot);
		if (g_colocatedPreloaded == 1)
			pushColocatedMotion();
		else
			vipWriteControlBus(VIP_CB_MACROBLOCK_NUMBER, g_macroblockCount);
		vipWriteControlBus(VIP_CB_UNIT_COMMAND(0), g_entropyCommand);
		if (waitEntropy())
			return LOOP_ENTROPY_TIMEOUT;
		if (vipReadControlBus(VIP_CB_UNIT_ERROR(0)))
			return LOOP_ENTROPY_FAILED;
		if (g_motionSave == VIP_MOTION_SAVE_INFERRED || g_motionSave == VIP_MOTION_SAVE_ALL)
			saveMacroblockMotion();
		while (g_listsDone == 1)
			;
		if (g_directMacroblock) {
			g_directMacroblock();
			while (g_colocatedDone == 1)
				;
		}

		startTransform();
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(1)))
			;
		slot = vipReadControlBus(VIP_CB_PREDICTION_STAGE);
		vipWriteControlBus(VIP_CB_PREDICTION_STAGE, nextPipelineSlot(slot));
		vipWriteControlBus(VIP_CB_PREDICTION_COMMAND, VIP_UNIT_START);

		/* unit 3, once unit 2 is past the macroblocks before (startUnit3's wait) */
		count = g_predictionCount;
		for (spins = 0; vipReadControlBus(VIP_CB_UNIT2_COUNT) <= count; ) {
			if (++spins == WAIT_SPINS) {
				vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 0);
				vipWriteControlBus(VIP_CB_UNIT_ENABLE(2), 1);
				vipWriteControlBus(VIP_CB_UNIT2_COUNT, g_predictionCount + 1);
				break;
			}
		}
		slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(6));
		vipWriteControlBus(VIP_CB_PIPELINE_SLOT(6), nextPipelineSlot(slot));
		vipWriteControlBus(VIP_CB_PIPELINE_SLOT(5), !vipReadControlBus(VIP_CB_PIPELINE_SLOT(5)));
		if (!g_mbaffFrame || !g_pairState) {
			slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(7));
			vipWriteControlBus(VIP_CB_PIPELINE_SLOT(7), (slot + 1) & 3);
		}
		vipWriteControlBus(VIP_CB_UNIT_COMMAND(3), VIP_UNIT_START);
		g_predictionCount++;
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(3)))
			;

		/* the deblocking's slots follow slot 7 */
		slot = vipReadControlBus(VIP_CB_PIPELINE_SLOT(7));
		vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT, slot);
		if (!g_mbaffFrame || !g_pairState) {
			vipWriteControlBus(VIP_CB_PIPELINE_SLOT(9), slot);
			vipWriteControlBus(VIP_CB_PIPELINE_SLOT(8), (slot + 3) & 3);
		}

		if (!g_mbaffFrame) {
			readFrameNeighbours();
		} else {
			if (g_pairState && g_neighbourY)
				readMbaffNeighbours();
			if (g_neighbourX == g_mbaffLastX) {
				g_neighbourY++;
				g_neighbourX = 0;
			} else {
				g_neighbourX++;
			}
		}
		while (g_stageDone[1] == 1)
			;

		if (!g_mbaffFrame)
			vipWriteControlBus(VIP_CB_UNIT_COMMAND(4), VIP_UNIT_START);
		else if (g_pairState)
			vipWriteControlBus(VIP_CB_UNIT_COMMAND(4), DEBLOCK_PAIR);
		vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT,
			vipReadControlBus(VIP_CB_PIPELINE_SLOT(8)));
		while (vipReadControlBus(VIP_CB_UNIT_COMMAND(4)))
			;

		if (!g_mbaffFrame) {
			if (g_widthMbsMinus1) {
				if (g_macroblockCount ||
				    g_sliceMbAddress % (g_widthMbsMinus1 + 1)) {
					writeMacroblock(g_mbY, 0, 0);
					if (g_mbX == g_widthMbsMinus1) {
						g_mbY++;
						g_mbX = 0;
					} else {
						g_mbX++;
					}
				}
			} else {
				vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT,
					vipReadControlBus(VIP_CB_PIPELINE_SLOT(7)));
				writeMacroblock(g_mbY, 0, 0);
				g_mbY++;
			}
		} else if (g_mbaffLastX != 1) {
			if (g_pairState && (g_macroblockCount >= 3 ||
			    (g_sliceMbAddress >> 1) % (g_widthMbsMinus1 + 1)))
				writeMacroblock(g_mbY << 1, 1, g_fieldHistory >> 1 & 1);
			/* as the firmware has it, by the slice's address, not its count */
			if (g_sliceMbAddress >= 3 && g_macroblockCount) {
				if (g_mbX == g_mbaffLastX) {
					g_mbY++;
					g_mbX = 0;
				} else {
					g_mbX++;
				}
			}
		} else {
			if (g_pairState) {
				vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT,
					vipReadControlBus(VIP_CB_PIPELINE_SLOT(7)));
				writeMacroblock(g_mbY << 1, 1, g_fieldHistory & 1);
			}
			if (g_mbX == g_mbaffLastX) {
				g_mbY++;
				g_mbX = 0;
			} else {
				g_mbX++;
			}
		}

		if (g_mbaffFrame)
			g_fieldHistory = g_fieldHistory << 1 |
				vipReadControlBus(VIP_CB_FIELD_MACROBLOCK);
		while (g_stageDone[0] == 1)
			;
		g_sliceMbAddress++;
		g_macroblockCount++;
		if (g_mbaffFrame)
			g_pairState = !g_pairState;
		running = vipReadControlBus(VIP_CB_SLICE_RUNNING);
		if (!running)
			break;
	} while (g_sliceMbAddress != g_pictureMbs);

	/* the last macroblock, when it ends its row */
	vipWriteControlBus(VIP_CB_MACROBLOCK_SLOT, vipReadControlBus(VIP_CB_PIPELINE_SLOT(9)));
	if (!g_mbaffFrame) {
		if (g_widthMbsMinus1) {
			if (g_mbX == g_widthMbsMinus1) {
				writeMacroblock(g_mbY, 0, 0);
				g_mbY++;
				g_mbX = 0;
			} else {
				g_mbX++;
			}
		}
	} else if (g_mbaffLastX != 1) {
		if (g_mbaffLastX == g_mbX + 2) {
			writeMacroblock(g_mbY << 1, 1, g_fieldHistory & 1);
			g_mbY++;
			g_mbX = 0;
		} else {
			g_mbX += 3;	/* as the firmware has it */
		}
	}
	while (g_stageDone[0] == 1)
		;
	if (running && g_sliceMbAddress == g_pictureMbs)
		return LOOP_PICTURE_END;
	return LOOP_DONE;
}
#endif
