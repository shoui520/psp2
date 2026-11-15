/*
 * The transmit power compensation for the temperature (0x3e09c - 0x3e86a, wlan/txlimit.c): the
 * SoC's temperature sensor is sampled, a calibration entry of the channel says what the power
 * limit of that sample is, and the limit goes to the radio's bus. The firmware's placeholder
 * names, "wlanPs*Header" and "wlanPs*Descriptor", are kept.
 */
#ifndef ROBIN_TXLIMIT_H
#define ROBIN_TXLIMIT_H

#include <stdint.h>

/* The calibration entry of a channel (8 bytes: the radio's response to the temperature). */
struct WlanLimitCurve {
	uint8_t reserved0[2];
	uint8_t corner;		/* the sample the response bends at */
	uint8_t reference;	/* the sample of the reference point */
	uint8_t divisorAbove;	/* the response above the reference, per sample */
	uint8_t divisorBelow;	/* the response below it */
	uint8_t gain;
	uint8_t offset;
};

_Static_assert(sizeof(struct WlanLimitCurve) == 8, "limit curve");

/* What wlanCalibrationLookupInterpolated() returns for a channel. */
struct WlanLimitLookup {
	uint8_t gain;
	uint8_t divisorBelow;
	uint8_t divisorAbove;
	uint8_t corner;
	uint8_t reference;
	uint8_t step;
	uint16_t origin;
};

_Static_assert(sizeof(struct WlanLimitLookup) == 8, "limit lookup");

/* The state of the compensation (0x596f4). */
struct WlanLimitState {
	uint8_t kind;		/* 0: off, 1: the limit follows the sample, 5: only the sample is kept */
	uint8_t reference;	/* the sample at the calibration's reference point */
	uint8_t step;
	uint8_t phase;		/* 0..3: how the limit moves (WLAN_LIMIT_PHASE_*); 4..7: the last mode */
	uint8_t gain;
	uint8_t divisorBelow;
	uint8_t divisorAbove;
	uint8_t corner;
	uint16_t origin;
	uint16_t position;	/* the MAC's position of the last sample */
	uint16_t limit;
	uint16_t limitApplied;	/* 0xffff: none */
	uint8_t sample;
	uint8_t usedSample;
	uint16_t usedLimit;
	uint16_t positionSampled;
	uint8_t sampleSampled;
	uint8_t sampleKept;
	uint16_t target;	/* 0xffff: none */
	uint16_t tracked;
	uint32_t countdown;	/* the 10 ms until the target is dropped */
	uint32_t stamp;		/* the microseconds of the last decay */
};

_Static_assert(sizeof(struct WlanLimitState) == 36, "limit state");

/* WlanLimitState.phase, low nibble */
enum {
	WLAN_LIMIT_PHASE_NONE = 0,
	WLAN_LIMIT_PHASE_START = 1,
	WLAN_LIMIT_PHASE_RISING = 2,
	WLAN_LIMIT_PHASE_FALLING = 3,
};

int wlanPsSyncHeader(void);						/* 0x3e09c */
int wlanPsApplyHeader(void);						/* 0x3e0c0 */
void wlanPsResolveHeader(uint32_t argument);				/* 0x3e0f6 */
void wlanPsScheduleHeader(void);					/* 0x3e0f8 */
void wlanPsNotifyHeader(void);						/* 0x3e104 */
int wlanPsFormatHeader(void (*callback)(int sample, int frame));	/* 0x3e13a */
int wlanPsInspectHeader(void (*callback)(int sample, int frame));	/* 0x3e176 */
int wlanPsTrackHeader(const struct WlanLimitCurve *curve, int sample);	/* 0x3e1c4 */
int wlanPsManageHeader(int sample, int reference, int corner, int gain, int divisorBelow,
		       int divisorAbove);				/* 0x3e21e */
int wlanPsAcquireHeader(int sample, int reference);			/* 0x3e266 */
void wlanPsReleaseHeader(void);						/* 0x3e280 */
int wlanPsSetupHeader(void);						/* 0x3e29c */
int wlanPsClearHeader(unsigned int band, unsigned int channel, unsigned int bandwidth, int limit); /* 0x3e2e0 */
int wlanPsConvertHeader(void);						/* 0x3e32e */
void wlanPsVerifyHeader(void);						/* 0x3e354 */
void wlanPsSelectHeader(unsigned int band, uint32_t channel);		/* 0x3e3aa */
int wlanPsAdjustHeader(unsigned int sample, unsigned int limit);	/* 0x3e43a */
void wlanPsRecordHeader(unsigned int value);				/* 0x3e4b4 */
void wlanPsFilterHeader(unsigned int band, uint32_t channel, unsigned int bandwidth); /* 0x3e4ce */
int wlanPsHandleDescriptor(void);					/* 0x3e752 */
unsigned int wlanPsProcessDescriptor(unsigned int band, unsigned int sample); /* 0x3e776 */
void wlanPsUpdateDescriptor(unsigned int value);			/* 0x3e7c6 */
void wlanPsConfigureDescriptor(int sample, int reset);	/* 0x3e7da */
void wlanPsCheckDescriptor(int enable);					/* 0x3e82c */
void wlanPsValidateDescriptor(const void *block);			/* 0x3e86a */

#endif /* ROBIN_TXLIMIT_H */
