/*
 * SceSulpha public API entry points.
 */

#include "sulpha_internal.h"

volatile SceUInt32 g_sulphaNextNodeId = 1;
volatile SceInt32 g_sulphaInitRefCount = -1;
volatile SceUInt32 g_sulphaUpdateLock = 0;
SulphaServer *g_sulphaServer = NULL;

static inline __attribute__((always_inline)) SceBool sulphaAcquireRef(void)
{
	for (;;) {
		SceInt32 cur = g_sulphaInitRefCount;
		if (cur < 0)
			return false;
		while ((SceInt32)sulphaAtomicLoad32(&g_sulphaInitRefCount) == cur) {
			if (sulphaAtomicStore32((SceUInt32)(cur + 1), &g_sulphaInitRefCount) == 0)
				return true;
		}
	}
}

static void sulphaReleaseRef(void)
{
	sulphaAtomicDec32(&g_sulphaInitRefCount);
}

int sceSulphaNetworkInit(void)
{
	return sulphaDeci4pInit();
}

int sceSulphaNetworkShutdown(void)
{
	return sulphaDeci4pShutdown();
}

int sceSulphaGetDefaultConfig(SceSulphaConfig *config)
{
	if (config == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	config->notifyCallback = NULL;
	config->port = SULPHA_DEFAULT_PORT;
	config->bookmarkCount = SULPHA_DEFAULT_BOOKMARKS;
	return 0;
}

int sceSulphaGetNeededMemory(const SceSulphaConfig *config, SceUInt32 *sizeInBytes)
{
	if (config == NULL || sizeInBytes == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	return sulphaServerGetNeededMemory(config, sizeInBytes);
}

int sceSulphaInit(const SceSulphaConfig *config, void *buffer, SceUInt32 sizeInBytes)
{
	SceUInt32 neededSize = 0;
	SulphaAllocator alloc;
	SceInt32 res;

	if (config == NULL || buffer == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (sizeInBytes == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (g_sulphaServer != NULL)
		return SCE_SULPHA_ERROR_ALREADY_INITIALIZED;

	do {
		if ((SceInt32)sulphaAtomicLoad32(&g_sulphaInitRefCount) != -1)
			return SCE_SULPHA_ERROR_ALREADY_INITIALIZED;
	} while (sulphaAtomicStore32((SceUInt32)-2, &g_sulphaInitRefCount) != 0);

	res = sulphaServerGetNeededMemory(config, &neededSize);
	if (res < 0) {
		g_sulphaInitRefCount = -1;
		return res;
	}

	if (sizeInBytes > neededSize)
		return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

	SceUIntPtr base = (SceUIntPtr)buffer;
	alloc.magic = SULPHA_ALLOC_MAGIC_LIVE;
	alloc.totalSize = sizeInBytes;
	alloc.current = base + sizeof(SulphaServer);
	alloc.end = base + sizeInBytes;
	alloc.base = base;
	alloc.alloc = sulphaBumpAlloc;

	SulphaServer *alignedServer = (SulphaServer *)((base + 3U) & ~3U);
	res = sulphaServerInit(alignedServer, config, &alloc);
	if (res < 0) {
		g_sulphaInitRefCount = -1;
		return res;
	}

	g_sulphaInitRefCount = 0;
	g_sulphaServer = alignedServer;
	return res;
}

int sceSulphaShutdown(void)
{
	while (g_sulphaInitRefCount >= 0) {
		while ((SceInt32)sulphaAtomicLoad32(&g_sulphaInitRefCount) == 0) {
			if (sulphaAtomicStore32((SceUInt32)-3, &g_sulphaInitRefCount) == 0) {
				if (g_sulphaServer == NULL)
					return SCE_SULPHA_ERROR_UNINITIALIZED;
				SceInt32 res = sulphaServerShutdown(g_sulphaServer);
				g_sulphaServer = NULL;
				g_sulphaInitRefCount = -1;
				return res;
			}
		}
	}
	return SCE_SULPHA_ERROR_UNINITIALIZED;
}

int sceSulphaUpdate(void)
{
	do {
		if (sulphaAtomicLoad32(&g_sulphaUpdateLock) != 0)
			return 0;
	} while (sulphaAtomicStore32(1, &g_sulphaUpdateLock) != 0);

	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaServerUpdate(g_sulphaServer);
	g_sulphaUpdateLock = 0;
	sulphaReleaseRef();
	return res;
}

int sceSulphaFileConnect(const char *filename)
{
	SceBool connected = false;
	SceInt32 res;

	if (filename == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	sulphaServerIsFileConnected(g_sulphaServer, &connected);
	if (connected)
		res = SCE_SULPHA_ERROR_CAPTURE_STARTED;
	else
		res = sulphaServerFileConnect(g_sulphaServer, filename);

	sulphaReleaseRef();
	return res;
}

int sceSulphaFileDisconnect(void)
{
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaServerFileDisconnect(g_sulphaServer);
	sulphaReleaseRef();
	return res;
}

int sceSulphaAgentsGetNeededMemory(const SulphaAgentsConfig *config, SceUInt32 *sizeInBytes)
{
	if (config == NULL || sizeInBytes == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (config->count > SULPHA_MAX_AGENTS)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (config->agents == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaServerAgentsGetNeededMemory(g_sulphaServer, config, sizeInBytes);
	sulphaReleaseRef();
	return res;
}

int sceSulphaAgentsRegister(const SulphaAgentsConfig *config, void *buffer,
			    SceUInt32 sizeInBytes, SceUInt64 *handles)
{
	SceUInt32 neededSize = 0;
	SulphaAllocator alloc;
	SceInt32 res;

	if (config == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (sizeInBytes == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (buffer == NULL || handles == NULL || buffer == g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_POINTER;

	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	res = sceSulphaAgentsGetNeededMemory(config, &neededSize);
	if (res >= 0) {
		if (sizeInBytes > neededSize)
			return SCE_SULPHA_ERROR_OUT_OF_MEMORY;

		SceUIntPtr base = (SceUIntPtr)buffer;
		alloc.magic = SULPHA_ALLOC_MAGIC_LIVE;
		alloc.totalSize = sizeInBytes;
		alloc.current = base;
		alloc.end = base + sizeInBytes;
		alloc.base = base;
		alloc.alloc = sulphaBumpAlloc;

		res = sulphaServerAgentsRegister(g_sulphaServer, config, &alloc, handles);
	}

	sulphaReleaseRef();
	return res;
}

int sceSulphaAgentsUnregister(const SceUInt64 *handles, SceUInt32 count)
{
	if (handles == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaServerAgentsUnregister(g_sulphaServer, handles, count);
	sulphaReleaseRef();
	return res;
}

int sceSulphaSetBookmark(const SceSulphaUTF8 *name, SceInt32 id)
{
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaServerSetBookmark(g_sulphaServer, name, id);
	sulphaReleaseRef();
	return res;
}

int sceSulphaGetAgent(SceUInt64 handle, SulphaAgent **outAgent, void **outUserData)
{
	SceUInt32 typeId = (SceUInt32)(handle >> 8);
	SceUInt8 slotIndex;

	if (outAgent == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (handle == 0)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	slotIndex = (SceUInt8)((SceUInt8)handle - 1U);
	if (slotIndex >= SULPHA_MAX_AGENTS - 1)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;

	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SulphaAgent *agent = &g_sulphaServer->agents[slotIndex];
	SceInt32 res;
	if (agent->def != NULL && agent->def->typeId == typeId) {
		*outAgent = agent;
		if (outUserData != NULL)
			*outUserData = agent->userData;
		res = 0;
	} else {
		*outAgent = NULL;
		if (outUserData != NULL)
			*outUserData = NULL;
		res = SCE_SULPHA_ERROR_INVALID_VALUE;
	}

	sulphaReleaseRef();
	return res;
}

int sceSulphaNodeNew(SulphaNode **outNode, SulphaAgent *agent, const SulphaNode *parent)
{
	if (outNode == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (agent == NULL || agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentNodeNew(outNode, agent, parent);
	sulphaReleaseRef();
	return res;
}

int sceSulphaNodeDelete(SulphaNode *node, SceUInt8 eventId)
{
	if (node == NULL || node->nodeId == 0 || node->agent == NULL ||
	    node->agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentNodeDelete(node->agent, node, eventId);
	sulphaReleaseRef();
	return res;
}

int sceSulphaEventNew(SulphaEvent **outEvent, SulphaAgent *agent, SceUInt8 eventId)
{
	if (outEvent == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (eventId == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (agent == NULL || agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentEventNew(agent, outEvent, eventId);
	sulphaReleaseRef();
	return res;
}

int sceSulphaEventDelete(SulphaEvent *event)
{
	if (event == NULL || event->payload == NULL || event->agent == NULL ||
	    event->agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentEventDelete(event->agent, event, false);
	sulphaReleaseRef();
	return res;
}

int sceSulphaEventAdd(SulphaEvent *event, SceUInt8 fieldId, const void *data, SceUInt32 size)
{
	if (event == NULL || event->payload == NULL || event->agent == NULL ||
	    event->agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (data == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (fieldId == 0)
		return SCE_SULPHA_ERROR_INVALID_VALUE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentEventAdd(event, fieldId, data, size);
	sulphaReleaseRef();
	return res;
}

int sceSulphaEventReport(SulphaEvent *event, const SulphaNode *node)
{
	if (event == NULL || event->payload == NULL || event->agent == NULL ||
	    event->agent->server != g_sulphaServer)
		return SCE_SULPHA_ERROR_INVALID_HANDLE;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	SceInt32 res = sulphaAgentEventReport(event->agent, event, node);
	sulphaReleaseRef();
	return res;
}

int sceSulphaGetTimestamp(SceUInt64 *timestamp)
{
	if (timestamp == NULL)
		return SCE_SULPHA_ERROR_INVALID_POINTER;
	if (!sulphaAcquireRef())
		return SCE_SULPHA_ERROR_UNINITIALIZED;

	*timestamp = sulphaTimerGetTimestamp(&g_sulphaServer->timer);
	sulphaReleaseRef();
	return 0;
}
