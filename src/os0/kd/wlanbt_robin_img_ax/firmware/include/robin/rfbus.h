/* Serial register bus to the radio and the register-write tables for it. */
#ifndef ROBIN_RFBUS_H
#define ROBIN_RFBUS_H

#include <stdint.h>

/* One entry of a register-write table; the table ends with 0xffff, 0xff. */
struct PhyRegisterEntry {
	uint16_t reg;
	uint8_t value;
	uint8_t padding;
};

int rfBusEnable(void);
void rfBusWaitIdle(void);
void rfBusLock(void);
void rfBusUnlock(void);
void rfBusAcquire(void);
void rfBusRelease(void);
uint8_t rfBusRead(uint16_t reg);
void rfBusWrite(uint16_t reg, uint8_t value);
void rfBusWriteTable(const struct PhyRegisterEntry *table);
void rfBusSetBits(uint16_t reg, uint8_t mask);
void rfBusClearBits(uint16_t reg, uint8_t mask);
void rfBusWriteIndexed(int index, uint8_t value);

#endif /* ROBIN_RFBUS_H */
