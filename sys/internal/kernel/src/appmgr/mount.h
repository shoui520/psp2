#pragma once

#include <stddef.h>
#include <scetypes.h>
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

/* sceAppMgrGetSaveDataInfo's result */
typedef struct AppmgrSaveDataInfo {
	SceUInt32 mediaType;		/* 4 ux0, 3 grw0/gro0, 2 ur0/vs0, 1 other */
	SceUInt32 mounted;
	SceUInt32 broken;
	SceUInt32 savedataVersion;
	SceUInt32 savedataOwnershipFlags;
	SceUInt32 maxSize;
	SceUInt32 freeSizeKiB;
} AppmgrSaveDataInfo;

_Static_assert(sizeof(AppmgrSaveDataInfo) == 28, "save data information");

/* sceAppMgrAddContMount's parameters: the drive the caller expects add-on content at */
typedef struct AppmgrAddContMountParam {
	unsigned char reserved[48];
	char mountPoint[16];
} AppmgrAddContMountParam;

_Static_assert(sizeof(AppmgrAddContMountParam) == 64 &&
	       offsetof(AppmgrAddContMountParam, mountPoint) == 48,
	       "add-on content mount parameters");

int sceAppMgrAddContMount(SceUID pid, const AppmgrAddContMountParam *param);

/* the parameters of the add mounts (add-on content, save data): the title, its passcode and the drive to use */
typedef struct AppmgrAddMountParam {
	char titleId[16];
	char passcode[32];
	ScePfsRndDriveId mountPoint;
} AppmgrAddMountParam;

_Static_assert(sizeof(AppmgrAddMountParam) == 64 &&
	       offsetof(AppmgrAddMountParam, passcode) == 16 &&
	       offsetof(AppmgrAddMountParam, mountPoint) == 48,
	       "add mount parameters");

int sceAppMgrAddContAddMount(const AppmgrAddMountParam *param, int add);
int sceAppMgrSaveDataAddMount(const AppmgrAddMountParam *param);
int sceAppMgrThemeDataMount(const char *contentId, const char *themePath,
			    const char *rifPath, ScePfsRndDriveId *mountPoint);

/* addcont.c */
int sceAppMgrIsRelativeAddContAdd(const char *titleId, int add);

/* savedata.c */
int sceAppMgrIsRelativeSaveDataAdd(const char *titleId);

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

/* mount.c */
int appmgrDecodeString(const unsigned char *source, char *destination, SceSize size);

/* the character classes of title and content IDs */
#define APPMGR_IS_UPPER(c)	((unsigned int)((c) - 'A') <= 'Z' - 'A')
#define APPMGR_IS_DIGIT(c)	((unsigned int)((c) - '0') <= 9)
int appmgrRegisterMountDebugSettingCallback(void);
int appmgrIsDevelopmentPath(const char *path);
int sceAppMgrIsGameProgram(void);
int sceAppMgrIsNonGameProgram(void);
int sceAppMgrIsPsNowClient(void);
int sceAppMgrIsDevelopmentMode(void);
int appmgrIsMediaDriveMountable(int mountId, SceUInt32 contentFlags);
int appmgrGetAddcontTitleId(const char *sfoPath, char *titleId);
int appmgrMakeRandomDriveId(const char *prefix, char *driveId);
int appmgrGetMountPointById(int mountId, char *mountPoint);
int appmgrIsMountIdAllowed(unsigned int mountId);
int appmgrFindPfsMount(AppmgrPfsMount *mount, int mountId, const char *path,
		       AppmgrPfsMount **found);
int appmgrCreatePfsDirectory(unsigned int mountId, const char *path, const void *keystone);
int appmgrMountById(SceUID pid, unsigned int mountId, const char *id, const void *keystone,
		    ScePfsRndDriveId *mountPoint);
int appmgrMountSystemDirectory(SceUID pid, unsigned int mountId, ScePfsRndDriveId *mountPoint);
int sceAppMgrPspSaveDataRootMount(ScePfsRndDriveId *mountPoint);
int sceAppMgrTrophyMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint);
int sceAppMgrWorkDirMountById(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint);
int sceAppMgrGetSaveDataInfo(const char *appPath, const char *titleId, SceUInt32 attributes,
			     AppmgrSaveDataInfo *info);
int sceAppMgrLoadSaveDataSystemFile(const char *path, void *buffer, SceSize size);
int sceAppMgrUpdateSaveDataParam(const char *path);
int sceAppMgrInstallDirMount(unsigned int mountId, const char *id, const void *keystone,
			     ScePfsRndDriveId *mountPoint);
int sceAppMgrLoopBackMount(unsigned int mountId, const char *id, ScePfsRndDriveId *mountPoint);
int appmgrMountPfsAcidDirectory(const char *drive, const char *directory);
int appmgrUnmountPfsAcidDirectory(const char *drive, const char *directory);
int sceAppMgrUmountByPid(SceUID pid, const char *driveId);
int sceAppMgrForceUmount(int mountId);
int appmgrNormalizePath(const char *path, char *normalized);
struct AppmgrProcessContent;
int appmgrReadParamSfo(SceUID pid, const char *path, int isSfoPath,
		       struct AppmgrProcessContent *content);
int appmgrGetVs0UserModuleDrive(char *mountPoint);
int appmgrGetVs0UserDataDrive(char *mountPoint);
int appmgrConvertVs0UserDrivePath(const char *path, char *output, SceSize size);
int appmgrCountPfsMountReferences(unsigned int mountId, int flags);
int appmgrCheckRifGD(const char *path, AppmgrLicenseInfo *info);
int appmgrGetUserDirPathById(int userId, int partitionId, char *output, unsigned int capacity);
int appmgrResolvePath(const char *path, SceUID pid, char *output, SceSize size);
int appmgrReleaseProcessMounts(SceUID pid);
int appmgrResolveLaunchPath(const char *path, char *output, SceSize size);
int sceAppMgrGetRawPath(char *path, char *resolvedPath, int capacity);
int appmgrExtractSaveDataDeviceName(const char *path, char *device, SceSize capacity);
void *appmgrAllocMountHeapMemory(SceSize size);

/* appmgr_system_data.c */
int appmgrRemoveDirectoryTree(const char *path);
int appmgrRemoveFileInSteps(const char *path, unsigned int stepMiB);

/* savedata.c */
int appmgrInitAppInfoCache(SceUID pid);
int appmgrFreeMountHeapMemory(void *memory);

/* The SELF's eboot parameters (its control record 6), as sceAppMgrGetEbootParam copies them */
typedef struct AppmgrEbootParam {
	SceUInt32 reserved;
	SceUInt32 attribute;		/* 1: runs in the background, 4: uses common dialogs */
	SceSize phcontMemSize;		/* KiB */
	SceSize memSize;		/* KiB; 0: the game budget */
	unsigned char padding[240];
} AppmgrEbootParam;

_Static_assert(sizeof(AppmgrEbootParam) == 256, "eboot parameters");

int sceAppMgrGetEbootParam(const char *path, AppmgrEbootParam *ebootParam);
struct AppState;
int appmgrRemoveAddcontMountEntries(struct AppState *app, const char *driveId);
