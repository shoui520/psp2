/*
 * The 88W8787's radio controllers: 8051 cores that run the small programs the
 * ARM firmware loads through the serial window at 0x8000 (the WLAN radio's from
 * wlanPacketBufferPoolMemory, the Bluetooth radio's from btPoolHeaderMemory).
 *
 * The standard 8051 registers come from SDCC's <8051.h>. The core's other
 * special function registers and the radio registers in XDATA are Marvell's and
 * no document or record names them, so they are named by their address here.
 */
#ifndef RADIO8051_H
#define RADIO8051_H

#include <8051.h>
#include <stdint.h>

/* Special function registers outside the 8051's own. */
__sfr __at(0x86) SFR_86;
__sfr __at(0x8e) SFR_8E;
__sfr __at(0xc0) SFR_C0;
__sbit __at(0xc0) SFR_C0_0;
__sfr __at(0xc8) SFR_C8;
__sbit __at(0xc8) SFR_C8_0;
__sbit __at(0xc9) SFR_C8_1;
__sfr __at(0xd9) SFR_D9;
__sfr __at(0xda) SFR_DA;
__sfr __at(0xdb) SFR_DB;
__sfr __at(0xdc) SFR_DC;
__sfr __at(0xdd) SFR_DD;
__sfr __at(0xde) SFR_DE;
__sfr __at(0xe3) SFR_E3;
__sfr __at(0xe4) SFR_E4;
__sfr __at(0xe6) SFR_E6;
__sfr __at(0xe8) SFR_E8;
__sfr __at(0xe9) SFR_E9;
__sfr __at(0xea) SFR_EA;
__sfr __at(0xeb) SFR_EB;
__sfr __at(0xec) SFR_EC;
__sfr __at(0xed) SFR_ED;
__sfr __at(0xee) SFR_EE;
__sfr __at(0xf1) SFR_F1;
__sfr __at(0xf2) SFR_F2;
__sfr __at(0xf3) SFR_F3;
__sfr __at(0xf4) SFR_F4;
__sfr __at(0xf5) SFR_F5;

/* A radio register in XDATA. */
#define RADIO_REG(addr)	(*(volatile __xdata uint8_t *)(addr))

#endif /* RADIO8051_H */
