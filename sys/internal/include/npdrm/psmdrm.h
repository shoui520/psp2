#pragma once

#include <stdint.h>
#include <npdrm/npdrm.h>

struct NpDrmCheckActDataArgs;

int scePsmDrmGetActInfo(int *actType, int *versionFlag, uint64_t *accountId, const struct NpDrmCheckActDataArgs *userArgs);
int scePsmDrmGetRifName(char *name);
int scePsmDrmRemoveActData(uint64_t *accountId);

int scePsmDrmGetRifInfo(const ScePsmDrmLicense *license, char *contentId, uint64_t *accountId, const struct NpDrmCheckActDataArgs *userArgs);
int scePsmDrmGetRifKey(const ScePsmDrmLicense *license_buf, ScePsmDrmKeySet *keydata, int *flags, const ScePsmDrmExpireTime *exp_time);
int scePsmDrmGetRifNameForInstall(char *name, const ScePsmDrmLicense *license);

/* ScePsmDrmForDriver: not written in C yet, the prototypes are the callers' (package.c, gcauthmgr) */
int ScePsmDrmForDriver_4CD5375C(SceUInt64 *accountId);	/* vitasdk: kscePsmDrmRemoveActData */
/* vitasdk: kscePsmDrmGetRifKey */
int ScePsmDrmForDriver_8C8CFD01(const ScePsmDrmLicense *license, ScePsmDrmKeySet *keydata,
				int *flags, SceRtcTick *startTime, SceRtcTick *expirationTime);
/* vitasdk: kscePsmDrmGetRifInfo */
int ScePsmDrmForDriver_984F9017(const ScePsmDrmLicense *license, char *contentId,
				SceUInt64 *accountId, SceUInt64 *startTime,
				SceUInt64 *expirationTime);
/* vitasdk: kscePsmDrmGetActInfo */
int ScePsmDrmForDriver_B09003A7(int *actType, int *versionFlag, SceUInt64 *accountId,
				SceUInt64 *startTime, SceUInt64 *expirationTime);
int ScePsmDrmForDriver_CB73E9D3(void *activation, const char *aesKey);
