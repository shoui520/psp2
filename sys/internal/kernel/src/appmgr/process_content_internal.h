#pragma once

#include "mount.h"

struct AppmgrMapFile;

typedef struct AppmgrProcessContent {
	SceUInt32 selfControlFlags;
	SceUInt32 selfControlMetadata;
	SceUInt32 selfControlMode;
	SceUInt32 maxFD;
	SceUInt32 maxDIR;
	SceUInt32 primaryResourceQuota;
	SceUInt32 secondaryResourceQuota;
	unsigned char reservedControl[32];
	SceUInt32 launchFlags;
	SceUInt32 appVersion;
	unsigned char reservedVersion[8];
	SceUInt32 savedataVersion;
	SceUInt32 savedataOwnershipFlags;
	SceUInt32 savedataMaxSize;
	SceUInt32 savedataEnvironmentFlags;
	SceInt32 safeMemoryMode;
	unsigned char reservedParameters[4];
	char contentId[48];
	char npCommunicationId[13];
	unsigned char padding[3];
	char title[128];
	char shortTitle[52];
	SceUInt32 attributeMinorMode;
	SceUInt32 attributeMinor;
	SceUInt32 contentFlags;
	char devicePrefix[16];
	char appDirectory[292];
	char category[4];
	SceUInt32 attribute;
	SceUInt32 attribute2;
	SceUInt32 savedataAttribute;
	AppmgrLicenseInfo license;
	unsigned char keystone[96];
	char titleId[16];
	char patchTitleId[16];
	char addContTitleIds[8][16];
	char saveDataTitleIds[8][16];
	char originalTitleId[16];
	char foregroundLookupName[16];
	struct AppmgrMapFile *mapFile;
	SceFiosOverlayID overlayIds[16];
	unsigned char reservedTail[12];
} AppmgrProcessContent;

_Static_assert(sizeof(AppmgrProcessContent) == 1288, "process content size");
_Static_assert(offsetof(AppmgrProcessContent, selfControlFlags) == 0,
	       "process content selfControlFlags");
_Static_assert(offsetof(AppmgrProcessContent, selfControlMetadata) == 4,
	       "process content selfControlMetadata");
_Static_assert(offsetof(AppmgrProcessContent, selfControlMode) == 8,
	       "process content selfControlMode");
_Static_assert(offsetof(AppmgrProcessContent, maxFD) == 12,
	       "process content maxFD");
_Static_assert(offsetof(AppmgrProcessContent, maxDIR) == 16,
	       "process content maxDIR");
_Static_assert(offsetof(AppmgrProcessContent, primaryResourceQuota) == 20,
	       "process content primaryResourceQuota");
_Static_assert(offsetof(AppmgrProcessContent, secondaryResourceQuota) == 24,
	       "process content secondaryResourceQuota");
_Static_assert(offsetof(AppmgrProcessContent, reservedControl) == 28,
	       "process content reservedControl");
_Static_assert(offsetof(AppmgrProcessContent, launchFlags) == 60,
	       "process content launchFlags");
_Static_assert(offsetof(AppmgrProcessContent, appVersion) == 64,
	       "process content appVersion");
_Static_assert(offsetof(AppmgrProcessContent, reservedVersion) == 68,
	       "process content reservedVersion");
_Static_assert(offsetof(AppmgrProcessContent, savedataVersion) == 76,
	       "process content savedataVersion");
_Static_assert(offsetof(AppmgrProcessContent, savedataOwnershipFlags) == 80,
	       "process content savedataOwnershipFlags");
_Static_assert(offsetof(AppmgrProcessContent, savedataMaxSize) == 84,
	       "process content savedataMaxSize");
_Static_assert(offsetof(AppmgrProcessContent, savedataEnvironmentFlags) == 88,
	       "process content savedataEnvironmentFlags");
_Static_assert(offsetof(AppmgrProcessContent, safeMemoryMode) == 92,
	       "process content safeMemoryMode");
