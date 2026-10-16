#include <sce/debug.h>
#include "exfatfs_internal.h"

extern int UVFAT_InitDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 cluster, SceExfatfsFileSystem *fileSystem,
	unsigned int flags, SceUInt32 size);
extern int UVFAT_ReadDirEntry(SceExfatfsSharedResource *sharedResource,
	int fd, void *entry, int flags, SceExfatfsFileSystem *fileSystem);
extern int _exfatfs_release_temporal_file(SceExfatfsFileSystem *fileSystem);
extern SceUInt32 UVFAT_NextCluster(SceExfatfsSharedResource *sharedResource,
	SceUInt32 cluster, SceExfatfsFileSystem *fileSystem);
extern SceUInt64 clusterToDeviceSector(SceUInt32 cluster,
	SceExfatfsFileSystem *fileSystem);
extern int UVFAT_ReadDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	SceSize size, void *buffer);
extern void *allocateFatBuffer(unsigned int size);
extern void freeFatBuffer(void *buffer, unsigned int size);

int UVFAT_readupcasetable(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsUpcaseTableEntry entry;
	SceUInt32 cluster, expectedChecksum, checksum = 0;
	SceUInt64 remaining, allocationSize;
	SceUInt16 tableIndex = 0;
	SceUInt16 *destination;
	SceSize sectorSize;
	SceExfatfsUpcaseBlock *block;
	int result;

	result = UVFAT_InitDirEntry_ex(sharedResource, 0, fileSystem->firstClusterOfRoot,
		fileSystem, 0, 0);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 49, "%s: UVFAT_InitDirEntry_ex failed(%d)\n",
			"UVFAT_readupcasetable", result);
		return result;
	}
	for (;;) {
		result = UVFAT_ReadDirEntry(sharedResource, 0, &entry, 0, fileSystem);
		if (result) {
			_exfatfs_release_temporal_file(fileSystem);
			return result;
		}
		if (entry.entryType == 0x82)
			break;
		if (!entry.entryType) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 69, "NO up-case table in root.\n");
			_exfatfs_release_temporal_file(fileSystem);
			return -15;
		}
	}
	_exfatfs_release_temporal_file(fileSystem);
	cluster = entry.firstCluster[0] | (entry.firstCluster[2] << 16) |
		(entry.firstCluster[3] << 24) | (entry.firstCluster[1] << 8);
	remaining = ((SceUInt64)(entry.dataLength[4] | (entry.dataLength[6] << 16) |
		(entry.dataLength[7] << 24) | (entry.dataLength[5] << 8)) << 32) |
		entry.dataLength[0] | (entry.dataLength[2] << 16) |
		(entry.dataLength[3] << 24) | (entry.dataLength[1] << 8);
	expectedChecksum = entry.tableChecksum[0] | (entry.tableChecksum[2] << 16) |
		(entry.tableChecksum[3] << 24) | (entry.tableChecksum[1] << 8);
	sectorSize = fileSystem->sectorSize;
	allocationSize = ((remaining + (sectorSize - 1)) / sectorSize) * sectorSize;
	fileSystem->upcase.table = allocateFatBuffer(allocationSize);
	if (!fileSystem->upcase.table) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 304, "UVFAT_initupcasetable",
			"%s: No memory\n", "UVFAT_initupcasetable");
		goto invalid;
	}
	fileSystem->upcase.allocationSize = allocationSize;
	fileSystem->upcase.runHead = NULL;
	fileSystem->upcase.runTailLink = &fileSystem->upcase.runHead;
	block = allocateFatBuffer(2048);
	fileSystem->upcase.blockList = block;
	if (!block) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 218, "_initUtableListBp",
			"%s: No memory\n", "_initUtableListBp");
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 314, "UVFAT_initupcasetable",
			"%s: upcasetable init error\n", "UVFAT_initupcasetable");
		goto invalid;
	}
	block->next = NULL;
	block->allocationSize = 2048;
	block->nextEntry = block->entries;
	destination = fileSystem->upcase.table;
	while (remaining) {
		SceUInt32 nextCluster, sectorIndex;
		SceUInt64 sector;

		if (cluster <= 1 || cluster > fileSystem->ClusterMAX) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 142, "UVFAT_copyupcasetable",
				"%s: Ellegal cluster no found\n", "UVFAT_copyupcasetable");
			goto invalid;
		}
		nextCluster = UVFAT_NextCluster(NULL, cluster, fileSystem);
		sector = clusterToDeviceSector(cluster, fileSystem);
		sectorSize = fileSystem->sectorSize;
		for (sectorIndex = 0; sectorIndex < fileSystem->clusterSectorCount; sectorIndex++) {
			SceUInt32 byteCount, byteIndex;

			result = UVFAT_ReadDevice(fileSystem, sector, sectorSize, destination);
			if (result)
				goto cleanup;
			byteCount = sectorSize;
			if ((SceUInt64)sectorSize >= remaining)
				byteCount = remaining;
			for (byteIndex = 0; byteIndex < byteCount; byteIndex += 2) {
				unsigned char *bytes = (unsigned char *)destination + byteIndex;
				SceUInt16 code = bytes[0] | (bytes[1] << 8);

				if (tableIndex <= 127) {
					SceUInt16 expected = tableIndex;

					if ((SceUInt16)(tableIndex - 'a') <= 'z' - 'a')
						expected = tableIndex - 32;
					if (code != expected) {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 172, "UVFAT_copyupcasetable",
							"%s: upcasetable first 128byte NG\n", "UVFAT_copyupcasetable");
						goto invalid;
					}
				}
				if (tableIndex && fileSystem->upcase.table[tableIndex - 1] == 0xffff) {
					SceExfatfsUpcaseRun *run;
					SceExfatfsUpcaseBlock *nextBlock;

					block = fileSystem->upcase.blockList;
					if (!block) {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 238, "_getUtNextEmptyList",
							"%s: upcasetable list not found\n", "_getUtNextEmptyList");
						block = allocateFatBuffer(2048);
						fileSystem->upcase.blockList = block;
						if (!block) {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 218, "_initUtableListBp",
								"%s: No memory\n", "_initUtableListBp");
							SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 241, "_getUtNextEmptyList",
								"%s: upcasetable init error\n", "_getUtNextEmptyList");
							goto invalid;
						}
						block->next = NULL;
						block->allocationSize = 2048;
						block->nextEntry = block->entries;
					}
					while ((nextBlock = block->next))
						block = nextBlock;
					run = block->nextEntry;
					if ((uintptr_t)run + sizeof(*run) > (uintptr_t)(block + 1)) {
						block->next = allocateFatBuffer(2048);
						if (!block->next) {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 218, "_initUtableListBp",
								"%s: No memory\n", "_initUtableListBp");
							SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 258, "_getUtNextEmptyList",
								"%s: upcasetable init error\n", "_getUtNextEmptyList");
							goto invalid;
						}
						block = block->next;
						block->next = nextBlock;
						block->allocationSize = 2048;
						block->nextEntry = block->entries;
						run = block->nextEntry;
					}
					block->nextEntry = run + 1;
					if (!run)
						goto invalid;
					run->tableIndex = tableIndex - 1;
					run->next = NULL;
					run->skipCount = code;
					*fileSystem->upcase.runTailLink = run;
					fileSystem->upcase.runTailLink = &run->next;
				}
				tableIndex++;
				checksum = ((checksum >> 1) | (checksum << 31)) + bytes[0];
				checksum = ((checksum >> 1) | (checksum << 31)) + bytes[1];
			}
			if ((SceUInt64)sectorSize >= remaining)
				goto complete;
			sector++;
			remaining -= sectorSize;
			destination += sectorSize / 2;
		}
		cluster = nextCluster;
	}
