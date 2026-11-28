#ifndef VSH_APP_SHELL_MUSICCORE_MP4_BOXES_H
#define VSH_APP_SHELL_MUSICCORE_MP4_BOXES_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

static const uint32_t MP4_BOX_ERR_UNSUPPORTED = 0x80280021u;   /* -2144862175 */
static const uint32_t MP4_BOX_ERR_CORRUPT = 0x80280022u;       /* -2144862174 */
static const uint32_t MP4_BOX_ERR_NOT_READY = 0x80280024u;     /* -2144862172 */
static const uint32_t MP4_BOX_ERR_BAD_PARENT = 0x80280025u;    /* -2144862171 */
static const uint32_t MP4_BOX_ERR_NOT_FOUND = 0x80280028u;     /* -2144862168 */
static const uint32_t MP4_IO_ERR_BAD_PARAM = 0x8010AFF1u;      /* -2146390031 */

extern uint32_t readBigEndian32(const void *ptr);
extern uint32_t readBigEndian24(const void *ptr);
extern uint16_t readBigEndian16(const void *ptr);

/* Cached multi-stream I/O interface used by MP4 box readers (+24 getFileSize, +68 mapStreamRange) */
class Mp4CachedIo {
public:
    virtual ~Mp4CachedIo() {}
    virtual int isInitialized() const = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() const = 0;
    virtual int getFileSize() const = 0;
    virtual const char *getPath() const = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(uint32_t mode) = 0;
    virtual int applyOpenFilePriority(int mode) = 0;
    virtual void flushCache() = 0;
    virtual int setCacheHint(int hintMode) = 0;
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, uint32_t *outErr,
                                 int *outMappedSize) = 0;
    virtual void *mapRangePinned(int offset, uint32_t size, int pinFlag,
                                 int *outPinToken, uint32_t *outErr, uint32_t *outMappedSize) = 0;
    virtual int unpinRange(int pinToken, uint32_t *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, uint32_t *outErr) = 0;
    virtual void *mapStreamRange(int streamIdx, uint32_t offset, uint32_t size,
                                 uint32_t *outErr) = 0;
};

uint32_t readBitField(const uint8_t *buffer, uint32_t bitOffset, uint8_t bitCount);

/* Base ISO BMFF Box & Reader (0x811FFBF8 - 0x811FFF5B) */
class Mp4BoxBase {
public:
    explicit Mp4BoxBase(const char *fourcc);
    virtual ~Mp4BoxBase();

    virtual uint32_t getContentOffset() const;

    void resetParsed();
    bool isParsed() const;

    uint32_t m_boxSize;      /* +4 */
    char m_fourcc[4];        /* +8 */
    uint32_t m_fileOffset;   /* +12 */
    uint32_t m_headerSize;   /* +16 */
    int m_isParsed;          /* +20 */
};

class Mp4BoxReaderBase {
public:
    Mp4BoxReaderBase(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4BoxReaderBase();

    uint16_t countChildBoxes(const Mp4BoxBase *parentBox, const Mp4BoxBase *targetProto,
                             uint32_t *outErr);
    int locateChildBox(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                       Mp4BoxBase *outBox, uint32_t *outErr);

protected:
    Mp4CachedIo *m_io; /* +4 */
    int m_streamIdx;   /* +8 */
};

class Mp4FullBox : public Mp4BoxBase {
public:
    Mp4FullBox(const char *fourcc, uint8_t version);
    virtual ~Mp4FullBox();

    virtual uint32_t getContentOffset() const;

    uint8_t m_version;         /* +24 */
    uint8_t m_flags[3];        /* +25..+27 */
    uint32_t m_fullHeaderSize; /* +28 */
};

class Mp4FullBoxReader : public Mp4BoxReaderBase {
public:
    Mp4FullBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4FullBoxReader();

    int readFullBoxHeader(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
                          Mp4FullBox *outBox, uint32_t *outErr);
};

/* MPEG-4 Descriptors (0x811FFF5C - 0x812016F3) */
class Mp4DescriptorBase {
public:
    explicit Mp4DescriptorBase(uint8_t tag);
    virtual ~Mp4DescriptorBase();

