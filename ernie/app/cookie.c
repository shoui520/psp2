/*
 * The Cookie: its registers over the I2C bus. Registers 3-8 are kept as last written, so a
 * setter writes only a change; register 3's low byte is read back first, as the Cookie
 * clears its bit 7 itself.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "charge.h"
#include "config.h"
#include "cookie.h"
#include "iica.h"
#include "kernel.h"
#include "log.h"
#include "powerreq.h"
#include "powerseq.h"
#include "task.h"

#if UPD_0103
extern unsigned char updReset;		/* the updater's (updater.h): a reset started it */
#endif

#if SOURCE_0103
/*
 * The slim's Cookie: with no Cookie (its version 0xff) nothing goes on the bus. Its registers
 * are written once from a table, the controls write without comparing (the update wrappers,
 * out of the manual charge mode, compare), and register 6's bit 3 stands for register 3's bit
 * 7 (cookieReadReg3).
 */
static struct IicaRequest cookieIica;
union CookieReg cookieRegs[6];

/* the slim's COOKIE_REG and COOKIE_ADDRESS: DOL-1001 calls them, the slim has the
   constants */
unsigned char cookieReg(unsigned char reg)
{
	return (unsigned char)(configGet()->cookieRegHigh ? reg + 0xa0 : reg);
}

unsigned int cookieAddress(void)
{
	return configGet()->cookieRegHigh ? 0xa4 : 0xa8;
}

/* DOL-1002 has no Cookie to talk to: cookieWrite and cookieRead do nothing, and only
   cookieInitRegs goes on the bus; USS-1002's 0x0105 transfers while the Cookie's version is
   read (not 0, 0xfd or 0xff), its version and register setup going to the bus directly; both
   log a failed transfer (events 0x23 and 0x24) */
#define COOKIE_GATED		(BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104)
#if COOKIE_GATED
#define COOKIE_NO_TRANSFER	(deviceVersions.cookie[0] == 0 || deviceVersions.cookie[0] == 0xfd || \
				 deviceVersions.cookie[0] == 0xff)
static unsigned char cookieBusWrite(unsigned char *data, unsigned char length);
static unsigned char cookieBusRead(unsigned char *reg, unsigned char *data);
#define COOKIE_INIT_WRITE	cookieBusWrite
#else
#define COOKIE_INIT_WRITE	cookieWrite
#endif

#if BOARD_DOL1001
#define COOKIE_SLIM_REG(reg)	cookieReg(reg)
#define COOKIE_SLIM_ADDRESS	cookieAddress()
#else
#define COOKIE_SLIM_REG(reg)	(reg)
#define COOKIE_SLIM_ADDRESS	0xa8
#endif

void cookieInit(void)
{
}

/* each register to its first value; Sony's table leaves the registers' numbers to be set */
struct CookieRegInit {
	unsigned char	reg;
	union CookieReg	*last;
	unsigned int	value;
};

void cookieInitRegs(void)
{
#if BOARD_USS1001 && SOURCE_0104
	unsigned char result;
#endif
	unsigned char i;
	struct CookieRegInit regs[6] = {
		{ 0, &cookieRegs[0], 0x0014 },
#if BOARD_DOL1001
		{ 0, &cookieRegs[1], 0x0050 },
		{ 0, &cookieRegs[2], 0x00c0 },
#else
		{ 0, &cookieRegs[1], 0x0060 },
		{ 0, &cookieRegs[2], 0x0000 },
#endif
		{ 0, &cookieRegs[3], 0x0010 },
		{ 0, &cookieRegs[4], 0x0020 },
		{ 0, &cookieRegs[5], 0x0010 },
	};

	regs[0].reg = COOKIE_SLIM_REG(3);
	regs[1].reg = COOKIE_SLIM_REG(4);
	regs[2].reg = COOKIE_SLIM_REG(5);
	regs[3].reg = COOKIE_SLIM_REG(6);
	regs[4].reg = COOKIE_SLIM_REG(7);
	regs[5].reg = COOKIE_SLIM_REG(8);

#if UPD_0103
	/* the PS TV's updater: the defaults written after a reset only, else the registers read */
	if (updReset) {
#endif
	for (i = 0; i < 6; i++) {
		elmoCommand[0] = regs[i].reg;
		elmoCommand[1] = (unsigned char)regs[i].value;
		elmoCommand[2] = (unsigned char)(regs[i].value >> 8);
#if BOARD_USS1001 && SOURCE_0104
		result = COOKIE_INIT_WRITE(elmoCommand, 3);
		if (result == 0)
#else
		if (COOKIE_INIT_WRITE(elmoCommand, 3) == 0)
#endif
			regs[i].last->word = regs[i].value;
	}
#if UPD_0103
	} else {
		for (i = 0; i < 6; i++)
			cookieRead(&regs[i].reg, regs[i].last->byte);
	}
#endif
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
	/* DOL-1002 (and USS-1002's 0x0105) tests register 3's bit 15 and does nothing with it */
	if (cookieRegs[0].byte[1] & 0x80)
		;
#endif
}

