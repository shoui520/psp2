/*
 * SpkgGroupManager, SpkgParallelOperation and SpkgOperationElement (Sony's
 * spkg_group_manager.cpp): parses a PUP's SPKG segments into component groups,
 * estimates inspection and update durations, and runs inspection and update in
 * parallel across two threads.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <update_mgr/pup_file.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "spkg_group.h"
#include "spkg_group_diff.h"
#include "spkg_group_full.h"
#include "spkg_group_manager.h"
#ifdef UPDATER_CORE_2014
#include "update_utility.h"
#endif

#define UPDATER_FILE_ID			0xf1080714

#define PUP_PARSE_BUFFER_ALIGN		512
#define PUP_PARSE_BUFFER_SIZE		0x40000
#define PUP_SECTOR_SHIFT		9
#define PUP_DIFF_MANIFEST_SEG_MASK	(~0xfULL)
#define PUP_DIFF_MANIFEST_SEG_BASE	0x210ULL
#define PUP_SPKG_SEG_MASK		(~0xffULL)
#define PUP_SPKG_SEG_BASE		0x300ULL

#define SPKG_TYPE_DIFF_MANIFEST		4
#ifdef UPDATER_CORE_2014
#define SPKG_DIFF_GROUP_MANIFEST_INDEX	22
#define SPKG_GROUP_INDEX_OS0		2
#define SPKG_GROUP_INDEX_OS0_EXT	3
#define SPKG_GROUP_INDEX_VS0		5
#define SPKG_GROUP_INDEX_VS0_EXT	6
#define SPKG_GROUP_INDEX_PD0		9
#else
#define SPKG_DIFF_GROUP_MANIFEST_INDEX	19
#define SPKG_GROUP_INDEX_OS0		2
#define SPKG_GROUP_INDEX_VS0		4
#endif

#define VSH_MOUNT_ID_VS0		0x300
#define VSH_MOUNT_ID_SA0		0xb00
#define VSH_MOUNT_ID_PD0		0xc00

#define MIN_COMPRESSED_SPKG_VERSION	0x01000000U
#define INSPECT_BASE_OVERHEAD_MS	400ULL

/* certified-file header (0x30 bytes) preceding an SPKG payload inside a PUP segment */
struct CertifiedFileHeader {
	SceUInt32 magic;
	SceUInt32 version;
	SceUInt16 sdkType;
	SceUInt16 headerType;
	SceUInt32 metadataOffset;
	SceUInt64 fileOffset;
	SceUInt64 fileSize;
	SceUInt64 headerSize;
	SceUInt64 segmentCount;
};

/* whether a given SPKG type uses differential update (SpkgGroupDiff) */
#ifdef UPDATER_CORE_2014
static const SceUInt8 s_isDiffSpkgType[29] = {
	0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 0,
	0, 0, 0, 0, 1,
};
#else
static const SceUInt8 s_isDiffSpkgType[26] = {
	0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 1, 0,
	0, 0,
};
#endif

static int s_updatePhaseCount = 0;

SpkgOperationElement::SpkgOperationElement()
{
	m_length = 0;
	m_buffer = NULL;
	m_bufferSize = 0;
	m_group = NULL;
	m_operation = SPKG_OP_NONE;
	m_spkgIndex = -1;
}

