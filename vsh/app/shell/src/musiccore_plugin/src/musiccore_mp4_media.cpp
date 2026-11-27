/*
 * SceShell musiccore_plugin - Mp4MediaInfo and Mp4StreamParser (ISO BMFF /
 * MP4 audio track box traversal, sample table lookup, and AAC/SBR framing).
 * Original address range (3.65-CEX): 0x811EBA70 - 0x811EDBD3
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

extern void *allocFromHeap(size_t size);
extern int mapMp4ParserError(int parserErr);

static const uint32_t FOURCC_MP4 = 0x2034504Du;

static const int MP4_ERR_OPEN_FAIL = -2146394111;   /* 0x8010A001 */
static const int MP4_ERR_OPEN_IO = -2146394109;     /* 0x8010A003 */
static const int MP4_ERR_READ_IO = -2146394108;     /* 0x8010A004 */
static const int MP4_ERR_NOT_OPEN = -2146394107;    /* 0x8010A005 */
static const int MP4_ERR_EOF = -2146394105;         /* 0x8010A007 */
static const int MP4_ERR_UNSUPPORTED = -2144862175; /* 0x80280021 */
static const int MP4_ERR_CORRUPT = -2144862174;     /* 0x80280022 */
static const int MP4_ERR_BOX_EOF = -2144862168;     /* 0x80280028 */

struct TrackMetadataResult {
    int channels;
    uint8_t hasChannels;
    uint8_t padChannels[3];
    int bitRate;
    uint8_t hasBitRate;
    uint8_t padBitRate[3];
    unsigned int durationMs;
    uint8_t hasDuration;
    uint8_t padDuration[3];
    int sampleRate;
    uint8_t hasSampleRate;
    uint8_t padSampleRate[3];
    int codecProfile;
    uint8_t hasCodecProfile;
    uint8_t padCodecProfile[3];
    int codecSubType;
    uint8_t hasCodecSubType;
    uint8_t padCodecSubType[3];
};

struct StreamIoInterface {
    virtual ~StreamIoInterface() {}
    virtual int isInitialized() = 0;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int isOpen() = 0;
    virtual int getSize() = 0;
    virtual int getPosition() = 0;
    virtual int setPath(const char *path) = 0;
    virtual int setPriorityMode(uint32_t priorityMode) = 0;
    virtual int applyOpenFilePriority(int priorityMode) = 0;
    virtual int flushCache() = 0;
    virtual int setIoHint(int hintMode, int hintParam) = 0;
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, int *outErr, int *outMapped) = 0;
    virtual int mapRange(int offset, unsigned int size, int flags, int *outMapped,
                         int *outErr, int extra) = 0;
    virtual int unpinRange(int pinToken, int *outErr) = 0;
    virtual int getCacheGeometry(int *outBlockCount, uint32_t *outChunkSize) = 0;
    virtual int checkRangeReadable(int sizeBytes, int *outErr) = 0;
};

extern StreamIoInterface *createLocalCacheIoEx(const char *path, int chunkSize, int blockCount,
                                               int readAhead, int extraChunk, int extraBlocks);

struct Mp4FramePacket {
    void *frameData;
    unsigned int frameSize;
    int sampleRate;
    int samplesPerFrame;
    uint8_t flags;
    uint8_t channelCount;
    uint8_t padChannel[6];
    uint16_t leadingSkip;
    uint16_t trailingSkip;
};

/* Box data and reader storage structs (implemented in musiccore_mp4_boxes.cpp) */
struct Mp4BoxNode {
    uint8_t storage[28];
};

struct Mp4MvhdBox {
    uint8_t storage[52];
};

struct Mp4HdlrBox {
    uint8_t storage[64];
};

struct Mp4MdhdBox {
    uint8_t storage[52];
};

struct Mp4StsdBox {
    uint8_t storage[40];
};

struct Mp4SampleEntryBox {
    uint8_t storage[40];
};

struct Mp4Mp4aBox {
    uint8_t storage[36];
};

struct Mp4EsdsHeaderBox {
    uint8_t storage[36];
};

struct Mp4DecoderConfigBox {
    uint8_t storage[48];
};

struct Mp4AudioSpecConfig {
    uint8_t storage[56];
};

struct Mp4StblBox {
    uint8_t storage[28];
};

struct Mp4SttsBox {
    uint8_t storage[40];
};

struct Mp4StszBox {
    uint8_t storage[44];
};

struct Mp4StscBox {
    uint8_t storage[40];
};

struct Mp4StcoBox {
    uint8_t storage[40];
};

struct Mp4BoxReader {
    uint8_t storage[12];
};

struct Mp4StsdBoxReader {
    uint8_t storage[24];
};

struct Mp4SampleTable {
    uint8_t storage[100];
};

extern void mp4BoxReset(void *box);

extern void mp4MoovBoxInit(Mp4BoxNode *box);
extern void mp4MoovBoxDestroy(Mp4BoxNode *box);
extern void mp4MoovReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4MoovReaderDestroy(Mp4BoxReader *reader);
extern int mp4MoovReaderRead(Mp4BoxReader *reader, Mp4BoxNode *outBox, unsigned int *outErr);

extern void mp4MvhdBoxInit(Mp4MvhdBox *box);
extern void mp4MvhdBoxDestroy(Mp4MvhdBox *box);
extern void mp4MvhdReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4MvhdReaderDestroy(Mp4BoxReader *reader);
extern int mp4MvhdReaderRead(Mp4BoxReader *reader, Mp4BoxNode *moov, Mp4MvhdBox *outBox, unsigned int *outErr);

