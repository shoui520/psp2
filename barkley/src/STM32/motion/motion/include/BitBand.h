/*
 * The Cortex-M3's bit-band: a word in the alias region for each bit of the
 * peripherals' (and the SRAM's) first megabyte, a bit read and written alone.
 */
#ifndef BITBAND_H
#define BITBAND_H

#include "stm32f10x_type.h"

/* the alias of bit bit of the word at addr */
#define BITBAND(addr, bit) \
	((vu32 *)(((u32)(addr) & 0xF0000000) + ((u32)(addr) << 5) + ((bit) << 2) + 0x02000000))

inline vu32 *BitBand(u32 addr, u32 bit)
{
	return BITBAND(addr, bit);
}

#endif