/* data[0] is the register */
#if COOKIE_GATED
unsigned char cookieWrite(unsigned char *data, unsigned char length)
{
#if BOARD_USS1001
	if (COOKIE_NO_TRANSFER)
		return 0;
	return cookieBusWrite(data, length);
#else
	return 0;
#endif
}

static unsigned char cookieBusWrite(unsigned char *data, unsigned char length)
#else
unsigned char cookieWrite(unsigned char *data, unsigned char length)
#endif
{
	unsigned char status;
#if !UPD_0103
	ER ercd;
	FLGPTN ptn;
	unsigned char i;
#endif

#if !COOKIE_GATED
	if (deviceVersions.cookie[0] == 0xff)
		return 0;
#endif
	cookieIica.rxData = NULL;
	cookieIica.rxCount = 0;
	cookieIica.txData = data;
	cookieIica.txCount = length;
	cookieIica.address = COOKIE_SLIM_ADDRESS;
	cookieIica.status = &status;
#if UPD_0103	/* the PS TV's updater: no kernel, the transfer waited for in iicaTransfer */
	iicaTransfer(&cookieIica);
#else
	for (i = 1; i < length; i++)
		;
	clr_flg(4, ~0x10);
	iicaStart(&cookieIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
#endif
#if COOKIE_GATED
	if (status) {
		logEvent(0x23);
		logEventValue(0x24, status);
	}
#endif
	return status;
}

/* a register's two bytes; with no Cookie 0 */
#if COOKIE_GATED
unsigned char cookieRead(unsigned char *reg, unsigned char *data)
{
#if BOARD_USS1001
	if (COOKIE_NO_TRANSFER) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	return cookieBusRead(reg, data);
#else
	*data++ = 0;
	*data = 0;
	return 0;
#endif
}

static unsigned char cookieBusRead(unsigned char *reg, unsigned char *data)
#else
unsigned char cookieRead(unsigned char *reg, unsigned char *data)
#endif
{
	unsigned char status;
#if !UPD_0103
	ER ercd;
	FLGPTN ptn;
#endif

#if !COOKIE_GATED
	if (deviceVersions.cookie[0] == 0xff) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
#endif
	cookieIica.rxData = data;
	cookieIica.rxCount = 2;
	cookieIica.txData = reg;
	cookieIica.txCount = 1;
	cookieIica.address = COOKIE_SLIM_ADDRESS;
	cookieIica.status = &status;
#if UPD_0103
	iicaTransfer(&cookieIica);
#else
	clr_flg(4, ~0x10);
	iicaStart(&cookieIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
#endif
#if COOKIE_GATED
	if (status) {
		logEvent(0x23);
		logEventValue(0x24, status);
	}
#endif
	return status;
}

/* the version as Sony's sets it here: two bytes of a struct, not the array */
struct CookieVersion {
	unsigned char	low;
	unsigned char	high;
};
#define COOKIE_VERSION(version)	((struct CookieVersion *)(version))

/* register 0; with no Cookie 0xffff */
#if BOARD_DOL1002
/* DOL-1002: 0xfdfd, no Cookie */
unsigned char cookieReadVersion(unsigned char *version)
{
	COOKIE_VERSION(version)->low = COOKIE_VERSION(version)->high = 0xfd;
	return 0;
}
#elif BOARD_USS1001 && SOURCE_0104
/* USS-1002's 0x0105: from the bus */
unsigned char cookieReadVersion(unsigned char *version)
{
	unsigned char reg;

	reg = COOKIE_SLIM_REG(0);
	return cookieBusRead(&reg, version);
}
#else
unsigned char cookieReadVersion(unsigned char *version)
{
	unsigned char reg;

	if (deviceVersions.cookie[0] == 0xff) {
		COOKIE_VERSION(version)->low = COOKIE_VERSION(version)->high = 0xff;
		return 0;
	}
	reg = COOKIE_SLIM_REG(0);
	return cookieRead(&reg, version);
}
#endif

/* register 4's low byte, out of the manual charge mode and when it changes */
unsigned char cookieUpdateChargingCurrent(unsigned char current)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0 && current != cookieRegs[1].byte[0])
		cookieCtrlChargingCurrent(current);
	return result;
}

