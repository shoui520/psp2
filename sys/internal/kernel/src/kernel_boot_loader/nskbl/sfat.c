/*
 * NSKBL: SceSfat, the boot loader's read-only FAT12/16/32 driver (sfat.h). A volume
 * starts at sector 0, at the os0 partition of Sony's MBR, or at a PC MBR's first
 * partition; its files are found by 8.3 or long name (Shift-JIS bytes kept whole) and
 * read a cluster run at a time, straight into the caller's buffer when it is aligned.
 * Sony built it at -O3, with signed characters (CMakeLists.txt).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include "sfat.h"

#define SFAT_FILES		16
#define SFAT_SECTOR_SIZE	0x200
#define SFAT_BUFFER_ALIGN	32

/* a cluster number: the FAT12/16 root directory, the chain's end, a read that failed */
#define SFAT_ROOT		-2
#define SFAT_END		-1
#define SFAT_CLUSTER_ERROR	-100

#define SFAT_FAT12		0
#define SFAT_FAT16		1
#define SFAT_FAT32		2

#define SFAT_ATTR_LONG_NAME	0x0f
#define SFAT_LONG_NAME_LAST	0x40		/* the first long name entry, the name's end */
#define SFAT_LONG_NAME_CHARS	13
#define SFAT_ENTRY_DELETED	0xe5
#define SFAT_ENTRY_KANJI_E5	0x05		/* a name whose first byte is 0xe5 */

#define SFAT_SCE_PARTITIONS	16
#define SFAT_SCE_PARTITION_OS0	3
#define SFAT_SCE_PARTITION_ACTIVE 0x01

/* a Shift-JIS lead byte, whose next byte is compared as it is */
#define SFAT_KANJI_LEAD(c)	((SceUInt8)((c) + 0x61) > 0x40 && (c) > 0x80)
#define SFAT_TOUPPER(c)		((c) - 'a' <= 'z' - 'a' ? (c) & ~0x20 : (c))

/* name unknown: a partition of Sony's MBR */
typedef struct __attribute__((packed)) SfatScePartition {
	SceUInt32 offset;		/* sectors */
	SceUInt32 size;
	SceUInt8 code;			/* 3: os0 */
	SceUInt8 type;
	SceUInt8 active;
	SceUInt32 flags;
	SceUInt16 reserved;
} SfatScePartition;

/* name unknown: a FAT boot sector, Sony's MBR, or a PC MBR */
typedef union SfatBootSector {
	SceUInt8 bytes[SFAT_SECTOR_SIZE];
	struct __attribute__((packed)) {
		SceUInt8 jump[3];
		char oemName[8];
		SceUInt16 bytesPerSector;	/* 0x0b */
		SceUInt8 sectorsPerCluster;	/* 0x0d */
		SceUInt16 reservedSectors;	/* 0x0e */
		SceUInt8 fatCount;		/* 0x10 */
		SceUInt16 rootEntries;		/* 0x11 */
		SceUInt16 totalSectors16;	/* 0x13 */
		SceUInt8 media;			/* 0x15 */
		SceUInt16 fatSectors16;		/* 0x16 */
		SceUInt16 sectorsPerTrack;	/* 0x18 */
		SceUInt16 heads;		/* 0x1a */
		SceUInt32 hiddenSectors;	/* 0x1c */
		SceUInt32 totalSectors32;	/* 0x20 */
		SceUInt32 fatSectors32;		/* 0x24: FAT32 */
		SceUInt16 extFlags;		/* 0x28 */
		SceUInt16 version;		/* 0x2a */
		SceUInt32 rootCluster;		/* 0x2c */
	} bpb;
	struct __attribute__((packed)) {
		char magic[32];			/* "Sony Computer Entertainment Inc." */
		SceUInt8 header[0x30];
		SfatScePartition partitions[SFAT_SCE_PARTITIONS];	/* 0x50 */
		SceUInt8 padding[0x1fe - 0x160];
		SceUInt16 signature;		/* 0x1fe */
	} sceMbr;
} SfatBootSector;

