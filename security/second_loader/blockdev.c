/*
 * blockdev.c — Raw Block Device File Descriptor Layer
 *
 * Address range: 0x806b78..0x8070e9
 *
 * Provides a POSIX-like open/close/lseek/read file descriptor abstraction
 * over the raw eMMC block device (`/ABCDEFGHI`), handling unaligned head/tail
 * sector reads through a 512-byte bounce buffer.
 */

#include "second_loader.h"

typedef struct SceBlockdevFd { /* name ours, raw block device file descriptor */
	u32	is_open;
	u32	open_flags;
	u64	offset;
	u32	sector_size;
	u32	total_sectors;
	int	(*read_sectors)(void *dst, u32 size, u32 startSector);
	u32	single_sector_mode;
} SceBlockdevFd;

/* .rodata: 0x80c358..0x80c363 */
static const char s_blockdevPath[12] = "/ABCDEFGHI";

/* .sdata: 0x80cd0c */
static u32 s_blockdevOpenMask = 0;

/* .bss: 0x80d27c..0x80d47b */
static u8 s_blockdevBounceBuf[0x200];

/* COMMON: 0x80da70..0x80db0f */
SceBlockdevFd g_blockdevFds[5];

/* 0x806b78: blockdevFindFreeFd (name ours) */
int blockdevFindFreeFd(void)
{
	if ((s_blockdevOpenMask & 0x02U) == 0)
		return 1;
	if ((s_blockdevOpenMask & 0x04U) == 0)
		return 2;
	if ((s_blockdevOpenMask & 0x08U) == 0)
		return 3;
	if ((s_blockdevOpenMask & 0x10U) == 0)
		return 4;
	return -1;
}

/* 0x806bac: blockdevAllocFd (name ours) */
int blockdevAllocFd(void)
{
	int fd = blockdevFindFreeFd();

	if (fd != -1)
		s_blockdevOpenMask |= (1U << fd);
	return fd;
}

/* 0x806bce: blockdevOpen (name ours) */
int blockdevOpen(const char *path, u32 flags)
{
	u32 pathLen;
	u32 devLen;
	u32 totalSectors;
	int fd;

	if (path != NULL) {
		pathLen = strnlen(path, 0x100);
		if (pathLen != 0) {
			devLen = strnlen(s_blockdevPath, 11);
			if (strncmp(path, s_blockdevPath, devLen) == 0 && pathLen == devLen) {
				totalSectors = sdifGetTotalSectors();
				fd = blockdevAllocFd();
				if (fd != -1) {
					g_blockdevFds[fd].sector_size = 0x200;
					g_blockdevFds[fd].is_open = 1;
					g_blockdevFds[fd].offset = 0;
					g_blockdevFds[fd].open_flags = flags;
					g_blockdevFds[fd].total_sectors = totalSectors;
					g_blockdevFds[fd].read_sectors = sdifReadSectorsAligned;
					g_blockdevFds[fd].single_sector_mode = 1;
					return fd;
				}
			}
		}
	}
	return -1;
}

/* 0x806c62: blockdevFreeFd (name ours) */
int blockdevFreeFd(int fd)
{
	if (fd < 1)
		return -1;

	g_blockdevFds[fd].total_sectors = 0;
	g_blockdevFds[fd].is_open = 0;
	g_blockdevFds[fd].open_flags = 0;
	g_blockdevFds[fd].sector_size = 0;
	g_blockdevFds[fd].read_sectors = NULL;
	g_blockdevFds[fd].offset = 0;
	g_blockdevFds[fd].single_sector_mode = 0;
	s_blockdevOpenMask &= ~(1U << fd);
	return 0;
}

/* 0x806cb2: blockdevIsOpen (name ours) */
u32 blockdevIsOpen(int fd)
{
	if (fd < 1)
		return 0;
	return s_blockdevOpenMask & (1U << fd);
}

/* 0x806cc6: blockdevClose (name ours) */
int blockdevClose(int fd)
{
	if (blockdevIsOpen(fd) != 0)
		return blockdevFreeFd(fd);
	return -1;
}

