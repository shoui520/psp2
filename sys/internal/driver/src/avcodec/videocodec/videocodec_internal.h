#pragma once

#include <scetypes.h>
#include <kernel/types.h>
#include <sysmem/spinlock.h>
#include <threadmgr/fast_mutex_object.h>
#include <vnz_wrapper/venezia_debug.h>
#include <avcdec.h>
#include <stddef.h>
#include <avcodec/avcodec.h>

/*
 * An MPEG-4 decoder's picture: an SceAvcdecPicture with a shorter stream information,
 * SceAvcdecInfo's fields in their MPEG-4 VOL form.
 */
typedef struct M4vdecInfo {
	SceUInt32 vopTimeIncrementResolution;
	SceUChar8 fixedVopRate;
	SceUChar8 aspectRatioInfo;
	SceUShort16 parWidth;
	SceUShort16 parHeight;
	SceUChar8 colourPrimaries;
	SceUChar8 transferCharacteristics;
	SceUChar8 matrixCoefficients;
	SceUChar8 videoRange;
	SceUChar8 infoFlags;
	SceUChar8 padding;
	SceVideodecTimeStamp outputPts;
} M4vdecInfo;

_Static_assert(sizeof(M4vdecInfo) == 24, "M4V picture information ABI");

typedef struct M4vdecPicture {
	SceSize size;
	SceAvcdecFrame frame;
	M4vdecInfo info;
} M4vdecPicture;

_Static_assert(sizeof(M4vdecPicture) == 92, "M4V picture ABI");

typedef struct M4vdecAu {
	unsigned char timingMetadata[16];
	SceAvcdecBuf es;
} M4vdecAu;

_Static_assert(sizeof(M4vdecAu) == 24, "M4V access unit ABI");
_Static_assert(offsetof(M4vdecAu, es) == 16, "M4V elementary stream ABI");

typedef struct M4vdecArrayPicture {
	unsigned int numOfOutput;
	unsigned int numOfElm;
	struct M4vdecPicture **pPicture;
} M4vdecArrayPicture;

_Static_assert(sizeof(M4vdecArrayPicture) == 12, "M4V picture array ABI");
_Static_assert(offsetof(M4vdecArrayPicture, pPicture) == 8,
	       "M4V picture pointers ABI");

typedef struct VideocodecSeiUserDataEntry {
	unsigned char payloadType;
	unsigned char padding[3];
	SceSize payloadSize;
} VideocodecSeiUserDataEntry;

typedef struct VideocodecSeiClockTimestamp {
	unsigned char clockMetadata[13];
	unsigned char padding[3];
	int timeOffset;
} VideocodecSeiClockTimestamp;

typedef struct VideocodecSeiPictureTiming {
	unsigned char valid;
	unsigned char padding[3];
	unsigned int cpbRemovalDelay;
	unsigned int dpbOutputDelay;
	unsigned char picStruct;
	unsigned char clockTimestampFlag[3];
	VideocodecSeiClockTimestamp clockTimestamp[3];
} VideocodecSeiPictureTiming;

_Static_assert(sizeof(VideocodecSeiClockTimestamp) == 20,
	       "SEI clock timestamp ABI");
_Static_assert(offsetof(VideocodecSeiClockTimestamp, timeOffset) == 16,
	       "SEI clock offset ABI");
_Static_assert(sizeof(VideocodecSeiPictureTiming) == 76,
	       "SEI picture timing ABI");
_Static_assert(offsetof(VideocodecSeiPictureTiming, clockTimestamp) == 16,
	       "SEI picture clocks ABI");

typedef struct VideocodecSeiUserData {
	SceSize size;
	unsigned char numOfUserData;
	unsigned char padding[3];
	SceSize userDataSize;
	VideocodecSeiUserDataEntry userData[15];
} VideocodecSeiUserData;

_Static_assert(sizeof(VideocodecSeiUserDataEntry) == 8, "SEI user data entry ABI");
_Static_assert(sizeof(VideocodecSeiUserData) == 132, "SEI user data ABI");
_Static_assert(offsetof(VideocodecSeiUserData, userData) == 12,
	       "SEI user data entries ABI");

/* Complete resident owner, separate from the following DSP0 codec state. */
typedef struct VideocodecResident {
	int processLocalStorageId;
	SceUID activeProcessId;
	SceUID ownerProcessIds[4];
	void (*notifyProcessChange)(SceUID pid, int active);
	struct ThreadmgrFastMutex allMutex;
} VideocodecResident;

_Static_assert(sizeof(VideocodecResident) == 92, "video codec resident ABI");

/* a range a codec call locks for the hardware: pid 0 locks by mode, else by process */
typedef struct VideocodecLockedRange {
	int mode;
	uintptr_t address;
	SceSize size;
} VideocodecLockedRange;

typedef struct VideocodecLockedRanges {
	SceUID pid;
	int errorBase;
	unsigned char numOfRanges;
	unsigned char maxRanges;
	unsigned char reserved[2];
	VideocodecLockedRange *ranges;
} VideocodecLockedRanges;

_Static_assert(sizeof(VideocodecLockedRange) == 12, "video codec locked range ABI");
_Static_assert(sizeof(VideocodecLockedRanges) == 16, "video codec locked ranges ABI");

/* the encoder's errors: the video decoder's with 0x1000 set */
#define VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM		(-2141054974)	/* 0x80621802 */
#define VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER	(-2141054967)	/* 0x80621809 */

/* the error bases a locked range reports its failures from */
#define VIDEOCODEC_DECODER_ERROR_BASE	(-2141061120)	/* 0x80620000 */
#define VIDEOCODEC_ENCODER_ERROR_BASE	(-2141057024)	/* 0x80621000 */

