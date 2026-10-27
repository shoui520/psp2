/*
 * mem_region.c: keyring slot access permission configuration, keyring wipe on
 * shutdown, memory range validators (Cmep SRAM, SK SRAM, SM SRAM, TrustZone
 * DRAM, non-secure DRAM), and Secure Kernel state / buffer accessors
 * (0x801b0a..0x80203b).
 *
 * Function and variable names are ours.
 */

#include "secure_kernel.h"

/* .sdata (0x807cbc..0x807cd3) */
static uint32_t s_srvkListSize = 0xffffffffu;
static uint32_t s_srvkListAddr = 0xffffffffu;
static uint32_t s_debugLogBufferSize = 0xffffffffu;
static uint32_t s_debugLogBufferAddr = 0xffffffffu;
static uint32_t s_armCommBufferAddr = 0xffffffffu;
static uint32_t s_skState = 1u;

/* .sbss (0x80816c) */
static uint32_t s_smReturnStatusAddr;

/* 0x801b0a */
void setKeyringSlotProtection(uint16_t slotIndex, uint32_t protMask)
{
	REG32(0xe0030024u) = (protMask & 0xffff0000u) | (uint32_t)slotIndex;
}

/* 0x801b20 */
void enableBusErrorInterrupts(void)
{
	REG32(0xe0020000u) = 0x0002000fu;
}

/* 0x801b2c */
int setupKeyringAccessPermissions(void)
{
	uint32_t zeroSlot[8];
	uint32_t slot;

	for (slot = 0u; slot < 0x80u; slot++) {
		if (slot < 2u || slot == 0x10u ||
		    (slot >= 0x21u && slot <= 0x24u) ||
		    (slot >= 0x30u && slot <= 0x34u)) {
			continue;
		}
		if (slot == 8u)
			setKeyringSlotProtection(8u, 0x1ffe0000u);
		else
			setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x100u; slot < 0x180u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x200u; slot < 0x218u; slot++) {
		uint32_t rel = slot - 0x204u;

		if (rel <= 18u) {
			uint32_t bit = 1u << rel;

			if ((bit & 0x0004c003u) != 0u)
				continue;
			if ((bit & 0x00000c00u) != 0u) {
				setKeyringSlotProtection((uint16_t)slot, 0x1fef0000u);
				continue;
			}
		}
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x300u; slot < 0x400u; slot++) {
		uint32_t rel = slot - 0x340u;

		if (rel <= 8u && ((1u << rel) & 0x000001e1u) != 0u)
			continue;
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x400u; slot < 0x480u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x500u; slot < 0x580u; slot++) {
		uint32_t rel = slot - 0x500u;

		if (rel <= 27u && ((1u << rel) & 0x0fffffdfu) != 0u)
			continue;
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x600u; slot < 0x608u; slot++) {
		if (slot - 0x600u > 4u)
			setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x700u; slot < 0x800u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	memset(zeroSlot, 0, sizeof(zeroSlot));
	if ((REG32(0xe0062180u) & 0x100u) == 0u) {
		writeKeyringSlot32(0x517u, zeroSlot);
		writeKeyringSlot32(0x519u, zeroSlot);
	}
	writeKeyringSlot32(0x516u, zeroSlot);

	for (slot = 0x500u; slot < 0x516u; slot++) {
		uint32_t rel = slot - 0x501u;
		uint32_t status;

		REG32(0xe0030028u) = slot & 0xffffu;
		status = REG32(0xe003002cu);
		if (rel <= 20u) {
			uint32_t bit = 1u << rel;

			if ((bit & 0x001cffeeu) != 0u) {
				if (status != 0x18000003u)
					return (int)0x800f032f;
			} else if ((bit & 1u) != 0u) {
				if (status != 0x10000007u)
					return (int)0x800f032f;
			}
		}
	}

	return lockKeyringSlot50eIfReady();
}

/* 0x801d36 */
void wipeAndLockAllKeyringSlots(void)
{
	uint32_t zeroSlot[8];
	uint32_t slot;

	memset(zeroSlot, 0, sizeof(zeroSlot));

	for (slot = 0u; slot < 0x80u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x100u; slot < 0x180u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x200u; slot < 0x218u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x300u; slot < 0x400u; slot++)
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);

	for (slot = 0x400u; slot < 0x480u; slot++) {
		writeKeyringSlot32((uint16_t)slot, zeroSlot);
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x500u; slot < 0x580u; slot++) {
		writeKeyringSlot32((uint16_t)slot, zeroSlot);
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x600u; slot < 0x608u; slot++) {
		writeKeyringSlot32((uint16_t)slot, zeroSlot);
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}

	for (slot = 0x700u; slot < 0x800u; slot++) {
		writeKeyringSlot32((uint16_t)slot, zeroSlot);
		setKeyringSlotProtection((uint16_t)slot, 0x1fff0000u);
	}
}

/* 0x801e1c */
int isAddrInCmepSram(uint32_t addr)
{
	if (addr < 0x00800000u)
		return 0;
	return (addr < 0x00800000u + 0x00020000u);
}

/* 0x801e36 */
int isAddrInSkSram(uint32_t addr)
{
	if (addr < 0x00800000u)
		return 0;
	return (addr < 0x00800000u + 0x0000a000u);
}

/* 0x801e50 */
int isAddrInSmSram(uint32_t addr)
{
	if (addr < 0x0080a000u)
		return 0;
	return (addr < 0x0080a000u + 0x00016000u);
}

