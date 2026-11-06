/* SegmentAnalysis: which PUP segments a download has (sceUpdaterCreateSegmentAnalysisHandler). The names are ours. */
#ifndef UPDATER_CORE_SEGMENT_ANALYSIS_H
#define UPDATER_CORE_SEGMENT_ANALYSIS_H

#include <scetypes.h>
#include <updater_core/updater_core.h>

class SegmentAnalysis {
public:
	SegmentAnalysis();
	virtual ~SegmentAnalysis();

	void Initialize(SceUInt64 size);
	int IsSegmentAvailable(SceUpdaterSegmentData *data, int *result);

private:
	SceUInt8 m_state[220];	/* laid out with its file (0xe0 bytes in all) */
};

#endif
