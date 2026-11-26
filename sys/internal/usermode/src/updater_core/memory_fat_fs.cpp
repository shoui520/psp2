/*
 * MemoryFatFs (in updater_core, 0x8100de68..0x810104a3): builds a FAT12/FAT16/FAT32
 * filesystem image in memory ("Updater Memory File System") with BPB, FSInfo, 8.3/LFN
 * directory entries and cluster chains for differential os0:/vs0: updates.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "memory_fat_fs.h"
#include "updater_log.h"
#include "updater_memory.h"

#define FAT_SECTOR_SIZE_MAX		512
#define FAT_DIR_ENTRY_SIZE		32
#define FAT_SHORT_NAME_LEN		11
#define FAT_LFN_CHARS_PER_ENTRY		13
#define FAT_MAX_NAME_LEN		255

#define FAT_CLUSTERS_FAT12_MAX		0xff5U
#define FAT_CLUSTERS_FAT16_MAX		0xfff5U

#define FAT_EOC_MARK			0x0fffffffU
#define FAT_EOC_FAT12			0x0fffU
#define FAT_EOC_FAT16			0xffffU
#define FAT_EOC_FAT32			0x0fffffffU
#define FAT_EOC_MIN_FAT12		0x0ff8U
#define FAT_EOC_MIN_FAT16		0xfff8U
#define FAT_EOC_MIN_FAT32		0x0ffffff8U

#define FAT_ATTR_HIDDEN			0x02
#define FAT_ATTR_SYSTEM			0x04
#define FAT_ATTR_LFN			0x0f
#define FAT_ATTR_DIRECTORY		0x10
#define FAT_ATTR_ARCHIVE		0x20

#define FAT_STAT_ATTR_HIDDEN_SYSTEM	0x006
#define FAT_STAT_ATTR_SYSTEM		0x106

#define FAT_CTYPE_PRINTABLE_MASK	0x97

#define FAT_LOG_ERR(err, line) \
	do { \
		UpdaterLogPrintf("ERROR : "); \
		UpdaterLogPrintf("0x%08x (%d)\n", (err), (line)); \
	} while (0)

#define FAT_LOG_ERR_FMT(err, line, fmt, ...) \
	do { \
		UpdaterLogPrintf("ERROR : "); \
		UpdaterLogPrintf("0x%08x (%d) : " fmt "\n", (err), (line), __VA_ARGS__); \
	} while (0)

static const SceUInt8 s_lfnCharOffsets[FAT_LFN_CHARS_PER_ENTRY] = {
	1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
};

static inline void putLe16(SceUInt8 *dst, SceUInt16 value)
{
	dst[0] = (SceUInt8)(value & 0xffU);
	dst[1] = (SceUInt8)((value >> 8) & 0xffU);
}

static inline void putLe32(SceUInt8 *dst, SceUInt32 value)
{
	dst[0] = (SceUInt8)(value & 0xffU);
	dst[1] = (SceUInt8)((value >> 8) & 0xffU);
	dst[2] = (SceUInt8)((value >> 16) & 0xffU);
	dst[3] = (SceUInt8)((value >> 24) & 0xffU);
}

MemoryFatFs::MemoryFatFs()
{
	m_unusedState = 0;
	m_clusterBuffer = NULL;
	m_fatBuffer = NULL;
	m_serialNumber = 0;
	m_fatStartSector = 0;
	m_rootDirStartSector = 0;
	m_dataStartSector = 0;
	m_sectorsPerFat = 0;
	m_sectorsPerCluster = 0;
	m_totalClusters = 0;
	m_totalSectors = 0;
	m_fatType = MEMORY_FAT_TYPE_12;
	m_sectorSize = 0;
	m_clusterSize = 0;
	m_dirEntryIndex = 0;
	m_currentDirCluster = 0;
	m_dirStartCluster = 0;
	m_nextFreeCluster = 0;
	m_previousFatType = 0;
	m_imageBuffer = NULL;
	m_imageSize = 0;
	m_usedImageSize = 0;
}

MemoryFatFs::~MemoryFatFs()
{
}

int MemoryFatFs::ReadSectors(void *dst, SceUInt32 startSector, SceUInt32 sectorCount,
			     SceSize dstSize)
{
	SceUInt32 byteOffset = m_sectorSize * startSector;
	SceSize byteLen = m_sectorSize * sectorCount;

	if (byteOffset > m_imageSize || byteOffset + byteLen > m_imageSize || byteLen > dstSize) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 1212, "%d", byteOffset);
		return (int)SCE_UPDATER_ERROR_FAT_RANGE;
	}
	sceClibMemcpy(dst, m_imageBuffer + byteOffset, byteLen);
	return (int)(sectorCount * m_sectorSize);
}

int MemoryFatFs::WriteSectors(const void *src, SceUInt32 startSector, SceUInt32 sectorCount)
{
	SceUInt32 byteOffset = m_sectorSize * startSector;
	SceSize byteLen = m_sectorSize * sectorCount;

	if (byteOffset > m_imageSize || byteOffset + byteLen > m_imageSize) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 1174, "%d", byteOffset);
		return (int)SCE_UPDATER_ERROR_FAT_RANGE;
	}
	sceClibMemcpy(m_imageBuffer + byteOffset, src, byteLen);
	return 0;
}

int MemoryFatFs::FormatVolume(SceUInt32 sectorSize, SceUInt32 clusterSize, void *buffer,
			      SceUInt64 totalSize, const char *volumeLabel)
{
	SceUInt32 totalSectors;
	SceUInt32 estClusters;
	SceUInt32 fatType;
	SceUInt32 prevFatType;
	SceUInt32 sectorsPerCluster;
	SceUInt32 fatBytes;
	SceUInt32 usableSectors;
	SceUInt32 reservedSectors;
	SceUInt16 rootEntryCount;
	SceUInt32 rootDirBytes;
	SceUInt32 rawFatSectors;
	SceUInt32 rootDirSectors;
	SceUInt32 layoutOverhead;
	SceUInt32 sectorsPerFat;
	SceUInt32 totalClusters;
	SceUInt32 currentSector;
	int ret;

	(void)buffer;
	totalSectors = (SceUInt32)(totalSize / sectorSize);
	m_totalSectors = totalSectors;
	estClusters = (SceUInt32)(totalSize / clusterSize);
	sectorsPerCluster = m_sectorsPerCluster;
	fatType = MEMORY_FAT_TYPE_12;
	if (estClusters >= FAT_CLUSTERS_FAT12_MAX)
		fatType = MEMORY_FAT_TYPE_16;
	if (estClusters >= FAT_CLUSTERS_FAT16_MAX)
		fatType = MEMORY_FAT_TYPE_32;
	prevFatType = m_previousFatType;

	for (;;) {
		if (fatType == MEMORY_FAT_TYPE_32) {
			rootDirBytes = 0;
			fatBytes = sectorSize + 4 * estClusters + 7;
			usableSectors = totalSectors - 32;
			rootEntryCount = 0;
			reservedSectors = 32;
		} else if (fatType == MEMORY_FAT_TYPE_16) {
			fatBytes = sectorSize + 2 * estClusters + 3;
			usableSectors = totalSectors - 2;
			reservedSectors = 2;
			rootEntryCount = 512;
			rootDirBytes = 0x4000;
		} else if (fatType == MEMORY_FAT_TYPE_12) {
			fatBytes = sectorSize + ((3 * estClusters + 1) >> 1) + 2;
			usableSectors = totalSectors - 2;
			reservedSectors = 2;
			rootEntryCount = 512;
			rootDirBytes = 0x4000;
		} else {
			m_sectorsPerCluster = sectorsPerCluster;
			m_previousFatType = prevFatType;
			m_fatType = fatType;
			return -1;
		}

		rawFatSectors = fatBytes / sectorSize;
		rootDirSectors = rootDirBytes / sectorSize;
		layoutOverhead = reservedSectors + 2 * rawFatSectors + rootDirSectors;
		sectorsPerCluster = clusterSize / sectorSize;
		sectorsPerFat = rawFatSectors +
				((((sectorsPerCluster + layoutOverhead - 1) &
				   ~(sectorsPerCluster - 1)) - layoutOverhead) >> 1);
		totalClusters = (usableSectors - 2 * sectorsPerFat - rootDirSectors) /
				sectorsPerCluster;

		if ((fatType == MEMORY_FAT_TYPE_16 && totalClusters < FAT_CLUSTERS_FAT12_MAX) ||
		    (fatType == MEMORY_FAT_TYPE_32 && totalClusters < FAT_CLUSTERS_FAT16_MAX)) {
			estClusters = totalClusters;
			if (prevFatType != 0) {
				m_sectorsPerCluster = sectorsPerCluster;
				m_previousFatType = prevFatType;
				m_fatType = fatType;
				FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_FORMAT, 489, "%x",
						(int)SCE_UPDATER_ERROR_FAT_FORMAT);
				return (int)SCE_UPDATER_ERROR_FAT_FORMAT;
			}
			prevFatType = fatType;
			fatType >>= 4;
			continue;
		}
		break;
	}

	m_totalClusters = totalClusters;
	m_fatType = fatType;
	m_previousFatType = prevFatType;
	m_sectorsPerCluster = sectorsPerCluster;

	/* Build BPB boot sector */
	sceClibMemset(m_sectorBuffer, 0, FAT_SECTOR_SIZE_MAX);
	m_sectorBuffer[0] = 0xeb;
	m_sectorBuffer[1] = 0xfe;
	m_sectorBuffer[2] = 0x90;
	m_sectorBuffer[3] = 0;
	sceClibMemcpy(&m_sectorBuffer[3], "SCEI       ", 8);
	putLe16(&m_sectorBuffer[11], (SceUInt16)sectorSize);
	m_sectorBuffer[13] = (SceUInt8)m_sectorsPerCluster;
	putLe16(&m_sectorBuffer[14], (SceUInt16)reservedSectors);
	m_sectorBuffer[16] = 2;
	putLe16(&m_sectorBuffer[17], rootEntryCount);
	if (totalSectors >= 0x10000U)
		putLe32(&m_sectorBuffer[32], totalSectors);
	else
		putLe16(&m_sectorBuffer[19], (SceUInt16)totalSectors);
	m_sectorBuffer[21] = 0xf8;
	putLe16(&m_sectorBuffer[24], 63);
	putLe16(&m_sectorBuffer[26], 255);
	putLe32(&m_sectorBuffer[28], 0);

	if (m_fatType == MEMORY_FAT_TYPE_32) {
		putLe32(&m_sectorBuffer[67], m_serialNumber);
		putLe32(&m_sectorBuffer[36], sectorsPerFat);
		putLe32(&m_sectorBuffer[44], 2);
		putLe16(&m_sectorBuffer[48], 1);
		putLe16(&m_sectorBuffer[50], 6);
		m_sectorBuffer[64] = 0x80;
		m_sectorBuffer[66] = 0x29;
		sceClibMemcpy(&m_sectorBuffer[71], "NO NAME    FAT32   ", 19);
		if (volumeLabel != NULL) {
			sceClibMemset(&m_sectorBuffer[71], ' ', FAT_SHORT_NAME_LEN);
			sceClibStrncpy(reinterpret_cast<char *>(&m_sectorBuffer[71]),
				       volumeLabel, FAT_SHORT_NAME_LEN);
		}
	} else {
		putLe32(&m_sectorBuffer[39], m_serialNumber);
		putLe16(&m_sectorBuffer[22], (SceUInt16)sectorsPerFat);
		m_sectorBuffer[36] = 0x80;
		m_sectorBuffer[38] = 0x29;
		if (m_fatType == MEMORY_FAT_TYPE_12)
			sceClibMemcpy(&m_sectorBuffer[43], "NO NAME    FAT12   ", 19);
		else
			sceClibMemcpy(&m_sectorBuffer[43], "NO NAME    FAT16   ", 19);
		if (volumeLabel != NULL) {
			sceClibMemset(&m_sectorBuffer[43], ' ', FAT_SHORT_NAME_LEN);
			sceClibStrncpy(reinterpret_cast<char *>(&m_sectorBuffer[43]),
				       volumeLabel, FAT_SHORT_NAME_LEN);
		}
	}
	m_sectorBuffer[510] = 0x55;
	m_sectorBuffer[511] = 0xaa;

	m_fatStartSector = reservedSectors;
	m_sectorsPerFat = sectorsPerFat;

	ret = WriteSectors(m_sectorBuffer, 0, 1);
	if (ret < 0)
		return ret;
	if (m_fatType == MEMORY_FAT_TYPE_32) {
		ret = WriteSectors(m_sectorBuffer, 6, 1);
		if (ret < 0)
			return ret;
	}

	currentSector = reservedSectors;
	for (int fatCopy = 0; fatCopy < 2; ++fatCopy) {
		sceClibMemset(m_sectorBuffer, 0, FAT_SECTOR_SIZE_MAX);
		if (m_fatType == MEMORY_FAT_TYPE_32) {
			putLe32(&m_sectorBuffer[0], 0xfffffffaU - 2);
			putLe32(&m_sectorBuffer[4], 0xffffffffU);
			putLe32(&m_sectorBuffer[8], 0x0fffffffU);
		} else if (m_fatType == MEMORY_FAT_TYPE_12) {
			putLe32(&m_sectorBuffer[0], 0x00fffff8U);
		} else {
			putLe32(&m_sectorBuffer[0], 0xfffffff8U);
		}
		ret = WriteSectors(m_sectorBuffer, currentSector++, 1);
		if (ret < 0)
			return ret;

		sceClibMemset(m_sectorBuffer, 0, FAT_SECTOR_SIZE_MAX);
		for (SceUInt32 sec = 1; sec < sectorsPerFat; ++sec) {
			ret = WriteSectors(m_sectorBuffer, currentSector++, 1);
			if (ret < 0)
				return ret;
		}
	}

	m_rootDirStartSector = currentSector;
	m_dataStartSector = currentSector + rootDirSectors;

	{
		SceUInt8 rootClearSectors = (SceUInt8)(m_fatType == MEMORY_FAT_TYPE_32
						       ? m_sectorsPerCluster : rootDirSectors);

		for (int sec = 0; sec < rootClearSectors; ++sec) {
			ret = WriteSectors(m_sectorBuffer, currentSector++, 1);
			if (ret < 0)
				return ret;
		}
	}

	if (m_fatType == MEMORY_FAT_TYPE_32) {
		SceUInt32 freeClusters = totalClusters - 1;

		m_sectorBuffer[510] = 0x55;
		m_sectorBuffer[511] = 0xaa;
		m_sectorBuffer[0] = 'R';
		m_sectorBuffer[1] = 'R';
		m_sectorBuffer[2] = 'a';
		m_sectorBuffer[3] = 'A';
		m_sectorBuffer[484] = 'r';
		m_sectorBuffer[485] = 'r';
		m_sectorBuffer[486] = 'A';
		m_sectorBuffer[487] = 'a';
		putLe32(&m_sectorBuffer[488], freeClusters);
		putLe32(&m_sectorBuffer[492], 0xffffffffU);
		ret = WriteSectors(m_sectorBuffer, 1, 1);
		if (ret < 0)
			return ret;
		ret = WriteSectors(m_sectorBuffer, 7, 1);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int MemoryFatFs::Initialize(SceUInt32 sectorSize, SceUInt32 clusterSize, SceUInt64 totalSize,
			    const char *volumeLabel, SceUInt32 serialNumber, void *buffer)
{
	int ret;

	m_sectorSize = sectorSize;
	m_clusterSize = clusterSize;
	m_imageBuffer = static_cast<SceUInt8 *>(buffer);
	m_imageSize = (SceUInt32)totalSize;
	m_serialNumber = serialNumber;

	ret = FormatVolume(sectorSize, clusterSize, buffer, totalSize, volumeLabel);
	if (ret != 0)
		return ret;

	m_clusterBuffer = static_cast<SceUInt8 *>(
		g_updaterMemoryInfo.updaterCoreAlloc(clusterSize));
	if (m_clusterBuffer == NULL) {
		FAT_LOG_ERR_FMT(SCE_UPDATER_ERROR_NO_MEMORY, 266, "%u", clusterSize);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}
	m_fatBuffer = static_cast<SceUInt8 *>(
		g_updaterMemoryInfo.updaterCoreAlloc(m_sectorsPerFat * m_sectorSize));
	if (m_fatBuffer == NULL) {
		FAT_LOG_ERR_FMT(SCE_UPDATER_ERROR_NO_MEMORY, 274, "%u * %u",
				m_sectorsPerFat, m_sectorSize);
		return SCE_UPDATER_ERROR_NO_MEMORY;
	}

	ret = ReadSectors(m_fatBuffer, m_fatStartSector, m_sectorsPerFat,
			  m_sectorsPerFat * m_sectorSize);
	if (ret == -1)
		return ret;

	m_dirStartCluster = 0;
	m_dirEntryIndex = 0;
	m_nextFreeCluster = 3;
	return 0;
}

int MemoryFatFs::Finalize(void)
{
	int ret;

	ret = WriteSectors(m_fatBuffer, m_fatStartSector, m_sectorsPerFat);
	if (ret < 0) {
		g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
		g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
		return ret;
	}
	ret = WriteSectors(m_fatBuffer, m_fatStartSector + m_sectorsPerFat, m_sectorsPerFat);
	if (ret < 0) {
		g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
		g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
		return ret;
	}
	if (m_fatType == MEMORY_FAT_TYPE_32) {
		ret = ReadSectors(m_sectorBuffer, 1, 1, FAT_SECTOR_SIZE_MAX);
		if (ret < 0) {
			g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
			g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
			return ret;
		}
		putLe32(&m_sectorBuffer[488],
			(SceUInt32)(m_totalClusters - (SceUInt32)m_nextFreeCluster + 1));
		putLe32(&m_sectorBuffer[492], (SceUInt32)(m_nextFreeCluster - 1));
		/* Sony checks WriteSectors(1) without overwriting ret; preserve exact return */
		if (WriteSectors(m_sectorBuffer, 1, 1) < 0) {
			g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
			g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
			return ret;
		}
		ret = WriteSectors(m_sectorBuffer, 7, 1);
		if (ret < 0) {
			g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
			g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
			return ret;
		}
	}
	g_updaterMemoryInfo.updaterCoreFree(m_clusterBuffer);
	g_updaterMemoryInfo.updaterCoreFree(m_fatBuffer);
	return 0;
}

SceUInt32 MemoryFatFs::ClusterToSector(SceUInt32 cluster)
{
	if (cluster == 0)
		return m_rootDirStartSector;
	if (cluster - 2 < m_totalClusters)
		return (cluster - 2) * m_sectorsPerCluster + m_dataStartSector;
	return 0;
}

SceUInt8 MemoryFatFs::ComputeShortNameChecksum(void)
{
	SceUInt8 sum = 0;

	for (int i = 0; i < FAT_SHORT_NAME_LEN; ++i)
		sum = (SceUInt8)(((sum & 1U) ? 0x80U : 0U) + (sum >> 1) +
				 (SceUInt8)m_shortName[i]);
	return sum;
}

int MemoryFatFs::SetFatEntry(SceUInt32 cluster, SceUInt32 value)
{
	if (value == FAT_EOC_MARK) {
		switch (m_fatType) {
		case MEMORY_FAT_TYPE_32:
			value = FAT_EOC_FAT32;
			break;
		case MEMORY_FAT_TYPE_16:
			value = FAT_EOC_FAT16;
			break;
		case MEMORY_FAT_TYPE_12:
			value = FAT_EOC_FAT12;
			break;
		default:
			break;
		}
	}

	switch (m_fatType) {
	case MEMORY_FAT_TYPE_12: {
		SceUInt32 fatOffset = cluster + (cluster >> 1);
		SceUInt32 sectorBase = (fatOffset / m_sectorSize) * m_sectorSize;
		SceUInt32 bytePos = sectorBase + (fatOffset & (m_sectorSize - 1));
		SceUInt32 nextOffset = fatOffset + 1;
		SceUInt32 nextSectorBase;

		if ((cluster & 1U) != 0)
			m_fatBuffer[bytePos] = (SceUInt8)((m_fatBuffer[bytePos] & 0x0fU) |
							  ((value << 4) & 0xf0U));
		else
			m_fatBuffer[bytePos] = (SceUInt8)(value & 0xffU);

		nextSectorBase = (nextOffset / m_sectorSize) * m_sectorSize;
		if (sectorBase != nextSectorBase)
			sectorBase += m_sectorSize;
		bytePos = sectorBase + (nextOffset & (m_sectorSize - 1));
		if ((cluster & 1U) != 0)
			m_fatBuffer[bytePos] = (SceUInt8)((value >> 4) & 0xffU);
		else
			m_fatBuffer[bytePos] = (SceUInt8)((m_fatBuffer[bytePos] & 0xf0U) |
							  ((value >> 8) & 0x0fU));
		break;
	}
	case MEMORY_FAT_TYPE_16: {
		SceUInt32 bytePos = (cluster / (m_sectorSize >> 1)) * m_sectorSize +
				    ((cluster * 2) & (m_sectorSize - 1));

		m_fatBuffer[bytePos] = (SceUInt8)(value & 0xffU);
		m_fatBuffer[bytePos + 1] = (SceUInt8)((value >> 8) & 0xffU);
		break;
	}
	case MEMORY_FAT_TYPE_32: {
		SceUInt32 bytePos = (cluster / (m_sectorSize >> 2)) * m_sectorSize +
				    ((cluster * 4) & (m_sectorSize - 1));

		m_fatBuffer[bytePos] = (SceUInt8)(value & 0xffU);
		m_fatBuffer[bytePos + 1] = (SceUInt8)((value >> 8) & 0xffU);
		m_fatBuffer[bytePos + 2] = (SceUInt8)((value >> 16) & 0xffU);
		m_fatBuffer[bytePos + 3] = (SceUInt8)((value >> 24) & 0xffU);
		break;
	}
	default:
		return -1;
	}
	return 0;
}

SceUInt32 MemoryFatFs::AllocateCluster(int *error)
{
	SceUInt32 cluster;
	SceUInt32 byteOffset;

	*error = 0;
	if (m_nextFreeCluster >= (SceUInt64)(m_totalClusters + 2)) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_NO_SPACE, 1679, "%llu\n >= (%d + 2)",
				m_nextFreeCluster, m_totalClusters);
		*error = (int)SCE_UPDATER_ERROR_FAT_NO_SPACE;
		return 0;
	}

	cluster = (SceUInt32)m_nextFreeCluster;
	SetFatEntry(cluster, FAT_EOC_MARK);
	sceClibMemset(m_clusterBuffer, 0, m_clusterSize);
	byteOffset = ClusterToSector(cluster) * m_sectorSize;
	if (byteOffset > m_imageSize || byteOffset + m_clusterSize > m_imageSize) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 1699, "%d", byteOffset);
		*error = (int)SCE_UPDATER_ERROR_FAT_RANGE;
		return 0;
	}
	sceClibMemcpy(m_imageBuffer + byteOffset, m_clusterBuffer, m_clusterSize);
	m_nextFreeCluster++;
	return cluster;
}