extern void mp4TrakBoxInit(Mp4BoxNode *box);
extern void mp4TrakBoxDestroy(Mp4BoxNode *box);
extern void mp4TrakReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4TrakReaderDestroy(Mp4BoxReader *reader);
extern int mp4TrakReaderCount(Mp4BoxReader *reader, Mp4BoxNode *moov, unsigned int *outErr);
extern int mp4TrakReaderGet(Mp4BoxReader *reader, Mp4BoxNode *moov, int index, Mp4BoxNode *outBox, unsigned int *outErr);

extern void mp4HdlrBoxInit(Mp4HdlrBox *box);
extern void mp4HdlrBoxDestroy(Mp4HdlrBox *box);
extern int mp4HdlrIsSoundTrack(const Mp4HdlrBox *box);
extern void mp4HdlrReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4HdlrReaderDestroy(Mp4BoxReader *reader);
extern int mp4HdlrReaderRead(Mp4BoxReader *reader, Mp4BoxNode *trak, Mp4HdlrBox *outBox, unsigned int *outErr);

extern void mp4MdiaBoxInit(Mp4BoxNode *box);
extern void mp4MdiaBoxDestroy(Mp4BoxNode *box);
extern void mp4MdiaReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4MdiaReaderDestroy(Mp4BoxReader *reader);

extern void mp4MdhdBoxInit(Mp4MdhdBox *box);
extern void mp4MdhdBoxDestroy(Mp4MdhdBox *box);
extern void mp4MdhdReaderInit(Mp4BoxReader *reader);
extern void mp4MdhdReaderDestroy(Mp4BoxReader *reader);
extern int mp4MdhdReaderRead(Mp4BoxReader *reader, Mp4BoxNode *mdia, Mp4MdhdBox *outBox, unsigned int *outErr);
extern unsigned int mp4MdhdGetTimeScale(const Mp4MdhdBox *box);
extern unsigned int mp4MdhdGetDuration(const Mp4MdhdBox *box);

extern void mp4MinfBoxInit(Mp4BoxNode *box);
extern void mp4MinfBoxDestroy(Mp4BoxNode *box);
extern void mp4MinfReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4MinfReaderDestroy(Mp4BoxReader *reader);

extern void mp4SmhdBoxInit(Mp4BoxNode *box);
extern void mp4SmhdBoxDestroy(Mp4BoxNode *box);
extern void mp4SmhdReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4SmhdReaderDestroy(Mp4BoxReader *reader);
extern int mp4ChildBoxRead(Mp4BoxReader *reader, Mp4BoxNode *parent, void *outChild, unsigned int *outErr);
extern unsigned int mp4BoxGetPayloadSize(const void *box);
extern unsigned int mp4BoxGetPayloadOffset(const void *box);
extern unsigned int mp4BoxGetHeaderSize(const void *box);
extern unsigned int mp4BoxGetEntryCount(const void *box);
extern unsigned int mp4BoxGetInnerOffset(const void *box);

extern void mp4StblBoxInit(Mp4StblBox *box);
extern void mp4StblBoxDestroy(Mp4StblBox *box);
extern void mp4StblReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4StblReaderDestroy(Mp4BoxReader *reader);

extern void mp4StsdBoxInit(Mp4StsdBox *box);
extern void mp4StsdBoxDestroy(Mp4StsdBox *box);
extern void mp4StsdReaderInit(Mp4StsdBoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4StsdReaderDestroy(Mp4StsdBoxReader *reader);
extern int mp4StsdReaderRead(Mp4StsdBoxReader *reader, Mp4StblBox *stbl, Mp4StsdBox *outBox, unsigned int *outErr);
extern int mp4StsdReaderFindEntry(Mp4StsdBoxReader *reader, Mp4StsdBox *stsd, const char *handler,
                                  int index, int entrySize, Mp4SampleEntryBox *outEntry, unsigned int *outErr);

extern void mp4SampleEntryInit(Mp4SampleEntryBox *box);
extern void mp4SampleEntryDestroy(Mp4SampleEntryBox *box);

extern void mp4Mp4aBoxInit(Mp4Mp4aBox *box);
extern void mp4Mp4aBoxDestroy(Mp4Mp4aBox *box);
extern void mp4Mp4aReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4Mp4aReaderDestroy(Mp4BoxReader *reader);
extern int mp4Mp4aReaderRead(Mp4BoxReader *reader, Mp4SampleEntryBox *entry, Mp4Mp4aBox *outBox, unsigned int *outErr);

extern void mp4EsdsBoxInit(Mp4EsdsHeaderBox *box);
extern void mp4EsdsBoxDestroy(Mp4EsdsHeaderBox *box);
extern void mp4EsdsReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4EsdsReaderDestroy(Mp4BoxReader *reader);
extern int mp4EsdsReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                             Mp4EsdsHeaderBox *outBox, unsigned int *outErr);

extern void mp4DecoderConfigInit(Mp4DecoderConfigBox *box);
extern void mp4DecoderConfigDestroy(Mp4DecoderConfigBox *box);
extern void mp4DecoderConfigReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4DecoderConfigReaderDestroy(Mp4BoxReader *reader);
extern int mp4DecoderConfigReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                                      Mp4DecoderConfigBox *outBox, unsigned int *outErr);
extern int mp4DecoderConfigGetObjectType(const Mp4DecoderConfigBox *box);

extern void mp4AudioSpecConfigInit(Mp4AudioSpecConfig *cfg);
extern void mp4AudioSpecConfigDestroy(Mp4AudioSpecConfig *cfg);
extern void mp4AudioSpecConfigReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4AudioSpecConfigReaderDestroy(Mp4BoxReader *reader);
extern int mp4AudioSpecConfigReaderRead(Mp4BoxReader *reader, unsigned int offset, unsigned int size,
                                        Mp4AudioSpecConfig *outCfg, unsigned int *outErr);
