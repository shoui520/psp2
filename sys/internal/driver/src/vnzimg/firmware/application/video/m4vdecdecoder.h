/*
 * The MPEG-4 decoder's decoder engine (0x8471ac-0x84b4b0): the second of
 * the decoder's two engine threads. It is the AVC decoder's engine
 * (avcdecdecoder.c) on records of its own: the runs the helper parsed are
 * cut into stripes of macroblocks, a stripe's macroblocks read, then its
 * luma and chroma decoded, as VThread jobs on the other cores, a row behind
 * the one above; the decoded pictures go back to the service.
 */
#ifndef M4VDEC_DECODER_H
#define M4VDEC_DECODER_H

#include "avcdecdecoder.h"	/* the engines' states, flags, jobs and row flags */

/* the engine's memory: its header, its info and its status */
#define M4VDEC_DECODER_FIXED_SIZE	320
#define M4VDEC_DECODER_STATUS_SIZE	64
#define M4VDEC_DECODER_FRAMES		2	/* the frames kept, by table */
#define M4VDEC_STRIPE_SIZE		192	/* a core's M4vdecStripe */

/* the engine's info: what its jobs share (128 bytes) */
typedef struct M4vdecDecoderInfo {
	VnzKernelThread *thread;		/* the decoder engine's */
	VnzKernelThread *parserThread;
	int32_t id;				/* the decoder's */
	int16_t index;				/* the decoder's slot */
	uint16_t reserved;
	VnzKernelVThreadContext *context;	/* the helper's, the stripes' jobs run on it */
	uint16_t jobsPerKind;			/* the stripes' jobs, by kind */
	uint8_t mbSize;				/* 16 */
	uint8_t mode;				/* the command's: macroblocks a stripe */
	uint16_t widthInMbs;
	uint16_t heightInMbs;
	uint32_t luma;				/* the picture's frame */
	uint32_t chroma;
	AvcdecLockArea *lockArea;
	uint32_t frameSize;
	uint16_t width;
	uint16_t pitch;
	uint32_t chromaOffset;			/* from the luma */
	uint32_t pictureSize2;
	uint32_t frames;			/* the VideoM4vFrame records */
	int8_t table;				/* the picture's */
	uint8_t reserved2[3];
	const VideoM4vCommand *command;		/* the picture's */
	struct M4vdecDecoderStatus *status;
	uint32_t row0;				/* the helper's context's memory: the rows above */
	uint32_t row1;
	uint32_t row2;
	uint32_t table1024;
	uint32_t table2048b;
	uint32_t table1024b;
	uint32_t table4096;
	uint32_t referenceLuma;			/* the last picture's frame, 0 none */
	uint32_t referenceChroma;
	uint32_t reserved3[5];
} M4vdecDecoderInfo;

/* the pictures decoded and their frames (64 bytes) */
typedef struct M4vdecDecoderStatus {
	uint32_t count;				/* the pictures decoded */
	uint8_t outputCount;			/* the parser's: the pictures to give out */
	uint8_t outputStart;
	uint8_t reserved[2];
	uint32_t frames;			/* the VideoM4vFrame records */
	int8_t table;				/* the last picture's */
	uint8_t reserved2[3];
	uint32_t frame[M4VDEC_DECODER_FRAMES];	/* a table's frame, raw; 0 none */
	uint32_t reserved3[10];
} M4vdecDecoderStatus;

/* a macroblock row's entry, in the ring of 32 (64 bytes) */
typedef struct M4vdecRow {
	uint32_t job;				/* the run's VideoHelperJob, raw */
	uint8_t x;				/* the stripe's first macroblock */
	uint8_t row;
	uint8_t count;				/* the stripe's macroblocks */
	uint8_t reserved;
	uint8_t runEnd;				/* the run ends in the stripe */
	uint8_t leftover;			/* the stripe's macroblocks past the run's end */
	uint8_t fromStart;			/* the run took the row from its start */
	uint8_t reserved2;
	uint32_t jobIndex;			/* the stripe's jobs */
	uint32_t common;
	uint8_t flags[32];			/* ROW_FLAG_*, by row / 32 */
	uint8_t reserved3[12];
} M4vdecRow;

/* the run the helper parsed, as its first job reads it (64 bytes) */
typedef struct M4vdecRun {
	uint32_t job;				/* a VideoHelperJob */
	uint8_t firstX;
	uint8_t firstY;
	uint8_t count;
	uint8_t quant;				/* the run's last macroblock's */
	uint32_t common;			/* the cores' work areas */
	uint32_t reserved2[13];
} M4vdecRun;

/* a core's work area for the run's first job (64 bytes, 192 apart) */
typedef struct M4vdecWork {
	uint8_t firstX;
	uint8_t firstY;
	uint8_t reserved[2];
	VideoM4vMacroblock *macroblocks;	/* the job's first buffer */
	const VideoHelperJob *job;
	uint32_t reserved2[13];
} M4vdecWork;

