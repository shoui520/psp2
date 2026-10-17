/* SceUsbstorMg: undocumented driver imports, named by their original NIDs. */
#pragma once

#include <psp2kern/types.h>
#include <magicgate/magicgate.h>

int SceSblACMgrForDriver_A27E47A7(int pid);
int SceMagicGateForDriver_0201E54B(void *info);
int SceMagicGateForDriver_144BFA76(int handle);
int SceMagicGateForDriver_1BCE34AD(int handle, unsigned index, void *buf, SceSize size);
int SceMagicGateForDriver_30DF2E82(void);
int SceMagicGateForDriver_42A9D74E(int handle, void *tag, const void *info,
				const void *challenge, const void *response, void *buf);
int SceMagicGateForDriver_46A28FAA(void);
int SceMagicGateForDriver_4F4B2DC9(void *dst, const void *src, const void *iv,
				SceSize size, int handle);
int SceMagicGateForDriver_593016E1(void *tag, const void *buf, unsigned reserved,
				SceSize size, int handle);
int SceMagicGateForDriver_6D3B42E6(void);
int SceMagicGateForDriver_758D7158(void *buf, SceSize size);
int SceMagicGateForDriver_75B445DA(int handle, unsigned index, const void *buf, SceSize size);
int SceMagicGateForDriver_7B052A10(void);
int SceMagicGateForDriver_8E2E577E(void);
int SceMagicGateForDriver_B0827B4F(void *dst, const void *src, const void *iv,
				SceSize size, int handle);
int SceMagicGateForDriver_CA74A13D(void *buf, SceSize size, const void *temp,
				SceSize tempSize, const void *challenge);
int SceMagicGateForDriver_D819D5C6(int handle, MagicGateFileInfo *info);
int SceMagicGateForDriver_FEF11D84(unsigned type);
