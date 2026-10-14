/*
 * Build selection: the board (hardware ID, as in KBL Param's hardware info) and the
 * firmware version, both given by the Makefile (-DERNIE_HW=..., -DERNIE_FW=...).
 */
#ifndef ERNIE_H
#define ERNIE_H

#ifndef ERNIE_HW
#error "ERNIE_HW (hardware ID) not set"
#endif
#ifndef ERNIE_FW
#error "ERNIE_FW (firmware version) not set"
#endif

/* board family: the hardware ID's top byte (0x00 1x/3x prototypes, 0x004 IRT/IRS, 0x006
 * IRS-1001, 0x007 DOL (PS TV), 0x008 USS (PCH-2000)) */
#define HW_FAMILY		(ERNIE_HW >> 20)
#define HW_IS(id)		(ERNIE_HW == (id))
#define FW_SINCE(version)	(ERNIE_FW >= (version))

#endif