/* the engine's header (128 bytes) */
typedef struct M4vdecDecoder {
	int32_t state;				/* DECODER_* */
	M4vdecDecoderInfo *info;
	uint32_t rows;				/* M4vdecRow, 32 */
	uint32_t run;				/* an M4vdecRun */
	uint32_t common;
	M4vdecDecoderStatus *status;
	uint32_t request;			/* the decode's VideoEngineRequest */
	int8_t *rowStripes;			/* by row: the stripes started */
	VnzKernelMessageQueue *parsedQueue;	/* the parser engine's and the helper's */
	VnzKernelMessageQueue *helperQueue;
	VnzKernelMessageQueue *decodeQueue;
	VnzKernelMessageQueue *serviceQueue;
	VnzKernelVThreadContext *context;	/* the service's */
	uint8_t receive;			/* a message is wanted from the parsed queue */
	uint8_t jobRow;				/* the row of the run to give back */
	uint8_t row;				/* the row the next run starts in */
	uint8_t doneRow;
	uint8_t rowJobs;			/* the stripes of a row, and one */
	uint8_t runEnded;			/* a run ended inside a row */
	uint8_t helperWait;			/* the helper waits for the next run */
	uint8_t runEndRow;
	uint8_t pictureStart;			/* the run starts a picture */
	uint8_t outputStart;			/* the parser's pictures to give out */
	uint8_t outputCount;
	uint8_t lastStripe;			/* the last stripe's jobs: 1 or 2 */
	uint8_t signalRow;			/* the rows whose first stripe was signalled */
	uint8_t lastRow;
	uint8_t reserved[2];
	uint32_t waitingRows;			/* a bit per row: its next stripe waits */
	uint32_t reserved2[14];
} M4vdecDecoder;

/* a macroblock's kind, as the run's first job writes it over the parser's type (0x8fc174) */
#define M4V_KIND_INTRA			0x01
#define M4V_KIND_NOT_CODED		0x02	/* no vectors, no blocks */
#define M4V_KIND_FOUR_VECTORS		0x04
#define M4V_KIND_QUANT			0x40	/* a dquant */
#define M4V_KIND_MASK			0x7f
#define M4V_KIND_AC_PREDICTION		0x80	/* ac_pred_flag */

/* the parser's VideoM4vMacroblock once the run's first job read it (8 bytes) */
typedef struct M4vdecMacroblock {
	union {
		uint32_t word;
		struct {
			uint8_t kind;		/* M4V_KIND_* */
			uint8_t blocks;		/* the blocks coded, by 1 << (5 - block) */
			uint8_t acCoded;
			uint8_t dcCoded;
		};
	};
	int8_t quant;				/* the macroblock's quantiser */
	uint8_t reserved[3];
} M4vdecMacroblock;

/* a motion vector, in half samples */
typedef union M4vVector {
	uint32_t word;
	struct {
		int16_t x;
		int16_t y;
	};
} M4vVector;

/* a macroblock of the row above, for the one below (64 bytes, a macroblock and one) */
typedef struct M4vdecAbove {
	uint32_t kind;				/* its M4vdecMacroblock's word */
	M4vVector vectors[2];			/* its blocks 2 and 3's */
	int8_t quant;
	uint8_t available;			/* in the packet: it may be predicted from */
	uint8_t reserved[50];
} M4vdecAbove;

/* a macroblock as its stripe's jobs share it, in the row's ring (32 bytes) */
typedef struct M4vdecMbRecord {
	uint32_t kind;				/* its M4vdecMacroblock's word */
	int8_t quant;
	uint8_t available;			/* read: the macroblock after may predict from it */
	int8_t leftQuant;
	int8_t aboveQuant;
	M4vVector vectors[4];			/* by block */
	M4vVector chromaVector;
	uint32_t reserved;
} M4vdecMbRecord;

/* where the stripe's first job reads the run's buffers (64 bytes) */
typedef struct M4vdecCursor {
	const M4vdecMacroblock *macroblock;	/* the next */
	const M4vVector *differences;		/* the next motion vector difference */
	uint32_t aboveLeftKind;			/* the macroblock above the one before's */
	uint8_t aboveLeftAvailable;
	uint8_t reserved[51];
} M4vdecCursor;

/* a core's work area for a stripe's jobs (192 bytes, 192 apart) */
typedef struct M4vdecStripe {
	uint8_t x;				/* the macroblock's */
	uint8_t row;
	uint8_t reserved[2];
	uint32_t kind;				/* M4V_KIND_* and the coded blocks */
	const VideoM4vCommand *command;
	M4vdecMbRecord *mb;			/* the macroblock's, in the row's ring */
	M4vdecAbove *above;			/* the macroblock above's, in the rows above */
	uint32_t reserved2[2];
	M4vdecCursor *cursor;
	uint32_t reserved3[5];
	uint8_t aboveAvailable;
	uint8_t reserved4;
	uint8_t leftAvailable;
	uint8_t aboveRightAvailable;
	M4vdecMbRecord *left;			/* the macroblock before's */
	M4vdecAbove *aboveRight;
	int8_t quant;
	uint8_t reserved5[31];
	uint8_t limit;				/* the first macroblock below not in the packet */
	uint8_t reserved6;
	uint8_t shortHeader;
	uint8_t fcode;				/* the packet's vop_fcode */
	uint8_t reserved7[92];
} M4vdecStripe;

/* the coefficients two blocks of the row above give the ones below them (64 bytes) */
typedef struct M4vdecAboveBlocks {
	struct {
		int16_t dc;
		int16_t ac[7];			/* the first row's */
	} blocks[2];				/* luma: blocks 2 and 3; chroma: Cb and Cr */
	uint8_t reserved[32];
} M4vdecAboveBlocks;

/* the stripes' jobs */
void m4vdecStripeMacroblocks(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x849290 */
void m4vdecStripeLuma(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x849644 */
void m4vdecStripeChroma(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x8499cc */
void m4vdecRunMacroblocks(M4vdecRun *run, M4vdecWork *work);		/* 0x849d5c */

#endif
