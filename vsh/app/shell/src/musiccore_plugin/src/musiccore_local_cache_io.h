/*
 * SceShell musiccore_plugin - LocalCacheIo class declaration (0x811F3C98..0x811F51FB,
 * vtable 0x815337A8). Shared by musiccore_sync_cache_io.cpp and WavCacheIo /
 * At9CacheIo in musiccore_wav_at9_media.cpp.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_LOCAL_CACHE_IO_H
#define VSH_APP_SHELL_MUSICCORE_LOCAL_CACHE_IO_H

#include <file/localfile.h>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

class LocalCacheIo {
public:
    LocalCacheIo(const char *path, int chunkSize, int blockCount, uint8_t readAhead);
    LocalCacheIo(const char *path, int chunkSize, int blockCount, uint8_t readAhead,
                 int extraChunkSize, int extraBlockCount);
    virtual ~LocalCacheIo();

    virtual int isInitialized() const;
    virtual int open();
    virtual int close();
    virtual int isOpen() const;
    virtual int getFileSize() const;
    virtual const char *getPath() const { return m_filePath; }
    virtual int setPath(const char *path);
    virtual int setPriorityMode(uint32_t priorityMode);
    virtual int applyOpenFilePriority(int priorityMode);
    virtual void flushCache() {}
    virtual int setCacheHint(int hintMode);
    virtual void *mapRangeSimple(int offset, int size, int unusedFlag, int *outErr, int extra);
    virtual void *mapRange(int offset, int size, uint8_t flags, int *outMappedSize,
                           int *outErr, int extra);
    virtual int checkRangeReadable(int sizeBytes, int *outErr);
    virtual void getCacheGeometry(int *outBlockCount, int *outChunkSize) const
    {
        *outBlockCount = m_blockCount;
        *outChunkSize = m_chunkSize;
    }
    virtual int prefetchRange(uint8_t flags, int offset, int size, int *outErr);

    static bool isRegularFile(const char *path);

protected:
    static int openCallback(const char *path, void *userCtx);
    static int closeCallback(void *userCtx);
    static int64_t seekCallback(void *userCtx, int64_t offset, int whence);
    static int readCallback(void *userCtx, void *dst, size_t size);
    static int getFileSizeCallback(const char *path, uint32_t *outSize);
    static int setThreadPriorityCallback(int priorityMode);

    bool copyPath(const char *path);
    bool reopenAfterSuspend(paf::common::SharedPtr<paf::File> &activeFile);

    void *m_cacheHandle;
    uint8_t m_initialized;
    uint8_t m_padInit[3];
    char *m_filePath;
    paf::common::SharedPtr<paf::File> m_file;
    int m_priorityMode;
    int m_padPriority;
    int64_t m_filePos;
    int m_chunkSize;
    int m_blockCount;
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_LOCAL_CACHE_IO_H */
