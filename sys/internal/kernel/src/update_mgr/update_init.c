#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include "update_objects.h"

unsigned int UpdateUtilInit(void)
{
	SceKernelHeapCreateOpt option;
	UpdateUtilBufferEntry *entry;
	SceSblUpdateMgrNode *node;
	char name[32];
	int index, result;

	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&updateMutex,
		"SceSblUpdateMgr", 0, NULL);
	if (result)
		return result;
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&updateTableMutex,
		"SceSblUpdateMgrTable", 0, NULL);
	if (result)
		return result;
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&updateMbrMutex,
		"SceSblUpdateMgrMbr", 0, NULL);
	if (result)
		return result;
	result = sceKernelInitializeFastMutex((SceKernelFastMutex *)&updateComMutex,
		"SceSblUpdateMgrCom", 0, NULL);
	if (result)
		return result;
	for (entry = updateBufferTable; entry != updateBufferTable + 4; entry++)
		*entry = (UpdateUtilBufferEntry){0};
	node = updateNodes;
	index = 0;
	do {
		snprintf(name, sizeof(name), "SceSblUpdateMgrNode%d", index);
		result = sceKernelInitializeFastMutex(
			(SceKernelFastMutex *)&node->mutex, name, 0, NULL);
		if (result)
			return result;
		index++;
		node->initialized = 0;
		node++;
	} while (index != 4);
	option = (SceKernelHeapCreateOpt){
		.size = sizeof(option),
		.flags = HEAP_AUTO_EXTEND,
	};
	updateHeapId = sceKernelCreateHeap("SceSblUs", 0x20000, &option);
	return updateHeapId < 0 ? (int)0x800f020c : 0;
}

SceUID UpdateUtilGetHeapId(void)
{
	return updateHeapId;
}
