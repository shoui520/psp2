/*
 * The radio's registers in the bus modes of the RAM code (sys/rfreg.c). A
 * mode is named by the configuration it sets.
 */
#ifndef ROBIN_RFREG_H
#define ROBIN_RFREG_H

#include <stdint.h>

extern uint16_t rfBusControlModes[10];	/* SQRAM 0xc0011b80 */

void rfBusEnter0000(void);
void rfBusLeave0000(void);
int rfBusEnter0082(void);
void rfBusLeave0082(void);
unsigned int rfBusEnter0002(void);
void rfBusLeave0002(void);
void rfBusEnter5350(void);
void rfBusLeave5350(void);
void rfBusEnter1350(void);
void rfBusLeave1350(void);
void rfBusWriteReg16(uint16_t reg, uint16_t value);

void rfRegBegin(void);
void rfRegEnd(void);
void rfRegWrite(uint16_t reg, uint16_t value);
uint8_t rfRegRead(uint16_t reg);
void rfRegSetBits(uint16_t reg, uint16_t mask);
void rfRegClearBits(uint16_t reg, uint16_t mask);

#endif /* ROBIN_RFREG_H */