/* 0x806cee: blockdevLseek (name ours) */
s64 blockdevLseek(int fd, s64 offset, int whence)
{
	s64 targetOffset;
	s64 devCapacity;

	if (blockdevIsOpen(fd) == 0)
		return -1LL;

	if (whence == 1) {
		targetOffset = offset + (s64)g_blockdevFds[fd].offset;
	} else if (whence == 2) {
		targetOffset = (s64)((u64)g_blockdevFds[fd].sector_size * (u64)g_blockdevFds[fd].total_sectors);
	} else if (whence == 0) {
		targetOffset = offset;
	} else {
		return -1LL;
	}

	devCapacity = (s64)((u64)g_blockdevFds[fd].sector_size * (u64)g_blockdevFds[fd].total_sectors);
	if ((u64)targetOffset > (u64)devCapacity || targetOffset < 0)
		return -1LL;

	g_blockdevFds[fd].offset = (u64)targetOffset;
	return targetOffset;
}

/* 0x806de0: blockdevReadPartialSector (name ours) */
int blockdevReadPartialSector(void *dst, u64 offset, u32 size, u32 sectorSize, const SceBlockdevFd *dev)
{
	u32 byteInSector;
	u32 sectorIdx;

	if (dst != NULL && size != 0 && size <= sectorSize &&
	    dev != NULL && dev->read_sectors != NULL) {
		byteInSector = (u32)(offset % (u64)sectorSize);
		if (byteInSector + size <= sectorSize &&
		    byteInSector < sectorSize &&
		    byteInSector + size >= size) {
			sectorIdx = (u32)(offset / (u64)sectorSize);
			if (dev->read_sectors(s_blockdevBounceBuf, sectorSize, sectorIdx) == 0) {
				memcpy(dst, s_blockdevBounceBuf + byteInSector, size);
				return 0;
			}
		}
	}
	return -1;
}

/* 0x806e9a: blockdevRead (name ours) */
int blockdevRead(int fd, void *buf, u32 size)
{
	SceBlockdevFd *dev;
	u64 curOffset;
	u32 sectorSize;
	u64 endSector;
	u32 byteInSector;
	u32 bytesRead;
	u32 alignedBytes;
	u32 startSector;
	u32 tailBytes;

	if (blockdevIsOpen(fd) == 0 || buf == NULL || size == 0)
		return -1;

	dev = &g_blockdevFds[fd];
	curOffset = dev->offset;
	sectorSize = dev->sector_size;
	endSector = (curOffset + (u64)size) / (u64)sectorSize;
	if (endSector > (u64)dev->total_sectors)
		return -1;

	byteInSector = (u32)(curOffset % (u64)sectorSize);
	if (byteInSector == 0) {
		bytesRead = 0;
	} else {
		bytesRead = sectorSize - byteInSector;
		if (size < bytesRead)
			bytesRead = size;
		if (blockdevReadPartialSector(buf, curOffset, bytesRead, sectorSize, dev) != 0)
			return -1;
		dev->offset += (s64)(s32)bytesRead;
		if (bytesRead == size)
			return (int)bytesRead;
		buf = (u8 *)buf + bytesRead;
		curOffset += (s64)(s32)bytesRead;
	}

	startSector = (u32)(curOffset / (u64)sectorSize);
	alignedBytes = (size - bytesRead) - ((size - bytesRead) % sectorSize);
	if (dev->single_sector_mode == 0) {
		if (dev->read_sectors(buf, alignedBytes, startSector) != 0)
			return -1;
		curOffset += (s64)(s32)alignedBytes;
		buf = (u8 *)buf + alignedBytes;
		bytesRead += alignedBytes;
		dev->offset += (s64)(s32)alignedBytes;
	} else {
		s32 remAligned = (s32)alignedBytes;

		while (remAligned > 0) {
			if (dev->read_sectors(buf, sectorSize, startSector) != 0)
				return -1;
			startSector++;
			buf = (u8 *)buf + sectorSize;
			bytesRead += sectorSize;
			dev->offset += (u64)sectorSize;
			curOffset += (u64)sectorSize;
			remAligned -= (s32)sectorSize;
		}
	}

	if (bytesRead == size)
		return (int)bytesRead;

	tailBytes = size - bytesRead;
	if ((s32)tailBytes <= 0)
		return (int)bytesRead;

	if (blockdevReadPartialSector(buf, curOffset, tailBytes, sectorSize, dev) == 0) {
		dev->offset += (s64)(s32)tailBytes;
		return (int)(bytesRead + tailBytes);
	}

	return -1;
}
