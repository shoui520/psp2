/*
 * The microcontroller's registers that the build's device file does not describe. The build
 * compiles for the uPD78F1030 (hw/<id>.mk), whose device file names the registers of Sony's
 * D79F0109 but has no external interrupt 13: its edge bits, EGP1.5 and EGN1.5, are
 * read-only there, and so are ports 3 and 9, their mode registers and PU9. They are written through a
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

/* port 3's bit 3 (P3.3) */
#define P33			SFR_BITS(0xfff03).b3

/* port 3's bit 1 (P3.1) */
#define P31			SFR_BITS(0xfff03).b1

/* port 3's bit 4 (P3.4) and its mode (PM3.4), and bit 7 (P3.7): the 3G modem's lines (bbmc.c) */
#define P34			SFR_BITS(0xfff03).b4
#define PM34			SFR_BITS(0xfff23).b4
#define P37			SFR_BITS(0xfff03).b7

/* port 3's bit 6 (P3.6) and its mode (PM3.6) */
#define P36			SFR_BITS(0xfff03).b6
#define PM36			SFR_BITS(0xfff23).b6

/* port 4's bit 0 (P4.0): the updater toggles it every 4 ms (tm00Interrupt) */
#define P40			SFR_BITS(0xfff04).b0

/* port 9's bit 0 (P9.0) and its mode (PM9.0) */
#define P90			SFR_BITS(0xfff09).b0
#define PM90			SFR_BITS(0xfff29).b0

/* port 9's bits 3 and 4 (RxD4, TxD4), their modes and bit 3's pull-up (PU9.3) */
#define P94			SFR_BITS(0xfff09).b4
#define PM93			SFR_BITS(0xfff29).b3
#define PM94			SFR_BITS(0xfff29).b4
#define PU93			SFR_BITS(0xf0039).b3

/* port 9's bit 5 (P9.5): on while the system is on (powerseq.c) */
#define P95			SFR_BITS(0xfff09).b5

/* port 9's bits 1 and 2 (P9.1, P9.2): low while the host is off */
#define P91			SFR_BITS(0xfff09).b1
#define P92			SFR_BITS(0xfff09).b2

/* port 9's bit 6 (P9.6): the battery's bootloader line (0x08b0) */
#define P96			SFR_BITS(0xfff09).b6

/* port 9's bit 7 (P9.7): the analog sticks' power */
#define P97			SFR_BITS(0xfff09).b7

/* port 12's bit 5 (P12.5): the 3G modem's power in mode 1 (bbmc.c) */
#define P125			SFR_BITS(0xfff0c).b5

#endif
