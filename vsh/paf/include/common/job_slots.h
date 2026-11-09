/*
 * paf::common::JobSlots: jobs kept by index. A job enqueued in a slot is a
 * SlotJobItem; when it finishes uncanceled, its slot is cleared and the
 * removed function is told. No Sony name is known for either class (their
 * exports are by NID only); the names are ours.
 */
#ifndef PAF_COMMON_JOB_SLOTS_H
#define PAF_COMMON_JOB_SLOTS_H

#include <stdint.h>
#include <std/vector>
#include <common/shared_ptr.h>
#include <job/job.h>

namespace paf {
namespace common {

class JobSlots;

/* the job of a slot: it tells its JobSlots when it finished */
class SlotJobItem : public job::JobItem {
public:
	SlotJobItem(const char *name, const job::JobItem::Option *option);

	virtual ~SlotJobItem()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	JobSlots *m_owner;
};

class JobSlots {
public:
	/* told the slot and the job when a job finished */
	typedef void (*RemovedFunction)(uint32_t index, common::SharedPtr<job::JobItem> item,
					void *userdata);

	JobSlots();
	~JobSlots();

	void Clear(uint32_t index);
	__declspec(dllexport) common::SharedPtr<job::JobItem> Get(uint32_t index);
	/* the job's slot, -1 without one */
	__declspec(dllexport) int32_t Find(job::JobItem *job);
	void Complete(SlotJobItem *job);
	void Grow(uint32_t size);
	void Cancel(uint32_t index);
	/* the job takes the slot; below 0 when the queue refused it */
	int32_t Enqueue(uint32_t index, job::JobQueue *queue, common::SharedPtr<job::JobItem> &item);
	__declspec(dllexport) void CancelAll();
	bool Contains(uint32_t index);
	uint32_t Size() const;
	/* the first empty slot, else the next one */
	uint32_t FreeIndex();
	int32_t Find(const common::SharedPtr<job::JobItem> &item);
	void SetRemovedFunction(RemovedFunction function, void *userdata);

	vector<common::SharedPtr<job::JobItem> > m_items;
	RemovedFunction m_on_removed;
	void *m_userdata;
};

}
}

#endif
