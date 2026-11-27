/* PupController (pup_controller.cpp): reads and verifies a PUP. The class name is ours. */
#ifndef UPDATER_CORE_PUP_CONTROLLER_H
#define UPDATER_CORE_PUP_CONTROLLER_H

#include <scetypes.h>
#include <updater_core/updater_core.h>

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
	int GetPupInformation(const char *fileName, SceUpdaterPupInformation *information);
	int NetworkPreCheckVersion(SceUInt32 version, int *result);
	int NetworkCheckBaseVersion(SceUInt32 version, int *result);
	int CheckNewerThanSystemVersion(SceUInt32 version, int *result);
	SceUInt64 GetSegmentAnalysisSize(void);
#ifdef UPDATER_CORE_2014
	static int InitializeCache(void);
	static int FinalizeCache(void);
#endif

private:
#ifdef UPDATER_CORE_2014
	static int clearCache(void);
	int openCachedPup(const char *fileName);
	int readCachedHeader(void);
	__attribute__((noinline)) int getPupInfoCached(const char *fileName,
						       SceUpdaterPupInformation *information);
	int getPupInfoDirect(const char *fileName, SceUpdaterPupInformation *information);
#endif
	int getSegmentInfo(const char *fileName, SceUInt64 segmentId, SceUInt64 *size,
			   SceUInt64 *offset);
};

#endif
