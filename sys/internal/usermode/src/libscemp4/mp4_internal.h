#ifndef MP4_INTERNAL_H
#define MP4_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <sceavplayer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SceClibMspace;

SceClibMspace sceClibMspaceCreate(void *memblock, SceSize size);
void sceClibMspaceDestroy(SceClibMspace mspace);
void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
void sceClibMspaceFree(SceClibMspace mspace, void *ptr);

void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
void *sceClibMemset(void *dst, int ch, SceSize len);
int sceClibMemcmp(const void *lhs, const void *rhs, SceSize len);
int sceClibStrcmp(const char *lhs, const char *rhs);
int sceClibStrncasecmp(const char *lhs, const char *rhs, SceSize len);
char *sceClibStrrchr(const char *src, int ch);
SceSize sceClibStrnlen(const char *src, SceSize maxLen);

int sceKernelGetProcessTitleId(char *titleId, SceSize size);
int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

#ifdef __cplusplus
}
#endif

/* Internal error codes */
enum Mp4DemuxError {
	MP4_OK                     = 0,
	MP4_ERR_NO_MEMORY          = (int)0xF000FF01,
	MP4_ERR_THREAD_FAILED      = (int)0xF001FF01,
	MP4_ERR_OPEN_FAILED        = (int)0xF002FF01,
	MP4_ERR_BUFFER_UNDERRUN    = (int)0xF002FF03,
	MP4_ERR_STREAM_STOPPED     = (int)0xF002FFFF,
	MP4_ERR_EOF                = (int)0xF003FF01,
	MP4_ERR_INVALID_PARAM      = (int)0xF0040001,
	MP4_ERR_GENERIC            = (int)0xF00400FF,
	MP4_ERR_MISSING_ATOM       = (int)0xF005FF01,
	MP4_ERR_SEEK_OUT_OF_RANGE  = (int)0xF005FFFF,
	MP4_ERR_END_OF_FRAGMENT    = (int)0xF006FF01,
	MP4_ERR_END_OF_STREAM      = (int)0xF007FF01
};

#define MP4_FOURCC(ch0, ch1, ch2, ch3) \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
	 ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))

enum Mp4FourCC {
	FOURCC_MOOV = MP4_FOURCC('m', 'o', 'o', 'v'),
	FOURCC_MDAT = MP4_FOURCC('m', 'd', 'a', 't'),
	FOURCC_MVHD = MP4_FOURCC('m', 'v', 'h', 'd'),
	FOURCC_TRAK = MP4_FOURCC('t', 'r', 'a', 'k'),
	FOURCC_TKHD = MP4_FOURCC('t', 'k', 'h', 'd'),
	FOURCC_EDTS = MP4_FOURCC('e', 'd', 't', 's'),
	FOURCC_ELST = MP4_FOURCC('e', 'l', 's', 't'),
	FOURCC_MDIA = MP4_FOURCC('m', 'd', 'i', 'a'),
	FOURCC_MDHD = MP4_FOURCC('m', 'd', 'h', 'd'),
	FOURCC_MINF = MP4_FOURCC('m', 'i', 'n', 'f'),
	FOURCC_STBL = MP4_FOURCC('s', 't', 'b', 'l'),
	FOURCC_STSD = MP4_FOURCC('s', 't', 's', 'd'),
	FOURCC_STTS = MP4_FOURCC('s', 't', 't', 's'),
	FOURCC_CTTS = MP4_FOURCC('c', 't', 't', 's'),
	FOURCC_STSS = MP4_FOURCC('s', 't', 's', 's'),
	FOURCC_STSC = MP4_FOURCC('s', 't', 's', 'c'),
	FOURCC_STSZ = MP4_FOURCC('s', 't', 's', 'z'),
	FOURCC_STCO = MP4_FOURCC('s', 't', 'c', 'o'),
	FOURCC_CO64 = MP4_FOURCC('c', 'o', '6', '4'),
	FOURCC_MOOF = MP4_FOURCC('m', 'o', 'o', 'f'),
	FOURCC_MFHD = MP4_FOURCC('m', 'f', 'h', 'd'),
	FOURCC_TRAF = MP4_FOURCC('t', 'r', 'a', 'f'),
	FOURCC_TFHD = MP4_FOURCC('t', 'f', 'h', 'd'),
	FOURCC_TRUN = MP4_FOURCC('t', 'r', 'u', 'n'),
	FOURCC_SDTP = MP4_FOURCC('s', 'd', 't', 'p'),
	FOURCC_SUBS = MP4_FOURCC('s', 'u', 'b', 's'),
	FOURCC_SIDX = MP4_FOURCC('s', 'i', 'd', 'x'),
	FOURCC_ESDS = MP4_FOURCC('e', 's', 'd', 's'),
	FOURCC_WAVE = MP4_FOURCC('w', 'a', 'v', 'e'),
	FOURCC_AVCC = MP4_FOURCC('a', 'v', 'c', 'C'),
	FOURCC_MP4V = MP4_FOURCC('m', 'p', '4', 'v'),
	FOURCC_JVT1 = MP4_FOURCC('j', 'v', 't', '1'),
	FOURCC_AVC1 = MP4_FOURCC('a', 'v', 'c', '1'),
	FOURCC_RAW  = MP4_FOURCC('r', 'a', 'w', ' '),
	FOURCC_I420 = MP4_FOURCC('I', '4', '2', '0'),
	FOURCC_YUY2 = MP4_FOURCC('Y', 'U', 'Y', '2'),
	FOURCC_MP4A = MP4_FOURCC('m', 'p', '4', 'a'),
	FOURCC_LPCM = MP4_FOURCC('l', 'p', 'c', 'm'),
	FOURCC_ALAW = MP4_FOURCC('a', 'l', 'a', 'w'),
	FOURCC_ULAW = MP4_FOURCC('u', 'l', 'a', 'w'),
	FOURCC_SAMR = MP4_FOURCC('s', 'a', 'm', 'r'),
	FOURCC_SAWB = MP4_FOURCC('s', 'a', 'w', 'b'),
	FOURCC_TX3G = MP4_FOURCC('t', 'x', '3', 'g'),
	FOURCC_D263 = MP4_FOURCC('d', '2', '6', '3'),
	FOURCC_DAMR = MP4_FOURCC('d', 'a', 'm', 'r')
};

