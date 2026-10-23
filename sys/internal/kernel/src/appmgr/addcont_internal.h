#pragma once

#include <scetypes.h>

struct AppmgrProcessContent;

/* addcont.c */
int appmgrCheckRifUpdatable(const struct AppmgrProcessContent *content);
int sceAppMgrContentInstallPeriodStart(void);
int sceAppMgrContentInstallPeriodStop(void);
int appmgrRemoveTemporaryPhoto(SceUID pid);
