/*
 * The codec service (0x85fdd4-): one thread that takes the codecs'
 * requests, keeps a slot per decoder and encoder, and runs each codec's
 * engine as two threads of its own; plus a helper thread for the jobs the
 * engines share.
 */
#ifndef VIDEO_SERVICE_H
#define VIDEO_SERVICE_H

#include "video.h"

#define VIDEO_NO_THREAD			((VkThread *)-1)
#define SERVICE_CORE			(VNZ_FIRST_CORE_ID + 7)
#define SERVICE_PRIORITY		3

/* the thread flags the service waits on */
#define SERVICE_FLAG_ENGINE		0x0002
#define SERVICE_FLAG_ENGINE2		0x0008
#define SERVICE_FLAG_REQUEST		0x0200
#define SERVICE_FLAG_RELEASE		0x2000
#define SERVICE_FLAG_EXIT		0x8000

/* a request's size: the service's, the engines' and the helper's */
#define REQUEST_SIZE			128

/* a codec's slot: its engine's threads and queues, the client's queue */
typedef struct VideoServiceSlot {
	uint32_t memory;		/* the engine's: the first thread's, then the second's */
	VkThread *thread;
	VkThread *thread2;
	VkMessageQueue *auQueue;	/* the access units, to the engine's thread taking them */
	VkMessageQueue *decodeQueue;	/* the decodes */
	VkMessageQueue *wakeQueue;
	VkMessageQueue *innerQueue;	/* between the engine's threads */
	uint32_t auRequests;		/* two of 128 bytes, used in turn */
	uint32_t decodeRequests;	/* two of 128 bytes, used in turn */
	uint8_t auIndex;
	uint8_t decodeIndex;
	int16_t id;			/* -1 free */
	VkThread *clientThread;
	VkMessageQueue *clientQueue;
	uint32_t replies;		/* two of 64 bytes, used in turn */
	uint8_t replyIndex;
	uint8_t codec;
	uint8_t option;
	uint8_t reserved;
	union {
		struct {
			uint16_t maxStreams;
			uint16_t maxFrames;
		};
		uint32_t work2;		/* an encoder's */
	};
	uint32_t instance;		/* the client's */
} VideoServiceSlot;

/* a decoder's slot (an encoder's is the 64-byte slot alone) */
typedef struct VideoServiceDecoder {
	VideoServiceSlot slot;
	uint32_t option5[2];
	uint32_t option4[2];
	uint32_t reserved[12];
} VideoServiceDecoder;

/* the service (192 bytes), followed by the slots and the helper's memory */
typedef struct VideoService {
	uint32_t reserved;
	int32_t state;			/* 2 running, 0 to stop */
	VkThread *helper;
	uint32_t helperMemory;
	VideoServiceDecoder *decoders;
	VideoServiceSlot *encoders;
	int8_t numOfDecoders;
	int8_t numOfEncoders;
	int8_t decodersInUse;
	int8_t encodersInUse;
	uint16_t commandSize;
	uint16_t horizontal;
	int8_t queueSize;		/* a power of two, twice the codecs */
	int8_t current;			/* the slot of the request taken */
	int16_t lastId;
	VkMessageQueue *helperQueue;
	VkMessageQueue *helperQueue2;
	VkMessageQueue *helperQueue3;
	VkMessageQueue *engineQueue;	/* the engines' replies */
	VkMessageQueue *engineQueue2;
	VkVThreadResource *resource;
	VkVThreadContext *encoderContext;
	VkVThreadContext *context[2];
	volatile uint8_t *contextLock[2];
	volatile uint8_t *encoderLock;
	VkThread *client;
	int32_t status;
	uint32_t replyToggle;		/* a bit per slot: its reply in use */
	uint32_t replyReady;
	VkMessageQueue *requestQueue;
	VkMessageQueue *releaseQueue;
	uint32_t flagAccepted;
	uint32_t flagReplied;
	uint32_t (*callback[3])(void);
	uint32_t version;
	uint32_t encoderOption;
	uint32_t reserved2[14];
} VideoService;

/* what the engine is asked, and answers, about an access unit or a decode */
typedef struct VideoEngineRequest {
	int32_t id;			/* asked: the codec's */
	uint32_t buffer;
	uint32_t bufferSize;
	uint32_t esOffset;		/* answered: the frame released, with esSize */
	uint32_t esSize;
	uint32_t pictures;		/* asked: the context; answered: the pictures */
	int8_t index;			/* the slot */
	int8_t numOfPictures;		/* asked: the decoders in use */
	uint8_t auMode;
	uint8_t codec;
	uint8_t flags;			/* VIDEO_ENGINE_* */
	uint8_t contextFlags;		/* VIDEO_ENGINE_CONTEXT_* */
	uint8_t numOfRefFrames;
	uint8_t option;
	uint16_t horizontal;
	uint16_t vertical;
	int32_t status;
	VkThread *thread;		/* asked: the parser engine's thread, its stream, */
	uint32_t stream;		/* its job and its queue to the decoder engine */
	uint32_t job;
	VkMessageQueue *queue;
	uint32_t user;
	uint32_t sequenceInfo;
	uint32_t dts[2];
	uint32_t pts[2];
	uint32_t reserved3[12];
} VideoEngineRequest;

#define VIDEO_ENGINE_SEQUENCE		0x01
#define VIDEO_ENGINE_CONTEXT		0x02
#define VIDEO_ENGINE_DRAIN		0x04
#define VIDEO_ENGINE_RESTART		0x08

#define VIDEO_ENGINE_CONTEXT_FIELD	0x01
#define VIDEO_ENGINE_CONTEXT_OPTION	0x02
#define VIDEO_ENGINE_CONTEXT_MACROBLOCK	0x04

/* the codecs' requests as the service reads them (128 bytes) */
typedef struct VideoServiceRequest {
	uint32_t instance;
	uint32_t reserved;
	uint32_t id;
	uint32_t code;
	uint8_t reserved2[2];
	uint8_t codec;
	uint8_t reserved3;
	uint32_t dts[2];
	uint32_t pts[2];
	uint32_t option5[2];
	uint32_t option4[2];
	uint32_t user;
	union {
		struct {
			uint32_t buffer;
			uint32_t bufferSize;
			uint32_t esOffset;
			uint32_t esSize;
		};
		struct {
			uint32_t reserved4;
			VkThread *thread;
			VkMessageQueue *queue;
			uint32_t frames;
		};
	};
	uint16_t maxStreams;
	uint16_t maxFrames;
	union {
		struct {
			uint8_t auMode;
			uint8_t sequence;
			uint8_t context;
			uint8_t drain;
			uint8_t fieldDecode;
			uint8_t contextOption;
			uint8_t restart;
			uint8_t numOfRefFrames;
			uint8_t option;
			uint8_t macroblockData;
			uint16_t horizontal;
			uint16_t vertical;
			uint16_t reserved5;
			uint32_t contextAddress;
		};
		struct {
			uint32_t work2;		/* an encoder's */
			uint32_t encoderOption;
		};
	};
	uint32_t reserved6[8];
} VideoServiceRequest;

/* the reply (64 bytes) the codecs read (avcdec.c's AvcdecReply) */
typedef struct VideoServiceReply {
	uint32_t instance;
	uint32_t id;
	uint32_t index;
	uint32_t code;
	uint32_t kind;
	uint32_t user;
	uint32_t esOffset;
	uint32_t frame;
	int32_t status;
	uint32_t pictures;
	uint8_t numOfPictures;
	uint8_t reset;
	uint8_t sequence;
	uint8_t fieldDecode;
	uint8_t option;
	uint8_t numOfRefFrames;
	uint8_t macroblockData;
	uint8_t reserved;
	uint16_t horizontal;
	uint16_t vertical;
	uint32_t sequenceInfo;
	uint32_t reserved2[2];
} VideoServiceReply;

/* the bitstream unit's registers, through the `dsp` instruction */
#define VIDEO_BITS_PEEK			0x1000	/* the next n bits */
#define VIDEO_BITS_SKIP			0x2000	/* n bits consumed: the bits it wants pushed */
#define VIDEO_BITS_PUSH			0x3000	/* 8 bits pushed: the bits it still wants */
#define VIDEO_BITS_READ			0x8010	/* + register: read */
#define VIDEO_BITS_WRITE		0x9010	/* + register: write */
#define VIDEO_BITS_CONTROL		0
#define VIDEO_BITS_WORD			1
#define VIDEO_BITS_STATE2		2
#define VIDEO_BITS_STATE3		3
#define VIDEO_BITS_POSITION		4
#define VIDEO_BITS_STATUS		5
#define VIDEO_BITS_STATE6		6
#define VIDEO_BITS_GET(reg)		(VIDEO_BITS_READ + (reg))
#define VIDEO_BITS_SET(reg)		(VIDEO_BITS_WRITE + (reg))
#define VIDEO_BITS_END			5	/* VIDEO_BITS_STATUS: nothing more to push */

#define VIDEO_DSP(code, a, b) \
	__asm__ __volatile__("dsp %0,%1,%2" : "+r"(a) : "r"(b), "i"(code))
#define VIDEO_DSP_GET(code, b) ({ uint32_t value_; \
	__asm__ __volatile__("dsp %0,%1,%2" : "=r"(value_) : "r"(b), "i"(code)); value_; })
#define VIDEO_DSP_READ(code) ({ uint32_t value_; \
	__asm__ __volatile__("dsp %0,$0,%1" : "=r"(value_) : "i"(code)); value_; })
#define VIDEO_DSP_WRITE(code, value) \
	__asm__ __volatile__("dsp %0,$0,%1" : : "r"(value), "i"(code))

/* a stream read through the bitstream unit: a ring, or one range when ringStart is 0 */
typedef struct VideoBitstream {
	uint32_t ringStart;
	uint32_t ringEnd;		/* its last byte */
	uint32_t read;
	int32_t remaining;
} VideoBitstream;

/* the bitstream unit's descriptors: a list of ranges it reads */
typedef struct VideoUnitDescriptor {
	uint32_t next;			/* the next descriptor, 0 the last */
	uint32_t flags;			/* VIDEO_UNIT_DESCRIPTOR_LAST */
	uint32_t address;
	uint32_t size;
} VideoUnitDescriptor;

#define VIDEO_UNIT_DESCRIPTOR_LAST	0x2

/* the bitstream unit's registers, all of them, as the M4V decoder saves them */
typedef struct VideoBitsRegisters {
	uint32_t control;
	uint32_t word;
	uint32_t state2;
	uint32_t state3;
	uint32_t position;
	uint32_t status;
	uint32_t state6;
} VideoBitsRegisters;

/* the bitstream unit's registers saved with a slice */
typedef struct VideoBitsState {
	uint8_t ended;			/* the picture's slices ended early */
	uint8_t control;
	uint8_t state3;
	uint8_t status;
	uint32_t bitOffset;		/* into the unit's first byte */
	uint32_t word;
	uint32_t state2;
	uint32_t position;
	uint32_t state6;
} VideoBitsState;

/*
 * The parameter sets the stream keeps, by id: two lists, most recently used
 * first, over one array of nodes (the first list's, then the second's). A
 * list of no capacity is not kept: the id is its own index.
 */
