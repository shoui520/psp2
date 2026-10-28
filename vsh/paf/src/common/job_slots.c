#include "job_slots.h"

#define PafPlacementNew _ZnwjPv
extern void *PafPlacementNew(size_t size, void *where);

static const struct {
	int offset_to_top;
	const void *type_info;
	JobItemVTable methods;
} slotJobItemClass = {
	0, NULL, {
		SlotJobItemDestroy, SlotJobItemDelete, JobItemCancel,
		SlotJobItemRun, SlotJobItemFinish
	}
};

SlotJobItem *SlotJobItemConstruct(SlotJobItem *self, const char *name,
				 const JobItemOption *option)
{
	JobItemConstructBase(&self->base, name, option);
	self->base.vtable = &slotJobItemClass.methods;
	self->m_owner = NULL;
	return self;
}

SlotJobItem *SlotJobItemConstructBase(SlotJobItem *self, const char *name,
				     const JobItemOption *option)
{
	JobItemConstructBase(&self->base, name, option);
	self->base.vtable = &slotJobItemClass.methods;
	self->m_owner = NULL;
	return self;
}

int SlotJobItemRun(JobItem *base)
{
	return 0;
}

JobSlots *JobSlotsClear(JobSlots *self, size_t index)
{
	JobItemSharedPtr *entry;

	if (index >= self->m_items._size)
		return self;
	entry = &self->m_items._data[index];
	JobItemSharedPtrRelease(entry);
	entry->m_ptr = NULL;
	entry->m_ref_counter = NULL;
	return NULL;
}

uintptr_t JobSlotsGet(JobItemSharedPtr *result, JobSlots *self, size_t index)
{
	uintptr_t value;
	JobItem *ptr;
	JobItemRefCounter *counter;

	if (index >= self->m_items._size) {
		result->m_ptr = NULL;
		result->m_ref_counter = NULL;
		return 0;
	}
	value = (uintptr_t)self->m_items._data;
	ptr = self->m_items._data[index].m_ptr;
	result->m_ptr = ptr;
	counter = self->m_items._data[index].m_ref_counter;
	result->m_ref_counter = counter;
	/* Preserve the original return register as well as the output object. */
	if (ptr)
		value = (uint32_t)sceAtomicIncrement32AcqRel(&counter->m_counter) + 1;
	return value;
}

int JobSlotsFind(JobSlots *self, JobItem *job)
{
	size_t size = self->m_items._size;
	size_t current_size = size;
	size_t index;

	for (index = 0; index < size; ++index) {
		if (index >= current_size) {
			ScePafStdc_D608246A();
			current_size = self->m_items._size;
		}
		if (self->m_items._data[index].m_ptr == job)
			return index;
	}
	return -1;
}

void JobSlotsComplete(JobSlots *self, SlotJobItem *job)
{
	size_t size = self->m_items._size;
	size_t current_size = size;
	size_t index;
	JobItemSharedPtr item = { NULL, NULL };

	for (index = 0; index < size; ++index) {
		if (index >= current_size) {
			ScePafStdc_D608246A();
			current_size = self->m_items._size;
		}
		if (self->m_items._data[index].m_ptr == (JobItem *)job)
			break;
	}
	if (index == size || index == (size_t)-1)
		return;
	if (index < current_size) {
		item = self->m_items._data[index];
		JobItemSharedPtrAddRef(&item);
	}
	JobSlotsClear(self, index);
	if (self->m_on_removed) {
		JobItemSharedPtr callback_item = item;

		JobItemSharedPtrAddRef(&callback_item);
		self->m_on_removed(index, &callback_item, self->m_userdata);
		JobItemSharedPtrRelease(&callback_item);
	}
	JobItemSharedPtrRelease(&item);
}

void SlotJobItemFinish(JobItem *base, int result)
{
	SlotJobItem *self = (SlotJobItem *)base;

	if (!JobItemIsCanceled(base) && self->m_owner)
		JobSlotsComplete(self->m_owner, self);
}

void JobSlotsGrow(JobSlots *self, size_t size)
{
	if (self->m_items._size < size) {
		JobItemSharedPtr empty = { NULL, NULL };

		JobItemVectorResize(&self->m_items, size, &empty);
		JobItemSharedPtrRelease(&empty);
	}
}

JobSlots *JobSlotsCancel(JobSlots *self, size_t index)
{
	JobItemSharedPtr *entry;

	if (index >= self->m_items._size)
		return self;
	entry = &self->m_items._data[index];
	if (entry->m_ptr) {
		entry->m_ptr->vtable->Cancel(entry->m_ptr);
		JobItemSharedPtrRelease(entry);
		entry->m_ptr = NULL;
		entry->m_ref_counter = NULL;
	}
	return NULL;
}

int JobSlotsEnqueue(JobSlots *self, size_t index, JobQueue *queue, JobItemSharedPtr *item)
{
	JobItemSharedPtr *entry;
	JobItem *ptr;
	JobItemRefCounter *counter;
	SlotJobItem *job;
	int result;

	JobSlotsCancel(self, index);
	JobSlotsGrow(self, index + 1);
	if (index >= self->m_items._size)
		ScePafStdc_D608246A();
	entry = &self->m_items._data[index];
	counter = item->m_ref_counter;
	ptr = item->m_ptr;
	if (ptr != entry->m_ptr) {
		JobItemSharedPtrRelease(entry);
		entry->m_ptr = ptr;
		entry->m_ref_counter = counter;
		JobItemSharedPtrAddRef(entry);
	}
	job = (SlotJobItem *)item->m_ptr;
	job->m_owner = self;
	result = JobQueueEnqueue(queue, item);
	if (result) {
		job->m_owner = NULL;
		if (index >= self->m_items._size)
			ScePafStdc_D608246A();
		entry = &self->m_items._data[index];
		JobItemSharedPtrRelease(entry);
		entry->m_ptr = NULL;
		entry->m_ref_counter = NULL;
	}
	return result;
}

