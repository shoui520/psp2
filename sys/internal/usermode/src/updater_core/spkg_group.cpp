/*
 * SpkgGroup (Sony's spkg_group.cpp, 0x810061e8..0x81009393): base SPKG group handling
 * PUP reads, inspection, chunk ordering, progress callbacks and SceSblSsUpdateMgr status
 * polling / error mapping.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>
#include "updater_array.h"
#include "updater_log.h"
#include "updater_memory.h"
#include "updater_progress.h"
#include "spkg_group.h"
#include "spkg_group_manager.h"

#define UPDATER_FILE_ID			0xb4624c3a

#define SPKG_READ_BUFFER_ALIGN		512
#define SPKG_READ_BUFFER_SIZE		0x40000
#define SPKG_SECTOR_SHIFT		9
#define SPKG_STATUS_POLL_DELAY_US	10000
#define SPKG_STATUS_POLL_STEP_MS	10
#define SPKG_STATUS_PROGRESS_DONE	5
#define SPKG_PROGRESS_TYPE_INSPECT	5
#define SPKG_PROGRESS_TYPE_UPDATE	6

#define SCE_SBL_US_ERROR_INVALID_OP	((int)0x800f0210)
#define SCE_SBL_US_ERROR_UP_TO_DATE	((int)0x800f0236)

/* shared speed and overhead constants referenced by SpkgGroup, SpkgGroupDiff and SpkgGroupFull */
const float g_spkgReadSpeed = 16779.12f;
const float g_spkgInspectSpeed = 12623.78f;
const float g_spkgCompressedWriteSpeed = 4877.99f;
const float g_spkgInspectOverheadMs = 20.3f;
const float g_spkgUpdateOverheadMs = 180.0f;