complete:
	if (expectedChecksum == checksum)
		return 0;
invalid:
	result = -15;
cleanup:
	if (fileSystem->upcase.table) {
		sectorSize = fileSystem->sectorSize;
		allocationSize = fileSystem->upcase.allocationSize + sectorSize - 1;
		freeFatBuffer(fileSystem->upcase.table, (allocationSize / sectorSize) * sectorSize);
		fileSystem->upcase.table = NULL;
	}
	block = fileSystem->upcase.blockList;
	if (block) {
		SceExfatfsUpcaseBlock *next = block->next;

		if (next) {
			for (;;) {
				freeFatBuffer(block, 2048);
				block = next;
				if (!next->next)
					break;
				next = next->next;
			}
		} else {
			next = block;
		}
		freeFatBuffer(next, 2048);
	}
	fileSystem->upcase.blockList = NULL;
	return result;
}

void freeUpcaseStorage(SceExfatfsFileSystem *fileSystem)
{
	SceExfatfsUpcaseBlock *block, *next;

	if (fileSystem->upcase.table) {
		SceSize sectorSize = fileSystem->sectorSize;
		SceUInt64 size = fileSystem->upcase.allocationSize + (sectorSize - 1);

		freeFatBuffer(fileSystem->upcase.table, (size / sectorSize) * sectorSize);
		fileSystem->upcase.table = NULL;
	}
	block = fileSystem->upcase.blockList;
	if (block) {
		next = block->next;
		if (next) {
			for (;;) {
				freeFatBuffer(block, 2048);
				block = next;
				if (!next->next)
					break;
				next = next->next;
			}
		} else {
			next = block;
		}
		freeFatBuffer(next, 2048);
	}
	fileSystem->upcase.blockList = NULL;
}

unsigned int lookupUpcaseCharacter(unsigned int unicode, SceExfatfsUpcaseTable *table)
{
	SceExfatfsUpcaseRun *run = table->runHead;
	unsigned int index = unicode;
	unsigned int first;

	if (!run)
		goto lookup;
	first = run->tableIndex;
	if (unicode < first)
		goto lookup;
	for (;;) {
		unsigned int count = run->skipCount;

		if (first + count > index)
			return unicode;
		run = run->next;
		index += 2 - count;
		if (run) {
			first = run->tableIndex;
			if (first <= index)
				continue;
		}
		break;
	}
lookup:
	return table->table[index];
}
