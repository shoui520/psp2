/*
 * fw1: the parsed stream, from PU0 to PU1. PU0 parses each NAL unit with its
 * bitstream unit and writes a record of it to the records ring, and the parse
 * results, the record's data, to the slice ring (pu0/decoder.c); PU1 reads both
 * (pu1/decode.c).
 */
#pragma once

#include "runtime.h"

/*
 * A record of a record ring: what a part of the stream is, where it is and the
 * timestamps that came with it. The demultiplexer's records (pu0/demux.h), the
 * parsed stream's and the user data's (pu1/sei.c) all have this form.
 */
typedef struct VipRecord {
	uint32_t header;		/* type << 16 | VIP_RECORD_* */
	uint32_t pts;			/* bits 0-31 */
	uint32_t dts;
	uint32_t position;		/* the data's ring position */
} VipRecord;

#define VIP_RECORD_TYPE_SHIFT		16
#define VIP_RECORD_PTS			0x8000
#define VIP_RECORD_DTS			0x4000
#define VIP_RECORD_PTS_HIGH		0x2000	/* bit 32 of the timestamps */
#define VIP_RECORD_DTS_HIGH		0x1000
#define VIP_RECORD_FLAGS_SHIFT		12

/* a record header's VIP_RECORD_* timestamp flags, shifted down */
#define VIP_TIME_PTS		(VIP_RECORD_PTS >> VIP_RECORD_FLAGS_SHIFT)
#define VIP_TIME_DTS		(VIP_RECORD_DTS >> VIP_RECORD_FLAGS_SHIFT)
#define VIP_TIME_PTS_HIGH	(VIP_RECORD_PTS_HIGH >> VIP_RECORD_FLAGS_SHIFT)
#define VIP_TIME_DTS_HIGH	(VIP_RECORD_DTS_HIGH >> VIP_RECORD_FLAGS_SHIFT)
#define VIP_TIME_FLAGS		0xf

#define VIP_PTS_MASK		0x1ffffffffull	/* 33-bit PTS and DTS, in 90 kHz units */

/* a record's DTS or PTS, with bit 32 from its flags */
static inline uint64_t recordDts(uint32_t flags, uint32_t low)
{
	return (uint64_t)(flags & VIP_TIME_DTS_HIGH ? 1 : 0) << 32 | low;
}

static inline uint64_t recordPts(uint32_t flags, uint32_t low)
{
	return (uint64_t)(flags & VIP_TIME_PTS_HIGH ? 1 : 0) << 32 | low;
}

/*
 * The record types of an H.264 stream's NAL units: the decoder's input (in fw1
 * the demultiplexer's output, in FW2 the scanner's).
 */
#define VIP_RECORD_AVC_START		0x3300	/* a stream starts here */
#define VIP_RECORD_AVC_ZEROS		0x3301	/* zero bytes */
#define VIP_RECORD_AVC_PICTURE		0x3302	/* a slice that starts a picture */
#define VIP_RECORD_AVC_SLICE		0x3303
#define VIP_RECORD_AVC_SEI		0x3304
#define VIP_RECORD_AVC_SPS		0x3305
#define VIP_RECORD_AVC_PPS		0x3306
#define VIP_RECORD_AVC_DELIMITER	0x3307
#define VIP_RECORD_AVC_END_SEQUENCE	0x3308
#define VIP_RECORD_AVC_END_STREAM	0x3309
#define VIP_RECORD_AVC_OTHER		0x330a