/* name unknown: a directory entry */
typedef struct SfatDirEntry {
	SceUInt8 name[11];		/* 8.3, blank padded; a long name entry's sequence first */
	SceUInt8 attr;			/* 0x0b */
	SceUInt8 reserved;
	SceUInt8 checksum;		/* 0x0d: a long name entry's, of its short name */
	SceUInt8 times[6];
	SceUInt16 clusterHigh;		/* 0x14: FAT32 */
	SceUInt16 modifyTime;
	SceUInt16 modifyDate;
	SceUInt16 clusterLow;		/* 0x1a */
	SceInt32 size;			/* 0x1c */
} SfatDirEntry;

/* name unknown: an open file */
typedef struct SfatFile {
	SfatVolume *volume;		/* NULL: free */
	int firstCluster;
	int dirCluster;			/* 0x08: the directory holding its entry */
	SceUInt32 entryOffset;		/* 0x0c: the entry's byte offset in that directory */
	SceUInt32 entrySector;		/* 0x10 */
	int cluster;			/* 0x14: the cluster at position, 0 unknown */
	SceUInt32 position;		/* 0x18 */
	SceUInt32 size;			/* 0x1c */
} SfatFile;

/* the UCS-2 characters of a long name entry, by byte offset */
static const SceUInt8 s_longNameOffsets[SFAT_LONG_NAME_CHARS] = {
	1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30,
};

static SfatFile s_files[SFAT_FILES];
static SceUInt8 s_fatSector[SFAT_SECTOR_SIZE] __attribute__((aligned(SFAT_BUFFER_ALIGN)));
SfatReadSectors g_sfatReadHook;
static SfatBootSector s_bootSector __attribute__((aligned(SFAT_BUFFER_ALIGN)));
static SfatDirEntry s_dirSector[SFAT_SECTOR_SIZE / sizeof(SfatDirEntry)]
	__attribute__((aligned(SFAT_BUFFER_ALIGN)));
static SceUID s_heap;
void *g_sfatReadHookArgument;

const char *SfatNextPathElement(char *element, SceSize size, const char *path);
void SfatSplitPath(const char *path, char *dir, SceSize dirSize, char *name, SceSize nameSize);
void SfatMakeShortName(const char *name, char *base, char *ext);

/* name unknown: count sectors of the volume from sector on */
static inline int SfatVolumeRead(const SfatVolume *volume, SceUInt32 sector, SceUInt32 count,
				 void *buffer)
{
	if (g_sfatReadHook != NULL)
		return g_sfatReadHook(g_sfatReadHookArgument, sector, count, buffer);
	if (volume->read == NULL)
		return SFAT_ERROR_NOT_FOUND;
	return volume->read(volume->readArgument, sector, count, buffer);
}

/* name unknown: the cluster after cluster in its chain; SFAT_END, or SFAT_CLUSTER_ERROR */
int SfatGetNextCluster(SfatVolume *volume, int cluster)
{
	SceUInt32 sectorSize, sector, low, high;
	int offset, next;

	if (cluster - 2 > volume->clusterCount)
		return SFAT_CLUSTER_ERROR;
	switch (volume->fatType) {
	case SFAT_FAT12:
		offset = cluster * 3 / 2;
		if (volume->fatCache != NULL) {
			low = ((SceUInt8 *)volume->fatCache)[offset];
			high = ((SceUInt8 *)volume->fatCache)[offset + 1];
		} else {
			sectorSize = volume->sectorSize;
			sector = volume->fatStart + offset / sectorSize;
			offset %= sectorSize;
			if (SfatVolumeRead(volume, sector, sectorSize >> 9, s_fatSector) < 0)
				return SFAT_CLUSTER_ERROR;
			low = s_fatSector[offset];
			if (offset == volume->sectorSize - 1) {
				/* the entry crosses into the next sector */
				if (SfatVolumeRead(volume, sector + 1, volume->sectorSize >> 9,
						   s_fatSector) < 0)
					return SFAT_CLUSTER_ERROR;
				high = s_fatSector[0];
			} else {
				high = s_fatSector[offset + 1];
			}
		}
		if (cluster & 1)
			next = high << 4 | low >> 4;
		else
			next = (low | high << 8) & 0xfff;
		if (next > 0xff7)
			return SFAT_END;
		return next;
	case SFAT_FAT16:
		if (volume->fatCache != NULL) {
			next = ((SceUInt16 *)volume->fatCache)[cluster];
		} else {
			sectorSize = volume->sectorSize;
			if (SfatVolumeRead(volume, volume->fatStart + cluster / (sectorSize >> 1),
					   sectorSize >> 9, s_fatSector) < 0)
				return SFAT_CLUSTER_ERROR;
			next = ((SceUInt16 *)s_fatSector)[cluster % (volume->sectorSize >> 1)];
		}
		if (next == 0xffff)
			return SFAT_END;
		return next;
	case SFAT_FAT32:
		if (volume->fatCache != NULL) {
			next = ((SceUInt32 *)volume->fatCache)[cluster];
		} else {
			sectorSize = volume->sectorSize;
			if (SfatVolumeRead(volume, volume->fatStart + cluster / (sectorSize >> 2),
					   sectorSize >> 9, s_fatSector) < 0)
				return SFAT_CLUSTER_ERROR;
			next = ((SceUInt32 *)s_fatSector)[cluster % (volume->sectorSize >> 2)];
		}
		if (next == 0x0fffffff)
			return SFAT_END;
		return next;
	}
	return SFAT_CLUSTER_ERROR;
}

