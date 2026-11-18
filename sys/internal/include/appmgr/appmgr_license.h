/* An application's license as appmgr reads it. */
#pragma once

#include <stddef.h>
#include <scetypes.h>

/*
 * An application's license, as appmgrCheckRifGD reads it from the license file of a path
 * (sceAppMgrCheckRifGD; the shell reads its flags and times)
 */
typedef struct AppmgrLicenseInfo {
	char contentId[48];
	int licenseVersion;
	int licenseFlags;
	int flags;
	unsigned int skuFlags;
	SceUInt64 accountId;
	SceUInt64 flags2;
	SceUInt64 licStartTime;
	SceUInt64 licExpTime;
	unsigned char klicense[16];
} AppmgrLicenseInfo;

_Static_assert(sizeof(AppmgrLicenseInfo) == 112, "license information");
_Static_assert(offsetof(AppmgrLicenseInfo, licenseVersion) == 48 &&
	       offsetof(AppmgrLicenseInfo, licenseFlags) == 52 &&
	       offsetof(AppmgrLicenseInfo, flags) == 56 &&
	       offsetof(AppmgrLicenseInfo, skuFlags) == 60 &&
	       offsetof(AppmgrLicenseInfo, accountId) == 64 &&
	       offsetof(AppmgrLicenseInfo, flags2) == 72 &&
	       offsetof(AppmgrLicenseInfo, licStartTime) == 80 &&
	       offsetof(AppmgrLicenseInfo, licExpTime) == 88 &&
	       offsetof(AppmgrLicenseInfo, klicense) == 96,
	       "license information fields");
