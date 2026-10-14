#include "bbmc_update.h"

int UpdateUtilLockCom(void);
int UpdateUtilUnlockCom(void);

int BbmcUtilGetFirmwareVersion(uint32_t *version)
{
	int result;

	if (!version)
		return (int)0x800f0216;
	UpdateUtilLockCom();
	if (updateBbmcInitialized) {
		result = (int)0x800f0202;
	} else {
		result = BbmcUtilInitialize();
		if (result) {
			UpdateUtilUnlockCom();
			return result;
		}
	}
	UpdateUtilUnlockCom();
	if (!BbmcUtilIsAvailable())
		return result;
	if (updateBbmcContext.firmwareVersion) {
		*version = updateBbmcContext.firmwareVersion;
		return 0;
	}
	result = SceBbmcForDriver_7CB72009(
		updateBbmcContext.subscriber.subscriberId, version);
	if (!result)
		updateBbmcContext.firmwareVersion = *version;
	return result;
}