int SpkgOperationElement::ReleaseBuffer(void)
{
	int ret = 0;

	if (m_buffer != NULL) {
		ret = sceSblUsReleaseBuffer(m_buffer);
		if (ret != 0) {
#ifdef UPDATER_CORE_2014
			UPDATER_ERROR_FMT(1421, "%x", ret);
#else
			UPDATER_ERROR_FMT(1288, "%x", ret);
#endif
			if (ret == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
				ret = SCE_UPDATER_ERROR_FREE_BUF_INVALID_ARG;
			else if (ret == SCE_SBL_US_ERROR_NOT_PERMITTED)
				ret = SCE_UPDATER_ERROR_FREE_BUF_NOT_PERMIT;
		}
		m_buffer = NULL;
		m_bufferSize = 0;
	}
	return ret;
}

SpkgOperationElement::~SpkgOperationElement()
{
	ReleaseBuffer();
}

int SpkgOperationElement::AllocateBuffer(SceSize size)
{
	int ret = 0;

	if (size != m_bufferSize) {
		ReleaseBuffer();
		if (size != 0) {
			ret = sceSblUsAllocateBuffer(size, &m_buffer);
			if (ret != 0) {
#ifdef UPDATER_CORE_2014
				UPDATER_ERROR_FMT(1389, "%d : %x", size, ret);
#else
				UPDATER_ERROR_FMT(1256, "%d : %x", size, ret);
#endif
				switch (ret) {
				case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
					ret = SCE_UPDATER_ERROR_ALLOC_BUF_INVALID_ARG;
					break;
				case SCE_SBL_US_ERROR_NO_MEMORY:
					ret = SCE_UPDATER_ERROR_ALLOC_BUF_NO_MEMORY;
					break;
				case SCE_SBL_US_ERROR_NOT_PERMITTED:
					ret = SCE_UPDATER_ERROR_ALLOC_BUF_NOT_PERMIT;
					break;
				default:
					break;
				}
				m_buffer = NULL;
				return ret;
			}
		}
		m_bufferSize = size;
	}
	return ret;
}

int SpkgOperationElement::SetTask(SceSize bufferSize, int operation, SpkgGroup *group,
				  int spkgIndex)
{
	int ret;

	ret = AllocateBuffer(bufferSize);
	if (ret < 0)
		return ret;
	m_operation = operation;
	m_group = group;
	m_spkgIndex = spkgIndex;
	return 0;
}

int SpkgOperationElement::Operate(void)
{
	int spkgType = 0;
	int ret;

	m_group->GetType(&spkgType);
	switch (m_operation) {
	case SPKG_OP_UPDATE:
		sceClibPrintf("[INFO]writing system firmware phase %d(0x%x)\n",
			      s_updatePhaseCount + 1, spkgType);
		ret = m_group->UpdateSpkg(this);
		if (ret >= 0) {
			sceClibPrintf("[INFO]writing system firmware phase %d done\n",
				      s_updatePhaseCount + 1);
		}
		break;
	case SPKG_OP_INSPECT:
		ret = m_group->InspectSpkg(this);
		break;
	default:
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR(1364);
#else
		UPDATER_ERROR(1231);
#endif
		ret = SCE_UPDATER_ERROR_INVALID_ARGUMENT;
		break;
	}
	s_updatePhaseCount++;
	return ret;
}

int SpkgParallelOperation::CreateElement(ParallelOperationElement **element)
{
	SpkgOperationElement *created = new SpkgOperationElement();

	*element = created;
	if (created == NULL)
		return SCE_UPDATER_ERROR_NO_MEMORY;
	return 0;
}

void SpkgGroupManager::recordUpdatedComponent(int spkgType)
{
	switch (spkgType) {
	case 1:
		m_updatedComponentMask |= 1U;
		break;
	case 11:
		m_updatedComponentMask |= 2U;
		break;
	case 8:
		m_updatedComponentMask |= 4U;
		break;
	case 12:
		m_updatedComponentMask |= 8U;
		break;
	default:
		break;
	}
}

int SpkgGroupManager::updateAllGroups(SceUID pupFd)
{
	int ret = 0;
	int count = (int)m_activeGroups.GetCount();
	int spkgType = 0;
	int i;

	for (i = 0; i < count; ++i) {
		m_currentGroup = static_cast<SpkgGroup *>(m_activeGroups.Get(i));
		ret = m_currentGroup->GetType(&spkgType);
		if (ret != 0) {
			m_currentGroup = NULL;
			return ret;
		}
		ret = m_currentGroup->EnqueueUpdate(pupFd);
		if (ret != 0) {
			sceClibPrintf("[ERROR]writing system firmware phase %d failure = 0x%x\n",
				      i + 1, ret);
			m_updatedComponentMask = 3;
			return ret;
		}
		recordUpdatedComponent(spkgType);
	}
	return ret;
}

int SpkgGroupManager::inspectAllGroups(SceUID pupFd)
{
	int ret = 0;
	int i;

	m_totalInspectedCount = 0;
	for (i = 0; i < SPKG_GROUP_MAX; ++i) {
		if (m_groups[i] != NULL) {
			ret = m_groups[i]->EnqueueInspect(pupFd);
			if (ret != 0)
				break;
		}
	}
	return ret;
}

int SpkgParallelOperation::mainOperation(void *arg)
{
	int ret;

	if (arg != NULL)
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR(1449);
#else
		UPDATER_ERROR(1316);
#endif
	switch (m_operation) {
	case SPKG_OP_UPDATE:
		ret = g_spkgGroupManager->updateAllGroups(m_pupFd);
		break;
	case SPKG_OP_INSPECT:
		ret = g_spkgGroupManager->inspectAllGroups(m_pupFd);
		break;
	default:
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(1464, "%d", m_operation);
#else
		UPDATER_ERROR_FMT(1331, "%d", m_operation);
#endif
		ret = SCE_UPDATER_ERROR_INVALID_ARGUMENT;
		break;
	}
	m_operation = SPKG_OP_NONE;
	return ret;
}

SpkgGroupManager::SpkgGroupManager(int reserved)
	: m_activeGroups(4)
{
	(void)reserved;
	m_currentGroup = NULL;
	m_initialized = false;
	m_parallelOp = NULL;
	m_updatedComponentMask = 3;
	m_totalInspectedCount = 0;
	m_hasDiffGroup = 0;
	m_activeGroups.Clear();
	m_estimatedInspectMs = 0;
	m_estimatedUpdateMs = 0;
	m_inspectStartTime.quad = 0;
	m_updateStartTime.quad = 0;
	sceClibMemset(m_groups, 0, sizeof(m_groups));
	sceClibMemset(&m_lastProgress, 0, sizeof(m_lastProgress));
	m_lastProgress.inspectRemainingMs = ~0ULL;
	m_lastProgress.updateRemainingMs = ~0ULL;
}

void SpkgGroupManager::Terminate(void)
{
	int i;

	for (i = 0; i < SPKG_GROUP_MAX; ++i) {
		if (m_groups[i] != NULL) {
			delete m_groups[i];
			m_groups[i] = NULL;
		}
	}
	m_activeGroups.Clear();
#ifdef UPDATER_CORE_2014
	m_initialized = false;
#endif
	m_estimatedInspectMs = 0;
	m_estimatedUpdateMs = 0;
}

SpkgGroupManager::~SpkgGroupManager()
{
	Terminate();
	if (m_parallelOp != NULL) {
		delete m_parallelOp;
		m_parallelOp = NULL;
	}
}

int SpkgGroupManager::unmountPartitions(void)
{
	int ret;

	ret = vshIoUmount(VSH_MOUNT_ID_VS0, 1, 0, 0);
	if (ret != 0 && ret != (int)SCE_ERROR_ERRNO_ENOENT) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(202, "%x", ret);
#else
		UPDATER_ERROR_FMT(198, "%x", ret);
#endif
		return ret;
	}
	ret = vshIoUmount(VSH_MOUNT_ID_SA0, 1, 0, 0);
	if (ret != 0 && ret != (int)SCE_ERROR_ERRNO_ENOENT) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(214, "%x", ret);
#else
		UPDATER_ERROR_FMT(210, "%x", ret);
#endif
		return ret;
	}
	ret = vshIoUmount(VSH_MOUNT_ID_PD0, 1, 0, 0);
	if (ret != 0 && ret != (int)SCE_ERROR_ERRNO_ENOENT) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(226, "%x", ret);
