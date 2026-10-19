/*
 * PU1: the picture units, their control-bus registers and their RAM. Each unit n
 * has its enable at 0x7n00 (VIP_CB_UNIT_ENABLE) and its command at 0x7n01:
 * written VIP_UNIT_START, it reads 0 when the unit is done. Unit 5 has four
 * channels that move macroblock data between the units and DRAM, each with its
 * command and the macroblock position it works at. The names are provisional,
 * from what the firmware writes. FW2's codec (fw2/codec) drives the same units.
 */
#pragma once

#include "../common/runtime.h"
#include "../common/dma.h"

/* the picture units on PU1's control bus: each command register reads 0 when idle */
#define VIP_CB_PREDICTION_COMMAND	0x5001
#define VIP_PICTURE_UNITS		5	/* and a sixth, VIP_CB_UNIT_ENABLE(5), without a command */

/* the clocks of the picture units */
#define VIP_CLOCK_PICTURE_UNITS		0x1e	/* VIP_CB_CLOCK_ENABLE */
#define VIP_CLOCK_REQUEST_UNITS		0x7f	/* VIP_CB_CLOCK_REQUEST */

#define VIP_CB_UNIT_COMMAND(unit)	(0x7001 + 0x100 * (unit))
#define VIP_CB_UNIT_ERROR(unit)		(0x7003 + 0x100 * (unit))	/* nonzero: it failed */
#define VIP_UNIT_START			2

/*
 * The macroblock pipeline's buffers between the units: which of its two, three or
 * four a unit uses next. The macroblock loop moves 0 and 2 on at unit 0's start,
 * 1, 3 and 4 at unit 1's, 5, 6 and 7 at unit 3's, 8 and 9 at the deblocking's;
 * 10 (VIP_CB_MACROBLOCK_SLOT) is 8's last but one, the slot the macroblock written
 * out is in. The three-way ones go 0, 1, 2 (nextPipelineSlot), the two-way ones
 * 0, 1, the four-way ones 0 to 3.
 */
#define VIP_CB_PIPELINE_SLOT(n)		(0x6010 + (n))	/* 0-10; FW2 also 13 */

/* unit 0: the slice */
#define VIP_CB_SLICE_PARAMETERS		0x7010	/* g_sliceParameters[] */
#define VIP_CB_SLICE_POSITION		0x7011	/* g_sliceSetup + (position << 17) */
#define VIP_CB_TOP_FIELD_ORDER_CNT	0x7012
#define VIP_CB_BOTTOM_FIELD_ORDER_CNT	0x7013
#define VIP_CB_STANDARD_PICTURE		0x7015	/* g_standardPicture */
#define VIP_CB_COLOCATED_FIELDS		0x7016	/* the colocated picture is a field pair */
#define VIP_CB_SLICE_DATA		0x7018
#define VIP_CB_SLICE_COUNTER		0x701c	/* against VIP_CB_PREDICTION_SLICE */
#define VIP_CB_SLICE_START		0x701e	/* y << 8 | x */
#define VIP_CB_SLICE_LIMIT		0x701f

/* unit 0: the motion vectors of the macroblock decoded, in two banks */
#define VIP_CB_MOTION_BANK0		0x7040	/* 16 registers */
#define VIP_CB_MOTION_BANK1		0x7060

/*
 * written: the colocated motion of the next direct macroblock, in its bank, as a
 * frame macroblock and as a field macroblock use it (8 registers each; direct.c)
 */
#define VIP_CB_FRAME_COLOCATED(bank)	(VIP_CB_MOTION_BANK0 + 0x20 * (bank))
#define VIP_CB_FIELD_COLOCATED(bank)	(VIP_CB_MOTION_BANK0 + 0x20 * (bank) + 8)

/* unit 1: the inverse transform */
#define VIP_CB_SCALING_MATRIX		0x7110	/* 1: the scaling lists at VIP_UNIT_SCALING */

/* the prediction unit */
#define VIP_CB_PREDICTION_ERROR		0x5003	/* nonzero: it failed */
#define VIP_CB_PREDICTION_STATUS	0x5010	/* VIP_PREDICTION_FULL: it takes none */
#define VIP_CB_PREDICTION_SLICE		0x5011
#define VIP_CB_PREDICTION_STAGE		0x5012	/* a pipeline buffer's selector */

#define VIP_PREDICTION_FULL		3

/* unit 2 */
#define VIP_CB_UNIT2_COUNT		0x7210	/* the macroblocks it finished */