int videocodecInitLockedRanges(VideocodecLockedRanges *ranges, SceUID pid,
	VideocodecLockedRange *entries, unsigned int maxRanges, int errorBase);
int videocodecUnlockRanges(VideocodecLockedRanges *ranges);
int videocodecUnlockRangesProc(VideocodecLockedRanges *ranges, SceUID pid);

/* videocodecLockRange's flags and cache operations */
#define VIDEOCODEC_LOCK_SCATTERED		1
#define VIDEOCODEC_CACHE_CLEAN			1
#define VIDEOCODEC_CACHE_INVALIDATE		2
#define VIDEOCODEC_CACHE_CLEAN_INVALIDATE	3

int videocodecLockRange(VideocodecLockedRanges *ranges, uintptr_t *pPhysicalAddress,
	uintptr_t address, SceSize size, int mode, int cacheOp, unsigned int flags);
int videocodecConvertM4vdecError(int result);

/*
 * The frame a colour conversion reads or writes: an SceAvcdecFrame, followed, when its
 * pixelType has VIDEOCODEC_FRAME_SEPARATE_PLANES, by the addresses of its separate planes.
 */
typedef struct VideocodecFrame {
	SceAvcdecFrame frame;
	void *pPlane[3];
	unsigned int reserved[3];
} VideocodecFrame;

_Static_assert(sizeof(VideocodecFrame) == 88, "video codec frame ABI");
_Static_assert(offsetof(VideocodecFrame, pPlane) == 64, "video codec frame planes ABI");

/* an SceAvcdecPicture whose frame may have separate planes */
typedef struct VideocodecPicture {
	SceSize size;
	VideocodecFrame frame;
	SceAvcdecInfo info;
} VideocodecPicture;

_Static_assert(sizeof(VideocodecPicture) == 124, "video codec picture ABI");

/* pixelType flags above the pixel format */
#define VIDEOCODEC_FRAME_SEPARATE_PLANES	0x40000	/* the planes are pPlane[0..2] */
#define VIDEOCODEC_FRAME_KEEP_SIZE		0x80000	/* convert to the frame's whole size */
#define VIDEOCODEC_FRAME_WHOLE_FIELDS		0x10000	/* an interlaced source in one pass */

/* the pixel formats a conversion takes besides the SDK's (pixelType & 0xFF) */
#define VIDEOCODEC_PIXEL_BGR565			0x01
#define VIDEOCODEC_PIXEL_BGRA5551		0x02
#define VIDEOCODEC_PIXEL_YUV420_TILED		0x11
#define VIDEOCODEC_PIXEL_YUV420_PACKED_TILED	0x21
#define VIDEOCODEC_PIXEL_PHYSICAL_PACKED	0x41	/* physical planes, pitches in framePitch */
#define VIDEOCODEC_PIXEL_PHYSICAL		0x80	/* a physical picture; pPicture[1]: its fields */

/* videocodecLockRange: a range outside the uncached window is kept coherent by cacheOp */
#define VIDEOCODEC_LOCK_CACHED			0x10

/* the MPEG-4 decoder's and the encoder's errors: the AVC decoder's with 0x900 or 0x1000 set */
#define VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM		(-2141058814)	/* 0x80620902 */
#define VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER	(-2141058807)	/* 0x80620909 */
#define VIDEOCODEC_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE	(-2141058802)	/* 0x8062090E */

#define VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE	(-2141057010)	/* 0x8062100E */

/* the frame the encoder converts its input into: YUV420, tiled */
typedef struct VideocodecEncoderFrame {
	void *pPicture;
	unsigned int framePitch;
	unsigned int frameWidth;
	unsigned int frameHeight;
} VideocodecEncoderFrame;

int videocodecM4vdecCsc(M4vdecPicture *pPictureOut, const M4vdecPicture *pPictureIn);
int videocodecAvcdecCsc(SceAvcdecPicture *pPictureOut, const SceAvcdecPicture *pPictureIn);

/* the IFTU's tiled formats */
#define VIDEOCODEC_IFTU_TILED			0x80000000U
#define VIDEOCODEC_IFTU_PHYSICAL		0x80140000U
int videocodecConvertAvcencError(int result);

/* the hardware memory a codec library runs in: a memory block, or a process's unmap memory */
#define VIDEOCODEC_MEMORY_BLOCK		0
#define VIDEOCODEC_MEMORY_UNMAP		(-1)

typedef struct VideocodecMemory {
	void *base;
	SceSize size;
	int kind;
	SceUID memBlockId;
} VideocodecMemory;

_Static_assert(sizeof(VideocodecMemory) == 16, "video codec memory ABI");

/* an AVC decoder on the Venezia codec engine */
typedef struct VideocodecAvcDecoder {
	SceAvcdecQueryDecoderInfo info;
	unsigned int mode;
	unsigned int handle;
	unsigned char decodeState[1500];
	unsigned int decodeFlags;
	unsigned char decodeStatus[4];
	unsigned char pictureStatus[4];
	unsigned int pictureFlags;
	struct ThreadmgrFastMutex mutex;
	VideocodecLockedRanges lockedRanges;
	VideocodecLockedRange lockedRange[1];
	unsigned int reserved;
} VideocodecAvcDecoder;

_Static_assert(sizeof(VideocodecAvcDecoder) == 1632, "video codec AVC decoder ABI");
_Static_assert(offsetof(VideocodecAvcDecoder, mutex) == 1536, "AVC decoder mutex ABI");

/* a decoder on the DSP0 (AVC or MPEG-4) */
typedef struct VideocodecDsp0Decoder {
	SceAvcdecQueryDecoderInfo info;
	unsigned int mode;
	unsigned int handle;
	unsigned char decodeStatus[4];
	unsigned int decodeFlags;
	unsigned char pictureStatus[8];
	unsigned int pictureFlags;
	struct ThreadmgrFastMutex mutex;
	VideocodecLockedRanges lockedRanges;
	VideocodecLockedRange lockedRange[1];
	unsigned int reserved2;
} VideocodecDsp0Decoder;

