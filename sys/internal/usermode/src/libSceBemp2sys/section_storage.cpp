#include "be_mp2sys_internal.h"

SectionStorage::SectionStorage()
{
	m_tables[1].sections = 0;
	m_currentIndex = 0;
	m_demuxer = 0;
	m_memPool = 0;
	m_tables[0].version = -1;
	m_tables[0].sectionCount = 0;
	m_tables[0].capacity = 0;
	m_tables[0].sections = 0;
	m_tables[1].version = -1;
	m_tables[1].sectionCount = 0;
	m_tables[1].capacity = 0;
}

int SectionStorage::releaseTableBlocks(SectionTableVersion *table, uint8_t keepArray)
{
	for (uint32_t i = 0; i < table->sectionCount; ++i) {
		m_memPool->ReturnBlock(table->sections[i].block);
		table->sections[i].length = 0;
		table->sections[i].hasCrc = 0;
		table->sections[i].valid = 0;
		table->sections[i].block = 0;
	}
	if (!keepArray) {
		delete[] table->sections;
		table->sections = 0;
	}
	return 1;
}

int SectionStorage::Clear()
{
	for (int idx = 0; idx < 2; ++idx) {
		if (m_tables[idx].sections != 0)
			releaseTableBlocks(&m_tables[idx], 0);
		m_tables[idx].version = -1;
		m_tables[idx].sectionCount = 0;
		m_tables[idx].capacity = 0;
		m_tables[idx].sections = 0;
	}
	m_demuxer = 0;
	m_memPool = 0;
	return 1;
}

SectionStorage::~SectionStorage()
{
	Clear();
}

int SectionStorage::Init(TsDemuxer *demuxer)
{
	if (m_demuxer != 0)
		return 0;

	m_currentIndex = 0;
	m_demuxer = demuxer;
	m_memPool = &demuxer->m_memBlockPool;
	m_tables[0].version = -1;
	m_tables[0].sectionCount = 0;
	m_tables[0].capacity = 0;
	m_tables[0].sections = 0;
	m_tables[1].version = -1;
	m_tables[1].sectionCount = 0;
	m_tables[1].capacity = 0;
	m_tables[1].sections = 0;
	return 1;
}

int SectionStorage::MakeSectionArray(SectionTableVersion *table, int32_t count)
{
	SectionSlot *arr = new SectionSlot[count];
	table->sections = arr;
	if (arr == 0) {
		table->version = -1;
		table->sectionCount = 0;
		table->capacity = 0;
		table->sections = 0;
		beLogPrintf(2, "[DEBUG] %s : Malloc fail!\n", "MakeSectionArray");
		return 0;
	}

	for (int32_t i = 0; i < count; ++i) {
		arr[i].hasCrc = 0;
		arr[i].valid = 0;
		arr[i].length = 0;
		arr[i].block = 0;
	}
	return 1;
}

int SectionStorage::SetSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
	const void *buf, int32_t length, uint8_t hasCrc)
{
	if (length > m_memPool->m_blockSize) {
		beLogPrintf(2, "[DEBUG] %s : length[%d] is over [%d]\n", "SetSection", length, m_memPool->m_blockSize);
		return 0;
	}

	uint32_t totalSections = (uint32_t)(lastSectionNum + 1);
	SectionTableVersion *target = &m_tables[m_currentIndex];
	if (target->version != version) {
		target = &m_tables[(m_currentIndex + 1) % 2];
		if (target->version != version) {
			if (target->sections != 0) {
				releaseTableBlocks(target, 1);
				if (target->capacity < totalSections) {
					delete[] target->sections;
					if (!MakeSectionArray(target, (int32_t)totalSections))
						return 0;
					target->capacity = totalSections;
				}
			} else {
				if (!MakeSectionArray(target, (int32_t)totalSections))
					return 0;
				target->capacity = totalSections;
			}
			target->version = version;
			target->sectionCount = totalSections;
		}
	}

	if (target->sectionCount != totalSections) {
		beLogPrintf(2, "[DEBUG] %s : last section number mismatch!\n", "SetSection");
		return 0;
	}

	SectionSlot *slot = &target->sections[sectionNum];
	if (slot->block == 0) {
		slot->valid = 0;
		slot->hasCrc = 0;
		slot->length = 0;
		slot->block = 0;
		slot->block = m_memPool->GetBlock();
		if (slot->block == 0) {
			beLogPrintf(2, "[DEBUG] %s : Get Block fails!\n", "SetSection");
			return 0;
		}
	}

	uint8_t *dst = m_memPool->GetBlockData(slot->block);
	beMemcpy(dst, buf, length);
	slot->length = length;
	slot->hasCrc = hasCrc;
	slot->valid = 1;
	return 1;
}

