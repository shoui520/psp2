/*
 * The video codec library (RPCs 0-7, 16-20, 32-39 and 96-100; 0x81ff4c-):
 * the ARM's SceAvcodec "dsp0 videocodec". The library and each codec
 * instance run as server threads that take one command at a time.
 */
#pragma once

#include "../application.h"

#define VIDEO_ERROR_MEMORY		(-10001)

/* the server threads' kinds (videoServerEntries) */
#define VIDEO_SERVER_LIBRARY		1
#define VIDEO_SERVER_AVCDEC		2
#define VIDEO_SERVER_M4VDEC		3
#define VIDEO_SERVER_AVCENC		4

/* a server's flags: a command waits, a command is done */
#define VIDEO_FLAG_COMMAND		0x10
#define VIDEO_FLAG_DONE			0x20

/* the commands every server takes */
#define VIDEO_COMMAND_EXIT		4096

/* the AVC decoder server's commands (the library's create and delete too) */
#define AVCDEC_QUERY_MEMORY		257
#define AVCDEC_INITIALIZE		258
#define AVCDEC_QUERY_SEQUENCE		259
#define AVCDEC_QUERY_CONTEXT		260
#define AVCDEC_INITIALIZE_CONTEXT	261
#define AVCDEC_DECODE			262
#define AVCDEC_DECODE_STOP		263
#define AVCDEC_TERMINATE		264

/* the bitstream unit: fed by descriptors, it raises interrupt 21 when done */
#define VIDEO_UNIT			((volatile uint32_t *)0x4f5000)
#define VIDEO_UNIT_ADDRESS		3
#define VIDEO_UNIT_COMMAND		7	/* 6 load, (bit offset + 7) << 8 start, 16 */
#define VIDEO_UNIT_DESCRIPTORS		8
#define VIDEO_UNIT_RUN_STATUS		10	/* after a run: 129 the stream ended */
#define VIDEO_UNIT_RUN_DONE		11	/* after a run: its macroblocks done */
#define VIDEO_UNIT_BITS			13	/* the bits a run wrote; cleared after */
#define VIDEO_UNIT_STATUS		20	/* read and cleared by the interrupt */

/* VIDEO_UNIT_COMMAND's commands, and VIDEO_UNIT_RUN_STATUS's end */
#define VIDEO_UNIT_LOAD			6
#define VIDEO_UNIT_RUN			16	/* run the slice */
#define VIDEO_UNIT_ENDED		129	/* the stream ended */

/* the AVC profiles (profile_idc) */
#define PROFILE_BASELINE		66
#define PROFILE_MAIN			77

/* a server thread, in the first 40 bytes of its context */
typedef struct VideoServer {
	uint32_t command;
	uint32_t argument[5];
	int32_t result;
	VkThread *thread;
	VkThread *client;		/* who sent the command */
	volatile uint8_t *lock;
} VideoServer;

/* the library's parameters, as the library thread completes them */
typedef struct VideoLibraryParam {
	uint32_t resource;		/* the ARM's optional buffer */
	uint32_t core;			/* bit numbers of the two cores' masks */
	uint32_t core2;
	uint32_t (*callback[3])(void);
	int32_t numOfDecoders;
	int32_t numOfEncoders;
	uint32_t commandSize;
	uint16_t horizontal;
	uint8_t configuration[2];
	uint32_t queryData[2];
} VideoLibraryParam;

/* Dsp0VideocodecLibraryCommand's first 64 bytes, all the Venezia reads */
typedef struct VideoLibraryCommand {
	uint32_t context;		/* VideoServer: physical, then mapped */
	uint32_t library;		/* the VideoLibrary made */
	VideoLibraryParam param;
	uint32_t workAddress;		/* physical, then mapped */
	uint32_t workSize;
} VideoLibraryCommand;

/* the library, in the first 64 bytes of its work memory */
typedef struct VideoLibrary {
	uint32_t reserved[3];
	int32_t service;		/* the service's handle */
	uint32_t coreMask;
	uint32_t coreMask2;
	uint32_t serviceInfo[2];
	uint32_t reserved2;
	uint32_t serviceMemory;		/* the work memory after this */
	uint32_t (*callback[3])(void);
	volatile uint8_t *lock;
	uint8_t numOfDecoders;
	uint8_t reserved3;
	uint8_t numOfEncoders;
	uint8_t reserved4;
	uint16_t horizontal;
	int16_t active;
} VideoLibrary;

