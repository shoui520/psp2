/*
 * partition.c — Master Boot Record (MBR) & SLB2 Boot Loader File System
 *
 * Address range: 0x805ad8..0x806063
 *
 * Parses the Sony Computer Entertainment MBR at sector 0 of eMMC/SD storage,
 * determines whether the active SLB2 container resides in the primary or
 * backup boot loader slot, identifies the active os0 partition slot, and
 * provides a file-descriptor-based reader for entries inside the SLB2
 * container cached at 0x81e000.
 */

#include "second_loader.h"

#define SCE_MBR_PART_OS0	0x03U
#define SCE_SLB2_CACHE_ADDR	0x0081e000U
#define SCE_SECTOR_BOUNCE_ADDR	0x40000200U

typedef struct SceSlb2Fd { /* name ours, open SLB2 file state */
	s64	offset;
	u32	start_sector;
	u32	file_size;
	u32	sector_size;
	u32	dev_id;
} SceSlb2Fd;

/* .rodata: 0x80c164..0x80c187 */
static const char s_sceMbrMagic[36] = "Sony Computer Entertainment Inc.";

/* .data: 0x80ca08..0x80ca0f */
static u32 s_activeOs0PartitionId[2] = { 2, 2 };

/* .bss: 0x80d240..0x80d273 */
static s32 s_slb2InitState[2];
static u32 s_slb2StartSector[2];
static u32 s_slb2IsFirstSlot[2];
static SceSlb2Fd s_slb2Fds[1];
static u32 s_slb2OpenMask;

/*
 * 0x805ad8: slb2InitDevice (name ours)
 *
 * Reads the MBR at sector 0 of device `devId` (0 = eMMC, 1 = SD), validates
 * the Sony MBR magic, records the active SLB2 start sector, redundancy slot
 * match flag, and active os0 partition ID, then reads the SLB2 header into
 * SRAM at 0x81e000 + devId * 0x200.
 */
int slb2InitDevice(u32 devId)
{
	int (*readSectors)(void *dst, u32 size, u32 startSector);
	const SceMbr *mbr = (const SceMbr *)SCE_SECTOR_BOUNCE_ADDR;
	SceSlb2Header *slb2Hdr;
	u32 activeSlb2Sector;
	u32 firstSlb2Sector;
	u32 os0InactiveCount;
	u32 i;
	int res;

	res = s_slb2InitState[devId];
	if (res < 0)
		return res;
	if (res == 1)
		return 0;
	if (res != 0)
		return SCE_SBL_ERROR_SL_EINVAL;

	readSectors = (devId == 0) ? readBootStorageSectors0 : readBootStorageSectors1;
	res = readSectors((void *)SCE_SECTOR_BOUNCE_ADDR, 0x200, 0);
	if (res != 0) {
		s_slb2InitState[devId] = SCE_SBL_ERROR_SL_EIO;
		return SCE_SBL_ERROR_SL_EIO;
	}

	if (memcmp((const void *)SCE_SECTOR_BOUNCE_ADDR, s_sceMbrMagic, sizeof(s_sceMbrMagic)) != 0) {
		s_slb2InitState[devId] = SCE_SBL_ERROR_SL_EILLEGAL;
		return SCE_SBL_ERROR_SL_EILLEGAL;
	}

	activeSlb2Sector = mbr->current_bl_lba;
	firstSlb2Sector = mbr->bl_bank0_lba;
	s_slb2StartSector[devId] = activeSlb2Sector;
	s_slb2IsFirstSlot[devId] = (activeSlb2Sector == firstSlb2Sector);

	os0InactiveCount = 0;
	for (i = 0; i < 16; i++) {
		if (mbr->partitions[i].code == SCE_MBR_PART_OS0) {
			if ((mbr->partitions[i].active & 1U) != 0)
				break;
			os0InactiveCount++;
		}
	}
	s_activeOs0PartitionId[devId] = ((os0InactiveCount == 0) ^ 1U) + 2U;

	res = readSectors((void *)SCE_SECTOR_BOUNCE_ADDR, 0x200, activeSlb2Sector);
	if (res != 0) {
		res = SCE_SBL_ERROR_SL_EIO;
		s_slb2InitState[devId] = res;
		return res;
	}

	slb2Hdr = (SceSlb2Header *)(SCE_SLB2_CACHE_ADDR + devId * 0x200U);
	memcpy(slb2Hdr, (const void *)SCE_SECTOR_BOUNCE_ADDR, 0x200);
	if (slb2Hdr->magic == SCE_SLB2_MAGIC) {
		s_slb2InitState[devId] = 1;
		return 0;
	}

	res = SCE_SBL_ERROR_SL_EILLEGAL;
	s_slb2InitState[devId] = res;
	return res;
}

/*
 * 0x805c52: slb2LookupFile (name ours)
 *
 * Searches the cached SLB2 directory for `invFileName` (stored in bitwise-NOT
 * obfuscated form by the caller) and returns the absolute start sector and
 * byte size of the matching entry.
 */
int slb2LookupFile(u32 devId, const char *invFileName, u32 *outStartSector, u32 *outFileSize)
{
	const SceSlb2Header *slb2Hdr;
	const SceSlb2Entry *entry;
	u32 i;
	u32 j;
	int res;

	if (devId >= 2)
		return SCE_SBL_ERROR_SL_EINVAL;

	*outFileSize = 0;
	*outStartSector = 0;

	res = slb2InitDevice(devId);
	if (res != 0)
		return res;

	slb2Hdr = (const SceSlb2Header *)(SCE_SLB2_CACHE_ADDR + devId * 0x200U);
	entry = slb2Hdr->entryList;
	for (i = 0; i < slb2Hdr->entryNum; i++, entry++) {
		if (i == 10) {
			uartDebugReportEvent(2, 0x62, 0x800f0017U, 0);
			return SCE_SBL_ERROR_SL_ENOENT;
		}
		for (j = 0; j < 32; j++) {
			if (invFileName[j] == '\0' && entry->m_file_name[j] == '\0') {
				*outStartSector = s_slb2StartSector[devId] + entry->fileStartSector;
				*outFileSize = entry->fileSizeInByte;
				return 0;
			}
			if ((s8)invFileName[j] != ~(s8)entry->m_file_name[j])
				break;
		}
	}

	return SCE_SBL_ERROR_SL_ENOENT;
}