typedef struct VideoCacheList {
	uint8_t capacity;
	uint8_t count;
	uint8_t head;
} VideoCacheList;

typedef struct VideoCacheNode {
	uint8_t id;
	int8_t next;
} VideoCacheNode;

typedef struct VideoParameterCache {
	VideoCacheList lists[2];
	VideoCacheNode nodes[];
} VideoParameterCache;

#define VIDEO_CACHE_EVICTED		256	/* added to the index of a node reused */

int32_t videoCacheFind(VideoParameterCache *cache, uint32_t id);		/* 0x862cec */
int32_t videoCacheAdd(VideoParameterCache *cache, uint32_t id);			/* 0x862d40 */
void videoCacheDrop(VideoParameterCache *cache, uint32_t index);		/* 0x862dca */
int32_t videoCacheFind2(VideoParameterCache *cache, uint32_t id);		/* 0x862dfc */
int32_t videoCacheAdd2(VideoParameterCache *cache, uint32_t id);		/* 0x862e60 */
void videoCacheDrop2(VideoParameterCache *cache, uint32_t index);		/* 0x862ef2 */

/* the parser's errors: below 16384 noted, otherwise fatal (back to the job's jump,
 * after closing what the stage has open) */
#define VIDEO_ERROR_FATAL		16384
#define ERROR_SLICES_ENDED		8201	/* the picture's slices ended */
#define VIDEO_STAGE_NONE		0
#define VIDEO_STAGE_ADD_SET		1	/* the parameter set being added */
#define VIDEO_STAGE_BUFFER		2
#define VIDEO_STAGE_ADD_SET2		3	/* the set read, the second being added */
#define VIDEO_STAGE_SET2		4
#define VIDEO_STAGE_SETS		5
#define VIDEO_STAGE_SETS2		6
#define VIDEO_STAGE_SLICE		7

/* the NAL unit types the helper, the parser and the encoder look at */
#define NAL_SLICE			1
#define NAL_IDR				5
#define NAL_SEI				6
#define NAL_SPS				7
#define NAL_PPS				8
#define NAL_AUD				9
#define NAL_FILLER			12

/* a 33-bit time stamp, doubled; -1 none (word aligned in the MPEG-4 structures) */
typedef int64_t VideoTime __attribute__((aligned(4)));

#define VIDEO_TIME_NONE		(-1LL)
#define VIDEO_TIME_WRAP		0x3ffffffffLL
#define VIDEO_FIELDS_FRAME	2

/*
 * The access unit's stream as the parser engine and its jobs see it (128
 * bytes): the unit's bytes mapped from the client's ring (in two pieces when
 * it wraps), and the engine's tables.
 */
typedef struct VideoStream {
	uint32_t id;			/* the decoder's */
	uint32_t base;			/* the unit's first byte, mapped */
	uint32_t esSize;
	uint32_t ringStart;		/* the ring's start, mapped when the unit wraps; 0 */
	uint32_t ringEnd;		/* the last byte mapped */
	uint32_t mapped;		/* the piece mapped from the unit's line */
	uint32_t bufferSize;		/* the ring's */
	uint32_t esOffset;		/* the unit's place in the ring */
	int8_t auMode;			/* the request's, | 8 when it restarts */
	uint8_t sequence;		/* only the sequence is asked */
	uint16_t cacheSize;		/* the parameter cache's */
	uint8_t option;			/* the request's */
	uint8_t reserved[3];
	uint32_t parameterCache;	/* a VideoParameterCache */
	uint32_t parameterSets;		/* 1088 bytes each */
	uint32_t parameterSets2;	/* 16 bytes each */
	uint32_t pictureData;		/* 256 bytes a picture */
	uint32_t nal;			/* the unit being parsed (a VideoNal, 1216 bytes) */
	uint32_t pictures;		/* VideoAvcPicture, nal->pictures of them */
	uint32_t pictureExtras;		/* 16 bytes each */
	/*
	 * The access unit's times, -1 none. They come in as the request's
	 * "dts" and "pts" words, in that order, but the decoder takes the first
	 * as the PTS: it warns when it is before the second, and it carries the
	 * second on by a frame when it is missing, as the DTS.
	 */
	VideoTime pts;
	VideoTime dts;
	uint32_t reserved2[11];
} VideoStream;

/* a picture of the stream's (128 bytes) */
typedef struct VideoAvcPicture {
	uint32_t address;		/* its luma, its chroma and field data after */
	int32_t picOrderCnt;
	uint32_t frameNum;
	int32_t frameNumWrap;
	int32_t picNum;
	uint32_t pocPeriod;		/* the IDR pictures and operations 5 before it */
	int8_t longTermPicNum;
	int8_t longTermFrameIdx;
	uint8_t outputState;		/* AVC_OUTPUT_* */
	uint8_t referenceInfo;		/* 1 short-term, 2 long-term */
	int8_t picStruct;		/* the picture timing SEI's */
	uint8_t reserved3;
	uint8_t idr;
	uint8_t damaged;		/* a reference it uses was incomplete */
	uint8_t incomplete;		/* its slices ended before its last macroblock */
	uint8_t free;			/* the picture may be taken */
	uint8_t missingReference;	/* a reference it uses was missing */
	uint8_t frameCropping;
	uint32_t sps;			/* its sequence's parameter set (1088 bytes) */
	uint32_t data;			/* its 256 bytes of the stream's picture data */
	int8_t topFieldOffset;		/* TopFieldOrderCnt less PicOrderCnt */
	int8_t bottomFieldOffset;
	uint16_t width;			/* the luma's width and pitch, in bytes */
	uint16_t pitch;
	uint16_t pixelWidth;		/* its macroblocks', in pixels */
	uint16_t pixelHeight;
	uint8_t reserved4[2];
	VideoTime pts;			/* the access unit's, doubled */
	VideoTime dts;
	VideoTime time;			/* the DTS, or the last picture's plus a frame */
	int32_t fieldCount;		/* fields before it, 0 at the stream's first */
	uint32_t cpbRemovalDelay;	/* its picture timing SEI's, when the stream asks */
	uint32_t dpbOutputDelay;
	uint8_t reserved6[36];
} VideoAvcPicture;

/* a unit of the stream (a NAL unit) found in the ring, read through the unit */
/* pred_weight_table(): one list's weights and offsets */
/* a picture's output state */
#define AVC_OUTPUT_DECODING	1
#define AVC_OUTPUT_WAITING	2		/* decoded, in output order still to come */
#define AVC_OUTPUT_GIVEN	4		/* its record given out */
#define AVC_OUTPUT_RELEASED	8		/* to be freed once it is no reference */

/*
 * A picture given out (128 bytes): a record of the decoder's data (16 of
 * them, a ring), the next one's address in it.
 */
typedef struct VideoAvcFrame {
	uint32_t luma;
	uint32_t chroma;
	uint16_t width;			/* its macroblocks', in pixels */
	uint16_t height;
	uint16_t chromaWidth;
	uint16_t chromaHeight;
	uint16_t bufferWidth;		/* the frames' luma width and pitch */
	uint16_t pitch;
	int32_t picOrderCnt;
	uint8_t frameCropping;
	uint8_t reserved;
	int8_t picStruct;
	uint8_t errors;			/* 1 damaged, 2 incomplete */
	uint8_t idr;
	uint8_t reserved2[3];
	int16_t cropTop;		/* the sequence's, when it crops */
	int16_t cropBottom;
	int16_t cropLeft;
	int16_t cropRight;
	VideoTime dts;			/* the picture's time */
	VideoTime pts;
	int32_t fieldCount;
	uint32_t fields;
	uint32_t next;			/* the next record */
	uint32_t number;		/* the pictures given out before it */
	uint32_t vui;
	uint32_t sei;			/* the picture's SEI data */
	uint8_t cropUnitX;		/* 2 and 2, when it crops */
	uint8_t cropUnitY;
	int8_t topFieldOffset;
	int8_t bottomFieldOffset;
	uint32_t frameRateCode;
	uint8_t reserved3[40];
} VideoAvcFrame;

/* a picture's 16 bytes beside it: its frame as a reference */
typedef struct VideoAvcPictureExtra {
	uint32_t frameNum;
	int32_t frameNumWrap;
	int8_t reference;		/* 1 a short-term reference */
	uint8_t reserved[7];
} VideoAvcPictureExtra;

typedef struct VideoAvcWeights {
	uint16_t lumaWeight[16];
	int8_t lumaOffset[16];
	uint16_t chromaWeight[2][16];
	int8_t chromaOffset[2][16];
} VideoAvcWeights;

/* the slice header's word for the unit (nal->sliceHeader) */
#define AVC_SLICE_TYPE			0x00000007	/* slice_type % 5 */
#define AVC_SLICE_TYPE_ALL		0x00000008	/* slice_type >= 5 */
#define AVC_SLICE_DIRECT_SPATIAL	0x00000020
#define AVC_SLICE_DEBLOCKING_SHIFT	6		/* disable_deblocking_filter_idc */
#define AVC_SLICE_REFERENCE		0x00000800	/* nal_ref_idc != 0 */
#define AVC_SLICE_BIPRED_SHIFT		16		/* weighted_bipred_idc */
#define AVC_SLICE_WEIGHTED		0x00040000	/* weighted_pred_flag */
#define AVC_SLICE_CONSTRAINED_INTRA	0x00080000
#define AVC_SLICE_TRANSFORM_8X8		0x00100000
#define AVC_SLICE_CABAC			0x00200000	/* not run on the unit */
#define AVC_SLICE_FRAME_MBS_ONLY	0x01000000
#define AVC_SLICE_DIRECT_8X8		0x08000000

/* slice_type % 5 */
#define AVC_SLICE_P			0
#define AVC_SLICE_B			1
#define AVC_SLICE_I			2

/*
 * The CABAC decoder of a slice (9.3): the engine's range and offset, read
 * from the bitstream unit, and the 400 context variables (pStateIdx << 1 |
 * valMPS) of the slice's cabac_init_idc and QP.
 */
#define AVC_CABAC_CONTEXTS		400
#define AVC_CABAC_END_OF_SLICE		276	/* ctxIdx of end_of_slice_flag */

typedef struct VideoAvcCabac {
	VideoBitstream *bits;
	uint32_t range;			/* codIRange */
	uint32_t offset;		/* codIOffset */
	uint8_t contexts[AVC_CABAC_CONTEXTS];
} VideoAvcCabac;

/*
 * A macroblock as the CABAC decoder's neighbours see it (32 bytes): the
 * entries of a row buffer, one per column (the entry above a macroblock
 * keeps the row above's until the macroblock replaces it), after a left
 * one; the row buffer's first entry holds only the left macroblock's
 * absMvd column.
 */
