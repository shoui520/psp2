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

/* a request's code (the codecs' SERVICE_*) */
#define SERVICE_OPEN			1
#define SERVICE_CLOSE			2
#define SERVICE_NONE			3
#define SERVICE_AU			19
#define SERVICE_DECODE			20

/* a reply's kind */
#define SERVICE_REPLY_AU		2
#define SERVICE_REPLY_PICTURES		8
#define SERVICE_REPLY_DONE		2048

/* the codecs (the request's codec byte) */
#define SERVICE_CODEC_AVCDEC		0
#define SERVICE_CODEC_M4VDEC		1
#define SERVICE_CODEC_AVCENC		2

/* the thread flags the service waits on */
#define SERVICE_FLAG_ENGINE		0x0002
#define SERVICE_FLAG_ENGINE2		0x0008
#define SERVICE_FLAG_REQUEST		0x0200
#define SERVICE_FLAG_RELEASE		0x2000
#define SERVICE_FLAG_EXIT		0x8000

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
	uint32_t reserved;
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
	uint32_t reserved2[4];
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
#define VIDEO_STAGE_NONE		0
#define VIDEO_STAGE_ADD_SET		1	/* the parameter set being added */
#define VIDEO_STAGE_BUFFER		2
#define VIDEO_STAGE_ADD_SET2		3	/* the set read, the second being added */
#define VIDEO_STAGE_SET2		4
#define VIDEO_STAGE_SETS		5
#define VIDEO_STAGE_SETS2		6
#define VIDEO_STAGE_SLICE		7

/* the NAL unit types the helper and the parser look at */
#define NAL_SLICE			1
#define NAL_IDR				5
#define NAL_SEI				6
#define NAL_AUD				9
#define NAL_FILLER			12

/* the stream's ring, as the parser sees it */
typedef struct VideoStream {
	uint32_t reserved;
	uint32_t base;			/* the lowest address in use */
	uint32_t reserved2;
	uint32_t ringStart;
	uint32_t ringEnd;		/* its last byte */
	uint32_t reserved3[5];
	uint32_t parameterCache;	/* a VideoParameterCache */
	uint32_t parameterSets;		/* 1088 bytes each */
	uint32_t reserved4[2];
	uint32_t nal;			/* the unit being parsed (a VideoNal, 1216 bytes) */
	uint32_t pictures;		/* VideoAvcPicture, nal->pictures of them */
	uint32_t pictureExtras;		/* 16 bytes each */
} VideoStream;

/* a picture of the stream's (128 bytes) */
typedef struct VideoAvcPicture {
	uint8_t reserved[27];
	uint8_t referenceInfo;
	uint8_t reserved2[3];
	uint8_t incomplete;		/* its slices ended before its last macroblock */
	uint8_t reserved3[96];
} VideoAvcPicture;

/* a unit of the stream (a NAL unit) found in the ring, read through the unit */
typedef struct VideoNal {
	VideoBitstream bits;
	uint32_t reserved[2];
	uint32_t search;		/* where the search goes on */
	int32_t searchLeft;
	uint32_t start;			/* its first byte */
	int32_t size;
	uint32_t flags;			/* 0x200000: nothing for the unit to do */
	uint32_t pictureCount;
	uint32_t lastPictureCount;
	int32_t error;
	int16_t firstMacroblock;	/* of the run */
	int16_t macroblocks;		/* in the picture */
	uint8_t widthInMacroblocks;
	uint8_t reserved2;
	int16_t rowsPerRun;		/* a run that starts a row takes at most these */
	uint16_t reserved6;
	int16_t runMacroblocks;		/* the unit's word 8: the run's length */
	int16_t doneMacroblocks;	/* the unit's words 11 and 10, when it is not run */
	int16_t doneStatus;
	uint8_t reserved7[3];
	uint8_t load;			/* the slice's data goes to the unit */
	uint32_t context;		/* the helper's context, command and slice for the run */
	uint32_t slice;
	uint32_t command;
	uint32_t reserved8;
	uint8_t type;
	uint8_t refIdc;
	uint8_t reserved3;
	uint8_t pictures;		/* entries of the stream's picture tables */
	uint8_t reserved9[5];
	uint8_t escaped;		/* emulation prevention bytes are not removed */
	uint8_t parameterSet;		/* the stream's 1088-byte table in use */
	uint8_t parameterSet2;		/* and its 16-byte one */
	uint8_t reserved4[88];
	int8_t picture;			/* the picture decoded */
	uint8_t reserved10[2];
	uint8_t waitFrame;
	uint8_t reserved5[17];
	uint8_t pictureInfo[2];		/* given to the engine with the job */
	uint8_t reserved11[25];
	uint8_t referenceInfo;
	uint8_t reserved12[7];
	uint8_t failed;
	uint8_t skip;			/* the unit is not run */
	uint8_t reserved13;
	int8_t nextType;		/* of the unit after the slice, -1 none */
	VideoBitsState state;
	uint8_t reserved14[18];
	uint16_t nextFirstMacroblock;	/* of the slice after this one */
} VideoNal;

