#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

int sceSblRtcMgrGetCpRtcPhysicalForUser(SceUInt32 *time);
int sceSblRtcMgrSetCpRtcPhysicalForUser(SceUInt32 time);
