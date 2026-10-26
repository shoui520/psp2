/*
 * Board: the MCU's set-up, the clocks (a system clock of 16 MHz from the 24 MHz
 * oscillator) and the pins, and the program's devices.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x_type.h"

/* the clocks, the pins, the vector table and the interrupt priorities */
void InitBoard();

/* the clocks again, after the STOP mode (which runs on the HSI) */
void RestoreClocks();

struct DeviceInfo;

/* the program's devices set up, then started */
void InitDevices();
void StartDevices();

/* what main prints: the device information and the firmware's checksum */
const DeviceInfo *GetDeviceInfo();
const u16 *GetChecksum();

/* the host link on again, after the STOP mode */
void ResumeFromStop();

#endif
