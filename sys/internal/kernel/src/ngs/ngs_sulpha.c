/*
 * Sulpha, the SDK's audio debugger: the trace of a system update, the
 * snapshot of one object, and the built-in module descriptions.
 */

#include <stddef.h>
#include <string.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>

#include "ngs_module_memory.h"
#include "ngs_system.h"
#include "ngs_rack.h"
#include "ngs_player.h"
#include <ngs/error.h>

#define NGS_SULPHA_TRACE_MAGIC		0x30405060
#define NGS_SULPHA_PCM_FORMAT		0x6d63706c	/* "lpcm" */
#define NGS_SULPHA_PCM_LAYOUT		0x02363173
#define NGS_SULPHA_PCM_SAMPLE_RATE	48000

/* what sceNgsSulphaGetInfoInternal reports */
#define NGS_SULPHA_OBJECT_SYSTEM	0x02420100
#define NGS_SULPHA_OBJECT_RACK		0x02420200
#define NGS_SULPHA_OBJECT_VOICE		0x02420300

enum NgsSulphaEventType {
	NGS_SULPHA_EVENT_END = 0,
	NGS_SULPHA_EVENT_SYSTEM = 1,
	NGS_SULPHA_EVENT_RACK = 2,
	NGS_SULPHA_EVENT_VOICE = 3,
	NGS_SULPHA_EVENT_PATCH = 4,
	NGS_SULPHA_EVENT_MODULE_PARAMS = 5,
	NGS_SULPHA_EVENT_PLAYER_STATUS = 6,
	NGS_SULPHA_EVENT_OUTPUT_DATA = 7,
};

/*
 * A buffer of events: a header, then events up to an END event. The events
 * are written into a byte buffer, so they alias everything.
 */
typedef struct NgsSulphaTraceHeader {
	unsigned int cycleCount;
	unsigned int magic;
	unsigned int eventCount;
	SceSize size;
} __attribute__((may_alias)) NgsSulphaTraceHeader;

typedef struct NgsSulphaEvent {
	unsigned int type;
	SceSize size;
} __attribute__((may_alias)) NgsSulphaEvent;

typedef struct NgsSulphaTrace {
	NgsSulphaTraceHeader *header;
	SceSize size;
	NgsSulphaEvent *cursor;
	unsigned int eventCount;
	SceSize remaining;
	SceSize used;
} NgsSulphaTrace;

typedef struct NgsSulphaSystemEvent {
	NgsSulphaEvent event;
	SceNgsHSynSystem system;
	unsigned int reserved12;
	unsigned int reserved16;
	unsigned int eventIndex;
	unsigned int modulesLinked;
	unsigned int maxRacks;
	unsigned int maxVoices;
	unsigned int granularity;
	unsigned int sampleRate;
	unsigned int elapsed;
} __attribute__((may_alias)) NgsSulphaSystemEvent;

typedef struct NgsSulphaRackEvent {
	NgsSulphaEvent event;
	SceNgsHRack rack;
	SceNgsHSynSystem system;
	unsigned int rackIndex;
	unsigned int eventIndex;
	unsigned int voiceCount;
	unsigned int order;
	unsigned int state;
	unsigned int channelsPerVoice;
	unsigned int patchesPerOutput;
	unsigned int reserved44;
} __attribute__((may_alias)) NgsSulphaRackEvent;

typedef struct NgsSulphaVoiceEvent {
	NgsSulphaEvent event;
	SceNgsHVoice voice;
	SceNgsHRack rack;
	unsigned int voiceIndex;
	unsigned int eventIndex;
	unsigned int moduleCount;
	unsigned int pauseState;
	unsigned int runState;
	unsigned int flags;
	unsigned int channels;
	int reserved24;
	unsigned int reserved48;
} __attribute__((may_alias)) NgsSulphaVoiceEvent;

typedef struct NgsSulphaPatchEvent {
	NgsSulphaEvent event;
	SceNgsHPatch patch;
	SceNgsHVoice source;
	unsigned int patchIndex;
	unsigned int eventIndex;
	unsigned int reserved23;
	unsigned int channels;
	unsigned int connected;	/* the patch feeds an input mixer */
	int destinationReserved24;
	SceNgsVolumeMatrix volumes;
} __attribute__((may_alias)) NgsSulphaPatchEvent;

