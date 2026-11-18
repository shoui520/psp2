/*
 * The slim models' (PCH-2000) syscon, an RL78/G13 (R5F1ZCRK: CA78K0R's R5F100SK device file),
 * names some of the 78K0R/KG3-L's registers otherwise: the same registers at the same
 * addresses, the RL78 device file's names for the code's 78K0R names.
 */
#ifndef RL78_H
#define RL78_H

#if CHIP_RL78

#include "chip.h"

/* the IICA: unit 0 */
#define IICA		IICA0
#define IICS		IICS0
#define IICF		IICF0
#define IICCTL0		IICCTL00
#define IICCTL1		IICCTL01
#define IICWL		IICWL0
#define IICWH		IICWH0
#define SVA		SVA0
#define IICE		IICE0
#define LREL		LREL0
#define WREL		WREL0
#define SPIE		SPIE0
#define WTIM		WTIM0
#define ACKE		ACKE0
#define STT		STT0
#define SPT		SPT0
#define MSTS		MSTS0
#define ALD		ALD0
#define EXC		EXC0
#define COI		COI0
#define TRC		TRC0
#define ACKD		ACKD0
#define STD		STD0
#define SPD		SPD0
#define STCF		STCF0
#define IICBSY		IICBSY0
#define STCEN		STCEN0
#define IICRSV		IICRSV0
#define IICAEN		IICA0EN

/* the A/D converter: unit 0 */
#define ADM		ADM0

/* peripheral enable register 1 (0xf007a), which the R5F100SK's device file leaves out (the
   RL78's other device files name it); the slim's startup clears it */
#define PER1		(*(volatile unsigned char __near *)0xf007a)
#define DTCEN		SFR_BITS(0xf007a).b3	/* PER1.3: the data transfer controller */

/* the data transfer controller's registers, which that device file leaves out too: the vector
   table's page and the activation enables, eight sources each */
#define DTCBAR		(*(volatile unsigned char __near *)0xf02e0)
#define DTCEN0		(*(volatile unsigned char __near *)0xf02e8)
#define DTCEN1		(*(volatile unsigned char __near *)0xf02e9)
#define DTCEN2		(*(volatile unsigned char __near *)0xf02ea)
#define DTCEN3		(*(volatile unsigned char __near *)0xf02eb)
#define DTCEN4		(*(volatile unsigned char __near *)0xf02ec)

/*
 * Sony's R5F1ZCRK has TM00's interrupt at vector 0x20, where the R5F100SK's device file has
 * CSI01's: from there to IICA0 (0x2c) each interrupt is one vector up, and its request, mask and
 * priority bits one bit up (IF0H.6 to IF1L.4). Those bits are written at Sony's places.
 */
#undef TMIF00
#undef TMMK00
#undef CSIIF01
#undef CSIMK01
#undef IICAIF
#undef IICAMK
#define TMIF00		IF0H.6
#define TMMK00		MK0H.6
#define CSIIF01		IF0H.7
#define CSIMK01		MK0H.7
#define IICAIF		IF1L.4
#define IICAMK		MK1L.4
#define IICAPR0		SFR_BITS(0xfffea).b4
#define IICAPR1		SFR_BITS(0xfffee).b4

/*
 * Sony's chip has a third serial array unit (SAU2), at 0xf0280 in the RL78's layout, which the
 * device file leaves out: here under the 78K0R/KG3-L's names for its SAU2 (uart4.c's), at the
 * slim's addresses. PER1.1 powers it; its interrupts are IF3L/MK3L bits 5 (send), 6 (receive)
 * and 7 (error), at vectors 0x6e, 0x70 and 0x72.
 */
#define SAU2EN		SFR_BITS(0xf007a).b1
#define SSR20		(*(volatile unsigned int __near *)0xf0280)
#define SSR21		(*(volatile unsigned int __near *)0xf0282)
#define SIR20L		(*(volatile unsigned char __near *)0xf0288)
#define SIR21		(*(volatile unsigned int __near *)0xf028a)
#define SIR21L		(*(volatile unsigned char __near *)0xf028a)
#define SMR20		(*(volatile unsigned int __near *)0xf0290)
#define SMR21		(*(volatile unsigned int __near *)0xf0292)
#define SCR20		(*(volatile unsigned int __near *)0xf0298)
#define SCR21		(*(volatile unsigned int __near *)0xf029a)
#define SS20		SFR_BITS(0xf02a2).b0
#define SS21		SFR_BITS(0xf02a2).b1
#define ST2L		(*(volatile unsigned char __near *)0xf02a4)
#define SPS2L		(*(volatile unsigned char __near *)0xf02a6)
#define SO2		(*(volatile unsigned int __near *)0xf02a8)
#define SOE20		SFR_BITS(0xf02aa).b0
#define SOL2		(*(volatile unsigned int __near *)0xf02b4)
#define SDR20		(*(volatile unsigned int __near *)0xfff78)
#define SDR21		(*(volatile unsigned int __near *)0xfff7a)
#define STIF4		SFR_BITS(0xfffd2).b5
#define SRIF4		SFR_BITS(0xfffd2).b6
#define SREIF4		SFR_BITS(0xfffd2).b7
#define STMK4		SFR_BITS(0xfffd6).b5
#define SRMK4		SFR_BITS(0xfffd6).b6
#define SREMK4		SFR_BITS(0xfffd6).b7
#undef SRPR04
#undef SRPR14
#undef SREPR04
#undef SREPR14
#define SRPR04		SFR_BITS(0xfffda).b6	/* PR03L.6, PR13L.6 */
#define SRPR14		SFR_BITS(0xfffde).b6
#define SREPR04		SFR_BITS(0xfffda).b7	/* PR03L.7, PR13L.7 */
#define SREPR14		SFR_BITS(0xfffde).b7

/*
 * A register of Sony's chip at 0xf0270, outside the RL78's: USS-1001's serial lines on the
 * multi-connector (app/serialline.c) are written to it as they are connected.
 */
#define SERIALSEL	(*(volatile unsigned char __near *)0xf0270)

#endif	/* CHIP_RL78 */

#endif
