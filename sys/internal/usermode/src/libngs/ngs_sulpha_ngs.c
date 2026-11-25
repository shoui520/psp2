/*
 * Sulpha NGS object tracking, snapshot processing, and public sceSulphaNgs* API.
 */
#include "ngs_internal.h"

extern NgsSulphaSynthTraceConfig g_ngsSynthTraceCfg;
extern NgsSulphaApiTraceConfig g_ngsApiTraceCfg;
extern NgsSulphaModuleTraceConfig g_ngsModuleTraceCfg;
extern NgsSulphaPcmCaptureConfig g_ngsPcmCaptureCfg;
extern NgsSulphaNamedObjectsConfig g_ngsNamedObjectsCfg;
extern NgsSulphaMessageLogConfig g_ngsMessageLogCfg;

volatile SceInt32 g_ngsSulphaState = -1;
NgsSulphaContext g_ngsSulphaCtx;

static SceUInt32 g_ngsSulphaModuleIds[NGS_MAX_MODULES_PER_VOICE];
static SceUInt32 g_ngsSulphaModuleCount;
static const NgsSulphaApiTraceEventDesc *g_ngsSulphaApiTable;
static SceUInt8 g_ngsSulphaApiCount;

static inline SceInt32 ngsSulphaEnter(void)
{
	for (;;) {
		SceInt32 cur = g_ngsSulphaState;
		if (cur < 0)
			return SCE_SULPHA_ERROR_UNINITIALIZED;
		while ((SceInt32)ngsAtomicLoad32(&g_ngsSulphaState) == cur) {
			if (ngsAtomicStore32((SceUInt32)(cur + 1), &g_ngsSulphaState) == 0)
				return SCE_OK;
		}
	}
}

static inline void ngsSulphaLeave(void)
{
	for (;;) {
		SceInt32 cur = g_ngsSulphaState;
		while ((SceInt32)ngsAtomicLoad32(&g_ngsSulphaState) == cur) {
			if (ngsAtomicStore32((SceUInt32)(cur - 1), &g_ngsSulphaState) == 0)
				return;
		}
	}
}

static inline __attribute__((always_inline)) NgsSulphaTrackedObject *ngsHashLookupLocked(SceUInt32 key)
{
	SceInt32 bucket = (SceInt32)(key % (SceUInt32)g_ngsSulphaCtx.objectMap.bucketCount);
	for (NgsSulphaHashEntry *e = g_ngsSulphaCtx.objectMap.buckets[bucket]; e != NULL; e = e->next) {
		if (e->key == key)
			return e->value;
	}
	return NULL;
}

static inline __attribute__((always_inline)) NgsSulphaTrackedObject *ngsHashLookup(SceUInt32 key)
{
	ngsRwLockReadLock(&g_ngsSulphaCtx.rwLock);
	NgsSulphaTrackedObject *obj = ngsHashLookupLocked(key);
	ngsRwLockReadUnlock(&g_ngsSulphaCtx.rwLock);
	return obj;
}

static inline NgsSulphaTrackedObject *ngsHashFindByParent(void *parentSulphaNode)
{
	NgsSulphaTrackedObject *found = NULL;
	ngsRwLockReadLock(&g_ngsSulphaCtx.rwLock);
	for (SceInt32 b = 0; b < g_ngsSulphaCtx.objectMap.bucketCount && found == NULL; b++) {
		for (NgsSulphaHashEntry *e = g_ngsSulphaCtx.objectMap.buckets[b]; e != NULL; e = e->next) {
			if (e->value == NULL || e->value->parentSulphaNode == parentSulphaNode) {
				found = e->value;
				break;
			}
		}
	}
	ngsRwLockReadUnlock(&g_ngsSulphaCtx.rwLock);
	return found;
}