typedef struct NgsSulphaModuleParamsEvent {
	NgsSulphaEvent event;
	NgsVoiceModuleParams *moduleParams;
	SceNgsHVoice voice;
	unsigned int moduleIndex;
	unsigned int eventIndex;
	unsigned int moduleId;
	unsigned int reserved28;
	unsigned int bypassed;
	SceSize paramsSize;
	unsigned char params[4];
} __attribute__((may_alias)) NgsSulphaModuleParamsEvent;

typedef struct NgsSulphaPlayerStatusEvent {
	NgsSulphaEvent event;
	void *context;
	NgsVoiceModuleParams *moduleParams;
	unsigned int moduleIndex;
	unsigned int eventIndex;
	unsigned int reserved24;
	unsigned int reserved28;
	unsigned int bufferOffset;
	unsigned short loopCount;
	signed char currentBuffer;
	unsigned char padding;
} __attribute__((may_alias)) NgsSulphaPlayerStatusEvent;

typedef struct NgsSulphaOutputDataEvent {
	NgsSulphaEvent event;
	void *state;
	NgsVoiceModuleParams *moduleParams;
	unsigned int moduleIndex;
	unsigned int eventIndex;
	unsigned int format;
	unsigned int layout;
	unsigned int channels;
	unsigned int sampleRate;
	unsigned int cycleCount;
	SceSize dataSize;
	unsigned char data[4];
} __attribute__((may_alias)) NgsSulphaOutputDataEvent;

_Static_assert(sizeof(NgsSulphaSystemEvent) == 48, "NgsSulphaSystemEvent size");
_Static_assert(sizeof(NgsSulphaRackEvent) == 48, "NgsSulphaRackEvent size");
_Static_assert(sizeof(NgsSulphaVoiceEvent) == 52, "NgsSulphaVoiceEvent size");
_Static_assert(sizeof(NgsSulphaPatchEvent) == 56, "NgsSulphaPatchEvent size");
_Static_assert(sizeof(NgsSulphaModuleParamsEvent) == 44,
	"NgsSulphaModuleParamsEvent size");
_Static_assert(sizeof(NgsSulphaPlayerStatusEvent) == 40,
	"NgsSulphaPlayerStatusEvent size");
_Static_assert(sizeof(NgsSulphaOutputDataEvent) == 52,
	"NgsSulphaOutputDataEvent size");

typedef struct NgsSulphaRegistration {
	unsigned int type;
	unsigned int handle;
	unsigned int reserved8;
} NgsSulphaRegistration;

static NgsSulphaTrace ngsSulphaTrace;
static SceUInt64 ngsSulphaUpdateTime;

static inline void ngsSulphaStartTrace(NgsSulphaTrace *trace,
				       const SceNgsBufferInfo *buffer)
{
	trace->size = buffer->size;
	trace->remaining = buffer->size - 20;
	trace->cursor = (NgsSulphaEvent *)((NgsSulphaTraceHeader *)buffer->data + 1);
	trace->used = 0;
	trace->header = buffer->data;
	trace->eventCount = 0;
	trace->cursor->type = NGS_SULPHA_EVENT_END;
}

static inline void ngsSulphaNextEvent(NgsSulphaTrace *trace, SceSize size)
{
	NgsSulphaEvent *next;

	trace->eventCount++;
	trace->remaining -= size;
	trace->used += size;
	next = (NgsSulphaEvent *)((unsigned char *)trace->cursor + size);
	next->type = NGS_SULPHA_EVENT_END;
	trace->cursor = next;
}

static inline void ngsSulphaEndTrace(NgsSulphaTrace *trace,
				     unsigned int cycleCount)
{
	NgsSulphaTraceHeader *header = trace->header;

	header->cycleCount = cycleCount;
	header->magic = NGS_SULPHA_TRACE_MAGIC;
	header->eventCount = trace->eventCount;
	header->size = (unsigned char *)trace->cursor - (unsigned char *)header;
}

