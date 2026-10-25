/*
 * The microcontroller's registers that the build's device file does not describe. The build
 * compiles for the uPD78F1030 (hw/<id>.mk), whose device file names the registers of Sony's
 * D79F0109 but has no external interrupt 13: its edge bits, EGP1.5 and EGN1.5, are
 * read-only there, and so are ports 3 and 9 and their mode registers. They are written through a
 * bit-field at their addresses (the same CLR1/SET1 on the sfr).
 */
#ifndef ERNIE_CHIP_H
#define ERNIE_CHIP_H

struct SfrBits {
	unsigned char	b0:1;
	unsigned char	b1:1;
	unsigned char	b2:1;
	unsigned char	b3:1;
	unsigned char	b4:1;
	unsigned char	b5:1;
	unsigned char	b6:1;
	unsigned char	b7:1;
};

#define SFR_BITS(address)	(*(volatile __near struct SfrBits *)(address))

/* INTP13's rising and falling edge enables (EGP1.5, EGN1.5) */
#define EGP13			SFR_BITS(0xfff3a).b5
#define EGN13			SFR_BITS(0xfff3b).b5

/* port 3's bit 6 (P3.6) and its mode (PM3.6) */
#define P36			SFR_BITS(0xfff03).b6
#define PM36			SFR_BITS(0xfff23).b6

/* port 9's bit 0 (P9.0) and its mode (PM9.0) */
#define P90			SFR_BITS(0xfff09).b0
#define PM90			SFR_BITS(0xfff29).b0

/* port 9's bit 7 (P9.7): the analog sticks' power */
#define P97			SFR_BITS(0xfff09).b7

#endif
