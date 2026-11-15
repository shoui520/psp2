/*
 * The transmit power the RF is programmed with (0x00030c58..0x00030e20).
 * The rates fall into eight power classes; each class has the most the host
 * allows (wlanTxPowerClassMaximumSet()) and the three RF levels the
 * calibration data gives for it on the current channel, written to the RF
 * registers 0x1d5 + 3 * class. The filename and the names are inferred.
 */
#include <stdint.h>
#include "robin/calibration.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/socpower.h"
#include "robin/txpower.h"

#define WLAN_TX_POWER_CLASSES	8
#define WLAN_TX_POWER_RATE_INDEXES	31
#define WLAN_TX_POWER_PATHS	1

/* Each class's three RF level registers, of RF path 0. */
#define RF_REG_TX_POWER_LEVEL(path, class, n)	(((path) << 8) + (class) * 3 + 0x1d5 + (n))
#define RF_REG_TX_POWER_GATE	0x1af

/* What a class is: what the calibration's lookup is asked for. */
#define WLAN_TX_POWER_KIND_HT20	7
#define WLAN_TX_POWER_KIND_HT40	8

struct WlanTxPowerClass {
	uint8_t kind;			/* WLAN_TX_POWER_KIND_*, else legacy */
	uint8_t maximum[4];		/* int32_t, unaligned: dBm */
	uint8_t levels[3];		/* the RF levels last written */
};
_Static_assert(sizeof(struct WlanTxPowerClass) == 8, "transmit power class");

/* 1: the RF's transmit power is on (wlanTxPowerRfEnable()); 0xff before the first call. */
uint8_t wlanTxPowerRfEnabled __attribute__((section(".itcm.rw"))) = 0xff;	/* 0x54f3c */

/* Each rate index's class (0xff: none). */
static uint8_t wlanTxPowerRateClasses[WLAN_TX_POWER_RATE_INDEXES]
	__attribute__((section(".itcm.rw"))) = {		/* 0x54f3d */
	1, 1, 1, 1, 0xff, 4, 4, 4, 4, 3, 3, 2, 2, 0xff, 4, 4,
	4, 3, 3, 2, 2, 2, 7, 7, 7, 7, 6, 6, 5, 5, 5,
};

static struct WlanTxPowerClass wlanTxPowerClasses[WLAN_TX_POWER_CLASSES]
	__attribute__((section(".itcm.rw"))) = {		/* 0x54f5c */
	{ WLAN_TX_POWER_KIND_HT20, { 12 }, { 0x48 } },
	{ 3, { 12 }, { 0x58 } },
	{ WLAN_TX_POWER_KIND_HT20, { 12 }, { 0x46 } },
	{ WLAN_TX_POWER_KIND_HT20, { 12 }, { 0x4a } },
	{ WLAN_TX_POWER_KIND_HT20, { 12 }, { 0x50 } },
	{ WLAN_TX_POWER_KIND_HT40, { 12 }, { 0x44 } },
	{ WLAN_TX_POWER_KIND_HT40, { 12 }, { 0x48 } },
	{ WLAN_TX_POWER_KIND_HT40, { 12 }, { 0x4e } },
};

uint8_t wlanTxPowerRateClass(uint32_t index)
{
	return wlanTxPowerRateClasses[index];
}

int wlanTxPowerClassMaximumGet(unsigned int class)
{
	return __aeabi_uread4(wlanTxPowerClasses[class].maximum);
}

int wlanTxPowerClassMaximumSet(unsigned int class, int32_t value)
{
	__aeabi_uwrite4(value, wlanTxPowerClasses[class].maximum);
	return value;
}

void wlanTxPowerLevel0Write(unsigned int path, unsigned int class, uint8_t value)
{
	uint16_t reg = RF_REG_TX_POWER_LEVEL(path, class, 0);

	rfBusAcquire();
	rfBusWrite(reg, value);
	rfBusRelease();
}

void wlanTxPowerLevel1Write(unsigned int path, unsigned int class, uint8_t value)
{
	uint16_t reg = RF_REG_TX_POWER_LEVEL(path, class, 1);

	rfBusAcquire();
	rfBusWrite(reg, value);
	rfBusRelease();
}

void wlanTxPowerLevel2Write(unsigned int path, unsigned int class, uint8_t value)
{
	uint16_t reg = RF_REG_TX_POWER_LEVEL(path, class, 2);

	rfBusAcquire();
	rfBusWrite(reg, value);
	rfBusRelease();
}

/*
 * Looks a class's levels up in the calibration data for the current channel
 * (the defaults of the band when it has none) and writes them to the RF.
 * The 40 MHz classes (5..7) are left alone on a 20 MHz channel.
 */
void wlanTxPowerClassApply(unsigned int class)
{
	struct WlanTxPowerClass *entry;
	struct WlanCalibrationKey key;
	struct WlanTxPowerRequest request;
	uint8_t levels[4];
	unsigned int path;

	if (class >= WLAN_TX_POWER_CLASSES)
		return;
	entry = &wlanTxPowerClasses[class];
	key.band = rfGetBand();
	key.channel = rfGetChannel();
	request.power = __aeabi_uread4(entry->maximum);
	if (class - 5 <= 2) {
		if (rfGetBandwidth() != 2 && rfGetBandwidth() != 3)
			return;
		request.secondary = rfGetBandwidth();
	} else {
		request.secondary = 1;
	}
	if (entry->kind == WLAN_TX_POWER_KIND_HT40)
		request.bandwidth = 2;
	else if (entry->kind == WLAN_TX_POWER_KIND_HT20)
		request.bandwidth = 1;
	else
		request.bandwidth = 0;
	for (path = 0; path < WLAN_TX_POWER_PATHS; path++) {
		uint8_t *level = &entry->levels[path];

		if (wlanTxPowerLookup(key, request, levels)) {
			level[0] = levels[2];
			level[1] = levels[0];
			level[2] = levels[1];
		} else if (rfGetBand() == 0) {
			level[0] = 72;
			level[1] = 0;
			level[2] = 0;
		} else if (rfGetBand() == 1) {
			level[0] = 59;
			level[1] = 0;
			level[2] = 0;
		}
		wlanTxPowerLevel0Write(path, class, level[0]);
		wlanTxPowerLevel1Write(path, class, level[1]);
		wlanTxPowerLevel2Write(path, class, level[2]);
	}
}

/* The channel changed: every class is looked up again. */
void wlanTxPowerClassesApply(void)
{
	unsigned int class;

	for (class = 0; class < WLAN_TX_POWER_CLASSES; class = (uint8_t)(class + 1))
		wlanTxPowerClassApply(class);
}

/* Turns the RF's transmit power on (1) or off (0). */
void wlanTxPowerRfEnable(int enable)
{
	if (enable == 1) {
		wlanTxPowerRfEnabled = enable;
		macResetRelease();
		rfBusClearBits(RF_REG_TX_POWER_GATE, 15);
	} else if (enable == 0) {
		wlanTxPowerRfEnabled = enable;
		macResetAssert();
		rfBusSetBits(RF_REG_TX_POWER_GATE, 15);
	}
}
