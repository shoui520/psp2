/* Sony: exfatfs.c */
#include "exfatfs_internal.h"

int initializeLegacyDrive(SceExfatfsDrive *drive)
{
	int result;
	int i;

	if (drive->state & 0x4000) {
		drive->state &= 0x2000;
		for (i = 0; i < 64; i++)
			drive->legacyFiles[i]->cachedEntry = 0;
	}
	if (!(drive->state & 0x2000))
		return 0;
	drive->state = 0;
	result = drive->legacyOperations->initialize(drive);
	if (result < 0) {
		drive->state |= 0x20;
		return result;
	}
	drive->state |= 1;
	return 0;
}
