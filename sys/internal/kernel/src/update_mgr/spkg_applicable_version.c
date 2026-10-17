#include "update_internal.h"
#include "spkg_hardware.h"
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sblaimgr.h>

int SpkgInfoUtilGetApplicableVersion(int type, UpdateApplicableVersion *info)
{
	SceSblUsSpkgInfo packageInfo;
	unsigned int version, minimumVersion;
	int hardwareIndex = 0;
	int result;

	if (!info)
		return (int)0x800f0216;
	memset(info, 0, sizeof(*info));
	if (type != 1 && type != 9)
		return 0;
	result = SpkgInfoUtilGetSpkgInfo(type, &packageInfo);
	if (result)
		return result;
	version = packageInfo.version;
	result = SpkgInfoUtilGetHardwareIndex(&hardwareIndex);
	if (result)
		return result;
	minimumVersion = spkgHardwarePolicies[hardwareIndex].minimumVersion;
	if (minimumVersion == 0xffffffffu)
		minimumVersion = version;
	else if (SceSblAIMgrForDriver_D78B04A2() && minimumVersion < version)
		minimumVersion = version;
	info->version = minimumVersion;
	info->condition = 1;
	return 0;
}