    uint8_t m_tag;           /* +4 */
    uint8_t m_padTag[3];     /* +5..+7 */
    uint32_t m_fileOffset;   /* +8 */
    uint32_t m_totalSize;    /* +12 */
    uint32_t m_headerSize;   /* +16 */
    int m_isParsed;          /* +20 */
};

class Mp4DescriptorReaderBase {
public:
    Mp4DescriptorReaderBase(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DescriptorReaderBase();

    int readDescriptorHeader(uint32_t fileOffset, uint32_t availSize,
                             Mp4DescriptorBase *outDesc, uint32_t *outErr);

protected:
    Mp4CachedIo *m_io; /* +4 */
    int m_streamIdx;   /* +8 */
};

class Mp4DecoderSpecificInfo : public Mp4DescriptorBase {
public:
    Mp4DecoderSpecificInfo();
    virtual ~Mp4DecoderSpecificInfo();

    uint32_t m_bodyOffset; /* +24 */
};

class Mp4DecoderSpecificInfoReader : public Mp4DescriptorReaderBase {
public:
    Mp4DecoderSpecificInfoReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DecoderSpecificInfoReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4DecoderSpecificInfo *outInfo,
             uint32_t *outErr);
};

class Mp4AudioSpecificConfig : public Mp4DecoderSpecificInfo {
public:
    Mp4AudioSpecificConfig();
    virtual ~Mp4AudioSpecificConfig();

    uint32_t getAudioObjectType() const;
    uint32_t getSamplingFrequency() const;
    uint8_t getChannelConfig() const;

    uint8_t m_rawHeaderBytes[4];   /* +28..+31 */
    uint32_t m_audioObjectType;    /* +32 */
    uint32_t m_samplingFrequency;  /* +36 */
    uint8_t m_channelConfig;       /* +40 */
    uint8_t m_padChannel[3];       /* +41..+43 */
    uint32_t m_extSampleRate;      /* +44 */
    uint8_t m_sbrPresentFlag;      /* +48 */
    uint8_t m_padSbr[3];           /* +49..+51 */
    uint32_t m_parsedEndOffset;    /* +52 */
};

class Mp4AudioSpecificConfigReader : public Mp4DecoderSpecificInfoReader {
public:
    Mp4AudioSpecificConfigReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4AudioSpecificConfigReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4AudioSpecificConfig *outConfig,
             uint32_t *outErr);
};

bool isSupportedAacProfile(const Mp4AudioSpecificConfig *entryCountHolder,
                           const Mp4AudioSpecificConfig *asc);

class Mp4DecoderConfigDescriptor : public Mp4DescriptorBase {
public:
    Mp4DecoderConfigDescriptor();
    virtual ~Mp4DecoderConfigDescriptor();

    uint8_t m_objectTypeIndication; /* +24 */
    uint8_t m_streamType;           /* +25 */
    uint8_t m_upstreamFlag;         /* +26 */
    uint8_t m_padFlags;             /* +27 */
    uint32_t m_bufferSizeDb;        /* +28 */
    uint32_t m_maxBitrate;          /* +32 */
    uint32_t m_avgBitrate;          /* +36 */
    Mp4DescriptorBase *m_decSpec;   /* +40 */
    uint32_t m_parsedEndOffset;     /* +44 */
};

class Mp4DecoderConfigReader : public Mp4DescriptorReaderBase {
public:
    Mp4DecoderConfigReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4DecoderConfigReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4DecoderConfigDescriptor *outDesc,
             uint32_t *outErr);
};

class Mp4EsDescriptor : public Mp4DescriptorBase {
public:
    Mp4EsDescriptor();
    virtual ~Mp4EsDescriptor();