int MemoryFatFs::WriteDirectoryEntry(const char *name, const SceIoStat *stat, int needsLfn,
				     int lfnLength, SceUInt32 timestamp, SceUInt32 firstCluster)
{
	SceUInt8 *cursor = m_clusterBuffer;
	int lfnEntryCount = 0;
	int totalSlots = 1;
	int overflowSlots = 0;
	int slotsInCurrent;
	SceUInt32 nextCluster = 0;
	SceUInt64 startIndex;
	SceUInt32 maxEntriesPerCluster = m_clusterSize / FAT_DIR_ENTRY_SIZE;
	int allocError;

	if (needsLfn != 0) {
		lfnEntryCount = (lfnLength + (FAT_LFN_CHARS_PER_ENTRY - 1)) /
				FAT_LFN_CHARS_PER_ENTRY;
		totalSlots = lfnEntryCount + 1;
	}
	if (m_dirEntryIndex + totalSlots > maxEntriesPerCluster)
		overflowSlots = (int)(m_dirEntryIndex + totalSlots - maxEntriesPerCluster);
	slotsInCurrent = totalSlots - overflowSlots;

	if (overflowSlots != 0) {
		nextCluster = AllocateCluster(&allocError);
		SetFatEntry((SceUInt32)m_currentDirCluster, nextCluster);
	}
	if (slotsInCurrent <= 0) {
		m_currentDirCluster = nextCluster;
		m_dirEntryIndex = 0;
		slotsInCurrent = totalSlots;
		overflowSlots = 0;
	}
	startIndex = m_dirEntryIndex;

	if (needsLfn != 0) {
		SceUInt8 checksum = ComputeShortNameChecksum();
		int padCount = FAT_LFN_CHARS_PER_ENTRY * lfnEntryCount - lfnLength;
		int seq = lfnEntryCount;
		int charPos = lfnLength - 1;
		int slotPos = FAT_LFN_CHARS_PER_ENTRY - 1;
		int writtenSlots = slotsInCurrent;

		cursor[0] = (SceUInt8)(seq | 0x40);
		cursor[11] = FAT_ATTR_LFN;
		cursor[12] = 0;
		cursor[13] = checksum;
		cursor[26] = 0;
		cursor[27] = 0;
		seq--;

		for (int pad = 0; pad < padCount; ++pad) {
			SceUInt8 off = s_lfnCharOffsets[slotPos--];

			cursor[off] = 0xff;
			cursor[off + 1] = 0xff;
		}
		while (slotPos >= 0) {
			SceUInt8 off = s_lfnCharOffsets[slotPos--];
			SceUInt16 ch = m_longName[charPos--];

			cursor[off] = (SceUInt8)(ch & 0xffU);
			cursor[off + 1] = (SceUInt8)((ch >> 8) & 0xffU);
		}
		m_dirEntryIndex++;
		writtenSlots--;
		cursor += FAT_DIR_ENTRY_SIZE;

		while (writtenSlots > 0 && charPos >= 0) {
			cursor[0] = (SceUInt8)(seq--);
			cursor[11] = FAT_ATTR_LFN;
			cursor[12] = 0;
			cursor[13] = checksum;
			cursor[26] = 0;
			cursor[27] = 0;
			for (int pos = FAT_LFN_CHARS_PER_ENTRY - 1; pos >= 0; --pos) {
				SceUInt8 off = s_lfnCharOffsets[pos];
				SceUInt16 ch = m_longName[charPos--];

				cursor[off] = (SceUInt8)(ch & 0xffU);
				cursor[off + 1] = (SceUInt8)((ch >> 8) & 0xffU);
			}
			m_dirEntryIndex++;
			writtenSlots--;
			cursor += FAT_DIR_ENTRY_SIZE;
		}

		if (overflowSlots != 0) {
			SceUInt32 byteOffset = ClusterToSector((SceUInt32)m_currentDirCluster) *
					       m_sectorSize +
					       (SceUInt32)(startIndex * FAT_DIR_ENTRY_SIZE);
			SceSize flushLen = (SceSize)(slotsInCurrent * FAT_DIR_ENTRY_SIZE);

			if (byteOffset > m_imageSize || byteOffset + flushLen > m_imageSize) {
				FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 1844, "%d",
						byteOffset);
				return (int)SCE_UPDATER_ERROR_FAT_RANGE;
			}
			if (byteOffset == 0) {
				FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 1848, "%d", 0);
				return (int)SCE_UPDATER_ERROR_FAT_RANGE;
			}
			sceClibMemcpy(m_imageBuffer + byteOffset, m_clusterBuffer, flushLen);

			cursor = m_clusterBuffer;
			m_currentDirCluster = nextCluster;
			m_dirEntryIndex = 0;
			startIndex = 0;

			while (seq != 0) {
				cursor[0] = (SceUInt8)(seq--);
				cursor[11] = FAT_ATTR_LFN;
				cursor[12] = 0;
				cursor[13] = checksum;
				cursor[26] = 0;
				cursor[27] = 0;
				for (int pos = FAT_LFN_CHARS_PER_ENTRY - 1; pos >= 0; --pos) {
					SceUInt8 off = s_lfnCharOffsets[pos];
					SceUInt16 ch = m_longName[charPos--];

					cursor[off] = (SceUInt8)(ch & 0xffU);
					cursor[off + 1] = (SceUInt8)((ch >> 8) & 0xffU);
				}
				m_dirEntryIndex++;
				cursor += FAT_DIR_ENTRY_SIZE;
			}
		}
	}

	sceClibMemset(cursor, 0, FAT_DIR_ENTRY_SIZE);
	sceClibMemcpy(cursor, m_shortName, FAT_SHORT_NAME_LEN);
	if (SCE_STM_ISDIR(stat->st_mode)) {
		if (stat->st_attr == FAT_STAT_ATTR_HIDDEN_SYSTEM) {
			cursor[11] = FAT_ATTR_DIRECTORY | FAT_ATTR_SYSTEM | FAT_ATTR_HIDDEN;
		} else if (stat->st_attr == FAT_STAT_ATTR_SYSTEM) {
			cursor[11] = FAT_ATTR_DIRECTORY | FAT_ATTR_SYSTEM;
		} else {
			FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_DIR_ATTR, 2027, "%s (%o)",
					name, stat->st_mode);
			return (int)SCE_UPDATER_ERROR_FAT_DIR_ATTR;
		}
		putLe32(&cursor[28], 0);
	} else if (SCE_STM_ISREG(stat->st_mode)) {
		if (stat->st_attr == FAT_STAT_ATTR_HIDDEN_SYSTEM) {
			cursor[11] = FAT_ATTR_ARCHIVE | FAT_ATTR_SYSTEM | FAT_ATTR_HIDDEN;
		} else if (stat->st_attr == FAT_STAT_ATTR_SYSTEM) {
			cursor[11] = FAT_ATTR_ARCHIVE | FAT_ATTR_SYSTEM;
		} else {
			FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_FILE_ATTR, 2041, "%s (%o)",
					name, stat->st_mode);
			return (int)SCE_UPDATER_ERROR_FAT_FILE_ATTR;
		}
		putLe32(&cursor[28], (SceUInt32)stat->st_size);
	} else {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_ENTRY_TYPE, 2047, "%s", name);
		return (int)SCE_UPDATER_ERROR_FAT_ENTRY_TYPE;
	}

	(void)timestamp;
	putLe32(&cursor[14], m_serialNumber);
	putLe32(&cursor[22], m_serialNumber);
	putLe16(&cursor[20], (SceUInt16)((firstCluster >> 16) & 0xffffU));
	putLe16(&cursor[26], (SceUInt16)(firstCluster & 0xffffU));

	m_dirEntryIndex++;
	{
		SceUInt32 byteOffset = ClusterToSector((SceUInt32)m_currentDirCluster) *
				       m_sectorSize +
				       (SceUInt32)(startIndex * FAT_DIR_ENTRY_SIZE);
		SceSize writeLen = (SceSize)((m_dirEntryIndex - startIndex) * FAT_DIR_ENTRY_SIZE);

		if (byteOffset > m_imageSize || byteOffset + writeLen > m_imageSize) {
			FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 2073, "%d", byteOffset);
			return (int)SCE_UPDATER_ERROR_FAT_RANGE;
		}
		sceClibMemcpy(m_imageBuffer + byteOffset, m_clusterBuffer, writeLen);
	}
	return 0;
}

