/*
 * The Cookie, as the updater has it (an older revision of the application's): its registers
 * over the I2C bus at 0xa8, each transfer waited for by polling (iicaTransfer). Registers
 * 3-8 are kept as last written, so a setter writes only a change.
 */
#pragma sfr

#include <stddef.h>

#include "ernie.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "iica.h"
#include "powerseq.h"

static struct IicaRequest cookieIica;
union CookieReg cookieRegs[6];

void cookieInit(void)
{
	cookieRegs[0].word = 0x0014;
	cookieRegs[1].word = 0x0050;
	cookieRegs[2].word = 0x00c0;
	cookieRegs[3].word = 0x0010;
	cookieRegs[4].word = 0x0020;
	cookieRegs[5].word = 0x0010;
}

/* data[0] is the register */
unsigned char cookieWrite(unsigned char *data, unsigned char length)
{
	unsigned char status;

	cookieIica.rxData = NULL;
	cookieIica.rxCount = 0;
	cookieIica.txData = data;
	cookieIica.txCount = length;
	cookieIica.address = 0xa8;
	cookieIica.status = &status;
	iicaTransfer(&cookieIica);
	return status;
}

/* a register's two bytes */
unsigned char cookieRead(unsigned char *reg, unsigned char *data)
{
	unsigned char status;

	cookieIica.rxData = data;
	cookieIica.rxCount = 2;
	cookieIica.txData = reg;
	cookieIica.txCount = 1;
	cookieIica.address = 0xa8;
	cookieIica.status = &status;
	iicaTransfer(&cookieIica);
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
		elmoCommand[0] = COOKIE_REG(4);
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
		elmoCommand[0] = COOKIE_REG(8);
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

	if (on)
		value = cookieRegs[0].byte[1] & 0x7f;
	else
		value = cookieRegs[0].byte[1] | 0x80;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_REG(3);
		elmoCommand[1] = cookieRegs[0].byte[0];
		elmoCommand[2] = value;
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[1] = value;
	} else
		result = 0;
	return result;
}

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

/* register 6's low byte: 0x08c4 (sceSysconCtrlSupplyCurrentUsb) */
unsigned char cookieCtrlSupplyCurrentUsb(unsigned char current)
{
	unsigned char result;

	elmoCommand[0] = COOKIE_REG(6);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[3].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[3].byte[0] = current;
	return result;
}

/* register 5's low byte: 0x08c3 (sceSysconCtrlSupplyCurrentAc) */
unsigned char cookieCtrlSupplyCurrentAc(unsigned char current)
{
	unsigned char result;

	elmoCommand[0] = COOKIE_REG(5);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[2].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[2].byte[0] = current;
	return result;
}

/* register 7's low byte, its bits 5-7 */
unsigned char cookieCtrlReg7(unsigned char value)
{
	unsigned char result;

	value &= 0xe0;
	elmoCommand[0] = COOKIE_REG(7);
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

	reg = COOKIE_REG(1);
	return cookieRead(&reg, status);
}

unsigned char cookieReadStatus2(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_REG(2);
	return cookieRead(&reg, status);
}

/* register 3's bit 6 clear while on and nothing holds it: 0x0892 (sceSysconCtrlChargeAC).
   Sony's compares the new value with the register's high byte, not the low one it sets. */
unsigned char cookieCtrlChargeAc(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	if (on) {
		if (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 &&
		    chargeControl.1)
			value = cookieRegs[0].byte[0] & 0xbf;
		else {
			result = 0;
			goto done;
		}
	} else
		value = cookieRegs[0].byte[0] | 0x40;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_REG(3);
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
