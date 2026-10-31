#include <memory/range_allocator.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <kernel/sysmem/memblock.h>
#include <sysmem/memblock.h>

namespace paf {
namespace memory {

uint32_t RangeAllocator::GetType() const
{
	return 1;
}

AllocationBlockType RangeAllocator::GetBlockType() const
{
	return m_range_memblk_type;
}

const char *RangeAllocator::GetName() const
{
	return m_range_name;
}

void RangeAllocator::GetRange(void **membase, size_t *size) const
{
	*membase = m_range_addr;
	*size = m_range_size;
}

size_t RangeAllocator::GetFreeSize()
{
	return m_free_size;
}

/* Takes the first free chunk the aligned range fits in, splitting off what is left on either side. */
uintptr_t RangeAllocator::Alloc(int alignment, size_t size)
{
	m_mutex.Lock();
	if (alignment > 1 && (alignment & (alignment - 1)) == 0) {
		size_t length = (size + alignment - 1) & ~(alignment - 1);
		Chunk **link = &m_free_chunks;

		for (Chunk *chunk = m_free_chunks; chunk != NULL;
		     link = &chunk->next_free, chunk = chunk->next_free) {
			if (chunk->addr & CHUNK_USED)
				continue;

			uintptr_t start = chunk->addr & ~CHUNK_USED;
			Chunk *next = chunk->next;
			uintptr_t addr = (start + alignment - 1) & ~(alignment - 1);
			uintptr_t end = next->addr & ~CHUNK_USED;

			if (addr + length > end)
				continue;
			if (addr > start) {
				Chunk *nextFree = chunk->next_free;
				Chunk *head = (Chunk *)sce_paf_malloc(sizeof(Chunk));

				if (head != NULL) {
					head->next = next;
					head->next_free = nextFree;
					head->addr = addr;
					chunk->next = head;
					chunk->next_free = head;
				}
				link = &chunk->next_free;
				chunk = head;
				if (chunk == NULL)
					break;
			}
			if (addr + length < end) {
				Chunk *tailNext = chunk->next;
				uintptr_t tailAddr = (chunk->addr & ~CHUNK_USED) + length;
				Chunk *tailNextFree = chunk->next_free;
				Chunk *tail = (Chunk *)sce_paf_malloc(sizeof(Chunk));

				if (tail != NULL) {
					tail->next = tailNext;
					tail->next_free = tailNextFree;
					tail->addr = tailAddr;
					chunk->next = tail;
					chunk->next_free = tail;
				}
				if (tail == NULL)
					break;
			}
			*link = chunk->next_free;
			chunk->addr |= CHUNK_USED;
			chunk->next_free = NULL;
			m_free_size -= length;
			m_mutex.Unlock();
			return addr;
		}
	}
	m_mutex.Unlock();
	if (!m_range_disable_dbgprint)
		sce_paf_printf_dummy();
	return 0;
}

/* Puts the chunk at addr back on the free list and merges it with free neighbours. */
void RangeAllocator::Free(uintptr_t addr)
{
	m_mutex.Lock();

	Chunk *chunk = m_chunks;
	Chunk **link = &m_free_chunks;

	if (chunk == NULL) {
		m_mutex.Unlock();
		return;
	}

	Chunk *prev = NULL;

	for (Chunk *next = chunk->next; next != NULL; next = next->next) {
		uintptr_t start = chunk->addr & ~CHUNK_USED;

		if (addr < start)
			break;
		if (addr == start) {
			chunk->addr = start;
			chunk->next_free = *link;
			*link = chunk;
			next = chunk->next;
			m_free_size += (next->addr & ~CHUNK_USED) - start;
			if (prev != NULL && !(prev->addr & CHUNK_USED)) {
				Chunk *merged = prev->next;

				if (merged != NULL) {
					prev->next = merged->next;
					prev->next_free = merged->next_free;
					delete merged;
				}
				chunk = prev;
				next = prev->next;
			}
			if (next != NULL && !(next->addr & CHUNK_USED) && next->next != NULL) {
				chunk->next = next->next;
				chunk->next_free = next->next_free;
				delete next;
			}
			m_mutex.Unlock();
			return;
		}
		if (!(chunk->addr & CHUNK_USED))
			link = &chunk->next_free;
		prev = chunk;
		chunk = next;
	}
	m_mutex.Unlock();
}

void RangeAllocator::QueryFreeSize(size_t *size)
{
	m_mutex.Lock();
	*size = m_free_size;
	m_mutex.Unlock();
}

/* The release build prints nothing. */
void RangeAllocator::DumpInfo(int sort_policy)
{
	m_mutex.Lock();
	m_mutex.Unlock();
	sce_paf_printf_dummy();
}

int RangeAllocator::CheckMemoryLeaks()
{
	return 0;
}

RangeAllocator::RangeAllocator(void *membase, size_t size, const char *name, const Option *option)
	: m_range_addr(membase), m_range_size(size), m_free_size(size), m_memory_attached(false),
	  m_range_disable_dbgprint(false), m_range_disable_assert_on_alloc_failure(true),
	  m_mutex(name, 0)
{
	SceKernelMemBlockType type;

	m_range_memblk_type = AllocationBlockType_User;
	if (option != NULL && option->memblk_type != AllocationBlockType_User)
		m_range_memblk_type = option->memblk_type;
	switch (m_range_memblk_type) {
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
			m_range_disable_dbgprint = true;
		if (!option->disable_assert_on_alloc_failure)
			m_range_disable_assert_on_alloc_failure = false;
	}
	sce_paf_strncpy(m_range_name, name, sizeof(m_range_name) - 1);

	char *base = (char *)m_range_addr;
	size_t length = m_range_size;

	m_range_name[sizeof(m_range_name) - 1] = '\0';
	if (base != NULL) {
		m_memory_attached = true;
	} else {
		size_t padding = 0;

		if (length & 4095)
			padding = 4096 - (length & 4095);
		m_memblk_id = sceKernelAllocMemBlock(m_range_name, type, length + padding, NULL);

		void *block = NULL;

		sceKernelGetMemBlockBase(m_memblk_id, &block);
		base = (char *)block;
		length = m_range_size;
		m_range_addr = block;
		m_memory_attached = false;
	}

	/* The whole range as one free chunk, ended by a chunk at its end. */
	Chunk *last = (Chunk *)sce_paf_malloc(sizeof(Chunk));
	Chunk *first = NULL;

	if (last != NULL) {
		last->next = NULL;
		last->next_free = NULL;
		last->addr = (uintptr_t)(base + length);

		uintptr_t start = (uintptr_t)m_range_addr;

		first = (Chunk *)sce_paf_malloc(sizeof(Chunk));
		if (first != NULL) {
			first->next = last;
			first->next_free = NULL;
			first->addr = start;
		}
	}
	m_chunks = first;
	m_free_chunks = first;
}

RangeAllocator::~RangeAllocator()
{
	for (Chunk *chunk = m_chunks; chunk != NULL;) {
		Chunk *next = chunk->next;

		delete chunk;
		chunk = next;
	}
	if (m_range_addr != NULL && !m_memory_attached)
		sceKernelFreeMemBlock(m_memblk_id);
}

}
}
