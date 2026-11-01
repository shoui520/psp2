/*
 * paf::common::JobSlots and SlotJobItem (names ours, see the header), and the
 * NID-only abstract class Sony's file ends with.
 */
#include <common/job_slots.h>
#include <std/stdlib.h>

namespace paf {
namespace common {

SlotJobItem::SlotJobItem(const char *name, const job::JobItem::Option *option)
	: job::JobItem(name, option), m_owner(NULL)
{
}

int SlotJobItem::Run()
{
	return 0;
}

void JobSlots::Clear(uint32_t index)
{
	if (index < m_items.size())
		m_items.at(index).reset();
}

inline common::SharedPtr<job::JobItem> JobSlots::Get(uint32_t index)
{
	if (index >= m_items.size())
		return common::SharedPtr<job::JobItem>();
	return m_items.at(index);
}

inline int32_t JobSlots::Find(job::JobItem *job)
{
	uint32_t size = m_items.size();

	for (uint32_t i = 0; i < size; ++i) {
		if (m_items.at(i).get() == job)
			return i;
	}
	return -1;
}

void JobSlots::Complete(SlotJobItem *job)
{
	int32_t index = Find(job);

	if (index == -1)
		return;

	common::SharedPtr<job::JobItem> item = Get(index);

	Clear(index);
	if (m_on_removed != NULL)
		m_on_removed(index, item, m_userdata);
}

void SlotJobItem::Finish(int result)
{
	if (!IsCanceled() && m_owner != NULL)
		m_owner->Complete(this);
}

void JobSlots::Grow(uint32_t size)
{
	if (m_items.size() < size)
		m_items.resize(size, common::SharedPtr<job::JobItem>());
}

void JobSlots::Cancel(uint32_t index)
{
	if (index >= m_items.size())
		return;

	common::SharedPtr<job::JobItem> &item = m_items.at(index);

	if (item.get() != NULL) {
		item->Cancel();
		item.reset();
	}
}

int32_t JobSlots::Enqueue(uint32_t index, job::JobQueue *queue,
			  common::SharedPtr<job::JobItem> &item)
{
	Cancel(index);
	Grow(index + 1);
	m_items.at(index) = item;

	SlotJobItem *job = static_cast<SlotJobItem *>(item.get());

	job->m_owner = this;

	int32_t result = queue->Enqueue(item);

	if (result != 0) {
		job->m_owner = NULL;
		m_items.at(index).reset();
	}
	return result;
}

inline void JobSlots::CancelAll()
{
	uint32_t size = m_items.size();

	for (uint32_t i = 0; i < size; ++i)
		Cancel(i);
}

bool JobSlots::Contains(uint32_t index)
{
	return index < m_items.size() && m_items.at(index).get() != NULL;
}

uint32_t JobSlots::Size() const
{
	return m_items.size();
}

uint32_t JobSlots::FreeIndex()
{
	uint32_t size = m_items.size();
	int32_t index = -1;

	for (uint32_t i = 0; i < size; ++i) {
		if (m_items.at(i).get() == NULL) {
			index = i;
			break;
		}
	}
	if (index == -1)
		return m_items.size();
	return index;
}

int32_t JobSlots::Find(const common::SharedPtr<job::JobItem> &item)
{
	return Find(item.get());
}

void JobSlots::SetRemovedFunction(RemovedFunction function, void *userdata)
{
	m_on_removed = function;
	m_userdata = userdata;
}

JobSlots::JobSlots()
{
	m_userdata = NULL;
	m_on_removed = NULL;
}

JobSlots::~JobSlots()
{
	CancelAll();
}

Interface_0691F7F7::Interface_0691F7F7()
{
}

Interface_0691F7F7::~Interface_0691F7F7()
{
}

}
}