/* the codec service's requests (service.c): a request's code */
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

/* a codec instance (avcdec.c, m4vdec.c, avcenc.c): its memory, and a reply's */
#define INSTANCE_SIZE			128
#define REPLY_SIZE			64

/* its states (the encoder's STATE_ENCODING is STATE_DECODING) */
#define STATE_NONE			0
#define STATE_INITIALIZED		3
#define STATE_READY			4
#define STATE_DECODING			5

/* the requests to its server thread (the encoder's picture and encode are the AU and decode) */
#define REQUEST_INITIALIZE		0
#define REQUEST_TERMINATE		1
#define REQUEST_AU			3
#define REQUEST_DECODE			4

/* the reply bits a request waits for */
#define WAIT_AU				1
#define WAIT_DECODE			2
#define WAIT_DONE			4

/*
 * An access unit's stream as the ARM gives it: the buffer it lies in and
 * the unit's own range, physical; the decoder's copy has them mapped.
 */
typedef struct VideoAu {
	uint32_t buffer;
	uint32_t bufferSize;
	uint32_t es;
	uint32_t esSize;
	uint8_t advance;		/* the next unit is in the other buffer */
	uint8_t reserved[7];
	uint32_t pts[2];
	uint32_t dts[2];
	uint32_t numOfOutput;		/* pictures given back */
	uint32_t status;
} VideoAu;

/* what an access unit request asks */
#define MODE_DECODE			0
#define MODE_SEQUENCE			1
#define MODE_CONTEXT			2
#define MODE_DRAIN			3
#define MODE_RESTART			4

/* the unit's own modes (VideoAu.advance) */
#define AU_END				0
#define AU_NORMAL			1
#define AU_SKIP				3
#define AU_RESET			7

/* a decoder command (Dsp0VideocodecDecoderCommand's first 208 bytes) */
typedef struct VideoDecoderCommand {
	uint32_t context;		/* VideoDecoderContext: physical, then mapped */
	uint32_t handle;		/* the codec's instance */
	uint32_t param[10];		/* the operation's parameters */
	uint32_t auIndex;		/* which of au[] is current */
	uint32_t reserved;
	VideoAu au[2];
	uint32_t pictures;		/* VideoPicture[numOfPictures] */
	uint32_t numOfPictures;
	uint32_t workAddress;
	uint32_t workSize;
	uint32_t contextAddress;
	uint32_t contextSize;
	uint32_t reserved2[2];
	uint32_t option[6];
} VideoDecoderCommand;

/* an MPEG-4 decoder command: the AVC one with shorter parameters */
typedef struct VideoM4vdecCommand {
	uint32_t context;
	uint32_t handle;
	uint32_t param[8];
	uint32_t auIndex;
	uint32_t reserved;
	VideoAu au[2];
	uint32_t pictures;
	uint32_t numOfPictures;
	uint32_t workAddress;
	uint32_t workSize;
	uint32_t contextAddress;
	uint32_t contextSize;
	uint32_t reserved2[2];
	uint32_t option[6];
} VideoM4vdecCommand;

/* a buffer the encoder fills: physical when given back */
typedef struct VideoEncoderBuffer {
	uint32_t address;
	uint32_t size;
	uint8_t type;			/* the NAL unit's */
	uint8_t refIdc;
	uint8_t reserved[2];
} VideoEncoderBuffer;

/* a picture to encode: its luma plane, the chroma after it */
typedef struct VideoEncoderFrame {
	uint32_t luma;
	uint32_t reserved;
	uint32_t chroma;
	uint32_t reserved2;
	uint16_t width;
	uint16_t height;
	uint16_t pitch;
	uint16_t chromaPitch;
	uint8_t type;			/* VIDEO_ENCODER_FRAME_*, 0: none */
	uint8_t reserved3[3];
	uint32_t numOfNal;		/* NAL units made from it */
} VideoEncoderFrame;

