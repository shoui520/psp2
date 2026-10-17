/* Descriptive file name: no source-file debug records exist in the original. */
#include "mgvideo_private.h"
#include <usbstormg/usbstormg.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr_imports.h>
#include <psp2kern/kernel/sysclib.h>
#include "mgvideo_result.h"

int SceMagicGateForDriver_3EDD73EB(int handle, int index);

int initializeVideoContext(MgVideoContext *context, uint64_t *challenge)
{
	(void)challenge;
	context->contentKeySlot = -2;
	context->modeFlags = 32;
	return 0;
}

int terminateVideoContext(MgVideoContext *context)
{
	if (context->fileHandle >= 0)
		context->fileHandle = -1;
	return SceMagicGateForDriver_8E2E577E();
}

int removeVideoRecord(MgVideoContext *context, int index, void *record)
{
	(void)record;
	return SceMagicGateForDriver_3EDD73EB(context->fileHandle, index);
}

int module_start(SceSize arglen, const void *argp)
{
	int mutex = ksceKernelCreateMutex("SceMgVideoDrv", 0x8002, 0, 0);

	if (mutex < 0)
		return 2;
	driverMutex = mutex;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	if (driverMutex > 0) {
		ksceKernelDeleteMutex(driverMutex);
		driverMutex = -1;
	}
	return 0;
}

struct SceProcEventHandler;
extern const struct SceProcEventHandler videoProcessEventCallbacks;
int ksceKernelRegisterProcEventHandler(const char *name,
		const struct SceProcEventHandler *handler, int flags);
int ksceKernelUnregisterProcEventHandler(SceUID handler);
int SceMagicGateForDriver_2D5069DE(int slot, const MagicGateRecordHeader *record,
				unsigned int size);
int SceMagicGateForDriver_01FD0260(int slot, const void *input, int sourceSlot);
int SceMagicGateForDriver_4E8B4EB5(int slot, const void *input, int sourceSlot);
int SceMagicGateForDriver_07BDCE2F(void *output, int slot, int sourceSlot);
void notifyContextFinalized(void);
int readVideoEkb2Record(void *output, unsigned int capacity);

int createVideoContext(int type, int flags, int options)
{
	uint8_t probe[64] __attribute__((aligned(16)));
	MgVideoContext *context;
	MgVideoProcessReference *freeReference, *reference;
	MgVideoRecordStream *stream;
	int owner, heap, result, processId, handle, intr;
	unsigned int index;

	if (flags || type || options)
		return (int)0x8050c002;
	if (videoReferences > 0) {
		videoReferences++;
	} else {
#define INITIALIZE_PROCESS_REFERENCE(n) do { \
	processReferences[n].processId = -1; \
	processReferences[n].references = 0; \
	processReferences[n].contextHandle = 0; \
} while (0)
		INITIALIZE_PROCESS_REFERENCE(0);
		INITIALIZE_PROCESS_REFERENCE(1);
		INITIALIZE_PROCESS_REFERENCE(2);
		INITIALIZE_PROCESS_REFERENCE(3);
		INITIALIZE_PROCESS_REFERENCE(4);
		INITIALIZE_PROCESS_REFERENCE(5);
		INITIALIZE_PROCESS_REFERENCE(6);
		INITIALIZE_PROCESS_REFERENCE(7);
#undef INITIALIZE_PROCESS_REFERENCE
		memset(recordStreams, 0, sizeof(recordStreams));
		recordStreams[0].contentKeySlot = -1;
		recordStreams[0].processId = -1;
		recordStreams[1].contentKeySlot = -1;
		recordStreams[1].processId = -1;
		result = SceMagicGateForDriver_46A28FAA();
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			return result;
		}
		result = SceMagicGateForDriver_12CE769D(&videoRandomChallenge);
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			SceMagicGateForDriver_6D3B42E6();
			return result;
		}
		result = ksceKernelRegisterProcEventHandler("SceMgVideo",
						   &videoProcessEventCallbacks, 0);
		if (result < 0) {
			SceMagicGateForDriver_6D3B42E6();
			return result;
		}
		processEventHandler = result;
		videoReferences = 1;
	}
	owner = 1;
	context = videoContexts[1];
	if (context && context->driverType == 16)
		goto existing;
	if (context)
		owner = -1;
	context = videoContexts[0];
	if (context && context->driverType == 16) {
		owner = 0;
		goto existing;
	}
	if (!context)
		owner = 0;
	if (owner == -1) {
		result = (int)0x8050c015;
		goto unreference;
	}
	heap = contextHeaps[owner];
	if (heap <= 0) {
		heap = ksceKernelCreateHeap("mgvideo", sizeof(MgVideoContext), 0);
		if (heap < 0) {
			result = (int)0x8050c004;
			goto unreference;
		}
		contextHeaps[owner] = heap;
	}
	context = ksceKernelAllocHeapMemory(heap, sizeof(*context));
	if (!context) {
		result = (int)0x8050c004;
		ksceKernelDeleteHeap(heap);
		contextHeaps[owner] = -1;
		goto unreference;
	}
	result = ksceKernelCreateEventFlag("SceMgVideoDrv", 0x9000, -1, 0);
	if (result < 0) {
		ksceKernelFreeHeapMemory(heap, context);
		ksceKernelDeleteHeap(heap);
		contextHeaps[owner] = -1;
		goto unreference;
	}
	context->eventFlag = result;
	context->references = 0;
	context->driverType = 16;
	videoContexts[owner] = context;