#else
		UPDATER_ERROR_FMT(222, "%x", ret);
#endif
		return ret;
	}
	return 0;
}

int SpkgGroupManager::UpdateAllSpkg(const char *fileName)
{
	int ret;
	SceUID fd;
	int closeRet;

	sceKernelGetSystemTime(&m_updateStartTime);
	ret = unmountPartitions();
	if (ret != 0)
		return ret;
	s_updatePhaseCount = 0;
	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(150, "%s %o : %x", fileName, 0, fd);
#else
		UPDATER_ERROR_FMT(146, "%s %o : %x", fileName, 0, fd);
#endif
		return SCE_UPDATER_ERROR_SPKG_OPEN;
	}
	if (m_parallelOp != NULL) {
		m_parallelOp->SetOperation(SPKG_OP_UPDATE, fd);
		ret = m_parallelOp->ExecuteOperation(NULL);
	} else {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR(157);
#else
		UPDATER_ERROR(153);
#endif
	}
	closeRet = sceIoClose(fd);
	if (closeRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(170, "%x", closeRet);
#else
		UPDATER_ERROR_FMT(166, "%x", closeRet);
#endif
		if (ret == 0)
			return SCE_UPDATER_ERROR_SPKG_CLOSE;
	}
	return ret;
}

void SpkgGroupManager::calculateEstimateTime(void)
{
	SceUpdaterEstimateTime groupTime;
	int i;

	for (i = 0; i < SPKG_GROUP_MAX; ++i) {
		if (m_groups[i] != NULL) {
			m_groups[i]->EstimateTime(&groupTime);
			m_estimatedInspectMs += groupTime.inspectMs;
			m_estimatedUpdateMs += groupTime.updateMs;
		}
	}
	m_estimatedInspectMs += INSPECT_BASE_OVERHEAD_MS;
}

