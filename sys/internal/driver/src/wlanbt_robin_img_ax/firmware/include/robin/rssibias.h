/*
 * The RSSI bias that follows the radio's temperature (sys/rssibias.c,
 * 0x4cc00..0x4cca0): the power calibration of the channel gives a reference
 * sample and a slope per antenna, and the power save sampler's callback turns
 * each new sample into the bias wlanSetRssiBias() applies.
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_RSSIBIAS_H
#define ROBIN_RSSIBIAS_H

#include <stdint.h>

/* The channel's calibration quad (wlanCalibrationLookupQuad()). */
struct RssiBiasCalibration {
	uint8_t reserved;
	uint8_t reference;		/* the sample at which the bias is 0 */
	int8_t slope[2];		/* per antenna, thousandths of a dB per sample step */
};

_Static_assert(sizeof(struct RssiBiasCalibration) == 4, "RSSI bias calibration");

struct RssiBias {
	int8_t last;			/* the bias last applied */
	uint8_t reserved[3];
	uint32_t registered;		/* fmCoexManageBuffer() is the sampler's callback */
	struct RssiBiasCalibration calibration;
};

_Static_assert(sizeof(struct RssiBias) == 12, "RSSI bias");

extern struct RssiBias rssiBias;			/* 0x55420 */

int8_t fmCoexInspectBuffer(int sample);			/* 0x4cc00 */
void fmCoexTrackBuffer(uint8_t bias, int force);	/* 0x4cc28 */
void fmCoexManageBuffer(int sample, int force);		/* 0x4cc48 */
void fmCoexAcquireBuffer(int enable);			/* 0x4cc5c */
void fmCoexReleaseBuffer(const void *block);		/* 0x4cc90 */

#endif /* ROBIN_RSSIBIAS_H */