typedef struct VideoAvcCabacMb {
	uint16_t absMvd[2][4];		/* by list: Min(|mvd x|, 33) | Min(|mvd y|, 33) << 8,
					   by 4x4 column (the row below's) or row (the left's) */
	uint8_t unavailable;		/* outside the slice, or intra */
	uint8_t coded;			/* not skipped */
	uint8_t mbTypeBin;		/* mb_type's first bin: not I_NxN, not B_Direct_16x16 */
	uint8_t chromaPredMode;		/* intra_chroma_pred_mode is not 0 */
	uint8_t refIdxNonZero;		/* a bit per 8x8 block, list 1 from bit 4 */
	uint8_t cbp;			/* the luma bits inverted: an 8x8 block not coded */
	uint8_t qpDeltaNonZero;
	uint8_t lumaDcCbf;
	uint16_t lumaCbf;		/* coded_block_flag of the 4x4 blocks */
	uint8_t chromaDcCbf[2];
	uint8_t chromaAcCbf[2];		/* a bit per 4x4 block */
	uint8_t reserved[2];		/* written with the cbf words, never read */
} VideoAvcCabacMb;

/* a macroblock of the run's first buffer, as the unit reads it (16 bytes) */
typedef struct VideoAvcMbInfo {
	union {
		struct {
			uint8_t skipped;
			uint8_t mbType;	/* of the slice type, I types after the P or B ones */
			uint8_t cbp;	/* coded_block_pattern */
			uint8_t reserved;	/* written 0 */
		};
		uint32_t kind;		/* the decoder engine's, once read: AVC_MB_* */
	};
	uint8_t subMbType[4];		/* the decoder engine's AVC_MB_* of each, once read */
	union {
		int8_t qpDelta;		/* mb_qp_delta */
		int8_t qp;		/* the decoder engine's, once read */
	};
	uint8_t reserved2;		/* written 0 */
	uint8_t qpPrevious;		/* the decoder engine's: the QP before it */
	uint8_t reserved3;
	uint32_t cbf;			/* coded_block_flag: luma 4x4 by bit, Cb and Cr AC
					   by 4 bits from bit 16, luma, Cb and Cr DC at 24 */
} VideoAvcMbInfo;

typedef struct VideoNal {
	VideoBitstream bits;
	uint32_t reserved[2];
	uint32_t search;		/* where the search goes on */
	int32_t searchLeft;
	uint32_t start;			/* its first byte */
	int32_t size;
	uint32_t sliceHeader;		/* AVC_SLICE_*: the slice's header for the unit */
	uint32_t pictureCount;
	uint32_t lastPictureCount;
	int32_t error;
	int16_t firstMacroblock;	/* of the run */
	int16_t macroblocks;		/* in the picture */
	uint8_t widthInMacroblocks;
	uint8_t heightInMacroblocks;
	int16_t rowsPerRun;		/* a run that starts a row takes at most these */
	uint8_t runColumn;		/* of the run's first macroblock */
	uint8_t runRow;
	int16_t runMacroblocks;		/* the unit's word 8: the run's length */
	int16_t doneMacroblocks;	/* the unit's words 11 and 10, when it is not run */
	int16_t doneStatus;
	uint16_t sliceDone;		/* the slice header clears it */
	uint8_t sliceParsed;		/* a slice header was parsed */
	uint8_t load;			/* the slice's data goes to the unit */
	uint32_t context;		/* the helper's context, command and slice for the run */
	uint32_t slice;
	uint32_t command;
	uint32_t outputFrame;		/* the record the next output picture gets */
	uint8_t type;
	uint8_t refIdc;
	uint8_t numRefFrames;		/* the sequence's num_ref_frames */
	uint8_t pictures;		/* entries of the stream's picture tables */
	uint8_t numReorderFrames;	/* the sequence's */
	int8_t picStruct;		/* the picture timing SEI's, 0 none */
	uint8_t gapsInFrameNumAllowed;
	uint8_t frameCropping;
	uint8_t spsId;			/* of the picture parameter set in use */
	uint8_t contextOption;		/* the sequence's: answered as VIDEO_ENGINE_CONTEXT_OPTION */
	uint8_t parameterSet;		/* the stream's 1088-byte table in use */
	uint8_t parameterSet2;		/* and its 16-byte one */
	uint16_t frameNum;
	uint8_t reserved4[2];
	uint32_t prevFrameNum;
	int32_t frameNumOffset;		/* FrameNumOffset, kept as prevFrameNumOffset */
	uint32_t maxFrameNum;
	uint32_t pictureFrameNum;		/* the picture's, for the reference lists */
	uint32_t pictureMaxFrameNum;
	uint32_t pocPeriod;		/* IDR pictures and operations 5 seen */
	int32_t prevPicOrderCntMsb;
	int32_t prevPicOrderCntLsb;
	int32_t picOrderCnt;
	int32_t topFieldOrderCnt;
	int32_t bottomFieldOrderCnt;
	int32_t deltaPicOrderCnt[2];
	uint16_t picOrderCntLsb;
	uint8_t mmco5;			/* memory_management_control_operation 5 was seen */
	uint8_t cabacInitIdc;
	int8_t numRefIdxActive[2];
	uint8_t adaptiveRefPicMarking;
	uint8_t longTermReference;
	uint8_t refPicListReordering[2];
	int8_t sliceQpDelta;
	int8_t sliceAlphaC0Offset;
	int8_t sliceBetaOffset;
	uint8_t lumaLog2WeightDenom;
	uint8_t chromaLog2WeightDenom;
	uint8_t idr;			/* an IDR picture, or a recovery point */
	uint16_t width;			/* the pictures': luma width, pitch, sizes */
	uint16_t pitch;
	uint32_t frameSize;
	uint32_t lumaSize;
	uint32_t chromaSize;
	int8_t picture;			/* the picture decoded */
	uint8_t seiFound;
	uint8_t idrPending;
	uint8_t fieldDecode;		/* the sequence's: answered as VIDEO_ENGINE_CONTEXT_FIELD */
	uint8_t numOfRefFrames;		/* the sequence's */
	uint8_t reserved10;
	uint16_t horizontal;
	uint16_t vertical;
	uint8_t reserved5[2];
	uint32_t decoderData;		/* the engine info's */
	uint32_t frameCount;
	uint8_t outputSlot;		/* of outputCounts, toggled each picture */
	uint8_t outputStart;		/* the picture's records in outputRing: given to */
	int8_t outputCount;		/* the engine with the job */
	uint8_t outputEnd;		/* where the next record goes */
	uint8_t outputDone;		/* the first record not yet released */
	uint8_t reserved16;
	uint8_t scanned;		/* the slice types were scanned */
	uint8_t outputPending;
	uint8_t outputCounts[2];	/* the records of the last two pictures */
	uint8_t outputRing[16];		/* the pictures given out, by record */
	uint8_t restart;		/* the access unit restarts */
	int8_t restartPicture;		/* the picture the restarted unit decodes */
	uint8_t referenceInfo;
	uint8_t reserved12[3];
	int32_t outputTime;
	uint8_t failed;
	uint8_t skip;			/* the unit is not run */
	uint8_t startPending;		/* the unit's first slice is still to be read */
	int8_t nextType;		/* of the unit after the slice, -1 none */
	VideoBitsState state;
	uint16_t mbRowSize;		/* the run buffers' row sizes: mb info, 16 bytes a macroblock */
	uint16_t intraRowSize;		/* intra prediction modes, 16 */
	uint16_t chromaModeRowSize;	/* chroma prediction modes, 1 */
	uint16_t refIdxRowSize;		/* reference indices, 8 */
	uint16_t mvdRowSize;		/* motion vector differences, 128 */
	uint16_t coefRowSize;		/* luma coefficients, 544, aligned */
	uint16_t chromaCoefRowSize;	/* Cb and Cr coefficients, 136, aligned */
	uint16_t pcmRowSize;		/* PCM luma samples, 256 */
	uint16_t pcmChromaRowSize;	/* PCM chroma samples, 64 */
	uint16_t nextFirstMacroblock;	/* of the slice after this one */
	VideoTime firstPts;		/* the decoder's, doubled: the first picture's */
	VideoTime firstDts;
	VideoTime lastPts;
	VideoTime lastTime;		/* the last picture's decode time */
	uint32_t frameFields;		/* the last record's fields */
	int32_t fieldCount;		/* the last picture's */
	int32_t frameDuration;		/* of a frame, in 180 kHz ticks; -1 unknown */
	uint8_t timeWrap;		/* toggled when the decode time wraps */
	uint8_t reserved18[3];
	VideoAvcCabac cabac;		/* the slice's, when it is CABAC */
	uint8_t sliceQp;			/* pic_init_qp plus slice_qp_delta */
	uint8_t reserved20[27];
	VideoAvcWeights weights[2];	/* pred_weight_table(), by list */
	uint8_t mmcoOperation[16];	/* dec_ref_pic_marking(), up to 0 */
	uint16_t mmcoArgument[16];	/* each operation's arguments, in order */
	uint8_t reorderingIdc[2][16];	/* ref_pic_list_reordering(), up to 3 */
	uint16_t reorderingValue[2][16];
} VideoNal;

/* a parser's view: its bit reader and the unit it reads */
typedef struct VideoParser {
	VideoBitstream *bits;
	VideoNal *nal;
} VideoParser;


/* the MPEG-4 decoder's stream as the helper's jobs get it (64 bytes) */
typedef struct VideoM4vStream {
	uint32_t reserved[3];
	uint32_t ringStart;
	uint32_t ringEnd;		/* its last byte */
	uint32_t reserved2[4];
	uint32_t unit;			/* a VideoM4vUnit, 256 bytes */
	uint32_t tables;		/* VideoM4vTable, unit->tables of them */
	VideoTime pts;			/* the access unit's */
	VideoTime dts;
	uint32_t reserved3;
} VideoM4vStream;

/* a VOP's header (64 bytes) */
typedef struct VideoM4vVop {
	uint8_t reserved[13];
	int8_t notCoded;		/* vop_coded 0: no run on the unit */
	uint8_t reserved2[2];
	uint32_t data;			/* given out with the picture */
	int8_t shortHeader;		/* short_video_header: H.263 pictures and GOBs */
	int8_t pictureHeader;		/* the picture header is still to be read */
	uint8_t reserved3[23];
	uint8_t info[3];		/* given out with the picture, with info2 */
	int16_t info3;
	int8_t frameRate;
	uint8_t info2;
	uint32_t info4;
	uint8_t reserved4[8];
} VideoM4vVop;

/* a video packet's header (256 bytes) */
typedef struct VideoM4vPacket {
	int8_t predicted;		/* a P-VOP: the resync marker is 16 + fcode bits */
	int8_t vopHeader;		/* the VOP header is still to be read */
	uint8_t reserved[14];
	int32_t fcode;			/* vop_fcode_forward */
	int32_t mode;
	uint8_t reserved2[40];
	int32_t runEnd;			/* short header: the macroblock the GOB ends at */
	int32_t unitRuns;		/* the unit decodes the macroblocks */
	int32_t skipped;
	uint8_t reserved3[180];
} VideoM4vPacket;

/* the decoder's tables of a picture (64 bytes each) */
typedef struct VideoM4vTable {
	uint32_t address;		/* the picture's luma, its chroma after */
	uint8_t state;			/* 1 being decoded, 4 decoded */
	uint8_t info;
	uint8_t reserved;
	int8_t info2;
	int8_t failed;
	int8_t concealed;
	uint8_t reserved2[2];
	uint16_t width;
	uint16_t height;
	uint16_t codedWidth;		/* in macroblocks, times 16 */
	uint16_t codedHeight;
	VideoTime pts;
	VideoTime dts;
	VideoTime time;			/* the dts, or one made from the last */
	uint32_t fieldCount;		/* in fields: 2 a frame */
	uint8_t reserved3[16];
} VideoM4vTable;