#ifdef UPDATER_CORE_2014
const float g_spkgWriteSpeedTable[29] = {
#else
const float g_spkgWriteSpeedTable[26] = {
#endif
	1.0f,		/* 0 */
	4930.52f,	/* 1 */
	1.0f,		/* 2 */
	1.0f,		/* 3 */
	1.0f,		/* 4 */
	1.0f,		/* 5 */
	1.0f,		/* 6 */
	1.0f,		/* 7 */
	1.13f,		/* 8 */
	1413.23f,	/* 9 */
	4930.52f,	/* 10 */
	175.37f,	/* 11 */
	3.17f,		/* 12 */
	78.0f,		/* 13 */
	1.0f,		/* 14 */
	1.0f,		/* 15 */
	0.87f,		/* 16 */
	0.39f,		/* 17 */
	1.0f,		/* 18 */
	1.0f,		/* 19 */
	1.0f,		/* 20 */
	1.0f,		/* 21 */
	1.0f,		/* 22 */
	4930.52f,	/* 23 */
	4930.52f,	/* 24 */
	1.0f,		/* 25 */
#ifdef UPDATER_CORE_2014
	1.0f,		/* 26 */
	1.0f,		/* 27 */
	1.0f,		/* 28 */
#endif
};

SceUpdaterProgressCallback g_updaterProgressCallback = NULL;
SceUInt32 g_updaterProgressInterval = 0;
static SceUInt32 s_progressElapsedMs = 0;

SpkgGroup::SpkgGroup()
	: m_spkgList(4)
{
	m_inspectedCount = 0;
	m_spkgList.Clear();
	m_totalSize = 0;
	m_inspectedSize = 0;
	m_processedSize = 0;
}

SpkgGroup::~SpkgGroup()
{
	int count = (int)m_spkgList.GetCount();
	int i;

	for (i = 0; i < count; ++i)
		g_updaterMemoryInfo.updaterCoreFree(m_spkgList.Get(i));
	m_spkgList.Clear();
}

int SpkgGroup::AddSpkg(SpkgEntry *entry)
{
	SpkgEntry *copy;

	m_totalSize += entry->segmentSize;
	entry->status = SPKG_ENTRY_STATUS_INITIAL;
	copy = static_cast<SpkgEntry *>(g_updaterMemoryInfo.updaterCoreAlloc(sizeof(SpkgEntry)));
	sceClibMemcpy(copy, entry, sizeof(SpkgEntry));
	return m_spkgList.Push(copy);
}

int SpkgGroup::ReadSpkg(SceUID pupFd, const SpkgEntry *entry, void *buffer)
{
	SceUInt64 alignedOffset = (SceUInt32)((entry->segmentOffset >> SPKG_SECTOR_SHIFT)
					      << SPKG_SECTOR_SHIFT);
	SceUInt8 *tempBuffer = NULL;
	SceOff currentOffset = (SceOff)alignedOffset;
	SceUInt32 totalBytes;
	SceUInt32 remaining;
	SceSize chunkSize;
	SceUInt8 *dst = static_cast<SceUInt8 *>(buffer);
	SceSSize readLen;
	SceSize copied;

	if (alignedOffset != entry->segmentOffset) {
		tempBuffer = static_cast<SceUInt8 *>(
			g_updaterMemoryInfo.updaterCoreMemalign(SPKG_READ_BUFFER_ALIGN,
								SPKG_READ_BUFFER_SIZE));
		if (tempBuffer == NULL) {
			UPDATER_ERROR_FMT(378, "%d %d", SPKG_READ_BUFFER_ALIGN,
					  SPKG_READ_BUFFER_SIZE);
			return SCE_UPDATER_ERROR_NO_MEMORY;
		}
	}
	totalBytes = (SceUInt32)(entry->segmentSize + (entry->segmentOffset - alignedOffset));
	remaining = totalBytes;
	chunkSize = remaining < SPKG_READ_BUFFER_SIZE ? remaining : SPKG_READ_BUFFER_SIZE;
	for (;;) {
		if (tempBuffer != NULL) {
			readLen = sceIoPread(pupFd, tempBuffer, chunkSize, currentOffset);
			if (readLen < 0) {
				UPDATER_ERROR_FMT(397, "%x", readLen);
				g_updaterMemoryInfo.updaterCoreFree(tempBuffer);
				return SCE_UPDATER_ERROR_SPKG_PREAD;
			}
			if (remaining == totalBytes) {
				SceUInt32 lead = (SceUInt32)(entry->segmentOffset - alignedOffset);

				copied = chunkSize - lead;
				sceClibMemcpy(dst, tempBuffer + lead, copied);
			} else {
				copied = chunkSize;
				sceClibMemcpy(dst, tempBuffer, copied);
			}
		} else {
			readLen = sceIoPread(pupFd, dst, chunkSize, currentOffset);
			if (readLen < 0) {
				UPDATER_ERROR_FMT(418, "%x", readLen);
				g_updaterMemoryInfo.updaterCoreFree(tempBuffer);
				return SCE_UPDATER_ERROR_SPKG_PREAD;
			}
			copied = chunkSize;
		}
		if (chunkSize < SPKG_READ_BUFFER_SIZE)
			break;
		remaining -= SPKG_READ_BUFFER_SIZE;
		if (remaining < SPKG_READ_BUFFER_SIZE)
			chunkSize = remaining;
		currentOffset += readLen;
		dst += copied;
	}
	if (tempBuffer != NULL)
		g_updaterMemoryInfo.updaterCoreFree(tempBuffer);
	return 0;
}

int SpkgGroup::EnqueueInspect(SceUID pupFd)
{
	int count = (int)m_spkgList.GetCount();
	SpkgOperationElement *element = NULL;
	int ret = 0;
	int i;

	m_inspectedCount = 0;
	m_inspectedSize = 0;
	m_processedSize = 0;
	for (i = 0; i < count; ++i) {
		SpkgEntry *entry;

		ret = g_spkgGroupManager->GetParallelOperation()->GetFreeChannel().Pop(
			reinterpret_cast<void **>(&element), true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(173, "%x", ret);
			return ret;
		}
		entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));
		ret = element->SetTask((SceSize)entry->segmentSize, SPKG_OP_INSPECT, this, i);
		if (ret < 0) {
			UPDATER_ERROR_FMT(181, "%x", ret);
			return ret;
		}
		entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));
		ret = ReadSpkg(pupFd, entry, element->GetBuffer());
		if (ret != 0)
			return ret;
		ret = g_spkgGroupManager->GetParallelOperation()->GetFullChannel().Push(
			element, true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(207, "%x", ret);
			return ret;
		}
	}
	return ret;
}