_Static_assert(sizeof(VideocodecDsp0Decoder) == 136, "video codec DSP0 decoder ABI");
_Static_assert(offsetof(VideocodecDsp0Decoder, mutex) == 40, "DSP0 decoder mutex ABI");

/* an AVC encoder on the DSP0 */
typedef struct VideocodecEncoder {
	unsigned int handle;
	unsigned char encodeState[164];
	unsigned int encodeFlags;
	unsigned char encodeStatus[8];
	unsigned int pictureFlags;
	unsigned char parameters[112];
	struct ThreadmgrFastMutex mutex;
	VideocodecLockedRanges lockedRanges;
	VideocodecLockedRange lockedRange[1];
	unsigned int reserved;
} VideocodecEncoder;

_Static_assert(sizeof(VideocodecEncoder) == 392, "video codec encoder ABI");
_Static_assert(offsetof(VideocodecEncoder, mutex) == 296, "encoder mutex ABI");

/* SceVideodecQueryInitInfoHwAvcdec without its size: what a library was initialized for */
typedef struct VideocodecLibraryInfo {
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfRefFrames;
	unsigned int numOfStreams;
} VideocodecLibraryInfo;

/* what _sceVideodecQueryInstanceNongameapp reports of a process's AVC decoder library */
typedef struct VideocodecInstanceInfo {
	SceSize size;
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfRefFrames;
	unsigned int numOfStreams;
	unsigned int memoryType;
	unsigned int numOfInstances;
} VideocodecInstanceInfo;

_Static_assert(sizeof(VideocodecInstanceInfo) == 28, "video codec instance info ABI");

/* VideocodecInstanceInfo.memoryType: the library's memory block, or the process's unmap memory */
#define VIDEOCODEC_INSTANCE_MEMORY_BLOCK	1
#define VIDEOCODEC_INSTANCE_MEMORY_UNMAP	3

int SceAvcodecForDriver_F9E271DE(unsigned int codecType, VideocodecInstanceInfo *pInfo);

/* a process's video codecs: _GetVideocodecGlobal's process-local storage */
typedef struct VideocodecGlobal {
	struct ThreadmgrFastMutex *pAllMutex;
	unsigned int reserved;
	int avcdecInitCount;
	VideocodecLibraryInfo avcdecInfo;
	VideocodecAvcDecoder avcDecoders[4];
	VideocodecMemory avcdecMemory;
	unsigned int avcdecFlags;
	unsigned int reserved2;
	int dsp0InitCount;
	unsigned int dsp0Flags;
	VideocodecLibraryInfo dsp0DecoderInfo;
	VideocodecLibraryInfo dsp0EncoderInfo;
	unsigned int dsp0Library;
	VideocodecDsp0Decoder dsp0Decoders[4];
	VideocodecEncoder encoders[1];
	VideocodecMemory dsp0Memory;
	unsigned int dsp0MemoryFlags;
	unsigned int dsp0Config;
} VideocodecGlobal;

_Static_assert(sizeof(VideocodecGlobal) == 7584, "video codec process storage ABI");
_Static_assert(offsetof(VideocodecGlobal, avcdecMemory) == 6556, "AVC decoder memory ABI");
_Static_assert(offsetof(VideocodecGlobal, dsp0Decoders) == 6624, "DSP0 decoders ABI");
_Static_assert(offsetof(VideocodecGlobal, encoders) == 7168, "encoders ABI");

/* a physically contiguous block a codec library allocates itself */
typedef struct VideocodecMemBlock {
	int kind;
	SceUID memBlockId;
	void *base;
	SceSize size;
} VideocodecMemBlock;

_Static_assert(sizeof(VideocodecMemBlock) == 16, "video codec memory block ABI");

int videocodecFreeMemBlock(VideocodecMemBlock *pMemBlock);
int videocodecAllocMemBlock(const char *name, SceKernelMemBlockType type, SceSize alignment,
	SceSize size, VideocodecMemBlock *pMemBlock);

/* the slots of each kind a library holds, or has locked or initialized so far */
typedef struct VideocodecSlotCounts {
	unsigned int numOfAvcDecoders;
	unsigned int numOfDsp0Decoders;
	unsigned int numOfEncoders;
} VideocodecSlotCounts;

/* Complete host/RPC allocation; opaque spans contain live coprocessor data. */
typedef struct Dsp0VideocodecLibraryCommand {
	uintptr_t contextAddress;
	unsigned int libraryToken;
	unsigned char coprocessorHeader[24];
	int numOfDecoders;
	int numOfEncoders;
	SceSize commandSize;
	unsigned short horizontal;
	unsigned char coprocessorConfiguration[2];
	unsigned int coprocessorQueryData[2];
	uintptr_t workAddress;
	SceSize workSize;
	unsigned char coprocessorState[192];
} Dsp0VideocodecLibraryCommand;

_Static_assert(sizeof(Dsp0VideocodecLibraryCommand) == 256,
	       "DSP0 library command ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, numOfDecoders) == 32,
	       "DSP0 decoder count ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, horizontal) == 44,
	       "DSP0 library configuration ABI");
_Static_assert(offsetof(Dsp0VideocodecLibraryCommand, workAddress) == 56,
	       "DSP0 library work memory ABI");

/*
 * One of a decoder's two access-unit slots. The host copies the stream into the
 * slot's buffer; timestamps go in as 64-bit values.
 */
typedef struct Dsp0VideocodecAccessUnit {
	uintptr_t bufferAddress;
	SceSize bufferSize;
	uintptr_t esAddress;
	SceSize esSize;
	unsigned char valid;
	unsigned char coprocessorFlags[7];
	SceUInt64 dts;
	SceUInt64 pts;
	int numOfOutput;
	unsigned int coprocessorStatus;
} Dsp0VideocodecAccessUnit;

