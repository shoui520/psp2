/*
 * The Cookie (app/cookie.c), the power management chip on the I2C bus at 0xa8 (0xa4 where the
 * settings move its registers): its status registers 1 and 2, and registers 3-8, kept here as
 * last written.
 */
#ifndef ERNIE_COOKIE_H
#define ERNIE_COOKIE_H

/* a register's number: from 0xa0 where the settings say so */
#define COOKIE_REG(reg)		((unsigned char)(configGet()->cookieRegHigh ? (reg) + 0xa0 : (reg)))

/* a register as read and written: byte[0] goes first on the bus */
union CookieReg {
	unsigned int	word;
	unsigned char	byte[2];
};

extern union CookieReg cookieRegs[6];	/* registers 3-8 */

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
extern void cookieReadReg3(void);
extern unsigned char cookieCtrlChargeAc(unsigned char on);
extern unsigned char cookieCtrlReg3Mode(unsigned char mode);
extern unsigned char cookieCtrlReg3Bit2(unsigned char on);
extern unsigned char cookieCtrlReg3Bit0(void);

#endif
