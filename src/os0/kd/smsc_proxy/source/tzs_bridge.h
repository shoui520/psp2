/*
 * TrustZone bridge: 2 x 8 message buffers of 0x80 bytes in the sysroot shared
 * memory (ARM->TZ requests, TZ->ARM interrupt messages) plus a control word.
 */

#pragma once

#include <psp2kern/types.h>

#define TZS_BRIDGE_MSG_SIZE	0x80
#define TZS_BRIDGE_MSG_COUNT	8

int sceSblTzsBridgeInit(void);
int sceSblTzsBridgeAllocArgArea(SceSize size, void **buf, SceSize *bufSize);
int sceSblTzsBridgePushArgArea(int index);
int sceSblTzsBridgePopArgArea(int index, void **buf, SceSize *bufSize);
int sceSblTzsBridgePopIntrArea(void **buf, SceSize *bufSize);
int sceSblTzsBridgeFreeArgArea(int index);