/* a parser's view: its bit reader and the unit it reads */
typedef struct VideoParser {
	VideoBitstream *bits;
	VideoNal *nal;
} VideoParser;

/* a 33-bit time stamp, doubled; -1 none (word aligned in the MPEG-4 structures) */
typedef int64_t VideoTime __attribute__((aligned(4)));

#define VIDEO_TIME_NONE		(-1LL)
#define VIDEO_TIME_WRAP		0x3ffffffffLL

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
	uint32_t parameterSet;
	uint32_t reserved;
	uint32_t parameterSet2;		/* 16 bytes */
	uint32_t reserved5;
	uint32_t buffer;		/* 256 bytes */
	uint32_t reserved6;
	uint32_t slice;			/* 896 bytes */
	VideoAvcPicture *pictures;
	VideoAvcPicture *picture;
	uint32_t pictureExtras;
	uint32_t reserved2;
	uint8_t first;
	uint8_t reserved3[35];
	uint32_t jump[8];		/* videoSetJump's, for the parser's errors */
	uint32_t reserved4[18];
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
	uint32_t reserved2;
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
	uint16_t reserved4;
	uint8_t log2MaxFrameNum;
	uint8_t initialCpbRemovalDelayLength;
	uint8_t cpbRemovalDelayLength;
	uint8_t dpbOutputDelayLength;
	uint8_t pendingBits;		/* the unit's partial byte */
	uint8_t picStructPresent;
	uint8_t reserved8[26];
	uint32_t commandResult;
	uint8_t cabacState[52];		/* the job's, while the helper holds the unit */
} VideoAvcWriter;

/* the NAL units written for a picture */
#define VIDEO_AVC_NALS		12
typedef struct VideoAvcNalList {
	uint8_t count;
	uint8_t reserved[11];
	uint8_t type[VIDEO_AVC_NALS];
	uint8_t refIdc[VIDEO_AVC_NALS];
	uint32_t start[VIDEO_AVC_NALS];
	uint32_t size[VIDEO_AVC_NALS];
} VideoAvcNalList;

/* the sequence's parameters, as the client set them */
typedef struct VideoAvcSequence {
	uint8_t reserved[20];
	uint8_t profileIdc;
	uint8_t levelIdc;
	uint8_t reserved2;
	uint8_t aspectRatioIdc;		/* 0: no aspect ratio information */
	uint16_t sarWidth;
	uint16_t sarHeight;
	uint8_t reserved3;
	uint8_t frameCropping;
	uint8_t cropRight;
	uint8_t cropBottom;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
	uint32_t vclBitRate;
	uint32_t nalBitRate;
	uint32_t vclCpbSize;
	uint32_t nalCpbSize;
	uint8_t reserved4;
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
	uint32_t reserved;
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
	uint32_t reserved;
	int32_t size;			/* twice the bytes the unit may write */
	uint32_t writer;		/* a VideoAvcWriter, 192 bytes */
	uint32_t nals;			/* a VideoAvcNalList, 192 bytes */
	uint32_t reserved2[4];
	int8_t groupRows[8];		/* the rows of each group of jobs */
	uint8_t groups;
	uint8_t idr;
	uint8_t reserved3;
	int8_t cabac;			/* entropy_coding_mode_flag */
	uint8_t noVui;
	uint8_t reserved4[19];
} VideoAvcStreamInfo;

typedef VideoAvcStreamInfo VideoAvcOptions;

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
typedef struct VideoHelperSlice {
	uint32_t context;
	uint32_t command;
	uint8_t reserved[888];
} VideoHelperSlice;

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

#endif
