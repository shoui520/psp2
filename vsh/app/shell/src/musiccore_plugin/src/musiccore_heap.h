/*
 * SceShell musiccore_plugin - Shared local heap allocator and BufferPool declarations.
 */
#ifndef VSH_APP_SHELL_MUSICCORE_HEAP_H
#define VSH_APP_SHELL_MUSICCORE_HEAP_H

#include <kernel.h>
#include <stddef.h>
#include <stdint.h>

namespace sce {
namespace MusicCore {

void *getLocalHeap();
void *localAlloc(SceSize size);
void localFree(void *ptr);

void initLocalHeap();
void termLocalHeap();

int makeChannelId(char type, char slot);
int getChannelType(char channelId);
int getChannelSlot(unsigned char channelId);

int loadAudioCodecModule();
int unloadAudioCodecModule();
bool isAudioCodecModuleLoaded();

struct BufferEntry {
    int index;
    bool inUse;
    int kind;
    void *data;
    SceSize size;

    BufferEntry(int idx, int bufKind, void *bufPtr, SceSize bufSize)
        : index(idx), inUse(false), kind(bufKind), data(bufPtr), size(bufSize)
    {
    }

    virtual ~BufferEntry() {}
};

class BufferPool {
public:
    BufferPool();
    virtual ~BufferPool();

    int init();
    int term();

    void *acquireLarge();
    int releaseLarge(void *ptr);
    void *acquireMedium();
    int releaseMedium(void *ptr);
    void *acquireTyped(int kind);
    int releaseTyped(void *ptr);

    static BufferPool *getInstance();

private:
    void *m_backingMem;
    BufferEntry *m_largeSlots[2];
    BufferEntry *m_mediumSlots[8];
    BufferEntry *m_typedSlots[5];
};

} /* namespace MusicCore */
} /* namespace sce */

#endif /* VSH_APP_SHELL_MUSICCORE_HEAP_H */
