/*
 * The AVC decoder's decoder engine (0x82cd90-0x834588): the second of the
 * decoder's two engine threads. It takes the runs of macroblocks the helper
 * parsed, cuts each macroblock row into stripes, and decodes the stripes as
 * VThread jobs on the other cores (prediction, residual, deblocking), a row
 * behind the one above it; then it hands the decoded pictures back to the
 * service.
 */
#ifndef AVCDEC_DECODER_H
#define AVCDEC_DECODER_H

#include "service.h"

/* the engine's memory: its header, its info and its status */
#define DECODER_HEADER_SIZE		128
#define DECODER_INFO_SIZE		128
#define DECODER_STATUS_SIZE		128
#define DECODER_FIXED_SIZE		384	/* all of them */
#define DECODER_COMMAND_SIZE		64	/* a VideoAvcDecodeCommand */
#define DECODER_JOB_SIZE		128	/* a VideoHelperJob */
#define DECODER_RUN_SIZE		64
#define DECODER_WORK_SIZE		64	/* a core's, 192 bytes apart */
#define DECODER_WORK_STRIDE		192
#define DECODER_ROW_SIZE		64
#define DECODER_ROWS			32	/* the rows' ring */
#define DECODER_ROWS_SIZE		2048
#define DECODER_ROW_STRIPES_SIZE	128
#define DECODER_CONTEXT_SIZE		64	/* a VideoHelperContext */
#define DECODER_FRAMES			18	/* the frames, by picture */

/* the engine thread's states */
#define DECODER_STOPPED			0
#define DECODER_IDLE			2	/* waiting for a decode */
#define DECODER_WAITING			3	/* waiting for the parser's picture */
#define DECODER_RUNNING			4

/* the thread's flags */
#define DECODER_FLAG_LOOP		0x00000001	/* look at the queues again */
#define DECODER_FLAG_ANSWERED		0x00000004	/* the decode was answered */
#define DECODER_FLAG_FLUSHED		0x00004000	/* the flush job is done */
#define DECODER_FLAG_EXIT		0x00008000
#define DECODER_FLAG_RUN_DONE		0x00010000	/* the helper's run is decoded */
#define DECODER_FLAG_NEXT_RUN		0x00020000	/* the helper may take the next run */
#define DECODER_FLAG_PICTURE_DONE	0x00040000	/* the picture's last row is decoded */
#define DECODER_FLAG_OUTPUT_DONE	0x00080000	/* the output job is done */
#define DECODER_FLAG_STRIPE_DONE	0x00100000	/* a stripe's jobs are done */
#define DECODER_FLAG_RUN_READ		0x00200000	/* the run's job read the run */
#define DECODER_FLAG_OUTPUT		0x00400000	/* answer with the pictures */
#define DECODER_FLAG_OUTPUT_NONE	0x00800000	/* answer without pictures */
#define DECODER_FLAGS			0x00ffc005

/* the jobs of a decoder's slot on the service's context (index * 8 + n) */
#define DECODER_JOB_REFERENCES		4
#define DECODER_JOB_OUTPUT		5
#define DECODER_JOB_FLUSH		6
#define DECODER_JOB_RUN			7

/* a row's flags, in the row's entry: what its stripes wait for and start */
#define ROW_FLAG_STRIPE_END		0x01	/* a stripe of the row before ends here */
#define ROW_FLAG_LAST			0x02	/* the picture's last row */
#define ROW_FLAG_START_BELOW		0x08	/* the row starts the row two below */
#define ROW_FLAG_ONE_BELOW		0x10	/* that row's stripe has one job */
#define ROW_FLAG_TWO_BELOW		0x20	/* that row's stripe has two jobs */
#define ROW_FLAG_FEWER_BELOW		0x40	/* the stripe below waits for one less */
#define ROW_FLAG_SECOND_ONE		0x80	/* the second waits for one only */