extern int mp4AudioSpecConfigHasSbr(const Mp4AudioSpecConfig *cfg);
extern int mp4AudioSpecConfigGetExtSampleRate(const Mp4AudioSpecConfig *cfg);
extern int mp4AudioSpecConfigGetBaseSampleRate(const Mp4AudioSpecConfig *cfg);
extern uint8_t mp4AudioSpecConfigGetChannels(const Mp4AudioSpecConfig *cfg);
extern int mp4ValidateAudioTrack(const Mp4MdhdBox *mdhd, const Mp4StsdBox *stsd,
                                 const Mp4DecoderConfigBox *decCfg, const Mp4AudioSpecConfig *asc);

extern void mp4SttsBoxInit(Mp4SttsBox *box);
extern void mp4SttsBoxDestroy(Mp4SttsBox *box);
extern void mp4SttsReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4SttsReaderDestroy(Mp4BoxReader *reader);
extern int mp4SttsReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4SttsBox *outBox, unsigned int *outErr);

extern void mp4StszBoxInit(Mp4StszBox *box);
extern void mp4StszBoxDestroy(Mp4StszBox *box);
extern void mp4StszReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4StszReaderDestroy(Mp4BoxReader *reader);
extern int mp4StszReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StszBox *outBox, unsigned int *outErr);
extern unsigned int mp4StszGetSampleCount(const Mp4StszBox *box);

extern void mp4StscBoxInit(Mp4StscBox *box);
extern void mp4StscBoxDestroy(Mp4StscBox *box);
extern void mp4StscReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4StscReaderDestroy(Mp4BoxReader *reader);
extern int mp4StscReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StscBox *outBox, unsigned int *outErr);

extern void mp4StcoBoxInit(Mp4StcoBox *box);
extern void mp4StcoBoxDestroy(Mp4StcoBox *box);
extern void mp4StcoReaderInit(Mp4BoxReader *reader, StreamIoInterface *io, int mode);
extern void mp4StcoReaderDestroy(Mp4BoxReader *reader);
extern int mp4StcoReaderRead(Mp4BoxReader *reader, Mp4StblBox *stbl, Mp4StcoBox *outBox, unsigned int *outErr);

extern void mp4SampleTableInit(Mp4SampleTable *tbl, Mp4BoxReader *sttsRdr, Mp4BoxReader *stszRdr,
                               Mp4BoxReader *stscRdr, Mp4BoxReader *stcoRdr);
extern void mp4SampleTableDestroy(Mp4SampleTable *tbl);
extern void mp4SampleTableSetStts(Mp4SampleTable *tbl, Mp4SttsBox *box);
extern void mp4SampleTableSetStsz(Mp4SampleTable *tbl, Mp4StszBox *box);
extern void mp4SampleTableSetStsc(Mp4SampleTable *tbl, Mp4StscBox *box);
extern void mp4SampleTableSetStco(Mp4SampleTable *tbl, Mp4StcoBox *box);
extern int mp4SampleTableGetSample(Mp4SampleTable *tbl, unsigned int sampleIdx,
                                   int *outOffset, unsigned int *outSize, int *outTime, int *outErr);
extern int mp4SampleTableFindByTime(Mp4SampleTable *tbl, unsigned int targetTicks,
                                    unsigned int *outSampleIdx, int *outErr);

static void normalizeBoxIoError(unsigned int *err)
{
    if (*err == (unsigned int)MP4_ERR_EOF || *err == (unsigned int)MP4_ERR_BOX_EOF) {
        *err = (unsigned int)MP4_ERR_CORRUPT;
    }
}

class Mp4StreamParser {
public:
    StreamIoInterface *ioStream;
    int openStatus;
    unsigned int currentSampleIndex;
    Mp4BoxReader mdhdReader;
    Mp4BoxReader stblReader;
    Mp4BoxReader sttsReader;
    Mp4BoxReader stszReader;
    Mp4BoxReader stscReader;
    Mp4BoxReader stcoReader;
    Mp4MdhdBox mdhdBox;
    Mp4StblBox stblBox;
    Mp4SttsBox sttsBox;
    Mp4StszBox stszBox;
    Mp4StscBox stscBox;
    Mp4StcoBox stcoBox;
    Mp4DecoderConfigBox decoderConfigBox;
    Mp4AudioSpecConfig audioSpecConfig;
    Mp4SampleTable sampleTable;

    /* 0x811EBCCC */
    explicit Mp4StreamParser(StreamIoInterface *io)
        : ioStream(io),
          openStatus(-1),
          currentSampleIndex(0)
    {
        mp4MdhdReaderInit(&mdhdReader);
        mp4StblReaderInit(&stblReader, ioStream, 1);
        mp4SttsReaderInit(&sttsReader, ioStream, 1);
        mp4StszReaderInit(&stszReader, ioStream, 2);
        mp4StscReaderInit(&stscReader, ioStream, 3);
        mp4StcoReaderInit(&stcoReader, ioStream, 4);
        mp4MdhdBoxInit(&mdhdBox);
        mp4StblBoxInit(&stblBox);
        mp4SttsBoxInit(&sttsBox);
        mp4StszBoxInit(&stszBox);
        mp4StscBoxInit(&stscBox);
        mp4StcoBoxInit(&stcoBox);
        mp4DecoderConfigInit(&decoderConfigBox);
        mp4AudioSpecConfigInit(&audioSpecConfig);
        mp4SampleTableInit(&sampleTable, &sttsReader, &stszReader, &stscReader, &stcoReader);
    }