/* unit 3: the prediction */
#define VIP_CB_PREDICTION_START_X	0x7310	/* x * 2 */
#define VIP_CB_PREDICTION_MONOCHROME	0x7311	/* chroma_format_idc 0 */
#define VIP_CB_PREDICTION_FIELD_PIC	0x7312
#define VIP_CB_PREDICTION_MBAFF		0x7313	/* mb_adaptive_frame_field_flag */
#define VIP_CB_PREDICTION_WIDTH		0x7316	/* PicWidthInMbs - 1 */
#define VIP_CB_TRANSFORM_8X8_MODE	0x7317
#define VIP_CB_WEIGHTED_PRED		0x7318
#define VIP_CB_WEIGHTED_BIPRED_IDC	0x7319
#define VIP_CB_PREDICTION_SLICE_TYPE	0x731a

/* unit 4: the deblocking */
#define VIP_CB_DEBLOCKING_MBAFF		0x7410
#define VIP_CB_DEBLOCKING_FIELD_PIC	0x7415
#define VIP_CB_DEBLOCKING_MONOCHROME	0x7416

/* unit 5's channels */
#define VIP_CB_CHANNEL_WIDTH		0x7510	/* PicWidthInMbs - 1 */
#define VIP_CB_COLOCATED_COMMAND	0x7520	/* the colocated motion's reads */
#define VIP_CB_COLOCATED_MODE		0x7521
#define VIP_CB_COLOCATED_ADDRESS	0x7522
#define VIP_CB_COLOCATED_LAYOUT		0x7523	/* COLOCATED_* */
#define VIP_CB_COLOCATED_STRIDE		0x7524
#define VIP_CB_COLOCATED_X		0x7525
#define VIP_CB_COLOCATED_Y		0x7526
#define VIP_CB_COLOCATED_DATA		0x7527	/* reads the macroblock's DRAM address */
#define VIP_CB_COLOCATED_ROW		0x7528
#define VIP_CB_MOTION_COMMAND		0x7530	/* the decoded motion's writes */
#define VIP_CB_MOTION_MODE		0x7531	/* 1: every block's, 0: the corners' */
#define VIP_CB_MOTION_BASE		0x7532	/* the picture's motion data in DRAM */
#define VIP_CB_MOTION_LAYOUT		0x7533
#define VIP_CB_MOTION_STRIDE		0x7534
#define VIP_CB_MOTION_X			0x7535
#define VIP_CB_MOTION_Y			0x7536
#define VIP_CB_MOTION_ADDRESS		0x7537
#define VIP_CB_NEIGHBOUR_COMMAND	0x7540	/* the row above's reads */
#define VIP_CB_NEIGHBOUR_MODE		0x7541
#define VIP_CB_NEIGHBOUR_BASE		0x7542	/* in the picture */
#define VIP_CB_NEIGHBOUR_LAYOUT		0x7543
#define VIP_CB_NEIGHBOUR_STRIDE		0x7544
#define VIP_CB_NEIGHBOUR_X		0x7545
#define VIP_CB_NEIGHBOUR_Y		0x7546
#define VIP_CB_NEIGHBOUR_ADDRESS	0x7547	/* the row above's DRAM address */
#define VIP_CB_PICTURE_COMMAND		0x7550	/* the decoded picture's writes */
#define VIP_CB_PICTURE_MODE		0x7551
#define VIP_CB_PICTURE_BASE		0x7552
#define VIP_CB_PICTURE_LAYOUT		0x7553
#define VIP_CB_PICTURE_STRIDE		0x7554
#define VIP_CB_PICTURE_X		0x7555
#define VIP_CB_PICTURE_Y		0x7556
#define VIP_CB_PICTURE_ADDRESS		0x7557	/* the macroblock's DRAM address */

/* unit 6: the reference lists */
#define VIP_CB_LIST_FIELD_PIC		0x7610
#define VIP_CB_LIST_SLICE_TYPE		0x7611
#define VIP_CB_LIST0_MODIFIED		0x7612
#define VIP_CB_LIST1_MODIFIED		0x7613
#define VIP_CB_LIST0_LAST		0x7614	/* the largest reference index */
#define VIP_CB_LIST1_LAST		0x7615
#define VIP_CB_LIST_FRAME_NUM		0x7616
#define VIP_CB_LIST_LOG2_MAX_FRAME_NUM	0x7617	/* log2_max_frame_num_minus4 */
#define VIP_CB_LIST_TOP_ORDER_CNT	0x7618
#define VIP_CB_LIST_BOTTOM_ORDER_CNT	0x7619
#define VIP_CB_COLOCATED_FIELD		0x761a	/* the colocated field pair's field */

