/*
 * SegmentAnalysis (segment_analysis.cpp): follows a PUP as it is downloaded, piece by piece
 * (sceUpdaterIsSegmentAvailable), and says when one segment of it has arrived whole. It reads
 * the header, then the segment table until the segment's entry, then counts the bytes up to
 * the segment's end. The names are ours.
 */
#ifndef UPDATER_CORE_SEGMENT_ANALYSIS_H
#define UPDATER_CORE_SEGMENT_ANALYSIS_H

#include <scetypes.h>
#include <update_mgr/pup_file.h>
#include <updater_core/updater_core.h>
#include "updater_object.h"

class SegmentAnalysis : public UpdaterObject {
public:
	SegmentAnalysis();
	virtual ~SegmentAnalysis();

	void Initialize(SceUInt64 segmentId);
	int IsSegmentAvailable(SceUpdaterSegmentData *data, int *result);

private:
	int findSegment(SceUpdaterSegmentData *data, SceUInt64 *remaining, bool *found);

	SceUInt64 m_segmentId;		/* the segment waited for */
	SceInt64 m_processed;		/* the bytes of the PUP seen so far */
	SceInt64 m_segmentOffset;	/* the segment's place, from its entry */
	SceInt64 m_segmentSize;
	PupHeader m_header;
	int m_headerRead;
	PupSegment m_entry;		/* the table entry being read */
	SceInt32 m_entryLength;		/* its bytes read so far, when a piece ends inside it */
	int m_found;			/* the segment's entry has been read */
	SceUInt64 m_entryIndex;		/* the entries read */
};

#endif