/* 0x805d20: slb2IsPrimaryBootSlot (name ours) */
int slb2IsPrimaryBootSlot(u32 devId, u32 *outIsPrimary)
{
	int res = SCE_SBL_ERROR_SL_EINVAL;

	if (devId < 2) {
		res = slb2InitDevice(devId);
		if (res == 0)
			*outIsPrimary = s_slb2IsFirstSlot[devId];
	}
	return res;
}

/* 0x805d58: slb2GetActiveOs0PartitionId (name ours) */
int slb2GetActiveOs0PartitionId(u32 devId, u32 *outPartitionId)
{
	int res = SCE_SBL_ERROR_SL_EINVAL;

	if (devId < 2) {
		res = slb2InitDevice(devId);
		if (res == 0)
			*outPartitionId = s_activeOs0PartitionId[devId];
	}
	return res;
}

/* 0x805d90: partitionStorageIsOpen (name ours) */
u32 partitionStorageIsOpen(int fd)
{
	if (fd == 0)
		return s_slb2OpenMask & 1U;
	return 0;
}

/* 0x805da0: partitionStorageOpen (name ours) */
int partitionStorageOpen(u32 devId, const char *invFileName, int flags)
{
	u32 startSector = 0;
	u32 fileSize = 0;

	(void)flags;

	if (devId < 2 &&
	    slb2LookupFile(devId, invFileName, &startSector, &fileSize) == 0 &&
	    (s_slb2OpenMask & 1U) == 0) {
		s_slb2OpenMask |= 1U;
		s_slb2Fds[0].offset = 0;
		s_slb2Fds[0].dev_id = devId;
		s_slb2Fds[0].start_sector = startSector;
		s_slb2Fds[0].file_size = fileSize;
		s_slb2Fds[0].sector_size = 0x200;
		return 0;
	}
	return -1;
}

/* 0x805e04: partitionStorageClose (name ours) */
int partitionStorageClose(int fd)
{
	if (partitionStorageIsOpen(fd) == 0 || fd != 0)
		return -1;
	s_slb2OpenMask &= ~1U;
	return 0;
}

/* 0x805e2e: partitionStorageLseek (name ours) */
s64 partitionStorageLseek(int fd, s64 offset, int whence)
{
	s64 newOffset;

	if (partitionStorageIsOpen(fd) != 0) {
		if (whence == 1)
			newOffset = offset + s_slb2Fds[fd].offset;
		else if (whence == 2)
			newOffset = (s64)(u64)s_slb2Fds[fd].file_size;
		else if (whence == 0)
			newOffset = offset;
		else
			return -1LL;

		if (newOffset >= 0 && (u64)newOffset <= (u64)s_slb2Fds[fd].file_size) {
			s_slb2Fds[fd].offset = offset;
			return newOffset;
		}
	}
	return -1LL;
}

/* 0x805ee6: partitionStorageRead (name ours) */
int partitionStorageRead(int fd, void *buf, u32 size)
{
	int (*readSectors)(void *dst, u32 size, u32 startSector);
	SceSlb2Fd *file;
	u32 bytesRead;
	u32 chunkSize;
	s32 rem;
	int res;

	if (partitionStorageIsOpen(fd) == 0)
		return -1;

	file = &s_slb2Fds[fd];
	if (file->dev_id == 1)
		readSectors = readBootStorageSectors1;
	else if (file->dev_id == 0)
		readSectors = readBootStorageSectors0;
	else
		return -1;

	bytesRead = 0;
	while ((s32)size > 0) {
		s64 curOffset = file->offset;

		if (((u32)curOffset & 0x1ffU) == 0 && (s32)size >= 0x200) {
			chunkSize = size & ~0x1ffU;
			res = readSectors((u8 *)buf + bytesRead, chunkSize,
					  file->start_sector + (u32)(curOffset / 512));
			if (res != 0)
				return -1;
		} else {
			res = readSectors((void *)SCE_SECTOR_BOUNCE_ADDR, 0x200,
					  file->start_sector + (u32)(curOffset / 512));
			if (res != 0)
				return -1;

			rem = (s32)(file->offset % 512);
			if (rem == 0 || (s32)size < 0x200 - rem)
				chunkSize = size;
			else
				chunkSize = (u32)(0x200 - rem);

			rem = (s32)(file->offset % 512);
			bigmacMemcpy((u8 *)buf + bytesRead,
				     (const void *)(SCE_SECTOR_BOUNCE_ADDR + rem),
				     chunkSize);
		}

		bytesRead += chunkSize;
		file->offset += (u64)chunkSize;
		size -= chunkSize;
	}

	return (int)bytesRead;
}

/* 0x806026: partitionStorageGetStat (name ours) */
int partitionStorageGetStat(int fd, void *outStatBuf)
{
	u32 *outStat = (u32 *)outStatBuf;

	if (partitionStorageIsOpen(fd) == 0 || outStat == NULL)
		return -1;

	outStat[0] = s_slb2Fds[fd].file_size;
	outStat[1] = s_slb2Fds[fd].sector_size;
	return 0;
}