int SpkgGroupManager::getGroupIndex(int spkgType)
{
	int index = -1;

#ifdef UPDATER_CORE_2014
	switch (spkgType) {
	case 9:
		index = 0;
		break;
	case 1:
		index = 1;
		break;
	case 21:
		index = 2;
		break;
	case 10:
		index = 4;
		break;
	case 22:
		index = 5;
		break;
	case 23:
		index = 7;
		break;
	case 24:
		index = 8;
		break;
	case 28:
		index = 9;
		break;
	case 14:
		index = 10;
		break;
	case 12:
		index = 11;
		break;
	case 15:
		index = 12;
		break;
	case 16:
		index = 13;
		break;
	case 17:
		index = 14;
		break;
	case 13:
		index = 15;
		break;
	case 18:
		index = 16;
		break;
	case 19:
		index = 17;
		break;
	case 11:
		index = 18;
		break;
	case 20:
		index = 19;
		break;
	case 25:
		index = 20;
		break;
	case 8:
		index = 21;
		break;
	case 4:
		index = 22;
		break;
	default:
		break;
	}
#else
	switch (spkgType) {
	case 9:
		index = 0;
		break;
	case 1:
		index = 1;
		break;
	case 21:
		index = 2;
		break;
	case 10:
		index = 3;
		break;
	case 22:
		index = 4;
		break;
	case 23:
		index = 5;
		break;
	case 24:
		index = 6;
		break;
	case 14:
		index = 7;
		break;
	case 12:
		index = 8;
		break;
	case 15:
		index = 9;
		break;
	case 16:
		index = 10;
		break;
	case 17:
		index = 11;
		break;
	case 13:
		index = 12;
		break;
	case 18:
		index = 13;
		break;
	case 19:
		index = 14;
		break;
	case 11:
		index = 15;
		break;
	case 20:
		index = 16;
		break;
	case 25:
		index = 17;
		break;
	case 8:
		index = 18;
		break;
	case 4:
		index = 19;
		break;
	default:
		break;
	}
#endif
	return index;
}

int SpkgGroupManager::registerSpkg(SpkgEntry *entry, SceUInt64 segmentId)
{
	int index;
	SpkgGroup *group;

	index = getGroupIndex((int)entry->type);
	if (index == -1) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(722, "0x%x", entry->type);
#else
		UPDATER_ERROR_FMT(648, "0x%x", entry->type);
#endif
		return 0;
	}
#ifdef UPDATER_CORE_2014
	if (index == SPKG_DIFF_GROUP_MANIFEST_INDEX) {
		if (segmentId == 0x220ULL || segmentId == 0x230ULL)
			index = SPKG_GROUP_INDEX_OS0_EXT;
		else if (segmentId == 0x221ULL || segmentId == 0x231ULL)
			index = SPKG_GROUP_INDEX_VS0_EXT;
		else if (segmentId == 0x240ULL)
			index = SPKG_GROUP_INDEX_PD0;
		else if (entry->hardwareInfo == 1)
			index = SPKG_GROUP_INDEX_OS0;
		else
			index = SPKG_GROUP_INDEX_VS0;
	} else if (index == SPKG_GROUP_INDEX_OS0) {
		if ((SceUInt16)entry->platform != 0)
			index = SPKG_GROUP_INDEX_OS0_EXT;
	} else if (index == SPKG_GROUP_INDEX_VS0) {
		if ((SceUInt16)entry->platform != 0)
			index = SPKG_GROUP_INDEX_VS0_EXT;
	}
#else
	if (index == SPKG_DIFF_GROUP_MANIFEST_INDEX) {
		if (entry->hardwareInfo == 1)
			index = SPKG_GROUP_INDEX_OS0;
		else
			index = SPKG_GROUP_INDEX_VS0;
	}
#endif
	group = m_groups[index];
	if (group == NULL) {
		if (s_isDiffSpkgType[entry->type] != 0) {
			group = new SpkgGroupDiff();
#ifdef UPDATER_CORE_2014
			if (index == SPKG_GROUP_INDEX_OS0 || index == SPKG_GROUP_INDEX_VS0)
				m_hasDiffGroup = 1;
#else
			m_hasDiffGroup = 1;
#endif
		} else {
			group = new SpkgGroupFull();
		}
		if (group == NULL)
			return SCE_UPDATER_ERROR_NO_MEMORY;
		m_groups[index] = group;
	}
	if ((segmentId & PUP_SPKG_SEG_MASK) == PUP_SPKG_SEG_BASE)
		group->AddSpkg(entry);
	else
		group->SetDiffManifestSpkg(entry);
	return 0;
}

