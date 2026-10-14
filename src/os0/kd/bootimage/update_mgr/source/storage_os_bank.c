#include "update_internal.h"

int StorageUtilSwitchOSBank(SceUID heapId, SceMbr *mbr)
{
	SceMbrPartitionEntry *partition = mbr->partitions;

	do {
		if (!partition)
			return (int)0x800f0216;
		if (partition->code == 3) {
			if (partition->active & 1) {
				partition->active = 0;
			} else {
				unsigned int offset = partition->offset;

				partition->active = 1;
				mbr->current_os_lba = offset;
			}
		}
		partition++;
	} while (partition != mbr->partitions + 16);
	return 0;
}