void SpkgGroup::PrepareUpdateOrder(bool *hasPending)
{
	int count = (int)m_spkgList.GetCount();
	UpdaterArray hardwareIds(count);
	UpdaterArray snapshot(count);

	*hasPending = false;
	if (count <= 1) {
		SpkgEntry *entry = static_cast<SpkgEntry *>(m_spkgList.Get(0));

		if (entry->status == SPKG_ENTRY_STATUS_INSPECTED)
			*hasPending = true;
		return;
	}
	for (int i = 0; i < count; ++i) {
		SpkgEntry *entry = static_cast<SpkgEntry *>(m_spkgList.Get(i));

		if (entry->status != SPKG_ENTRY_STATUS_SKIPPED) {
			int uniqueCount = (int)hardwareIds.GetCount();
			int j;

			for (j = 0; j < uniqueCount; ++j) {
				if (*static_cast<SceUInt64 *>(hardwareIds.Get(j)) ==
				    entry->hardwareInfo)
					break;
			}
			if (j >= uniqueCount) {
				SceUInt64 *hwId = static_cast<SceUInt64 *>(
					g_updaterMemoryInfo.updaterCoreAlloc(sizeof(SceUInt64)));
				*hwId = entry->hardwareInfo;
				hardwareIds.Push(hwId);
			}
		}
	}
	int uniqueCount = (int)hardwareIds.GetCount();
	if (uniqueCount <= 0) {
		*hasPending = false;
		return;
	}
	for (int i = 0; i < count; ++i) {
		SpkgEntry *copy = static_cast<SpkgEntry *>(
			g_updaterMemoryInfo.updaterCoreAlloc(sizeof(SpkgEntry)));
		sceClibMemcpy(copy, m_spkgList.Get(i), sizeof(SpkgEntry));
		snapshot.Push(copy);
		g_updaterMemoryInfo.updaterCoreFree(m_spkgList.Get(i));
	}
	m_spkgList.Clear();
	for (int h = 0; h < uniqueCount; ++h) {
		SceUInt64 expectedChunk = 1;

		for (int i = 0; i < count; ++i) {
			SpkgEntry *entry = static_cast<SpkgEntry *>(snapshot.Get(i));

			if (entry->status != SPKG_ENTRY_STATUS_SKIPPED &&
			    *static_cast<SceUInt64 *>(hardwareIds.Get(h)) == entry->hardwareInfo &&
			    entry->chunkIndex == expectedChunk) {
				m_spkgList.Push(snapshot.Get(i));
				expectedChunk++;
				i = -1;
			}
		}
	}
	for (int i = 0; i < count; ++i) {
		SpkgEntry *entry = static_cast<SpkgEntry *>(snapshot.Get(i));

		if (entry->status == SPKG_ENTRY_STATUS_SKIPPED)
			m_spkgList.Push(snapshot.Get(i));
	}
	for (int h = 0; h < uniqueCount; ++h)
		g_updaterMemoryInfo.updaterCoreFree(hardwareIds.Get(h));
	hardwareIds.Clear();
	*hasPending = true;
}

int SpkgGroup::GetType(int *type)
{
	if (m_spkgList.GetCount() == 0)
		return SCE_UPDATER_ERROR_NOT_FOUND;
	*type = (int)static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
	return 0;
}

int SpkgGroup::GetInspectedCount(int *count)
{
	if (m_spkgList.GetCount() == 0)
		return SCE_UPDATER_ERROR_NOT_FOUND;
	*count = m_inspectedCount;
	return 0;
}