static inline void ngsSulphaTraceSystem(NgsSulphaTrace *trace,
					NgsSynSystem *system,
					unsigned int elapsed)
{
	NgsSulphaSystemEvent *event = (NgsSulphaSystemEvent *)trace->cursor;

	if (event == NULL || trace->remaining < sizeof(*event))
		return;
	event->event.type = NGS_SULPHA_EVENT_SYSTEM;
	event->event.size = sizeof(*event);
	event->system = ngsGetSystemHandle(system);
	event->reserved12 = 0;
	event->reserved16 = 0;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->modulesLinked = system->ModulesLinked;
	event->maxRacks = system->MaxRacks;
	event->maxVoices = system->MaxVoices;
	event->granularity = system->Granularity;
	event->sampleRate = system->SampleRate;
	event->elapsed = elapsed;
	ngsSulphaNextEvent(trace, event->event.size);
}

static inline void ngsSulphaTraceRack(NgsSulphaTrace *trace, NgsSynRack *rack)
{
	NgsSulphaRackEvent *event = (NgsSulphaRackEvent *)trace->cursor;

	if (event == NULL || trace->remaining < sizeof(*event))
		return;
	event->event.type = NGS_SULPHA_EVENT_RACK;
	event->event.size = sizeof(*event);
	event->rack = ngsGetRackHandle(rack);
	event->system = ngsGetSystemHandle(rack->system);
	event->rackIndex = rack->index;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->voiceCount = rack->voiceCount;
	event->order = rack->reserved102;
	event->state = rack->state;
	event->channelsPerVoice = rack->channelsPerVoice;
	event->patchesPerOutput = rack->patchesPerOutput;
	event->reserved44 = 0;
	ngsSulphaNextEvent(trace, event->event.size);
}

static inline void ngsSulphaTraceModuleParams(NgsSulphaTrace *trace,
		NgsVoice *voice, const NgsRackModule *modules,
		unsigned int index, unsigned int updated)
{
	NgsSulphaModuleParamsEvent *event =
		(NgsSulphaModuleParamsEvent *)trace->cursor;
	NgsSynRack *rack;
	NgsVoiceDsp *dsp;
	NgsVoiceModuleParams *params;
	unsigned char *dspParams;
	unsigned int paramsOffset;
	SceSize size;

	if (modules == NULL || event == NULL || trace->remaining < sizeof(*event))
		return;
	rack = voice->rack;
	dsp = rack->voiceDsps[voice->index];
	params = &voice->moduleParams[index];
	dspParams = dsp->params;
	paramsOffset = dsp->rackDsp->layouts[index].paramsOffset;
	event->moduleParams = params;
	event->event.type = NGS_SULPHA_EVENT_MODULE_PARAMS;
	event->event.size = sizeof(*event);
	event->voice = ngsGetVoiceHandle(voice);
	event->moduleId = modules[index].moduleId;
	event->paramsSize = 0;
	event->reserved28 = 0;
	event->moduleIndex = index;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->bypassed = (dsp->bypassFlags >> index) & 1;
	size = rack->paramsSizes[index];
	if (size != 0 && size + event->event.size <= trace->remaining &&
	    (updated & (1 << index)) != 0) {
		event->event.size += (unsigned short)size;
		event->paramsSize = rack->paramsSizes[index];
		size = rack->paramsSizes[index];
		if (updated == ~0u) {
			if (size != 0)
				memcpy(event->params, params->params, size);
		} else if (size != 0) {
			memcpy(event->params, dspParams + paramsOffset, size);
		}
	}
	ngsSulphaNextEvent(trace, event->event.size);
}

static inline void ngsSulphaTracePatch(NgsSulphaTrace *trace, NgsPatch *patch)
{
	NgsSulphaPatchEvent *event = (NgsSulphaPatchEvent *)trace->cursor;

	if (patch == NULL || patch->source == NULL || event == NULL ||
	    trace->remaining < sizeof(*event))
		return;
	event->event.type = NGS_SULPHA_EVENT_PATCH;
	event->event.size = sizeof(*event);
	event->patch = ngsGetPatchHandle(patch);
	event->source = ngsGetVoiceHandle(patch->source);
	event->patchIndex = patch->index;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->reserved23 = patch->reserved23;
	event->channels = patch->channels;
	event->connected = patch->mixerContext != NULL;
	event->destinationReserved24 = patch->destination != NULL ?
		patch->destination->reserved24 : 0;
	if (patch->volumes != NULL)
		event->volumes = *patch->volumes;
	else
		memset(&event->volumes, 0, sizeof(event->volumes));
	ngsSulphaNextEvent(trace, event->event.size);
}

