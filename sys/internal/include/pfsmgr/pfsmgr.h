#pragma once

#include <scetypes.h>
#include <stdint.h>

/* the random drive name a PFS mount is reached by (vitasdk's name; no Sony name found) */
typedef struct ScePfsRndDriveId {
	char drive_id[0x10];
} ScePfsRndDriveId;

_Static_assert(sizeof(ScePfsRndDriveId) == 0x10, "ScePfsRndDriveId");

int scePfsAcidDirUnmount(const char *name, const char *dir);
int ScePfsMgrForKernel_27FB7618(const char *name, SceUInt32 *out1, SceUInt32 *out2);
int scePfsMount2(const char *path, const char *name, const void *klicensee, SceUInt32 mode);
int scePfsDisapprove(const char *name, SceUInt64 authId);
int scePfsAcidDirMount(const char *name, const char *dir, const void *klicensee);
int scePfsUnmount(const char *name);
int ScePfsMgrForKernel_7E4B76E6(const char *name, void *out);
int ScePfsMgrForKernel_806D2EA1(const char *name, SceUInt32 value);
int scePfsAcidDirApprove(const char *name, const char *dir);
int scePfsMount(const char *path, const char *name, SceUInt64 authId, const void *klicensee, SceUInt32 mode);
int scePfsAcidDirSet(const char *name, const char *dir);
int scePfsApprove(const char *name, SceUInt64 authId);
int ScePfsMgrForKernel_E9D672AB(const char *name);
