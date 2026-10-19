/*
 * PU0's bitstream unit (control bus 0x7000) and what the decoder keeps of the
 * stream's parameter sets: shared by fw1's decoder (decoder.h) and FW2's
 * parsers (fw2/parser).
 *
 * The unit reads a NAL unit from the read stream buffer and writes its results to
 * the write stream buffer, each 1 KB in local RAM bank 1 in two 512-byte halves
 * (stream.h has fw1's DMA side of them).
 */
#pragma once

#include "../common/parsed.h"

/* the size of the CABAC state the unit keeps, by chroma */
#define CABAC_ROW_SIZE		0x100
#define CABAC_ROW_SIZE_CHROMA	0x180

/* the CABAC state the unit clears before a slice: 0x1c0 bytes at its 0x800 */
#define CABAC_STATE		0x800
#define CABAC_STATE_SIZE	0x1c0

/* the output space a NAL unit needs, by its type */
#define SPACE_SPS		0x800
#define SPACE_PPS		0x100
#define SPACE_MARKER		0x10
#define SPACE_OTHER		0x1000
#define SPACE_RECORDS		0x40

/* with less input space than this and no record, the input is stuck */
#define INPUT_STUCK		0x280

/* parsed records go out three at a time while an SEI unit is parsed */
#define SEI_RECORDS		3
#define SEI_COPY_WORDS		0x80

/* local RAM bank 1: the bitstream unit's buffers */
#define VIP_BANK1		(VIP_LOCAL_RAM | VIP_LOCAL_BANK1)
#define VIP_NAL_BUFFER		(VIP_BANK1 + VIP_READ_BUFFER)	/* the read stream */
#define VIP_SLICE_BUFFER	(VIP_BANK1 + VIP_WRITE_BUFFER)	/* the write stream */
#define VIP_PARSED_BUFFER	(VIP_BANK1 + 0x800)	/* the unit's parse results */
#define VIP_NAL_BUFFER_SIZE	0x400
#define VIP_BANK1_SAVED		0x1a00	/* what vipSaveHardware keeps of bank 1 */

/* the bitstream unit */
#define VIP_CB_BITSTREAM_COMMAND	0x7001	/* VIP_BITSTREAM_*; reads 0 when done */
#define VIP_CB_BITSTREAM_MODE		0x7002
#define VIP_CB_BITSTREAM_STATUS		0x7003	/* after a parse: nonzero on an error */
#define VIP_CB_BITSTREAM_NAL_HEADER	0x7010	/* nal_ref_idc << 5 | nal_unit_type */
#define VIP_CB_BITSTREAM_SLICE_SETUP	0x7011	/* for VIP_BITSTREAM_SLICE_DATA */
#define VIP_CB_BITSTREAM_CABAC_SETUP	0x7012	/* for VIP_BITSTREAM_CABAC_INIT */
#define VIP_CB_BITSTREAM_PICTURE_SETUP	0x7013	/* for VIP_BITSTREAM_SLICE_HEADER */
#define VIP_CB_BITSTREAM_SMALL_PICTURE	0x7014	/* narrower than the level's limit */
#define VIP_CB_BITSTREAM_PICTURE_WIDTH	0x7015	/* in macroblocks, minus 1 */
#define VIP_CB_BITSTREAM_SLICE_START	0x7016	/* row << 8 | column */
#define VIP_CB_BITSTREAM_NAL_BITS	0x7017	/* the NAL unit's length */
#define VIP_CB_BITSTREAM_READ_LENGTH	0x7018	/* bits to read (0: 32), or signed */
#define VIP_CB_BITSTREAM_READ_VALUE	0x7019
#define VIP_CB_BITSTREAM_GOLOMB_BITS	0x701a	/* the bits an exp-Golomb read took */
#define VIP_CB_BITSTREAM_MORE_DATA	0x701b	/* the NAL unit has more after this */
#define VIP_CB_BITSTREAM_READ_WORDS	0x7020	/* words taken from the read stream */
#define VIP_CB_BITSTREAM_WRITE_WORDS	0x7021	/* the last word written; 0x80: half 1 */

/* VIP_CB_BITSTREAM_COMMAND */
#define VIP_BITSTREAM_LOAD		2	/* start the read stream over */
#define VIP_BITSTREAM_DECODE		3	/* decode to the end of the unit */
#define VIP_BITSTREAM_READ		4
#define VIP_BITSTREAM_READ_GOLOMB	5
#define VIP_BITSTREAM_CABAC_INIT	6
#define VIP_BITSTREAM_SLICE_DATA	7
#define VIP_BITSTREAM_CONTINUE		8	/* after VIP_BITSTREAM_FLUSH */
#define VIP_BITSTREAM_SPS		9
#define VIP_BITSTREAM_PPS		0xa
#define VIP_BITSTREAM_SLICE_HEADER	0xb
#define VIP_BITSTREAM_FLUSH		0xc	/* write out a macroblock row's state */

