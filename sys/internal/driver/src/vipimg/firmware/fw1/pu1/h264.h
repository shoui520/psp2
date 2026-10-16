/*
 * The H.264 decoder's structures, shared by fw1's PU1 codec and FW2's codec
 * (fw2/codec): the parameter sets as the codec keeps them, and its picture
 * buffers.
 */
#pragma once

#include "frame.h"

/*
 * the decoder's rings (fw1's VipDecoder.rings, FW2's VipCodecDecoder.rings); fw1's
 * debug point's values 0-4 give rings 0, 1, 3, 4, 2
 */
#define VIP_RING_SLICES		0	/* PU0's parsed stream: read */
#define VIP_RING_RECORDS	1	/* its records: read */
#define VIP_RING_OUTPUT		2	/* written */
#define VIP_RING_USER_DATA	3	/* the SEI user data: written, if given */
#define VIP_RING_USER_RECORDS	4	/* its records */

/*
 * PU0's parse results of a record's data, read in from the slice ring a buffer at a
 * time (readParsed). The parameter sets come as PU0's spsParsed and ppsParsed write
 * them.
 */
#define VIP_PARSED_WORDS	232

extern uint32_t g_parsed[VIP_PARSED_WORDS];

/* scaling lists, in raster order */
typedef struct VipScalingMatrix {
	uint8_t lists4x4[6][16];
	uint8_t lists8x8[2][64];
} VipScalingMatrix;

#define VIP_SCALING_LISTS	8

/* profile_idc */
#define VIP_PROFILE_BASELINE	66
#define VIP_PROFILE_MAIN	77
#define VIP_PROFILE_HIGH	100

/* a sequence parameter set */
typedef struct VipSps {
	VipScalingMatrix scaling;
	int8_t valid;
	uint8_t profileIdc;
	uint8_t constraintSet1;		/* constraint_set1_flag: it keeps to Main profile's */
	uint8_t levelIdc;
	uint8_t chroma;			/* chroma_format_idc != 0 */
	uint8_t log2MaxFrameNumMinus4;
	uint8_t picWidthInMbsMinus1;
	uint8_t picHeightInMapUnitsMinus1;
	uint8_t frameMbsOnly;
	uint8_t mbAdaptiveFrameField;
	uint8_t log2MaxPicOrderCntLsbMinus4;
	uint8_t deltaPicOrderAlwaysZero;
	uint8_t picOrderCntType;
	uint8_t reserved0;
	uint16_t numRefFramesInPicOrderCntCycle;
	int32_t offsetForRefFrame[255];
	int32_t offsetForNonRefPic;
	int32_t offsetForTopToBottomField;
	uint8_t maxNumRefFrames;
	uint8_t direct8x8Inference;
	uint8_t scalingMatrixPresent;
	uint8_t reserved1;
	uint8_t scalingListPresent[VIP_SCALING_LISTS];
	uint16_t frameCropLeft;		/* frame_crop_*_offset */
	uint16_t frameCropRight;
	uint16_t frameCropTop;
	uint16_t frameCropBottom;

	/* vui_parameters() (takeVui) */
	uint8_t nalHrdPresent;
	uint8_t vclHrdPresent;
	uint8_t aspectRatioIdc;		/* 0 without aspect_ratio_info */
	uint8_t reserved2;
	uint16_t sarWidth;		/* with aspect_ratio_idc 255 (Extended_SAR) */
	uint16_t sarHeight;
	uint8_t fixedFrameRate;
	uint8_t chromaSampleLocTypeTopField;
	uint8_t chromaSampleLocTypeBottomField;
	uint8_t picStructPresent;
	uint32_t numUnitsInTick;	/* 0 without timing_info */
	uint32_t timeScale;
	uint32_t reserved3;
#ifndef VIP_FW2	/* FW2 keeps each SPS's VUI apart (fw2/codec) */
	int8_t vuiPresent;
	uint8_t reserved4[3];
	uint32_t vui[VIP_VUI_WORDS];	/* PU0's parse results, as they came */
	uint32_t nalHrd[VIP_HRD_WORDS];
	uint32_t vclHrd[VIP_HRD_WORDS];
#endif
} VipSps;

#ifdef VIP_FW2
_Static_assert(sizeof(VipSps) == 0x520, "VipSps");
#else
_Static_assert(sizeof(VipSps) == 0x75c, "VipSps");
#endif

