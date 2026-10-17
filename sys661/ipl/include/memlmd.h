/*
 * Module decryption (source/memlmd.c).
 */
#ifndef MEMLMD_H
#define MEMLMD_H

#include "ipl.h"

int IsRamAddress(u32 addr);
int sceUtilsBufferCopyByPolling(void *out, void *in, int cmd);
int kirk7(void *buf, SceSize size, u32 keyCode);
int kirk8(void *buf, SceSize size);
int memlmdMemcmp(const u8 *a, const u8 *b, SceSize n);
int checkWhitelist(const u8 *prx, const u8 *list, SceSize listSize);
int sceUtilsGetLoadModuleABLengthByPolling(u8 *prx, SceSize size, SceSize *newSize);
int memlmdUnsign(u8 *prx, SceSize size);
int memlmdInitializeScrambleKey(u32 unk, const u8 *hash);
int memlmdClearScrambleKey(u32 unk);
int memlmdCompareSubType(u32 tag);
int memlmdCompareLatestSubType(u32 tag);

#endif