    /* 0x811EBD90 / 0x811EBE1E */
    virtual ~Mp4StreamParser()
    {
        mp4SampleTableDestroy(&sampleTable);
        mp4AudioSpecConfigDestroy(&audioSpecConfig);
        mp4DecoderConfigDestroy(&decoderConfigBox);
        mp4StcoBoxDestroy(&stcoBox);
        mp4StscBoxDestroy(&stscBox);
        mp4StszBoxDestroy(&stszBox);
        mp4SttsBoxDestroy(&sttsBox);
        mp4StblBoxDestroy(&stblBox);
        mp4MdhdBoxDestroy(&mdhdBox);
        mp4StcoReaderDestroy(&stcoReader);
        mp4StscReaderDestroy(&stscReader);
        mp4StszReaderDestroy(&stszReader);
        mp4SttsReaderDestroy(&sttsReader);
        mp4StblReaderDestroy(&stblReader);
        mp4MdhdReaderDestroy(&mdhdReader);
    }

    /* 0x811EBE30 */
    int closeStream()
    {
        if (ioStream == NULL) {
            return -1;
        }
        mp4BoxReset(&decoderConfigBox);
        mp4BoxReset(&audioSpecConfig);
        mp4BoxReset(&stcoBox);
        mp4BoxReset(&stscBox);
        mp4BoxReset(&stszBox);
        mp4BoxReset(&sttsBox);
        mp4BoxReset(&stblBox);
        mp4BoxReset(&mdhdBox);

        int ret = 0;
        if (ioStream->isOpen() == 1) {
            ret = ioStream->close();
            if (ret == 0) {
                openStatus = -1;
            }
        }
        return ret;
    }

    /* 0x811EBEA2 */
    int openStream(unsigned int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = 0xFFFFFFFFu;
            return -1;
        }
        if (openStatus >= 0) {
            *outErr = 0;
            return openStatus;
        }

        int openRet = ioStream->open();
        if (openRet < 0) {
            *outErr = (unsigned int)MP4_ERR_OPEN_FAIL;
            return openRet;
        }
        openStatus = openRet;

        Mp4BoxNode moovBox;
        Mp4BoxReader moovReader;
        mp4MoovBoxInit(&moovBox);
        mp4MoovReaderInit(&moovReader, ioStream, 1);

        int result = -1;
        bool ok = true;