/* register 4's low byte: 0x08c2 (sceSysconCtrlChargingCurrent) and the temperature's range */
unsigned char cookieCtrlChargingCurrent(unsigned char current)
{
	unsigned char result;

	elmoCommand[0] = COOKIE_SLIM_REG(4);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[1].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[1].byte[0] = current;
	return result;
}

/* register 8's low byte, out of the manual charge mode and when it changes */
unsigned char cookieUpdateReg8(unsigned char value)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0 && value != cookieRegs[5].byte[0])
		cookieCtrlReg8(value);
	return result;
}

/* register 8's low byte: the temperature's range */
unsigned char cookieCtrlReg8(unsigned char value)
{
	unsigned char result;

	elmoCommand[0] = COOKIE_SLIM_REG(8);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[5].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[5].byte[0] = value;
	return result;
}

/* the battery's charge, out of the manual charge mode */
unsigned char cookieUpdateBatteryCharge(unsigned char on)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0)
		result = cookieCtrlBatteryCharge(on);
	return result;
}

/* register 3's bit 15 clear while on: 0x08c1 (sceSysconCtrlBatteryCharge) */
unsigned char cookieCtrlBatteryCharge(unsigned char on)
{
	unsigned char result;
	unsigned char value;

#if BOARD_DOL1001
	cookieReadReg3();
#endif
	if (on)
		value = cookieRegs[0].byte[1] & 0x7f;
	else
		value = cookieRegs[0].byte[1] | 0x80;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = cookieRegs[0].byte[0];
		elmoCommand[2] = value;
		result = cookieWrite(elmoCommand, 3);
		if (result == 0) {
#if BOARD_DOL1002 || BOARD_USS1001 && SOURCE_0104
			/* DOL-1002 (and USS-1002's 0x0105) tests on again and does nothing with it */
			if (on)
				;
#endif
			cookieRegs[0].byte[1] = value;
		}
	} else
		result = 0;
	return result;
}

#if !BOARD_DOL1001
/* register 3's bit 13 set while on */
unsigned char cookieCtrlReg3Bit13(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	if (on)
		value = cookieRegs[0].byte[1] | 0x20;
	else
		value = cookieRegs[0].byte[1] & 0xdf;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = cookieRegs[0].byte[0];
		elmoCommand[2] = value;
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[1] = value;
	} else
		result = 0;
	return result;
}

/* register 3's bit 12 set while on (the charge's boost), then register 6 read back */
unsigned char cookieCtrlReg3Bit12(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	if (on)
		value = cookieRegs[0].byte[1] | 0x10;
	else
		value = cookieRegs[0].byte[1] & 0xef;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = cookieRegs[0].byte[0];
		elmoCommand[2] = value;
		result = cookieWrite(elmoCommand, 3);
		if (result == 0) {
			cookieRegs[0].byte[1] = value;
			cookieReadReg3();
			task5SetFlag(4, 0x0400);
		}
	} else
		result = 0;
	return result;
}
#endif

