#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include "update_objects.h"

int UpdateUtilGetNodeStatus(int nodeType, UpdateUtilNodeStatus *status, int lock)
{
	SceSblUpdateMgrNode *node;
	UpdateUtilNodeStatus copy;

	if ((unsigned int)nodeType - 1 > 2 || !status)
		return (int)0x800f0216;
	if (lock) {
		node = &updateNodes[nodeType];
		lockFastMutex((SceKernelFastMutex *)&node->mutex);
		if (node->initialized) {
			copy = node->status;
			*status = copy;
			sceKernelUnlockFastMutex((SceKernelFastMutex *)&node->mutex);
			return 0;
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&node->mutex);
		return (int)0x800f0229;
	}
	node = &updateNodes[nodeType];
	if (!node->initialized)
		return (int)0x800f0229;
	copy = node->status;
	*status = copy;
	return 0;
}

int UpdateUtilGetNodeStatusLocked(int nodeType, UpdateUtilNodeStatus *status)
{
	return UpdateUtilGetNodeStatus(nodeType, status, 1);
}
