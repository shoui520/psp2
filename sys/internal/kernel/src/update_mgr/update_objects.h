#ifndef UPDATE_MGR_OBJECTS_H
#define UPDATE_MGR_OBJECTS_H

#include <threadmgr/fast_mutex_object.h>
#include "update_internal.h"

typedef struct SceSblUpdateMgrNode {
	struct ThreadmgrFastMutex mutex;
	uint32_t initialized;
	uint32_t reserved;
	UpdateUtilNodeStatus status;
} SceSblUpdateMgrNode;

typedef struct UpdateUtilBufferEntry {
	void *kernelBuffer;
	void *userBuffer;
	SceSize size;
	uint32_t allocated;
	uint32_t inUse;
	SceUID pid;
	SceUID kernelMemBlock;
	SceUID userMemBlock;
} UpdateUtilBufferEntry;

_Static_assert(sizeof(SceSblUpdateMgrNode) == 96 &&
	       offsetof(SceSblUpdateMgrNode, initialized) == 64 &&
	       offsetof(SceSblUpdateMgrNode, status) == 72, "update node layout");
_Static_assert(sizeof(UpdateUtilBufferEntry) == 32 &&
	       offsetof(UpdateUtilBufferEntry, pid) == 20 &&
	       offsetof(UpdateUtilBufferEntry, kernelMemBlock) == 24,
	       "update buffer entry layout");

extern struct ThreadmgrFastMutex updateMutex;
extern struct ThreadmgrFastMutex updateMbrMutex;
extern struct ThreadmgrFastMutex updateTableMutex;
extern SceSblUpdateMgrNode updateNodes[4];
extern struct ThreadmgrFastMutex updateComMutex;
extern UpdateUtilBufferEntry updateBufferTable[4];
extern SceUID updateHeapId;

#endif
