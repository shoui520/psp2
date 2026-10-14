/*
 * PU1: a B slice's direct prediction with direct_8x8_inference (H.264 8.4.1.2).
 * The colocated unit walks the colocated picture's macroblocks as the slice goes
 * and gives each one's motion record's DRAM address (decodeSlice starts it); the
 * records are read in by one list DMA, VIP_MOTION_RECORDS at a time, into
 * g_colocatedMotion. Before each macroblock the loop calls g_directMacroblock,
 * which puts the next macroblock's colocated motion into the registers of its
 * bank, lists the record the unit gives next in the place that one came from, and
 * reads the list in again when it has gone round. What goes into the registers
 * depends on how the current and the colocated pictures were coded: a field
 * macroblock takes a frame macroblock pair's half of its parity, a frame
 * macroblock a field macroblock's upper or lower half. The start functions read
 * the first records in and put the first macroblock's motion in. FW2's are these
 * with the work memory always laid out by fields (memory mode 1), and its
 * records in its local RAM.
 */
#ifdef VIP_FW2
#include "../../fw2/codec/codec.h"
#define DIRECT_BSS	VIP_CODEC_LOCAL
#else
#include "codec.h"
#define DIRECT_BSS	VIP_TASK0_BSS
#endif

/* g_colocatedReads' DMA, like VIP_MOTION_DMA's with g_motionSize and 0x10000 (raw) */
#define READS_DMA		(VIP_DMA_LIST | 0x31200 | VIP_DMA_READ)
#define READS_PARAMETER1	0x10000

static VipDmaDescriptor g_colocatedReads[VIP_MOTION_RECORDS] DIRECT_BSS;
static uint32_t g_colocatedMotion[VIP_MOTION_RECORDS][VIP_MOTION_RECORD_WORDS]
	VIP_SLICE_BUFFER(colocated);
static uint8_t g_colocatedLowerHalf DIRECT_BSS;	/* a frame from fields: an odd row */

/* a record read's length, by g_memoryMode (raw) */
static uint32_t recordLength(void)
{
#ifdef VIP_FW2
	return 2;
#else
	return g_memoryMode ? 2 : 4;
#endif
}

/* where a frame macroblock pair's bottom record is, after the top one */
static uint32_t bottomOffset(void)
{
#ifdef VIP_FW2
	return 32;
#else
	return g_memoryMode ? 32 : 16;
#endif
}

/* The colocated unit's next record address, and start it on the one after. */
static uint32_t takeColocated(void)
{
	uint32_t address = vipReadControlBus(VIP_CB_COLOCATED_DATA);

	vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
	return address;
}

/* the same, when the unit has it */
static uint32_t nextColocated(void)
{
	while (vipReadControlBus(VIP_CB_COLOCATED_COMMAND))
		;
	return takeColocated();
}

static void queueColocatedReads(volatile uint32_t *done)
{
	queueListDma(READS_DMA, (uint32_t)g_colocatedReads << 16 | sizeof(g_colocatedReads),
		g_motionSize, READS_PARAMETER1, NULL, done);
}

static void waitColocatedReads(void)
{
	while (g_colocatedDone == 1)
		;
}

/* List the next VIP_MOTION_RECORDS macroblocks the unit gives. */
static void listColocated(void)
{
	uint32_t i;

	for (i = 0; i < VIP_MOTION_RECORDS; i++) {
		g_colocatedReads[i].external = nextColocated();
		g_colocatedReads[i].localLength =
			(uint32_t)g_colocatedMotion[i] << 16 | recordLength();
	}
}

/* List the next frame macroblock pairs the unit gives, top then bottom. */
static void listColocatedPairs(void)
{
	uint32_t address, i;

	for (i = 0; i < VIP_MOTION_RECORDS; i += 2) {
		address = nextColocated();
		g_colocatedReads[i].external = address;
		g_colocatedReads[i + 1].external = address + bottomOffset();
		g_colocatedReads[i].localLength =
			(uint32_t)g_colocatedMotion[i] << 16 | recordLength();
		g_colocatedReads[i + 1].localLength =
			(uint32_t)g_colocatedMotion[i + 1] << 16 | recordLength();
	}
}

/* Read the listed records in; the address the unit gives next, after them. */
static uint32_t readColocated(void)
{
	volatile uint32_t done;
	uint32_t next;

	queueColocatedReads(&done);
	next = takeColocated();
	while (done == 1)
		;
	return next;
}

static int isFieldMacroblock(const uint32_t *record)
{
	return record[VIP_MOTION_FIELD_WORD] >> VIP_MOTION_FIELD_SHIFT;
}

/*
 * The puts write the 8 registers from reg, which mep_stcb needs as a constant: a
 * macroblock's motion as it is; a frame macroblock pair's as a field macroblock
 * of the parity uses it; a field macroblock's as a frame macroblock uses its
 * upper or lower half.
 */