int SpkgGroupManager::parsePup(SceUID pupFd)
{
	int ret = 0;
	SceSblUsSpkgInfo systemInfo;
	SceOff seekRet;
	PupHeader header;
	SceSSize readRet;
	PupSegment *segments = NULL;
	SceUInt8 *readBuffer;
	SceUInt64 segmentCount;
	int i;
#ifdef UPDATER_CORE_2014
	UpdateUtility utility;
	int modelType;
#endif

	if (m_initialized)
		return 0;
	systemInfo.version = 0;
	ret = sceSblUsGetSpkgInfo(SCE_SBL_US_SPKG_TYPE_SYSTEM, &systemInfo);
	if (ret != 0) {
		UpdaterLogPrintf("ERROR : ");
#ifdef UPDATER_CORE_2014
		UpdaterLogPrintf("0x%08x (%d) :\n", UPDATER_FILE_ID, 461);
#else
		UpdaterLogPrintf("0x%08x (%d) :\n", UPDATER_FILE_ID, 454);
#endif
		return ret;
	}
	seekRet = sceIoLseek(pupFd, 0, SCE_SEEK_SET);
	if (seekRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(468, "%lld", seekRet);
#else
		UPDATER_ERROR_FMT(461, "%lld", seekRet);
#endif
		return SCE_UPDATER_ERROR_SPKG_SEEK;
	}
	readRet = sceIoRead(pupFd, &header, sizeof(header));
	if (readRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(476, "%x", readRet);
#else
		UPDATER_ERROR_FMT(469, "%x", readRet);
#endif
		return SCE_UPDATER_ERROR_SPKG_READ;
	}
	segmentCount = header.m_segment_num;
	segments = static_cast<PupSegment *>(
		g_updaterMemoryInfo.updaterCoreAlloc((SceSize)(segmentCount * sizeof(PupSegment))));
	if (segments == NULL) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(493, "%d %lld", (int)sizeof(PupSegment), segmentCount);
#else
		UPDATER_ERROR_FMT(486, "%d %lld", (int)sizeof(PupSegment), segmentCount);
#endif
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
	readRet = sceIoRead(pupFd, segments, (SceSize)(segmentCount * sizeof(PupSegment)));
	if (readRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(501, "%x", readRet);
#else
		UPDATER_ERROR_FMT(494, "%x", readRet);
#endif
		g_updaterMemoryInfo.updaterCoreFree(segments);
		return SCE_UPDATER_ERROR_SPKG_READ;
	}
	readBuffer = static_cast<SceUInt8 *>(
		g_updaterMemoryInfo.updaterCoreMemalign(PUP_PARSE_BUFFER_ALIGN,
							PUP_PARSE_BUFFER_SIZE));
	if (readBuffer == NULL) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(512, "%d %d", PUP_PARSE_BUFFER_ALIGN, PUP_PARSE_BUFFER_SIZE);
#else
		UPDATER_ERROR_FMT(505, "%d %d", PUP_PARSE_BUFFER_ALIGN, PUP_PARSE_BUFFER_SIZE);
#endif
		g_updaterMemoryInfo.updaterCoreFree(segments);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
#ifdef UPDATER_CORE_2014
	ret = utility.GetHardwareModel(&modelType);
	if (ret != 0) {
		g_updaterMemoryInfo.updaterCoreFree(segments);
		return ret;
	}
#endif
	for (i = 0; (SceInt64)i < (SceInt64)segmentCount; ++i) {
		SceUInt64 segId = segments[i].segment_id;

#ifdef UPDATER_CORE_2014
		if ((segId & PUP_DIFF_MANIFEST_SEG_MASK) == PUP_DIFF_MANIFEST_SEG_BASE ||
		    (segId & PUP_SPKG_SEG_MASK) == PUP_SPKG_SEG_BASE ||
		    segId == 0x220ULL || segId == 0x221ULL ||
		    segId == 0x230ULL || segId == 0x231ULL ||
		    segId == 0x240ULL || segId == 0x250ULL || segId == 0x260ULL) {
#else
		if ((segId & PUP_DIFF_MANIFEST_SEG_MASK) == PUP_DIFF_MANIFEST_SEG_BASE ||
		    (segId & PUP_SPKG_SEG_MASK) == PUP_SPKG_SEG_BASE) {
#endif
			CertifiedFileHeader certHeader;
			SpkgPackageInfo pkgInfo;
			SpkgChunkInfo chunkInfo;
			SpkgEntry entry;
			SceInt32 alignedPos;
			SceInt32 headerAlignedPos;
			SceUInt8 *infoPtr;

#ifdef UPDATER_CORE_2014
			switch (segId) {
			case 0x220ULL:
			case 0x230ULL:
			case 0x250ULL:
			case 0x260ULL:
				UpdaterLogPrintf("ERROR : ");
				UpdaterLogPrintf("segment ID (%llx) is not support\n", segId);
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_NOT_SUPPORTED;
			case 0x221ULL:
				if (modelType != 1)
					continue;
				break;
			case 0x231ULL:
				if (modelType != 2)
					continue;
				break;
			default:
				break;
			}
#endif
			alignedPos = (SceInt32)((segments[i].offset >> PUP_SECTOR_SHIFT)
						<< PUP_SECTOR_SHIFT);
			seekRet = sceIoLseek(pupFd, (SceOff)alignedPos, SCE_SEEK_SET);
			if (seekRet < 0) {
#ifdef UPDATER_CORE_2014
				UPDATER_ERROR_FMT(583, "%lld : %lld",
						  segments[i].offset, seekRet);
#else
				UPDATER_ERROR_FMT(531, "%lld : %lld",
						  segments[i].offset, seekRet);
#endif
				g_updaterMemoryInfo.updaterCoreFree(readBuffer);
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_SPKG_SEEK;
			}
			readRet = sceIoRead(pupFd, readBuffer, PUP_PARSE_BUFFER_SIZE);
			if (readRet < 0) {
#ifdef UPDATER_CORE_2014
				UPDATER_ERROR_FMT(592, "%x", readRet);
#else
				UPDATER_ERROR_FMT(540, "%x", readRet);
#endif
				g_updaterMemoryInfo.updaterCoreFree(readBuffer);
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_SPKG_READ;
			}
			sceClibMemcpy(&certHeader,
				      readBuffer + ((SceUInt32)segments[i].offset - (SceUInt32)alignedPos),
				      sizeof(certHeader));
			headerAlignedPos = (SceInt32)(((segments[i].offset + certHeader.fileOffset)
						       >> PUP_SECTOR_SHIFT) << PUP_SECTOR_SHIFT);
			seekRet = sceIoLseek(pupFd, (SceOff)headerAlignedPos, SCE_SEEK_SET);
			if (seekRet < 0) {
#ifdef UPDATER_CORE_2014
				UPDATER_ERROR_FMT(606, "%d : %lld", headerAlignedPos, seekRet);
#else
				UPDATER_ERROR_FMT(554, "%d : %lld", headerAlignedPos, seekRet);
#endif
				g_updaterMemoryInfo.updaterCoreFree(readBuffer);
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_SPKG_SEEK;
			}
			readRet = sceIoRead(pupFd, readBuffer, PUP_PARSE_BUFFER_SIZE);
			if (readRet < 0) {
#ifdef UPDATER_CORE_2014
				UPDATER_ERROR_FMT(615, "%x", readRet);
#else
				UPDATER_ERROR_FMT(563, "%x", readRet);
#endif
				g_updaterMemoryInfo.updaterCoreFree(readBuffer);
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_SPKG_READ;
			}
			infoPtr = readBuffer +
				  ((SceUInt32)(segments[i].offset + certHeader.fileOffset) -
				   (SceUInt32)headerAlignedPos);
			sceClibMemcpy(&pkgInfo, infoPtr, sizeof(pkgInfo));
			sceClibMemcpy(&chunkInfo, infoPtr + sizeof(pkgInfo), sizeof(chunkInfo));
#ifdef UPDATER_CORE_2014
			if ((pkgInfo.type == 21 || pkgInfo.type == 22) &&
			    (((SceUInt16)pkgInfo.platform == 1 && modelType != 1) ||
			     ((SceUInt16)pkgInfo.platform == 2 && modelType != 2)))
				continue;
#endif
			entry.type = pkgInfo.type;
			entry.version = (SceUInt32)(pkgInfo.updateVersion & 0xffffffffULL);
			entry.buildNo = (SceUInt32)(pkgInfo.updateVersion >> 32);
			entry.segmentOffset = segments[i].offset;
			entry.segmentSize = segments[i].size;
			entry.hardwareInfo = pkgInfo.hardwareInfo;
			entry.chunkIndex = chunkInfo.chunkIndex;
			entry.decryptedSize = pkgInfo.decryptedSize;
			entry.finalSize = pkgInfo.finalSize;
#ifdef UPDATER_CORE_2014
			entry.platform = pkgInfo.platform;
#endif
			if (pkgInfo.decryptedSize != pkgInfo.finalSize &&
			    systemInfo.version != 0 &&
			    systemInfo.version < MIN_COMPRESSED_SPKG_VERSION) {
				UpdaterLogPrintf("ERROR : ");
#ifdef UPDATER_CORE_2014
				UpdaterLogPrintf("0x%08x (%d) :\n", UPDATER_FILE_ID, 675);
#else
				UpdaterLogPrintf("0x%08x (%d) :\n", UPDATER_FILE_ID, 603);
#endif
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return SCE_UPDATER_ERROR_UNSUPPORTED_SYSTEM;
			}
			ret = registerSpkg(&entry, segId);
			if (ret != 0) {
				g_updaterMemoryInfo.updaterCoreFree(segments);
				return ret;
			}
		}
	}
	calculateEstimateTime();
	m_initialized = true;
	g_updaterMemoryInfo.updaterCoreFree(readBuffer);
	g_updaterMemoryInfo.updaterCoreFree(segments);
	return ret;
}

int SpkgGroupManager::GetEstimateProcessTime(const char *fileName,
					     SceUpdaterEstimateTime *estimateTime)
{
	SceUID fd;
	int ret;
	int closeRet;

	if (m_estimatedInspectMs != 0 && m_estimatedUpdateMs != 0) {
		estimateTime->inspectMs = m_estimatedInspectMs;
		estimateTime->updateMs = m_estimatedUpdateMs;
		return 0;
	}
	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(938, "%s %o %x", fileName, 0, fd);
#else
		UPDATER_ERROR_FMT(813, "%s %o %x", fileName, 0, fd);
#endif
		return SCE_UPDATER_ERROR_SPKG_OPEN;
	}
	ret = parsePup(fd);
	if (ret != 0) {
		sceIoClose(fd);
		return ret;
	}
	closeRet = sceIoClose(fd);
	if (closeRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(955, "%x", closeRet);
#else
		UPDATER_ERROR_FMT(830, "%x", closeRet);
#endif
		return SCE_UPDATER_ERROR_SPKG_CLOSE;
	}
	estimateTime->inspectMs = m_estimatedInspectMs;
	estimateTime->updateMs = m_estimatedUpdateMs;
	return 0;
}