        if (mp4MoovReaderRead(&moovReader, &moovBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }

        Mp4MvhdBox mvhdBox;
        Mp4BoxReader mvhdReader;
        mp4MvhdBoxInit(&mvhdBox);
        mp4MvhdReaderInit(&mvhdReader, ioStream, 1);
        if (ok && mp4MvhdReaderRead(&mvhdReader, &moovBox, &mvhdBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }

        Mp4BoxNode trakBox;
        Mp4BoxReader trakReader;
        Mp4HdlrBox hdlrBox;
        Mp4BoxReader hdlrReader;
        mp4TrakBoxInit(&trakBox);
        mp4TrakReaderInit(&trakReader, ioStream, 1);
        mp4HdlrBoxInit(&hdlrBox);
        mp4HdlrReaderInit(&hdlrReader, ioStream, 1);

        if (ok) {
            int trackCount = mp4TrakReaderCount(&trakReader, &moovBox, outErr);
            if (*outErr != 0) {
                normalizeBoxIoError(outErr);
                ok = false;
            } else if (trackCount < 1) {
                *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
                ok = false;
            } else {
                bool foundSound = false;
                for (int idx = 1; idx <= trackCount; ++idx) {
                    if (mp4TrakReaderGet(&trakReader, &moovBox, idx, &trakBox, outErr) == 0) {
                        normalizeBoxIoError(outErr);
                        ok = false;
                        break;
                    }
                    if (mp4HdlrReaderRead(&hdlrReader, &trakBox, &hdlrBox, outErr) == 0) {
                        normalizeBoxIoError(outErr);
                        ok = false;
                        break;
                    }
                    if (mp4HdlrIsSoundTrack(&hdlrBox) != 0) {
                        foundSound = true;
                        break;
                    }
                }
                if (ok && !foundSound) {
                    *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
                    ok = false;
                }
            }
        }

        Mp4BoxNode mdiaBox;
        Mp4BoxReader mdiaReader;
        mp4MdiaBoxInit(&mdiaBox);
        mp4MdiaReaderInit(&mdiaReader, ioStream, 1);
        if (ok && mp4ChildBoxRead(&mdiaReader, &trakBox, &mdiaBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }
        if (ok && mp4MdhdReaderRead(&mdhdReader, &mdiaBox, &mdhdBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }

        Mp4BoxNode minfBox;
        Mp4BoxReader minfReader;
        mp4MinfBoxInit(&minfBox);
        mp4MinfReaderInit(&minfReader, ioStream, 1);
        if (ok && mp4ChildBoxRead(&minfReader, &mdiaBox, &minfBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }

        Mp4BoxNode smhdBox;
        Mp4BoxReader smhdReader;
        mp4SmhdBoxInit(&smhdBox);
        mp4SmhdReaderInit(&smhdReader, ioStream, 1);
        if (ok) {
            if (mp4ChildBoxRead(&smhdReader, &minfBox, &smhdBox, outErr) == 0) {
                normalizeBoxIoError(outErr);
                ok = false;
            } else if (mp4BoxGetPayloadSize(&smhdBox) != 36u) {
                *outErr = (unsigned int)MP4_ERR_CORRUPT;
                ok = false;
            } else if (mp4ChildBoxRead(&stblReader, &minfBox, &stblBox, outErr) == 0) {
                normalizeBoxIoError(outErr);
                ok = false;
            }
        }

        Mp4StsdBox stsdBox;
        Mp4StsdBoxReader stsdReader;
        mp4StsdBoxInit(&stsdBox);
        mp4StsdReaderInit(&stsdReader, ioStream, 1);
        if (ok) {
            if (mp4StsdReaderRead(&stsdReader, &stblBox, &stsdBox, outErr) == 0) {
                normalizeBoxIoError(outErr);
                ok = false;
            } else if (mp4BoxGetEntryCount(&stsdBox) != 1u) {
                *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
                ok = false;
            }
        }

        Mp4SampleEntryBox sampleEntry;
        mp4SampleEntryInit(&sampleEntry);
        if (ok && mp4StsdReaderFindEntry(&stsdReader, &stsdBox, "soun@", 1, 40, &sampleEntry, outErr) == 0) {
            ok = false;
        }

        Mp4Mp4aBox mp4aBox;
        Mp4BoxReader mp4aReader;
        mp4Mp4aBoxInit(&mp4aBox);
        mp4Mp4aReaderInit(&mp4aReader, ioStream, 1);
        unsigned int esdsStart = 0;
        unsigned int esdsLen = 0;
        if (ok) {
            if (mp4Mp4aReaderRead(&mp4aReader, &sampleEntry, &mp4aBox, outErr) == 0) {
                ok = false;
            } else {
                unsigned int mp4aOff = mp4BoxGetPayloadOffset(&mp4aBox);
                unsigned int mp4aHdr = mp4BoxGetHeaderSize(&mp4aBox);
                unsigned int mp4aEnd = mp4aOff + mp4BoxGetPayloadSize(&mp4aBox);
                esdsStart = mp4aOff + mp4aHdr;
                if (esdsStart > mp4aEnd) {
                    *outErr = (unsigned int)MP4_ERR_CORRUPT;
                    ok = false;
                } else {
                    esdsLen = mp4aEnd - esdsStart;
                }
            }
        }

        Mp4EsdsHeaderBox esdsBox;
        Mp4BoxReader esdsReader;
        mp4EsdsBoxInit(&esdsBox);
        mp4EsdsReaderInit(&esdsReader, ioStream, 1);
        unsigned int decCfgStart = 0;
        unsigned int decCfgLen = 0;
        if (ok) {
            if (mp4EsdsReaderRead(&esdsReader, esdsStart, esdsLen, &esdsBox, outErr) == 0) {
                ok = false;
            } else {
                unsigned int esdsOff = mp4BoxGetInnerOffset(&esdsBox);
                unsigned int esdsHdr = mp4BoxGetHeaderSize(&esdsBox);
                unsigned int esdsEnd = esdsOff + mp4BoxGetPayloadOffset(&esdsBox);
                decCfgStart = esdsOff + esdsHdr;
                if (decCfgStart > esdsEnd) {
                    *outErr = (unsigned int)MP4_ERR_CORRUPT;
                    ok = false;
                } else {
                    decCfgLen = esdsEnd - decCfgStart;
                }
            }
        }

        Mp4BoxReader decCfgReader;
        mp4DecoderConfigReaderInit(&decCfgReader, ioStream, 1);
        unsigned int ascStart = 0;
        unsigned int ascLen = 0;
        if (ok) {
            if (mp4DecoderConfigReaderRead(&decCfgReader, decCfgStart, decCfgLen, &decoderConfigBox, outErr) == 0) {
                ok = false;
            } else if (mp4DecoderConfigGetObjectType(&decoderConfigBox) != 64) {
                *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
                ok = false;
            } else {
                unsigned int cfgOff = mp4BoxGetInnerOffset(&decoderConfigBox);
                unsigned int cfgEnd = cfgOff + mp4BoxGetPayloadOffset(&decoderConfigBox);
                ascStart = cfgOff + mp4MdhdGetDuration(reinterpret_cast<const Mp4MdhdBox *>(&decoderConfigBox));
                if (ascStart > cfgEnd) {
                    *outErr = (unsigned int)MP4_ERR_CORRUPT;
                    ok = false;
                } else {
                    ascLen = cfgEnd - ascStart;
                }
            }
        }

        Mp4BoxReader ascReader;
        mp4AudioSpecConfigReaderInit(&ascReader, ioStream, 1);
        if (ok) {
            if (mp4AudioSpecConfigReaderRead(&ascReader, ascStart, ascLen, &audioSpecConfig, outErr) == 0) {
                ok = false;
            } else if (mp4ValidateAudioTrack(&mdhdBox, &stsdBox, &decoderConfigBox, &audioSpecConfig) == 0) {
                *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
                ok = false;
            }
        }

        if (ok && mp4SttsReaderRead(&sttsReader, &stblBox, &sttsBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }
        if (ok && mp4StszReaderRead(&stszReader, &stblBox, &stszBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }
        if (ok && mp4StscReaderRead(&stscReader, &stblBox, &stscBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }
        if (ok && mp4StcoReaderRead(&stcoReader, &stblBox, &stcoBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }

        if (ok) {
            mp4SampleTableSetStts(&sampleTable, &sttsBox);
            mp4SampleTableSetStsz(&sampleTable, &stszBox);
            mp4SampleTableSetStsc(&sampleTable, &stscBox);
            mp4SampleTableSetStco(&sampleTable, &stcoBox);
            result = openRet;
        } else {
            closeStream();
        }

        mp4AudioSpecConfigReaderDestroy(&ascReader);
        mp4DecoderConfigReaderDestroy(&decCfgReader);
        mp4EsdsReaderDestroy(&esdsReader);
        mp4EsdsBoxDestroy(&esdsBox);
        mp4Mp4aReaderDestroy(&mp4aReader);
        mp4Mp4aBoxDestroy(&mp4aBox);
        mp4SampleEntryDestroy(&sampleEntry);
        mp4StsdReaderDestroy(&stsdReader);
        mp4StsdBoxDestroy(&stsdBox);
        mp4SmhdReaderDestroy(&smhdReader);
        mp4SmhdBoxDestroy(&smhdBox);
        mp4MinfReaderDestroy(&minfReader);
        mp4MinfBoxDestroy(&minfBox);
        mp4MdiaReaderDestroy(&mdiaReader);
        mp4MdiaBoxDestroy(&mdiaBox);
        mp4HdlrReaderDestroy(&hdlrReader);
        mp4HdlrBoxDestroy(&hdlrBox);
        mp4TrakReaderDestroy(&trakReader);
        mp4TrakBoxDestroy(&trakBox);
        mp4MvhdReaderDestroy(&mvhdReader);
        mp4MvhdBoxDestroy(&mvhdBox);
        mp4MoovReaderDestroy(&moovReader);
        mp4MoovBoxDestroy(&moovBox);
        return result;
    }

    /* 0x811ED282 */
    int getStreamPosition()
    {
        if (ioStream == NULL) {
            return 0;
        }
        return ioStream->getPosition();
    }

    /* 0x811ED294 */
    int setIoHint(int mode)
    {
        if (ioStream == NULL) {
            return 0;
        }
        switch (mode) {
        case 4:
            ioStream->setIoHint(3, 0);
            return 1;
        case 2:
            ioStream->setIoHint(2, 0);
            return 1;
        case 1:
            ioStream->setIoHint(1, 0);
            return 1;
        case 0:
            ioStream->setIoHint(0, 0);
            return 1;
        default:
            return 0;
        }
    }

    int resolveEffectiveSampleRate() const
    {
        int rate = 0;
        if (mp4AudioSpecConfigHasSbr(&audioSpecConfig) != 0) {
            rate = mp4AudioSpecConfigGetExtSampleRate(&audioSpecConfig);
        }
        if (rate == 0) {
            rate = mp4AudioSpecConfigGetBaseSampleRate(&audioSpecConfig);
        }
        return rate;
    }

    /* 0x811ED2E8 */
    int getDurationMs(unsigned int *outDurationMs, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = resolveEffectiveSampleRate();
        unsigned int timeScale = mp4MdhdGetTimeScale(&mdhdBox);
        unsigned int duration = mp4MdhdGetDuration(&mdhdBox);
        if (rate == 0) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        *outDurationMs = (unsigned int)(((float)duration * 1000.0f) / (float)timeScale);
        *outErr = 0;
        return 1;
    }

    /* 0x811ED39C */
    int getBaseSampleRate(int *outRate, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = mp4AudioSpecConfigGetBaseSampleRate(&audioSpecConfig);
        *outRate = rate;
        if (rate == 0) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        *outErr = 0;
        return 1;
    }

    /* 0x811ED3EC */
    int getSbrSampleRate(int *outRate, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = 0;
        if (mp4AudioSpecConfigHasSbr(&audioSpecConfig) != 0) {
            rate = mp4AudioSpecConfigGetExtSampleRate(&audioSpecConfig);
        }
        *outRate = rate;
        if (rate == 0 && mp4AudioSpecConfigHasSbr(&audioSpecConfig) != 0) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        *outErr = 0;
        return 1;
    }

    /* 0x811ED454 */
    int getHasSbr(uint8_t *outHasSbr, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        *outHasSbr = (uint8_t)mp4AudioSpecConfigHasSbr(&audioSpecConfig);
        *outErr = 0;
        return 1;
    }

    /* 0x811ED496 */
    int getSamplesPerFrame(int16_t *outSamples, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        *outSamples = (mp4AudioSpecConfigHasSbr(&audioSpecConfig) != 0) ? 2048 : 1024;
        *outErr = 0;
        return 1;
    }

    int loadSampleAtIndex(unsigned int sampleIdx, int effectiveRate, Mp4FramePacket *outPacket, int *outErr)
    {
        int fileOffset = 0;
        unsigned int sampleSize = 0;
        int sampleTime = 0;
        if (mp4SampleTableGetSample(&sampleTable, sampleIdx, &fileOffset, &sampleSize, &sampleTime, outErr) == 0) {
            return 0;
        }
        if (sampleSize > 0x7FFFFFFFu) {
            *outErr = MP4_ERR_UNSUPPORTED;
            return 0;
        }

        int mappedLen = 0;
        int ioErr = -1;
        int dataPtr = ioStream->mapRange(fileOffset, sampleSize, 0, &mappedLen, &ioErr, 0);
        if (dataPtr == 0) {
            *outErr = (ioErr == MP4_ERR_EOF) ? MP4_ERR_CORRUPT : ioErr;
            return 0;
        }

        outPacket->frameData = reinterpret_cast<void *>(dataPtr);
        outPacket->frameSize = sampleSize;
        outPacket->sampleRate = effectiveRate;
        outPacket->samplesPerFrame = (mp4AudioSpecConfigHasSbr(&audioSpecConfig) != 0) ? 2048 : 1024;
        outPacket->flags = 0;
        outPacket->leadingSkip = 0;
        outPacket->trailingSkip = 0;
        outPacket->channelCount = mp4AudioSpecConfigGetChannels(&audioSpecConfig);
        currentSampleIndex = sampleIdx;
        *outErr = 0;
        return 1;
    }

    /* 0x811ED4E2 */
    int readFirstSample(Mp4FramePacket *outPacket, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = resolveEffectiveSampleRate();
        if (rate == 0 || mp4StszGetSampleCount(&stszBox) == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        return loadSampleAtIndex(1u, rate, outPacket, outErr);
    }

    /* 0x811ED650 */
    int readNextSample(int skipCount, Mp4FramePacket *outPacket, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = resolveEffectiveSampleRate();
        unsigned int totalSamples = mp4StszGetSampleCount(&stszBox);
        if (rate == 0 || totalSamples == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        unsigned int nextIdx = currentSampleIndex + (unsigned int)skipCount + 1u;
        if (nextIdx > totalSamples) {
            *outErr = MP4_ERR_EOF;
            return 0;
        }
        return loadSampleAtIndex(nextIdx, rate, outPacket, outErr);
    }

    /* 0x811ED7E0 */
    int readPrevSample(int skipCount, Mp4FramePacket *outPacket, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = resolveEffectiveSampleRate();
        if (rate == 0 || mp4StszGetSampleCount(&stszBox) == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }
        if (currentSampleIndex <= (unsigned int)(skipCount + 1)) {
            *outErr = MP4_ERR_EOF;
            return 0;
        }
        unsigned int prevIdx = currentSampleIndex - (unsigned int)skipCount - 1u;
        return loadSampleAtIndex(prevIdx, rate, outPacket, outErr);
    }

    /* 0x811ED976 */
    int seekToTimeMs(unsigned int targetMs, Mp4FramePacket *outPacket, unsigned int *outSampleIdx, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        if (ioStream->isOpen() == 0) {
            *outErr = MP4_ERR_NOT_OPEN;
            return 0;
        }
        int rate = resolveEffectiveSampleRate();
        unsigned int timeScale = mp4MdhdGetTimeScale(&mdhdBox);
        unsigned int totalSamples = mp4StszGetSampleCount(&stszBox);
        if (rate == 0 || timeScale == 0u || totalSamples == 0u) {
            *outErr = MP4_ERR_CORRUPT;
            return 0;
        }

        unsigned int targetTicks = (unsigned int)(((float)targetMs / 1000.0f) * (float)timeScale);
        unsigned int foundIdx = 0;
        if (mp4SampleTableFindByTime(&sampleTable, targetTicks, &foundIdx, outErr) == 0) {
            if (*outErr == MP4_ERR_BOX_EOF) {
                *outErr = MP4_ERR_EOF;
            }
            return 0;
        }
        if (foundIdx == 0u || foundIdx > totalSamples) {
            *outErr = MP4_ERR_EOF;
            return 0;
        }
        if (loadSampleAtIndex(foundIdx, rate, outPacket, outErr) == 0) {
            return 0;
        }
        *outSampleIdx = foundIdx;
        return 1;
    }

    /* 0x811EDB70 */
    int checkSampleReadable(const Mp4FramePacket *packet, int *outErr)
    {
        if (ioStream == NULL) {
            *outErr = -1;
            return 0;
        }
        int ioErr = -1;
        if (ioStream->checkRangeReadable((int)packet->frameSize, &ioErr) != 0) {
            *outErr = 0;
            return 1;
        }
        *outErr = ioErr;
        return 0;
    }
};

struct MediaInfoRecord {
    void *vtable;
    uint32_t formatFourCC;
    uint8_t isRetained;
    char filePath[2051];
};

extern void mediaInfoRecordInit(MediaInfoRecord *rec, const char *path, uint32_t fourCC);
extern void mediaInfoRecordDestroy(MediaInfoRecord *rec);

class Mp4MediaInfo {
public:
    uint32_t formatFourCC;
    uint8_t isRetained;
    char filePath[2051];
    int ioMode;
    Mp4StreamParser *parser;
    StreamIoInterface *ioStream;

    /* 0x811EBA70 */
    Mp4MediaInfo(const char *path, int mode)
        : ioMode(mode),
          parser(NULL),
          ioStream(NULL)
    {
        mediaInfoRecordInit(reinterpret_cast<MediaInfoRecord *>(this), path, FOURCC_MP4);
        if (mode == 16) {
            ioStream = createLocalCacheIoEx(filePath, 0x4000, 32, 1, 0x4000, 4);
            if (ioStream != NULL) {
                void *mem = allocFromHeap(sizeof(Mp4StreamParser));
                if (mem != NULL) {
                    parser = new (mem) Mp4StreamParser(ioStream);
                }
                if (parser == NULL) {
                    delete ioStream;
                    ioStream = NULL;
                }
            }
        }
    }

    /* 0x811EBB0E / 0x811EBB4C */
    virtual ~Mp4MediaInfo()
    {
        if (parser != NULL) {
            delete parser;
            parser = NULL;
        }
        if (ioStream != NULL) {
            delete ioStream;
            ioStream = NULL;
        }
        mediaInfoRecordDestroy(reinterpret_cast<MediaInfoRecord *>(this));
    }

    /* 0x811EBB90 */
    virtual int parseMetadata(TrackMetadataResult *outMeta)
    {
        outMeta->hasCodecSubType = 0;
        outMeta->hasCodecProfile = 0;
        outMeta->hasSampleRate = 0;
        outMeta->hasDuration = 0;
        outMeta->hasBitRate = 0;
        outMeta->hasChannels = 0;

        if (parser == NULL) {
            return 33;
        }

        unsigned int openErr = 0;
        if (parser->openStream(&openErr) != 0) {
            return mapMp4ParserError((int)openErr);
        }

        Mp4FramePacket firstPacket;
        int parserErr = 0;
        if (parser->readFirstSample(&firstPacket, &parserErr) == 1) {
            outMeta->channels = firstPacket.channelCount;
            outMeta->hasChannels = 1;
            if (parser->checkSampleReadable(&firstPacket, &parserErr) == 1) {
                int durOk = parser->getDurationMs(&outMeta->durationMs, &parserErr);
                outMeta->hasDuration = (uint8_t)durOk;
                if (durOk == 1) {
                    int rateOk = parser->getBaseSampleRate(&outMeta->sampleRate, &parserErr);
                    outMeta->hasSampleRate = (uint8_t)rateOk;
                    if (rateOk == 1) {
                        outMeta->codecProfile = 13;
                        outMeta->hasCodecProfile = 1;
                        outMeta->codecSubType = 4;
                        outMeta->hasCodecSubType = 1;
                        parserErr = 0;
                    }
                }
            }
        }

        int result;
        switch (parserErr) {
        case 0:
            return 0;
        case -1:
            result = -1;
            break;
        case MP4_ERR_UNSUPPORTED:
            result = 81;
            break;
        case MP4_ERR_READ_IO:
            result = 19;
            break;
        case MP4_ERR_OPEN_IO:
            result = 20;
            break;
        default:
            result = mapMp4ParserError(parserErr);
            if (result == 0) {
                return 0;
            }
            break;
        }

        outMeta->hasChannels = 0;
        outMeta->hasBitRate = 0;
        outMeta->hasDuration = 0;
        outMeta->hasSampleRate = 0;
        outMeta->hasCodecProfile = 0;
        outMeta->hasCodecSubType = 0;
        return result;
    }
};

struct Mp4FrameInfo;

void *mp4MediaInfoConstruct(void *mem, const char *path, int ioPriority)
{
    return new (mem) Mp4MediaInfo(path, ioPriority);
}

int mp4StreamOpen(Mp4StreamParser *stream, uint32_t *outErr)
{
    unsigned int err = 0;
    int ret = (stream != NULL) ? stream->openStream(&err) : -1;
    if (outErr != NULL)
        *outErr = err;
    return ret;
}

const char *mp4StreamGetPath(Mp4StreamParser *stream)
{
    return (stream != NULL) ? reinterpret_cast<const char *>(stream->getStreamPosition()) : NULL;
}

int mp4StreamSetReadDirection(Mp4StreamParser *stream, int direction)
{
    return (stream != NULL) ? stream->setIoHint(direction) : -1;
}

int mp4StreamGetDurationMs(Mp4StreamParser *stream, int *outDurationMs, uint32_t *outErr)
{
    unsigned int duration = 0;
    int err = 0;
    int ret = (stream != NULL) ? stream->getDurationMs(&duration, &err) : -1;
    if (outDurationMs != NULL)
        *outDurationMs = static_cast<int>(duration);
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamGetSampleRate(Mp4StreamParser *stream, uint32_t *outSampleRate, uint32_t *outErr)
{
    int rate = 0;
    int err = 0;
    int ret = (stream != NULL) ? stream->getBaseSampleRate(&rate, &err) : -1;
    if (outSampleRate != NULL)
        *outSampleRate = static_cast<uint32_t>(rate);
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamGetExtSampleRate(Mp4StreamParser *stream, uint32_t *outExtRate, uint32_t *outErr)
{
    int rate = 0;
    int err = 0;
    int ret = (stream != NULL) ? stream->getSbrSampleRate(&rate, &err) : -1;
    if (outExtRate != NULL)
        *outExtRate = static_cast<uint32_t>(rate);
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamGetSbrFlag(Mp4StreamParser *stream, uint8_t *outSbrFlag, uint32_t *outErr)
{
    int err = 0;
    int ret = (stream != NULL) ? stream->getHasSbr(outSbrFlag, &err) : -1;
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamGetSamplesPerFrame(Mp4StreamParser *stream, uint16_t *outSamples, uint32_t *outErr)
{
    int16_t samples = 0;
    int err = 0;
    int ret = (stream != NULL) ? stream->getSamplesPerFrame(&samples, &err) : -1;
    if (outSamples != NULL)
        *outSamples = static_cast<uint16_t>(samples);
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamReadFirstFrame(Mp4StreamParser *stream, Mp4FrameInfo *outFrame, uint32_t *outErr)
{
    int err = 0;
    int ret = (stream != NULL)
        ? stream->readFirstSample(reinterpret_cast<Mp4FramePacket *>(outFrame), &err)
        : -1;
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamReadNextFrame(Mp4StreamParser *stream, int skipFrames, Mp4FrameInfo *outFrame,
                           uint32_t *outErr)
{
    int err = 0;
    int ret = (stream != NULL)
        ? stream->readNextSample(skipFrames, reinterpret_cast<Mp4FramePacket *>(outFrame), &err)
        : -1;
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamReadPrevFrame(Mp4StreamParser *stream, int skipFrames, Mp4FrameInfo *outFrame,
                           uint32_t *outErr)
{
    int err = 0;
    int ret = (stream != NULL)
        ? stream->readPrevSample(skipFrames, reinterpret_cast<Mp4FramePacket *>(outFrame), &err)
        : -1;
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamSeekAndReadFrame(Mp4StreamParser *stream, uint32_t targetMs, Mp4FrameInfo *outFrame,
                              int *outFrameIndex, uint32_t *outErr)
{
    unsigned int sampleIdx = 0;
    int err = 0;
    int ret = (stream != NULL)
        ? stream->seekToTimeMs(targetMs, reinterpret_cast<Mp4FramePacket *>(outFrame),
                               &sampleIdx, &err)
        : -1;
    if (outFrameIndex != NULL)
        *outFrameIndex = static_cast<int>(sampleIdx);
    if (outErr != NULL)
        *outErr = static_cast<uint32_t>(err);
    return ret;
}

int mp4StreamQueryMetadata(Mp4StreamParser *stream, int titleCap, int artistCap,
                           int albumCap, int genreCap, int artCap, void *metaOut,
                           int extraCap, uint32_t *outErr)
{
    (void)stream;
    (void)titleCap;
    (void)artistCap;
    (void)albumCap;
    (void)genreCap;
    (void)artCap;
    (void)metaOut;
    (void)extraCap;
    if (outErr != NULL)
        *outErr = 0;
    return 0;
}

} // namespace MusicCore
} // namespace sce
