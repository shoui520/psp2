/*
 * The radio's power state per band: whether the power amplifier and its
 * calibration are on, set up when the channel changes (sys/phy.c). Marvell
 * published no source; the names say what the code does and the file name
 * is inferred.
 */
#include <stddef.h>
#include "robin/calibration.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/socpower.h"
#include "robin/soc.h"

/* Not converted yet; provisional names. */
void wlanPsCheckDescriptor(int enable);				/* 0x3e82c */
void wlanPsValidateDescriptor(const void *block);		/* 0x3e86a: keeps the 8 bytes */
void fmCoexAcquireBuffer(int enable);				/* 0x4cc5c */
void fmCoexReleaseBuffer(const void *block);			/* 0x4cc90: keeps the 4 bytes */

extern uint8_t wlanPhyBand;					/* SQRAM 0xc0011bd5: the band the state is for */
extern uint8_t wlanPhyPowerMode;				/* SQRAM 0xc0011bd6: 0 on, 1 off, from the calibration flags */

/* Clears the two power bits of the SoC. */
void socRfPowerClear(void)
{
	SOC_POWER_CONTROL &= 0xf5ffffff;
}

void socRfPowerSet(void)
{
	SOC_POWER_CONTROL |= 0x0a000000;
}

/* Puts the radio in the power state the band's calibration flags ask for; returns it. */
int phyChannelPowerCheck(unsigned int band)
{
	uint8_t flags[3];

	if (wlanPhyBand != band) {
		if (wlanCalibrationLookupFlags(band, flags) == 0) {
			flags[0] = 1;
			flags[1] = 0;
			flags[2] = 0;
		}
		wlanPhyBand = band;
		wlanPhyPowerMode = flags[0];
		if (flags[0] == 0) {
			wlanPsCheckDescriptor(0);
			rfPowerUp(flags[1], flags[2]);
			socRfPowerSet();
			rfRegSequenceB();
		} else if (flags[0] == 1) {
			rfBusAcquire();
			rfPowerDown();
			rfBusRelease();
			socRfPowerClear();
			rfRegSequenceA();
		}
	}
	return wlanPhyPowerMode;
}

/* Loads the channel's power calibration block. */
void phyChannelPowerLoad(unsigned int band, uint32_t channel)
{
	struct WlanCalibrationKey key = { band, channel, { 0, 0 } };
	uint8_t block[8];

	if (wlanCalibrationLookupBlock(key, block) != 0) {
		wlanPsValidateDescriptor(block);
		wlanPsCheckDescriptor(1);
	}
}

void phyChannelPowerApply(unsigned int band, uint32_t channel)
{
	struct WlanCalibrationKey key = { band, channel, { 0, 0 } };
	uint8_t values[4];
	int found = wlanCalibrationLookupQuad(key, values);

	if (found != 0) {
		fmCoexReleaseBuffer(values);
		found = 1;
	}
	fmCoexAcquireBuffer(found);
}