/* an encoder's output: a picture's place in the stream buffer */
typedef struct VideoEncoderOutput {
	uint32_t address;
	uint32_t size;
	uint32_t numOfSei;		/* extra units to fetch afterwards */
	uint8_t info[2];
	uint8_t type;
	uint8_t reserved;
	uint32_t info2;
	uint32_t info3;
} VideoEncoderOutput;

/* an AVC encoder command (Dsp0VideocodecEncoderCommand's first 404 bytes) */
typedef struct VideoEncoderCommand {
	uint32_t context;		/* VideoEncoderContext: physical, then mapped */
	uint32_t handle;
	uint32_t param[9];
	uint32_t option;		/* &option1 when present */
	uint32_t option2;		/* &option2 when present */
	uint32_t reserved;
	uint8_t option1Present;		/* option1 starts here */
	uint8_t reserved2[7];
	uint8_t option2Present;
	uint8_t reserved3[3];
	uint32_t option2Data[2];	/* option2 starts here */
	uint32_t workAddress;
	uint32_t workSize;
	uint32_t work2Address;
	uint32_t work2Size;
	uint32_t work3Size;
	VideoEncoderBuffer sps;
	VideoEncoderBuffer pps;
	uint32_t encodeParam[6];
	uint32_t outputs;		/* VideoEncoderOutput[numOfOutputs] */
	uint32_t numOfOutputs;
	VideoEncoderBuffer sei[12];
	uint32_t parameterSets;		/* 256 bytes each: physical, then mapped */
	uint32_t parameterSets2;
	uint32_t frameIndex;
	VideoEncoderFrame frame[2];
	uint32_t reserved4[2];
	uint32_t pictureParam[6];
} VideoEncoderCommand;

/* the work memory record the encoder's initialization reads */
typedef struct VideoEncoderWork {
	uint32_t reserved;
	uint32_t workSize;
	uint32_t work2;
	uint32_t work2Size;
} VideoEncoderWork;

/* the encoder's context */
typedef struct VideoEncoderContext {
	VideoServer server;
	VideoEncoderWork workInfo;
	uint32_t reserved[4];
	VnzMemory work;
	VnzMemory work2Memory;
	VnzMemory frameMemory[2];
	VideoEncoderFrame frame[2];	/* mapped */
} VideoEncoderContext;

/* a decoder's context: its server and the memory it keeps open */
typedef struct VideoDecoderContext {
	VideoServer server;
	VnzMemory stream[2];		/* the two access unit buffers */
	VnzMemory context;
	uint32_t reserved;
	VideoAu au;			/* the current unit, mapped */
} VideoDecoderContext;

/* a decoded picture, as the codec gives it back */
typedef struct VideoPicture {
	uint32_t plane[4];		/* luma top, bottom; chroma top, bottom */
	uint16_t width;
	uint16_t pitch;
	uint16_t height;
	uint16_t chromaHeight;
	uint8_t info[4];
	uint32_t frame;
	uint32_t frameTop;
	uint32_t frameBottom;
	uint8_t structure;
	uint8_t field;
	uint8_t valid;
	uint8_t type;
	uint16_t crop[4];
	uint32_t data[3];		/* 256 bytes each */
	uint32_t pts[2];
	uint32_t dts[2];
	uint32_t userData;
	uint32_t userDataSize;
	uint32_t reserved[10];
} VideoPicture;

/* an MPEG-4 decoder's picture (64 bytes) */
typedef struct VideoM4vPicture {
	uint32_t plane[4];		/* luma top, bottom; chroma top, bottom */
	uint16_t width;
	uint16_t pitch;
	uint16_t height;
	uint16_t chromaHeight;
	uint8_t info[4];
	uint32_t data;
	uint8_t frameRate;		/* 1..8, 255 unknown */
	uint8_t vopInfo[4];
	uint8_t reserved[3];
	uint32_t pts[2];
	uint32_t dts[2];
	uint32_t userData;
	uint32_t userDataSize;
} VideoM4vPicture;

