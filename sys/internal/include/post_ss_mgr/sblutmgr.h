#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int sceSblUtMgrGetCurrentSecureTick(SceUInt32 *left);
int sceSblUtMgrIsTrilithiumFlagEnabled(char *flag, SceSize size);
int sceSblUtMgrReadUtoken(void *utoken, SceSize size);
int sceSblUtMgrResetUtokenFile(void);
int sceSblUtMgrUpdateUtoken(const void *utoken, SceSize size);