_Static_assert(sizeof(Dsp0VideocodecAccessUnit) == 48, "DSP0 access unit ABI");

/* the VUI of the stream an AVC picture belongs to, in the order of H.264's syntax */
typedef struct Dsp0VideocodecVui {
	unsigned char aspectRatioInfoPresentFlag;
	unsigned char aspectRatioIdc;
	unsigned char padding0[2];
	unsigned int sarWidth;
	unsigned int sarHeight;
	unsigned char overscanInfoPresentFlag;
	unsigned char overscanAppropriateFlag;
	unsigned char videoSignalTypePresentFlag;
	unsigned char videoFormat;
	unsigned char videoFullRangeFlag;
	unsigned char colourDescriptionPresentFlag;
	unsigned char colourPrimaries;
	unsigned char transferCharacteristics;
	unsigned char matrixCoefficients;
	unsigned char chromaLocInfoPresentFlag;
	unsigned char timingInfoPresentFlag;
	unsigned char padding1;
	unsigned int chromaSampleLocTypeTopField;
	unsigned int chromaSampleLocTypeBottomField;
	unsigned int numUnitsInTick;
	unsigned int timeScale;
	unsigned char fixedFrameRateFlag;
	unsigned char nalHrdParametersPresentFlag;
	unsigned char vclHrdParametersPresentFlag;
	unsigned char lowDelayHrdFlag;
	unsigned char picStructPresentFlag;
} Dsp0VideocodecVui;

_Static_assert(offsetof(Dsp0VideocodecVui, timingInfoPresentFlag) == 22, "DSP0 VUI timing ABI");
_Static_assert(offsetof(Dsp0VideocodecVui, numUnitsInTick) == 32, "DSP0 VUI ticks ABI");
_Static_assert(offsetof(Dsp0VideocodecVui, picStructPresentFlag) == 44, "DSP0 VUI pic_struct ABI");

/* the SEI messages of an AVC picture */
#define DSP0_VIDEOCODEC_SEI_PICTURE_TIMING	0x2

typedef struct Dsp0VideocodecSei {
	unsigned char flags;			/* DSP0_VIDEOCODEC_SEI_* present */
	unsigned char coprocessorSei[19];
	VideocodecSeiPictureTiming pictureTiming;
} Dsp0VideocodecSei;

_Static_assert(offsetof(Dsp0VideocodecSei, pictureTiming) == 20, "DSP0 SEI timing ABI");

/*
 * The picture the coprocessor reports: its planes as physical addresses, its size and
 * pitches, and per codec its crop, stream information and time stamp. dsp0's decode turns
 * the AVC information pointers into virtual addresses.
 */
typedef struct Dsp0VideocodecPicture {
	uintptr_t lumaAddress;
	unsigned char coprocessorInfo0[4];
	uintptr_t chromaAddress;
	unsigned char coprocessorInfo1[4];
	unsigned short frameWidth;
	unsigned short frameHeight;
	unsigned short lumaPitch;
	unsigned short chromaPitch;
	union {
		struct {
			unsigned char coprocessorInfo[19];
			unsigned char frameCroppingFlag;
			unsigned short frameCropLeftOffset;
			unsigned short frameCropRightOffset;
			unsigned short frameCropTopOffset;
			unsigned short frameCropBottomOffset;
			const Dsp0VideocodecVui *pVui;
			const Dsp0VideocodecSei *pSei;
			const void *pUserData;
			unsigned char coprocessorState[8];
			SceUInt64 pts;
		} avc;
		struct {
			unsigned char transferCharacteristics;
			unsigned char matrixCoefficients;
			unsigned char videoRange;
			unsigned char infoFlags;
			unsigned int vopTimeIncrementResolution;
			unsigned char fixedVopRate;
			unsigned char aspectRatioInfo;
			unsigned char parWidth;
			unsigned char parHeight;
			unsigned char colourPrimaries;
			unsigned char coprocessorInfo[11];
			SceUInt64 pts;
		} m4v;
	};
	unsigned char coprocessorState[176];
} Dsp0VideocodecPicture;

_Static_assert(sizeof(Dsp0VideocodecPicture) == 256, "DSP0 picture ABI");
_Static_assert(offsetof(Dsp0VideocodecPicture, avc.frameCroppingFlag) == 43, "DSP0 crop ABI");
_Static_assert(offsetof(Dsp0VideocodecPicture, avc.pVui) == 52, "DSP0 VUI pointer ABI");
_Static_assert(offsetof(Dsp0VideocodecPicture, avc.pts) == 72, "DSP0 pts ABI");
_Static_assert(offsetof(Dsp0VideocodecPicture, m4v.vopTimeIncrementResolution) == 28,
	       "DSP0 M4V time ABI");
_Static_assert(offsetof(Dsp0VideocodecPicture, m4v.pts) == 48, "DSP0 M4V pts ABI");

/* Complete host/RPC decoder command; opaque spans are coprocessor-only. */
typedef struct Dsp0VideocodecDecoderCommand {
	uintptr_t contextAddress;
	unsigned int coprocessorHandle;
	unsigned int libraryToken;
	unsigned int coprocessorSetup[2];
	unsigned short horizontal;
	unsigned short vertical;
	unsigned int numOfRefFrames;
	unsigned char coprocessorModes[3];
	unsigned char decodeMode;
	unsigned char coprocessorHeader[16];
	unsigned int auIndex;
	unsigned int coprocessorAuState;
	Dsp0VideocodecAccessUnit au[2];
	Dsp0VideocodecPicture *pPicture;
	unsigned int numOfPicture;
	uintptr_t workAddress;
	SceSize workSize;
	uintptr_t frameBufAddress;
	SceSize frameBufSize;
	unsigned char coprocessorState[80];
} Dsp0VideocodecDecoderCommand;

