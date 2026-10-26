/*
 * main: the system and the devices set up, the program information printed (UserVer,
 * HwInfo, serial, LoaderVer, the firmware's checksum), then the devices started and the
 * tasks run; after each STOP mode the clocks, the timer slots and the host link are
 * restored and the tasks run again.
 */

#include <stdio.h>

#include "Board.h"
#include "MotionProcessor.h"
#include "System.h"

int main()
{
	const DeviceInfo *info;
	const u16 *checksum;

	InitSystem();
	InitDevices();
	info = GetDeviceInfo();
	checksum = GetChecksum();
	printf("\n\n-- Motion Program Info --\n");
	printf("UserVer   : 0x%04X(%u)\n", info->userVersion, info->userVersion);
	printf("HwInfo    : 0x%04X(%u)\n", info->hwInfo, info->hwInfo);
	printf("serial    : %lu\n", info->serial);
	printf("LoaderVer : 0x%04X(%u)\n", info->loaderVersion, info->loaderVersion);
	printf("Firm csum : 0x%04X\n", *checksum);
	printf("-------------------------\n");
	StartSystem();
	StartDevices();
	for (;;) {
		RunTasks(0, 0);
		ResumeSystem();
		ResumeFromStop();
	}
}
