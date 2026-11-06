/*
 * UpdateUtility (Sony's update_utility.cpp): the console's information, the update mode and
 * the result the update leaves for the next boot, power control, and the console checks the
 * update makes before it starts. The class and method names are ours.
 */
#ifndef UPDATER_CORE_UPDATE_UTILITY_H
#define UPDATER_CORE_UPDATE_UTILITY_H

#include <scetypes.h>
#include <updater_core/updater_core.h>

class UpdateUtility {
public:
	UpdateUtility();
	~UpdateUtility();

	int GetDeviceInfo(SceUpdaterDeviceInfo *deviceInfo);
	int GetUpdateModeState(void);
	int GetUpdateMode(SceUInt8 *mode);
	int SetUpdateMode(SceUInt8 mode);
	int UpdateModeSetLastError(int error);
	int UpdateModeGetLastError(int *error);
	int PowerControl(int operation, int flags);
	static int CheckDeviceStatus(void);

private:
	void getResultFilePath(char *path, SceSize size);
	int convertErrorCodePowerControl(int operation, int error);
};

#endif
