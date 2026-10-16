#include "update_internal.h"
#include <psp2kern/kernel/sysclib.h>

int SysconUtilGetFirmwareVersion(uint32_t *version);
int SysconUtilGetConfigVersion(uint32_t *version);
int SysconUtilGetDownloaderVersion(uint32_t *version);
int MotionUtilGetFirmwareVersion(uint32_t *version);
int MotionMagnUtilGetFirmwareVersion(uint32_t *version);
int TouchUtilGetConfigVersion(unsigned int port, uint32_t *version);
int BicUtilGetFirmwareVersion(uint32_t *version);
int BicUtilGetDataFlashVersion(uint32_t *version);
int CpUtilGetBoardVersion(uint32_t *version);
int BbmcUtilGetFirmwareVersion(uint32_t *version);
int BbmcUtilGetStatus(uint32_t *status);

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
	storedVersion = version;
	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	info->version = storedVersion;
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
