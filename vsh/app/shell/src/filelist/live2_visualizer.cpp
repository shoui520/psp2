/*
 * live2_visualizer.cpp: Live2Visualizer and Live2UpdateRecordJob
 * (0x8134D680..0x8134D97B).
 */
#include "filelist_internal.h"

namespace filelist {

namespace {

static const char kLive2VisualizerTypeName[] = "live2_visualizer";

/* Background slot job that fetches a cell's Live2Record from Live2Iterator (56 bytes) */
class Live2UpdateRecordJob : public paf::common::SlotJobItem {
public:
	Live2UpdateRecordJob(const paf::common::SharedPtr<Live2Iterator> &iterator,
			     SceInt64 cellId)
		: paf::common::SlotJobItem("Live2UpdateRecordJob", NULL),
		  m_iterator(iterator),
		  m_cellId(cellId),
		  m_record()
	{
	}

	virtual ~Live2UpdateRecordJob()
	{
	}

	/* 0x81443748 */
	virtual int32_t Run()
	{
		m_record = m_iterator->GetRecordByCellId(m_cellId);
		return 0;
	}

	paf::common::SharedPtr<Live2Iterator> m_iterator;
	SceInt64 m_cellId;
	paf::intrusive_ptr<Live2Record> m_record;
};

} /* anonymous namespace */

/* 0x8134D680 */
bool Live2Visualizer::IsInherit(const char *type) const
{
	if (type == kLive2VisualizerTypeName)
		return true;
	return BasicVisualizer::IsInherit(type);
}

/* 0x8134D698 */
const char *Live2Visualizer::GetTypeName() const
{
	return kLive2VisualizerTypeName;
}

const char *Live2Visualizer::TypeName()
{
	return kLive2VisualizerTypeName;
}

/* 0x8134D6A4 / 0x8134D72C */
Live2Visualizer::Live2Visualizer(
	paf::ui::Widget *parent,
	paf::Plugin *plugin,
	const paf::IDParam &templateId,
	const paf::IDParam &itemId,
	const paf::common::SharedPtr<Live2Iterator> &iterator,
	SceInt64 cellId)
	: BasicVisualizer(parent, plugin, templateId, itemId),
	  m_recordJobSlots(),
	  m_record(),
	  m_iterator(iterator),
	  m_cellId(cellId),
	  m_isRecordLoaded(false)
{
	m_recordJobSlots.SetRemovedFunction(RecordJobRemovedCallback, this);
}

Live2Visualizer::~Live2Visualizer()
{
}

/* 0x8134D748 */
void Live2Visualizer::RequestRecordUpdate()
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new Live2UpdateRecordJob(m_iterator, m_cellId));
	paf::job::JobQueue *queue =
		paf::ui::filelist::Manager::GetJobQueue(paf::ui::filelist::Manager::QUEUE_TYPE_FAST);
	m_recordJobSlots.Enqueue(0, queue, job);
}

/* 0x8134D860 */
paf::common::SharedPtr<Live2AccessorProxy> Live2Visualizer::GetAccessorProxy()
{
	return m_iterator->GetAccessorProxy();
}

} /* namespace filelist */

class ActivityFeedItemBase;

void activityUiGetItemContext(paf::common::SharedPtr<int> *outContext,
			      ActivityFeedItemBase *self)
{
	paf::common::SharedPtr<filelist::Live2AccessorProxy> proxy =
		reinterpret_cast<filelist::Live2Visualizer *>(self)->GetAccessorProxy();
	if (outContext != NULL)
		*outContext = *reinterpret_cast<paf::common::SharedPtr<int> *>(&proxy);
}

namespace filelist {

/* 0x8134D86A */
void Live2Visualizer::Stop()
{
	m_recordJobSlots.CancelAll();
	if (m_isRecordLoaded) {
		m_iterator->ReleaseCellId(m_cellId);
		m_isRecordLoaded = false;
	}
	BasicVisualizer::Stop();
}

void Live2Visualizer::OnRecordUpdated()
{
}

void Live2Visualizer::OnRecordCleared()
{
}

/* 0x8134D894 */
void Live2Visualizer::RecordJobRemovedCallback(
	uint32_t slotIndex,
	paf::common::SharedPtr<paf::job::JobItem> job,
	void *userdata)
{
	static_cast<Live2Visualizer *>(userdata)->OnRecordJobRemoved(slotIndex, job);
}

/* 0x8134D938 */
void Live2Visualizer::OnRecordJobRemoved(
	uint32_t slotIndex,
	paf::common::SharedPtr<paf::job::JobItem> job)
{
	if (slotIndex != 0)
		return;

	Live2UpdateRecordJob *updateJob = static_cast<Live2UpdateRecordJob *>(job.get());
	m_record = updateJob->m_record;
	if (!m_isRecordLoaded) {
		m_iterator->PinCellRowid(m_cellId);
		m_isRecordLoaded = true;
	}
	OnRecordUpdated();
}

} /* namespace filelist */