#define PUT_MOTION(reg, record) do {					\
	const uint32_t *motion_ = (record);				\
	vipWriteControlBus((reg) + 0, motion_[0]);			\
	vipWriteControlBus((reg) + 1, motion_[1]);			\
	vipWriteControlBus((reg) + 2, motion_[2]);			\
	vipWriteControlBus((reg) + 3, motion_[3]);			\
	vipWriteControlBus((reg) + 4, motion_[4]);			\
	vipWriteControlBus((reg) + 5, motion_[6]);			\
	vipWriteControlBus((reg) + 6, motion_[8]);			\
	vipWriteControlBus((reg) + 7, motion_[10]);			\
} while (0)

#define PUT_FIELD_HALF(reg, top, bottom, bottomField) do {		\
	const uint32_t *top_ = (top), *bottom_ = (bottom);		\
	if (!(bottomField)) {						\
		vipWriteControlBus((reg) + 0, top_[0]);			\
		vipWriteControlBus((reg) + 1, top_[1]);			\
		vipWriteControlBus((reg) + 2, bottom_[2]);		\
		vipWriteControlBus((reg) + 3, bottom_[3]);		\
	} else {							\
		vipWriteControlBus((reg) + 0, top_[12]);		\
		vipWriteControlBus((reg) + 1, top_[13]);		\
		vipWriteControlBus((reg) + 2, bottom_[14]);		\
		vipWriteControlBus((reg) + 3, bottom_[15]);		\
	}								\
	vipWriteControlBus((reg) + 4, top_[4]);				\
	vipWriteControlBus((reg) + 5, top_[6]);				\
	vipWriteControlBus((reg) + 6, bottom_[9]);			\
	vipWriteControlBus((reg) + 7, bottom_[11]);			\
} while (0)

#define PUT_FRAME_HALF(reg, record, lower) do {				\
	const uint32_t *motion_ = (record);				\
	if (!(lower)) {							\
		vipWriteControlBus((reg) + 0, motion_[0]);		\
		vipWriteControlBus((reg) + 2, motion_[0]);		\
		vipWriteControlBus((reg) + 1, motion_[1]);		\
		vipWriteControlBus((reg) + 3, motion_[1]);		\
		vipWriteControlBus((reg) + 4, motion_[4]);		\
		vipWriteControlBus((reg) + 5, motion_[6]);		\
		vipWriteControlBus((reg) + 6, motion_[5]);		\
		vipWriteControlBus((reg) + 7, motion_[7]);		\
	} else {							\
		vipWriteControlBus((reg) + 0, motion_[2]);		\
		vipWriteControlBus((reg) + 2, motion_[2]);		\
		vipWriteControlBus((reg) + 1, motion_[3]);		\
		vipWriteControlBus((reg) + 3, motion_[3]);		\
		vipWriteControlBus((reg) + 4, motion_[9]);		\
		vipWriteControlBus((reg) + 5, motion_[11]);		\
		vipWriteControlBus((reg) + 6, motion_[8]);		\
		vipWriteControlBus((reg) + 7, motion_[10]);		\
	}								\
} while (0)

/* The macroblock written out's slot in the units' RAM (VIP_CB_MACROBLOCK_SLOT). */
uint32_t macroblockSlot(void)
{
	return VIP_UNIT_MACROBLOCK_SLOTS +
		vipReadControlBus(VIP_CB_MACROBLOCK_SLOT) * VIP_UNIT_MACROBLOCK_SLOT_SIZE;
}

/* a field picture from a field pair */
void startFieldDirect(void)
{
	listColocated();
	g_colocatedReads[0].external = readColocated();
	PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0]);
}

/* a field picture from a frame */
void startFieldFromFrameDirect(void)
{
	volatile uint32_t done;
	uint32_t next;

	listColocatedPairs();
	next = takeColocated();
	queueColocatedReads(&done);
	while (done == 1)
		;
	g_colocatedReads[0].external = next;
	g_colocatedReads[1].external = next + bottomOffset();
	PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0], g_colocatedMotion[1],
		g_bottomField);
}

/* a field picture from an MBAFF frame */
void startFieldFromMbaffDirect(void)
{
	listColocated();
	g_colocatedReads[0].external = readColocated();
	g_colocatedReads[1].external = nextColocated();
	if (isFieldMacroblock(g_colocatedMotion[0]))
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(0),
			g_colocatedMotion[g_bottomField ? 1 : 0]);
	else
		PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0],
			g_colocatedMotion[1], g_bottomField);
}

/*
 * a frame from a field pair: g_colocatedColumn, where the slice starts in its two
 * rows, becomes its column and g_colocatedLowerHalf
 */