    uint8_t m_esIdLow;             /* +24 */
    uint8_t m_streamDependFlag;    /* +25 */
    uint8_t m_urlFlag;             /* +26 */
    uint8_t m_padFlags;            /* +27 */
    uint8_t m_ocrStreamFlag;       /* +28 */
    uint8_t m_streamPriority;      /* +29 */
    uint8_t m_padTail[2];          /* +30..+31 */
    uint32_t m_parsedEndOffset;    /* +32 */
};

class Mp4EsDescriptorReader : public Mp4DescriptorReaderBase {
public:
    Mp4EsDescriptorReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4EsDescriptorReader();

    int read(uint32_t fileOffset, uint32_t availSize, Mp4EsDescriptor *outDesc, uint32_t *outErr);
};

/* Sample Entry, Container, and Audio Track Boxes */
class Mp4SampleEntryBox : public Mp4BoxBase {
public:
    explicit Mp4SampleEntryBox(const char *fourcc);
    virtual ~Mp4SampleEntryBox();

    uint16_t m_dataReferenceIndex; /* +24 */
    uint16_t m_padRef;             /* +26 */
    uint32_t m_sampleEntryEnd;     /* +28 */
};

class Mp4SampleEntryReader : public Mp4BoxReaderBase {
public:
    Mp4SampleEntryReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4SampleEntryReader();

    int read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
             Mp4SampleEntryBox *outBox, uint32_t *outErr);
};

class Mp4AudioSampleEntryBox : public Mp4SampleEntryBox {
public:
    Mp4AudioSampleEntryBox();
    virtual ~Mp4AudioSampleEntryBox();

    virtual uint32_t getContentOffset() const;

    uint16_t m_sampleRateHi; /* +32 */
    uint16_t m_padRate;      /* +34 */
    uint32_t m_audioEntryEnd;/* +36 */
};

class Mp4AudioSampleEntryReader : public Mp4SampleEntryReader {
public:
    Mp4AudioSampleEntryReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4AudioSampleEntryReader();

    int read(const Mp4BoxBase *parentBox, uint16_t occurrenceIndex,
             Mp4AudioSampleEntryBox *outBox, uint32_t *outErr);
};

class Mp4ContainerBox : public Mp4BoxBase {
public:
    explicit Mp4ContainerBox(const char *fourcc);
    virtual ~Mp4ContainerBox();

    uint32_t m_childrenOffset; /* +24 */
};

class Mp4ContainerReader : public Mp4BoxReaderBase {
public:
    Mp4ContainerReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4ContainerReader();

    int readChildContainer(const Mp4BoxBase *parentBox, Mp4ContainerBox *outBox, uint32_t *outErr);
    int readTopContainer(Mp4ContainerBox *outBox, uint32_t *outErr);
    uint16_t countTracks(const Mp4BoxBase *moovBox, uint32_t *outErr);
    int readTrackAt(const Mp4BoxBase *moovBox, uint16_t trackIndex,
                    Mp4ContainerBox *outTrak, uint32_t *outErr);
};

class Mp4EsdsBox : public Mp4FullBox {
public:
    Mp4EsdsBox();
    virtual ~Mp4EsdsBox();

    uint32_t m_descriptorOffset; /* +32 */
};

class Mp4EsdsBoxReader : public Mp4FullBoxReader {
public:
    Mp4EsdsBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4EsdsBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4EsdsBox *outBox, uint32_t *outErr);
};

class Mp4MdhdBox : public Mp4FullBox {
public:
    Mp4MdhdBox();
    virtual ~Mp4MdhdBox();

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_timescale;        /* +40 */
    uint32_t m_duration;         /* +44 */
    uint32_t m_parsedEndOffset;  /* +48 */
};

class Mp4MdhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4MdhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4MdhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4MdhdBox *outBox, uint32_t *outErr);
};

class Mp4MvhdBox : public Mp4FullBox {
public:
    Mp4MvhdBox();
    virtual ~Mp4MvhdBox();

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_timescale;        /* +40 */
    uint32_t m_duration;         /* +44 */
    uint32_t m_parsedEndOffset;  /* +48 */
};

class Mp4MvhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4MvhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4MvhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4MvhdBox *outBox, uint32_t *outErr);
};

