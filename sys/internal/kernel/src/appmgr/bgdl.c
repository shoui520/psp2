/* Background-download queue storage and publication. */

#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include <appmgr/appmgr_rdrfp.h>
#include "app_state_internal.h"

extern SceUID g_appmgrCoreHeap;
extern SceUID g_appmgrAppMutex;

enum {
	BGDL_QUEUE_ITEMS = 32,
	BGDL_QUEUE_DATA_SIZE = 0x700,
	BGDL_QUEUE_STORAGE_SIZE = 0x720,
};

/* a queued background download, as the shell publishes it */
struct AppmgrBgdlQueueItem {
	SceUInt32 type;		/* 1, 3: an application; 2: additional content */
	SceUInt32 state;
	char contentId[48];	/* its title ID at 7 */
};

_Static_assert(sizeof(struct AppmgrBgdlQueueItem) * BGDL_QUEUE_ITEMS == BGDL_QUEUE_DATA_SIZE,
	       "background-download queue");

/* These operations allocate and copy the entire externally supplied buffer. */
void *g_appmgrBgdlQueueStorage;

int appmgrInitializeBgdlQueueStorage(void)
{
	SceAllocOpt opt;
	int result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		if (g_appmgrBgdlQueueStorage != NULL)
			sceKernelFreeHeapMemory(g_appmgrCoreHeap, g_appmgrBgdlQueueStorage);
		result = 0;
		opt.mmapKind = 0;
		opt.mmapBase = NULL;
		opt.mmapSize = 0;
		opt.size = sizeof(opt);
		opt.align = 32;
		g_appmgrBgdlQueueStorage = sceKernelAllocHeapMemoryWithOption(
			g_appmgrCoreHeap, BGDL_QUEUE_STORAGE_SIZE, &opt);
		if (g_appmgrBgdlQueueStorage != NULL)
			memset(g_appmgrBgdlQueueStorage, 0, BGDL_QUEUE_STORAGE_SIZE);
		else
			result = (int)0x80801006;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	return result;
}

int SceAppMgrForDriver_5B1BE482(const void *status)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		if (status != NULL)
			memcpy(g_appmgrBgdlQueueStorage, status, BGDL_QUEUE_DATA_SIZE);
		else
			result = (int)0x80801001;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	sceSyscallLeave(syscallState);
	return result;
}

/* Copies the queued downloads of the caller's application and its additional content. */
int appmgrBgdlGetQueueStatus(struct AppmgrBgdlQueueItem *status)
{
	const struct AppmgrBgdlQueueItem *item;
	const AppmgrProcessContent *content;
	struct AppState *app;
	SceUInt32 syscallState = sceSyscallEnter();
	int result, count, index, addcont;

	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result < 0)
		goto leave;
	if (status == NULL) {
		result = (int)0x80801001;
		goto unlock;
	}
	memset(status, 0, BGDL_QUEUE_DATA_SIZE);
	app = appmgrFindAppByProcessId(g_appmgrAppStates, sceKernelGetProcessId());
	if (app == NULL) {
		result = (int)0x80802012;
		goto unlock;
	}
	content = &app->process.content;
	item = g_appmgrBgdlQueueStorage;
	count = 0;
	for (index = 0; index < BGDL_QUEUE_ITEMS; index++, item++) {
		if (item->contentId[0] == 0)
			break;
		if (item->type == 1 || item->type == 3) {
			if (strncmp(content->titleId, &item->contentId[7], 9) == 0)
				status[count++] = *item;
		} else if (item->type == 2) {
			for (addcont = 0; addcont < 8; addcont++) {
				if (addcont != 0 && content->addContTitleIds[addcont][0] == 0)
					break;
				if (strncmp(content->addContTitleIds[addcont],
					    &item->contentId[7], 9) == 0) {
					status[count++] = *item;
					break;
				}
			}
		}
	}
	result = 0;
unlock:
	sceKernelUnlockMutex(g_appmgrAppMutex, 1);
leave:
	sceSyscallLeave(syscallState);
	return result;
}
