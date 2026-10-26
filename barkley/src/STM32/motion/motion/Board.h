/*
 * Board: the MCU's set-up, the clocks (a system clock of 16 MHz from the 24 MHz
 * oscillator) and the pins, and the program's devices.
 */
#ifndef BOARD_H
#define BOARD_H

/* the clocks, the pins, the vector table and the interrupt priorities */
void InitBoard();

/* the clocks again, after the STOP mode (which runs on the HSI) */
void RestoreClocks();

#endif