class Mp4TkhdBox : public Mp4FullBox {
public:
    Mp4TkhdBox();
    virtual ~Mp4TkhdBox();

    int isAudioTrackVolume() const;

    uint32_t m_creationTime;     /* +32 */
    uint32_t m_modificationTime; /* +36 */
    uint32_t m_trackId;          /* +40 */
    uint32_t m_duration;         /* +44 */
    uint8_t m_volumeBytes[2];    /* +48..+49 */
    uint8_t m_dimensionBytes[10];/* +50..+59 */
    uint32_t m_parsedEndOffset;  /* +60 */
};

class Mp4TkhdBoxReader : public Mp4FullBoxReader {
public:
    Mp4TkhdBoxReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4TkhdBoxReader();

    int read(const Mp4BoxBase *parentBox, Mp4TkhdBox *outBox, uint32_t *outErr);
};

class Mp4TableBox : public Mp4FullBox {
public:
    explicit Mp4TableBox(const char *fourcc);
    virtual ~Mp4TableBox();

    uint32_t m_entryCount;  /* +32 */
    uint32_t m_tableOffset; /* +36 */
};

class Mp4StcoReader : public Mp4FullBoxReader {
public:
    Mp4StcoReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StcoReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);
    uint32_t getChunkOffset(const Mp4TableBox *stco, uint32_t chunkIndex, uint32_t *outErr);
};

class Mp4StscReader : public Mp4FullBoxReader {
public:
    Mp4StscReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StscReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);

    inline uint32_t readEntryField(const Mp4TableBox *stsc, uint32_t entryIndex,
                                   uint32_t fieldOffset, uint32_t *outErr)
    {
        if (!stsc->isParsed()) {
            *outErr = MP4_BOX_ERR_NOT_READY;
            return 0;
        }
        if (entryIndex == 0u || entryIndex > stsc->m_entryCount) {
            *outErr = MP4_IO_ERR_BAD_PARAM;
            return 0;
        }

        uint32_t mapErr = 0xFFFFFFFFu;
        uint32_t off = stsc->m_fileOffset + stsc->m_tableOffset + 12u * (entryIndex - 1u) + fieldOffset;
        const uint8_t *ptr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
        if (ptr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        *outErr = 0;
        return readBigEndian32(ptr);
    }

    uint32_t getFirstChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
    uint32_t getSamplesPerChunk(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
    uint32_t getSampleDescIndex(const Mp4TableBox *stsc, uint32_t idx, uint32_t *err);
};

class Mp4StsdReader : public Mp4FullBoxReader {
public:
    Mp4StsdReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StsdReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);
    int readSampleEntry(const Mp4TableBox *stsd, const char *handlerType, uint32_t entryIndex,
                        size_t entryStructSize, Mp4AudioSampleEntryBox *outEntry, uint32_t *outErr);

private:
    Mp4AudioSampleEntryReader m_audioEntryReader; /* +12 */
};

class Mp4StszBox : public Mp4FullBox {
public:
    Mp4StszBox();
    virtual ~Mp4StszBox();

    uint32_t m_fixedSampleSize; /* +32 */
    uint32_t m_sampleCount;     /* +36 */
    uint32_t m_tableOffset;     /* +40 */
};

class Mp4StszReader : public Mp4FullBoxReader {
public:
    Mp4StszReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4StszReader();

    int read(const Mp4BoxBase *parentBox, Mp4StszBox *outBox, uint32_t *outErr);
    uint32_t getSampleSize(const Mp4StszBox *stsz, uint32_t sampleIndex, uint32_t *outErr);
};

class Mp4SttsReader : public Mp4FullBoxReader {
public:
    Mp4SttsReader(Mp4CachedIo *io, int streamIdx);
    virtual ~Mp4SttsReader();

    int read(const Mp4BoxBase *parentBox, Mp4TableBox *outBox, uint32_t *outErr);