/* name unknown: the checksum a long name keeps of its short name */
static inline SceUInt8 SfatShortNameChecksum(const SceUInt8 *name)
{
	SceUInt8 sum = 0;
	int i;

	for (i = 0; i < 11; i++)
		sum = (SceUInt8)((sum >> 1) + (sum << 7) + name[i]);
	return sum;
}

/* name unknown: whether the short name's part matches, letters in either case */
static inline int SfatMatchShortName(const SceUInt8 *entry, const char *part, int length)
{
	unsigned int c, n;
	int i;

	for (i = 0; i < length; i++) {
		c = entry[i];
		n = (SceUInt8)part[i];
		if (SFAT_KANJI_LEAD(c)) {
			if (c != n)
				return 0;
			i++;
			if (entry[i] != (SceUInt8)part[i])
				return 0;
		}
		if (SFAT_TOUPPER(c) != SFAT_TOUPPER(n))
			return 0;
	}
	return 1;
}

/*
 * name unknown: the entry named name in the directory at cluster (SFAT_ROOT: the root),
 * in s_dirSector; its byte offset in the directory and its sector in *offsetOut and
 * *sectorOut. NULL when it is not there
 */
SfatDirEntry *SfatFindEntry(SfatVolume *volume, int cluster, const char *name,
			    SceUInt32 *offsetOut, SceUInt32 *sectorOut)
{
	SceUInt16 longName[1024];
	char ext[4];
	char base[12];
	SfatDirEntry *entry;
	SceUInt32 offset, sector, end, entries, i;
	unsigned int c, n;
	int current, count, sequence, checksum, first, k;

	if (offsetOut != NULL)
		*offsetOut = -1;
	if (sectorOut != NULL)
		*sectorOut = -1;
	SfatMakeShortName(name, base, ext);
	sequence = -1;
	checksum = 0;
	offset = 0;
	if ((SceUInt8)base[0] == SFAT_ENTRY_DELETED)
		base[0] = SFAT_ENTRY_KANJI_E5;
	current = cluster;
	for (;;) {
		if (cluster == SFAT_ROOT) {
			if (volume->fatType == SFAT_FAT32) {
				cluster = volume->rootCluster;
				count = volume->sectorsPerCluster;
				sector = volume->rootStart;
				current = cluster;
			} else {
				sector = volume->rootStart;
				count = volume->dataStart - sector;
			}
		} else {
			count = volume->sectorsPerCluster;
			sector = volume->dataStart + count * (current - 2);
		}
		if (count > 0) {
			end = sector + count;
			do {
				if (SfatVolumeRead(volume, sector, volume->sectorSize >> 9, s_dirSector) < 0)
					return NULL;
				entries = volume->sectorSize >> 5;
				for (i = 0; i < entries; i++, offset += sizeof(*entry)) {
					entry = &s_dirSector[i];
					first = entry->name[0];
					if (first == 0)
						return NULL;
					if (first == SFAT_ENTRY_DELETED || first == '.')
						continue;
					if (entry->attr == SFAT_ATTR_LONG_NAME) {
						if (first & SFAT_LONG_NAME_LAST) {
							if (sequence != -1) {
								sequence = -1;
								continue;
							}
							first &= ~SFAT_LONG_NAME_LAST;
							checksum = entry->checksum;
							memset(longName, 0, sizeof(longName));
						} else if (first != sequence || entry->checksum != checksum) {
							sequence = -1;
							continue;
						}
						sequence = first - 1;
						for (k = 0; k < SFAT_LONG_NAME_CHARS; k++)
							longName[sequence * SFAT_LONG_NAME_CHARS + k] =
								((SceUInt8 *)entry)[s_longNameOffsets[k]] +
								(((SceUInt8 *)entry)[s_longNameOffsets[k] + 1] << 8);
						continue;
					}
					if (sequence == 0 && checksum == SfatShortNameChecksum(entry->name)) {
						for (k = 0; name[k] != '\0'; k++) {
							c = (SceUInt16)name[k];
							n = longName[k];
							if (SFAT_TOUPPER(c) != SFAT_TOUPPER(n))
								break;
						}
						if (name[k] == '\0' && longName[k] == 0)
							goto found;
					}
					if (SfatMatchShortName(entry->name, base, 8) &&
					    SfatMatchShortName(&entry->name[8], ext, 3))
						goto found;
					sequence = -1;
				}
			} while (++sector != end);
		}
		if (cluster == SFAT_ROOT)
			return NULL;
		current = SfatGetNextCluster(volume, current);
		if (current == SFAT_END)
			return NULL;
	}

found:
	if (offsetOut != NULL)
		*offsetOut = offset;
	if (sectorOut != NULL)
		*sectorOut = sector;
	return entry;
}