int SectionStorage::SetSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
	MemBlockNode *block, int32_t length, uint8_t hasCrc)
{
	if (!m_memPool->IsValidBlock(block)) {
		beLogPrintf(2, "[DEBUG] %s : Invalid MemBlock!\n", "SetSection");
		return 0;
	}

	uint32_t totalSections = (uint32_t)(lastSectionNum + 1);
	SectionTableVersion *target = &m_tables[m_currentIndex];
	if (target->version != version) {
		target = &m_tables[(m_currentIndex + 1) % 2];
		if (target->version != version) {
			if (target->sections != 0) {
				releaseTableBlocks(target, 1);
				if (target->capacity < totalSections) {
					delete[] target->sections;
					if (!MakeSectionArray(target, (int32_t)totalSections))
						return 0;
					target->capacity = totalSections;
				}
			} else {
				if (!MakeSectionArray(target, (int32_t)totalSections))
					return 0;
				target->capacity = totalSections;
			}
			target->version = version;
			target->sectionCount = totalSections;
		}
	}

	if (target->sectionCount != totalSections) {
		beLogPrintf(2, "[DEBUG] %s : last section number mismatch!\n", "SetSection");
		return 0;
	}

	SectionSlot *slot = &target->sections[sectionNum];
	if (slot->block != 0 && slot->block != block)
		m_memPool->ReturnBlock(slot->block);

	slot->block = block;
	slot->length = length;
	slot->hasCrc = hasCrc;
	slot->valid = 1;
	return 1;
}

int SectionStorage::AccumulateSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
	const void *buf, int32_t length, uint8_t hasCrc)
{
	if (length > m_memPool->m_blockSize) {
		beLogPrintf(2, "[DEBUG] %s : length[%d] is over [%d]\n", "AccumulateSection", length, m_memPool->m_blockSize);
		return 0;
	}

	uint32_t totalSections = (uint32_t)(lastSectionNum + 1);
	SectionTableVersion *target = &m_tables[(m_currentIndex + 1) % 2];
	if (target->version != version) {
		if (target->sections != 0) {
			releaseTableBlocks(target, 1);
			if (target->capacity < totalSections) {
				delete[] target->sections;
				if (!MakeSectionArray(target, (int32_t)totalSections))
					return 0;
				target->capacity = totalSections;
			}
		} else {
			if (!MakeSectionArray(target, (int32_t)totalSections))
				return 0;
			target->capacity = totalSections;
		}
		target->version = version;
		target->sectionCount = totalSections;
	} else if (target->sections == 0) {
		if (!MakeSectionArray(target, (int32_t)totalSections))
			return 0;
		target->sectionCount = totalSections;
		target->capacity = totalSections;
	} else if (target->sectionCount != totalSections) {
		releaseTableBlocks(target, 1);
		if (target->capacity < totalSections) {
			delete[] target->sections;
			if (!MakeSectionArray(target, (int32_t)totalSections))
				return 0;
			target->capacity = totalSections;
		}
		target->sectionCount = totalSections;
	}

	if (target->sectionCount != totalSections) {
		beLogPrintf(2, "[DEBUG] %s : last section number mismatch!\n", "AccumulateSection");
		return 0;
	}

	SectionSlot *slot = &target->sections[sectionNum];
	if (slot->block == 0) {
		slot->valid = 0;
		slot->hasCrc = 0;
		slot->length = 0;
		slot->block = 0;
		slot->block = m_memPool->GetBlock();
		if (slot->block == 0) {
			beLogPrintf(2, "[DEBUG] %s : Get Block fails!\n", "AccumulateSection");
			return 0;
		}
	}

	uint8_t *dst = m_memPool->GetBlockData(slot->block);
	beMemcpy(dst, buf, length);
	slot->length = length;
	slot->hasCrc = hasCrc;
	slot->valid = 1;
	return 1;
}