int SpkgGroup::GetVersion(SceUInt32 *version)
{
	if (m_spkgList.GetCount() == 0)
		return SCE_UPDATER_ERROR_NOT_FOUND;
	*version = (SceUInt32)static_cast<SpkgEntry *>(m_spkgList.Get(0))->version;
	return 0;
}

int SpkgGroup::GetBuildNo(SceUInt32 *buildNo)
{
	if (m_spkgList.GetCount() == 0)
		return SCE_UPDATER_ERROR_NOT_FOUND;
	*buildNo = (SceUInt32)static_cast<SpkgEntry *>(m_spkgList.Get(0))->buildNo;
	return 0;
}

float SpkgGroup::GetWriteSpeed(const SpkgEntry *entry)
{
	float speed = g_spkgWriteSpeedTable[entry->type];

	if (entry->decryptedSize != entry->finalSize)
		speed = g_spkgCompressedWriteSpeed;
	return speed;
}

int SpkgGroup::GetUpdatePackageType(int index)
{
	int packageType = 0;

	switch (static_cast<SpkgEntry *>(m_spkgList.Get(index))->type) {
	case 9:
		packageType = 9;
		break;
	case 1:
		packageType = 1;
		break;
	case 10:
		packageType = 10;
		break;
	case 12:
		packageType = 12;
		break;
	case 13:
		packageType = 13;
		break;
	case 14:
		packageType = 14;
		break;
	case 11:
		packageType = 11;
		break;
	case 8:
		packageType = 8;
		break;
	case 15:
		packageType = 15;
		break;
	case 16:
		packageType = 16;
		break;
	case 17:
		packageType = 17;
		break;
	case 18:
		packageType = 18;
		break;
	case 19:
		packageType = 19;
		break;
	case 20:
		packageType = 20;
		break;
	case 21:
		packageType = 21;
		break;
	case 22:
		packageType = 22;
		break;
	case 23:
		packageType = 23;
		break;
	case 24:
		packageType = 24;
		break;
	case 3:
		packageType = 3;
		break;
	case 25:
		packageType = 25;
		break;
#ifdef UPDATER_CORE_2014
	case 28:
		packageType = 28;
		break;
#endif
	default:
		break;
	}
	return packageType;
}

SceUInt32 SpkgGroup::GetUpdateFlags(void)
{
	SceUInt32 flags = 5;

	if (vshSblAimgrIsCEX() == 1)
		flags = 9;
	return flags;
}

void SpkgGroup::NotifyProgress(int operation)
{
	if (g_updaterProgressCallback == NULL)
		return;
	s_progressElapsedMs += SPKG_STATUS_POLL_STEP_MS;
	if (s_progressElapsedMs >= g_updaterProgressInterval) {
		SceUpdaterProgress progress;
		int ret;
		int callbackType;

		s_progressElapsedMs = 0;
		ret = sceUpdaterGetProgress(&progress);
		if (ret != 0) {
			UPDATER_ERROR_FMT(1205, "%x", ret);
			return;
		}
		if (operation == SPKG_OP_UPDATE)
			callbackType = SPKG_PROGRESS_TYPE_UPDATE;
		else if (operation == SPKG_OP_INSPECT)
			callbackType = SPKG_PROGRESS_TYPE_INSPECT;
		else
			return;
		g_updaterProgressCallback(callbackType, 0, progress);
	}
}

int SpkgGroup::WaitStatus(int nodeType, SceUInt32 requestId, int *statusResult)
{
	SceUInt32 seqNo;
	SceUInt32 result;
	SceUInt32 progress;
	SceUInt32 writtenRates;
	int ret;

	*statusResult = 0;
	for (;;) {
		ret = sceKernelDelayThread(SPKG_STATUS_POLL_DELAY_US);
		if (ret != 0) {
			UPDATER_ERROR_FMT(1084, "%d : %x", SPKG_STATUS_POLL_DELAY_US, ret);
			return ret;
		}
		ret = sceSblUsGetStatus(nodeType, requestId, &seqNo, &result,
					&progress, &writtenRates);
		if (ret != 0) {
			UPDATER_ERROR_FMT(1091, "%d %d  %d %x %d %d : %x",
					  nodeType, requestId, seqNo, result,
					  progress, writtenRates, ret);
			return ret;
		}
		if (progress == SPKG_STATUS_PROGRESS_DONE)
			break;
		NotifyProgress(nodeType);
	}
	if (result != 0)
		*statusResult = (int)result;
	return 0;
}