_Static_assert(sizeof(Dsp0VideocodecDecoderCommand) == 256, "DSP0 decoder command ABI");
_Static_assert(offsetof(Dsp0VideocodecDecoderCommand, au) == 56, "DSP0 access units ABI");
_Static_assert(offsetof(Dsp0VideocodecDecoderCommand, pPicture) == 152, "DSP0 picture ABI");
_Static_assert(offsetof(Dsp0VideocodecDecoderCommand, frameBufAddress) == 168,
	       "DSP0 frame buffer ABI");

/* The M4V decoder's host/RPC command; its access units sit 8 bytes before the AVC one's. */
typedef struct Dsp0VideocodecM4vDecoderCommand {
	uintptr_t contextAddress;
	unsigned int coprocessorHandle;
	unsigned int libraryToken;
	unsigned short horizontal;
	unsigned short vertical;
	SceSize maxEsSize;
	unsigned char coprocessorModes[20];
	unsigned int auIndex;
	unsigned char reserved[4];
	Dsp0VideocodecAccessUnit au[2];
	Dsp0VideocodecPicture *pPicture;
	unsigned int numOfPicture;
	uintptr_t workAddress;
	SceSize workSize;
	uintptr_t frameBufAddress;
	SceSize frameBufSize;
	unsigned char coprocessorState[88];
} Dsp0VideocodecM4vDecoderCommand;

_Static_assert(sizeof(Dsp0VideocodecM4vDecoderCommand) == 256, "DSP0 M4V decoder command ABI");
_Static_assert(offsetof(Dsp0VideocodecM4vDecoderCommand, auIndex) == 40, "DSP0 M4V AU index ABI");
_Static_assert(offsetof(Dsp0VideocodecM4vDecoderCommand, au) == 48, "DSP0 M4V AU ABI");
_Static_assert(offsetof(Dsp0VideocodecM4vDecoderCommand, pPicture) == 144,
	       "DSP0 M4V picture ABI");
_Static_assert(offsetof(Dsp0VideocodecM4vDecoderCommand, frameBufAddress) == 160,
	       "DSP0 M4V frame buffer ABI");

/* what an encode reports: the stream it wrote and how many NAL units */
typedef struct Dsp0VideocodecEncodeResult {
	uintptr_t streamAddress;
	SceSize streamSize;
	unsigned int numOfNal;
	unsigned char coprocessorFlags[4];
	unsigned int timeStamp[2];
} Dsp0VideocodecEncodeResult;

typedef struct Dsp0VideocodecNalUnit {
	uintptr_t address;
	SceSize size;
	unsigned char type;
	unsigned char referencePriority;
	unsigned char padding[2];
} Dsp0VideocodecNalUnit;

typedef struct Dsp0VideocodecInputPicture {
	uintptr_t address;
	unsigned char coprocessorInfo[12];
	unsigned short alignedHorizontal;
	unsigned short alignedVertical;
	unsigned short lumaPitch;
	unsigned short chromaPitch;
	unsigned char valid;
	unsigned char coprocessorState[3];
	unsigned int numOfOutput;
} Dsp0VideocodecInputPicture;

typedef struct AvcencEncoderParameter {
	unsigned int bitRate[2];
	SceSize cpbSize;
	unsigned int gopLength;
	unsigned int numUnitsInTick;
	unsigned int timeScale;
} AvcencEncoderParameter;

_Static_assert(sizeof(AvcencEncoderParameter) == 24,
	       "AVC encoder parameter ABI");

/* what an AVC encoder is created for (64 bytes; checked by dsp0VideocodecCheckEncoderInitParam) */
typedef struct Dsp0VideocodecEncoderInitParam {
	unsigned char profile;			/* 'B' or 'N' */
	unsigned char level;
	unsigned char userOption[2];
	unsigned int pitch;
	unsigned int alignedHorizontal;
	unsigned int alignedVertical;
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfStreams;
	unsigned int rateControl[4];
	unsigned char codingOption[3];
	unsigned char padding0;
	unsigned char sliceOption;
	unsigned char padding1[3];
	unsigned short sliceParam[2];
	unsigned char vuiPresent;
	unsigned char padding2;
	unsigned char vui[6];
} Dsp0VideocodecEncoderInitParam;

_Static_assert(sizeof(Dsp0VideocodecEncoderInitParam) == 64, "DSP0 encoder init param ABI");

/* Complete host/RPC encoder command; opaque spans are coprocessor-only. */
typedef struct Dsp0VideocodecEncoderCommand {
	uintptr_t contextAddress;
	unsigned int coprocessorHandle;
	unsigned int libraryToken;
	unsigned char profile;			/* 'B' baseline, 'M' main */
	unsigned char level;
	unsigned short horizontal;
	unsigned short vertical;
	unsigned char fixedMode0;		/* 0 */
	unsigned char userOption0;
	unsigned char fixedMode1;		/* 0 */
	unsigned char fixedMode2;		/* 1 */
	unsigned char mainProfile;
	unsigned char userOption1;
	unsigned int rateControl[4];
	unsigned char codingOption[3];
	unsigned char padding0;
	unsigned int coprocessorCounters[3];
	unsigned char sliceOption;
	unsigned char padding1[3];
	unsigned short sliceParam[2];
	unsigned char vuiPresent;
	unsigned char padding2[3];
	unsigned char vui[6];
	unsigned char vuiReserved[2];
	uintptr_t streamAddress;
	SceSize streamSize;		/* stream memory, when queried */
	uintptr_t bufferAddress;
	SceSize bufferSize;		/* frame memory, when queried */
	SceSize workSize;		/* work memory, when queried */
	Dsp0VideocodecNalUnit headers[2];	/* SPS and PPS */
	AvcencEncoderParameter parameter;
	Dsp0VideocodecEncodeResult *pEncodeResult;
	unsigned int numOfPicture;
	Dsp0VideocodecNalUnit nal[12];
	uintptr_t coprocessorBufferAddress[2];	/* physical */
	unsigned int pictureIndex;
	Dsp0VideocodecInputPicture picture[2];
	unsigned char coprocessorState[140];
} Dsp0VideocodecEncoderCommand;

