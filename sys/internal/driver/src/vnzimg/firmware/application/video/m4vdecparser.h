/*
 * The MPEG-4 decoder's parser engine (0x841730-0x8471ac): the first of the
 * decoder's two engine threads, as the AVC decoder's (avcdecparser.h). It
 * takes the access units, maps their bytes, and parses them with VThread
 * jobs (the VOL, VOP, GOB and video packet headers), the helper running
 * their macroblocks, then hands each picture to the decoder engine's thread.
 */
#ifndef M4VDEC_PARSER_H
#define M4VDEC_PARSER_H

#include "service.h"

/* the engine's memory: its header, its info, the stream, the job, the unit, the VOL and the VOP */
#define PARSER_HEADER_SIZE		64
#define PARSER_INFO_SIZE		64
#define PARSER_STREAM_SIZE		64
#define PARSER_JOB_SIZE			1408
#define PARSER_UNIT_SIZE		256
/* a run's status: its macroblocks go on, or it ended */
#define RUN_ENDED		3

/* the markers looked for, by their bits */
#define SHORT_VIDEO_START_BITS	22
#define SHORT_VIDEO_START	0x20	/* short_video_start_marker */
#define SHORT_VIDEO_END		0x3f	/* short_video_end_marker */
#define GOB_START_BITS		17
#define START_CODE_PREFIX_BITS	24
#define RESYNC_BITS(vop)	((vop)->codingType ? (vop)->fcode + 16 : 17)

#define PARSER_VOL_SIZE			64
#define PARSER_VOP_SIZE			256
#define PARSER_FIXED_SIZE		2176	/* all of them */
#define PARSER_FRAMES_SIZE		2048	/* the pictures' records */
#define PARSER_TABLE_SIZE		64	/* a VideoM4vTable */
#define PARSER_TABLES			2	/* the tables alternate */
#define PARSER_FRAME_SIZE		128	/* a VideoM4vFrame */

/* the size of the frames when no context gives it */
#define PARSER_DEFAULT_WIDTH		1280
#define PARSER_DEFAULT_HEIGHT		720

/* the engine thread's states */
#define PARSER_STOPPED			0
#define PARSER_IDLE			2	/* waiting for an access unit */
#define PARSER_PARSING			3	/* the unit's jobs run */
#define PARSER_STARTING			4	/* a unit taken, its first job running */
#define PARSER_HELPING			5	/* the helper runs the unit's macroblocks */

/* the engine thread's flags */
#define PARSER_FLAG_AU			0x00000001	/* the access unit queue */
#define PARSER_FLAG_WAKE		0x00000004	/* the wake queue */
#define PARSER_FLAG_CONTEXT		0x00000010	/* the context was set: the unit is done */
#define PARSER_FLAG_DRAIN		0x00000040	/* the stream is drained */
#define PARSER_FLAG_OUTPUT		0x00000080	/* a picture not decoded: output only */
#define PARSER_FLAG_FAILED		0x00000100	/* the unit failed */
#define PARSER_FLAG_EXIT		0x00008000
#define PARSER_FLAG_JOB_DONE		0x00010000	/* the helper or the output job is done */
#define PARSER_FLAG_DECODE		0x00020000	/* a picture to decode: the helper runs it */
#define PARSER_FLAG_SEQUENCE		0x00040000	/* only the sequence was asked: the unit is done */
#define PARSER_FLAGS			0x000781d5

/* the parser's errors (the unit's) */
#define ERROR_START_CODE		8193	/* a start code the stream may not have */
#define ERROR_RESYNC			8192	/* the parser lost its place: on to a resync marker */
#define ERROR_OBJECT_TYPE		8208	/* a profile or video object type not decoded */
#define ERROR_TIME			8229	/* the first times are negative */
#define ERROR_TIME_ORDER		8230	/* the pts is before the dts */
#define ERROR_INTERLACED		16402
#define ERROR_WIDTH			16403	/* beyond the decoder's */
#define ERROR_HEIGHT			16404
#define ERROR_PARTITIONED		16405	/* data partitioning the context does not take */
#define ERROR_HEADER			0x8006	/* a header the decoder does not take, or out of place */
#define ERROR_VOP			0x8007	/* a VOP or packet header the decoder does not take */
#define ERROR_VLC			0x8023	/* a code no VLC table of the macroblock layer has */
#define ERROR_FATAL			16384	/* from here on an error ends the job */
#define ERRORS_FATAL			0xc000

