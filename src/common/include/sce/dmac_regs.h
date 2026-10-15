/*
 * The seven DMA controllers, one 0x1000-byte register page each, in pairs:
 * DMAC0-1 at 0xe3000000, DMAC2-3 at 0xe5000000, DMAC4-5 at 0xe0400000 (DMAC5
 * serves the crypto engine) and DMAC6 at 0xe50c0000. SceDmacmgr maps them as
 * SceDmacmgrDmac<n>Reg; the register layout is in its dmacmgr.h. Addresses
 * from those memblocks and the henkaku wiki's Physical Memory page.
 */
#pragma once

#include <stddef.h>

#define DMAC0_REG_BASE			0xe3000000
#define DMAC1_REG_BASE			0xe3010000
#define DMAC2_REG_BASE			0xe5000000
#define DMAC3_REG_BASE			0xe5010000
#define DMAC4_REG_BASE			0xe0400000
#define DMAC5_REG_BASE			0xe0410000
#define DMAC6_REG_BASE			0xe50c0000
#define DMAC_REG_SIZE			0x1000

/* DMAC5's key ring (SceSblDMAC5DmacKRBase): 32 key slots of 256 bits that DMAC5
 * operations name by number (the henkaku wiki's DMAC page) */
#define DMAC5_KEYRING_REG_BASE		0xe04e0000
#define DMAC5_KEYRING_SLOTS		32

typedef struct SceDmac5KeyringSlot {
	volatile unsigned int words[8];
} SceDmac5KeyringSlot;

typedef struct SceDmac5KeyringRegisters {
	SceDmac5KeyringSlot slot[DMAC5_KEYRING_SLOTS];
	volatile unsigned int nonSecureAccess;	/* bit n: slot n readable in Non-Secure state */
	unsigned int reserved[0x2ff];
} SceDmac5KeyringRegisters;

_Static_assert(sizeof(SceDmac5KeyringSlot) == 0x20, "keyring slot");
_Static_assert(offsetof(SceDmac5KeyringRegisters, nonSecureAccess) == 0x400, "keyring access");
_Static_assert(sizeof(SceDmac5KeyringRegisters) == DMAC_REG_SIZE, "keyring page");