int SpkgGroupManager::InspectAllSpackage(const char *fileName)
{
	SceUID fd;
	int ret;
	int closeRet;
	SceUpdaterEstimateTime estimateTime;
	int i;

	sceKernelGetSystemTime(&m_inspectStartTime);
	s_updatePhaseCount = 0;
	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(343, "%s %o : %x", fileName, 0, fd);
#else
		UPDATER_ERROR_FMT(339, "%s %o : %x", fileName, 0, fd);
#endif
		return SCE_UPDATER_ERROR_SPKG_OPEN;
	}
	ret = parsePup(fd);
	if (ret == 0) {
		if (m_parallelOp == NULL) {
			m_parallelOp = new SpkgParallelOperation();
			if (m_parallelOp != NULL)
				ret = m_parallelOp->SetUpOperation();
		}
		if (m_parallelOp != NULL && ret >= 0) {
			m_parallelOp->SetOperation(SPKG_OP_INSPECT, fd);
			ret = m_parallelOp->ExecuteOperation(NULL);
			if (ret >= 0) {
				for (i = 0; i < SPKG_GROUP_MAX; ++i) {
					int inspectedCount = 0;
					bool hasPending = false;

					if (m_groups[i] != NULL) {
						m_groups[i]->PrepareUpdateOrder(&hasPending);
						if (hasPending) {
							m_activeGroups.Push(m_groups[i]);
							ret = m_groups[i]->GetInspectedCount(
								&inspectedCount);
							if (ret != 0)
								break;
							m_totalInspectedCount += inspectedCount;
						}
					}
				}
			}
		}
	}
	closeRet = sceIoClose(fd);
	if (closeRet < 0) {
#ifdef UPDATER_CORE_2014
		UPDATER_ERROR_FMT(399, "%x", closeRet);
#else
		UPDATER_ERROR_FMT(395, "%x", closeRet);
#endif
		if (ret == 0)
			return SCE_UPDATER_ERROR_SPKG_CLOSE;
	} else if (ret == 0) {
		return GetEstimateProcessTime(fileName, &estimateTime);
	}
	return ret;
}