static inline void ngsSulphaWritePlayerStatus(NgsSulphaTrace *trace,
		NgsVoice *voice, const NgsRackModule *modules,
		unsigned int index, unsigned int moduleId)
{
	NgsSulphaPlayerStatusEvent *event =
		(NgsSulphaPlayerStatusEvent *)trace->cursor;
	const NgsPlayerStatus *status;
	NgsVoiceDsp *dsp;
	unsigned char *context;

	if (modules == NULL || event == NULL || trace->remaining < sizeof(*event))
		return;
	dsp = voice->rack->voiceDsps[voice->index];
	context = dsp->moduleMemory + dsp->rackDsp->layouts[index].contextOffset;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->moduleParams = &voice->moduleParams[index];
	event->moduleIndex = index;
	event->event.type = NGS_SULPHA_EVENT_PLAYER_STATUS;
	event->event.size = sizeof(*event);
	event->context = context;
	event->reserved28 = 4;
	if (moduleId == SCE_NGS_AT9_ID) {
		status = &((const NgsAt9PlayerContext *)context)->status;
		event->currentBuffer = status->currentBuffer;
		event->loopCount = status->loopCount;
		event->bufferOffset = status->bufferOffset;
	} else {
		status = &((const NgsPcmPlayerContext *)context)->status;
		event->currentBuffer = status->currentBuffer;
		event->loopCount = status->loopCount;
		event->bufferOffset = status->bufferOffset;
	}
	event->reserved24 = 0;
	ngsSulphaNextEvent(trace, event->event.size);
}

static __attribute__((noinline)) void ngsSulphaTracePlayerStatus(NgsSulphaTrace *trace, NgsVoice *voice,
		const NgsRackModule *modules, unsigned int index,
		unsigned int moduleId)
{
	if (modules == NULL || voice == NULL)
		return;
	ngsSulphaWritePlayerStatus(trace, voice, modules, index, moduleId);
}

void ngsSulphaTraceOutputData(NgsSulphaTrace *trace, NgsVoice *voice,
		const NgsRackModule *modules, unsigned int index)
{
	NgsSulphaOutputDataEvent *event =
		(NgsSulphaOutputDataEvent *)trace->cursor;
	NgsSynSystem *system;
	NgsVoiceDsp *dsp;
	unsigned char *state;
	SceSize size;

	if (modules == NULL || voice == NULL || event == NULL ||
	    trace->remaining < sizeof(*event))
		return;
	system = voice->rack->system;
	dsp = voice->rack->voiceDsps[voice->index];
	state = (unsigned char *)dsp->inputMixerMemory +
		dsp->rackDsp->layouts[index].stateOffset;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->moduleParams = &voice->moduleParams[index];
	event->moduleIndex = index;
	event->event.type = NGS_SULPHA_EVENT_OUTPUT_DATA;
	event->event.size = sizeof(*event);
	event->state = state;
	event->format = NGS_SULPHA_PCM_FORMAT;
	event->layout = NGS_SULPHA_PCM_LAYOUT;
	event->dataSize = 0;
	event->channels = voice->channels;
	event->sampleRate = NGS_SULPHA_PCM_SAMPLE_RATE;
	event->cycleCount = system->CycleCount;
	size = dsp->inputMixerMemorySize;
	if (size + sizeof(*event) <= trace->remaining) {
		event->dataSize = size;
		event->event.size = (unsigned short)size + sizeof(*event);
		if (dsp->inputMixerMemorySize != 0)
			memcpy(event->data, state, dsp->inputMixerMemorySize);
	}
	ngsSulphaNextEvent(trace, event->event.size);
}

