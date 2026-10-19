#include "bbmc_update.h"
#include "spkg_peripherals.h"

int BbmcUtilGetStatus(uint32_t *status)
{
	uint32_t resultStatus = 0;
	uint32_t version;
	int result;

	result = BbmcUtilGetFirmwareVersion(&version);
	if (result == (int)0x800f0202) {
		*status = 1;
		return 0;
	}
	if (!result) {
		result = SceBbmcForDriver_4281E476(
			updateBbmcContext.subscriber.subscriberId, &resultStatus);
		if (!result)
			*status = resultStatus;
	}
	return result;
}