/* a decoded picture's record (128 bytes) */
typedef struct VideoM4vFrame {
	uint32_t luma;
	uint32_t chroma;
	uint16_t width;
	uint16_t height;
	uint16_t chromaWidth;
	uint16_t chromaHeight;
	uint16_t displayWidth;
	uint16_t displayHeight;
	uint32_t number;
	uint8_t reserved;
	uint8_t flags;
	uint8_t info;
	uint8_t errors;			/* 1 failed, 2 concealed */
	int8_t info2;
	uint8_t reserved2[11];
	VideoTime dts;			/* the table's time */
	VideoTime pts;
	uint32_t fieldCount;
	uint32_t fields;
	uint32_t reserved3;
	uint32_t number2;
	uint32_t vopInfo4;
	uint8_t vopInfo[4];
	uint32_t data;
	int8_t frameRate;
	uint8_t reserved4;
	int16_t vopInfo3;
	uint8_t reserved5[40];
} VideoM4vFrame;

/* the unit's command, as the engine reads it (64 bytes) */
typedef struct VideoM4vCommand {
	uint8_t reserved[11];
	int8_t table;
	uint32_t address;
	uint16_t width;
	uint16_t height;
	uint32_t pictureSize;
	uint32_t chromaOffset;
	uint32_t pictureSize2;
	uint32_t frames;
	uint8_t reserved2[28];
} VideoM4vCommand;

/* the stream unit being parsed (256 bytes) */
typedef struct VideoM4vUnit {
	VideoBitstream bits;
	uint32_t reserved[4];
	uint32_t start;			/* its first byte */
	int32_t size;
	uint32_t pictureCount;
	int32_t error;
	int16_t firstMacroblock;	/* of the run */
	int16_t macroblocks;		/* in the picture */
	int16_t widthInMacroblocks;
	int16_t heightInMacroblocks;
	int16_t runLength;		/* a run's macroblocks */
	int16_t runOffset;		/* the run starts this far into a row */
	int16_t reserved3;
	int16_t runMacroblocks;		/* the unit's word 8: the run's length */
	int16_t doneMacroblocks;	/* the unit's words 11 and 10, when it is not run */
	int16_t doneStatus;
	uint8_t reserved4[3];
	uint8_t load;			/* the packet's data goes to the unit */
	uint32_t context;		/* the helper's context and command for the run */
	uint32_t command;
	VideoM4vFrame *frame;		/* the picture's record */
	uint32_t reserved5;
	uint16_t width;
	uint16_t height;
	uint32_t pictureSize;
	uint32_t chromaOffset;		/* from the luma */
	uint32_t pictureSize2;
	uint8_t tables;
	int8_t table;			/* the picture's */
	int8_t info2;
	uint8_t reserved6[9];
	uint32_t frames;		/* VideoM4vFrame, one a table */
	uint32_t frameCount;
	uint8_t pictureInfo[2];		/* given to the engine with the job, swapped */
	uint8_t reserved7[6];
	uint32_t flags;
	VideoM4vVop *vop;
	VideoM4vPacket *packet;
	uint8_t reserved8;
	int8_t lastTable;
	int8_t failed;
	uint8_t reserved9;
	int32_t dataBits;		/* the bits before the unit's data, past the first byte */
	VideoBitsRegisters registers;
	VideoTime firstDts;
	VideoTime firstPts;
	VideoTime lastPts;		/* the frames' */
	VideoTime lastTime;		/* the tables' */
	uint32_t frameFields;
	uint32_t tableFields;
	int32_t timeIncrement;		/* a frame's, doubled; -1 none */
	int8_t wrapped;
	uint8_t reserved10[31];
} VideoM4vUnit;

/* a VThread job's state on a unit (1408 bytes) */
typedef struct VideoM4vJob {
	VideoBitstream *bits;
	VideoM4vUnit *unit;
	uint32_t reserved;
	VideoM4vTable *tables;
	VideoM4vTable *table;
	uint32_t jump[8];		/* videoSetJump's, for the parser's errors */
	uint8_t reserved2[1356];
} VideoM4vJob;

void videoParserError(const VideoStream *stream, VideoParser *parser, uint32_t stage,
	int32_t code);								/* 0x862c0c */

/* bitstream.c */
uint32_t videoBitsBegin(const VideoStream *stream, VideoParser *parser);
void videoBitsSave(VideoBitsState *state);
void videoBitsRestore(const VideoBitsState *state);
int32_t videoBitsSeek(const VideoStream *stream, VideoNal *nal);
uint32_t videoBitsMoreData(const VideoBitstream *bits);
uint32_t videoBitsHasBytes(const VideoBitstream *bits);
uint32_t videoBitsAligned(const VideoBitstream *bits);
void videoBitsRefill(VideoBitstream *bits, uint32_t wanted);
uint32_t videoBitsReadByte(VideoBitstream *bits);
uint32_t videoBitsRead(VideoBitstream *bits, uint32_t count);
int32_t videoBitsReadSigned(VideoBitstream *bits, uint32_t count);
uint32_t videoBitsPeek(VideoBitstream *bits, uint32_t count);
uint32_t videoBitsUe(VideoBitstream *bits);
uint32_t videoBitsTe(VideoBitstream *bits, int32_t max);
int32_t videoBitsSe(VideoBitstream *bits);
uint32_t videoNalFind(const VideoStream *stream, VideoParser *parser, uint32_t whole);
uint32_t videoNalSliceTypes(const VideoStream *stream, VideoParser *parser);
int32_t videoNalNext(const VideoStream *stream, VideoParser *parser, uint32_t whole);
uint32_t videoNalScan(const VideoStream *stream, VideoParser *parser);

struct VideoHelperShared;

/* m4vbits.c */
uint32_t m4vdecBitsBegin(const VideoM4vStream *stream, VideoM4vJob *job);
void m4vdecBitsSave(VideoBitsRegisters *registers);
void m4vdecBitsRestore(const VideoBitsRegisters *registers);
int32_t m4vdecBitsTell(const VideoM4vStream *stream, VideoM4vUnit *unit);
void m4vdecUnitLoad(const struct VideoHelperShared *shared, const VideoM4vStream *stream,
	const VideoM4vUnit *unit);
void m4vdecBitsSkipUnit(const struct VideoHelperShared *shared, VideoBitstream *bits);
void m4vdecBitsRefill(VideoBitstream *bits, uint32_t wanted);
uint32_t m4vdecBitsRead(VideoBitstream *bits, uint32_t count);
uint32_t m4vdecBitsPeek(VideoBitstream *bits, uint32_t count);
uint32_t m4vdecBitsPeekAligned(VideoBitstream *bits, uint32_t count);
uint32_t m4vdecBitsOffset(const VideoBitstream *bits);
uint32_t m4vdecBitsValidStuffing(VideoBitstream *bits);
uint32_t m4vdecBitsByteAlign(VideoBitstream *bits);
uint32_t m4vdecBitsNextStartCode(const VideoM4vStream *stream, VideoM4vJob *job);

/* the helper's thread flags (helper.c), which the codecs' helpers set too */
#define HELPER_FLAG_JOB			0x00000002	/* the job queue */
#define HELPER_FLAG_ENCODER		0x00000008	/* the encoder's queue */
#define HELPER_FLAG_IDLE		0x00000100
#define HELPER_FLAG_JOBS_DONE		0x00004000	/* the VThread jobs */
#define HELPER_FLAG_EXIT		0x00008000
#define HELPER_FLAG_START		0x00010000	/* the start queue */
#define HELPER_FLAG_FINISH		0x00020000
#define HELPER_FLAG_UNIT		0x00040000	/* the unit's interrupt */
#define HELPER_FLAG_RESTART		0x00080000
#define HELPER_FLAG_STOP		0x00100000
#define HELPER_FLAG_RESTARTED		0x00200000
#define HELPER_FLAG_FINISHED		0x00400000
#define HELPER_FLAG_ENCODER_FINISH	0x00800000
#define HELPER_FLAG_ENCODER_STOP	0x01000000
#define HELPER_FLAGS			0x01ffc10a

/* the codecs' helpers (avchelper.c, m4vhelper.c, avcenchelper.c): the sizes of what
 * they share with the helper and of a job, their commands and slices */
#define SHARED_SIZE		128
#define JOB_SIZE		128
#define COMMANDS		2
#define COMMAND_SIZE		64
#define SLICES			4

/* the VThread jobs, by the slot's index on the context */
#define JOB_INDEX_PREPARE(slot)	((slot) * 8 + 1)
#define JOB_INDEX_SLICE(slot)	((slot) * 8 + 3)

/* a job the engines give the helper (a 128-byte message) */
typedef struct VideoHelperRequest {
	uint32_t stream;
	uint32_t reserved[5];
	int8_t slot;
	uint8_t reserved2[2];
	uint8_t codec;
	uint8_t reserved3;
	uint8_t flags;			/* VIDEO_HELPER_* */
	uint8_t reserved4[10];
	VkThread *engineThread;
	uint32_t argument;
	uint32_t argument2;
	VkMessageQueue *engineQueue;
	uint32_t reserved5[19];
} VideoHelperRequest;

#define VIDEO_HELPER_FIRST		0x01
#define VIDEO_HELPER_HOLD		0x02
#define VIDEO_HELPER_PUBLIC		0x08

/* what the helper and the engine thread it serves share (128 bytes) */
typedef struct VideoHelperShared {
	VkThread *thread;
	VkThread *engineThread;
	uint32_t stream;
	int16_t slot;
	uint8_t first;
	uint8_t reserved5;
	uint32_t reserved;
	uint32_t argument;
	uint32_t argument2;
	uint32_t contexts;		/* two job contexts of 64 bytes */
	uint32_t commands;		/* two of 64 bytes */
	uint32_t slices;		/* four of 896 bytes */
	uint32_t context;
	uint32_t command;
	uint32_t slice;
	uint32_t request;
	uint32_t encoderArgument;
	uint32_t interruptStatus;
	uint32_t (*callback[3])(void);
	uint32_t version;
	uint8_t publicStream;		/* the stream by public addresses, not uncached */
	uint8_t ready;
	uint8_t hold;
	uint8_t reserved3;
	uint32_t descriptors;
	uint32_t tables[5];
	uint32_t status;		/* the run's: 3 the slice ended */
	uint32_t finished;		/* the picture is done */
	uint32_t encoderDone;
	uint32_t reserved4[2];
} VideoHelperShared;

/* a job context of the helper (64 bytes): the VThread context its jobs run on, their memory */
typedef struct VideoHelperContext {
	VkVThreadContext *context;
	uint32_t table128;		/* the names say the sizes, until the jobs are read */
	uint32_t lockArea;		/* its first word: the context's lock */
	uint32_t table2048;
	uint32_t header64;
	uint32_t common;		/* shared by both contexts, 1536 bytes */
	uint32_t row0;			/* three rows of the picture's macroblocks */
	uint32_t table1024;
	uint32_t row1;
	uint32_t row2;
	uint32_t table2048b;
	uint32_t table1024b;
	uint32_t table4096;
	uint32_t table16384;
	uint32_t table8192;
	uint32_t reserved;
} VideoHelperContext;