void ngsSulphaTraceVoice(NgsSulphaTrace *trace, NgsVoice *voice)
{
	NgsSulphaVoiceEvent *event = (NgsSulphaVoiceEvent *)trace->cursor;
	NgsSynRack *rack;

	if (voice == NULL)
		return;
	if (event == NULL || trace->remaining < sizeof(*event))
		return;
	rack = voice->rack;
	event->event.type = NGS_SULPHA_EVENT_VOICE;
	event->event.size = sizeof(*event);
	event->voice = ngsGetVoiceHandle(voice);
	event->rack = ngsGetRackHandle(voice->rack);
	event->voiceIndex = voice->index;
	event->eventIndex = (unsigned short)trace->eventCount;
	event->pauseState = voice->playState.pauseState;
	event->runState = voice->playState.runState;
	event->flags = voice->flags;
	event->channels = voice->channels;
	event->reserved24 = voice->reserved24;
	event->reserved48 = 0;
	event->moduleCount = rack->moduleList.moduleCount;
	ngsSulphaNextEvent(trace, event->event.size);
}

/* Starts the trace of an update: the parameters the batch changed, then
 * the voices and patches that changed since the last update. */
void ngsSulphaTraceUpdate(NgsSynSystem *system, NgsDspBatchEntry *entries,
			  int entryCount, SceNgsBufferInfo *buffer)
{
	NgsSulphaTrace *trace = &ngsSulphaTrace;
	const NgsRackModule *modules;
	NgsVoiceDsp *dsp;
	NgsVoice *voice;
	NgsSynRack *rack;
	unsigned int updated, i, j, k, patchCount;
	unsigned short index;
	int entry;

	if (entries == NULL || system->synthUpdateCallback == NULL ||
	    buffer == NULL)
		return;
	ngsSulphaStartTrace(trace, buffer);
	for (entry = 0; entry < entryCount; entry++) {
		if (entries[entry].status != 0)
			continue;
		__builtin_prefetch(&entries[entry + 2].heapOffset);
		dsp = (NgsVoiceDsp *)((unsigned char *)system->context->systemHeap +
			entries[entry].heapOffset);
		voice = dsp->voice;
		if ((voice->dirtyFlags2 & 6) == 0)
			continue;
		updated = dsp->updatedParams;
		rack = voice->rack;
		modules = rack->moduleList.modules;
		for (index = 0; index < rack->moduleList.moduleCount; index++) {
			if ((updated & (1 << index)) != 0)
				ngsSulphaTraceModuleParams(trace, voice,
					modules, index, updated);
		}
	}
	for (i = 0; i < system->MaxRacks; i++) {
		rack = system->processingRacks[i];
		if (rack == NULL || rack->voiceCount == 0)
			continue;
		for (j = 0; j < rack->voiceCount; j++) {
			voice = &rack->voices[j];
			if ((voice->dirtyFlags2 & 8) != 0)
				ngsSulphaTraceVoice(trace, voice);
			if ((voice->dirtyFlags2 & 1) != 0) {
				patchCount = rack->patchesPerOutput *
					rack->voiceOutputCount;
				for (k = 0; k < patchCount; k++)
					ngsSulphaTracePatch(trace,
						&voice->patches[k]);
			}
			voice->dirtyFlags2 = 0;
		}
	}
	ngsSulphaUpdateTime = sceKernelGetSystemTimeWide();
}

/* Ends the trace of an update: the system, then the players' status and
 * the outputs' data after the codec engine ran. */
void ngsSulphaEndUpdate(NgsSynSystem *system, NgsDspBatchEntry *entries,
			int entryCount)
{
	NgsSulphaTrace *trace = &ngsSulphaTrace;
	const NgsRackModule *modules;
	NgsVoiceDsp *dsp;
	NgsVoice *voice;
	NgsSynRack *rack;
	unsigned int moduleId;
	unsigned short index;
	SceUInt64 now;
	int entry;

	if (entries == NULL || system->synthUpdateCallback == NULL)
		return;
	now = sceKernelGetSystemTimeWide();
	ngsSulphaTraceSystem(trace, system,
		(unsigned int)now - (unsigned int)ngsSulphaUpdateTime);
	for (entry = 0; entry < entryCount; entry++) {
		if (entries[entry].status != 0)
			continue;
		__builtin_prefetch(&entries[entry + 2].heapOffset);
		dsp = (NgsVoiceDsp *)((unsigned char *)system->context->systemHeap +
			entries[entry].heapOffset);
		voice = dsp->voice;
		rack = voice->rack;
		modules = rack->moduleList.modules;
		for (index = 0; index < rack->moduleList.moduleCount; index++) {
			moduleId = modules[index].moduleId;
			if (moduleId == SCE_NGS_PLAYER_ID)
				ngsSulphaWritePlayerStatus(trace, voice,
					modules, index, SCE_NGS_PLAYER_ID);
			else if (moduleId == SCE_NGS_AT9_ID)
				ngsSulphaWritePlayerStatus(trace, voice,
					modules, index, SCE_NGS_AT9_ID);
			else if (moduleId == SCE_NGS_OUTPUT_ID)
				ngsSulphaTraceOutputData(trace, voice,
					modules, index);
		}
	}
	ngsSulphaEndTrace(trace, system->CycleCount);
}