_Static_assert(sizeof(Dsp0VideocodecEncoderCommand) == 512, "DSP0 encoder command ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoderCommand, vui) == 68, "DSP0 encoder VUI ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoderCommand, streamAddress) == 76,
	       "DSP0 encoder stream ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoderCommand, parameter) == 120,
	       "DSP0 encoder rate control ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoderCommand, pEncodeResult) == 144,
	       "DSP0 encode result ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoderCommand, picture) == 308,
	       "DSP0 encoder pictures ABI");

/* the M4V decoder's counterpart of SceAvcdecQueryDecoderInfo */
typedef struct Dsp0VideocodecM4vDecoderInfo {
	unsigned int horizontal;
	unsigned int vertical;
	SceSize maxEsSize;
} Dsp0VideocodecM4vDecoderInfo;

/* what a DSP0 decoder is created with */
typedef struct Dsp0VideocodecDecoderParam {
	void *pFrameBuf;
	SceSize frameBufSize;
	const void *pInfo;	/* VideocodecDsp0Decoder or Dsp0VideocodecM4vDecoderInfo */
} Dsp0VideocodecDecoderParam;

/*
 * Where a DSP0 decode puts its pictures: up to one element, each a pointer to
 * the caller's picture pointer.
 */
typedef struct Dsp0VideocodecArrayPicture {
	unsigned int numOfElm;
	Dsp0VideocodecPicture ***pPicture;
} Dsp0VideocodecArrayPicture;

typedef struct Dsp0VideocodecAuBuffer {
	uintptr_t address;
	SceSize size;
} Dsp0VideocodecAuBuffer;

/* A decoder slot; its buffers are laid out by dsp0VideocodecInitLibrary. */
typedef struct Dsp0VideocodecDecoder {
	int state;				/* 0 free, 256 AVC, 512 M4V */
	void *pCommand;				/* 256 bytes */
	uintptr_t contextAddress;		/* 256 bytes, physical */
	Dsp0VideocodecPicture *pPicture;
	Dsp0VideocodecAuBuffer au[2];
	uintptr_t workAddress;			/* physical */
	SceSize workSize;
	struct ThreadmgrFastMutex mutex;
	struct ThreadmgrFastMutex *pMutex;
	unsigned int auStatus;
	int status;				/* 2 while it is created */
} Dsp0VideocodecDecoder;

_Static_assert(sizeof(Dsp0VideocodecDecoder) == 116, "DSP0 decoder slot ABI");
_Static_assert(offsetof(Dsp0VideocodecDecoder, mutex) == 40, "DSP0 decoder mutex ABI");

typedef struct Dsp0VideocodecEncoderPicture {
	uintptr_t address;
	SceSize size;
} Dsp0VideocodecEncoderPicture;

/* An encoder slot; its buffers are laid out by dsp0VideocodecInitLibrary. */
typedef struct Dsp0VideocodecEncoder {
	int state;				/* 0 free */
	Dsp0VideocodecEncoderCommand *pCommand;
	uintptr_t contextAddress;		/* 256 bytes, physical */
	uintptr_t coprocessorBufferAddress[2];	/* 256 bytes each, physical */
	Dsp0VideocodecEncodeResult *pEncodeResult;
	Dsp0VideocodecEncoderPicture picture[3];
	struct ThreadmgrFastMutex mutex;
	struct ThreadmgrFastMutex *pMutex;
	unsigned int pictureStatus;
	Dsp0VideocodecEncoderInitParam param;
	int status;				/* 2 while it is created */
} Dsp0VideocodecEncoder;