/* the Cookie held (powerCookieClear, powerCookieSet), then the AC's charge */
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

/* register 6, out of the manual charge mode, at most chargeCookieSetting and
   chargeTempSetting */
unsigned char cookieUpdateSupplyCurrentUsb(unsigned char current)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0) {
#if CHARGE_0104
		result = cookieCtrlSupplyCurrentUsb(chargeLimitCurrent(current));
#else
		if (chargeCookieSetting < current)
			current = chargeCookieSetting;
		if (chargeTempSetting < current)
			current = chargeTempSetting;
		result = cookieCtrlSupplyCurrentUsb(current);
#endif
	}
	return result;
}

/* register 6's bits 4-7: 0x08c4 (sceSysconCtrlSupplyCurrentUsb) */
unsigned char cookieCtrlSupplyCurrentUsb(unsigned char current)
{
	unsigned char result;

#if !BOARD_DOL1001
	cookieReadReg3();
	current = (cookieRegs[3].byte[0] & 0x0f) | current;
#endif
	elmoCommand[0] = COOKIE_SLIM_REG(6);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[3].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[3].byte[0] = current;
	return result;
}

/* the AC's supply current, as the USB's: the slim has none to set */
unsigned char cookieUpdateSupplyCurrent(unsigned char current)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0) {
#if CHARGE_0104
		result = cookieCtrlSupplyCurrent(chargeLimitCurrent(current));
#else
		if (chargeCookieSetting < current)
			current = chargeCookieSetting;
		if (chargeTempSetting < current)
			current = chargeTempSetting;
		result = cookieCtrlSupplyCurrent(current);
#endif
	}
	return result;
}

#if BOARD_DOL1001
/* DOL-1001's register 5's bits 4-7: 0x08c3 (sceSysconCtrlSupplyCurrentAc) */
unsigned char cookieCtrlSupplyCurrent(unsigned char current)
{
	unsigned char result;

	elmoCommand[0] = COOKIE_SLIM_REG(5);
	elmoCommand[1] = current;
	elmoCommand[2] = cookieRegs[2].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[2].byte[0] = current;
	return result;
}
#else
unsigned char cookieCtrlSupplyCurrent(unsigned char current)
{
	unsigned char result = 0;

	return result;
}
#endif

/* register 7's low byte, its bits 5-7 */
unsigned char cookieCtrlReg7(unsigned char value)
{
	unsigned char result;

	value &= 0xe0;
	elmoCommand[0] = COOKIE_SLIM_REG(7);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[4].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[4].byte[0] = value;
	return result;
}

#if !UPD_0103	/* the PS TV's updater: none of the currents' encodings, nor register 1 */
/* the currents set, as their registers' bits 4-7 */
unsigned char cookieGetChargingCurrent(void)
{
	return cookieRegs[1].byte[0] & 0xf0;
}

unsigned char cookieGetSupplyCurrentUsb(void)
{
	return cookieRegs[3].byte[0] & 0xf0;
}

#if BOARD_DOL1001
/* DOL-1001's AC supply current: register 5 */
unsigned char cookieGetSupplyCurrent(void)
{
	return cookieRegs[2].byte[0] & 0xf0;
}

/* DOL-1001's currents: the host's value is the register's bits 4-7 */
unsigned char cookieChargingCurrentEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;

	result = 0;
	*reg = value << 4;
	return result;
}

unsigned char cookieSupplyCurrentUsbEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;
	unsigned char bits;

	result = 0;
	bits = 0;
	bits = value << 4;
	if (result == 0)
		*reg = bits;
	return result;
}

unsigned char cookieSupplyCurrentEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;

	result = 0;
	*reg = value << 4;
	return result;
}

unsigned char cookieChargingCurrentDecode(unsigned char *out, unsigned char reg)
{
	*out = reg >> 4;
	return 0;
}

unsigned char cookieSupplyCurrentUsbDecode(unsigned char *out, unsigned char reg)
{
	unsigned char result;
	unsigned char value;

	result = 0;
	value = 0;
	value = reg >> 4;
	if (result == 0)
		*out = value;
	return result;
}