SceInt32 sceNgsSulphaGetInfoInternal(const NgsSulphaRegistration *registration,
				     SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSulphaRegistration object;
	SceNgsBufferInfo callerBuffer;
	SceNgsBufferInfo buffer;
	NgsSulphaTrace trace;
	NgsSynSystem *system;
	NgsSynRack *rack;
	NgsVoice *voice;
	unsigned int moduleId, k, patchCount;
	unsigned short index;
	SceUID memBlock;
	SceSize size;
	int result;

	memBlock = 0;
	buffer.data = NULL;
	buffer.size = 0;
	result = ngsCopyFromCaller(&object, registration, sizeof(object), 1);
	if (result < 0)
		goto release;
	result = ngsCopyFromCaller(&callerBuffer, info, sizeof(callerBuffer), 1);
	if (result < 0)
		goto release;
	result = ngsAllocateDebugBuffer(&buffer, &memBlock, callerBuffer.size);
	if (result < 0)
		goto release;
	ngsSulphaStartTrace(&trace, &buffer);
	switch (object.type) {
	case NGS_SULPHA_OBJECT_RACK:
		rack = ngsGetRackFromHandle(object.handle);
		if (rack == NULL)
			goto error;
		ngsSulphaTraceRack(&trace, rack);
		break;
	case NGS_SULPHA_OBJECT_VOICE:
		voice = ngsGetVoiceFromHandle(object.handle);
		if (voice == NULL)
			goto error;
		rack = voice->rack;
		ngsSulphaTraceVoice(&trace, voice);
		patchCount = rack->patchesPerOutput * rack->voiceOutputCount;
		for (k = 0; k < patchCount; k++)
			ngsSulphaTracePatch(&trace, &voice->patches[k]);
		for (index = 0; index < rack->moduleList.moduleCount; index++) {
			moduleId = rack->moduleList.modules[index].moduleId;
			ngsSulphaTraceModuleParams(&trace, voice, rack->moduleList.modules,
						   index, ~0u);
			if (moduleId == SCE_NGS_PLAYER_ID)
				ngsSulphaTracePlayerStatus(&trace, voice,
					rack->moduleList.modules, index, SCE_NGS_PLAYER_ID);
			else if (moduleId == SCE_NGS_AT9_ID)
				ngsSulphaTracePlayerStatus(&trace, voice,
					rack->moduleList.modules, index, SCE_NGS_AT9_ID);
			else if (moduleId == SCE_NGS_OUTPUT_ID)
				ngsSulphaTraceOutputData(&trace, voice,
					rack->moduleList.modules, index);
		}
		break;
	case NGS_SULPHA_OBJECT_SYSTEM:
		system = ngsGetSystemFromHandle(object.handle);
		if (system == NULL)
			goto error;
		ngsSulphaTraceSystem(&trace, system, 0);
		break;
	default:
		goto error;
	}
	ngsSulphaEndTrace(&trace, 0);
	if (trace.used > trace.size)
		goto error;
	size = trace.header->size + 4;
	if (size >= buffer.size)
		size = buffer.size;
	result = ngsCopyToCaller(callerBuffer.data, buffer.data, size, 1);
	goto release;
error:
	result = SCE_NGS_ERROR;
release:
	ngsReleaseDebugBuffer(&buffer, memBlock);
	sceSyscallLeave(syscall);
	return result;
}

