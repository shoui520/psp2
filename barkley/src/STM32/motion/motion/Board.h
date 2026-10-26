/*
 * Board: the MCU's set-up, the clocks (a system clock of 16 MHz from the 24 MHz
 * oscillator) and the pins, and the program's devices.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x_type.h"

/* RCC_CFGR2 (the value line's PREDIV1, the oscillator's divider before the PLL); the
   library has no field for it */
#define RCC_CFGR2	(*(vu32 *)(RCC_BASE + 0x2C))

/* the clocks (image 00: main sets them), the pins, the vector table and the interrupt
   priorities */
void InitBoard();

#if MOTION_IMAGE != 0
/* the clocks again, after the STOP mode (which runs on the HSI) */
void RestoreClocks();
#endif

struct DeviceInfo;

/* the program's devices set up, then started */
void InitDevices();
void StartDevices();

/* what main prints: the device information and the firmware's checksum */
const DeviceInfo *GetDeviceInfo();
const u16 *GetChecksum();

#if MOTION_IMAGE != 0
/* the host link on again, after the STOP mode */
void ResumeFromStop();
#endif

#endif