existing:
	if (context->references > 0) {
		context->references++;
		goto reference;
	}
	context->state = 0;
	context->contentKeySlot = -1;
	if (context->driverType != 16) {
		result = (int)0x8050c002;
		goto destroy;
	}
	context->callbacks.getWrappedKey = SceMagicGateForDriver_07BDCE2F;
	context->callbacks.notifyFinalized = notifyContextFinalized;
	context->callbacks.writeRecord = writeVideoKeyringRecord;
	context->callbacks.unwrapDriverKey = SceMagicGateForDriver_4E8B4EB5;
	context->callbacks.installRecord = SceMagicGateForDriver_2D5069DE;
	context->callbacks.unwrapKey = SceMagicGateForDriver_01FD0260;
	context->callbacks.getFileInfo = SceMagicGateForDriver_D819D5C6;
	context->callbacks.initialize = initializeVideoContext;
	context->callbacks.terminate = terminateVideoContext;
	context->callbacks.readRecord = readVideoKeyringRecord;
	context->callbacks.removeRecord = removeVideoRecord;
	context->callbacks.resetRecord = removeVideoRecord;
	context->callbacks.getEkb2Record = readVideoEkb2Record;
	context->modeFlags = 32;
	context->parserDialect = -1;
	result = SceMagicGateForDriver_30DF2E82();
	if (result < 0)
		goto close;
	context->parserDialect = 1;
	result = context->callbacks.getEkb2Record(probe, sizeof(probe));
	if (result < 0)
		goto close;
	/* The API returns a truncated serialized record, not a partial C object. */
	memcpy(&context->deviceIdentity,
	       probe + offsetof(MagicGateBundledEkb2Record, header.tag),
	       sizeof(((MagicGateBundledEkb2Record *)0)->header.tag));
	result = SceMagicGateForDriver_7B052A10();
	if (result < 0)
		goto close;
	context->fileHandle = result;
	context->references = 1;
	context->state = 1;
reference:
	handle = owner | 0x10000;
	processId = sceKernelGetProcessId();
	result = processId;
	if (processId < 0)
		goto destroy;
	freeReference = 0;
	for (reference = processReferences; reference != processReferences + 8;
	     reference++) {
		if (reference->processId == -1) {
			if (!freeReference)
				freeReference = reference;
		} else if (reference->processId == processId &&
			   reference->contextHandle == handle) {
			result = reference->references++;
			if (result < 0)
				goto destroy;
			return handle;
		}
	}
	if (!freeReference) {
		result = (int)0x8050c015;
		goto destroy;
	}
	freeReference->processId = processId;
	freeReference->contextHandle = handle;
	freeReference->references = 1;
	return handle;
close:
	TRANSLATE_MAGICGATE_RESULT(result);
	if (context->parserDialect >= 0)
		SceMagicGateForDriver_8E2E577E();
destroy:
	if (context == videoContexts[0])
		owner = 0;
	else if (context == videoContexts[1])
		owner = 1;
	else
		goto unreference;
	ksceKernelDeleteEventFlag(context->eventFlag);
	videoContexts[owner] = 0;
	ksceKernelFreeHeapMemory(contextHeaps[owner], context);
	ksceKernelDeleteHeap(contextHeaps[owner]);
	contextHeaps[owner] = -1;
unreference:
	if (videoReferences > 0 && --videoReferences == 0) {
		ksceKernelUnregisterProcEventHandler(processEventHandler);
		intr = ksceKernelCpuSuspendIntr();
		index = 0;
		do {
			if (recordStreams[index].active) {
				index |= 0x20000;
				stream = &recordStreams[(uint16_t)index];
				if (stream->contentKeySlot >= 0) {
					SceMagicGateForDriver_144BFA76(stream->contentKeySlot);
					stream->contentKeySlot = -1;
				}
				stream->active = 0;
				stream->processId = -1;
			}
		} while (++index <= 1);
		ksceKernelCpuResumeIntr(intr);
		SceMagicGateForDriver_6D3B42E6();
	}
	return result;
}

