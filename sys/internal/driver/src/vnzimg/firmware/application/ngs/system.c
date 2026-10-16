/*
 * The NGS systems (RPC 320-327): the module table and its work memory,
 * systems and racks, and the update that runs every voice of a system as
 * VThread jobs.
 *
 * Everything here lives in memory the ARM shares, so each object is purged
 * from this core's cache before it is read and flushed after it is written.
 */
#include "ngs.h"

#define LINE		64
#define L2_LINE		256

/* the cache lines that hold [address, address + size) */
static uint32_t lineStart(uint32_t address)
{
	return address & ~(LINE - 1);
}

static uint32_t lineLength(uint32_t address, uint32_t size)
{
	return ((address + size + LINE - 1) & ~(LINE - 1)) - lineStart(address);
}

/* the transfers of an update: dropped from the caches before it runs */
static void transfersPrepare(NgsUpdate *update)
{
	uint32_t i, done;

	cachePurgeRange((uint32_t)update->transfers,
		ngsRoundUp(update->transferCapacity * sizeof(NgsTransfer), L2_LINE));
	for (i = 0; i < update->transferCount; i++) {
		NgsTransfer *transfer = &update->transfers[i];

		cacheL2Invalidate(transfer->inputAddress, transfer->inputSize);
		cacheL2Invalidate(transfer->outputAddress, transfer->outputSize);
		cacheL2Invalidate(transfer->output2Address, transfer->output2Size);
		done = (uint32_t)transfer->done;
		cachePurgeRange(lineStart(done), lineLength(done, 4));
		*transfer->done = 0;
		cacheFlushRange(lineStart(done), lineLength(done, 4));
	}
}

/* and written back to the ARM after it */
static void transfersFinish(NgsUpdate *update)
{
	uint32_t i;

	cachePurgeRange((uint32_t)update->transfers,
		ngsRoundUp(update->transferCapacity * sizeof(NgsTransfer), L2_LINE));
	for (i = 0; i < update->transferCount; i++) {
		cacheL2Writeback(update->transfers[i].outputAddress,
			update->transfers[i].outputSize);
		cacheL2Writeback(update->transfers[i].output2Address,
			update->transfers[i].output2Size);
	}
}

/* an update that failed: its jobs are taken back */
static void updateAbort(NgsSystem *system)
{
	int32_t count = system->maxVoices + system->maxRacks + 33;
	VkVThreadContext *context;
	int32_t i;

	if (system->context == NULL)
		return;
	context = vkVThreadContextLock(system->context);
	if (context == NULL)
		return;
	for (i = 0; i < count; i++)
		vkVThreadJobClear(context, i);
	vkVThreadContextUnlock(context);
}

static void voiceJob(uint32_t contextArgument, uint32_t voiceArgument, uint32_t next,
	uint32_t systemArgument);
static void barrierJob(uint32_t contextArgument, uint32_t barrierArgument, uint32_t thread,
	uint32_t size);
static void taskJob(uint32_t contextArgument, uint32_t taskArgument, uint32_t signal,
	uint32_t systemArgument);

/* the first barrier after a job, or the one before */
static int32_t nextBarrier(NgsJob *jobs, int32_t from, int32_t count, int32_t barrier)
{
	int32_t i;

	for (i = from; i < count; i++)
		if (jobs[i].type == NGS_JOB_BARRIER)
			return i;
	return barrier;
}

/*
 * Run an update: every job is set in the system's VThread context, and the
 * calling thread waits until the last barrier wakes it.
 */
