/*
 * null_cell_source.cpp: count-driven FileList CellSource backed by a
 * NullVisualizerFactory (0x8135D7F0..0x8135D95B).
 */
#include "filelist_internal.h"

namespace filelist {

static const char s_nullCellSourceTypeName[] = "null_cell_source";

/* 0x8135D7F0 */
bool NullCellSource::IsInherit(const char *type) const
{
	if (type == s_nullCellSourceTypeName)
		return true;
	return paf::ui::filelist::CellSource::IsInherit(type);
}

/* 0x8135D808 */
const char *NullCellSource::GetTypeName() const
{
	return s_nullCellSourceTypeName;
}

const char *NullCellSource::TypeName()
{
	return s_nullCellSourceTypeName;
}

/* 0x8135D814 */
NullCellSource::NullCellSource(const paf::common::SharedPtr<NullVisualizerFactory> &factory,
			       int initialCount)
	: m_factory(factory),
	  m_pendingCount(initialCount),
	  m_focusIndex(0),
	  m_pendingFocus(-1)
{
}

/* 0x8135D85A / 0x8135D8CA */
NullCellSource::~NullCellSource()
{
}

/* 0x8135D8DA */
bool NullCellSource::IsDirty()
{
	return (m_pendingCount != 0 || m_pendingFocus >= 0);
}

/* 0x8135D8EC */
void NullCellSource::Update(paf::ui::filelist::CellListAccessor &accessor)
{
	if (m_pendingCount != 0) {
		uint32_t currentCount = accessor.GetNum();
		accessor.Delete(0, currentCount);
		accessor.Insert(0, static_cast<uint32_t>(m_pendingCount));
		m_pendingCount = 0;
	}
	if (m_pendingFocus >= 0) {
		accessor.SetFocus(m_focusIndex, paf::ui::ListView::FOCUS_ALIGN_TYPE_HEAD, NULL);
		m_pendingFocus = -1;
	}
}

/* 0x8135D934 */
int64_t NullCellSource::GetID(int32_t index)
{
	return static_cast<int64_t>(index);
}

/* 0x8135D93E */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> NullCellSource::GetVisualizer(
	paf::ui::filelist::VisualizerBindParam &param)
{
	if (m_factory.get() != NULL)
		return m_factory->CreateVisualizer(param.m_parent, param.m_index);
	return paf::ui::filelist::CellSource::GetVisualizer(param);
}

} /* namespace filelist */

paf::ui::filelist::CellSource *activityUiCreateSingleCellSource(void)
{
	paf::common::SharedPtr<filelist::NullVisualizerFactory> emptyFactory;
	return new filelist::NullCellSource(emptyFactory, 1);
}