int releaseVideoContext(int handle)
{
	MgVideoContext *context;
	MgVideoProcessReference *reference;
	MgVideoRecordStream *stream;
	int processId, references, owner, intr;
	unsigned int index;

	processId = sceKernelGetProcessId();
	if (processId >= 0) {
#define FIND_CONTEXT_REFERENCE(n) do { \
	if (processReferences[n].processId == processId && \
	    processReferences[n].contextHandle == handle) { \
		reference = &processReferences[n]; \
		goto found; \
	} \
} while (0)
		FIND_CONTEXT_REFERENCE(0);
		FIND_CONTEXT_REFERENCE(1);
		FIND_CONTEXT_REFERENCE(2);
		FIND_CONTEXT_REFERENCE(3);
		FIND_CONTEXT_REFERENCE(4);
		FIND_CONTEXT_REFERENCE(5);
		FIND_CONTEXT_REFERENCE(6);
		FIND_CONTEXT_REFERENCE(7);
#undef FIND_CONTEXT_REFERENCE
	}
	goto unreference;
found:
	if (--reference->references <= 0) {
		reference->processId = -1;
		reference->contextHandle = -1;
	}
unreference:
	references = videoReferences;
	if (references <= 0)
		return (int)0x8050c011;
	if (handle >= 0 && (handle & 0x10000)) {
		context = videoContexts[(uint16_t)handle];
		if ((uint16_t)handle <= 1 && context->references > 0 && context->state) {
			if (--context->references == 0) {
				context->callbacks.terminate(context);
				if (context->contentKeySlot >= 0)
					SceMagicGateForDriver_144BFA76(context->contentKeySlot);
				if (context == videoContexts[0])
					owner = 0;
				else if (context == videoContexts[1])
					owner = 1;
				else
					goto reload;
				ksceKernelDeleteEventFlag(context->eventFlag);
				videoContexts[owner] = 0;
				ksceKernelFreeHeapMemory(contextHeaps[owner], context);
				ksceKernelDeleteHeap(contextHeaps[owner]);
				contextHeaps[owner] = -1;
reload:
				references = videoReferences;
				if (references <= 0)
					return (int)0x8050c011;
			}
		}
	}
	references--;
	videoReferences = references;
	if (references == 0) {
		ksceKernelUnregisterProcEventHandler(processEventHandler);
		intr = ksceKernelCpuSuspendIntr();
		index = 0;
		do {
			while (!recordStreams[index].active) {
				if (++index > 1)
					goto resume;
			}
			index |= 0x20000;
			stream = &recordStreams[(uint16_t)index];
			if (stream->contentKeySlot >= 0) {
				SceMagicGateForDriver_144BFA76(stream->contentKeySlot);
				stream->contentKeySlot = -1;
			}
			stream->active = 0;
			stream->processId = -1;
		} while (++index <= 1);
resume:
		ksceKernelCpuResumeIntr(intr);
		SceMagicGateForDriver_6D3B42E6();
	}
	return references;
}

int waitVideoRecord(int handle, int index, int flags)
{
	MgVideoContext *context;
	SceUInt32 timeout, *waitTimeout;
	unsigned int shift, eventBits;
	int result, polling;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (index > 31)
		return (int)0x8050c002;
	if (flags & 2) {
		timeout = 0;
		waitTimeout = &timeout;
		polling = 1;
	} else {
		if (flags)
			return (int)0x8050c002;
		waitTimeout = 0;
		polling = 0;
	}
	shift = (uint8_t)index;
	eventBits = shift < 32 ? 1U << shift : 0;
	result = ksceKernelWaitEventFlag(context->eventFlag, eventBits, 5, 0,
				       waitTimeout);
	if (result >= 0)
		return 0;
	if (result == (int)0x80028005 && polling)
		return (int)0x8050c015;
	return result;
}

int signalVideoRecord(int handle, int index)
{
	MgVideoContext *context;
	unsigned int shift, eventBits;
	int result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (index > 31)
		return (int)0x8050c002;
	shift = (uint8_t)index;
	eventBits = shift < 32 ? 1U << shift : 0;
	result = ksceKernelSetEventFlag(context->eventFlag, eventBits);
	return result & (result >> 31);
}

int initializeVideoKeyring(int handle)
{
	MagicGateFileInfo info __attribute__((aligned(16)));
	MgVideoContext *context;
	MgVideoRecordStream *stream;
	unsigned int index;
	int result, intr;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	result = context->callbacks.getFileInfo(context->fileHandle, &info);
	if (result >= 0) {
		if (context->state > 1)
			return context->modeFlags;
	} else {
		TRANSLATE_MAGICGATE_RESULT(result);
		if (result != (int)0x8050c022)
			return result;
		context->state = 1;
	}
	intr = ksceKernelCpuSuspendIntr();
	index = 0;
	do {
		if (recordStreams[index].active) {
			index |= 0x20000;
			stream = &recordStreams[(uint16_t)index];
			if (stream->contentKeySlot >= 0) {
				SceMagicGateForDriver_144BFA76(stream->contentKeySlot);
				stream->contentKeySlot = -1;
			}
			stream->active = 0;
			stream->processId = -1;
		}
	} while (++index <= 1);
	ksceKernelCpuResumeIntr(intr);
	result = context->callbacks.initialize(context, &videoRandomChallenge);
	if (result < 0)
		return result;
	result = loadVideoKeyring(context);
	if (result < 0)
		return result;
	context->state = 2;
	return context->modeFlags;
}