static inline __attribute__((always_inline)) NgsSulphaTrackedObject *ngsHashAllocAndInsert(SceUInt32 handle, void *parentSulphaNode)
{
	ngsRwLockWriteLock(&g_ngsSulphaCtx.rwLock);
	NgsSulphaTrackedObject *obj = NULL;
	if (g_ngsSulphaCtx.objectPool.allocIndex < g_ngsSulphaCtx.objectPool.capacity) {
		obj = (NgsSulphaTrackedObject *)g_ngsSulphaCtx.objectPool.freeStack[g_ngsSulphaCtx.objectPool.allocIndex++];
		if (obj != NULL) {
			obj->handle = handle;
			obj->agentRecord = NULL;
			obj->sulphaNode = NULL;
			obj->parentSulphaNode = parentSulphaNode;

			SceInt32 bucket = (SceInt32)(handle % (SceUInt32)g_ngsSulphaCtx.objectMap.bucketCount);
			NgsSulphaHashEntry *existing = g_ngsSulphaCtx.objectMap.buckets[bucket];
			while (existing != NULL && existing->key != handle)
				existing = existing->next;

			if (existing != NULL) {
				existing->key = handle;
				existing->value = obj;
			} else if (g_ngsSulphaCtx.objectMap.entryPool.allocIndex <
				   g_ngsSulphaCtx.objectMap.entryPool.capacity) {
				NgsSulphaHashEntry *entry = (NgsSulphaHashEntry *)
					g_ngsSulphaCtx.objectMap.entryPool.freeStack[
						g_ngsSulphaCtx.objectMap.entryPool.allocIndex++];
				if (entry != NULL) {
					entry->key = handle;
					entry->value = obj;
					entry->next = g_ngsSulphaCtx.objectMap.buckets[bucket];
					g_ngsSulphaCtx.objectMap.buckets[bucket] = entry;
					g_ngsSulphaCtx.objectMap.count++;
				}
			}
		}
	}
	ngsRwLockWriteUnlock(&g_ngsSulphaCtx.rwLock);
	return obj;
}

static inline void ngsHashRemoveAndFree(NgsSulphaTrackedObject *obj)
{
	ngsRwLockWriteLock(&g_ngsSulphaCtx.rwLock);
	SceUInt32 key = obj->handle;
	SceInt32 bucket = (SceInt32)(key % (SceUInt32)g_ngsSulphaCtx.objectMap.bucketCount);
	NgsSulphaHashEntry **pp = &g_ngsSulphaCtx.objectMap.buckets[bucket];

	while (*pp != NULL) {
		if ((*pp)->key == key) {
			NgsSulphaHashEntry *victim = *pp;
			*pp = victim->next;
			SceUIntPtr base = (SceUIntPtr)g_ngsSulphaCtx.objectMap.entryPool.storage;
			SceUIntPtr end = base + (SceUInt32)(g_ngsSulphaCtx.objectMap.entryPool.elemSize *
							    g_ngsSulphaCtx.objectMap.entryPool.capacity);
			if ((SceUIntPtr)victim >= base && (SceUIntPtr)victim < end) {
				g_ngsSulphaCtx.objectMap.entryPool.allocIndex--;
				g_ngsSulphaCtx.objectMap.entryPool.freeStack[
					g_ngsSulphaCtx.objectMap.entryPool.allocIndex] = victim;
			}
			g_ngsSulphaCtx.objectMap.count--;
			break;
		}
		pp = &(*pp)->next;
	}

	SceUIntPtr objBase = (SceUIntPtr)g_ngsSulphaCtx.objectPool.storage;
	SceUIntPtr objEnd = objBase + (SceUInt32)(g_ngsSulphaCtx.objectPool.elemSize *
						  g_ngsSulphaCtx.objectPool.capacity);
	if ((SceUIntPtr)obj >= objBase && (SceUIntPtr)obj < objEnd) {
		g_ngsSulphaCtx.objectPool.allocIndex--;
		g_ngsSulphaCtx.objectPool.freeStack[g_ngsSulphaCtx.objectPool.allocIndex] = obj;
	}
	ngsRwLockWriteUnlock(&g_ngsSulphaCtx.rwLock);
}

