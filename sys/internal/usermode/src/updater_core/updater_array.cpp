/*
 * UpdaterArray (in updater_core, 0x81005dac..0x810061e7): growable pointer array used by
 * SpkgGroup and SpkgGroupDiff to store SPKG entries, hardware IDs and manifest records.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "updater_array.h"
#include "updater_memory.h"

UpdaterArray::UpdaterArray(SceUInt32 capacity)
{
	m_items = NULL;
	m_count = 0;
	m_capacity = capacity;
	m_items = static_cast<void **>(
		g_updaterMemoryInfo.updaterCoreAlloc(capacity * sizeof(void *)));
	if (m_items != NULL)
		sceClibMemset(m_items, 0, m_capacity * sizeof(void *));
	else
		m_capacity = 0;
}

UpdaterArray::UpdaterArray()
{
	m_items = NULL;
	m_count = 0;
	m_capacity = 0;
}

UpdaterArray::~UpdaterArray()
{
	if (m_items != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(m_items);
		m_items = NULL;
	}
}

int UpdaterArray::Push(void *item)
{
	int ret;

	ret = EnsureCapacity(m_count);
	if (ret != 0)
		return ret;
	m_items[m_count++] = item;
	return 0;
}

int UpdaterArray::PushArray(void **items, SceUInt32 count)
{
	int ret = 0;
	SceUInt32 i;

	for (i = 0; i < count; ++i) {
		ret = Push(items[i]);
		if (ret != 0)
			break;
	}
	return ret;
}

void *UpdaterArray::Get(SceUInt32 index)
{
	if (index < m_count)
		return m_items[index];
	return NULL;
}

int UpdaterArray::Set(SceUInt32 index, void *item)
{
	int ret;

	ret = EnsureCapacity(index);
	if (ret != 0)
		return ret;
	if (index >= m_count)
		m_count = index + 1;
	m_items[index] = item;
	return 0;
}

void *UpdaterArray::Pop(void)
{
	if (m_count == 0)
		return NULL;
	m_count--;
	return m_items[m_count];
}

int UpdaterArray::CopyTo(SceUInt32 count, void **dst)
{
	if (count > m_count)
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	sceClibMemcpy(dst, m_items, count * sizeof(void *));
	return 0;
}

int UpdaterArray::Resize(SceUInt32 count)
{
	int ret;

	ret = EnsureCapacity(count);
	if (ret != 0)
		return ret;
	m_count = count;
	return 0;
}

int UpdaterArray::BinarySearch(int (*compare)(void *, void *), void *key, int *outIndex)
{
	int low = 0;
	int high = (int)m_count - 1;

	while (low <= high) {
		int mid = (low + high) / 2;
		int cmp = compare(key, Get(mid));

		if (cmp > 0) {
			high = mid - 1;
		} else if (cmp < 0) {
			low = mid + 1;
		} else {
			*outIndex = mid;
			return 1;
		}
	}
	*outIndex = low - 1;
	return 0;
}

int UpdaterArray::Insert(SceUInt32 index, void *item)
{
	int ret;

	if (index > m_count)
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	ret = EnsureCapacity(m_count);
	if (ret != 0)
		return ret;
	ret = Shift(index, true);
	if (ret != 0)
		return ret;
	return Set(index, item);
}

int UpdaterArray::Remove(SceUInt32 index)
{
	int ret;

	if (index >= m_count)
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	ret = Shift(index + 1, false);
	if (ret != 0)
		return ret;
	m_count--;
	return 0;
}

int UpdaterArray::EnsureCapacity(SceUInt32 index)
{
	SceUInt32 capacity = m_capacity;
	void **items;

	if (index >= capacity) {
		while (capacity < 0x100 && index >= capacity)
			capacity = capacity * 2 + 1;
		if (capacity >= 0x100)
			capacity &= ~0xffU;
		while (capacity <= index)
			capacity += 128;
		if (capacity > 0x800 && index < 0x800)
			capacity = 0x800;
		items = static_cast<void **>(
			g_updaterMemoryInfo.updaterCoreAlloc(capacity * sizeof(void *)));
		if (items == NULL)
			return SCE_UPDATER_ERROR_NO_MEMORY;
		sceClibMemset(items + m_capacity, 0, (capacity - m_capacity) * sizeof(void *));
		if (m_items != NULL) {
			sceClibMemcpy(items, m_items, m_capacity * sizeof(void *));
			g_updaterMemoryInfo.updaterCoreFree(m_items);
		}
		m_items = items;
		m_capacity = capacity;
	}
	return 0;
}

int UpdaterArray::Shift(SceUInt32 index, bool shiftRight)
{
	int ret = 0;

	if (shiftRight) {
		int i;

		for (i = (int)m_count - 1; i >= (int)index; --i) {
			ret = Set(i + 1, Get(i));
			if (ret != 0)
				break;
		}
	} else {
		SceUInt32 i;

		for (i = index; i < m_count; ++i) {
			ret = Set(i - 1, Get(i));
			if (ret != 0)
				break;
		}
	}
	return ret;
}