/* video/library.c */
int32_t videoMemoryOpenPublic(VnzMemory *memory);
int32_t videoMemoryClosePublic(VnzMemory *memory, uint32_t physical);
int32_t videoMemoryOpenPrivate(VnzMemory *memory);
int32_t videoMemoryClosePrivate(VnzMemory *memory);
int32_t videoMemoryOpenProtected(VnzMemory *memory);
int32_t videoMemoryCloseProtected(VnzMemory *memory);
uint32_t videoWritebackToPhysical(uint32_t address, uint32_t size);
uint32_t videoPurgeToPhysical(uint32_t address, uint32_t size);
uint32_t videoReopenPrivate(uint32_t address, uint32_t size);
uint32_t videoInvalidate(uint32_t address, uint32_t size);
int32_t videoVThreadExecute(VkVThreadContext *context, uint32_t index, VkJobFunction entry,
	uint32_t argument, uint32_t argument2, uint32_t argument3, uint32_t argument4);
int32_t videoServerCall(VideoServer *server, uint32_t command, uint32_t argument,
	uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5);
int32_t videoServerCreate(uint32_t kind, VideoServer *server);
int32_t videoServerDelete(VideoServer *server, uint32_t argument);

/* the library's service (0x82224c-) */
uint32_t videoLibraryQueryMemory(const VideoLibraryParam *param);
int32_t videoLibraryInitialize(uint32_t *library, const VideoLibraryParam *param,
	uint32_t workAddress, uint32_t workSize);
int32_t videoLibraryTerminate(VideoLibrary *library);

/* the codec service, in the shared library (0x860326-) */
uint32_t videoServiceQueryMemory(uint32_t numOfDecoders, uint32_t numOfEncoders,
	uint32_t commandSize, uint32_t horizontal);
void videoServiceSetup(uint32_t service, uint32_t resource, uint32_t coreMask,
	uint32_t coreMask2, uint32_t numOfDecoders, uint32_t numOfEncoders, uint32_t commandSize,
	uint32_t horizontal, uint32_t (*callback)(void), uint32_t (*callback2)(void),
	uint32_t (*callback3)(void), uint32_t version);
int32_t videoServiceOpen(uint32_t service, uint32_t *info, uint32_t *info2,
	uint32_t numOfDecoders, uint32_t numOfEncoders, uint32_t horizontal);
void videoServiceClose(uint32_t service, int32_t handle);
uint32_t videoServiceDecoderSize(void);
uint32_t videoServiceEncoderSize(void);

/* the service's messages: a codec's request taken, and its reply slot freed */
void videoServiceRequest(uint32_t service, uint32_t request);
void videoServiceRelease(uint32_t service, uint32_t serviceSlot);

void videoBusSync(void);

/* the server threads */
void avcdecThread(VideoServer *server, uint32_t argument2);
void m4vdecThread(VideoServer *server, uint32_t argument2);
void avcencThread(VideoServer *server, uint32_t argument2);

/* the MPEG-4 decoder server's commands */
#define M4VDEC_QUERY_MEMORY		513
#define M4VDEC_INITIALIZE		514
#define M4VDEC_QUERY_SEQUENCE		515
#define M4VDEC_QUERY_CONTEXT		516
#define M4VDEC_INITIALIZE_CONTEXT	517
#define M4VDEC_DECODE			518
#define M4VDEC_DECODE_STOP		519
#define M4VDEC_TERMINATE		520

/* the AVC encoder server's commands */
#define AVCENC_QUERY_MEMORY		769
#define AVCENC_QUERY_MEMORY2		770
#define AVCENC_INITIALIZE		771
#define AVCENC_GET_PARAMETER_SETS	772
#define AVCENC_SET_PARAM		773
#define AVCENC_ENCODE			774
#define AVCENC_ENCODE_STOP		775
#define AVCENC_GET_SEI			776
#define AVCENC_TERMINATE		777

/* the AVC encoder (0x822944-) */
uint32_t avcencQueryMemory(const uint32_t *param);
uint32_t avcencQueryMemory2(const uint32_t *param);
uint32_t avcencQueryMemory3(const uint32_t *param);
int32_t avcencInitialize(uint32_t *handle, const uint32_t *param, uint32_t workAddress,
	uint32_t workSize, uint32_t work2Address, uint32_t work2Size);
