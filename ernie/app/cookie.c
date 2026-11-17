/*
 * The Cookie: its registers over the I2C bus. Registers 3-8 are kept as last written, so a
 * setter writes only a change; register 3's low byte is read back first, as the Cookie
 * clears its bit 7 itself.
 */
#pragma sfr

#include <stddef.h>

#include "ernie.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "iica.h"
#include "kernel.h"
#include "powerreq.h"
#include "powerseq.h"

static struct IicaRequest cookieIica;
#if FW_SINCE(0x00090804)
union CookieReg cookieRegs[6];
#else
union CookieReg cookieRegs[7];			/* 0x00090206: register 6 too, set only here */
#endif

void cookieInit(void)
{
	cookieRegs[0].word = 0x0014;
	cookieRegs[1].word = 0x0050;
	cookieRegs[2].word = 0x00c0;
	cookieRegs[3].word = 0x0010;
	cookieRegs[4].word = 0x0020;
	cookieRegs[5].word = 0x0010;
#if !FW_SINCE(0x00090804)
	cookieRegs[6].word = 0xa020;
#endif
}

/* data[0] is the register */
unsigned char cookieWrite(unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;
	unsigned char i;

	cookieIica.rxData = NULL;
	cookieIica.rxCount = 0;
	cookieIica.txData = data;
	cookieIica.txCount = length;
	cookieIica.address = COOKIE_ADDRESS;
	cookieIica.status = &status;
	for (i = 1; i < length; i++)
		;
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
	iicaStart(&cookieIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

/* a register's two bytes */
unsigned char cookieRead(unsigned char *reg, unsigned char *data)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	cookieIica.rxData = data;
	cookieIica.rxCount = 2;
	cookieIica.txData = reg;
	cookieIica.txCount = 1;
	cookieIica.address = COOKIE_ADDRESS;
	cookieIica.status = &status;
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
	iicaStart(&cookieIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
	return status;
}

/* register 0 */
unsigned char cookieReadVersion(unsigned char *version)
{
	unsigned char reg;

	reg = COOKIE_REG(0);
	return cookieRead(&reg, version);
}

/* register 4's low byte: 0x08c2 (sceSysconCtrlChargingCurrent) and the temperature's range */
unsigned char cookieCtrlChargingCurrent(unsigned char current)
{
	unsigned char result;

	if (current != cookieRegs[1].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(4);
		elmoCommand[1] = current;
		elmoCommand[2] = cookieRegs[1].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[1].byte[0] = current;
	} else
		result = 0;
	return result;
}

/* register 8's low byte: the temperature's range */
unsigned char cookieCtrlReg8(unsigned char value)
{
	unsigned char result;

	if (value != cookieRegs[5].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(8);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[5].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[5].byte[0] = value;
	} else
		result = 0;
	return result;
}

/* register 3's bit 15 clear while on: 0x08c1 (sceSysconCtrlBatteryCharge) */
unsigned char cookieCtrlBatteryCharge(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	cookieReadReg3();
	if (on)
		value = cookieRegs[0].byte[1] & 0x7f;
	else
		value = cookieRegs[0].byte[1] | 0x80;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = cookieRegs[0].byte[0];
		elmoCommand[2] = value;
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[1] = value;
	} else
		result = 0;
	return result;
}

#if FW_SINCE(0x00090804)
/* register 3's bit 7 (powerCookieSet, powerCookieClear), then the AC's charge */
unsigned char cookieCtrlCharge(unsigned char on)
{
	unsigned char result;

	if (on)
		powerCookieClear();
	else
		powerCookieSet();
	result = cookieCtrlChargeAc(on);
	return result;
}
#else
/* 0x00090206: register 3's bits 6 and 7 clear while on and nothing holds them */
unsigned char cookieCtrlCharge(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	if (on) {
#if FW_SINCE(0x00090206)
		if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1)
			value = cookieRegs[0].byte[0] & 0x3f;
		else {
			result = 0;
			goto done;
		}
#else
		value = cookieRegs[0].byte[0] & 0x3f;	/* 0x00090009: no lock */
#endif
	} else
		value = cookieRegs[0].byte[0] | 0xc0;
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
#if FW_SINCE(0x00090206)
done:
#endif
	return result;
}
#endif

/* register 6's low byte, at most chargeCookieSetting (0x00090804: as given): 0x08c4 (sceSysconCtrlSupplyCurrentUsb) */
unsigned char cookieCtrlSupplyCurrentUsb(unsigned char current)
{
	unsigned char result;

#if FW_SINCE(0x00090903)
	if (chargeCookieSetting < current)
		current = chargeCookieSetting;
#endif
	elmoCommand[0] = COOKIE_CTRL_REG(6);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[3].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[3].byte[0] = current;
	return result;
}

/* register 5's low byte, halved on a Cookie before 0x31 and at most chargeCookieSetting
   (0x00090804: as given): 0x08c3 (sceSysconCtrlSupplyCurrentAc) */
unsigned char cookieCtrlSupplyCurrentAc(unsigned char current)
{
	unsigned char result;

#if FW_SINCE(0x00090903)
	if (deviceVersions.cookie[1] < 0x31)
		current >>= 1;
	if (chargeCookieSetting < current)
		current = chargeCookieSetting;
#endif
	elmoCommand[0] = COOKIE_CTRL_REG(5);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[2].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[2].byte[0] = current;
	return result;
}