/* the frames' lock and the rows done, in the helper's context */
typedef struct AvcdecLockArea {
	volatile uint8_t *lock;
	uint32_t pending;
	uint32_t doneRows;		/* a bit per row, by row % 32 */
} AvcdecLockArea;

/* the engine's info: what its jobs share (128 bytes) */
typedef struct AvcdecDecoderInfo {
	VkThread *thread;		/* the decoder engine's */
	VkThread *parserThread;
	int32_t id;			/* the decoder's */
	int16_t index;			/* the decoder's slot */
	uint16_t reserved;
	VkVThreadContext *context;	/* the helper's, the stripes' jobs run on it */
	uint16_t jobsPerKind;		/* the stripes' jobs, by kind */
	uint8_t mbSize;			/* 16 */
	uint8_t mode;			/* the command's: macroblocks a stripe */
	uint8_t widthInMbs;
	uint8_t heightInMbs;
	uint8_t reserved2[2];
	uint32_t luma;			/* the picture's frame */
	uint32_t chroma;
	uint32_t motion;		/* its motion after it, 0 none */
	AvcdecLockArea *lockArea;
	uint32_t frameSize;
	uint16_t width;
	uint16_t pitch;
	uint32_t lumaSize;
	uint32_t chromaSize;
	int32_t picOrderCnt;
	uint32_t decoderData;
	int8_t picture;
	uint8_t refIdc;
	uint8_t reserved3[2];
	uint32_t command;		/* the picture's VideoAvcDecodeCommand */
	struct AvcdecDecoderStatus *status;
	uint32_t row0;			/* the helper's context's memory */
	uint32_t row1;
	uint32_t row2;
	uint32_t table1024;
	uint32_t table2048b;
	uint32_t table1024b;
	uint32_t table4096;
	uint32_t table16384;
	uint32_t table8192;
	uint32_t reserved4[3];
} AvcdecDecoderInfo;

/* the pictures decoded and their frames (128 bytes) */
typedef struct AvcdecDecoderStatus {
	uint32_t count;			/* the pictures decoded */
	uint8_t outputCount;		/* the parser's: the pictures to give out */
	uint8_t outputStart;
	uint8_t reserved[2];
	uint32_t decoderData;		/* the VideoAvcFrame records */
	uint32_t frames[DECODER_FRAMES];	/* a picture's frame, raw; 0 none */
	uint32_t reserved2[11];
} AvcdecDecoderStatus;

/* a macroblock row's entry, in the ring of 32 (64 bytes) */
typedef struct AvcdecRow {
	uint32_t job;			/* the run's VideoHelperJob, raw */
	uint8_t x;			/* the stripe's first macroblock */
	uint8_t row;
	uint8_t reserved;
	uint8_t runEnd;			/* the run ends in the stripe */
	uint8_t leftover;		/* the stripe's macroblocks past the run's end */
	uint8_t fromStart;		/* the run took the row from its start */
	uint8_t failed;
	uint8_t reserved2;
	uint16_t count;			/* the stripe's macroblocks */
	uint16_t reserved3;
	uint32_t sliceHeader;
	uint32_t slice;
	uint32_t jobIndex;		/* the stripe's jobs */
	uint32_t common;
	uint8_t flags[32];		/* ROW_FLAG_*, by row / 32 */
} AvcdecRow;

/* the run the helper parsed, as its first job reads it (64 bytes) */
typedef struct AvcdecRun {
	uint32_t job;			/* a VideoHelperJob */
	uint8_t firstX;
	uint8_t firstY;
	int8_t qp;
	int8_t lastQp;			/* the run's last macroblock's */
	VideoAvcMbInfo *lastRow;	/* the start of the last row it read, 0 after */
	int16_t count;
	uint16_t reserved;
	uint32_t sliceHeader;
	uint32_t common;		/* the cores' work areas */
	uint32_t reserved2[10];
} AvcdecRun;