_Static_assert(sizeof(Dsp0VideocodecEncoder) == 188, "DSP0 encoder slot ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoder, mutex) == 48, "DSP0 encoder mutex ABI");
_Static_assert(offsetof(Dsp0VideocodecEncoder, param) == 120, "DSP0 encoder parameters ABI");

/* what a DSP0 encoder is created with; it returns the stream's SPS and PPS */
typedef struct Dsp0VideocodecEncoderParam {
	uintptr_t streamAddress;
	SceSize streamSize;
	uintptr_t bufferAddress;
	SceSize bufferSize;
	const Dsp0VideocodecEncoderInitParam *pInitParam;
	Dsp0VideocodecNalUnit headers[2];
	AvcencEncoderParameter parameter;
} Dsp0VideocodecEncoderParam;

_Static_assert(sizeof(Dsp0VideocodecEncoderParam) == 68, "DSP0 encoder create ABI");

/* a picture handed to a DSP0 encode; mode 0 or a NULL picture flushes */
typedef struct Dsp0VideocodecEncodeFrame {
	const void *pPicture;
	unsigned int pitch;
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int mode;			/* 1, 32, 64 or 128 */
	AvcencEncoderParameter parameter;
} Dsp0VideocodecEncodeFrame;

/* what a DSP0 encode returns */
typedef struct Dsp0VideocodecEncodeAu {
	SceVideodecTimeStamp pts;
	uintptr_t streamAddress;
	SceSize streamSize;
	unsigned char coprocessorFlags[3];
	unsigned char numOfNal;
	Dsp0VideocodecNalUnit nal[12];
} Dsp0VideocodecEncodeAu;

_Static_assert(sizeof(Dsp0VideocodecEncodeFrame) == 44, "DSP0 encode frame ABI");
_Static_assert(sizeof(Dsp0VideocodecEncodeAu) == 164, "DSP0 encode AU ABI");

typedef struct Dsp0VideocodecInitParam {
	unsigned int horizontal;
	unsigned int vertical;
	unsigned int numOfRefFrames;
	unsigned int numOfStreams;
	unsigned int numOfEncoders;
} Dsp0VideocodecInitParam;

_Static_assert(sizeof(Dsp0VideocodecInitParam) == 20, "DSP0 initialization ABI");

typedef struct Dsp0VideocodecGlobal {
	Dsp0VideocodecInitParam param;		/* what the library was initialized for */
	void *workMemory;
	struct Dsp0VideocodecLibraryCommand *library;
	struct ThreadmgrFastMutex mutex;
	struct ThreadmgrFastMutex *pMutex;
	int initialized;
	Dsp0VideocodecDecoder *decoders;
	Dsp0VideocodecEncoder *encoders;
	SceKernelSpinlock statusLock;
	int status;
} Dsp0VideocodecGlobal;

_Static_assert(sizeof(Dsp0VideocodecGlobal) == 116, "DSP0 process storage ABI");

/* the memory a DSP0 library is built in, and what for */
typedef struct Dsp0VideocodecLibraryMemory {
	void *base;
	SceSize size;
	const Dsp0VideocodecInitParam *param;
} Dsp0VideocodecLibraryMemory;

int dsp0VideocodecInitLibrary(unsigned int *pLibrary, const Dsp0VideocodecLibraryMemory *pMemory);

typedef struct AvcencNalUnit {
	void *data;
	SceSize size;
	unsigned char type;
	unsigned char referencePriority;
	unsigned char padding[2];
} AvcencNalUnit;

typedef struct AvcencCscFrame {
	SceSize size;
	unsigned int pixelType;
	unsigned int framePitch;
	unsigned int frameWidth;
	unsigned int frameHeight;
	SceAvcdecFrameOption opt;
	void *pPicture[2];
	unsigned int encodeMode;
} AvcencCscFrame;

_Static_assert(sizeof(AvcencCscFrame) == 48, "AVC encoder CSC frame ABI");
_Static_assert(offsetof(AvcencCscFrame, pPicture) == 36,
	       "AVC encoder CSC picture addresses ABI");

int SceAvcodecForDriver_4F11F915(AvcencCscFrame *pDst,
			       const AvcencCscFrame *pSrc);	/* vitasdk: ksceAvcencCsc */
int videocodecAvcencInputCsc(VideocodecEncoderFrame *pDst, const AvcencCscFrame *pSrc,
	unsigned int flags);
int videocodecAvcencCsc(AvcencCscFrame *pDst, const AvcencCscFrame *pSrc);
int videocodecDecoderCsc(SceAvcdecFrame *pFrameOut, const SceAvcdecFrame *pFrameIn,
		unsigned int cscCoefficient, const SceAvcdecArrayPicture *pArrayPicture);
int videocodecAvcdecPicture(SceAvcdecPictureForInterlaced *pPicture, const VeneziaDebugSnapshotOutput *pOutput,
		const SceAvcdecArrayPicture *pArrayPicture, int interlaced);
int videocodecDsp0AvcdecPicture(SceAvcdecPictureForInterlaced *pPicture, Dsp0VideocodecPicture *const *ppOutput,
		const SceAvcdecArrayPicture *pArrayPicture);
int videocodecDsp0M4vdecPicture(M4vdecPicture *pPicture, Dsp0VideocodecPicture *const *ppOutput,
		const M4vdecArrayPicture *pArrayPicture);
int videocodecAvcdecCscInternal(VideocodecPicture *pPictureOut, const VideocodecPicture *pPictureIn);

typedef struct AvcencControl {
	unsigned int handle;
	void *workMemory;
	SceSize workMemorySize;
	void *frameMemory;
	SceSize frameMemorySize;
	void *headerMemory;
	SceSize headerMemorySize;
	AvcencNalUnit headers[2];
} AvcencControl;

struct AvcencAu;

typedef struct AvcencArrayAu {
	unsigned int numOfOutput;
	unsigned int numOfElm;
	struct AvcencAu **pAu;
} AvcencArrayAu;

_Static_assert(sizeof(AvcencArrayAu) == 12, "AVC encoder output array ABI");
_Static_assert(offsetof(AvcencArrayAu, pAu) == 8,
	       "AVC encoder output addresses ABI");

int SceAvcodecForDriver_DE515BFF(const AvcencControl *pCtrl,
		       AvcencArrayAu *pArrayAu);	/* vitasdk: ksceAvcencEncodeStop */
/* vitasdk: ksceAvcencEncode */
int SceAvcodecForDriver_A0E63278(const AvcencControl *pCtrl,
		    const AvcencCscFrame *pFrame, AvcencArrayAu *pArrayAu);

_Static_assert(sizeof(AvcencNalUnit) == 12, "AVC encoder NAL descriptor ABI");
_Static_assert(sizeof(AvcencControl) == 52, "AVC encoder control ABI");
_Static_assert(offsetof(AvcencControl, headers) == 28,
	       "AVC encoder header outputs ABI");

int videocodecTermLibraryForProcess(unsigned int codecType, unsigned int flags,
				  SceUID pid);
void *_GetVideocodecGlobal(SceUID pid);
int videocodecConvertValidationError(int result);
int videocodecConvertCodecEngineError(int result);
int videocodecConvertDecoderError(int result);
int SceAvcodecForDriver_27211864(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_D9FCDF3E(SceAvcdecCtrl *pCtrl, SceUID cbid);
int SceAvcodecForDriver_13C530E4(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_E4F668C5(SceAvcdecCtrl *pCtrl, SceUID cbid);
int SceAvcodecForDriver_F714DB9A(SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_D7CD7F82(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout);
int SceAvcodecForDriver_D2ACCE22(SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_78BA8E98(SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_6E250A5F(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture, const SceUInt *timeout);
int SceAvcodecForDriver_663B3195(unsigned int codecType,
	SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int videocodecCreateDecoder(unsigned int codecType,
	SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int videocodecValidatePublicDecoderInfo(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int videocodecValidateDecoderInfo(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_D5A34236(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C518A109(SceAvcdecCtrl *pCtrl, int trickMode);
int SceAvcodecForDriver_FACCD284(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_22D7B653(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_4AE6B437(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_4DD3826D(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_95E8AFCA(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_6DC39EDC(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_C93DA0C7(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_BC55D10F(unsigned int codecType, SceVideodecCtrl *pCtrl,
	const SceVideodecQueryInitInfo *pInitInfo);
int SceAvcodecForDriver_427914D5(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiUserData *pUserData, SceAvcdecBuf *pBuf);
int SceAvcodecForDriver_BC244D21(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiPictureTiming *pPictureTiming);
int SceAvcodecForDriver_77529FD2(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_F67F93DC(const SceAvcdecCtrl *pCtrl,
	M4vdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_3F146942(const SceAvcdecCtrl *pCtrl,
	M4vdecArrayPicture *pArrayPicture, M4vdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_2C7A9841(const SceAvcdecCtrl *pCtrl,
	const M4vdecAu *pAu, M4vdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_823E187E(const SceAvcdecCtrl *pCtrl,
	const M4vdecAu *pAu, M4vdecArrayPicture *pArrayPicture,
	M4vdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_91E94B76(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_FD805BF1(const SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_F8BCBE20(SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout);
int SceAvcodecForDriver_E355CAAE(const SceAvcdecCtrl *pCtrl,
	SceAvcdecArrayPicture *pArrayPicture, SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_EAEB16D5(const SceAvcdecCtrl *pCtrl,
	const SceAvcdecAu *pAu, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_536C8296(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_86B1ACC5(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo,
	const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_32DEE3C5(unsigned int codecType,
	const SceVideodecQueryInitInfo *pInitInfo,
	const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo);
int SceAvcodecForDriver_301CCA25(unsigned int codecType, SceAvcdecCtrl *pCtrl,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_01387A93(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery,
		SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C114C499(unsigned int codecType,
	const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_C9EF7057(SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, const SceUInt *timeout);
int SceAvcodecForDriver_90F394E6(SceAvcdecCtrl *pCtrl, SceUID cbid,
			       unsigned int eventMask);
int SceAvcodecForDriver_E7E14DE4(SceAvcdecCtrl *pCtrl, SceUID cbid,
			       unsigned int eventMask);
int SceAvcodecForDriver_A2085B6B(unsigned int codecType, unsigned int size);
/* vitasdk: ksceAvcencSetAvailablePreset */
int SceAvcodecForDriver_7DC13C29(unsigned int codecType, unsigned int preset);
int SceAvcodecForDriver_2895CB6D(AvcencControl *pCtrl);	/* vitasdk: ksceAvcencDeleteEncoder */
int SceAvcodecForDriver_229AB2CF(const AvcencControl *pCtrl);	/* vitasdk: ksceAvcencEncodeFlush */
/* vitasdk: ksceAvcencSetEncoderParameter */
int SceAvcodecForDriver_E0232BFE(const AvcencControl *pCtrl,
			       const AvcencEncoderParameter *pParameter);
int SceAvcodecForDriver_1A584DB7(const AvcencControl *pCtrl, unsigned int index,
				AvcencNalUnit *pNalUnit);	/* vitasdk: ksceAvcencGetNalUnit */

int SceAvcodecForDriver_20892154(const SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture);
int SceAvcodecForDriver_0CC84413(const SceAvcdecCtrl *pCtrl,
	const VeneziaVipVideoInput *pAu, SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_73401748(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_866B49BF(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_43985AA8(unsigned int codecType, unsigned int flags);
int SceAvcodecForDriver_12DAE04A(unsigned int codecType, unsigned int flags);
int SceAvcodecForDriver_20C83C00(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_3CB7661A(unsigned int codecType, unsigned int mode);
int SceAvcodecForDriver_C8366BB8(unsigned int codecType, unsigned int flag);
int SceAvcodecForDriver_7E8CC4E5(unsigned int codecType, SceAvcdecCtrl *pCtrl,
			       const SceAvcdecQueryDecoderInfo *pDecoderInfo);
int SceAvcodecForDriver_EA07A3B7(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_43A9CD35(SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_48DECA56(const SceAvcdecCtrl *pCtrl,
			       const SceAvcdecAu *pAu,
			       SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_DFC0AAC6(const SceAvcdecCtrl *pCtrl,
			       SceAvcdecArrayPicture *pArrayPicture);
int SceAvcodecForDriver_D46B8463(const SceAvcdecCtrl *pCtrl);
int SceAvcodecForDriver_16B501C9(SceAvcdecPicture *pDst,
			       const SceAvcdecPicture *pSrc);

/* videocodec/ce_dsp0_videocodec.c */
int initializeDsp0Videocodec(void);
int _sceDsp0VideocodecProcessCreateEntry(SceUID pid, SceUInt32 type);
int _sceDsp0VideocodecProcessExitEntry(SceUID pid, SceUInt32 type);
int dsp0VideocodecIsProcessActive(SceUID pid);
int dsp0VideocodecIsProcessSuspended(SceUID pid);
int dsp0VideocodecProcessResume(SceUID pid);

/* videocodec/videocodec_init.c */
int initializeVideocodec(void);
int videocodecProcessSuspend(SceUID pid);
int videocodecProcessResume(SceUID pid);
