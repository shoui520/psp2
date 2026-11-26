/*
 * Dynamic array of void* pointers used by SpkgGroupManager, SpkgGroup and TarArchive
 * (implemented at the start of spkg_group.cpp, 0x81005dac..0x810061e7). The class and
 * method names are ours.
 */
#ifndef UPDATER_CORE_UPDATER_ARRAY_H
#define UPDATER_CORE_UPDATER_ARRAY_H

#include <scetypes.h>
#include "updater_object.h"

class UpdaterArray : public UpdaterObject {
public:
	UpdaterArray(SceUInt32 capacity);
	UpdaterArray();
	virtual ~UpdaterArray();

	virtual int Push(void *item);
	virtual int PushArray(void **items, SceUInt32 count);
	virtual void *Get(SceUInt32 index);
	virtual int Set(SceUInt32 index, void *item);
	virtual int EnsureCapacity(SceUInt32 index);
	virtual int Shift(SceUInt32 index, bool shiftRight);

	void *Pop(void);
	int CopyTo(SceUInt32 count, void **dst);
	int Resize(SceUInt32 count);
	int BinarySearch(int (*compare)(void *, void *), void *key, int *outIndex);
	int Insert(SceUInt32 index, void *item);
	int Remove(SceUInt32 index);

	SceUInt32 GetCount(void) const
	{
		return m_count;
	}

	void Clear(void)
	{
		m_count = 0;
	}

private:
	void **m_items;
	SceUInt32 m_count;
	SceUInt32 m_capacity;
};

#endif