bool MemoryFatFs::IsEndOfClusterChain(SceUInt32 cluster)
{
	if (m_fatType == MEMORY_FAT_TYPE_32)
		return cluster >= FAT_EOC_MIN_FAT32;
	if (m_fatType == MEMORY_FAT_TYPE_16)
		return cluster >= FAT_EOC_MIN_FAT16;
	if (m_fatType == MEMORY_FAT_TYPE_12)
		return cluster >= FAT_EOC_MIN_FAT12;
	return false;
}

SceUInt32 MemoryFatFs::GetFatEntry(SceUInt32 cluster)
{
	switch (m_fatType) {
	case MEMORY_FAT_TYPE_12: {
		SceUInt32 fatOffset = cluster + (cluster >> 1);
		SceUInt32 sectorBase = (fatOffset / m_sectorSize) * m_sectorSize;
		SceUInt32 lo = m_fatBuffer[sectorBase + (fatOffset & (m_sectorSize - 1))];
		SceUInt32 nextOffset = fatOffset + 1;
		SceUInt32 hi;
		SceUInt32 combined;

		if (sectorBase != (nextOffset / m_sectorSize) * m_sectorSize)
			sectorBase += m_sectorSize;
		hi = m_fatBuffer[sectorBase + (nextOffset & (m_sectorSize - 1))];
		combined = lo | (hi << 8);
		return (cluster & 1U) != 0 ? (combined >> 4) : (combined & 0xfffU);
	}
	case MEMORY_FAT_TYPE_16: {
		SceUInt32 bytePos = (cluster / (m_sectorSize >> 1)) * m_sectorSize +
				    ((cluster * 2) & (m_sectorSize - 1));

		return (SceUInt32)m_fatBuffer[bytePos] |
		       ((SceUInt32)m_fatBuffer[bytePos + 1] << 8);
	}
	case MEMORY_FAT_TYPE_32: {
		SceUInt32 bytePos = (cluster / (m_sectorSize >> 2)) * m_sectorSize +
				    ((cluster * 4) & (m_sectorSize - 1));

		return ((SceUInt32)m_fatBuffer[bytePos] |
			((SceUInt32)m_fatBuffer[bytePos + 1] << 8) |
			((SceUInt32)m_fatBuffer[bytePos + 2] << 16) |
			((SceUInt32)m_fatBuffer[bytePos + 3] << 24)) & 0x0fffffffU;
	}
	default:
		return (SceUInt32)-1;
	}
}

