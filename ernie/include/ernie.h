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
/* IRT-001: DEM-3000H (0x00315200) and DEM-3000G (0x00315000), one board's code */
#define HW_IRT001		(HW_IS(0x00315200) || HW_IS(0x00315000))
#define FW_SINCE(version)	(ERNIE_FW >= (version))
/* the 0x0103 source: the slim models' firmware (0x01030505 on), which the PS TV's (0x01030603 on,
   on the fat models' chip and wiring) is built from too; board.h's BOARD_USS1001 is the slim's
   board itself (its RL78, pins and touch panels) */
#define SOURCE_0103		FW_SINCE(0x01030000)
/* the 0x0104 source, DOL-1002's (0x01040301) and the slim's 0x0105: touch.c has one function
   for both panels where 0x0103 has one for each, and knows the controllers with T100 */
#define SOURCE_0104		FW_SINCE(0x01040000)
/* the updater built from the 0x0103 source (the PS TV's 0x01030603 on): its application's files
   with the kernel's tasks and timers left out */
#ifdef ERNIE_UPDATER
#define UPD_0103		SOURCE_0103
#else
#define UPD_0103		0
#endif
/* the application's revision a program's files are built from (its version block gives it):
   the updaters of 0x01000306, 0x0100060D, 0x01030511 and 0x0105 carry the application's
   0x00090800, 0x0100010A, 0x01030505 and 0x01050030, every other program its own */
#if (defined(ERNIE_UPDATER)) && ERNIE_FW == 0x01000306
#define APP_FW			0x00090800
#elif (defined(ERNIE_UPDATER)) && ERNIE_FW == 0x0100060D
#define APP_FW			0x0100010A
#elif (defined(ERNIE_UPDATER)) && ERNIE_FW == 0x01030511
#define APP_FW			0x01030505
#elif (defined(ERNIE_UPDATER)) && ERNIE_FW >= 0x01050000	/* the slim's 0x0105 updater */
#define APP_FW			0x01050030
#else
#define APP_FW			ERNIE_FW
#endif
#define APP_SINCE(version)	((version) <= APP_FW)
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
