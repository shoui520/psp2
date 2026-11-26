/*
 * SpkgGroupDiff (Sony's spkg_group_diff.cpp): differential system package update for os0:
 * and vs0:. Extracts the diff manifest and TAR payloads, constructs a complete FAT image in
 * a user memblock ("Updater Memory File System"), writes it to the partition block device,
 * and commits the SPKG via sceSblUsUpdateSpackage.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include <update_mgr/update_mgr.h>
#include <updater_core/updater_core.h>
#include <vshbridge/vshbridge.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "spkg_group.h"
#include "spkg_group_diff.h"
#include "spkg_group_manager.h"

extern "C" {
#include "../fsutil/fs_util.h"
}

#define UPDATER_FILE_ID			0xa5370423

#define SPKG_NODE_TYPE_UPDATE		1
#define SPKG_NODE_TYPE_EXTRACT		3

#define SCE_SBL_US_ERROR_INVALID_OP	((int)0x800f0210)
#define SCE_SBL_US_ERROR_UP_TO_DATE	((int)0x800f0236)

#define DIFF_BLOCK_WRITE_CHUNK		0x400000U
#define DIFF_FAT_SECTOR_SIZE		512U
#define DIFF_PARTITION_CONF_COUNT	16
#define DIFF_SPKG_TABLE_COUNT		25

#define DIFF_ESTIMATE_COPY_SPEED	4374.0f
#define DIFF_ESTIMATE_FLUSH_SPEED	6005.0f
#define DIFF_ESTIMATE_INSPECT_SCALE	1.5

struct DiffPartitionConf {
	SceUInt8 partitionCode;
	SceUInt8 fsType;
	SceUInt8 reserved[2];
	SceUInt32 clusterSize;
	SceUInt32 flags;
};

/* Sony's partition_conf.h table (0x81018900) */
static const DiffPartitionConf s_partitionConf[DIFF_PARTITION_CONF_COUNT] = {
	{ 1,  0xda, { 0, 0 }, 0x0000, 0x00000000 },
	{ 2,  0xda, { 0, 0 }, 0x0000, 0x00000000 },
	{ 2,  0xda, { 0, 0 }, 0x0000, 0x00000000 },
	{ 3,  0x06, { 0, 0 }, 0x1000, 0x00000000 },
	{ 3,  0x06, { 0, 0 }, 0x1000, 0x00000000 },
	{ 12, 0x06, { 0, 0 }, 0x4000, 0x00000000 },
	{ 6,  0x06, { 0, 0 }, 0x1000, 0x01000001 },
	{ 4,  0x06, { 0, 0 }, 0x1000, 0x00000001 },
	{ 5,  0x06, { 0, 0 }, 0x1000, 0x01000101 },
	{ 11, 0x06, { 0, 0 }, 0x4000, 0x01000101 },
	{ 14, 0x06, { 0, 0 }, 0x4000, 0x00000000 },
	{ 7,  0x07, { 0, 0 }, 0x8000, 0x01000101 },
	{ 0,  0x00, { 0, 0 }, 0x0000, 0x00000000 },
	{ 0,  0x00, { 0, 0 }, 0x0000, 0x00000000 },
	{ 0,  0x00, { 0, 0 }, 0x0000, 0x00000000 },
	{ 0,  0x00, { 0, 0 }, 0x0000, 0x00000000 },
};

/* target partition logical volume size by SPKG type (0x810189c0) */
static const SceInt32 s_partitionVolumeSize[DIFF_SPKG_TABLE_COUNT] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0x01000000, 0x10000000, 0,
	0,
};

/* memblock size for "Updater Memory File System" by SPKG type (0x81018a24) */
static const SceSize s_memBlockAllocSize[DIFF_SPKG_TABLE_COUNT] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0x01000000, 0x0c800000, 0,
	0,
};

/* partition_conf partition code by SPKG type (0x81018a88) */
static const SceUInt8 s_partitionCode[DIFF_SPKG_TABLE_COUNT] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 3, 4, 0,
	0,
};

