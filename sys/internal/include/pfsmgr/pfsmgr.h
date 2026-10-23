#pragma once

#include <scetypes.h>
#include <stdint.h>

/* the random drive name a PFS mount is reached by (vitasdk's name; no Sony name found) */
typedef struct ScePfsRndDriveId {
	char drive_id[0x10];
} ScePfsRndDriveId;

_Static_assert(sizeof(ScePfsRndDriveId) == 0x10, "ScePfsRndDriveId");

/* vitasdk: kscePfsAcidDirUnmount */
int ScePfsMgrForKernel_00C8AF80(const char *name, const char *dir);
int ScePfsMgrForKernel_27FB7618(const char *name, SceUInt32 *out1, SceUInt32 *out2);
/* vitasdk: kscePfsMount2 */
int ScePfsMgrForKernel_2D48AEA2(const char *path, const char *name, const void *klicensee, SceUInt32 mode);
/* vitasdk: kscePfsDisapprove */
int ScePfsMgrForKernel_2D67D8CA(const char *name, SceUInt64 authId);
/* vitasdk: kscePfsAcidDirMount */
int ScePfsMgrForKernel_4A4A8243(const char *name, const char *dir, const void *klicensee);
int ScePfsMgrForKernel_680BC384(const char *name);	/* vitasdk: kscePfsUnmount */
int ScePfsMgrForKernel_7E4B76E6(const char *name, void *out);
int ScePfsMgrForKernel_806D2EA1(const char *name, SceUInt32 value);
/* vitasdk: kscePfsAcidDirApprove */
int ScePfsMgrForKernel_8CDA249A(const char *name, const char *dir);
/* vitasdk: kscePfsMount */
int ScePfsMgrForKernel_A772209C(const char *path, const char *name, SceUInt64 authId, const void *klicensee, SceUInt32 mode);
/* vitasdk: kscePfsAcidDirSet */
int ScePfsMgrForKernel_B0CC0A1A(const char *name, const char *dir);
/* vitasdk: kscePfsApprove */
int ScePfsMgrForKernel_D8D0FEE5(const char *name, SceUInt64 authId);
int ScePfsMgrForKernel_E9D672AB(const char *name);