int SpkgGroup::ConvertErrorCodeInspectResult(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x801803f0;
		break;
	case 4:
		base = 0x801806a0;
		break;
	case 8:
		base = 0x80180400;
		break;
	case 9:
		base = 0x80180410;
		break;
	case 10:
		base = 0x80180420;
		break;
	case 11:
		base = 0x80180430;
		break;
	case 12:
		base = 0x80180440;
		break;
	case 13:
		base = 0x80180450;
		break;
	case 14:
		base = 0x80180460;
		break;
	case 15:
		base = 0x80180470;
		break;
	case 16:
		base = 0x80180480;
		break;
	case 17:
		base = 0x80180490;
		break;
	case 18:
		base = 0x801804a0;
		break;
	case 19:
		base = 0x801804b0;
		break;
	case 20:
		base = 0x801804c0;
		break;
	case 21:
		base = 0x80180660;
		break;
	case 22:
		base = 0x80180680;
		break;
	case 23:
		base = 0x80180630;
		break;
	case 24:
		base = 0x801807b0;
		break;
	case 25:
		base = 0x80180830;
		break;
	default:
		return 0;
	}

	switch (error) {
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 5;
	case SCE_SBL_US_ERROR_MISMATCH:
		/* Sony's original binary has a typo in case 8: 0x8025bfa6 (-2145009754)
		   instead of 0x80180406 (-2145909754) */
		if (packageType == 8)
			return (int)0x8025bfa6;
		return base + 6;
	case SCE_SBL_US_ERROR_UP_TO_DATE:
		return error;
	case SCE_SBL_US_ERROR_BAD_VERSION:
		return error;
	case SCE_SBL_US_ERROR_IO:
		return base + 9;
	case SCE_SBL_US_ERROR_TRY_AGAIN:
		return base + 10;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return base + 11;
	case SCE_SBL_US_ERROR_NOT_FOUND:
		return error;
	case SCE_SBL_US_ERROR_BAD_MAGIC:
		return base + 13;
	case SCE_SBL_US_ERROR_PLATFORM:
		return error;
	default:
		return error;
	}
}

int SpkgGroup::ConvertErrorCodeInspectStatus(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x801803f0;
		break;
	case 4:
		base = 0x801806a0;
		break;
	case 8:
		base = 0x80180400;
		break;
	case 9:
		base = 0x80180410;
		break;
	case 10:
		base = 0x80180420;
		break;
	case 11:
		base = 0x80180430;
		break;
	case 12:
		base = 0x80180440;
		break;
	case 13:
		base = 0x80180450;
		break;
	case 14:
		base = 0x80180460;
		break;
	case 15:
		base = 0x80180470;
		break;
	case 16:
		base = 0x80180480;
		break;
	case 17:
		base = 0x80180490;
		break;
	case 18:
		base = 0x801804a0;
		break;
	case 19:
		base = 0x801804b0;
		break;
	case 20:
		base = 0x801804c0;
		break;
	case 21:
		base = 0x80180660;
		break;
	case 22:
		base = 0x80180680;
		break;
	case 23:
		base = 0x80180630;
		break;
	case 24:
		base = 0x801807b0;
		break;
	case 25:
		base = 0x80180830;
		break;
	default:
		return 0;
	}

	switch (error) {
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 1;
	case SCE_SBL_US_ERROR_NOT_FOUND:
		if (packageType >= 14 && packageType <= 20)
			return base + 3;
		return base + 2;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		if (packageType >= 14 && packageType <= 20)
			return base + 2;
		if (packageType == 22)
			return base;
		return base + 3;
	default:
		return error;
	}
}