/* the parsed stream's record types */
#define VIP_PARSED_START		0x3310	/* the stream starts over */
#define VIP_PARSED_PADDING		0x3311	/* zero bytes after the unit before */
#define VIP_PARSED_PICTURE_HEADER	0x3312	/* a picture's first slice header */
#define VIP_PARSED_SLICE_HEADER		0x3313
#define VIP_PARSED_CABAC_DATA		0x3314
#define VIP_PARSED_PADDING_DATA		0x3315
#define VIP_PARSED_CAVLC_DATA		0x3316
#define VIP_PARSED_VUI			0x3317
#define VIP_PARSED_SEI			0x3318	/* the payload as it is; its type << 4 */
#define VIP_PARSED_SEI_OTHER		0x3319
#define VIP_PARSED_BUFFERING_PERIOD	0x331a
#define VIP_PARSED_PICTURE_TIMING	0x331b
#define VIP_PARSED_PAN_SCAN		0x331c
#define VIP_PARSED_USER_DATA		0x331d	/* registered */
#define VIP_PARSED_USER_DATA_UUID	0x331e	/* unregistered */
#define VIP_PARSED_RECOVERY_POINT	0x331f
#define VIP_PARSED_RESERVED		0x3320	/* not written; PU1 passes over it */
#define VIP_PARSED_SPS			0x3321
#define VIP_PARSED_PPS			0x3322
#define VIP_PARSED_DELIMITER		0x3323
#define VIP_PARSED_END_SEQUENCE		0x3324
#define VIP_PARSED_END_STREAM		0x3325
#define VIP_PARSED_OTHER		0x3326
/* ends a picture that has begun (lookAhead); nothing here writes it (provisional) */
#define VIP_PARSED_PICTURE_END		0x3102

/*
 * FW2: the ends the ARM side marks in the input, as records (parser0's scanner
 * writes them); the parser passes them on to the codec.
 */
#define VIP_RECORD_PICTURE_END		VIP_PARSED_PICTURE_END	/* the command ends */
#define VIP_RECORD_STREAM_END		0x3103	/* the parser closes */
#define VIP_PARSED_STREAM_END		VIP_RECORD_STREAM_END

/* FW2's SEI message besides fw1's */
#define VIP_PARSED_FRAME_PACKING	0x3327	/* frame_packing_arrangement() */

/* a padding record's value: the bits of the data's last byte */
#define VIP_PADDING_BITS_SHIFT		9

/* a slice header record's value */
#define VIP_PARSED_FIELD_PIC_SHIFT	11	/* field_pic_flag */
#define VIP_PARSED_FIELD_PIC		(1 << VIP_PARSED_FIELD_PIC_SHIFT)

/* pic_timing()'s words */
#define VIP_TIMING_CPB_REMOVAL_DELAY	0
#define VIP_TIMING_DPB_OUTPUT_DELAY	1
#define VIP_TIMING_PIC_STRUCT		2	/* pic_struct << 28, clock_timestamp_flag[i] << 27 - i */
#define VIP_TIMING_PIC_STRUCT_SHIFT	28
#define VIP_TIMING_CLOCK_TIMESTAMP	27
#define VIP_TIMING_CLOCK		3	/* the first clock timestamp: ct_type in bits 31-30 */
#define VIP_TIMING_CT_TYPE_SHIFT	30

/*
 * A slice header's parse results: VIP_SLICE_HEADER_WORDS words, then the parts
 * the first word says are there, in this order: list 0's ref_pic_list_modification
 * (if the lists are left to PU1), pred_weight_table, dec_ref_pic_marking, and
 * list 1's ref_pic_list_modification.
 */
#define VIP_SLICE_HEADER_WORDS		8
#define VIP_SLICE_LIST_WORDS		0x40	/* ref_pic_list_modification, both lists */
#define VIP_SLICE_WEIGHT_WORDS		0x80	/* pred_weight_table */
#define VIP_SLICE_MARKING_WORDS		0x20	/* dec_ref_pic_marking */
#define VIP_SLICE_LIST_ENTRIES		32	/* modification_of_pic_nums_idc << 29 | the value */
#define VIP_SLICE_LIST_END		(3u << 29)	/* modification_of_pic_nums_idc 3 */