int SectionStorage::AccumulateSection(int32_t version, int32_t sectionNum, int32_t lastSectionNum,
	MemBlockNode *block, int32_t length, uint8_t hasCrc)
{
	if (!m_memPool->IsValidBlock(block)) {
		beLogPrintf(2, "[DEBUG] %s : Invalid MemBlock!\n", "AccumulateSection");
		return 0;
	}

	uint32_t totalSections = (uint32_t)(lastSectionNum + 1);
	SectionTableVersion *target = &m_tables[(m_currentIndex + 1) % 2];
	if (target->version != version) {
		if (target->sections != 0) {
			releaseTableBlocks(target, 1);
			if (target->capacity < totalSections) {
				delete[] target->sections;
				if (!MakeSectionArray(target, (int32_t)totalSections))
					return 0;
				target->capacity = totalSections;
			}
		} else {
			if (!MakeSectionArray(target, (int32_t)totalSections))
				return 0;
			target->capacity = totalSections;
		}
		target->version = version;
		target->sectionCount = totalSections;
	} else if (target->sections == 0) {
		if (!MakeSectionArray(target, (int32_t)totalSections))
			return 0;
		target->sectionCount = totalSections;
		target->capacity = totalSections;
	} else if (target->sectionCount != totalSections) {
		releaseTableBlocks(target, 1);
		if (target->capacity < totalSections) {
			delete[] target->sections;
			if (!MakeSectionArray(target, (int32_t)totalSections))
				return 0;
			target->capacity = totalSections;
		}
		target->sectionCount = totalSections;
	}

	if (target->sectionCount != totalSections) {
		beLogPrintf(2, "[DEBUG] %s : last section number mismatch!\n", "AccumulateSection");
		return 0;
	}

	SectionSlot *slot = &target->sections[sectionNum];
	if (slot->block != 0 && slot->block != block)
		m_memPool->ReturnBlock(slot->block);

	slot->block = block;
	slot->length = length;
	slot->hasCrc = hasCrc;
	slot->valid = 1;
	return 1;
}

uint8_t *SectionStorage::GetSectionBuf(int32_t version, uint32_t sectionNum, int32_t *outLength)
{
	SectionTableVersion *found = 0;
	for (int i = 0; i < 2; ++i) {
		if (m_tables[i].version == version) {
			found = &m_tables[i];
			break;
		}
	}
	if (found == 0) {
		beLogPrintf(2, "[DEBUG] %s : Invalid Version[%u]\n", "GetSectionBuf", version);
		return 0;
	}
	if (sectionNum >= found->sectionCount) {
		beLogPrintf(2, "[DEBUG] %s : Invalid Section Number[%u], section count=[%u]\n",
			"GetSectionBuf", sectionNum, found->sectionCount);
		return 0;
	}
	*outLength = found->sections[sectionNum].length;
	return m_memPool->GetBlockData(found->sections[sectionNum].block);
}

int32_t SectionStorage::GetCurrentVersion()
{
	return m_tables[m_currentIndex].version;
}

uint32_t SectionStorage::GetCurrentVersionAndCount(int32_t *outVersion)
{
	*outVersion = m_tables[m_currentIndex].version;
	return m_tables[m_currentIndex].sectionCount;
}

uint8_t *SectionStorage::GetSectionBufOnCurVersion(uint32_t sectionNum, int32_t *outLength)
{
	SectionTableVersion *cur = &m_tables[m_currentIndex];
	if (sectionNum >= cur->sectionCount) {
		beLogPrintf(2, "[DEBUG] %s : Invalid Section Number[%u], section count=[%u]\n",
			"GetSectionBufOnCurVersion", sectionNum, cur->sectionCount);
		return 0;
	}
	*outLength = cur->sections[sectionNum].length;
	return m_memPool->GetBlockData(cur->sections[sectionNum].block);
}