int SpkgGroupManager::AbortOperation(void)
{
	return 0;
}

int SpkgGroupManager::GetProgress(SceUpdaterProgress *progress)
{
	SceKernelSysClock clock;
	SceUInt64 now;

	sceClibMemset(progress, 0, sizeof(*progress));
	sceKernelGetSystemTime(&clock);
	now = clock.quad;
	if (m_updateStartTime.quad == 0) {
		SceUInt64 elapsed = (now - m_inspectStartTime.quad) / 1000;
		SceUInt64 remaining;

		if (elapsed >= m_estimatedInspectMs)
			elapsed = m_estimatedInspectMs;
		progress->inspectElapsedMs = elapsed;
		remaining = m_estimatedInspectMs - elapsed;
		progress->inspectRemainingMs = remaining;
		progress->inspectRate = (float)((elapsed * 100) / m_estimatedInspectMs);
		progress->rate = (SceInt32)progress->inspectRate;
		progress->totalRate =
			(float)((elapsed * 100) / (m_estimatedInspectMs + m_estimatedUpdateMs));
		if (remaining > m_lastProgress.inspectRemainingMs)
			progress->inspectRemainingMs = m_lastProgress.inspectRemainingMs;
		if (progress->inspectRate < m_lastProgress.inspectRate) {
			progress->inspectRate = m_lastProgress.inspectRate;
			progress->rate = m_lastProgress.rate;
			progress->totalRate = m_lastProgress.totalRate;
		}
	} else {
		SceUInt64 elapsed;
		SceUInt64 remaining;

		progress->inspectRate = 100.0f;
		elapsed = (now - m_updateStartTime.quad) / 1000;
		if (elapsed >= m_estimatedUpdateMs)
			elapsed = m_estimatedUpdateMs;
		progress->updateElapsedMs = elapsed;
		remaining = m_estimatedUpdateMs - elapsed;
		progress->updateRemainingMs = remaining;
		progress->updateRate = (float)((elapsed * 100) / m_estimatedUpdateMs);
		progress->rate = (SceInt32)progress->updateRate;
		progress->totalRate = (float)(((elapsed + m_estimatedInspectMs) * 100) /
					      (m_estimatedInspectMs + m_estimatedUpdateMs));
		if (remaining > m_lastProgress.updateRemainingMs)
			progress->updateRemainingMs = m_lastProgress.updateRemainingMs;
		if (progress->updateRate < m_lastProgress.updateRate) {
			progress->updateRate = m_lastProgress.updateRate;
			progress->rate = m_lastProgress.rate;
			progress->totalRate = m_lastProgress.totalRate;
		}
	}
	m_lastProgress = *progress;
	return 0;
}