enum Mp4CodecFormat {
	MP4_FORMAT_UNKNOWN   = 0,
	MP4_FORMAT_MPEG4_VID = 1,
	MP4_FORMAT_AVC       = 2,
	MP4_FORMAT_RAW_VID   = 4,
	MP4_FORMAT_AAC       = 5,
	MP4_FORMAT_PCM_AUDIO = 6,
	MP4_FORMAT_TIMEDTEXT = 7
};

/* Allocator tag bytes prepended to tagged heap blocks */
enum Mp4AllocTag {
	MP4_TAG_EXT_ALLOC_D0 = 0xD0,
	MP4_TAG_EXT_ALLOC_DD = 0xDD,
	MP4_TAG_MSPACE_DE    = 0xDE,
	MP4_TAG_MSPACE_DF    = 0xDF
};

static inline uint16_t mp4ReadBe16(const uint8_t *buf)
{
	return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

static inline uint32_t mp4ReadBe32(const uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	       ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

static inline uint64_t mp4ReadBe64(const uint8_t *buf)
{
	return ((uint64_t)mp4ReadBe32(buf) << 32) | mp4ReadBe32(buf + 4);
}

static inline uint32_t mp4Bswap32(uint32_t val)
{
	return ((val & 0x000000FFu) << 24) | ((val & 0x0000FF00u) << 8) |
	       ((val & 0x00FF0000u) >> 8) | ((val & 0xFF000000u) >> 24);
}

static inline uint16_t mp4Bswap16(uint16_t val)
{
	return (uint16_t)((val >> 8) | (val << 8));
}

/* Global mspace and debug level */
extern int g_mp4DebugLevel;
extern SceClibMspace g_mp4Mspace;
extern SceKernelLwMutexWork g_mp4DemuxMutex;

int mp4MspaceInit(SceSize size, void *memblock);
int mp4MspaceDestroy(void);
void *mp4MspaceAllocZero(int alignment, SceSize size);
static inline __attribute__((always_inline)) void mp4MspaceFree(void *ptr)
{
	operator delete(ptr);
}

#ifdef __cplusplus
void *operator new(size_t size);
#endif

/* Atomic helpers (using ARM ldrex/strex via SNC builtins) */
static inline void mp4AtomicStore32(volatile uint32_t *addr, uint32_t val)
{
	do {
		__builtin_ldrex(addr);
	} while (__builtin_strex(val, addr) != 0);
}

static inline uint32_t mp4AtomicInc32(volatile uint32_t *addr)
{
	uint32_t val;
	do {
		val = __builtin_ldrex(addr) + 1;
	} while (__builtin_strex(val, addr) != 0);
	return val;
}

static inline uint32_t mp4AtomicDec32(volatile uint32_t *addr)
{
	uint32_t val;
	do {
		val = __builtin_ldrex(addr) - 1;
	} while (__builtin_strex(val, addr) != 0);
	return val;
}

static inline void mp4AtomicStore64(volatile uint64_t *addr, uint64_t val)
{
	do {
		__builtin_ldrexd(addr);
	} while (__builtin_strexd(val, addr) != 0);
}

static inline uint64_t mp4AtomicAdd64(volatile uint64_t *addr, uint64_t delta)
{
	uint64_t val;
	do {
		val = __builtin_ldrexd(addr) + delta;
	} while (__builtin_strexd(val, addr) != 0);
	return val;
}

/* Reference count control block ("CriticalSmart") */
struct Mp4RefControl {
	SceKernelLwMutexWork mutex;
	int32_t              refCount;
	int32_t              pad;
};

/* Public / internal sample buffer passed by caller */
struct Mp4DemuxSample {
	uint8_t  *data;
	int32_t   capacity;
	int32_t   size;
	int32_t   reserved;
	int64_t   pts;
	int64_t   dts;
	int64_t   duration;
	int32_t   isKeyFrame;
	int32_t   trackIndex;
	uint8_t   isVideo;
	uint8_t   pad[7];
};

/* Internal 320-byte stream description (0x140) */
struct Mp4StreamDescription {
	uint32_t streamType;
	uint32_t codecType;
	uint32_t widthOrSampleRate;
	uint32_t heightOrBitsPerSample;
	uint32_t profileOrBitrateMax;
	uint32_t levelOrBitrateAvg;
	uint32_t refFramesOrBufferSize;
	uint32_t maxReorderOrChannels;
	uint32_t maxBufferSize;
	uint32_t reservedA;
	int64_t  frameDuration;
	int64_t  aspectRatioFixed;
	uint8_t  languageCode[4];
	uint16_t configSize;
	uint8_t  configData[258];
};

/* External stream info returned by SceMp4_8BE0E3D3 */
struct Mp4ExternalStreamInfo {
	uint32_t streamType;
	uint32_t codecType;
	uint32_t videoWidth;
	uint32_t videoHeight;
	uint32_t videoProfile;
	uint32_t videoLevel;
	uint32_t videoRefFrames;
	uint32_t videoMaxReorder;
	uint32_t videoMaxBufferSize;
	uint32_t videoReserved;
	int64_t  videoFrameDuration;
	int64_t  videoAspectRatio;
	uint32_t audioSampleRate;
	uint32_t audioBitsPerSample;
	uint32_t audioBitrateMax;
	uint32_t audioBitrateAvg;
	uint32_t audioBufferSize;
	uint32_t audioChannels;
	uint8_t  languageCode[4];
	uint8_t  configSize;
	uint8_t  configData[255];
};

/* Doubly linked list used throughout libscemp4 ("0Demux_Stl_Handler_Mutex ") */
template <typename T>
struct Mp4ListNode {
	Mp4ListNode<T> *prev;
	T               value;
	Mp4ListNode<T> *next;
};

template <typename T>
class Mp4ListIterator {
public:
	virtual ~Mp4ListIterator() {}

	void           *listPtr;
	Mp4ListNode<T> *currentNode;
	uint32_t        index;
};

template <typename T>
struct Mp4List {
	Mp4ListNode<T>      *head;
	Mp4ListNode<T>      *tail;
	uint32_t             count;
	uint32_t             totalBytes;
	uint32_t             isShallowCopy;
	Mp4ListIterator<T>  *beginIter;
	Mp4ListIterator<T>  *endIter;
	Mp4ListNode<T>      *sentinel;
	SceKernelLwMutexWork mutex;
};

/* Forward declarations */
class Mp4DataReader;
class Mp4Atom;
class Mp4SampleExtractor;
class Mp4StreamFormat;
class Mp4SampleSizeTable;
class Mp4SyncSampleTable;
class Mp4TimeToSampleTable;
class Mp4TrackFragment;
class Mp4FragmentManager;
class Mp4Track;
class Mp4Movie;
class Mp4FileStreamReader;
class Mp4FileStreamDataReader;
class Mp4Demuxer;

/* Base data reader interface */
class Mp4DataReader {
public:
	virtual ~Mp4DataReader();
	virtual int read(int64_t offset, uint32_t length, uint8_t *dst);
	virtual int64_t getLength();
	virtual int isStreaming();
	virtual int getBufferSize();
	virtual void reset();
};

/* Sample extractor interface */
class Mp4SampleExtractor {
public:
	virtual ~Mp4SampleExtractor() {}
	virtual int getOutputSize(int sampleSize) { (void)sampleSize; return 0; }
	virtual void reset() {}
	virtual int extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize) {
		(void)sample; (void)movie; (void)offset; (void)sampleSize; (void)outSize;
		return MP4_ERR_GENERIC;
	}
};

/* ISO BMFF Atom */
class Mp4Atom {
public:
	Mp4Atom(Mp4DataReader *reader, int64_t offset, int64_t size, uint32_t fourcc, int headerSize, SceAvPlayerMemAllocator *allocator);
	virtual ~Mp4Atom();
	virtual int readData(int64_t relOffset, uint32_t length, void *dst);
	virtual int64_t getSize() { return atomSize; }
	virtual int isLoaded();
	virtual uint8_t *mapBuffer();
	virtual int unmapBuffer();
	virtual int64_t getOffset() { return atomOffset; }
	virtual uint32_t getFourCC() { return atomFourcc; }
	virtual int getHeaderSize() { return atomHeaderSize; }
	virtual int getChildCount();
	virtual Mp4Atom *getChildAt(int index);
	virtual int countChildrenByType(uint32_t fourcc);
	virtual Mp4Atom *findChildByType(uint32_t fourcc, int occurrence);
	virtual Mp4Atom *findChildByTypeAtOrAfter(uint32_t fourcc, uint32_t minOccurrence);
	virtual int64_t getChildOffsetByType(uint32_t fourcc, uint32_t minOccurrence);
	virtual void parseChildren(int64_t startOffset);

	inline __attribute__((always_inline)) Mp4Atom *findOrParseChild(uint32_t fourcc, int occurrence)
	{
		Mp4Atom *found = this->findChildByType(fourcc, occurrence);
		if (found != NULL)
			return found;
		this->parseChildren(this->parsedOffset);
		return this->findChildByType(fourcc, occurrence);
	}

	SceAvPlayerMemAllocator *allocator;
	Mp4DataReader           *reader;
	int64_t                  atomOffset;
	int64_t                  atomSize;
	int32_t                  atomHeaderSize;
	uint32_t                 atomFourcc;
	Mp4List<Mp4Atom *>       children;
	uint8_t                 *mappedData;
	volatile uint32_t        mapRefCount;
	int64_t                  parsedOffset;
};

/* Sample size / sample-to-chunk / chunk-offset table (stsz, stsc, stco/co64) */
class Mp4SampleSizeTable {
public:
	Mp4SampleSizeTable();
	int init(Mp4Atom *stblAtom);
	uint32_t getSampleSize(int sampleIndex);
	int64_t getSampleOffset(int sampleIndex);

	Mp4Atom *stszAtom;
	Mp4Atom *stszMappedAtom;
	uint8_t *stszData;
	int32_t  sampleCount;
	int32_t  maxSampleSize;
	int32_t  uniformSampleSize;
	int32_t  stscEntryCount;
	int32_t  chunkCount;
	uint8_t  isCo64;
	uint8_t  pad[3];
	Mp4Atom *stscAtom;
	Mp4Atom *stscMappedAtom;
	uint8_t *stscData;
	Mp4Atom *stcoAtom;
	Mp4Atom *stcoMappedAtom;
	uint8_t *stcoData;
	int32_t  cachedFirstSample;
	int32_t  cachedEndSample;
	int32_t  cachedStscIndex;
};

/* Sync sample table (stss) */
class Mp4SyncSampleTable {
public:
	Mp4SyncSampleTable();
	~Mp4SyncSampleTable();
	int init(Mp4Atom *stblAtom);
	int32_t findPrevSyncSample(int32_t sampleIndex);
	int32_t findNextSyncSample(int32_t sampleIndex);

	Mp4Atom *stssAtom;
	uint8_t *stssData;
	int32_t  entryCount;
};

/* Time-to-sample and composition-offset table (stts, ctts) */
class Mp4TimeToSampleTable {
public:
	Mp4TimeToSampleTable();
	int init(int32_t timescale, int64_t editOffset, Mp4Atom *stblAtom);
	int64_t ticksToMediaTime(int64_t ticks90k);
	int64_t mediaTimeToTicks(int64_t mediaTime);
	int64_t getCompositionOffset(int32_t sampleIndex);
	int32_t findSampleByTimestamp(int64_t ticks90k);
	int64_t getSampleDts(int32_t sampleIndex);
	int64_t getSamplePts(int32_t sampleIndex);
	int64_t getSampleDuration(int32_t sampleIndex);

	int32_t  mediaTimescale;
	int64_t  editTimeOffset;
	Mp4Atom *sttsAtom;
	Mp4Atom *cttsAtom;
	Mp4Atom *sttsMappedAtom;
	uint8_t *sttsData;
	Mp4Atom *cttsMappedAtom;
	uint8_t *cttsData;
	uint32_t sttsEntryCount;
	uint32_t cttsEntryCount;
	int32_t  cachedSampleIndex;
	uint32_t cachedSttsEntry;
	int64_t  cachedMediaTime;
	uint32_t cachedCttsEntry;
	int32_t  cachedCttsSample;
	float    ticksPerMediaUnit;
};

/* Buffered atom copy used by Mp4TrackFragment */
struct Mp4BoxBuffer {
	uint8_t  *data;
	uint32_t  size;
	uint32_t  capacity;
};

/* Segment index entry parsed from sidx */
struct Mp4SidxEntry {
	uint32_t referencedSize;
	float    durationTicks;
	float    startTimeTicks;
	uint8_t  startsWithSap;
	uint8_t  pad[3];
	uint32_t sapDeltaTime;
};

/* Fragmented MP4 per-track run (traf / tfhd / trun) */
class Mp4TrackFragment {
public:
	Mp4TrackFragment(const uint32_t *initParams, int32_t fragmentCount, int32_t trackHint);
	~Mp4TrackFragment();
	int clear();
	int parseTfhd(Mp4Atom *tfhdAtom);
	int parseTrafBoxes(Mp4Atom *trafAtom, int64_t moofOffset);
	int prepareRun(float startTime, float runDuration);
	int advanceSampleInfo();
	int nextSample();
	int prevSample();
	int64_t getCurrentOffset();
	int64_t getCurrentPts();
	int64_t getCurrentDts();
	int32_t getCurrentSampleSize();
	bool isAtFirstSample();
	bool isAtEnd();

	uint32_t      trackId;
	int64_t       baseDataOffset;
	float         defaultSampleDuration;
	int64_t       defaultSampleSize;
	uint32_t      sampleCount;
	uint8_t       enabled;
	uint8_t       enabledPad[11];
	Mp4BoxBuffer *trunBuf;
	Mp4BoxBuffer *sdtpBuf;
	Mp4BoxBuffer *subsBuf;
	int64_t       currentSampleOffset;
	float         currentDts;
	float         currentPts;
	int32_t       currentSampleSize;
	int32_t       currentSampleIndex;
	uint32_t      mediaTimescale;
	int64_t       trackDuration;
	int32_t       totalFragments;
	float         timeScaleFactor;
	float         fragmentDuration;
	uint32_t      initParams[3];
	int32_t       trackHint;
};

/* Fragmented MP4 manager (moof / mfhd / sidx) */
class Mp4FragmentManager {
public:
	Mp4FragmentManager(Mp4Atom *rootAtom, int32_t primaryVideoTimescale, int32_t moofCount, const uint32_t *initParams, int32_t trackHint);
	~Mp4FragmentManager();
	void clear();
	bool isEndOfFile();
	int parseSidx();
	Mp4SidxEntry *getSidxEntry(uint32_t index);
	int loadMoof(Mp4Atom *moofAtom);
	int dispatchBox(Mp4Atom *atom, int mode);
	int loadNextMoof();
	int loadPrevMoof();
	int seekToTime(int32_t unused, uint64_t targetTicks90k, int64_t *outActualTicks);
	Mp4TrackFragment *getTrackFragment(uint32_t index);

	Mp4Atom                      *rootAtom;
	uint32_t                      sequenceNumber;
	uint32_t                      currentMoofIndex;
	int32_t                       primaryTimescale;
	int32_t                       totalMoofCount;
	uint8_t                       reachedEof;
	uint8_t                       eofPad[3];
	Mp4List<Mp4TrackFragment *>   trackFragments;
	Mp4List<Mp4SidxEntry>         sidxEntries;
	uint32_t                      initParams[3];
	int32_t                       trackHint;
};

/* Async file streaming chunk */
struct Mp4StreamChunk {
	uint64_t fileOffset;
	uint32_t validSize;
	uint32_t reserved;
	uint8_t *buffer;
	uint32_t pad;
};

/* Asynchronous buffered file stream reader ("avPlayer FileStreaming") */
class Mp4FileStreamReader {
public:
	Mp4FileStreamReader(int32_t streamFlags);
	~Mp4FileStreamReader();
	int releaseChunk(Mp4StreamChunk *chunk);
	int seekTo(int64_t offset);
	int suspendStreaming();
	int resumeStreaming();
	inline __attribute__((always_inline)) int abortStreaming()
	{
		mp4AtomicStore32(&this->activeFlag, 0);
		return 0;
	}
	int closeFile();
	int stopAndClose();
	int64_t queryFileSize();
	void streamingLoop();
	int openAndStart(const char *filename, int32_t unused, SceAvPlayerFileReplacement *fileOps, int32_t basePriority);
	int readAt(uint8_t *dst, int64_t offset, SceSize length, uint32_t *outRead);
	int configureCacheLimit(int64_t interleaveLimit);

	Mp4List<Mp4StreamChunk *>    chunkQueue;
	volatile uint32_t            shuttingDown;
	uint32_t                     shutdownPad;
	SceClibMspace                chunkMspace;
	uint32_t                     mspacePad;
	SceKernelLwMutexWork         queueMutex;
	SceUID                       condId;
	SceUID                       mutexId;
	volatile uint64_t            readCursor;
	volatile uint64_t            cachedStartOffset;
	volatile uint64_t            cachedEndOffset;
	uint64_t                     fileSize;
	uint8_t                     *mspaceBacking;
	volatile uint32_t            threadRunning;
	volatile uint32_t            bypassCache;
	volatile uint32_t            cacheDisabled;
	volatile uint32_t            readErrorFlag;
	volatile uint32_t            stalledFlag;
	SceKernelLwMutexWork         ioMutex;
	SceUID                       threadId;
	SceAvPlayerFileReplacement  *fileOps;
	int32_t                      streamFlags;
	int32_t                      state;
	volatile uint32_t            activeFlag;
};

/* DataReader wrapper over Mp4FileStreamReader */
class Mp4FileStreamDataReader : public Mp4DataReader {
public:
	inline __attribute__((always_inline)) Mp4FileStreamDataReader(int32_t inFlags)
	{
		this->streamFlags = inFlags;
		this->streamReader = new Mp4FileStreamReader(inFlags);
	}
	virtual ~Mp4FileStreamDataReader();
	virtual int read(int64_t offset, uint32_t length, uint8_t *dst);
	virtual int64_t getLength();
	virtual int isStreaming() { return 0; }
	virtual int getBufferSize() { return 0; }
	virtual void reset() {}

	Mp4FileStreamReader *streamReader;
	int32_t              streamFlags;
};

/* H.264 SPS / VUI parsed entry (80 bytes) */
struct Mp4AvcSpsInfo {
	int32_t  nalSize;
	uint8_t  valid;
	uint8_t  profileIdc;
	uint8_t  levelIdc;
	uint8_t  headerPad;
	uint32_t spsId;
	uint32_t maxFrameNum;
	int32_t  picWidthMbs;
	int32_t  picHeightMbs;
	int32_t  picOrderCntType;
	uint32_t maxNumRefFrames;
	uint8_t  frameMbsOnlyFlag;
	uint8_t  spsReserved[7];
	uint8_t  frameCroppingFlag;
	uint8_t  vuiPresentFlag;
	uint8_t  vuiPad[2];
	uint8_t  aspectRatioIdc;
	uint8_t  aspectPad[3];
	float    sarRatio;
	uint32_t sarWidth;
	uint32_t sarHeight;
	uint32_t maxNumReorderFrames;
	uint32_t videoFormat;
	uint32_t maxDecFrameBuffering;
	uint32_t numUnitsInTick;
	uint32_t timeScale;
};

/* Timed-text font table entry (8 bytes) */
struct Mp4FontRecord {
	uint16_t fontId;
	uint8_t  nameLength;
	uint8_t  pad;
	char    *fontName;
};

/* Track codec format & extractor owner (504 bytes) */
class Mp4StreamFormat {
public:
	Mp4StreamFormat(int32_t streamFlags);
	~Mp4StreamFormat();
	int parseEsdsAtom(Mp4Atom *esdsAtom);
	Mp4AvcSpsInfo *getSpsInfo(uint32_t index);
	uint32_t bumpAvcLevel(Mp4AvcSpsInfo *sps);
	int readExpGolomb(uint8_t *bitOffset, int32_t *outVal, const uint8_t *data, uint32_t maxVal);
	int readBits(const uint8_t **ptr, uint8_t *bitOffset, uint32_t numBits, int32_t *outVal, uint32_t maxVal, bool isExpGolomb);
	bool skipHrdParameters(const uint8_t **ptr, uint8_t *bitOffset);
	void parseVuiParameters(Mp4AvcSpsInfo *sps, const uint8_t *data, int bitOffset);
	uint32_t computeMaxDpbFrames(const Mp4AvcSpsInfo *sps);
	void parseAvcSpsList();
	int parseTimedTextConfig();
	int parseAudioSampleEntry(Mp4Atom *sampleEntryAtom);
	int parseSampleDescription(int64_t avgFrameDuration, Mp4Atom *sampleEntryAtom);
	bool isVideo() const;
	bool isAudio() const;
	bool isTimedText() const;
	int fillTimedTextDesc(Mp4StreamDescription *outDesc);
	int fillAudioDesc(Mp4StreamDescription *outDesc);
	void applySpsToVideoDesc(Mp4StreamDescription *outDesc, uint32_t spsIndex);
	int fillVideoDesc(Mp4StreamDescription *outDesc, uint32_t spsIndex);
	int getStreamDescription(Mp4StreamDescription *outDesc, uint32_t index, uint32_t spsIndex);
	bool selectStreamDescription(const Mp4StreamDescription *desc);

	int32_t                    format;
	uint8_t                   *codecConfig;
	int32_t                    codecConfigSize;
	uint8_t                   *esdsDecoderConfig;
	int32_t                    esdsDecoderConfigSize;
	int32_t                    videoWidth;
	int32_t                    videoHeight;
	int64_t                    avgFrameDuration;
	uint32_t                   rawFourcc;
	uint32_t                   rawBitDepth;
	uint32_t                   defaultSpsIndex;
	Mp4StreamDescription       selectedDesc;
	Mp4SampleExtractor        *extractor;
	uint32_t                   extractorPad;
	Mp4List<Mp4AvcSpsInfo>     spsList;
	uint32_t                   tx3gDisplayFlags;
	uint8_t                    tx3gHorizJust;
	uint8_t                    tx3gVertJust;
	uint8_t                    tx3gBgColor[4];
	int16_t                    tx3gBoxTop;
	int16_t                    tx3gBoxLeft;
	int16_t                    tx3gBoxBottom;
	int16_t                    tx3gBoxRight;
	uint16_t                   tx3gStartChar;
	uint16_t                   tx3gEndChar;
	uint16_t                   tx3gFontId;
	uint8_t                    tx3gFaceStyle;
	uint8_t                    tx3gFontSize;
	uint8_t                    tx3gTextColor[4];
	uint16_t                   tx3gTextPad;
	uint16_t                   tx3gFontCount;
	uint16_t                   tx3gFontPad;
	Mp4FontRecord             *tx3gFonts;
	int32_t                    streamFlags;
	uint16_t                   audioChannels;
	uint16_t                   audioBitsPerSample;
	uint16_t                   audioSampleRate;
	uint16_t                   audioRatePad;
};

/* Movie track (208 bytes) */
class Mp4Track {
public:
	Mp4Track(Mp4Atom *trakAtom, Mp4Movie *ownerMovie, int32_t trackIndex, SceAvPlayerMemAllocator *allocator, int32_t streamFlags);
	inline __attribute__((always_inline)) ~Mp4Track();
	bool parseStsd(int64_t avgFrameDuration, Mp4Atom *stsdAtom);
	int parseMedia(Mp4Atom *mdiaAtom, int64_t editOffset);
	uint64_t parseEditList(Mp4Atom *edtsAtom);
	int buildSampleOffsets(Mp4Atom *stscAtom);
	int loadChunkOffsets(Mp4Atom *stcoAtom);
	int loadSampleSizes(Mp4Atom *stszAtom);
	int loadTimeToSample(Mp4Atom *sttsAtom, bool isVideoTrack);
	int loadCompositionOffsets(Mp4Atom *cttsAtom);
	int loadSyncSamples(Mp4Atom *stssAtom);
	int loadSeekTables();
	bool isVideo() const;
	bool isAudio() const;
	bool isTimedText() const;
	int getStreamDescription(Mp4StreamDescription *outDesc, uint32_t index, uint32_t spsIndex);
	bool selectStreamDescription(const Mp4StreamDescription *desc);
	Mp4SampleExtractor *getExtractor();
	int findSampleForSeek(int forward, uint64_t targetTicks, int32_t *outSampleIndex, int64_t *outTimestamp);
	bool findKeyFrame(bool forward, uint32_t currentSample, uint32_t *outSample, int64_t *outTimestamp);
	int readSampleData(int32_t sampleIndex, uint32_t length, void *dst);

	SceAvPlayerMemAllocator *allocator;
	int32_t                  streamFlags;
	Mp4Atom                 *trakAtom;
	Mp4Movie                *ownerMovie;
	int32_t                  trackIndex;
	int32_t                  mediaTimescale;
	int64_t                  trackDuration;
	uint8_t                  languageCode[4];
	Mp4Atom                 *stblAtom;
	Mp4StreamFormat         *streamFormat;
	Mp4RefControl           *streamFormatRef;
	Mp4SampleSizeTable      *sampleSizeTable;
	Mp4RefControl           *sampleSizeTableRef;
	Mp4SyncSampleTable      *syncSampleTable;
	Mp4RefControl           *syncSampleTableRef;
	Mp4TimeToSampleTable    *timeToSampleTable;
	Mp4RefControl           *timeToSampleTableRef;
	int32_t                  trackTableReserved;
	uint32_t                *syncSampleIndices;
	uint32_t                *sttsSampleCounts;
	uint32_t                *sttsSampleDeltas;
	uint32_t                 cttsTotalSamples;
	int64_t                 *cttsOffsets90k;
	uint32_t                *sampleSizes;
	uint32_t                *chunkOffsets;
	uint32_t                *sampleOffsets;
	uint32_t                 totalSampleCount;
	uint32_t                 totalChunkCount;
	uint64_t                 totalDataBytes;
	uint32_t                 maxSampleSize;
	uint32_t                 maxSampleSizeIndex;
	uint32_t                 sttsEntryCount;
	uint32_t                 sttsTotalSamples;
	uint32_t                 syncSampleCount;
	float                    peakBytesPerSec;
	int64_t                 *sampleDts90k;
	uint32_t                 dtsTablePad;
	uint8_t                  mdhdVersion1;
	uint8_t                  mdhdVersionPad[3];
	uint32_t                 tkhdMatrix[9];
	Mp4TrackFragment        *fragmentTrack;
	Mp4FragmentManager      *fragmentMgr;
};

/* Parsed MP4 movie container (112 bytes) */
class Mp4Movie {
public:
	Mp4Movie(Mp4Atom *rootAtom, SceAvPlayerMemAllocator *allocator, int32_t streamFlags);
	inline __attribute__((always_inline)) ~Mp4Movie();
	int readData(int64_t offset, uint32_t length, void *dst);

	Mp4Atom                 *rootAtom;
	Mp4RefControl           *rootAtomRef;
	Mp4List<Mp4Track *>      tracks;
	int32_t                  movieTimescale;
	uint64_t                 movieDurationRaw;
	int64_t                  movieDuration100us;
	int32_t                  streamFlags;
	int32_t                  isFragmented;
	int32_t                  fragmentPad;
	Mp4FragmentManager      *fragmentMgr;
};

/* Per-track playback cursor stored in Mp4Demuxer's track list (16 bytes) */
struct Mp4TrackCursor {
	uint8_t   enabled;
	uint8_t   pad[3];
	Mp4Track *track;
	int32_t   currentSample;
	int32_t   streamFlags;
};

/* Top-level MP4 demuxer handle (120 bytes) */
class Mp4Demuxer {
public:
	int openFile(const char *filename, SceAvPlayerFileReplacement *fileOps, SceAvPlayerMemAllocator *allocator, int32_t basePriority, int32_t streamFlags);
	int64_t parseMovie(int32_t *outTrackCount);
	int closeMovie();
	int resetCursors();
	int resumeStreaming();
	int suspendStreaming();
	int abortStreaming();
	void analyzeInterleave();
	int getExternalStreamInfo(uint32_t trackIndex, Mp4ExternalStreamInfo *outInfo);
	int getStreamDescription(int32_t trackIndex, Mp4StreamDescription *outDesc);
	int setTrackEnabled(int32_t trackIndex, bool enabled);
	int findEnabledVideoTrack();
	int findEnabledNonVideoTrack();
	int seekToTime(int32_t unused, int64_t targetTicks90k);
	int stepKeyFrameForward(Mp4DemuxSample *outSample);
	int stepKeyFrameBackward(Mp4DemuxSample *outSample);
	int readNextSample(Mp4DemuxSample *outSample);
	int copyLastSampleData(void *dst);
	int readTrackSample(uint32_t trackIndex, Mp4DemuxSample *outSample);

	int32_t                   demuxHeaderReserved;
	SceAvPlayerMemAllocator  *allocator;
	Mp4List<Mp4TrackCursor>   cursors;
	Mp4DemuxSample           *lastSample;
	Mp4FileStreamDataReader  *dataReader;
	Mp4Movie                 *movie;
	int64_t                   maxInterleaveGap;
	int64_t                   defaultBufferSize;
	int64_t                   demuxTailReserved;
	int32_t                   streamFlags;
	uint8_t                   isM3d;
	uint8_t                   isNonInterleaved;
	uint8_t                   tailPad[2];
};

/* Helper functions for Mp4TrackCursor */
void mp4CursorSetEnabled(Mp4TrackCursor *cursor, bool enabled);

static inline __attribute__((always_inline)) void mp4FreeTaggedBlock(SceAvPlayerMemAllocator *allocator, void *ptr)
{
	if (ptr == NULL)
		return;
	uint8_t *raw = (uint8_t *)ptr - 1;
	if (raw[0] == MP4_TAG_MSPACE_DF)
		mp4MspaceFree(raw);
	else
		allocator->deallocate(allocator->objectPointer, raw);
}

inline __attribute__((always_inline)) Mp4Track::~Mp4Track()
{
	mp4FreeTaggedBlock(this->allocator, this->syncSampleIndices);
	this->syncSampleIndices = NULL;
	mp4FreeTaggedBlock(this->allocator, this->sttsSampleCounts);
	this->sttsSampleCounts = NULL;
	mp4FreeTaggedBlock(this->allocator, this->sttsSampleDeltas);
	this->sttsSampleDeltas = NULL;
	mp4FreeTaggedBlock(this->allocator, this->sampleSizes);
	this->sampleSizes = NULL;
	mp4FreeTaggedBlock(this->allocator, this->chunkOffsets);
	this->chunkOffsets = NULL;
	mp4FreeTaggedBlock(this->allocator, this->sampleOffsets);
	this->sampleOffsets = NULL;

	if (this->sampleDts90k != NULL) {
		this->allocator->deallocate(this->allocator->objectPointer, this->sampleDts90k);
		this->sampleDts90k = NULL;
	}
	if (this->cttsOffsets90k != NULL) {
		this->allocator->deallocate(this->allocator->objectPointer, this->cttsOffsets90k);
		this->cttsOffsets90k = NULL;
	}

	if (this->streamFormatRef != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&this->streamFormatRef->refCount);
		if (this->streamFormatRef->refCount <= 0) {
			sceKernelDeleteLwMutex(&this->streamFormatRef->mutex);
			mp4MspaceFree(this->streamFormatRef);
			if (this->streamFormat != NULL) {
				this->streamFormat->~Mp4StreamFormat();
				mp4MspaceFree(this->streamFormat);
			}
		}
		this->streamFormat = NULL;
		this->streamFormatRef = NULL;
	}
	if (this->sampleSizeTableRef != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&this->sampleSizeTableRef->refCount);
		if (this->sampleSizeTableRef->refCount <= 0) {
			sceKernelDeleteLwMutex(&this->sampleSizeTableRef->mutex);
			mp4MspaceFree(this->sampleSizeTableRef);
			if (this->sampleSizeTable != NULL) {
				if (this->sampleSizeTable->stcoMappedAtom != NULL)
					this->sampleSizeTable->stcoMappedAtom->unmapBuffer();
				if (this->sampleSizeTable->stscMappedAtom != NULL)
					this->sampleSizeTable->stscMappedAtom->unmapBuffer();
				if (this->sampleSizeTable->stszMappedAtom != NULL)
					this->sampleSizeTable->stszMappedAtom->unmapBuffer();
				mp4MspaceFree(this->sampleSizeTable);
			}
		}
		this->sampleSizeTable = NULL;
		this->sampleSizeTableRef = NULL;
	}
	if (this->syncSampleTableRef != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&this->syncSampleTableRef->refCount);
		if (this->syncSampleTableRef->refCount <= 0) {
			sceKernelDeleteLwMutex(&this->syncSampleTableRef->mutex);
			mp4MspaceFree(this->syncSampleTableRef);
			if (this->syncSampleTable != NULL) {
				this->syncSampleTable->~Mp4SyncSampleTable();
				mp4MspaceFree(this->syncSampleTable);
			}
		}
		this->syncSampleTable = NULL;
		this->syncSampleTableRef = NULL;
	}
	if (this->timeToSampleTableRef != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&this->timeToSampleTableRef->refCount);
		if (this->timeToSampleTableRef->refCount <= 0) {
			sceKernelDeleteLwMutex(&this->timeToSampleTableRef->mutex);
			mp4MspaceFree(this->timeToSampleTableRef);
			if (this->timeToSampleTable != NULL) {
				if (this->timeToSampleTable->sttsMappedAtom != NULL)
					this->timeToSampleTable->sttsMappedAtom->unmapBuffer();
				if (this->timeToSampleTable->cttsMappedAtom != NULL)
					this->timeToSampleTable->cttsMappedAtom->unmapBuffer();
				mp4MspaceFree(this->timeToSampleTable);
			}
		}
		this->timeToSampleTable = NULL;
		this->timeToSampleTableRef = NULL;
	}
}

