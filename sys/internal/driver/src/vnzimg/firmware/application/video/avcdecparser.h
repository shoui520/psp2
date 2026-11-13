/*
 * The AVC decoder's parser engine (0x824ca0-0x82cd90): the first of the
 * decoder's two engine threads. It takes the access units, maps their
 * bytes, and parses them with VThread jobs (the slice headers, the
 * reference lists and the tables the decoder engine needs), then hands each
 * picture to the decoder engine's thread.
 */
#ifndef AVCDEC_PARSER_H
#define AVCDEC_PARSER_H

#include "service.h"

/* the engine's memory: its header, its info, the stream, the job and the unit */
#define PARSER_HEADER_SIZE		64
#define PARSER_INFO_SIZE		128
#define PARSER_STREAM_SIZE		128
#define PARSER_JOB_SIZE			192
#define PARSER_NAL_SIZE			1216
#define PARSER_FIXED_SIZE		1728	/* all of them */
#define PICTURE_SIZE			128	/* a VideoAvcPicture */
#define PICTURE_DATA_SIZE		256
#define PICTURE_EXTRA_SIZE		16
#define PARAMETER_SET_SIZE		1088
#define PARAMETER_SET2_SIZE		16
#define MAX_PICTURES			16	/* the frames a stream keeps, the two in work aside */

/* the engine thread's states */
#define PARSER_STOPPED			0
#define PARSER_IDLE			2	/* waiting for an access unit */
#define PARSER_PARSING			3	/* the unit's jobs run */
#define PARSER_STARTING			4	/* a unit taken, its first job running */
#define PARSER_HELPING			5	/* the helper parses the unit's slices */

/* the thread's flags */
#define PARSER_FLAG_AU			0x00000001	/* the access unit queue */
#define PARSER_FLAG_WAKE		0x00000004	/* the wake queue */
#define PARSER_FLAG_CONTEXT		0x00000010	/* the context was set: the unit is done */
#define PARSER_FLAG_DRAIN		0x00000040	/* the stream is drained */
#define PARSER_FLAG_OUTPUT		0x00000080	/* a picture of slices not decoded: output only */
#define PARSER_FLAG_FAILED		0x00000100	/* the unit failed */
#define PARSER_FLAG_EXIT		0x00008000
#define PARSER_FLAG_JOB_DONE		0x00010000	/* the helper or the output job is done */
#define PARSER_FLAG_DECODE		0x00020000	/* a picture to decode: the helper takes its slices */
#define PARSER_FLAG_SEQUENCE		0x00040000	/* only the sequence was asked: the unit is done */
#define PARSER_FLAGS			0x000781d5

/* the sequence parameter set's limits and the profiles the engine knows */
#define AVC_MAX_SPS			32
#define AVC_PROFILE_BASELINE		66
#define AVC_PROFILE_MAIN		77
#define AVC_PROFILE_EXTENDED		88
#define AVC_FRAME_RATE_UNKNOWN		0xff

/* hrd_parameters(): the last CPB's values (12 bytes) */
typedef struct VideoAvcHrd {
	uint8_t cpbCnt;			/* cpb_cnt_minus1 */
	uint8_t bitRateScale;
	uint8_t cpbSizeScale;
	uint8_t cbrFlag;
	uint32_t bitRateValue;		/* bit_rate_value_minus1 */
	uint32_t cpbSizeValue;		/* cpb_size_value_minus1 */
} VideoAvcHrd;

/* vui_parameters(), in the stream's data (256 bytes) */
typedef struct VideoAvcVui {
	uint8_t aspectRatioInfoPresent;
	uint8_t aspectRatioIdc;
	uint8_t reserved[2];
	uint32_t sarWidth;
	uint32_t sarHeight;
	uint8_t overscanInfoPresent;
	uint8_t overscanAppropriate;
	uint8_t videoSignalTypePresent;
	uint8_t videoFormat;
	uint8_t videoFullRange;
	uint8_t colourDescriptionPresent;
	uint8_t colourPrimaries;
	uint8_t transferCharacteristics;
	uint8_t matrixCoefficients;
	uint8_t chromaLocInfoPresent;
	uint8_t timingInfoPresent;
	uint8_t reserved2;
	uint32_t chromaSampleLocTypeTopField;
	uint32_t chromaSampleLocTypeBottomField;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint8_t fixedFrameRate;
	uint8_t nalHrdPresent;
	uint8_t vclHrdPresent;
	uint8_t lowDelayHrd;
	uint8_t picStructPresent;
	uint8_t bitstreamRestriction;
	uint8_t motionVectorsOverPicBoundaries;
	uint8_t reserved3;
	uint32_t maxBytesPerPicDenom;
	uint32_t maxBitsPerMbDenom;
	uint32_t log2MaxMvLengthHorizontal;
	uint32_t log2MaxMvLengthVertical;
	uint32_t numReorderFrames;
	uint32_t maxDecFrameBuffering;
	uint8_t initialCpbRemovalDelayLength;	/* the HRDs' lengths, minus 1 */
	uint8_t cpbRemovalDelayLength;
	uint8_t dpbOutputDelayLength;
	uint8_t timeOffsetLength;
	VideoAvcHrd nalHrd;
	VideoAvcHrd vclHrd;
	uint8_t reserved4[156];
} VideoAvcVui;