_Static_assert(offsetof(AppmgrProcessContent, reservedParameters) == 96,
	       "process content reservedParameters");
_Static_assert(offsetof(AppmgrProcessContent, contentId) == 100,
	       "process content contentId");
_Static_assert(offsetof(AppmgrProcessContent, npCommunicationId) == 148,
	       "process content npCommunicationId");
_Static_assert(offsetof(AppmgrProcessContent, padding) == 161,
	       "process content padding");
_Static_assert(offsetof(AppmgrProcessContent, title) == 164,
	       "process content title");
_Static_assert(offsetof(AppmgrProcessContent, shortTitle) == 292,
	       "process content shortTitle");
_Static_assert(offsetof(AppmgrProcessContent, attributeMinorMode) == 344,
	       "process content attributeMinorMode");
_Static_assert(offsetof(AppmgrProcessContent, attributeMinor) == 348,
	       "process content attributeMinor");
_Static_assert(offsetof(AppmgrProcessContent, contentFlags) == 352,
	       "process content contentFlags");
_Static_assert(offsetof(AppmgrProcessContent, devicePrefix) == 356,
	       "process content devicePrefix");
_Static_assert(offsetof(AppmgrProcessContent, appDirectory) == 372,
	       "process content appDirectory");
_Static_assert(offsetof(AppmgrProcessContent, category) == 664,
	       "process content category");
_Static_assert(offsetof(AppmgrProcessContent, attribute) == 668,
	       "process content attribute");
_Static_assert(offsetof(AppmgrProcessContent, attribute2) == 672,
	       "process content attribute2");
_Static_assert(offsetof(AppmgrProcessContent, savedataAttribute) == 676,
	       "process content savedataAttribute");
_Static_assert(offsetof(AppmgrProcessContent, license) == 680,
	       "process content license");
_Static_assert(offsetof(AppmgrProcessContent, keystone) == 792,
	       "process content keystone");
_Static_assert(offsetof(AppmgrProcessContent, titleId) == 888,
	       "process content titleId");
_Static_assert(offsetof(AppmgrProcessContent, patchTitleId) == 904,
	       "process content patchTitleId");
_Static_assert(offsetof(AppmgrProcessContent, addContTitleIds) == 920,
	       "process content addContTitleIds");
_Static_assert(offsetof(AppmgrProcessContent, saveDataTitleIds) == 1048,
	       "process content saveDataTitleIds");
_Static_assert(offsetof(AppmgrProcessContent, originalTitleId) == 1176,
	       "process content originalTitleId");
_Static_assert(offsetof(AppmgrProcessContent, foregroundLookupName) == 1192,
	       "process content foregroundLookupName");
_Static_assert(offsetof(AppmgrProcessContent, mapFile) == 1208,
	       "process content mapFile");
_Static_assert(offsetof(AppmgrProcessContent, overlayIds) == 1212,
	       "process content overlayIds");
_Static_assert(offsetof(AppmgrProcessContent, reservedTail) == 1276,
	       "process content reservedTail");

extern const char appmgrProgramCategories[6][4];
int appmgrValidateSaveDataMount(SceUID pid, const char *path, int skipVerification,
			      AppmgrProcessContent *content, SceUInt32 *maxSize);
int appmgrReadKeystone(SceUID pid, const char *driveId, void *keystone);
int appmgrUpdateFakeNoMemoryCardOverlays(SceUID pid, AppmgrProcessContent *content);
int appmgrReadSaveDataMetadata(const char *path, AppmgrProcessContent *content,
			     SceUInt32 *maxSize, SceUInt64 *accountId);
int appmgrUpdateSaveDataParams(const char *path, const char *titleId,
			     SceUInt32 maxSize, SceUInt32 *flag);
int appmgrGetAddContPath(const AppmgrProcessContent *content, const char *titleId,
			char *path, SceSize size);
int appmgrSelectSafeMemoryMode(const AppmgrProcessContent *content);
