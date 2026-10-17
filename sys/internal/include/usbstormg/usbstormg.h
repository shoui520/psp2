/* SceUsbstorMg: undocumented driver imports, named by their original NIDs. */
#pragma once

#include <psp2kern/types.h>
#include <magicgate/magicgate.h>

int SceSblACMgrForDriver_A27E47A7(int pid);
int SceMagicGateForDriver_1BCE34AD(int handle, unsigned index, void *buf, SceSize size);
int SceMagicGateForDriver_42A9D74E(int handle, void *tag, const void *info,
				const void *challenge, const void *response, void *buf);
int SceMagicGateForDriver_593016E1(void *tag, const void *buf, unsigned reserved,
				SceSize size, int handle);
int SceMagicGateForDriver_75B445DA(int handle, unsigned index, const void *buf, SceSize size);
