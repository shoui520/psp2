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
	int8_t outputCount;			/* the parser's: the pictures to give out */
	int8_t outputStart;
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

/*
 * The AC/DC prediction's neighbours of a macroblock, in the rows above (64
 * bytes, a macroblock and one): the first rows of the blocks above it, the
 * first columns of the blocks to its left; each DC (the first) dequantised.
 */
typedef struct M4vdecPredictors {
	int16_t rows[2][8];			/* luma: blocks 2 and 3's; chroma: Cb, Cr */
	int16_t columns[2][8];			/* luma: blocks 1 and 3's; chroma: Cb, Cr */
} M4vdecPredictors;

/* where the luma and chroma jobs read the run's coefficients (64 bytes) */
typedef struct M4vdecBlockCursor {
	const int16_t *coefficients;		/* the next block's */
	int16_t aboveLeftDc;			/* the DC of the block above the one before */
	uint8_t reserved[58];
} M4vdecBlockCursor;

/* where the chroma job reads the run's coefficients (64 bytes) */
typedef struct M4vdecChromaCursor {
	const int16_t *coefficients[2];		/* the next Cb and Cr blocks' */
	int16_t aboveLeftDc[2];			/* the DCs of the blocks above the one before */
	uint8_t reserved[52];
} M4vdecChromaCursor;

/*
 * A block in the IVC2 coprocessor while it is decoded: what the luma and
 * chroma helpers hand the kernels (c9-c11, c16-c31).
 */
typedef struct M4vdecBlock {
	int16_t coef[8][8];			/* c16-c31: the coefficients, then the samples */
	int16_t predictor[8];			/* c10, c11: the AC prediction's row or column */
	int16_t dc[4];				/* c9: the DC prediction, in lane 0 */
} M4vdecBlock;

/*
 * A block's reference samples in the IVC2 coprocessor (c14-c31), 9 x 9:
 * what motion compensation interpolates from.
 */
typedef struct M4vdecReference {
	uint8_t rows[9][8];			/* c23-c31: each row's first 8 */
	uint8_t next[9][8];			/* c14-c22: each row one sample on (until then its ninth, first) */
} M4vdecReference;

/* a core's work area for a stripe's jobs (192 bytes, 192 apart) */
typedef struct M4vdecStripe {
	uint8_t x;				/* the macroblock's */
	uint8_t row;
	uint8_t reserved[2];
	uint32_t kind;				/* M4V_KIND_* and the coded blocks */
	const VideoM4vCommand *command;
	M4vdecMbRecord *mb;			/* the macroblock's, in the row's ring */
	M4vdecAbove *above;			/* the macroblock above's, in the rows above */
	M4vdecPredictors *predictors;		/* the luma job's: the macroblock's, in the rows above */
	M4vdecPredictors *chromaPredictors;	/* the chroma job's */
	M4vdecCursor *cursor;
	M4vdecBlockCursor *blockCursor;		/* the luma job's */
	M4vdecChromaCursor *chromaCursor;	/* the chroma job's */
	uint32_t reserved3[3];
	uint8_t aboveAvailable;
	uint8_t reserved4;
	uint8_t leftAvailable;
	uint8_t aboveRightAvailable;
	M4vdecMbRecord *left;			/* the macroblock before's */
	M4vdecAbove *aboveRight;
	int8_t quant;
	int8_t leftQuant;			/* the luma and chroma jobs': the neighbours' quantisers */
	int8_t aboveQuant;
	uint8_t reserved5;
	uint8_t *luma;				/* the macroblock's samples, in the frame */
	uint8_t *chroma;			/* its Cb and Cr, in the frame */
	uint16_t width;				/* the picture's, in samples */
	uint16_t chromaWidth;
	uint16_t height;
	uint16_t chromaHeight;
	int16_t frameWidth;			/* the frame's, in luma samples */
	int16_t chromaFrameWidth;		/* the frame's pitch, for the chroma's tiles */
	const uint8_t *reference;		/* the last picture's luma */
	const uint8_t *referenceChroma;
	uint8_t limit;				/* the first macroblock below not in the packet */
	uint8_t roundingType;			/* vop_rounding_type */
	uint8_t shortHeader;
	uint8_t fcode;				/* the packet's vop_fcode */
	uint8_t reserved11[92];
} M4vdecStripe;