typedef union SulphaNgsModuleQuery {
	struct {
		unsigned int version;
		unsigned int moduleId;
		unsigned int inputCount;
		unsigned int outputCount;
		char info[256];
		unsigned int paramInterfaceCount;
		unsigned int dspControl;
		unsigned int flags;
		SceSize paramsSize;
	} module;
	struct {
		unsigned int paramsId;
		SceSize size;
		unsigned int paramCount;
	} interface;
	struct {
		char name[64];
		char units[32];
		char description[128];
		unsigned int type;
		NgsParamValue minimum;
		NgsParamValue maximum;
		NgsParamValue defaultValue;
	} parameter;
} SulphaNgsModuleQuery;

typedef char SulphaNgsModuleQuerySize[(sizeof(SulphaNgsModuleQuery) == 288) ? 1 : -1];

int sceNgsSulphaQueryModuleInternal(unsigned int type, SulphaNgsModuleQuery *debug,
				   unsigned int moduleId, unsigned int index)
{
	NgsModuleDefinition *module;
	struct NgsModuleInitParams params;
	NgsModuleMemoryInfo memory;
	SulphaNgsModuleQuery query;
	const struct NgsModuleParamInterface *interface;
	const NgsModuleParamDescription *parameter;
	unsigned int syscall = sceSyscallEnter();
	unsigned int interfaceIndex = index >> 16;
	int result;

	module = NULL;
	result = ngsFindBuiltinModule(moduleId, &module);
	if (module == NULL) {
		result = (int)0x804a0002;
		goto leave;
	}
	if (result < 0)
		goto leave;
	switch (type) {
	case 1:
		if (module->paramInterfaces == NULL)
			return (int)0x804a0002;
		if (interfaceIndex >= module->paramInterfaceCount)
			goto leave;
		interface = module->paramInterfaces[interfaceIndex];
		memset(&query, 0, sizeof(query));
		if (interface != NULL) {
			query.interface.paramsId = interface->paramsId;
			query.interface.size = interface->size;
			query.interface.paramCount = interface->paramCount;
		}
		break;
	case 0:
		memset(&query, 0, sizeof(query));
		query.module.version = module->version;
		query.module.moduleId = module->moduleId;
		query.module.inputCount = module->inputCount;
		query.module.outputCount = module->outputCount;
		query.module.paramInterfaceCount = module->paramInterfaceCount;
		query.module.dspControl = module->dspControl;
		query.module.flags = module->flags & 1;
		strncpy(query.module.info, module->info, sizeof(query.module.info));
		if (module->getMemoryInfo != NULL) {
			params.paramInterfaceCount = module->paramInterfaceCount;
			params.paramInterfaces = module->paramInterfaces;
			params.nGranularity = 256;
			params.nSampleRate = 48000;
			params.nChannelsPerVoice = 2;
			if (module->getMemoryInfo(&params, &memory) == 0)
				query.module.paramsSize = memory.paramsSize;
		}
		break;
	case 2:
		if (interfaceIndex >= module->paramInterfaceCount)
			goto leave;
		interface = module->paramInterfaces[interfaceIndex];
		memset(&query, 0, sizeof(query));
		if (interface == NULL || (unsigned short)index >= interface->paramCount)
			goto leave;
		parameter = &interface->params[(unsigned short)index];
		if (parameter->name != NULL)
			strncpy(query.parameter.name, parameter->name,
				sizeof(query.parameter.name));
		if (parameter->units != NULL)
			strncpy(query.parameter.units, parameter->units,
				sizeof(query.parameter.units));
		if (parameter->description != NULL)
			strncpy(query.parameter.description, parameter->description,
				sizeof(query.parameter.description));
		query.parameter.minimum = parameter->minimum;
		query.parameter.maximum = parameter->maximum;
		query.parameter.defaultValue = parameter->defaultValue;
		query.parameter.type = parameter->type;
		result = ngsCopyToCaller(debug, &query, sizeof(query), 1);
		goto leave;
	default:
		goto leave;
	}
	result = ngsCopyToCaller(debug, &query, sizeof(query), 1);
leave:
	sceSyscallLeave(syscall);
	return result;
}