/* the slice header words' fields, as PU0's bitstream unit writes them */
#define VIP_SLICE_FIRST_MB_SHIFT		16	/* first word: first_mb_in_slice, 13 bits */
#define VIP_SLICE_TYPE_SHIFT		12	/* slice_type, 4 bits */
#define VIP_SLICE_PPS_ID_SHIFT		4	/* pic_parameter_set_id, 8 bits */
#define VIP_SLICE_IDR			3
#define VIP_SLICE_REFERENCE			2	/* nal_ref_idc != 0 */
#define VIP_SLICE_FRAME_NUM_SHIFT		16	/* second word */
#define VIP_SLICE_FIELD_PIC			15
#define VIP_SLICE_BOTTOM_FIELD		14
#define VIP_SLICE_POC_LSB			2	/* pic_order_cnt_lsb */
#define VIP_SLICE_DELTA_POC_BOTTOM		3	/* delta_pic_order_cnt_bottom */
#define VIP_SLICE_DELTA_POC			4	/* delta_pic_order_cnt[0], [1] */
#define VIP_SLICE_LISTS			6
#define VIP_SLICE_DIRECT_SPATIAL		31	/* direct_spatial_mv_pred_flag */
#define VIP_SLICE_OVERRIDE			30	/* num_ref_idx_active_override_flag */
#define VIP_SLICE_L0_ACTIVE_SHIFT		25	/* num_ref_idx_l0_active_minus1, 5 bits */
#define VIP_SLICE_L1_ACTIVE_SHIFT		20
#define VIP_SLICE_L0_MODIFIED		19	/* ref_pic_list_modification_flag_l0 */
#define VIP_SLICE_L1_MODIFIED		18
#define VIP_SLICE_LUMA_DENOM_SHIFT		15	/* luma_log2_weight_denom, 3 bits */
#define VIP_SLICE_CHROMA_DENOM_SHIFT	12
#define VIP_SLICE_CODING			7
#define VIP_SLICE_NO_OUTPUT_OF_PRIOR_PICS	31
#define VIP_SLICE_LONG_TERM_REFERENCE	30
#define VIP_SLICE_QP_DELTA_SHIFT		21	/* slice_qp_delta, 7 bits */
#define VIP_SLICE_DEBLOCKING_SHIFT		19	/* disable_deblocking_filter_idc, 2 bits */
#define VIP_SLICE_ALPHA_SHIFT		15	/* slice_alpha_c0_offset_div2, 4 bits */
#define VIP_SLICE_BETA_SHIFT		11

/* bits of the first word */
#define VIP_SLICE_NEEDS_LISTS		31	/* the lists are left to PU1 */
#define VIP_SLICE_HAS_WEIGHTS		30
#define VIP_SLICE_HAS_MARKING		29	/* adaptive_ref_pic_marking_mode_flag */

/*
 * A VIP_PARSED_VUI record's parse results: the NAL HRD's words, the VCL HRD's,
 * then the VUI's own.
 */
#define VIP_HRD_WORDS			68	/* the present flag at bit 31 of the first */
#define VIP_VUI_WORDS			6
#define VIP_VUI_NAL_HRD			0
#define VIP_VUI_VCL_HRD			VIP_HRD_WORDS
#define VIP_VUI_FIRST			(2 * VIP_HRD_WORDS)

/* the parameter sets: how many there may be */
#define VIP_SPS_COUNT			32
#define VIP_PPS_COUNT			256

/* the words of their parse results: the optional tables follow the fixed words */
#define SPS_WORDS			7
#define PPS_WORDS			2
#define SCALING_LIST_WORDS		0x39
#define REF_FRAME_OFFSET_WORDS		0xff

/* dec_ref_pic_marking()'s words */
#define MARKING_OPERATION_SHIFT		29
#define MARKING_DIFFERENCE_SHIFT	11	/* difference_of_pic_nums_minus1, 18 bits */
#define MARKING_LONG_TERM_IDX_SHIFT	6
#define MARKING_MAX_LONG_TERM_SHIFT	1

/* the SEI records' parse results */
#define VIP_BUFFERING_PERIOD_WORDS	130
#define VIP_PICTURE_TIMING_WORDS	12
#define VIP_PAN_SCAN_WORDS		16
#define VIP_RECOVERY_POINT_WORDS	2