/* the stripes' jobs */
void m4vdecStripeMacroblocks(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x849290 */
void m4vdecStripeLuma(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x849644 */
void m4vdecStripeChroma(uint32_t info, uint32_t row, uint32_t unused,
	uint32_t unused2);						/* 0x8499cc */
void m4vdecRunMacroblocks(M4vdecRun *run, M4vdecWork *work);		/* 0x849d5c */

/* the block kernels (m4vdeckernel.c) */
void m4vdecBlockLoad(M4vdecBlock *block, const int16_t *coefficients);	/* 0x8a8cb0 */
void m4vdecBlockLoadDc(M4vdecBlock *block, const int16_t *coefficients);	/* 0x8a8d40 */
void m4vdecIdctRows(M4vdecBlock *block);					/* 0x8a8d90 */
void m4vdecIdctColumns(M4vdecBlock *block);				/* 0x8a91d0 */
void m4vdecDequantize(M4vdecBlock *block, uint32_t even, uint32_t quant);	/* 0x8a9620 */
void m4vdecBlockStore(const M4vdecBlock *block, uint8_t *samples);	/* 0x8a9998 */
void m4vdecBlockAdd(const M4vdecBlock *block, uint8_t *samples);		/* 0x8a99e8 */
void m4vdecPredictorsStore(const M4vdecBlock *block, int16_t *column,
	int16_t *row);							/* 0x8a9af8 */
void m4vdecChromaBlockLoad(M4vdecBlock *block, const int16_t *coefficients);	/* 0x8a9b38 */
void m4vdecChromaBlockLoadDc(M4vdecBlock *block,
	const int16_t *coefficients);					/* 0x8a9bc8 */
void m4vdecChromaIdctRows(M4vdecBlock *block);				/* 0x8a9c18 */
void m4vdecChromaIdctColumns(M4vdecBlock *block);			/* 0x8aa058 */
void m4vdecChromaDequantize(M4vdecBlock *block, uint32_t even,
	uint32_t quant);						/* 0x8aa4a8 */
void m4vdecChromaBlockStore(const M4vdecBlock *block, uint8_t *samples);	/* 0x8aa820 */
void m4vdecChromaBlockAdd(const M4vdecBlock *block, uint8_t *samples);	/* 0x8aa870 */
void m4vdecChromaPredictorsStore(const M4vdecBlock *block, int16_t *row,
	int16_t *column);						/* 0x8aa980 */
void m4vdecAcPredictLeft(M4vdecBlock *block);				/* 0x8aab00 */
void m4vdecAcPredictLeftScaled(M4vdecBlock *block, int32_t quant,
	int32_t predictorQuant);					/* 0x8aaba0 */
void m4vdecAcPredictTop(M4vdecBlock *block);				/* 0x8aac60 */
void m4vdecAcPredictTopScaled(M4vdecBlock *block, int32_t quant,
	int32_t predictorQuant);					/* 0x8aac80 */
void m4vdecReferenceLeftEdge(M4vdecReference *reference, uint32_t shift);	/* 0x8aacc0 */
void m4vdecReferenceLeftOutside(M4vdecReference *reference);		/* 0x8aad30 */
void m4vdecReferenceRightEdge(M4vdecReference *reference, uint32_t shift);	/* 0x8aad78 */
void m4vdecReferenceRightOutside(M4vdecReference *reference);		/* 0x8aadc0 */
void m4vdecReferenceTopEdge(M4vdecReference *reference, int32_t y);	/* 0x8aade8 */
void m4vdecReferenceBottomEdge(M4vdecReference *reference, int32_t past);	/* 0x8aae40 */
void m4vdecReferenceNext(M4vdecReference *reference);			/* 0x8aae98 */
void m4vdecReferenceLoad(M4vdecReference *reference, const uint8_t *first,
	const uint8_t *second, int32_t stride, uint32_t y, uint32_t shift);	/* 0x8aaec0 */
void m4vdecPredictFull(const M4vdecReference *reference, uint8_t *samples);	/* 0x8ab290 */
void m4vdecPredictAcross(const M4vdecReference *reference, uint8_t *samples);	/* 0x8ab2e0 */
void m4vdecPredictAcrossRounded(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8ab330 */
void m4vdecPredictDown(const M4vdecReference *reference, uint8_t *samples);	/* 0x8ab380 */
void m4vdecPredictDownRounded(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8ab3d0 */
void m4vdecPredictBoth(const M4vdecReference *reference, uint8_t *samples,
	uint32_t round);						/* 0x8ab420 */
void m4vdecChromaReferenceLeftEdge(M4vdecReference *reference,
	uint32_t shift);						/* 0x8ab518 */
void m4vdecChromaReferenceLeftOutside(M4vdecReference *reference);	/* 0x8ab588 */
void m4vdecChromaReferenceRightEdge(M4vdecReference *reference,
	uint32_t shift);						/* 0x8ab5d0 */
void m4vdecChromaReferenceRightOutside(M4vdecReference *reference);	/* 0x8ab618 */
void m4vdecChromaReferenceTopEdge(M4vdecReference *reference, int32_t y);	/* 0x8ab640 */
void m4vdecChromaReferenceBottomEdge(M4vdecReference *reference,
	int32_t past);							/* 0x8ab698 */
void m4vdecChromaReferenceNarrow(M4vdecReference *reference);		/* 0x8ab6f0 */
void m4vdecChromaReferenceNext(M4vdecReference *reference);		/* 0x8ab740 */
void m4vdecChromaReferenceLoad(M4vdecReference *reference, const uint8_t *first,
	const uint8_t *second, int32_t stride, uint32_t y, uint32_t shift);	/* 0x8ab768 */
void m4vdecChromaPredictFull(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8abad0 */
void m4vdecChromaPredictAcross(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8abb20 */
void m4vdecChromaPredictAcrossRounded(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8abb70 */
void m4vdecChromaPredictDown(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8abbc0 */
void m4vdecChromaPredictDownRounded(const M4vdecReference *reference,
	uint8_t *samples);						/* 0x8abc10 */
void m4vdecChromaPredictBoth(const M4vdecReference *reference, uint8_t *samples,
	uint32_t round);						/* 0x8abc60 */

#endif
