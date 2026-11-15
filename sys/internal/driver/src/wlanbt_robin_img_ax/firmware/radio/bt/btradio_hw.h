/*
 * The Bluetooth radio controller's 8051 core: its special function registers
 * beyond the standard ones, and the radio registers in XDATA. Nothing names
 * them; they are named by address (and, where the program shows it, by use).
 */
#ifndef BTRADIO_HW_H
#define BTRADIO_HW_H

#include <8051.h>
#include <stdint.h>

__sfr __at(0x86) SFR_WDTREL;
__sfr __at(0x8e) SFR_8E;
__sfr __at(0xc0) IRCON;
__sfr __at(0xc1) STATE;		/* the program's state number: SFR 0xC1 */
__sfr __at(0xc2) CCL1;		/* the compare value the calibration drives */
__sfr __at(0xc8) T2CON;
__sfr __at(0xda) PORT_INDEX;	/* the channel index the host writes (read as a variable) */
__sfr __at(0xde) CTRELL;
__sfr __at(0xdf) CTRELH;
__sfr __at(0xe2) CML3;
__sfr __at(0xe3) CMH3;
__sfr __at(0xe4) CML4;
__sfr __at(0xe5) CMH4;
__sfr __at(0xe6) CML5;
__sfr __at(0xe7) CMH5;
__sfr __at(0xe8) P4;
__sfr __at(0xe9) MD0;
__sfr __at(0xea) MD1;
__sfr __at(0xeb) MD2;
__sfr __at(0xec) MD3;
__sfr __at(0xed) MD4;
__sfr __at(0xee) MD5;
__sfr __at(0xef) ARCON;
__sfr __at(0xf2) CML6;
__sfr __at(0xf3) CMH6;
__sfr __at(0xf4) CML7;
__sfr __at(0xf5) CMH7;
__sfr __at(0xf6) CMEN;
__sfr __at(0xf8) P5;
__sfr __at(0xf9) SFR_F9;
__sfr __at(0xfa) P6;
__sfr __at(0xfb) SFR_FB;
__sfr __at(0xfc) SFR_FC;
__sfr __at(0xfd) SFR_FD;
__sfr __at(0xfe) SFR_FE;
__sfr __at(0xff) SFR_FF;

__sbit __at(0xc0) IRCON_0;
__sbit __at(0xc1) IRCON_1;
__sbit __at(0xc2) IRCON_2;
__sbit __at(0xc3) IRCON_3;
__sbit __at(0xc4) IRCON_4;
__sbit __at(0xc5) IRCON_5;
__sbit __at(0xc6) IRCON_6;
__sbit __at(0xc7) IRCON_7;
__sbit __at(0xc8) T2CON_0;
__sbit __at(0xc9) T2CON_1;
__sbit __at(0xca) T2CON_2;
__sbit __at(0xcb) T2CON_3;
__sbit __at(0xcc) T2CON_4;
__sbit __at(0xcd) T2CON_5;
__sbit __at(0xce) T2CON_6;
__sbit __at(0xcf) T2CON_7;
__sbit __at(0xea) P4_2;
__sbit __at(0xeb) P4_3;
__sbit __at(0xed) P4_5;
__sbit __at(0xee) P4_6;
__sbit __at(0xef) P4_7;
__sbit __at(0xfb) P5_3;
__sbit __at(0xfc) P5_4;
__sbit __at(0xfd) P5_5;
__sbit __at(0xff) P5_7;

#define RADIO_REG(addr)	(*(volatile __xdata uint8_t *)(addr))

#endif /* BTRADIO_HW_H */