unsigned char cookieSupplyCurrentDecode(unsigned char *out, unsigned char reg)
{
	unsigned char result;

	result = 0;
	*out = reg >> 4;
	return result;
}
#else
unsigned char cookieGetSupplyCurrent(void)
{
	return 0;
}

/* a current as the host gives it to its register's bits 4-7, 1 for one it cannot be: the
   charging current even, 2-30 */
unsigned char cookieChargingCurrentEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;

	result = 0;
	if ((value & 1) != 0 || value < 2 || value >= 0x1f)
		result = 1;
	else
		*reg = (value << 3) & 0xf0;
	return result;
}

/* the USB supply's: 1-24 in twelve steps */
unsigned char cookieSupplyCurrentUsbEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;
	unsigned char bits;

	result = 0;
	bits = 0;
	switch (value) {
	case 1:
		bits = 0x10;
		break;
	case 3:
		bits = 0x20;
		break;
	case 5:
		bits = 0x30;
		break;
	case 7:
		bits = 0x40;
		break;
	case 9:
		bits = 0x50;
		break;
	case 10:
		bits = 0x60;
		break;
	case 12:
		bits = 0x70;
		break;
	case 15:
		bits = 0x80;
		break;
	case 17:
		bits = 0x90;
		break;
	case 19:
		bits = 0xa0;
		break;
	case 21:
		bits = 0xb0;
		break;
	case 24:
		bits = 0xc0;
		break;
	default:
		result = 1;
		break;
	}
	if (result == 0)
		*reg = bits;
	return result;
}

/* the AC's: none */
unsigned char cookieSupplyCurrentEncode(unsigned char *reg, unsigned char value)
{
	unsigned char result;
	unsigned char bits;

	result = 0;
	switch (value) {
	default:
		result = 1;
		return result;
	}
	*reg = bits;
	return result;
}

/* and back, as the host reads them (0x0841, 0x0843) */
unsigned char cookieChargingCurrentDecode(unsigned char *out, unsigned char reg)
{
	*out = reg >> 3;
	return 0;
}

unsigned char cookieSupplyCurrentUsbDecode(unsigned char *out, unsigned char reg)
{
	unsigned char result;
	unsigned char value;

	result = 0;
	value = 0;
	switch (reg) {
	case 0x10:
		value = 1;
		break;
	case 0x20:
		value = 3;
		break;
	case 0x30:
		value = 5;
		break;
	case 0x40:
		value = 7;
		break;
	case 0x50:
		value = 9;
		break;
	case 0x60:
		value = 10;
		break;
	case 0x70:
		value = 12;
		break;
	case 0x80:
		value = 15;
		break;
	case 0x90:
		value = 17;
		break;
	case 0xa0:
		value = 19;
		break;
	case 0xb0:
		value = 21;
		break;
	case 0xc0:
		value = 24;
		break;
	default:
		result = 1;
		break;
	}
	if (result == 0)
		*out = value;
	return result;
}

unsigned char cookieSupplyCurrentDecode(unsigned char *out, unsigned char reg)
{
	unsigned char result;
	unsigned char value;

	result = 0;
	switch (reg) {
	default:
		result = 1;
		return result;
	}
	*out = value;
	return result;
}
#endif
#endif	/* !UPD_0103 */

unsigned char cookieReadStatus1(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_SLIM_REG(1);
	return cookieRead(&reg, status);
}

unsigned char cookieReadStatus2(unsigned char *status)
{
	unsigned char reg;

	reg = COOKIE_SLIM_REG(2);
	return cookieRead(&reg, status);
}

