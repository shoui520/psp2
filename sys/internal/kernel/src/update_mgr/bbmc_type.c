#include "bbmc_update.h"

int BbmcUtilCheckFirmwareType(uint64_t hardwareInfo)
{
	uint32_t actualHardware;
	int (*getHardware)(int, uint32_t *) = SceBbmcForDriver_E89B746D;
	int result;

	UpdateUtilLockCom();
	if (!updateBbmcInitialized) {
		result = BbmcUtilInitialize();
		if (result) {
			UpdateUtilUnlockCom();
			return result;
		}
	}
	UpdateUtilUnlockCom();
	if (!BbmcUtilIsAvailable())
		return (int)0x800f0202;
	result = getHardware(updateBbmcContext.subscriber.subscriberId,
		&actualHardware);
	if (!result && (uint64_t)actualHardware != hardwareInfo)
		result = (int)0x800f0202;
	return result;
}
