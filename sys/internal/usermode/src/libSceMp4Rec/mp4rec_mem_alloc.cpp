/*
 * SceLibMp4Recorder memory allocator implementations.
 */
#include "mp4rec_internal.h"

MspaceMemAllocator::MspaceMemAllocator()
{
	m_initialized = 0;
	m_phyMemBase = NULL;
	m_phyMemSize = 0;
	m_heapSize = 0;
	m_mspace = NULL;
	m_heapBase = NULL;
}

MspaceMemAllocator::~MspaceMemAllocator()
{
}

int32_t MspaceMemAllocator::Reset()
{
	return 0;
}

int32_t MspaceMemAllocator::Init(size_t heapSize, void *heapBase, uint32_t phySize, void *phyBase)
{
	m_heapSize = heapSize;
	m_heapBase = heapBase;
	m_phyMemSize = phySize;
	m_phyMemBase = phyBase;
	m_mspace = mspace_create(heapBase, heapSize);
	if (m_mspace == NULL)
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	m_initialized = 1;
	return 0;
}

int32_t MspaceMemAllocator::Term()
{
	if (m_mspace != NULL) {
		mspace_destroy(m_mspace);
		m_mspace = NULL;
	}
	m_initialized = 0;
	return 0;
}

void *MspaceMemAllocator::Malloc(size_t size)
{
	if (m_initialized != 0)
		return mspace_malloc(m_mspace, size);
	return NULL;
}

void *MspaceMemAllocator::Memalign(size_t align, size_t size)
{
	if (m_initialized != 0)
		return mspace_memalign(m_mspace, align, size);
	return NULL;
}

void MspaceMemAllocator::Free(void *ptr)
{
	if (m_initialized != 0)
		mspace_free(m_mspace, ptr);
}

size_t MspaceMemAllocator::GetMaxUsed()
{
	SceLibcMallocManagedSize stats;
	mspace_malloc_stats(m_mspace, &stats);
	return stats.currentSystemSize;
}

ExternalMemAllocator::ExternalMemAllocator()
{
	m_initialized = 0;
	m_phyMemBase = NULL;
	m_allocIf = NULL;
	m_phyMemSize = 0;
}

ExternalMemAllocator::~ExternalMemAllocator()
{
}

int32_t ExternalMemAllocator::Reset()
{
	return 0;
}

int32_t ExternalMemAllocator::Init(AllocatorInterface *allocIf, uint32_t phySize, void *phyBase)
{
	if (allocIf == NULL)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	m_allocIf = allocIf;
	m_phyMemSize = phySize;
	m_phyMemBase = phyBase;
	m_initialized = 1;
	return 0;
}

int32_t ExternalMemAllocator::Term()
{
	m_initialized = 0;
	return 0;
}

void *ExternalMemAllocator::Malloc(size_t size)
{
	if (m_initialized != 0)
		return m_allocIf->Malloc(size);
	return NULL;
}

void *ExternalMemAllocator::Memalign(size_t align, size_t size)
{
	if (m_initialized != 0)
		return m_allocIf->Memalign(align, size);
	return NULL;
}

void ExternalMemAllocator::Free(void *ptr)
{
	if (m_initialized != 0)
		m_allocIf->Free(ptr);
}

size_t ExternalMemAllocator::GetMaxUsed()
{
	return 0;
}

AllocatorInterface *ExternalMemAllocator::GetInterface()
{
	if (m_initialized != 0)
		return m_allocIf;
	return NULL;
}
