/*
 * Walking libult's lists in user memory. Sony's file name for these is
 * unknown; they are kept out of ult_sync_obj.c because Sony's build did not
 * inline them there.
 */
#include "ult_sync_obj.h"

/* name unknown: thread IDs of the waiters on a list, optionally filtered */
int getUltWaitThreadIds(SceUID pid, SceUIntPtr waitList, SceUInt32 mask, SceUIntPtr object,
			SceUInt32 maxThreads, SceUInt32 maxIds, SceUInt32 *ids)
{
	UltLink link;
	UltWaiter waiter;
	SceUIntPtr p;
	SceUInt32 flags = 0, n = 0, tid;
	int i = 0, ret;

	for (p = waitList & ~7; isValidUltPointer(p); p = link.next & ~7) {
		ret = sceKernelCopyFromUserProc(pid, &link, (const void *)p, sizeof(link));
		if (ret != 0)
			return ret;
		if (i != 0) {
			ret = sceKernelCopyFromUserProc(pid, &waiter, (const void *)link.data,
							 sizeof(waiter));
			if (ret != 0)
				return ret;
			if ((mask == 0 || (flags & mask) != 0) &&
			    (object == 0 || waiter.object == object)) {
				if (n >= maxThreads)
					return SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS;
				if (ids != NULL) {
					ret = getTidUltId(pid, waiter.thread, &tid);
					if (ret != 0)
						return ret;
					if (n < maxIds)
						ids[n] = tid;
				}
				n++;
			}
		}
		i++;
		flags = link.flags;
	}
	return n;
}

/* name unknown: count the data of a list, copying it out when data != NULL */
int copyUltQueueData(SceUID pid, SceUIntPtr dataList, SceUInt32 maxData, SceSize dataSize,
		     SceSize stride, void *data)
{
	UltLink link;
	SceUIntPtr p;
	SceUInt32 n = 0;
	int first = 1, ret;

	for (p = dataList & ~7; isValidUltPointer(p); p = link.next & ~7) {
		ret = sceKernelCopyFromUserProc(pid, &link, (const void *)p, sizeof(link));
		if (ret != 0)
			return ret;
		if (!first) {
			if (link.data != 0 && data != NULL) {
				if (n >= maxData)
					return SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS;
				ret = sceKernelCopyFromUserProc(pid, data, (const void *)link.data,
								 dataSize);
				if (ret != 0)
					return ret;
			}
			n++;
			data = (char *)data + stride;
		}
		first = 0;
	}
	return n;
}

/* name unknown: the addresses of the data of a list */
int getUltQueueDataIds(SceUID pid, SceUIntPtr dataList, SceUInt32 maxData, SceUInt32 maxIds,
		       SceUInt32 *ids)
{
	UltLink link;
	SceUIntPtr p;
	SceUInt32 n = 0;
	int first = 1, ret;

	for (p = dataList & ~7; isValidUltPointer(p); p = link.next & ~7) {
		ret = sceKernelCopyFromUserProc(pid, &link, (const void *)p, sizeof(link));
		if (ret != 0)
			return ret;
		if (!first) {
			if (ids != NULL && link.data != 0) {
				if (n >= maxData)
					return SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS;
				if (n < maxIds)
					ids[n] = link.data;
			}
			n++;
		}
		first = 0;
	}
	return n;
}