const NgsSulphaAgentsConfig *ngsSulphaBuildAgentsConfig(const SceSulphaNgsConfig *config)
{
	ngsSulphaGetModuleList(g_ngsSulphaModuleIds, NGS_MAX_MODULES_PER_VOICE, &g_ngsSulphaModuleCount);

	g_ngsSynthTraceCfg.header.typeId = NGS_SULPHA_AGENT_SYNTH_TRACE;
	g_ngsSynthTraceCfg.header.configSize = sizeof(g_ngsSynthTraceCfg);
	ngsSynthTraceGetDefaultConfig(&g_ngsSynthTraceCfg);

	g_ngsModuleTraceCfg.header.typeId = NGS_SULPHA_AGENT_MODULE_TRACE;
	g_ngsModuleTraceCfg.header.configSize = sizeof(g_ngsModuleTraceCfg);
	ngsModuleTraceGetDefaultConfig(&g_ngsModuleTraceCfg);
	g_ngsModuleTraceCfg.moduleCount = g_ngsSulphaModuleCount;
	g_ngsModuleTraceCfg.moduleIds = g_ngsSulphaModuleIds;

	g_ngsApiTraceCfg.header.typeId = NGS_SULPHA_AGENT_API_TRACE;
	g_ngsApiTraceCfg.header.configSize = sizeof(g_ngsApiTraceCfg);
	ngsApiTraceGetDefaultConfig(&g_ngsApiTraceCfg);
	ngsGetApiTraceTable(&g_ngsSulphaApiTable, &g_ngsSulphaApiCount);
	g_ngsApiTraceCfg.eventCount = g_ngsSulphaApiCount;
	g_ngsApiTraceCfg.events = g_ngsSulphaApiTable;
	g_ngsApiTraceCfg.reserved0 = 0;
	g_ngsApiTraceCfg.reserved1 = 0;

	g_ngsPcmCaptureCfg.header.typeId = NGS_SULPHA_AGENT_PCM_CAPTURE;
	g_ngsPcmCaptureCfg.header.configSize = sizeof(g_ngsPcmCaptureCfg);
	ngsPcmCaptureGetDefaultConfig(&g_ngsPcmCaptureCfg);

	g_ngsNamedObjectsCfg.header.typeId = NGS_SULPHA_AGENT_NAMED_OBJECTS;
	g_ngsNamedObjectsCfg.header.configSize = sizeof(g_ngsNamedObjectsCfg);
	ngsNamedObjectsGetDefaultConfig(&g_ngsNamedObjectsCfg);
	g_ngsNamedObjectsCfg.maxNamedObjects = config->maxNamedObjects;

	g_ngsMessageLogCfg.header.typeId = NGS_SULPHA_AGENT_MESSAGE_LOG;
	g_ngsMessageLogCfg.header.configSize = sizeof(g_ngsMessageLogCfg);
	ngsMessageLogGetDefaultConfig(&g_ngsMessageLogCfg);
	g_ngsMessageLogCfg.maxTraceBufferBytes = config->maxTraceBufferBytes;

	return &g_ngsSulphaAgentsConfig;
}

int ngsSulphaApiTraceHook(const NgsSulphaApiTraceEventDesc *desc, ...)
{
	va_list ap;
	va_start(ap, desc);
	int ret = ngsApiTraceReportCall(g_ngsSulphaCtx.apiTraceHandle,
					(SceUInt8)desc->eventId,
					(SceUInt8)desc->paramCount,
					ap);
	va_end(ap);
	return ret;
}

