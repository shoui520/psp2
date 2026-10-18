#pragma once

#include <scetypes.h>
#include <stdint.h>

struct NpDrmPackageInstallStartedArgs;

int _sceNpDrmPackageCheck(const void *buffer, unsigned int size, const unsigned char *digest, unsigned int identifier);
int _sceNpDrmPackageInstallStarted(int kind, unsigned int total, const char *msg, const struct NpDrmPackageInstallStartedArgs *opt);
int _sceNpDrmPackageUninstallStarted(int kind, unsigned int total, const char *msg, const struct NpDrmPackageInstallStartedArgs *opt);
int _sceNpDrmSaveDataFormatStarted(int kind, unsigned int total, const char *msg, const struct NpDrmPackageInstallStartedArgs *opt);
int _sceNpDrmSaveDataInstallStarted(int kind, unsigned int total, const char *msg, const struct NpDrmPackageInstallStartedArgs *opt);
int sceNpDrmPackageInstallOngoing(int kind, unsigned int percent);
int sceNpDrmPackageIsGameExist(void);
int sceNpDrmPackageUninstallOngoing(int kind, unsigned int percent);
int sceNpDrmSaveDataFormatOngoing(int kind, unsigned int percent);
int sceNpDrmSaveDataInstallOngoing(int kind, unsigned int percent);