/* VIP_CB_BITSTREAM_MODE: the NAL unit, or its bits as they are (VUI, SEI) */
#define VIP_BITSTREAM_MODE_NAL		0x7c
#define VIP_BITSTREAM_MODE_RAW		0x4c

/* VIP_CB_BITSTREAM_STATUS */
#define VIP_BITSTREAM_LAST_BITS		0x1f	/* slice data: bits used of its last word */
#define VIP_BITSTREAM_DATA_ERROR	0x40
#define VIP_BITSTREAM_CABAC_SHIFT	5	/* CABAC: 1 while the slice goes on */

/* VIP_CB_BITSTREAM_WRITE_WORDS */
#define VIP_BITSTREAM_WORD_MASK		0x7f
#define VIP_BITSTREAM_HALF1		0x80

/* g_decoderState */
#define VIP_DECODER_IDLE		1
#define VIP_DECODER_BUSY		2
#define VIP_DECODER_SPS			3	/* the unit parses these */
#define VIP_DECODER_PPS			4
#define VIP_DECODER_SLICE_HEADER	5
#define VIP_DECODER_CAVLC		6	/* and decodes slice data */
#define VIP_DECODER_CABAC		7
#define VIP_DECODER_ENDING		8	/* the unit finishes the stream */
#define VIP_DECODER_FAILED		0xa

/* g_errorCode: the class in the high half */
#define VIP_DECODER_ERROR_SPS		0x10000
#define VIP_DECODER_ERROR_PPS		0x10001
#define VIP_DECODER_ERROR_SLICE_HEADER	0x10002
#define VIP_DECODER_ERROR_SEI_TYPE	0x10010
#define VIP_DECODER_ERROR_SEI_SIZE	0x10011
#define VIP_DECODER_ERROR_SLICE_DATA	0x20000
#define VIP_DECODER_ERROR_CLASS		0xffff0000

/*
 * A record of the parsed stream: like the decoder's input records, with
 * a value in the low 12 bits of the header and the timestamp flags above it.
 */
typedef VipRecord VipParsedRecord;

#define VIP_RECORD_FLAGS_MASK		0xf
#define VIP_RECORD_VALUE_MASK		0xfff

/* the parsed records of one NAL unit, before they go out */
#define VIP_PARSED_RECORDS		16

/* hrd_parameters(): what the SEI parsers need of it */
typedef struct VipHrdInfo {
	int8_t present;
	uint8_t cpbCntMinus1;
	uint8_t initialCpbRemovalDelayLengthMinus1;
	uint8_t cpbRemovalDelayLengthMinus1;
	uint8_t dpbOutputDelayLengthMinus1;
	uint8_t timeOffsetLength;
	uint8_t reserved[2];
} VipHrdInfo;

/* a sequence parameter set, as the slice headers and SEI parsers need it */
typedef struct VipSps {
	int8_t valid;
	uint8_t levelIdc;
	uint8_t chroma;			/* chroma_format_idc != 0 */
	uint8_t log2MaxFrameNumMinus4;
	int8_t frameMbsOnly;
	int8_t mbAdaptiveFrameField;
	uint8_t log2MaxPicOrderCntLsbMinus4;
	uint8_t picOrderCntType;
	int8_t deltaPicOrderAlwaysZero;
	uint8_t picWidthInMbsMinus1;
	uint8_t direct8x8Inference;
	int8_t picStructPresent;
	VipHrdInfo nalHrd;
	VipHrdInfo vclHrd;
} VipSps;

/* a picture parameter set */
typedef struct VipPps {
	int8_t valid;
	int8_t entropyCodingMode;	/* 1: CABAC */
	uint8_t spsId;
	uint8_t numRefIdxL0DefaultActiveMinus1;
	uint8_t numRefIdxL1DefaultActiveMinus1;
	int8_t bottomFieldPicOrderInFramePresent;
	uint8_t weightedPred;
	uint8_t weightedBipredIdc;
	uint8_t constrainedIntraPred;
	uint8_t picInitQp;
	uint8_t deblockingFilterControlPresent;
	uint8_t reserved;
} VipPps;

/* the slice header's fields slice data needs */
typedef struct VipSliceInfo {
	uint8_t sliceType;
	int8_t fieldPic;
	int8_t numRefIdxActiveOverride;
	uint8_t numRefIdxL0ActiveMinus1;
	uint8_t numRefIdxL1ActiveMinus1;
	uint8_t cabacInitIdc;
	uint8_t qp;
	uint8_t reserved;
} VipSliceInfo;