/* a picture parameter set */
typedef struct VipPps {
	VipScalingMatrix scaling;
	int8_t valid;
	uint8_t spsId;
	uint8_t entropyCodingMode;
	uint8_t bottomFieldPicOrderInFramePresent;
	uint8_t numRefIdxL0DefaultActiveMinus1;
	uint8_t numRefIdxL1DefaultActiveMinus1;
	uint8_t weightedPred;
	uint8_t weightedBipredIdc;
	uint8_t picInitQpMinus26;	/* 6 bits, two's complement */
	uint8_t chromaQpIndexOffset;	/* 5 bits, two's complement */
	uint8_t deblockingFilterControlPresent;
	uint8_t transform8x8Mode;	/* bit 16 of the second word (inferred) */
	uint8_t constrainedIntraPred;
	uint8_t secondChromaQpIndexOffset;
	int8_t scalingMatrixPresent;
	uint8_t scalingListPresent[VIP_SCALING_LISTS];
	uint8_t reserved;
} VipPps;

_Static_assert(sizeof(VipPps) == 0xf8, "VipPps");

#define VIP_SPS_COUNT		32
#define VIP_PPS_COUNT		256

/* a 64-bit time as the codec's structures keep it, at word alignment */
typedef uint64_t VipTime __attribute__((aligned(4)));

/*
 * A picture buffer: a frame store of the decoder, with what the codec keeps of the
 * picture in it. Each set of frame stores has VIP_FRAME_SET of them.
 */
typedef struct VipPictureBuffer {
	uint32_t picture;		/* DRAM: the picture */
	uint32_t colocated;		/* DRAM: its motion data */
	VipFrameHandle *frame;
	int8_t reference;		/* VIP_REFERENCE_*: the frame's marking */
	int8_t topReference;		/* each field's */
	int8_t bottomReference;
	uint8_t coding;			/* VIP_CODING_* */
	uint32_t longTermFrameIdx;
	uint32_t frameNum;
	uint32_t generation;		/* g_generation when it was decoded */
	int32_t picNum;			/* PicNum, or LongTermPicNum */
	int32_t topPicOrderCnt;		/* TopFieldOrderCnt */
	int32_t bottomPicOrderCnt;	/* BottomFieldOrderCnt */
	int8_t idle;			/* 1: not being decoded or shown */
	uint8_t index;			/* in its set */
	int8_t ptsValid;
	int8_t timeStarted;		/* g_timeStarted when the PTS was worked out */
	VipTime presentationTime;	/* 33 bits */
	VipTime decodeTime;		/* (provisional) */
#ifndef VIP_FW2	/* FW2's ends here */
	uint8_t reserved3[0x48 - 0x3c];
	uint32_t outputFlags;		/* VIP_OUTPUT_* (provisional) */
#endif
} VipPictureBuffer;

#ifdef VIP_FW2
_Static_assert(sizeof(VipPictureBuffer) == 0x3c, "VipPictureBuffer");
#else
_Static_assert(sizeof(VipPictureBuffer) == 0x4c, "VipPictureBuffer");
#endif

/* a decoded picture's record in the output ring (provisional) */
typedef struct VipOutputRecord {
	uint32_t header;	/* the record type << 16 | VIP_RECORD_* */
	uint32_t pts;		/* bits 31-0 */
	uint32_t flags;		/* VipPictureBuffer.outputFlags; FW2's are 0 */
	VipFrame *frame;
} VipOutputRecord;

/* VipPictureBuffer.coding */
#define VIP_CODING_FRAME	0
#define VIP_CODING_FIELDS	1	/* field pictures */
#define VIP_CODING_MBAFF	2	/* MbaffFrameFlag */

/* VipPictureBuffer.reference, topReference, bottomReference */
#define VIP_REFERENCE_NONE	0
#define VIP_REFERENCE_SHORT	1
#define VIP_REFERENCE_LONG	2
#define VIP_MAX_REFERENCES	16	/* frames a list is made from */

/* g_sliceType: slice_type % 5 */
#define VIP_SLICE_TYPE_P	0
#define VIP_SLICE_TYPE_B	1
#define VIP_SLICE_TYPE_I	2

/* g_motionOwners: the motion data no picture holds */
#define VIP_MOTION_NO_OWNER	0xff

/* g_picStruct: pic_struct (Table D-1) */
#define VIP_PIC_STRUCT_FRAME		0
#define VIP_PIC_STRUCT_TOP_FIELD	1
#define VIP_PIC_STRUCT_BOTTOM_FIELD	2
#define VIP_PIC_STRUCT_TOP_BOTTOM	3
#define VIP_PIC_STRUCT_BOTTOM_TOP	4
#define VIP_PIC_STRUCT_TOP_BOTTOM_TOP	5
#define VIP_PIC_STRUCT_BOTTOM_TOP_BOTTOM 6
#define VIP_PIC_STRUCT_FRAME_DOUBLING	7
#define VIP_PIC_STRUCT_FRAME_TRIPLING	8

