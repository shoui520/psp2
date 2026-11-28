/*
 * SceShell musiccore_plugin - Mp4MediaInfo and Mp4StreamParser (ISO BMFF /
 * MP4 audio track box traversal, sample table lookup, and AAC/SBR framing).
 * Original address range (3.65-CEX): 0x811EBA70 - 0x811EDBD3
 */

#include <kernel.h>
#include <std/new>
#include <stddef.h>
#include <stdint.h>
#include "musiccore_media.h"

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

static inline void normalizeBoxIoError(unsigned int *err)
{
    if (*err == (unsigned int)MP4_ERR_EOF || *err == (unsigned int)MP4_ERR_BOX_EOF) {
        *err = (unsigned int)MP4_ERR_CORRUPT;
    }
}

int Mp4StreamParser::resolveEffectiveSampleRate() const
{
    int rate = 0;
    if (audioSpecConfig.m_sbrPresentFlag != 0) {
        rate = static_cast<int>(audioSpecConfig.m_extSampleRate);
    }
    if (rate == 0) {
        rate = static_cast<int>(audioSpecConfig.getSamplingFrequency());
    }
    return rate;
}

int Mp4StreamParser::loadSampleAtIndex(unsigned int sampleIdx, int effectiveRate,
                                       Mp4FramePacket *outPacket, int *outErr)
{
    uint32_t fileOffset = 0;
    uint32_t sampleSize = 0;
    uint32_t sampleDescIdx = 0;
    uint32_t err = 0;
    if (sampleTable.lookupSampleLocation(sampleIdx, &fileOffset, &sampleSize, &sampleDescIdx, &err) == 0) {
        *outErr = static_cast<int>(err);
        return 0;
    }
    if (sampleSize > 0x7FFFFFFFu) {
        *outErr = MP4_ERR_UNSUPPORTED;
        return 0;
    }

    int mappedLen = 0;
    int ioErr = -1;
    int dataPtr = ioStream->mapRange(static_cast<int>(fileOffset), sampleSize, 0, &mappedLen, &ioErr, 0);
    if (dataPtr == 0) {
        *outErr = (ioErr == MP4_ERR_EOF) ? MP4_ERR_CORRUPT : ioErr;
        return 0;
    }

    outPacket->frameData = reinterpret_cast<void *>(dataPtr);
    outPacket->frameSize = sampleSize;
    outPacket->sampleRate = effectiveRate;
    outPacket->samplesPerFrame = (audioSpecConfig.m_sbrPresentFlag != 0) ? 2048 : 1024;
    outPacket->flags = 0;
    outPacket->leadingSkip = 0;
    outPacket->trailingSkip = 0;
    outPacket->channelCount = audioSpecConfig.getChannelConfig();
    currentSampleIndex = sampleIdx;
    *outErr = 0;
    return 1;
}

/* 0x811EBCCC */
Mp4StreamParser::Mp4StreamParser(StreamIoInterface *io)
    : ioStream(io),
      openStatus(-1),
      currentSampleIndex(0),
      mdhdReader(NULL, 0),
      stblReader(reinterpret_cast<Mp4CachedIo *>(io), 1),
      sttsReader(reinterpret_cast<Mp4CachedIo *>(io), 1),
      stszReader(reinterpret_cast<Mp4CachedIo *>(io), 2),
      stscReader(reinterpret_cast<Mp4CachedIo *>(io), 3),
      stcoReader(reinterpret_cast<Mp4CachedIo *>(io), 4),
      mdhdBox(),
      stblBox("stbl"),
      sttsBox("stts"),
      stszBox(),
      stscBox("stsc"),
      stcoBox("stco"),
      decoderConfigBox(),
      audioSpecConfig(),
      sampleTable(&sttsReader, &stszReader, &stscReader, &stcoReader)
{
}

/* 0x811EBD90 / 0x811EBE1E */
Mp4StreamParser::~Mp4StreamParser()
{
}