/* the engine's info, read by its jobs (64 bytes) */
typedef struct M4vdecParserInfo {
	VnzKernelThread *thread;		/* the parser engine's */
	VnzKernelThread *decoderThread;	/* the decoder engine's */
	int32_t id;			/* the decoder's */
	int32_t index;			/* the decoder's slot */
	uint32_t reserved;
	uint32_t frames;		/* VideoM4vFrame, the pictures' records */
	uint32_t tables;		/* VideoM4vTable, PARSER_TABLES of them */
	uint16_t commandSize;		/* the service's */
	uint16_t horizontal;
	uint16_t contextHorizontal;	/* the context's, valid when set */
	uint16_t contextVertical;
	uint8_t contextSet;
	uint8_t contextMacroblock;	/* VIDEO_ENGINE_CONTEXT_MACROBLOCK */
	uint8_t reserved2[2];
	VideoTime option5;		/* the decoder's options, doubled */
	VideoTime option4;
	uint32_t reserved3[2];
} M4vdecParserInfo;

/* what the engine tells the decoder engine of a picture parsed (a message's word) */
typedef struct M4vdecParsed {
	uint8_t valid;
	uint8_t flush;			/* the stream was flushed */
	uint8_t pictureInfo[2];		/* the unit's, swapped */
} M4vdecParsed;

/* the engine's header (64 bytes) */
typedef struct M4vdecParser {
	int32_t state;			/* PARSER_* */
	M4vdecParserInfo *info;
	uint32_t stream;		/* a VideoM4vStream */
	uint32_t job;			/* a VideoM4vJob */
	uint32_t unit;			/* a VideoM4vUnit */
	uint32_t request;		/* the access unit's VideoEngineRequest */
	union {
		M4vdecParsed parsed;
		uint32_t parsedWord;
	};
	uint32_t frames;		/* the pictures' memory */
	uint8_t prepared;		/* the pictures were laid out */
	uint8_t flush;			/* the pictures are flushed out */
	uint8_t output;			/* the unit puts pictures out (no run for the helper) */
	uint8_t keepError;		/* the output job keeps the unit's error */
	uint8_t option;
	uint8_t reserved[3];
	VnzKernelMessageQueue *auQueue;
	VnzKernelMessageQueue *serviceQueue;	/* the service's, for the units done */
	VnzKernelMessageQueue *helperQueue;
	VnzKernelMessageQueue *decoderQueue;	/* the decoder engine's */
	VnzKernelMessageQueue *wakeQueue;
	VnzKernelVThreadContext *context;
} M4vdecParser;

/* m4vdecmacroblock.c: a TCOEF code's coefficient */
typedef struct M4vdecCoefficient {
	int16_t last;			/* the block's last */
	int16_t run;			/* the zeros before it */
	int16_t level;
	int16_t sign;			/* 1: negative */
} M4vdecCoefficient;