static int32_t updateRun(NgsSystem *system, NgsUpdate *update, int32_t count)
{
	NgsJob *jobs = update->jobs;
	VkVThreadContext *context;
	int32_t barrier, failed, i;
	uint32_t result;
	int32_t error;

	transfersPrepare(update);
	context = vkVThreadContextLock(system->context);
	barrier = nextBarrier(jobs, 0, count, -1);

	failed = 0;
	for (i = 0; i < count; i++) {
		NgsJob *job = &jobs[i];

		if (job->type == NGS_JOB_VOICE) {
			result = vkVThreadJobSet(context, i, job->wanted, voiceJob,
				(uint32_t)system->context,
				(uint32_t)system->update + job->offset, barrier,
				(uint32_t)system);
			if (result) {
				failed = 1;
				break;
			}
		} else if (job->type == NGS_JOB_BARRIER) {
			NgsBarrier *object = (NgsBarrier *)job->offset;

			cachePurgeInvalidate((uint32_t)object, ngsRoundUp(sizeof(NgsBarrier), L2_LINE));
			if (object->next != -1)
				result = vkVThreadJobSet(context, i, job->wanted, barrierJob,
					(uint32_t)system->context, (uint32_t)object,
					(uint32_t)vnzThreadCurrent(), object->size);
			else
				result = vkVThreadJobSetNotify(context, i, job->wanted, barrierJob,
					(uint32_t)system->context, (uint32_t)object,
					(uint32_t)vnzThreadCurrent(), object->size,
					vnzThreadCurrent(), 1);
			if (result) {
				failed = 2;
				break;
			}
			barrier = nextBarrier(jobs, i + 1, count, barrier);
		} else if (job->type == NGS_JOB_TASK) {
			result = vkVThreadJobSet(context, i, 0, taskJob,
				(uint32_t)system->context, job->offset, job->wanted,
				(uint32_t)system);
			if (result) {
				failed = 3;
				break;
			}
		} else if (job->type != NGS_JOB_NONE) {
			failed = 4;
			break;
		}
	}
	vkVThreadContextUnlock(context);

	if (failed == 0) {
		vnzThreadWaitFlags(1);
		error = 0;
	} else {
		updateAbort(system);
		error = SCE_NGS_ERROR_INTERNAL_PROCESSING;
	}
	transfersFinish(update);
	return error;
}