/* a job buffer (128 bytes, four): the slice it carries and its memory by commandSize */
#define VIDEO_HELPER_JOB_BUFFERS	11
typedef struct VideoHelperJob {
	uint8_t codec;
	uint8_t ended;			/* the picture's slices ended */
	int16_t count;			/* macroblocks in the run */
	uint16_t commandSize;
	int16_t first;
	uint8_t firstX;
	uint8_t firstY;
	uint8_t endX;
	uint8_t endY;
	uint8_t load;
	uint8_t sliceUsed;		/* the picture is done: give its slice back */
	uint8_t commandUsed;		/* and its command */
	union {
		uint8_t failed;
		uint8_t fcode;		/* M4V: the packet's vop_fcode */
	};
	uint32_t slice;			/* its 896-byte slice; the slice's second word, its command */
	uint32_t buffers[VIDEO_HELPER_JOB_BUFFERS];
	uint8_t pictureInfo[2];
	uint8_t mode;			/* M4V: the packet's */
	uint8_t reserved;
	uint32_t command;		/* M4V: the command the job took */
	uint8_t nextX;
	uint8_t nextY;
	uint8_t reserved2[54];
} VideoHelperJob;

/* a VThread job's state on a slice (192 bytes): the parser and the tables */
typedef struct VideoSliceJob {
	VideoParser parser;
	uint32_t parameterSet;		/* the sequence's, 1088 bytes */
	uint32_t pps;			/* the picture's, in parameterSet2 */
	uint32_t parameterSet2;		/* the line of 16-byte sets holding it */
	uint32_t reserved5;
	uint32_t buffer;		/* 256 bytes */
	uint32_t reserved6;
	uint32_t slice;			/* 896 bytes */
	VideoAvcPicture *pictures;
	VideoAvcPicture *picture;
	uint32_t pictureExtras;
	int8_t numRefIdxActive[2];	/* the CABAC decoder's */
	int8_t refListSize[2];		/* the lists' pictures, as reordered */
	uint8_t fieldDecode;		/* the context's (the helper's first) */
	uint8_t contextOption;
	int8_t refListWork[18];		/* a list as it is reordered */
	uint8_t reserved3[16];
	uint32_t jump[8];		/* videoSetJump's, for the parser's errors */
	VideoAvcMbInfo *mbInfo;		/* CABAC: the run buffers at the row */
	int8_t *intraModes;		/* prev_intra4x4_pred_mode, -1, or rem_intra4x4_pred_mode */
	uint8_t *chromaModes;
	int8_t *refIdx;			/* by list, four 8x8 blocks */
	uint32_t *mvd;			/* x | y << 16, in the order read */
	int16_t *lumaCoef;
	int16_t *cbCoef;
	int16_t *crCoef;
	uint8_t *pcmLuma;
	uint8_t *pcmCb;
	uint8_t *pcmCr;
	uint32_t reserved4[2];
	VideoAvcCabacMb *rowBuffer;	/* CABAC: the neighbours */
	VideoAvcCabacMb *mb;		/* the macroblock's entry */
	VideoAvcCabac *cabac;
	uint32_t reserved7[2];
} VideoSliceJob;

/* the helper's memory (after the codecs' slots) */
typedef struct VideoHelper {
	int32_t state;			/* 2 idle, 3 waiting, 4 running, 5 working */
	uint32_t reserved;
	VideoHelperShared *shared;
	VkMessageQueue *startQueue;
	VkMessageQueue *jobQueue;
	VkMessageQueue *engineQueue;
	VkMessageQueue *encoderQueue;
	uint32_t pendingJobs[3];
	VkInterruptHandler *interrupt;
	uint32_t *interruptStatus;	/* the word the interrupt writes */
	VkVThreadContext *context;
	uint8_t commandsInUse;		/* a bit per command */
	uint8_t slicesInUse;		/* a bit per slice */
	uint8_t needCommand;
	uint8_t needSlice;
	int8_t pending;
	uint8_t codec;
	int8_t encoderJobs;
	uint8_t hold;
	uint32_t encoderArgument;
	uint32_t descriptors[8];	/* the unit's, two of four words */
	uint32_t interruptWord;
} VideoHelper;

/*
 * The AVC encoder's stream: the bytes written (by the bit writer, or by the
 * unit) into its buffer, and the stream's parameters the headers carry.
 */
#define VIDEO_AVC_UNIT_PARAMETERS	7

typedef struct VideoAvcWriter {
	uint32_t start;
	uint32_t end;			/* its last byte */
	uint32_t position;
	uint32_t job;			/* the job, whose jump an overflow takes (job + 164);
					 * the copies count it down, the unit's path keeps
					 * the bytes left in it */
	uint8_t reserved;
	uint8_t emulations;		/* emulation prevention bytes written */
	int16_t bitPosition;		/* in the byte at position */
	uint32_t history;		/* the last bytes, for emulation prevention */
	uint32_t publicBase;
	uint32_t length;
	uint32_t publicPosition;
	uint32_t offset;		/* of start in the stream */
	uint32_t buffer;
	uint32_t bufferSize;
	uint32_t sliceType;		/* 2 I, 0 P */
	uint32_t pictures;		/* encoded since the stream's start */
	uint32_t noUnit;		/* the unit does not run */
	uint32_t reserved6;
	uint16_t macroblocks;
	uint8_t widthInMacroblocks;
	uint8_t heightInMacroblocks;
	uint8_t rowsPerJob;
	int8_t jobs;			/* the jobs the helper takes per picture */
	uint8_t mode;			/* the command's */
	uint8_t newGroup;		/* the next job starts a group */
	int16_t firstMacroblock;	/* of the next job */
	uint8_t reserved5;
	uint8_t row;			/* of the next job */
	uint8_t group;
	uint8_t rowsLeft;		/* in the group */
	uint8_t reserved7[2];
	int16_t sliceState[2];		/* cleared after each slice's macroblocks */
	int16_t unitParameters[VIDEO_AVC_UNIT_PARAMETERS];
	uint16_t idrPicId;
	uint16_t frameNum;
	uint16_t previousFrameNum;	/* the last picture's */
	uint8_t log2MaxFrameNum;
	uint8_t initialCpbRemovalDelayLength;
	uint8_t cpbRemovalDelayLength;
	uint8_t dpbOutputDelayLength;
	uint8_t pendingBits;		/* the unit's partial byte */
	uint8_t picStructPresent;
	uint8_t reserved8[2];
	uint32_t picOrderCntStart;	/* the stream's first */
	uint32_t picOrderCnt;		/* the picture's: twice its number */
	uint32_t picOrderCntOutput;
	uint32_t gopByteLimit;		/* the bytes a second the pictures since the IDR may take */
	uint32_t gopBytes;		/* the bytes they took */
	uint16_t gopPictures;
	uint16_t frameRate;		/* pictures a second */
	uint32_t commandResult;		/* the picture's VideoAvcEncodeResult */
	uint8_t cabacState[52];		/* the job's, while the helper holds the unit */
} VideoAvcWriter;

/* what the client is told of a picture encoded (its command's result) */
typedef struct VideoAvcEncodeResult {
	uint32_t reserved[5];
	uint32_t picOrderCnt;		/* twice its frame number */
} VideoAvcEncodeResult;

/* a writer's noUnit, and the error its job jumps back with: the buffer is full, the stream ended */
#define WRITER_OVERFLOW		4096
#define WRITER_ENDED		0x8001

#define NALS_SIZE		192	/* a VideoAvcNalList */
/* the NAL units written for a picture */
#define VIDEO_AVC_NALS		12
typedef struct VideoAvcNalList {
	uint8_t count;
	uint8_t pictureType;		/* 4 an IDR picture, 1 not */
	uint8_t reserved;
	uint8_t pictureFlags;		/* VIDEO_AVC_PICTURE_* */
	uint32_t picOrderCnt;
	uint32_t picOrderCntOutput;
	uint8_t type[VIDEO_AVC_NALS];
	uint8_t refIdc[VIDEO_AVC_NALS];
	uint32_t start[VIDEO_AVC_NALS];
	uint32_t size[VIDEO_AVC_NALS];
} VideoAvcNalList;

/*
 * The sequence's parameters, as the client set them, after the encoder
 * engine's own (the engine's memory's second 64 bytes)
 */
typedef struct VideoAvcSequence {
	VkThread *thread;		/* the encoder engine's */
	VkThread *thread2;		/* its stream thread */
	int32_t id;			/* the codec's */
	int16_t index;			/* the slot's */
	uint16_t commandSize;		/* the helper's */
	uint16_t width;			/* in whole macroblocks */
	uint16_t height;
	uint8_t profileIdc;
	uint8_t levelIdc;
	uint8_t parameterSets;		/* 1 before the first picture, 2 before each IDR */
	uint8_t aspectRatioIdc;		/* 0: no aspect ratio information */
	uint16_t sarWidth;
	uint16_t sarHeight;
	uint8_t idrAfterFailure;	/* a picture that failed is followed by an IDR */
	uint8_t frameCropping;
	uint8_t cropRight;
	uint8_t cropBottom;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint32_t vclBitRate;
	uint32_t nalBitRate;
	uint32_t vclCpbSize;
	uint32_t nalCpbSize;
	uint8_t initialized;		/* the parameter sets written */
	uint8_t videoSignalPresent;
	uint8_t videoFormat;
	uint8_t videoFullRange;
	uint8_t colourDescriptionPresent;
	uint8_t colourPrimaries;
	uint8_t transferCharacteristics;
	uint8_t matrixCoefficients;
} VideoAvcSequence;

/* the picture being encoded */
typedef struct VideoAvcPictureInfo {
	uint32_t encoder;		/* the encoder's memory as the stream thread takes it, 64 bytes */
	uint32_t reference;		/* 64 bytes */
	uint32_t reserved2;
	int8_t qp;
	uint8_t reserved3[5];
	int8_t alphaOffset;
	int8_t betaOffset;
	uint32_t reserved4[23];
	uint32_t initialCpbRemovalDelay;
	uint32_t vclInitialCpbRemovalDelay;
	uint32_t initialCpbRemovalDelayOffset;
	uint32_t vclInitialCpbRemovalDelayOffset;
} VideoAvcPictureInfo;

/* a macroblock as the encoder decided it (16 bytes) */
typedef struct VideoAvcMacroblock {
	uint8_t skip;
	uint8_t type;			/* mb_type: I types in an I slice, P types (+5 intra) in P */
	uint8_t cbp;			/* coded_block_pattern */
	uint8_t reserved;
	uint8_t subTypes[4];		/* sub_mb_type */
	int8_t qpDelta;
	uint8_t reserved2[3];
	uint32_t coded;			/* bits: 0-15 luma blocks, 16-19 Cb AC, 20-23 Cr AC,
					 * 24 Intra16x16 DC, 25 Cb DC, 26 Cr DC */
} VideoAvcMacroblock;