/* 0x811EBE30 */
int Mp4StreamParser::closeStream()
{
    if (ioStream == NULL) {
        return -1;
    }
    decoderConfigBox.m_isParsed = 0;
    audioSpecConfig.m_isParsed = 0;
    stcoBox.resetParsed();
    stscBox.resetParsed();
    stszBox.resetParsed();
    sttsBox.resetParsed();
    stblBox.resetParsed();
    mdhdBox.resetParsed();

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
int Mp4StreamParser::openStream(unsigned int *outErr)
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

    Mp4CachedIo *cachedIo = reinterpret_cast<Mp4CachedIo *>(ioStream);
    Mp4ContainerBox moovBox("moov");
    Mp4ContainerReader moovReader(cachedIo, 1);

    int result = -1;
    bool ok = true;

    if (moovReader.readTopContainer(&moovBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }

    Mp4MvhdBox mvhdBox;
    Mp4MvhdBoxReader mvhdReader(cachedIo, 1);
    if (ok && mvhdReader.read(&moovBox, &mvhdBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }

    Mp4ContainerBox trakBox("trak");
    Mp4ContainerReader trakReader(cachedIo, 1);
    Mp4TkhdBox hdlrBox;
    Mp4TkhdBoxReader hdlrReader(cachedIo, 1);

    if (ok) {
        int trackCount = static_cast<int>(trakReader.countTracks(&moovBox, outErr));
        if (*outErr != 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        } else if (trackCount < 1) {
            *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
            ok = false;
        } else {
            bool foundSound = false;
            for (int idx = 1; idx <= trackCount; ++idx) {
                if (trakReader.readTrackAt(&moovBox, static_cast<uint16_t>(idx + 1), &trakBox, outErr) == 0) {
                    normalizeBoxIoError(outErr);
                    ok = false;
                    break;
                }
                if (hdlrReader.read(&trakBox, &hdlrBox, outErr) == 0) {
                    normalizeBoxIoError(outErr);
                    ok = false;
                    break;
                }
                if (hdlrBox.isAudioTrackVolume() != 0) {
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

    Mp4ContainerBox mdiaBox("mdia");
    Mp4ContainerReader mdiaReader(cachedIo, 1);
    if (ok && mdiaReader.readChildContainer(&trakBox, &mdiaBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }
    if (ok && mdhdReader.read(&mdiaBox, &mdhdBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }

    Mp4ContainerBox minfBox("minf");
    Mp4ContainerReader minfReader(cachedIo, 1);
    if (ok && minfReader.readChildContainer(&mdiaBox, &minfBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }

    Mp4ContainerBox smhdBox("smhd");
    Mp4ContainerReader smhdReader(cachedIo, 1);
    if (ok) {
        uint32_t smhdPayload = (smhdBox.m_boxSize >= smhdBox.m_headerSize)
            ? (smhdBox.m_boxSize - smhdBox.m_headerSize) : 0u;
        if (smhdReader.readChildContainer(&minfBox, &smhdBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        } else if (((smhdBox.m_boxSize >= smhdBox.m_headerSize)
                    ? (smhdBox.m_boxSize - smhdBox.m_headerSize) : 0u) != 36u) {
            (void)smhdPayload;
            *outErr = (unsigned int)MP4_ERR_CORRUPT;
            ok = false;
        } else if (stblReader.readChildContainer(&minfBox, &stblBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        }
    }

    Mp4TableBox stsdBox("stsd");
    Mp4StsdReader stsdReader(cachedIo, 1);
    if (ok) {
        if (stsdReader.read(&stblBox, &stsdBox, outErr) == 0) {
            normalizeBoxIoError(outErr);
            ok = false;
        } else if (stsdBox.m_entryCount != 1u) {
            *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
            ok = false;
        }
    }

    Mp4AudioSampleEntryBox sampleEntry;
    if (ok && stsdReader.readSampleEntry(&stsdBox, "soun@", 1u, 40u, &sampleEntry, outErr) == 0) {
        ok = false;
    }

    Mp4EsdsBox mp4aBox;
    Mp4EsdsBoxReader mp4aReader(cachedIo, 1);
    unsigned int esdsStart = 0;
    unsigned int esdsLen = 0;
    if (ok) {
        if (mp4aReader.read(&sampleEntry, &mp4aBox, outErr) == 0) {
            ok = false;
        } else {
            unsigned int mp4aOff = mp4aBox.m_fileOffset + mp4aBox.m_headerSize;
            unsigned int mp4aHdr = mp4aBox.m_headerSize;
            unsigned int mp4aEnd = mp4aOff +
                ((mp4aBox.m_boxSize >= mp4aBox.m_headerSize) ? (mp4aBox.m_boxSize - mp4aBox.m_headerSize) : 0u);
            esdsStart = mp4aOff + mp4aHdr;
            if (esdsStart > mp4aEnd) {
                *outErr = (unsigned int)MP4_ERR_CORRUPT;
                ok = false;
            } else {
                esdsLen = mp4aEnd - esdsStart;
            }
        }
    }

    Mp4EsDescriptor esdsBox;
    Mp4EsDescriptorReader esdsReader(cachedIo, 1);
    unsigned int decCfgStart = 0;
    unsigned int decCfgLen = 0;
    if (ok) {
        if (esdsReader.read(esdsStart, esdsLen, &esdsBox, outErr) == 0) {
            ok = false;
        } else {
            unsigned int esdsOff = esdsBox.m_fileOffset;
            unsigned int esdsHdr = esdsBox.m_headerSize;
            unsigned int esdsEnd = esdsOff + esdsBox.m_totalSize;
            decCfgStart = esdsOff + esdsHdr;
            if (decCfgStart > esdsEnd) {
                *outErr = (unsigned int)MP4_ERR_CORRUPT;
                ok = false;
            } else {
                decCfgLen = esdsEnd - decCfgStart;
            }
        }
    }

    Mp4DecoderConfigReader decCfgReader(cachedIo, 1);
    unsigned int ascStart = 0;
    unsigned int ascLen = 0;
    if (ok) {
        if (decCfgReader.read(decCfgStart, decCfgLen, &decoderConfigBox, outErr) == 0) {
            ok = false;
        } else if (decoderConfigBox.m_objectTypeIndication != 64u) {
            *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
            ok = false;
        } else {
            unsigned int cfgOff = decoderConfigBox.m_fileOffset;
            unsigned int cfgEnd = cfgOff + decoderConfigBox.m_totalSize;
            ascStart = cfgOff + decoderConfigBox.m_parsedEndOffset;
            if (ascStart > cfgEnd) {
                *outErr = (unsigned int)MP4_ERR_CORRUPT;
                ok = false;
            } else {
                ascLen = cfgEnd - ascStart;
            }
        }
    }

    Mp4AudioSpecificConfigReader ascReader(cachedIo, 1);
    if (ok) {
        if (ascReader.read(ascStart, ascLen, &audioSpecConfig, outErr) == 0) {
            ok = false;
        } else if (!isSupportedAacProfile(reinterpret_cast<const Mp4AudioSpecificConfig *>(&stsdBox),
                                          &audioSpecConfig)) {
            *outErr = (unsigned int)MP4_ERR_UNSUPPORTED;
            ok = false;
        }
    }

    if (ok && sttsReader.read(&stblBox, &sttsBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }
    if (ok && stszReader.read(&stblBox, &stszBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }
    if (ok && stscReader.read(&stblBox, &stscBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }
    if (ok && stcoReader.read(&stblBox, &stcoBox, outErr) == 0) {
        normalizeBoxIoError(outErr);
        ok = false;
    }

    if (ok) {
        sampleTable.attachStts(&sttsBox);
        sampleTable.attachStsz(&stszBox);
        sampleTable.attachStsc(&stscBox);
        sampleTable.attachStco(&stcoBox);
        result = openRet;
    } else {
        closeStream();
    }

    return result;
}

/* 0x811ED282 */
int Mp4StreamParser::getStreamPosition()
{
    if (ioStream == NULL) {
        return 0;
    }
    return ioStream->getPosition();
}

/* 0x811ED294 */
int Mp4StreamParser::setIoHint(int mode)
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

/* 0x811ED2E8 */
int Mp4StreamParser::getDurationMs(unsigned int *outDurationMs, int *outErr)
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
    unsigned int timeScale = mdhdBox.m_timescale;
    unsigned int duration = mdhdBox.m_duration;
    if (rate == 0) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }
    *outDurationMs = (unsigned int)(((float)duration * 1000.0f) / (float)timeScale);
    *outErr = 0;
    return 1;
}

/* 0x811ED39C */
int Mp4StreamParser::getBaseSampleRate(int *outRate, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP4_ERR_NOT_OPEN;
        return 0;
    }
    int rate = static_cast<int>(audioSpecConfig.getSamplingFrequency());
    *outRate = rate;
    if (rate == 0) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }
    *outErr = 0;
    return 1;
}

/* 0x811ED3EC */
int Mp4StreamParser::getSbrSampleRate(int *outRate, int *outErr)
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
    if (audioSpecConfig.m_sbrPresentFlag != 0) {
        rate = static_cast<int>(audioSpecConfig.m_extSampleRate);
    }
    *outRate = rate;
    if (rate == 0 && audioSpecConfig.m_sbrPresentFlag != 0) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }
    *outErr = 0;
    return 1;
}

/* 0x811ED454 */
int Mp4StreamParser::getHasSbr(uint8_t *outHasSbr, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP4_ERR_NOT_OPEN;
        return 0;
    }
    *outHasSbr = audioSpecConfig.m_sbrPresentFlag;
    *outErr = 0;
    return 1;
}

/* 0x811ED496 */
int Mp4StreamParser::getSamplesPerFrame(int16_t *outSamples, int *outErr)
{
    if (ioStream == NULL) {
        *outErr = -1;
        return 0;
    }
    if (ioStream->isOpen() == 0) {
        *outErr = MP4_ERR_NOT_OPEN;
        return 0;
    }
    *outSamples = (audioSpecConfig.m_sbrPresentFlag != 0) ? 2048 : 1024;
    *outErr = 0;
    return 1;
}

/* 0x811ED4E2 */
int Mp4StreamParser::readFirstSample(Mp4FramePacket *outPacket, int *outErr)
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
    if (rate == 0 || stszBox.m_sampleCount == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }
    return loadSampleAtIndex(1u, rate, outPacket, outErr);
}

/* 0x811ED650 */
int Mp4StreamParser::readNextSample(int skipCount, Mp4FramePacket *outPacket, int *outErr)
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
    unsigned int totalSamples = stszBox.m_sampleCount;
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
int Mp4StreamParser::readPrevSample(int skipCount, Mp4FramePacket *outPacket, int *outErr)
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
    if (rate == 0 || stszBox.m_sampleCount == 0u) {
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
int Mp4StreamParser::seekToTimeMs(unsigned int targetMs, Mp4FramePacket *outPacket, unsigned int *outSampleIdx, int *outErr)
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
    unsigned int timeScale = mdhdBox.m_timescale;
    unsigned int totalSamples = stszBox.m_sampleCount;
    if (rate == 0 || timeScale == 0u || totalSamples == 0u) {
        *outErr = MP4_ERR_CORRUPT;
        return 0;
    }

    unsigned int targetTicks = (unsigned int)(((float)targetMs / 1000.0f) * (float)timeScale);
    uint32_t foundIdx = 0;
    uint32_t err = 0;
    if (sampleTable.lookupSampleByTimestamp(targetTicks, &foundIdx, &err) == 0) {
        *outErr = (static_cast<int>(err) == MP4_ERR_BOX_EOF) ? MP4_ERR_EOF : static_cast<int>(err);
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
int Mp4StreamParser::checkSampleReadable(const Mp4FramePacket *packet, int *outErr)
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

/* 0x811EBA70 */
Mp4MediaInfo::Mp4MediaInfo(const char *path, int mode)
    : MediaInfoBase(path, FOURCC_MP4),
      ioMode(mode),
      parser(NULL),
      ioStream(NULL)
{
    if (mode == 16) {
        ioStream = createLocalCacheIoEx(m_path, 0x4000, 32, 1, 0x4000, 4);
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
Mp4MediaInfo::~Mp4MediaInfo()
{
    if (parser != NULL) {
        delete parser;
        parser = NULL;
    }
    if (ioStream != NULL) {
        delete ioStream;
        ioStream = NULL;
    }
}

/* 0x811EBB90 */
int Mp4MediaInfo::parseMetadata(TrackMetadataResult *outMeta)
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

} // namespace MusicCore
} // namespace sce
