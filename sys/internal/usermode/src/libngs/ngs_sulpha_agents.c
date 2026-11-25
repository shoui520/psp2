/*
 * Sulpha profiling agents for SceNgsUser:
 * API Trace, Module Trace, Message Log, Named Objects, PCM Audio Capture, Synth Trace.
 */
#include "ngs_internal.h"

static inline void ngsPoolInit(NgsSulphaPool *pool, void *storage, SceInt32 capacity, SceInt32 elemSize)
{
	pool->storage = storage;
	pool->capacity = capacity;
	pool->elemSize = elemSize;
	pool->allocIndex = 0;
	if (storage != NULL) {
		void **stack = (void **)((SceUInt8 *)storage + (SceUInt32)(elemSize * capacity));
		pool->freeStack = stack;
		for (SceInt32 i = 0; i < capacity; i++)
			stack[i] = (SceUInt8 *)storage + (SceUInt32)(elemSize * i);
	}
}

static inline void *ngsPoolAlloc(NgsSulphaPool *pool)
{
	if (pool->allocIndex >= pool->capacity)
		return NULL;
	return pool->freeStack[pool->allocIndex++];
}

static inline void ngsPoolFree(NgsSulphaPool *pool, void *ptr)
{
	SceUIntPtr base = (SceUIntPtr)pool->storage;
	SceUIntPtr addr = (SceUIntPtr)ptr;
	if (addr >= base && addr < base + (SceUInt32)(pool->elemSize * pool->capacity)) {
		pool->allocIndex--;
		pool->freeStack[pool->allocIndex] = ptr;
	}
}

static inline const char *ngsInternString(NgsSulphaModuleTraceAgent *state, const char *str)
{
	SceSize len = ngsStrnlen(str, 256);
	SceSize lenWithNull = len + 1;
	SceUInt32 used = state->stringUsed;

	if (used > lenWithNull) {
		SceUInt32 limit = used - lenWithNull;
		for (SceUInt32 pos = 0; pos < limit; pos++) {
			if (ngsMemcmp(&state->stringPool[pos], str, lenWithNull) == 0)
				return &state->stringPool[pos];
		}
	}

	if (len + used < sizeof(state->stringPool)) {
		char *dst = &state->stringPool[used];
		ngsSafeStrncpy(dst, sizeof(state->stringPool) - used, str, len);
		dst[len] = '\0';
		state->stringUsed += len + 1;
		return dst;
	}

	return NULL;
}