/* a core's work area for the run's first job (64 bytes, 192 apart) */
typedef struct AvcdecWork {
	uint8_t firstX;
	uint8_t firstY;
	uint8_t ended;
	uint8_t reserved;
	uint32_t sliceHeader;
	VideoAvcMbInfo *mbInfo;		/* the job's buffers */
	const VideoHelperJob *job;
	const uint8_t *chromaModes;
	uint32_t reserved2[11];
} AvcdecWork;

/* the engine's header (128 bytes) */
typedef struct AvcdecDecoder {
	int32_t state;			/* DECODER_* */
	AvcdecDecoderInfo *info;
	uint32_t rows;			/* AvcdecRow, 32 */
	uint32_t run;			/* an AvcdecRun */
	uint32_t common;
	AvcdecDecoderStatus *status;
	uint32_t request;		/* the decode's VideoEngineRequest */
	int8_t *rowStripes;		/* by row: the stripes started */
	VkMessageQueue *parsedQueue;	/* the parser engine's and the helper's */
	VkMessageQueue *helperQueue;
	VkMessageQueue *decodeQueue;
	VkMessageQueue *serviceQueue;
	VkVThreadContext *context;	/* the service's */
	uint8_t receive;		/* a message is wanted from the parsed queue */
	uint8_t jobRow;			/* the row of the run to give back */
	uint8_t row;			/* the row the next run starts in */
	uint8_t doneRow;
	uint8_t rowJobs;		/* the stripes of a row, and one */
	uint8_t runEnded;		/* a run ended inside a row */
	uint8_t helperWait;		/* the helper waits for the next run */
	uint8_t runEndRow;
	uint8_t pictureStart;		/* the run starts a picture */
	uint8_t outputStart;		/* the parser's pictures to give out */
	uint8_t outputCount;
	int8_t qp;
	uint32_t slice;			/* the run's slice */
	uint32_t sliceHeader;
	uint8_t missingReference;
	uint8_t lastStripe;		/* the last stripe's jobs: 1 or 2 */
	uint8_t signalRow;		/* the rows whose first stripe was signalled */
	uint8_t lastRow;
	uint32_t waitingRows;		/* a bit per row: its next stripe waits */
	uint32_t reserved[12];
} AvcdecDecoder;

/*
 * A macroblock's kind, the word the decoder engine writes over the first
 * of its VideoAvcMbInfo: from its mb_type, the slice's, and its flags.
 */
#define AVC_MB_PARTITION		0x00000003	/* 3 16x16, 2 16x8, 1 8x16, 0 8x8 */
#define AVC_MB_PARTS_SHIFT		2		/* log2 of the partitions */
#define AVC_MB_INTRA			0x00000200
#define AVC_MB_REF0			0x00000400	/* P_8x8ref0 */
#define AVC_MB_SKIP			0x00000800
#define AVC_MB_SKIP_SHIFT		11
#define AVC_MB_INTRA16_MODE_SHIFT	12		/* Intra16x16PredMode */
#define AVC_MB_CHROMA_MODE_SHIFT	14		/* intra_chroma_pred_mode */
#define AVC_MB_CBP_SHIFT		16		/* coded_block_pattern */
#define AVC_MB_KEEP_RIGHT		0x01000000	/* the macroblock right is intra */
#define AVC_MB_KEEP_BOTTOM		0x02000000	/* a macroblock below is intra */
#define AVC_MB_INVALID			0x0000800c	/* an mb_type out of range */

/* the kind's class: bits 6, 7 and 9 */
#define AVC_MB_CLASS(kind)		(((int32_t)(kind) >> 6) & 0xb)
#define AVC_MB_CLASS_INTRA4X4		8
#define AVC_MB_CLASS_INTRA16X16		9
#define AVC_MB_CLASS_PCM		10

/* the stripes' jobs */
void avcdecStripePredict(uint32_t info, uint32_t row, uint32_t unused, uint32_t unused2);
void avcdecStripeResidual(uint32_t info, uint32_t row, uint32_t unused, uint32_t unused2);
void avcdecStripeFilter(uint32_t info, uint32_t row, uint32_t unused, uint32_t unused2);

#endif