/* seq_parameter_set_rbsp(), as the engine keeps it (1088 bytes) */
typedef struct VideoAvcSps {
	uint8_t id;
	uint8_t profile;
	uint8_t level;
	uint8_t constraintSet0;
	uint8_t constraintSet1;
	uint8_t constraintSet2;
	uint8_t constraintSet3;
	uint8_t frameMbsOnly;		/* frame_mbs_only_flag: always 1 */
	uint8_t direct8x8Inference;
	uint8_t gapsInFrameNumAllowed;
	uint8_t log2MaxFrameNum;
	uint8_t picOrderCntType;
	uint8_t log2MaxPicOrderCntLsb;
	uint8_t deltaPicOrderAlwaysZero;
	uint8_t numRefFramesInPicOrderCntCycle;
	uint8_t numRefFrames;
	int32_t offsetForNonRefPic;
	int32_t offsetForTopToBottomField;
	uint32_t maxFrameNum;
	uint32_t maxPicOrderCntLsb;
	uint16_t widthInMbs;
	uint16_t heightInMbs;
	uint16_t width;			/* in pixels */
	uint16_t height;
	uint32_t maxDpbKbytes;		/* the level's MaxDpbMbs, in KiB of frames */
	uint8_t maxDpbFrames;		/* the level's, or max_dec_frame_buffering */
	uint8_t frameCropping;
	uint16_t cropLeft;
	uint16_t cropRight;
	uint16_t cropTop;
	uint16_t cropBottom;
	uint8_t rowsPerRun;		/* macroblock rows a helper run takes at most */
	uint8_t frameRateCode;		/* from the timing info, AVC_FRAME_RATE_UNKNOWN */
	uint8_t vuiPresent;
	uint8_t nalHrdPresent;
	uint8_t vclHrdPresent;
	uint8_t numReorderFrames;
	uint32_t vui;			/* a VideoAvcVui, 0 none */
	int32_t offsetForRefFrame[255];
	uint32_t reserved;
} VideoAvcSps;

/* the SEI messages the engine keeps of a picture, in its picture data (256 bytes) */
#define AVC_SEI_BUFFERING_PERIOD	0x01
#define AVC_SEI_PIC_TIMING		0x02
#define AVC_SEI_RECOVERY_POINT		0x04
#define AVC_SEI_PAN_SCAN		0x08

typedef struct VideoAvcClockTimestamp {
	uint8_t ctType;
	uint8_t nuitFieldBased;
	uint8_t countingType;
	uint8_t fullTimestamp;
	uint8_t discontinuity;
	uint8_t cntDropped;
	uint8_t nFrames;
	uint8_t secondsValue;
	uint8_t minutesValue;
	uint8_t hoursValue;
	uint8_t secondsFlag;
	uint8_t minutesFlag;
	uint8_t hoursFlag;
	uint8_t reserved[3];
	int32_t timeOffset;
} VideoAvcClockTimestamp;