void startFrameFromFieldsDirect(void)
{
	uint32_t widthMbs = g_widthMbsMinus1 + 1;

	listColocated();
	g_colocatedReads[0].external = readColocated();
	if (g_colocatedColumn < widthMbs) {
		g_colocatedLowerHalf = 0;
	} else {
		g_colocatedLowerHalf = 1;
		g_colocatedColumn -= widthMbs;
	}
	PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0), g_colocatedMotion[0], g_colocatedLowerHalf);
}

/* a frame from a frame */
void startFrameDirect(void)
{
	listColocated();
	g_colocatedReads[0].external = readColocated();
	PUT_MOTION(VIP_CB_FRAME_COLOCATED(0), g_colocatedMotion[0]);
}

/* an MBAFF frame from a field pair */
void startMbaffFromFieldsDirect(void)
{
	uint32_t next;

	listColocatedPairs();
	next = readColocated();
	g_colocatedReads[0].external = next;
	g_colocatedReads[1].external = next + bottomOffset();
	PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0), g_colocatedMotion[g_colocatedField ? 1 : 0], 0);
	PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0]);
}

/* an MBAFF frame from a frame */
void startMbaffFromFrameDirect(void)
{
	listColocated();
	g_colocatedReads[0].external = readColocated();
	if (!isFieldMacroblock(g_colocatedMotion[0])) {
		PUT_MOTION(VIP_CB_FRAME_COLOCATED(0), g_colocatedMotion[0]);
		PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0],
			g_colocatedMotion[1], 0);
	} else {
		PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0),
			g_colocatedMotion[g_colocatedField ? 1 : 0], 0);
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[0]);
	}
}

/*
 * The macroblock handlers: the next macroblock, g_macroblockCount + 1, goes into
 * the bank of its parity.
 */
void fieldDirectMacroblock(void)
{
	uint32_t next = (g_macroblockCount + 1) & (VIP_MOTION_RECORDS - 1), address;

	address = takeColocated();
	waitColocatedReads();
	if (!(next & 1))
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), g_colocatedMotion[next]);
	else
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(1), g_colocatedMotion[next]);
	g_colocatedReads[next].external = address;
	if (next == VIP_MOTION_RECORDS - 1)
		queueColocatedReads(&g_colocatedDone);
}

/* by frame macroblock pairs, a pair a macroblock */
void fieldFromFrameDirectMacroblock(void)
{
	uint32_t next = g_macroblockCount + 1, pair = 2 * (next & 7), address;
	const uint32_t *top = g_colocatedMotion[pair];
	const uint32_t *bottom = g_colocatedMotion[pair + 1];

	address = takeColocated();
	waitColocatedReads();
	if (!(next & 1))
		PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), top, bottom, g_bottomField);
	else
		PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(1), top, bottom, g_bottomField);
	g_colocatedReads[pair].external = address;
	g_colocatedReads[pair + 1].external = address + bottomOffset();
	if (pair == VIP_MOTION_RECORDS - 2)
		queueColocatedReads(&g_colocatedDone);
}

/* by macroblock pairs, frame or field, a pair a macroblock */
void fieldFromMbaffDirectMacroblock(void)
{
	uint32_t next = g_macroblockCount + 1, pair = 2 * (next & 7), address;
	const uint32_t *top = g_colocatedMotion[pair];
	const uint32_t *bottom = g_colocatedMotion[pair + 1];
	const uint32_t *field = g_bottomField ? bottom : top;

	address = takeColocated();
	waitColocatedReads();
	if (!(next & 1)) {
		if (isFieldMacroblock(top))
			PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), field);
		else
			PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), top, bottom, g_bottomField);
	} else {
		if (isFieldMacroblock(top))
			PUT_MOTION(VIP_CB_FIELD_COLOCATED(1), field);
		else
			PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(1), top, bottom, g_bottomField);
	}
	g_colocatedReads[pair].external = address;
	g_colocatedReads[pair + 1].external = takeColocated();
	if (pair == VIP_MOTION_RECORDS - 2)
		queueColocatedReads(&g_colocatedDone);
}

/* the halves alternate by row, the column going round at the row's end */
void frameFromFieldsDirectMacroblock(void)
{
	uint32_t next = (g_macroblockCount + 1) & (VIP_MOTION_RECORDS - 1), address;
	const uint32_t *record = g_colocatedMotion[next];

	address = takeColocated();
	if (g_colocatedColumn == g_widthMbsMinus1) {
		g_colocatedColumn = 0;
		g_colocatedLowerHalf = !g_colocatedLowerHalf;
	} else {
		g_colocatedColumn++;
	}
	waitColocatedReads();
	if (!(next & 1))
		PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0), record, g_colocatedLowerHalf);
	else
		PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(1), record, g_colocatedLowerHalf);
	g_colocatedReads[next].external = address;
	if (next == VIP_MOTION_RECORDS - 1)
		queueColocatedReads(&g_colocatedDone);
}

