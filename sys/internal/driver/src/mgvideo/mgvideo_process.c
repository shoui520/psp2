/* Descriptive source name: the original has no source-file debug records. */
#include "mgvideo_private.h"
#include <processmgr/processmgr_imports.h>
#include <usbstormg/usbstormg.h>
#include <psp2kern/kernel/proc_event.h>

int releaseProcessVideoContexts(int processId)
{
	MgVideoProcessReference *reference;
	MgVideoContext *context;
	MgVideoRecordStream *stream;
	int result, slot, intr, owner;
	unsigned int index, handle;

	result = ksceKernelLockMutex(driverMutex, 1, 0);
	if (result < 0)
		return result;
	if (videoReferences <= 0)
		goto unlock;
	owner = processId & ~(processId >> 31);
	if (recordStreams[0].processId == owner) {
		if (recordStreams[0].contentKeySlot >= 0) {
			SceMagicGateForDriver_144BFA76(recordStreams[0].contentKeySlot);
			recordStreams[0].contentKeySlot = -1;
		}
		recordStreams[0].active = 0;
		recordStreams[0].processId = -1;
	}
	if (recordStreams[1].processId == owner) {
		if (recordStreams[1].contentKeySlot >= 0) {
			SceMagicGateForDriver_144BFA76(recordStreams[1].contentKeySlot);
			recordStreams[1].contentKeySlot = -1;
		}
		recordStreams[1].active = 0;
		recordStreams[1].processId = -1;
	}
	if (processId < 0)
		goto unlock;
	for (reference = processReferences; reference != processReferences + 8;
	     reference++) {
		__builtin_prefetch(reference + 2);
		if (reference->processId != processId)
			continue;
		while (reference->references-- > 0) {
			handle = reference->contextHandle;
			if (videoReferences <= 0)
				continue;
			if ((int)handle >= 0 && (handle & 0x10000) &&
			    (context = videoContexts[(uint16_t)handle],
			     (uint16_t)handle <= 1) &&
			    context->references > 0 && context->state != 0) {
				if (--context->references == 0) {
					context->callbacks.terminate(context);
					if (context->contentKeySlot >= 0)
						SceMagicGateForDriver_144BFA76(context->contentKeySlot);
					if (context == videoContexts[0])
						slot = 0;
					else if (context == videoContexts[1])
						slot = 1;
					else
						goto unreference;
					ksceKernelDeleteEventFlag(context->eventFlag);
					videoContexts[slot] = 0;
					ksceKernelFreeHeapMemory(contextHeaps[slot], context);
					ksceKernelDeleteHeap(contextHeaps[slot]);
					contextHeaps[slot] = -1;
				}
			}
unreference:
			if (videoReferences <= 0)
				continue;
			if (--videoReferences != 0)
				continue;
			ksceKernelUnregisterProcEventHandler(processEventHandler);
			intr = ksceKernelCpuSuspendIntr();
			for (index = 0; index <= 1; index++) {
				if (recordStreams[index].active == 0)
					continue;
				index |= 0x20000;
				stream = &recordStreams[(uint16_t)index];
				if (stream->contentKeySlot >= 0) {
					SceMagicGateForDriver_144BFA76(stream->contentKeySlot);
					stream->contentKeySlot = -1;
				}
				stream->active = 0;
				stream->processId = -1;
			}
			ksceKernelCpuResumeIntr(intr);
			SceMagicGateForDriver_6D3B42E6();
		}
		reference->processId = -1;
		__builtin_prefetch(&reference[2].contextHandle);
		reference->contextHandle = -1;
	}
unlock:
	ksceKernelUnlockMutex(driverMutex, 1);
	return 0;
}

struct SceProcEventInvokeParam1;

int onVideoProcessKill(int processId, struct SceProcEventInvokeParam1 *event,
		       int eventType)
{
	return releaseProcessVideoContexts(processId);
}

int onVideoProcessExit(int processId, struct SceProcEventInvokeParam1 *event,
		       int eventType)
{
	return releaseProcessVideoContexts(processId);
}