int SpkgGroupManager::GetResult(void)
{
	return (int)m_updatedComponentMask;
}

int SpkgGroupManager::GetSpackageHeader(int type, SceUpdaterSpkgHeader *header)
{
	SpkgGroup *group = m_groups[type];
	SceUInt32 version = ~0U;
	SceUInt32 buildNo = ~0U;
	int ret;

	if (group == NULL)
		return SCE_UPDATER_ERROR_NOT_FOUND;
	ret = group->GetVersion(&version);
	if (ret != 0)
		return ret;
	header->version = version;
	ret = m_groups[type]->GetBuildNo(&buildNo);
	if (ret != 0)
		return ret;
	header->buildNo = buildNo;
	return 0;
}

int SpkgGroupManager::getSpkgTypeFromGroupIndex(int groupIndex)
{
	int spkgType = 0;

	switch (groupIndex) {
	case 0:
		spkgType = 9;
		break;
	case 1:
		spkgType = 1;
		break;
	case 2:
		spkgType = 21;
		break;
	case 3:
		spkgType = 10;
		break;
	case 4:
		spkgType = 22;
		break;
	case 5:
		spkgType = 23;
		break;
	case 6:
		spkgType = 24;
		break;
	case 7:
		spkgType = 14;
		break;
	case 8:
		spkgType = 12;
		break;
	case 9:
		spkgType = 15;
		break;
	case 10:
		spkgType = 16;
		break;
	case 11:
		spkgType = 17;
		break;
	case 12:
		spkgType = 13;
		break;
	case 13:
		spkgType = 18;
		break;
	case 14:
		spkgType = 19;
		break;
	case 15:
		spkgType = 11;
		break;
	case 16:
		spkgType = 20;
		break;
	case 17:
		spkgType = 25;
		break;
	case 18:
		spkgType = 8;
		break;
	case 19:
		spkgType = 4;
		break;
	default:
		break;
	}
	return spkgType;
}

int SpkgGroupManager::CheckAcceptableSpkgType(const char *fileName, const int *types, int count,
					      int *result)
{
	SceUID fd;
	int ret;
	int closeRet;
	int i;

	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(963, "%s %o : %x", fileName, 0, fd);
		return SCE_UPDATER_ERROR_SPKG_OPEN;
	}
	ret = parsePup(fd);
	if (ret != 0) {
		sceIoClose(fd);
		return ret;
	}
	*result = 0;
	/* Sony reused the loop index i for both the outer and inner loops */
	for (i = 0; i < count; ++i) {
		int expectedType = getSpkgTypeFromGroupIndex(types[i]);
		int groupType;

		for (i = 0; i < SPKG_GROUP_MAX; ++i) {
			if (m_groups[i] != NULL) {
				ret = m_groups[i]->GetType(&groupType);
				if (ret != 0)
					return ret;
				if (expectedType == groupType) {
					*result = 1;
					break;
				}
			}
		}
		if (i >= SPKG_GROUP_MAX) {
			*result = 0;
			break;
		}
	}
	closeRet = sceIoClose(fd);
	if (closeRet < 0) {
		UPDATER_ERROR_FMT(1008, "%x", closeRet);
		return SCE_UPDATER_ERROR_SPKG_CLOSE;
	}
	return ret;
}

void SpkgGroupManager::AdjustEstimatedTime(SceInt64 inspectDeltaMs, SceInt64 updateDeltaMs)
{
	m_estimatedInspectMs += inspectDeltaMs;
	m_estimatedUpdateMs += updateDeltaMs;
}

int SpkgGroupManager::IsDiffUpdate(void)
{
	return m_hasDiffGroup;
}
