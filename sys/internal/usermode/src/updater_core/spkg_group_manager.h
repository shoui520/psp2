/*
 * SpkgGroupManager (Sony's spkg_group_manager.cpp): groups all system packages of a PUP
 * by target component, coordinates parallel inspection and update via SpkgParallelOperation
 * and SpkgOperationElement, and tracks overall progress and estimated time. The class and
 * method names are ours.
 */
#ifndef UPDATER_CORE_SPKG_GROUP_MANAGER_H
#define UPDATER_CORE_SPKG_GROUP_MANAGER_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <updater_core/updater_core.h>
#include "parallel_operation_base.h"
#include "updater_array.h"
#include "updater_object.h"

class SpkgGroup;
struct SpkgEntry;

#ifdef UPDATER_CORE_2014
#define SPKG_GROUP_MAX		23
#else
#define SPKG_GROUP_MAX		20
#endif

enum SpkgOperationType {
	SPKG_OP_NONE	= 0,
	SPKG_OP_UPDATE	= 1,
	SPKG_OP_INSPECT	= 2,
};

/* one SPKG work item passed between producer and consumer threads (28 bytes) */
class SpkgOperationElement : public ParallelOperationElement {
public:
	SpkgOperationElement();
	virtual ~SpkgOperationElement();

	virtual int Operate(void);

	int ReleaseBuffer(void);
	int AllocateBuffer(SceSize size);
	int SetTask(SceSize bufferSize, int operation, SpkgGroup *group, int spkgIndex);

	void *GetBuffer(void) const
	{
		return m_buffer;
	}

	int GetSpkgIndex(void) const
	{
		return m_spkgIndex;
	}

private:
	SceInt32 m_length;
	void *m_buffer;
	SceSize m_bufferSize;
	SpkgGroup *m_group;
	int m_operation;
	int m_spkgIndex;
};

/* parallel producer/consumer runner for inspecting or updating SPKG groups (192 bytes) */
class SpkgParallelOperation : public ParallelOperationBase {
public:
	SpkgParallelOperation()
	{
		m_operation = SPKG_OP_NONE;
		m_pupFd = -1;
	}

	virtual ~SpkgParallelOperation()
	{
	}

	int SetUpOperation(void)
	{
		return SetUp();
	}

	int ExecuteOperation(void *arg)
	{
		return Execute(arg);
	}

	ProducerConsumerChannel &GetFullChannel(void)
	{
		return m_fullChannel;
	}

	ProducerConsumerChannel &GetFreeChannel(void)
	{
		return m_freeChannel;
	}

	void SetOperation(int operation, SceUID pupFd)
	{
		m_pupFd = pupFd;
		m_operation = operation;
	}

	SceUID GetPupFd(void) const
	{
		return m_pupFd;
	}

protected:
	virtual int CreateElement(ParallelOperationElement **element);
	virtual int mainOperation(void *arg);

private:
	int m_operation;
	SceUID m_pupFd;
};

class SpkgGroupManager : public UpdaterObject {
public:
	SpkgGroupManager(int reserved);
	virtual ~SpkgGroupManager();

	void Terminate(void);
	int UpdateAllSpkg(const char *fileName);
	int GetEstimateProcessTime(const char *fileName, SceUpdaterEstimateTime *estimateTime);
	int InspectAllSpackage(const char *fileName);
	int AbortOperation(void);
	int GetProgress(SceUpdaterProgress *progress);
	int GetResult(void);
	int GetSpackageHeader(int type, SceUpdaterSpkgHeader *header);
	int CheckAcceptableSpkgType(const char *fileName, const int *types, int count, int *result);
	void AdjustEstimatedTime(SceInt64 inspectDeltaMs, SceInt64 updateDeltaMs);
	int IsDiffUpdate(void);

	SpkgParallelOperation *GetParallelOperation(void) const
	{
		return m_parallelOp;
	}

private:
	friend class SpkgParallelOperation;

	void recordUpdatedComponent(int spkgType);
	int updateAllGroups(SceUID pupFd);
	int inspectAllGroups(SceUID pupFd);
	int unmountPartitions(void);
	void calculateEstimateTime(void);
	int getGroupIndex(int spkgType);
	int registerSpkg(SpkgEntry *entry, SceUInt64 segmentId);
	int parsePup(SceUID pupFd);
	int getSpkgTypeFromGroupIndex(int groupIndex);

	UpdaterArray m_activeGroups;
	SpkgGroup *m_currentGroup;
	bool m_initialized;
	SpkgGroup *m_groups[SPKG_GROUP_MAX];
	SpkgParallelOperation *m_parallelOp;
	SceUInt32 m_updatedComponentMask;
	int m_totalInspectedCount;
	SceUInt64 m_estimatedInspectMs;
	SceUInt64 m_estimatedUpdateMs;
	SceUpdaterProgress m_lastProgress;
	SceKernelSysClock m_inspectStartTime;
	SceKernelSysClock m_updateStartTime;
#ifdef UPDATER_CORE_2014
	SceUInt8 m_reserved[120];
#else
	SceUInt8 m_reserved[132];
#endif
	int m_hasDiffGroup;
};

extern SpkgGroupManager *g_spkgGroupManager;

#endif