/*
 * name unknown: the entry of path, from the root; its directory's cluster in *dirCluster
 * (SFAT_CLUSTER_ERROR: a directory on the way is missing)
 */
SfatDirEntry *SfatLookup(SfatVolume *volume, const char *path, int *dirCluster,
			 SceUInt32 *offset, SceUInt32 *sector)
{
	char name[256];
	char dir[256];
	char element[16];
	SceUInt32 elementSector;
	SfatDirEntry *entry;
	const char *p;
	int cluster;

	SfatSplitPath(path, dir, sizeof(dir) - 1, name, sizeof(name) - 1);
	cluster = SFAT_ROOT;
	if (!((dir[0] == '\\' || dir[0] == '/') && dir[1] == '\0')) {
		p = dir;
		while ((p = SfatNextPathElement(element, sizeof(element), p)) != NULL) {
			entry = SfatFindEntry(volume, cluster, element, NULL, &elementSector);
			if (entry == NULL) {
				cluster = SFAT_CLUSTER_ERROR;
				break;
			}
			cluster = entry->clusterLow;
			if (volume->fatType == SFAT_FAT32)
				cluster |= entry->clusterHigh << 16;
		}
	}
	if (dirCluster != NULL)
		*dirCluster = cluster;
	if (cluster == SFAT_CLUSTER_ERROR)
		return NULL;
	return SfatFindEntry(volume, cluster, name, offset, sector);
}

int SfatInit(void)
{
	SceKernelHeapCreateOpt opt;
	int i;

	for (i = 0; i < SFAT_FILES; i++) {
		s_files[i].volume = NULL;
		s_files[i].firstCluster = -1;
		s_files[i].dirCluster = -1;
		s_files[i].entryOffset = 0;
		s_files[i].entrySector = -1;
		s_files[i].cluster = -1;
		s_files[i].position = 0;
		s_files[i].size = 0;
	}
	opt.size = sizeof(opt);
	opt.attr = HEAP_AUTO_EXTEND;
	s_heap = sceKernelCreateHeap("SceSfat", 0x10000, &opt);
	if (s_heap < 0)
		return SCE_ERROR_ERRNO_ENOMEM;
	return 0;
}

int SfatTerm(void)
{
	sceKernelDeleteHeap(s_heap);
	return 0;
}

