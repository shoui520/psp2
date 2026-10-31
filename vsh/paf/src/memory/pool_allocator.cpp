#include <memory/pool_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>

namespace paf {
namespace memory {

uint32_t PoolAllocator::GetType() const
{
	return 2;
}

AllocationBlockType PoolAllocator::GetBlockType() const
{
	return m_pool_memblk_type;
}

const char *PoolAllocator::GetName() const
{
	return m_pool_name;
}

void PoolAllocator::GetRange(void **membase, size_t *size) const
{
	*membase = m_pool_addr;
	*size = m_pool_size;
}

size_t PoolAllocator::GetFreeSize()
{
	m_mutex.Lock();

	size_t size = m_block_size * m_num_free;

	m_mutex.Unlock();
	return size;
}

void *PoolAllocator::Alloc()
{
	m_mutex.Lock();

	FreeBlock *block = m_free_list;

	if (block != NULL) {
		m_num_free--;
		m_free_list = block->next;
	} else if (!m_pool_disable_dbgprint) {
		sce_paf_printf_dummy();
	}
	m_mutex.Unlock();
	return block;
}

void PoolAllocator::Free(void *ptr)
{
	m_mutex.Lock();
	if (ptr != NULL) {
		FreeBlock *block = (FreeBlock *)ptr;

		block->next = m_free_list;
		m_num_free++;
		m_free_list = block;
	}
	m_mutex.Unlock();
}

void PoolAllocator::QueryInfo(PoolInfo *info)
{
	m_mutex.Lock();
	info->block_size = m_block_size;
	info->num_blocks = m_num_blocks;
	info->num_free = m_num_free;
	m_mutex.Unlock();
}

/* The release build prints nothing. */
void PoolAllocator::DumpInfo(int sort_policy)
{
	m_mutex.Lock();
	m_mutex.Unlock();
	sce_paf_printf_dummy();
}

PoolAllocator::PoolAllocator(void *membase, size_t size, size_t blockSize, const char *name,
			     const Option *option)
	: m_pool_addr(membase), m_pool_size(size), m_block_size(blockSize), m_memory_attached(false),
	  m_pool_disable_dbgprint(false), m_pool_disable_assert_on_alloc_failure(true),
	  m_num_blocks(0), m_num_free(0), m_free_list(NULL), m_mutex(name, 0)
{
	SceKernelMemBlockType type;

	m_pool_memblk_type = AllocationBlockType_User;
	if (option != NULL && option->memblk_type != AllocationBlockType_User)
		m_pool_memblk_type = option->memblk_type;
	switch (m_pool_memblk_type) {
	case AllocationBlockType_User:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
		break;
	case AllocationBlockType_User_Cdlg:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
		break;
	default:
		type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW;
		break;
	}
	if (option != NULL) {
		if (option->disable_dbgprint)
			m_pool_disable_dbgprint = true;
		if (!option->disable_assert_on_alloc_failure)
			m_pool_disable_assert_on_alloc_failure = false;
	}
	if (blockSize & 15)
		blockSize = blockSize - (blockSize & 15) + 16;
	m_block_size = blockSize;
	m_num_free = m_num_blocks = m_pool_size / blockSize;
	sce_paf_strncpy(m_pool_name, name, sizeof(m_pool_name) - 1);
	m_pool_name[sizeof(m_pool_name) - 1] = '\0';
	if (m_pool_addr != NULL) {
		m_memory_attached = true;
	} else {
		size_t padding = 0;

		if (m_pool_size & 4095)
			padding = 4096 - (m_pool_size & 4095);
		m_memblk_id = sceKernelAllocMemBlock(m_pool_name, type, m_pool_size + padding, NULL);

		void *base = NULL;

		sceKernelGetMemBlockBase(m_memblk_id, &base);
		m_pool_addr = base;
		m_memory_attached = false;
	}
	for (uint32_t i = 0; i < m_num_blocks; i++) {
		char *blocks = (char *)m_pool_addr;
		FreeBlock *next = NULL;

		if (i + 1 < m_num_blocks)
			next = (FreeBlock *)(blocks + m_block_size + i * m_block_size);
		((FreeBlock *)(blocks + i * m_block_size))->next = next;
	}
	m_free_list = (FreeBlock *)m_pool_addr;
}

/* Blocks still handed out are reported, not freed. */
PoolAllocator::~PoolAllocator()
{
	if (m_num_free != m_num_blocks)
		sce_paf_printf_dummy();
	if (m_pool_addr != NULL && !m_memory_attached)
		sceKernelFreeMemBlock(m_memblk_id);
}

}
}
