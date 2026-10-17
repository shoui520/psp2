#pragma once

#include <psp2/types.h>

int sceRegMgrGetKeyBin(const char *category, const char *name,
		     void *buffer, SceSize size);
int sceRegMgrSetKeyBin(const char *category, const char *name,
		     const void *buffer, SceSize size);

int sceRegMgrSetKeyInt(const char *category, const char *name, int value);
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

int ksceRegMgrSystemParamGetInt(int id, int *value);