int SfatMount(SfatVolume *volume, SceUInt32 flags, SfatReadSectors read, void *argument)
{
	const SfatScePartition *partition;
	SceUInt32 start, rootEntries, bytesPerSector, rootSectors, fatSectors, fatSectors16;
	SceUInt32 totalSectors, reserved, clusters, fatBytes, dataStart;
	void *block;
	int res, i;

	if (volume == NULL)
		return SFAT_ERROR_INVALID_ARGUMENT;
	volume->flags = flags;
	volume->read = read;
	volume->readArgument = argument;
	volume->sectorSize = SFAT_SECTOR_SIZE;
	volume->dataStart = -1;
	if (flags & SFAT_MOUNT_NO_MBR) {
		start = 0;
		volume->partitionStart = 0;
	} else if (flags & SFAT_MOUNT_SCE_MBR) {
		res = SfatVolumeRead(volume, 0, 1, &s_bootSector);
		if (res < 0)
			return res;
		if (strncmp((const char *)s_bootSector.sceMbr.magic, "Sony Computer Entertainment Inc.",
			    sizeof(s_bootSector.sceMbr.magic)) != 0 ||
		    s_bootSector.sceMbr.signature != 0xaa55)
			return SFAT_ERROR_NOT_FOUND;
		for (i = 0; i < SFAT_SCE_PARTITIONS; i++) {
			partition = &s_bootSector.sceMbr.partitions[i];
			if (partition->code == SFAT_SCE_PARTITION_OS0 &&
			    (partition->active & SFAT_SCE_PARTITION_ACTIVE))
				break;
		}
		if (i == SFAT_SCE_PARTITIONS)
			return SFAT_ERROR_NOT_FOUND;
		start = partition->offset;
		volume->partitionStart = start;
	} else {
		res = SfatVolumeRead(volume, 0, 1, &s_bootSector);
		if (res < 0)
			return res;
		if (s_bootSector.bytes[0x1fe] == 0x55 && s_bootSector.bytes[0x1ff] == 0xaa &&
		    (s_bootSector.bytes[0] == 0xeb ? s_bootSector.bytes[2] != 0x90 :
						    s_bootSector.bytes[0] != 0xe9)) {
			/* a PC MBR: its first partition */
			start = s_bootSector.bytes[0x1c6] | s_bootSector.bytes[0x1c7] << 8 |
				s_bootSector.bytes[0x1c8] << 16 | s_bootSector.bytes[0x1c9] << 24;
			volume->partitionStart = start;
		} else {
			start = 0;
			volume->partitionStart = 0;
		}
	}

	res = SfatVolumeRead(volume, start, volume->sectorSize >> 9, &s_bootSector);
	if (res < 0)
		return res;
	rootEntries = s_bootSector.bpb.rootEntries;
	bytesPerSector = s_bootSector.bpb.bytesPerSector;
	rootSectors = (int)(rootEntries * 32 + bytesPerSector - 1) / (int)bytesPerSector;
	fatSectors16 = s_bootSector.bpb.fatSectors16;
	fatSectors = fatSectors16 != 0 ? fatSectors16 : s_bootSector.bpb.fatSectors32;
	totalSectors = s_bootSector.bpb.totalSectors16 != 0 ? s_bootSector.bpb.totalSectors16 :
							       s_bootSector.bpb.totalSectors32;
	reserved = s_bootSector.bpb.reservedSectors;
	clusters = (totalSectors - (reserved + s_bootSector.bpb.fatCount * fatSectors + rootSectors)) /
		   s_bootSector.bpb.sectorsPerCluster;
	if (clusters <= 0xff4)
		volume->fatType = SFAT_FAT12;
	else if (clusters <= 0xfff4)
		volume->fatType = SFAT_FAT16;
	else
		volume->fatType = SFAT_FAT32;
	volume->reservedSectors = reserved;
	volume->rootEntries = rootEntries;
	volume->sectorSize = bytesPerSector;
	volume->fatSectors = fatSectors16;
	volume->sectorsPerCluster = s_bootSector.bpb.sectorsPerCluster;
	volume->clusterSize = bytesPerSector * s_bootSector.bpb.sectorsPerCluster;
	volume->fatCount = s_bootSector.bpb.fatCount;
	volume->fatStart = reserved + volume->partitionStart;
	volume->totalSectors = s_bootSector.bpb.totalSectors16 != 0 ? s_bootSector.bpb.totalSectors16 :
								     s_bootSector.bpb.totalSectors32;
	if (volume->fatType == SFAT_FAT32) {
		if (volume->fatSectors == 0)
			volume->fatSectors = s_bootSector.bpb.fatSectors32;
		dataStart = volume->fatStart + volume->fatCount * volume->fatSectors;
		volume->dataStart = dataStart;
		volume->rootCluster = s_bootSector.bpb.rootCluster;
		volume->rootStart = dataStart + volume->sectorsPerCluster * (volume->rootCluster - 2);
	} else {
		volume->rootStart = volume->fatStart + volume->fatCount * volume->fatSectors;
		volume->dataStart = volume->rootStart + volume->rootEntries * 32 / volume->sectorSize;
	}
	volume->clusterCount = (volume->totalSectors + volume->partitionStart - volume->dataStart) /
			       volume->sectorsPerCluster;
	volume->freeHint = -1;
	if (bytesPerSector - 1 >= SFAT_SECTOR_SIZE)
		return SFAT_ERROR_SECTOR_SIZE;

	block = sceKernelAllocHeapMemory(s_heap, volume->clusterSize + SFAT_BUFFER_ALIGN - 1);
	volume->clusterBlock = block;
	if (block == NULL) {
		res = SFAT_ERROR_NO_MEMORY;
		goto error;
	}
	volume->fatCacheBlock = NULL;
	volume->fatCache = NULL;
	volume->cluster = (void *)(((uintptr_t)block + SFAT_BUFFER_ALIGN - 1) &
				   ~(SFAT_BUFFER_ALIGN - 1));
	if (!(flags & SFAT_MOUNT_CACHE_FAT))
		return 0;

	fatBytes = volume->fatSectors * volume->sectorSize;
	block = sceKernelAllocHeapMemory(s_heap, fatBytes + SFAT_BUFFER_ALIGN - 1);
	volume->fatCacheBlock = block;
	if (block == NULL) {
		res = SFAT_ERROR_NO_MEMORY;
		goto error;
	}
	volume->fatCache = (void *)(((uintptr_t)block + SFAT_BUFFER_ALIGN - 1) &
				    ~(SFAT_BUFFER_ALIGN - 1));
	res = SfatVolumeRead(volume, volume->fatStart, fatBytes >> 9, volume->fatCache);
	if (res == 0)
		return 0;

error:
	if (volume->fatCacheBlock != NULL) {
		sceKernelFreeHeapMemory(s_heap, volume->fatCacheBlock);
		volume->fatCacheBlock = NULL;
		volume->fatCache = NULL;
	}
	if (volume->clusterBlock != NULL) {
		sceKernelFreeHeapMemory(s_heap, volume->clusterBlock);
		volume->clusterBlock = NULL;
		volume->cluster = NULL;
	}
	return res;
}

