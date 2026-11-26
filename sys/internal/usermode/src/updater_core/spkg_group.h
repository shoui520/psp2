/*
 * SpkgGroup (Sony's spkg_group.cpp): base class for a group of system packages (SPKGs)
 * belonging to one target component, handling reading from the PUP, inspection, ordering
 * and progress/status polling. The class and method names are ours.
 */
#ifndef UPDATER_CORE_SPKG_GROUP_H
#define UPDATER_CORE_SPKG_GROUP_H

#include <scetypes.h>
#include <updater_core/updater_core.h>
#include "updater_object.h"
#include "updater_array.h"

class SpkgOperationElement;

enum SpkgEntryStatus {
	SPKG_ENTRY_STATUS_INITIAL	= 0,
	SPKG_ENTRY_STATUS_SKIPPED	= 1,
	SPKG_ENTRY_STATUS_INSPECTED	= 2,
	SPKG_ENTRY_STATUS_UPDATED	= 3,
};

/* metadata for one SPKG segment extracted from a PUP (72 bytes; 80 bytes in 2014) */
struct SpkgEntry {
	SceUInt32 type;
	SceUInt32 status;
	SceUInt64 version;
	SceUInt64 buildNo;
	SceUInt64 segmentOffset;
	SceUInt64 segmentSize;
	SceUInt64 hardwareInfo;
	SceUInt64 chunkIndex;
	SceUInt64 decryptedSize;
	SceUInt64 finalSize;
#ifdef UPDATER_CORE_2014
	SceUInt32 platform;
#endif
};

/* shared time-estimation constants defined in spkg_group.cpp */
extern const float g_spkgReadSpeed;
extern const float g_spkgInspectSpeed;
extern const float g_spkgCompressedWriteSpeed;
extern const float g_spkgInspectOverheadMs;
extern const float g_spkgUpdateOverheadMs;
#ifdef UPDATER_CORE_2014
extern const float g_spkgWriteSpeedTable[29];
#else
extern const float g_spkgWriteSpeedTable[26];
#endif

class SpkgGroup : public UpdaterObject {
public:
	SpkgGroup();
	virtual ~SpkgGroup();

	virtual void EstimateTime(SceUpdaterEstimateTime *estimateTime) = 0;
	virtual int EnqueueUpdate(SceUID pupFd) = 0;
	virtual void SetDiffManifestSpkg(const SpkgEntry *entry) = 0;
	virtual int UpdateSpkg(SpkgOperationElement *element)
	{
		return -1;
	}
	virtual void OnInspectSkipped(SpkgEntry *entry) = 0;

	int AddSpkg(SpkgEntry *entry);
	int ReadSpkg(SceUID pupFd, const SpkgEntry *entry, void *buffer);
	int EnqueueInspect(SceUID pupFd);
	void PrepareUpdateOrder(bool *hasPending);
	int GetType(int *type);
	int GetInspectedCount(int *count);
	int GetVersion(SceUInt32 *version);
	int GetBuildNo(SceUInt32 *buildNo);
	float GetWriteSpeed(const SpkgEntry *entry);
	int GetUpdatePackageType(int index);
	SceUInt32 GetUpdateFlags(void);
	void NotifyProgress(int operation);
	int WaitStatus(int nodeType, SceUInt32 requestId, int *statusResult);
	int ConvertErrorCodeInspectResult(int packageType, int error);
	int ConvertErrorCodeInspectStatus(int packageType, int error);
	int ConvertErrorCodeInspectRequest(int packageType, int error);
	int InspectSpkg(SpkgOperationElement *element);

protected:
	UpdaterArray m_spkgList;
	int m_inspectedCount;
	SceUInt64 m_totalSize;
	SceUInt64 m_inspectedSize;
	SceUInt64 m_processedSize;
	SceUInt64 m_estimatedInspectMs;
	SceUInt64 m_estimatedUpdateMs;
};

#endif