int SpkgGroup::ConvertErrorCodeInspectRequest(int packageType, int error)
{
	int base;

	switch (packageType) {
	case 1:
		base = 0x80180210;
		break;
	case 4:
		base = 0x80180690;
		break;
	case 8:
		base = 0x80180220;
		break;
	case 9:
		base = 0x80180230;
		break;
	case 10:
		base = 0x80180240;
		break;
	case 11:
		base = 0x80180250;
		break;
	case 12:
		base = 0x80180260;
		break;
	case 13:
		base = 0x80180270;
		break;
	case 14:
		base = 0x80180280;
		break;
	case 15:
		base = 0x80180290;
		break;
	case 16:
		base = 0x801802a0;
		break;
	case 17:
		base = 0x801802b0;
		break;
	case 18:
		base = 0x801802c0;
		break;
	case 19:
		base = 0x801802d0;
		break;
	case 20:
		base = 0x801802e0;
		break;
	case 21:
		base = 0x80180650;
		break;
	case 22:
		base = 0x80180670;
		break;
	case 23:
		base = 0x80180610;
		break;
	case 24:
		base = 0x80180790;
		break;
	case 25:
		base = 0x80180810;
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
	default:
		return error;
	}
}

int SpkgGroup::InspectSpkg(SpkgOperationElement *element)
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
	ret = sceSblUsInspectSpackage(packageType, element->GetBuffer(),
				      (SceSize)entry->segmentSize, flags, &requestId);
	if (ret != 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(258, "%d %lld %x : %x", packageType, entry->segmentSize,
				  flags, ret);
#else
		UPDATER_ERROR_FMT(253, "%d %lld %x : %x", packageType, entry->segmentSize,
				  flags, ret);
#endif
		ret = ConvertErrorCodeInspectRequest(packageType, ret);
		if ((ret != SCE_SBL_US_ERROR_BAD_VERSION ||
		     (packageType != 9 && packageType != 1 && packageType != 10 &&
		      packageType != 21 && packageType != 22)) &&
		    (ret == SCE_SBL_US_ERROR_UP_TO_DATE || ret == SCE_SBL_US_ERROR_NOT_FOUND ||
		     ret == SCE_SBL_US_ERROR_BAD_VERSION)) {
			OnInspectSkipped(entry);
			m_processedSize += entry->segmentSize;
			return 0;
		}
		return ret;
	}
	ret = WaitStatus(SPKG_OP_INSPECT, requestId, &statusResult);
	if (ret != 0)
		return ConvertErrorCodeInspectStatus(packageType, ret);
	if (statusResult != 0) {
		ret = ConvertErrorCodeInspectResult(packageType, statusResult);
		if ((ret != SCE_SBL_US_ERROR_BAD_VERSION ||
		     (packageType != 9 && packageType != 1 && packageType != 10 &&
		      packageType != 21 && packageType != 22)) &&
		    (ret == SCE_SBL_US_ERROR_UP_TO_DATE || ret == SCE_SBL_US_ERROR_NOT_FOUND ||
		     ret == SCE_SBL_US_ERROR_BAD_VERSION)) {
			OnInspectSkipped(entry);
			m_processedSize += entry->segmentSize;
			return 0;
		}
		return ret;
	}
	entry->status = SPKG_ENTRY_STATUS_INSPECTED;
	m_inspectedCount++;
	m_inspectedSize += entry->segmentSize;
	m_processedSize += entry->segmentSize;
	sceKernelGetSystemTime(&endClock);
	{
		SceUInt64 elapsedMs = (endClock.quad - startClock.quad) / 1000;
		float delta = (float)elapsedMs -
			      ((float)entry->segmentSize / g_spkgInspectSpeed +
			       g_spkgInspectOverheadMs);

		g_spkgGroupManager->AdjustEstimatedTime((SceInt64)delta, 0);
	}
	return 0;
}