void JobSlotsCancelAll(JobSlots *self)
{
	size_t size = self->m_items._size;
	size_t index;

	for (index = 0; index < size; ++index)
		JobSlotsCancel(self, index);
}

int JobSlotsContains(JobSlots *self, size_t index)
{
	return index < self->m_items._size && self->m_items._data[index].m_ptr != NULL;
}

size_t JobSlotsFreeIndex(JobSlots *self)
{
	size_t size = self->m_items._size;
	size_t current_size = size;
	size_t index;

	for (index = 0; index < size; ++index) {
		if (index >= current_size) {
			ScePafStdc_D608246A();
			current_size = self->m_items._size;
		}
		if (!self->m_items._data[index].m_ptr)
			return index;
	}
	return current_size;
}

int JobSlotsFindShared(JobSlots *self, const JobItemSharedPtr *item)
{
	JobItem *job = item->m_ptr;
	size_t size = self->m_items._size;
	size_t current_size = size;
	size_t index;

	for (index = 0; index < size; ++index) {
		if (index >= current_size) {
			ScePafStdc_D608246A();
			current_size = self->m_items._size;
		}
		if (self->m_items._data[index].m_ptr == job)
			return index;
	}
	return -1;
}

JobSlots *JobSlotsConstruct(JobSlots *self)
{
	self->m_items._data = NULL;
	self->m_items._size = 0;
	self->m_items._capacity = 0;
	self->m_userdata = NULL;
	self->m_on_removed = NULL;
	return self;
}

JobSlots *JobSlotsConstructBase(JobSlots *self)
{
	self->m_items._data = NULL;
	self->m_items._size = 0;
	self->m_items._capacity = 0;
	self->m_userdata = NULL;
	self->m_on_removed = NULL;
	return self;
}

JobSlots *JobSlotsDestroy(JobSlots *self)
{
	size_t size = self->m_items._size;
	size_t index;

	for (index = 0; index < size; ++index)
		JobSlotsCancel(self, index);
	JobItemVectorDestroy(&self->m_items);
	return self;
}

JobSlots *JobSlotsDestroyBase(JobSlots *self)
{
	size_t size = self->m_items._size;
	size_t index;

	for (index = 0; index < size; ++index)
		JobSlotsCancel(self, index);
	JobItemVectorDestroy(&self->m_items);
	return self;
}

JobItemVector *JobItemVectorDestroy(JobItemVector *self)
{
	if (self->_data) {
		size_t size = self->_size;

		while (size)
			JobItemSharedPtrRelease(&self->_data[--size]);
		ScePafStdc_1B77082E(self->_data);
	}
	self->_data = (JobItemSharedPtr *)-1;
	self->_size = (size_t)-1;
	self->_capacity = (size_t)-1;
	return self;
}

int JobItemVectorReallocate(JobItemVector *self, size_t capacity)
{
	int had_storage = self->_data != NULL;
	JobItemSharedPtr *data = ScePafStdc_FC5CD359(sizeof(*data) * capacity);
	size_t index;

	if (!had_storage) {
		if (!data)
			ScePafStdc_D608246A();
		self->_data = data;
		if (!data)
			return 0;
		self->_capacity = capacity;
		return 1;
	}
	if (!data) {
		ScePafStdc_D608246A();
		return 0;
	}
	for (index = 0; index < self->_size; ++index) {
		JobItemSharedPtr *entry = PafPlacementNew(sizeof(*entry), &data[index]);

		if (entry) {
			entry->m_ptr = self->_data[index].m_ptr;
			entry->m_ref_counter = self->_data[index].m_ref_counter;
			JobItemSharedPtrAddRef(entry);
		}
	}
	for (index = 0; index < self->_size; ++index)
		JobItemSharedPtrRelease(&self->_data[index]);
	ScePafStdc_1B77082E(self->_data);
	self->_data = data;
	self->_capacity = capacity;
	return 1;
}

void JobItemVectorResize(JobItemVector *self, size_t size, const JobItemSharedPtr *value)
{
	size_t count = self->_size;
	size_t capacity = self->_capacity;

	if (size < count) {
		do {
			JobItemSharedPtrRelease(&self->_data[count - 1]);
			count = self->_size - 1;
			self->_size = count;
			if ((int32_t)count < (int32_t)(capacity - 20))
				capacity -= 10;
		} while (size < count);
		if (self->_capacity != capacity)
			JobItemVectorReallocate(self, capacity);
	} else if (size > count) {
		size_t index;

		while (capacity < size)
			capacity += 10;
		if (self->_capacity != capacity) {
			int allocated = JobItemVectorReallocate(self, capacity);

			count = self->_size;
			if (!allocated)
				return;
		}
		for (index = count; index < size; ++index) {
			JobItemSharedPtr *entry = PafPlacementNew(sizeof(*entry), &self->_data[index]);

			if (entry) {
				entry->m_ptr = value->m_ptr;
				entry->m_ref_counter = value->m_ref_counter;
				JobItemSharedPtrAddRef(entry);
			}
		}
		self->_size = size;
	}
}

JobItem *SlotJobItemDestroy(JobItem *base)
{
	base->vtable = &slotJobItemClass.methods;
	JobItemDestroyBase(base);
	return base;
}

void SlotJobItemDelete(JobItem *base)
{
	base->vtable = &slotJobItemClass.methods;
	JobItemDestroyBase(base);
	ScePafStdc_1B77082E(base);
}