uint8_t *SectionStorage::GetSectionBufAccumulated(int32_t *outVersion, uint32_t sectionNum, int32_t *outLength)
{
	SectionTableVersion *acc = &m_tables[(m_currentIndex + 1) % 2];
	if (sectionNum >= acc->sectionCount) {
		beLogPrintf(2, "[DEBUG] %s : Invalid Section Number[%u], section count=[%u]\n",
			"GetSectionBufAccumulated", sectionNum, acc->sectionCount);
		return 0;
	}
	*outVersion = acc->version;
	*outLength = acc->sections[sectionNum].length;
	return m_memPool->GetBlockData(acc->sections[sectionNum].block);
}

int SectionStorage::IsAccumulatedSameAsCurrent()
{
	if (m_tables[0].version != m_tables[1].version ||
	    m_tables[0].capacity != m_tables[1].capacity ||
	    m_tables[0].sectionCount != m_tables[1].sectionCount) {
		return 0;
	}

	for (uint32_t i = 0; i < m_tables[0].sectionCount; ++i) {
		uint8_t *bufA = m_memPool->GetBlockData(m_tables[0].sections[i].block);
		uint8_t *bufB = m_memPool->GetBlockData(m_tables[1].sections[i].block);
		int32_t lenA = m_tables[0].sections[i].length;
		int32_t lenB = m_tables[1].sections[i].length;
		uint8_t crcA = m_tables[0].sections[i].hasCrc;
		uint8_t crcB = m_tables[1].sections[i].hasCrc;

		if (bufA == bufB && bufA == 0)
			break;
		if (lenA != lenB || crcA != crcB)
			return 0;
		if (bufA == 0 || bufB == 0)
			return 0;
		if (crcA != 0) {
			if (beMemcmp(bufA + lenA - 4, bufB + lenB - 4, 4) != 0)
				return 0;
		} else {
			if (beMemcmp(bufA, bufB, lenA) != 0)
				return 0;
		}
	}
	return 1;
}

int SectionStorage::ChangeCurrentVersion(int32_t version)
{
	int32_t newIdx = -1;
	int32_t oldIdx = -1;

	if (m_tables[0].version == m_tables[1].version) {
		if (!IsAccumulatedSameAsCurrent()) {
			oldIdx = m_currentIndex;
			newIdx = (oldIdx + 1) % 2;
		}
	} else {
		for (int i = 0; i < 2; ++i) {
			if (m_tables[i].version == version) {
				newIdx = i;
				oldIdx = (i + 1) % 2;
				break;
			}
		}
	}

	if (newIdx == -1) {
		beLogPrintf(2, "[DEBUG] %s : There are no sections with version [%u]\n",
			"ChangeCurrentVersion", version);
		return 0;
	}

	m_currentIndex = newIdx;
	SectionTableVersion *oldTable = &m_tables[oldIdx];
	if (oldTable->sections != 0)
		releaseTableBlocks(oldTable, 0);
	oldTable->version = -1;
	oldTable->sectionCount = 0;
	oldTable->capacity = 0;
	oldTable->sections = 0;
	return 1;
}

int SectionStorage::IsAccumulatedComplete()
{
	SectionTableVersion *acc = &m_tables[(m_currentIndex + 1) % 2];
	if (acc->sectionCount == 0 || acc->sectionCount > acc->capacity)
		return 0;

	for (uint32_t i = 0; i < acc->sectionCount; ++i) {
		if (!acc->sections[i].valid)
			return 0;
	}
	return 1;
}

int SectionStorage::DiscardAccumulated()
{
	SectionTableVersion *acc = &m_tables[(m_currentIndex + 1) % 2];
	if (acc->sections != 0)
		releaseTableBlocks(acc, 0);
	acc->sectionCount = 0;
	acc->version = -1;
	acc->capacity = 0;
	acc->sections = 0;
	return -1;
}