void ngsSulphaSynthUpdateCallback(SceNgsBufferInfo *buf)
{
	if (buf == NULL || buf->data == NULL)
		return;

	const NgsSulphaSnapshotHeader *hdr = (const NgsSulphaSnapshotHeader *)buf->data;
	if (hdr->totalSize > buf->size || hdr->magic != NGS_SULPHA_SNAPSHOT_MAGIC || hdr->recordCount == 0)
		return;

	const NgsSulphaRecordHeader *rec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)buf->data + 16);
	while (rec != NULL && rec->recordType != 0) {
		const SceUInt32 *payload = (const SceUInt32 *)(rec + 1);
		NgsSulphaTrackedObject *obj = ngsHashLookup(payload[0]);

		switch (rec->recordType) {
		case 1:
		case 2:
		case 3:
		case 4:
			ngsSynthTraceUpdate(g_ngsSulphaCtx.synthTraceHandle, payload, obj);
			break;
		case 5:
			ngsModuleTraceUpdate(g_ngsSulphaCtx.moduleTraceHandle, payload, obj);
			break;
		case 6:
			ngsPcmCaptureUpdateSource(g_ngsSulphaCtx.pcmCaptureHandle, payload, obj);
			break;
		case 7:
			ngsPcmCaptureUpdateSink(g_ngsSulphaCtx.pcmCaptureHandle, payload, obj);
			break;
		default:
			break;
		}

		if (rec->nextOffset == 0)
			break;
		rec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)rec + rec->nextOffset);
	}
}

SceInt32 ngsSulphaUnregisterRack(SceNgsHRack rackHandle)
{
	NgsSulphaTrackedObject *rackObj = ngsHashLookup(rackHandle);
	if (rackObj == NULL)
		return SCE_OK;

	void *rackNode = rackObj->sulphaNode;
	while (g_ngsSulphaCtx.objectMap.count != 0) {
		NgsSulphaTrackedObject *voiceObj = ngsHashFindByParent(rackNode);
		if (voiceObj == NULL)
			break;

		void *voiceNode = voiceObj->sulphaNode;
		while (g_ngsSulphaCtx.objectMap.count != 0) {
			NgsSulphaTrackedObject *childObj = ngsHashFindByParent(voiceNode);
			if (childObj == NULL)
				break;

			void *childNode = childObj->sulphaNode;
			while (g_ngsSulphaCtx.objectMap.count != 0) {
				NgsSulphaTrackedObject *leafObj = ngsHashFindByParent(childNode);
				if (leafObj == NULL)
					break;

				if (ngsPcmCaptureDestroySource(g_ngsSulphaCtx.pcmCaptureHandle, leafObj) != 0)
					ngsPcmCaptureDestroySink(g_ngsSulphaCtx.pcmCaptureHandle, leafObj);
				ngsHashRemoveAndFree(leafObj);
			}

			if (ngsSynthTraceDestroy(g_ngsSulphaCtx.synthTraceHandle, 4, childObj) != 0)
				ngsModuleTraceDestroy(g_ngsSulphaCtx.moduleTraceHandle, childObj);
			ngsHashRemoveAndFree(childObj);
		}

		ngsSynthTraceDestroy(g_ngsSulphaCtx.synthTraceHandle, 3, voiceObj);
		ngsHashRemoveAndFree(voiceObj);
	}

	ngsSynthTraceDestroy(g_ngsSulphaCtx.synthTraceHandle, 2, rackObj);
	ngsHashRemoveAndFree(rackObj);
	return SCE_OK;
}

SceInt32 ngsSulphaUnregisterSystem(SceNgsHSynSystem systemHandle)
{
	NgsSulphaTrackedObject *sysObj = ngsHashLookup(systemHandle);
	if (sysObj == NULL)
		return SCE_OK;

	void *sysNode = sysObj->sulphaNode;
	while (g_ngsSulphaCtx.objectMap.count != 0) {
		NgsSulphaTrackedObject *rackObj = ngsHashFindByParent(sysNode);
		if (rackObj == NULL)
			break;
		ngsSulphaUnregisterRack(rackObj->handle);
	}

	ngsSynthTraceDestroy(g_ngsSulphaCtx.synthTraceHandle, 1, sysObj);
	ngsHashRemoveAndFree(sysObj);
	return SCE_OK;
}