int MemoryFatFs::ShortNameExistsInCurrentDir(const void *shortName)
{
	SceUInt32 cluster = (SceUInt32)m_dirStartCluster;
	int bytesRead;
	SceUInt8 *entry;

	bytesRead = ReadSectors(m_clusterBuffer, ClusterToSector(cluster),
				m_sectorsPerCluster, m_clusterSize);
	entry = m_clusterBuffer;
	while (entry[0] != 0) {
		if (entry[0] != 0xe5 && (entry[11] & FAT_ATTR_LFN) != FAT_ATTR_LFN) {
			if (sceClibMemcmp(shortName, entry, FAT_SHORT_NAME_LEN) == 0)
				return 1;
		}
		entry += FAT_DIR_ENTRY_SIZE;
		if (entry >= m_clusterBuffer + bytesRead) {
			SceUInt32 nextCluster = GetFatEntry(cluster);

			if (IsEndOfClusterChain(nextCluster))
				return 0;
			cluster = nextCluster;
			bytesRead = ReadSectors(m_clusterBuffer, ClusterToSector(cluster),
						m_sectorsPerCluster, m_clusterSize);
			if (bytesRead < 0)
				return bytesRead;
			entry = m_clusterBuffer;
		}
	}
	return 0;
}

int MemoryFatFs::MakeShortAndLongName(const char *name, int *lfnLength)
{
	int len;
	int endIdx;
	int trimmedLen;
	int startIdx;
	int dotIdx;
	int partEnd;
	int dstPos = 0;
	int dstLimit;
	int lossy = 0;
	int hasLower = 0;
	int strippedLeading = 0;

	len = (int)sceClibStrnlen(name, FAT_MAX_NAME_LEN + 1);
	if ((SceUInt32)len > FAT_MAX_NAME_LEN) {
		FAT_LOG_ERR((int)SCE_UPDATER_ERROR_FAT_NAME_LONG, 1252);
		return (int)SCE_UPDATER_ERROR_FAT_NAME_LONG;
	}

	for (int i = 0; i < len; ++i) {
		unsigned char ch = (unsigned char)name[i];

		if (ch < 0x80 && sceClibStrchr("\"*:<>?|\x7f", ch) != NULL) {
			FAT_LOG_ERR((int)SCE_UPDATER_ERROR_FAT_NAME_CHAR, 1265);
			return (int)SCE_UPDATER_ERROR_FAT_NAME_CHAR;
		}
		if ((sceClibLookCtypeTable(ch) & FAT_CTYPE_PRINTABLE_MASK) == 0) {
			FAT_LOG_ERR((int)SCE_UPDATER_ERROR_FAT_NAME_CHAR, 1270);
			return (int)SCE_UPDATER_ERROR_FAT_NAME_CHAR;
		}
		m_longName[i] = ch;
	}

	endIdx = len - 1;
	while (endIdx >= 0 && (m_longName[endIdx] == ' ' || m_longName[endIdx] == '.'))
		endIdx--;
	if (endIdx == 0) {
		FAT_LOG_ERR((int)SCE_UPDATER_ERROR_FAT_NAME_EMPTY, 1286);
		return (int)SCE_UPDATER_ERROR_FAT_NAME_EMPTY;
	}

	trimmedLen = endIdx + 1;
	m_longName[trimmedLen] = 0;
	*lfnLength = trimmedLen + 1;

	sceClibMemset(m_shortName, ' ', FAT_SHORT_NAME_LEN);
	startIdx = 0;
	while (m_longName[startIdx] == ' ' || m_longName[startIdx] == '.')
		startIdx++;
	if (startIdx != 0)
		strippedLeading = 1;

	dotIdx = endIdx;
	do {
		dotIdx--;
	} while (m_longName[dotIdx] != '.' && dotIdx >= startIdx);

	partEnd = (dotIdx > startIdx) ? dotIdx : trimmedLen;
	dstLimit = 8;
	for (;;) {
		while (startIdx < partEnd && dstPos < dstLimit) {
			SceUInt16 ch = m_longName[startIdx++];

			if (ch == ' ' || ch == '.')
				continue;
			if (sceClibStrchr("+,;=[]", ch) != NULL) {
				lossy = 1;
				ch = '_';
			} else if (ch >= 'a' && ch <= 'z') {
				hasLower = 1;
				ch = (SceUInt16)(ch - ('a' - 'A'));
			}
			m_shortName[dstPos++] = (char)ch;
		}
		if (startIdx < partEnd)
			lossy = 1;
		if (dstLimit == FAT_SHORT_NAME_LEN || partEnd == trimmedLen)
			break;
		startIdx = ++dotIdx;
		partEnd = trimmedLen;
		dstPos = dstLimit;
		dstLimit = FAT_SHORT_NAME_LEN;
	}

	if (lossy != 0) {
		int stemLen = 0;
		int tildePos;
		int suffix = 1;

		while (stemLen < 6 && m_shortName[stemLen] != ' ')
			stemLen++;
		tildePos = stemLen < 6 ? stemLen : 6;
		m_shortName[tildePos] = '~';
		m_shortName[tildePos + 1] = '1';
		while (ShortNameExistsInCurrentDir(m_shortName) != 0) {
			suffix++;
			if (suffix < 10) {
				m_shortName[tildePos + 1] = (char)('0' + suffix);
			} else {
				int twoDigitPos = (tildePos == 6) ? 5 : tildePos;

				m_shortName[twoDigitPos] = '~';
				m_shortName[twoDigitPos + 1] = (char)('0' + suffix / 10);
				m_shortName[twoDigitPos + 2] = (char)('0' + suffix % 10);
				while (ShortNameExistsInCurrentDir(m_shortName) != 0) {
					suffix++;
					m_shortName[twoDigitPos + 1] = (char)('0' + suffix / 10);
					m_shortName[twoDigitPos + 2] = (char)('0' + suffix % 10);
				}
				break;
			}
		}
	}
	return hasLower | lossy | strippedLeading;
}

