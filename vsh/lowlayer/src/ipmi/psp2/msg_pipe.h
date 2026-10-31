#ifndef IPMI_PSP2_MSG_PIPE_H
#define IPMI_PSP2_MSG_PIPE_H

#include <stddef.h>
#include <kernel.h>

namespace IPMI {

/*
 * Where a message pipe's buffer comes from. Sony's warning names KERNEL_BUDGET; the
 * other names are ours, after the pipe type each one selects.
 */
enum MemoryBudget {
	DEFAULT_BUDGET = 0,
	KERNEL_BUDGET = 1,
	USER_BUDGET = 2,
	PHYCONT_BUDGET = 3
};

/* One piece of a message or a method's input: an address and a size (Sony's IPMI::DataInfo). */
struct DataInfo {
	const void *pData;
	SceSize dataSize;
};

/* The client event flag an asynchronous call sets when its result arrives. */
struct EventFlagInfo {
	SceUInt32 index;
	SceUInt32 bitPattern;
};

/* One of a method's output buffers (Sony's IPMI::BufferInfo): outSize is what was written. */
struct BufferInfo {
	void *pBuffer;
	SceSize bufferSize;
	SceSize outSize;
};

namespace impl {
namespace psp2 {

/* sceKernelCreateMsgPipe's type for PHYCONT_BUDGET; the SDK names only 64 and 66. */
#define MSG_PIPE_TYPE_USER_MAIN_PHYCONT		65

class MsgPipe {
public:
	MsgPipe();
	~MsgPipe();

	int create(const char *name, SceSize bufSize, int budget, int openLimit);
	int open(const char *name);
	int close();
	int send(const void *buf, SceSize size, SceUInt32 *timeout);
	int trySend(const void *buf, SceSize size);
	int sendVector(const DataInfo *info, SceUInt32 count, SceUInt32 *timeout);
	int trySendVector(const DataInfo *info, SceUInt32 count);
	int tryReceive(void *buf, SceSize *result, SceSize size);
	int cancel();

private:
	SceUID m_id;
	bool m_owner;
};

}
}
}

#endif