/* unit 7: the weighted prediction */
#define VIP_CB_WEIGHT_CHROMA		0x7710	/* chroma_format_idc != 0 */
#define VIP_CB_WEIGHT_LUMA_DENOM	0x7711
#define VIP_CB_WEIGHT_CHROMA_DENOM	0x7712

/* local RAM bank 2, the picture units' working memory, and what the codec keeps there */
#define VIP_UNIT_RAM			0x600000
#define VIP_UNIT_RAM_WORDS		0x4000
#define VIP_UNIT_WEIGHTS		0x0000	/* pred_weight_table */
#define VIP_UNIT_WEIGHTS_SIZE		0x200
#define VIP_UNIT_LIST0_CHANGES		0x0200	/* ref_pic_list_modification */
#define VIP_UNIT_LIST1_CHANGES		0x0280
#define VIP_UNIT_LIST_CHANGES_SIZE	0x80
#define VIP_UNIT_SLICE_STATE		0x2200	/* cleared for each picture */
#define VIP_UNIT_SLICE_STATE_SIZE	0x120
#define VIP_UNIT_OPEN_STATE		0x2b00	/* cleared at open */
#define VIP_UNIT_OPEN_STATE_SIZE	0xc0
#define VIP_UNIT_CODEC_STATE		0x3100	/* FW2: cleared at the codec's open */
#define VIP_UNIT_CODEC_STATE_SIZE	0x120
#define VIP_UNIT_MACROBLOCK_SLOTS	0x3a00	/* by VIP_CB_MACROBLOCK_SLOT */
#define VIP_UNIT_MACROBLOCK_SLOT_SIZE	0x500
#define VIP_UNIT_SLOT_FILL		0x140	/* in a slot: fillMacroblocks' colour */
#define VIP_UNIT_SLOT_FILL_SIZE		0x300
#define VIP_UNIT_FINAL_LIST1		0x0500	/* RefPicList1 as unit 6 made it */
#define VIP_UNIT_IMPLICIT_WEIGHTS	0x5200	/* a macroblock's, by VIP_CB_PIPELINE_SLOT(2) */
#define VIP_UNIT_IMPLICIT_WEIGHTS_SIZE	0x28
#define VIP_UNIT_SCALING		0x2b00	/* the scaling lists, intra then inter */
#define VIP_UNIT_REF_LIST		0x2c00	/* a P slice's RefPicList0 */
#define VIP_UNIT_LIST0			0x2d80	/* a B slice's RefPicList0 */
#define VIP_UNIT_LIST1			0x2f00	/* and RefPicList1 */

/*
 * An entry of the units' reference lists. A field list has one per field; a frame
 * list two per frame, the second giving only the bottom field's order count.
 */
typedef struct VipUnitReference {
	uint32_t picture;		/* VIP_UNIT_PICTURE_* */
	int32_t picOrderCnt;
	uint32_t picNum;		/* VIP_UNIT_PIC_NUM_* */
} VipUnitReference;

#define VIP_UNIT_LIST_ENTRIES		32
#define VIP_UNIT_PICTURE_LONG_TERM	0x80000000
#define VIP_UNIT_PICTURE_FIELDS		0x40000000	/* coded as field pictures */
#define VIP_UNIT_PICTURE_BOTTOM		0x20000000	/* the bottom field */
#define VIP_UNIT_PICTURE_ADDRESS	0x1fffffe0
#define VIP_UNIT_PICTURE_INDEX		0x1f
#define VIP_UNIT_PIC_NUM_LONG_TERM	0x80000000
#define VIP_UNIT_PIC_NUM_MASK		0x3ffff

#define vipUnitList(offset)	((VipUnitReference *)(VIP_UNIT_RAM + (offset)))