#if !UPD_0103
/* status registers 1 and 2 as the power status has them (0x08c6) */
unsigned char cookieReadReg1(unsigned char *out)
{
	unsigned char result;
	unsigned char reg;
	union CookieStatus1 status1;
	union CookieStatus2 status2;
	union PowerStatus status;

	reg = COOKIE_SLIM_REG(1);
	result = cookieRead(&reg, status1.byte);
	if (result != 0)
		goto done;
	reg = COOKIE_SLIM_REG(2);
	result = cookieRead(&reg, status2.byte);
	if (result != 0)
		goto done;
	memset(&status, 0, sizeof(status));
	status.bits.charging = status1.bits.charging;
	status.bits.battery = status1.bits.battery;
	status.bits.ac = status1.bits.ac;
	status.bits.status3 = status1.bits.status3;
	status.bits.status2 = status1.bits.status2;
	status.bits.status1 = status1.bits.status1;
	status.bits.level = status1.bits.level;
#if BOARD_DOL1001
	status.bits.usb = status2.bits.usb;
	status.bits.supply = status1.bits.state;
	status.bits.vbus1 = status2.bits.vbus;
	status.bits.vbus2 = status2.bits.vbusCharge;
#else
	status.bits.usb = status2.bits.usb;
	status.bits.supply = status2.bits.supply;
	status.bits.vbus1 = status.bits.vbus2 = status2.bits.vbus;
#endif
	out[0] = status.byte[0];
	out[1] = status.byte[1];
	out[2] = status.byte[2];
	out[3] = status.byte[3];
done:
	return result;
}
#endif

#if !BOARD_DOL1001
/* register 3 */
unsigned char cookieReadReg3Value(unsigned char *value)
{
	unsigned char reg;

	reg = COOKIE_SLIM_REG(3);
	return cookieRead(&reg, value);
}
#endif

/* register 6 read back: the Cookie cleared bit 3, so the USB's mode is new */
void cookieReadReg3(void)
{
	unsigned char reg;
	unsigned char was;

#if BOARD_DOL1001
	was = cookieRegs[0].byte[0] & 0x80;
	reg = COOKIE_SLIM_REG(3);
	cookieRead(&reg, cookieRegs[0].byte);
	if (!(cookieRegs[0].byte[0] & 0x80) && was) {
#else
	was = cookieRegs[3].byte[0] & 0x08;
	reg = COOKIE_SLIM_REG(6);
	cookieRead(&reg, cookieRegs[3].byte);
	if (!(cookieRegs[3].byte[0] & 0x08) && was) {
#endif
		chargeUsbMode = 0;
		chargeUsbHold = 0;
	}
}

/* the AC's charge, out of the manual charge mode, while on only when nothing holds the
   Cookie and the charge may run: 0x0892 (sceSysconCtrlChargeAC) */
unsigned char cookieCtrlChargeAc(unsigned char on)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0) {
		if (on && (powerStatus.bits.cookieLock0 || powerStatus.bits.cookieLock1 ||
			   chargeCookieHeld || !chargeControl.1))
			;
		else
			result = cookieSetChargeAc(on);
	}
	return result;
}

#if BOARD_DOL1001
/* DOL-1001's: register 3's bit 6 clear while on (Sony's compares the new low byte with the
   high byte) */