/* VSH mount ID for mounting the source partition during diff build (0x81018aa4) */
static const int s_vshMountId[DIFF_SPKG_TABLE_COUNT] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0x300, 0,
	0,
};

/* mount prefix by SPKG type (0x81018b1c) */
static const char *const s_mountPrefix[DIFF_SPKG_TABLE_COUNT] = {
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "os0:", "vs0:", "",
	"",
};

/* block device path by SPKG type (0x81018bb4) */
static const char *const s_blockDevicePath[DIFF_SPKG_TABLE_COUNT] = {
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "sdstor0:int-lp-ina-os", "sdstor0:int-lp-ign-vsh", "",
	"",
};

SpkgGroupDiff::SpkgGroupDiff()
{
	m_manifestBuffer = NULL;
	m_memBlockId = 0;
	m_memBlockBase = NULL;
	m_manifestOffset = 0;
	m_serialNumber = 0;
	m_tarBuffer = NULL;
	m_tarSpkgIndex = 0;
	m_unusedDiffState = 0;
	sceClibMemset(&m_manifestSpkg, 0, sizeof(m_manifestSpkg));
	sceClibMemset(m_currentDirPath, 0, sizeof(m_currentDirPath));
	m_tarArchive.Reset();
}

SpkgGroupDiff::~SpkgGroupDiff()
{
}

int SpkgGroupDiff::EnqueueUpdate(SceUID pupFd)
{
	SpkgOperationElement *element = NULL;
	int ret;

	(void)pupFd;
	ret = g_spkgGroupManager->GetParallelOperation()->GetFreeChannel().Pop(
		reinterpret_cast<void **>(&element), true);
	if (ret < 0) {
		UPDATER_ERROR_FMT(274, "%x", ret);
		return ret;
	}
	ret = element->SetTask(0, SPKG_OP_UPDATE, this, 0);
	if (ret < 0) {
		UPDATER_ERROR_FMT(283, "%x", ret);
		return ret;
	}
	ret = g_spkgGroupManager->GetParallelOperation()->GetFullChannel().Push(element, true);
	if (ret < 0) {
		UPDATER_ERROR_FMT(297, "%x", ret);
		return ret;
	}
	return 0;
}

int SpkgGroupDiff::freeImageMemBlock(void)
{
	int ret;

	ret = sceKernelFreeMemBlock(m_memBlockId);
	if (ret < 0) {
		UPDATER_ERROR_FMT(582, "%x", ret);
		return SCE_UPDATER_ERROR_MEMBLOCK_FREE;
	}
	return ret;
}

void SpkgGroupDiff::freeTarBuffer(void)
{
	if (m_tarBuffer != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(m_tarBuffer);
		m_tarBuffer = NULL;
	}
}

int SpkgGroupDiff::unmountTargetPartition(void)
{
	SceUInt32 spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;

	if (s_vshMountId[spkgType] != 0) {
		spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
		vshIoUmount(s_vshMountId[spkgType], 1, 0, 0);
	}
	return 0;
}

