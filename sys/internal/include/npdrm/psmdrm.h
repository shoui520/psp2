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
