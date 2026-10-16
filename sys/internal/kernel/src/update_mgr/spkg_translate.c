#include "update_internal.h"

int SpkgInfoUtilTranslateParams(int packageType, int flags,
		uint32_t *purpose, uint32_t *bank)
{
	SceMbr mbr;
	int result;

	if (!bank || !purpose)
		return (int)0x800f0216;
	result = StorageUtilReadMbr(&mbr);
	if (result)
		return result;
	switch (packageType) {
	case 1:
	case 21: {
		uint32_t firstOffset = 0;
		uint32_t secondOffset = 0;
		const SceMbrPartitionEntry *partition;

		*purpose = 3;
		for (partition = mbr.partitions; partition != mbr.partitions + 16;
		     ++partition) {
			if (partition->code == 3) {
				if (firstOffset) {
					if (!secondOffset)
						secondOffset = partition->offset;
				} else {
					firstOffset = partition->offset;
				}
			}
		}
		if (flags != 16)
			*bank = firstOffset == mbr.current_os_lba;
		else
			*bank = firstOffset != mbr.current_os_lba;
		return 0;
	}
	case 8:
		*purpose = 5;
		*bank = 0;
		return 0;
	case 9:
		*purpose = 2;
		if (flags == 16)
			*bank = mbr.current_bl_lba != mbr.bl_bank0_lba;
		else
			*bank = mbr.current_bl_lba == mbr.bl_bank0_lba;
		return 0;
	case 10:
	case 22:
		*purpose = 4;
		*bank = 0;
		return 0;
	case 11:
		*purpose = 6;
		*bank = 0;
		return 0;
	case 12:
		*purpose = 8;
		*bank = 0;
		return 0;
	case 16:
		*purpose = 9;
		*bank = 0;
		return 0;
	case 23:
		*purpose = 12;
		*bank = 0;
		return 0;
	case 24:
		*purpose = 13;
		*bank = 0;
		return 0;
	case 28:
		*purpose = 14;
		*bank = flags == 64;
		return 0;
	default:
		return (int)0x800f0202;
	}
}
