/*
 * SpkgGroupFull (Sony's spkg_group_full.cpp): full (non-differential) system package group,
 * enqueueing each non-skipped SPKG to the consumer thread and writing it with
 * sceSblUsUpdateSpackage.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "spkg_group.h"
#include "spkg_group_full.h"
#include "spkg_group_manager.h"

#define UPDATER_FILE_ID			0x9a9c3bd8

#define SPKG_NODE_TYPE_UPDATE		1

#define SCE_SBL_US_ERROR_INVALID_OP	((int)0x800f0210)
#define SCE_SBL_US_ERROR_UP_TO_DATE	((int)0x800f0236)

SpkgGroupFull::SpkgGroupFull()
{
	m_updatedCount = 0;
}

SpkgGroupFull::~SpkgGroupFull()
{
}

int SpkgGroupFull::EnqueueUpdate(SceUID pupFd)
{
	int count = (int)m_spkgList.GetCount();
	SpkgOperationElement *element = NULL;
	int ret = 0;
	int i;

	m_updatedCount = 0;
	m_processedSize = 0;
	for (i = 0; i < count; ++i) {
		SpkgEntry *entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));

		if (entry->status == SPKG_ENTRY_STATUS_SKIPPED)
			continue;
		ret = g_spkgGroupManager->GetParallelOperation()->GetFreeChannel().Pop(
			reinterpret_cast<void **>(&element), true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(85, "%x", ret);
			return ret;
		}
		entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));
		ret = element->SetTask((SceSize)entry->segmentSize, SPKG_OP_UPDATE, this, i);
		if (ret < 0) {
			UPDATER_ERROR_FMT(93, "%x", ret);
			return ret;
		}
		entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));
		ret = ReadSpkg(pupFd, entry, element->GetBuffer());
		if (ret != 0)
			return ret;
		ret = g_spkgGroupManager->GetParallelOperation()->GetFullChannel().Push(
			element, true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(120, "%x", ret);
			return ret;
		}
	}
	return ret;
}

void SpkgGroupFull::EstimateTime(SceUpdaterEstimateTime *estimateTime)
{
	int count = (int)m_spkgList.GetCount();
	SpkgEntry *firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
	float baseMs = ((float)firstEntry->segmentSize / g_spkgReadSpeed + 0.0f) +
		       (float)m_totalSize / g_spkgInspectSpeed;
	float writeSpeed;

	m_estimatedInspectMs = (SceUInt64)(baseMs + (float)count * g_spkgInspectOverheadMs);
	firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
	writeSpeed = GetWriteSpeed(firstEntry);
	m_estimatedUpdateMs = (SceUInt64)((baseMs + (float)m_totalSize / writeSpeed) +
					  (float)count * (g_spkgUpdateOverheadMs +
							  g_spkgInspectOverheadMs));
	estimateTime->inspectMs = m_estimatedInspectMs;
	estimateTime->updateMs = m_estimatedUpdateMs;
}

void SpkgGroupFull::OnInspectSkipped(SpkgEntry *entry)
{
	float perSpkgOverhead = g_spkgUpdateOverheadMs + g_spkgInspectOverheadMs;
	float writeSpeed;
	SceUInt64 skippedMs;
	SceUInt64 delta;

	entry->status = SPKG_ENTRY_STATUS_SKIPPED;
	writeSpeed = GetWriteSpeed(entry);
	skippedMs = (SceUInt64)(perSpkgOverhead + (float)entry->segmentSize / writeSpeed);
	if (skippedMs > m_estimatedUpdateMs) {
		delta = m_estimatedUpdateMs;
		m_estimatedUpdateMs = 0;
	} else {
		m_estimatedUpdateMs -= skippedMs;
		delta = skippedMs;
	}
	g_spkgGroupManager->AdjustEstimatedTime(0, -(SceInt64)delta);
}

void SpkgGroupFull::SetDiffManifestSpkg(const SpkgEntry *entry)
{
	(void)entry;
}

int SpkgGroupFull::convertErrorCodeUpdateResult(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x80180300;
		break;
	case 8:
		base = 0x80180310;
		break;
	case 9:
		base = 0x80180320;
		break;
	case 10:
		base = 0x80180330;
		break;
	case 11:
		base = 0x80180340;
		break;
	case 12:
		base = 0x80180350;
		break;
	case 13:
		base = 0x80180360;
		break;
	case 14:
		base = 0x80180370;
		break;
	case 15:
		base = 0x80180380;
		break;
	case 16:
		base = 0x80180390;
		break;
	case 17:
		base = 0x801803a0;
		break;
	case 18:
		base = 0x801803b0;
		break;
	case 19:
		base = 0x801803c0;
		break;
	case 20:
		base = 0x801803d0;
		break;
	case 23:
		base = 0x80180620;
		break;
	case 24:
		base = 0x801807a0;
		break;
	case 25:
		base = 0x80180820;
		break;
	default:
		return 0;
	}

	switch (error) {
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 5;
	case SCE_SBL_US_ERROR_MISMATCH:
		return base + 6;
	case SCE_SBL_US_ERROR_UP_TO_DATE:
		return base + 7;
	case SCE_SBL_US_ERROR_BAD_VERSION:
		return SCE_SBL_US_ERROR_BAD_VERSION;
	case SCE_SBL_US_ERROR_IO:
		return base + 9;
	case SCE_SBL_US_ERROR_TRY_AGAIN:
		return base + 10;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return base + 11;
	case SCE_SBL_US_ERROR_NOT_FOUND:
		return base + 12;
	case SCE_SBL_US_ERROR_BAD_MAGIC:
		return base + 13;
	case SCE_SBL_US_ERROR_PLATFORM:
		return SCE_SBL_US_ERROR_PLATFORM;
	default:
		return error;
	}
}

int SpkgGroupFull::convertErrorCodeUpdateStatus(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x80180300;
		break;
	case 8:
		base = 0x80180310;
		break;
	case 9:
		base = 0x80180320;
		break;
	case 10:
		base = 0x80180330;
		break;
	case 11:
		base = 0x80180340;
		break;
	case 12:
		base = 0x80180350;
		break;
	case 13:
		base = 0x80180360;
		break;
	case 14:
		base = 0x80180370;
		break;
	case 15:
		base = 0x80180380;
		break;
	case 16:
		base = 0x80180390;
		break;
	case 17:
		base = 0x801803a0;
		break;
	case 18:
		base = 0x801803b0;
		break;
	case 19:
		base = 0x801803c0;
		break;
	case 20:
		base = 0x801803d0;
		break;
	case 23:
		base = 0x80180620;
		break;
	case 24:
		base = 0x801807a0;
		break;
	case 25:
		base = 0x80180820;
		break;
	default:
		return 0;
	}

	switch (error) {
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 1;
	case SCE_SBL_US_ERROR_NOT_FOUND:
		return base + 2;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return base + 3;
	default:
		return error;
	}
}

int SpkgGroupFull::convertErrorCodeUpdateRequest(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x80180130;
		break;
	case 8:
		base = 0x80180140;
		break;
	case 9:
		base = 0x80180150;
		break;
	case 10:
		base = 0x80180160;
		break;
	case 11:
		base = 0x80180170;
		break;
	case 12:
		base = 0x80180180;
		break;
	case 13:
		base = 0x80180190;
		break;
	case 14:
		base = 0x801801a0;
		break;
	case 15:
		base = 0x801801b0;
		break;
	case 16:
		base = 0x801801c0;
		break;
	case 17:
		base = 0x801801d0;
		break;
	case 18:
		base = 0x801801e0;
		break;
	case 19:
		base = 0x801801f0;
		break;
	case 20:
		base = 0x80180200;
		break;
	case 23:
		base = 0x80180600;
		break;
	case 24:
		base = 0x80180780;
		break;
	case 25:
		base = 0x80180800;
		break;
	default:
		return 0;
	}

	switch (error) {
	case SCE_SBL_US_ERROR_NOT_PERMITTED:
		return base + 1;
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 2;
	case SCE_SBL_US_ERROR_INVALID_OP:
		return base + 3;
	case SCE_SBL_US_ERROR_IO:
		return base + 4;
	case SCE_SBL_US_ERROR_TRY_AGAIN:
		return base + 5;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return base + 6;
	case SCE_SBL_US_ERROR_BAD_MAGIC:
		return base + 7;
	case SCE_SBL_US_ERROR_NOT_FOUND:
		return base + 8;
	default:
		return error;
	}
}

int SpkgGroupFull::UpdateSpkg(SpkgOperationElement *element)
{
	SceKernelSysClock startClock;
	SceKernelSysClock endClock;
	int packageType;
	SceUInt32 flags;
	SpkgEntry *entry;
	SceUInt32 requestId;
	int statusResult;
	int ret;

	sceKernelGetSystemTime(&startClock);
	packageType = GetUpdatePackageType(element->GetSpkgIndex());
	flags = GetUpdateFlags();
	entry = static_cast<SpkgEntry *>(m_spkgList.Get(element->GetSpkgIndex()));
	ret = sceSblUsUpdateSpackage(packageType, element->GetBuffer(),
				     (SceSize)entry->segmentSize, flags, &requestId);
	if (ret != 0) {
		UPDATER_ERROR_FMT(166, "%d %lld, %x : %x", packageType, entry->segmentSize,
				  flags, ret);
		return convertErrorCodeUpdateRequest(packageType, ret);
	}
	ret = WaitStatus(SPKG_NODE_TYPE_UPDATE, requestId, &statusResult);
	if (ret != 0)
		return convertErrorCodeUpdateStatus(packageType, ret);
	if (statusResult != 0)
		return convertErrorCodeUpdateResult(packageType, statusResult);
	entry->status = SPKG_ENTRY_STATUS_UPDATED;
	m_updatedCount++;
	sceKernelGetSystemTime(&endClock);
	{
		SceUInt64 elapsedMs = (endClock.quad - startClock.quad) / 1000;
		float inspectPart = (float)entry->segmentSize / g_spkgInspectSpeed;
		float writeSpeed = GetWriteSpeed(entry);
		float expectedMs = ((inspectPart + (float)entry->segmentSize / writeSpeed) +
				    g_spkgInspectOverheadMs) + g_spkgUpdateOverheadMs;
		float delta = (float)elapsedMs - expectedMs;

		g_spkgGroupManager->AdjustEstimatedTime(0, (SceInt64)delta);
	}
	return 0;
}
