#include "be_mp2sys_internal.h"

MemBlockPool::MemBlockPool()
	: m_freeList(0),
	  m_usedList(0),
	  m_totalCount(0),
	  m_usedCount(0),
	  m_growCount(0),
	  m_blockSize(0),
	  m_initialCount(0),
	  m_maxFreeKeep(0)
{
	m_mutex.Initialize(0);
}

int MemBlockPool::freeListNodes(MemBlockNode *head)
{
	while (head != 0) {
		if (head->buffer != 0)
			delete[] head->buffer;
		MemBlockNode *next = head->next;
		delete head;
		head = next;
	}
	return 1;
}

int MemBlockPool::Destroy()
{
	m_mutex.Lock(0);
	freeListNodes(m_freeList);
	freeListNodes(m_usedList);
	m_freeList = 0;
	m_usedList = 0;
	m_initialCount = 0;
	m_maxFreeKeep = 0;
	m_growCount = 0;
	m_blockSize = 0;
	m_totalCount = 0;
	m_usedCount = 0;
	m_mutex.Unlock();
	return 1;
}

MemBlockPool::~MemBlockPool()
{
	Destroy();
	m_mutex.Release();
}

MemBlockNode *MemBlockPool::allocateNodes(int32_t count, int32_t blockSize)
{
	MemBlockNode *prev = 0;
	MemBlockNode *node = 0;

	for (int32_t i = 0; i < count; ++i) {
		node = new MemBlockNode;
		if (node == 0) {
			freeListNodes(prev);
			return 0;
		}
		node->next = prev;
		node->buffer = new uint8_t[blockSize];
		if (node->buffer == 0) {
			freeListNodes(node);
			return 0;
		}
		prev = node;
	}
	return node;
}

int MemBlockPool::Create(int32_t blockSize, int32_t initialCount, int32_t growCount, int32_t maxFreeKeep)
{
	if (m_freeList != 0 || m_usedList != 0) {
		beLogPrintf(2, "[DEBUG] %s : Maybe already created.\n", "Create");
		return 0;
	}

	MemBlockNode *list = allocateNodes(initialCount, blockSize);
	if (list == 0)
		return 0;

	m_mutex.Lock(0);
	m_freeList = list;
	m_blockSize = blockSize;
	m_usedList = 0;
	m_initialCount = initialCount;
	m_totalCount = initialCount;
	m_usedCount = 0;
	m_growCount = growCount;
	m_maxFreeKeep = maxFreeKeep;
	m_mutex.Unlock();
	return 1;
}

int MemBlockPool::DestroyIfEmpty()
{
	if (m_usedCount > 0)
		return 0;
	Destroy();
	return 1;
}

MemBlockNode *MemBlockPool::GetBlock()
{
	m_mutex.Lock(0);
	MemBlockNode *block = m_freeList;
	if (block == 0) {
		block = allocateNodes(m_growCount, m_blockSize);
		if (block == 0) {
			m_freeList = 0;
			m_mutex.Unlock();
			return 0;
		}
		m_totalCount += m_growCount;
	}

	m_freeList = block->next;
	block->next = m_usedList;
	m_usedList = block;
	m_usedCount += 1;
	m_mutex.Unlock();
	return block;
}

int MemBlockPool::trimExcessFreeBlocks()
{
	int32_t freeCount = m_totalCount - m_usedCount;
	int32_t excess = freeCount - m_maxFreeKeep;
	if (m_usedCount + m_maxFreeKeep > m_initialCount) {
		MemBlockNode *cur = m_freeList;
		while (excess > 0) {
			MemBlockNode *next = cur->next;
			delete[] cur->buffer;
			delete cur;
			m_freeList = next;
			--m_totalCount;
			--excess;
			cur = next;
		}
	}
	return m_totalCount;
}

int MemBlockPool::ReturnBlock(MemBlockNode *block)
{
	MemBlockNode *cur = m_usedList;
	MemBlockNode *prev = 0;
	m_mutex.Lock(0);

	while (cur != 0) {
		if (cur == block) {
			if (prev != 0)
				prev->next = cur->next;
			else
				m_usedList = cur->next;

			cur->next = m_freeList;
			m_freeList = cur;
			--m_usedCount;

			if (m_totalCount - m_usedCount > m_maxFreeKeep)
				trimExcessFreeBlocks();

			m_mutex.Unlock();
			return 1;
		}
		prev = cur;
		cur = cur->next;
	}

	m_mutex.Unlock();
	return 0;
}

int MemBlockPool::IsValidBlock(MemBlockNode *block)
{
	m_mutex.Lock(0);
	for (MemBlockNode *cur = m_usedList; cur != 0; cur = cur->next) {
		if (cur == block) {
			m_mutex.Unlock();
			return 1;
		}
	}
	m_mutex.Unlock();
	return 0;
}

uint8_t *MemBlockPool::GetBlockData(MemBlockNode *block)
{
	m_mutex.Lock(0);
	for (MemBlockNode *cur = m_usedList; cur != 0; cur = cur->next) {
		if (cur == block) {
			m_mutex.Unlock();
			return block->buffer;
		}
	}
	m_mutex.Unlock();
	return 0;
}
