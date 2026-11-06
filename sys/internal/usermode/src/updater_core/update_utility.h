/* UpdateUtility (update_utility.cpp): the update mode, the device and power. The class name is ours. */
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
};

#endif