/* the picture units' registers (names provisional, from what is written) */
#define VIP_CB_PREDICTION_CONTROL	0x5002
#define VIP_CB_PREDICTION_MODE		0x5013
#define VIP_CB_PREDICTION_SETUP		0x5100	/* the work memory's space */
#define VIP_CB_DIVIDER_COMMAND		0x6201	/* VIP_DIVIDER_*; reads 0 when done */
#define VIP_CB_DIVIDEND			0x6210
#define VIP_CB_DIVISOR			0x6211
#define VIP_CB_QUOTIENT			0x6212
#define VIP_CB_REMAINDER		0x6213
#define VIP_CB_UNIT_MODE(unit)		(0x7002 + 0x100 * (unit))
#define VIP_ENTROPY_UNIT_MODE		6	/* VIP_CB_UNIT_MODE(0) */
#define VIP_CB_SLICE_QP			0x7014	/* SliceQPY */
#define VIP_CB_MACROBLOCK_NUMBER	0x7017	/* g_macroblockCount; 0 at each picture */
#define VIP_CB_SLICE_RUNNING		0x701d	/* 1: another macroblock started; 0 stops */
#define VIP_CB_FIELD_MACROBLOCK		0x7020	/* the last decoded was a field macroblock */
#define VIP_CB_MACROBLOCK_SLOT		VIP_CB_PIPELINE_SLOT(10)
#define VIP_CB_LUMA_LOG2_WEIGHT_DENOM	0x7314
#define VIP_CB_CHROMA_LOG2_WEIGHT_DENOM	0x7315
#define VIP_CB_DEBLOCKING_IDC		0x7411	/* disable_deblocking_filter_idc */
#define VIP_CB_ALPHA_OFFSET		0x7412	/* slice_alpha_c0_offset_div2 */
#define VIP_CB_BETA_OFFSET		0x7413	/* slice_beta_offset_div2 */

#define VIP_DIVIDER_DIVIDE		2

/* Divide with the picture units' divider; the quotient, and the remainder. */
static inline uint32_t hardwareDivide(uint32_t dividend, uint32_t divisor,
	uint32_t *remainder)
{
	vipWriteControlBus(VIP_CB_DIVIDEND, dividend);
	vipWriteControlBus(VIP_CB_DIVISOR, divisor);
	vipWriteControlBus(VIP_CB_DIVIDER_COMMAND, VIP_DIVIDER_DIVIDE);
	while (vipReadControlBus(VIP_CB_DIVIDER_COMMAND))
		;
	*remainder = vipReadControlBus(VIP_CB_REMAINDER);
	return vipReadControlBus(VIP_CB_QUOTIENT);
}

/* the motion a slice's macroblocks leave, and a B slice's colocated macroblock */
#define VIP_MOTION_RECORDS	16	/* the motion saved at a slice's end */
#define VIP_MOTION_RECORD_WORDS	16
#define VIP_MOTION_FIELD_WORD	4	/* its bit 31: a field macroblock */
#define VIP_MOTION_FIELD_SHIFT	31
#define VIP_MOTION_MACROBLOCK_WORDS	20	/* a macroblock's motion, VIP_MOTION_SAVE_ALL */

/* the decoded motion written out, VIP_MOTION_RECORDS records at a time */
#define VIP_MOTION_DMA		(VIP_DMA_LIST | 0x31100)

/* without direct_8x8_inference: the colocated macroblock read in to g_colocatedMacroblock */
#define VIP_COLOCATED_DMA	(VIP_DMA_LIST | 0x31000 | VIP_DMA_READ)

/*
 * The buffers a slice's macroblocks use (the colocated motion, then the motion
 * they leave), and PU0's parse results (g_parsed), which the slice no longer
 * needs: the linker scripts lay the two slice buffers out together and put
 * g_parsed over them, VIP_PARSED_OVERLAP bytes in, as the firmware does, so that
 * the local RAM data fits.
 */
#ifdef VIP_FW2
#define VIP_SLICE_BUFFER(name)	__attribute__((section(".local.program." #name), aligned(8)))
#else
#define VIP_SLICE_BUFFER(name)	__attribute__((section(".task0.bss." #name), aligned(8)))
#endif
#define VIP_PARSED_OVERLAP	0x200

/*
 * The motion a slice's macroblocks leave: with VIP_MOTION_SAVE_INFERRED records
 * that go out VIP_MOTION_RECORDS at a time, with VIP_MOTION_SAVE_ALL each
 * macroblock's at once.
 */
typedef union VipMotionBuffer {
	uint32_t records[VIP_MOTION_RECORDS][VIP_MOTION_RECORD_WORDS];
	struct {
		uint32_t motion[VIP_MOTION_MACROBLOCK_WORDS];
		uint32_t reserved[2 * VIP_MOTION_RECORD_WORDS - VIP_MOTION_MACROBLOCK_WORDS];
		VipDmaDescriptor list;		/* the third record's place */
	} macroblock;
} VipMotionBuffer;