#if FW_SINCE(0x00090206)
/* register 7's low byte, its bits 5-7 */
unsigned char cookieCtrlReg7(unsigned char value)
{
	unsigned char result;

	value &= 0xe0;
	elmoCommand[0] = COOKIE_CTRL_REG(7);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[4].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[4].byte[0] = value;
	return result;
}

unsigned char cookieReadStatus1(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_CTRL_REG(1);
	return cookieRead(&reg, status);
}

unsigned char cookieReadStatus2(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_CTRL_REG(2);
	return cookieRead(&reg, status);
}

#endif

#if FW_SINCE(0x01000109)
/* register 3 read back: the Cookie cleared bit 7, so the USB's mode is new */
void cookieReadReg3(void)
{
	unsigned char reg;
	unsigned char was;

	was = cookieRegs[0].byte[0] & 0x80;
	reg = COOKIE_REG(3);
	cookieRead(&reg, cookieRegs[0].byte);
	if (!(cookieRegs[0].byte[0] & 0x80) && was) {
		chargeUsbMode = 0;
		chargeUsbHold = 0;
	}
}
#endif

#if FW_SINCE(0x00090206)
/* register 3's bit 6 clear while on and nothing holds it: 0x0892 (sceSysconCtrlChargeAC).
   Sony's compares the new value with the register's high byte, not the low one it sets. */
unsigned char cookieCtrlChargeAc(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	cookieReadReg3();
	if (on) {
#if FW_SINCE(0x01000107)
		if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 &&
		    !chargeCookieHeld && chargeControl.1)
#elif FW_SINCE(0x00090804)
		if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 &&
		    chargeControl.1)
#else
		if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1)
#endif
			value = cookieRegs[0].byte[0] & 0xbf;
		else {
			result = 0;
			goto done;
		}
	} else
		value = cookieRegs[0].byte[0] | 0x40;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
done:
	return result;
}

#endif

/* register 3's bit 2 clear in mode 1 */
unsigned char cookieCtrlReg3Mode(unsigned char mode)
{
	unsigned char result;
	unsigned char value;

#if FW_SINCE(0x01000104)
	cookieReadReg3();
	if (mode == 1)
		value = cookieRegs[0].byte[0] & 0xfb;
	else
		value = cookieRegs[0].byte[0] | 0x04;
#else
	/* before 0x01000104 the register is not read first, and chargeLedOff keeps bit 2 */
	if (mode == 1)
		value = cookieRegs[0].byte[0] & 0xfb;
	else {
#if FW_SINCE(0x00090804)
		if (configGet()->start.bits.chargeLedOff) {
			result = 0;
			goto done;
		}
#endif
		value = cookieRegs[0].byte[0] | 0x04;
	}
#endif
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
#if !FW_SINCE(0x01000104) && FW_SINCE(0x00090804)
done:
#endif
	return result;
}

/* register 3's bit 2 set while on, else bits 0 and 2 clear */
unsigned char cookieCtrlReg3Bit2(unsigned char on)
{
	unsigned char result;
	unsigned char value;

#if FW_SINCE(0x01000104)
	cookieReadReg3();
	if (on)
		value = cookieRegs[0].byte[0] | 0x04;
	else
		value = cookieRegs[0].byte[0] & 0xfa;
#else
	/* before 0x01000104 bit 0, not read first, and kept clear by chargeLedOff */
	if (on) {
#if FW_SINCE(0x00090804)
		if (configGet()->start.bits.chargeLedOff) {
			result = 0;
			goto done;
		}
#endif
		value = cookieRegs[0].byte[0] | 0x01;
	} else
		value = cookieRegs[0].byte[0] & 0xfe;
#endif
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
#if !FW_SINCE(0x01000104) && FW_SINCE(0x00090804)
done:
#endif
	return result;
}

#if !FW_SINCE(0x00090206)	/* 0x00090009's order */
/* register 7's low byte, its bits 5-7 */
unsigned char cookieCtrlReg7(unsigned char value)
{
	unsigned char result;

	value &= 0xe0;
	elmoCommand[0] = COOKIE_CTRL_REG(7);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[4].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[4].byte[0] = value;
	return result;
}

/* 0x00090009: register 3's bit 6 clear while on, no lock and no read first */
unsigned char cookieCtrlChargeAc(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	if (on)
		value = cookieRegs[0].byte[0] & 0xbf;
	else
		value = cookieRegs[0].byte[0] | 0x40;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
}

unsigned char cookieReadStatus1(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_CTRL_REG(1);
	return cookieRead(&reg, status);
}

unsigned char cookieReadStatus2(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_CTRL_REG(2);
	return cookieRead(&reg, status);
}

#endif

#if FW_SINCE(0x00090804)	/* led.c's */
/* register 3's bit 0 set and bit 2 clear */
unsigned char cookieCtrlReg3Bit0(void)
{
	unsigned char value;
	unsigned char result;

	cookieReadReg3();
	value = (cookieRegs[0].byte[0] & 0xfb) | 0x01;
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_CTRL_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
}
#endif