void ngsSulphaObjectNotifyHook(const NgsSulphaRegistration *reg)
{
	if (reg->type == NGS_SULPHA_REG_SYSTEM) {
		if (reg->isCreate == 0) {
			ngsSulphaUnregisterSystem(reg->handle);
			return;
		}
		if (ngsSulphaGetInfo(reg, &g_ngsSulphaCtx.queryBuffer) != SCE_NGS_OK)
			return;

		const NgsSulphaSnapshotHeader *hdr = (const NgsSulphaSnapshotHeader *)g_ngsSulphaCtx.queryBuffer.data;
		if (hdr->totalSize > g_ngsSulphaCtx.queryBuffer.size ||
		    hdr->magic != NGS_SULPHA_SNAPSHOT_MAGIC ||
		    hdr->recordCount == 0) {
			return;
		}

		const NgsSulphaRecordHeader *rec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)hdr + 16);
		if (rec->recordType == 1) {
			const SceUInt32 *payload = (const SceUInt32 *)(rec + 1);
			NgsSulphaTrackedObject *sysObj = ngsHashAllocAndInsert(payload[0], NULL);
			ngsSynthTraceCreate(g_ngsSulphaCtx.synthTraceHandle, 1, payload, sysObj);
			ngsSulphaSetSynthUpdateCallback(reg->handle, ngsSulphaSynthUpdateCallback,
							&g_ngsSulphaCtx.updateBuffer);
		}
		return;
	}

	if (reg->type != NGS_SULPHA_REG_RACK)
		return;

	if (reg->isCreate == 0) {
		ngsSulphaUnregisterRack(reg->handle);
		return;
	}

	if (ngsSulphaGetInfo(reg, &g_ngsSulphaCtx.queryBuffer) != SCE_NGS_OK)
		return;

	const NgsSulphaSnapshotHeader *hdr = (const NgsSulphaSnapshotHeader *)g_ngsSulphaCtx.queryBuffer.data;
	if (hdr->totalSize > g_ngsSulphaCtx.queryBuffer.size ||
	    hdr->magic != NGS_SULPHA_SNAPSHOT_MAGIC ||
	    hdr->recordCount == 0) {
		return;
	}

	const NgsSulphaRecordHeader *rec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)hdr + 16);
	if (rec->recordType != 2)
		return;

	const SceUInt32 *rackPayload = (const SceUInt32 *)(rec + 1);
	SceNgsHRack rackHandle = reg->handle;
	SceUInt32 numVoices = rackPayload[4];
	NgsSulphaTrackedObject *parentSys = ngsHashLookup(rackPayload[1]);
	void *parentSysNode = (parentSys != NULL) ? parentSys->sulphaNode : NULL;
	NgsSulphaTrackedObject *rackObj = ngsHashAllocAndInsert(rackPayload[0], parentSysNode);
	ngsSynthTraceCreate(g_ngsSulphaCtx.synthTraceHandle, 2, rackPayload, rackObj);

	for (SceUInt32 v = 0; v < numVoices; v++) {
		NgsSulphaRegistration voiceReg;
		SceNgsHVoice hVoice = SCE_NGS_INVALID_HANDLE;
		voiceReg.type = NGS_SULPHA_REG_VOICE;
		voiceReg.isCreate = 0;

		if (sceNgsRackGetVoiceHandleInternal(rackHandle, v, &hVoice) < 0)
			continue;
		voiceReg.handle = hVoice;

		if (ngsSulphaGetInfo(&voiceReg, &g_ngsSulphaCtx.queryBuffer) < 0)
			continue;

		const NgsSulphaSnapshotHeader *vhdr = (const NgsSulphaSnapshotHeader *)g_ngsSulphaCtx.queryBuffer.data;
		if (vhdr->totalSize > g_ngsSulphaCtx.queryBuffer.size ||
		    vhdr->magic != NGS_SULPHA_SNAPSHOT_MAGIC ||
		    vhdr->recordCount == 0) {
			continue;
		}

		const NgsSulphaRecordHeader *vrec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)vhdr + 16);
		while (vrec != NULL && vrec->recordType != 0) {
			const SceUInt32 *payload = (const SceUInt32 *)(vrec + 1);
			NgsSulphaTrackedObject *parent = NULL;
			NgsSulphaTrackedObject *obj = NULL;

			switch (vrec->recordType) {
			case 3:
				parent = ngsHashLookup(payload[1]);
				obj = ngsHashAllocAndInsert(payload[0], parent ? parent->sulphaNode : NULL);
				ngsSynthTraceCreate(g_ngsSulphaCtx.synthTraceHandle, 3, payload, obj);
				break;
			case 4:
				parent = ngsHashLookup(payload[1]);
				obj = ngsHashAllocAndInsert(payload[0], parent ? parent->sulphaNode : NULL);
				ngsSynthTraceCreate(g_ngsSulphaCtx.synthTraceHandle, 4, payload, obj);
				break;
			case 5:
				parent = ngsHashLookup(payload[1]);
				obj = ngsHashAllocAndInsert(payload[0], parent ? parent->sulphaNode : NULL);
				ngsModuleTraceCreate(g_ngsSulphaCtx.moduleTraceHandle, payload, obj);
				break;
			case 6:
				parent = ngsHashLookup(payload[1]);
				obj = ngsHashAllocAndInsert(payload[0], parent ? parent->sulphaNode : NULL);
				ngsPcmCaptureCreateSource(g_ngsSulphaCtx.pcmCaptureHandle, payload, obj);
				break;
			case 7:
				parent = ngsHashLookup(payload[1]);
				obj = ngsHashAllocAndInsert(payload[0], parent ? parent->sulphaNode : NULL);
				ngsPcmCaptureCreateSink(g_ngsSulphaCtx.pcmCaptureHandle, payload, obj);
				break;
			default:
				break;
			}

			if (vrec->nextOffset == 0)
				break;
			vrec = (const NgsSulphaRecordHeader *)((const SceUInt8 *)vrec + vrec->nextOffset);
		}
	}
}

