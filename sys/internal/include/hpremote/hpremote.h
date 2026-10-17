/* SceHpremote dependencies with unresolved NID names. */
#pragma once
#include <stdint.h>
#include <syscon/syscon.h>
int SceHpremoteForDriver_D22913DB(void (*handler)(int, int, int, int, int, void *),
			       void *arg);
int SceHpremoteForDriver_219C58F1(void);
int SceHpremoteForDriver_2229EF51(void);
int SceHpremoteForDriver_71B5FB67(void);
int SceCodecForDriver_CF398D8A(void);
int SceCodecForDriver_E483B663(int enable);