/* 0x801e6a */
int isAddrInTzDram(uint32_t addr)
{
	if (addr < 0x40000000u)
		return 0;
	return (addr < 0x40200000u);
}

/* 0x801e7e */
int isAddrInNonSecureDram(uint32_t addr)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} dramSize;
	uint32_t dramEnd;

	dramSize.b[0] = REG8(0xe0062260u);
	dramSize.b[1] = REG8(0xe0062261u);
	dramSize.b[2] = REG8(0xe0062262u);
	dramSize.b[3] = REG8(0xe0062263u);

	if (addr <= 0x401fffffu)
		return 0;

	dramEnd = dramSize.w + 0x40000000u;
	if (dramEnd < dramSize.w || addr < dramEnd)
		return 1;
	return 0;
}

/* 0x801ee2 */
int checkMemoryRangeWithValidator(uint32_t addr, uint32_t size,
		uint32_t panicCode, int (*validator)(uint32_t))
{
	uint32_t endAddr;

	if (size != 0u) {
		endAddr = addr + size - 1u;
		if (endAddr < addr) {
			setPanicArmInterruptCode(0x00400000u);
			skPanic("%08x\n", panicCode);
		}
	} else {
		endAddr = addr;
	}

	if (validator(addr) == 0)
		return 0;
	return (validator(endAddr) != 0);
}

/* 0x801f3a */
void setArmCommBufferAddr(uint32_t addr)
{
	s_armCommBufferAddr = addr;
}

/* 0x801f40 */
uint32_t getArmCommBufferAddr(void)
{
	return s_armCommBufferAddr;
}

/* 0x801f46 */
void setSmReturnStatusAddr(uint32_t addr)
{
	s_smReturnStatusAddr = addr;
}

/* 0x801f4c */
uint32_t getSmReturnStatusAddr(void)
{
	return s_smReturnStatusAddr;
}

/* 0x801f52 */
uint32_t getSmSramBase(void)
{
	return 0x0080a000u;
}

/* 0x801f58 */
uint32_t getSmSramEnd(void)
{
	return 0x0080a000u + 0x00016000u;
}

/* 0x801f64 */
uint32_t getSmSramSize(void)
{
	return 0x00016000u;
}

/* 0x801f6a */
int checkRangeInCmepSram(uint32_t addr, uint32_t size)
{
	return checkMemoryRangeWithValidator(addr, size, 10u, isAddrInCmepSram);
}

/* 0x801f74 */
int checkRangeInSkSram(uint32_t addr, uint32_t size)
{
	return checkMemoryRangeWithValidator(addr, size, 11u, isAddrInSkSram);
}

/* 0x801f7e */
int checkRangeInSmSram(uint32_t addr, uint32_t size)
{
	return checkMemoryRangeWithValidator(addr, size, 12u, isAddrInSmSram);
}

/* 0x801f88 */
int checkRangeInTzDram(uint32_t addr, uint32_t size)
{
	return checkMemoryRangeWithValidator(addr, size, 30u, isAddrInTzDram);
}

/* 0x801f92 */
int checkRangeInNonSecureDram(uint32_t addr, uint32_t size)
{
	return checkMemoryRangeWithValidator(addr, size, 31u, isAddrInNonSecureDram);
}

/* 0x801f9c */
int checkRangeInAnyDram(uint32_t addr, uint32_t size)
{
	if (checkMemoryRangeWithValidator(addr, size, 30u, isAddrInTzDram) != 0)
		return 1;
	return (checkMemoryRangeWithValidator(addr, size, 31u, isAddrInNonSecureDram) != 0);
}

/* 0x801fd4 */
void setDebugLogBufferAddr(uint32_t addr)
{
	s_debugLogBufferAddr = addr;
}

/* 0x801fda */
uint32_t getDebugLogBufferAddr(void)
{
	return s_debugLogBufferAddr;
}

/* 0x801fe0 */
void setDebugLogBufferSize(uint32_t size)
{
	s_debugLogBufferSize = size;
}

/* 0x801fe6 */
uint32_t getDebugLogBufferSize(void)
{
	return s_debugLogBufferSize;
}

/* 0x801fec */
uint32_t getSkSrvkSramBase(void)
{
	return 0x00808ff0u;
}

/* 0x801ff2 */
uint32_t getSkSrvkSramSize(void)
{
	return 0x00001000u;
}

/* 0x801ff8 */
void setSrvkListAddr(uint32_t addr)
{
	s_srvkListAddr = addr;
}

/* 0x801ffe */
uint32_t getSrvkListAddr(void)
{
	return s_srvkListAddr;
}

/* 0x802004 */
void setSrvkListSize(uint32_t size)
{
	s_srvkListSize = size;
}

/* 0x80200a */
uint32_t getSrvkListSize(void)
{
	return s_srvkListSize;
}

/* 0x802010 */
void setSkState(uint32_t state)
{
	s_skState = state;
}

/* 0x802016 */
uint32_t getSkState(void)
{
	return s_skState;
}

/* 0x80201c */
int isCommandAllowedInCurrentState(uint32_t cmdIndex)
{
	static const uint32_t s_allowedCmdMaskByState[11] = {
		0x000u, 0x000u, 0x002u, 0x78eu, 0x002u, 0x072u,
		0x042u, 0x002u, 0x002u, 0x000u, 0x000u,
	};

	return (((1u << cmdIndex) & s_allowedCmdMaskByState[s_skState]) != 0u);
}
