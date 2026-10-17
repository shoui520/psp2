#include "heap_state.h"
#include <kernel/error.h>

struct MspaceBackingBlock {
	SceUInt32 encodedId;
};

extern int sceKernelDeleteLwMutex(SceKernelLwMutexWork *work);
extern void *sceKernelGetTLSAddr(int slot);
extern int sceKernelFreeMemBlock(SceUID id);
extern SceUID sceKernelFindMemBlockByAddr(const void *address, SceSize size);
extern void abort(void) __attribute__((noreturn));

/* SCE: allocator-owned segments carry a TLS-obfuscated kernel memblock ID. */
int sceClibMspaceDestroy(void *mspace)
{
	struct MspaceStateView *space = mspace;
	struct MspaceSegment segment, next;
	const struct MspaceBackingBlock *block;
	SceUInt32 encodedId;
	SceUID id;
	int result;

	if (space->flags & MSPACE_USE_LOCK) {
		if (sceKernelDeleteLwMutex(&space->mutex) < 0)
			abort();
	}
	if (!(space->flags & MSPACE_OWN_SEGMENTS))
		return 0;

	segment = space->segment;
	space->segment.base = NULL;
	space->segment.size = 0;
	space->segment.next = NULL;
	do {
		next = segment.next ? *segment.next : segment;
		if (segment.flags & MSPACE_SEGMENT_MMAPPED) {
			block = segment.base;
			encodedId = block->encodedId;
			id = encodedId ^ *(SceUInt32 *)sceKernelGetTLSAddr(7);
			result = sceKernelFreeMemBlock(id);
			if (result) {
				if (result != SCE_KERNEL_ERROR_INVALID_UID)
					abort();
				id = sceKernelFindMemBlockByAddr(segment.base, segment.size);
				if (id <= 0 || sceKernelFreeMemBlock(id))
					abort();
			}
		}
		if (!segment.next)
			break;
		segment = next;
	} while (1);
	return 0;
}
