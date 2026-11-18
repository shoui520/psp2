/*
 * Build selection: the board (hardware ID, as in KBL Param's hardware info), the board whose
 * code it runs and the firmware version, given by the Makefile (-DERNIE_HW=..., -DERNIE_BOARD=...,
 * -DERNIE_FW=...).
 */
#ifndef ERNIE_H
#define ERNIE_H

#ifndef ERNIE_HW
#error "ERNIE_HW (hardware ID) not set"
#endif
#ifndef ERNIE_FW
#error "ERNIE_FW (firmware version) not set"
#endif

/* the board whose code the build is: the hardware ID, or the board another one's image is
   built from (hw/<id>.mk's BOARD: the DEX boards 0x00405000 and 0x00405200 run PCH-1000's,
   differing only in their ID and nvsHostOffsetMin) */
#ifndef ERNIE_BOARD
#define ERNIE_BOARD		ERNIE_HW
#endif
#define BOARD_IS(id)		(ERNIE_BOARD == (id))

/* the board whose kernel the build runs: the code's board's, but where hw/<id>.mk's KERNEL says
   otherwise (0x00315200: 0x00411000's code on DEM-3000L's kernel) */
#ifndef ERNIE_KERNEL
#define ERNIE_KERNEL		ERNIE_BOARD
#endif

/* board family: the hardware ID's top byte (0x00 1x/3x prototypes, 0x004 IRT/IRS, 0x006
 * IRS-1001, 0x007 DOL (PS TV), 0x008 USS (PCH-2000)) */
#define HW_FAMILY		(ERNIE_HW >> 20)
#define HW_IS(id)		(ERNIE_HW == (id))
#define FW_SINCE(version)	(ERNIE_FW >= (version))
/* the chip: the slim models' RL78/G13 (0x008), else the 78K0R/KG3-L */
#define CHIP_RL78		(HW_FAMILY == 0x008)

/* the retail boards' 0x010001xx (PCH-1000 0x00406000, PDEL 0x00416000): built from a source
   of their own, between 0x00090903's and the DEM boards' 0x010001xx (no transition 0x5b, a
   transition's result read as a byte; PCH-1000: a 60-tick AC delay after an off or a suspend), by the
   0x00090903 compiler */
#define FW_RETAIL_10X		(FW_SINCE(0x01000104) && !FW_SINCE(0x01000306) && \
				 (BOARD_IS(0x00406000) || HW_IS(0x00416000)))

/* tables 0x00090804 keeps in initialised RAM (@@INITL, copied by cstart) and later versions
   in the constants */
#if FW_SINCE(0x00090903)
#define TABLE_CONST		const
#else
#define TABLE_CONST
#endif

/*
 * a far pointer moved by n bytes over all its 24 bits: Sony's compiler adds to a far pointer
 * as a long (@@luadd), the build's CA78K0R V1.72 to its low 16 bits only
 */
#define FAR_ADD(p, n)		((void *)((unsigned long)(p) + (n)))

#include "rl78.h"

#endif