int MemoryFatFs::CreateDirectory(const char *name, const SceIoStat *stat, SceUInt32 *outCluster)
{
	int lfnLength;
	int needsLfn;
	int allocError;
	SceUInt32 dirCluster;
	SceUInt32 byteOffset;
	int ret;

	needsLfn = MakeShortAndLongName(name, &lfnLength);
	if (needsLfn < 0)
		return needsLfn;

	dirCluster = AllocateCluster(&allocError);
	if (allocError != 0)
		return allocError;

	/* Initialize "." and ".." entries in the new directory cluster */
	sceClibMemset(m_sectorBuffer, 0, FAT_SECTOR_SIZE_MAX);
	sceClibMemset(m_sectorBuffer, ' ', FAT_SHORT_NAME_LEN);
	m_sectorBuffer[0] = '.';
	m_sectorBuffer[11] = FAT_ATTR_DIRECTORY;
	putLe32(&m_sectorBuffer[14], m_serialNumber);
	putLe32(&m_sectorBuffer[22], m_serialNumber);
	putLe16(&m_sectorBuffer[20], (SceUInt16)((dirCluster >> 16) & 0xffffU));
	putLe16(&m_sectorBuffer[26], (SceUInt16)(dirCluster & 0xffffU));

	sceClibMemcpy(&m_sectorBuffer[FAT_DIR_ENTRY_SIZE], m_sectorBuffer, FAT_DIR_ENTRY_SIZE);
	m_sectorBuffer[FAT_DIR_ENTRY_SIZE + 1] = '.';
	{
		SceUInt32 parentCluster = (SceUInt32)m_dirStartCluster;

		putLe16(&m_sectorBuffer[FAT_DIR_ENTRY_SIZE + 20],
			(SceUInt16)((parentCluster >> 16) & 0xffffU));
		putLe16(&m_sectorBuffer[FAT_DIR_ENTRY_SIZE + 26],
			(SceUInt16)(parentCluster & 0xffffU));
	}

	byteOffset = ClusterToSector(dirCluster) * m_sectorSize;
	if (byteOffset > m_imageSize || byteOffset + 2 * FAT_DIR_ENTRY_SIZE > m_imageSize) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 761, "%d", byteOffset);
		return (int)SCE_UPDATER_ERROR_FAT_RANGE;
	}
	if (byteOffset == 0) {
		FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 765, "%d", 0);
		return (int)SCE_UPDATER_ERROR_FAT_RANGE;
	}
	sceClibMemcpy(m_imageBuffer + byteOffset, m_sectorBuffer, 2 * FAT_DIR_ENTRY_SIZE);

	ret = WriteDirectoryEntry(name, stat, needsLfn, lfnLength, m_serialNumber, dirCluster);
	if (ret != 0)
		return ret;
	*outCluster = dirCluster;
	return 0;
}

