#pragma once

#include <stddef.h>
#include <psp2kern/types.h>
#include <pfsmgr/pfsmgr.h>

typedef struct AppmgrPfsMount {
	int mountId;
	char appPath[292];
	ScePfsRndDriveId mountPoint;
	char titleId[16];
	SceUInt64 programAuthorityIds[16];
	struct AppmgrPfsMount *prev;
	struct AppmgrPfsMount *next;
} AppmgrPfsMount;

_Static_assert(sizeof(AppmgrPfsMount) == 464, "PFS mount");
_Static_assert(offsetof(AppmgrPfsMount, mountId) == 0 &&
	       offsetof(AppmgrPfsMount, appPath) == 4 &&
	       offsetof(AppmgrPfsMount, mountPoint) == 296 &&
	       offsetof(AppmgrPfsMount, titleId) == 312 &&
	       offsetof(AppmgrPfsMount, programAuthorityIds) == 328 &&
	       offsetof(AppmgrPfsMount, prev) == 456 &&
	       offsetof(AppmgrPfsMount, next) == 460,
	       "PFS mount fields");

typedef struct AppmgrLicenseInfo {
	char contentId[48];
	int licenseVersion;
	int licenseFlags;
	int flags;
	int skuFlags;
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