int SfatUnmount(SfatVolume *volume, int flags)
{
	int i;

	(void)flags;
	for (i = 0; i < SFAT_FILES; i++) {
		if (s_files[i].volume == volume)
			return SFAT_ERROR_BUSY;
	}
	if (volume->fatCacheBlock != NULL) {
		sceKernelFreeHeapMemory(s_heap, volume->fatCacheBlock);
		volume->fatCacheBlock = NULL;
		volume->fatCache = NULL;
	}
	if (volume->clusterBlock != NULL) {
		sceKernelFreeHeapMemory(s_heap, volume->clusterBlock);
		volume->clusterBlock = NULL;
		volume->cluster = NULL;
	}
	return 0;
}

int SfatOpen(SfatVolume *volume, const char *path, int flags)
{
	SfatDirEntry *entry;
	SfatFile *file;
	int dirCluster;
	SceUInt32 sector, offset;
	int fd, i, cluster;

	(void)flags;
	for (fd = 0; fd < SFAT_FILES; fd++) {
		if (s_files[fd].volume == NULL)
			break;
	}
	if (fd == SFAT_FILES)
		return SFAT_ERROR_NO_MEMORY;
	entry = SfatLookup(volume, path, &dirCluster, &offset, &sector);
	if (entry == NULL)
		return SFAT_ERROR_NOT_FOUND;
	if (entry->size < 0)
		return SFAT_ERROR_FILE_SIZE;
	for (i = 0; i < SFAT_FILES; i++) {
		if (s_files[i].volume == volume && s_files[i].dirCluster == dirCluster &&
		    s_files[i].entryOffset == offset)
			return SFAT_ERROR_BUSY;
	}
	cluster = entry->clusterLow;
	if (volume->fatType == SFAT_FAT32)
		cluster |= entry->clusterHigh << 16;
	file = &s_files[fd];
	file->size = entry->size;
	file->volume = volume;
	file->entryOffset = offset;
	file->firstCluster = cluster;
	file->dirCluster = dirCluster;
	file->cluster = cluster;
	file->position = 0;
	file->entrySector = sector;
	return fd;
}

