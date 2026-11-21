/*
 * The tile backend's pools: the parameter the page's process names its memory by, and the pools
 * allocated under it, mapped for the GPU to read. Sony's file name is not known (the module has
 * no debug records); the names are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <gxm.h>
#include "tilebackend.h"

/* a pool: its memory block, where it is and its size */
struct TilebackendPool {
	TilebackendPool() : m_prev(NULL), m_next(NULL), m_uid(-1), m_base(NULL), m_size(0),
		m_reserved0(0), m_reserved1(0)
	{
	}

	TilebackendPool *m_prev;
	TilebackendPool *m_next;
	char m_name[64];
	SceUID m_uid;
	void *m_base;
	SceSize m_size;
	SceUInt32 m_reserved0;
	SceUInt32 m_reserved1;
};

/* the pools, by name */
class TilebackendPoolList {
public:
	TilebackendPoolList() : m_head(NULL), m_tail(NULL)
	{
	}

	TilebackendPool *Find(const char *name)
	{
		for (TilebackendPool *pool = m_head; pool != NULL; pool = pool->m_next) {
			if (sceClibStrncmp(name, pool->m_name, sizeof(pool->m_name)) == 0)
				return pool;
		}
		return NULL;
	}

	TilebackendPool *FindOrAdd(const char *name);

	void Remove(TilebackendPool *pool)
	{
		TilebackendPool *prev = pool->m_prev;
		TilebackendPool *next = pool->m_next;

		if (prev != NULL)
			prev->m_next = next;
		else
			m_head = next;
		if (next != NULL)
			next->m_prev = prev;
		else
			m_tail = prev;
	}

	TilebackendPool *m_head;
	TilebackendPool *m_tail;
};

static TilebackendParam s_param("", 0, SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW,
	TilebackendTileSize(256, 256));
static TilebackendPoolList s_pools;
/* a second list, which nothing uses */
static TilebackendPoolList s_unusedPools;

TilebackendParam::TilebackendParam(const char *name, SceSize poolSize,
	SceKernelMemBlockType poolType, TilebackendTileSize tileSize)
{
	char processId[9];
	char base[28];

	sceClibSnprintf(processId, sizeof(processId), "%08X", sceKernelGetProcessId());
	sceClibStrlcpy(base, name, sizeof(base));
	sceClibStrlcat(base, processId, sizeof(base));
	m_poolSize = poolSize;
	sceClibStrlcpy(m_poolName, base, sizeof(m_poolName));
	sceClibStrlcat(m_poolName, "Pool", sizeof(m_poolName));
	sceClibStrlcpy(m_serverName, base, sizeof(m_serverName));
	sceClibStrlcat(m_serverName, "Svr", sizeof(m_serverName));
	m_poolType = poolType;
	m_tileSize = tileSize;
}

TilebackendParam::~TilebackendParam()
{
}

void ScePafWebMapView_DE7C5CBE(const char *name, SceSize poolSize,
	SceKernelMemBlockType poolType, TilebackendTileSize tileSize)
{
	s_param = TilebackendParam(name, poolSize, poolType, tileSize);
}

TilebackendParam *ScePafWebMapView_C2581D7F(void)
{
	return &s_param;
}

/* the pool of the name, added at the end when there is none */
TilebackendPool *TilebackendPoolList::FindOrAdd(const char *name)
{
	TilebackendPool *pool = Find(name);

	if (pool == NULL) {
		pool = new TilebackendPool();
		sceClibStrlcpy(pool->m_name, name, sizeof(pool->m_name));

		TilebackendPool *tail = m_tail;

		if (tail != NULL)
			tail->m_next = pool;
		else
			m_head = pool;
		pool->m_prev = tail;
		m_tail = pool;
		pool->m_next = NULL;
	}
	return pool;
}

int ScePafWebMapView_6E0719FB(TilebackendParam *param)
{
	SceSize size = param->m_poolSize;
	SceKernelMemBlockType type = param->m_poolType;
	TilebackendPool *pool = s_pools.FindOrAdd(param->m_poolName);
	MemBlockUserOpt opt;
	SceUID uid;
	int error;

	pool->m_size = size;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
	opt.openFlags = MEMBLOCK_OPEN_RW;
	for (int i = 0; i < 8; i++)
		opt.capabilities[i] = 0;
	uid = sceKernelAllocMemBlock(param->m_poolName, type, size, (SceKernelAllocMemBlockOpt *)&opt);
	if (uid < 0)
		return uid;
	pool->m_uid = uid;
	error = sceKernelGetMemBlockBase(uid, &pool->m_base);
	if (error == 0)
		error = sceGxmMapMemory(pool->m_base, pool->m_size, SCE_GXM_MEMORY_ATTRIB_READ);
	if (error != 0) {
		s_pools.Remove(pool);
		delete pool;
		return error;
	}
	return 0;
}

int ScePafWebMapView_047488BB(TilebackendParam *param)
{
	TilebackendPool *pool = s_pools.Find(param->m_poolName);

	if (pool != NULL) {
		sceGxmUnmapMemory(pool->m_base);
		sceKernelFreeMemBlock(pool->m_uid);
		s_pools.Remove(pool);
		delete pool;
	}
	return 0;
}

void *GetTilebackendPoolBase(TilebackendParam *param)
{
	TilebackendPool *pool = s_pools.Find(param->m_poolName);

	if (pool == NULL)
		return NULL;
	return pool->m_base;
}
