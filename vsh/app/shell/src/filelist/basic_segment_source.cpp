/*
 * basic_segment_source.cpp: BasicSegmentSource (paf::ui::filelist::SegmentSource
 * subclass with queued insert/delete operations, 0x8134A848..0x8134ADD3).
 */
#include "filelist_internal.h"
#include <std/string.h>

namespace filelist {

namespace {

static const char kBasicSegmentSourceTypeName[] = "basic_segment_source";

enum SegmentOpType {
	SEGMENT_OP_INSERT = 0,
	SEGMENT_OP_REMOVE = 1
};

} /* anonymous namespace */

BasicSegmentSource::PendingOp::PendingOp()
	: opType(SEGMENT_OP_REMOVE),
	  index(0),
	  entry()
{
	sce_paf_memset(&option, 0, sizeof(option));
	option.m_header_disappearance = true;
}

/* 0x8134A86C */
BasicSegmentSource::PendingOp::PendingOp(
	int segIndex,
	const paf::common::SharedPtr<SegmentEntry> &segEntry,
	const paf::ui::filelist::SegmentListAccessor::OptionParam &opt)
	: opType(SEGMENT_OP_INSERT),
	  index(segIndex),
	  entry(segEntry),
	  option(opt)
{
}

BasicSegmentSource::BasicSegmentSource()
{
}

paf::ui::filelist::SegmentSource *createBasicSegmentSource()
{
	return new BasicSegmentSource();
}

BasicSegmentSource::~BasicSegmentSource()
{
}

/* 0x8134A848 */
bool BasicSegmentSource::IsInherit(const char *type) const
{
	if (type == kBasicSegmentSourceTypeName)
		return true;
	return paf::ui::filelist::SegmentSource::IsInherit(type);
}

/* 0x8134A860 */
const char *BasicSegmentSource::GetTypeName() const
{
	return kBasicSegmentSourceTypeName;
}

const char *BasicSegmentSource::TypeName()
{
	return kBasicSegmentSourceTypeName;
}

/* 0x8134A8D2 */
void BasicSegmentSource::InsertSegment(
	int index,
	const paf::intrusive_ptr<paf::ui::filelist::CellSource> &cellSource,
	const paf::ui::filelist::SegmentListAccessor::OptionParam &option)
{
	SegmentEntry *seg = new SegmentEntry();
	if (seg != NULL) {
		seg->segmentId = ScePafCommon_AC2F3710();
		seg->cellSource = cellSource;
	}
	paf::common::SharedPtr<SegmentEntry> entryRef(seg);
	PendingOp op(index, entryRef, option);
	m_pendingOps.push_back(op);
}

/* 0x8134AAA2 */
void BasicSegmentSource::RemoveSegment(int index)
{
	PendingOp op;
	op.opType = SEGMENT_OP_REMOVE;
	op.index = index;
	m_pendingOps.push_back(op);
}

/* 0x8134AB4C */
paf::intrusive_ptr<paf::ui::filelist::CellSource> BasicSegmentSource::GetSegmentCellSource(
	int index)
{
	paf::vector<paf::common::SharedPtr<SegmentEntry> > simulated(m_segments);

	for (paf::vector<PendingOp>::iterator it = m_pendingOps.begin();
	     it != m_pendingOps.end(); ++it) {
		int opIndex = it->index;
		if (it->opType != SEGMENT_OP_INSERT) {
			if (opIndex >= 0 && static_cast<size_t>(opIndex) < simulated.size())
				simulated.erase(simulated.begin() + opIndex);
		} else {
			if (opIndex >= 0 && static_cast<size_t>(opIndex) <= simulated.size())
				simulated.insert(simulated.begin() + opIndex, it->entry);
		}
	}

	if (index < 0 || static_cast<size_t>(index) >= simulated.size())
		return paf::intrusive_ptr<paf::ui::filelist::CellSource>();

	return simulated[static_cast<size_t>(index)]->cellSource;
}

} /* namespace filelist */

class ActivityFeedCellSource;

void activityUiSegmentSetCell(paf::ui::filelist::SegmentSource *source, int segmentIndex,
			      paf::common::RefObj **cellSource,
			      paf::ui::filelist::SegmentListAccessor::OptionParam *option)
{
	paf::intrusive_ptr<paf::ui::filelist::CellSource> cellRef(
		cellSource != NULL ? reinterpret_cast<paf::ui::filelist::CellSource *>(*cellSource) : NULL);
	static_cast<filelist::BasicSegmentSource *>(source)->InsertSegment(
		segmentIndex, cellRef, *option);
}

void activityUiSegmentRemoveCell(paf::ui::filelist::SegmentSource *source, int segmentIndex)
{
	static_cast<filelist::BasicSegmentSource *>(source)->RemoveSegment(segmentIndex);
}

void activityUiSegmentGetCell(ActivityFeedCellSource **outCellSource,
			      paf::ui::filelist::SegmentSource *source, int segmentIndex)
{
	paf::intrusive_ptr<paf::ui::filelist::CellSource> cellRef =
		static_cast<filelist::BasicSegmentSource *>(source)->GetSegmentCellSource(segmentIndex);
	paf::ui::filelist::CellSource *rawCell = cellRef.get();
	if (rawCell != NULL)
		rawCell->AddRef();
	if (outCellSource != NULL)
		*outCellSource = reinterpret_cast<ActivityFeedCellSource *>(rawCell);
}

namespace filelist {

/* 0x8134AC6A */
int64_t BasicSegmentSource::GetID(int32_t index)
{
	if (index < 0 || static_cast<size_t>(index) >= m_segments.size())
		return paf::ui::filelist::SegmentSource::GetID(index);
	return m_segments[static_cast<size_t>(index)]->segmentId;
}

/* 0x8134AC86 */
bool BasicSegmentSource::IsDirty()
{
	return m_pendingOps.size() != 0;
}

/* 0x8134AC90 */
void BasicSegmentSource::Update(paf::ui::filelist::SegmentListAccessor &accessor)
{
	for (paf::vector<PendingOp>::iterator it = m_pendingOps.begin();
	     it != m_pendingOps.end(); ++it) {
		int opIndex = it->index;
		if (it->opType != SEGMENT_OP_INSERT) {
			if (opIndex >= 0 && static_cast<size_t>(opIndex) < m_segments.size()) {
				m_segments.erase(m_segments.begin() + opIndex);
				accessor.Delete(opIndex, 1);
			}
		} else {
			if (opIndex >= 0 && static_cast<size_t>(opIndex) <= m_segments.size()) {
				m_segments.insert(m_segments.begin() + opIndex, it->entry);
				accessor.Insert(opIndex, 1);
				accessor.SetOption(opIndex, it->option);
			}
		}
	}
	m_pendingOps.clear();
}

/* 0x8134AD9E */
paf::intrusive_ptr<paf::ui::filelist::CellSource> BasicSegmentSource::GetCellSource(
	int32_t index, int64_t id)
{
	if (index < 0 || static_cast<size_t>(index) >= m_segments.size())
		return paf::ui::filelist::SegmentSource::GetCellSource(index, id);
	return m_segments[static_cast<size_t>(index)]->cellSource;
}

} /* namespace filelist */
