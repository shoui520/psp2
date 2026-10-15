/*
 * The seven DMA controllers, one 0x1000-byte register page each, in pairs:
 * DMAC0-1 at 0xe3000000, DMAC2-3 at 0xe5000000, DMAC4-5 at 0xe0400000 (DMAC5
 * serves the crypto engine) and DMAC6 at 0xe50c0000. SceDmacmgr maps them as
 * SceDmacmgrDmac<n>Reg; the register layout is in its dmacmgr.h. Addresses
 * from those memblocks and the henkaku wiki's Physical Memory page.
 */
#pragma once

#define DMAC0_REG_BASE			0xe3000000
#define DMAC1_REG_BASE			0xe3010000
#define DMAC2_REG_BASE			0xe5000000
#define DMAC3_REG_BASE			0xe5010000
#define DMAC4_REG_BASE			0xe0400000
#define DMAC5_REG_BASE			0xe0410000
#define DMAC6_REG_BASE			0xe50c0000
#define DMAC_REG_SIZE			0x1000