void frameDirectMacroblock(void)
{
	uint32_t next = (g_macroblockCount + 1) & (VIP_MOTION_RECORDS - 1), address;

	address = takeColocated();
	waitColocatedReads();
	if (!(next & 1))
		PUT_MOTION(VIP_CB_FRAME_COLOCATED(0), g_colocatedMotion[next]);
	else
		PUT_MOTION(VIP_CB_FRAME_COLOCATED(1), g_colocatedMotion[next]);
	g_colocatedReads[next].external = address;
	if (next == VIP_MOTION_RECORDS - 1)
		queueColocatedReads(&g_colocatedDone);
}

/*
 * An MBAFF pair's top macroblock takes the field pair's upper halves, its bottom
 * one the lower; the unit gives a pair's address before its top macroblock.
 */
void mbaffFromFieldsDirectMacroblock(void)
{
	uint32_t next = (g_macroblockCount + 1) & (VIP_MOTION_RECORDS - 1), address;
	const uint32_t *top = g_colocatedMotion[next & ~1u];
	const uint32_t *bottom = g_colocatedMotion[next | 1];
	const uint32_t *field = g_colocatedField ? bottom : top;

	waitColocatedReads();
	if (!(next & 1)) {
		address = takeColocated();
		PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0), field, 0);
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), top);
		g_colocatedReads[next].external = address;
		g_colocatedReads[next + 1].external = address + bottomOffset();
	} else {
		PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(1), field, 1);
		PUT_MOTION(VIP_CB_FIELD_COLOCATED(1), bottom);
	}
	if (next == VIP_MOTION_RECORDS - 1)
		queueColocatedReads(&g_colocatedDone);
}

/*
 * By frame or field macroblock pairs. A field macroblock gets the top field's
 * half of a frame pair, in the bottom macroblock too (the firmware's).
 */
void mbaffFromFrameDirectMacroblock(void)
{
	uint32_t next = (g_macroblockCount + 1) & (VIP_MOTION_RECORDS - 1), address;
	const uint32_t *top = g_colocatedMotion[next & ~1u];
	const uint32_t *bottom = g_colocatedMotion[next | 1];
	const uint32_t *field = g_colocatedField ? bottom : top;

	address = takeColocated();
	waitColocatedReads();
	if (!(next & 1)) {
		if (!isFieldMacroblock(top)) {
			PUT_MOTION(VIP_CB_FRAME_COLOCATED(0), top);
			PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(0), top, bottom, 0);
		} else {
			PUT_MOTION(VIP_CB_FIELD_COLOCATED(0), top);
			PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(0), field, 0);
		}
	} else {
		if (!isFieldMacroblock(top)) {
			PUT_MOTION(VIP_CB_FRAME_COLOCATED(1), bottom);
			PUT_FIELD_HALF(VIP_CB_FIELD_COLOCATED(1), top, bottom, 0);
		} else {
			PUT_MOTION(VIP_CB_FIELD_COLOCATED(1), bottom);
			PUT_FRAME_HALF(VIP_CB_FRAME_COLOCATED(1), field, 1);
		}
	}
	g_colocatedReads[next].external = address;
	if (next == VIP_MOTION_RECORDS - 1)
		queueColocatedReads(&g_colocatedDone);
}

/*
 * Without direct_8x8_inference, the SD loop's: put the colocated macroblock
 * preloadColocated read in into the registers of bank 0, and read the next one in
 * over it.
 */
void pushColocatedMotion(void)
{
	const uint32_t *motion = g_colocatedMacroblock;
	uint32_t address = vipReadControlBus(VIP_CB_COLOCATED_DATA);

	vipWriteControlBus(VIP_CB_COLOCATED_COMMAND, VIP_UNIT_START);
	while (g_colocatedDone == 1)
		;
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x0, motion[0]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x2, motion[1]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x4, motion[2]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x6, motion[3]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x10, motion[4]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x11, motion[5]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x12, motion[6]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x13, motion[7]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x14, motion[8]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x15, motion[9]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x16, motion[10]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x17, motion[11]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x18, motion[12]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x19, motion[13]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1a, motion[14]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1b, motion[15]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1c, motion[16]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1d, motion[17]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1e, motion[18]);
	vipWriteControlBus(VIP_CB_MOTION_BANK0 + 0x1f, motion[19]);
	g_colocatedList.external = address;
	g_colocatedList.localLength = (uint32_t)g_colocatedMacroblock << 16 |
		sizeof(g_colocatedMacroblock);
	queueListDma(VIP_COLOCATED_DMA, (uint32_t)&g_colocatedList << 16 |
		sizeof(g_colocatedList), 0, 0, NULL, &g_colocatedDone);
}