int SpkgGroupDiff::convertErrorCodeUpdateResult(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x80180740;
	else if (packageType == 22)
		base = 0x80180750;
	else
		return 0;

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

int SpkgGroupDiff::convertErrorCodeUpdateStatus(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x80180740;
	else if (packageType == 22)
		base = 0x80180750;
	else
		return 0;

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

int SpkgGroupDiff::convertErrorCodeUpdateRequest(int packageType, int error)
{
	if (packageType == 21) {
		switch (error) {
		case SCE_SBL_US_ERROR_NOT_PERMITTED:
			return 0x80180721;
		case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
			return 0x80180722;
		case SCE_SBL_US_ERROR_INVALID_OP:
			return 0x80180723;
		case SCE_SBL_US_ERROR_IO:
			return 0x80180724;
		case SCE_SBL_US_ERROR_TRY_AGAIN:
			return 0x80180725;
		case SCE_SBL_US_ERROR_NO_MEMORY:
			return 0x80180726;
		case SCE_SBL_US_ERROR_BAD_MAGIC:
			return 0x80180727;
		case SCE_SBL_US_ERROR_NOT_FOUND:
			return 0x80180728;
		default:
			return error;
		}
	}
	if (packageType == 22) {
		switch (error) {
		case SCE_SBL_US_ERROR_NOT_PERMITTED:
			return 0x80180731;
		case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		case SCE_SBL_US_ERROR_INVALID_OP:
			return 0x80180732;
		case SCE_SBL_US_ERROR_IO:
			return 0x80180733;
		case SCE_SBL_US_ERROR_TRY_AGAIN:
			return 0x80180735;
		case SCE_SBL_US_ERROR_NO_MEMORY:
			return 0x80180736;
		case SCE_SBL_US_ERROR_BAD_MAGIC:
			return 0x80180737;
		case SCE_SBL_US_ERROR_NOT_FOUND:
			return 0x80180738;
		default:
			return error;
		}
	}
	return 0;
}

int SpkgGroupDiff::commitHeaderSpkg(SceUID pupFd)
{
	int packageType;
	SceUInt32 flags;
	SpkgEntry *firstEntry;
	void *spkgBuffer = NULL;
	SceUInt32 requestId;
	int statusResult;
	int ret;
	int releaseRet;

	packageType = GetUpdatePackageType(0);
	flags = GetUpdateFlags();
	firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
	ret = sceSblUsAllocateBuffer((SceSize)firstEntry->segmentSize, &spkgBuffer);
	if (ret != 0) {
		firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
		UPDATER_ERROR_FMT(1254, "%d: %x", (SceSize)firstEntry->segmentSize, ret);
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
		spkgBuffer = NULL;
	} else {
		firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
		ret = ReadSpkg(pupFd, firstEntry, spkgBuffer);
		if (ret == 0) {
			firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
			ret = sceSblUsUpdateSpackage(packageType, spkgBuffer,
						     (SceSize)firstEntry->segmentSize,
						     flags, &requestId);
			if (ret != 0) {
				firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
				UPDATER_ERROR_FMT(1279, "%d %d %x : %x", packageType,
						  (SceSize)firstEntry->segmentSize, flags, ret);
				ret = convertErrorCodeUpdateRequest(packageType, ret);
			} else {
				ret = WaitStatus(SPKG_NODE_TYPE_UPDATE, requestId, &statusResult);
				if (ret != 0)
					ret = convertErrorCodeUpdateStatus(packageType, ret);
				else if (statusResult != 0)
					ret = convertErrorCodeUpdateResult(packageType, statusResult);
			}
		}
	}
	releaseRet = sceSblUsReleaseBuffer(spkgBuffer);
	if (releaseRet != 0) {
		UPDATER_ERROR_FMT(1300, "%x", releaseRet);
		if (releaseRet == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			releaseRet = SCE_UPDATER_ERROR_FREE_BUF_INVALID_ARG;
		else if (releaseRet == SCE_SBL_US_ERROR_NOT_PERMITTED)
			releaseRet = SCE_UPDATER_ERROR_FREE_BUF_NOT_PERMIT;
		if (ret == 0)
			return releaseRet;
	}
	return ret;
}

int SpkgGroupDiff::flushImageAndCommit(SceUID pupFd)
{
	char devicePath[128];
	SpkgEntry *firstEntry;
	SceInt64 remainingBytes;
	int ret;
	SceUID fd;
	int closeRet;

	firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));
	sceClibStrncpy(devicePath, s_blockDevicePath[firstEntry->type], sizeof(devicePath));
	remainingBytes = (SceInt64)m_fatFs.GetUsedImageSize();
	ret = m_fatFs.Finalize();
	if (ret != 0)
		return ret;
	fd = sceIoOpen(devicePath, SCE_O_RDWR, 0);
	if (fd < 0) {
		UPDATER_ERROR_FMT(1165, "%s : %x", devicePath, fd);
		return SCE_UPDATER_ERROR_BLOCK_OPEN;
	}
	{
		SceSize writeSize = remainingBytes < (SceInt64)DIFF_BLOCK_WRITE_CHUNK
					    ? (SceSize)remainingBytes
					    : DIFF_BLOCK_WRITE_CHUNK;

		for (;;) {
			SceSSize written = sceIoWrite(fd, m_memBlockBase, writeSize);

			if (written < 0) {
				UPDATER_ERROR_FMT(1184, "%x %d : %x", fd, writeSize, written);
				ret = SCE_UPDATER_ERROR_BLOCK_WRITE;
				break;
			}
			m_memBlockBase += written;
			if ((SceSize)written == writeSize) {
				remainingBytes -= DIFF_BLOCK_WRITE_CHUNK;
				if (remainingBytes <= 0)
					break;
				if (remainingBytes < (SceInt64)DIFF_BLOCK_WRITE_CHUNK)
					writeSize = (SceSize)remainingBytes;
			} else {
				writeSize -= (SceSize)written;
			}
		}
	}
	closeRet = sceIoClose(fd);
	if (closeRet < 0) {
		UPDATER_ERROR_FMT(1209, "%x : %x", fd, closeRet);
		if (ret == 0)
			return SCE_UPDATER_ERROR_BLOCK_CLOSE;
		return ret;
	}
	if (ret == 0)
		ret = commitHeaderSpkg(pupFd);
	if (ret == 0)
		freeImageMemBlock();
	return ret;
}

int SpkgGroupDiff::convertErrorCodeGetExtract(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x80180700;
	else if (packageType == 22)
		base = 0x80180708;
	else if (packageType == 4)
		base = 0x80180710;
	else
		return 0;

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

int SpkgGroupDiff::convertErrorCodeExtractResult(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x801806d0;
	else if (packageType == 22)
		base = 0x801806e0;
	else if (packageType == 4)
		base = 0x801806f0;
	else
		return 0;

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

int SpkgGroupDiff::convertErrorCodeExtractStatus(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x801806d0;
	else if (packageType == 22)
		base = 0x801806e0;
	else if (packageType == 4)
		base = 0x801806f0;
	else
		return 0;

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

int SpkgGroupDiff::convertErrorCodeExtractRequest(int packageType, int error)
{
	int base;

	if (packageType == 21)
		base = 0x801806b0;
	else if (packageType == 22)
		base = 0x801806b8;
	else if (packageType == 4)
		base = 0x801806c0;
	else
		return 0;

	switch (error) {
	case SCE_SBL_US_ERROR_NOT_PERMITTED:
		return base + 1;
	case SCE_SBL_US_ERROR_INVALID_ARGUMENT:
		return base + 2;
	case SCE_SBL_US_ERROR_INVALID_OP:
		return base + 3;
	case SCE_SBL_US_ERROR_NO_MEMORY:
		return base + 4;
	case SCE_SBL_US_ERROR_BAD_MAGIC:
		return base + 5;
	default:
		return error;
	}
}

int SpkgGroupDiff::extractSpkg(SceUID pupFd, const SpkgEntry *entry, void *outputBuffer)
{
	void *spkgBuffer = NULL;
	SceUInt32 requestId;
	int statusResult;
	int ret;
	int releaseRet;

	ret = sceSblUsAllocateBuffer((SceSize)entry->segmentSize, &spkgBuffer);
	if (ret != 0) {
		UPDATER_ERROR_FMT(1041, "%d: %x", (SceSize)entry->segmentSize, ret);
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
	} else {
		ret = ReadSpkg(pupFd, entry, spkgBuffer);
		if (ret == 0) {
			SceUInt32 flags = GetUpdateFlags();

			ret = sceSblUsExtractSpackage(SPKG_NODE_TYPE_EXTRACT, spkgBuffer,
						      (SceSize)entry->segmentSize, flags,
						      &requestId);
			if (ret != 0) {
				UPDATER_ERROR_FMT(1081, "%d %lld %x : %x", SPKG_NODE_TYPE_EXTRACT,
						  entry->segmentSize, flags, ret);
				ret = convertErrorCodeExtractRequest((int)entry->type, ret);
			} else {
				ret = WaitStatus(SPKG_NODE_TYPE_EXTRACT, requestId, &statusResult);
				if (ret != 0) {
					ret = convertErrorCodeExtractStatus((int)entry->type, ret);
				} else if (statusResult != 0) {
					ret = convertErrorCodeExtractResult((int)entry->type,
									    statusResult);
				} else {
					ret = sceSblUsGetExtractSpackage(SPKG_NODE_TYPE_EXTRACT,
									 requestId,
									 (SceUInt32)entry->decryptedSize,
									 (SceSize)entry->finalSize,
									 outputBuffer);
					if (ret != 0) {
						UPDATER_ERROR_FMT(1110,
								  "%d %d %lld %lld : %x",
								  SPKG_NODE_TYPE_EXTRACT, requestId,
								  entry->decryptedSize,
								  entry->finalSize, ret);
						ret = convertErrorCodeGetExtract((int)entry->type,
										 ret);
					}
				}
			}
		}
	}
	releaseRet = sceSblUsReleaseBuffer(spkgBuffer);
	if (releaseRet != 0) {
		UPDATER_ERROR_FMT(1118, "%x", releaseRet);
		if (releaseRet == SCE_SBL_US_ERROR_INVALID_ARGUMENT)
			releaseRet = SCE_UPDATER_ERROR_FREE_BUF_INVALID_ARG;
		else if (releaseRet == SCE_SBL_US_ERROR_NOT_PERMITTED)
			releaseRet = SCE_UPDATER_ERROR_FREE_BUF_NOT_PERMIT;
		if (ret == 0)
			return releaseRet;
	}
	return ret;
}

int SpkgGroupDiff::addTarFileToMemoryFs(SceUID pupFd, const char *tarPath, const char *fileName,
					SceIoStat *stat)
{
	int fileCount = 0;
	SceUInt32 fileSize;
	SceUInt32 fileOffset;
	int ret;

	if (m_tarSpkgIndex >= m_spkgList.GetCount())
		return SCE_UPDATER_ERROR_DIFF_TAR_EXHAUSTED;
	m_tarArchive.GetFileCount(&fileCount);
	if (fileCount == 0) {
		SpkgEntry *entry = static_cast<SpkgEntry *>(m_spkgList.Get(m_tarSpkgIndex));

		ret = extractSpkg(pupFd, entry, m_tarBuffer);
		if (ret != 0)
			return ret;
		m_tarArchive.Reset();
		entry = static_cast<SpkgEntry *>(m_spkgList.Get(m_tarSpkgIndex));
		m_tarArchive.Parse(m_tarBuffer, (SceSize)entry->finalSize);
	}
	ret = m_tarArchive.FindFile(tarPath, &fileSize, &fileOffset);
	if (ret != 0)
		return ret;
	stat->st_size = (SceOff)(SceInt32)fileSize;
	stat->st_mode = SCE_STM_FREG;
	ret = m_fatFs.AddFileFromMemory(fileName, stat, m_tarBuffer + fileOffset);
	if (ret == 0 && m_tarArchive.GetRemainingFileCount() == 0) {
		m_tarArchive.Clear();
		m_tarSpkgIndex++;
	}
	return ret;
}

int SpkgGroupDiff::applyManifestDirectory(SceUID pupFd)
{
	SceIoStat entryStat;
	SceIoStat srcStat;
	SceUInt32 dirCluster;
	char entryPath[256];
	char srcPath[256];
	int pos = 0;
	int lineStart = m_manifestOffset;
	int ret = 0;

	while (m_manifestOffset < (int)m_manifestSpkg.finalSize) {
		char ch = m_manifestBuffer[m_manifestOffset];

		if (pos == 0)
			lineStart = m_manifestOffset;
		if (ch != ':') {
			entryPath[pos++] = ch;
			m_manifestOffset++;
			continue;
		}
		entryPath[pos] = '\0';
		m_manifestOffset += 3;
		if (m_manifestBuffer[m_manifestOffset] == '5')
			entryStat.st_attr = 0x106;
		else
			entryStat.st_attr = 0x006;
		if (m_currentDirPath[0] != '\0') {
			SceSize prefixLen = sceClibStrnlen(m_currentDirPath, 0x100);

			if (sceClibStrncmp(m_currentDirPath, entryPath, prefixLen) != 0) {
				*sceClibStrrchr(m_currentDirPath, '/') = '\0';
				sceClibStrrchr(m_currentDirPath, '/')[1] = '\0';
				m_manifestOffset = lineStart;
				return 0;
			}
		}
		int isDir = 0;
		if (entryPath[pos - 1] == '/') {
			isDir = 1;
			entryPath[pos - 1] = '\0';
		}
		char *entryName = sceClibStrrchr(entryPath, '/') + 1;
		m_manifestOffset += 2;
		char kind = m_manifestBuffer[m_manifestOffset];
		if (kind == 'K') {
			if (isDir) {
				SceUInt64 savedEntryIndex;
				SceUInt64 savedCurrentCluster;
				SceUInt64 savedDirStart;

				entryStat.st_mode = SCE_STM_FDIR;
				ret = m_fatFs.CreateDirectory(entryName, &entryStat, &dirCluster);
				if (ret != 0)
					return ret;
				m_fatFs.GetDirectoryContext(&savedEntryIndex, &savedCurrentCluster,
							    &savedDirStart);
				m_fatFs.SetDirectoryContext(0, dirCluster, dirCluster);
				if (m_currentDirPath[0] == '\0')
					sceClibStrncpy(m_currentDirPath, "/",
						       sizeof(m_currentDirPath));
				sceClibStrncat(m_currentDirPath, entryName,
					       sizeof(m_currentDirPath));
				sceClibStrncat(m_currentDirPath, "/", sizeof(m_currentDirPath));
				m_manifestOffset += 2;
				if (m_manifestBuffer[m_manifestOffset + 1] == '\n')
					m_manifestOffset++;
				ret = applyManifestDirectory(pupFd);
				if (ret != 0)
					return ret;
				m_fatFs.SetDirectoryContext(savedEntryIndex, savedCurrentCluster,
							    savedDirStart);
				m_manifestOffset--;
			} else {
				SpkgEntry *firstEntry = static_cast<SpkgEntry *>(m_spkgList.Get(0));

				entryStat.st_mode = SCE_STM_FREG;
				sceClibStrncpy(srcPath, s_mountPrefix[firstEntry->type],
					       sizeof(srcPath));
				sceClibStrncat(srcPath, entryPath, sizeof(srcPath));
				ret = sceIoGetstat(srcPath, &srcStat);
				if (ret < 0) {
					UpdaterLogPrintf("ERROR : ");
					UpdaterLogPrintf("sceIoGetstat(%s) : %x\n", srcPath, ret);
					return ret;
				}
				entryStat.st_size = srcStat.st_size;
				ret = m_fatFs.AddFileFromPath(entryName, srcPath, &entryStat);
				if (ret != 0)
					return ret;
				m_manifestOffset++;
				if (m_manifestBuffer[m_manifestOffset + 1] == '\n')
					m_manifestOffset++;
			}
		} else if (kind == 'F') {
			ret = addTarFileToMemoryFs(pupFd, entryPath, entryName, &entryStat);
			if (ret != 0)
				return ret;
			m_manifestOffset++;
			if (m_manifestBuffer[m_manifestOffset + 1] == '\n')
				m_manifestOffset++;
		}
		pos = 0;
		m_manifestOffset++;
	}
	return ret;
}

int SpkgGroupDiff::mountTargetPartition(void)
{
	int ret = 0;
	SceUInt32 spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;

	if (s_vshMountId[spkgType] != 0) {
		spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
		ret = (int)vshIoMount(s_vshMountId[spkgType], NULL, 0, NULL, 0, 0);
		if (ret < 0) {
			spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
			UPDATER_ERROR_FMT(747, "[%d] : %x", spkgType, ret);
			return SCE_UPDATER_ERROR_DIFF_MOUNT;
		}
	}
	return ret;
}

int SpkgGroupDiff::allocateTarBuffer(void)
{
	int count = (int)m_spkgList.GetCount();
	SceSize maxSize = 0;
	int i;

	for (i = 0; i < count; ++i) {
		SceSize finalSize = (SceSize)static_cast<SpkgEntry *>(m_spkgList.Get(i))->finalSize;

		if ((SceInt32)maxSize < (SceInt32)finalSize)
			maxSize = finalSize;
	}
	m_tarBuffer = static_cast<SceUInt8 *>(g_updaterMemoryInfo.updaterCoreAlloc(maxSize));
	if (m_tarBuffer == NULL) {
		UPDATER_ERROR_FMT(718, "%d", maxSize);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
	return 0;
}

int SpkgGroupDiff::allocateImageMemBlock(void)
{
	SceUInt32 spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
	void *base;
	int ret;

	m_memBlockId = sceKernelAllocMemBlock("Updater Memory File System",
					      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					      s_memBlockAllocSize[spkgType], NULL);
	if (m_memBlockId < 0) {
		UPDATER_ERROR_FMT(551, "%x", m_memBlockId);
		return SCE_UPDATER_ERROR_MEMBLOCK_ALLOC;
	}
	ret = sceKernelGetMemBlockBase(m_memBlockId, &base);
	if (ret < 0) {
		UPDATER_ERROR_FMT(559, "%d : %x", m_memBlockId, ret);
		freeImageMemBlock();
		return SCE_UPDATER_ERROR_MEMBLOCK_BASE;
	}
	m_memBlockBase = static_cast<SceUInt8 *>(base);
	return ret;
}

int SpkgGroupDiff::setupMemoryFs(void)
{
	SceUInt32 spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
	SceUInt8 targetCode = s_partitionCode[spkgType];
	int i;

	for (i = 0; i < DIFF_PARTITION_CONF_COUNT; ++i) {
		if (s_partitionConf[i].partitionCode == targetCode) {
			SceUInt32 clusterSize = s_partitionConf[i].clusterSize;
			SceUInt32 volSpkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
			SceInt64 volumeSize = (SceInt64)s_partitionVolumeSize[volSpkgType];
			int ret;

			ret = allocateImageMemBlock();
			if (ret != 0)
				return ret;
			ret = m_fatFs.Initialize(DIFF_FAT_SECTOR_SIZE, clusterSize,
						 (SceUInt64)volumeSize, NULL, m_serialNumber,
						 m_memBlockBase);
			if (ret != 0) {
				int freeRet = freeImageMemBlock();

				if (freeRet != 0)
					UPDATER_ERROR_FMT(521, "%x", freeRet);
			}
			return ret;
		}
	}
	return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
}

int SpkgGroupDiff::parseManifestSerialNumber(void)
{
	char prefix[28] = "### serialNumber: 0x";
	SceSize prefixLen = sceClibStrnlen(prefix, 0x100);
	SceUInt32 offset;
	SceUInt32 limit;
	SceUInt32 value = 0;

	if (sceClibStrncmp(m_manifestBuffer, prefix, prefixLen) != 0) {
		UpdaterLogPrintf("ERROR : ");
		UpdaterLogPrintf("0x%08x (%d)\n", SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER, 651);
		return SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER;
	}
	m_serialNumber = 0;
	offset = prefixLen;
	limit = prefixLen + 8;
	while (offset < (SceUInt32)m_manifestSpkg.finalSize) {
		char ch = m_manifestBuffer[offset];

		if (ch == '\n') {
			offset++;
			break;
		}
		if ((SceInt32)offset >= (SceInt32)limit) {
			UpdaterLogPrintf("ERROR : ");
			UpdaterLogPrintf("0x%08x (%d)\n",
					 SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER, 665);
			return SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER;
		}
		value <<= 4;
		m_serialNumber = value;
		if (ch >= '0' && ch <= '9') {
			value |= (SceUInt32)(ch - '0');
		} else if (ch >= 'A' && ch <= 'F') {
			value |= (SceUInt32)(ch - 'A' + 10);
		} else if (ch >= 'a' && ch <= 'f') {
			value |= (SceUInt32)(ch - 'a' + 10);
		} else {
			UpdaterLogPrintf("ERROR : ");
			UpdaterLogPrintf("0x%08x (%d)\n",
					 SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER, 681);
			return SCE_UPDATER_ERROR_DIFF_MANIFEST_HEADER;
		}
		m_serialNumber = value;
		offset++;
	}
	m_manifestOffset = (int)offset;
	return 0;
}

int SpkgGroupDiff::extractManifest(SceUID pupFd)
{
	int ret;

	m_manifestBuffer = static_cast<char *>(
		g_updaterMemoryInfo.updaterCoreAlloc((SceSize)m_manifestSpkg.finalSize));
	if (m_manifestBuffer == NULL) {
		UPDATER_ERROR_FMT(604, "%lld", m_manifestSpkg.finalSize);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
	ret = extractSpkg(pupFd, &m_manifestSpkg, m_manifestBuffer);
	if (ret != 0) {
		g_updaterMemoryInfo.updaterCoreFree(m_manifestBuffer);
		m_manifestBuffer = NULL;
	}
	return ret;
}

int SpkgGroupDiff::UpdateSpkg(SpkgOperationElement *element)
{
	SceUID pupFd = g_spkgGroupManager->GetParallelOperation()->GetPupFd();
	int ret;

	(void)element;
	ret = extractManifest(pupFd);
	if (ret == 0) {
		ret = parseManifestSerialNumber();
		if (ret == 0) {
			ret = setupMemoryFs();
			if (ret == 0) {
				ret = allocateTarBuffer();
				if (ret == 0) {
					ret = mountTargetPartition();
					if (ret == 0)
						ret = applyManifestDirectory(pupFd);
					if (ret == 0) {
						freeTarBuffer();
						unmountTargetPartition();
						ret = flushImageAndCommit(pupFd);
					} else {
						unmountTargetPartition();
						freeTarBuffer();
						freeImageMemBlock();
					}
				}
			}
		}
	}
	if (m_manifestBuffer != NULL) {
		g_updaterMemoryInfo.updaterCoreFree(m_manifestBuffer);
		m_manifestBuffer = NULL;
	}
	return ret;
}

void SpkgGroupDiff::EstimateTime(SceUpdaterEstimateTime *estimateTime)
{
	SceUInt64 maxSize = 0;
	SceUInt64 freeSize = 0;
	SceUInt32 spkgType = static_cast<SpkgEntry *>(m_spkgList.Get(0))->type;
	int ret;
	SceUInt64 inspectMs;
	SceUInt64 updateMs;
	float totalSizeFloat;
	float manifestSizeFloat;
	float usedSizeFloat;
	float maxSizeFloat;

	ret = sceFsUtilGetDevInfo(s_mountPrefix[spkgType], &maxSize, &freeSize);
	if (ret < 0)
		UPDATER_ERROR_FMT(408, "%x", ret);
	totalSizeFloat = (float)m_totalSize;
	inspectMs = (SceUInt64)((totalSizeFloat / g_spkgReadSpeed +
				 totalSizeFloat / g_spkgInspectSpeed) +
				(float)(int)m_spkgList.GetCount() * g_spkgInspectOverheadMs);
	manifestSizeFloat = (float)m_manifestSpkg.segmentSize;
	usedSizeFloat = (float)(maxSize - freeSize);
	maxSizeFloat = (float)maxSize;
	updateMs = (SceUInt64)(((double)(manifestSizeFloat / g_spkgReadSpeed +
					 manifestSizeFloat / g_spkgInspectSpeed) +
				(double)inspectMs * DIFF_ESTIMATE_INSPECT_SCALE) +
			       (double)(usedSizeFloat / DIFF_ESTIMATE_COPY_SPEED) +
			       (double)(maxSizeFloat / DIFF_ESTIMATE_FLUSH_SPEED));
	m_estimatedInspectMs = inspectMs;
	m_estimatedUpdateMs = updateMs;
	estimateTime->inspectMs = inspectMs;
	estimateTime->updateMs = updateMs;
}

void SpkgGroupDiff::OnInspectSkipped(SpkgEntry *entry)
{
	entry->status = SPKG_ENTRY_STATUS_SKIPPED;
}

void SpkgGroupDiff::SetDiffManifestSpkg(const SpkgEntry *entry)
{
	m_manifestSpkg = *entry;
}
