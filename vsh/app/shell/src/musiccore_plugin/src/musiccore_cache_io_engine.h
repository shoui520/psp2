/*
 * SceShell musiccore_plugin - CacheIoEngine API shared between
 * musiccore_cache_io_engine.cpp and musiccore_sync_cache_io.cpp.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_CACHE_IO_ENGINE_H
#define VSH_APP_SHELL_MUSICCORE_CACHE_IO_ENGINE_H

#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

class CacheIoEngine;

typedef int (*StreamOpenCallback)(const char *path, void **outHandle, void *userCtx, void *extraCtx);
typedef int (*StreamCloseCallback)(void *handle, void *userCtx, void *extraCtx);
typedef int64_t (*StreamSeekCallback)(void *handle, int64_t offset, int whence, void *userCtx, void *extraCtx);
typedef int (*StreamReadCallback)(void *handle, void *dstBuf, uint32_t size, int isSync,
                                  void *userCtx, void *extraCtx);
typedef int (*StreamStatSizeCallback)(const char *path, uint32_t *outSize);
typedef int (*StreamErrorCallback)(void *handle);

struct AsyncRingConfig {
    uint32_t blockSize;
    uint32_t blockCount;
};

struct PrefetchPinToken {
    int firstSlot;
    int secondSlot;
};

int cacheIoCreate(uint8_t syncStreamCount, const uint32_t *syncBlockSizes,
                  uint8_t asyncStreamCount, const AsyncRingConfig *asyncConfigs,
                  uint8_t enableWorkers, int threadPriority,
                  void *userCtx, void *extraCtx, CacheIoEngine **outEngine);
int cacheIoDestroy(CacheIoEngine *engine);
int cacheIoSetCallbacks(CacheIoEngine *engine, StreamOpenCallback openCb, StreamCloseCallback closeCb,
                        StreamSeekCallback seekCb, StreamReadCallback readCb,
                        StreamStatSizeCallback statSizeCb, StreamErrorCallback errCb);
int cacheIoSetStreamPath(CacheIoEngine *engine, const char *path);
int cacheIoGetStreamPath(const CacheIoEngine *engine, const char **outPath);
int cacheIoOpenStream(CacheIoEngine *engine, void *streamHandle);
int cacheIoCloseStream(CacheIoEngine *engine);
int cacheIoIsStreamOpen(const CacheIoEngine *engine, uint8_t *outOpen);
int cacheIoGetStreamSize(const CacheIoEngine *engine, int64_t *outSize);
int cacheIoSetDirectionHint(CacheIoEngine *engine, int directionMode, int forcePrefetch);
int cacheIoReadSync(CacheIoEngine *engine, uint8_t streamId, int64_t offset, uint32_t length,
                    void **outPtr, uint8_t *outDidIo, uint32_t *outAvail);
int cacheIoReadAsync(CacheIoEngine *engine, uint8_t streamId, int64_t offset, uint32_t length,
                     uint8_t pinBlocks, void **outPtr, PrefetchPinToken **outToken,
                     uint8_t *outWokeWorker, uint32_t *outAvail);
int cacheIoReleaseAsyncPin(CacheIoEngine *engine, uint8_t streamId, PrefetchPinToken **ioToken);

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_CACHE_IO_ENGINE_H */