int32_t avcencGetParameterSets(uint32_t handle, uint32_t sps, uint32_t pps);
int32_t avcencSetParam(uint32_t handle, const uint32_t *param);
int32_t avcencEncode(uint32_t handle, VideoEncoderFrame *frame, uint32_t outputs,
	uint32_t numOfOutputs);
int32_t avcencEncodeStop(uint32_t handle, VideoEncoderFrame *frame, uint32_t outputs,
	uint32_t numOfOutputs);
int32_t avcencGetSei(uint32_t handle, uint32_t buffer);
int32_t avcencTerminate(uint32_t handle);
uint32_t avcencVersion(void);
void avcencReceive(uint32_t argument, uint32_t address);

/* the AVC encoder's core (0x834de0-) */
uint32_t avcencQueryContextMemory(void);
uint32_t avcencQueryStreamMemory(uint32_t count);
uint32_t avcencQueryFrameMemory(uint32_t numOfRefFrames, uint32_t horizontal,
	uint32_t vertical);
uint32_t avcencQueryFrames(uint32_t numOfRefFrames, uint32_t horizontal, uint32_t vertical);
uint32_t avcencQueryRateMemory(void);

/* the AVC decoder (0x82358a-) */
uint32_t avcdecQueryMemory(const uint32_t *param);
int32_t avcdecInitialize(uint32_t *handle, const uint32_t *param, uint32_t workAddress,
	uint32_t workSize);
int32_t avcdecQuerySequence(uint32_t handle, VideoAu *au, const uint32_t *param, uint32_t pictures);
uint32_t avcdecQueryContext(const uint32_t *param);
int32_t avcdecInitializeContext(uint32_t handle, const uint32_t *param, uint32_t address,
	uint32_t size);
int32_t avcdecDecode(uint32_t handle, VideoAu *au, uint32_t pictures, uint32_t numOfPictures,
	uint32_t *option);
int32_t avcdecDecodeStop(uint32_t handle, VideoAu *au, uint32_t pictures,
	uint32_t numOfPictures, uint32_t *option);
int32_t avcdecTerminate(uint32_t handle);
void avcdecReceive(uint32_t argument, uint32_t address);

/* the AVC decoder's core (0x8256b2-) */
uint32_t avcdecQueryParserMemory(uint32_t maxStreams, uint32_t maxFrames);
uint32_t avcdecQueryDecoderMemory(void);
uint32_t avcdecQueryFrameMemory(uint32_t numOfRefFrames, uint32_t horizontal,
	uint32_t vertical);
uint32_t avcdecQueryFieldMemory(uint32_t numOfRefFrames, uint32_t horizontal,
	uint32_t vertical);

/* the MPEG-4 decoder (0x82461c-) */
uint32_t m4vdecQueryMemory(const uint32_t *param);
int32_t m4vdecInitialize(uint32_t *handle, const uint32_t *param, uint32_t workAddress,
	uint32_t workSize);
int32_t m4vdecQuerySequence(uint32_t handle, VideoAu *au, const uint32_t *param, uint32_t pictures);
uint32_t m4vdecQueryContext(const uint32_t *param);
int32_t m4vdecInitializeContext(uint32_t handle, const uint32_t *param, uint32_t address,
	uint32_t size);
int32_t m4vdecDecode(uint32_t handle, VideoAu *au, uint32_t pictures, uint32_t numOfPictures,
	uint32_t *option);
int32_t m4vdecDecodeStop(uint32_t handle, VideoAu *au, uint32_t pictures,
	uint32_t numOfPictures, uint32_t *option);
int32_t m4vdecTerminate(uint32_t handle);
uint32_t m4vdecVersion(void);
void m4vdecReceive(uint32_t argument, uint32_t address);

/* the MPEG-4 decoder's core: its memory */
uint32_t m4vdecQueryParserMemory(void);
uint32_t m4vdecQueryMacroblockMemory(uint32_t horizontal, uint32_t vertical,
	uint32_t macroblockData);
uint32_t m4vdecQueryFrameMemory(uint32_t horizontal, uint32_t vertical);
uint32_t m4vdecQueryDecoderMemory(void);

/* the video engine, on the other cores (0x824c96-) */
uint32_t videoEngineVersion(void);

