/*
 * The Pervasive part of SceDriverTzs: pervasive_tzs.c (initialization, power
 * sequences, parking), pervasive_control_tzs.c (reset and gate words) and
 * pervasive_clock_tzs.c (clock selects).
 */
#pragma once

#include "driver_tzs.h"

extern SceKernelSpinlock s_pervasiveLock;

/* pervasive_control_tzs.c */
int ResetSetWithFlag(volatile uint32_t *reg, uint32_t mask, volatile uint32_t *flag, int enable);
int ResetDeviceWithFlag(volatile uint32_t *reg, volatile uint32_t *flag, int enable);
int ResetDisableWithFlag(volatile uint32_t *reg, volatile uint32_t *flag);
int ResetSet(volatile uint32_t *reg, uint32_t mask, int enable);
int ResetDevice(volatile uint32_t *reg, int enable);
int ResetDisableDevice(volatile uint32_t *reg);
int ResetEnableDevice(volatile uint32_t *reg);
int ResetEnableWithFlag(volatile uint32_t *reg, volatile uint32_t *flag);
int ResetEnableDeviceWithFlag(volatile uint32_t *reg, volatile uint32_t *flag);
int PervasiveResetMonitorCall(int offset, int enable, int arg2, int arg3);
int GpuResetEnable(void);
int GpuResetDisable(void);
int AllegrexResetEnable(void);
int AllegrexResetDisable(void);
int SonyRegbusResetEnable(void);
int SonyRegbusResetDisable(void);
int VeneziaResetEnable(void);
int VipResetEnable(void);
int VeneziaRegisterAccessResetEnable(void);
int VeneziaRegisterAccessResetDisable(void);
int VipRegisterAccessResetEnable(void);
int VipRegisterAccessResetDisable(void);
int GpuIsolationEnable(void);
int GpuIsolationDisable(void);
int VeneziaIsolationEnable(void);
int VeneziaIsolationDisable(void);
int GpuBusIdle(void);
int VeneziaBusIdle(void);
int GateSet(volatile uint32_t *reg, uint32_t mask, int enable);
int GateDevice(volatile uint32_t *reg, int enable);
int GateSetDevice(volatile uint32_t *reg, int enable);
int GateDisableDevice(volatile uint32_t *reg);
int GateEnableDevice(volatile uint32_t *reg);
int GateEnable(volatile uint32_t *reg, uint32_t mask);
int GateDisable(volatile uint32_t *reg, uint32_t mask);
int PervasiveGateMonitorCall(int offset, int enable, int arg2, int arg3);
int AllegrexGateEnable(void);
int AllegrexGateDisable(void);
int SonyRegbusGateEnable(void);
int SonyRegbusGateDisable(void);
int CompatRamGateEnable(void);
int CompatRamGateDisable(void);
int VeneziaGateEnable(void);
int VeneziaGateDisable(void);
int VeneziaSecureGateEnable(void);
int VeneziaSecureGateDisable(void);
int VipGateEnable(void);
int VipGateDisable(void);
int GpuPowerClockEnable(void);
int GpuPowerClockDisable(void);
int PervasiveClockMonitorCall(int op, unsigned int value, int arg2, int arg3);

/* pervasive_clock_tzs.c */
int VeneziaClockSet(unsigned int clock);
int VipClockSet(unsigned int clock);
int PixelClockInit(void);
int PervasiveMiscInit(void);
