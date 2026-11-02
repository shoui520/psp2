/*
 * paf::graph::SurfacePool: the memory the surfaces are made in, handed out by a heap or a range
 * allocator, and the list of every pool. A shared host pool writes the offset of each allocation
 * to a block named after the pool; the client pool opens that block and hands out the same
 * offsets in the same order.
 */
#include <stddef.h>
#include <stdint.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <std/list>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <thread/thread.h>
#include <src/paf_types.h>

/* the allocation offsets a shared pool can keep: one page of them */
#define SHARED_OFFSET_MAX	0x400
#define SHARED_OFFSET_SIZE	0x1000

namespace paf {
namespace graph {

size_t SurfacePool::GetFreeSize() const
{
	if (m_allocator != NULL)
		return m_allocator->GetFreeSize();

	return 0;
}

SurfacePool::SurfacePool(void *pool_addr, int32_t pool_size, const char *pool_name,
			 const memory::HeapAllocator::Option *option, MemoryType type)
	: m_allocator(NULL), m_addr(pool_addr), m_size(pool_size), m_name(NULL), m_type(type),
	  m_shared_uid(-1), m_shared_addr(NULL), m_count(0), m_mutex("ScePafSurfacePool", 0)
{
	if (m_type != MemoryType_SharedClient) {
		void *addr = (void *)(((uintptr_t)m_addr + 15) & ~15);
		size_t size = m_size - ((uintptr_t)addr - (uintptr_t)m_addr);

		m_allocator = new SurfacePoolAllocator_Heap(addr, size, pool_name, option);
		m_allocator->SetDisableAssertOnAllocFailure(true);
	}
	if (m_type != MemoryType_Normal) {
		char name[32];
		MemBlockUserOpt opt;

		sce_paf_snprintf(name, sizeof(name), "%sOffset", pool_name);
		if (m_type != MemoryType_SharedClient) {
			sce_paf_memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
			opt.openFlags = MEMBLOCK_OPEN_RO;
			for (int i = 0; i < 8; i++)
				opt.capabilities[i] = 0;
			m_shared_uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW,
							      SHARED_OFFSET_SIZE,
							      (SceKernelAllocMemBlockOpt *)&opt);
		} else {
			m_shared_uid = sceKernelOpenMemBlock(name, MEMBLOCK_OPEN_RO);
		}
		if (m_shared_uid >= 0 &&
		    sceKernelGetMemBlockBase(m_shared_uid, (void **)&m_shared_addr) == 0 &&
		    m_type == MemoryType_SharedHost)
			sce_paf_memset(m_shared_addr, 0xff, SHARED_OFFSET_SIZE);
	}

	size_t usage = 0;

	if (m_allocator != NULL) {
		size_t size = m_size;

		usage = size - m_allocator->GetFreeSize();
	}
	m_max_usage = usage;

	if (pool_name != NULL) {
		size_t len = sce_paf_strlen(pool_name) + 1;

		if (len > 64)
			len = 64;
		m_name = (char *)sce_paf_malloc(len);
		if (m_name != NULL)
			sce_paf_strlcpy(m_name, pool_name, len);
	} else {
		m_name = NULL;
	}

	m_list_mutex.Lock();
	m_pool_list.push_back(this);
	m_list_mutex.Unlock();
}

SurfacePool::SurfacePool(const Option *option)
	: m_allocator(NULL), m_addr(option->pool_addr), m_size(option->pool_size), m_name(NULL),
	  m_type(option->type), m_shared_uid(-1), m_shared_addr(NULL), m_count(0),
	  m_mutex("ScePafSurfacePool", 0)
{
	if (m_type != MemoryType_SharedClient) {
		void *addr = (void *)(((uintptr_t)m_addr + 15) & ~15);
		size_t size = m_size - ((uintptr_t)addr - (uintptr_t)m_addr);

		if (option->heap_allocator_option != NULL || option->fast_allocator_option == NULL)
			m_allocator = new SurfacePoolAllocator_Heap(addr, size, option->pool_name,
								    option->heap_allocator_option);
		else
			m_allocator = new SurfacePoolAllocator_Range(addr, size, option->pool_name,
								     option->fast_allocator_option);
		m_allocator->SetDisableAssertOnAllocFailure(true);
	}
	if (m_type != MemoryType_Normal) {
		char name[32];
		MemBlockUserOpt opt;

		sce_paf_snprintf(name, sizeof(name), "%sOffset", option->pool_name);
		if (m_type != MemoryType_SharedClient) {
			sce_paf_memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
			opt.openFlags = MEMBLOCK_OPEN_RO;
			for (int i = 0; i < 8; i++)
				opt.capabilities[i] = 0;
			m_shared_uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW,
							      SHARED_OFFSET_SIZE,
							      (SceKernelAllocMemBlockOpt *)&opt);
		} else {
			m_shared_uid = sceKernelOpenMemBlock(name, MEMBLOCK_OPEN_RO);
		}
		if (m_shared_uid >= 0 &&
		    sceKernelGetMemBlockBase(m_shared_uid, (void **)&m_shared_addr) == 0 &&
		    m_type == MemoryType_SharedHost)
			sce_paf_memset(m_shared_addr, 0xff, SHARED_OFFSET_SIZE);
	}

