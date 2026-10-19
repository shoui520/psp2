/*
 * SceHpremote (os0:kd/hpremote.skprx): SceHpremoteForDriver, the headphone remote's exports, each
 * declared once with its definition as the prototype.
 */
#pragma once
#include <stdint.h>
#include <syscon/syscon.h>

int SceHpremoteForDriver_D22913DB(void (*handler)(int, int, int, int, int, void *), void *arg);
int SceHpremoteForDriver_C7154136(int callback);
int SceHpremoteForDriver_68D64A0F(int callback);
int SceHpremoteForDriver_71B5FB67(void);	/* the remote's input */
int SceHpremoteForDriver_2229EF51(void);	/* the remote's multi type */
int SceHpremoteForDriver_7E69EDA4(void);	/* status 0 */
int SceHpremoteForDriver_208DB1BD(void);	/* status 1 */
int SceHpremoteForDriver_219C58F1(void);	/* status 2 */
int SceHpremoteForDriver_7735C403(void);	/* headphone remote off */
int SceHpremoteForDriver_4D1E622C(void);
int SceHpremoteForDriver_7B038374(void **state);