SceInt32 ngsSulphaLookupObject(SceUInt32 handle, NgsSulphaTrackedObject **outObj)
{
	*outObj = ngsHashLookup(handle);
	return SCE_OK;
}

int sceSulphaNgsGetDefaultConfig(SceSulphaNgsConfig *config)
{
	if (config == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	config->maxNamedObjects = 1024;
	config->maxTraceBufferBytes = 4096;
	return SCE_OK;
}

int sceSulphaNgsGetNeededMemory(const SceSulphaNgsConfig *config, SceUInt32 *sizeInBytes)
{
	if (config == NULL || sizeInBytes == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	const NgsSulphaAgentsConfig *agentsCfg = ngsSulphaBuildAgentsConfig(config);
	int ret = sceSulphaAgentsGetNeededMemory(agentsCfg, sizeInBytes);
	if (ret >= 0)
		*sizeInBytes += NGS_SULPHA_LOCAL_MEM_SIZE;
	return ret;
}

int sceSulphaNgsInit(const SceSulphaNgsConfig *config, void *buffer, SceUInt32 sizeInBytes)
{
	if (config == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (sizeInBytes < 0x22000U)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (buffer == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	for (;;) {
		if ((SceInt32)ngsAtomicLoad32(&g_ngsSulphaState) != -1)
			return SCE_SULPHA_ERROR_ALREADY_INITIALIZED;
		if (ngsAtomicStore32((SceUInt32)-2, &g_ngsSulphaState) == 0)
			break;
	}

	ngsMemset(&g_ngsSulphaCtx, 0, sizeof(g_ngsSulphaCtx));

	SceUInt8 *base = (SceUInt8 *)buffer;

	/* Object pool: 4096 * 16 bytes + 4096 * 4 bytes = 81920 bytes */
	g_ngsSulphaCtx.objectPool.storage = base;
	g_ngsSulphaCtx.objectPool.freeStack = (void **)(base + 0x10000);
	g_ngsSulphaCtx.objectPool.capacity = NGS_SULPHA_OBJECT_POOL_COUNT;
	g_ngsSulphaCtx.objectPool.elemSize = sizeof(NgsSulphaTrackedObject);
	g_ngsSulphaCtx.objectPool.allocIndex = 0;
	for (SceInt32 i = 0; i < NGS_SULPHA_OBJECT_POOL_COUNT; i++)
		g_ngsSulphaCtx.objectPool.freeStack[i] = base + (SceUInt32)(i * sizeof(NgsSulphaTrackedObject));

	/* Hash entry pool: 6151 * 12 bytes = 73812 bytes at +81920, freeStack at +155732 */
	SceUInt8 *entryStorage = base + 81920;
	g_ngsSulphaCtx.objectMap.entryPool.storage = entryStorage;
	g_ngsSulphaCtx.objectMap.entryPool.capacity = NGS_SULPHA_HASH_BUCKETS;
	g_ngsSulphaCtx.objectMap.entryPool.elemSize = sizeof(NgsSulphaHashEntry);
	g_ngsSulphaCtx.objectMap.entryPool.allocIndex = 0;
	if (entryStorage != NULL) {
		g_ngsSulphaCtx.objectMap.entryPool.freeStack = (void **)(base + 155732);
		for (SceInt32 i = 0; i < NGS_SULPHA_HASH_BUCKETS; i++)
			g_ngsSulphaCtx.objectMap.entryPool.freeStack[i] =
				entryStorage + (SceUInt32)(i * sizeof(NgsSulphaHashEntry));
	}

	/* Hash buckets: 6151 * 4 bytes = 24604 bytes at +180336 */
	g_ngsSulphaCtx.objectMap.buckets = (NgsSulphaHashEntry **)(base + 180336);
	g_ngsSulphaCtx.objectMap.bucketCount = NGS_SULPHA_HASH_BUCKETS;
	ngsMemset(g_ngsSulphaCtx.objectMap.buckets, 0, sizeof(NgsSulphaHashEntry *) * NGS_SULPHA_HASH_BUCKETS);

	/* Update buffer (128 KiB at +204940) and query buffer (8 KiB at +336012) */
	g_ngsSulphaCtx.updateBuffer.data = base + 204940;
	g_ngsSulphaCtx.updateBuffer.size = 0x20000U;
	g_ngsSulphaCtx.queryBuffer.data = base + 336012;
	g_ngsSulphaCtx.queryBuffer.size = 0x2000U;

	const NgsSulphaAgentsConfig *agentsCfg = ngsSulphaBuildAgentsConfig(config);
	int ret = sceSulphaAgentsRegister(agentsCfg, base + NGS_SULPHA_LOCAL_MEM_SIZE,
					  sizeInBytes - NGS_SULPHA_LOCAL_MEM_SIZE,
					  &g_ngsSulphaCtx.synthTraceHandle);
	if (ret < 0) {
		g_ngsSulphaState = -1;
		return ret;
	}

	ngsSetSulphaObjectNotify(ngsSulphaObjectNotifyHook);
	ngsSetSulphaApiTrace(ngsSulphaApiTraceHook);
	g_ngsSulphaState = 0;
	return ret;
}

int sceSulphaNgsShutdown(void)
{
	if (g_ngsSulphaCtx.synthTraceHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	for (;;) {
		if (g_ngsSulphaState < 0)
			return SCE_SULPHA_ERROR_UNINITIALIZED;
		if (ngsAtomicLoad32(&g_ngsSulphaState) == 0 &&
		    ngsAtomicStore32((SceUInt32)-3, &g_ngsSulphaState) == 0) {
			break;
		}
	}

	ngsSetSulphaApiTrace(NULL);
	ngsSetSulphaObjectNotify(NULL);

	for (;;) {
		NgsSulphaTrackedObject *root = ngsHashFindByParent(NULL);
		if (root == NULL)
			break;

		if (ngsSynthTraceIsSystem(g_ngsSulphaCtx.synthTraceHandle, root) < 0)
			ngsHashRemoveAndFree(root);
		else
			ngsSulphaUnregisterSystem(root->handle);
	}

	int ret = sceSulphaAgentsUnregister(&g_ngsSulphaCtx.synthTraceHandle, 6);
	ngsMemset(&g_ngsSulphaCtx, 0, sizeof(g_ngsSulphaCtx));
	g_ngsSulphaState = -1;
	return ret;
}

int sceSulphaNgsSetSynthName(SceNgsHSynSystem synthHandle, const char *name)
{
	if (g_ngsSulphaCtx.synthTraceHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;
	if ((synthHandle & NGS_HANDLE_SYSTEM_MAGIC) != NGS_HANDLE_SYSTEM_MAGIC)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	int ret = ngsSulphaEnter();
	if (ret < 0)
		return ret;

	NgsSulphaTrackedObject *obj = ngsHashLookup(synthHandle);
	ret = ngsSynthTraceSetName(g_ngsSulphaCtx.synthTraceHandle, obj, name);
	ngsSulphaLeave();
	return ret;
}

int sceSulphaNgsSetRackName(SceNgsHRack rackHandle, const char *name)
{
	if (g_ngsSulphaCtx.synthTraceHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;
	if ((rackHandle & NGS_HANDLE_RACK_MAGIC) != NGS_HANDLE_RACK_MAGIC)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	int ret = ngsSulphaEnter();
	if (ret < 0)
		return ret;

	NgsSulphaTrackedObject *obj = ngsHashLookup(rackHandle);
	ret = ngsSynthTraceSetName(g_ngsSulphaCtx.synthTraceHandle, obj, name);
	ngsSulphaLeave();
	return ret;
}

int sceSulphaNgsSetVoiceName(SceNgsHVoice voiceHandle, const char *name)
{
	if (g_ngsSulphaCtx.synthTraceHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;
	if ((voiceHandle & NGS_HANDLE_VOICE_MAGIC) != NGS_HANDLE_VOICE_MAGIC)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	int ret = ngsSulphaEnter();
	if (ret < 0)
		return ret;

	NgsSulphaTrackedObject *obj = ngsHashLookup(voiceHandle);
	ret = ngsSynthTraceSetName(g_ngsSulphaCtx.synthTraceHandle, obj, name);
	ngsSulphaLeave();
	return ret;
}

int sceSulphaNgsSetSampleName(const void *location, SceUInt32 length, const char *name)
{
	if (g_ngsSulphaCtx.namedObjectsHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;
	if (location == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (length == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;

	int ret = ngsSulphaEnter();
	if (ret < 0)
		return ret;

	ret = ngsNamedObjectsSetSample(g_ngsSulphaCtx.namedObjectsHandle, location, length, name);
	ngsSulphaLeave();
	return ret;
}

int sceSulphaNgsTrace(const char *message)
{
	if (g_ngsSulphaCtx.messageLogHandle == 0)
		return SCE_SULPHA_ERROR_UNINITIALIZED;
	if (message == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	int ret = ngsSulphaEnter();
	if (ret < 0)
		return ret;

	ret = ngsMessageLogReport(g_ngsSulphaCtx.messageLogHandle, 4, "Ngs", "%s", message);
	ngsSulphaLeave();
	return ret;
}
