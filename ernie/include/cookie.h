/*
 * The Cookie (app/cookie.c), the power management chip on the I2C bus at 0xa8 (0xa4 where the
 * settings move its registers): its status registers 1 and 2, and registers 3-8, kept here as
 * last written.
 */
#ifndef ERNIE_COOKIE_H
#define ERNIE_COOKIE_H

#include "board.h"
#include "ernie.h"

/* a register's number: from 0xa0 where the settings say so (the updater's always from 0) */
#if defined(ERNIE_UPDATER) || !BOARD_COOKIE_REG_HIGH
#define COOKIE_REG(reg)		(reg)
#else
#define COOKIE_REG(reg)		((unsigned char)(configGet()->cookieRegHigh ? (reg) + 0xa0 : (reg)))
#endif

/* a register the controls write and the status reads: 0x00090206 numbers them from 0
   whatever the settings say (only its version read takes the settings' base) */
#if FW_SINCE(0x00090502)
#define COOKIE_CTRL_REG(reg)	COOKIE_REG(reg)
#else
#define COOKIE_CTRL_REG(reg)	(reg)
#endif

/* the Cookie's address on the I2C bus (the updater's: always 0xa8) */
#if BOARD_COOKIE_REG_HIGH
#ifdef ERNIE_UPDATER
#define COOKIE_ADDRESS		0xa8
#else
#define COOKIE_ADDRESS		(configGet()->cookieRegHigh ? 0xa4 : 0xa8)
#endif
#else
#define COOKIE_ADDRESS		0xa8
#endif

/* a register as read and written: byte[0] goes first on the bus */
union CookieReg {
	unsigned int	word;
	unsigned char	byte[2];
};

#if FW_SINCE(0x00090502)
extern union CookieReg cookieRegs[6];	/* registers 3-8 */
#else
extern union CookieReg cookieRegs[7];	/* registers 3-9 */
#endif

extern void cookieInit(void);
extern unsigned char cookieWrite(unsigned char *data, unsigned char length);
extern unsigned char cookieRead(unsigned char *reg, unsigned char *data);
extern unsigned char cookieReadVersion(unsigned char *version);
extern unsigned char cookieCtrlChargingCurrent(unsigned char current);
extern unsigned char cookieCtrlReg8(unsigned char value);
extern unsigned char cookieCtrlBatteryCharge(unsigned char on);
extern unsigned char cookieCtrlCharge(unsigned char on);
extern unsigned char cookieCtrlSupplyCurrentUsb(unsigned char current);
extern unsigned char cookieCtrlSupplyCurrentAc(unsigned char current);
extern unsigned char cookieCtrlReg7(unsigned char value);
extern unsigned char cookieReadStatus1(unsigned char *status);
extern unsigned char cookieReadStatus2(unsigned char *status);
#if FW_SINCE(0x01000109)
extern void cookieReadReg3(void);
#else
/* before 0x01000109 register 3 is not read back: what was last written to it stands */
#define cookieReadReg3()
#endif
extern unsigned char cookieCtrlChargeAc(unsigned char on);
extern unsigned char cookieCtrlReg3Mode(unsigned char mode);
extern unsigned char cookieCtrlReg3Bit2(unsigned char on);
#if SOURCE_0103
/* the slim's COOKIE_REG and COOKIE_ADDRESS as functions (nothing calls them), its registers'
   first values written, register 3's bit 13 */
extern unsigned char cookieReg(unsigned char reg);
extern unsigned int cookieAddress(void);
extern void cookieInitRegs(void);
extern unsigned char cookieCtrlReg3Bit13(unsigned char on);
/* the slim's: the charging current (register 2) and the USB supply current (register 6) set,
   their bits 4-7, and as the host reads them (0x0841, 0x0843) */
extern unsigned char cookieGetChargingCurrent(void);
extern unsigned char cookieGetSupplyCurrentUsb(void);
extern unsigned char cookieChargingCurrentDecode(unsigned char *out, unsigned char reg);
extern unsigned char cookieSupplyCurrentUsbDecode(unsigned char *out, unsigned char reg);
/* and back, 1 for a value it cannot be (0x08c2, 0x08c4) */
extern unsigned char cookieChargingCurrentEncode(unsigned char *reg, unsigned char value);
extern unsigned char cookieSupplyCurrentUsbEncode(unsigned char *reg, unsigned char value);
extern unsigned char cookieReadReg1(unsigned char *reg);
extern unsigned char cookieUpdateReg6Bit3(unsigned char clear);
extern unsigned char cookieCtrlReg6Bit3(unsigned char clear);
/* the AC's supply current, which the slim has not: 0, and 1 for every value */
extern unsigned char cookieGetSupplyCurrent(void);
extern unsigned char cookieSupplyCurrentEncode(unsigned char *reg, unsigned char value);
extern unsigned char cookieSupplyCurrentDecode(unsigned char *out, unsigned char reg);
#endif
extern unsigned char cookieCtrlReg3Bit0(void);
#if SOURCE_0103
/* the slim's: the charge's settings, sent only out of the manual charge mode and when they
   change; register 3 read into value; its bit 12 (the charge's boost) */
extern unsigned char cookieUpdateChargingCurrent(unsigned char current);
extern unsigned char cookieUpdateReg8(unsigned char value);
extern unsigned char cookieReadReg3Value(unsigned char *value);
extern unsigned char cookieCtrlReg3Bit12(unsigned char on);
extern unsigned char cookieUpdateSupplyCurrent(unsigned char current);
extern unsigned char cookieUpdateSupplyCurrentUsb(unsigned char current);
extern unsigned char cookieCtrlSupplyCurrent(unsigned char current);	/* answers 0 */
extern unsigned char cookieUpdateBatteryCharge(unsigned char on);
extern unsigned char cookieSetChargeAc(unsigned char on);		/* answers 0 */
#endif

#endif
