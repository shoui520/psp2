/*
 * The boot loaders' Pervasive driver (pervasive_kbl.c, pervasive_init_kbl.c):
 * what boot.c and pervasive_init_kbl.c call.
 */
#pragma once

#include <stdint.h>

/* pervasive_init_kbl.c */
int PervasiveInit(void);

/* pervasive_kbl.c */
int ArmDebuggerResetDisable(void);
int DebugPaResetDisable(void);
int DebugBusResetDisable(void);
int BootDeviceResetDisable(void);
int Lpddr2ResetDisable(int port);
int TimerResetDisable(void);
int DebugDeviceResetDisable(void);
int Lpddr2IsolationDisable(int port);
int ArmGateEnable(uint32_t mask);
int ArmDebuggerGateEnable(void);
int DebugPaGateEnable(void);
int DebugBusGateEnable(void);
int BootDeviceGateEnable(void);
int Lpddr2GateEnable(int port);
int TimerGateEnable(void);
int DebugDeviceGateEnable(void);
int LowioPixelClockInit(void);
int PervasiveMiscInit(void);

/* the reset registers, which boot.c uses too */
#define PERVASIVE_RESET		((ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE)
