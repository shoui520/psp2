/*
 * The microcontroller's registers that the build's device file does not describe. The build
 * compiles for the uPD78F1030 (hw/<id>.mk), whose device file names the registers of Sony's
 * D79F0109 but has no external interrupt 13: its edge bits, EGP1.5 and EGN1.5, are
 * read-only there. They are written through a bit-field at their addresses (the same
 * CLR1/SET1 on the sfr).
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

#endif