/* RPC 321, sceNgsSystemUpdate: run one update of a system */
int32_t ngsSystemUpdate(NgsGlobal *global, NgsUpdate *update, uint32_t size, uint32_t index)
{
	NgsSystem *system;
	int32_t result;

	cachePurgeRange((uint32_t)update, L2_LINE);
	cacheL2Invalidate((uint32_t)update, size);
	cachePurgeRange((uint32_t)update->jobs,
		ngsRoundUp(update->jobCount * sizeof(NgsJob), L2_LINE));
	cachePurgeInvalidate((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	cachePurgeRange((uint32_t)global->moduleTable, NGS_MODULE_TABLE_HEADER);

	system = global->systems[index];
	cachePurgeRange((uint32_t)system, LINE);
	system->update = (uint8_t *)update;
	cacheFlushRange((uint32_t)system, LINE);

	result = updateRun(system, update, update->jobCount);
	cacheL2Writeback((uint32_t)update, size);
	return result;
}

static void moduleTableRegister(NgsModuleTable *table)
{
	static const NgsModuleType types[] = {
		{ SCE_NGS_COMPRESSOR_ID, ngsCompressorInit, ngsCompressorProcess, 0 },
		{ SCE_NGS_DISTORTION_ID, ngsDistortionInit, ngsDistortionProcess, 0 },
		{ SCE_NGS_ENVELOPE_ID, ngsEnvelopeInit, ngsEnvelopeProcess, 0 },
		{ SCE_NGS_FILTER_ID, ngsFilterInit, ngsFilterProcess, 0 },
		{ SCE_NGS_INPUT_MIXER_ID, ngsInputMixerInit, ngsInputMixerProcess, 0 },
		{ SCE_NGS_MIXER_ID, ngsMixerInit, ngsMixerProcess, 0 },
		{ SCE_NGS_OUTPUT_ID, ngsOutputInit, ngsOutputProcess, 0 },
		{ SCE_NGS_PARAM_EQ_ID, ngsParamEqInit, ngsParamEqProcess, 0 },
		{ SCE_NGS_PAUSER_ID, ngsPauserInit, ngsPauserProcess, 0 },
		{ SCE_NGS_PLAYER_ID, ngsPlayerInit, ngsPlayerProcess, 0 },
		{ SCE_NGS_REVERB_ID, ngsReverbInit, ngsReverbProcess,
			(uint32_t)ngsReverbAuxiliary },
		{ SCE_NGS_GENERATOR_ID, ngsGeneratorInit, ngsGeneratorProcess, 0 },
		{ SCE_NGS_AT9_ID, ngsAt9Init, ngsAt9Process, 0 },
		{ SCE_NGS_DELAY_ID, ngsDelayInit, ngsDelayProcess, 0 },
		{ SCE_NGS_PITCHSHIFT_ID, ngsPitchShiftInit, ngsPitchShiftProcess, 0 },
	};
	uint32_t i;

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		table->types[i] = types[i];
	for (; i < NGS_MODULE_TYPES; i++) {
		table->types[i].moduleId = 0;
		table->types[i].initialize = NULL;
		table->types[i].process = NULL;
		table->types[i].auxiliary = 0;
	}
}

/* a module id to its functions; the id is replaced by the process function */
static int32_t moduleLookup(NgsModuleTable *table, uint32_t *process, uint32_t *initialize)
{
	uint32_t moduleId = *process;
	NgsModuleType *type = table->types;
	int32_t i;

	for (i = 0; i < NGS_MODULE_TYPES; i++, type++) {
		if (type->moduleId == moduleId) {
			*process = (uint32_t)type->process;
			*initialize = (uint32_t)type->initialize;
			return 0;
		}
	}
	return SCE_NGS_ERROR_MODULE_NOT_AVAIL;
}

static NgsModuleTable *moduleTableAllocate(uint32_t *size)
{
	uint32_t length = ngsRoundUp256(NGS_MODULE_TABLE_SIZE);
	NgsModuleTable *table;

	*size = length;
	table = (NgsModuleTable *)rpcAllocate(1, length);
	if (table == NULL) {
		*size = 0;
		return NULL;
	}
	memset(table, 0, length);
	cacheFlushRange((uint32_t)table, length);
	table->size = length;
	return table;
}

static NgsSystem *systemAllocate(NgsSystemInitParams *params, uint32_t *size)
{
	uint32_t length = ngsRoundUp(params->nMaxRacks * 8 + LINE, LINE);
	NgsSystem *system = (NgsSystem *)rpcAllocate(1, length);

	*size = length;
	if (system == NULL) {
		*size = 0;
		return NULL;
	}
	memset(system, 0, length);
	cacheFlushRange((uint32_t)system, length);
	system->size = length;
	return system;
}

static void moduleTableFree(NgsModuleTable **holder)
{
	NgsModuleTable *table = *holder;
	uint32_t i;

	for (i = 0; i < NGS_WORK_BUFFERS; i++)
		vnzLockFree(table->workLocks[i]);
	vnzLockFree(table->lock);
	rpcFree(1, (uint32_t)*holder, (*holder)->size);
	*holder = NULL;
}

static void systemFree(NgsSystem **holder)
{
	NgsSystem *system = *holder;

	vkVThreadContextDelete(system->context);
	vkVThreadResourceDelete(system->resource);
	rpcFree(1, (uint32_t)system, system->size);
	*holder = NULL;
}

static int32_t moduleTableInitialize(NgsModuleTable *table, uint32_t size)
{
	int32_t i;

	cachePurgeRange((uint32_t)table, size);
	table->workBuffers = (uint8_t *)table + NGS_MODULE_TABLE_HEADER;
	table->size = size;
	table->workBufferSize = NGS_WORK_BUFFER_SIZE;
	for (i = 0; i < NGS_WORK_BUFFERS; i++)
		table->workLocks[i] = vnzLockAllocate();
	table->initializedMark = 6;
	table->lock = vnzLockAllocate();
	moduleTableRegister(table);
	cacheFlushWriteback((uint32_t)table, size);
	return 0;
}

static int32_t systemInitialize(NgsSystem *system, NgsModuleTable *table,
	NgsSystemInitParams *params, uint32_t size)
{
	uint8_t *rackSlots = (uint8_t *)system + LINE;

	cachePurgeRange((uint32_t)system, size);
	system->moduleTable = table;
	system->size = size;
	system->sampleRate = params->nSampleRate;
	system->granularity = params->nGranularity;
	system->maxRacks = params->nMaxRacks;
	system->maxVoices = params->nMaxVoices;
	system->rackSlots = rackSlots;
	if (ngsRoundUp((uint32_t)(rackSlots + params->nMaxRacks * 8) - (uint32_t)system,
		LINE) != size)
		return SCE_NGS_ERROR;

	system->resource = vkVThreadResourceCreate(0x2000);
	if (system->resource == NULL)
		return SCE_NGS_ERROR_INTERNAL_ALLOC;
	system->context = vkVThreadContextCreate(system->maxVoices + system->maxRacks + 33,
		system->resource);
	if (system->context == NULL)
		return SCE_NGS_ERROR_INTERNAL_ALLOC;
	cacheFlushRange((uint32_t)system, size);
	return 0;
}

/*
 * RPC 320, sceNgsSystemInit: a system in a free slot, the module table made
 * with the first. Returns the slot.
 */
int32_t ngsSystemCreate(NgsGlobal *global, uint32_t limits, uint32_t granularity,
	uint32_t sampleRate)
{
	NgsSystemInitParams params;
	uint32_t systemSize = 0, tableSize;
	NgsSystem **slot;
	int32_t result, i;

	cachePurgeInvalidate((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	params.nSampleRate = sampleRate;
	params.nGranularity = granularity;
	params.nMaxRacks = limits & 0xffff;
	params.nMaxVoices = (limits >> 16) & 0xffff;

	for (i = 0, slot = global->systems; i < NGS_SYSTEMS; i++, slot++) {
		if (*slot)
			continue;
		if (++global->systemCount == 1) {
			global->moduleTable = moduleTableAllocate(&tableSize);
			if (global->moduleTable == NULL)
				break;
			result = moduleTableInitialize(global->moduleTable, tableSize);
			if (result)
				goto release;
		}

		*slot = systemAllocate(&params, &systemSize);
		if (*slot) {
			result = systemInitialize(*slot, global->moduleTable, &params, systemSize);
			if (result)
				systemFree(slot);
		} else {
			result = SCE_NGS_ERROR_INTERNAL_ALLOC;
		}
		if (*slot == NULL || result)
			goto release;

		cacheFlushWriteback((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
		cacheFlushRange((uint32_t)*slot, (*slot)->size);
		return i;
	}
	return SCE_NGS_ERROR_INTERNAL_ALLOC;

release:
	if (--global->systemCount == 0)
		moduleTableFree(&global->moduleTable);
	return result;
}

/* RPC 322, sceNgsSystemRelease */
int32_t ngsSystemRelease(NgsGlobal *global, int32_t index)
{
	NgsModuleTable *table;
	NgsSystem **slot;

	if (index >= NGS_SYSTEMS || index < 0 || global == NULL)
		return SCE_NGS_ERROR;
	cachePurgeInvalidate((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	if (global->systemCount == 0)
		return SCE_NGS_ERROR;
	slot = &global->systems[index];
	if (*slot == NULL)
		return SCE_NGS_ERROR;

	cachePurgeRange((uint32_t)*slot, LINE);
	systemFree(slot);
	if (--global->systemCount == 0) {
		table = global->moduleTable;
		cachePurgeRange((uint32_t)table, NGS_MODULE_TABLE_HEADER);
		moduleTableFree(&global->moduleTable);
		cacheFlushRange((uint32_t)table, NGS_MODULE_TABLE_HEADER);
	}
	cacheFlushWriteback((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	return 0;
}

/* RPC 323, sceNgsRackInit: the rack's module ids resolved, its lock made */
int32_t ngsRackInit(NgsGlobal *global, int32_t index, NgsRack *rack, uint32_t size)
{
	volatile uint8_t *lock;
	uint32_t i;

	if (index >= NGS_SYSTEMS || index < 0)
		return SCE_NGS_ERROR;
	cachePurgeInvalidate((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	if (global->moduleTable == NULL)
		return SCE_NGS_ERROR;
	cachePurgeRange((uint32_t)global->moduleTable, NGS_MODULE_TABLE_HEADER);
	if (global->systemCount == 0 || global->systems[index] == NULL)
		return SCE_NGS_ERROR;

	cachePurgeInvalidate((uint32_t)rack, size);
	for (i = 0; i < rack->moduleCount; i++)
		if (moduleLookup(global->moduleTable, (uint32_t *)&rack->process[i],
			(uint32_t *)&rack->initialize[i]))
			return SCE_NGS_ERROR_MODULE_NOT_AVAIL;
	lock = vnzLockAllocate();
	rack->lock = lock;
	cacheFlushWriteback((uint32_t)rack, size);
	return lock ? 0 : SCE_NGS_ERROR_INTERNAL_ALLOC;
}

/* RPC 324, sceNgsRackRelease */
int32_t ngsRackRelease(NgsGlobal *global, int32_t index, NgsRack *rack, uint32_t size)
{
	cachePurgeInvalidate((uint32_t)rack, size);
	if (rack->lock)
		vnzLockFree(rack->lock);
	rack->lock = NULL;
	cacheFlushWriteback((uint32_t)rack, size);
	return 0;
}

/* RPC 325 and 326: an array of locks for the ARM */
int32_t ngsLocksCreate(NgsLockArray *locks, uint32_t size)
{
	uint32_t i, j;
	int32_t result = 0;

	cachePurgeInvalidate((uint32_t)locks, size);
	for (i = 0; i < locks->count; i++) {
		locks->locks[i] = vnzLockAllocate();
		if (locks->locks[i] == NULL) {
			for (j = 0; j < i; j++) {
				vnzLockFree(locks->locks[j]);
				locks->locks[j] = NULL;
			}
			result = SCE_NGS_ERROR_INTERNAL_ALLOC;
			break;
		}
	}
	cacheFlushWriteback((uint32_t)locks, size);
	return result;
}

int32_t ngsLocksDelete(NgsLockArray *locks, uint32_t size)
{
	uint32_t i;

	cachePurgeInvalidate((uint32_t)locks, size);
	for (i = 0; i < locks->count; i++) {
		if (locks->locks[i]) {
			vnzLockFree(locks->locks[i]);
			locks->locks[i] = NULL;
		}
	}
	cacheFlushWriteback((uint32_t)locks, size);
	return 0;
}

/* RPC 327: a module type's auxiliary entry */
uint32_t ngsModuleAuxiliary(NgsGlobal *global, uint32_t moduleId)
{
	int32_t i;

	cachePurgeInvalidate((uint32_t)global, ngsRoundUp256(sizeof(NgsGlobal)));
	cachePurgeRange((uint32_t)global->moduleTable, NGS_MODULE_TABLE_HEADER);
	for (i = 0; i < NGS_MODULE_TYPES; i++)
		if (global->moduleTable->types[i].moduleId == moduleId)
			return global->moduleTable->types[i].auxiliary;
	return 0;
}

uint32_t ngsRoundUp256(uint32_t size)
{
	return (size + 255) & ~255;
}

/* work memory taken from a cursor, aligned to 256 */
uint32_t ngsCursorAllocate256(uint32_t *cursor, uint32_t size)
{
	uint32_t address = *cursor;

	address += (256 - (address & 0xff)) & 0xff;
	*cursor = address + ngsRoundUp256(size);
	return address;
}

uint32_t ngsRoundUp(uint32_t size, uint32_t alignment)
{
	return (size + alignment - 1) & -alignment;
}

uint32_t ngsCursorAllocate(uint32_t *cursor, uint32_t size, uint32_t alignment)
{
	uint32_t address = *cursor;

	if (alignment < 2) {
		*cursor = address + size;
		return address;
	}
	address += (alignment - address % alignment) % alignment;
	*cursor = address + ngsRoundUp(size, alignment);
	return address;
}

/*
 * A run of work buffers for count bytes, each buffer's lock taken: under the
 * table's lock, until a run of free buffers is found.
 */
static void workLocksRelease(NgsModuleTable *table, uint32_t start, uint32_t count)
{
	uint32_t i;

	for (i = start; i < start + count; i++)
		vnzSpinUnlock(table->workLocks[i]);
}

static uint32_t workBuffersAcquire(NgsModuleTable *table, uint32_t count)
{
	uint32_t start, run, i;

	vnzSpinLock(table->lock);
	for (;;) {
		start = NGS_WORK_BUFFERS;
		run = 0;
		for (i = 0; i < NGS_WORK_BUFFERS; i++) {
			if (vnzSpinTry(table->workLocks[i])) {
				/* taken: give back the run so far */
				workLocksRelease(table, start, run);
				run = 0;
				continue;
			}
			if (run == 0)
				start = i;
			if (++run == count) {
				vnzSpinUnlock(table->lock);
				return start;
			}
		}
		workLocksRelease(table, start, run);
	}
}

static uint32_t workBufferCount(uint32_t size)
{
	uint32_t count = size / NGS_WORK_BUFFER_SIZE;

	if (size % NGS_WORK_BUFFER_SIZE)
		count++;
	return count;
}

/* work memory for a task */
static void workAcquire(NgsSystem *system, uint32_t size, uint8_t **work, uint32_t *workSize,
	uint32_t *first, uint32_t *count)
{
	uint32_t buffers = workBufferCount(size), start;

	if (buffers == 0) {
		*work = NULL;
		*workSize = 0;
		*count = 0;
		*first = 0;
		return;
	}
	start = workBuffersAcquire(system->moduleTable, buffers);
	*first = start;
	*count = buffers;
	*work = system->moduleTable->workBuffers + start * NGS_WORK_BUFFER_SIZE;
	*workSize = buffers * NGS_WORK_BUFFER_SIZE;
}

/* work memory for a voice: its rack's scratch, then its channel buffers */
static void voiceWorkAcquire(NgsSystem *system, NgsRack *rack, uint8_t **scratch,
	uint32_t *scratchSize, uint32_t *first, uint32_t *count, NgsBuffer **buffers,
	uint32_t maxBuffers)
{
	uint32_t bufferSize = ngsBufferSize(rack->channels, NGS_SAMPLE_SIZE, NGS_MAX_GRANULARITY);
	uint32_t scratchLength = ngsRoundUp256(rack->scratchSize);
	uint32_t blocks = workBufferCount(rack->bufferCount * bufferSize + scratchLength);
	uint32_t start, cursor, i;

	if (blocks == 0) {
		*scratch = NULL;
		*scratchSize = 0;
		*count = 0;
		*first = 0;
		return;
	}
	start = workBuffersAcquire(system->moduleTable, blocks);
	*first = start;
	*count = blocks;
	*scratch = system->moduleTable->workBuffers + start * NGS_WORK_BUFFER_SIZE;
	*scratchSize = scratchLength;

	cursor = (uint32_t)*scratch + scratchLength;
	for (i = 0; i < rack->bufferCount; i++)
		buffers[i] = ngsBufferCreate(rack->channels, NGS_SAMPLE_SIZE, NGS_MAX_GRANULARITY,
			&cursor, 0);
}

static void workRelease(NgsSystem *system, uint32_t first, int32_t count)
{
	if (count < 1)
		return;
	cachePurgeRange((uint32_t)system->moduleTable->workBuffers +
		first * NGS_WORK_BUFFER_SIZE, count * NGS_WORK_BUFFER_SIZE);
	workLocksRelease(system->moduleTable, first, count);
}

/*
 * The buffers of a module's inputs or outputs, from the voice's routes: a
 * route names one of the rack's buffers, and may name a channel of it.
 */
static void routesRead(NgsBufferList *list, const uint8_t *routes, uint32_t *cursor,
	NgsRack *rack, NgsBuffer **buffers, int32_t *channel, int32_t outputs)
{
	uint32_t route, index;
	int32_t i, word;

	for (i = 0; i < list->count; i++) {
		route = routes[*cursor];
		*channel = -1;
		if (route & NGS_ROUTE_CHANNEL) {
			*channel = route & 0xf;
			index = (int32_t)(route - NGS_ROUTE_CHANNEL) >> 4;
		} else {
			index = route;
		}
		list->buffers[i] = index < rack->bufferCount ? buffers[index] : NULL;
		(*cursor)++;
		if (!outputs)
			continue;
		list->buffers[i]->flags = 0;
		for (word = 0; word < 4; word++)
			list->buffers[i]->header[word] = rack->bufferHeader[word];
	}
}

/* a voice's position in its rack's list of voices to report */
static void voiceReport(NgsVoice *voice)
{
	NgsRack *rack = voice->rack;
	uint32_t countAddress = (uint32_t)rack->finishedCount, slotAddress, count;

	vnzSpinLock(rack->lock);
	cachePurgeRange(lineStart(countAddress), lineLength(countAddress, 4));
	count = *rack->finishedCount;
	slotAddress = (uint32_t)&rack->finishedVoices[count];
	cachePurgeRange(lineStart(slotAddress), lineLength(slotAddress, 4));
	rack->finishedVoices[count] = voice->handle;
	cacheFlushRange(lineStart(slotAddress), lineLength(slotAddress, 4));
	*rack->finishedCount = count + 1;
	cacheFlushRange(lineStart(countAddress), lineLength(countAddress, 4));
	vnzSpinUnlock(rack->lock);
}

/* the voice's first free callback entry takes a module's callback */
static void callbackRecord(NgsVoiceStatus *status, uint32_t module, NgsModuleParams *params)
{
	uint32_t i;

	for (i = 0; i < 2; i++) {
		NgsCallback *callback = &status->callbacks[i];

		if (callback->module != NGS_NO_MODULE)
			continue;
		callback->module = module;
		callback->callbackData = params->callbackData;
		callback->callbackData2 = params->callbackData2;
		callback->callbackPtr = params->callbackPtr;
		return;
	}
}

/*
 * A voice job: the voice's modules run in order on its rack's buffers, the
 * reset ones initialized first; the voice's status says what happened, and
 * the voice is reported to the ARM when anything did.
 */
static void voiceJob(uint32_t contextArgument, uint32_t voiceArgument, uint32_t next,
	uint32_t systemArgument)
{
	VkVThreadContext *context = (VkVThreadContext *)contextArgument;
	NgsVoice *voice = (NgsVoice *)voiceArgument;
	NgsSystem *system = (NgsSystem *)systemArgument;
	NgsBuffer *buffers[NGS_VOICE_BUFFERS], *inputBuffers[8], *outputBuffers[8];
	uint8_t *dataList[8], *userParams;
	uint32_t extra[2];
	NgsBufferList inputs, outputs;
	NgsModuleParams params;
	uint32_t inputCursor = 0, outputCursor = 0, first, count, scratchSize;
	uint8_t *scratch;
	int32_t channel = -1;
	int16_t changes, reports;
	uint32_t userParamsPurged, ran, failed, module, bit, initialized;
	NgsVoiceMasks *masks;
	NgsVoiceStatus *status;
	NgsModuleSlot *slot;
	NgsRack *rack;
	VkVThreadContext *locked;
	uint32_t i, newState;

	cachePurgeRange((uint32_t)system, LINE);
	cachePurgeRange((uint32_t)voice, LINE);
	cachePurgeRange((uint32_t)voice, voice->size);
	cachePurgeRange((uint32_t)voice->status, voice->statusSize);
	cachePurgeRange((uint32_t)system->moduleTable, NGS_MODULE_TABLE_HEADER);
	cachePurgeRange((uint32_t)voice->settings, voice->settingsSize);
	cachePurgeRange((uint32_t)voice->states, voice->statesSize);
	rack = voice->rack;
	cachePurgeRange((uint32_t)rack, 128);
	cachePurgeRange((uint32_t)rack, rack->size);

	voiceWorkAcquire(system, rack, &scratch, &scratchSize, &first, &count, buffers,
		NGS_VOICE_BUFFERS);
	for (i = 0; i < rack->bufferCount; i++) {
		buffers[i]->channels = rack->channels;
		buffers[i]->samples = system->granularity;
		buffers[i]->channelSize = system->granularity * NGS_SAMPLE_SIZE;
		buffers[i]->channelMask = 0;
	}
	for (; i < NGS_VOICE_BUFFERS; i++)
		buffers[i] = NULL;

	params.sampleRate = system->sampleRate;
	params.voiceState = voice->state;
	params.scratchSize = scratchSize;
	params.scratch = scratch;
	params.flags = 0;
	params.userParamCount = 0;
	params.userParams = NULL;
	params.dataCount = 0;
	params.data = dataList;
	params.extra = extra;
	inputs.buffers = inputBuffers;
	outputs.buffers = outputBuffers;

	masks = (NgsVoiceMasks *)(voice->settings + rack->maskOffset);
	if (voice->state & SCE_NGS_VOICE_STATE_ACTIVE) {
		masks->enabled = 0;
		for (module = 0; module < rack->moduleCount; module++)
			masks->enabled |= 1 << module;
		masks->reset = voice->reset;
	} else {
		masks->reset |= voice->reset;
	}

	status = voice->status;
	for (i = 0; i < 2; i++)
		status->callbacks[i].module = NGS_NO_MODULE;
	status->error = 0;
	status->activeModules = 0;
	changes = 0;
	reports = 0;
	userParamsPurged = 0;
	ran = 0;
	failed = 0;

	for (module = 0; module < rack->moduleCount; module++) {
		bit = 1 << module;
		if (!(masks->enabled & bit)) {
			/* skipped: its inputs pass, its outputs are fresh */
			inputs.count = rack->inputCounts[module];
			inputCursor += inputs.count;
			outputs.count = rack->outputCounts[module];
			routesRead(&outputs, voice->outputRoutes, &outputCursor, rack, buffers,
				&channel, 1);
			continue;
		}

		params.flags |= NGS_RUN_ENABLED;
		if (voice->bypassed & bit)
			params.flags |= NGS_RUN_BYPASSED;
		else
			params.flags &= ~NGS_RUN_BYPASSED;

		inputs.count = rack->inputCounts[module];
		routesRead(&inputs, voice->inputRoutes, &inputCursor, rack, buffers, &channel, 0);
		outputs.count = rack->outputCounts[module];
		routesRead(&outputs, voice->outputRoutes, &outputCursor, rack, buffers, &channel, 1);

		slot = &rack->modules[module];
		params.dataCount = 0;
		if (slot->hasSettings)
			dataList[params.dataCount++] = voice->settings + slot->settingsOffset;
		if (slot->hasStates)
			dataList[params.dataCount++] = voice->states +
				slot->statesOffset;

		initialized = 0;
		if (masks->reset & bit) {
			userParams = voice->userParams + slot->userParamsOffset;
			if (!userParamsPurged) {
				cachePurgeRange((uint32_t)voice->userParams, voice->userParamsSize);
				userParamsPurged = 1;
			}
			params.userParamCount = 1;
			params.userParams = &userParams;
			initialized = rack->initialize[module](&inputs, &outputs, &params);
			if (initialized) {
				changes++;
				reports++;
				status->error = initialized;
				status->errorModule = module;
				failed = 1;
				break;
			}
			masks->reset &= ~bit;
		}

		params.userParamCount = 0;
		params.callbackRequested = 0;
		if (!(params.flags & NGS_RUN_BYPASSED)) {
			for (i = 0; i < outputs.count; i++)
				outputs.buffers[i]->flags = 0;
			for (i = 0; i < outputs.count; i++) {
				NgsBuffer *buffer = outputs.buffers[i];
				int32_t c;

				for (c = 0; c < buffer->channels; c++)
					cacheInvalidateFlush((uint32_t)buffer->channelData[c],
						buffer->channelSize);
			}
			if (!initialized)
				rack->process[module](&inputs, &outputs, &params);
			if (channel >= 0)
				ngsRouteChannel(outputs.buffers[0], voice, channel);
		} else {
			uint8_t flags = rack->moduleFlags[module];

			if (!(flags & (NGS_MODULE_PASS_THROUGH | NGS_MODULE_NO_COPY)) &&
				inputs.count >= 1 && outputs.count >= 1 && channel >= 0)
				ngsRouteChannel(inputs.buffers[0], voice, channel);
			if (flags & NGS_MODULE_PASS_THROUGH) {
				if (!initialized)
					rack->process[module](&inputs, &outputs, &params);
				ngsRouteChannel(outputs.buffers[0], voice, channel);
			}
		}

		if (params.flags & NGS_RUN_ENABLED)
			status->activeModules |= bit;
		if ((params.flags & NGS_RUN_ENABLED) && !(params.flags & NGS_RUN_BYPASSED))
			ran++;
		if (params.callbackRequested) {
			callbackRecord(status, module, &params);
			changes++;
			reports++;
		}
	}

	status->state = voice->state;
	if (failed || ran == 0 || (params.flags & NGS_RUN_STOP))
		newState = SCE_NGS_VOICE_STATE_PAUSED;
	else if (params.voiceState & (SCE_NGS_VOICE_STATE_PENDING | NGS_VOICE_STATE_KEYED_OFF))
		newState = (params.flags & NGS_RUN_KEEP_PENDING) ? SCE_NGS_VOICE_STATE_PENDING :
			SCE_NGS_VOICE_STATE_PAUSED;
	else
		newState = status->state;
	status->state = newState;

	if (status->state != voice->state) {
		reports++;
		changes++;
	} else if (status->activeModules != masks->enabled) {
		changes++;
	}
	masks->enabled = status->activeModules;
	workRelease(system, first, count);

	if (changes == 0)
		status->state = NGS_STATUS_UNCHANGED;
	else if (reports == 0)
		status->state = NGS_STATUS_CALLBACK_ONLY;
	if (changes >= 1)
		voiceReport(voice);

	cacheFlushRange((uint32_t)voice->settings, voice->settingsSize);
	cacheFlushRange((uint32_t)voice->states, voice->statesSize);
	cacheFlushRange((uint32_t)voice->status, voice->statusSize);

	locked = vkVThreadContextLock(context);
	vkVThreadJobSignal(locked, next);
	vkVThreadContextUnlock(locked);
}

/* a barrier: the jobs after it are signalled */
static void barrierJob(uint32_t contextArgument, uint32_t barrierArgument, uint32_t thread,
	uint32_t size)
{
	NgsBarrier *barrier = (NgsBarrier *)barrierArgument;
	VkVThreadContext *context;
	int32_t i;

	cachePurgeInvalidate((uint32_t)barrier, size);
	context = vkVThreadContextLock((VkVThreadContext *)contextArgument);
	for (i = 0; i < barrier->signalCount; i++)
		vkVThreadJobSignal(context, barrier->signals[i]);
	vkVThreadContextUnlock(context);
}

/* a task: its function run on its data, with work memory when it asks */
static void taskJob(uint32_t contextArgument, uint32_t taskArgument, uint32_t signal,
	uint32_t systemArgument)
{
	NgsTask *task = (NgsTask *)taskArgument;
	NgsSystem *system = (NgsSystem *)systemArgument;
	uint8_t *work = NULL;
	uint32_t workSize = 0, first, count, start, length;
	VkVThreadContext *context;

	cachePurgeRange(lineStart(taskArgument), lineLength(taskArgument, sizeof(NgsTask)));
	cachePurgeRange((uint32_t)system, LINE);
	cachePurgeRange((uint32_t)system->moduleTable, NGS_MODULE_TABLE_HEADER);
	start = lineStart(task->address);
	length = lineLength(task->address, task->size);
	cachePurgeRange(start, length);

	if (task->workSize)
		workAcquire(system, task->workSize, &work, &workSize, &first, &count);
	task->function(task->address, task->size, work, workSize);
	if (work)
		workRelease(system, first, count);
	cacheFlushRange(start, length);

	context = vkVThreadContextLock((VkVThreadContext *)contextArgument);
	vkVThreadJobSignal(context, signal);
	vkVThreadContextUnlock(context);
}