	size_t usage = 0;

	if (m_allocator != NULL) {
		size_t size = m_size;

		usage = size - m_allocator->GetFreeSize();
	}
	m_max_usage = usage;

	if (option->pool_name != NULL) {
		size_t len = sce_paf_strlen(option->pool_name) + 1;

		if (len > 64)
			len = 64;
		m_name = (char *)sce_paf_malloc(len);
		if (m_name != NULL)
			sce_paf_strlcpy(m_name, option->pool_name, len);
	} else {
		m_name = NULL;
	}

	m_list_mutex.Lock();
	m_pool_list.push_back(this);
	m_list_mutex.Unlock();
}

SurfacePool::~SurfacePool()
{
	Surface::DeactivateSurfacePool(this);
	SurfaceCLUT::DeactivateSurfacePool(this);
	if (m_allocator != NULL) {
		delete m_allocator;
		m_allocator = NULL;
	}
	if (m_shared_uid >= 0) {
		if (m_type == MemoryType_SharedClient)
			sceKernelCloseMemBlock(m_shared_uid);
		else
			sceKernelFreeMemBlock(m_shared_uid);
		m_shared_uid = -1;
	}
	if (m_name != NULL)
		sce_paf_free(m_name);

	m_list_mutex.Lock();
	m_pool_list.remove(this);
	m_list_mutex.Unlock();
}

void *SurfacePool::Allocate(int32_t align_bytes, int32_t size)
{
	thread::MutexLock lock(&m_mutex);
	void *ptr = NULL;

	if (m_allocator != NULL) {
		if (align_bytes < 16)
			align_bytes = 16;
		ptr = m_allocator->Allocate(align_bytes, size);
		if (ptr != NULL) {
			if (m_shared_addr != NULL && m_count < SHARED_OFFSET_MAX &&
			    m_shared_addr[m_count] == -1)
				m_shared_addr[m_count] = (uintptr_t)ptr - (uintptr_t)m_addr;
			m_count++;
		}

		size_t usage = m_size - GetFreeSize();

		if (usage > m_max_usage)
			m_max_usage = usage;
	} else if (m_shared_addr != NULL && m_count < SHARED_OFFSET_MAX) {
		int32_t offset = m_shared_addr[m_count];

		if (offset != -1) {
			m_count++;
			ptr = (char *)m_addr + offset;
		}
	}

	return ptr;
}

void SurfacePool::Free(void *ptr)
{
	thread::MutexLock lock(&m_mutex);

	if (m_allocator != NULL)
		m_allocator->Free(ptr);
	m_count--;
}

SurfacePool *SurfacePool::GetPool(int32_t index)
{
	m_list_mutex.Lock();
	for (list<SurfacePool *>::iterator it = m_pool_list.begin(); it != m_pool_list.end();
	     ++it) {
		if (index <= 0) {
			SurfacePool *pool = *it;

			m_list_mutex.Unlock();
			return pool;
		}
		index--;
	}
	m_list_mutex.Unlock();

	return NULL;
}

int32_t SurfacePool::GetIndex(SurfacePool *pool)
{
	int32_t index = 0;

	m_list_mutex.Lock();
	for (list<SurfacePool *>::iterator it = m_pool_list.begin(); it != m_pool_list.end();
	     ++it) {
		if (*it == pool) {
			m_list_mutex.Unlock();
			return index;
		}
		index++;
	}
	m_list_mutex.Unlock();

	return -1;
}

SurfacePool *SurfacePool::FindPool(const char *name)
{
	m_list_mutex.Lock();
	for (list<SurfacePool *>::iterator it = m_pool_list.begin(); it != m_pool_list.end();
	     ++it) {
		if ((*it)->m_name != NULL && sce_paf_strcmp((*it)->m_name, name) == 0) {
			SurfacePool *pool = *it;

			m_list_mutex.Unlock();
			return pool;
		}
	}
	m_list_mutex.Unlock();

	return NULL;
}

void SurfacePool::SetDisableAssertOnAllocFailure(bool disable)
{
	if (m_allocator != NULL)
		m_allocator->SetDisableAssertOnAllocFailure(disable);
}

list<SurfacePool *> SurfacePool::m_pool_list;
thread::Mutex SurfacePool::m_list_mutex("ScePafSurfacePoolList", 0);

}
}

using namespace paf;
using namespace paf::graph;

int32_t ScePafGraphics_404AC3D7(SurfacePool *pool)
{
	if (pool->m_allocator != NULL)
		return pool->m_allocator->IsDbgPrintEnabled();

	return true;
}