typedef struct VideoAvcSei {
	uint8_t flags;			/* AVC_SEI_*: the messages found */
	uint8_t reserved[3];
	uint32_t seqParameterSetId;	/* buffering_period() */
	uint32_t nalInitialCpbRemovalDelay;
	uint32_t nalInitialCpbRemovalDelayOffset;
	uint32_t vclInitialCpbRemovalDelay;
	uint32_t vclInitialCpbRemovalDelayOffset;
	uint32_t cpbRemovalDelay;	/* pic_timing() */
	uint32_t dpbOutputDelay;
	uint8_t picStruct;
	uint8_t clockTimestampFlag[3];
	VideoAvcClockTimestamp clockTimestamp[3];
	uint32_t recoveryFrameCnt;	/* recovery_point() */
	uint8_t exactMatch;
	uint8_t brokenLink;
	uint8_t changingSliceGroupIdc;
	uint8_t reserved2;
	uint32_t panScanRectId;		/* pan_scan_rect() */
	uint8_t panScanRectCancel;
	uint8_t reserved3[3];
	int32_t panScanRectLeftOffset[3];
	int32_t panScanRectRightOffset[3];
	int32_t panScanRectTopOffset[3];
	int32_t panScanRectBottomOffset[3];
	uint32_t panScanRectRepetitionPeriod;
	uint8_t reserved4[92];
} VideoAvcSei;

/* pic_parameter_set_rbsp(), as the engine keeps it (16 bytes) */
typedef struct VideoAvcPps {
	uint8_t id;
	uint8_t spsId;
	uint8_t entropyCodingMode;
	uint8_t weightedPred;
	uint8_t weightedBipredIdc;
	uint8_t constrainedIntraPred;
	uint8_t transform8x8Mode;
	uint8_t bottomFieldPicOrderInFramePresent;
	uint8_t numRefIdxL0Active;
	uint8_t numRefIdxL1Active;
	uint8_t picInitQp;
	int8_t chromaQpIndexOffset;
	int8_t secondChromaQpIndexOffset;
	uint8_t deblockingFilterControlPresent;
	uint8_t reserved[2];
} VideoAvcPps;

/* the engine's info: what its jobs share (128 bytes) */
typedef struct AvcdecParserInfo {
	VkThread *thread;		/* the parser engine's */
	VkThread *decoderThread;	/* the decoder engine's */
	int32_t id;			/* the decoder's */
	int16_t index;			/* the decoder's slot */
	uint16_t cacheSize;
	uint32_t reserved;
	uint32_t parameterSets;		/* 1088 bytes each, maxStreams of them */
	uint32_t parameterSets2;	/* 16 bytes each, maxFrames of them */
	uint32_t parameterCache;
	uint32_t streamData;		/* 256 bytes a stream */
	uint32_t pictureData;		/* 256 bytes a picture */
	uint32_t decoderData;		/* 2048 bytes */
	uint32_t pictures;		/* VideoAvcPicture */
	uint32_t pictureExtras;		/* 16 bytes each */
	uint16_t commandSize;		/* the service's */
	uint16_t horizontal;
	uint16_t contextHorizontal;	/* the context's, valid when set */
	uint16_t contextVertical;
	uint8_t contextRefFrames;
	uint8_t fieldDecode;
	uint8_t contextOption;
	uint8_t contextSet;
	VideoTime option5;		/* the decoder's options, doubled */
	VideoTime option4;
	uint32_t reserved2[12];
} AvcdecParserInfo;

/* what the engine tells the decoder engine of a picture parsed (a message's word) */
typedef struct AvcdecParsed {
	uint8_t valid;
	uint8_t flush;			/* the stream was flushed */
	uint8_t pictureInfo[2];		/* the unit's, swapped */
} AvcdecParsed;

/* the engine's header (64 bytes) */
typedef struct AvcdecParser {
	int32_t state;			/* PARSER_* */
	AvcdecParserInfo *info;
	uint32_t stream;		/* a VideoStream */
	uint32_t job;			/* a VideoSliceJob */
	uint32_t nal;			/* a VideoNal */
	uint32_t request;		/* the access unit's VideoEngineRequest */
	union {
		AvcdecParsed parsed;
		uint32_t parsedWord;
	};
	uint32_t frames;		/* the pictures' memory */
	uint8_t prepared;		/* the pictures were laid out */
	uint8_t flush;			/* the pictures are flushed out */
	uint8_t output;			/* the unit puts pictures out (no slices for the helper) */
	uint8_t keepError;		/* the output job keeps the unit's error */
	uint8_t option;
	uint8_t reserved[3];
	VkMessageQueue *auQueue;
	VkMessageQueue *serviceQueue;	/* the service's, for the units done */
	VkMessageQueue *helperQueue;
	VkMessageQueue *decoderQueue;	/* the decoder engine's */
	VkMessageQueue *wakeQueue;
	VkVThreadContext *context;
} AvcdecParser;

#endif