inline __attribute__((always_inline)) Mp4Movie::~Mp4Movie()
{
	Mp4ListNode<Mp4Track *> *cur = this->tracks.head;
	while (cur != NULL) {
		Mp4Track *trk = cur->value;
		if (trk != NULL) {
			trk->~Mp4Track();
			mp4MspaceFree(trk);
		}
		cur = cur->next;
	}

	while (this->tracks.count != 0) {
		sceKernelLockLwMutex(&this->tracks.mutex, 1, NULL);
		if (this->tracks.count != 0 && this->tracks.tail != NULL) {
			Mp4ListNode<Mp4Track *> *tail = this->tracks.tail;
			Mp4ListNode<Mp4Track *> *prev = tail->prev;
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->tracks.tail = prev;
			this->tracks.count--;
			this->tracks.totalBytes -= sizeof(Mp4ListNode<Mp4Track *>);
		}
		sceKernelUnlockLwMutex(&this->tracks.mutex, 1);
	}

	if (this->tracks.isShallowCopy == 0) {
		if (this->tracks.beginIter != NULL)
			delete this->tracks.beginIter;
		if (this->tracks.endIter != NULL)
			delete this->tracks.endIter;
		if (this->tracks.sentinel != NULL)
			mp4MspaceFree(this->tracks.sentinel);
	}
	sceKernelDeleteLwMutex(&this->tracks.mutex);

	if (this->rootAtomRef != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&this->rootAtomRef->refCount);
		if (this->rootAtomRef->refCount <= 0) {
			sceKernelDeleteLwMutex(&this->rootAtomRef->mutex);
			mp4MspaceFree(this->rootAtomRef);
			if (this->rootAtom != NULL)
				delete this->rootAtom;
		}
	}
	this->rootAtom = NULL;
	this->rootAtomRef = NULL;
}
int mp4CursorAdvance(Mp4TrackCursor *cursor);
int mp4CursorRewind(Mp4TrackCursor *cursor);
int mp4CursorReadSample(Mp4TrackCursor *cursor, Mp4Movie *movie, Mp4DemuxSample *outSample);
int mp4CursorSkipFragment(Mp4TrackCursor *cursor, Mp4DemuxSample *outSample);
int mp4CursorSeekFragment(Mp4TrackCursor *cursor, int32_t unused, uint64_t targetTicks, int64_t *outActual);

#endif /* MP4_INTERNAL_H */