/* the parser's parts the jobs run (the MPEG-4 engine's) */
void m4vdecVopHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x843736 */
void m4vdecShortHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x84394e */
uint32_t m4vdecGobHeader(VideoM4vJob *job);				/* 0x843b90 */
uint32_t m4vdecPacketHeader(const VideoM4vStream *stream, VideoM4vJob *job);	/* 0x843c16 */
void m4vdecRunUnit(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x843e06 */
void m4vdecRunPartitioned(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x844044 */
void m4vdecRunNotCoded(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x8440f2 */
void m4vdecRunFailed(const VideoHelperShared *shared, const VideoM4vStream *stream,
	VideoM4vJob *job);						/* 0x84415c */
uint32_t m4vdecRunStatus(VideoM4vUnit *unit, VideoBitstream *bits, uint32_t gobStart,
	int32_t count);							/* 0x8458e6 */
/* m4vdecmacroblock.c: the macroblock layer */
void m4vdecRowBuffers(VideoM4vJob *job, const VideoHelperJob *run, uint32_t row);	/* 0x84423c */
uint32_t m4vdecMcbpcIntra(VideoBitstream *bits, VideoM4vJob *job);	/* 0x8442a8 */
uint32_t m4vdecMcbpcInter(VideoBitstream *bits, VideoM4vJob *job);	/* 0x844318 */
void m4vdecCoefficientRead(M4vdecCoefficient *out, VideoBitstream *bits, VideoM4vJob *job,
	uint32_t intra, uint32_t shortHeader);				/* 0x844384 */
void m4vdecCoefficientReadReversible(M4vdecCoefficient *out, VideoBitstream *bits,
	uint32_t intraBlock);						/* 0x844706 */
uint32_t m4vdecCbpy(VideoBitstream *bits, VideoM4vJob *job, uint32_t intra);	/* 0x8454de */
void m4vdecBlockCoefficients(VideoBitstream *bits, VideoM4vJob *job, int32_t *block,
	uint32_t inter, uint32_t reversible, uint32_t dcAsTcoef,
	uint32_t shortHeader);						/* 0x84554a */
int32_t m4vdecMotionCode(VideoBitstream *bits, VideoM4vJob *job);	/* 0x8455e2 */
uint32_t m4vdecDcSize(VideoBitstream *bits, int32_t block);		/* 0x845688 */
uint32_t m4vdecPacketEnd(VideoBitstream *bits, uint32_t resyncBits);	/* 0x845836 */
uint32_t m4vdecMarkerNext(VideoBitstream *bits, uint32_t resyncBits);	/* 0x8458a2 */
uint32_t m4vdecDcAsTcoef(VideoM4vJob *job, uint32_t type, int32_t quant);	/* 0x84593a */
uint32_t m4vdecMacroblockType(uint32_t type);				/* 0x845968 */
void m4vdecMotionVector(VideoM4vJob *job, VideoBitstream *bits, uint32_t index);	/* 0x845992 */
int32_t m4vdecDcDifferential(VideoBitstream *bits, int32_t block);	/* 0x845a62 */
void m4vdecBlocks(VideoM4vJob *job, VideoBitstream *bits, uint32_t type, uint32_t intraH263,
	uint8_t coded, int32_t quantDc, uint32_t dcGiven, uint32_t predict,
	const int16_t *dcs);						/* 0x845ab8 */
void m4vdecMacroblockIntra(VideoM4vJob *job, VideoBitstream *bits, int32_t *quant,
	uint32_t currentQuant);						/* 0x845d48 */
void m4vdecMacroblockInter(VideoM4vJob *job, VideoBitstream *bits, int32_t *quant,
	uint32_t currentQuant);						/* 0x845e88 */
uint32_t m4vdecGobStartNext(VideoM4vJob *job);				/* 0x846044 */
int32_t m4vdecPacketIntra(VideoM4vJob *job, const VideoHelperJob *run);	/* 0x84605e */
int32_t m4vdecRunIntra(VideoM4vJob *job, const VideoHelperJob *run);	/* 0x8464ee */
int32_t m4vdecPacketInter(VideoM4vJob *job, const VideoHelperJob *run);	/* 0x8466fe */
int32_t m4vdecRunInter(VideoM4vJob *job, const VideoHelperJob *run);	/* 0x846c1c */
int32_t m4vdecVopNotCoded(VideoM4vJob *job, const VideoHelperJob *run);	/* 0x846e4c */
void m4vdecDcReset(VideoM4vJob *job);					/* 0x846ef4 */
void m4vdecDcRowStart(VideoM4vJob *job, int32_t column);		/* 0x846f38 */
uint32_t m4vdecDcPredict(VideoM4vJob *job, int16_t differential, int16_t *dcs, int32_t quant,
	int32_t block);							/* 0x846f66 */
void m4vdecDcStore(VideoM4vJob *job, const int16_t *dcs);		/* 0x84704c */
void m4vdecDcSkip(VideoM4vJob *job);					/* 0x8470be */
int32_t m4vdecDcDivide(int32_t value, int32_t scaler);			/* 0x84711a */
uint8_t m4vdecDcScalerLuma(int32_t quant);				/* 0x84713c */
uint8_t m4vdecDcScalerChroma(int32_t quant);				/* 0x847162 */
void m4vdecParserError(const VideoM4vStream *stream, VideoM4vJob *job, uint32_t stage,
	int32_t code);							/* 0x847184 */

#endif