/* g_ctType: the first clock timestamp's ct_type */
#define VIP_CT_TYPE_PROGRESSIVE		0
#define VIP_CT_TYPE_INTERLACED		1
#define VIP_CT_TYPE_UNKNOWN		2	/* or no clock timestamp */

/* g_randomAccess: where decoding could start (provisional) */
#define RANDOM_ACCESS_NONE		0
#define RANDOM_ACCESS_RECOVERY		1	/* an I picture or a recovery point SEI */
#define RANDOM_ACCESS_IDR		2
#define RANDOM_ACCESS_BROKEN_LINK	3	/* a recovery point SEI's broken_link_flag */

/*
 * The slice header of the picture being decoded (takePictureHeader): what the
 * reference marking and picture order need of it.
 */
#define VIP_MARKING_OPERATIONS	VIP_SLICE_MARKING_WORDS

typedef struct VipSliceHeader {
	uint8_t idr;
	uint8_t reference;		/* nal_ref_idc != 0 */
	int8_t adaptiveMarking;		/* adaptive_ref_pic_marking_mode_flag */
	uint8_t reserved0;
	uint16_t firstMbInSlice;
	uint16_t frameNum;
	uint8_t fieldPic;
	uint8_t bottomField;
	uint16_t picOrderCntLsb;
	int32_t deltaPicOrderCntBottom;
	int32_t deltaPicOrderCnt[2];
	uint8_t longTermReference;	/* long_term_reference_flag */
	uint8_t reserved1[3];
	/* dec_ref_pic_marking(): operation 0 ends the list */
	uint8_t operation[VIP_MARKING_OPERATIONS];	/* memory_management_control_operation */
	/* difference_of_pic_nums_minus1, or for operation 2 long_term_pic_num */
	uint32_t differenceOfPicNumsMinus1[VIP_MARKING_OPERATIONS];
	uint8_t longTermFrameIdx[VIP_MARKING_OPERATIONS];
	int8_t maxLongTermFrameIdxPlus1[VIP_MARKING_OPERATIONS];
} VipSliceHeader;

_Static_assert(sizeof(VipSliceHeader) == 0xfc, "VipSliceHeader");

/* VipSliceHeader.operation: memory_management_control_operation (Table 7-9) */
#define VIP_MMCO_END			0
#define VIP_MMCO_FORGET_SHORT		1	/* a short-term picture unused */
#define VIP_MMCO_FORGET_LONG		2	/* a long-term picture unused */
#define VIP_MMCO_SHORT_TO_LONG		3
#define VIP_MMCO_MAX_LONG_TERM		4	/* MaxLongTermFrameIdx */
#define VIP_MMCO_FORGET_ALL		5	/* all unused, as after an IDR */
#define VIP_MMCO_CURRENT_TO_LONG	6

/* takePictureHeader's results, both codecs' */
#define VIP_PICTURE_DONE		0
#define VIP_PICTURE_LAID_OUT		1	/* the frame stores were laid out again */
#define VIP_PICTURE_DRAIN		2	/* a new size: drain the old pictures first */
#define VIP_PICTURE_NO_PARAMETERS	(-1)
#define VIP_PICTURE_TOO_LARGE		(-2)
#define VIP_PICTURE_TOO_MANY_FRAMES	(-3)
#define VIP_PICTURE_NOT_PAIRED		(-4)	/* not the second field of the first */

/*
 * The decode's counters for the ARM side (provisional names): fw1's in DRAM
 * (VIP_CODEC_COUNTERS), FW2's in the context.
 */
typedef struct VipCodecCounters {
	uint32_t pictures;		/* g_pictureCount */
	uint32_t reserved0;
	uint32_t outputs;		/* g_outputCount */
	uint32_t reserved1;
	uint32_t underruns;		/* g_underrunCount */
	uint32_t reserved2[3];
	uint32_t skippedPictures;	/* the output skipped them: not decoded */
	uint32_t droppedPictures;	/* the output dropped them at the decode */
	uint32_t bufferTimeouts;	/* no picture buffer came free: the DPB went */
	uint32_t reserved3;
	uint32_t spsErrors;		/* SPS records without data */
	uint32_t ppsErrors;
	uint32_t headerErrors;		/* slice header records without data */
	uint32_t sliceErrors;		/* slices decodeSlice failed */
} VipCodecCounters;

/* scaling.c */
extern const uint8_t g_default4x4[2][16];	/* Default_4x4_Intra, _Inter */
extern const uint8_t g_default8x8[2][64];

uint32_t takeScalingMatrix(VipScalingMatrix *matrix, uint8_t *present, uint32_t word);