int SfatClose(int fd)
{
	SfatFile *file = &s_files[fd];

	if (file->volume == NULL)
		return SFAT_ERROR_BAD_FD;
	file->volume = NULL;
	file->firstCluster = -1;
	file->dirCluster = -1;
	return 0;
}

int SfatRead(int fd, void *buffer, SceSize size)
{
	SfatFile *file = &s_files[fd];
	SfatVolume *volume = file->volume;
	SceUInt32 position, done, base, sectorSize, clusterSize, offset, inCluster, chunk;
	SceUInt32 sector, bytes;
	char *target;
	int cluster, next, res;

	if (volume == NULL)
		return SFAT_ERROR_BAD_FD;
	position = file->position;
	if (position + size > file->size) {
		if (position > file->size)
			return SFAT_ERROR_POSITION;
		size = file->size - position;
	}

	/* the cluster at position, from the file's first when not known */
	cluster = file->cluster;
	if (cluster == 0 && file->firstCluster != 0) {
		cluster = file->firstCluster;
		if (cluster != SFAT_ROOT && position > volume->clusterSize - 1) {
			if (cluster == SFAT_END) {
				cluster = 0;
			} else {
				base = 0;
				for (;;) {
					cluster = SfatGetNextCluster(volume, cluster);
					if (cluster == 0) {
						cluster = SFAT_CLUSTER_ERROR;
						break;
					}
					base += volume->clusterSize;
					if (position <= volume->clusterSize - 1 + base)
						break;
					if (cluster == SFAT_END) {
						cluster = 0;
						break;
					}
				}
			}
		}
	}

	res = 0;
	done = 0;
	while (done < size) {
		volume = file->volume;
		sectorSize = volume->sectorSize;
		clusterSize = volume->clusterSize;
		offset = position % sectorSize;
		inCluster = position % clusterSize;
		chunk = inCluster != 0 ? clusterSize - inCluster : clusterSize;
		if (position + chunk > file->size)
			chunk = file->size - position;
		if (done + chunk > size)
			chunk = size - done;
		if (cluster == SFAT_ROOT) {
			sector = position / sectorSize + volume->rootStart;
		} else {
			if (cluster == 0)
				return SFAT_ERROR_IO;
			sector = inCluster / sectorSize + volume->dataStart +
				 volume->sectorsPerCluster * (cluster - 2);
		}
		if (offset != 0 || ((uintptr_t)buffer & (SFAT_BUFFER_ALIGN - 1)) != 0 ||
		    chunk % sectorSize != 0) {
			/* through the volume's cluster buffer */
			next = cluster;
			target = volume->cluster;
		} else {
			next = cluster;
			if (volume->cluster != buffer && cluster != SFAT_ROOT) {
				next = SfatGetNextCluster(volume, cluster);
				if (next == SFAT_CLUSTER_ERROR)
					return SFAT_ERROR_IO;
				volume = file->volume;
			}
			target = buffer;
			/* the clusters that follow on the disk, in one read */
			while (next == cluster + 1) {
				if (file->size < position + clusterSize + chunk ||
				    size < done + clusterSize + chunk)
					break;
				chunk += clusterSize;
				cluster = next;
				if (cluster != SFAT_ROOT) {
					next = SfatGetNextCluster(volume, cluster);
					if (next == SFAT_CLUSTER_ERROR)
						return SFAT_ERROR_IO;
					volume = file->volume;
				}
			}
		}
		bytes = volume->sectorSize * ((sectorSize - 1 + offset + chunk) / sectorSize);
		res = SfatVolumeRead(volume, sector, bytes >> 9, target);
		if (res >= 0) {
			if (file->volume->cluster == target)
				memcpy(buffer, target + offset, chunk);
			buffer = (char *)buffer + chunk;
		}
		done += chunk;
		position += chunk;
		if (cluster != SFAT_ROOT && position % file->volume->clusterSize == 0) {
			cluster = SfatGetNextCluster(file->volume, cluster);
			if (cluster == SFAT_CLUSTER_ERROR)
				return SFAT_ERROR_IO;
		}
	}
	if (cluster == SFAT_END)
		cluster = 0;
	file->cluster = cluster;
	file->position += done;
	if (res != 0)
		return res;
	return done;
}