    inline uint32_t readEntryField(const Mp4TableBox *stts, uint32_t entryIndex,
                                   uint32_t fieldOffset, uint32_t *outErr)
    {
        if (!stts->isParsed()) {
            *outErr = MP4_BOX_ERR_NOT_READY;
            return 0;
        }
        if (entryIndex == 0u || entryIndex > stts->m_entryCount) {
            *outErr = MP4_IO_ERR_BAD_PARAM;
            return 0;
        }

        uint32_t mapErr = 0xFFFFFFFFu;
        uint32_t off = stts->m_fileOffset + stts->m_tableOffset + 8u * (entryIndex - 1u) + fieldOffset;
        const uint8_t *ptr = (const uint8_t *)m_io->mapStreamRange(m_streamIdx, off, 4u, &mapErr);
        if (ptr == NULL) {
            *outErr = mapErr;
            return 0;
        }
        *outErr = 0;
        return readBigEndian32(ptr);
    }

    uint32_t getSampleCount(const Mp4TableBox *stts, uint32_t idx, uint32_t *err);
    uint32_t getSampleDelta(const Mp4TableBox *stts, uint32_t idx, uint32_t *err);
};

class Mp4SampleTableNavigator {
public:
    Mp4SampleTableNavigator(Mp4SttsReader *sttsReader, Mp4StszReader *stszReader,
                            Mp4StscReader *stscReader, Mp4StcoReader *stcoReader);
    virtual ~Mp4SampleTableNavigator();

    void attachStts(Mp4TableBox *stts);
    void attachStsz(Mp4StszBox *stsz);
    void attachStsc(Mp4TableBox *stsc);
    void attachStco(Mp4TableBox *stco);

    int lookupChunkOffset(uint32_t chunkIndex, uint32_t *outOffset, uint32_t *outErr);
    int sumSampleSizes(uint32_t firstSample, uint32_t lastSample, uint32_t *outTotalBytes,
                       uint32_t *outErr);
    int lookupSampleChunk(uint32_t sampleIndex, uint32_t *outChunkIndex,
                          uint32_t *outFirstSampleInChunk, uint32_t *outSamplesPerChunk,
                          uint32_t *outSampleDescIndex, uint32_t *outErr);
    int lookupSampleLocation(uint32_t sampleIndex, uint32_t *outFileOffset,
                             uint32_t *outSampleSize, uint32_t *outSampleDescIndex,
                             uint32_t *outErr);
    int lookupSampleByTimestamp(uint32_t targetTimeTicks, uint32_t *outSampleIndex,
                                uint32_t *outErr);

private:
    Mp4SttsReader *m_sttsReader;      /* +4 */
    Mp4TableBox *m_sttsBox;           /* +8 */
    uint32_t m_sttsEntryIdx;          /* +12 */
    uint32_t m_sttsFirstSample;       /* +16 */
    uint32_t m_sttsStartTime;         /* +20 */
    uint32_t m_sttsSampleCount;       /* +24 */
    uint32_t m_sttsSampleDelta;       /* +28 */

    Mp4StszReader *m_stszReader;      /* +32 */
    Mp4StszBox *m_stszBox;            /* +36 */

    Mp4StscReader *m_stscReader;      /* +40 */
    Mp4TableBox *m_stscBox;           /* +44 */
    uint32_t m_stscEntryIdx;          /* +48 */
    uint32_t m_stscFirstChunk;        /* +52 */
    uint32_t m_stscSamplesPerChunk;   /* +56 */
    uint32_t m_stscSampleDescIdx;     /* +60 */
    uint32_t m_stscFirstSample;       /* +64 */
    uint32_t m_stscNextFirstChunk;    /* +68 */

    uint32_t m_cachedSizeSum;         /* +72 */
    uint32_t m_cachedSumFirst;        /* +76 */
    uint32_t m_cachedSumLast;         /* +80 */

    Mp4StcoReader *m_stcoReader;      /* +84 */
    Mp4TableBox *m_stcoBox;           /* +88 */
    uint32_t m_cachedChunkIdx;        /* +92 */
    uint32_t m_cachedChunkOffset;     /* +96 */
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_MP4_BOXES_H */
