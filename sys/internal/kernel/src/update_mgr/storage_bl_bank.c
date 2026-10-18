#include <iofilemgr/iofilemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "update_internal.h"
#include "sbls.h"

int StorageUtilSwitchBLBank(SceUID heapId, SceMbr *mbr)
{
	SceSlb2Header *header;
	SceSlb2Entry *entry;
	SceMbrPartitionEntry *partition;
	SceUID fd;
	uint32_t index, entryCount, sectorCount, loaderSector, bank;
	int permission, result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	header = SceSysmemForDriver_7B4CB60A(heapId, 4096);
	if (!header) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x800f020c;
	}
	memset(header, 0, 4096);
	fd = StorageUtileGetPartitionFd(2, 0);
	if (fd < 0) {
		updateLastResult = 0x8010002;
		result = (int)0x800f0205;
		goto cleanup;
	}
	if (sceIoRead(fd, header, 4096) != 4096) {
		updateLastResult = 0x8010003;
		result = (int)0x800f0205;
		goto cleanup;
	}
	if (header->magic != SCE_SLB2_MAGIC) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	entryCount = header->entryNum;
	if (!entryCount) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	entry = header->entryList;
	index = 0;
	sectorCount = 0;
	loaderSector = 0;
	do {
		index++;
		if (!strncmp(entry->m_file_name, "second_loader.enp", 31)) {
			loaderSector = entry->fileStartSector;
			sectorCount = (entry->fileSizeInByte + 511) >> 9;
		}
		entry++;
	} while (index != entryCount);
	if (!sectorCount) {
		result = (int)0x800f0224;
		goto cleanup;
	}
	bank = mbr->bl_bank0_lba;
	if (mbr->current_bl_lba == bank)
		bank = mbr->bl_bank1_lba;
	mbr->current_bl_lba = bank;
	mbr->loader_start = loaderSector + bank;
	mbr->loader_count = sectorCount;
	partition = mbr->partitions;
	for (index = 0; index != 16; index++) {
		if (partition->code == 2)
			partition->active = !(partition->active & 1);
		partition++;
	}
	result = 0;
cleanup:
	sceKernelFreeHeapMemory(heapId, header);
	sceIoClose(fd);
	sceKernelSetThreadAccessLevel(permission);
	return result;
}
