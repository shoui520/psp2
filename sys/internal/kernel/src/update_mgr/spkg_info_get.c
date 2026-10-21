#include "update_internal.h"
#include <sysmem/sysclib.h>
#include "spkg_peripherals.h"
#include "bbmc_update.h"
#include "bic_update.h"
#include "motion_update.h"
#include "syscon_update.h"
#include "touch_update.h"

int SpkgInfoUtilGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info)
{
	uint32_t storedVersion, status = 0, version;
	int result = SpkgInfoUtilGetVersion(packageType, 16, &storedVersion, 1, -1);

	version = 0;
	switch (packageType) {
	case 8:
		if (!SysconUtilGetFirmwareVersion(&version))
			goto liveVersion;
		break;
	case 20:
		if (SysconUtilGetConfigVersion(&version))
			break;
		/* Successful configuration query continues through the CP query. */
	case 11:
		if (!CpUtilGetBoardVersion(&version))
			goto liveVersion;
		break;
	case 12:
		if (!MotionUtilGetFirmwareVersion(&version))
			goto liveVersion;
		break;
	case 13:
		if (!BbmcUtilGetFirmwareVersion(&version))
			goto liveVersion;
		break;
	case 15:
		if (!MotionMagnUtilGetFirmwareVersion(&version))
			goto liveVersion;
		break;
	case 16:
		if (!TouchUtilGetConfigVersion(0, &version))
			goto liveVersion;
		break;
	case 17:
		if (!TouchUtilGetFirmwareVersion(0, &version))
			goto liveVersion;
		break;
	case 18:
		if (!BicUtilGetFirmwareVersion(&version))
			goto liveVersion;
		break;
	case 19:
		if (!BicUtilGetDataFlashVersion(&version))
			goto liveVersion;
		break;
	case 25:
		if (!SysconUtilGetDownloaderVersion(&version))
			goto liveVersion;
		break;
	}
	if (result)
		return (int)0x800f0202;
	memset(info, result, sizeof(*info));
	info->size = sizeof(*info);
	info->version = storedVersion;
	goto statusCheck;

liveVersion:
	{
		uint32_t liveVersion = version;

		memset(info, 0, sizeof(*info));
		info->size = sizeof(*info);
		info->version = liveVersion;
	}
statusCheck:
	if (packageType == 13) {
		if (BbmcUtilGetStatus(&status) || !status)
			info->status[0] = 1;
	} else if (packageType == 28) {
		result = SpkgInfoUtilGetVersion(28, 64, &version, 1, -1);
		info->status[1] = !result && version != 0;
	}
	return 0;
}
