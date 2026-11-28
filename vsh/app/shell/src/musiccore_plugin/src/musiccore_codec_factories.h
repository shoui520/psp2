/*
 * SceShell musiccore_plugin - Concrete codec pipeline factories (Mp3Factory,
 * Mp4Factory, WavFactory, At9Factory).
 */
#ifndef VSH_APP_SHELL_MUSICCORE_CODEC_FACTORIES_H
#define VSH_APP_SHELL_MUSICCORE_CODEC_FACTORIES_H

#include <stdint.h>

#include "musiccore_pipeline_types.h"

namespace sce {
namespace MusicCore {

class Mp3Factory {
public:
    Mp3Factory() {}
    virtual ~Mp3Factory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf);
    PipelineEventListener *createPipeline(int mode, AudioOpenCallback openCb,
                                          AudioCloseCallback closeCb, AudioWriteCallback writeCb,
                                          AudioFlushCallback flushCb, void *userContext);
    int destroyPipeline(PipelineEventListener *pipeline);
    void *createMediaInfo(const char *path, int ioPriority);
    int destroyMediaInfo(void **ioMediaInfo);
};

class Mp4Factory {
public:
    Mp4Factory();
    virtual ~Mp4Factory();

    virtual uint32_t getHeapSizes(uint32_t *outParserHeap, uint32_t *outDecoderHeap,
                                  uint32_t *outIoHeap);
    virtual void initialize();
    virtual void finalize();
    virtual PipelineEventListener *createPipeline(int playMode, AudioOpenCallback openCb,
                                                  AudioCloseCallback closeCb,
                                                  AudioWriteCallback writeCb,
                                                  AudioFlushCallback flushCb, void *userContext);
    virtual void *createMediaInfo(const char *path, int ioPriority);
};

class WavFactory {
public:
    WavFactory() {}
    virtual ~WavFactory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *outDecoderBuf, uint32_t *outAudioBuf);
    PipelineEventListener *createPipeline(AudioOpenCallback openCb, AudioCloseCallback closeCb,
                                          AudioWriteCallback writeCb, AudioFlushCallback flushCb,
                                          void *userContext);
    int destroyPipeline(PipelineEventListener *pipeline);
    void *createMediaInfo(const char *path);
};

class At9Factory {
public:
    At9Factory() {}
    virtual ~At9Factory() {}

    int getBufferSizes(uint32_t *outParserBuf, uint32_t *unusedDecoderBuf, uint32_t *outAudioBuf);
    PipelineEventListener *createPipeline(int mode, AudioOpenCallback openCb,
                                          AudioCloseCallback closeCb, AudioWriteCallback writeCb,
                                          AudioFlushCallback flushCb, void *userContext,
                                          int loopCount);
    void *createMediaInfo(const char *path, int loopMode);
};

Mp3Factory *getMp3Factory();
Mp4Factory *getMp4FactoryInstance();
At9Factory *getAt9Factory();
extern WavFactory g_wavFactory;

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_CODEC_FACTORIES_H */