/* g_motionSave: what of a reference picture's motion its pictures keep */
#define VIP_MOTION_SAVE_NONE		0
#define VIP_MOTION_SAVE_INFERRED	1	/* direct_8x8_inference_flag's */
#define VIP_MOTION_SAVE_ALL		2

/* in a picture, the bottom field's lines come VIP_BOTTOM_FIELD_OFFSET after the top's */
#define VIP_BOTTOM_FIELD_OFFSET	0x80000

/* the macroblock loop's DMA descriptors, the picture's writes among them (g_codecDma.loop) */
#define VIP_LOOP_DESCRIPTORS	13

/* the read stream (stream.h): its buffer halves (vip.h), read in from the slice's segments */
#define VIP_STREAM_DMA		(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ)
#define VIP_CB_SLICE_READY0	0x7019
#define VIP_CB_SLICE_READY1	0x701a

/*
 * how PU1 (picture.c, slice.c, codec.c, layout.c) and FW2's codec (fw2/codec) set
 * the units up for a picture
 */

/* the parameters of the picture's slices, by slice type (VIP_CB_SLICE_PARAMETERS) */
#define PARAMETER_CHROMA_SHIFT		31	/* chroma_format_idc != 0 */
#define PARAMETER_MBAFF_SHIFT		30
#define PARAMETER_WIDTH_SHIFT		23	/* pic_width_in_mbs_minus1 */
#define PARAMETER_HEIGHT_SHIFT		16	/* pic_height_in_map_units_minus1 */
#define PARAMETER_FRAME_MBS_SHIFT	15	/* frame_mbs_only_flag */
#define PARAMETER_DIRECT_8X8_SHIFT	14	/* direct_8x8_inference_flag */
#define PARAMETER_WEIGHTING_SHIFT	12	/* weighted_pred_flag, weighted_bipred_idc */
#define PARAMETER_CHROMA_QP_SHIFT	7	/* chroma_qp_index_offset */
#define PARAMETER_TRANSFORM_8X8_SHIFT	6
#define PARAMETER_CONSTRAINED_SHIFT	5	/* constrained_intra_pred_flag */
					/* second_chroma_qp_index_offset in 0-4 */

/* the scaling lists in the units' RAM: of each kind, Y, Cb and Cr 4x4, then Y 8x8 */
#define SCALING_KIND_SIZE	0x70
#define SCALING_4X4		16
#define SCALING_8X8		64
#define SCALING_SIZE		(2 * SCALING_KIND_SIZE)
#define FLAT_SCALING		0x10101010	/* Flat_4x4_16, Flat_8x8_16 */
#define NO_SCALING		0x01010101	/* a profile without scaling matrices */
#define GREY			0x80808080	/* a monochrome picture's chroma */

#define SCALING_DMA		(VIP_DMA_LIST | VIP_DMA_BANK1 | VIP_DMA_READ)
#define FILL_DMA		(VIP_DMA_LIST | VIP_DMA_FILL | VIP_DMA_EXTERNAL_LOCAL)

/* the macroblock loop's writes, their local offset and length */
#define LOOP_LOCAL(offset, length)	((offset) << 16 | (length))

/* the picture's base in DRAM for the units' row above and its writes */
#define NEIGHBOUR_FRAME		0x400048
#define PICTURE_FRAME		0x3fffe8
#define NEIGHBOUR_FIELD		0x400078	/* a field's, or an MBAFF frame's */
#define PICTURE_FIELD		0x3fffb8
#define FRAME_STRIDE		96
#define FIELD_STRIDE		192
#define CHANNEL_LAYOUT		2

/* the entropy decoding's unit 0 commands */
#define ENTROPY_CAVLC		3
#define ENTROPY_CABAC		4

/* the slice's tables go from local RAM to the units' */
#define SLICE_TABLE_DMA		(VIP_DMA_LIST | VIP_DMA_EXTERNAL_LOCAL)

/* the picture units' settings at reset */
#define PREDICTION_SETUP	3	/* VIP_CB_PREDICTION_MODE (provisional) */

/* the motion data's place before the stream is laid out */
#define DEFAULT_COLOCATED	0x4ce0

/* the work memory's rows (layout.c) */
#define VIP_WORK_PITCH		0x2000

/* clearing a picture's motion data: a fill of DRAM from local RAM (the bits above
   VIP_DMA_LIST are not known) */
#define CLEAR_MOTION_DMA	(VIP_DMA_LIST | VIP_DMA_FILL | 0x31000)
