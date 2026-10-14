/* SceHpremote dependencies with unresolved NID names. */
#pragma once
#include <stdint.h>
int SceHpremoteForDriver_D22913DB(void (*handler)(int, int, int, int, int, void *),
			       void *arg);
int SceHpremoteForDriver_219C58F1(void);
int SceHpremoteForDriver_2229EF51(void);
int SceHpremoteForDriver_71B5FB67(void);
int SceSysconForDriver_142D5E82(void);
int ksceSysconGetMultiCnInfo(uint32_t *info);
int SceSysconForDriver_35E1689F(void (*handler)(int), void *arg);
int SceSysconForDriver_5CDDA14D(int enable);
int SceSysconForDriver_63352A39(void (*handler)(int), void *arg);
int SceSysconForDriver_ACC7F71E(void);
int SceSysconForDriver_C3504ADE(void);
uint32_t ksceSysconGetHardwareInfo(void);
int SceSysconForDriver_DE613081(void (*handler)(int), void *arg);
int SceCodecForDriver_CF398D8A(void);
int SceCodecForDriver_E483B663(int enable);
