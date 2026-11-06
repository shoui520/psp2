/* PupController (pup_controller.cpp): reads and verifies a PUP. The class name is ours. */
#ifndef UPDATER_CORE_PUP_CONTROLLER_H
#define UPDATER_CORE_PUP_CONTROLLER_H

#include <scetypes.h>

class PupController {
public:
	PupController();
	~PupController();

	int VerifyPup(const char *fileName);
	int VerifyPupSegment(const char *fileName, SceUInt64 segmentId);
	int Parse(const char *fileName, SceUInt64 segmentId, void *pData, SceUInt64 size,
		  SceUInt64 offset);
	int GetSegmentSize(const char *fileName, SceUInt64 segmentId, SceUInt64 *size);
	int IsApplicablePup(const char *fileName);
	int GetPupInformation(const char *fileName, void *information);
	int NetworkPreCheckVersion(void *versionInfo, void *result);
	int NetworkCheckBaseVersion(void *versionInfo, void *result);
	int CheckNewerThanSystemVersion(void *versionInfo, void *result);
	SceUInt64 GetSegmentAnalysisSize(void);
};

#endif