int SfatLseek(int fd, int offset, int whence)
{
	SfatFile *file = &s_files[fd];
	int position;

	if (file->volume == NULL)
		return SFAT_ERROR_BAD_FD;
	switch (whence) {
	case SCE_SEEK_SET:
		position = offset;
		file->position = position;
		break;
	case SCE_SEEK_CUR:
		position = file->position + offset;
		file->position = position;
		break;
	case SCE_SEEK_END:
		position = file->size + offset;
		file->position = position;
		break;
	default:
		return SFAT_ERROR_INVALID_WHENCE;
	}
	file->cluster = 0;
	if (position < 0) {
		file->position = 0;
		file->cluster = file->firstCluster;
		return SFAT_ERROR_SEEK_RANGE;
	}
	if (file->size < (SceUInt32)position) {
		file->position = file->size;
		return SFAT_ERROR_SEEK_RANGE;
	}
	return position;
}

/*
 * name unknown: the path's first element into element (size bytes), a leading separator
 * skipped; the rest of the path after it, or NULL at the end
 */
__attribute__((noinline))
const char *SfatNextPathElement(char *element, SceSize size, const char *path)
{
	unsigned int c;
	int start, i;

	if (path == NULL) {
		if (element != NULL)
			element[0] = '\0';
		return NULL;
	}
	start = path[0] == '\\' || path[0] == '/';
	if (path[start] == '\0')
		return NULL;
	for (i = start; (c = (SceUInt8)path[i]) != '\0'; i++) {
		if (SFAT_KANJI_LEAD(c))
			i++;
		else if (c == '\\' || c == '/')
			break;
	}
	if (element != NULL) {
		strncpy(element, &path[start], size);
		element[i - start] = '\0';
	}
	if (path[i] != '\0')
		return &path[i + 1];
	return NULL;
}

/* name unknown: path's directory, its last separator kept, and its file name */
__attribute__((noinline))
void SfatSplitPath(const char *path, char *dir, SceSize dirSize, char *name, SceSize nameSize)
{
	unsigned int c;
	int last, i;

	if (dir != NULL)
		dir[0] = '\0';
	if (name != NULL)
		name[0] = '\0';
	if (path == NULL)
		return;
	last = 0;
	for (i = 0; (c = (SceUInt8)path[i]) != '\0'; i++) {
		if (SFAT_KANJI_LEAD(c))
			i++;
		else if (c == '\\' || c == '/')
			last = i;
	}
	if (dir != NULL) {
		strncpy(dir, path, dirSize);
		dir[last + 1] = '\0';
	}
	if (name != NULL)
		strncpy(name, &path[last + 1], nameSize);
}

/* name unknown: name as a short name's base and extension, blank padded */
__attribute__((noinline))
void SfatMakeShortName(const char *name, char *base, char *ext)
{
	const char *p;
	int i, k;

	i = 0;
	for (k = 0; k < 8; k++) {
		if (name[i] == '\0' || name[i] == '.')
			base[k] = ' ';
		else
			base[k] = name[i++];
	}
	base[8] = '\0';
	for (p = &name[i]; *p != '\0'; p++) {
		if (*p == '.')
			i = p + 1 - name;
	}
	for (k = 0; k < 3; k++) {
		if (name[i] == '\0')
			ext[k] = ' ';
		else
			ext[k] = name[i++];
	}
	ext[3] = '\0';
}