int MemoryFatFs::AddFileFromPath(const char *name, const char *srcPath, const SceIoStat *stat)
{
	int lfnLength;
	int needsLfn;
	int allocError;
	SceUInt32 currentCluster;
	int ret;

	needsLfn = MakeShortAndLongName(name, &lfnLength);
	if (needsLfn < 0)
		return needsLfn;

	currentCluster = AllocateCluster(&allocError);
	if (allocError != 0)
		return allocError;

	ret = WriteDirectoryEntry(name, stat, needsLfn, lfnLength, m_serialNumber, currentCluster);
	if (ret != 0)
		return ret;

	{
		SceUInt32 fileSize = (SceUInt32)stat->st_size;

		if (fileSize != 0) {
			SceUInt32 clusterCount = (fileSize + m_clusterSize - 1) / m_clusterSize;
			SceUID fd = sceIoOpen(srcPath, SCE_O_RDONLY, 0);

			if (fd < 0) {
				FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_MEMFS_OPEN, 835,
						"%s %o : %x", srcPath, 0, fd);
				return fd;
			}

			for (SceUInt32 idx = 0; idx < clusterCount; ++idx) {
				SceSSize readLen = sceIoRead(fd, m_clusterBuffer, m_clusterSize);
				SceUInt32 byteOffset;
				SceUInt64 endPos;

				if (readLen < 0) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_MEMFS_READ, 850,
							"%x %d : %x", fd, m_clusterSize, readLen);
					return readLen;
				}
				byteOffset = ClusterToSector(currentCluster) * m_sectorSize;
				if (byteOffset > m_imageSize ||
				    byteOffset + (SceSize)readLen > m_imageSize) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 870,
							"%d", byteOffset);
					return (int)SCE_UPDATER_ERROR_FAT_RANGE;
				}
				if (byteOffset == 0) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 874,
							"%d", 0);
					return (int)SCE_UPDATER_ERROR_FAT_RANGE;
				}
				sceClibMemcpy(m_imageBuffer + byteOffset, m_clusterBuffer,
					      (SceSize)readLen);

				endPos = (SceUInt64)(byteOffset + m_clusterSize);
				m_usedImageSize = endPos;
				if (endPos % m_clusterSize != 0)
					m_usedImageSize = (endPos / m_clusterSize) * m_clusterSize +
							  m_clusterSize;

				if (idx + 1 == clusterCount)
					break;
				{
					int nextErr;
					SceUInt32 nextCluster = AllocateCluster(&nextErr);

					SetFatEntry(currentCluster, nextCluster);
					currentCluster = nextCluster;
				}
			}

			ret = sceIoClose(fd);
			if (ret < 0)
				FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_MEMFS_CLOSE, 907,
						"%x : %x", fd, ret);
		}
	}
	return 0;
}