SceInt32 ngsApiTraceAgentInit(void *agent, const void *cfgPtr, void *userData, SceUInt32 userDataSize)
{
	const NgsSulphaApiTraceConfig *cfg = (const NgsSulphaApiTraceConfig *)cfgPtr;
	NgsSulphaAgentDef *def = (NgsSulphaAgentDef *)userData;
	(void)agent;

	if (userDataSize == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (cfg == NULL || def == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	def->name = "NGS API Trace";
	def->versionMajor = 1;
	def->versionMinor = 0;
	def->versionBuild = 0;
	def->desc = "Captures the NGS API calls and return codes.";
	def->uuid[0] = 0xA197078AU;
	def->uuid[1] = 0x491D90D2U;
	def->uuid[2] = 0x7F8CEFBFU;
	def->uuid[3] = 0x45AF45F0U;
	def->typeId = NGS_SULPHA_AGENT_API_TRACE;
	def->eventDefCount = (SceUInt8)cfg->eventCount;
	def->subEventDefCount = 0;
	def->enumDefCount = 1;
	def->reserved = 0;
	def->eventDefs = NULL;
	def->subEventDefs = NULL;
	def->enumDefs = &g_ngsReturnCodeEnumDef;

	if (cfg->eventCount == 0)
		return SCE_OK;

	NgsSulphaEventDef *events = (NgsSulphaEventDef *)(def + 1);
	NgsSulphaFieldDef *fields = (NgsSulphaFieldDef *)(events + cfg->eventCount);
	def->eventDefs = events;

	for (SceUInt32 i = 0; i < cfg->eventCount; i++) {
		const NgsSulphaApiTraceEventDesc *src = &cfg->events[i];
		if ((SceUInt8)src->eventId == 0)
			return SCE_SULPHA_ERROR_INVALID_VALUE;

		events[i].id = (SceUInt8)src->eventId;
		events[i].fieldCount = (SceUInt8)src->paramCount;
		events[i].flagsAndBlocks = (events[i].flagsAndBlocks & ~0x3U) | 2U;
		events[i].name = src->name;
		events[i].desc = NULL;
		events[i].fields = fields;

		for (SceUInt32 p = 0; p < (SceUInt8)src->paramCount; p++) {
			const NgsSulphaApiTraceParamDesc *pdesc = &src->params[p];
			SceUInt8 ftype = (SceUInt8)pdesc->fieldType;
			if (ftype == 0x8EU || ftype == 0xFEU)
				ftype = 0x03U;

			fields->id = (SceUInt8)(p + 1);
			fields->type = ftype;
			fields->count = 1;
			fields->name = pdesc->name;
			fields->desc = NULL;
			fields->unit = pdesc->unit;
			fields++;
		}
	}

	return SCE_OK;
}

SceInt32 ngsModuleTraceAgentInit(void *agent, const void *cfgPtr, void *userData, SceUInt32 userDataSize)
{
	const NgsSulphaModuleTraceConfig *cfg = (const NgsSulphaModuleTraceConfig *)cfgPtr;
	NgsSulphaModuleTraceAgent *state = (NgsSulphaModuleTraceAgent *)userData;
	(void)agent;

	if (userDataSize == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (cfg == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	state->typeId = NGS_SULPHA_AGENT_MODULE_TRACE;
	state->started = 0;
	state->rwLock = 0;
	state->moduleCount = (SceUInt8)cfg->moduleCount;
	state->stringUsed = 0;
	state->stringPool[0] = '\0';

	NgsSulphaEventDef *events = (NgsSulphaEventDef *)(state + 1);
	NgsSulphaExtFieldDef *extFields = (NgsSulphaExtFieldDef *)(events + cfg->moduleCount);
	SceUInt32 maxParamBytes = 0;
	SceUInt32 recordStride = 24;

	if (cfg->moduleCount != 0) {
		state->agentDef.name = "NGS Module Trace";
		state->agentDef.versionMajor = 1;
		state->agentDef.versionMinor = 0;
		state->agentDef.versionBuild = 0;
		state->agentDef.desc = "Captures the parameter changes made to the NGS voice modules.";
		state->agentDef.uuid[0] = 0xB53C0C47U;
		state->agentDef.uuid[1] = 0x4F564F1CU;
		state->agentDef.uuid[2] = 0xE099869FU;
		state->agentDef.uuid[3] = 0xC26A8489U;
		state->agentDef.typeId = NGS_SULPHA_AGENT_MODULE_TRACE;
		state->agentDef.eventDefCount = 0;
		state->agentDef.subEventDefCount = 0;
		state->agentDef.enumDefCount = 0;
		state->agentDef.reserved = 0;
		state->agentDef.eventDefs = events;
		state->agentDef.subEventDefs = NULL;
		state->agentDef.enumDefs = NULL;

		const char *strBypass = ngsInternString(state, "BypassFlag");
		const char *strIndex = ngsInternString(state, "Index");
		const char *strCycles = ngsInternString(state, "Cycles");

		for (SceUInt32 m = 0; m < cfg->moduleCount; m++) {
			union {
				NgsSulphaModuleQuery query;
				NgsSulphaModuleQueryHeader hdr;
			} q;
			q.hdr.moduleId = cfg->moduleIds[m];
			q.hdr.sectionIndex = 0;

			if (ngsSulphaQueryModule(0, &q) >= 0) {
				char modName[128];
				modName[0] = '\0';
				const char *p = q.hdr.descText;
				while (*p != '\0') {
					if (p[0] == 'n' && ngsMemcmp(p, "name = ", 7) == 0) {
						const char *src = p + 7;
						SceUInt32 k = 0;
						while (src[k] != '\0' && k < 126) {
							modName[k] = src[k];
							k++;
						}
						modName[k] = '\0';
						char *nl = modName;
						while (*nl == '\n')
							nl++;
						while (*nl != '\0' && *nl != '\n')
							nl++;
						if (*nl == '\n')
							*nl = '\0';
						break;
					}
					p++;
				}

				const char *internModName = ngsInternString(state, modName);
				SceUInt32 sectionCount = q.query.sectionCount;
				SceUInt32 paramSize = q.query.paramSize;
				state->moduleIds[m] = cfg->moduleIds[m];

				SceUInt8 totalFields = 3;
				for (SceUInt32 s = 0; s < sectionCount; s++) {
					q.hdr.sectionIndex = s;
					if (ngsSulphaQueryModule(1, &q) >= 0)
						totalFields += (SceUInt8)q.hdr.paramCount;
				}

				events[m].id = (SceUInt8)(m + 1);
				events[m].fieldCount = totalFields;
				events[m].flagsAndBlocks = (events[m].flagsAndBlocks & ~0xFU) | 0xEU;
				events[m].name = internModName;
				events[m].desc = NULL;
				events[m].fields = extFields;

				/* Standard 3 header fields: Index, BypassFlag, Cycles */
				ngsMemset(extFields, 0, 3 * sizeof(NgsSulphaExtFieldDef));
				extFields[0].id = 1;
				extFields[0].type = 1;
				extFields[0].count = 1;
				extFields[0].name = strIndex;

				extFields[1].id = 2;
				extFields[1].type = 1;
				extFields[1].count = 1;
				extFields[1].name = strBypass;
				extFields[1].maxVal = 1;

				extFields[2].id = 3;
				extFields[2].type = 3;
				extFields[2].count = 1;
				extFields[2].name = strCycles;
				extFields += 3;

				SceUInt8 nextFieldId = 4;
				for (SceUInt32 s = 0; s < sectionCount; s++) {
					q.hdr.sectionIndex = s;
					if (ngsSulphaQueryModule(1, &q) >= 0) {
						SceUInt32 pCount = q.hdr.paramCount;
						for (SceUInt32 pi = 0; pi < pCount; pi++) {
							q.hdr.paramIndex = pi;
							if (ngsSulphaQueryModule(2, &q) >= 0) {
								const char *pName = ngsInternString(state, q.query.paramName);
								const char *pDesc = ngsInternString(state, q.query.paramDesc);
								SceUInt32 pType = q.query.paramType;
								SceUInt8 mapped = (pType - 1U <= 8U) ? g_ngsModuleParamTypeMap[pType - 1U] : 0;

								extFields->id = (SceUInt8)(nextFieldId + pi);
								extFields->type = mapped;
								extFields->count = 1;
								extFields->name = pName;
								extFields->desc = pDesc;
								extFields->unit = NULL;

								switch (pType) {
								case 1:
								case 2:
								case 5:
									extFields->minVal = q.query.minVal;
									extFields->maxVal = q.query.maxVal;
									extFields->defaultVal = q.query.defaultVal;
									break;
								case 3:
								case 6:
								case 7:
									extFields->minVal = (SceUInt8)q.query.minVal;
									extFields->maxVal = (SceUInt8)q.query.maxVal;
									extFields->defaultVal = (SceUInt8)q.query.defaultVal;
									break;
								case 8:
								case 9:
									extFields->minVal = (SceUInt16)q.query.minVal;
									extFields->maxVal = (SceUInt16)q.query.maxVal;
									extFields->defaultVal = (SceUInt16)q.query.defaultVal;
									break;
								default:
									break;
								}
							}
							extFields++;
						}
						nextFieldId += (SceUInt8)pCount;
					}
				}

				if (paramSize > maxParamBytes)
					maxParamBytes = paramSize;
			}
			state->agentDef.eventDefCount++;
		}
		recordStride = (maxParamBytes + 27U) & ~3U;
	}

	SceUInt8 *poolStorage = (SceUInt8 *)(extFields + 1);
	ngsListInit(&state->recordList);
	state->maxParamBytes = maxParamBytes;
	ngsMemset(poolStorage, 0, (recordStride + 4U) * cfg->maxInstances);
	ngsPoolInit(&state->recordPool, poolStorage, (SceInt32)cfg->maxInstances, (SceInt32)recordStride);
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentInit(void *agent, const void *cfgPtr, void *userData, SceUInt32 userDataSize)
{
	const NgsSulphaNamedObjectsConfig *cfg = (const NgsSulphaNamedObjectsConfig *)cfgPtr;
	NgsSulphaNamedObjectsAgent *state = (NgsSulphaNamedObjectsAgent *)userData;

	if (userDataSize == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (cfg == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceUInt32 maxObj = cfg->maxNamedObjects;
	if (userDataSize < 48U * maxObj + sizeof(NgsSulphaNamedObjectsAgent))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	state->typeId = NGS_SULPHA_AGENT_NAMED_OBJECTS;
	state->started = 0;
	state->capacity = maxObj;
	state->count = 0;
	state->entries = (NgsSulphaNamedEntry *)(state + 1);

	for (SceUInt32 i = 0; i < maxObj; i++) {
		state->entries[i].sulphaNode = NULL;
		state->entries[i].location = NULL;
		state->entries[i].name[0] = '\0';
	}

	sceSulphaNodeNew(&state->rootNode, agent, NULL);
	return SCE_OK;
}

SceInt32 ngsPcmCaptureAgentInit(void *agent, const void *cfgPtr, void *userData, SceUInt32 userDataSize)
{
	const NgsSulphaPcmCaptureConfig *cfg = (const NgsSulphaPcmCaptureConfig *)cfgPtr;
	NgsSulphaPcmCaptureAgent *state = (NgsSulphaPcmCaptureAgent *)userData;
	(void)agent;

	if (cfg == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceUInt32 maxSinks = cfg->maxSinks;
	SceUInt32 maxSources = cfg->maxSources;
	if (36U * maxSinks + 28U * maxSources + sizeof(NgsSulphaPcmCaptureAgent) > userDataSize)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	state->typeId = NGS_SULPHA_AGENT_PCM_CAPTURE;
	state->started = 0;
	state->rwLock = 0;
	ngsListInit(&state->sinkList);
	ngsListInit(&state->sourceList);

	SceUInt8 *sinkStorage = (SceUInt8 *)(state + 1);
	ngsMemset(sinkStorage, 0, 36U * maxSinks);
	ngsPoolInit(&state->sinkPool, sinkStorage, (SceInt32)maxSinks, sizeof(NgsSulphaPcmSinkRecord));

	SceUInt8 *sourceStorage = sinkStorage + 36U * maxSinks;
	ngsMemset(sourceStorage, 0, 28U * maxSources);
	ngsPoolInit(&state->sourcePool, sourceStorage, (SceInt32)maxSources, sizeof(NgsSulphaPcmSourceRecord));

	return SCE_OK;
}

SceInt32 ngsSynthTraceAgentInit(void *agent, const void *cfgPtr, void *userData, SceUInt32 userDataSize)
{
	const NgsSulphaSynthTraceConfig *cfg = (const NgsSulphaSynthTraceConfig *)cfgPtr;
	NgsSulphaSynthTraceAgent *state = (NgsSulphaSynthTraceAgent *)userData;
	(void)agent;

	if (userDataSize == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (cfg == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceUInt32 maxObj = cfg->maxObjects;
	SceUInt32 poolBytes = 84U * maxObj;
	if (userDataSize < poolBytes + sizeof(NgsSulphaSynthTraceAgent))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	state->typeId = NGS_SULPHA_AGENT_SYNTH_TRACE;
	state->started = 0;
	state->rwLock = 0;
	ngsListInit(&state->recordList);

	SceUInt8 *storage = (SceUInt8 *)(state + 1);
	ngsMemset(storage, 0, poolBytes);
	ngsPoolInit(&state->recordPool, storage, (SceInt32)maxObj, sizeof(NgsSulphaSynthRecord));
	return SCE_OK;
}

SceInt32 ngsApiTraceAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaApiTraceConfig *cfg = (const NgsSulphaApiTraceConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_API_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaApiTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	if (cfg->eventCount == 0) {
		outReq->userDataSize = 104;
		outReq->defCopySize = 52;
		outReq->eventCount = cfg->maxEvents;
		return SCE_OK;
	}
	if (cfg->events == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceUInt32 maxPayload = 0;
	SceUInt32 defSize = 52U + 16U * cfg->eventCount;

	for (SceUInt32 i = 0; i < cfg->eventCount; i++) {
		const NgsSulphaApiTraceEventDesc *ev = &cfg->events[i];
		SceUInt32 payload = 0;
		for (SceUInt32 p = 0; p < (SceUInt8)ev->paramCount; p++) {
			SceUInt32 t = ev->params[p].fieldType;
			if (t == 4U || t == 7U || t == 8U || t == 0x84U) {
				payload += 8;
			} else if (t == 1U || t == 2U || t == 3U ||
				   t == 0x81U || t == 0x82U || t == 0x83U ||
				   t == 0x8DU || t == 0x8EU || t == 0xFEU) {
				payload += 4;
			} else {
				return SCE_SULPHA_ERROR_INVALID_VALUE;
			}
		}
		if (payload > maxPayload)
			maxPayload = payload;
		defSize += 16U * (SceUInt8)ev->paramCount;
	}

	outReq->userDataSize = defSize + 52U;
	outReq->defCopySize = defSize;
	outReq->heapSize = cfg->maxEvents * ((maxPayload + 127U) & ~127U);
	outReq->eventCount = cfg->maxEvents;
	return SCE_OK;
}

SceInt32 ngsMessageLogAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaMessageLogConfig *cfg = (const NgsSulphaMessageLogConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_MESSAGE_LOG ||
	    cfg->header.configSize != sizeof(NgsSulphaMessageLogConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	outReq->userDataSize = 0;
	outReq->defCopySize = 16U * g_ngsMessageLogEventDefs[0].fieldCount + 68U;
	outReq->heapSize = cfg->maxTraceBufferBytes;
	outReq->eventCount = cfg->maxTraceBufferBytes >> 4;
	return SCE_OK;
}

SceInt32 ngsModuleTraceAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaModuleTraceConfig *cfg = (const NgsSulphaModuleTraceConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_MODULE_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaModuleTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	if (cfg->moduleCount == 0) {
		outReq->userDataSize = 8476;
		outReq->defCopySize = 52;
		outReq->heapSize = 0;
		outReq->eventCount = cfg->maxInstances;
		outReq->nodeCount = cfg->maxInstances;
		return SCE_OK;
	}
	if (cfg->moduleIds == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->moduleCount > 31U)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceUInt32 defSize = 52;
	SceUInt32 maxParamBytes = 0;

	for (SceUInt32 m = 0; m < cfg->moduleCount; m++) {
		union {
			NgsSulphaModuleQuery query;
			NgsSulphaModuleQueryHeader hdr;
		} q;
		defSize += 112;
		q.hdr.moduleId = cfg->moduleIds[m];
		q.hdr.sectionIndex = 0;
		if (ngsSulphaQueryModule(0, &q) >= 0) {
			SceUInt32 sectionCount = q.query.sectionCount;
			SceUInt32 paramSize = q.query.paramSize;
			for (SceUInt32 s = 0; s < sectionCount; s++) {
				q.hdr.sectionIndex = s;
				if (ngsSulphaQueryModule(1, &q) >= 0)
					defSize += paramSize + 32U * q.hdr.paramCount;
			}
			if (paramSize > maxParamBytes)
				maxParamBytes = paramSize;
		}
	}

	SceUInt32 maxInst = cfg->maxInstances;
	outReq->userDataSize = defSize + 4U * maxInst + sizeof(NgsSulphaModuleTraceAgent) +
			       ((maxParamBytes + 27U) & ~3U) * maxInst;
	outReq->defCopySize = defSize;
	outReq->heapSize = maxInst * ((maxParamBytes + 63U) & ~63U);
	outReq->eventCount = maxInst;
	outReq->nodeCount = maxInst;
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaNamedObjectsConfig *cfg = (const NgsSulphaNamedObjectsConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_NAMED_OBJECTS ||
	    cfg->header.configSize != sizeof(NgsSulphaNamedObjectsConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceUInt32 count = cfg->maxNamedObjects;
	outReq->userDataSize = 48U * count + sizeof(NgsSulphaNamedObjectsAgent);
	outReq->defCopySize = 16U * g_ngsNamedObjectsEventDefs[0].fieldCount + 68U;
	outReq->heapSize = 48U * count;
	outReq->eventCount = count;
	outReq->nodeCount = count;
	return SCE_OK;
}

SceInt32 ngsPcmCaptureAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaPcmCaptureConfig *cfg = (const NgsSulphaPcmCaptureConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_PCM_CAPTURE ||
	    cfg->header.configSize != sizeof(NgsSulphaPcmCaptureConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceUInt32 sinks = cfg->maxSinks;
	SceUInt32 sources = cfg->maxSources;
	SceUInt32 total = sinks + sources;
	SceUInt32 frames = cfg->framesPerObject;
	SceUInt32 totalFields = g_ngsPcmCaptureEventDefs[0].fieldCount +
				g_ngsPcmCaptureEventDefs[1].fieldCount +
				g_ngsPcmCaptureEventDefs[2].fieldCount;

	outReq->userDataSize = 24U * sources + 32U * sinks + sizeof(NgsSulphaPcmCaptureAgent) + 4U * total;
	outReq->defCopySize = 16U * totalFields + 100U;
	outReq->heapSize = frames * (24U * sources + 2080U * sinks);
	outReq->eventCount = frames * total;
	outReq->nodeCount = total;
	return SCE_OK;
}

SceInt32 ngsSynthTraceAgentGetMemReq(const void *cfgPtr, NgsSulphaAgentMemReq *outReq)
{
	const NgsSulphaSynthTraceConfig *cfg = (const NgsSulphaSynthTraceConfig *)cfgPtr;

	if (outReq == NULL || cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_SYNTH_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaSynthTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceUInt32 count = cfg->maxObjects;
	SceUInt32 totalFields = g_ngsSynthTraceEventDefs[0].fieldCount +
				g_ngsSynthTraceEventDefs[1].fieldCount +
				g_ngsSynthTraceEventDefs[2].fieldCount +
				g_ngsSynthTraceEventDefs[3].fieldCount;

	outReq->userDataSize = 84U * count + sizeof(NgsSulphaSynthTraceAgent);
	outReq->defCopySize = 16U * totalFields + 116U;
	outReq->heapSize = 80U * count;
	outReq->eventCount = count;
	outReq->nodeCount = count;
	return SCE_OK;
}

SceInt32 ngsApiTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	if (outDef == NULL || userData == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = (const NgsSulphaAgentDef *)userData;
	return SCE_OK;
}

SceInt32 ngsMessageLogAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	(void)userData;
	if (outDef == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = &g_ngsMessageLogAgentDef;
	return SCE_OK;
}

SceInt32 ngsModuleTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	NgsSulphaModuleTraceAgent *state = (NgsSulphaModuleTraceAgent *)userData;
	if (outDef == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = &state->agentDef;
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	if (userData == NULL || outDef == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = &g_ngsNamedObjectsAgentDef;
	return SCE_OK;
}

SceInt32 ngsPcmCaptureAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	if (userData == NULL || outDef == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = &g_ngsPcmCaptureAgentDef;
	return SCE_OK;
}

SceInt32 ngsSynthTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef)
{
	if (userData == NULL || outDef == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	*outDef = &g_ngsSynthTraceAgentDef;
	return SCE_OK;
}

SceInt32 ngsApiTraceGetDefaultConfig(NgsSulphaApiTraceConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_API_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaApiTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultApiTraceConfig, sizeof(*cfg));
	return SCE_OK;
}

SceInt32 ngsApiTraceReportCall(SceUInt64 agentHandle, SceUInt32 eventId, SceUInt32 paramCount, va_list ap)
{
	void *agent = NULL;
	NgsSulphaAgentDef *def = NULL;

	if (eventId == 0 || paramCount == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&def);
	if (def == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (ret < 0)
		return ret;

	const NgsSulphaEventDef *evDef = NULL;
	for (SceUInt32 i = 0; i < def->eventDefCount; i++) {
		if (def->eventDefs[i].id == eventId) {
			evDef = &def->eventDefs[i];
			break;
		}
	}
	if (evDef == NULL || evDef->fields == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (evDef->fieldCount != paramCount)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	const NgsSulphaFieldDef *fields = (const NgsSulphaFieldDef *)evDef->fields;
	SceInt32 firstArg = va_arg(ap, SceInt32);
	void *event = NULL;
	sceSulphaEventNew(&event, agent, (SceUInt8)eventId, 0);
	if (event == NULL)
		return ret;

	sceSulphaEventAdd(event, fields[0].id, &firstArg, 4);

	for (SceUInt32 i = 1; i < evDef->fieldCount && ret >= 0; i++) {
		SceUInt8 ftype = fields[i].type;
		switch (ftype) {
		case 1:
		case 0x81: {
			SceUInt8 val = (SceUInt8)va_arg(ap, int);
			sceSulphaEventAdd(event, fields[i].id, &val, 1);
			break;
		}
		case 2:
		case 0x82: {
			SceUInt16 val = (SceUInt16)va_arg(ap, int);
			sceSulphaEventAdd(event, fields[i].id, &val, 2);
			break;
		}
		case 3:
		case 0x83: {
			SceUInt32 val = va_arg(ap, SceUInt32);
			sceSulphaEventAdd(event, fields[i].id, &val, 4);
			break;
		}
		case 4:
		case 8:
		case 0x84: {
			double val = va_arg(ap, double);
			sceSulphaEventAdd(event, fields[i].id, &val, 8);
			break;
		}
		case 7: {
			float val = (float)va_arg(ap, double);
			sceSulphaEventAdd(event, fields[i].id, &val, 4);
			break;
		}
		case 0x8D: {
			SceUInt32 handle = va_arg(ap, SceUInt32);
			NgsSulphaTrackedObject *obj = NULL;
			ngsSulphaLookupObject(handle, &obj);
			if (obj != NULL)
				sceSulphaEventAdd(event, fields[i].id, &obj->sulphaNode, 4);
			else
				ret = SCE_SULPHA_ERROR_INVALID_VALUE;
			break;
		}
		default:
			ret = SCE_SULPHA_ERROR_INVALID_VALUE;
			break;
		}
	}

	if (ret >= 0)
		return sceSulphaEventReport(event, NULL);

	sceSulphaEventDelete(event);
	return ret;
}

SceInt32 ngsMessageLogEmit(void *agent, SceUInt8 eventId, const char *component, const char *fmt, va_list ap)
{
	void *event = NULL;
	char msgBuf[256];

	sceSulphaEventNew(&event, agent, eventId, 0);
	if (event == NULL)
		return SCE_OK;

	if (component != NULL) {
		SceSize compLen = ngsStrnlen(component, 128);
		sceSulphaEventAdd(event, 3, component, compLen);
	}
	ngsFormatString(msgBuf, sizeof(msgBuf), fmt, ap);
	SceSize msgLen = ngsStrnlen(msgBuf, sizeof(msgBuf));
	sceSulphaEventAdd(event, 4, msgBuf, msgLen);
	return sceSulphaEventReport(event, NULL);
}

SceInt32 ngsMessageLogGetDefaultConfig(NgsSulphaMessageLogConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_MESSAGE_LOG ||
	    cfg->header.configSize != sizeof(NgsSulphaMessageLogConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultMessageLogConfig, sizeof(*cfg));
	return SCE_OK;
}

SceInt32 ngsMessageLogReport(SceUInt64 agentHandle, SceUInt32 eventId, const char *component, const char *fmt, ...)
{
	if (fmt == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (eventId != 4U)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	void *agent = NULL;
	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, NULL);
	if (ret >= 0) {
		va_list ap;
		va_start(ap, fmt);
		ret = ngsMessageLogEmit(agent, (SceUInt8)eventId, component, fmt, ap);
		va_end(ap);
	}
	return ret;
}

SceInt32 ngsModuleTraceAgentOnStart(void *agent, void *userData)
{
	NgsSulphaModuleTraceAgent *state = (NgsSulphaModuleTraceAgent *)userData;
	if (agent == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	state->started = 0;
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentOnStart(void *agent, void *userData)
{
	NgsSulphaNamedObjectsAgent *state = (NgsSulphaNamedObjectsAgent *)userData;
	if (agent == NULL || state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	state->started = 0;
	return SCE_OK;
}

SceInt32 ngsPcmCaptureAgentOnStart(void *agent, void *userData)
{
	NgsSulphaPcmCaptureAgent *state = (NgsSulphaPcmCaptureAgent *)userData;
	if (agent == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	state->started = 0;
	return SCE_OK;
}

SceInt32 ngsSynthTraceAgentOnStart(void *agent, void *userData)
{
	NgsSulphaSynthTraceAgent *state = (NgsSulphaSynthTraceAgent *)userData;
	if (agent == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (state == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	state->started = 0;
	return SCE_OK;
}

SceInt32 ngsModuleTraceEmitRecord(void *event, const NgsSulphaEventDef *eventDef, const NgsSulphaModuleRecord *rec)
{
	if (rec->eventId == 0)
		return SCE_SULPHA_ERROR_INVALID_EVENT_ID;

	const NgsSulphaExtFieldDef *fields = (const NgsSulphaExtFieldDef *)eventDef->fields;
	sceSulphaEventAdd(event, 1, &rec->moduleIndex, 1);
	sceSulphaEventAdd(event, 2, &rec->bypassFlag, 1);
	sceSulphaEventAdd(event, 3, &rec->cycles, 4);

	const SceUInt8 *cursor = (const SceUInt8 *)(rec + 1);
	SceInt32 err = SCE_OK;

	for (SceUInt32 i = 3; i < eventDef->fieldCount; i++) {
		SceUInt8 ftype = fields[i].type;
		switch (ftype) {
		case 1:
		case 0x81: {
			SceUInt8 val = *cursor++;
			sceSulphaEventAdd(event, fields[i].id, &val, 1);
			break;
		}
		case 2:
		case 0x82: {
			SceUInt16 val;
			ngsMemcpy(&val, cursor, 2);
			cursor += 2;
			sceSulphaEventAdd(event, fields[i].id, &val, 2);
			break;
		}
		case 3:
		case 7:
		case 0x83: {
			SceUInt32 val;
			ngsMemcpy(&val, cursor, 4);
			cursor += 4;
			sceSulphaEventAdd(event, fields[i].id, &val, 4);
			break;
		}
		case 4:
		case 8:
		case 0x84: {
			double val;
			ngsMemcpy(&val, cursor, 8);
			cursor += 8;
			sceSulphaEventAdd(event, fields[i].id, &val, 8);
			break;
		}
		default:
			err = SCE_SULPHA_ERROR_INVALID_VALUE;
			break;
		}
	}

	if (err != SCE_OK) {
		sceSulphaEventDelete(event);
		return err;
	}
	return sceSulphaEventReport(event, rec->sulphaNode);
}

SceInt32 ngsModuleTraceAgentOnStarted(void *agent, void *userData)
{
	NgsSulphaModuleTraceAgent *state = (NgsSulphaModuleTraceAgent *)userData;

	ngsRwLockReadLock(&state->rwLock);
	for (NgsSulphaListNode *it = state->recordList.next; it != &state->recordList; it = it->next) {
		NgsSulphaModuleRecord *rec = (NgsSulphaModuleRecord *)it;
		void *event = NULL;
		sceSulphaEventNew(&event, agent, rec->eventId, 0);
		if (event != NULL)
			ngsModuleTraceEmitRecord(event, &state->agentDef.eventDefs[rec->eventId - 1], rec);
	}
	ngsRwLockReadUnlock(&state->rwLock);
	ngsAtomicInc32(&state->started);
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentOnStarted(void *agent, void *userData)
{
	NgsSulphaNamedObjectsAgent *state = (NgsSulphaNamedObjectsAgent *)userData;

	for (SceUInt32 i = 0; i < state->count; i++) {
		NgsSulphaNamedEntry *entry = &state->entries[i];
		if (entry->location != NULL) {
			void *event = NULL;
			sceSulphaEventNew(&event, agent, 1, 0);
			if (event != NULL) {
				sceSulphaEventAdd(event, 1, &entry->location, 4);
				sceSulphaEventAdd(event, 2, &entry->length, 4);
				SceSize nameLen = ngsStrnlen(entry->name, sizeof(entry->name));
				sceSulphaEventAdd(event, 3, entry->name, nameLen);
				sceSulphaEventReport(event, entry->sulphaNode);
			}
		}
	}

	ngsAtomicInc32(&state->started);
	return SCE_OK;
}

SceInt32 ngsPcmCaptureAgentOnStarted(void *agent, void *userData)
{
	NgsSulphaPcmCaptureAgent *state = (NgsSulphaPcmCaptureAgent *)userData;

	ngsRwLockReadLock(&state->rwLock);
	for (NgsSulphaListNode *it = state->sinkList.next; it != &state->sinkList; it = it->next) {
		NgsSulphaPcmSinkRecord *rec = (NgsSulphaPcmSinkRecord *)it;
		void *event = NULL;
		sceSulphaEventNew(&event, agent, 2, 0);
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 2, &rec->dataType, 4);
		sceSulphaEventAdd(event, 3, &rec->format, 4);
		sceSulphaEventAdd(event, 4, &rec->channels, 4);
		sceSulphaEventAdd(event, 5, &rec->sampleRate, 4);
		sceSulphaEventReport(event, rec->sulphaNode);
	}
	for (NgsSulphaListNode *it = state->sourceList.next; it != &state->sourceList; it = it->next) {
		NgsSulphaPcmSourceRecord *rec = (NgsSulphaPcmSourceRecord *)it;
		void *event = NULL;
		sceSulphaEventNew(&event, agent, 3, 0);
		sceSulphaEventAdd(event, 1, &rec->bufferCount, 1);
		sceSulphaEventAdd(event, 2, &rec->currentBuffer, 1);
		sceSulphaEventAdd(event, 3, &rec->position, 4);
		sceSulphaEventAdd(event, 4, &rec->loopCount, 2);
		sceSulphaEventReport(event, rec->sulphaNode);
	}
	ngsRwLockReadUnlock(&state->rwLock);
	ngsAtomicInc32(&state->started);
	return SCE_OK;
}

SceInt32 ngsSynthTraceAgentOnStarted(void *agent, void *userData)
{
	NgsSulphaSynthTraceAgent *state = (NgsSulphaSynthTraceAgent *)userData;

	ngsRwLockReadLock(&state->rwLock);
	for (NgsSulphaListNode *it = state->recordList.next; it != &state->recordList; it = it->next) {
		NgsSulphaSynthRecord *rec = (NgsSulphaSynthRecord *)it;
		void *event = NULL;
		sceSulphaEventNew(&event, agent, rec->objectType, 0);
		if (event != NULL)
			ngsSynthTraceEmitRecord(event, rec, rec->name);
	}
	ngsRwLockReadUnlock(&state->rwLock);
	ngsAtomicInc32(&state->started);
	return SCE_OK;
}

SceInt32 ngsModuleTraceGetDefaultConfig(NgsSulphaModuleTraceConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_MODULE_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaModuleTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultModuleTraceConfig, sizeof(*cfg));
	return SCE_OK;
}

char *ngsSafeStrncpy(char *dst, SceUInt32 dstSize, const char *src, SceUInt32 maxCount)
{
	if (dst == NULL || dstSize == 0)
		return dst;
	if (src == NULL || maxCount == 0) {
		dst[0] = '\0';
		return dst;
	}
	if (maxCount == (SceUInt32)-1) {
		SceUInt32 rem = dstSize;
		char *out = dst;
		while (rem > 0) {
			char c = *src++;
			*out++ = c;
			if (c == '\0')
				return dst;
			rem--;
		}
		dst[dstSize - 1] = '\0';
		return dst;
	}
	if (dstSize > maxCount) {
		SceUInt32 rem = dstSize;
		char *out = dst;
		while (rem > 0) {
			char c = *src++;
			*out = c;
			if (c == '\0')
				return dst;
			out++;
			rem--;
			maxCount--;
			if (maxCount == 0) {
				*out = '\0';
				return dst;
			}
		}
		dst[0] = '\0';
	}
	return dst;
}

SceInt32 ngsFormatString(char *buf, SceInt32 size, const char *fmt, va_list ap)
{
	SceInt32 written = ngsVsnprintf(buf, (SceSize)size, fmt, ap);
	if (written < 0 || written >= size) {
		buf[size - 1] = '\0';
		return 0;
	}
	return 1;
}

SceInt32 ngsModuleTraceCreate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaModuleTraceAgent *state = NULL;

	if (snapshotData == NULL || snapshotData[4] == 0 || obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_MODULE_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaModuleRecord *rec = (NgsSulphaModuleRecord *)ngsPoolAlloc(&state->recordPool);
	if (rec == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	SceUInt8 evId = 0;
	SceUInt32 modId = snapshotData[4];
	for (SceUInt32 i = 0; i < state->moduleCount; i++) {
		if (state->moduleIds[i] == modId) {
			evId = (SceUInt8)(i + 1);
			break;
		}
	}

	obj->agentRecord = rec;

	ngsRwLockWriteLock(&state->rwLock);
	ngsListPushBack(&state->recordList, &rec->list);
	ngsRwLockWriteUnlock(&state->rwLock);

	sceSulphaNodeNew(&obj->sulphaNode, agent, obj->parentSulphaNode);
	rec->sulphaNode = obj->sulphaNode;
	rec->eventId = evId;
	rec->moduleIndex = (SceUInt8)snapshotData[2];
	rec->bypassFlag = (SceUInt8)snapshotData[6];
	rec->cycles = snapshotData[5];

	SceUInt32 paramBytes = snapshotData[7];
	if (paramBytes > 8U && paramBytes - 8U <= state->maxParamBytes)
		ngsMemcpy(rec + 1, &snapshotData[10], paramBytes - 8U);

	if (evId != 0 && state->started != 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, evId, 0);
		if (event != NULL)
			ret = ngsModuleTraceEmitRecord(event, &state->agentDef.eventDefs[evId - 1], rec);
	}
	return ret;
}

SceInt32 ngsModuleTraceUpdate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaModuleTraceAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL || snapshotData == NULL || snapshotData[4] == 0)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_MODULE_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	SceUInt8 evId = 0;
	SceUInt32 modId = snapshotData[4];
	for (SceUInt32 i = 0; i < state->moduleCount; i++) {
		if (state->moduleIds[i] == modId) {
			evId = (SceUInt8)(i + 1);
			break;
		}
	}
	if (evId == 0)
		return SCE_SULPHA_ERROR_INVALID_EVENT_ID;

	NgsSulphaModuleRecord *rec = (NgsSulphaModuleRecord *)obj->agentRecord;
	if (rec->sulphaNode != obj->sulphaNode)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	rec->bypassFlag = (SceUInt8)snapshotData[6];
	rec->cycles = snapshotData[5];
	SceUInt32 paramBytes = snapshotData[7];
	if (paramBytes > 8U && paramBytes - 8U <= state->maxParamBytes)
		ngsMemcpy(rec + 1, &snapshotData[10], paramBytes - 8U);

	if (state->started != 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, evId, 0);
		if (event != NULL)
			ret = ngsModuleTraceEmitRecord(event, &state->agentDef.eventDefs[rec->eventId - 1], rec);
	}
	return ret;
}

SceInt32 ngsModuleTraceDestroy(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaModuleTraceAgent *state = NULL;

	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_MODULE_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaModuleRecord *rec = (NgsSulphaModuleRecord *)obj->agentRecord;
	if (rec->sulphaNode != obj->sulphaNode)
		return ret;

	sceSulphaNodeDelete(rec->sulphaNode, rec->eventId);
	ngsRwLockWriteLock(&state->rwLock);
	ngsListRemove(&rec->list);
	ngsPoolFree(&state->recordPool, rec);
	ngsRwLockWriteUnlock(&state->rwLock);
	return SCE_OK;
}

SceInt32 ngsNamedObjectsAgentShutdown(void *agent, void *userData)
{
	NgsSulphaNamedObjectsAgent *state = (NgsSulphaNamedObjectsAgent *)userData;
	if (agent == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	for (SceUInt32 i = 0; i < state->count; i++) {
		if (state->entries[i].sulphaNode != NULL)
			sceSulphaNodeDelete(state->entries[i].sulphaNode, 1);
	}
	if (state->rootNode != NULL)
		sceSulphaNodeDelete(state->rootNode, 1);
	return SCE_OK;
}

SceInt32 ngsNamedObjectsGetDefaultConfig(NgsSulphaNamedObjectsConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_NAMED_OBJECTS ||
	    cfg->header.configSize != sizeof(NgsSulphaNamedObjectsConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultNamedObjectsConfig, sizeof(*cfg));
	return SCE_OK;
}

SceInt32 ngsNamedObjectsSetSample(SceUInt64 agentHandle, const void *location, SceUInt32 length, const char *name)
{
	void *agent = NULL;
	NgsSulphaNamedObjectsAgent *state = NULL;

	if (location == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	if (sceSulphaGetAgent(agentHandle, &agent, (void **)&state) < 0 ||
	    state == NULL || state->typeId != NGS_SULPHA_AGENT_NAMED_OBJECTS) {
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	}

	if (state->count >= state->capacity)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	NgsSulphaNamedEntry *entry = NULL;
	for (SceUInt32 i = 0; i < state->count; i++) {
		if (state->entries[i].location == location)
			entry = &state->entries[i];
	}
	if (entry == NULL) {
		entry = &state->entries[state->count];
		state->count++;
	}

	entry->location = location;
	entry->length = length;

	if (name != NULL) {
		if (entry->sulphaNode == NULL)
			sceSulphaNodeNew(&entry->sulphaNode, agent, state->rootNode);
		ngsStrncpy(entry->name, name, sizeof(entry->name));
		entry->name[sizeof(entry->name) - 1] = '\0';

		void *event = NULL;
		sceSulphaEventNew(&event, agent, 1, 0);
		if (event != NULL) {
			sceSulphaEventAdd(event, 1, &entry->location, 4);
			sceSulphaEventAdd(event, 2, &entry->length, 4);
			SceSize nameLen = ngsStrnlen(entry->name, sizeof(entry->name));
			sceSulphaEventAdd(event, 3, entry->name, nameLen);
			sceSulphaEventReport(event, entry->sulphaNode);
		}
	} else {
		sceSulphaNodeDelete(entry->sulphaNode, 1);
		entry->sulphaNode = NULL;
		entry->location = location;
		entry->name[0] = '\0';
	}
	return SCE_OK;
}

SceInt32 ngsPcmCaptureGetDefaultConfig(NgsSulphaPcmCaptureConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_PCM_CAPTURE ||
	    cfg->header.configSize != sizeof(NgsSulphaPcmCaptureConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultPcmCaptureConfig, sizeof(*cfg));
	return SCE_OK;
}

SceInt32 ngsPcmCaptureCreateSource(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (snapshotData == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (snapshotData[0] == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSourceRecord *rec = (NgsSulphaPcmSourceRecord *)ngsPoolAlloc(&state->sourcePool);
	if (rec == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	obj->agentRecord = rec;
	ngsRwLockWriteLock(&state->rwLock);
	ngsListPushBack(&state->sourceList, &rec->list);
	ngsRwLockWriteUnlock(&state->rwLock);

	sceSulphaNodeNew(&obj->sulphaNode, agent, obj->parentSulphaNode);
	rec->sulphaNode = obj->sulphaNode;
	rec->handle = snapshotData[0];
	rec->bufferCount = (SceUInt8)snapshotData[5];
	rec->currentBuffer = (SceInt8)((const SceUInt8 *)snapshotData)[30];
	rec->position = snapshotData[6];
	rec->loopCount = (SceInt16)(snapshotData[7] & 0xFFFFU);

	if (ret >= 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, 3, 0);
		if (event != NULL) {
			sceSulphaEventAdd(event, 1, &rec->bufferCount, 1);
			sceSulphaEventAdd(event, 2, &rec->currentBuffer, 1);
			sceSulphaEventAdd(event, 3, &rec->position, 4);
			sceSulphaEventAdd(event, 4, &rec->loopCount, 2);
			ret = sceSulphaEventReport(event, rec->sulphaNode);
		}
	}
	return ret;
}

SceInt32 ngsPcmCaptureCreateSink(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (snapshotData == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (snapshotData[0] == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSinkRecord *rec = (NgsSulphaPcmSinkRecord *)ngsPoolAlloc(&state->sinkPool);
	if (rec == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	obj->agentRecord = rec;
	ngsRwLockWriteLock(&state->rwLock);
	ngsListPushBack(&state->sinkList, &rec->list);
	ngsRwLockWriteUnlock(&state->rwLock);

	sceSulphaNodeNew(&obj->sulphaNode, agent, obj->parentSulphaNode);
	rec->sulphaNode = obj->sulphaNode;
	rec->handle = snapshotData[0];
	rec->dataType = snapshotData[4];
	rec->format = snapshotData[5];
	rec->channels = snapshotData[6];
	rec->sampleRate = snapshotData[7];

	if (ret >= 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, 2, 0);
		if (event != NULL) {
			sceSulphaEventAdd(event, 1, &rec->handle, 4);
			sceSulphaEventAdd(event, 2, &rec->dataType, 4);
			sceSulphaEventAdd(event, 3, &rec->format, 4);
			sceSulphaEventAdd(event, 4, &rec->channels, 4);
			sceSulphaEventAdd(event, 5, &rec->sampleRate, 4);
			ret = sceSulphaEventReport(event, rec->sulphaNode);
		}
	}
	return ret;
}

SceInt32 ngsPcmCaptureUpdateSource(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL || snapshotData == NULL || snapshotData[0] == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSourceRecord *rec = (NgsSulphaPcmSourceRecord *)obj->agentRecord;
	if (rec->handle == snapshotData[0]) {
		rec->bufferCount = (SceUInt8)snapshotData[5];
		rec->currentBuffer = (SceInt8)((const SceUInt8 *)snapshotData)[30];
		rec->position = snapshotData[6];
		rec->loopCount = (SceInt16)(snapshotData[7] & 0xFFFFU);
		if (state->started != 0) {
			void *event = NULL;
			sceSulphaEventNew(&event, agent, 3, 0);
			if (event != NULL) {
				sceSulphaEventAdd(event, 2, &rec->currentBuffer, 1);
				sceSulphaEventAdd(event, 3, &rec->position, 4);
				sceSulphaEventAdd(event, 4, &rec->loopCount, 2);
				ret = sceSulphaEventReport(event, rec->sulphaNode);
			}
		}
	}
	return ret;
}

SceInt32 ngsPcmCaptureUpdateSink(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL || snapshotData == NULL || snapshotData[0] == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSinkRecord *rec = (NgsSulphaPcmSinkRecord *)obj->agentRecord;
	if (rec->handle != snapshotData[0] || state->started == 0)
		return ret;

	SceUInt32 pcmSize = snapshotData[9];
	if (pcmSize == 0 || pcmSize > 2048U)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	void *event = NULL;
	sceSulphaEventNew(&event, agent, 1, 0);
	if (event != NULL) {
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 2, &snapshotData[8], 4);
		sceSulphaEventAdd(event, 3, &snapshotData[10], pcmSize);
		sceSulphaEventAdd(event, 4, &snapshotData[9], 4);
		ret = sceSulphaEventReport(event, NULL);
	}
	return ret;
}

SceInt32 ngsPcmCaptureDestroySink(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSinkRecord *rec = (NgsSulphaPcmSinkRecord *)obj->agentRecord;
	if (rec->sulphaNode != obj->sulphaNode)
		return ret;

	sceSulphaNodeDelete(rec->sulphaNode, 2);
	ngsRwLockWriteLock(&state->rwLock);
	ngsListRemove(&rec->list);
	ngsPoolFree(&state->sinkPool, rec);
	ngsRwLockWriteUnlock(&state->rwLock);
	return SCE_OK;
}

SceInt32 ngsPcmCaptureDestroySource(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaPcmCaptureAgent *state = NULL;

	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_PCM_CAPTURE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaPcmSourceRecord *rec = (NgsSulphaPcmSourceRecord *)obj->agentRecord;
	if (rec->sulphaNode != obj->sulphaNode)
		return ret;

	sceSulphaNodeDelete(rec->sulphaNode, 3);
	ngsRwLockWriteLock(&state->rwLock);
	ngsListRemove(&rec->list);
	ngsPoolFree(&state->sourcePool, rec);
	ngsRwLockWriteUnlock(&state->rwLock);
	return SCE_OK;
}

SceInt32 ngsSynthTraceEmitRecord(void *event, const NgsSulphaSynthRecord *rec, const char *name)
{
	switch (rec->objectType) {
	case 1:
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 8, &rec->index, 2);
		sceSulphaEventAdd(event, 2, &rec->params[1], 4);
		sceSulphaEventAdd(event, 3, &rec->params[2], 4);
		sceSulphaEventAdd(event, 4, &rec->params[3], 4);
		sceSulphaEventAdd(event, 6, &rec->params[0], 4);
		sceSulphaEventAdd(event, 5, &rec->params[4], 4);
		sceSulphaEventAdd(event, 10, &rec->params[5], 4);
		if (name != NULL && name[0] != '\0')
			sceSulphaEventAdd(event, 9, name, ngsStrnlen(name, sizeof(rec->name)));
		return sceSulphaEventReport(event, rec->sulphaNode);
	case 2:
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 4, &rec->index, 2);
		sceSulphaEventAdd(event, 2, &rec->params[0], 4);
		sceSulphaEventAdd(event, 5, &rec->params[1], 4);
		sceSulphaEventAdd(event, 3, &rec->params[3], 4);
		sceSulphaEventAdd(event, 6, &rec->params[4], 4);
		sceSulphaEventAdd(event, 7, &rec->params[2], 4);
		sceSulphaEventAdd(event, 9, &rec->params[5], 4);
		if (name != NULL && name[0] != '\0')
			sceSulphaEventAdd(event, 8, name, ngsStrnlen(name, sizeof(rec->name)));
		return sceSulphaEventReport(event, rec->sulphaNode);
	case 3:
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 7, &rec->index, 2);
		sceSulphaEventAdd(event, 2, &rec->params[1], 4);
		sceSulphaEventAdd(event, 3, &rec->params[2], 4);
		sceSulphaEventAdd(event, 4, &rec->params[3], 4);
		sceSulphaEventAdd(event, 5, &rec->params[4], 4);
		sceSulphaEventAdd(event, 6, &rec->params[5], 4);
		sceSulphaEventAdd(event, 9, &rec->params[6], 4);
		if (name != NULL && name[0] != '\0')
			sceSulphaEventAdd(event, 8, name, ngsStrnlen(name, sizeof(rec->name)));
		return sceSulphaEventReport(event, rec->sulphaNode);
	case 4:
		sceSulphaEventAdd(event, 1, &rec->handle, 4);
		sceSulphaEventAdd(event, 2, &rec->params[3], 4);
		sceSulphaEventAdd(event, 3, &rec->params[1], 4);
		sceSulphaEventAdd(event, 4, &rec->params[0], 4);
		sceSulphaEventAdd(event, 5, &rec->params[2], 4);
		if (name != NULL && name[0] != '\0')
			sceSulphaEventAdd(event, 6, name, ngsStrnlen(name, sizeof(rec->name)));
		return sceSulphaEventReport(event, rec->sulphaNode);
	default:
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	}
}

SceInt32 ngsSynthTraceCopyParams(NgsSulphaSynthRecord *rec, const SceUInt32 *snapshotData)
{
	SceSize copySize;
	switch (rec->objectType) {
	case 1:
	case 2:
		copySize = 24;
		break;
	case 3:
		copySize = 28;
		break;
	case 4:
		copySize = 32;
		break;
	default:
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	}

	if (ngsMemcmp(rec->params, &snapshotData[4], copySize) == 0)
		return 1;

	ngsMemcpy(rec->params, &snapshotData[4], copySize);
	return 0;
}

SceInt32 ngsSynthTraceGetDefaultConfig(NgsSulphaSynthTraceConfig *cfg)
{
	if (cfg == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (cfg->header.typeId != NGS_SULPHA_AGENT_SYNTH_TRACE ||
	    cfg->header.configSize != sizeof(NgsSulphaSynthTraceConfig))
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	ngsMemcpyChecked(cfg, sizeof(*cfg), &g_ngsDefaultSynthTraceConfig, sizeof(*cfg));
	return SCE_OK;
}

SceInt32 ngsSynthTraceCreate(SceUInt64 agentHandle, SceUInt32 objectType,
			     const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaSynthTraceAgent *state = NULL;

	if (snapshotData == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (snapshotData[0] == 0 || objectType == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (obj == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_SYNTH_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaSynthRecord *rec = (NgsSulphaSynthRecord *)ngsPoolAlloc(&state->recordPool);
	if (rec == NULL)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	obj->agentRecord = rec;
	ngsRwLockWriteLock(&state->rwLock);
	ngsListPushBack(&state->recordList, &rec->list);
	ngsRwLockWriteUnlock(&state->rwLock);

	sceSulphaNodeNew(&obj->sulphaNode, agent, obj->parentSulphaNode);
	rec->sulphaNode = obj->sulphaNode;
	rec->handle = obj->handle;
	rec->objectType = (SceUInt8)objectType;
	rec->index = (SceUInt16)snapshotData[2];

	switch (objectType) {
	case 1:
		ngsStrncpy(rec->name, "Synth", sizeof(rec->name));
		break;
	case 2:
		ngsStrncpy(rec->name, "Rack", sizeof(rec->name));
		break;
	case 3:
		ngsStrncpy(rec->name, "Voice", sizeof(rec->name));
		break;
	case 4:
		ngsStrncpy(rec->name, "Patch", sizeof(rec->name));
		break;
	default:
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	}

	if (ngsSynthTraceCopyParams(rec, snapshotData) == 0 && state->started != 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, (SceUInt8)objectType, 0);
		if (event != NULL)
			return ngsSynthTraceEmitRecord(event, rec, rec->name);
	}
	return ret;
}

SceInt32 ngsSynthTraceDestroy(SceUInt64 agentHandle, SceUInt8 eventId, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaSynthTraceAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_SYNTH_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaSynthRecord *rec = (NgsSulphaSynthRecord *)obj->agentRecord;
	if (rec->handle != obj->handle)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	sceSulphaNodeDelete(rec->sulphaNode, eventId);
	ngsRwLockWriteLock(&state->rwLock);
	ngsListRemove(&rec->list);
	ngsPoolFree(&state->recordPool, rec);
	ngsRwLockWriteUnlock(&state->rwLock);
	ngsMemset(rec, 0, sizeof(*rec));
	return SCE_OK;
}

SceInt32 ngsSynthTraceUpdate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaSynthTraceAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (snapshotData == NULL || snapshotData[0] == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	SceInt32 ret = sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_SYNTH_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaSynthRecord *rec = (NgsSulphaSynthRecord *)obj->agentRecord;
	if (rec->handle == snapshotData[0] &&
	    ngsSynthTraceCopyParams(rec, snapshotData) == 0 &&
	    state->started != 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, rec->objectType, 0);
		if (event != NULL)
			ret = ngsSynthTraceEmitRecord(event, rec, NULL);
	}
	return ret;
}

SceInt32 ngsSynthTraceIsSystem(SceUInt64 agentHandle, const NgsSulphaTrackedObject *obj)
{
	void *agent = NULL;
	NgsSulphaSynthTraceAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_SYNTH_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	const NgsSulphaSynthRecord *rec = (const NgsSulphaSynthRecord *)obj->agentRecord;
	return (rec->objectType == 1) ? SCE_OK : NGS_SULPHA_ERROR_INTERNAL;
}

SceInt32 ngsSynthTraceSetName(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj, const char *name)
{
	void *agent = NULL;
	NgsSulphaSynthTraceAgent *state = NULL;

	if (obj == NULL || obj->agentRecord == NULL)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	sceSulphaGetAgent(agentHandle, &agent, (void **)&state);
	if (state == NULL || state->typeId != NGS_SULPHA_AGENT_SYNTH_TRACE)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	NgsSulphaSynthRecord *rec = (NgsSulphaSynthRecord *)obj->agentRecord;
	if (name != NULL) {
		ngsStrncpy(rec->name, name, sizeof(rec->name));
		rec->name[sizeof(rec->name) - 1] = '\0';
	} else {
		switch (rec->objectType) {
		case 1:
			ngsStrncpy(rec->name, "Synth", sizeof(rec->name));
			break;
		case 2:
			ngsStrncpy(rec->name, "Rack", sizeof(rec->name));
			break;
		case 3:
			ngsStrncpy(rec->name, "Voice", sizeof(rec->name));
			break;
		case 4:
			ngsStrncpy(rec->name, "Patch", sizeof(rec->name));
			break;
		default:
			return SCE_SULPHA_ERROR_INVALID_VALUE;
		}
	}

	if (state->started != 0) {
		void *event = NULL;
		sceSulphaEventNew(&event, agent, rec->objectType, 0);
		if (event != NULL)
			ngsSynthTraceEmitRecord(event, rec, rec->name);
	}
	return SCE_OK;
}
