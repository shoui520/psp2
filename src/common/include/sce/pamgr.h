/*
 * ScePamgrForDriver (performance analysis manager), as used so far.
 */

#pragma once

#include <psp2kern/types.h>

/* name unknown: a 64-bit timestamp (os0:kd/deci4p_ttyp.skprx output packets) */
SceUInt64 ScePamgrForDriver_FE9BFD27(void);

/* recovered from os0:kd/deci4p_pamp.skprx call sites (key: from sceKernelPaRegister) */
int sceKernelPaRegister(void);
int sceKernelPaUnregister(int key);
int ScePamgrForDriver_0B7F6F53(int key);
int ScePamgrForDriver_0BCD4966(int key, int unit, SceUInt16 value);
int ScePamgrForDriver_16A24732(int unit, int index, SceUInt32 value);
int ScePamgrForDriver_17C81A92(int key, void *config);
int ScePamgrForDriver_1A7733E0(int key);
SceUInt32 ScePamgrForDriver_3ACF4316(void);
int ScePamgrForDriver_3C12A0CD(int key, void (*callback)(int event, void *ctx), void *ctx);
int ScePamgrForDriver_49A43754(int key, void *config);
int ScePamgrForDriver_5D3D46E4(int key, void *config);
int ScePamgrForDriver_6005FDAA(int unit, int *value);
void ScePamgrForDriver_7F3814A4(void *info);
SceUInt32 ScePamgrForDriver_80CA3B39(void);
int ScePamgrForDriver_81764B21(int key);
int ScePamgrForDriver_95F4079E(int key, SceUInt32 physical);
int ScePamgrForDriver_9935D8EF(int key);
int ScePamgrForDriver_A1E448E7(int key, void *config);
int ScePamgrForDriver_A708D62A(int key);
int ScePamgrForDriver_B9EE1620(int key);
int ScePamgrForDriver_D9EFF812(int key, SceUInt32 physical);
int ScePamgrForDriver_DB62FEFE(int key);
int ScePamgrForDriver_E8FF4812(int key, void *config);
SceUInt32 ScePamgrForDriver_EB70556B(void);
int ScePamgrForDriver_F2D5105A(int key, void *config);
int ScePamgrForDriver_FB04BBEF(int key);
SceUInt32 ScePamgrForDriver_FB46C3CD(void);