/* the residual's tables (avcdeckernel.c, 0x91b1f8-0x91b346), the encoder's too */
extern const int16_t avcdecLevelScale[6][16];
extern const uint8_t avcdecQpShift[88];
extern const uint8_t avcdecQpRemainder[54];

/*
 * The luma prediction's kernels, the intra prediction's and residual's and
 * the deblocking's (avcdeckernel.c): the decoder's, whose copies the encoder
 * calls too (avcenckernel.c)
 */

/*
 * The luma prediction in the coprocessor, as its fetches and kernels hand
 * it on: the reference samples a fetch read (clamped to the picture), the
 * 8x8 block (c8-c15), a 4x4 block before it is placed (c16, c18) and list
 * 0's block read back for the weighting (c20-c23, c0-c3).
 */
typedef struct AvcInter {
	uint8_t window[13][16];		/* from x - 2 and y - 2 where the filter reads them */
	uint8_t taps[9][4];		/* a 4x4 block's along its filter: rows, or the columns */
	uint8_t block[8][8];
	uint8_t small[4][4];
	uint8_t list0[8][8];
} AvcInter;

/* the luma prediction's kernels (0x895140-0x897dd8) */
#define LUMA_HALF		0x02	/* their mode: the half sample alone */
void avcdecLoad4(AvcInter *inter, const uint8_t *address, const uint8_t *next, uint32_t y,
	uint32_t step, uint32_t shift);
