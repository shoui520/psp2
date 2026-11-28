/*
 * SceShell musiccore_plugin - Concrete audio codec decoders (Mp3Decoder,
 * Mp4Decoder, At9Decoder) and BufRefPipe declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_CODEC_DECODERS_H
#define VSH_APP_SHELL_MUSICCORE_CODEC_DECODERS_H

#include <stdint.h>

#include "musiccore_pipeline_types.h"

namespace sce {
namespace MusicCore {

/* SceAudiocodecCtrl layout (96 bytes) */
struct AudiocodecCtrl {
    uint32_t size;              /* +0: struct size or decode status */
    void *workMem;              /* +4: 4-byte aligned codec work memory */
    uint32_t workMemSize;       /* +8 */
    int decodeError;            /* +12 */
    void *inputBuf;             /* +16 */
    int inputUsedBytes;         /* +20 */
    void *outputBuf;            /* +24 */
    int outputBytes;            /* +28 */
    union {
        struct {
            int inputBytes;     /* +32 */
            uint32_t reserved[15];
        } mp3;
        struct {
            uint8_t isAdts;     /* +32 */
            uint8_t padAdts[3];
            uint32_t reserved;  /* +36 */
            uint32_t sampleRate;/* +40 */
            uint8_t isSbr;      /* +44 */
            uint8_t padTail[51];
        } aac;
        struct {
            uint8_t configData[4]; /* +32 */
            uint8_t padTail[60];
        } at9;
        uint8_t raw[64];
    } config;
};

struct Mp4DecoderConfig {
    uint32_t outputSampleRate;
    uint32_t inputSampleRate;
    uint32_t parserErrorCode;
    uint8_t isSbrEnabled;
    uint8_t padTail[3];
};

struct At9DecoderConfig {
    uint32_t parserErrorCode;
    uint8_t configBytes[4];
};

/* Mp3Decoder (168 bytes, 0x81207E74 - 0x8120825B) */
class Mp3Decoder : public AudioDecoderBase {
public:
    explicit Mp3Decoder(const char *threadName);
    virtual ~Mp3Decoder();

    virtual int openDecoder();
    virtual int closeDecoder();
    virtual int decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf);
    virtual void configureDecoder(const void *configData, uint32_t configSize);

private:
    AudiocodecCtrl *m_ctrl;             /* +56 */
    uint8_t m_ctrlArena[100];           /* +60..+159 */
    void *m_workMemPool;                /* +160 */
    uint8_t m_resetPending;             /* +164 */
    uint8_t m_padTail[3];               /* +165..+167 */
};

/* Mp4Decoder (208 bytes, 0x8120825C - 0x81208793) */
class Mp4Decoder : public AudioDecoderBase {
public:
    explicit Mp4Decoder(const char *threadName);
    virtual ~Mp4Decoder();

    virtual int openDecoder();
    virtual int closeDecoder();
    virtual int decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf);
    virtual void configureDecoder(const void *configData, uint32_t configSize);

private:
    AudiocodecCtrl *m_ctrl;             /* +56 */
    uint8_t m_ctrlArena[100];           /* +60..+159 */
    void *m_workMemPool;                /* +160 */
    uint8_t m_resetPending;             /* +164 */
    uint8_t m_padReset[3];              /* +165..+167 */
    Mp4DecoderConfig m_pendingCfg;      /* +168..+183 */
    Mp4DecoderConfig m_activeCfg;       /* +184..+199 */
    int m_lastPacketFlags;              /* +200 */
    uint8_t m_consecutiveErrors;        /* +204 */
    uint8_t m_padTail[3];               /* +205..+207 */
};

/* At9Decoder (1196 bytes, 0x81208794 - 0x81208C33) */
class At9Decoder : public AudioDecoderBase {
public:
    explicit At9Decoder(const char *threadName);
    virtual ~At9Decoder();

    virtual int openDecoder();
    virtual int closeDecoder();
    virtual int decodeFrame(MediaFrameBuffer *inBuf, MediaFrameBuffer *outBuf);
    virtual void configureDecoder(const void *configData, uint32_t configSize);

private:
    AudiocodecCtrl *m_ctrl;             /* +56 */
    uint8_t m_ctrlArena[100];           /* +60..+159 */
    void *m_workMemPool;                /* +160 */
    uint32_t m_parserErrorCode;         /* +164 */
    uint8_t m_at9ConfigBytes[4];        /* +168..+171 */
    int16_t m_planarPcm[2][256];        /* +172..+1195 */
};

/* BufRefPipe ("SceShellMusicCoreBufRefPipe", 44 bytes, 0x81209610 - 0x81209AA7) */
class BufRefPipe : public PipeInterface {
public:
    BufRefPipe(int slotCount, PipeProducerControl *producer, PipeConsumerControl *consumer);
    virtual ~BufRefPipe();

    virtual void setPositionListener(PositionChangeListener *listener) { m_posListener = listener; }
    virtual bool isInitialized() const;
    virtual int getPlayPositionMs() const { return m_playPositionMs; }
    virtual void setPlayPositionMs(int posMs) { m_playPositionMs = posMs; }
    virtual void reset();
    virtual bool hasQueuedFrame() const { return m_readIndex >= 0; }
    virtual bool hasReadableFrame() const { return m_readIndex >= 0; }
    virtual int acquireWriteSlot(MediaFrameBuffer **outBuf);
    virtual int commitWriteSlot(MediaFrameBuffer *buf);
    virtual int acquireReadSlot(MediaFrameBuffer **outBuf);
    virtual int releaseReadSlot(MediaFrameBuffer *buf);

private:
    int m_slotCount;                        /* +4 */
    MediaFrameBuffer *m_slots;              /* +8 */
    int m_readIndex;                        /* +12 */
    int m_writeIndex;                       /* +16 */
    PipeProducerControl *m_producer;        /* +20 */
    PipeConsumerControl *m_consumer;        /* +24 */
    PositionChangeListener *m_posListener;  /* +28 */
    int m_playPositionMs;                   /* +32 */
    SyncMutex m_lock;                       /* +36 */
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_CODEC_DECODERS_H */
