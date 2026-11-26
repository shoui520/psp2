/*
 * SpkgGroupFull (Sony's spkg_group_full.cpp): full (non-differential) system package group,
 * enqueueing each non-skipped SPKG to the parallel update thread and writing it via
 * sceSblUsUpdateSpackage. The class and method names are ours.
 */
#ifndef UPDATER_CORE_SPKG_GROUP_FULL_H
#define UPDATER_CORE_SPKG_GROUP_FULL_H

#include <scetypes.h>
#include "spkg_group.h"

/* 72 bytes (0x48) */
class SpkgGroupFull : public SpkgGroup {
public:
	SpkgGroupFull();
	virtual ~SpkgGroupFull();

	virtual void EstimateTime(SceUpdaterEstimateTime *estimateTime);
	virtual int EnqueueUpdate(SceUID pupFd);
	virtual void SetDiffManifestSpkg(const SpkgEntry *entry);
	virtual int UpdateSpkg(SpkgOperationElement *element);
	virtual void OnInspectSkipped(SpkgEntry *entry);

private:
	int convertErrorCodeUpdateResult(int packageType, int error);
	int convertErrorCodeUpdateStatus(int packageType, int error);
	int convertErrorCodeUpdateRequest(int packageType, int error);

	int m_updatedCount;
};

#endif