unsigned char cookieSetChargeAc(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	value = 0;
	cookieReadReg3();
	if (on)
		value = cookieRegs[0].byte[0] & 0xbf;
	else
		value = cookieRegs[0].byte[0] | 0x40;
	if (value != cookieRegs[0].byte[1]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
}
#else
/* the slim has no AC charge to set */
unsigned char cookieSetChargeAc(unsigned char on)
{
	unsigned char result;
	unsigned char value;

	value = 0;
	result = 0;
	return result;
}
#endif

/* register 6's bit 3, out of the manual charge mode, while on only when nothing holds the
   Cookie */
unsigned char cookieUpdateReg6Bit3(unsigned char clear)
{
	unsigned char result = 0;

	if (chargeGetManualMode() == 0) {
		if (!clear || (!powerStatus.bits.cookieLock0 && !powerStatus.bits.cookieLock1 &&
			       !chargeCookieHeld))
			result = cookieCtrlReg6Bit3(clear);
	}
	return result;
}

/* register 6's bit 3 clear while on */
unsigned char cookieCtrlReg6Bit3(unsigned char clear)
{
	unsigned char result;
	unsigned char value;

	cookieReadReg3();
#if BOARD_DOL1001
	if (clear)
		value = cookieRegs[0].byte[0] & 0x7f;
	else
		value = cookieRegs[0].byte[0] | 0x80;
	elmoCommand[0] = COOKIE_SLIM_REG(3);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[0].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[0].byte[0] = elmoCommand[1];
#else
	if (clear)
		value = cookieRegs[3].byte[0] & 0xf7;
	else
		value = cookieRegs[3].byte[0] | 0x08;
	elmoCommand[0] = COOKIE_SLIM_REG(6);
	elmoCommand[1] = value;
	elmoCommand[2] = cookieRegs[3].byte[1];
	result = cookieWrite(elmoCommand, 3);
	if (result == 0)
		cookieRegs[3].byte[0] = elmoCommand[1];
#endif
	return result;
}

/* register 3's bit 2 clear in mode 1 */
unsigned char cookieCtrlReg3Mode(unsigned char mode)
{
	unsigned char result;
	unsigned char value;

#if BOARD_DOL1001
	cookieReadReg3();
#endif
	if (mode == 1)
		value = cookieRegs[0].byte[0] & 0xfb;
	else
		value = cookieRegs[0].byte[0] | 0x04;
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
}

/* register 3's bit 0 set and bit 2 clear while on, else both clear */
unsigned char cookieCtrlReg3Bit2(unsigned char on)
{
	unsigned char result;
	unsigned char value;

#if BOARD_DOL1001
	cookieReadReg3();
#endif
#if BOARD_DOL1001
	if (on)
		value = cookieRegs[0].byte[0] | 0x04;	/* DOL-1001: bit 2 set */
	else
#else
	if (on) {
		value = cookieRegs[0].byte[0] & 0xfb;
		value |= 0x01;
	} else
#endif
		value = cookieRegs[0].byte[0] & 0xfa;
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
}

/* register 3's bit 0 set and bit 2 clear (the PS TV's updater: nothing) */
unsigned char cookieCtrlReg3Bit0(void)
{
#if UPD_0103
	return 0;
#else
	unsigned char value;
	unsigned char result;

#if BOARD_DOL1001
	cookieReadReg3();
#endif
	value = (cookieRegs[0].byte[0] & 0xfb) | 0x01;
	if (value != cookieRegs[0].byte[0]) {
		elmoCommand[0] = COOKIE_SLIM_REG(3);
		elmoCommand[1] = value;
		elmoCommand[2] = cookieRegs[0].byte[1];
		result = cookieWrite(elmoCommand, 3);
		if (result == 0)
			cookieRegs[0].byte[0] = value;
	} else
		result = 0;
	return result;
#endif
}

#else
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

#ifdef ERNIE_UPDATER
/* the updater's (0x0100060D), with no kernel: the transfer waited for in iicaTransfer */
unsigned char cookieWrite(unsigned char *data, unsigned char length)
{
	unsigned char status;

	cookieIica.rxData = NULL;
	cookieIica.rxCount = 0;
	cookieIica.txData = data;
	cookieIica.txCount = length;
	cookieIica.address = COOKIE_ADDRESS;
	cookieIica.status = &status;
	iicaTransfer(&cookieIica);
	return status;
}

unsigned char cookieRead(unsigned char *reg, unsigned char *data)
{
	unsigned char status;

	cookieIica.rxData = data;
	cookieIica.rxCount = 2;
	cookieIica.txData = reg;
	cookieIica.txCount = 1;
	cookieIica.address = COOKIE_ADDRESS;
	cookieIica.status = &status;
	iicaTransfer(&cookieIica);
	return status;
}
#else
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
#endif

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

#if FW_SINCE(0x00090009)	/* 0x00080504 has only cookieCtrlReg7 here */
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
#endif

#if FW_SINCE(0x00090804)	/* led.c's */
/* register 3's bit 0 set and bit 2 clear (the updater's: nothing, and nothing calls it) */
#ifdef ERNIE_UPDATER
unsigned char cookieCtrlReg3Bit0(void)
{
	return 0;
}
#else
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
#endif

#endif	/* SOURCE_0103 */