void avcdecLoad4Tall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcdecLoad4Wide(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcdecLoad4WideTall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcdecLoad8(AvcInter *inter, const uint8_t *address, const uint8_t *next, uint32_t y,
	uint32_t step, uint32_t shift);
void avcdecLoad8Tall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcdecLoad8Wide(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift);
void avcdecLoad8WideTall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift);
void avcdecEdgeLeft4(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft4Tall(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft4Wide(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft4WideFar(AvcInter *inter);
void avcdecEdgeLeft4WideTall(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft4WideFarTall(AvcInter *inter);
void avcdecEdgeLeft8(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft8Tall(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft8Wide(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft8WideFar(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft8WideTall(AvcInter *inter, int32_t shift);
void avcdecEdgeLeft8WideFarTall(AvcInter *inter, int32_t shift);
void avcdecEdgeRight4(AvcInter *inter, int32_t shift);
void avcdecEdgeRight4Tall(AvcInter *inter, int32_t shift);
void avcdecEdgeRight4Wide(AvcInter *inter, int32_t shift);
void avcdecEdgeRight4WideFar(AvcInter *inter);
void avcdecEdgeRight4WideTall(AvcInter *inter, int32_t shift);
void avcdecEdgeRight4WideFarTall(AvcInter *inter);
void avcdecEdgeRight8(AvcInter *inter, int32_t shift);
void avcdecEdgeRight8Tall(AvcInter *inter, int32_t shift);
void avcdecEdgeRight8Wide(AvcInter *inter, int32_t shift);
void avcdecEdgeRight8WideFar(AvcInter *inter, int32_t shift);
void avcdecEdgeRight8WideTall(AvcInter *inter, int32_t shift);
void avcdecEdgeRight8WideFarTall(AvcInter *inter, int32_t shift);
void avcdecEdgeTop4(AvcInter *inter, int32_t y);
void avcdecEdgeTop4Tall(AvcInter *inter, int32_t y);
void avcdecEdgeTop8(AvcInter *inter, int32_t y);
void avcdecEdgeTop8Tall(AvcInter *inter, int32_t y);
void avcdecEdgeBottom4(AvcInter *inter, int32_t below);
void avcdecEdgeBottom4Tall(AvcInter *inter, int32_t below);
void avcdecEdgeBottom8(AvcInter *inter, int32_t below);
void avcdecEdgeBottom8Tall(AvcInter *inter, int32_t below);
void avcdecFull4(AvcInter *inter);
void avcdecTapsVertical4(AvcInter *inter);
void avcdecTapsHorizontal4(AvcInter *inter);
void avcdecLumaVertical4(AvcInter *inter, uint32_t mode);
void avcdecLumaHorizontal4(AvcInter *inter, uint32_t mode);
void avcdecLumaCentre4(AvcInter *inter, uint32_t mode);
void avcdecLumaDiagonal4(AvcInter *inter, uint32_t mode);
void avcdecLumaVertical8(AvcInter *inter, uint32_t mode);
void avcdecLumaHorizontal8(AvcInter *inter, uint32_t mode);
void avcdecLumaCentre8(AvcInter *inter, uint32_t mode);
void avcdecLumaDiagonal8(AvcInter *inter, uint32_t mode);
void avcdecLoadList0(AvcInter *inter, const uint8_t *samples);
void avcdecWeight(AvcInter *inter, int32_t weight, int32_t offset, int32_t round,
	int32_t shift);
void avcdecWeightBi(AvcInter *inter, int32_t weight0, int32_t weight1, int32_t offset,
	int32_t round, int32_t shift);
void avcdecAverage(AvcInter *inter);
void avcdecPlace4(AvcInter *inter, uint32_t block);

/*
 * The samples an intra macroblock is predicted from, by column (64 bytes):
 * the bottom row of the macroblock above, the corner above left, the right
 * column of the macroblock left; the decoder's residual job keeps them in
 * info->row1, the encoder's chunk job (with its coded 4x4 blocks) in the
 * chunk's neighbours.
 */
typedef struct AvcEdges {
	uint8_t above[16];
	uint8_t leftAbove[7];		/* the macroblock left's row above, its samples 8..14 */
	uint8_t aboveLeft;		/* and 15: this one's above left */
	uint8_t left[16];
	uint16_t leftBlocks;		/* the encoder's: the macroblock left's coded 4x4 blocks */
	uint16_t topBlocks;		/* the macroblock above's, then this one's */
	uint8_t reserved2[20];
} AvcEdges;

typedef char avcEdgesSizeCheck[sizeof(AvcEdges) == 64 ? 1 : -1];

/*
 * The residual in the coprocessor: the dequantisation (c8-c11, c5), a 4x4
 * block's residual (c12-c15), an Intra16x16 macroblock's DC levels (c17-c20,
 * the 8x8 block's in c16), the 8x8 block reconstructed (c21-c28) and an
 * Intra4x4 macroblock's neighbours (c26-c31); the encoder's copies of the
 * kernels keep an Intra16x16 macroblock's DC levels in c16-c19 (the 8x8
 * block's in c20), the dequantisation in c10, c11.
 */
typedef struct AvcRecon {
	int16_t scale[16];		/* LevelScale by QP % 6, by position */
	uint32_t shift;			/* QP / 6 */
	int16_t residual[4][4];
	int16_t dc[4][4];		/* by 8x8 block, its 4x4 blocks' */
	int16_t blockDc[4];		/* the 8x8 block's, the next first */
	uint8_t block[8][8];
	uint8_t mbAbove[16];		/* the macroblock's: the row above */
	uint8_t mbAboveRight[8];	/* the macroblock above right's, or the last repeated */
	uint8_t bottom[16];		/* the first 8x8 blocks' last rows */
	uint8_t above[16];		/* the 8x8 block's: above it and above right */
	uint8_t left[8];
	uint8_t aboveLeft;
	uint8_t predicted[4][4];	/* the encoder's: the 4x4 block just predicted (c16, c17) */
} AvcRecon;

/* the intra prediction's and the residual's kernels (0x899110-0x89a768) */
void avcdecDcIntra16(AvcRecon *recon, const int16_t *coef, uint32_t qp);
void avcdecIdctAc(AvcRecon *recon, const int16_t *coef);
void avcdecIdctDc(AvcRecon *recon);
void avcdecIdct(AvcRecon *recon, const int16_t *coef);
void avcdecAddIntra0(AvcRecon *recon);
void avcdecAddIntra1(AvcRecon *recon);
void avcdecAddIntra2(AvcRecon *recon);
void avcdecAddIntra3(AvcRecon *recon);
void avcdecStoreIntra(const AvcRecon *recon, uint8_t *samples);
void avcdecIntraNeighbours(AvcRecon *recon, const AvcEdges *edges,
	uint32_t aboveRight);
void avcdecIntraNext(AvcRecon *recon, uint32_t block, const AvcEdges *edges);
void avcdecIntra4x4Block0(AvcRecon *recon, uint32_t mode, uint32_t left, uint32_t above);
void avcdecIntra4x4Block1(AvcRecon *recon, uint32_t mode, uint32_t above);
void avcdecIntra4x4Block2(AvcRecon *recon, uint32_t mode, uint32_t left);
void avcdecIntra4x4Block3(AvcRecon *recon, uint32_t mode);
void avcdecIntraKeep(const AvcRecon *recon, const uint8_t *samples, AvcEdges *edges);
void avcdecLoadIntra16(AvcRecon *recon, const uint8_t *samples);
void avcdecAddIntra16Block0(AvcRecon *recon);
void avcdecAddIntra16Block1(AvcRecon *recon);
void avcdecAddIntra16Block2(AvcRecon *recon);
void avcdecAddIntra16Block3(AvcRecon *recon);
void avcdecStoreIntra16(const AvcRecon *recon, uint8_t *samples);
void avcdecKeepRightIntra16(const AvcRecon *recon, uint8_t **right);
void avcdecIntra16Vertical(const AvcEdges *edges, uint8_t *samples);
void avcdecIntra16Horizontal(const uint8_t *left, uint8_t *samples);
void avcdecIntra16Dc(const uint8_t *left, const AvcEdges *edges, uint32_t leftAvailable,
	uint32_t aboveAvailable, uint8_t *samples);
void avcdecIntra16Plane(const uint8_t *left, const AvcEdges *edges, uint32_t aboveLeft,
	uint8_t *samples);

/*
 * The samples the chroma job predicts an intra macroblock from, by column,
 * in info->row2 (64 bytes): the bottom rows of the macroblock above, its
 * corners, the right columns of the macroblock left; Cb then Cr.
 */
typedef struct AvcChromaEdges {
	uint8_t above[2][8];
	uint8_t reserved[6];
	uint8_t aboveLeftCr;
	uint8_t aboveLeftCb;
	uint8_t left[2][8];
	uint8_t reserved2[24];
} AvcChromaEdges;

/*
 * The chroma prediction in the coprocessor: the reference samples a fetch
 * read (c22-c31), a 4x4 block's put by its 2x2 blocks (c12-c23) and each
 * list's prediction, a row's Cb then its Cr (c8-c11, c12-c15).
 */
typedef struct AvcChromaInter {
	uint8_t window[2][5][8];	/* by component: from (x, y) where the fetch read them */
	uint8_t quarter[4][3][2][4];	/* by 2x2 block: its window's rows, Cb then Cr */
	uint8_t block[2][4][2][4];	/* by list and row */
} AvcChromaInter;

/*
 * The chroma residual in the coprocessor: the dequantisation (c9-c10, c8,
 * c5), the DC levels (c11), a 4x4 block's residual (c12-c15), and the
 * samples (c16-c31): rows, or each 4x4 block's in pairs of registers
 * (avcdecChromaBlocks*).
 */
typedef struct AvcChromaRecon {
	int16_t scale[8];		/* LevelScale by QP % 6, positions 0-7 */
	uint32_t dcShift;		/* |QP / 6 - 1| */
	uint32_t shift;			/* QP / 6 */
	int16_t dc[4];			/* the next 4x4 block's last */
	int16_t residual[4][4];
	uint8_t rows[2][8][8];		/* by component */
} AvcChromaRecon;

/* the chroma job's kernels (0x89a760-0x89cfe0) */
void avcdecChromaLoad(AvcChromaInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcdecChromaEdgeLeft(AvcChromaInter *inter, int32_t shift);
void avcdecChromaEdgeRight(AvcChromaInter *inter, int32_t shift);
void avcdecChromaEdgeTop(AvcChromaInter *inter, int32_t y);
void avcdecChromaEdgeBottom(AvcChromaInter *inter, int32_t below);
void avcdecChromaLoadSmall(AvcChromaInter *inter, const uint8_t *address,
	const uint8_t *next, uint32_t y, uint32_t step, uint32_t shift);
void avcdecChromaEdgeLeftSmall(AvcChromaInter *inter, int32_t shift);
void avcdecChromaEdgeRightSmall(AvcChromaInter *inter, int32_t shift);
void avcdecChromaEdgeTopSmall(AvcChromaInter *inter, int32_t y);
void avcdecChromaEdgeBottomSmall(AvcChromaInter *inter, int32_t below);
void avcdecChromaQuarter0(AvcChromaInter *inter);
void avcdecChromaQuarter1(AvcChromaInter *inter);
void avcdecChromaQuarter2(AvcChromaInter *inter);
void avcdecChromaQuarter3(AvcChromaInter *inter);
void avcdecChromaPredict(AvcChromaInter *inter, uint32_t fraction);
void avcdecChromaPredictList1(AvcChromaInter *inter, uint32_t fraction);
void avcdecChromaPredictQuarters(AvcChromaInter *inter, uint32_t fraction0,
	uint32_t fraction1, uint32_t fraction2, uint32_t fraction3);
void avcdecChromaPredictQuartersList1(AvcChromaInter *inter, uint32_t fraction0,
	uint32_t fraction1, uint32_t fraction2, uint32_t fraction3);
void avcdecChromaStore(const AvcChromaInter *inter, uint8_t *cb, uint8_t *cr);
void avcdecChromaStoreList1(const AvcChromaInter *inter, uint8_t *cb, uint8_t *cr);
void avcdecChromaStoreAverage(AvcChromaInter *inter, uint8_t *cb, uint8_t *cr);
void avcdecChromaLoadRows(AvcChromaRecon *recon, const uint8_t *samples);
void avcdecChromaLoadBottom(AvcChromaRecon *recon, const uint8_t *samples);
void avcdecChromaDc(AvcChromaRecon *recon, const int16_t *coef, uint32_t qp);
void avcdecChromaIdctDc(AvcChromaRecon *recon);
void avcdecChromaIdct(AvcChromaRecon *recon, const int16_t *coef);
void avcdecChromaStoreCb(const AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaStoreCr(const AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaStoreRows(const AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaAddDcCb(AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaAddDcCr(AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaBlocksCb(AvcChromaRecon *recon);
void avcdecChromaBlocksCr(AvcChromaRecon *recon);
void avcdecChromaAddCb0(AvcChromaRecon *recon);
void avcdecChromaAddCb1(AvcChromaRecon *recon);
void avcdecChromaAddCb2(AvcChromaRecon *recon);
void avcdecChromaAddCb3(AvcChromaRecon *recon);
void avcdecChromaAddCr0(AvcChromaRecon *recon);
void avcdecChromaAddCr1(AvcChromaRecon *recon);
void avcdecChromaAddCr2(AvcChromaRecon *recon);
void avcdecChromaAddCr3(AvcChromaRecon *recon);
void avcdecChromaRowsCb(AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaRowsCr(AvcChromaRecon *recon, uint8_t *samples);
void avcdecChromaKeepBottom(const AvcChromaRecon *recon, AvcChromaEdges *edges);
void avcdecChromaKeepRight(const AvcChromaRecon *recon, uint8_t (*left)[8]);
void avcdecChromaIntraDc(AvcChromaRecon *recon, const AvcChromaEdges *edges,
	uint32_t leftAvailable, uint32_t aboveAvailable);
void avcdecChromaIntraHorizontal(AvcChromaRecon *recon, const AvcChromaEdges *edges);
void avcdecChromaIntraVertical(AvcChromaRecon *recon, const AvcChromaEdges *edges);
void avcdecChromaIntraPlane(AvcChromaRecon *recon, const AvcChromaEdges *edges);

/* the luma's deblocking in the coprocessor: half the macroblock, the samples across an edge */
typedef struct AvcDeblock {
	uint8_t rows[8][16];		/* each 8x8 half turned for the vertical edges */
	uint8_t p[4][8];		/* p3 to p0: rows above, or the columns left turned */
} AvcDeblock;

/* the chroma's deblocking in the coprocessor: the macroblock, the samples across an edge */
typedef struct AvcChromaDeblock {
	uint8_t rows[2][8][8];		/* by component: rows, or turned for the vertical edges */
	uint8_t p[2][2][8];		/* by component: p1, p0 */
} AvcChromaDeblock;

/* the luma's (0x897dd8-0x898f80) */
void avcdecDeblockLoad(AvcDeblock *deblock, const uint8_t *samples);
void avcdecDeblockStore(const AvcDeblock *deblock, uint8_t *samples);
void avcdecDeblockStoreLeft(const AvcDeblock *deblock, uint8_t *samples);
void avcdecDeblockTurnLeft(AvcDeblock *deblock);
void avcdecDeblockTurnRight(AvcDeblock *deblock);
uint32_t avcdecDeblockEdge(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
uint32_t avcdecDeblockEdgeRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcdecDeblockInner(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcdecDeblockInnerRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcdecDeblockMiddle(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcdecDeblockInnerLast(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);

/* the chroma's (0x89b650-0x89bed8) */
void avcdecChromaDeblockLoad(AvcChromaDeblock *deblock, const uint8_t *samples);
void avcdecChromaDeblockStore(const AvcChromaDeblock *deblock, uint8_t *samples);
void avcdecChromaDeblockStoreLeft(const AvcChromaDeblock *deblock, uint8_t *samples);
void avcdecChromaDeblockKeepLeft(const AvcChromaDeblock *deblock, uint8_t (*columns)[2][8]);
uint32_t avcdecChromaDeblockTurn(AvcChromaDeblock *deblock);
uint32_t avcdecChromaDeblockMask(const AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths, uint32_t inner);
uint32_t avcdecChromaDeblockEdge(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths);
uint32_t avcdecChromaDeblockInner(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths);

/* an encoding job's state */
#define VIDEO_AVC_SEIS		4	/* buffering period, picture timing, pan-scan, recovery point */
/* the encoder's own CABAC coder, for skipped pictures (H.264 9.3.4) */
typedef struct VideoAvcCabacCoder {
	uint16_t count;			/* macroblocks coded */
	uint8_t x;
	uint8_t lastX;
	uint32_t low;			/* codILow */
	uint32_t range;			/* codIRange */
	uint32_t firstBit;		/* firstBitFlag */
	int32_t outstanding;		/* bitsOutstanding */
	uint8_t state;			/* mb_skip_flag's pStateIdx << 1 | valMPS */
} VideoAvcCabacCoder;

typedef struct VideoAvcEncodeJob {
	VideoAvcWriter *writer;
	VideoAvcWriter *stream;
	VideoAvcPictureInfo *picture;
	VideoAvcNalList *nals;
	int8_t sei[VIDEO_AVC_SEIS];
	uint32_t cabacState;		/* writer->cabacState when the helper holds the unit */
	uint32_t unitState;		/* the CABAC engine's command word */
	uint32_t macroblock;		/* the row's buffers, from the job's (VideoAvcMacroblock) */
	uint32_t predModes;		/* int8 per 4x4 block */
	uint32_t chromaModes;		/* uint8 per macroblock */
	uint32_t rowBuffer3;
	uint32_t mvds;			/* int16 pairs */
	uint32_t luma;			/* 16 int16 coefficients per block */
	uint32_t cb;
	uint32_t cr;
	uint32_t reserved[7];
	VideoAvcCabacCoder *coder;
	uint32_t reserved2[16];
	uint32_t skipRun;		/* mb_skip_run of a skipped picture */
	uint32_t binCount;		/* CABAC's bins, for the zero words */
	uint32_t jump[8];
} VideoAvcEncodeJob;

/* the encoder's stream as the helper's jobs get it (64 bytes) */
typedef struct VideoAvcStreamInfo {
	uint32_t buffer;		/* the stream's, used a half at a time */
	int32_t size;			/* twice the bytes the unit may write */
	uint32_t writer;		/* a VideoAvcWriter, 192 bytes */
	uint32_t nals;			/* a VideoAvcNalList, 192 bytes */
	uint32_t settings;		/* the sequence's AvcencSequence, 128 bytes */
	uint32_t interval;		/* the settings' */
	uint32_t bitRate;
	uint16_t skipSliceBytes;	/* a skipped picture's: its slices' */
	uint16_t skipHeaderBytes;	/* and the rest */
	int8_t groupRows[8];		/* the rows of each group of jobs */
	uint8_t groups;
	uint8_t idr;
	uint8_t reserved3;
	int8_t cabac;			/* entropy_coding_mode_flag */
	uint8_t noVui;
	uint8_t reserved4[19];
} VideoAvcStreamInfo;

typedef VideoAvcStreamInfo VideoAvcOptions;

/* the picture's flags in its NAL list */
#define VIDEO_AVC_PICTURE_SKIPPED	0x01	/* replaced by a skipped picture */
#define VIDEO_AVC_PICTURE_FAILED	0x02	/* and an IDR follows */
#define VIDEO_AVC_PICTURE_OVER_RATE	0x04	/* the IDR period's pictures took more than the bit rate */
#define VIDEO_AVC_PICTURE_REPLACED	0x08	/* an IDR follows a skipped picture */
#define VIDEO_AVC_PICTURE_FIRST		0x10	/* the stream's first picture replaced */

/* a command of the helper's, as the encoder fills it (64 bytes) */
typedef struct VideoAvcCommand {
	int16_t parameters[VIDEO_AVC_UNIT_PARAMETERS];
	uint16_t reserved;
	uint16_t reserved2;
	uint8_t widthInMacroblocks;
	uint8_t heightInMacroblocks;
	uint8_t mode;
	uint8_t reserved3[23];
	uint32_t result;		/* what the picture came to */
	uint32_t reserved4[4];
} VideoAvcCommand;

/* the head of a helper's slice: its context and its command */
/* a reference's weights for the unit: luma, then each chroma's */
typedef struct VideoAvcSliceWeight {
	int16_t lumaWeight;
	int16_t lumaOffset;
	int16_t chromaWeight;
	int16_t chromaOffset;
	int16_t chromaWeight2;
	int16_t chromaOffset2;
} VideoAvcSliceWeight;

typedef struct VideoHelperSlice {
	uint32_t context;
	uint32_t command;
	uint32_t sliceHeader;		/* the decoder's: AVC_SLICE_* */
	uint8_t qp;
	int8_t chromaQpIndexOffset;
	int8_t numRefIdxActive[2];
	int8_t refListSize[2];		/* the lists' pictures, as reordered */
	int8_t alphaOffset;		/* doubled: slice_alpha_c0_offset_div2 */
	int8_t betaOffset;
	uint8_t lumaLog2WeightDenom;
	uint8_t chromaLog2WeightDenom;
	uint8_t colocatedShortTerm;	/* RefPicList1[0] is a short-term reference */
	uint8_t missingReference;	/* a reference is missing: the slice is concealed */
	int8_t refList[2][18];		/* -1, the pictures' indexes, -1 after */
	int16_t distScaleFactor[16];	/* temporal direct, by list 0's index */
	int8_t refIdxOfPicture[18];	/* list 0's index of each picture */
	uint8_t reserved2[14];
	uint32_t colocated;		/* the decoder's: RefPicList1[0]'s motion */
	uint32_t refLuma[2][16];	/* the decoder's: the references' frames */
	uint32_t refChroma[2][16];
	union {
		VideoAvcSliceWeight weights[2][16];	/* explicit: by list and index */
		int16_t implicitWeight[192];	/* implicit: list 0's weight, by l0 * L1 count + l1 */
	};
	uint8_t reserved3[128];
} VideoHelperSlice;

/* the decoder's picture command: its row sizes, as the NAL's */
typedef struct VideoAvcDecodeCommand {
	uint16_t mbRowSize;
	uint16_t intraRowSize;
	uint16_t chromaModeRowSize;
	uint16_t refIdxRowSize;
	uint16_t mvdRowSize;
	uint16_t coefRowSize;
	uint16_t chromaCoefRowSize;
	uint16_t pcmRowSize;
	uint16_t pcmChromaRowSize;
	uint8_t widthInMacroblocks;
	uint8_t heightInMacroblocks;
	uint8_t mode;			/* 6 below 40 macroblocks across, else 8 */
	int8_t picture;			/* the picture decoded, and its frame */
	uint8_t reserved[2];
	uint32_t address;
	uint16_t width;
	uint16_t pitch;
	uint32_t frameSize;
	uint32_t lumaSize;
	uint32_t chromaSize;
	uint32_t decoderData;
	int32_t picOrderCnt;
	uint8_t type;			/* the NAL's */
	uint8_t refIdc;
	uint8_t reserved2[10];
} VideoAvcDecodeCommand;

/* the encoder engine's message to the helper: encode the job */
void avcencHelperEncode(uint32_t helper, uint32_t argument);		/* 0x86454e */

/* the helper's memory: its header and tables, the shared block, the contexts, the
 * commands, the slices, the job buffers, the contexts' common memory, then theirs */
#define VIDEO_HELPER_TABLE128		128
#define VIDEO_HELPER_TABLE1088		1088
#define VIDEO_HELPER_TABLE1152		1152
#define VIDEO_HELPER_SHARED		5248
#define VIDEO_HELPER_CONTEXTS		5376
#define VIDEO_HELPER_COMMANDS		5504
#define VIDEO_HELPER_SLICES		5760
#define VIDEO_HELPER_JOBS		9344
#define VIDEO_HELPER_COMMON		9856
#define VIDEO_HELPER_FIXED		11392
#define VIDEO_HELPER_SLICE_SIZE		896

/* what the AVC parser engine tells the decoder engine of a picture parsed (a message's word) */
typedef struct AvcdecParsed {
	uint8_t valid;
	uint8_t flush;			/* the stream was flushed */
	uint8_t pictureInfo[2];		/* the unit's, swapped */
} AvcdecParsed;

/* the engines: two threads per codec, each entered with its memory and the codec */
void avcdecParserThread(uint32_t memory, uint32_t codec);		/* 0x824ca0 */
void avcdecParserAu(uint32_t memory, uint32_t request);		/* 0x824f96 */
void avcdecParserWake(uint32_t memory, uint32_t request);		/* 0x8252f6 */
void avcdecParserInitialize(uint32_t memory, VideoService *service, int32_t index);
void avcdecDecoderThread(uint32_t memory, uint32_t codec);		/* 0x82cd90 */
void avcdecDecoderDecode(uint32_t memory, uint32_t request);		/* 0x82d2e4 */
void avcdecDecoderInitialize(uint32_t memory, VideoService *service, int32_t index);
void avcencEncoderThread(uint32_t memory, uint32_t codec);		/* 0x834588 */
void avcencEncoderEncode(uint32_t memory, uint32_t request);		/* 0x834904 */
void avcencEncoderInitialize(uint32_t memory, VideoService *service, int32_t index);
void avcencEncoderSequence(uint32_t memory, uint32_t settings);		/* 0x83482a */
/* QPc by qPI (Table 8-15), the decoder's and the encoder's (avcdeckernel.c) */
extern const uint8_t avcChromaQp[52];

void avcencStreamThread(uint32_t memory, uint32_t codec);		/* 0x8356b0 */
void avcencStreamAu(uint32_t memory, uint32_t request);		/* 0x835b8c */
void avcencStreamInitialize(uint32_t memory, VideoService *service, int32_t index);
void m4vdecParserThread(uint32_t memory, uint32_t codec);		/* 0x841730 */
void m4vdecParserAu(uint32_t memory, uint32_t request);		/* 0x841a10 */
void m4vdecParserWake(uint32_t memory, uint32_t request);		/* 0x841cf2 */
void m4vdecParserInitialize(uint32_t memory, VideoService *service, int32_t index);
void m4vdecDecoderThread(uint32_t memory, uint32_t codec);		/* 0x8471ac */
void m4vdecDecoderDecode(uint32_t memory, uint32_t request);		/* 0x847676 */
void m4vdecDecoderInitialize(uint32_t memory, VideoService *service, int32_t index);

/* the helper's memory and start (0x86238c-) */
uint32_t videoHelperQueryMemory(uint32_t commandSize, uint32_t horizontal);
void videoHelperInitialize(uint32_t memory, VideoService *service);
void videoHelperThread(uint32_t memory, uint32_t argument2);
void videoHelperStart(uint32_t helper, uint32_t request);		/* 0x8615ba */
void videoHelperJob(uint32_t helper, uint32_t job);			/* 0x8616b0 */
uint32_t videoHelperInterrupt(uint32_t status, uint32_t argument2);	/* 0x86186a */
void videoHelperLoad(const VideoHelperShared *shared, const VideoStream *stream,
	const VideoNal *nal);							/* 0x861a12 */

/* the engines' answers to the service */
void videoServiceAuDone(uint32_t service, uint32_t request);
void videoServicePicturesDone(uint32_t service, uint32_t request);

/* video/jump.S: a return to a point saved, out of a bitstream error */
int32_t videoSetJump(uint32_t *buffer);
void videoLongJump(uint32_t *buffer, int32_t value);

/* application/video/avcdeccabac.c: the CABAC slices, decoded here, not on the unit */
void avcdecCabacStart(const VideoHelperShared *shared, VideoSliceJob *job, VideoHelperJob *run);
int32_t avcdecCabacSliceI(VideoSliceJob *job, VideoHelperJob *run);
int32_t avcdecCabacSliceP(VideoSliceJob *job, VideoHelperJob *run);
int32_t avcdecCabacSliceB(VideoSliceJob *job, VideoHelperJob *run);
void avcdecCabacFinish(const VideoHelperShared *shared, VideoSliceJob *job, VideoHelperJob *run);

/* application/video/avcenccabac.c */
void avcencSliceData(VideoAvcEncodeJob *job, const VideoHelperJob *run);
void avcencCabacBegin(VideoAvcEncodeJob *job);
void avcencCabacAlign(VideoAvcEncodeJob *job);
void avcencCabacNalHeader(VideoAvcEncodeJob *job, uint32_t header);
void avcencCabacSliceHeader(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb);
void avcencCabacSliceInit(VideoAvcEncodeJob *job, uint32_t sliceType);
void avcencCabacFlush(VideoAvcEncodeJob *job);
void avcencRunOpen(const VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoHelperJob *run);
void avcencRunClose(const VideoHelperShared *shared, VideoAvcEncodeJob *job, VideoHelperJob *run);
void avcencCabacReset(VideoAvcEncodeJob *job, uint32_t value);
void avcencCabacInit(VideoAvcEncodeJob *job, uint32_t sliceType);
void avcencCabacSkipped(VideoAvcEncodeJob *job, uint32_t sliceType);

/* application/video/avcencencoder.c: the stream opened for a job and closed after it */
void avcencStreamOpenEnd(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);
void avcencStreamBegin(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);
void avcencStreamEnd(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job);

/* application/video/avcencheader.c */
void avcencWriteSps(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job);
void avcencWritePps(const VideoAvcSequence *sequence, const VideoAvcOptions *options,
	VideoAvcEncodeJob *job);
void avcencWriteAud(const VideoAvcOptions *options, VideoAvcEncodeJob *job, uint32_t force);
void avcencWriteSkipSlice(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb);
void avcencWriterBegin(const VideoAvcOptions *options, VideoAvcEncodeJob *job, uint32_t keepBits);
void avcencCabacZeroWords(const VideoAvcStreamInfo *info, VideoAvcEncodeJob *job2);
void avcencUnitLoad(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencUnitFinish(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencUnitNalEnd(VideoHelperShared *shared, const VideoAvcStreamInfo *info,
	VideoAvcWriter *writer);
void avcencBitsPut(VideoAvcWriter *writer, int32_t count, uint32_t value);
void avcencBitsCopyBlock(VideoAvcWriter *writer, const uint32_t *source);
void avcencBitsCopy(VideoAvcWriter *writer, const uint32_t *source, int32_t size);
void avcencWriteSei(const VideoAvcOptions *options, VideoAvcEncodeJob *job);
void avcencWriteSliceHeader(const VideoAvcOptions *options, VideoAvcEncodeJob *job,
	uint32_t firstMb);

/* application/video/avcenchelper.c */
void avcencHelperStart(VideoHelper *helper, VideoHelperShared *shared);
void avcencHelperJob(VideoHelper *helper, uint32_t address);
void avcencHelperRun(VideoHelper *helper, uint32_t job);
void avcencHelperFinish(VideoHelper *helper, uint32_t argument);
void avcencHelperStep(VideoHelper *helper);
void avcencHelperStop(VideoHelper *helper);

/* application/video/avchelper.c */
void avcdecHelperStart(VideoHelper *helper, VideoHelperShared *shared);
void avcdecHelperJob(VideoHelper *helper, uint32_t address);
void avcdecHelperRun(VideoHelper *helper, uint32_t job);
void avcdecHelperFinish(VideoHelper *helper);
void avcdecHelperStep(VideoHelper *helper);

/* application/video/m4vhelper.c */
void m4vdecHelperStart(VideoHelper *helper, VideoHelperShared *shared);
void m4vdecHelperJob(VideoHelper *helper, uint32_t address);
void m4vdecHelperRun(VideoHelper *helper, uint32_t job);
void m4vdecHelperFinish(VideoHelper *helper);
void m4vdecHelperStep(VideoHelper *helper);

#endif