int MemoryFatFs::AddFileFromMemory(const char *name, const SceIoStat *stat, const void *fileData)
{
	int lfnLength;
	int needsLfn;
	int allocError;
	SceUInt32 currentCluster;
	int ret;

	needsLfn = MakeShortAndLongName(name, &lfnLength);
	if (needsLfn < 0)
		return needsLfn;

	currentCluster = AllocateCluster(&allocError);
	if (allocError != 0)
		return allocError;

	ret = WriteDirectoryEntry(name, stat, needsLfn, lfnLength, m_serialNumber, currentCluster);
	if (ret != 0)
		return ret;

	{
		SceUInt32 fileSize = (SceUInt32)stat->st_size;

		if (fileSize != 0) {
			SceUInt32 clusterCount = (fileSize + m_clusterSize - 1) / m_clusterSize;
			SceUInt32 remaining = fileSize;
			const SceUInt8 *src = static_cast<const SceUInt8 *>(fileData);

			for (SceUInt32 idx = 0; idx < clusterCount; ++idx) {
				SceUInt32 byteOffset = ClusterToSector(currentCluster) *
						       m_sectorSize;
				SceSize copyLen;
				SceUInt64 endPos;

				if (byteOffset > m_imageSize) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 964,
							"%d", byteOffset);
					return (int)SCE_UPDATER_ERROR_FAT_RANGE;
				}
				if (byteOffset == 0) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 968,
							"%d", 0);
					return (int)SCE_UPDATER_ERROR_FAT_RANGE;
				}
				copyLen = remaining < m_clusterSize ? remaining : m_clusterSize;
				if (byteOffset + copyLen > m_imageSize) {
					FAT_LOG_ERR_FMT((int)SCE_UPDATER_ERROR_FAT_RANGE, 979,
							"%d", byteOffset);
					return (int)SCE_UPDATER_ERROR_FAT_RANGE;
				}
				sceClibMemcpy(m_imageBuffer + byteOffset, src, copyLen);

				endPos = (SceUInt64)(byteOffset + m_clusterSize);
				m_usedImageSize = endPos;
				if (endPos % m_clusterSize != 0)
					m_usedImageSize = (endPos / m_clusterSize) * m_clusterSize +
							  m_clusterSize;

				src += copyLen;
				remaining -= copyLen;
				if (idx + 1 == clusterCount)
					break;
				{
					int nextErr;
					SceUInt32 nextCluster = AllocateCluster(&nextErr);

					SetFatEntry(currentCluster, nextCluster);
					currentCluster = nextCluster;
				}
			}
		}
	}
	return 0;
}

void MemoryFatFs::SetDirectoryContext(SceUInt64 entryIndex, SceUInt64 currentCluster,
				      SceUInt64 dirStartCluster)
{
	m_dirEntryIndex = entryIndex;
	m_dirStartCluster = dirStartCluster;
	m_currentDirCluster = currentCluster;
}

void MemoryFatFs::GetDirectoryContext(SceUInt64 *entryIndex, SceUInt64 *currentCluster,
				      SceUInt64 *dirStartCluster)
{
	*entryIndex = m_dirEntryIndex;
	*dirStartCluster = m_dirStartCluster;
	*currentCluster = m_currentDirCluster;
}

SceUInt64 MemoryFatFs::GetUsedImageSize(void)
{
	return m_usedImageSize;
}
